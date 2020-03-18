#! /bin/bash
# SPDX-License-Identifier: BSD-3-Clause

. ${DIR}/tun_aesgcm_defs.sh

if [[ -z "${CRYPTO_FLBK_TYPE}" ]]; then
	CRYPTO_FLBK_TYPE="fallback lookaside-none"
fi

SGW_CFG_XPRM_IN="port_id 0 type inline-crypto-offload ${CRYPTO_FLBK_TYPE}"
