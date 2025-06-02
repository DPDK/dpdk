/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */

#include <arpa/inet.h>

#include <eal_export.h>
#include <rte_errno.h>
#include <rte_ether.h>
#include <rte_fib.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_ip.h>

#include "rte_node_ip4_api.h"

#include "node_private.h"

/* IP4 Lookup global data struct */
struct ip4_lookup_fib_node_main {
	struct rte_fib *fib[RTE_MAX_NUMA_NODES];
};

struct ip4_lookup_fib_node_ctx {
	/* Socket's FIB */
	struct rte_fib *fib;
	/* Dynamic offset to mbuf priv1 */
	int mbuf_priv1_off;
};

static struct ip4_lookup_fib_node_main ip4_lookup_fib_nm;

#define FIB_DEFAULT_MAX_ROUTES (UINT16_MAX)
#define FIB_DEFAULT_NUM_TBL8   (UINT16_MAX / 2)
#define FIB_DEFAULT_NH (RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP << 16)

#define IP4_LOOKUP_NODE_FIB(ctx) \
	(((struct ip4_lookup_fib_node_ctx *)ctx)->fib)

#define IP4_LOOKUP_FIB_NODE_PRIV1_OFF(ctx) \
	(((struct ip4_lookup_fib_node_ctx *)ctx)->mbuf_priv1_off)

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_node_ip4_fib_create, 25.07)
int
rte_node_ip4_fib_create(int socket, struct rte_fib_conf *conf)
{
	struct ip4_lookup_fib_node_main *nm = &ip4_lookup_fib_nm;
	char s[RTE_FIB_NAMESIZE];

	/* One fib per socket */
	if (nm->fib[socket])
		return 0;

	conf->default_nh = FIB_DEFAULT_NH;
	snprintf(s, sizeof(s), "IPV4_LOOKUP_FIB_%d", socket);
	nm->fib[socket] = rte_fib_create(s, socket, conf);
	if (nm->fib[socket] == NULL)
		return -rte_errno;

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_node_ip4_fib_route_add, 25.07)
int
rte_node_ip4_fib_route_add(uint32_t ip, uint8_t depth, uint16_t next_hop,
			   enum rte_node_ip4_lookup_next next_node)
{
	char abuf[INET6_ADDRSTRLEN];
	unsigned int nb_sockets;
	struct in_addr in;
	uint8_t socket;
	uint32_t val;
	int ret;

	nb_sockets = rte_socket_count();
	in.s_addr = htonl(ip);
	inet_ntop(AF_INET, &in, abuf, sizeof(abuf));
	/* Embedded next node id into 24 bit next hop */
	val = ((next_node << 16) | next_hop) & ((1ull << 24) - 1);
	node_dbg("ip4_lookup_fib", "FIB: Adding route %s / %d nh (0x%x)", abuf, depth, val);

	for (socket = 0; socket < nb_sockets; socket++) {
		if (!ip4_lookup_fib_nm.fib[socket])
			continue;

		ret = rte_fib_add(ip4_lookup_fib_nm.fib[socket], ip, depth, val);
		if (ret < 0) {
			node_err("ip4_lookup_fib",
				 "Unable to add entry %s / %d nh (%x) to FIB on sock %d, rc=%d",
				 abuf, depth, val, socket, ret);
			return ret;
		}
	}

	return 0;
}

static int
setup_fib(int socket)
{
	struct ip4_lookup_fib_node_main *nm = &ip4_lookup_fib_nm;
	struct rte_fib_conf conf;
	char s[RTE_FIB_NAMESIZE];

	/* One fib per socket */
	if (nm->fib[socket])
		return 0;

	conf.type = RTE_FIB_DIR24_8;
	conf.default_nh = FIB_DEFAULT_NH;
	conf.max_routes = FIB_DEFAULT_MAX_ROUTES;
	conf.rib_ext_sz = 0;
	conf.dir24_8.nh_sz = RTE_FIB_DIR24_8_4B;
	conf.dir24_8.num_tbl8 = FIB_DEFAULT_NUM_TBL8;
	conf.flags = 0;
	snprintf(s, sizeof(s), "IPV4_LOOKUP_FIB_%d", socket);
	nm->fib[socket] = rte_fib_create(s, socket, &conf);
	if (nm->fib[socket] == NULL)
		return -rte_errno;

	return 0;
}

static int
ip4_lookup_fib_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	static uint8_t init_once;
	int rc;

	RTE_BUILD_BUG_ON(sizeof(struct ip4_lookup_fib_node_ctx) > RTE_NODE_CTX_SZ);

	if (!init_once) {
		node_mbuf_priv1_dynfield_offset = rte_mbuf_dynfield_register(
				&node_mbuf_priv1_dynfield_desc);
		if (node_mbuf_priv1_dynfield_offset < 0)
			return -rte_errno;

		init_once = 1;
	}

	rc = setup_fib(graph->socket);
	if (rc) {
		node_err("ip4_lookup_fib", "Failed to setup fib for sock %u, rc=%d",
				graph->socket, rc);
		return rc;
	}

	/* Update socket's FIB and mbuf dyn priv1 offset in node ctx */
	IP4_LOOKUP_NODE_FIB(node->ctx) = ip4_lookup_fib_nm.fib[graph->socket];
	IP4_LOOKUP_FIB_NODE_PRIV1_OFF(node->ctx) = node_mbuf_priv1_dynfield_offset;

	node_dbg("ip4_lookup_fib", "Initialized ip4_lookup_fib node");

	return 0;
}

static struct rte_node_xstats ip4_lookup_fib_xstats = {
	.nb_xstats = 1,
	.xstat_desc = {
		[0] = "ip4_lookup_fib_error",
	},
};

static struct rte_node_register ip4_lookup_fib_node = {
	.name = "ip4_lookup_fib",

	.init = ip4_lookup_fib_node_init,
	.xstats = &ip4_lookup_fib_xstats,

	.nb_edges = RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP + 1,
	.next_nodes = {
		[RTE_NODE_IP4_LOOKUP_NEXT_IP4_LOCAL] = "ip4_local",
		[RTE_NODE_IP4_LOOKUP_NEXT_REWRITE] = "ip4_rewrite",
		[RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP] = "pkt_drop",
	},
};

RTE_NODE_REGISTER(ip4_lookup_fib_node);
