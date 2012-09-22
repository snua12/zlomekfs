# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

include_directories(${PROTOBUF_C_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
PROTOBUF_C_GENERATE_C(PROTO_SRCS PROTO_HDRS  zfs.proto)
add_executable(server server.c ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(server ${PROTOBUF_C_LIBRARY})

add_executable(client client.c ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(client ${PROTOBUF_C_LIBRARY})

