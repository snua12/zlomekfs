# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

if (ANDROID)
	set(zfs_dirent_SRC zfs_dirent_android.c)
	add_subdirectory(linux)
	set(zfs_dirent_LIBS linux_readdir)
else()
	set(zfs_dirent_SRC zfs_dirent.c)
endif()


add_library(zfs_dirent ${BUILDTYPE} ${zfs_dirent_SRC})

target_link_libraries(zfs_dirent ${zfs_dirent_LIBS})

install(
TARGETS zfs_dirent
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

