include_directories(${PROTOBUF_C_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
PROTOBUF_C_GENERATE_C(PROTO_SRCS PROTO_HDRS  zfs.proto)
add_executable(main main.c ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(main ${PROTOBUF_C_LIBRARY})

