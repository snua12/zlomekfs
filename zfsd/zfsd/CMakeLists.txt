SET(zfsd_SRCS
   zfsd.c
   zfsd_state.c
)

add_executable(zfsd ${zfsd_SRCS})
target_link_libraries(zfsd ${FUSE_LIBRARIES} ${LCONFIG_LIBRARIES} log memory fuse_iface control)

# dumb cmake 2.6 needs this hint (some code is in C++)
set_target_properties(zfsd PROPERTIES LINKER_LANGUAGE CXX)

#test_enabled(unit result)
if(NOT result EQUAL -1)

#	SET(zfsTest_SRCS
#	)

#	add_executable(zfsTest ${zfsTest_SRCS})
#	target_link_libraries(zfsTest ${IPC_GTEST_LIBRARIES})
#	add_test(zfsTest zfsTest)

endif()

#### Install

install(
TARGETS zfsd
DESTINATION ${ZFS_INSTALL_DIR}/bin
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
