# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.


add_library(dokan_tools ${BUILDTYPE} dokan_tools.c) 
target_link_libraries(dokan_tools log)

add_library(dokan_iface ${BUILDTYPE} dokan_iface.c) 
target_link_libraries(dokan_iface dokan_tools protocol ${DOKAN_LIBRARIES} pthread)

### google Test
test_enabled(gtest result)
if(NOT result EQUAL -1)

        SET(dokan_tools_test_SRCS
           dokan_tools_test.cpp
        )
    
        #set_source_files_properties(${dokan_tools_test_SRCS} COMPILE_FLAGS)

        add_executable(dokan_tools_test ${dokan_tools_test_SRCS})
        target_link_libraries(dokan_tools_test ${ZFS_GTEST_LIBRARIES} dokan_tools)
        add_test(dokan_tools_test dokan_tools_test)

endif()

install(
TARGETS dokan_iface
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

