# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

add_executable(POSIXapi_test POSIXapi_test.c file_tests.c filename_generator.c dir_tests.c)

target_link_libraries(win32api_test syscall_collector)

