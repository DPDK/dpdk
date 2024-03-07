/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_ETHDEV_PRIV_H
#define APP_GRAPH_ETHDEV_PRIV_H

#include "ethdev.h"

#define NS_PER_SEC 1E9

#define ETHDEV_RXQ_RSS_MAX	16
#define ETHDEV_RX_DESC_DEFAULT 1024
#define ETHDEV_TX_DESC_DEFAULT 1024

struct ethdev_rss_config {
	uint32_t queue_id[ETHDEV_RXQ_RSS_MAX];
	uint32_t n_queues;
};

struct ethdev_config {
	char dev_name[RTE_ETH_NAME_MAX_LEN];
	uint16_t port_id;

	struct {
		uint32_t n_queues;
		uint32_t queue_size;
		char mempool_name[RTE_MEMPOOL_NAMESIZE];
		struct rte_mempool *mp;
		struct ethdev_rss_config *rss;
	} rx;

	struct {
		uint32_t n_queues;
		uint32_t queue_size;
	} tx;

	int promiscuous;
	uint32_t mtu;
};

struct ethdev {
	TAILQ_ENTRY(ethdev) next;
	uint16_t tx_port_id;
	struct ethdev_config config;
	struct ipv4_addr_config ip4_addr;
	struct ipv6_addr_config ip6_addr;
};
TAILQ_HEAD(ethdev_head, ethdev);
#endif
