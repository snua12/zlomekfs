#!/bin/sh
# $FreeBSD: src/tools/regression/fstest/tests/mkfifo/00.t,v 1.2 2007/01/25 20:50:02 pjd Exp $

desc="mkfifo creates fifo files"

dir=`dirname $0`
. ${dir}/../misc.sh

if [ "${os}:${fs}" = "cygwin:zlomekFS" ]; then
	echo "1..1"
	# for zlomekFS test was disabled
	empty_test
	exit 0
fi

echo "1..36"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n1} 0755
cdir=`pwd`
cd ${n1}

# POSIX: The file permission bits of the new FIFO shall be initialized from
# mode. The file permission bits of the mode argument shall be modified by the
# process' file creation mask.
expect 0 mkfifo ${n0} 0755
expect fifo,0755 lstat ${n0} type,mode
expect 0 unlink ${n0}
expect 0 mkfifo ${n0} 0151
expect fifo,0151 lstat ${n0} type,mode
expect 0 unlink ${n0}
expect 0 -U 077 mkfifo ${n0} 0151
expect fifo,0100 lstat ${n0} type,mode
expect 0 unlink ${n0}
expect 0 -U 070 mkfifo ${n0} 0345
expect fifo,0305 lstat ${n0} type,mode
expect 0 unlink ${n0}
expect 0 -U 0501 mkfifo ${n0} 0345
expect fifo,0244 lstat ${n0} type,mode
expect 0 unlink ${n0}

# POSIX: The FIFO's user ID shall be set to the process' effective user ID.
# The FIFO's group ID shall be set to the group ID of the parent directory or to
# the effective group ID of the process.

case "${fs}" in
zlomekFS)
	empty_test
	empty_test
	empty_test
	empty_test
	empty_test
	empty_test
	empty_test
	empty_test
	empty_test
	empty_test
	empty_test
;;
*)
	expect 0 chown . 65535 65535
	expect 0 -u 65535 -g 65535 mkfifo ${n0} 0755
	expect 65535,65535 lstat ${n0} uid,gid
	expect 0 unlink ${n0}
	expect 0 -u 65535 -g 65534 mkfifo ${n0} 0755
	expect "65535,6553[45]" lstat ${n0} uid,gid
	expect 0 unlink ${n0}
	expect 0 chmod . 0777
	expect 0 -u 65534 -g 65533 mkfifo ${n0} 0755
	expect "65534,6553[35]" lstat ${n0} uid,gid
	expect 0 unlink ${n0}
;;
esac

# POSIX: Upon successful completion, mkfifo() shall mark for update the
# st_atime, st_ctime, and st_mtime fields of the file. Also, the st_ctime and
# st_mtime fields of the directory that contains the new entry shall be marked
# for update.
expect 0 chown . 0 0
time=`${fstest} stat . ctime`
sleep 1
expect 0 mkfifo ${n0} 0755
atime=`${fstest} stat ${n0} atime`
test_check $time -lt $atime
mtime=`${fstest} stat ${n0} mtime`
test_check $time -lt $mtime
ctime=`${fstest} stat ${n0} ctime`
test_check $time -lt $ctime
mtime=`${fstest} stat . mtime`
test_check $time -lt $mtime
ctime=`${fstest} stat . ctime`
test_check $time -lt $ctime
expect 0 unlink ${n0}

cd ${cdir}
expect 0 rmdir ${n1}
