include_directories(.)
add_library(linux_readdir closedir.c  opendir.c  readdir.c  seekdir.c  telldir.c readdir_r.c)

add_executable(dir_test dir_test.c)
target_link_libraries(dir_test linux_readdir)

