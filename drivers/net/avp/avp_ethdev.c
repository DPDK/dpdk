/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2013-2017 Wind River Systems, Inc.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ether.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_spinlock.h>
#include <rte_byteorder.h>
#include <rte_dev.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_io.h>

#include "rte_avp_common.h"
#include "rte_avp_fifo.h"

#include "avp_logs.h"

static int avp_dev_create(struct rte_pci_device *pci_dev,
			  struct rte_eth_dev *eth_dev);

static int avp_dev_configure(struct rte_eth_dev *dev);
static int avp_dev_start(struct rte_eth_dev *dev);
static int avp_dev_stop(struct rte_eth_dev *dev);
static int avp_dev_close(struct rte_eth_dev *dev);
static int avp_dev_info_get(struct rte_eth_dev *dev,
			    struct rte_eth_dev_info *dev_info);
static int avp_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static int avp_dev_link_update(struct rte_eth_dev *dev, int wait_to_complete);
static int avp_dev_promiscuous_enable(struct rte_eth_dev *dev);
static int avp_dev_promiscuous_disable(struct rte_eth_dev *dev);

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

static uint16_t avp_recv_scattered_pkts(void *rx_queue,
					struct rte_mbuf **rx_pkts,
					uint16_t nb_pkts);

static uint16_t avp_recv_pkts(void *rx_queue,
			      struct rte_mbuf **rx_pkts,
			      uint16_t nb_pkts);

static uint16_t avp_xmit_scattered_pkts(void *tx_queue,
					struct rte_mbuf **tx_pkts,
					uint16_t nb_pkts);

static uint16_t avp_xmit_pkts(void *tx_queue,
			      struct rte_mbuf **tx_pkts,
			      uint16_t nb_pkts);

static void avp_dev_rx_queue_release(void *rxq);
static void avp_dev_tx_queue_release(void *txq);

static int avp_dev_stats_get(struct rte_eth_dev *dev,
			      struct rte_eth_stats *stats);
static int avp_dev_stats_reset(struct rte_eth_dev *dev);


#define AVP_MAX_RX_BURST 64
#define AVP_MAX_TX_BURST 64
#define AVP_MAX_MAC_ADDRS 1
#define AVP_MIN_RX_BUFSIZE RTE_ETHER_MIN_LEN


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
	.dev_start           = avp_dev_start,
	.dev_stop            = avp_dev_stop,
	.dev_close           = avp_dev_close,
	.dev_infos_get       = avp_dev_info_get,
	.vlan_offload_set    = avp_vlan_offload_set,
	.stats_get           = avp_dev_stats_get,
	.stats_reset         = avp_dev_stats_reset,
	.link_update         = avp_dev_link_update,
	.promiscuous_enable  = avp_dev_promiscuous_enable,
	.promiscuous_disable = avp_dev_promiscuous_disable,
	.rx_queue_setup      = avp_dev_rx_queue_setup,
	.rx_queue_release    = avp_dev_rx_queue_release,
	.tx_queue_setup      = avp_dev_tx_queue_setup,
	.tx_queue_release    = avp_dev_tx_queue_release,
};

/**@{ AVP device flags */
#define AVP_F_PROMISC (1 << 1)
#define AVP_F_CONFIGURED (1 << 2)
#define AVP_F_LINKUP (1 << 3)
#define AVP_F_DETACHED (1 << 4)
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
	struct rte_ether_addr ethaddr; /**< Host specified MAC address */
	struct rte_eth_dev_data *dev_data;
	/**< Back pointer to ethernet device data */
	volatile uint32_t flags; /**< Device operational flags */
	uint16_t port_id; /**< Ethernet port identifier */
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

	/* mutual exclusion over the 'flag' and 'resp_q/req_q' fields */
	rte_spinlock_t lock;

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

		if (retry == 0) {
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
avp_dev_ctrl_set_link_state(struct rte_eth_dev *eth_dev, unsigned int state)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_avp_request request;
	int ret;

	/* setup a link state change request */
	memset(&request, 0, sizeof(request));
	request.req_id = RTE_AVP_REQ_CFG_NETWORK_IF;
	request.if_up = state;

	ret = avp_dev_process_request(avp, &request);

	return ret == 0 ? request.result : ret;
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

static int
avp_dev_ctrl_shutdown(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_avp_request request;
	int ret;

	/* setup a shutdown request */
	memset(&request, 0, sizeof(request));
	request.req_id = RTE_AVP_REQ_SHUTDOWN_DEVICE;

	ret = avp_dev_process_request(avp, &request);

	return ret == 0 ? request.result : ret;
}

/* translate from host mbuf virtual address to guest virtual address */
static inline void *
avp_dev_translate_buffer(struct avp_dev *avp, void *host_mbuf_address)
{
	return RTE_PTR_ADD(RTE_PTR_SUB(host_mbuf_address,
				       (uintptr_t)avp->host_mbuf_addr),
			   (uintptr_t)avp->mbuf_addr);
}

/* translate from host physical address to guest virtual address */
static void *
avp_dev_translate_address(struct rte_eth_dev *eth_dev,
			  rte_iova_t host_phys_addr)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
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
			addr = RTE_PTR_ADD(addr, (uintptr_t)offset);

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
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
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

static int
avp_dev_detach(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	int ret;

	PMD_DRV_LOG(NOTICE, "Detaching port %u from AVP device 0x%" PRIx64 "\n",
		    eth_dev->data->port_id, avp->device_id);

	rte_spinlock_lock(&avp->lock);

	if (avp->flags & AVP_F_DETACHED) {
		PMD_DRV_LOG(NOTICE, "port %u already detached\n",
			    eth_dev->data->port_id);
		ret = 0;
		goto unlock;
	}

	/* shutdown the device first so the host stops sending us packets. */
	ret = avp_dev_ctrl_shutdown(eth_dev);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to send/recv shutdown to host, ret=%d\n",
			    ret);
		avp->flags &= ~AVP_F_DETACHED;
		goto unlock;
	}

	avp->flags |= AVP_F_DETACHED;
	rte_wmb();

	/* wait for queues to acknowledge the presence of the detach flag */
	rte_delay_ms(1);

	ret = 0;

unlock:
	rte_spinlock_unlock(&avp->lock);
	return ret;
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
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
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

static int
avp_dev_attach(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_avp_device_config config;
	unsigned int i;
	int ret;

	PMD_DRV_LOG(NOTICE, "Attaching port %u to AVP device 0x%" PRIx64 "\n",
		    eth_dev->data->port_id, avp->device_id);

	rte_spinlock_lock(&avp->lock);

	if (!(avp->flags & AVP_F_DETACHED)) {
		PMD_DRV_LOG(NOTICE, "port %u already attached\n",
			    eth_dev->data->port_id);
		ret = 0;
		goto unlock;
	}

	/*
	 * make sure that the detached flag is set prior to reconfiguring the
	 * queues.
	 */
	avp->flags |= AVP_F_DETACHED;
	rte_wmb();

	/*
	 * re-run the device create utility which will parse the new host info
	 * and setup the AVP device queue pointers.
	 */
	ret = avp_dev_create(RTE_ETH_DEV_TO_PCI(eth_dev), eth_dev);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to re-create AVP device, ret=%d\n",
			    ret);
		goto unlock;
	}

	if (avp->flags & AVP_F_CONFIGURED) {
		/*
		 * Update the receive queue mapping to handle cases where the
		 * source and destination hosts have different queue
		 * requirements.  As long as the DETACHED flag is asserted the
		 * queue table should not be referenced so it should be safe to
		 * update it.
		 */
		_avp_set_queue_counts(eth_dev);
		for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
			_avp_set_rx_queue_mappings(eth_dev, i);

		/*
		 * Update the host with our config details so that it knows the
		 * device is active.
		 */
		memset(&config, 0, sizeof(config));
		config.device_id = avp->device_id;
		config.driver_type = RTE_AVP_DRIVER_TYPE_DPDK;
		config.driver_version = AVP_DPDK_DRIVER_VERSION;
		config.features = avp->features;
		config.num_tx_queues = avp->num_tx_queues;
		config.num_rx_queues = avp->num_rx_queues;
		config.if_up = !!(avp->flags & AVP_F_LINKUP);

		ret = avp_dev_ctrl_set_config(eth_dev, &config);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Config request failed by host, ret=%d\n",
				    ret);
			goto unlock;
		}
	}

	rte_wmb();
	avp->flags &= ~AVP_F_DETACHED;

	ret = 0;

unlock:
	rte_spinlock_unlock(&avp->lock);
	return ret;
}

static void
avp_dev_interrupt_handler(void *data)
{
	struct rte_eth_dev *eth_dev = data;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	void *registers = pci_dev->mem_resource[RTE_AVP_PCI_MMIO_BAR].addr;
	uint32_t status, value;
	int ret;

	if (registers == NULL)
		rte_panic("no mapped MMIO register space\n");

	/* read the interrupt status register
	 * note: this register clears on read so all raised interrupts must be
	 *    handled or remembered for later processing
	 */
	status = AVP_READ32(
		RTE_PTR_ADD(registers,
			    RTE_AVP_INTERRUPT_STATUS_OFFSET));

	if (status & RTE_AVP_MIGRATION_INTERRUPT_MASK) {
		/* handle interrupt based on current status */
		value = AVP_READ32(
			RTE_PTR_ADD(registers,
				    RTE_AVP_MIGRATION_STATUS_OFFSET));
		switch (value) {
		case RTE_AVP_MIGRATION_DETACHED:
			ret = avp_dev_detach(eth_dev);
			break;
		case RTE_AVP_MIGRATION_ATTACHED:
			ret = avp_dev_attach(eth_dev);
			break;
		default:
			PMD_DRV_LOG(ERR, "unexpected migration status, status=%u\n",
				    value);
			ret = -EINVAL;
		}

		/* acknowledge the request by writing out our current status */
		value = (ret == 0 ? value : RTE_AVP_MIGRATION_ERROR);
		AVP_WRITE32(value,
			    RTE_PTR_ADD(registers,
					RTE_AVP_MIGRATION_ACK_OFFSET));

		PMD_DRV_LOG(NOTICE, "AVP migration interrupt handled\n");
	}

	if (status & ~RTE_AVP_MIGRATION_INTERRUPT_MASK)
		PMD_DRV_LOG(WARNING, "AVP unexpected interrupt, status=0x%08x\n",
			    status);

	/* re-enable UIO interrupt handling */
	ret = rte_intr_ack(&pci_dev->intr_handle);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to re-enable UIO interrupts, ret=%d\n",
			    ret);
		/* continue */
	}
}

static int
avp_dev_enable_interrupts(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	void *registers = pci_dev->mem_resource[RTE_AVP_PCI_MMIO_BAR].addr;
	int ret;

	if (registers == NULL)
		return -EINVAL;

	/* enable UIO interrupt handling */
	ret = rte_intr_enable(&pci_dev->intr_handle);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to enable UIO interrupts, ret=%d\n",
			    ret);
		return ret;
	}

	/* inform the device that all interrupts are enabled */
	AVP_WRITE32(RTE_AVP_APP_INTERRUPTS_MASK,
		    RTE_PTR_ADD(registers, RTE_AVP_INTERRUPT_MASK_OFFSET));

	return 0;
}

static int
avp_dev_disable_interrupts(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	void *registers = pci_dev->mem_resource[RTE_AVP_PCI_MMIO_BAR].addr;
	int ret;

	if (registers == NULL)
		return 0;

	/* inform the device that all interrupts are disabled */
	AVP_WRITE32(RTE_AVP_NO_INTERRUPTS_MASK,
		    RTE_PTR_ADD(registers, RTE_AVP_INTERRUPT_MASK_OFFSET));

	/* enable UIO interrupt handling */
	ret = rte_intr_disable(&pci_dev->intr_handle);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to disable UIO interrupts, ret=%d\n",
			    ret);
		return ret;
	}

	return 0;
}

static int
avp_dev_setup_interrupts(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int ret;

	/* register a callback handler with UIO for interrupt notifications */
	ret = rte_intr_callback_register(&pci_dev->intr_handle,
					 avp_dev_interrupt_handler,
					 (void *)eth_dev);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to register UIO interrupt callback, ret=%d\n",
			    ret);
		return ret;
	}

	/* enable interrupt processing */
	return avp_dev_enable_interrupts(eth_dev);
}

static int
avp_dev_migration_pending(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	void *registers = pci_dev->mem_resource[RTE_AVP_PCI_MMIO_BAR].addr;
	uint32_t value;

	if (registers == NULL)
		return 0;

	value = AVP_READ32(RTE_PTR_ADD(registers,
				       RTE_AVP_MIGRATION_STATUS_OFFSET));
	if (value == RTE_AVP_MIGRATION_DETACHED) {
		/* migration is in progress; ack it if we have not already */
		AVP_WRITE32(value,
			    RTE_PTR_ADD(registers,
					RTE_AVP_MIGRATION_ACK_OFFSET));
		return 1;
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
		rte_spinlock_init(&avp->lock);
		memcpy(&avp->ethaddr.addr_bytes[0],
		       host_info->ethaddr, RTE_ETHER_ADDR_LEN);
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

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	eth_dev->dev_ops = &avp_eth_dev_ops;
	eth_dev->rx_pkt_burst = &avp_recv_pkts;
	eth_dev->tx_pkt_burst = &avp_xmit_pkts;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		/*
		 * no setup required on secondary processes.  All data is saved
		 * in dev_private by the primary process. All resource should
		 * be mapped to the same virtual address so all pointers should
		 * be valid.
		 */
		if (eth_dev->data->scattered_rx) {
			PMD_DRV_LOG(NOTICE, "AVP device configured for chained mbufs\n");
			eth_dev->rx_pkt_burst = avp_recv_scattered_pkts;
			eth_dev->tx_pkt_burst = avp_xmit_scattered_pkts;
		}
		return 0;
	}

	rte_eth_copy_pci_info(eth_dev, pci_dev);
	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	/* Check current migration status */
	if (avp_dev_migration_pending(eth_dev)) {
		PMD_DRV_LOG(ERR, "VM live migration operation in progress\n");
		return -EBUSY;
	}

	/* Check BAR resources */
	ret = avp_dev_check_regions(eth_dev);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to validate BAR resources, ret=%d\n",
			    ret);
		return ret;
	}

	/* Enable interrupts */
	ret = avp_dev_setup_interrupts(eth_dev);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to enable interrupts, ret=%d\n", ret);
		return ret;
	}

	/* Handle each subtype */
	ret = avp_dev_create(pci_dev, eth_dev);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to create device, ret=%d\n", ret);
		return ret;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("avp_ethdev",
					RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate %d bytes needed to store MAC addresses\n",
			    RTE_ETHER_ADDR_LEN);
		return -ENOMEM;
	}

	/* Get a mac from device config */
	rte_ether_addr_copy(&avp->ethaddr, &eth_dev->data->mac_addrs[0]);

	return 0;
}

static int
eth_avp_dev_uninit(struct rte_eth_dev *eth_dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	if (eth_dev->data == NULL)
		return 0;

	avp_dev_close(eth_dev);

	return 0;
}

static int
eth_avp_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		  struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct avp_adapter),
			eth_avp_dev_init);
}

static int
eth_avp_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev,
					      eth_avp_dev_uninit);
}

static struct rte_pci_driver rte_avp_pmd = {
	.id_table = pci_id_avp_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_avp_pci_probe,
	.remove = eth_avp_pci_remove,
};

static int
avp_dev_enable_scattered(struct rte_eth_dev *eth_dev,
			 struct avp_dev *avp)
{
	unsigned int max_rx_pkt_len;

	max_rx_pkt_len = eth_dev->data->dev_conf.rxmode.max_rx_pkt_len;

	if ((max_rx_pkt_len > avp->guest_mbuf_size) ||
	    (max_rx_pkt_len > avp->host_mbuf_size)) {
		/*
		 * If the guest MTU is greater than either the host or guest
		 * buffers then chained mbufs have to be enabled in the TX
		 * direction.  It is assumed that the application will not need
		 * to send packets larger than their max_rx_pkt_len (MRU).
		 */
		return 1;
	}

	if ((avp->max_rx_pkt_len > avp->guest_mbuf_size) ||
	    (avp->max_rx_pkt_len > avp->host_mbuf_size)) {
		/*
		 * If the host MRU is greater than its own mbuf size or the
		 * guest mbuf size then chained mbufs have to be enabled in the
		 * RX direction.
		 */
		return 1;
	}

	return 0;
}

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

	if (avp_dev_enable_scattered(eth_dev, avp)) {
		if (!eth_dev->data->scattered_rx) {
			PMD_DRV_LOG(NOTICE, "AVP device configured for chained mbufs\n");
			eth_dev->data->scattered_rx = 1;
			eth_dev->rx_pkt_burst = avp_recv_scattered_pkts;
			eth_dev->tx_pkt_burst = avp_xmit_scattered_pkts;
		}
	}

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

static inline int
_avp_cmp_ether_addr(struct rte_ether_addr *a, struct rte_ether_addr *b)
{
	uint16_t *_a = (uint16_t *)&a->addr_bytes[0];
	uint16_t *_b = (uint16_t *)&b->addr_bytes[0];
	return (_a[0] ^ _b[0]) | (_a[1] ^ _b[1]) | (_a[2] ^ _b[2]);
}

static inline int
_avp_mac_filter(struct avp_dev *avp, struct rte_mbuf *m)
{
	struct rte_ether_hdr *eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	if (likely(_avp_cmp_ether_addr(&avp->ethaddr, &eth->d_addr) == 0)) {
		/* allow all packets destined to our address */
		return 0;
	}

	if (likely(rte_is_broadcast_ether_addr(&eth->d_addr))) {
		/* allow all broadcast packets */
		return 0;
	}

	if (likely(rte_is_multicast_ether_addr(&eth->d_addr))) {
		/* allow all multicast packets */
		return 0;
	}

	if (avp->flags & AVP_F_PROMISC) {
		/* allow all packets when in promiscuous mode */
		return 0;
	}

	return -1;
}

#ifdef RTE_LIBRTE_AVP_DEBUG_BUFFERS
static inline void
__avp_dev_buffer_sanity_check(struct avp_dev *avp, struct rte_avp_desc *buf)
{
	struct rte_avp_desc *first_buf;
	struct rte_avp_desc *pkt_buf;
	unsigned int pkt_len;
	unsigned int nb_segs;
	void *pkt_data;
	unsigned int i;

	first_buf = avp_dev_translate_buffer(avp, buf);

	i = 0;
	pkt_len = 0;
	nb_segs = first_buf->nb_segs;
	do {
		/* Adjust pointers for guest addressing */
		pkt_buf = avp_dev_translate_buffer(avp, buf);
		if (pkt_buf == NULL)
			rte_panic("bad buffer: segment %u has an invalid address %p\n",
				  i, buf);
		pkt_data = avp_dev_translate_buffer(avp, pkt_buf->data);
		if (pkt_data == NULL)
			rte_panic("bad buffer: segment %u has a NULL data pointer\n",
				  i);
		if (pkt_buf->data_len == 0)
			rte_panic("bad buffer: segment %u has 0 data length\n",
				  i);
		pkt_len += pkt_buf->data_len;
		nb_segs--;
		i++;

	} while (nb_segs && (buf = pkt_buf->next) != NULL);

	if (nb_segs != 0)
		rte_panic("bad buffer: expected %u segments found %u\n",
			  first_buf->nb_segs, (first_buf->nb_segs - nb_segs));
	if (pkt_len != first_buf->pkt_len)
		rte_panic("bad buffer: expected length %u found %u\n",
			  first_buf->pkt_len, pkt_len);
}

#define avp_dev_buffer_sanity_check(a, b) \
	__avp_dev_buffer_sanity_check((a), (b))

#else /* RTE_LIBRTE_AVP_DEBUG_BUFFERS */

#define avp_dev_buffer_sanity_check(a, b) do {} while (0)

#endif

/*
 * Copy a host buffer chain to a set of mbufs.	This function assumes that
 * there exactly the required number of mbufs to copy all source bytes.
 */
static inline struct rte_mbuf *
avp_dev_copy_from_buffers(struct avp_dev *avp,
			  struct rte_avp_desc *buf,
			  struct rte_mbuf **mbufs,
			  unsigned int count)
{
	struct rte_mbuf *m_previous = NULL;
	struct rte_avp_desc *pkt_buf;
	unsigned int total_length = 0;
	unsigned int copy_length;
	unsigned int src_offset;
	struct rte_mbuf *m;
	uint16_t ol_flags;
	uint16_t vlan_tci;
	void *pkt_data;
	unsigned int i;

	avp_dev_buffer_sanity_check(avp, buf);

	/* setup the first source buffer */
	pkt_buf = avp_dev_translate_buffer(avp, buf);
	pkt_data = avp_dev_translate_buffer(avp, pkt_buf->data);
	total_length = pkt_buf->pkt_len;
	src_offset = 0;

	if (pkt_buf->ol_flags & RTE_AVP_RX_VLAN_PKT) {
		ol_flags = PKT_RX_VLAN;
		vlan_tci = pkt_buf->vlan_tci;
	} else {
		ol_flags = 0;
		vlan_tci = 0;
	}

	for (i = 0; (i < count) && (buf != NULL); i++) {
		/* fill each destination buffer */
		m = mbufs[i];

		if (m_previous != NULL)
			m_previous->next = m;

		m_previous = m;

		do {
			/*
			 * Copy as many source buffers as will fit in the
			 * destination buffer.
			 */
			copy_length = RTE_MIN((avp->guest_mbuf_size -
					       rte_pktmbuf_data_len(m)),
					      (pkt_buf->data_len -
					       src_offset));
			rte_memcpy(RTE_PTR_ADD(rte_pktmbuf_mtod(m, void *),
					       rte_pktmbuf_data_len(m)),
				   RTE_PTR_ADD(pkt_data, src_offset),
				   copy_length);
			rte_pktmbuf_data_len(m) += copy_length;
			src_offset += copy_length;

			if (likely(src_offset == pkt_buf->data_len)) {
				/* need a new source buffer */
				buf = pkt_buf->next;
				if (buf != NULL) {
					pkt_buf = avp_dev_translate_buffer(
						avp, buf);
					pkt_data = avp_dev_translate_buffer(
						avp, pkt_buf->data);
					src_offset = 0;
				}
			}

			if (unlikely(rte_pktmbuf_data_len(m) ==
				     avp->guest_mbuf_size)) {
				/* need a new destination mbuf */
				break;
			}

		} while (buf != NULL);
	}

	m = mbufs[0];
	m->ol_flags = ol_flags;
	m->nb_segs = count;
	rte_pktmbuf_pkt_len(m) = total_length;
	m->vlan_tci = vlan_tci;

	__rte_mbuf_sanity_check(m, 1);

	return m;
}

static uint16_t
avp_recv_scattered_pkts(void *rx_queue,
			struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	struct avp_queue *rxq = (struct avp_queue *)rx_queue;
	struct rte_avp_desc *avp_bufs[AVP_MAX_RX_BURST];
	struct rte_mbuf *mbufs[RTE_AVP_MAX_MBUF_SEGMENTS];
	struct avp_dev *avp = rxq->avp;
	struct rte_avp_desc *pkt_buf;
	struct rte_avp_fifo *free_q;
	struct rte_avp_fifo *rx_q;
	struct rte_avp_desc *buf;
	unsigned int count, avail, n;
	unsigned int guest_mbuf_size;
	struct rte_mbuf *m;
	unsigned int required;
	unsigned int buf_len;
	unsigned int port_id;
	unsigned int i;

	if (unlikely(avp->flags & AVP_F_DETACHED)) {
		/* VM live migration in progress */
		return 0;
	}

	guest_mbuf_size = avp->guest_mbuf_size;
	port_id = avp->port_id;
	rx_q = avp->rx_q[rxq->queue_id];
	free_q = avp->free_q[rxq->queue_id];

	/* setup next queue to service */
	rxq->queue_id = (rxq->queue_id < rxq->queue_limit) ?
		(rxq->queue_id + 1) : rxq->queue_base;

	/* determine how many slots are available in the free queue */
	count = avp_fifo_free_count(free_q);

	/* determine how many packets are available in the rx queue */
	avail = avp_fifo_count(rx_q);

	/* determine how many packets can be received */
	count = RTE_MIN(count, avail);
	count = RTE_MIN(count, nb_pkts);
	count = RTE_MIN(count, (unsigned int)AVP_MAX_RX_BURST);

	if (unlikely(count == 0)) {
		/* no free buffers, or no buffers on the rx queue */
		return 0;
	}

	/* retrieve pending packets */
	n = avp_fifo_get(rx_q, (void **)&avp_bufs, count);
	PMD_RX_LOG(DEBUG, "Receiving %u packets from Rx queue at %p\n",
		   count, rx_q);

	count = 0;
	for (i = 0; i < n; i++) {
		/* prefetch next entry while processing current one */
		if (i + 1 < n) {
			pkt_buf = avp_dev_translate_buffer(avp,
							   avp_bufs[i + 1]);
			rte_prefetch0(pkt_buf);
		}
		buf = avp_bufs[i];

		/* Peek into the first buffer to determine the total length */
		pkt_buf = avp_dev_translate_buffer(avp, buf);
		buf_len = pkt_buf->pkt_len;

		/* Allocate enough mbufs to receive the entire packet */
		required = (buf_len + guest_mbuf_size - 1) / guest_mbuf_size;
		if (rte_pktmbuf_alloc_bulk(avp->pool, mbufs, required)) {
			rxq->dev_data->rx_mbuf_alloc_failed++;
			continue;
		}

		/* Copy the data from the buffers to our mbufs */
		m = avp_dev_copy_from_buffers(avp, buf, mbufs, required);

		/* finalize mbuf */
		m->port = port_id;

		if (_avp_mac_filter(avp, m) != 0) {
			/* silently discard packets not destined to our MAC */
			rte_pktmbuf_free(m);
			continue;
		}

		/* return new mbuf to caller */
		rx_pkts[count++] = m;
		rxq->bytes += buf_len;
	}

	rxq->packets += count;

	/* return the buffers to the free queue */
	avp_fifo_put(free_q, (void **)&avp_bufs[0], n);

	return count;
}


static uint16_t
avp_recv_pkts(void *rx_queue,
	      struct rte_mbuf **rx_pkts,
	      uint16_t nb_pkts)
{
	struct avp_queue *rxq = (struct avp_queue *)rx_queue;
	struct rte_avp_desc *avp_bufs[AVP_MAX_RX_BURST];
	struct avp_dev *avp = rxq->avp;
	struct rte_avp_desc *pkt_buf;
	struct rte_avp_fifo *free_q;
	struct rte_avp_fifo *rx_q;
	unsigned int count, avail, n;
	unsigned int pkt_len;
	struct rte_mbuf *m;
	char *pkt_data;
	unsigned int i;

	if (unlikely(avp->flags & AVP_F_DETACHED)) {
		/* VM live migration in progress */
		return 0;
	}

	rx_q = avp->rx_q[rxq->queue_id];
	free_q = avp->free_q[rxq->queue_id];

	/* setup next queue to service */
	rxq->queue_id = (rxq->queue_id < rxq->queue_limit) ?
		(rxq->queue_id + 1) : rxq->queue_base;

	/* determine how many slots are available in the free queue */
	count = avp_fifo_free_count(free_q);

	/* determine how many packets are available in the rx queue */
	avail = avp_fifo_count(rx_q);

	/* determine how many packets can be received */
	count = RTE_MIN(count, avail);
	count = RTE_MIN(count, nb_pkts);
	count = RTE_MIN(count, (unsigned int)AVP_MAX_RX_BURST);

	if (unlikely(count == 0)) {
		/* no free buffers, or no buffers on the rx queue */
		return 0;
	}

	/* retrieve pending packets */
	n = avp_fifo_get(rx_q, (void **)&avp_bufs, count);
	PMD_RX_LOG(DEBUG, "Receiving %u packets from Rx queue at %p\n",
		   count, rx_q);

	count = 0;
	for (i = 0; i < n; i++) {
		/* prefetch next entry while processing current one */
		if (i < n - 1) {
			pkt_buf = avp_dev_translate_buffer(avp,
							   avp_bufs[i + 1]);
			rte_prefetch0(pkt_buf);
		}

		/* Adjust host pointers for guest addressing */
		pkt_buf = avp_dev_translate_buffer(avp, avp_bufs[i]);
		pkt_data = avp_dev_translate_buffer(avp, pkt_buf->data);
		pkt_len = pkt_buf->pkt_len;

		if (unlikely((pkt_len > avp->guest_mbuf_size) ||
			     (pkt_buf->nb_segs > 1))) {
			/*
			 * application should be using the scattered receive
			 * function
			 */
			rxq->errors++;
			continue;
		}

		/* process each packet to be transmitted */
		m = rte_pktmbuf_alloc(avp->pool);
		if (unlikely(m == NULL)) {
			rxq->dev_data->rx_mbuf_alloc_failed++;
			continue;
		}

		/* copy data out of the host buffer to our buffer */
		m->data_off = RTE_PKTMBUF_HEADROOM;
		rte_memcpy(rte_pktmbuf_mtod(m, void *), pkt_data, pkt_len);

		/* initialize the local mbuf */
		rte_pktmbuf_data_len(m) = pkt_len;
		rte_pktmbuf_pkt_len(m) = pkt_len;
		m->port = avp->port_id;

		if (pkt_buf->ol_flags & RTE_AVP_RX_VLAN_PKT) {
			m->ol_flags = PKT_RX_VLAN;
			m->vlan_tci = pkt_buf->vlan_tci;
		}

		if (_avp_mac_filter(avp, m) != 0) {
			/* silently discard packets not destined to our MAC */
			rte_pktmbuf_free(m);
			continue;
		}

		/* return new mbuf to caller */
		rx_pkts[count++] = m;
		rxq->bytes += pkt_len;
	}

	rxq->packets += count;

	/* return the buffers to the free queue */
	avp_fifo_put(free_q, (void **)&avp_bufs[0], n);

	return count;
}

/*
 * Copy a chained mbuf to a set of host buffers.  This function assumes that
 * there are sufficient destination buffers to contain the entire source
 * packet.
 */
static inline uint16_t
avp_dev_copy_to_buffers(struct avp_dev *avp,
			struct rte_mbuf *mbuf,
			struct rte_avp_desc **buffers,
			unsigned int count)
{
	struct rte_avp_desc *previous_buf = NULL;
	struct rte_avp_desc *first_buf = NULL;
	struct rte_avp_desc *pkt_buf;
	struct rte_avp_desc *buf;
	size_t total_length;
	struct rte_mbuf *m;
	size_t copy_length;
	size_t src_offset;
	char *pkt_data;
	unsigned int i;

	__rte_mbuf_sanity_check(mbuf, 1);

	m = mbuf;
	src_offset = 0;
	total_length = rte_pktmbuf_pkt_len(m);
	for (i = 0; (i < count) && (m != NULL); i++) {
		/* fill each destination buffer */
		buf = buffers[i];

		if (i < count - 1) {
			/* prefetch next entry while processing this one */
			pkt_buf = avp_dev_translate_buffer(avp, buffers[i + 1]);
			rte_prefetch0(pkt_buf);
		}

		/* Adjust pointers for guest addressing */
		pkt_buf = avp_dev_translate_buffer(avp, buf);
		pkt_data = avp_dev_translate_buffer(avp, pkt_buf->data);

		/* setup the buffer chain */
		if (previous_buf != NULL)
			previous_buf->next = buf;
		else
			first_buf = pkt_buf;

		previous_buf = pkt_buf;

		do {
			/*
			 * copy as many source mbuf segments as will fit in the
			 * destination buffer.
			 */
			copy_length = RTE_MIN((avp->host_mbuf_size -
					       pkt_buf->data_len),
					      (rte_pktmbuf_data_len(m) -
					       src_offset));
			rte_memcpy(RTE_PTR_ADD(pkt_data, pkt_buf->data_len),
				   RTE_PTR_ADD(rte_pktmbuf_mtod(m, void *),
					       src_offset),
				   copy_length);
			pkt_buf->data_len += copy_length;
			src_offset += copy_length;

			if (likely(src_offset == rte_pktmbuf_data_len(m))) {
				/* need a new source buffer */
				m = m->next;
				src_offset = 0;
			}

			if (unlikely(pkt_buf->data_len ==
				     avp->host_mbuf_size)) {
				/* need a new destination buffer */
				break;
			}

		} while (m != NULL);
	}

	first_buf->nb_segs = count;
	first_buf->pkt_len = total_length;

	if (mbuf->ol_flags & PKT_TX_VLAN_PKT) {
		first_buf->ol_flags |= RTE_AVP_TX_VLAN_PKT;
		first_buf->vlan_tci = mbuf->vlan_tci;
	}

	avp_dev_buffer_sanity_check(avp, buffers[0]);

	return total_length;
}


static uint16_t
avp_xmit_scattered_pkts(void *tx_queue,
			struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts)
{
	struct rte_avp_desc *avp_bufs[(AVP_MAX_TX_BURST *
				       RTE_AVP_MAX_MBUF_SEGMENTS)] = {};
	struct avp_queue *txq = (struct avp_queue *)tx_queue;
	struct rte_avp_desc *tx_bufs[AVP_MAX_TX_BURST];
	struct avp_dev *avp = txq->avp;
	struct rte_avp_fifo *alloc_q;
	struct rte_avp_fifo *tx_q;
	unsigned int count, avail, n;
	unsigned int orig_nb_pkts;
	struct rte_mbuf *m;
	unsigned int required;
	unsigned int segments;
	unsigned int tx_bytes;
	unsigned int i;

	orig_nb_pkts = nb_pkts;
	if (unlikely(avp->flags & AVP_F_DETACHED)) {
		/* VM live migration in progress */
		/* TODO ... buffer for X packets then drop? */
		txq->errors += nb_pkts;
		return 0;
	}

	tx_q = avp->tx_q[txq->queue_id];
	alloc_q = avp->alloc_q[txq->queue_id];

	/* limit the number of transmitted packets to the max burst size */
	if (unlikely(nb_pkts > AVP_MAX_TX_BURST))
		nb_pkts = AVP_MAX_TX_BURST;

	/* determine how many buffers are available to copy into */
	avail = avp_fifo_count(alloc_q);
	if (unlikely(avail > (AVP_MAX_TX_BURST *
			      RTE_AVP_MAX_MBUF_SEGMENTS)))
		avail = AVP_MAX_TX_BURST * RTE_AVP_MAX_MBUF_SEGMENTS;

	/* determine how many slots are available in the transmit queue */
	count = avp_fifo_free_count(tx_q);

	/* determine how many packets can be sent */
	nb_pkts = RTE_MIN(count, nb_pkts);

	/* determine how many packets will fit in the available buffers */
	count = 0;
	segments = 0;
	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];
		if (likely(i < (unsigned int)nb_pkts - 1)) {
			/* prefetch next entry while processing this one */
			rte_prefetch0(tx_pkts[i + 1]);
		}
		required = (rte_pktmbuf_pkt_len(m) + avp->host_mbuf_size - 1) /
			avp->host_mbuf_size;

		if (unlikely((required == 0) ||
			     (required > RTE_AVP_MAX_MBUF_SEGMENTS)))
			break;
		else if (unlikely(required + segments > avail))
			break;
		segments += required;
		count++;
	}
	nb_pkts = count;

	if (unlikely(nb_pkts == 0)) {
		/* no available buffers, or no space on the tx queue */
		txq->errors += orig_nb_pkts;
		return 0;
	}

	PMD_TX_LOG(DEBUG, "Sending %u packets on Tx queue at %p\n",
		   nb_pkts, tx_q);

	/* retrieve sufficient send buffers */
	n = avp_fifo_get(alloc_q, (void **)&avp_bufs, segments);
	if (unlikely(n != segments)) {
		PMD_TX_LOG(DEBUG, "Failed to allocate buffers "
			   "n=%u, segments=%u, orig=%u\n",
			   n, segments, orig_nb_pkts);
		txq->errors += orig_nb_pkts;
		return 0;
	}

	tx_bytes = 0;
	count = 0;
	for (i = 0; i < nb_pkts; i++) {
		/* process each packet to be transmitted */
		m = tx_pkts[i];

		/* determine how many buffers are required for this packet */
		required = (rte_pktmbuf_pkt_len(m) + avp->host_mbuf_size - 1) /
			avp->host_mbuf_size;

		tx_bytes += avp_dev_copy_to_buffers(avp, m,
						    &avp_bufs[count], required);
		tx_bufs[i] = avp_bufs[count];
		count += required;

		/* free the original mbuf */
		rte_pktmbuf_free(m);
	}

	txq->packets += nb_pkts;
	txq->bytes += tx_bytes;

#ifdef RTE_LIBRTE_AVP_DEBUG_BUFFERS
	for (i = 0; i < nb_pkts; i++)
		avp_dev_buffer_sanity_check(avp, tx_bufs[i]);
#endif

	/* send the packets */
	n = avp_fifo_put(tx_q, (void **)&tx_bufs[0], nb_pkts);
	if (unlikely(n != orig_nb_pkts))
		txq->errors += (orig_nb_pkts - n);

	return n;
}


static uint16_t
avp_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct avp_queue *txq = (struct avp_queue *)tx_queue;
	struct rte_avp_desc *avp_bufs[AVP_MAX_TX_BURST];
	struct avp_dev *avp = txq->avp;
	struct rte_avp_desc *pkt_buf;
	struct rte_avp_fifo *alloc_q;
	struct rte_avp_fifo *tx_q;
	unsigned int count, avail, n;
	struct rte_mbuf *m;
	unsigned int pkt_len;
	unsigned int tx_bytes;
	char *pkt_data;
	unsigned int i;

	if (unlikely(avp->flags & AVP_F_DETACHED)) {
		/* VM live migration in progress */
		/* TODO ... buffer for X packets then drop?! */
		txq->errors++;
		return 0;
	}

	tx_q = avp->tx_q[txq->queue_id];
	alloc_q = avp->alloc_q[txq->queue_id];

	/* limit the number of transmitted packets to the max burst size */
	if (unlikely(nb_pkts > AVP_MAX_TX_BURST))
		nb_pkts = AVP_MAX_TX_BURST;

	/* determine how many buffers are available to copy into */
	avail = avp_fifo_count(alloc_q);

	/* determine how many slots are available in the transmit queue */
	count = avp_fifo_free_count(tx_q);

	/* determine how many packets can be sent */
	count = RTE_MIN(count, avail);
	count = RTE_MIN(count, nb_pkts);

	if (unlikely(count == 0)) {
		/* no available buffers, or no space on the tx queue */
		txq->errors += nb_pkts;
		return 0;
	}

	PMD_TX_LOG(DEBUG, "Sending %u packets on Tx queue at %p\n",
		   count, tx_q);

	/* retrieve sufficient send buffers */
	n = avp_fifo_get(alloc_q, (void **)&avp_bufs, count);
	if (unlikely(n != count)) {
		txq->errors++;
		return 0;
	}

	tx_bytes = 0;
	for (i = 0; i < count; i++) {
		/* prefetch next entry while processing the current one */
		if (i < count - 1) {
			pkt_buf = avp_dev_translate_buffer(avp,
							   avp_bufs[i + 1]);
			rte_prefetch0(pkt_buf);
		}

		/* process each packet to be transmitted */
		m = tx_pkts[i];

		/* Adjust pointers for guest addressing */
		pkt_buf = avp_dev_translate_buffer(avp, avp_bufs[i]);
		pkt_data = avp_dev_translate_buffer(avp, pkt_buf->data);
		pkt_len = rte_pktmbuf_pkt_len(m);

		if (unlikely((pkt_len > avp->guest_mbuf_size) ||
					 (pkt_len > avp->host_mbuf_size))) {
			/*
			 * application should be using the scattered transmit
			 * function; send it truncated to avoid the performance
			 * hit of having to manage returning the already
			 * allocated buffer to the free list.  This should not
			 * happen since the application should have set the
			 * max_rx_pkt_len based on its MTU and it should be
			 * policing its own packet sizes.
			 */
			txq->errors++;
			pkt_len = RTE_MIN(avp->guest_mbuf_size,
					  avp->host_mbuf_size);
		}

		/* copy data out of our mbuf and into the AVP buffer */
		rte_memcpy(pkt_data, rte_pktmbuf_mtod(m, void *), pkt_len);
		pkt_buf->pkt_len = pkt_len;
		pkt_buf->data_len = pkt_len;
		pkt_buf->nb_segs = 1;
		pkt_buf->next = NULL;

		if (m->ol_flags & PKT_TX_VLAN_PKT) {
			pkt_buf->ol_flags |= RTE_AVP_TX_VLAN_PKT;
			pkt_buf->vlan_tci = m->vlan_tci;
		}

		tx_bytes += pkt_len;

		/* free the original mbuf */
		rte_pktmbuf_free(m);
	}

	txq->packets += count;
	txq->bytes += tx_bytes;

	/* send the packets */
	n = avp_fifo_put(tx_q, (void **)&avp_bufs[0], count);

	return n;
}

static void
avp_dev_rx_queue_release(void *rx_queue)
{
	struct avp_queue *rxq = (struct avp_queue *)rx_queue;
	struct avp_dev *avp = rxq->avp;
	struct rte_eth_dev_data *data = avp->dev_data;
	unsigned int i;

	for (i = 0; i < avp->num_rx_queues; i++) {
		if (data->rx_queues[i] == rxq) {
			rte_free(data->rx_queues[i]);
			data->rx_queues[i] = NULL;
		}
	}
}

static void
avp_dev_rx_queue_release_all(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_eth_dev_data *data = avp->dev_data;
	unsigned int i;

	for (i = 0; i < avp->num_rx_queues; i++) {
		if (data->rx_queues[i]) {
			rte_free(data->rx_queues[i]);
			data->rx_queues[i] = NULL;
		}
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
		if (data->tx_queues[i] == txq) {
			rte_free(data->tx_queues[i]);
			data->tx_queues[i] = NULL;
		}
	}
}

static void
avp_dev_tx_queue_release_all(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_eth_dev_data *data = avp->dev_data;
	unsigned int i;

	for (i = 0; i < avp->num_tx_queues; i++) {
		if (data->tx_queues[i]) {
			rte_free(data->tx_queues[i]);
			data->tx_queues[i] = NULL;
		}
	}
}

static int
avp_dev_configure(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_avp_device_info *host_info;
	struct rte_avp_device_config config;
	int mask = 0;
	void *addr;
	int ret;

	rte_spinlock_lock(&avp->lock);
	if (avp->flags & AVP_F_DETACHED) {
		PMD_DRV_LOG(ERR, "Operation not supported during VM live migration\n");
		ret = -ENOTSUP;
		goto unlock;
	}

	addr = pci_dev->mem_resource[RTE_AVP_PCI_DEVICE_BAR].addr;
	host_info = (struct rte_avp_device_info *)addr;

	/* Setup required number of queues */
	_avp_set_queue_counts(eth_dev);

	mask = (ETH_VLAN_STRIP_MASK |
		ETH_VLAN_FILTER_MASK |
		ETH_VLAN_EXTEND_MASK);
	ret = avp_vlan_offload_set(eth_dev, mask);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "VLAN offload set failed by host, ret=%d\n",
			    ret);
		goto unlock;
	}

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
	rte_spinlock_unlock(&avp->lock);
	return ret;
}

static int
avp_dev_start(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&avp->lock);
	if (avp->flags & AVP_F_DETACHED) {
		PMD_DRV_LOG(ERR, "Operation not supported during VM live migration\n");
		ret = -ENOTSUP;
		goto unlock;
	}

	/* update link state */
	ret = avp_dev_ctrl_set_link_state(eth_dev, 1);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Link state change failed by host, ret=%d\n",
			    ret);
		goto unlock;
	}

	/* remember current link state */
	avp->flags |= AVP_F_LINKUP;

	ret = 0;

unlock:
	rte_spinlock_unlock(&avp->lock);
	return ret;
}

static int
avp_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	int ret;

	rte_spinlock_lock(&avp->lock);
	if (avp->flags & AVP_F_DETACHED) {
		PMD_DRV_LOG(ERR, "Operation not supported during VM live migration\n");
		ret = -ENOTSUP;
		goto unlock;
	}

	/* remember current link state */
	avp->flags &= ~AVP_F_LINKUP;

	/* update link state */
	ret = avp_dev_ctrl_set_link_state(eth_dev, 0);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Link state change failed by host, ret=%d\n",
			    ret);
	}

unlock:
	rte_spinlock_unlock(&avp->lock);
	return ret;
}

static int
avp_dev_close(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rte_spinlock_lock(&avp->lock);
	if (avp->flags & AVP_F_DETACHED) {
		PMD_DRV_LOG(ERR, "Operation not supported during VM live migration\n");
		goto unlock;
	}

	/* remember current link state */
	avp->flags &= ~AVP_F_LINKUP;
	avp->flags &= ~AVP_F_CONFIGURED;

	ret = avp_dev_disable_interrupts(eth_dev);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to disable interrupts\n");
		/* continue */
	}

	/* update device state */
	ret = avp_dev_ctrl_shutdown(eth_dev);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Device shutdown failed by host, ret=%d\n",
			    ret);
		/* continue */
	}

	/* release dynamic storage for rx/tx queues */
	avp_dev_rx_queue_release_all(eth_dev);
	avp_dev_tx_queue_release_all(eth_dev);

unlock:
	rte_spinlock_unlock(&avp->lock);
	return 0;
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

static int
avp_dev_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	rte_spinlock_lock(&avp->lock);
	if ((avp->flags & AVP_F_PROMISC) == 0) {
		avp->flags |= AVP_F_PROMISC;
		PMD_DRV_LOG(DEBUG, "Promiscuous mode enabled on %u\n",
			    eth_dev->data->port_id);
	}
	rte_spinlock_unlock(&avp->lock);

	return 0;
}

static int
avp_dev_promiscuous_disable(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	rte_spinlock_lock(&avp->lock);
	if ((avp->flags & AVP_F_PROMISC) != 0) {
		avp->flags &= ~AVP_F_PROMISC;
		PMD_DRV_LOG(DEBUG, "Promiscuous mode disabled on %u\n",
			    eth_dev->data->port_id);
	}
	rte_spinlock_unlock(&avp->lock);

	return 0;
}

static int
avp_dev_info_get(struct rte_eth_dev *eth_dev,
		 struct rte_eth_dev_info *dev_info)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	dev_info->max_rx_queues = avp->max_rx_queues;
	dev_info->max_tx_queues = avp->max_tx_queues;
	dev_info->min_rx_bufsize = AVP_MIN_RX_BUFSIZE;
	dev_info->max_rx_pktlen = avp->max_rx_pkt_len;
	dev_info->max_mac_addrs = AVP_MAX_MAC_ADDRS;
	if (avp->host_features & RTE_AVP_FEATURE_VLAN_OFFLOAD) {
		dev_info->rx_offload_capa = DEV_RX_OFFLOAD_VLAN_STRIP;
		dev_info->tx_offload_capa = DEV_TX_OFFLOAD_VLAN_INSERT;
	}

	return 0;
}

static int
avp_vlan_offload_set(struct rte_eth_dev *eth_dev, int mask)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct rte_eth_conf *dev_conf = &eth_dev->data->dev_conf;
	uint64_t offloads = dev_conf->rxmode.offloads;

	if (mask & ETH_VLAN_STRIP_MASK) {
		if (avp->host_features & RTE_AVP_FEATURE_VLAN_OFFLOAD) {
			if (offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
				avp->features |= RTE_AVP_FEATURE_VLAN_OFFLOAD;
			else
				avp->features &= ~RTE_AVP_FEATURE_VLAN_OFFLOAD;
		} else {
			PMD_DRV_LOG(ERR, "VLAN strip offload not supported\n");
		}
	}

	if (mask & ETH_VLAN_FILTER_MASK) {
		if (offloads & DEV_RX_OFFLOAD_VLAN_FILTER)
			PMD_DRV_LOG(ERR, "VLAN filter offload not supported\n");
	}

	if (mask & ETH_VLAN_EXTEND_MASK) {
		if (offloads & DEV_RX_OFFLOAD_VLAN_EXTEND)
			PMD_DRV_LOG(ERR, "VLAN extend offload not supported\n");
	}

	return 0;
}

static int
avp_dev_stats_get(struct rte_eth_dev *eth_dev, struct rte_eth_stats *stats)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	unsigned int i;

	for (i = 0; i < avp->num_rx_queues; i++) {
		struct avp_queue *rxq = avp->dev_data->rx_queues[i];

		if (rxq) {
			stats->ipackets += rxq->packets;
			stats->ibytes += rxq->bytes;
			stats->ierrors += rxq->errors;

			stats->q_ipackets[i] += rxq->packets;
			stats->q_ibytes[i] += rxq->bytes;
			stats->q_errors[i] += rxq->errors;
		}
	}

	for (i = 0; i < avp->num_tx_queues; i++) {
		struct avp_queue *txq = avp->dev_data->tx_queues[i];

		if (txq) {
			stats->opackets += txq->packets;
			stats->obytes += txq->bytes;
			stats->oerrors += txq->errors;

			stats->q_opackets[i] += txq->packets;
			stats->q_obytes[i] += txq->bytes;
		}
	}

	return 0;
}

static int
avp_dev_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct avp_dev *avp = AVP_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	unsigned int i;

	for (i = 0; i < avp->num_rx_queues; i++) {
		struct avp_queue *rxq = avp->dev_data->rx_queues[i];

		if (rxq) {
			rxq->bytes = 0;
			rxq->packets = 0;
			rxq->errors = 0;
		}
	}

	for (i = 0; i < avp->num_tx_queues; i++) {
		struct avp_queue *txq = avp->dev_data->tx_queues[i];

		if (txq) {
			txq->bytes = 0;
			txq->packets = 0;
			txq->errors = 0;
		}
	}

	return 0;
}

RTE_PMD_REGISTER_PCI(net_avp, rte_avp_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_avp, pci_id_avp_map);
RTE_LOG_REGISTER(avp_logtype_driver, pmd.net.avp.driver, NOTICE);
