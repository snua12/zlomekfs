#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/link/06.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="link returns EACCES when a component of either path prefix denies search permission"

dir=`dirname $0`
. ${dir}/../misc.sh

if [ "${fs}" = "zlomekFS" ]; then
	echo "1..1"
	# for zlomekFS test was disabled
	empty_test
	exit 0
fi

echo "1..18"

n0=`namegen`
n1=`namegen`
n2=`namegen`
n3=`namegen`
n4=`namegen`

expect 0 mkdir ${n0} 0755
cdir=`pwd`
cd ${n0}

expect 0 mkdir ${n1} 0755
expect 0 chown ${n1} 65534 65534
expect 0 mkdir ${n2} 0755
expect 0 chown ${n2} 65534 65534
expect 0 -u 65534 -g 65534 create ${n1}/${n3} 0644

expect 0 -u 65534 -g 65534 link ${n1}/${n3} ${n2}/${n4}
expect 0 -u 65534 -g 65534 unlink ${n2}/${n4}

expect 0 chmod ${n1} 0644
expect EACCES -u 65534 -g 65534 link ${n1}/${n3} ${n1}/${n4}
expect EACCES -u 65534 -g 65534 link ${n1}/${n3} ${n2}/${n4}

expect 0 chmod ${n1} 0755
expect 0 chmod ${n2} 0644
expect EACCES -u 65534 -g 65534 link ${n1}/${n3} ${n2}/${n4}

expect 0 unlink ${n1}/${n3}
expect 0 rmdir ${n1}
expect 0 rmdir ${n2}

cd ${cdir}
expect 0 rmdir ${n0}
