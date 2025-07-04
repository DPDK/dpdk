/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __CN20K_RXTX_H__
#define __CN20K_RXTX_H__

#include <rte_security.h>

/* ROC Constants */
#include "roc_constants.h"

/* Platform definition */
#include "roc_platform.h"

/* IO */
#if defined(__aarch64__)
#include "roc_io.h"
#else
#include "roc_io_generic.h"
#endif

/* HW structure definition */
#include "hw/cpt.h"
#include "hw/nix.h"
#include "hw/npa.h"
#include "hw/npc.h"
#include "hw/ssow.h"

/* NPA */
#include "roc_npa_dp.h"

/* SSO */
#include "roc_sso_dp.h"

/* CPT */
#include "roc_cpt.h"

#include "roc_ie_ot.h"

#include "roc_ie_ow.h"

/* NIX Inline dev */
#include "roc_nix_inl_dp.h"

#include "cnxk_ethdev_dp.h"

struct cn20k_eth_txq {
	uint64_t send_hdr_w0;
	int64_t __rte_atomic fc_cache_pkts;
	uint64_t __rte_atomic *fc_mem;
	uintptr_t lmt_base;
	rte_iova_t io_addr;
	uint16_t sqes_per_sqb_log2;
	int16_t nb_sqb_bufs_adj;
	uint8_t flag;
	rte_iova_t cpt_io_addr;
	uint64_t sa_base;
	uint64_t __rte_atomic *cpt_fc;
	uint16_t cpt_desc;
	int32_t __rte_atomic *cpt_fc_sw;
	uint64_t lso_tun_fmt;
	uint64_t ts_mem;
	uint64_t mark_flag : 8;
	uint64_t mark_fmt : 48;
	struct cnxk_eth_txq_comp tx_compl;
} __plt_cache_aligned;

struct cn20k_eth_rxq {
	uint64_t mbuf_initializer;
	uintptr_t desc;
	void *lookup_mem;
	uintptr_t cq_door;
	uint64_t wdata;
	int64_t *cq_status;
	uint32_t head;
	uint32_t qmask;
	uint32_t available;
	uint16_t data_off;
	uint64_t sa_base;
	uint64_t lmt_base;
	uint64_t meta_aura;
	uintptr_t meta_pool;
	uint16_t rq;
	uint64_t mp_buf_sz;
	struct cnxk_timesync_info *tstamp;
} __plt_cache_aligned;

/* Private data in sw rsvd area of struct roc_ot_ipsec_inb_sa */
struct cn20k_inb_priv_data {
	void *userdata;
	int reass_dynfield_off;
	int reass_dynflag_bit;
	struct cnxk_eth_sec_sess *eth_sec;
};

struct __rte_packed_begin cn20k_sec_sess_priv {
	union {
		struct {
			uint32_t sa_idx;
			uint8_t inb_sa : 1;
			uint8_t outer_ip_ver : 1;
			uint8_t mode : 1;
			uint8_t roundup_byte : 5;
			uint8_t roundup_len;
			uint16_t partial_len : 10;
			uint16_t chksum : 2;
			uint16_t dec_ttl : 1;
			uint16_t nixtx_off : 1;
			uint16_t rsvd : 2;
		};

		uint64_t u64;
	};
} __rte_packed_end;

#define LMT_OFF(lmt_addr, lmt_num, offset)                                                         \
	(void *)((uintptr_t)(lmt_addr) + ((uint64_t)(lmt_num) << ROC_LMT_LINE_SIZE_LOG2) + (offset))

static inline uint16_t
nix_tx_compl_nb_pkts(struct cn20k_eth_txq *txq, const uint64_t wdata, const uint32_t qmask)
{
	uint16_t available = txq->tx_compl.available;

	/* Update the available count if cached value is not enough */
	if (!unlikely(available)) {
		uint64_t reg, head, tail;

		/* Use LDADDA version to avoid reorder */
		reg = roc_atomic64_add_sync(wdata, txq->tx_compl.cq_status);
		/* CQ_OP_STATUS operation error */
		if (reg & BIT_ULL(NIX_CQ_OP_STAT_OP_ERR) || reg & BIT_ULL(NIX_CQ_OP_STAT_CQ_ERR))
			return 0;

		tail = reg & 0xFFFFF;
		head = (reg >> 20) & 0xFFFFF;
		if (tail < head)
			available = tail - head + qmask + 1;
		else
			available = tail - head;

		txq->tx_compl.available = available;
	}
	return available;
}

static inline void
handle_tx_completion_pkts(struct cn20k_eth_txq *txq, uint8_t mt_safe)
{
#define CNXK_NIX_CQ_ENTRY_SZ 128
#define CQE_SZ(x)	     ((x) * CNXK_NIX_CQ_ENTRY_SZ)

	uint16_t tx_pkts = 0, nb_pkts;
	const uintptr_t desc = txq->tx_compl.desc_base;
	const uint64_t wdata = txq->tx_compl.wdata;
	const uint32_t qmask = txq->tx_compl.qmask;
	uint32_t head = txq->tx_compl.head;
	struct nix_cqe_hdr_s *tx_compl_cq;
	struct nix_send_comp_s *tx_compl_s0;
	struct rte_mbuf *m_next, *m;

	if (mt_safe)
		rte_spinlock_lock(&txq->tx_compl.ext_buf_lock);

	nb_pkts = nix_tx_compl_nb_pkts(txq, wdata, qmask);
	while (tx_pkts < nb_pkts) {
		rte_prefetch_non_temporal((void *)(desc + (CQE_SZ((head + 2) & qmask))));
		tx_compl_cq = (struct nix_cqe_hdr_s *)(desc + CQE_SZ(head));
		tx_compl_s0 = (struct nix_send_comp_s *)((uint64_t *)tx_compl_cq + 1);
		m = txq->tx_compl.ptr[tx_compl_s0->sqe_id];
		while (m->next != NULL) {
			m_next = m->next;
			rte_pktmbuf_free_seg(m);
			m = m_next;
		}
		rte_pktmbuf_free_seg(m);
		txq->tx_compl.ptr[tx_compl_s0->sqe_id] = NULL;

		head++;
		head &= qmask;
		tx_pkts++;
	}
	txq->tx_compl.head = head;
	txq->tx_compl.available -= nb_pkts;

	plt_write64((wdata | nb_pkts), txq->tx_compl.cq_door);

	if (mt_safe)
		rte_spinlock_unlock(&txq->tx_compl.ext_buf_lock);
}

static __rte_always_inline uint64_t
cn20k_cpt_tx_steor_data(void)
{
	/* We have two CPT instructions per LMTLine TODO */
	const uint64_t dw_m1 = ROC_CN10K_TWO_CPT_INST_DW_M1;
	uint64_t data;

	/* This will be moved to addr area */
	data = dw_m1 << 16;
	data |= dw_m1 << 19;
	data |= dw_m1 << 22;
	data |= dw_m1 << 25;
	data |= dw_m1 << 28;
	data |= dw_m1 << 31;
	data |= dw_m1 << 34;
	data |= dw_m1 << 37;
	data |= dw_m1 << 40;
	data |= dw_m1 << 43;
	data |= dw_m1 << 46;
	data |= dw_m1 << 49;
	data |= dw_m1 << 52;
	data |= dw_m1 << 55;
	data |= dw_m1 << 58;
	data |= dw_m1 << 61;

	return data;
}

static __rte_always_inline void
cn20k_nix_sec_steorl(uintptr_t io_addr, uint32_t lmt_id, uint8_t lnum, uint8_t loff, uint8_t shft)
{
	uint64_t data;
	uintptr_t pa;

	/* Check if there is any CPT instruction to submit */
	if (!lnum && !loff)
		return;

	data = cn20k_cpt_tx_steor_data();
	/* Update lmtline use for partial end line */
	if (loff) {
		data &= ~(0x7ULL << shft);
		/* Update it to half full i.e 64B */
		data |= (0x3UL << shft);
	}

	pa = io_addr | ((data >> 16) & 0x7) << 4;
	data &= ~(0x7ULL << 16);
	/* Update lines - 1 that contain valid data */
	data |= ((uint64_t)(lnum + loff - 1)) << 12;
	data |= (uint64_t)lmt_id;

	/* STEOR */
	roc_lmt_submit_steorl(data, pa);
}

#endif /* __CN20K_RXTX_H__ */
