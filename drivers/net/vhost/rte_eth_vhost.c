/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 IGEL Co., Ltd.
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
 *     * Neither the name of IGEL Co.,Ltd. nor the names of its
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
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_vdev.h>
#include <rte_kvargs.h>
#include <rte_vhost.h>
#include <rte_spinlock.h>

#include "rte_eth_vhost.h"

enum {VIRTIO_RXQ, VIRTIO_TXQ, VIRTIO_QNUM};

#define ETH_VHOST_IFACE_ARG		"iface"
#define ETH_VHOST_QUEUES_ARG		"queues"
#define ETH_VHOST_CLIENT_ARG		"client"
#define ETH_VHOST_DEQUEUE_ZERO_COPY	"dequeue-zero-copy"
#define VHOST_MAX_PKT_BURST 32

static const char *valid_arguments[] = {
	ETH_VHOST_IFACE_ARG,
	ETH_VHOST_QUEUES_ARG,
	ETH_VHOST_CLIENT_ARG,
	ETH_VHOST_DEQUEUE_ZERO_COPY,
	NULL
};

static struct ether_addr base_eth_addr = {
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
	uint8_t port;
	uint16_t virtqueue_id;
	struct vhost_stats stats;
};

struct pmd_internal {
	rte_atomic32_t dev_attached;
	char *dev_name;
	char *iface_name;
	uint16_t max_queues;
	rte_atomic32_t started;
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
		.link_duplex = ETH_LINK_FULL_DUPLEX,
		.link_status = ETH_LINK_DOWN
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
	 offsetof(struct vhost_queue, stats.pkts)},
	{"total_bytes",
	 offsetof(struct vhost_queue, stats.bytes)},
	{"missed_pkts",
	 offsetof(struct vhost_queue, stats.missed_pkts)},
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
	 offsetof(struct vhost_queue, stats.pkts)},
	{"total_bytes",
	 offsetof(struct vhost_queue, stats.bytes)},
	{"missed_pkts",
	 offsetof(struct vhost_queue, stats.missed_pkts)},
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

static void
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

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		vq = dev->data->rx_queues[i];
		if (!vq)
			continue;
		vq->stats.xstats[VHOST_UNICAST_PKT] = vq->stats.pkts
				- (vq->stats.xstats[VHOST_BROADCAST_PKT]
				+ vq->stats.xstats[VHOST_MULTICAST_PKT]);
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		vq = dev->data->tx_queues[i];
		if (!vq)
			continue;
		vq->stats.xstats[VHOST_UNICAST_PKT] = vq->stats.pkts
				+ vq->stats.missed_pkts
				- (vq->stats.xstats[VHOST_BROADCAST_PKT]
				+ vq->stats.xstats[VHOST_MULTICAST_PKT]);
	}
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
vhost_count_multicast_broadcast(struct vhost_queue *vq,
				struct rte_mbuf *mbuf)
{
	struct ether_addr *ea = NULL;
	struct vhost_stats *pstats = &vq->stats;

	ea = rte_pktmbuf_mtod(mbuf, struct ether_addr *);
	if (is_multicast_ether_addr(ea)) {
		if (is_broadcast_ether_addr(ea))
			pstats->xstats[VHOST_BROADCAST_PKT]++;
		else
			pstats->xstats[VHOST_MULTICAST_PKT]++;
	}
}

static void
vhost_update_packet_xstats(struct vhost_queue *vq,
			   struct rte_mbuf **bufs,
			   uint16_t count)
{
	uint32_t pkt_len = 0;
	uint64_t i = 0;
	uint64_t index;
	struct vhost_stats *pstats = &vq->stats;

	for (i = 0; i < count ; i++) {
		pkt_len = bufs[i]->pkt_len;
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
		vhost_count_multicast_broadcast(vq, bufs[i]);
	}
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
		r->stats.bytes += bufs[i]->pkt_len;
	}

	vhost_update_packet_xstats(r, bufs, nb_rx);

out:
	rte_atomic32_set(&r->while_queuing, 0);

	return nb_rx;
}

static uint16_t
eth_vhost_tx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	struct vhost_queue *r = q;
	uint16_t i, nb_tx = 0;
	uint16_t nb_send = nb_bufs;

	if (unlikely(rte_atomic32_read(&r->allow_queuing) == 0))
		return 0;

	rte_atomic32_set(&r->while_queuing, 1);

	if (unlikely(rte_atomic32_read(&r->allow_queuing) == 0))
		goto out;

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

	r->stats.pkts += nb_tx;
	r->stats.missed_pkts += nb_bufs - nb_tx;

	for (i = 0; likely(i < nb_tx); i++)
		r->stats.bytes += bufs[i]->pkt_len;

	vhost_update_packet_xstats(r, bufs, nb_tx);

	/* According to RFC2863 page42 section ifHCOutMulticastPkts and
	 * ifHCOutBroadcastPkts, the counters "multicast" and "broadcast"
	 * are increased when packets are not transmitted successfully.
	 */
	for (i = nb_tx; i < nb_bufs; i++)
		vhost_count_multicast_broadcast(r, bufs[i]);

	for (i = 0; likely(i < nb_tx); i++)
		rte_pktmbuf_free(bufs[i]);
out:
	rte_atomic32_set(&r->while_queuing, 0);

	return nb_tx;
}

static int
eth_dev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
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

static void
update_queuing_status(struct rte_eth_dev *dev)
{
	struct pmd_internal *internal = dev->data->dev_private;
	struct vhost_queue *vq;
	unsigned int i;
	int allow_queuing = 1;

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

static int
new_device(int vid)
{
	struct rte_eth_dev *eth_dev;
	struct internal_list *list;
	struct pmd_internal *internal;
	struct vhost_queue *vq;
	unsigned i;
	char ifname[PATH_MAX];
#ifdef RTE_LIBRTE_VHOST_NUMA
	int newnode;
#endif

	rte_vhost_get_ifname(vid, ifname, sizeof(ifname));
	list = find_internal_resource(ifname);
	if (list == NULL) {
		RTE_LOG(INFO, PMD, "Invalid device name: %s\n", ifname);
		return -1;
	}

	eth_dev = list->eth_dev;
	internal = eth_dev->data->dev_private;

#ifdef RTE_LIBRTE_VHOST_NUMA
	newnode = rte_vhost_get_numa_node(vid);
	if (newnode >= 0)
		eth_dev->data->numa_node = newnode;
#endif

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		vq = eth_dev->data->rx_queues[i];
		if (vq == NULL)
			continue;
		vq->vid = vid;
		vq->internal = internal;
		vq->port = eth_dev->data->port_id;
	}
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		vq = eth_dev->data->tx_queues[i];
		if (vq == NULL)
			continue;
		vq->vid = vid;
		vq->internal = internal;
		vq->port = eth_dev->data->port_id;
	}

	for (i = 0; i < rte_vhost_get_vring_num(vid); i++)
		rte_vhost_enable_guest_notification(vid, i, 0);

	rte_vhost_get_mtu(vid, &eth_dev->data->mtu);

	eth_dev->data->dev_link.link_status = ETH_LINK_UP;

	rte_atomic32_set(&internal->dev_attached, 1);
	update_queuing_status(eth_dev);

	RTE_LOG(INFO, PMD, "New connection established\n");

	_rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_INTR_LSC,
				      NULL, NULL);

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
		RTE_LOG(ERR, PMD, "Invalid interface name: %s\n", ifname);
		return;
	}
	eth_dev = list->eth_dev;
	internal = eth_dev->data->dev_private;

	rte_atomic32_set(&internal->dev_attached, 0);
	update_queuing_status(eth_dev);

	eth_dev->data->dev_link.link_status = ETH_LINK_DOWN;

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		vq = eth_dev->data->rx_queues[i];
		if (vq == NULL)
			continue;
		vq->vid = -1;
	}
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		vq = eth_dev->data->tx_queues[i];
		if (vq == NULL)
			continue;
		vq->vid = -1;
	}

	state = vring_states[eth_dev->data->port_id];
	rte_spinlock_lock(&state->lock);
	for (i = 0; i <= state->max_vring; i++) {
		state->cur[i] = false;
		state->seen[i] = false;
	}
	state->max_vring = 0;
	rte_spinlock_unlock(&state->lock);

	RTE_LOG(INFO, PMD, "Connection closed\n");

	_rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_INTR_LSC,
				      NULL, NULL);
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
		RTE_LOG(ERR, PMD, "Invalid interface name: %s\n", ifname);
		return -1;
	}

	eth_dev = list->eth_dev;
	/* won't be NULL */
	state = vring_states[eth_dev->data->port_id];
	rte_spinlock_lock(&state->lock);
	state->cur[vring] = enable;
	state->max_vring = RTE_MAX(vring, state->max_vring);
	rte_spinlock_unlock(&state->lock);

	RTE_LOG(INFO, PMD, "vring%u is %s\n",
			vring, enable ? "enabled" : "disabled");

	_rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_QUEUE_STATE,
				      NULL, NULL);

	return 0;
}

static struct vhost_device_ops vhost_ops = {
	.new_device          = new_device,
	.destroy_device      = destroy_device,
	.vring_state_changed = vring_state_changed,
};

int
rte_eth_vhost_get_queue_event(uint8_t port_id,
		struct rte_eth_vhost_queue_event *event)
{
	struct rte_vhost_vring_state *state;
	unsigned int i;
	int idx;

	if (port_id >= RTE_MAX_ETHPORTS) {
		RTE_LOG(ERR, PMD, "Invalid port id\n");
		return -1;
	}

	state = vring_states[port_id];
	if (!state) {
		RTE_LOG(ERR, PMD, "Unused port\n");
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
rte_eth_vhost_get_vid_from_port_id(uint8_t port_id)
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
eth_dev_start(struct rte_eth_dev *dev)
{
	struct pmd_internal *internal = dev->data->dev_private;

	rte_atomic32_set(&internal->started, 1);
	update_queuing_status(dev);

	return 0;
}

static void
eth_dev_stop(struct rte_eth_dev *dev)
{
	struct pmd_internal *internal = dev->data->dev_private;

	rte_atomic32_set(&internal->started, 0);
	update_queuing_status(dev);
}

static void
eth_dev_close(struct rte_eth_dev *dev)
{
	struct pmd_internal *internal;
	struct internal_list *list;
	unsigned int i;

	internal = dev->data->dev_private;
	if (!internal)
		return;

	eth_dev_stop(dev);

	rte_vhost_driver_unregister(internal->iface_name);

	list = find_internal_resource(internal->iface_name);
	if (!list)
		return;

	pthread_mutex_lock(&internal_list_lock);
	TAILQ_REMOVE(&internal_list, list, next);
	pthread_mutex_unlock(&internal_list_lock);
	rte_free(list);

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		rte_free(dev->data->rx_queues[i]);
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		rte_free(dev->data->tx_queues[i]);

	rte_free(dev->data->mac_addrs);
	free(internal->dev_name);
	free(internal->iface_name);
	rte_free(internal);

	dev->data->dev_private = NULL;
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
		RTE_LOG(ERR, PMD, "Failed to allocate memory for rx queue\n");
		return -ENOMEM;
	}

	vq->mb_pool = mb_pool;
	vq->virtqueue_id = rx_queue_id * VIRTIO_QNUM + VIRTIO_TXQ;
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
		RTE_LOG(ERR, PMD, "Failed to allocate memory for tx queue\n");
		return -ENOMEM;
	}

	vq->virtqueue_id = tx_queue_id * VIRTIO_QNUM + VIRTIO_RXQ;
	dev->data->tx_queues[tx_queue_id] = vq;

	return 0;
}

static void
eth_dev_info(struct rte_eth_dev *dev,
	     struct rte_eth_dev_info *dev_info)
{
	struct pmd_internal *internal;

	internal = dev->data->dev_private;
	if (internal == NULL) {
		RTE_LOG(ERR, PMD, "Invalid device specified\n");
		return;
	}

	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = internal->max_queues;
	dev_info->max_tx_queues = internal->max_queues;
	dev_info->min_rx_bufsize = 0;
}

static void
eth_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	unsigned i;
	unsigned long rx_total = 0, tx_total = 0, tx_missed_total = 0;
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
		tx_missed_total += vq->stats.missed_pkts;
		tx_total += stats->q_opackets[i];

		stats->q_obytes[i] = vq->stats.bytes;
		tx_total_bytes += stats->q_obytes[i];
	}

	stats->ipackets = rx_total;
	stats->opackets = tx_total;
	stats->oerrors = tx_missed_total;
	stats->ibytes = rx_total_bytes;
	stats->obytes = tx_total_bytes;
}

static void
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
}

static void
eth_queue_release(void *q)
{
	rte_free(q);
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
eth_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct vhost_queue *vq;

	vq = dev->data->rx_queues[rx_queue_id];
	if (vq == NULL)
		return 0;

	return rte_vhost_rx_queue_count(vq->vid, vq->virtqueue_id);
}

static const struct eth_dev_ops ops = {
	.dev_start = eth_dev_start,
	.dev_stop = eth_dev_stop,
	.dev_close = eth_dev_close,
	.dev_configure = eth_dev_configure,
	.dev_infos_get = eth_dev_info,
	.rx_queue_setup = eth_rx_queue_setup,
	.tx_queue_setup = eth_tx_queue_setup,
	.rx_queue_release = eth_queue_release,
	.tx_queue_release = eth_queue_release,
	.tx_done_cleanup = eth_tx_done_cleanup,
	.rx_queue_count = eth_rx_queue_count,
	.link_update = eth_link_update,
	.stats_get = eth_stats_get,
	.stats_reset = eth_stats_reset,
	.xstats_reset = vhost_dev_xstats_reset,
	.xstats_get = vhost_dev_xstats_get,
	.xstats_get_names = vhost_dev_xstats_get_names,
};

static struct rte_vdev_driver pmd_vhost_drv;

static int
eth_dev_vhost_create(struct rte_vdev_device *dev, char *iface_name,
	int16_t queues, const unsigned int numa_node, uint64_t flags)
{
	const char *name = rte_vdev_device_name(dev);
	struct rte_eth_dev_data *data = NULL;
	struct pmd_internal *internal = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	struct ether_addr *eth_addr = NULL;
	struct rte_vhost_vring_state *vring_state = NULL;
	struct internal_list *list = NULL;

	RTE_LOG(INFO, PMD, "Creating VHOST-USER backend on numa socket %u\n",
		numa_node);

	/* now do all data allocation - for eth_dev structure and internal
	 * (private) data
	 */
	data = rte_zmalloc_socket(name, sizeof(*data), 0, numa_node);
	if (data == NULL)
		goto error;

	list = rte_zmalloc_socket(name, sizeof(*list), 0, numa_node);
	if (list == NULL)
		goto error;

	/* reserve an ethdev entry */
	eth_dev = rte_eth_vdev_allocate(dev, sizeof(*internal));
	if (eth_dev == NULL)
		goto error;

	eth_addr = rte_zmalloc_socket(name, sizeof(*eth_addr), 0, numa_node);
	if (eth_addr == NULL)
		goto error;
	*eth_addr = base_eth_addr;
	eth_addr->addr_bytes[5] = eth_dev->data->port_id;

	vring_state = rte_zmalloc_socket(name,
			sizeof(*vring_state), 0, numa_node);
	if (vring_state == NULL)
		goto error;

	/* now put it all together
	 * - store queue data in internal,
	 * - point eth_dev_data to internals
	 * - and point eth_dev structure to new eth_dev_data structure
	 */
	internal = eth_dev->data->dev_private;
	internal->dev_name = strdup(name);
	if (internal->dev_name == NULL)
		goto error;
	internal->iface_name = strdup(iface_name);
	if (internal->iface_name == NULL)
		goto error;

	list->eth_dev = eth_dev;
	pthread_mutex_lock(&internal_list_lock);
	TAILQ_INSERT_TAIL(&internal_list, list, next);
	pthread_mutex_unlock(&internal_list_lock);

	rte_spinlock_init(&vring_state->lock);
	vring_states[eth_dev->data->port_id] = vring_state;

	/* We'll replace the 'data' originally allocated by eth_dev. So the
	 * vhost PMD resources won't be shared between multi processes.
	 */
	rte_memcpy(data, eth_dev->data, sizeof(*data));
	eth_dev->data = data;

	data->nb_rx_queues = queues;
	data->nb_tx_queues = queues;
	internal->max_queues = queues;
	data->dev_link = pmd_link;
	data->mac_addrs = eth_addr;
	data->dev_flags =
		RTE_ETH_DEV_DETACHABLE | RTE_ETH_DEV_INTR_LSC;

	eth_dev->dev_ops = &ops;

	/* finally assign rx and tx ops */
	eth_dev->rx_pkt_burst = eth_vhost_rx;
	eth_dev->tx_pkt_burst = eth_vhost_tx;

	if (rte_vhost_driver_register(iface_name, flags))
		goto error;

	if (rte_vhost_driver_callback_register(iface_name, &vhost_ops) < 0) {
		RTE_LOG(ERR, PMD, "Can't register callbacks\n");
		goto error;
	}

	if (rte_vhost_driver_start(iface_name) < 0) {
		RTE_LOG(ERR, PMD, "Failed to start driver for %s\n",
			iface_name);
		goto error;
	}

	return data->port_id;

error:
	if (internal) {
		free(internal->iface_name);
		free(internal->dev_name);
	}
	rte_free(vring_state);
	rte_free(eth_addr);
	if (eth_dev)
		rte_eth_dev_release_port(eth_dev);
	rte_free(internal);
	rte_free(list);
	rte_free(data);

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
	int client_mode = 0;
	int dequeue_zero_copy = 0;

	RTE_LOG(INFO, PMD, "Initializing pmd_vhost for %s\n",
		rte_vdev_device_name(dev));

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

	if (rte_kvargs_count(kvlist, ETH_VHOST_DEQUEUE_ZERO_COPY) == 1) {
		ret = rte_kvargs_process(kvlist, ETH_VHOST_DEQUEUE_ZERO_COPY,
					 &open_int, &dequeue_zero_copy);
		if (ret < 0)
			goto out_free;

		if (dequeue_zero_copy)
			flags |= RTE_VHOST_USER_DEQUEUE_ZERO_COPY;
	}

	if (dev->device.numa_node == SOCKET_ID_ANY)
		dev->device.numa_node = rte_socket_id();

	eth_dev_vhost_create(dev, iface_name, queues, dev->device.numa_node,
		flags);

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
	RTE_LOG(INFO, PMD, "Un-Initializing pmd_vhost for %s\n", name);

	/* find an ethdev entry */
	eth_dev = rte_eth_dev_allocated(name);
	if (eth_dev == NULL)
		return -ENODEV;

	eth_dev_close(eth_dev);

	rte_free(vring_states[eth_dev->data->port_id]);
	vring_states[eth_dev->data->port_id] = NULL;

	rte_free(eth_dev->data);

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
	"queues=<int>");
