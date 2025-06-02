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
	static uint8_t init_once;
	int rc;

	RTE_BUILD_BUG_ON(sizeof(struct ip6_lookup_fib_node_ctx) > RTE_NODE_CTX_SZ);

	if (!init_once) {
		node_mbuf_priv1_dynfield_offset = rte_mbuf_dynfield_register(
				&node_mbuf_priv1_dynfield_desc);
		if (node_mbuf_priv1_dynfield_offset < 0)
			return -rte_errno;

		init_once = 1;
	}

	rc = setup_fib6(graph->socket);
	if (rc) {
		node_err("ip6_lookup_fib", "Failed to setup fib6 for sock %u, rc=%d",
				graph->socket, rc);
		return rc;
	}

	/* Update socket's FIB and mbuf dyn priv1 offset in node ctx */
	IP6_LOOKUP_FIB_NODE(node->ctx) = ip6_lookup_fib_nm.fib6[graph->socket];
	IP6_LOOKUP_FIB_NODE_PRIV1_OFF(node->ctx) = node_mbuf_priv1_dynfield_offset;

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
