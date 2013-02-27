# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

SET(zfsd_SRCS
   zfsd.c
   zfsd_state.c
   zfsd_args_shared.c
)

if (ANDROID)
	SET(pthread_lib)
else()
	SET(pthread_lib "pthread")
endif()

SET(zfsd_LIBS
    ${LCONFIG_LIBRARIES}
    ${pthread_lib}
    threading
    protocol
    zfs_log
    memory
    control
)

if (HAVE_FUSE)
	SET(zfsd_SRCS ${zfsd_SRCS} zfsd_args_fuse.c)
	SET(zfsd_LIBS ${zfsd_LIBS} fuse_iface)
elseif(HAVE_DOKAN)
	SET(zfsd_SRCS ${zfsd_SRCS} zfsd_args_nofuse.c)
	SET(zfsd_LIBS ${zfsd_LIBS} dokan_iface)
else()
	SET(zfsd_SRCS ${zfsd_SRCS} zfsd_args_nofuse.c)
endif()

if (ENABLE_HTTP_INTERFACE)
	SET(zfsd_LIBS ${zfsd_LIBS} http_iface microhttpd)
endif()

add_executable(zfsd ${zfsd_SRCS})
target_link_libraries(zfsd  ${zfsd_LIBS})

# dumb cmake 2.6 needs this hint (some code is in C++)
set_target_properties(zfsd PROPERTIES LINKER_LANGUAGE CXX)

#### Install

install(
TARGETS zfsd
DESTINATION ${ZFS_INSTALL_DIR}/bin
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
