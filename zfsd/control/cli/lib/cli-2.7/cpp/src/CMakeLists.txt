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

set(CLI_CFLAGS_OTHER_STR "-I. -I${CMAKE_CURRENT_SOURCE_DIR}/../include -I/usr/include/ncurses ${CLI_CFLAGS_OTHER_STR}")
set(CMAKE_CXX_FLAGS "${CLI_CFLAGS_OTHER_STR} ${CMAKE_CXX_FLAGS}")

add_library(cli ${BUILDTYPE} ${cli_LIB_SRCS})
target_link_libraries(cli ncurses)

install(
TARGETS cli
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

