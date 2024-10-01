/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */
#ifndef __CN20K_TX_H__
#define __CN20K_TX_H__

#include "cn20k_rxtx.h"
#include <rte_eventdev.h>
#include <rte_vect.h>

#define NIX_TX_OFFLOAD_NONE	      (0)
#define NIX_TX_OFFLOAD_L3_L4_CSUM_F   BIT(0)
#define NIX_TX_OFFLOAD_OL3_OL4_CSUM_F BIT(1)
#define NIX_TX_OFFLOAD_VLAN_QINQ_F    BIT(2)
#define NIX_TX_OFFLOAD_MBUF_NOFF_F    BIT(3)
#define NIX_TX_OFFLOAD_TSO_F	      BIT(4)
#define NIX_TX_OFFLOAD_TSTAMP_F	      BIT(5)
#define NIX_TX_OFFLOAD_SECURITY_F     BIT(6)
#define NIX_TX_OFFLOAD_MAX	      (NIX_TX_OFFLOAD_SECURITY_F << 1)

/* Flags to control xmit_prepare function.
 * Defining it from backwards to denote its been
 * not used as offload flags to pick function
 */
#define NIX_TX_VWQE_F	   BIT(14)
#define NIX_TX_MULTI_SEG_F BIT(15)

#define NIX_TX_NEED_SEND_HDR_W1                                                                    \
	(NIX_TX_OFFLOAD_L3_L4_CSUM_F | NIX_TX_OFFLOAD_OL3_OL4_CSUM_F |                             \
	 NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_TSO_F)

#define NIX_TX_NEED_EXT_HDR                                                                        \
	(NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_TSTAMP_F | NIX_TX_OFFLOAD_TSO_F)

#define NIX_XMIT_FC_OR_RETURN(txq, pkts)                                                           \
	do {                                                                                       \
		int64_t avail;                                                                     \
		/* Cached value is low, Update the fc_cache_pkts */                                \
		if (unlikely((txq)->fc_cache_pkts < (pkts))) {                                     \
			avail = txq->nb_sqb_bufs_adj - *txq->fc_mem;                               \
			/* Multiply with sqe_per_sqb to express in pkts */                         \
			(txq)->fc_cache_pkts = (avail << (txq)->sqes_per_sqb_log2) - avail;        \
			/* Check it again for the room */                                          \
			if (unlikely((txq)->fc_cache_pkts < (pkts)))                               \
				return 0;                                                          \
		}                                                                                  \
	} while (0)

#define NIX_XMIT_FC_OR_RETURN_MTS(txq, pkts)                                                       \
	do {                                                                                       \
		int64_t *fc_cache = &(txq)->fc_cache_pkts;                                         \
		uint8_t retry_count = 8;                                                           \
		int64_t val, newval;                                                               \
	retry:                                                                                     \
		/* Reduce the cached count */                                                      \
		val = (int64_t)__atomic_fetch_sub(fc_cache, pkts, __ATOMIC_RELAXED);               \
		val -= pkts;                                                                       \
		/* Cached value is low, Update the fc_cache_pkts */                                \
		if (unlikely(val < 0)) {                                                           \
			/* Multiply with sqe_per_sqb to express in pkts */                         \
			newval = txq->nb_sqb_bufs_adj -                                            \
				 __atomic_load_n(txq->fc_mem, __ATOMIC_RELAXED);                   \
			newval = (newval << (txq)->sqes_per_sqb_log2) - newval;                    \
			newval -= pkts;                                                            \
			if (!__atomic_compare_exchange_n(fc_cache, &val, newval, false,            \
							 __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {    \
				if (retry_count) {                                                 \
					retry_count--;                                             \
					goto retry;                                                \
				} else                                                             \
					return 0;                                                  \
			}                                                                          \
			/* Update and check it again for the room */                               \
			if (unlikely(newval < 0))                                                  \
				return 0;                                                          \
		}                                                                                  \
	} while (0)

#define NIX_XMIT_FC_CHECK_RETURN(txq, pkts)                                                        \
	do {                                                                                       \
		if (unlikely((txq)->flag))                                                         \
			NIX_XMIT_FC_OR_RETURN_MTS(txq, pkts);                                      \
		else {                                                                             \
			NIX_XMIT_FC_OR_RETURN(txq, pkts);                                          \
			/* Reduce the cached count */                                              \
			txq->fc_cache_pkts -= pkts;                                                \
		}                                                                                  \
	} while (0)

/* Encoded number of segments to number of dwords macro, each value of nb_segs
 * is encoded as 4bits.
 */
#define NIX_SEGDW_MAGIC 0x76654432210ULL

#define NIX_NB_SEGS_TO_SEGDW(x) ((NIX_SEGDW_MAGIC >> ((x) << 2)) & 0xF)

static __plt_always_inline uint8_t
cn20k_nix_mbuf_sg_dwords(struct rte_mbuf *m)
{
	uint32_t nb_segs = m->nb_segs;
	uint16_t aura0, aura;
	int segw, sg_segs;

	aura0 = roc_npa_aura_handle_to_aura(m->pool->pool_id);

	nb_segs--;
	segw = 2;
	sg_segs = 1;
	while (nb_segs) {
		m = m->next;
		aura = roc_npa_aura_handle_to_aura(m->pool->pool_id);
		if (aura != aura0) {
			segw += 2 + (sg_segs == 2);
			sg_segs = 0;
		} else {
			segw += (sg_segs == 0); /* SUBDC */
			segw += 1;		/* IOVA */
			sg_segs += 1;
			sg_segs %= 3;
		}
		nb_segs--;
	}

	return (segw + 1) / 2;
}

static __plt_always_inline void
cn20k_nix_tx_mbuf_validate(struct rte_mbuf *m, const uint32_t flags)
{
#ifdef RTE_LIBRTE_MBUF_DEBUG
	uint16_t segdw;

	segdw = cn20k_nix_mbuf_sg_dwords(m);
	segdw += 1 + !!(flags & NIX_TX_NEED_EXT_HDR) + !!(flags & NIX_TX_OFFLOAD_TSTAMP_F);

	PLT_ASSERT(segdw <= 8);
#else
	RTE_SET_USED(m);
	RTE_SET_USED(flags);
#endif
}

static __plt_always_inline void
cn20k_nix_vwqe_wait_fc(struct cn20k_eth_txq *txq, uint16_t req)
{
	int64_t cached, refill;
	int64_t pkts;

retry:
#ifdef RTE_ARCH_ARM64

	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldxr %[pkts], [%[addr]]			\n"
		     "		tbz %[pkts], 63, .Ldne%=		\n"
		     "		sevl					\n"
		     ".Lrty%=:	wfe					\n"
		     "		ldxr %[pkts], [%[addr]]			\n"
		     "		tbnz %[pkts], 63, .Lrty%=		\n"
		     ".Ldne%=:						\n"
		     : [pkts] "=&r"(pkts)
		     : [addr] "r"(&txq->fc_cache_pkts)
		     : "memory");
#else
	RTE_SET_USED(pkts);
	while (__atomic_load_n(&txq->fc_cache_pkts, __ATOMIC_RELAXED) < 0)
		;
#endif
	cached = __atomic_fetch_sub(&txq->fc_cache_pkts, req, __ATOMIC_ACQUIRE) - req;
	/* Check if there is enough space, else update and retry. */
	if (cached >= 0)
		return;

	/* Check if we have space else retry. */
#ifdef RTE_ARCH_ARM64
	int64_t val;

	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldxr %[val], [%[addr]]			\n"
		     "		sub %[val], %[adj], %[val]		\n"
		     "		lsl %[refill], %[val], %[shft]		\n"
		     "		sub %[refill], %[refill], %[val]	\n"
		     "		sub %[refill], %[refill], %[sub]	\n"
		     "		cmp %[refill], #0x0			\n"
		     "		b.ge .Ldne%=				\n"
		     "		sevl					\n"
		     ".Lrty%=:	wfe					\n"
		     "		ldxr %[val], [%[addr]]			\n"
		     "		sub %[val], %[adj], %[val]		\n"
		     "		lsl %[refill], %[val], %[shft]		\n"
		     "		sub %[refill], %[refill], %[val]	\n"
		     "		sub %[refill], %[refill], %[sub]	\n"
		     "		cmp %[refill], #0x0			\n"
		     "		b.lt .Lrty%=				\n"
		     ".Ldne%=:						\n"
		     : [refill] "=&r"(refill), [val] "=&r" (val)
		     : [addr] "r"(txq->fc_mem), [adj] "r"(txq->nb_sqb_bufs_adj),
		       [shft] "r"(txq->sqes_per_sqb_log2), [sub] "r"(req)
		     : "memory");
#else
	do {
		refill = (txq->nb_sqb_bufs_adj - __atomic_load_n(txq->fc_mem, __ATOMIC_RELAXED));
		refill = (refill << txq->sqes_per_sqb_log2) - refill;
		refill -= req;
	} while (refill < 0);
#endif
	if (!__atomic_compare_exchange(&txq->fc_cache_pkts, &cached, &refill, 0, __ATOMIC_RELEASE,
				       __ATOMIC_RELAXED))
		goto retry;
}

/* Function to determine no of tx subdesc required in case ext
 * sub desc is enabled.
 */
static __rte_always_inline int
cn20k_nix_tx_ext_subs(const uint16_t flags)
{
	return (flags & NIX_TX_OFFLOAD_TSTAMP_F) ?
		       2 :
		       ((flags & (NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_TSO_F)) ? 1 : 0);
}

static __rte_always_inline uint64_t
cn20k_nix_tx_steor_data(const uint16_t flags)
{
	const uint64_t dw_m1 = cn20k_nix_tx_ext_subs(flags) + 1;
	uint64_t data;

	/* This will be moved to addr area */
	data = dw_m1;
	/* 15 vector sizes for single seg */
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
cn20k_nix_tx_skeleton(struct cn20k_eth_txq *txq, uint64_t *cmd, const uint16_t flags,
		      const uint16_t static_sz)
{
	if (static_sz)
		cmd[0] = txq->send_hdr_w0;
	else
		cmd[0] = (txq->send_hdr_w0 & 0xFFFFF00000000000) |
			 ((uint64_t)(cn20k_nix_tx_ext_subs(flags) + 1) << 40);
	cmd[1] = 0;

	if (flags & NIX_TX_NEED_EXT_HDR) {
		if (flags & NIX_TX_OFFLOAD_TSTAMP_F)
			cmd[2] = (NIX_SUBDC_EXT << 60) | BIT_ULL(15);
		else
			cmd[2] = NIX_SUBDC_EXT << 60;
		cmd[3] = 0;
		if (!(flags & NIX_TX_OFFLOAD_MBUF_NOFF_F))
			cmd[4] = (NIX_SUBDC_SG << 60) | (NIX_SENDLDTYPE_LDWB << 58) | BIT_ULL(48);
		else
			cmd[4] = (NIX_SUBDC_SG << 60) | BIT_ULL(48);
	} else {
		if (!(flags & NIX_TX_OFFLOAD_MBUF_NOFF_F))
			cmd[2] = (NIX_SUBDC_SG << 60) | (NIX_SENDLDTYPE_LDWB << 58) | BIT_ULL(48);
		else
			cmd[2] = (NIX_SUBDC_SG << 60) | BIT_ULL(48);
	}
}

static __rte_always_inline void
cn20k_nix_sec_fc_wait(struct cn20k_eth_txq *txq, uint16_t nb_pkts)
{
	int32_t nb_desc, val, newval;
	int32_t *fc_sw;
	uint64_t *fc;

	/* Check if there is any CPT instruction to submit */
	if (!nb_pkts)
		return;

again:
	fc_sw = txq->cpt_fc_sw;
#ifdef RTE_ARCH_ARM64
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldxr %w[pkts], [%[addr]]		\n"
		     "		tbz %w[pkts], 31, .Ldne%=		\n"
		     "		sevl					\n"
		     ".Lrty%=:	wfe					\n"
		     "		ldxr %w[pkts], [%[addr]]		\n"
		     "		tbnz %w[pkts], 31, .Lrty%=		\n"
		     ".Ldne%=:						\n"
		     : [pkts] "=&r"(val)
		     : [addr] "r"(fc_sw)
		     : "memory");
#else
	/* Wait for primary core to refill FC. */
	while (__atomic_load_n(fc_sw, __ATOMIC_RELAXED) < 0)
		;
#endif

	val = __atomic_fetch_sub(fc_sw, nb_pkts, __ATOMIC_ACQUIRE) - nb_pkts;
	if (likely(val >= 0))
		return;

	nb_desc = txq->cpt_desc;
	fc = txq->cpt_fc;
#ifdef RTE_ARCH_ARM64
	asm volatile(PLT_CPU_FEATURE_PREAMBLE
		     "		ldxr %[refill], [%[addr]]		\n"
		     "		sub %[refill], %[desc], %[refill]	\n"
		     "		sub %[refill], %[refill], %[pkts]	\n"
		     "		cmp %[refill], #0x0			\n"
		     "		b.ge .Ldne%=				\n"
		     "		sevl					\n"
		     ".Lrty%=:	wfe					\n"
		     "		ldxr %[refill], [%[addr]]		\n"
		     "		sub %[refill], %[desc], %[refill]	\n"
		     "		sub %[refill], %[refill], %[pkts]	\n"
		     "		cmp %[refill], #0x0			\n"
		     "		b.lt .Lrty%=				\n"
		     ".Ldne%=:						\n"
		     : [refill] "=&r"(newval)
		     : [addr] "r"(fc), [desc] "r"(nb_desc), [pkts] "r"(nb_pkts)
		     : "memory");
#else
	while (true) {
		newval = nb_desc - __atomic_load_n(fc, __ATOMIC_RELAXED);
		newval -= nb_pkts;
		if (newval >= 0)
			break;
	}
#endif

	if (!__atomic_compare_exchange_n(fc_sw, &val, newval, false, __ATOMIC_RELEASE,
					 __ATOMIC_RELAXED))
		goto again;
}

#if defined(RTE_ARCH_ARM64)

static __rte_always_inline void
cn20k_nix_prep_sec(struct rte_mbuf *m, uint64_t *cmd, uintptr_t *nixtx_addr, uintptr_t lbase,
		   uint8_t *lnum, uint8_t *loff, uint8_t *shft, uint64_t sa_base,
		   const uint16_t flags)
{
	struct cn20k_sec_sess_priv sess_priv;
	uint32_t pkt_len, dlen_adj, rlen;
	struct nix_send_hdr_s *send_hdr;
	uint8_t l3l4type, chksum;
	uint64x2_t cmd01, cmd23;
	union nix_send_sg_s *sg;
	uint8_t l2_len, l3_len;
	uintptr_t dptr, nixtx;
	uint64_t ucode_cmd[4];
	uint64_t *laddr, w0;
	uint16_t tag;
	uint64_t sa;

	/* Move to our line from base */
	sess_priv.u64 = *rte_security_dynfield(m);
	send_hdr = (struct nix_send_hdr_s *)cmd;
	if (flags & NIX_TX_NEED_EXT_HDR)
		sg = (union nix_send_sg_s *)&cmd[4];
	else
		sg = (union nix_send_sg_s *)&cmd[2];

	if (flags & NIX_TX_NEED_SEND_HDR_W1) {
		/* Extract l3l4type either from il3il4type or ol3ol4type */
		if (flags & NIX_TX_OFFLOAD_L3_L4_CSUM_F && flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F) {
			l2_len = (cmd[1] >> 16) & 0xFF;
			/* L4 ptr from send hdr includes l2 and l3 len */
			l3_len = ((cmd[1] >> 24) & 0xFF) - l2_len;
			l3l4type = (cmd[1] >> 40) & 0xFF;
		} else {
			l2_len = cmd[1] & 0xFF;
			/* L4 ptr from send hdr includes l2 and l3 len */
			l3_len = ((cmd[1] >> 8) & 0xFF) - l2_len;
			l3l4type = (cmd[1] >> 32) & 0xFF;
		}

		chksum = (l3l4type & 0x1) << 1 | !!(l3l4type & 0x30);
		chksum = ~chksum;
		sess_priv.chksum = sess_priv.chksum & chksum;
		/* Clear SEND header flags */
		cmd[1] &= ~(0xFFFFUL << 32);
	} else {
		l2_len = m->l2_len;
		l3_len = m->l3_len;
	}

	/* Retrieve DPTR */
	dptr = *(uint64_t *)(sg + 1);
	pkt_len = send_hdr->w0.total;

	/* Calculate dlen adj */
	dlen_adj = pkt_len - l2_len;
	/* Exclude l3 len from roundup for transport mode */
	dlen_adj -= sess_priv.mode ? 0 : l3_len;
	rlen = (dlen_adj + sess_priv.roundup_len) + (sess_priv.roundup_byte - 1);
	rlen &= ~(uint64_t)(sess_priv.roundup_byte - 1);
	rlen += sess_priv.partial_len;
	dlen_adj = rlen - dlen_adj;

	/* Update send descriptors. Security is single segment only */
	send_hdr->w0.total = pkt_len + dlen_adj;

	/* CPT word 5 and word 6 */
	w0 = 0;
	ucode_cmd[2] = 0;
	if (flags & NIX_TX_MULTI_SEG_F && m->nb_segs > 1) {
		struct rte_mbuf *last = rte_pktmbuf_lastseg(m);

		/* Get area where NIX descriptor needs to be stored */
		nixtx = rte_pktmbuf_mtod_offset(last, uintptr_t, last->data_len + dlen_adj);
		nixtx += BIT_ULL(7);
		nixtx = (nixtx - 1) & ~(BIT_ULL(7) - 1);
		nixtx += 16;

		dptr = nixtx + ((flags & NIX_TX_NEED_EXT_HDR) ? 32 : 16);

		/* Set l2 length as data offset */
		w0 = (uint64_t)l2_len << 16;
		w0 |= cn20k_nix_tx_ext_subs(flags) + NIX_NB_SEGS_TO_SEGDW(m->nb_segs);
		ucode_cmd[1] = dptr | ((uint64_t)m->nb_segs << 60);
	} else {
		/* Get area where NIX descriptor needs to be stored */
		nixtx = dptr + pkt_len + dlen_adj;
		nixtx += BIT_ULL(7);
		nixtx = (nixtx - 1) & ~(BIT_ULL(7) - 1);
		nixtx += 16;

		w0 |= cn20k_nix_tx_ext_subs(flags) + 1ULL;
		dptr += l2_len;
		ucode_cmd[1] = dptr;
		sg->seg1_size = pkt_len + dlen_adj;
		pkt_len -= l2_len;
	}
	w0 |= ((((int64_t)nixtx - (int64_t)dptr) & 0xFFFFF) << 32);
	/* CPT word 0 and 1 */
	cmd01 = vdupq_n_u64(0);
	cmd01 = vsetq_lane_u64(w0, cmd01, 0);
	/* CPT_RES_S is 16B above NIXTX */
	cmd01 = vsetq_lane_u64(nixtx - 16, cmd01, 1);

	/* Return nixtx addr */
	*nixtx_addr = nixtx;

	/* CPT Word 4 and Word 7 */
	tag = sa_base & 0xFFFFUL;
	sa_base &= ~0xFFFFUL;
	sa = (uintptr_t)roc_nix_inl_ot_ipsec_outb_sa(sa_base, sess_priv.sa_idx);
	ucode_cmd[3] = (ROC_CPT_DFLT_ENG_GRP_SE_IE << 61 | 1UL << 60 | sa);
	ucode_cmd[0] = (ROC_IE_OT_MAJOR_OP_PROCESS_OUTBOUND_IPSEC << 48 | 1UL << 54 |
			((uint64_t)sess_priv.chksum) << 32 | ((uint64_t)sess_priv.dec_ttl) << 34 |
			pkt_len);

	/* CPT word 2 and 3 */
	cmd23 = vdupq_n_u64(0);
	cmd23 = vsetq_lane_u64((((uint64_t)RTE_EVENT_TYPE_CPU << 28) | tag |
				CNXK_ETHDEV_SEC_OUTB_EV_SUB << 20), cmd23, 0);
	cmd23 = vsetq_lane_u64((uintptr_t)m | 1, cmd23, 1);

	/* Move to our line */
	laddr = LMT_OFF(lbase, *lnum, *loff ? 64 : 0);

	/* Write CPT instruction to lmt line */
	vst1q_u64(laddr, cmd01);
	vst1q_u64((laddr + 2), cmd23);

	*(__uint128_t *)(laddr + 4) = *(__uint128_t *)ucode_cmd;
	*(__uint128_t *)(laddr + 6) = *(__uint128_t *)(ucode_cmd + 2);

	/* Move to next line for every other CPT inst */
	*loff = !(*loff);
	*lnum = *lnum + (*loff ? 0 : 1);
	*shft = *shft + (*loff ? 0 : 3);
}

#else

static __rte_always_inline void
cn20k_nix_prep_sec(struct rte_mbuf *m, uint64_t *cmd, uintptr_t *nixtx_addr, uintptr_t lbase,
		   uint8_t *lnum, uint8_t *loff, uint8_t *shft, uint64_t sa_base,
		   const uint16_t flags)
{
	RTE_SET_USED(m);
	RTE_SET_USED(cmd);
	RTE_SET_USED(nixtx_addr);
	RTE_SET_USED(lbase);
	RTE_SET_USED(lnum);
	RTE_SET_USED(loff);
	RTE_SET_USED(shft);
	RTE_SET_USED(sa_base);
	RTE_SET_USED(flags);
}
#endif

static inline void
cn20k_nix_free_extmbuf(struct rte_mbuf *m)
{
	struct rte_mbuf *m_next;
	while (m != NULL) {
		m_next = m->next;
		rte_pktmbuf_free_seg(m);
		m = m_next;
	}
}

static __rte_always_inline uint64_t
cn20k_nix_prefree_seg(struct rte_mbuf *m, struct rte_mbuf **extm, struct cn20k_eth_txq *txq,
		      struct nix_send_hdr_s *send_hdr, uint64_t *aura)
{
	struct rte_mbuf *prev = NULL;
	uint32_t sqe_id;

	if (RTE_MBUF_HAS_EXTBUF(m)) {
		if (unlikely(txq->tx_compl.ena == 0)) {
			m->next = *extm;
			*extm = m;
			return 1;
		}
		if (send_hdr->w0.pnc) {
			sqe_id = send_hdr->w1.sqe_id;
			prev = txq->tx_compl.ptr[sqe_id];
			m->next = prev;
			txq->tx_compl.ptr[sqe_id] = m;
		} else {
			sqe_id = __atomic_fetch_add(&txq->tx_compl.sqe_id, 1, __ATOMIC_RELAXED);
			send_hdr->w0.pnc = 1;
			send_hdr->w1.sqe_id = sqe_id & txq->tx_compl.nb_desc_mask;
			txq->tx_compl.ptr[send_hdr->w1.sqe_id] = m;
		}
		return 1;
	} else {
		return cnxk_nix_prefree_seg(m, aura);
	}
}

static __rte_always_inline void
cn20k_nix_xmit_prepare_tso(struct rte_mbuf *m, const uint64_t flags)
{
	uint64_t mask, ol_flags = m->ol_flags;

	if (flags & NIX_TX_OFFLOAD_TSO_F && (ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
		uintptr_t mdata = rte_pktmbuf_mtod(m, uintptr_t);
		uint16_t *iplen, *oiplen, *oudplen;
		uint16_t lso_sb, paylen;

		mask = -!!(ol_flags & (RTE_MBUF_F_TX_OUTER_IPV4 | RTE_MBUF_F_TX_OUTER_IPV6));
		lso_sb = (mask & (m->outer_l2_len + m->outer_l3_len)) + m->l2_len + m->l3_len +
			 m->l4_len;

		/* Reduce payload len from base headers */
		paylen = m->pkt_len - lso_sb;

		/* Get iplen position assuming no tunnel hdr */
		iplen = (uint16_t *)(mdata + m->l2_len + (2 << !!(ol_flags & RTE_MBUF_F_TX_IPV6)));
		/* Handle tunnel tso */
		if ((flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F) &&
		    (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)) {
			const uint8_t is_udp_tun =
				(CNXK_NIX_UDP_TUN_BITMASK >>
				 ((ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) >> 45)) &
				0x1;

			oiplen = (uint16_t *)(mdata + m->outer_l2_len +
					      (2 << !!(ol_flags & RTE_MBUF_F_TX_OUTER_IPV6)));
			*oiplen = rte_cpu_to_be_16(rte_be_to_cpu_16(*oiplen) - paylen);

			/* Update format for UDP tunneled packet */
			if (is_udp_tun) {
				oudplen =
					(uint16_t *)(mdata + m->outer_l2_len + m->outer_l3_len + 4);
				*oudplen = rte_cpu_to_be_16(rte_be_to_cpu_16(*oudplen) - paylen);
			}

			/* Update iplen position to inner ip hdr */
			iplen = (uint16_t *)(mdata + lso_sb - m->l3_len - m->l4_len +
					     (2 << !!(ol_flags & RTE_MBUF_F_TX_IPV6)));
		}

		*iplen = rte_cpu_to_be_16(rte_be_to_cpu_16(*iplen) - paylen);
	}
}

static __rte_always_inline void
cn20k_nix_xmit_prepare(struct cn20k_eth_txq *txq, struct rte_mbuf *m, struct rte_mbuf **extm,
		       uint64_t *cmd, const uint16_t flags, const uint64_t lso_tun_fmt, bool *sec,
		       uint8_t mark_flag, uint64_t mark_fmt)
{
	uint8_t mark_off = 0, mark_vlan = 0, markptr = 0;
	struct nix_send_ext_s *send_hdr_ext;
	struct nix_send_hdr_s *send_hdr;
	uint64_t ol_flags = 0, mask;
	union nix_send_hdr_w1_u w1;
	union nix_send_sg_s *sg;
	uint16_t mark_form = 0;

	send_hdr = (struct nix_send_hdr_s *)cmd;
	if (flags & NIX_TX_NEED_EXT_HDR) {
		send_hdr_ext = (struct nix_send_ext_s *)(cmd + 2);
		sg = (union nix_send_sg_s *)(cmd + 4);
		/* Clear previous markings */
		send_hdr_ext->w0.lso = 0;
		send_hdr_ext->w0.mark_en = 0;
		send_hdr_ext->w1.u = 0;
		ol_flags = m->ol_flags;
	} else {
		sg = (union nix_send_sg_s *)(cmd + 2);
	}

	if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F)
		send_hdr->w0.pnc = 0;

	if (flags & (NIX_TX_NEED_SEND_HDR_W1 | NIX_TX_OFFLOAD_SECURITY_F)) {
		ol_flags = m->ol_flags;
		w1.u = 0;
	}

	if (!(flags & NIX_TX_MULTI_SEG_F))
		send_hdr->w0.total = m->data_len;
	else
		send_hdr->w0.total = m->pkt_len;
	send_hdr->w0.aura = roc_npa_aura_handle_to_aura(m->pool->pool_id);

	/*
	 * L3type:  2 => IPV4
	 *          3 => IPV4 with csum
	 *          4 => IPV6
	 * L3type and L3ptr needs to be set for either
	 * L3 csum or L4 csum or LSO
	 *
	 */

	if ((flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F) && (flags & NIX_TX_OFFLOAD_L3_L4_CSUM_F)) {
		const uint8_t csum = !!(ol_flags & RTE_MBUF_F_TX_OUTER_UDP_CKSUM);
		const uint8_t ol3type = ((!!(ol_flags & RTE_MBUF_F_TX_OUTER_IPV4)) << 1) +
					((!!(ol_flags & RTE_MBUF_F_TX_OUTER_IPV6)) << 2) +
					!!(ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM);

		/* Outer L3 */
		w1.ol3type = ol3type;
		mask = 0xffffull << ((!!ol3type) << 4);
		w1.ol3ptr = ~mask & m->outer_l2_len;
		w1.ol4ptr = ~mask & (w1.ol3ptr + m->outer_l3_len);

		/* Outer L4 */
		w1.ol4type = csum + (csum << 1);

		/* Inner L3 */
		w1.il3type = ((!!(ol_flags & RTE_MBUF_F_TX_IPV4)) << 1) +
			     ((!!(ol_flags & RTE_MBUF_F_TX_IPV6)) << 2);
		w1.il3ptr = w1.ol4ptr + m->l2_len;
		w1.il4ptr = w1.il3ptr + m->l3_len;
		/* Increment it by 1 if it is IPV4 as 3 is with csum */
		w1.il3type = w1.il3type + !!(ol_flags & RTE_MBUF_F_TX_IP_CKSUM);

		/* Inner L4 */
		w1.il4type = (ol_flags & RTE_MBUF_F_TX_L4_MASK) >> 52;

		/* In case of no tunnel header use only
		 * shift IL3/IL4 fields a bit to use
		 * OL3/OL4 for header checksum
		 */
		mask = !ol3type;
		w1.u = ((w1.u & 0xFFFFFFFF00000000) >> (mask << 3)) |
		       ((w1.u & 0X00000000FFFFFFFF) >> (mask << 4));

	} else if (flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F) {
		const uint8_t csum = !!(ol_flags & RTE_MBUF_F_TX_OUTER_UDP_CKSUM);
		const uint8_t outer_l2_len = m->outer_l2_len;

		/* Outer L3 */
		w1.ol3ptr = outer_l2_len;
		w1.ol4ptr = outer_l2_len + m->outer_l3_len;
		/* Increment it by 1 if it is IPV4 as 3 is with csum */
		w1.ol3type = ((!!(ol_flags & RTE_MBUF_F_TX_OUTER_IPV4)) << 1) +
			     ((!!(ol_flags & RTE_MBUF_F_TX_OUTER_IPV6)) << 2) +
			     !!(ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM);

		/* Outer L4 */
		w1.ol4type = csum + (csum << 1);

	} else if (flags & NIX_TX_OFFLOAD_L3_L4_CSUM_F) {
		const uint8_t l2_len = m->l2_len;

		/* Always use OLXPTR and OLXTYPE when only
		 * when one header is present
		 */

		/* Inner L3 */
		w1.ol3ptr = l2_len;
		w1.ol4ptr = l2_len + m->l3_len;
		/* Increment it by 1 if it is IPV4 as 3 is with csum */
		w1.ol3type = ((!!(ol_flags & RTE_MBUF_F_TX_IPV4)) << 1) +
			     ((!!(ol_flags & RTE_MBUF_F_TX_IPV6)) << 2) +
			     !!(ol_flags & RTE_MBUF_F_TX_IP_CKSUM);

		/* Inner L4 */
		w1.ol4type = (ol_flags & RTE_MBUF_F_TX_L4_MASK) >> 52;
	}

	if (flags & NIX_TX_NEED_EXT_HDR && flags & NIX_TX_OFFLOAD_VLAN_QINQ_F) {
		const uint8_t ipv6 = !!(ol_flags & RTE_MBUF_F_TX_IPV6);
		const uint8_t ip = !!(ol_flags & (RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IPV6));

		send_hdr_ext->w1.vlan1_ins_ena = !!(ol_flags & RTE_MBUF_F_TX_VLAN);
		/* HW will update ptr after vlan0 update */
		send_hdr_ext->w1.vlan1_ins_ptr = 12;
		send_hdr_ext->w1.vlan1_ins_tci = m->vlan_tci;

		send_hdr_ext->w1.vlan0_ins_ena = !!(ol_flags & RTE_MBUF_F_TX_QINQ);
		/* 2B before end of l2 header */
		send_hdr_ext->w1.vlan0_ins_ptr = 12;
		send_hdr_ext->w1.vlan0_ins_tci = m->vlan_tci_outer;
		/* Fill for VLAN marking only when VLAN insertion enabled */
		mark_vlan = ((mark_flag & CNXK_TM_MARK_VLAN_DEI) &
			     (send_hdr_ext->w1.vlan1_ins_ena || send_hdr_ext->w1.vlan0_ins_ena));

		/* Mask requested flags with packet data information */
		mark_off = mark_flag & ((ip << 2) | (ip << 1) | mark_vlan);
		mark_off = ffs(mark_off & CNXK_TM_MARK_MASK);

		mark_form = (mark_fmt >> ((mark_off - !!mark_off) << 4));
		mark_form = (mark_form >> (ipv6 << 3)) & 0xFF;
		markptr = m->l2_len + (mark_form >> 7) - (mark_vlan << 2);

		send_hdr_ext->w0.mark_en = !!mark_off;
		send_hdr_ext->w0.markform = mark_form & 0x7F;
		send_hdr_ext->w0.markptr = markptr;
	}

	if (flags & NIX_TX_NEED_EXT_HDR && flags & NIX_TX_OFFLOAD_TSO_F &&
	    (ol_flags & RTE_MBUF_F_TX_TCP_SEG)) {
		uint16_t lso_sb;
		uint64_t mask;

		mask = -(!w1.il3type);
		lso_sb = (mask & w1.ol4ptr) + (~mask & w1.il4ptr) + m->l4_len;

		send_hdr_ext->w0.lso_sb = lso_sb;
		send_hdr_ext->w0.lso = 1;
		send_hdr_ext->w0.lso_mps = m->tso_segsz;
		send_hdr_ext->w0.lso_format =
			NIX_LSO_FORMAT_IDX_TSOV4 + !!(ol_flags & RTE_MBUF_F_TX_IPV6);
		w1.ol4type = NIX_SENDL4TYPE_TCP_CKSUM;

		/* Handle tunnel tso */
		if ((flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F) &&
		    (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)) {
			const uint8_t is_udp_tun =
				(CNXK_NIX_UDP_TUN_BITMASK >>
				 ((ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) >> 45)) &
				0x1;
			uint8_t shift = is_udp_tun ? 32 : 0;

			shift += (!!(ol_flags & RTE_MBUF_F_TX_OUTER_IPV6) << 4);
			shift += (!!(ol_flags & RTE_MBUF_F_TX_IPV6) << 3);

			w1.il4type = NIX_SENDL4TYPE_TCP_CKSUM;
			w1.ol4type = is_udp_tun ? NIX_SENDL4TYPE_UDP_CKSUM : 0;
			/* Update format for UDP tunneled packet */
			send_hdr_ext->w0.lso_format = (lso_tun_fmt >> shift);
		}
	}

	if (flags & NIX_TX_NEED_SEND_HDR_W1)
		send_hdr->w1.u = w1.u;

	if (!(flags & NIX_TX_MULTI_SEG_F)) {
		struct rte_mbuf *cookie;

		sg->seg1_size = send_hdr->w0.total;
		*(rte_iova_t *)(sg + 1) = rte_mbuf_data_iova(m);
		cookie = RTE_MBUF_DIRECT(m) ? m : rte_mbuf_from_indirect(m);

		if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F) {
			uint64_t aura;

			/* DF bit = 1 if refcount of current mbuf or parent mbuf
			 *		is greater than 1
			 * DF bit = 0 otherwise
			 */
			aura = send_hdr->w0.aura;
			send_hdr->w0.df = cn20k_nix_prefree_seg(m, extm, txq, send_hdr, &aura);
			send_hdr->w0.aura = aura;
		}
#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
		/* Mark mempool object as "put" since it is freed by NIX */
		if (!send_hdr->w0.df)
			RTE_MEMPOOL_CHECK_COOKIES(cookie->pool, (void **)&cookie, 1, 0);
#else
		RTE_SET_USED(cookie);
#endif
	} else {
		sg->seg1_size = m->data_len;
		*(rte_iova_t *)(sg + 1) = rte_mbuf_data_iova(m);

		/* NOFF is handled later for multi-seg */
	}

	if (flags & NIX_TX_OFFLOAD_SECURITY_F)
		*sec = !!(ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD);
}

static __rte_always_inline void
cn20k_nix_xmit_mv_lmt_base(uintptr_t lmt_addr, uint64_t *cmd, const uint16_t flags)
{
	struct nix_send_ext_s *send_hdr_ext;
	union nix_send_sg_s *sg;

	/* With minimal offloads, 'cmd' being local could be optimized out to
	 * registers. In other cases, 'cmd' will be in stack. Intent is
	 * 'cmd' stores content from txq->cmd which is copied only once.
	 */
	*((struct nix_send_hdr_s *)lmt_addr) = *(struct nix_send_hdr_s *)cmd;
	lmt_addr += 16;
	if (flags & NIX_TX_NEED_EXT_HDR) {
		send_hdr_ext = (struct nix_send_ext_s *)(cmd + 2);
		*((struct nix_send_ext_s *)lmt_addr) = *send_hdr_ext;
		lmt_addr += 16;

		sg = (union nix_send_sg_s *)(cmd + 4);
	} else {
		sg = (union nix_send_sg_s *)(cmd + 2);
	}
	/* In case of multi-seg, sg template is stored here */
	*((union nix_send_sg_s *)lmt_addr) = *sg;
	*(rte_iova_t *)(lmt_addr + 8) = *(rte_iova_t *)(sg + 1);
}

static __rte_always_inline void
cn20k_nix_xmit_prepare_tstamp(struct cn20k_eth_txq *txq, uintptr_t lmt_addr,
			      const uint64_t ol_flags, const uint16_t no_segdw,
			      const uint16_t flags)
{
	if (flags & NIX_TX_OFFLOAD_TSTAMP_F) {
		const uint8_t is_ol_tstamp = !(ol_flags & RTE_MBUF_F_TX_IEEE1588_TMST);
		uint64_t *lmt = (uint64_t *)lmt_addr;
		uint16_t off = (no_segdw - 1) << 1;
		struct nix_send_mem_s *send_mem;

		send_mem = (struct nix_send_mem_s *)(lmt + off);
		/* Packets for which PKT_TX_IEEE1588_TMST is not set, tx tstamp
		 * should not be recorded, hence changing the alg type to
		 * NIX_SENDMEMALG_SUB and also changing send mem addr field to
		 * next 8 bytes as it corrupts the actual Tx tstamp registered
		 * address.
		 */
		send_mem->w0.subdc = NIX_SUBDC_MEM;
		send_mem->w0.alg = NIX_SENDMEMALG_SETTSTMP + (is_ol_tstamp << 3);
		send_mem->addr = (rte_iova_t)(((uint64_t *)txq->ts_mem) + is_ol_tstamp);
	}
}

static __rte_always_inline uint16_t
cn20k_nix_prepare_mseg(struct cn20k_eth_txq *txq, struct rte_mbuf *m, struct rte_mbuf **extm,
		       uint64_t *cmd, const uint16_t flags)
{
	uint64_t prefree = 0, aura0, aura, nb_segs, segdw;
	struct nix_send_hdr_s *send_hdr;
	union nix_send_sg_s *sg, l_sg;
	union nix_send_sg2_s l_sg2;
	struct rte_mbuf *cookie;
	struct rte_mbuf *m_next;
	uint8_t off, is_sg2;
	uint64_t len, dlen;
	uint64_t ol_flags;
	uint64_t *slist;

	send_hdr = (struct nix_send_hdr_s *)cmd;

	if (flags & NIX_TX_NEED_EXT_HDR)
		off = 2;
	else
		off = 0;

	sg = (union nix_send_sg_s *)&cmd[2 + off];
	len = send_hdr->w0.total;
	if (flags & NIX_TX_OFFLOAD_SECURITY_F)
		ol_flags = m->ol_flags;

	/* Start from second segment, first segment is already there */
	dlen = m->data_len;
	is_sg2 = 0;
	l_sg.u = sg->u;
	/* Clear l_sg.u first seg length that might be stale from vector path */
	l_sg.u &= ~0xFFFFUL;
	l_sg.u |= dlen;
	len -= dlen;
	nb_segs = m->nb_segs - 1;
	m_next = m->next;
	m->next = NULL;
	m->nb_segs = 1;
	slist = &cmd[3 + off + 1];

	cookie = RTE_MBUF_DIRECT(m) ? m : rte_mbuf_from_indirect(m);
	/* Set invert df if buffer is not to be freed by H/W */
	if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F) {
		aura = send_hdr->w0.aura;
		prefree = cn20k_nix_prefree_seg(m, extm, txq, send_hdr, &aura);
		send_hdr->w0.aura = aura;
		l_sg.i1 = prefree;
	}

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
	/* Mark mempool object as "put" since it is freed by NIX */
	if (!prefree)
		RTE_MEMPOOL_CHECK_COOKIES(cookie->pool, (void **)&cookie, 1, 0);
	rte_io_wmb();
#else
	RTE_SET_USED(cookie);
#endif

	/* Quickly handle single segmented packets. With this if-condition
	 * compiler will completely optimize out the below do-while loop
	 * from the Tx handler when NIX_TX_MULTI_SEG_F offload is not set.
	 */
	if (!(flags & NIX_TX_MULTI_SEG_F))
		goto done;

	aura0 = send_hdr->w0.aura;
	m = m_next;
	if (!m)
		goto done;

	/* Fill mbuf segments */
	do {
		uint64_t iova;

		/* Save the current mbuf properties. These can get cleared in
		 * cnxk_nix_prefree_seg()
		 */
		m_next = m->next;
		iova = rte_mbuf_data_iova(m);
		dlen = m->data_len;
		len -= dlen;

		nb_segs--;
		aura = aura0;
		prefree = 0;

		m->next = NULL;

		cookie = RTE_MBUF_DIRECT(m) ? m : rte_mbuf_from_indirect(m);
		if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F) {
			aura = roc_npa_aura_handle_to_aura(m->pool->pool_id);
			prefree = cn20k_nix_prefree_seg(m, extm, txq, send_hdr, &aura);
			is_sg2 = aura != aura0 && !prefree;
		}

		if (unlikely(is_sg2)) {
			/* This mbuf belongs to a different pool and
			 * DF bit is not to be set, so use SG2 subdesc
			 * so that it is freed to the appropriate pool.
			 */

			/* Write the previous descriptor out */
			sg->u = l_sg.u;

			/* If the current SG subdc does not have any
			 * iovas in it, then the SG2 subdc can overwrite
			 * that SG subdc.
			 *
			 * If the current SG subdc has 2 iovas in it, then
			 * the current iova word should be left empty.
			 */
			slist += (-1 + (int)l_sg.segs);
			sg = (union nix_send_sg_s *)slist;

			l_sg2.u = l_sg.u & 0xC00000000000000; /* LD_TYPE */
			l_sg2.subdc = NIX_SUBDC_SG2;
			l_sg2.aura = aura;
			l_sg2.seg1_size = dlen;
			l_sg.u = l_sg2.u;

			slist++;
			*slist = iova;
			slist++;
		} else {
			*slist = iova;
			/* Set invert df if buffer is not to be freed by H/W */
			l_sg.u |= (prefree << (l_sg.segs + 55));
			/* Set the segment length */
			l_sg.u |= ((uint64_t)dlen << (l_sg.segs << 4));
			l_sg.segs += 1;
			slist++;
		}

		if ((is_sg2 || l_sg.segs > 2) && nb_segs) {
			sg->u = l_sg.u;
			/* Next SG subdesc */
			sg = (union nix_send_sg_s *)slist;
			l_sg.u &= 0xC00000000000000; /* LD_TYPE */
			l_sg.subdc = NIX_SUBDC_SG;
			slist++;
		}

#ifdef RTE_LIBRTE_MEMPOOL_DEBUG
		/* Mark mempool object as "put" since it is freed by NIX
		 */
		if (!prefree)
			RTE_MEMPOOL_CHECK_COOKIES(cookie->pool, (void **)&cookie, 1, 0);
#else
		RTE_SET_USED(cookie);
#endif
		m = m_next;
	} while (nb_segs);

done:
	/* Add remaining bytes of security data to last seg */
	if (flags & NIX_TX_OFFLOAD_SECURITY_F && ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD && len) {
		uint8_t shft = (l_sg.subdc == NIX_SUBDC_SG) ? ((l_sg.segs - 1) << 4) : 0;

		dlen = ((l_sg.u >> shft) & 0xFFFFULL) + len;
		l_sg.u = l_sg.u & ~(0xFFFFULL << shft);
		l_sg.u |= dlen << shft;
	}

	/* Write the last subdc out */
	sg->u = l_sg.u;

	segdw = (uint64_t *)slist - (uint64_t *)&cmd[2 + off];
	/* Roundup extra dwords to multiple of 2 */
	segdw = (segdw >> 1) + (segdw & 0x1);
	/* Default dwords */
	segdw += (off >> 1) + 1 + !!(flags & NIX_TX_OFFLOAD_TSTAMP_F);
	send_hdr->w0.sizem1 = segdw - 1;

	return segdw;
}

static __rte_always_inline uint16_t
cn20k_nix_xmit_pkts(void *tx_queue, uint64_t *ws, struct rte_mbuf **tx_pkts, uint16_t pkts,
		    uint64_t *cmd, const uint16_t flags)
{
	struct cn20k_eth_txq *txq = tx_queue;
	const rte_iova_t io_addr = txq->io_addr;
	uint8_t lnum, c_lnum, c_shft, c_loff;
	uintptr_t pa, lbase = txq->lmt_base;
	uint16_t lmt_id, burst, left, i;
	struct rte_mbuf *extm = NULL;
	uintptr_t c_lbase = lbase;
	uint64_t lso_tun_fmt = 0;
	uint64_t mark_fmt = 0;
	uint8_t mark_flag = 0;
	rte_iova_t c_io_addr;
	uint16_t c_lmt_id;
	uint64_t sa_base;
	uintptr_t laddr;
	uint64_t data;
	bool sec;

	if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F && txq->tx_compl.ena)
		handle_tx_completion_pkts(txq, flags & NIX_TX_VWQE_F);

	if (!(flags & NIX_TX_VWQE_F))
		NIX_XMIT_FC_CHECK_RETURN(txq, pkts);

	/* Get cmd skeleton */
	cn20k_nix_tx_skeleton(txq, cmd, flags, !(flags & NIX_TX_VWQE_F));

	if (flags & NIX_TX_OFFLOAD_TSO_F)
		lso_tun_fmt = txq->lso_tun_fmt;

	if (flags & NIX_TX_OFFLOAD_VLAN_QINQ_F) {
		mark_fmt = txq->mark_fmt;
		mark_flag = txq->mark_flag;
	}

	/* Get LMT base address and LMT ID as lcore id */
	ROC_LMT_BASE_ID_GET(lbase, lmt_id);
	if (flags & NIX_TX_OFFLOAD_SECURITY_F) {
		ROC_LMT_CPT_BASE_ID_GET(c_lbase, c_lmt_id);
		c_io_addr = txq->cpt_io_addr;
		sa_base = txq->sa_base;
	}

	left = pkts;
again:
	burst = left > 32 ? 32 : left;

	lnum = 0;
	if (flags & NIX_TX_OFFLOAD_SECURITY_F) {
		c_lnum = 0;
		c_loff = 0;
		c_shft = 16;
	}

	for (i = 0; i < burst; i++) {
		/* Perform header writes for TSO, barrier at
		 * lmt steorl will suffice.
		 */
		if (flags & NIX_TX_OFFLOAD_TSO_F)
			cn20k_nix_xmit_prepare_tso(tx_pkts[i], flags);

		cn20k_nix_xmit_prepare(txq, tx_pkts[i], &extm, cmd, flags, lso_tun_fmt, &sec,
				       mark_flag, mark_fmt);

		laddr = (uintptr_t)LMT_OFF(lbase, lnum, 0);

		/* Prepare CPT instruction and get nixtx addr */
		if (flags & NIX_TX_OFFLOAD_SECURITY_F && sec)
			cn20k_nix_prep_sec(tx_pkts[i], cmd, &laddr, c_lbase, &c_lnum, &c_loff,
					   &c_shft, sa_base, flags);

		/* Move NIX desc to LMT/NIXTX area */
		cn20k_nix_xmit_mv_lmt_base(laddr, cmd, flags);
		cn20k_nix_xmit_prepare_tstamp(txq, laddr, tx_pkts[i]->ol_flags, 4, flags);
		if (!(flags & NIX_TX_OFFLOAD_SECURITY_F) || !sec)
			lnum++;
	}

	if ((flags & NIX_TX_VWQE_F) && !(ws[3] & BIT_ULL(35)))
		ws[3] = roc_sso_hws_head_wait(ws[0]);

	left -= burst;
	tx_pkts += burst;

	/* Submit CPT instructions if any */
	if (flags & NIX_TX_OFFLOAD_SECURITY_F) {
		uint16_t sec_pkts = ((c_lnum << 1) + c_loff);

		/* Reduce pkts to be sent to CPT */
		burst -= sec_pkts;
		if (flags & NIX_TX_VWQE_F)
			cn20k_nix_vwqe_wait_fc(txq, sec_pkts);
		cn20k_nix_sec_fc_wait(txq, sec_pkts);
		cn20k_nix_sec_steorl(c_io_addr, c_lmt_id, c_lnum, c_loff, c_shft);
	}

	/* Trigger LMTST */
	if (burst > 16) {
		data = cn20k_nix_tx_steor_data(flags);
		pa = io_addr | (data & 0x7) << 4;
		data &= ~0x7ULL;
		data |= (15ULL << 12);
		data |= (uint64_t)lmt_id;

		if (flags & NIX_TX_VWQE_F)
			cn20k_nix_vwqe_wait_fc(txq, 16);
		/* STEOR0 */
		roc_lmt_submit_steorl(data, pa);

		data = cn20k_nix_tx_steor_data(flags);
		pa = io_addr | (data & 0x7) << 4;
		data &= ~0x7ULL;
		data |= ((uint64_t)(burst - 17)) << 12;
		data |= (uint64_t)(lmt_id + 16);

		if (flags & NIX_TX_VWQE_F)
			cn20k_nix_vwqe_wait_fc(txq, burst - 16);
		/* STEOR1 */
		roc_lmt_submit_steorl(data, pa);
	} else if (burst) {
		data = cn20k_nix_tx_steor_data(flags);
		pa = io_addr | (data & 0x7) << 4;
		data &= ~0x7ULL;
		data |= ((uint64_t)(burst - 1)) << 12;
		data |= (uint64_t)lmt_id;

		if (flags & NIX_TX_VWQE_F)
			cn20k_nix_vwqe_wait_fc(txq, burst);
		/* STEOR0 */
		roc_lmt_submit_steorl(data, pa);
	}

	rte_io_wmb();
	if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F && !txq->tx_compl.ena) {
		cn20k_nix_free_extmbuf(extm);
		extm = NULL;
	}

	if (left)
		goto again;

	return pkts;
}

static __rte_always_inline uint16_t
cn20k_nix_xmit_pkts_mseg(void *tx_queue, uint64_t *ws, struct rte_mbuf **tx_pkts, uint16_t pkts,
			 uint64_t *cmd, const uint16_t flags)
{
	struct cn20k_eth_txq *txq = tx_queue;
	uintptr_t pa0, pa1, lbase = txq->lmt_base;
	const rte_iova_t io_addr = txq->io_addr;
	uint16_t segdw, lmt_id, burst, left, i;
	struct rte_mbuf *extm = NULL;
	uint8_t lnum, c_lnum, c_loff;
	uintptr_t c_lbase = lbase;
	uint64_t lso_tun_fmt = 0;
	uint64_t mark_fmt = 0;
	uint8_t mark_flag = 0;
	uint64_t data0, data1;
	rte_iova_t c_io_addr;
	uint8_t shft, c_shft;
	__uint128_t data128;
	uint16_t c_lmt_id;
	uint64_t sa_base;
	uintptr_t laddr;
	bool sec;

	if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F && txq->tx_compl.ena)
		handle_tx_completion_pkts(txq, flags & NIX_TX_VWQE_F);

	if (!(flags & NIX_TX_VWQE_F))
		NIX_XMIT_FC_CHECK_RETURN(txq, pkts);

	/* Get cmd skeleton */
	cn20k_nix_tx_skeleton(txq, cmd, flags, !(flags & NIX_TX_VWQE_F));

	if (flags & NIX_TX_OFFLOAD_TSO_F)
		lso_tun_fmt = txq->lso_tun_fmt;

	if (flags & NIX_TX_OFFLOAD_VLAN_QINQ_F) {
		mark_fmt = txq->mark_fmt;
		mark_flag = txq->mark_flag;
	}

	/* Get LMT base address and LMT ID as lcore id */
	ROC_LMT_BASE_ID_GET(lbase, lmt_id);
	if (flags & NIX_TX_OFFLOAD_SECURITY_F) {
		ROC_LMT_CPT_BASE_ID_GET(c_lbase, c_lmt_id);
		c_io_addr = txq->cpt_io_addr;
		sa_base = txq->sa_base;
	}

	left = pkts;
again:
	burst = left > 32 ? 32 : left;
	shft = 16;
	data128 = 0;

	lnum = 0;
	if (flags & NIX_TX_OFFLOAD_SECURITY_F) {
		c_lnum = 0;
		c_loff = 0;
		c_shft = 16;
	}

	for (i = 0; i < burst; i++) {
		cn20k_nix_tx_mbuf_validate(tx_pkts[i], flags);

		/* Perform header writes for TSO, barrier at
		 * lmt steorl will suffice.
		 */
		if (flags & NIX_TX_OFFLOAD_TSO_F)
			cn20k_nix_xmit_prepare_tso(tx_pkts[i], flags);

		cn20k_nix_xmit_prepare(txq, tx_pkts[i], &extm, cmd, flags, lso_tun_fmt, &sec,
				       mark_flag, mark_fmt);

		laddr = (uintptr_t)LMT_OFF(lbase, lnum, 0);

		/* Prepare CPT instruction and get nixtx addr */
		if (flags & NIX_TX_OFFLOAD_SECURITY_F && sec)
			cn20k_nix_prep_sec(tx_pkts[i], cmd, &laddr, c_lbase, &c_lnum, &c_loff,
					   &c_shft, sa_base, flags);

		/* Move NIX desc to LMT/NIXTX area */
		cn20k_nix_xmit_mv_lmt_base(laddr, cmd, flags);
		/* Store sg list directly on lmt line */
		segdw = cn20k_nix_prepare_mseg(txq, tx_pkts[i], &extm, (uint64_t *)laddr, flags);
		cn20k_nix_xmit_prepare_tstamp(txq, laddr, tx_pkts[i]->ol_flags, segdw, flags);
		if (!(flags & NIX_TX_OFFLOAD_SECURITY_F) || !sec) {
			lnum++;
			data128 |= (((__uint128_t)(segdw - 1)) << shft);
			shft += 3;
		}
	}

	if ((flags & NIX_TX_VWQE_F) && !(ws[3] & BIT_ULL(35)))
		ws[3] = roc_sso_hws_head_wait(ws[0]);

	left -= burst;
	tx_pkts += burst;

	/* Submit CPT instructions if any */
	if (flags & NIX_TX_OFFLOAD_SECURITY_F) {
		uint16_t sec_pkts = ((c_lnum << 1) + c_loff);

		/* Reduce pkts to be sent to CPT */
		burst -= sec_pkts;
		if (flags & NIX_TX_VWQE_F)
			cn20k_nix_vwqe_wait_fc(txq, sec_pkts);
		cn20k_nix_sec_fc_wait(txq, sec_pkts);
		cn20k_nix_sec_steorl(c_io_addr, c_lmt_id, c_lnum, c_loff, c_shft);
	}

	data0 = (uint64_t)data128;
	data1 = (uint64_t)(data128 >> 64);
	/* Make data0 similar to data1 */
	data0 >>= 16;
	/* Trigger LMTST */
	if (burst > 16) {
		pa0 = io_addr | (data0 & 0x7) << 4;
		data0 &= ~0x7ULL;
		/* Move lmtst1..15 sz to bits 63:19 */
		data0 <<= 16;
		data0 |= (15ULL << 12);
		data0 |= (uint64_t)lmt_id;

		if (flags & NIX_TX_VWQE_F)
			cn20k_nix_vwqe_wait_fc(txq, 16);
		/* STEOR0 */
		roc_lmt_submit_steorl(data0, pa0);

		pa1 = io_addr | (data1 & 0x7) << 4;
		data1 &= ~0x7ULL;
		data1 <<= 16;
		data1 |= ((uint64_t)(burst - 17)) << 12;
		data1 |= (uint64_t)(lmt_id + 16);

		if (flags & NIX_TX_VWQE_F)
			cn20k_nix_vwqe_wait_fc(txq, burst - 16);
		/* STEOR1 */
		roc_lmt_submit_steorl(data1, pa1);
	} else if (burst) {
		pa0 = io_addr | (data0 & 0x7) << 4;
		data0 &= ~0x7ULL;
		/* Move lmtst1..15 sz to bits 63:19 */
		data0 <<= 16;
		data0 |= ((burst - 1ULL) << 12);
		data0 |= (uint64_t)lmt_id;

		if (flags & NIX_TX_VWQE_F)
			cn20k_nix_vwqe_wait_fc(txq, burst);
		/* STEOR0 */
		roc_lmt_submit_steorl(data0, pa0);
	}

	rte_io_wmb();
	if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F && !txq->tx_compl.ena) {
		cn20k_nix_free_extmbuf(extm);
		extm = NULL;
	}

	if (left)
		goto again;

	return pkts;
}

#define L3L4CSUM_F   NIX_TX_OFFLOAD_L3_L4_CSUM_F
#define OL3OL4CSUM_F NIX_TX_OFFLOAD_OL3_OL4_CSUM_F
#define VLAN_F	     NIX_TX_OFFLOAD_VLAN_QINQ_F
#define NOFF_F	     NIX_TX_OFFLOAD_MBUF_NOFF_F
#define TSO_F	     NIX_TX_OFFLOAD_TSO_F
#define TSP_F	     NIX_TX_OFFLOAD_TSTAMP_F
#define T_SEC_F	     NIX_TX_OFFLOAD_SECURITY_F

/* [T_SEC_F] [TSP] [TSO] [NOFF] [VLAN] [OL3OL4CSUM] [L3L4CSUM] */
#define NIX_TX_FASTPATH_MODES_0_15                                                                 \
	T(no_offload, 6, NIX_TX_OFFLOAD_NONE)                                                      \
	T(l3l4csum, 6, L3L4CSUM_F)                                                                 \
	T(ol3ol4csum, 6, OL3OL4CSUM_F)                                                             \
	T(ol3ol4csum_l3l4csum, 6, OL3OL4CSUM_F | L3L4CSUM_F)                                       \
	T(vlan, 6, VLAN_F)                                                                         \
	T(vlan_l3l4csum, 6, VLAN_F | L3L4CSUM_F)                                                   \
	T(vlan_ol3ol4csum, 6, VLAN_F | OL3OL4CSUM_F)                                               \
	T(vlan_ol3ol4csum_l3l4csum, 6, VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)                         \
	T(noff, 6, NOFF_F)                                                                         \
	T(noff_l3l4csum, 6, NOFF_F | L3L4CSUM_F)                                                   \
	T(noff_ol3ol4csum, 6, NOFF_F | OL3OL4CSUM_F)                                               \
	T(noff_ol3ol4csum_l3l4csum, 6, NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)                         \
	T(noff_vlan, 6, NOFF_F | VLAN_F)                                                           \
	T(noff_vlan_l3l4csum, 6, NOFF_F | VLAN_F | L3L4CSUM_F)                                     \
	T(noff_vlan_ol3ol4csum, 6, NOFF_F | VLAN_F | OL3OL4CSUM_F)                                 \
	T(noff_vlan_ol3ol4csum_l3l4csum, 6, NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_16_31                                                                \
	T(tso, 6, TSO_F)                                                                           \
	T(tso_l3l4csum, 6, TSO_F | L3L4CSUM_F)                                                     \
	T(tso_ol3ol4csum, 6, TSO_F | OL3OL4CSUM_F)                                                 \
	T(tso_ol3ol4csum_l3l4csum, 6, TSO_F | OL3OL4CSUM_F | L3L4CSUM_F)                           \
	T(tso_vlan, 6, TSO_F | VLAN_F)                                                             \
	T(tso_vlan_l3l4csum, 6, TSO_F | VLAN_F | L3L4CSUM_F)                                       \
	T(tso_vlan_ol3ol4csum, 6, TSO_F | VLAN_F | OL3OL4CSUM_F)                                   \
	T(tso_vlan_ol3ol4csum_l3l4csum, 6, TSO_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)             \
	T(tso_noff, 6, TSO_F | NOFF_F)                                                             \
	T(tso_noff_l3l4csum, 6, TSO_F | NOFF_F | L3L4CSUM_F)                                       \
	T(tso_noff_ol3ol4csum, 6, TSO_F | NOFF_F | OL3OL4CSUM_F)                                   \
	T(tso_noff_ol3ol4csum_l3l4csum, 6, TSO_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)             \
	T(tso_noff_vlan, 6, TSO_F | NOFF_F | VLAN_F)                                               \
	T(tso_noff_vlan_l3l4csum, 6, TSO_F | NOFF_F | VLAN_F | L3L4CSUM_F)                         \
	T(tso_noff_vlan_ol3ol4csum, 6, TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)                     \
	T(tso_noff_vlan_ol3ol4csum_l3l4csum, 6, TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_32_47                                                                \
	T(ts, 8, TSP_F)                                                                            \
	T(ts_l3l4csum, 8, TSP_F | L3L4CSUM_F)                                                      \
	T(ts_ol3ol4csum, 8, TSP_F | OL3OL4CSUM_F)                                                  \
	T(ts_ol3ol4csum_l3l4csum, 8, TSP_F | OL3OL4CSUM_F | L3L4CSUM_F)                            \
	T(ts_vlan, 8, TSP_F | VLAN_F)                                                              \
	T(ts_vlan_l3l4csum, 8, TSP_F | VLAN_F | L3L4CSUM_F)                                        \
	T(ts_vlan_ol3ol4csum, 8, TSP_F | VLAN_F | OL3OL4CSUM_F)                                    \
	T(ts_vlan_ol3ol4csum_l3l4csum, 8, TSP_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)              \
	T(ts_noff, 8, TSP_F | NOFF_F)                                                              \
	T(ts_noff_l3l4csum, 8, TSP_F | NOFF_F | L3L4CSUM_F)                                        \
	T(ts_noff_ol3ol4csum, 8, TSP_F | NOFF_F | OL3OL4CSUM_F)                                    \
	T(ts_noff_ol3ol4csum_l3l4csum, 8, TSP_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)              \
	T(ts_noff_vlan, 8, TSP_F | NOFF_F | VLAN_F)                                                \
	T(ts_noff_vlan_l3l4csum, 8, TSP_F | NOFF_F | VLAN_F | L3L4CSUM_F)                          \
	T(ts_noff_vlan_ol3ol4csum, 8, TSP_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)                      \
	T(ts_noff_vlan_ol3ol4csum_l3l4csum, 8, TSP_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_48_63                                                                \
	T(ts_tso, 8, TSP_F | TSO_F)                                                                \
	T(ts_tso_l3l4csum, 8, TSP_F | TSO_F | L3L4CSUM_F)                                          \
	T(ts_tso_ol3ol4csum, 8, TSP_F | TSO_F | OL3OL4CSUM_F)                                      \
	T(ts_tso_ol3ol4csum_l3l4csum, 8, TSP_F | TSO_F | OL3OL4CSUM_F | L3L4CSUM_F)                \
	T(ts_tso_vlan, 8, TSP_F | TSO_F | VLAN_F)                                                  \
	T(ts_tso_vlan_l3l4csum, 8, TSP_F | TSO_F | VLAN_F | L3L4CSUM_F)                            \
	T(ts_tso_vlan_ol3ol4csum, 8, TSP_F | TSO_F | VLAN_F | OL3OL4CSUM_F)                        \
	T(ts_tso_vlan_ol3ol4csum_l3l4csum, 8, TSP_F | TSO_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)  \
	T(ts_tso_noff, 8, TSP_F | TSO_F | NOFF_F)                                                  \
	T(ts_tso_noff_l3l4csum, 8, TSP_F | TSO_F | NOFF_F | L3L4CSUM_F)                            \
	T(ts_tso_noff_ol3ol4csum, 8, TSP_F | TSO_F | NOFF_F | OL3OL4CSUM_F)                        \
	T(ts_tso_noff_ol3ol4csum_l3l4csum, 8, TSP_F | TSO_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)  \
	T(ts_tso_noff_vlan, 8, TSP_F | TSO_F | NOFF_F | VLAN_F)                                    \
	T(ts_tso_noff_vlan_l3l4csum, 8, TSP_F | TSO_F | NOFF_F | VLAN_F | L3L4CSUM_F)              \
	T(ts_tso_noff_vlan_ol3ol4csum, 8, TSP_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)          \
	T(ts_tso_noff_vlan_ol3ol4csum_l3l4csum, 8,                                                 \
	  TSP_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_64_79                                                                \
	T(sec, 6, T_SEC_F)                                                                         \
	T(sec_l3l4csum, 6, T_SEC_F | L3L4CSUM_F)                                                   \
	T(sec_ol3ol4csum, 6, T_SEC_F | OL3OL4CSUM_F)                                               \
	T(sec_ol3ol4csum_l3l4csum, 6, T_SEC_F | OL3OL4CSUM_F | L3L4CSUM_F)                         \
	T(sec_vlan, 6, T_SEC_F | VLAN_F)                                                           \
	T(sec_vlan_l3l4csum, 6, T_SEC_F | VLAN_F | L3L4CSUM_F)                                     \
	T(sec_vlan_ol3ol4csum, 6, T_SEC_F | VLAN_F | OL3OL4CSUM_F)                                 \
	T(sec_vlan_ol3ol4csum_l3l4csum, 6, T_SEC_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)           \
	T(sec_noff, 6, T_SEC_F | NOFF_F)                                                           \
	T(sec_noff_l3l4csum, 6, T_SEC_F | NOFF_F | L3L4CSUM_F)                                     \
	T(sec_noff_ol3ol4csum, 6, T_SEC_F | NOFF_F | OL3OL4CSUM_F)                                 \
	T(sec_noff_ol3ol4csum_l3l4csum, 6, T_SEC_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)           \
	T(sec_noff_vlan, 6, T_SEC_F | NOFF_F | VLAN_F)                                             \
	T(sec_noff_vlan_l3l4csum, 6, T_SEC_F | NOFF_F | VLAN_F | L3L4CSUM_F)                       \
	T(sec_noff_vlan_ol3ol4csum, 6, T_SEC_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)                   \
	T(sec_noff_vlan_ol3ol4csum_l3l4csum, 6,                                                    \
	  T_SEC_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_80_95                                                                \
	T(sec_tso, 6, T_SEC_F | TSO_F)                                                             \
	T(sec_tso_l3l4csum, 6, T_SEC_F | TSO_F | L3L4CSUM_F)                                       \
	T(sec_tso_ol3ol4csum, 6, T_SEC_F | TSO_F | OL3OL4CSUM_F)                                   \
	T(sec_tso_ol3ol4csum_l3l4csum, 6, T_SEC_F | TSO_F | OL3OL4CSUM_F | L3L4CSUM_F)             \
	T(sec_tso_vlan, 6, T_SEC_F | TSO_F | VLAN_F)                                               \
	T(sec_tso_vlan_l3l4csum, 6, T_SEC_F | TSO_F | VLAN_F | L3L4CSUM_F)                         \
	T(sec_tso_vlan_ol3ol4csum, 6, T_SEC_F | TSO_F | VLAN_F | OL3OL4CSUM_F)                     \
	T(sec_tso_vlan_ol3ol4csum_l3l4csum, 6,                                                     \
	  T_SEC_F | TSO_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)                                    \
	T(sec_tso_noff, 6, T_SEC_F | TSO_F | NOFF_F)                                               \
	T(sec_tso_noff_l3l4csum, 6, T_SEC_F | TSO_F | NOFF_F | L3L4CSUM_F)                         \
	T(sec_tso_noff_ol3ol4csum, 6, T_SEC_F | TSO_F | NOFF_F | OL3OL4CSUM_F)                     \
	T(sec_tso_noff_ol3ol4csum_l3l4csum, 6,                                                     \
	  T_SEC_F | TSO_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)                                    \
	T(sec_tso_noff_vlan, 6, T_SEC_F | TSO_F | NOFF_F | VLAN_F)                                 \
	T(sec_tso_noff_vlan_l3l4csum, 6, T_SEC_F | TSO_F | NOFF_F | VLAN_F | L3L4CSUM_F)           \
	T(sec_tso_noff_vlan_ol3ol4csum, 6, T_SEC_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)       \
	T(sec_tso_noff_vlan_ol3ol4csum_l3l4csum, 6,                                                \
	  T_SEC_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_96_111                                                               \
	T(sec_ts, 8, T_SEC_F | TSP_F)                                                              \
	T(sec_ts_l3l4csum, 8, T_SEC_F | TSP_F | L3L4CSUM_F)                                        \
	T(sec_ts_ol3ol4csum, 8, T_SEC_F | TSP_F | OL3OL4CSUM_F)                                    \
	T(sec_ts_ol3ol4csum_l3l4csum, 8, T_SEC_F | TSP_F | OL3OL4CSUM_F | L3L4CSUM_F)              \
	T(sec_ts_vlan, 8, T_SEC_F | TSP_F | VLAN_F)                                                \
	T(sec_ts_vlan_l3l4csum, 8, T_SEC_F | TSP_F | VLAN_F | L3L4CSUM_F)                          \
	T(sec_ts_vlan_ol3ol4csum, 8, T_SEC_F | TSP_F | VLAN_F | OL3OL4CSUM_F)                      \
	T(sec_ts_vlan_ol3ol4csum_l3l4csum, 8,                                                      \
	  T_SEC_F | TSP_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)                                    \
	T(sec_ts_noff, 8, T_SEC_F | TSP_F | NOFF_F)                                                \
	T(sec_ts_noff_l3l4csum, 8, T_SEC_F | TSP_F | NOFF_F | L3L4CSUM_F)                          \
	T(sec_ts_noff_ol3ol4csum, 8, T_SEC_F | TSP_F | NOFF_F | OL3OL4CSUM_F)                      \
	T(sec_ts_noff_ol3ol4csum_l3l4csum, 8,                                                      \
	  T_SEC_F | TSP_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)                                    \
	T(sec_ts_noff_vlan, 8, T_SEC_F | TSP_F | NOFF_F | VLAN_F)                                  \
	T(sec_ts_noff_vlan_l3l4csum, 8, T_SEC_F | TSP_F | NOFF_F | VLAN_F | L3L4CSUM_F)            \
	T(sec_ts_noff_vlan_ol3ol4csum, 8, T_SEC_F | TSP_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)        \
	T(sec_ts_noff_vlan_ol3ol4csum_l3l4csum, 8,                                                 \
	  T_SEC_F | TSP_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES_112_127                                                              \
	T(sec_ts_tso, 8, T_SEC_F | TSP_F | TSO_F)                                                  \
	T(sec_ts_tso_l3l4csum, 8, T_SEC_F | TSP_F | TSO_F | L3L4CSUM_F)                            \
	T(sec_ts_tso_ol3ol4csum, 8, T_SEC_F | TSP_F | TSO_F | OL3OL4CSUM_F)                        \
	T(sec_ts_tso_ol3ol4csum_l3l4csum, 8, T_SEC_F | TSP_F | TSO_F | OL3OL4CSUM_F | L3L4CSUM_F)  \
	T(sec_ts_tso_vlan, 8, T_SEC_F | TSP_F | TSO_F | VLAN_F)                                    \
	T(sec_ts_tso_vlan_l3l4csum, 8, T_SEC_F | TSP_F | TSO_F | VLAN_F | L3L4CSUM_F)              \
	T(sec_ts_tso_vlan_ol3ol4csum, 8, T_SEC_F | TSP_F | TSO_F | VLAN_F | OL3OL4CSUM_F)          \
	T(sec_ts_tso_vlan_ol3ol4csum_l3l4csum, 8,                                                  \
	  T_SEC_F | TSP_F | TSO_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)                            \
	T(sec_ts_tso_noff, 8, T_SEC_F | TSP_F | TSO_F | NOFF_F)                                    \
	T(sec_ts_tso_noff_l3l4csum, 8, T_SEC_F | TSP_F | TSO_F | NOFF_F | L3L4CSUM_F)              \
	T(sec_ts_tso_noff_ol3ol4csum, 8, T_SEC_F | TSP_F | TSO_F | NOFF_F | OL3OL4CSUM_F)          \
	T(sec_ts_tso_noff_ol3ol4csum_l3l4csum, 8,                                                  \
	  T_SEC_F | TSP_F | TSO_F | NOFF_F | OL3OL4CSUM_F | L3L4CSUM_F)                            \
	T(sec_ts_tso_noff_vlan, 8, T_SEC_F | TSP_F | TSO_F | NOFF_F | VLAN_F)                      \
	T(sec_ts_tso_noff_vlan_l3l4csum, 8,                                                        \
	  T_SEC_F | TSP_F | TSO_F | NOFF_F | VLAN_F | L3L4CSUM_F)                                  \
	T(sec_ts_tso_noff_vlan_ol3ol4csum, 8,                                                      \
	  T_SEC_F | TSP_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F)                                \
	T(sec_ts_tso_noff_vlan_ol3ol4csum_l3l4csum, 8,                                             \
	  T_SEC_F | TSP_F | TSO_F | NOFF_F | VLAN_F | OL3OL4CSUM_F | L3L4CSUM_F)

#define NIX_TX_FASTPATH_MODES                                                                      \
	NIX_TX_FASTPATH_MODES_0_15                                                                 \
	NIX_TX_FASTPATH_MODES_16_31                                                                \
	NIX_TX_FASTPATH_MODES_32_47                                                                \
	NIX_TX_FASTPATH_MODES_48_63                                                                \
	NIX_TX_FASTPATH_MODES_64_79                                                                \
	NIX_TX_FASTPATH_MODES_80_95                                                                \
	NIX_TX_FASTPATH_MODES_96_111                                                               \
	NIX_TX_FASTPATH_MODES_112_127

#define T(name, sz, flags)                                                                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_##name(                              \
		void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_mseg_##name(                         \
		void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_vec_##name(                          \
		void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts);                         \
	uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_vec_mseg_##name(                     \
		void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts);

NIX_TX_FASTPATH_MODES
#undef T

#define NIX_TX_XMIT(fn, sz, flags)                                                                 \
	uint16_t __rte_noinline __rte_hot fn(void *tx_queue, struct rte_mbuf **tx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		uint64_t cmd[sz];                                                                  \
		/* For TSO inner checksum is a must */                                             \
		if (((flags) & NIX_TX_OFFLOAD_TSO_F) && !((flags) & NIX_TX_OFFLOAD_L3_L4_CSUM_F))  \
			return 0;                                                                  \
		return cn20k_nix_xmit_pkts(tx_queue, NULL, tx_pkts, pkts, cmd, flags);             \
	}

#define NIX_TX_XMIT_MSEG(fn, sz, flags)                                                            \
	uint16_t __rte_noinline __rte_hot fn(void *tx_queue, struct rte_mbuf **tx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		uint64_t cmd[(sz) + CNXK_NIX_TX_MSEG_SG_DWORDS - 2];                               \
		/* For TSO inner checksum is a must */                                             \
		if (((flags) & NIX_TX_OFFLOAD_TSO_F) && !((flags) & NIX_TX_OFFLOAD_L3_L4_CSUM_F))  \
			return 0;                                                                  \
		return cn20k_nix_xmit_pkts_mseg(tx_queue, NULL, tx_pkts, pkts, cmd,                \
						flags | NIX_TX_MULTI_SEG_F);                       \
	}

#define NIX_TX_XMIT_VEC(fn, sz, flags)                                                             \
	uint16_t __rte_noinline __rte_hot fn(void *tx_queue, struct rte_mbuf **tx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		RTE_SET_USED(tx_queue);                                                            \
		RTE_SET_USED(tx_pkts);                                                             \
		RTE_SET_USED(pkts);                                                                \
		return 0;                                                                          \
	}

#define NIX_TX_XMIT_VEC_MSEG(fn, sz, flags)                                                        \
	uint16_t __rte_noinline __rte_hot fn(void *tx_queue, struct rte_mbuf **tx_pkts,            \
					     uint16_t pkts)                                        \
	{                                                                                          \
		RTE_SET_USED(tx_queue);                                                            \
		RTE_SET_USED(tx_pkts);                                                             \
		RTE_SET_USED(pkts);                                                                \
		return 0;                                                                          \
	}

uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_all_offload(void *tx_queue,
								  struct rte_mbuf **tx_pkts,
								  uint16_t pkts);

uint16_t __rte_noinline __rte_hot cn20k_nix_xmit_pkts_vec_all_offload(void *tx_queue,
								      struct rte_mbuf **tx_pkts,
								      uint16_t pkts);
#endif /* __CN20K_TX_H__ */
