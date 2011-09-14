#!/bin/sh
build_dir="build"
cmake_opts="-DCONFIGURATION=Debug -DLIBS=static -DTESTS=all -DCMAKE_INSTALL_PREFIX=/usr"
mkdir "${build_dir}" \
&& cd "${build_dir}" \
&& cmake .. ${cmake_opts} \
&& make
