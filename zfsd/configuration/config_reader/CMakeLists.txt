SET(config_reader_SRC
	local_config.c
	shared_config.c
)

add_library(config_reader ${BUILD_TYPE} ${config_reader_SRC})
target_link_libraries(config_reader zfs_config log zfsio)

## google test
test_enabled(gtest result)
if(NOT reqult EQUAL -1)
#	add_executable(local_config_test local_config.c local_config_test.cpp config_defaults.c zfs_config.c)
#	target_link_libraries(local_config_test ${ZFS_GTEST_LIBRARIES} ${LCONFIG_LIBRARIES} log memory)
#	add_test(local_cofnig_test local_config_test)
endif()

install(
TARGETS config_reader
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

