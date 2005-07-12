#
# Makefile for the Linux ZFS filesystem routines.
#
# If you want debugging output, please uncomment the following line.

# EXTRA_CFLAGS += -DDEBUG

ifneq ($(KERNELRELEASE),)

ifeq ($(CONFIG_ZFS_FS),)
CONFIG_ZFS_FS := m
endif

obj-$(CONFIG_ZFS_FS) += zfs.o
zfs-objs := chardev.o data-coding.o dir.o file.o inode.o super.o zfs_prot.o zfsd_call.o

else

KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

zfs.ko:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

install: zfs.ko
	mkdir -p $(DESTDIR)/lib/modules/$(shell uname -r)/kernel/fs/zfs
	install -m 644 -o 0 -g 0 zfs.ko $(DESTDIR)/lib/modules/$(shell uname -r)/kernel/fs/zfs/
	depmod -a

clean:
	-rm -f *.o *.ko .*.cmd

endif
