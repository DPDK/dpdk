/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdint.h>
#include <stdbool.h>
#include <linux/virtio_net.h>

#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_vhost.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_sctp.h>
#include <rte_arp.h>
#include <rte_spinlock.h>
#include <rte_malloc.h>

#include "iotlb.h"
#include "vhost.h"

#define MAX_PKT_BURST 32

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

	rte_smp_wmb();

	vhost_log_cache_sync(dev, vq);

	*(volatile uint16_t *)&vq->used->idx += vq->shadow_used_idx;
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

	rte_smp_wmb();

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
	rte_smp_wmb();
	vq->desc_packed[vq->shadow_last_used_idx].flags = used_elem->flags;

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

	flags = PACKED_DESC_ENQUEUE_USED_FLAG(vq->used_wrap_counter);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		vq->desc_packed[vq->last_used_idx + i].id = ids[i];
		vq->desc_packed[vq->last_used_idx + i].len = lens[i];
	}

	rte_smp_wmb();

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		vq->desc_packed[vq->last_used_idx + i].flags = flags;

	vhost_log_cache_used_vring(dev, vq, vq->last_used_idx *
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

	rte_smp_wmb();
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
vhost_shadow_enqueue_single_packed(struct virtio_net *dev,
				   struct vhost_virtqueue *vq,
				   uint32_t len[],
				   uint16_t id[],
				   uint16_t count[],
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

	if (vq->shadow_aligned_idx >= PACKED_BATCH_SIZE) {
		do_data_copy_enqueue(dev, vq);
		vhost_flush_enqueue_shadow_packed(dev, vq);
	}
}

static __rte_always_inline void
vhost_flush_dequeue_packed(struct virtio_net *dev,
			   struct vhost_virtqueue *vq)
{
	int shadow_count;
	if (!vq->shadow_used_idx)
		return;

	shadow_count = vq->last_used_idx - vq->shadow_last_used_idx;
	if (shadow_count <= 0)
		shadow_count += vq->size;

	if ((uint32_t)shadow_count >= (vq->size - MAX_PKT_BURST)) {
		do_data_copy_dequeue(vq);
		vhost_flush_dequeue_shadow_packed(dev, vq);
		vhost_vring_call_packed(dev, vq);
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
	uint64_t csum_l4 = m_buf->ol_flags & PKT_TX_L4_MASK;

	if (m_buf->ol_flags & PKT_TX_TCP_SEG)
		csum_l4 |= PKT_TX_TCP_CKSUM;

	if (csum_l4) {
		net_hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
		net_hdr->csum_start = m_buf->l2_len + m_buf->l3_len;

		switch (csum_l4) {
		case PKT_TX_TCP_CKSUM:
			net_hdr->csum_offset = (offsetof(struct rte_tcp_hdr,
						cksum));
			break;
		case PKT_TX_UDP_CKSUM:
			net_hdr->csum_offset = (offsetof(struct rte_udp_hdr,
						dgram_cksum));
			break;
		case PKT_TX_SCTP_CKSUM:
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
	if (m_buf->ol_flags & PKT_TX_IP_CKSUM) {
		struct rte_ipv4_hdr *ipv4_hdr;

		ipv4_hdr = rte_pktmbuf_mtod_offset(m_buf, struct rte_ipv4_hdr *,
						   m_buf->l2_len);
		ipv4_hdr->hdr_checksum = 0;
		ipv4_hdr->hdr_checksum = rte_ipv4_cksum(ipv4_hdr);
	}

	if (m_buf->ol_flags & PKT_TX_TCP_SEG) {
		if (m_buf->ol_flags & PKT_TX_IPV4)
			net_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
		else
			net_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV6;
		net_hdr->gso_size = m_buf->tso_segsz;
		net_hdr->hdr_len = m_buf->l2_len + m_buf->l3_len
					+ m_buf->l4_len;
	} else if (m_buf->ol_flags & PKT_TX_UDP_SEG) {
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

		len += descs[idx].len;

		if (unlikely(map_one_desc(dev, vq, buf_vec, &vec_id,
						descs[idx].addr, descs[idx].len,
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

		*len += descs[i].len;
		if (unlikely(map_one_desc(dev, vq, buf_vec, &vec_id,
						descs[i].addr, descs[i].len,
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
			*len += descs[avail_idx].len;

			if (unlikely(map_one_desc(dev, vq, buf_vec, &vec_id,
							descs[avail_idx].addr,
							descs[avail_idx].len,
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
copy_mbuf_to_desc(struct virtio_net *dev, struct vhost_virtqueue *vq,
			    struct rte_mbuf *m, struct buf_vector *buf_vec,
			    uint16_t nr_vec, uint16_t num_buffers)
{
	uint32_t vec_idx = 0;
	uint32_t mbuf_offset, mbuf_avail;
	uint32_t buf_offset, buf_avail;
	uint64_t buf_addr, buf_iova, buf_len;
	uint32_t cpy_len;
	uint64_t hdr_addr;
	struct rte_mbuf *hdr_mbuf;
	struct batch_copy_elem *batch_copy = vq->batch_copy_elems;
	struct virtio_net_hdr_mrg_rxbuf tmp_hdr, *hdr = NULL;
	int error = 0;

	if (unlikely(m == NULL)) {
		error = -1;
		goto out;
	}

	buf_addr = buf_vec[vec_idx].buf_addr;
	buf_iova = buf_vec[vec_idx].buf_iova;
	buf_len = buf_vec[vec_idx].buf_len;

	if (unlikely(buf_len < dev->vhost_hlen && nr_vec <= 1)) {
		error = -1;
		goto out;
	}

	hdr_mbuf = m;
	hdr_addr = buf_addr;
	if (unlikely(buf_len < dev->vhost_hlen))
		hdr = &tmp_hdr;
	else
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
	while (mbuf_avail != 0 || m->next != NULL) {
		/* done with current buf, get the next one */
		if (buf_avail == 0) {
			vec_idx++;
			if (unlikely(vec_idx >= nr_vec)) {
				error = -1;
				goto out;
			}

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

		if (likely(cpy_len > MAX_BATCH_LEN ||
					vq->batch_copy_nb_elems >= vq->size)) {
			rte_memcpy((void *)((uintptr_t)(buf_addr + buf_offset)),
				rte_pktmbuf_mtod_offset(m, void *, mbuf_offset),
				cpy_len);
			vhost_log_cache_write_iova(dev, vq,
						   buf_iova + buf_offset,
						   cpy_len);
			PRINT_PACKET(dev, (uintptr_t)(buf_addr + buf_offset),
				cpy_len, 0);
		} else {
			batch_copy[vq->batch_copy_nb_elems].dst =
				(void *)((uintptr_t)(buf_addr + buf_offset));
			batch_copy[vq->batch_copy_nb_elems].src =
				rte_pktmbuf_mtod_offset(m, void *, mbuf_offset);
			batch_copy[vq->batch_copy_nb_elems].log_addr =
				buf_iova + buf_offset;
			batch_copy[vq->batch_copy_nb_elems].len = cpy_len;
			vq->batch_copy_nb_elems++;
		}

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		buf_avail  -= cpy_len;
		buf_offset += cpy_len;
	}

out:

	return error;
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
	uint32_t size = pkt->pkt_len + dev->vhost_hlen;
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

	if (copy_mbuf_to_desc(dev, vq, pkt, buf_vec, nr_vec, num_buffers) < 0)
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

	avail_head = *((volatile uint16_t *)&vq->avail->idx);

	/*
	 * The ordering between avail index and
	 * desc reads needs to be enforced.
	 */
	rte_smp_rmb();

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

		if (copy_mbuf_to_desc(dev, vq, pkts[pkt_idx],
						buf_vec, nr_vec,
						num_buffers) < 0) {
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
virtio_dev_rx_batch_packed(struct virtio_net *dev,
			   struct vhost_virtqueue *vq,
			   struct rte_mbuf **pkts)
{
	bool wrap_counter = vq->avail_wrap_counter;
	struct vring_packed_desc *descs = vq->desc_packed;
	uint16_t avail_idx = vq->last_avail_idx;
	uint64_t desc_addrs[PACKED_BATCH_SIZE];
	struct virtio_net_hdr_mrg_rxbuf *hdrs[PACKED_BATCH_SIZE];
	uint32_t buf_offset = dev->vhost_hlen;
	uint64_t lens[PACKED_BATCH_SIZE];
	uint16_t ids[PACKED_BATCH_SIZE];
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

	rte_smp_rmb();

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
		if (unlikely(lens[i] != descs[avail_idx + i].len))
			return -1;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		rte_prefetch0((void *)(uintptr_t)desc_addrs[i]);
		hdrs[i] = (struct virtio_net_hdr_mrg_rxbuf *)
					(uintptr_t)desc_addrs[i];
		lens[i] = pkts[i]->pkt_len + dev->vhost_hlen;
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

	return 0;
}

static __rte_always_inline int16_t
virtio_dev_rx_single_packed(struct virtio_net *dev,
			    struct vhost_virtqueue *vq,
			    struct rte_mbuf *pkt)
{
	struct buf_vector buf_vec[BUF_VECTOR_MAX];
	uint16_t nr_descs = 0;

	rte_smp_rmb();
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
		     struct vhost_virtqueue *vq,
		     struct rte_mbuf **pkts,
		     uint32_t count)
{
	uint32_t pkt_idx = 0;
	uint32_t remained = count;

	do {
		rte_prefetch0(&vq->desc_packed[vq->last_avail_idx]);

		if (remained >= PACKED_BATCH_SIZE) {
			if (!virtio_dev_rx_batch_packed(dev, vq,
							&pkts[pkt_idx])) {
				pkt_idx += PACKED_BATCH_SIZE;
				remained -= PACKED_BATCH_SIZE;
				continue;
			}
		}

		if (virtio_dev_rx_single_packed(dev, vq, pkts[pkt_idx]))
			break;
		pkt_idx++;
		remained--;

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

	if (unlikely(vq->enabled == 0))
		goto out_access_unlock;

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_lock(vq);

	if (unlikely(vq->access_ok == 0))
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

	return virtio_dev_rx(dev, queue_id, pkts, count);
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

static void
parse_ethernet(struct rte_mbuf *m, uint16_t *l4_proto, void **l4_hdr)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	void *l3_hdr = NULL;
	struct rte_ether_hdr *eth_hdr;
	uint16_t ethertype;

	eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	m->l2_len = sizeof(struct rte_ether_hdr);
	ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (ethertype == RTE_ETHER_TYPE_VLAN) {
		struct rte_vlan_hdr *vlan_hdr =
			(struct rte_vlan_hdr *)(eth_hdr + 1);

		m->l2_len += sizeof(struct rte_vlan_hdr);
		ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	}

	l3_hdr = (char *)eth_hdr + m->l2_len;

	switch (ethertype) {
	case RTE_ETHER_TYPE_IPV4:
		ipv4_hdr = l3_hdr;
		*l4_proto = ipv4_hdr->next_proto_id;
		m->l3_len = (ipv4_hdr->version_ihl & 0x0f) * 4;
		*l4_hdr = (char *)l3_hdr + m->l3_len;
		m->ol_flags |= PKT_TX_IPV4;
		break;
	case RTE_ETHER_TYPE_IPV6:
		ipv6_hdr = l3_hdr;
		*l4_proto = ipv6_hdr->proto;
		m->l3_len = sizeof(struct rte_ipv6_hdr);
		*l4_hdr = (char *)l3_hdr + m->l3_len;
		m->ol_flags |= PKT_TX_IPV6;
		break;
	default:
		m->l3_len = 0;
		*l4_proto = 0;
		*l4_hdr = NULL;
		break;
	}
}

static __rte_always_inline void
vhost_dequeue_offload(struct virtio_net_hdr *hdr, struct rte_mbuf *m)
{
	uint16_t l4_proto = 0;
	void *l4_hdr = NULL;
	struct rte_tcp_hdr *tcp_hdr = NULL;

	if (hdr->flags == 0 && hdr->gso_type == VIRTIO_NET_HDR_GSO_NONE)
		return;

	parse_ethernet(m, &l4_proto, &l4_hdr);
	if (hdr->flags == VIRTIO_NET_HDR_F_NEEDS_CSUM) {
		if (hdr->csum_start == (m->l2_len + m->l3_len)) {
			switch (hdr->csum_offset) {
			case (offsetof(struct rte_tcp_hdr, cksum)):
				if (l4_proto == IPPROTO_TCP)
					m->ol_flags |= PKT_TX_TCP_CKSUM;
				break;
			case (offsetof(struct rte_udp_hdr, dgram_cksum)):
				if (l4_proto == IPPROTO_UDP)
					m->ol_flags |= PKT_TX_UDP_CKSUM;
				break;
			case (offsetof(struct rte_sctp_hdr, cksum)):
				if (l4_proto == IPPROTO_SCTP)
					m->ol_flags |= PKT_TX_SCTP_CKSUM;
				break;
			default:
				break;
			}
		}
	}

	if (l4_hdr && hdr->gso_type != VIRTIO_NET_HDR_GSO_NONE) {
		switch (hdr->gso_type & ~VIRTIO_NET_HDR_GSO_ECN) {
		case VIRTIO_NET_HDR_GSO_TCPV4:
		case VIRTIO_NET_HDR_GSO_TCPV6:
			tcp_hdr = l4_hdr;
			m->ol_flags |= PKT_TX_TCP_SEG;
			m->tso_segsz = hdr->gso_size;
			m->l4_len = (tcp_hdr->data_off & 0xf0) >> 2;
			break;
		case VIRTIO_NET_HDR_GSO_UDP:
			m->ol_flags |= PKT_TX_UDP_SEG;
			m->tso_segsz = hdr->gso_size;
			m->l4_len = sizeof(struct rte_udp_hdr);
			break;
		default:
			VHOST_LOG_DATA(WARNING,
				"unsupported gso type %u.\n", hdr->gso_type);
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
		  struct rte_mbuf *m, struct rte_mempool *mbuf_pool)
{
	uint32_t buf_avail, buf_offset;
	uint64_t buf_addr, buf_iova, buf_len;
	uint32_t mbuf_avail, mbuf_offset;
	uint32_t cpy_len;
	struct rte_mbuf *cur = m, *prev = m;
	struct virtio_net_hdr tmp_hdr;
	struct virtio_net_hdr *hdr = NULL;
	/* A counter to avoid desc dead loop chain */
	uint16_t vec_idx = 0;
	struct batch_copy_elem *batch_copy = vq->batch_copy_elems;
	int error = 0;

	buf_addr = buf_vec[vec_idx].buf_addr;
	buf_iova = buf_vec[vec_idx].buf_iova;
	buf_len = buf_vec[vec_idx].buf_len;

	if (unlikely(buf_len < dev->vhost_hlen && nr_vec <= 1)) {
		error = -1;
		goto out;
	}

	if (virtio_net_with_host_offload(dev)) {
		if (unlikely(buf_len < sizeof(struct virtio_net_hdr))) {
			/*
			 * No luck, the virtio-net header doesn't fit
			 * in a contiguous virtual area.
			 */
			copy_vnet_hdr_from_desc(&tmp_hdr, buf_vec);
			hdr = &tmp_hdr;
		} else {
			hdr = (struct virtio_net_hdr *)((uintptr_t)buf_addr);
		}
	}

	/*
	 * A virtio driver normally uses at least 2 desc buffers
	 * for Tx: the first for storing the header, and others
	 * for storing the data.
	 */
	if (unlikely(buf_len < dev->vhost_hlen)) {
		buf_offset = dev->vhost_hlen - buf_len;
		vec_idx++;
		buf_addr = buf_vec[vec_idx].buf_addr;
		buf_iova = buf_vec[vec_idx].buf_iova;
		buf_len = buf_vec[vec_idx].buf_len;
		buf_avail  = buf_len - buf_offset;
	} else if (buf_len == dev->vhost_hlen) {
		if (unlikely(++vec_idx >= nr_vec))
			goto out;
		buf_addr = buf_vec[vec_idx].buf_addr;
		buf_iova = buf_vec[vec_idx].buf_iova;
		buf_len = buf_vec[vec_idx].buf_len;

		buf_offset = 0;
		buf_avail = buf_len;
	} else {
		buf_offset = dev->vhost_hlen;
		buf_avail = buf_vec[vec_idx].buf_len - dev->vhost_hlen;
	}

	PRINT_PACKET(dev,
			(uintptr_t)(buf_addr + buf_offset),
			(uint32_t)buf_avail, 0);

	mbuf_offset = 0;
	mbuf_avail  = m->buf_len - RTE_PKTMBUF_HEADROOM;
	while (1) {
		uint64_t hpa;

		cpy_len = RTE_MIN(buf_avail, mbuf_avail);

		/*
		 * A desc buf might across two host physical pages that are
		 * not continuous. In such case (gpa_to_hpa returns 0), data
		 * will be copied even though zero copy is enabled.
		 */
		if (unlikely(dev->dequeue_zero_copy && (hpa = gpa_to_hpa(dev,
					buf_iova + buf_offset, cpy_len)))) {
			cur->data_len = cpy_len;
			cur->data_off = 0;
			cur->buf_addr =
				(void *)(uintptr_t)(buf_addr + buf_offset);
			cur->buf_iova = hpa;

			/*
			 * In zero copy mode, one mbuf can only reference data
			 * for one or partial of one desc buff.
			 */
			mbuf_avail = cpy_len;
		} else {
			if (likely(cpy_len > MAX_BATCH_LEN ||
				   vq->batch_copy_nb_elems >= vq->size ||
				   (hdr && cur == m))) {
				rte_memcpy(rte_pktmbuf_mtod_offset(cur, void *,
								   mbuf_offset),
					   (void *)((uintptr_t)(buf_addr +
							   buf_offset)),
					   cpy_len);
			} else {
				batch_copy[vq->batch_copy_nb_elems].dst =
					rte_pktmbuf_mtod_offset(cur, void *,
								mbuf_offset);
				batch_copy[vq->batch_copy_nb_elems].src =
					(void *)((uintptr_t)(buf_addr +
								buf_offset));
				batch_copy[vq->batch_copy_nb_elems].len =
					cpy_len;
				vq->batch_copy_nb_elems++;
			}
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
			buf_iova = buf_vec[vec_idx].buf_iova;
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
			if (unlikely(dev->dequeue_zero_copy))
				rte_mbuf_refcnt_update(cur, 1);

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
		vhost_dequeue_offload(hdr, m);

out:

	return error;
}

static __rte_always_inline struct zcopy_mbuf *
get_zmbuf(struct vhost_virtqueue *vq)
{
	uint16_t i;
	uint16_t last;
	int tries = 0;

	/* search [last_zmbuf_idx, zmbuf_size) */
	i = vq->last_zmbuf_idx;
	last = vq->zmbuf_size;

again:
	for (; i < last; i++) {
		if (vq->zmbufs[i].in_use == 0) {
			vq->last_zmbuf_idx = i + 1;
			vq->zmbufs[i].in_use = 1;
			return &vq->zmbufs[i];
		}
	}

	tries++;
	if (tries == 1) {
		/* search [0, last_zmbuf_idx) */
		i = 0;
		last = vq->last_zmbuf_idx;
		goto again;
	}

	return NULL;
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

	/* Try to use pkt buffer to store shinfo to reduce the amount of memory
	 * required, otherwise store shinfo in the new buffer.
	 */
	if (rte_pktmbuf_tailroom(pkt) >= sizeof(*shinfo))
		shinfo = rte_pktmbuf_mtod(pkt,
					  struct rte_mbuf_ext_shared_info *);
	else {
		total_len += sizeof(*shinfo) + sizeof(uintptr_t);
		total_len = RTE_ALIGN_CEIL(total_len, sizeof(uintptr_t));
	}

	if (unlikely(total_len > UINT16_MAX))
		return -ENOSPC;

	buf_len = total_len;
	buf = rte_malloc(NULL, buf_len, RTE_CACHE_LINE_SIZE);
	if (unlikely(buf == NULL))
		return -ENOMEM;

	/* Initialize shinfo */
	if (shinfo) {
		shinfo->free_cb = virtio_dev_extbuf_free;
		shinfo->fcb_opaque = buf;
		rte_mbuf_ext_refcnt_set(shinfo, 1);
	} else {
		shinfo = rte_pktmbuf_ext_shinfo_init_helper(buf, &buf_len,
					      virtio_dev_extbuf_free, buf);
		if (unlikely(shinfo == NULL)) {
			rte_free(buf);
			VHOST_LOG_DATA(ERR, "Failed to init shinfo\n");
			return -1;
		}
	}

	iova = rte_malloc_virt2iova(buf);
	rte_pktmbuf_attach_extbuf(pkt, buf, iova, buf_len, shinfo);
	rte_pktmbuf_reset_headroom(pkt);

	return 0;
}

/*
 * Allocate a host supported pktmbuf.
 */
static __rte_always_inline struct rte_mbuf *
virtio_dev_pktmbuf_alloc(struct virtio_net *dev, struct rte_mempool *mp,
			 uint32_t data_len)
{
	struct rte_mbuf *pkt = rte_pktmbuf_alloc(mp);

	if (unlikely(pkt == NULL)) {
		VHOST_LOG_DATA(ERR,
			"Failed to allocate memory for mbuf.\n");
		return NULL;
	}

	if (rte_pktmbuf_tailroom(pkt) >= data_len)
		return pkt;

	/* attach an external buffer if supported */
	if (dev->extbuf && !virtio_dev_extbuf_alloc(pkt, data_len))
		return pkt;

	/* check if chained buffers are allowed */
	if (!dev->linearbuf)
		return pkt;

	/* Data doesn't fit into the buffer and the host supports
	 * only linear buffers
	 */
	rte_pktmbuf_free(pkt);

	return NULL;
}

static __rte_noinline uint16_t
virtio_dev_tx_split(struct virtio_net *dev, struct vhost_virtqueue *vq,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
{
	uint16_t i;
	uint16_t free_entries;

	if (unlikely(dev->dequeue_zero_copy)) {
		struct zcopy_mbuf *zmbuf, *next;

		for (zmbuf = TAILQ_FIRST(&vq->zmbuf_list);
		     zmbuf != NULL; zmbuf = next) {
			next = TAILQ_NEXT(zmbuf, next);

			if (mbuf_is_consumed(zmbuf->mbuf)) {
				update_shadow_used_ring_split(vq,
						zmbuf->desc_idx, 0);
				TAILQ_REMOVE(&vq->zmbuf_list, zmbuf, next);
				restore_mbuf(zmbuf->mbuf);
				rte_pktmbuf_free(zmbuf->mbuf);
				put_zmbuf(zmbuf);
				vq->nr_zmbuf -= 1;
			}
		}

		if (likely(vq->shadow_used_idx)) {
			flush_shadow_used_ring_split(dev, vq);
			vhost_vring_call_split(dev, vq);
		}
	}

	free_entries = *((volatile uint16_t *)&vq->avail->idx) -
			vq->last_avail_idx;
	if (free_entries == 0)
		return 0;

	/*
	 * The ordering between avail index and
	 * desc reads needs to be enforced.
	 */
	rte_smp_rmb();

	rte_prefetch0(&vq->avail->ring[vq->last_avail_idx & (vq->size - 1)]);

	VHOST_LOG_DATA(DEBUG, "(%d) %s\n", dev->vid, __func__);

	count = RTE_MIN(count, MAX_PKT_BURST);
	count = RTE_MIN(count, free_entries);
	VHOST_LOG_DATA(DEBUG, "(%d) about to dequeue %u buffers\n",
			dev->vid, count);

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

		if (likely(dev->dequeue_zero_copy == 0))
			update_shadow_used_ring_split(vq, head_idx, 0);

		pkts[i] = virtio_dev_pktmbuf_alloc(dev, mbuf_pool, buf_len);
		if (unlikely(pkts[i] == NULL))
			break;

		err = copy_desc_to_mbuf(dev, vq, buf_vec, nr_vec, pkts[i],
				mbuf_pool);
		if (unlikely(err)) {
			rte_pktmbuf_free(pkts[i]);
			break;
		}

		if (unlikely(dev->dequeue_zero_copy)) {
			struct zcopy_mbuf *zmbuf;

			zmbuf = get_zmbuf(vq);
			if (!zmbuf) {
				rte_pktmbuf_free(pkts[i]);
				break;
			}
			zmbuf->mbuf = pkts[i];
			zmbuf->desc_idx = head_idx;

			/*
			 * Pin lock the mbuf; we will check later to see
			 * whether the mbuf is freed (when we are the last
			 * user) or not. If that's the case, we then could
			 * update the used ring safely.
			 */
			rte_mbuf_refcnt_update(pkts[i], 1);

			vq->nr_zmbuf += 1;
			TAILQ_INSERT_TAIL(&vq->zmbuf_list, zmbuf, next);
		}
	}
	vq->last_avail_idx += i;

	if (likely(dev->dequeue_zero_copy == 0)) {
		do_data_copy_dequeue(vq);
		if (unlikely(i < count))
			vq->shadow_used_idx = i;
		if (likely(vq->shadow_used_idx)) {
			flush_shadow_used_ring_split(dev, vq);
			vhost_vring_call_split(dev, vq);
		}
	}

	return i;
}

static __rte_always_inline int
vhost_reserve_avail_batch_packed(struct virtio_net *dev,
				 struct vhost_virtqueue *vq,
				 struct rte_mempool *mbuf_pool,
				 struct rte_mbuf **pkts,
				 uint16_t avail_idx,
				 uintptr_t *desc_addrs,
				 uint16_t *ids)
{
	bool wrap = vq->avail_wrap_counter;
	struct vring_packed_desc *descs = vq->desc_packed;
	struct virtio_net_hdr *hdr;
	uint64_t lens[PACKED_BATCH_SIZE];
	uint64_t buf_lens[PACKED_BATCH_SIZE];
	uint32_t buf_offset = dev->vhost_hlen;
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

	rte_smp_rmb();

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		lens[i] = descs[avail_idx + i].len;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		desc_addrs[i] = vhost_iova_to_vva(dev, vq,
						  descs[avail_idx + i].addr,
						  &lens[i], VHOST_ACCESS_RW);
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (unlikely((lens[i] != descs[avail_idx + i].len)))
			return -1;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		pkts[i] = virtio_dev_pktmbuf_alloc(dev, mbuf_pool, lens[i]);
		if (!pkts[i])
			goto free_buf;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		buf_lens[i] = pkts[i]->buf_len - pkts[i]->data_off;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (unlikely(buf_lens[i] < (lens[i] - buf_offset)))
			goto free_buf;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		pkts[i]->pkt_len = descs[avail_idx + i].len - buf_offset;
		pkts[i]->data_len = pkts[i]->pkt_len;
		ids[i] = descs[avail_idx + i].id;
	}

	if (virtio_net_with_host_offload(dev)) {
		vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
			hdr = (struct virtio_net_hdr *)(desc_addrs[i]);
			vhost_dequeue_offload(hdr, pkts[i]);
		}
	}

	return 0;

free_buf:
	for (i = 0; i < PACKED_BATCH_SIZE; i++)
		rte_pktmbuf_free(pkts[i]);

	return -1;
}

static __rte_always_inline int
virtio_dev_tx_batch_packed(struct virtio_net *dev,
			   struct vhost_virtqueue *vq,
			   struct rte_mempool *mbuf_pool,
			   struct rte_mbuf **pkts)
{
	uint16_t avail_idx = vq->last_avail_idx;
	uint32_t buf_offset = dev->vhost_hlen;
	uintptr_t desc_addrs[PACKED_BATCH_SIZE];
	uint16_t ids[PACKED_BATCH_SIZE];
	uint16_t i;

	if (vhost_reserve_avail_batch_packed(dev, vq, mbuf_pool, pkts,
					     avail_idx, desc_addrs, ids))
		return -1;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		rte_prefetch0((void *)(uintptr_t)desc_addrs[i]);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		rte_memcpy(rte_pktmbuf_mtod_offset(pkts[i], void *, 0),
			   (void *)(uintptr_t)(desc_addrs[i] + buf_offset),
			   pkts[i]->pkt_len);

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
			    struct rte_mbuf **pkts,
			    uint16_t *buf_id,
			    uint16_t *desc_count)
{
	struct buf_vector buf_vec[BUF_VECTOR_MAX];
	uint32_t buf_len;
	uint16_t nr_vec = 0;
	int err;

	if (unlikely(fill_vec_buf_packed(dev, vq,
					 vq->last_avail_idx, desc_count,
					 buf_vec, &nr_vec,
					 buf_id, &buf_len,
					 VHOST_ACCESS_RO) < 0))
		return -1;

	*pkts = virtio_dev_pktmbuf_alloc(dev, mbuf_pool, buf_len);
	if (unlikely(*pkts == NULL)) {
		VHOST_LOG_DATA(ERR,
			"Failed to allocate memory for mbuf.\n");
		return -1;
	}

	err = copy_desc_to_mbuf(dev, vq, buf_vec, nr_vec, *pkts,
				mbuf_pool);
	if (unlikely(err)) {
		rte_pktmbuf_free(*pkts);
		return -1;
	}

	return 0;
}

static __rte_always_inline int
virtio_dev_tx_single_packed(struct virtio_net *dev,
			    struct vhost_virtqueue *vq,
			    struct rte_mempool *mbuf_pool,
			    struct rte_mbuf **pkts)
{

	uint16_t buf_id, desc_count;

	if (vhost_dequeue_single_packed(dev, vq, mbuf_pool, pkts, &buf_id,
					&desc_count))
		return -1;

	if (virtio_net_is_inorder(dev))
		vhost_shadow_dequeue_single_packed_inorder(vq, buf_id,
							   desc_count);
	else
		vhost_shadow_dequeue_single_packed(vq, buf_id, desc_count);

	vq_inc_last_avail_packed(vq, desc_count);

	return 0;
}

static __rte_always_inline int
virtio_dev_tx_batch_packed_zmbuf(struct virtio_net *dev,
				 struct vhost_virtqueue *vq,
				 struct rte_mempool *mbuf_pool,
				 struct rte_mbuf **pkts)
{
	struct zcopy_mbuf *zmbufs[PACKED_BATCH_SIZE];
	uintptr_t desc_addrs[PACKED_BATCH_SIZE];
	uint16_t ids[PACKED_BATCH_SIZE];
	uint16_t i;

	uint16_t avail_idx = vq->last_avail_idx;

	if (vhost_reserve_avail_batch_packed(dev, vq, mbuf_pool, pkts,
					     avail_idx, desc_addrs, ids))
		return -1;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		zmbufs[i] = get_zmbuf(vq);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (!zmbufs[i])
			goto free_pkt;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		zmbufs[i]->mbuf = pkts[i];
		zmbufs[i]->desc_idx = avail_idx + i;
		zmbufs[i]->desc_count = 1;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		rte_mbuf_refcnt_update(pkts[i], 1);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		TAILQ_INSERT_TAIL(&vq->zmbuf_list, zmbufs[i], next);

	vq->nr_zmbuf += PACKED_BATCH_SIZE;
	vq_inc_last_avail_packed(vq, PACKED_BATCH_SIZE);

	return 0;

free_pkt:
	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		rte_pktmbuf_free(pkts[i]);

	return -1;
}

static __rte_always_inline int
virtio_dev_tx_single_packed_zmbuf(struct virtio_net *dev,
				  struct vhost_virtqueue *vq,
				  struct rte_mempool *mbuf_pool,
				  struct rte_mbuf **pkts)
{
	uint16_t buf_id, desc_count;
	struct zcopy_mbuf *zmbuf;

	if (vhost_dequeue_single_packed(dev, vq, mbuf_pool, pkts, &buf_id,
					&desc_count))
		return -1;

	zmbuf = get_zmbuf(vq);
	if (!zmbuf) {
		rte_pktmbuf_free(*pkts);
		return -1;
	}
	zmbuf->mbuf = *pkts;
	zmbuf->desc_idx = vq->last_avail_idx;
	zmbuf->desc_count = desc_count;

	rte_mbuf_refcnt_update(*pkts, 1);

	vq->nr_zmbuf += 1;
	TAILQ_INSERT_TAIL(&vq->zmbuf_list, zmbuf, next);

	vq_inc_last_avail_packed(vq, desc_count);
	return 0;
}

static __rte_always_inline void
free_zmbuf(struct vhost_virtqueue *vq)
{
	struct zcopy_mbuf *next = NULL;
	struct zcopy_mbuf *zmbuf;

	for (zmbuf = TAILQ_FIRST(&vq->zmbuf_list);
	     zmbuf != NULL; zmbuf = next) {
		next = TAILQ_NEXT(zmbuf, next);

		uint16_t last_used_idx = vq->last_used_idx;

		if (mbuf_is_consumed(zmbuf->mbuf)) {
			uint16_t flags;
			flags = vq->desc_packed[last_used_idx].flags;
			if (vq->used_wrap_counter) {
				flags |= VRING_DESC_F_USED;
				flags |= VRING_DESC_F_AVAIL;
			} else {
				flags &= ~VRING_DESC_F_USED;
				flags &= ~VRING_DESC_F_AVAIL;
			}

			vq->desc_packed[last_used_idx].id = zmbuf->desc_idx;
			vq->desc_packed[last_used_idx].len = 0;

			rte_smp_wmb();
			vq->desc_packed[last_used_idx].flags = flags;

			vq_inc_last_used_packed(vq, zmbuf->desc_count);

			TAILQ_REMOVE(&vq->zmbuf_list, zmbuf, next);
			restore_mbuf(zmbuf->mbuf);
			rte_pktmbuf_free(zmbuf->mbuf);
			put_zmbuf(zmbuf);
			vq->nr_zmbuf -= 1;
		}
	}
}

static __rte_noinline uint16_t
virtio_dev_tx_packed_zmbuf(struct virtio_net *dev,
			   struct vhost_virtqueue *vq,
			   struct rte_mempool *mbuf_pool,
			   struct rte_mbuf **pkts,
			   uint32_t count)
{
	uint32_t pkt_idx = 0;
	uint32_t remained = count;

	free_zmbuf(vq);

	do {
		if (remained >= PACKED_BATCH_SIZE) {
			if (!virtio_dev_tx_batch_packed_zmbuf(dev, vq,
				mbuf_pool, &pkts[pkt_idx])) {
				pkt_idx += PACKED_BATCH_SIZE;
				remained -= PACKED_BATCH_SIZE;
				continue;
			}
		}

		if (virtio_dev_tx_single_packed_zmbuf(dev, vq, mbuf_pool,
						      &pkts[pkt_idx]))
			break;
		pkt_idx++;
		remained--;

	} while (remained);

	if (pkt_idx)
		vhost_vring_call_packed(dev, vq);

	return pkt_idx;
}

static __rte_always_inline bool
next_desc_is_avail(const struct vhost_virtqueue *vq)
{
	bool wrap_counter = vq->avail_wrap_counter;
	uint16_t next_used_idx = vq->last_used_idx + 1;

	if (next_used_idx >= vq->size) {
		next_used_idx -= vq->size;
		wrap_counter ^= 1;
	}

	return desc_is_avail(&vq->desc_packed[next_used_idx], wrap_counter);
}

static __rte_noinline uint16_t
virtio_dev_tx_packed(struct virtio_net *dev,
		     struct vhost_virtqueue *vq,
		     struct rte_mempool *mbuf_pool,
		     struct rte_mbuf **pkts,
		     uint32_t count)
{
	uint32_t pkt_idx = 0;
	uint32_t remained = count;

	do {
		rte_prefetch0(&vq->desc_packed[vq->last_avail_idx]);

		if (remained >= PACKED_BATCH_SIZE) {
			if (!virtio_dev_tx_batch_packed(dev, vq, mbuf_pool,
							&pkts[pkt_idx])) {
				vhost_flush_dequeue_packed(dev, vq);
				pkt_idx += PACKED_BATCH_SIZE;
				remained -= PACKED_BATCH_SIZE;
				continue;
			}
		}

		if (virtio_dev_tx_single_packed(dev, vq, mbuf_pool,
						&pkts[pkt_idx]))
			break;
		vhost_flush_dequeue_packed(dev, vq);
		pkt_idx++;
		remained--;

	} while (remained);

	if (vq->shadow_used_idx) {
		do_data_copy_dequeue(vq);

		if (remained && !next_desc_is_avail(vq)) {
			/*
			 * The guest may be waiting to TX some buffers to
			 * enqueue more to avoid bufferfloat, so we try to
			 * reduce latency here.
			 */
			vhost_flush_dequeue_shadow_packed(dev, vq);
			vhost_vring_call_packed(dev, vq);
		}
	}

	return pkt_idx;
}

uint16_t
rte_vhost_dequeue_burst(int vid, uint16_t queue_id,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
{
	struct virtio_net *dev;
	struct rte_mbuf *rarp_mbuf = NULL;
	struct vhost_virtqueue *vq;

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

	if (unlikely(vq->enabled == 0)) {
		count = 0;
		goto out_access_unlock;
	}

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_lock(vq);

	if (unlikely(vq->access_ok == 0))
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
	 * rte_atomic16_cmpset() causes a write if using cmpxchg. This could
	 * result in false sharing between enqueue and dequeue.
	 *
	 * Prevent unnecessary false sharing by reading broadcast_rarp first
	 * and only performing cmpset if the read indicates it is likely to
	 * be set.
	 */
	if (unlikely(rte_atomic16_read(&dev->broadcast_rarp) &&
			rte_atomic16_cmpset((volatile uint16_t *)
				&dev->broadcast_rarp.cnt, 1, 0))) {

		rarp_mbuf = rte_net_make_rarp_packet(mbuf_pool, &dev->mac);
		if (rarp_mbuf == NULL) {
			VHOST_LOG_DATA(ERR, "Failed to make RARP packet.\n");
			count = 0;
			goto out;
		}
		count -= 1;
	}

	if (vq_is_packed(dev)) {
		if (unlikely(dev->dequeue_zero_copy))
			count = virtio_dev_tx_packed_zmbuf(dev, vq, mbuf_pool,
							   pkts, count);
		else
			count = virtio_dev_tx_packed(dev, vq, mbuf_pool, pkts,
						     count);
	} else
		count = virtio_dev_tx_split(dev, vq, mbuf_pool, pkts, count);

out:
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_unlock(vq);

out_access_unlock:
	rte_spinlock_unlock(&vq->access_lock);

	if (unlikely(rarp_mbuf != NULL)) {
		/*
		 * Inject it to the head of "pkts" array, so that switch's mac
		 * learning table will get updated first.
		 */
		memmove(&pkts[1], pkts, count * sizeof(struct rte_mbuf *));
		pkts[0] = rarp_mbuf;
		count += 1;
	}

	return count;
}
