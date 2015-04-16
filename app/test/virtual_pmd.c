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
#include <rte_ring.h>

#include "virtual_pmd.h"

#define MAX_PKT_BURST 512

static const char *virtual_ethdev_driver_name = "Virtual PMD";

struct virtual_ethdev_private {
	struct eth_dev_ops dev_ops;
	struct rte_eth_stats eth_stats;

	struct rte_ring *rx_queue;
	struct rte_ring *tx_queue;

	int tx_burst_fail_count;
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
	void *pkt = NULL;
	struct virtual_ethdev_private *prv = eth_dev->data->dev_private;

	eth_dev->data->dev_link.link_status = 0;
	eth_dev->data->dev_started = 0;
	while (rte_ring_dequeue(prv->rx_queue, &pkt) != -ENOENT)
		rte_pktmbuf_free(pkt);

	while (rte_ring_dequeue(prv->tx_queue, &pkt) != -ENOENT)
		rte_pktmbuf_free(pkt);
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
	void *pkt = NULL;

	while (rte_ring_dequeue(dev_private->tx_queue, &pkt) == -ENOBUFS)
			rte_pktmbuf_free(pkt);

	/* Reset internal statistics */
	memset(&dev_private->eth_stats, 0, sizeof(dev_private->eth_stats));
}

static void
virtual_ethdev_promiscuous_mode_enable(struct rte_eth_dev *dev __rte_unused)
{}

static void
virtual_ethdev_promiscuous_mode_disable(struct rte_eth_dev *dev __rte_unused)
{}


static const struct eth_dev_ops virtual_ethdev_default_dev_ops = {
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
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct virtual_ethdev_private *dev_private = dev->data->dev_private;
	struct eth_dev_ops *dev_ops = &dev_private->dev_ops;

	if (success)
		dev_ops->dev_start = virtual_ethdev_start_success;
	else
		dev_ops->dev_start = virtual_ethdev_start_fail;

}

void
virtual_ethdev_configure_fn_set_success(uint8_t port_id, uint8_t success)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct virtual_ethdev_private *dev_private = dev->data->dev_private;
	struct eth_dev_ops *dev_ops = &dev_private->dev_ops;

	if (success)
		dev_ops->dev_configure = virtual_ethdev_configure_success;
	else
		dev_ops->dev_configure = virtual_ethdev_configure_fail;
}

void
virtual_ethdev_rx_queue_setup_fn_set_success(uint8_t port_id, uint8_t success)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct virtual_ethdev_private *dev_private = dev->data->dev_private;
	struct eth_dev_ops *dev_ops = &dev_private->dev_ops;

	if (success)
		dev_ops->rx_queue_setup = virtual_ethdev_rx_queue_setup_success;
	else
		dev_ops->rx_queue_setup = virtual_ethdev_rx_queue_setup_fail;
}

void
virtual_ethdev_tx_queue_setup_fn_set_success(uint8_t port_id, uint8_t success)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct virtual_ethdev_private *dev_private = dev->data->dev_private;
	struct eth_dev_ops *dev_ops = &dev_private->dev_ops;

	if (success)
		dev_ops->tx_queue_setup = virtual_ethdev_tx_queue_setup_success;
	else
		dev_ops->tx_queue_setup = virtual_ethdev_tx_queue_setup_fail;
}

void
virtual_ethdev_link_update_fn_set_success(uint8_t port_id, uint8_t success)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct virtual_ethdev_private *dev_private = dev->data->dev_private;
	struct eth_dev_ops *dev_ops = &dev_private->dev_ops;

	if (success)
		dev_ops->link_update = virtual_ethdev_link_update_success;
	else
		dev_ops->link_update = virtual_ethdev_link_update_fail;
}


static uint16_t
virtual_ethdev_rx_burst_success(void *queue __rte_unused,
							 struct rte_mbuf **bufs,
							 uint16_t nb_pkts)
{
	struct rte_eth_dev *vrtl_eth_dev;
	struct virtual_ethdev_queue *pq_map;
	struct virtual_ethdev_private *dev_private;

	int rx_count, i;

	pq_map = (struct virtual_ethdev_queue *)queue;
	vrtl_eth_dev = &rte_eth_devices[pq_map->port_id];
	dev_private = vrtl_eth_dev->data->dev_private;

	rx_count = rte_ring_dequeue_burst(dev_private->rx_queue, (void **) bufs,
			nb_pkts);

	/* increments ipackets count */
	dev_private->eth_stats.ipackets += rx_count;

	/* increments ibytes count */
	for (i = 0; i < rx_count; i++)
		dev_private->eth_stats.ibytes += rte_pktmbuf_pkt_len(bufs[i]);

	return rx_count;
}

static uint16_t
virtual_ethdev_rx_burst_fail(void *queue __rte_unused,
							 struct rte_mbuf **bufs __rte_unused,
							 uint16_t nb_pkts __rte_unused)
{
	return 0;
}

static uint16_t
virtual_ethdev_tx_burst_success(void *queue, struct rte_mbuf **bufs,
		uint16_t nb_pkts)
{
	struct virtual_ethdev_queue *tx_q = (struct virtual_ethdev_queue *)queue;

	struct rte_eth_dev *vrtl_eth_dev;
	struct virtual_ethdev_private *dev_private;

	int i;

	vrtl_eth_dev = &rte_eth_devices[tx_q->port_id];
	dev_private = vrtl_eth_dev->data->dev_private;

	if (!vrtl_eth_dev->data->dev_link.link_status)
		nb_pkts = 0;
	else
		nb_pkts = rte_ring_enqueue_burst(dev_private->tx_queue, (void **)bufs,
				nb_pkts);

	/* increment opacket count */
	dev_private->eth_stats.opackets += nb_pkts;

	/* increment obytes count */
	for (i = 0; i < nb_pkts; i++)
		dev_private->eth_stats.obytes += rte_pktmbuf_pkt_len(bufs[i]);

	return nb_pkts;
}

static uint16_t
virtual_ethdev_tx_burst_fail(void *queue, struct rte_mbuf **bufs,
		uint16_t nb_pkts)
{
	struct rte_eth_dev *vrtl_eth_dev = NULL;
	struct virtual_ethdev_queue *tx_q = NULL;
	struct virtual_ethdev_private *dev_private = NULL;

	int i;

	tx_q = (struct virtual_ethdev_queue *)queue;
	vrtl_eth_dev = &rte_eth_devices[tx_q->port_id];
	dev_private = vrtl_eth_dev->data->dev_private;

	if (dev_private->tx_burst_fail_count < nb_pkts) {
		int successfully_txd = nb_pkts - dev_private->tx_burst_fail_count;

		/* increment opacket count */
		dev_private->eth_stats.opackets += successfully_txd;

		/* free packets in burst */
		for (i = 0; i < successfully_txd; i++) {
			/* free packets in burst */
			if (bufs[i] != NULL)
				rte_pktmbuf_free(bufs[i]);

			bufs[i] = NULL;
		}

		return successfully_txd;
	}

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
	struct virtual_ethdev_private *dev_private = NULL;
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	dev_private = vrtl_eth_dev->data->dev_private;

	if (success)
		vrtl_eth_dev->tx_pkt_burst = virtual_ethdev_tx_burst_success;
	else
		vrtl_eth_dev->tx_pkt_burst = virtual_ethdev_tx_burst_fail;

	dev_private->tx_burst_fail_count = 0;
}

void
virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(uint8_t port_id,
		uint8_t packet_fail_count)
{
	struct virtual_ethdev_private *dev_private = NULL;
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];


	dev_private = vrtl_eth_dev->data->dev_private;
	dev_private->tx_burst_fail_count = packet_fail_count;
}

void
virtual_ethdev_set_link_status(uint8_t port_id, uint8_t link_status)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	vrtl_eth_dev->data->dev_link.link_status = link_status;
}

void
virtual_ethdev_simulate_link_status_interrupt(uint8_t port_id,
		uint8_t link_status)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	vrtl_eth_dev->data->dev_link.link_status = link_status;

	_rte_eth_dev_callback_process(vrtl_eth_dev, RTE_ETH_EVENT_INTR_LSC);
}

int
virtual_ethdev_add_mbufs_to_rx_queue(uint8_t port_id,
		struct rte_mbuf **pkt_burst, int burst_length)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];
	struct virtual_ethdev_private *dev_private =
			vrtl_eth_dev->data->dev_private;

	return rte_ring_enqueue_burst(dev_private->rx_queue, (void **)pkt_burst,
			burst_length);
}

int
virtual_ethdev_get_mbufs_from_tx_queue(uint8_t port_id,
		struct rte_mbuf **pkt_burst, int burst_length)
{
	struct virtual_ethdev_private *dev_private;
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	dev_private = vrtl_eth_dev->data->dev_private;
	return rte_ring_dequeue_burst(dev_private->tx_queue, (void **)pkt_burst,
		burst_length);
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
		uint8_t socket_id, uint8_t isr_support)
{
	struct rte_pci_device *pci_dev = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct eth_driver *eth_drv = NULL;
	struct rte_pci_driver *pci_drv = NULL;
	struct rte_pci_id *id_table = NULL;
	struct virtual_ethdev_private *dev_private = NULL;
	char name_buf[RTE_RING_NAMESIZE];


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

	id_table = rte_zmalloc_socket(name, sizeof(*id_table), 0, socket_id);
	if (id_table == NULL)
		goto err;
	id_table->device_id = 0xBEEF;

	dev_private = rte_zmalloc_socket(name, sizeof(*dev_private), 0, socket_id);
	if (dev_private == NULL)
		goto err;

	snprintf(name_buf, sizeof(name_buf), "%s_rxQ", name);
	dev_private->rx_queue = rte_ring_create(name_buf, MAX_PKT_BURST, socket_id,
			0);
	if (dev_private->rx_queue == NULL)
		goto err;

	snprintf(name_buf, sizeof(name_buf), "%s_txQ", name);
	dev_private->tx_queue = rte_ring_create(name_buf, MAX_PKT_BURST, socket_id,
			0);
	if (dev_private->tx_queue == NULL)
		goto err;

	/* reserve an ethdev entry */
	eth_dev = rte_eth_dev_allocate(name, RTE_ETH_DEV_PCI);
	if (eth_dev == NULL)
		goto err;

	pci_dev->numa_node = socket_id;
	pci_drv->name = virtual_ethdev_driver_name;
	pci_drv->id_table = id_table;

	if (isr_support)
		pci_drv->drv_flags |= RTE_PCI_DRV_INTR_LSC;
	else
		pci_drv->drv_flags &= ~RTE_PCI_DRV_INTR_LSC;


	eth_drv->pci_drv = (struct rte_pci_driver)(*pci_drv);
	eth_dev->driver = eth_drv;

	eth_dev->data->nb_rx_queues = (uint16_t)1;
	eth_dev->data->nb_tx_queues = (uint16_t)1;

	TAILQ_INIT(&(eth_dev->link_intr_cbs));

	eth_dev->data->dev_link.link_status = 0;
	eth_dev->data->dev_link.link_speed = ETH_LINK_SPEED_10000;
	eth_dev->data->dev_link.link_duplex = ETH_LINK_FULL_DUPLEX;

	eth_dev->data->mac_addrs = rte_zmalloc(name, ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL)
		goto err;

	memcpy(eth_dev->data->mac_addrs, mac_addr,
			sizeof(*eth_dev->data->mac_addrs));

	eth_dev->data->dev_started = 0;
	eth_dev->data->promiscuous = 0;
	eth_dev->data->scattered_rx = 0;
	eth_dev->data->all_multicast = 0;

	eth_dev->data->dev_private = dev_private;

	/* Copy default device operation functions */
	dev_private->dev_ops = virtual_ethdev_default_dev_ops;
	eth_dev->dev_ops = &dev_private->dev_ops;

	eth_dev->pci_dev = pci_dev;
	eth_dev->pci_dev->driver = &eth_drv->pci_drv;

	eth_dev->rx_pkt_burst = virtual_ethdev_rx_burst_success;
	eth_dev->tx_pkt_burst = virtual_ethdev_tx_burst_success;

	return eth_dev->data->port_id;

err:
	rte_free(pci_dev);
	rte_free(pci_drv);
	rte_free(eth_drv);
	rte_free(id_table);
	rte_free(dev_private);

	return -1;
}
