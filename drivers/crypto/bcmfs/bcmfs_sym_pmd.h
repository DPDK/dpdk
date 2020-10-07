/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BCMFS_SYM_PMD_H_
#define _BCMFS_SYM_PMD_H_

#include <rte_cryptodev.h>

#include "bcmfs_device.h"

#define CRYPTODEV_NAME_BCMFS_SYM_PMD	crypto_bcmfs

#define BCMFS_CRYPTO_MAX_HW_DESCS_PER_REQ	16

extern uint8_t cryptodev_bcmfs_driver_id;

/** private data structure for a BCMFS device.
 *  This BCMFS device is a device offering only symmetric crypto service,
 *  there can be one of these on each bcmfs_pci_device (VF).
 */
struct bcmfs_sym_dev_private {
	/* The bcmfs device hosting the service */
	struct bcmfs_device *fsdev;
	/* Device instance for this rte_cryptodev */
	uint8_t sym_dev_id;
	/* BCMFS device symmetric crypto capabilities */
	const struct rte_cryptodev_capabilities *fsdev_capabilities;
};

int
bcmfs_sym_dev_create(struct bcmfs_device *fdev);

int
bcmfs_sym_dev_destroy(struct bcmfs_device *fdev);

#endif /* _BCMFS_SYM_PMD_H_ */
