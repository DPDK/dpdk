/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef RTE_EXEC_ENV_LINUX
#error "KNI is not supported"
#endif

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/version.h>

#include <rte_string_fns.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_kni.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal_memconfig.h>
#include <rte_kni_common.h>
#include "rte_kni_fifo.h"

#define MAX_MBUF_BURST_NUM            32

/* Maximum number of ring entries */
#define KNI_FIFO_COUNT_MAX     1024
#define KNI_FIFO_SIZE          (KNI_FIFO_COUNT_MAX * sizeof(void *) + \
					sizeof(struct rte_kni_fifo))

#define KNI_REQUEST_MBUF_NUM_MAX      32

#define KNI_MEM_CHECK(cond, fail) do { if (cond) goto fail; } while (0)

#define KNI_MZ_NAME_FMT			"kni_info_%s"
#define KNI_TX_Q_MZ_NAME_FMT		"kni_tx_%s"
#define KNI_RX_Q_MZ_NAME_FMT		"kni_rx_%s"
#define KNI_ALLOC_Q_MZ_NAME_FMT		"kni_alloc_%s"
#define KNI_FREE_Q_MZ_NAME_FMT		"kni_free_%s"
#define KNI_REQ_Q_MZ_NAME_FMT		"kni_req_%s"
#define KNI_RESP_Q_MZ_NAME_FMT		"kni_resp_%s"
#define KNI_SYNC_ADDR_MZ_NAME_FMT	"kni_sync_%s"

TAILQ_HEAD(rte_kni_list, rte_tailq_entry);

static struct rte_tailq_elem rte_kni_tailq = {
	.name = "RTE_KNI",
};
EAL_REGISTER_TAILQ(rte_kni_tailq)

/**
 * KNI context
 */
struct rte_kni {
	char name[RTE_KNI_NAMESIZE];        /**< KNI interface name */
	uint16_t group_id;                  /**< Group ID of KNI devices */
	uint32_t slot_id;                   /**< KNI pool slot ID */
	struct rte_mempool *pktmbuf_pool;   /**< pkt mbuf mempool */
	unsigned int mbuf_size;                 /**< mbuf size */

	const struct rte_memzone *m_tx_q;   /**< TX queue memzone */
	const struct rte_memzone *m_rx_q;   /**< RX queue memzone */
	const struct rte_memzone *m_alloc_q;/**< Alloc queue memzone */
	const struct rte_memzone *m_free_q; /**< Free queue memzone */

	struct rte_kni_fifo *tx_q;          /**< TX queue */
	struct rte_kni_fifo *rx_q;          /**< RX queue */
	struct rte_kni_fifo *alloc_q;       /**< Allocated mbufs queue */
	struct rte_kni_fifo *free_q;        /**< To be freed mbufs queue */

	const struct rte_memzone *m_req_q;  /**< Request queue memzone */
	const struct rte_memzone *m_resp_q; /**< Response queue memzone */
	const struct rte_memzone *m_sync_addr;/**< Sync addr memzone */

	/* For request & response */
	struct rte_kni_fifo *req_q;         /**< Request queue */
	struct rte_kni_fifo *resp_q;        /**< Response queue */
	void *sync_addr;                   /**< Req/Resp Mem address */

	struct rte_kni_ops ops;             /**< operations for request */
};

enum kni_ops_status {
	KNI_REQ_NO_REGISTER = 0,
	KNI_REQ_REGISTERED,
};

static void kni_free_mbufs(struct rte_kni *kni);
static void kni_allocate_mbufs(struct rte_kni *kni);

static volatile int kni_fd = -1;

/* Shall be called before any allocation happens */
int
rte_kni_init(unsigned int max_kni_ifaces __rte_unused)
{
	RTE_LOG(WARNING, KNI, "WARNING: KNI is deprecated and will be removed in DPDK 23.11\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
	if (rte_eal_iova_mode() != RTE_IOVA_PA) {
		RTE_LOG(ERR, KNI, "KNI requires IOVA as PA\n");
		return -1;
	}
#endif

	/* Check FD and open */
	if (kni_fd < 0) {
		kni_fd = open("/dev/" KNI_DEVICE, O_RDWR);
		if (kni_fd < 0) {
			RTE_LOG(ERR, KNI,
				"Can not open /dev/%s\n", KNI_DEVICE);
			return -1;
		}
	}

	return 0;
}

static struct rte_kni *
__rte_kni_get(const char *name)
{
	struct rte_kni *kni;
	struct rte_tailq_entry *te;
	struct rte_kni_list *kni_list;

	kni_list = RTE_TAILQ_CAST(rte_kni_tailq.head, rte_kni_list);

	TAILQ_FOREACH(te, kni_list, next) {
		kni = te->data;
		if (strncmp(name, kni->name, RTE_KNI_NAMESIZE) == 0)
			break;
	}

	if (te == NULL)
		kni = NULL;

	return kni;
}

static int
kni_reserve_mz(struct rte_kni *kni)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, KNI_TX_Q_MZ_NAME_FMT, kni->name);
	kni->m_tx_q = rte_memzone_reserve(mz_name, KNI_FIFO_SIZE, SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG);
	KNI_MEM_CHECK(kni->m_tx_q == NULL, tx_q_fail);

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, KNI_RX_Q_MZ_NAME_FMT, kni->name);
	kni->m_rx_q = rte_memzone_reserve(mz_name, KNI_FIFO_SIZE, SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG);
	KNI_MEM_CHECK(kni->m_rx_q == NULL, rx_q_fail);

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, KNI_ALLOC_Q_MZ_NAME_FMT, kni->name);
	kni->m_alloc_q = rte_memzone_reserve(mz_name, KNI_FIFO_SIZE, SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG);
	KNI_MEM_CHECK(kni->m_alloc_q == NULL, alloc_q_fail);

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, KNI_FREE_Q_MZ_NAME_FMT, kni->name);
	kni->m_free_q = rte_memzone_reserve(mz_name, KNI_FIFO_SIZE, SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG);
	KNI_MEM_CHECK(kni->m_free_q == NULL, free_q_fail);

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, KNI_REQ_Q_MZ_NAME_FMT, kni->name);
	kni->m_req_q = rte_memzone_reserve(mz_name, KNI_FIFO_SIZE, SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG);
	KNI_MEM_CHECK(kni->m_req_q == NULL, req_q_fail);

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, KNI_RESP_Q_MZ_NAME_FMT, kni->name);
	kni->m_resp_q = rte_memzone_reserve(mz_name, KNI_FIFO_SIZE, SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG);
	KNI_MEM_CHECK(kni->m_resp_q == NULL, resp_q_fail);

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, KNI_SYNC_ADDR_MZ_NAME_FMT, kni->name);
	kni->m_sync_addr = rte_memzone_reserve(mz_name, KNI_FIFO_SIZE, SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG);
	KNI_MEM_CHECK(kni->m_sync_addr == NULL, sync_addr_fail);

	return 0;

sync_addr_fail:
	rte_memzone_free(kni->m_resp_q);
resp_q_fail:
	rte_memzone_free(kni->m_req_q);
req_q_fail:
	rte_memzone_free(kni->m_free_q);
free_q_fail:
	rte_memzone_free(kni->m_alloc_q);
alloc_q_fail:
	rte_memzone_free(kni->m_rx_q);
rx_q_fail:
	rte_memzone_free(kni->m_tx_q);
tx_q_fail:
	return -1;
}

static void
kni_release_mz(struct rte_kni *kni)
{
	rte_memzone_free(kni->m_tx_q);
	rte_memzone_free(kni->m_rx_q);
	rte_memzone_free(kni->m_alloc_q);
	rte_memzone_free(kni->m_free_q);
	rte_memzone_free(kni->m_req_q);
	rte_memzone_free(kni->m_resp_q);
	rte_memzone_free(kni->m_sync_addr);
}

struct rte_kni *
rte_kni_alloc(struct rte_mempool *pktmbuf_pool,
	      const struct rte_kni_conf *conf,
	      struct rte_kni_ops *ops)
{
	int ret;
	struct rte_kni_device_info dev_info;
	struct rte_kni *kni;
	struct rte_tailq_entry *te;
	struct rte_kni_list *kni_list;

	if (!pktmbuf_pool || !conf || !conf->name[0])
		return NULL;

	/* Check if KNI subsystem has been initialized */
	if (kni_fd < 0) {
		RTE_LOG(ERR, KNI, "KNI subsystem has not been initialized. Invoke rte_kni_init() first\n");
		return NULL;
	}

	rte_mcfg_tailq_write_lock();

	kni = __rte_kni_get(conf->name);
	if (kni != NULL) {
		RTE_LOG(ERR, KNI, "KNI already exists\n");
		goto unlock;
	}

	te = rte_zmalloc("KNI_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RTE_LOG(ERR, KNI, "Failed to allocate tailq entry\n");
		goto unlock;
	}

	kni = rte_zmalloc("KNI", sizeof(struct rte_kni), RTE_CACHE_LINE_SIZE);
	if (kni == NULL) {
		RTE_LOG(ERR, KNI, "KNI memory allocation failed\n");
		goto kni_fail;
	}

	strlcpy(kni->name, conf->name, RTE_KNI_NAMESIZE);

	if (ops)
		memcpy(&kni->ops, ops, sizeof(struct rte_kni_ops));
	else
		kni->ops.port_id = UINT16_MAX;

	memset(&dev_info, 0, sizeof(dev_info));
	dev_info.core_id = conf->core_id;
	dev_info.force_bind = conf->force_bind;
	dev_info.group_id = conf->group_id;
	dev_info.mbuf_size = conf->mbuf_size;
	dev_info.mtu = conf->mtu;
	dev_info.min_mtu = conf->min_mtu;
	dev_info.max_mtu = conf->max_mtu;

	memcpy(dev_info.mac_addr, conf->mac_addr, RTE_ETHER_ADDR_LEN);

	strlcpy(dev_info.name, conf->name, RTE_KNI_NAMESIZE);

	ret = kni_reserve_mz(kni);
	if (ret < 0)
		goto mz_fail;

	/* TX RING */
	kni->tx_q = kni->m_tx_q->addr;
	kni_fifo_init(kni->tx_q, KNI_FIFO_COUNT_MAX);
	dev_info.tx_phys = kni->m_tx_q->iova;

	/* RX RING */
	kni->rx_q = kni->m_rx_q->addr;
	kni_fifo_init(kni->rx_q, KNI_FIFO_COUNT_MAX);
	dev_info.rx_phys = kni->m_rx_q->iova;

	/* ALLOC RING */
	kni->alloc_q = kni->m_alloc_q->addr;
	kni_fifo_init(kni->alloc_q, KNI_FIFO_COUNT_MAX);
	dev_info.alloc_phys = kni->m_alloc_q->iova;

	/* FREE RING */
	kni->free_q = kni->m_free_q->addr;
	kni_fifo_init(kni->free_q, KNI_FIFO_COUNT_MAX);
	dev_info.free_phys = kni->m_free_q->iova;

	/* Request RING */
	kni->req_q = kni->m_req_q->addr;
	kni_fifo_init(kni->req_q, KNI_FIFO_COUNT_MAX);
	dev_info.req_phys = kni->m_req_q->iova;

	/* Response RING */
	kni->resp_q = kni->m_resp_q->addr;
	kni_fifo_init(kni->resp_q, KNI_FIFO_COUNT_MAX);
	dev_info.resp_phys = kni->m_resp_q->iova;

	/* Req/Resp sync mem area */
	kni->sync_addr = kni->m_sync_addr->addr;
	dev_info.sync_va = kni->m_sync_addr->addr;
	dev_info.sync_phys = kni->m_sync_addr->iova;

	kni->pktmbuf_pool = pktmbuf_pool;
	kni->group_id = conf->group_id;
	kni->mbuf_size = conf->mbuf_size;

	dev_info.iova_mode = (rte_eal_iova_mode() == RTE_IOVA_VA) ? 1 : 0;

	ret = ioctl(kni_fd, RTE_KNI_IOCTL_CREATE, &dev_info);
	if (ret < 0)
		goto ioctl_fail;

	te->data = kni;

	kni_list = RTE_TAILQ_CAST(rte_kni_tailq.head, rte_kni_list);
	TAILQ_INSERT_TAIL(kni_list, te, next);

	rte_mcfg_tailq_write_unlock();

	/* Allocate mbufs and then put them into alloc_q */
	kni_allocate_mbufs(kni);

	return kni;

ioctl_fail:
	kni_release_mz(kni);
mz_fail:
	rte_free(kni);
kni_fail:
	rte_free(te);
unlock:
	rte_mcfg_tailq_write_unlock();

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

static void *
va2pa(struct rte_mbuf *m)
{
	return (void *)((unsigned long)m -
			((unsigned long)m->buf_addr - (unsigned long)rte_mbuf_iova_get(m)));
}

static void *
va2pa_all(struct rte_mbuf *mbuf)
{
	void *phy_mbuf = va2pa(mbuf);
	struct rte_mbuf *next = mbuf->next;
	while (next) {
		mbuf->next = va2pa(next);
		mbuf = next;
		next = mbuf->next;
	}
	return phy_mbuf;
}

static void
obj_free(struct rte_mempool *mp __rte_unused, void *opaque, void *obj,
		unsigned obj_idx __rte_unused)
{
	struct rte_mbuf *m = obj;
	void *mbuf_phys = opaque;

	if (va2pa(m) == mbuf_phys)
		rte_pktmbuf_free(m);
}

static void
kni_free_fifo_phy(struct rte_mempool *mp, struct rte_kni_fifo *fifo)
{
	void *mbuf_phys;
	int ret;

	do {
		ret = kni_fifo_get(fifo, &mbuf_phys, 1);
		if (ret)
			rte_mempool_obj_iter(mp, obj_free, mbuf_phys);
	} while (ret);
}

int
rte_kni_release(struct rte_kni *kni)
{
	struct rte_tailq_entry *te;
	struct rte_kni_list *kni_list;
	struct rte_kni_device_info dev_info;
	uint32_t retry = 5;

	if (!kni)
		return -1;

	kni_list = RTE_TAILQ_CAST(rte_kni_tailq.head, rte_kni_list);

	rte_mcfg_tailq_write_lock();

	TAILQ_FOREACH(te, kni_list, next) {
		if (te->data == kni)
			break;
	}

	if (te == NULL)
		goto unlock;

	strlcpy(dev_info.name, kni->name, sizeof(dev_info.name));
	if (ioctl(kni_fd, RTE_KNI_IOCTL_RELEASE, &dev_info) < 0) {
		RTE_LOG(ERR, KNI, "Fail to release kni device\n");
		goto unlock;
	}

	TAILQ_REMOVE(kni_list, te, next);

	rte_mcfg_tailq_write_unlock();

	/* mbufs in all fifo should be released, except request/response */

	/* wait until all rxq packets processed by kernel */
	while (kni_fifo_count(kni->rx_q) && retry--)
		usleep(1000);

	if (kni_fifo_count(kni->rx_q))
		RTE_LOG(ERR, KNI, "Fail to free all Rx-q items\n");

	kni_free_fifo_phy(kni->pktmbuf_pool, kni->alloc_q);
	kni_free_fifo(kni->tx_q);
	kni_free_fifo(kni->free_q);

	kni_release_mz(kni);

	rte_free(kni);

	rte_free(te);

	return 0;

unlock:
	rte_mcfg_tailq_write_unlock();

	return -1;
}

/* default callback for request of configuring device mac address */
static int
kni_config_mac_address(uint16_t port_id, uint8_t mac_addr[])
{
	int ret = 0;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		RTE_LOG(ERR, KNI, "Invalid port id %d\n", port_id);
		return -EINVAL;
	}

	RTE_LOG(INFO, KNI, "Configure mac address of %d", port_id);

	ret = rte_eth_dev_default_mac_addr_set(port_id,
					(struct rte_ether_addr *)mac_addr);
	if (ret < 0)
		RTE_LOG(ERR, KNI, "Failed to config mac_addr for port %d\n",
			port_id);

	return ret;
}

/* default callback for request of configuring promiscuous mode */
static int
kni_config_promiscusity(uint16_t port_id, uint8_t to_on)
{
	int ret;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		RTE_LOG(ERR, KNI, "Invalid port id %d\n", port_id);
		return -EINVAL;
	}

	RTE_LOG(INFO, KNI, "Configure promiscuous mode of %d to %d\n",
		port_id, to_on);

	if (to_on)
		ret = rte_eth_promiscuous_enable(port_id);
	else
		ret = rte_eth_promiscuous_disable(port_id);

	if (ret != 0)
		RTE_LOG(ERR, KNI,
			"Failed to %s promiscuous mode for port %u: %s\n",
			to_on ? "enable" : "disable", port_id,
			rte_strerror(-ret));

	return ret;
}

/* default callback for request of configuring allmulticast mode */
static int
kni_config_allmulticast(uint16_t port_id, uint8_t to_on)
{
	int ret;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		RTE_LOG(ERR, KNI, "Invalid port id %d\n", port_id);
		return -EINVAL;
	}

	RTE_LOG(INFO, KNI, "Configure allmulticast mode of %d to %d\n",
		port_id, to_on);

	if (to_on)
		ret = rte_eth_allmulticast_enable(port_id);
	else
		ret = rte_eth_allmulticast_disable(port_id);
	if (ret != 0)
		RTE_LOG(ERR, KNI,
			"Failed to %s allmulticast mode for port %u: %s\n",
			to_on ? "enable" : "disable", port_id,
			rte_strerror(-ret));

	return ret;
}

int
rte_kni_handle_request(struct rte_kni *kni)
{
	unsigned int ret;
	struct rte_kni_request *req = NULL;

	if (kni == NULL)
		return -1;

	/* Get request mbuf */
	ret = kni_fifo_get(kni->req_q, (void **)&req, 1);
	if (ret != 1)
		return 0; /* It is OK of can not getting the request mbuf */

	if (req != kni->sync_addr) {
		RTE_LOG(ERR, KNI, "Wrong req pointer %p\n", req);
		return -1;
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
			req->result = kni->ops.config_network_if(kni->ops.port_id,
								 req->if_up);
		break;
	case RTE_KNI_REQ_CHANGE_MAC_ADDR: /* Change MAC Address */
		if (kni->ops.config_mac_address)
			req->result = kni->ops.config_mac_address(
					kni->ops.port_id, req->mac_addr);
		else if (kni->ops.port_id != UINT16_MAX)
			req->result = kni_config_mac_address(
					kni->ops.port_id, req->mac_addr);
		break;
	case RTE_KNI_REQ_CHANGE_PROMISC: /* Change PROMISCUOUS MODE */
		if (kni->ops.config_promiscusity)
			req->result = kni->ops.config_promiscusity(
					kni->ops.port_id, req->promiscusity);
		else if (kni->ops.port_id != UINT16_MAX)
			req->result = kni_config_promiscusity(
					kni->ops.port_id, req->promiscusity);
		break;
	case RTE_KNI_REQ_CHANGE_ALLMULTI: /* Change ALLMULTICAST MODE */
		if (kni->ops.config_allmulticast)
			req->result = kni->ops.config_allmulticast(
					kni->ops.port_id, req->allmulti);
		else if (kni->ops.port_id != UINT16_MAX)
			req->result = kni_config_allmulticast(
					kni->ops.port_id, req->allmulti);
		break;
	default:
		RTE_LOG(ERR, KNI, "Unknown request id %u\n", req->req_id);
		req->result = -EINVAL;
		break;
	}

	/* if needed, construct response buffer and put it back to resp_q */
	if (!req->async)
		ret = kni_fifo_put(kni->resp_q, (void **)&req, 1);
	else
		ret = 1;
	if (ret != 1) {
		RTE_LOG(ERR, KNI, "Fail to put the muf back to resp_q\n");
		return -1; /* It is an error of can't putting the mbuf back */
	}

	return 0;
}

unsigned
rte_kni_tx_burst(struct rte_kni *kni, struct rte_mbuf **mbufs, unsigned int num)
{
	num = RTE_MIN(kni_fifo_free_count(kni->rx_q), num);
	void *phy_mbufs[num];
	unsigned int ret;
	unsigned int i;

	for (i = 0; i < num; i++)
		phy_mbufs[i] = va2pa_all(mbufs[i]);

	ret = kni_fifo_put(kni->rx_q, phy_mbufs, num);

	/* Get mbufs from free_q and then free them */
	kni_free_mbufs(kni);

	return ret;
}

unsigned
rte_kni_rx_burst(struct rte_kni *kni, struct rte_mbuf **mbufs, unsigned int num)
{
	unsigned int ret = kni_fifo_get(kni->tx_q, (void **)mbufs, num);

	/* If buffers removed or alloc_q is empty, allocate mbufs and then put them into alloc_q */
	if (ret || (kni_fifo_count(kni->alloc_q) == 0))
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
	void *phys[MAX_MBUF_BURST_NUM];
	int allocq_free;

	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pool) !=
			 offsetof(struct rte_kni_mbuf, pool));
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_addr) !=
			 offsetof(struct rte_kni_mbuf, buf_addr));
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, next) !=
			 offsetof(struct rte_kni_mbuf, next));
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_off) !=
			 offsetof(struct rte_kni_mbuf, data_off));
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			 offsetof(struct rte_kni_mbuf, data_len));
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			 offsetof(struct rte_kni_mbuf, pkt_len));
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) !=
			 offsetof(struct rte_kni_mbuf, ol_flags));

	/* Check if pktmbuf pool has been configured */
	if (kni->pktmbuf_pool == NULL) {
		RTE_LOG(ERR, KNI, "No valid mempool for allocating mbufs\n");
		return;
	}

	allocq_free = kni_fifo_free_count(kni->alloc_q);
	allocq_free = (allocq_free > MAX_MBUF_BURST_NUM) ?
		MAX_MBUF_BURST_NUM : allocq_free;
	for (i = 0; i < allocq_free; i++) {
		pkts[i] = rte_pktmbuf_alloc(kni->pktmbuf_pool);
		if (unlikely(pkts[i] == NULL)) {
			/* Out of memory */
			RTE_LOG(ERR, KNI, "Out of memory\n");
			break;
		}
		phys[i] = va2pa(pkts[i]);
	}

	/* No pkt mbuf allocated */
	if (i <= 0)
		return;

	ret = kni_fifo_put(kni->alloc_q, phys, i);

	/* Check if any mbufs not put into alloc_q, and then free them */
	if (ret >= 0 && ret < i && ret < MAX_MBUF_BURST_NUM) {
		int j;

		for (j = ret; j < i; j++)
			rte_pktmbuf_free(pkts[j]);
	}
}

struct rte_kni *
rte_kni_get(const char *name)
{
	struct rte_kni *kni;

	if (name == NULL || name[0] == '\0')
		return NULL;

	rte_mcfg_tailq_read_lock();

	kni = __rte_kni_get(name);

	rte_mcfg_tailq_read_unlock();

	return kni;
}

const char *
rte_kni_get_name(const struct rte_kni *kni)
{
	return kni->name;
}

static enum kni_ops_status
kni_check_request_register(struct rte_kni_ops *ops)
{
	/* check if KNI request ops has been registered*/
	if (ops == NULL)
		return KNI_REQ_NO_REGISTER;

	if (ops->change_mtu == NULL
	    && ops->config_network_if == NULL
	    && ops->config_mac_address == NULL
	    && ops->config_promiscusity == NULL
	    && ops->config_allmulticast == NULL)
		return KNI_REQ_NO_REGISTER;

	return KNI_REQ_REGISTERED;
}

int
rte_kni_register_handlers(struct rte_kni *kni, struct rte_kni_ops *ops)
{
	enum kni_ops_status req_status;

	if (ops == NULL) {
		RTE_LOG(ERR, KNI, "Invalid KNI request operation.\n");
		return -1;
	}

	if (kni == NULL) {
		RTE_LOG(ERR, KNI, "Invalid kni info.\n");
		return -1;
	}

	req_status = kni_check_request_register(&kni->ops);
	if (req_status == KNI_REQ_REGISTERED) {
		RTE_LOG(ERR, KNI, "The KNI request operation has already registered.\n");
		return -1;
	}

	memcpy(&kni->ops, ops, sizeof(struct rte_kni_ops));
	return 0;
}

int
rte_kni_unregister_handlers(struct rte_kni *kni)
{
	if (kni == NULL) {
		RTE_LOG(ERR, KNI, "Invalid kni info.\n");
		return -1;
	}

	memset(&kni->ops, 0, sizeof(struct rte_kni_ops));

	return 0;
}

int
rte_kni_update_link(struct rte_kni *kni, unsigned int linkup)
{
	char path[64];
	char old_carrier[2];
	const char *new_carrier;
	int old_linkup;
	int fd, ret;

	if (kni == NULL)
		return -1;

	snprintf(path, sizeof(path), "/sys/devices/virtual/net/%s/carrier",
		kni->name);

	fd = open(path, O_RDWR);
	if (fd == -1) {
		RTE_LOG(ERR, KNI, "Failed to open file: %s.\n", path);
		return -1;
	}

	ret = read(fd, old_carrier, 2);
	if (ret < 1) {
		close(fd);
		return -1;
	}
	old_linkup = (old_carrier[0] == '1');

	if (old_linkup == (int)linkup)
		goto out;

	new_carrier = linkup ? "1" : "0";
	ret = write(fd, new_carrier, 1);
	if (ret < 1) {
		RTE_LOG(ERR, KNI, "Failed to write file: %s.\n", path);
		close(fd);
		return -1;
	}
out:
	close(fd);
	return old_linkup;
}

void
rte_kni_close(void)
{
	if (kni_fd < 0)
		return;

	close(kni_fd);
	kni_fd = -1;
}
