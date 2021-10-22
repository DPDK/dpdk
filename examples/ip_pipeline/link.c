/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "link.h"
#include "mempool.h"

static struct link_list link_list;

int
link_init(void)
{
	TAILQ_INIT(&link_list);

	return 0;
}

struct link *
link_find(const char *name)
{
	struct link *link;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(link, &link_list, node)
		if (strcmp(link->name, name) == 0)
			return link;

	return NULL;
}

struct link *
link_next(struct link *link)
{
	return (link == NULL) ? TAILQ_FIRST(&link_list) : TAILQ_NEXT(link, node);
}

static struct rte_eth_conf port_conf_default = {
	.link_speeds = 0,
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_NONE,
		.mtu = 9000 - (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN), /* Jumbo frame MTU */
		.split_hdr_size = 0, /* Header split buffer size */
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
	struct link_params_rss *rss)
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

struct link *
link_create(const char *name, struct link_params *params)
{
	struct rte_eth_dev_info port_info;
	struct rte_eth_conf port_conf;
	struct link *link;
	struct link_params_rss *rss;
	struct mempool *mempool;
	uint32_t cpu_id, i;
	int status;
	uint16_t port_id;

	/* Check input params */
	if ((name == NULL) ||
		link_find(name) ||
		(params == NULL) ||
		(params->rx.n_queues == 0) ||
		(params->rx.queue_size == 0) ||
		(params->tx.n_queues == 0) ||
		(params->tx.queue_size == 0))
		return NULL;

	port_id = params->port_id;
	if (params->dev_name) {
		status = rte_eth_dev_get_port_by_name(params->dev_name,
			&port_id);

		if (status)
			return NULL;
	} else
		if (!rte_eth_dev_is_valid_port(port_id))
			return NULL;

	if (rte_eth_dev_info_get(port_id, &port_info) != 0)
		return NULL;

	mempool = mempool_find(params->rx.mempool_name);
	if (mempool == NULL)
		return NULL;

	rss = params->rx.rss;
	if (rss) {
		if ((port_info.reta_size == 0) ||
			(port_info.reta_size > RTE_ETH_RSS_RETA_SIZE_512))
			return NULL;

		if ((rss->n_queues == 0) ||
			(rss->n_queues >= LINK_RXQ_RSS_MAX))
			return NULL;

		for (i = 0; i < rss->n_queues; i++)
			if (rss->queue_id[i] >= port_info.max_rx_queues)
				return NULL;
	}

	/**
	 * Resource create
	 */
	/* Port */
	memcpy(&port_conf, &port_conf_default, sizeof(port_conf));
	if (rss) {
		port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
		port_conf.rx_adv_conf.rss_conf.rss_hf =
			(RTE_ETH_RSS_IP | RTE_ETH_RSS_TCP | RTE_ETH_RSS_UDP) &
			port_info.flow_type_rss_offloads;
	}

	cpu_id = (uint32_t) rte_eth_dev_socket_id(port_id);
	if (cpu_id == (uint32_t) SOCKET_ID_ANY)
		cpu_id = 0;

	status = rte_eth_dev_configure(
		port_id,
		params->rx.n_queues,
		params->tx.n_queues,
		&port_conf);

	if (status < 0)
		return NULL;

	if (params->promiscuous) {
		status = rte_eth_promiscuous_enable(port_id);
		if (status != 0)
			return NULL;
	}

	/* Port RX */
	for (i = 0; i < params->rx.n_queues; i++) {
		status = rte_eth_rx_queue_setup(
			port_id,
			i,
			params->rx.queue_size,
			cpu_id,
			NULL,
			mempool->m);

		if (status < 0)
			return NULL;
	}

	/* Port TX */
	for (i = 0; i < params->tx.n_queues; i++) {
		status = rte_eth_tx_queue_setup(
			port_id,
			i,
			params->tx.queue_size,
			cpu_id,
			NULL);

		if (status < 0)
			return NULL;
	}

	/* Port start */
	status = rte_eth_dev_start(port_id);
	if (status < 0)
		return NULL;

	if (rss) {
		status = rss_setup(port_id, port_info.reta_size, rss);

		if (status) {
			rte_eth_dev_stop(port_id);
			return NULL;
		}
	}

	/* Port link up */
	status = rte_eth_dev_set_link_up(port_id);
	if ((status < 0) && (status != -ENOTSUP)) {
		rte_eth_dev_stop(port_id);
		return NULL;
	}

	/* Node allocation */
	link = calloc(1, sizeof(struct link));
	if (link == NULL) {
		rte_eth_dev_stop(port_id);
		return NULL;
	}

	/* Node fill in */
	strlcpy(link->name, name, sizeof(link->name));
	link->port_id = port_id;
	link->n_rxq = params->rx.n_queues;
	link->n_txq = params->tx.n_queues;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&link_list, link, node);

	return link;
}

int
link_is_up(const char *name)
{
	struct rte_eth_link link_params;
	struct link *link;

	/* Check input params */
	if (name == NULL)
		return 0;

	link = link_find(name);
	if (link == NULL)
		return 0;

	/* Resource */
	if (rte_eth_link_get(link->port_id, &link_params) < 0)
		return 0;

	return (link_params.link_status == RTE_ETH_LINK_DOWN) ? 0 : 1;
}
