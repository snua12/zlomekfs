#!/bin/sh

#generatesi sample single node config

GEN_SIMPLE_CFG="${1}"
SRC_DIR="${2}"
DEST_DIR="${3}"

# prepare test directory
mkdir -p "${DEST_DIR}"

#copy sample config
cp -r "${SRC_DIR}/"* "${DEST_DIR}/"

ZFS_LOCAL_CFG="${DEST_DIR}/etc/zfsd/zfsd.conf"
export ZFS_INSTALL_PREFIX="${DEST_DIR}"

"${GEN_SIMPLE_CFG}" "${ZFS_LOCAL_CFG}"
