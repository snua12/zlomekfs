# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

add_executable(win32api_test win32api_test.c file_tests.c filename_generator.c dir_tests.c)
target_link_libraries(win32api_test syscall_collector)

add_executable(win32api_op_test win32api_op_test.c file_tests.c filename_generator.c dir_tests.c)
target_link_libraries(win32api_op_test syscall_collector)

