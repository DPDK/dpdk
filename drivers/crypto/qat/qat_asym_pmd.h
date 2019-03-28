/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */


#ifndef _QAT_ASYM_PMD_H_
#define _QAT_ASYM_PMD_H_

#include <rte_cryptodev.h>
#include "qat_device.h"

/** Intel(R) QAT Asymmetric Crypto PMD driver name */
#define CRYPTODEV_NAME_QAT_ASYM_PMD	crypto_qat_asym


extern uint8_t cryptodev_qat_asym_driver_id;

/** private data structure for a QAT device.
 * This QAT device is a device offering only asymmetric crypto service,
 * there can be one of these on each qat_pci_device (VF).
 */
struct qat_asym_dev_private {
	struct qat_pci_device *qat_dev;
	/**< The qat pci device hosting the service */
	uint8_t asym_dev_id;
	/**< Device instance for this rte_cryptodev */
	const struct rte_cryptodev_capabilities *qat_dev_capabilities;
	/* QAT device asymmetric crypto capabilities */
};

uint16_t
qat_asym_pmd_enqueue_op_burst(void *qp, struct rte_crypto_op **ops,
			      uint16_t nb_ops);

uint16_t
qat_asym_pmd_dequeue_op_burst(void *qp, struct rte_crypto_op **ops,
			      uint16_t nb_ops);

int qat_asym_session_configure(struct rte_cryptodev *dev,
		struct rte_crypto_asym_xform *xform,
		struct rte_cryptodev_asym_session *sess,
		struct rte_mempool *mempool);

int
qat_asym_dev_create(struct qat_pci_device *qat_pci_dev);

int
qat_asym_dev_destroy(struct qat_pci_device *qat_pci_dev);

#endif /* _QAT_ASYM_PMD_H_ */
