/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 IGEL Co., Ltd.
 * Copyright(c) 2016-2018 Intel Corporation
 */
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/epoll.h>

#include <rte_mbuf.h>
#include <ethdev_driver.h>
#include <ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_bus_vdev.h>
#include <rte_kvargs.h>
#include <rte_vhost.h>
#include <rte_spinlock.h>

#include "rte_eth_vhost.h"

RTE_LOG_REGISTER_DEFAULT(vhost_logtype, NOTICE);

#define VHOST_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, vhost_logtype, __VA_ARGS__)

enum {VIRTIO_RXQ, VIRTIO_TXQ, VIRTIO_QNUM};

#define ETH_VHOST_IFACE_ARG		"iface"
#define ETH_VHOST_QUEUES_ARG		"queues"
#define ETH_VHOST_CLIENT_ARG		"client"
#define ETH_VHOST_IOMMU_SUPPORT		"iommu-support"
#define ETH_VHOST_POSTCOPY_SUPPORT	"postcopy-support"
#define ETH_VHOST_VIRTIO_NET_F_HOST_TSO "tso"
#define ETH_VHOST_LINEAR_BUF  "linear-buffer"
#define ETH_VHOST_EXT_BUF  "ext-buffer"
#define VHOST_MAX_PKT_BURST 32

static const char *valid_arguments[] = {
	ETH_VHOST_IFACE_ARG,
	ETH_VHOST_QUEUES_ARG,
	ETH_VHOST_CLIENT_ARG,
	ETH_VHOST_IOMMU_SUPPORT,
	ETH_VHOST_POSTCOPY_SUPPORT,
	ETH_VHOST_VIRTIO_NET_F_HOST_TSO,
	ETH_VHOST_LINEAR_BUF,
	ETH_VHOST_EXT_BUF,
	NULL
};

static struct rte_ether_addr base_eth_addr = {
	.addr_bytes = {
		0x56 /* V */,
		0x48 /* H */,
		0x4F /* O */,
		0x53 /* S */,
		0x54 /* T */,
		0x00
	}
};

enum vhost_xstats_pkts {
	VHOST_UNDERSIZE_PKT = 0,
	VHOST_64_PKT,
	VHOST_65_TO_127_PKT,
	VHOST_128_TO_255_PKT,
	VHOST_256_TO_511_PKT,
	VHOST_512_TO_1023_PKT,
	VHOST_1024_TO_1522_PKT,
	VHOST_1523_TO_MAX_PKT,
	VHOST_BROADCAST_PKT,
	VHOST_MULTICAST_PKT,
	VHOST_UNICAST_PKT,
	VHOST_PKT,
	VHOST_BYTE,
	VHOST_MISSED_PKT,
	VHOST_ERRORS_PKT,
	VHOST_ERRORS_FRAGMENTED,
	VHOST_ERRORS_JABBER,
	VHOST_UNKNOWN_PROTOCOL,
	VHOST_XSTATS_MAX,
};

struct vhost_stats {
	uint64_t pkts;
	uint64_t bytes;
	uint64_t missed_pkts;
	uint64_t xstats[VHOST_XSTATS_MAX];
};

struct vhost_queue {
	int vid;
	rte_atomic32_t allow_queuing;
	rte_atomic32_t while_queuing;
	struct pmd_internal *internal;
	struct rte_mempool *mb_pool;
	uint16_t port;
	uint16_t virtqueue_id;
	struct vhost_stats stats;
	int intr_enable;
	rte_spinlock_t intr_lock;
};

struct pmd_internal {
	rte_atomic32_t dev_attached;
	char *iface_name;
	uint64_t flags;
	uint64_t disable_flags;
	uint16_t max_queues;
	int vid;
	rte_atomic32_t started;
	uint8_t vlan_strip;
};

struct internal_list {
	TAILQ_ENTRY(internal_list) next;
	struct rte_eth_dev *eth_dev;
};

TAILQ_HEAD(internal_list_head, internal_list);
static struct internal_list_head internal_list =
	TAILQ_HEAD_INITIALIZER(internal_list);

static pthread_mutex_t internal_list_lock = PTHREAD_MUTEX_INITIALIZER;

static struct rte_eth_link pmd_link = {
		.link_speed = 10000,
		.link_duplex = RTE_ETH_LINK_FULL_DUPLEX,
		.link_status = RTE_ETH_LINK_DOWN
};

struct rte_vhost_vring_state {
	rte_spinlock_t lock;

	bool cur[RTE_MAX_QUEUES_PER_PORT * 2];
	bool seen[RTE_MAX_QUEUES_PER_PORT * 2];
	unsigned int index;
	unsigned int max_vring;
};

static struct rte_vhost_vring_state *vring_states[RTE_MAX_ETHPORTS];

#define VHOST_XSTATS_NAME_SIZE 64

struct vhost_xstats_name_off {
	char name[VHOST_XSTATS_NAME_SIZE];
	uint64_t offset;
};

/* [rx]_is prepended to the name string here */
static const struct vhost_xstats_name_off vhost_rxport_stat_strings[] = {
	{"good_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_PKT])},
	{"total_bytes",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_BYTE])},
	{"missed_pkts",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_MISSED_PKT])},
	{"broadcast_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_BROADCAST_PKT])},
	{"multicast_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_MULTICAST_PKT])},
	{"unicast_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_UNICAST_PKT])},
	 {"undersize_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_UNDERSIZE_PKT])},
	{"size_64_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_64_PKT])},
	{"size_65_to_127_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_65_TO_127_PKT])},
	{"size_128_to_255_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_128_TO_255_PKT])},
	{"size_256_to_511_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_256_TO_511_PKT])},
	{"size_512_to_1023_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_512_TO_1023_PKT])},
	{"size_1024_to_1522_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_1024_TO_1522_PKT])},
	{"size_1523_to_max_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_1523_TO_MAX_PKT])},
	{"errors_with_bad_CRC",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_ERRORS_PKT])},
	{"fragmented_errors",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_ERRORS_FRAGMENTED])},
	{"jabber_errors",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_ERRORS_JABBER])},
	{"unknown_protos_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_UNKNOWN_PROTOCOL])},
};

/* [tx]_ is prepended to the name string here */
static const struct vhost_xstats_name_off vhost_txport_stat_strings[] = {
	{"good_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_PKT])},
	{"total_bytes",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_BYTE])},
	{"missed_pkts",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_MISSED_PKT])},
	{"broadcast_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_BROADCAST_PKT])},
	{"multicast_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_MULTICAST_PKT])},
	{"unicast_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_UNICAST_PKT])},
	{"undersize_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_UNDERSIZE_PKT])},
	{"size_64_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_64_PKT])},
	{"size_65_to_127_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_65_TO_127_PKT])},
	{"size_128_to_255_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_128_TO_255_PKT])},
	{"size_256_to_511_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_256_TO_511_PKT])},
	{"size_512_to_1023_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_512_TO_1023_PKT])},
	{"size_1024_to_1522_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_1024_TO_1522_PKT])},
	{"size_1523_to_max_packets",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_1523_TO_MAX_PKT])},
	{"errors_with_bad_CRC",
	 offsetof(struct vhost_queue, stats.xstats[VHOST_ERRORS_PKT])},
};

#define VHOST_NB_XSTATS_RXPORT (sizeof(vhost_rxport_stat_strings) / \
				sizeof(vhost_rxport_stat_strings[0]))

#define VHOST_NB_XSTATS_TXPORT (sizeof(vhost_txport_stat_strings) / \
				sizeof(vhost_txport_stat_strings[0]))

static int
vhost_dev_xstats_reset(struct rte_eth_dev *dev)
{
	struct vhost_queue *vq = NULL;
	unsigned int i = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		vq = dev->data->rx_queues[i];
		if (!vq)
			continue;
		memset(&vq->stats, 0, sizeof(vq->stats));
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		vq = dev->data->tx_queues[i];
		if (!vq)
			continue;
		memset(&vq->stats, 0, sizeof(vq->stats));
	}

	return 0;
}

static int
vhost_dev_xstats_get_names(struct rte_eth_dev *dev __rte_unused,
			   struct rte_eth_xstat_name *xstats_names,
			   unsigned int limit __rte_unused)
{
	unsigned int t = 0;
	int count = 0;
	int nstats = VHOST_NB_XSTATS_RXPORT + VHOST_NB_XSTATS_TXPORT;

	if (!xstats_names)
		return nstats;
	for (t = 0; t < VHOST_NB_XSTATS_RXPORT; t++) {
		snprintf(xstats_names[count].name,
			 sizeof(xstats_names[count].name),
			 "rx_%s", vhost_rxport_stat_strings[t].name);
		count++;
	}
	for (t = 0; t < VHOST_NB_XSTATS_TXPORT; t++) {
		snprintf(xstats_names[count].name,
			 sizeof(xstats_names[count].name),
			 "tx_%s", vhost_txport_stat_strings[t].name);
		count++;
	}
	return count;
}

static int
vhost_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		     unsigned int n)
{
	unsigned int i;
	unsigned int t;
	unsigned int count = 0;
	struct vhost_queue *vq = NULL;
	unsigned int nxstats = VHOST_NB_XSTATS_RXPORT + VHOST_NB_XSTATS_TXPORT;

	if (n < nxstats)
		return nxstats;

	for (t = 0; t < VHOST_NB_XSTATS_RXPORT; t++) {
		xstats[count].value = 0;
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			vq = dev->data->rx_queues[i];
			if (!vq)
				continue;
			xstats[count].value +=
				*(uint64_t *)(((char *)vq)
				+ vhost_rxport_stat_strings[t].offset);
		}
		xstats[count].id = count;
		count++;
	}
	for (t = 0; t < VHOST_NB_XSTATS_TXPORT; t++) {
		xstats[count].value = 0;
		for (i = 0; i < dev->data->nb_tx_queues; i++) {
			vq = dev->data->tx_queues[i];
			if (!vq)
				continue;
			xstats[count].value +=
				*(uint64_t *)(((char *)vq)
				+ vhost_txport_stat_strings[t].offset);
		}
		xstats[count].id = count;
		count++;
	}
	return count;
}

static inline void
vhost_count_xcast_packets(struct vhost_queue *vq,
				struct rte_mbuf *mbuf)
{
	struct rte_ether_addr *ea = NULL;
	struct vhost_stats *pstats = &vq->stats;

	ea = rte_pktmbuf_mtod(mbuf, struct rte_ether_addr *);
	if (rte_is_multicast_ether_addr(ea)) {
		if (rte_is_broadcast_ether_addr(ea))
			pstats->xstats[VHOST_BROADCAST_PKT]++;
		else
			pstats->xstats[VHOST_MULTICAST_PKT]++;
	} else {
		pstats->xstats[VHOST_UNICAST_PKT]++;
	}
}

static __rte_always_inline void
vhost_update_single_packet_xstats(struct vhost_queue *vq, struct rte_mbuf *buf)
{
	uint32_t pkt_len = 0;
	uint64_t index;
	struct vhost_stats *pstats = &vq->stats;

	pstats->xstats[VHOST_PKT]++;
	pkt_len = buf->pkt_len;
	if (pkt_len == 64) {
		pstats->xstats[VHOST_64_PKT]++;
	} else if (pkt_len > 64 && pkt_len < 1024) {
		index = (sizeof(pkt_len) * 8)
			- __builtin_clz(pkt_len) - 5;
		pstats->xstats[index]++;
	} else {
		if (pkt_len < 64)
			pstats->xstats[VHOST_UNDERSIZE_PKT]++;
		else if (pkt_len <= 1522)
			pstats->xstats[VHOST_1024_TO_1522_PKT]++;
		else if (pkt_len > 1522)
			pstats->xstats[VHOST_1523_TO_MAX_PKT]++;
	}
	vhost_count_xcast_packets(vq, buf);
}

static uint16_t
eth_vhost_rx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	struct vhost_queue *r = q;
	uint16_t i, nb_rx = 0;
	uint16_t nb_receive = nb_bufs;

	if (unlikely(rte_atomic32_read(&r->allow_queuing) == 0))
		return 0;

	rte_atomic32_set(&r->while_queuing, 1);

	if (unlikely(rte_atomic32_read(&r->allow_queuing) == 0))
		goto out;

	/* Dequeue packets from guest TX queue */
	while (nb_receive) {
		uint16_t nb_pkts;
		uint16_t num = (uint16_t)RTE_MIN(nb_receive,
						 VHOST_MAX_PKT_BURST);

		nb_pkts = rte_vhost_dequeue_burst(r->vid, r->virtqueue_id,
						  r->mb_pool, &bufs[nb_rx],
						  num);

		nb_rx += nb_pkts;
		nb_receive -= nb_pkts;
		if (nb_pkts < num)
			break;
	}

	r->stats.pkts += nb_rx;

	for (i = 0; likely(i < nb_rx); i++) {
		bufs[i]->port = r->port;
		bufs[i]->vlan_tci = 0;

		if (r->internal->vlan_strip)
			rte_vlan_strip(bufs[i]);

		r->stats.bytes += bufs[i]->pkt_len;
		r->stats.xstats[VHOST_BYTE] += bufs[i]->pkt_len;

		vhost_update_single_packet_xstats(r, bufs[i]);
	}

out:
	rte_atomic32_set(&r->while_queuing, 0);

	return nb_rx;
}

static uint16_t
eth_vhost_tx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	struct vhost_queue *r = q;
	uint16_t i, nb_tx = 0;
	uint16_t nb_send = 0;
	uint64_t nb_bytes = 0;
	uint64_t nb_missed = 0;

	if (unlikely(rte_atomic32_read(&r->allow_queuing) == 0))
		return 0;

	rte_atomic32_set(&r->while_queuing, 1);

	if (unlikely(rte_atomic32_read(&r->allow_queuing) == 0))
		goto out;

	for (i = 0; i < nb_bufs; i++) {
		struct rte_mbuf *m = bufs[i];

		/* Do VLAN tag insertion */
		if (m->ol_flags & RTE_MBUF_F_TX_VLAN) {
			int error = rte_vlan_insert(&m);
			if (unlikely(error)) {
				rte_pktmbuf_free(m);
				continue;
			}
		}

		bufs[nb_send] = m;
		++nb_send;
	}

	/* Enqueue packets to guest RX queue */
	while (nb_send) {
		uint16_t nb_pkts;
		uint16_t num = (uint16_t)RTE_MIN(nb_send,
						 VHOST_MAX_PKT_BURST);

		nb_pkts = rte_vhost_enqueue_burst(r->vid, r->virtqueue_id,
						  &bufs[nb_tx], num);

		nb_tx += nb_pkts;
		nb_send -= nb_pkts;
		if (nb_pkts < num)
			break;
	}

	for (i = 0; likely(i < nb_tx); i++) {
		nb_bytes += bufs[i]->pkt_len;
		vhost_update_single_packet_xstats(r, bufs[i]);
	}

	nb_missed = nb_bufs - nb_tx;

	r->stats.pkts += nb_tx;
	r->stats.bytes += nb_bytes;
	r->stats.missed_pkts += nb_missed;

	r->stats.xstats[VHOST_BYTE] += nb_bytes;
	r->stats.xstats[VHOST_MISSED_PKT] += nb_missed;
	r->stats.xstats[VHOST_UNICAST_PKT] += nb_missed;

	/* According to RFC2863, ifHCOutUcastPkts, ifHCOutMulticastPkts and
	 * ifHCOutBroadcastPkts counters are increased when packets are not
	 * transmitted successfully.
	 */
	for (i = nb_tx; i < nb_bufs; i++)
		vhost_count_xcast_packets(r, bufs[i]);

	for (i = 0; likely(i < nb_tx); i++)
		rte_pktmbuf_free(bufs[i]);
out:
	rte_atomic32_set(&r->while_queuing, 0);

	return nb_tx;
}

static inline struct internal_list *
find_internal_resource(char *ifname)
{
	int found = 0;
	struct internal_list *list;
	struct pmd_internal *internal;

	if (ifname == NULL)
		return NULL;

	pthread_mutex_lock(&internal_list_lock);

	TAILQ_FOREACH(list, &internal_list, next) {
		internal = list->eth_dev->data->dev_private;
		if (!strcmp(internal->iface_name, ifname)) {
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
eth_vhost_update_intr(struct rte_eth_dev *eth_dev, uint16_t rxq_idx)
{
	struct rte_intr_handle *handle = eth_dev->intr_handle;
	struct rte_epoll_event rev, *elist;
	int epfd, ret;

	if (handle == NULL)
		return 0;

	elist = rte_intr_elist_index_get(handle, rxq_idx);
	if (rte_intr_efds_index_get(handle, rxq_idx) == elist->fd)
		return 0;

	VHOST_LOG(INFO, "kickfd for rxq-%d was changed, updating handler.\n",
			rxq_idx);

	if (elist->fd != -1)
		VHOST_LOG(ERR, "Unexpected previous kickfd value (Got %d, expected -1).\n",
			elist->fd);

	/*
	 * First remove invalid epoll event, and then install
	 * the new one. May be solved with a proper API in the
	 * future.
	 */
	epfd = elist->epfd;
	rev = *elist;
	ret = rte_epoll_ctl(epfd, EPOLL_CTL_DEL, rev.fd,
			elist);
	if (ret) {
		VHOST_LOG(ERR, "Delete epoll event failed.\n");
		return ret;
	}

	rev.fd = rte_intr_efds_index_get(handle, rxq_idx);
	if (rte_intr_elist_index_set(handle, rxq_idx, rev))
		return -rte_errno;

	elist = rte_intr_elist_index_get(handle, rxq_idx);
	ret = rte_epoll_ctl(epfd, EPOLL_CTL_ADD, rev.fd, elist);
	if (ret) {
		VHOST_LOG(ERR, "Add epoll event failed.\n");
		return ret;
	}

	return 0;
}

static int
eth_rxq_intr_enable(struct rte_eth_dev *dev, uint16_t qid)
{
	struct rte_vhost_vring vring;
	struct vhost_queue *vq;
	int old_intr_enable, ret = 0;

	vq = dev->data->rx_queues[qid];
	if (!vq) {
		VHOST_LOG(ERR, "rxq%d is not setup yet\n", qid);
		return -1;
	}

	rte_spinlock_lock(&vq->intr_lock);
	old_intr_enable = vq->intr_enable;
	vq->intr_enable = 1;
	ret = eth_vhost_update_intr(dev, qid);
	rte_spinlock_unlock(&vq->intr_lock);

	if (ret < 0) {
		VHOST_LOG(ERR, "Failed to update rxq%d's intr\n", qid);
		vq->intr_enable = old_intr_enable;
		return ret;
	}

	ret = rte_vhost_get_vhost_vring(vq->vid, (qid << 1) + 1, &vring);
	if (ret < 0) {
		VHOST_LOG(ERR, "Failed to get rxq%d's vring\n", qid);
		return ret;
	}
	VHOST_LOG(INFO, "Enable interrupt for rxq%d\n", qid);
	rte_vhost_enable_guest_notification(vq->vid, (qid << 1) + 1, 1);
	rte_wmb();

	return ret;
}

static int
eth_rxq_intr_disable(struct rte_eth_dev *dev, uint16_t qid)
{
	struct rte_vhost_vring vring;
	struct vhost_queue *vq;
	int ret = 0;

	vq = dev->data->rx_queues[qid];
	if (!vq) {
		VHOST_LOG(ERR, "rxq%d is not setup yet\n", qid);
		return -1;
	}

	ret = rte_vhost_get_vhost_vring(vq->vid, (qid << 1) + 1, &vring);
	if (ret < 0) {
		VHOST_LOG(ERR, "Failed to get rxq%d's vring", qid);
		return ret;
	}
	VHOST_LOG(INFO, "Disable interrupt for rxq%d\n", qid);
	rte_vhost_enable_guest_notification(vq->vid, (qid << 1) + 1, 0);
	rte_wmb();

	vq->intr_enable = 0;

	return 0;
}

static void
eth_vhost_uninstall_intr(struct rte_eth_dev *dev)
{
	struct rte_intr_handle *intr_handle = dev->intr_handle;

	if (intr_handle != NULL) {
		rte_intr_vec_list_free(intr_handle);
		rte_intr_instance_free(intr_handle);
	}
	dev->intr_handle = NULL;
}

static int
eth_vhost_install_intr(struct rte_eth_dev *dev)
{
	struct rte_vhost_vring vring;
	struct vhost_queue *vq;
	int nb_rxq = dev->data->nb_rx_queues;
	int i;
	int ret;

	/* uninstall firstly if we are reconnecting */
	if (dev->intr_handle != NULL)
		eth_vhost_uninstall_intr(dev);

	dev->intr_handle = rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (dev->intr_handle == NULL) {
		VHOST_LOG(ERR, "Fail to allocate intr_handle\n");
		return -ENOMEM;
	}
	if (rte_intr_efd_counter_size_set(dev->intr_handle, sizeof(uint64_t)))
		return -rte_errno;

	if (rte_intr_vec_list_alloc(dev->intr_handle, NULL, nb_rxq)) {
		VHOST_LOG(ERR,
			"Failed to allocate memory for interrupt vector\n");
		rte_intr_instance_free(dev->intr_handle);
		return -ENOMEM;
	}


	VHOST_LOG(INFO, "Prepare intr vec\n");
	for (i = 0; i < nb_rxq; i++) {
		if (rte_intr_vec_list_index_set(dev->intr_handle, i, RTE_INTR_VEC_RXTX_OFFSET + i))
			return -rte_errno;
		if (rte_intr_efds_index_set(dev->intr_handle, i, -1))
			return -rte_errno;
		vq = dev->data->rx_queues[i];
		if (!vq) {
			VHOST_LOG(INFO, "rxq-%d not setup yet, skip!\n", i);
			continue;
		}

		ret = rte_vhost_get_vhost_vring(vq->vid, (i << 1) + 1, &vring);
		if (ret < 0) {
			VHOST_LOG(INFO,
				"Failed to get rxq-%d's vring, skip!\n", i);
			continue;
		}

		if (vring.kickfd < 0) {
			VHOST_LOG(INFO,
				"rxq-%d's kickfd is invalid, skip!\n", i);
			continue;
		}

		if (rte_intr_efds_index_set(dev->intr_handle, i, vring.kickfd))
			continue;
		VHOST_LOG(INFO, "Installed intr vec for rxq-%d\n", i);
	}

	if (rte_intr_nb_efd_set(dev->intr_handle, nb_rxq))
		return -rte_errno;

	if (rte_intr_max_intr_set(dev->intr_handle, nb_rxq + 1))
		return -rte_errno;

	if (rte_intr_type_set(dev->intr_handle, RTE_INTR_HANDLE_VDEV))
		return -rte_errno;

	return 0;
}

static void
update_queuing_status(struct rte_eth_dev *dev)
{
	struct pmd_internal *internal = dev->data->dev_private;
	struct vhost_queue *vq;
	unsigned int i;
	int allow_queuing = 1;

	if (!dev->data->rx_queues || !dev->data->tx_queues)
		return;

	if (rte_atomic32_read(&internal->started) == 0 ||
	    rte_atomic32_read(&internal->dev_attached) == 0)
		allow_queuing = 0;

	/* Wait until rx/tx_pkt_burst stops accessing vhost device */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		vq = dev->data->rx_queues[i];
		if (vq == NULL)
			continue;
		rte_atomic32_set(&vq->allow_queuing, allow_queuing);
		while (rte_atomic32_read(&vq->while_queuing))
			rte_pause();
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		vq = dev->data->tx_queues[i];
		if (vq == NULL)
			continue;
		rte_atomic32_set(&vq->allow_queuing, allow_queuing);
		while (rte_atomic32_read(&vq->while_queuing))
			rte_pause();
	}
}

static void
queue_setup(struct rte_eth_dev *eth_dev, struct pmd_internal *internal)
{
	struct vhost_queue *vq;
	int i;

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		vq = eth_dev->data->rx_queues[i];
		if (!vq)
			continue;
		vq->vid = internal->vid;
		vq->internal = internal;
		vq->port = eth_dev->data->port_id;
	}
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		vq = eth_dev->data->tx_queues[i];
		if (!vq)
			continue;
		vq->vid = internal->vid;
		vq->internal = internal;
		vq->port = eth_dev->data->port_id;
	}
}

static int
new_device(int vid)
{
	struct rte_eth_dev *eth_dev;
	struct internal_list *list;
	struct pmd_internal *internal;
	struct rte_eth_conf *dev_conf;
	unsigned i;
	char ifname[PATH_MAX];
#ifdef RTE_LIBRTE_VHOST_NUMA
	int newnode;
#endif

	rte_vhost_get_ifname(vid, ifname, sizeof(ifname));
	list = find_internal_resource(ifname);
	if (list == NULL) {
		VHOST_LOG(INFO, "Invalid device name: %s\n", ifname);
		return -1;
	}

	eth_dev = list->eth_dev;
	internal = eth_dev->data->dev_private;
	dev_conf = &eth_dev->data->dev_conf;

#ifdef RTE_LIBRTE_VHOST_NUMA
	newnode = rte_vhost_get_numa_node(vid);
	if (newnode >= 0)
		eth_dev->data->numa_node = newnode;
#endif

	internal->vid = vid;
	if (rte_atomic32_read(&internal->started) == 1) {
		queue_setup(eth_dev, internal);

		if (dev_conf->intr_conf.rxq) {
			if (eth_vhost_install_intr(eth_dev) < 0) {
				VHOST_LOG(INFO,
					"Failed to install interrupt handler.");
					return -1;
			}
		}
	} else {
		VHOST_LOG(INFO, "RX/TX queues not exist yet\n");
	}

	for (i = 0; i < rte_vhost_get_vring_num(vid); i++)
		rte_vhost_enable_guest_notification(vid, i, 0);

	rte_vhost_get_mtu(vid, &eth_dev->data->mtu);

	eth_dev->data->dev_link.link_status = RTE_ETH_LINK_UP;

	rte_atomic32_set(&internal->dev_attached, 1);
	update_queuing_status(eth_dev);

	VHOST_LOG(INFO, "Vhost device %d created\n", vid);

	rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_INTR_LSC, NULL);

	return 0;
}

static void
destroy_device(int vid)
{
	struct rte_eth_dev *eth_dev;
	struct pmd_internal *internal;
	struct vhost_queue *vq;
	struct internal_list *list;
	char ifname[PATH_MAX];
	unsigned i;
	struct rte_vhost_vring_state *state;

	rte_vhost_get_ifname(vid, ifname, sizeof(ifname));
	list = find_internal_resource(ifname);
	if (list == NULL) {
		VHOST_LOG(ERR, "Invalid interface name: %s\n", ifname);
		return;
	}
	eth_dev = list->eth_dev;
	internal = eth_dev->data->dev_private;

	rte_atomic32_set(&internal->dev_attached, 0);
	update_queuing_status(eth_dev);

	eth_dev->data->dev_link.link_status = RTE_ETH_LINK_DOWN;

	if (eth_dev->data->rx_queues && eth_dev->data->tx_queues) {
		for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
			vq = eth_dev->data->rx_queues[i];
			if (!vq)
				continue;
			vq->vid = -1;
		}
		for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
			vq = eth_dev->data->tx_queues[i];
			if (!vq)
				continue;
			vq->vid = -1;
		}
	}

	state = vring_states[eth_dev->data->port_id];
	rte_spinlock_lock(&state->lock);
	for (i = 0; i <= state->max_vring; i++) {
		state->cur[i] = false;
		state->seen[i] = false;
	}
	state->max_vring = 0;
	rte_spinlock_unlock(&state->lock);

	VHOST_LOG(INFO, "Vhost device %d destroyed\n", vid);
	eth_vhost_uninstall_intr(eth_dev);

	rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_INTR_LSC, NULL);
}

static int
vring_conf_update(int vid, struct rte_eth_dev *eth_dev, uint16_t vring_id)
{
	struct rte_eth_conf *dev_conf = &eth_dev->data->dev_conf;
	struct pmd_internal *internal = eth_dev->data->dev_private;
	struct vhost_queue *vq;
	struct rte_vhost_vring vring;
	int rx_idx = vring_id % 2 ? (vring_id - 1) >> 1 : -1;
	int ret = 0;

	/*
	 * The vring kickfd may be changed after the new device notification.
	 * Update it when the vring state is updated.
	 */
	if (rx_idx >= 0 && rx_idx < eth_dev->data->nb_rx_queues &&
	    rte_atomic32_read(&internal->dev_attached) &&
	    rte_atomic32_read(&internal->started) &&
	    dev_conf->intr_conf.rxq) {
		ret = rte_vhost_get_vhost_vring(vid, vring_id, &vring);
		if (ret) {
			VHOST_LOG(ERR, "Failed to get vring %d information.\n",
					vring_id);
			return ret;
		}

		if (rte_intr_efds_index_set(eth_dev->intr_handle, rx_idx,
						   vring.kickfd))
			return -rte_errno;

		vq = eth_dev->data->rx_queues[rx_idx];
		if (!vq) {
			VHOST_LOG(ERR, "rxq%d is not setup yet\n", rx_idx);
			return -1;
		}

		rte_spinlock_lock(&vq->intr_lock);
		if (vq->intr_enable)
			ret = eth_vhost_update_intr(eth_dev, rx_idx);
		rte_spinlock_unlock(&vq->intr_lock);
	}

	return ret;
}

static int
vring_state_changed(int vid, uint16_t vring, int enable)
{
	struct rte_vhost_vring_state *state;
	struct rte_eth_dev *eth_dev;
	struct internal_list *list;
	char ifname[PATH_MAX];

	rte_vhost_get_ifname(vid, ifname, sizeof(ifname));
	list = find_internal_resource(ifname);
	if (list == NULL) {
		VHOST_LOG(ERR, "Invalid interface name: %s\n", ifname);
		return -1;
	}

	eth_dev = list->eth_dev;
	/* won't be NULL */
	state = vring_states[eth_dev->data->port_id];

	if (enable && vring_conf_update(vid, eth_dev, vring))
		VHOST_LOG(INFO, "Failed to update vring-%d configuration.\n",
			  (int)vring);

	rte_spinlock_lock(&state->lock);
	if (state->cur[vring] == enable) {
		rte_spinlock_unlock(&state->lock);
		return 0;
	}
	state->cur[vring] = enable;
	state->max_vring = RTE_MAX(vring, state->max_vring);
	rte_spinlock_unlock(&state->lock);

	VHOST_LOG(INFO, "vring%u is %s\n",
			vring, enable ? "enabled" : "disabled");

	rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_QUEUE_STATE, NULL);

	return 0;
}

static struct rte_vhost_device_ops vhost_ops = {
	.new_device          = new_device,
	.destroy_device      = destroy_device,
	.vring_state_changed = vring_state_changed,
};

static int
vhost_driver_setup(struct rte_eth_dev *eth_dev)
{
	struct pmd_internal *internal = eth_dev->data->dev_private;
	struct internal_list *list = NULL;
	struct rte_vhost_vring_state *vring_state = NULL;
	unsigned int numa_node = eth_dev->device->numa_node;
	const char *name = eth_dev->device->name;

	/* Don't try to setup again if it has already been done. */
	list = find_internal_resource(internal->iface_name);
	if (list)
		return 0;

	list = rte_zmalloc_socket(name, sizeof(*list), 0, numa_node);
	if (list == NULL)
		return -1;

	vring_state = rte_zmalloc_socket(name, sizeof(*vring_state),
					 0, numa_node);
	if (vring_state == NULL)
		goto free_list;

	list->eth_dev = eth_dev;
	pthread_mutex_lock(&internal_list_lock);
	TAILQ_INSERT_TAIL(&internal_list, list, next);
	pthread_mutex_unlock(&internal_list_lock);

	rte_spinlock_init(&vring_state->lock);
	vring_states[eth_dev->data->port_id] = vring_state;

	if (rte_vhost_driver_register(internal->iface_name, internal->flags))
		goto list_remove;

	if (internal->disable_flags) {
		if (rte_vhost_driver_disable_features(internal->iface_name,
						      internal->disable_flags))
			goto drv_unreg;
	}

	if (rte_vhost_driver_callback_register(internal->iface_name,
					       &vhost_ops) < 0) {
		VHOST_LOG(ERR, "Can't register callbacks\n");
		goto drv_unreg;
	}

	if (rte_vhost_driver_start(internal->iface_name) < 0) {
		VHOST_LOG(ERR, "Failed to start driver for %s\n",
			  internal->iface_name);
		goto drv_unreg;
	}

	return 0;

drv_unreg:
	rte_vhost_driver_unregister(internal->iface_name);
list_remove:
	vring_states[eth_dev->data->port_id] = NULL;
	pthread_mutex_lock(&internal_list_lock);
	TAILQ_REMOVE(&internal_list, list, next);
	pthread_mutex_unlock(&internal_list_lock);
	rte_free(vring_state);
free_list:
	rte_free(list);

	return -1;
}

int
rte_eth_vhost_get_queue_event(uint16_t port_id,
		struct rte_eth_vhost_queue_event *event)
{
	struct rte_vhost_vring_state *state;
	unsigned int i;
	int idx;

	if (port_id >= RTE_MAX_ETHPORTS) {
		VHOST_LOG(ERR, "Invalid port id\n");
		return -1;
	}

	state = vring_states[port_id];
	if (!state) {
		VHOST_LOG(ERR, "Unused port\n");
		return -1;
	}

	rte_spinlock_lock(&state->lock);
	for (i = 0; i <= state->max_vring; i++) {
		idx = state->index++ % (state->max_vring + 1);

		if (state->cur[idx] != state->seen[idx]) {
			state->seen[idx] = state->cur[idx];
			event->queue_id = idx / 2;
			event->rx = idx & 1;
			event->enable = state->cur[idx];
			rte_spinlock_unlock(&state->lock);
			return 0;
		}
	}
	rte_spinlock_unlock(&state->lock);

	return -1;
}

int
rte_eth_vhost_get_vid_from_port_id(uint16_t port_id)
{
	struct internal_list *list;
	struct rte_eth_dev *eth_dev;
	struct vhost_queue *vq;
	int vid = -1;

	if (!rte_eth_dev_is_valid_port(port_id))
		return -1;

	pthread_mutex_lock(&internal_list_lock);

	TAILQ_FOREACH(list, &internal_list, next) {
		eth_dev = list->eth_dev;
		if (eth_dev->data->port_id == port_id) {
			vq = eth_dev->data->rx_queues[0];
			if (vq) {
				vid = vq->vid;
			}
			break;
		}
	}

	pthread_mutex_unlock(&internal_list_lock);

	return vid;
}

static int
eth_dev_configure(struct rte_eth_dev *dev)
{
	struct pmd_internal *internal = dev->data->dev_private;
	const struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;

	/* NOTE: the same process has to operate a vhost interface
	 * from beginning to end (from eth_dev configure to eth_dev close).
	 * It is user's responsibility at the moment.
	 */
	if (vhost_driver_setup(dev) < 0)
		return -1;

	internal->vlan_strip = !!(rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP);

	return 0;
}

static int
eth_dev_start(struct rte_eth_dev *eth_dev)
{
	struct pmd_internal *internal = eth_dev->data->dev_private;
	struct rte_eth_conf *dev_conf = &eth_dev->data->dev_conf;

	queue_setup(eth_dev, internal);

	if (rte_atomic32_read(&internal->dev_attached) == 1) {
		if (dev_conf->intr_conf.rxq) {
			if (eth_vhost_install_intr(eth_dev) < 0) {
				VHOST_LOG(INFO,
					"Failed to install interrupt handler.");
					return -1;
			}
		}
	}

	rte_atomic32_set(&internal->started, 1);
	update_queuing_status(eth_dev);

	return 0;
}

static int
eth_dev_stop(struct rte_eth_dev *dev)
{
	struct pmd_internal *internal = dev->data->dev_private;

	dev->data->dev_started = 0;
	rte_atomic32_set(&internal->started, 0);
	update_queuing_status(dev);

	return 0;
}

static int
eth_dev_close(struct rte_eth_dev *dev)
{
	struct pmd_internal *internal;
	struct internal_list *list;
	unsigned int i, ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	internal = dev->data->dev_private;
	if (!internal)
		return 0;

	ret = eth_dev_stop(dev);

	list = find_internal_resource(internal->iface_name);
	if (list) {
		rte_vhost_driver_unregister(internal->iface_name);
		pthread_mutex_lock(&internal_list_lock);
		TAILQ_REMOVE(&internal_list, list, next);
		pthread_mutex_unlock(&internal_list_lock);
		rte_free(list);
	}

	if (dev->data->rx_queues)
		for (i = 0; i < dev->data->nb_rx_queues; i++)
			rte_free(dev->data->rx_queues[i]);

	if (dev->data->tx_queues)
		for (i = 0; i < dev->data->nb_tx_queues; i++)
			rte_free(dev->data->tx_queues[i]);

	rte_free(internal->iface_name);
	rte_free(internal);

	dev->data->dev_private = NULL;

	rte_free(vring_states[dev->data->port_id]);
	vring_states[dev->data->port_id] = NULL;

	return ret;
}

static int
eth_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		   uint16_t nb_rx_desc __rte_unused,
		   unsigned int socket_id,
		   const struct rte_eth_rxconf *rx_conf __rte_unused,
		   struct rte_mempool *mb_pool)
{
	struct vhost_queue *vq;

	vq = rte_zmalloc_socket(NULL, sizeof(struct vhost_queue),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (vq == NULL) {
		VHOST_LOG(ERR, "Failed to allocate memory for rx queue\n");
		return -ENOMEM;
	}

	vq->mb_pool = mb_pool;
	vq->virtqueue_id = rx_queue_id * VIRTIO_QNUM + VIRTIO_TXQ;
	rte_spinlock_init(&vq->intr_lock);
	dev->data->rx_queues[rx_queue_id] = vq;

	return 0;
}

static int
eth_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		   uint16_t nb_tx_desc __rte_unused,
		   unsigned int socket_id,
		   const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct vhost_queue *vq;

	vq = rte_zmalloc_socket(NULL, sizeof(struct vhost_queue),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (vq == NULL) {
		VHOST_LOG(ERR, "Failed to allocate memory for tx queue\n");
		return -ENOMEM;
	}

	vq->virtqueue_id = tx_queue_id * VIRTIO_QNUM + VIRTIO_RXQ;
	rte_spinlock_init(&vq->intr_lock);
	dev->data->tx_queues[tx_queue_id] = vq;

	return 0;
}

static int
eth_dev_info(struct rte_eth_dev *dev,
	     struct rte_eth_dev_info *dev_info)
{
	struct pmd_internal *internal;

	internal = dev->data->dev_private;
	if (internal == NULL) {
		VHOST_LOG(ERR, "Invalid device specified\n");
		return -ENODEV;
	}

	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = internal->max_queues;
	dev_info->max_tx_queues = internal->max_queues;
	dev_info->min_rx_bufsize = 0;

	dev_info->tx_offload_capa = RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
				RTE_ETH_TX_OFFLOAD_VLAN_INSERT;
	dev_info->rx_offload_capa = RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

	return 0;
}

static int
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	unsigned i;
	unsigned long rx_total = 0, tx_total = 0;
	unsigned long rx_total_bytes = 0, tx_total_bytes = 0;
	struct vhost_queue *vq;

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_rx_queues; i++) {
		if (dev->data->rx_queues[i] == NULL)
			continue;
		vq = dev->data->rx_queues[i];
		stats->q_ipackets[i] = vq->stats.pkts;
		rx_total += stats->q_ipackets[i];

		stats->q_ibytes[i] = vq->stats.bytes;
		rx_total_bytes += stats->q_ibytes[i];
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < dev->data->nb_tx_queues; i++) {
		if (dev->data->tx_queues[i] == NULL)
			continue;
		vq = dev->data->tx_queues[i];
		stats->q_opackets[i] = vq->stats.pkts;
		tx_total += stats->q_opackets[i];

		stats->q_obytes[i] = vq->stats.bytes;
		tx_total_bytes += stats->q_obytes[i];
	}

	stats->ipackets = rx_total;
	stats->opackets = tx_total;
	stats->ibytes = rx_total_bytes;
	stats->obytes = tx_total_bytes;

	return 0;
}

static int
eth_stats_reset(struct rte_eth_dev *dev)
{
	struct vhost_queue *vq;
	unsigned i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (dev->data->rx_queues[i] == NULL)
			continue;
		vq = dev->data->rx_queues[i];
		vq->stats.pkts = 0;
		vq->stats.bytes = 0;
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (dev->data->tx_queues[i] == NULL)
			continue;
		vq = dev->data->tx_queues[i];
		vq->stats.pkts = 0;
		vq->stats.bytes = 0;
		vq->stats.missed_pkts = 0;
	}

	return 0;
}

static void
eth_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	rte_free(dev->data->rx_queues[qid]);
}

static void
eth_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	rte_free(dev->data->tx_queues[qid]);
}

static int
eth_tx_done_cleanup(void *txq __rte_unused, uint32_t free_cnt __rte_unused)
{
	/*
	 * vHost does not hang onto mbuf. eth_vhost_tx() copies packet data
	 * and releases mbuf, so nothing to cleanup.
	 */
	return 0;
}

static int
eth_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused)
{
	return 0;
}

static uint32_t
eth_rx_queue_count(void *rx_queue)
{
	struct vhost_queue *vq;

	vq = rx_queue;
	if (vq == NULL)
		return 0;

	return rte_vhost_rx_queue_count(vq->vid, vq->virtqueue_id);
}

#define CLB_VAL_IDX 0
#define CLB_MSK_IDX 1
#define CLB_MATCH_IDX 2
static int
vhost_monitor_callback(const uint64_t value,
		const uint64_t opaque[RTE_POWER_MONITOR_OPAQUE_SZ])
{
	const uint64_t m = opaque[CLB_MSK_IDX];
	const uint64_t v = opaque[CLB_VAL_IDX];
	const uint64_t c = opaque[CLB_MATCH_IDX];

	if (c)
		return (value & m) == v ? -1 : 0;
	else
		return (value & m) == v ? 0 : -1;
}

static int
vhost_get_monitor_addr(void *rx_queue, struct rte_power_monitor_cond *pmc)
{
	struct vhost_queue *vq = rx_queue;
	struct rte_vhost_power_monitor_cond vhost_pmc;
	int ret;
	if (vq == NULL)
		return -EINVAL;
	ret = rte_vhost_get_monitor_addr(vq->vid, vq->virtqueue_id,
			&vhost_pmc);
	if (ret < 0)
		return -EINVAL;
	pmc->addr = vhost_pmc.addr;
	pmc->opaque[CLB_VAL_IDX] = vhost_pmc.val;
	pmc->opaque[CLB_MSK_IDX] = vhost_pmc.mask;
	pmc->opaque[CLB_MATCH_IDX] = vhost_pmc.match;
	pmc->size = vhost_pmc.size;
	pmc->fn = vhost_monitor_callback;

	return 0;
}

static const struct eth_dev_ops ops = {
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_close = eth_dev_close,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.rx_queue_release = eth_rx_queue_release,
	.tx_queue_release = eth_tx_queue_release,
	.tx_done_cleanup = eth_tx_done_cleanup,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
	.xstats_reset = vhost_dev_xstats_reset,
	.xstats_get = vhost_dev_xstats_get,
	.xstats_get_names = vhost_dev_xstats_get_names,
	.rx_queue_intr_enable = eth_rxq_intr_enable,
	.rx_queue_intr_disable = eth_rxq_intr_disable,
	.get_monitor_addr = vhost_get_monitor_addr,
};

static int
eth_dev_vhost_create(struct rte_vdev_device *dev, char *iface_name,
	int16_t queues, const unsigned int numa_node, uint64_t flags,
	uint64_t disable_flags)
{
	const char *name = rte_vdev_device_name(dev);
	struct rte_eth_dev_data *data;
	struct pmd_internal *internal = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct rte_ether_addr *eth_addr = NULL;

	VHOST_LOG(INFO, "Creating VHOST-USER backend on numa socket %u\n",
		numa_node);

	/* reserve an ethdev entry */
	eth_dev = rte_eth_vdev_allocate(dev, sizeof(*internal));
	if (eth_dev == NULL)
		goto error;
	data = eth_dev->data;

	eth_addr = rte_zmalloc_socket(name, sizeof(*eth_addr), 0, numa_node);
	if (eth_addr == NULL)
		goto error;
	data->mac_addrs = eth_addr;
	*eth_addr = base_eth_addr;
	eth_addr->addr_bytes[5] = eth_dev->data->port_id;

	/* now put it all together
	 * - store queue data in internal,
	 * - point eth_dev_data to internals
	 * - and point eth_dev structure to new eth_dev_data structure
	 */
	internal = eth_dev->data->dev_private;
	internal->iface_name = rte_malloc_socket(name, strlen(iface_name) + 1,
						 0, numa_node);
	if (internal->iface_name == NULL)
		goto error;
	strcpy(internal->iface_name, iface_name);

	data->nb_rx_queues = queues;
	data->nb_tx_queues = queues;
	internal->max_queues = queues;
	internal->vid = -1;
	internal->flags = flags;
	internal->disable_flags = disable_flags;
	data->dev_link = pmd_link;
	data->dev_flags = RTE_ETH_DEV_INTR_LSC |
				RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;
	data->promiscuous = 1;
	data->all_multicast = 1;

	eth_dev->dev_ops = &ops;
	eth_dev->rx_queue_count = eth_rx_queue_count;

	/* finally assign rx and tx ops */
	eth_dev->rx_pkt_burst = eth_vhost_rx;
	eth_dev->tx_pkt_burst = eth_vhost_tx;

	rte_eth_dev_probing_finish(eth_dev);
	return 0;

error:
	if (internal)
		rte_free(internal->iface_name);
	rte_eth_dev_release_port(eth_dev);

	return -1;
}

static inline int
open_iface(const char *key __rte_unused, const char *value, void *extra_args)
{
	const char **iface_name = extra_args;

	if (value == NULL)
		return -1;

	*iface_name = value;

	return 0;
}

static inline int
open_int(const char *key __rte_unused, const char *value, void *extra_args)
{
	uint16_t *n = extra_args;

	if (value == NULL || extra_args == NULL)
		return -EINVAL;

	*n = (uint16_t)strtoul(value, NULL, 0);
	if (*n == USHRT_MAX && errno == ERANGE)
		return -1;

	return 0;
}

static int
rte_pmd_vhost_probe(struct rte_vdev_device *dev)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;
	char *iface_name;
	uint16_t queues;
	uint64_t flags = 0;
	uint64_t disable_flags = 0;
	int client_mode = 0;
	int iommu_support = 0;
	int postcopy_support = 0;
	int tso = 0;
	int linear_buf = 0;
	int ext_buf = 0;
	struct rte_eth_dev *eth_dev;
	const char *name = rte_vdev_device_name(dev);

	VHOST_LOG(INFO, "Initializing pmd_vhost for %s\n", name);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			VHOST_LOG(ERR, "Failed to probe %s\n", name);
			return -1;
		}
		eth_dev->rx_pkt_burst = eth_vhost_rx;
		eth_dev->tx_pkt_burst = eth_vhost_tx;
		eth_dev->dev_ops = &ops;
		if (dev->device.numa_node == SOCKET_ID_ANY)
			dev->device.numa_node = rte_socket_id();
		eth_dev->device = &dev->device;
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	kvlist = rte_kvargs_parse(rte_vdev_device_args(dev), valid_arguments);
	if (kvlist == NULL)
		return -1;

	if (rte_kvargs_count(kvlist, ETH_VHOST_IFACE_ARG) == 1) {
		ret = rte_kvargs_process(kvlist, ETH_VHOST_IFACE_ARG,
					 &open_iface, &iface_name);
		if (ret < 0)
			goto out_free;
	} else {
		ret = -1;
		goto out_free;
	}

	if (rte_kvargs_count(kvlist, ETH_VHOST_QUEUES_ARG) == 1) {
		ret = rte_kvargs_process(kvlist, ETH_VHOST_QUEUES_ARG,
					 &open_int, &queues);
		if (ret < 0 || queues > RTE_MAX_QUEUES_PER_PORT)
			goto out_free;

	} else
		queues = 1;

	if (rte_kvargs_count(kvlist, ETH_VHOST_CLIENT_ARG) == 1) {
		ret = rte_kvargs_process(kvlist, ETH_VHOST_CLIENT_ARG,
					 &open_int, &client_mode);
		if (ret < 0)
			goto out_free;

		if (client_mode)
			flags |= RTE_VHOST_USER_CLIENT;
	}

	if (rte_kvargs_count(kvlist, ETH_VHOST_IOMMU_SUPPORT) == 1) {
		ret = rte_kvargs_process(kvlist, ETH_VHOST_IOMMU_SUPPORT,
					 &open_int, &iommu_support);
		if (ret < 0)
			goto out_free;

		if (iommu_support)
			flags |= RTE_VHOST_USER_IOMMU_SUPPORT;
	}

	if (rte_kvargs_count(kvlist, ETH_VHOST_POSTCOPY_SUPPORT) == 1) {
		ret = rte_kvargs_process(kvlist, ETH_VHOST_POSTCOPY_SUPPORT,
					 &open_int, &postcopy_support);
		if (ret < 0)
			goto out_free;

		if (postcopy_support)
			flags |= RTE_VHOST_USER_POSTCOPY_SUPPORT;
	}

	if (rte_kvargs_count(kvlist, ETH_VHOST_VIRTIO_NET_F_HOST_TSO) == 1) {
		ret = rte_kvargs_process(kvlist,
				ETH_VHOST_VIRTIO_NET_F_HOST_TSO,
				&open_int, &tso);
		if (ret < 0)
			goto out_free;

		if (tso == 0) {
			disable_flags |= (1ULL << VIRTIO_NET_F_HOST_TSO4);
			disable_flags |= (1ULL << VIRTIO_NET_F_HOST_TSO6);
		}
	}

	if (rte_kvargs_count(kvlist, ETH_VHOST_LINEAR_BUF) == 1) {
		ret = rte_kvargs_process(kvlist,
				ETH_VHOST_LINEAR_BUF,
				&open_int, &linear_buf);
		if (ret < 0)
			goto out_free;

		if (linear_buf == 1)
			flags |= RTE_VHOST_USER_LINEARBUF_SUPPORT;
	}

	if (rte_kvargs_count(kvlist, ETH_VHOST_EXT_BUF) == 1) {
		ret = rte_kvargs_process(kvlist,
				ETH_VHOST_EXT_BUF,
				&open_int, &ext_buf);
		if (ret < 0)
			goto out_free;

		if (ext_buf == 1)
			flags |= RTE_VHOST_USER_EXTBUF_SUPPORT;
	}

	if (dev->device.numa_node == SOCKET_ID_ANY)
		dev->device.numa_node = rte_socket_id();

	ret = eth_dev_vhost_create(dev, iface_name, queues,
				   dev->device.numa_node, flags, disable_flags);
	if (ret == -1)
		VHOST_LOG(ERR, "Failed to create %s\n", name);

out_free:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
rte_pmd_vhost_remove(struct rte_vdev_device *dev)
{
	const char *name;
	struct rte_eth_dev *eth_dev = NULL;

	name = rte_vdev_device_name(dev);
	VHOST_LOG(INFO, "Un-Initializing pmd_vhost for %s\n", name);

	/* find an ethdev entry */
	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev == NULL)
		return 0;

	eth_dev_close(eth_dev);
	rte_eth_dev_release_port(eth_dev);

	return 0;
}

static struct rte_vdev_driver pmd_vhost_drv = {
	.probe = rte_pmd_vhost_probe,
	.remove = rte_pmd_vhost_remove,
};

RTE_PMD_REGISTER_VDEV(net_vhost, pmd_vhost_drv);
RTE_PMD_REGISTER_ALIAS(net_vhost, eth_vhost);
RTE_PMD_REGISTER_PARAM_STRING(net_vhost,
	"iface=<ifc> "
	"queues=<int> "
	"client=<0|1> "
	"iommu-support=<0|1> "
	"postcopy-support=<0|1> "
	"tso=<0|1> "
	"linear-buffer=<0|1> "
	"ext-buffer=<0|1>");
