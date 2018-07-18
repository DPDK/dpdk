/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_EF10_RX_EV_H
#define _SFC_EF10_RX_EV_H

#include <rte_mbuf.h>

#include "efx_types.h"
#include "efx_regs.h"
#include "efx_regs_ef10.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void
sfc_ef10_rx_ev_to_offloads(const efx_qword_t rx_ev, struct rte_mbuf *m,
			   uint64_t ol_mask)
{
	uint32_t tun_ptype = 0;
	/* Which event bit is mapped to PKT_RX_IP_CKSUM_* */
	int8_t ip_csum_err_bit;
	/* Which event bit is mapped to PKT_RX_L4_CKSUM_* */
	int8_t l4_csum_err_bit;
	uint32_t l2_ptype = 0;
	uint32_t l3_ptype = 0;
	uint32_t l4_ptype = 0;
	uint64_t ol_flags = 0;

	if (unlikely(rx_ev.eq_u64[0] &
		rte_cpu_to_le_64((1ull << ESF_DZ_RX_ECC_ERR_LBN) |
				 (1ull << ESF_DZ_RX_ECRC_ERR_LBN) |
				 (1ull << ESF_DZ_RX_PARSE_INCOMPLETE_LBN)))) {
		/* Zero packet type is used as a marker to dicard bad packets */
		goto done;
	}

#if SFC_EF10_RX_EV_ENCAP_SUPPORT
	switch (EFX_QWORD_FIELD(rx_ev, ESF_EZ_RX_ENCAP_HDR)) {
	default:
		/* Unexpected encapsulation tag class */
		SFC_ASSERT(false);
		/* FALLTHROUGH */
	case ESE_EZ_ENCAP_HDR_NONE:
		break;
	case ESE_EZ_ENCAP_HDR_VXLAN:
		/*
		 * It is definitely UDP, but we have no information
		 * about IPv4 vs IPv6 and VLAN tagging.
		 */
		tun_ptype = RTE_PTYPE_TUNNEL_VXLAN | RTE_PTYPE_L4_UDP;
		break;
	case ESE_EZ_ENCAP_HDR_GRE:
		/*
		 * We have no information about IPv4 vs IPv6 and VLAN tagging.
		 */
		tun_ptype = RTE_PTYPE_TUNNEL_NVGRE;
		break;
	}
#endif

	if (tun_ptype == 0) {
		ip_csum_err_bit = ESF_DZ_RX_IPCKSUM_ERR_LBN;
		l4_csum_err_bit = ESF_DZ_RX_TCPUDP_CKSUM_ERR_LBN;
	} else {
		ip_csum_err_bit = ESF_EZ_RX_IP_INNER_CHKSUM_ERR_LBN;
		l4_csum_err_bit = ESF_EZ_RX_TCP_UDP_INNER_CHKSUM_ERR_LBN;
		if (unlikely(EFX_TEST_QWORD_BIT(rx_ev,
						ESF_DZ_RX_IPCKSUM_ERR_LBN)))
			ol_flags |= PKT_RX_EIP_CKSUM_BAD;
	}

	switch (EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_ETH_TAG_CLASS)) {
	case ESE_DZ_ETH_TAG_CLASS_NONE:
		l2_ptype = (tun_ptype == 0) ? RTE_PTYPE_L2_ETHER :
			RTE_PTYPE_INNER_L2_ETHER;
		break;
	case ESE_DZ_ETH_TAG_CLASS_VLAN1:
		l2_ptype = (tun_ptype == 0) ? RTE_PTYPE_L2_ETHER_VLAN :
			RTE_PTYPE_INNER_L2_ETHER_VLAN;
		break;
	case ESE_DZ_ETH_TAG_CLASS_VLAN2:
		l2_ptype = (tun_ptype == 0) ? RTE_PTYPE_L2_ETHER_QINQ :
			RTE_PTYPE_INNER_L2_ETHER_QINQ;
		break;
	default:
		/* Unexpected Eth tag class */
		SFC_ASSERT(false);
	}

	switch (EFX_QWORD_FIELD(rx_ev, ESF_DZ_RX_L3_CLASS)) {
	case ESE_DZ_L3_CLASS_IP4_FRAG:
		l4_ptype = (tun_ptype == 0) ? RTE_PTYPE_L4_FRAG :
			RTE_PTYPE_INNER_L4_FRAG;
		/* FALLTHROUGH */
	case ESE_DZ_L3_CLASS_IP4:
		l3_ptype = (tun_ptype == 0) ? RTE_PTYPE_L3_IPV4_EXT_UNKNOWN :
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN;
		ol_flags |= PKT_RX_RSS_HASH |
			((EFX_TEST_QWORD_BIT(rx_ev, ip_csum_err_bit)) ?
			 PKT_RX_IP_CKSUM_BAD : PKT_RX_IP_CKSUM_GOOD);
		break;
	case ESE_DZ_L3_CLASS_IP6_FRAG:
		l4_ptype = (tun_ptype == 0) ? RTE_PTYPE_L4_FRAG :
			RTE_PTYPE_INNER_L4_FRAG;
		/* FALLTHROUGH */
	case ESE_DZ_L3_CLASS_IP6:
		l3_ptype = (tun_ptype == 0) ? RTE_PTYPE_L3_IPV6_EXT_UNKNOWN :
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN;
		ol_flags |= PKT_RX_RSS_HASH;
		break;
	case ESE_DZ_L3_CLASS_ARP:
		/* Override Layer 2 packet type */
		/* There is no ARP classification for inner packets */
		if (tun_ptype == 0)
			l2_ptype = RTE_PTYPE_L2_ETHER_ARP;
		break;
	case ESE_DZ_L3_CLASS_UNKNOWN:
		break;
	default:
		/* Unexpected Layer 3 class */
		SFC_ASSERT(false);
	}

	/*
	 * RX_L4_CLASS is 3 bits wide on Huntington and Medford, but is only
	 * 2 bits wide on Medford2. Check it is safe to use the Medford2 field
	 * and values for all EF10 controllers.
	 */
	RTE_BUILD_BUG_ON(ESF_FZ_RX_L4_CLASS_LBN != ESF_DE_RX_L4_CLASS_LBN);
	switch (EFX_QWORD_FIELD(rx_ev, ESF_FZ_RX_L4_CLASS)) {
	case ESE_FZ_L4_CLASS_TCP:
		 RTE_BUILD_BUG_ON(ESE_FZ_L4_CLASS_TCP != ESE_DE_L4_CLASS_TCP);
		l4_ptype = (tun_ptype == 0) ? RTE_PTYPE_L4_TCP :
			RTE_PTYPE_INNER_L4_TCP;
		ol_flags |=
			(EFX_TEST_QWORD_BIT(rx_ev, l4_csum_err_bit)) ?
			PKT_RX_L4_CKSUM_BAD : PKT_RX_L4_CKSUM_GOOD;
		break;
	case ESE_FZ_L4_CLASS_UDP:
		 RTE_BUILD_BUG_ON(ESE_FZ_L4_CLASS_UDP != ESE_DE_L4_CLASS_UDP);
		l4_ptype = (tun_ptype == 0) ? RTE_PTYPE_L4_UDP :
			RTE_PTYPE_INNER_L4_UDP;
		ol_flags |=
			(EFX_TEST_QWORD_BIT(rx_ev, l4_csum_err_bit)) ?
			PKT_RX_L4_CKSUM_BAD : PKT_RX_L4_CKSUM_GOOD;
		break;
	case ESE_FZ_L4_CLASS_UNKNOWN:
		 RTE_BUILD_BUG_ON(ESE_FZ_L4_CLASS_UNKNOWN !=
				  ESE_DE_L4_CLASS_UNKNOWN);
		break;
	default:
		/* Unexpected Layer 4 class */
		SFC_ASSERT(false);
	}

	SFC_ASSERT(l2_ptype != 0);

done:
	m->ol_flags = ol_flags & ol_mask;
	m->packet_type = tun_ptype | l2_ptype | l3_ptype | l4_ptype;
}


#ifdef __cplusplus
}
#endif
#endif /* _SFC_EF10_RX_EV_H */
