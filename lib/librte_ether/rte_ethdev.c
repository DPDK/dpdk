/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
#include <netinet/in.h>

#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
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
#include <rte_string_fns.h>

#include "rte_ether.h"
#include "rte_ethdev.h"

static const char *MZ_RTE_ETH_DEV_DATA = "rte_eth_dev_data";
struct rte_eth_dev rte_eth_devices[RTE_MAX_ETHPORTS];
static struct rte_eth_dev_data *rte_eth_dev_data;
static uint8_t nb_ports;

/* spinlock for eth device callbacks */
static rte_spinlock_t rte_eth_dev_cb_lock = RTE_SPINLOCK_INITIALIZER;

/* spinlock for add/remove rx callbacks */
static rte_spinlock_t rte_eth_rx_cb_lock = RTE_SPINLOCK_INITIALIZER;

/* spinlock for add/remove tx callbacks */
static rte_spinlock_t rte_eth_tx_cb_lock = RTE_SPINLOCK_INITIALIZER;

/* store statistics names and its offset in stats structure  */
struct rte_eth_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned offset;
};

static const struct rte_eth_xstats_name_off rte_stats_strings[] = {
	{"rx_good_packets", offsetof(struct rte_eth_stats, ipackets)},
	{"tx_good_packets", offsetof(struct rte_eth_stats, opackets)},
	{"rx_good_bytes", offsetof(struct rte_eth_stats, ibytes)},
	{"tx_good_bytes", offsetof(struct rte_eth_stats, obytes)},
	{"rx_errors", offsetof(struct rte_eth_stats, ierrors)},
	{"tx_errors", offsetof(struct rte_eth_stats, oerrors)},
	{"rx_mbuf_allocation_errors", offsetof(struct rte_eth_stats,
		rx_nombuf)},
};

#define RTE_NB_STATS (sizeof(rte_stats_strings) / sizeof(rte_stats_strings[0]))

static const struct rte_eth_xstats_name_off rte_rxq_stats_strings[] = {
	{"packets", offsetof(struct rte_eth_stats, q_ipackets)},
	{"bytes", offsetof(struct rte_eth_stats, q_ibytes)},
	{"errors", offsetof(struct rte_eth_stats, q_errors)},
};

#define RTE_NB_RXQ_STATS (sizeof(rte_rxq_stats_strings) /	\
		sizeof(rte_rxq_stats_strings[0]))

static const struct rte_eth_xstats_name_off rte_txq_stats_strings[] = {
	{"packets", offsetof(struct rte_eth_stats, q_opackets)},
	{"bytes", offsetof(struct rte_eth_stats, q_obytes)},
};
#define RTE_NB_TXQ_STATS (sizeof(rte_txq_stats_strings) /	\
		sizeof(rte_txq_stats_strings[0]))


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
	uint32_t active;                        /**< Callback is executing */
};

enum {
	STAT_QMAP_TX = 0,
	STAT_QMAP_RX
};

enum {
	DEV_DETACHED = 0,
	DEV_ATTACHED
};

static void
rte_eth_dev_data_alloc(void)
{
	const unsigned flags = 0;
	const struct rte_memzone *mz;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
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

struct rte_eth_dev *
rte_eth_dev_allocated(const char *name)
{
	unsigned i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if ((rte_eth_devices[i].attached == DEV_ATTACHED) &&
		    strcmp(rte_eth_devices[i].data->name, name) == 0)
			return &rte_eth_devices[i];
	}
	return NULL;
}

static uint8_t
rte_eth_dev_find_free_port(void)
{
	unsigned i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (rte_eth_devices[i].attached == DEV_DETACHED)
			return i;
	}
	return RTE_MAX_ETHPORTS;
}

struct rte_eth_dev *
rte_eth_dev_allocate(const char *name, enum rte_eth_dev_type type)
{
	uint8_t port_id;
	struct rte_eth_dev *eth_dev;

	port_id = rte_eth_dev_find_free_port();
	if (port_id == RTE_MAX_ETHPORTS) {
		RTE_PMD_DEBUG_TRACE("Reached maximum number of Ethernet ports\n");
		return NULL;
	}

	if (rte_eth_dev_data == NULL)
		rte_eth_dev_data_alloc();

	if (rte_eth_dev_allocated(name) != NULL) {
		RTE_PMD_DEBUG_TRACE("Ethernet Device with name %s already allocated!\n",
				name);
		return NULL;
	}

	eth_dev = &rte_eth_devices[port_id];
	eth_dev->data = &rte_eth_dev_data[port_id];
	snprintf(eth_dev->data->name, sizeof(eth_dev->data->name), "%s", name);
	eth_dev->data->port_id = port_id;
	eth_dev->attached = DEV_ATTACHED;
	eth_dev->dev_type = type;
	nb_ports++;
	return eth_dev;
}

static int
rte_eth_dev_create_unique_device_name(char *name, size_t size,
		struct rte_pci_device *pci_dev)
{
	int ret;

	ret = snprintf(name, size, "%d:%d.%d",
			pci_dev->addr.bus, pci_dev->addr.devid,
			pci_dev->addr.function);
	if (ret < 0)
		return ret;
	return 0;
}

int
rte_eth_dev_release_port(struct rte_eth_dev *eth_dev)
{
	if (eth_dev == NULL)
		return -EINVAL;

	eth_dev->attached = DEV_DETACHED;
	nb_ports--;
	return 0;
}

static int
rte_eth_dev_init(struct rte_pci_driver *pci_drv,
		 struct rte_pci_device *pci_dev)
{
	struct eth_driver    *eth_drv;
	struct rte_eth_dev *eth_dev;
	char ethdev_name[RTE_ETH_NAME_MAX_LEN];

	int diag;

	eth_drv = (struct eth_driver *)pci_drv;

	/* Create unique Ethernet device name using PCI address */
	rte_eth_dev_create_unique_device_name(ethdev_name,
			sizeof(ethdev_name), pci_dev);

	eth_dev = rte_eth_dev_allocate(ethdev_name, RTE_ETH_DEV_PCI);
	if (eth_dev == NULL)
		return -ENOMEM;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		eth_dev->data->dev_private = rte_zmalloc("ethdev private structure",
				  eth_drv->dev_private_size,
				  RTE_CACHE_LINE_SIZE);
		if (eth_dev->data->dev_private == NULL)
			rte_panic("Cannot allocate memzone for private port data\n");
	}
	eth_dev->pci_dev = pci_dev;
	eth_dev->driver = eth_drv;
	eth_dev->data->rx_mbuf_alloc_failed = 0;

	/* init user callbacks */
	TAILQ_INIT(&(eth_dev->link_intr_cbs));

	/*
	 * Set the default MTU.
	 */
	eth_dev->data->mtu = ETHER_MTU;

	/* Invoke PMD device initialization function */
	diag = (*eth_drv->eth_dev_init)(eth_dev);
	if (diag == 0)
		return 0;

	RTE_PMD_DEBUG_TRACE("driver %s: eth_dev_init(vendor_id=0x%u device_id=0x%x) failed\n",
			pci_drv->name,
			(unsigned) pci_dev->id.vendor_id,
			(unsigned) pci_dev->id.device_id);
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(eth_dev->data->dev_private);
	rte_eth_dev_release_port(eth_dev);
	return diag;
}

static int
rte_eth_dev_uninit(struct rte_pci_device *pci_dev)
{
	const struct eth_driver *eth_drv;
	struct rte_eth_dev *eth_dev;
	char ethdev_name[RTE_ETH_NAME_MAX_LEN];
	int ret;

	if (pci_dev == NULL)
		return -EINVAL;

	/* Create unique Ethernet device name using PCI address */
	rte_eth_dev_create_unique_device_name(ethdev_name,
			sizeof(ethdev_name), pci_dev);

	eth_dev = rte_eth_dev_allocated(ethdev_name);
	if (eth_dev == NULL)
		return -ENODEV;

	eth_drv = (const struct eth_driver *)pci_dev->driver;

	/* Invoke PMD device uninit function */
	if (*eth_drv->eth_dev_uninit) {
		ret = (*eth_drv->eth_dev_uninit)(eth_dev);
		if (ret)
			return ret;
	}

	/* free ether device */
	rte_eth_dev_release_port(eth_dev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(eth_dev->data->dev_private);

	eth_dev->pci_dev = NULL;
	eth_dev->driver = NULL;
	eth_dev->data = NULL;

	return 0;
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
	eth_drv->pci_drv.devuninit = rte_eth_dev_uninit;
	rte_eal_pci_register(&eth_drv->pci_drv);
}

int
rte_eth_dev_is_valid_port(uint8_t port_id)
{
	if (port_id >= RTE_MAX_ETHPORTS ||
	    rte_eth_devices[port_id].attached != DEV_ATTACHED)
		return 0;
	else
		return 1;
}

int
rte_eth_dev_socket_id(uint8_t port_id)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -1);
	return rte_eth_devices[port_id].data->numa_node;
}

uint8_t
rte_eth_dev_count(void)
{
	return nb_ports;
}

static enum rte_eth_dev_type
rte_eth_dev_get_device_type(uint8_t port_id)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, RTE_ETH_DEV_UNKNOWN);
	return rte_eth_devices[port_id].dev_type;
}

static int
rte_eth_dev_get_addr_by_port(uint8_t port_id, struct rte_pci_addr *addr)
{
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	if (addr == NULL) {
		RTE_PMD_DEBUG_TRACE("Null pointer is specified\n");
		return -EINVAL;
	}

	*addr = rte_eth_devices[port_id].pci_dev->addr;
	return 0;
}

int
rte_eth_dev_get_name_by_port(uint8_t port_id, char *name)
{
	char *tmp;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	if (name == NULL) {
		RTE_PMD_DEBUG_TRACE("Null pointer is specified\n");
		return -EINVAL;
	}

	/* shouldn't check 'rte_eth_devices[i].data',
	 * because it might be overwritten by VDEV PMD */
	tmp = rte_eth_dev_data[port_id].name;
	strcpy(name, tmp);
	return 0;
}

int
rte_eth_dev_get_port_by_name(const char *name, uint8_t *port_id)
{
	int i;

	if (name == NULL) {
		RTE_PMD_DEBUG_TRACE("Null pointer is specified\n");
		return -EINVAL;
	}

	*port_id = RTE_MAX_ETHPORTS;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {

		if (!strncmp(name,
			rte_eth_dev_data[i].name, strlen(name))) {

			*port_id = i;

			return 0;
		}
	}
	return -ENODEV;
}

static int
rte_eth_dev_get_port_by_addr(const struct rte_pci_addr *addr, uint8_t *port_id)
{
	int i;
	struct rte_pci_device *pci_dev = NULL;

	if (addr == NULL) {
		RTE_PMD_DEBUG_TRACE("Null pointer is specified\n");
		return -EINVAL;
	}

	*port_id = RTE_MAX_ETHPORTS;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {

		pci_dev = rte_eth_devices[i].pci_dev;

		if (pci_dev &&
			!rte_eal_compare_pci_addr(&pci_dev->addr, addr)) {

			*port_id = i;

			return 0;
		}
	}
	return -ENODEV;
}

static int
rte_eth_dev_is_detachable(uint8_t port_id)
{
	uint32_t dev_flags;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	switch (rte_eth_devices[port_id].data->kdrv) {
	case RTE_KDRV_IGB_UIO:
	case RTE_KDRV_UIO_GENERIC:
	case RTE_KDRV_NIC_UIO:
	case RTE_KDRV_NONE:
		break;
	case RTE_KDRV_VFIO:
	default:
		return -ENOTSUP;
	}
	dev_flags = rte_eth_devices[port_id].data->dev_flags;
	if ((dev_flags & RTE_ETH_DEV_DETACHABLE) &&
		(!(dev_flags & RTE_ETH_DEV_BONDED_SLAVE)))
		return 0;
	else
		return 1;
}

/* attach the new physical device, then store port_id of the device */
static int
rte_eth_dev_attach_pdev(struct rte_pci_addr *addr, uint8_t *port_id)
{
	/* re-construct pci_device_list */
	if (rte_eal_pci_scan())
		goto err;
	/* Invoke probe func of the driver can handle the new device. */
	if (rte_eal_pci_probe_one(addr))
		goto err;

	if (rte_eth_dev_get_port_by_addr(addr, port_id))
		goto err;

	return 0;
err:
	return -1;
}

/* detach the new physical device, then store pci_addr of the device */
static int
rte_eth_dev_detach_pdev(uint8_t port_id, struct rte_pci_addr *addr)
{
	struct rte_pci_addr freed_addr;
	struct rte_pci_addr vp;

	/* get pci address by port id */
	if (rte_eth_dev_get_addr_by_port(port_id, &freed_addr))
		goto err;

	/* Zeroed pci addr means the port comes from virtual device */
	vp.domain = vp.bus = vp.devid = vp.function = 0;
	if (rte_eal_compare_pci_addr(&vp, &freed_addr) == 0)
		goto err;

	/* invoke devuninit func of the pci driver,
	 * also remove the device from pci_device_list */
	if (rte_eal_pci_detach(&freed_addr))
		goto err;

	*addr = freed_addr;
	return 0;
err:
	return -1;
}

/* attach the new virtual device, then store port_id of the device */
static int
rte_eth_dev_attach_vdev(const char *vdevargs, uint8_t *port_id)
{
	char *name = NULL, *args = NULL;
	int ret = -1;

	/* parse vdevargs, then retrieve device name and args */
	if (rte_eal_parse_devargs_str(vdevargs, &name, &args))
		goto end;

	/* walk around dev_driver_list to find the driver of the device,
	 * then invoke probe function of the driver.
	 * rte_eal_vdev_init() updates port_id allocated after
	 * initialization.
	 */
	if (rte_eal_vdev_init(name, args))
		goto end;

	if (rte_eth_dev_get_port_by_name(name, port_id))
		goto end;

	ret = 0;
end:
	free(name);
	free(args);

	return ret;
}

/* detach the new virtual device, then store the name of the device */
static int
rte_eth_dev_detach_vdev(uint8_t port_id, char *vdevname)
{
	char name[RTE_ETH_NAME_MAX_LEN];

	/* get device name by port id */
	if (rte_eth_dev_get_name_by_port(port_id, name))
		goto err;
	/* walk around dev_driver_list to find the driver of the device,
	 * then invoke uninit function of the driver */
	if (rte_eal_vdev_uninit(name))
		goto err;

	strncpy(vdevname, name, sizeof(name));
	return 0;
err:
	return -1;
}

/* attach the new device, then store port_id of the device */
int
rte_eth_dev_attach(const char *devargs, uint8_t *port_id)
{
	struct rte_pci_addr addr;
	int ret = -1;

	if ((devargs == NULL) || (port_id == NULL)) {
		ret = -EINVAL;
		goto err;
	}

	if (eal_parse_pci_DomBDF(devargs, &addr) == 0) {
		ret = rte_eth_dev_attach_pdev(&addr, port_id);
		if (ret < 0)
			goto err;
	} else {
		ret = rte_eth_dev_attach_vdev(devargs, port_id);
		if (ret < 0)
			goto err;
	}

	return 0;
err:
	RTE_LOG(ERR, EAL, "Driver, cannot attach the device\n");
	return ret;
}

/* detach the device, then store the name of the device */
int
rte_eth_dev_detach(uint8_t port_id, char *name)
{
	struct rte_pci_addr addr;
	int ret = -1;

	if (name == NULL) {
		ret = -EINVAL;
		goto err;
	}

	/* check whether the driver supports detach feature, or not */
	if (rte_eth_dev_is_detachable(port_id))
		goto err;

	if (rte_eth_dev_get_device_type(port_id) == RTE_ETH_DEV_PCI) {
		ret = rte_eth_dev_get_addr_by_port(port_id, &addr);
		if (ret < 0)
			goto err;

		ret = rte_eth_dev_detach_pdev(port_id, &addr);
		if (ret < 0)
			goto err;

		snprintf(name, RTE_ETH_NAME_MAX_LEN,
			"%04x:%02x:%02x.%d",
			addr.domain, addr.bus,
			addr.devid, addr.function);
	} else {
		ret = rte_eth_dev_detach_vdev(port_id, name);
		if (ret < 0)
			goto err;
	}

	return 0;

err:
	RTE_LOG(ERR, EAL, "Driver, cannot detach the device\n");
	return ret;
}

static int
rte_eth_dev_rx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	uint16_t old_nb_queues = dev->data->nb_rx_queues;
	void **rxq;
	unsigned i;

	if (dev->data->rx_queues == NULL && nb_queues != 0) { /* first time configuration */
		dev->data->rx_queues = rte_zmalloc("ethdev->rx_queues",
				sizeof(dev->data->rx_queues[0]) * nb_queues,
				RTE_CACHE_LINE_SIZE);
		if (dev->data->rx_queues == NULL) {
			dev->data->nb_rx_queues = 0;
			return -(ENOMEM);
		}
	} else if (dev->data->rx_queues != NULL && nb_queues != 0) { /* re-configure */
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_release, -ENOTSUP);

		rxq = dev->data->rx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->rx_queue_release)(rxq[i]);
		rxq = rte_realloc(rxq, sizeof(rxq[0]) * nb_queues,
				RTE_CACHE_LINE_SIZE);
		if (rxq == NULL)
			return -(ENOMEM);
		if (nb_queues > old_nb_queues) {
			uint16_t new_qs = nb_queues - old_nb_queues;

			memset(rxq + old_nb_queues, 0,
				sizeof(rxq[0]) * new_qs);
		}

		dev->data->rx_queues = rxq;

	} else if (dev->data->rx_queues != NULL && nb_queues == 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_release, -ENOTSUP);

		rxq = dev->data->rx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->rx_queue_release)(rxq[i]);
	}
	dev->data->nb_rx_queues = nb_queues;
	return 0;
}

int
rte_eth_dev_rx_queue_start(uint8_t port_id, uint16_t rx_queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", rx_queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_start, -ENOTSUP);

	if (dev->data->rx_queue_state[rx_queue_id] != RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_PMD_DEBUG_TRACE("Queue %" PRIu16" of device with port_id=%" PRIu8
			" already started\n",
			rx_queue_id, port_id);
		return 0;
	}

	return dev->dev_ops->rx_queue_start(dev, rx_queue_id);

}

int
rte_eth_dev_rx_queue_stop(uint8_t port_id, uint16_t rx_queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", rx_queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_stop, -ENOTSUP);

	if (dev->data->rx_queue_state[rx_queue_id] == RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_PMD_DEBUG_TRACE("Queue %" PRIu16" of device with port_id=%" PRIu8
			" already stopped\n",
			rx_queue_id, port_id);
		return 0;
	}

	return dev->dev_ops->rx_queue_stop(dev, rx_queue_id);

}

int
rte_eth_dev_tx_queue_start(uint8_t port_id, uint16_t tx_queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", tx_queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_start, -ENOTSUP);

	if (dev->data->tx_queue_state[tx_queue_id] != RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_PMD_DEBUG_TRACE("Queue %" PRIu16" of device with port_id=%" PRIu8
			" already started\n",
			tx_queue_id, port_id);
		return 0;
	}

	return dev->dev_ops->tx_queue_start(dev, tx_queue_id);

}

int
rte_eth_dev_tx_queue_stop(uint8_t port_id, uint16_t tx_queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", tx_queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_stop, -ENOTSUP);

	if (dev->data->tx_queue_state[tx_queue_id] == RTE_ETH_QUEUE_STATE_STOPPED) {
		RTE_PMD_DEBUG_TRACE("Queue %" PRIu16" of device with port_id=%" PRIu8
			" already stopped\n",
			tx_queue_id, port_id);
		return 0;
	}

	return dev->dev_ops->tx_queue_stop(dev, tx_queue_id);

}

static int
rte_eth_dev_tx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	uint16_t old_nb_queues = dev->data->nb_tx_queues;
	void **txq;
	unsigned i;

	if (dev->data->tx_queues == NULL && nb_queues != 0) { /* first time configuration */
		dev->data->tx_queues = rte_zmalloc("ethdev->tx_queues",
						   sizeof(dev->data->tx_queues[0]) * nb_queues,
						   RTE_CACHE_LINE_SIZE);
		if (dev->data->tx_queues == NULL) {
			dev->data->nb_tx_queues = 0;
			return -(ENOMEM);
		}
	} else if (dev->data->tx_queues != NULL && nb_queues != 0) { /* re-configure */
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_release, -ENOTSUP);

		txq = dev->data->tx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->tx_queue_release)(txq[i]);
		txq = rte_realloc(txq, sizeof(txq[0]) * nb_queues,
				  RTE_CACHE_LINE_SIZE);
		if (txq == NULL)
			return -ENOMEM;
		if (nb_queues > old_nb_queues) {
			uint16_t new_qs = nb_queues - old_nb_queues;

			memset(txq + old_nb_queues, 0,
			       sizeof(txq[0]) * new_qs);
		}

		dev->data->tx_queues = txq;

	} else if (dev->data->tx_queues != NULL && nb_queues == 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_release, -ENOTSUP);

		txq = dev->data->tx_queues;

		for (i = nb_queues; i < old_nb_queues; i++)
			(*dev->dev_ops->tx_queue_release)(txq[i]);
	}
	dev->data->nb_tx_queues = nb_queues;
	return 0;
}

uint32_t
rte_eth_speed_bitflag(uint32_t speed, int duplex)
{
	switch (speed) {
	case ETH_SPEED_NUM_10M:
		return duplex ? ETH_LINK_SPEED_10M : ETH_LINK_SPEED_10M_HD;
	case ETH_SPEED_NUM_100M:
		return duplex ? ETH_LINK_SPEED_100M : ETH_LINK_SPEED_100M_HD;
	case ETH_SPEED_NUM_1G:
		return ETH_LINK_SPEED_1G;
	case ETH_SPEED_NUM_2_5G:
		return ETH_LINK_SPEED_2_5G;
	case ETH_SPEED_NUM_5G:
		return ETH_LINK_SPEED_5G;
	case ETH_SPEED_NUM_10G:
		return ETH_LINK_SPEED_10G;
	case ETH_SPEED_NUM_20G:
		return ETH_LINK_SPEED_20G;
	case ETH_SPEED_NUM_25G:
		return ETH_LINK_SPEED_25G;
	case ETH_SPEED_NUM_40G:
		return ETH_LINK_SPEED_40G;
	case ETH_SPEED_NUM_50G:
		return ETH_LINK_SPEED_50G;
	case ETH_SPEED_NUM_56G:
		return ETH_LINK_SPEED_56G;
	case ETH_SPEED_NUM_100G:
		return ETH_LINK_SPEED_100G;
	default:
		return 0;
	}
}

int
rte_eth_dev_configure(uint8_t port_id, uint16_t nb_rx_q, uint16_t nb_tx_q,
		      const struct rte_eth_conf *dev_conf)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	int diag;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	if (nb_rx_q > RTE_MAX_QUEUES_PER_PORT) {
		RTE_PMD_DEBUG_TRACE(
			"Number of RX queues requested (%u) is greater than max supported(%d)\n",
			nb_rx_q, RTE_MAX_QUEUES_PER_PORT);
		return -EINVAL;
	}

	if (nb_tx_q > RTE_MAX_QUEUES_PER_PORT) {
		RTE_PMD_DEBUG_TRACE(
			"Number of TX queues requested (%u) is greater than max supported(%d)\n",
			nb_tx_q, RTE_MAX_QUEUES_PER_PORT);
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_configure, -ENOTSUP);

	if (dev->data->dev_started) {
		RTE_PMD_DEBUG_TRACE(
		    "port %d must be stopped to allow configuration\n", port_id);
		return -EBUSY;
	}

	/* Copy the dev_conf parameter into the dev structure */
	memcpy(&dev->data->dev_conf, dev_conf, sizeof(dev->data->dev_conf));

	/*
	 * Check that the numbers of RX and TX queues are not greater
	 * than the maximum number of RX and TX queues supported by the
	 * configured device.
	 */
	(*dev->dev_ops->dev_infos_get)(dev, &dev_info);

	if (nb_rx_q == 0 && nb_tx_q == 0) {
		RTE_PMD_DEBUG_TRACE("ethdev port_id=%d both rx and tx queue cannot be 0\n", port_id);
		return -EINVAL;
	}

	if (nb_rx_q > dev_info.max_rx_queues) {
		RTE_PMD_DEBUG_TRACE("ethdev port_id=%d nb_rx_queues=%d > %d\n",
				port_id, nb_rx_q, dev_info.max_rx_queues);
		return -EINVAL;
	}

	if (nb_tx_q > dev_info.max_tx_queues) {
		RTE_PMD_DEBUG_TRACE("ethdev port_id=%d nb_tx_queues=%d > %d\n",
				port_id, nb_tx_q, dev_info.max_tx_queues);
		return -EINVAL;
	}

	/*
	 * If link state interrupt is enabled, check that the
	 * device supports it.
	 */
	if ((dev_conf->intr_conf.lsc == 1) &&
		(!(dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC))) {
			RTE_PMD_DEBUG_TRACE("driver %s does not support lsc\n",
					dev->data->drv_name);
			return -EINVAL;
	}

	/*
	 * If jumbo frames are enabled, check that the maximum RX packet
	 * length is supported by the configured device.
	 */
	if (dev_conf->rxmode.jumbo_frame == 1) {
		if (dev_conf->rxmode.max_rx_pkt_len >
		    dev_info.max_rx_pktlen) {
			RTE_PMD_DEBUG_TRACE("ethdev port_id=%d max_rx_pkt_len %u"
				" > max valid value %u\n",
				port_id,
				(unsigned)dev_conf->rxmode.max_rx_pkt_len,
				(unsigned)dev_info.max_rx_pktlen);
			return -EINVAL;
		} else if (dev_conf->rxmode.max_rx_pkt_len < ETHER_MIN_LEN) {
			RTE_PMD_DEBUG_TRACE("ethdev port_id=%d max_rx_pkt_len %u"
				" < min valid value %u\n",
				port_id,
				(unsigned)dev_conf->rxmode.max_rx_pkt_len,
				(unsigned)ETHER_MIN_LEN);
			return -EINVAL;
		}
	} else {
		if (dev_conf->rxmode.max_rx_pkt_len < ETHER_MIN_LEN ||
			dev_conf->rxmode.max_rx_pkt_len > ETHER_MAX_LEN)
			/* Use default value */
			dev->data->dev_conf.rxmode.max_rx_pkt_len =
							ETHER_MAX_LEN;
	}

	/*
	 * Setup new number of RX/TX queues and reconfigure device.
	 */
	diag = rte_eth_dev_rx_queue_config(dev, nb_rx_q);
	if (diag != 0) {
		RTE_PMD_DEBUG_TRACE("port%d rte_eth_dev_rx_queue_config = %d\n",
				port_id, diag);
		return diag;
	}

	diag = rte_eth_dev_tx_queue_config(dev, nb_tx_q);
	if (diag != 0) {
		RTE_PMD_DEBUG_TRACE("port%d rte_eth_dev_tx_queue_config = %d\n",
				port_id, diag);
		rte_eth_dev_rx_queue_config(dev, 0);
		return diag;
	}

	diag = (*dev->dev_ops->dev_configure)(dev);
	if (diag != 0) {
		RTE_PMD_DEBUG_TRACE("port%d dev_configure = %d\n",
				port_id, diag);
		rte_eth_dev_rx_queue_config(dev, 0);
		rte_eth_dev_tx_queue_config(dev, 0);
		return diag;
	}

	return 0;
}

static void
rte_eth_dev_config_restore(uint8_t port_id)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct ether_addr addr;
	uint16_t i;
	uint32_t pool = 0;

	dev = &rte_eth_devices[port_id];

	rte_eth_dev_info_get(port_id, &dev_info);

	if (RTE_ETH_DEV_SRIOV(dev).active)
		pool = RTE_ETH_DEV_SRIOV(dev).def_vmdq_idx;

	/* replay MAC address configuration */
	for (i = 0; i < dev_info.max_mac_addrs; i++) {
		addr = dev->data->mac_addrs[i];

		/* skip zero address */
		if (is_zero_ether_addr(&addr))
			continue;

		/* add address to the hardware */
		if  (*dev->dev_ops->mac_addr_add &&
			(dev->data->mac_pool_sel[i] & (1ULL << pool)))
			(*dev->dev_ops->mac_addr_add)(dev, &addr, i, pool);
		else {
			RTE_PMD_DEBUG_TRACE("port %d: MAC address array not supported\n",
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

	/* replay all multicast configuration */
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

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_start, -ENOTSUP);

	if (dev->data->dev_started != 0) {
		RTE_PMD_DEBUG_TRACE("Device with port_id=%" PRIu8
			" already started\n",
			port_id);
		return 0;
	}

	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	rte_eth_dev_config_restore(port_id);

	if (dev->data->dev_conf.intr_conf.lsc == 0) {
		RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->link_update, -ENOTSUP);
		(*dev->dev_ops->link_update)(dev, 0);
	}
	return 0;
}

void
rte_eth_dev_stop(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_stop);

	if (dev->data->dev_started == 0) {
		RTE_PMD_DEBUG_TRACE("Device with port_id=%" PRIu8
			" already stopped\n",
			port_id);
		return;
	}

	dev->data->dev_started = 0;
	(*dev->dev_ops->dev_stop)(dev);
}

int
rte_eth_dev_set_link_up(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_set_link_up, -ENOTSUP);
	return (*dev->dev_ops->dev_set_link_up)(dev);
}

int
rte_eth_dev_set_link_down(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_set_link_down, -ENOTSUP);
	return (*dev->dev_ops->dev_set_link_down)(dev);
}

void
rte_eth_dev_close(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_close);
	dev->data->dev_started = 0;
	(*dev->dev_ops->dev_close)(dev);

	rte_free(dev->data->rx_queues);
	dev->data->rx_queues = NULL;
	rte_free(dev->data->tx_queues);
	dev->data->tx_queues = NULL;
}

int
rte_eth_rx_queue_setup(uint8_t port_id, uint16_t rx_queue_id,
		       uint16_t nb_rx_desc, unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mp)
{
	int ret;
	uint32_t mbp_buf_size;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", rx_queue_id);
		return -EINVAL;
	}

	if (dev->data->dev_started) {
		RTE_PMD_DEBUG_TRACE(
		    "port %d must be stopped to allow configuration\n", port_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_setup, -ENOTSUP);

	/*
	 * Check the size of the mbuf data buffer.
	 * This value must be provided in the private data of the memory pool.
	 * First check that the memory pool has a valid private data.
	 */
	rte_eth_dev_info_get(port_id, &dev_info);
	if (mp->private_data_size < sizeof(struct rte_pktmbuf_pool_private)) {
		RTE_PMD_DEBUG_TRACE("%s private_data_size %d < %d\n",
				mp->name, (int) mp->private_data_size,
				(int) sizeof(struct rte_pktmbuf_pool_private));
		return -ENOSPC;
	}
	mbp_buf_size = rte_pktmbuf_data_room_size(mp);

	if ((mbp_buf_size - RTE_PKTMBUF_HEADROOM) < dev_info.min_rx_bufsize) {
		RTE_PMD_DEBUG_TRACE("%s mbuf_data_room_size %d < %d "
				"(RTE_PKTMBUF_HEADROOM=%d + min_rx_bufsize(dev)"
				"=%d)\n",
				mp->name,
				(int)mbp_buf_size,
				(int)(RTE_PKTMBUF_HEADROOM +
				      dev_info.min_rx_bufsize),
				(int)RTE_PKTMBUF_HEADROOM,
				(int)dev_info.min_rx_bufsize);
		return -EINVAL;
	}

	if (nb_rx_desc > dev_info.rx_desc_lim.nb_max ||
			nb_rx_desc < dev_info.rx_desc_lim.nb_min ||
			nb_rx_desc % dev_info.rx_desc_lim.nb_align != 0) {

		RTE_PMD_DEBUG_TRACE("Invalid value for nb_rx_desc(=%hu), "
			"should be: <= %hu, = %hu, and a product of %hu\n",
			nb_rx_desc,
			dev_info.rx_desc_lim.nb_max,
			dev_info.rx_desc_lim.nb_min,
			dev_info.rx_desc_lim.nb_align);
		return -EINVAL;
	}

	if (rx_conf == NULL)
		rx_conf = &dev_info.default_rxconf;

	ret = (*dev->dev_ops->rx_queue_setup)(dev, rx_queue_id, nb_rx_desc,
					      socket_id, rx_conf, mp);
	if (!ret) {
		if (!dev->data->min_rx_buf_size ||
		    dev->data->min_rx_buf_size > mbp_buf_size)
			dev->data->min_rx_buf_size = mbp_buf_size;
	}

	return ret;
}

int
rte_eth_tx_queue_setup(uint8_t port_id, uint16_t tx_queue_id,
		       uint16_t nb_tx_desc, unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", tx_queue_id);
		return -EINVAL;
	}

	if (dev->data->dev_started) {
		RTE_PMD_DEBUG_TRACE(
		    "port %d must be stopped to allow configuration\n", port_id);
		return -EBUSY;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_setup, -ENOTSUP);

	rte_eth_dev_info_get(port_id, &dev_info);

	if (nb_tx_desc > dev_info.tx_desc_lim.nb_max ||
	    nb_tx_desc < dev_info.tx_desc_lim.nb_min ||
	    nb_tx_desc % dev_info.tx_desc_lim.nb_align != 0) {
		RTE_PMD_DEBUG_TRACE("Invalid value for nb_tx_desc(=%hu), "
				"should be: <= %hu, = %hu, and a product of %hu\n",
				nb_tx_desc,
				dev_info.tx_desc_lim.nb_max,
				dev_info.tx_desc_lim.nb_min,
				dev_info.tx_desc_lim.nb_align);
		return -EINVAL;
	}

	if (tx_conf == NULL)
		tx_conf = &dev_info.default_txconf;

	return (*dev->dev_ops->tx_queue_setup)(dev, tx_queue_id, nb_tx_desc,
					       socket_id, tx_conf);
}

void
rte_eth_tx_buffer_drop_callback(struct rte_mbuf **pkts, uint16_t unsent,
		void *userdata __rte_unused)
{
	unsigned i;

	for (i = 0; i < unsent; i++)
		rte_pktmbuf_free(pkts[i]);
}

void
rte_eth_tx_buffer_count_callback(struct rte_mbuf **pkts, uint16_t unsent,
		void *userdata)
{
	uint64_t *count = userdata;
	unsigned i;

	for (i = 0; i < unsent; i++)
		rte_pktmbuf_free(pkts[i]);

	*count += unsent;
}

int
rte_eth_tx_buffer_set_err_callback(struct rte_eth_dev_tx_buffer *buffer,
		buffer_tx_error_fn cbfn, void *userdata)
{
	buffer->error_callback = cbfn;
	buffer->error_userdata = userdata;
	return 0;
}

int
rte_eth_tx_buffer_init(struct rte_eth_dev_tx_buffer *buffer, uint16_t size)
{
	int ret = 0;

	if (buffer == NULL)
		return -EINVAL;

	buffer->size = size;
	if (buffer->error_callback == NULL) {
		ret = rte_eth_tx_buffer_set_err_callback(
			buffer, rte_eth_tx_buffer_drop_callback, NULL);
	}

	return ret;
}

void
rte_eth_promiscuous_enable(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->promiscuous_enable);
	(*dev->dev_ops->promiscuous_enable)(dev);
	dev->data->promiscuous = 1;
}

void
rte_eth_promiscuous_disable(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->promiscuous_disable);
	dev->data->promiscuous = 0;
	(*dev->dev_ops->promiscuous_disable)(dev);
}

int
rte_eth_promiscuous_get(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	return dev->data->promiscuous;
}

void
rte_eth_allmulticast_enable(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->allmulticast_enable);
	(*dev->dev_ops->allmulticast_enable)(dev);
	dev->data->all_multicast = 1;
}

void
rte_eth_allmulticast_disable(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->allmulticast_disable);
	dev->data->all_multicast = 0;
	(*dev->dev_ops->allmulticast_disable)(dev);
}

int
rte_eth_allmulticast_get(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

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

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.intr_conf.lsc != 0)
		rte_eth_dev_atomic_read_link_status(dev, eth_link);
	else {
		RTE_FUNC_PTR_OR_RET(*dev->dev_ops->link_update);
		(*dev->dev_ops->link_update)(dev, 1);
		*eth_link = dev->data->dev_link;
	}
}

void
rte_eth_link_get_nowait(uint8_t port_id, struct rte_eth_link *eth_link)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.intr_conf.lsc != 0)
		rte_eth_dev_atomic_read_link_status(dev, eth_link);
	else {
		RTE_FUNC_PTR_OR_RET(*dev->dev_ops->link_update);
		(*dev->dev_ops->link_update)(dev, 0);
		*eth_link = dev->data->dev_link;
	}
}

int
rte_eth_stats_get(uint8_t port_id, struct rte_eth_stats *stats)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	memset(stats, 0, sizeof(*stats));

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_get, -ENOTSUP);
	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
	(*dev->dev_ops->stats_get)(dev, stats);
	return 0;
}

void
rte_eth_stats_reset(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->stats_reset);
	(*dev->dev_ops->stats_reset)(dev);
	dev->data->rx_mbuf_alloc_failed = 0;
}

static int
get_xstats_count(uint8_t port_id)
{
	struct rte_eth_dev *dev;
	int count;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);
	dev = &rte_eth_devices[port_id];
	if (dev->dev_ops->xstats_get_names != NULL) {
		count = (*dev->dev_ops->xstats_get_names)(dev, NULL, 0);
		if (count < 0)
			return count;
	} else
		count = 0;
	count += RTE_NB_STATS;
	count += dev->data->nb_rx_queues * RTE_NB_RXQ_STATS;
	count += dev->data->nb_tx_queues * RTE_NB_TXQ_STATS;
	return count;
}

int
rte_eth_xstats_get_names(uint8_t port_id,
	struct rte_eth_xstat_name *xstats_names,
	unsigned size)
{
	struct rte_eth_dev *dev;
	int cnt_used_entries;
	int cnt_expected_entries;
	int cnt_driver_entries;
	uint32_t idx, id_queue;

	cnt_expected_entries = get_xstats_count(port_id);
	if (xstats_names == NULL || cnt_expected_entries < 0 ||
			(int)size < cnt_expected_entries)
		return cnt_expected_entries;

	/* port_id checked in get_xstats_count() */
	dev = &rte_eth_devices[port_id];
	cnt_used_entries = 0;

	for (idx = 0; idx < RTE_NB_STATS; idx++) {
		snprintf(xstats_names[cnt_used_entries].name,
			sizeof(xstats_names[0].name),
			"%s", rte_stats_strings[idx].name);
		cnt_used_entries++;
	}
	for (id_queue = 0; id_queue < dev->data->nb_rx_queues; id_queue++) {
		for (idx = 0; idx < RTE_NB_RXQ_STATS; idx++) {
			snprintf(xstats_names[cnt_used_entries].name,
				sizeof(xstats_names[0].name),
				"rx_q%u%s",
				id_queue, rte_rxq_stats_strings[idx].name);
			cnt_used_entries++;
		}

	}
	for (id_queue = 0; id_queue < dev->data->nb_tx_queues; id_queue++) {
		for (idx = 0; idx < RTE_NB_TXQ_STATS; idx++) {
			snprintf(xstats_names[cnt_used_entries].name,
				sizeof(xstats_names[0].name),
				"tx_q%u%s",
				id_queue, rte_txq_stats_strings[idx].name);
			cnt_used_entries++;
		}
	}

	if (dev->dev_ops->xstats_get_names != NULL) {
		/* If there are any driver-specific xstats, append them
		 * to end of list.
		 */
		cnt_driver_entries = (*dev->dev_ops->xstats_get_names)(
			dev,
			xstats_names + cnt_used_entries,
			size - cnt_used_entries);
		if (cnt_driver_entries < 0)
			return cnt_driver_entries;
		cnt_used_entries += cnt_driver_entries;
	}

	return cnt_used_entries;
}

/* retrieve ethdev extended statistics */
int
rte_eth_xstats_get(uint8_t port_id, struct rte_eth_xstat *xstats,
	unsigned n)
{
	struct rte_eth_stats eth_stats;
	struct rte_eth_dev *dev;
	unsigned count = 0, i, q;
	signed xcount = 0;
	uint64_t val, *stats_ptr;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];

	/* Return generic statistics */
	count = RTE_NB_STATS + (dev->data->nb_rx_queues * RTE_NB_RXQ_STATS) +
		(dev->data->nb_tx_queues * RTE_NB_TXQ_STATS);

	/* implemented by the driver */
	if (dev->dev_ops->xstats_get != NULL) {
		/* Retrieve the xstats from the driver at the end of the
		 * xstats struct.
		 */
		xcount = (*dev->dev_ops->xstats_get)(dev,
				     xstats ? xstats + count : NULL,
				     (n > count) ? n - count : 0);

		if (xcount < 0)
			return xcount;
	}

	if (n < count + xcount || xstats == NULL)
		return count + xcount;

	/* now fill the xstats structure */
	count = 0;
	rte_eth_stats_get(port_id, &eth_stats);

	/* global stats */
	for (i = 0; i < RTE_NB_STATS; i++) {
		stats_ptr = RTE_PTR_ADD(&eth_stats,
					rte_stats_strings[i].offset);
		val = *stats_ptr;
		xstats[count++].value = val;
	}

	/* per-rxq stats */
	for (q = 0; q < dev->data->nb_rx_queues; q++) {
		for (i = 0; i < RTE_NB_RXQ_STATS; i++) {
			stats_ptr = RTE_PTR_ADD(&eth_stats,
					rte_rxq_stats_strings[i].offset +
					q * sizeof(uint64_t));
			val = *stats_ptr;
			xstats[count++].value = val;
		}
	}

	/* per-txq stats */
	for (q = 0; q < dev->data->nb_tx_queues; q++) {
		for (i = 0; i < RTE_NB_TXQ_STATS; i++) {
			stats_ptr = RTE_PTR_ADD(&eth_stats,
					rte_txq_stats_strings[i].offset +
					q * sizeof(uint64_t));
			val = *stats_ptr;
			xstats[count++].value = val;
		}
	}

	for (i = 0; i < count + xcount; i++)
		xstats[i].id = i;

	return count + xcount;
}

/* reset ethdev extended statistics */
void
rte_eth_xstats_reset(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	/* implemented by the driver */
	if (dev->dev_ops->xstats_reset != NULL) {
		(*dev->dev_ops->xstats_reset)(dev);
		return;
	}

	/* fallback to default */
	rte_eth_stats_reset(port_id);
}

static int
set_queue_stats_mapping(uint8_t port_id, uint16_t queue_id, uint8_t stat_idx,
		uint8_t is_rx)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_stats_mapping_set, -ENOTSUP);
	return (*dev->dev_ops->queue_stats_mapping_set)
			(dev, queue_id, stat_idx, is_rx);
}


int
rte_eth_dev_set_tx_queue_stats_mapping(uint8_t port_id, uint16_t tx_queue_id,
		uint8_t stat_idx)
{
	return set_queue_stats_mapping(port_id, tx_queue_id, stat_idx,
			STAT_QMAP_TX);
}


int
rte_eth_dev_set_rx_queue_stats_mapping(uint8_t port_id, uint16_t rx_queue_id,
		uint8_t stat_idx)
{
	return set_queue_stats_mapping(port_id, rx_queue_id, stat_idx,
			STAT_QMAP_RX);
}

void
rte_eth_dev_info_get(uint8_t port_id, struct rte_eth_dev_info *dev_info)
{
	struct rte_eth_dev *dev;
	const struct rte_eth_desc_lim lim = {
		.nb_max = UINT16_MAX,
		.nb_min = 0,
		.nb_align = 1,
	};

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];

	memset(dev_info, 0, sizeof(struct rte_eth_dev_info));
	dev_info->rx_desc_lim = lim;
	dev_info->tx_desc_lim = lim;

	RTE_FUNC_PTR_OR_RET(*dev->dev_ops->dev_infos_get);
	(*dev->dev_ops->dev_infos_get)(dev, dev_info);
	dev_info->pci_dev = dev->pci_dev;
	dev_info->driver_name = dev->data->drv_name;
	dev_info->nb_rx_queues = dev->data->nb_rx_queues;
	dev_info->nb_tx_queues = dev->data->nb_tx_queues;
}

int
rte_eth_dev_get_supported_ptypes(uint8_t port_id, uint32_t ptype_mask,
				 uint32_t *ptypes, int num)
{
	int i, j;
	struct rte_eth_dev *dev;
	const uint32_t *all_ptypes;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_supported_ptypes_get, 0);
	all_ptypes = (*dev->dev_ops->dev_supported_ptypes_get)(dev);

	if (!all_ptypes)
		return 0;

	for (i = 0, j = 0; all_ptypes[i] != RTE_PTYPE_UNKNOWN; ++i)
		if (all_ptypes[i] & ptype_mask) {
			if (j < num)
				ptypes[j] = all_ptypes[i];
			j++;
		}

	return j;
}

void
rte_eth_macaddr_get(uint8_t port_id, struct ether_addr *mac_addr)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_RET(port_id);
	dev = &rte_eth_devices[port_id];
	ether_addr_copy(&dev->data->mac_addrs[0], mac_addr);
}


int
rte_eth_dev_get_mtu(uint8_t port_id, uint16_t *mtu)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	*mtu = dev->data->mtu;
	return 0;
}

int
rte_eth_dev_set_mtu(uint8_t port_id, uint16_t mtu)
{
	int ret;
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mtu_set, -ENOTSUP);

	ret = (*dev->dev_ops->mtu_set)(dev, mtu);
	if (!ret)
		dev->data->mtu = mtu;

	return ret;
}

int
rte_eth_dev_vlan_filter(uint8_t port_id, uint16_t vlan_id, int on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	if (!(dev->data->dev_conf.rxmode.hw_vlan_filter)) {
		RTE_PMD_DEBUG_TRACE("port %d: vlan-filtering disabled\n", port_id);
		return -ENOSYS;
	}

	if (vlan_id > 4095) {
		RTE_PMD_DEBUG_TRACE("(port_id=%d) invalid vlan_id=%u > 4095\n",
				port_id, (unsigned) vlan_id);
		return -EINVAL;
	}
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_filter_set, -ENOTSUP);

	return (*dev->dev_ops->vlan_filter_set)(dev, vlan_id, on);
}

int
rte_eth_dev_set_vlan_strip_on_queue(uint8_t port_id, uint16_t rx_queue_id, int on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid rx_queue_id=%d\n", port_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_strip_queue_set, -ENOTSUP);
	(*dev->dev_ops->vlan_strip_queue_set)(dev, rx_queue_id, on);

	return 0;
}

int
rte_eth_dev_set_vlan_ether_type(uint8_t port_id,
				enum rte_vlan_type vlan_type,
				uint16_t tpid)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_tpid_set, -ENOTSUP);

	return (*dev->dev_ops->vlan_tpid_set)(dev, vlan_type, tpid);
}

int
rte_eth_dev_set_vlan_offload(uint8_t port_id, int offload_mask)
{
	struct rte_eth_dev *dev;
	int ret = 0;
	int mask = 0;
	int cur, org = 0;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	/*check which option changed by application*/
	cur = !!(offload_mask & ETH_VLAN_STRIP_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.hw_vlan_strip);
	if (cur != org) {
		dev->data->dev_conf.rxmode.hw_vlan_strip = (uint8_t)cur;
		mask |= ETH_VLAN_STRIP_MASK;
	}

	cur = !!(offload_mask & ETH_VLAN_FILTER_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.hw_vlan_filter);
	if (cur != org) {
		dev->data->dev_conf.rxmode.hw_vlan_filter = (uint8_t)cur;
		mask |= ETH_VLAN_FILTER_MASK;
	}

	cur = !!(offload_mask & ETH_VLAN_EXTEND_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.hw_vlan_extend);
	if (cur != org) {
		dev->data->dev_conf.rxmode.hw_vlan_extend = (uint8_t)cur;
		mask |= ETH_VLAN_EXTEND_MASK;
	}

	/*no change*/
	if (mask == 0)
		return ret;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_offload_set, -ENOTSUP);
	(*dev->dev_ops->vlan_offload_set)(dev, mask);

	return ret;
}

int
rte_eth_dev_get_vlan_offload(uint8_t port_id)
{
	struct rte_eth_dev *dev;
	int ret = 0;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.rxmode.hw_vlan_strip)
		ret |= ETH_VLAN_STRIP_OFFLOAD;

	if (dev->data->dev_conf.rxmode.hw_vlan_filter)
		ret |= ETH_VLAN_FILTER_OFFLOAD;

	if (dev->data->dev_conf.rxmode.hw_vlan_extend)
		ret |= ETH_VLAN_EXTEND_OFFLOAD;

	return ret;
}

int
rte_eth_dev_set_vlan_pvid(uint8_t port_id, uint16_t pvid, int on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_pvid_set, -ENOTSUP);
	(*dev->dev_ops->vlan_pvid_set)(dev, pvid, on);

	return 0;
}

int
rte_eth_dev_flow_ctrl_get(uint8_t port_id, struct rte_eth_fc_conf *fc_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->flow_ctrl_get, -ENOTSUP);
	memset(fc_conf, 0, sizeof(*fc_conf));
	return (*dev->dev_ops->flow_ctrl_get)(dev, fc_conf);
}

int
rte_eth_dev_flow_ctrl_set(uint8_t port_id, struct rte_eth_fc_conf *fc_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if ((fc_conf->send_xon != 0) && (fc_conf->send_xon != 1)) {
		RTE_PMD_DEBUG_TRACE("Invalid send_xon, only 0/1 allowed\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->flow_ctrl_set, -ENOTSUP);
	return (*dev->dev_ops->flow_ctrl_set)(dev, fc_conf);
}

int
rte_eth_dev_priority_flow_ctrl_set(uint8_t port_id, struct rte_eth_pfc_conf *pfc_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (pfc_conf->priority > (ETH_DCB_NUM_USER_PRIORITIES - 1)) {
		RTE_PMD_DEBUG_TRACE("Invalid priority, only 0-7 allowed\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	/* High water, low water validation are device specific */
	if  (*dev->dev_ops->priority_flow_ctrl_set)
		return (*dev->dev_ops->priority_flow_ctrl_set)(dev, pfc_conf);
	return -ENOTSUP;
}

static int
rte_eth_check_reta_mask(struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	uint16_t i, num;

	if (!reta_conf)
		return -EINVAL;

	if (reta_size != RTE_ALIGN(reta_size, RTE_RETA_GROUP_SIZE)) {
		RTE_PMD_DEBUG_TRACE("Invalid reta size, should be %u aligned\n",
							RTE_RETA_GROUP_SIZE);
		return -EINVAL;
	}

	num = reta_size / RTE_RETA_GROUP_SIZE;
	for (i = 0; i < num; i++) {
		if (reta_conf[i].mask)
			return 0;
	}

	return -EINVAL;
}

static int
rte_eth_check_reta_entry(struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size,
			 uint16_t max_rxq)
{
	uint16_t i, idx, shift;

	if (!reta_conf)
		return -EINVAL;

	if (max_rxq == 0) {
		RTE_PMD_DEBUG_TRACE("No receive queue is available\n");
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if ((reta_conf[idx].mask & (1ULL << shift)) &&
			(reta_conf[idx].reta[shift] >= max_rxq)) {
			RTE_PMD_DEBUG_TRACE("reta_conf[%u]->reta[%u]: %u exceeds "
				"the maximum rxq index: %u\n", idx, shift,
				reta_conf[idx].reta[shift], max_rxq);
			return -EINVAL;
		}
	}

	return 0;
}

int
rte_eth_dev_rss_reta_update(uint8_t port_id,
			    struct rte_eth_rss_reta_entry64 *reta_conf,
			    uint16_t reta_size)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	/* Check mask bits */
	ret = rte_eth_check_reta_mask(reta_conf, reta_size);
	if (ret < 0)
		return ret;

	dev = &rte_eth_devices[port_id];

	/* Check entry value */
	ret = rte_eth_check_reta_entry(reta_conf, reta_size,
				dev->data->nb_rx_queues);
	if (ret < 0)
		return ret;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->reta_update, -ENOTSUP);
	return (*dev->dev_ops->reta_update)(dev, reta_conf, reta_size);
}

int
rte_eth_dev_rss_reta_query(uint8_t port_id,
			   struct rte_eth_rss_reta_entry64 *reta_conf,
			   uint16_t reta_size)
{
	struct rte_eth_dev *dev;
	int ret;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	/* Check mask bits */
	ret = rte_eth_check_reta_mask(reta_conf, reta_size);
	if (ret < 0)
		return ret;

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->reta_query, -ENOTSUP);
	return (*dev->dev_ops->reta_query)(dev, reta_conf, reta_size);
}

int
rte_eth_dev_rss_hash_update(uint8_t port_id, struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev *dev;
	uint16_t rss_hash_protos;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	rss_hash_protos = rss_conf->rss_hf;
	if ((rss_hash_protos != 0) &&
	    ((rss_hash_protos & ETH_RSS_PROTO_MASK) == 0)) {
		RTE_PMD_DEBUG_TRACE("Invalid rss_hash_protos=0x%x\n",
				rss_hash_protos);
		return -EINVAL;
	}
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rss_hash_update, -ENOTSUP);
	return (*dev->dev_ops->rss_hash_update)(dev, rss_conf);
}

int
rte_eth_dev_rss_hash_conf_get(uint8_t port_id,
			      struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rss_hash_conf_get, -ENOTSUP);
	return (*dev->dev_ops->rss_hash_conf_get)(dev, rss_conf);
}

int
rte_eth_dev_udp_tunnel_port_add(uint8_t port_id,
				struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (udp_tunnel == NULL) {
		RTE_PMD_DEBUG_TRACE("Invalid udp_tunnel parameter\n");
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= RTE_TUNNEL_TYPE_MAX) {
		RTE_PMD_DEBUG_TRACE("Invalid tunnel type\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->udp_tunnel_port_add, -ENOTSUP);
	return (*dev->dev_ops->udp_tunnel_port_add)(dev, udp_tunnel);
}

int
rte_eth_dev_udp_tunnel_port_delete(uint8_t port_id,
				   struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	if (udp_tunnel == NULL) {
		RTE_PMD_DEBUG_TRACE("Invalid udp_tunnel parameter\n");
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= RTE_TUNNEL_TYPE_MAX) {
		RTE_PMD_DEBUG_TRACE("Invalid tunnel type\n");
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->udp_tunnel_port_del, -ENOTSUP);
	return (*dev->dev_ops->udp_tunnel_port_del)(dev, udp_tunnel);
}

int
rte_eth_led_on(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_led_on, -ENOTSUP);
	return (*dev->dev_ops->dev_led_on)(dev);
}

int
rte_eth_led_off(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_led_off, -ENOTSUP);
	return (*dev->dev_ops->dev_led_off)(dev);
}

/*
 * Returns index into MAC address array of addr. Use 00:00:00:00:00:00 to find
 * an empty spot.
 */
static int
get_mac_addr_index(uint8_t port_id, const struct ether_addr *addr)
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

static const struct ether_addr null_mac_addr;

int
rte_eth_dev_mac_addr_add(uint8_t port_id, struct ether_addr *addr,
			uint32_t pool)
{
	struct rte_eth_dev *dev;
	int index;
	uint64_t pool_mask;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_add, -ENOTSUP);

	if (is_zero_ether_addr(addr)) {
		RTE_PMD_DEBUG_TRACE("port %d: Cannot add NULL MAC address\n",
			port_id);
		return -EINVAL;
	}
	if (pool >= ETH_64_POOLS) {
		RTE_PMD_DEBUG_TRACE("pool id must be 0-%d\n", ETH_64_POOLS - 1);
		return -EINVAL;
	}

	index = get_mac_addr_index(port_id, addr);
	if (index < 0) {
		index = get_mac_addr_index(port_id, &null_mac_addr);
		if (index < 0) {
			RTE_PMD_DEBUG_TRACE("port %d: MAC address array full\n",
				port_id);
			return -ENOSPC;
		}
	} else {
		pool_mask = dev->data->mac_pool_sel[index];

		/* Check if both MAC address and pool is already there, and do nothing */
		if (pool_mask & (1ULL << pool))
			return 0;
	}

	/* Update NIC */
	(*dev->dev_ops->mac_addr_add)(dev, addr, index, pool);

	/* Update address in NIC data structure */
	ether_addr_copy(addr, &dev->data->mac_addrs[index]);

	/* Update pool bitmap in NIC data structure */
	dev->data->mac_pool_sel[index] |= (1ULL << pool);

	return 0;
}

int
rte_eth_dev_mac_addr_remove(uint8_t port_id, struct ether_addr *addr)
{
	struct rte_eth_dev *dev;
	int index;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_remove, -ENOTSUP);

	index = get_mac_addr_index(port_id, addr);
	if (index == 0) {
		RTE_PMD_DEBUG_TRACE("port %d: Cannot remove default MAC address\n", port_id);
		return -EADDRINUSE;
	} else if (index < 0)
		return 0;  /* Do nothing if address wasn't found */

	/* Update NIC */
	(*dev->dev_ops->mac_addr_remove)(dev, index);

	/* Update address in NIC data structure */
	ether_addr_copy(&null_mac_addr, &dev->data->mac_addrs[index]);

	/* reset pool bitmap */
	dev->data->mac_pool_sel[index] = 0;

	return 0;
}

int
rte_eth_dev_default_mac_addr_set(uint8_t port_id, struct ether_addr *addr)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (!is_valid_assigned_ether_addr(addr))
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_set, -ENOTSUP);

	/* Update default address in NIC data structure */
	ether_addr_copy(addr, &dev->data->mac_addrs[0]);

	(*dev->dev_ops->mac_addr_set)(dev, addr);

	return 0;
}

int
rte_eth_dev_set_vf_rxmode(uint8_t port_id,  uint16_t vf,
				uint16_t rx_mode, uint8_t on)
{
	uint16_t num_vfs;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);

	num_vfs = dev_info.max_vfs;
	if (vf > num_vfs) {
		RTE_PMD_DEBUG_TRACE("set VF RX mode:invalid VF id %d\n", vf);
		return -EINVAL;
	}

	if (rx_mode == 0) {
		RTE_PMD_DEBUG_TRACE("set VF RX mode:mode mask ca not be zero\n");
		return -EINVAL;
	}
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_vf_rx_mode, -ENOTSUP);
	return (*dev->dev_ops->set_vf_rx_mode)(dev, vf, rx_mode, on);
}

/*
 * Returns index into MAC address array of addr. Use 00:00:00:00:00:00 to find
 * an empty spot.
 */
static int
get_hash_mac_addr_index(uint8_t port_id, const struct ether_addr *addr)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	unsigned i;

	rte_eth_dev_info_get(port_id, &dev_info);
	if (!dev->data->hash_mac_addrs)
		return -1;

	for (i = 0; i < dev_info.max_hash_mac_addrs; i++)
		if (memcmp(addr, &dev->data->hash_mac_addrs[i],
			ETHER_ADDR_LEN) == 0)
			return i;

	return -1;
}

int
rte_eth_dev_uc_hash_table_set(uint8_t port_id, struct ether_addr *addr,
				uint8_t on)
{
	int index;
	int ret;
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (is_zero_ether_addr(addr)) {
		RTE_PMD_DEBUG_TRACE("port %d: Cannot add NULL MAC address\n",
			port_id);
		return -EINVAL;
	}

	index = get_hash_mac_addr_index(port_id, addr);
	/* Check if it's already there, and do nothing */
	if ((index >= 0) && (on))
		return 0;

	if (index < 0) {
		if (!on) {
			RTE_PMD_DEBUG_TRACE("port %d: the MAC address was not "
				"set in UTA\n", port_id);
			return -EINVAL;
		}

		index = get_hash_mac_addr_index(port_id, &null_mac_addr);
		if (index < 0) {
			RTE_PMD_DEBUG_TRACE("port %d: MAC address array full\n",
					port_id);
			return -ENOSPC;
		}
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->uc_hash_table_set, -ENOTSUP);
	ret = (*dev->dev_ops->uc_hash_table_set)(dev, addr, on);
	if (ret == 0) {
		/* Update address in NIC data structure */
		if (on)
			ether_addr_copy(addr,
					&dev->data->hash_mac_addrs[index]);
		else
			ether_addr_copy(&null_mac_addr,
					&dev->data->hash_mac_addrs[index]);
	}

	return ret;
}

int
rte_eth_dev_uc_all_hash_table_set(uint8_t port_id, uint8_t on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->uc_all_hash_table_set, -ENOTSUP);
	return (*dev->dev_ops->uc_all_hash_table_set)(dev, on);
}

int
rte_eth_dev_set_vf_rx(uint8_t port_id, uint16_t vf, uint8_t on)
{
	uint16_t num_vfs;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);

	num_vfs = dev_info.max_vfs;
	if (vf > num_vfs) {
		RTE_PMD_DEBUG_TRACE("port %d: invalid vf id\n", port_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_vf_rx, -ENOTSUP);
	return (*dev->dev_ops->set_vf_rx)(dev, vf, on);
}

int
rte_eth_dev_set_vf_tx(uint8_t port_id, uint16_t vf, uint8_t on)
{
	uint16_t num_vfs;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);

	num_vfs = dev_info.max_vfs;
	if (vf > num_vfs) {
		RTE_PMD_DEBUG_TRACE("set pool tx:invalid pool id=%d\n", vf);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_vf_tx, -ENOTSUP);
	return (*dev->dev_ops->set_vf_tx)(dev, vf, on);
}

int
rte_eth_dev_set_vf_vlan_filter(uint8_t port_id, uint16_t vlan_id,
			       uint64_t vf_mask, uint8_t vlan_on)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	if (vlan_id > ETHER_MAX_VLAN_ID) {
		RTE_PMD_DEBUG_TRACE("VF VLAN filter:invalid VLAN id=%d\n",
			vlan_id);
		return -EINVAL;
	}

	if (vf_mask == 0) {
		RTE_PMD_DEBUG_TRACE("VF VLAN filter:pool_mask can not be 0\n");
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_vf_vlan_filter, -ENOTSUP);
	return (*dev->dev_ops->set_vf_vlan_filter)(dev, vlan_id,
						   vf_mask, vlan_on);
}

int rte_eth_set_queue_rate_limit(uint8_t port_id, uint16_t queue_idx,
					uint16_t tx_rate)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_link link;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);
	link = dev->data->dev_link;

	if (queue_idx > dev_info.max_tx_queues) {
		RTE_PMD_DEBUG_TRACE("set queue rate limit:port %d: "
				"invalid queue id=%d\n", port_id, queue_idx);
		return -EINVAL;
	}

	if (tx_rate > link.link_speed) {
		RTE_PMD_DEBUG_TRACE("set queue rate limit:invalid tx_rate=%d, "
				"bigger than link speed= %d\n",
			tx_rate, link.link_speed);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_queue_rate_limit, -ENOTSUP);
	return (*dev->dev_ops->set_queue_rate_limit)(dev, queue_idx, tx_rate);
}

int rte_eth_set_vf_rate_limit(uint8_t port_id, uint16_t vf, uint16_t tx_rate,
				uint64_t q_msk)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_link link;

	if (q_msk == 0)
		return 0;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);
	link = dev->data->dev_link;

	if (vf > dev_info.max_vfs) {
		RTE_PMD_DEBUG_TRACE("set VF rate limit:port %d: "
				"invalid vf id=%d\n", port_id, vf);
		return -EINVAL;
	}

	if (tx_rate > link.link_speed) {
		RTE_PMD_DEBUG_TRACE("set VF rate limit:invalid tx_rate=%d, "
				"bigger than link speed= %d\n",
				tx_rate, link.link_speed);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_vf_rate_limit, -ENOTSUP);
	return (*dev->dev_ops->set_vf_rate_limit)(dev, vf, tx_rate, q_msk);
}

int
rte_eth_mirror_rule_set(uint8_t port_id,
			struct rte_eth_mirror_conf *mirror_conf,
			uint8_t rule_id, uint8_t on)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (mirror_conf->rule_type == 0) {
		RTE_PMD_DEBUG_TRACE("mirror rule type can not be 0.\n");
		return -EINVAL;
	}

	if (mirror_conf->dst_pool >= ETH_64_POOLS) {
		RTE_PMD_DEBUG_TRACE("Invalid dst pool, pool id must be 0-%d\n",
				ETH_64_POOLS - 1);
		return -EINVAL;
	}

	if ((mirror_conf->rule_type & (ETH_MIRROR_VIRTUAL_POOL_UP |
	     ETH_MIRROR_VIRTUAL_POOL_DOWN)) &&
	    (mirror_conf->pool_mask == 0)) {
		RTE_PMD_DEBUG_TRACE("Invalid mirror pool, pool mask can not be 0.\n");
		return -EINVAL;
	}

	if ((mirror_conf->rule_type & ETH_MIRROR_VLAN) &&
	    mirror_conf->vlan.vlan_mask == 0) {
		RTE_PMD_DEBUG_TRACE("Invalid vlan mask, vlan mask can not be 0.\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mirror_rule_set, -ENOTSUP);

	return (*dev->dev_ops->mirror_rule_set)(dev, mirror_conf, rule_id, on);
}

int
rte_eth_mirror_rule_reset(uint8_t port_id, uint8_t rule_id)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mirror_rule_reset, -ENOTSUP);

	return (*dev->dev_ops->mirror_rule_reset)(dev, rule_id);
}

int
rte_eth_dev_callback_register(uint8_t port_id,
			enum rte_eth_event_type event,
			rte_eth_dev_cb_fn cb_fn, void *cb_arg)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_callback *user_cb;

	if (!cb_fn)
		return -EINVAL;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	rte_spinlock_lock(&rte_eth_dev_cb_lock);

	TAILQ_FOREACH(user_cb, &(dev->link_intr_cbs), next) {
		if (user_cb->cb_fn == cb_fn &&
			user_cb->cb_arg == cb_arg &&
			user_cb->event == event) {
			break;
		}
	}

	/* create a new callback. */
	if (user_cb == NULL)
		user_cb = rte_zmalloc("INTR_USER_CALLBACK",
					sizeof(struct rte_eth_dev_callback), 0);
	if (user_cb != NULL) {
		user_cb->cb_fn = cb_fn;
		user_cb->cb_arg = cb_arg;
		user_cb->event = event;
		TAILQ_INSERT_TAIL(&(dev->link_intr_cbs), user_cb, next);
	}

	rte_spinlock_unlock(&rte_eth_dev_cb_lock);
	return (user_cb == NULL) ? -ENOMEM : 0;
}

int
rte_eth_dev_callback_unregister(uint8_t port_id,
			enum rte_eth_event_type event,
			rte_eth_dev_cb_fn cb_fn, void *cb_arg)
{
	int ret;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_callback *cb, *next;

	if (!cb_fn)
		return -EINVAL;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);

	dev = &rte_eth_devices[port_id];
	rte_spinlock_lock(&rte_eth_dev_cb_lock);

	ret = 0;
	for (cb = TAILQ_FIRST(&dev->link_intr_cbs); cb != NULL; cb = next) {

		next = TAILQ_NEXT(cb, next);

		if (cb->cb_fn != cb_fn || cb->event != event ||
				(cb->cb_arg != (void *)-1 &&
				cb->cb_arg != cb_arg))
			continue;

		/*
		 * if this callback is not executing right now,
		 * then remove it.
		 */
		if (cb->active == 0) {
			TAILQ_REMOVE(&(dev->link_intr_cbs), cb, next);
			rte_free(cb);
		} else {
			ret = -EAGAIN;
		}
	}

	rte_spinlock_unlock(&rte_eth_dev_cb_lock);
	return ret;
}

void
_rte_eth_dev_callback_process(struct rte_eth_dev *dev,
	enum rte_eth_event_type event)
{
	struct rte_eth_dev_callback *cb_lst;
	struct rte_eth_dev_callback dev_cb;

	rte_spinlock_lock(&rte_eth_dev_cb_lock);
	TAILQ_FOREACH(cb_lst, &(dev->link_intr_cbs), next) {
		if (cb_lst->cb_fn == NULL || cb_lst->event != event)
			continue;
		dev_cb = *cb_lst;
		cb_lst->active = 1;
		rte_spinlock_unlock(&rte_eth_dev_cb_lock);
		dev_cb.cb_fn(dev->data->port_id, dev_cb.event,
						dev_cb.cb_arg);
		rte_spinlock_lock(&rte_eth_dev_cb_lock);
		cb_lst->active = 0;
	}
	rte_spinlock_unlock(&rte_eth_dev_cb_lock);
}

int
rte_eth_dev_rx_intr_ctl(uint8_t port_id, int epfd, int op, void *data)
{
	uint32_t vec;
	struct rte_eth_dev *dev;
	struct rte_intr_handle *intr_handle;
	uint16_t qid;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	intr_handle = &dev->pci_dev->intr_handle;
	if (!intr_handle->intr_vec) {
		RTE_PMD_DEBUG_TRACE("RX Intr vector unset\n");
		return -EPERM;
	}

	for (qid = 0; qid < dev->data->nb_rx_queues; qid++) {
		vec = intr_handle->intr_vec[qid];
		rc = rte_intr_rx_ctl(intr_handle, epfd, op, vec, data);
		if (rc && rc != -EEXIST) {
			RTE_PMD_DEBUG_TRACE("p %u q %u rx ctl error"
					" op %d epfd %d vec %u\n",
					port_id, qid, op, epfd, vec);
		}
	}

	return 0;
}

const struct rte_memzone *
rte_eth_dma_zone_reserve(const struct rte_eth_dev *dev, const char *ring_name,
			 uint16_t queue_id, size_t size, unsigned align,
			 int socket_id)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(z_name, sizeof(z_name), "%s_%s_%d_%d",
		 dev->driver->pci_drv.name, ring_name,
		 dev->data->port_id, queue_id);

	mz = rte_memzone_lookup(z_name);
	if (mz)
		return mz;

	if (rte_xen_dom0_supported())
		return rte_memzone_reserve_bounded(z_name, size, socket_id,
						   0, align, RTE_PGSIZE_2M);
	else
		return rte_memzone_reserve_aligned(z_name, size, socket_id,
						   0, align);
}

int
rte_eth_dev_rx_intr_ctl_q(uint8_t port_id, uint16_t queue_id,
			  int epfd, int op, void *data)
{
	uint32_t vec;
	struct rte_eth_dev *dev;
	struct rte_intr_handle *intr_handle;
	int rc;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid RX queue_id=%u\n", queue_id);
		return -EINVAL;
	}

	intr_handle = &dev->pci_dev->intr_handle;
	if (!intr_handle->intr_vec) {
		RTE_PMD_DEBUG_TRACE("RX Intr vector unset\n");
		return -EPERM;
	}

	vec = intr_handle->intr_vec[queue_id];
	rc = rte_intr_rx_ctl(intr_handle, epfd, op, vec, data);
	if (rc && rc != -EEXIST) {
		RTE_PMD_DEBUG_TRACE("p %u q %u rx ctl error"
				" op %d epfd %d vec %u\n",
				port_id, queue_id, op, epfd, vec);
		return rc;
	}

	return 0;
}

int
rte_eth_dev_rx_intr_enable(uint8_t port_id,
			   uint16_t queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_intr_enable, -ENOTSUP);
	return (*dev->dev_ops->rx_queue_intr_enable)(dev, queue_id);
}

int
rte_eth_dev_rx_intr_disable(uint8_t port_id,
			    uint16_t queue_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_intr_disable, -ENOTSUP);
	return (*dev->dev_ops->rx_queue_intr_disable)(dev, queue_id);
}

#ifdef RTE_NIC_BYPASS
int rte_eth_dev_bypass_init(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_init, -ENOTSUP);
	(*dev->dev_ops->bypass_init)(dev);
	return 0;
}

int
rte_eth_dev_bypass_state_show(uint8_t port_id, uint32_t *state)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_state_show, -ENOTSUP);
	(*dev->dev_ops->bypass_state_show)(dev, state);
	return 0;
}

int
rte_eth_dev_bypass_state_set(uint8_t port_id, uint32_t *new_state)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_state_set, -ENOTSUP);
	(*dev->dev_ops->bypass_state_set)(dev, new_state);
	return 0;
}

int
rte_eth_dev_bypass_event_show(uint8_t port_id, uint32_t event, uint32_t *state)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_state_show, -ENOTSUP);
	(*dev->dev_ops->bypass_event_show)(dev, event, state);
	return 0;
}

int
rte_eth_dev_bypass_event_store(uint8_t port_id, uint32_t event, uint32_t state)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_event_set, -ENOTSUP);
	(*dev->dev_ops->bypass_event_set)(dev, event, state);
	return 0;
}

int
rte_eth_dev_wd_timeout_store(uint8_t port_id, uint32_t timeout)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_wd_timeout_set, -ENOTSUP);
	(*dev->dev_ops->bypass_wd_timeout_set)(dev, timeout);
	return 0;
}

int
rte_eth_dev_bypass_ver_show(uint8_t port_id, uint32_t *ver)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_ver_show, -ENOTSUP);
	(*dev->dev_ops->bypass_ver_show)(dev, ver);
	return 0;
}

int
rte_eth_dev_bypass_wd_timeout_show(uint8_t port_id, uint32_t *wd_timeout)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_wd_timeout_show, -ENOTSUP);
	(*dev->dev_ops->bypass_wd_timeout_show)(dev, wd_timeout);
	return 0;
}

int
rte_eth_dev_bypass_wd_reset(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_wd_reset, -ENOTSUP);
	(*dev->dev_ops->bypass_wd_reset)(dev);
	return 0;
}
#endif

int
rte_eth_dev_filter_supported(uint8_t port_id, enum rte_filter_type filter_type)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->filter_ctrl, -ENOTSUP);
	return (*dev->dev_ops->filter_ctrl)(dev, filter_type,
				RTE_ETH_FILTER_NOP, NULL);
}

int
rte_eth_dev_filter_ctrl(uint8_t port_id, enum rte_filter_type filter_type,
		       enum rte_filter_op filter_op, void *arg)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->filter_ctrl, -ENOTSUP);
	return (*dev->dev_ops->filter_ctrl)(dev, filter_type, filter_op, arg);
}

void *
rte_eth_add_rx_callback(uint8_t port_id, uint16_t queue_id,
		rte_rx_callback_fn fn, void *user_param)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	rte_errno = ENOTSUP;
	return NULL;
#endif
	/* check input parameters */
	if (!rte_eth_dev_is_valid_port(port_id) || fn == NULL ||
		    queue_id >= rte_eth_devices[port_id].data->nb_rx_queues) {
		rte_errno = EINVAL;
		return NULL;
	}
	struct rte_eth_rxtx_callback *cb = rte_zmalloc(NULL, sizeof(*cb), 0);

	if (cb == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	cb->fn.rx = fn;
	cb->param = user_param;

	rte_spinlock_lock(&rte_eth_rx_cb_lock);
	/* Add the callbacks in fifo order. */
	struct rte_eth_rxtx_callback *tail =
		rte_eth_devices[port_id].post_rx_burst_cbs[queue_id];

	if (!tail) {
		rte_eth_devices[port_id].post_rx_burst_cbs[queue_id] = cb;

	} else {
		while (tail->next)
			tail = tail->next;
		tail->next = cb;
	}
	rte_spinlock_unlock(&rte_eth_rx_cb_lock);

	return cb;
}

void *
rte_eth_add_first_rx_callback(uint8_t port_id, uint16_t queue_id,
		rte_rx_callback_fn fn, void *user_param)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	rte_errno = ENOTSUP;
	return NULL;
#endif
	/* check input parameters */
	if (!rte_eth_dev_is_valid_port(port_id) || fn == NULL ||
		queue_id >= rte_eth_devices[port_id].data->nb_rx_queues) {
		rte_errno = EINVAL;
		return NULL;
	}

	struct rte_eth_rxtx_callback *cb = rte_zmalloc(NULL, sizeof(*cb), 0);

	if (cb == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	cb->fn.rx = fn;
	cb->param = user_param;

	rte_spinlock_lock(&rte_eth_rx_cb_lock);
	/* Add the callbacks at fisrt position*/
	cb->next = rte_eth_devices[port_id].post_rx_burst_cbs[queue_id];
	rte_smp_wmb();
	rte_eth_devices[port_id].post_rx_burst_cbs[queue_id] = cb;
	rte_spinlock_unlock(&rte_eth_rx_cb_lock);

	return cb;
}

void *
rte_eth_add_tx_callback(uint8_t port_id, uint16_t queue_id,
		rte_tx_callback_fn fn, void *user_param)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	rte_errno = ENOTSUP;
	return NULL;
#endif
	/* check input parameters */
	if (!rte_eth_dev_is_valid_port(port_id) || fn == NULL ||
		    queue_id >= rte_eth_devices[port_id].data->nb_tx_queues) {
		rte_errno = EINVAL;
		return NULL;
	}

	struct rte_eth_rxtx_callback *cb = rte_zmalloc(NULL, sizeof(*cb), 0);

	if (cb == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	cb->fn.tx = fn;
	cb->param = user_param;

	rte_spinlock_lock(&rte_eth_tx_cb_lock);
	/* Add the callbacks in fifo order. */
	struct rte_eth_rxtx_callback *tail =
		rte_eth_devices[port_id].pre_tx_burst_cbs[queue_id];

	if (!tail) {
		rte_eth_devices[port_id].pre_tx_burst_cbs[queue_id] = cb;

	} else {
		while (tail->next)
			tail = tail->next;
		tail->next = cb;
	}
	rte_spinlock_unlock(&rte_eth_tx_cb_lock);

	return cb;
}

int
rte_eth_remove_rx_callback(uint8_t port_id, uint16_t queue_id,
		struct rte_eth_rxtx_callback *user_cb)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	return -ENOTSUP;
#endif
	/* Check input parameters. */
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);
	if (user_cb == NULL ||
			queue_id >= rte_eth_devices[port_id].data->nb_rx_queues)
		return -EINVAL;

	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_eth_rxtx_callback *cb;
	struct rte_eth_rxtx_callback **prev_cb;
	int ret = -EINVAL;

	rte_spinlock_lock(&rte_eth_rx_cb_lock);
	prev_cb = &dev->post_rx_burst_cbs[queue_id];
	for (; *prev_cb != NULL; prev_cb = &cb->next) {
		cb = *prev_cb;
		if (cb == user_cb) {
			/* Remove the user cb from the callback list. */
			*prev_cb = cb->next;
			ret = 0;
			break;
		}
	}
	rte_spinlock_unlock(&rte_eth_rx_cb_lock);

	return ret;
}

int
rte_eth_remove_tx_callback(uint8_t port_id, uint16_t queue_id,
		struct rte_eth_rxtx_callback *user_cb)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	return -ENOTSUP;
#endif
	/* Check input parameters. */
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -EINVAL);
	if (user_cb == NULL ||
			queue_id >= rte_eth_devices[port_id].data->nb_tx_queues)
		return -EINVAL;

	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	int ret = -EINVAL;
	struct rte_eth_rxtx_callback *cb;
	struct rte_eth_rxtx_callback **prev_cb;

	rte_spinlock_lock(&rte_eth_tx_cb_lock);
	prev_cb = &dev->pre_tx_burst_cbs[queue_id];
	for (; *prev_cb != NULL; prev_cb = &cb->next) {
		cb = *prev_cb;
		if (cb == user_cb) {
			/* Remove the user cb from the callback list. */
			*prev_cb = cb->next;
			ret = 0;
			break;
		}
	}
	rte_spinlock_unlock(&rte_eth_tx_cb_lock);

	return ret;
}

int
rte_eth_rx_queue_info_get(uint8_t port_id, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (qinfo == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	if (queue_id >= dev->data->nb_rx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rxq_info_get, -ENOTSUP);

	memset(qinfo, 0, sizeof(*qinfo));
	dev->dev_ops->rxq_info_get(dev, queue_id, qinfo);
	return 0;
}

int
rte_eth_tx_queue_info_get(uint8_t port_id, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (qinfo == NULL)
		return -EINVAL;

	dev = &rte_eth_devices[port_id];
	if (queue_id >= dev->data->nb_tx_queues) {
		RTE_PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", queue_id);
		return -EINVAL;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->txq_info_get, -ENOTSUP);

	memset(qinfo, 0, sizeof(*qinfo));
	dev->dev_ops->txq_info_get(dev, queue_id, qinfo);
	return 0;
}

int
rte_eth_dev_set_mc_addr_list(uint8_t port_id,
			     struct ether_addr *mc_addr_set,
			     uint32_t nb_mc_addr)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_mc_addr_list, -ENOTSUP);
	return dev->dev_ops->set_mc_addr_list(dev, mc_addr_set, nb_mc_addr);
}

int
rte_eth_timesync_enable(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_enable, -ENOTSUP);
	return (*dev->dev_ops->timesync_enable)(dev);
}

int
rte_eth_timesync_disable(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_disable, -ENOTSUP);
	return (*dev->dev_ops->timesync_disable)(dev);
}

int
rte_eth_timesync_read_rx_timestamp(uint8_t port_id, struct timespec *timestamp,
				   uint32_t flags)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_read_rx_timestamp, -ENOTSUP);
	return (*dev->dev_ops->timesync_read_rx_timestamp)(dev, timestamp, flags);
}

int
rte_eth_timesync_read_tx_timestamp(uint8_t port_id, struct timespec *timestamp)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_read_tx_timestamp, -ENOTSUP);
	return (*dev->dev_ops->timesync_read_tx_timestamp)(dev, timestamp);
}

int
rte_eth_timesync_adjust_time(uint8_t port_id, int64_t delta)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_adjust_time, -ENOTSUP);
	return (*dev->dev_ops->timesync_adjust_time)(dev, delta);
}

int
rte_eth_timesync_read_time(uint8_t port_id, struct timespec *timestamp)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_read_time, -ENOTSUP);
	return (*dev->dev_ops->timesync_read_time)(dev, timestamp);
}

int
rte_eth_timesync_write_time(uint8_t port_id, const struct timespec *timestamp)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	dev = &rte_eth_devices[port_id];

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->timesync_write_time, -ENOTSUP);
	return (*dev->dev_ops->timesync_write_time)(dev, timestamp);
}

int
rte_eth_dev_get_reg_info(uint8_t port_id, struct rte_dev_reg_info *info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_reg, -ENOTSUP);
	return (*dev->dev_ops->get_reg)(dev, info);
}

int
rte_eth_dev_get_eeprom_length(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_eeprom_length, -ENOTSUP);
	return (*dev->dev_ops->get_eeprom_length)(dev);
}

int
rte_eth_dev_get_eeprom(uint8_t port_id, struct rte_dev_eeprom_info *info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_eeprom, -ENOTSUP);
	return (*dev->dev_ops->get_eeprom)(dev, info);
}

int
rte_eth_dev_set_eeprom(uint8_t port_id, struct rte_dev_eeprom_info *info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_eeprom, -ENOTSUP);
	return (*dev->dev_ops->set_eeprom)(dev, info);
}

int
rte_eth_dev_get_dcb_info(uint8_t port_id,
			     struct rte_eth_dcb_info *dcb_info)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	memset(dcb_info, 0, sizeof(struct rte_eth_dcb_info));

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->get_dcb_info, -ENOTSUP);
	return (*dev->dev_ops->get_dcb_info)(dev, dcb_info);
}

void
rte_eth_copy_pci_info(struct rte_eth_dev *eth_dev, struct rte_pci_device *pci_dev)
{
	if ((eth_dev == NULL) || (pci_dev == NULL)) {
		RTE_PMD_DEBUG_TRACE("NULL pointer eth_dev=%p pci_dev=%p\n",
				eth_dev, pci_dev);
		return;
	}

	eth_dev->data->dev_flags = 0;
	if (pci_dev->driver->drv_flags & RTE_PCI_DRV_INTR_LSC)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_LSC;
	if (pci_dev->driver->drv_flags & RTE_PCI_DRV_DETACHABLE)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_DETACHABLE;

	eth_dev->data->kdrv = pci_dev->kdrv;
	eth_dev->data->numa_node = pci_dev->numa_node;
	eth_dev->data->drv_name = pci_dev->driver->name;
}

int
rte_eth_dev_l2_tunnel_eth_type_conf(uint8_t port_id,
				    struct rte_eth_l2_tunnel_conf *l2_tunnel)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);
	if (l2_tunnel == NULL) {
		RTE_PMD_DEBUG_TRACE("Invalid l2_tunnel parameter\n");
		return -EINVAL;
	}

	if (l2_tunnel->l2_tunnel_type >= RTE_TUNNEL_TYPE_MAX) {
		RTE_PMD_DEBUG_TRACE("Invalid tunnel type\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->l2_tunnel_eth_type_conf,
				-ENOTSUP);
	return (*dev->dev_ops->l2_tunnel_eth_type_conf)(dev, l2_tunnel);
}

int
rte_eth_dev_l2_tunnel_offload_set(uint8_t port_id,
				  struct rte_eth_l2_tunnel_conf *l2_tunnel,
				  uint32_t mask,
				  uint8_t en)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	if (l2_tunnel == NULL) {
		RTE_PMD_DEBUG_TRACE("Invalid l2_tunnel parameter\n");
		return -EINVAL;
	}

	if (l2_tunnel->l2_tunnel_type >= RTE_TUNNEL_TYPE_MAX) {
		RTE_PMD_DEBUG_TRACE("Invalid tunnel type.\n");
		return -EINVAL;
	}

	if (mask == 0) {
		RTE_PMD_DEBUG_TRACE("Mask should have a value.\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->l2_tunnel_offload_set,
				-ENOTSUP);
	return (*dev->dev_ops->l2_tunnel_offload_set)(dev, l2_tunnel, mask, en);
}
