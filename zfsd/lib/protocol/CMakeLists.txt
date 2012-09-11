# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

#removed
#add_subdirectory(protobuf)

target_link_libraries(protocol dir)

install(
TARGETS protocol
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

