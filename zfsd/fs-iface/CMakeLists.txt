# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

add_subdirectory(tests)

if (HAVE_FUSE)
	add_subdirectory(fuse_iface)
elseif (HAVE_DOKAN)
	add_subdirectory(dokan_iface)
endif()

