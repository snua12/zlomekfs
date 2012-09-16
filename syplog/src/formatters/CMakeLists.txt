# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.


SET(formatters_SRCS
  formatter.c
  raw-formatter.c
  user-readable-formatter.c
)

add_library(formatters ${BUILDTYPE} ${formatters_SRCS})
target_link_libraries(formatters)


test_enabled(gtest result)
if(NOT result EQUAL -1)

       SET(formatters_test_SRCS
         formatter_test.cpp
        )
    
        #set_source_files_properties(${formatter_test_SRCS} COMPILE_FLAGS)

        add_executable(formatters_test ${formatters_test_SRCS})
        target_link_libraries(formatters_test ${ZFS_GTEST_LIBRARIES} formatters)
        add_test(formatters_test formatters_test)

endif()

install(
TARGETS formatters
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

