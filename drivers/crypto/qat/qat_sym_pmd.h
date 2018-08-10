/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#ifndef _QAT_SYM_PMD_H_
#define _QAT_SYM_PMD_H_

#ifdef BUILD_QAT_SYM

#include <rte_cryptodev.h>

#include "qat_sym_capabilities.h"
#include "qat_device.h"

/** Intel(R) QAT Symmetric Crypto PMD driver name */
#define CRYPTODEV_NAME_QAT_SYM_PMD	crypto_qat

extern uint8_t cryptodev_qat_driver_id;

/** private data structure for a QAT device.
 * This QAT device is a device offering only symmetric crypto service,
 * there can be one of these on each qat_pci_device (VF),
 * in future there may also be private data structures for other services.
 */
struct qat_sym_dev_private {
	struct qat_pci_device *qat_dev;
	/**< The qat pci device hosting the service */
	uint8_t sym_dev_id;
	/**< Device instance for this rte_cryptodev */
	const struct rte_cryptodev_capabilities *qat_dev_capabilities;
	/* QAT device symmetric crypto capabilities */
};

int
qat_sym_dev_create(struct qat_pci_device *qat_pci_dev);

int
qat_sym_dev_destroy(struct qat_pci_device *qat_pci_dev);

#endif
#endif /* _QAT_SYM_PMD_H_ */
