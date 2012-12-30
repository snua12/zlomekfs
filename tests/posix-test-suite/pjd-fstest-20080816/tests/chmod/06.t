#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chmod/06.t,v 1.1 2007/01/17 01:42:08 pjd Exp $

desc="chmod returns ELOOP if too many symbolic links were encountered in translating the pathname"

dir=`dirname $0`
. ${dir}/../misc.sh

if [ "${fs}" = "zlomekFS" ]; then
	echo "1..1"
	# for zlomekFS test was disabled
	empty_test
	exit 0
fi

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 symlink ${n0} ${n1}
expect 0 symlink ${n1} ${n0}
expect ELOOP chmod ${n0}/test 0644
expect ELOOP chmod ${n1}/test 0644
expect 0 unlink ${n0}
expect 0 unlink ${n1}
