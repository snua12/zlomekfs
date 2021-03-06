#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chmod/01.t,v 1.1 2007/01/17 01:42:08 pjd Exp $

desc="chmod returns ENOTDIR if a component of the path prefix is not a directory"

dir=`dirname $0`
. ${dir}/../misc.sh

if [ "${fs}" = "zlomekFS" ]; then
	echo "1..1"
	# for zlomekFS test was disabled
	empty_test
	exit 0
fi

echo "1..5"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect 0 create ${n0}/${n1} 0644
expect ENOTDIR chmod ${n0}/${n1}/test 0644
expect 0 unlink ${n0}/${n1}
expect 0 rmdir ${n0}
