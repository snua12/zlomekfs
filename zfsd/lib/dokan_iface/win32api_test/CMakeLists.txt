add_executable(win32api_test win32api_test.c file_tests.c filename_generator.c dir_tests.c)
target_link_libraries(win32api_test log)

