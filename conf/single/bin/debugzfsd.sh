#!/bin/sh

ZFSD="/bin/zfsd"
MP="${1}"
CFG="config=/etc/zfsd/zfsd.conf"
LL=",loglevel=2"

gdb --args "${ZFSD}" -o "${CFG}${LL}" "${MP}"

