#!/bin/sh

#generatesi sample single node config

GEN_SIMPLE_CFG="${1}"
SRC_DIR="${2}"
DEST_DIR="${3}"

# prepare test directory
mkdir -p "${DEST_DIR}"

#copy sample config
cp -r "${SRC_DIR}/"* "${DEST_DIR}/"


mkdir -p "${DEST_DIR}/var/log"
chmod 755 "${DEST_DIR}/var/log"
mkdir -p "${DEST_DIR}/var/zfs/data"
chmod 755 "${DEST_DIR}/var/zfs/data"
mkdir -p "${DEST_DIR}/var/zfs/config"
chmod 755 "${DEST_DIR}/var/zfs/config"

ZFS_DEFAULT_UID=$(id -u)
if [ "${ZFS_DEFAULT_UID}" ]; then
	export ZFS_DEFAULT_UID
fi

ZFS_DEFAULT_GID=$(id -g)
if [ "${ZFS_DEFAULT_GID}" ]; then
	export ZFS_DEFAULT_GID
fi

ZFS_LOCAL_CFG="${DEST_DIR}/etc/zfsd/zfsd.conf"
export ZFS_INSTALL_PREFIX="${DEST_DIR}"


"${GEN_SIMPLE_CFG}" "${ZFS_LOCAL_CFG}"
