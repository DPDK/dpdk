/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#ifndef __CN10K_TX_WORKER_H__
#define __CN10K_TX_WORKER_H__

#include "cn10k_tx.h"
#include "cn10k_eventdev.h"
#include "cnxk_eventdev_dp.h"
#include <rte_event_eth_tx_adapter.h>

/* CN10K Tx event fastpath */

static __rte_always_inline struct cn10k_eth_txq *
cn10k_sso_hws_xtract_meta(struct rte_mbuf *m, const uint64_t *txq_data)
{
	return (struct cn10k_eth_txq
			*)(txq_data[(txq_data[m->port] >> 48) +
				    rte_event_eth_tx_adapter_txq_get(m)] &
			   (BIT_ULL(48) - 1));
}

static __rte_always_inline void
cn10k_sso_txq_fc_wait(const struct cn10k_eth_txq *txq)
{
	int64_t avail;

#ifdef RTE_ARCH_ARM64
	int64_t val;

	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldxr %[val], [%[addr]]			\n"
		     "		sub %[val], %[adj], %[val]		\n"
		     "		lsl %[refill], %[val], %[shft]		\n"
		     "		sub %[refill], %[refill], %[val]	\n"
		     "		cmp %[refill], #0x0			\n"
		     "		b.gt .Ldne%=				\n"
		     "		sevl					\n"
		     ".Lrty%=:	wfe					\n"
		     "		ldxr %[val], [%[addr]]			\n"
		     "		sub %[val], %[adj], %[val]		\n"
		     "		lsl %[refill], %[val], %[shft]		\n"
		     "		sub %[refill], %[refill], %[val]	\n"
		     "		cmp %[refill], #0x0			\n"
		     "		b.le .Lrty%=				\n"
		     ".Ldne%=:						\n"
		     : [refill] "=&r"(avail), [val] "=&r" (val)
		     : [addr] "r"(txq->fc_mem), [adj] "r"(txq->nb_sqb_bufs_adj),
		       [shft] "r"(txq->sqes_per_sqb_log2)
		     : "memory");
#else
	do {
		avail = txq->nb_sqb_bufs_adj - __atomic_load_n(txq->fc_mem, __ATOMIC_RELAXED);
	} while (((avail << txq->sqes_per_sqb_log2) - avail) <= 0);
#endif
}

static __rte_always_inline int32_t
cn10k_sso_sq_depth(const struct cn10k_eth_txq *txq)
{
	int32_t avail = (int32_t)txq->nb_sqb_bufs_adj -
			(int32_t)__atomic_load_n(txq->fc_mem, __ATOMIC_RELAXED);
	return (avail << txq->sqes_per_sqb_log2) - avail;
}

static __rte_always_inline uint16_t
cn10k_sso_tx_one(struct cn10k_sso_hws *ws, struct rte_mbuf *m, uint64_t *cmd,
		 uint16_t lmt_id, uintptr_t lmt_addr, uint8_t sched_type,
		 const uint64_t *txq_data, const uint32_t flags)
{
	uint8_t lnum = 0, loff = 0, shft = 0;
	struct rte_mbuf *extm = NULL;
	struct cn10k_eth_txq *txq;
	uintptr_t laddr;
	uint16_t segdw;
	uintptr_t pa;
	bool sec;

	txq = cn10k_sso_hws_xtract_meta(m, txq_data);
	if (cn10k_sso_sq_depth(txq) <= 0)
		return 0;

	if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F && txq->tx_compl.ena)
		handle_tx_completion_pkts(txq, 1);

	cn10k_nix_tx_skeleton(txq, cmd, flags, 0);
	/* Perform header writes before barrier
	 * for TSO
	 */
	if (flags & NIX_TX_OFFLOAD_TSO_F)
		cn10k_nix_xmit_prepare_tso(m, flags);

	cn10k_nix_xmit_prepare(txq, m, &extm, cmd, flags, txq->lso_tun_fmt, &sec,
			       txq->mark_flag, txq->mark_fmt);

	laddr = lmt_addr;
	/* Prepare CPT instruction and get nixtx addr if
	 * it is for CPT on same lmtline.
	 */
	if (flags & NIX_TX_OFFLOAD_SECURITY_F && sec)
		cn10k_nix_prep_sec(m, cmd, &laddr, lmt_addr, &lnum, &loff,
				   &shft, txq->sa_base, flags);

	/* Move NIX desc to LMT/NIXTX area */
	cn10k_nix_xmit_mv_lmt_base(laddr, cmd, flags);

	if (flags & NIX_TX_MULTI_SEG_F)
		segdw = cn10k_nix_prepare_mseg(txq, m, &extm, (uint64_t *)laddr, flags);
	else
		segdw = cn10k_nix_tx_ext_subs(flags) + 2;

	cn10k_nix_xmit_prepare_tstamp(txq, laddr, m->ol_flags, segdw, flags);
	if (flags & NIX_TX_OFFLOAD_SECURITY_F && sec)
		pa = txq->cpt_io_addr | 3 << 4;
	else
		pa = txq->io_addr | ((segdw - 1) << 4);

	if (!CNXK_TAG_IS_HEAD(ws->gw_rdata) && !sched_type)
		ws->gw_rdata = roc_sso_hws_head_wait(ws->base);

	cn10k_sso_txq_fc_wait(txq);
	if (flags & NIX_TX_OFFLOAD_SECURITY_F && sec)
		cn10k_nix_sec_fc_wait_one(txq);

	roc_lmt_submit_steorl(lmt_id, pa);

	/* Memory barrier to make sure lmtst store completes */
	rte_io_wmb();

	if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F && !txq->tx_compl.ena)
		cn10k_nix_free_extmbuf(extm);

	return 1;
}

static __rte_always_inline uint16_t
cn10k_sso_vwqe_split_tx(struct cn10k_sso_hws *ws, struct rte_mbuf **mbufs,
			uint16_t nb_mbufs, uint64_t *cmd,
			const uint64_t *txq_data, const uint32_t flags)
{
	uint16_t count = 0, port, queue, ret = 0, last_idx = 0;
	struct cn10k_eth_txq *txq;
	int32_t space;
	int i;

	port = mbufs[0]->port;
	queue = rte_event_eth_tx_adapter_txq_get(mbufs[0]);
	for (i = 0; i < nb_mbufs; i++) {
		if (port != mbufs[i]->port ||
		    queue != rte_event_eth_tx_adapter_txq_get(mbufs[i])) {
			if (count) {
				txq = (struct cn10k_eth_txq
					       *)(txq_data[(txq_data[port] >>
							    48) +
							   queue] &
						  (BIT_ULL(48) - 1));
				/* Transmit based on queue depth */
				space = cn10k_sso_sq_depth(txq);
				if (space < count)
					goto done;
				cn10k_nix_xmit_pkts_vector(
					txq, (uint64_t *)ws, &mbufs[last_idx],
					count, cmd, flags | NIX_TX_VWQE_F);
				ret += count;
				count = 0;
			}
			port = mbufs[i]->port;
			queue = rte_event_eth_tx_adapter_txq_get(mbufs[i]);
			last_idx = i;
		}
		count++;
	}
	if (count) {
		txq = (struct cn10k_eth_txq
			       *)(txq_data[(txq_data[port] >> 48) + queue] &
				  (BIT_ULL(48) - 1));
		/* Transmit based on queue depth */
		space = cn10k_sso_sq_depth(txq);
		if (space < count)
			goto done;
		cn10k_nix_xmit_pkts_vector(txq, (uint64_t *)ws,
					   &mbufs[last_idx], count, cmd,
					   flags | NIX_TX_VWQE_F);
		ret += count;
	}
done:
	return ret;
}

static __rte_always_inline uint16_t
cn10k_sso_hws_event_tx(struct cn10k_sso_hws *ws, struct rte_event *ev,
		       uint64_t *cmd, const uint64_t *txq_data,
		       const uint32_t flags)
{
	struct cn10k_eth_txq *txq;
	struct rte_mbuf *m;
	uintptr_t lmt_addr;
	uint16_t lmt_id;

	lmt_addr = ws->lmt_base;
	ROC_LMT_BASE_ID_GET(lmt_addr, lmt_id);

	if (ev->event_type & RTE_EVENT_TYPE_VECTOR) {
		struct rte_mbuf **mbufs = ev->vec->mbufs;
		uint64_t meta = *(uint64_t *)ev->vec;
		uint16_t offset, nb_pkts, left;
		int32_t space;

		nb_pkts = meta & 0xFFFF;
		offset = (meta >> 16) & 0xFFF;
		if (meta & BIT(31)) {
			txq = (struct cn10k_eth_txq
				       *)(txq_data[(txq_data[meta >> 32] >>
						    48) +
						   (meta >> 48)] &
					  (BIT_ULL(48) - 1));

			/* Transmit based on queue depth */
			space = cn10k_sso_sq_depth(txq);
			if (space <= 0)
				return 0;
			nb_pkts = nb_pkts < space ? nb_pkts : (uint16_t)space;
			cn10k_nix_xmit_pkts_vector(txq, (uint64_t *)ws,
						   mbufs + offset, nb_pkts, cmd,
						   flags | NIX_TX_VWQE_F);
		} else {
			nb_pkts = cn10k_sso_vwqe_split_tx(ws, mbufs + offset,
							  nb_pkts, cmd,
							  txq_data, flags);
		}
		left = (meta & 0xFFFF) - nb_pkts;

		if (!left) {
			rte_mempool_put(rte_mempool_from_obj(ev->vec), ev->vec);
		} else {
			*(uint64_t *)ev->vec =
				(meta & ~0xFFFFFFFUL) |
				(((uint32_t)nb_pkts + offset) << 16) | left;
		}
		rte_prefetch0(ws);
		return !left;
	}

	m = ev->mbuf;
	return cn10k_sso_tx_one(ws, m, cmd, lmt_id, lmt_addr, ev->sched_type,
				txq_data, flags);
}

#define T(name, sz, flags)                                                     \
	uint16_t __rte_hot cn10k_sso_hws_tx_adptr_enq_##name(                  \
		void *port, struct rte_event ev[], uint16_t nb_events);        \
	uint16_t __rte_hot cn10k_sso_hws_tx_adptr_enq_seg_##name(              \
		void *port, struct rte_event ev[], uint16_t nb_events);

NIX_TX_FASTPATH_MODES
#undef T

#define SSO_TX(fn, sz, flags)                                                  \
	uint16_t __rte_hot fn(void *port, struct rte_event ev[],               \
			      uint16_t nb_events)                              \
	{                                                                      \
		struct cn10k_sso_hws *ws = port;                               \
		uint64_t cmd[sz];                                              \
		RTE_SET_USED(nb_events);                                       \
		return cn10k_sso_hws_event_tx(                                 \
			ws, &ev[0], cmd, (const uint64_t *)ws->tx_adptr_data,  \
			flags);                                                \
	}

#define SSO_TX_SEG(fn, sz, flags)                                              \
	uint16_t __rte_hot fn(void *port, struct rte_event ev[],               \
			      uint16_t nb_events)                              \
	{                                                                      \
		uint64_t cmd[(sz) + CNXK_NIX_TX_MSEG_SG_DWORDS - 2];           \
		struct cn10k_sso_hws *ws = port;                               \
		RTE_SET_USED(nb_events);                                       \
		return cn10k_sso_hws_event_tx(                                 \
			ws, &ev[0], cmd, (const uint64_t *)ws->tx_adptr_data,  \
			(flags) | NIX_TX_MULTI_SEG_F);                         \
	}

#endif
