/*
 *   BSD LICENSE
 *
 * Copyright (c) 2013-2017, Wind River Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3) Neither the name of Wind River Systems nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_byteorder.h>
#include <rte_dev.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_io.h>

#include "rte_avp_common.h"
#include "rte_avp_fifo.h"

#include "avp_logs.h"



#define AVP_DEV_TO_PCI(eth_dev) RTE_DEV_TO_PCI((eth_dev)->device)


#define AVP_MAX_MAC_ADDRS 1
#define AVP_MIN_RX_BUFSIZE ETHER_MIN_LEN


/*
 * Defines the number of microseconds to wait before checking the response
 * queue for completion.
 */
#define AVP_REQUEST_DELAY_USECS (5000)

/*
 * Defines the number times to check the response queue for completion before
 * declaring a timeout.
 */
#define AVP_MAX_REQUEST_RETRY (100)

/* Defines the current PCI driver version number */
#define AVP_DPDK_DRIVER_VERSION RTE_AVP_CURRENT_GUEST_VERSION

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_avp_map[] = {
	{ .vendor_id = RTE_AVP_PCI_VENDOR_ID,
	  .device_id = RTE_AVP_PCI_DEVICE_ID,
	  .subsystem_vendor_id = RTE_AVP_PCI_SUB_VENDOR_ID,
	  .subsystem_device_id = RTE_AVP_PCI_SUB_DEVICE_ID,
	  .class_id = RTE_CLASS_ANY_ID,
	},

	{ .vendor_id = 0, /* sentinel */
	},
};


/**@{ AVP device flags */
#define AVP_F_PROMISC (1 << 1)
#define AVP_F_CONFIGURED (1 << 2)
#define AVP_F_LINKUP (1 << 3)
/**@} */

/* Ethernet device validation marker */
#define AVP_ETHDEV_MAGIC 0x92972862

/*
 * Defines the AVP device attributes which are attached to an RTE ethernet
 * device
 */
struct avp_dev {
	uint32_t magic; /**< Memory validation marker */
	uint64_t device_id; /**< Unique system identifier */
	struct ether_addr ethaddr; /**< Host specified MAC address */
	struct rte_eth_dev_data *dev_data;
	/**< Back pointer to ethernet device data */
	volatile uint32_t flags; /**< Device operational flags */
	uint8_t port_id; /**< Ethernet port identifier */
	struct rte_mempool *pool; /**< pkt mbuf mempool */
	unsigned int guest_mbuf_size; /**< local pool mbuf size */
	unsigned int host_mbuf_size; /**< host mbuf size */
	unsigned int max_rx_pkt_len; /**< maximum receive unit */
	uint32_t host_features; /**< Supported feature bitmap */
	uint32_t features; /**< Enabled feature bitmap */
	unsigned int num_tx_queues; /**< Negotiated number of transmit queues */
	unsigned int max_tx_queues; /**< Maximum number of transmit queues */
	unsigned int num_rx_queues; /**< Negotiated number of receive queues */
	unsigned int max_rx_queues; /**< Maximum number of receive queues */

	struct rte_avp_fifo *tx_q[RTE_AVP_MAX_QUEUES]; /**< TX queue */
	struct rte_avp_fifo *rx_q[RTE_AVP_MAX_QUEUES]; /**< RX queue */
	struct rte_avp_fifo *alloc_q[RTE_AVP_MAX_QUEUES];
	/**< Allocated mbufs queue */
	struct rte_avp_fifo *free_q[RTE_AVP_MAX_QUEUES];
	/**< To be freed mbufs queue */

	/* For request & response */
	struct rte_avp_fifo *req_q; /**< Request queue */
	struct rte_avp_fifo *resp_q; /**< Response queue */
	void *host_sync_addr; /**< (host) Req/Resp Mem address */
	void *sync_addr; /**< Req/Resp Mem address */
	void *host_mbuf_addr; /**< (host) MBUF pool start address */
	void *mbuf_addr; /**< MBUF pool start address */
} __rte_cache_aligned;

/* RTE ethernet private data */
struct avp_adapter {
	struct avp_dev avp;
} __rte_cache_aligned;


/* 32-bit MMIO register write */
#define AVP_WRITE32(_value, _addr) rte_write32_relaxed((_value), (_addr))

/* 32-bit MMIO register read */
#define AVP_READ32(_addr) rte_read32_relaxed((_addr))

/* Macro to cast the ethernet device private data to a AVP object */
#define AVP_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct avp_adapter *)adapter)->avp)

/*
 * Defines the structure of a AVP device queue for the purpose of handling the
 * receive and transmit burst callback functions
 */
struct avp_queue {
	struct rte_eth_dev_data *dev_data;
	/**< Backpointer to ethernet device data */
	struct avp_dev *avp; /**< Backpointer to AVP device */
	uint16_t queue_id;
	/**< Queue identifier used for indexing current queue */
	uint16_t queue_base;
	/**< Base queue identifier for queue servicing */
	uint16_t queue_limit;
	/**< Maximum queue identifier for queue servicing */

	uint64_t packets;
	uint64_t bytes;
	uint64_t errors;
};

/* translate from host physical address to guest virtual address */
static void *
avp_dev_translate_address(struct rte_eth_dev *eth_dev,
			  phys_addr_t host_phys_addr)
{
	struct rte_pci_device *pci_dev = AVP_DEV_TO_PCI(eth_dev);
	struct rte_mem_resource *resource;
	struct rte_avp_memmap_info *info;
	struct rte_avp_memmap *map;
	off_t offset;
	void *addr;
	unsigned int i;

	addr = pci_dev->mem_resource[RTE_AVP_PCI_MEMORY_BAR].addr;
	resource = &pci_dev->mem_resource[RTE_AVP_PCI_MEMMAP_BAR];
	info = (struct rte_avp_memmap_info *)resource->addr;

	offset = 0;
	for (i = 0; i < info->nb_maps; i++) {
		/* search all segments looking for a matching address */
		map = &info->maps[i];

		if ((host_phys_addr >= map->phys_addr) &&
			(host_phys_addr < (map->phys_addr + map->length))) {
			/* address is within this segment */
			offset += (host_phys_addr - map->phys_addr);
			addr = RTE_PTR_ADD(addr, offset);

			PMD_DRV_LOG(DEBUG, "Translating host physical 0x%" PRIx64 " to guest virtual 0x%p\n",
				    host_phys_addr, addr);

			return addr;
		}
		offset += map->length;
	}

	return NULL;
}

/* verify that the incoming device version is compatible with our version */
static int
avp_dev_version_check(uint32_t version)
{
	uint32_t driver = RTE_AVP_STRIP_MINOR_VERSION(AVP_DPDK_DRIVER_VERSION);
	uint32_t device = RTE_AVP_STRIP_MINOR_VERSION(version);

	if (device <= driver) {
		/* the host driver version is less than or equal to ours */
		return 0;
	}

	return 1;
}

/* verify that memory regions have expected version and validation markers */
static int
avp_dev_check_regions(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = AVP_DEV_TO_PCI(eth_dev);
	struct rte_avp_memmap_info *memmap;
	struct rte_avp_device_info *info;
	struct rte_mem_resource *resource;
	unsigned int i;

	/* Dump resource info for debug */
	for (i = 0; i < PCI_MAX_RESOURCE; i++) {
		resource = &pci_dev->mem_resource[i];
		if ((resource->phys_addr == 0) || (resource->len == 0))
			continue;

		PMD_DRV_LOG(DEBUG, "resource[%u]: phys=0x%" PRIx64 " len=%" PRIu64 " addr=%p\n",
			    i, resource->phys_addr,
			    resource->len, resource->addr);

		switch (i) {
		case RTE_AVP_PCI_MEMMAP_BAR:
			memmap = (struct rte_avp_memmap_info *)resource->addr;
			if ((memmap->magic != RTE_AVP_MEMMAP_MAGIC) ||
			    (memmap->version != RTE_AVP_MEMMAP_VERSION)) {
				PMD_DRV_LOG(ERR, "Invalid memmap magic 0x%08x and version %u\n",
					    memmap->magic, memmap->version);
				return -EINVAL;
			}
			break;

		case RTE_AVP_PCI_DEVICE_BAR:
			info = (struct rte_avp_device_info *)resource->addr;
			if ((info->magic != RTE_AVP_DEVICE_MAGIC) ||
			    avp_dev_version_check(info->version)) {
				PMD_DRV_LOG(ERR, "Invalid device info magic 0x%08x or version 0x%08x > 0x%08x\n",
					    info->magic, info->version,
					    AVP_DPDK_DRIVER_VERSION);
				return -EINVAL;
			}
			break;

		case RTE_AVP_PCI_MEMORY_BAR:
		case RTE_AVP_PCI_MMIO_BAR:
			if (resource->addr == NULL) {
				PMD_DRV_LOG(ERR, "Missing address space for BAR%u\n",
					    i);
				return -EINVAL;
			}
			break;

		case RTE_AVP_PCI_MSIX_BAR:
		default:
			/* no validation required */
			break;
		}
	}

	return 0;
}

/*
 * create a AVP device using the supplied device info by first translating it
 * to guest address space(s).
 */
static int
avp_dev_create(struct rte_pci_device *pci_dev,
	       struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_avp_device_info *host_info;
	struct rte_mem_resource *resource;
	unsigned int i;

	resource = &pci_dev->mem_resource[RTE_AVP_PCI_DEVICE_BAR];
	if (resource->addr == NULL) {
		PMD_DRV_LOG(ERR, "BAR%u is not mapped\n",
			    RTE_AVP_PCI_DEVICE_BAR);
		return -EFAULT;
	}
	host_info = (struct rte_avp_device_info *)resource->addr;

	if ((host_info->magic != RTE_AVP_DEVICE_MAGIC) ||
		avp_dev_version_check(host_info->version)) {
		PMD_DRV_LOG(ERR, "Invalid AVP PCI device, magic 0x%08x version 0x%08x > 0x%08x\n",
			    host_info->magic, host_info->version,
			    AVP_DPDK_DRIVER_VERSION);
		return -EINVAL;
	}

	PMD_DRV_LOG(DEBUG, "AVP host device is v%u.%u.%u\n",
		    RTE_AVP_GET_RELEASE_VERSION(host_info->version),
		    RTE_AVP_GET_MAJOR_VERSION(host_info->version),
		    RTE_AVP_GET_MINOR_VERSION(host_info->version));

	PMD_DRV_LOG(DEBUG, "AVP host supports %u to %u TX queue(s)\n",
		    host_info->min_tx_queues, host_info->max_tx_queues);
	PMD_DRV_LOG(DEBUG, "AVP host supports %u to %u RX queue(s)\n",
		    host_info->min_rx_queues, host_info->max_rx_queues);
	PMD_DRV_LOG(DEBUG, "AVP host supports features 0x%08x\n",
		    host_info->features);

	if (avp->magic != AVP_ETHDEV_MAGIC) {
		/*
		 * First time initialization (i.e., not during a VM
		 * migration)
		 */
		memset(avp, 0, sizeof(*avp));
		avp->magic = AVP_ETHDEV_MAGIC;
		avp->dev_data = eth_dev->data;
		avp->port_id = eth_dev->data->port_id;
		avp->host_mbuf_size = host_info->mbuf_size;
		avp->host_features = host_info->features;
		memcpy(&avp->ethaddr.addr_bytes[0],
		       host_info->ethaddr, ETHER_ADDR_LEN);
		/* adjust max values to not exceed our max */
		avp->max_tx_queues =
			RTE_MIN(host_info->max_tx_queues, RTE_AVP_MAX_QUEUES);
		avp->max_rx_queues =
			RTE_MIN(host_info->max_rx_queues, RTE_AVP_MAX_QUEUES);
	} else {
		/* Re-attaching during migration */

		/* TODO... requires validation of host values */
		if ((host_info->features & avp->features) != avp->features) {
			PMD_DRV_LOG(ERR, "AVP host features mismatched; 0x%08x, host=0x%08x\n",
				    avp->features, host_info->features);
			/* this should not be possible; continue for now */
		}
	}

	/* the device id is allowed to change over migrations */
	avp->device_id = host_info->device_id;

	/* translate incoming host addresses to guest address space */
	PMD_DRV_LOG(DEBUG, "AVP first host tx queue at 0x%" PRIx64 "\n",
		    host_info->tx_phys);
	PMD_DRV_LOG(DEBUG, "AVP first host alloc queue at 0x%" PRIx64 "\n",
		    host_info->alloc_phys);
	for (i = 0; i < avp->max_tx_queues; i++) {
		avp->tx_q[i] = avp_dev_translate_address(eth_dev,
			host_info->tx_phys + (i * host_info->tx_size));

		avp->alloc_q[i] = avp_dev_translate_address(eth_dev,
			host_info->alloc_phys + (i * host_info->alloc_size));
	}

	PMD_DRV_LOG(DEBUG, "AVP first host rx queue at 0x%" PRIx64 "\n",
		    host_info->rx_phys);
	PMD_DRV_LOG(DEBUG, "AVP first host free queue at 0x%" PRIx64 "\n",
		    host_info->free_phys);
	for (i = 0; i < avp->max_rx_queues; i++) {
		avp->rx_q[i] = avp_dev_translate_address(eth_dev,
			host_info->rx_phys + (i * host_info->rx_size));
		avp->free_q[i] = avp_dev_translate_address(eth_dev,
			host_info->free_phys + (i * host_info->free_size));
	}

	PMD_DRV_LOG(DEBUG, "AVP host request queue at 0x%" PRIx64 "\n",
		    host_info->req_phys);
	PMD_DRV_LOG(DEBUG, "AVP host response queue at 0x%" PRIx64 "\n",
		    host_info->resp_phys);
	PMD_DRV_LOG(DEBUG, "AVP host sync address at 0x%" PRIx64 "\n",
		    host_info->sync_phys);
	PMD_DRV_LOG(DEBUG, "AVP host mbuf address at 0x%" PRIx64 "\n",
		    host_info->mbuf_phys);
	avp->req_q = avp_dev_translate_address(eth_dev, host_info->req_phys);
	avp->resp_q = avp_dev_translate_address(eth_dev, host_info->resp_phys);
	avp->sync_addr =
		avp_dev_translate_address(eth_dev, host_info->sync_phys);
	avp->mbuf_addr =
		avp_dev_translate_address(eth_dev, host_info->mbuf_phys);

	/*
	 * store the host mbuf virtual address so that we can calculate
	 * relative offsets for each mbuf as they are processed
	 */
	avp->host_mbuf_addr = host_info->mbuf_va;
	avp->host_sync_addr = host_info->sync_va;

	/*
	 * store the maximum packet length that is supported by the host.
	 */
	avp->max_rx_pkt_len = host_info->max_rx_pkt_len;
	PMD_DRV_LOG(DEBUG, "AVP host max receive packet length is %u\n",
				host_info->max_rx_pkt_len);

	return 0;
}

/*
 * This function is based on probe() function in avp_pci.c
 * It returns 0 on success.
 */
static int
eth_avp_dev_init(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp =
		AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_pci_device *pci_dev;
	int ret;

	pci_dev = AVP_DEV_TO_PCI(eth_dev);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		/*
		 * no setup required on secondary processes.  All data is saved
		 * in dev_private by the primary process. All resource should
		 * be mapped to the same virtual address so all pointers should
		 * be valid.
		 */
		return 0;
	}

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	eth_dev->data->dev_flags |= RTE_ETH_DEV_DETACHABLE;

	/* Check BAR resources */
	ret = avp_dev_check_regions(eth_dev);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to validate BAR resources, ret=%d\n",
			    ret);
		return ret;
	}

	/* Handle each subtype */
	ret = avp_dev_create(pci_dev, eth_dev);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to create device, ret=%d\n", ret);
		return ret;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("avp_ethdev", ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate %d bytes needed to store MAC addresses\n",
			    ETHER_ADDR_LEN);
		return -ENOMEM;
	}

	/* Get a mac from device config */
	ether_addr_copy(&avp->ethaddr, &eth_dev->data->mac_addrs[0]);

	return 0;
}

static int
eth_avp_dev_uninit(struct rte_eth_dev *eth_dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	if (eth_dev->data == NULL)
		return 0;

	if (eth_dev->data->mac_addrs != NULL) {
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
	}

	return 0;
}


static struct eth_driver rte_avp_pmd = {
	{
		.id_table = pci_id_avp_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
		.probe = rte_eth_dev_pci_probe,
		.remove = rte_eth_dev_pci_remove,
	},
	.eth_dev_init = eth_avp_dev_init,
	.eth_dev_uninit = eth_avp_dev_uninit,
	.dev_private_size = sizeof(struct avp_adapter),
};

RTE_PMD_REGISTER_PCI(net_avp, rte_avp_pmd.pci_drv);
RTE_PMD_REGISTER_PCI_TABLE(net_avp, pci_id_avp_map);
