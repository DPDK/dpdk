/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _OTX_CRYPTODEV_H_
#define _OTX_CRYPTODEV_H_

/* Cavium OCTEON TX crypto PMD device name */
#define CRYPTODEV_NAME_OCTEONTX_PMD	crypto_octeontx

#define CPT_LOGTYPE otx_cpt_logtype

extern int otx_cpt_logtype;

/*
 * Crypto device driver ID
 */
extern uint8_t otx_cryptodev_driver_id;

#endif /* _OTX_CRYPTODEV_H_ */
