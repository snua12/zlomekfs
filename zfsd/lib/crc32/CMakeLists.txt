add_library(crc32 ${BUILDTYPE} crc32.c)
target_link_libraries(crc32)

### google Test
test_enabled(gtest result)
if(NOT result EQUAL -1)

        SET(crc32_test_SRCS
           crc32_test.cpp
        )
    
        #set_source_files_properties(${crc32_test_SRCS} COMPILE_FLAGS)

        add_executable(crc32_test ${crc32_test_SRCS})
        target_link_libraries(crc32_test ${ZFS_GTEST_LIBRARIES} crc32)
        add_test(crc32_test crc32_test)

endif()

install(
TARGETS crc32
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

