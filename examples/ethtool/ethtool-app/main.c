/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */


#include <stdio.h>
#include <stdlib.h>

#include <rte_common.h>
#include <rte_spinlock.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include "ethapp.h"

#define MAX_PORTS RTE_MAX_ETHPORTS
#define MAX_BURST_LENGTH 32
#define PORT_RX_QUEUE_SIZE 1024
#define PORT_TX_QUEUE_SIZE 1024
#define PKTPOOL_EXTRA_SIZE 512
#define PKTPOOL_CACHE 32


struct txq_port {
	uint16_t cnt_unsent;
	struct rte_mbuf *buf_frames[MAX_BURST_LENGTH];
};

struct app_port {
	struct ether_addr mac_addr;
	struct txq_port txq;
	rte_spinlock_t lock;
	int port_active;
	int port_dirty;
	int idx_port;
	struct rte_mempool *pkt_pool;
};

struct app_config {
	struct app_port ports[MAX_PORTS];
	int cnt_ports;
	int exit_now;
};


struct app_config app_cfg;


void lock_port(int idx_port)
{
	struct app_port *ptr_port = &app_cfg.ports[idx_port];

	rte_spinlock_lock(&ptr_port->lock);
}

void unlock_port(int idx_port)
{
	struct app_port *ptr_port = &app_cfg.ports[idx_port];

	rte_spinlock_unlock(&ptr_port->lock);
}

void mark_port_active(int idx_port)
{
	struct app_port *ptr_port = &app_cfg.ports[idx_port];

	ptr_port->port_active = 1;
}

void mark_port_inactive(int idx_port)
{
	struct app_port *ptr_port = &app_cfg.ports[idx_port];

	ptr_port->port_active = 0;
}

void mark_port_newmac(int idx_port)
{
	struct app_port *ptr_port = &app_cfg.ports[idx_port];

	ptr_port->port_dirty = 1;
}

static void setup_ports(struct app_config *app_cfg, int cnt_ports)
{
	int idx_port;
	int size_pktpool;
	struct rte_eth_conf cfg_port;
	struct rte_eth_dev_info dev_info;
	char str_name[16];
	uint16_t nb_rxd = PORT_RX_QUEUE_SIZE;
	uint16_t nb_txd = PORT_TX_QUEUE_SIZE;
	struct rte_eth_txconf txconf;

	memset(&cfg_port, 0, sizeof(cfg_port));
	cfg_port.txmode.mq_mode = ETH_MQ_TX_NONE;
	cfg_port.rxmode.ignore_offload_bitfield = 1;

	for (idx_port = 0; idx_port < cnt_ports; idx_port++) {
		struct app_port *ptr_port = &app_cfg->ports[idx_port];

		rte_eth_dev_info_get(idx_port, &dev_info);
		size_pktpool = dev_info.rx_desc_lim.nb_max +
			dev_info.tx_desc_lim.nb_max + PKTPOOL_EXTRA_SIZE;

		snprintf(str_name, 16, "pkt_pool%i", idx_port);
		ptr_port->pkt_pool = rte_pktmbuf_pool_create(
			str_name,
			size_pktpool, PKTPOOL_CACHE,
			0,
			RTE_MBUF_DEFAULT_BUF_SIZE,
			rte_socket_id()
			);
		if (ptr_port->pkt_pool == NULL)
			rte_exit(EXIT_FAILURE,
				"rte_pktmbuf_pool_create failed"
				);

		printf("Init port %i..\n", idx_port);
		ptr_port->port_active = 1;
		ptr_port->port_dirty = 0;
		ptr_port->idx_port = idx_port;

		if (rte_eth_dev_configure(idx_port, 1, 1, &cfg_port) < 0)
			rte_exit(EXIT_FAILURE,
				 "rte_eth_dev_configure failed");
		if (rte_eth_dev_adjust_nb_rx_tx_desc(idx_port, &nb_rxd,
						     &nb_txd) < 0)
			rte_exit(EXIT_FAILURE,
				 "rte_eth_dev_adjust_nb_rx_tx_desc failed");

		if (rte_eth_rx_queue_setup(
			    idx_port, 0, nb_rxd,
			    rte_eth_dev_socket_id(idx_port), NULL,
			    ptr_port->pkt_pool) < 0)
			rte_exit(EXIT_FAILURE,
				 "rte_eth_rx_queue_setup failed"
				);
		txconf = dev_info.default_txconf;
		txconf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
		if (rte_eth_tx_queue_setup(
			    idx_port, 0, nb_txd,
			    rte_eth_dev_socket_id(idx_port), &txconf) < 0)
			rte_exit(EXIT_FAILURE,
				 "rte_eth_tx_queue_setup failed"
				);
		if (rte_eth_dev_start(idx_port) < 0)
			rte_exit(EXIT_FAILURE,
				 "%s:%i: rte_eth_dev_start failed",
				 __FILE__, __LINE__
				);
		rte_eth_macaddr_get(idx_port, &ptr_port->mac_addr);
		rte_spinlock_init(&ptr_port->lock);
	}
}

static void process_frame(struct app_port *ptr_port,
	struct rte_mbuf *ptr_frame)
{
	struct ether_hdr *ptr_mac_hdr;

	ptr_mac_hdr = rte_pktmbuf_mtod(ptr_frame, struct ether_hdr *);
	ether_addr_copy(&ptr_mac_hdr->s_addr, &ptr_mac_hdr->d_addr);
	ether_addr_copy(&ptr_port->mac_addr, &ptr_mac_hdr->s_addr);
}

static int slave_main(__attribute__((unused)) void *ptr_data)
{
	struct app_port *ptr_port;
	struct rte_mbuf *ptr_frame;
	struct txq_port *txq;

	uint16_t cnt_recv_frames;
	uint16_t idx_frame;
	uint16_t cnt_sent;
	uint16_t idx_port;
	uint16_t lock_result;

	while (app_cfg.exit_now == 0) {
		for (idx_port = 0; idx_port < app_cfg.cnt_ports; idx_port++) {
			/* Check that port is active and unlocked */
			ptr_port = &app_cfg.ports[idx_port];
			lock_result = rte_spinlock_trylock(&ptr_port->lock);
			if (lock_result == 0)
				continue;
			if (ptr_port->port_active == 0) {
				rte_spinlock_unlock(&ptr_port->lock);
				continue;
			}
			txq = &ptr_port->txq;

			/* MAC address was updated */
			if (ptr_port->port_dirty == 1) {
				rte_eth_macaddr_get(ptr_port->idx_port,
					&ptr_port->mac_addr);
				ptr_port->port_dirty = 0;
			}

			/* Incoming frames */
			cnt_recv_frames = rte_eth_rx_burst(
				ptr_port->idx_port, 0,
				&txq->buf_frames[txq->cnt_unsent],
				RTE_DIM(txq->buf_frames) - txq->cnt_unsent
				);
			if (cnt_recv_frames > 0) {
				for (idx_frame = 0;
					idx_frame < cnt_recv_frames;
					idx_frame++) {
					ptr_frame = txq->buf_frames[
						idx_frame + txq->cnt_unsent];
					process_frame(ptr_port, ptr_frame);
				}
				txq->cnt_unsent += cnt_recv_frames;
			}

			/* Outgoing frames */
			if (txq->cnt_unsent > 0) {
				cnt_sent = rte_eth_tx_burst(
					ptr_port->idx_port, 0,
					txq->buf_frames,
					txq->cnt_unsent
					);
				/* Shuffle up unsent frame pointers */
				for (idx_frame = cnt_sent;
					idx_frame < txq->cnt_unsent;
					idx_frame++)
					txq->buf_frames[idx_frame - cnt_sent] =
						txq->buf_frames[idx_frame];
				txq->cnt_unsent -= cnt_sent;
			}
			rte_spinlock_unlock(&ptr_port->lock);
		} /* end for( idx_port ) */
	} /* end for(;;) */

	return 0;
}

int main(int argc, char **argv)
{
	int cnt_args_parsed;
	uint32_t id_core;
	uint32_t cnt_ports;

	/* Init runtime environment */
	cnt_args_parsed = rte_eal_init(argc, argv);
	if (cnt_args_parsed < 0)
		rte_exit(EXIT_FAILURE, "rte_eal_init(): Failed");

	cnt_ports = rte_eth_dev_count_avail();
	printf("Number of NICs: %i\n", cnt_ports);
	if (cnt_ports == 0)
		rte_exit(EXIT_FAILURE, "No available NIC ports!\n");
	if (cnt_ports > MAX_PORTS) {
		printf("Info: Using only %i of %i ports\n",
			cnt_ports, MAX_PORTS
			);
		cnt_ports = MAX_PORTS;
	}

	setup_ports(&app_cfg, cnt_ports);

	app_cfg.exit_now = 0;
	app_cfg.cnt_ports = cnt_ports;

	if (rte_lcore_count() < 2)
		rte_exit(EXIT_FAILURE, "No available slave core!\n");
	/* Assume there is an available slave.. */
	id_core = rte_lcore_id();
	id_core = rte_get_next_lcore(id_core, 1, 1);
	rte_eal_remote_launch(slave_main, NULL, id_core);

	ethapp_main();

	app_cfg.exit_now = 1;
	RTE_LCORE_FOREACH_SLAVE(id_core) {
		if (rte_eal_wait_lcore(id_core) < 0)
			return -1;
	}

	return 0;
}
