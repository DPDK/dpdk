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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_prefetch.h>
#include <rte_string_fns.h>
#include <rte_errno.h>

#include "virtio_logs.h"
#include "virtio_ethdev.h"
#include "virtqueue.h"

#ifdef RTE_LIBRTE_VIRTIO_DEBUG_DUMP
#define VIRTIO_DUMP_PACKET(m, len) rte_pktmbuf_dump(stdout, m, len)
#else
#define  VIRTIO_DUMP_PACKET(m, len) do { } while (0)
#endif

static inline struct rte_mbuf *
rte_rxmbuf_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;

	m = __rte_mbuf_raw_alloc(mp);
	__rte_mbuf_sanity_check_raw(m, RTE_MBUF_PKT, 0);

	return m;
}

static void
virtio_dev_vring_start(struct rte_eth_dev *dev, struct virtqueue *vq, int queue_type)
{
	struct rte_mbuf *m;
	int i, nbufs, error, size = vq->vq_nentries;
	struct vring *vr = &vq->vq_ring;
	uint8_t *ring_mem = vq->vq_ring_virt_mem;
	char vq_name[VIRTQUEUE_MAX_NAME_SZ];
	PMD_INIT_FUNC_TRACE();

	/*
	 * Reinitialise since virtio port might have been stopped and restarted
	 */
	memset(vq->vq_ring_virt_mem, 0, vq->vq_ring_size);
	vring_init(vr, size, ring_mem, vq->vq_alignment);
	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;
	memset(vq->vq_descx, 0, sizeof(struct vq_desc_extra) * vq->vq_nentries);

	/* Chain all the descriptors in the ring with an END */
	for (i = 0; i < size - 1; i++)
		vr->desc[i].next = (uint16_t)(i + 1);
	vr->desc[i].next = VQ_RING_DESC_CHAIN_END;

	/*
	 * Disable device(host) interrupting guest
	 */
	virtqueue_disable_intr(vq);

	snprintf(vq_name, sizeof(vq_name), "port_%d_rx_vq",
					dev->data->port_id);
	PMD_INIT_LOG(DEBUG, "vq name: %s\n", vq->vq_name);

	/* Only rx virtqueue needs mbufs to be allocated at initialization */
	if (queue_type == VTNET_RQ) {
		if (vq->mpool == NULL)
			rte_exit(EXIT_FAILURE,
			"Cannot allocate initial mbufs for rx virtqueue\n");

		/* Allocate blank mbufs for the each rx descriptor */
		nbufs = 0;
		error = ENOSPC;
		while (!virtqueue_full(vq)) {
			m = rte_rxmbuf_alloc(vq->mpool);
			if (m == NULL)
				break;

			/******************************************
			*         Enqueue allocated buffers        *
			*******************************************/
			error = virtqueue_enqueue_recv_refill(vq, m);

			if (error) {
				rte_pktmbuf_free_seg(m);
				break;
			}
			nbufs++;
		}

		vq_update_avail_idx(vq);

		PMD_INIT_LOG(DEBUG, "Allocated %d bufs\n", nbufs);

		VIRTIO_WRITE_REG_2(vq->hw, VIRTIO_PCI_QUEUE_SEL,
			vq->vq_queue_index);
		VIRTIO_WRITE_REG_4(vq->hw, VIRTIO_PCI_QUEUE_PFN,
			vq->mz->phys_addr >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
	} else if (queue_type == VTNET_TQ) {
		VIRTIO_WRITE_REG_2(vq->hw, VIRTIO_PCI_QUEUE_SEL,
			vq->vq_queue_index);
		VIRTIO_WRITE_REG_4(vq->hw, VIRTIO_PCI_QUEUE_PFN,
			vq->mz->phys_addr >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
	} else {
		VIRTIO_WRITE_REG_2(vq->hw, VIRTIO_PCI_QUEUE_SEL,
			vq->vq_queue_index);
		VIRTIO_WRITE_REG_4(vq->hw, VIRTIO_PCI_QUEUE_PFN,
			vq->mz->phys_addr >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
	}
}

void
virtio_dev_cq_start(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw
		= VIRTIO_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	virtio_dev_vring_start(dev, hw->cvq, VTNET_CQ);
	VIRTQUEUE_DUMP((struct virtqueue *)hw->cvq);
}

void
virtio_dev_rxtx_start(struct rte_eth_dev *dev)
{
	/*
	 * Start receive and transmit vrings
	 * -	Setup vring structure for all queues
	 * -	Initialize descriptor for the rx vring
	 * -	Allocate blank mbufs for the each rx descriptor
	 *
	 */
	int i;

	PMD_INIT_FUNC_TRACE();

	/* Start rx vring. */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		virtio_dev_vring_start(dev, dev->data->rx_queues[i], VTNET_RQ);
		VIRTQUEUE_DUMP((struct virtqueue *)dev->data->rx_queues[i]);
	}

	/* Start tx vring. */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		virtio_dev_vring_start(dev, dev->data->tx_queues[i], VTNET_TQ);
		VIRTQUEUE_DUMP((struct virtqueue *)dev->data->tx_queues[i]);
	}
}

int
virtio_dev_rx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			unsigned int socket_id,
			__rte_unused const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mp)
{
	uint8_t vtpci_queue_idx = 2 * queue_idx + VTNET_SQ_RQ_QUEUE_IDX;
	struct virtqueue *vq;
	int ret;

	PMD_INIT_FUNC_TRACE();
	ret = virtio_dev_queue_setup(dev, VTNET_RQ, queue_idx, vtpci_queue_idx,
			nb_desc, socket_id, &vq);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "tvq initialization failed\n");
		return ret;
	}

	/* Create mempool for rx mbuf allocation */
	vq->mpool = mp;

	dev->data->rx_queues[queue_idx] = vq;
	return 0;
}

/*
 * struct rte_eth_dev *dev: Used to update dev
 * uint16_t nb_desc: Defaults to values read from config space
 * unsigned int socket_id: Used to allocate memzone
 * const struct rte_eth_txconf *tx_conf: Used to setup tx engine
 * uint16_t queue_idx: Just used as an index in dev txq list
 */
int
virtio_dev_tx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			unsigned int socket_id,
			__rte_unused const struct rte_eth_txconf *tx_conf)
{
	uint8_t vtpci_queue_idx = 2 * queue_idx + VTNET_SQ_TQ_QUEUE_IDX;
	struct virtqueue *vq;
	int ret;

	PMD_INIT_FUNC_TRACE();
	ret = virtio_dev_queue_setup(dev, VTNET_TQ, queue_idx, vtpci_queue_idx,
			nb_desc, socket_id, &vq);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "rvq initialization failed\n");
		return ret;
	}

	dev->data->tx_queues[queue_idx] = vq;
	return 0;
}

static void
virtio_discard_rxbuf(struct virtqueue *vq, struct rte_mbuf *m)
{
	int error;
	/*
	 * Requeue the discarded mbuf. This should always be
	 * successful since it was just dequeued.
	 */
	error = virtqueue_enqueue_recv_refill(vq, m);
	if (unlikely(error)) {
		RTE_LOG(ERR, PMD, "cannot requeue discarded mbuf");
		rte_pktmbuf_free_seg(m);
	}
}

#define VIRTIO_MBUF_BURST_SZ 64
#define DESC_PER_CACHELINE (CACHE_LINE_SIZE / sizeof(struct vring_desc))
uint16_t
virtio_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct virtqueue *rxvq = rx_queue;
	struct virtio_hw *hw = rxvq->hw;
	struct rte_mbuf *rxm, *new_mbuf;
	uint16_t nb_used, num, nb_rx = 0;
	uint32_t len[VIRTIO_MBUF_BURST_SZ];
	struct rte_mbuf *rcv_pkts[VIRTIO_MBUF_BURST_SZ];
	int error;
	uint32_t i, nb_enqueued = 0;

	nb_used = VIRTQUEUE_NUSED(rxvq);

	rmb();

	num = (uint16_t)(likely(nb_used <= nb_pkts) ? nb_used : nb_pkts);
	num = (uint16_t)(likely(num <= VIRTIO_MBUF_BURST_SZ) ? num : VIRTIO_MBUF_BURST_SZ);
	if (likely(num > DESC_PER_CACHELINE))
		num = num - ((rxvq->vq_used_cons_idx + num) % DESC_PER_CACHELINE);

	if (num == 0)
		return 0;

	num = virtqueue_dequeue_burst_rx(rxvq, rcv_pkts, len, num);
	PMD_RX_LOG(DEBUG, "used:%d dequeue:%d\n", nb_used, num);
	for (i = 0; i < num ; i++) {
		rxm = rcv_pkts[i];

		PMD_RX_LOG(DEBUG, "packet len:%d\n", len[i]);

		if (unlikely(len[i]
			     < (uint32_t)hw->vtnet_hdr_size + ETHER_HDR_LEN)) {
			PMD_RX_LOG(ERR, "Packet drop\n");
			nb_enqueued++;
			virtio_discard_rxbuf(rxvq, rxm);
			hw->eth_stats.ierrors++;
			continue;
		}

		rxm->pkt.in_port = rxvq->port_id;
		rxm->pkt.data = (char *)rxm->buf_addr + RTE_PKTMBUF_HEADROOM;
		rxm->pkt.nb_segs = 1;
		rxm->pkt.next = NULL;
		rxm->pkt.pkt_len  = (uint32_t)(len[i]
					       - sizeof(struct virtio_net_hdr));
		rxm->pkt.data_len = (uint16_t)(len[i]
					       - sizeof(struct virtio_net_hdr));

		VIRTIO_DUMP_PACKET(rxm, rxm->pkt.data_len);

		rx_pkts[nb_rx++] = rxm;
		hw->eth_stats.ibytes += len[i] - sizeof(struct virtio_net_hdr);
		hw->eth_stats.q_ibytes[rxvq->queue_id] += len[i]
			- sizeof(struct virtio_net_hdr);
	}

	hw->eth_stats.ipackets += nb_rx;
	hw->eth_stats.q_ipackets[rxvq->queue_id] += nb_rx;

	/* Allocate new mbuf for the used descriptor */
	error = ENOSPC;
	while (likely(!virtqueue_full(rxvq))) {
		new_mbuf = rte_rxmbuf_alloc(rxvq->mpool);
		if (unlikely(new_mbuf == NULL)) {
			hw->eth_stats.rx_nombuf++;
			break;
		}
		error = virtqueue_enqueue_recv_refill(rxvq, new_mbuf);
		if (unlikely(error)) {
			rte_pktmbuf_free_seg(new_mbuf);
			break;
		}
		nb_enqueued++;
	}
	if (likely(nb_enqueued)) {
		if (unlikely(virtqueue_kick_prepare(rxvq))) {
			virtqueue_notify(rxvq);
			PMD_RX_LOG(DEBUG, "Notified\n");
		}
	}

	vq_update_avail_idx(rxvq);

	return nb_rx;
}

uint16_t
virtio_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct virtqueue *txvq = tx_queue;
	struct rte_mbuf *txm;
	uint16_t nb_used, nb_tx, num;
	int error;
	struct virtio_hw *hw;

	nb_tx = 0;

	if (unlikely(nb_pkts < 1))
		return nb_pkts;

	PMD_TX_LOG(DEBUG, "%d packets to xmit", nb_pkts);
	nb_used = VIRTQUEUE_NUSED(txvq);

	rmb();

	hw = txvq->hw;
	num = (uint16_t)(likely(nb_used < VIRTIO_MBUF_BURST_SZ) ? nb_used : VIRTIO_MBUF_BURST_SZ);

	while (nb_tx < nb_pkts) {
		if (virtqueue_full(txvq) && num) {
			virtqueue_dequeue_pkt_tx(txvq);
			num--;
		}

		if (!virtqueue_full(txvq)) {
			txm = tx_pkts[nb_tx];
			/* Enqueue Packet buffers */
			error = virtqueue_enqueue_xmit(txvq, txm);
			if (unlikely(error)) {
				if (error == ENOSPC)
					PMD_TX_LOG(ERR, "virtqueue_enqueue Free count = 0\n");
				else if (error == EMSGSIZE)
					PMD_TX_LOG(ERR, "virtqueue_enqueue Free count < 1\n");
				else
					PMD_TX_LOG(ERR, "virtqueue_enqueue error: %d\n", error);
				break;
			}
			nb_tx++;
			hw->eth_stats.obytes += txm->pkt.data_len;
			hw->eth_stats.q_obytes[txvq->queue_id]
				+= txm->pkt.data_len;
		} else {
			PMD_TX_LOG(ERR, "No free tx descriptors to transmit\n");
			break;
		}
	}
	vq_update_avail_idx(txvq);

	hw->eth_stats.opackets += nb_tx;
	hw->eth_stats.q_opackets[txvq->queue_id] += nb_tx;

	if (unlikely(virtqueue_kick_prepare(txvq))) {
		virtqueue_notify(txvq);
		PMD_TX_LOG(DEBUG, "Notified backend after xmit\n");
	}

	return nb_tx;
}
