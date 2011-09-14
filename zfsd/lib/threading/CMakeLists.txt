add_library(threading ${BUILDTYPE} pthread-wrapper.c  thread.c)
target_link_libraries(threading semaphore queue)

install(
TARGETS threading
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

