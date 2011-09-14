add_library(configuration ${BUILDTYPE} configuration.c)
target_link_libraries(configuration)

install(
TARGETS configuration
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

