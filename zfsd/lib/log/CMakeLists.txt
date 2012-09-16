# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

add_library(log ${BUILDTYPE} log.c)
target_link_libraries(log syplog zfs_config)

### google Test
#test_enabled(gtest result)
#if(NOT result EQUAL -1)

 #       SET(log_test_SRCS
 #          log_test.cpp
 #       )
    
        #set_source_files_properties(${log_test_SRCS} COMPILE_FLAGS)

  #      add_executable(log_test ${log_test_SRCS})
  #      target_link_libraries(log_test ${ZFS_GTEST_LIBRARIES} log)
  #      add_test(log_test log_test)

#endif()

install(
TARGETS log
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

