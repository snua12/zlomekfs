#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/open/17.t,v 1.1 2007/01/17 01:42:10 pjd Exp $

desc="open returns ENXIO when O_NONBLOCK is set, the named file is a fifo, O_WRONLY is set, and no process has the file open for reading"

dir=`dirname $0`
. ${dir}/../misc.sh

if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then
	echo "1..1"
	# for zlomekFS test was disabled
	empty_test
	exit 0
fi

echo "1..3"

n0=`namegen`

expect 0 mkfifo ${n0} 0644
expect ENXIO open ${n0} O_WRONLY,O_NONBLOCK
expect 0 unlink ${n0}
