/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_common.h>
#include <rte_memzone.h>

#include "cnxk_ethdev.h"

#define SA_BASE_TBL_SZ	(RTE_MAX_ETHPORTS * sizeof(uintptr_t))
#define LOOKUP_ARRAY_SZ (PTYPE_ARRAY_SZ + ERR_ARRAY_SZ + SA_BASE_TBL_SZ)
const uint32_t *
cnxk_nix_supported_ptypes_get(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);

	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER_QINQ,      /* LB */
		RTE_PTYPE_L2_ETHER_VLAN,      /* LB */
		RTE_PTYPE_L2_ETHER_TIMESYNC,  /* LB */
		RTE_PTYPE_L2_ETHER_ARP,	      /* LC */
		RTE_PTYPE_L2_ETHER_NSH,	      /* LC */
		RTE_PTYPE_L2_ETHER_FCOE,      /* LC */
		RTE_PTYPE_L2_ETHER_MPLS,      /* LC */
		RTE_PTYPE_L3_IPV4,	      /* LC */
		RTE_PTYPE_L3_IPV4_EXT,	      /* LC */
		RTE_PTYPE_L3_IPV6,	      /* LC */
		RTE_PTYPE_L3_IPV6_EXT,	      /* LC */
		RTE_PTYPE_L4_TCP,	      /* LD */
		RTE_PTYPE_L4_UDP,	      /* LD */
		RTE_PTYPE_L4_SCTP,	      /* LD */
		RTE_PTYPE_L4_ICMP,	      /* LD */
		RTE_PTYPE_L4_IGMP,	      /* LD */
		RTE_PTYPE_TUNNEL_GRE,	      /* LD */
		RTE_PTYPE_TUNNEL_ESP,	      /* LD */
		RTE_PTYPE_TUNNEL_NVGRE,	      /* LD */
		RTE_PTYPE_TUNNEL_VXLAN,	      /* LE */
		RTE_PTYPE_TUNNEL_GENEVE,      /* LE */
		RTE_PTYPE_TUNNEL_GTPC,	      /* LE */
		RTE_PTYPE_TUNNEL_GTPU,	      /* LE */
		RTE_PTYPE_TUNNEL_VXLAN_GPE,   /* LE */
		RTE_PTYPE_TUNNEL_MPLS_IN_GRE, /* LE */
		RTE_PTYPE_TUNNEL_MPLS_IN_UDP, /* LE */
		RTE_PTYPE_INNER_L2_ETHER,     /* LF */
		RTE_PTYPE_INNER_L3_IPV4,      /* LG */
		RTE_PTYPE_INNER_L3_IPV6,      /* LG */
		RTE_PTYPE_INNER_L4_TCP,	      /* LH */
		RTE_PTYPE_INNER_L4_UDP,	      /* LH */
		RTE_PTYPE_INNER_L4_SCTP,      /* LH */
		RTE_PTYPE_INNER_L4_ICMP,      /* LH */
		RTE_PTYPE_UNKNOWN,
	};

	return ptypes;
}

/*
 * +------------------ +------------------ +
 * |  | IL4 | IL3| IL2 | TU | L4 | L3 | L2 |
 * +-------------------+-------------------+
 *
 * +-------------------+------------------ +
 * |  | LH | LG  | LF  | LE | LD | LC | LB |
 * +-------------------+-------------------+
 *
 * ptype       [LE - LD - LC - LB]  = TU  - L4 -  L3  - T2
 * ptype_tunnel[LH - LG - LF]  = IL4 - IL3 - IL2 - TU
 *
 */
static void
nix_create_non_tunnel_ptype_array(uint16_t *ptype)
{
	uint8_t lb, lc, ld, le;
	uint16_t val;
	uint32_t idx;

	for (idx = 0; idx < PTYPE_NON_TUNNEL_ARRAY_SZ; idx++) {
		lb = idx & 0xF;
		lc = (idx & 0xF0) >> 4;
		ld = (idx & 0xF00) >> 8;
		le = (idx & 0xF000) >> 12;
		val = RTE_PTYPE_UNKNOWN;

		switch (lb) {
		case NPC_LT_LB_STAG_QINQ:
			val |= RTE_PTYPE_L2_ETHER_QINQ;
			break;
		case NPC_LT_LB_CTAG:
			val |= RTE_PTYPE_L2_ETHER_VLAN;
			break;
		}

		switch (lc) {
		case NPC_LT_LC_ARP:
			val |= RTE_PTYPE_L2_ETHER_ARP;
			break;
		case NPC_LT_LC_NSH:
			val |= RTE_PTYPE_L2_ETHER_NSH;
			break;
		case NPC_LT_LC_FCOE:
			val |= RTE_PTYPE_L2_ETHER_FCOE;
			break;
		case NPC_LT_LC_MPLS:
			val |= RTE_PTYPE_L2_ETHER_MPLS;
			break;
		case NPC_LT_LC_IP:
			val |= RTE_PTYPE_L3_IPV4;
			break;
		case NPC_LT_LC_IP_OPT:
			val |= RTE_PTYPE_L3_IPV4_EXT;
			break;
		case NPC_LT_LC_IP6:
			val |= RTE_PTYPE_L3_IPV6;
			break;
		case NPC_LT_LC_IP6_EXT:
			val |= RTE_PTYPE_L3_IPV6_EXT;
			break;
		case NPC_LT_LC_PTP:
			val |= RTE_PTYPE_L2_ETHER_TIMESYNC;
			break;
		}

		switch (ld) {
		case NPC_LT_LD_TCP:
			val |= RTE_PTYPE_L4_TCP;
			break;
		case NPC_LT_LD_UDP:
			val |= RTE_PTYPE_L4_UDP;
			break;
		case NPC_LT_LD_SCTP:
			val |= RTE_PTYPE_L4_SCTP;
			break;
		case NPC_LT_LD_ICMP:
		case NPC_LT_LD_ICMP6:
			val |= RTE_PTYPE_L4_ICMP;
			break;
		case NPC_LT_LD_IGMP:
			val |= RTE_PTYPE_L4_IGMP;
			break;
		case NPC_LT_LD_GRE:
			val |= RTE_PTYPE_TUNNEL_GRE;
			break;
		case NPC_LT_LD_NVGRE:
			val |= RTE_PTYPE_TUNNEL_NVGRE;
			break;
		}

		switch (le) {
		case NPC_LT_LE_VXLAN:
			val |= RTE_PTYPE_TUNNEL_VXLAN;
			break;
		case NPC_LT_LE_ESP:
			val |= RTE_PTYPE_TUNNEL_ESP;
			break;
		case NPC_LT_LE_VXLANGPE:
			val |= RTE_PTYPE_TUNNEL_VXLAN_GPE;
			break;
		case NPC_LT_LE_GENEVE:
			val |= RTE_PTYPE_TUNNEL_GENEVE;
			break;
		case NPC_LT_LE_GTPC:
			val |= RTE_PTYPE_TUNNEL_GTPC;
			break;
		case NPC_LT_LE_GTPU:
			val |= RTE_PTYPE_TUNNEL_GTPU;
			break;
		case NPC_LT_LE_TU_MPLS_IN_GRE:
			val |= RTE_PTYPE_TUNNEL_MPLS_IN_GRE;
			break;
		case NPC_LT_LE_TU_MPLS_IN_UDP:
			val |= RTE_PTYPE_TUNNEL_MPLS_IN_UDP;
			break;
		}
		ptype[idx] = val;
	}
}

#define TU_SHIFT(x) ((x) >> PTYPE_NON_TUNNEL_WIDTH)
static void
nix_create_tunnel_ptype_array(uint16_t *ptype)
{
	uint8_t lf, lg, lh;
	uint16_t val;
	uint32_t idx;

	/* Skip non tunnel ptype array memory */
	ptype = ptype + PTYPE_NON_TUNNEL_ARRAY_SZ;

	for (idx = 0; idx < PTYPE_TUNNEL_ARRAY_SZ; idx++) {
		lf = idx & 0xF;
		lg = (idx & 0xF0) >> 4;
		lh = (idx & 0xF00) >> 8;
		val = RTE_PTYPE_UNKNOWN;

		switch (lf) {
		case NPC_LT_LF_TU_ETHER:
			val |= TU_SHIFT(RTE_PTYPE_INNER_L2_ETHER);
			break;
		}
		switch (lg) {
		case NPC_LT_LG_TU_IP:
			val |= TU_SHIFT(RTE_PTYPE_INNER_L3_IPV4);
			break;
		case NPC_LT_LG_TU_IP6:
			val |= TU_SHIFT(RTE_PTYPE_INNER_L3_IPV6);
			break;
		}
		switch (lh) {
		case NPC_LT_LH_TU_TCP:
			val |= TU_SHIFT(RTE_PTYPE_INNER_L4_TCP);
			break;
		case NPC_LT_LH_TU_UDP:
			val |= TU_SHIFT(RTE_PTYPE_INNER_L4_UDP);
			break;
		case NPC_LT_LH_TU_SCTP:
			val |= TU_SHIFT(RTE_PTYPE_INNER_L4_SCTP);
			break;
		case NPC_LT_LH_TU_ICMP:
		case NPC_LT_LH_TU_ICMP6:
			val |= TU_SHIFT(RTE_PTYPE_INNER_L4_ICMP);
			break;
		}

		ptype[idx] = val;
	}
}

static void
nix_create_rx_ol_flags_array(void *mem)
{
	uint16_t idx, errcode, errlev;
	uint32_t val, *ol_flags;

	/* Skip ptype array memory */
	ol_flags = (uint32_t *)((uint8_t *)mem + PTYPE_ARRAY_SZ);

	for (idx = 0; idx < BIT(ERRCODE_ERRLEN_WIDTH); idx++) {
		errlev = idx & 0xf;
		errcode = (idx & 0xff0) >> 4;

		val = RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN;
		val |= RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN;
		val |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_UNKNOWN;

		switch (errlev) {
		case NPC_ERRLEV_RE:
			/* Mark all errors as BAD checksum errors
			 * including Outer L2 length mismatch error
			 */
			if (errcode) {
				val |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
				val |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
			} else {
				val |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
				val |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
			}
			break;
		case NPC_ERRLEV_LC:
			if (errcode == NPC_EC_OIP4_CSUM ||
			    errcode == NPC_EC_IP_FRAG_OFFSET_1) {
				val |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
				val |= RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD;
			} else {
				val |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
			}
			break;
		case NPC_ERRLEV_LG:
			if (errcode == NPC_EC_IIP4_CSUM)
				val |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
			else
				val |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
			break;
		case NPC_ERRLEV_NIX:
			if (errcode == NIX_RX_PERRCODE_OL4_CHK ||
			    errcode == NIX_RX_PERRCODE_OL4_LEN ||
			    errcode == NIX_RX_PERRCODE_OL4_PORT) {
				val |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
				val |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
				val |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD;
			} else if (errcode == NIX_RX_PERRCODE_IL4_CHK ||
				   errcode == NIX_RX_PERRCODE_IL4_LEN ||
				   errcode == NIX_RX_PERRCODE_IL4_PORT) {
				val |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
				val |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
			} else if (errcode == NIX_RX_PERRCODE_IL3_LEN ||
				   errcode == NIX_RX_PERRCODE_OL3_LEN) {
				val |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
			} else {
				val |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
				val |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
			}
			break;
		}
		ol_flags[idx] = val;
	}
}

void *
cnxk_nix_fastpath_lookup_mem_get(void)
{
	const char name[] = CNXK_NIX_FASTPATH_LOOKUP_MEM;
	const struct rte_memzone *mz;
	void *mem;

	mz = rte_memzone_lookup(name);
	if (mz != NULL)
		return mz->addr;

	/* Request for the first time */
	mz = rte_memzone_reserve_aligned(name, LOOKUP_ARRAY_SZ, SOCKET_ID_ANY,
					 0, ROC_ALIGN);
	if (mz != NULL) {
		mem = mz->addr;
		/* Form the ptype array lookup memory */
		nix_create_non_tunnel_ptype_array(mem);
		nix_create_tunnel_ptype_array(mem);
		/* Form the rx ol_flags based on errcode */
		nix_create_rx_ol_flags_array(mem);
		return mem;
	}
	return NULL;
}

int
cnxk_nix_lookup_mem_sa_base_set(struct cnxk_eth_dev *dev)
{
	void *lookup_mem = cnxk_nix_fastpath_lookup_mem_get();
	uint16_t port = dev->eth_dev->data->port_id;
	uintptr_t sa_base_tbl;
	uintptr_t sa_base;
	uint8_t sa_w;

	if (!lookup_mem)
		return -EIO;

	sa_base = roc_nix_inl_inb_sa_base_get(&dev->nix, dev->inb.inl_dev);
	if (!sa_base)
		return -ENOTSUP;

	sa_w = plt_log2_u32(dev->nix.ipsec_in_max_spi + 1);

	/* Set SA Base in lookup mem */
	sa_base_tbl = (uintptr_t)lookup_mem;
	sa_base_tbl += PTYPE_ARRAY_SZ + ERR_ARRAY_SZ;
	*((uintptr_t *)sa_base_tbl + port) = sa_base | sa_w;
	return 0;
}

int
cnxk_nix_lookup_mem_sa_base_clear(struct cnxk_eth_dev *dev)
{
	void *lookup_mem = cnxk_nix_fastpath_lookup_mem_get();
	uint16_t port = dev->eth_dev->data->port_id;
	uintptr_t sa_base_tbl;

	if (!lookup_mem)
		return -EIO;

	/* Set SA Base in lookup mem */
	sa_base_tbl = (uintptr_t)lookup_mem;
	sa_base_tbl += PTYPE_ARRAY_SZ + ERR_ARRAY_SZ;
	*((uintptr_t *)sa_base_tbl + port) = 0;
	return 0;
}
