/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 ZTE Corporation
 */

#ifndef _ZSDA_CRYPTO_PMD_H_
#define _ZSDA_CRYPTO_PMD_H_

#include "cryptodev_pmd.h"

#include "zsda_qp.h"

/* ZSDA Crypto PMD driver name */
#define CRYPTODEV_NAME_ZSDA_PMD crypto_zsda
#define ZSDA_CIPHER_KEY_MAX_LEN 64

#define ZSDA_OPC_EC_AES_XTS_256		0x0  /**< Encry AES-XTS-256 */
#define ZSDA_OPC_EC_AES_XTS_512		0x01 /**< Encry AES-XTS-512 */
#define ZSDA_OPC_EC_SM4_XTS_256		0x02 /**< Encry SM4-XTS-256 */
#define ZSDA_OPC_DC_AES_XTS_256		0x08 /**< Decry AES-XTS-256 */
#define ZSDA_OPC_DC_AES_XTS_512		0x09 /**< Decry AES-XTS-512 */
#define ZSDA_OPC_DC_SM4_XTS_256		0x0A /**< Decry SM4-XTS-256 */
#define ZSDA_OPC_HASH_SHA1		0x20 /**< Hash-SHA1 */
#define ZSDA_OPC_HASH_SHA2_224		0x21 /**< Hash-SHA2-224 */
#define ZSDA_OPC_HASH_SHA2_256		0x22 /**< Hash-SHA2-256 */
#define ZSDA_OPC_HASH_SHA2_384		0x23 /**< Hash-SHA2-384 */
#define ZSDA_OPC_HASH_SHA2_512		0x24 /**< Hash-SHA2-512 */
#define ZSDA_OPC_HASH_SM3		0x25 /**< Hash-SM3 */


/** private data structure for a ZSDA device.
 * This ZSDA device is a device offering only symmetric crypto service,
 * there can be one of these on each zsda_pci_device (VF).
 */
struct zsda_crypto_dev_private {
	struct zsda_pci_device *zsda_pci_dev;
	/**< The zsda pci device hosting the service */
	struct rte_cryptodev *cryptodev;
	/**< The pointer to this crypto device structure */
	const struct rte_cryptodev_capabilities *zsda_crypto_capabilities;
	/**< ZSDA device crypto capabilities */
	const struct rte_memzone *capa_mz;
	/**< Shared memzone for storing capabilities */
};

int zsda_crypto_dev_create(struct zsda_pci_device *zsda_pci_dev);

void zsda_crypto_dev_destroy(struct zsda_pci_device *zsda_pci_dev);

#endif /* _ZSDA_CRYPTO_PMD_H_ */
