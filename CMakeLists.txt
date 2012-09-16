# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

cmake_minimum_required(VERSION 2.8)

project(ZlomekFS)
if (CYGWIN)
	set(CMAKE_LEGACY_CYGWIN_WIN32 1)
endif()

#add our modules dir
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)

function(print_help)
	message("")
	message("Usage: cmake <path_to_source> <options>")
	message("Available options:")
	message("    -DCONFIGURATION=   - configuration type [Debug,Release], default: Debug")
	message("        .. specifies default LIBS")
	message("        .. also sets default CFLAGS")
  message("    -DVERSION= - enable or disable zfs version, choices [0,1], default 1")
  message("    -DDBUS= - enable or disable dbus interface, choices [0,1], defaulr 1")
	message("    -DCFLAGS=   - specify additional CFLAGS for build")
	message("    -DPREFIX=   - installation prefix, default: $build/installed")
	message("    -DTESTS=   - specify list of test groups to enable [all,gtest,unit], default: none")
	message("")
endfunction()

option (ENABLE_VERSIONS "Enable ZFS version support" ON) 
if (ENABLE_VERSIONS)
  set(VERSIONS_LIBRARIES version)
endif (ENABLE_VERSIONS)

option(ENABLE_FS_INTERFACE "Enable fs interface (like fuse or dokan)" ON)
option (ENABLE_CHECKING "Enable checking in ZFS daemon" OFF)
option (ENABLE_MUTEX_LOCKED "Enable checking in mutex operations" OFF)
option (ENABLE_DBUS "Enable dbus control in ZFS daemon" OFF)
option (ENABLE_PROFILE "Enable The GNU Profiler" OFF)
if (ENABLE_PROFILE)
	add_definitions(-pg)
	set(CMAKE_EXE_LINKER_FLAGS "-pg ${CMAKE_EXE_LINKER_FLAGS}")
endif(ENABLE_PROFILE)

##############################################################
############## Build configurations
##############################################################

### Input variables CFLAGS, CONFIGURATION

set(CMAKE_CONFIGURATION_TYPES "Debug Release" CACHE string "Configuration types" FORCE)

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -ggdb -Wall -Wextra -Wformat-security -Winit-self -Wfloat-equal -Wshadow -Wbad-function-cast -Wcast-align -Wmissing-declarations -DDEBUG ${CFLAGS}" CACHE string "Debug cflags" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-O2 -g0 ${CFLAGS}" CACHE string "Release CFLAGS" FORCE)

if(NOT CONFIGURATION)
        if(NOT CMAKE_BUILD_TYPE)
                set(CMAKE_BUILD_TYPE Debug)
                set(BUILDTYPE STATIC)
        endif()     
elseif(CONFIGURATION STREQUAL Debug)
        set(CMAKE_BUILD_TYPE ${CONFIGURATION})
        set(BUILDTYPE STATIC)
elseif(CONFIGURATION STREQUAL Release)
        set(CMAKE_BUILD_TYPE ${CONFIGURATION})
        set(BUILDTYPE STATIC)
endif()  

# make this persistent
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE string "Build configuration" FORCE)

##############################################################
############## Summary
##############################################################
message("Configuration: ${CMAKE_BUILD_TYPE}")


##############################################################
############## Test part
##############################################################

macro(enable_test name)
	list(APPEND TESTS_ENABLED ${name})
	message("  Enabling ${name} test group")
endmacro()

macro(test_enabled name result)
	list(FIND TESTS_ENABLED ${name} ${result})
endmacro()

# Input variables: TESTS=groups [comma delimited]

ENABLE_TESTING()
string(REPLACE "," ";" TESTS "${TESTS}") #convert to semicolon delimiting

# list of enabled tests
set(TESTS_ENABLED "")

# first check for "all" keyword
list(FIND TESTS all ALLTESTS)
if(NOT ALLTESTS EQUAL -1)
	enable_test(unit)
	enable_test(gtest)
else()
	foreach(i ${TESTS})
		if(i STREQUAL utils OR i STREQUAL utils)  # group utils
			enable_test(${i})
		else()
			message("Unknown test group: ${i}")
			print_help()
			message(FATAL_ERROR "Cannot proceed!")
		endif()
	endforeach()
endif()


##############################################################
############## Doc part
##############################################################

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(doc
  ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Generating API documentation with Doxygen" VERBATIM
  )
endif(DOXYGEN_FOUND)

##############################################################
############## Main part
##############################################################

# find libraries

# enable pkg config support
INCLUDE(${CMAKE_ROOT}/Modules/FindPkgConfig.cmake)

if (ENABLE_FS_INTERFACE)
#libfuse
	PKG_CHECK_MODULES(FUSE fuse>=2.6)
	if (FUSE_FOUND EQUAL 1)
		set(HAVE_FUSE 1)
		message(STATUS "fuse found")
		set(ENABLE_FS_INTERFACE 1)
		# Mac OS X build fix, convert list to string
		string(REPLACE ";" " " FUSE_CFLAGS_OTHER_STR "${FUSE_CFLAGS_OTHER}")
		set(CMAKE_C_FLAGS "${FUSE_CFLAGS_OTHER_STR} ${CMAKE_C_FLAGS}")
	else()
		message(STATUS "fuse not found")
#dokan
		PKG_CHECK_MODULES(DOKAN dokan>=0.6)
		if (DOKAN_FOUND EQUAL 1)
			set(HAVE_DOKAN 1)
			message(STATUS "dokan found")
			set(ENABLE_FS_INTERFACE 1)
		else()
			message(STATUS "dokan not found")
			message(FATAL_ERROR "no fuse interface was found, install fuse or dokan")
		endif()
	endif()
else()
	set(HAVE_FUSE 0)
	set(HAVE_DOKAN 0)
endif()


#libconfig
PKG_CHECK_MODULES(LCONFIG libconfig>=1.3)
if (NOT LCONFIG_FOUND EQUAL 1)
	message(FATAL_ERROR "libconfig not found")
endif()

# openssl
include(FindOpenSSL)
if(NOT OPENSSL_FOUND)
	message(FATAL_ERROR "Openssl not found")
endif()

#Protobuf
#include(${CMAKE_SOURCE_DIR}/cmake/modules/FindProtobuf-c.cmake)
#find_package(Protobuf-c REQUIRED)

# google test -- our pthread-patched version
test_enabled(gtest result)
if(NOT result EQUAL -1)
	include(FindGTest)
	if(NOT GTEST_FOUND)
		message(FATAL_ERROR "gtest not found")
	endif()
	SET(ZFS_GTEST_LIBRARIES ${GTEST_BOTH_LIBRARIES} pthread)
endif()

#check headers
include(CheckIncludeFiles)
## usage: CHECK_INCLUDE_FILES (<header> <RESULT_VARIABLE> )
check_include_files ("arpa/inet.h" HAVE_ARPA_INET_H)
check_include_files ("assert.h" HAVE_ASSERT_H)
check_include_files ("ctype.h" HAVE_CTYPE_H)
check_include_files ("dirent.h" HAVE_DIRENT_H)
check_include_files ("errno.h" HAVE_ERRNO_H)
check_include_files ("fcntl.h" HAVE_FCNTL_H)
check_include_files ("getopt.h" HAVE_GETOPT_H)
check_include_files ("grp.h" HAVE_GRP_H)
check_include_files ("libgen.h" HAVE_LIBGEN_H)
check_include_files ("limits.h" HAVE_LIMITS_H)
check_include_files ("netdb.h" HAVE_NETDB_H)
check_include_files ("netinet/in.h" HAVE_NETINET_IN_H)
check_include_files ("pthread.h" HAVE_PTHREAD_H)
check_include_files ("pwd.h" HAVE_PWD_H)
check_include_files ("semaphore.h" HAVE_SEMAPHORE_H)
check_include_files ("signal.h" HAVE_SIGNAL_H)
check_include_files ("stdarg.h" HAVE_STDARGS_H)
check_include_files ("stdlib.h" HAVE_STDLIB_H)
check_include_files ("string.h" HAVE_STRING_H)
check_include_files ("sys/mman.h" HAVE_SYS_MMAN_H)
check_include_files ("sys/poll.h" HAVE_SYS_POLL_H)
check_include_files ("sys/socket.h" HAVE_SYS_SOCKET_H)
check_include_files ("sys/stat.h" HAVE_SYS_STAT_H)
check_include_files ("sys/time.h" HAVE_SYS_TIME_H)
check_include_files ("sys/types.h" HAVE_SYS_TIPES_H)
check_include_files ("sys/un.h" HAVE_SYS_UN_H)
check_include_files ("sys/wait.h" HAVE_SYS_WAIT_H)
check_include_files ("syslog.h" HAVE_SYSLOG_H)
check_include_files ("time.h" HAVE_TIME_H)
check_include_files ("errno.h" HAVE_ERRNO_H)
check_include_files ("inttypes.h" HAVE_INTTYPES_H)
check_include_files ("stddef.h" HAVE_STDDEF_H)
check_include_files ("stdio.h" HAVE_STDIO_H)
check_include_files ("sys/types.h" HAV_SYS_TYPES_H)
check_include_files ("execinfo.h" HAVE_EXEC_H)
check_include_files ("ucontext.h" HAVE_UCONTEXT_H)
check_include_files ("unistd.h" HAVE_UNISTD_H)
check_include_files ("utime.h" HAVE_UTIME_H)
check_include_files ("valgrind/memcheck.h" HAVE_VALGRIND_MEMCHECK_H)
check_include_files ("wchar.h" HAVE_WCHAR_H)
check_include_files ("winbase.h" HAVE_WINBASE_H)
check_include_files ("windows.h" HAVE_WINDOWS_H)
#CHECK_INCLUDE_FILES ("sys/param.h;sys/mount.h" HAVE_SYS_MOUNT_H)
#CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)


#check functions
include(CheckFunctionExists)
check_function_exists(fopencookie HAVE_FOPENCOOKIE)
check_function_exists(mkstemp HAVE_MKSTEMP)
set(CMAKE_REQUIRED_LIBRARIES "pthread")
check_function_exists(pthread_barrier_wait HAVE_PTHREAD_BARRIER_WAIT)
check_function_exists(pthread_yield HAVE_PTHREAD_yield)
check_function_exists(mlockall HAVE_MLOCKALL)
check_function_exists(fdopendir HAVE_FDOPENDIR)

# configure headers
configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/include/config.h)
#configure_file(confdefs.h.cmake ${CMAKE_BINARY_DIR}/include/confdefs.h)

# global include directories
include_directories(
# system includes
/usr/include
# cmake generated configuration headers
${CMAKE_BINARY_DIR}/include
# fuse headers
${FUSE_INCLUDE_DIRS}
# dbus dir
${DBUS_INCLUDE_DIRS}
# openssl header
${OPENSSL_INCLUDE_DIR}
#syplog header
${CMAKE_SOURCE_DIR}/regression-testing/syplog/src/
)

link_directories(
${FUSE_LIBRARY_DIRS}
${DBUS_LIBRARY_DIRS}
${OPENSSL_LIBRARY_DIR}
)

set(ZFSD_SOURCE_DIR ${CMAKE_SOURCE_DIR}/zfsd)
add_subdirectory(zfsd)
add_subdirectory(regression-testing/syplog/src)

