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



static int avp_dev_configure(struct rte_eth_dev *dev);
static void avp_dev_info_get(struct rte_eth_dev *dev,
			     struct rte_eth_dev_info *dev_info);
static void avp_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static int avp_dev_link_update(struct rte_eth_dev *dev,
			       __rte_unused int wait_to_complete);
static int avp_dev_rx_queue_setup(struct rte_eth_dev *dev,
				  uint16_t rx_queue_id,
				  uint16_t nb_rx_desc,
				  unsigned int socket_id,
				  const struct rte_eth_rxconf *rx_conf,
				  struct rte_mempool *pool);

static int avp_dev_tx_queue_setup(struct rte_eth_dev *dev,
				  uint16_t tx_queue_id,
				  uint16_t nb_tx_desc,
				  unsigned int socket_id,
				  const struct rte_eth_txconf *tx_conf);

static void avp_dev_rx_queue_release(void *rxq);
static void avp_dev_tx_queue_release(void *txq);
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

/*
 * dev_ops for avp, bare necessities for basic operation
 */
static const struct eth_dev_ops avp_eth_dev_ops = {
	.dev_configure       = avp_dev_configure,
	.dev_infos_get       = avp_dev_info_get,
	.vlan_offload_set    = avp_vlan_offload_set,
	.link_update         = avp_dev_link_update,
	.rx_queue_setup      = avp_dev_rx_queue_setup,
	.rx_queue_release    = avp_dev_rx_queue_release,
	.tx_queue_setup      = avp_dev_tx_queue_setup,
	.tx_queue_release    = avp_dev_tx_queue_release,
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

/* send a request and wait for a response
 *
 * @warning must be called while holding the avp->lock spinlock.
 */
static int
avp_dev_process_request(struct avp_dev *avp, struct rte_avp_request *request)
{
	unsigned int retry = AVP_MAX_REQUEST_RETRY;
	void *resp_addr = NULL;
	unsigned int count;
	int ret;

	PMD_DRV_LOG(DEBUG, "Sending request %u to host\n", request->req_id);

	request->result = -ENOTSUP;

	/* Discard any stale responses before starting a new request */
	while (avp_fifo_get(avp->resp_q, (void **)&resp_addr, 1))
		PMD_DRV_LOG(DEBUG, "Discarding stale response\n");

	rte_memcpy(avp->sync_addr, request, sizeof(*request));
	count = avp_fifo_put(avp->req_q, &avp->host_sync_addr, 1);
	if (count < 1) {
		PMD_DRV_LOG(ERR, "Cannot send request %u to host\n",
			    request->req_id);
		ret = -EBUSY;
		goto done;
	}

	while (retry--) {
		/* wait for a response */
		usleep(AVP_REQUEST_DELAY_USECS);

		count = avp_fifo_count(avp->resp_q);
		if (count >= 1) {
			/* response received */
			break;
		}

		if ((count < 1) && (retry == 0)) {
			PMD_DRV_LOG(ERR, "Timeout while waiting for a response for %u\n",
				    request->req_id);
			ret = -ETIME;
			goto done;
		}
	}

	/* retrieve the response */
	count = avp_fifo_get(avp->resp_q, (void **)&resp_addr, 1);
	if ((count != 1) || (resp_addr != avp->host_sync_addr)) {
		PMD_DRV_LOG(ERR, "Invalid response from host, count=%u resp=%p host_sync_addr=%p\n",
			    count, resp_addr, avp->host_sync_addr);
		ret = -ENODATA;
		goto done;
	}

	/* copy to user buffer */
	rte_memcpy(request, avp->sync_addr, sizeof(*request));
	ret = 0;

	PMD_DRV_LOG(DEBUG, "Result %d received for request %u\n",
		    request->result, request->req_id);

done:
	return ret;
}

static int
avp_dev_ctrl_set_config(struct rte_eth_dev *eth_dev,
			struct rte_avp_device_config *config)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_avp_request request;
	int ret;

	/* setup a configure request */
	memset(&request, 0, sizeof(request));
	request.req_id = RTE_AVP_REQ_CFG_DEVICE;
	memcpy(&request.config, config, sizeof(request.config));

	ret = avp_dev_process_request(avp, &request);

	return ret == 0 ? request.result : ret;
}

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

static void
_avp_set_rx_queue_mappings(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	struct avp_dev *avp =
		AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct avp_queue *rxq;
	uint16_t queue_count;
	uint16_t remainder;

	rxq = (struct avp_queue *)eth_dev->data->rx_queues[rx_queue_id];

	/*
	 * Must map all AVP fifos as evenly as possible between the configured
	 * device queues.  Each device queue will service a subset of the AVP
	 * fifos. If there is an odd number of device queues the first set of
	 * device queues will get the extra AVP fifos.
	 */
	queue_count = avp->num_rx_queues / eth_dev->data->nb_rx_queues;
	remainder = avp->num_rx_queues % eth_dev->data->nb_rx_queues;
	if (rx_queue_id < remainder) {
		/* these queues must service one extra FIFO */
		rxq->queue_base = rx_queue_id * (queue_count + 1);
		rxq->queue_limit = rxq->queue_base + (queue_count + 1) - 1;
	} else {
		/* these queues service the regular number of FIFO */
		rxq->queue_base = ((remainder * (queue_count + 1)) +
				   ((rx_queue_id - remainder) * queue_count));
		rxq->queue_limit = rxq->queue_base + queue_count - 1;
	}

	PMD_DRV_LOG(DEBUG, "rxq %u at %p base %u limit %u\n",
		    rx_queue_id, rxq, rxq->queue_base, rxq->queue_limit);

	rxq->queue_id = rxq->queue_base;
}

static void
_avp_set_queue_counts(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = AVP_DEV_TO_PCI(eth_dev);
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_avp_device_info *host_info;
	void *addr;

	addr = pci_dev->mem_resource[RTE_AVP_PCI_DEVICE_BAR].addr;
	host_info = (struct rte_avp_device_info *)addr;

	/*
	 * the transmit direction is not negotiated beyond respecting the max
	 * number of queues because the host can handle arbitrary guest tx
	 * queues (host rx queues).
	 */
	avp->num_tx_queues = eth_dev->data->nb_tx_queues;

	/*
	 * the receive direction is more restrictive.  The host requires a
	 * minimum number of guest rx queues (host tx queues) therefore
	 * negotiate a value that is at least as large as the host minimum
	 * requirement.  If the host and guest values are not identical then a
	 * mapping will be established in the receive_queue_setup function.
	 */
	avp->num_rx_queues = RTE_MAX(host_info->min_rx_queues,
				     eth_dev->data->nb_rx_queues);

	PMD_DRV_LOG(DEBUG, "Requesting %u Tx and %u Rx queues from host\n",
		    avp->num_tx_queues, avp->num_rx_queues);
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
	eth_dev->dev_ops = &avp_eth_dev_ops;

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

static int
avp_dev_rx_queue_setup(struct rte_eth_dev *eth_dev,
		       uint16_t rx_queue_id,
		       uint16_t nb_rx_desc,
		       unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *pool)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_pktmbuf_pool_private *mbp_priv;
	struct avp_queue *rxq;

	if (rx_queue_id >= eth_dev->data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "RX queue id is out of range: rx_queue_id=%u, nb_rx_queues=%u\n",
			    rx_queue_id, eth_dev->data->nb_rx_queues);
		return -EINVAL;
	}

	/* Save mbuf pool pointer */
	avp->pool = pool;

	/* Save the local mbuf size */
	mbp_priv = rte_mempool_get_priv(pool);
	avp->guest_mbuf_size = (uint16_t)(mbp_priv->mbuf_data_room_size);
	avp->guest_mbuf_size -= RTE_PKTMBUF_HEADROOM;

	PMD_DRV_LOG(DEBUG, "AVP max_rx_pkt_len=(%u,%u) mbuf_size=(%u,%u)\n",
		    avp->max_rx_pkt_len,
		    eth_dev->data->dev_conf.rxmode.max_rx_pkt_len,
		    avp->host_mbuf_size,
		    avp->guest_mbuf_size);

	/* allocate a queue object */
	rxq = rte_zmalloc_socket("ethdev RX queue", sizeof(struct avp_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate new Rx queue object\n");
		return -ENOMEM;
	}

	/* save back pointers to AVP and Ethernet devices */
	rxq->avp = avp;
	rxq->dev_data = eth_dev->data;
	eth_dev->data->rx_queues[rx_queue_id] = (void *)rxq;

	/* setup the queue receive mapping for the current queue. */
	_avp_set_rx_queue_mappings(eth_dev, rx_queue_id);

	PMD_DRV_LOG(DEBUG, "Rx queue %u setup at %p\n", rx_queue_id, rxq);

	(void)nb_rx_desc;
	(void)rx_conf;
	return 0;
}

static int
avp_dev_tx_queue_setup(struct rte_eth_dev *eth_dev,
		       uint16_t tx_queue_id,
		       uint16_t nb_tx_desc,
		       unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct avp_queue *txq;

	if (tx_queue_id >= eth_dev->data->nb_tx_queues) {
		PMD_DRV_LOG(ERR, "TX queue id is out of range: tx_queue_id=%u, nb_tx_queues=%u\n",
			    tx_queue_id, eth_dev->data->nb_tx_queues);
		return -EINVAL;
	}

	/* allocate a queue object */
	txq = rte_zmalloc_socket("ethdev TX queue", sizeof(struct avp_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate new Tx queue object\n");
		return -ENOMEM;
	}

	/* only the configured set of transmit queues are used */
	txq->queue_id = tx_queue_id;
	txq->queue_base = tx_queue_id;
	txq->queue_limit = tx_queue_id;

	/* save back pointers to AVP and Ethernet devices */
	txq->avp = avp;
	txq->dev_data = eth_dev->data;
	eth_dev->data->tx_queues[tx_queue_id] = (void *)txq;

	PMD_DRV_LOG(DEBUG, "Tx queue %u setup at %p\n", tx_queue_id, txq);

	(void)nb_tx_desc;
	(void)tx_conf;
	return 0;
}

static void
avp_dev_rx_queue_release(void *rx_queue)
{
	struct avp_queue *rxq = (struct avp_queue *)rx_queue;
	struct avp_dev *avp = rxq->avp;
	struct rte_eth_dev_data *data = avp->dev_data;
	unsigned int i;

	for (i = 0; i < avp->num_rx_queues; i++) {
		if (data->rx_queues[i] == rxq)
			data->rx_queues[i] = NULL;
	}
}

static void
avp_dev_tx_queue_release(void *tx_queue)
{
	struct avp_queue *txq = (struct avp_queue *)tx_queue;
	struct avp_dev *avp = txq->avp;
	struct rte_eth_dev_data *data = avp->dev_data;
	unsigned int i;

	for (i = 0; i < avp->num_tx_queues; i++) {
		if (data->tx_queues[i] == txq)
			data->tx_queues[i] = NULL;
	}
}

static int
avp_dev_configure(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = AVP_DEV_TO_PCI(eth_dev);
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_avp_device_info *host_info;
	struct rte_avp_device_config config;
	int mask = 0;
	void *addr;
	int ret;

	addr = pci_dev->mem_resource[RTE_AVP_PCI_DEVICE_BAR].addr;
	host_info = (struct rte_avp_device_info *)addr;

	/* Setup required number of queues */
	_avp_set_queue_counts(eth_dev);

	mask = (ETH_VLAN_STRIP_MASK |
		ETH_VLAN_FILTER_MASK |
		ETH_VLAN_EXTEND_MASK);
	avp_vlan_offload_set(eth_dev, mask);

	/* update device config */
	memset(&config, 0, sizeof(config));
	config.device_id = host_info->device_id;
	config.driver_type = RTE_AVP_DRIVER_TYPE_DPDK;
	config.driver_version = AVP_DPDK_DRIVER_VERSION;
	config.features = avp->features;
	config.num_tx_queues = avp->num_tx_queues;
	config.num_rx_queues = avp->num_rx_queues;

	ret = avp_dev_ctrl_set_config(eth_dev, &config);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Config request failed by host, ret=%d\n",
			    ret);
		goto unlock;
	}

	avp->flags |= AVP_F_CONFIGURED;
	ret = 0;

unlock:
	return ret;
}


static int
avp_dev_link_update(struct rte_eth_dev *eth_dev,
					__rte_unused int wait_to_complete)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_eth_link *link = &eth_dev->data->dev_link;

	link->link_speed = ETH_SPEED_NUM_10G;
	link->link_duplex = ETH_LINK_FULL_DUPLEX;
	link->link_status = !!(avp->flags & AVP_F_LINKUP);

	return -1;
}


static void
avp_dev_info_get(struct rte_eth_dev *eth_dev,
		 struct rte_eth_dev_info *dev_info)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	dev_info->driver_name = "rte_avp_pmd";
	dev_info->pci_dev = RTE_DEV_TO_PCI(eth_dev->device);
	dev_info->max_rx_queues = avp->max_rx_queues;
	dev_info->max_tx_queues = avp->max_tx_queues;
	dev_info->min_rx_bufsize = AVP_MIN_RX_BUFSIZE;
	dev_info->max_rx_pktlen = avp->max_rx_pkt_len;
	dev_info->max_mac_addrs = AVP_MAX_MAC_ADDRS;
	if (avp->host_features & RTE_AVP_FEATURE_VLAN_OFFLOAD) {
		dev_info->rx_offload_capa = DEV_RX_OFFLOAD_VLAN_STRIP;
		dev_info->tx_offload_capa = DEV_TX_OFFLOAD_VLAN_INSERT;
	}
}

static void
avp_vlan_offload_set(struct rte_eth_dev *eth_dev, int mask)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	if (mask & ETH_VLAN_STRIP_MASK) {
		if (avp->host_features & RTE_AVP_FEATURE_VLAN_OFFLOAD) {
			if (eth_dev->data->dev_conf.rxmode.hw_vlan_strip)
				avp->features |= RTE_AVP_FEATURE_VLAN_OFFLOAD;
			else
				avp->features &= ~RTE_AVP_FEATURE_VLAN_OFFLOAD;
		} else {
			PMD_DRV_LOG(ERR, "VLAN strip offload not supported\n");
		}
	}

	if (mask & ETH_VLAN_FILTER_MASK) {
		if (eth_dev->data->dev_conf.rxmode.hw_vlan_filter)
			PMD_DRV_LOG(ERR, "VLAN filter offload not supported\n");
	}

	if (mask & ETH_VLAN_EXTEND_MASK) {
		if (eth_dev->data->dev_conf.rxmode.hw_vlan_extend)
			PMD_DRV_LOG(ERR, "VLAN extend offload not supported\n");
	}
}

RTE_PMD_REGISTER_PCI(net_avp, rte_avp_pmd.pci_drv);
RTE_PMD_REGISTER_PCI_TABLE(net_avp, pci_id_avp_map);
