
add_executable(dump-intervals dump-intervals.c)
target_link_libraries(dump-intervals ${FUSE_LIBRARIES} interval-tree)

add_executable(dump-metadata dump-metadata.c)
target_link_libraries(dump-metadata ${FUSE_LIBRARIES} metadata journal)

# dumb cmake 2.6 needs this hint (some code is in C++)
#set_target_properties(zfsd PROPERTIES LINKER_LANGUAGE CXX)

#### Install
install(
TARGETS dump-intervals dump-metadata
DESTINATION ${ZFS_INSTALL_DIR}/bin
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
