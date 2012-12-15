# find ncurses libs
PKG_CHECK_MODULES(NCURSES ncurses>=4.0)
if (NCURSES_FOUND EQUAL 1)
	message(STATUS "ncurses found")

	link_directories(${NCURSES_LIBRARY_DIRS})
	include_directories(${NCURSES_INCLUDE_DIRS})
else()
	set(CURSES_NEED_NCURSES FALSE)
	find_package(Curses)
	if (CURSES_FOUND)
		message(STATUS "ncurses found")
		include_directories(${CURSES_INCLUDE_DIRS})
		link_directories(${CURSES_LIBRARY})
	else()
		message(FATAL_ERROR "ncurses not found")
	endif()
endif()


SET(cli_LIB_SRCS
cli.cpp element.cpp
menu.cpp param_int.cpp
syntax_tag.cpp ui_float.cpp
ui_yesno.cpp command_line.cpp
endl.cpp ncurses_console.cpp
param_string.cpp telnet.cpp
ui_int.cpp #win_console.cpp
command_line_edition.cpp
file_device.cpp
non_blocking_io_device.cpp
resource_string.cpp tk.cpp
ui_less.cpp
command_line_history.cpp
help.cpp object.cpp
shell.cpp traces.cpp
ui_line.cpp config_menu.cpp
io_device.cpp param.cpp
single_command.cpp
traces_menu.cpp
ui_more.cpp consistency.cpp
io_mux.cpp param_float.cpp
string_device.cpp ui.cpp
ui_password.cpp debug.cpp
keyword.cpp param_host.cpp
syntax_node.cpp
ui_choice.cpp ui_text.cpp)

include_directories( "." "${CMAKE_CURRENT_SOURCE_DIR}/../include")

add_library(cli ${BUILDTYPE} ${cli_LIB_SRCS})
target_link_libraries(cli ${NCURSES_LIBRARIES})

install(
TARGETS cli
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

