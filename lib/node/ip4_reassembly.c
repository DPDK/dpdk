/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_ip.h>
#include <rte_ip_frag.h>
#include <rte_mbuf.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include "rte_node_ip4_api.h"

#include "ip4_reassembly_priv.h"
#include "node_private.h"

struct ip4_reassembly_elem {
	struct ip4_reassembly_elem *next;
	struct ip4_reassembly_ctx ctx;
	rte_node_t node_id;
};

/* IP4 reassembly global data struct */
struct ip4_reassembly_node_main {
	struct ip4_reassembly_elem *head;
};

typedef struct ip4_reassembly_ctx ip4_reassembly_ctx_t;
typedef struct ip4_reassembly_elem ip4_reassembly_elem_t;

static struct ip4_reassembly_node_main ip4_reassembly_main;

static uint16_t
ip4_reassembly_node_process(struct rte_graph *graph, struct rte_node *node, void **objs,
			    uint16_t nb_objs)
{
#define PREFETCH_OFFSET 4
	struct rte_mbuf *mbuf, *mbuf_out;
	struct rte_ip_frag_death_row *dr;
	struct ip4_reassembly_ctx *ctx;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ip_frag_tbl *tbl;
	void **to_next, **to_free;
	uint16_t idx = 0;
	int i;

	ctx = (struct ip4_reassembly_ctx *)node->ctx;

	/* Get core specific reassembly tbl */
	tbl = ctx->tbl;
	dr = ctx->dr;

	for (i = 0; i < PREFETCH_OFFSET && i < nb_objs; i++) {
		rte_prefetch0(rte_pktmbuf_mtod_offset((struct rte_mbuf *)objs[i], void *,
						      sizeof(struct rte_ether_hdr)));
	}

	to_next = node->objs;
	for (i = 0; i < nb_objs - PREFETCH_OFFSET; i++) {
#if RTE_GRAPH_BURST_SIZE > 64
		/* Prefetch next-next mbufs */
		if (likely(i + 8 < nb_objs))
			rte_prefetch0(objs[i + 8]);
#endif
		rte_prefetch0(rte_pktmbuf_mtod_offset((struct rte_mbuf *)objs[i + PREFETCH_OFFSET],
						      void *, sizeof(struct rte_ether_hdr)));
		mbuf = (struct rte_mbuf *)objs[i];

		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr *,
						   sizeof(struct rte_ether_hdr));
		if (rte_ipv4_frag_pkt_is_fragmented(ipv4_hdr)) {
			/* prepare mbuf: setup l2_len/l3_len. */
			mbuf->l2_len = sizeof(struct rte_ether_hdr);
			mbuf->l3_len = sizeof(struct rte_ipv4_hdr);

			mbuf_out = rte_ipv4_frag_reassemble_packet(tbl, dr, mbuf, rte_rdtsc(),
								   ipv4_hdr);
		} else {
			mbuf_out = mbuf;
		}

		if (mbuf_out)
			to_next[idx++] = (void *)mbuf_out;
	}

	for (; i < nb_objs; i++) {
		mbuf = (struct rte_mbuf *)objs[i];

		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ipv4_hdr *,
						   sizeof(struct rte_ether_hdr));
		if (rte_ipv4_frag_pkt_is_fragmented(ipv4_hdr)) {
			/* prepare mbuf: setup l2_len/l3_len. */
			mbuf->l2_len = sizeof(struct rte_ether_hdr);
			mbuf->l3_len = sizeof(struct rte_ipv4_hdr);

			mbuf_out = rte_ipv4_frag_reassemble_packet(tbl, dr, mbuf, rte_rdtsc(),
								   ipv4_hdr);
		} else {
			mbuf_out = mbuf;
		}

		if (mbuf_out)
			to_next[idx++] = (void *)mbuf_out;
	}
	node->idx = idx;
	rte_node_next_stream_move(graph, node, 1);
	if (dr->cnt) {
		to_free = rte_node_next_stream_get(graph, node,
						   RTE_NODE_IP4_REASSEMBLY_NEXT_PKT_DROP, dr->cnt);
		rte_memcpy(to_free, dr->row, dr->cnt * sizeof(to_free[0]));
		rte_node_next_stream_put(graph, node, RTE_NODE_IP4_REASSEMBLY_NEXT_PKT_DROP,
					 dr->cnt);
		idx += dr->cnt;
		dr->cnt = 0;
	}

	return idx;
}

int
rte_node_ip4_reassembly_configure(struct rte_node_ip4_reassembly_cfg *cfg, uint16_t cnt)
{
	ip4_reassembly_elem_t *elem;
	int i;

	for (i = 0; i < cnt; i++) {
		elem = malloc(sizeof(ip4_reassembly_elem_t));
		if (elem == NULL)
			return -ENOMEM;
		elem->ctx.dr = cfg[i].dr;
		elem->ctx.tbl = cfg[i].tbl;
		elem->node_id = cfg[i].node_id;
		elem->next = ip4_reassembly_main.head;
		ip4_reassembly_main.head = elem;
	}

	return 0;
}

static int
ip4_reassembly_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	ip4_reassembly_ctx_t *ctx = (ip4_reassembly_ctx_t *)node->ctx;
	ip4_reassembly_elem_t *elem = ip4_reassembly_main.head;

	RTE_SET_USED(graph);
	while (elem) {
		if (elem->node_id == node->id) {
			/* Update node specific context */
			memcpy(ctx, &elem->ctx, sizeof(ip4_reassembly_ctx_t));
			break;
		}
		elem = elem->next;
	}

	return 0;
}

static struct rte_node_register ip4_reassembly_node = {
	.process = ip4_reassembly_node_process,
	.name = "ip4_reassembly",

	.init = ip4_reassembly_node_init,

	.nb_edges = RTE_NODE_IP4_REASSEMBLY_NEXT_PKT_DROP + 1,
	.next_nodes = {
		[RTE_NODE_IP4_REASSEMBLY_NEXT_PKT_DROP] = "pkt_drop",
	},
};

struct rte_node_register *
ip4_reassembly_node_get(void)
{
	return &ip4_reassembly_node;
}

RTE_NODE_REGISTER(ip4_reassembly_node);
