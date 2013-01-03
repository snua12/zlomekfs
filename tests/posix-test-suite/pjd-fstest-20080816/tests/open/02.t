#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/open/02.t,v 1.1 2007/01/17 01:42:10 pjd Exp $

desc="open returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

case "${fs}" in
zlomekFS)
	empty_test
	empty_test
	empty_test
	if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then # zlomekFS via dokan iface  does not support rights
		expect ENOENT open ${name256} O_CREAT 0620
	else
		expect ENAMETOOLONG open ${name256} O_CREAT 0620
	fi
	;;
*)
	expect 0 open ${name255} O_CREAT 0620
	expect 0620 stat ${name255} mode
	expect 0 unlink ${name255}
	expect ENAMETOOLONG open ${name256} O_CREAT 0620
	;;
esac
