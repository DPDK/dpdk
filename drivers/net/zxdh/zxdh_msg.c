/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <stdbool.h>

#include <rte_common.h>
#include <rte_memcpy.h>
#include <rte_spinlock.h>
#include <rte_cycles.h>
#include <inttypes.h>
#include <rte_malloc.h>

#include "zxdh_ethdev.h"
#include "zxdh_logs.h"
#include "zxdh_msg.h"

#define ZXDH_REPS_INFO_FLAG_USABLE  0x00
#define ZXDH_BAR_SEQID_NUM_MAX      256

#define ZXDH_PCIEID_IS_PF_MASK      (0x0800)
#define ZXDH_PCIEID_PF_IDX_MASK     (0x0700)
#define ZXDH_PCIEID_VF_IDX_MASK     (0x00ff)
#define ZXDH_PCIEID_EP_IDX_MASK     (0x7000)
/* PCIEID bit field offset */
#define ZXDH_PCIEID_PF_IDX_OFFSET   (8)
#define ZXDH_PCIEID_EP_IDX_OFFSET   (12)

#define ZXDH_MULTIPLY_BY_8(x)       ((x) << 3)
#define ZXDH_MULTIPLY_BY_32(x)      ((x) << 5)
#define ZXDH_MULTIPLY_BY_256(x)     ((x) << 8)

#define ZXDH_MAX_EP_NUM              (4)
#define ZXDH_MAX_HARD_SPINLOCK_NUM   (511)

#define ZXDH_BAR0_SPINLOCK_OFFSET       (0x4000)
#define ZXDH_FW_SHRD_OFFSET             (0x5000)
#define ZXDH_FW_SHRD_INNER_HW_LABEL_PAT (0x800)
#define ZXDH_HW_LABEL_OFFSET   \
	(ZXDH_FW_SHRD_OFFSET + ZXDH_FW_SHRD_INNER_HW_LABEL_PAT)

struct zxdh_dev_stat {
	bool is_mpf_scanned;
	bool is_res_init;
	int16_t dev_cnt; /* probe cnt */
};

struct zxdh_seqid_item {
	void *reps_addr;
	uint16_t id;
	uint16_t buffer_len;
	uint16_t flag;
};

struct zxdh_seqid_ring {
	uint16_t cur_id;
	rte_spinlock_t lock;
	struct zxdh_seqid_item reps_info_tbl[ZXDH_BAR_SEQID_NUM_MAX];
};

static struct zxdh_dev_stat g_dev_stat;
static struct zxdh_seqid_ring g_seqid_ring;
static rte_spinlock_t chan_lock;

static uint16_t
zxdh_pcie_id_to_hard_lock(uint16_t src_pcieid, uint8_t dst)
{
	uint16_t lock_id = 0;
	uint16_t pf_idx = (src_pcieid & ZXDH_PCIEID_PF_IDX_MASK) >> ZXDH_PCIEID_PF_IDX_OFFSET;
	uint16_t ep_idx = (src_pcieid & ZXDH_PCIEID_EP_IDX_MASK) >> ZXDH_PCIEID_EP_IDX_OFFSET;

	switch (dst) {
	/* msg to risc */
	case ZXDH_MSG_CHAN_END_RISC:
		lock_id = ZXDH_MULTIPLY_BY_8(ep_idx) + pf_idx;
		break;
	/* msg to pf/vf */
	case ZXDH_MSG_CHAN_END_VF:
	case ZXDH_MSG_CHAN_END_PF:
		lock_id = ZXDH_MULTIPLY_BY_8(ep_idx) + pf_idx +
				ZXDH_MULTIPLY_BY_8(1 + ZXDH_MAX_EP_NUM);
		break;
	default:
		lock_id = 0;
		break;
	}
	if (lock_id >= ZXDH_MAX_HARD_SPINLOCK_NUM)
		lock_id = 0;

	return lock_id;
}

static void
label_write(uint64_t label_lock_addr, uint32_t lock_id, uint16_t value)
{
	*(volatile uint16_t *)(label_lock_addr + lock_id * 2) = value;
}

static void
spinlock_write(uint64_t virt_lock_addr, uint32_t lock_id, uint8_t data)
{
	*(volatile uint8_t *)((uint64_t)virt_lock_addr + (uint64_t)lock_id) = data;
}

static int32_t
zxdh_spinlock_unlock(uint32_t virt_lock_id, uint64_t virt_addr, uint64_t label_addr)
{
	label_write((uint64_t)label_addr, virt_lock_id, 0);
	spinlock_write(virt_addr, virt_lock_id, 0);
	return 0;
}

/**
 * Fun: PF init hard_spinlock addr
 */
static int
bar_chan_pf_init_spinlock(uint16_t pcie_id, uint64_t bar_base_addr)
{
	int lock_id = zxdh_pcie_id_to_hard_lock(pcie_id, ZXDH_MSG_CHAN_END_RISC);

	zxdh_spinlock_unlock(lock_id, bar_base_addr + ZXDH_BAR0_SPINLOCK_OFFSET,
			bar_base_addr + ZXDH_HW_LABEL_OFFSET);
	lock_id = zxdh_pcie_id_to_hard_lock(pcie_id, ZXDH_MSG_CHAN_END_VF);
	zxdh_spinlock_unlock(lock_id, bar_base_addr + ZXDH_BAR0_SPINLOCK_OFFSET,
			bar_base_addr + ZXDH_HW_LABEL_OFFSET);
	return 0;
}

int
zxdh_msg_chan_hwlock_init(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;

	if (!hw->is_pf)
		return 0;
	return bar_chan_pf_init_spinlock(hw->pcie_id, (uint64_t)(hw->bar_addr[ZXDH_BAR0_INDEX]));
}

int
zxdh_msg_chan_init(void)
{
	uint16_t seq_id = 0;

	g_dev_stat.dev_cnt++;
	if (g_dev_stat.is_res_init)
		return ZXDH_BAR_MSG_OK;

	rte_spinlock_init(&chan_lock);
	g_seqid_ring.cur_id = 0;
	rte_spinlock_init(&g_seqid_ring.lock);

	for (seq_id = 0; seq_id < ZXDH_BAR_SEQID_NUM_MAX; seq_id++) {
		struct zxdh_seqid_item *reps_info = &g_seqid_ring.reps_info_tbl[seq_id];

		reps_info->id = seq_id;
		reps_info->flag = ZXDH_REPS_INFO_FLAG_USABLE;
	}
	g_dev_stat.is_res_init = true;
	return ZXDH_BAR_MSG_OK;
}

int
zxdh_bar_msg_chan_exit(void)
{
	if (!g_dev_stat.is_res_init || (--g_dev_stat.dev_cnt > 0))
		return ZXDH_BAR_MSG_OK;

	g_dev_stat.is_res_init = false;
	return ZXDH_BAR_MSG_OK;
}
