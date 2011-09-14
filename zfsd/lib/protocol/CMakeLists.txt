add_library(protocol ${BUILDTYPE} data-coding.c  zfs-prot.c)
target_link_libraries(protocol dir)

install(
TARGETS protocol
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

