# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

target_link_libraries(alloc-pool memory)

### google Test
test_enabled(gtest result)
if(NOT result EQUAL -1)

        SET(alloc-pool_test_SRCS
           alloc-pool_test.cpp
        )
    
        #set_source_files_properties(${alloc-pool_test_SRCS} COMPILE_FLAGS)

        add_executable(alloc-pool_test ${alloc-pool_test_SRCS})
        target_link_libraries(alloc-pool_test ${ZFS_GTEST_LIBRARIES} alloc-pool)
        add_test(alloc-pool_test alloc-pool_test)

endif()

install(
TARGETS alloc-pool
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

