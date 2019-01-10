#! /bin/bash

. ${DIR}/trs_aesgcm_defs.sh

CRYPTO_DEV='--vdev="crypto_null0"'
SGW_CFG_XPRM='port_id 0 type inline-crypto-offload'
