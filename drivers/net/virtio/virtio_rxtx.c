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
#include <rte_byteorder.h>

#include "virtio_logs.h"
#include "virtio_ethdev.h"
#include "virtio_pci.h"
#include "virtqueue.h"
#include "virtio_rxtx.h"

#ifdef RTE_LIBRTE_VIRTIO_DEBUG_DUMP
#define VIRTIO_DUMP_PACKET(m, len) rte_pktmbuf_dump(stdout, m, len)
#else
#define  VIRTIO_DUMP_PACKET(m, len) do { } while (0)
#endif


#define VIRTIO_SIMPLE_FLAGS ((uint32_t)ETH_TXQ_FLAGS_NOMULTSEGS | \
	ETH_TXQ_FLAGS_NOOFFLOADS)

#ifdef RTE_MACHINE_CPUFLAG_SSSE3
static int use_simple_rxtx;
#endif

static void
vq_ring_free_chain(struct virtqueue *vq, uint16_t desc_idx)
{
	struct vring_desc *dp, *dp_tail;
	struct vq_desc_extra *dxp;
	uint16_t desc_idx_last = desc_idx;

	dp  = &vq->vq_ring.desc[desc_idx];
	dxp = &vq->vq_descx[desc_idx];
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt + dxp->ndescs);
	if ((dp->flags & VRING_DESC_F_INDIRECT) == 0) {
		while (dp->flags & VRING_DESC_F_NEXT) {
			desc_idx_last = dp->next;
			dp = &vq->vq_ring.desc[dp->next];
		}
	}
	dxp->ndescs = 0;

	/*
	 * We must append the existing free chain, if any, to the end of
	 * newly freed chain. If the virtqueue was completely used, then
	 * head would be VQ_RING_DESC_CHAIN_END (ASSERTed above).
	 */
	if (vq->vq_desc_tail_idx == VQ_RING_DESC_CHAIN_END) {
		vq->vq_desc_head_idx = desc_idx;
	} else {
		dp_tail = &vq->vq_ring.desc[vq->vq_desc_tail_idx];
		dp_tail->next = desc_idx;
	}

	vq->vq_desc_tail_idx = desc_idx_last;
	dp->next = VQ_RING_DESC_CHAIN_END;
}

static uint16_t
virtqueue_dequeue_burst_rx(struct virtqueue *vq, struct rte_mbuf **rx_pkts,
			   uint32_t *len, uint16_t num)
{
	struct vring_used_elem *uep;
	struct rte_mbuf *cookie;
	uint16_t used_idx, desc_idx;
	uint16_t i;

	/*  Caller does the check */
	for (i = 0; i < num ; i++) {
		used_idx = (uint16_t)(vq->vq_used_cons_idx & (vq->vq_nentries - 1));
		uep = &vq->vq_ring.used->ring[used_idx];
		desc_idx = (uint16_t) uep->id;
		len[i] = uep->len;
		cookie = (struct rte_mbuf *)vq->vq_descx[desc_idx].cookie;

		if (unlikely(cookie == NULL)) {
			PMD_DRV_LOG(ERR, "vring descriptor with no mbuf cookie at %u\n",
				vq->vq_used_cons_idx);
			break;
		}

		rte_prefetch0(cookie);
		rte_packet_prefetch(rte_pktmbuf_mtod(cookie, void *));
		rx_pkts[i]  = cookie;
		vq->vq_used_cons_idx++;
		vq_ring_free_chain(vq, desc_idx);
		vq->vq_descx[desc_idx].cookie = NULL;
	}

	return i;
}

#ifndef DEFAULT_TX_FREE_THRESH
#define DEFAULT_TX_FREE_THRESH 32
#endif

/* Cleanup from completed transmits. */
static void
virtio_xmit_cleanup(struct virtqueue *vq, uint16_t num)
{
	uint16_t i, used_idx, desc_idx;
	for (i = 0; i < num; i++) {
		struct vring_used_elem *uep;
		struct vq_desc_extra *dxp;

		used_idx = (uint16_t)(vq->vq_used_cons_idx & (vq->vq_nentries - 1));
		uep = &vq->vq_ring.used->ring[used_idx];

		desc_idx = (uint16_t) uep->id;
		dxp = &vq->vq_descx[desc_idx];
		vq->vq_used_cons_idx++;
		vq_ring_free_chain(vq, desc_idx);

		if (dxp->cookie != NULL) {
			rte_pktmbuf_free(dxp->cookie);
			dxp->cookie = NULL;
		}
	}
}


static inline int
virtqueue_enqueue_recv_refill(struct virtqueue *vq, struct rte_mbuf *cookie)
{
	struct vq_desc_extra *dxp;
	struct virtio_hw *hw = vq->hw;
	struct vring_desc *start_dp;
	uint16_t needed = 1;
	uint16_t head_idx, idx;

	if (unlikely(vq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(vq->vq_free_cnt < needed))
		return -EMSGSIZE;

	head_idx = vq->vq_desc_head_idx;
	if (unlikely(head_idx >= vq->vq_nentries))
		return -EFAULT;

	idx = head_idx;
	dxp = &vq->vq_descx[idx];
	dxp->cookie = (void *)cookie;
	dxp->ndescs = needed;

	start_dp = vq->vq_ring.desc;
	start_dp[idx].addr =
		VIRTIO_MBUF_ADDR(cookie, vq) +
		RTE_PKTMBUF_HEADROOM - hw->vtnet_hdr_size;
	start_dp[idx].len =
		cookie->buf_len - RTE_PKTMBUF_HEADROOM + hw->vtnet_hdr_size;
	start_dp[idx].flags =  VRING_DESC_F_WRITE;
	idx = start_dp[idx].next;
	vq->vq_desc_head_idx = idx;
	if (vq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END)
		vq->vq_desc_tail_idx = idx;
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - needed);
	vq_update_avail_ring(vq, head_idx);

	return 0;
}

static inline void
virtqueue_enqueue_xmit(struct virtnet_tx *txvq, struct rte_mbuf *cookie,
		       uint16_t needed, int use_indirect, int can_push)
{
	struct vq_desc_extra *dxp;
	struct virtqueue *vq = txvq->vq;
	struct vring_desc *start_dp;
	uint16_t seg_num = cookie->nb_segs;
	uint16_t head_idx, idx;
	uint16_t head_size = vq->hw->vtnet_hdr_size;
	unsigned long offs;

	head_idx = vq->vq_desc_head_idx;
	idx = head_idx;
	dxp = &vq->vq_descx[idx];
	dxp->cookie = (void *)cookie;
	dxp->ndescs = needed;

	start_dp = vq->vq_ring.desc;

	if (can_push) {
		/* put on zero'd transmit header (no offloads) */
		void *hdr = rte_pktmbuf_prepend(cookie, head_size);

		memset(hdr, 0, head_size);
	} else if (use_indirect) {
		/* setup tx ring slot to point to indirect
		 * descriptor list stored in reserved region.
		 *
		 * the first slot in indirect ring is already preset
		 * to point to the header in reserved region
		 */
		struct virtio_tx_region *txr = txvq->virtio_net_hdr_mz->addr;

		offs = idx * sizeof(struct virtio_tx_region)
			+ offsetof(struct virtio_tx_region, tx_indir);

		start_dp[idx].addr  = txvq->virtio_net_hdr_mem + offs;
		start_dp[idx].len   = (seg_num + 1) * sizeof(struct vring_desc);
		start_dp[idx].flags = VRING_DESC_F_INDIRECT;

		/* loop below will fill in rest of the indirect elements */
		start_dp = txr[idx].tx_indir;
		idx = 1;
	} else {
		/* setup first tx ring slot to point to header
		 * stored in reserved region.
		 */
		offs = idx * sizeof(struct virtio_tx_region)
			+ offsetof(struct virtio_tx_region, tx_hdr);

		start_dp[idx].addr  = txvq->virtio_net_hdr_mem + offs;
		start_dp[idx].len   = vq->hw->vtnet_hdr_size;
		start_dp[idx].flags = VRING_DESC_F_NEXT;
		idx = start_dp[idx].next;
	}

	do {
		start_dp[idx].addr  = VIRTIO_MBUF_DATA_DMA_ADDR(cookie, vq);
		start_dp[idx].len   = cookie->data_len;
		start_dp[idx].flags = cookie->next ? VRING_DESC_F_NEXT : 0;
		idx = start_dp[idx].next;
	} while ((cookie = cookie->next) != NULL);

	if (use_indirect)
		idx = vq->vq_ring.desc[head_idx].next;

	vq->vq_desc_head_idx = idx;
	if (vq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END)
		vq->vq_desc_tail_idx = idx;
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - needed);
	vq_update_avail_ring(vq, head_idx);
}

static void
virtio_dev_vring_start(struct virtqueue *vq)
{
	int size = vq->vq_nentries;
	struct vring *vr = &vq->vq_ring;
	uint8_t *ring_mem = vq->vq_ring_virt_mem;

	PMD_INIT_FUNC_TRACE();

	/*
	 * Reinitialise since virtio port might have been stopped and restarted
	 */
	memset(vq->vq_ring_virt_mem, 0, vq->vq_ring_size);
	vring_init(vr, size, ring_mem, VIRTIO_PCI_VRING_ALIGN);
	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;
	memset(vq->vq_descx, 0, sizeof(struct vq_desc_extra) * vq->vq_nentries);

	vring_desc_init(vr->desc, size);

	/*
	 * Disable device(host) interrupting guest
	 */
	virtqueue_disable_intr(vq);
}

void
virtio_dev_cq_start(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (hw->cvq && hw->cvq->vq) {
		virtio_dev_vring_start(hw->cvq->vq);
		VIRTQUEUE_DUMP((struct virtqueue *)hw->cvq->vq);
	}
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
	uint16_t i;
	uint16_t desc_idx;

	PMD_INIT_FUNC_TRACE();

	/* Start rx vring. */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct virtnet_rx *rxvq = dev->data->rx_queues[i];
		struct virtqueue *vq = rxvq->vq;
		int error, nbufs;
		struct rte_mbuf *m;

		virtio_dev_vring_start(vq);
		if (rxvq->mpool == NULL) {
			rte_exit(EXIT_FAILURE,
				"Cannot allocate mbufs for rx virtqueue");
		}

		/* Allocate blank mbufs for the each rx descriptor */
		nbufs = 0;
		error = ENOSPC;

#ifdef RTE_MACHINE_CPUFLAG_SSSE3
		if (use_simple_rxtx) {
			for (desc_idx = 0; desc_idx < vq->vq_nentries;
			     desc_idx++) {
				vq->vq_ring.avail->ring[desc_idx] = desc_idx;
				vq->vq_ring.desc[desc_idx].flags =
					VRING_DESC_F_WRITE;
			}
		}
#endif
		memset(&rxvq->fake_mbuf, 0, sizeof(rxvq->fake_mbuf));
		for (desc_idx = 0; desc_idx < RTE_PMD_VIRTIO_RX_MAX_BURST;
		     desc_idx++) {
			vq->sw_ring[vq->vq_nentries + desc_idx] =
				&rxvq->fake_mbuf;
		}

		while (!virtqueue_full(vq)) {
			m = rte_mbuf_raw_alloc(rxvq->mpool);
			if (m == NULL)
				break;

			/******************************************
			*         Enqueue allocated buffers        *
			*******************************************/
#ifdef RTE_MACHINE_CPUFLAG_SSSE3
			if (use_simple_rxtx)
				error = virtqueue_enqueue_recv_refill_simple(vq, m);
			else
#endif
				error = virtqueue_enqueue_recv_refill(vq, m);
			if (error) {
				rte_pktmbuf_free(m);
				break;
			}
			nbufs++;
		}

		vq_update_avail_idx(vq);

		PMD_INIT_LOG(DEBUG, "Allocated %d bufs", nbufs);

		VIRTQUEUE_DUMP(vq);
	}

	/* Start tx vring. */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct virtnet_tx *txvq = dev->data->tx_queues[i];
		struct virtqueue *vq = txvq->vq;

		virtio_dev_vring_start(vq);
#ifdef RTE_MACHINE_CPUFLAG_SSSE3
		if (use_simple_rxtx) {
			uint16_t mid_idx  = vq->vq_nentries >> 1;

			for (desc_idx = 0; desc_idx < mid_idx; desc_idx++) {
				vq->vq_ring.avail->ring[desc_idx] =
					desc_idx + mid_idx;
				vq->vq_ring.desc[desc_idx + mid_idx].next =
					desc_idx;
				vq->vq_ring.desc[desc_idx + mid_idx].addr =
					txvq->virtio_net_hdr_mem +
					offsetof(struct virtio_tx_region, tx_hdr);
				vq->vq_ring.desc[desc_idx + mid_idx].len =
					vq->hw->vtnet_hdr_size;
				vq->vq_ring.desc[desc_idx + mid_idx].flags =
					VRING_DESC_F_NEXT;
				vq->vq_ring.desc[desc_idx].flags = 0;
			}
			for (desc_idx = mid_idx; desc_idx < vq->vq_nentries;
			     desc_idx++)
				vq->vq_ring.avail->ring[desc_idx] = desc_idx;
		}
#endif
		VIRTQUEUE_DUMP(vq);
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
	uint16_t vtpci_queue_idx = 2 * queue_idx + VTNET_SQ_RQ_QUEUE_IDX;
	struct virtnet_rx *rxvq;
	int ret;

	PMD_INIT_FUNC_TRACE();
	ret = virtio_dev_queue_setup(dev, VTNET_RQ, queue_idx, vtpci_queue_idx,
			nb_desc, socket_id, (void **)&rxvq);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "rvq initialization failed");
		return ret;
	}

	/* Create mempool for rx mbuf allocation */
	rxvq->mpool = mp;

	dev->data->rx_queues[queue_idx] = rxvq;

#ifdef RTE_MACHINE_CPUFLAG_SSSE3
	virtio_rxq_vec_setup(rxvq);
#endif

	return 0;
}

void
virtio_dev_rx_queue_release(void *rxq)
{
	struct virtnet_rx *rxvq = rxq;
	struct virtqueue *vq;
	const struct rte_memzone *mz;

	if (rxvq == NULL)
		return;

	/*
	 * rxvq is freed when vq is freed, and as mz should be freed after the
	 * del_queue, so we reserve the mz pointer first.
	 */
	vq = rxvq->vq;
	mz = rxvq->mz;

	virtio_dev_queue_release(vq);
	rte_memzone_free(mz);
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
			const struct rte_eth_txconf *tx_conf)
{
	uint8_t vtpci_queue_idx = 2 * queue_idx + VTNET_SQ_TQ_QUEUE_IDX;

#ifdef RTE_MACHINE_CPUFLAG_SSSE3
	struct virtio_hw *hw = dev->data->dev_private;
#endif
	struct virtnet_tx *txvq;
	struct virtqueue *vq;
	uint16_t tx_free_thresh;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if ((tx_conf->txq_flags & ETH_TXQ_FLAGS_NOXSUMS)
	    != ETH_TXQ_FLAGS_NOXSUMS) {
		PMD_INIT_LOG(ERR, "TX checksum offload not supported\n");
		return -EINVAL;
	}

#ifdef RTE_MACHINE_CPUFLAG_SSSE3
	/* Use simple rx/tx func if single segment and no offloads */
	if ((tx_conf->txq_flags & VIRTIO_SIMPLE_FLAGS) == VIRTIO_SIMPLE_FLAGS &&
	     !vtpci_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF)) {
		PMD_INIT_LOG(INFO, "Using simple rx/tx path");
		dev->tx_pkt_burst = virtio_xmit_pkts_simple;
		dev->rx_pkt_burst = virtio_recv_pkts_vec;
		use_simple_rxtx = 1;
	}
#endif

	ret = virtio_dev_queue_setup(dev, VTNET_TQ, queue_idx, vtpci_queue_idx,
			nb_desc, socket_id, (void **)&txvq);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "tvq initialization failed");
		return ret;
	}
	vq = txvq->vq;

	tx_free_thresh = tx_conf->tx_free_thresh;
	if (tx_free_thresh == 0)
		tx_free_thresh =
			RTE_MIN(vq->vq_nentries / 4, DEFAULT_TX_FREE_THRESH);

	if (tx_free_thresh >= (vq->vq_nentries - 3)) {
		RTE_LOG(ERR, PMD, "tx_free_thresh must be less than the "
			"number of TX entries minus 3 (%u)."
			" (tx_free_thresh=%u port=%u queue=%u)\n",
			vq->vq_nentries - 3,
			tx_free_thresh, dev->data->port_id, queue_idx);
		return -EINVAL;
	}

	vq->vq_free_thresh = tx_free_thresh;

	dev->data->tx_queues[queue_idx] = txvq;
	return 0;
}

void
virtio_dev_tx_queue_release(void *txq)
{
	struct virtnet_tx *txvq = txq;
	struct virtqueue *vq;
	const struct rte_memzone *mz;
	const struct rte_memzone *hdr_mz;

	if (txvq == NULL)
		return;

	/*
	 * txvq is freed when vq is freed, and as mz should be freed after the
	 * del_queue, so we reserve the mz pointer first.
	 */
	vq = txvq->vq;
	mz = txvq->mz;
	hdr_mz = txvq->virtio_net_hdr_mz;

	virtio_dev_queue_release(vq);
	rte_memzone_free(mz);
	rte_memzone_free(hdr_mz);
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
		rte_pktmbuf_free(m);
	}
}

static void
virtio_update_packet_stats(struct virtnet_stats *stats, struct rte_mbuf *mbuf)
{
	uint32_t s = mbuf->pkt_len;
	struct ether_addr *ea;

	if (s == 64) {
		stats->size_bins[1]++;
	} else if (s > 64 && s < 1024) {
		uint32_t bin;

		/* count zeros, and offset into correct bin */
		bin = (sizeof(s) * 8) - __builtin_clz(s) - 5;
		stats->size_bins[bin]++;
	} else {
		if (s < 64)
			stats->size_bins[0]++;
		else if (s < 1519)
			stats->size_bins[6]++;
		else if (s >= 1519)
			stats->size_bins[7]++;
	}

	ea = rte_pktmbuf_mtod(mbuf, struct ether_addr *);
	if (is_multicast_ether_addr(ea)) {
		if (is_broadcast_ether_addr(ea))
			stats->broadcast++;
		else
			stats->multicast++;
	}
}

#define VIRTIO_MBUF_BURST_SZ 64
#define DESC_PER_CACHELINE (RTE_CACHE_LINE_SIZE / sizeof(struct vring_desc))
uint16_t
virtio_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = rxvq->vq;
	struct virtio_hw *hw;
	struct rte_mbuf *rxm, *new_mbuf;
	uint16_t nb_used, num, nb_rx;
	uint32_t len[VIRTIO_MBUF_BURST_SZ];
	struct rte_mbuf *rcv_pkts[VIRTIO_MBUF_BURST_SZ];
	int error;
	uint32_t i, nb_enqueued;
	uint32_t hdr_size;

	nb_used = VIRTQUEUE_NUSED(vq);

	virtio_rmb();

	num = (uint16_t)(likely(nb_used <= nb_pkts) ? nb_used : nb_pkts);
	num = (uint16_t)(likely(num <= VIRTIO_MBUF_BURST_SZ) ? num : VIRTIO_MBUF_BURST_SZ);
	if (likely(num > DESC_PER_CACHELINE))
		num = num - ((vq->vq_used_cons_idx + num) % DESC_PER_CACHELINE);

	num = virtqueue_dequeue_burst_rx(vq, rcv_pkts, len, num);
	PMD_RX_LOG(DEBUG, "used:%d dequeue:%d", nb_used, num);

	hw = vq->hw;
	nb_rx = 0;
	nb_enqueued = 0;
	hdr_size = hw->vtnet_hdr_size;

	for (i = 0; i < num ; i++) {
		rxm = rcv_pkts[i];

		PMD_RX_LOG(DEBUG, "packet len:%d", len[i]);

		if (unlikely(len[i] < hdr_size + ETHER_HDR_LEN)) {
			PMD_RX_LOG(ERR, "Packet drop");
			nb_enqueued++;
			virtio_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		rxm->port = rxvq->port_id;
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->ol_flags = 0;
		rxm->vlan_tci = 0;

		rxm->nb_segs = 1;
		rxm->next = NULL;
		rxm->pkt_len = (uint32_t)(len[i] - hdr_size);
		rxm->data_len = (uint16_t)(len[i] - hdr_size);

		if (hw->vlan_strip)
			rte_vlan_strip(rxm);

		VIRTIO_DUMP_PACKET(rxm, rxm->data_len);

		rx_pkts[nb_rx++] = rxm;

		rxvq->stats.bytes += rx_pkts[nb_rx - 1]->pkt_len;
		virtio_update_packet_stats(&rxvq->stats, rxm);
	}

	rxvq->stats.packets += nb_rx;

	/* Allocate new mbuf for the used descriptor */
	error = ENOSPC;
	while (likely(!virtqueue_full(vq))) {
		new_mbuf = rte_mbuf_raw_alloc(rxvq->mpool);
		if (unlikely(new_mbuf == NULL)) {
			struct rte_eth_dev *dev
				= &rte_eth_devices[rxvq->port_id];
			dev->data->rx_mbuf_alloc_failed++;
			break;
		}
		error = virtqueue_enqueue_recv_refill(vq, new_mbuf);
		if (unlikely(error)) {
			rte_pktmbuf_free(new_mbuf);
			break;
		}
		nb_enqueued++;
	}

	if (likely(nb_enqueued)) {
		vq_update_avail_idx(vq);

		if (unlikely(virtqueue_kick_prepare(vq))) {
			virtqueue_notify(vq);
			PMD_RX_LOG(DEBUG, "Notified");
		}
	}

	return nb_rx;
}

uint16_t
virtio_recv_mergeable_pkts(void *rx_queue,
			struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = rxvq->vq;
	struct virtio_hw *hw;
	struct rte_mbuf *rxm, *new_mbuf;
	uint16_t nb_used, num, nb_rx;
	uint32_t len[VIRTIO_MBUF_BURST_SZ];
	struct rte_mbuf *rcv_pkts[VIRTIO_MBUF_BURST_SZ];
	struct rte_mbuf *prev;
	int error;
	uint32_t i, nb_enqueued;
	uint32_t seg_num;
	uint16_t extra_idx;
	uint32_t seg_res;
	uint32_t hdr_size;

	nb_used = VIRTQUEUE_NUSED(vq);

	virtio_rmb();

	PMD_RX_LOG(DEBUG, "used:%d", nb_used);

	hw = vq->hw;
	nb_rx = 0;
	i = 0;
	nb_enqueued = 0;
	seg_num = 0;
	extra_idx = 0;
	seg_res = 0;
	hdr_size = hw->vtnet_hdr_size;

	while (i < nb_used) {
		struct virtio_net_hdr_mrg_rxbuf *header;

		if (nb_rx == nb_pkts)
			break;

		num = virtqueue_dequeue_burst_rx(vq, rcv_pkts, len, 1);
		if (num != 1)
			continue;

		i++;

		PMD_RX_LOG(DEBUG, "dequeue:%d", num);
		PMD_RX_LOG(DEBUG, "packet len:%d", len[0]);

		rxm = rcv_pkts[0];

		if (unlikely(len[0] < hdr_size + ETHER_HDR_LEN)) {
			PMD_RX_LOG(ERR, "Packet drop");
			nb_enqueued++;
			virtio_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		header = (struct virtio_net_hdr_mrg_rxbuf *)((char *)rxm->buf_addr +
			RTE_PKTMBUF_HEADROOM - hdr_size);
		seg_num = header->num_buffers;

		if (seg_num == 0)
			seg_num = 1;

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->nb_segs = seg_num;
		rxm->next = NULL;
		rxm->ol_flags = 0;
		rxm->vlan_tci = 0;
		rxm->pkt_len = (uint32_t)(len[0] - hdr_size);
		rxm->data_len = (uint16_t)(len[0] - hdr_size);

		rxm->port = rxvq->port_id;
		rx_pkts[nb_rx] = rxm;
		prev = rxm;

		seg_res = seg_num - 1;

		while (seg_res != 0) {
			/*
			 * Get extra segments for current uncompleted packet.
			 */
			uint16_t  rcv_cnt =
				RTE_MIN(seg_res, RTE_DIM(rcv_pkts));
			if (likely(VIRTQUEUE_NUSED(vq) >= rcv_cnt)) {
				uint32_t rx_num =
					virtqueue_dequeue_burst_rx(vq,
					rcv_pkts, len, rcv_cnt);
				i += rx_num;
				rcv_cnt = rx_num;
			} else {
				PMD_RX_LOG(ERR,
					   "No enough segments for packet.");
				nb_enqueued++;
				virtio_discard_rxbuf(vq, rxm);
				rxvq->stats.errors++;
				break;
			}

			extra_idx = 0;

			while (extra_idx < rcv_cnt) {
				rxm = rcv_pkts[extra_idx];

				rxm->data_off = RTE_PKTMBUF_HEADROOM - hdr_size;
				rxm->next = NULL;
				rxm->pkt_len = (uint32_t)(len[extra_idx]);
				rxm->data_len = (uint16_t)(len[extra_idx]);

				if (prev)
					prev->next = rxm;

				prev = rxm;
				rx_pkts[nb_rx]->pkt_len += rxm->pkt_len;
				extra_idx++;
			};
			seg_res -= rcv_cnt;
		}

		if (hw->vlan_strip)
			rte_vlan_strip(rx_pkts[nb_rx]);

		VIRTIO_DUMP_PACKET(rx_pkts[nb_rx],
			rx_pkts[nb_rx]->data_len);

		rxvq->stats.bytes += rx_pkts[nb_rx]->pkt_len;
		virtio_update_packet_stats(&rxvq->stats, rx_pkts[nb_rx]);
		nb_rx++;
	}

	rxvq->stats.packets += nb_rx;

	/* Allocate new mbuf for the used descriptor */
	error = ENOSPC;
	while (likely(!virtqueue_full(vq))) {
		new_mbuf = rte_mbuf_raw_alloc(rxvq->mpool);
		if (unlikely(new_mbuf == NULL)) {
			struct rte_eth_dev *dev
				= &rte_eth_devices[rxvq->port_id];
			dev->data->rx_mbuf_alloc_failed++;
			break;
		}
		error = virtqueue_enqueue_recv_refill(vq, new_mbuf);
		if (unlikely(error)) {
			rte_pktmbuf_free(new_mbuf);
			break;
		}
		nb_enqueued++;
	}

	if (likely(nb_enqueued)) {
		vq_update_avail_idx(vq);

		if (unlikely(virtqueue_kick_prepare(vq))) {
			virtqueue_notify(vq);
			PMD_RX_LOG(DEBUG, "Notified");
		}
	}

	return nb_rx;
}

uint16_t
virtio_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct virtnet_tx *txvq = tx_queue;
	struct virtqueue *vq = txvq->vq;
	struct virtio_hw *hw = vq->hw;
	uint16_t hdr_size = hw->vtnet_hdr_size;
	uint16_t nb_used, nb_tx;
	int error;

	if (unlikely(nb_pkts < 1))
		return nb_pkts;

	PMD_TX_LOG(DEBUG, "%d packets to xmit", nb_pkts);
	nb_used = VIRTQUEUE_NUSED(vq);

	virtio_rmb();
	if (likely(nb_used > vq->vq_nentries - vq->vq_free_thresh))
		virtio_xmit_cleanup(vq, nb_used);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		struct rte_mbuf *txm = tx_pkts[nb_tx];
		int can_push = 0, use_indirect = 0, slots, need;

		/* Do VLAN tag insertion */
		if (unlikely(txm->ol_flags & PKT_TX_VLAN_PKT)) {
			error = rte_vlan_insert(&txm);
			if (unlikely(error)) {
				rte_pktmbuf_free(txm);
				continue;
			}
		}

		/* optimize ring usage */
		if (vtpci_with_feature(hw, VIRTIO_F_ANY_LAYOUT) &&
		    rte_mbuf_refcnt_read(txm) == 1 &&
		    RTE_MBUF_DIRECT(txm) &&
		    txm->nb_segs == 1 &&
		    rte_pktmbuf_headroom(txm) >= hdr_size &&
		    rte_is_aligned(rte_pktmbuf_mtod(txm, char *),
				   __alignof__(struct virtio_net_hdr_mrg_rxbuf)))
			can_push = 1;
		else if (vtpci_with_feature(hw, VIRTIO_RING_F_INDIRECT_DESC) &&
			 txm->nb_segs < VIRTIO_MAX_TX_INDIRECT)
			use_indirect = 1;

		/* How many main ring entries are needed to this Tx?
		 * any_layout => number of segments
		 * indirect   => 1
		 * default    => number of segments + 1
		 */
		slots = use_indirect ? 1 : (txm->nb_segs + !can_push);
		need = slots - vq->vq_free_cnt;

		/* Positive value indicates it need free vring descriptors */
		if (unlikely(need > 0)) {
			nb_used = VIRTQUEUE_NUSED(vq);
			virtio_rmb();
			need = RTE_MIN(need, (int)nb_used);

			virtio_xmit_cleanup(vq, need);
			need = slots - vq->vq_free_cnt;
			if (unlikely(need > 0)) {
				PMD_TX_LOG(ERR,
					   "No free tx descriptors to transmit");
				break;
			}
		}

		/* Enqueue Packet buffers */
		virtqueue_enqueue_xmit(txvq, txm, slots, use_indirect, can_push);

		txvq->stats.bytes += txm->pkt_len;
		virtio_update_packet_stats(&txvq->stats, txm);
	}

	txvq->stats.packets += nb_tx;

	if (likely(nb_tx)) {
		vq_update_avail_idx(vq);

		if (unlikely(virtqueue_kick_prepare(vq))) {
			virtqueue_notify(vq);
			PMD_TX_LOG(DEBUG, "Notified backend after xmit");
		}
	}

	return nb_tx;
}
