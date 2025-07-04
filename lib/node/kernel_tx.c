/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell International Ltd.
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_ip.h>

#include "kernel_tx_priv.h"
#include "node_private.h"

static __rte_always_inline void
kernel_tx_process_mbuf(struct rte_node *node, struct rte_mbuf **mbufs, uint16_t cnt)
{
	kernel_tx_node_ctx_t *ctx = (kernel_tx_node_ctx_t *)node->ctx;
	struct sockaddr_in sin = {0};
	struct rte_ipv4_hdr *ip4;
	size_t len;
	char *buf;
	int i;

	for (i = 0; i < cnt; i++) {
		ip4 = rte_pktmbuf_mtod(mbufs[i], struct rte_ipv4_hdr *);
		len = rte_pktmbuf_data_len(mbufs[i]);
		buf = (char *)ip4;

		sin.sin_family = AF_INET;
		sin.sin_port = 0;
		sin.sin_addr.s_addr = ip4->dst_addr;

		if (sendto(ctx->sock, buf, len, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0)
			node_err("kernel_tx", "Unable to send packets: %s", strerror(errno));
	}
}

static uint16_t
kernel_tx_node_process(struct rte_graph *graph __rte_unused, struct rte_node *node, void **objs,
			 uint16_t nb_objs)
{
	struct rte_mbuf **pkts = (struct rte_mbuf **)objs;
	uint16_t obj_left = nb_objs;

#define PREFETCH_CNT 4

	while (obj_left >= 12) {
		/* Prefetch next-next mbufs */
		rte_prefetch0(pkts[8]);
		rte_prefetch0(pkts[9]);
		rte_prefetch0(pkts[10]);
		rte_prefetch0(pkts[11]);

		/* Prefetch next mbuf data */
		rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[4], void *, pkts[4]->l2_len));
		rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[5], void *, pkts[5]->l2_len));
		rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[6], void *, pkts[6]->l2_len));
		rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[7], void *, pkts[7]->l2_len));

		kernel_tx_process_mbuf(node, pkts, PREFETCH_CNT);

		obj_left -= PREFETCH_CNT;
		pkts += PREFETCH_CNT;
	}

	while (obj_left > 0) {
		kernel_tx_process_mbuf(node, pkts, 1);

		obj_left--;
		pkts++;
	}

	rte_pktmbuf_free_bulk((struct rte_mbuf **)objs, nb_objs);

	return nb_objs;
}

static int
kernel_tx_node_init(const struct rte_graph *graph __rte_unused, struct rte_node *node)
{
	kernel_tx_node_ctx_t *ctx = (kernel_tx_node_ctx_t *)node->ctx;

	ctx->sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (ctx->sock < 0)
		node_err("kernel_tx", "Unable to open RAW socket");

	return 0;
}

static void
kernel_tx_node_fini(const struct rte_graph *graph __rte_unused, struct rte_node *node)
{
	kernel_tx_node_ctx_t *ctx = (kernel_tx_node_ctx_t *)node->ctx;

	if (ctx->sock >= 0) {
		close(ctx->sock);
		ctx->sock = -1;
	}
}

static struct rte_node_register kernel_tx_node_base = {
	.process = kernel_tx_node_process,
	.name = "kernel_tx",

	.init = kernel_tx_node_init,
	.fini = kernel_tx_node_fini,

	.nb_edges = 0,
};

struct rte_node_register *
kernel_tx_node_get(void)
{
	return &kernel_tx_node_base;
}

RTE_NODE_REGISTER(kernel_tx_node_base);
