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

/* Macro to cast the ethernet device private data to a AVP object */
#define AVP_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct avp_adapter *)adapter)->avp)

/*
 * This function is based on probe() function in avp_pci.c
 * It returns 0 on success.
 */
static int
eth_avp_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;

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

	return 0;
}

static int
eth_avp_dev_uninit(struct rte_eth_dev *eth_dev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	if (eth_dev->data == NULL)
		return 0;

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
