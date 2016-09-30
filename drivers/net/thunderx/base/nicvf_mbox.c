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

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "nicvf_plat.h"

#define NICVF_MBOX_PF_RESPONSE_DELAY_US   (1000)

static const char *mbox_message[NIC_MBOX_MSG_MAX] =  {
	[NIC_MBOX_MSG_INVALID]            = "NIC_MBOX_MSG_INVALID",
	[NIC_MBOX_MSG_READY]              = "NIC_MBOX_MSG_READY",
	[NIC_MBOX_MSG_ACK]                = "NIC_MBOX_MSG_ACK",
	[NIC_MBOX_MSG_NACK]               = "NIC_MBOX_MSG_ACK",
	[NIC_MBOX_MSG_QS_CFG]             = "NIC_MBOX_MSG_QS_CFG",
	[NIC_MBOX_MSG_RQ_CFG]             = "NIC_MBOX_MSG_RQ_CFG",
	[NIC_MBOX_MSG_SQ_CFG]             = "NIC_MBOX_MSG_SQ_CFG",
	[NIC_MBOX_MSG_RQ_DROP_CFG]        = "NIC_MBOX_MSG_RQ_DROP_CFG",
	[NIC_MBOX_MSG_SET_MAC]            = "NIC_MBOX_MSG_SET_MAC",
	[NIC_MBOX_MSG_SET_MAX_FRS]        = "NIC_MBOX_MSG_SET_MAX_FRS",
	[NIC_MBOX_MSG_CPI_CFG]            = "NIC_MBOX_MSG_CPI_CFG",
	[NIC_MBOX_MSG_RSS_SIZE]           = "NIC_MBOX_MSG_RSS_SIZE",
	[NIC_MBOX_MSG_RSS_CFG]            = "NIC_MBOX_MSG_RSS_CFG",
	[NIC_MBOX_MSG_RSS_CFG_CONT]       = "NIC_MBOX_MSG_RSS_CFG_CONT",
	[NIC_MBOX_MSG_RQ_BP_CFG]          = "NIC_MBOX_MSG_RQ_BP_CFG",
	[NIC_MBOX_MSG_RQ_SW_SYNC]         = "NIC_MBOX_MSG_RQ_SW_SYNC",
	[NIC_MBOX_MSG_BGX_LINK_CHANGE]    = "NIC_MBOX_MSG_BGX_LINK_CHANGE",
	[NIC_MBOX_MSG_ALLOC_SQS]          = "NIC_MBOX_MSG_ALLOC_SQS",
	[NIC_MBOX_MSG_LOOPBACK]           = "NIC_MBOX_MSG_LOOPBACK",
	[NIC_MBOX_MSG_RESET_STAT_COUNTER] = "NIC_MBOX_MSG_RESET_STAT_COUNTER",
	[NIC_MBOX_MSG_CFG_DONE]           = "NIC_MBOX_MSG_CFG_DONE",
	[NIC_MBOX_MSG_SHUTDOWN]           = "NIC_MBOX_MSG_SHUTDOWN",
	[NIC_MBOX_MSG_RES_BIT]            = "NIC_MBOX_MSG_RES_BIT",
	[NIC_MBOX_MSG_RSS_SIZE_RES_BIT]   = "NIC_MBOX_MSG_RSS_SIZE",
	[NIC_MBOX_MSG_ALLOC_SQS_RES_BIT]  = "NIC_MBOX_MSG_ALLOC_SQS",
};

static inline const char * __attribute__((unused))
nicvf_mbox_msg_str(int msg)
{
	assert(msg >= 0 && msg < NIC_MBOX_MSG_MAX);
	/* undefined messages */
	if (mbox_message[msg] == NULL)
		msg = 0;
	return mbox_message[msg];
}

static inline void
nicvf_mbox_send_msg_to_pf_raw(struct nicvf *nic, struct nic_mbx *mbx)
{
	uint64_t *mbx_data;
	uint64_t mbx_addr;
	int i;

	mbx_addr = NIC_VF_PF_MAILBOX_0_1;
	mbx_data = (uint64_t *)mbx;
	for (i = 0; i < NIC_PF_VF_MAILBOX_SIZE; i++) {
		nicvf_reg_write(nic, mbx_addr, *mbx_data);
		mbx_data++;
		mbx_addr += sizeof(uint64_t);
	}
	nicvf_mbox_log("msg sent %s (VF%d)",
			nicvf_mbox_msg_str(mbx->msg.msg), nic->vf_id);
}

static inline void
nicvf_mbox_send_async_msg_to_pf(struct nicvf *nic, struct nic_mbx *mbx)
{
	nicvf_mbox_send_msg_to_pf_raw(nic, mbx);
	/* Messages without ack are racy!*/
	nicvf_delay_us(NICVF_MBOX_PF_RESPONSE_DELAY_US);
}

static inline int
nicvf_mbox_send_msg_to_pf(struct nicvf *nic, struct nic_mbx *mbx)
{
	long timeout;
	long sleep = 10;
	int i, retry = 5;

	for (i = 0; i < retry; i++) {
		nic->pf_acked = false;
		nic->pf_nacked = false;
		nicvf_smp_wmb();

		nicvf_mbox_send_msg_to_pf_raw(nic, mbx);
		/* Give some time to get PF response */
		nicvf_delay_us(NICVF_MBOX_PF_RESPONSE_DELAY_US);
		timeout = NIC_MBOX_MSG_TIMEOUT;
		while (timeout > 0) {
			/* Periodic poll happens from nicvf_interrupt() */
			nicvf_smp_rmb();

			if (nic->pf_nacked)
				return -EINVAL;
			if (nic->pf_acked)
				return 0;

			nicvf_delay_us(NICVF_MBOX_PF_RESPONSE_DELAY_US);
			timeout -= sleep;
		}
		nicvf_log_error("PF didn't ack to msg 0x%02x %s VF%d (%d/%d)",
				mbx->msg.msg, nicvf_mbox_msg_str(mbx->msg.msg),
				nic->vf_id, i, retry);
	}
	return -EBUSY;
}


int
nicvf_handle_mbx_intr(struct nicvf *nic)
{
	struct nic_mbx mbx;
	uint64_t *mbx_data = (uint64_t *)&mbx;
	uint64_t mbx_addr = NIC_VF_PF_MAILBOX_0_1;
	size_t i;

	for (i = 0; i < NIC_PF_VF_MAILBOX_SIZE; i++) {
		*mbx_data = nicvf_reg_read(nic, mbx_addr);
		mbx_data++;
		mbx_addr += sizeof(uint64_t);
	}

	/* Overwrite the message so we won't receive it again */
	nicvf_reg_write(nic, NIC_VF_PF_MAILBOX_0_1, 0x0);

	nicvf_mbox_log("msg received id=0x%hhx %s (VF%d)", mbx.msg.msg,
			nicvf_mbox_msg_str(mbx.msg.msg), nic->vf_id);

	switch (mbx.msg.msg) {
	case NIC_MBOX_MSG_READY:
		nic->vf_id = mbx.nic_cfg.vf_id & 0x7F;
		nic->tns_mode = mbx.nic_cfg.tns_mode & 0x7F;
		nic->node = mbx.nic_cfg.node_id;
		nic->sqs_mode = mbx.nic_cfg.sqs_mode;
		nic->loopback_supported = mbx.nic_cfg.loopback_supported;
		ether_addr_copy((struct ether_addr *)mbx.nic_cfg.mac_addr,
				(struct ether_addr *)nic->mac_addr);
		nic->pf_acked = true;
		break;
	case NIC_MBOX_MSG_ACK:
		nic->pf_acked = true;
		break;
	case NIC_MBOX_MSG_NACK:
		nic->pf_nacked = true;
		break;
	case NIC_MBOX_MSG_RSS_SIZE_RES_BIT:
		nic->rss_info.rss_size = mbx.rss_size.ind_tbl_size;
		nic->pf_acked = true;
		break;
	case NIC_MBOX_MSG_BGX_LINK_CHANGE:
		nic->link_up = mbx.link_status.link_up;
		nic->duplex = mbx.link_status.duplex;
		nic->speed = mbx.link_status.speed;
		nic->pf_acked = true;
		break;
	case NIC_MBOX_MSG_ALLOC_SQS_RES_BIT:
		assert_primary(nic);
		if (mbx.sqs_alloc.qs_count != nic->sqs_count) {
			nicvf_log_error("Received %" PRIu8 "/%" PRIu8
			                " secondary qsets",
			                mbx.sqs_alloc.qs_count,
			                nic->sqs_count);
			abort();
		}
		for (i = 0; i < mbx.sqs_alloc.qs_count; i++) {
			if (mbx.sqs_alloc.svf[i] != nic->snicvf[i]->vf_id) {
				nicvf_log_error("Received secondary qset[%zu] "
				                "ID %" PRIu8 " expected %"
				                PRIu8, i, mbx.sqs_alloc.svf[i],
				                nic->snicvf[i]->vf_id);
				abort();
			}
		}
		nic->pf_acked = true;
		break;
	default:
		nicvf_log_error("Invalid message from PF, msg_id=0x%hhx %s",
				mbx.msg.msg, nicvf_mbox_msg_str(mbx.msg.msg));
		break;
	}
	nicvf_smp_wmb();

	return mbx.msg.msg;
}

/*
 * Checks if VF is able to communicate with PF
 * and also gets the VNIC number this VF is associated to.
 */
int
nicvf_mbox_check_pf_ready(struct nicvf *nic)
{
	struct nic_mbx mbx = { .msg = {.msg = NIC_MBOX_MSG_READY} };

	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_set_mac_addr(struct nicvf *nic,
			const uint8_t mac[NICVF_MAC_ADDR_SIZE])
{
	struct nic_mbx mbx = { .msg = {0} };
	int i;

	mbx.msg.msg = NIC_MBOX_MSG_SET_MAC;
	mbx.mac.vf_id = nic->vf_id;
	for (i = 0; i < 6; i++)
		mbx.mac.mac_addr[i] = mac[i];

	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_config_cpi(struct nicvf *nic, uint32_t qcnt)
{
	struct nic_mbx mbx = { .msg = { 0 } };

	mbx.msg.msg = NIC_MBOX_MSG_CPI_CFG;
	mbx.cpi_cfg.vf_id = nic->vf_id;
	mbx.cpi_cfg.cpi_alg = nic->cpi_alg;
	mbx.cpi_cfg.rq_cnt = qcnt;

	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_get_rss_size(struct nicvf *nic)
{
	struct nic_mbx mbx = { .msg = { 0 } };

	mbx.msg.msg = NIC_MBOX_MSG_RSS_SIZE;
	mbx.rss_size.vf_id = nic->vf_id;

	/* Result will be stored in nic->rss_info.rss_size */
	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_config_rss(struct nicvf *nic)
{
	struct nic_mbx mbx = { .msg = { 0 } };
	struct nicvf_rss_reta_info *rss = &nic->rss_info;
	size_t tot_len = rss->rss_size;
	size_t cur_len;
	size_t cur_idx = 0;
	size_t i;

	mbx.rss_cfg.vf_id = nic->vf_id;
	mbx.rss_cfg.hash_bits = rss->hash_bits;
	mbx.rss_cfg.tbl_len = 0;
	mbx.rss_cfg.tbl_offset = 0;

	while (cur_idx < tot_len) {
		cur_len = nicvf_min(tot_len - cur_idx,
				(size_t)RSS_IND_TBL_LEN_PER_MBX_MSG);
		mbx.msg.msg = (cur_idx > 0) ?
			NIC_MBOX_MSG_RSS_CFG_CONT : NIC_MBOX_MSG_RSS_CFG;
		mbx.rss_cfg.tbl_offset = cur_idx;
		mbx.rss_cfg.tbl_len = cur_len;
		for (i = 0; i < cur_len; i++)
			mbx.rss_cfg.ind_tbl[i] = rss->ind_tbl[cur_idx++];

		if (nicvf_mbox_send_msg_to_pf(nic, &mbx))
			return NICVF_ERR_RSS_TBL_UPDATE;
	}

	return 0;
}

int
nicvf_mbox_rq_config(struct nicvf *nic, uint16_t qidx,
		     struct pf_rq_cfg *pf_rq_cfg)
{
	struct nic_mbx mbx = { .msg = { 0 } };

	mbx.msg.msg = NIC_MBOX_MSG_RQ_CFG;
	mbx.rq.qs_num = nic->vf_id;
	mbx.rq.rq_num = qidx;
	mbx.rq.cfg = pf_rq_cfg->value;
	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_sq_config(struct nicvf *nic, uint16_t qidx)
{
	struct nic_mbx mbx = { .msg = { 0 } };

	mbx.msg.msg = NIC_MBOX_MSG_SQ_CFG;
	mbx.sq.qs_num = nic->vf_id;
	mbx.sq.sq_num = qidx;
	mbx.sq.sqs_mode = nic->sqs_mode;
	mbx.sq.cfg = (nic->vf_id << 3) | qidx;
	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_qset_config(struct nicvf *nic, struct pf_qs_cfg *qs_cfg)
{
	struct nic_mbx mbx = { .msg = { 0 } };

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	qs_cfg->be = 1;
#endif
	/* Send a mailbox msg to PF to config Qset */
	mbx.msg.msg = NIC_MBOX_MSG_QS_CFG;
	mbx.qs.num = nic->vf_id;
	mbx.qs.sqs_count = nic->sqs_count;
	mbx.qs.cfg = qs_cfg->value;
	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_request_sqs(struct nicvf *nic)
{
	struct nic_mbx mbx = { .msg = { 0 } };
	size_t i;

	assert_primary(nic);
	assert(nic->sqs_count > 0);
	assert(nic->sqs_count <= MAX_SQS_PER_VF);

	mbx.sqs_alloc.msg = NIC_MBOX_MSG_ALLOC_SQS;
	mbx.sqs_alloc.spec = 1;
	mbx.sqs_alloc.qs_count = nic->sqs_count;

	/* Set no of Rx/Tx queues in each of the SQsets */
	for (i = 0; i < nic->sqs_count; i++)
		mbx.sqs_alloc.svf[i] = nic->snicvf[i]->vf_id;

	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_rq_drop_config(struct nicvf *nic, uint16_t qidx, bool enable)
{
	struct nic_mbx mbx = { .msg = { 0 } };
	struct pf_rq_drop_cfg *drop_cfg;

	/* Enable CQ drop to reserve sufficient CQEs for all tx packets */
	mbx.msg.msg = NIC_MBOX_MSG_RQ_DROP_CFG;
	mbx.rq.qs_num = nic->vf_id;
	mbx.rq.rq_num = qidx;
	drop_cfg = (struct pf_rq_drop_cfg *)&mbx.rq.cfg;
	drop_cfg->value = 0;
	if (enable) {
		drop_cfg->cq_red = 1;
		drop_cfg->cq_drop = 2;
	}
	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_update_hw_max_frs(struct nicvf *nic, uint16_t mtu)
{
	struct nic_mbx mbx = { .msg = { 0 } };

	mbx.msg.msg = NIC_MBOX_MSG_SET_MAX_FRS;
	mbx.frs.max_frs = mtu;
	mbx.frs.vf_id = nic->vf_id;
	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_rq_sync(struct nicvf *nic)
{
	struct nic_mbx mbx = { .msg = { 0 } };

	/* Make sure all packets in the pipeline are written back into mem */
	mbx.msg.msg = NIC_MBOX_MSG_RQ_SW_SYNC;
	mbx.rq.cfg = 0;
	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_rq_bp_config(struct nicvf *nic, uint16_t qidx, bool enable)
{
	struct nic_mbx mbx = { .msg = { 0 } };

	mbx.msg.msg = NIC_MBOX_MSG_RQ_BP_CFG;
	mbx.rq.qs_num = nic->vf_id;
	mbx.rq.rq_num = qidx;
	mbx.rq.cfg = 0;
	if (enable)
		mbx.rq.cfg = (1ULL << 63) | (1ULL << 62) | (nic->vf_id << 0);
	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_loopback_config(struct nicvf *nic, bool enable)
{
	struct nic_mbx mbx = { .msg = { 0 } };

	mbx.lbk.msg = NIC_MBOX_MSG_LOOPBACK;
	mbx.lbk.vf_id = nic->vf_id;
	mbx.lbk.enable = enable;
	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

int
nicvf_mbox_reset_stat_counters(struct nicvf *nic, uint16_t rx_stat_mask,
			       uint8_t tx_stat_mask, uint16_t rq_stat_mask,
			       uint16_t sq_stat_mask)
{
	struct nic_mbx mbx = { .msg = { 0 } };

	mbx.reset_stat.msg = NIC_MBOX_MSG_RESET_STAT_COUNTER;
	mbx.reset_stat.rx_stat_mask = rx_stat_mask;
	mbx.reset_stat.tx_stat_mask = tx_stat_mask;
	mbx.reset_stat.rq_stat_mask = rq_stat_mask;
	mbx.reset_stat.sq_stat_mask = sq_stat_mask;
	return nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

void
nicvf_mbox_shutdown(struct nicvf *nic)
{
	struct nic_mbx mbx = { .msg = { 0 } };

	mbx.msg.msg = NIC_MBOX_MSG_SHUTDOWN;
	nicvf_mbox_send_msg_to_pf(nic, &mbx);
}

void
nicvf_mbox_cfg_done(struct nicvf *nic)
{
	struct nic_mbx mbx = { .msg = { 0 } };

	mbx.msg.msg = NIC_MBOX_MSG_CFG_DONE;
	nicvf_mbox_send_async_msg_to_pf(nic, &mbx);
}
