# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

cmake_minimum_required(VERSION 2.8.4)

project(ZlomekFS)
if (CYGWIN)
	set(CMAKE_LEGACY_CYGWIN_WIN32 1)
endif()

# enable pkg config support
INCLUDE(${CMAKE_ROOT}/Modules/FindPkgConfig.cmake)

#add our modules dir
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)

function(print_help)
	message("")
	message("Usage: cmake <path_to_source> <options>")
	message("    -DCFLAGS=   - specify additional CFLAGS for build")
	message("")
endfunction()

option (ENABLE_VERSIONS "Enable ZFS version support" OFF) 
if (ENABLE_VERSIONS)
  set(VERSIONS_LIBRARIES version)
endif (ENABLE_VERSIONS)

option(ENABLE_FS_INTERFACE "Enable fs interface (like fuse or dokan)" ON)
option(ENABLE_HTTP_INTERFACE "Enable http fs interface (like GoFlex Satellite)" OFF)
option (ENABLE_CHECKING "Enable checking in ZFS daemon" OFF)
option (ENABLE_DEBUG_PRINT "Enable printing data to log" OFF)
option (ENABLE_MUTEX_LOCKED "Enable checking in mutex operations" OFF)
option (ENABLE_DBUS "Enable dbus control in ZFS daemon" OFF)
if (ENABLE_DBUS)
	PKG_CHECK_MODULES(DBUS dbus-1)
	if (DBUS_FOUND)
		link_directories(${DBUS_LIBRARY_DIRS})
		include_directories(${DBUS__INCLUDE_DIRS})
	else()
		message(FATAL_ERROR "Dbus not found")
	endif()
endif()

option (ENABLE_CLI_CONSOLE "Enable zlomekFS command line console interface" OFF)
option (ENABLE_CLI_TELNET "Enable zlomekFS command line telnet interface" OFF)
if (ENABLE_CLI_CONSOLE OR ENABLE_CLI_TELNET)
	set(ENABLE_CLI ON)
	message(STATUS "ENABLING CLI Debug interface")
else()
	set(ENABLE_CLI OFF)
endif()

option (ENABLE_PROFILE "Enable The GNU Profiler" OFF)
if (ENABLE_PROFILE)
	message (STATUS "Enabling profiling with gprof")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
	set(CMAKE_SHAREDBoost_USE_MULTITHREADED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")
	set(CMAKE_EXE_FLAGS "${CMAKE_EXE_FLAGS} -pg")
endif (ENABLE_PROFILE)

option(ENABLE_UNIT_TESTS "Enable unit tests build" OFF)

##############################################################
############## Build configurations
##############################################################

### Input variables CFLAGS, CONFIGURATION

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -ggdb -Wall -Wextra -Wformat-security -Winit-self -Wfloat-equal -Wshadow -Wbad-function-cast -Wcast-align -Wmissing-declarations -DDEBUG ${CFLAGS}" CACHE string "Debug cflags" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-O2 -g0 ${CFLAGS}" CACHE string "Release CFLAGS" FORCE)

set(BUILDTYPE STATIC)

# Release build is default
if (NOT CMAKE_BUILD_TYPE)
                set(CMAKE_BUILD_TYPE  Release)
endif()

# supress some error reporting for android
if (ANDROID)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-cast-align -Wno-unused-variable")
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

if (ENABLE_UNIT_TESTS)
	enable_test(unit)
	enable_test(gtest)
endif(ENABLE_UNIT_TESTS)

# first check for "all" keyword
list(FIND TESTS all ALLTESTS)

##############################################################
############## Doc part
##############################################################

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
	find_program(GRAPHVIZ_DOT dot)
	if (GRAPHVIZ_DOT)
		set(HAVE_GRAPHVIZ_DOT "YES")
	else()
		set(HAVE_GRAPHVIZ_DOT "NO")
	endif() 

	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
	add_custom_target(doc
	${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "Generating API documentation with Doxygen" VERBATIM
)
	if (ENABLE_CLI)
		add_dependencies(doc generate_zfsd_cli_doc)
	endif()
endif(DOXYGEN_FOUND)

##############################################################
############## Main part
##############################################################

# find libraries


if (ENABLE_FS_INTERFACE)
#libfuse
	set(FUSE_LIBRARY_DIRS "" CACHE PATH "Path to libfuse library dirs.")
	set(FUSE_INCLUDE_DIRS "" CACHE PATH "Path to libfuse include dirs.")
	if (FUSE_LIBRARY_DIRS AND  FUSE_INCLUDE_DIRS)
		set(FUSE_LIBRARIES fuse)
		add_definitions(-D_FILE_OFFSET_BITS=64)
		set(FUSE_FOUND 1)
	endif()

	if (NOT FUSE_FOUND)
		PKG_CHECK_MODULES(FUSE fuse>=2.6)
	endif()

	if (FUSE_FOUND EQUAL 1)
		set(HAVE_FUSE 1)
		message(STATUS "fuse found")
		set(ENABLE_FS_INTERFACE 1)

		# Mac OS X build fix, convert list to string
		string(REPLACE ";" " " FUSE_CFLAGS_OTHER_STR "${FUSE_CFLAGS_OTHER}")
		set(CMAKE_C_FLAGS "${FUSE_CFLAGS_OTHER_STR} ${CMAKE_C_FLAGS}")

		link_directories(${FUSE_LIBRARY_DIRS})
		include_directories(${FUSE_INCLUDE_DIRS})
	else()
		message(STATUS "fuse not found")
#dokan
		PKG_CHECK_MODULES(DOKAN dokan>=0.6)
		if (DOKAN_FOUND EQUAL 1)
			set(HAVE_DOKAN 1)
			message(STATUS "dokan found")
			set(ENABLE_FS_INTERFACE 1)

			link_directories(${DOKAN_LIBRARY_DIRS})
			include_directories(${DOKAN_INCLUDE_DIRS})
		else()
			message(STATUS "dokan not found")
			message(FATAL_ERROR "no fuse interface was found, install fuse or dokan")
		endif()
	endif()
else()
	set(HAVE_FUSE 0)
	set(HAVE_DOKAN 0)
endif()

# fuse libraries
if (HAVE_FUSE)
SET(FS_IFACE_LIBS fuse_iface)
elseif(HAVE_DOKAN)
SET(FS_IFACE_LIBS dokan_iface)
endif()


if (ENABLE_HTTP_INTERFACE)
	PKG_CHECK_MODULES(MHD libmicrohttpd>=0.4.6)
	if (MHD_FOUND EQUAL 1)
		message(STATUS "libmicrohttpd found")

		link_directories(${MHD_LIBRARY_DIRS})
		include_directories(${MHD_INCLUDE_DIRS})
	else()
		message(FATAL_ERROR "libmicrohttpd not found")
	endif()
endif()


#libconfig

set(LCONFIG_LIBRARY_DIRS "" CACHE PATH "Path to libconfig library dirs.")
set(LCONFIG_INCLUDE_DIRS "" CACHE PATH "Path to libconfig include dirs.")
if (LCONFIG_LIBRARY_DIRS AND LCONFIG_INCLUDE_DIRS)
	set(LCONFIG_LIBRARIES libconfig.a)
	set(LCONFIG_FOUND 1)
endif()

if (NOT LCONFIG_FOUND)
	PKG_CHECK_MODULES(LCONFIG libconfig>=1.3)
endif()

if (LCONFIG_FOUND EQUAL 1)
	link_directories(${LCONFIG_LIBRARY_DIRS})
	include_directories(${LCONFIG_INCLUDE_DIRS})
else()
	message(FATAL_ERROR "libconfig not found")
endif()

# openssl
PKG_CHECK_MODULES(OPENSSL openssl)
if(OPENSSL_FOUND EQUAL 1)
	link_directories(${OPENSSL_LIBRARY_DIRS})
	include_directories(${OPENSSL_INCLUDE_DIRS})
else()
#	message(FATAL_ERROR "Openssl not found")
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
check_include_files ("execinfo.h" HAVE_EXECINFO_H)
if (HAVE_EXECINFO_H)
	set(CMAKE_C_FLAGS_DEBUG "-rdynamic ${CMAKE_C_FLAGS_DEBUG}")
endif(HAVE_EXECINFO_H)

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
configure_file(zfsd-bconfig.h.cmake ${CMAKE_BINARY_DIR}/include/zfsd-bconfig.h)
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
${CMAKE_SOURCE_DIR}/syplog/src/
)

link_directories(
${DOKAN_LIBRARY_DIRS}
${LCONFIG_LIBRARY_DIRS}
${FUSE_LIBRARY_DIRS}
${DBUS_LIBRARY_DIRS}
${OPENSSL_LIBRARY_DIR}
)

set(ZFSD_SOURCE_DIR ${CMAKE_SOURCE_DIR}/zfsd)
add_subdirectory(zfsd)
add_subdirectory(syplog/src)


add_custom_target(sample_config
	DEPENDS zfs_gen_sample_cfg
	COMMAND "${CMAKE_SOURCE_DIR}/scripts/gen_single_ncfg.sh" "${CMAKE_BINARY_DIR}/zfsd/configuration/gen_sample_cfg/zfs_gen_sample_cfg" "${CMAKE_SOURCE_DIR}/conf/single/" "${CMAKE_BINARY_DIR}/sample_config/single"
)


if(CYGWIN)
	set(ZFS_MOUNT_POINT "z:")

	if (ENABLE_CLI_CONSOLE)
			set(FORCE_BACKGROUND "")
	else()
			set(FORCE_BACKGROUND "&")
	endif()

	add_custom_target(run_single
	COMMAND "${CMAKE_BINARY_DIR}/zfsd/zfsd/zfsd" -o "config=${CMAKE_BINARY_DIR}/sample_config/single/etc/zfsd/zfsd.conf" "${ZFS_MOUNT_POINT}" "${FORCE_BACKGROUND}" ; echo "ZlomekFS is mounted ${ZFS_MOUNT_POINT}") 

	find_program(DOKANCTL dokanctl)
	if (DOKANCTL)
		add_custom_target(terminate_single
		COMMAND "${DOKANCTL}"  /u "${ZFS_MOUNT_POINT}" || true)
	else()
		message("dokanctl is not avaiable, can not unmount zlomekFS")
	endif()
else()
	set(ZFS_MOUNT_POINT "${CMAKE_BINARY_DIR}/sample_config/single/mnt")

	add_custom_target(run_single
	COMMAND mkdir -p "${ZFS_MOUNT_POINT}" && "${CMAKE_BINARY_DIR}/zfsd/zfsd/zfsd" -o "config=${CMAKE_BINARY_DIR}/sample_config/single/etc/zfsd/zfsd.conf" "${ZFS_MOUNT_POINT}" && echo "ZlomekFS is mounted ${ZFS_MOUNT_POINT}")

	find_program(FUSERMOUNT fusermount)
	if (FUSERMOUNT)
		add_custom_target(terminate_single
		COMMAND "${FUSERMOUNT}" -u "${ZFS_MOUNT_POINT}")
	else() # fuse4x don't have fusermount
		add_custom_target(terminate_single
		COMMAND umount "${ZFS_MOUNT_POINT}")
	endif()
endif()

#fstest support
set(FSTEST_DIR ${CMAKE_SOURCE_DIR}/tests/posix-test-suite/pjd-fstest-20080816/)
add_subdirectory(${FSTEST_DIR})

set(ZFS_TEST_DIR "${ZFS_MOUNT_POINT}/data/testdir")
find_program(PROVE prove)
add_custom_target(run_fstest
	COMMAND mkdir -p "${ZFS_TEST_DIR}" && cd "${ZFS_TEST_DIR}" && export FSTEST="${CMAKE_BINARY_DIR}/tests/posix-test-suite/pjd-fstest-20080816/fstest" && "${PROVE}" -r "${FSTEST_DIR}/tests/")

