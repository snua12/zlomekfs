#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/mkdir/02.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="mkdir returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"
case "${fs}" in
zlomekFS)
	expect 0 mkdir ${name255} 0755
	expect 0 rmdir ${name255}
	if [ "${os}" = "cygwin" ]; then
		expect ENOENT mkdir ${name256} 0755
	else
		expect ENAMETOOLONG mkdir ${name256} 0755
	fi
	;;
*)
	expect 0 mkdir ${name255} 0755
	expect 0 rmdir ${name255}
	expect ENAMETOOLONG mkdir ${name256} 0755
	;;
esac
