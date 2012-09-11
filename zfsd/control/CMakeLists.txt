# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.


if (ENABLE_DBUS)
# dbus
PKG_CHECK_MODULES(DBUS dbus-1>=1.0)
  if (NOT FUSE_FOUND EQUAL 1)
    message(FATAL_ERROR "dbus not found")
  endif()

  add_subdirectory(dbus)
  set(CONTROL_DBUS_LIBS controlDbus)
endif (ENABLE_DBUS)

add_library(syplog_control_wrapper ${BUILDTYPE} syplog_wrapper.c)
target_link_libraries(syplog_control_wrapper log)

add_library(control ${BUILDTYPE} control.c)
target_link_libraries(control ${CONTROL_DBUS_LIBS} log syplog_control_wrapper)

add_executable(control_test control_test.c)
target_link_libraries(control_test control)

install(
TARGETS control syplog_control_wrapper
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

