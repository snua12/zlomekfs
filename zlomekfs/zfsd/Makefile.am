# Makefile.
# Copyright (C) 2007, 2008 Jiri Zouhar
# Copyright (C) other contributors as outlined in CREDITS
#
# This file is part of zlomekfs
#
# zlomekfs is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# zlomekfs is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with zlomekfs; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

EXTRA_DIST = zfs-prot.def

INCLUDES = \
	-I${top_srcdir}/fuse/include \
	-I/usr/include/syplog \
	-I/usr/include/dbus-1.0 \
	-I/usr/lib/dbus-1.0/include 

AM_LDFLAGS = -lrt -ldl -lpthread -ldbus-1

AM_CFLAGS = -Wall -W -Wcast-align -Wcast-qual -Wmissing-noreturn \
	-Wnested-externs -Wpointer-arith -Wshadow -Wstrict-prototypes \
	-Wundef -pthread -D_FILE_OFFSET_BITS=64 -pthread -DUSE_SYSLOG 

${top_builddir}/fuse/lib/.libs/libfuse.a:
	make -C ${top_builddir}/fuse/

bin_PROGRAMS = dump-metadata dump-intervals
sbin_PROGRAMS = zfsd

nobase_pkginclude_HEADERS = \
	dbus-service-descriptors.h

noinst_HEADERS = alloc-pool.h cap.h config.h constant.h crc32.h data-coding.h \
	dbus-provider.h dbus-zfsd-service.h dir.h fh.h fibheap.h file.h \
	hardlink-list.h hashfile.h hashtab.h interval.h journal.h  kernel.h log.h \
	md5.h memory.h metadata.h network.h node.h pthread-wrapper.h queue.h random.h \
	semaphore.h splay-tree.h system.h thread.h update.h user-group.h util.h \
	varray.h volume.h zfsd.h  zfs-prot.h


zfsd_SOURCES = zfsd.c \
	  alloc-pool.c \
	  cap.c config.c constant.c crc32.c \
	  data-coding.c dir.c \
	  fh.c fibheap.c file.c \
	  hardlink-list.c hashfile.c hashtab.c \
	  interval.c \
	  journal.c \
	  kernel.c \
	  log.c \
	  md5.c memory.c metadata.c \
	  network.c node.c \
	  pthread-wrapper.c \
	  queue.c \
	  random.c \
	  semaphore.c splay-tree.c \
	  thread.c \
	  update.c user-group.c util.c \
	  varray.c volume.c\
	  zfs-prot.c \
	  dbus-zfsd-service.c \
	  dbus-provider.c
zfsd_LDADD =  -lsyplog ${top_builddir}/fuse/lib/.libs/libfuse.a


dump_metadata_SOURCES = dump-metadata.c \
		   alloc-pool.c \
		   constant.c crc32.c \
		   fibheap.c \
		   hardlink-list.c hashfile.c hashtab.c \
		   interval.c \
		   journal.c \
		   log.c \
		   memory.c metadata.c \
		   node.c \
		   pthread-wrapper.c \
		   splay-tree.c \
		   user-group.c \
		   util.c \
		   varray.c
dump_metadata_LDADD = ${top_builddir}/fuse/lib/.libs/libfuse.a

dump_intervals_SOURCES = dump-intervals.c \
		    alloc-pool.c \
		    interval.c \
		    log.c \
		    node.c \
		    crc32.c \
		    hashtab.c \
		    user-group.c \
		    pthread-wrapper.c \
		    memory.c \
		    splay-tree.c \
		    util.c \
		    varray.c
dump_intervals_LDADD = ${top_builddir}/fuse/lib/.libs/libfuse.a

test: zfsd
	./test.py ./zfsd
