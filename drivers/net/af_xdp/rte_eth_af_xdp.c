/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation.
 */
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <linux/if_xdp.h>
#include <linux/if_link.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include "af_xdp_deps.h"
#include <bpf/xsk.h>

#include <rte_ethdev.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_vdev.h>
#include <rte_kvargs.h>
#include <rte_bus_vdev.h>
#include <rte_string_fns.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_spinlock.h>

#include "compat.h"


#ifndef SOL_XDP
#define SOL_XDP 283
#endif

#ifndef AF_XDP
#define AF_XDP 44
#endif

#ifndef PF_XDP
#define PF_XDP AF_XDP
#endif

RTE_LOG_REGISTER(af_xdp_logtype, pmd.net.af_xdp, NOTICE);

#define AF_XDP_LOG(level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, af_xdp_logtype,	\
		"%s(): " fmt, __func__, ##args)

#define ETH_AF_XDP_FRAME_SIZE		2048
#define ETH_AF_XDP_NUM_BUFFERS		4096
#define ETH_AF_XDP_DFLT_NUM_DESCS	XSK_RING_CONS__DEFAULT_NUM_DESCS
#define ETH_AF_XDP_DFLT_START_QUEUE_IDX	0
#define ETH_AF_XDP_DFLT_QUEUE_COUNT	1

#define ETH_AF_XDP_RX_BATCH_SIZE	32
#define ETH_AF_XDP_TX_BATCH_SIZE	32


struct xsk_umem_info {
	struct xsk_umem *umem;
	struct rte_ring *buf_ring;
	const struct rte_memzone *mz;
	struct rte_mempool *mb_pool;
	void *buffer;
	uint8_t refcnt;
	uint32_t max_xsks;
};

struct rx_stats {
	uint64_t rx_pkts;
	uint64_t rx_bytes;
	uint64_t rx_dropped;
};

struct pkt_rx_queue {
	struct xsk_ring_cons rx;
	struct xsk_umem_info *umem;
	struct xsk_socket *xsk;
	struct rte_mempool *mb_pool;

	struct rx_stats stats;

	struct xsk_ring_prod fq;
	struct xsk_ring_cons cq;

	struct pkt_tx_queue *pair;
	struct pollfd fds[1];
	int xsk_queue_idx;
};

struct tx_stats {
	uint64_t tx_pkts;
	uint64_t tx_bytes;
	uint64_t tx_dropped;
};

struct pkt_tx_queue {
	struct xsk_ring_prod tx;
	struct xsk_umem_info *umem;

	struct tx_stats stats;

	struct pkt_rx_queue *pair;
	int xsk_queue_idx;
};

struct pmd_internals {
	int if_index;
	char if_name[IFNAMSIZ];
	int start_queue_idx;
	int queue_cnt;
	int max_queue_cnt;
	int combined_queue_cnt;
	bool shared_umem;
	char prog_path[PATH_MAX];
	bool custom_prog_configured;

	struct rte_ether_addr eth_addr;

	struct pkt_rx_queue *rx_queues;
	struct pkt_tx_queue *tx_queues;
};

#define ETH_AF_XDP_IFACE_ARG			"iface"
#define ETH_AF_XDP_START_QUEUE_ARG		"start_queue"
#define ETH_AF_XDP_QUEUE_COUNT_ARG		"queue_count"
#define ETH_AF_XDP_SHARED_UMEM_ARG		"shared_umem"
#define ETH_AF_XDP_PROG_ARG			"xdp_prog"

static const char * const valid_arguments[] = {
	ETH_AF_XDP_IFACE_ARG,
	ETH_AF_XDP_START_QUEUE_ARG,
	ETH_AF_XDP_QUEUE_COUNT_ARG,
	ETH_AF_XDP_SHARED_UMEM_ARG,
	ETH_AF_XDP_PROG_ARG,
	NULL
};

static const struct rte_eth_link pmd_link = {
	.link_speed = ETH_SPEED_NUM_10G,
	.link_duplex = ETH_LINK_FULL_DUPLEX,
	.link_status = ETH_LINK_DOWN,
	.link_autoneg = ETH_LINK_AUTONEG
};

/* List which tracks PMDs to facilitate sharing UMEMs across them. */
struct internal_list {
	TAILQ_ENTRY(internal_list) next;
	struct rte_eth_dev *eth_dev;
};

TAILQ_HEAD(internal_list_head, internal_list);
static struct internal_list_head internal_list =
	TAILQ_HEAD_INITIALIZER(internal_list);

static pthread_mutex_t internal_list_lock = PTHREAD_MUTEX_INITIALIZER;

#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
static inline int
reserve_fill_queue_zc(struct xsk_umem_info *umem, uint16_t reserve_size,
		      struct rte_mbuf **bufs, struct xsk_ring_prod *fq)
{
	uint32_t idx;
	uint16_t i;

	if (unlikely(!xsk_ring_prod__reserve(fq, reserve_size, &idx))) {
		for (i = 0; i < reserve_size; i++)
			rte_pktmbuf_free(bufs[i]);
		AF_XDP_LOG(DEBUG, "Failed to reserve enough fq descs.\n");
		return -1;
	}

	for (i = 0; i < reserve_size; i++) {
		__u64 *fq_addr;
		uint64_t addr;

		fq_addr = xsk_ring_prod__fill_addr(fq, idx++);
		addr = (uint64_t)bufs[i] - (uint64_t)umem->buffer -
				umem->mb_pool->header_size;
		*fq_addr = addr;
	}

	xsk_ring_prod__submit(fq, reserve_size);

	return 0;
}
#else
static inline int
reserve_fill_queue_cp(struct xsk_umem_info *umem, uint16_t reserve_size,
		      struct rte_mbuf **bufs __rte_unused,
		      struct xsk_ring_prod *fq)
{
	void *addrs[reserve_size];
	uint32_t idx;
	uint16_t i;

	if (rte_ring_dequeue_bulk(umem->buf_ring, addrs, reserve_size, NULL)
		    != reserve_size) {
		AF_XDP_LOG(DEBUG, "Failed to get enough buffers for fq.\n");
		return -1;
	}

	if (unlikely(!xsk_ring_prod__reserve(fq, reserve_size, &idx))) {
		AF_XDP_LOG(DEBUG, "Failed to reserve enough fq descs.\n");
		rte_ring_enqueue_bulk(umem->buf_ring, addrs,
				reserve_size, NULL);
		return -1;
	}

	for (i = 0; i < reserve_size; i++) {
		__u64 *fq_addr;

		fq_addr = xsk_ring_prod__fill_addr(fq, idx++);
		*fq_addr = (uint64_t)addrs[i];
	}

	xsk_ring_prod__submit(fq, reserve_size);

	return 0;
}
#endif

static inline int
reserve_fill_queue(struct xsk_umem_info *umem, uint16_t reserve_size,
		   struct rte_mbuf **bufs, struct xsk_ring_prod *fq)
{
#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	return reserve_fill_queue_zc(umem, reserve_size, bufs, fq);
#else
	return reserve_fill_queue_cp(umem, reserve_size, bufs, fq);
#endif
}

#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
static uint16_t
af_xdp_rx_zc(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct pkt_rx_queue *rxq = queue;
	struct xsk_ring_cons *rx = &rxq->rx;
	struct xsk_ring_prod *fq = &rxq->fq;
	struct xsk_umem_info *umem = rxq->umem;
	uint32_t idx_rx = 0;
	unsigned long rx_bytes = 0;
	int rcvd, i;
	struct rte_mbuf *fq_bufs[ETH_AF_XDP_RX_BATCH_SIZE];

	/* allocate bufs for fill queue replenishment after rx */
	if (rte_pktmbuf_alloc_bulk(umem->mb_pool, fq_bufs, nb_pkts)) {
		AF_XDP_LOG(DEBUG,
			"Failed to get enough buffers for fq.\n");
		return 0;
	}

	rcvd = xsk_ring_cons__peek(rx, nb_pkts, &idx_rx);

	if (rcvd == 0) {
#if defined(XDP_USE_NEED_WAKEUP)
		if (xsk_ring_prod__needs_wakeup(fq))
			(void)poll(rxq->fds, 1, 1000);
#endif

		goto out;
	}

	for (i = 0; i < rcvd; i++) {
		const struct xdp_desc *desc;
		uint64_t addr;
		uint32_t len;
		uint64_t offset;

		desc = xsk_ring_cons__rx_desc(rx, idx_rx++);
		addr = desc->addr;
		len = desc->len;

		offset = xsk_umem__extract_offset(addr);
		addr = xsk_umem__extract_addr(addr);

		bufs[i] = (struct rte_mbuf *)
				xsk_umem__get_data(umem->buffer, addr +
					umem->mb_pool->header_size);
		bufs[i]->data_off = offset - sizeof(struct rte_mbuf) -
			rte_pktmbuf_priv_size(umem->mb_pool) -
			umem->mb_pool->header_size;

		rte_pktmbuf_pkt_len(bufs[i]) = len;
		rte_pktmbuf_data_len(bufs[i]) = len;
		rx_bytes += len;
	}

	xsk_ring_cons__release(rx, rcvd);

	(void)reserve_fill_queue(umem, rcvd, fq_bufs, fq);

	/* statistics */
	rxq->stats.rx_pkts += rcvd;
	rxq->stats.rx_bytes += rx_bytes;

out:
	if (rcvd != nb_pkts)
		rte_mempool_put_bulk(umem->mb_pool, (void **)&fq_bufs[rcvd],
				     nb_pkts - rcvd);

	return rcvd;
}
#else
static uint16_t
af_xdp_rx_cp(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct pkt_rx_queue *rxq = queue;
	struct xsk_ring_cons *rx = &rxq->rx;
	struct xsk_umem_info *umem = rxq->umem;
	struct xsk_ring_prod *fq = &rxq->fq;
	uint32_t idx_rx = 0;
	unsigned long rx_bytes = 0;
	int rcvd, i;
	uint32_t free_thresh = fq->size >> 1;
	struct rte_mbuf *mbufs[ETH_AF_XDP_RX_BATCH_SIZE];

	if (xsk_prod_nb_free(fq, free_thresh) >= free_thresh)
		(void)reserve_fill_queue(umem, ETH_AF_XDP_RX_BATCH_SIZE,
					 NULL, fq);

	if (unlikely(rte_pktmbuf_alloc_bulk(rxq->mb_pool, mbufs, nb_pkts) != 0))
		return 0;

	rcvd = xsk_ring_cons__peek(rx, nb_pkts, &idx_rx);
	if (rcvd == 0) {
#if defined(XDP_USE_NEED_WAKEUP)
		if (xsk_ring_prod__needs_wakeup(fq))
			(void)poll(rxq->fds, 1, 1000);
#endif

		goto out;
	}

	for (i = 0; i < rcvd; i++) {
		const struct xdp_desc *desc;
		uint64_t addr;
		uint32_t len;
		void *pkt;

		desc = xsk_ring_cons__rx_desc(rx, idx_rx++);
		addr = desc->addr;
		len = desc->len;
		pkt = xsk_umem__get_data(rxq->umem->mz->addr, addr);

		rte_memcpy(rte_pktmbuf_mtod(mbufs[i], void *), pkt, len);
		rte_ring_enqueue(umem->buf_ring, (void *)addr);
		rte_pktmbuf_pkt_len(mbufs[i]) = len;
		rte_pktmbuf_data_len(mbufs[i]) = len;
		rx_bytes += len;
		bufs[i] = mbufs[i];
	}

	xsk_ring_cons__release(rx, rcvd);

	/* statistics */
	rxq->stats.rx_pkts += rcvd;
	rxq->stats.rx_bytes += rx_bytes;

out:
	if (rcvd != nb_pkts)
		rte_mempool_put_bulk(rxq->mb_pool, (void **)&mbufs[rcvd],
				     nb_pkts - rcvd);

	return rcvd;
}
#endif

static uint16_t
eth_af_xdp_rx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	nb_pkts = RTE_MIN(nb_pkts, ETH_AF_XDP_RX_BATCH_SIZE);

#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	return af_xdp_rx_zc(queue, bufs, nb_pkts);
#else
	return af_xdp_rx_cp(queue, bufs, nb_pkts);
#endif
}

static void
pull_umem_cq(struct xsk_umem_info *umem, int size, struct xsk_ring_cons *cq)
{
	size_t i, n;
	uint32_t idx_cq = 0;

	n = xsk_ring_cons__peek(cq, size, &idx_cq);

	for (i = 0; i < n; i++) {
		uint64_t addr;
		addr = *xsk_ring_cons__comp_addr(cq, idx_cq++);
#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
		addr = xsk_umem__extract_addr(addr);
		rte_pktmbuf_free((struct rte_mbuf *)
					xsk_umem__get_data(umem->buffer,
					addr + umem->mb_pool->header_size));
#else
		rte_ring_enqueue(umem->buf_ring, (void *)addr);
#endif
	}

	xsk_ring_cons__release(cq, n);
}

static void
kick_tx(struct pkt_tx_queue *txq, struct xsk_ring_cons *cq)
{
	struct xsk_umem_info *umem = txq->umem;

	pull_umem_cq(umem, XSK_RING_CONS__DEFAULT_NUM_DESCS, cq);

#if defined(XDP_USE_NEED_WAKEUP)
	if (xsk_ring_prod__needs_wakeup(&txq->tx))
#endif
		while (send(xsk_socket__fd(txq->pair->xsk), NULL,
			    0, MSG_DONTWAIT) < 0) {
			/* some thing unexpected */
			if (errno != EBUSY && errno != EAGAIN && errno != EINTR)
				break;

			/* pull from completion queue to leave more space */
			if (errno == EAGAIN)
				pull_umem_cq(umem,
					     XSK_RING_CONS__DEFAULT_NUM_DESCS,
					     cq);
		}
}

#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
static uint16_t
af_xdp_tx_zc(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct pkt_tx_queue *txq = queue;
	struct xsk_umem_info *umem = txq->umem;
	struct rte_mbuf *mbuf;
	unsigned long tx_bytes = 0;
	int i;
	uint32_t idx_tx;
	uint16_t count = 0;
	struct xdp_desc *desc;
	uint64_t addr, offset;
	struct xsk_ring_cons *cq = &txq->pair->cq;
	uint32_t free_thresh = cq->size >> 1;

	if (xsk_cons_nb_avail(cq, free_thresh) >= free_thresh)
		pull_umem_cq(umem, XSK_RING_CONS__DEFAULT_NUM_DESCS, cq);

	for (i = 0; i < nb_pkts; i++) {
		mbuf = bufs[i];

		if (mbuf->pool == umem->mb_pool) {
			if (!xsk_ring_prod__reserve(&txq->tx, 1, &idx_tx)) {
				kick_tx(txq, cq);
				if (!xsk_ring_prod__reserve(&txq->tx, 1,
							    &idx_tx))
					goto out;
			}
			desc = xsk_ring_prod__tx_desc(&txq->tx, idx_tx);
			desc->len = mbuf->pkt_len;
			addr = (uint64_t)mbuf - (uint64_t)umem->buffer -
					umem->mb_pool->header_size;
			offset = rte_pktmbuf_mtod(mbuf, uint64_t) -
					(uint64_t)mbuf +
					umem->mb_pool->header_size;
			offset = offset << XSK_UNALIGNED_BUF_OFFSET_SHIFT;
			desc->addr = addr | offset;
			count++;
		} else {
			struct rte_mbuf *local_mbuf =
					rte_pktmbuf_alloc(umem->mb_pool);
			void *pkt;

			if (local_mbuf == NULL)
				goto out;

			if (!xsk_ring_prod__reserve(&txq->tx, 1, &idx_tx)) {
				rte_pktmbuf_free(local_mbuf);
				kick_tx(txq, cq);
				goto out;
			}

			desc = xsk_ring_prod__tx_desc(&txq->tx, idx_tx);
			desc->len = mbuf->pkt_len;

			addr = (uint64_t)local_mbuf - (uint64_t)umem->buffer -
					umem->mb_pool->header_size;
			offset = rte_pktmbuf_mtod(local_mbuf, uint64_t) -
					(uint64_t)local_mbuf +
					umem->mb_pool->header_size;
			pkt = xsk_umem__get_data(umem->buffer, addr + offset);
			offset = offset << XSK_UNALIGNED_BUF_OFFSET_SHIFT;
			desc->addr = addr | offset;
			rte_memcpy(pkt, rte_pktmbuf_mtod(mbuf, void *),
					desc->len);
			rte_pktmbuf_free(mbuf);
			count++;
		}

		tx_bytes += mbuf->pkt_len;
	}

	kick_tx(txq, cq);

out:
	xsk_ring_prod__submit(&txq->tx, count);

	txq->stats.tx_pkts += count;
	txq->stats.tx_bytes += tx_bytes;
	txq->stats.tx_dropped += nb_pkts - count;

	return count;
}
#else
static uint16_t
af_xdp_tx_cp(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct pkt_tx_queue *txq = queue;
	struct xsk_umem_info *umem = txq->umem;
	struct rte_mbuf *mbuf;
	void *addrs[ETH_AF_XDP_TX_BATCH_SIZE];
	unsigned long tx_bytes = 0;
	int i;
	uint32_t idx_tx;
	struct xsk_ring_cons *cq = &txq->pair->cq;

	nb_pkts = RTE_MIN(nb_pkts, ETH_AF_XDP_TX_BATCH_SIZE);

	pull_umem_cq(umem, nb_pkts, cq);

	nb_pkts = rte_ring_dequeue_bulk(umem->buf_ring, addrs,
					nb_pkts, NULL);
	if (nb_pkts == 0)
		return 0;

	if (xsk_ring_prod__reserve(&txq->tx, nb_pkts, &idx_tx) != nb_pkts) {
		kick_tx(txq, cq);
		rte_ring_enqueue_bulk(umem->buf_ring, addrs, nb_pkts, NULL);
		return 0;
	}

	for (i = 0; i < nb_pkts; i++) {
		struct xdp_desc *desc;
		void *pkt;

		desc = xsk_ring_prod__tx_desc(&txq->tx, idx_tx + i);
		mbuf = bufs[i];
		desc->len = mbuf->pkt_len;

		desc->addr = (uint64_t)addrs[i];
		pkt = xsk_umem__get_data(umem->mz->addr,
					 desc->addr);
		rte_memcpy(pkt, rte_pktmbuf_mtod(mbuf, void *), desc->len);
		tx_bytes += mbuf->pkt_len;
		rte_pktmbuf_free(mbuf);
	}

	xsk_ring_prod__submit(&txq->tx, nb_pkts);

	kick_tx(txq, cq);

	txq->stats.tx_pkts += nb_pkts;
	txq->stats.tx_bytes += tx_bytes;

	return nb_pkts;
}
#endif

static uint16_t
eth_af_xdp_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	return af_xdp_tx_zc(queue, bufs, nb_pkts);
#else
	return af_xdp_tx_cp(queue, bufs, nb_pkts);
#endif
}

static int
eth_dev_start(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = ETH_LINK_UP;

	return 0;
}

/* This function gets called when the current port gets stopped. */
static int
eth_dev_stop(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = ETH_LINK_DOWN;
	return 0;
}

/* Find ethdev in list */
static inline struct internal_list *
find_internal_resource(struct pmd_internals *port_int)
{
	int found = 0;
	struct internal_list *list = NULL;

	if (port_int == NULL)
		return NULL;

	pthread_mutex_lock(&internal_list_lock);

	TAILQ_FOREACH(list, &internal_list, next) {
		struct pmd_internals *list_int =
				list->eth_dev->data->dev_private;
		if (list_int == port_int) {
			found = 1;
			break;
		}
	}

	pthread_mutex_unlock(&internal_list_lock);

	if (!found)
		return NULL;

	return list;
}

/* Check if the netdev,qid context already exists */
static inline bool
ctx_exists(struct pkt_rx_queue *rxq, const char *ifname,
		struct pkt_rx_queue *list_rxq, const char *list_ifname)
{
	bool exists = false;

	if (rxq->xsk_queue_idx == list_rxq->xsk_queue_idx &&
			!strncmp(ifname, list_ifname, IFNAMSIZ)) {
		AF_XDP_LOG(ERR, "ctx %s,%i already exists, cannot share umem\n",
					ifname, rxq->xsk_queue_idx);
		exists = true;
	}

	return exists;
}

/* Get a pointer to an existing UMEM which overlays the rxq's mb_pool */
static inline int
get_shared_umem(struct pkt_rx_queue *rxq, const char *ifname,
			struct xsk_umem_info **umem)
{
	struct internal_list *list;
	struct pmd_internals *internals;
	int i = 0, ret = 0;
	struct rte_mempool *mb_pool = rxq->mb_pool;

	if (mb_pool == NULL)
		return ret;

	pthread_mutex_lock(&internal_list_lock);

	TAILQ_FOREACH(list, &internal_list, next) {
		internals = list->eth_dev->data->dev_private;
		for (i = 0; i < internals->queue_cnt; i++) {
			struct pkt_rx_queue *list_rxq =
						&internals->rx_queues[i];
			if (rxq == list_rxq)
				continue;
			if (mb_pool == internals->rx_queues[i].mb_pool) {
				if (ctx_exists(rxq, ifname, list_rxq,
						internals->if_name)) {
					ret = -1;
					goto out;
				}
				if (__atomic_load_n(
					&internals->rx_queues[i].umem->refcnt,
							__ATOMIC_ACQUIRE)) {
					*umem = internals->rx_queues[i].umem;
					goto out;
				}
			}
		}
	}

out:
	pthread_mutex_unlock(&internal_list_lock);

	return ret;
}

static int
eth_dev_configure(struct rte_eth_dev *dev)
{
	struct pmd_internals *internal = dev->data->dev_private;

	/* rx/tx must be paired */
	if (dev->data->nb_rx_queues != dev->data->nb_tx_queues)
		return -EINVAL;

	if (internal->shared_umem) {
		struct internal_list *list = NULL;
		const char *name = dev->device->name;

		/* Ensure PMD is not already inserted into the list */
		list = find_internal_resource(internal);
		if (list)
			return 0;

		list = rte_zmalloc_socket(name, sizeof(*list), 0,
					dev->device->numa_node);
		if (list == NULL)
			return -1;

		list->eth_dev = dev;
		pthread_mutex_lock(&internal_list_lock);
		TAILQ_INSERT_TAIL(&internal_list, list, next);
		pthread_mutex_unlock(&internal_list_lock);
	}

	return 0;
}

static int
eth_dev_info(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;

	dev_info->if_index = internals->if_index;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = ETH_FRAME_LEN;
	dev_info->max_rx_queues = internals->queue_cnt;
	dev_info->max_tx_queues = internals->queue_cnt;

	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	dev_info->max_mtu = getpagesize() -
				sizeof(struct rte_mempool_objhdr) -
				sizeof(struct rte_mbuf) -
				RTE_PKTMBUF_HEADROOM - XDP_PACKET_HEADROOM;
#else
	dev_info->max_mtu = ETH_AF_XDP_FRAME_SIZE - XDP_PACKET_HEADROOM;
#endif

	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = ETH_AF_XDP_DFLT_NUM_DESCS;
	dev_info->default_txportconf.ring_size = ETH_AF_XDP_DFLT_NUM_DESCS;

	return 0;
}

static int
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct xdp_statistics xdp_stats;
	struct pkt_rx_queue *rxq;
	struct pkt_tx_queue *txq;
	socklen_t optlen;
	int i, ret;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		optlen = sizeof(struct xdp_statistics);
		rxq = &internals->rx_queues[i];
		txq = rxq->pair;
		stats->q_ipackets[i] = rxq->stats.rx_pkts;
		stats->q_ibytes[i] = rxq->stats.rx_bytes;

		stats->q_opackets[i] = txq->stats.tx_pkts;
		stats->q_obytes[i] = txq->stats.tx_bytes;

		stats->ipackets += stats->q_ipackets[i];
		stats->ibytes += stats->q_ibytes[i];
		stats->imissed += rxq->stats.rx_dropped;
		stats->oerrors += txq->stats.tx_dropped;
		ret = getsockopt(xsk_socket__fd(rxq->xsk), SOL_XDP,
				XDP_STATISTICS, &xdp_stats, &optlen);
		if (ret != 0) {
			AF_XDP_LOG(ERR, "getsockopt() failed for XDP_STATISTICS.\n");
			return -1;
		}
		stats->imissed += xdp_stats.rx_dropped;

		stats->opackets += stats->q_opackets[i];
		stats->obytes += stats->q_obytes[i];
	}

	return 0;
}

static int
eth_stats_reset(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	int i;

	for (i = 0; i < internals->queue_cnt; i++) {
		memset(&internals->rx_queues[i].stats, 0,
					sizeof(struct rx_stats));
		memset(&internals->tx_queues[i].stats, 0,
					sizeof(struct tx_stats));
	}

	return 0;
}

static void
remove_xdp_program(struct pmd_internals *internals)
{
	uint32_t curr_prog_id = 0;

	if (bpf_get_link_xdp_id(internals->if_index, &curr_prog_id,
				XDP_FLAGS_UPDATE_IF_NOEXIST)) {
		AF_XDP_LOG(ERR, "bpf_get_link_xdp_id failed\n");
		return;
	}
	bpf_set_link_xdp_fd(internals->if_index, -1,
			XDP_FLAGS_UPDATE_IF_NOEXIST);
}

static void
xdp_umem_destroy(struct xsk_umem_info *umem)
{
#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	umem->mb_pool = NULL;
#else
	rte_memzone_free(umem->mz);
	umem->mz = NULL;

	rte_ring_free(umem->buf_ring);
	umem->buf_ring = NULL;
#endif

	rte_free(umem);
}

static int
eth_dev_close(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct pkt_rx_queue *rxq;
	int i;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	AF_XDP_LOG(INFO, "Closing AF_XDP ethdev on numa socket %u\n",
		rte_socket_id());

	for (i = 0; i < internals->queue_cnt; i++) {
		rxq = &internals->rx_queues[i];
		if (rxq->umem == NULL)
			break;
		xsk_socket__delete(rxq->xsk);

		if (__atomic_sub_fetch(&rxq->umem->refcnt, 1, __ATOMIC_ACQUIRE)
				== 0) {
			(void)xsk_umem__delete(rxq->umem->umem);
			xdp_umem_destroy(rxq->umem);
		}

		/* free pkt_tx_queue */
		rte_free(rxq->pair);
		rte_free(rxq);
	}

	/*
	 * MAC is not allocated dynamically, setting it to NULL would prevent
	 * from releasing it in rte_eth_dev_release_port.
	 */
	dev->data->mac_addrs = NULL;

	remove_xdp_program(internals);

	if (internals->shared_umem) {
		struct internal_list *list;

		/* Remove ethdev from list used to track and share UMEMs */
		list = find_internal_resource(internals);
		if (list) {
			pthread_mutex_lock(&internal_list_lock);
			TAILQ_REMOVE(&internal_list, list, next);
			pthread_mutex_unlock(&internal_list_lock);
			rte_free(list);
		}
	}

	return 0;
}

static void
eth_queue_release(void *q __rte_unused)
{
}

static int
eth_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused)
{
	return 0;
}

#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
static inline uintptr_t get_base_addr(struct rte_mempool *mp, uint64_t *align)
{
	struct rte_mempool_memhdr *memhdr;
	uintptr_t memhdr_addr, aligned_addr;

	memhdr = STAILQ_FIRST(&mp->mem_list);
	memhdr_addr = (uintptr_t)memhdr->addr;
	aligned_addr = memhdr_addr & ~(getpagesize() - 1);
	*align = memhdr_addr - aligned_addr;

	return aligned_addr;
}

static struct
xsk_umem_info *xdp_umem_configure(struct pmd_internals *internals,
				  struct pkt_rx_queue *rxq)
{
	struct xsk_umem_info *umem = NULL;
	int ret;
	struct xsk_umem_config usr_config = {
		.fill_size = ETH_AF_XDP_DFLT_NUM_DESCS * 2,
		.comp_size = ETH_AF_XDP_DFLT_NUM_DESCS,
		.flags = XDP_UMEM_UNALIGNED_CHUNK_FLAG};
	void *base_addr = NULL;
	struct rte_mempool *mb_pool = rxq->mb_pool;
	uint64_t umem_size, align = 0;

	if (internals->shared_umem) {
		if (get_shared_umem(rxq, internals->if_name, &umem) < 0)
			return NULL;

		if (umem != NULL &&
			__atomic_load_n(&umem->refcnt, __ATOMIC_ACQUIRE) <
					umem->max_xsks) {
			AF_XDP_LOG(INFO, "%s,qid%i sharing UMEM\n",
					internals->if_name, rxq->xsk_queue_idx);
			__atomic_fetch_add(&umem->refcnt, 1, __ATOMIC_ACQUIRE);
		}
	}

	if (umem == NULL) {
		usr_config.frame_size =
			rte_mempool_calc_obj_size(mb_pool->elt_size,
						  mb_pool->flags, NULL);
		usr_config.frame_headroom = mb_pool->header_size +
						sizeof(struct rte_mbuf) +
						rte_pktmbuf_priv_size(mb_pool) +
						RTE_PKTMBUF_HEADROOM;

		umem = rte_zmalloc_socket("umem", sizeof(*umem), 0,
					  rte_socket_id());
		if (umem == NULL) {
			AF_XDP_LOG(ERR, "Failed to allocate umem info");
			return NULL;
		}

		umem->mb_pool = mb_pool;
		base_addr = (void *)get_base_addr(mb_pool, &align);
		umem_size = (uint64_t)mb_pool->populated_size *
				(uint64_t)usr_config.frame_size +
				align;

		ret = xsk_umem__create(&umem->umem, base_addr, umem_size,
				&rxq->fq, &rxq->cq, &usr_config);
		if (ret) {
			AF_XDP_LOG(ERR, "Failed to create umem");
			goto err;
		}
		umem->buffer = base_addr;

		if (internals->shared_umem) {
			umem->max_xsks = mb_pool->populated_size /
						ETH_AF_XDP_NUM_BUFFERS;
			AF_XDP_LOG(INFO, "Max xsks for UMEM %s: %u\n",
						mb_pool->name, umem->max_xsks);
		}

		__atomic_store_n(&umem->refcnt, 1, __ATOMIC_RELEASE);
	}

#else
static struct
xsk_umem_info *xdp_umem_configure(struct pmd_internals *internals,
				  struct pkt_rx_queue *rxq)
{
	struct xsk_umem_info *umem;
	const struct rte_memzone *mz;
	struct xsk_umem_config usr_config = {
		.fill_size = ETH_AF_XDP_DFLT_NUM_DESCS,
		.comp_size = ETH_AF_XDP_DFLT_NUM_DESCS,
		.frame_size = ETH_AF_XDP_FRAME_SIZE,
		.frame_headroom = 0 };
	char ring_name[RTE_RING_NAMESIZE];
	char mz_name[RTE_MEMZONE_NAMESIZE];
	int ret;
	uint64_t i;

	umem = rte_zmalloc_socket("umem", sizeof(*umem), 0, rte_socket_id());
	if (umem == NULL) {
		AF_XDP_LOG(ERR, "Failed to allocate umem info");
		return NULL;
	}

	snprintf(ring_name, sizeof(ring_name), "af_xdp_ring_%s_%u",
		       internals->if_name, rxq->xsk_queue_idx);
	umem->buf_ring = rte_ring_create(ring_name,
					 ETH_AF_XDP_NUM_BUFFERS,
					 rte_socket_id(),
					 0x0);
	if (umem->buf_ring == NULL) {
		AF_XDP_LOG(ERR, "Failed to create rte_ring\n");
		goto err;
	}

	for (i = 0; i < ETH_AF_XDP_NUM_BUFFERS; i++)
		rte_ring_enqueue(umem->buf_ring,
				 (void *)(i * ETH_AF_XDP_FRAME_SIZE));

	snprintf(mz_name, sizeof(mz_name), "af_xdp_umem_%s_%u",
		       internals->if_name, rxq->xsk_queue_idx);
	mz = rte_memzone_reserve_aligned(mz_name,
			ETH_AF_XDP_NUM_BUFFERS * ETH_AF_XDP_FRAME_SIZE,
			rte_socket_id(), RTE_MEMZONE_IOVA_CONTIG,
			getpagesize());
	if (mz == NULL) {
		AF_XDP_LOG(ERR, "Failed to reserve memzone for af_xdp umem.\n");
		goto err;
	}

	ret = xsk_umem__create(&umem->umem, mz->addr,
			       ETH_AF_XDP_NUM_BUFFERS * ETH_AF_XDP_FRAME_SIZE,
			       &rxq->fq, &rxq->cq,
			       &usr_config);

	if (ret) {
		AF_XDP_LOG(ERR, "Failed to create umem");
		goto err;
	}
	umem->mz = mz;

#endif
	return umem;

err:
	xdp_umem_destroy(umem);
	return NULL;
}

static int
load_custom_xdp_prog(const char *prog_path, int if_index)
{
	int ret, prog_fd = -1;
	struct bpf_object *obj;
	struct bpf_map *map;

	ret = bpf_prog_load(prog_path, BPF_PROG_TYPE_XDP, &obj, &prog_fd);
	if (ret) {
		AF_XDP_LOG(ERR, "Failed to load program %s\n", prog_path);
		return ret;
	}

	/*
	 * The loaded program must provision for a map of xsks, such that some
	 * traffic can be redirected to userspace. When the xsk is created,
	 * libbpf inserts it into the map.
	 */
	map = bpf_object__find_map_by_name(obj, "xsks_map");
	if (!map) {
		AF_XDP_LOG(ERR, "Failed to find xsks_map in %s\n", prog_path);
		return -1;
	}

	/* Link the program with the given network device */
	ret = bpf_set_link_xdp_fd(if_index, prog_fd,
					XDP_FLAGS_UPDATE_IF_NOEXIST);
	if (ret) {
		AF_XDP_LOG(ERR, "Failed to set prog fd %d on interface\n",
				prog_fd);
		return -1;
	}

	AF_XDP_LOG(INFO, "Successfully loaded XDP program %s with fd %d\n",
				prog_path, prog_fd);

	return 0;
}

static int
xsk_configure(struct pmd_internals *internals, struct pkt_rx_queue *rxq,
	      int ring_size)
{
	struct xsk_socket_config cfg;
	struct pkt_tx_queue *txq = rxq->pair;
	int ret = 0;
	int reserve_size = ETH_AF_XDP_DFLT_NUM_DESCS;
	struct rte_mbuf *fq_bufs[reserve_size];

	rxq->umem = xdp_umem_configure(internals, rxq);
	if (rxq->umem == NULL)
		return -ENOMEM;
	txq->umem = rxq->umem;

	cfg.rx_size = ring_size;
	cfg.tx_size = ring_size;
	cfg.libbpf_flags = 0;
	cfg.xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST;
	cfg.bind_flags = 0;

#if defined(XDP_USE_NEED_WAKEUP)
	cfg.bind_flags |= XDP_USE_NEED_WAKEUP;
#endif

	if (strnlen(internals->prog_path, PATH_MAX) &&
				!internals->custom_prog_configured) {
		ret = load_custom_xdp_prog(internals->prog_path,
					   internals->if_index);
		if (ret) {
			AF_XDP_LOG(ERR, "Failed to load custom XDP program %s\n",
					internals->prog_path);
			goto err;
		}
		internals->custom_prog_configured = 1;
	}

	if (internals->shared_umem)
		ret = create_shared_socket(&rxq->xsk, internals->if_name,
				rxq->xsk_queue_idx, rxq->umem->umem, &rxq->rx,
				&txq->tx, &rxq->fq, &rxq->cq, &cfg);
	else
		ret = xsk_socket__create(&rxq->xsk, internals->if_name,
				rxq->xsk_queue_idx, rxq->umem->umem, &rxq->rx,
				&txq->tx, &cfg);

	if (ret) {
		AF_XDP_LOG(ERR, "Failed to create xsk socket.\n");
		goto err;
	}

#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	ret = rte_pktmbuf_alloc_bulk(rxq->umem->mb_pool, fq_bufs, reserve_size);
	if (ret) {
		AF_XDP_LOG(DEBUG, "Failed to get enough buffers for fq.\n");
		goto err;
	}
#endif
	ret = reserve_fill_queue(rxq->umem, reserve_size, fq_bufs, &rxq->fq);
	if (ret) {
		xsk_socket__delete(rxq->xsk);
		AF_XDP_LOG(ERR, "Failed to reserve fill queue.\n");
		goto err;
	}

	return 0;

err:
	if (__atomic_sub_fetch(&rxq->umem->refcnt, 1, __ATOMIC_ACQUIRE) == 0)
		xdp_umem_destroy(rxq->umem);

	return ret;
}

static int
eth_rx_queue_setup(struct rte_eth_dev *dev,
		   uint16_t rx_queue_id,
		   uint16_t nb_rx_desc,
		   unsigned int socket_id __rte_unused,
		   const struct rte_eth_rxconf *rx_conf __rte_unused,
		   struct rte_mempool *mb_pool)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct pkt_rx_queue *rxq;
	int ret;

	rxq = &internals->rx_queues[rx_queue_id];

	AF_XDP_LOG(INFO, "Set up rx queue, rx queue id: %d, xsk queue id: %d\n",
		   rx_queue_id, rxq->xsk_queue_idx);

#ifndef XDP_UMEM_UNALIGNED_CHUNK_FLAG
	uint32_t buf_size, data_size;

	/* Now get the space available for data in the mbuf */
	buf_size = rte_pktmbuf_data_room_size(mb_pool) -
		RTE_PKTMBUF_HEADROOM;
	data_size = ETH_AF_XDP_FRAME_SIZE;

	if (data_size > buf_size) {
		AF_XDP_LOG(ERR, "%s: %d bytes will not fit in mbuf (%d bytes)\n",
			dev->device->name, data_size, buf_size);
		ret = -ENOMEM;
		goto err;
	}
#endif

	rxq->mb_pool = mb_pool;

	if (xsk_configure(internals, rxq, nb_rx_desc)) {
		AF_XDP_LOG(ERR, "Failed to configure xdp socket\n");
		ret = -EINVAL;
		goto err;
	}

	rxq->fds[0].fd = xsk_socket__fd(rxq->xsk);
	rxq->fds[0].events = POLLIN;

	dev->data->rx_queues[rx_queue_id] = rxq;
	return 0;

err:
	return ret;
}

static int
eth_tx_queue_setup(struct rte_eth_dev *dev,
		   uint16_t tx_queue_id,
		   uint16_t nb_tx_desc __rte_unused,
		   unsigned int socket_id __rte_unused,
		   const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct pkt_tx_queue *txq;

	txq = &internals->tx_queues[tx_queue_id];

	dev->data->tx_queues[tx_queue_id] = txq;
	return 0;
}

static int
eth_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct pmd_internals *internals = dev->data->dev_private;
	struct ifreq ifr = { .ifr_mtu = mtu };
	int ret;
	int s;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return -EINVAL;

	strlcpy(ifr.ifr_name, internals->if_name, IFNAMSIZ);
	ret = ioctl(s, SIOCSIFMTU, &ifr);
	close(s);

	return (ret < 0) ? -errno : 0;
}

static int
eth_dev_change_flags(char *if_name, uint32_t flags, uint32_t mask)
{
	struct ifreq ifr;
	int ret = 0;
	int s;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return -errno;

	strlcpy(ifr.ifr_name, if_name, IFNAMSIZ);
	if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) {
		ret = -errno;
		goto out;
	}
	ifr.ifr_flags &= mask;
	ifr.ifr_flags |= flags;
	if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0) {
		ret = -errno;
		goto out;
	}
out:
	close(s);
	return ret;
}

static int
eth_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;

	return eth_dev_change_flags(internals->if_name, IFF_PROMISC, ~0);
}

static int
eth_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct pmd_internals *internals = dev->data->dev_private;

	return eth_dev_change_flags(internals->if_name, 0, ~IFF_PROMISC);
}

static const struct eth_dev_ops ops = {
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_close = eth_dev_close,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.mtu_set = eth_dev_mtu_set,
	.promiscuous_enable = eth_dev_promiscuous_enable,
	.promiscuous_disable = eth_dev_promiscuous_disable,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.rx_queue_release = eth_queue_release,
	.tx_queue_release = eth_queue_release,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
};

/** parse integer from integer argument */
static int
parse_integer_arg(const char *key __rte_unused,
		  const char *value, void *extra_args)
{
	int *i = (int *)extra_args;
	char *end;

	*i = strtol(value, &end, 10);
	if (*i < 0) {
		AF_XDP_LOG(ERR, "Argument has to be positive.\n");
		return -EINVAL;
	}

	return 0;
}

/** parse name argument */
static int
parse_name_arg(const char *key __rte_unused,
	       const char *value, void *extra_args)
{
	char *name = extra_args;

	if (strnlen(value, IFNAMSIZ) > IFNAMSIZ - 1) {
		AF_XDP_LOG(ERR, "Invalid name %s, should be less than %u bytes.\n",
			   value, IFNAMSIZ);
		return -EINVAL;
	}

	strlcpy(name, value, IFNAMSIZ);

	return 0;
}

/** parse xdp prog argument */
static int
parse_prog_arg(const char *key __rte_unused,
	       const char *value, void *extra_args)
{
	char *path = extra_args;

	if (strnlen(value, PATH_MAX) == PATH_MAX) {
		AF_XDP_LOG(ERR, "Invalid path %s, should be less than %u bytes.\n",
			   value, PATH_MAX);
		return -EINVAL;
	}

	if (access(value, F_OK) != 0) {
		AF_XDP_LOG(ERR, "Error accessing %s: %s\n",
			   value, strerror(errno));
		return -EINVAL;
	}

	strlcpy(path, value, PATH_MAX);

	return 0;
}

static int
xdp_get_channels_info(const char *if_name, int *max_queues,
				int *combined_queues)
{
	struct ethtool_channels channels;
	struct ifreq ifr;
	int fd, ret;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -1;

	channels.cmd = ETHTOOL_GCHANNELS;
	ifr.ifr_data = (void *)&channels;
	strlcpy(ifr.ifr_name, if_name, IFNAMSIZ);
	ret = ioctl(fd, SIOCETHTOOL, &ifr);
	if (ret) {
		if (errno == EOPNOTSUPP) {
			ret = 0;
		} else {
			ret = -errno;
			goto out;
		}
	}

	if (channels.max_combined == 0 || errno == EOPNOTSUPP) {
		/* If the device says it has no channels, then all traffic
		 * is sent to a single stream, so max queues = 1.
		 */
		*max_queues = 1;
		*combined_queues = 1;
	} else {
		*max_queues = channels.max_combined;
		*combined_queues = channels.combined_count;
	}

 out:
	close(fd);
	return ret;
}

static int
parse_parameters(struct rte_kvargs *kvlist, char *if_name, int *start_queue,
			int *queue_cnt, int *shared_umem, char *prog_path)
{
	int ret;

	ret = rte_kvargs_process(kvlist, ETH_AF_XDP_IFACE_ARG,
				 &parse_name_arg, if_name);
	if (ret < 0)
		goto free_kvlist;

	ret = rte_kvargs_process(kvlist, ETH_AF_XDP_START_QUEUE_ARG,
				 &parse_integer_arg, start_queue);
	if (ret < 0)
		goto free_kvlist;

	ret = rte_kvargs_process(kvlist, ETH_AF_XDP_QUEUE_COUNT_ARG,
				 &parse_integer_arg, queue_cnt);
	if (ret < 0 || *queue_cnt <= 0) {
		ret = -EINVAL;
		goto free_kvlist;
	}

	ret = rte_kvargs_process(kvlist, ETH_AF_XDP_SHARED_UMEM_ARG,
				&parse_integer_arg, shared_umem);
	if (ret < 0)
		goto free_kvlist;

	ret = rte_kvargs_process(kvlist, ETH_AF_XDP_PROG_ARG,
				 &parse_prog_arg, prog_path);
	if (ret < 0)
		goto free_kvlist;

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
get_iface_info(const char *if_name,
	       struct rte_ether_addr *eth_addr,
	       int *if_index)
{
	struct ifreq ifr;
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

	if (sock < 0)
		return -1;

	strlcpy(ifr.ifr_name, if_name, IFNAMSIZ);
	if (ioctl(sock, SIOCGIFINDEX, &ifr))
		goto error;

	*if_index = ifr.ifr_ifindex;

	if (ioctl(sock, SIOCGIFHWADDR, &ifr))
		goto error;

	rte_memcpy(eth_addr, ifr.ifr_hwaddr.sa_data, RTE_ETHER_ADDR_LEN);

	close(sock);
	return 0;

error:
	close(sock);
	return -1;
}

static struct rte_eth_dev *
init_internals(struct rte_vdev_device *dev, const char *if_name,
		int start_queue_idx, int queue_cnt, int shared_umem,
		const char *prog_path)
{
	const char *name = rte_vdev_device_name(dev);
	const unsigned int numa_node = dev->device.numa_node;
	struct pmd_internals *internals;
	struct rte_eth_dev *eth_dev;
	int ret;
	int i;

	internals = rte_zmalloc_socket(name, sizeof(*internals), 0, numa_node);
	if (internals == NULL)
		return NULL;

	internals->start_queue_idx = start_queue_idx;
	internals->queue_cnt = queue_cnt;
	strlcpy(internals->if_name, if_name, IFNAMSIZ);
	strlcpy(internals->prog_path, prog_path, PATH_MAX);
	internals->custom_prog_configured = 0;

#ifndef ETH_AF_XDP_SHARED_UMEM
	if (shared_umem) {
		AF_XDP_LOG(ERR, "Shared UMEM feature not available. "
				"Check kernel and libbpf version\n");
		goto err_free_internals;
	}
#endif
	internals->shared_umem = shared_umem;

	if (xdp_get_channels_info(if_name, &internals->max_queue_cnt,
				  &internals->combined_queue_cnt)) {
		AF_XDP_LOG(ERR, "Failed to get channel info of interface: %s\n",
				if_name);
		goto err_free_internals;
	}

	if (queue_cnt > internals->combined_queue_cnt) {
		AF_XDP_LOG(ERR, "Specified queue count %d is larger than combined queue count %d.\n",
				queue_cnt, internals->combined_queue_cnt);
		goto err_free_internals;
	}

	internals->rx_queues = rte_zmalloc_socket(NULL,
					sizeof(struct pkt_rx_queue) * queue_cnt,
					0, numa_node);
	if (internals->rx_queues == NULL) {
		AF_XDP_LOG(ERR, "Failed to allocate memory for rx queues.\n");
		goto err_free_internals;
	}

	internals->tx_queues = rte_zmalloc_socket(NULL,
					sizeof(struct pkt_tx_queue) * queue_cnt,
					0, numa_node);
	if (internals->tx_queues == NULL) {
		AF_XDP_LOG(ERR, "Failed to allocate memory for tx queues.\n");
		goto err_free_rx;
	}
	for (i = 0; i < queue_cnt; i++) {
		internals->tx_queues[i].pair = &internals->rx_queues[i];
		internals->rx_queues[i].pair = &internals->tx_queues[i];
		internals->rx_queues[i].xsk_queue_idx = start_queue_idx + i;
		internals->tx_queues[i].xsk_queue_idx = start_queue_idx + i;
	}

	ret = get_iface_info(if_name, &internals->eth_addr,
			     &internals->if_index);
	if (ret)
		goto err_free_tx;

	eth_dev = rte_eth_vdev_allocate(dev, 0);
	if (eth_dev == NULL)
		goto err_free_tx;

	eth_dev->data->dev_private = internals;
	eth_dev->data->dev_link = pmd_link;
	eth_dev->data->mac_addrs = &internals->eth_addr;
	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;
	eth_dev->dev_ops = &ops;
	eth_dev->rx_pkt_burst = eth_af_xdp_rx;
	eth_dev->tx_pkt_burst = eth_af_xdp_tx;

#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	AF_XDP_LOG(INFO, "Zero copy between umem and mbuf enabled.\n");
#endif

	return eth_dev;

err_free_tx:
	rte_free(internals->tx_queues);
err_free_rx:
	rte_free(internals->rx_queues);
err_free_internals:
	rte_free(internals);
	return NULL;
}

static int
rte_pmd_af_xdp_probe(struct rte_vdev_device *dev)
{
	struct rte_kvargs *kvlist;
	char if_name[IFNAMSIZ] = {'\0'};
	int xsk_start_queue_idx = ETH_AF_XDP_DFLT_START_QUEUE_IDX;
	int xsk_queue_cnt = ETH_AF_XDP_DFLT_QUEUE_COUNT;
	int shared_umem = 0;
	char prog_path[PATH_MAX] = {'\0'};
	struct rte_eth_dev *eth_dev = NULL;
	const char *name;

	AF_XDP_LOG(INFO, "Initializing pmd_af_xdp for %s\n",
		rte_vdev_device_name(dev));

	name = rte_vdev_device_name(dev);
	if (rte_eal_process_type() == RTE_PROC_SECONDARY &&
		strlen(rte_vdev_device_args(dev)) == 0) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (eth_dev == NULL) {
			AF_XDP_LOG(ERR, "Failed to probe %s\n", name);
			return -EINVAL;
		}
		eth_dev->dev_ops = &ops;
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	kvlist = rte_kvargs_parse(rte_vdev_device_args(dev), valid_arguments);
	if (kvlist == NULL) {
		AF_XDP_LOG(ERR, "Invalid kvargs key\n");
		return -EINVAL;
	}

	if (dev->device.numa_node == SOCKET_ID_ANY)
		dev->device.numa_node = rte_socket_id();

	if (parse_parameters(kvlist, if_name, &xsk_start_queue_idx,
			     &xsk_queue_cnt, &shared_umem, prog_path) < 0) {
		AF_XDP_LOG(ERR, "Invalid kvargs value\n");
		return -EINVAL;
	}

	if (strlen(if_name) == 0) {
		AF_XDP_LOG(ERR, "Network interface must be specified\n");
		return -EINVAL;
	}

	eth_dev = init_internals(dev, if_name, xsk_start_queue_idx,
					xsk_queue_cnt, shared_umem, prog_path);
	if (eth_dev == NULL) {
		AF_XDP_LOG(ERR, "Failed to init internals\n");
		return -1;
	}

	rte_eth_dev_probing_finish(eth_dev);

	return 0;
}

static int
rte_pmd_af_xdp_remove(struct rte_vdev_device *dev)
{
	struct rte_eth_dev *eth_dev = NULL;

	AF_XDP_LOG(INFO, "Removing AF_XDP ethdev on numa socket %u\n",
		rte_socket_id());

	if (dev == NULL)
		return -1;

	/* find the ethdev entry */
	eth_dev = rte_eth_dev_allocated(rte_vdev_device_name(dev));
	if (eth_dev == NULL)
		return 0;

	eth_dev_close(eth_dev);
	rte_eth_dev_release_port(eth_dev);


	return 0;
}

static struct rte_vdev_driver pmd_af_xdp_drv = {
	.probe = rte_pmd_af_xdp_probe,
	.remove = rte_pmd_af_xdp_remove,
};

RTE_PMD_REGISTER_VDEV(net_af_xdp, pmd_af_xdp_drv);
RTE_PMD_REGISTER_PARAM_STRING(net_af_xdp,
			      "iface=<string> "
			      "start_queue=<int> "
			      "queue_count=<int> "
			      "shared_umem=<int> "
			      "xdp_prog=<string> ");
