/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <arpa/inet.h>
#include <sys/socket.h>

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_ip.h>
#include <rte_lpm.h>

#include "rte_node_ip4_api.h"

#include "node_private.h"

#define IPV4_L3FWD_LPM_MAX_RULES 1024
#define IPV4_L3FWD_LPM_NUMBER_TBL8S (1 << 8)

/* IP4 Lookup global data struct */
struct ip4_lookup_node_main {
	struct rte_lpm *lpm_tbl[RTE_MAX_NUMA_NODES];
};

struct ip4_lookup_node_ctx {
	/* Socket's LPM table */
	struct rte_lpm *lpm;
	/* Dynamic offset to mbuf priv1 */
	int mbuf_priv1_off;
};

int node_mbuf_priv1_dynfield_offset = -1;

static struct ip4_lookup_node_main ip4_lookup_nm;

#define IP4_LOOKUP_NODE_LPM(ctx) \
	(((struct ip4_lookup_node_ctx *)ctx)->lpm)

#define IP4_LOOKUP_NODE_PRIV1_OFF(ctx) \
	(((struct ip4_lookup_node_ctx *)ctx)->mbuf_priv1_off)

#if defined(__ARM_NEON)
#include "ip4_lookup_neon.h"
#elif defined(RTE_ARCH_X86)
#include "ip4_lookup_sse.h"
#endif

static uint16_t
ip4_lookup_node_process_scalar(struct rte_graph *graph, struct rte_node *node,
			void **objs, uint16_t nb_objs)
{
	struct rte_lpm *lpm = IP4_LOOKUP_NODE_LPM(node->ctx);
	const int dyn = IP4_LOOKUP_NODE_PRIV1_OFF(node->ctx);
	struct rte_ipv4_hdr *ipv4_hdr;
	void **to_next, **from;
	uint16_t last_spec = 0;
	struct rte_mbuf *mbuf;
	rte_edge_t next_index;
	uint16_t held = 0;
	uint32_t drop_nh;
	int i, rc;

	/* Speculative next */
	next_index = RTE_NODE_IP4_LOOKUP_NEXT_REWRITE;
	/* Drop node */
	drop_nh = ((uint32_t)RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP) << 16;
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
		node_mbuf_priv1(mbuf, dyn)->cksum = ipv4_hdr->hdr_checksum;
		node_mbuf_priv1(mbuf, dyn)->ttl = ipv4_hdr->time_to_live;

		rc = rte_lpm_lookup(lpm, rte_be_to_cpu_32(ipv4_hdr->dst_addr),
				    &next_hop);
		next_hop = (rc == 0) ? next_hop : drop_nh;

		node_mbuf_priv1(mbuf, dyn)->nh = (uint16_t)next_hop;
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

int
rte_node_ip4_route_add(uint32_t ip, uint8_t depth, uint16_t next_hop,
		       enum rte_node_ip4_lookup_next next_node)
{
	char abuf[INET6_ADDRSTRLEN];
	struct in_addr in;
	uint8_t socket;
	uint32_t val;
	int ret;

	in.s_addr = htonl(ip);
	inet_ntop(AF_INET, &in, abuf, sizeof(abuf));
	/* Embedded next node id into 24 bit next hop */
	val = ((next_node << 16) | next_hop) & ((1ull << 24) - 1);
	node_dbg("ip4_lookup", "LPM: Adding route %s / %d nh (0x%x)", abuf,
		 depth, val);

	for (socket = 0; socket < RTE_MAX_NUMA_NODES; socket++) {
		if (!ip4_lookup_nm.lpm_tbl[socket])
			continue;

		ret = rte_lpm_add(ip4_lookup_nm.lpm_tbl[socket],
				  ip, depth, val);
		if (ret < 0) {
			node_err("ip4_lookup",
				 "Unable to add entry %s / %d nh (%x) to LPM table on sock %d, rc=%d\n",
				 abuf, depth, val, socket, ret);
			return ret;
		}
	}

	return 0;
}

static int
setup_lpm(struct ip4_lookup_node_main *nm, int socket)
{
	struct rte_lpm_config config_ipv4;
	char s[RTE_LPM_NAMESIZE];

	/* One LPM table per socket */
	if (nm->lpm_tbl[socket])
		return 0;

	/* create the LPM table */
	config_ipv4.max_rules = IPV4_L3FWD_LPM_MAX_RULES;
	config_ipv4.number_tbl8s = IPV4_L3FWD_LPM_NUMBER_TBL8S;
	config_ipv4.flags = 0;
	snprintf(s, sizeof(s), "IPV4_L3FWD_LPM_%d", socket);
	nm->lpm_tbl[socket] = rte_lpm_create(s, socket, &config_ipv4);
	if (nm->lpm_tbl[socket] == NULL)
		return -rte_errno;

	return 0;
}

static int
ip4_lookup_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	uint16_t socket, lcore_id;
	static uint8_t init_once;
	int rc;

	RTE_SET_USED(graph);
	RTE_BUILD_BUG_ON(sizeof(struct ip4_lookup_node_ctx) > RTE_NODE_CTX_SZ);

	if (!init_once) {
		node_mbuf_priv1_dynfield_offset = rte_mbuf_dynfield_register(
				&node_mbuf_priv1_dynfield_desc);
		if (node_mbuf_priv1_dynfield_offset < 0)
			return -rte_errno;

		/* Setup LPM tables for all sockets */
		RTE_LCORE_FOREACH(lcore_id)
		{
			socket = rte_lcore_to_socket_id(lcore_id);
			rc = setup_lpm(&ip4_lookup_nm, socket);
			if (rc) {
				node_err("ip4_lookup",
					 "Failed to setup lpm tbl for sock %u, rc=%d",
					 socket, rc);
				return rc;
			}
		}
		init_once = 1;
	}

	/* Update socket's LPM and mbuf dyn priv1 offset in node ctx */
	IP4_LOOKUP_NODE_LPM(node->ctx) = ip4_lookup_nm.lpm_tbl[graph->socket];
	IP4_LOOKUP_NODE_PRIV1_OFF(node->ctx) = node_mbuf_priv1_dynfield_offset;

#if defined(__ARM_NEON) || defined(RTE_ARCH_X86)
	if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128)
		node->process = ip4_lookup_node_process_vec;
#endif

	node_dbg("ip4_lookup", "Initialized ip4_lookup node");

	return 0;
}

static struct rte_node_register ip4_lookup_node = {
	.process = ip4_lookup_node_process_scalar,
	.name = "ip4_lookup",

	.init = ip4_lookup_node_init,

	.nb_edges = RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP + 1,
	.next_nodes = {
		[RTE_NODE_IP4_LOOKUP_NEXT_IP4_LOCAL] = "ip4_local",
		[RTE_NODE_IP4_LOOKUP_NEXT_REWRITE] = "ip4_rewrite",
		[RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP] = "pkt_drop",
	},
};

RTE_NODE_REGISTER(ip4_lookup_node);
