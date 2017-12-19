/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <rte_hash_crc.h>
#include <rte_hash.h>

#define MAX_NODES             16
/*
 * Shared port info, including statistics information for display by server.
 * Structure will be put in a memzone.
 * - All port id values share one cache line as this data will be read-only
 * during operation.
 * - All rx statistic values share cache lines, as this data is written only
 * by the server process. (rare reads by stats display)
 * - The tx statistics have values for all ports per cache line, but the stats
 * themselves are written by the nodes, so we have a distinct set, on different
 * cache lines for each node to use.
 */
struct rx_stats {
	uint64_t rx[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

struct tx_stats {
	uint64_t tx[RTE_MAX_ETHPORTS];
	uint64_t tx_drop[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

struct filter_stats {
	uint64_t drop;
	uint64_t passed;
} __rte_cache_aligned;

struct shared_info {
	uint8_t num_nodes;
	uint16_t num_ports;
	uint32_t num_flows;
	uint16_t id[RTE_MAX_ETHPORTS];
	struct rx_stats rx_stats;
	struct tx_stats tx_stats[MAX_NODES];
	struct filter_stats filter_stats[MAX_NODES];
};

/* define common names for structures shared between server and node */
#define MP_NODE_RXQ_NAME "MProc_Node_%u_RX"
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define MZ_SHARED_INFO "MProc_shared_info"

/*
 * Given the rx queue name template above, get the queue name
 */
static inline const char *
get_rx_queue_name(unsigned int id)
{
	/*
	 * Buffer for return value. Size calculated by %u being replaced
	 * by maximum 3 digits (plus an extra byte for safety)
	 */
	static char buffer[sizeof(MP_NODE_RXQ_NAME) + 2];

	snprintf(buffer, sizeof(buffer) - 1, MP_NODE_RXQ_NAME, id);
	return buffer;
}

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

#endif
