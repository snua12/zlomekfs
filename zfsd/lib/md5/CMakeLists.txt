# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

add_library(md5 ${BUILDTYPE} md5.c)

target_link_libraries(md5)

### google Test
test_enabled(gtest result)
if(NOT result EQUAL -1)

        SET(md5_test_SRCS
           md5_test.cpp
        )
    
        #set_source_files_properties(${md5_test_SRCS} COMPILE_FLAGS)

        add_executable(md5_test ${md5_test_SRCS})
        target_link_libraries(md5_test ${ZFS_GTEST_LIBRARIES} md5)
        add_test(md5_test md5_test)

endif()

install(
TARGETS md5
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

