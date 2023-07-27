/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>

#include "ethdev_rx_priv.h"
#include "node_private.h"

static struct ethdev_rx_node_main ethdev_rx_main;

static __rte_always_inline uint16_t
ethdev_rx_node_process_inline(struct rte_graph *graph, struct rte_node *node,
			      ethdev_rx_node_ctx_t *ctx)
{
	uint16_t count, next_index;
	uint16_t port, queue;

	port = ctx->port_id;
	queue = ctx->queue_id;
	next_index = ctx->cls_next;

	/* Get pkts from port */
	count = rte_eth_rx_burst(port, queue, (struct rte_mbuf **)node->objs,
				 RTE_GRAPH_BURST_SIZE);

	if (!count)
		return 0;
	node->idx = count;
	/* Enqueue to next node */
	rte_node_next_stream_move(graph, node, next_index);

	return count;
}

static __rte_always_inline uint16_t
ethdev_rx_node_process(struct rte_graph *graph, struct rte_node *node,
		       void **objs, uint16_t cnt)
{
	ethdev_rx_node_ctx_t *ctx = (ethdev_rx_node_ctx_t *)node->ctx;
	uint16_t n_pkts = 0;

	RTE_SET_USED(objs);
	RTE_SET_USED(cnt);

	n_pkts = ethdev_rx_node_process_inline(graph, node, ctx);
	return n_pkts;
}

static inline uint32_t
l3_ptype(uint16_t etype, uint32_t ptype)
{
	ptype = ptype & ~RTE_PTYPE_L3_MASK;
	if (etype == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
		ptype |= RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
	else if (etype == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6))
		ptype |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
	return ptype;
}

/* Callback for soft ptype parsing */
static uint16_t
eth_pkt_parse_cb(uint16_t port, uint16_t queue, struct rte_mbuf **mbufs,
		 uint16_t nb_pkts, uint16_t max_pkts, void *user_param)
{
	struct rte_mbuf *mbuf0, *mbuf1, *mbuf2, *mbuf3;
	struct rte_ether_hdr *eth_hdr;
	uint16_t etype, n_left;
	struct rte_mbuf **pkts;

	RTE_SET_USED(port);
	RTE_SET_USED(queue);
	RTE_SET_USED(max_pkts);
	RTE_SET_USED(user_param);

	pkts = mbufs;
	n_left = nb_pkts;
	while (n_left >= 12) {

		/* Prefetch next-next mbufs */
		rte_prefetch0(pkts[8]);
		rte_prefetch0(pkts[9]);
		rte_prefetch0(pkts[10]);
		rte_prefetch0(pkts[11]);

		/* Prefetch next mbuf data */
		rte_prefetch0(
			rte_pktmbuf_mtod(pkts[4], struct rte_ether_hdr *));
		rte_prefetch0(
			rte_pktmbuf_mtod(pkts[5], struct rte_ether_hdr *));
		rte_prefetch0(
			rte_pktmbuf_mtod(pkts[6], struct rte_ether_hdr *));
		rte_prefetch0(
			rte_pktmbuf_mtod(pkts[7], struct rte_ether_hdr *));

		mbuf0 = pkts[0];
		mbuf1 = pkts[1];
		mbuf2 = pkts[2];
		mbuf3 = pkts[3];
		pkts += 4;
		n_left -= 4;

		/* Extract ptype of mbuf0 */
		eth_hdr = rte_pktmbuf_mtod(mbuf0, struct rte_ether_hdr *);
		etype = eth_hdr->ether_type;
		mbuf0->packet_type = l3_ptype(etype, 0);

		/* Extract ptype of mbuf1 */
		eth_hdr = rte_pktmbuf_mtod(mbuf1, struct rte_ether_hdr *);
		etype = eth_hdr->ether_type;
		mbuf1->packet_type = l3_ptype(etype, 0);

		/* Extract ptype of mbuf2 */
		eth_hdr = rte_pktmbuf_mtod(mbuf2, struct rte_ether_hdr *);
		etype = eth_hdr->ether_type;
		mbuf2->packet_type = l3_ptype(etype, 0);

		/* Extract ptype of mbuf3 */
		eth_hdr = rte_pktmbuf_mtod(mbuf3, struct rte_ether_hdr *);
		etype = eth_hdr->ether_type;
		mbuf3->packet_type = l3_ptype(etype, 0);
	}

	while (n_left > 0) {
		mbuf0 = pkts[0];

		pkts += 1;
		n_left -= 1;

		/* Extract ptype of mbuf0 */
		eth_hdr = rte_pktmbuf_mtod(mbuf0, struct rte_ether_hdr *);
		etype = eth_hdr->ether_type;
		mbuf0->packet_type = l3_ptype(etype, 0);
	}

	return nb_pkts;
}

#define MAX_PTYPES 16
static int
ethdev_ptype_setup(uint16_t port, uint16_t queue)
{
	uint8_t l3_ipv4 = 0, l3_ipv6 = 0;
	uint32_t ptypes[MAX_PTYPES];
	int i, rc;

	/* Check IPv4 & IPv6 ptype support */
	rc = rte_eth_dev_get_supported_ptypes(port, RTE_PTYPE_L3_MASK, ptypes,
					      MAX_PTYPES);
	for (i = 0; i < rc; i++) {
		if (ptypes[i] & RTE_PTYPE_L3_IPV4)
			l3_ipv4 = 1;
		if (ptypes[i] & RTE_PTYPE_L3_IPV6)
			l3_ipv6 = 1;
	}

	if (!l3_ipv4 || !l3_ipv6) {
		node_info("ethdev_rx",
			  "Enabling ptype callback for required ptypes on port %u\n",
			  port);

		if (!rte_eth_add_rx_callback(port, queue, eth_pkt_parse_cb,
					     NULL)) {
			node_err("ethdev_rx",
				 "Failed to add rx ptype cb: port=%d, queue=%d\n",
				 port, queue);
			return -EINVAL;
		}
	}

	return 0;
}

static int
ethdev_rx_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	ethdev_rx_node_ctx_t *ctx = (ethdev_rx_node_ctx_t *)node->ctx;
	ethdev_rx_node_elem_t *elem = ethdev_rx_main.head;

	RTE_SET_USED(graph);

	while (elem) {
		if (elem->nid == node->id) {
			/* Update node specific context */
			memcpy(ctx, &elem->ctx, sizeof(ethdev_rx_node_ctx_t));
			break;
		}
		elem = elem->next;
	}

	RTE_VERIFY(elem != NULL);

	/* Check and setup ptype */
	return ethdev_ptype_setup(ctx->port_id, ctx->queue_id);
}

struct ethdev_rx_node_main *
ethdev_rx_get_node_data_get(void)
{
	return &ethdev_rx_main;
}

static struct rte_node_register ethdev_rx_node_base = {
	.process = ethdev_rx_node_process,
	.flags = RTE_NODE_SOURCE_F,
	.name = "ethdev_rx",

	.init = ethdev_rx_node_init,

	.nb_edges = ETHDEV_RX_NEXT_MAX,
	.next_nodes = {
		[ETHDEV_RX_NEXT_PKT_CLS] = "pkt_cls",
		[ETHDEV_RX_NEXT_IP4_LOOKUP] = "ip4_lookup",
		[ETHDEV_RX_NEXT_IP4_REASSEMBLY] = "ip4_reassembly",
	},
};

struct rte_node_register *
ethdev_rx_node_get(void)
{
	return &ethdev_rx_node_base;
}

RTE_NODE_REGISTER(ethdev_rx_node_base);
