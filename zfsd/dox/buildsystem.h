/**
 *  \file buildsystem.h
 * 
 *  \brief Build system notes
 *  \author Ales Snuparek 
 *
 */

/*! \page zfs-build ZlomekFS build system 
 * ZlomekFS build system is based on CMake http://www.cmake.org/.
 * It is posible configure build type (Debug, Release) and 
 * enable or disable funkcionality.
 * \section how_build_zlomekfs How build zlomekFS
 * - enter directory with sources
 * - mkdir build # create build directory
 * - cmake .. -i # configure build
 * - make # build zlomekFS
 *
 * \section zlomekfs_make_targets Make targets
 * - make # build zlomekFS
 * - make clean # clean source code
 * - make doc # generate Doxygen documentation
 * - make sample_config # generate zlomekFS single node config in directory ${BUILD_DIR}/sample_config/single/
 * - make run_single # start zlomekFS with sample config
 * - make terminate_single # stop zlomekFS
 * - make fstest # run tests on zlomekFS http://www.tuxera.com/community/posix-test-suite/
 * - make test # run unit tests on zlomekFS
 *
 * \section zlomekfs_cmake_options ZlomekFS CMake options
 * - CMAKE_BUILD_TYPE Debug|Release
 * - ENABLE_CHECKING OFF|ON # enable or disable dynamic check during runtime (asserts, mutex locks)
 * - ENABLE_CLI OFF|ON # enable or disable control CLI interface
 * - ENABLE_CLI_CONSOLE OFF|ON # enable or disable CLI console
 * - ENABLE_CLI_TELNET OFF|ON # enable or disable CLI over telnet
 * - ENABLE_DBUS OFF|ON # enable or disable control over DBUS
 * - ENABLE_FS_INTERFACE OFF|ON # enable or disable fuse interface
 * - ENABLE_HTTP_INTERFACE OFF|ON # enable or disable fuse interface over HTTP pages
 * - ENABLE_PROFILE OFF|ON # enable or disable build with gprofile
 * - ENABLE_UNIT_TESTS OFF|ON # enable or disable unit test support
 * - ENABLE_VERSIONS OFF|ON # enable or disable zlomekFS versioning support
 *
 * \section zlomekfs_macro_for_cond_build ZlomekFS macros for conditional build
 * - ENABLE_VERSIONS
 *  -ENABLE_CHECKING
 * - ENABLE_DBUS
 * - ENABLE_CLI
 * - ENABLE_CLI_CONSOLE
 * - ENABLE_CLI_TELNET
 * - ENABLE_FS_INTERFACE
 * - ENABLE_HTTP_INTERFACE
 */

