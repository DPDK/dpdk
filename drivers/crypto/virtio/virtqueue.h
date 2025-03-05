/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */

#ifndef _VIRTQUEUE_H_
#define _VIRTQUEUE_H_

#include <stdint.h>

#include <rte_atomic.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mempool.h>

#include "virtio_cvq.h"
#include "virtio_pci.h"
#include "virtio_ring.h"
#include "virtio_logs.h"
#include "virtio_crypto.h"
#include "virtio_rxtx.h"

struct rte_mbuf;

/*
 * Per virtio_config.h in Linux.
 *     For virtio_pci on SMP, we don't need to order with respect to MMIO
 *     accesses through relaxed memory I/O windows, so smp_mb() et al are
 *     sufficient.
 *
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

#define VIRTQUEUE_MAX_NAME_SZ 32

enum { VTCRYPTO_DATAQ = 0, VTCRYPTO_CTRLQ = 1 };

/**
 * The maximum virtqueue size is 2^15. Use that value as the end of
 * descriptor chain terminator since it will never be a valid index
 * in the descriptor table. This is used to verify we are correctly
 * handling vq_free_cnt.
 */
#define VQ_RING_DESC_CHAIN_END 32768

struct vq_desc_extra {
	void     *crypto_op;
	void     *cookie;
	uint16_t ndescs;
	uint16_t next;
};

#define virtcrypto_dq_to_vq(dvq) container_of(dvq, struct virtqueue, dq)
#define virtcrypto_cq_to_vq(cvq) container_of(cvq, struct virtqueue, cq)

struct virtqueue {
	/**< virtio_crypto_hw structure pointer. */
	struct virtio_crypto_hw *hw;
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

	union {
		struct virtcrypto_data dq;
		struct virtcrypto_ctl cq;
	};

	/**< mem zone to populate RX ring. */
	const struct rte_memzone *mz;
	/**< memzone to populate hdr and request. */
	struct rte_mempool *mpool;
	uint8_t     dev_id;              /**< Device identifier. */
	uint16_t    vq_queue_index;       /**< PCI queue index */

	void        *vq_ring_virt_mem;    /**< linear address of vring*/
	unsigned int vq_ring_size;
	phys_addr_t vq_ring_mem;          /**< physical address of vring */

	uint16_t    vq_free_cnt; /**< num of desc available */
	uint16_t    vq_nentries; /**< vring desc numbers */

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

	/* Statistics */
	uint64_t	packets_sent_total;
	uint64_t	packets_sent_failed;
	uint64_t	packets_received_total;
	uint64_t	packets_received_failed;

	uint16_t  *notify_addr;

	struct vq_desc_extra vq_descx[];
};

/**
 * Tell the backend not to interrupt us.
 */
void virtqueue_disable_intr(struct virtqueue *vq);

/**
 *  Get all mbufs to be freed.
 */
void virtqueue_detatch_unused(struct virtqueue *vq);

struct virtqueue *virtcrypto_queue_alloc(struct virtio_crypto_hw *hw, uint16_t index,
		uint16_t num, int node, const char *name);

void virtcrypto_queue_free(struct virtqueue *vq);

static inline int
virtqueue_full(const struct virtqueue *vq)
{
	return vq->vq_free_cnt == 0;
}

#define VIRTQUEUE_NUSED(vq) \
	((uint16_t)((vq)->vq_split.ring.used->idx - (vq)->vq_used_cons_idx))

static inline void
vq_update_avail_idx(struct virtqueue *vq)
{
	virtio_wmb(0);
	vq->vq_split.ring.avail->idx = vq->vq_avail_idx;
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
	return !(vq->vq_split.ring.used->flags & VRING_USED_F_NO_NOTIFY);
}

static inline void
virtqueue_notify(struct virtqueue *vq)
{
	/*
	 * Ensure updated avail->idx is visible to host.
	 * For virtio on IA, the notification is through io port operation
	 * which is a serialization instruction itself.
	 */
	VTPCI_OPS(vq->hw)->notify_queue(vq->hw, vq);
}

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

static inline int
virtio_get_queue_type(struct virtio_crypto_hw *hw, uint16_t vq_idx)
{
	if (vq_idx == hw->max_dataqueues)
		return VTCRYPTO_CTRLQ;
	else
		return VTCRYPTO_DATAQ;
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
		virtio_rmb(0);
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

/**
 * Dump virtqueue internal structures, for debug purpose only.
 */
#define VIRTQUEUE_SPLIT_DUMP(vq) do { \
	uint16_t used_idx, nused; \
	used_idx = (vq)->vq_split.ring.used->idx; \
	nused = (uint16_t)(used_idx - (vq)->vq_used_cons_idx); \
	VIRTIO_CRYPTO_INIT_LOG_DBG(\
	  "VQ: - size=%d; free=%d; used=%d; desc_head_idx=%d;" \
	  " avail.idx=%d; used_cons_idx=%d; used.idx=%d;" \
	  " avail.flags=0x%x; used.flags=0x%x", \
	  (vq)->vq_nentries, (vq)->vq_free_cnt, nused, \
	  (vq)->vq_desc_head_idx, (vq)->vq_split.ring.avail->idx, \
	  (vq)->vq_used_cons_idx, (vq)->vq_split.ring.used->idx, \
	  (vq)->vq_split.ring.avail->flags, (vq)->vq_split.ring.used->flags); \
} while (0)

#define VIRTQUEUE_PACKED_DUMP(vq) do { \
	uint16_t nused; \
	nused = (vq)->vq_nentries - (vq)->vq_free_cnt; \
	VIRTIO_CRYPTO_INIT_LOG_DBG(\
	  "VQ: - size=%d; free=%d; used=%d; desc_head_idx=%d;" \
	  " avail_idx=%d; used_cons_idx=%d;" \
	  " avail.flags=0x%x; wrap_counter=%d", \
	  (vq)->vq_nentries, (vq)->vq_free_cnt, nused, \
	  (vq)->vq_desc_head_idx, (vq)->vq_avail_idx, \
	  (vq)->vq_used_cons_idx, (vq)->vq_packed.cached_flags, \
	  (vq)->vq_packed.used_wrap_counter); \
} while (0)

#define VIRTQUEUE_DUMP(vq) do { \
	if (vtpci_with_packed_queue((vq)->hw)) \
		VIRTQUEUE_PACKED_DUMP(vq); \
	else \
		VIRTQUEUE_SPLIT_DUMP(vq); \
} while (0)

#endif /* _VIRTQUEUE_H_ */
