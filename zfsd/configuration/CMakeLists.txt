SET(configuration_SRC
  configuration.c
  read_config.c
  config_defaults.c
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
target_link_libraries(configuration)

install(
TARGETS configuration
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

