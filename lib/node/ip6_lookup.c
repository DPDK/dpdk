/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <arpa/inet.h>
#include <sys/socket.h>

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_ip.h>
#include <rte_lpm6.h>

#include "rte_node_ip6_api.h"

#include "node_private.h"

#define IPV6_L3FWD_LPM_MAX_RULES 1024
#define IPV6_L3FWD_LPM_NUMBER_TBL8S (1 << 8)

/* IP6 Lookup global data struct */
struct ip6_lookup_node_main {
	struct rte_lpm6 *lpm_tbl[RTE_MAX_NUMA_NODES];
};

struct ip6_lookup_node_ctx {
	/* Socket's LPM table */
	struct rte_lpm6 *lpm6;
	/* Dynamic offset to mbuf priv1 */
	int mbuf_priv1_off;
};

static struct ip6_lookup_node_main ip6_lookup_nm;

#define IP6_LOOKUP_NODE_LPM(ctx) \
	(((struct ip6_lookup_node_ctx *)ctx)->lpm6)

#define IP6_LOOKUP_NODE_PRIV1_OFF(ctx) \
	(((struct ip6_lookup_node_ctx *)ctx)->mbuf_priv1_off)

static uint16_t
ip6_lookup_node_process_scalar(struct rte_graph *graph, struct rte_node *node,
			void **objs, uint16_t nb_objs)
{
	struct rte_mbuf *mbuf0, *mbuf1, *mbuf2, *mbuf3, **pkts;
	struct rte_lpm6 *lpm6 = IP6_LOOKUP_NODE_LPM(node->ctx);
	const int dyn = IP6_LOOKUP_NODE_PRIV1_OFF(node->ctx);
	struct rte_ipv6_hdr *ipv6_hdr;
	void **to_next, **from;
	uint16_t last_spec = 0;
	rte_edge_t next_index;
	uint16_t n_left_from;
	uint16_t held = 0;
	uint32_t drop_nh;
	int i, rc;

	/* Speculative next */
	next_index = RTE_NODE_IP6_LOOKUP_NEXT_REWRITE;
	/* Drop node */
	drop_nh = ((uint32_t)RTE_NODE_IP6_LOOKUP_NEXT_PKT_DROP) << 16;

	pkts = (struct rte_mbuf **)objs;
	from = objs;
	n_left_from = nb_objs;

	for (i = OBJS_PER_CLINE; i < RTE_GRAPH_BURST_SIZE; i += OBJS_PER_CLINE)
		rte_prefetch0(&objs[i]);

	for (i = 0; i < 4 && i < n_left_from; i++)
		rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[i], void *,
						sizeof(struct rte_ether_hdr)));

	/* Get stream for the speculated next node */
	to_next = rte_node_next_stream_get(graph, node, next_index, nb_objs);
	while (n_left_from >= 4) {
		uint8_t ip_batch[4][16];
		int32_t next_hop[4];
		uint16_t next[4];

#if RTE_GRAPH_BURST_SIZE > 64
		/* Prefetch next-next mbufs */
		if (likely(n_left_from > 11)) {
			rte_prefetch0(pkts[8]);
			rte_prefetch0(pkts[9]);
			rte_prefetch0(pkts[10]);
			rte_prefetch0(pkts[11]);
		}
#endif
		/* Prefetch next mbuf data */
		if (likely(n_left_from > 7)) {
			rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[4], void *,
						sizeof(struct rte_ether_hdr)));
			rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[5], void *,
						sizeof(struct rte_ether_hdr)));
			rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[6], void *,
						sizeof(struct rte_ether_hdr)));
			rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[7], void *,
						sizeof(struct rte_ether_hdr)));
		}

		mbuf0 = pkts[0];
		mbuf1 = pkts[1];
		mbuf2 = pkts[2];
		mbuf3 = pkts[3];

		pkts += 4;
		n_left_from -= 4;

		/* Extract DIP of mbuf0 */
		ipv6_hdr = rte_pktmbuf_mtod_offset(mbuf0, struct rte_ipv6_hdr *,
				sizeof(struct rte_ether_hdr));
		/* Extract hop_limits as ipv6 hdr is in cache */
		node_mbuf_priv1(mbuf0, dyn)->ttl = ipv6_hdr->hop_limits;
		rte_memcpy(ip_batch[0], ipv6_hdr->dst_addr, 16);

		/* Extract DIP of mbuf1 */
		ipv6_hdr = rte_pktmbuf_mtod_offset(mbuf1, struct rte_ipv6_hdr *,
				sizeof(struct rte_ether_hdr));
		/* Extract hop_limits as ipv6 hdr is in cache */
		node_mbuf_priv1(mbuf1, dyn)->ttl = ipv6_hdr->hop_limits;
		rte_memcpy(ip_batch[1], ipv6_hdr->dst_addr, 16);

		/* Extract DIP of mbuf2 */
		ipv6_hdr = rte_pktmbuf_mtod_offset(mbuf2, struct rte_ipv6_hdr *,
				sizeof(struct rte_ether_hdr));
		/* Extract hop_limits as ipv6 hdr is in cache */
		node_mbuf_priv1(mbuf2, dyn)->ttl = ipv6_hdr->hop_limits;
		rte_memcpy(ip_batch[2], ipv6_hdr->dst_addr, 16);

		/* Extract DIP of mbuf3 */
		ipv6_hdr = rte_pktmbuf_mtod_offset(mbuf3, struct rte_ipv6_hdr *,
				sizeof(struct rte_ether_hdr));
		/* Extract hop_limits as ipv6 hdr is in cache */
		node_mbuf_priv1(mbuf3, dyn)->ttl = ipv6_hdr->hop_limits;
		rte_memcpy(ip_batch[3], ipv6_hdr->dst_addr, 16);

		rte_lpm6_lookup_bulk_func(lpm6, ip_batch, next_hop, 4);

		next_hop[0] = (next_hop[0] < 0) ? (int32_t)drop_nh : next_hop[0];
		node_mbuf_priv1(mbuf0, dyn)->nh = (uint16_t)next_hop[0];
		next[0] = (uint16_t)(next_hop[0] >> 16);

		next_hop[1] = (next_hop[1] < 0) ? (int32_t)drop_nh : next_hop[1];
		node_mbuf_priv1(mbuf1, dyn)->nh = (uint16_t)next_hop[1];
		next[1] = (uint16_t)(next_hop[1] >> 16);

		next_hop[2] = (next_hop[2] < 0) ? (int32_t)drop_nh : next_hop[2];
		node_mbuf_priv1(mbuf2, dyn)->nh = (uint16_t)next_hop[2];
		next[2] = (uint16_t)(next_hop[2] >> 16);

		next_hop[3] = (next_hop[3] < 0) ? (int32_t)drop_nh : next_hop[3];
		node_mbuf_priv1(mbuf3, dyn)->nh = (uint16_t)next_hop[3];
		next[3] = (uint16_t)(next_hop[3] >> 16);

		rte_edge_t fix_spec = ((next_index == next[0]) &&
					(next_index == next[1]) &&
					(next_index == next[2]) &&
					(next_index == next[3]));

		if (unlikely(fix_spec == 0)) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			/* Next0 */
			if (next_index == next[0]) {
				to_next[0] = from[0];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next[0], from[0]);
			}

			/* Next1 */
			if (next_index == next[1]) {
				to_next[0] = from[1];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next[1], from[1]);
			}

			/* Next2 */
			if (next_index == next[2]) {
				to_next[0] = from[2];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next[2], from[2]);
			}

			/* Next3 */
			if (next_index == next[3]) {
				to_next[0] = from[3];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next[3], from[3]);
			}

			from += 4;
		} else {
			last_spec += 4;
		}
	}

	while (n_left_from > 0) {
		uint32_t next_hop;
		uint16_t next0;

		mbuf0 = pkts[0];

		pkts += 1;
		n_left_from -= 1;

		/* Extract DIP of mbuf0 */
		ipv6_hdr = rte_pktmbuf_mtod_offset(mbuf0, struct rte_ipv6_hdr *,
						sizeof(struct rte_ether_hdr));
		/* Extract TTL as IPv6 hdr is in cache */
		node_mbuf_priv1(mbuf0, dyn)->ttl = ipv6_hdr->hop_limits;

		rc = rte_lpm6_lookup(lpm6, ipv6_hdr->dst_addr, &next_hop);
		next_hop = (rc == 0) ? next_hop : drop_nh;

		node_mbuf_priv1(mbuf0, dyn)->nh = (uint16_t)next_hop;
		next_hop = next_hop >> 16;
		next0 = (uint16_t)next_hop;

		if (unlikely(next_index ^ next0)) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			rte_node_enqueue_x1(graph, node, next0, from[0]);
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

int
rte_node_ip6_route_add(const uint8_t *ip, uint8_t depth, uint16_t next_hop,
		       enum rte_node_ip6_lookup_next next_node)
{
	char abuf[INET6_ADDRSTRLEN];
	struct in6_addr in6;
	uint8_t socket;
	uint32_t val;
	int ret;

	memcpy(in6.s6_addr, ip, RTE_LPM6_IPV6_ADDR_SIZE);
	inet_ntop(AF_INET6, &in6, abuf, sizeof(abuf));
	/* Embedded next node id into 24 bit next hop */
	val = ((next_node << 16) | next_hop) & ((1ull << 24) - 1);
	node_dbg("ip6_lookup", "LPM: Adding route %s / %d nh (0x%x)", abuf,
		 depth, val);

	for (socket = 0; socket < RTE_MAX_NUMA_NODES; socket++) {
		if (!ip6_lookup_nm.lpm_tbl[socket])
			continue;

		ret = rte_lpm6_add(ip6_lookup_nm.lpm_tbl[socket], ip, depth,
				   val);
		if (ret < 0) {
			node_err("ip6_lookup",
				 "Unable to add entry %s / %d nh (%x) to LPM "
				 "table on sock %d, rc=%d\n",
				 abuf, depth, val, socket, ret);
			return ret;
		}
	}

	return 0;
}

static int
setup_lpm6(struct ip6_lookup_node_main *nm, int socket)
{
	struct rte_lpm6_config config_ipv6;
	char s[RTE_LPM6_NAMESIZE];

	/* One LPM table per socket */
	if (nm->lpm_tbl[socket])
		return 0;

	/* create the LPM table */
	config_ipv6.max_rules = IPV6_L3FWD_LPM_MAX_RULES;
	config_ipv6.number_tbl8s = IPV6_L3FWD_LPM_NUMBER_TBL8S;
	config_ipv6.flags = 0;
	snprintf(s, sizeof(s), "IPV6_L3FWD_LPM_%d", socket);
	nm->lpm_tbl[socket] = rte_lpm6_create(s, socket, &config_ipv6);
	if (nm->lpm_tbl[socket] == NULL)
		return -rte_errno;

	return 0;
}

static int
ip6_lookup_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	uint16_t socket, lcore_id;
	static uint8_t init_once;
	int rc;

	RTE_SET_USED(graph);
	RTE_BUILD_BUG_ON(sizeof(struct ip6_lookup_node_ctx) > RTE_NODE_CTX_SZ);

	if (!init_once) {
		node_mbuf_priv1_dynfield_offset =
			rte_mbuf_dynfield_register(
				&node_mbuf_priv1_dynfield_desc);
		if (node_mbuf_priv1_dynfield_offset < 0)
			return -rte_errno;

		/* Setup LPM tables for all sockets */
		RTE_LCORE_FOREACH(lcore_id)
		{
			socket = rte_lcore_to_socket_id(lcore_id);
			rc = setup_lpm6(&ip6_lookup_nm, socket);
			if (rc) {
				node_err("ip6_lookup",
					 "Failed to setup lpm6 tbl for "
					 "sock %u, rc=%d", socket, rc);
				return rc;
			}
		}
		init_once = 1;
	}

	/* Update socket's LPM and mbuf dyn priv1 offset in node ctx */
	IP6_LOOKUP_NODE_LPM(node->ctx) = ip6_lookup_nm.lpm_tbl[graph->socket];
	IP6_LOOKUP_NODE_PRIV1_OFF(node->ctx) =
					node_mbuf_priv1_dynfield_offset;

	node_dbg("ip6_lookup", "Initialized ip6_lookup node");

	return 0;
}

static struct rte_node_register ip6_lookup_node = {
	.process = ip6_lookup_node_process_scalar,
	.name = "ip6_lookup",

	.init = ip6_lookup_node_init,

	.nb_edges = RTE_NODE_IP6_LOOKUP_NEXT_MAX,
	.next_nodes = {
		[RTE_NODE_IP6_LOOKUP_NEXT_REWRITE] = "ip6_rewrite",
		[RTE_NODE_IP6_LOOKUP_NEXT_PKT_DROP] = "pkt_drop",
	},
};

RTE_NODE_REGISTER(ip6_lookup_node);
