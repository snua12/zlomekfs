add_library(version ${BUILDTYPE} version.c)
target_link_libraries(version zfs_dirent)

install(
TARGETS version
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

