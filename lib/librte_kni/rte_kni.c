/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
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
 */

#ifndef RTE_EXEC_ENV_LINUXAPP
#error "KNI is not supported"
#endif

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/if.h>

#include <rte_string_fns.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_kni.h>
#include <rte_memzone.h>
#include <exec-env/rte_kni_common.h>
#include "rte_kni_fifo.h"

#define MAX_MBUF_BURST_NUM            32

/* Maximum number of ring entries */
#define KNI_FIFO_COUNT_MAX     1024
#define KNI_FIFO_SIZE          (KNI_FIFO_COUNT_MAX * sizeof(void *) + \
					sizeof(struct rte_kni_fifo))

#define KNI_REQUEST_MBUF_NUM_MAX      32

/**
 * KNI context
 */
struct rte_kni {
	char name[IFNAMSIZ];                /**< KNI interface name */
	uint8_t port_id;                    /**< Port id KNI associate with */
	struct rte_mempool *pktmbuf_pool;   /**< pkt mbuf mempool */
	unsigned mbuf_size;                 /**< mbuf size */

	struct rte_kni_fifo *tx_q;          /**< TX queue */
	struct rte_kni_fifo *rx_q;          /**< RX queue */
	struct rte_kni_fifo *alloc_q;       /**< Allocated mbufs queue */
	struct rte_kni_fifo *free_q;        /**< To be freed mbufs queue */

	/* For request & response */
	struct rte_kni_fifo *req_q;         /**< Request queue */
	struct rte_kni_fifo *resp_q;        /**< Response queue */
	void * sync_addr;					/**< Req/Resp Mem address */

	struct rte_kni_ops ops;             /**< operations for request */
};

static void kni_free_mbufs(struct rte_kni *kni);
static void kni_allocate_mbufs(struct rte_kni *kni);

static int kni_fd = -1;


struct rte_kni *
rte_kni_create(uint8_t port_id,
		unsigned mbuf_size,
		struct rte_mempool *pktmbuf_pool,
		struct rte_kni_ops *ops)
{
	struct rte_kni_device_info dev_info;
	struct rte_eth_dev_info eth_dev_info;
	struct rte_kni *ctx;
	char itf_name[IFNAMSIZ];
#define OBJNAMSIZ 32
	char obj_name[OBJNAMSIZ];
	const struct rte_memzone *mz;

	if (port_id >= RTE_MAX_ETHPORTS || pktmbuf_pool == NULL || !ops)
		return NULL;

	/* Check FD and open once */
	if (kni_fd < 0) {
		kni_fd = open("/dev/" KNI_DEVICE, O_RDWR);
		if (kni_fd < 0) {
			RTE_LOG(ERR, KNI, "Can not open /dev/%s\n",
							KNI_DEVICE);
			return NULL;
		}
	}

	rte_eth_dev_info_get(port_id, &eth_dev_info);
	RTE_LOG(INFO, KNI, "pci: %02x:%02x:%02x \t %02x:%02x\n",
					eth_dev_info.pci_dev->addr.bus,
					eth_dev_info.pci_dev->addr.devid,
					eth_dev_info.pci_dev->addr.function,
					eth_dev_info.pci_dev->id.vendor_id,
					eth_dev_info.pci_dev->id.device_id);
	dev_info.bus = eth_dev_info.pci_dev->addr.bus;
	dev_info.devid = eth_dev_info.pci_dev->addr.devid;
	dev_info.function = eth_dev_info.pci_dev->addr.function;
	dev_info.vendor_id = eth_dev_info.pci_dev->id.vendor_id;
	dev_info.device_id = eth_dev_info.pci_dev->id.device_id;

	ctx = rte_zmalloc("kni devs", sizeof(struct rte_kni), 0);
	if (ctx == NULL)
		rte_panic("Cannot allocate memory for kni dev\n");
	memcpy(&ctx->ops, ops, sizeof(struct rte_kni_ops));

	rte_snprintf(itf_name, IFNAMSIZ, "vEth%u", port_id);
	rte_snprintf(ctx->name, IFNAMSIZ, itf_name);
	rte_snprintf(dev_info.name, IFNAMSIZ, itf_name);

	/* TX RING */
	rte_snprintf(obj_name, OBJNAMSIZ, "kni_tx_%d", port_id);
	mz = rte_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	if (mz == NULL || mz->addr == NULL)
		rte_panic("Cannot create kni_tx_%d queue\n", port_id);
	ctx->tx_q = mz->addr;
	kni_fifo_init(ctx->tx_q, KNI_FIFO_COUNT_MAX);
	dev_info.tx_phys = mz->phys_addr;

	/* RX RING */
	rte_snprintf(obj_name, OBJNAMSIZ, "kni_rx_%d", port_id);
	mz = rte_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	if (mz == NULL || mz->addr == NULL)
		rte_panic("Cannot create kni_rx_%d queue\n", port_id);
	ctx->rx_q = mz->addr;
	kni_fifo_init(ctx->rx_q, KNI_FIFO_COUNT_MAX);
	dev_info.rx_phys = mz->phys_addr;

	/* ALLOC RING */
	rte_snprintf(obj_name, OBJNAMSIZ, "kni_alloc_%d", port_id);
	mz = rte_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	if (mz == NULL || mz->addr == NULL)
		rte_panic("Cannot create kni_alloc_%d queue\n", port_id);
	ctx->alloc_q = mz->addr;
	kni_fifo_init(ctx->alloc_q, KNI_FIFO_COUNT_MAX);
	dev_info.alloc_phys = mz->phys_addr;

	/* FREE RING */
	rte_snprintf(obj_name, OBJNAMSIZ, "kni_free_%d", port_id);
	mz = rte_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	if (mz == NULL || mz->addr == NULL)
		rte_panic("Cannot create kni_free_%d queue\n", port_id);
	ctx->free_q = mz->addr;
	kni_fifo_init(ctx->free_q, KNI_FIFO_COUNT_MAX);
	dev_info.free_phys = mz->phys_addr;

	/* Request RING */
	rte_snprintf(obj_name, OBJNAMSIZ, "kni_req_%d", port_id);
	mz = rte_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	if (mz == NULL || mz->addr == NULL)
		rte_panic("Cannot create kni_req_%d ring\n", port_id);
	ctx->req_q = mz->addr;
	kni_fifo_init(ctx->req_q, KNI_FIFO_COUNT_MAX);
	dev_info.req_phys = mz->phys_addr;

	/* Response RING */
	rte_snprintf(obj_name, OBJNAMSIZ, "kni_resp_%d", port_id);
	mz = rte_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	if (mz == NULL || mz->addr == NULL)
		rte_panic("Cannot create kni_resp_%d ring\n", port_id);
	ctx->resp_q = mz->addr;
	kni_fifo_init(ctx->resp_q, KNI_FIFO_COUNT_MAX);
	dev_info.resp_phys = mz->phys_addr;

	/* Req/Resp sync mem area */
	rte_snprintf(obj_name, OBJNAMSIZ, "kni_sync_%d", port_id);
	mz = rte_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	if (mz == NULL || mz->addr == NULL)
		rte_panic("Cannot create kni_sync_%d mem\n", port_id);
	ctx->sync_addr = mz->addr;
	dev_info.sync_va = mz->addr;
	dev_info.sync_phys = mz->phys_addr;

	/* MBUF mempool */
	mz = rte_memzone_lookup("MP_mbuf_pool");
	if (mz == NULL) {
		RTE_LOG(ERR, KNI, "Can not find MP_mbuf_pool\n");
		goto fail;
	}
	dev_info.mbuf_va = mz->addr;
	dev_info.mbuf_phys = mz->phys_addr;
	ctx->pktmbuf_pool = pktmbuf_pool;
	ctx->port_id = port_id;
	ctx->mbuf_size = mbuf_size;

	/* Configure the buffer size which will be checked in kernel module */
	dev_info.mbuf_size = ctx->mbuf_size;

	if (ioctl(kni_fd, RTE_KNI_IOCTL_CREATE, &dev_info) < 0) {
		RTE_LOG(ERR, KNI, "Fail to create kni device\n");
		goto fail;
	}

	return ctx;

fail:
	if (ctx != NULL)
		rte_free(ctx);

	return NULL;
}

/**
 * It is called in the same lcore of receiving packets, and polls the request
 * mbufs sent from kernel space. Then analyzes it and calls the specific
 * actions for the specific requests. Finally constructs the response mbuf and
 * puts it back to the resp_q.
 */
static int
kni_request_handler(struct rte_kni *kni)
{
	unsigned ret;
	struct rte_kni_request *req;

	if (kni == NULL)
		return -1;

	/* Get request mbuf */
	ret = kni_fifo_get(kni->req_q, (void **)&req, 1);
	if (ret != 1)
		return 0; /* It is OK of can not getting the request mbuf */

	if (req != kni->sync_addr) {
		rte_panic("Wrong req pointer %p\n", req);
	}

	/* Analyze the request and call the relevant actions for it */
	switch (req->req_id) {
	case RTE_KNI_REQ_CHANGE_MTU: /* Change MTU */
		if (kni->ops.change_mtu)
			req->result = kni->ops.change_mtu(kni->port_id,
							req->new_mtu);
		break;
	case RTE_KNI_REQ_CFG_NETWORK_IF: /* Set network interface up/down */
		if (kni->ops.config_network_if)
			req->result = kni->ops.config_network_if(kni->port_id,
								req->if_up);
		break;
	default:
		RTE_LOG(ERR, KNI, "Unknown request id %u\n", req->req_id);
		req->result = -EINVAL;
		break;
	}

	/* Construct response mbuf and put it back to resp_q */
	ret = kni_fifo_put(kni->resp_q, (void **)&req, 1);
	if (ret != 1) {
		RTE_LOG(ERR, KNI, "Fail to put the muf back to resp_q\n");
		return -1; /* It is an error of can't putting the mbuf back */
	}

	return 0;
}

unsigned
rte_kni_tx_burst(struct rte_kni *kni, struct rte_mbuf **mbufs, unsigned num)
{
	unsigned ret = kni_fifo_put(kni->rx_q, (void **)mbufs, num);

	/* Get mbufs from free_q and then free them */
	kni_free_mbufs(kni);

	/* Handle the requests from kernel space */
	kni_request_handler(kni);

	return ret;
}

unsigned
rte_kni_rx_burst(struct rte_kni *kni, struct rte_mbuf **mbufs, unsigned num)
{
	unsigned ret = kni_fifo_get(kni->tx_q, (void **)mbufs, num);

	/* Allocate mbufs and then put them into alloc_q */
	kni_allocate_mbufs(kni);

	return ret;
}

static void
kni_free_mbufs(struct rte_kni *kni)
{
	int i, ret;
	struct rte_mbuf *pkts[MAX_MBUF_BURST_NUM];

	ret = kni_fifo_get(kni->free_q, (void **)pkts, MAX_MBUF_BURST_NUM);
	if (likely(ret > 0)) {
		for (i = 0; i < ret; i++)
			rte_pktmbuf_free(pkts[i]);
	}
}

static void
kni_allocate_mbufs(struct rte_kni *kni)
{
	int i, ret;
	struct rte_mbuf *pkts[MAX_MBUF_BURST_NUM];

	/* Check if pktmbuf pool has been configured */
	if (kni->pktmbuf_pool == NULL) {
		RTE_LOG(ERR, KNI, "No valid mempool for allocating mbufs\n");
		return;
	}

	for (i = 0; i < MAX_MBUF_BURST_NUM; i++) {
		pkts[i] = rte_pktmbuf_alloc(kni->pktmbuf_pool);
		if (unlikely(pkts[i] == NULL)) {
			/* Out of memory */
			RTE_LOG(ERR, KNI, "Out of memory\n");
			break;
		}
	}

	/* No pkt mbuf alocated */
	if (i <= 0)
		return;

	ret = kni_fifo_put(kni->alloc_q, (void **)pkts, i);

	/* Check if any mbufs not put into alloc_q, and then free them */
	if (ret >= 0 && ret < i && ret < MAX_MBUF_BURST_NUM) {
		int j;

		for (j = ret; j < i; j++)
			rte_pktmbuf_free(pkts[j]);
	}
}

uint8_t
rte_kni_get_port_id(struct rte_kni *kni)
{
	if (kni == NULL)
		return ~0x0;

	return kni->port_id;
}

