# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

add_library(fuse_iface ${BUILDTYPE} fuse_iface.c)

target_link_libraries(fuse_iface threading protocol ${FUSE_LIBRARIES})

install(
TARGETS fuse_iface
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

