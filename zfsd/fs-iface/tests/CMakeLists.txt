# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.


include_directories(syscall_collector)

if (HAVE_DOKAN)
add_subdirectory(win32api_test)
endif()
add_subdirectory(syscall_collector)
add_subdirectory(POSIXapi_test)

