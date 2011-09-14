cmake_minimum_required(VERSION 2.6)
if(COMMAND cmake_policy)
   cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)
SET(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS 1)

function(print_help)
	message("")
	message("Usage: cmake <path_to_source> <options>")
	message("Available options:")
	message("    -DCONFIGURATION=   - configuration type [Debug,Release], default: Debug")
	message("        .. specifies default LIBS")
	message("        .. also sets default CFLAGS")
	message("    -DLIBS=   - override libraries type [shared,static]")
	message("    -DCFLAGS=   - specify additional CFLAGS for build")
	message("    -DPREFIX=   - installation prefix, default: $build/installed")
	message("    -DTESTS=   - specify list of test groups to enable [all,utils,eventer,sysconfig,email,gtest,unit], default: none")
	message("               - tests in group "email" are not enabled even by "all", because they can cause a lot of email spam! enable at your own risk!")
	message("")
endfunction()


if(NOT PREFIX)
	set(ZFS_INSTALL_DIR ${CMAKE_SOURCE_DIR}/installed)
else()
	set(ZFS_INSTALL_DIR ${PREFIX})
endif()

set(CMAKE_INSTALL_RPATH ${ZFS_INSTALL_DIR}/lib)

# make this persistent
set(PREFIX ${PREFIX} CACHE string "Installation directory" FORCE)

##############################################################
############## Build configurations
##############################################################

### Input variables CFLAGS, CONFIGURATION

set(CMAKE_CONFIGURATION_TYPES "Debug Release" CACHE string "Configuration types" FORCE)

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -ggdb -Wall -Wextra -Wformat-security -Winit-self -Wfloat-equal -Wshadow -Wbad-function-cast -Wcast-align -Wmissing-declarations -DDEBUG ${MASTER_CFLAGS} ${CFLAGS}" CACHE string "Debug cflags" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-O2 -g0 ${MASTER_CFLAGS} ${PLATFORM_CFLAGS} ${CFLAGS}" CACHE string "Release CFLAGS" FORCE)

# make this persistent
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE string "Build configuration" FORCE)

#############################################################
############## Library sharedness
#############################################################

# Input variables: BUILD

if(NOT LIBS)
elseif(LIBS STREQUAL static)
	set(BUILDTYPE STATIC)
elseif(LIBS STREQUAL shared)
	set(BUILDTYPE SHARED)
else()
	message("Error: invalid build type!")
	print_help()
	message(FATAL_ERROR "Cannot proceed!")
endif()

# make this persistent
set(LIBS ${LIBS} CACHE string "Library sharedness" FORCE)

##############################################################
############## Summary
##############################################################

message("Library type: ${BUILDTYPE}")
message("Prefix: ${IPC_INSTALL_DIR}")
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
############## Main part
##############################################################

# find libraries

# enable pkg config support
INCLUDE(${CMAKE_ROOT}/Modules/FindPkgConfig.cmake)

# libfuse
PKG_CHECK_MODULES(FUSE fuse>=2.6)

# dbus
PKG_CHECK_MODULES(DBUS dbus-1>=1.0)

# openssl
include(FindOpenSSL)
if(NOT OPENSSL_FOUND)
	message(FATAL_ERROR "Openssl not found")
endif()

# google test -- our pthread-patched version
test_enabled(gtest result)
if(NOT result EQUAL -1)
	include(FindGTest)
	if(NOT GTEST_FOUND)
		message(FATAL_ERROR "gtest not found")
	endif()
	SET(ZFS_GTEST_LIBRARIES ${GTEST_BOTH_LIBRARIES} pthread)
endif()



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

SET(CMAKE_C_FLAGS "-Wall -Werror -O2 -s ${FUSE_CFLAGS_OTHER} ${FUSE_CFLAGS_OTHER}")


set(ZFSD_SOURCE_DIR ${CMAKE_SOURCE_DIR}/zfsd)

add_subdirectory(zfsd)
add_subdirectory(regression-testing/syplog/src)

#### Install

install(
FILES ${CMAKE_BINARY_DIR}/info
DESTINATION ${IPC_INSTALL_DIR}/
PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

