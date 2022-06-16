/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdint.h>
#include <stdbool.h>
#include <linux/virtio_net.h>

#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_net.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_vhost.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_sctp.h>
#include <rte_arp.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>
#include <rte_vhost_async.h>

#include "iotlb.h"
#include "vhost.h"

#define MAX_BATCH_LEN 256

static  __rte_always_inline bool
rxvq_is_mergeable(struct virtio_net *dev)
{
	return dev->features & (1ULL << VIRTIO_NET_F_MRG_RXBUF);
}

static  __rte_always_inline bool
virtio_net_is_inorder(struct virtio_net *dev)
{
	return dev->features & (1ULL << VIRTIO_F_IN_ORDER);
}

static bool
is_valid_virt_queue_idx(uint32_t idx, int is_tx, uint32_t nr_vring)
{
	return (is_tx ^ (idx & 1)) == 0 && idx < nr_vring;
}

static inline void
do_data_copy_enqueue(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	struct batch_copy_elem *elem = vq->batch_copy_elems;
	uint16_t count = vq->batch_copy_nb_elems;
	int i;

	for (i = 0; i < count; i++) {
		rte_memcpy(elem[i].dst, elem[i].src, elem[i].len);
		vhost_log_cache_write_iova(dev, vq, elem[i].log_addr,
					   elem[i].len);
		PRINT_PACKET(dev, (uintptr_t)elem[i].dst, elem[i].len, 0);
	}

	vq->batch_copy_nb_elems = 0;
}

static inline void
do_data_copy_dequeue(struct vhost_virtqueue *vq)
{
	struct batch_copy_elem *elem = vq->batch_copy_elems;
	uint16_t count = vq->batch_copy_nb_elems;
	int i;

	for (i = 0; i < count; i++)
		rte_memcpy(elem[i].dst, elem[i].src, elem[i].len);

	vq->batch_copy_nb_elems = 0;
}

static __rte_always_inline void
do_flush_shadow_used_ring_split(struct virtio_net *dev,
			struct vhost_virtqueue *vq,
			uint16_t to, uint16_t from, uint16_t size)
{
	rte_memcpy(&vq->used->ring[to],
			&vq->shadow_used_split[from],
			size * sizeof(struct vring_used_elem));
	vhost_log_cache_used_vring(dev, vq,
			offsetof(struct vring_used, ring[to]),
			size * sizeof(struct vring_used_elem));
}

static __rte_always_inline void
flush_shadow_used_ring_split(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	uint16_t used_idx = vq->last_used_idx & (vq->size - 1);

	if (used_idx + vq->shadow_used_idx <= vq->size) {
		do_flush_shadow_used_ring_split(dev, vq, used_idx, 0,
					  vq->shadow_used_idx);
	} else {
		uint16_t size;

		/* update used ring interval [used_idx, vq->size] */
		size = vq->size - used_idx;
		do_flush_shadow_used_ring_split(dev, vq, used_idx, 0, size);

		/* update the left half used ring interval [0, left_size] */
		do_flush_shadow_used_ring_split(dev, vq, 0, size,
					  vq->shadow_used_idx - size);
	}
	vq->last_used_idx += vq->shadow_used_idx;

	vhost_log_cache_sync(dev, vq);

	__atomic_add_fetch(&vq->used->idx, vq->shadow_used_idx,
			   __ATOMIC_RELEASE);
	vq->shadow_used_idx = 0;
	vhost_log_used_vring(dev, vq, offsetof(struct vring_used, idx),
		sizeof(vq->used->idx));
}

static __rte_always_inline void
update_shadow_used_ring_split(struct vhost_virtqueue *vq,
			 uint16_t desc_idx, uint32_t len)
{
	uint16_t i = vq->shadow_used_idx++;

	vq->shadow_used_split[i].id  = desc_idx;
	vq->shadow_used_split[i].len = len;
}

static __rte_always_inline void
vhost_flush_enqueue_shadow_packed(struct virtio_net *dev,
				  struct vhost_virtqueue *vq)
{
	int i;
	uint16_t used_idx = vq->last_used_idx;
	uint16_t head_idx = vq->last_used_idx;
	uint16_t head_flags = 0;

	/* Split loop in two to save memory barriers */
	for (i = 0; i < vq->shadow_used_idx; i++) {
		vq->desc_packed[used_idx].id = vq->shadow_used_packed[i].id;
		vq->desc_packed[used_idx].len = vq->shadow_used_packed[i].len;

		used_idx += vq->shadow_used_packed[i].count;
		if (used_idx >= vq->size)
			used_idx -= vq->size;
	}

	/* The ordering for storing desc flags needs to be enforced. */
	rte_atomic_thread_fence(__ATOMIC_RELEASE);

	for (i = 0; i < vq->shadow_used_idx; i++) {
		uint16_t flags;

		if (vq->shadow_used_packed[i].len)
			flags = VRING_DESC_F_WRITE;
		else
			flags = 0;

		if (vq->used_wrap_counter) {
			flags |= VRING_DESC_F_USED;
			flags |= VRING_DESC_F_AVAIL;
		} else {
			flags &= ~VRING_DESC_F_USED;
			flags &= ~VRING_DESC_F_AVAIL;
		}

		if (i > 0) {
			vq->desc_packed[vq->last_used_idx].flags = flags;

			vhost_log_cache_used_vring(dev, vq,
					vq->last_used_idx *
					sizeof(struct vring_packed_desc),
					sizeof(struct vring_packed_desc));
		} else {
			head_idx = vq->last_used_idx;
			head_flags = flags;
		}

		vq_inc_last_used_packed(vq, vq->shadow_used_packed[i].count);
	}

	vq->desc_packed[head_idx].flags = head_flags;

	vhost_log_cache_used_vring(dev, vq,
				head_idx *
				sizeof(struct vring_packed_desc),
				sizeof(struct vring_packed_desc));

	vq->shadow_used_idx = 0;
	vhost_log_cache_sync(dev, vq);
}

static __rte_always_inline void
vhost_flush_dequeue_shadow_packed(struct virtio_net *dev,
				  struct vhost_virtqueue *vq)
{
	struct vring_used_elem_packed *used_elem = &vq->shadow_used_packed[0];

	vq->desc_packed[vq->shadow_last_used_idx].id = used_elem->id;
	/* desc flags is the synchronization point for virtio packed vring */
	__atomic_store_n(&vq->desc_packed[vq->shadow_last_used_idx].flags,
			 used_elem->flags, __ATOMIC_RELEASE);

	vhost_log_cache_used_vring(dev, vq, vq->shadow_last_used_idx *
				   sizeof(struct vring_packed_desc),
				   sizeof(struct vring_packed_desc));
	vq->shadow_used_idx = 0;
	vhost_log_cache_sync(dev, vq);
}

static __rte_always_inline void
vhost_flush_enqueue_batch_packed(struct virtio_net *dev,
				 struct vhost_virtqueue *vq,
				 uint64_t *lens,
				 uint16_t *ids)
{
	uint16_t i;
	uint16_t flags;
	uint16_t last_used_idx;
	struct vring_packed_desc *desc_base;

	last_used_idx = vq->last_used_idx;
	desc_base = &vq->desc_packed[last_used_idx];

	flags = PACKED_DESC_ENQUEUE_USED_FLAG(vq->used_wrap_counter);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		desc_base[i].id = ids[i];
		desc_base[i].len = lens[i];
	}

	rte_atomic_thread_fence(__ATOMIC_RELEASE);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		desc_base[i].flags = flags;
	}

	vhost_log_cache_used_vring(dev, vq, last_used_idx *
				   sizeof(struct vring_packed_desc),
				   sizeof(struct vring_packed_desc) *
				   PACKED_BATCH_SIZE);
	vhost_log_cache_sync(dev, vq);

	vq_inc_last_used_packed(vq, PACKED_BATCH_SIZE);
}

static __rte_always_inline void
vhost_shadow_dequeue_batch_packed_inorder(struct vhost_virtqueue *vq,
					  uint16_t id)
{
	vq->shadow_used_packed[0].id = id;

	if (!vq->shadow_used_idx) {
		vq->shadow_last_used_idx = vq->last_used_idx;
		vq->shadow_used_packed[0].flags =
			PACKED_DESC_DEQUEUE_USED_FLAG(vq->used_wrap_counter);
		vq->shadow_used_packed[0].len = 0;
		vq->shadow_used_packed[0].count = 1;
		vq->shadow_used_idx++;
	}

	vq_inc_last_used_packed(vq, PACKED_BATCH_SIZE);
}

static __rte_always_inline void
vhost_shadow_dequeue_batch_packed(struct virtio_net *dev,
				  struct vhost_virtqueue *vq,
				  uint16_t *ids)
{
	uint16_t flags;
	uint16_t i;
	uint16_t begin;

	flags = PACKED_DESC_DEQUEUE_USED_FLAG(vq->used_wrap_counter);

	if (!vq->shadow_used_idx) {
		vq->shadow_last_used_idx = vq->last_used_idx;
		vq->shadow_used_packed[0].id  = ids[0];
		vq->shadow_used_packed[0].len = 0;
		vq->shadow_used_packed[0].count = 1;
		vq->shadow_used_packed[0].flags = flags;
		vq->shadow_used_idx++;
		begin = 1;
	} else
		begin = 0;

	vhost_for_each_try_unroll(i, begin, PACKED_BATCH_SIZE) {
		vq->desc_packed[vq->last_used_idx + i].id = ids[i];
		vq->desc_packed[vq->last_used_idx + i].len = 0;
	}

	rte_atomic_thread_fence(__ATOMIC_RELEASE);
	vhost_for_each_try_unroll(i, begin, PACKED_BATCH_SIZE)
		vq->desc_packed[vq->last_used_idx + i].flags = flags;

	vhost_log_cache_used_vring(dev, vq, vq->last_used_idx *
				   sizeof(struct vring_packed_desc),
				   sizeof(struct vring_packed_desc) *
				   PACKED_BATCH_SIZE);
	vhost_log_cache_sync(dev, vq);

	vq_inc_last_used_packed(vq, PACKED_BATCH_SIZE);
}

static __rte_always_inline void
vhost_shadow_dequeue_single_packed(struct vhost_virtqueue *vq,
				   uint16_t buf_id,
				   uint16_t count)
{
	uint16_t flags;

	flags = vq->desc_packed[vq->last_used_idx].flags;
	if (vq->used_wrap_counter) {
		flags |= VRING_DESC_F_USED;
		flags |= VRING_DESC_F_AVAIL;
	} else {
		flags &= ~VRING_DESC_F_USED;
		flags &= ~VRING_DESC_F_AVAIL;
	}

	if (!vq->shadow_used_idx) {
		vq->shadow_last_used_idx = vq->last_used_idx;

		vq->shadow_used_packed[0].id  = buf_id;
		vq->shadow_used_packed[0].len = 0;
		vq->shadow_used_packed[0].flags = flags;
		vq->shadow_used_idx++;
	} else {
		vq->desc_packed[vq->last_used_idx].id = buf_id;
		vq->desc_packed[vq->last_used_idx].len = 0;
		vq->desc_packed[vq->last_used_idx].flags = flags;
	}

	vq_inc_last_used_packed(vq, count);
}

static __rte_always_inline void
vhost_shadow_dequeue_single_packed_inorder(struct vhost_virtqueue *vq,
					   uint16_t buf_id,
					   uint16_t count)
{
	uint16_t flags;

	vq->shadow_used_packed[0].id = buf_id;

	flags = vq->desc_packed[vq->last_used_idx].flags;
	if (vq->used_wrap_counter) {
		flags |= VRING_DESC_F_USED;
		flags |= VRING_DESC_F_AVAIL;
	} else {
		flags &= ~VRING_DESC_F_USED;
		flags &= ~VRING_DESC_F_AVAIL;
	}

	if (!vq->shadow_used_idx) {
		vq->shadow_last_used_idx = vq->last_used_idx;
		vq->shadow_used_packed[0].len = 0;
		vq->shadow_used_packed[0].flags = flags;
		vq->shadow_used_idx++;
	}

	vq_inc_last_used_packed(vq, count);
}

static __rte_always_inline void
vhost_shadow_enqueue_packed(struct vhost_virtqueue *vq,
				   uint32_t *len,
				   uint16_t *id,
				   uint16_t *count,
				   uint16_t num_buffers)
{
	uint16_t i;

	for (i = 0; i < num_buffers; i++) {
		/* enqueue shadow flush action aligned with batch num */
		if (!vq->shadow_used_idx)
			vq->shadow_aligned_idx = vq->last_used_idx &
				PACKED_BATCH_MASK;
		vq->shadow_used_packed[vq->shadow_used_idx].id  = id[i];
		vq->shadow_used_packed[vq->shadow_used_idx].len = len[i];
		vq->shadow_used_packed[vq->shadow_used_idx].count = count[i];
		vq->shadow_aligned_idx += count[i];
		vq->shadow_used_idx++;
	}
}

static __rte_always_inline void
vhost_shadow_enqueue_single_packed(struct virtio_net *dev,
				   struct vhost_virtqueue *vq,
				   uint32_t *len,
				   uint16_t *id,
				   uint16_t *count,
				   uint16_t num_buffers)
{
	vhost_shadow_enqueue_packed(vq, len, id, count, num_buffers);

	if (vq->shadow_aligned_idx >= PACKED_BATCH_SIZE) {
		do_data_copy_enqueue(dev, vq);
		vhost_flush_enqueue_shadow_packed(dev, vq);
	}
}

/* avoid write operation when necessary, to lessen cache issues */
#define ASSIGN_UNLESS_EQUAL(var, val) do {	\
	if ((var) != (val))			\
		(var) = (val);			\
} while (0)

static __rte_always_inline void
virtio_enqueue_offload(struct rte_mbuf *m_buf, struct virtio_net_hdr *net_hdr)
{
	uint64_t csum_l4 = m_buf->ol_flags & RTE_MBUF_F_TX_L4_MASK;

	if (m_buf->ol_flags & RTE_MBUF_F_TX_TCP_SEG)
		csum_l4 |= RTE_MBUF_F_TX_TCP_CKSUM;

	if (csum_l4) {
		/*
		 * Pseudo-header checksum must be set as per Virtio spec.
		 *
		 * Note: We don't propagate rte_net_intel_cksum_prepare()
		 * errors, as it would have an impact on performance, and an
		 * error would mean the packet is dropped by the guest instead
		 * of being dropped here.
		 */
		rte_net_intel_cksum_prepare(m_buf);

		net_hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
		net_hdr->csum_start = m_buf->l2_len + m_buf->l3_len;

		switch (csum_l4) {
		case RTE_MBUF_F_TX_TCP_CKSUM:
			net_hdr->csum_offset = (offsetof(struct rte_tcp_hdr,
						cksum));
			break;
		case RTE_MBUF_F_TX_UDP_CKSUM:
			net_hdr->csum_offset = (offsetof(struct rte_udp_hdr,
						dgram_cksum));
			break;
		case RTE_MBUF_F_TX_SCTP_CKSUM:
			net_hdr->csum_offset = (offsetof(struct rte_sctp_hdr,
						cksum));
			break;
		}
	} else {
		ASSIGN_UNLESS_EQUAL(net_hdr->csum_start, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->csum_offset, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->flags, 0);
	}

	/* IP cksum verification cannot be bypassed, then calculate here */
	if (m_buf->ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
		struct rte_ipv4_hdr *ipv4_hdr;

		ipv4_hdr = rte_pktmbuf_mtod_offset(m_buf, struct rte_ipv4_hdr *,
						   m_buf->l2_len);
		ipv4_hdr->hdr_checksum = 0;
		ipv4_hdr->hdr_checksum = rte_ipv4_cksum(ipv4_hdr);
	}

	if (m_buf->ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		if (m_buf->ol_flags & RTE_MBUF_F_TX_IPV4)
			net_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
		else
			net_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV6;
		net_hdr->gso_size = m_buf->tso_segsz;
		net_hdr->hdr_len = m_buf->l2_len + m_buf->l3_len
					+ m_buf->l4_len;
	} else if (m_buf->ol_flags & RTE_MBUF_F_TX_UDP_SEG) {
		net_hdr->gso_type = VIRTIO_NET_HDR_GSO_UDP;
		net_hdr->gso_size = m_buf->tso_segsz;
		net_hdr->hdr_len = m_buf->l2_len + m_buf->l3_len +
			m_buf->l4_len;
	} else {
		ASSIGN_UNLESS_EQUAL(net_hdr->gso_type, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->gso_size, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->hdr_len, 0);
	}
}

static __rte_always_inline int
map_one_desc(struct virtio_net *dev, struct vhost_virtqueue *vq,
		struct buf_vector *buf_vec, uint16_t *vec_idx,
		uint64_t desc_iova, uint64_t desc_len, uint8_t perm)
{
	uint16_t vec_id = *vec_idx;

	while (desc_len) {
		uint64_t desc_addr;
		uint64_t desc_chunck_len = desc_len;

		if (unlikely(vec_id >= BUF_VECTOR_MAX))
			return -1;

		desc_addr = vhost_iova_to_vva(dev, vq,
				desc_iova,
				&desc_chunck_len,
				perm);
		if (unlikely(!desc_addr))
			return -1;

		rte_prefetch0((void *)(uintptr_t)desc_addr);

		buf_vec[vec_id].buf_iova = desc_iova;
		buf_vec[vec_id].buf_addr = desc_addr;
		buf_vec[vec_id].buf_len  = desc_chunck_len;

		desc_len -= desc_chunck_len;
		desc_iova += desc_chunck_len;
		vec_id++;
	}
	*vec_idx = vec_id;

	return 0;
}

static __rte_always_inline int
fill_vec_buf_split(struct virtio_net *dev, struct vhost_virtqueue *vq,
			 uint32_t avail_idx, uint16_t *vec_idx,
			 struct buf_vector *buf_vec, uint16_t *desc_chain_head,
			 uint32_t *desc_chain_len, uint8_t perm)
{
	uint16_t idx = vq->avail->ring[avail_idx & (vq->size - 1)];
	uint16_t vec_id = *vec_idx;
	uint32_t len    = 0;
	uint64_t dlen;
	uint32_t nr_descs = vq->size;
	uint32_t cnt    = 0;
	struct vring_desc *descs = vq->desc;
	struct vring_desc *idesc = NULL;

	if (unlikely(idx >= vq->size))
		return -1;

	*desc_chain_head = idx;

	if (vq->desc[idx].flags & VRING_DESC_F_INDIRECT) {
		dlen = vq->desc[idx].len;
		nr_descs = dlen / sizeof(struct vring_desc);
		if (unlikely(nr_descs > vq->size))
			return -1;

		descs = (struct vring_desc *)(uintptr_t)
			vhost_iova_to_vva(dev, vq, vq->desc[idx].addr,
						&dlen,
						VHOST_ACCESS_RO);
		if (unlikely(!descs))
			return -1;

		if (unlikely(dlen < vq->desc[idx].len)) {
			/*
			 * The indirect desc table is not contiguous
			 * in process VA space, we have to copy it.
			 */
			idesc = vhost_alloc_copy_ind_table(dev, vq,
					vq->desc[idx].addr, vq->desc[idx].len);
			if (unlikely(!idesc))
				return -1;

			descs = idesc;
		}

		idx = 0;
	}

	while (1) {
		if (unlikely(idx >= nr_descs || cnt++ >= nr_descs)) {
			free_ind_table(idesc);
			return -1;
		}

		dlen = descs[idx].len;
		len += dlen;

		if (unlikely(map_one_desc(dev, vq, buf_vec, &vec_id,
						descs[idx].addr, dlen,
						perm))) {
			free_ind_table(idesc);
			return -1;
		}

		if ((descs[idx].flags & VRING_DESC_F_NEXT) == 0)
			break;

		idx = descs[idx].next;
	}

	*desc_chain_len = len;
	*vec_idx = vec_id;

	if (unlikely(!!idesc))
		free_ind_table(idesc);

	return 0;
}

/*
 * Returns -1 on fail, 0 on success
 */
static inline int
reserve_avail_buf_split(struct virtio_net *dev, struct vhost_virtqueue *vq,
				uint32_t size, struct buf_vector *buf_vec,
				uint16_t *num_buffers, uint16_t avail_head,
				uint16_t *nr_vec)
{
	uint16_t cur_idx;
	uint16_t vec_idx = 0;
	uint16_t max_tries, tries = 0;

	uint16_t head_idx = 0;
	uint32_t len = 0;

	*num_buffers = 0;
	cur_idx  = vq->last_avail_idx;

	if (rxvq_is_mergeable(dev))
		max_tries = vq->size - 1;
	else
		max_tries = 1;

	while (size > 0) {
		if (unlikely(cur_idx == avail_head))
			return -1;
		/*
		 * if we tried all available ring items, and still
		 * can't get enough buf, it means something abnormal
		 * happened.
		 */
		if (unlikely(++tries > max_tries))
			return -1;

		if (unlikely(fill_vec_buf_split(dev, vq, cur_idx,
						&vec_idx, buf_vec,
						&head_idx, &len,
						VHOST_ACCESS_RW) < 0))
			return -1;
		len = RTE_MIN(len, size);
		update_shadow_used_ring_split(vq, head_idx, len);
		size -= len;

		cur_idx++;
		*num_buffers += 1;
	}

	*nr_vec = vec_idx;

	return 0;
}

static __rte_always_inline int
fill_vec_buf_packed_indirect(struct virtio_net *dev,
			struct vhost_virtqueue *vq,
			struct vring_packed_desc *desc, uint16_t *vec_idx,
			struct buf_vector *buf_vec, uint32_t *len, uint8_t perm)
{
	uint16_t i;
	uint32_t nr_descs;
	uint16_t vec_id = *vec_idx;
	uint64_t dlen;
	struct vring_packed_desc *descs, *idescs = NULL;

	dlen = desc->len;
	descs = (struct vring_packed_desc *)(uintptr_t)
		vhost_iova_to_vva(dev, vq, desc->addr, &dlen, VHOST_ACCESS_RO);
	if (unlikely(!descs))
		return -1;

	if (unlikely(dlen < desc->len)) {
		/*
		 * The indirect desc table is not contiguous
		 * in process VA space, we have to copy it.
		 */
		idescs = vhost_alloc_copy_ind_table(dev,
				vq, desc->addr, desc->len);
		if (unlikely(!idescs))
			return -1;

		descs = idescs;
	}

	nr_descs =  desc->len / sizeof(struct vring_packed_desc);
	if (unlikely(nr_descs >= vq->size)) {
		free_ind_table(idescs);
		return -1;
	}

	for (i = 0; i < nr_descs; i++) {
		if (unlikely(vec_id >= BUF_VECTOR_MAX)) {
			free_ind_table(idescs);
			return -1;
		}

		dlen = descs[i].len;
		*len += dlen;
		if (unlikely(map_one_desc(dev, vq, buf_vec, &vec_id,
						descs[i].addr, dlen,
						perm)))
			return -1;
	}
	*vec_idx = vec_id;

	if (unlikely(!!idescs))
		free_ind_table(idescs);

	return 0;
}

static __rte_always_inline int
fill_vec_buf_packed(struct virtio_net *dev, struct vhost_virtqueue *vq,
				uint16_t avail_idx, uint16_t *desc_count,
				struct buf_vector *buf_vec, uint16_t *vec_idx,
				uint16_t *buf_id, uint32_t *len, uint8_t perm)
{
	bool wrap_counter = vq->avail_wrap_counter;
	struct vring_packed_desc *descs = vq->desc_packed;
	uint16_t vec_id = *vec_idx;
	uint64_t dlen;

	if (avail_idx < vq->last_avail_idx)
		wrap_counter ^= 1;

	/*
	 * Perform a load-acquire barrier in desc_is_avail to
	 * enforce the ordering between desc flags and desc
	 * content.
	 */
	if (unlikely(!desc_is_avail(&descs[avail_idx], wrap_counter)))
		return -1;

	*desc_count = 0;
	*len = 0;

	while (1) {
		if (unlikely(vec_id >= BUF_VECTOR_MAX))
			return -1;

		if (unlikely(*desc_count >= vq->size))
			return -1;

		*desc_count += 1;
		*buf_id = descs[avail_idx].id;

		if (descs[avail_idx].flags & VRING_DESC_F_INDIRECT) {
			if (unlikely(fill_vec_buf_packed_indirect(dev, vq,
							&descs[avail_idx],
							&vec_id, buf_vec,
							len, perm) < 0))
				return -1;
		} else {
			dlen = descs[avail_idx].len;
			*len += dlen;

			if (unlikely(map_one_desc(dev, vq, buf_vec, &vec_id,
							descs[avail_idx].addr,
							dlen,
							perm)))
				return -1;
		}

		if ((descs[avail_idx].flags & VRING_DESC_F_NEXT) == 0)
			break;

		if (++avail_idx >= vq->size) {
			avail_idx -= vq->size;
			wrap_counter ^= 1;
		}
	}

	*vec_idx = vec_id;

	return 0;
}

static __rte_noinline void
copy_vnet_hdr_to_desc(struct virtio_net *dev, struct vhost_virtqueue *vq,
		struct buf_vector *buf_vec,
		struct virtio_net_hdr_mrg_rxbuf *hdr)
{
	uint64_t len;
	uint64_t remain = dev->vhost_hlen;
	uint64_t src = (uint64_t)(uintptr_t)hdr, dst;
	uint64_t iova = buf_vec->buf_iova;

	while (remain) {
		len = RTE_MIN(remain,
				buf_vec->buf_len);
		dst = buf_vec->buf_addr;
		rte_memcpy((void *)(uintptr_t)dst,
				(void *)(uintptr_t)src,
				len);

		PRINT_PACKET(dev, (uintptr_t)dst,
				(uint32_t)len, 0);
		vhost_log_cache_write_iova(dev, vq,
				iova, len);

		remain -= len;
		iova += len;
		src += len;
		buf_vec++;
	}
}

static __rte_always_inline int
async_iter_initialize(struct vhost_async *async)
{
	struct rte_vhost_iov_iter *iter;

	if (unlikely(async->iovec_idx >= VHOST_MAX_ASYNC_VEC)) {
		VHOST_LOG_DATA(ERR, "no more async iovec available\n");
		return -1;
	}

	iter = async->iov_iter + async->iter_idx;
	iter->iov = async->iovec + async->iovec_idx;
	iter->nr_segs = 0;

	return 0;
}

static __rte_always_inline int
async_iter_add_iovec(struct vhost_async *async, void *src, void *dst, size_t len)
{
	struct rte_vhost_iov_iter *iter;
	struct rte_vhost_iovec *iovec;

	if (unlikely(async->iovec_idx >= VHOST_MAX_ASYNC_VEC)) {
		static bool vhost_max_async_vec_log;

		if (!vhost_max_async_vec_log) {
			VHOST_LOG_DATA(ERR, "no more async iovec available\n");
			vhost_max_async_vec_log = true;
		}

		return -1;
	}

	iter = async->iov_iter + async->iter_idx;
	iovec = async->iovec + async->iovec_idx;

	iovec->src_addr = src;
	iovec->dst_addr = dst;
	iovec->len = len;

	iter->nr_segs++;
	async->iovec_idx++;

	return 0;
}

static __rte_always_inline void
async_iter_finalize(struct vhost_async *async)
{
	async->iter_idx++;
}

static __rte_always_inline void
async_iter_cancel(struct vhost_async *async)
{
	struct rte_vhost_iov_iter *iter;

	iter = async->iov_iter + async->iter_idx;
	async->iovec_idx -= iter->nr_segs;
	iter->nr_segs = 0;
	iter->iov = NULL;
}

static __rte_always_inline void
async_iter_reset(struct vhost_async *async)
{
	async->iter_idx = 0;
	async->iovec_idx = 0;
}

static __rte_always_inline int
async_mbuf_to_desc_seg(struct virtio_net *dev, struct vhost_virtqueue *vq,
		struct rte_mbuf *m, uint32_t mbuf_offset,
		uint64_t buf_iova, uint32_t cpy_len)
{
	struct vhost_async *async = vq->async;
	uint64_t mapped_len;
	uint32_t buf_offset = 0;
	void *host_iova;

	while (cpy_len) {
		host_iova = (void *)(uintptr_t)gpa_to_first_hpa(dev,
				buf_iova + buf_offset, cpy_len, &mapped_len);
		if (unlikely(!host_iova)) {
			VHOST_LOG_DATA(ERR, "(%d) %s: failed to get host_iova.\n",
				       dev->vid, __func__);
			return -1;
		}

		if (unlikely(async_iter_add_iovec(async,
						(void *)(uintptr_t)rte_pktmbuf_iova_offset(m,
							mbuf_offset),
						host_iova, (size_t)mapped_len)))
			return -1;

		cpy_len -= (uint32_t)mapped_len;
		mbuf_offset += (uint32_t)mapped_len;
		buf_offset += (uint32_t)mapped_len;
	}

	return 0;
}

static __rte_always_inline void
sync_mbuf_to_desc_seg(struct virtio_net *dev, struct vhost_virtqueue *vq,
		struct rte_mbuf *m, uint32_t mbuf_offset,
		uint64_t buf_addr, uint64_t buf_iova, uint32_t cpy_len)
{
	struct batch_copy_elem *batch_copy = vq->batch_copy_elems;

	if (likely(cpy_len > MAX_BATCH_LEN || vq->batch_copy_nb_elems >= vq->size)) {
		rte_memcpy((void *)((uintptr_t)(buf_addr)),
				rte_pktmbuf_mtod_offset(m, void *, mbuf_offset),
				cpy_len);
		vhost_log_cache_write_iova(dev, vq, buf_iova, cpy_len);
		PRINT_PACKET(dev, (uintptr_t)(buf_addr), cpy_len, 0);
	} else {
		batch_copy[vq->batch_copy_nb_elems].dst =
			(void *)((uintptr_t)(buf_addr));
		batch_copy[vq->batch_copy_nb_elems].src =
			rte_pktmbuf_mtod_offset(m, void *, mbuf_offset);
		batch_copy[vq->batch_copy_nb_elems].log_addr = buf_iova;
		batch_copy[vq->batch_copy_nb_elems].len = cpy_len;
		vq->batch_copy_nb_elems++;
	}
}

static __rte_always_inline int
mbuf_to_desc(struct virtio_net *dev, struct vhost_virtqueue *vq,
		struct rte_mbuf *m, struct buf_vector *buf_vec,
		uint16_t nr_vec, uint16_t num_buffers, bool is_async)
{
	uint32_t vec_idx = 0;
	uint32_t mbuf_offset, mbuf_avail;
	uint32_t buf_offset, buf_avail;
	uint64_t buf_addr, buf_iova, buf_len;
	uint32_t cpy_len;
	uint64_t hdr_addr;
	struct rte_mbuf *hdr_mbuf;
	struct virtio_net_hdr_mrg_rxbuf tmp_hdr, *hdr = NULL;
	struct vhost_async *async = vq->async;

	if (unlikely(m == NULL))
		return -1;

	buf_addr = buf_vec[vec_idx].buf_addr;
	buf_iova = buf_vec[vec_idx].buf_iova;
	buf_len = buf_vec[vec_idx].buf_len;

	if (unlikely(buf_len < dev->vhost_hlen && nr_vec <= 1))
		return -1;

	hdr_mbuf = m;
	hdr_addr = buf_addr;
	if (unlikely(buf_len < dev->vhost_hlen)) {
		memset(&tmp_hdr, 0, sizeof(struct virtio_net_hdr_mrg_rxbuf));
		hdr = &tmp_hdr;
	} else
		hdr = (struct virtio_net_hdr_mrg_rxbuf *)(uintptr_t)hdr_addr;

	VHOST_LOG_DATA(DEBUG, "(%d) RX: num merge buffers %d\n",
		dev->vid, num_buffers);

	if (unlikely(buf_len < dev->vhost_hlen)) {
		buf_offset = dev->vhost_hlen - buf_len;
		vec_idx++;
		buf_addr = buf_vec[vec_idx].buf_addr;
		buf_iova = buf_vec[vec_idx].buf_iova;
		buf_len = buf_vec[vec_idx].buf_len;
		buf_avail = buf_len - buf_offset;
	} else {
		buf_offset = dev->vhost_hlen;
		buf_avail = buf_len - dev->vhost_hlen;
	}

	mbuf_avail  = rte_pktmbuf_data_len(m);
	mbuf_offset = 0;

	if (is_async) {
		if (async_iter_initialize(async))
			return -1;
	}

	while (mbuf_avail != 0 || m->next != NULL) {
		/* done with current buf, get the next one */
		if (buf_avail == 0) {
			vec_idx++;
			if (unlikely(vec_idx >= nr_vec))
				goto error;

			buf_addr = buf_vec[vec_idx].buf_addr;
			buf_iova = buf_vec[vec_idx].buf_iova;
			buf_len = buf_vec[vec_idx].buf_len;

			buf_offset = 0;
			buf_avail  = buf_len;
		}

		/* done with current mbuf, get the next one */
		if (mbuf_avail == 0) {
			m = m->next;

			mbuf_offset = 0;
			mbuf_avail  = rte_pktmbuf_data_len(m);
		}

		if (hdr_addr) {
			virtio_enqueue_offload(hdr_mbuf, &hdr->hdr);
			if (rxvq_is_mergeable(dev))
				ASSIGN_UNLESS_EQUAL(hdr->num_buffers,
						num_buffers);

			if (unlikely(hdr == &tmp_hdr)) {
				copy_vnet_hdr_to_desc(dev, vq, buf_vec, hdr);
			} else {
				PRINT_PACKET(dev, (uintptr_t)hdr_addr,
						dev->vhost_hlen, 0);
				vhost_log_cache_write_iova(dev, vq,
						buf_vec[0].buf_iova,
						dev->vhost_hlen);
			}

			hdr_addr = 0;
		}

		cpy_len = RTE_MIN(buf_avail, mbuf_avail);

		if (is_async) {
			if (async_mbuf_to_desc_seg(dev, vq, m, mbuf_offset,
						buf_iova + buf_offset, cpy_len) < 0)
				goto error;
		} else {
			sync_mbuf_to_desc_seg(dev, vq, m, mbuf_offset,
					buf_addr + buf_offset,
					buf_iova + buf_offset, cpy_len);
		}

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		buf_avail  -= cpy_len;
		buf_offset += cpy_len;
	}

	if (is_async)
		async_iter_finalize(async);

	return 0;
error:
	if (is_async)
		async_iter_cancel(async);

	return -1;
}

static __rte_always_inline int
vhost_enqueue_single_packed(struct virtio_net *dev,
			    struct vhost_virtqueue *vq,
			    struct rte_mbuf *pkt,
			    struct buf_vector *buf_vec,
			    uint16_t *nr_descs)
{
	uint16_t nr_vec = 0;
	uint16_t avail_idx = vq->last_avail_idx;
	uint16_t max_tries, tries = 0;
	uint16_t buf_id = 0;
	uint32_t len = 0;
	uint16_t desc_count;
	uint32_t size = pkt->pkt_len + sizeof(struct virtio_net_hdr_mrg_rxbuf);
	uint16_t num_buffers = 0;
	uint32_t buffer_len[vq->size];
	uint16_t buffer_buf_id[vq->size];
	uint16_t buffer_desc_count[vq->size];

	if (rxvq_is_mergeable(dev))
		max_tries = vq->size - 1;
	else
		max_tries = 1;

	while (size > 0) {
		/*
		 * if we tried all available ring items, and still
		 * can't get enough buf, it means something abnormal
		 * happened.
		 */
		if (unlikely(++tries > max_tries))
			return -1;

		if (unlikely(fill_vec_buf_packed(dev, vq,
						avail_idx, &desc_count,
						buf_vec, &nr_vec,
						&buf_id, &len,
						VHOST_ACCESS_RW) < 0))
			return -1;

		len = RTE_MIN(len, size);
		size -= len;

		buffer_len[num_buffers] = len;
		buffer_buf_id[num_buffers] = buf_id;
		buffer_desc_count[num_buffers] = desc_count;
		num_buffers += 1;

		*nr_descs += desc_count;
		avail_idx += desc_count;
		if (avail_idx >= vq->size)
			avail_idx -= vq->size;
	}

	if (mbuf_to_desc(dev, vq, pkt, buf_vec, nr_vec, num_buffers, false) < 0)
		return -1;

	vhost_shadow_enqueue_single_packed(dev, vq, buffer_len, buffer_buf_id,
					   buffer_desc_count, num_buffers);

	return 0;
}

static __rte_noinline uint32_t
virtio_dev_rx_split(struct virtio_net *dev, struct vhost_virtqueue *vq,
	struct rte_mbuf **pkts, uint32_t count)
{
	uint32_t pkt_idx = 0;
	uint16_t num_buffers;
	struct buf_vector buf_vec[BUF_VECTOR_MAX];
	uint16_t avail_head;

	/*
	 * The ordering between avail index and
	 * desc reads needs to be enforced.
	 */
	avail_head = __atomic_load_n(&vq->avail->idx, __ATOMIC_ACQUIRE);

	rte_prefetch0(&vq->avail->ring[vq->last_avail_idx & (vq->size - 1)]);

	for (pkt_idx = 0; pkt_idx < count; pkt_idx++) {
		uint32_t pkt_len = pkts[pkt_idx]->pkt_len + dev->vhost_hlen;
		uint16_t nr_vec = 0;

		if (unlikely(reserve_avail_buf_split(dev, vq,
						pkt_len, buf_vec, &num_buffers,
						avail_head, &nr_vec) < 0)) {
			VHOST_LOG_DATA(DEBUG,
				"(%d) failed to get enough desc from vring\n",
				dev->vid);
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		VHOST_LOG_DATA(DEBUG, "(%d) current index %d | end index %d\n",
			dev->vid, vq->last_avail_idx,
			vq->last_avail_idx + num_buffers);

		if (mbuf_to_desc(dev, vq, pkts[pkt_idx], buf_vec, nr_vec,
					num_buffers, false) < 0) {
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		vq->last_avail_idx += num_buffers;
	}

	do_data_copy_enqueue(dev, vq);

	if (likely(vq->shadow_used_idx)) {
		flush_shadow_used_ring_split(dev, vq);
		vhost_vring_call_split(dev, vq);
	}

	return pkt_idx;
}

static __rte_always_inline int
virtio_dev_rx_sync_batch_check(struct virtio_net *dev,
			   struct vhost_virtqueue *vq,
			   struct rte_mbuf **pkts,
			   uint64_t *desc_addrs,
			   uint64_t *lens)
{
	bool wrap_counter = vq->avail_wrap_counter;
	struct vring_packed_desc *descs = vq->desc_packed;
	uint16_t avail_idx = vq->last_avail_idx;
	uint32_t buf_offset = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	uint16_t i;

	if (unlikely(avail_idx & PACKED_BATCH_MASK))
		return -1;

	if (unlikely((avail_idx + PACKED_BATCH_SIZE) > vq->size))
		return -1;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (unlikely(pkts[i]->next != NULL))
			return -1;
		if (unlikely(!desc_is_avail(&descs[avail_idx + i],
					    wrap_counter)))
			return -1;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		lens[i] = descs[avail_idx + i].len;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (unlikely(pkts[i]->pkt_len > (lens[i] - buf_offset)))
			return -1;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		desc_addrs[i] = vhost_iova_to_vva(dev, vq,
						  descs[avail_idx + i].addr,
						  &lens[i],
						  VHOST_ACCESS_RW);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (unlikely(!desc_addrs[i]))
			return -1;
		if (unlikely(lens[i] != descs[avail_idx + i].len))
			return -1;
	}

	return 0;
}

static __rte_always_inline void
virtio_dev_rx_batch_packed_copy(struct virtio_net *dev,
			   struct vhost_virtqueue *vq,
			   struct rte_mbuf **pkts,
			   uint64_t *desc_addrs,
			   uint64_t *lens)
{
	uint32_t buf_offset = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	struct virtio_net_hdr_mrg_rxbuf *hdrs[PACKED_BATCH_SIZE];
	struct vring_packed_desc *descs = vq->desc_packed;
	uint16_t avail_idx = vq->last_avail_idx;
	uint16_t ids[PACKED_BATCH_SIZE];
	uint16_t i;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		rte_prefetch0((void *)(uintptr_t)desc_addrs[i]);
		hdrs[i] = (struct virtio_net_hdr_mrg_rxbuf *)
					(uintptr_t)desc_addrs[i];
		lens[i] = pkts[i]->pkt_len +
			sizeof(struct virtio_net_hdr_mrg_rxbuf);
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		virtio_enqueue_offload(pkts[i], &hdrs[i]->hdr);

	vq_inc_last_avail_packed(vq, PACKED_BATCH_SIZE);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		rte_memcpy((void *)(uintptr_t)(desc_addrs[i] + buf_offset),
			   rte_pktmbuf_mtod_offset(pkts[i], void *, 0),
			   pkts[i]->pkt_len);
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		vhost_log_cache_write_iova(dev, vq, descs[avail_idx + i].addr,
					   lens[i]);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		ids[i] = descs[avail_idx + i].id;

	vhost_flush_enqueue_batch_packed(dev, vq, lens, ids);
}

static __rte_always_inline int
virtio_dev_rx_sync_batch_packed(struct virtio_net *dev,
			   struct vhost_virtqueue *vq,
			   struct rte_mbuf **pkts)
{
	uint64_t desc_addrs[PACKED_BATCH_SIZE];
	uint64_t lens[PACKED_BATCH_SIZE];

	if (virtio_dev_rx_sync_batch_check(dev, vq, pkts, desc_addrs, lens) == -1)
		return -1;

	if (vq->shadow_used_idx) {
		do_data_copy_enqueue(dev, vq);
		vhost_flush_enqueue_shadow_packed(dev, vq);
	}

	virtio_dev_rx_batch_packed_copy(dev, vq, pkts, desc_addrs, lens);

	return 0;
}

static __rte_always_inline int16_t
virtio_dev_rx_single_packed(struct virtio_net *dev,
			    struct vhost_virtqueue *vq,
			    struct rte_mbuf *pkt)
{
	struct buf_vector buf_vec[BUF_VECTOR_MAX];
	uint16_t nr_descs = 0;

	if (unlikely(vhost_enqueue_single_packed(dev, vq, pkt, buf_vec,
						 &nr_descs) < 0)) {
		VHOST_LOG_DATA(DEBUG,
				"(%d) failed to get enough desc from vring\n",
				dev->vid);
		return -1;
	}

	VHOST_LOG_DATA(DEBUG, "(%d) current index %d | end index %d\n",
			dev->vid, vq->last_avail_idx,
			vq->last_avail_idx + nr_descs);

	vq_inc_last_avail_packed(vq, nr_descs);

	return 0;
}

static __rte_noinline uint32_t
virtio_dev_rx_packed(struct virtio_net *dev,
		     struct vhost_virtqueue *__rte_restrict vq,
		     struct rte_mbuf **__rte_restrict pkts,
		     uint32_t count)
{
	uint32_t pkt_idx = 0;

	do {
		rte_prefetch0(&vq->desc_packed[vq->last_avail_idx]);

		if (count - pkt_idx >= PACKED_BATCH_SIZE) {
			if (!virtio_dev_rx_sync_batch_packed(dev, vq,
							&pkts[pkt_idx])) {
				pkt_idx += PACKED_BATCH_SIZE;
				continue;
			}
		}

		if (virtio_dev_rx_single_packed(dev, vq, pkts[pkt_idx]))
			break;
		pkt_idx++;

	} while (pkt_idx < count);

	if (vq->shadow_used_idx) {
		do_data_copy_enqueue(dev, vq);
		vhost_flush_enqueue_shadow_packed(dev, vq);
	}

	if (pkt_idx)
		vhost_vring_call_packed(dev, vq);

	return pkt_idx;
}

static __rte_always_inline uint32_t
virtio_dev_rx(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_virtqueue *vq;
	uint32_t nb_tx = 0;

	VHOST_LOG_DATA(DEBUG, "(%d) %s\n", dev->vid, __func__);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->nr_vring))) {
		VHOST_LOG_DATA(ERR, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];

	rte_spinlock_lock(&vq->access_lock);

	if (unlikely(!vq->enabled))
		goto out_access_unlock;

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_lock(vq);

	if (unlikely(!vq->access_ok))
		if (unlikely(vring_translate(dev, vq) < 0))
			goto out;

	count = RTE_MIN((uint32_t)MAX_PKT_BURST, count);
	if (count == 0)
		goto out;

	if (vq_is_packed(dev))
		nb_tx = virtio_dev_rx_packed(dev, vq, pkts, count);
	else
		nb_tx = virtio_dev_rx_split(dev, vq, pkts, count);

out:
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_unlock(vq);

out_access_unlock:
	rte_spinlock_unlock(&vq->access_lock);

	return nb_tx;
}

uint16_t
rte_vhost_enqueue_burst(int vid, uint16_t queue_id,
	struct rte_mbuf **__rte_restrict pkts, uint16_t count)
{
	struct virtio_net *dev = get_device(vid);

	if (!dev)
		return 0;

	if (unlikely(!(dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET))) {
		VHOST_LOG_DATA(ERR,
			"(%d) %s: built-in vhost net backend is disabled.\n",
			dev->vid, __func__);
		return 0;
	}

	return virtio_dev_rx(dev, queue_id, pkts, count);
}

static __rte_always_inline uint16_t
async_get_first_inflight_pkt_idx(struct vhost_virtqueue *vq)
{
	struct vhost_async *async = vq->async;

	if (async->pkts_idx >= async->pkts_inflight_n)
		return async->pkts_idx - async->pkts_inflight_n;
	else
		return vq->size - async->pkts_inflight_n + async->pkts_idx;
}

static __rte_always_inline void
store_dma_desc_info_split(struct vring_used_elem *s_ring, struct vring_used_elem *d_ring,
		uint16_t ring_size, uint16_t s_idx, uint16_t d_idx, uint16_t count)
{
	size_t elem_size = sizeof(struct vring_used_elem);

	if (d_idx + count <= ring_size) {
		rte_memcpy(d_ring + d_idx, s_ring + s_idx, count * elem_size);
	} else {
		uint16_t size = ring_size - d_idx;

		rte_memcpy(d_ring + d_idx, s_ring + s_idx, size * elem_size);
		rte_memcpy(d_ring, s_ring + s_idx + size, (count - size) * elem_size);
	}
}

static __rte_always_inline void
store_dma_desc_info_packed(struct vring_used_elem_packed *s_ring,
		struct vring_used_elem_packed *d_ring,
		uint16_t ring_size, uint16_t s_idx, uint16_t d_idx, uint16_t count)
{
	size_t elem_size = sizeof(struct vring_used_elem_packed);

	if (d_idx + count <= ring_size) {
		rte_memcpy(d_ring + d_idx, s_ring + s_idx, count * elem_size);
	} else {
		uint16_t size = ring_size - d_idx;

		rte_memcpy(d_ring + d_idx, s_ring + s_idx, size * elem_size);
		rte_memcpy(d_ring, s_ring + s_idx + size, (count - size) * elem_size);
	}
}

static __rte_noinline uint32_t
virtio_dev_rx_async_submit_split(struct virtio_net *dev,
	struct vhost_virtqueue *vq, uint16_t queue_id,
	struct rte_mbuf **pkts, uint32_t count)
{
	struct buf_vector buf_vec[BUF_VECTOR_MAX];
	uint32_t pkt_idx = 0;
	uint16_t num_buffers;
	uint16_t avail_head;

	struct vhost_async *async = vq->async;
	struct async_inflight_info *pkts_info = async->pkts_info;
	uint32_t pkt_err = 0;
	int32_t n_xfer;
	uint16_t slot_idx = 0;

	/*
	 * The ordering between avail index and desc reads need to be enforced.
	 */
	avail_head = __atomic_load_n(&vq->avail->idx, __ATOMIC_ACQUIRE);

	rte_prefetch0(&vq->avail->ring[vq->last_avail_idx & (vq->size - 1)]);

	async_iter_reset(async);

	for (pkt_idx = 0; pkt_idx < count; pkt_idx++) {
		uint32_t pkt_len = pkts[pkt_idx]->pkt_len + dev->vhost_hlen;
		uint16_t nr_vec = 0;

		if (unlikely(reserve_avail_buf_split(dev, vq, pkt_len, buf_vec,
						&num_buffers, avail_head, &nr_vec) < 0)) {
			VHOST_LOG_DATA(DEBUG, "(%d) failed to get enough desc from vring\n",
					dev->vid);
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		VHOST_LOG_DATA(DEBUG, "(%d) current index %d | end index %d\n",
			dev->vid, vq->last_avail_idx, vq->last_avail_idx + num_buffers);

		if (mbuf_to_desc(dev, vq, pkts[pkt_idx], buf_vec, nr_vec, num_buffers, true) < 0) {
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		slot_idx = (async->pkts_idx + pkt_idx) & (vq->size - 1);
		pkts_info[slot_idx].descs = num_buffers;
		pkts_info[slot_idx].mbuf = pkts[pkt_idx];

		vq->last_avail_idx += num_buffers;
	}

	if (unlikely(pkt_idx == 0))
		return 0;

	n_xfer = async->ops.transfer_data(dev->vid, queue_id, async->iov_iter, 0, pkt_idx);
	if (unlikely(n_xfer < 0)) {
		VHOST_LOG_DATA(ERR, "(%d) %s: failed to transfer data for queue id %d.\n",
				dev->vid, __func__, queue_id);
		n_xfer = 0;
	}

	pkt_err = pkt_idx - n_xfer;
	if (unlikely(pkt_err)) {
		uint16_t num_descs = 0;

		/* update number of completed packets */
		pkt_idx = n_xfer;

		/* calculate the sum of descriptors to revert */
		while (pkt_err-- > 0) {
			num_descs += pkts_info[slot_idx & (vq->size - 1)].descs;
			slot_idx--;
		}

		/* recover shadow used ring and available ring */
		vq->shadow_used_idx -= num_descs;
		vq->last_avail_idx -= num_descs;
	}

	/* keep used descriptors */
	if (likely(vq->shadow_used_idx)) {
		uint16_t to = async->desc_idx_split & (vq->size - 1);

		store_dma_desc_info_split(vq->shadow_used_split,
				async->descs_split, vq->size, 0, to,
				vq->shadow_used_idx);

		async->desc_idx_split += vq->shadow_used_idx;

		async->pkts_idx += pkt_idx;
		if (async->pkts_idx >= vq->size)
			async->pkts_idx -= vq->size;

		async->pkts_inflight_n += pkt_idx;
		vq->shadow_used_idx = 0;
	}

	return pkt_idx;
}


static __rte_always_inline int
vhost_enqueue_async_packed(struct virtio_net *dev,
			    struct vhost_virtqueue *vq,
			    struct rte_mbuf *pkt,
			    struct buf_vector *buf_vec,
			    uint16_t *nr_descs,
			    uint16_t *nr_buffers)
{
	uint16_t nr_vec = 0;
	uint16_t avail_idx = vq->last_avail_idx;
	uint16_t max_tries, tries = 0;
	uint16_t buf_id = 0;
	uint32_t len = 0;
	uint16_t desc_count = 0;
	uint32_t size = pkt->pkt_len + sizeof(struct virtio_net_hdr_mrg_rxbuf);
	uint32_t buffer_len[vq->size];
	uint16_t buffer_buf_id[vq->size];
	uint16_t buffer_desc_count[vq->size];

	if (rxvq_is_mergeable(dev))
		max_tries = vq->size - 1;
	else
		max_tries = 1;

	while (size > 0) {
		/*
		 * if we tried all available ring items, and still
		 * can't get enough buf, it means something abnormal
		 * happened.
		 */
		if (unlikely(++tries > max_tries))
			return -1;

		if (unlikely(fill_vec_buf_packed(dev, vq,
						avail_idx, &desc_count,
						buf_vec, &nr_vec,
						&buf_id, &len,
						VHOST_ACCESS_RW) < 0))
			return -1;

		len = RTE_MIN(len, size);
		size -= len;

		buffer_len[*nr_buffers] = len;
		buffer_buf_id[*nr_buffers] = buf_id;
		buffer_desc_count[*nr_buffers] = desc_count;
		*nr_buffers += 1;
		*nr_descs += desc_count;
		avail_idx += desc_count;
		if (avail_idx >= vq->size)
			avail_idx -= vq->size;
	}

	if (unlikely(mbuf_to_desc(dev, vq, pkt, buf_vec, nr_vec, *nr_buffers, true) < 0))
		return -1;

	vhost_shadow_enqueue_packed(vq, buffer_len, buffer_buf_id, buffer_desc_count, *nr_buffers);

	return 0;
}

static __rte_always_inline int16_t
virtio_dev_rx_async_packed(struct virtio_net *dev, struct vhost_virtqueue *vq,
			    struct rte_mbuf *pkt, uint16_t *nr_descs, uint16_t *nr_buffers)
{
	struct buf_vector buf_vec[BUF_VECTOR_MAX];

	if (unlikely(vhost_enqueue_async_packed(dev, vq, pkt, buf_vec,
					nr_descs, nr_buffers) < 0)) {
		VHOST_LOG_DATA(DEBUG, "(%d) failed to get enough desc from vring\n", dev->vid);
		return -1;
	}

	VHOST_LOG_DATA(DEBUG, "(%d) current index %d | end index %d\n",
			dev->vid, vq->last_avail_idx, vq->last_avail_idx + *nr_descs);

	return 0;
}

static __rte_always_inline void
dma_error_handler_packed(struct vhost_virtqueue *vq, uint16_t slot_idx,
			uint32_t nr_err, uint32_t *pkt_idx)
{
	uint16_t descs_err = 0;
	uint16_t buffers_err = 0;
	struct async_inflight_info *pkts_info = vq->async->pkts_info;

	*pkt_idx -= nr_err;
	/* calculate the sum of buffers and descs of DMA-error packets. */
	while (nr_err-- > 0) {
		descs_err += pkts_info[slot_idx % vq->size].descs;
		buffers_err += pkts_info[slot_idx % vq->size].nr_buffers;
		slot_idx--;
	}

	if (vq->last_avail_idx >= descs_err) {
		vq->last_avail_idx -= descs_err;
	} else {
		vq->last_avail_idx = vq->last_avail_idx + vq->size - descs_err;
		vq->avail_wrap_counter ^= 1;
	}

	vq->shadow_used_idx -= buffers_err;
}

static __rte_noinline uint32_t
virtio_dev_rx_async_submit_packed(struct virtio_net *dev,
	struct vhost_virtqueue *vq, uint16_t queue_id,
	struct rte_mbuf **pkts, uint32_t count)
{
	uint32_t pkt_idx = 0;
	uint32_t remained = count;
	int32_t n_xfer;
	uint16_t num_buffers;
	uint16_t num_descs;

	struct vhost_async *async = vq->async;
	struct async_inflight_info *pkts_info = async->pkts_info;
	uint32_t pkt_err = 0;
	uint16_t slot_idx = 0;

	do {
		rte_prefetch0(&vq->desc_packed[vq->last_avail_idx]);

		num_buffers = 0;
		num_descs = 0;
		if (unlikely(virtio_dev_rx_async_packed(dev, vq, pkts[pkt_idx],
						&num_descs, &num_buffers) < 0))
			break;

		slot_idx = (async->pkts_idx + pkt_idx) % vq->size;

		pkts_info[slot_idx].descs = num_descs;
		pkts_info[slot_idx].nr_buffers = num_buffers;
		pkts_info[slot_idx].mbuf = pkts[pkt_idx];

		pkt_idx++;
		remained--;
		vq_inc_last_avail_packed(vq, num_descs);
	} while (pkt_idx < count);

	if (unlikely(pkt_idx == 0))
		return 0;

	n_xfer = async->ops.transfer_data(dev->vid, queue_id, async->iov_iter, 0, pkt_idx);
	if (unlikely(n_xfer < 0)) {
		VHOST_LOG_DATA(ERR, "(%d) %s: failed to transfer data for queue id %d.\n",
				dev->vid, __func__, queue_id);
		n_xfer = 0;
	}

	pkt_err = pkt_idx - n_xfer;

	async_iter_reset(async);

	if (unlikely(pkt_err))
		dma_error_handler_packed(vq, slot_idx, pkt_err, &pkt_idx);

	if (likely(vq->shadow_used_idx)) {
		/* keep used descriptors. */
		store_dma_desc_info_packed(vq->shadow_used_packed, async->buffers_packed,
					vq->size, 0, async->buffer_idx_packed,
					vq->shadow_used_idx);

		async->buffer_idx_packed += vq->shadow_used_idx;
		if (async->buffer_idx_packed >= vq->size)
			async->buffer_idx_packed -= vq->size;

		async->pkts_idx += pkt_idx;
		if (async->pkts_idx >= vq->size)
			async->pkts_idx -= vq->size;

		vq->shadow_used_idx = 0;
		async->pkts_inflight_n += pkt_idx;
	}

	return pkt_idx;
}

static __rte_always_inline void
write_back_completed_descs_split(struct vhost_virtqueue *vq, uint16_t n_descs)
{
	struct vhost_async *async = vq->async;
	uint16_t nr_left = n_descs;
	uint16_t nr_copy;
	uint16_t to, from;

	do {
		from = async->last_desc_idx_split & (vq->size - 1);
		nr_copy = nr_left + from <= vq->size ? nr_left : vq->size - from;
		to = vq->last_used_idx & (vq->size - 1);

		if (to + nr_copy <= vq->size) {
			rte_memcpy(&vq->used->ring[to], &async->descs_split[from],
					nr_copy * sizeof(struct vring_used_elem));
		} else {
			uint16_t size = vq->size - to;

			rte_memcpy(&vq->used->ring[to], &async->descs_split[from],
					size * sizeof(struct vring_used_elem));
			rte_memcpy(&vq->used->ring[0], &async->descs_split[from + size],
					(nr_copy - size) * sizeof(struct vring_used_elem));
		}

		async->last_desc_idx_split += nr_copy;
		vq->last_used_idx += nr_copy;
		nr_left -= nr_copy;
	} while (nr_left > 0);
}

static __rte_always_inline void
write_back_completed_descs_packed(struct vhost_virtqueue *vq,
				uint16_t n_buffers)
{
	struct vhost_async *async = vq->async;
	uint16_t from = async->last_buffer_idx_packed;
	uint16_t used_idx = vq->last_used_idx;
	uint16_t head_idx = vq->last_used_idx;
	uint16_t head_flags = 0;
	uint16_t i;

	/* Split loop in two to save memory barriers */
	for (i = 0; i < n_buffers; i++) {
		vq->desc_packed[used_idx].id = async->buffers_packed[from].id;
		vq->desc_packed[used_idx].len = async->buffers_packed[from].len;

		used_idx += async->buffers_packed[from].count;
		if (used_idx >= vq->size)
			used_idx -= vq->size;

		from++;
		if (from >= vq->size)
			from = 0;
	}

	/* The ordering for storing desc flags needs to be enforced. */
	rte_atomic_thread_fence(__ATOMIC_RELEASE);

	from = async->last_buffer_idx_packed;

	for (i = 0; i < n_buffers; i++) {
		uint16_t flags;

		if (async->buffers_packed[from].len)
			flags = VRING_DESC_F_WRITE;
		else
			flags = 0;

		if (vq->used_wrap_counter) {
			flags |= VRING_DESC_F_USED;
			flags |= VRING_DESC_F_AVAIL;
		} else {
			flags &= ~VRING_DESC_F_USED;
			flags &= ~VRING_DESC_F_AVAIL;
		}

		if (i > 0) {
			vq->desc_packed[vq->last_used_idx].flags = flags;
		} else {
			head_idx = vq->last_used_idx;
			head_flags = flags;
		}

		vq_inc_last_used_packed(vq, async->buffers_packed[from].count);

		from++;
		if (from == vq->size)
			from = 0;
	}

	vq->desc_packed[head_idx].flags = head_flags;
	async->last_buffer_idx_packed = from;
}

static __rte_always_inline uint16_t
vhost_poll_enqueue_completed(struct virtio_net *dev, uint16_t queue_id,
		struct rte_mbuf **pkts, uint16_t count)
{
	struct vhost_virtqueue *vq = dev->virtqueue[queue_id];
	struct vhost_async *async = vq->async;
	struct async_inflight_info *pkts_info = async->pkts_info;
	int32_t n_cpl;
	uint16_t n_descs = 0, n_buffers = 0;
	uint16_t start_idx, from, i;

	n_cpl = async->ops.check_completed_copies(dev->vid, queue_id, 0, count);
	if (unlikely(n_cpl < 0)) {
		VHOST_LOG_DATA(ERR, "(%d) %s: failed to check completed copies for queue id %d.\n",
				dev->vid, __func__, queue_id);
		return 0;
	}

	if (n_cpl == 0)
		return 0;

	start_idx = async_get_first_inflight_pkt_idx(vq);

	for (i = 0; i < n_cpl; i++) {
		from = (start_idx + i) % vq->size;
		/* Only used with packed ring */
		n_buffers += pkts_info[from].nr_buffers;
		/* Only used with split ring */
		n_descs += pkts_info[from].descs;
		pkts[i] = pkts_info[from].mbuf;
	}

	async->pkts_inflight_n -= n_cpl;

	if (likely(vq->enabled && vq->access_ok)) {
		if (vq_is_packed(dev)) {
			write_back_completed_descs_packed(vq, n_buffers);
			vhost_vring_call_packed(dev, vq);
		} else {
			write_back_completed_descs_split(vq, n_descs);
			__atomic_add_fetch(&vq->used->idx, n_descs, __ATOMIC_RELEASE);
			vhost_vring_call_split(dev, vq);
		}
	} else {
		if (vq_is_packed(dev)) {
			async->last_buffer_idx_packed += n_buffers;
			if (async->last_buffer_idx_packed >= vq->size)
				async->last_buffer_idx_packed -= vq->size;
		} else {
			async->last_desc_idx_split += n_descs;
		}
	}

	return n_cpl;
}

uint16_t
rte_vhost_poll_enqueue_completed(int vid, uint16_t queue_id,
		struct rte_mbuf **pkts, uint16_t count)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_virtqueue *vq;
	uint16_t n_pkts_cpl = 0;

	if (unlikely(!dev))
		return 0;

	VHOST_LOG_DATA(DEBUG, "(%d) %s\n", dev->vid, __func__);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->nr_vring))) {
		VHOST_LOG_DATA(ERR, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];

	if (!rte_spinlock_trylock(&vq->access_lock)) {
		VHOST_LOG_DATA(DEBUG,
			"%s: virtqueue %u is busy.\n",
			__func__, queue_id);
		return 0;
	}

	if (unlikely(!vq->async)) {
		VHOST_LOG_DATA(ERR, "(%d) %s: async not registered for queue id %d.\n",
			dev->vid, __func__, queue_id);
		goto out;
	}

	n_pkts_cpl = vhost_poll_enqueue_completed(dev, queue_id, pkts, count);

out:
	rte_spinlock_unlock(&vq->access_lock);

	return n_pkts_cpl;
}

uint16_t
rte_vhost_clear_queue_thread_unsafe(int vid, uint16_t queue_id,
		struct rte_mbuf **pkts, uint16_t count)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_virtqueue *vq;
	uint16_t n_pkts_cpl = 0;

	if (!dev)
		return 0;

	VHOST_LOG_DATA(DEBUG, "(%d) %s\n", dev->vid, __func__);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->nr_vring))) {
		VHOST_LOG_DATA(ERR, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];

	if (unlikely(!vq->async)) {
		VHOST_LOG_DATA(ERR, "(%d) %s: async not registered for queue id %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	n_pkts_cpl = vhost_poll_enqueue_completed(dev, queue_id, pkts, count);

	return n_pkts_cpl;
}

static __rte_always_inline uint32_t
virtio_dev_rx_async_submit(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_virtqueue *vq;
	uint32_t nb_tx = 0;

	VHOST_LOG_DATA(DEBUG, "(%d) %s\n", dev->vid, __func__);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->nr_vring))) {
		VHOST_LOG_DATA(ERR, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];

	rte_spinlock_lock(&vq->access_lock);

	if (unlikely(!vq->enabled || !vq->async))
		goto out_access_unlock;

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_lock(vq);

	if (unlikely(!vq->access_ok))
		if (unlikely(vring_translate(dev, vq) < 0))
			goto out;

	count = RTE_MIN((uint32_t)MAX_PKT_BURST, count);
	if (count == 0)
		goto out;

	if (vq_is_packed(dev))
		nb_tx = virtio_dev_rx_async_submit_packed(dev, vq, queue_id,
				pkts, count);
	else
		nb_tx = virtio_dev_rx_async_submit_split(dev, vq, queue_id,
				pkts, count);

out:
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_unlock(vq);

out_access_unlock:
	rte_spinlock_unlock(&vq->access_lock);

	return nb_tx;
}

uint16_t
rte_vhost_submit_enqueue_burst(int vid, uint16_t queue_id,
		struct rte_mbuf **pkts, uint16_t count)
{
	struct virtio_net *dev = get_device(vid);

	if (!dev)
		return 0;

	if (unlikely(!(dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET))) {
		VHOST_LOG_DATA(ERR,
			"(%d) %s: built-in vhost net backend is disabled.\n",
			dev->vid, __func__);
		return 0;
	}

	return virtio_dev_rx_async_submit(dev, queue_id, pkts, count);
}

static inline bool
virtio_net_with_host_offload(struct virtio_net *dev)
{
	if (dev->features &
			((1ULL << VIRTIO_NET_F_CSUM) |
			 (1ULL << VIRTIO_NET_F_HOST_ECN) |
			 (1ULL << VIRTIO_NET_F_HOST_TSO4) |
			 (1ULL << VIRTIO_NET_F_HOST_TSO6) |
			 (1ULL << VIRTIO_NET_F_HOST_UFO)))
		return true;

	return false;
}

static int
parse_headers(struct rte_mbuf *m, uint8_t *l4_proto)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	struct rte_ether_hdr *eth_hdr;
	uint16_t ethertype;
	uint16_t data_len = rte_pktmbuf_data_len(m);

	if (data_len < sizeof(struct rte_ether_hdr))
		return -EINVAL;

	eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	m->l2_len = sizeof(struct rte_ether_hdr);
	ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (ethertype == RTE_ETHER_TYPE_VLAN) {
		if (data_len < sizeof(struct rte_ether_hdr) +
				sizeof(struct rte_vlan_hdr))
			goto error;

		struct rte_vlan_hdr *vlan_hdr =
			(struct rte_vlan_hdr *)(eth_hdr + 1);

		m->l2_len += sizeof(struct rte_vlan_hdr);
		ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	}

	switch (ethertype) {
	case RTE_ETHER_TYPE_IPV4:
		if (data_len < m->l2_len + sizeof(struct rte_ipv4_hdr))
			goto error;
		ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *,
				m->l2_len);
		m->l3_len = rte_ipv4_hdr_len(ipv4_hdr);
		if (data_len < m->l2_len + m->l3_len)
			goto error;
		m->ol_flags |= RTE_MBUF_F_TX_IPV4;
		*l4_proto = ipv4_hdr->next_proto_id;
		break;
	case RTE_ETHER_TYPE_IPV6:
		if (data_len < m->l2_len + sizeof(struct rte_ipv6_hdr))
			goto error;
		ipv6_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv6_hdr *,
				m->l2_len);
		m->l3_len = sizeof(struct rte_ipv6_hdr);
		m->ol_flags |= RTE_MBUF_F_TX_IPV6;
		*l4_proto = ipv6_hdr->proto;
		break;
	default:
		/* a valid L3 header is needed for further L4 parsing */
		goto error;
	}

	/* both CSUM and GSO need a valid L4 header */
	switch (*l4_proto) {
	case IPPROTO_TCP:
		if (data_len < m->l2_len + m->l3_len +
				sizeof(struct rte_tcp_hdr))
			goto error;
		break;
	case IPPROTO_UDP:
		if (data_len < m->l2_len + m->l3_len +
				sizeof(struct rte_udp_hdr))
			goto error;
		break;
	case IPPROTO_SCTP:
		if (data_len < m->l2_len + m->l3_len +
				sizeof(struct rte_sctp_hdr))
			goto error;
		break;
	default:
		goto error;
	}

	return 0;

error:
	m->l2_len = 0;
	m->l3_len = 0;
	m->ol_flags = 0;
	return -EINVAL;
}

static __rte_always_inline void
vhost_dequeue_offload_legacy(struct virtio_net_hdr *hdr, struct rte_mbuf *m)
{
	uint8_t l4_proto = 0;
	struct rte_tcp_hdr *tcp_hdr = NULL;
	uint16_t tcp_len;
	uint16_t data_len = rte_pktmbuf_data_len(m);

	if (parse_headers(m, &l4_proto) < 0)
		return;

	if (hdr->flags == VIRTIO_NET_HDR_F_NEEDS_CSUM) {
		if (hdr->csum_start == (m->l2_len + m->l3_len)) {
			switch (hdr->csum_offset) {
			case (offsetof(struct rte_tcp_hdr, cksum)):
				if (l4_proto != IPPROTO_TCP)
					goto error;
				m->ol_flags |= RTE_MBUF_F_TX_TCP_CKSUM;
				break;
			case (offsetof(struct rte_udp_hdr, dgram_cksum)):
				if (l4_proto != IPPROTO_UDP)
					goto error;
				m->ol_flags |= RTE_MBUF_F_TX_UDP_CKSUM;
				break;
			case (offsetof(struct rte_sctp_hdr, cksum)):
				if (l4_proto != IPPROTO_SCTP)
					goto error;
				m->ol_flags |= RTE_MBUF_F_TX_SCTP_CKSUM;
				break;
			default:
				goto error;
			}
		} else {
			goto error;
		}
	}

	if (hdr->gso_type != VIRTIO_NET_HDR_GSO_NONE) {
		switch (hdr->gso_type & ~VIRTIO_NET_HDR_GSO_ECN) {
		case VIRTIO_NET_HDR_GSO_TCPV4:
		case VIRTIO_NET_HDR_GSO_TCPV6:
			if (l4_proto != IPPROTO_TCP)
				goto error;
			tcp_hdr = rte_pktmbuf_mtod_offset(m,
					struct rte_tcp_hdr *,
					m->l2_len + m->l3_len);
			tcp_len = (tcp_hdr->data_off & 0xf0) >> 2;
			if (data_len < m->l2_len + m->l3_len + tcp_len)
				goto error;
			m->ol_flags |= RTE_MBUF_F_TX_TCP_SEG;
			m->tso_segsz = hdr->gso_size;
			m->l4_len = tcp_len;
			break;
		case VIRTIO_NET_HDR_GSO_UDP:
			if (l4_proto != IPPROTO_UDP)
				goto error;
			m->ol_flags |= RTE_MBUF_F_TX_UDP_SEG;
			m->tso_segsz = hdr->gso_size;
			m->l4_len = sizeof(struct rte_udp_hdr);
			break;
		default:
			VHOST_LOG_DATA(WARNING,
				"unsupported gso type %u.\n", hdr->gso_type);
			goto error;
		}
	}
	return;

error:
	m->l2_len = 0;
	m->l3_len = 0;
	m->ol_flags = 0;
}

static __rte_always_inline void
vhost_dequeue_offload(struct virtio_net_hdr *hdr, struct rte_mbuf *m,
	bool legacy_ol_flags)
{
	struct rte_net_hdr_lens hdr_lens;
	int l4_supported = 0;
	uint32_t ptype;

	if (hdr->flags == 0 && hdr->gso_type == VIRTIO_NET_HDR_GSO_NONE)
		return;

	if (legacy_ol_flags) {
		vhost_dequeue_offload_legacy(hdr, m);
		return;
	}

	m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN;

	ptype = rte_net_get_ptype(m, &hdr_lens, RTE_PTYPE_ALL_MASK);
	m->packet_type = ptype;
	if ((ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_TCP ||
	    (ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_UDP ||
	    (ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_SCTP)
		l4_supported = 1;

	/* According to Virtio 1.1 spec, the device only needs to look at
	 * VIRTIO_NET_HDR_F_NEEDS_CSUM in the packet transmission path.
	 * This differs from the processing incoming packets path where the
	 * driver could rely on VIRTIO_NET_HDR_F_DATA_VALID flag set by the
	 * device.
	 *
	 * 5.1.6.2.1 Driver Requirements: Packet Transmission
	 * The driver MUST NOT set the VIRTIO_NET_HDR_F_DATA_VALID and
	 * VIRTIO_NET_HDR_F_RSC_INFO bits in flags.
	 *
	 * 5.1.6.2.2 Device Requirements: Packet Transmission
	 * The device MUST ignore flag bits that it does not recognize.
	 */
	if (hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM) {
		uint32_t hdrlen;

		hdrlen = hdr_lens.l2_len + hdr_lens.l3_len + hdr_lens.l4_len;
		if (hdr->csum_start <= hdrlen && l4_supported != 0) {
			m->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_NONE;
		} else {
			/* Unknown proto or tunnel, do sw cksum. We can assume
			 * the cksum field is in the first segment since the
			 * buffers we provided to the host are large enough.
			 * In case of SCTP, this will be wrong since it's a CRC
			 * but there's nothing we can do.
			 */
			uint16_t csum = 0, off;

			if (rte_raw_cksum_mbuf(m, hdr->csum_start,
					rte_pktmbuf_pkt_len(m) - hdr->csum_start, &csum) < 0)
				return;
			if (likely(csum != 0xffff))
				csum = ~csum;
			off = hdr->csum_offset + hdr->csum_start;
			if (rte_pktmbuf_data_len(m) >= off + 1)
				*rte_pktmbuf_mtod_offset(m, uint16_t *, off) = csum;
		}
	}

	if (hdr->gso_type != VIRTIO_NET_HDR_GSO_NONE) {
		if (hdr->gso_size == 0)
			return;

		switch (hdr->gso_type & ~VIRTIO_NET_HDR_GSO_ECN) {
		case VIRTIO_NET_HDR_GSO_TCPV4:
		case VIRTIO_NET_HDR_GSO_TCPV6:
			if ((ptype & RTE_PTYPE_L4_MASK) != RTE_PTYPE_L4_TCP)
				break;
			m->ol_flags |= RTE_MBUF_F_RX_LRO | RTE_MBUF_F_RX_L4_CKSUM_NONE;
			m->tso_segsz = hdr->gso_size;
			break;
		case VIRTIO_NET_HDR_GSO_UDP:
			if ((ptype & RTE_PTYPE_L4_MASK) != RTE_PTYPE_L4_UDP)
				break;
			m->ol_flags |= RTE_MBUF_F_RX_LRO | RTE_MBUF_F_RX_L4_CKSUM_NONE;
			m->tso_segsz = hdr->gso_size;
			break;
		default:
			break;
		}
	}
}

static __rte_noinline void
copy_vnet_hdr_from_desc(struct virtio_net_hdr *hdr,
		struct buf_vector *buf_vec)
{
	uint64_t len;
	uint64_t remain = sizeof(struct virtio_net_hdr);
	uint64_t src;
	uint64_t dst = (uint64_t)(uintptr_t)hdr;

	while (remain) {
		len = RTE_MIN(remain, buf_vec->buf_len);
		src = buf_vec->buf_addr;
		rte_memcpy((void *)(uintptr_t)dst,
				(void *)(uintptr_t)src, len);

		remain -= len;
		dst += len;
		buf_vec++;
	}
}

static __rte_always_inline int
copy_desc_to_mbuf(struct virtio_net *dev, struct vhost_virtqueue *vq,
		  struct buf_vector *buf_vec, uint16_t nr_vec,
		  struct rte_mbuf *m, struct rte_mempool *mbuf_pool,
		  bool legacy_ol_flags)
{
	uint32_t buf_avail, buf_offset;
	uint64_t buf_addr, buf_len;
	uint32_t mbuf_avail, mbuf_offset;
	uint32_t hdr_remain = dev->vhost_hlen;
	uint32_t cpy_len;
	struct rte_mbuf *cur = m, *prev = m;
	struct virtio_net_hdr tmp_hdr;
	struct virtio_net_hdr *hdr = NULL;
	uint16_t vec_idx;
	struct batch_copy_elem *batch_copy = vq->batch_copy_elems;
	int error = 0;

	/*
	 * The caller has checked the descriptors chain is larger than the
	 * header size.
	 */

	if (virtio_net_with_host_offload(dev)) {
		if (unlikely(buf_vec[0].buf_len < sizeof(struct virtio_net_hdr))) {
			/*
			 * No luck, the virtio-net header doesn't fit
			 * in a contiguous virtual area.
			 */
			copy_vnet_hdr_from_desc(&tmp_hdr, buf_vec);
			hdr = &tmp_hdr;
		} else {
			hdr = (struct virtio_net_hdr *)((uintptr_t)buf_vec[0].buf_addr);
		}
	}

	for (vec_idx = 0; vec_idx < nr_vec; vec_idx++) {
		if (buf_vec[vec_idx].buf_len > hdr_remain)
			break;

		hdr_remain -= buf_vec[vec_idx].buf_len;
	}

	buf_addr = buf_vec[vec_idx].buf_addr;
	buf_len = buf_vec[vec_idx].buf_len;
	buf_offset = hdr_remain;
	buf_avail = buf_vec[vec_idx].buf_len - hdr_remain;

	PRINT_PACKET(dev,
			(uintptr_t)(buf_addr + buf_offset),
			(uint32_t)buf_avail, 0);

	mbuf_offset = 0;
	mbuf_avail  = m->buf_len - RTE_PKTMBUF_HEADROOM;
	while (1) {
		cpy_len = RTE_MIN(buf_avail, mbuf_avail);

		if (likely(cpy_len > MAX_BATCH_LEN ||
					vq->batch_copy_nb_elems >= vq->size ||
					(hdr && cur == m))) {
			rte_memcpy(rte_pktmbuf_mtod_offset(cur, void *,
						mbuf_offset),
					(void *)((uintptr_t)(buf_addr +
							buf_offset)), cpy_len);
		} else {
			batch_copy[vq->batch_copy_nb_elems].dst =
				rte_pktmbuf_mtod_offset(cur, void *,
						mbuf_offset);
			batch_copy[vq->batch_copy_nb_elems].src =
				(void *)((uintptr_t)(buf_addr + buf_offset));
			batch_copy[vq->batch_copy_nb_elems].len = cpy_len;
			vq->batch_copy_nb_elems++;
		}

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		buf_avail -= cpy_len;
		buf_offset += cpy_len;

		/* This buf reaches to its end, get the next one */
		if (buf_avail == 0) {
			if (++vec_idx >= nr_vec)
				break;

			buf_addr = buf_vec[vec_idx].buf_addr;
			buf_len = buf_vec[vec_idx].buf_len;

			buf_offset = 0;
			buf_avail  = buf_len;

			PRINT_PACKET(dev, (uintptr_t)buf_addr,
					(uint32_t)buf_avail, 0);
		}

		/*
		 * This mbuf reaches to its end, get a new one
		 * to hold more data.
		 */
		if (mbuf_avail == 0) {
			cur = rte_pktmbuf_alloc(mbuf_pool);
			if (unlikely(cur == NULL)) {
				VHOST_LOG_DATA(ERR, "Failed to "
					"allocate memory for mbuf.\n");
				error = -1;
				goto out;
			}

			prev->next = cur;
			prev->data_len = mbuf_offset;
			m->nb_segs += 1;
			m->pkt_len += mbuf_offset;
			prev = cur;

			mbuf_offset = 0;
			mbuf_avail  = cur->buf_len - RTE_PKTMBUF_HEADROOM;
		}
	}

	prev->data_len = mbuf_offset;
	m->pkt_len    += mbuf_offset;

	if (hdr)
		vhost_dequeue_offload(hdr, m, legacy_ol_flags);

out:

	return error;
}

static void
virtio_dev_extbuf_free(void *addr __rte_unused, void *opaque)
{
	rte_free(opaque);
}

static int
virtio_dev_extbuf_alloc(struct rte_mbuf *pkt, uint32_t size)
{
	struct rte_mbuf_ext_shared_info *shinfo = NULL;
	uint32_t total_len = RTE_PKTMBUF_HEADROOM + size;
	uint16_t buf_len;
	rte_iova_t iova;
	void *buf;

	total_len += sizeof(*shinfo) + sizeof(uintptr_t);
	total_len = RTE_ALIGN_CEIL(total_len, sizeof(uintptr_t));

	if (unlikely(total_len > UINT16_MAX))
		return -ENOSPC;

	buf_len = total_len;
	buf = rte_malloc(NULL, buf_len, RTE_CACHE_LINE_SIZE);
	if (unlikely(buf == NULL))
		return -ENOMEM;

	/* Initialize shinfo */
	shinfo = rte_pktmbuf_ext_shinfo_init_helper(buf, &buf_len,
						virtio_dev_extbuf_free, buf);
	if (unlikely(shinfo == NULL)) {
		rte_free(buf);
		VHOST_LOG_DATA(ERR, "Failed to init shinfo\n");
		return -1;
	}

	iova = rte_malloc_virt2iova(buf);
	rte_pktmbuf_attach_extbuf(pkt, buf, iova, buf_len, shinfo);
	rte_pktmbuf_reset_headroom(pkt);

	return 0;
}

/*
 * Prepare a host supported pktmbuf.
 */
static __rte_always_inline int
virtio_dev_pktmbuf_prep(struct virtio_net *dev, struct rte_mbuf *pkt,
			 uint32_t data_len)
{
	if (rte_pktmbuf_tailroom(pkt) >= data_len)
		return 0;

	/* attach an external buffer if supported */
	if (dev->extbuf && !virtio_dev_extbuf_alloc(pkt, data_len))
		return 0;

	/* check if chained buffers are allowed */
	if (!dev->linearbuf)
		return 0;

	return -1;
}

__rte_always_inline
static uint16_t
virtio_dev_tx_split(struct virtio_net *dev, struct vhost_virtqueue *vq,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count,
	bool legacy_ol_flags)
{
	uint16_t i;
	uint16_t free_entries;
	uint16_t dropped = 0;
	static bool allocerr_warned;

	/*
	 * The ordering between avail index and
	 * desc reads needs to be enforced.
	 */
	free_entries = __atomic_load_n(&vq->avail->idx, __ATOMIC_ACQUIRE) -
			vq->last_avail_idx;
	if (free_entries == 0)
		return 0;

	rte_prefetch0(&vq->avail->ring[vq->last_avail_idx & (vq->size - 1)]);

	VHOST_LOG_DATA(DEBUG, "(%d) %s\n", dev->vid, __func__);

	count = RTE_MIN(count, MAX_PKT_BURST);
	count = RTE_MIN(count, free_entries);
	VHOST_LOG_DATA(DEBUG, "(%d) about to dequeue %u buffers\n",
			dev->vid, count);

	if (rte_pktmbuf_alloc_bulk(mbuf_pool, pkts, count))
		return 0;

	for (i = 0; i < count; i++) {
		struct buf_vector buf_vec[BUF_VECTOR_MAX];
		uint16_t head_idx;
		uint32_t buf_len;
		uint16_t nr_vec = 0;
		int err;

		if (unlikely(fill_vec_buf_split(dev, vq,
						vq->last_avail_idx + i,
						&nr_vec, buf_vec,
						&head_idx, &buf_len,
						VHOST_ACCESS_RO) < 0))
			break;

		update_shadow_used_ring_split(vq, head_idx, 0);

		if (unlikely(buf_len <= dev->vhost_hlen)) {
			dropped += 1;
			i++;
			break;
		}

		buf_len -= dev->vhost_hlen;

		err = virtio_dev_pktmbuf_prep(dev, pkts[i], buf_len);
		if (unlikely(err)) {
			/*
			 * mbuf allocation fails for jumbo packets when external
			 * buffer allocation is not allowed and linear buffer
			 * is required. Drop this packet.
			 */
			if (!allocerr_warned) {
				VHOST_LOG_DATA(ERR,
					"Failed mbuf alloc of size %d from %s on %s.\n",
					buf_len, mbuf_pool->name, dev->ifname);
				allocerr_warned = true;
			}
			dropped += 1;
			i++;
			break;
		}

		err = copy_desc_to_mbuf(dev, vq, buf_vec, nr_vec, pkts[i],
				mbuf_pool, legacy_ol_flags);
		if (unlikely(err)) {
			if (!allocerr_warned) {
				VHOST_LOG_DATA(ERR,
					"Failed to copy desc to mbuf on %s.\n",
					dev->ifname);
				allocerr_warned = true;
			}
			dropped += 1;
			i++;
			break;
		}
	}

	if (dropped)
		rte_pktmbuf_free_bulk(&pkts[i - 1], count - i + 1);

	vq->last_avail_idx += i;

	do_data_copy_dequeue(vq);
	if (unlikely(i < count))
		vq->shadow_used_idx = i;
	if (likely(vq->shadow_used_idx)) {
		flush_shadow_used_ring_split(dev, vq);
		vhost_vring_call_split(dev, vq);
	}

	return (i - dropped);
}

__rte_noinline
static uint16_t
virtio_dev_tx_split_legacy(struct virtio_net *dev,
	struct vhost_virtqueue *vq, struct rte_mempool *mbuf_pool,
	struct rte_mbuf **pkts, uint16_t count)
{
	return virtio_dev_tx_split(dev, vq, mbuf_pool, pkts, count, true);
}

__rte_noinline
static uint16_t
virtio_dev_tx_split_compliant(struct virtio_net *dev,
	struct vhost_virtqueue *vq, struct rte_mempool *mbuf_pool,
	struct rte_mbuf **pkts, uint16_t count)
{
	return virtio_dev_tx_split(dev, vq, mbuf_pool, pkts, count, false);
}

static __rte_always_inline int
vhost_reserve_avail_batch_packed(struct virtio_net *dev,
				 struct vhost_virtqueue *vq,
				 struct rte_mbuf **pkts,
				 uint16_t avail_idx,
				 uintptr_t *desc_addrs,
				 uint16_t *ids)
{
	bool wrap = vq->avail_wrap_counter;
	struct vring_packed_desc *descs = vq->desc_packed;
	uint64_t lens[PACKED_BATCH_SIZE];
	uint64_t buf_lens[PACKED_BATCH_SIZE];
	uint32_t buf_offset = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	uint16_t flags, i;

	if (unlikely(avail_idx & PACKED_BATCH_MASK))
		return -1;
	if (unlikely((avail_idx + PACKED_BATCH_SIZE) > vq->size))
		return -1;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		flags = descs[avail_idx + i].flags;
		if (unlikely((wrap != !!(flags & VRING_DESC_F_AVAIL)) ||
			     (wrap == !!(flags & VRING_DESC_F_USED))  ||
			     (flags & PACKED_DESC_SINGLE_DEQUEUE_FLAG)))
			return -1;
	}

	rte_atomic_thread_fence(__ATOMIC_ACQUIRE);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		lens[i] = descs[avail_idx + i].len;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		desc_addrs[i] = vhost_iova_to_vva(dev, vq,
						  descs[avail_idx + i].addr,
						  &lens[i], VHOST_ACCESS_RW);
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (unlikely(!desc_addrs[i]))
			return -1;
		if (unlikely((lens[i] != descs[avail_idx + i].len)))
			return -1;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (virtio_dev_pktmbuf_prep(dev, pkts[i], lens[i]))
			goto err;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		buf_lens[i] = pkts[i]->buf_len - pkts[i]->data_off;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (unlikely(buf_lens[i] < (lens[i] - buf_offset)))
			goto err;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		pkts[i]->pkt_len = lens[i] - buf_offset;
		pkts[i]->data_len = pkts[i]->pkt_len;
		ids[i] = descs[avail_idx + i].id;
	}

	return 0;

err:
	return -1;
}

static __rte_always_inline int
virtio_dev_tx_batch_packed(struct virtio_net *dev,
			   struct vhost_virtqueue *vq,
			   struct rte_mbuf **pkts,
			   bool legacy_ol_flags)
{
	uint16_t avail_idx = vq->last_avail_idx;
	uint32_t buf_offset = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	struct virtio_net_hdr *hdr;
	uintptr_t desc_addrs[PACKED_BATCH_SIZE];
	uint16_t ids[PACKED_BATCH_SIZE];
	uint16_t i;

	if (vhost_reserve_avail_batch_packed(dev, vq, pkts, avail_idx,
					     desc_addrs, ids))
		return -1;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		rte_prefetch0((void *)(uintptr_t)desc_addrs[i]);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		rte_memcpy(rte_pktmbuf_mtod_offset(pkts[i], void *, 0),
			   (void *)(uintptr_t)(desc_addrs[i] + buf_offset),
			   pkts[i]->pkt_len);

	if (virtio_net_with_host_offload(dev)) {
		vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
			hdr = (struct virtio_net_hdr *)(desc_addrs[i]);
			vhost_dequeue_offload(hdr, pkts[i], legacy_ol_flags);
		}
	}

	if (virtio_net_is_inorder(dev))
		vhost_shadow_dequeue_batch_packed_inorder(vq,
			ids[PACKED_BATCH_SIZE - 1]);
	else
		vhost_shadow_dequeue_batch_packed(dev, vq, ids);

	vq_inc_last_avail_packed(vq, PACKED_BATCH_SIZE);

	return 0;
}

static __rte_always_inline int
vhost_dequeue_single_packed(struct virtio_net *dev,
			    struct vhost_virtqueue *vq,
			    struct rte_mempool *mbuf_pool,
			    struct rte_mbuf *pkts,
			    uint16_t *buf_id,
			    uint16_t *desc_count,
			    bool legacy_ol_flags)
{
	struct buf_vector buf_vec[BUF_VECTOR_MAX];
	uint32_t buf_len;
	uint16_t nr_vec = 0;
	int err;
	static bool allocerr_warned;

	if (unlikely(fill_vec_buf_packed(dev, vq,
					 vq->last_avail_idx, desc_count,
					 buf_vec, &nr_vec,
					 buf_id, &buf_len,
					 VHOST_ACCESS_RO) < 0))
		return -1;

	if (unlikely(buf_len <= dev->vhost_hlen))
		return -1;

	buf_len -= dev->vhost_hlen;

	if (unlikely(virtio_dev_pktmbuf_prep(dev, pkts, buf_len))) {
		if (!allocerr_warned) {
			VHOST_LOG_DATA(ERR,
				"Failed mbuf alloc of size %d from %s on %s.\n",
				buf_len, mbuf_pool->name, dev->ifname);
			allocerr_warned = true;
		}
		return -1;
	}

	err = copy_desc_to_mbuf(dev, vq, buf_vec, nr_vec, pkts,
				mbuf_pool, legacy_ol_flags);
	if (unlikely(err)) {
		if (!allocerr_warned) {
			VHOST_LOG_DATA(ERR,
				"Failed to copy desc to mbuf on %s.\n",
				dev->ifname);
			allocerr_warned = true;
		}
		return -1;
	}

	return 0;
}

static __rte_always_inline int
virtio_dev_tx_single_packed(struct virtio_net *dev,
			    struct vhost_virtqueue *vq,
			    struct rte_mempool *mbuf_pool,
			    struct rte_mbuf *pkts,
			    bool legacy_ol_flags)
{

	uint16_t buf_id, desc_count = 0;
	int ret;

	ret = vhost_dequeue_single_packed(dev, vq, mbuf_pool, pkts, &buf_id,
					&desc_count, legacy_ol_flags);

	if (likely(desc_count > 0)) {
		if (virtio_net_is_inorder(dev))
			vhost_shadow_dequeue_single_packed_inorder(vq, buf_id,
								   desc_count);
		else
			vhost_shadow_dequeue_single_packed(vq, buf_id,
					desc_count);

		vq_inc_last_avail_packed(vq, desc_count);
	}

	return ret;
}

__rte_always_inline
static uint16_t
virtio_dev_tx_packed(struct virtio_net *dev,
		     struct vhost_virtqueue *__rte_restrict vq,
		     struct rte_mempool *mbuf_pool,
		     struct rte_mbuf **__rte_restrict pkts,
		     uint32_t count,
		     bool legacy_ol_flags)
{
	uint32_t pkt_idx = 0;

	if (rte_pktmbuf_alloc_bulk(mbuf_pool, pkts, count))
		return 0;

	do {
		rte_prefetch0(&vq->desc_packed[vq->last_avail_idx]);

		if (count - pkt_idx >= PACKED_BATCH_SIZE) {
			if (!virtio_dev_tx_batch_packed(dev, vq,
							&pkts[pkt_idx],
							legacy_ol_flags)) {
				pkt_idx += PACKED_BATCH_SIZE;
				continue;
			}
		}

		if (virtio_dev_tx_single_packed(dev, vq, mbuf_pool,
						pkts[pkt_idx],
						legacy_ol_flags))
			break;
		pkt_idx++;
	} while (pkt_idx < count);

	if (pkt_idx != count)
		rte_pktmbuf_free_bulk(&pkts[pkt_idx], count - pkt_idx);

	if (vq->shadow_used_idx) {
		do_data_copy_dequeue(vq);

		vhost_flush_dequeue_shadow_packed(dev, vq);
		vhost_vring_call_packed(dev, vq);
	}

	return pkt_idx;
}

__rte_noinline
static uint16_t
virtio_dev_tx_packed_legacy(struct virtio_net *dev,
	struct vhost_virtqueue *__rte_restrict vq, struct rte_mempool *mbuf_pool,
	struct rte_mbuf **__rte_restrict pkts, uint32_t count)
{
	return virtio_dev_tx_packed(dev, vq, mbuf_pool, pkts, count, true);
}

__rte_noinline
static uint16_t
virtio_dev_tx_packed_compliant(struct virtio_net *dev,
	struct vhost_virtqueue *__rte_restrict vq, struct rte_mempool *mbuf_pool,
	struct rte_mbuf **__rte_restrict pkts, uint32_t count)
{
	return virtio_dev_tx_packed(dev, vq, mbuf_pool, pkts, count, false);
}

uint16_t
rte_vhost_dequeue_burst(int vid, uint16_t queue_id,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
{
	struct virtio_net *dev;
	struct rte_mbuf *rarp_mbuf = NULL;
	struct vhost_virtqueue *vq;
	int16_t success = 1;

	dev = get_device(vid);
	if (!dev)
		return 0;

	if (unlikely(!(dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET))) {
		VHOST_LOG_DATA(ERR,
			"(%d) %s: built-in vhost net backend is disabled.\n",
			dev->vid, __func__);
		return 0;
	}

	if (unlikely(!is_valid_virt_queue_idx(queue_id, 1, dev->nr_vring))) {
		VHOST_LOG_DATA(ERR,
			"(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];

	if (unlikely(rte_spinlock_trylock(&vq->access_lock) == 0))
		return 0;

	if (unlikely(!vq->enabled)) {
		count = 0;
		goto out_access_unlock;
	}

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_lock(vq);

	if (unlikely(!vq->access_ok))
		if (unlikely(vring_translate(dev, vq) < 0)) {
			count = 0;
			goto out;
		}

	/*
	 * Construct a RARP broadcast packet, and inject it to the "pkts"
	 * array, to looks like that guest actually send such packet.
	 *
	 * Check user_send_rarp() for more information.
	 *
	 * broadcast_rarp shares a cacheline in the virtio_net structure
	 * with some fields that are accessed during enqueue and
	 * __atomic_compare_exchange_n causes a write if performed compare
	 * and exchange. This could result in false sharing between enqueue
	 * and dequeue.
	 *
	 * Prevent unnecessary false sharing by reading broadcast_rarp first
	 * and only performing compare and exchange if the read indicates it
	 * is likely to be set.
	 */
	if (unlikely(__atomic_load_n(&dev->broadcast_rarp, __ATOMIC_ACQUIRE) &&
			__atomic_compare_exchange_n(&dev->broadcast_rarp,
			&success, 0, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED))) {

		rarp_mbuf = rte_net_make_rarp_packet(mbuf_pool, &dev->mac);
		if (rarp_mbuf == NULL) {
			VHOST_LOG_DATA(ERR, "Failed to make RARP packet.\n");
			count = 0;
			goto out;
		}
		/*
		 * Inject it to the head of "pkts" array, so that switch's mac
		 * learning table will get updated first.
		 */
		pkts[0] = rarp_mbuf;
		pkts++;
		count -= 1;
	}

	if (vq_is_packed(dev)) {
		if (dev->flags & VIRTIO_DEV_LEGACY_OL_FLAGS)
			count = virtio_dev_tx_packed_legacy(dev, vq, mbuf_pool, pkts, count);
		else
			count = virtio_dev_tx_packed_compliant(dev, vq, mbuf_pool, pkts, count);
	} else {
		if (dev->flags & VIRTIO_DEV_LEGACY_OL_FLAGS)
			count = virtio_dev_tx_split_legacy(dev, vq, mbuf_pool, pkts, count);
		else
			count = virtio_dev_tx_split_compliant(dev, vq, mbuf_pool, pkts, count);
	}

out:
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_unlock(vq);

out_access_unlock:
	rte_spinlock_unlock(&vq->access_lock);

	if (unlikely(rarp_mbuf != NULL))
		count += 1;

	return count;
}
