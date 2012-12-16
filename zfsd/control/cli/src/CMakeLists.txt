# Copyright (C) 2008, 2012 Ales Snuparek
#
# This file is part of ZFS build system.

set(CLI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/cli-2.7")
set(CLI_CPPCLIC_XSL "${CLI_SOURCE_DIR}/cpp/xsl/cppclic.xsl")
set(CLI_DIR "${CLI_SOURCE_DIR}/schema")

set(ZFSD_CLI_XML "${CMAKE_CURRENT_SOURCE_DIR}/zfsd_cli.xml")
set(ZFSD_CLI_INCLUDE "${CMAKE_BINARY_DIR}/include")
set(ZFSD_CLI_H "${ZFSD_CLI_INCLUDE}/zfsd_cli.h")

find_program(XSLTPROC xsltproc)
if(XSLTPROC)
	message(STATUS "xsltproc found: ${XSLTPROC}" )
else()
	message(FATAL_ERROR "xsltproc not found")
endif()

find_program(XMLLINT xmllint)
if(XMLLINT)
	message(STATUS "xmllint found: ${XMLLINT}" )
else()
	message(FATAL_ERROR "xmllint not found")
endif()

ADD_CUSTOM_COMMAND(
	OUTPUT  ${ZFSD_CLI_H}
	DEPENDS ${ZFSD_CLI_XML}
	COMMAND 
		"${XMLLINT}" --schema "${CLI_DIR}/cli.xsd" --noout "${ZFSD_CLI_XML}" 
		&& "${XMLLINT}" --relaxng "${CLI_DIR}/cli.rng" --noout "${ZFSD_CLI_XML}" 
		&& "${XSLTPROC}" -o "${ZFSD_CLI_H}" --stringparam STR_CliClassName 'ZfsdCli' "${CLI_CPPCLIC_XSL}" "${ZFSD_CLI_XML}" 
)

set(CLI_CFLAGS_OTHER_STR "-I${CMAKE_CURRENT_SOURCE_DIR} -I${ZFSD_CLI_INCLUDE} -I${CLI_SOURCE_DIR}/cpp/include -I${CLI_SOURCE_DIR}/cpp/include/cli")
set(CLI_CFLAGS_OTHER_STR "-I${CMAKE_CURRENT_SOURCE_DIR}/../inc/ ${CLI_CFLAGS_OTHER_STR}")
set(CMAKE_CXX_FLAGS "${CLI_CFLAGS_OTHER_STR} ${CMAKE_CXX_FLAGS}")

add_library(zfsd_cli ${BUILDTYPE} control_zfsd_cli.cpp "${ZFSD_CLI_H}")
target_link_libraries(zfsd_cli log pthread)

install(
TARGETS zfsd_cli
DESTINATION ${ZFS_INSTALL_DIR}/lib
PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

