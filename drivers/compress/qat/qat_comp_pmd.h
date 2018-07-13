/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2018 Intel Corporation
 */

#ifndef _QAT_COMP_PMD_H_
#define _QAT_COMP_PMD_H_

#ifdef RTE_LIBRTE_COMPRESSDEV

#include <rte_compressdev.h>
#include <rte_compressdev_pmd.h>

#include "qat_device.h"

/** private data structure for a QAT compression device.
 * This QAT device is a device offering only a compression service,
 * there can be one of these on each qat_pci_device (VF).
 */
struct qat_comp_dev_private {
	struct qat_pci_device *qat_dev;
	/**< The qat pci device hosting the service */
	struct rte_compressdev *compressdev;
	/**< The pointer to this compression device structure */
	const struct rte_compressdev_capabilities *qat_dev_capabilities;
	/* QAT device compression capabilities */
	const struct rte_memzone *interm_buff_mz;
	/**< The device's memory for intermediate buffers */
	struct rte_mempool *xformpool;
	/**< The device's pool for qat_comp_xforms */

};

void
qat_comp_stats_reset(struct rte_compressdev *dev);

void
qat_comp_stats_get(struct rte_compressdev *dev,
		struct rte_compressdev_stats *stats);
int
qat_comp_qp_release(struct rte_compressdev *dev, uint16_t queue_pair_id);

int
qat_comp_qp_setup(struct rte_compressdev *dev, uint16_t qp_id,
		  uint32_t max_inflight_ops, int socket_id);

int
qat_comp_dev_config(struct rte_compressdev *dev,
		struct rte_compressdev_config *config);

int
qat_comp_dev_close(struct rte_compressdev *dev);

void
qat_comp_dev_info_get(struct rte_compressdev *dev,
			struct rte_compressdev_info *info);

#endif
#endif /* _QAT_COMP_PMD_H_ */
