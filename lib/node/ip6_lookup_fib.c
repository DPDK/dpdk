/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#include <arpa/inet.h>

#include <eal_export.h>
#include <rte_errno.h>
#include <rte_ether.h>
#include <rte_fib6.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_ip.h>

#include "rte_node_ip6_api.h"

#include "node_private.h"

/* IP6 Lookup FIB global data struct */
struct ip6_lookup_fib_node_main {
	struct rte_fib6 *fib6[RTE_MAX_NUMA_NODES];
};

struct ip6_lookup_fib_node_ctx {
	/* Socket's FIB6 */
	struct rte_fib6 *fib6;
	/* Dynamic offset to mbuf priv1 */
	int mbuf_priv1_off;
};

static struct ip6_lookup_fib_node_main ip6_lookup_fib_nm;

#define FIB6_DEFAULT_MAX_ROUTES (UINT16_MAX)
#define FIB6_DEFAULT_NUM_TBL8   (UINT16_MAX / 2)
#define FIB6_DEFAULT_NH (RTE_NODE_IP6_LOOKUP_NEXT_PKT_DROP << 16)

#define IP6_LOOKUP_FIB_NODE(ctx) \
	(((struct ip6_lookup_fib_node_ctx *)ctx)->fib6)

#define IP6_LOOKUP_FIB_NODE_PRIV1_OFF(ctx) \
	(((struct ip6_lookup_fib_node_ctx *)ctx)->mbuf_priv1_off)

static uint16_t
ip6_lookup_fib_node_process(struct rte_graph *graph, struct rte_node *node, void **objs,
			    uint16_t nb_objs)
{
	struct rte_mbuf *mbuf0, *mbuf1, *mbuf2, *mbuf3, **pkts;
	const int dyn = IP6_LOOKUP_FIB_NODE_PRIV1_OFF(node->ctx);
	struct rte_fib6 *fib = IP6_LOOKUP_FIB_NODE(node->ctx);
	struct rte_ipv6_addr ip[RTE_GRAPH_BURST_SIZE] = { 0 };
	uint64_t next_hop[RTE_GRAPH_BURST_SIZE];
	struct rte_ipv6_hdr *ipv6_hdr;
	uint16_t lookup_err = 0;
	void **to_next, **from;
	uint16_t last_spec = 0;
	rte_edge_t next_index;
	uint16_t n_left_from;
	uint16_t held = 0;
	uint16_t next;
	int i;

	/* Speculative next */
	next_index = RTE_NODE_IP6_LOOKUP_NEXT_REWRITE;

	pkts = (struct rte_mbuf **)objs;
	from = objs;
	n_left_from = nb_objs;

	/* Get stream for the speculated next node */
	to_next = rte_node_next_stream_get(graph, node, next_index, nb_objs);

	for (i = 0; i < 4 && i < n_left_from; i++)
		rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[i], void *,
					sizeof(struct rte_ether_hdr)));

	i = 0;
	while (n_left_from >= 4) {
		if (likely(n_left_from > 7)) {

			/* Prefetch next-next mbufs */
			if (likely(n_left_from > 11)) {
				rte_prefetch0(pkts[8]);
				rte_prefetch0(pkts[9]);
				rte_prefetch0(pkts[10]);
				rte_prefetch0(pkts[11]);
			}

			/* Prefetch next mbuf data */
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

		ip[i++] = ipv6_hdr->dst_addr;

		/* Extract DIP of mbuf1 */
		ipv6_hdr = rte_pktmbuf_mtod_offset(mbuf1, struct rte_ipv6_hdr *,
				sizeof(struct rte_ether_hdr));
		/* Extract hop_limits as ipv6 hdr is in cache */
		node_mbuf_priv1(mbuf1, dyn)->ttl = ipv6_hdr->hop_limits;

		ip[i++] = ipv6_hdr->dst_addr;

		/* Extract DIP of mbuf2 */
		ipv6_hdr = rte_pktmbuf_mtod_offset(mbuf2, struct rte_ipv6_hdr *,
				sizeof(struct rte_ether_hdr));
		/* Extract hop_limits as ipv6 hdr is in cache */
		node_mbuf_priv1(mbuf2, dyn)->ttl = ipv6_hdr->hop_limits;

		ip[i++] = ipv6_hdr->dst_addr;

		/* Extract DIP of mbuf3 */
		ipv6_hdr = rte_pktmbuf_mtod_offset(mbuf3, struct rte_ipv6_hdr *,
				sizeof(struct rte_ether_hdr));
		/* Extract hop_limits as ipv6 hdr is in cache */
		node_mbuf_priv1(mbuf3, dyn)->ttl = ipv6_hdr->hop_limits;

		ip[i++] = ipv6_hdr->dst_addr;
	}
	while (n_left_from > 0) {
		mbuf0 = pkts[0];
		pkts += 1;
		n_left_from -= 1;

		/* Extract DIP of mbuf0 */
		ipv6_hdr = rte_pktmbuf_mtod_offset(mbuf0, struct rte_ipv6_hdr *,
				sizeof(struct rte_ether_hdr));
		/* Extract hop_limits as ipv6 hdr is in cache */
		node_mbuf_priv1(mbuf0, dyn)->ttl = ipv6_hdr->hop_limits;

		ip[i++] = ipv6_hdr->dst_addr;
	}

	rte_fib6_lookup_bulk(fib, ip, next_hop, nb_objs);

	for (i = 0; i < nb_objs; i++) {
		mbuf0 = (struct rte_mbuf *)objs[i];
		node_mbuf_priv1(mbuf0, dyn)->nh = (uint16_t)next_hop[i];
		next = (uint16_t)(next_hop[i] >> 16);

		if (unlikely(next_index ^ next)) {
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

		if (unlikely(next_hop[i] == FIB6_DEFAULT_NH))
			lookup_err += 1;
	}

	/* !!! Home run !!! */
	if (likely(last_spec == nb_objs)) {
		rte_node_next_stream_move(graph, node, next_index);
		return nb_objs;
	}

	NODE_INCREMENT_XSTAT_ID(node, 0, lookup_err != 0, lookup_err);
	held += last_spec;
	rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
	rte_node_next_stream_put(graph, node, next_index, held);

	return nb_objs;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_node_ip6_fib_create, 25.07)
int
rte_node_ip6_fib_create(int socket, struct rte_fib6_conf *conf)
{
	struct ip6_lookup_fib_node_main *nm = &ip6_lookup_fib_nm;
	char s[RTE_FIB6_NAMESIZE];

	/* One fib6 per socket */
	if (nm->fib6[socket])
		return 0;

	conf->default_nh = FIB6_DEFAULT_NH;
	snprintf(s, sizeof(s), "IPV6_LOOKUP_FIB_%u", socket);
	nm->fib6[socket] = rte_fib6_create(s, socket, conf);
	if (nm->fib6[socket] == NULL)
		return -rte_errno;

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_node_ip6_fib_route_add, 25.07)
int
rte_node_ip6_fib_route_add(const struct rte_ipv6_addr *ip, uint8_t depth, uint16_t next_hop,
			   enum rte_node_ip6_lookup_next next_node)
{
	char abuf[INET6_ADDRSTRLEN];
	unsigned int nb_sockets;
	uint8_t socket;
	uint32_t val;
	int ret;

	nb_sockets = rte_socket_count();
	inet_ntop(AF_INET6, ip, abuf, sizeof(abuf));
	/* Embedded next node id into 24 bit next hop */
	val = ((next_node << 16) | next_hop) & ((1ull << 24) - 1);
	node_dbg("ip6_lookup_fib", "FIB6: Adding route %s / %d nh (0x%x)", abuf, depth, val);

	for (socket = 0; socket < nb_sockets; socket++) {
		if (!ip6_lookup_fib_nm.fib6[socket])
			continue;

		ret = rte_fib6_add(ip6_lookup_fib_nm.fib6[socket], ip, depth, val);
		if (ret < 0) {
			node_err("ip6_lookup_fib",
				 "Unable to add entry %s / %d nh (%x) to FIB on sock %d, rc=%d",
				 abuf, depth, val, socket, ret);
			return ret;
		}
	}

	return 0;
}

static int
setup_fib6(int socket)
{
	struct ip6_lookup_fib_node_main *nm = &ip6_lookup_fib_nm;
	struct rte_fib6_conf conf;
	char s[RTE_FIB6_NAMESIZE];

	/* One fib6 per socket */
	if (nm->fib6[socket])
		return 0;

	conf.type = RTE_FIB6_TRIE;
	conf.default_nh = FIB6_DEFAULT_NH;
	conf.max_routes = FIB6_DEFAULT_MAX_ROUTES;
	conf.rib_ext_sz = 0;
	conf.trie.nh_sz = RTE_FIB6_TRIE_4B;
	conf.trie.num_tbl8 = FIB6_DEFAULT_NUM_TBL8;
	snprintf(s, sizeof(s), "IPV6_LOOKUP_FIB_%u", socket);
	nm->fib6[socket] = rte_fib6_create(s, socket, &conf);
	if (nm->fib6[socket] == NULL)
		return -rte_errno;

	return 0;
}

static int
ip6_lookup_fib_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	int rc, dyn;

	RTE_BUILD_BUG_ON(sizeof(struct ip6_lookup_fib_node_ctx) > RTE_NODE_CTX_SZ);

	dyn = rte_node_mbuf_dynfield_register();
	if (dyn < 0) {
		node_err("ip6_lookup_fib", "Failed to register mbuf dynfield, rc=%d",
			 -rte_errno);
		return -rte_errno;
	}

	rc = setup_fib6(graph->socket);
	if (rc) {
		node_err("ip6_lookup_fib", "Failed to setup fib6 for sock %u, rc=%d",
				graph->socket, rc);
		return rc;
	}

	/* Update socket's FIB and mbuf dyn priv1 offset in node ctx */
	IP6_LOOKUP_FIB_NODE(node->ctx) = ip6_lookup_fib_nm.fib6[graph->socket];
	IP6_LOOKUP_FIB_NODE_PRIV1_OFF(node->ctx) = dyn;

	node_dbg("ip6_lookup_fib", "Initialized ip6_lookup_fib node");

	return 0;
}

static struct rte_node_xstats ip6_lookup_fib_xstats = {
	.nb_xstats = 1,
	.xstat_desc = {
		[0] = "ip6_lookup_fib_error",
	},
};

static struct rte_node_register ip6_lookup_fib_node = {
	.process = ip6_lookup_fib_node_process,
	.name = "ip6_lookup_fib",

	.init = ip6_lookup_fib_node_init,
	.xstats = &ip6_lookup_fib_xstats,

	.nb_edges = RTE_NODE_IP6_LOOKUP_NEXT_PKT_DROP + 1,
	.next_nodes = {
		[RTE_NODE_IP6_LOOKUP_NEXT_REWRITE] = "ip6_rewrite",
		[RTE_NODE_IP6_LOOKUP_NEXT_PKT_DROP] = "pkt_drop",
	},
};

RTE_NODE_REGISTER(ip6_lookup_fib_node);
