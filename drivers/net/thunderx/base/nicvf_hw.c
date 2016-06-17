/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2016.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium networks nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "nicvf_plat.h"

struct nicvf_reg_info {
	uint32_t offset;
	const char *name;
};

#define NICVF_REG_POLL_ITER_NR   (10)
#define NICVF_REG_POLL_DELAY_US  (2000)
#define NICVF_REG_INFO(reg) {reg, #reg}

static const struct nicvf_reg_info nicvf_reg_tbl[] = {
	NICVF_REG_INFO(NIC_VF_CFG),
	NICVF_REG_INFO(NIC_VF_PF_MAILBOX_0_1),
	NICVF_REG_INFO(NIC_VF_INT),
	NICVF_REG_INFO(NIC_VF_INT_W1S),
	NICVF_REG_INFO(NIC_VF_ENA_W1C),
	NICVF_REG_INFO(NIC_VF_ENA_W1S),
	NICVF_REG_INFO(NIC_VNIC_RSS_CFG),
	NICVF_REG_INFO(NIC_VNIC_RQ_GEN_CFG),
};

static const struct nicvf_reg_info nicvf_multi_reg_tbl[] = {
	{NIC_VNIC_RSS_KEY_0_4 + 0,  "NIC_VNIC_RSS_KEY_0"},
	{NIC_VNIC_RSS_KEY_0_4 + 8,  "NIC_VNIC_RSS_KEY_1"},
	{NIC_VNIC_RSS_KEY_0_4 + 16, "NIC_VNIC_RSS_KEY_2"},
	{NIC_VNIC_RSS_KEY_0_4 + 24, "NIC_VNIC_RSS_KEY_3"},
	{NIC_VNIC_RSS_KEY_0_4 + 32, "NIC_VNIC_RSS_KEY_4"},
	{NIC_VNIC_TX_STAT_0_4 + 0,  "NIC_VNIC_STAT_TX_OCTS"},
	{NIC_VNIC_TX_STAT_0_4 + 8,  "NIC_VNIC_STAT_TX_UCAST"},
	{NIC_VNIC_TX_STAT_0_4 + 16,  "NIC_VNIC_STAT_TX_BCAST"},
	{NIC_VNIC_TX_STAT_0_4 + 24,  "NIC_VNIC_STAT_TX_MCAST"},
	{NIC_VNIC_TX_STAT_0_4 + 32,  "NIC_VNIC_STAT_TX_DROP"},
	{NIC_VNIC_RX_STAT_0_13 + 0,  "NIC_VNIC_STAT_RX_OCTS"},
	{NIC_VNIC_RX_STAT_0_13 + 8,  "NIC_VNIC_STAT_RX_UCAST"},
	{NIC_VNIC_RX_STAT_0_13 + 16, "NIC_VNIC_STAT_RX_BCAST"},
	{NIC_VNIC_RX_STAT_0_13 + 24, "NIC_VNIC_STAT_RX_MCAST"},
	{NIC_VNIC_RX_STAT_0_13 + 32, "NIC_VNIC_STAT_RX_RED"},
	{NIC_VNIC_RX_STAT_0_13 + 40, "NIC_VNIC_STAT_RX_RED_OCTS"},
	{NIC_VNIC_RX_STAT_0_13 + 48, "NIC_VNIC_STAT_RX_ORUN"},
	{NIC_VNIC_RX_STAT_0_13 + 56, "NIC_VNIC_STAT_RX_ORUN_OCTS"},
	{NIC_VNIC_RX_STAT_0_13 + 64, "NIC_VNIC_STAT_RX_FCS"},
	{NIC_VNIC_RX_STAT_0_13 + 72, "NIC_VNIC_STAT_RX_L2ERR"},
	{NIC_VNIC_RX_STAT_0_13 + 80, "NIC_VNIC_STAT_RX_DRP_BCAST"},
	{NIC_VNIC_RX_STAT_0_13 + 88, "NIC_VNIC_STAT_RX_DRP_MCAST"},
	{NIC_VNIC_RX_STAT_0_13 + 96, "NIC_VNIC_STAT_RX_DRP_L3BCAST"},
	{NIC_VNIC_RX_STAT_0_13 + 104, "NIC_VNIC_STAT_RX_DRP_L3MCAST"},
};

static const struct nicvf_reg_info nicvf_qset_cq_reg_tbl[] = {
	NICVF_REG_INFO(NIC_QSET_CQ_0_7_CFG),
	NICVF_REG_INFO(NIC_QSET_CQ_0_7_CFG2),
	NICVF_REG_INFO(NIC_QSET_CQ_0_7_THRESH),
	NICVF_REG_INFO(NIC_QSET_CQ_0_7_BASE),
	NICVF_REG_INFO(NIC_QSET_CQ_0_7_HEAD),
	NICVF_REG_INFO(NIC_QSET_CQ_0_7_TAIL),
	NICVF_REG_INFO(NIC_QSET_CQ_0_7_DOOR),
	NICVF_REG_INFO(NIC_QSET_CQ_0_7_STATUS),
	NICVF_REG_INFO(NIC_QSET_CQ_0_7_STATUS2),
	NICVF_REG_INFO(NIC_QSET_CQ_0_7_DEBUG),
};

static const struct nicvf_reg_info nicvf_qset_rq_reg_tbl[] = {
	NICVF_REG_INFO(NIC_QSET_RQ_0_7_CFG),
	NICVF_REG_INFO(NIC_QSET_RQ_0_7_STATUS0),
	NICVF_REG_INFO(NIC_QSET_RQ_0_7_STATUS1),
};

static const struct nicvf_reg_info nicvf_qset_sq_reg_tbl[] = {
	NICVF_REG_INFO(NIC_QSET_SQ_0_7_CFG),
	NICVF_REG_INFO(NIC_QSET_SQ_0_7_THRESH),
	NICVF_REG_INFO(NIC_QSET_SQ_0_7_BASE),
	NICVF_REG_INFO(NIC_QSET_SQ_0_7_HEAD),
	NICVF_REG_INFO(NIC_QSET_SQ_0_7_TAIL),
	NICVF_REG_INFO(NIC_QSET_SQ_0_7_DOOR),
	NICVF_REG_INFO(NIC_QSET_SQ_0_7_STATUS),
	NICVF_REG_INFO(NIC_QSET_SQ_0_7_DEBUG),
	NICVF_REG_INFO(NIC_QSET_SQ_0_7_STATUS0),
	NICVF_REG_INFO(NIC_QSET_SQ_0_7_STATUS1),
};

static const struct nicvf_reg_info nicvf_qset_rbdr_reg_tbl[] = {
	NICVF_REG_INFO(NIC_QSET_RBDR_0_1_CFG),
	NICVF_REG_INFO(NIC_QSET_RBDR_0_1_THRESH),
	NICVF_REG_INFO(NIC_QSET_RBDR_0_1_BASE),
	NICVF_REG_INFO(NIC_QSET_RBDR_0_1_HEAD),
	NICVF_REG_INFO(NIC_QSET_RBDR_0_1_TAIL),
	NICVF_REG_INFO(NIC_QSET_RBDR_0_1_DOOR),
	NICVF_REG_INFO(NIC_QSET_RBDR_0_1_STATUS0),
	NICVF_REG_INFO(NIC_QSET_RBDR_0_1_STATUS1),
	NICVF_REG_INFO(NIC_QSET_RBDR_0_1_PRFCH_STATUS),
};

int
nicvf_base_init(struct nicvf *nic)
{
	nic->hwcap = 0;
	if (nic->subsystem_device_id == 0)
		return NICVF_ERR_BASE_INIT;

	if (nicvf_hw_version(nic) == NICVF_PASS2)
		nic->hwcap |= NICVF_CAP_TUNNEL_PARSING;

	return NICVF_OK;
}

/* dump on stdout if data is NULL */
int
nicvf_reg_dump(struct nicvf *nic,  uint64_t *data)
{
	uint32_t i, q;
	bool dump_stdout;

	dump_stdout = data ? 0 : 1;

	for (i = 0; i < NICVF_ARRAY_SIZE(nicvf_reg_tbl); i++)
		if (dump_stdout)
			nicvf_log("%24s  = 0x%" PRIx64 "\n",
				nicvf_reg_tbl[i].name,
				nicvf_reg_read(nic, nicvf_reg_tbl[i].offset));
		else
			*data++ = nicvf_reg_read(nic, nicvf_reg_tbl[i].offset);

	for (i = 0; i < NICVF_ARRAY_SIZE(nicvf_multi_reg_tbl); i++)
		if (dump_stdout)
			nicvf_log("%24s  = 0x%" PRIx64 "\n",
				nicvf_multi_reg_tbl[i].name,
				nicvf_reg_read(nic,
					nicvf_multi_reg_tbl[i].offset));
		else
			*data++ = nicvf_reg_read(nic,
					nicvf_multi_reg_tbl[i].offset);

	for (q = 0; q < MAX_CMP_QUEUES_PER_QS; q++)
		for (i = 0; i < NICVF_ARRAY_SIZE(nicvf_qset_cq_reg_tbl); i++)
			if (dump_stdout)
				nicvf_log("%30s(%d)  = 0x%" PRIx64 "\n",
					nicvf_qset_cq_reg_tbl[i].name, q,
					nicvf_queue_reg_read(nic,
					nicvf_qset_cq_reg_tbl[i].offset, q));
			else
				*data++ = nicvf_queue_reg_read(nic,
					nicvf_qset_cq_reg_tbl[i].offset, q);

	for (q = 0; q < MAX_RCV_QUEUES_PER_QS; q++)
		for (i = 0; i < NICVF_ARRAY_SIZE(nicvf_qset_rq_reg_tbl); i++)
			if (dump_stdout)
				nicvf_log("%30s(%d)  = 0x%" PRIx64 "\n",
					nicvf_qset_rq_reg_tbl[i].name, q,
					nicvf_queue_reg_read(nic,
					nicvf_qset_rq_reg_tbl[i].offset, q));
			else
				*data++ = nicvf_queue_reg_read(nic,
					nicvf_qset_rq_reg_tbl[i].offset, q);

	for (q = 0; q < MAX_SND_QUEUES_PER_QS; q++)
		for (i = 0; i < NICVF_ARRAY_SIZE(nicvf_qset_sq_reg_tbl); i++)
			if (dump_stdout)
				nicvf_log("%30s(%d)  = 0x%" PRIx64 "\n",
					nicvf_qset_sq_reg_tbl[i].name, q,
					nicvf_queue_reg_read(nic,
					nicvf_qset_sq_reg_tbl[i].offset, q));
			else
				*data++ = nicvf_queue_reg_read(nic,
					nicvf_qset_sq_reg_tbl[i].offset, q);

	for (q = 0; q < MAX_RCV_BUF_DESC_RINGS_PER_QS; q++)
		for (i = 0; i < NICVF_ARRAY_SIZE(nicvf_qset_rbdr_reg_tbl); i++)
			if (dump_stdout)
				nicvf_log("%30s(%d)  = 0x%" PRIx64 "\n",
					nicvf_qset_rbdr_reg_tbl[i].name, q,
					nicvf_queue_reg_read(nic,
					nicvf_qset_rbdr_reg_tbl[i].offset, q));
			else
				*data++ = nicvf_queue_reg_read(nic,
					nicvf_qset_rbdr_reg_tbl[i].offset, q);
	return 0;
}

int
nicvf_reg_get_count(void)
{
	int nr_regs;

	nr_regs = NICVF_ARRAY_SIZE(nicvf_reg_tbl);
	nr_regs += NICVF_ARRAY_SIZE(nicvf_multi_reg_tbl);
	nr_regs += NICVF_ARRAY_SIZE(nicvf_qset_cq_reg_tbl) *
			MAX_CMP_QUEUES_PER_QS;
	nr_regs += NICVF_ARRAY_SIZE(nicvf_qset_rq_reg_tbl) *
			MAX_RCV_QUEUES_PER_QS;
	nr_regs += NICVF_ARRAY_SIZE(nicvf_qset_sq_reg_tbl) *
			MAX_SND_QUEUES_PER_QS;
	nr_regs += NICVF_ARRAY_SIZE(nicvf_qset_rbdr_reg_tbl) *
			MAX_RCV_BUF_DESC_RINGS_PER_QS;

	return nr_regs;
}

static int
nicvf_qset_config_internal(struct nicvf *nic, bool enable)
{
	int ret;
	struct pf_qs_cfg pf_qs_cfg = {.value = 0};

	pf_qs_cfg.ena = enable ? 1 : 0;
	pf_qs_cfg.vnic = nic->vf_id;
	ret = nicvf_mbox_qset_config(nic, &pf_qs_cfg);
	return ret ? NICVF_ERR_SET_QS : 0;
}

/* Requests PF to assign and enable Qset */
int
nicvf_qset_config(struct nicvf *nic)
{
	/* Enable Qset */
	return nicvf_qset_config_internal(nic, true);
}

int
nicvf_qset_reclaim(struct nicvf *nic)
{
	/* Disable Qset */
	return nicvf_qset_config_internal(nic, false);
}

static int
cmpfunc(const void *a, const void *b)
{
	return (*(const uint32_t *)a - *(const uint32_t *)b);
}

static uint32_t
nicvf_roundup_list(uint32_t val, uint32_t list[], uint32_t entries)
{
	uint32_t i;

	qsort(list, entries, sizeof(uint32_t), cmpfunc);
	for (i = 0; i < entries; i++)
		if (val <= list[i])
			break;
	/* Not in the list */
	if (i >= entries)
		return 0;
	else
		return list[i];
}

static void
nicvf_handle_qset_err_intr(struct nicvf *nic)
{
	uint16_t qidx;
	uint64_t status;

	nicvf_log("%s (VF%d)\n", __func__, nic->vf_id);
	nicvf_reg_dump(nic, NULL);

	for (qidx = 0; qidx < MAX_CMP_QUEUES_PER_QS; qidx++) {
		status = nicvf_queue_reg_read(
				nic, NIC_QSET_CQ_0_7_STATUS, qidx);
		if (!(status & NICVF_CQ_ERR_MASK))
			continue;

		if (status & NICVF_CQ_WR_FULL)
			nicvf_log("[%d]NICVF_CQ_WR_FULL\n", qidx);
		if (status & NICVF_CQ_WR_DISABLE)
			nicvf_log("[%d]NICVF_CQ_WR_DISABLE\n", qidx);
		if (status & NICVF_CQ_WR_FAULT)
			nicvf_log("[%d]NICVF_CQ_WR_FAULT\n", qidx);
		nicvf_queue_reg_write(nic, NIC_QSET_CQ_0_7_STATUS, qidx, 0);
	}

	for (qidx = 0; qidx < MAX_SND_QUEUES_PER_QS; qidx++) {
		status = nicvf_queue_reg_read(
				nic, NIC_QSET_SQ_0_7_STATUS, qidx);
		if (!(status & NICVF_SQ_ERR_MASK))
			continue;

		if (status & NICVF_SQ_ERR_STOPPED)
			nicvf_log("[%d]NICVF_SQ_ERR_STOPPED\n", qidx);
		if (status & NICVF_SQ_ERR_SEND)
			nicvf_log("[%d]NICVF_SQ_ERR_SEND\n", qidx);
		if (status & NICVF_SQ_ERR_DPE)
			nicvf_log("[%d]NICVF_SQ_ERR_DPE\n", qidx);
		nicvf_queue_reg_write(nic, NIC_QSET_SQ_0_7_STATUS, qidx, 0);
	}

	for (qidx = 0; qidx < MAX_RCV_BUF_DESC_RINGS_PER_QS; qidx++) {
		status = nicvf_queue_reg_read(nic,
				NIC_QSET_RBDR_0_1_STATUS0, qidx);
		status &= NICVF_RBDR_FIFO_STATE_MASK;
		status >>= NICVF_RBDR_FIFO_STATE_SHIFT;

		if (status == RBDR_FIFO_STATE_FAIL)
			nicvf_log("[%d]RBDR_FIFO_STATE_FAIL\n", qidx);
		nicvf_queue_reg_write(nic, NIC_QSET_RBDR_0_1_STATUS0, qidx, 0);
	}

	nicvf_disable_all_interrupts(nic);
	abort();
}

/*
 * Handle poll mode driver interested "mbox" and "queue-set error" interrupts.
 * This function is not re-entrant.
 * The caller should provide proper serialization.
 */
int
nicvf_reg_poll_interrupts(struct nicvf *nic)
{
	int msg = 0;
	uint64_t intr;

	intr = nicvf_reg_read(nic, NIC_VF_INT);
	if (intr & NICVF_INTR_MBOX_MASK) {
		nicvf_reg_write(nic, NIC_VF_INT, NICVF_INTR_MBOX_MASK);
		msg = nicvf_handle_mbx_intr(nic);
	}
	if (intr & NICVF_INTR_QS_ERR_MASK) {
		nicvf_reg_write(nic, NIC_VF_INT, NICVF_INTR_QS_ERR_MASK);
		nicvf_handle_qset_err_intr(nic);
	}
	return msg;
}

static int
nicvf_qset_poll_reg(struct nicvf *nic, uint16_t qidx, uint32_t offset,
		    uint32_t bit_pos, uint32_t bits, uint64_t val)
{
	uint64_t bit_mask;
	uint64_t reg_val;
	int timeout = NICVF_REG_POLL_ITER_NR;

	bit_mask = (1ULL << bits) - 1;
	bit_mask = (bit_mask << bit_pos);

	while (timeout) {
		reg_val = nicvf_queue_reg_read(nic, offset, qidx);
		if (((reg_val & bit_mask) >> bit_pos) == val)
			return NICVF_OK;
		nicvf_delay_us(NICVF_REG_POLL_DELAY_US);
		timeout--;
	}
	return NICVF_ERR_REG_POLL;
}

int
nicvf_qset_rbdr_reclaim(struct nicvf *nic, uint16_t qidx)
{
	uint64_t status;
	int timeout = NICVF_REG_POLL_ITER_NR;
	struct nicvf_rbdr *rbdr = nic->rbdr;

	/* Save head and tail pointers for freeing up buffers */
	if (rbdr) {
		rbdr->head = nicvf_queue_reg_read(nic,
				NIC_QSET_RBDR_0_1_HEAD, qidx) >> 3;
		rbdr->tail = nicvf_queue_reg_read(nic,
				NIC_QSET_RBDR_0_1_TAIL,	qidx) >> 3;
		rbdr->next_tail = rbdr->tail;
	}

	/* Reset RBDR */
	nicvf_queue_reg_write(nic, NIC_QSET_RBDR_0_1_CFG, qidx,
				NICVF_RBDR_RESET);

	/* Disable RBDR */
	nicvf_queue_reg_write(nic, NIC_QSET_RBDR_0_1_CFG, qidx, 0);
	if (nicvf_qset_poll_reg(nic, qidx, NIC_QSET_RBDR_0_1_STATUS0,
				62, 2, 0x00))
		return NICVF_ERR_RBDR_DISABLE;

	while (1) {
		status = nicvf_queue_reg_read(nic,
				NIC_QSET_RBDR_0_1_PRFCH_STATUS,	qidx);
		if ((status & 0xFFFFFFFF) == ((status >> 32) & 0xFFFFFFFF))
			break;
		nicvf_delay_us(NICVF_REG_POLL_DELAY_US);
		timeout--;
		if (!timeout)
			return NICVF_ERR_RBDR_PREFETCH;
	}

	nicvf_queue_reg_write(nic, NIC_QSET_RBDR_0_1_CFG, qidx,
			NICVF_RBDR_RESET);
	if (nicvf_qset_poll_reg(nic, qidx,
			NIC_QSET_RBDR_0_1_STATUS0, 62, 2, 0x02))
		return NICVF_ERR_RBDR_RESET1;

	nicvf_queue_reg_write(nic, NIC_QSET_RBDR_0_1_CFG, qidx, 0x00);
	if (nicvf_qset_poll_reg(nic, qidx,
			NIC_QSET_RBDR_0_1_STATUS0, 62, 2, 0x00))
		return NICVF_ERR_RBDR_RESET2;

	return NICVF_OK;
}

static int
nicvf_qsize_regbit(uint32_t len, uint32_t len_shift)
{
	int val;

	val = ((uint32_t)log2(len) - len_shift);
	assert(val >= NICVF_QSIZE_MIN_VAL);
	assert(val <= NICVF_QSIZE_MAX_VAL);
	return val;
}

int
nicvf_qset_rbdr_config(struct nicvf *nic, uint16_t qidx)
{
	int ret;
	uint64_t head, tail;
	struct nicvf_rbdr *rbdr = nic->rbdr;
	struct rbdr_cfg rbdr_cfg = {.value = 0};

	ret = nicvf_qset_rbdr_reclaim(nic, qidx);
	if (ret)
		return ret;

	/* Set descriptor base address */
	nicvf_queue_reg_write(nic, NIC_QSET_RBDR_0_1_BASE, qidx, rbdr->phys);

	/* Enable RBDR  & set queue size */
	rbdr_cfg.ena = 1;
	rbdr_cfg.reset = 0;
	rbdr_cfg.ldwb = 0;
	rbdr_cfg.qsize = nicvf_qsize_regbit(rbdr->qlen_mask + 1,
						RBDR_SIZE_SHIFT);
	rbdr_cfg.avg_con = 0;
	rbdr_cfg.lines = rbdr->buffsz / 128;

	nicvf_queue_reg_write(nic, NIC_QSET_RBDR_0_1_CFG, qidx, rbdr_cfg.value);

	/* Verify proper RBDR reset */
	head = nicvf_queue_reg_read(nic, NIC_QSET_RBDR_0_1_HEAD, qidx);
	tail = nicvf_queue_reg_read(nic, NIC_QSET_RBDR_0_1_TAIL, qidx);

	if (head | tail)
		return NICVF_ERR_RBDR_RESET;

	return NICVF_OK;
}

uint32_t
nicvf_qsize_rbdr_roundup(uint32_t val)
{
	uint32_t list[] = {RBDR_QUEUE_SZ_8K, RBDR_QUEUE_SZ_16K,
			RBDR_QUEUE_SZ_32K, RBDR_QUEUE_SZ_64K,
			RBDR_QUEUE_SZ_128K, RBDR_QUEUE_SZ_256K,
			RBDR_QUEUE_SZ_512K};
	return nicvf_roundup_list(val, list, NICVF_ARRAY_SIZE(list));
}

int
nicvf_qset_rbdr_precharge(struct nicvf *nic, uint16_t ridx,
			  rbdr_pool_get_handler handler,
			  void *opaque, uint32_t max_buffs)
{
	struct rbdr_entry_t *desc, *desc0;
	struct nicvf_rbdr *rbdr = nic->rbdr;
	uint32_t count;
	nicvf_phys_addr_t phy;

	assert(rbdr != NULL);
	desc = rbdr->desc;
	count = 0;
	/* Don't fill beyond max numbers of desc */
	while (count < rbdr->qlen_mask) {
		if (count >= max_buffs)
			break;
		desc0 = desc + count;
		phy = handler(opaque);
		if (phy) {
			desc0->full_addr = phy;
			count++;
		} else {
			break;
		}
	}
	nicvf_smp_wmb();
	nicvf_queue_reg_write(nic, NIC_QSET_RBDR_0_1_DOOR, ridx, count);
	rbdr->tail = nicvf_queue_reg_read(nic,
				NIC_QSET_RBDR_0_1_TAIL, ridx) >> 3;
	rbdr->next_tail = rbdr->tail;
	nicvf_smp_rmb();
	return 0;
}

int
nicvf_qset_rbdr_active(struct nicvf *nic, uint16_t qidx)
{
	return nicvf_queue_reg_read(nic, NIC_QSET_RBDR_0_1_STATUS0, qidx);
}

int
nicvf_qset_sq_reclaim(struct nicvf *nic, uint16_t qidx)
{
	uint64_t head, tail;
	struct sq_cfg sq_cfg;

	sq_cfg.value = nicvf_queue_reg_read(nic, NIC_QSET_SQ_0_7_CFG, qidx);

	/* Disable send queue */
	nicvf_queue_reg_write(nic, NIC_QSET_SQ_0_7_CFG, qidx, 0);

	/* Check if SQ is stopped */
	if (sq_cfg.ena && nicvf_qset_poll_reg(nic, qidx, NIC_QSET_SQ_0_7_STATUS,
				NICVF_SQ_STATUS_STOPPED_BIT, 1, 0x01))
		return NICVF_ERR_SQ_DISABLE;

	/* Reset send queue */
	nicvf_queue_reg_write(nic, NIC_QSET_SQ_0_7_CFG, qidx, NICVF_SQ_RESET);
	head = nicvf_queue_reg_read(nic, NIC_QSET_SQ_0_7_HEAD, qidx) >> 4;
	tail = nicvf_queue_reg_read(nic, NIC_QSET_SQ_0_7_TAIL, qidx) >> 4;
	if (head | tail)
		return  NICVF_ERR_SQ_RESET;

	return 0;
}

int
nicvf_qset_sq_config(struct nicvf *nic, uint16_t qidx, struct nicvf_txq *txq)
{
	int ret;
	struct sq_cfg sq_cfg = {.value = 0};

	ret = nicvf_qset_sq_reclaim(nic, qidx);
	if (ret)
		return ret;

	/* Send a mailbox msg to PF to config SQ */
	if (nicvf_mbox_sq_config(nic, qidx))
		return  NICVF_ERR_SQ_PF_CFG;

	/* Set queue base address */
	nicvf_queue_reg_write(nic, NIC_QSET_SQ_0_7_BASE, qidx, txq->phys);

	/* Enable send queue  & set queue size */
	sq_cfg.ena = 1;
	sq_cfg.reset = 0;
	sq_cfg.ldwb = 0;
	sq_cfg.qsize = nicvf_qsize_regbit(txq->qlen_mask + 1, SND_QSIZE_SHIFT);
	sq_cfg.tstmp_bgx_intf = 0;
	nicvf_queue_reg_write(nic, NIC_QSET_SQ_0_7_CFG, qidx, sq_cfg.value);

	/* Ring doorbell so that H/W restarts processing SQEs */
	nicvf_queue_reg_write(nic, NIC_QSET_SQ_0_7_DOOR, qidx, 0);

	return 0;
}

uint32_t
nicvf_qsize_sq_roundup(uint32_t val)
{
	uint32_t list[] = {SND_QUEUE_SZ_1K, SND_QUEUE_SZ_2K,
			SND_QUEUE_SZ_4K, SND_QUEUE_SZ_8K,
			SND_QUEUE_SZ_16K, SND_QUEUE_SZ_32K,
			SND_QUEUE_SZ_64K};
	return nicvf_roundup_list(val, list, NICVF_ARRAY_SIZE(list));
}

int
nicvf_qset_rq_reclaim(struct nicvf *nic, uint16_t qidx)
{
	/* Disable receive queue */
	nicvf_queue_reg_write(nic, NIC_QSET_RQ_0_7_CFG, qidx, 0);
	return nicvf_mbox_rq_sync(nic);
}

int
nicvf_qset_rq_config(struct nicvf *nic, uint16_t qidx, struct nicvf_rxq *rxq)
{
	struct pf_rq_cfg pf_rq_cfg = {.value = 0};
	struct rq_cfg rq_cfg = {.value = 0};

	if (nicvf_qset_rq_reclaim(nic, qidx))
		return NICVF_ERR_RQ_CLAIM;

	pf_rq_cfg.strip_pre_l2 = 0;
	/* First cache line of RBDR data will be allocated into L2C */
	pf_rq_cfg.caching = RQ_CACHE_ALLOC_FIRST;
	pf_rq_cfg.cq_qs = nic->vf_id;
	pf_rq_cfg.cq_idx = qidx;
	pf_rq_cfg.rbdr_cont_qs = nic->vf_id;
	pf_rq_cfg.rbdr_cont_idx = 0;
	pf_rq_cfg.rbdr_strt_qs = nic->vf_id;
	pf_rq_cfg.rbdr_strt_idx = 0;

	/* Send a mailbox msg to PF to config RQ */
	if (nicvf_mbox_rq_config(nic, qidx, &pf_rq_cfg))
		return NICVF_ERR_RQ_PF_CFG;

	/* Select Rx backpressure */
	if (nicvf_mbox_rq_bp_config(nic, qidx, rxq->rx_drop_en))
		return NICVF_ERR_RQ_BP_CFG;

	/* Send a mailbox msg to PF to config RQ drop */
	if (nicvf_mbox_rq_drop_config(nic, qidx, rxq->rx_drop_en))
		return NICVF_ERR_RQ_DROP_CFG;

	/* Enable Receive queue */
	rq_cfg.ena = 1;
	nicvf_queue_reg_write(nic, NIC_QSET_RQ_0_7_CFG, qidx, rq_cfg.value);

	return 0;
}

int
nicvf_qset_cq_reclaim(struct nicvf *nic, uint16_t qidx)
{
	uint64_t tail, head;

	/* Disable completion queue */
	nicvf_queue_reg_write(nic, NIC_QSET_CQ_0_7_CFG, qidx, 0);
	if (nicvf_qset_poll_reg(nic, qidx, NIC_QSET_CQ_0_7_CFG, 42, 1, 0))
		return NICVF_ERR_CQ_DISABLE;

	/* Reset completion queue */
	nicvf_queue_reg_write(nic, NIC_QSET_CQ_0_7_CFG, qidx, NICVF_CQ_RESET);
	tail = nicvf_queue_reg_read(nic, NIC_QSET_CQ_0_7_TAIL, qidx) >> 9;
	head = nicvf_queue_reg_read(nic, NIC_QSET_CQ_0_7_HEAD, qidx) >> 9;
	if (head | tail)
		return  NICVF_ERR_CQ_RESET;

	/* Disable timer threshold (doesn't get reset upon CQ reset) */
	nicvf_queue_reg_write(nic, NIC_QSET_CQ_0_7_CFG2, qidx, 0);
	return 0;
}

int
nicvf_qset_cq_config(struct nicvf *nic, uint16_t qidx, struct nicvf_rxq *rxq)
{
	int ret;
	struct cq_cfg cq_cfg = {.value = 0};

	ret = nicvf_qset_cq_reclaim(nic, qidx);
	if (ret)
		return ret;

	/* Set completion queue base address */
	nicvf_queue_reg_write(nic, NIC_QSET_CQ_0_7_BASE, qidx, rxq->phys);

	cq_cfg.ena = 1;
	cq_cfg.reset = 0;
	/* Writes of CQE will be allocated into L2C */
	cq_cfg.caching = 1;
	cq_cfg.qsize = nicvf_qsize_regbit(rxq->qlen_mask + 1, CMP_QSIZE_SHIFT);
	cq_cfg.avg_con = 0;
	nicvf_queue_reg_write(nic, NIC_QSET_CQ_0_7_CFG, qidx, cq_cfg.value);

	/* Set threshold value for interrupt generation */
	nicvf_queue_reg_write(nic, NIC_QSET_CQ_0_7_THRESH, qidx, 0);
	nicvf_queue_reg_write(nic, NIC_QSET_CQ_0_7_CFG2, qidx, 0);
	return 0;
}

uint32_t
nicvf_qsize_cq_roundup(uint32_t val)
{
	uint32_t list[] = {CMP_QUEUE_SZ_1K, CMP_QUEUE_SZ_2K,
			CMP_QUEUE_SZ_4K, CMP_QUEUE_SZ_8K,
			CMP_QUEUE_SZ_16K, CMP_QUEUE_SZ_32K,
			CMP_QUEUE_SZ_64K};
	return nicvf_roundup_list(val, list, NICVF_ARRAY_SIZE(list));
}


void
nicvf_vlan_hw_strip(struct nicvf *nic, bool enable)
{
	uint64_t val;

	val = nicvf_reg_read(nic, NIC_VNIC_RQ_GEN_CFG);
	if (enable)
		val |= (STRIP_FIRST_VLAN << 25);
	else
		val &= ~((STRIP_SECOND_VLAN | STRIP_FIRST_VLAN) << 25);

	nicvf_reg_write(nic, NIC_VNIC_RQ_GEN_CFG, val);
}

void
nicvf_rss_set_key(struct nicvf *nic, uint8_t *key)
{
	int idx;
	uint64_t addr, val;
	uint64_t *keyptr = (uint64_t *)key;

	addr = NIC_VNIC_RSS_KEY_0_4;
	for (idx = 0; idx < RSS_HASH_KEY_SIZE; idx++) {
		val = nicvf_cpu_to_be_64(*keyptr);
		nicvf_reg_write(nic, addr, val);
		addr += sizeof(uint64_t);
		keyptr++;
	}
}

void
nicvf_rss_get_key(struct nicvf *nic, uint8_t *key)
{
	int idx;
	uint64_t addr, val;
	uint64_t *keyptr = (uint64_t *)key;

	addr = NIC_VNIC_RSS_KEY_0_4;
	for (idx = 0; idx < RSS_HASH_KEY_SIZE; idx++) {
		val = nicvf_reg_read(nic, addr);
		*keyptr = nicvf_be_to_cpu_64(val);
		addr += sizeof(uint64_t);
		keyptr++;
	}
}

void
nicvf_rss_set_cfg(struct nicvf *nic, uint64_t val)
{
	nicvf_reg_write(nic, NIC_VNIC_RSS_CFG, val);
}

uint64_t
nicvf_rss_get_cfg(struct nicvf *nic)
{
	return nicvf_reg_read(nic, NIC_VNIC_RSS_CFG);
}

int
nicvf_rss_reta_update(struct nicvf *nic, uint8_t *tbl, uint32_t max_count)
{
	uint32_t idx;
	struct nicvf_rss_reta_info *rss = &nic->rss_info;

	/* result will be stored in nic->rss_info.rss_size */
	if (nicvf_mbox_get_rss_size(nic))
		return NICVF_ERR_RSS_GET_SZ;

	assert(rss->rss_size > 0);
	rss->hash_bits = (uint8_t)log2(rss->rss_size);
	for (idx = 0; idx < rss->rss_size && idx < max_count; idx++)
		rss->ind_tbl[idx] = tbl[idx];

	if (nicvf_mbox_config_rss(nic))
		return NICVF_ERR_RSS_TBL_UPDATE;

	return NICVF_OK;
}

int
nicvf_rss_reta_query(struct nicvf *nic, uint8_t *tbl, uint32_t max_count)
{
	uint32_t idx;
	struct nicvf_rss_reta_info *rss = &nic->rss_info;

	/* result will be stored in nic->rss_info.rss_size */
	if (nicvf_mbox_get_rss_size(nic))
		return NICVF_ERR_RSS_GET_SZ;

	assert(rss->rss_size > 0);
	rss->hash_bits = (uint8_t)log2(rss->rss_size);
	for (idx = 0; idx < rss->rss_size && idx < max_count; idx++)
		tbl[idx] = rss->ind_tbl[idx];

	return NICVF_OK;
}

int
nicvf_rss_config(struct nicvf *nic, uint32_t  qcnt, uint64_t cfg)
{
	uint32_t idx;
	uint8_t default_reta[NIC_MAX_RSS_IDR_TBL_SIZE];
	uint8_t default_key[RSS_HASH_KEY_BYTE_SIZE] = {
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD,
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD,
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD,
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD,
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD
	};

	if (nic->cpi_alg != CPI_ALG_NONE)
		return -EINVAL;

	if (cfg == 0)
		return -EINVAL;

	/* Update default RSS key and cfg */
	nicvf_rss_set_key(nic, default_key);
	nicvf_rss_set_cfg(nic, cfg);

	/* Update default RSS RETA */
	for (idx = 0; idx < NIC_MAX_RSS_IDR_TBL_SIZE; idx++)
		default_reta[idx] = idx % qcnt;

	return nicvf_rss_reta_update(nic, default_reta,
			NIC_MAX_RSS_IDR_TBL_SIZE);
}

int
nicvf_rss_term(struct nicvf *nic)
{
	uint32_t idx;
	uint8_t disable_rss[NIC_MAX_RSS_IDR_TBL_SIZE];

	nicvf_rss_set_cfg(nic, 0);
	/* Redirect the output to 0th queue  */
	for (idx = 0; idx < NIC_MAX_RSS_IDR_TBL_SIZE; idx++)
		disable_rss[idx] = 0;

	return nicvf_rss_reta_update(nic, disable_rss,
			NIC_MAX_RSS_IDR_TBL_SIZE);
}

int
nicvf_loopback_config(struct nicvf *nic, bool enable)
{
	if (enable && nic->loopback_supported == 0)
		return NICVF_ERR_LOOPBACK_CFG;

	return nicvf_mbox_loopback_config(nic, enable);
}

void
nicvf_hw_get_stats(struct nicvf *nic, struct nicvf_hw_stats *stats)
{
	stats->rx_bytes = NICVF_GET_RX_STATS(RX_OCTS);
	stats->rx_ucast_frames = NICVF_GET_RX_STATS(RX_UCAST);
	stats->rx_bcast_frames = NICVF_GET_RX_STATS(RX_BCAST);
	stats->rx_mcast_frames = NICVF_GET_RX_STATS(RX_MCAST);
	stats->rx_fcs_errors = NICVF_GET_RX_STATS(RX_FCS);
	stats->rx_l2_errors = NICVF_GET_RX_STATS(RX_L2ERR);
	stats->rx_drop_red = NICVF_GET_RX_STATS(RX_RED);
	stats->rx_drop_red_bytes = NICVF_GET_RX_STATS(RX_RED_OCTS);
	stats->rx_drop_overrun = NICVF_GET_RX_STATS(RX_ORUN);
	stats->rx_drop_overrun_bytes = NICVF_GET_RX_STATS(RX_ORUN_OCTS);
	stats->rx_drop_bcast = NICVF_GET_RX_STATS(RX_DRP_BCAST);
	stats->rx_drop_mcast = NICVF_GET_RX_STATS(RX_DRP_MCAST);
	stats->rx_drop_l3_bcast = NICVF_GET_RX_STATS(RX_DRP_L3BCAST);
	stats->rx_drop_l3_mcast = NICVF_GET_RX_STATS(RX_DRP_L3MCAST);

	stats->tx_bytes_ok = NICVF_GET_TX_STATS(TX_OCTS);
	stats->tx_ucast_frames_ok = NICVF_GET_TX_STATS(TX_UCAST);
	stats->tx_bcast_frames_ok = NICVF_GET_TX_STATS(TX_BCAST);
	stats->tx_mcast_frames_ok = NICVF_GET_TX_STATS(TX_MCAST);
	stats->tx_drops = NICVF_GET_TX_STATS(TX_DROP);
}

void
nicvf_hw_get_rx_qstats(struct nicvf *nic, struct nicvf_hw_rx_qstats *qstats,
		       uint16_t qidx)
{
	qstats->q_rx_bytes =
		nicvf_queue_reg_read(nic, NIC_QSET_RQ_0_7_STATUS0, qidx);
	qstats->q_rx_packets =
		nicvf_queue_reg_read(nic, NIC_QSET_RQ_0_7_STATUS1, qidx);
}

void
nicvf_hw_get_tx_qstats(struct nicvf *nic, struct nicvf_hw_tx_qstats *qstats,
		       uint16_t qidx)
{
	qstats->q_tx_bytes =
		nicvf_queue_reg_read(nic, NIC_QSET_SQ_0_7_STATUS0, qidx);
	qstats->q_tx_packets =
		nicvf_queue_reg_read(nic, NIC_QSET_SQ_0_7_STATUS1, qidx);
}
