if (HAVE_DOKAN)
add_subdirectory(win32api_test)
endif()
add_subdirectory(POSIXapi_test)
add_subdirectory(syscall_collector)

