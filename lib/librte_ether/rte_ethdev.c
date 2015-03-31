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

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
#define PMD_DEBUG_TRACE(fmt, args...) do {                        \
		RTE_LOG(ERR, PMD, "%s: " fmt, __func__, ## args); \
	} while (0)
#else
#define PMD_DEBUG_TRACE(fmt, args...)
#endif

/* Macros for checking for restricting functions to primary instance only */
#define PROC_PRIMARY_OR_ERR_RET(retval) do { \
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) { \
		PMD_DEBUG_TRACE("Cannot run in secondary processes\n"); \
		return (retval); \
	} \
} while(0)
#define PROC_PRIMARY_OR_RET() do { \
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) { \
		PMD_DEBUG_TRACE("Cannot run in secondary processes\n"); \
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

/* store statistics names and its offset in stats structure  */
struct rte_eth_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned offset;
};

static struct rte_eth_xstats_name_off rte_stats_strings[] = {
	 {"rx_packets", offsetof(struct rte_eth_stats, ipackets)},
	 {"tx_packets", offsetof(struct rte_eth_stats, opackets)},
	 {"rx_bytes", offsetof(struct rte_eth_stats, ibytes)},
	 {"tx_bytes", offsetof(struct rte_eth_stats, obytes)},
	 {"tx_errors", offsetof(struct rte_eth_stats, oerrors)},
	 {"rx_missed_errors", offsetof(struct rte_eth_stats, imissed)},
	 {"rx_crc_errors", offsetof(struct rte_eth_stats, ibadcrc)},
	 {"rx_bad_length_errors", offsetof(struct rte_eth_stats, ibadlen)},
	 {"rx_errors", offsetof(struct rte_eth_stats, ierrors)},
	 {"alloc_rx_buff_failed", offsetof(struct rte_eth_stats, rx_nombuf)},
	 {"fdir_match", offsetof(struct rte_eth_stats, fdirmatch)},
	 {"fdir_miss", offsetof(struct rte_eth_stats, fdirmiss)},
	 {"tx_flow_control_xon", offsetof(struct rte_eth_stats, tx_pause_xon)},
	 {"rx_flow_control_xon", offsetof(struct rte_eth_stats, rx_pause_xon)},
	 {"tx_flow_control_xoff", offsetof(struct rte_eth_stats, tx_pause_xoff)},
	 {"rx_flow_control_xoff", offsetof(struct rte_eth_stats, rx_pause_xoff)},
};
#define RTE_NB_STATS (sizeof(rte_stats_strings) / sizeof(rte_stats_strings[0]))

static struct rte_eth_xstats_name_off rte_rxq_stats_strings[] = {
	{"rx_packets", offsetof(struct rte_eth_stats, q_ipackets)},
	{"rx_bytes", offsetof(struct rte_eth_stats, q_ibytes)},
};
#define RTE_NB_RXQ_STATS (sizeof(rte_rxq_stats_strings) /	\
		sizeof(rte_rxq_stats_strings[0]))

static struct rte_eth_xstats_name_off rte_txq_stats_strings[] = {
	{"tx_packets", offsetof(struct rte_eth_stats, q_opackets)},
	{"tx_bytes", offsetof(struct rte_eth_stats, q_obytes)},
	{"tx_errors", offsetof(struct rte_eth_stats, q_errors)},
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
		PMD_DEBUG_TRACE("Reached maximum number of Ethernet ports\n");
		return NULL;
	}

	if (rte_eth_dev_data == NULL)
		rte_eth_dev_data_alloc();

	if (rte_eth_dev_allocated(name) != NULL) {
		PMD_DEBUG_TRACE("Ethernet Device with name %s already allocated!\n", name);
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

static inline int
rte_eth_dev_create_unique_device_name(char *name, size_t size,
		struct rte_pci_device *pci_dev)
{
	int ret;

	if ((name == NULL) || (pci_dev == NULL))
		return -EINVAL;

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

	eth_dev->attached = 0;
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

	if (rte_eal_process_type() == RTE_PROC_PRIMARY){
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
		return (0);

	PMD_DEBUG_TRACE("driver %s: eth_dev_init(vendor_id=0x%u device_id=0x%x)"
			" failed\n", pci_drv->name,
			(unsigned) pci_dev->id.vendor_id,
			(unsigned) pci_dev->id.device_id);
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(eth_dev->data->dev_private);
	eth_dev->attached = DEV_DETACHED;
	nb_ports--;
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

static int
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
	if (!rte_eth_dev_is_valid_port(port_id))
		return -1;
	return rte_eth_devices[port_id].pci_dev->numa_node;
}

uint8_t
rte_eth_dev_count(void)
{
	return (nb_ports);
}

/* So far, DPDK hotplug function only supports linux */
#ifdef RTE_LIBRTE_EAL_HOTPLUG

static enum rte_eth_dev_type
rte_eth_dev_get_device_type(uint8_t port_id)
{
	if (!rte_eth_dev_is_valid_port(port_id))
		return RTE_ETH_DEV_UNKNOWN;
	return rte_eth_devices[port_id].dev_type;
}

static int
rte_eth_dev_save(struct rte_eth_dev *devs, size_t size)
{
	if ((devs == NULL) ||
	    (size != sizeof(struct rte_eth_dev) * RTE_MAX_ETHPORTS))
		return -EINVAL;

	/* save current rte_eth_devices */
	memcpy(devs, rte_eth_devices, size);
	return 0;
}

static int
rte_eth_dev_get_changed_port(struct rte_eth_dev *devs, uint8_t *port_id)
{
	if ((devs == NULL) || (port_id == NULL))
		return -EINVAL;

	/* check which port was attached or detached */
	for (*port_id = 0; *port_id < RTE_MAX_ETHPORTS; (*port_id)++, devs++) {
		if (rte_eth_devices[*port_id].attached ^ devs->attached)
			return 0;
	}
	return -ENODEV;
}

static int
rte_eth_dev_get_addr_by_port(uint8_t port_id, struct rte_pci_addr *addr)
{
	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	if (addr == NULL) {
		PMD_DEBUG_TRACE("Null pointer is specified\n");
		return -EINVAL;
	}

	*addr = rte_eth_devices[port_id].pci_dev->addr;
	return 0;
}

static int
rte_eth_dev_get_name_by_port(uint8_t port_id, char *name)
{
	char *tmp;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	if (name == NULL) {
		PMD_DEBUG_TRACE("Null pointer is specified\n");
		return -EINVAL;
	}

	/* shouldn't check 'rte_eth_devices[i].data',
	 * because it might be overwritten by VDEV PMD */
	tmp = rte_eth_dev_data[port_id].name;
	strcpy(name, tmp);
	return 0;
}

static int
rte_eth_dev_is_detachable(uint8_t port_id)
{
	uint32_t drv_flags;

	if (port_id >= RTE_MAX_ETHPORTS) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	if (rte_eth_devices[port_id].dev_type == RTE_ETH_DEV_PCI) {
		switch (rte_eth_devices[port_id].pci_dev->kdrv) {
		case RTE_KDRV_IGB_UIO:
		case RTE_KDRV_UIO_GENERIC:
			break;
		case RTE_KDRV_VFIO:
		default:
			return -ENOTSUP;
		}
	}

	drv_flags = rte_eth_devices[port_id].driver->pci_drv.drv_flags;
	return !(drv_flags & RTE_PCI_DRV_DETACHABLE);
}

/* attach the new physical device, then store port_id of the device */
static int
rte_eth_dev_attach_pdev(struct rte_pci_addr *addr, uint8_t *port_id)
{
	uint8_t new_port_id;
	struct rte_eth_dev devs[RTE_MAX_ETHPORTS];

	if ((addr == NULL) || (port_id == NULL))
		goto err;

	/* save current port status */
	if (rte_eth_dev_save(devs, sizeof(devs)))
		goto err;
	/* re-construct pci_device_list */
	if (rte_eal_pci_scan())
		goto err;
	/* invoke probe func of the driver can handle the new device.
	 * TODO:
	 * rte_eal_pci_probe_one() should return port_id.
	 * And rte_eth_dev_save() and rte_eth_dev_get_changed_port()
	 * should be removed. */
	if (rte_eal_pci_probe_one(addr))
		goto err;
	/* get port_id enabled by above procedures */
	if (rte_eth_dev_get_changed_port(devs, &new_port_id))
		goto err;

	*port_id = new_port_id;
	return 0;
err:
	RTE_LOG(ERR, EAL, "Driver, cannot attach the device\n");
	return -1;
}

/* detach the new physical device, then store pci_addr of the device */
static int
rte_eth_dev_detach_pdev(uint8_t port_id, struct rte_pci_addr *addr)
{
	struct rte_pci_addr freed_addr;
	struct rte_pci_addr vp;

	if (addr == NULL)
		goto err;

	/* check whether the driver supports detach feature, or not */
	if (rte_eth_dev_is_detachable(port_id))
		goto err;

	/* get pci address by port id */
	if (rte_eth_dev_get_addr_by_port(port_id, &freed_addr))
		goto err;

	/* Zerod pci addr means the port comes from virtual device */
	vp.domain = vp.bus = vp.devid = vp.function = 0;
	if (rte_eal_compare_pci_addr(&vp, &freed_addr) == 0)
		goto err;

	/* invoke close func of the driver,
	 * also remove the device from pci_device_list */
	if (rte_eal_pci_close_one(&freed_addr))
		goto err;

	*addr = freed_addr;
	return 0;
err:
	RTE_LOG(ERR, EAL, "Driver, cannot detach the device\n");
	return -1;
}

/* attach the new virtual device, then store port_id of the device */
static int
rte_eth_dev_attach_vdev(const char *vdevargs, uint8_t *port_id)
{
	char *name = NULL, *args = NULL;
	uint8_t new_port_id;
	struct rte_eth_dev devs[RTE_MAX_ETHPORTS];
	int ret = -1;

	if ((vdevargs == NULL) || (port_id == NULL))
		goto end;

	/* parse vdevargs, then retrieve device name and args */
	if (rte_eal_parse_devargs_str(vdevargs, &name, &args))
		goto end;

	/* save current port status */
	if (rte_eth_dev_save(devs, sizeof(devs)))
		goto end;
	/* walk around dev_driver_list to find the driver of the device,
	 * then invoke probe function o the driver.
	 * TODO:
	 * rte_eal_vdev_init() should return port_id,
	 * And rte_eth_dev_save() and rte_eth_dev_get_changed_port()
	 * should be removed. */
	if (rte_eal_vdev_init(name, args))
		goto end;
	/* get port_id enabled by above procedures */
	if (rte_eth_dev_get_changed_port(devs, &new_port_id))
		goto end;
	ret = 0;
	*port_id = new_port_id;
end:
	if (name)
		free(name);
	if (args)
		free(args);

	if (ret < 0)
		RTE_LOG(ERR, EAL, "Driver, cannot attach the device\n");
	return ret;
}

/* detach the new virtual device, then store the name of the device */
static int
rte_eth_dev_detach_vdev(uint8_t port_id, char *vdevname)
{
	char name[RTE_ETH_NAME_MAX_LEN];

	if (vdevname == NULL)
		goto err;

	/* check whether the driver supports detach feature, or not */
	if (rte_eth_dev_is_detachable(port_id))
		goto err;

	/* get device name by port id */
	if (rte_eth_dev_get_name_by_port(port_id, name))
		goto err;
	/* walk around dev_driver_list to find the driver of the device,
	 * then invoke close function o the driver */
	if (rte_eal_vdev_uninit(name))
		goto err;

	strncpy(vdevname, name, sizeof(name));
	return 0;
err:
	RTE_LOG(ERR, EAL, "Driver, cannot detach the device\n");
	return -1;
}

/* attach the new device, then store port_id of the device */
int
rte_eth_dev_attach(const char *devargs, uint8_t *port_id)
{
	struct rte_pci_addr addr;

	if ((devargs == NULL) || (port_id == NULL))
		return -EINVAL;

	if (eal_parse_pci_DomBDF(devargs, &addr) == 0)
		return rte_eth_dev_attach_pdev(&addr, port_id);
	else
		return rte_eth_dev_attach_vdev(devargs, port_id);
}

/* detach the device, then store the name of the device */
int
rte_eth_dev_detach(uint8_t port_id, char *name)
{
	struct rte_pci_addr addr;
	int ret;

	if (name == NULL)
		return -EINVAL;

	if (rte_eth_dev_get_device_type(port_id) == RTE_ETH_DEV_PCI) {
		ret = rte_eth_dev_get_addr_by_port(port_id, &addr);
		if (ret < 0)
			return ret;

		ret = rte_eth_dev_detach_pdev(port_id, &addr);
		if (ret == 0)
			snprintf(name, RTE_ETH_NAME_MAX_LEN,
				"%04x:%02x:%02x.%d",
				addr.domain, addr.bus,
				addr.devid, addr.function);

		return ret;
	} else
		return rte_eth_dev_detach_vdev(port_id, name);
}
#else /* RTE_LIBRTE_EAL_HOTPLUG */
int
rte_eth_dev_attach(const char *devargs __rte_unused,
			uint8_t *port_id __rte_unused)
{
	RTE_LOG(ERR, EAL, "Hotplug support isn't enabled\n");
	return -1;
}

/* detach the device, then store the name of the device */
int
rte_eth_dev_detach(uint8_t port_id __rte_unused,
			char *name __rte_unused)
{
	RTE_LOG(ERR, EAL, "Hotplug support isn't enabled\n");
	return -1;
}
#endif /* RTE_LIBRTE_EAL_HOTPLUG */

static int
rte_eth_dev_rx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	uint16_t old_nb_queues = dev->data->nb_rx_queues;
	void **rxq;
	unsigned i;

	if (dev->data->rx_queues == NULL) { /* first time configuration */
		dev->data->rx_queues = rte_zmalloc("ethdev->rx_queues",
				sizeof(dev->data->rx_queues[0]) * nb_queues,
				RTE_CACHE_LINE_SIZE);
		if (dev->data->rx_queues == NULL) {
			dev->data->nb_rx_queues = 0;
			return -(ENOMEM);
		}
	} else { /* re-configure */
		FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_release, -ENOTSUP);

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

	}
	dev->data->nb_rx_queues = nb_queues;
	return (0);
}

int
rte_eth_dev_rx_queue_start(uint8_t port_id, uint16_t rx_queue_id)
{
	struct rte_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR_RET(-E_RTE_SECONDARY);

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", rx_queue_id);
		return -EINVAL;
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_start, -ENOTSUP);

	return dev->dev_ops->rx_queue_start(dev, rx_queue_id);

}

int
rte_eth_dev_rx_queue_stop(uint8_t port_id, uint16_t rx_queue_id)
{
	struct rte_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR_RET(-E_RTE_SECONDARY);

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		PMD_DEBUG_TRACE("Invalid RX queue_id=%d\n", rx_queue_id);
		return -EINVAL;
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_stop, -ENOTSUP);

	return dev->dev_ops->rx_queue_stop(dev, rx_queue_id);

}

int
rte_eth_dev_tx_queue_start(uint8_t port_id, uint16_t tx_queue_id)
{
	struct rte_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR_RET(-E_RTE_SECONDARY);

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", tx_queue_id);
		return -EINVAL;
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_start, -ENOTSUP);

	return dev->dev_ops->tx_queue_start(dev, tx_queue_id);

}

int
rte_eth_dev_tx_queue_stop(uint8_t port_id, uint16_t tx_queue_id)
{
	struct rte_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR_RET(-E_RTE_SECONDARY);

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	if (tx_queue_id >= dev->data->nb_tx_queues) {
		PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", tx_queue_id);
		return -EINVAL;
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_stop, -ENOTSUP);

	return dev->dev_ops->tx_queue_stop(dev, tx_queue_id);

}

static int
rte_eth_dev_tx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	uint16_t old_nb_queues = dev->data->nb_tx_queues;
	void **txq;
	unsigned i;

	if (dev->data->tx_queues == NULL) { /* first time configuration */
		dev->data->tx_queues = rte_zmalloc("ethdev->tx_queues",
				sizeof(dev->data->tx_queues[0]) * nb_queues,
				RTE_CACHE_LINE_SIZE);
		if (dev->data->tx_queues == NULL) {
			dev->data->nb_tx_queues = 0;
			return -(ENOMEM);
		}
	} else { /* re-configure */
		FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_release, -ENOTSUP);

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

	}
	dev->data->nb_tx_queues = nb_queues;
	return (0);
}

static int
rte_eth_dev_check_vf_rss_rxq_num(uint8_t port_id, uint16_t nb_rx_q)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	switch (nb_rx_q) {
	case 1:
	case 2:
		RTE_ETH_DEV_SRIOV(dev).active =
			ETH_64_POOLS;
		break;
	case 4:
		RTE_ETH_DEV_SRIOV(dev).active =
			ETH_32_POOLS;
		break;
	default:
		return -EINVAL;
	}

	RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool = nb_rx_q;
	RTE_ETH_DEV_SRIOV(dev).def_pool_q_idx =
		dev->pci_dev->max_vfs * nb_rx_q;

	return 0;
}

static int
rte_eth_dev_check_mq_mode(uint8_t port_id, uint16_t nb_rx_q, uint16_t nb_tx_q,
		      const struct rte_eth_conf *dev_conf)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

	if (RTE_ETH_DEV_SRIOV(dev).active != 0) {
		/* check multi-queue mode */
		if ((dev_conf->rxmode.mq_mode == ETH_MQ_RX_DCB) ||
		    (dev_conf->rxmode.mq_mode == ETH_MQ_RX_DCB_RSS) ||
		    (dev_conf->txmode.mq_mode == ETH_MQ_TX_DCB)) {
			/* SRIOV only works in VMDq enable mode */
			PMD_DEBUG_TRACE("ethdev port_id=%" PRIu8
					" SRIOV active, "
					"wrong VMDQ mq_mode rx %u tx %u\n",
					port_id,
					dev_conf->rxmode.mq_mode,
					dev_conf->txmode.mq_mode);
			return (-EINVAL);
		}

		switch (dev_conf->rxmode.mq_mode) {
		case ETH_MQ_RX_VMDQ_DCB:
		case ETH_MQ_RX_VMDQ_DCB_RSS:
			/* DCB/RSS VMDQ in SRIOV mode, not implement yet */
			PMD_DEBUG_TRACE("ethdev port_id=%" PRIu8
					" SRIOV active, "
					"unsupported VMDQ mq_mode rx %u\n",
					port_id, dev_conf->rxmode.mq_mode);
			return (-EINVAL);
		case ETH_MQ_RX_RSS:
			PMD_DEBUG_TRACE("ethdev port_id=%" PRIu8
					" SRIOV active, "
					"Rx mq mode is changed from:"
					"mq_mode %u into VMDQ mq_mode %u\n",
					port_id,
					dev_conf->rxmode.mq_mode,
					dev->data->dev_conf.rxmode.mq_mode);
		case ETH_MQ_RX_VMDQ_RSS:
			dev->data->dev_conf.rxmode.mq_mode = ETH_MQ_RX_VMDQ_RSS;
			if (nb_rx_q <= RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool)
				if (rte_eth_dev_check_vf_rss_rxq_num(port_id, nb_rx_q) != 0) {
					PMD_DEBUG_TRACE("ethdev port_id=%d"
						" SRIOV active, invalid queue"
						" number for VMDQ RSS, allowed"
						" value are 1, 2 or 4\n",
						port_id);
					return -EINVAL;
				}
			break;
		default: /* ETH_MQ_RX_VMDQ_ONLY or ETH_MQ_RX_NONE */
			/* if nothing mq mode configure, use default scheme */
			dev->data->dev_conf.rxmode.mq_mode = ETH_MQ_RX_VMDQ_ONLY;
			if (RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool > 1)
				RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool = 1;
			break;
		}

		switch (dev_conf->txmode.mq_mode) {
		case ETH_MQ_TX_VMDQ_DCB:
			/* DCB VMDQ in SRIOV mode, not implement yet */
			PMD_DEBUG_TRACE("ethdev port_id=%" PRIu8
					" SRIOV active, "
					"unsupported VMDQ mq_mode tx %u\n",
					port_id, dev_conf->txmode.mq_mode);
			return (-EINVAL);
		default: /* ETH_MQ_TX_VMDQ_ONLY or ETH_MQ_TX_NONE */
			/* if nothing mq mode configure, use default scheme */
			dev->data->dev_conf.txmode.mq_mode = ETH_MQ_TX_VMDQ_ONLY;
			break;
		}

		/* check valid queue number */
		if ((nb_rx_q > RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool) ||
		    (nb_tx_q > RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool)) {
			PMD_DEBUG_TRACE("ethdev port_id=%d SRIOV active, "
				    "queue number must less equal to %d\n",
					port_id, RTE_ETH_DEV_SRIOV(dev).nb_q_per_pool);
			return (-EINVAL);
		}
	} else {
		/* For vmdb+dcb mode check our configuration before we go further */
		if (dev_conf->rxmode.mq_mode == ETH_MQ_RX_VMDQ_DCB) {
			const struct rte_eth_vmdq_dcb_conf *conf;

			if (nb_rx_q != ETH_VMDQ_DCB_NUM_QUEUES) {
				PMD_DEBUG_TRACE("ethdev port_id=%d VMDQ+DCB, nb_rx_q "
						"!= %d\n",
						port_id, ETH_VMDQ_DCB_NUM_QUEUES);
				return (-EINVAL);
			}
			conf = &(dev_conf->rx_adv_conf.vmdq_dcb_conf);
			if (! (conf->nb_queue_pools == ETH_16_POOLS ||
			       conf->nb_queue_pools == ETH_32_POOLS)) {
				PMD_DEBUG_TRACE("ethdev port_id=%d VMDQ+DCB selected, "
						"nb_queue_pools must be %d or %d\n",
						port_id, ETH_16_POOLS, ETH_32_POOLS);
				return (-EINVAL);
			}
		}
		if (dev_conf->txmode.mq_mode == ETH_MQ_TX_VMDQ_DCB) {
			const struct rte_eth_vmdq_dcb_tx_conf *conf;

			if (nb_tx_q != ETH_VMDQ_DCB_NUM_QUEUES) {
				PMD_DEBUG_TRACE("ethdev port_id=%d VMDQ+DCB, nb_tx_q "
						"!= %d\n",
						port_id, ETH_VMDQ_DCB_NUM_QUEUES);
				return (-EINVAL);
			}
			conf = &(dev_conf->tx_adv_conf.vmdq_dcb_tx_conf);
			if (! (conf->nb_queue_pools == ETH_16_POOLS ||
			       conf->nb_queue_pools == ETH_32_POOLS)) {
				PMD_DEBUG_TRACE("ethdev port_id=%d VMDQ+DCB selected, "
						"nb_queue_pools != %d or nb_queue_pools "
						"!= %d\n",
						port_id, ETH_16_POOLS, ETH_32_POOLS);
				return (-EINVAL);
			}
		}

		/* For DCB mode check our configuration before we go further */
		if (dev_conf->rxmode.mq_mode == ETH_MQ_RX_DCB) {
			const struct rte_eth_dcb_rx_conf *conf;

			if (nb_rx_q != ETH_DCB_NUM_QUEUES) {
				PMD_DEBUG_TRACE("ethdev port_id=%d DCB, nb_rx_q "
						"!= %d\n",
						port_id, ETH_DCB_NUM_QUEUES);
				return (-EINVAL);
			}
			conf = &(dev_conf->rx_adv_conf.dcb_rx_conf);
			if (! (conf->nb_tcs == ETH_4_TCS ||
			       conf->nb_tcs == ETH_8_TCS)) {
				PMD_DEBUG_TRACE("ethdev port_id=%d DCB selected, "
						"nb_tcs != %d or nb_tcs "
						"!= %d\n",
						port_id, ETH_4_TCS, ETH_8_TCS);
				return (-EINVAL);
			}
		}

		if (dev_conf->txmode.mq_mode == ETH_MQ_TX_DCB) {
			const struct rte_eth_dcb_tx_conf *conf;

			if (nb_tx_q != ETH_DCB_NUM_QUEUES) {
				PMD_DEBUG_TRACE("ethdev port_id=%d DCB, nb_tx_q "
						"!= %d\n",
						port_id, ETH_DCB_NUM_QUEUES);
				return (-EINVAL);
			}
			conf = &(dev_conf->tx_adv_conf.dcb_tx_conf);
			if (! (conf->nb_tcs == ETH_4_TCS ||
			       conf->nb_tcs == ETH_8_TCS)) {
				PMD_DEBUG_TRACE("ethdev port_id=%d DCB selected, "
						"nb_tcs != %d or nb_tcs "
						"!= %d\n",
						port_id, ETH_4_TCS, ETH_8_TCS);
				return (-EINVAL);
			}
		}
	}
	return 0;
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
	PROC_PRIMARY_OR_ERR_RET(-E_RTE_SECONDARY);

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}

	if (nb_rx_q > RTE_MAX_QUEUES_PER_PORT) {
		PMD_DEBUG_TRACE(
			"Number of RX queues requested (%u) is greater than max supported(%d)\n",
			nb_rx_q, RTE_MAX_QUEUES_PER_PORT);
		return (-EINVAL);
	}

	if (nb_tx_q > RTE_MAX_QUEUES_PER_PORT) {
		PMD_DEBUG_TRACE(
			"Number of TX queues requested (%u) is greater than max supported(%d)\n",
			nb_tx_q, RTE_MAX_QUEUES_PER_PORT);
		return (-EINVAL);
	}

	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_configure, -ENOTSUP);

	if (dev->data->dev_started) {
		PMD_DEBUG_TRACE(
		    "port %d must be stopped to allow configuration\n", port_id);
		return (-EBUSY);
	}

	/*
	 * Check that the numbers of RX and TX queues are not greater
	 * than the maximum number of RX and TX queues supported by the
	 * configured device.
	 */
	(*dev->dev_ops->dev_infos_get)(dev, &dev_info);
	if (nb_rx_q > dev_info.max_rx_queues) {
		PMD_DEBUG_TRACE("ethdev port_id=%d nb_rx_queues=%d > %d\n",
				port_id, nb_rx_q, dev_info.max_rx_queues);
		return (-EINVAL);
	}
	if (nb_rx_q == 0) {
		PMD_DEBUG_TRACE("ethdev port_id=%d nb_rx_q == 0\n", port_id);
		return (-EINVAL);
	}

	if (nb_tx_q > dev_info.max_tx_queues) {
		PMD_DEBUG_TRACE("ethdev port_id=%d nb_tx_queues=%d > %d\n",
				port_id, nb_tx_q, dev_info.max_tx_queues);
		return (-EINVAL);
	}
	if (nb_tx_q == 0) {
		PMD_DEBUG_TRACE("ethdev port_id=%d nb_tx_q == 0\n", port_id);
		return (-EINVAL);
	}

	/* Copy the dev_conf parameter into the dev structure */
	memcpy(&dev->data->dev_conf, dev_conf, sizeof(dev->data->dev_conf));

	/*
	 * If link state interrupt is enabled, check that the
	 * device supports it.
	 */
	if (dev_conf->intr_conf.lsc == 1) {
		const struct rte_pci_driver *pci_drv = &dev->driver->pci_drv;

		if (!(pci_drv->drv_flags & RTE_PCI_DRV_INTR_LSC)) {
			PMD_DEBUG_TRACE("driver %s does not support lsc\n",
					pci_drv->name);
			return (-EINVAL);
		}
	}

	/*
	 * If jumbo frames are enabled, check that the maximum RX packet
	 * length is supported by the configured device.
	 */
	if (dev_conf->rxmode.jumbo_frame == 1) {
		if (dev_conf->rxmode.max_rx_pkt_len >
		    dev_info.max_rx_pktlen) {
			PMD_DEBUG_TRACE("ethdev port_id=%d max_rx_pkt_len %u"
				" > max valid value %u\n",
				port_id,
				(unsigned)dev_conf->rxmode.max_rx_pkt_len,
				(unsigned)dev_info.max_rx_pktlen);
			return (-EINVAL);
		}
		else if (dev_conf->rxmode.max_rx_pkt_len < ETHER_MIN_LEN) {
			PMD_DEBUG_TRACE("ethdev port_id=%d max_rx_pkt_len %u"
				" < min valid value %u\n",
				port_id,
				(unsigned)dev_conf->rxmode.max_rx_pkt_len,
				(unsigned)ETHER_MIN_LEN);
			return (-EINVAL);
		}
	} else {
		if (dev_conf->rxmode.max_rx_pkt_len < ETHER_MIN_LEN ||
			dev_conf->rxmode.max_rx_pkt_len > ETHER_MAX_LEN)
			/* Use default value */
			dev->data->dev_conf.rxmode.max_rx_pkt_len =
							ETHER_MAX_LEN;
	}

	/* multipe queue mode checking */
	diag = rte_eth_dev_check_mq_mode(port_id, nb_rx_q, nb_tx_q, dev_conf);
	if (diag != 0) {
		PMD_DEBUG_TRACE("port%d rte_eth_dev_check_mq_mode = %d\n",
				port_id, diag);
		return diag;
	}

	/*
	 * Setup new number of RX/TX queues and reconfigure device.
	 */
	diag = rte_eth_dev_rx_queue_config(dev, nb_rx_q);
	if (diag != 0) {
		PMD_DEBUG_TRACE("port%d rte_eth_dev_rx_queue_config = %d\n",
				port_id, diag);
		return diag;
	}

	diag = rte_eth_dev_tx_queue_config(dev, nb_tx_q);
	if (diag != 0) {
		PMD_DEBUG_TRACE("port%d rte_eth_dev_tx_queue_config = %d\n",
				port_id, diag);
		rte_eth_dev_rx_queue_config(dev, 0);
		return diag;
	}

	diag = (*dev->dev_ops->dev_configure)(dev);
	if (diag != 0) {
		PMD_DEBUG_TRACE("port%d dev_configure = %d\n",
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
	PROC_PRIMARY_OR_ERR_RET(-E_RTE_SECONDARY);

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}

	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_start, -ENOTSUP);

	if (dev->data->dev_started != 0) {
		PMD_DEBUG_TRACE("Device with port_id=%" PRIu8
			" already started\n",
			port_id);
		return (0);
	}

	diag = (*dev->dev_ops->dev_start)(dev);
	if (diag == 0)
		dev->data->dev_started = 1;
	else
		return diag;

	rte_eth_dev_config_restore(port_id);

	if (dev->data->dev_conf.intr_conf.lsc != 0) {
		FUNC_PTR_OR_ERR_RET(*dev->dev_ops->link_update, -ENOTSUP);
		(*dev->dev_ops->link_update)(dev, 0);
	}
	return 0;
}

void
rte_eth_dev_stop(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_RET();

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_RET(*dev->dev_ops->dev_stop);

	if (dev->data->dev_started == 0) {
		PMD_DEBUG_TRACE("Device with port_id=%" PRIu8
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

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR_RET(-E_RTE_SECONDARY);

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_set_link_up, -ENOTSUP);
	return (*dev->dev_ops->dev_set_link_up)(dev);
}

int
rte_eth_dev_set_link_down(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR_RET(-E_RTE_SECONDARY);

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_set_link_down, -ENOTSUP);
	return (*dev->dev_ops->dev_set_link_down)(dev);
}

void
rte_eth_dev_close(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_RET();

	if (!rte_eth_dev_is_valid_port(port_id)) {
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
	int ret;
	uint32_t mbp_buf_size;
	struct rte_eth_dev *dev;
	struct rte_pktmbuf_pool_private *mbp_priv;
	struct rte_eth_dev_info dev_info;

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR_RET(-E_RTE_SECONDARY);

	if (!rte_eth_dev_is_valid_port(port_id)) {
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
		    "port %d must be stopped to allow configuration\n", port_id);
		return -EBUSY;
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_setup, -ENOTSUP);

	/*
	 * Check the size of the mbuf data buffer.
	 * This value must be provided in the private data of the memory pool.
	 * First check that the memory pool has a valid private data.
	 */
	rte_eth_dev_info_get(port_id, &dev_info);
	if (mp->private_data_size < sizeof(struct rte_pktmbuf_pool_private)) {
		PMD_DEBUG_TRACE("%s private_data_size %d < %d\n",
				mp->name, (int) mp->private_data_size,
				(int) sizeof(struct rte_pktmbuf_pool_private));
		return (-ENOSPC);
	}
	mbp_priv = rte_mempool_get_priv(mp);
	mbp_buf_size = mbp_priv->mbuf_data_room_size;

	if ((mbp_buf_size - RTE_PKTMBUF_HEADROOM) < dev_info.min_rx_bufsize) {
		PMD_DEBUG_TRACE("%s mbuf_data_room_size %d < %d "
				"(RTE_PKTMBUF_HEADROOM=%d + min_rx_bufsize(dev)"
				"=%d)\n",
				mp->name,
				(int)mbp_buf_size,
				(int)(RTE_PKTMBUF_HEADROOM +
				      dev_info.min_rx_bufsize),
				(int)RTE_PKTMBUF_HEADROOM,
				(int)dev_info.min_rx_bufsize);
		return (-EINVAL);
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

	/* This function is only safe when called from the primary process
	 * in a multi-process setup*/
	PROC_PRIMARY_OR_ERR_RET(-E_RTE_SECONDARY);

	if (!rte_eth_dev_is_valid_port(port_id)) {
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
		    "port %d must be stopped to allow configuration\n", port_id);
		return -EBUSY;
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_infos_get, -ENOTSUP);
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->tx_queue_setup, -ENOTSUP);

	rte_eth_dev_info_get(port_id, &dev_info);

	if (tx_conf == NULL)
		tx_conf = &dev_info.default_txconf;

	return (*dev->dev_ops->tx_queue_setup)(dev, tx_queue_id, nb_tx_desc,
					       socket_id, tx_conf);
}

void
rte_eth_promiscuous_enable(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.intr_conf.lsc != 0)
		rte_eth_dev_atomic_read_link_status(dev, eth_link);
	else {
		FUNC_PTR_OR_RET(*dev->dev_ops->link_update);
		(*dev->dev_ops->link_update)(dev, 1);
		*eth_link = dev->data->dev_link;
	}
}

void
rte_eth_link_get_nowait(uint8_t port_id, struct rte_eth_link *eth_link)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.intr_conf.lsc != 0)
		rte_eth_dev_atomic_read_link_status(dev, eth_link);
	else {
		FUNC_PTR_OR_RET(*dev->dev_ops->link_update);
		(*dev->dev_ops->link_update)(dev, 0);
		*eth_link = dev->data->dev_link;
	}
}

int
rte_eth_stats_get(uint8_t port_id, struct rte_eth_stats *stats)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	memset(stats, 0, sizeof(*stats));

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->stats_get, -ENOTSUP);
	(*dev->dev_ops->stats_get)(dev, stats);
	stats->rx_nombuf = dev->data->rx_mbuf_alloc_failed;
	return 0;
}

void
rte_eth_stats_reset(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_RET(*dev->dev_ops->stats_reset);
	(*dev->dev_ops->stats_reset)(dev);
}

/* retrieve ethdev extended statistics */
int
rte_eth_xstats_get(uint8_t port_id, struct rte_eth_xstats *xstats,
	unsigned n)
{
	struct rte_eth_stats eth_stats;
	struct rte_eth_dev *dev;
	unsigned count, i, q;
	uint64_t val;
	char *stats_ptr;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -1;
	}

	dev = &rte_eth_devices[port_id];

	/* implemented by the driver */
	if (dev->dev_ops->xstats_get != NULL)
		return (*dev->dev_ops->xstats_get)(dev, xstats, n);

	/* else, return generic statistics */
	count = RTE_NB_STATS;
	count += dev->data->nb_rx_queues * RTE_NB_RXQ_STATS;
	count += dev->data->nb_tx_queues * RTE_NB_TXQ_STATS;
	if (n < count)
		return count;

	/* now fill the xstats structure */

	count = 0;
	memset(&eth_stats, 0, sizeof(eth_stats));
	rte_eth_stats_get(port_id, &eth_stats);

	/* global stats */
	for (i = 0; i < RTE_NB_STATS; i++) {
		stats_ptr = (char *)&eth_stats + rte_stats_strings[i].offset;
		val = *(uint64_t *)stats_ptr;
		snprintf(xstats[count].name, sizeof(xstats[count].name),
			"%s", rte_stats_strings[i].name);
		xstats[count++].value = val;
	}

	/* per-rxq stats */
	for (q = 0; q < dev->data->nb_rx_queues; q++) {
		for (i = 0; i < RTE_NB_RXQ_STATS; i++) {
			stats_ptr = (char *)&eth_stats;
			stats_ptr += rte_rxq_stats_strings[i].offset;
			stats_ptr += q * sizeof(uint64_t);
			val = *(uint64_t *)stats_ptr;
			snprintf(xstats[count].name, sizeof(xstats[count].name),
				"rx_queue_%u_%s", q,
				rte_rxq_stats_strings[i].name);
			xstats[count++].value = val;
		}
	}

	/* per-txq stats */
	for (q = 0; q < dev->data->nb_tx_queues; q++) {
		for (i = 0; i < RTE_NB_TXQ_STATS; i++) {
			stats_ptr = (char *)&eth_stats;
			stats_ptr += rte_txq_stats_strings[i].offset;
			stats_ptr += q * sizeof(uint64_t);
			val = *(uint64_t *)stats_ptr;
			snprintf(xstats[count].name, sizeof(xstats[count].name),
				"tx_queue_%u_%s", q,
				rte_txq_stats_strings[i].name);
			xstats[count++].value = val;
		}
	}

	return count;
}

/* reset ethdev extended statistics */
void
rte_eth_xstats_reset(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}

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

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->queue_stats_mapping_set, -ENOTSUP);
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &rte_eth_devices[port_id];

	memset(dev_info, 0, sizeof(struct rte_eth_dev_info));

	FUNC_PTR_OR_RET(*dev->dev_ops->dev_infos_get);
	(*dev->dev_ops->dev_infos_get)(dev, dev_info);
	dev_info->pci_dev = dev->pci_dev;
	if (dev->driver)
		dev_info->driver_name = dev->driver->pci_drv.name;
}

void
rte_eth_macaddr_get(uint8_t port_id, struct ether_addr *mac_addr)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return;
	}

	dev = &rte_eth_devices[port_id];
	ether_addr_copy(&dev->data->mac_addrs[0], mac_addr);
}


int
rte_eth_dev_get_mtu(uint8_t port_id, uint16_t *mtu)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	*mtu = dev->data->mtu;
	return 0;
}

int
rte_eth_dev_set_mtu(uint8_t port_id, uint16_t mtu)
{
	int ret;
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mtu_set, -ENOTSUP);

	ret = (*dev->dev_ops->mtu_set)(dev, mtu);
	if (!ret)
		dev->data->mtu = mtu;

	return ret;
}

int
rte_eth_dev_vlan_filter(uint8_t port_id, uint16_t vlan_id, int on)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
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

	return (*dev->dev_ops->vlan_filter_set)(dev, vlan_id, on);
}

int
rte_eth_dev_set_vlan_strip_on_queue(uint8_t port_id, uint16_t rx_queue_id, int on)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	if (rx_queue_id >= dev->data->nb_rx_queues) {
		PMD_DEBUG_TRACE("Invalid rx_queue_id=%d\n", port_id);
		return (-EINVAL);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_strip_queue_set, -ENOTSUP);
	(*dev->dev_ops->vlan_strip_queue_set)(dev, rx_queue_id, on);

	return (0);
}

int
rte_eth_dev_set_vlan_ether_type(uint8_t port_id, uint16_t tpid)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_tpid_set, -ENOTSUP);
	(*dev->dev_ops->vlan_tpid_set)(dev, tpid);

	return (0);
}

int
rte_eth_dev_set_vlan_offload(uint8_t port_id, int offload_mask)
{
	struct rte_eth_dev *dev;
	int ret = 0;
	int mask = 0;
	int cur, org = 0;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];

	/*check which option changed by application*/
	cur = !!(offload_mask & ETH_VLAN_STRIP_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.hw_vlan_strip);
	if (cur != org){
		dev->data->dev_conf.rxmode.hw_vlan_strip = (uint8_t)cur;
		mask |= ETH_VLAN_STRIP_MASK;
	}

	cur = !!(offload_mask & ETH_VLAN_FILTER_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.hw_vlan_filter);
	if (cur != org){
		dev->data->dev_conf.rxmode.hw_vlan_filter = (uint8_t)cur;
		mask |= ETH_VLAN_FILTER_MASK;
	}

	cur = !!(offload_mask & ETH_VLAN_EXTEND_OFFLOAD);
	org = !!(dev->data->dev_conf.rxmode.hw_vlan_extend);
	if (cur != org){
		dev->data->dev_conf.rxmode.hw_vlan_extend = (uint8_t)cur;
		mask |= ETH_VLAN_EXTEND_MASK;
	}

	/*no change*/
	if(mask == 0)
		return ret;

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_offload_set, -ENOTSUP);
	(*dev->dev_ops->vlan_offload_set)(dev, mask);

	return ret;
}

int
rte_eth_dev_get_vlan_offload(uint8_t port_id)
{
	struct rte_eth_dev *dev;
	int ret = 0;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];

	if (dev->data->dev_conf.rxmode.hw_vlan_strip)
		ret |= ETH_VLAN_STRIP_OFFLOAD ;

	if (dev->data->dev_conf.rxmode.hw_vlan_filter)
		ret |= ETH_VLAN_FILTER_OFFLOAD ;

	if (dev->data->dev_conf.rxmode.hw_vlan_extend)
		ret |= ETH_VLAN_EXTEND_OFFLOAD ;

	return ret;
}

int
rte_eth_dev_set_vlan_pvid(uint8_t port_id, uint16_t pvid, int on)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_pvid_set, -ENOTSUP);
	(*dev->dev_ops->vlan_pvid_set)(dev, pvid, on);

	return 0;
}

int
rte_eth_dev_fdir_add_signature_filter(uint8_t port_id,
				      struct rte_fdir_filter *fdir_filter,
				      uint8_t queue)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
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
				"None l4type, source & destinations ports " \
				"should be null!\n");
		return (-EINVAL);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_add_signature_filter, -ENOTSUP);
	return (*dev->dev_ops->fdir_add_signature_filter)(dev, fdir_filter,
								queue);
}

int
rte_eth_dev_fdir_update_signature_filter(uint8_t port_id,
					 struct rte_fdir_filter *fdir_filter,
					 uint8_t queue)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
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
				"None l4type, source & destinations ports " \
				"should be null!\n");
		return (-EINVAL);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_update_signature_filter, -ENOTSUP);
	return (*dev->dev_ops->fdir_update_signature_filter)(dev, fdir_filter,
								queue);

}

int
rte_eth_dev_fdir_remove_signature_filter(uint8_t port_id,
					 struct rte_fdir_filter *fdir_filter)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
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
				"should be null!\n");
		return (-EINVAL);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_remove_signature_filter, -ENOTSUP);
	return (*dev->dev_ops->fdir_remove_signature_filter)(dev, fdir_filter);
}

int
rte_eth_dev_fdir_get_infos(uint8_t port_id, struct rte_eth_fdir *fdir)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	if (! (dev->data->dev_conf.fdir_conf.mode)) {
		PMD_DEBUG_TRACE("port %d: pkt-filter disabled\n", port_id);
		return (-ENOSYS);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_infos_get, -ENOTSUP);

	(*dev->dev_ops->fdir_infos_get)(dev, fdir);
	return (0);
}

int
rte_eth_dev_fdir_add_perfect_filter(uint8_t port_id,
				    struct rte_fdir_filter *fdir_filter,
				    uint16_t soft_id, uint8_t queue,
				    uint8_t drop)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
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
				"None l4type, source & destinations ports " \
				"should be null!\n");
		return (-EINVAL);
	}

	/* For now IPv6 is not supported with perfect filter */
	if (fdir_filter->iptype == RTE_FDIR_IPTYPE_IPV6)
		return (-ENOTSUP);

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_add_perfect_filter, -ENOTSUP);
	return (*dev->dev_ops->fdir_add_perfect_filter)(dev, fdir_filter,
								soft_id, queue,
								drop);
}

int
rte_eth_dev_fdir_update_perfect_filter(uint8_t port_id,
				       struct rte_fdir_filter *fdir_filter,
				       uint16_t soft_id, uint8_t queue,
				       uint8_t drop)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
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
				"None l4type, source & destinations ports " \
				"should be null!\n");
		return (-EINVAL);
	}

	/* For now IPv6 is not supported with perfect filter */
	if (fdir_filter->iptype == RTE_FDIR_IPTYPE_IPV6)
		return (-ENOTSUP);

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_update_perfect_filter, -ENOTSUP);
	return (*dev->dev_ops->fdir_update_perfect_filter)(dev, fdir_filter,
							soft_id, queue, drop);
}

int
rte_eth_dev_fdir_remove_perfect_filter(uint8_t port_id,
				       struct rte_fdir_filter *fdir_filter,
				       uint16_t soft_id)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
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
				"None l4type, source & destinations ports " \
				"should be null!\n");
		return (-EINVAL);
	}

	/* For now IPv6 is not supported with perfect filter */
	if (fdir_filter->iptype == RTE_FDIR_IPTYPE_IPV6)
		return (-ENOTSUP);

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_remove_perfect_filter, -ENOTSUP);
	return (*dev->dev_ops->fdir_remove_perfect_filter)(dev, fdir_filter,
								soft_id);
}

int
rte_eth_dev_fdir_set_masks(uint8_t port_id, struct rte_fdir_masks *fdir_mask)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	if (! (dev->data->dev_conf.fdir_conf.mode)) {
		PMD_DEBUG_TRACE("port %d: pkt-filter disabled\n", port_id);
		return (-ENOSYS);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->fdir_set_masks, -ENOTSUP);
	return (*dev->dev_ops->fdir_set_masks)(dev, fdir_mask);
}

int
rte_eth_dev_flow_ctrl_get(uint8_t port_id, struct rte_eth_fc_conf *fc_conf)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->flow_ctrl_get, -ENOTSUP);
	memset(fc_conf, 0, sizeof(*fc_conf));
	return (*dev->dev_ops->flow_ctrl_get)(dev, fc_conf);
}

int
rte_eth_dev_flow_ctrl_set(uint8_t port_id, struct rte_eth_fc_conf *fc_conf)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if ((fc_conf->send_xon != 0) && (fc_conf->send_xon != 1)) {
		PMD_DEBUG_TRACE("Invalid send_xon, only 0/1 allowed\n");
		return (-EINVAL);
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->flow_ctrl_set, -ENOTSUP);
	return (*dev->dev_ops->flow_ctrl_set)(dev, fc_conf);
}

int
rte_eth_dev_priority_flow_ctrl_set(uint8_t port_id, struct rte_eth_pfc_conf *pfc_conf)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if (pfc_conf->priority > (ETH_DCB_NUM_USER_PRIORITIES - 1)) {
		PMD_DEBUG_TRACE("Invalid priority, only 0-7 allowed\n");
		return (-EINVAL);
	}

	dev = &rte_eth_devices[port_id];
	/* High water, low water validation are device specific */
	if  (*dev->dev_ops->priority_flow_ctrl_set)
		return (*dev->dev_ops->priority_flow_ctrl_set)(dev, pfc_conf);
	return (-ENOTSUP);
}

static inline int
rte_eth_check_reta_mask(struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	uint16_t i, num;

	if (!reta_conf)
		return -EINVAL;

	if (reta_size != RTE_ALIGN(reta_size, RTE_RETA_GROUP_SIZE)) {
		PMD_DEBUG_TRACE("Invalid reta size, should be %u aligned\n",
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

static inline int
rte_eth_check_reta_entry(struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size,
			 uint8_t max_rxq)
{
	uint16_t i, idx, shift;

	if (!reta_conf)
		return -EINVAL;

	if (max_rxq == 0) {
		PMD_DEBUG_TRACE("No receive queue is available\n");
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if ((reta_conf[idx].mask & (1ULL << shift)) &&
			(reta_conf[idx].reta[shift] >= max_rxq)) {
			PMD_DEBUG_TRACE("reta_conf[%u]->reta[%u]: %u exceeds "
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

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

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->reta_update, -ENOTSUP);
	return (*dev->dev_ops->reta_update)(dev, reta_conf, reta_size);
}

int
rte_eth_dev_rss_reta_query(uint8_t port_id,
			   struct rte_eth_rss_reta_entry64 *reta_conf,
			   uint16_t reta_size)
{
	struct rte_eth_dev *dev;
	int ret;

	if (port_id >= nb_ports) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	/* Check mask bits */
	ret = rte_eth_check_reta_mask(reta_conf, reta_size);
	if (ret < 0)
		return ret;

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->reta_query, -ENOTSUP);
	return (*dev->dev_ops->reta_query)(dev, reta_conf, reta_size);
}

int
rte_eth_dev_rss_hash_update(uint8_t port_id, struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev *dev;
	uint16_t rss_hash_protos;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	rss_hash_protos = rss_conf->rss_hf;
	if ((rss_hash_protos != 0) &&
	    ((rss_hash_protos & ETH_RSS_PROTO_MASK) == 0)) {
		PMD_DEBUG_TRACE("Invalid rss_hash_protos=0x%x\n",
				rss_hash_protos);
		return (-EINVAL);
	}
	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rss_hash_update, -ENOTSUP);
	return (*dev->dev_ops->rss_hash_update)(dev, rss_conf);
}

int
rte_eth_dev_rss_hash_conf_get(uint8_t port_id,
			      struct rte_eth_rss_conf *rss_conf)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rss_hash_conf_get, -ENOTSUP);
	return (*dev->dev_ops->rss_hash_conf_get)(dev, rss_conf);
}

int
rte_eth_dev_udp_tunnel_add(uint8_t port_id,
			   struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	if (udp_tunnel == NULL) {
		PMD_DEBUG_TRACE("Invalid udp_tunnel parameter\n");
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= RTE_TUNNEL_TYPE_MAX) {
		PMD_DEBUG_TRACE("Invalid tunnel type\n");
		return -EINVAL;
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->udp_tunnel_add, -ENOTSUP);
	return (*dev->dev_ops->udp_tunnel_add)(dev, udp_tunnel);
}

int
rte_eth_dev_udp_tunnel_delete(uint8_t port_id,
			      struct rte_eth_udp_tunnel *udp_tunnel)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	dev = &rte_eth_devices[port_id];

	if (udp_tunnel == NULL) {
		PMD_DEBUG_TRACE("Invalid udp_tunnel parametr\n");
		return -EINVAL;
	}

	if (udp_tunnel->prot_type >= RTE_TUNNEL_TYPE_MAX) {
		PMD_DEBUG_TRACE("Invalid tunnel type\n");
		return -EINVAL;
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->udp_tunnel_del, -ENOTSUP);
	return (*dev->dev_ops->udp_tunnel_del)(dev, udp_tunnel);
}

int
rte_eth_led_on(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
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
	uint64_t pool_mask;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mac_addr_add, -ENOTSUP);

	if (is_zero_ether_addr(addr)) {
		PMD_DEBUG_TRACE("port %d: Cannot add NULL MAC address\n",
			port_id);
		return (-EINVAL);
	}
	if (pool >= ETH_64_POOLS) {
		PMD_DEBUG_TRACE("pool id must be 0-%d\n",ETH_64_POOLS - 1);
		return (-EINVAL);
	}

	index = get_mac_addr_index(port_id, addr);
	if (index < 0) {
		index = get_mac_addr_index(port_id, &null_mac_addr);
		if (index < 0) {
			PMD_DEBUG_TRACE("port %d: MAC address array full\n",
				port_id);
			return (-ENOSPC);
		}
	} else {
		pool_mask = dev->data->mac_pool_sel[index];

		/* Check if both MAC address and pool is alread there, and do nothing */
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
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

	/* reset pool bitmap */
	dev->data->mac_pool_sel[index] = 0;

	return 0;
}

int
rte_eth_dev_set_vf_rxmode(uint8_t port_id,  uint16_t vf,
				uint16_t rx_mode, uint8_t on)
{
	uint16_t num_vfs;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("set VF RX mode:Invalid port_id=%d\n",
				port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);

	num_vfs = dev_info.max_vfs;
	if (vf > num_vfs)
	{
		PMD_DEBUG_TRACE("set VF RX mode:invalid VF id %d\n", vf);
		return (-EINVAL);
	}
	if (rx_mode == 0)
	{
		PMD_DEBUG_TRACE("set VF RX mode:mode mask ca not be zero\n");
		return (-EINVAL);
	}
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_vf_rx_mode, -ENOTSUP);
	return (*dev->dev_ops->set_vf_rx_mode)(dev, vf, rx_mode, on);
}

/*
 * Returns index into MAC address array of addr. Use 00:00:00:00:00:00 to find
 * an empty spot.
 */
static inline int
get_hash_mac_addr_index(uint8_t port_id, struct ether_addr *addr)
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("unicast hash setting:Invalid port_id=%d\n",
			port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	if (is_zero_ether_addr(addr)) {
		PMD_DEBUG_TRACE("port %d: Cannot add NULL MAC address\n",
			port_id);
		return (-EINVAL);
	}

	index = get_hash_mac_addr_index(port_id, addr);
	/* Check if it's already there, and do nothing */
	if ((index >= 0) && (on))
		return 0;

	if (index < 0) {
		if (!on) {
			PMD_DEBUG_TRACE("port %d: the MAC address was not"
				"set in UTA\n", port_id);
			return (-EINVAL);
		}

		index = get_hash_mac_addr_index(port_id, &null_mac_addr);
		if (index < 0) {
			PMD_DEBUG_TRACE("port %d: MAC address array full\n",
					port_id);
			return (-ENOSPC);
		}
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->uc_hash_table_set, -ENOTSUP);
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("unicast hash setting:Invalid port_id=%d\n",
			port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->uc_all_hash_table_set, -ENOTSUP);
	return (*dev->dev_ops->uc_all_hash_table_set)(dev, on);
}

int
rte_eth_dev_set_vf_rx(uint8_t port_id,uint16_t vf, uint8_t on)
{
	uint16_t num_vfs;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);

	num_vfs = dev_info.max_vfs;
	if (vf > num_vfs)
	{
		PMD_DEBUG_TRACE("port %d: invalid vf id\n", port_id);
		return (-EINVAL);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_vf_rx, -ENOTSUP);
	return (*dev->dev_ops->set_vf_rx)(dev, vf,on);
}

int
rte_eth_dev_set_vf_tx(uint8_t port_id,uint16_t vf, uint8_t on)
{
	uint16_t num_vfs;
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("set pool tx:Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);

	num_vfs = dev_info.max_vfs;
	if (vf > num_vfs)
	{
		PMD_DEBUG_TRACE("set pool tx:invalid pool id=%d\n", vf);
		return (-EINVAL);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_vf_tx, -ENOTSUP);
	return (*dev->dev_ops->set_vf_tx)(dev, vf,on);
}

int
rte_eth_dev_set_vf_vlan_filter(uint8_t port_id, uint16_t vlan_id,
				 uint64_t vf_mask,uint8_t vlan_on)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("VF VLAN filter:invalid port id=%d\n",
				port_id);
		return (-ENODEV);
	}
	dev = &rte_eth_devices[port_id];

	if(vlan_id > ETHER_MAX_VLAN_ID)
	{
		PMD_DEBUG_TRACE("VF VLAN filter:invalid VLAN id=%d\n",
			vlan_id);
		return (-EINVAL);
	}
	if (vf_mask == 0)
	{
		PMD_DEBUG_TRACE("VF VLAN filter:pool_mask can not be 0\n");
		return (-EINVAL);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_vf_vlan_filter, -ENOTSUP);
	return (*dev->dev_ops->set_vf_vlan_filter)(dev, vlan_id,
						vf_mask,vlan_on);
}

int rte_eth_set_queue_rate_limit(uint8_t port_id, uint16_t queue_idx,
					uint16_t tx_rate)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_link link;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("set queue rate limit:invalid port id=%d\n",
				port_id);
		return -ENODEV;
	}

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);
	link = dev->data->dev_link;

	if (queue_idx > dev_info.max_tx_queues) {
		PMD_DEBUG_TRACE("set queue rate limit:port %d: "
				"invalid queue id=%d\n", port_id, queue_idx);
		return -EINVAL;
	}

	if (tx_rate > link.link_speed) {
		PMD_DEBUG_TRACE("set queue rate limit:invalid tx_rate=%d, "
				"bigger than link speed= %d\n",
			tx_rate, link.link_speed);
		return -EINVAL;
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_queue_rate_limit, -ENOTSUP);
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("set VF rate limit:invalid port id=%d\n",
				port_id);
		return -ENODEV;
	}

	dev = &rte_eth_devices[port_id];
	rte_eth_dev_info_get(port_id, &dev_info);
	link = dev->data->dev_link;

	if (vf > dev_info.max_vfs) {
		PMD_DEBUG_TRACE("set VF rate limit:port %d: "
				"invalid vf id=%d\n", port_id, vf);
		return -EINVAL;
	}

	if (tx_rate > link.link_speed) {
		PMD_DEBUG_TRACE("set VF rate limit:invalid tx_rate=%d, "
				"bigger than link speed= %d\n",
				tx_rate, link.link_speed);
		return -EINVAL;
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->set_vf_rate_limit, -ENOTSUP);
	return (*dev->dev_ops->set_vf_rate_limit)(dev, vf, tx_rate, q_msk);
}

int
rte_eth_mirror_rule_set(uint8_t port_id,
			struct rte_eth_vmdq_mirror_conf *mirror_conf,
			uint8_t rule_id, uint8_t on)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if (mirror_conf->rule_type_mask == 0) {
		PMD_DEBUG_TRACE("mirror rule type can not be 0.\n");
		return (-EINVAL);
	}

	if (mirror_conf->dst_pool >= ETH_64_POOLS) {
		PMD_DEBUG_TRACE("Invalid dst pool, pool id must"
			"be 0-%d\n",ETH_64_POOLS - 1);
		return (-EINVAL);
	}

	if ((mirror_conf->rule_type_mask & ETH_VMDQ_POOL_MIRROR) &&
		(mirror_conf->pool_mask == 0)) {
		PMD_DEBUG_TRACE("Invalid mirror pool, pool mask can not"
				"be 0.\n");
		return (-EINVAL);
	}

	if(rule_id >= ETH_VMDQ_NUM_MIRROR_RULE)
	{
		PMD_DEBUG_TRACE("Invalid rule_id, rule_id must be 0-%d\n",
			ETH_VMDQ_NUM_MIRROR_RULE - 1);
		return (-EINVAL);
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mirror_rule_set, -ENOTSUP);

	return (*dev->dev_ops->mirror_rule_set)(dev, mirror_conf, rule_id, on);
}

int
rte_eth_mirror_rule_reset(uint8_t port_id, uint8_t rule_id)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if(rule_id >= ETH_VMDQ_NUM_MIRROR_RULE)
	{
		PMD_DEBUG_TRACE("Invalid rule_id, rule_id must be 0-%d\n",
			ETH_VMDQ_NUM_MIRROR_RULE-1);
		return (-EINVAL);
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->mirror_rule_reset, -ENOTSUP);

	return (*dev->dev_ops->mirror_rule_reset)(dev, rule_id);
}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
uint16_t
rte_eth_rx_burst(uint8_t port_id, uint16_t queue_id,
		 struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return 0;
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->rx_pkt_burst, 0);
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

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return 0;
	}

	dev = &rte_eth_devices[port_id];

	FUNC_PTR_OR_ERR_RET(*dev->tx_pkt_burst, 0);
	if (queue_id >= dev->data->nb_tx_queues) {
		PMD_DEBUG_TRACE("Invalid TX queue_id=%d\n", queue_id);
		return 0;
	}
	return (*dev->tx_pkt_burst)(dev->data->tx_queues[queue_id],
						tx_pkts, nb_pkts);
}

uint32_t
rte_eth_rx_queue_count(uint8_t port_id, uint16_t queue_id)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return 0;
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_queue_count, 0);
	return (*dev->dev_ops->rx_queue_count)(dev, queue_id);
}

int
rte_eth_rx_descriptor_done(uint8_t port_id, uint16_t queue_id, uint16_t offset)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->rx_descriptor_done, -ENOTSUP);
	return (*dev->dev_ops->rx_descriptor_done)( \
		dev->data->rx_queues[queue_id], offset);
}
#endif

int
rte_eth_dev_callback_register(uint8_t port_id,
			enum rte_eth_event_type event,
			rte_eth_dev_cb_fn cb_fn, void *cb_arg)
{
	struct rte_eth_dev *dev;
	struct rte_eth_dev_callback *user_cb;

	if (!cb_fn)
		return (-EINVAL);

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}

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
	if (user_cb == NULL && (user_cb = rte_zmalloc("INTR_USER_CALLBACK",
			sizeof(struct rte_eth_dev_callback), 0)) != NULL) {
		user_cb->cb_fn = cb_fn;
		user_cb->cb_arg = cb_arg;
		user_cb->event = event;
		TAILQ_INSERT_TAIL(&(dev->link_intr_cbs), user_cb, next);
	}

	rte_spinlock_unlock(&rte_eth_dev_cb_lock);
	return ((user_cb == NULL) ? -ENOMEM : 0);
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
		return (-EINVAL);

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-EINVAL);
	}

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
	return (ret);
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
#ifdef RTE_NIC_BYPASS
int rte_eth_dev_bypass_init(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if ((dev= &rte_eth_devices[port_id]) == NULL) {
		PMD_DEBUG_TRACE("Invalid port device\n");
		return (-ENODEV);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_init, -ENOTSUP);
	(*dev->dev_ops->bypass_init)(dev);
	return 0;
}

int
rte_eth_dev_bypass_state_show(uint8_t port_id, uint32_t *state)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if ((dev= &rte_eth_devices[port_id]) == NULL) {
		PMD_DEBUG_TRACE("Invalid port device\n");
		return (-ENODEV);
	}
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_state_show, -ENOTSUP);
	(*dev->dev_ops->bypass_state_show)(dev, state);
	return 0;
}

int
rte_eth_dev_bypass_state_set(uint8_t port_id, uint32_t *new_state)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if ((dev= &rte_eth_devices[port_id]) == NULL) {
		PMD_DEBUG_TRACE("Invalid port device\n");
		return (-ENODEV);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_state_set, -ENOTSUP);
	(*dev->dev_ops->bypass_state_set)(dev, new_state);
	return 0;
}

int
rte_eth_dev_bypass_event_show(uint8_t port_id, uint32_t event, uint32_t *state)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if ((dev= &rte_eth_devices[port_id]) == NULL) {
		PMD_DEBUG_TRACE("Invalid port device\n");
		return (-ENODEV);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_state_show, -ENOTSUP);
	(*dev->dev_ops->bypass_event_show)(dev, event, state);
	return 0;
}

int
rte_eth_dev_bypass_event_store(uint8_t port_id, uint32_t event, uint32_t state)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if ((dev= &rte_eth_devices[port_id]) == NULL) {
		PMD_DEBUG_TRACE("Invalid port device\n");
		return (-ENODEV);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_event_set, -ENOTSUP);
	(*dev->dev_ops->bypass_event_set)(dev, event, state);
	return 0;
}

int
rte_eth_dev_wd_timeout_store(uint8_t port_id, uint32_t timeout)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if ((dev= &rte_eth_devices[port_id]) == NULL) {
		PMD_DEBUG_TRACE("Invalid port device\n");
		return (-ENODEV);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_wd_timeout_set, -ENOTSUP);
	(*dev->dev_ops->bypass_wd_timeout_set)(dev, timeout);
	return 0;
}

int
rte_eth_dev_bypass_ver_show(uint8_t port_id, uint32_t *ver)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if ((dev= &rte_eth_devices[port_id]) == NULL) {
		PMD_DEBUG_TRACE("Invalid port device\n");
		return (-ENODEV);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_ver_show, -ENOTSUP);
	(*dev->dev_ops->bypass_ver_show)(dev, ver);
	return 0;
}

int
rte_eth_dev_bypass_wd_timeout_show(uint8_t port_id, uint32_t *wd_timeout)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if ((dev= &rte_eth_devices[port_id]) == NULL) {
		PMD_DEBUG_TRACE("Invalid port device\n");
		return (-ENODEV);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_wd_timeout_show, -ENOTSUP);
	(*dev->dev_ops->bypass_wd_timeout_show)(dev, wd_timeout);
	return 0;
}

int
rte_eth_dev_bypass_wd_reset(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return (-ENODEV);
	}

	if ((dev= &rte_eth_devices[port_id]) == NULL) {
		PMD_DEBUG_TRACE("Invalid port device\n");
		return (-ENODEV);
	}

	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->bypass_wd_reset, -ENOTSUP);
	(*dev->dev_ops->bypass_wd_reset)(dev);
	return 0;
}
#endif

int
rte_eth_dev_filter_supported(uint8_t port_id, enum rte_filter_type filter_type)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->filter_ctrl, -ENOTSUP);
	return (*dev->dev_ops->filter_ctrl)(dev, filter_type,
				RTE_ETH_FILTER_NOP, NULL);
}

int
rte_eth_dev_filter_ctrl(uint8_t port_id, enum rte_filter_type filter_type,
		       enum rte_filter_op filter_op, void *arg)
{
	struct rte_eth_dev *dev;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		PMD_DEBUG_TRACE("Invalid port_id=%d\n", port_id);
		return -ENODEV;
	}

	dev = &rte_eth_devices[port_id];
	FUNC_PTR_OR_ERR_RET(*dev->dev_ops->filter_ctrl, -ENOTSUP);
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
	cb->next = rte_eth_devices[port_id].post_rx_burst_cbs[queue_id];
	rte_eth_devices[port_id].post_rx_burst_cbs[queue_id] = cb;
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
	cb->next = rte_eth_devices[port_id].pre_tx_burst_cbs[queue_id];
	rte_eth_devices[port_id].pre_tx_burst_cbs[queue_id] = cb;
	return cb;
}

int
rte_eth_remove_rx_callback(uint8_t port_id, uint16_t queue_id,
		struct rte_eth_rxtx_callback *user_cb)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	return (-ENOTSUP);
#endif
	/* Check input parameters. */
	if (!rte_eth_dev_is_valid_port(port_id) || user_cb == NULL ||
		    queue_id >= rte_eth_devices[port_id].data->nb_rx_queues) {
		return (-EINVAL);
	}

	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_eth_rxtx_callback *cb = dev->post_rx_burst_cbs[queue_id];
	struct rte_eth_rxtx_callback *prev_cb;

	/* Reset head pointer and remove user cb if first in the list. */
	if (cb == user_cb) {
		dev->post_rx_burst_cbs[queue_id] = user_cb->next;
		return 0;
	}

	/* Remove the user cb from the callback list. */
	do {
		prev_cb = cb;
		cb = cb->next;

		if (cb == user_cb) {
			prev_cb->next = user_cb->next;
			return 0;
		}

	} while (cb != NULL);

	/* Callback wasn't found. */
	return (-EINVAL);
}

int
rte_eth_remove_tx_callback(uint8_t port_id, uint16_t queue_id,
		struct rte_eth_rxtx_callback *user_cb)
{
#ifndef RTE_ETHDEV_RXTX_CALLBACKS
	return (-ENOTSUP);
#endif
	/* Check input parameters. */
	if (!rte_eth_dev_is_valid_port(port_id) || user_cb == NULL ||
		    queue_id >= rte_eth_devices[port_id].data->nb_tx_queues) {
		return (-EINVAL);
	}

	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct rte_eth_rxtx_callback *cb = dev->pre_tx_burst_cbs[queue_id];
	struct rte_eth_rxtx_callback *prev_cb;

	/* Reset head pointer and remove user cb if first in the list. */
	if (cb == user_cb) {
		dev->pre_tx_burst_cbs[queue_id] = user_cb->next;
		return 0;
	}

	/* Remove the user cb from the callback list. */
	do {
		prev_cb = cb;
		cb = cb->next;

		if (cb == user_cb) {
			prev_cb->next = user_cb->next;
			return 0;
		}

	} while (cb != NULL);

	/* Callback wasn't found. */
	return (-EINVAL);
}
