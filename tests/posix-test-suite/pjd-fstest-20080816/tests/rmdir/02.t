#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/rmdir/02.t,v 1.1 2007/01/17 01:42:11 pjd Exp $

desc="rmdir returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

case "${fs}" in
zlomekFS)
	expect 0 mkdir ${name255} 0755
	expect 0 rmdir ${name255}
	expect ENOENT rmdir ${name255}
	if [ "${os}" = "cygwin" ]; then
		expect ENOENT rmdir ${name256}
	else
		expect ENAMETOOLONG rmdir ${name256}
	fi
	;;
*)
	expect 0 mkdir ${name255} 0755
	expect 0 rmdir ${name255}
	expect ENOENT rmdir ${name255}
	expect ENAMETOOLONG rmdir ${name256}
	;;
esac
