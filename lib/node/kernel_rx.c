/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell International Ltd.
 */

#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_net.h>

#include "ethdev_rx_priv.h"
#include "kernel_rx_priv.h"
#include "node_private.h"

static inline struct rte_mbuf *
alloc_rx_mbuf(kernel_rx_node_ctx_t *ctx)
{
	kernel_rx_info_t *rx = ctx->recv_info;

	if (rx->idx >= rx->cnt) {
		uint16_t cnt;

		rx->idx = 0;
		rx->cnt = 0;

		cnt = rte_pktmbuf_alloc_bulk(ctx->pktmbuf_pool, rx->rx_bufs, KERN_RX_CACHE_COUNT);
		if (cnt <= 0)
			return NULL;

		rx->cnt = cnt;
	}

	return rx->rx_bufs[rx->idx++];
}

static inline void
mbuf_update(struct rte_mbuf **mbufs, uint16_t nb_pkts)
{
	struct rte_net_hdr_lens hdr_lens;
	struct rte_mbuf *m;
	int i;

	for (i = 0; i < nb_pkts; i++) {
		m = mbufs[i];

		m->packet_type = rte_net_get_ptype(m, &hdr_lens, RTE_PTYPE_ALL_MASK);

		m->ol_flags = 0;
		m->tx_offload = 0;

		m->l2_len = hdr_lens.l2_len;
		m->l3_len = hdr_lens.l3_len;
		m->l4_len = hdr_lens.l4_len;
	}
}

static uint16_t
recv_pkt_parse(void **objs, uint16_t nb_pkts)
{
	uint16_t pkts_left = nb_pkts;
	struct rte_mbuf **pkts;
	int i;

	pkts = (struct rte_mbuf **)objs;

	if (pkts_left >= 4) {
		for (i = 0; i < 4; i++)
			rte_prefetch0(rte_pktmbuf_mtod(pkts[i], void *));
	}

	while (pkts_left >= 12) {
		/* Prefetch next-next mbufs */
		rte_prefetch0(pkts[8]);
		rte_prefetch0(pkts[9]);
		rte_prefetch0(pkts[10]);
		rte_prefetch0(pkts[11]);

		/* Prefetch next mbuf data */
		rte_prefetch0(rte_pktmbuf_mtod(pkts[4], void *));
		rte_prefetch0(rte_pktmbuf_mtod(pkts[5], void *));
		rte_prefetch0(rte_pktmbuf_mtod(pkts[6], void *));
		rte_prefetch0(rte_pktmbuf_mtod(pkts[7], void *));

		/* Extract ptype of mbufs */
		mbuf_update(pkts, 4);

		pkts += 4;
		pkts_left -= 4;
	}

	if (pkts_left > 0)
		mbuf_update(pkts, pkts_left);

	return nb_pkts;
}

static uint16_t
kernel_rx_node_do(struct rte_graph *graph, struct rte_node *node, kernel_rx_node_ctx_t *ctx)
{
	kernel_rx_info_t *rx;
	uint16_t next_index;
	int fd;

	rx = ctx->recv_info;
	next_index = rx->node_next;

	fd = rx->sock;
	if (fd > 0) {
		struct rte_mbuf **mbufs;
		uint16_t len = 0, count = 0;
		int nb_cnt, i;

		nb_cnt = (node->size >= RTE_GRAPH_BURST_SIZE) ? RTE_GRAPH_BURST_SIZE : node->size;

		mbufs = (struct rte_mbuf **)node->objs;
		for (i = 0; i < nb_cnt; i++) {
			struct rte_mbuf *m = alloc_rx_mbuf(ctx);

			if (!m)
				break;

			len = read(fd, rte_pktmbuf_mtod(m, char *), rte_pktmbuf_tailroom(m));
			if (len == 0 || len == 0xFFFF) {
				rte_pktmbuf_free(m);
				if (rx->idx <= 0)
					node_dbg("kernel_rx", "rx_mbuf array is empty\n");
				rx->idx--;
				break;
			}
			*mbufs++ = m;

			m->port = node->id;
			rte_pktmbuf_data_len(m) = len;

			count++;
		}

		if (count) {
			recv_pkt_parse(node->objs, count);
			node->idx = count;

			/* Enqueue to next node */
			rte_node_next_stream_move(graph, node, next_index);
		}

		return count;
	}

	return 0;
}

static uint16_t
kernel_rx_node_process(struct rte_graph *graph, struct rte_node *node, void **objs,
			 uint16_t nb_objs)
{
	kernel_rx_node_ctx_t *ctx = (kernel_rx_node_ctx_t *)node->ctx;
	int fd;

	RTE_SET_USED(objs);
	RTE_SET_USED(nb_objs);

	if (!ctx)
		return 0;

	fd = ctx->recv_info->sock;
	if (fd > 0) {
		struct pollfd fds = {.fd = fd, .events = POLLIN};

		if (poll(&fds, 1, 0) > 0) {
			if (fds.revents & POLLIN)
				return kernel_rx_node_do(graph, node, ctx);
		}
	}

	return 0;
}

static int
kernel_rx_node_init(const struct rte_graph *graph, struct rte_node *node)
{
	struct kernel_rx_node_main *rx_node_main = kernel_rx_node_data_get();
	kernel_rx_node_ctx_t *ctx = (kernel_rx_node_ctx_t *)node->ctx;
	kernel_rx_node_elem_t *elem = rx_node_main->head;
	kernel_rx_info_t *recv_info;
	int sock;

	while (elem) {
		if (elem->nid == node->id) {
			/* Update node specific context */
			memcpy(ctx, &elem->ctx, sizeof(kernel_rx_node_ctx_t));
			break;
		}
		elem = elem->next;
	}

	RTE_VERIFY(elem != NULL);

	if (ctx->pktmbuf_pool == NULL) {
		node_err("kernel_rx", "Invalid mbuf pool on graph %s\n", graph->name);
		return -EINVAL;
	}

	recv_info = rte_zmalloc_socket("kernel_rx_info", sizeof(kernel_rx_info_t),
				       RTE_CACHE_LINE_SIZE, graph->socket);
	if (!recv_info) {
		node_err("kernel_rx", "Kernel recv_info is NULL\n");
		return -ENOMEM;
	}

	sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (sock < 0) {
		node_err("kernel_rx", "Unable to open RAW socket\n");
		return sock;
	}

	recv_info->sock = sock;
	ctx->recv_info = recv_info;

	return 0;
}

static void
kernel_rx_node_fini(const struct rte_graph *graph __rte_unused, struct rte_node *node)
{
	kernel_rx_node_ctx_t *ctx = (kernel_rx_node_ctx_t *)node->ctx;

	if (ctx->recv_info) {
		close(ctx->recv_info->sock);
		ctx->recv_info->sock = -1;
		rte_free(ctx->recv_info);
	}

	ctx->recv_info = NULL;
}

struct kernel_rx_node_main *
kernel_rx_node_data_get(void)
{
	static struct kernel_rx_node_main kernel_rx_main;

	return &kernel_rx_main;
}

static struct rte_node_register kernel_rx_node_base = {
	.process = kernel_rx_node_process,
	.flags = RTE_NODE_SOURCE_F,
	.name = "kernel_rx",

	.init = kernel_rx_node_init,
	.fini = kernel_rx_node_fini,

	.nb_edges = KERNEL_RX_NEXT_MAX,
	.next_nodes = {
			[KERNEL_RX_NEXT_PKT_CLS] = "pkt_cls",
			[KERNEL_RX_NEXT_IP4_LOOKUP] = "ip4_lookup",
	},
};

struct rte_node_register *
kernel_rx_node_get(void)
{
	return &kernel_rx_node_base;
}

RTE_NODE_REGISTER(kernel_rx_node_base);
