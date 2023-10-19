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

struct ethdev_show_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t dev;
	cmdline_fixed_string_t show;
};

struct ethdev_stats_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t dev;
	cmdline_fixed_string_t stats;
};

struct ethdev_mtu_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t dev;
	cmdline_fixed_string_t mtu;
	uint16_t size;
};

struct ethdev_prom_mode_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t dev;
	cmdline_fixed_string_t prom;
	cmdline_fixed_string_t enable;
};

struct ethdev_ip4_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t dev;
	cmdline_fixed_string_t ip4;
	cmdline_fixed_string_t addr;
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t ip;
	cmdline_fixed_string_t netmask;
	cmdline_fixed_string_t mask;
};

struct ethdev_ip6_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t dev;
	cmdline_fixed_string_t ip6;
	cmdline_fixed_string_t addr;
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t ip;
	cmdline_fixed_string_t netmask;
	cmdline_fixed_string_t mask;
};

struct ethdev_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t dev;
	cmdline_fixed_string_t rxq;
	cmdline_fixed_string_t txq;
	cmdline_fixed_string_t mempool;
	uint16_t nb_rxq;
	uint16_t nb_txq;
};

struct ethdev_help_cmd_tokens {
	cmdline_fixed_string_t help;
	cmdline_fixed_string_t ethdev;
};

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
	struct ethdev_config config;
	struct ipv4_addr_config ip4_addr;
	struct ipv6_addr_config ip6_addr;
};
TAILQ_HEAD(ethdev_head, ethdev);
#endif
