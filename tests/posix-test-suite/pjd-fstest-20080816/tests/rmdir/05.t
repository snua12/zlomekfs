#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/rmdir/05.t,v 1.1 2007/01/17 01:42:11 pjd Exp $

desc="rmdir returns ELOOP if too many symbolic links were encountered in translating the pathname"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 symlink ${n0} ${n1}
expect 0 symlink ${n1} ${n0}
if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then
	expect ENOTDIR rmdir ${n0}/test
	expect ENOTDIR rmdir ${n1}/test
else
	expect ELOOP rmdir ${n0}/test
	expect ELOOP rmdir ${n1}/test
fi
expect 0 unlink ${n0}
expect 0 unlink ${n1}
