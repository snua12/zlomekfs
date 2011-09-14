add_library(fuse_iface ${BUILDTYPE} kernel.c)
target_link_libraries(fuse_iface threading protocol)

install(
TARGETS fuse_iface
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

