#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/mkfifo/09.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="mkfifo returns EEXIST if the named file exists"

dir=`dirname $0`
. ${dir}/../misc.sh

if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then
	echo "1..1"
	# for zlomekFS test was disabled
	empty_test
	exit 0
fi

echo "1..12"

n0=`namegen`

expect 0 mkdir ${n0} 0755
expect EEXIST mkfifo ${n0} 0644
expect 0 rmdir ${n0}

expect 0 create ${n0} 0644
expect EEXIST mkfifo ${n0} 0644
expect 0 unlink ${n0}

expect 0 symlink test ${n0}
expect EEXIST mkfifo ${n0} 0644
expect 0 unlink ${n0}

expect 0 mkfifo ${n0} 0644
expect EEXIST mkfifo ${n0} 0644
expect 0 unlink ${n0}
