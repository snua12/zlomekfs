add_library(zfs_config ${BUILD_TYPE} zfs_config.c)

test_enabled(gtest result)
if(NOT reqult EQUAL -1)
#	add_executable(local_config_test local_config.c local_config_test.cpp config_defaults.c zfs_config.c)
#	target_link_libraries(local_config_test ${ZFS_GTEST_LIBRARIES} ${LCONFIG_LIBRARIES} log memory)
#	add_test(local_cofnig_test local_config_test)
endif()

install(
TARGETS zfs_config
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

