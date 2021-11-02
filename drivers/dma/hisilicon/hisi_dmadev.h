/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 HiSilicon Limited
 */

#ifndef HISI_DMADEV_H
#define HISI_DMADEV_H

#include <rte_byteorder.h>
#include <rte_common.h>

#define BIT(x)	(1ul << (x))
#define BITS_PER_LONG	(__SIZEOF_LONG__ * 8)
#define GENMASK(h, l) \
		(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#define BF_SHF(x) (__builtin_ffsll(x) - 1)
#define FIELD_GET(mask, reg) \
		((typeof(mask))(((reg) & (mask)) >> BF_SHF(mask)))

#define PCI_VENDOR_ID_HUAWEI			0x19e5
#define HISI_DMA_DEVICE_ID			0xA122
#define HISI_DMA_PCI_REVISION_ID_REG		0x08
#define HISI_DMA_REVISION_HIP08B		0x21

#define HISI_DMA_MAX_HW_QUEUES			4

/**
 * The HIP08B(HiSilicon IP08) and later Chip(e.g. HiSilicon IP09) are DMA iEPs,
 * they have the same pci device id but with different pci revision.
 * Unfortunately, they have different register layouts, so the layout
 * enumerations are defined.
 */
enum {
	HISI_DMA_REG_LAYOUT_INVALID = 0,
	HISI_DMA_REG_LAYOUT_HIP08
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
 * The first part of queue region is basically the same for HIP08 and later chip
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
#define HISI_DMA_QUEUE_ERR_INT_NUM0_REG		0x84
#define HISI_DMA_QUEUE_ERR_INT_NUM1_REG		0x88
#define HISI_DMA_QUEUE_ERR_INT_NUM2_REG		0x8C
#define HISI_DMA_QUEUE_REGION_SIZE		0x100

/**
 * HiSilicon IP08 DMA register and field define:
 */
#define HISI_DMA_HIP08_QUEUE_BASE			0x0
#define HISI_DMA_HIP08_QUEUE_CTRL0_ERR_ABORT_B		2
#define HISI_DMA_HIP08_QUEUE_INT_MASK_REG		0x44
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
 * In fact, there are multiple states, but it need to pay attention to
 * the following two states for the driver:
 */
enum {
	HISI_DMA_STATE_IDLE = 0,
	HISI_DMA_STATE_RUN,
};

struct hisi_dma_dev {
	struct rte_dma_dev_data *data;
	uint8_t revision; /**< PCI revision. */
	uint8_t reg_layout; /**< hardware register layout. */
	void *io_base;
	uint8_t queue_id; /**< hardware DMA queue index. */
};

#endif /* HISI_DMADEV_H */
