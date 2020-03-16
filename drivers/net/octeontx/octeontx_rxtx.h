/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef	__OCTEONTX_RXTX_H__
#define	__OCTEONTX_RXTX_H__

#include <rte_ethdev_driver.h>

#define OFFLOAD_FLAGS					\
	uint16_t rx_offload_flags;			\
	uint16_t tx_offload_flags

#define BIT(nr) (1UL << (nr))

#define OCCTX_RX_OFFLOAD_NONE		(0)
#define OCCTX_RX_OFFLOAD_RSS_F          BIT(0)
#define OCCTX_RX_VLAN_FLTR_F            BIT(1)
#define OCCTX_RX_MULTI_SEG_F		BIT(15)

#define OCCTX_TX_OFFLOAD_NONE		(0)
#define OCCTX_TX_OFFLOAD_MBUF_NOFF_F	BIT(3)

#define OCCTX_TX_MULTI_SEG_F		BIT(15)
/* Packet type table */
#define PTYPE_SIZE	OCCTX_PKI_LTYPE_LAST

static const uint32_t __rte_cache_aligned
ptype_table[PTYPE_SIZE][PTYPE_SIZE][PTYPE_SIZE] = {
	[LC_NONE][LE_NONE][LF_NONE] = RTE_PTYPE_UNKNOWN,
	[LC_NONE][LE_NONE][LF_IPSEC_ESP] = RTE_PTYPE_UNKNOWN,
	[LC_NONE][LE_NONE][LF_IPFRAG] = RTE_PTYPE_L4_FRAG,
	[LC_NONE][LE_NONE][LF_IPCOMP] = RTE_PTYPE_UNKNOWN,
	[LC_NONE][LE_NONE][LF_TCP] = RTE_PTYPE_L4_TCP,
	[LC_NONE][LE_NONE][LF_UDP] = RTE_PTYPE_L4_UDP,
	[LC_NONE][LE_NONE][LF_GRE] = RTE_PTYPE_TUNNEL_GRE,
	[LC_NONE][LE_NONE][LF_UDP_GENEVE] = RTE_PTYPE_TUNNEL_GENEVE,
	[LC_NONE][LE_NONE][LF_UDP_VXLAN] = RTE_PTYPE_TUNNEL_VXLAN,
	[LC_NONE][LE_NONE][LF_NVGRE] = RTE_PTYPE_TUNNEL_NVGRE,

	[LC_IPV4][LE_NONE][LF_NONE] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_UNKNOWN,
	[LC_IPV4][LE_NONE][LF_IPSEC_ESP] =
				RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L3_IPV4,
	[LC_IPV4][LE_NONE][LF_IPFRAG] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_FRAG,
	[LC_IPV4][LE_NONE][LF_IPCOMP] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_UNKNOWN,
	[LC_IPV4][LE_NONE][LF_TCP] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_TCP,
	[LC_IPV4][LE_NONE][LF_UDP] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP,
	[LC_IPV4][LE_NONE][LF_GRE] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_GRE,
	[LC_IPV4][LE_NONE][LF_UDP_GENEVE] =
				RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_GENEVE,
	[LC_IPV4][LE_NONE][LF_UDP_VXLAN] =
				RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_VXLAN,
	[LC_IPV4][LE_NONE][LF_NVGRE] =
				RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_NVGRE,

	[LC_IPV4_OPT][LE_NONE][LF_NONE] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_UNKNOWN,
	[LC_IPV4_OPT][LE_NONE][LF_IPSEC_ESP] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L3_IPV4,
	[LC_IPV4_OPT][LE_NONE][LF_IPFRAG] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_FRAG,
	[LC_IPV4_OPT][LE_NONE][LF_IPCOMP] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_UNKNOWN,
	[LC_IPV4_OPT][LE_NONE][LF_TCP] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_TCP,
	[LC_IPV4_OPT][LE_NONE][LF_UDP] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_L4_UDP,
	[LC_IPV4_OPT][LE_NONE][LF_GRE] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_TUNNEL_GRE,
	[LC_IPV4_OPT][LE_NONE][LF_UDP_GENEVE] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_TUNNEL_GENEVE,
	[LC_IPV4_OPT][LE_NONE][LF_UDP_VXLAN] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_TUNNEL_VXLAN,
	[LC_IPV4_OPT][LE_NONE][LF_NVGRE] =
				RTE_PTYPE_L3_IPV4_EXT | RTE_PTYPE_TUNNEL_NVGRE,

	[LC_IPV6][LE_NONE][LF_NONE] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_UNKNOWN,
	[LC_IPV6][LE_NONE][LF_IPSEC_ESP] =
				RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L3_IPV4,
	[LC_IPV6][LE_NONE][LF_IPFRAG] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_FRAG,
	[LC_IPV6][LE_NONE][LF_IPCOMP] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_UNKNOWN,
	[LC_IPV6][LE_NONE][LF_TCP] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_TCP,
	[LC_IPV6][LE_NONE][LF_UDP] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP,
	[LC_IPV6][LE_NONE][LF_GRE] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_TUNNEL_GRE,
	[LC_IPV6][LE_NONE][LF_UDP_GENEVE] =
				RTE_PTYPE_L3_IPV6 | RTE_PTYPE_TUNNEL_GENEVE,
	[LC_IPV6][LE_NONE][LF_UDP_VXLAN] =
				RTE_PTYPE_L3_IPV6 | RTE_PTYPE_TUNNEL_VXLAN,
	[LC_IPV6][LE_NONE][LF_NVGRE] =
				RTE_PTYPE_L3_IPV4 | RTE_PTYPE_TUNNEL_NVGRE,
	[LC_IPV6_OPT][LE_NONE][LF_NONE] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_UNKNOWN,
	[LC_IPV6_OPT][LE_NONE][LF_IPSEC_ESP] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L3_IPV4,
	[LC_IPV6_OPT][LE_NONE][LF_IPFRAG] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_FRAG,
	[LC_IPV6_OPT][LE_NONE][LF_IPCOMP] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_UNKNOWN,
	[LC_IPV6_OPT][LE_NONE][LF_TCP] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_TCP,
	[LC_IPV6_OPT][LE_NONE][LF_UDP] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_L4_UDP,
	[LC_IPV6_OPT][LE_NONE][LF_GRE] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_TUNNEL_GRE,
	[LC_IPV6_OPT][LE_NONE][LF_UDP_GENEVE] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_TUNNEL_GENEVE,
	[LC_IPV6_OPT][LE_NONE][LF_UDP_VXLAN] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_TUNNEL_VXLAN,
	[LC_IPV6_OPT][LE_NONE][LF_NVGRE] =
				RTE_PTYPE_L3_IPV6_EXT | RTE_PTYPE_TUNNEL_NVGRE,

};


static __rte_always_inline uint64_t
octeontx_pktmbuf_detach(struct rte_mbuf *m)
{
	struct rte_mempool *mp = m->pool;
	uint32_t mbuf_size, buf_len;
	struct rte_mbuf *md;
	uint16_t priv_size;
	uint16_t refcount;

	/* Update refcount of direct mbuf */
	md = rte_mbuf_from_indirect(m);
	refcount = rte_mbuf_refcnt_update(md, -1);

	priv_size = rte_pktmbuf_priv_size(mp);
	mbuf_size = (uint32_t)(sizeof(struct rte_mbuf) + priv_size);
	buf_len = rte_pktmbuf_data_room_size(mp);

	m->priv_size = priv_size;
	m->buf_addr = (char *)m + mbuf_size;
	m->buf_iova = rte_mempool_virt2iova(m) + mbuf_size;
	m->buf_len = (uint16_t)buf_len;
	rte_pktmbuf_reset_headroom(m);
	m->data_len = 0;
	m->ol_flags = 0;
	m->next = NULL;
	m->nb_segs = 1;

	/* Now indirect mbuf is safe to free */
	rte_pktmbuf_free(m);

	if (refcount == 0) {
		rte_mbuf_refcnt_set(md, 1);
		md->data_len = 0;
		md->ol_flags = 0;
		md->next = NULL;
		md->nb_segs = 1;
		return 0;
	} else {
		return 1;
	}
}

static __rte_always_inline uint64_t
octeontx_prefree_seg(struct rte_mbuf *m)
{
	if (likely(rte_mbuf_refcnt_read(m) == 1)) {
		if (!RTE_MBUF_DIRECT(m))
			return octeontx_pktmbuf_detach(m);

		m->next = NULL;
		m->nb_segs = 1;
		return 0;
	} else if (rte_mbuf_refcnt_update(m, -1) == 0) {
		if (!RTE_MBUF_DIRECT(m))
			return octeontx_pktmbuf_detach(m);

		rte_mbuf_refcnt_set(m, 1);
		m->next = NULL;
		m->nb_segs = 1;
		return 0;
	}

	/* Mbuf is having refcount more than 1 so need not to be freed */
	return 1;
}

static __rte_always_inline uint16_t
__octeontx_xmit_prepare(struct rte_mbuf *tx_pkt, uint64_t *cmd_buf,
			const uint16_t flag)
{
	uint16_t gaura_id, nb_desc = 0;

	/* Setup PKO_SEND_HDR_S */
	cmd_buf[nb_desc++] = tx_pkt->data_len & 0xffff;
	cmd_buf[nb_desc++] = 0x0;

	/* SEND_HDR[DF] bit controls if buffer is to be freed or
	 * not, as SG_DESC[I] and SEND_HDR[II] are clear.
	 */
	if (flag & OCCTX_TX_OFFLOAD_MBUF_NOFF_F)
		cmd_buf[0] |= (octeontx_prefree_seg(tx_pkt) <<
			       58);

	/* Mark mempool object as "put" since it is freed by PKO */
	if (!(cmd_buf[0] & (1ULL << 58)))
		__mempool_check_cookies(tx_pkt->pool, (void **)&tx_pkt,
					1, 0);
	/* Get the gaura Id */
	gaura_id = octeontx_fpa_bufpool_gpool((uintptr_t)
					      tx_pkt->pool->pool_id);

	/* Setup PKO_SEND_BUFLINK_S */
	cmd_buf[nb_desc++] = PKO_SEND_BUFLINK_SUBDC |
		PKO_SEND_BUFLINK_LDTYPE(0x1ull) |
		PKO_SEND_BUFLINK_GAUAR((long)gaura_id) |
		tx_pkt->data_len;
	cmd_buf[nb_desc++] = rte_mbuf_data_iova(tx_pkt);

	return nb_desc;
}

static __rte_always_inline uint16_t
__octeontx_xmit_mseg_prepare(struct rte_mbuf *tx_pkt, uint64_t *cmd_buf,
			const uint16_t flag)
{
	uint16_t nb_segs, nb_desc = 0;
	uint16_t gaura_id, len = 0;
	struct rte_mbuf *m_next = NULL;

	nb_segs = tx_pkt->nb_segs;
	/* Setup PKO_SEND_HDR_S */
	cmd_buf[nb_desc++] = tx_pkt->pkt_len & 0xffff;
	cmd_buf[nb_desc++] = 0x0;

	do {
		m_next = tx_pkt->next;
		/* To handle case where mbufs belong to diff pools, like
		 * fragmentation
		 */
		gaura_id = octeontx_fpa_bufpool_gpool((uintptr_t)
						      tx_pkt->pool->pool_id);

		/* Setup PKO_SEND_GATHER_S */
		cmd_buf[nb_desc] = PKO_SEND_GATHER_SUBDC		 |
				   PKO_SEND_GATHER_LDTYPE(0x1ull)	 |
				   PKO_SEND_GATHER_GAUAR((long)gaura_id) |
				   tx_pkt->data_len;

		/* SG_DESC[I] bit controls if buffer is to be freed or
		 * not, as SEND_HDR[DF] and SEND_HDR[II] are clear.
		 */
		if (flag & OCCTX_TX_OFFLOAD_MBUF_NOFF_F) {
			cmd_buf[nb_desc] |=
			     (octeontx_prefree_seg(tx_pkt) << 57);
		}

		/* Mark mempool object as "put" since it is freed by
		 * PKO.
		 */
		if (!(cmd_buf[nb_desc] & (1ULL << 57))) {
			tx_pkt->next = NULL;
			__mempool_check_cookies(tx_pkt->pool,
						(void **)&tx_pkt, 1, 0);
		}
		nb_desc++;

		cmd_buf[nb_desc++] = rte_mbuf_data_iova(tx_pkt);

		nb_segs--;
		len += tx_pkt->data_len;
		tx_pkt = m_next;
	} while (nb_segs);

	return nb_desc;
}

static __rte_always_inline uint16_t
__octeontx_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		     uint16_t nb_pkts, uint64_t *cmd_buf,
		     const uint16_t flags)
{
	struct octeontx_txq *txq = tx_queue;
	octeontx_dq_t *dq = &txq->dq;
	uint16_t count = 0, nb_desc;
	rte_cio_wmb();

	while (count < nb_pkts) {
		if (unlikely(*((volatile int64_t *)dq->fc_status_va) < 0))
			break;

		if (flags & OCCTX_TX_MULTI_SEG_F) {
			nb_desc = __octeontx_xmit_mseg_prepare(tx_pkts[count],
							       cmd_buf, flags);
		} else {
			nb_desc = __octeontx_xmit_prepare(tx_pkts[count],
							  cmd_buf, flags);
		}

		octeontx_reg_lmtst(dq->lmtline_va, dq->ioreg_va, cmd_buf,
				   nb_desc);

		count++;
	}
	return count;
}

uint16_t
octeontx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

#define NOFF_F       OCCTX_TX_OFFLOAD_MBUF_NOFF_F
#define MULT_F       OCCTX_TX_MULTI_SEG_F
/* [NOFF] [MULTI_SEG] */
#define OCCTX_TX_FASTPATH_MODES						      \
T(no_offload,				0, 0,	4,   OCCTX_TX_OFFLOAD_NONE)   \
T(mseg,					0, 1,	14,  MULT_F)		      \
T(noff,					1, 0,	4,   NOFF_F)		      \
T(noff_mseg,				1, 1,	14,  NOFF_F | MULT_F)

 #endif /* __OCTEONTX_RXTX_H__ */
