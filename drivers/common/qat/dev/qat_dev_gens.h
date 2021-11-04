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

const struct qat_qp_hw_data *
qat_qp_get_hw_data_gen1(struct qat_pci_device *dev,
		enum qat_service_type service_type, uint16_t qp_id);

int
qat_qp_rings_per_service_gen1(struct qat_pci_device *qat_dev,
		enum qat_service_type service);

void
qat_qp_csr_build_ring_base_gen1(void *io_addr,
		struct qat_queue *queue);

void
qat_qp_adf_arb_enable_gen1(const struct qat_queue *txq,
		void *base_addr, rte_spinlock_t *lock);

void
qat_qp_adf_arb_disable_gen1(const struct qat_queue *txq,
		void *base_addr, rte_spinlock_t *lock);

void
qat_qp_adf_configure_queues_gen1(struct qat_qp *qp);

void
qat_qp_csr_write_tail_gen1(struct qat_qp *qp, struct qat_queue *q);

void
qat_qp_csr_write_head_gen1(struct qat_qp *qp, struct qat_queue *q,
		uint32_t new_head);

void
qat_qp_csr_setup_gen1(struct qat_pci_device *qat_dev,
		void *io_addr, struct qat_qp *qp);

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

#endif
