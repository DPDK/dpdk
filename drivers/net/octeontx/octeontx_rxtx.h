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
#define OCCTX_RX_MULTI_SEG_F		BIT(15)

#define OCCTX_TX_OFFLOAD_NONE		(0)

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

static __rte_always_inline int
__octeontx_xmit_pkts(void *lmtline_va, void *ioreg_va, int64_t *fc_status_va,
			struct rte_mbuf *tx_pkt, const uint16_t flag)
{
	uint8_t sz = (4 + (!!(flag & OCCTX_TX_MULTI_SEG_F) * 10));
	/* Max size of PKO SEND desc is 112 bytes*/
	uint64_t cmd_buf[sz] __rte_cache_aligned;
	uint8_t nb_segs, nb_desc = 0;
	uint16_t gaura_id, len = 0;
	struct rte_mbuf *m_next = NULL;

	if (unlikely(*((volatile int64_t *)fc_status_va) < 0))
		return -ENOSPC;


	if (flag & OCCTX_TX_MULTI_SEG_F) {
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
			cmd_buf[nb_desc] = PKO_SEND_GATHER_SUBDC           |
					     PKO_SEND_GATHER_LDTYPE(0x1ull)  |
					     PKO_SEND_GATHER_GAUAR((long)
								   gaura_id) |
					     tx_pkt->data_len;
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
	} else {
		/* Setup PKO_SEND_HDR_S */
		cmd_buf[nb_desc++] = tx_pkt->data_len & 0xffff;
		cmd_buf[nb_desc++] = 0x0;

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
	}
	octeontx_reg_lmtst(lmtline_va, ioreg_va, cmd_buf, nb_desc);

	return 0;
}

uint16_t
octeontx_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

uint16_t
octeontx_xmit_pkts_mseg(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);

uint16_t
octeontx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

#endif /* __OCTEONTX_RXTX_H__ */
