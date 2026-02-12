/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#include <rte_pci.h>

#include "qat_device.h"
#include "qat_qp.h"
#include "adf_transport_access_macros_gen_lcevf.h"
#include "adf_pf2vf_msg.h"
#include "qat_pf2vf.h"

#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define BITS_PER_ULONG		(sizeof(unsigned long) * 8)

#define LCE_DEVICE_NAME_SIZE			64
#define LCE_DEVICE_MAX_BANKS			2080
#define LCE_DIV_ROUND_UP(n, d)  (((n) + (d) - 1) / (d))
#define LCE_DEVICE_BITMAP_SIZE	LCE_DIV_ROUND_UP(LCE_DEVICE_MAX_BANKS, BITS_PER_ULONG)

/* QAT GEN_LCE specific macros */
#define QAT_GEN_LCE_BUNDLE_NUM		LCE_DEVICE_MAX_BANKS
#define QAT_GEN4_QPS_PER_BUNDLE_NUM	1

/**
 * struct lce_vfio_dev_cap - LCE device capabilities
 *
 * Device level capabilities and service level capabilities
 */
struct lce_vfio_dev_cap {
	uint16_t device_num;
	uint16_t device_type;
	uint32_t capability_mask;
	uint32_t extended_capabilities;
	uint16_t max_banks;
	uint16_t max_rings_per_bank;
	uint16_t arb_mask;
	uint16_t services;
	uint16_t pkg_id;
	uint16_t node_id;
	uint8_t device_name[LCE_DEVICE_NAME_SIZE];
};

/* struct lce_vfio_dev_cy_cap - CY capabilities of LCE device */
struct lce_vfio_dev_cy_cap {
	uint32_t nr_banks;
	unsigned long bitmap[LCE_DEVICE_BITMAP_SIZE];
};

struct lce_qat_domain {
	uint32_t nid        :3;
	uint32_t fid        :7;
	uint32_t ftype      :2;
	uint32_t vfid       :13;
	uint32_t rid        :4;
	uint32_t vld        :1;
	uint32_t desc_over  :1;
	uint32_t pasid_vld  :1;
	uint32_t pasid      :20;
};

struct lce_qat_buf_domain {
	uint32_t bank_id:   20;
	uint32_t type:      4;
	uint32_t resv:      8;
	struct lce_qat_domain dom;
};

struct qat_dev_gen_lce_extra {
	struct qat_qp_hw_data
	    qp_gen_lce_data[QAT_GEN_LCE_BUNDLE_NUM][QAT_GEN4_QPS_PER_BUNDLE_NUM];
};

static struct qat_pf2vf_dev qat_pf2vf_gen_lce = {
	.pf2vf_offset = ADF_4XXXIOV_PF2VM_OFFSET,
	.vf2pf_offset = ADF_4XXXIOV_VM2PF_OFFSET,
	.pf2vf_type_shift = ADF_PFVF_2X_MSGTYPE_SHIFT,
	.pf2vf_type_mask = ADF_PFVF_2X_MSGTYPE_MASK,
	.pf2vf_data_shift = ADF_PFVF_2X_MSGDATA_SHIFT,
	.pf2vf_data_mask = ADF_PFVF_2X_MSGDATA_MASK,
};

static int
qat_select_valid_queue_gen_lce(struct qat_pci_device *qat_dev, int qp_id,
			    enum qat_service_type service_type)
{
	int i = 0, valid_qps = 0;
	struct qat_dev_gen_lce_extra *dev_extra = qat_dev->dev_private;

	for (; i < QAT_GEN_LCE_BUNDLE_NUM; i++) {
		if (dev_extra->qp_gen_lce_data[i][0].service_type == service_type) {
			if (valid_qps == qp_id)
				return i;
			++valid_qps;
		}
	}
	return -1;
}

static const struct qat_qp_hw_data *
qat_qp_get_hw_data_gen_lce(struct qat_pci_device *qat_dev,
			enum qat_service_type service_type, uint16_t qp_id)
{
	struct qat_dev_gen_lce_extra *dev_extra = qat_dev->dev_private;
	int ring_pair = qat_select_valid_queue_gen_lce(qat_dev, qp_id, service_type);

	if (ring_pair < 0)
		return NULL;

	return &dev_extra->qp_gen_lce_data[ring_pair][0];
}

static int
qat_qp_rings_per_service_gen_lce(struct qat_pci_device *qat_dev,
			      enum qat_service_type service)
{
	int i = 0, count = 0, max_ops_per_srv = 0;
	struct qat_dev_gen_lce_extra *dev_extra = qat_dev->dev_private;

	max_ops_per_srv = QAT_GEN_LCE_BUNDLE_NUM;
	for (i = 0, count = 0; i < max_ops_per_srv; i++)
		if (dev_extra->qp_gen_lce_data[i][0].service_type == service)
			count++;
	return count;
}

static int qat_dev_read_config_gen_lce(struct qat_pci_device *qat_dev)
{
	struct qat_dev_gen_lce_extra *dev_extra = qat_dev->dev_private;
	struct qat_qp_hw_data *hw_data;

	/** Enable only crypto ring: RP-0 */
	hw_data = &dev_extra->qp_gen_lce_data[0][0];
	memset(hw_data, 0, sizeof(*hw_data));

	hw_data->service_type = QAT_SERVICE_SYMMETRIC;
	hw_data->tx_msg_size = 128;
	hw_data->rx_msg_size = 32;

	hw_data->tx_ring_num = 0;
	hw_data->rx_ring_num = 1;

	hw_data->hw_bundle_num = 0;

	return 0;
}

static void qat_qp_build_ring_base_gen_lce(void *io_addr, struct qat_queue *queue)
{
	uint64_t queue_base;

	queue_base = BUILD_RING_BASE_ADDR_GEN_LCE(queue->base_phys_addr, queue->queue_size);
	WRITE_CSR_RING_BASE_GEN_LCEVF(io_addr, queue->hw_bundle_number,
			queue->hw_queue_number, queue_base);
}

static void
qat_qp_adf_arb_enable_gen_lce(const struct qat_queue *txq,
			   void *base_addr, rte_spinlock_t *lock)
{
	uint32_t arb_csr_offset = 0, value;

	rte_spinlock_lock(lock);
	arb_csr_offset = ADF_ARB_RINGSRVARBEN_OFFSET +
			(ADF_RING_BUNDLE_SIZE_GEN_LCE * txq->hw_bundle_number);
	value = ADF_CSR_RD(base_addr + ADF_RING_CSR_ADDR_OFFSET_GEN_LCEVF, arb_csr_offset);
	value |= 0x01;
	ADF_CSR_WR(base_addr, arb_csr_offset, value);
	rte_spinlock_unlock(lock);
}

static void
qat_qp_adf_arb_disable_gen_lce(const struct qat_queue *txq, void *base_addr, rte_spinlock_t *lock)
{
	uint32_t arb_csr_offset = 0, value;

	rte_spinlock_lock(lock);
	arb_csr_offset = ADF_ARB_RINGSRVARBEN_OFFSET +
			(ADF_RING_BUNDLE_SIZE_GEN_LCE * txq->hw_bundle_number);
	value = ADF_CSR_RD(base_addr + ADF_RING_CSR_ADDR_OFFSET_GEN_LCEVF, arb_csr_offset);
	value &= ~(0x01);
	ADF_CSR_WR(base_addr, arb_csr_offset, value);
	rte_spinlock_unlock(lock);
}

static void
qat_qp_adf_configure_queues_gen_lce(struct qat_qp *qp)
{
	uint32_t q_tx_config, q_resp_config;
	struct qat_queue *q_tx = &qp->tx_q, *q_rx = &qp->rx_q;

	/* q_tx/rx->queue_size is initialized as per bundle config register */
	q_tx_config = BUILD_RING_CONFIG(q_tx->queue_size);

	q_resp_config = BUILD_RESP_RING_CONFIG(q_rx->queue_size,
					       ADF_RING_NEAR_WATERMARK_512,
					       ADF_RING_NEAR_WATERMARK_0);

	WRITE_CSR_RING_CONFIG_GEN_LCEVF(qp->mmap_bar_addr, q_tx->hw_bundle_number,
			q_tx->hw_queue_number, q_tx_config);
	WRITE_CSR_RING_CONFIG_GEN_LCEVF(qp->mmap_bar_addr, q_rx->hw_bundle_number,
			q_rx->hw_queue_number, q_resp_config);
}

static void
qat_qp_csr_write_tail_gen_lce(struct qat_qp *qp, struct qat_queue *q)
{
	WRITE_CSR_RING_TAIL_GEN_LCEVF(qp->mmap_bar_addr, q->hw_bundle_number,
				   q->hw_queue_number, q->tail);
}

static void
qat_qp_csr_write_head_gen_lce(struct qat_qp *qp, struct qat_queue *q, uint32_t new_head)
{
	WRITE_CSR_RING_HEAD_GEN_LCEVF(qp->mmap_bar_addr, q->hw_bundle_number,
				   q->hw_queue_number, new_head);
}

static void
qat_qp_csr_setup_gen_lce(struct qat_pci_device *qat_dev, void *io_addr, struct qat_qp *qp)
{
	qat_qp_build_ring_base_gen_lce(io_addr, &qp->tx_q);
	qat_qp_build_ring_base_gen_lce(io_addr, &qp->rx_q);
	qat_qp_adf_configure_queues_gen_lce(qp);
	qat_qp_adf_arb_enable_gen_lce(&qp->tx_q, qp->mmap_bar_addr, &qat_dev->arb_csr_lock);
}

static struct qat_qp_hw_spec_funcs qat_qp_hw_spec_gen_lce = {
	.qat_qp_rings_per_service = qat_qp_rings_per_service_gen_lce,
	.qat_qp_build_ring_base = qat_qp_build_ring_base_gen_lce,
	.qat_qp_adf_arb_enable = qat_qp_adf_arb_enable_gen_lce,
	.qat_qp_adf_arb_disable = qat_qp_adf_arb_disable_gen_lce,
	.qat_qp_adf_configure_queues = qat_qp_adf_configure_queues_gen_lce,
	.qat_qp_csr_write_tail = qat_qp_csr_write_tail_gen_lce,
	.qat_qp_csr_write_head = qat_qp_csr_write_head_gen_lce,
	.qat_qp_csr_setup = qat_qp_csr_setup_gen_lce,
	.qat_qp_get_hw_data = qat_qp_get_hw_data_gen_lce,
};

static int
qat_reset_ring_pairs_gen_lce(struct qat_pci_device *qat_pci_dev __rte_unused)
{
	return 0;
}

static const struct rte_mem_resource*
qat_dev_get_transport_bar_gen_lce(struct rte_pci_device *pci_dev)
{
	return &pci_dev->mem_resource[0];
}

static int
qat_dev_get_misc_bar_gen_lce(struct rte_mem_resource **mem_resource,
			  struct rte_pci_device *pci_dev)
{
	*mem_resource = &pci_dev->mem_resource[2];
	return 0;
}

static int
qat_dev_get_extra_size_gen_lce(void)
{
	return sizeof(struct qat_dev_gen_lce_extra);
}

static int
qat_dev_get_slice_map_gen_lce(uint32_t *map __rte_unused,
	const struct rte_pci_device *pci_dev __rte_unused)
{
	return 0;
}

static struct qat_dev_hw_spec_funcs qat_dev_hw_spec_gen_lce = {
	.qat_dev_reset_ring_pairs = qat_reset_ring_pairs_gen_lce,
	.qat_dev_get_transport_bar = qat_dev_get_transport_bar_gen_lce,
	.qat_dev_get_misc_bar = qat_dev_get_misc_bar_gen_lce,
	.qat_dev_read_config = qat_dev_read_config_gen_lce,
	.qat_dev_get_extra_size = qat_dev_get_extra_size_gen_lce,
	.qat_dev_get_slice_map = qat_dev_get_slice_map_gen_lce,
};

RTE_INIT(qat_dev_gen_lce_init)
{
	qat_qp_hw_spec[QAT_GEN_LCE] = &qat_qp_hw_spec_gen_lce;
	qat_dev_hw_spec[QAT_GEN_LCE] = &qat_dev_hw_spec_gen_lce;
	qat_gen_config[QAT_GEN_LCE].dev_gen = QAT_GEN_LCE;
	qat_gen_config[QAT_GEN_LCE].pf2vf_dev = &qat_pf2vf_gen_lce;
}
