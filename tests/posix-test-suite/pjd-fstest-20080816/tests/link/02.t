#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/link/02.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="link returns ENAMETOOLONG if a component of either pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then
	echo "1..1"
	# for zlomekFS test was disabled
	empty_test
	exit 0
fi

echo "1..10"

n0=`namegen`

case "${fs}" in
zlomekFS)
	expect 0 create ${name143} 0644
	expect 0 link ${name143} ${n0}
	expect 0 unlink ${name143}

	#TODO FIX
	#expect 0 link ${n0} ${name143}
	empty_test

	expect 0 unlink ${n0}

	#TODO FIX
	#expect 0 unlink ${name143}
	empty_test

	expect 0 create ${n0} 0644
	expect ENAMETOOLONG link ${n0} ${name256}
	expect 0 unlink ${n0}

	#TODO FIX
	#expect ENAMETOOLONG link ${namei144} ${n0}
	empty_test
;;
*)
	expect 0 create ${name255} 0644
	expect 0 link ${name255} ${n0}
	expect 0 unlink ${name255}
	expect 0 link ${n0} ${name255}
	expect 0 unlink ${n0}
	expect 0 unlink ${name255}

	expect 0 create ${n0} 0644
	expect ENAMETOOLONG link ${n0} ${name256}
	expect 0 unlink ${n0}
	expect ENAMETOOLONG link ${name256} ${n0}
	;;
esac
