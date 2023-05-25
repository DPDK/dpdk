/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CN9K_ETHDEV_H__
#define __CN9K_ETHDEV_H__

#include <cnxk_ethdev.h>
#include <cnxk_security.h>
#include <cnxk_security_ar.h>

struct cn9k_eth_txq {
	uint64_t send_hdr_w0;
	int64_t fc_cache_pkts;
	uint64_t *fc_mem;
	void *lmt_addr;
	rte_iova_t io_addr;
	uint64_t lso_tun_fmt;
	uint64_t ts_mem;
	uint16_t sqes_per_sqb_log2;
	int16_t nb_sqb_bufs_adj;
	rte_iova_t cpt_io_addr;
	uint64_t sa_base;
	uint64_t *cpt_fc;
	uint16_t cpt_desc;
	uint64_t mark_flag : 8;
	uint64_t mark_fmt : 48;
	struct cnxk_eth_txq_comp tx_compl;
} __plt_cache_aligned;

struct cn9k_eth_rxq {
	uint64_t mbuf_initializer;
	uint64_t data_off;
	uintptr_t desc;
	void *lookup_mem;
	uintptr_t cq_door;
	uint64_t wdata;
	int64_t *cq_status;
	uint32_t head;
	uint32_t qmask;
	uint32_t available;
	uint16_t rq;
	struct cnxk_timesync_info *tstamp;
} __plt_cache_aligned;

/* Private data in sw rsvd area of struct roc_onf_ipsec_inb_sa */
struct cn9k_inb_priv_data {
	void *userdata;
	uint32_t replay_win_sz;
	struct cnxk_on_ipsec_ar ar;
	struct cnxk_eth_sec_sess *eth_sec;
};

/* Private data in sw rsvd area of struct roc_onf_ipsec_outb_sa */
struct cn9k_outb_priv_data {
	union {
		uint64_t esn;
		struct {
			uint32_t seq;
			uint32_t esn_hi;
		};
	};

	/* Rlen computation data */
	struct cnxk_ipsec_outb_rlens rlens;

	/* IP identifier */
	uint16_t ip_id;

	/* SA index */
	uint32_t sa_idx;

	/* Flags */
	uint16_t copy_salt : 1;

	/* Salt */
	uint32_t nonce;

	/* User data pointer */
	void *userdata;

	/* Back pointer to eth sec session */
	struct cnxk_eth_sec_sess *eth_sec;

	/* IV in DBG mode */
	uint8_t iv_dbg[ROC_IE_ON_MAX_IV_LEN];
};

struct cn9k_sec_sess_priv {
	union {
		struct {
			uint32_t sa_idx;
			uint8_t inb_sa : 1;
			uint8_t rsvd1 : 2;
			uint8_t roundup_byte : 5;
			uint8_t roundup_len;
			uint16_t partial_len;
		};

		uint64_t u64;
	};
} __rte_packed;

/* Rx and Tx routines */
void cn9k_eth_set_rx_function(struct rte_eth_dev *eth_dev);
void cn9k_eth_set_tx_function(struct rte_eth_dev *eth_dev);

/* Security context setup */
void cn9k_eth_sec_ops_override(void);

static inline uint16_t
nix_tx_compl_nb_pkts(struct cn9k_eth_txq *txq, const uint64_t wdata,
		const uint32_t qmask)
{
	uint16_t available = txq->tx_compl.available;

	/* Update the available count if cached value is not enough */
	if (!unlikely(available)) {
		uint64_t reg, head, tail;

		/* Use LDADDA version to avoid reorder */
		reg = roc_atomic64_add_sync(wdata, txq->tx_compl.cq_status);
		/* CQ_OP_STATUS operation error */
		if (reg & BIT_ULL(NIX_CQ_OP_STAT_OP_ERR) ||
				reg & BIT_ULL(NIX_CQ_OP_STAT_CQ_ERR))
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
handle_tx_completion_pkts(struct cn9k_eth_txq *txq, uint8_t mt_safe)
{
#define CNXK_NIX_CQ_ENTRY_SZ 128
#define CQE_SZ(x)            ((x) * CNXK_NIX_CQ_ENTRY_SZ)

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
		rte_prefetch_non_temporal((void *)(desc +
					(CQE_SZ((head + 2) & qmask))));
		tx_compl_cq = (struct nix_cqe_hdr_s *)
			(desc + CQE_SZ(head));
		tx_compl_s0 = (struct nix_send_comp_s *)
			((uint64_t *)tx_compl_cq + 1);
		m = txq->tx_compl.ptr[tx_compl_s0->sqe_id];
		while (m->next != NULL) {
			m_next = m->next;
			rte_pktmbuf_free_seg(m);
			m = m_next;
		}
		rte_pktmbuf_free_seg(m);

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


#endif /* __CN9K_ETHDEV_H__ */
