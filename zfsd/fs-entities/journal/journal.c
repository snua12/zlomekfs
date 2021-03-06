/*! \file \brief Journal datatype.  */

/* Copyright (C) 2004 Josef Zlomek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#include "system.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "pthread-wrapper.h"
#include "journal.h"
#include "memory.h"
#include "crc32.h"
#include "alloc-pool.h"

/*! Alloc pool of journal_entry.  */
static alloc_pool journal_pool;

/*! Mutex protecting journal_pool.  */
static pthread_mutex_t journal_mutex;

/*! Return hash value for journal entry X.  */

static hash_t journal_hash(const void *x)
{
	return JOURNAL_HASH((const struct journal_entry_def *)x);
}

/*! Compare journal entries X and Y.  */

static int journal_eq(const void *x, const void *y)
{
	const struct journal_entry_def *j1 = (const struct journal_entry_def *)x;
	const struct journal_entry_def *j2 = (const struct journal_entry_def *)y;

	return (j1->oper == j2->oper
			&& j1->name.len == j2->name.len
			&& strcmp(j1->name.str, j2->name.str) == 0);
}

/*! Create a new journal with initial NELEM elements.  */

journal_t journal_create(unsigned int nelem, pthread_mutex_t * mutex)
{
	journal_t journal;

	journal = (journal_t) xmalloc(sizeof(struct journal_def));
	journal->htab = htab_create(nelem, journal_hash, journal_eq, NULL, NULL);
	journal->mutex = mutex;
	journal->first = NULL;
	journal->last = NULL;
	journal->fd = -1;
	journal->generation = 0;

	return journal;
}

/*! Empty the journal JOURNAL.  */

void journal_empty(journal_t journal)
{
	journal_entry entry, next;

	CHECK_MUTEX_LOCKED(journal->mutex);

	zfsd_mutex_lock(&journal_mutex);
	for (entry = journal->first; entry; entry = next)
	{
		next = entry->next;
		free(entry->name.str);
		pool_free(journal_pool, entry);
	}
	zfsd_mutex_unlock(&journal_mutex);

	journal->first = NULL;
	journal->last = NULL;
	htab_empty(journal->htab);
}

/*! Destroy journal JOURNAL.  */

void journal_destroy(journal_t journal)
{
	journal_entry entry, next;

	CHECK_MUTEX_LOCKED(journal->mutex);

	zfsd_mutex_lock(&journal_mutex);
	for (entry = journal->first; entry; entry = next)
	{
		next = entry->next;
		free(entry->name.str);
		pool_free(journal_pool, entry);
	}
	zfsd_mutex_unlock(&journal_mutex);

	htab_destroy(journal->htab);
	free(journal);
}

/*! Insert a journal entry and return true if the journal has changed. \param 
   journal Journal into which the entry will be inserted. \param oper The type 
   of operation of the journal entry. \param local_fh Local file handle of the 
   corresponding file. \param master_fh Master file handle of the
   corresponding file. \param master_version Master version of the file.
   \param name Name of the file. \param copy Copy the name.  */

bool
journal_insert(journal_t journal, journal_operation_t oper,
			   zfs_fh * local_fh, zfs_fh * master_fh,
			   uint64_t master_version, string * name, bool copy)
{
	journal_entry entry;
	void **slot;

	CHECK_MUTEX_LOCKED(journal->mutex);

	if (oper == JOURNAL_OPERATION_DEL)
	{
		/* If we are adding a DEL entry try to anihilate ;-) it with the
		   corresponding ADD entry.  */
		if (journal_delete(journal, JOURNAL_OPERATION_ADD, name))
		{
			if (!copy)
			{
				/* If we shall not copy NAME the NAME is dynamically allocated
				   and caller does not free it so we have to free it now.  */
				free(name->str);
			}
			return true;
		}
	}

	zfsd_mutex_lock(&journal_mutex);
	entry = (journal_entry) pool_alloc(journal_pool);
	zfsd_mutex_unlock(&journal_mutex);
	entry->oper = oper;
	entry->name = *name;

	slot = htab_find_slot_with_hash(journal->htab, entry, JOURNAL_HASH(entry),
									INSERT);
	if (*slot)
	{
		journal_entry old = (journal_entry) * slot;

		/* When there already is an entry with the same operation and name in
		   the journal, zfsd has crashed and left the journal in inconsistent
		   state. In this case, delete the old entry and add a new one.  */

		if (old->next)
			old->next->prev = old->prev;
		else
			journal->last = old->prev;
		if (old->prev)
			old->prev->next = old->next;
		else
			journal->first = old->next;

		free(old->name.str);
		zfsd_mutex_lock(&journal_mutex);
		pool_free(journal_pool, old);
		zfsd_mutex_unlock(&journal_mutex);
	}

	entry->dev = local_fh->dev;
	entry->ino = local_fh->ino;
	entry->gen = local_fh->gen;
	entry->master_fh = *master_fh;
	entry->master_version = master_version;
	if (copy)
		entry->name.str = (char *)xmemdup(name->str, entry->name.len + 1);

	*slot = entry;
	entry->next = NULL;
	entry->prev = journal->last;
	if (journal->last)
		journal->last->next = entry;
	journal->last = entry;
	if (journal->first == NULL)
		journal->first = entry;
	return true;
}

/*! Return true if the journal entry is a member of the journal. \param
   journal Journal in which the entry should be looked up. \param oper The
   type of operation of the journal entry. \param name The name of file of the 
   journal entry.  */

bool journal_member(journal_t journal, journal_operation_t oper, string * name)
{
	struct journal_entry_def entry;

	CHECK_MUTEX_LOCKED(journal->mutex);

	entry.oper = oper;
	entry.name = *name;
	return (htab_find_with_hash(journal->htab, &entry, JOURNAL_HASH(&entry))
			!= NULL);
}

/*! Delete the journal entry.l entry is a member of the journal. \param
   journal Journal from which the entry should be deleted. \param oper The
   type of operation of the journal entry. \param name The name of file of the 
   journal entry.  */

bool journal_delete(journal_t journal, journal_operation_t oper, string * name)
{
	struct journal_entry_def entry;
	journal_entry del;
	void **slot;

	CHECK_MUTEX_LOCKED(journal->mutex);

	entry.oper = oper;
	entry.name = *name;
	slot =
		htab_find_slot_with_hash(journal->htab, &entry, JOURNAL_HASH(&entry),
								 NO_INSERT);
	if (!slot)
		return false;

	del = (journal_entry) * slot;
	if (del->next)
		del->next->prev = del->prev;
	else
		journal->last = del->prev;
	if (del->prev)
		del->prev->next = del->next;
	else
		journal->first = del->next;

	free(del->name.str);
	zfsd_mutex_lock(&journal_mutex);
	pool_free(journal_pool, del);
	zfsd_mutex_unlock(&journal_mutex);
	htab_clear_slot(journal->htab, slot);

	return true;
}

/*! Delete a journal entry ENTRY from journal JOURNAL. Return true if it was
   really deleted.  */

bool journal_delete_entry(journal_t journal, journal_entry entry)
{
	void **slot;

	CHECK_MUTEX_LOCKED(journal->mutex);

	slot = htab_find_slot_with_hash(journal->htab, entry, JOURNAL_HASH(entry),
									NO_INSERT);
	if (!slot)
		return false;

	if (entry->next)
		entry->next->prev = entry->prev;
	else
		journal->last = entry->prev;
	if (entry->prev)
		entry->prev->next = entry->next;
	else
		journal->first = entry->next;

	free(entry->name.str);
	zfsd_mutex_lock(&journal_mutex);
	pool_free(journal_pool, entry);
	zfsd_mutex_unlock(&journal_mutex);
	htab_clear_slot(journal->htab, slot);

	return true;
}

/*! Print the journal JOURNAL to file F.  */

void print_journal(FILE * f, journal_t journal)
{
	journal_entry entry;

	for (entry = journal->first; entry; entry = entry->next)
	{
		switch (entry->oper)
		{
		case JOURNAL_OPERATION_ADD:
			fprintf(f, "  ADD");
			break;

		case JOURNAL_OPERATION_DEL:
			fprintf(f, "  DEL");
			break;

		default:
			fprintf(f, "  ???");
			break;
		}

		fprintf(f, " %s %" PRIu32 ",%" PRIu32 ",%" PRIu32,
				entry->name.str, entry->dev, entry->ino, entry->gen);
		fprintf(f, " [%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%"
				PRIu32 "]\n", entry->master_fh.sid, entry->master_fh.vid,
				entry->master_fh.dev, entry->master_fh.ino,
				entry->master_fh.gen);
	}
}

/*! Print the journal JOURNAL to STDERR.  */

void debug_journal(journal_t journal)
{
	print_journal(stderr, journal);
}

/*! Initialize data structures in JOURNAL.C.  */

void initialize_journal_c(void)
{
	zfsd_mutex_init(&journal_mutex);
	journal_pool = create_alloc_pool("journal_pool",
									 sizeof(struct journal_entry_def),
									 1020, &journal_mutex);
}

/*! Destroy data structures in JOURNAL.C.  */

void cleanup_journal_c(void)
{
	zfsd_mutex_lock(&journal_mutex);
#ifdef ENABLE_CHECKING
	if (journal_pool->elts_free < journal_pool->elts_allocated)
		message(LOG_WARNING, FACILITY_MEMORY,
				"Memory leak (%u elements) in journal_pool.\n",
				journal_pool->elts_allocated - journal_pool->elts_free);
#endif
	free_alloc_pool(journal_pool);
	zfsd_mutex_unlock(&journal_mutex);
	zfsd_mutex_destroy(&journal_mutex);
}
