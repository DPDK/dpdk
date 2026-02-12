/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _VIRTQUEUE_H_
#define _VIRTQUEUE_H_

#include <stdint.h>

#include <rte_atomic.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_net.h>

#include "virtio.h"
#include "virtio_ring.h"
#include "virtio_logs.h"
#include "virtio_rxtx.h"
#include "virtio_cvq.h"

struct rte_mbuf;

#define DEFAULT_TX_FREE_THRESH 32
#define DEFAULT_RX_FREE_THRESH 32

#define VIRTIO_MBUF_BURST_SZ 64
/*
 * Per virtio_ring.h in Linux.
 *     For virtio_pci on SMP, we don't need to order with respect to MMIO
 *     accesses through relaxed memory I/O windows, so thread_fence is
 *     sufficient.
 *
 *     For using virtio to talk to real devices (eg. vDPA) we do need real
 *     barriers.
 */
static inline void
virtio_mb(uint8_t weak_barriers)
{
	if (weak_barriers)
		rte_atomic_thread_fence(rte_memory_order_seq_cst);
	else
		rte_mb();
}

static inline void
virtio_rmb(uint8_t weak_barriers)
{
	if (weak_barriers)
		rte_atomic_thread_fence(rte_memory_order_acquire);
	else
		rte_io_rmb();
}

static inline void
virtio_wmb(uint8_t weak_barriers)
{
	if (weak_barriers)
		rte_atomic_thread_fence(rte_memory_order_release);
	else
		rte_io_wmb();
}

static inline uint16_t
virtqueue_fetch_flags_packed(struct vring_packed_desc *dp,
			      uint8_t weak_barriers)
{
	uint16_t flags;

	if (weak_barriers) {
/* x86 prefers to using rte_io_rmb over rte_atomic_load_explicit as it reports
 * a better perf(~1.5%), which comes from the saved branch by the compiler.
 * The if and else branch are identical  on the platforms except Arm.
 */
#ifdef RTE_ARCH_ARM
		flags = rte_atomic_load_explicit(&dp->flags, rte_memory_order_acquire);
#else
		flags = dp->flags;
		rte_io_rmb();
#endif
	} else {
		flags = dp->flags;
		rte_io_rmb();
	}

	return flags;
}

static inline void
virtqueue_store_flags_packed(struct vring_packed_desc *dp,
			      uint16_t flags, uint8_t weak_barriers)
{
	if (weak_barriers) {
/* x86 prefers to using rte_io_wmb over rte_atomic_store_explicit as it reports
 * a better perf(~1.5%), which comes from the saved branch by the compiler.
 * The if and else branch are identical on the platforms except Arm.
 */
#ifdef RTE_ARCH_ARM
		rte_atomic_store_explicit(&dp->flags, flags, rte_memory_order_release);
#else
		rte_io_wmb();
		dp->flags = flags;
#endif
	} else {
		rte_io_wmb();
		dp->flags = flags;
	}
}

#ifdef RTE_PMD_PACKET_PREFETCH
#define rte_packet_prefetch(p)  rte_prefetch1(p)
#else
#define rte_packet_prefetch(p)  do {} while(0)
#endif

#define VIRTQUEUE_MAX_NAME_SZ 32

#ifdef RTE_ARCH_32
#define VIRTIO_MBUF_ADDR_MASK(vq) ((vq)->mbuf_addr_mask)
#else
#define VIRTIO_MBUF_ADDR_MASK(vq) UINT64_MAX
#endif

/**
 * Return the IOVA (or virtual address in case of virtio-user) of mbuf
 * data buffer.
 *
 * The address is firstly casted to the word size (sizeof(uintptr_t))
 * before casting it to uint64_t. It is then masked with the expected
 * address length (64 bits for virtio-pci, word size for virtio-user).
 *
 * This is to make it work with different combination of word size (64
 * bit and 32 bit) and virtio device (virtio-pci and virtio-user).
 */
#define VIRTIO_MBUF_ADDR(mb, vq) \
	((*(uint64_t *)((uintptr_t)(mb) + (vq)->mbuf_addr_offset)) & \
		VIRTIO_MBUF_ADDR_MASK(vq))

/**
 * Return the physical address (or virtual address in case of
 * virtio-user) of mbuf data buffer, taking care of mbuf data offset
 */
#define VIRTIO_MBUF_DATA_DMA_ADDR(mb, vq) \
	(VIRTIO_MBUF_ADDR(mb, vq) + (mb)->data_off)

#define VTNET_SQ_RQ_QUEUE_IDX 0
#define VTNET_SQ_TQ_QUEUE_IDX 1

enum { VTNET_RQ = 0, VTNET_TQ = 1, VTNET_CQ = 2 };
/**
 * The maximum virtqueue size is 2^15. Use that value as the end of
 * descriptor chain terminator since it will never be a valid index
 * in the descriptor table. This is used to verify we are correctly
 * handling vq_free_cnt.
 */
#define VQ_RING_DESC_CHAIN_END 32768

#define VIRTIO_NET_OK     0
#define VIRTIO_NET_ERR    1

struct vq_desc_extra {
	void *cookie;
	uint16_t ndescs;
	uint16_t next;
};

#define virtnet_rxq_to_vq(rxvq) container_of(rxvq, struct virtqueue, rxq)
#define virtnet_txq_to_vq(txvq) container_of(txvq, struct virtqueue, txq)
#define virtnet_cq_to_vq(cvq) container_of(cvq, struct virtqueue, cq)

struct virtqueue {
	struct virtio_hw  *hw; /**< virtio_hw structure pointer. */
	union {
		struct {
			/**< vring keeping desc, used and avail */
			struct vring ring;
		} vq_split;

		struct {
			/**< vring keeping descs and events */
			struct vring_packed ring;
			bool used_wrap_counter;
			uint16_t cached_flags; /**< cached flags for descs */
			uint16_t event_flags_shadow;
		} vq_packed;
	};

	uint16_t vq_used_cons_idx; /**< last consumed descriptor */
	uint16_t vq_nentries;  /**< vring desc numbers */
	uint16_t vq_free_cnt;  /**< num of desc available */
	uint16_t vq_avail_idx; /**< sync until needed */
	uint16_t vq_free_thresh; /**< free threshold */

	/**
	 * Head of the free chain in the descriptor table. If
	 * there are no free descriptors, this will be set to
	 * VQ_RING_DESC_CHAIN_END.
	 */
	uint16_t  vq_desc_head_idx;
	uint16_t  vq_desc_tail_idx;
	uint16_t  vq_queue_index;   /**< PCI queue index */

	void *vq_ring_virt_mem;  /**< linear address of vring*/
	unsigned int vq_ring_size;
	uint16_t mbuf_addr_offset;
	uint64_t mbuf_addr_mask;

	union {
		struct virtnet_rx rxq;
		struct virtnet_tx txq;
		struct virtnet_ctl cq;
	};

	const struct rte_memzone *mz; /**< mem zone to populate ring. */
	rte_iova_t vq_ring_mem; /**< physical address of vring,
	                         * or virtual address for virtio_user. */

	uint16_t  *notify_addr;
	struct vq_desc_extra vq_descx[];
};

/* If multiqueue is provided by host, then we support it. */
#define VIRTIO_NET_CTRL_MQ   4

#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET        0
#define VIRTIO_NET_CTRL_MQ_RSS_CONFIG          1

#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MIN        1
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX        0x8000

/**
 * This is the first element of the scatter-gather list.  If you don't
 * specify GSO or CSUM features, you can simply ignore the header.
 */
struct virtio_net_hdr {
#define VIRTIO_NET_HDR_F_NEEDS_CSUM 1    /**< Use csum_start,csum_offset*/
#define VIRTIO_NET_HDR_F_DATA_VALID 2    /**< Checksum is valid */
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
 * This is the version of the header to use when the HASH_REPORT
 * feature has been negotiated.
 */
struct virtio_net_hdr_hash_report {
	struct virtio_net_hdr_mrg_rxbuf hdr;
	uint32_t hash_value;
#define VIRTIO_NET_HASH_REPORT_NONE	0
#define VIRTIO_NET_HASH_REPORT_IPV4	1
#define VIRTIO_NET_HASH_REPORT_TCPV4	2
#define VIRTIO_NET_HASH_REPORT_UDPV4	3
#define VIRTIO_NET_HASH_REPORT_IPV6	4
#define VIRTIO_NET_HASH_REPORT_TCPV6	5
#define VIRTIO_NET_HASH_REPORT_UDPV6	6
#define VIRTIO_NET_HASH_REPORT_IPV6_EX	7
#define VIRTIO_NET_HASH_REPORT_TCPV6_EX	8
#define VIRTIO_NET_HASH_REPORT_UDPV6_EX	9
	uint16_t hash_report;
	uint16_t pad_reserved;
};

/* Region reserved to allow for transmit header and indirect ring */
#define VIRTIO_MAX_TX_INDIRECT 8
struct virtio_tx_region {
	struct virtio_net_hdr_mrg_rxbuf tx_hdr;
	union __rte_aligned(16) {
		struct vring_desc tx_indir[VIRTIO_MAX_TX_INDIRECT];
		struct vring_packed_desc
			tx_packed_indir[VIRTIO_MAX_TX_INDIRECT];
	};
};

static inline int
desc_is_used(struct vring_packed_desc *desc, struct virtqueue *vq)
{
	uint16_t used, avail, flags;

	flags = virtqueue_fetch_flags_packed(desc, vq->hw->weak_barriers);
	used = !!(flags & VRING_PACKED_DESC_F_USED);
	avail = !!(flags & VRING_PACKED_DESC_F_AVAIL);

	return avail == used && used == vq->vq_packed.used_wrap_counter;
}

static inline void
vring_desc_init_packed(struct virtqueue *vq, int n)
{
	int i;
	for (i = 0; i < n - 1; i++) {
		vq->vq_packed.ring.desc[i].id = i;
		vq->vq_descx[i].next = i + 1;
	}
	vq->vq_packed.ring.desc[i].id = i;
	vq->vq_descx[i].next = VQ_RING_DESC_CHAIN_END;
}

/* Chain all the descriptors in the ring with an END */
static inline void
vring_desc_init_split(struct vring_desc *dp, uint16_t n)
{
	uint16_t i;

	for (i = 0; i < n - 1; i++)
		dp[i].next = (uint16_t)(i + 1);
	dp[i].next = VQ_RING_DESC_CHAIN_END;
}

static inline void
vring_desc_init_indirect_packed(struct vring_packed_desc *dp, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		dp[i].id = (uint16_t)i;
		dp[i].flags = VRING_DESC_F_WRITE;
	}
}

/**
 * Tell the backend not to interrupt us. Implementation for packed virtqueues.
 */
static inline void
virtqueue_disable_intr_packed(struct virtqueue *vq)
{
	if (vq->vq_packed.event_flags_shadow != RING_EVENT_FLAGS_DISABLE) {
		vq->vq_packed.event_flags_shadow = RING_EVENT_FLAGS_DISABLE;
		vq->vq_packed.ring.driver->desc_event_flags =
			vq->vq_packed.event_flags_shadow;
	}
}

/**
 * Tell the backend not to interrupt us. Implementation for split virtqueues.
 */
static inline void
virtqueue_disable_intr_split(struct virtqueue *vq)
{
	vq->vq_split.ring.avail->flags |= VRING_AVAIL_F_NO_INTERRUPT;
}

/**
 * Tell the backend not to interrupt us.
 */
static inline void
virtqueue_disable_intr(struct virtqueue *vq)
{
	if (virtio_with_packed_queue(vq->hw))
		virtqueue_disable_intr_packed(vq);
	else
		virtqueue_disable_intr_split(vq);
}

/**
 * Tell the backend to interrupt. Implementation for packed virtqueues.
 */
static inline void
virtqueue_enable_intr_packed(struct virtqueue *vq)
{
	if (vq->vq_packed.event_flags_shadow == RING_EVENT_FLAGS_DISABLE) {
		vq->vq_packed.event_flags_shadow = RING_EVENT_FLAGS_ENABLE;
		vq->vq_packed.ring.driver->desc_event_flags =
			vq->vq_packed.event_flags_shadow;
	}
}

/**
 * Tell the backend to interrupt. Implementation for split virtqueues.
 */
static inline void
virtqueue_enable_intr_split(struct virtqueue *vq)
{
	vq->vq_split.ring.avail->flags &= (~VRING_AVAIL_F_NO_INTERRUPT);
}

/**
 * Tell the backend to interrupt us.
 */
static inline void
virtqueue_enable_intr(struct virtqueue *vq)
{
	if (virtio_with_packed_queue(vq->hw))
		virtqueue_enable_intr_packed(vq);
	else
		virtqueue_enable_intr_split(vq);
}

/**
 *  Get all mbufs to be freed.
 */
struct rte_mbuf *virtqueue_detach_unused(struct virtqueue *vq);

/* Flush the elements in the used ring. */
void virtqueue_rxvq_flush(struct virtqueue *vq);

int virtqueue_rxvq_reset_packed(struct virtqueue *vq);

int virtqueue_txvq_reset_packed(struct virtqueue *vq);

void virtqueue_txq_indirect_headers_init(struct virtqueue *vq);

struct virtqueue *virtqueue_alloc(struct virtio_hw *hw, uint16_t index,
		uint16_t num, int type, int node, const char *name);

void virtqueue_free(struct virtqueue *vq);

static inline int
virtqueue_full(const struct virtqueue *vq)
{
	return vq->vq_free_cnt == 0;
}

static inline int
virtio_get_queue_type(struct virtio_hw *hw, uint16_t vq_idx)
{
	if (vq_idx == hw->max_queue_pairs * 2)
		return VTNET_CQ;
	else if (vq_idx % 2 == 0)
		return VTNET_RQ;
	else
		return VTNET_TQ;
}

/* virtqueue_nused has load-acquire or rte_io_rmb insed */
static inline uint16_t
virtqueue_nused(const struct virtqueue *vq)
{
	uint16_t idx;

	if (vq->hw->weak_barriers) {
	/**
	 * x86 prefers to using rte_smp_rmb over rte_atomic_load_explicit as it
	 * reports a slightly better perf, which comes from the saved
	 * branch by the compiler.
	 * The if and else branches are identical with the smp and io
	 * barriers both defined as compiler barriers on x86.
	 */
#ifdef RTE_ARCH_X86_64
		idx = vq->vq_split.ring.used->idx;
		rte_smp_rmb();
#else
		idx = rte_atomic_load_explicit(&(vq)->vq_split.ring.used->idx,
				rte_memory_order_acquire);
#endif
	} else {
		idx = vq->vq_split.ring.used->idx;
		rte_io_rmb();
	}
	return idx - vq->vq_used_cons_idx;
}

void vq_ring_free_chain(struct virtqueue *vq, uint16_t desc_idx);
void vq_ring_free_chain_packed(struct virtqueue *vq, uint16_t used_idx);
void vq_ring_free_inorder(struct virtqueue *vq, uint16_t desc_idx,
			  uint16_t num);

static inline void
vq_update_avail_idx(struct virtqueue *vq)
{
	if (vq->hw->weak_barriers) {
	/* x86 prefers to using rte_smp_wmb over rte_atomic_store_explicit as
	 * it reports a slightly better perf, which comes from the
	 * saved branch by the compiler.
	 * The if and else branches are identical with the smp and
	 * io barriers both defined as compiler barriers on x86.
	 */
#ifdef RTE_ARCH_X86_64
		rte_smp_wmb();
		vq->vq_split.ring.avail->idx = vq->vq_avail_idx;
#else
		rte_atomic_store_explicit(&vq->vq_split.ring.avail->idx,
				 vq->vq_avail_idx, rte_memory_order_release);
#endif
	} else {
		rte_io_wmb();
		vq->vq_split.ring.avail->idx = vq->vq_avail_idx;
	}
}

static inline void
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
	if (unlikely(vq->vq_split.ring.avail->ring[avail_idx] != desc_idx))
		vq->vq_split.ring.avail->ring[avail_idx] = desc_idx;
	vq->vq_avail_idx++;
}

static inline int
virtqueue_kick_prepare(struct virtqueue *vq)
{
	/*
	 * Ensure updated avail->idx is visible to vhost before reading
	 * the used->flags.
	 */
	virtio_mb(vq->hw->weak_barriers);
	return !(vq->vq_split.ring.used->flags & VRING_USED_F_NO_NOTIFY);
}

static inline int
virtqueue_kick_prepare_packed(struct virtqueue *vq)
{
	uint16_t flags;

	/*
	 * Ensure updated data is visible to vhost before reading the flags.
	 */
	virtio_mb(vq->hw->weak_barriers);
	flags = vq->vq_packed.ring.device->desc_event_flags;

	return flags != RING_EVENT_FLAGS_DISABLE;
}

/*
 * virtqueue_kick_prepare*() or the virtio_wmb() should be called
 * before this function to be sure that all the data is visible to vhost.
 */
static inline void
virtqueue_notify(struct virtqueue *vq)
{
	VIRTIO_OPS(vq->hw)->notify_queue(vq->hw, vq);
}

#ifdef RTE_LIBRTE_VIRTIO_DEBUG_DUMP
#define VIRTQUEUE_DUMP(vq) do { \
	uint16_t used_idx, nused; \
	used_idx = rte_atomic_load_explicit(&(vq)->vq_split.ring.used->idx, \
				   rte_memory_order_relaxed); \
	nused = (uint16_t)(used_idx - (vq)->vq_used_cons_idx); \
	if (virtio_with_packed_queue((vq)->hw)) { \
		PMD_INIT_LOG(DEBUG, \
		"VQ: - size=%d; free=%d; used_cons_idx=%d; avail_idx=%d;" \
		" cached_flags=0x%x; used_wrap_counter=%d", \
		(vq)->vq_nentries, (vq)->vq_free_cnt, (vq)->vq_used_cons_idx, \
		(vq)->vq_avail_idx, (vq)->vq_packed.cached_flags, \
		(vq)->vq_packed.used_wrap_counter); \
		break; \
	} \
	PMD_INIT_LOG(DEBUG, \
	  "VQ: - size=%d; free=%d; used=%d; desc_head_idx=%d;" \
	  " avail.idx=%d; used_cons_idx=%d; used.idx=%d;" \
	  " avail.flags=0x%x; used.flags=0x%x", \
	  (vq)->vq_nentries, (vq)->vq_free_cnt, nused, (vq)->vq_desc_head_idx, \
	  (vq)->vq_split.ring.avail->idx, (vq)->vq_used_cons_idx, \
	  rte_atomic_load_explicit(&(vq)->vq_split.ring.used->idx, rte_memory_order_relaxed), \
	  (vq)->vq_split.ring.avail->flags, (vq)->vq_split.ring.used->flags); \
} while (0)
#else
#define VIRTQUEUE_DUMP(vq) do { } while (0)
#endif

/* avoid write operation when necessary, to lessen cache issues */
#define ASSIGN_UNLESS_EQUAL(var, val) do {	\
	typeof(var) *const var_ = &(var);	\
	typeof(val)  const val_ = (val);	\
	if (*var_ != val_)			\
		*var_ = val_;			\
} while (0)

#define virtqueue_clear_net_hdr(hdr) do {		\
	typeof(hdr) hdr_ = (hdr);			\
	ASSIGN_UNLESS_EQUAL((hdr_)->csum_start, 0);	\
	ASSIGN_UNLESS_EQUAL((hdr_)->csum_offset, 0);	\
	ASSIGN_UNLESS_EQUAL((hdr_)->flags, 0);		\
	ASSIGN_UNLESS_EQUAL((hdr_)->gso_type, 0);	\
	ASSIGN_UNLESS_EQUAL((hdr_)->gso_size, 0);	\
	ASSIGN_UNLESS_EQUAL((hdr_)->hdr_len, 0);	\
} while (0)

static inline void
virtqueue_xmit_offload(struct virtio_net_hdr *hdr, struct rte_mbuf *cookie)
{
	uint64_t csum_l4 = cookie->ol_flags & RTE_MBUF_F_TX_L4_MASK;
	uint16_t o_l23_len = (cookie->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) ?
			     cookie->outer_l2_len + cookie->outer_l3_len : 0;

	if (cookie->ol_flags & RTE_MBUF_F_TX_TCP_SEG)
		csum_l4 |= RTE_MBUF_F_TX_TCP_CKSUM;

	switch (csum_l4) {
	case RTE_MBUF_F_TX_UDP_CKSUM:
		hdr->csum_start = o_l23_len + cookie->l2_len + cookie->l3_len;
		hdr->csum_offset = offsetof(struct rte_udp_hdr, dgram_cksum);
		hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
		break;

	case RTE_MBUF_F_TX_TCP_CKSUM:
		hdr->csum_start = o_l23_len + cookie->l2_len + cookie->l3_len;
		hdr->csum_offset = offsetof(struct rte_tcp_hdr, cksum);
		hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
		break;

	default:
		ASSIGN_UNLESS_EQUAL(hdr->csum_start, 0);
		ASSIGN_UNLESS_EQUAL(hdr->csum_offset, 0);
		ASSIGN_UNLESS_EQUAL(hdr->flags, 0);
		break;
	}

	/* TCP Segmentation Offload */
	if (cookie->ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		hdr->gso_type = (cookie->ol_flags & RTE_MBUF_F_TX_IPV6) ?
			VIRTIO_NET_HDR_GSO_TCPV6 :
			VIRTIO_NET_HDR_GSO_TCPV4;
		hdr->gso_size = cookie->tso_segsz;
		hdr->hdr_len = o_l23_len + cookie->l2_len + cookie->l3_len +
			       cookie->l4_len;
	} else {
		ASSIGN_UNLESS_EQUAL(hdr->gso_type, 0);
		ASSIGN_UNLESS_EQUAL(hdr->gso_size, 0);
		ASSIGN_UNLESS_EQUAL(hdr->hdr_len, 0);
	}
}

static inline void
virtqueue_enqueue_xmit_packed(struct virtnet_tx *txvq, struct rte_mbuf *cookie,
			      uint16_t needed, int use_indirect, int can_push,
			      int in_order)
{
	struct virtio_tx_region *txr = txvq->hdr_mz->addr;
	struct vq_desc_extra *dxp;
	struct virtqueue *vq = virtnet_txq_to_vq(txvq);
	struct vring_packed_desc *start_dp, *head_dp;
	uint16_t idx, id, head_idx, head_flags;
	int16_t head_size = vq->hw->vtnet_hdr_size;
	struct virtio_net_hdr *hdr;
	uint16_t prev;
	bool prepend_header = false;
	uint16_t seg_num = cookie->nb_segs;

	id = in_order ? vq->vq_avail_idx : vq->vq_desc_head_idx;

	dxp = &vq->vq_descx[id];
	dxp->ndescs = needed;
	dxp->cookie = cookie;

	head_idx = vq->vq_avail_idx;
	idx = head_idx;
	prev = head_idx;
	start_dp = vq->vq_packed.ring.desc;

	head_dp = &vq->vq_packed.ring.desc[idx];
	head_flags = cookie->next ? VRING_DESC_F_NEXT : 0;
	head_flags |= vq->vq_packed.cached_flags;

	if (can_push) {
		/* prepend cannot fail, checked by caller */
		hdr = rte_pktmbuf_mtod_offset(cookie, struct virtio_net_hdr *,
					      -head_size);
		prepend_header = true;

		/* if offload disabled, it is not zeroed below, do it now */
		if (!vq->hw->has_tx_offload)
			virtqueue_clear_net_hdr(hdr);
	} else if (use_indirect) {
		/* setup tx ring slot to point to indirect
		 * descriptor list stored in reserved region.
		 *
		 * the first slot in indirect ring is already preset
		 * to point to the header in reserved region
		 */
		start_dp[idx].addr = txvq->hdr_mem + RTE_PTR_DIFF(&txr[idx].tx_packed_indir, txr);
		start_dp[idx].len = (seg_num + 1) * sizeof(struct vring_packed_desc);
		/* Packed descriptor id needs to be restored when inorder. */
		if (in_order)
			start_dp[idx].id = idx;
		/* reset flags for indirect desc */
		head_flags = VRING_DESC_F_INDIRECT;
		head_flags |= vq->vq_packed.cached_flags;
		hdr = (struct virtio_net_hdr *)&txr[idx].tx_hdr;

		/* loop below will fill in rest of the indirect elements */
		start_dp = txr[idx].tx_packed_indir;
		idx = 1;
	} else {
		/* setup first tx ring slot to point to header
		 * stored in reserved region.
		 */
		start_dp[idx].addr = txvq->hdr_mem + RTE_PTR_DIFF(&txr[idx].tx_hdr, txr);
		start_dp[idx].len = vq->hw->vtnet_hdr_size;
		head_flags |= VRING_DESC_F_NEXT;
		hdr = (struct virtio_net_hdr *)&txr[idx].tx_hdr;
		idx++;
		if (idx >= vq->vq_nentries) {
			idx -= vq->vq_nentries;
			vq->vq_packed.cached_flags ^=
				VRING_PACKED_DESC_F_AVAIL_USED;
		}
	}

	if (vq->hw->has_tx_offload)
		virtqueue_xmit_offload(hdr, cookie);

	do {
		uint16_t flags;

		start_dp[idx].addr = VIRTIO_MBUF_DATA_DMA_ADDR(cookie, vq);
		start_dp[idx].len  = cookie->data_len;
		if (prepend_header) {
			start_dp[idx].addr -= head_size;
			start_dp[idx].len += head_size;
			prepend_header = false;
		}

		if (likely(idx != head_idx)) {
			flags = cookie->next ? VRING_DESC_F_NEXT : 0;
			flags |= vq->vq_packed.cached_flags;
			start_dp[idx].flags = flags;
		}
		prev = idx;
		idx++;
		if (idx >= vq->vq_nentries) {
			idx -= vq->vq_nentries;
			vq->vq_packed.cached_flags ^=
				VRING_PACKED_DESC_F_AVAIL_USED;
		}
	} while ((cookie = cookie->next) != NULL);

	start_dp[prev].id = id;

	if (use_indirect) {
		idx = head_idx;
		if (++idx >= vq->vq_nentries) {
			idx -= vq->vq_nentries;
			vq->vq_packed.cached_flags ^=
				VRING_PACKED_DESC_F_AVAIL_USED;
		}
	}

	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - needed);
	vq->vq_avail_idx = idx;

	if (!in_order) {
		vq->vq_desc_head_idx = dxp->next;
		if (vq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END)
			vq->vq_desc_tail_idx = VQ_RING_DESC_CHAIN_END;
	}

	virtqueue_store_flags_packed(head_dp, head_flags,
				     vq->hw->weak_barriers);
}

static void
vq_ring_free_id_packed(struct virtqueue *vq, uint16_t id)
{
	struct vq_desc_extra *dxp;

	dxp = &vq->vq_descx[id];
	vq->vq_free_cnt += dxp->ndescs;

	if (vq->vq_desc_tail_idx == VQ_RING_DESC_CHAIN_END)
		vq->vq_desc_head_idx = id;
	else
		vq->vq_descx[vq->vq_desc_tail_idx].next = id;

	vq->vq_desc_tail_idx = id;
	dxp->next = VQ_RING_DESC_CHAIN_END;
}

static void
virtio_xmit_cleanup_inorder_packed(struct virtqueue *vq, uint16_t num)
{
	uint16_t used_idx, id, curr_id, free_cnt = 0;
	uint16_t size = vq->vq_nentries;
	struct vring_packed_desc *desc = vq->vq_packed.ring.desc;
	struct vq_desc_extra *dxp;
	int nb = num;

	used_idx = vq->vq_used_cons_idx;
	/* desc_is_used has a load-acquire or rte_io_rmb inside
	 * and wait for used desc in virtqueue.
	 */
	while (nb > 0 && desc_is_used(&desc[used_idx], vq)) {
		id = desc[used_idx].id;
		do {
			curr_id = used_idx;
			dxp = &vq->vq_descx[used_idx];
			used_idx += dxp->ndescs;
			free_cnt += dxp->ndescs;
			nb -= dxp->ndescs;
			if (used_idx >= size) {
				used_idx -= size;
				vq->vq_packed.used_wrap_counter ^= 1;
			}
			if (dxp->cookie != NULL) {
				rte_pktmbuf_free(dxp->cookie);
				dxp->cookie = NULL;
			}
		} while (curr_id != id);
	}
	vq->vq_used_cons_idx = used_idx;
	vq->vq_free_cnt += free_cnt;
}

static void
virtio_xmit_cleanup_normal_packed(struct virtqueue *vq, uint16_t num)
{
	uint16_t used_idx, id;
	uint16_t size = vq->vq_nentries;
	struct vring_packed_desc *desc = vq->vq_packed.ring.desc;
	struct vq_desc_extra *dxp;

	used_idx = vq->vq_used_cons_idx;
	/* desc_is_used has a load-acquire or rte_io_rmb inside
	 * and wait for used desc in virtqueue.
	 */
	while (num-- && desc_is_used(&desc[used_idx], vq)) {
		id = desc[used_idx].id;
		dxp = &vq->vq_descx[id];
		vq->vq_used_cons_idx += dxp->ndescs;
		if (vq->vq_used_cons_idx >= size) {
			vq->vq_used_cons_idx -= size;
			vq->vq_packed.used_wrap_counter ^= 1;
		}
		vq_ring_free_id_packed(vq, id);
		if (dxp->cookie != NULL) {
			rte_pktmbuf_free(dxp->cookie);
			dxp->cookie = NULL;
		}
		used_idx = vq->vq_used_cons_idx;
	}
}

/* Cleanup from completed transmits. */
static inline void
virtio_xmit_cleanup_packed(struct virtqueue *vq, uint16_t num, int in_order)
{
	if (in_order)
		virtio_xmit_cleanup_inorder_packed(vq, num);
	else
		virtio_xmit_cleanup_normal_packed(vq, num);
}

static inline void
virtio_xmit_cleanup(struct virtqueue *vq, uint16_t num)
{
	uint16_t i, used_idx, desc_idx;
	for (i = 0; i < num; i++) {
		struct vring_used_elem *uep;
		struct vq_desc_extra *dxp;

		used_idx = (uint16_t)(vq->vq_used_cons_idx &
				(vq->vq_nentries - 1));
		uep = &vq->vq_split.ring.used->ring[used_idx];

		desc_idx = (uint16_t)uep->id;
		dxp = &vq->vq_descx[desc_idx];
		vq->vq_used_cons_idx++;
		vq_ring_free_chain(vq, desc_idx);

		if (dxp->cookie != NULL) {
			rte_pktmbuf_free(dxp->cookie);
			dxp->cookie = NULL;
		}
	}
}

/* Cleanup from completed inorder transmits. */
static __rte_always_inline void
virtio_xmit_cleanup_inorder(struct virtqueue *vq, uint16_t num)
{
	uint16_t i, idx = vq->vq_used_cons_idx;
	int16_t free_cnt = 0;
	struct vq_desc_extra *dxp = NULL;

	if (unlikely(num == 0))
		return;

	for (i = 0; i < num; i++) {
		dxp = &vq->vq_descx[idx++ & (vq->vq_nentries - 1)];
		free_cnt += dxp->ndescs;
		if (dxp->cookie != NULL) {
			rte_pktmbuf_free(dxp->cookie);
			dxp->cookie = NULL;
		}
	}

	vq->vq_free_cnt += free_cnt;
	vq->vq_used_cons_idx = idx;
}
#endif /* _VIRTQUEUE_H_ */
