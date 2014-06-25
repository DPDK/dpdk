/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_memory.h>

#include "virtual_pmd.h"

#define MAX_PKT_BURST 512

static const char *virtual_ethdev_driver_name = "Virtual PMD";

struct virtual_ethdev_private {
	struct rte_eth_stats eth_stats;

	struct rte_mbuf *rx_pkt_burst[MAX_PKT_BURST];
	int rx_pkt_burst_len;
};

struct virtual_ethdev_queue {
	int port_id;
	int queue_id;
};

static int
virtual_ethdev_start_success(struct rte_eth_dev *eth_dev __rte_unused)
{
	eth_dev->data->dev_started = 1;

	return 0;
}

static int
virtual_ethdev_start_fail(struct rte_eth_dev *eth_dev __rte_unused)
{
	eth_dev->data->dev_started = 0;

	return -1;
}
static void  virtual_ethdev_stop(struct rte_eth_dev *eth_dev __rte_unused)
{
	eth_dev->data->dev_link.link_status = 0;
	eth_dev->data->dev_started = 0;
}

static void
virtual_ethdev_close(struct rte_eth_dev *dev __rte_unused)
{}

static int
virtual_ethdev_configure_success(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int
virtual_ethdev_configure_fail(struct rte_eth_dev *dev __rte_unused)
{
	return -1;
}

static void
virtual_ethdev_info_get(struct rte_eth_dev *dev __rte_unused,
		struct rte_eth_dev_info *dev_info)
{
	dev_info->driver_name = virtual_ethdev_driver_name;
	dev_info->max_mac_addrs = 1;

	dev_info->max_rx_pktlen = (uint32_t)2048;

	dev_info->max_rx_queues = (uint16_t)128;
	dev_info->max_tx_queues = (uint16_t)512;

	dev_info->min_rx_bufsize = 0;
	dev_info->pci_dev = NULL;
}

static int
virtual_ethdev_rx_queue_setup_success(struct rte_eth_dev *dev,
		uint16_t rx_queue_id, uint16_t nb_rx_desc __rte_unused,
		unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf __rte_unused,
		struct rte_mempool *mb_pool __rte_unused)
{
	struct virtual_ethdev_queue *rx_q;

	rx_q = (struct virtual_ethdev_queue *)rte_zmalloc_socket(NULL,
			sizeof(struct virtual_ethdev_queue), 0, socket_id);

	if (rx_q == NULL)
		return -1;

	rx_q->port_id = dev->data->port_id;
	rx_q->queue_id = rx_queue_id;

	dev->data->rx_queues[rx_queue_id] = rx_q;

	return 0;
}

static int
virtual_ethdev_rx_queue_setup_fail(struct rte_eth_dev *dev __rte_unused,
		uint16_t rx_queue_id __rte_unused, uint16_t nb_rx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_rxconf *rx_conf __rte_unused,
		struct rte_mempool *mb_pool __rte_unused)
{
	return -1;
}

static int
virtual_ethdev_tx_queue_setup_success(struct rte_eth_dev *dev,
		uint16_t tx_queue_id, uint16_t nb_tx_desc __rte_unused,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct virtual_ethdev_queue *tx_q;

	tx_q = (struct virtual_ethdev_queue *)rte_zmalloc_socket(NULL,
			sizeof(struct virtual_ethdev_queue), 0, socket_id);

	if (tx_q == NULL)
		return -1;

	tx_q->port_id = dev->data->port_id;
	tx_q->queue_id = tx_queue_id;

	dev->data->tx_queues[tx_queue_id] = tx_q;

	return 0;
}

static int
virtual_ethdev_tx_queue_setup_fail(struct rte_eth_dev *dev __rte_unused,
		uint16_t tx_queue_id __rte_unused, uint16_t nb_tx_desc __rte_unused,
		unsigned int socket_id __rte_unused,
		const struct rte_eth_txconf *tx_conf __rte_unused)
{
	return -1;
}

static void
virtual_ethdev_rx_queue_release(void *q __rte_unused)
{
}

static void
virtual_ethdev_tx_queue_release(void *q __rte_unused)
{
}

static int
virtual_ethdev_link_update_success(struct rte_eth_dev *bonded_eth_dev,
		int wait_to_complete __rte_unused)
{
	if (!bonded_eth_dev->data->dev_started)
		bonded_eth_dev->data->dev_link.link_status = 0;

	return 0;
}

static int
virtual_ethdev_link_update_fail(struct rte_eth_dev *bonded_eth_dev __rte_unused,
		int wait_to_complete __rte_unused)
{
	return -1;
}

static void
virtual_ethdev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct virtual_ethdev_private *dev_private = dev->data->dev_private;

	if (stats)
		rte_memcpy(stats, &dev_private->eth_stats, sizeof(*stats));
}

static void
virtual_ethdev_stats_reset(struct rte_eth_dev *dev)
{
	struct virtual_ethdev_private *dev_private = dev->data->dev_private;

	dev_private->rx_pkt_burst_len = 0;

	/* Reset internal statistics */
	memset(&dev_private->eth_stats, 0, sizeof(dev_private->eth_stats));
}

static void
virtual_ethdev_promiscuous_mode_enable(struct rte_eth_dev *dev __rte_unused)
{}

static void
virtual_ethdev_promiscuous_mode_disable(struct rte_eth_dev *dev __rte_unused)
{}


static struct eth_dev_ops virtual_ethdev_default_dev_ops = {
		.dev_configure = virtual_ethdev_configure_success,
		.dev_start = virtual_ethdev_start_success,
		.dev_stop = virtual_ethdev_stop,
		.dev_close = virtual_ethdev_close,
		.dev_infos_get = virtual_ethdev_info_get,
		.rx_queue_setup = virtual_ethdev_rx_queue_setup_success,
		.tx_queue_setup = virtual_ethdev_tx_queue_setup_success,
		.rx_queue_release = virtual_ethdev_rx_queue_release,
		.tx_queue_release = virtual_ethdev_tx_queue_release,
		.link_update = virtual_ethdev_link_update_success,
		.stats_get = virtual_ethdev_stats_get,
		.stats_reset = virtual_ethdev_stats_reset,
		.promiscuous_enable = virtual_ethdev_promiscuous_mode_enable,
		.promiscuous_disable = virtual_ethdev_promiscuous_mode_disable
};


void
virtual_ethdev_start_fn_set_success(uint8_t port_id, uint8_t success)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	if (success)
		vrtl_eth_dev->dev_ops->dev_start = virtual_ethdev_start_success;
	else
		vrtl_eth_dev->dev_ops->dev_start = virtual_ethdev_start_fail;

}

void
virtual_ethdev_configure_fn_set_success(uint8_t port_id, uint8_t success)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	if (success)
		vrtl_eth_dev->dev_ops->dev_configure = virtual_ethdev_configure_success;
	else
		vrtl_eth_dev->dev_ops->dev_configure = virtual_ethdev_configure_fail;
}

void
virtual_ethdev_rx_queue_setup_fn_set_success(uint8_t port_id, uint8_t success)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	if (success)
		vrtl_eth_dev->dev_ops->rx_queue_setup =
				virtual_ethdev_rx_queue_setup_success;
	else
		vrtl_eth_dev->dev_ops->rx_queue_setup =
				virtual_ethdev_rx_queue_setup_fail;
}

void
virtual_ethdev_tx_queue_setup_fn_set_success(uint8_t port_id, uint8_t success)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	if (success)
		vrtl_eth_dev->dev_ops->tx_queue_setup =
				virtual_ethdev_tx_queue_setup_success;
	else
		vrtl_eth_dev->dev_ops->tx_queue_setup =
				virtual_ethdev_tx_queue_setup_fail;
}

void
virtual_ethdev_link_update_fn_set_success(uint8_t port_id, uint8_t success)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	if (success)
		vrtl_eth_dev->dev_ops->link_update = virtual_ethdev_link_update_success;
	else
		vrtl_eth_dev->dev_ops->link_update = virtual_ethdev_link_update_fail;
}


static uint16_t
virtual_ethdev_rx_burst_success(void *queue __rte_unused,
							 struct rte_mbuf **bufs,
							 uint16_t nb_pkts)
{
	struct rte_eth_dev *vrtl_eth_dev;
	struct virtual_ethdev_queue *pq_map;
	struct virtual_ethdev_private *dev_private;

	int i;

	pq_map = (struct virtual_ethdev_queue *)queue;

	vrtl_eth_dev = &rte_eth_devices[pq_map->port_id];

	dev_private = vrtl_eth_dev->data->dev_private;

	if (dev_private->rx_pkt_burst_len > 0) {
		if (dev_private->rx_pkt_burst_len < nb_pkts) {

			for (i = 0; i < dev_private->rx_pkt_burst_len; i++) {
				bufs[i] = dev_private->rx_pkt_burst[i];
				dev_private->rx_pkt_burst[i] = NULL;
			}

			dev_private->eth_stats.ipackets = dev_private->rx_pkt_burst_len;
		}
		/* reset private burst values */
		dev_private->rx_pkt_burst_len = 0;
	}

	return dev_private->eth_stats.ipackets;
}

static uint16_t
virtual_ethdev_rx_burst_fail(void *queue __rte_unused,
							 struct rte_mbuf **bufs __rte_unused,
							 uint16_t nb_pkts __rte_unused)
{
	return 0;
}

static uint16_t
virtual_ethdev_tx_burst_success(void *queue,
							 struct rte_mbuf **bufs __rte_unused,
							 uint16_t nb_pkts)
{
	struct rte_eth_dev *vrtl_eth_dev;
	struct virtual_ethdev_queue *tx_q;
	struct virtual_ethdev_private *dev_private;
	int i;

	tx_q = (struct virtual_ethdev_queue *)queue;

	vrtl_eth_dev = &rte_eth_devices[tx_q->port_id];

	if (vrtl_eth_dev->data->dev_link.link_status) {
		dev_private = vrtl_eth_dev->data->dev_private;
		dev_private->eth_stats.opackets += nb_pkts;

		return nb_pkts;
	}

	/* free packets in burst */
	for (i = 0; i < nb_pkts; i++) {
		if (bufs[i] != NULL)
			rte_pktmbuf_free(bufs[i]);

		bufs[i] = NULL;
	}

	return 0;
}


static uint16_t
virtual_ethdev_tx_burst_fail(void *queue __rte_unused,
		struct rte_mbuf **bufs __rte_unused, uint16_t nb_pkts __rte_unused)
{
	return 0;
}


void
virtual_ethdev_rx_burst_fn_set_success(uint8_t port_id, uint8_t success)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	if (success)
		vrtl_eth_dev->rx_pkt_burst = virtual_ethdev_rx_burst_success;
	else
		vrtl_eth_dev->rx_pkt_burst = virtual_ethdev_rx_burst_fail;
}


void
virtual_ethdev_tx_burst_fn_set_success(uint8_t port_id, uint8_t success)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	if (success)
		vrtl_eth_dev->tx_pkt_burst = virtual_ethdev_tx_burst_success;
	else
		vrtl_eth_dev->tx_pkt_burst = virtual_ethdev_tx_burst_fail;
}


void
virtual_ethdev_simulate_link_status_interrupt(uint8_t port_id, uint8_t link_status)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	vrtl_eth_dev->data->dev_link.link_status = link_status;

	_rte_eth_dev_callback_process(vrtl_eth_dev, RTE_ETH_EVENT_INTR_LSC);
}



void
virtual_ethdev_add_mbufs_to_rx_queue(uint8_t port_id,
		struct rte_mbuf **pkt_burst, int burst_length)
{
	struct virtual_ethdev_private *dev_private = NULL;
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	int i;

	dev_private = vrtl_eth_dev->data->dev_private;

	for (i = 0; i < burst_length; i++)
		dev_private->rx_pkt_burst[i] = pkt_burst[i];

	dev_private->rx_pkt_burst_len = burst_length;
}

static uint8_t
get_number_of_sockets(void)
{
	int sockets = 0;
	int i;
	const struct rte_memseg *ms = rte_eal_get_physmem_layout();

	for (i = 0; i < RTE_MAX_MEMSEG && ms[i].addr != NULL; i++) {
		if (sockets < ms[i].socket_id)
			sockets = ms[i].socket_id;
	}
	/* Number of sockets = maximum socket_id + 1 */
	return ++sockets;
}


int
virtual_ethdev_create(const char *name, struct ether_addr *mac_addr,
		uint8_t socket_id)
{
	struct rte_pci_device *pci_dev = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct eth_driver *eth_drv = NULL;
	struct rte_pci_driver *pci_drv = NULL;
	struct eth_dev_ops *dev_ops = NULL;
	struct rte_pci_id *id_table = NULL;
	struct virtual_ethdev_private *dev_private = NULL;


	/* now do all data allocation - for eth_dev structure, dummy pci driver
	 * and internal (dev_private) data
	 */

	if (socket_id >= get_number_of_sockets())
		goto err;

	pci_dev = rte_zmalloc_socket(name, sizeof(*pci_dev), 0, socket_id);
	if (pci_dev == NULL)
		goto err;

	eth_drv = rte_zmalloc_socket(name, sizeof(*eth_drv), 0, socket_id);
	if (eth_drv == NULL)
		goto err;

	pci_drv = rte_zmalloc_socket(name, sizeof(*pci_drv), 0, socket_id);
	if (pci_drv == NULL)
		goto err;

	dev_ops = rte_zmalloc_socket(name, sizeof(*dev_ops), 0, socket_id);
	if (dev_ops == NULL)
		goto err;

	id_table = rte_zmalloc_socket(name, sizeof(*id_table), 0, socket_id);
	if (id_table == NULL)
		goto err;

	dev_private = rte_zmalloc_socket(name, sizeof(*dev_private), 0, socket_id);
	if (dev_private == NULL)
		goto err;

	/* reserve an ethdev entry */
	eth_dev = rte_eth_dev_allocate(name);
	if (eth_dev == NULL)
		goto err;

	pci_dev->numa_node = socket_id;
	pci_drv->name = virtual_ethdev_driver_name;
	pci_drv->id_table = id_table;

	eth_drv->pci_drv = (struct rte_pci_driver)(*pci_drv);
	eth_dev->driver = eth_drv;

	eth_dev->data->nb_rx_queues = (uint16_t)1;
	eth_dev->data->nb_tx_queues = (uint16_t)1;

	TAILQ_INIT(&(eth_dev->callbacks));

	eth_dev->data->dev_link.link_status = 0;
	eth_dev->data->dev_link.link_speed = ETH_LINK_SPEED_10000;
	eth_dev->data->dev_link.link_duplex = ETH_LINK_FULL_DUPLEX;

	eth_dev->data->mac_addrs = rte_zmalloc(name, ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL)
		goto err;

	memcpy(eth_dev->data->mac_addrs, mac_addr,
			sizeof(*eth_dev->data->mac_addrs));
	eth_dev->data->mac_addrs->addr_bytes[5] = eth_dev->data->port_id;

	eth_dev->data->dev_started = 0;
	eth_dev->data->promiscuous = 0;
	eth_dev->data->scattered_rx = 0;
	eth_dev->data->all_multicast = 0;

	memset(dev_private, 0, sizeof(*dev_private));
	eth_dev->data->dev_private = dev_private;

	eth_dev->dev_ops = dev_ops;

	/* Copy default device operation functions */
	memcpy(eth_dev->dev_ops, &virtual_ethdev_default_dev_ops,
			sizeof(*eth_dev->dev_ops));

	eth_dev->pci_dev = pci_dev;
	eth_dev->pci_dev->driver = &eth_drv->pci_drv;

	eth_dev->pci_dev->driver->id_table->device_id = 0xBEEF;

	eth_dev->rx_pkt_burst = virtual_ethdev_rx_burst_success;
	eth_dev->tx_pkt_burst = virtual_ethdev_tx_burst_success;

	return eth_dev->data->port_id;

err:
	if (pci_dev)
		rte_free(pci_dev);
	if (pci_drv)
		rte_free(pci_drv);
	if (eth_drv)
		rte_free(eth_drv);
	if (dev_ops)
		rte_free(dev_ops);
	if (id_table)
		rte_free(id_table);
	if (dev_private)
		rte_free(dev_private);

	return -1;
}
