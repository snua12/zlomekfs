# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

include_directories(
.
${ZFSD_SOURCE_DIR}/configuration
${ZFSD_SOURCE_DIR}/configuration/config_reader
${ZFSD_SOURCE_DIR}/configuration/zfs_config
${ZFSD_SOURCE_DIR}/configuration/iface
${ZFSD_SOURCE_DIR}/control
${ZFSD_SOURCE_DIR}/control/dbus
${ZFSD_SOURCE_DIR}/include
${ZFSD_SOURCE_DIR}/user-group
${ZFSD_SOURCE_DIR}/zfsd
${ZFSD_SOURCE_DIR}/fs-entities
${ZFSD_SOURCE_DIR}/fs-entities/journal
${ZFSD_SOURCE_DIR}/fs-entities/file
${ZFSD_SOURCE_DIR}/fs-entities/handle
${ZFSD_SOURCE_DIR}/fs-entities/hardlink
${ZFSD_SOURCE_DIR}/fs-entities/node
${ZFSD_SOURCE_DIR}/fs-entities/metadata
${ZFSD_SOURCE_DIR}/fs-entities/dir
${ZFSD_SOURCE_DIR}/fs-entities/cap
${ZFSD_SOURCE_DIR}/fs-entities/volume
${ZFSD_SOURCE_DIR}/tools
${ZFSD_SOURCE_DIR}/update
${ZFSD_SOURCE_DIR}/lib
${ZFSD_SOURCE_DIR}/lib/log
${ZFSD_SOURCE_DIR}/lib/hashtab
${ZFSD_SOURCE_DIR}/lib/util
${ZFSD_SOURCE_DIR}/lib/semaphore
${ZFSD_SOURCE_DIR}/lib/queue
${ZFSD_SOURCE_DIR}/lib/threading
${ZFSD_SOURCE_DIR}/lib/constant
${ZFSD_SOURCE_DIR}/lib/memory
${ZFSD_SOURCE_DIR}/lib/varray
${ZFSD_SOURCE_DIR}/lib/interval-tree
${ZFSD_SOURCE_DIR}/lib/crc32
${ZFSD_SOURCE_DIR}/lib/hashfile
${ZFSD_SOURCE_DIR}/lib/alloc-pool
${ZFSD_SOURCE_DIR}/lib/network
${ZFSD_SOURCE_DIR}/lib/protocol
${ZFSD_SOURCE_DIR}/lib/splay-tree
${ZFSD_SOURCE_DIR}/lib/random
${ZFSD_SOURCE_DIR}/lib/md5
${ZFSD_SOURCE_DIR}/lib/fibheap
${ZFSD_SOURCE_DIR}/lib/zfsio
${ZFSD_SOURCE_DIR}/lib/zfs_dirent
${ZFSD_SOURCE_DIR}/version
${ZFSD_SOURCE_DIR}/fs-iface/include
)

add_subdirectory(configuration)
add_subdirectory(control)
add_subdirectory(user-group)
add_subdirectory(lib)
add_subdirectory(fs-entities)
add_subdirectory(zfsd)
add_subdirectory(tools)
add_subdirectory(update)

if (ENABLE_VERSIONS)
  add_subdirectory(version)
endif(ENABLE_VERSIONS)

if (ENABLE_FS_INTERFACE)
  add_subdirectory(fs-iface)
endif()

