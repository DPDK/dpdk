/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef _ZSDA_DEVICE_H_
#define _ZSDA_DEVICE_H_

#include "zsda_qp_common.h"
#include "zsda_comp_pmd.h"
#include "zsda_crypto_pmd.h"

#define MAX_QPS_ON_FUNCTION			128
#define ZSDA_DEV_NAME_MAX_LEN		64

struct zsda_device_info {
	const struct rte_memzone *mz;
	/**< mz to store the：  struct zsda_pci_device ,    so it can be
	 * shared across processes
	 */
	struct rte_device comp_rte_dev;
	/**< This represents the compression subset of this pci device.
	 * Register with this rather than with the one in
	 * pci_dev so that its driver can have a compression-specific name
	 */
	struct rte_device crypto_rte_dev;
	/**< This represents the crypto subset of this pci device.
	 * Register with this rather than with the one in
	 * pci_dev so that its driver can have a crypto-specific name
	 */
	struct rte_pci_device *pci_dev;
};

extern struct zsda_device_info zsda_devs[];

struct zsda_qp_hw_data {
	bool used;

	uint8_t tx_ring_num;
	uint8_t rx_ring_num;
	uint16_t tx_msg_size;
	uint16_t rx_msg_size;
};

struct zsda_qp_hw {
	struct zsda_qp_hw_data data[MAX_QPS_ON_FUNCTION];
};

struct zsda_pci_device {
	/* Data used by all services */
	char name[ZSDA_DEV_NAME_MAX_LEN];
	/**< Name of zsda pci device */
	uint8_t zsda_dev_id;
	/**< Id of device instance for this zsda pci device */

	struct rte_pci_device *pci_dev;

	/* Data relating to compression service */
	struct zsda_comp_dev_private *comp_dev;
	/**< link back to compressdev private data */

	/* Data relating to crypto service */
	struct zsda_crypto_dev_private *crypto_dev_priv;
	/**< link back to cryptodev private data */

	struct zsda_qp_hw zsda_hw_qps[ZSDA_MAX_SERVICES];
	uint16_t zsda_qp_hw_num[ZSDA_MAX_SERVICES];
};

#endif /* _ZSDA_DEVICE_H_ */
