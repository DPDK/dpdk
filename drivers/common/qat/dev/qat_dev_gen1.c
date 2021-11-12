/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include "qat_device.h"
#include "qat_qp.h"
#include "adf_transport_access_macros.h"
#include "qat_dev_gens.h"

#include <stdint.h>

#define ADF_ARB_REG_SLOT			0x1000

#define WRITE_CSR_ARB_RINGSRVARBEN(csr_addr, index, value) \
	ADF_CSR_WR(csr_addr, ADF_ARB_RINGSRVARBEN_OFFSET + \
	(ADF_ARB_REG_SLOT * index), value)

__extension__
const struct qat_qp_hw_data qat_gen1_qps[QAT_MAX_SERVICES]
					 [ADF_MAX_QPS_ON_ANY_SERVICE] = {
	/* queue pairs which provide an asymmetric crypto service */
	[QAT_SERVICE_ASYMMETRIC] = {
		{
			.service_type = QAT_SERVICE_ASYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 0,
			.rx_ring_num = 8,
			.tx_msg_size = 64,
			.rx_msg_size = 32,

		}, {
			.service_type = QAT_SERVICE_ASYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 1,
			.rx_ring_num = 9,
			.tx_msg_size = 64,
			.rx_msg_size = 32,
		}
	},
	/* queue pairs which provide a symmetric crypto service */
	[QAT_SERVICE_SYMMETRIC] = {
		{
			.service_type = QAT_SERVICE_SYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 2,
			.rx_ring_num = 10,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		},
		{
			.service_type = QAT_SERVICE_SYMMETRIC,
			.hw_bundle_num = 0,
			.tx_ring_num = 3,
			.rx_ring_num = 11,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		}
	},
	/* queue pairs which provide a compression service */
	[QAT_SERVICE_COMPRESSION] = {
		{
			.service_type = QAT_SERVICE_COMPRESSION,
			.hw_bundle_num = 0,
			.tx_ring_num = 6,
			.rx_ring_num = 14,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		}, {
			.service_type = QAT_SERVICE_COMPRESSION,
			.hw_bundle_num = 0,
			.tx_ring_num = 7,
			.rx_ring_num = 15,
			.tx_msg_size = 128,
			.rx_msg_size = 32,
		}
	}
};

const struct qat_qp_hw_data *
qat_qp_get_hw_data_gen1(struct qat_pci_device *dev __rte_unused,
		enum qat_service_type service_type, uint16_t qp_id)
{
	return qat_gen1_qps[service_type] + qp_id;
}

int
qat_qp_rings_per_service_gen1(struct qat_pci_device *qat_dev,
		enum qat_service_type service)
{
	int i = 0, count = 0;

	for (i = 0; i < ADF_MAX_QPS_ON_ANY_SERVICE; i++) {
		const struct qat_qp_hw_data *hw_qps =
				qat_qp_get_hw_data(qat_dev, service, i);

		if (hw_qps == NULL)
			continue;
		if (hw_qps->service_type == service && hw_qps->tx_msg_size)
			count++;
	}

	return count;
}

void
qat_qp_csr_build_ring_base_gen1(void *io_addr,
			struct qat_queue *queue)
{
	uint64_t queue_base;

	queue_base = BUILD_RING_BASE_ADDR(queue->base_phys_addr,
			queue->queue_size);
	WRITE_CSR_RING_BASE(io_addr, queue->hw_bundle_number,
		queue->hw_queue_number, queue_base);
}

void
qat_qp_adf_arb_enable_gen1(const struct qat_queue *txq,
			void *base_addr, rte_spinlock_t *lock)
{
	uint32_t arb_csr_offset = 0, value;

	rte_spinlock_lock(lock);
	arb_csr_offset = ADF_ARB_RINGSRVARBEN_OFFSET +
			(ADF_ARB_REG_SLOT *
			txq->hw_bundle_number);
	value = ADF_CSR_RD(base_addr,
			arb_csr_offset);
	value |= (0x01 << txq->hw_queue_number);
	ADF_CSR_WR(base_addr, arb_csr_offset, value);
	rte_spinlock_unlock(lock);
}

void
qat_qp_adf_arb_disable_gen1(const struct qat_queue *txq,
			void *base_addr, rte_spinlock_t *lock)
{
	uint32_t arb_csr_offset =  ADF_ARB_RINGSRVARBEN_OFFSET +
				(ADF_ARB_REG_SLOT * txq->hw_bundle_number);
	uint32_t value;

	rte_spinlock_lock(lock);
	value = ADF_CSR_RD(base_addr, arb_csr_offset);
	value &= ~(0x01 << txq->hw_queue_number);
	ADF_CSR_WR(base_addr, arb_csr_offset, value);
	rte_spinlock_unlock(lock);
}

void
qat_qp_adf_configure_queues_gen1(struct qat_qp *qp)
{
	uint32_t q_tx_config, q_resp_config;
	struct qat_queue *q_tx = &qp->tx_q, *q_rx = &qp->rx_q;

	q_tx_config = BUILD_RING_CONFIG(q_tx->queue_size);
	q_resp_config = BUILD_RESP_RING_CONFIG(q_rx->queue_size,
			ADF_RING_NEAR_WATERMARK_512,
			ADF_RING_NEAR_WATERMARK_0);
	WRITE_CSR_RING_CONFIG(qp->mmap_bar_addr,
		q_tx->hw_bundle_number,	q_tx->hw_queue_number,
		q_tx_config);
	WRITE_CSR_RING_CONFIG(qp->mmap_bar_addr,
		q_rx->hw_bundle_number,	q_rx->hw_queue_number,
		q_resp_config);
}

void
qat_qp_csr_write_tail_gen1(struct qat_qp *qp, struct qat_queue *q)
{
	WRITE_CSR_RING_TAIL(qp->mmap_bar_addr, q->hw_bundle_number,
		q->hw_queue_number, q->tail);
}

void
qat_qp_csr_write_head_gen1(struct qat_qp *qp, struct qat_queue *q,
			uint32_t new_head)
{
	WRITE_CSR_RING_HEAD(qp->mmap_bar_addr, q->hw_bundle_number,
			q->hw_queue_number, new_head);
}

void
qat_qp_csr_setup_gen1(struct qat_pci_device *qat_dev,
			void *io_addr, struct qat_qp *qp)
{
	qat_qp_csr_build_ring_base_gen1(io_addr, &qp->tx_q);
	qat_qp_csr_build_ring_base_gen1(io_addr, &qp->rx_q);
	qat_qp_adf_configure_queues_gen1(qp);
	qat_qp_adf_arb_enable_gen1(&qp->tx_q, qp->mmap_bar_addr,
					&qat_dev->arb_csr_lock);
}

static struct qat_qp_hw_spec_funcs qat_qp_hw_spec_gen1 = {
	.qat_qp_rings_per_service = qat_qp_rings_per_service_gen1,
	.qat_qp_build_ring_base = qat_qp_csr_build_ring_base_gen1,
	.qat_qp_adf_arb_enable = qat_qp_adf_arb_enable_gen1,
	.qat_qp_adf_arb_disable = qat_qp_adf_arb_disable_gen1,
	.qat_qp_adf_configure_queues = qat_qp_adf_configure_queues_gen1,
	.qat_qp_csr_write_tail = qat_qp_csr_write_tail_gen1,
	.qat_qp_csr_write_head = qat_qp_csr_write_head_gen1,
	.qat_qp_csr_setup = qat_qp_csr_setup_gen1,
	.qat_qp_get_hw_data = qat_qp_get_hw_data_gen1,
};

int
qat_reset_ring_pairs_gen1(struct qat_pci_device *qat_pci_dev __rte_unused)
{
	/*
	 * Ring pairs reset not supported on base, continue
	 */
	return 0;
}

const struct rte_mem_resource *
qat_dev_get_transport_bar_gen1(struct rte_pci_device *pci_dev)
{
	return &pci_dev->mem_resource[0];
}

int
qat_dev_get_misc_bar_gen1(struct rte_mem_resource **mem_resource __rte_unused,
		struct rte_pci_device *pci_dev __rte_unused)
{
	return -1;
}

int
qat_dev_read_config_gen1(struct qat_pci_device *qat_dev __rte_unused)
{
	/*
	 * Base generations do not have configuration,
	 * but set this pointer anyway that we can
	 * distinguish higher generations faulty set to NULL
	 */
	return 0;
}

int
qat_dev_get_extra_size_gen1(void)
{
	return 0;
}

static struct qat_dev_hw_spec_funcs qat_dev_hw_spec_gen1 = {
	.qat_dev_reset_ring_pairs = qat_reset_ring_pairs_gen1,
	.qat_dev_get_transport_bar = qat_dev_get_transport_bar_gen1,
	.qat_dev_get_misc_bar = qat_dev_get_misc_bar_gen1,
	.qat_dev_read_config = qat_dev_read_config_gen1,
	.qat_dev_get_extra_size = qat_dev_get_extra_size_gen1,
};

RTE_INIT(qat_dev_gen_gen1_init)
{
	qat_qp_hw_spec[QAT_GEN1] = &qat_qp_hw_spec_gen1;
	qat_dev_hw_spec[QAT_GEN1] = &qat_dev_hw_spec_gen1;
	qat_gen_config[QAT_GEN1].dev_gen = QAT_GEN1;
}
