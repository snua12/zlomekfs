# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

SET(config_reader_SRC
	local_config.c
	shared_config.c
	config_common.c
)

add_library(config_reader ${BUILD_TYPE} ${config_reader_SRC})
target_link_libraries(config_reader zfs_config log zfsio ${LCONFIG_LIBRARIES})

## google test
test_enabled(gtest result)
if(NOT result EQUAL -1)

	SET(config_reader_test_SRC config_reader_test.cpp)

	add_executable(config_reader_test ${config_reader_test_SRC})
	target_link_libraries(config_reader_test ${ZFS_GTEST_LIBRARIES} config_reader)
	add_test(config_reader_test config_reader_test)
endif()

install(
TARGETS config_reader
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

