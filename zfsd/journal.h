/*! \file
    \brief Journal datatype.  */

/* Copyright (C) 2004 Josef Zlomek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html */

#ifndef JOURNAL_H
#define JOURNAL_H

#include "system.h"
#include <inttypes.h>
#include <stdio.h>
#include "pthread.h"
#include "memory.h"
#include "hashtab.h"
#include "crc32.h"
#include "zfs_prot.h"

/*! Hash function for journal entry J.  */
#define JOURNAL_HASH(J)							\
  (crc32_update (crc32_buffer ((J)->name.str, (J)->name.len),		\
		 &(J)->oper, sizeof ((J)->oper)))

/*! Operation stored to journal.  */
typedef enum journal_operation_def
{
  JOURNAL_OPERATION_ADD,		/*!< add directory entry */
  JOURNAL_OPERATION_DEL,		/*!< delete directory entry */
  JOURNAL_OPERATION_LAST_AND_UNUSED
} journal_operation_t;

/*! Journal entry.  */
typedef struct journal_entry_def *journal_entry;
struct journal_entry_def
{
  journal_entry next;		/*!< next entry in the doubly linked chain */
  journal_entry prev;		/*!< previous entry in the doubly linked chain */

  uint32_t dev;			/*!< device of the local file handle */
  uint32_t ino;			/*!< inode of the local file handle */
  uint32_t gen;			/*!< generation of the local file handle */
  journal_operation_t oper;	/*!< journaled operation */
  string name;			/*!< name of local file */
  zfs_fh master_fh;		/*!< master file handle */
  uint64_t master_version;	/*!< master version of the file */
};

/*! Definition of journal datatype.  */
typedef struct journal_def
{
  /*! Hash table.  */
  htab_t htab;

  /*! Mutex which must be locked when accessing the journal.  */
  pthread_mutex_t *mutex;

  /*! First and last node of the doubly-linked chain.  */
  journal_entry first;
  journal_entry last;

  /*! File descriptor associated with the journal.  */
  int fd;

  /*! Generation of opened file descriptor.  */
  unsigned int generation;
} *journal_t;

extern journal_t journal_create (unsigned int nelem, pthread_mutex_t *mutex);
extern void journal_empty (journal_t journal);
extern void journal_destroy (journal_t journal);
extern bool journal_insert (journal_t journal, journal_operation_t oper,
			    zfs_fh *local_fh, zfs_fh *master_fh,
			    uint64_t master_version, string *name, bool copy);
extern bool journal_member (journal_t journal, journal_operation_t oper,
			    string *name);
extern bool journal_delete (journal_t journal, journal_operation_t oper,
			    string *name);
extern bool journal_delete_entry (journal_t journal, journal_entry entry);
extern void print_journal (FILE *f, journal_t journal);
extern void debug_journal (journal_t journal);

extern void initialize_journal_c (void);
extern void cleanup_journal_c (void);

#endif
