#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/mkfifo/02.t,v 1.1 2007/01/17 01:42:09 pjd Exp $

desc="mkfifo returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

case "${fs}" in
zlomekFS)
	expect 0 mkfifo ${name143} 0644
	expect 0 unlink ${name143}
	expect ENAMETOOLONG mkfifo ${name144} 0644
;;
*)
	expect 0 mkfifo ${name255} 0644
	expect 0 unlink ${name255}
	expect ENAMETOOLONG mkfifo ${name256} 0644
;;
esac
