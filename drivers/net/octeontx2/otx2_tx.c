/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_vect.h>

#include "otx2_ethdev.h"

#define NIX_XMIT_FC_OR_RETURN(txq, pkts) do {				\
	/* Cached value is low, Update the fc_cache_pkts */		\
	if (unlikely((txq)->fc_cache_pkts < (pkts))) {			\
		/* Multiply with sqe_per_sqb to express in pkts */	\
		(txq)->fc_cache_pkts =					\
			((txq)->nb_sqb_bufs_adj - *(txq)->fc_mem) <<    \
				(txq)->sqes_per_sqb_log2;		\
		/* Check it again for the room */			\
		if (unlikely((txq)->fc_cache_pkts < (pkts)))		\
			return 0;					\
	}								\
} while (0)


static __rte_always_inline uint16_t
nix_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	      uint16_t pkts, uint64_t *cmd, const uint16_t flags)
{
	struct otx2_eth_txq *txq = tx_queue; uint16_t i;
	const rte_iova_t io_addr = txq->io_addr;
	void *lmt_addr = txq->lmt_addr;
	uint64_t lso_tun_fmt;

	NIX_XMIT_FC_OR_RETURN(txq, pkts);

	otx2_lmt_mov(cmd, &txq->cmd[0], otx2_nix_tx_ext_subs(flags));

	/* Perform header writes before barrier for TSO */
	if (flags & NIX_TX_OFFLOAD_TSO_F) {
		lso_tun_fmt = txq->lso_tun_fmt;
		for (i = 0; i < pkts; i++)
			otx2_nix_xmit_prepare_tso(tx_pkts[i], flags);
	}

	/* Lets commit any changes in the packet here as no further changes
	 * to the packet will be done unless no fast free is enabled.
	 */
	if (!(flags & NIX_TX_OFFLOAD_MBUF_NOFF_F))
		rte_io_wmb();

	for (i = 0; i < pkts; i++) {
		otx2_nix_xmit_prepare(tx_pkts[i], cmd, flags, lso_tun_fmt);
		/* Passing no of segdw as 4: HDR + EXT + SG + SMEM */
		otx2_nix_xmit_prepare_tstamp(cmd, &txq->cmd[0],
					     tx_pkts[i]->ol_flags, 4, flags);
		otx2_nix_xmit_one(cmd, lmt_addr, io_addr, flags);
	}

	/* Reduce the cached count */
	txq->fc_cache_pkts -= pkts;

	return pkts;
}

static __rte_always_inline uint16_t
nix_xmit_pkts_mseg(void *tx_queue, struct rte_mbuf **tx_pkts,
		   uint16_t pkts, uint64_t *cmd, const uint16_t flags)
{
	struct otx2_eth_txq *txq = tx_queue; uint64_t i;
	const rte_iova_t io_addr = txq->io_addr;
	void *lmt_addr = txq->lmt_addr;
	uint64_t lso_tun_fmt;
	uint16_t segdw;

	NIX_XMIT_FC_OR_RETURN(txq, pkts);

	otx2_lmt_mov(cmd, &txq->cmd[0], otx2_nix_tx_ext_subs(flags));

	/* Perform header writes before barrier for TSO */
	if (flags & NIX_TX_OFFLOAD_TSO_F) {
		lso_tun_fmt = txq->lso_tun_fmt;
		for (i = 0; i < pkts; i++)
			otx2_nix_xmit_prepare_tso(tx_pkts[i], flags);
	}

	/* Lets commit any changes in the packet here as no further changes
	 * to the packet will be done unless no fast free is enabled.
	 */
	if (!(flags & NIX_TX_OFFLOAD_MBUF_NOFF_F))
		rte_io_wmb();

	for (i = 0; i < pkts; i++) {
		otx2_nix_xmit_prepare(tx_pkts[i], cmd, flags, lso_tun_fmt);
		segdw = otx2_nix_prepare_mseg(tx_pkts[i], cmd, flags);
		otx2_nix_xmit_prepare_tstamp(cmd, &txq->cmd[0],
					     tx_pkts[i]->ol_flags, segdw,
					     flags);
		otx2_nix_xmit_mseg_one(cmd, lmt_addr, io_addr, segdw);
	}

	/* Reduce the cached count */
	txq->fc_cache_pkts -= pkts;

	return pkts;
}

#if defined(RTE_ARCH_ARM64)

#define NIX_DESCS_PER_LOOP	4
static __rte_always_inline uint16_t
nix_xmit_pkts_vector(void *tx_queue, struct rte_mbuf **tx_pkts,
		     uint16_t pkts, uint64_t *cmd, const uint16_t flags)
{
	uint64x2_t dataoff_iova0, dataoff_iova1, dataoff_iova2, dataoff_iova3;
	uint64x2_t len_olflags0, len_olflags1, len_olflags2, len_olflags3;
	uint64_t *mbuf0, *mbuf1, *mbuf2, *mbuf3;
	uint64x2_t senddesc01_w0, senddesc23_w0;
	uint64x2_t senddesc01_w1, senddesc23_w1;
	uint64x2_t sgdesc01_w0, sgdesc23_w0;
	uint64x2_t sgdesc01_w1, sgdesc23_w1;
	struct otx2_eth_txq *txq = tx_queue;
	uint64_t *lmt_addr = txq->lmt_addr;
	rte_iova_t io_addr = txq->io_addr;
	uint64x2_t ltypes01, ltypes23;
	uint64x2_t xtmp128, ytmp128;
	uint64x2_t xmask01, xmask23;
	uint64x2_t cmd00, cmd01;
	uint64x2_t cmd10, cmd11;
	uint64x2_t cmd20, cmd21;
	uint64x2_t cmd30, cmd31;
	uint64_t lmt_status, i;
	uint16_t pkts_left;

	NIX_XMIT_FC_OR_RETURN(txq, pkts);

	pkts_left = pkts & (NIX_DESCS_PER_LOOP - 1);
	pkts = RTE_ALIGN_FLOOR(pkts, NIX_DESCS_PER_LOOP);

	/* Reduce the cached count */
	txq->fc_cache_pkts -= pkts;

	/* Lets commit any changes in the packet here as no further changes
	 * to the packet will be done unless no fast free is enabled.
	 */
	if (!(flags & NIX_TX_OFFLOAD_MBUF_NOFF_F))
		rte_io_wmb();

	senddesc01_w0 = vld1q_dup_u64(&txq->cmd[0]);
	senddesc23_w0 = senddesc01_w0;
	senddesc01_w1 = vdupq_n_u64(0);
	senddesc23_w1 = senddesc01_w1;
	sgdesc01_w0 = vld1q_dup_u64(&txq->cmd[2]);
	sgdesc23_w0 = sgdesc01_w0;

	for (i = 0; i < pkts; i += NIX_DESCS_PER_LOOP) {
		/* Clear lower 32bit of SEND_HDR_W0 and SEND_SG_W0 */
		senddesc01_w0 = vbicq_u64(senddesc01_w0,
					  vdupq_n_u64(0xFFFFFFFF));
		sgdesc01_w0 = vbicq_u64(sgdesc01_w0,
					vdupq_n_u64(0xFFFFFFFF));

		senddesc23_w0 = senddesc01_w0;
		sgdesc23_w0 = sgdesc01_w0;

		/* Move mbufs to iova */
		mbuf0 = (uint64_t *)tx_pkts[0];
		mbuf1 = (uint64_t *)tx_pkts[1];
		mbuf2 = (uint64_t *)tx_pkts[2];
		mbuf3 = (uint64_t *)tx_pkts[3];

		mbuf0 = (uint64_t *)((uintptr_t)mbuf0 +
				     offsetof(struct rte_mbuf, buf_iova));
		mbuf1 = (uint64_t *)((uintptr_t)mbuf1 +
				     offsetof(struct rte_mbuf, buf_iova));
		mbuf2 = (uint64_t *)((uintptr_t)mbuf2 +
				     offsetof(struct rte_mbuf, buf_iova));
		mbuf3 = (uint64_t *)((uintptr_t)mbuf3 +
				     offsetof(struct rte_mbuf, buf_iova));
		/*
		 * Get mbuf's, olflags, iova, pktlen, dataoff
		 * dataoff_iovaX.D[0] = iova,
		 * dataoff_iovaX.D[1](15:0) = mbuf->dataoff
		 * len_olflagsX.D[0] = ol_flags,
		 * len_olflagsX.D[1](63:32) = mbuf->pkt_len
		 */
		dataoff_iova0  = vld1q_u64(mbuf0);
		len_olflags0 = vld1q_u64(mbuf0 + 2);
		dataoff_iova1  = vld1q_u64(mbuf1);
		len_olflags1 = vld1q_u64(mbuf1 + 2);
		dataoff_iova2  = vld1q_u64(mbuf2);
		len_olflags2 = vld1q_u64(mbuf2 + 2);
		dataoff_iova3  = vld1q_u64(mbuf3);
		len_olflags3 = vld1q_u64(mbuf3 + 2);

		if (flags & NIX_TX_OFFLOAD_MBUF_NOFF_F) {
			struct rte_mbuf *mbuf;
			/* Set don't free bit if reference count > 1 */
			xmask01 = vdupq_n_u64(0);
			xmask23 = xmask01;

			mbuf = (struct rte_mbuf *)((uintptr_t)mbuf0 -
				offsetof(struct rte_mbuf, buf_iova));

			if (otx2_nix_prefree_seg(mbuf))
				vsetq_lane_u64(0x80000, xmask01, 0);
			else
				__mempool_check_cookies(mbuf->pool,
							(void **)&mbuf,
							1, 0);

			mbuf = (struct rte_mbuf *)((uintptr_t)mbuf1 -
				offsetof(struct rte_mbuf, buf_iova));
			if (otx2_nix_prefree_seg(mbuf))
				vsetq_lane_u64(0x80000, xmask01, 1);
			else
				__mempool_check_cookies(mbuf->pool,
							(void **)&mbuf,
							1, 0);

			mbuf = (struct rte_mbuf *)((uintptr_t)mbuf2 -
				offsetof(struct rte_mbuf, buf_iova));
			if (otx2_nix_prefree_seg(mbuf))
				vsetq_lane_u64(0x80000, xmask23, 0);
			else
				__mempool_check_cookies(mbuf->pool,
							(void **)&mbuf,
							1, 0);

			mbuf = (struct rte_mbuf *)((uintptr_t)mbuf3 -
				offsetof(struct rte_mbuf, buf_iova));
			if (otx2_nix_prefree_seg(mbuf))
				vsetq_lane_u64(0x80000, xmask23, 1);
			else
				__mempool_check_cookies(mbuf->pool,
							(void **)&mbuf,
							1, 0);
			senddesc01_w0 = vorrq_u64(senddesc01_w0, xmask01);
			senddesc23_w0 = vorrq_u64(senddesc23_w0, xmask23);
			/* Ensuring mbuf fields which got updated in
			 * otx2_nix_prefree_seg are written before LMTST.
			 */
			rte_io_wmb();
		} else {
			struct rte_mbuf *mbuf;
			/* Mark mempool object as "put" since
			 * it is freed by NIX
			 */
			mbuf = (struct rte_mbuf *)((uintptr_t)mbuf0 -
				offsetof(struct rte_mbuf, buf_iova));
			__mempool_check_cookies(mbuf->pool, (void **)&mbuf,
						1, 0);

			mbuf = (struct rte_mbuf *)((uintptr_t)mbuf1 -
				offsetof(struct rte_mbuf, buf_iova));
			__mempool_check_cookies(mbuf->pool, (void **)&mbuf,
						1, 0);

			mbuf = (struct rte_mbuf *)((uintptr_t)mbuf2 -
				offsetof(struct rte_mbuf, buf_iova));
			__mempool_check_cookies(mbuf->pool, (void **)&mbuf,
						1, 0);

			mbuf = (struct rte_mbuf *)((uintptr_t)mbuf3 -
				offsetof(struct rte_mbuf, buf_iova));
			__mempool_check_cookies(mbuf->pool, (void **)&mbuf,
						1, 0);
			RTE_SET_USED(mbuf);
		}

		/* Move mbufs to point pool */
		mbuf0 = (uint64_t *)((uintptr_t)mbuf0 +
			 offsetof(struct rte_mbuf, pool) -
			 offsetof(struct rte_mbuf, buf_iova));
		mbuf1 = (uint64_t *)((uintptr_t)mbuf1 +
			 offsetof(struct rte_mbuf, pool) -
			 offsetof(struct rte_mbuf, buf_iova));
		mbuf2 = (uint64_t *)((uintptr_t)mbuf2 +
			 offsetof(struct rte_mbuf, pool) -
			 offsetof(struct rte_mbuf, buf_iova));
		mbuf3 = (uint64_t *)((uintptr_t)mbuf3 +
			 offsetof(struct rte_mbuf, pool) -
			 offsetof(struct rte_mbuf, buf_iova));

		if (flags &
		    (NIX_TX_OFFLOAD_OL3_OL4_CSUM_F |
		     NIX_TX_OFFLOAD_L3_L4_CSUM_F)) {
			/* Get tx_offload for ol2, ol3, l2, l3 lengths */
			/*
			 * E(8):OL2_LEN(7):OL3_LEN(9):E(24):L3_LEN(9):L2_LEN(7)
			 * E(8):OL2_LEN(7):OL3_LEN(9):E(24):L3_LEN(9):L2_LEN(7)
			 */

			asm volatile ("LD1 {%[a].D}[0],[%[in]]\n\t" :
				      [a]"+w"(senddesc01_w1) :
				      [in]"r"(mbuf0 + 2) : "memory");

			asm volatile ("LD1 {%[a].D}[1],[%[in]]\n\t" :
				      [a]"+w"(senddesc01_w1) :
				      [in]"r"(mbuf1 + 2) : "memory");

			asm volatile ("LD1 {%[b].D}[0],[%[in]]\n\t" :
				      [b]"+w"(senddesc23_w1) :
				      [in]"r"(mbuf2 + 2) : "memory");

			asm volatile ("LD1 {%[b].D}[1],[%[in]]\n\t" :
				      [b]"+w"(senddesc23_w1) :
				      [in]"r"(mbuf3 + 2) : "memory");

			/* Get pool pointer alone */
			mbuf0 = (uint64_t *)*mbuf0;
			mbuf1 = (uint64_t *)*mbuf1;
			mbuf2 = (uint64_t *)*mbuf2;
			mbuf3 = (uint64_t *)*mbuf3;
		} else {
			/* Get pool pointer alone */
			mbuf0 = (uint64_t *)*mbuf0;
			mbuf1 = (uint64_t *)*mbuf1;
			mbuf2 = (uint64_t *)*mbuf2;
			mbuf3 = (uint64_t *)*mbuf3;
		}

		const uint8x16_t shuf_mask2 = {
			0x4, 0x5, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xc, 0xd, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		};
		xtmp128 = vzip2q_u64(len_olflags0, len_olflags1);
		ytmp128 = vzip2q_u64(len_olflags2, len_olflags3);

		/* Clear dataoff_iovaX.D[1] bits other than dataoff(15:0) */
		const uint64x2_t and_mask0 = {
			0xFFFFFFFFFFFFFFFF,
			0x000000000000FFFF,
		};

		dataoff_iova0 = vandq_u64(dataoff_iova0, and_mask0);
		dataoff_iova1 = vandq_u64(dataoff_iova1, and_mask0);
		dataoff_iova2 = vandq_u64(dataoff_iova2, and_mask0);
		dataoff_iova3 = vandq_u64(dataoff_iova3, and_mask0);

		/*
		 * Pick only 16 bits of pktlen preset at bits 63:32
		 * and place them at bits 15:0.
		 */
		xtmp128 = vqtbl1q_u8(xtmp128, shuf_mask2);
		ytmp128 = vqtbl1q_u8(ytmp128, shuf_mask2);

		/* Add pairwise to get dataoff + iova in sgdesc_w1 */
		sgdesc01_w1 = vpaddq_u64(dataoff_iova0, dataoff_iova1);
		sgdesc23_w1 = vpaddq_u64(dataoff_iova2, dataoff_iova3);

		/* Orr both sgdesc_w0 and senddesc_w0 with 16 bits of
		 * pktlen at 15:0 position.
		 */
		sgdesc01_w0 = vorrq_u64(sgdesc01_w0, xtmp128);
		sgdesc23_w0 = vorrq_u64(sgdesc23_w0, ytmp128);
		senddesc01_w0 = vorrq_u64(senddesc01_w0, xtmp128);
		senddesc23_w0 = vorrq_u64(senddesc23_w0, ytmp128);

		if ((flags & NIX_TX_OFFLOAD_L3_L4_CSUM_F) &&
		    !(flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F)) {
			/*
			 * Lookup table to translate ol_flags to
			 * il3/il4 types. But we still use ol3/ol4 types in
			 * senddesc_w1 as only one header processing is enabled.
			 */
			const uint8x16_t tbl = {
				/* [0-15] = il4type:il3type */
				0x04, /* none (IPv6 assumed) */
				0x14, /* PKT_TX_TCP_CKSUM (IPv6 assumed) */
				0x24, /* PKT_TX_SCTP_CKSUM (IPv6 assumed) */
				0x34, /* PKT_TX_UDP_CKSUM (IPv6 assumed) */
				0x03, /* PKT_TX_IP_CKSUM */
				0x13, /* PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM */
				0x23, /* PKT_TX_IP_CKSUM | PKT_TX_SCTP_CKSUM */
				0x33, /* PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM */
				0x02, /* PKT_TX_IPV4  */
				0x12, /* PKT_TX_IPV4 | PKT_TX_TCP_CKSUM */
				0x22, /* PKT_TX_IPV4 | PKT_TX_SCTP_CKSUM */
				0x32, /* PKT_TX_IPV4 | PKT_TX_UDP_CKSUM */
				0x03, /* PKT_TX_IPV4 | PKT_TX_IP_CKSUM */
				0x13, /* PKT_TX_IPV4 | PKT_TX_IP_CKSUM |
				       * PKT_TX_TCP_CKSUM
				       */
				0x23, /* PKT_TX_IPV4 | PKT_TX_IP_CKSUM |
				       * PKT_TX_SCTP_CKSUM
				       */
				0x33, /* PKT_TX_IPV4 | PKT_TX_IP_CKSUM |
				       * PKT_TX_UDP_CKSUM
				       */
			};

			/* Extract olflags to translate to iltypes */
			xtmp128 = vzip1q_u64(len_olflags0, len_olflags1);
			ytmp128 = vzip1q_u64(len_olflags2, len_olflags3);

			/*
			 * E(47):L3_LEN(9):L2_LEN(7+z)
			 * E(47):L3_LEN(9):L2_LEN(7+z)
			 */
			senddesc01_w1 = vshlq_n_u64(senddesc01_w1, 1);
			senddesc23_w1 = vshlq_n_u64(senddesc23_w1, 1);

			/* Move OLFLAGS bits 55:52 to 51:48
			 * with zeros preprended on the byte and rest
			 * don't care
			 */
			xtmp128 = vshrq_n_u8(xtmp128, 4);
			ytmp128 = vshrq_n_u8(ytmp128, 4);
			/*
			 * E(48):L3_LEN(8):L2_LEN(z+7)
			 * E(48):L3_LEN(8):L2_LEN(z+7)
			 */
			const int8x16_t tshft3 = {
				-1, 0, 8, 8, 8,	8, 8, 8,
				-1, 0, 8, 8, 8,	8, 8, 8,
			};

			senddesc01_w1 = vshlq_u8(senddesc01_w1, tshft3);
			senddesc23_w1 = vshlq_u8(senddesc23_w1, tshft3);

			/* Do the lookup */
			ltypes01 = vqtbl1q_u8(tbl, xtmp128);
			ltypes23 = vqtbl1q_u8(tbl, ytmp128);

			/* Just use ld1q to retrieve aura
			 * when we don't need tx_offload
			 */
			mbuf0 = (uint64_t *)((uintptr_t)mbuf0 +
					offsetof(struct rte_mempool, pool_id));
			mbuf1 = (uint64_t *)((uintptr_t)mbuf1 +
					offsetof(struct rte_mempool, pool_id));
			mbuf2 = (uint64_t *)((uintptr_t)mbuf2 +
					offsetof(struct rte_mempool, pool_id));
			mbuf3 = (uint64_t *)((uintptr_t)mbuf3 +
					offsetof(struct rte_mempool, pool_id));

			/* Pick only relevant fields i.e Bit 48:55 of iltype
			 * and place it in ol3/ol4type of senddesc_w1
			 */
			const uint8x16_t shuf_mask0 = {
				0xFF, 0xFF, 0xFF, 0xFF,	0x6, 0xFF, 0xFF, 0xFF,
				0xFF, 0xFF, 0xFF, 0xFF, 0xE, 0xFF, 0xFF, 0xFF,
			};

			ltypes01 = vqtbl1q_u8(ltypes01, shuf_mask0);
			ltypes23 = vqtbl1q_u8(ltypes23, shuf_mask0);

			/* Prepare ol4ptr, ol3ptr from ol3len, ol2len.
			 * a [E(32):E(16):OL3(8):OL2(8)]
			 * a = a + (a << 8)
			 * a [E(32):E(16):(OL3+OL2):OL2]
			 * => E(32):E(16)::OL4PTR(8):OL3PTR(8)
			 */
			senddesc01_w1 = vaddq_u8(senddesc01_w1,
						 vshlq_n_u16(senddesc01_w1, 8));
			senddesc23_w1 = vaddq_u8(senddesc23_w1,
						 vshlq_n_u16(senddesc23_w1, 8));

			/* Create first half of 4W cmd for 4 mbufs (sgdesc) */
			cmd01 = vzip1q_u64(sgdesc01_w0, sgdesc01_w1);
			cmd11 = vzip2q_u64(sgdesc01_w0, sgdesc01_w1);
			cmd21 = vzip1q_u64(sgdesc23_w0, sgdesc23_w1);
			cmd31 = vzip2q_u64(sgdesc23_w0, sgdesc23_w1);

			xmask01 = vdupq_n_u64(0);
			xmask23 = xmask01;
			asm volatile ("LD1 {%[a].H}[0],[%[in]]\n\t" :
				[a]"+w"(xmask01) : [in]"r"(mbuf0) : "memory");

			asm volatile ("LD1 {%[a].H}[4],[%[in]]\n\t" :
				 [a]"+w"(xmask01) : [in]"r"(mbuf1) : "memory");

			asm volatile ("LD1 {%[b].H}[0],[%[in]]\n\t" :
				 [b]"+w"(xmask23) : [in]"r"(mbuf2) : "memory");

			asm volatile ("LD1 {%[b].H}[4],[%[in]]\n\t" :
				 [b]"+w"(xmask23) : [in]"r"(mbuf3) : "memory");
			xmask01 = vshlq_n_u64(xmask01, 20);
			xmask23 = vshlq_n_u64(xmask23, 20);

			senddesc01_w0 = vorrq_u64(senddesc01_w0, xmask01);
			senddesc23_w0 = vorrq_u64(senddesc23_w0, xmask23);
			/* Move ltypes to senddesc*_w1 */
			senddesc01_w1 = vorrq_u64(senddesc01_w1, ltypes01);
			senddesc23_w1 = vorrq_u64(senddesc23_w1, ltypes23);

			/* Create first half of 4W cmd for 4 mbufs (sendhdr) */
			cmd00 = vzip1q_u64(senddesc01_w0, senddesc01_w1);
			cmd10 = vzip2q_u64(senddesc01_w0, senddesc01_w1);
			cmd20 = vzip1q_u64(senddesc23_w0, senddesc23_w1);
			cmd30 = vzip2q_u64(senddesc23_w0, senddesc23_w1);

		} else if (!(flags & NIX_TX_OFFLOAD_L3_L4_CSUM_F) &&
			   (flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F)) {
			/*
			 * Lookup table to translate ol_flags to
			 * ol3/ol4 types.
			 */

			const uint8x16_t tbl = {
				/* [0-15] = ol4type:ol3type */
				0x00, /* none */
				0x03, /* OUTER_IP_CKSUM */
				0x02, /* OUTER_IPV4 */
				0x03, /* OUTER_IPV4 | OUTER_IP_CKSUM */
				0x04, /* OUTER_IPV6 */
				0x00, /* OUTER_IPV6 | OUTER_IP_CKSUM */
				0x00, /* OUTER_IPV6 | OUTER_IPV4 */
				0x00, /* OUTER_IPV6 | OUTER_IPV4 |
				       * OUTER_IP_CKSUM
				       */
				0x00, /* OUTER_UDP_CKSUM */
				0x33, /* OUTER_UDP_CKSUM | OUTER_IP_CKSUM */
				0x32, /* OUTER_UDP_CKSUM | OUTER_IPV4 */
				0x33, /* OUTER_UDP_CKSUM | OUTER_IPV4 |
				       * OUTER_IP_CKSUM
				       */
				0x34, /* OUTER_UDP_CKSUM | OUTER_IPV6 */
				0x00, /* OUTER_UDP_CKSUM | OUTER_IPV6 |
				       * OUTER_IP_CKSUM
				       */
				0x00, /* OUTER_UDP_CKSUM | OUTER_IPV6 |
				       * OUTER_IPV4
				       */
				0x00, /* OUTER_UDP_CKSUM | OUTER_IPV6 |
				       * OUTER_IPV4 | OUTER_IP_CKSUM
				       */
			};

			/* Extract olflags to translate to iltypes */
			xtmp128 = vzip1q_u64(len_olflags0, len_olflags1);
			ytmp128 = vzip1q_u64(len_olflags2, len_olflags3);

			/*
			 * E(47):OL3_LEN(9):OL2_LEN(7+z)
			 * E(47):OL3_LEN(9):OL2_LEN(7+z)
			 */
			const uint8x16_t shuf_mask5 = {
				0x6, 0x5, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				0xE, 0xD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			};
			senddesc01_w1 = vqtbl1q_u8(senddesc01_w1, shuf_mask5);
			senddesc23_w1 = vqtbl1q_u8(senddesc23_w1, shuf_mask5);

			/* Extract outer ol flags only */
			const uint64x2_t o_cksum_mask = {
				0x1C00020000000000,
				0x1C00020000000000,
			};

			xtmp128 = vandq_u64(xtmp128, o_cksum_mask);
			ytmp128 = vandq_u64(ytmp128, o_cksum_mask);

			/* Extract OUTER_UDP_CKSUM bit 41 and
			 * move it to bit 61
			 */

			xtmp128 = xtmp128 | vshlq_n_u64(xtmp128, 20);
			ytmp128 = ytmp128 | vshlq_n_u64(ytmp128, 20);

			/* Shift oltype by 2 to start nibble from BIT(56)
			 * instead of BIT(58)
			 */
			xtmp128 = vshrq_n_u8(xtmp128, 2);
			ytmp128 = vshrq_n_u8(ytmp128, 2);
			/*
			 * E(48):L3_LEN(8):L2_LEN(z+7)
			 * E(48):L3_LEN(8):L2_LEN(z+7)
			 */
			const int8x16_t tshft3 = {
				-1, 0, 8, 8, 8, 8, 8, 8,
				-1, 0, 8, 8, 8, 8, 8, 8,
			};

			senddesc01_w1 = vshlq_u8(senddesc01_w1, tshft3);
			senddesc23_w1 = vshlq_u8(senddesc23_w1, tshft3);

			/* Do the lookup */
			ltypes01 = vqtbl1q_u8(tbl, xtmp128);
			ltypes23 = vqtbl1q_u8(tbl, ytmp128);

			/* Just use ld1q to retrieve aura
			 * when we don't need tx_offload
			 */
			mbuf0 = (uint64_t *)((uintptr_t)mbuf0 +
					offsetof(struct rte_mempool, pool_id));
			mbuf1 = (uint64_t *)((uintptr_t)mbuf1 +
					offsetof(struct rte_mempool, pool_id));
			mbuf2 = (uint64_t *)((uintptr_t)mbuf2 +
					offsetof(struct rte_mempool, pool_id));
			mbuf3 = (uint64_t *)((uintptr_t)mbuf3 +
					offsetof(struct rte_mempool, pool_id));

			/* Pick only relevant fields i.e Bit 56:63 of oltype
			 * and place it in ol3/ol4type of senddesc_w1
			 */
			const uint8x16_t shuf_mask0 = {
				0xFF, 0xFF, 0xFF, 0xFF,	0x7, 0xFF, 0xFF, 0xFF,
				0xFF, 0xFF, 0xFF, 0xFF, 0xF, 0xFF, 0xFF, 0xFF,
			};

			ltypes01 = vqtbl1q_u8(ltypes01, shuf_mask0);
			ltypes23 = vqtbl1q_u8(ltypes23, shuf_mask0);

			/* Prepare ol4ptr, ol3ptr from ol3len, ol2len.
			 * a [E(32):E(16):OL3(8):OL2(8)]
			 * a = a + (a << 8)
			 * a [E(32):E(16):(OL3+OL2):OL2]
			 * => E(32):E(16)::OL4PTR(8):OL3PTR(8)
			 */
			senddesc01_w1 = vaddq_u8(senddesc01_w1,
						 vshlq_n_u16(senddesc01_w1, 8));
			senddesc23_w1 = vaddq_u8(senddesc23_w1,
						 vshlq_n_u16(senddesc23_w1, 8));

			/* Create second half of 4W cmd for 4 mbufs (sgdesc) */
			cmd01 = vzip1q_u64(sgdesc01_w0, sgdesc01_w1);
			cmd11 = vzip2q_u64(sgdesc01_w0, sgdesc01_w1);
			cmd21 = vzip1q_u64(sgdesc23_w0, sgdesc23_w1);
			cmd31 = vzip2q_u64(sgdesc23_w0, sgdesc23_w1);

			xmask01 = vdupq_n_u64(0);
			xmask23 = xmask01;
			asm volatile ("LD1 {%[a].H}[0],[%[in]]\n\t" :
				 [a]"+w"(xmask01) : [in]"r"(mbuf0) : "memory");

			asm volatile ("LD1 {%[a].H}[4],[%[in]]\n\t" :
				 [a]"+w"(xmask01) : [in]"r"(mbuf1) : "memory");

			asm volatile ("LD1 {%[b].H}[0],[%[in]]\n\t" :
				 [b]"+w"(xmask23) : [in]"r"(mbuf2) : "memory");

			asm volatile ("LD1 {%[b].H}[4],[%[in]]\n\t" :
				 [b]"+w"(xmask23) : [in]"r"(mbuf3) : "memory");
			xmask01 = vshlq_n_u64(xmask01, 20);
			xmask23 = vshlq_n_u64(xmask23, 20);

			senddesc01_w0 = vorrq_u64(senddesc01_w0, xmask01);
			senddesc23_w0 = vorrq_u64(senddesc23_w0, xmask23);
			/* Move ltypes to senddesc*_w1 */
			senddesc01_w1 = vorrq_u64(senddesc01_w1, ltypes01);
			senddesc23_w1 = vorrq_u64(senddesc23_w1, ltypes23);

			/* Create first half of 4W cmd for 4 mbufs (sendhdr) */
			cmd00 = vzip1q_u64(senddesc01_w0, senddesc01_w1);
			cmd10 = vzip2q_u64(senddesc01_w0, senddesc01_w1);
			cmd20 = vzip1q_u64(senddesc23_w0, senddesc23_w1);
			cmd30 = vzip2q_u64(senddesc23_w0, senddesc23_w1);

		} else if ((flags & NIX_TX_OFFLOAD_L3_L4_CSUM_F) &&
			   (flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F)) {
			/* Lookup table to translate ol_flags to
			 * ol4type, ol3type, il4type, il3type of senddesc_w1
			 */
			const uint8x16x2_t tbl = {
			{
				{
					/* [0-15] = il4type:il3type */
					0x04, /* none (IPv6) */
					0x14, /* PKT_TX_TCP_CKSUM (IPv6) */
					0x24, /* PKT_TX_SCTP_CKSUM (IPv6) */
					0x34, /* PKT_TX_UDP_CKSUM (IPv6) */
					0x03, /* PKT_TX_IP_CKSUM */
					0x13, /* PKT_TX_IP_CKSUM |
					       * PKT_TX_TCP_CKSUM
					       */
					0x23, /* PKT_TX_IP_CKSUM |
					       * PKT_TX_SCTP_CKSUM
					       */
					0x33, /* PKT_TX_IP_CKSUM |
					       * PKT_TX_UDP_CKSUM
					       */
					0x02, /* PKT_TX_IPV4 */
					0x12, /* PKT_TX_IPV4 |
					       * PKT_TX_TCP_CKSUM
					       */
					0x22, /* PKT_TX_IPV4 |
					       * PKT_TX_SCTP_CKSUM
					       */
					0x32, /* PKT_TX_IPV4 |
					       * PKT_TX_UDP_CKSUM
					       */
					0x03, /* PKT_TX_IPV4 |
					       * PKT_TX_IP_CKSUM
					       */
					0x13, /* PKT_TX_IPV4 | PKT_TX_IP_CKSUM |
					       * PKT_TX_TCP_CKSUM
					       */
					0x23, /* PKT_TX_IPV4 | PKT_TX_IP_CKSUM |
					       * PKT_TX_SCTP_CKSUM
					       */
					0x33, /* PKT_TX_IPV4 | PKT_TX_IP_CKSUM |
					       * PKT_TX_UDP_CKSUM
					       */
				},

				{
					/* [16-31] = ol4type:ol3type */
					0x00, /* none */
					0x03, /* OUTER_IP_CKSUM */
					0x02, /* OUTER_IPV4 */
					0x03, /* OUTER_IPV4 | OUTER_IP_CKSUM */
					0x04, /* OUTER_IPV6 */
					0x00, /* OUTER_IPV6 | OUTER_IP_CKSUM */
					0x00, /* OUTER_IPV6 | OUTER_IPV4 */
					0x00, /* OUTER_IPV6 | OUTER_IPV4 |
					       * OUTER_IP_CKSUM
					       */
					0x00, /* OUTER_UDP_CKSUM */
					0x33, /* OUTER_UDP_CKSUM |
					       * OUTER_IP_CKSUM
					       */
					0x32, /* OUTER_UDP_CKSUM |
					       * OUTER_IPV4
					       */
					0x33, /* OUTER_UDP_CKSUM |
					       * OUTER_IPV4 | OUTER_IP_CKSUM
					       */
					0x34, /* OUTER_UDP_CKSUM |
					       * OUTER_IPV6
					       */
					0x00, /* OUTER_UDP_CKSUM | OUTER_IPV6 |
					       * OUTER_IP_CKSUM
					       */
					0x00, /* OUTER_UDP_CKSUM | OUTER_IPV6 |
					       * OUTER_IPV4
					       */
					0x00, /* OUTER_UDP_CKSUM | OUTER_IPV6 |
					       * OUTER_IPV4 | OUTER_IP_CKSUM
					       */
				},
			}
			};

			/* Extract olflags to translate to oltype & iltype */
			xtmp128 = vzip1q_u64(len_olflags0, len_olflags1);
			ytmp128 = vzip1q_u64(len_olflags2, len_olflags3);

			/*
			 * E(8):OL2_LN(7):OL3_LN(9):E(23):L3_LN(9):L2_LN(7+z)
			 * E(8):OL2_LN(7):OL3_LN(9):E(23):L3_LN(9):L2_LN(7+z)
			 */
			const uint32x4_t tshft_4 = {
				1, 0,
				1, 0,
			};
			senddesc01_w1 = vshlq_u32(senddesc01_w1, tshft_4);
			senddesc23_w1 = vshlq_u32(senddesc23_w1, tshft_4);

			/*
			 * E(32):L3_LEN(8):L2_LEN(7+Z):OL3_LEN(8):OL2_LEN(7+Z)
			 * E(32):L3_LEN(8):L2_LEN(7+Z):OL3_LEN(8):OL2_LEN(7+Z)
			 */
			const uint8x16_t shuf_mask5 = {
				0x6, 0x5, 0x0, 0x1, 0xFF, 0xFF, 0xFF, 0xFF,
				0xE, 0xD, 0x8, 0x9, 0xFF, 0xFF,	0xFF, 0xFF,
			};
			senddesc01_w1 = vqtbl1q_u8(senddesc01_w1, shuf_mask5);
			senddesc23_w1 = vqtbl1q_u8(senddesc23_w1, shuf_mask5);

			/* Extract outer and inner header ol_flags */
			const uint64x2_t oi_cksum_mask = {
				0x1CF0020000000000,
				0x1CF0020000000000,
			};

			xtmp128 = vandq_u64(xtmp128, oi_cksum_mask);
			ytmp128 = vandq_u64(ytmp128, oi_cksum_mask);

			/* Extract OUTER_UDP_CKSUM bit 41 and
			 * move it to bit 61
			 */

			xtmp128 = xtmp128 | vshlq_n_u64(xtmp128, 20);
			ytmp128 = ytmp128 | vshlq_n_u64(ytmp128, 20);

			/* Shift right oltype by 2 and iltype by 4
			 * to start oltype nibble from BIT(58)
			 * instead of BIT(56) and iltype nibble from BIT(48)
			 * instead of BIT(52).
			 */
			const int8x16_t tshft5 = {
				8, 8, 8, 8, 8, 8, -4, -2,
				8, 8, 8, 8, 8, 8, -4, -2,
			};

			xtmp128 = vshlq_u8(xtmp128, tshft5);
			ytmp128 = vshlq_u8(ytmp128, tshft5);
			/*
			 * E(32):L3_LEN(8):L2_LEN(8):OL3_LEN(8):OL2_LEN(8)
			 * E(32):L3_LEN(8):L2_LEN(8):OL3_LEN(8):OL2_LEN(8)
			 */
			const int8x16_t tshft3 = {
				-1, 0, -1, 0, 0, 0, 0, 0,
				-1, 0, -1, 0, 0, 0, 0, 0,
			};

			senddesc01_w1 = vshlq_u8(senddesc01_w1, tshft3);
			senddesc23_w1 = vshlq_u8(senddesc23_w1, tshft3);

			/* Mark Bit(4) of oltype */
			const uint64x2_t oi_cksum_mask2 = {
				0x1000000000000000,
				0x1000000000000000,
			};

			xtmp128 = vorrq_u64(xtmp128, oi_cksum_mask2);
			ytmp128 = vorrq_u64(ytmp128, oi_cksum_mask2);

			/* Do the lookup */
			ltypes01 = vqtbl2q_u8(tbl, xtmp128);
			ltypes23 = vqtbl2q_u8(tbl, ytmp128);

			/* Just use ld1q to retrieve aura
			 * when we don't need tx_offload
			 */
			mbuf0 = (uint64_t *)((uintptr_t)mbuf0 +
					offsetof(struct rte_mempool, pool_id));
			mbuf1 = (uint64_t *)((uintptr_t)mbuf1 +
					offsetof(struct rte_mempool, pool_id));
			mbuf2 = (uint64_t *)((uintptr_t)mbuf2 +
					offsetof(struct rte_mempool, pool_id));
			mbuf3 = (uint64_t *)((uintptr_t)mbuf3 +
					offsetof(struct rte_mempool, pool_id));

			/* Pick only relevant fields i.e Bit 48:55 of iltype and
			 * Bit 56:63 of oltype and place it in corresponding
			 * place in senddesc_w1.
			 */
			const uint8x16_t shuf_mask0 = {
				0xFF, 0xFF, 0xFF, 0xFF, 0x7, 0x6, 0xFF, 0xFF,
				0xFF, 0xFF, 0xFF, 0xFF,	0xF, 0xE, 0xFF, 0xFF,
			};

			ltypes01 = vqtbl1q_u8(ltypes01, shuf_mask0);
			ltypes23 = vqtbl1q_u8(ltypes23, shuf_mask0);

			/* Prepare l4ptr, l3ptr, ol4ptr, ol3ptr from
			 * l3len, l2len, ol3len, ol2len.
			 * a [E(32):L3(8):L2(8):OL3(8):OL2(8)]
			 * a = a + (a << 8)
			 * a [E:(L3+L2):(L2+OL3):(OL3+OL2):OL2]
			 * a = a + (a << 16)
			 * a [E:(L3+L2+OL3+OL2):(L2+OL3+OL2):(OL3+OL2):OL2]
			 * => E(32):IL4PTR(8):IL3PTR(8):OL4PTR(8):OL3PTR(8)
			 */
			senddesc01_w1 = vaddq_u8(senddesc01_w1,
						 vshlq_n_u32(senddesc01_w1, 8));
			senddesc23_w1 = vaddq_u8(senddesc23_w1,
						 vshlq_n_u32(senddesc23_w1, 8));

			/* Create second half of 4W cmd for 4 mbufs (sgdesc) */
			cmd01 = vzip1q_u64(sgdesc01_w0, sgdesc01_w1);
			cmd11 = vzip2q_u64(sgdesc01_w0, sgdesc01_w1);
			cmd21 = vzip1q_u64(sgdesc23_w0, sgdesc23_w1);
			cmd31 = vzip2q_u64(sgdesc23_w0, sgdesc23_w1);

			/* Continue preparing l4ptr, l3ptr, ol4ptr, ol3ptr */
			senddesc01_w1 = vaddq_u8(senddesc01_w1,
						vshlq_n_u32(senddesc01_w1, 16));
			senddesc23_w1 = vaddq_u8(senddesc23_w1,
						vshlq_n_u32(senddesc23_w1, 16));

			xmask01 = vdupq_n_u64(0);
			xmask23 = xmask01;
			asm volatile ("LD1 {%[a].H}[0],[%[in]]\n\t" :
				 [a]"+w"(xmask01) : [in]"r"(mbuf0) : "memory");

			asm volatile ("LD1 {%[a].H}[4],[%[in]]\n\t" :
				 [a]"+w"(xmask01) : [in]"r"(mbuf1) : "memory");

			asm volatile ("LD1 {%[b].H}[0],[%[in]]\n\t" :
				 [b]"+w"(xmask23) : [in]"r"(mbuf2) : "memory");

			asm volatile ("LD1 {%[b].H}[4],[%[in]]\n\t" :
				 [b]"+w"(xmask23) : [in]"r"(mbuf3) : "memory");
			xmask01 = vshlq_n_u64(xmask01, 20);
			xmask23 = vshlq_n_u64(xmask23, 20);

			senddesc01_w0 = vorrq_u64(senddesc01_w0, xmask01);
			senddesc23_w0 = vorrq_u64(senddesc23_w0, xmask23);
			/* Move ltypes to senddesc*_w1 */
			senddesc01_w1 = vorrq_u64(senddesc01_w1, ltypes01);
			senddesc23_w1 = vorrq_u64(senddesc23_w1, ltypes23);

			/* Create first half of 4W cmd for 4 mbufs (sendhdr) */
			cmd00 = vzip1q_u64(senddesc01_w0, senddesc01_w1);
			cmd10 = vzip2q_u64(senddesc01_w0, senddesc01_w1);
			cmd20 = vzip1q_u64(senddesc23_w0, senddesc23_w1);
			cmd30 = vzip2q_u64(senddesc23_w0, senddesc23_w1);
		} else {
			/* Just use ld1q to retrieve aura
			 * when we don't need tx_offload
			 */
			mbuf0 = (uint64_t *)((uintptr_t)mbuf0 +
					offsetof(struct rte_mempool, pool_id));
			mbuf1 = (uint64_t *)((uintptr_t)mbuf1 +
					offsetof(struct rte_mempool, pool_id));
			mbuf2 = (uint64_t *)((uintptr_t)mbuf2 +
					offsetof(struct rte_mempool, pool_id));
			mbuf3 = (uint64_t *)((uintptr_t)mbuf3 +
					offsetof(struct rte_mempool, pool_id));
			xmask01 = vdupq_n_u64(0);
			xmask23 = xmask01;
			asm volatile ("LD1 {%[a].H}[0],[%[in]]\n\t" :
				 [a]"+w"(xmask01) : [in]"r"(mbuf0) : "memory");

			asm volatile ("LD1 {%[a].H}[4],[%[in]]\n\t" :
				 [a]"+w"(xmask01) : [in]"r"(mbuf1) : "memory");

			asm volatile ("LD1 {%[b].H}[0],[%[in]]\n\t" :
				 [b]"+w"(xmask23) : [in]"r"(mbuf2) : "memory");

			asm volatile ("LD1 {%[b].H}[4],[%[in]]\n\t" :
				 [b]"+w"(xmask23) : [in]"r"(mbuf3) : "memory");
			xmask01 = vshlq_n_u64(xmask01, 20);
			xmask23 = vshlq_n_u64(xmask23, 20);

			senddesc01_w0 = vorrq_u64(senddesc01_w0, xmask01);
			senddesc23_w0 = vorrq_u64(senddesc23_w0, xmask23);

			/* Create 4W cmd for 4 mbufs (sendhdr, sgdesc) */
			cmd00 = vzip1q_u64(senddesc01_w0, senddesc01_w1);
			cmd01 = vzip1q_u64(sgdesc01_w0, sgdesc01_w1);
			cmd10 = vzip2q_u64(senddesc01_w0, senddesc01_w1);
			cmd11 = vzip2q_u64(sgdesc01_w0, sgdesc01_w1);
			cmd20 = vzip1q_u64(senddesc23_w0, senddesc23_w1);
			cmd21 = vzip1q_u64(sgdesc23_w0, sgdesc23_w1);
			cmd30 = vzip2q_u64(senddesc23_w0, senddesc23_w1);
			cmd31 = vzip2q_u64(sgdesc23_w0, sgdesc23_w1);
		}

		do {
			vst1q_u64(lmt_addr, cmd00);
			vst1q_u64(lmt_addr + 2, cmd01);
			vst1q_u64(lmt_addr + 4, cmd10);
			vst1q_u64(lmt_addr + 6, cmd11);
			vst1q_u64(lmt_addr + 8, cmd20);
			vst1q_u64(lmt_addr + 10, cmd21);
			vst1q_u64(lmt_addr + 12, cmd30);
			vst1q_u64(lmt_addr + 14, cmd31);
			lmt_status = otx2_lmt_submit(io_addr);

		} while (lmt_status == 0);
		tx_pkts = tx_pkts + NIX_DESCS_PER_LOOP;
	}

	if (unlikely(pkts_left))
		pkts += nix_xmit_pkts(tx_queue, tx_pkts, pkts_left, cmd, flags);

	return pkts;
}

#else
static __rte_always_inline uint16_t
nix_xmit_pkts_vector(void *tx_queue, struct rte_mbuf **tx_pkts,
		     uint16_t pkts, uint64_t *cmd, const uint16_t flags)
{
	RTE_SET_USED(tx_queue);
	RTE_SET_USED(tx_pkts);
	RTE_SET_USED(pkts);
	RTE_SET_USED(cmd);
	RTE_SET_USED(flags);
	return 0;
}
#endif

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
static uint16_t __rte_noinline	__rte_hot					\
otx2_nix_xmit_pkts_ ## name(void *tx_queue,				\
			struct rte_mbuf **tx_pkts, uint16_t pkts)	\
{									\
	uint64_t cmd[sz];						\
									\
	/* For TSO inner checksum is a must */				\
	if (((flags) & NIX_TX_OFFLOAD_TSO_F) &&				\
	    !((flags) & NIX_TX_OFFLOAD_L3_L4_CSUM_F))			\
		return 0;						\
	return nix_xmit_pkts(tx_queue, tx_pkts, pkts, cmd, flags);	\
}

NIX_TX_FASTPATH_MODES
#undef T

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
static uint16_t __rte_noinline	__rte_hot					\
otx2_nix_xmit_pkts_mseg_ ## name(void *tx_queue,			\
			struct rte_mbuf **tx_pkts, uint16_t pkts)	\
{									\
	uint64_t cmd[(sz) + NIX_TX_MSEG_SG_DWORDS - 2];			\
									\
	/* For TSO inner checksum is a must */				\
	if (((flags) & NIX_TX_OFFLOAD_TSO_F) &&				\
	    !((flags) & NIX_TX_OFFLOAD_L3_L4_CSUM_F))			\
		return 0;						\
	return nix_xmit_pkts_mseg(tx_queue, tx_pkts, pkts, cmd,		\
				  (flags) | NIX_TX_MULTI_SEG_F);	\
}

NIX_TX_FASTPATH_MODES
#undef T

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
static uint16_t __rte_noinline	__rte_hot					\
otx2_nix_xmit_pkts_vec_ ## name(void *tx_queue,				\
			struct rte_mbuf **tx_pkts, uint16_t pkts)	\
{									\
	uint64_t cmd[sz];						\
									\
	/* VLAN, TSTMP, TSO is not supported by vec */			\
	if ((flags) & NIX_TX_OFFLOAD_VLAN_QINQ_F ||			\
	    (flags) & NIX_TX_OFFLOAD_TSTAMP_F ||			\
	    (flags) & NIX_TX_OFFLOAD_TSO_F)				\
		return 0;						\
	return nix_xmit_pkts_vector(tx_queue, tx_pkts, pkts, cmd, (flags)); \
}

NIX_TX_FASTPATH_MODES
#undef T

static inline void
pick_tx_func(struct rte_eth_dev *eth_dev,
	     const eth_tx_burst_t tx_burst[2][2][2][2][2][2][2])
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	/* [SEC] [TSTMP] [NOFF] [VLAN] [OL3_OL4_CSUM] [IL3_IL4_CSUM] */
	eth_dev->tx_pkt_burst = tx_burst
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_SECURITY_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_TSO_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_TSTAMP_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_MBUF_NOFF_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_VLAN_QINQ_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_L3_L4_CSUM_F)];
}

void
otx2_eth_set_tx_function(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	const eth_tx_burst_t nix_eth_tx_burst[2][2][2][2][2][2][2] = {
#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
	[f6][f5][f4][f3][f2][f1][f0] =  otx2_nix_xmit_pkts_ ## name,

NIX_TX_FASTPATH_MODES
#undef T
	};

	const eth_tx_burst_t nix_eth_tx_burst_mseg[2][2][2][2][2][2][2] = {
#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
	[f6][f5][f4][f3][f2][f1][f0] =  otx2_nix_xmit_pkts_mseg_ ## name,

NIX_TX_FASTPATH_MODES
#undef T
	};

	const eth_tx_burst_t nix_eth_tx_vec_burst[2][2][2][2][2][2][2] = {
#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			\
	[f6][f5][f4][f3][f2][f1][f0] =  otx2_nix_xmit_pkts_vec_ ## name,

NIX_TX_FASTPATH_MODES
#undef T
	};

	if (dev->scalar_ena ||
	    (dev->tx_offload_flags &
	     (NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_TSTAMP_F |
	      NIX_TX_OFFLOAD_TSO_F)))
		pick_tx_func(eth_dev, nix_eth_tx_burst);
	else
		pick_tx_func(eth_dev, nix_eth_tx_vec_burst);

	if (dev->tx_offloads & DEV_TX_OFFLOAD_MULTI_SEGS)
		pick_tx_func(eth_dev, nix_eth_tx_burst_mseg);

	rte_mb();
}
