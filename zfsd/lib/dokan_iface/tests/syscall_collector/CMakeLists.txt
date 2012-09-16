# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

add_library(syscall_collector ${BUILDTYPE} syscall_collector.c)

target_link_libraries(syscall_collector m)

