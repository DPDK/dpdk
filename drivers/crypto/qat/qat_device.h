/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */
#ifndef _QAT_DEVICE_H_
#define _QAT_DEVICE_H_

#include <rte_cryptodev_pmd.h>
#include <rte_bus_pci.h>
#include "qat_common.h"
#include "qat_logs.h"

extern uint8_t cryptodev_qat_driver_id;

extern int qat_crypto_sym_qp_release(struct rte_cryptodev *dev,
	uint16_t queue_pair_id);

/** private data structure for each QAT device */
struct qat_pmd_private {
	unsigned int max_nb_queue_pairs;
	/**< Max number of queue pairs supported by device */
	unsigned int max_nb_sessions;
	/**< Max number of sessions supported by device */
	enum qat_device_gen qat_dev_gen;
	/**< QAT device generation */
	const struct rte_cryptodev_capabilities *qat_dev_capabilities;
};

int qat_dev_config(struct rte_cryptodev *dev,
		struct rte_cryptodev_config *config);
int qat_dev_start(struct rte_cryptodev *dev);
void qat_dev_stop(struct rte_cryptodev *dev);
int qat_dev_close(struct rte_cryptodev *dev);
void qat_dev_info_get(struct rte_cryptodev *dev,
	struct rte_cryptodev_info *info);

#endif /* _QAT_DEVICE_H_ */
