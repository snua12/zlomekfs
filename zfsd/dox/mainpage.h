/** @file mainpage.h
* @brief Description of zlomekFS project
*
*/ 
/** 
*  @mainpage zlomekFS Documentation
*
* @authors Josef Zlomek (initial implementation in 2005)
* @authors Miroslav Trmac (FUSE integration in 2007)
* @authors Jiri Zouhar (logging, d-bus, regression testing in 2008)
* @authors Rastislav Wartiak (versioning support in 2010)
* @authors Ales Snuparek (configuration rewrite, build system rewrite,portability to Android and Windows in 2013)
* @section intro Introduction
*
* Project Home Page: http://nenya.ms.mff.cuni.cz/~ceres/prj/zlomekFS
*
* zlomekFS is a distributed file system that supports disconnected 
* operation using local cache. 
*
* During synchronization of local changes
* it offers easy-to-use conflict resolution mechanism.
*
* Further improved it became a file system with no specific kernel code.
* It has therefore a good potential in future public use.
*
* \section Documentation
* \subsection for_users For users
* \li Configuration: \ref configuration
* \li CLI: \ref zfs-cli
*
* \subsection for_developers For developers
* \li Design overview: \ref zfs-design
* \li Implementation overview: \ref zfs-implementation
* \li Build options and targets overview: \ref zfs-build
* \li Coding conventions: \ref coding-style
* \li Configuration: \ref zfs-configuration
* \li Filesystem Interface: \ref fs-iface
*/
