/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
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
static int  virtual_ethdev_stop(struct rte_eth_dev *eth_dev __rte_unused)
{
	void *pkt = NULL;
	struct virtual_ethdev_private *prv = eth_dev->data->dev_private;

	eth_dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;
	eth_dev->data->dev_started = 0;
	while (rte_ring_dequeue(prv->rx_queue, &pkt) != -ENOENT)
		rte_pktmbuf_free(pkt);

	while (rte_ring_dequeue(prv->tx_queue, &pkt) != -ENOENT)
		rte_pktmbuf_free(pkt);

	return 0;
}

static int
virtual_ethdev_close(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

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

static int
virtual_ethdev_info_get(struct rte_eth_dev *dev __rte_unused,
		struct rte_eth_dev_info *dev_info)
{
	dev_info->driver_name = virtual_ethdev_driver_name;
	dev_info->max_mac_addrs = 1;

	dev_info->max_rx_pktlen = (uint32_t)2048;

	dev_info->max_rx_queues = (uint16_t)128;
	dev_info->max_tx_queues = (uint16_t)512;

	dev_info->min_rx_bufsize = 0;

	return 0;
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

static int
virtual_ethdev_link_update_success(struct rte_eth_dev *bonded_eth_dev,
		int wait_to_complete __rte_unused)
{
	if (!bonded_eth_dev->data->dev_started)
		bonded_eth_dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;

	return 0;
}

static int
virtual_ethdev_link_update_fail(struct rte_eth_dev *bonded_eth_dev __rte_unused,
		int wait_to_complete __rte_unused)
{
	return -1;
}

static int
virtual_ethdev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct virtual_ethdev_private *dev_private = dev->data->dev_private;

	if (stats)
		rte_memcpy(stats, &dev_private->eth_stats, sizeof(*stats));

	return 0;
}

static int
virtual_ethdev_stats_reset(struct rte_eth_dev *dev)
{
	struct virtual_ethdev_private *dev_private = dev->data->dev_private;
	void *pkt = NULL;

	while (rte_ring_dequeue(dev_private->tx_queue, &pkt) == -ENOBUFS)
			rte_pktmbuf_free(pkt);

	/* Reset internal statistics */
	memset(&dev_private->eth_stats, 0, sizeof(dev_private->eth_stats));

	return 0;
}

static int
virtual_ethdev_promiscuous_mode_enable(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int
virtual_ethdev_promiscuous_mode_disable(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static int
virtual_ethdev_mac_address_set(__rte_unused struct rte_eth_dev *dev,
			       __rte_unused struct rte_ether_addr *addr)
{
	return 0;
}

static const struct eth_dev_ops virtual_ethdev_default_dev_ops = {
	.dev_configure = virtual_ethdev_configure_success,
	.dev_start = virtual_ethdev_start_success,
	.dev_stop = virtual_ethdev_stop,
	.dev_close = virtual_ethdev_close,
	.dev_infos_get = virtual_ethdev_info_get,
	.rx_queue_setup = virtual_ethdev_rx_queue_setup_success,
	.tx_queue_setup = virtual_ethdev_tx_queue_setup_success,
	.link_update = virtual_ethdev_link_update_success,
	.mac_addr_set = virtual_ethdev_mac_address_set,
	.stats_get = virtual_ethdev_stats_get,
	.stats_reset = virtual_ethdev_stats_reset,
	.promiscuous_enable = virtual_ethdev_promiscuous_mode_enable,
	.promiscuous_disable = virtual_ethdev_promiscuous_mode_disable
};

void
virtual_ethdev_start_fn_set_success(uint16_t port_id, uint8_t success)
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
virtual_ethdev_configure_fn_set_success(uint16_t port_id, uint8_t success)
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
virtual_ethdev_rx_queue_setup_fn_set_success(uint16_t port_id, uint8_t success)
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
virtual_ethdev_tx_queue_setup_fn_set_success(uint16_t port_id, uint8_t success)
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
virtual_ethdev_link_update_fn_set_success(uint16_t port_id, uint8_t success)
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
			nb_pkts, NULL);

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
	struct virtual_ethdev_queue *tx_q = queue;

	struct rte_eth_dev *vrtl_eth_dev;
	struct virtual_ethdev_private *dev_private;

	int i;

	vrtl_eth_dev = &rte_eth_devices[tx_q->port_id];
	dev_private = vrtl_eth_dev->data->dev_private;

	if (!vrtl_eth_dev->data->dev_link.link_status)
		nb_pkts = 0;
	else
		nb_pkts = rte_ring_enqueue_burst(dev_private->tx_queue, (void **)bufs,
				nb_pkts, NULL);

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

	tx_q = queue;
	vrtl_eth_dev = &rte_eth_devices[tx_q->port_id];
	dev_private = vrtl_eth_dev->data->dev_private;

	if (dev_private->tx_burst_fail_count < nb_pkts) {
		int successfully_txd = nb_pkts - dev_private->tx_burst_fail_count;

		/* increment opacket count */
		dev_private->eth_stats.opackets += successfully_txd;

		/* free packets in burst */
		for (i = 0; i < successfully_txd; i++) {
			/* free packets in burst */
			rte_pktmbuf_free(bufs[i]);

			bufs[i] = NULL;
		}

		return successfully_txd;
	}

	return 0;
}


void
virtual_ethdev_rx_burst_fn_set_success(uint16_t port_id, uint8_t success)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	rte_eth_dev_stop(port_id);

	if (success)
		vrtl_eth_dev->rx_pkt_burst = virtual_ethdev_rx_burst_success;
	else
		vrtl_eth_dev->rx_pkt_burst = virtual_ethdev_rx_burst_fail;

	rte_eth_dev_start(port_id);
}


void
virtual_ethdev_tx_burst_fn_set_success(uint16_t port_id, uint8_t success)
{
	struct virtual_ethdev_private *dev_private = NULL;
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	rte_eth_dev_stop(port_id);
	dev_private = vrtl_eth_dev->data->dev_private;

	if (success)
		vrtl_eth_dev->tx_pkt_burst = virtual_ethdev_tx_burst_success;
	else
		vrtl_eth_dev->tx_pkt_burst = virtual_ethdev_tx_burst_fail;

	dev_private->tx_burst_fail_count = 0;
	rte_eth_dev_start(port_id);
}

void
virtual_ethdev_tx_burst_fn_set_tx_pkt_fail_count(uint16_t port_id,
		uint8_t packet_fail_count)
{
	struct virtual_ethdev_private *dev_private = NULL;
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	dev_private = vrtl_eth_dev->data->dev_private;
	dev_private->tx_burst_fail_count = packet_fail_count;
}

void
virtual_ethdev_set_link_status(uint16_t port_id, uint8_t link_status)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	vrtl_eth_dev->data->dev_link.link_status = link_status;
}

void
virtual_ethdev_simulate_link_status_interrupt(uint16_t port_id,
		uint8_t link_status)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	vrtl_eth_dev->data->dev_link.link_status = link_status;

	rte_eth_dev_callback_process(vrtl_eth_dev, RTE_ETH_EVENT_INTR_LSC,
				     NULL);
}

int
virtual_ethdev_add_mbufs_to_rx_queue(uint16_t port_id,
		struct rte_mbuf **pkt_burst, int burst_length)
{
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];
	struct virtual_ethdev_private *dev_private =
			vrtl_eth_dev->data->dev_private;

	return rte_ring_enqueue_burst(dev_private->rx_queue, (void **)pkt_burst,
			burst_length, NULL);
}

int
virtual_ethdev_get_mbufs_from_tx_queue(uint16_t port_id,
		struct rte_mbuf **pkt_burst, int burst_length)
{
	struct virtual_ethdev_private *dev_private;
	struct rte_eth_dev *vrtl_eth_dev = &rte_eth_devices[port_id];

	dev_private = vrtl_eth_dev->data->dev_private;
	return rte_ring_dequeue_burst(dev_private->tx_queue, (void **)pkt_burst,
		burst_length, NULL);
}


int
virtual_ethdev_create(const char *name, struct rte_ether_addr *mac_addr,
		uint8_t socket_id, uint8_t isr_support)
{
	struct rte_pci_device *pci_dev = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct rte_pci_driver *pci_drv = NULL;
	struct rte_pci_id *id_table = NULL;
	struct virtual_ethdev_private *dev_private = NULL;
	char name_buf[RTE_RING_NAMESIZE];


	/* now do all data allocation - for eth_dev structure, dummy pci driver
	 * and internal (dev_private) data
	 */

	pci_dev = rte_zmalloc_socket(name, sizeof(*pci_dev), 0, socket_id);
	if (pci_dev == NULL)
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
	eth_dev = rte_eth_dev_allocate(name);
	if (eth_dev == NULL)
		goto err;

	pci_dev->device.numa_node = socket_id;
	pci_dev->device.name = eth_dev->data->name;
	pci_drv->driver.name = virtual_ethdev_driver_name;
	pci_drv->id_table = id_table;

	if (isr_support)
		pci_drv->drv_flags |= RTE_PCI_DRV_INTR_LSC;
	else
		pci_drv->drv_flags &= ~RTE_PCI_DRV_INTR_LSC;


	eth_dev->device = &pci_dev->device;
	eth_dev->device->driver = &pci_drv->driver;

	eth_dev->data->nb_rx_queues = (uint16_t)1;
	eth_dev->data->nb_tx_queues = (uint16_t)1;

	eth_dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;
	eth_dev->data->dev_link.link_speed = RTE_ETH_SPEED_NUM_10G;
	eth_dev->data->dev_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;

	eth_dev->data->mac_addrs = rte_zmalloc(name, RTE_ETHER_ADDR_LEN, 0);
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

	pci_dev->device.driver = &pci_drv->driver;
	eth_dev->device = &pci_dev->device;

	eth_dev->rx_pkt_burst = virtual_ethdev_rx_burst_success;
	eth_dev->tx_pkt_burst = virtual_ethdev_tx_burst_success;

	rte_eth_dev_probing_finish(eth_dev);

	return eth_dev->data->port_id;

err:
	rte_free(pci_dev);
	rte_free(pci_drv);
	rte_free(id_table);
	rte_free(dev_private);

	return -1;
}
