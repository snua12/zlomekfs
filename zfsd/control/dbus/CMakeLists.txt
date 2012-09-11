# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

target_link_libraries(controlDbus ${DBUS_LIBRARIES} log)

### google Test
test_enabled(gtest result)
if(NOT result EQUAL -1)

        SET(dbus-provider_test_SRCS
           dbus-provider_test.cpp
        )
    
        #set_source_files_properties(${dbus-provider_test_SRCS} COMPILE_FLAGS)

        add_executable(dbus-provider_test ${dbus-provider_test_SRCS})
        target_link_libraries(dbus-provider_test ${ZFS_GTEST_LIBRARIES} controlDbus)
        add_test(dbus-provider_test dbus-provider_test)

endif()

install(
TARGETS controlDbus
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

