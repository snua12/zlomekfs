# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

if(HAVE_FOPENCOOKIE)
	SET(zfsio_SRC
	   zfsio_fopencookie.c
	)
else()
	SET(zfsio_SRC
	   zfsio_mkstemp.c
	)
endif()

add_library(zfsio ${BUILDTYPE} ${zfsio_SRC})
target_link_libraries(zfsio)

install(
TARGETS zfsio
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

