# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

add_library(http_iface ${BUILDTYPE} http_iface.c) 
target_link_libraries(http_iface protocol ${DOKAN_LIBRARIES} pthread ${MHD_LIBRARIES})

install(
TARGETS http_iface
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

