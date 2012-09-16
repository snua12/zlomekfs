# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

add_library(user-group ${BUILDTYPE} user-group.c)
#TODO node is to big, needs thisnode variable
target_link_libraries(user-group node)

install(
TARGETS user-group
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

