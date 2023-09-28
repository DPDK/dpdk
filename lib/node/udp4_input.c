/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell International Ltd.
 */

#include <arpa/inet.h>
#include <sys/socket.h>

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_ip.h>
#include <rte_lpm.h>
#include <rte_hash.h>
#include <rte_fbk_hash.h>
#include <rte_jhash.h>
#include <rte_hash_crc.h>

#include "rte_node_udp4_input_api.h"

#include "node_private.h"

#define UDP4_INPUT_HASH_TBL_SIZE 1024

#define UDP4_INPUT_NODE_HASH(ctx) \
	(((struct udp4_input_node_ctx *)ctx)->hash)

#define UDP4_INPUT_NODE_NEXT_INDEX(ctx) \
	(((struct udp4_input_node_ctx *)ctx)->next_index)


/* UDP4 input  global data struct */
struct udp4_input_node_main {
	struct rte_hash *hash_tbl[RTE_MAX_NUMA_NODES];
};

static struct udp4_input_node_main udp4_input_nm;

struct udp4_input_node_ctx {
	/* Socket's Hash table */
	struct rte_hash *hash;
	/* Cached next index */
	uint16_t next_index;
};

struct flow_key {
	uint32_t prt_dst;
};

static struct rte_hash_parameters udp4_params = {
	.entries = UDP4_INPUT_HASH_TBL_SIZE,
	.key_len = sizeof(uint32_t),
	.hash_func = rte_jhash,
	.hash_func_init_val = 0,
	.socket_id = 0,
};

int
rte_node_udp4_dst_port_add(uint32_t dst_port, rte_edge_t next_node)
{
	uint8_t socket;
	int rc;

	for (socket = 0; socket < RTE_MAX_NUMA_NODES; socket++) {
		if (!udp4_input_nm.hash_tbl[socket])
			continue;

		rc = rte_hash_add_key_data(udp4_input_nm.hash_tbl[socket],
					   &dst_port, (void *)(uintptr_t)next_node);
		if (rc < 0) {
			node_err("udp4_lookup", "Failed to add key for sock %u, rc=%d",
					socket, rc);
			return rc;
		}
	}
	return 0;
}

int
rte_node_udp4_usr_node_add(const char *usr_node)
{
	const char *next_nodes = usr_node;
	rte_node_t udp4_input_node_id, count;

	udp4_input_node_id = rte_node_from_name("udp4_input");
	count = rte_node_edge_update(udp4_input_node_id, RTE_EDGE_ID_INVALID,
				     &next_nodes, 1);
	if (count == 0) {
		node_dbg("udp4_input", "Adding usr node as edge to udp4_input failed");
		return count;
	}
	count = rte_node_edge_count(udp4_input_node_id) - 1;
	return count;
}

static int
setup_udp4_dstprt_hash(struct udp4_input_node_main *nm, int socket)
{
	struct rte_hash_parameters *hash_udp4 = &udp4_params;
	char s[RTE_HASH_NAMESIZE];

	/* One Hash table per socket */
	if (nm->hash_tbl[socket])
		return 0;

	/* create Hash table */
	snprintf(s, sizeof(s), "UDP4_INPUT_HASH_%d", socket);
	hash_udp4->name = s;
	hash_udp4->socket_id = socket;
	nm->hash_tbl[socket] = rte_hash_create(hash_udp4);
	if (nm->hash_tbl[socket] == NULL)
		return -rte_errno;

	return 0;
}

static int
udp4_input_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	uint16_t socket, lcore_id;
	static uint8_t init_once;
	int rc;

	RTE_SET_USED(graph);
	RTE_BUILD_BUG_ON(sizeof(struct udp4_input_node_ctx) > RTE_NODE_CTX_SZ);

	if (!init_once) {

		/* Setup HASH tables for all sockets */
		RTE_LCORE_FOREACH(lcore_id)
		{
			socket = rte_lcore_to_socket_id(lcore_id);
			rc = setup_udp4_dstprt_hash(&udp4_input_nm, socket);
			if (rc) {
				node_err("udp4_lookup",
						"Failed to setup hash tbl for sock %u, rc=%d",
						socket, rc);
				return rc;
			}
		}
		init_once = 1;
	}

	UDP4_INPUT_NODE_HASH(node->ctx) = udp4_input_nm.hash_tbl[graph->socket];

	node_dbg("udp4_input", "Initialized udp4_input node");
	return 0;
}

static uint16_t
udp4_input_node_process_scalar(struct rte_graph *graph, struct rte_node *node,
			       void **objs, uint16_t nb_objs)
{
	struct rte_hash *hash_tbl_handle = UDP4_INPUT_NODE_HASH(node->ctx);
	rte_edge_t next_index, udplookup_node;
	struct rte_udp_hdr *pkt_udp_hdr;
	uint16_t last_spec = 0;
	void **to_next, **from;
	struct rte_mbuf *mbuf;
	uint16_t held = 0;
	uint16_t next = 0;
	int i, rc;

	/* Speculative next */
	next_index = UDP4_INPUT_NODE_NEXT_INDEX(node->ctx);

	from = objs;

	to_next = rte_node_next_stream_get(graph, node, next_index, nb_objs);
	for (i = 0; i < nb_objs; i++) {
		struct flow_key key_port;

		mbuf = (struct rte_mbuf *)objs[i];
		pkt_udp_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_udp_hdr *,
						sizeof(struct rte_ether_hdr) +
						sizeof(struct rte_ipv4_hdr));

		key_port.prt_dst = rte_cpu_to_be_16(pkt_udp_hdr->dst_port);
		rc = rte_hash_lookup_data(hash_tbl_handle,
					  &key_port.prt_dst,
					  (void **)&udplookup_node);
		next = (rc < 0) ? RTE_NODE_UDP4_INPUT_NEXT_PKT_DROP
				    : udplookup_node;

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
	/* Save the last next used */
	UDP4_INPUT_NODE_NEXT_INDEX(node->ctx) = next;

	return nb_objs;
}

static struct rte_node_register udp4_input_node = {
	.process = udp4_input_node_process_scalar,
	.name = "udp4_input",

	.init = udp4_input_node_init,

	.nb_edges = RTE_NODE_UDP4_INPUT_NEXT_PKT_DROP + 1,
	.next_nodes = {
		[RTE_NODE_UDP4_INPUT_NEXT_PKT_DROP] = "pkt_drop",
	},
};

RTE_NODE_REGISTER(udp4_input_node);
