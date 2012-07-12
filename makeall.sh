#!/bin/sh

build_dir="build"
cmake_opts="-DCONFIGURATION=Debug -DTESTS=all -DCMAKE_INSTALL_PREFIX=/usr"

die()
{
	echo "${1}" 1>&2
	exit 1
}

! [ -d "${build_dir}" ]  && mkdir "${build_dir}"
! [ -d "${build_dir}" ] && die "Failed to create build directory \"${build_dir}\"" 1>&2
! cd "${build_dir}" && die "Failed to switch to build directory \"${build_dir}\"" 1>&2
! cmake .. ${cmake_opts} && die "Failed to configure ZlomekFS"
! make && die "Failed to build ZlomekFS"
