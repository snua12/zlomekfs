SET(syplog_SRCS
  syplog.c
	formatters/formatter.c
	settings-loader.c
	formatters/user-readable-formatter.c
	formatters/raw-formatter.c
	media/medium.c
	media/file-medium.c
	media/print-medium.c
	media/shm-medium.c
	media/medium-api.c
	syp-error.c
	control/control-protocol.c
	control/listener.c
	control/control.c
)


add_library(syplog ${BUILDTYPE} ${syplog_SRCS})
target_link_libraries(syplog varray log)

### google Test
#test_enabled(gtest result)
#if(NOT result EQUAL -1)

#       SET(syplog_test_SRCS
#         syplog_test.cpp
#        )
    
        #set_source_files_properties(${syplog_test_SRCS} COMPILE_FLAGS)

#        add_executable(syplog_test ${memory_test_SRCS})
 #       target_link_libraries(syplog_test ${ZFS_GTEST_LIBRARIES} memory)
 #       add_test(syplog_test memory_test)

#endif()

install(
TARGETS syplog
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

