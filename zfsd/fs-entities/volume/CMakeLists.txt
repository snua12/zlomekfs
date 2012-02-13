add_library(volume ${BUILDTYPE} volume.c)
target_link_libraries(volume metadata fh dir)

install(
TARGETS volume
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

