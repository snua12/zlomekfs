# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

if (ENABLE_DBUS)
# dbus
PKG_CHECK_MODULES(DBUS dbus-1>=1.0)
  if (NOT DBUS_FOUND EQUAL 1)
    message(FATAL_ERROR "dbus not found")
  endif()

  add_subdirectory(dbus)
  set(CONTROL_DBUS_LIBS controlDbus)
endif (ENABLE_DBUS)

if (ENABLE_CLI)
	add_subdirectory(cli)
	set(CMAKE_C_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/cli/inc ${CMAKE_C_FLAGS}")
	set(CONTROL_LIBS zfsd_cli cli)
endif (ENABLE_CLI)

add_library(syplog_control_wrapper ${BUILDTYPE} syplog_wrapper.c)
target_link_libraries(syplog_control_wrapper zfs_log)

add_library(control ${BUILDTYPE} control.c)
target_link_libraries(control ${CONTROL_DBUS_LIBS} zfs_log syplog_control_wrapper ${CONTROL_LIBS})

add_executable(control_test control_test.c)
target_link_libraries(control_test control)

install(
TARGETS control syplog_control_wrapper
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

