/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#include <rte_mbuf.h>
#include <rte_memory.h>

#include "base/ngbe_type.h"
#include "ngbe_ptypes.h"

/* The ngbe_ptype_lookup is used to convert from the 8-bit ptid in the
 * hardware to a bit-field that can be used by SW to more easily determine the
 * packet type.
 *
 * Macros are used to shorten the table lines and make this table human
 * readable.
 *
 * We store the PTYPE in the top byte of the bit field - this is just so that
 * we can check that the table doesn't have a row missing, as the index into
 * the table should be the PTYPE.
 */
#define TPTE(ptid, l2, l3, l4, tun, el2, el3, el4) \
	[ptid] = (RTE_PTYPE_L2_##l2 | \
		RTE_PTYPE_L3_##l3 | \
		RTE_PTYPE_L4_##l4 | \
		RTE_PTYPE_TUNNEL_##tun | \
		RTE_PTYPE_INNER_L2_##el2 | \
		RTE_PTYPE_INNER_L3_##el3 | \
		RTE_PTYPE_INNER_L4_##el4)

#define RTE_PTYPE_L2_NONE               0
#define RTE_PTYPE_L3_NONE               0
#define RTE_PTYPE_L4_NONE               0
#define RTE_PTYPE_TUNNEL_NONE           0
#define RTE_PTYPE_INNER_L2_NONE         0
#define RTE_PTYPE_INNER_L3_NONE         0
#define RTE_PTYPE_INNER_L4_NONE         0

static u32 ngbe_ptype_lookup[NGBE_PTID_MAX] __rte_cache_aligned = {
	/* L2:0-3 L3:4-7 L4:8-11 TUN:12-15 EL2:16-19 EL3:20-23 EL2:24-27 */
	/* L2: ETH */
	TPTE(0x10, ETHER,          NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x11, ETHER,          NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x12, ETHER_TIMESYNC, NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x13, ETHER_FIP,      NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x14, ETHER_LLDP,     NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x15, ETHER_CNM,      NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x16, ETHER_EAPOL,    NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x17, ETHER_ARP,      NONE, NONE, NONE, NONE, NONE, NONE),
	/* L2: Ethertype Filter */
	TPTE(0x18, ETHER_FILTER,   NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x19, ETHER_FILTER,   NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x1A, ETHER_FILTER,   NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x1B, ETHER_FILTER,   NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x1C, ETHER_FILTER,   NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x1D, ETHER_FILTER,   NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x1E, ETHER_FILTER,   NONE, NONE, NONE, NONE, NONE, NONE),
	TPTE(0x1F, ETHER_FILTER,   NONE, NONE, NONE, NONE, NONE, NONE),
	/* L3: IP */
	TPTE(0x20, ETHER, IPV4, NONFRAG, NONE, NONE, NONE, NONE),
	TPTE(0x21, ETHER, IPV4, FRAG,    NONE, NONE, NONE, NONE),
	TPTE(0x22, ETHER, IPV4, NONFRAG, NONE, NONE, NONE, NONE),
	TPTE(0x23, ETHER, IPV4, UDP,     NONE, NONE, NONE, NONE),
	TPTE(0x24, ETHER, IPV4, TCP,     NONE, NONE, NONE, NONE),
	TPTE(0x25, ETHER, IPV4, SCTP,    NONE, NONE, NONE, NONE),
	TPTE(0x29, ETHER, IPV6, FRAG,    NONE, NONE, NONE, NONE),
	TPTE(0x2A, ETHER, IPV6, NONFRAG, NONE, NONE, NONE, NONE),
	TPTE(0x2B, ETHER, IPV6, UDP,     NONE, NONE, NONE, NONE),
	TPTE(0x2C, ETHER, IPV6, TCP,     NONE, NONE, NONE, NONE),
	TPTE(0x2D, ETHER, IPV6, SCTP,    NONE, NONE, NONE, NONE),
	/* IPv4 -> IPv4/IPv6 */
	TPTE(0x81, ETHER, IPV4, NONE, IP, NONE, IPV4, FRAG),
	TPTE(0x82, ETHER, IPV4, NONE, IP, NONE, IPV4, NONFRAG),
	TPTE(0x83, ETHER, IPV4, NONE, IP, NONE, IPV4, UDP),
	TPTE(0x84, ETHER, IPV4, NONE, IP, NONE, IPV4, TCP),
	TPTE(0x85, ETHER, IPV4, NONE, IP, NONE, IPV4, SCTP),
	TPTE(0x89, ETHER, IPV4, NONE, IP, NONE, IPV6, FRAG),
	TPTE(0x8A, ETHER, IPV4, NONE, IP, NONE, IPV6, NONFRAG),
	TPTE(0x8B, ETHER, IPV4, NONE, IP, NONE, IPV6, UDP),
	TPTE(0x8C, ETHER, IPV4, NONE, IP, NONE, IPV6, TCP),
	TPTE(0x8D, ETHER, IPV4, NONE, IP, NONE, IPV6, SCTP),
	/* IPv6 -> IPv4/IPv6 */
	TPTE(0xC1, ETHER, IPV6, NONE, IP, NONE, IPV4, FRAG),
	TPTE(0xC2, ETHER, IPV6, NONE, IP, NONE, IPV4, NONFRAG),
	TPTE(0xC3, ETHER, IPV6, NONE, IP, NONE, IPV4, UDP),
	TPTE(0xC4, ETHER, IPV6, NONE, IP, NONE, IPV4, TCP),
	TPTE(0xC5, ETHER, IPV6, NONE, IP, NONE, IPV4, SCTP),
	TPTE(0xC9, ETHER, IPV6, NONE, IP, NONE, IPV6, FRAG),
	TPTE(0xCA, ETHER, IPV6, NONE, IP, NONE, IPV6, NONFRAG),
	TPTE(0xCB, ETHER, IPV6, NONE, IP, NONE, IPV6, UDP),
	TPTE(0xCC, ETHER, IPV6, NONE, IP, NONE, IPV6, TCP),
	TPTE(0xCD, ETHER, IPV6, NONE, IP, NONE, IPV6, SCTP),
};

u32 *ngbe_get_supported_ptypes(void)
{
	static u32 ptypes[] = {
		/* For non-vec functions,
		 * refers to ngbe_rxd_pkt_info_to_pkt_type();
		 */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_TUNNEL_IP,
		RTE_PTYPE_INNER_L3_IPV6,
		RTE_PTYPE_INNER_L3_IPV6_EXT,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	return ptypes;
}

static inline u8
ngbe_encode_ptype_mac(u32 ptype)
{
	u8 ptid;

	ptid = NGBE_PTID_PKT_MAC;

	switch (ptype & RTE_PTYPE_L2_MASK) {
	case RTE_PTYPE_UNKNOWN:
		break;
	case RTE_PTYPE_L2_ETHER_TIMESYNC:
		ptid |= NGBE_PTID_TYP_TS;
		break;
	case RTE_PTYPE_L2_ETHER_ARP:
		ptid |= NGBE_PTID_TYP_ARP;
		break;
	case RTE_PTYPE_L2_ETHER_LLDP:
		ptid |= NGBE_PTID_TYP_LLDP;
		break;
	default:
		ptid |= NGBE_PTID_TYP_MAC;
		break;
	}

	return ptid;
}

static inline u8
ngbe_encode_ptype_ip(u32 ptype)
{
	u8 ptid;

	ptid = NGBE_PTID_PKT_IP;

	switch (ptype & RTE_PTYPE_L3_MASK) {
	case RTE_PTYPE_L3_IPV4:
	case RTE_PTYPE_L3_IPV4_EXT:
	case RTE_PTYPE_L3_IPV4_EXT_UNKNOWN:
		break;
	case RTE_PTYPE_L3_IPV6:
	case RTE_PTYPE_L3_IPV6_EXT:
	case RTE_PTYPE_L3_IPV6_EXT_UNKNOWN:
		ptid |= NGBE_PTID_PKT_IPV6;
		break;
	default:
		return ngbe_encode_ptype_mac(ptype);
	}

	switch (ptype & RTE_PTYPE_L4_MASK) {
	case RTE_PTYPE_L4_TCP:
		ptid |= NGBE_PTID_TYP_TCP;
		break;
	case RTE_PTYPE_L4_UDP:
		ptid |= NGBE_PTID_TYP_UDP;
		break;
	case RTE_PTYPE_L4_SCTP:
		ptid |= NGBE_PTID_TYP_SCTP;
		break;
	case RTE_PTYPE_L4_FRAG:
		ptid |= NGBE_PTID_TYP_IPFRAG;
		break;
	default:
		ptid |= NGBE_PTID_TYP_IPDATA;
		break;
	}

	return ptid;
}

static inline u8
ngbe_encode_ptype_tunnel(u32 ptype)
{
	u8 ptid;

	ptid = NGBE_PTID_PKT_TUN;

	switch (ptype & RTE_PTYPE_L3_MASK) {
	case RTE_PTYPE_L3_IPV4:
	case RTE_PTYPE_L3_IPV4_EXT:
	case RTE_PTYPE_L3_IPV4_EXT_UNKNOWN:
		break;
	case RTE_PTYPE_L3_IPV6:
	case RTE_PTYPE_L3_IPV6_EXT:
	case RTE_PTYPE_L3_IPV6_EXT_UNKNOWN:
		ptid |= NGBE_PTID_TUN_IPV6;
		break;
	default:
		return ngbe_encode_ptype_ip(ptype);
	}

	/* VXLAN/GRE/Teredo/VXLAN-GPE are not supported in EM */
	switch (ptype & RTE_PTYPE_TUNNEL_MASK) {
	case RTE_PTYPE_TUNNEL_IP:
		ptid |= NGBE_PTID_TUN_EI;
		break;
	case RTE_PTYPE_TUNNEL_GRE:
	case RTE_PTYPE_TUNNEL_VXLAN_GPE:
		ptid |= NGBE_PTID_TUN_EIG;
		break;
	case RTE_PTYPE_TUNNEL_VXLAN:
	case RTE_PTYPE_TUNNEL_NVGRE:
	case RTE_PTYPE_TUNNEL_GENEVE:
	case RTE_PTYPE_TUNNEL_GRENAT:
		break;
	default:
		return ptid;
	}

	switch (ptype & RTE_PTYPE_INNER_L2_MASK) {
	case RTE_PTYPE_INNER_L2_ETHER:
		ptid |= NGBE_PTID_TUN_EIGM;
		break;
	case RTE_PTYPE_INNER_L2_ETHER_VLAN:
		ptid |= NGBE_PTID_TUN_EIGMV;
		break;
	case RTE_PTYPE_INNER_L2_ETHER_QINQ:
		ptid |= NGBE_PTID_TUN_EIGMV;
		break;
	default:
		break;
	}

	switch (ptype & RTE_PTYPE_INNER_L3_MASK) {
	case RTE_PTYPE_INNER_L3_IPV4:
	case RTE_PTYPE_INNER_L3_IPV4_EXT:
	case RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN:
		break;
	case RTE_PTYPE_INNER_L3_IPV6:
	case RTE_PTYPE_INNER_L3_IPV6_EXT:
	case RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN:
		ptid |= NGBE_PTID_PKT_IPV6;
		break;
	default:
		return ptid;
	}

	switch (ptype & RTE_PTYPE_INNER_L4_MASK) {
	case RTE_PTYPE_INNER_L4_TCP:
		ptid |= NGBE_PTID_TYP_TCP;
		break;
	case RTE_PTYPE_INNER_L4_UDP:
		ptid |= NGBE_PTID_TYP_UDP;
		break;
	case RTE_PTYPE_INNER_L4_SCTP:
		ptid |= NGBE_PTID_TYP_SCTP;
		break;
	case RTE_PTYPE_INNER_L4_FRAG:
		ptid |= NGBE_PTID_TYP_IPFRAG;
		break;
	default:
		ptid |= NGBE_PTID_TYP_IPDATA;
		break;
	}

	return ptid;
}

u32 ngbe_decode_ptype(u8 ptid)
{
	if (-1 != ngbe_etflt_id(ptid))
		return RTE_PTYPE_UNKNOWN;

	return ngbe_ptype_lookup[ptid];
}

u8 ngbe_encode_ptype(u32 ptype)
{
	u8 ptid = 0;

	if (ptype & RTE_PTYPE_TUNNEL_MASK)
		ptid = ngbe_encode_ptype_tunnel(ptype);
	else if (ptype & RTE_PTYPE_L3_MASK)
		ptid = ngbe_encode_ptype_ip(ptype);
	else if (ptype & RTE_PTYPE_L2_MASK)
		ptid = ngbe_encode_ptype_mac(ptype);
	else
		ptid = NGBE_PTID_NULL;

	return ptid;
}
