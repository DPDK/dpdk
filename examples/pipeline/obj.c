/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_cryptodev.h>

#include "obj.h"

/*
 * ethdev
 */
static struct rte_eth_conf port_conf_default = {
	.link_speeds = 0,
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_NONE,
		.mtu = 9000 - (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN), /* Jumbo frame MTU */
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_key_len = 40,
			.rss_hf = 0,
		},
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
	.lpbk_mode = 0,
};

#define RETA_CONF_SIZE     (RTE_ETH_RSS_RETA_SIZE_512 / RTE_ETH_RETA_GROUP_SIZE)

static int
rss_setup(uint16_t port_id,
	uint16_t reta_size,
	struct ethdev_params_rss *rss)
{
	struct rte_eth_rss_reta_entry64 reta_conf[RETA_CONF_SIZE];
	uint32_t i;
	int status;

	/* RETA setting */
	memset(reta_conf, 0, sizeof(reta_conf));

	for (i = 0; i < reta_size; i++)
		reta_conf[i / RTE_ETH_RETA_GROUP_SIZE].mask = UINT64_MAX;

	for (i = 0; i < reta_size; i++) {
		uint32_t reta_id = i / RTE_ETH_RETA_GROUP_SIZE;
		uint32_t reta_pos = i % RTE_ETH_RETA_GROUP_SIZE;
		uint32_t rss_qs_pos = i % rss->n_queues;

		reta_conf[reta_id].reta[reta_pos] =
			(uint16_t) rss->queue_id[rss_qs_pos];
	}

	/* RETA update */
	status = rte_eth_dev_rss_reta_update(port_id,
		reta_conf,
		reta_size);

	return status;
}

int
ethdev_config(const char *name, struct ethdev_params *params)
{
	struct rte_eth_dev_info port_info;
	struct rte_eth_conf port_conf;
	struct ethdev_params_rss *rss;
	struct rte_mempool *mempool;
	uint32_t i;
	int numa_node, status;
	uint16_t port_id = 0;

	/* Check input params */
	if (!name ||
	    !name[0] ||
	    !params ||
	    !params->rx.n_queues ||
	    !params->rx.queue_size ||
	    !params->tx.n_queues ||
	    !params->tx.queue_size)
		return -EINVAL;

	status = rte_eth_dev_get_port_by_name(name, &port_id);
	if (status)
		return -EINVAL;

	status = rte_eth_dev_info_get(port_id, &port_info);
	if (status)
		return -EINVAL;

	mempool = rte_mempool_lookup(params->rx.mempool_name);
	if (!mempool)
		return -EINVAL;

	rss = params->rx.rss;
	if (rss) {
		if (!port_info.reta_size || port_info.reta_size > RTE_ETH_RSS_RETA_SIZE_512)
			return -EINVAL;

		if (!rss->n_queues || rss->n_queues >= ETHDEV_RXQ_RSS_MAX)
			return -EINVAL;

		for (i = 0; i < rss->n_queues; i++)
			if (rss->queue_id[i] >= port_info.max_rx_queues)
				return -EINVAL;
	}

	/* Port */
	memcpy(&port_conf, &port_conf_default, sizeof(port_conf));
	if (rss) {
		uint64_t rss_hf = RTE_ETH_RSS_IP | RTE_ETH_RSS_TCP | RTE_ETH_RSS_UDP;

		port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
		port_conf.rx_adv_conf.rss_conf.rss_hf = rss_hf & port_info.flow_type_rss_offloads;
	}

	numa_node = rte_eth_dev_socket_id(port_id);
	if (numa_node == SOCKET_ID_ANY)
		numa_node = 0;

	status = rte_eth_dev_configure(
		port_id,
		params->rx.n_queues,
		params->tx.n_queues,
		&port_conf);

	if (status < 0)
		return -EINVAL;

	if (params->promiscuous) {
		status = rte_eth_promiscuous_enable(port_id);
		if (status)
			return -EINVAL;
	}

	/* Port RX */
	for (i = 0; i < params->rx.n_queues; i++) {
		status = rte_eth_rx_queue_setup(
			port_id,
			i,
			params->rx.queue_size,
			numa_node,
			NULL,
			mempool);

		if (status < 0)
			return -EINVAL;
	}

	/* Port TX */
	for (i = 0; i < params->tx.n_queues; i++) {
		status = rte_eth_tx_queue_setup(
			port_id,
			i,
			params->tx.queue_size,
			numa_node,
			NULL);

		if (status < 0)
			return -EINVAL;
	}

	/* Port start */
	status = rte_eth_dev_start(port_id);
	if (status < 0)
		return -EINVAL;

	if (rss) {
		status = rss_setup(port_id, port_info.reta_size, rss);

		if (status) {
			rte_eth_dev_stop(port_id);
			return -EINVAL;
		}
	}

	/* Port link up */
	status = rte_eth_dev_set_link_up(port_id);
	if ((status < 0) && (status != -ENOTSUP)) {
		rte_eth_dev_stop(port_id);
		return -EINVAL;
	}

	return 0;
}

/*
 * cryptodev
 */
int
cryptodev_config(const char *name, struct cryptodev_params *params)
{
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_config dev_conf;
	struct rte_cryptodev_qp_conf queue_conf;
	uint8_t dev_id;
	uint32_t socket_id, i;
	int status;

	/* Check input parameters. */
	if (!name ||
	    !params->n_queue_pairs ||
	    !params->queue_size)
		return -EINVAL;

	/* Find the crypto device. */
	status = rte_cryptodev_get_dev_id(name);
	if (status < 0)
		return -EINVAL;

	dev_id = (uint8_t)status;

	rte_cryptodev_info_get(dev_id, &dev_info);
	if (params->n_queue_pairs > dev_info.max_nb_queue_pairs)
		return -EINVAL;

	socket_id = rte_cryptodev_socket_id(dev_id);

	/* Configure the crypto device. */
	memset(&dev_conf, 0, sizeof(dev_conf));
	dev_conf.socket_id = socket_id;
	dev_conf.nb_queue_pairs = params->n_queue_pairs;
	dev_conf.ff_disable = 0;

	status = rte_cryptodev_configure(dev_id, &dev_conf);
	if (status)
		return status;

	/* Configure the crypto device queue pairs. */
	memset(&queue_conf, 0, sizeof(queue_conf));
	queue_conf.nb_descriptors = params->queue_size;
	queue_conf.mp_session = NULL;

	for (i = 0; i < params->n_queue_pairs; i++) {
		status = rte_cryptodev_queue_pair_setup(dev_id, i, &queue_conf, socket_id);
		if (status)
			return status;
	}

	/* Start the crypto device. */
	status = rte_cryptodev_start(dev_id);
	if (status)
		return status;

	return 0;
}
