if (ENABLE_DBUS)
  add_subdirectory(dbus)
  set(CONTROL_DBUS_LIBS controlDbus)
endif (ENABLE_DBUS)

add_library(control ${BUILDTYPE} control.c)
target_link_libraries(control ${CONTROL_DBUS_LIBS} log)

add_executable(controlTest controlTest.c)
target_link_libraries(controlTest control)

install(
TARGETS control
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

