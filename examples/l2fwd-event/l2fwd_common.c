/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "l2fwd_common.h"

int
l2fwd_event_init_ports(struct l2fwd_resources *rsrc)
{
	uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
	uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;
	struct rte_eth_conf port_conf = {
		.rxmode = {
			.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
			.split_hdr_size = 0,
		},
		.txmode = {
			.mq_mode = ETH_MQ_TX_NONE,
		},
	};
	uint16_t nb_ports_available = 0;
	uint16_t port_id;
	int ret;

	if (rsrc->event_mode) {
		port_conf.rxmode.mq_mode = ETH_MQ_RX_RSS;
		port_conf.rx_adv_conf.rss_conf.rss_key = NULL;
		port_conf.rx_adv_conf.rss_conf.rss_hf = ETH_RSS_IP;
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

		if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				DEV_TX_OFFLOAD_MBUF_FAST_FREE;
		ret = rte_eth_dev_configure(port_id, 1, 1, &local_port_conf);
		if (ret < 0)
			rte_panic("Cannot configure device: err=%d, port=%u\n",
				  ret, port_id);

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
		ret = rte_eth_rx_queue_setup(port_id, 0, nb_rxd,
					     rte_eth_dev_socket_id(port_id),
					     &rxq_conf,
					     rsrc->pktmbuf_pool);
		if (ret < 0)
			rte_panic("rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, port_id);

		/* init one TX queue on each port */
		fflush(stdout);
		txq_conf = dev_info.default_txconf;
		txq_conf.offloads = local_port_conf.txmode.offloads;
		ret = rte_eth_tx_queue_setup(port_id, 0, nb_txd,
				rte_eth_dev_socket_id(port_id),
				&txq_conf);
		if (ret < 0)
			rte_panic("rte_eth_tx_queue_setup:err=%d, port=%u\n",
				  ret, port_id);

		rte_eth_promiscuous_enable(port_id);

		printf("Port %u,MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
			port_id,
			rsrc->eth_addr[port_id].addr_bytes[0],
			rsrc->eth_addr[port_id].addr_bytes[1],
			rsrc->eth_addr[port_id].addr_bytes[2],
			rsrc->eth_addr[port_id].addr_bytes[3],
			rsrc->eth_addr[port_id].addr_bytes[4],
			rsrc->eth_addr[port_id].addr_bytes[5]);
	}

	return nb_ports_available;
}
