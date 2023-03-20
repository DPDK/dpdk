/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation.
 */
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <linux/if_xdp.h>
#include <linux/if_link.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include "af_xdp_deps.h"

#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <ethdev_vdev.h>
#include <rte_kvargs.h>
#include <bus_vdev_driver.h>
#include <rte_string_fns.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <dev_driver.h>
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
#include <rte_power_intrinsics.h>

#include "compat.h"
#include "eal_filesystem.h"

#ifndef SO_PREFER_BUSY_POLL
#define SO_PREFER_BUSY_POLL 69
#endif
#ifndef SO_BUSY_POLL_BUDGET
#define SO_BUSY_POLL_BUDGET 70
#endif


#ifndef SOL_XDP
#define SOL_XDP 283
#endif

#ifndef AF_XDP
#define AF_XDP 44
#endif

#ifndef PF_XDP
#define PF_XDP AF_XDP
#endif

RTE_LOG_REGISTER_DEFAULT(af_xdp_logtype, NOTICE);

#define AF_XDP_LOG(level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, af_xdp_logtype,	\
		"%s(): " fmt, __func__, ##args)

#define ETH_AF_XDP_FRAME_SIZE		2048
#define ETH_AF_XDP_NUM_BUFFERS		4096
#define ETH_AF_XDP_DFLT_NUM_DESCS	XSK_RING_CONS__DEFAULT_NUM_DESCS
#define ETH_AF_XDP_DFLT_START_QUEUE_IDX	0
#define ETH_AF_XDP_DFLT_QUEUE_COUNT	1
#define ETH_AF_XDP_DFLT_BUSY_BUDGET	64
#define ETH_AF_XDP_DFLT_BUSY_TIMEOUT	20

#define ETH_AF_XDP_RX_BATCH_SIZE	XSK_RING_CONS__DEFAULT_NUM_DESCS
#define ETH_AF_XDP_TX_BATCH_SIZE	XSK_RING_CONS__DEFAULT_NUM_DESCS

#define ETH_AF_XDP_ETH_OVERHEAD		(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN)

#define ETH_AF_XDP_MP_KEY "afxdp_mp_send_fds"

#define MAX_LONG_OPT_SZ			64
#define UDS_MAX_FD_NUM			2
#define UDS_MAX_CMD_LEN			64
#define UDS_MAX_CMD_RESP		128
#define UDS_XSK_MAP_FD_MSG		"/xsk_map_fd"
#define UDS_SOCK			"/tmp/afxdp.sock"
#define UDS_CONNECT_MSG			"/connect"
#define UDS_HOST_OK_MSG			"/host_ok"
#define UDS_HOST_NAK_MSG		"/host_nak"
#define UDS_VERSION_MSG			"/version"
#define UDS_XSK_MAP_FD_MSG		"/xsk_map_fd"
#define UDS_XSK_SOCKET_MSG		"/xsk_socket"
#define UDS_FD_ACK_MSG			"/fd_ack"
#define UDS_FD_NAK_MSG			"/fd_nak"
#define UDS_FIN_MSG			"/fin"
#define UDS_FIN_ACK_MSG			"/fin_ack"

static int afxdp_dev_count;

/* Message header to synchronize fds via IPC */
struct ipc_hdr {
	char port_name[RTE_DEV_NAME_MAX_LEN];
	/* The file descriptors are in the dedicated part
	 * of the Unix message to be translated by the kernel.
	 */
};

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
	int busy_budget;
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
	bool force_copy;
	bool use_cni;
	struct bpf_map *map;

	struct rte_ether_addr eth_addr;

	struct pkt_rx_queue *rx_queues;
	struct pkt_tx_queue *tx_queues;
};

struct pmd_process_private {
	int rxq_xsk_fds[RTE_MAX_QUEUES_PER_PORT];
};

#define ETH_AF_XDP_IFACE_ARG			"iface"
#define ETH_AF_XDP_START_QUEUE_ARG		"start_queue"
#define ETH_AF_XDP_QUEUE_COUNT_ARG		"queue_count"
#define ETH_AF_XDP_SHARED_UMEM_ARG		"shared_umem"
#define ETH_AF_XDP_PROG_ARG			"xdp_prog"
#define ETH_AF_XDP_BUDGET_ARG			"busy_budget"
#define ETH_AF_XDP_FORCE_COPY_ARG		"force_copy"
#define ETH_AF_XDP_USE_CNI_ARG			"use_cni"

static const char * const valid_arguments[] = {
	ETH_AF_XDP_IFACE_ARG,
	ETH_AF_XDP_START_QUEUE_ARG,
	ETH_AF_XDP_QUEUE_COUNT_ARG,
	ETH_AF_XDP_SHARED_UMEM_ARG,
	ETH_AF_XDP_PROG_ARG,
	ETH_AF_XDP_BUDGET_ARG,
	ETH_AF_XDP_FORCE_COPY_ARG,
	ETH_AF_XDP_USE_CNI_ARG,
	NULL
};

static const struct rte_eth_link pmd_link = {
	.link_speed = RTE_ETH_SPEED_NUM_10G,
	.link_duplex = RTE_ETH_LINK_FULL_DUPLEX,
	.link_status = RTE_ETH_LINK_DOWN,
	.link_autoneg = RTE_ETH_LINK_AUTONEG
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
	int i;
	struct rte_mbuf *fq_bufs[ETH_AF_XDP_RX_BATCH_SIZE];

	nb_pkts = xsk_ring_cons__peek(rx, nb_pkts, &idx_rx);

	if (nb_pkts == 0) {
		/* we can assume a kernel >= 5.11 is in use if busy polling is
		 * enabled and thus we can safely use the recvfrom() syscall
		 * which is only supported for AF_XDP sockets in kernels >=
		 * 5.11.
		 */
		if (rxq->busy_budget) {
			(void)recvfrom(xsk_socket__fd(rxq->xsk), NULL, 0,
				       MSG_DONTWAIT, NULL, NULL);
		} else if (xsk_ring_prod__needs_wakeup(fq)) {
			(void)poll(&rxq->fds[0], 1, 1000);
		}

		return 0;
	}

	/* allocate bufs for fill queue replenishment after rx */
	if (rte_pktmbuf_alloc_bulk(umem->mb_pool, fq_bufs, nb_pkts)) {
		AF_XDP_LOG(DEBUG,
			"Failed to get enough buffers for fq.\n");
		/* rollback cached_cons which is added by
		 * xsk_ring_cons__peek
		 */
		rx->cached_cons -= nb_pkts;
		return 0;
	}

	for (i = 0; i < nb_pkts; i++) {
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

	xsk_ring_cons__release(rx, nb_pkts);
	(void)reserve_fill_queue(umem, nb_pkts, fq_bufs, fq);

	/* statistics */
	rxq->stats.rx_pkts += nb_pkts;
	rxq->stats.rx_bytes += rx_bytes;

	return nb_pkts;
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
	int i;
	uint32_t free_thresh = fq->size >> 1;
	struct rte_mbuf *mbufs[ETH_AF_XDP_RX_BATCH_SIZE];

	if (xsk_prod_nb_free(fq, free_thresh) >= free_thresh)
		(void)reserve_fill_queue(umem, nb_pkts, NULL, fq);

	nb_pkts = xsk_ring_cons__peek(rx, nb_pkts, &idx_rx);
	if (nb_pkts == 0) {
#if defined(XDP_USE_NEED_WAKEUP)
		if (xsk_ring_prod__needs_wakeup(fq))
			(void)poll(rxq->fds, 1, 1000);
#endif
		return 0;
	}

	if (unlikely(rte_pktmbuf_alloc_bulk(rxq->mb_pool, mbufs, nb_pkts))) {
		/* rollback cached_cons which is added by
		 * xsk_ring_cons__peek
		 */
		rx->cached_cons -= nb_pkts;
		return 0;
	}

	for (i = 0; i < nb_pkts; i++) {
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

	xsk_ring_cons__release(rx, nb_pkts);

	/* statistics */
	rxq->stats.rx_pkts += nb_pkts;
	rxq->stats.rx_bytes += rx_bytes;

	return nb_pkts;
}
#endif

static uint16_t
af_xdp_rx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	return af_xdp_rx_zc(queue, bufs, nb_pkts);
#else
	return af_xdp_rx_cp(queue, bufs, nb_pkts);
#endif
}

static uint16_t
eth_af_xdp_rx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	uint16_t nb_rx;

	if (likely(nb_pkts <= ETH_AF_XDP_RX_BATCH_SIZE))
		return af_xdp_rx(queue, bufs, nb_pkts);

	/* Split larger batch into smaller batches of size
	 * ETH_AF_XDP_RX_BATCH_SIZE or less.
	 */
	nb_rx = 0;
	while (nb_pkts) {
		uint16_t ret, n;

		n = (uint16_t)RTE_MIN(nb_pkts, ETH_AF_XDP_RX_BATCH_SIZE);
		ret = af_xdp_rx(queue, &bufs[nb_rx], n);
		nb_rx = (uint16_t)(nb_rx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < n)
			break;
	}

	return nb_rx;
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

	if (tx_syscall_needed(&txq->tx))
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

out:
	xsk_ring_prod__submit(&txq->tx, count);
	kick_tx(txq, cq);

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

static uint16_t
af_xdp_tx_cp_batch(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	uint16_t nb_tx;

	if (likely(nb_pkts <= ETH_AF_XDP_TX_BATCH_SIZE))
		return af_xdp_tx_cp(queue, bufs, nb_pkts);

	nb_tx = 0;
	while (nb_pkts) {
		uint16_t ret, n;

		/* Split larger batch into smaller batches of size
		 * ETH_AF_XDP_TX_BATCH_SIZE or less.
		 */
		n = (uint16_t)RTE_MIN(nb_pkts, ETH_AF_XDP_TX_BATCH_SIZE);
		ret = af_xdp_tx_cp(queue, &bufs[nb_tx], n);
		nb_tx = (uint16_t)(nb_tx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < n)
			break;
	}

	return nb_tx;
}
#endif

static uint16_t
eth_af_xdp_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	return af_xdp_tx_zc(queue, bufs, nb_pkts);
#else
	return af_xdp_tx_cp_batch(queue, bufs, nb_pkts);
#endif
}

static int
eth_dev_start(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = RTE_ETH_LINK_UP;

	return 0;
}

/* This function gets called when the current port gets stopped. */
static int
eth_dev_stop(struct rte_eth_dev *dev)
{
	dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;
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

#define CLB_VAL_IDX 0
static int
eth_monitor_callback(const uint64_t value,
		const uint64_t opaque[RTE_POWER_MONITOR_OPAQUE_SZ])
{
	const uint64_t v = opaque[CLB_VAL_IDX];
	const uint64_t m = (uint32_t)~0;

	/* if the value has changed, abort entering power optimized state */
	return (value & m) == v ? 0 : -1;
}

static int
eth_get_monitor_addr(void *rx_queue, struct rte_power_monitor_cond *pmc)
{
	struct pkt_rx_queue *rxq = rx_queue;
	unsigned int *prod = rxq->rx.producer;
	const uint32_t cur_val = rxq->rx.cached_prod; /* use cached value */

	/* watch for changes in producer ring */
	pmc->addr = (void *)prod;

	/* store current value */
	pmc->opaque[CLB_VAL_IDX] = cur_val;
	pmc->fn = eth_monitor_callback;

	/* AF_XDP producer ring index is 32-bit */
	pmc->size = sizeof(uint32_t);

	return 0;
}

static int
eth_dev_info(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;

	dev_info->if_index = internals->if_index;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_queues = internals->queue_cnt;
	dev_info->max_tx_queues = internals->queue_cnt;

	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	dev_info->max_rx_pktlen = getpagesize() -
				  sizeof(struct rte_mempool_objhdr) -
				  sizeof(struct rte_mbuf) -
				  RTE_PKTMBUF_HEADROOM - XDP_PACKET_HEADROOM;
#else
	dev_info->max_rx_pktlen = ETH_AF_XDP_FRAME_SIZE - XDP_PACKET_HEADROOM;
#endif
	dev_info->max_mtu = dev_info->max_rx_pktlen - ETH_AF_XDP_ETH_OVERHEAD;

	dev_info->default_rxportconf.burst_size = ETH_AF_XDP_DFLT_BUSY_BUDGET;
	dev_info->default_txportconf.burst_size = ETH_AF_XDP_DFLT_BUSY_BUDGET;
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
	struct pmd_process_private *process_private = dev->process_private;
	struct xdp_statistics xdp_stats;
	struct pkt_rx_queue *rxq;
	struct pkt_tx_queue *txq;
	socklen_t optlen;
	int i, ret, fd;

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
		fd = process_private->rxq_xsk_fds[i];
		ret = fd >= 0 ? getsockopt(fd, SOL_XDP, XDP_STATISTICS,
					   &xdp_stats, &optlen) : -1;
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

#ifdef RTE_NET_AF_XDP_LIBBPF_XDP_ATTACH

static int link_xdp_prog_with_dev(int ifindex, int fd, __u32 flags)
{
	return bpf_xdp_attach(ifindex, fd, flags, NULL);
}

static int
remove_xdp_program(struct pmd_internals *internals)
{
	uint32_t curr_prog_id = 0;
	int ret;

	ret = bpf_xdp_query_id(internals->if_index, XDP_FLAGS_UPDATE_IF_NOEXIST,
			       &curr_prog_id);
	if (ret != 0) {
		AF_XDP_LOG(ERR, "bpf_xdp_query_id failed\n");
		return ret;
	}

	ret = bpf_xdp_detach(internals->if_index, XDP_FLAGS_UPDATE_IF_NOEXIST,
			     NULL);
	if (ret != 0)
		AF_XDP_LOG(ERR, "bpf_xdp_detach failed\n");
	return ret;
}

#else

static int link_xdp_prog_with_dev(int ifindex, int fd, __u32 flags)
{
	return bpf_set_link_xdp_fd(ifindex, fd, flags);
}

static int
remove_xdp_program(struct pmd_internals *internals)
{
	uint32_t curr_prog_id = 0;
	int ret;

	ret = bpf_get_link_xdp_id(internals->if_index, &curr_prog_id,
				  XDP_FLAGS_UPDATE_IF_NOEXIST);
	if (ret != 0) {
		AF_XDP_LOG(ERR, "bpf_get_link_xdp_id failed\n");
		return ret;
	}

	ret = bpf_set_link_xdp_fd(internals->if_index, -1,
				  XDP_FLAGS_UPDATE_IF_NOEXIST);
	if (ret != 0)
		AF_XDP_LOG(ERR, "bpf_set_link_xdp_fd failed\n");
	return ret;
}

#endif

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
		goto out;

	AF_XDP_LOG(INFO, "Closing AF_XDP ethdev on numa socket %u\n",
		rte_socket_id());

	for (i = 0; i < internals->queue_cnt; i++) {
		rxq = &internals->rx_queues[i];
		if (rxq->umem == NULL)
			break;
		xsk_socket__delete(rxq->xsk);

		if (__atomic_fetch_sub(&rxq->umem->refcnt, 1, __ATOMIC_ACQUIRE) - 1
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

	if (remove_xdp_program(internals) != 0)
		AF_XDP_LOG(ERR, "Error while removing XDP program.\n");

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

out:
	rte_free(dev->process_private);

	return 0;
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
				if (__atomic_load_n(&internals->rx_queues[i].umem->refcnt,
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
			AF_XDP_LOG(ERR, "Failed to allocate umem info\n");
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
			AF_XDP_LOG(ERR, "Failed to create umem [%d]: [%s]\n",
				   errno, strerror(errno));
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

	return umem;

err:
	xdp_umem_destroy(umem);
	return NULL;
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
		AF_XDP_LOG(ERR, "Failed to allocate umem info\n");
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
		AF_XDP_LOG(ERR, "Failed to create umem\n");
		goto err;
	}
	umem->mz = mz;

	return umem;

err:
	xdp_umem_destroy(umem);
	return NULL;
}
#endif

static int
load_custom_xdp_prog(const char *prog_path, int if_index, struct bpf_map **map)
{
	int ret, prog_fd;
	struct bpf_object *obj;

	prog_fd = load_program(prog_path, &obj);
	if (prog_fd < 0) {
		AF_XDP_LOG(ERR, "Failed to load program %s\n", prog_path);
		return -1;
	}

	/*
	 * The loaded program must provision for a map of xsks, such that some
	 * traffic can be redirected to userspace.
	 */
	*map = bpf_object__find_map_by_name(obj, "xsks_map");
	if (!*map) {
		AF_XDP_LOG(ERR, "Failed to find xsks_map in %s\n", prog_path);
		return -1;
	}

	/* Link the program with the given network device */
	ret = link_xdp_prog_with_dev(if_index, prog_fd,
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

/* Detect support for busy polling through setsockopt(). */
static int
configure_preferred_busy_poll(struct pkt_rx_queue *rxq)
{
	int sock_opt = 1;
	int fd = xsk_socket__fd(rxq->xsk);
	int ret = 0;

	ret = setsockopt(fd, SOL_SOCKET, SO_PREFER_BUSY_POLL,
			(void *)&sock_opt, sizeof(sock_opt));
	if (ret < 0) {
		AF_XDP_LOG(DEBUG, "Failed to set SO_PREFER_BUSY_POLL\n");
		goto err_prefer;
	}

	sock_opt = ETH_AF_XDP_DFLT_BUSY_TIMEOUT;
	ret = setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, (void *)&sock_opt,
			sizeof(sock_opt));
	if (ret < 0) {
		AF_XDP_LOG(DEBUG, "Failed to set SO_BUSY_POLL\n");
		goto err_timeout;
	}

	sock_opt = rxq->busy_budget;
	ret = setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL_BUDGET,
			(void *)&sock_opt, sizeof(sock_opt));
	if (ret < 0) {
		AF_XDP_LOG(DEBUG, "Failed to set SO_BUSY_POLL_BUDGET\n");
	} else {
		AF_XDP_LOG(INFO, "Busy polling budget set to: %u\n",
					rxq->busy_budget);
		return 0;
	}

	/* setsockopt failure - attempt to restore xsk to default state and
	 * proceed without busy polling support.
	 */
	sock_opt = 0;
	ret = setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, (void *)&sock_opt,
			sizeof(sock_opt));
	if (ret < 0) {
		AF_XDP_LOG(ERR, "Failed to unset SO_BUSY_POLL\n");
		return -1;
	}

err_timeout:
	sock_opt = 0;
	ret = setsockopt(fd, SOL_SOCKET, SO_PREFER_BUSY_POLL,
			(void *)&sock_opt, sizeof(sock_opt));
	if (ret < 0) {
		AF_XDP_LOG(ERR, "Failed to unset SO_PREFER_BUSY_POLL\n");
		return -1;
	}

err_prefer:
	rxq->busy_budget = 0;
	return 0;
}

static int
init_uds_sock(struct sockaddr_un *server)
{
	int sock;

	sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sock < 0) {
		AF_XDP_LOG(ERR, "Failed to opening stream socket\n");
		return -1;
	}

	server->sun_family = AF_UNIX;
	strlcpy(server->sun_path, UDS_SOCK, sizeof(server->sun_path));

	if (connect(sock, (struct sockaddr *)server, sizeof(struct sockaddr_un)) < 0) {
		close(sock);
		AF_XDP_LOG(ERR, "Error connecting stream socket errno = [%d]: [%s]\n",
			   errno, strerror(errno));
		return -1;
	}

	return sock;
}

struct msg_internal {
	char response[UDS_MAX_CMD_RESP];
	int len_param;
	int num_fds;
	int fds[UDS_MAX_FD_NUM];
};

static int
send_msg(int sock, char *request, int *fd)
{
	int snd;
	struct iovec iov;
	struct msghdr msgh;
	struct cmsghdr *cmsg;
	struct sockaddr_un dst;
	char control[CMSG_SPACE(sizeof(*fd))];

	memset(&dst, 0, sizeof(dst));
	dst.sun_family = AF_UNIX;
	strlcpy(dst.sun_path, UDS_SOCK, sizeof(dst.sun_path));

	/* Initialize message header structure */
	memset(&msgh, 0, sizeof(msgh));
	memset(control, 0, sizeof(control));
	iov.iov_base = request;
	iov.iov_len = strlen(request);

	msgh.msg_name = &dst;
	msgh.msg_namelen = sizeof(dst);
	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = control;
	msgh.msg_controllen = sizeof(control);

	/* Translate the FD. */
	cmsg = CMSG_FIRSTHDR(&msgh);
	cmsg->cmsg_len = CMSG_LEN(sizeof(*fd));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	memcpy(CMSG_DATA(cmsg), fd, sizeof(*fd));

	/* Send the request message. */
	do {
		snd = sendmsg(sock, &msgh, 0);
	} while (snd < 0 && errno == EINTR);

	return snd;
}

static int
read_msg(int sock, char *response, struct sockaddr_un *s, int *fd)
{
	int msglen;
	struct msghdr msgh;
	struct iovec iov;
	char control[CMSG_SPACE(sizeof(*fd))];
	struct cmsghdr *cmsg;

	/* Initialize message header structure */
	memset(&msgh, 0, sizeof(msgh));
	iov.iov_base = response;
	iov.iov_len = UDS_MAX_CMD_RESP;

	msgh.msg_name = s;
	msgh.msg_namelen = sizeof(*s);
	msgh.msg_iov = &iov;
	msgh.msg_iovlen = 1;
	msgh.msg_control = control;
	msgh.msg_controllen = sizeof(control);

	msglen = recvmsg(sock, &msgh, 0);

	/* zero length message means socket was closed */
	if (msglen == 0)
		return 0;

	if (msglen < 0) {
		AF_XDP_LOG(ERR, "recvmsg failed, %s\n", strerror(errno));
		return -1;
	}

	/* read auxiliary FDs if any */
	for (cmsg = CMSG_FIRSTHDR(&msgh); cmsg != NULL;
			cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
				cmsg->cmsg_type == SCM_RIGHTS) {
			memcpy(fd, CMSG_DATA(cmsg), sizeof(*fd));
			break;
		}
	}

	response[msglen] = '\0';
	return msglen;
}

static int
make_request_cni(int sock, struct sockaddr_un *server, char *request,
		 int *req_fd, char *response, int *out_fd)
{
	int rval;

	AF_XDP_LOG(DEBUG, "Request: [%s]\n", request);

	/* if no file descriptor to send then directly write to socket.
	 * else use sendmsg() to send the file descriptor.
	 */
	if (req_fd == NULL)
		rval = write(sock, request, strlen(request));
	else
		rval = send_msg(sock, request, req_fd);

	if (rval < 0) {
		AF_XDP_LOG(ERR, "Write error %s\n", strerror(errno));
		return -1;
	}

	rval = read_msg(sock, response, server, out_fd);
	if (rval <= 0) {
		AF_XDP_LOG(ERR, "Read error %d\n", rval);
		return -1;
	}
	AF_XDP_LOG(DEBUG, "Response: [%s]\n", request);

	return 0;
}

static int
check_response(char *response, char *exp_resp, long size)
{
	return strncmp(response, exp_resp, size);
}

static int
get_cni_fd(char *if_name)
{
	char request[UDS_MAX_CMD_LEN], response[UDS_MAX_CMD_RESP];
	char hostname[MAX_LONG_OPT_SZ], exp_resp[UDS_MAX_CMD_RESP];
	struct sockaddr_un server;
	int xsk_map_fd = -1, out_fd = 0;
	int sock, err;

	err = gethostname(hostname, MAX_LONG_OPT_SZ - 1);
	if (err)
		return -1;

	memset(&server, 0, sizeof(server));
	sock = init_uds_sock(&server);
	if (sock < 0)
		return -1;

	/* Initiates handshake to CNI send: /connect,hostname */
	snprintf(request, sizeof(request), "%s,%s", UDS_CONNECT_MSG, hostname);
	memset(response, 0, sizeof(response));
	if (make_request_cni(sock, &server, request, NULL, response, &out_fd) < 0) {
		AF_XDP_LOG(ERR, "Error in processing cmd [%s]\n", request);
		goto err_close;
	}

	/* Expect /host_ok */
	strlcpy(exp_resp, UDS_HOST_OK_MSG, UDS_MAX_CMD_LEN);
	if (check_response(response, exp_resp, strlen(exp_resp)) < 0) {
		AF_XDP_LOG(ERR, "Unexpected response [%s]\n", response);
		goto err_close;
	}
	/* Request for "/version" */
	strlcpy(request, UDS_VERSION_MSG, UDS_MAX_CMD_LEN);
	memset(response, 0, sizeof(response));
	if (make_request_cni(sock, &server, request, NULL, response, &out_fd) < 0) {
		AF_XDP_LOG(ERR, "Error in processing cmd [%s]\n", request);
		goto err_close;
	}

	/* Request for file descriptor for netdev name*/
	snprintf(request, sizeof(request), "%s,%s", UDS_XSK_MAP_FD_MSG, if_name);
	memset(response, 0, sizeof(response));
	if (make_request_cni(sock, &server, request, NULL, response, &out_fd) < 0) {
		AF_XDP_LOG(ERR, "Error in processing cmd [%s]\n", request);
		goto err_close;
	}

	if (out_fd < 0) {
		AF_XDP_LOG(ERR, "Error in processing cmd [%s]\n", request);
		goto err_close;
	}

	xsk_map_fd = out_fd;

	/* Expect fd_ack with file descriptor */
	strlcpy(exp_resp, UDS_FD_ACK_MSG, UDS_MAX_CMD_LEN);
	if (check_response(response, exp_resp, strlen(exp_resp)) < 0) {
		AF_XDP_LOG(ERR, "Unexpected response [%s]\n", response);
		goto err_close;
	}

	/* Initiate close connection */
	strlcpy(request, UDS_FIN_MSG, UDS_MAX_CMD_LEN);
	memset(response, 0, sizeof(response));
	if (make_request_cni(sock, &server, request, NULL, response, &out_fd) < 0) {
		AF_XDP_LOG(ERR, "Error in processing cmd [%s]\n", request);
		goto err_close;
	}

	/* Connection close */
	strlcpy(exp_resp, UDS_FIN_ACK_MSG, UDS_MAX_CMD_LEN);
	if (check_response(response, exp_resp, strlen(exp_resp)) < 0) {
		AF_XDP_LOG(ERR, "Unexpected response [%s]\n", response);
		goto err_close;
	}
	close(sock);

	return xsk_map_fd;

err_close:
	close(sock);
	return -1;
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
	bool reserve_before;

	rxq->umem = xdp_umem_configure(internals, rxq);
	if (rxq->umem == NULL)
		return -ENOMEM;
	txq->umem = rxq->umem;
	reserve_before = __atomic_load_n(&rxq->umem->refcnt, __ATOMIC_ACQUIRE) <= 1;

#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	ret = rte_pktmbuf_alloc_bulk(rxq->umem->mb_pool, fq_bufs, reserve_size);
	if (ret) {
		AF_XDP_LOG(DEBUG, "Failed to get enough buffers for fq.\n");
		goto out_umem;
	}
#endif

	/* reserve fill queue of queues not (yet) sharing UMEM */
	if (reserve_before) {
		ret = reserve_fill_queue(rxq->umem, reserve_size, fq_bufs, &rxq->fq);
		if (ret) {
			AF_XDP_LOG(ERR, "Failed to reserve fill queue.\n");
			goto out_umem;
		}
	}

	cfg.rx_size = ring_size;
	cfg.tx_size = ring_size;
	cfg.libbpf_flags = 0;
	cfg.xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST;
	cfg.bind_flags = 0;

	/* Force AF_XDP socket into copy mode when users want it */
	if (internals->force_copy)
		cfg.bind_flags |= XDP_COPY;

#if defined(XDP_USE_NEED_WAKEUP)
	cfg.bind_flags |= XDP_USE_NEED_WAKEUP;
#endif

	/* Disable libbpf from loading XDP program */
	if (internals->use_cni)
		cfg.libbpf_flags |= XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD;

	if (strnlen(internals->prog_path, PATH_MAX)) {
		if (!internals->custom_prog_configured) {
			ret = load_custom_xdp_prog(internals->prog_path,
							internals->if_index,
							&internals->map);
			if (ret) {
				AF_XDP_LOG(ERR, "Failed to load custom XDP program %s\n",
						internals->prog_path);
				goto out_umem;
			}
			internals->custom_prog_configured = 1;
		}
		cfg.libbpf_flags |= XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD;
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
		goto out_umem;
	}

	if (!reserve_before) {
		/* reserve fill queue of queues sharing UMEM */
		ret = reserve_fill_queue(rxq->umem, reserve_size, fq_bufs, &rxq->fq);
		if (ret) {
			AF_XDP_LOG(ERR, "Failed to reserve fill queue.\n");
			goto out_xsk;
		}
	}

	/* insert the xsk into the xsks_map */
	if (internals->custom_prog_configured) {
		int err, fd;

		fd = xsk_socket__fd(rxq->xsk);
		err = bpf_map_update_elem(bpf_map__fd(internals->map),
					  &rxq->xsk_queue_idx, &fd, 0);
		if (err) {
			AF_XDP_LOG(ERR, "Failed to insert xsk in map.\n");
			goto out_xsk;
		}
	}

	if (internals->use_cni) {
		int err, fd, map_fd;

		/* get socket fd from CNI plugin */
		map_fd = get_cni_fd(internals->if_name);
		if (map_fd < 0) {
			AF_XDP_LOG(ERR, "Failed to receive CNI plugin fd\n");
			goto out_xsk;
		}
		/* get socket fd */
		fd = xsk_socket__fd(rxq->xsk);
		err = bpf_map_update_elem(map_fd, &rxq->xsk_queue_idx, &fd, 0);
		if (err) {
			AF_XDP_LOG(ERR, "Failed to insert unprivileged xsk in map.\n");
			goto out_xsk;
		}
	} else if (rxq->busy_budget) {
		ret = configure_preferred_busy_poll(rxq);
		if (ret) {
			AF_XDP_LOG(ERR, "Failed configure busy polling.\n");
			goto out_xsk;
		}
	}

	return 0;

out_xsk:
	xsk_socket__delete(rxq->xsk);
out_umem:
	if (__atomic_fetch_sub(&rxq->umem->refcnt, 1, __ATOMIC_ACQUIRE) - 1 == 0)
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
	struct pmd_process_private *process_private = dev->process_private;
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

	if (!rxq->busy_budget)
		AF_XDP_LOG(DEBUG, "Preferred busy polling not enabled\n");

	rxq->fds[0].fd = xsk_socket__fd(rxq->xsk);
	rxq->fds[0].events = POLLIN;

	process_private->rxq_xsk_fds[rx_queue_id] = rxq->fds[0].fd;

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
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
	.get_monitor_addr = eth_get_monitor_addr,
};

/* CNI option works in unprivileged container environment
 * and ethernet device functionality will be reduced. So
 * additional customiszed eth_dev_ops struct is needed
 * for cni. Promiscuous enable and disable functionality
 * is removed.
 **/
static const struct eth_dev_ops ops_cni = {
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_close = eth_dev_close,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.mtu_set = eth_dev_mtu_set,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
	.get_monitor_addr = eth_get_monitor_addr,
};

/** parse busy_budget argument */
static int
parse_budget_arg(const char *key __rte_unused,
		  const char *value, void *extra_args)
{
	int *i = (int *)extra_args;
	char *end;

	*i = strtol(value, &end, 10);
	if (*i < 0 || *i > UINT16_MAX) {
		AF_XDP_LOG(ERR, "Invalid busy_budget %i, must be >= 0 and <= %u\n",
				*i, UINT16_MAX);
		return -EINVAL;
	}

	return 0;
}

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
		 int *queue_cnt, int *shared_umem, char *prog_path,
		 int *busy_budget, int *force_copy, int *use_cni)
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

	ret = rte_kvargs_process(kvlist, ETH_AF_XDP_BUDGET_ARG,
				&parse_budget_arg, busy_budget);
	if (ret < 0)
		goto free_kvlist;

	ret = rte_kvargs_process(kvlist, ETH_AF_XDP_FORCE_COPY_ARG,
				&parse_integer_arg, force_copy);
	if (ret < 0)
		goto free_kvlist;

	ret = rte_kvargs_process(kvlist, ETH_AF_XDP_USE_CNI_ARG,
				 &parse_integer_arg, use_cni);
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
	       const char *prog_path, int busy_budget, int force_copy,
	       int use_cni)
{
	const char *name = rte_vdev_device_name(dev);
	const unsigned int numa_node = dev->device.numa_node;
	struct pmd_process_private *process_private;
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
	internals->force_copy = force_copy;
	internals->use_cni = use_cni;

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
		internals->rx_queues[i].busy_budget = busy_budget;
	}

	ret = get_iface_info(if_name, &internals->eth_addr,
			     &internals->if_index);
	if (ret)
		goto err_free_tx;

	process_private = (struct pmd_process_private *)
		rte_zmalloc_socket(name, sizeof(struct pmd_process_private),
				   RTE_CACHE_LINE_SIZE, numa_node);
	if (process_private == NULL) {
		AF_XDP_LOG(ERR, "Failed to alloc memory for process private\n");
		goto err_free_tx;
	}

	eth_dev = rte_eth_vdev_allocate(dev, 0);
	if (eth_dev == NULL)
		goto err_free_pp;

	eth_dev->data->dev_private = internals;
	eth_dev->data->dev_link = pmd_link;
	eth_dev->data->mac_addrs = &internals->eth_addr;
	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;
	if (!internals->use_cni)
		eth_dev->dev_ops = &ops;
	else
		eth_dev->dev_ops = &ops_cni;

	eth_dev->rx_pkt_burst = eth_af_xdp_rx;
	eth_dev->tx_pkt_burst = eth_af_xdp_tx;
	eth_dev->process_private = process_private;

	for (i = 0; i < queue_cnt; i++)
		process_private->rxq_xsk_fds[i] = -1;

#if defined(XDP_UMEM_UNALIGNED_CHUNK_FLAG)
	AF_XDP_LOG(INFO, "Zero copy between umem and mbuf enabled.\n");
#endif

	return eth_dev;

err_free_pp:
	rte_free(process_private);
err_free_tx:
	rte_free(internals->tx_queues);
err_free_rx:
	rte_free(internals->rx_queues);
err_free_internals:
	rte_free(internals);
	return NULL;
}

/* Secondary process requests rxq fds from primary. */
static int
afxdp_mp_request_fds(const char *name, struct rte_eth_dev *dev)
{
	struct pmd_process_private *process_private = dev->process_private;
	struct timespec timeout = {.tv_sec = 1, .tv_nsec = 0};
	struct rte_mp_msg request, *reply;
	struct rte_mp_reply replies;
	struct ipc_hdr *request_param = (struct ipc_hdr *)request.param;
	int i, ret;

	/* Prepare the request */
	memset(&request, 0, sizeof(request));
	strlcpy(request.name, ETH_AF_XDP_MP_KEY, sizeof(request.name));
	strlcpy(request_param->port_name, name,
		sizeof(request_param->port_name));
	request.len_param = sizeof(*request_param);

	/* Send the request and receive the reply */
	AF_XDP_LOG(DEBUG, "Sending multi-process IPC request for %s\n", name);
	ret = rte_mp_request_sync(&request, &replies, &timeout);
	if (ret < 0 || replies.nb_received != 1) {
		AF_XDP_LOG(ERR, "Failed to request fds from primary: %d\n",
			   rte_errno);
		return -1;
	}
	reply = replies.msgs;
	AF_XDP_LOG(DEBUG, "Received multi-process IPC reply for %s\n", name);
	if (dev->data->nb_rx_queues != reply->num_fds) {
		AF_XDP_LOG(ERR, "Incorrect number of fds received: %d != %d\n",
			   reply->num_fds, dev->data->nb_rx_queues);
		return -EINVAL;
	}

	for (i = 0; i < reply->num_fds; i++)
		process_private->rxq_xsk_fds[i] = reply->fds[i];

	free(reply);
	return 0;
}

/* Primary process sends rxq fds to secondary. */
static int
afxdp_mp_send_fds(const struct rte_mp_msg *request, const void *peer)
{
	struct rte_eth_dev *dev;
	struct pmd_process_private *process_private;
	struct rte_mp_msg reply;
	const struct ipc_hdr *request_param =
		(const struct ipc_hdr *)request->param;
	struct ipc_hdr *reply_param =
		(struct ipc_hdr *)reply.param;
	const char *request_name = request_param->port_name;
	int i;

	AF_XDP_LOG(DEBUG, "Received multi-process IPC request for %s\n",
		   request_name);

	/* Find the requested port */
	dev = rte_eth_dev_get_by_name(request_name);
	if (!dev) {
		AF_XDP_LOG(ERR, "Failed to get port id for %s\n", request_name);
		return -1;
	}
	process_private = dev->process_private;

	/* Populate the reply with the xsk fd for each queue */
	reply.num_fds = 0;
	if (dev->data->nb_rx_queues > RTE_MP_MAX_FD_NUM) {
		AF_XDP_LOG(ERR, "Number of rx queues (%d) exceeds max number of fds (%d)\n",
			   dev->data->nb_rx_queues, RTE_MP_MAX_FD_NUM);
		return -EINVAL;
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		reply.fds[reply.num_fds++] = process_private->rxq_xsk_fds[i];

	/* Send the reply */
	strlcpy(reply.name, request->name, sizeof(reply.name));
	strlcpy(reply_param->port_name, request_name,
		sizeof(reply_param->port_name));
	reply.len_param = sizeof(*reply_param);
	AF_XDP_LOG(DEBUG, "Sending multi-process IPC reply for %s\n",
		   reply_param->port_name);
	if (rte_mp_reply(&reply, peer) < 0) {
		AF_XDP_LOG(ERR, "Failed to reply to multi-process IPC request\n");
		return -1;
	}
	return 0;
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
	int busy_budget = -1, ret;
	int force_copy = 0;
	int use_cni = 0;
	struct rte_eth_dev *eth_dev = NULL;
	const char *name = rte_vdev_device_name(dev);

	AF_XDP_LOG(INFO, "Initializing pmd_af_xdp for %s\n", name);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (eth_dev == NULL) {
			AF_XDP_LOG(ERR, "Failed to probe %s\n", name);
			return -EINVAL;
		}
		eth_dev->dev_ops = &ops;
		eth_dev->device = &dev->device;
		eth_dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
		eth_dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
		eth_dev->process_private = (struct pmd_process_private *)
			rte_zmalloc_socket(name,
					   sizeof(struct pmd_process_private),
					   RTE_CACHE_LINE_SIZE,
					   eth_dev->device->numa_node);
		if (eth_dev->process_private == NULL) {
			AF_XDP_LOG(ERR,
				"Failed to alloc memory for process private\n");
			return -ENOMEM;
		}

		/* Obtain the xsk fds from the primary process. */
		if (afxdp_mp_request_fds(name, eth_dev))
			return -1;

		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	kvlist = rte_kvargs_parse(rte_vdev_device_args(dev), valid_arguments);
	if (kvlist == NULL) {
		AF_XDP_LOG(ERR, "Invalid kvargs key\n");
		return -EINVAL;
	}

	if (parse_parameters(kvlist, if_name, &xsk_start_queue_idx,
			     &xsk_queue_cnt, &shared_umem, prog_path,
			     &busy_budget, &force_copy, &use_cni) < 0) {
		AF_XDP_LOG(ERR, "Invalid kvargs value\n");
		return -EINVAL;
	}

	if (use_cni && busy_budget > 0) {
		AF_XDP_LOG(ERR, "When '%s' parameter is used, '%s' parameter is not valid\n",
			ETH_AF_XDP_USE_CNI_ARG, ETH_AF_XDP_BUDGET_ARG);
		return -EINVAL;
	}

	if (use_cni && strnlen(prog_path, PATH_MAX)) {
		AF_XDP_LOG(ERR, "When '%s' parameter is used, '%s' parameter is not valid\n",
			ETH_AF_XDP_USE_CNI_ARG, ETH_AF_XDP_PROG_ARG);
			return -EINVAL;
	}

	if (strlen(if_name) == 0) {
		AF_XDP_LOG(ERR, "Network interface must be specified\n");
		return -EINVAL;
	}

	/* get numa node id from net sysfs */
	if (dev->device.numa_node == SOCKET_ID_ANY) {
		unsigned long numa = 0;
		char numa_path[PATH_MAX];

		snprintf(numa_path, sizeof(numa_path), "/sys/class/net/%s/device/numa_node",
			 if_name);
		if (eal_parse_sysfs_value(numa_path, &numa) != 0)
			dev->device.numa_node = rte_socket_id();
		else
			dev->device.numa_node = numa;
	}

	busy_budget = busy_budget == -1 ? ETH_AF_XDP_DFLT_BUSY_BUDGET :
					busy_budget;

	eth_dev = init_internals(dev, if_name, xsk_start_queue_idx,
				 xsk_queue_cnt, shared_umem, prog_path,
				 busy_budget, force_copy, use_cni);
	if (eth_dev == NULL) {
		AF_XDP_LOG(ERR, "Failed to init internals\n");
		return -1;
	}

	/* Register IPC callback which shares xsk fds from primary to secondary */
	if (!afxdp_dev_count) {
		ret = rte_mp_action_register(ETH_AF_XDP_MP_KEY, afxdp_mp_send_fds);
		if (ret < 0 && rte_errno != ENOTSUP) {
			AF_XDP_LOG(ERR, "%s: Failed to register multi-process IPC callback: %s\n",
				   name, strerror(rte_errno));
			return -1;
		}
	}
	afxdp_dev_count++;

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
	if (afxdp_dev_count == 1)
		rte_mp_action_unregister(ETH_AF_XDP_MP_KEY);
	afxdp_dev_count--;
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
			      "xdp_prog=<string> "
			      "busy_budget=<int> "
			      "force_copy=<int> "
			      "use_cni=<int> ");
