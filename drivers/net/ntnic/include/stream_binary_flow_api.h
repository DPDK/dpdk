/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _STREAM_BINARY_FLOW_API_H_
#define _STREAM_BINARY_FLOW_API_H_

#include <rte_ether.h>
#include "rte_flow.h"
#include "rte_flow_driver.h"

/* Max RSS hash key length in bytes */
#define MAX_RSS_KEY_LEN 40

/* NT specific MASKs for RSS configuration */
/* NOTE: Masks are required for correct RSS configuration, do not modify them! */
#define NT_ETH_RSS_IPV4_MASK                                                                      \
	(RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_OTHER |              \
	 RTE_ETH_RSS_NONFRAG_IPV4_SCTP | RTE_ETH_RSS_NONFRAG_IPV4_TCP |                           \
	 RTE_ETH_RSS_NONFRAG_IPV4_UDP)

#define NT_ETH_RSS_IPV6_MASK                                                                      \
	(RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 | RTE_ETH_RSS_IPV6_EX |                         \
	 RTE_ETH_RSS_IPV6_TCP_EX | RTE_ETH_RSS_IPV6_UDP_EX | RTE_ETH_RSS_NONFRAG_IPV6_OTHER |     \
	 RTE_ETH_RSS_NONFRAG_IPV6_SCTP | RTE_ETH_RSS_NONFRAG_IPV6_TCP |                           \
	 RTE_ETH_RSS_NONFRAG_IPV6_UDP)

#define NT_ETH_RSS_IP_MASK                                                                        \
	(NT_ETH_RSS_IPV4_MASK | NT_ETH_RSS_IPV6_MASK | RTE_ETH_RSS_L3_SRC_ONLY |                  \
	 RTE_ETH_RSS_L3_DST_ONLY)

/* List of all RSS flags supported for RSS calculation offload */
#define NT_ETH_RSS_OFFLOAD_MASK                                                                   \
	(RTE_ETH_RSS_ETH | RTE_ETH_RSS_L2_PAYLOAD | RTE_ETH_RSS_IP | RTE_ETH_RSS_TCP |            \
	 RTE_ETH_RSS_UDP | RTE_ETH_RSS_SCTP | RTE_ETH_RSS_L2_SRC_ONLY | RTE_ETH_RSS_L2_DST_ONLY | \
	 RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY | RTE_ETH_RSS_L3_SRC_ONLY |            \
	 RTE_ETH_RSS_L3_DST_ONLY | RTE_ETH_RSS_VLAN | RTE_ETH_RSS_LEVEL_MASK |                    \
	 RTE_ETH_RSS_IPV4_CHKSUM | RTE_ETH_RSS_L4_CHKSUM | RTE_ETH_RSS_PORT | RTE_ETH_RSS_GTPU)

/*
 * Flow frontend for binary programming interface
 */

#define FLOW_MAX_QUEUES 128

#define RAW_ENCAP_DECAP_ELEMS_MAX 16

extern uint64_t rte_tsc_freq;
extern rte_spinlock_t hwlock;

/*
 * Flow eth dev profile determines how the FPGA module resources are
 * managed and what features are available
 */
enum flow_eth_dev_profile {
	FLOW_ETH_DEV_PROFILE_INLINE = 0,
};

struct flow_queue_id_s {
	int id;
	int hw_id;
};

/*
 * RTE_FLOW_ACTION_TYPE_RAW_ENCAP
 */
struct flow_action_raw_encap {
	uint8_t *data;
	uint8_t *preserve;
	size_t size;
	struct rte_flow_item items[RAW_ENCAP_DECAP_ELEMS_MAX];
	int item_count;
};

/*
 * RTE_FLOW_ACTION_TYPE_RAW_DECAP
 */
struct flow_action_raw_decap {
	uint8_t *data;
	size_t size;
	struct rte_flow_item items[RAW_ENCAP_DECAP_ELEMS_MAX];
	int item_count;
};

struct flow_eth_dev;             /* port device */
struct flow_handle;

#endif  /* _STREAM_BINARY_FLOW_API_H_ */
