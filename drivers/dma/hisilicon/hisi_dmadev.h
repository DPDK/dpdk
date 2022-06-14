/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 HiSilicon Limited
 */

#ifndef HISI_DMADEV_H
#define HISI_DMADEV_H

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_memzone.h>
#include <rte_dmadev_pmd.h>

#define BIT(x)	(1ul << (x))
#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#define GENMASK(h, l) \
		(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define BF_SHF(x) (__builtin_ffsll(x) - 1)
#define FIELD_GET(mask, reg) \
		((typeof(mask))(((reg) & (mask)) >> BF_SHF(mask)))

#define lower_32_bits(x) ((uint32_t)(x))
#define upper_32_bits(x) ((uint32_t)(((x) >> 16) >> 16))

#define PCI_VENDOR_ID_HUAWEI			0x19e5
#define HISI_DMA_DEVICE_ID			0xA122
#define HISI_DMA_PCI_REVISION_ID_REG		0x08
#define HISI_DMA_REVISION_HIP08B		0x21
#define HISI_DMA_REVISION_HIP09A		0x30

#define HISI_DMA_MAX_HW_QUEUES			4
#define HISI_DMA_MAX_DESC_NUM			8192
#define HISI_DMA_MIN_DESC_NUM			32

/**
 * The HIP08B(HiSilicon IP08) and HIP09B(HiSilicon IP09) are DMA iEPs, they
 * have the same pci device id but different pci revision.
 * Unfortunately, they have different register layouts, so two layout
 * enumerations are defined.
 */
enum {
	HISI_DMA_REG_LAYOUT_INVALID = 0,
	HISI_DMA_REG_LAYOUT_HIP08,
	HISI_DMA_REG_LAYOUT_HIP09
};

/**
 * Hardware PCI bar register MAP:
 *
 *     --------------
 *     | Misc-reg-0 |
 *     |            |
 *     --------------   -> Queue base
 *     |            |
 *     | Queue-0    |
 *     |            |
 *     --------------   ---
 *     |            |    ^
 *     | Queue-1    |   Queue region
 *     |            |    v
 *     --------------   ---
 *     | ...        |
 *     | Queue-x    |
 *     | ...        |
 *     --------------
 *     | Misc-reg-1 |
 *     --------------
 *
 * As described above, a single queue register is continuous and occupies the
 * length of queue-region. The global offset for a single queue register is
 * calculated by:
 *     offset = queue-base + (queue-id * queue-region) + reg-offset-in-region.
 *
 * The first part of queue region is basically the same for HIP08 and HIP09
 * register layouts, therefore, HISI_QUEUE_* registers are defined for it.
 */
#define HISI_DMA_QUEUE_SQ_BASE_L_REG		0x0
#define HISI_DMA_QUEUE_SQ_BASE_H_REG		0x4
#define HISI_DMA_QUEUE_SQ_DEPTH_REG		0x8
#define HISI_DMA_QUEUE_SQ_TAIL_REG		0xC
#define HISI_DMA_QUEUE_CQ_BASE_L_REG		0x10
#define HISI_DMA_QUEUE_CQ_BASE_H_REG		0x14
#define HISI_DMA_QUEUE_CQ_DEPTH_REG		0x18
#define HISI_DMA_QUEUE_CQ_HEAD_REG		0x1C
#define HISI_DMA_QUEUE_CTRL0_REG		0x20
#define HISI_DMA_QUEUE_CTRL0_EN_B		0
#define HISI_DMA_QUEUE_CTRL0_PAUSE_B		4
#define HISI_DMA_QUEUE_CTRL1_REG		0x24
#define HISI_DMA_QUEUE_CTRL1_RESET_B		0
#define HISI_DMA_QUEUE_FSM_REG			0x30
#define HISI_DMA_QUEUE_FSM_STS_M		GENMASK(3, 0)
#define HISI_DMA_QUEUE_INT_STATUS_REG		0x40
#define HISI_DMA_QUEUE_INT_MASK_REG		0x44
#define HISI_DMA_QUEUE_ERR_INT_NUM0_REG		0x84
#define HISI_DMA_QUEUE_ERR_INT_NUM1_REG		0x88
#define HISI_DMA_QUEUE_ERR_INT_NUM2_REG		0x8C
#define HISI_DMA_QUEUE_REGION_SIZE		0x100

/**
 * HiSilicon IP08 DMA register and field define:
 */
#define HISI_DMA_HIP08_QUEUE_BASE			0x0
#define HISI_DMA_HIP08_QUEUE_CTRL0_ERR_ABORT_B		2
#define HISI_DMA_HIP08_QUEUE_INT_MASK_M			GENMASK(14, 0)
#define HISI_DMA_HIP08_QUEUE_ERR_INT_NUM3_REG		0x90
#define HISI_DMA_HIP08_QUEUE_ERR_INT_NUM4_REG		0x94
#define HISI_DMA_HIP08_QUEUE_ERR_INT_NUM5_REG		0x98
#define HISI_DMA_HIP08_QUEUE_ERR_INT_NUM6_REG		0x48
#define HISI_DMA_HIP08_MODE_REG				0x217C
#define HISI_DMA_HIP08_MODE_SEL_B			0
#define HISI_DMA_HIP08_DUMP_START_REG			0x2000
#define HISI_DMA_HIP08_DUMP_END_REG			0x2280

/**
 * HiSilicon IP09 DMA register and field define:
 */
#define HISI_DMA_HIP09_QUEUE_BASE			0x2000
#define HISI_DMA_HIP09_QUEUE_CTRL0_ERR_ABORT_M		GENMASK(31, 28)
#define HISI_DMA_HIP09_QUEUE_CTRL1_VA_ENABLE_B		2
#define HISI_DMA_HIP09_QUEUE_INT_MASK_M			0x1
#define HISI_DMA_HIP09_QUEUE_ERR_INT_STATUS_REG		0x48
#define HISI_DMA_HIP09_QUEUE_ERR_INT_MASK_REG		0x4C
#define HISI_DMA_HIP09_QUEUE_ERR_INT_MASK_M		GENMASK(18, 1)
#define HISI_DMA_HIP09_QUEUE_CFG_REG(queue_id)		(0x800 + \
							 (queue_id) * 0x20)
#define HISI_DMA_HIP09_QUEUE_CFG_LINK_DOWN_MASK_B	16
#define HISI_DMA_HIP09_DUMP_REGION_A_START_REG		0x0
#define HISI_DMA_HIP09_DUMP_REGION_A_END_REG		0x368
#define HISI_DMA_HIP09_DUMP_REGION_B_START_REG		0x800
#define HISI_DMA_HIP09_DUMP_REGION_B_END_REG		0xA08
#define HISI_DMA_HIP09_DUMP_REGION_C_START_REG		0x1800
#define HISI_DMA_HIP09_DUMP_REGION_C_END_REG		0x1A4C
#define HISI_DMA_HIP09_DUMP_REGION_D_START_REG		0x1C00
#define HISI_DMA_HIP09_DUMP_REGION_D_END_REG		0x1CC4

/**
 * In fact, there are multiple states, but it need to pay attention to
 * the following three states for the driver:
 */
enum {
	HISI_DMA_STATE_IDLE = 0,
	HISI_DMA_STATE_RUN,
	/**
	 * All of the submitted descriptor are finished, and the queue
	 * is waiting for new descriptors.
	 */
	HISI_DMA_STATE_CPL,
};

/**
 * Hardware complete status define:
 */
#define HISI_DMA_STATUS_SUCCESS			0x0
#define HISI_DMA_STATUS_INVALID_OPCODE		0x1
#define HISI_DMA_STATUS_INVALID_LENGTH		0x2
#define HISI_DMA_STATUS_USER_ABORT		0x4
#define HISI_DMA_STATUS_REMOTE_READ_ERROR	0x10
#define HISI_DMA_STATUS_AXI_READ_ERROR		0x20
#define HISI_DMA_STATUS_AXI_WRITE_ERROR		0x40
#define HISI_DMA_STATUS_DATA_POISON		0x80
#define HISI_DMA_STATUS_SQE_READ_ERROR		0x100
#define HISI_DMA_STATUS_SQE_READ_POISION	0x200
#define HISI_DMA_STATUS_REMOTE_DATA_POISION	0x400
#define HISI_DMA_STATUS_LINK_DOWN_ERROR		0x800

/**
 * After scanning the CQ array, the CQ head register needs to be updated.
 * Updating the register involves write memory barrier operations.
 * Here use the following method to reduce WMB operations:
 *   a) malloc more CQEs, which correspond to the macro HISI_DMA_CQ_RESERVED.
 *   b) update the CQ head register after accumulated number of completed CQs
 *      is greater than or equal to HISI_DMA_CQ_RESERVED.
 */
#define HISI_DMA_CQ_RESERVED		64

struct hisi_dma_sqe {
	uint32_t dw0;
#define SQE_FENCE_FLAG	BIT(10)
#define SQE_OPCODE_M2M	0x4
	uint32_t dw1;
	uint32_t dw2;
	uint32_t length;
	uint64_t src_addr;
	uint64_t dst_addr;
};

struct hisi_dma_cqe {
	uint64_t rsv;
	uint64_t misc;
#define CQE_SQ_HEAD_MASK	GENMASK(15, 0)
#define CQE_VALID_B		BIT(48)
#define CQE_STATUS_MASK		GENMASK(63, 49)
};

struct hisi_dma_dev {
	struct hisi_dma_sqe *sqe;
	volatile struct hisi_dma_cqe *cqe;
	uint16_t *status; /* the completion status array of SQEs. */

	volatile void *sq_tail_reg; /**< register address for doorbell. */
	volatile void *cq_head_reg; /**< register address for answer CQ. */

	uint16_t sq_depth_mask; /**< SQ depth - 1, the SQ depth is power of 2 */
	uint16_t cq_depth; /* CQ depth */

	uint16_t ridx; /**< ring index which will assign to the next request. */
	/** ring index which returned by hisi_dmadev_completed APIs. */
	uint16_t cridx;

	/**
	 * SQE array management fields:
	 *
	 *  -----------------------------------------------------
	 *  | SQE0 | SQE1 | SQE2 |   ...  | SQEx | ... | SQEn-1 |
	 *  -----------------------------------------------------
	 *     ^             ^               ^
	 *     |             |               |
	 *   sq_head     cq_sq_head       sq_tail
	 *
	 *  sq_head: index to the oldest completed request, this filed was
	 *           updated by hisi_dmadev_completed* APIs.
	 *  sq_tail: index of the next new request, this field was updated by
	 *           hisi_dmadev_copy API.
	 *  cq_sq_head: next index of index that has been completed by hardware,
	 *              this filed was updated by hisi_dmadev_completed* APIs.
	 *
	 *  [sq_head, cq_sq_head): the SQEs that hardware already completed.
	 *  [cq_sq_head, sq_tail): the SQEs that hardware processing.
	 */
	uint16_t sq_head;
	uint16_t sq_tail;
	uint16_t cq_sq_head;
	/**
	 * The driver scans the CQE array, if the valid bit changes, the CQE is
	 * considered valid.
	 * Note: One CQE is corresponding to one or several SQEs, e.g. app
	 *       submits two copy requests, the hardware processes the two SQEs,
	 *       but it may write back only one CQE and the CQE's sq_head field
	 *       indicates the index of the second copy request in the SQE
	 *       array.
	 */
	uint16_t cq_head; /**< CQ index for next scans. */
	/** accumulated number of completed CQs
	 * @see HISI_DMA_CQ_RESERVED
	 */
	uint16_t cqs_completed;
	uint8_t cqe_vld; /**< valid bit for CQE, will change for every round. */

	uint64_t submitted;
	uint64_t completed;
	uint64_t errors;
	uint64_t qfulls;

	/**
	 * The following fields are not accessed in the I/O path, so they are
	 * placed at the end.
	 */
	struct rte_dma_dev_data *data;
	uint8_t revision; /**< PCI revision. */
	uint8_t reg_layout; /**< hardware register layout. */
	void *io_base;
	uint8_t queue_id; /**< hardware DMA queue index. */
	const struct rte_memzone *iomz;
	uint32_t iomz_sz;
	rte_iova_t sqe_iova;
	rte_iova_t cqe_iova;
};

#endif /* HISI_DMADEV_H */
