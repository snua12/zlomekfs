#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/symlink/07.t,v 1.1 2007/01/17 01:42:11 pjd Exp $

desc="symlink returns ELOOP if too many symbolic links were encountered in translating the name2 path name"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 symlink ${n0} ${n1}
expect 0 symlink ${n1} ${n0}
if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then
	expect ENOTDIR symlink test ${n0}/test
	expect ENOTDIR symlink test ${n1}/test
else
	expect ELOOP symlink test ${n0}/test
	expect ELOOP symlink test ${n1}/test
fi
expect 0 unlink ${n0}
expect 0 unlink ${n1}
