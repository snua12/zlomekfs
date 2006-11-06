# Makefile.
# Copyright (C) 2003, 2004 Josef Zlomek
# Copyright (C) other contributors as outlined in CREDITS
#
# This file is part of zlomekFS.
#
# zlomekFS is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# zlomekFS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with zlomekFS; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA


# This makefile invokes the kernel build system to build
# the module. Since the kernel build system requires
# that a single makefile only modifies a single
# directory, we need the symbolic links to
# the files shared with the daemon.

# A drawback of this solution is that the compiler settings
# for the module are different from the compiler settings
# for the daemon, which has a potential to wreak havoc.


ifdef KERNELRELEASE
# We are inside the kernel build system

CONFIG_ZFS_FS ?= m

obj-$(CONFIG_ZFS_FS) += zfs.o
zfs-objs := chardev.o data-coding.o dir.o file.o inode.o super.o zfs-prot.o zfsd-call.o

else
# We are outside the kernel build system

.PHONY: zfs.ko install clean

INSTALL = install

KK_DIR = /lib/modules/$(shell uname -r)/build
KK_MOD = $(shell pwd)

zfs.ko:
	$(MAKE) -C $(KK_DIR) M=$(KK_MOD) modules

install: zfs.ko
	mkdir -p /lib/modules/$(shell uname -r)/kernel/fs/zfs
	$(INSTALL) -m 644 -o 0 -g 0 zfs.ko /lib/modules/$(shell uname -r)/kernel/fs/zfs
	depmod -a

clean:
	-rm -rf *.o *.ko *.mod.c .*.cmd .tmp_versions Modules.symvers

endif

