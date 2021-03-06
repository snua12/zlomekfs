#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/mkdir/07.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="mkdir returns ELOOP if too many symbolic links were encountered in translating the pathname"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 symlink ${n0} ${n1}
expect 0 symlink ${n1} ${n0}

if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then
	expect ENOTDIR mkdir ${n0}/test 0755
	expect ENOTDIR mkdir ${n1}/test 0755
else
	expect ELOOP mkdir ${n0}/test 0755
	expect ELOOP mkdir ${n1}/test 0755
fi
expect 0 unlink ${n0}
expect 0 unlink ${n1}
