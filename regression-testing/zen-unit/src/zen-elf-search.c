/*! \file
    \brief Implementation of elf binary search.
    @see zen-elf-search.h

*/

/* Copyright (C) 2007, 2008 Jiri Zouhar

   This file is part of Zen Unit.

   Zen-unit is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Zen-unit is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   Zen-unit; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html 
*/

#include "config.h"

#ifdef HAVE_LIBELF_LIBELF_H
#include <libelf/libelf.h>
#endif
#ifdef HAVE_LIBELF_GELF_H
#include <libelf/gelf.h>
#endif
#ifdef HAVE_LIBELF_H
#include <libelf.h>
#endif
#ifdef HAVE_GELF_H
#include <gelf.h>
#endif


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "zen-elf-search.h"

/// Walk through  elf symbol table.
/** Walk through one elf symbol and calls callback_func on every function symbol found in table.
 *
 * @param elf libelf opened elf object handler
 * @param section elf section descriptor
 * @param header elf section header descriptor
 * @param callback_func callback for reporting symbols
 * @param data cookie for callbacks and reporting structure at the same time.
 * @param offset where the object is mapped into memory
 * @see report_callback_def
*/
void walk_symtab (Elf * elf, Elf_Scn * section, GElf_Shdr * header,
	report_callback_def callback_func, void * callback_data, off_t offset)
{
	Elf_Data * data = NULL;
	if (!(data = elf_getdata(section,data)))
	{
		report_error ("can't read data for section\n");
		return;
	}

	unsigned int symbol_count = header->sh_size / header->sh_entsize;
	unsigned int index = 0;

	for (index = header->sh_info; index < symbol_count; index ++)
	{
		GElf_Sym symbol;
		char * name = NULL;
		if (!gelf_getsym (data, index, &symbol) 
			|| symbol.st_shndx == SHN_UNDEF)
			continue;

		if (symbol.st_value != 0 && // only resolved symbols
			(name = elf_strptr(elf,header->sh_link, symbol.st_name))
			!= NULL && // with name
			GELF_ST_TYPE(symbol.st_info) == STT_FUNC) // only functions
		{
			callback_func (name, (void *)(offset + symbol.st_value), callback_data);
		}
	}
}

/// Walk through elf file (opened).
/** Walk through elf file and calls walk_symtab on every symbol table found.
 *
 * @param desc libelf opened elf object handler
 * @param callback_func callback for reporting symbols
 * @param data cookie for callbacks and reporting structure at the same time.
 * @param offset where the object is mapped into memory
 * @see report_callback_def
 * @see walk_symtab
*/
zen_error walk_sections (Elf * desc, report_callback_def callback_func,
	void * data, off_t offset)
{
	Elf_Scn * section = NULL;
	GElf_Shdr sec_header;
	while ((section = elf_nextscn(desc, section)))
	{
		if (gelf_getshdr (section, &sec_header) != &sec_header)
			continue;

		if (sec_header.sh_type == SHT_SYMTAB ||
			sec_header.sh_type == SHT_DYNSYM)
			walk_symtab (desc, section, &sec_header, callback_func, data, offset);
	}

	return ZEN_NOERR;

}

/// search in elf file for symbols
zen_error walk_elf_file (const char * name, report_callback_def callback_func, 
	void * data, off_t offset)
{
	zen_error ret_code = ZEN_NOERR;
	int fd = open (name, O_RDONLY);
	if (fd < 0)
	{
		report_error ("can't open file %s\n", name);
		ret_code = ZEN_ERR_BAD_FILE;
		goto FINISHING;
	}

	if (elf_version(EV_CURRENT) == EV_NONE)
	{
		int err = elf_errno();
		report_error ("can't get elf version\n\t%d: %s\n",
			err, elf_errmsg(err));
		ret_code = elf_to_zen_err (err);
		goto FINISHING;
	}


	Elf * desc = NULL;
	desc = elf_begin (fd, ELF_C_READ, desc);
	if (desc == NULL)
	{
		int err = elf_errno();
		report_error ("can't begin elf\n\n%d: %s\n",
			err, elf_errmsg(err));
		ret_code = elf_to_zen_err (err);
		goto FINISHING;
	}
	
	ret_code = walk_sections (desc, callback_func, data, offset);

FINISHING:
	if (desc != NULL)
		elf_end (desc);

	if (fd >=0)
		close (fd);

	return ret_code;
}
