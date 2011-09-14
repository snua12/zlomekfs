add_library(controlDbus ${BUILDTYPE} dbus-provider.c  dbus-zfsd-service.c)
target_link_libraries(contolDbus)

install(
TARGETS controlDbus
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

