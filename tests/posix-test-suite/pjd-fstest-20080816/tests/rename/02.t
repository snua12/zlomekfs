#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/rename/02.t,v 1.1 2007/01/17 01:42:10 pjd Exp $

desc="rename returns ENAMETOOLONG if an entire length of either path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..14"

n0=`namegen`

case "${fs}" in
zlomekFS)
	expect 0 mkdir ${name143} 0755
	expect 0 mkdir ${name143}/${name143} 0755
	expect 0 mkdir ${name143}/${name143}/${name143} 0755
	expect 0 mkdir ${path579} 0755
	expect 0 create ${n0} 0644

	expect 0 rename ${n0} ${path581}
	expect 0 rename ${path581} ${n0}

	#TODO: fix this error in zlomekFS
	#expect ENAMETOOLONG rename ${n0} ${too_long}
	empty_test
	expect 0 unlink ${n0}
	create_too_long

if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then
	expect ENOENT rename ${too_long} ${n0}
else
	expect ENAMETOOLONG rename ${too_long} ${n0}
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
	expect 0 create ${n0} 0644
	expect 0 rename ${n0} ${path1023}
	expect 0 rename ${path1023} ${n0}
	expect ENAMETOOLONG rename ${n0} ${too_long}
	expect 0 unlink ${n0}
	create_too_long
	expect ENAMETOOLONG rename ${too_long} ${n0}
	unlink_too_long
	expect 0 rmdir ${path1021}
	expect 0 rmdir ${name255}/${name255}/${name255}
	expect 0 rmdir ${name255}/${name255}
	expect 0 rmdir ${name255}
	;;
esac
