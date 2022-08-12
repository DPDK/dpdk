/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "l2fwd_common.h"

int
l2fwd_event_init_ports(struct l2fwd_resources *rsrc)
{
	uint16_t nb_rxd = RX_DESC_DEFAULT;
	uint16_t nb_txd = TX_DESC_DEFAULT;
	struct rte_eth_conf port_conf = {
		.txmode = {
			.mq_mode = RTE_ETH_MQ_TX_NONE,
		},
	};
	uint16_t nb_ports_available = 0;
	uint16_t port_id;
	int ret;

	if (rsrc->event_mode) {
		port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
		port_conf.rx_adv_conf.rss_conf.rss_key = NULL;
		port_conf.rx_adv_conf.rss_conf.rss_hf = RTE_ETH_RSS_IP;
	}

	/* Initialise each port */
	RTE_ETH_FOREACH_DEV(port_id) {
		struct rte_eth_conf local_port_conf = port_conf;
		struct rte_eth_dev_info dev_info;
		struct rte_eth_rxconf rxq_conf;
		struct rte_eth_txconf txq_conf;

		/* skip ports that are not enabled */
		if ((rsrc->enabled_port_mask & (1 << port_id)) == 0) {
			printf("Skipping disabled port %u\n", port_id);
			continue;
		}
		nb_ports_available++;

		/* init port */
		printf("Initializing port %u... ", port_id);
		fflush(stdout);

		ret = rte_eth_dev_info_get(port_id, &dev_info);
		if (ret != 0)
			rte_panic("Error during getting device (port %u) info: %s\n",
				  port_id, strerror(-ret));
		local_port_conf.rx_adv_conf.rss_conf.rss_hf &=
			dev_info.flow_type_rss_offloads;
		if (local_port_conf.rx_adv_conf.rss_conf.rss_hf !=
				port_conf.rx_adv_conf.rss_conf.rss_hf) {
			printf("Port %u modified RSS hash function based on hardware support,"
			       "requested:%#"PRIx64" configured:%#"PRIx64"",
				port_id,
				port_conf.rx_adv_conf.rss_conf.rss_hf,
				local_port_conf.rx_adv_conf.rss_conf.rss_hf);
		}

		if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
		/* Configure RX and TX queue. 8< */
		ret = rte_eth_dev_configure(port_id, 1, 1, &local_port_conf);
		if (ret < 0)
			rte_panic("Cannot configure device: err=%d, port=%u\n",
				  ret, port_id);
		/* >8 End of configuration RX and TX queue. */

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_panic("Cannot adjust number of descriptors: err=%d, port=%u\n",
				  ret, port_id);

		rte_eth_macaddr_get(port_id, &rsrc->eth_addr[port_id]);

		/* init one RX queue */
		fflush(stdout);
		rxq_conf = dev_info.default_rxconf;
		rxq_conf.offloads = local_port_conf.rxmode.offloads;
		/* Using lcore to poll one or several ports. 8< */
		ret = rte_eth_rx_queue_setup(port_id, 0, nb_rxd,
					     rte_eth_dev_socket_id(port_id),
					     &rxq_conf,
					     rsrc->pktmbuf_pool);
		if (ret < 0)
			rte_panic("rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, port_id);

		/* >8 End of using lcore to poll one or several ports. */

		/* Init one TX queue on each port. 8< */
		fflush(stdout);
		txq_conf = dev_info.default_txconf;
		txq_conf.offloads = local_port_conf.txmode.offloads;
		ret = rte_eth_tx_queue_setup(port_id, 0, nb_txd,
				rte_eth_dev_socket_id(port_id),
				&txq_conf);
		if (ret < 0)
			rte_panic("rte_eth_tx_queue_setup:err=%d, port=%u\n",
				  ret, port_id);
		/* >8 End of init one TX queue on each port. */

		rte_eth_promiscuous_enable(port_id);

		printf("Port %u,MAC address: " RTE_ETHER_ADDR_PRT_FMT "\n\n",
			port_id,
			RTE_ETHER_ADDR_BYTES(&rsrc->eth_addr[port_id]));
	}

	return nb_ports_available;
}

static void
l2fwd_event_vector_array_free(struct rte_event events[], uint16_t num)
{
	uint16_t i;

	for (i = 0; i < num; i++) {
		rte_pktmbuf_free_bulk(
			&events[i].vec->mbufs[events[i].vec->elem_offset],
			events[i].vec->nb_elem);
		rte_mempool_put(rte_mempool_from_obj(events[i].vec),
				events[i].vec);
	}
}

static void
l2fwd_event_port_flush(uint8_t event_d_id __rte_unused, struct rte_event ev,
		       void *args __rte_unused)
{
	if (ev.event_type & RTE_EVENT_TYPE_VECTOR)
		l2fwd_event_vector_array_free(&ev, 1);
	else
		rte_pktmbuf_free(ev.mbuf);
}

void
l2fwd_event_worker_cleanup(uint8_t event_d_id, uint8_t port_id,
			   struct rte_event events[], uint16_t nb_enq,
			   uint16_t nb_deq, uint8_t is_vector)
{
	int i;

	if (nb_deq) {
		if (is_vector)
			l2fwd_event_vector_array_free(events + nb_enq,
						      nb_deq - nb_enq);
		else
			for (i = nb_enq; i < nb_deq; i++)
				rte_pktmbuf_free(events[i].mbuf);

		for (i = 0; i < nb_deq; i++)
			events[i].op = RTE_EVENT_OP_RELEASE;
		rte_event_enqueue_burst(event_d_id, port_id, events, nb_deq);
	}

	rte_event_port_quiesce(event_d_id, port_id, l2fwd_event_port_flush,
			       NULL);
}
