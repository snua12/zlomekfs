#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/mkdir/03.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="mkdir returns ENAMETOOLONG if an entire path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..11"

case "${fs}" in
zlomekFS)
	expect 0 mkdir ${name143} 0755
	expect 0 mkdir ${name143}/${name143} 0755
	expect 0 mkdir ${name143}/${name143}/${name143} 0755
	expect 0 mkdir ${path579} 0755
	expect 0 mkdir ${path581} 0755
	expect 0 rmdir ${path581}
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
	expect 0 mkdir ${path1023} 0755
	expect 0 rmdir ${path1023}
	expect 0 rmdir ${path1021}
	expect 0 rmdir ${name255}/${name255}/${name255}
	expect 0 rmdir ${name255}/${name255}
	expect 0 rmdir ${name255}
	;;
esac

create_too_long
if [ "${os}" = "cygwin" ]; then
	expect ENOENT mkdir ${too_long} 0755
else
	expect ENAMETOOLONG mkdir ${too_long} 0755
fi
unlink_too_long

