# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

	SET (threading_SRCS pthread-wrapper.c thread.c)
else()
	SET (threading_SRCS pthread-wrapper.c thread.c barrier.c)
endif()

add_library(threading ${BUILDTYPE} ${threading_SRCS})
target_link_libraries(threading semaphore queue)

install(
TARGETS threading
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

