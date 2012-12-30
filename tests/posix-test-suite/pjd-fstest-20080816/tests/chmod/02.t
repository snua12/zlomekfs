#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/chmod/02.t,v 1.1 2007/01/17 01:42:08 pjd Exp $

desc="chmod returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

if [ "${fs}" = "zlomekFS" ]; then
	echo "1..1"
	# for zlomekFS test was disabled
	empty_test
	exit 0
fi

echo "1..5"

expect 0 create ${name255} 0644
expect 0 chmod ${name255} 0620
expect 0620 stat ${name255} mode
expect 0 unlink ${name255}
expect ENAMETOOLONG chmod ${name256} 0620
