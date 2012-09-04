/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 *  version: DPDK.L.1.2.3-3
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_errno.h>
#include <rte_spinlock.h>

#include "rte_ether.h"
#include "rte_ethdev.h"

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
#define PMD_DEBUG_TRACE(fmt, args...) do {                        \
		RTE_LOG(ERR, PMD, "%s: " fmt, __func__, ## args); \
	} while (0)
#else
#define PMD_DEBUG_TRACE(fmt, args...)
#endif

/* define two macros for quick checking for restricting functions to primary
 * instance only. First macro is for functions returning an int - and therefore
 * an error code, second macro is for functions returning null.
 */
#define PROC_PRIMARY_OR_ERR() do { \
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) { \
		PMD_DEBUG_TRACE("Cannot run %s in secondary processes\n", \
							__func__); \
			return (-E_RTE_SECONDARY); \
		} \
} while(0)

#define PROC_PRIMARY_OR_RET() do { \
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) { \
		PMD_DEBUG_TRACE("Cannot run %s in secondary processes\n", \
							__func__); \
		return; \
	} \
} while(0)

/* Macros to check for invlaid function pointers in dev_ops structure */
#define FUNC_PTR_OR_ERR_RET(func, retval) do { \
	if ((func) == NULL) { \
		PMD_DEBUG_TRACE("Function not supported\n"); \
		return (retval); \
	} \
} while(0)
#define FUNC_PTR_OR_RET(func) do { \
	if ((func) == NULL) { \
		PMD_DEBUG_TRACE("Function not supported\n"); \
		return; \
	} \
} while(0)

static const char *MZ_RTE_ETH_DEV_DATA = "rte_eth_dev_data";
struct rte_eth_dev rte_eth_devices[RTE_MAX_ETHPORTS];
static struct rte_eth_dev_data *rte_eth_dev_data = NULL;
static uint8_t nb_ports = 0;

/* spinlock for eth device callbacks */
static rte_spinlock_t rte_eth_dev_cb_lock = RTE_SPINLOCK_INITIALIZER;

/**
 * The user application callback description.
 *
 * It contains callback address to be registered by user application,
 * the pointer to the parameters for callback, and the event type.
 */
struct rte_eth_dev_callback {
	TAILQ_ENTRY(rte_eth_dev_callback) next; /**< Callbacks list */
	rte_eth_dev_cb_fn cb_fn;                /**< Callback address */
	void *cb_arg;                           /**< Parameter for callback */
	enum rte_eth_event_type event;          /**< Interrupt event type */
};

static inline void
rte_eth_dev_data_alloc(void)
{
	const unsigned flags = 0;
	const struct rte_memzone *mz;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY){
		mz = rte_memzone_reserve(MZ_RTE_ETH_DEV_DATA,
				RTE_MAX_ETHPORTS * sizeof(*rte_eth_dev_data),
				rte_socket_id(), flags);
	} else
		mz = rte_memzone_lookup(MZ_RTE_ETH_DEV_DATA);
	if (mz == NULL)
		rte_panic("Cannot allocate memzone for ethernet port data\n");

	rte_eth_dev_data = mz->addr;
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		memset(rte_eth_dev_data, 0,
				RTE_MAX_ETHPORTS * sizeof(*rte_eth_dev_data));
}

static inline struct rte_eth_dev *
rte_eth_dev_allocate(void)
{
	struct rte_eth_dev *eth_dev;

	if (nb_ports == RTE_MAX_ETHPORTS)
		return NULL;

	if (rte_eth_dev_data == NULL)
		rte_eth_dev_data_alloc();

	eth_dev = &rte_eth_devices[nb_ports];
	eth_dev->data = &rte_eth_dev_data[nb_ports];
	eth_dev->data->port_id = nb_ports++;
	return eth_dev;
}

static int
rte_eth_dev_init(struct rte_pci_driver *pci_drv,
		 struct rte_pci_device *pci_dev)
{
	struct eth_driver    *eth_drv;
	struct rte_eth_dev *eth_dev;
	int diag;

	eth_drv = (struct eth_driver *)pci_drv;

	eth_dev = rte_eth_dev_allocate();
	if (eth_dev == NULL)
		return -ENOMEM;


	if (rte_eal_process_type() == RTE_PROC_PRIMARY){
		eth_dev->data->dev_private = rte_zmalloc("ethdev private structure",
				  eth_drv->dev_private_size,
				  CACHE_LINE_SIZE);
		if (eth_dev->data->dev_private == NULL)
			return -ENOMEM;
	}
	eth_dev->pci_dev = pci_dev;
	eth_dev->driver = eth_drv;
	eth_dev->data->rx_mbuf_alloc_failed = 0;

	/* init user callbacks */
	TAILQ_INIT(&(eth_dev->callbacks));

	/*
	 * Set the default maximum frame size.
	 */
	eth_dev->data->max_frame_size = ETHER_MAX_LEN;

	/* Invoke PMD device initialization function */
	diag = (*eth_drv->eth_dev_init)(eth_drv, eth_dev);
	if (diag == 0)
		return (0);

	PMD_DEBUG_TRACE("driver %s: eth_dev_init(vendor_id=0x%u device_id=0x%x)"
			" failed\n", pci_drv->name,
			(unsigned) pci_dev->id.vendor_id,
			(unsigned) pci_dev->id.device_id);
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(eth_dev->data->dev_private);
	nb_ports--;
	return diag;
}

/**
 * Register an Ethernet [Poll Mode] driver.
 *
 * Function invoked by the initialization function of an Ethernet driver
 * to simultaneously register itself as a PCI driver and as an Ethernet
 * Poll Mode Driver.
 * Invokes the rte_eal_pci_register() function to register the *pci_drv*
 * structure embedded in the *eth_drv* structure, after having stored the
 * address of the rte_eth_dev_init() function in the *devinit* field of
 * the *pci_drv* structure.
 * During the PCI probing phase, the rte_eth_dev_init() function is
 * invoked for each PCI [Ethernet device] matching the embedded PCI
 * identifiers provided by the driver.
 */
void
rte_eth_driver_register(struct eth_driver *eth_drv)
{
	eth_drv->pci_drv.devinit = rte_eth_dev_init;
	rte_eal_pci_register(&eth_drv->pci_drv);
}

uint8_t
rte_eth_dev_count(void)
{
	return (nb_ports);
}

int
rte_eth_dev_configure(uint8_t port_id, uint16_t nb_rx_q, uint16_t nb_tx_q,
		      const struct rte_eth_conf *dev_conf)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	int diag;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR();

	if (port_id >= nb_ports || port_id >= RTE_MAX_ETHPORTS) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_configure, -ENOTSUP);

	if (dev->data->dev_started) {
		PMD_DEBUG_TRACE(
		    "port %d must be stopped to allow configuration", port_id);
		return -EBUSY;
	}

	/*
	 * Check that the numbers of RX and TX queues are not greater
	 * than the maximum number of RX and TX queues supported by the
	 * configured device.
	 */
	(*dev->dev_ops->dev_infos_get)(dev, &dev_info);
	if (nb_rx_q > dev_info.max_rx_queues) {
		PMD_DEBUG_TRACE("ethdev port_id=%d nb_rx_queues=%d > %d",
				port_id, nb_rx_q, dev_info.max_rx_queues);
		return (-EINVAL);
	}
	if (nb_rx_q == 0) {
		PMD_DEBUG_TRACE("ethdev port_id=%d nb_rx_q == 0", port_id);
		return (-EINVAL);
	}

	if (nb_tx_q > dev_info.max_tx_queues) {
		PMD_DEBUG_TRACE("ethdev port_id=%d nb_tx_queues=%d > %d",
				port_id, nb_tx_q, dev_info.max_tx_queues);
		return (-EINVAL);
	}
	if (nb_tx_q == 0) {
		PMD_DEBUG_TRACE("ethdev port_id=%d nb_tx_q == 0", port_id);
		return (-EINVAL);
	}

	/* Copy the dev_conf parameter into the dev structure */
	memcpy(&dev->data->dev_conf, dev_conf, sizeof(dev->data->dev_conf));

	/*
	 * If jumbo frames are enabled, check that the maximum RX packet
	 * length is supported by the configured device.
	 */
	if (dev_conf->rxmode.jumbo_frame == 1) {
		if (dev_conf->rxmode.max_rx_pkt_len >
		    dev_info.max_rx_pktlen) {
			PMD_DEBUG_TRACE("ethdev port_id=%d max_rx_pkt_len %u"
				" > max valid value %u",
				port_id,
				(unsigned)dev_conf->rxmode.max_rx_pkt_len,
				(unsigned)dev_info.max_rx_pktlen);
			return (-EINVAL);
		}
	} else
		/* Use default value */
		dev->data->dev_conf.rxmode.max_rx_pkt_len = ETHER_MAX_LEN;

	/* For vmdb+dcb mode check our configuration before we go further */
	if (dev_conf->rxmode.mq_mode == ETH_VMDQ_DCB) {
		const struct rte_eth_vmdq_dcb_conf *conf;

		if (nb_rx_q != ETH_VMDQ_DCB_NUM_QUEUES) {
			PMD_DEBUG_TRACE("ethdev port_id=%d VMDQ+DCB, nb_rx_q "
					"!= %d",
					port_id, ETH_VMDQ_DCB_NUM_QUEUES);
			return (-EINVAL);
		}
		conf = &(dev_conf->rx_adv_conf.vmdq_dcb_conf);
		if (! (conf->nb_queue_pools == ETH_16_POOLS ||
		       conf->nb_queue_pools == ETH_32_POOLS)) {
		    PMD_DEBUG_TRACE("ethdev port_id=%d VMDQ+DCB selected, "
				    "nb_queue_pools != %d or nb_queue_pools "
				    "!= %d",
				    port_id, ETH_16_POOLS, ETH_32_POOLS);
		    return (-EINVAL);
		}
	}

	diag = (*dev->dev_ops->dev_configure)(dev, nb_rx_q, nb_tx_q);
	if (diag != 0) {
		rte_free(dev->data->rx_queues);
		rte_free(dev->data->tx_queues);
	}
	return diag;
}

static void
rte_eth_dev_config_restore(uint8_t port_id)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct ether_addr addr;
	uint16_t i;

	dev = &rte_eth_devices[port_id];

	rte_eth_dev_info_get(port_id, &dev_info);

	/* replay MAC address configuration */
	for (i = 0; i < dev_info.max_mac_addrs; i++) {
		addr = dev->data->mac_addrs[i];

		/* skip zero address */
		if (is_zero_ether_addr(&addr))
			continue;

		/* add address to the hardware */
		if  (*dev->dev_ops->mac_addr_add)
			(*dev->dev_ops->mac_addr_add)(dev, &addr, i, 0);
		else {
			PMD_DEBUG_TRACE("port %d: MAC address array not supported\n",
					port_id);
			/* exit the loop but not return an error */
			break;
		}
	}

	/* replay promiscuous configuration */
	if (rte_eth_promiscuous_get(port_id) == 1)
		rte_eth_promiscuous_enable(port_id);
	else if (rte_eth_promiscuous_get(port_id) == 0)
		rte_eth_promiscuous_disable(port_id);

	/* replay allmulticast configuration */
	if (rte_eth_allmulticast_get(port_id) == 1)
		rte_eth_allmulticast_enable(port_id);
	else if (rte_eth_allmulticast_get(port_id) == 0)
		rte_eth_allmulticast_disable(port_id);
}

int
rte_eth_dev_start(uint8_t port_id)
{
	struct rte_eth_dev *dev;
	int diag;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR();

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_start, -ENOTSUP);
	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	rte_eth_dev_config_restore(port_id);

	return 0;
}

void
rte_eth_dev_stop(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_RET();

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_RET(*dev->dev_ops->dev_stop);
	dev->data->dev_started = 0;
	(*dev->dev_ops->dev_stop)(dev);
}

void
rte_eth_dev_close(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_RET();

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_RET(*dev->dev_ops->dev_close);
	dev->data->dev_started = 0;
	(*dev->dev_ops->dev_close)(dev);
}

int
rte_eth_rx_queue_setup(uint8_t port_id, uint16_t rx_queue_id,
		       uint16_t nb_rx_desc, unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mp)
{
	struct rte_eth_dev *dev;
	struct rte_pktmbuf_pool_private *mbp_priv;
	struct rte_eth_dev_info dev_info;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR();

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}
	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", rx_queue_id);
		return (-EINVAL);
	}

	if (dev->data->dev_started) {
		PMD_DEBUG_TRACE(
		    "port %d must be stopped to allow configuration", port_id);
		return -EBUSY;
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_setup, -ENOTSUP);

	/*
	 * Check the size of the mbuf data buffer.
	 * This value must be provided in the private data of the memory pool.
	 * First check that the memory pool has a valid private data.
	 */
	(*dev->dev_ops->dev_infos_get)(dev, &dev_info);
	if (mp->private_data_size < sizeof(struct rte_pktmbuf_pool_private)) {
		PMD_DEBUG_TRACE("%s private_data_size %d < %d\n",
				mp->name, (int) mp->private_data_size,
				(int) sizeof(struct rte_pktmbuf_pool_private));
		return (-ENOSPC);
	}
	mbp_priv = (struct rte_pktmbuf_pool_private *)
		((char *)mp + sizeof(struct rte_mempool));
	if ((uint32_t) (mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM) <
	    dev_info.min_rx_bufsize) {
		PMD_DEBUG_TRACE("%s mbuf_data_room_size %d < %d "
				"(RTE_PKTMBUF_HEADROOM=%d + min_rx_bufsize(dev)"
				"=%d)\n",
				mp->name,
				(int)mbp_priv->mbuf_data_room_size,
				(int)(RTE_PKTMBUF_HEADROOM +
				      dev_info.min_rx_bufsize),
				(int)RTE_PKTMBUF_HEADROOM,
				(int)dev_info.min_rx_bufsize);
		return (-EINVAL);
	}

	return (*dev->dev_ops->rx_queue_setup)(dev, rx_queue_id, nb_rx_desc,
					       socket_id, rx_conf, mp);
}

int
rte_eth_tx_queue_setup(uint8_t port_id, uint16_t tx_queue_id,
		       uint16_t nb_tx_desc, unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf)
{
	struct rte_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR();

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}
	dev = &rte_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", tx_queue_id);
		return (-EINVAL);
	}

	if (dev->data->dev_started) {
		PMD_DEBUG_TRACE(
		    "port %d must be stopped to allow configuration", port_id);
		return -EBUSY;
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_setup, -ENOTSUP);
	return (*dev->dev_ops->tx_queue_setup)(dev, tx_queue_id, nb_tx_desc,
					       socket_id, tx_conf);
}

void
rte_eth_promiscuous_enable(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_RET(*dev->dev_ops->promiscuous_enable);
	(*dev->dev_ops->promiscuous_enable)(dev);
	dev->data->promiscuous = 1;
}

void
rte_eth_promiscuous_disable(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_RET(*dev->dev_ops->promiscuous_disable);
	dev->data->promiscuous = 0;
	(*dev->dev_ops->promiscuous_disable)(dev);
}

int
rte_eth_promiscuous_get(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -1;
	}

	dev = &rte_eth_devices[port_id];
	return dev->data->promiscuous;
}

void
rte_eth_allmulticast_enable(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_RET(*dev->dev_ops->allmulticast_enable);
	(*dev->dev_ops->allmulticast_enable)(dev);
	dev->data->all_multicast = 1;
}

void
rte_eth_allmulticast_disable(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_RET(*dev->dev_ops->allmulticast_disable);
	dev->data->all_multicast = 0;
	(*dev->dev_ops->allmulticast_disable)(dev);
}

int
rte_eth_allmulticast_get(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -1;
	}

	dev = &rte_eth_devices[port_id];
	return dev->data->all_multicast;
}

static inline int
rte_eth_dev_atomic_read_link_status(struct rte_eth_dev *dev,
				struct rte_eth_link *link)
{
	struct rte_eth_link *dst = link;
	struct rte_eth_link *src = &(dev->data->dev_link);

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

void
rte_eth_link_get(uint8_t port_id, struct rte_eth_link *eth_link)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}
	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_RET(*dev->dev_ops->link_update);

	if (dev->data->dev_conf.intr_conf.lsc != 0)
		rte_eth_dev_atomic_read_link_status(dev, eth_link);
	else {
		(*dev->dev_ops->link_update)(dev, 1);
		*eth_link = dev->data->dev_link;
	}
}

void
rte_eth_link_get_nowait(uint8_t port_id, struct rte_eth_link *eth_link)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}
	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_RET(*dev->dev_ops->link_update);

	if (dev->data->dev_conf.intr_conf.lsc != 0)
		rte_eth_dev_atomic_read_link_status(dev, eth_link);
	else {
		(*dev->dev_ops->link_update)(dev, 0);
		*eth_link = dev->data->dev_link;
	}
}

void
rte_eth_stats_get(uint8_t port_id, struct rte_eth_stats *stats)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_RET(*dev->dev_ops->stats_get);
	(*dev->dev_ops->stats_get)(dev, stats);
	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
}

void
rte_eth_stats_reset(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_RET(*dev->dev_ops->stats_reset);
	(*dev->dev_ops->stats_reset)(dev);
}

void
rte_eth_dev_info_get(uint8_t port_id, struct rte_eth_dev_info *dev_info)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_RET(*dev->dev_ops->dev_infos_get);
	(*dev->dev_ops->dev_infos_get)(dev, dev_info);
	dev_info->pci_dev = dev->pci_dev;
	dev_info->driver_name = dev->driver->pci_drv.name;
}

void
rte_eth_macaddr_get(uint8_t port_id, struct ether_addr *mac_addr)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}
	dev = &rte_eth_devices[port_id];
	ether_addr_copy(&dev->data->mac_addrs[0], mac_addr);
}

int
rte_eth_dev_vlan_filter(uint8_t port_id, uint16_t vlan_id, int on)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}
	dev = &rte_eth_devices[port_id];
	if (! (dev->data->dev_conf.rxmode.hw_vlan_filter)) {
		PMD_DEBUG_TRACE("port %d: vlan-filtering disabled\n", port_id);
		return (-ENOSYS);
	}
	if (vlan_id > 4095) {
		PMD_DEBUG_TRACE("(port_id=%d) invalid vlan_id=%u > 4095\n",
				port_id, (unsigned) vlan_id);
		return (-EINVAL);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_filter_set, -ENOTSUP);
	(*dev->dev_ops->vlan_filter_set)(dev, vlan_id, on);
	return (0);
}

int
rte_eth_dev_fdir_add_signature_filter(uint8_t port_id,
				      struct rte_fdir_filter *fdir_filter,
				      uint8_t queue)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode != RTE_FDIR_MODE_SIGNATURE) {
		PMD_DEBUG_TRACE("port %d: invalid FDIR mode=%u\n",
				port_id, dev->data->dev_conf.fdir_conf.mode);
		return (-ENOSYS);
	}

	if ((fdir_filter->l4type == RTE_FDIR_L4TYPE_SCTP
	     || fdir_filter->l4type == RTE_FDIR_L4TYPE_NONE)
	    && (fdir_filter->port_src || fdir_filter->port_dst)) {
		PMD_DEBUG_TRACE(" Port are meaningless for SCTP and " \
				"None l4type source & destinations ports " \
				"should be null!");
		return (-EINVAL);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_add_signature_filter, -ENOTSUP);
	if (*dev->dev_ops->fdir_add_signature_filter)
		return (*dev->dev_ops->fdir_add_signature_filter)(dev,
								  fdir_filter,
								  queue);

	PMD_DEBUG_TRACE("port %d: FDIR feature not supported\n", port_id);
	return (-ENOTSUP);
}

int
rte_eth_dev_fdir_update_signature_filter(uint8_t port_id,
					 struct rte_fdir_filter *fdir_filter,
					 uint8_t queue)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode != RTE_FDIR_MODE_SIGNATURE) {
		PMD_DEBUG_TRACE("port %d: invalid FDIR mode=%u\n",
				port_id, dev->data->dev_conf.fdir_conf.mode);
		return (-ENOSYS);
	}

	if ((fdir_filter->l4type == RTE_FDIR_L4TYPE_SCTP
	     || fdir_filter->l4type == RTE_FDIR_L4TYPE_NONE)
	    && (fdir_filter->port_src || fdir_filter->port_dst)) {
		PMD_DEBUG_TRACE(" Port are meaningless for SCTP and " \
				"None l4type source & destinations ports " \
				"should be null!");
		return (-EINVAL);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_update_signature_filter, -ENOTSUP);
	if (*dev->dev_ops->fdir_update_signature_filter)
		return (*dev->dev_ops->fdir_update_signature_filter)(dev,
								     fdir_filter,
								     queue);


	PMD_DEBUG_TRACE("port %d: FDIR feature not supported\n", port_id);
	return (-ENOTSUP);
}

int
rte_eth_dev_fdir_remove_signature_filter(uint8_t port_id,
					 struct rte_fdir_filter *fdir_filter)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode != RTE_FDIR_MODE_SIGNATURE) {
		PMD_DEBUG_TRACE("port %d: invalid FDIR mode=%u\n",
				port_id, dev->data->dev_conf.fdir_conf.mode);
		return (-ENOSYS);
	}

	if ((fdir_filter->l4type == RTE_FDIR_L4TYPE_SCTP
	     || fdir_filter->l4type == RTE_FDIR_L4TYPE_NONE)
	    && (fdir_filter->port_src || fdir_filter->port_dst)) {
		PMD_DEBUG_TRACE(" Port are meaningless for SCTP and " \
				"None l4type source & destinations ports " \
				"should be null!");
		return (-EINVAL);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_remove_signature_filter, -ENOTSUP);
	if (*dev->dev_ops->fdir_remove_signature_filter)
		return (*dev->dev_ops->fdir_remove_signature_filter)(dev,
								     fdir_filter);

	PMD_DEBUG_TRACE("port %d: FDIR feature not supported\n", port_id);
	return (-ENOTSUP);
}

int
rte_eth_dev_fdir_get_infos(uint8_t port_id, struct rte_eth_fdir *fdir)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	if (! (dev->data->dev_conf.fdir_conf.mode)) {
		PMD_DEBUG_TRACE("port %d: pkt-filter disabled\n", port_id);
		return (-ENOSYS);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_infos_get, -ENOTSUP);
	if (*dev->dev_ops->fdir_infos_get) {
		(*dev->dev_ops->fdir_infos_get)(dev, fdir);
		return (0);
	}

	PMD_DEBUG_TRACE("port %d: FDIR feature not supported\n", port_id);
	return (-ENOTSUP);
}

int
rte_eth_dev_fdir_add_perfect_filter(uint8_t port_id,
				    struct rte_fdir_filter *fdir_filter,
				    uint16_t soft_id, uint8_t queue,
				    uint8_t drop)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode != RTE_FDIR_MODE_PERFECT) {
		PMD_DEBUG_TRACE("port %d: invalid FDIR mode=%u\n",
				port_id, dev->data->dev_conf.fdir_conf.mode);
		return (-ENOSYS);
	}

	if ((fdir_filter->l4type == RTE_FDIR_L4TYPE_SCTP
	     || fdir_filter->l4type == RTE_FDIR_L4TYPE_NONE)
	    && (fdir_filter->port_src || fdir_filter->port_dst)) {
		PMD_DEBUG_TRACE(" Port are meaningless for SCTP and " \
				"None l4type source & destinations ports " \
				"should be null!");
		return (-EINVAL);
	}

	/* For now IPv6 is not supported with perfect filter */
	if (fdir_filter->iptype == RTE_FDIR_IPTYPE_IPV6)
		return (-ENOTSUP);

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_add_perfect_filter, -ENOTSUP);
	if  (*dev->dev_ops->fdir_add_perfect_filter)
		return (*dev->dev_ops->fdir_add_perfect_filter)(dev, fdir_filter,
								soft_id, queue,
								drop);

	PMD_DEBUG_TRACE("port %d: FDIR feature not supported\n", port_id);
	return (-ENOTSUP);
}

int
rte_eth_dev_fdir_update_perfect_filter(uint8_t port_id,
				       struct rte_fdir_filter *fdir_filter,
				       uint16_t soft_id, uint8_t queue,
				       uint8_t drop)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode != RTE_FDIR_MODE_PERFECT) {
		PMD_DEBUG_TRACE("port %d: invalid FDIR mode=%u\n",
				port_id, dev->data->dev_conf.fdir_conf.mode);
		return (-ENOSYS);
	}

	if ((fdir_filter->l4type == RTE_FDIR_L4TYPE_SCTP
	     || fdir_filter->l4type == RTE_FDIR_L4TYPE_NONE)
	    && (fdir_filter->port_src || fdir_filter->port_dst)) {
		PMD_DEBUG_TRACE(" Port are meaningless for SCTP and " \
				"None l4type source & destinations ports " \
				"should be null!");
		return (-EINVAL);
	}

	/* For now IPv6 is not supported with perfect filter */
	if (fdir_filter->iptype == RTE_FDIR_IPTYPE_IPV6)
		return (-ENOTSUP);

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_update_perfect_filter, -ENOTSUP);
	if  (*dev->dev_ops->fdir_update_perfect_filter)
		return (*dev->dev_ops->fdir_update_perfect_filter)(dev,
								   fdir_filter,
								   soft_id,
								   queue,
								   drop);

	PMD_DEBUG_TRACE("port %d: FDIR feature not supported\n", port_id);
	return (-ENOTSUP);
}

int
rte_eth_dev_fdir_remove_perfect_filter(uint8_t port_id,
				       struct rte_fdir_filter *fdir_filter,
				       uint16_t soft_id)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.fdir_conf.mode != RTE_FDIR_MODE_PERFECT) {
		PMD_DEBUG_TRACE("port %d: invalid FDIR mode=%u\n",
				port_id, dev->data->dev_conf.fdir_conf.mode);
		return (-ENOSYS);
	}

	if ((fdir_filter->l4type == RTE_FDIR_L4TYPE_SCTP
	     || fdir_filter->l4type == RTE_FDIR_L4TYPE_NONE)
	    && (fdir_filter->port_src || fdir_filter->port_dst)) {
		PMD_DEBUG_TRACE(" Port are meaningless for SCTP and " \
				"None l4type source & destinations ports " \
				"should be null!");
		return (-EINVAL);
	}

	/* For now IPv6 is not supported with perfect filter */
	if (fdir_filter->iptype == RTE_FDIR_IPTYPE_IPV6)
		return (-ENOTSUP);

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_remove_perfect_filter, -ENOTSUP);
	if  (*dev->dev_ops->fdir_remove_perfect_filter)
		return (*dev->dev_ops->fdir_remove_perfect_filter)(dev,
								   fdir_filter,
								   soft_id);

	PMD_DEBUG_TRACE("port %d: FDIR feature not supported\n", port_id);
	return -ENOTSUP;
}

int
rte_eth_dev_fdir_set_masks(uint8_t port_id, struct rte_fdir_masks *fdir_mask)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	if (! (dev->data->dev_conf.fdir_conf.mode)) {
		PMD_DEBUG_TRACE("port %d: pkt-filter disabled\n", port_id);
		return (-ENOSYS);
	}

	/* IPv6 mask are not supported */
	if (fdir_mask->src_ipv6_mask)
		return (-ENOTSUP);

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_set_masks, -ENOTSUP);
	if  (*dev->dev_ops->fdir_set_masks)
		return (*dev->dev_ops->fdir_set_masks)(dev, fdir_mask);

	PMD_DEBUG_TRACE("port %d: FDIR feature not supported\n",
			port_id);
	return -ENOTSUP;
}

int
rte_eth_dev_flow_ctrl_set(uint8_t port_id, struct rte_eth_fc_conf *fc_conf)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if ((fc_conf->send_xon != 0) && (fc_conf->send_xon != 1)) {
		PMD_DEBUG_TRACE("Invalid send_xon, only 0/1 allowed\n");
		return (-EINVAL);
	}

	dev = &rte_eth_devices[port_id];

	/* High water, low water validation are device specific */
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->flow_ctrl_set, -ENOTSUP);
	if  (*dev->dev_ops->flow_ctrl_set)
		return (*dev->dev_ops->flow_ctrl_set)(dev, fc_conf);

	return -ENOTSUP;
}

int
rte_eth_led_on(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_led_on, -ENOTSUP);
	return ((*dev->dev_ops->dev_led_on)(dev));
}

int
rte_eth_led_off(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_led_off, -ENOTSUP);
	return ((*dev->dev_ops->dev_led_off)(dev));
}

/*
 * Returns index into MAC address array of addr. Use 00:00:00:00:00:00 to find
 * an empty spot.
 */
static inline int
get_mac_addr_index(uint8_t port_id, struct ether_addr *addr)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	unsigned i;

	rte_eth_dev_info_get(port_id, &dev_info);

	for (i = 0; i < dev_info.max_mac_addrs; i++)
		if (memcmp(addr, &dev->data->mac_addrs[i], ETHER_ADDR_LEN) == 0)
			return i;

	return -1;
}

static struct ether_addr null_mac_addr = {{0, 0, 0, 0, 0, 0}};

int
rte_eth_dev_mac_addr_add(uint8_t port_id, struct ether_addr *addr,
		uint32_t pool)
{
	struct rte_eth_dev *dev;
	int index;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_add, -ENOTSUP);
	if (is_zero_ether_addr(addr)) {
		PMD_DEBUG_TRACE("port %d: Cannot add NULL MAC address\n", port_id);
		return (-EINVAL);
	}

	/* Check if it's already there, and do nothing */
	index = get_mac_addr_index(port_id, addr);
	if (index >= 0)
		return 0;

	index = get_mac_addr_index(port_id, &null_mac_addr);
	if (index < 0) {
		PMD_DEBUG_TRACE("port %d: MAC address array full\n", port_id);
		return (-ENOSPC);
	}

	/* Update NIC */
	(*dev->dev_ops->mac_addr_add)(dev, addr, index, pool);

	/* Update address in NIC data structure */
	ether_addr_copy(addr, &dev->data->mac_addrs[index]);

	return 0;
}

int
rte_eth_dev_mac_addr_remove(uint8_t port_id, struct ether_addr *addr)
{
	struct rte_eth_dev *dev;
	int index;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_remove, -ENOTSUP);
	index = get_mac_addr_index(port_id, addr);
	if (index == 0) {
		PMD_DEBUG_TRACE("port %d: Cannot remove default MAC address\n", port_id);
		return (-EADDRINUSE);
	} else if (index < 0)
		return 0;  /* Do nothing if address wasn't found */

	/* Update NIC */
	(*dev->dev_ops->mac_addr_remove)(dev, index);

	/* Update address in NIC data structure */
	ether_addr_copy(&null_mac_addr, &dev->data->mac_addrs[index]);

	return 0;
}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
uint16_t
rte_eth_rx_burst(uint8_t port_id, uint16_t queue_id,
		 struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return 0;
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->rx_pkt_burst, -ENOTSUP);
	if (queue_id >= dev->data->nb_rx_queues) {
		PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", queue_id);
		return 0;
	}
	return (*dev->rx_pkt_burst)(dev->data->rx_queues[queue_id],
						rx_pkts, nb_pkts);
}

uint16_t
rte_eth_tx_burst(uint8_t port_id, uint16_t queue_id,
		 struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct rte_eth_dev *dev;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return 0;
	}
	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->tx_pkt_burst, -ENOTSUP);
	if (queue_id >= dev->data->nb_tx_queues) {
		PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", queue_id);
		return 0;
	}
	return (*dev->tx_pkt_burst)(dev->data->tx_queues[queue_id],
						tx_pkts, nb_pkts);
}
#endif

int
rte_eth_dev_callback_register(uint8_t port_id,
			enum rte_eth_event_type event,
			rte_eth_dev_cb_fn cb_fn, void *cb_arg)
{
	int ret = -1;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_callback *user_cb = NULL;

	if (!cb_fn)
		return -1;
	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -1;
	}
	dev = &rte_eth_devices[port_id];
	rte_spinlock_lock(&rte_eth_dev_cb_lock);
	TAILQ_FOREACH(user_cb, &(dev->callbacks), next) {
		if (user_cb->cb_fn == cb_fn &&
			user_cb->cb_arg == cb_arg &&
			user_cb->event == event) {
			ret = 0;
			goto out;
		}
	}
	user_cb = rte_malloc("INTR_USER_CALLBACK",
		sizeof(struct rte_eth_dev_callback), 0);
	if (!user_cb)
		goto out;
	user_cb->cb_fn = cb_fn;
	user_cb->cb_arg = cb_arg;
	user_cb->event = event;
	TAILQ_INSERT_TAIL(&(dev->callbacks), user_cb, next);
	ret = 0;

out:
	rte_spinlock_unlock(&rte_eth_dev_cb_lock);

	return ret;
}

int
rte_eth_dev_callback_unregister(uint8_t port_id,
			enum rte_eth_event_type event,
			rte_eth_dev_cb_fn cb_fn, void *cb_arg)
{
	int ret = -1;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_callback *cb_lst = NULL;

	if (!cb_fn)
		return -1;
	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -1;
	}
	dev = &rte_eth_devices[port_id];
	rte_spinlock_lock(&rte_eth_dev_cb_lock);
	TAILQ_FOREACH(cb_lst, &(dev->callbacks), next) {
		if (cb_lst->cb_fn != cb_fn || cb_lst->event != event)
			continue;
		if (cb_lst->cb_arg == (void *)-1 ||
				cb_lst->cb_arg == cb_arg) {
			TAILQ_REMOVE(&(dev->callbacks), cb_lst, next);
			rte_free(cb_lst);
			ret = 0;
		}
	}

	rte_spinlock_unlock(&rte_eth_dev_cb_lock);

	return ret;
}

void
_rte_eth_dev_callback_process(struct rte_eth_dev *dev, enum rte_eth_event_type event)
{
	struct rte_eth_dev_callback *cb_lst = NULL;
	struct rte_eth_dev_callback dev_cb;

	rte_spinlock_lock(&rte_eth_dev_cb_lock);
	TAILQ_FOREACH(cb_lst, &(dev->callbacks), next) {
		if (cb_lst->cb_fn == NULL || cb_lst->event != event)
			continue;
		dev_cb = *cb_lst;
		rte_spinlock_unlock(&rte_eth_dev_cb_lock);
		dev_cb.cb_fn(dev->data->port_id, dev_cb.event,
						dev_cb.cb_arg);
		rte_spinlock_lock(&rte_eth_dev_cb_lock);
	}
	rte_spinlock_unlock(&rte_eth_dev_cb_lock);
}

