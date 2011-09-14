add_library(hardlink ${BUILDTYPE} hardlink-list.c)
target_link_libraries(hardlink threading)

install(
TARGETS hardlink
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

