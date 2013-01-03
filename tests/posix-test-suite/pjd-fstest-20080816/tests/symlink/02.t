#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/symlink/02.t,v 1.1 2007/01/17 01:42:11 pjd Exp $

desc="symlink returns ENAMETOOLONG if a component of the name2 pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..7"

n0=`namegen`

case "${fs}" in
zlomekFS)
	expect 0 symlink ${name143} ${n0}
	expect 0 unlink ${n0}
	expect 0 symlink ${n0} ${name143}
	expect 0 unlink ${name143}

	if [ "${os}" = "cygwin" ]; then
		expect ENOENT symlink ${n0} ${name256}
	else
		expect ENAMETOOLONG symlink ${n0} ${name256}
	fi

	expect 0 symlink ${name144} ${n0}
	expect 0 unlink ${n0}
	;;
*)
	expect 0 symlink ${name255} ${n0}
	expect 0 unlink ${n0}
	expect 0 symlink ${n0} ${name255}
	expect 0 unlink ${name255}

	expect ENAMETOOLONG symlink ${n0} ${name256}
	expect 0 symlink ${name256} ${n0}
	expect 0 unlink ${n0}
	;;
esac
