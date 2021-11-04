/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef _QAT_DEV_GENS_H_
#define _QAT_DEV_GENS_H_

#include "qat_device.h"
#include "qat_qp.h"

#include <stdint.h>

extern const struct qat_qp_hw_data qat_gen1_qps[QAT_MAX_SERVICES]
					 [ADF_MAX_QPS_ON_ANY_SERVICE];

int
qat_dev_get_extra_size_gen1(void);

int
qat_reset_ring_pairs_gen1(
		struct qat_pci_device *qat_pci_dev);
const struct
rte_mem_resource *qat_dev_get_transport_bar_gen1(
		struct rte_pci_device *pci_dev);
int
qat_dev_get_misc_bar_gen1(struct rte_mem_resource **mem_resource,
		struct rte_pci_device *pci_dev);
int
qat_dev_read_config_gen1(struct qat_pci_device *qat_dev);

int
qat_query_svc_gen4(struct qat_pci_device *qat_dev, uint8_t *val);

#endif
