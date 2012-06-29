add_executable(POSIXapi_test POSIXapi_test.c file_tests.c filename_generator.c dir_tests.c)
target_link_libraries(POSIXapi_test syscall_collector)

