add_library(fibheap ${BUILDTYPE} fibheap.c)
target_link_libraries(fibheap varray log)

install(
TARGETS fibheap
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

