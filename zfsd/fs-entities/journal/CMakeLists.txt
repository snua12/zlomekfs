add_library(journal ${BUILDTYPE} journal.c)
target_link_libraries(journal hashtab crc32)

install(
TARGETS journal
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

