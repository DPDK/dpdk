/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <dev_driver.h>
#include <rte_pci.h>

#include "qat_device.h"
#include "qat_qp.h"
#include "adf_transport_access_macros_gen4vf.h"
#include "adf_pf2vf_msg.h"
#include "qat_pf2vf.h"

#include <stdint.h>

/* QAT GEN 4 specific macros */
#define QAT_GEN4_BUNDLE_NUM             4
#define QAT_GEN4_QPS_PER_BUNDLE_NUM     1

struct qat_dev_gen4_extra {
	struct qat_qp_hw_data qp_gen4_data[QAT_GEN4_BUNDLE_NUM]
		[QAT_GEN4_QPS_PER_BUNDLE_NUM];
};

static struct qat_pf2vf_dev qat_pf2vf_gen4 = {
	.pf2vf_offset = ADF_4XXXIOV_PF2VM_OFFSET,
	.vf2pf_offset = ADF_4XXXIOV_VM2PF_OFFSET,
	.pf2vf_type_shift = ADF_PFVF_2X_MSGTYPE_SHIFT,
	.pf2vf_type_mask = ADF_PFVF_2X_MSGTYPE_MASK,
	.pf2vf_data_shift = ADF_PFVF_2X_MSGDATA_SHIFT,
	.pf2vf_data_mask = ADF_PFVF_2X_MSGDATA_MASK,
};

static int
qat_query_svc_gen4(struct qat_pci_device *qat_dev, uint8_t *val)
{
	struct qat_pf2vf_msg pf2vf_msg;

	pf2vf_msg.msg_type = ADF_VF2PF_MSGTYPE_GET_SMALL_BLOCK_REQ;
	pf2vf_msg.block_hdr = ADF_VF2PF_BLOCK_MSG_GET_RING_TO_SVC_REQ;
	pf2vf_msg.msg_data = 2;
	return qat_pf2vf_exch_msg(qat_dev, pf2vf_msg, 2, val);
}

static int
qat_select_valid_queue_gen4(struct qat_pci_device *qat_dev, int qp_id,
			enum qat_service_type service_type)
{
	int i = 0, valid_qps = 0;
	struct qat_dev_gen4_extra *dev_extra = qat_dev->dev_private;

	for (; i < QAT_GEN4_BUNDLE_NUM; i++) {
		if (dev_extra->qp_gen4_data[i][0].service_type ==
			service_type) {
			if (valid_qps == qp_id)
				return i;
			++valid_qps;
		}
	}
	return -1;
}

static const struct qat_qp_hw_data *
qat_qp_get_hw_data_gen4(struct qat_pci_device *qat_dev,
		enum qat_service_type service_type, uint16_t qp_id)
{
	struct qat_dev_gen4_extra *dev_extra = qat_dev->dev_private;
	int ring_pair = qat_select_valid_queue_gen4(qat_dev, qp_id,
			service_type);

	if (ring_pair < 0)
		return NULL;

	return &dev_extra->qp_gen4_data[ring_pair][0];
}

static int
qat_qp_rings_per_service_gen4(struct qat_pci_device *qat_dev,
		enum qat_service_type service)
{
	int i = 0, count = 0, max_ops_per_srv = 0;
	struct qat_dev_gen4_extra *dev_extra = qat_dev->dev_private;

	max_ops_per_srv = QAT_GEN4_BUNDLE_NUM;
	for (i = 0, count = 0; i < max_ops_per_srv; i++)
		if (dev_extra->qp_gen4_data[i][0].service_type == service)
			count++;
	return count;
}

static enum qat_service_type
gen4_pick_service(uint8_t hw_service)
{
	switch (hw_service) {
	case QAT_SVC_SYM:
		return QAT_SERVICE_SYMMETRIC;
	case QAT_SVC_COMPRESSION:
		return QAT_SERVICE_COMPRESSION;
	case QAT_SVC_ASYM:
		return QAT_SERVICE_ASYMMETRIC;
	default:
		return QAT_SERVICE_INVALID;
	}
}

static int
qat_dev_read_config_gen4(struct qat_pci_device *qat_dev)
{
	int i = 0;
	uint16_t svc = 0;
	struct qat_dev_gen4_extra *dev_extra = qat_dev->dev_private;
	struct qat_qp_hw_data *hw_data;
	enum qat_service_type service_type;
	uint8_t hw_service;

	if (qat_query_svc_gen4(qat_dev, (uint8_t *)&svc))
		return -EFAULT;
	for (; i < QAT_GEN4_BUNDLE_NUM; i++) {
		hw_service = (svc >> (3 * i)) & 0x7;
		service_type = gen4_pick_service(hw_service);
		if (service_type == QAT_SERVICE_INVALID) {
			QAT_LOG(ERR,
				"Unrecognized service on bundle %d",
				i);
			return -ENOTSUP;
		}
		hw_data = &dev_extra->qp_gen4_data[i][0];
		memset(hw_data, 0, sizeof(*hw_data));
		hw_data->service_type = service_type;
		if (service_type == QAT_SERVICE_ASYMMETRIC) {
			hw_data->tx_msg_size = 64;
			hw_data->rx_msg_size = 32;
		} else if (service_type == QAT_SERVICE_SYMMETRIC ||
				service_type ==
					QAT_SERVICE_COMPRESSION) {
			hw_data->tx_msg_size = 128;
			hw_data->rx_msg_size = 32;
		}
		hw_data->tx_ring_num = 0;
		hw_data->rx_ring_num = 1;
		hw_data->hw_bundle_num = i;
	}
	return 0;
}

static void
qat_qp_build_ring_base_gen4(void *io_addr,
			struct qat_queue *queue)
{
	uint64_t queue_base;

	queue_base = BUILD_RING_BASE_ADDR_GEN4(queue->base_phys_addr,
			queue->queue_size);
	WRITE_CSR_RING_BASE_GEN4VF(io_addr, queue->hw_bundle_number,
		queue->hw_queue_number, queue_base);
}

static void
qat_qp_adf_arb_enable_gen4(const struct qat_queue *txq,
			void *base_addr, rte_spinlock_t *lock)
{
	uint32_t arb_csr_offset = 0, value;

	rte_spinlock_lock(lock);
	arb_csr_offset = ADF_ARB_RINGSRVARBEN_OFFSET +
			(ADF_RING_BUNDLE_SIZE_GEN4 *
			txq->hw_bundle_number);
	value = ADF_CSR_RD(base_addr + ADF_RING_CSR_ADDR_OFFSET_GEN4VF,
			arb_csr_offset);
	value |= (0x01 << txq->hw_queue_number);
	ADF_CSR_WR(base_addr, arb_csr_offset, value);
	rte_spinlock_unlock(lock);
}

static void
qat_qp_adf_arb_disable_gen4(const struct qat_queue *txq,
			void *base_addr, rte_spinlock_t *lock)
{
	uint32_t arb_csr_offset = 0, value;

	rte_spinlock_lock(lock);
	arb_csr_offset = ADF_ARB_RINGSRVARBEN_OFFSET +
			(ADF_RING_BUNDLE_SIZE_GEN4 *
			txq->hw_bundle_number);
	value = ADF_CSR_RD(base_addr + ADF_RING_CSR_ADDR_OFFSET_GEN4VF,
			arb_csr_offset);
	value &= ~(0x01 << txq->hw_queue_number);
	ADF_CSR_WR(base_addr, arb_csr_offset, value);
	rte_spinlock_unlock(lock);
}

static void
qat_qp_adf_configure_queues_gen4(struct qat_qp *qp)
{
	uint32_t q_tx_config, q_resp_config;
	struct qat_queue *q_tx = &qp->tx_q, *q_rx = &qp->rx_q;

	q_tx_config = BUILD_RING_CONFIG(q_tx->queue_size);
	q_resp_config = BUILD_RESP_RING_CONFIG(q_rx->queue_size,
			ADF_RING_NEAR_WATERMARK_512,
			ADF_RING_NEAR_WATERMARK_0);

	WRITE_CSR_RING_CONFIG_GEN4VF(qp->mmap_bar_addr,
		q_tx->hw_bundle_number,	q_tx->hw_queue_number,
		q_tx_config);
	WRITE_CSR_RING_CONFIG_GEN4VF(qp->mmap_bar_addr,
		q_rx->hw_bundle_number,	q_rx->hw_queue_number,
		q_resp_config);
}

static void
qat_qp_csr_write_tail_gen4(struct qat_qp *qp, struct qat_queue *q)
{
	WRITE_CSR_RING_TAIL_GEN4VF(qp->mmap_bar_addr,
		q->hw_bundle_number, q->hw_queue_number, q->tail);
}

static void
qat_qp_csr_write_head_gen4(struct qat_qp *qp, struct qat_queue *q,
			uint32_t new_head)
{
	WRITE_CSR_RING_HEAD_GEN4VF(qp->mmap_bar_addr,
			q->hw_bundle_number, q->hw_queue_number, new_head);
}

static void
qat_qp_csr_setup_gen4(struct qat_pci_device *qat_dev,
			void *io_addr, struct qat_qp *qp)
{
	qat_qp_build_ring_base_gen4(io_addr, &qp->tx_q);
	qat_qp_build_ring_base_gen4(io_addr, &qp->rx_q);
	qat_qp_adf_configure_queues_gen4(qp);
	qat_qp_adf_arb_enable_gen4(&qp->tx_q, qp->mmap_bar_addr,
					&qat_dev->arb_csr_lock);
}

static struct qat_qp_hw_spec_funcs qat_qp_hw_spec_gen4 = {
	.qat_qp_rings_per_service = qat_qp_rings_per_service_gen4,
	.qat_qp_build_ring_base = qat_qp_build_ring_base_gen4,
	.qat_qp_adf_arb_enable = qat_qp_adf_arb_enable_gen4,
	.qat_qp_adf_arb_disable = qat_qp_adf_arb_disable_gen4,
	.qat_qp_adf_configure_queues = qat_qp_adf_configure_queues_gen4,
	.qat_qp_csr_write_tail = qat_qp_csr_write_tail_gen4,
	.qat_qp_csr_write_head = qat_qp_csr_write_head_gen4,
	.qat_qp_csr_setup = qat_qp_csr_setup_gen4,
	.qat_qp_get_hw_data = qat_qp_get_hw_data_gen4,
};

static int
qat_reset_ring_pairs_gen4(struct qat_pci_device *qat_pci_dev)
{
	int ret = 0, i;
	uint8_t data[4];
	struct qat_pf2vf_msg pf2vf_msg;

	pf2vf_msg.msg_type = ADF_VF2PF_MSGTYPE_RP_RESET;
	pf2vf_msg.block_hdr = -1;
	for (i = 0; i < QAT_GEN4_BUNDLE_NUM; i++) {
		pf2vf_msg.msg_data = i;
		ret = qat_pf2vf_exch_msg(qat_pci_dev, pf2vf_msg, 1, data);
		if (ret) {
			QAT_LOG(ERR, "QAT error when reset bundle no %d",
				i);
			return ret;
		}
	}

	return 0;
}

static const struct rte_mem_resource *
qat_dev_get_transport_bar_gen4(struct rte_pci_device *pci_dev)
{
	return &pci_dev->mem_resource[0];
}

static int
qat_dev_get_misc_bar_gen4(struct rte_mem_resource **mem_resource,
		struct rte_pci_device *pci_dev)
{
	*mem_resource = &pci_dev->mem_resource[2];
	return 0;
}

static int
qat_dev_get_slice_map_gen4(uint32_t *map __rte_unused,
	const struct rte_pci_device *pci_dev __rte_unused)
{
	return 0;
}

static int
qat_dev_get_extra_size_gen4(void)
{
	return sizeof(struct qat_dev_gen4_extra);
}

static struct qat_dev_hw_spec_funcs qat_dev_hw_spec_gen4 = {
	.qat_dev_reset_ring_pairs = qat_reset_ring_pairs_gen4,
	.qat_dev_get_transport_bar = qat_dev_get_transport_bar_gen4,
	.qat_dev_get_misc_bar = qat_dev_get_misc_bar_gen4,
	.qat_dev_read_config = qat_dev_read_config_gen4,
	.qat_dev_get_extra_size = qat_dev_get_extra_size_gen4,
	.qat_dev_get_slice_map = qat_dev_get_slice_map_gen4,
};

RTE_INIT(qat_dev_gen_4_init)
{
	qat_qp_hw_spec[QAT_GEN4] = &qat_qp_hw_spec_gen4;
	qat_dev_hw_spec[QAT_GEN4] = &qat_dev_hw_spec_gen4;
	qat_gen_config[QAT_GEN4].dev_gen = QAT_GEN4;
	qat_gen_config[QAT_GEN4].pf2vf_dev = &qat_pf2vf_gen4;
}
