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
#include <rte_ethdev_driver.h>
#include <rte_ethdev_vdev.h>
#include <rte_malloc.h>
#include <rte_kvargs.h>
#include <rte_bus_vdev.h>
#include <rte_string_fns.h>

#include "rte_eth_memif.h"
#include "memif_socket.h"

#define ETH_MEMIF_ID_ARG		"id"
#define ETH_MEMIF_ROLE_ARG		"role"
#define ETH_MEMIF_PKT_BUFFER_SIZE_ARG	"bsize"
#define ETH_MEMIF_RING_SIZE_ARG		"rsize"
#define ETH_MEMIF_SOCKET_ARG		"socket"
#define ETH_MEMIF_MAC_ARG		"mac"
#define ETH_MEMIF_ZC_ARG		"zero-copy"
#define ETH_MEMIF_SECRET_ARG		"secret"

static const char * const valid_arguments[] = {
	ETH_MEMIF_ID_ARG,
	ETH_MEMIF_ROLE_ARG,
	ETH_MEMIF_PKT_BUFFER_SIZE_ARG,
	ETH_MEMIF_RING_SIZE_ARG,
	ETH_MEMIF_SOCKET_ARG,
	ETH_MEMIF_MAC_ARG,
	ETH_MEMIF_ZC_ARG,
	ETH_MEMIF_SECRET_ARG,
	NULL
};

const char *
memif_version(void)
{
	return ("memif-" RTE_STR(MEMIF_VERSION_MAJOR) "." RTE_STR(MEMIF_VERSION_MINOR));
}

static void
memif_dev_info(struct rte_eth_dev *dev __rte_unused, struct rte_eth_dev_info *dev_info)
{
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_pktlen = (uint32_t)ETH_FRAME_LEN;
	dev_info->max_rx_queues = ETH_MEMIF_MAX_NUM_Q_PAIRS;
	dev_info->max_tx_queues = ETH_MEMIF_MAX_NUM_Q_PAIRS;
	dev_info->min_rx_bufsize = 0;
}

static memif_ring_t *
memif_get_ring(struct pmd_internals *pmd, memif_ring_type_t type, uint16_t ring_num)
{
	/* rings only in region 0 */
	void *p = pmd->regions[0]->addr;
	int ring_size = sizeof(memif_ring_t) + sizeof(memif_desc_t) *
	    (1 << pmd->run.log2_ring_size);

	p = (uint8_t *)p + (ring_num + type * pmd->run.num_s2m_rings) * ring_size;

	return (memif_ring_t *)p;
}

static void *
memif_get_buffer(struct pmd_internals *pmd, memif_desc_t *d)
{
	return ((uint8_t *)pmd->regions[d->region]->addr + d->offset);
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
	struct pmd_internals *pmd = mq->pmd;
	memif_ring_t *ring = mq->ring;
	uint16_t cur_slot, last_slot, n_slots, ring_size, mask, s0;
	uint16_t n_rx_pkts = 0;
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

	if (unlikely((pmd->flags & ETH_MEMIF_FLAG_CONNECTED) == 0))
		return 0;
	if (unlikely(ring == NULL))
		return 0;

	/* consume interrupt */
	if ((ring->flags & MEMIF_RING_FLAG_MASK_INT) == 0)
		size = read(mq->intr_handle.fd, &b, sizeof(b));

	ring_size = 1 << mq->log2_ring_size;
	mask = ring_size - 1;

	cur_slot = (type == MEMIF_RING_S2M) ? mq->last_head : mq->last_tail;
	last_slot = (type == MEMIF_RING_S2M) ? ring->head : ring->tail;
	if (cur_slot == last_slot)
		goto refill;
	n_slots = last_slot - cur_slot;

	while (n_slots && n_rx_pkts < nb_pkts) {
		mbuf_head = rte_pktmbuf_alloc(mq->mempool);
		if (unlikely(mbuf_head == NULL))
			goto no_free_bufs;
		mbuf = mbuf_head;
		mbuf->port = mq->in_port;

next_slot:
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
					MIF_LOG(ERR, "%s: number-of-segments-overflow",
						rte_vdev_device_name(pmd->vdev));
					rte_pktmbuf_free(mbuf);
					goto no_free_bufs;
				}
			}
			cp_len = RTE_MIN(dst_len, src_len);

			rte_pktmbuf_data_len(mbuf) += cp_len;
			rte_pktmbuf_pkt_len(mbuf) = rte_pktmbuf_data_len(mbuf);
			if (mbuf != mbuf_head)
				rte_pktmbuf_pkt_len(mbuf_head) += cp_len;

			memcpy(rte_pktmbuf_mtod_offset(mbuf, void *, dst_off),
			       (uint8_t *)memif_get_buffer(pmd, d0) + src_off, cp_len);

			src_off += cp_len;
			dst_off += cp_len;
			src_len -= cp_len;
		} while (src_len);

		cur_slot++;
		n_slots--;

		if (d0->flags & MEMIF_DESC_FLAG_NEXT)
			goto next_slot;

		mq->n_bytes += rte_pktmbuf_pkt_len(mbuf_head);
		*bufs++ = mbuf_head;
		n_rx_pkts++;
	}

no_free_bufs:
	if (type == MEMIF_RING_S2M) {
		rte_mb();
		ring->tail = cur_slot;
		mq->last_head = cur_slot;
	} else {
		mq->last_tail = cur_slot;
	}

refill:
	if (type == MEMIF_RING_M2S) {
		head = ring->head;
		n_slots = ring_size - head + mq->last_tail;

		while (n_slots--) {
			s0 = head++ & mask;
			d0 = &ring->desc[s0];
			d0->length = pmd->run.pkt_buffer_size;
		}
		rte_mb();
		ring->head = head;
	}

	mq->n_pkts += n_rx_pkts;
	return n_rx_pkts;
}

static uint16_t
eth_memif_tx(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct memif_queue *mq = queue;
	struct pmd_internals *pmd = mq->pmd;
	memif_ring_t *ring = mq->ring;
	uint16_t slot, saved_slot, n_free, ring_size, mask, n_tx_pkts = 0;
	uint16_t src_len, src_off, dst_len, dst_off, cp_len;
	memif_ring_type_t type = mq->type;
	memif_desc_t *d0;
	struct rte_mbuf *mbuf;
	struct rte_mbuf *mbuf_head;
	uint64_t a;
	ssize_t size;

	if (unlikely((pmd->flags & ETH_MEMIF_FLAG_CONNECTED) == 0))
		return 0;
	if (unlikely(ring == NULL))
		return 0;

	ring_size = 1 << mq->log2_ring_size;
	mask = ring_size - 1;

	n_free = ring->tail - mq->last_tail;
	mq->last_tail += n_free;
	slot = (type == MEMIF_RING_S2M) ? ring->head : ring->tail;

	if (type == MEMIF_RING_S2M)
		n_free = ring_size - ring->head + mq->last_tail;
	else
		n_free = ring->head - ring->tail;

	while (n_tx_pkts < nb_pkts && n_free) {
		mbuf_head = *bufs++;
		mbuf = mbuf_head;

		saved_slot = slot;
		d0 = &ring->desc[slot & mask];
		dst_off = 0;
		dst_len = (type == MEMIF_RING_S2M) ?
			pmd->run.pkt_buffer_size : d0->length;

next_in_chain:
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
					dst_len = (type == MEMIF_RING_S2M) ?
					    pmd->run.pkt_buffer_size : d0->length;
					d0->flags = 0;
				} else {
					slot = saved_slot;
					goto no_free_slots;
				}
			}
			cp_len = RTE_MIN(dst_len, src_len);

			memcpy((uint8_t *)memif_get_buffer(pmd, d0) + dst_off,
			       rte_pktmbuf_mtod_offset(mbuf, void *, src_off),
			       cp_len);

			mq->n_bytes += cp_len;
			src_off += cp_len;
			dst_off += cp_len;
			src_len -= cp_len;
			dst_len -= cp_len;

			d0->length = dst_off;
		}

		if (rte_pktmbuf_is_contiguous(mbuf) == 0) {
			mbuf = mbuf->next;
			goto next_in_chain;
		}

		n_tx_pkts++;
		slot++;
		n_free--;
		rte_pktmbuf_free(mbuf_head);
	}

no_free_slots:
	rte_mb();
	if (type == MEMIF_RING_S2M)
		ring->head = slot;
	else
		ring->tail = slot;

	if ((ring->flags & MEMIF_RING_FLAG_MASK_INT) == 0) {
		a = 1;
		size = write(mq->intr_handle.fd, &a, sizeof(a));
		if (unlikely(size < 0)) {
			MIF_LOG(WARNING,
				"%s: Failed to send interrupt. %s",
				rte_vdev_device_name(pmd->vdev), strerror(errno));
		}
	}

	mq->n_err += nb_pkts - n_tx_pkts;
	mq->n_pkts += n_tx_pkts;
	return n_tx_pkts;
}

void
memif_free_regions(struct pmd_internals *pmd)
{
	int i;
	struct memif_region *r;

	/* regions are allocated contiguously, so it's
	 * enough to loop until 'pmd->regions_num'
	 */
	for (i = 0; i < pmd->regions_num; i++) {
		r = pmd->regions[i];
		if (r != NULL) {
			if (r->addr != NULL) {
				munmap(r->addr, r->region_size);
				if (r->fd > 0) {
					close(r->fd);
					r->fd = -1;
				}
			}
			rte_free(r);
			pmd->regions[i] = NULL;
		}
	}
	pmd->regions_num = 0;
}

static int
memif_region_init_shm(struct pmd_internals *pmd, uint8_t has_buffers)
{
	char shm_name[ETH_MEMIF_SHM_NAME_SIZE];
	int ret = 0;
	struct memif_region *r;

	if (pmd->regions_num >= ETH_MEMIF_MAX_REGION_NUM) {
		MIF_LOG(ERR, "%s: Too many regions.", rte_vdev_device_name(pmd->vdev));
		return -1;
	}

	r = rte_zmalloc("region", sizeof(struct memif_region), 0);
	if (r == NULL) {
		MIF_LOG(ERR, "%s: Failed to alloc memif region.",
			rte_vdev_device_name(pmd->vdev));
		return -ENOMEM;
	}

	/* calculate buffer offset */
	r->pkt_buffer_offset = (pmd->run.num_s2m_rings + pmd->run.num_m2s_rings) *
	    (sizeof(memif_ring_t) + sizeof(memif_desc_t) *
	    (1 << pmd->run.log2_ring_size));

	r->region_size = r->pkt_buffer_offset;
	/* if region has buffers, add buffers size to region_size */
	if (has_buffers == 1)
		r->region_size += (uint32_t)(pmd->run.pkt_buffer_size *
			(1 << pmd->run.log2_ring_size) *
			(pmd->run.num_s2m_rings +
			 pmd->run.num_m2s_rings));

	memset(shm_name, 0, sizeof(char) * ETH_MEMIF_SHM_NAME_SIZE);
	snprintf(shm_name, ETH_MEMIF_SHM_NAME_SIZE, "memif_region_%d",
		 pmd->regions_num);

	r->fd = memfd_create(shm_name, MFD_ALLOW_SEALING);
	if (r->fd < 0) {
		MIF_LOG(ERR, "%s: Failed to create shm file: %s.",
			rte_vdev_device_name(pmd->vdev),
			strerror(errno));
		ret = -1;
		goto error;
	}

	ret = fcntl(r->fd, F_ADD_SEALS, F_SEAL_SHRINK);
	if (ret < 0) {
		MIF_LOG(ERR, "%s: Failed to add seals to shm file: %s.",
			rte_vdev_device_name(pmd->vdev),
			strerror(errno));
		goto error;
	}

	ret = ftruncate(r->fd, r->region_size);
	if (ret < 0) {
		MIF_LOG(ERR, "%s: Failed to truncate shm file: %s.",
			rte_vdev_device_name(pmd->vdev),
			strerror(errno));
		goto error;
	}

	r->addr = mmap(NULL, r->region_size, PROT_READ |
		       PROT_WRITE, MAP_SHARED, r->fd, 0);
	if (r->addr == MAP_FAILED) {
		MIF_LOG(ERR, "%s: Failed to mmap shm region: %s.",
			rte_vdev_device_name(pmd->vdev),
			strerror(ret));
		ret = -1;
		goto error;
	}

	pmd->regions[pmd->regions_num] = r;
	pmd->regions_num++;

	return ret;

error:
	if (r->fd > 0)
		close(r->fd);
	r->fd = -1;

	return ret;
}

static int
memif_regions_init(struct pmd_internals *pmd)
{
	int ret;

	/* create one buffer region */
	ret = memif_region_init_shm(pmd, /* has buffer */ 1);
	if (ret < 0)
		return ret;

	return 0;
}

static void
memif_init_rings(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	memif_ring_t *ring;
	int i, j;
	uint16_t slot;

	for (i = 0; i < pmd->run.num_s2m_rings; i++) {
		ring = memif_get_ring(pmd, MEMIF_RING_S2M, i);
		ring->head = 0;
		ring->tail = 0;
		ring->cookie = MEMIF_COOKIE;
		ring->flags = 0;
		for (j = 0; j < (1 << pmd->run.log2_ring_size); j++) {
			slot = i * (1 << pmd->run.log2_ring_size) + j;
			ring->desc[j].region = 0;
			ring->desc[j].offset = pmd->regions[0]->pkt_buffer_offset +
				(uint32_t)(slot * pmd->run.pkt_buffer_size);
			ring->desc[j].length = pmd->run.pkt_buffer_size;
		}
	}

	for (i = 0; i < pmd->run.num_m2s_rings; i++) {
		ring = memif_get_ring(pmd, MEMIF_RING_M2S, i);
		ring->head = 0;
		ring->tail = 0;
		ring->cookie = MEMIF_COOKIE;
		ring->flags = 0;
		for (j = 0; j < (1 << pmd->run.log2_ring_size); j++) {
			slot = (i + pmd->run.num_s2m_rings) *
			    (1 << pmd->run.log2_ring_size) + j;
			ring->desc[j].region = 0;
			ring->desc[j].offset = pmd->regions[0]->pkt_buffer_offset +
				(uint32_t)(slot * pmd->run.pkt_buffer_size);
			ring->desc[j].length = pmd->run.pkt_buffer_size;
		}
	}
}

/* called only by slave */
static void
memif_init_queues(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_queue *mq;
	int i;

	for (i = 0; i < pmd->run.num_s2m_rings; i++) {
		mq = dev->data->tx_queues[i];
		mq->ring = memif_get_ring(pmd, MEMIF_RING_S2M, i);
		mq->log2_ring_size = pmd->run.log2_ring_size;
		/* queues located only in region 0 */
		mq->region = 0;
		mq->ring_offset = (uint8_t *)mq->ring - (uint8_t *)pmd->regions[0]->addr;
		mq->last_head = 0;
		mq->last_tail = 0;
		mq->intr_handle.fd = eventfd(0, EFD_NONBLOCK);
		if (mq->intr_handle.fd < 0) {
			MIF_LOG(WARNING,
				"%s: Failed to create eventfd for tx queue %d: %s.",
				rte_vdev_device_name(pmd->vdev), i,
				strerror(errno));
		}
	}

	for (i = 0; i < pmd->run.num_m2s_rings; i++) {
		mq = dev->data->rx_queues[i];
		mq->ring = memif_get_ring(pmd, MEMIF_RING_M2S, i);
		mq->log2_ring_size = pmd->run.log2_ring_size;
		/* queues located only in region 0 */
		mq->region = 0;
		mq->ring_offset = (uint8_t *)mq->ring - (uint8_t *)pmd->regions[0]->addr;
		mq->last_head = 0;
		mq->last_tail = 0;
		mq->intr_handle.fd = eventfd(0, EFD_NONBLOCK);
		if (mq->intr_handle.fd < 0) {
			MIF_LOG(WARNING,
				"%s: Failed to create eventfd for rx queue %d: %s.",
				rte_vdev_device_name(pmd->vdev), i,
				strerror(errno));
		}
	}
}

int
memif_init_regions_and_queues(struct rte_eth_dev *dev)
{
	int ret;

	ret = memif_regions_init(dev->data->dev_private);
	if (ret < 0)
		return ret;

	memif_init_rings(dev);

	memif_init_queues(dev);

	return 0;
}

int
memif_connect(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	struct memif_region *mr;
	struct memif_queue *mq;
	int i;

	for (i = 0; i < pmd->regions_num; i++) {
		mr = pmd->regions[i];
		if (mr != NULL) {
			if (mr->addr == NULL) {
				if (mr->fd < 0)
					return -1;
				mr->addr = mmap(NULL, mr->region_size,
						PROT_READ | PROT_WRITE,
						MAP_SHARED, mr->fd, 0);
				if (mr->addr == NULL)
					return -1;
			}
		}
	}

	for (i = 0; i < pmd->run.num_s2m_rings; i++) {
		mq = (pmd->role == MEMIF_ROLE_SLAVE) ?
		    dev->data->tx_queues[i] : dev->data->rx_queues[i];
		mq->ring = (memif_ring_t *)((uint8_t *)pmd->regions[mq->region]->addr +
			    mq->ring_offset);
		if (mq->ring->cookie != MEMIF_COOKIE) {
			MIF_LOG(ERR, "%s: Wrong cookie",
				rte_vdev_device_name(pmd->vdev));
			return -1;
		}
		mq->ring->head = 0;
		mq->ring->tail = 0;
		mq->last_head = 0;
		mq->last_tail = 0;
		/* enable polling mode */
		if (pmd->role == MEMIF_ROLE_MASTER)
			mq->ring->flags = MEMIF_RING_FLAG_MASK_INT;
	}
	for (i = 0; i < pmd->run.num_m2s_rings; i++) {
		mq = (pmd->role == MEMIF_ROLE_SLAVE) ?
		    dev->data->rx_queues[i] : dev->data->tx_queues[i];
		mq->ring = (memif_ring_t *)((uint8_t *)pmd->regions[mq->region]->addr +
			    mq->ring_offset);
		if (mq->ring->cookie != MEMIF_COOKIE) {
			MIF_LOG(ERR, "%s: Wrong cookie",
				rte_vdev_device_name(pmd->vdev));
			return -1;
		}
		mq->ring->head = 0;
		mq->ring->tail = 0;
		mq->last_head = 0;
		mq->last_tail = 0;
		/* enable polling mode */
		if (pmd->role == MEMIF_ROLE_SLAVE)
			mq->ring->flags = MEMIF_RING_FLAG_MASK_INT;
	}

	pmd->flags &= ~ETH_MEMIF_FLAG_CONNECTING;
	pmd->flags |= ETH_MEMIF_FLAG_CONNECTED;
	dev->data->dev_link.link_status = ETH_LINK_UP;
	MIF_LOG(INFO, "%s: Connected.", rte_vdev_device_name(pmd->vdev));
	return 0;
}

static int
memif_dev_start(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	int ret = 0;

	switch (pmd->role) {
	case MEMIF_ROLE_SLAVE:
		ret = memif_connect_slave(dev);
		break;
	case MEMIF_ROLE_MASTER:
		ret = memif_connect_master(dev);
		break;
	default:
		MIF_LOG(ERR, "%s: Unknown role: %d.",
			rte_vdev_device_name(pmd->vdev), pmd->role);
		ret = -1;
		break;
	}

	return ret;
}

static void
memif_dev_close(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	int i;

	memif_msg_enq_disconnect(pmd->cc, "Device closed", 0);
	memif_disconnect(dev);

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		(*dev->dev_ops->rx_queue_release)(dev->data->rx_queues[i]);
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		(*dev->dev_ops->tx_queue_release)(dev->data->tx_queues[i]);

	memif_socket_remove_device(dev);
}

static int
memif_dev_configure(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;

	/*
	 * SLAVE - TXQ
	 * MASTER - RXQ
	 */
	pmd->cfg.num_s2m_rings = (pmd->role == MEMIF_ROLE_SLAVE) ?
				  dev->data->nb_tx_queues : dev->data->nb_rx_queues;

	/*
	 * SLAVE - RXQ
	 * MASTER - TXQ
	 */
	pmd->cfg.num_m2s_rings = (pmd->role == MEMIF_ROLE_SLAVE) ?
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
		MIF_LOG(ERR, "%s: Failed to allocate tx queue id: %u",
			rte_vdev_device_name(pmd->vdev), qid);
		return -ENOMEM;
	}

	mq->type =
	    (pmd->role == MEMIF_ROLE_SLAVE) ? MEMIF_RING_S2M : MEMIF_RING_M2S;
	mq->n_pkts = 0;
	mq->n_bytes = 0;
	mq->n_err = 0;
	mq->intr_handle.fd = -1;
	mq->intr_handle.type = RTE_INTR_HANDLE_EXT;
	mq->pmd = pmd;
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
		MIF_LOG(ERR, "%s: Failed to allocate rx queue id: %u",
			rte_vdev_device_name(pmd->vdev), qid);
		return -ENOMEM;
	}

	mq->type = (pmd->role == MEMIF_ROLE_SLAVE) ? MEMIF_RING_M2S : MEMIF_RING_S2M;
	mq->n_pkts = 0;
	mq->n_bytes = 0;
	mq->n_err = 0;
	mq->intr_handle.fd = -1;
	mq->intr_handle.type = RTE_INTR_HANDLE_EXT;
	mq->mempool = mb_pool;
	mq->in_port = dev->data->port_id;
	mq->pmd = pmd;
	dev->data->rx_queues[qid] = mq;

	return 0;
}

static void
memif_queue_release(void *queue)
{
	struct memif_queue *mq = (struct memif_queue *)queue;

	if (!mq)
		return;

	rte_free(mq);
}

static int
memif_link_update(struct rte_eth_dev *dev __rte_unused,
		  int wait_to_complete __rte_unused)
{
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
	stats->oerrors = 0;

	tmp = (pmd->role == MEMIF_ROLE_SLAVE) ? pmd->run.num_s2m_rings :
	    pmd->run.num_m2s_rings;
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

	tmp = (pmd->role == MEMIF_ROLE_SLAVE) ? pmd->run.num_m2s_rings :
	    pmd->run.num_s2m_rings;
	nq = (tmp < RTE_ETHDEV_QUEUE_STAT_CNTRS) ? tmp :
	    RTE_ETHDEV_QUEUE_STAT_CNTRS;

	/* TX stats */
	for (i = 0; i < nq; i++) {
		mq = dev->data->tx_queues[i];
		stats->q_opackets[i] = mq->n_pkts;
		stats->q_obytes[i] = mq->n_bytes;
		stats->opackets += mq->n_pkts;
		stats->obytes += mq->n_bytes;
		stats->oerrors += mq->n_err;
	}
	return 0;
}

static void
memif_stats_reset(struct rte_eth_dev *dev)
{
	struct pmd_internals *pmd = dev->data->dev_private;
	int i;
	struct memif_queue *mq;

	for (i = 0; i < pmd->run.num_s2m_rings; i++) {
		mq = (pmd->role == MEMIF_ROLE_SLAVE) ? dev->data->tx_queues[i] :
		    dev->data->rx_queues[i];
		mq->n_pkts = 0;
		mq->n_bytes = 0;
		mq->n_err = 0;
	}
	for (i = 0; i < pmd->run.num_m2s_rings; i++) {
		mq = (pmd->role == MEMIF_ROLE_SLAVE) ? dev->data->rx_queues[i] :
		    dev->data->tx_queues[i];
		mq->n_pkts = 0;
		mq->n_bytes = 0;
		mq->n_err = 0;
	}
}

static int
memif_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t qid __rte_unused)
{
	struct pmd_internals *pmd = dev->data->dev_private;

	MIF_LOG(WARNING, "%s: Interrupt mode not supported.",
		rte_vdev_device_name(pmd->vdev));

	return -1;
}

static int
memif_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t qid __rte_unused)
{
	struct pmd_internals *pmd __rte_unused = dev->data->dev_private;

	return 0;
}

static const struct eth_dev_ops ops = {
	.dev_start = memif_dev_start,
	.dev_close = memif_dev_close,
	.dev_infos_get = memif_dev_info,
	.dev_configure = memif_dev_configure,
	.tx_queue_setup = memif_tx_queue_setup,
	.rx_queue_setup = memif_rx_queue_setup,
	.rx_queue_release = memif_queue_release,
	.tx_queue_release = memif_queue_release,
	.rx_queue_intr_enable = memif_rx_queue_intr_enable,
	.rx_queue_intr_disable = memif_rx_queue_intr_disable,
	.link_update = memif_link_update,
	.stats_get = memif_stats_get,
	.stats_reset = memif_stats_reset,
};

static int
memif_create(struct rte_vdev_device *vdev, enum memif_role_t role,
	     memif_interface_id_t id, uint32_t flags,
	     const char *socket_filename,
	     memif_log2_ring_size_t log2_ring_size,
	     uint16_t pkt_buffer_size, const char *secret,
	     struct rte_ether_addr *ether_addr)
{
	int ret = 0;
	struct rte_eth_dev *eth_dev;
	struct rte_eth_dev_data *data;
	struct pmd_internals *pmd;
	const unsigned int numa_node = vdev->device.numa_node;
	const char *name = rte_vdev_device_name(vdev);

	if (flags & ETH_MEMIF_FLAG_ZERO_COPY) {
		MIF_LOG(ERR, "Zero-copy slave not supported.");
		return -1;
	}

	eth_dev = rte_eth_vdev_allocate(vdev, sizeof(*pmd));
	if (eth_dev == NULL) {
		MIF_LOG(ERR, "%s: Unable to allocate device struct.", name);
		return -1;
	}

	pmd = eth_dev->data->dev_private;
	memset(pmd, 0, sizeof(*pmd));

	pmd->vdev = vdev;
	pmd->id = id;
	pmd->flags = flags;
	pmd->flags |= ETH_MEMIF_FLAG_DISABLED;
	pmd->role = role;

	ret = memif_socket_init(eth_dev, socket_filename);
	if (ret < 0)
		return ret;

	memset(pmd->secret, 0, sizeof(char) * ETH_MEMIF_SECRET_SIZE);
	if (secret != NULL)
		strlcpy(pmd->secret, secret, sizeof(pmd->secret));

	pmd->cfg.log2_ring_size = log2_ring_size;
	/* set in .dev_configure() */
	pmd->cfg.num_s2m_rings = 0;
	pmd->cfg.num_m2s_rings = 0;

	pmd->cfg.pkt_buffer_size = pkt_buffer_size;

	data = eth_dev->data;
	data->dev_private = pmd;
	data->numa_node = numa_node;
	data->mac_addrs = ether_addr;

	eth_dev->dev_ops = &ops;
	eth_dev->device = &vdev->device;
	eth_dev->rx_pkt_burst = eth_memif_rx;
	eth_dev->tx_pkt_burst = eth_memif_tx;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	rte_eth_dev_probing_finish(eth_dev);

	return 0;
}

static int
memif_set_role(const char *key __rte_unused, const char *value,
	       void *extra_args)
{
	enum memif_role_t *role = (enum memif_role_t *)extra_args;

	if (strstr(value, "master") != NULL) {
		*role = MEMIF_ROLE_MASTER;
	} else if (strstr(value, "slave") != NULL) {
		*role = MEMIF_ROLE_SLAVE;
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

	if (dir != NULL)
		rte_free(dir);

	return ret;
}

static int
memif_set_socket_filename(const char *key __rte_unused, const char *value,
			  void *extra_args)
{
	const char **socket_filename = (const char **)extra_args;

	*socket_filename = value;
	return memif_check_socket_filename(*socket_filename);
}

static int
memif_set_mac(const char *key __rte_unused, const char *value, void *extra_args)
{
	struct rte_ether_addr *ether_addr = (struct rte_ether_addr *)extra_args;
	int ret = 0;

	ret = sscanf(value, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
	       &ether_addr->addr_bytes[0], &ether_addr->addr_bytes[1],
	       &ether_addr->addr_bytes[2], &ether_addr->addr_bytes[3],
	       &ether_addr->addr_bytes[4], &ether_addr->addr_bytes[5]);
	if (ret != 6)
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
	enum memif_role_t role = MEMIF_ROLE_SLAVE;
	memif_interface_id_t id = 0;
	uint16_t pkt_buffer_size = ETH_MEMIF_DEFAULT_PKT_BUFFER_SIZE;
	memif_log2_ring_size_t log2_ring_size = ETH_MEMIF_DEFAULT_RING_SIZE;
	const char *socket_filename = ETH_MEMIF_DEFAULT_SOCKET_FILENAME;
	uint32_t flags = 0;
	const char *secret = NULL;
	struct rte_ether_addr *ether_addr = rte_zmalloc("",
		sizeof(struct rte_ether_addr), 0);

	rte_eth_random_addr(ether_addr->addr_bytes);

	MIF_LOG(INFO, "Initialize MEMIF: %s.", name);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		MIF_LOG(ERR, "Multi-processing not supported for memif.");
		/* TODO:
		 * Request connection information.
		 *
		 * Once memif in the primary process is connected,
		 * broadcast connection information.
		 */
		return -1;
	}

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

	/* create interface */
	ret = memif_create(vdev, role, id, flags, socket_filename,
			   log2_ring_size, pkt_buffer_size, secret, ether_addr);

exit:
	if (kvlist != NULL)
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

	rte_eth_dev_close(eth_dev->data->port_id);

	return 0;
}

static struct rte_vdev_driver pmd_memif_drv = {
	.probe = rte_pmd_memif_probe,
	.remove = rte_pmd_memif_remove,
};

RTE_PMD_REGISTER_VDEV(net_memif, pmd_memif_drv);

RTE_PMD_REGISTER_PARAM_STRING(net_memif,
			      ETH_MEMIF_ID_ARG "=<int>"
			      ETH_MEMIF_ROLE_ARG "=master|slave"
			      ETH_MEMIF_PKT_BUFFER_SIZE_ARG "=<int>"
			      ETH_MEMIF_RING_SIZE_ARG "=<int>"
			      ETH_MEMIF_SOCKET_ARG "=<string>"
			      ETH_MEMIF_MAC_ARG "=xx:xx:xx:xx:xx:xx"
			      ETH_MEMIF_ZC_ARG "=yes|no"
			      ETH_MEMIF_SECRET_ARG "=<string>");

int memif_logtype;

RTE_INIT(memif_init_log)
{
	memif_logtype = rte_log_register("pmd.net.memif");
	if (memif_logtype >= 0)
		rte_log_set_level(memif_logtype, RTE_LOG_NOTICE);
}
