/*! \file
    \brief

*/

/* Copyright (C) 2007 Jiri Zouhar

   This file is part of Zen Unit

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
   or download it from http://www.gnu.org/licenses/gpl.html 
*/

#include <libelf/libelf.h>
#include <libelf/gelf.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "zen-elf-search.h"

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

		if (symbol.st_value != 0 &&
			(name = elf_strptr(elf,header->sh_link, symbol.st_name)) != NULL)
		{
			callback_func (name, (void *)(offset + symbol.st_value), callback_data);
		}
	}
}

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
