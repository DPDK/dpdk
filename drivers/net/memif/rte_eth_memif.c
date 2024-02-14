/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 Cisco Systems, Inc.  All rights reserved.
 */

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/if_ether.h>
#include <errno.h>
#include <sys/eventfd.h>

#include <rte_version.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_kvargs.h>
#include <bus_vdev_driver.h>
#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal_memconfig.h>

#include "rte_eth_memif.h"
#include "memif_socket.h"

#define ETH_MEMIF_ID_ARG		"id"
#define ETH_MEMIF_ROLE_ARG		"role"
#define ETH_MEMIF_PKT_BUFFER_SIZE_ARG	"bsize"
#define ETH_MEMIF_RING_SIZE_ARG		"rsize"
#define ETH_MEMIF_SOCKET_ARG		"socket"
#define ETH_MEMIF_SOCKET_ABSTRACT_ARG	"socket-abstract"
#define ETH_MEMIF_OWNER_UID_ARG		"owner-uid"
#define ETH_MEMIF_OWNER_GID_ARG		"owner-gid"
#define ETH_MEMIF_MAC_ARG		"mac"
#define ETH_MEMIF_ZC_ARG		"zero-copy"
#define ETH_MEMIF_SECRET_ARG		"secret"

static const char * const valid_arguments[] = {
	ETH_MEMIF_ID_ARG,
	ETH_MEMIF_ROLE_ARG,
	ETH_MEMIF_PKT_BUFFER_SIZE_ARG,
	ETH_MEMIF_RING_SIZE_ARG,
	ETH_MEMIF_SOCKET_ARG,
	ETH_MEMIF_SOCKET_ABSTRACT_ARG,
	ETH_MEMIF_OWNER_UID_ARG,
	ETH_MEMIF_OWNER_GID_ARG,
	ETH_MEMIF_MAC_ARG,
	ETH_MEMIF_ZC_ARG,
	ETH_MEMIF_SECRET_ARG,
	NULL
};

static const struct rte_eth_link pmd_link = {
	.link_speed = RTE_ETH_SPEED_NUM_100G,
	.link_duplex = RTE_ETH_LINK_FULL_DUPLEX,
	.link_status = RTE_ETH_LINK_DOWN,
	.link_autoneg = RTE_ETH_LINK_AUTONEG
};

#define MEMIF_MP_SEND_REGION		"memif_mp_send_region"


static int memif_region_init_zc(const struct rte_memseg_list *msl,
				const struct rte_memseg *ms, void *arg);

const char *
memif_version(void)
{
	return ("memif-" RTE_STR(MEMIF_VERSION_MAJOR) "." RTE_STR(MEMIF_VERSION_MINOR));
}

/* Message header to synchronize regions */
struct mp_region_msg {
	char port_name[RTE_DEV_NAME_MAX_LEN];
	memif_region_index_t idx;
	memif_region_size_t size;
};

static int
memif_mp_send_region(const struct rte_mp_msg *msg, const void *peer)
{
	struct rte_eth_dev *dev;
	struct pmd_process_private *proc_private;
	const struct mp_region_msg *msg_param = (const struct mp_region_msg *)msg->param;
	struct rte_mp_msg reply;
	struct mp_region_msg *reply_param = (struct mp_region_msg *)reply.param;

	/* Get requested port */
	dev = rte_eth_dev_get_by_name(msg_param->port_name);
	if (!dev) {
		MIF_LOG(ERR, "Failed to get port id for %s",
			msg_param->port_name);
		return -1;
	}
	proc_private = dev->process_private;

	memset(&reply, 0, sizeof(reply));
	strlcpy(reply.name, msg->name, sizeof(reply.name));
	reply_param->idx = msg_param->idx;
	if (proc_private->regions[msg_param->idx] != NULL) {
		reply_param->size = proc_private->regions[msg_param->idx]->region_size;
		reply.fds[0] = proc_private->regions[msg_param->idx]->fd;
		reply.num_fds = 1;
	}
	reply.len_param = sizeof(*reply_param);
	if (rte_mp_reply(&reply, peer) < 0) {
		MIF_LOG(ERR, "Failed to reply to an add region request");
		return -1;
	}

	return 0;
}

/*
 * Request regions
 * Called by secondary process, when ports link status goes up.
 */
static int
memif_mp_request_regions(struct rte_eth_dev *dev)
{
	int ret, i;
	struct timespec timeout = {.tv_sec = 5, .tv_nsec = 0};
	struct rte_mp_msg msg, *reply;
	struct rte_mp_reply replies;
	struct mp_region_msg *msg_param = (struct mp_region_msg *)msg.param;
	struct mp_region_msg *reply_param;
	struct memif_region *r;
	struct pmd_process_private *proc_private = dev->process_private;
	struct pmd_internals *pmd = dev->data->dev_private;
	/* in case of zero-copy client, only request region 0 */
	uint16_t max_region_num = (pmd->flags & ETH_MEMIF_FLAG_ZERO_COPY) ?
				   1 : ETH_MEMIF_MAX_REGION_NUM;

	MIF_LOG(DEBUG, "Requesting memory regions");

	for (i = 0; i < max_region_num; i++) {
		/* Prepare the message */
		memset(&msg, 0, sizeof(msg));
		strlcpy(msg.name, MEMIF_MP_SEND_REGION, sizeof(msg.name));
		strlcpy(msg_param->port_name, dev->data->name,
			sizeof(msg_param->port_name));
		msg_param->idx = i;
		msg.len_param = sizeof(*msg_param);

		/* Send message */
		ret = rte_mp_request_sync(&msg, &replies, &timeout);
		if (ret < 0 || replies.nb_received != 1) {
			MIF_LOG(ERR, "Failed to send mp msg: %d",
				rte_errno);
			return -1;
		}

		reply = &replies.msgs[0];
		reply_param = (struct mp_region_msg *)reply->param;

		if (reply_param->size > 0) {
			r = rte_zmalloc("region", sizeof(struct memif_region), 0);
			if (r == NULL) {
				MIF_LOG(ERR, "Failed to alloc memif region.");
				free(reply);
				return -ENOMEM;
			}
			r->region_size = reply_param->size;
			if (reply->num_fds < 1) {
				MIF_LOG(ERR, "Missing file descriptor.");
				free(reply);
				return -1;
			}
			r->fd = reply->fds[0];
			r->addr = NULL;

			proc_private->regions[reply_param->idx] = r;
			proc_private->regions_num++;
		}
		free(reply);
	}

	if (pmd->flags & ETH_MEMIF_FLAG_ZERO_COPY) {
		ret = rte_memseg_walk(memif_region_init_zc, (void *)proc_private);
		if (ret < 0)
			return ret;
	}

	return memif_connect(dev);
}

static int
memif_dev_info(struct rte_eth_dev *dev __rte_unused, struct rte_eth_dev_info *dev_info)
{
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = RTE_ETHER_MAX_LEN;
	dev_info->max_rx_queues = ETH_MEMIF_MAX_NUM_Q_PAIRS;
	dev_info->max_tx_queues = ETH_MEMIF_MAX_NUM_Q_PAIRS;
	dev_info->min_rx_bufsize = 0;
	dev_info->tx_offload_capa = RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	return 0;
}

static memif_ring_t *
memif_get_ring(struct pmd_internals *pmd, struct pmd_process_private *proc_private,
	       memif_ring_type_t type, uint16_t ring_num)
{
	/* rings only in region 0 */
	void *p = proc_private->regions[0]->addr;
	int ring_size = sizeof(memif_ring_t) + sizeof(memif_desc_t) *
	    (1 << pmd->run.log2_ring_size);

	p = (uint8_t *)p + (ring_num + type * pmd->run.num_c2s_rings) * ring_size;

	return (memif_ring_t *)p;
}

static memif_region_offset_t
memif_get_ring_offset(struct rte_eth_dev *dev, struct memif_queue *mq,
		      memif_ring_type_t type, uint16_t num)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct pmd_process_private *proc_private = dev->process_private;

	return ((uint8_t *)memif_get_ring(pmd, proc_private, type, num) -
		(uint8_t *)proc_private->regions[mq->region]->addr);
}

static memif_ring_t *
memif_get_ring_from_queue(struct pmd_process_private *proc_private,
			  struct memif_queue *mq)
{
	struct memif_region *r;

	r = proc_private->regions[mq->region];
	if (r == NULL)
		return NULL;

	return (memif_ring_t *)((uint8_t *)r->addr + mq->ring_offset);
}

static void *
memif_get_buffer(struct pmd_process_private *proc_private, memif_desc_t *d)
{
	return ((uint8_t *)proc_private->regions[d->region]->addr + d->offset);
}

/* Free mbufs received by server */
static void
memif_free_stored_mbufs(struct pmd_process_private *proc_private, struct memif_queue *mq)
{
	uint16_t cur_tail;
	uint16_t mask = (1 << mq->log2_ring_size) - 1;
	memif_ring_t *ring = memif_get_ring_from_queue(proc_private, mq);

	/* FIXME: improve performance */
	/* The ring->tail acts as a guard variable between Tx and Rx
	 * threads, so using load-acquire pairs with store-release
	 * in function eth_memif_rx for C2S queues.
	 */
	cur_tail = __atomic_load_n(&ring->tail, __ATOMIC_ACQUIRE);
	while (mq->last_tail != cur_tail) {
		RTE_MBUF_PREFETCH_TO_FREE(mq->buffers[(mq->last_tail + 1) & mask]);
		rte_pktmbuf_free_seg(mq->buffers[mq->last_tail & mask]);
		mq->last_tail++;
	}
}

static int
memif_pktmbuf_chain(struct rte_mbuf *head, struct rte_mbuf *cur_tail,
		    struct rte_mbuf *tail)
{
	/* Check for number-of-segments-overflow */
	if (unlikely(head->nb_segs + tail->nb_segs > RTE_MBUF_MAX_NB_SEGS))
		return -EOVERFLOW;

	/* Chain 'tail' onto the old tail */
	cur_tail->next = tail;

	/* accumulate number of segments and total length. */
	head->nb_segs = (uint16_t)(head->nb_segs + tail->nb_segs);

	tail->pkt_len = tail->data_len;
	head->pkt_len += tail->pkt_len;

	return 0;
}

static uint16_t
eth_memif_rx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct memif_queue *mq = queue;
	struct pmd_internals *pmd = rte_eth_devices[mq->in_port].data->dev_private;
	struct pmd_process_private *proc_private =
		rte_eth_devices[mq->in_port].process_private;
	memif_ring_t *ring = memif_get_ring_from_queue(proc_private, mq);
	uint16_t cur_slot, last_slot, n_slots, ring_size, mask, s0;
	uint16_t pkts, rx_pkts, n_rx_pkts = 0;
	uint16_t mbuf_size = rte_pktmbuf_data_room_size(mq->mempool) -
		RTE_PKTMBUF_HEADROOM;
	uint16_t src_len, src_off, dst_len, dst_off, cp_len;
	memif_ring_type_t type = mq->type;
	memif_desc_t *d0;
	struct rte_mbuf *mbuf, *mbuf_head, *mbuf_tail;
	uint64_t b;
	ssize_t size __rte_unused;
	uint16_t head;
	int ret;
	struct rte_eth_link link;

	if (unlikely((pmd->flags & ETH_MEMIF_FLAG_CONNECTED) == 0))
		return 0;
	if (unlikely(ring == NULL)) {
		/* Secondary process will attempt to request regions. */
		ret = rte_eth_link_get(mq->in_port, &link);
		if (ret < 0)
			MIF_LOG(ERR, "Failed to get port %u link info: %s",
				mq->in_port, rte_strerror(-ret));
		return 0;
	}

	/* consume interrupt */
	if (((ring->flags & MEMIF_RING_FLAG_MASK_INT) == 0) &&
	    (rte_intr_fd_get(mq->intr_handle) >= 0))
		size = read(rte_intr_fd_get(mq->intr_handle), &b,
			    sizeof(b));

	ring_size = 1 << mq->log2_ring_size;
	mask = ring_size - 1;

	if (type == MEMIF_RING_C2S) {
		cur_slot = mq->last_head;
		last_slot = __atomic_load_n(&ring->head, __ATOMIC_ACQUIRE);
	} else {
		cur_slot = mq->last_tail;
		last_slot = __atomic_load_n(&ring->tail, __ATOMIC_ACQUIRE);
	}

	if (cur_slot == last_slot)
		goto refill;
	n_slots = last_slot - cur_slot;

	if (likely(mbuf_size >= pmd->cfg.pkt_buffer_size)) {
		struct rte_mbuf *mbufs[MAX_PKT_BURST];
next_bulk:
		ret = rte_pktmbuf_alloc_bulk(mq->mempool, mbufs, MAX_PKT_BURST);
		if (unlikely(ret < 0))
			goto no_free_bufs;

		rx_pkts = 0;
		pkts = nb_pkts < MAX_PKT_BURST ? nb_pkts : MAX_PKT_BURST;
		while (n_slots && rx_pkts < pkts) {
			mbuf_head = mbufs[rx_pkts];
			mbuf = mbuf_head;

next_slot1:
			mbuf->port = mq->in_port;
			s0 = cur_slot & mask;
			d0 = &ring->desc[s0];

			cp_len = d0->length;

			rte_pktmbuf_data_len(mbuf) = cp_len;
			rte_pktmbuf_pkt_len(mbuf) = cp_len;
			if (mbuf != mbuf_head)
				rte_pktmbuf_pkt_len(mbuf_head) += cp_len;

			rte_memcpy(rte_pktmbuf_mtod(mbuf, void *),
				(uint8_t *)memif_get_buffer(proc_private, d0), cp_len);

			cur_slot++;
			n_slots--;

			if (d0->flags & MEMIF_DESC_FLAG_NEXT) {
				mbuf_tail = mbuf;
				mbuf = rte_pktmbuf_alloc(mq->mempool);
				if (unlikely(mbuf == NULL)) {
					rte_pktmbuf_free_bulk(mbufs + rx_pkts,
							MAX_PKT_BURST - rx_pkts);
					goto no_free_bufs;
				}
				ret = memif_pktmbuf_chain(mbuf_head, mbuf_tail, mbuf);
				if (unlikely(ret < 0)) {
					MIF_LOG(ERR, "number-of-segments-overflow");
					rte_pktmbuf_free(mbuf);
					rte_pktmbuf_free_bulk(mbufs + rx_pkts,
							MAX_PKT_BURST - rx_pkts);
					goto no_free_bufs;
				}
				goto next_slot1;
			}

			mq->n_bytes += rte_pktmbuf_pkt_len(mbuf_head);
			*bufs++ = mbuf_head;
			rx_pkts++;
			n_rx_pkts++;
		}

		if (rx_pkts < MAX_PKT_BURST) {
			rte_pktmbuf_free_bulk(mbufs + rx_pkts, MAX_PKT_BURST - rx_pkts);
		} else {
			nb_pkts -= rx_pkts;
			if (nb_pkts)
				goto next_bulk;
		}
	} else {
		while (n_slots && n_rx_pkts < nb_pkts) {
			mbuf_head = rte_pktmbuf_alloc(mq->mempool);
			if (unlikely(mbuf_head == NULL))
				goto no_free_bufs;
			mbuf = mbuf_head;
			mbuf->port = mq->in_port;

next_slot2:
			s0 = cur_slot & mask;
			d0 = &ring->desc[s0];

			src_len = d0->length;
			dst_off = 0;
			src_off = 0;

			do {
				dst_len = mbuf_size - dst_off;
				if (dst_len == 0) {
					dst_off = 0;
					dst_len = mbuf_size;

					/* store pointer to tail */
					mbuf_tail = mbuf;
					mbuf = rte_pktmbuf_alloc(mq->mempool);
					if (unlikely(mbuf == NULL))
						goto no_free_bufs;
					mbuf->port = mq->in_port;
					ret = memif_pktmbuf_chain(mbuf_head, mbuf_tail, mbuf);
					if (unlikely(ret < 0)) {
						MIF_LOG(ERR, "number-of-segments-overflow");
						rte_pktmbuf_free(mbuf);
						goto no_free_bufs;
					}
				}
				cp_len = RTE_MIN(dst_len, src_len);

				rte_pktmbuf_data_len(mbuf) += cp_len;
				rte_pktmbuf_pkt_len(mbuf) = rte_pktmbuf_data_len(mbuf);
				if (mbuf != mbuf_head)
					rte_pktmbuf_pkt_len(mbuf_head) += cp_len;

				rte_memcpy(rte_pktmbuf_mtod_offset(mbuf, void *,
								   dst_off),
					(uint8_t *)memif_get_buffer(proc_private, d0) +
					src_off, cp_len);

				src_off += cp_len;
				dst_off += cp_len;
				src_len -= cp_len;
			} while (src_len);

			cur_slot++;
			n_slots--;

			if (d0->flags & MEMIF_DESC_FLAG_NEXT)
				goto next_slot2;

			mq->n_bytes += rte_pktmbuf_pkt_len(mbuf_head);
			*bufs++ = mbuf_head;
			n_rx_pkts++;
		}
	}

no_free_bufs:
	if (type == MEMIF_RING_C2S) {
		__atomic_store_n(&ring->tail, cur_slot, __ATOMIC_RELEASE);
		mq->last_head = cur_slot;
	} else {
		mq->last_tail = cur_slot;
	}

refill:
	if (type == MEMIF_RING_S2C) {
		/* ring->head is updated by the receiver and this function
		 * is called in the context of receiver thread. The loads in
		 * the receiver do not need to synchronize with its own stores.
		 */
		head = __atomic_load_n(&ring->head, __ATOMIC_RELAXED);
		n_slots = ring_size - head + mq->last_tail;

		while (n_slots--) {
			s0 = head++ & mask;
			d0 = &ring->desc[s0];
			d0->length = pmd->run.pkt_buffer_size;
		}
		__atomic_store_n(&ring->head, head, __ATOMIC_RELEASE);
	}

	mq->n_pkts += n_rx_pkts;
	return n_rx_pkts;
}

static uint16_t
eth_memif_rx_zc(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct memif_queue *mq = queue;
	struct pmd_internals *pmd = rte_eth_devices[mq->in_port].data->dev_private;
	struct pmd_process_private *proc_private =
		rte_eth_devices[mq->in_port].process_private;
	memif_ring_t *ring = memif_get_ring_from_queue(proc_private, mq);
	uint16_t cur_slot, last_slot, n_slots, ring_size, mask, s0, head;
	uint16_t n_rx_pkts = 0;
	memif_desc_t *d0;
	struct rte_mbuf *mbuf, *mbuf_tail;
	struct rte_mbuf *mbuf_head = NULL;
	int ret;
	struct rte_eth_link link;

	if (unlikely((pmd->flags & ETH_MEMIF_FLAG_CONNECTED) == 0))
		return 0;
	if (unlikely(ring == NULL)) {
		/* Secondary process will attempt to request regions. */
		rte_eth_link_get(mq->in_port, &link);
		return 0;
	}

	/* consume interrupt */
	if ((rte_intr_fd_get(mq->intr_handle) >= 0) &&
	    ((ring->flags & MEMIF_RING_FLAG_MASK_INT) == 0)) {
		uint64_t b;
		ssize_t size __rte_unused;
		size = read(rte_intr_fd_get(mq->intr_handle), &b,
			    sizeof(b));
	}

	ring_size = 1 << mq->log2_ring_size;
	mask = ring_size - 1;

	cur_slot = mq->last_tail;
	/* The ring->tail acts as a guard variable between Tx and Rx
	 * threads, so using load-acquire pairs with store-release
	 * to synchronize it between threads.
	 */
	last_slot = __atomic_load_n(&ring->tail, __ATOMIC_ACQUIRE);
	if (cur_slot == last_slot)
		goto refill;
	n_slots = last_slot - cur_slot;

	while (n_slots && n_rx_pkts < nb_pkts) {
		s0 = cur_slot & mask;

		d0 = &ring->desc[s0];
		mbuf_head = mq->buffers[s0];
		mbuf = mbuf_head;

next_slot:
		/* prefetch next descriptor */
		if (n_rx_pkts + 1 < nb_pkts)
			rte_prefetch0(&ring->desc[(cur_slot + 1) & mask]);

		mbuf->port = mq->in_port;
		rte_pktmbuf_data_len(mbuf) = d0->length;
		rte_pktmbuf_pkt_len(mbuf) = rte_pktmbuf_data_len(mbuf);

		mq->n_bytes += rte_pktmbuf_data_len(mbuf);

		cur_slot++;
		n_slots--;
		if (d0->flags & MEMIF_DESC_FLAG_NEXT) {
			s0 = cur_slot & mask;
			d0 = &ring->desc[s0];
			mbuf_tail = mbuf;
			mbuf = mq->buffers[s0];
			ret = memif_pktmbuf_chain(mbuf_head, mbuf_tail, mbuf);
			if (unlikely(ret < 0)) {
				MIF_LOG(ERR, "number-of-segments-overflow");
				goto refill;
			}
			goto next_slot;
		}

		*bufs++ = mbuf_head;
		n_rx_pkts++;
	}

	mq->last_tail = cur_slot;

/* Supply server with new buffers */
refill:
	/* ring->head is updated by the receiver and this function
	 * is called in the context of receiver thread. The loads in
	 * the receiver do not need to synchronize with its own stores.
	 */
	head = __atomic_load_n(&ring->head, __ATOMIC_RELAXED);
	n_slots = ring_size - head + mq->last_tail;

	if (n_slots < 32)
		goto no_free_mbufs;

	ret = rte_pktmbuf_alloc_bulk(mq->mempool, &mq->buffers[head & mask], n_slots);
	if (unlikely(ret < 0))
		goto no_free_mbufs;

	while (n_slots--) {
		s0 = head++ & mask;
		if (n_slots > 0)
			rte_prefetch0(mq->buffers[head & mask]);
		d0 = &ring->desc[s0];
		/* store buffer header */
		mbuf = mq->buffers[s0];
		/* populate descriptor */
		d0->length = rte_pktmbuf_data_room_size(mq->mempool) -
				RTE_PKTMBUF_HEADROOM;
		d0->region = 1;
		d0->offset = rte_pktmbuf_mtod(mbuf, uint8_t *) -
			(uint8_t *)proc_private->regions[d0->region]->addr;
	}
no_free_mbufs:
	/* The ring->head acts as a guard variable between Tx and Rx
	 * threads, so using store-release pairs with load-acquire
	 * in function eth_memif_tx.
	 */
	__atomic_store_n(&ring->head, head, __ATOMIC_RELEASE);

	mq->n_pkts += n_rx_pkts;

	return n_rx_pkts;
}

static uint16_t
eth_memif_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct memif_queue *mq = queue;
	struct pmd_internals *pmd = rte_eth_devices[mq->in_port].data->dev_private;
	struct pmd_process_private *proc_private =
		rte_eth_devices[mq->in_port].process_private;
	memif_ring_t *ring = memif_get_ring_from_queue(proc_private, mq);
	uint16_t slot, saved_slot, n_free, ring_size, mask, n_tx_pkts = 0;
	uint16_t src_len, src_off, dst_len, dst_off, cp_len, nb_segs;
	memif_ring_type_t type = mq->type;
	memif_desc_t *d0;
	struct rte_mbuf *mbuf;
	struct rte_mbuf *mbuf_head;
	uint64_t a;
	ssize_t size;
	struct rte_eth_link link;

	if (unlikely((pmd->flags & ETH_MEMIF_FLAG_CONNECTED) == 0))
		return 0;
	if (unlikely(ring == NULL)) {
		int ret;

		/* Secondary process will attempt to request regions. */
		ret = rte_eth_link_get(mq->in_port, &link);
		if (ret < 0)
			MIF_LOG(ERR, "Failed to get port %u link info: %s",
				mq->in_port, rte_strerror(-ret));
		return 0;
	}

	ring_size = 1 << mq->log2_ring_size;
	mask = ring_size - 1;

	if (type == MEMIF_RING_C2S) {
		/* For C2S queues ring->head is updated by the sender and
		 * this function is called in the context of sending thread.
		 * The loads in the sender do not need to synchronize with
		 * its own stores. Hence, the following load can be a
		 * relaxed load.
		 */
		slot = __atomic_load_n(&ring->head, __ATOMIC_RELAXED);
		n_free = ring_size - slot +
				__atomic_load_n(&ring->tail, __ATOMIC_ACQUIRE);
	} else {
		/* For S2C queues ring->tail is updated by the sender and
		 * this function is called in the context of sending thread.
		 * The loads in the sender do not need to synchronize with
		 * its own stores. Hence, the following load can be a
		 * relaxed load.
		 */
		slot = __atomic_load_n(&ring->tail, __ATOMIC_RELAXED);
		n_free = __atomic_load_n(&ring->head, __ATOMIC_ACQUIRE) - slot;
	}

	uint16_t i;
	struct rte_mbuf **buf_tmp = bufs;
	mbuf_head = *buf_tmp++;
	struct rte_mempool *mp = mbuf_head->pool;

	for (i = 1; i < nb_pkts; i++) {
		mbuf_head = *buf_tmp++;
		if (mbuf_head->pool != mp)
			break;
	}

	uint16_t mbuf_size = rte_pktmbuf_data_room_size(mp) - RTE_PKTMBUF_HEADROOM;
	if (i == nb_pkts && pmd->cfg.pkt_buffer_size >= mbuf_size) {
		buf_tmp = bufs;
		while (n_tx_pkts < nb_pkts && n_free) {
			mbuf_head = *bufs++;
			nb_segs = mbuf_head->nb_segs;
			mbuf = mbuf_head;

			saved_slot = slot;

next_in_chain1:
			d0 = &ring->desc[slot & mask];
			cp_len = rte_pktmbuf_data_len(mbuf);

			rte_memcpy((uint8_t *)memif_get_buffer(proc_private, d0),
				rte_pktmbuf_mtod(mbuf, void *), cp_len);

			d0->length = cp_len;
			mq->n_bytes += cp_len;
			slot++;
			n_free--;

			if (--nb_segs > 0) {
				if (n_free) {
					d0->flags |= MEMIF_DESC_FLAG_NEXT;
					mbuf = mbuf->next;
					goto next_in_chain1;
				} else {
					slot = saved_slot;
					goto free_mbufs;
				}
			}

			n_tx_pkts++;
		}
free_mbufs:
		rte_pktmbuf_free_bulk(buf_tmp, n_tx_pkts);
	} else {
		while (n_tx_pkts < nb_pkts && n_free) {
			mbuf_head = *bufs++;
			nb_segs = mbuf_head->nb_segs;
			mbuf = mbuf_head;

			saved_slot = slot;
			d0 = &ring->desc[slot & mask];
			dst_off = 0;
			dst_len = (type == MEMIF_RING_C2S) ?
				pmd->run.pkt_buffer_size : d0->length;

next_in_chain2:
			src_off = 0;
			src_len = rte_pktmbuf_data_len(mbuf);

			while (src_len) {
				if (dst_len == 0) {
					if (n_free) {
						slot++;
						n_free--;
						d0->flags |= MEMIF_DESC_FLAG_NEXT;
						d0 = &ring->desc[slot & mask];
						dst_off = 0;
						dst_len = (type == MEMIF_RING_C2S) ?
						    pmd->run.pkt_buffer_size : d0->length;
						d0->flags = 0;
					} else {
						slot = saved_slot;
						goto no_free_slots;
					}
				}
				cp_len = RTE_MIN(dst_len, src_len);

				rte_memcpy((uint8_t *)memif_get_buffer(proc_private,
								       d0) + dst_off,
					rte_pktmbuf_mtod_offset(mbuf, void *, src_off),
					cp_len);

				mq->n_bytes += cp_len;
				src_off += cp_len;
				dst_off += cp_len;
				src_len -= cp_len;
				dst_len -= cp_len;

				d0->length = dst_off;
			}

			if (--nb_segs > 0) {
				mbuf = mbuf->next;
				goto next_in_chain2;
			}

			n_tx_pkts++;
			slot++;
			n_free--;
			rte_pktmbuf_free(mbuf_head);
		}
	}

no_free_slots:
	if (type == MEMIF_RING_C2S)
		__atomic_store_n(&ring->head, slot, __ATOMIC_RELEASE);
	else
		__atomic_store_n(&ring->tail, slot, __ATOMIC_RELEASE);

	if (((ring->flags & MEMIF_RING_FLAG_MASK_INT) == 0) &&
	    (rte_intr_fd_get(mq->intr_handle) >= 0)) {
		a = 1;
		size = write(rte_intr_fd_get(mq->intr_handle), &a,
			     sizeof(a));
		if (unlikely(size < 0)) {
			MIF_LOG(WARNING,
				"Failed to send interrupt. %s", strerror(errno));
		}
	}

	mq->n_pkts += n_tx_pkts;
	return n_tx_pkts;
}

static int
memif_tx_one_zc(struct pmd_process_private *proc_private, struct memif_queue *mq,
		memif_ring_t *ring, struct rte_mbuf *mbuf, const uint16_t mask,
		uint16_t slot, uint16_t n_free)
{
	memif_desc_t *d0;
	uint16_t nb_segs = mbuf->nb_segs;
	int used_slots = 1;

next_in_chain:
	/* store pointer to mbuf to free it later */
	mq->buffers[slot & mask] = mbuf;
	/* populate descriptor */
	d0 = &ring->desc[slot & mask];
	d0->length = rte_pktmbuf_data_len(mbuf);
	mq->n_bytes += rte_pktmbuf_data_len(mbuf);
	/* FIXME: get region index */
	d0->region = 1;
	d0->offset = rte_pktmbuf_mtod(mbuf, uint8_t *) -
		(uint8_t *)proc_private->regions[d0->region]->addr;
	d0->flags = 0;

	/* check if buffer is chained */
	if (--nb_segs > 0) {
		if (n_free < 2)
			return 0;
		/* mark buffer as chained */
		d0->flags |= MEMIF_DESC_FLAG_NEXT;
		/* advance mbuf */
		mbuf = mbuf->next;
		/* update counters */
		used_slots++;
		slot++;
		n_free--;
		goto next_in_chain;
	}
	return used_slots;
}

static uint16_t
eth_memif_tx_zc(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct memif_queue *mq = queue;
	struct pmd_internals *pmd = rte_eth_devices[mq->in_port].data->dev_private;
	struct pmd_process_private *proc_private =
		rte_eth_devices[mq->in_port].process_private;
	memif_ring_t *ring = memif_get_ring_from_queue(proc_private, mq);
	uint16_t slot, n_free, ring_size, mask, n_tx_pkts = 0;
	struct rte_eth_link link;

	if (unlikely((pmd->flags & ETH_MEMIF_FLAG_CONNECTED) == 0))
		return 0;
	if (unlikely(ring == NULL)) {
		/* Secondary process will attempt to request regions. */
		rte_eth_link_get(mq->in_port, &link);
		return 0;
	}

	ring_size = 1 << mq->log2_ring_size;
	mask = ring_size - 1;

	/* free mbufs received by server */
	memif_free_stored_mbufs(proc_private, mq);

	/* ring type always MEMIF_RING_C2S */
	/* For C2S queues ring->head is updated by the sender and
	 * this function is called in the context of sending thread.
	 * The loads in the sender do not need to synchronize with
	 * its own stores. Hence, the following load can be a
	 * relaxed load.
	 */
	slot = __atomic_load_n(&ring->head, __ATOMIC_RELAXED);
	n_free = ring_size - slot + mq->last_tail;

	int used_slots;

	while (n_free && (n_tx_pkts < nb_pkts)) {
		while ((n_free > 4) && ((nb_pkts - n_tx_pkts) > 4)) {
			if ((nb_pkts - n_tx_pkts) > 8) {
				rte_prefetch0(*bufs + 4);
				rte_prefetch0(*bufs + 5);
				rte_prefetch0(*bufs + 6);
				rte_prefetch0(*bufs + 7);
			}
			used_slots = memif_tx_one_zc(proc_private, mq, ring, *bufs++,
				mask, slot, n_free);
			if (unlikely(used_slots < 1))
				goto no_free_slots;
			n_tx_pkts++;
			slot += used_slots;
			n_free -= used_slots;

			used_slots = memif_tx_one_zc(proc_private, mq, ring, *bufs++,
				mask, slot, n_free);
			if (unlikely(used_slots < 1))
				goto no_free_slots;
			n_tx_pkts++;
			slot += used_slots;
			n_free -= used_slots;

			used_slots = memif_tx_one_zc(proc_private, mq, ring, *bufs++,
				mask, slot, n_free);
			if (unlikely(used_slots < 1))
				goto no_free_slots;
			n_tx_pkts++;
			slot += used_slots;
			n_free -= used_slots;

			used_slots = memif_tx_one_zc(proc_private, mq, ring, *bufs++,
				mask, slot, n_free);
			if (unlikely(used_slots < 1))
				goto no_free_slots;
			n_tx_pkts++;
			slot += used_slots;
			n_free -= used_slots;
		}
		used_slots = memif_tx_one_zc(proc_private, mq, ring, *bufs++,
			mask, slot, n_free);
		if (unlikely(used_slots < 1))
			goto no_free_slots;
		n_tx_pkts++;
		slot += used_slots;
		n_free -= used_slots;
	}

no_free_slots:
	/* ring type always MEMIF_RING_C2S */
	/* The ring->head acts as a guard variable between Tx and Rx
	 * threads, so using store-release pairs with load-acquire
	 * in function eth_memif_rx for C2S rings.
	 */
	__atomic_store_n(&ring->head, slot, __ATOMIC_RELEASE);

	/* Send interrupt, if enabled. */
	if ((ring->flags & MEMIF_RING_FLAG_MASK_INT) == 0) {
		uint64_t a = 1;
		if (rte_intr_fd_get(mq->intr_handle) < 0)
			return -1;

		ssize_t size = write(rte_intr_fd_get(mq->intr_handle),
				     &a, sizeof(a));
		if (unlikely(size < 0)) {
			MIF_LOG(WARNING,
				"Failed to send interrupt. %s", strerror(errno));
		}
	}

	/* increment queue counters */
	mq->n_pkts += n_tx_pkts;

	return n_tx_pkts;
}

void
memif_free_regions(struct rte_eth_dev *dev)
{
	struct pmd_process_private *proc_private = dev->process_private;
	struct pmd_internals *pmd = dev->data->dev_private;
	int i;
	struct memif_region *r;

	/* regions are allocated contiguously, so it's
	 * enough to loop until 'proc_private->regions_num'
	 */
	for (i = 0; i < proc_private->regions_num; i++) {
		r = proc_private->regions[i];
		if (r != NULL) {
			/* This is memzone */
			if (i > 0 && (pmd->flags & ETH_MEMIF_FLAG_ZERO_COPY)) {
				r->addr = NULL;
				if (r->fd > 0)
					close(r->fd);
			}
			if (r->addr != NULL) {
				munmap(r->addr, r->region_size);
				if (r->fd > 0) {
					close(r->fd);
					r->fd = -1;
				}
			}
			rte_free(r);
			proc_private->regions[i] = NULL;
		}
	}
	proc_private->regions_num = 0;
}

static int
memif_region_init_zc(const struct rte_memseg_list *msl, const struct rte_memseg *ms,
		     void *arg)
{
	struct pmd_process_private *proc_private = (struct pmd_process_private *)arg;
	struct memif_region *r;

	if (proc_private->regions_num < 1) {
		MIF_LOG(ERR, "Missing descriptor region");
		return -1;
	}

	r = proc_private->regions[proc_private->regions_num - 1];

	if (r->addr != msl->base_va)
		r = proc_private->regions[++proc_private->regions_num - 1];

	if (r == NULL) {
		r = rte_zmalloc("region", sizeof(struct memif_region), 0);
		if (r == NULL) {
			MIF_LOG(ERR, "Failed to alloc memif region.");
			return -ENOMEM;
		}

		r->addr = msl->base_va;
		r->region_size = ms->len;
		r->fd = rte_memseg_get_fd(ms);
		if (r->fd < 0)
			return -1;
		r->pkt_buffer_offset = 0;

		proc_private->regions[proc_private->regions_num - 1] = r;
	} else {
		r->region_size += ms->len;
	}

	return 0;
}

static int
memif_region_init_shm(struct rte_eth_dev *dev, uint8_t has_buffers)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct pmd_process_private *proc_private = dev->process_private;
	char shm_name[ETH_MEMIF_SHM_NAME_SIZE];
	int ret = 0;
	struct memif_region *r;

	if (proc_private->regions_num >= ETH_MEMIF_MAX_REGION_NUM) {
		MIF_LOG(ERR, "Too many regions.");
		return -1;
	}

	r = rte_zmalloc("region", sizeof(struct memif_region), 0);
	if (r == NULL) {
		MIF_LOG(ERR, "Failed to alloc memif region.");
		return -ENOMEM;
	}

	/* calculate buffer offset */
	r->pkt_buffer_offset = (pmd->run.num_c2s_rings + pmd->run.num_s2c_rings) *
	    (sizeof(memif_ring_t) + sizeof(memif_desc_t) *
	    (1 << pmd->run.log2_ring_size));

	r->region_size = r->pkt_buffer_offset;
	/* if region has buffers, add buffers size to region_size */
	if (has_buffers == 1)
		r->region_size += (uint32_t)(pmd->run.pkt_buffer_size *
			(1 << pmd->run.log2_ring_size) *
			(pmd->run.num_c2s_rings +
			 pmd->run.num_s2c_rings));

	memset(shm_name, 0, sizeof(char) * ETH_MEMIF_SHM_NAME_SIZE);
	snprintf(shm_name, ETH_MEMIF_SHM_NAME_SIZE, "memif_region_%d",
		 proc_private->regions_num);

	r->fd = memfd_create(shm_name, MFD_ALLOW_SEALING);
	if (r->fd < 0) {
		MIF_LOG(ERR, "Failed to create shm file: %s.", strerror(errno));
		ret = -1;
		goto error;
	}

	ret = fcntl(r->fd, F_ADD_SEALS, F_SEAL_SHRINK);
	if (ret < 0) {
		MIF_LOG(ERR, "Failed to add seals to shm file: %s.", strerror(errno));
		goto error;
	}

	ret = ftruncate(r->fd, r->region_size);
	if (ret < 0) {
		MIF_LOG(ERR, "Failed to truncate shm file: %s.", strerror(errno));
		goto error;
	}

	r->addr = mmap(NULL, r->region_size, PROT_READ |
		       PROT_WRITE, MAP_SHARED, r->fd, 0);
	if (r->addr == MAP_FAILED) {
		MIF_LOG(ERR, "Failed to mmap shm region: %s.", strerror(ret));
		ret = -1;
		goto error;
	}

	proc_private->regions[proc_private->regions_num] = r;
	proc_private->regions_num++;

	return ret;

error:
	if (r->fd > 0)
		close(r->fd);
	r->fd = -1;

	return ret;
}

static int
memif_regions_init(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	int ret;

	/*
	 * Zero-copy exposes dpdk memory.
	 * Each memseg list will be represented by memif region.
	 * Zero-copy regions indexing: memseg list idx + 1,
	 * as we already have region 0 reserved for descriptors.
	 */
	if (pmd->flags & ETH_MEMIF_FLAG_ZERO_COPY) {
		/* create region idx 0 containing descriptors */
		ret = memif_region_init_shm(dev, 0);
		if (ret < 0)
			return ret;
		ret = rte_memseg_walk(memif_region_init_zc, (void *)dev->process_private);
		if (ret < 0)
			return ret;
	} else {
		/* create one memory region containing rings and buffers */
		ret = memif_region_init_shm(dev, /* has buffers */ 1);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void
memif_init_rings(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct pmd_process_private *proc_private = dev->process_private;
	memif_ring_t *ring;
	int i, j;
	uint16_t slot;

	for (i = 0; i < pmd->run.num_c2s_rings; i++) {
		ring = memif_get_ring(pmd, proc_private, MEMIF_RING_C2S, i);
		__atomic_store_n(&ring->head, 0, __ATOMIC_RELAXED);
		__atomic_store_n(&ring->tail, 0, __ATOMIC_RELAXED);
		ring->cookie = MEMIF_COOKIE;
		ring->flags = 0;

		if (pmd->flags & ETH_MEMIF_FLAG_ZERO_COPY)
			continue;

		for (j = 0; j < (1 << pmd->run.log2_ring_size); j++) {
			slot = i * (1 << pmd->run.log2_ring_size) + j;
			ring->desc[j].region = 0;
			ring->desc[j].offset =
				proc_private->regions[0]->pkt_buffer_offset +
				(uint32_t)(slot * pmd->run.pkt_buffer_size);
			ring->desc[j].length = pmd->run.pkt_buffer_size;
		}
	}

	for (i = 0; i < pmd->run.num_s2c_rings; i++) {
		ring = memif_get_ring(pmd, proc_private, MEMIF_RING_S2C, i);
		__atomic_store_n(&ring->head, 0, __ATOMIC_RELAXED);
		__atomic_store_n(&ring->tail, 0, __ATOMIC_RELAXED);
		ring->cookie = MEMIF_COOKIE;
		ring->flags = 0;

		if (pmd->flags & ETH_MEMIF_FLAG_ZERO_COPY)
			continue;

		for (j = 0; j < (1 << pmd->run.log2_ring_size); j++) {
			slot = (i + pmd->run.num_c2s_rings) *
			    (1 << pmd->run.log2_ring_size) + j;
			ring->desc[j].region = 0;
			ring->desc[j].offset =
				proc_private->regions[0]->pkt_buffer_offset +
				(uint32_t)(slot * pmd->run.pkt_buffer_size);
			ring->desc[j].length = pmd->run.pkt_buffer_size;
		}
	}
}

/* called only by client */
static int
memif_init_queues(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_queue *mq;
	int i;

	for (i = 0; i < pmd->run.num_c2s_rings; i++) {
		mq = dev->data->tx_queues[i];
		mq->log2_ring_size = pmd->run.log2_ring_size;
		/* queues located only in region 0 */
		mq->region = 0;
		mq->ring_offset = memif_get_ring_offset(dev, mq, MEMIF_RING_C2S, i);
		mq->last_head = 0;
		mq->last_tail = 0;
		if (rte_intr_fd_set(mq->intr_handle, eventfd(0, EFD_NONBLOCK)))
			return -rte_errno;

		if (rte_intr_fd_get(mq->intr_handle) < 0) {
			MIF_LOG(WARNING,
				"Failed to create eventfd for tx queue %d: %s.", i,
				strerror(errno));
		}
		mq->buffers = NULL;
		if (pmd->flags & ETH_MEMIF_FLAG_ZERO_COPY) {
			mq->buffers = rte_zmalloc("bufs", sizeof(struct rte_mbuf *) *
						  (1 << mq->log2_ring_size), 0);
			if (mq->buffers == NULL)
				return -ENOMEM;
		}
	}

	for (i = 0; i < pmd->run.num_s2c_rings; i++) {
		mq = dev->data->rx_queues[i];
		mq->log2_ring_size = pmd->run.log2_ring_size;
		/* queues located only in region 0 */
		mq->region = 0;
		mq->ring_offset = memif_get_ring_offset(dev, mq, MEMIF_RING_S2C, i);
		mq->last_head = 0;
		mq->last_tail = 0;
		if (rte_intr_fd_set(mq->intr_handle, eventfd(0, EFD_NONBLOCK)))
			return -rte_errno;
		if (rte_intr_fd_get(mq->intr_handle) < 0) {
			MIF_LOG(WARNING,
				"Failed to create eventfd for rx queue %d: %s.", i,
				strerror(errno));
		}
		mq->buffers = NULL;
		if (pmd->flags & ETH_MEMIF_FLAG_ZERO_COPY) {
			mq->buffers = rte_zmalloc("bufs", sizeof(struct rte_mbuf *) *
						  (1 << mq->log2_ring_size), 0);
			if (mq->buffers == NULL)
				return -ENOMEM;
		}
	}
	return 0;
}

int
memif_init_regions_and_queues(struct rte_eth_dev *dev)
{
	int ret;

	ret = memif_regions_init(dev);
	if (ret < 0)
		return ret;

	memif_init_rings(dev);

	ret = memif_init_queues(dev);
	if (ret < 0)
		return ret;

	return 0;
}

int
memif_connect(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct pmd_process_private *proc_private = dev->process_private;
	struct memif_region *mr;
	struct memif_queue *mq;
	memif_ring_t *ring;
	int i;

	for (i = 0; i < proc_private->regions_num; i++) {
		mr = proc_private->regions[i];
		if (mr != NULL) {
			if (mr->addr == NULL) {
				if (mr->fd < 0)
					return -1;
				mr->addr = mmap(NULL, mr->region_size,
						PROT_READ | PROT_WRITE,
						MAP_SHARED, mr->fd, 0);
				if (mr->addr == MAP_FAILED) {
					MIF_LOG(ERR, "mmap failed: %s\n",
						strerror(errno));
					return -1;
				}
			}
			if (i > 0 && (pmd->flags & ETH_MEMIF_FLAG_ZERO_COPY)) {
				/* close memseg file */
				close(mr->fd);
				mr->fd = -1;
			}
		}
	}

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		for (i = 0; i < pmd->run.num_c2s_rings; i++) {
			mq = (pmd->role == MEMIF_ROLE_CLIENT) ?
			    dev->data->tx_queues[i] : dev->data->rx_queues[i];
			ring = memif_get_ring_from_queue(proc_private, mq);
			if (ring == NULL || ring->cookie != MEMIF_COOKIE) {
				MIF_LOG(ERR, "Wrong ring");
				return -1;
			}
			__atomic_store_n(&ring->head, 0, __ATOMIC_RELAXED);
			__atomic_store_n(&ring->tail, 0, __ATOMIC_RELAXED);
			mq->last_head = 0;
			mq->last_tail = 0;
			/* enable polling mode */
			if (pmd->role == MEMIF_ROLE_SERVER)
				ring->flags = MEMIF_RING_FLAG_MASK_INT;
		}
		for (i = 0; i < pmd->run.num_s2c_rings; i++) {
			mq = (pmd->role == MEMIF_ROLE_CLIENT) ?
			    dev->data->rx_queues[i] : dev->data->tx_queues[i];
			ring = memif_get_ring_from_queue(proc_private, mq);
			if (ring == NULL || ring->cookie != MEMIF_COOKIE) {
				MIF_LOG(ERR, "Wrong ring");
				return -1;
			}
			__atomic_store_n(&ring->head, 0, __ATOMIC_RELAXED);
			__atomic_store_n(&ring->tail, 0, __ATOMIC_RELAXED);
			mq->last_head = 0;
			mq->last_tail = 0;
			/* enable polling mode */
			if (pmd->role == MEMIF_ROLE_CLIENT)
				ring->flags = MEMIF_RING_FLAG_MASK_INT;
		}

		pmd->flags &= ~ETH_MEMIF_FLAG_CONNECTING;
		pmd->flags |= ETH_MEMIF_FLAG_CONNECTED;
		dev->data->dev_link.link_status = RTE_ETH_LINK_UP;
	}
	MIF_LOG(INFO, "Connected.");
	return 0;
}

static int
memif_dev_start(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	int ret = 0;
	uint16_t i;

	switch (pmd->role) {
	case MEMIF_ROLE_CLIENT:
		ret = memif_connect_client(dev);
		break;
	case MEMIF_ROLE_SERVER:
		ret = memif_connect_server(dev);
		break;
	default:
		MIF_LOG(ERR, "Unknown role: %d.", pmd->role);
		ret = -1;
		break;
	}

	if (ret == 0) {
		for (i = 0; i < dev->data->nb_rx_queues; i++)
			dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
		for (i = 0; i < dev->data->nb_tx_queues; i++)
			dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	}

	return ret;
}

static int
memif_dev_stop(struct rte_eth_dev *dev)
{
	uint16_t i;

	memif_disconnect(dev);

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static int
memif_dev_close(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	int i;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		memif_msg_enq_disconnect(pmd->cc, "Device closed", 0);

		for (i = 0; i < dev->data->nb_rx_queues; i++)
			(*dev->dev_ops->rx_queue_release)(dev, i);
		for (i = 0; i < dev->data->nb_tx_queues; i++)
			(*dev->dev_ops->tx_queue_release)(dev, i);

		memif_socket_remove_device(dev);
	}

	rte_free(dev->process_private);

	return 0;
}

static int
memif_dev_configure(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;

	/*
	 * CLIENT - TXQ
	 * SERVER - RXQ
	 */
	pmd->cfg.num_c2s_rings = (pmd->role == MEMIF_ROLE_CLIENT) ?
				  dev->data->nb_tx_queues : dev->data->nb_rx_queues;

	/*
	 * CLIENT - RXQ
	 * SERVER - TXQ
	 */
	pmd->cfg.num_s2c_rings = (pmd->role == MEMIF_ROLE_CLIENT) ?
				  dev->data->nb_rx_queues : dev->data->nb_tx_queues;

	return 0;
}

static int
memif_tx_queue_setup(struct rte_eth_dev *dev,
		     uint16_t qid,
		     uint16_t nb_tx_desc __rte_unused,
		     unsigned int socket_id __rte_unused,
		     const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_queue *mq;

	mq = rte_zmalloc("tx-queue", sizeof(struct memif_queue), 0);
	if (mq == NULL) {
		MIF_LOG(ERR, "Failed to allocate tx queue id: %u", qid);
		return -ENOMEM;
	}

	/* Allocate interrupt instance */
	mq->intr_handle = rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_SHARED);
	if (mq->intr_handle == NULL) {
		MIF_LOG(ERR, "Failed to allocate intr handle");
		return -ENOMEM;
	}

	mq->type =
	    (pmd->role == MEMIF_ROLE_CLIENT) ? MEMIF_RING_C2S : MEMIF_RING_S2C;
	mq->n_pkts = 0;
	mq->n_bytes = 0;

	if (rte_intr_fd_set(mq->intr_handle, -1))
		return -rte_errno;

	if (rte_intr_type_set(mq->intr_handle, RTE_INTR_HANDLE_EXT))
		return -rte_errno;

	mq->in_port = dev->data->port_id;
	dev->data->tx_queues[qid] = mq;

	return 0;
}

static int
memif_rx_queue_setup(struct rte_eth_dev *dev,
		     uint16_t qid,
		     uint16_t nb_rx_desc __rte_unused,
		     unsigned int socket_id __rte_unused,
		     const struct rte_eth_rxconf *rx_conf __rte_unused,
		     struct rte_mempool *mb_pool)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_queue *mq;

	mq = rte_zmalloc("rx-queue", sizeof(struct memif_queue), 0);
	if (mq == NULL) {
		MIF_LOG(ERR, "Failed to allocate rx queue id: %u", qid);
		return -ENOMEM;
	}

	/* Allocate interrupt instance */
	mq->intr_handle = rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_SHARED);
	if (mq->intr_handle == NULL) {
		MIF_LOG(ERR, "Failed to allocate intr handle");
		return -ENOMEM;
	}

	mq->type = (pmd->role == MEMIF_ROLE_CLIENT) ? MEMIF_RING_S2C : MEMIF_RING_C2S;
	mq->n_pkts = 0;
	mq->n_bytes = 0;

	if (rte_intr_fd_set(mq->intr_handle, -1))
		return -rte_errno;

	if (rte_intr_type_set(mq->intr_handle, RTE_INTR_HANDLE_EXT))
		return -rte_errno;

	mq->mempool = mb_pool;
	mq->in_port = dev->data->port_id;
	dev->data->rx_queues[qid] = mq;

	return 0;
}

static void
memif_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct memif_queue *mq = dev->data->rx_queues[qid];

	if (!mq)
		return;

	rte_intr_instance_free(mq->intr_handle);
	rte_free(mq);
}

static void
memif_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct memif_queue *mq = dev->data->tx_queues[qid];

	if (!mq)
		return;

	rte_free(mq);
}

static int
memif_link_update(struct rte_eth_dev *dev,
		  int wait_to_complete __rte_unused)
{
	struct pmd_process_private *proc_private;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		proc_private = dev->process_private;
		if (dev->data->dev_link.link_status == RTE_ETH_LINK_UP &&
				proc_private->regions_num == 0) {
			memif_mp_request_regions(dev);
		} else if (dev->data->dev_link.link_status == RTE_ETH_LINK_DOWN &&
				proc_private->regions_num > 0) {
			memif_free_regions(dev);
		}
	}
	return 0;
}

static int
memif_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_queue *mq;
	int i;
	uint8_t tmp, nq;

	stats->ipackets = 0;
	stats->ibytes = 0;
	stats->opackets = 0;
	stats->obytes = 0;

	tmp = (pmd->role == MEMIF_ROLE_CLIENT) ? pmd->run.num_s2c_rings :
	    pmd->run.num_c2s_rings;
	nq = (tmp < RTE_ETHDEV_QUEUE_STAT_CNTRS) ? tmp :
	    RTE_ETHDEV_QUEUE_STAT_CNTRS;

	/* RX stats */
	for (i = 0; i < nq; i++) {
		mq = dev->data->rx_queues[i];
		stats->q_ipackets[i] = mq->n_pkts;
		stats->q_ibytes[i] = mq->n_bytes;
		stats->ipackets += mq->n_pkts;
		stats->ibytes += mq->n_bytes;
	}

	tmp = (pmd->role == MEMIF_ROLE_CLIENT) ? pmd->run.num_c2s_rings :
	    pmd->run.num_s2c_rings;
	nq = (tmp < RTE_ETHDEV_QUEUE_STAT_CNTRS) ? tmp :
	    RTE_ETHDEV_QUEUE_STAT_CNTRS;

	/* TX stats */
	for (i = 0; i < nq; i++) {
		mq = dev->data->tx_queues[i];
		stats->q_opackets[i] = mq->n_pkts;
		stats->q_obytes[i] = mq->n_bytes;
		stats->opackets += mq->n_pkts;
		stats->obytes += mq->n_bytes;
	}
	return 0;
}

static int
memif_stats_reset(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	int i;
	struct memif_queue *mq;

	for (i = 0; i < pmd->run.num_c2s_rings; i++) {
		mq = (pmd->role == MEMIF_ROLE_CLIENT) ? dev->data->tx_queues[i] :
		    dev->data->rx_queues[i];
		mq->n_pkts = 0;
		mq->n_bytes = 0;
	}
	for (i = 0; i < pmd->run.num_s2c_rings; i++) {
		mq = (pmd->role == MEMIF_ROLE_CLIENT) ? dev->data->rx_queues[i] :
		    dev->data->tx_queues[i];
		mq->n_pkts = 0;
		mq->n_bytes = 0;
	}

	return 0;
}

static const struct eth_dev_ops ops = {
	.dev_start = memif_dev_start,
	.dev_stop = memif_dev_stop,
	.dev_close = memif_dev_close,
	.dev_infos_get = memif_dev_info,
	.dev_configure = memif_dev_configure,
	.tx_queue_setup = memif_tx_queue_setup,
	.rx_queue_setup = memif_rx_queue_setup,
	.rx_queue_release = memif_rx_queue_release,
	.tx_queue_release = memif_tx_queue_release,
	.link_update = memif_link_update,
	.stats_get = memif_stats_get,
	.stats_reset = memif_stats_reset,
};

static int
memif_create(struct rte_vdev_device *vdev, enum memif_role_t role,
	     memif_interface_id_t id, uint32_t flags,
	     const char *socket_filename, uid_t owner_uid, gid_t owner_gid,
	     memif_log2_ring_size_t log2_ring_size,
	     uint16_t pkt_buffer_size, const char *secret,
	     struct rte_ether_addr *ether_addr)
{
	int ret = 0;
	struct rte_eth_dev *eth_dev;
	struct rte_eth_dev_data *data;
	struct pmd_internals *pmd;
	struct pmd_process_private *process_private;
	const unsigned int numa_node = vdev->device.numa_node;
	const char *name = rte_vdev_device_name(vdev);

	eth_dev = rte_eth_vdev_allocate(vdev, sizeof(*pmd));
	if (eth_dev == NULL) {
		MIF_LOG(ERR, "%s: Unable to allocate device struct.", name);
		return -1;
	}

	process_private = (struct pmd_process_private *)
		rte_zmalloc(name, sizeof(struct pmd_process_private),
			    RTE_CACHE_LINE_SIZE);

	if (process_private == NULL) {
		MIF_LOG(ERR, "Failed to alloc memory for process private");
		return -1;
	}
	eth_dev->process_private = process_private;

	pmd = eth_dev->data->dev_private;
	memset(pmd, 0, sizeof(*pmd));

	pmd->id = id;
	pmd->flags = flags;
	pmd->flags |= ETH_MEMIF_FLAG_DISABLED;
	pmd->role = role;
	/* Zero-copy flag irelevant to server. */
	if (pmd->role == MEMIF_ROLE_SERVER)
		pmd->flags &= ~ETH_MEMIF_FLAG_ZERO_COPY;
	pmd->owner_uid = owner_uid;
	pmd->owner_gid = owner_gid;

	ret = memif_socket_init(eth_dev, socket_filename);
	if (ret < 0)
		return ret;

	memset(pmd->secret, 0, sizeof(char) * ETH_MEMIF_SECRET_SIZE);
	if (secret != NULL)
		strlcpy(pmd->secret, secret, sizeof(pmd->secret));

	pmd->cfg.log2_ring_size = log2_ring_size;
	/* set in .dev_configure() */
	pmd->cfg.num_c2s_rings = 0;
	pmd->cfg.num_s2c_rings = 0;

	pmd->cfg.pkt_buffer_size = pkt_buffer_size;
	rte_spinlock_init(&pmd->cc_lock);

	data = eth_dev->data;
	data->dev_private = pmd;
	data->numa_node = numa_node;
	data->dev_link = pmd_link;
	data->mac_addrs = ether_addr;
	data->promiscuous = 1;
	data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	eth_dev->dev_ops = &ops;
	eth_dev->device = &vdev->device;
	if (pmd->flags & ETH_MEMIF_FLAG_ZERO_COPY) {
		eth_dev->rx_pkt_burst = eth_memif_rx_zc;
		eth_dev->tx_pkt_burst = eth_memif_tx_zc;
	} else {
		eth_dev->rx_pkt_burst = eth_memif_rx;
		eth_dev->tx_pkt_burst = eth_memif_tx;
	}

	rte_eth_dev_probing_finish(eth_dev);

	return 0;
}

static int
memif_set_role(const char *key __rte_unused, const char *value,
	       void *extra_args)
{
	enum memif_role_t *role = (enum memif_role_t *)extra_args;

	if (strstr(value, "server") != NULL) {
		*role = MEMIF_ROLE_SERVER;
	} else if (strstr(value, "client") != NULL) {
		*role = MEMIF_ROLE_CLIENT;
	} else if (strstr(value, "master") != NULL) {
		MIF_LOG(NOTICE, "Role argument \"master\" is deprecated, use \"server\"");
		*role = MEMIF_ROLE_SERVER;
	} else if (strstr(value, "slave") != NULL) {
		MIF_LOG(NOTICE, "Role argument \"slave\" is deprecated, use \"client\"");
		*role = MEMIF_ROLE_CLIENT;
	} else {
		MIF_LOG(ERR, "Unknown role: %s.", value);
		return -EINVAL;
	}
	return 0;
}

static int
memif_set_zc(const char *key __rte_unused, const char *value, void *extra_args)
{
	uint32_t *flags = (uint32_t *)extra_args;

	if (strstr(value, "yes") != NULL) {
		if (!rte_mcfg_get_single_file_segments()) {
			MIF_LOG(ERR, "Zero-copy doesn't support multi-file segments.");
			return -ENOTSUP;
		}
		*flags |= ETH_MEMIF_FLAG_ZERO_COPY;
	} else if (strstr(value, "no") != NULL) {
		*flags &= ~ETH_MEMIF_FLAG_ZERO_COPY;
	} else {
		MIF_LOG(ERR, "Failed to parse zero-copy param: %s.", value);
		return -EINVAL;
	}
	return 0;
}

static int
memif_set_id(const char *key __rte_unused, const char *value, void *extra_args)
{
	memif_interface_id_t *id = (memif_interface_id_t *)extra_args;

	/* even if parsing fails, 0 is a valid id */
	*id = strtoul(value, NULL, 10);
	return 0;
}

static int
memif_set_bs(const char *key __rte_unused, const char *value, void *extra_args)
{
	unsigned long tmp;
	uint16_t *pkt_buffer_size = (uint16_t *)extra_args;

	tmp = strtoul(value, NULL, 10);
	if (tmp == 0 || tmp > 0xFFFF) {
		MIF_LOG(ERR, "Invalid buffer size: %s.", value);
		return -EINVAL;
	}
	*pkt_buffer_size = tmp;
	return 0;
}

static int
memif_set_rs(const char *key __rte_unused, const char *value, void *extra_args)
{
	unsigned long tmp;
	memif_log2_ring_size_t *log2_ring_size =
	    (memif_log2_ring_size_t *)extra_args;

	tmp = strtoul(value, NULL, 10);
	if (tmp == 0 || tmp > ETH_MEMIF_MAX_LOG2_RING_SIZE) {
		MIF_LOG(ERR, "Invalid ring size: %s (max %u).",
			value, ETH_MEMIF_MAX_LOG2_RING_SIZE);
		return -EINVAL;
	}
	*log2_ring_size = tmp;
	return 0;
}

/* check if directory exists and if we have permission to read/write */
static int
memif_check_socket_filename(const char *filename)
{
	char *dir = NULL, *tmp;
	uint32_t idx;
	int ret = 0;

	if (strlen(filename) >= MEMIF_SOCKET_UN_SIZE) {
		MIF_LOG(ERR, "Unix socket address too long (max 108).");
		return -1;
	}

	tmp = strrchr(filename, '/');
	if (tmp != NULL) {
		idx = tmp - filename;
		dir = rte_zmalloc("memif_tmp", sizeof(char) * (idx + 1), 0);
		if (dir == NULL) {
			MIF_LOG(ERR, "Failed to allocate memory.");
			return -1;
		}
		strlcpy(dir, filename, sizeof(char) * (idx + 1));
	}

	if (dir == NULL || (faccessat(-1, dir, F_OK | R_OK |
					W_OK, AT_EACCESS) < 0)) {
		MIF_LOG(ERR, "Invalid socket directory.");
		ret = -EINVAL;
	}

	rte_free(dir);

	return ret;
}

static int
memif_set_socket_filename(const char *key __rte_unused, const char *value,
			  void *extra_args)
{
	const char **socket_filename = (const char **)extra_args;

	*socket_filename = value;
	return 0;
}

static int
memif_set_is_socket_abstract(const char *key __rte_unused, const char *value, void *extra_args)
{
	uint32_t *flags = (uint32_t *)extra_args;

	if (strstr(value, "yes") != NULL) {
		*flags |= ETH_MEMIF_FLAG_SOCKET_ABSTRACT;
	} else if (strstr(value, "no") != NULL) {
		*flags &= ~ETH_MEMIF_FLAG_SOCKET_ABSTRACT;
	} else {
		MIF_LOG(ERR, "Failed to parse socket-abstract param: %s.", value);
		return -EINVAL;
	}
	return 0;
}

static int
memif_set_owner(const char *key, const char *value, void *extra_args)
{
	RTE_ASSERT(sizeof(uid_t) == sizeof(uint32_t));
	RTE_ASSERT(sizeof(gid_t) == sizeof(uint32_t));

	unsigned long val;
	char *end = NULL;
	uint32_t *id = (uint32_t *)extra_args;

	val = strtoul(value, &end, 10);
	if (*value == '\0' || *end != '\0') {
		MIF_LOG(ERR, "Failed to parse %s: %s.", key, value);
		return -EINVAL;
	}
	if (val >= UINT32_MAX) {
		MIF_LOG(ERR, "Invalid %s: %s.", key, value);
		return -ERANGE;
	}

	*id = val;
	return 0;
}

static int
memif_set_mac(const char *key __rte_unused, const char *value, void *extra_args)
{
	struct rte_ether_addr *ether_addr = (struct rte_ether_addr *)extra_args;

	if (rte_ether_unformat_addr(value, ether_addr) < 0)
		MIF_LOG(WARNING, "Failed to parse mac '%s'.", value);
	return 0;
}

static int
memif_set_secret(const char *key __rte_unused, const char *value, void *extra_args)
{
	const char **secret = (const char **)extra_args;

	*secret = value;
	return 0;
}

static int
rte_pmd_memif_probe(struct rte_vdev_device *vdev)
{
	RTE_BUILD_BUG_ON(sizeof(memif_msg_t) != 128);
	RTE_BUILD_BUG_ON(sizeof(memif_desc_t) != 16);
	int ret = 0;
	struct rte_kvargs *kvlist;
	const char *name = rte_vdev_device_name(vdev);
	enum memif_role_t role = MEMIF_ROLE_CLIENT;
	memif_interface_id_t id = 0;
	uint16_t pkt_buffer_size = ETH_MEMIF_DEFAULT_PKT_BUFFER_SIZE;
	memif_log2_ring_size_t log2_ring_size = ETH_MEMIF_DEFAULT_RING_SIZE;
	const char *socket_filename = ETH_MEMIF_DEFAULT_SOCKET_FILENAME;
	uid_t owner_uid = -1;
	gid_t owner_gid = -1;
	uint32_t flags = 0;
	const char *secret = NULL;
	struct rte_ether_addr *ether_addr = rte_zmalloc("",
		sizeof(struct rte_ether_addr), 0);
	struct rte_eth_dev *eth_dev;

	rte_eth_random_addr(ether_addr->addr_bytes);

	MIF_LOG(INFO, "Initialize MEMIF: %s.", name);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			MIF_LOG(ERR, "Failed to probe %s", name);
			return -1;
		}

		eth_dev->dev_ops = &ops;
		eth_dev->device = &vdev->device;
		eth_dev->rx_pkt_burst = eth_memif_rx;
		eth_dev->tx_pkt_burst = eth_memif_tx;

		if (!rte_eal_primary_proc_alive(NULL)) {
			MIF_LOG(ERR, "Primary process is missing");
			return -1;
		}

		eth_dev->process_private = (struct pmd_process_private *)
			rte_zmalloc(name,
				sizeof(struct pmd_process_private),
				RTE_CACHE_LINE_SIZE);
		if (eth_dev->process_private == NULL) {
			MIF_LOG(ERR,
				"Failed to alloc memory for process private");
			return -1;
		}

		rte_eth_dev_probing_finish(eth_dev);

		return 0;
	}

	ret = rte_mp_action_register(MEMIF_MP_SEND_REGION, memif_mp_send_region);
	/*
	 * Primary process can continue probing, but secondary process won't
	 * be able to get memory regions information
	 */
	if (ret < 0 && rte_errno != EEXIST)
		MIF_LOG(WARNING, "Failed to register mp action callback: %s",
			strerror(rte_errno));

	/* use abstract address by default */
	flags |= ETH_MEMIF_FLAG_SOCKET_ABSTRACT;

	kvlist = rte_kvargs_parse(rte_vdev_device_args(vdev), valid_arguments);

	/* parse parameters */
	if (kvlist != NULL) {
		ret = rte_kvargs_process(kvlist, ETH_MEMIF_ROLE_ARG,
					 &memif_set_role, &role);
		if (ret < 0)
			goto exit;
		ret = rte_kvargs_process(kvlist, ETH_MEMIF_ID_ARG,
					 &memif_set_id, &id);
		if (ret < 0)
			goto exit;
		ret = rte_kvargs_process(kvlist, ETH_MEMIF_PKT_BUFFER_SIZE_ARG,
					 &memif_set_bs, &pkt_buffer_size);
		if (ret < 0)
			goto exit;
		ret = rte_kvargs_process(kvlist, ETH_MEMIF_RING_SIZE_ARG,
					 &memif_set_rs, &log2_ring_size);
		if (ret < 0)
			goto exit;
		ret = rte_kvargs_process(kvlist, ETH_MEMIF_SOCKET_ARG,
					 &memif_set_socket_filename,
					 (void *)(&socket_filename));
		if (ret < 0)
			goto exit;
		ret = rte_kvargs_process(kvlist, ETH_MEMIF_SOCKET_ABSTRACT_ARG,
					 &memif_set_is_socket_abstract, &flags);
		if (ret < 0)
			goto exit;
		ret = rte_kvargs_process(kvlist, ETH_MEMIF_OWNER_UID_ARG,
					 &memif_set_owner, &owner_uid);
		if (ret < 0)
			goto exit;
		ret = rte_kvargs_process(kvlist, ETH_MEMIF_OWNER_GID_ARG,
					 &memif_set_owner, &owner_gid);
		if (ret < 0)
			goto exit;
		ret = rte_kvargs_process(kvlist, ETH_MEMIF_MAC_ARG,
					 &memif_set_mac, ether_addr);
		if (ret < 0)
			goto exit;
		ret = rte_kvargs_process(kvlist, ETH_MEMIF_ZC_ARG,
					 &memif_set_zc, &flags);
		if (ret < 0)
			goto exit;
		ret = rte_kvargs_process(kvlist, ETH_MEMIF_SECRET_ARG,
					 &memif_set_secret, (void *)(&secret));
		if (ret < 0)
			goto exit;
	}

	if (!(flags & ETH_MEMIF_FLAG_SOCKET_ABSTRACT)) {
		ret = memif_check_socket_filename(socket_filename);
		if (ret < 0)
			goto exit;
	}

	/* create interface */
	ret = memif_create(vdev, role, id, flags, socket_filename, owner_uid, owner_gid,
			   log2_ring_size, pkt_buffer_size, secret, ether_addr);

exit:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
rte_pmd_memif_remove(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *eth_dev;

	eth_dev = rte_eth_dev_allocated(rte_vdev_device_name(vdev));
	if (eth_dev == NULL)
		return 0;

	return rte_eth_dev_close(eth_dev->data->port_id);
}

static struct rte_vdev_driver pmd_memif_drv = {
	.probe = rte_pmd_memif_probe,
	.remove = rte_pmd_memif_remove,
};

RTE_PMD_REGISTER_VDEV(net_memif, pmd_memif_drv);

RTE_PMD_REGISTER_PARAM_STRING(net_memif,
			      ETH_MEMIF_ID_ARG "=<int>"
			      ETH_MEMIF_ROLE_ARG "=server|client"
			      ETH_MEMIF_PKT_BUFFER_SIZE_ARG "=<int>"
			      ETH_MEMIF_RING_SIZE_ARG "=<int>"
			      ETH_MEMIF_SOCKET_ARG "=<string>"
			      ETH_MEMIF_SOCKET_ABSTRACT_ARG "=yes|no"
			      ETH_MEMIF_OWNER_UID_ARG "=<int>"
			      ETH_MEMIF_OWNER_GID_ARG "=<int>"
			      ETH_MEMIF_MAC_ARG "=xx:xx:xx:xx:xx:xx"
			      ETH_MEMIF_ZC_ARG "=yes|no"
			      ETH_MEMIF_SECRET_ARG "=<string>");

RTE_LOG_REGISTER_DEFAULT(memif_logtype, NOTICE);
