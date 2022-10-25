/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <rte_malloc.h>
#include <rte_tailq.h>
#include "base/i40e_prototype.h"
#include "i40e_logs.h"
#include "i40e_ethdev.h"
#include "i40e_hash.h"

#ifndef BIT
#define BIT(n)				(1UL << (n))
#endif

#ifndef BIT_ULL
#define BIT_ULL(n)			(1ULL << (n))
#endif

/* Pattern item headers */
#define I40E_HASH_HDR_ETH		0x01ULL
#define I40E_HASH_HDR_IPV4		0x10ULL
#define I40E_HASH_HDR_IPV6		0x20ULL
#define I40E_HASH_HDR_IPV6_FRAG		0x40ULL
#define I40E_HASH_HDR_TCP		0x100ULL
#define I40E_HASH_HDR_UDP		0x200ULL
#define I40E_HASH_HDR_SCTP		0x400ULL
#define I40E_HASH_HDR_ESP		0x10000ULL
#define I40E_HASH_HDR_L2TPV3		0x20000ULL
#define I40E_HASH_HDR_AH		0x40000ULL
#define I40E_HASH_HDR_GTPC		0x100000ULL
#define I40E_HASH_HDR_GTPU		0x200000ULL

#define I40E_HASH_HDR_INNER_SHIFT	32
#define I40E_HASH_HDR_IPV4_INNER	(I40E_HASH_HDR_IPV4 << \
					I40E_HASH_HDR_INNER_SHIFT)
#define I40E_HASH_HDR_IPV6_INNER	(I40E_HASH_HDR_IPV6 << \
					I40E_HASH_HDR_INNER_SHIFT)

/* ETH */
#define I40E_PHINT_ETH			I40E_HASH_HDR_ETH

/* IPv4 */
#define I40E_PHINT_IPV4			(I40E_HASH_HDR_ETH | I40E_HASH_HDR_IPV4)
#define I40E_PHINT_IPV4_TCP		(I40E_PHINT_IPV4 | I40E_HASH_HDR_TCP)
#define I40E_PHINT_IPV4_UDP		(I40E_PHINT_IPV4 | I40E_HASH_HDR_UDP)
#define I40E_PHINT_IPV4_SCTP		(I40E_PHINT_IPV4 | I40E_HASH_HDR_SCTP)

/* IPv6 */
#define I40E_PHINT_IPV6			(I40E_HASH_HDR_ETH | I40E_HASH_HDR_IPV6)
#define I40E_PHINT_IPV6_FRAG		(I40E_PHINT_IPV6 | \
					 I40E_HASH_HDR_IPV6_FRAG)
#define I40E_PHINT_IPV6_TCP		(I40E_PHINT_IPV6 | I40E_HASH_HDR_TCP)
#define I40E_PHINT_IPV6_UDP		(I40E_PHINT_IPV6 | I40E_HASH_HDR_UDP)
#define I40E_PHINT_IPV6_SCTP		(I40E_PHINT_IPV6 | I40E_HASH_HDR_SCTP)

/* ESP */
#define I40E_PHINT_IPV4_ESP		(I40E_PHINT_IPV4 | I40E_HASH_HDR_ESP)
#define I40E_PHINT_IPV6_ESP		(I40E_PHINT_IPV6 | I40E_HASH_HDR_ESP)
#define I40E_PHINT_IPV4_UDP_ESP		(I40E_PHINT_IPV4_UDP | \
					I40E_HASH_HDR_ESP)
#define I40E_PHINT_IPV6_UDP_ESP		(I40E_PHINT_IPV6_UDP | \
					I40E_HASH_HDR_ESP)

/* GTPC */
#define I40E_PHINT_IPV4_GTPC		(I40E_PHINT_IPV4_UDP | \
					I40E_HASH_HDR_GTPC)
#define I40E_PHINT_IPV6_GTPC		(I40E_PHINT_IPV6_UDP | \
					I40E_HASH_HDR_GTPC)

/* GTPU */
#define I40E_PHINT_IPV4_GTPU		(I40E_PHINT_IPV4_UDP | \
					I40E_HASH_HDR_GTPU)
#define I40E_PHINT_IPV4_GTPU_IPV4	(I40E_PHINT_IPV4_GTPU | \
					I40E_HASH_HDR_IPV4_INNER)
#define I40E_PHINT_IPV4_GTPU_IPV6	(I40E_PHINT_IPV4_GTPU | \
					I40E_HASH_HDR_IPV6_INNER)
#define I40E_PHINT_IPV6_GTPU		(I40E_PHINT_IPV6_UDP | \
					I40E_HASH_HDR_GTPU)
#define I40E_PHINT_IPV6_GTPU_IPV4	(I40E_PHINT_IPV6_GTPU | \
					I40E_HASH_HDR_IPV4_INNER)
#define I40E_PHINT_IPV6_GTPU_IPV6	(I40E_PHINT_IPV6_GTPU | \
					I40E_HASH_HDR_IPV6_INNER)

/* L2TPV3 */
#define I40E_PHINT_IPV4_L2TPV3		(I40E_PHINT_IPV4 | I40E_HASH_HDR_L2TPV3)
#define I40E_PHINT_IPV6_L2TPV3		(I40E_PHINT_IPV6 | I40E_HASH_HDR_L2TPV3)

/* AH */
#define I40E_PHINT_IPV4_AH		(I40E_PHINT_IPV4 | I40E_HASH_HDR_AH)
#define I40E_PHINT_IPV6_AH		(I40E_PHINT_IPV6 | I40E_HASH_HDR_AH)

/* Structure of mapping RSS type to input set */
struct i40e_hash_map_rss_inset {
	uint64_t rss_type;
	uint64_t inset;
};

const struct i40e_hash_map_rss_inset i40e_hash_rss_inset[] = {
	/* IPv4 */
	{ RTE_ETH_RSS_IPV4, I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST },
	{ RTE_ETH_RSS_FRAG_IPV4, I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST },

	{ RTE_ETH_RSS_NONFRAG_IPV4_OTHER,
	  I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST },

	{ RTE_ETH_RSS_NONFRAG_IPV4_TCP, I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT },

	{ RTE_ETH_RSS_NONFRAG_IPV4_UDP, I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT },

	{ RTE_ETH_RSS_NONFRAG_IPV4_SCTP, I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT | I40E_INSET_SCTP_VT },

	/* IPv6 */
	{ RTE_ETH_RSS_IPV6, I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST },
	{ RTE_ETH_RSS_FRAG_IPV6, I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST },

	{ RTE_ETH_RSS_NONFRAG_IPV6_OTHER,
	  I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST },

	{ RTE_ETH_RSS_NONFRAG_IPV6_TCP, I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT },

	{ RTE_ETH_RSS_NONFRAG_IPV6_UDP, I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT },

	{ RTE_ETH_RSS_NONFRAG_IPV6_SCTP, I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT | I40E_INSET_SCTP_VT },

	/* Port */
	{ RTE_ETH_RSS_PORT, I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT },

	/* Ether */
	{ RTE_ETH_RSS_L2_PAYLOAD, I40E_INSET_LAST_ETHER_TYPE },
	{ RTE_ETH_RSS_ETH, I40E_INSET_DMAC | I40E_INSET_SMAC },

	/* VLAN */
	{ RTE_ETH_RSS_S_VLAN, I40E_INSET_VLAN_OUTER },
	{ RTE_ETH_RSS_C_VLAN, I40E_INSET_VLAN_INNER },
};

#define I40E_HASH_VOID_NEXT_ALLOW	BIT_ULL(RTE_FLOW_ITEM_TYPE_ETH)

#define I40E_HASH_ETH_NEXT_ALLOW	(BIT_ULL(RTE_FLOW_ITEM_TYPE_IPV4) | \
					BIT_ULL(RTE_FLOW_ITEM_TYPE_IPV6) | \
					BIT_ULL(RTE_FLOW_ITEM_TYPE_VLAN))

#define I40E_HASH_IP_NEXT_ALLOW		(BIT_ULL(RTE_FLOW_ITEM_TYPE_TCP) | \
					BIT_ULL(RTE_FLOW_ITEM_TYPE_UDP) | \
					BIT_ULL(RTE_FLOW_ITEM_TYPE_SCTP) | \
					BIT_ULL(RTE_FLOW_ITEM_TYPE_ESP) | \
					BIT_ULL(RTE_FLOW_ITEM_TYPE_L2TPV3OIP) |\
					BIT_ULL(RTE_FLOW_ITEM_TYPE_AH))

#define I40E_HASH_IPV6_NEXT_ALLOW	(I40E_HASH_IP_NEXT_ALLOW | \
					BIT_ULL(RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT))

#define I40E_HASH_UDP_NEXT_ALLOW	(BIT_ULL(RTE_FLOW_ITEM_TYPE_GTPU) | \
					BIT_ULL(RTE_FLOW_ITEM_TYPE_GTPC))

#define I40E_HASH_GTPU_NEXT_ALLOW	(BIT_ULL(RTE_FLOW_ITEM_TYPE_IPV4) | \
					BIT_ULL(RTE_FLOW_ITEM_TYPE_IPV6))

static const uint64_t pattern_next_allow_items[] = {
	[RTE_FLOW_ITEM_TYPE_VOID] = I40E_HASH_VOID_NEXT_ALLOW,
	[RTE_FLOW_ITEM_TYPE_ETH] = I40E_HASH_ETH_NEXT_ALLOW,
	[RTE_FLOW_ITEM_TYPE_IPV4] = I40E_HASH_IP_NEXT_ALLOW,
	[RTE_FLOW_ITEM_TYPE_IPV6] = I40E_HASH_IPV6_NEXT_ALLOW,
	[RTE_FLOW_ITEM_TYPE_UDP] = I40E_HASH_UDP_NEXT_ALLOW,
	[RTE_FLOW_ITEM_TYPE_GTPU] = I40E_HASH_GTPU_NEXT_ALLOW,
};

static const uint64_t pattern_item_header[] = {
	[RTE_FLOW_ITEM_TYPE_ETH] = I40E_HASH_HDR_ETH,
	[RTE_FLOW_ITEM_TYPE_IPV4] = I40E_HASH_HDR_IPV4,
	[RTE_FLOW_ITEM_TYPE_IPV6] = I40E_HASH_HDR_IPV6,
	[RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT] = I40E_HASH_HDR_IPV6_FRAG,
	[RTE_FLOW_ITEM_TYPE_TCP] = I40E_HASH_HDR_TCP,
	[RTE_FLOW_ITEM_TYPE_UDP] = I40E_HASH_HDR_UDP,
	[RTE_FLOW_ITEM_TYPE_SCTP] = I40E_HASH_HDR_SCTP,
	[RTE_FLOW_ITEM_TYPE_ESP] = I40E_HASH_HDR_ESP,
	[RTE_FLOW_ITEM_TYPE_GTPC] = I40E_HASH_HDR_GTPC,
	[RTE_FLOW_ITEM_TYPE_GTPU] = I40E_HASH_HDR_GTPU,
	[RTE_FLOW_ITEM_TYPE_L2TPV3OIP] = I40E_HASH_HDR_L2TPV3,
	[RTE_FLOW_ITEM_TYPE_AH] = I40E_HASH_HDR_AH,
};

/* Structure of matched pattern */
struct i40e_hash_match_pattern {
	uint64_t pattern_type;
	uint64_t rss_mask;	/* Supported RSS type for this pattern */
	bool custom_pctype_flag;/* true for custom packet type */
	uint8_t pctype;
};

#define I40E_HASH_MAP_PATTERN(pattern, rss_mask, pctype) { \
	pattern, rss_mask, false, pctype  }

#define I40E_HASH_MAP_CUS_PATTERN(pattern, rss_mask, cus_pctype) { \
	pattern, rss_mask, true, cus_pctype }

#define I40E_HASH_L2_RSS_MASK		(RTE_ETH_RSS_VLAN | RTE_ETH_RSS_ETH | \
					RTE_ETH_RSS_L2_SRC_ONLY | \
					RTE_ETH_RSS_L2_DST_ONLY)

#define I40E_HASH_L23_RSS_MASK		(I40E_HASH_L2_RSS_MASK | \
					RTE_ETH_RSS_L3_SRC_ONLY | \
					RTE_ETH_RSS_L3_DST_ONLY)

#define I40E_HASH_IPV4_L23_RSS_MASK	(RTE_ETH_RSS_IPV4 | I40E_HASH_L23_RSS_MASK)
#define I40E_HASH_IPV6_L23_RSS_MASK	(RTE_ETH_RSS_IPV6 | I40E_HASH_L23_RSS_MASK)

#define I40E_HASH_L234_RSS_MASK		(I40E_HASH_L23_RSS_MASK | \
					RTE_ETH_RSS_PORT | RTE_ETH_RSS_L4_SRC_ONLY | \
					RTE_ETH_RSS_L4_DST_ONLY)

#define I40E_HASH_IPV4_L234_RSS_MASK	(I40E_HASH_L234_RSS_MASK | RTE_ETH_RSS_IPV4)
#define I40E_HASH_IPV6_L234_RSS_MASK	(I40E_HASH_L234_RSS_MASK | RTE_ETH_RSS_IPV6)

#define I40E_HASH_L4_TYPES		(RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
					RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
					RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
					RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
					RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
					RTE_ETH_RSS_NONFRAG_IPV6_SCTP)

/* Current supported patterns and RSS types.
 * All items that have the same pattern types are together.
 */
static const struct i40e_hash_match_pattern match_patterns[] = {
	/* Ether */
	I40E_HASH_MAP_PATTERN(I40E_PHINT_ETH,
			      RTE_ETH_RSS_L2_PAYLOAD | I40E_HASH_L2_RSS_MASK,
			      I40E_FILTER_PCTYPE_L2_PAYLOAD),

	/* IPv4 */
	I40E_HASH_MAP_PATTERN(I40E_PHINT_IPV4,
			      RTE_ETH_RSS_FRAG_IPV4 | I40E_HASH_IPV4_L23_RSS_MASK,
			      I40E_FILTER_PCTYPE_FRAG_IPV4),

	I40E_HASH_MAP_PATTERN(I40E_PHINT_IPV4,
			      RTE_ETH_RSS_NONFRAG_IPV4_OTHER |
			      I40E_HASH_IPV4_L23_RSS_MASK,
			      I40E_FILTER_PCTYPE_NONF_IPV4_OTHER),

	I40E_HASH_MAP_PATTERN(I40E_PHINT_IPV4_TCP,
			      RTE_ETH_RSS_NONFRAG_IPV4_TCP |
			      I40E_HASH_IPV4_L234_RSS_MASK,
			      I40E_FILTER_PCTYPE_NONF_IPV4_TCP),

	I40E_HASH_MAP_PATTERN(I40E_PHINT_IPV4_UDP,
			      RTE_ETH_RSS_NONFRAG_IPV4_UDP |
			      I40E_HASH_IPV4_L234_RSS_MASK,
			      I40E_FILTER_PCTYPE_NONF_IPV4_UDP),

	I40E_HASH_MAP_PATTERN(I40E_PHINT_IPV4_SCTP,
			      RTE_ETH_RSS_NONFRAG_IPV4_SCTP |
			      I40E_HASH_IPV4_L234_RSS_MASK,
			      I40E_FILTER_PCTYPE_NONF_IPV4_SCTP),

	/* IPv6 */
	I40E_HASH_MAP_PATTERN(I40E_PHINT_IPV6,
			      RTE_ETH_RSS_FRAG_IPV6 | I40E_HASH_IPV6_L23_RSS_MASK,
			      I40E_FILTER_PCTYPE_FRAG_IPV6),

	I40E_HASH_MAP_PATTERN(I40E_PHINT_IPV6,
			      RTE_ETH_RSS_NONFRAG_IPV6_OTHER |
			      I40E_HASH_IPV6_L23_RSS_MASK,
			      I40E_FILTER_PCTYPE_NONF_IPV6_OTHER),

	I40E_HASH_MAP_PATTERN(I40E_PHINT_IPV6_FRAG,
			      RTE_ETH_RSS_FRAG_IPV6 | I40E_HASH_L23_RSS_MASK,
			      I40E_FILTER_PCTYPE_FRAG_IPV6),

	I40E_HASH_MAP_PATTERN(I40E_PHINT_IPV6_TCP,
			      RTE_ETH_RSS_NONFRAG_IPV6_TCP |
			      I40E_HASH_IPV6_L234_RSS_MASK,
			      I40E_FILTER_PCTYPE_NONF_IPV6_TCP),

	I40E_HASH_MAP_PATTERN(I40E_PHINT_IPV6_UDP,
			      RTE_ETH_RSS_NONFRAG_IPV6_UDP |
			      I40E_HASH_IPV6_L234_RSS_MASK,
			      I40E_FILTER_PCTYPE_NONF_IPV6_UDP),

	I40E_HASH_MAP_PATTERN(I40E_PHINT_IPV6_SCTP,
			      RTE_ETH_RSS_NONFRAG_IPV6_SCTP |
			      I40E_HASH_IPV6_L234_RSS_MASK,
			      I40E_FILTER_PCTYPE_NONF_IPV6_SCTP),

	/* ESP */
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV4_ESP,
				  RTE_ETH_RSS_ESP, I40E_CUSTOMIZED_ESP_IPV4),
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV6_ESP,
				  RTE_ETH_RSS_ESP, I40E_CUSTOMIZED_ESP_IPV6),
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV4_UDP_ESP,
				  RTE_ETH_RSS_ESP, I40E_CUSTOMIZED_ESP_IPV4_UDP),
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV6_UDP_ESP,
				  RTE_ETH_RSS_ESP, I40E_CUSTOMIZED_ESP_IPV6_UDP),

	/* GTPC */
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV4_GTPC,
				  I40E_HASH_IPV4_L234_RSS_MASK,
				  I40E_CUSTOMIZED_GTPC),
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV6_GTPC,
				  I40E_HASH_IPV6_L234_RSS_MASK,
				  I40E_CUSTOMIZED_GTPC),

	/* GTPU */
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV4_GTPU,
				  I40E_HASH_IPV4_L234_RSS_MASK,
				  I40E_CUSTOMIZED_GTPU),
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV4_GTPU_IPV4,
				  RTE_ETH_RSS_GTPU, I40E_CUSTOMIZED_GTPU_IPV4),
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV4_GTPU_IPV6,
				  RTE_ETH_RSS_GTPU, I40E_CUSTOMIZED_GTPU_IPV6),
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV6_GTPU,
				  I40E_HASH_IPV6_L234_RSS_MASK,
				  I40E_CUSTOMIZED_GTPU),
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV6_GTPU_IPV4,
				  RTE_ETH_RSS_GTPU, I40E_CUSTOMIZED_GTPU_IPV4),
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV6_GTPU_IPV6,
				  RTE_ETH_RSS_GTPU, I40E_CUSTOMIZED_GTPU_IPV6),

	/* L2TPV3 */
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV4_L2TPV3,
				  RTE_ETH_RSS_L2TPV3, I40E_CUSTOMIZED_IPV4_L2TPV3),
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV6_L2TPV3,
				  RTE_ETH_RSS_L2TPV3, I40E_CUSTOMIZED_IPV6_L2TPV3),

	/* AH */
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV4_AH, RTE_ETH_RSS_AH,
				  I40E_CUSTOMIZED_AH_IPV4),
	I40E_HASH_MAP_CUS_PATTERN(I40E_PHINT_IPV6_AH, RTE_ETH_RSS_AH,
				  I40E_CUSTOMIZED_AH_IPV6),
};

static int
i40e_hash_get_pattern_type(const struct rte_flow_item pattern[],
			   uint64_t *pattern_types,
			   struct rte_flow_error *error)
{
	const char *message = "Pattern not supported";
	enum rte_flow_item_type prev_item_type = RTE_FLOW_ITEM_TYPE_VOID;
	enum rte_flow_item_type last_item_type = prev_item_type;
	uint64_t item_hdr, pattern_hdrs = 0;
	bool inner_flag = false;
	int vlan_count = 0;

	for (; pattern->type != RTE_FLOW_ITEM_TYPE_END; pattern++) {
		if (pattern->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;

		if (pattern->mask || pattern->spec || pattern->last) {
			message = "Header info should not be specified";
			goto not_sup;
		}

		/* Check the previous item allows this sub-item. */
		if (prev_item_type >= (enum rte_flow_item_type)
				RTE_DIM(pattern_next_allow_items) ||
		    !(pattern_next_allow_items[prev_item_type] &
				BIT_ULL(pattern->type)))
			goto not_sup;

		/* For VLAN item, it does no matter about to pattern type
		 * recognition. So just count the number of VLAN and do not
		 * change the value of variable `prev_item_type`.
		 */
		last_item_type = pattern->type;
		if (last_item_type == RTE_FLOW_ITEM_TYPE_VLAN) {
			if (vlan_count >= 2)
				goto not_sup;
			vlan_count++;
			continue;
		}

		prev_item_type = last_item_type;
		if (last_item_type >= (enum rte_flow_item_type)
				RTE_DIM(pattern_item_header))
			goto not_sup;

		item_hdr = pattern_item_header[last_item_type];
		assert(item_hdr);

		if (inner_flag) {
			item_hdr <<= I40E_HASH_HDR_INNER_SHIFT;

			/* Inner layer should not have GTPU item */
			if (last_item_type == RTE_FLOW_ITEM_TYPE_GTPU)
				goto not_sup;
		} else {
			if (last_item_type == RTE_FLOW_ITEM_TYPE_GTPU) {
				inner_flag = true;
				vlan_count = 0;
			}
		}

		if (item_hdr & pattern_hdrs)
			goto not_sup;

		pattern_hdrs |= item_hdr;
	}

	if (pattern_hdrs && last_item_type != RTE_FLOW_ITEM_TYPE_VLAN) {
		*pattern_types = pattern_hdrs;
		return 0;
	}

not_sup:
	return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				  pattern, message);
}

static uint64_t
i40e_hash_get_x722_ext_pctypes(uint8_t match_pctype)
{
	uint64_t pctypes = 0;

	switch (match_pctype) {
	case I40E_FILTER_PCTYPE_NONF_IPV4_TCP:
		pctypes = BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK);
		break;

	case I40E_FILTER_PCTYPE_NONF_IPV4_UDP:
		pctypes = BIT_ULL(I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP) |
			  BIT_ULL(I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP);
		break;

	case I40E_FILTER_PCTYPE_NONF_IPV6_TCP:
		pctypes = BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK);
		break;

	case I40E_FILTER_PCTYPE_NONF_IPV6_UDP:
		pctypes = BIT_ULL(I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP) |
			  BIT_ULL(I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP);
		break;
	}

	return pctypes;
}

static int
i40e_hash_translate_gtp_inset(struct i40e_rte_flow_rss_conf *rss_conf,
			      struct rte_flow_error *error)
{
	if (rss_conf->inset &
	    (I40E_INSET_IPV4_SRC | I40E_INSET_IPV6_SRC |
	    I40E_INSET_DST_PORT | I40E_INSET_SRC_PORT))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL,
					  "Only support external destination IP");

	if (rss_conf->inset & I40E_INSET_IPV4_DST)
		rss_conf->inset = (rss_conf->inset & ~I40E_INSET_IPV4_DST) |
				  I40E_INSET_TUNNEL_IPV4_DST;

	if (rss_conf->inset & I40E_INSET_IPV6_DST)
		rss_conf->inset = (rss_conf->inset & ~I40E_INSET_IPV6_DST) |
				  I40E_INSET_TUNNEL_IPV6_DST;

	return 0;
}

static int
i40e_hash_get_pctypes(const struct rte_eth_dev *dev,
		      const struct i40e_hash_match_pattern *match,
		      struct i40e_rte_flow_rss_conf *rss_conf,
		      struct rte_flow_error *error)
{
	if (match->custom_pctype_flag) {
		struct i40e_pf *pf;
		struct i40e_customized_pctype *custom_type;

		pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
		custom_type = i40e_find_customized_pctype(pf, match->pctype);
		if (!custom_type || !custom_type->valid)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "PCTYPE not supported");

		rss_conf->config_pctypes |= BIT_ULL(custom_type->pctype);

		if (match->pctype == I40E_CUSTOMIZED_GTPU ||
		    match->pctype == I40E_CUSTOMIZED_GTPC)
			return i40e_hash_translate_gtp_inset(rss_conf, error);
	} else {
		struct i40e_hw *hw =
				I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
		uint64_t types;

		rss_conf->config_pctypes |= BIT_ULL(match->pctype);
		if (hw->mac.type == I40E_MAC_X722) {
			types = i40e_hash_get_x722_ext_pctypes(match->pctype);
			rss_conf->config_pctypes |= types;
		}
	}

	return 0;
}

static int
i40e_hash_get_pattern_pctypes(const struct rte_eth_dev *dev,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action_rss *rss_act,
			      struct i40e_rte_flow_rss_conf *rss_conf,
			      struct rte_flow_error *error)
{
	uint64_t pattern_types = 0;
	bool match_flag = false;
	int i, ret;

	ret = i40e_hash_get_pattern_type(pattern, &pattern_types, error);
	if (ret)
		return ret;

	for (i = 0; i < (int)RTE_DIM(match_patterns); i++) {
		const struct i40e_hash_match_pattern *match =
							&match_patterns[i];

		/* Check pattern types match. All items that have the same
		 * pattern types are together, so if the pattern types match
		 * previous item but they doesn't match current item, it means
		 * the pattern types do not match all remain items.
		 */
		if (pattern_types != match->pattern_type) {
			if (match_flag)
				break;
			continue;
		}
		match_flag = true;

		/* Check RSS types match */
		if (!(rss_act->types & ~match->rss_mask)) {
			ret = i40e_hash_get_pctypes(dev, match,
						    rss_conf, error);
			if (ret)
				return ret;
		}
	}

	if (rss_conf->config_pctypes)
		return 0;

	if (match_flag)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "RSS types not supported");

	return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				  NULL, "Pattern not supported");
}

static uint64_t
i40e_hash_get_inset(uint64_t rss_types)
{
	uint64_t mask, inset = 0;
	int i;

	for (i = 0; i < (int)RTE_DIM(i40e_hash_rss_inset); i++) {
		if (rss_types & i40e_hash_rss_inset[i].rss_type)
			inset |= i40e_hash_rss_inset[i].inset;
	}

	if (!inset)
		return 0;

	/* If SRC_ONLY and DST_ONLY of the same level are used simultaneously,
	 * it is the same case as none of them are added.
	 */
	mask = rss_types & (RTE_ETH_RSS_L2_SRC_ONLY | RTE_ETH_RSS_L2_DST_ONLY);
	if (mask == RTE_ETH_RSS_L2_SRC_ONLY)
		inset &= ~I40E_INSET_DMAC;
	else if (mask == RTE_ETH_RSS_L2_DST_ONLY)
		inset &= ~I40E_INSET_SMAC;

	mask = rss_types & (RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY);
	if (mask == RTE_ETH_RSS_L3_SRC_ONLY)
		inset &= ~(I40E_INSET_IPV4_DST | I40E_INSET_IPV6_DST);
	else if (mask == RTE_ETH_RSS_L3_DST_ONLY)
		inset &= ~(I40E_INSET_IPV4_SRC | I40E_INSET_IPV6_SRC);

	mask = rss_types & (RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY);
	if (mask == RTE_ETH_RSS_L4_SRC_ONLY)
		inset &= ~I40E_INSET_DST_PORT;
	else if (mask == RTE_ETH_RSS_L4_DST_ONLY)
		inset &= ~I40E_INSET_SRC_PORT;

	if (rss_types & I40E_HASH_L4_TYPES) {
		uint64_t l3_mask = rss_types &
				   (RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY);
		uint64_t l4_mask = rss_types &
				   (RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY);

		if (l3_mask && !l4_mask)
			inset &= ~(I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT);
		else if (!l3_mask && l4_mask)
			inset &= ~(I40E_INSET_IPV4_DST | I40E_INSET_IPV6_DST |
				 I40E_INSET_IPV4_SRC | I40E_INSET_IPV6_SRC);
	}

	return inset;
}

static int
i40e_hash_config_func(struct i40e_hw *hw, enum rte_eth_hash_function func)
{
	struct i40e_pf *pf;
	uint32_t reg;
	uint8_t symmetric = 0;

	reg = i40e_read_rx_ctl(hw, I40E_GLQF_CTL);

	if (func == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR) {
		if (!(reg & I40E_GLQF_CTL_HTOEP_MASK))
			goto set_symmetric;

		reg &= ~I40E_GLQF_CTL_HTOEP_MASK;
	} else {
		if (func == RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ)
			symmetric = 1;

		if (reg & I40E_GLQF_CTL_HTOEP_MASK)
			goto set_symmetric;

		reg |= I40E_GLQF_CTL_HTOEP_MASK;
	}

	pf = &((struct i40e_adapter *)hw->back)->pf;
	if (pf->support_multi_driver) {
		PMD_DRV_LOG(ERR,
			    "Modify hash function is not permitted when multi-driver enabled");
		return -EPERM;
	}

	PMD_DRV_LOG(INFO, "NIC hash function is setting to %d", func);
	i40e_write_rx_ctl(hw, I40E_GLQF_CTL, reg);
	I40E_WRITE_FLUSH(hw);

set_symmetric:
	i40e_set_symmetric_hash_enable_per_port(hw, symmetric);
	return 0;
}

static int
i40e_hash_config_pctype_symmetric(struct i40e_hw *hw,
				  uint32_t pctype,
				  bool symmetric)
{
	struct i40e_pf *pf = &((struct i40e_adapter *)hw->back)->pf;
	uint32_t reg;

	reg = i40e_read_rx_ctl(hw, I40E_GLQF_HSYM(pctype));
	if (symmetric) {
		if (reg & I40E_GLQF_HSYM_SYMH_ENA_MASK)
			return 0;
		reg |= I40E_GLQF_HSYM_SYMH_ENA_MASK;
	} else {
		if (!(reg & I40E_GLQF_HSYM_SYMH_ENA_MASK))
			return 0;
		reg &= ~I40E_GLQF_HSYM_SYMH_ENA_MASK;
	}

	if (pf->support_multi_driver) {
		PMD_DRV_LOG(ERR,
			    "Enable/Disable symmetric hash is not permitted when multi-driver enabled");
		return -EPERM;
	}

	i40e_write_rx_ctl(hw, I40E_GLQF_HSYM(pctype), reg);
	I40E_WRITE_FLUSH(hw);
	return 0;
}

static void
i40e_hash_enable_pctype(struct i40e_hw *hw,
			uint32_t pctype, bool enable)
{
	uint32_t reg, reg_val, mask;

	if (pctype < 32) {
		mask = BIT(pctype);
		reg = I40E_PFQF_HENA(0);
	} else {
		mask = BIT(pctype - 32);
		reg = I40E_PFQF_HENA(1);
	}

	reg_val = i40e_read_rx_ctl(hw, reg);

	if (enable) {
		if (reg_val & mask)
			return;

		reg_val |= mask;
	} else {
		if (!(reg_val & mask))
			return;

		reg_val &= ~mask;
	}

	i40e_write_rx_ctl(hw, reg, reg_val);
	I40E_WRITE_FLUSH(hw);
}

static int
i40e_hash_config_pctype(struct i40e_hw *hw,
			struct i40e_rte_flow_rss_conf *rss_conf,
			uint32_t pctype)
{
	uint64_t rss_types = rss_conf->conf.types;
	int ret;

	if (rss_types == 0) {
		i40e_hash_enable_pctype(hw, pctype, false);
		return 0;
	}

	if (rss_conf->inset) {
		ret = i40e_set_hash_inset(hw, rss_conf->inset, pctype, false);
		if (ret)
			return ret;
	}

	i40e_hash_enable_pctype(hw, pctype, true);
	return 0;
}

static int
i40e_hash_config_region(struct i40e_pf *pf,
			const struct i40e_rte_flow_rss_conf *rss_conf)
{
	struct i40e_hw *hw = &pf->adapter->hw;
	struct rte_eth_dev *dev = &rte_eth_devices[pf->dev_data->port_id];
	struct i40e_queue_region_info *regions = pf->queue_region.region;
	uint32_t num = pf->queue_region.queue_region_number;
	uint32_t i, region_id_mask = 0;

	/* Use a 32 bit variable to represent all regions */
	RTE_BUILD_BUG_ON(I40E_REGION_MAX_INDEX > 31);

	/* Re-configure the region if it existed */
	for (i = 0; i < num; i++) {
		if (rss_conf->region_queue_start ==
		    regions[i].queue_start_index &&
		    rss_conf->region_queue_num == regions[i].queue_num) {
			uint32_t j;

			for (j = 0; j < regions[i].user_priority_num; j++) {
				if (regions[i].user_priority[j] ==
				    rss_conf->region_priority)
					return 0;
			}

			if (j >= I40E_MAX_USER_PRIORITY) {
				PMD_DRV_LOG(ERR,
					    "Priority number exceed the maximum %d",
					    I40E_MAX_USER_PRIORITY);
				return -ENOSPC;
			}

			regions[i].user_priority[j] = rss_conf->region_priority;
			regions[i].user_priority_num++;
			return i40e_flush_queue_region_all_conf(dev, hw, pf, 1);
		}

		region_id_mask |= BIT(regions[i].region_id);
	}

	if (num > I40E_REGION_MAX_INDEX) {
		PMD_DRV_LOG(ERR, "Queue region resource used up");
		return -ENOSPC;
	}

	/* Add a new region */

	pf->queue_region.queue_region_number++;
	memset(&regions[num], 0, sizeof(regions[0]));

	regions[num].region_id = rte_bsf32(~region_id_mask);
	regions[num].queue_num = rss_conf->region_queue_num;
	regions[num].queue_start_index = rss_conf->region_queue_start;
	regions[num].user_priority[0] = rss_conf->region_priority;
	regions[num].user_priority_num = 1;

	return i40e_flush_queue_region_all_conf(dev, hw, pf, 1);
}

static int
i40e_hash_config(struct i40e_pf *pf,
		 struct i40e_rte_flow_rss_conf *rss_conf)
{
	struct rte_flow_action_rss *rss_info = &rss_conf->conf;
	struct i40e_hw *hw = &pf->adapter->hw;
	uint64_t pctypes;
	int ret;

	if (rss_info->func != RTE_ETH_HASH_FUNCTION_DEFAULT) {
		ret = i40e_hash_config_func(hw, rss_info->func);
		if (ret)
			return ret;

		if (rss_info->func != RTE_ETH_HASH_FUNCTION_TOEPLITZ)
			rss_conf->misc_reset_flags |=
					I40E_HASH_FLOW_RESET_FLAG_FUNC;
	}

	if (rss_conf->region_queue_num > 0) {
		ret = i40e_hash_config_region(pf, rss_conf);
		if (ret)
			return ret;

		rss_conf->misc_reset_flags |= I40E_HASH_FLOW_RESET_FLAG_REGION;
	}

	if (rss_info->key_len > 0) {
		ret = i40e_set_rss_key(pf->main_vsi, rss_conf->key,
				       rss_info->key_len);
		if (ret)
			return ret;

		rss_conf->misc_reset_flags |= I40E_HASH_FLOW_RESET_FLAG_KEY;
	}

	/* Update lookup table */
	if (rss_info->queue_num > 0) {
		uint8_t lut[RTE_ETH_RSS_RETA_SIZE_512];
		uint32_t i, j = 0;

		for (i = 0; i < hw->func_caps.rss_table_size; i++) {
			lut[i] = (uint8_t)rss_info->queue[j];
			j = (j == rss_info->queue_num - 1) ? 0 : (j + 1);
		}

		ret = i40e_set_rss_lut(pf->main_vsi, lut, (uint16_t)i);
		if (ret)
			return ret;

		pf->hash_enabled_queues = 0;
		for (i = 0; i < rss_info->queue_num; i++)
			pf->hash_enabled_queues |= BIT_ULL(lut[i]);

		pf->adapter->rss_reta_updated = 0;
		rss_conf->misc_reset_flags |= I40E_HASH_FLOW_RESET_FLAG_QUEUE;
	}

	/* The codes behind configure the input sets and symmetric hash
	 * function of the packet types and enable hash on them.
	 */
	pctypes = rss_conf->config_pctypes;
	if (!pctypes)
		return 0;

	/* For first flow that will enable hash on any packet type, we clean
	 * the RSS sets that by legacy configuration commands and parameters.
	 */
	if (!pf->hash_filter_enabled) {
		i40e_pf_disable_rss(pf);
		pf->hash_filter_enabled = true;
	}

	do {
		uint32_t idx = rte_bsf64(pctypes);
		uint64_t bit = BIT_ULL(idx);

		if (rss_conf->symmetric_enable) {
			ret = i40e_hash_config_pctype_symmetric(hw, idx, true);
			if (ret)
				return ret;

			rss_conf->reset_symmetric_pctypes |= bit;
		}

		ret = i40e_hash_config_pctype(hw, rss_conf, idx);
		if (ret)
			return ret;

		rss_conf->reset_config_pctypes |= bit;
		pctypes &= ~bit;
	} while (pctypes);

	return 0;
}

static void
i40e_hash_parse_key(const struct rte_flow_action_rss *rss_act,
		    struct i40e_rte_flow_rss_conf *rss_conf)
{
	const uint8_t *key = rss_act->key;

	if (!key || rss_act->key_len != sizeof(rss_conf->key)) {
		const uint32_t rss_key_default[] = {0x6b793944,
			0x23504cb5, 0x5bea75b6, 0x309f4f12, 0x3dc0a2b8,
			0x024ddcdf, 0x339b8ca0, 0x4c4af64a, 0x34fac605,
			0x55d85839, 0x3a58997d, 0x2ec938e1, 0x66031581};

		if (rss_act->key_len != sizeof(rss_conf->key))
			PMD_DRV_LOG(WARNING,
				    "RSS key length invalid, must be %u bytes, now set key to default",
				    (uint32_t)sizeof(rss_conf->key));

		memcpy(rss_conf->key, rss_key_default, sizeof(rss_conf->key));
	} else {
		memcpy(rss_conf->key, key, sizeof(rss_conf->key));
	}

	rss_conf->conf.key = rss_conf->key;
	rss_conf->conf.key_len = sizeof(rss_conf->key);
}

static int
i40e_hash_parse_queues(const struct rte_eth_dev *dev,
		       const struct rte_flow_action_rss *rss_act,
		       struct i40e_rte_flow_rss_conf *rss_conf,
		       struct rte_flow_error *error)
{
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	uint16_t i;
	int max_queue;

	hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if (!rss_act->queue_num ||
	    rss_act->queue_num > hw->func_caps.rss_table_size)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "Invalid RSS queue number");

	if (rss_act->key_len)
		PMD_DRV_LOG(WARNING,
			    "RSS key is ignored when queues specified");

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	if (pf->dev_data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_VMDQ_FLAG)
		max_queue = i40e_pf_calc_configured_queues_num(pf);
	else
		max_queue = pf->dev_data->nb_rx_queues;

	max_queue = RTE_MIN(max_queue, I40E_MAX_Q_PER_TC);

	for (i = 0; i < rss_act->queue_num; i++) {
		if ((int)rss_act->queue[i] >= max_queue)
			break;
	}

	if (i < rss_act->queue_num)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "Invalid RSS queues");

	memcpy(rss_conf->queue, rss_act->queue,
	       rss_act->queue_num * sizeof(rss_conf->queue[0]));
	rss_conf->conf.queue = rss_conf->queue;
	rss_conf->conf.queue_num = rss_act->queue_num;
	return 0;
}

static int
i40e_hash_parse_queue_region(const struct rte_eth_dev *dev,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action_rss *rss_act,
			     struct i40e_rte_flow_rss_conf *rss_conf,
			     struct rte_flow_error *error)
{
	struct i40e_pf *pf;
	const struct rte_flow_item_vlan *vlan_spec, *vlan_mask;
	uint64_t hash_queues;
	uint32_t i;

	if (pattern[1].type != RTE_FLOW_ITEM_TYPE_END)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_NUM,
					  &pattern[1],
					  "Pattern not supported.");

	vlan_spec = pattern->spec;
	vlan_mask = pattern->mask;
	if (!vlan_spec || !vlan_mask ||
	    (rte_be_to_cpu_16(vlan_mask->hdr.vlan_tci) >> 13) != 7)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, pattern,
					  "Pattern error.");

	if (!rss_act->queue)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "Queues not specified");

	if (rss_act->key_len)
		PMD_DRV_LOG(WARNING,
			    "RSS key is ignored when configure queue region");

	/* Use a 64 bit variable to represent all queues in a region. */
	RTE_BUILD_BUG_ON(I40E_MAX_Q_PER_TC > 64);

	if (!rss_act->queue_num ||
	    !rte_is_power_of_2(rss_act->queue_num) ||
	    rss_act->queue_num + rss_act->queue[0] > I40E_MAX_Q_PER_TC)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "Queue number error");

	for (i = 1; i < rss_act->queue_num; i++) {
		if (rss_act->queue[i - 1] + 1 != rss_act->queue[i])
			break;
	}

	if (i < rss_act->queue_num)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL,
					  "Queues must be incremented continuously");

	/* Map all queues to bits of uint64_t */
	hash_queues = (BIT_ULL(rss_act->queue[0] + rss_act->queue_num) - 1) &
		      ~(BIT_ULL(rss_act->queue[0]) - 1);

	pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	if (hash_queues & ~pf->hash_enabled_queues)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "Some queues are not in LUT");

	rss_conf->region_queue_num = (uint8_t)rss_act->queue_num;
	rss_conf->region_queue_start = rss_act->queue[0];
	rss_conf->region_priority = rte_be_to_cpu_16(vlan_spec->hdr.vlan_tci) >> 13;
	return 0;
}

static int
i40e_hash_parse_global_conf(const struct rte_eth_dev *dev,
			    const struct rte_flow_item pattern[],
			    const struct rte_flow_action_rss *rss_act,
			    struct i40e_rte_flow_rss_conf *rss_conf,
			    struct rte_flow_error *error)
{
	if (rss_act->func == RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL,
					  "Symmetric function should be set with pattern types");

	rss_conf->conf.func = rss_act->func;

	if (rss_act->types)
		PMD_DRV_LOG(WARNING,
			    "RSS types are ignored when no pattern specified");

	if (pattern[0].type == RTE_FLOW_ITEM_TYPE_VLAN)
		return i40e_hash_parse_queue_region(dev, pattern, rss_act,
						    rss_conf, error);

	if (rss_act->queue)
		return i40e_hash_parse_queues(dev, rss_act, rss_conf, error);

	if (rss_act->key_len) {
		i40e_hash_parse_key(rss_act, rss_conf);
		return 0;
	}

	if (rss_act->func == RTE_ETH_HASH_FUNCTION_DEFAULT)
		PMD_DRV_LOG(WARNING, "Nothing change");
	return 0;
}

static bool
i40e_hash_validate_rss_types(uint64_t rss_types)
{
	uint64_t type, mask;

	/* Validate L2 */
	type = RTE_ETH_RSS_ETH & rss_types;
	mask = (RTE_ETH_RSS_L2_SRC_ONLY | RTE_ETH_RSS_L2_DST_ONLY) & rss_types;
	if (!type && mask)
		return false;

	/* Validate L3 */
	type = (I40E_HASH_L4_TYPES | RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
	       RTE_ETH_RSS_NONFRAG_IPV4_OTHER | RTE_ETH_RSS_IPV6 |
	       RTE_ETH_RSS_FRAG_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_OTHER) & rss_types;
	mask = (RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY) & rss_types;
	if (!type && mask)
		return false;

	/* Validate L4 */
	type = (I40E_HASH_L4_TYPES | RTE_ETH_RSS_PORT) & rss_types;
	mask = (RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY) & rss_types;
	if (!type && mask)
		return false;

	return true;
}

static int
i40e_hash_parse_pattern_act(const struct rte_eth_dev *dev,
			    const struct rte_flow_item pattern[],
			    const struct rte_flow_action_rss *rss_act,
			    struct i40e_rte_flow_rss_conf *rss_conf,
			    struct rte_flow_error *error)
{
	if (rss_act->queue)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL,
					  "RSS Queues not supported when pattern specified");

	switch (rss_act->func) {
	case RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ:
		rss_conf->symmetric_enable = true;
		break;
	case RTE_ETH_HASH_FUNCTION_DEFAULT:
	case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
	case RTE_ETH_HASH_FUNCTION_SIMPLE_XOR:
		break;
	default:
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				NULL,
				"RSS hash function not supported "
				"when pattern specified");
	}

	if (!i40e_hash_validate_rss_types(rss_act->types))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "RSS types are invalid");

	if (rss_act->key_len)
		i40e_hash_parse_key(rss_act, rss_conf);

	rss_conf->conf.func = rss_act->func;
	rss_conf->conf.types = rss_act->types;
	rss_conf->inset = i40e_hash_get_inset(rss_act->types);

	return i40e_hash_get_pattern_pctypes(dev, pattern, rss_act,
					     rss_conf, error);
}

int
i40e_hash_parse(const struct rte_eth_dev *dev,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct i40e_rte_flow_rss_conf *rss_conf,
		struct rte_flow_error *error)
{
	const struct rte_flow_action_rss *rss_act;

	if (actions[1].type != RTE_FLOW_ACTION_TYPE_END)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  &actions[1],
					  "Only support one action for RSS.");

	rss_act = (const struct rte_flow_action_rss *)actions[0].conf;
	if (rss_act->level)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  actions,
					  "RSS level is not supported");

	while (pattern->type == RTE_FLOW_ITEM_TYPE_VOID)
		pattern++;

	if (pattern[0].type == RTE_FLOW_ITEM_TYPE_END ||
	    pattern[0].type == RTE_FLOW_ITEM_TYPE_VLAN)
		return i40e_hash_parse_global_conf(dev, pattern, rss_act,
						   rss_conf, error);

	return i40e_hash_parse_pattern_act(dev, pattern, rss_act,
					   rss_conf, error);
}

static void
i40e_invalid_rss_filter(const struct i40e_rte_flow_rss_conf *ref_conf,
			struct i40e_rte_flow_rss_conf *conf)
{
	uint32_t reset_flags = conf->misc_reset_flags;

	conf->misc_reset_flags &= ~ref_conf->misc_reset_flags;

	if ((reset_flags & I40E_HASH_FLOW_RESET_FLAG_REGION) &&
	    (ref_conf->misc_reset_flags & I40E_HASH_FLOW_RESET_FLAG_REGION) &&
	    (conf->region_queue_start != ref_conf->region_queue_start ||
	     conf->region_queue_num != ref_conf->region_queue_num))
		conf->misc_reset_flags |= I40E_HASH_FLOW_RESET_FLAG_REGION;

	conf->reset_config_pctypes &= ~ref_conf->reset_config_pctypes;
	conf->reset_symmetric_pctypes &= ~ref_conf->reset_symmetric_pctypes;
}

int
i40e_hash_filter_restore(struct i40e_pf *pf)
{
	struct i40e_rss_filter *filter;
	int ret;

	TAILQ_FOREACH(filter, &pf->rss_config_list, next) {
		struct i40e_rte_flow_rss_conf *rss_conf =
						&filter->rss_filter_info;
		struct i40e_rss_filter *prev;

		rss_conf->misc_reset_flags = 0;
		rss_conf->reset_config_pctypes = 0;
		rss_conf->reset_symmetric_pctypes = 0;

		ret = i40e_hash_config(pf, rss_conf);
		if (ret) {
			pf->hash_filter_enabled = 0;
			i40e_pf_disable_rss(pf);
			PMD_DRV_LOG(ERR,
				    "Re-configure RSS failed, RSS has been disabled");
			return ret;
		}

		/* Invalid previous RSS filter */
		TAILQ_FOREACH(prev, &pf->rss_config_list, next) {
			if (prev == filter)
				break;
			i40e_invalid_rss_filter(rss_conf,
						&prev->rss_filter_info);
		}
	}

	return 0;
}

int
i40e_hash_filter_create(struct i40e_pf *pf,
			struct i40e_rte_flow_rss_conf *rss_conf)
{
	struct i40e_rss_filter *filter, *prev;
	struct i40e_rte_flow_rss_conf *new_conf;
	int ret;

	filter = rte_zmalloc("i40e_rss_filter", sizeof(*filter), 0);
	if (!filter) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory.");
		return -ENOMEM;
	}

	new_conf = &filter->rss_filter_info;

	memcpy(new_conf, rss_conf, sizeof(*new_conf));
	if (new_conf->conf.queue_num)
		new_conf->conf.queue = new_conf->queue;
	if (new_conf->conf.key_len)
		new_conf->conf.key = new_conf->key;

	ret = i40e_hash_config(pf, new_conf);
	if (ret) {
		rte_free(filter);
		if (i40e_pf_config_rss(pf))
			return ret;

		(void)i40e_hash_filter_restore(pf);
		return ret;
	}

	/* Invalid previous RSS filter */
	TAILQ_FOREACH(prev, &pf->rss_config_list, next)
		i40e_invalid_rss_filter(new_conf, &prev->rss_filter_info);

	TAILQ_INSERT_TAIL(&pf->rss_config_list, filter, next);
	return 0;
}

static int
i40e_hash_reset_conf(struct i40e_pf *pf,
		     struct i40e_rte_flow_rss_conf *rss_conf)
{
	struct i40e_hw *hw = &pf->adapter->hw;
	struct rte_eth_dev *dev;
	uint64_t inset;
	uint32_t idx;
	int ret;

	if (rss_conf->misc_reset_flags & I40E_HASH_FLOW_RESET_FLAG_FUNC) {
		ret = i40e_hash_config_func(hw, RTE_ETH_HASH_FUNCTION_TOEPLITZ);
		if (ret)
			return ret;

		rss_conf->misc_reset_flags &= ~I40E_HASH_FLOW_RESET_FLAG_FUNC;
	}

	if (rss_conf->misc_reset_flags & I40E_HASH_FLOW_RESET_FLAG_REGION) {
		dev = &rte_eth_devices[pf->dev_data->port_id];
		ret = i40e_flush_queue_region_all_conf(dev, hw, pf, 0);
		if (ret)
			return ret;

		rss_conf->misc_reset_flags &= ~I40E_HASH_FLOW_RESET_FLAG_REGION;
	}

	if (rss_conf->misc_reset_flags & I40E_HASH_FLOW_RESET_FLAG_KEY) {
		ret = i40e_pf_reset_rss_key(pf);
		if (ret)
			return ret;

		rss_conf->misc_reset_flags &= ~I40E_HASH_FLOW_RESET_FLAG_KEY;
	}

	if (rss_conf->misc_reset_flags & I40E_HASH_FLOW_RESET_FLAG_QUEUE) {
		if (!pf->adapter->rss_reta_updated) {
			ret = i40e_pf_reset_rss_reta(pf);
			if (ret)
				return ret;
		}

		pf->hash_enabled_queues = 0;
		rss_conf->misc_reset_flags &= ~I40E_HASH_FLOW_RESET_FLAG_QUEUE;
	}

	while (rss_conf->reset_config_pctypes) {
		idx = rte_bsf64(rss_conf->reset_config_pctypes);

		i40e_hash_enable_pctype(hw, idx, false);
		inset = i40e_get_default_input_set(idx);
		if (inset) {
			ret = i40e_set_hash_inset(hw, inset, idx, false);
			if (ret)
				return ret;
		}

		rss_conf->reset_config_pctypes &= ~BIT_ULL(idx);
	}

	while (rss_conf->reset_symmetric_pctypes) {
		idx = rte_bsf64(rss_conf->reset_symmetric_pctypes);

		ret = i40e_hash_config_pctype_symmetric(hw, idx, false);
		if (ret)
			return ret;

		rss_conf->reset_symmetric_pctypes &= ~BIT_ULL(idx);
	}

	return 0;
}

int
i40e_hash_filter_destroy(struct i40e_pf *pf,
			 const struct i40e_rss_filter *rss_filter)
{
	struct i40e_rss_filter *filter;
	int ret;

	TAILQ_FOREACH(filter, &pf->rss_config_list, next) {
		if (rss_filter == filter) {
			ret = i40e_hash_reset_conf(pf,
						   &filter->rss_filter_info);
			if (ret)
				return ret;

			TAILQ_REMOVE(&pf->rss_config_list, filter, next);
			rte_free(filter);
			return 0;
		}
	}

	return -ENOENT;
}

int
i40e_hash_filter_flush(struct i40e_pf *pf)
{
	struct rte_flow *flow, *next;

	RTE_TAILQ_FOREACH_SAFE(flow, &pf->flow_list, node, next) {
		if (flow->filter_type != RTE_ETH_FILTER_HASH)
			continue;

		if (flow->rule) {
			struct i40e_rss_filter *filter = flow->rule;
			int ret;

			ret = i40e_hash_reset_conf(pf,
						   &filter->rss_filter_info);
			if (ret)
				return ret;

			TAILQ_REMOVE(&pf->rss_config_list, filter, next);
			rte_free(filter);
		}

		TAILQ_REMOVE(&pf->flow_list, flow, node);
		rte_free(flow);
	}

	assert(!pf->rss_config_list.tqh_first);
	return 0;
}
