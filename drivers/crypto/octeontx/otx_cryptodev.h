/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _OTX_CRYPTODEV_H_
#define _OTX_CRYPTODEV_H_

/* Cavium OCTEON TX crypto PMD device name */
#define CRYPTODEV_NAME_OCTEONTX_PMD	crypto_octeontx

/* Device ID */
#define PCI_VENDOR_ID_CAVIUM		0x177d
#define CPT_81XX_PCI_VF_DEVICE_ID	0xa041

/*
 * Crypto device driver ID
 */
uint8_t otx_cryptodev_driver_id;

#endif /* _OTX_CRYPTODEV_H_ */
