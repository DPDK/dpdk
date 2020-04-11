/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <arpa/inet.h>
#include <sys/socket.h>

#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_ip.h>
#include <rte_lpm.h>
#include <rte_mbuf.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include "rte_node_ip4_api.h"

#include "node_private.h"

#define IPV4_L3FWD_LPM_MAX_RULES 1024
#define IPV4_L3FWD_LPM_NUMBER_TBL8S (1 << 8)

/* IP4 Lookup global data struct */
struct ip4_lookup_node_main {
	struct rte_lpm *lpm_tbl[RTE_MAX_NUMA_NODES];
};

#if defined(RTE_MACHINE_CPUFLAG_NEON)
#include "ip4_lookup_neon.h"
#elif defined(RTE_ARCH_X86)
#include "ip4_lookup_sse.h"
#else

static uint16_t
ip4_lookup_node_process(struct rte_graph *graph, struct rte_node *node,
			void **objs, uint16_t nb_objs)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	void **to_next, **from;
	uint16_t last_spec = 0;
	struct rte_mbuf *mbuf;
	rte_edge_t next_index;
	struct rte_lpm *lpm;
	uint16_t held = 0;
	uint32_t drop_nh;
	int i, rc;

	/* Speculative next */
	next_index = RTE_NODE_IP4_LOOKUP_NEXT_REWRITE;
	/* Drop node */
	drop_nh = ((uint32_t)RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP) << 16;

	/* Get socket specific LPM from ctx */
	lpm = *((struct rte_lpm **)node->ctx);
	from = objs;

	/* Get stream for the speculated next node */
	to_next = rte_node_next_stream_get(graph, node, next_index, nb_objs);
	for (i = 0; i < nb_objs; i++) {
		uint32_t next_hop;
		uint16_t next;

		mbuf = (struct rte_mbuf *)objs[i];

		/* Extract DIP of mbuf0 */
		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr *,
				sizeof(struct rte_ether_hdr));
		/* Extract cksum, ttl as ipv4 hdr is in cache */
		node_mbuf_priv1(mbuf)->cksum = ipv4_hdr->hdr_checksum;
		node_mbuf_priv1(mbuf)->ttl = ipv4_hdr->time_to_live;

		rc = rte_lpm_lookup(lpm, rte_be_to_cpu_32(ipv4_hdr->dst_addr),
				    &next_hop);
		next_hop = (rc == 0) ? next_hop : drop_nh;

		node_mbuf_priv1(mbuf)->nh = (uint16_t)next_hop;
		next_hop = next_hop >> 16;
		next = (uint16_t)next_hop;

		if (unlikely(next_index != next)) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			rte_node_enqueue_x1(graph, node, next, from[0]);
			from += 1;
		} else {
			last_spec += 1;
		}
	}

	/* !!! Home run !!! */
	if (likely(last_spec == nb_objs)) {
		rte_node_next_stream_move(graph, node, next_index);
		return nb_objs;
	}
	held += last_spec;
	rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
	rte_node_next_stream_put(graph, node, next_index, held);

	return nb_objs;
}

#endif

static int
ip4_lookup_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	RTE_SET_USED(graph);
	RTE_SET_USED(node);

	node_dbg("ip4_lookup", "Initialized ip4_lookup node");

	return 0;
}

static struct rte_node_register ip4_lookup_node = {
	.process = ip4_lookup_node_process,
	.name = "ip4_lookup",

	.init = ip4_lookup_node_init,

	.nb_edges = RTE_NODE_IP4_LOOKUP_NEXT_MAX,
	.next_nodes = {
		[RTE_NODE_IP4_LOOKUP_NEXT_REWRITE] = "ip4_rewrite",
		[RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP] = "pkt_drop",
	},
};

RTE_NODE_REGISTER(ip4_lookup_node);
