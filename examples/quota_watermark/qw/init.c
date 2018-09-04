/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <rte_eal.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_memzone.h>
#include <rte_ring.h>
#include <rte_string_fns.h>

#include "args.h"
#include "init.h"
#include "main.h"
#include "../include/conf.h"


static struct rte_eth_conf port_conf = {
		.rxmode = {
			.split_hdr_size = 0,
		},
		.txmode = {
			.mq_mode = ETH_DCB_NONE,
		},
};

static struct rte_eth_fc_conf fc_conf = {
		.mode       = RTE_FC_TX_PAUSE,
		.high_water = 80 * 510 / 100,
		.low_water  = 60 * 510 / 100,
		.pause_time = 1337,
		.send_xon   = 0,
};


void configure_eth_port(uint16_t port_id)
{
	int ret;
	uint16_t nb_rxd = RX_DESC_PER_QUEUE;
	uint16_t nb_txd = TX_DESC_PER_QUEUE;
	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_txconf txq_conf;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_conf local_port_conf = port_conf;

	rte_eth_dev_stop(port_id);

	rte_eth_dev_info_get(port_id, &dev_info);
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		local_port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	ret = rte_eth_dev_configure(port_id, 1, 1, &local_port_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot configure port %u (error %d)\n",
				(unsigned int) port_id, ret);

	ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd, &nb_txd);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
				"Cannot adjust number of descriptors for port %u (error %d)\n",
				(unsigned int) port_id, ret);

	/* Initialize the port's RX queue */
	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = local_port_conf.rxmode.offloads;
	ret = rte_eth_rx_queue_setup(port_id, 0, nb_rxd,
			rte_eth_dev_socket_id(port_id),
			&rxq_conf,
			mbuf_pool);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
				"Failed to setup RX queue on port %u (error %d)\n",
				(unsigned int) port_id, ret);

	/* Initialize the port's TX queue */
	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = local_port_conf.txmode.offloads;
	ret = rte_eth_tx_queue_setup(port_id, 0, nb_txd,
			rte_eth_dev_socket_id(port_id),
			&txq_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
				"Failed to setup TX queue on port %u (error %d)\n",
				(unsigned int) port_id, ret);

	/* Initialize the port's flow control */
	ret = rte_eth_dev_flow_ctrl_set(port_id, &fc_conf);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
				"Failed to setup hardware flow control on port %u (error %d)\n",
				(unsigned int) port_id, ret);

	/* Start the port */
	ret = rte_eth_dev_start(port_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Failed to start port %u (error %d)\n",
				(unsigned int) port_id, ret);

	/* Put it in promiscuous mode */
	rte_eth_promiscuous_enable(port_id);
}

void
init_dpdk(void)
{
	if (rte_eth_dev_count_avail() < 2)
		rte_exit(EXIT_FAILURE, "Not enough ethernet port available\n");
}

void init_ring(int lcore_id, uint16_t port_id)
{
	struct rte_ring *ring;
	char ring_name[RTE_RING_NAMESIZE];

	snprintf(ring_name, RTE_RING_NAMESIZE,
			"core%d_port%d", lcore_id, port_id);
	ring = rte_ring_create(ring_name, RING_SIZE, rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);

	if (ring == NULL)
		rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));

	*high_watermark = 80 * RING_SIZE / 100;

	rings[lcore_id][port_id] = ring;
}

void
pair_ports(void)
{
	uint16_t i, j;

	/* Pair ports with their "closest neighbour" in the portmask */
	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		if (is_bit_set(i, portmask))
			for (j = i + 1; j < RTE_MAX_ETHPORTS; j++)
				if (is_bit_set(j, portmask)) {
					port_pairs[i] = j;
					port_pairs[j] = i;
					i = j;
					break;
				}
}

void
setup_shared_variables(void)
{
	const struct rte_memzone *qw_memzone;

	qw_memzone = rte_memzone_reserve(QUOTA_WATERMARK_MEMZONE_NAME,
			3 * sizeof(int), rte_socket_id(), 0);
	if (qw_memzone == NULL)
		rte_exit(EXIT_FAILURE, "%s\n", rte_strerror(rte_errno));

	quota = qw_memzone->addr;
	low_watermark = (unsigned int *) qw_memzone->addr + 1;
	high_watermark = (unsigned int *) qw_memzone->addr + 2;
}
