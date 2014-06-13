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

#include <rte_atomic.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mempool.h>

#include "virtio_pci.h"
#include "virtio_ring.h"
#include "virtio_logs.h"

#define mb()  rte_mb()
#define wmb() rte_wmb()
#define rmb() rte_rmb()

#ifdef RTE_PMD_PACKET_PREFETCH
#define rte_packet_prefetch(p)  rte_prefetch1(p)
#else
#define rte_packet_prefetch(p)  do {} while(0)
#endif

#define VIRTQUEUE_MAX_NAME_SZ 32

#define RTE_MBUF_DATA_DMA_ADDR(mb) \
	(uint64_t) ((mb)->buf_physaddr + (uint64_t)((char *)((mb)->pkt.data) - \
	(char *)(mb)->buf_addr))

#define VTNET_SQ_RQ_QUEUE_IDX 0
#define VTNET_SQ_TQ_QUEUE_IDX 1
#define VTNET_SQ_CQ_QUEUE_IDX 2

enum { VTNET_RQ = 0, VTNET_TQ = 1, VTNET_CQ = 2 };
/**
 * The maximum virtqueue size is 2^15. Use that value as the end of
 * descriptor chain terminator since it will never be a valid index
 * in the descriptor table. This is used to verify we are correctly
 * handling vq_free_cnt.
 */
#define VQ_RING_DESC_CHAIN_END 32768

/**
 * Control the RX mode, ie. promiscuous, allmulti, etc...
 * All commands require an "out" sg entry containing a 1 byte
 * state value, zero = disable, non-zero = enable.  Commands
 * 0 and 1 are supported with the VIRTIO_NET_F_CTRL_RX feature.
 * Commands 2-5 are added with VIRTIO_NET_F_CTRL_RX_EXTRA.
 */
#define VIRTIO_NET_CTRL_RX              0
#define VIRTIO_NET_CTRL_RX_PROMISC      0
#define VIRTIO_NET_CTRL_RX_ALLMULTI     1
#define VIRTIO_NET_CTRL_RX_ALLUNI       2
#define VIRTIO_NET_CTRL_RX_NOMULTI      3
#define VIRTIO_NET_CTRL_RX_NOUNI        4
#define VIRTIO_NET_CTRL_RX_NOBCAST      5

/**
 * Control VLAN filtering
 *
 * The VLAN filter table is controlled via a simple ADD/DEL interface.
 * VLAN IDs not added may be filtered by the hypervisor.  Del is the
 * opposite of add.  Both commands expect an out entry containing a 2
 * byte VLAN ID.  VLAN filtering is available with the
 * VIRTIO_NET_F_CTRL_VLAN feature bit.
 */
#define VIRTIO_NET_CTRL_VLAN     2
#define VIRTIO_NET_CTRL_VLAN_ADD 0
#define VIRTIO_NET_CTRL_VLAN_DEL 1

struct virtio_net_ctrl_hdr {
	uint8_t class;
	uint8_t cmd;
} __attribute__((packed));

typedef uint8_t virtio_net_ctrl_ack;

#define VIRTIO_NET_OK     0
#define VIRTIO_NET_ERR    1

#define VIRTIO_MAX_CTRL_DATA 128

struct virtio_pmd_ctrl {
	struct virtio_net_ctrl_hdr hdr;
	virtio_net_ctrl_ack status;
	uint8_t data[VIRTIO_MAX_CTRL_DATA];
};

struct virtqueue {
	char        vq_name[VIRTQUEUE_MAX_NAME_SZ];
	struct virtio_hw         *hw;     /**< virtio_hw structure pointer. */
	const struct rte_memzone *mz;     /**< mem zone to populate RX ring. */
	const struct rte_memzone *virtio_net_hdr_mz; /**< memzone to populate hdr. */
	struct rte_mempool       *mpool;  /**< mempool for mbuf allocation */
	uint16_t    queue_id;             /**< DPDK queue index. */
	uint8_t     port_id;              /**< Device port identifier. */

	void        *vq_ring_virt_mem;    /**< linear address of vring*/
	int         vq_alignment;
	int         vq_ring_size;
	phys_addr_t vq_ring_mem;          /**< physical address of vring */

	struct vring vq_ring;    /**< vring keeping desc, used and avail */
	uint16_t    vq_free_cnt; /**< num of desc available */
	uint16_t    vq_nentries; /**< vring desc numbers */
	uint16_t    vq_queue_index;       /**< PCI queue index */
	/**
	 * Head of the free chain in the descriptor table. If
	 * there are no free descriptors, this will be set to
	 * VQ_RING_DESC_CHAIN_END.
	 */
	uint16_t  vq_desc_head_idx;
	uint16_t  vq_desc_tail_idx;
	/**
	 * Last consumed descriptor in the used table,
	 * trails vq_ring.used->idx.
	 */
	uint16_t vq_used_cons_idx;
	uint16_t vq_avail_idx;
	void     *virtio_net_hdr_mem; /**< hdr for each xmit packet */

	struct vq_desc_extra {
		void              *cookie;
		uint16_t          ndescs;
	} vq_descx[0];
};

/* If multiqueue is provided by host, then we suppport it. */
#ifndef VIRTIO_NET_F_MQ
/* Device supports Receive Flow Steering */
#define VIRTIO_NET_F_MQ 0x400000
#define VIRTIO_NET_CTRL_MQ   4
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET        0
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MIN        1
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX        0x8000
#endif

/**
 * This is the first element of the scatter-gather list.  If you don't
 * specify GSO or CSUM features, you can simply ignore the header.
 */
struct virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1    /**< Use csum_start,csum_offset*/
	uint8_t flags;
#define VIRTIO_NET_HDR_GSO_NONE     0    /**< Not a GSO frame */
#define VIRTIO_NET_HDR_GSO_TCPV4    1    /**< GSO frame, IPv4 TCP (TSO) */
#define VIRTIO_NET_HDR_GSO_UDP      3    /**< GSO frame, IPv4 UDP (UFO) */
#define VIRTIO_NET_HDR_GSO_TCPV6    4    /**< GSO frame, IPv6 TCP */
#define VIRTIO_NET_HDR_GSO_ECN      0x80 /**< TCP has ECN set */
	uint8_t gso_type;
	uint16_t hdr_len;     /**< Ethernet + IP + tcp/udp hdrs */
	uint16_t gso_size;    /**< Bytes to append to hdr_len per frame */
	uint16_t csum_start;  /**< Position to start checksumming from */
	uint16_t csum_offset; /**< Offset after that to place checksum */
};

/**
 * This is the version of the header to use when the MRG_RXBUF
 * feature has been negotiated.
 */
struct virtio_net_hdr_mrg_rxbuf {
	struct   virtio_net_hdr hdr;
	uint16_t num_buffers; /**< Number of merged rx buffers */
};

/**
 * Tell the backend not to interrupt us.
 */
void virtqueue_disable_intr(struct virtqueue *vq);
/**
 *  Dump virtqueue internal structures, for debug purpose only.
 */
void virtqueue_dump(struct virtqueue *vq);
/**
 *  Get all mbufs to be freed.
 */
struct rte_mbuf *virtqueue_detatch_unused(struct virtqueue *vq);

static inline int
virtqueue_full(const struct virtqueue *vq)
{
	return vq->vq_free_cnt == 0;
}

#define VIRTQUEUE_NUSED(vq) ((uint16_t)((vq)->vq_ring.used->idx - (vq)->vq_used_cons_idx))

static inline void __attribute__((always_inline))
vq_update_avail_idx(struct virtqueue *vq)
{
	rte_compiler_barrier();
	vq->vq_ring.avail->idx = vq->vq_avail_idx;
}

static inline void __attribute__((always_inline))
vq_update_avail_ring(struct virtqueue *vq, uint16_t desc_idx)
{
	uint16_t avail_idx;
	/*
	 * Place the head of the descriptor chain into the next slot and make
	 * it usable to the host. The chain is made available now rather than
	 * deferring to virtqueue_notify() in the hopes that if the host is
	 * currently running on another CPU, we can keep it processing the new
	 * descriptor.
	 */
	avail_idx = (uint16_t)(vq->vq_avail_idx & (vq->vq_nentries - 1));
	vq->vq_ring.avail->ring[avail_idx] = desc_idx;
	vq->vq_avail_idx++;
}

static inline int __attribute__((always_inline))
virtqueue_kick_prepare(struct virtqueue *vq)
{
	return !(vq->vq_ring.used->flags & VRING_USED_F_NO_NOTIFY);
}

static inline void __attribute__((always_inline))
virtqueue_notify(struct virtqueue *vq)
{
	/*
	 * Ensure updated avail->idx is visible to host. mb() necessary?
	 * For virtio on IA, the notificaiton is through io port operation
	 * which is a serialization instruction itself.
	 */
	VIRTIO_WRITE_REG_2(vq->hw, VIRTIO_PCI_QUEUE_NOTIFY, vq->vq_queue_index);
}

static inline void __attribute__((always_inline))
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

static inline int __attribute__((always_inline))
virtqueue_enqueue_recv_refill(struct virtqueue *vq, struct rte_mbuf *cookie)
{
	struct vq_desc_extra *dxp;
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
	start_dp[idx].addr  =
		(uint64_t) (cookie->buf_physaddr + RTE_PKTMBUF_HEADROOM - sizeof(struct virtio_net_hdr));
	start_dp[idx].len   = cookie->buf_len - RTE_PKTMBUF_HEADROOM + sizeof(struct virtio_net_hdr);
	start_dp[idx].flags =  VRING_DESC_F_WRITE;
	idx = start_dp[idx].next;
	vq->vq_desc_head_idx = idx;
	if (vq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END)
		vq->vq_desc_tail_idx = idx;
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - needed);
	vq_update_avail_ring(vq, head_idx);

	return 0;
}

static inline int __attribute__((always_inline))
virtqueue_enqueue_xmit(struct virtqueue *txvq, struct rte_mbuf *cookie)
{
	struct vq_desc_extra *dxp;
	struct vring_desc *start_dp;
	uint16_t needed = 2;
	uint16_t head_idx, idx;

	if (unlikely(txvq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(txvq->vq_free_cnt < needed))
		return -EMSGSIZE;
	head_idx = txvq->vq_desc_head_idx;
	if (unlikely(head_idx >= txvq->vq_nentries))
		return -EFAULT;

	idx = head_idx;
	dxp = &txvq->vq_descx[idx];
	if (dxp->cookie != NULL)
		rte_pktmbuf_free_seg(dxp->cookie);
	dxp->cookie = (void *)cookie;
	dxp->ndescs = needed;

	start_dp = txvq->vq_ring.desc;
	start_dp[idx].addr  = (uint64_t)(uintptr_t)txvq->virtio_net_hdr_mem + idx * sizeof(struct virtio_net_hdr);
	start_dp[idx].len   = sizeof(struct virtio_net_hdr);
	start_dp[idx].flags = VRING_DESC_F_NEXT;
	idx = start_dp[idx].next;
	start_dp[idx].addr  = RTE_MBUF_DATA_DMA_ADDR(cookie);
	start_dp[idx].len   = cookie->pkt.data_len;
	start_dp[idx].flags = 0;
	idx = start_dp[idx].next;
	txvq->vq_desc_head_idx = idx;
	if (txvq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END)
		txvq->vq_desc_tail_idx = idx;
	txvq->vq_free_cnt = (uint16_t)(txvq->vq_free_cnt - needed);
	vq_update_avail_ring(txvq, head_idx);

	return 0;
}

static inline uint16_t __attribute__((always_inline))
virtqueue_dequeue_burst_rx(struct virtqueue *vq, struct rte_mbuf **rx_pkts, uint32_t *len, uint16_t num)
{
	struct vring_used_elem *uep;
	struct rte_mbuf *cookie;
	uint16_t used_idx, desc_idx;
	uint16_t i;

	/*  Caller does the check */
	for (i = 0; i < num; i++) {
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
		rte_packet_prefetch(cookie->pkt.data);
		rx_pkts[i]  = cookie;
		vq->vq_used_cons_idx++;
		vq_ring_free_chain(vq, desc_idx);
		vq->vq_descx[desc_idx].cookie = NULL;
	}

	return i;
}

static inline uint16_t __attribute__((always_inline))
virtqueue_dequeue_pkt_tx(struct virtqueue *vq)
{
	struct vring_used_elem *uep;
	uint16_t used_idx, desc_idx;

	used_idx = (uint16_t)(vq->vq_used_cons_idx & (vq->vq_nentries - 1));
	uep = &vq->vq_ring.used->ring[used_idx];
	desc_idx = (uint16_t) uep->id;
	vq->vq_used_cons_idx++;
	vq_ring_free_chain(vq, desc_idx);

	return 0;
}

#ifdef RTE_LIBRTE_VIRTIO_DEBUG_DUMP
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

#endif /* _VIRTQUEUE_H_ */
