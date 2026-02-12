/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <sys/queue.h>

#include "testpmd.h"

/* Hairpin ports configuration mode. */
uint32_t hairpin_mode;

bool hairpin_multiport_mode;

queueid_t nb_hairpinq; /**< Number of hairpin queues per port. */

static LIST_HEAD(, hairpin_map) hairpin_map_head = LIST_HEAD_INITIALIZER();

struct hairpin_map {
	LIST_ENTRY(hairpin_map) entry; /**< List entry. */
	portid_t rx_port; /**< Hairpin Rx port ID. */
	portid_t tx_port; /**< Hairpin Tx port ID. */
	uint16_t rxq_head; /**< Hairpin Rx queue head. */
	uint16_t txq_head; /**< Hairpin Tx queue head. */
	uint16_t qnum; /**< Hairpin queues number. */
};

void
hairpin_add_multiport_map(struct hairpin_map *map)
{
	LIST_INSERT_HEAD(&hairpin_map_head, map, entry);
}

/*
 * Get the allowed maximum number of hairpin queues.
 * *pid return the port id which has minimal value of
 * max_hairpin_queues in all ports.
 */
queueid_t
get_allowed_max_nb_hairpinq(portid_t *pid)
{
	queueid_t allowed_max_hairpinq = RTE_MAX_QUEUES_PER_PORT;
	portid_t pi;
	struct rte_eth_hairpin_cap cap;

	RTE_ETH_FOREACH_DEV(pi) {
		if (rte_eth_dev_hairpin_capability_get(pi, &cap) != 0) {
			*pid = pi;
			return 0;
		}
		if (cap.max_nb_queues < allowed_max_hairpinq) {
			allowed_max_hairpinq = cap.max_nb_queues;
			*pid = pi;
		}
	}
	return allowed_max_hairpinq;
}

/*
 * Check input hairpin is valid or not.
 * If input hairpin is not greater than any of maximum number
 * of hairpin queues of all ports, it is valid.
 * if valid, return 0, else return -1
 */
int
check_nb_hairpinq(queueid_t hairpinq)
{
	queueid_t allowed_max_hairpinq;
	portid_t pid = 0;

	allowed_max_hairpinq = get_allowed_max_nb_hairpinq(&pid);
	if (hairpinq > allowed_max_hairpinq) {
		fprintf(stderr,
			"Fail: input hairpin (%u) can't be greater than max_hairpin_queues (%u) of port %u\n",
			hairpinq, allowed_max_hairpinq, pid);
		return -1;
	}
	return 0;
}

#define HAIRPIN_MODE_RX_FORCE_MEMORY RTE_BIT32(8)
#define HAIRPIN_MODE_TX_FORCE_MEMORY RTE_BIT32(9)

#define HAIRPIN_MODE_RX_LOCKED_MEMORY RTE_BIT32(12)
#define HAIRPIN_MODE_RX_RTE_MEMORY RTE_BIT32(13)

#define HAIRPIN_MODE_TX_LOCKED_MEMORY RTE_BIT32(16)
#define HAIRPIN_MODE_TX_RTE_MEMORY RTE_BIT32(17)

static int
port_config_hairpin_rxq(portid_t pi, uint16_t peer_tx_port,
			queueid_t rxq_head, queueid_t txq_head,
			uint16_t qcount, uint32_t manual_bind)
{
	int diag;
	queueid_t i, qi;
	struct rte_port *port = &ports[pi];
	struct rte_eth_hairpin_conf hairpin_conf = {
		.peer_count = 1,
		.peers[0].port = peer_tx_port,
		.manual_bind = manual_bind,
		.tx_explicit = !!(hairpin_mode & 0x10),
		.force_memory = !!(hairpin_mode & HAIRPIN_MODE_RX_FORCE_MEMORY),
		.use_locked_device_memory =
			!!(hairpin_mode & HAIRPIN_MODE_RX_LOCKED_MEMORY),
		.use_rte_memory = !!(hairpin_mode & HAIRPIN_MODE_RX_RTE_MEMORY),
	};

	for (qi = rxq_head, i = 0; qi < rxq_head + qcount; qi++, i++) {
		hairpin_conf.peers[0].queue = i + txq_head;
		diag = rte_eth_rx_hairpin_queue_setup(pi, qi, nb_rxd, &hairpin_conf);
		if (diag == 0)
			continue;

		/* Fail to setup rx queue, return */
		if (port->port_status == RTE_PORT_HANDLING)
			port->port_status = RTE_PORT_STOPPED;
		else
			fprintf(stderr,
				"Port %d can not be set back to stopped\n", pi);
		fprintf(stderr,
			"Port %u failed to configure hairpin on rxq %u.\n"
			"Peer port: %u peer txq: %u\n",
			pi, qi, peer_tx_port, i);
		/* try to reconfigure queues next time */
		port->need_reconfig_queues = 1;
		return -1;
	}
	return 0;
}

static int
port_config_hairpin_txq(portid_t pi, uint16_t peer_rx_port,
			queueid_t rxq_head, queueid_t txq_head,
			uint16_t qcount, uint32_t manual_bind)
{
	int diag;
	queueid_t i, qi;
	struct rte_port *port = &ports[pi];
	struct rte_eth_hairpin_conf hairpin_conf = {
		.peer_count = 1,
		.peers[0].port = peer_rx_port,
		.manual_bind = manual_bind,
		.tx_explicit = !!(hairpin_mode & 0x10),
		.force_memory = !!(hairpin_mode & HAIRPIN_MODE_TX_FORCE_MEMORY),
		.use_locked_device_memory =
			!!(hairpin_mode & HAIRPIN_MODE_TX_LOCKED_MEMORY),
		.use_rte_memory = !!(hairpin_mode & HAIRPIN_MODE_TX_RTE_MEMORY),
	};

	for (qi = txq_head, i = 0; qi < txq_head + qcount; qi++, i++) {
		hairpin_conf.peers[0].queue = i + rxq_head;
		diag = rte_eth_tx_hairpin_queue_setup(pi, qi, nb_txd, &hairpin_conf);
		if (diag == 0)
			continue;

		/* Fail to setup rx queue, return */
		if (port->port_status == RTE_PORT_HANDLING)
			port->port_status = RTE_PORT_STOPPED;
		else
			fprintf(stderr,
				"Port %d can not be set back to stopped\n", pi);
		fprintf(stderr,
			"Port %d failed to configure hairpin on txq %u.\n"
			"Peer port: %u peer rxq: %u\n",
			pi, qi, peer_rx_port, i);
		/* try to reconfigure queues next time */
		port->need_reconfig_queues = 1;
		return -1;
	}
	return 0;
}

static int
setup_legacy_hairpin_queues(portid_t pi, portid_t p_pi, uint16_t cnt_pi)
{
	int diag;
	uint16_t peer_rx_port = pi;
	uint16_t peer_tx_port = pi;
	uint32_t manual = 1;

	if (!(hairpin_mode & 0xf)) {
		peer_rx_port = pi;
		peer_tx_port = pi;
		manual = 0;
	} else if (hairpin_mode & 0x1) {
		peer_tx_port = rte_eth_find_next_owned_by(pi + 1,
							  RTE_ETH_DEV_NO_OWNER);
		if (peer_tx_port >= RTE_MAX_ETHPORTS)
			peer_tx_port = rte_eth_find_next_owned_by(0,
								  RTE_ETH_DEV_NO_OWNER);
		if (p_pi != RTE_MAX_ETHPORTS) {
			peer_rx_port = p_pi;
		} else {
			uint16_t next_pi;

			/* Last port will be the peer RX port of the first. */
			RTE_ETH_FOREACH_DEV(next_pi)
				peer_rx_port = next_pi;
		}
		manual = 1;
	} else if (hairpin_mode & 0x2) {
		if (cnt_pi & 0x1) {
			peer_rx_port = p_pi;
		} else {
			peer_rx_port = rte_eth_find_next_owned_by(pi + 1,
								  RTE_ETH_DEV_NO_OWNER);
			if (peer_rx_port >= RTE_MAX_ETHPORTS)
				peer_rx_port = pi;
		}
		peer_tx_port = peer_rx_port;
		manual = 1;
	}
	diag = port_config_hairpin_txq(pi, peer_rx_port, nb_rxq, nb_txq,
				       nb_hairpinq, manual);
	if (diag)
		return diag;
	diag = port_config_hairpin_rxq(pi, peer_tx_port, nb_rxq, nb_txq,
				       nb_hairpinq, manual);
	if (diag)
		return diag;
	return 0;
}

static int
setup_mapped_harpin_queues(portid_t pi)
{
	int ret = 0;
	struct hairpin_map *map;

	LIST_FOREACH(map, &hairpin_map_head, entry) {
		if (map->rx_port == pi) {
			ret = port_config_hairpin_rxq(pi, map->tx_port,
						      map->rxq_head,
						      map->txq_head,
						      map->qnum, true);
			if (ret)
				return ret;
		}
		if (map->tx_port == pi) {
			ret = port_config_hairpin_txq(pi, map->rx_port,
						      map->rxq_head,
						      map->txq_head,
						      map->qnum, true);
			if (ret)
				return ret;
		}
	}
	return 0;
}

/* Configure the Rx and Tx hairpin queues for the selected port. */
int
setup_hairpin_queues(portid_t pi, portid_t p_pi, uint16_t cnt_pi)
{
	if (hairpin_multiport_mode)
		return setup_mapped_harpin_queues(pi);

	return setup_legacy_hairpin_queues(pi, p_pi, cnt_pi);
}

int
hairpin_bind(uint16_t cfg_pi, portid_t *pl, portid_t *peer_pl)
{
	uint16_t i;
	portid_t pi;
	int peer_pi;
	int diag;
	int j;

	/* bind all started hairpin ports */
	for (i = 0; i < cfg_pi; i++) {
		pi = pl[i];
		/* bind current Tx to all peer Rx */
		peer_pi = rte_eth_hairpin_get_peer_ports(pi, peer_pl,
							 RTE_MAX_ETHPORTS, 1);
		if (peer_pi < 0)
			return peer_pi;
		for (j = 0; j < peer_pi; j++) {
			if (!port_is_started(peer_pl[j]))
				continue;
			diag = rte_eth_hairpin_bind(pi, peer_pl[j]);
			if (diag < 0) {
				fprintf(stderr,
					"Error during binding hairpin Tx port %u to %u: %s\n",
					pi, peer_pl[j],
					rte_strerror(-diag));
				return -1;
			}
		}
		/* bind all peer Tx to current Rx */
		peer_pi = rte_eth_hairpin_get_peer_ports(pi, peer_pl,
							 RTE_MAX_ETHPORTS, 0);
		if (peer_pi < 0)
			return peer_pi;
		for (j = 0; j < peer_pi; j++) {
			if (!port_is_started(peer_pl[j]))
				continue;
			diag = rte_eth_hairpin_bind(peer_pl[j], pi);
			if (diag < 0) {
				fprintf(stderr,
					"Error during binding hairpin Tx port %u to %u: %s\n",
					peer_pl[j], pi,
					rte_strerror(-diag));
				return -1;
			}
		}
	}
	return 0;
}

void
hairpin_map_usage(void)
{
	printf("  --hairpin-map=rxpi:rxq:txpi:txq:n: hairpin map.\n"
	       "    rxpi - Rx port index.\n"
	       "    rxq  - Rx queue.\n"
	       "    txpi - Tx port index.\n"
	       "    txq  - Tx queue.\n"
	       "    n    - hairpin queues number.\n");
}

int
parse_hairpin_map(const char *hpmap)
{
	/*
	 * Testpmd hairpin map format:
	 * <Rx port id:First Rx queue:Tx port id:First Tx queue:queues number>
	 */
	int ret;
	struct hairpin_map *map = calloc(1, sizeof(*map));

	if (!map)
		return -ENOMEM;

	ret = sscanf(hpmap, "%hu:%hu:%hu:%hu:%hu",
		     &map->rx_port, &map->rxq_head,
		     &map->tx_port, &map->txq_head, &map->qnum);
	if (ret != 5) {
		free(map);
		return -EINVAL;
	}
	hairpin_add_multiport_map(map);
	return 0;
}
