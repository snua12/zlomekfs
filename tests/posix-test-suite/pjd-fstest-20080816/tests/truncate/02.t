#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/truncate/02.t,v 1.1 2007/01/17 01:42:12 pjd Exp $

desc="truncate returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..5"

case "${fs}" in
zlomekFS)
	expect 0 create ${name255} 0644
	expect 0 truncate ${name255} 123
	expect 123 stat ${name255} size
	expect 0 unlink ${name255}

	if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then
		expect ENOENT truncate ${name256} 123
	else
		expect ENAMETOOLONG truncate ${name256} 123
	fi
	;;
*)
	expect 0 create ${name255} 0644
	expect 0 truncate ${name255} 123
	expect 123 stat ${name255} size
	expect 0 unlink ${name255}
	expect ENAMETOOLONG truncate ${name256} 123
	;;
esac
