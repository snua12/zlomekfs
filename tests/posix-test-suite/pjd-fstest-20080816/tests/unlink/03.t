#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/unlink/03.t,v 1.1 2007/01/17 01:42:12 pjd Exp $

desc="unlink returns ENAMETOOLONG if an entire path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..12"

case "${fs}" in
zlomekFS)
	expect 0 mkdir ${name143} 0755
	expect 0 mkdir ${name143}/${name143} 0755
	expect 0 mkdir ${name143}/${name143}/${name143} 0755
	expect 0 mkdir ${path579} 0755
	expect 0 create ${path581} 0644
	expect 0 unlink ${path581}
	expect ENOENT unlink ${path581}
	create_too_long
	if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then
		expect ENOENT unlink ${too_long}
	else
		expect ENAMETOOLONG unlink ${too_long}
	fi
	unlink_too_long
	expect 0 rmdir ${path579}
	expect 0 rmdir ${name143}/${name143}/${name143}
	expect 0 rmdir ${name143}/${name143}
	expect 0 rmdir ${name143}
	;;
*)
	expect 0 mkdir ${name255} 0755
	expect 0 mkdir ${name255}/${name255} 0755
	expect 0 mkdir ${name255}/${name255}/${name255} 0755
	expect 0 mkdir ${path1021} 0755
	expect 0 create ${path1023} 0644
	expect 0 unlink ${path1023}
	expect ENOENT unlink ${path1023}
	create_too_long
	expect ENAMETOOLONG unlink ${too_long}
	unlink_too_long
	expect 0 rmdir ${path1021}
	expect 0 rmdir ${name255}/${name255}/${name255}
	expect 0 rmdir ${name255}/${name255}
	expect 0 rmdir ${name255}
	;;
esac
