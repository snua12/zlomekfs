# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

target_link_libraries(hashfile util)

install(
TARGETS hashfile
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

