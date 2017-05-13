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

#ifndef _VIRTQUEUE_H_
#define _VIRTQUEUE_H_

#include <stdint.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_net.h>

#include <rte_atomic.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mempool.h>

#include "virtio_logs.h"

struct rte_mbuf;

/* The alignment to use between consumer and producer parts of vring. */
#define VIRTIO_PCI_VRING_ALIGN 4096

enum { VTNET_RQ = 0, VTNET_TQ = 1, VTNET_CQ = 2 };

/**
 * The maximum virtqueue size is 2^15. Use that value as the end of
 * descriptor chain terminator since it will never be a valid index
 * in the descriptor table. This is used to verify we are correctly
 * handling vq_free_cnt.
 */
#define VQ_RING_DESC_CHAIN_END 32768

#define VIRTQUEUE_MAX_NAME_SZ  32

struct pmd_internals {
	struct rte_eth_stats eth_stats;
	int port_id;
	int virtio_idx;
};


struct virtqueue {
	char vq_name[VIRTQUEUE_MAX_NAME_SZ];
	struct rte_mempool       *mpool;  /**< mempool for mbuf allocation */
	uint16_t    queue_id;             /**< DPDK queue index. */
	uint16_t    vq_queue_index;       /**< PCI queue index */
	uint8_t     port_id;              /**< Device port identifier. */

	void        *vq_ring_virt_mem;    /**< virtual address of vring*/
	int         vq_alignment;
	int         vq_ring_size;

	struct vring vq_ring;    /**< vring keeping desc, used and avail */
	struct pmd_internals *internals;  /**< virtio device internal info. */
	uint16_t    vq_nentries; /**< vring desc numbers */
	uint16_t    vq_desc_head_idx;
	uint16_t    vq_free_cnt; /**< num of desc available */
	uint16_t vq_used_cons_idx; /**< Last consumed desc in used table, trails vq_ring.used->idx*/

	struct vq_desc_extra {
		void              *cookie;
		uint16_t          ndescs;
	} vq_descx[0] __rte_cache_aligned;
};


#ifdef  RTE_LIBRTE_XENVIRT_DEBUG_DUMP
#define VIRTQUEUE_DUMP(vq) do { \
	uint16_t used_idx, nused; \
	used_idx = (vq)->vq_ring.used->idx; \
	nused = (uint16_t)(used_idx - (vq)->vq_used_cons_idx); \
	PMD_INIT_LOG(DEBUG, \
	  "VQ: %s - size=%d; free=%d; used=%d; desc_head_idx=%d;" \
	  " avail.idx=%d; used_cons_idx=%d; used.idx=%d;" \
	  " avail.flags=0x%x; used.flags=0x%x\n", \
	  (vq)->vq_name, (vq)->vq_nentries, (vq)->vq_free_cnt, nused, \
	  (vq)->vq_desc_head_idx, (vq)->vq_ring.avail->idx, \
	  (vq)->vq_used_cons_idx, (vq)->vq_ring.used->idx, \
	  (vq)->vq_ring.avail->flags, (vq)->vq_ring.used->flags); \
} while (0)
#else
#define VIRTQUEUE_DUMP(vq) do { } while (0)
#endif


/**
 *  Dump virtqueue internal structures, for debug purpose only.
 */
void virtqueue_dump(struct virtqueue *vq);

/**
 *  Get all mbufs to be freed.
 */
struct rte_mbuf * virtqueue_detatch_unused(struct virtqueue *vq);

static __rte_always_inline int
virtqueue_full(const struct virtqueue *vq)
{
	return vq->vq_free_cnt == 0;
}

#define VIRTQUEUE_NUSED(vq) ((uint16_t)((vq)->vq_ring.used->idx - (vq)->vq_used_cons_idx))

static __rte_always_inline void
vq_ring_update_avail(struct virtqueue *vq, uint16_t desc_idx)
{
	uint16_t avail_idx;
	/*
	 * Place the head of the descriptor chain into the next slot and make
	 * it usable to the host. The chain is made available now rather than
	 * deferring to virtqueue_notify() in the hopes that if the host is
	 * currently running on another CPU, we can keep it processing the new
	 * descriptor.
	 */
	avail_idx = (uint16_t)(vq->vq_ring.avail->idx & (vq->vq_nentries - 1));
	vq->vq_ring.avail->ring[avail_idx] = desc_idx;
	rte_smp_wmb();
	vq->vq_ring.avail->idx++;
}

static __rte_always_inline void
vq_ring_free_chain(struct virtqueue *vq, uint16_t desc_idx)
{
	struct vring_desc *dp;
	struct vq_desc_extra *dxp;

	dp  = &vq->vq_ring.desc[desc_idx];
	dxp = &vq->vq_descx[desc_idx];
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt + dxp->ndescs);
	while (dp->flags & VRING_DESC_F_NEXT) {
		dp = &vq->vq_ring.desc[dp->next];
	}
	dxp->ndescs = 0;

	/*
	 * We must append the existing free chain, if any, to the end of
	 * newly freed chain. If the virtqueue was completely used, then
	 * head would be VQ_RING_DESC_CHAIN_END (ASSERTed above).
	 */
	dp->next = vq->vq_desc_head_idx;
	vq->vq_desc_head_idx = desc_idx;
}

static __rte_always_inline int
virtqueue_enqueue_recv_refill(struct virtqueue *rxvq, struct rte_mbuf *cookie)
{
	const uint16_t needed = 1;
	const uint16_t head_idx = rxvq->vq_desc_head_idx;
	struct vring_desc *start_dp = rxvq->vq_ring.desc;
	struct vq_desc_extra *dxp;

	if (unlikely(rxvq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(rxvq->vq_free_cnt < needed))
		return -EMSGSIZE;
	if (unlikely(head_idx >= rxvq->vq_nentries))
		return -EFAULT;

	dxp = &rxvq->vq_descx[head_idx];
	dxp->cookie = (void *)cookie;
	dxp->ndescs = needed;

	start_dp[head_idx].addr  =
		(uint64_t) ((uintptr_t)cookie->buf_addr + RTE_PKTMBUF_HEADROOM - sizeof(struct virtio_net_hdr));
	start_dp[head_idx].len   = cookie->buf_len - RTE_PKTMBUF_HEADROOM + sizeof(struct virtio_net_hdr);
	start_dp[head_idx].flags = VRING_DESC_F_WRITE;
	rxvq->vq_desc_head_idx   = start_dp[head_idx].next;
	rxvq->vq_free_cnt        = (uint16_t)(rxvq->vq_free_cnt - needed);
	vq_ring_update_avail(rxvq, head_idx);

	return 0;
}

static __rte_always_inline int
virtqueue_enqueue_xmit(struct virtqueue *txvq, struct rte_mbuf *cookie)
{

	const uint16_t needed = 2;
	struct vring_desc *start_dp =  txvq->vq_ring.desc;
	uint16_t head_idx = txvq->vq_desc_head_idx;
	uint16_t idx      = head_idx;
	struct vq_desc_extra *dxp;

	if (unlikely(txvq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(txvq->vq_free_cnt < needed))
		return -EMSGSIZE;
	if (unlikely(head_idx >= txvq->vq_nentries))
		return -EFAULT;

	dxp = &txvq->vq_descx[idx];
	dxp->cookie = (void *)cookie;
	dxp->ndescs = needed;

	start_dp = txvq->vq_ring.desc;
	start_dp[idx].addr  = 0;
/*
 * TODO: save one desc here?
 */
	start_dp[idx].len   = sizeof(struct virtio_net_hdr);
	start_dp[idx].flags = VRING_DESC_F_NEXT;
	start_dp[idx].addr  = (uintptr_t)NULL;
	idx = start_dp[idx].next;
	start_dp[idx].addr  = (uint64_t)rte_pktmbuf_mtod(cookie, uintptr_t);
	start_dp[idx].len   = cookie->data_len;
	start_dp[idx].flags = 0;
	idx = start_dp[idx].next;
	txvq->vq_desc_head_idx = idx;
	txvq->vq_free_cnt = (uint16_t)(txvq->vq_free_cnt - needed);
	vq_ring_update_avail(txvq, head_idx);

	return 0;
}

static __rte_always_inline uint16_t
virtqueue_dequeue_burst(struct virtqueue *vq, struct rte_mbuf **rx_pkts, uint32_t *len, uint16_t num)
{
	struct vring_used_elem *uep;
	struct rte_mbuf *cookie;
	uint16_t used_idx, desc_idx;
	uint16_t i;
	/*  Caller does the check */
	for (i = 0; i < num ; i ++) {
		used_idx = (uint16_t)(vq->vq_used_cons_idx & (vq->vq_nentries - 1));
		uep = &vq->vq_ring.used->ring[used_idx];
		desc_idx = (uint16_t) uep->id;
		cookie = (struct rte_mbuf *)vq->vq_descx[desc_idx].cookie;
		if (unlikely(cookie == NULL)) {
			PMD_DRV_LOG(ERR, "vring descriptor with no mbuf cookie at %u\n",
				vq->vq_used_cons_idx);
			RTE_LOG(ERR, PMD, "%s: inconsistent (%u, %u)\n", __func__, used_idx , desc_idx);
			break;
		}
		len[i] = uep->len;
		rx_pkts[i]  = cookie;
		vq->vq_used_cons_idx++;
		vq_ring_free_chain(vq, desc_idx);
		vq->vq_descx[desc_idx].cookie = NULL;
	}
	return i;
}

#endif /* _VIRTQUEUE_H_ */
