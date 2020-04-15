/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#ifndef _IGC_ETHDEV_H_
#define _IGC_ETHDEV_H_

#include <rte_ethdev.h>

#include "base/igc_osdep.h"
#include "base/igc_hw.h"
#include "base/igc_i225.h"
#include "base/igc_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IGC_RSS_RDT_SIZD		128

/* VLAN filter table size */
#define IGC_VFTA_SIZE			128

#define IGC_QUEUE_PAIRS_NUM		4

#define IGC_HKEY_MAX_INDEX		10
#define IGC_RSS_RDT_SIZD		128

#define IGC_DEFAULT_REG_SIZE		4
#define IGC_DEFAULT_REG_SIZE_MASK	0xf

#define IGC_RSS_RDT_REG_SIZE		IGC_DEFAULT_REG_SIZE
#define IGC_RSS_RDT_REG_SIZE_MASK	IGC_DEFAULT_REG_SIZE_MASK
#define IGC_HKEY_REG_SIZE		IGC_DEFAULT_REG_SIZE
#define IGC_HKEY_SIZE			(IGC_HKEY_REG_SIZE * IGC_HKEY_MAX_INDEX)

/*
 * TDBA/RDBA should be aligned on 16 byte boundary. But TDLEN/RDLEN should be
 * multiple of 128 bytes. So we align TDBA/RDBA on 128 byte boundary.
 * This will also optimize cache line size effect.
 * H/W supports up to cache line size 128.
 */
#define IGC_ALIGN			128

#define IGC_TX_DESCRIPTOR_MULTIPLE	8
#define IGC_RX_DESCRIPTOR_MULTIPLE	8

#define IGC_RXD_ALIGN	((uint16_t)(IGC_ALIGN / \
		sizeof(union igc_adv_rx_desc)))
#define IGC_TXD_ALIGN	((uint16_t)(IGC_ALIGN / \
		sizeof(union igc_adv_tx_desc)))
#define IGC_MIN_TXD	IGC_TX_DESCRIPTOR_MULTIPLE
#define IGC_MAX_TXD	((uint16_t)(0x80000 / sizeof(union igc_adv_tx_desc)))
#define IGC_MIN_RXD	IGC_RX_DESCRIPTOR_MULTIPLE
#define IGC_MAX_RXD	((uint16_t)(0x80000 / sizeof(union igc_adv_rx_desc)))

#define IGC_TX_MAX_SEG		UINT8_MAX
#define IGC_TX_MAX_MTU_SEG	UINT8_MAX

#define IGC_RX_OFFLOAD_ALL	(    \
	DEV_RX_OFFLOAD_VLAN_STRIP  | \
	DEV_RX_OFFLOAD_VLAN_FILTER | \
	DEV_RX_OFFLOAD_VLAN_EXTEND | \
	DEV_RX_OFFLOAD_IPV4_CKSUM  | \
	DEV_RX_OFFLOAD_UDP_CKSUM   | \
	DEV_RX_OFFLOAD_TCP_CKSUM   | \
	DEV_RX_OFFLOAD_SCTP_CKSUM  | \
	DEV_RX_OFFLOAD_JUMBO_FRAME | \
	DEV_RX_OFFLOAD_KEEP_CRC    | \
	DEV_RX_OFFLOAD_SCATTER)

#define IGC_TX_OFFLOAD_ALL	(    \
	DEV_TX_OFFLOAD_VLAN_INSERT | \
	DEV_TX_OFFLOAD_IPV4_CKSUM  | \
	DEV_TX_OFFLOAD_UDP_CKSUM   | \
	DEV_TX_OFFLOAD_TCP_CKSUM   | \
	DEV_TX_OFFLOAD_SCTP_CKSUM  | \
	DEV_TX_OFFLOAD_TCP_TSO     | \
	DEV_TX_OFFLOAD_UDP_TSO	   | \
	DEV_TX_OFFLOAD_MULTI_SEGS)

#define IGC_RSS_OFFLOAD_ALL	(    \
	ETH_RSS_IPV4               | \
	ETH_RSS_NONFRAG_IPV4_TCP   | \
	ETH_RSS_NONFRAG_IPV4_UDP   | \
	ETH_RSS_IPV6               | \
	ETH_RSS_NONFRAG_IPV6_TCP   | \
	ETH_RSS_NONFRAG_IPV6_UDP   | \
	ETH_RSS_IPV6_EX            | \
	ETH_RSS_IPV6_TCP_EX        | \
	ETH_RSS_IPV6_UDP_EX)

#define IGC_MAX_ETQF_FILTERS		3	/* etqf(3) is used for 1588 */
#define IGC_ETQF_FILTER_1588		3
#define IGC_ETQF_QUEUE_SHIFT		16
#define IGC_ETQF_QUEUE_MASK		(7u << IGC_ETQF_QUEUE_SHIFT)

#define IGC_MAX_NTUPLE_FILTERS		8
#define IGC_NTUPLE_MAX_PRI		7

#define IGC_SYN_FILTER_ENABLE		0x01	/* syn filter enable field */
#define IGC_SYN_FILTER_QUEUE_SHIFT	1	/* syn filter queue field */
#define IGC_SYN_FILTER_QUEUE	0x0000000E	/* syn filter queue field */
#define IGC_RFCTL_SYNQFP	0x00080000	/* SYNQFP in RFCTL register */

/* structure for interrupt relative data */
struct igc_interrupt {
	uint32_t flags;
	uint32_t mask;
};

/* Union of RSS redirect table register */
union igc_rss_reta_reg {
	uint32_t dword;
	uint8_t  bytes[4];
};

/* Structure to per-queue statics */
struct igc_hw_queue_stats {
	u64	pqgprc[IGC_QUEUE_PAIRS_NUM];
	/* per queue good packets received count */
	u64	pqgptc[IGC_QUEUE_PAIRS_NUM];
	/* per queue good packets transmitted count */
	u64	pqgorc[IGC_QUEUE_PAIRS_NUM];
	/* per queue good octets received count */
	u64	pqgotc[IGC_QUEUE_PAIRS_NUM];
	/* per queue good octets transmitted count */
	u64	pqmprc[IGC_QUEUE_PAIRS_NUM];
	/* per queue multicast packets received count */
	u64	rqdpc[IGC_QUEUE_PAIRS_NUM];
	/* per receive queue drop packet count */
	u64	tqdpc[IGC_QUEUE_PAIRS_NUM];
	/* per transmit queue drop packet count */
};

/* local vfta copy */
struct igc_vfta {
	uint32_t vfta[IGC_VFTA_SIZE];
};

/* ethertype filter structure */
struct igc_ethertype_filter {
	uint16_t ether_type;
	uint16_t queue;
};

/* Structure of ntuple filter info. */
struct igc_ntuple_info {
	uint16_t dst_port;
	uint8_t proto;		/* l4 protocol. */

	/*
	 * the packet matched above 2tuple and contain any set bit will hit
	 * this filter.
	 */
	uint8_t tcp_flags;

	/*
	 * seven levels (001b-111b), 111b is highest, used when more than one
	 * filter matches.
	 */
	uint8_t priority;
	uint8_t dst_port_mask:1, /* if mask is 1b, do compare dst port. */
		proto_mask:1;    /* if mask is 1b, do compare protocol. */
};

/* Structure of n-tuple filter */
struct igc_ntuple_filter {
	RTE_STD_C11
	union {
		uint64_t hash_val;
		struct igc_ntuple_info tuple_info;
	};

	uint8_t queue;
};

/* Structure of TCP SYN filter */
struct igc_syn_filter {
	uint8_t queue;

	uint8_t hig_pri:1,	/* 1 - higher priority than other filters, */
				/* 0 - lower priority. */
		enable:1;	/* 1-enable; 0-disable */
};

/* Structure to store RTE flow RSS configure. */
struct igc_rss_filter {
	struct rte_flow_action_rss conf; /* RSS parameters. */
	uint8_t key[IGC_HKEY_MAX_INDEX * sizeof(uint32_t)]; /* Hash key. */
	uint16_t queue[IGC_RSS_RDT_SIZD];/* Queues indices to use. */
	uint8_t enable;	/* 1-enabled, 0-disabled */
};

/* Feature filter types */
enum igc_filter_type {
	IGC_FILTER_TYPE_ETHERTYPE,
	IGC_FILTER_TYPE_NTUPLE,
	IGC_FILTER_TYPE_SYN,
	IGC_FILTER_TYPE_HASH
};

/* Structure to store flow */
struct rte_flow {
	TAILQ_ENTRY(rte_flow) node;
	enum igc_filter_type filter_type;
	RTE_STD_C11
	char filter[0];		/* filter data */
};

/* Flow list header */
TAILQ_HEAD(igc_flow_list, rte_flow);

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct igc_adapter {
	struct igc_hw		hw;
	struct igc_hw_stats	stats;
	struct igc_hw_queue_stats queue_stats;
	int16_t txq_stats_map[IGC_QUEUE_PAIRS_NUM];
	int16_t rxq_stats_map[IGC_QUEUE_PAIRS_NUM];

	struct igc_interrupt	intr;
	struct igc_vfta	shadow_vfta;
	bool		stopped;

	struct igc_ethertype_filter ethertype_filters[IGC_MAX_ETQF_FILTERS];
	struct igc_ntuple_filter ntuple_filters[IGC_MAX_NTUPLE_FILTERS];
	struct igc_syn_filter syn_filter;
	struct igc_rss_filter rss_filter;
	struct igc_flow_list flow_list;
};

#define IGC_DEV_PRIVATE(_dev)	((_dev)->data->dev_private)

#define IGC_DEV_PRIVATE_HW(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->hw)

#define IGC_DEV_PRIVATE_STATS(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->stats)

#define IGC_DEV_PRIVATE_QUEUE_STATS(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->queue_stats)

#define IGC_DEV_PRIVATE_INTR(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->intr)

#define IGC_DEV_PRIVATE_VFTA(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->shadow_vfta)

#define IGC_DEV_PRIVATE_RSS_FILTER(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->rss_filter)

#define IGC_DEV_PRIVATE_FLOW_LIST(_dev) \
	(&((struct igc_adapter *)(_dev)->data->dev_private)->flow_list)

static inline void
igc_read_reg_check_set_bits(struct igc_hw *hw, uint32_t reg, uint32_t bits)
{
	uint32_t reg_val = IGC_READ_REG(hw, reg);

	bits |= reg_val;
	if (bits == reg_val)
		return;	/* no need to write back */

	IGC_WRITE_REG(hw, reg, bits);
}

static inline void
igc_read_reg_check_clear_bits(struct igc_hw *hw, uint32_t reg, uint32_t bits)
{
	uint32_t reg_val = IGC_READ_REG(hw, reg);

	bits = reg_val & ~bits;
	if (bits == reg_val)
		return;	/* no need to write back */

	IGC_WRITE_REG(hw, reg, bits);
}

#ifdef __cplusplus
}
#endif

#endif /* _IGC_ETHDEV_H_ */
