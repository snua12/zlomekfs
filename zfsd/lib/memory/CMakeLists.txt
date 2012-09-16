# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

add_library(memory ${BUILDTYPE} memory.c)

target_link_libraries(memory varray log)

### google Test
test_enabled(gtest result)
if(NOT result EQUAL -1)

        SET(memory_test_SRCS
           memory_test.cpp
        )
    
        #set_source_files_properties(${memory_test_SRCS} COMPILE_FLAGS)

        add_executable(memory_test ${memory_test_SRCS})
        target_link_libraries(memory_test ${ZFS_GTEST_LIBRARIES} memory)
        add_test(memory_test memory_test)

endif()

install(
TARGETS memory
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

