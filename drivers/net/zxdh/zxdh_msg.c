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
#include "rte_mtr_driver.h"

#include "zxdh_ethdev.h"
#include "zxdh_logs.h"
#include "zxdh_msg.h"
#include "zxdh_pci.h"
#include "zxdh_tables.h"
#include "zxdh_np.h"
#include "zxdh_common.h"

#define ZXDH_REPS_INFO_FLAG_USABLE  0x00
#define ZXDH_BAR_SEQID_NUM_MAX      256
#define ZXDH_REPS_INFO_FLAG_USED    0xa0

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

#define ZXDH_LOCK_PRIMARY_ID_MASK       (0x8000)
/* bar offset */
#define ZXDH_BAR0_CHAN_RISC_OFFSET      (0x2000)
#define ZXDH_BAR0_CHAN_PFVF_OFFSET      (0x3000)
#define ZXDH_BAR0_SPINLOCK_OFFSET       (0x4000)
#define ZXDH_FW_SHRD_OFFSET             (0x5000)
#define ZXDH_FW_SHRD_INNER_HW_LABEL_PAT (0x800)
#define ZXDH_HW_LABEL_OFFSET   \
	(ZXDH_FW_SHRD_OFFSET + ZXDH_FW_SHRD_INNER_HW_LABEL_PAT)

#define ZXDH_CHAN_RISC_SPINLOCK_OFFSET \
	(ZXDH_BAR0_SPINLOCK_OFFSET - ZXDH_BAR0_CHAN_RISC_OFFSET)
#define ZXDH_CHAN_PFVF_SPINLOCK_OFFSET \
	(ZXDH_BAR0_SPINLOCK_OFFSET - ZXDH_BAR0_CHAN_PFVF_OFFSET)
#define ZXDH_CHAN_RISC_LABEL_OFFSET    \
	(ZXDH_HW_LABEL_OFFSET - ZXDH_BAR0_CHAN_RISC_OFFSET)
#define ZXDH_CHAN_PFVF_LABEL_OFFSET    \
	(ZXDH_HW_LABEL_OFFSET - ZXDH_BAR0_CHAN_PFVF_OFFSET)

#define ZXDH_REPS_HEADER_LEN_OFFSET      1
#define ZXDH_REPS_HEADER_PAYLOAD_OFFSET  4
#define ZXDH_REPS_HEADER_REPLYED         0xff

#define ZXDH_BAR_MSG_CHAN_USABLE  0
#define ZXDH_BAR_MSG_CHAN_USED    1

#define ZXDH_BAR_MSG_POL_MASK     (0x10)
#define ZXDH_BAR_MSG_POL_OFFSET   (4)

#define ZXDH_BAR_ALIGN_WORD_MASK   0xfffffffc
#define ZXDH_BAR_MSG_VALID_MASK    1
#define ZXDH_BAR_MSG_VALID_OFFSET  0

#define ZXDH_BAR_PF_NUM             7
#define ZXDH_BAR_VF_NUM             256
#define ZXDH_BAR_INDEX_PF_TO_VF     0
#define ZXDH_BAR_INDEX_MPF_TO_MPF   0xff
#define ZXDH_BAR_INDEX_MPF_TO_PFVF  0
#define ZXDH_BAR_INDEX_PFVF_TO_MPF  0

#define ZXDH_MAX_HARD_SPINLOCK_ASK_TIMES  (1000)
#define ZXDH_SPINLOCK_POLLING_SPAN_US     (100)

#define ZXDH_BAR_MSG_SRC_NUM   3
#define ZXDH_BAR_MSG_SRC_MPF   0
#define ZXDH_BAR_MSG_SRC_PF    1
#define ZXDH_BAR_MSG_SRC_VF    2
#define ZXDH_BAR_MSG_SRC_ERR   0xff
#define ZXDH_BAR_MSG_DST_NUM   3
#define ZXDH_BAR_MSG_DST_RISC  0
#define ZXDH_BAR_MSG_DST_MPF   2
#define ZXDH_BAR_MSG_DST_PFVF  1
#define ZXDH_BAR_MSG_DST_ERR   0xff
#define ZXDH_BAR_INDEX_TO_RISC  0

#define ZXDH_BAR_CHAN_INDEX_SEND  0
#define ZXDH_BAR_CHAN_INDEX_RECV  1

#define ZXDH_BAR_CHAN_MSG_SYNC     0
#define ZXDH_BAR_CHAN_MSG_NO_EMEC  0
#define ZXDH_BAR_CHAN_MSG_EMEC     1
#define ZXDH_BAR_CHAN_MSG_NO_ACK   0
#define ZXDH_BAR_CHAN_MSG_ACK      1
#define ZXDH_MSG_REPS_OK           0xff

static uint8_t subchan_id_tbl[ZXDH_BAR_MSG_SRC_NUM][ZXDH_BAR_MSG_DST_NUM] = {
	{ZXDH_BAR_CHAN_INDEX_SEND, ZXDH_BAR_CHAN_INDEX_SEND, ZXDH_BAR_CHAN_INDEX_SEND},
	{ZXDH_BAR_CHAN_INDEX_SEND, ZXDH_BAR_CHAN_INDEX_SEND, ZXDH_BAR_CHAN_INDEX_RECV},
	{ZXDH_BAR_CHAN_INDEX_SEND, ZXDH_BAR_CHAN_INDEX_RECV, ZXDH_BAR_CHAN_INDEX_RECV}
};

static uint8_t chan_id_tbl[ZXDH_BAR_MSG_SRC_NUM][ZXDH_BAR_MSG_DST_NUM] = {
	{ZXDH_BAR_INDEX_TO_RISC, ZXDH_BAR_INDEX_MPF_TO_PFVF, ZXDH_BAR_INDEX_MPF_TO_MPF},
	{ZXDH_BAR_INDEX_TO_RISC, ZXDH_BAR_INDEX_PF_TO_VF,   ZXDH_BAR_INDEX_PFVF_TO_MPF},
	{ZXDH_BAR_INDEX_TO_RISC, ZXDH_BAR_INDEX_PF_TO_VF,   ZXDH_BAR_INDEX_PFVF_TO_MPF}
};

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
static uint8_t tmp_msg_header[ZXDH_BAR_MSG_ADDR_CHAN_INTERVAL];
static rte_spinlock_t chan_lock;

zxdh_bar_chan_msg_recv_callback zxdh_msg_recv_func_tbl[ZXDH_BAR_MSG_MODULE_NUM];

static inline const char
*zxdh_module_id_name(int val)
{
	switch (val) {
	case ZXDH_BAR_MODULE_DBG:        return "ZXDH_BAR_MODULE_DBG";
	case ZXDH_BAR_MODULE_TBL:        return "ZXDH_BAR_MODULE_TBL";
	case ZXDH_BAR_MODULE_MISX:       return "ZXDH_BAR_MODULE_MISX";
	case ZXDH_BAR_MODULE_SDA:        return "ZXDH_BAR_MODULE_SDA";
	case ZXDH_BAR_MODULE_RDMA:       return "ZXDH_BAR_MODULE_RDMA";
	case ZXDH_BAR_MODULE_DEMO:       return "ZXDH_BAR_MODULE_DEMO";
	case ZXDH_BAR_MODULE_SMMU:       return "ZXDH_BAR_MODULE_SMMU";
	case ZXDH_BAR_MODULE_MAC:        return "ZXDH_BAR_MODULE_MAC";
	case ZXDH_BAR_MODULE_VDPA:       return "ZXDH_BAR_MODULE_VDPA";
	case ZXDH_BAR_MODULE_VQM:        return "ZXDH_BAR_MODULE_VQM";
	case ZXDH_BAR_MODULE_NP:         return "ZXDH_BAR_MODULE_NP";
	case ZXDH_BAR_MODULE_VPORT:      return "ZXDH_BAR_MODULE_VPORT";
	case ZXDH_BAR_MODULE_BDF:        return "ZXDH_BAR_MODULE_BDF";
	case ZXDH_BAR_MODULE_RISC_READY: return "ZXDH_BAR_MODULE_RISC_READY";
	case ZXDH_BAR_MODULE_REVERSE:    return "ZXDH_BAR_MODULE_REVERSE";
	case ZXDH_BAR_MDOULE_NVME:       return "ZXDH_BAR_MDOULE_NVME";
	case ZXDH_BAR_MDOULE_NPSDK:      return "ZXDH_BAR_MDOULE_NPSDK";
	case ZXDH_BAR_MODULE_NP_TODO:    return "ZXDH_BAR_MODULE_NP_TODO";
	case ZXDH_MODULE_BAR_MSG_TO_PF:  return "ZXDH_MODULE_BAR_MSG_TO_PF";
	case ZXDH_MODULE_BAR_MSG_TO_VF:  return "ZXDH_MODULE_BAR_MSG_TO_VF";
	case ZXDH_MODULE_FLASH:          return "ZXDH_MODULE_FLASH";
	case ZXDH_BAR_MODULE_OFFSET_GET: return "ZXDH_BAR_MODULE_OFFSET_GET";
	case ZXDH_BAR_EVENT_OVS_WITH_VCB: return "ZXDH_BAR_EVENT_OVS_WITH_VCB";
	default: return "NA";
	}
}

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

static uint8_t
spinlock_read(uint64_t virt_lock_addr, uint32_t lock_id)
{
	return *(volatile uint8_t *)((uint64_t)virt_lock_addr + (uint64_t)lock_id);
}

static int32_t
zxdh_spinlock_lock(uint32_t virt_lock_id, uint64_t virt_addr,
		uint64_t label_addr, uint16_t primary_id)
{
	uint32_t lock_rd_cnt = 0;

	do {
		/* read to lock */
		uint8_t spl_val = spinlock_read(virt_addr, virt_lock_id);

		if (spl_val == 0) {
			label_write((uint64_t)label_addr, virt_lock_id, primary_id);
			break;
		}
		rte_delay_us_block(ZXDH_SPINLOCK_POLLING_SPAN_US);
		lock_rd_cnt++;
	} while (lock_rd_cnt < ZXDH_MAX_HARD_SPINLOCK_ASK_TIMES);
	if (lock_rd_cnt >= ZXDH_MAX_HARD_SPINLOCK_ASK_TIMES)
		return -1;

	return 0;
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

static int
zxdh_bar_chan_msgid_allocate(uint16_t *msgid)
{
	struct zxdh_seqid_item *seqid_reps_info = NULL;

	rte_spinlock_lock(&g_seqid_ring.lock);
	uint16_t g_id = g_seqid_ring.cur_id;
	uint16_t count = 0;
	int rc = 0;

	do {
		count++;
		++g_id;
		g_id %= ZXDH_BAR_SEQID_NUM_MAX;
		seqid_reps_info = &g_seqid_ring.reps_info_tbl[g_id];
	} while ((seqid_reps_info->flag != ZXDH_REPS_INFO_FLAG_USABLE) &&
		(count < ZXDH_BAR_SEQID_NUM_MAX));

	if (count >= ZXDH_BAR_SEQID_NUM_MAX) {
		rc = -1;
		goto out;
	}
	seqid_reps_info->flag = ZXDH_REPS_INFO_FLAG_USED;
	g_seqid_ring.cur_id = g_id;
	*msgid = g_id;
	rc = ZXDH_BAR_MSG_OK;

out:
	rte_spinlock_unlock(&g_seqid_ring.lock);
	return rc;
}

static uint16_t
zxdh_bar_chan_save_recv_info(struct zxdh_msg_recviver_mem *result, uint16_t *msg_id)
{
	int ret = zxdh_bar_chan_msgid_allocate(msg_id);

	if (ret != ZXDH_BAR_MSG_OK)
		return ZXDH_BAR_MSG_ERR_MSGID;

	PMD_MSG_LOG(DEBUG, "allocate msg_id: %u", *msg_id);
	struct zxdh_seqid_item *reps_info = &g_seqid_ring.reps_info_tbl[*msg_id];

	reps_info->reps_addr = result->recv_buffer;
	reps_info->buffer_len = result->buffer_len;
	return ZXDH_BAR_MSG_OK;
}

static uint8_t
zxdh_bar_msg_src_index_trans(uint8_t src)
{
	uint8_t src_index = 0;

	switch (src) {
	case ZXDH_MSG_CHAN_END_MPF:
		src_index = ZXDH_BAR_MSG_SRC_MPF;
		break;
	case ZXDH_MSG_CHAN_END_PF:
		src_index = ZXDH_BAR_MSG_SRC_PF;
		break;
	case ZXDH_MSG_CHAN_END_VF:
		src_index = ZXDH_BAR_MSG_SRC_VF;
		break;
	default:
		src_index = ZXDH_BAR_MSG_SRC_ERR;
		break;
	}
	return src_index;
}

static uint8_t
zxdh_bar_msg_dst_index_trans(uint8_t dst)
{
	uint8_t dst_index = 0;

	switch (dst) {
	case ZXDH_MSG_CHAN_END_MPF:
		dst_index = ZXDH_BAR_MSG_DST_MPF;
		break;
	case ZXDH_MSG_CHAN_END_PF:
		dst_index = ZXDH_BAR_MSG_DST_PFVF;
		break;
	case ZXDH_MSG_CHAN_END_VF:
		dst_index = ZXDH_BAR_MSG_DST_PFVF;
		break;
	case ZXDH_MSG_CHAN_END_RISC:
		dst_index = ZXDH_BAR_MSG_DST_RISC;
		break;
	default:
		dst_index = ZXDH_BAR_MSG_SRC_ERR;
		break;
	}
	return dst_index;
}

static int
zxdh_bar_chan_send_para_check(struct zxdh_pci_bar_msg *in,
		struct zxdh_msg_recviver_mem *result)
{
	uint8_t src_index = 0;
	uint8_t dst_index = 0;

	if (in == NULL || result == NULL) {
		PMD_MSG_LOG(ERR, "send para ERR: null para");
		return ZXDH_BAR_MSG_ERR_NULL_PARA;
	}
	src_index = zxdh_bar_msg_src_index_trans(in->src);
	dst_index = zxdh_bar_msg_dst_index_trans(in->dst);

	if (src_index == ZXDH_BAR_MSG_SRC_ERR || dst_index == ZXDH_BAR_MSG_DST_ERR) {
		PMD_MSG_LOG(ERR, "send para ERR: chan doesn't exist");
		return ZXDH_BAR_MSG_ERR_TYPE;
	}
	if (in->module_id >= ZXDH_BAR_MSG_MODULE_NUM) {
		PMD_MSG_LOG(ERR, "send para ERR: invalid module_id: %d", in->module_id);
		return ZXDH_BAR_MSG_ERR_MODULE;
	}
	if (in->payload_addr == NULL) {
		PMD_MSG_LOG(ERR, "send para ERR: null message");
		return ZXDH_BAR_MSG_ERR_BODY_NULL;
	}
	if (in->payload_len > ZXDH_BAR_MSG_PAYLOAD_MAX_LEN) {
		PMD_MSG_LOG(ERR, "send para ERR: len %d is too long", in->payload_len);
		return ZXDH_BAR_MSG_ERR_LEN;
	}
	if (in->virt_addr == 0 || result->recv_buffer == NULL) {
		PMD_MSG_LOG(ERR, "send para ERR: virt_addr or recv_buffer is NULL");
		return ZXDH_BAR_MSG_ERR_VIRTADDR_NULL;
	}
	if (result->buffer_len < ZXDH_REPS_HEADER_PAYLOAD_OFFSET)
		PMD_MSG_LOG(ERR, "recv buffer len is short than minimal 4 bytes");

	return ZXDH_BAR_MSG_OK;
}

static uint64_t
zxdh_subchan_addr_cal(uint64_t virt_addr, uint8_t chan_id, uint8_t subchan_id)
{
	return virt_addr + (2 * chan_id + subchan_id) * ZXDH_BAR_MSG_ADDR_CHAN_INTERVAL;
}

static uint16_t
zxdh_bar_chan_subchan_addr_get(struct zxdh_pci_bar_msg *in, uint64_t *subchan_addr)
{
	uint8_t src_index = zxdh_bar_msg_src_index_trans(in->src);
	uint8_t dst_index = zxdh_bar_msg_dst_index_trans(in->dst);
	uint16_t chan_id = chan_id_tbl[src_index][dst_index];
	uint16_t subchan_id = subchan_id_tbl[src_index][dst_index];

	*subchan_addr = zxdh_subchan_addr_cal(in->virt_addr, chan_id, subchan_id);
	return ZXDH_BAR_MSG_OK;
}

static int
zxdh_bar_hard_lock(uint16_t src_pcieid, uint8_t dst, uint64_t virt_addr)
{
	int ret = 0;
	uint16_t lockid = zxdh_pcie_id_to_hard_lock(src_pcieid, dst);

	PMD_MSG_LOG(DEBUG, "dev pcieid: 0x%x lock, get hardlockid: %u", src_pcieid, lockid);
	if (dst == ZXDH_MSG_CHAN_END_RISC)
		ret = zxdh_spinlock_lock(lockid, virt_addr + ZXDH_CHAN_RISC_SPINLOCK_OFFSET,
					virt_addr + ZXDH_CHAN_RISC_LABEL_OFFSET,
					src_pcieid | ZXDH_LOCK_PRIMARY_ID_MASK);
	else
		ret = zxdh_spinlock_lock(lockid, virt_addr + ZXDH_CHAN_PFVF_SPINLOCK_OFFSET,
					virt_addr + ZXDH_CHAN_PFVF_LABEL_OFFSET,
					src_pcieid | ZXDH_LOCK_PRIMARY_ID_MASK);

	return ret;
}

static void
zxdh_bar_hard_unlock(uint16_t src_pcieid, uint8_t dst, uint64_t virt_addr)
{
	uint16_t lockid = zxdh_pcie_id_to_hard_lock(src_pcieid, dst);

	PMD_MSG_LOG(DEBUG, "dev pcieid: 0x%x unlock, get hardlockid: %u", src_pcieid, lockid);
	if (dst == ZXDH_MSG_CHAN_END_RISC)
		zxdh_spinlock_unlock(lockid, virt_addr + ZXDH_CHAN_RISC_SPINLOCK_OFFSET,
				virt_addr + ZXDH_CHAN_RISC_LABEL_OFFSET);
	else
		zxdh_spinlock_unlock(lockid, virt_addr + ZXDH_CHAN_PFVF_SPINLOCK_OFFSET,
				virt_addr + ZXDH_CHAN_PFVF_LABEL_OFFSET);
}

static int
zxdh_bar_chan_lock(uint8_t src, uint8_t dst, uint16_t src_pcieid, uint64_t virt_addr)
{
	int ret = 0;
	uint8_t src_index = zxdh_bar_msg_src_index_trans(src);
	uint8_t dst_index = zxdh_bar_msg_dst_index_trans(dst);

	if (src_index == ZXDH_BAR_MSG_SRC_ERR || dst_index == ZXDH_BAR_MSG_DST_ERR) {
		PMD_MSG_LOG(ERR, "lock ERR: chan doesn't exist");
		return ZXDH_BAR_MSG_ERR_TYPE;
	}

	ret = zxdh_bar_hard_lock(src_pcieid, dst, virt_addr);
	if (ret != 0)
		PMD_MSG_LOG(ERR, "dev: 0x%x failed to lock", src_pcieid);

	return ret;
}

static int
zxdh_bar_chan_unlock(uint8_t src, uint8_t dst, uint16_t src_pcieid, uint64_t virt_addr)
{
	uint8_t src_index = zxdh_bar_msg_src_index_trans(src);
	uint8_t dst_index = zxdh_bar_msg_dst_index_trans(dst);

	if (src_index == ZXDH_BAR_MSG_SRC_ERR || dst_index == ZXDH_BAR_MSG_DST_ERR) {
		PMD_MSG_LOG(ERR, "unlock ERR: chan doesn't exist");
		return ZXDH_BAR_MSG_ERR_TYPE;
	}

	zxdh_bar_hard_unlock(src_pcieid, dst, virt_addr);

	return ZXDH_BAR_MSG_OK;
}

static void
zxdh_bar_chan_msgid_free(uint16_t msg_id)
{
	struct zxdh_seqid_item *seqid_reps_info = &g_seqid_ring.reps_info_tbl[msg_id];

	rte_spinlock_lock(&g_seqid_ring.lock);
	seqid_reps_info->flag = ZXDH_REPS_INFO_FLAG_USABLE;
	PMD_MSG_LOG(DEBUG, "free msg_id: %u", msg_id);
	rte_spinlock_unlock(&g_seqid_ring.lock);
}

static int
zxdh_bar_chan_reg_write(uint64_t subchan_addr, uint32_t offset, uint32_t data)
{
	uint32_t algin_offset = (offset & ZXDH_BAR_ALIGN_WORD_MASK);

	if (unlikely(algin_offset >= ZXDH_BAR_MSG_ADDR_CHAN_INTERVAL)) {
		PMD_MSG_LOG(ERR, "algin_offset exceeds channel size!");
		return -1;
	}
	*(uint32_t *)(subchan_addr + algin_offset) = data;
	return 0;
}

static int
zxdh_bar_chan_reg_read(uint64_t subchan_addr, uint32_t offset, uint32_t *pdata)
{
	uint32_t algin_offset = (offset & ZXDH_BAR_ALIGN_WORD_MASK);

	if (unlikely(algin_offset >= ZXDH_BAR_MSG_ADDR_CHAN_INTERVAL)) {
		PMD_MSG_LOG(ERR, "algin_offset exceeds channel size!");
		return -1;
	}
	*pdata = *(uint32_t *)(subchan_addr + algin_offset);
	return 0;
}

static uint16_t
zxdh_bar_chan_msg_header_set(uint64_t subchan_addr,
		struct zxdh_bar_msg_header *msg_header)
{
	uint32_t *data = (uint32_t *)msg_header;
	uint16_t i;

	for (i = 0; i < (ZXDH_BAR_MSG_PLAYLOAD_OFFSET >> 2); i++)
		zxdh_bar_chan_reg_write(subchan_addr, i * 4, *(data + i));

	return ZXDH_BAR_MSG_OK;
}

static uint16_t
zxdh_bar_chan_msg_header_get(uint64_t subchan_addr,
		struct zxdh_bar_msg_header *msg_header)
{
	uint32_t *data = (uint32_t *)msg_header;
	uint16_t i;

	for (i = 0; i < (ZXDH_BAR_MSG_PLAYLOAD_OFFSET >> 2); i++)
		zxdh_bar_chan_reg_read(subchan_addr, i * 4, data + i);

	return ZXDH_BAR_MSG_OK;
}

static uint16_t
zxdh_bar_chan_msg_payload_set(uint64_t subchan_addr, uint8_t *msg, uint16_t len)
{
	uint32_t *data = (uint32_t *)msg;
	uint32_t count = (len >> 2);
	uint32_t remain = (len & 0x3);
	uint32_t remain_data = 0;
	uint32_t i;

	for (i = 0; i < count; i++)
		zxdh_bar_chan_reg_write(subchan_addr, 4 * i +
				ZXDH_BAR_MSG_PLAYLOAD_OFFSET, *(data + i));
	if (remain) {
		for (i = 0; i < remain; i++)
			remain_data |= *((uint8_t *)(msg + len - remain + i)) << (8 * i);

		zxdh_bar_chan_reg_write(subchan_addr, 4 * count +
				ZXDH_BAR_MSG_PLAYLOAD_OFFSET, remain_data);
	}
	return ZXDH_BAR_MSG_OK;
}

static uint16_t
zxdh_bar_chan_msg_payload_get(uint64_t subchan_addr, uint8_t *msg, uint16_t len)
{
	uint32_t *data = (uint32_t *)msg;
	uint32_t count = (len >> 2);
	uint32_t remain_data = 0;
	uint32_t remain = (len & 0x3);
	uint32_t i;

	for (i = 0; i < count; i++)
		zxdh_bar_chan_reg_read(subchan_addr, 4 * i +
			ZXDH_BAR_MSG_PLAYLOAD_OFFSET, (data + i));
	if (remain) {
		zxdh_bar_chan_reg_read(subchan_addr, 4 * count +
				ZXDH_BAR_MSG_PLAYLOAD_OFFSET, &remain_data);
		for (i = 0; i < remain; i++)
			*((uint8_t *)(msg + (len - remain + i))) = remain_data >> (8 * i);
	}
	return ZXDH_BAR_MSG_OK;
}

static uint16_t
zxdh_bar_chan_msg_valid_set(uint64_t subchan_addr, uint8_t valid_label)
{
	uint32_t data;

	zxdh_bar_chan_reg_read(subchan_addr, ZXDH_BAR_MSG_VALID_OFFSET, &data);
	data &= (~ZXDH_BAR_MSG_VALID_MASK);
	data |= (uint32_t)valid_label;
	zxdh_bar_chan_reg_write(subchan_addr, ZXDH_BAR_MSG_VALID_OFFSET, data);
	return ZXDH_BAR_MSG_OK;
}

static uint16_t
zxdh_bar_chan_msg_send(uint64_t subchan_addr, void *payload_addr,
		uint16_t payload_len, struct zxdh_bar_msg_header *msg_header)
{
	uint16_t ret = 0;
	ret = zxdh_bar_chan_msg_header_set(subchan_addr, msg_header);

	ret = zxdh_bar_chan_msg_header_get(subchan_addr,
				(struct zxdh_bar_msg_header *)tmp_msg_header);

	ret = zxdh_bar_chan_msg_payload_set(subchan_addr,
				(uint8_t *)(payload_addr), payload_len);

	ret = zxdh_bar_chan_msg_payload_get(subchan_addr,
				tmp_msg_header, payload_len);

	ret = zxdh_bar_chan_msg_valid_set(subchan_addr, ZXDH_BAR_MSG_CHAN_USED);
	return ret;
}

static uint16_t
zxdh_bar_msg_valid_stat_get(uint64_t subchan_addr)
{
	uint32_t data;

	zxdh_bar_chan_reg_read(subchan_addr, ZXDH_BAR_MSG_VALID_OFFSET, &data);
	if (ZXDH_BAR_MSG_CHAN_USABLE == (data & ZXDH_BAR_MSG_VALID_MASK))
		return ZXDH_BAR_MSG_CHAN_USABLE;

	return ZXDH_BAR_MSG_CHAN_USED;
}

static uint16_t
zxdh_bar_chan_msg_poltag_set(uint64_t subchan_addr, uint8_t label)
{
	uint32_t data;

	zxdh_bar_chan_reg_read(subchan_addr, ZXDH_BAR_MSG_VALID_OFFSET, &data);
	data &= (~(uint32_t)ZXDH_BAR_MSG_POL_MASK);
	data |= ((uint32_t)label << ZXDH_BAR_MSG_POL_OFFSET);
	zxdh_bar_chan_reg_write(subchan_addr, ZXDH_BAR_MSG_VALID_OFFSET, data);
	return ZXDH_BAR_MSG_OK;
}

static uint16_t
zxdh_bar_chan_sync_msg_reps_get(uint64_t subchan_addr,
		uint64_t recv_buffer, uint16_t buffer_len)
{
	struct zxdh_bar_msg_header msg_header;
	uint16_t msg_id = 0;
	uint16_t msg_len = 0;

	zxdh_bar_chan_msg_header_get(subchan_addr, &msg_header);
	msg_id = msg_header.msg_id;
	struct zxdh_seqid_item *reps_info = &g_seqid_ring.reps_info_tbl[msg_id];

	if (reps_info->flag != ZXDH_REPS_INFO_FLAG_USED) {
		PMD_MSG_LOG(ERR, "msg_id %u unused", msg_id);
		return ZXDH_BAR_MSG_ERR_REPLY;
	}
	msg_len = msg_header.len;

	if (msg_len > buffer_len - 4) {
		PMD_MSG_LOG(ERR, "recv buffer len is: %u, but reply msg len is: %u",
				buffer_len, msg_len + 4);
		return ZXDH_BAR_MSG_ERR_REPSBUFF_LEN;
	}
	uint8_t *recv_msg = (uint8_t *)recv_buffer;

	zxdh_bar_chan_msg_payload_get(subchan_addr,
			recv_msg + ZXDH_REPS_HEADER_PAYLOAD_OFFSET, msg_len);
	*(uint16_t *)(recv_msg + ZXDH_REPS_HEADER_LEN_OFFSET) = msg_len;
	*recv_msg = ZXDH_REPS_HEADER_REPLYED; /* set reps's valid */
	return ZXDH_BAR_MSG_OK;
}

int
zxdh_bar_chan_sync_msg_send(struct zxdh_pci_bar_msg *in, struct zxdh_msg_recviver_mem *result)
{
	struct zxdh_bar_msg_header msg_header = {0};
	uint16_t seq_id = 0;
	uint64_t subchan_addr = 0;
	uint32_t time_out_cnt = 0;
	uint16_t valid = 0;
	int ret = 0;

	ret = zxdh_bar_chan_send_para_check(in, result);
	if (ret != ZXDH_BAR_MSG_OK)
		goto exit;

	ret = zxdh_bar_chan_save_recv_info(result, &seq_id);
	if (ret != ZXDH_BAR_MSG_OK)
		goto exit;

	zxdh_bar_chan_subchan_addr_get(in, &subchan_addr);

	msg_header.sync = ZXDH_BAR_CHAN_MSG_SYNC;
	msg_header.emec = in->emec;
	msg_header.usr  = 0;
	msg_header.rsv  = 0;
	msg_header.module_id  = in->module_id;
	msg_header.len        = in->payload_len;
	msg_header.msg_id     = seq_id;
	msg_header.src_pcieid = in->src_pcieid;
	msg_header.dst_pcieid = in->dst_pcieid;

	ret = zxdh_bar_chan_lock(in->src, in->dst, in->src_pcieid, in->virt_addr);
	if (ret != ZXDH_BAR_MSG_OK) {
		zxdh_bar_chan_msgid_free(seq_id);
		goto exit;
	}
	zxdh_bar_chan_msg_send(subchan_addr, in->payload_addr, in->payload_len, &msg_header);

	do {
		rte_delay_us_block(ZXDH_BAR_MSG_POLLING_SPAN);
		valid = zxdh_bar_msg_valid_stat_get(subchan_addr);
		++time_out_cnt;
	} while ((time_out_cnt < ZXDH_BAR_MSG_TIMEOUT_TH) && (valid == ZXDH_BAR_MSG_CHAN_USED));

	if (time_out_cnt == ZXDH_BAR_MSG_TIMEOUT_TH && valid != ZXDH_BAR_MSG_CHAN_USABLE) {
		zxdh_bar_chan_msg_valid_set(subchan_addr, ZXDH_BAR_MSG_CHAN_USABLE);
		zxdh_bar_chan_msg_poltag_set(subchan_addr, 0);
		PMD_MSG_LOG(ERR, "BAR MSG ERR: chan type time out");
		ret = ZXDH_BAR_MSG_ERR_TIME_OUT;
	} else {
		ret = zxdh_bar_chan_sync_msg_reps_get(subchan_addr,
					(uint64_t)result->recv_buffer, result->buffer_len);
	}
	zxdh_bar_chan_msgid_free(seq_id);
	zxdh_bar_chan_unlock(in->src, in->dst, in->src_pcieid, in->virt_addr);

exit:
	return ret;
}

static int
zxdh_bar_get_sum(uint8_t *ptr, uint8_t len)
{
	uint64_t sum = 0;
	int idx;

	for (idx = 0; idx < len; idx++)
		sum += *(ptr + idx);

	return (uint16_t)sum;
}

static int
zxdh_bar_chan_enable(struct zxdh_msix_para *para, uint16_t *vport)
{
	struct zxdh_bar_recv_msg recv_msg = {0};
	int ret = 0;
	int check_token = 0;
	int sum_res = 0;

	if (!para)
		return ZXDH_BAR_MSG_ERR_NULL;

	struct zxdh_msix_msg msix_msg = {
		.pcie_id = para->pcie_id,
		.vector_risc = para->vector_risc,
		.vector_pfvf = para->vector_pfvf,
		.vector_mpf = para->vector_mpf,
	};
	struct zxdh_pci_bar_msg in = {
		.virt_addr = para->virt_addr,
		.payload_addr = &msix_msg,
		.payload_len = sizeof(msix_msg),
		.emec = 0,
		.src = para->driver_type,
		.dst = ZXDH_MSG_CHAN_END_RISC,
		.module_id = ZXDH_BAR_MODULE_MISX,
		.src_pcieid = para->pcie_id,
		.dst_pcieid = 0,
		.usr = 0,
	};

	struct zxdh_msg_recviver_mem result = {
		.recv_buffer = &recv_msg,
		.buffer_len = sizeof(recv_msg),
	};

	ret = zxdh_bar_chan_sync_msg_send(&in, &result);
	if (ret != ZXDH_BAR_MSG_OK)
		return -ret;

	check_token = recv_msg.msix_reps.check;
	sum_res = zxdh_bar_get_sum((uint8_t *)&msix_msg, sizeof(msix_msg));

	if (check_token != sum_res) {
		PMD_MSG_LOG(ERR, "expect token: 0x%x, get token: 0x%x", sum_res, check_token);
		return ZXDH_BAR_MSG_ERR_REPLY;
	}
	*vport = recv_msg.msix_reps.vport;
	PMD_MSG_LOG(DEBUG, "vport of pcieid: 0x%x get success", para->pcie_id);
	return ZXDH_BAR_MSG_OK;
}

int
zxdh_msg_chan_enable(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msix_para misx_info = {
		.vector_risc = ZXDH_MSIX_FROM_RISCV,
		.vector_pfvf = ZXDH_MSIX_FROM_PFVF,
		.vector_mpf  = ZXDH_MSIX_FROM_MPF,
		.pcie_id     = hw->pcie_id,
		.driver_type = hw->is_pf ? ZXDH_MSG_CHAN_END_PF : ZXDH_MSG_CHAN_END_VF,
		.virt_addr   = (uint64_t)(hw->bar_addr[ZXDH_BAR0_INDEX] + ZXDH_CTRLCH_OFFSET),
	};

	return zxdh_bar_chan_enable(&misx_info, &hw->vport.vport);
}

static uint64_t
zxdh_recv_addr_get(uint8_t src_type, uint8_t dst_type, uint64_t virt_addr)
{
	uint8_t chan_id = 0;
	uint8_t subchan_id = 0;
	uint8_t src = 0;
	uint8_t dst = 0;

	src = zxdh_bar_msg_dst_index_trans(src_type);
	dst = zxdh_bar_msg_src_index_trans(dst_type);
	if (src == ZXDH_BAR_MSG_SRC_ERR || dst == ZXDH_BAR_MSG_DST_ERR)
		return 0;

	chan_id = chan_id_tbl[dst][src];
	subchan_id = 1 - subchan_id_tbl[dst][src];

	return zxdh_subchan_addr_cal(virt_addr, chan_id, subchan_id);
}

static void
zxdh_bar_msg_ack_async_msg_proc(struct zxdh_bar_msg_header *msg_header,
		uint8_t *receiver_buff)
{
	struct zxdh_seqid_item *reps_info = &g_seqid_ring.reps_info_tbl[msg_header->msg_id];

	if (reps_info->flag != ZXDH_REPS_INFO_FLAG_USED) {
		PMD_MSG_LOG(ERR, "msg_id: %u is released", msg_header->msg_id);
		return;
	}
	if (msg_header->len > reps_info->buffer_len - 4) {
		PMD_MSG_LOG(ERR, "reps_buf_len is %u, but reps_msg_len is %u",
				reps_info->buffer_len, msg_header->len + 4);
		goto free_id;
	}
	uint8_t *reps_buffer = (uint8_t *)reps_info->reps_addr;

	rte_memcpy(reps_buffer + 4, receiver_buff, msg_header->len);
	*(uint16_t *)(reps_buffer + 1) = msg_header->len;
	*(uint8_t *)(reps_info->reps_addr) = ZXDH_REPS_HEADER_REPLYED;

free_id:
	zxdh_bar_chan_msgid_free(msg_header->msg_id);
}

static void
zxdh_bar_msg_sync_msg_proc(uint64_t reply_addr,
		struct zxdh_bar_msg_header *msg_header,
		uint8_t *receiver_buff, void *dev)
{
	uint16_t reps_len = 0;
	uint8_t *reps_buffer = NULL;

	reps_buffer = rte_malloc(NULL, ZXDH_BAR_MSG_PAYLOAD_MAX_LEN, 0);
	if (reps_buffer == NULL)
		return;

	zxdh_bar_chan_msg_recv_callback recv_func = zxdh_msg_recv_func_tbl[msg_header->module_id];

	recv_func(receiver_buff, msg_header->len, reps_buffer, &reps_len, dev);
	msg_header->ack = ZXDH_BAR_CHAN_MSG_ACK;
	msg_header->len = reps_len;
	zxdh_bar_chan_msg_header_set(reply_addr, msg_header);
	zxdh_bar_chan_msg_payload_set(reply_addr, reps_buffer, reps_len);
	zxdh_bar_chan_msg_valid_set(reply_addr, ZXDH_BAR_MSG_CHAN_USABLE);
	rte_free(reps_buffer);
}

static uint64_t
zxdh_reply_addr_get(uint8_t sync, uint8_t src_type,
		uint8_t dst_type, uint64_t virt_addr)
{
	uint64_t recv_rep_addr = 0;
	uint8_t chan_id = 0;
	uint8_t subchan_id = 0;
	uint8_t src = 0;
	uint8_t dst = 0;

	src = zxdh_bar_msg_dst_index_trans(src_type);
	dst = zxdh_bar_msg_src_index_trans(dst_type);
	if (src == ZXDH_BAR_MSG_SRC_ERR || dst == ZXDH_BAR_MSG_DST_ERR)
		return 0;

	chan_id = chan_id_tbl[dst][src];
	subchan_id = 1 - subchan_id_tbl[dst][src];

	if (sync == ZXDH_BAR_CHAN_MSG_SYNC)
		recv_rep_addr = zxdh_subchan_addr_cal(virt_addr, chan_id, subchan_id);
	else
		recv_rep_addr = zxdh_subchan_addr_cal(virt_addr, chan_id, 1 - subchan_id);

	return recv_rep_addr;
}

static uint16_t
zxdh_bar_chan_msg_header_check(struct zxdh_bar_msg_header *msg_header)
{
	uint16_t len = 0;
	uint8_t module_id = 0;

	if (msg_header->valid != ZXDH_BAR_MSG_CHAN_USED) {
		PMD_MSG_LOG(ERR, "recv header ERR: valid label is not used");
		return ZXDH_BAR_MSG_ERR_MODULE;
	}
	module_id = msg_header->module_id;

	if (module_id >= (uint8_t)ZXDH_BAR_MSG_MODULE_NUM) {
		PMD_MSG_LOG(ERR, "recv header ERR: invalid module_id: %u", module_id);
		return ZXDH_BAR_MSG_ERR_MODULE;
	}
	len = msg_header->len;

	if (len > ZXDH_BAR_MSG_PAYLOAD_MAX_LEN) {
		PMD_MSG_LOG(ERR, "recv header ERR: invalid mesg len: %u", len);
		return ZXDH_BAR_MSG_ERR_LEN;
	}
	if (zxdh_msg_recv_func_tbl[msg_header->module_id] == NULL) {
		PMD_MSG_LOG(ERR, "recv header ERR: module:%s(%u) doesn't register",
				zxdh_module_id_name(module_id), module_id);
		return ZXDH_BAR_MSG_ERR_MODULE_NOEXIST;
	}
	return ZXDH_BAR_MSG_OK;
}

int
zxdh_bar_irq_recv(uint8_t src, uint8_t dst, uint64_t virt_addr, void *dev)
{
	struct zxdh_bar_msg_header msg_header;
	uint64_t recv_addr = 0;
	uint64_t reps_addr = 0;
	uint16_t ret = 0;
	uint8_t *recved_msg = NULL;

	recv_addr = zxdh_recv_addr_get(src, dst, virt_addr);
	if (recv_addr == 0) {
		PMD_MSG_LOG(ERR, "invalid driver type(src:%u, dst:%u)", src, dst);
		return -1;
	}

	zxdh_bar_chan_msg_header_get(recv_addr, &msg_header);
	ret = zxdh_bar_chan_msg_header_check(&msg_header);

	if (ret != ZXDH_BAR_MSG_OK) {
		PMD_MSG_LOG(ERR, "recv msg_head err, ret: %u", ret);
		return -1;
	}

	recved_msg = rte_malloc(NULL, msg_header.len, 0);
	if (recved_msg == NULL) {
		PMD_MSG_LOG(ERR, "malloc temp buff failed");
		return -1;
	}
	zxdh_bar_chan_msg_payload_get(recv_addr, recved_msg, msg_header.len);

	reps_addr = zxdh_reply_addr_get(msg_header.sync, src, dst, virt_addr);

	if (msg_header.sync == ZXDH_BAR_CHAN_MSG_SYNC) {
		zxdh_bar_msg_sync_msg_proc(reps_addr, &msg_header, recved_msg, dev);
		goto exit;
	}
	zxdh_bar_chan_msg_valid_set(recv_addr, ZXDH_BAR_MSG_CHAN_USABLE);
	if (msg_header.ack == ZXDH_BAR_CHAN_MSG_ACK) {
		zxdh_bar_msg_ack_async_msg_proc(&msg_header, recved_msg);
		goto exit;
	}
	return 0;

exit:
	rte_free(recved_msg);
	return ZXDH_BAR_MSG_OK;
}

int
zxdh_get_bar_offset(struct zxdh_bar_offset_params *paras,
		struct zxdh_bar_offset_res *res)
{
	uint16_t check_token;
	uint16_t sum_res;
	int ret;

	if (!paras)
		return ZXDH_BAR_MSG_ERR_NULL;

	struct zxdh_offset_get_msg send_msg = {
		.pcie_id = paras->pcie_id,
		.type = paras->type,
	};
	struct zxdh_pci_bar_msg in = {
		.payload_addr = &send_msg,
		.payload_len = sizeof(send_msg),
		.virt_addr = paras->virt_addr,
		.src = ZXDH_MSG_CHAN_END_PF,
		.dst = ZXDH_MSG_CHAN_END_RISC,
		.module_id = ZXDH_BAR_MODULE_OFFSET_GET,
		.src_pcieid = paras->pcie_id,
	};
	struct zxdh_bar_recv_msg recv_msg = {0};
	struct zxdh_msg_recviver_mem result = {
		.recv_buffer = &recv_msg,
		.buffer_len = sizeof(recv_msg),
	};
	ret = zxdh_bar_chan_sync_msg_send(&in, &result);
	if (ret != ZXDH_BAR_MSG_OK)
		return -ret;

	check_token = recv_msg.offset_reps.check;
	sum_res = zxdh_bar_get_sum((uint8_t *)&send_msg, sizeof(send_msg));

	if (check_token != sum_res) {
		PMD_MSG_LOG(ERR, "expect token: 0x%x, get token: 0x%x", sum_res, check_token);
		return ZXDH_BAR_MSG_ERR_REPLY;
	}
	res->bar_offset = recv_msg.offset_reps.offset;
	res->bar_length = recv_msg.offset_reps.length;
	return ZXDH_BAR_MSG_OK;
}

int
zxdh_vf_send_msg_to_pf(struct rte_eth_dev *dev,  void *msg_req,
			uint16_t msg_req_len, void *reply, uint16_t reply_len)
{
	struct zxdh_hw *hw  = dev->data->dev_private;
	struct zxdh_msg_recviver_mem result = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};
	int ret = 0;

	if (reply) {
		RTE_ASSERT(reply_len < ZXDH_ST_SZ_BYTES(msg_reply_info));
		result.recv_buffer  = reply;
		result.buffer_len = reply_len;
	} else {
		result.recv_buffer = zxdh_msg_reply_info;
		result.buffer_len = ZXDH_ST_SZ_BYTES(msg_reply_info);
	}

	void *reply_head_addr = ZXDH_ADDR_OF(msg_reply_info, result.recv_buffer, reply_head);
	void *reply_body_addr = ZXDH_ADDR_OF(msg_reply_info, result.recv_buffer, reply_body);

	struct zxdh_pci_bar_msg in = {
		.virt_addr = (uint64_t)(hw->bar_addr[ZXDH_BAR0_INDEX] +
				ZXDH_MSG_CHAN_PFVFSHARE_OFFSET),
		.payload_addr = msg_req,
		.payload_len = msg_req_len,
		.src = ZXDH_MSG_CHAN_END_VF,
		.dst = ZXDH_MSG_CHAN_END_PF,
		.module_id = ZXDH_MODULE_BAR_MSG_TO_PF,
		.src_pcieid = hw->pcie_id,
		.dst_pcieid = ZXDH_PF_PCIE_ID(hw->pcie_id),
	};

	ret = zxdh_bar_chan_sync_msg_send(&in, &result);
	if (ret != ZXDH_BAR_MSG_OK) {
		PMD_MSG_LOG(ERR,
			"vf[%d] send bar msg to pf failed.ret %d", hw->vport.vfid, ret);
		return -1;
	}

	uint8_t flag = ZXDH_GET(msg_reply_head, reply_head_addr, flag);
	uint16_t reps_len = ZXDH_GET(msg_reply_head, reply_head_addr, reps_len);
	if (flag != ZXDH_MSG_REPS_OK) {
		PMD_MSG_LOG(ERR, "vf[%d] get pf reply failed: reply_head flag : 0x%x(0xff is OK).replylen %d",
				hw->vport.vfid, flag, reps_len);
		return -1;
	}
	uint8_t reply_body_flag = ZXDH_GET(msg_reply_body, reply_body_addr, flag);
	if (reply_body_flag != ZXDH_REPS_SUCC) {
		PMD_MSG_LOG(ERR, "vf[%d] msg processing failed", hw->vfid);
		return -1;
	}
	return 0;
}

int32_t
zxdh_send_msg_to_riscv(struct rte_eth_dev *dev, void *msg_req,
			uint16_t msg_req_len, void *reply, uint16_t reply_len,
			enum ZXDH_BAR_MODULE_ID module_id)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_msg_recviver_mem result = {0};
	uint8_t zxdh_msg_reply_info[ZXDH_ST_SZ_BYTES(msg_reply_info)] = {0};

	if (reply) {
		RTE_ASSERT(reply_len < ZXDH_ST_SZ_BYTES(msg_reply_info));
		result.recv_buffer  = reply;
		result.buffer_len = reply_len;
	} else {
		result.recv_buffer = zxdh_msg_reply_info;
		result.buffer_len = ZXDH_ST_SZ_BYTES(msg_reply_info);
	}

	struct zxdh_pci_bar_msg in = {
		.payload_addr = msg_req,
		.payload_len = msg_req_len,
		.virt_addr = (uint64_t)(hw->bar_addr[ZXDH_BAR0_INDEX] + ZXDH_CTRLCH_OFFSET),
		.src = hw->is_pf ? ZXDH_MSG_CHAN_END_PF : ZXDH_MSG_CHAN_END_VF,
		.dst = ZXDH_MSG_CHAN_END_RISC,
		.module_id = module_id,
		.src_pcieid = hw->pcie_id,
	};

	if (zxdh_bar_chan_sync_msg_send(&in, &result) != ZXDH_BAR_MSG_OK) {
		PMD_MSG_LOG(ERR, "Failed to send sync messages or receive response");
		return -1;
	}

	return 0;
}

void
zxdh_msg_head_build(struct zxdh_hw *hw, enum zxdh_msg_type type,
		struct zxdh_msg_info *msg_info)
{
	struct zxdh_msg_head *msghead = &msg_info->msg_head;

	msghead->msg_type = type;
	msghead->vport    = hw->vport.vport;
	msghead->vf_id    = hw->vport.vfid;
	msghead->pcieid   = hw->pcie_id;
}

void
zxdh_agent_msg_build(struct zxdh_hw *hw, enum zxdh_agent_msg_type type,
		struct zxdh_msg_info *msg_info)
{
	struct zxdh_agent_msg_head *agent_head = &msg_info->agent_msg_head;

	agent_head->msg_type = type;
	agent_head->panel_id = hw->panel_id;
	agent_head->phyport = hw->phyport;
	agent_head->vf_id = hw->vfid;
	agent_head->pcie_id = hw->pcie_id;
}

static int
zxdh_bar_chan_msg_recv_register(uint8_t module_id, zxdh_bar_chan_msg_recv_callback callback)
{
	if (module_id >= (uint16_t)ZXDH_BAR_MSG_MODULE_NUM) {
		PMD_MSG_LOG(ERR, "register ERR: invalid module_id: %u.", module_id);
		return ZXDH_BAR_MSG_ERR_MODULE;
	}
	if (callback == NULL) {
		PMD_MSG_LOG(ERR, "register %s(%u) error: null callback.",
					zxdh_module_id_name(module_id), module_id);
		return ZXDH_BAR_MEG_ERR_NULL_FUNC;
	}
	if (zxdh_msg_recv_func_tbl[module_id] != NULL) {
		PMD_MSG_LOG(DEBUG, "register warning, event:%s(%u) already be registered.",
					zxdh_module_id_name(module_id), module_id);
		return ZXDH_BAR_MSG_ERR_REPEAT_REGISTER;
	}
	zxdh_msg_recv_func_tbl[module_id] = callback;
	return ZXDH_BAR_MSG_OK;
}

static int
zxdh_vf_promisc_init(struct zxdh_hw *hw, union zxdh_virport_num vport)
{
	int16_t ret;

	ret = zxdh_dev_broadcast_set(hw, vport.vport, true);
	return ret;
}

static int
zxdh_vf_promisc_uninit(struct zxdh_hw *hw, union zxdh_virport_num vport)
{
	int16_t ret;

	ret = zxdh_dev_broadcast_set(hw, vport.vport, false);
	return ret;
}

static int
zxdh_vf_vlan_table_init(struct zxdh_hw *hw, uint16_t vport)
{
	int ret = 0;
	ret = zxdh_vlan_filter_table_init(hw, vport);
	if (ret) {
		PMD_DRV_LOG(ERR, "vf vlan filter table init failed, code:%d", ret);
		return -1;
	}

	ret = zxdh_port_vlan_table_init(hw, vport);
	if (ret) {
		PMD_DRV_LOG(ERR, "vf port vlan table init failed, code:%d", ret);
		return -1;
	}
	return ret;
}

static int
zxdh_vf_port_init(struct zxdh_hw *pf_hw, uint16_t vport, void *cfg_data,
		void *res_info, uint16_t *res_len)
{
	struct zxdh_port_attr_table port_attr = {0};
	union zxdh_virport_num port = {.vport = vport};
	struct zxdh_vf_init_msg *vf_init_msg = (struct zxdh_vf_init_msg *)cfg_data;
	int ret = 0;

	RTE_ASSERT(!cfg_data || !pf_hw || !res_info || !res_len);
	*res_len = ZXDH_MSG_REPLYBODY_HEAD;
	port_attr.hit_flag = 1;
	port_attr.is_vf = 1;
	port_attr.phy_port = pf_hw->phyport;
	port_attr.is_up = 1;
	port_attr.rss_enable = 0;
	port_attr.pf_vfid = pf_hw->vfid;
	port_attr.hash_search_index = pf_hw->hash_search_index;
	port_attr.port_base_qid = vf_init_msg->base_qid;

	ret = zxdh_set_port_attr(pf_hw, vport, &port_attr);
	if (ret) {
		PMD_DRV_LOG(ERR, "set vport attr failed, code:%d", ret);
		goto proc_end;
	}

	ret = zxdh_vf_promisc_init(pf_hw, port);
	if (ret) {
		PMD_DRV_LOG(ERR, "vf_promisc_table_init failed, code:%d", ret);
		goto proc_end;
	}

	ret = zxdh_vf_vlan_table_init(pf_hw, vport);
	if (ret) {
		PMD_DRV_LOG(ERR, "vf vlan table init failed, code:%d", ret);
		goto proc_end;
	}

	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_SUCC);
	*res_len = sizeof(uint8_t);

	return ret;
proc_end:
	*res_len = sizeof(uint8_t);
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_FAIL);
	return ret;
}

static int
zxdh_mac_clear(struct zxdh_hw *hw, union zxdh_virport_num vport)
{
	uint16_t vf_id = vport.vfid;
	int i;
	int ret = 0;

	for (i = 0; (i != ZXDH_MAX_MAC_ADDRS); ++i) {
		if (!rte_is_zero_ether_addr(&hw->vfinfo[vf_id].vf_mac[i])) {
			ret = zxdh_del_mac_table(hw, vport.vport,
					&hw->vfinfo[vf_id].vf_mac[i],
					hw->hash_search_index, 0, 0);
			if (ret) {
				PMD_DRV_LOG(ERR, "vf_del_mac_failed. code:%d", ret);
				return ret;
			}
			memset(&hw->vfinfo[vf_id].vf_mac[i], 0, sizeof(struct rte_ether_addr));
		}
	}
	return ret;
}

static int
zxdh_vf_port_uninit(struct zxdh_hw *pf_hw,
		uint16_t vport, void *cfg_data __rte_unused,
		void *res_info, uint16_t *res_len)
{
	char str[ZXDH_MSG_REPLY_BODY_MAX_LEN] = "uninit";
	struct zxdh_port_attr_table port_attr = {0};
	union zxdh_virport_num vport_num = {.vport = vport};
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, res_info, reply_data);
	int ret = 0;

	*res_len =  ZXDH_MSG_REPLYBODY_HEAD;
	RTE_ASSERT(!cfg_data || !pf_hw || !res_info || !res_len);

	ret = zxdh_delete_port_attr(pf_hw, vport, &port_attr);
	if (ret) {
		PMD_DRV_LOG(ERR, "write port_attr_eram failed, code:%d", ret);
		goto proc_end;
	}

	ret = zxdh_mac_clear(pf_hw, vport_num);
	if (ret) {
		PMD_DRV_LOG(ERR, "zxdh_mac_clear failed, code:%d", ret);
		goto proc_end;
	}

	ret = zxdh_vf_promisc_uninit(pf_hw, vport_num);
	if (ret) {
		PMD_DRV_LOG(ERR, "vf_promisc_table_uninit failed, code:%d", ret);
		goto proc_end;
	}

	*res_len += strlen(str);
	rte_memcpy(reply_data_addr, str, strlen(str) + 1);
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_SUCC);
	return ret;

proc_end:
	*res_len += strlen(str);
	rte_memcpy(reply_data_addr, str, strlen(str) + 1);
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_FAIL);
	return ret;
}

static int
zxdh_add_vf_mac_table(struct zxdh_hw *hw, uint16_t vport, void *cfg_data,
		void *reply_body, uint16_t *reply_len)
{
	char str[ZXDH_MSG_REPLY_BODY_MAX_LEN] = "add mac";
	union zxdh_virport_num port = {0};
	struct zxdh_mac_filter *mac_filter = (struct zxdh_mac_filter *)cfg_data;
	struct rte_ether_addr *addr = &mac_filter->mac;
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, reply_body, reply_data);
	void *mac_reply_msg_addr = ZXDH_ADDR_OF(msg_reply_body, reply_body, mac_reply_msg);
	port.vport = vport;
	uint16_t vf_id = port.vfid;
	int i = 0, ret = 0;

	for (i = 0; i < ZXDH_MAX_MAC_ADDRS; i++)
		if (rte_is_same_ether_addr(&hw->vfinfo[vf_id].vf_mac[i], addr))
			goto success;

	ret = zxdh_add_mac_table(hw, vport, addr, hw->hash_search_index, 0, 0);
	if (ret == -EADDRINUSE) {
		ZXDH_SET(mac_reply_msg, mac_reply_msg_addr, mac_flag, ZXDH_EEXIST_MAC_FLAG);
		PMD_DRV_LOG(ERR, "vf vport 0x%x set mac ret 0x%x failed. mac is in used.",
				port.vport, ret);
		goto failure;
	}
	if (ret) {
		sprintf(str, "[PF GET MSG FROM VF]--VF add mac failed. code:%d\n", ret);
		PMD_DRV_LOG(ERR, " %s", str);
		goto failure;
	}
	for (i = 0; i < ZXDH_MAX_MAC_ADDRS; i++) {
		if (rte_is_zero_ether_addr(&hw->vfinfo[vf_id].vf_mac[i])) {
			memcpy(&hw->vfinfo[vf_id].vf_mac[i], addr, 6);
			break;
		}
	}

success:
	sprintf(str, " vport 0x%x set mac ret 0x%x\n", port.vport, ret);
	*reply_len =  strlen(str) + ZXDH_MSG_REPLYBODY_HEAD;
	rte_memcpy(reply_data_addr, str, strlen(str) + 1);
	ZXDH_SET(msg_reply_body, reply_body, flag, ZXDH_REPS_SUCC);
	PMD_DRV_LOG(DEBUG, " reply len %d", *reply_len);
	return ret;

failure:
	*reply_len = strlen(str) + ZXDH_MSG_REPLYBODY_HEAD;
	ZXDH_SET(msg_reply_body, reply_body, flag, ZXDH_REPS_FAIL);
	return ret;
}

static int
zxdh_del_vf_mac_table(struct zxdh_hw *hw, uint16_t vport, void *cfg_data,
	void *res_info, uint16_t *res_len)
{
	struct zxdh_mac_filter *mac_filter = (struct zxdh_mac_filter *)cfg_data;
	union zxdh_virport_num  port = (union zxdh_virport_num)vport;
	char str[ZXDH_MSG_REPLY_BODY_MAX_LEN] = "del mac";
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, res_info, reply_data);
	uint16_t vf_id = port.vfid;
	int ret, i = 0;

	PMD_DRV_LOG(DEBUG, "[PF GET MSG FROM VF]--vf mac to del.");
	ret = zxdh_del_mac_table(hw, vport, &mac_filter->mac, hw->hash_search_index, 0, 0);
	if (ret == -EADDRINUSE)
		ret = 0;

	if (ret) {
		sprintf(str, "[PF GET MSG FROM VF]--VF del mac failed. code:%d\n", ret);
		PMD_DRV_LOG(ERR, "%s", str);
		goto proc_end;
	}

	for (i = 0; i < ZXDH_MAX_MAC_ADDRS; i++) {
		if (rte_is_same_ether_addr(&hw->vfinfo[vf_id].vf_mac[i], &mac_filter->mac))
			memset(&hw->vfinfo[vf_id].vf_mac[i], 0, sizeof(struct rte_ether_addr));
	}

	sprintf(str, "vport 0x%x del mac ret 0x%x\n", port.vport, ret);
	*res_len =  strlen(str) + ZXDH_MSG_REPLYBODY_HEAD;
	rte_memcpy(reply_data_addr, str, strlen(str) + 1);
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_SUCC);
	return ret;

proc_end:
	*res_len = strlen(str) + ZXDH_MSG_REPLYBODY_HEAD;
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_FAIL);
	return ret;
}

static int
zxdh_vf_promisc_set(struct zxdh_hw *hw, uint16_t vport, void *cfg_data,
		void *reply, uint16_t *res_len)
{
	struct zxdh_port_promisc_msg *promisc_msg = (struct zxdh_port_promisc_msg *)cfg_data;
	int ret = 0;

	RTE_ASSERT(!cfg_data || !hw || !reply || !res_len);

	if (promisc_msg->mode == ZXDH_PROMISC_MODE) {
		zxdh_dev_unicast_table_set(hw, vport, promisc_msg->value);
		if (promisc_msg->mc_follow == 1)
			ret = zxdh_dev_multicast_table_set(hw, vport, promisc_msg->value);
	} else if (promisc_msg->mode == ZXDH_ALLMULTI_MODE) {
		ret = zxdh_dev_multicast_table_set(hw, vport, promisc_msg->value);
	} else {
		PMD_DRV_LOG(ERR, "promisc_set_msg mode[%u] error", promisc_msg->mode);
		goto proc_end;
	}

	*res_len = sizeof(struct zxdh_port_attr_set_msg) + sizeof(uint8_t);
	ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_SUCC);

	return ret;

proc_end:
	*res_len = sizeof(struct zxdh_port_attr_set_msg) + sizeof(uint8_t);
	ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_FAIL);
	return ret;
}

static int
zxdh_vf_vlan_filter_table_process(struct zxdh_hw *hw, uint16_t vport, void *cfg_data,
		void *res_info, uint16_t *res_len, uint8_t enable)
{
	struct zxdh_vlan_filter *vlan_filter = cfg_data;
	uint16_t vlan_id =  vlan_filter->vlan_id;
	char str[ZXDH_MSG_REPLY_BODY_MAX_LEN] = "vlan filter table";
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, res_info, reply_data);
	int ret = 0;

	ret = zxdh_vlan_filter_table_set(hw, vport, vlan_id, enable);
	if (ret)
		sprintf(str, "vlan filter op-code[%d] vlan id:%d failed, code:%d\n",
			enable, vlan_id, ret);

	*res_len = strlen(str) + sizeof(uint8_t);

	memcpy(reply_data_addr, str, strlen(str) + 1);
	if (ret == 0)
		ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_SUCC);
	else
		ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_FAIL);
	return ret;
}

static int
zxdh_vf_vlan_filter_table_add(struct zxdh_hw *hw, uint16_t vport, void *cfg_data,
		void *res_info, uint16_t *res_len)
{
	return zxdh_vf_vlan_filter_table_process(hw, vport, cfg_data, res_info, res_len, 1);
}

static int
zxdh_vf_vlan_filter_table_del(struct zxdh_hw *hw, uint16_t vport, void *cfg_data,
		void *res_info, uint16_t *res_len)
{
	return zxdh_vf_vlan_filter_table_process(hw, vport, cfg_data, res_info, res_len, 0);
}

static int
zxdh_vf_set_vlan_filter(struct zxdh_hw *hw, uint16_t vport, void *cfg_data,
		void *reply, uint16_t *res_len)
{
	struct zxdh_vlan_filter_set *vlan_filter = cfg_data;
	union zxdh_virport_num port = (union zxdh_virport_num)vport;
	char str[ZXDH_MSG_REPLY_BODY_MAX_LEN] = "vlan filter";
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, reply, reply_data);
	int ret = 0;
	uint16_t vfid = port.vfid;

	ret = zxdh_set_vlan_filter(hw, vport, vlan_filter->enable);
	if (ret)
		sprintf(str, "[vfid:%d] vlan filter. set failed, ret:%d\n", vfid, ret);

	*res_len = strlen(str) + sizeof(uint8_t);
	if (ret == 0)
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_SUCC);
	else
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_FAIL);
	memcpy(reply_data_addr, str, strlen(str) + 1);
	return ret;
}

static int
zxdh_vf_set_vlan_offload(struct zxdh_hw *hw, uint16_t vport, void *cfg_data,
		void *reply, uint16_t *res_len)
{
	struct zxdh_vlan_offload *vlan_offload = cfg_data;
	union zxdh_virport_num port = (union zxdh_virport_num)vport;
	char str[ZXDH_MSG_REPLY_BODY_MAX_LEN] = "vlan offload";
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, reply, reply_data);
	int ret = 0;
	uint16_t vfid = port.vfid;

	PMD_DRV_LOG(DEBUG, "vfid:%d, type:%s, enable:%d",
		vfid, vlan_offload->type == ZXDH_VLAN_STRIP_TYPE ? "vlan-strip" : "qinq-strip",
			vlan_offload->enable);
	ret = zxdh_set_vlan_offload(hw, vport, vlan_offload->type, vlan_offload->enable);
	if (ret)
		sprintf(str, "[vfid:%d] vlan offload set failed, ret:%d\n", vfid, ret);

	*res_len = strlen(str) + sizeof(uint8_t);
	if (ret == 0)
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_SUCC);
	else
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_FAIL);
	memcpy(reply_data_addr, str, strlen(str) + 1);
	return ret;
}

static int
zxdh_vf_rss_hf_get(struct zxdh_hw *hw, uint16_t vport, void *cfg_data __rte_unused,
			void *reply, uint16_t *res_len)
{
	char str[ZXDH_MSG_REPLY_BODY_MAX_LEN] = "rss_hf";
	struct zxdh_port_attr_table vport_att = {0};
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, reply, reply_data);
	void *rss_hf_msg_addr = ZXDH_ADDR_OF(msg_reply_body, reply, rss_hf_msg);
	int ret = 0;

	ret = zxdh_get_port_attr(hw, vport, &vport_att);
	if (ret) {
		sprintf(str, "get rss hash factor failed, ret:%d\n", ret);
		PMD_DRV_LOG(ERR, "get rss hash factor failed.");
		goto proc_end;
	}

	ZXDH_SET(rss_hf, rss_hf_msg_addr, rss_hf, vport_att.rss_hash_factor);

proc_end:
	*res_len = strlen(str) + sizeof(uint8_t);
	if (ret == 0)
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_SUCC);
	else
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_FAIL);
	memcpy(reply_data_addr, str, strlen(str) + 1);
	return ret;
}

static int
zxdh_vf_rss_hf_set(struct zxdh_hw *hw, uint16_t vport, void *cfg_data,
			void *reply, uint16_t *res_len)
{
	char str[ZXDH_MSG_REPLY_BODY_MAX_LEN] = "rss_hf";
	struct zxdh_rss_hf *rss_hf = cfg_data;
	struct zxdh_port_attr_table vport_att = {0};
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, reply, reply_data);
	int ret = 0;

	ret = zxdh_get_port_attr(hw, vport, &vport_att);
	if (ret) {
		sprintf(str, "set rss hash factor (set vport tbl failed, hf is %d). ret:%d\n",
				rss_hf->rss_hf, ret);
		PMD_DRV_LOG(ERR, "rss enable hash factor set failed");
		goto proc_end;
	}

	vport_att.rss_hash_factor = rss_hf->rss_hf;
	ret = zxdh_set_port_attr(hw, vport, &vport_att);
	if (ret) {
		sprintf(str, "set rss hash factor (set vport tbl failed, hf is %d). ret:%d\n",
				rss_hf->rss_hf, ret);
		PMD_DRV_LOG(ERR, "rss config hf failed");
	}

proc_end:
	*res_len = strlen(str) + sizeof(uint8_t);
	if (ret == 0)
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_SUCC);
	else
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_FAIL);
	memcpy(reply_data_addr, str, strlen(str) + 1);
	return ret;
}

static int
zxdh_vf_rss_enable(struct zxdh_hw *hw, uint16_t vport, void *cfg_data,
			void *reply, uint16_t *res_len)
{
	char str[ZXDH_MSG_REPLY_BODY_MAX_LEN] = "rss_enable";
	struct zxdh_rss_enable *rss_enable = cfg_data;
	struct zxdh_port_attr_table vport_att = {0};
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, reply, reply_data);
	int ret = 0;

	ret = zxdh_get_port_attr(hw, vport, &vport_att);
	if (ret) {
		sprintf(str, "set rss enable (get vport tbl failed, rss_enable is %d). ret:%d\n",
				rss_enable->enable, ret);
		PMD_DRV_LOG(ERR, "rss enable set failed");
		goto proc_end;
	}

	vport_att.rss_enable = rss_enable->enable;
	ret = zxdh_set_port_attr(hw, vport, &vport_att);
	if (ret) {
		sprintf(str, "set rss enable (set vport tbl failed, rss_enable is %d). ret:%d\n",
				rss_enable->enable, ret);
		PMD_DRV_LOG(ERR, "rss enable set failed");
	}

proc_end:
	*res_len = strlen(str) + sizeof(uint8_t);
	if (ret == 0)
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_SUCC);
	else
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_FAIL);
	memcpy(reply_data_addr, str, strlen(str) + 1);
	return ret;
}

static int
zxdh_vf_rss_table_set(struct zxdh_hw *hw, uint16_t vport, void *cfg_data,
		void *reply, uint16_t *res_len)
{
	char str[ZXDH_MSG_REPLY_BODY_MAX_LEN] = "rss_table";
	struct zxdh_rss_reta *rss_reta = cfg_data;
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, reply, reply_data);
	int32_t ret = 0;

	ret = zxdh_rss_table_set(hw, vport, rss_reta);
	if (ret)
		sprintf(str, "set rss reta tbl failed, code:%d", ret);

	*res_len = strlen(str) + sizeof(uint8_t);
	if (ret == 0)
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_SUCC);
	else
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_FAIL);
	memcpy(reply_data_addr, str, strlen(str) + 1);
	return ret;
}

static int
zxdh_vf_rss_table_get(struct zxdh_hw *hw, uint16_t vport, void *cfg_data __rte_unused,
		void *reply, uint16_t *res_len)
{
	char str[ZXDH_MSG_REPLY_BODY_MAX_LEN] = "rss_table";
	void *rss_reta_msg_addr = ZXDH_ADDR_OF(msg_reply_body, reply, rss_reta_msg);
	struct zxdh_rss_reta *rss_reta = (struct zxdh_rss_reta *)rss_reta_msg_addr;
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, reply, reply_data);
	int ret = 0;

	ret = zxdh_rss_table_get(hw, vport, rss_reta);
	if (ret)
		sprintf(str, "set rss reta tbl failed, code:%d", ret);

	if (ret == 0) {
		*res_len = ZXDH_ST_SZ_BYTES(rss_reta) + sizeof(uint8_t);
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_SUCC);
	} else {
		*res_len = strlen(str) + sizeof(uint8_t);
		ZXDH_SET(msg_reply_body, reply, flag, ZXDH_REPS_FAIL);
		memcpy(reply_data_addr, str, strlen(str) + 1);
	}

	return ret;
}

static int
zxdh_vf_port_attr_set(struct zxdh_hw *pf_hw, uint16_t vport, void *cfg_data,
	void *res_info, uint16_t *res_len)
{
	RTE_ASSERT(!cfg_data || !pf_hw);
	if (res_info)
		*res_len = 0;
	struct zxdh_port_attr_set_msg *attr_msg = (struct zxdh_port_attr_set_msg *)cfg_data;
	union zxdh_virport_num port = {.vport = vport};
	struct zxdh_port_attr_table port_attr = {0};

	int ret = zxdh_get_port_attr(pf_hw, vport, &port_attr);
	if (ret) {
		PMD_DRV_LOG(ERR, "get vport 0x%x(%d) attr failed", vport, port.vfid);
		return ret;
	}
	switch (attr_msg->mode) {
	case ZXDH_PORT_BASE_QID_FLAG:
		port_attr.port_base_qid = attr_msg->value;
		break;
	case ZXDH_PORT_MTU_OFFLOAD_EN_OFF_FLAG:
		port_attr.mtu_enable = attr_msg->value;
		break;
	case ZXDH_PORT_MTU_FLAG:
		port_attr.mtu = attr_msg->value;
		break;
	case ZXDH_PORT_VPORT_IS_UP_FLAG:
		port_attr.is_up = attr_msg->value;
		break;
	case ZXDH_PORT_OUTER_IP_CHECKSUM_OFFLOAD_FLAG:
		port_attr.outer_ip_checksum_offload = attr_msg->value;
		break;
	case ZXDH_PORT_IP_CHKSUM_FLAG:
		port_attr.ip_checksum_offload = attr_msg->value;
		break;
	case ZXDH_PORT_TCP_UDP_CHKSUM_FLAG:
		port_attr.tcp_udp_checksum_offload = attr_msg->value;
		break;
	case ZXDH_PORT_ACCELERATOR_OFFLOAD_FLAG_FLAG:
		port_attr.accelerator_offload_flag = attr_msg->value;
		break;
	case ZXDH_PORT_LRO_OFFLOAD_FLAG:
		port_attr.lro_offload = attr_msg->value;
		break;
	case ZXDH_PORT_EGRESS_METER_EN_OFF_FLAG:
		port_attr.egress_meter_enable = attr_msg->value;
		break;
	case ZXDH_PORT_INGRESS_METER_EN_OFF_FLAG:
		port_attr.ingress_meter_mode = attr_msg->value;
		break;
	default:
		PMD_DRV_LOG(ERR, "unsupported attr 0x%x set", attr_msg->mode);
		return -1;
	}
	ret = zxdh_set_port_attr(pf_hw, vport, &port_attr);
	if (ret) {
		PMD_DRV_LOG(ERR, "set port attr failed. code:%d", ret);
		return ret;
	}
	return ret;
}

static int
zxdh_vf_np_stats_update(struct zxdh_hw *pf_hw, uint16_t vport,
		void *cfg_data, void *res_info,
		uint16_t *res_len)
{
	struct zxdh_np_stats_updata_msg *np_stats_query =
			 (struct zxdh_np_stats_updata_msg  *)cfg_data;
	union zxdh_virport_num vport_num = {.vport = vport};
	struct zxdh_hw_stats_data stats_data;
	uint32_t is_clr = np_stats_query->clear_mode;
	uint32_t idx = 0;
	int ret = 0;

	void *hw_stats_addr = ZXDH_ADDR_OF(msg_reply_body, res_info, hw_stats);
	void *tx_unicast_pkts_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, tx_unicast_pkts);
	void *rx_unicast_pkts_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, rx_unicast_pkts);
	void *tx_unicast_bytes_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, tx_unicast_bytes);
	void *rx_unicast_bytes_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, rx_unicast_bytes);
	void *tx_multicast_pkts_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, tx_multicast_pkts);
	void *rx_multicast_pkts_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, rx_multicast_pkts);
	void *tx_multicast_bytes_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, tx_multicast_bytes);
	void *rx_multicast_bytes_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, rx_multicast_bytes);
	void *tx_broadcast_pkts_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, tx_broadcast_pkts);
	void *tx_broadcast_bytes_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, tx_broadcast_bytes);
	void *rx_broadcast_pkts_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, rx_broadcast_pkts);
	void *rx_broadcast_bytes_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, rx_broadcast_bytes);
	void *tx_mtu_drop_pkts_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, tx_mtu_drop_pkts);
	void *tx_mtu_drop_bytes_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, tx_mtu_drop_bytes);
	void *rx_mtu_drop_pkts_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, rx_mtu_drop_pkts);
	void *rx_mtu_drop_bytes_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, rx_mtu_drop_bytes);
	void *tx_mtr_drop_pkts_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, tx_mtr_drop_pkts);
	void *tx_mtr_drop_bytes_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, tx_mtr_drop_bytes);
	void *rx_mtr_drop_pkts_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, rx_mtr_drop_pkts);
	void *rx_mtr_drop_bytes_addr =
		ZXDH_ADDR_OF(hw_np_stats, hw_stats_addr, rx_mtr_drop_bytes);
	if (!res_len || !res_info) {
		PMD_DRV_LOG(ERR, "get stat invalid inparams");
		return -1;
	}
	if (is_clr == 1) {
		ret = zxdh_hw_np_stats_pf_reset(pf_hw->eth_dev, zxdh_vport_to_vfid(vport_num));
		return ret;
	}
	idx = zxdh_vport_to_vfid(vport_num) + ZXDH_UNICAST_STATS_EGRESS_BASE;
	ret = zxdh_np_dtb_stats_get(pf_hw->dev_id, pf_hw->dev_sd->dtb_sd.queueid,
			0, idx, (uint32_t *)&stats_data);
	if (ret) {
		PMD_DRV_LOG(ERR, "get stats failed. code:%d", ret);
		return ret;
	}
	zxdh_data_hi_to_lo(tx_unicast_pkts_addr);
	zxdh_data_hi_to_lo(tx_unicast_bytes_addr);

	idx = zxdh_vport_to_vfid(vport_num) + ZXDH_UNICAST_STATS_INGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(pf_hw->dev_id, pf_hw->dev_sd->dtb_sd.queueid,
			0, idx, (uint32_t *)&stats_data);
	if (ret) {
		PMD_DRV_LOG(ERR, "get stats failed. code:%d", ret);
		return ret;
	}
	zxdh_data_hi_to_lo(rx_unicast_pkts_addr);
	zxdh_data_hi_to_lo(rx_unicast_bytes_addr);

	idx = zxdh_vport_to_vfid(vport_num) + ZXDH_MULTICAST_STATS_EGRESS_BASE;
	ret = zxdh_np_dtb_stats_get(pf_hw->dev_id, pf_hw->dev_sd->dtb_sd.queueid,
			0, idx, (uint32_t *)&stats_data);
	if (ret) {
		PMD_DRV_LOG(ERR, "get stats failed. code:%d", ret);
		return ret;
	}
	zxdh_data_hi_to_lo(tx_multicast_pkts_addr);
	zxdh_data_hi_to_lo(tx_multicast_bytes_addr);

	idx = zxdh_vport_to_vfid(vport_num) + ZXDH_MULTICAST_STATS_INGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(pf_hw->dev_id, pf_hw->dev_sd->dtb_sd.queueid,
			0, idx, (uint32_t *)&stats_data);
	if (ret) {
		PMD_DRV_LOG(ERR, "get stats failed. code:%d", ret);
		return ret;
	}
	zxdh_data_hi_to_lo(rx_multicast_pkts_addr);
	zxdh_data_hi_to_lo(rx_multicast_bytes_addr);

	idx = zxdh_vport_to_vfid(vport_num) + ZXDH_BROAD_STATS_EGRESS_BASE;
	ret = zxdh_np_dtb_stats_get(pf_hw->dev_id, pf_hw->dev_sd->dtb_sd.queueid,
			0, idx, (uint32_t *)&stats_data);
	if (ret) {
		PMD_DRV_LOG(ERR, "get stats failed. code:%d", ret);
		return ret;
	}
	zxdh_data_hi_to_lo(tx_broadcast_pkts_addr);
	zxdh_data_hi_to_lo(tx_broadcast_bytes_addr);

	idx = zxdh_vport_to_vfid(vport_num) + ZXDH_BROAD_STATS_INGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(pf_hw->dev_id, pf_hw->dev_sd->dtb_sd.queueid,
			0, idx, (uint32_t *)&stats_data);
	if (ret) {
		PMD_DRV_LOG(ERR, "get stats failed. code:%d", ret);
		return ret;
	}
	zxdh_data_hi_to_lo(rx_broadcast_pkts_addr);
	zxdh_data_hi_to_lo(rx_broadcast_bytes_addr);

	idx = zxdh_vport_to_vfid(vport_num) + ZXDH_MTU_STATS_EGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(pf_hw->dev_id, pf_hw->dev_sd->dtb_sd.queueid,
			1, idx, (uint32_t *)&stats_data);
	if (ret) {
		PMD_DRV_LOG(ERR, "get stats failed. code:%d", ret);
		return ret;
	}
	ZXDH_SET(hw_np_stats, hw_stats_addr, tx_mtu_drop_pkts, stats_data.n_pkts_dropped);
	ZXDH_SET(hw_np_stats, hw_stats_addr, tx_mtu_drop_bytes, stats_data.n_bytes_dropped);
	zxdh_data_hi_to_lo(tx_mtu_drop_pkts_addr);
	zxdh_data_hi_to_lo(tx_mtu_drop_bytes_addr);

	idx = zxdh_vport_to_vfid(vport_num) + ZXDH_MTU_STATS_INGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(pf_hw->dev_id, pf_hw->dev_sd->dtb_sd.queueid,
			1, idx, (uint32_t *)&stats_data);
	if (ret) {
		PMD_DRV_LOG(ERR, "get stats failed. code:%d", ret);
		return ret;
	}
	ZXDH_SET(hw_np_stats, hw_stats_addr, rx_mtu_drop_pkts, stats_data.n_pkts_dropped);
	ZXDH_SET(hw_np_stats, hw_stats_addr, rx_mtu_drop_bytes, stats_data.n_bytes_dropped);
	zxdh_data_hi_to_lo(rx_mtu_drop_pkts_addr);
	zxdh_data_hi_to_lo(rx_mtu_drop_bytes_addr);

	idx = zxdh_vport_to_vfid(vport_num) + ZXDH_MTR_STATS_EGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(pf_hw->dev_id, pf_hw->dev_sd->dtb_sd.queueid,
			1, idx, (uint32_t *)&stats_data);
	if (ret) {
		PMD_DRV_LOG(ERR, "get stats failed. code:%d", ret);
		return ret;
	}
	ZXDH_SET(hw_np_stats, hw_stats_addr, tx_mtr_drop_pkts, stats_data.n_pkts_dropped);
	ZXDH_SET(hw_np_stats, hw_stats_addr, tx_mtr_drop_bytes, stats_data.n_bytes_dropped);

	zxdh_data_hi_to_lo(tx_mtr_drop_pkts_addr);
	zxdh_data_hi_to_lo(tx_mtr_drop_bytes_addr);

	idx = zxdh_vport_to_vfid(vport_num) + ZXDH_MTR_STATS_INGRESS_BASE;
	memset(&stats_data, 0, sizeof(stats_data));
	ret = zxdh_np_dtb_stats_get(pf_hw->dev_id, pf_hw->dev_sd->dtb_sd.queueid,
			1, idx, (uint32_t *)&stats_data);
	if (ret) {
		PMD_DRV_LOG(ERR, "get stats failed. code:%d", ret);
		return ret;
	}
	ZXDH_SET(hw_np_stats, hw_stats_addr, rx_mtr_drop_pkts, stats_data.n_pkts_dropped);
	ZXDH_SET(hw_np_stats, hw_stats_addr, rx_mtr_drop_bytes, stats_data.n_bytes_dropped);

	zxdh_data_hi_to_lo(rx_mtr_drop_pkts_addr);
	zxdh_data_hi_to_lo(rx_mtr_drop_bytes_addr);
	*res_len = sizeof(struct zxdh_hw_np_stats);

	return 0;
}

static int
zxdh_vf_mtr_hw_stats_get(struct zxdh_hw *pf_hw,
	uint16_t vport, void *cfg_data,
	void *res_info,
	uint16_t *res_len)
{
	struct zxdh_mtr_stats_query  *zxdh_mtr_stats_query =
			(struct zxdh_mtr_stats_query  *)cfg_data;
	union zxdh_virport_num v_port = {.vport = vport};
	uint8_t *hw_mtr_stats_addr = ZXDH_ADDR_OF(msg_reply_body, res_info, hw_mtr_stats);
	int ret = 0;

	uint32_t stat_baseaddr = zxdh_mtr_stats_query->direction ==
				ZXDH_EGRESS ?
				ZXDH_MTR_STATS_EGRESS_BASE : ZXDH_MTR_STATS_INGRESS_BASE;
	uint32_t idx = zxdh_vport_to_vfid(v_port) + stat_baseaddr;

	if (!res_len || !res_info) {
		PMD_DRV_LOG(ERR, "get stat invalid in params");
		return -1;
	}
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_FAIL);
	ret = zxdh_np_dtb_stats_get(pf_hw->dev_id, pf_hw->dev_sd->dtb_sd.queueid,
				1, idx, (uint32_t *)hw_mtr_stats_addr);
	if (ret) {
		PMD_DRV_LOG(ERR, "get dir %d stats  failed", zxdh_mtr_stats_query->direction);
		return ret;
	}
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_SUCC);
	*res_len = sizeof(struct zxdh_hw_mtr_stats);
	return 0;
}

static int
zxdh_vf_mtr_hw_profile_add(struct zxdh_hw *pf_hw,
	uint16_t vport,
	void *cfg_data,
	void *res_info,
	uint16_t *res_len)
{
	if (!cfg_data || !res_len || !res_info) {
		PMD_DRV_LOG(ERR, " get profileid invalid inparams");
		return -1;
	}
	struct rte_mtr_error error = {0};
	int ret = 0;
	uint64_t hw_profile_id = HW_PROFILE_MAX;
	void *mtr_profile_info_addr = ZXDH_ADDR_OF(msg_reply_body, res_info, mtr_profile_info);

	struct zxdh_plcr_profile_add  *zxdh_plcr_profile_add =
		(struct zxdh_plcr_profile_add *)cfg_data;

	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_FAIL);

	*res_len = sizeof(struct zxdh_mtr_profile_info);
	ret = zxdh_hw_profile_alloc_direct(pf_hw->eth_dev,
		zxdh_plcr_profile_add->car_type,
		&hw_profile_id, &error);

	if (ret) {
		PMD_DRV_LOG(ERR, "pf 0x%x for vf 0x%x alloc hw profile failed",
			pf_hw->vport.vport,
			vport
		);
		return -1;
	}
	zxdh_hw_profile_ref(hw_profile_id);
	ZXDH_SET(mtr_profile_info, mtr_profile_info_addr, profile_id, hw_profile_id);
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_SUCC);

	return 0;
}

static int
zxdh_vf_mtr_hw_profile_del(struct zxdh_hw *pf_hw,
	uint16_t vport,
	void *cfg_data,
	void *res_info,
	uint16_t *res_len)
{
	if (!cfg_data || !res_len || !res_info) {
		PMD_DRV_LOG(ERR, " del profileid  invalid inparams");
		return -1;
	}

	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_FAIL);
	*res_len = 0;
	struct zxdh_plcr_profile_free *mtr_profile_free = (struct zxdh_plcr_profile_free *)cfg_data;
	uint64_t profile_id = mtr_profile_free->profile_id;
	struct rte_mtr_error error = {0};
	int ret;

	if (profile_id >= HW_PROFILE_MAX) {
		PMD_DRV_LOG(ERR, " del profileid  invalid inparams");
		return -rte_mtr_error_set(&error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_PROFILE_ID, NULL,
				"Meter offload del profile failed  profilie id invalid ");
	}

	ret = zxdh_hw_profile_unref(pf_hw->eth_dev, mtr_profile_free->car_type, profile_id, &error);
	if (ret) {
		PMD_DRV_LOG(ERR,
			" del  hw vport %d profile %d failed. code:%d",
			vport,
			mtr_profile_free->profile_id,
			ret
		);
		return -rte_mtr_error_set(&error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_PROFILE_ID, NULL,
				"Meter offload del profile failed ");
	}
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_SUCC);
	return 0;
}

static int
zxdh_vf_mtr_hw_plcrflow_cfg(struct zxdh_hw *pf_hw,
	uint16_t vport,
	void *cfg_data,
	void *res_info,
	uint16_t *res_len)
{
	int ret = 0;

	if (!cfg_data || !res_info || !res_len) {
		PMD_DRV_LOG(ERR, " (vport %d) flow bind failed invalid inparams", vport);
		return -1;
	}
	struct rte_mtr_error error = {0};
	struct zxdh_plcr_flow_cfg *zxdh_plcr_flow_cfg = (struct zxdh_plcr_flow_cfg *)cfg_data;

	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_FAIL);
	*res_len = 0;
	ret = zxdh_np_stat_car_queue_cfg_set(pf_hw->dev_id,
		zxdh_plcr_flow_cfg->car_type,
		zxdh_plcr_flow_cfg->flow_id,
		zxdh_plcr_flow_cfg->drop_flag,
		zxdh_plcr_flow_cfg->plcr_en,
		(uint64_t)zxdh_plcr_flow_cfg->profile_id);
	if (ret) {
		PMD_DRV_LOG(ERR,
			" dpp_stat_car_queue_cfg_set failed flowid %d	profile id %d. code:%d",
			zxdh_plcr_flow_cfg->flow_id,
			zxdh_plcr_flow_cfg->profile_id,
			ret
		);
		return -rte_mtr_error_set(&error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_MTR_PARAMS,
				NULL, "Failed to bind plcr flow.");
	}
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_SUCC);
	return 0;
}

static int
zxdh_vf_mtr_hw_profile_cfg(struct zxdh_hw *pf_hw __rte_unused,
	uint16_t vport,
	void *cfg_data,
	void *res_info,
	uint16_t *res_len)
{
	int ret = 0;

	if (!cfg_data || !res_info || !res_len) {
		PMD_DRV_LOG(ERR, " cfg profile invalid inparams");
		return -1;
	}
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_FAIL);
	*res_len = 0;
	struct rte_mtr_error error = {0};
	struct zxdh_plcr_profile_cfg *zxdh_plcr_profile_cfg =
		(struct zxdh_plcr_profile_cfg *)cfg_data;
	union zxdh_offload_profile_cfg *plcr_param = &zxdh_plcr_profile_cfg->plcr_param;

	ret = zxdh_np_car_profile_cfg_set(pf_hw->dev_id,
		vport,
		zxdh_plcr_profile_cfg->car_type,
		zxdh_plcr_profile_cfg->packet_mode,
		zxdh_plcr_profile_cfg->hw_profile_id,
		plcr_param);
	if (ret) {
		PMD_DRV_LOG(ERR, "(vport %d)config hw profilefailed", vport);
		return -rte_mtr_error_set(&error, ENOTSUP, RTE_MTR_ERROR_TYPE_METER_PROFILE, NULL, "Meter offload cfg profile failed");
	}
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_SUCC);
	return 0;
}

static int
zxdh_vf_vlan_tpid_set(struct zxdh_hw *pf_hw, uint16_t vport, void *cfg_data,
		void *res_info, uint16_t *res_len)
{
	struct zxdh_vlan_tpid *vlan_tpid = (struct zxdh_vlan_tpid *)cfg_data;
	struct zxdh_port_vlan_table port_vlan_table = {0};
	int ret = 0;

	RTE_ASSERT(!cfg_data || !pf_hw || !res_info || !res_len);

	ret = zxdh_get_port_vlan_attr(pf_hw, vport, &port_vlan_table);
	if (ret) {
		PMD_DRV_LOG(ERR, "get port vlan attr failed, code:%d", ret);
		goto proc_end;
	}
	port_vlan_table.hit_flag = 1;
	port_vlan_table.business_vlan_tpid = vlan_tpid->tpid;
	ret = zxdh_set_port_vlan_attr(pf_hw, vport, &port_vlan_table);
	if (ret) {
		PMD_DRV_LOG(ERR, "set port vlan attr failed, code:%d", ret);
		goto proc_end;
	}

proc_end:
	*res_len = sizeof(uint8_t);
	ZXDH_SET(msg_reply_body, res_info, flag, ZXDH_REPS_FAIL);
	return ret;
}

static const zxdh_msg_process_callback zxdh_proc_cb[] = {
	[ZXDH_NULL] = NULL,
	[ZXDH_VF_PORT_INIT] = zxdh_vf_port_init,
	[ZXDH_VF_PORT_UNINIT] = zxdh_vf_port_uninit,
	[ZXDH_MAC_ADD] = zxdh_add_vf_mac_table,
	[ZXDH_MAC_DEL] = zxdh_del_vf_mac_table,
	[ZXDH_PORT_PROMISC_SET] = zxdh_vf_promisc_set,
	[ZXDH_VLAN_FILTER_SET] = zxdh_vf_set_vlan_filter,
	[ZXDH_VLAN_FILTER_ADD] = zxdh_vf_vlan_filter_table_add,
	[ZXDH_VLAN_FILTER_DEL] = zxdh_vf_vlan_filter_table_del,
	[ZXDH_VLAN_OFFLOAD] = zxdh_vf_set_vlan_offload,
	[ZXDH_VLAN_SET_TPID] = zxdh_vf_vlan_tpid_set,
	[ZXDH_RSS_ENABLE] = zxdh_vf_rss_enable,
	[ZXDH_RSS_RETA_GET] = zxdh_vf_rss_table_get,
	[ZXDH_RSS_RETA_SET] = zxdh_vf_rss_table_set,
	[ZXDH_RSS_HF_SET] = zxdh_vf_rss_hf_set,
	[ZXDH_RSS_HF_GET] = zxdh_vf_rss_hf_get,
	[ZXDH_PORT_ATTRS_SET] = zxdh_vf_port_attr_set,
	[ZXDH_GET_NP_STATS] = zxdh_vf_np_stats_update,
	[ZXDH_PORT_METER_STAT_GET] = zxdh_vf_mtr_hw_stats_get,
	[ZXDH_PLCR_CAR_PROFILE_ID_ADD] = zxdh_vf_mtr_hw_profile_add,
	[ZXDH_PLCR_CAR_PROFILE_ID_DELETE] =  zxdh_vf_mtr_hw_profile_del,
	[ZXDH_PLCR_CAR_QUEUE_CFG_SET] = zxdh_vf_mtr_hw_plcrflow_cfg,
	[ZXDH_PLCR_CAR_PROFILE_CFG_SET] = zxdh_vf_mtr_hw_profile_cfg,
};

static inline int
zxdh_config_process_callback(struct zxdh_hw *hw, struct zxdh_msg_info *msg_info,
	void *res, uint16_t *res_len)
{
	struct zxdh_msg_head *msghead = &msg_info->msg_head;
	int ret = -1;

	if (!res || !res_len) {
		PMD_DRV_LOG(ERR, " invalid param");
		return -1;
	}
	if (zxdh_proc_cb[msghead->msg_type]) {
		ret = zxdh_proc_cb[msghead->msg_type](hw, msghead->vport,
					(void *)&msg_info->data, res, res_len);
		if (!ret)
			ZXDH_SET(msg_reply_body, res, flag, ZXDH_REPS_SUCC);
		else
			ZXDH_SET(msg_reply_body, res, flag, ZXDH_REPS_FAIL);
	} else {
		ZXDH_SET(msg_reply_body, res, flag, ZXDH_REPS_INVALID);
	}
	*res_len += sizeof(uint8_t);
	return ret;
}

static int
pf_recv_bar_msg(void *pay_load, uint16_t len, void *reps_buffer,
	uint16_t *reps_len, void *eth_dev)
{
	struct zxdh_msg_info *msg_info = (struct zxdh_msg_info *)pay_load;
	void *reply_data_addr = ZXDH_ADDR_OF(msg_reply_body, reps_buffer, reply_data);
	struct rte_eth_dev *dev = (struct rte_eth_dev *)eth_dev;
	int32_t ret = 0;
	struct zxdh_hw *hw;
	uint16_t reply_len = 0;

	if (eth_dev == NULL) {
		PMD_DRV_LOG(ERR, "param invalid, dev is null.");
		ret = -2;
		goto msg_proc_end;
	}
	hw = dev->data->dev_private;

	if (msg_info->msg_head.msg_type >= ZXDH_MSG_TYPE_END) {
		PMD_DRV_LOG(ERR, "len %u msg_type %d unsupported",
			len, msg_info->msg_head.msg_type);
		ret = -2;
		goto msg_proc_end;
	}

	ret = zxdh_config_process_callback(hw, msg_info, reps_buffer, &reply_len);
	*reps_len = reply_len + ZXDH_ST_SZ_BYTES(msg_reply_head);
	return ret;

msg_proc_end:
	memcpy(reply_data_addr, &ret, sizeof(ret));
	reply_len = sizeof(ret);
	*reps_len = ZXDH_ST_SZ_BYTES(msg_reply_head) + reply_len;
	return ret;
}

void
zxdh_msg_cb_reg(struct zxdh_hw *hw)
{
	if (hw->is_pf)
		zxdh_bar_chan_msg_recv_register(ZXDH_MODULE_BAR_MSG_TO_PF, pf_recv_bar_msg);
}
