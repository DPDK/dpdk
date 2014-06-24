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

#ifndef RTE_EXEC_ENV_LINUXAPP
#error "KNI is not supported"
#endif

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

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

#define KNI_MZ_CHECK(mz) do { if (mz) goto fail; } while (0)

/**
 * KNI context
 */
struct rte_kni {
	char name[RTE_KNI_NAMESIZE];        /**< KNI interface name */
	uint16_t group_id;                  /**< Group ID of KNI devices */
	struct rte_mempool *pktmbuf_pool;   /**< pkt mbuf mempool */
	unsigned mbuf_size;                 /**< mbuf size */

	struct rte_kni_fifo *tx_q;          /**< TX queue */
	struct rte_kni_fifo *rx_q;          /**< RX queue */
	struct rte_kni_fifo *alloc_q;       /**< Allocated mbufs queue */
	struct rte_kni_fifo *free_q;        /**< To be freed mbufs queue */

	/* For request & response */
	struct rte_kni_fifo *req_q;         /**< Request queue */
	struct rte_kni_fifo *resp_q;        /**< Response queue */
	void * sync_addr;                   /**< Req/Resp Mem address */

	struct rte_kni_ops ops;             /**< operations for request */
	uint8_t in_use : 1;                 /**< kni in use */
};

enum kni_ops_status {
	KNI_REQ_NO_REGISTER = 0,
	KNI_REQ_REGISTERED,
};

static void kni_free_mbufs(struct rte_kni *kni);
static void kni_allocate_mbufs(struct rte_kni *kni);

static volatile int kni_fd = -1;

static const struct rte_memzone *
kni_memzone_reserve(const char *name, size_t len, int socket_id,
						unsigned flags)
{
	const struct rte_memzone *mz = rte_memzone_lookup(name);

	if (mz == NULL)
		mz = rte_memzone_reserve(name, len, socket_id, flags);

	return mz;
}

/* It is deprecated and just for backward compatibility */
struct rte_kni *
rte_kni_create(uint8_t port_id,
	       unsigned mbuf_size,
	       struct rte_mempool *pktmbuf_pool,
	       struct rte_kni_ops *ops)
{
	struct rte_kni_conf conf;
	struct rte_eth_dev_info info;

	memset(&info, 0, sizeof(info));
	memset(&conf, 0, sizeof(conf));
	rte_eth_dev_info_get(port_id, &info);

	snprintf(conf.name, sizeof(conf.name), "vEth%u", port_id);
	conf.addr = info.pci_dev->addr;
	conf.id = info.pci_dev->id;
	conf.group_id = (uint16_t)port_id;
	conf.mbuf_size = mbuf_size;

	/* Save the port id for request handling */
	ops->port_id = port_id;

	return rte_kni_alloc(pktmbuf_pool, &conf, ops);
}

struct rte_kni *
rte_kni_alloc(struct rte_mempool *pktmbuf_pool,
	      const struct rte_kni_conf *conf,
	      struct rte_kni_ops *ops)
{
	int ret;
	struct rte_kni_device_info dev_info;
	struct rte_kni *ctx;
	char intf_name[RTE_KNI_NAMESIZE];
#define OBJNAMSIZ 32
	char obj_name[OBJNAMSIZ];
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	if (!pktmbuf_pool || !conf || !conf->name[0])
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

	snprintf(intf_name, RTE_KNI_NAMESIZE, "%s", conf->name);
	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "KNI_INFO_%s", intf_name);
	mz = kni_memzone_reserve(mz_name, sizeof(struct rte_kni),
				SOCKET_ID_ANY, 0);
	KNI_MZ_CHECK(mz == NULL);
	ctx = mz->addr;

	if (ctx->in_use) {
		RTE_LOG(ERR, KNI, "KNI %s is in use\n", ctx->name);
		goto fail;
	}
	memset(ctx, 0, sizeof(struct rte_kni));
	if (ops)
		memcpy(&ctx->ops, ops, sizeof(struct rte_kni_ops));

	memset(&dev_info, 0, sizeof(dev_info));
	dev_info.bus = conf->addr.bus;
	dev_info.devid = conf->addr.devid;
	dev_info.function = conf->addr.function;
	dev_info.vendor_id = conf->id.vendor_id;
	dev_info.device_id = conf->id.device_id;
	dev_info.core_id = conf->core_id;
	dev_info.force_bind = conf->force_bind;
	dev_info.group_id = conf->group_id;
	dev_info.mbuf_size = conf->mbuf_size;

	snprintf(ctx->name, RTE_KNI_NAMESIZE, "%s", intf_name);
	snprintf(dev_info.name, RTE_KNI_NAMESIZE, "%s", intf_name);

	RTE_LOG(INFO, KNI, "pci: %02x:%02x:%02x \t %02x:%02x\n",
		dev_info.bus, dev_info.devid, dev_info.function,
			dev_info.vendor_id, dev_info.device_id);

	/* TX RING */
	snprintf(obj_name, OBJNAMSIZ, "kni_tx_%s", intf_name);
	mz = kni_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	KNI_MZ_CHECK(mz == NULL);
	ctx->tx_q = mz->addr;
	kni_fifo_init(ctx->tx_q, KNI_FIFO_COUNT_MAX);
	dev_info.tx_phys = mz->phys_addr;

	/* RX RING */
	snprintf(obj_name, OBJNAMSIZ, "kni_rx_%s", intf_name);
	mz = kni_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	KNI_MZ_CHECK(mz == NULL);
	ctx->rx_q = mz->addr;
	kni_fifo_init(ctx->rx_q, KNI_FIFO_COUNT_MAX);
	dev_info.rx_phys = mz->phys_addr;

	/* ALLOC RING */
	snprintf(obj_name, OBJNAMSIZ, "kni_alloc_%s", intf_name);
	mz = kni_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	KNI_MZ_CHECK(mz == NULL);
	ctx->alloc_q = mz->addr;
	kni_fifo_init(ctx->alloc_q, KNI_FIFO_COUNT_MAX);
	dev_info.alloc_phys = mz->phys_addr;

	/* FREE RING */
	snprintf(obj_name, OBJNAMSIZ, "kni_free_%s", intf_name);
	mz = kni_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	KNI_MZ_CHECK(mz == NULL);
	ctx->free_q = mz->addr;
	kni_fifo_init(ctx->free_q, KNI_FIFO_COUNT_MAX);
	dev_info.free_phys = mz->phys_addr;

	/* Request RING */
	snprintf(obj_name, OBJNAMSIZ, "kni_req_%s", intf_name);
	mz = kni_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	KNI_MZ_CHECK(mz == NULL);
	ctx->req_q = mz->addr;
	kni_fifo_init(ctx->req_q, KNI_FIFO_COUNT_MAX);
	dev_info.req_phys = mz->phys_addr;

	/* Response RING */
	snprintf(obj_name, OBJNAMSIZ, "kni_resp_%s", intf_name);
	mz = kni_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	KNI_MZ_CHECK(mz == NULL);
	ctx->resp_q = mz->addr;
	kni_fifo_init(ctx->resp_q, KNI_FIFO_COUNT_MAX);
	dev_info.resp_phys = mz->phys_addr;

	/* Req/Resp sync mem area */
	snprintf(obj_name, OBJNAMSIZ, "kni_sync_%s", intf_name);
	mz = kni_memzone_reserve(obj_name, KNI_FIFO_SIZE, SOCKET_ID_ANY, 0);
	KNI_MZ_CHECK(mz == NULL);
	ctx->sync_addr = mz->addr;
	dev_info.sync_va = mz->addr;
	dev_info.sync_phys = mz->phys_addr;

	/* MBUF mempool */
	snprintf(mz_name, sizeof(mz_name), RTE_MEMPOOL_OBJ_NAME,
		pktmbuf_pool->name);
	mz = rte_memzone_lookup(mz_name);
	KNI_MZ_CHECK(mz == NULL);
	dev_info.mbuf_va = mz->addr;
	dev_info.mbuf_phys = mz->phys_addr;
	ctx->pktmbuf_pool = pktmbuf_pool;
	ctx->group_id = conf->group_id;
	ctx->mbuf_size = conf->mbuf_size;

	ret = ioctl(kni_fd, RTE_KNI_IOCTL_CREATE, &dev_info);
	KNI_MZ_CHECK(ret < 0);

	ctx->in_use = 1;

	return ctx;

fail:

	return NULL;
}

static void
kni_free_fifo(struct rte_kni_fifo *fifo)
{
	int ret;
	struct rte_mbuf *pkt;

	do {
		ret = kni_fifo_get(fifo, (void **)&pkt, 1);
		if (ret)
			rte_pktmbuf_free(pkt);
	} while (ret);
}

int
rte_kni_release(struct rte_kni *kni)
{
	struct rte_kni_device_info dev_info;

	if (!kni || !kni->in_use)
		return -1;

	snprintf(dev_info.name, sizeof(dev_info.name), "%s", kni->name);
	if (ioctl(kni_fd, RTE_KNI_IOCTL_RELEASE, &dev_info) < 0) {
		RTE_LOG(ERR, KNI, "Fail to release kni device\n");
		return -1;
	}

	/* mbufs in all fifo should be released, except request/response */
	kni_free_fifo(kni->tx_q);
	kni_free_fifo(kni->rx_q);
	kni_free_fifo(kni->alloc_q);
	kni_free_fifo(kni->free_q);
	memset(kni, 0, sizeof(struct rte_kni));

	return 0;
}

int
rte_kni_handle_request(struct rte_kni *kni)
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
			req->result = kni->ops.change_mtu(kni->ops.port_id,
							req->new_mtu);
		break;
	case RTE_KNI_REQ_CFG_NETWORK_IF: /* Set network interface up/down */
		if (kni->ops.config_network_if)
			req->result = kni->ops.config_network_if(\
					kni->ops.port_id, req->if_up);
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

/* It is deprecated and just for backward compatibility */
uint8_t
rte_kni_get_port_id(struct rte_kni *kni)
{
	if (!kni)
		return ~0x0;

	return kni->ops.port_id;
}

struct rte_kni *
rte_kni_get(const char *name)
{
	struct rte_kni *kni;
	const struct rte_memzone *mz;
	char mz_name[RTE_MEMZONE_NAMESIZE];

	if (!name || !name[0])
		return NULL;

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "KNI_INFO_%s", name);
	mz = rte_memzone_lookup(mz_name);
	if (!mz)
		return NULL;

	kni = mz->addr;
	if (!kni->in_use)
		return NULL;

	return kni;
}

/*
 * It is deprecated and just for backward compatibility.
 */
struct rte_kni *
rte_kni_info_get(uint8_t port_id)
{
	char name[RTE_MEMZONE_NAMESIZE];

	if (port_id >= RTE_MAX_ETHPORTS)
		return NULL;

	snprintf(name, RTE_MEMZONE_NAMESIZE, "vEth%u", port_id);

	return rte_kni_get(name);
}

static enum kni_ops_status
kni_check_request_register(struct rte_kni_ops *ops)
{
	/* check if KNI request ops has been registered*/
	if( NULL == ops )
		return KNI_REQ_NO_REGISTER;

	if((NULL == ops->change_mtu) && (NULL == ops->config_network_if))
		return KNI_REQ_NO_REGISTER;

	return KNI_REQ_REGISTERED;
}

int
rte_kni_register_handlers(struct rte_kni *kni,struct rte_kni_ops *ops)
{
	enum kni_ops_status req_status;

	if (NULL == ops) {
		RTE_LOG(ERR, KNI, "Invalid KNI request operation.\n");
		return -1;
	}

	if (NULL == kni) {
		RTE_LOG(ERR, KNI, "Invalid kni info.\n");
		return -1;
	}

	req_status = kni_check_request_register(&kni->ops);
	if ( KNI_REQ_REGISTERED == req_status) {
		RTE_LOG(ERR, KNI, "The KNI request operation"
					"has already registered.\n");
		return -1;
	}

	memcpy(&kni->ops, ops, sizeof(struct rte_kni_ops));
	return 0;
}

int
rte_kni_unregister_handlers(struct rte_kni *kni)
{
	if (NULL == kni) {
		RTE_LOG(ERR, KNI, "Invalid kni info.\n");
		return -1;
	}

	kni->ops.change_mtu = NULL;
	kni->ops.config_network_if = NULL;
	return 0;
}
void
rte_kni_close(void)
{
	if (kni_fd < 0)
		return;

	close(kni_fd);
	kni_fd = -1;
}
