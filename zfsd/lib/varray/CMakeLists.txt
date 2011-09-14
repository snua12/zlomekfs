add_library(varray ${BUILDTYPE} varray.c)
target_link_libraries(varray)

### google Test
#test_enabled(gtest result)
#if(NOT result EQUAL -1)

 #       SET(varray_test_SRCS
 #          varray_test.cpp
 #       )
    
        #set_source_files_properties(${varray_test_SRCS} COMPILE_FLAGS)

  #      add_executable(varray_test ${varray_test_SRCS})
  #      target_link_libraries(varray_test ${ZFS_GTEST_LIBRARIES} varray)
  #      add_test(varray_test varray_test)

#endif()

install(
TARGETS varray
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

