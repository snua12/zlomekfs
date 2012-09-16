# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

add_library(splay-tree ${BUILDTYPE} splay-tree.c)

target_link_libraries(splay-tree varray util memory alloc-pool)

install(
TARGETS splay-tree
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

