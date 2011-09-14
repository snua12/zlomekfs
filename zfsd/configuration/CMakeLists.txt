add_subdirectory(config_reader)
add_subdirectory(zfs_config)

SET(configuration_SRC
  configuration.c
  config_parser.c
  reread_config.c
  config_user_mapping.c
  config_group_mapping.c
  config_user.c
  config_group.c
  cluster_config.c
  config_volume.c
)

add_library(configuration ${BUILDTYPE} ${configuration_SRC})
target_link_libraries(configuration zfs_config log zfsio config_reader)

## google test
test_enabled(gtest result)
if(NOT reqult EQUAL -1)
#	add_executable(local_config_test local_config.c local_config_test.cpp config_defaults.c zfs_config.c)
#	target_link_libraries(local_config_test ${ZFS_GTEST_LIBRARIES} ${LCONFIG_LIBRARIES} log memory)
#	add_test(local_cofnig_test local_config_test)
endif()

install(
TARGETS configuration
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

