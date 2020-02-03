/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "l2fwd_poll.h"

static inline void
l2fwd_poll_simple_forward(struct l2fwd_resources *rsrc, struct rte_mbuf *m,
			  uint32_t portid)
{
	struct rte_eth_dev_tx_buffer *buffer;
	uint32_t dst_port;
	int sent;

	dst_port = rsrc->dst_ports[portid];

	if (rsrc->mac_updating)
		l2fwd_mac_updating(m, dst_port, &rsrc->eth_addr[dst_port]);

	buffer = ((struct l2fwd_poll_resources *)rsrc->poll_rsrc)->tx_buffer[
								dst_port];
	sent = rte_eth_tx_buffer(dst_port, 0, buffer, m);
	if (sent)
		rsrc->port_stats[dst_port].tx += sent;
}

/* main poll mode processing loop */
static void
l2fwd_poll_main_loop(struct l2fwd_resources *rsrc)
{
	uint64_t prev_tsc, diff_tsc, cur_tsc, drain_tsc;
	struct l2fwd_poll_resources *poll_rsrc = rsrc->poll_rsrc;
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_eth_dev_tx_buffer *buf;
	struct lcore_queue_conf *qconf;
	uint32_t i, j, port_id, nb_rx;
	struct rte_mbuf *m;
	uint32_t lcore_id;
	int32_t sent;

	drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
			BURST_TX_DRAIN_US;
	prev_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &poll_rsrc->lcore_queue_conf[lcore_id];

	if (qconf->n_rx_port == 0) {
		printf("lcore %u has nothing to do\n", lcore_id);
		return;
	}

	printf("entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++) {

		port_id = qconf->rx_port_list[i];
		printf(" -- lcoreid=%u port_id=%u\n", lcore_id, port_id);

	}

	while (!rsrc->force_quit) {

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {
			for (i = 0; i < qconf->n_rx_port; i++) {
				port_id =
					rsrc->dst_ports[qconf->rx_port_list[i]];
				buf = poll_rsrc->tx_buffer[port_id];
				sent = rte_eth_tx_buffer_flush(port_id, 0, buf);
				if (sent)
					rsrc->port_stats[port_id].tx += sent;
			}

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_port; i++) {

			port_id = qconf->rx_port_list[i];
			nb_rx = rte_eth_rx_burst(port_id, 0, pkts_burst,
						 MAX_PKT_BURST);

			rsrc->port_stats[port_id].rx += nb_rx;

			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				l2fwd_poll_simple_forward(rsrc, m, port_id);
			}
		}
	}
}

static void
l2fwd_poll_lcore_config(struct l2fwd_resources *rsrc)
{
	struct l2fwd_poll_resources *poll_rsrc = rsrc->poll_rsrc;
	struct lcore_queue_conf *qconf = NULL;
	uint32_t rx_lcore_id = 0;
	uint16_t port_id;

	/* Initialize the port/queue configuration of each logical core */
	RTE_ETH_FOREACH_DEV(port_id) {
		/* skip ports that are not enabled */
		if ((rsrc->enabled_port_mask & (1 << port_id)) == 0)
			continue;

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       rx_lcore_id == rte_get_master_lcore() ||
		       poll_rsrc->lcore_queue_conf[rx_lcore_id].n_rx_port ==
		       rsrc->rx_queue_per_lcore) {
			rx_lcore_id++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_panic("Not enough cores\n");
		}

		if (qconf != &poll_rsrc->lcore_queue_conf[rx_lcore_id]) {
			/* Assigned a new logical core in the loop above. */
			qconf = &poll_rsrc->lcore_queue_conf[rx_lcore_id];
		}

		qconf->rx_port_list[qconf->n_rx_port] = port_id;
		qconf->n_rx_port++;
		printf("Lcore %u: RX port %u\n", rx_lcore_id, port_id);
	}
}

static void
l2fwd_poll_init_tx_buffers(struct l2fwd_resources *rsrc)
{
	struct l2fwd_poll_resources *poll_rsrc = rsrc->poll_rsrc;
	uint16_t port_id;
	int ret;

	RTE_ETH_FOREACH_DEV(port_id) {
		/* Initialize TX buffers */
		poll_rsrc->tx_buffer[port_id] = rte_zmalloc_socket("tx_buffer",
				RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
				rte_eth_dev_socket_id(port_id));
		if (poll_rsrc->tx_buffer[port_id] == NULL)
			rte_panic("Cannot allocate buffer for tx on port %u\n",
				  port_id);

		rte_eth_tx_buffer_init(poll_rsrc->tx_buffer[port_id],
				       MAX_PKT_BURST);

		ret = rte_eth_tx_buffer_set_err_callback(
				poll_rsrc->tx_buffer[port_id],
				rte_eth_tx_buffer_count_callback,
				&rsrc->port_stats[port_id].dropped);
		if (ret < 0)
			rte_panic("Cannot set error callback for tx buffer on port %u\n",
				  port_id);
	}
}

void
l2fwd_poll_resource_setup(struct l2fwd_resources *rsrc)
{
	struct l2fwd_poll_resources *poll_rsrc;

	poll_rsrc = rte_zmalloc("l2fwd_poll_rsrc",
				sizeof(struct l2fwd_poll_resources), 0);
	if (poll_rsrc == NULL)
		rte_panic("Failed to allocate resources for l2fwd poll mode\n");

	rsrc->poll_rsrc = poll_rsrc;
	l2fwd_poll_lcore_config(rsrc);
	l2fwd_poll_init_tx_buffers(rsrc);

	poll_rsrc->poll_main_loop = l2fwd_poll_main_loop;
}
