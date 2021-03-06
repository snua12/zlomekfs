#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/open/03.t,v 1.1 2007/01/17 01:42:10 pjd Exp $

desc="open returns ENAMETOOLONG if an entire path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..12"

case "${fs}" in
zlomekFS)
	expect 0 mkdir ${name143} 0755
	expect 0 mkdir ${name143}/${name143} 0755
	expect 0 mkdir ${name143}/${name143}/${name143} 0755
	expect 0 mkdir ${path579} 0755
	if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then # zlomekFS via dokan iface  does not support rights
		expect 0 open ${path581} O_CREAT 0644
		expect 0644 stat ${path581} mode
	else
		expect 0 open ${path581} O_CREAT 0642
		expect 0642 stat ${path581} mode
	fi
	expect 0 unlink ${path581}
	create_too_long
	if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then # zlomekFS via dokan iface  does not support rights
		expect ENOENT open ${too_long} O_CREAT 0642
	else
		expect ENAMETOOLONG open ${too_long} O_CREAT 0642
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
	expect 0 open ${path1023} O_CREAT 0642
	expect 0642 stat ${path1023} mode
	expect 0 unlink ${path1023}
	create_too_long
	expect ENAMETOOLONG open ${too_long} O_CREAT 0642
	unlink_too_long
	expect 0 rmdir ${path1021}
	expect 0 rmdir ${name255}/${name255}/${name255}
	expect 0 rmdir ${name255}/${name255}
	expect 0 rmdir ${name255}
;;
esac
