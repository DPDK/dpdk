/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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

#include "vhost.h"

#define MAX_PKT_BURST 32

static bool
is_valid_virt_queue_idx(uint32_t idx, int is_tx, uint32_t nr_vring)
{
	return (is_tx ^ (idx & 1)) == 0 && idx < nr_vring;
}

static __rte_always_inline void
do_flush_shadow_used_ring(struct virtio_net *dev, struct vhost_virtqueue *vq,
			  uint16_t to, uint16_t from, uint16_t size)
{
	rte_memcpy(&vq->used->ring[to],
			&vq->shadow_used_ring[from],
			size * sizeof(struct vring_used_elem));
	vhost_log_used_vring(dev, vq,
			offsetof(struct vring_used, ring[to]),
			size * sizeof(struct vring_used_elem));
}

static __rte_always_inline void
flush_shadow_used_ring(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	uint16_t used_idx = vq->last_used_idx & (vq->size - 1);

	if (used_idx + vq->shadow_used_idx <= vq->size) {
		do_flush_shadow_used_ring(dev, vq, used_idx, 0,
					  vq->shadow_used_idx);
	} else {
		uint16_t size;

		/* update used ring interval [used_idx, vq->size] */
		size = vq->size - used_idx;
		do_flush_shadow_used_ring(dev, vq, used_idx, 0, size);

		/* update the left half used ring interval [0, left_size] */
		do_flush_shadow_used_ring(dev, vq, 0, size,
					  vq->shadow_used_idx - size);
	}
	vq->last_used_idx += vq->shadow_used_idx;

	rte_smp_wmb();

	*(volatile uint16_t *)&vq->used->idx += vq->shadow_used_idx;
	vhost_log_used_vring(dev, vq, offsetof(struct vring_used, idx),
		sizeof(vq->used->idx));
}

static __rte_always_inline void
update_shadow_used_ring(struct vhost_virtqueue *vq,
			 uint16_t desc_idx, uint16_t len)
{
	uint16_t i = vq->shadow_used_idx++;

	vq->shadow_used_ring[i].id  = desc_idx;
	vq->shadow_used_ring[i].len = len;
}

/* avoid write operation when necessary, to lessen cache issues */
#define ASSIGN_UNLESS_EQUAL(var, val) do {	\
	if ((var) != (val))			\
		(var) = (val);			\
} while (0)

static void
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
			net_hdr->csum_offset = (offsetof(struct tcp_hdr,
						cksum));
			break;
		case PKT_TX_UDP_CKSUM:
			net_hdr->csum_offset = (offsetof(struct udp_hdr,
						dgram_cksum));
			break;
		case PKT_TX_SCTP_CKSUM:
			net_hdr->csum_offset = (offsetof(struct sctp_hdr,
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
		struct ipv4_hdr *ipv4_hdr;

		ipv4_hdr = rte_pktmbuf_mtod_offset(m_buf, struct ipv4_hdr *,
						   m_buf->l2_len);
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
	} else {
		ASSIGN_UNLESS_EQUAL(net_hdr->gso_type, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->gso_size, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->hdr_len, 0);
	}
}

static __rte_always_inline int
copy_mbuf_to_desc(struct virtio_net *dev, struct vring_desc *descs,
		  struct rte_mbuf *m, uint16_t desc_idx, uint32_t size)
{
	uint32_t desc_avail, desc_offset;
	uint32_t mbuf_avail, mbuf_offset;
	uint32_t cpy_len;
	struct vring_desc *desc;
	uint64_t desc_addr;
	/* A counter to avoid desc dead loop chain */
	uint16_t nr_desc = 1;

	desc = &descs[desc_idx];
	desc_addr = rte_vhost_gpa_to_vva(dev->mem, desc->addr);
	/*
	 * Checking of 'desc_addr' placed outside of 'unlikely' macro to avoid
	 * performance issue with some versions of gcc (4.8.4 and 5.3.0) which
	 * otherwise stores offset on the stack instead of in a register.
	 */
	if (unlikely(desc->len < dev->vhost_hlen) || !desc_addr)
		return -1;

	rte_prefetch0((void *)(uintptr_t)desc_addr);

	virtio_enqueue_offload(m, (struct virtio_net_hdr *)(uintptr_t)desc_addr);
	vhost_log_write(dev, desc->addr, dev->vhost_hlen);
	PRINT_PACKET(dev, (uintptr_t)desc_addr, dev->vhost_hlen, 0);

	desc_offset = dev->vhost_hlen;
	desc_avail  = desc->len - dev->vhost_hlen;

	mbuf_avail  = rte_pktmbuf_data_len(m);
	mbuf_offset = 0;
	while (mbuf_avail != 0 || m->next != NULL) {
		/* done with current mbuf, fetch next */
		if (mbuf_avail == 0) {
			m = m->next;

			mbuf_offset = 0;
			mbuf_avail  = rte_pktmbuf_data_len(m);
		}

		/* done with current desc buf, fetch next */
		if (desc_avail == 0) {
			if ((desc->flags & VRING_DESC_F_NEXT) == 0) {
				/* Room in vring buffer is not enough */
				return -1;
			}
			if (unlikely(desc->next >= size || ++nr_desc > size))
				return -1;

			desc = &descs[desc->next];
			desc_addr = rte_vhost_gpa_to_vva(dev->mem, desc->addr);
			if (unlikely(!desc_addr))
				return -1;

			desc_offset = 0;
			desc_avail  = desc->len;
		}

		cpy_len = RTE_MIN(desc_avail, mbuf_avail);
		rte_memcpy((void *)((uintptr_t)(desc_addr + desc_offset)),
			rte_pktmbuf_mtod_offset(m, void *, mbuf_offset),
			cpy_len);
		vhost_log_write(dev, desc->addr + desc_offset, cpy_len);
		PRINT_PACKET(dev, (uintptr_t)(desc_addr + desc_offset),
			     cpy_len, 0);

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		desc_avail  -= cpy_len;
		desc_offset += cpy_len;
	}

	return 0;
}

/**
 * This function adds buffers to the virtio devices RX virtqueue. Buffers can
 * be received from the physical port or from another virtio device. A packet
 * count is returned to indicate the number of packets that are successfully
 * added to the RX queue. This function works when the mbuf is scattered, but
 * it doesn't support the mergeable feature.
 */
static __rte_always_inline uint32_t
virtio_dev_rx(struct virtio_net *dev, uint16_t queue_id,
	      struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_virtqueue *vq;
	uint16_t avail_idx, free_entries, start_idx;
	uint16_t desc_indexes[MAX_PKT_BURST];
	struct vring_desc *descs;
	uint16_t used_idx;
	uint32_t i, sz;

	LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->nr_vring))) {
		RTE_LOG(ERR, VHOST_DATA, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];
	if (unlikely(vq->enabled == 0))
		return 0;

	avail_idx = *((volatile uint16_t *)&vq->avail->idx);
	start_idx = vq->last_used_idx;
	free_entries = avail_idx - start_idx;
	count = RTE_MIN(count, free_entries);
	count = RTE_MIN(count, (uint32_t)MAX_PKT_BURST);
	if (count == 0)
		return 0;

	LOG_DEBUG(VHOST_DATA, "(%d) start_idx %d | end_idx %d\n",
		dev->vid, start_idx, start_idx + count);

	/* Retrieve all of the desc indexes first to avoid caching issues. */
	rte_prefetch0(&vq->avail->ring[start_idx & (vq->size - 1)]);
	for (i = 0; i < count; i++) {
		used_idx = (start_idx + i) & (vq->size - 1);
		desc_indexes[i] = vq->avail->ring[used_idx];
		vq->used->ring[used_idx].id = desc_indexes[i];
		vq->used->ring[used_idx].len = pkts[i]->pkt_len +
					       dev->vhost_hlen;
		vhost_log_used_vring(dev, vq,
			offsetof(struct vring_used, ring[used_idx]),
			sizeof(vq->used->ring[used_idx]));
	}

	rte_prefetch0(&vq->desc[desc_indexes[0]]);
	for (i = 0; i < count; i++) {
		uint16_t desc_idx = desc_indexes[i];
		int err;

		if (vq->desc[desc_idx].flags & VRING_DESC_F_INDIRECT) {
			descs = (struct vring_desc *)(uintptr_t)
				rte_vhost_gpa_to_vva(dev->mem,
					vq->desc[desc_idx].addr);
			if (unlikely(!descs)) {
				count = i;
				break;
			}

			desc_idx = 0;
			sz = vq->desc[desc_idx].len / sizeof(*descs);
		} else {
			descs = vq->desc;
			sz = vq->size;
		}

		err = copy_mbuf_to_desc(dev, descs, pkts[i], desc_idx, sz);
		if (unlikely(err)) {
			used_idx = (start_idx + i) & (vq->size - 1);
			vq->used->ring[used_idx].len = dev->vhost_hlen;
			vhost_log_used_vring(dev, vq,
				offsetof(struct vring_used, ring[used_idx]),
				sizeof(vq->used->ring[used_idx]));
		}

		if (i + 1 < count)
			rte_prefetch0(&vq->desc[desc_indexes[i+1]]);
	}

	rte_smp_wmb();

	*(volatile uint16_t *)&vq->used->idx += count;
	vq->last_used_idx += count;
	vhost_log_used_vring(dev, vq,
		offsetof(struct vring_used, idx),
		sizeof(vq->used->idx));

	/* flush used->idx update before we read avail->flags. */
	rte_mb();

	/* Kick the guest if necessary. */
	if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT)
			&& (vq->callfd >= 0))
		eventfd_write(vq->callfd, (eventfd_t)1);
	return count;
}

static __rte_always_inline int
fill_vec_buf(struct virtio_net *dev, struct vhost_virtqueue *vq,
			 uint32_t avail_idx, uint32_t *vec_idx,
			 struct buf_vector *buf_vec, uint16_t *desc_chain_head,
			 uint16_t *desc_chain_len)
{
	uint16_t idx = vq->avail->ring[avail_idx & (vq->size - 1)];
	uint32_t vec_id = *vec_idx;
	uint32_t len    = 0;
	struct vring_desc *descs = vq->desc;

	*desc_chain_head = idx;

	if (vq->desc[idx].flags & VRING_DESC_F_INDIRECT) {
		descs = (struct vring_desc *)(uintptr_t)
			rte_vhost_gpa_to_vva(dev->mem, vq->desc[idx].addr);
		if (unlikely(!descs))
			return -1;

		idx = 0;
	}

	while (1) {
		if (unlikely(vec_id >= BUF_VECTOR_MAX || idx >= vq->size))
			return -1;

		len += descs[idx].len;
		buf_vec[vec_id].buf_addr = descs[idx].addr;
		buf_vec[vec_id].buf_len  = descs[idx].len;
		buf_vec[vec_id].desc_idx = idx;
		vec_id++;

		if ((descs[idx].flags & VRING_DESC_F_NEXT) == 0)
			break;

		idx = descs[idx].next;
	}

	*desc_chain_len = len;
	*vec_idx = vec_id;

	return 0;
}

/*
 * Returns -1 on fail, 0 on success
 */
static inline int
reserve_avail_buf_mergeable(struct virtio_net *dev, struct vhost_virtqueue *vq,
				uint32_t size, struct buf_vector *buf_vec,
				uint16_t *num_buffers, uint16_t avail_head)
{
	uint16_t cur_idx;
	uint32_t vec_idx = 0;
	uint16_t tries = 0;

	uint16_t head_idx = 0;
	uint16_t len = 0;

	*num_buffers = 0;
	cur_idx  = vq->last_avail_idx;

	while (size > 0) {
		if (unlikely(cur_idx == avail_head))
			return -1;

		if (unlikely(fill_vec_buf(dev, vq, cur_idx, &vec_idx, buf_vec,
						&head_idx, &len) < 0))
			return -1;
		len = RTE_MIN(len, size);
		update_shadow_used_ring(vq, head_idx, len);
		size -= len;

		cur_idx++;
		tries++;
		*num_buffers += 1;

		/*
		 * if we tried all available ring items, and still
		 * can't get enough buf, it means something abnormal
		 * happened.
		 */
		if (unlikely(tries >= vq->size))
			return -1;
	}

	return 0;
}

static __rte_always_inline int
copy_mbuf_to_desc_mergeable(struct virtio_net *dev, struct rte_mbuf *m,
			    struct buf_vector *buf_vec, uint16_t num_buffers)
{
	uint32_t vec_idx = 0;
	uint64_t desc_addr;
	uint32_t mbuf_offset, mbuf_avail;
	uint32_t desc_offset, desc_avail;
	uint32_t cpy_len;
	uint64_t hdr_addr, hdr_phys_addr;
	struct rte_mbuf *hdr_mbuf;

	if (unlikely(m == NULL))
		return -1;

	desc_addr = rte_vhost_gpa_to_vva(dev->mem, buf_vec[vec_idx].buf_addr);
	if (buf_vec[vec_idx].buf_len < dev->vhost_hlen || !desc_addr)
		return -1;

	hdr_mbuf = m;
	hdr_addr = desc_addr;
	hdr_phys_addr = buf_vec[vec_idx].buf_addr;
	rte_prefetch0((void *)(uintptr_t)hdr_addr);

	LOG_DEBUG(VHOST_DATA, "(%d) RX: num merge buffers %d\n",
		dev->vid, num_buffers);

	desc_avail  = buf_vec[vec_idx].buf_len - dev->vhost_hlen;
	desc_offset = dev->vhost_hlen;

	mbuf_avail  = rte_pktmbuf_data_len(m);
	mbuf_offset = 0;
	while (mbuf_avail != 0 || m->next != NULL) {
		/* done with current desc buf, get the next one */
		if (desc_avail == 0) {
			vec_idx++;
			desc_addr = rte_vhost_gpa_to_vva(dev->mem,
					buf_vec[vec_idx].buf_addr);
			if (unlikely(!desc_addr))
				return -1;

			/* Prefetch buffer address. */
			rte_prefetch0((void *)(uintptr_t)desc_addr);
			desc_offset = 0;
			desc_avail  = buf_vec[vec_idx].buf_len;
		}

		/* done with current mbuf, get the next one */
		if (mbuf_avail == 0) {
			m = m->next;

			mbuf_offset = 0;
			mbuf_avail  = rte_pktmbuf_data_len(m);
		}

		if (hdr_addr) {
			struct virtio_net_hdr_mrg_rxbuf *hdr;

			hdr = (struct virtio_net_hdr_mrg_rxbuf *)(uintptr_t)
				hdr_addr;
			virtio_enqueue_offload(hdr_mbuf, &hdr->hdr);
			ASSIGN_UNLESS_EQUAL(hdr->num_buffers, num_buffers);

			vhost_log_write(dev, hdr_phys_addr, dev->vhost_hlen);
			PRINT_PACKET(dev, (uintptr_t)hdr_addr,
				     dev->vhost_hlen, 0);

			hdr_addr = 0;
		}

		cpy_len = RTE_MIN(desc_avail, mbuf_avail);
		rte_memcpy((void *)((uintptr_t)(desc_addr + desc_offset)),
			rte_pktmbuf_mtod_offset(m, void *, mbuf_offset),
			cpy_len);
		vhost_log_write(dev, buf_vec[vec_idx].buf_addr + desc_offset,
			cpy_len);
		PRINT_PACKET(dev, (uintptr_t)(desc_addr + desc_offset),
			cpy_len, 0);

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		desc_avail  -= cpy_len;
		desc_offset += cpy_len;
	}

	return 0;
}

static __rte_always_inline uint32_t
virtio_dev_merge_rx(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_virtqueue *vq;
	uint32_t pkt_idx = 0;
	uint16_t num_buffers;
	struct buf_vector buf_vec[BUF_VECTOR_MAX];
	uint16_t avail_head;

	LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->nr_vring))) {
		RTE_LOG(ERR, VHOST_DATA, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];
	if (unlikely(vq->enabled == 0))
		return 0;

	count = RTE_MIN((uint32_t)MAX_PKT_BURST, count);
	if (count == 0)
		return 0;

	rte_prefetch0(&vq->avail->ring[vq->last_avail_idx & (vq->size - 1)]);

	vq->shadow_used_idx = 0;
	avail_head = *((volatile uint16_t *)&vq->avail->idx);
	for (pkt_idx = 0; pkt_idx < count; pkt_idx++) {
		uint32_t pkt_len = pkts[pkt_idx]->pkt_len + dev->vhost_hlen;

		if (unlikely(reserve_avail_buf_mergeable(dev, vq,
						pkt_len, buf_vec, &num_buffers,
						avail_head) < 0)) {
			LOG_DEBUG(VHOST_DATA,
				"(%d) failed to get enough desc from vring\n",
				dev->vid);
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		LOG_DEBUG(VHOST_DATA, "(%d) current index %d | end index %d\n",
			dev->vid, vq->last_avail_idx,
			vq->last_avail_idx + num_buffers);

		if (copy_mbuf_to_desc_mergeable(dev, pkts[pkt_idx],
						buf_vec, num_buffers) < 0) {
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		vq->last_avail_idx += num_buffers;
	}

	if (likely(vq->shadow_used_idx)) {
		flush_shadow_used_ring(dev, vq);

		/* flush used->idx update before we read avail->flags. */
		rte_mb();

		/* Kick the guest if necessary. */
		if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT)
				&& (vq->callfd >= 0))
			eventfd_write(vq->callfd, (eventfd_t)1);
	}

	return pkt_idx;
}

uint16_t
rte_vhost_enqueue_burst(int vid, uint16_t queue_id,
	struct rte_mbuf **pkts, uint16_t count)
{
	struct virtio_net *dev = get_device(vid);

	if (!dev)
		return 0;

	if (dev->features & (1 << VIRTIO_NET_F_MRG_RXBUF))
		return virtio_dev_merge_rx(dev, queue_id, pkts, count);
	else
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
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	void *l3_hdr = NULL;
	struct ether_hdr *eth_hdr;
	uint16_t ethertype;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	m->l2_len = sizeof(struct ether_hdr);
	ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (ethertype == ETHER_TYPE_VLAN) {
		struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);

		m->l2_len += sizeof(struct vlan_hdr);
		ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	}

	l3_hdr = (char *)eth_hdr + m->l2_len;

	switch (ethertype) {
	case ETHER_TYPE_IPv4:
		ipv4_hdr = l3_hdr;
		*l4_proto = ipv4_hdr->next_proto_id;
		m->l3_len = (ipv4_hdr->version_ihl & 0x0f) * 4;
		*l4_hdr = (char *)l3_hdr + m->l3_len;
		m->ol_flags |= PKT_TX_IPV4;
		break;
	case ETHER_TYPE_IPv6:
		ipv6_hdr = l3_hdr;
		*l4_proto = ipv6_hdr->proto;
		m->l3_len = sizeof(struct ipv6_hdr);
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
	struct tcp_hdr *tcp_hdr = NULL;

	if (hdr->flags == 0 && hdr->gso_type == VIRTIO_NET_HDR_GSO_NONE)
		return;

	parse_ethernet(m, &l4_proto, &l4_hdr);
	if (hdr->flags == VIRTIO_NET_HDR_F_NEEDS_CSUM) {
		if (hdr->csum_start == (m->l2_len + m->l3_len)) {
			switch (hdr->csum_offset) {
			case (offsetof(struct tcp_hdr, cksum)):
				if (l4_proto == IPPROTO_TCP)
					m->ol_flags |= PKT_TX_TCP_CKSUM;
				break;
			case (offsetof(struct udp_hdr, dgram_cksum)):
				if (l4_proto == IPPROTO_UDP)
					m->ol_flags |= PKT_TX_UDP_CKSUM;
				break;
			case (offsetof(struct sctp_hdr, cksum)):
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
		default:
			RTE_LOG(WARNING, VHOST_DATA,
				"unsupported gso type %u.\n", hdr->gso_type);
			break;
		}
	}
}

#define RARP_PKT_SIZE	64

static int
make_rarp_packet(struct rte_mbuf *rarp_mbuf, const struct ether_addr *mac)
{
	struct ether_hdr *eth_hdr;
	struct arp_hdr  *rarp;

	if (rarp_mbuf->buf_len < 64) {
		RTE_LOG(WARNING, VHOST_DATA,
			"failed to make RARP; mbuf size too small %u (< %d)\n",
			rarp_mbuf->buf_len, RARP_PKT_SIZE);
		return -1;
	}

	/* Ethernet header. */
	eth_hdr = rte_pktmbuf_mtod_offset(rarp_mbuf, struct ether_hdr *, 0);
	memset(eth_hdr->d_addr.addr_bytes, 0xff, ETHER_ADDR_LEN);
	ether_addr_copy(mac, &eth_hdr->s_addr);
	eth_hdr->ether_type = htons(ETHER_TYPE_RARP);

	/* RARP header. */
	rarp = (struct arp_hdr *)(eth_hdr + 1);
	rarp->arp_hrd = htons(ARP_HRD_ETHER);
	rarp->arp_pro = htons(ETHER_TYPE_IPv4);
	rarp->arp_hln = ETHER_ADDR_LEN;
	rarp->arp_pln = 4;
	rarp->arp_op  = htons(ARP_OP_REVREQUEST);

	ether_addr_copy(mac, &rarp->arp_data.arp_sha);
	ether_addr_copy(mac, &rarp->arp_data.arp_tha);
	memset(&rarp->arp_data.arp_sip, 0x00, 4);
	memset(&rarp->arp_data.arp_tip, 0x00, 4);

	rarp_mbuf->pkt_len  = rarp_mbuf->data_len = RARP_PKT_SIZE;

	return 0;
}

static __rte_always_inline void
put_zmbuf(struct zcopy_mbuf *zmbuf)
{
	zmbuf->in_use = 0;
}

static __rte_always_inline int
copy_desc_to_mbuf(struct virtio_net *dev, struct vring_desc *descs,
		  uint16_t max_desc, struct rte_mbuf *m, uint16_t desc_idx,
		  struct rte_mempool *mbuf_pool)
{
	struct vring_desc *desc;
	uint64_t desc_addr;
	uint32_t desc_avail, desc_offset;
	uint32_t mbuf_avail, mbuf_offset;
	uint32_t cpy_len;
	struct rte_mbuf *cur = m, *prev = m;
	struct virtio_net_hdr *hdr = NULL;
	/* A counter to avoid desc dead loop chain */
	uint32_t nr_desc = 1;

	desc = &descs[desc_idx];
	if (unlikely((desc->len < dev->vhost_hlen)) ||
			(desc->flags & VRING_DESC_F_INDIRECT))
		return -1;

	desc_addr = rte_vhost_gpa_to_vva(dev->mem, desc->addr);
	if (unlikely(!desc_addr))
		return -1;

	if (virtio_net_with_host_offload(dev)) {
		hdr = (struct virtio_net_hdr *)((uintptr_t)desc_addr);
		rte_prefetch0(hdr);
	}

	/*
	 * A virtio driver normally uses at least 2 desc buffers
	 * for Tx: the first for storing the header, and others
	 * for storing the data.
	 */
	if (likely((desc->len == dev->vhost_hlen) &&
		   (desc->flags & VRING_DESC_F_NEXT) != 0)) {
		desc = &descs[desc->next];
		if (unlikely(desc->flags & VRING_DESC_F_INDIRECT))
			return -1;

		desc_addr = rte_vhost_gpa_to_vva(dev->mem, desc->addr);
		if (unlikely(!desc_addr))
			return -1;

		desc_offset = 0;
		desc_avail  = desc->len;
		nr_desc    += 1;
	} else {
		desc_avail  = desc->len - dev->vhost_hlen;
		desc_offset = dev->vhost_hlen;
	}

	rte_prefetch0((void *)(uintptr_t)(desc_addr + desc_offset));

	PRINT_PACKET(dev, (uintptr_t)(desc_addr + desc_offset), desc_avail, 0);

	mbuf_offset = 0;
	mbuf_avail  = m->buf_len - RTE_PKTMBUF_HEADROOM;
	while (1) {
		uint64_t hpa;

		cpy_len = RTE_MIN(desc_avail, mbuf_avail);

		/*
		 * A desc buf might across two host physical pages that are
		 * not continuous. In such case (gpa_to_hpa returns 0), data
		 * will be copied even though zero copy is enabled.
		 */
		if (unlikely(dev->dequeue_zero_copy && (hpa = gpa_to_hpa(dev,
					desc->addr + desc_offset, cpy_len)))) {
			cur->data_len = cpy_len;
			cur->data_off = 0;
			cur->buf_addr = (void *)(uintptr_t)desc_addr;
			cur->buf_physaddr = hpa;

			/*
			 * In zero copy mode, one mbuf can only reference data
			 * for one or partial of one desc buff.
			 */
			mbuf_avail = cpy_len;
		} else {
			rte_memcpy(rte_pktmbuf_mtod_offset(cur, void *,
							   mbuf_offset),
				(void *)((uintptr_t)(desc_addr + desc_offset)),
				cpy_len);
		}

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		desc_avail  -= cpy_len;
		desc_offset += cpy_len;

		/* This desc reaches to its end, get the next one */
		if (desc_avail == 0) {
			if ((desc->flags & VRING_DESC_F_NEXT) == 0)
				break;

			if (unlikely(desc->next >= max_desc ||
				     ++nr_desc > max_desc))
				return -1;
			desc = &descs[desc->next];
			if (unlikely(desc->flags & VRING_DESC_F_INDIRECT))
				return -1;

			desc_addr = rte_vhost_gpa_to_vva(dev->mem, desc->addr);
			if (unlikely(!desc_addr))
				return -1;

			rte_prefetch0((void *)(uintptr_t)desc_addr);

			desc_offset = 0;
			desc_avail  = desc->len;

			PRINT_PACKET(dev, (uintptr_t)desc_addr, desc->len, 0);
		}

		/*
		 * This mbuf reaches to its end, get a new one
		 * to hold more data.
		 */
		if (mbuf_avail == 0) {
			cur = rte_pktmbuf_alloc(mbuf_pool);
			if (unlikely(cur == NULL)) {
				RTE_LOG(ERR, VHOST_DATA, "Failed to "
					"allocate memory for mbuf.\n");
				return -1;
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

	return 0;
}

static __rte_always_inline void
update_used_ring(struct virtio_net *dev, struct vhost_virtqueue *vq,
		 uint32_t used_idx, uint32_t desc_idx)
{
	vq->used->ring[used_idx].id  = desc_idx;
	vq->used->ring[used_idx].len = 0;
	vhost_log_used_vring(dev, vq,
			offsetof(struct vring_used, ring[used_idx]),
			sizeof(vq->used->ring[used_idx]));
}

static __rte_always_inline void
update_used_idx(struct virtio_net *dev, struct vhost_virtqueue *vq,
		uint32_t count)
{
	if (unlikely(count == 0))
		return;

	rte_smp_wmb();
	rte_smp_rmb();

	vq->used->idx += count;
	vhost_log_used_vring(dev, vq, offsetof(struct vring_used, idx),
			sizeof(vq->used->idx));

	/* Kick guest if required. */
	if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT)
			&& (vq->callfd >= 0))
		eventfd_write(vq->callfd, (eventfd_t)1);
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

static __rte_always_inline bool
mbuf_is_consumed(struct rte_mbuf *m)
{
	while (m) {
		if (rte_mbuf_refcnt_read(m) > 1)
			return false;
		m = m->next;
	}

	return true;
}

uint16_t
rte_vhost_dequeue_burst(int vid, uint16_t queue_id,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
{
	struct virtio_net *dev;
	struct rte_mbuf *rarp_mbuf = NULL;
	struct vhost_virtqueue *vq;
	uint32_t desc_indexes[MAX_PKT_BURST];
	uint32_t used_idx;
	uint32_t i = 0;
	uint16_t free_entries;
	uint16_t avail_idx;

	dev = get_device(vid);
	if (!dev)
		return 0;

	if (unlikely(!is_valid_virt_queue_idx(queue_id, 1, dev->nr_vring))) {
		RTE_LOG(ERR, VHOST_DATA, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];
	if (unlikely(vq->enabled == 0))
		return 0;

	if (unlikely(dev->dequeue_zero_copy)) {
		struct zcopy_mbuf *zmbuf, *next;
		int nr_updated = 0;

		for (zmbuf = TAILQ_FIRST(&vq->zmbuf_list);
		     zmbuf != NULL; zmbuf = next) {
			next = TAILQ_NEXT(zmbuf, next);

			if (mbuf_is_consumed(zmbuf->mbuf)) {
				used_idx = vq->last_used_idx++ & (vq->size - 1);
				update_used_ring(dev, vq, used_idx,
						 zmbuf->desc_idx);
				nr_updated += 1;

				TAILQ_REMOVE(&vq->zmbuf_list, zmbuf, next);
				rte_pktmbuf_free(zmbuf->mbuf);
				put_zmbuf(zmbuf);
				vq->nr_zmbuf -= 1;
			}
		}

		update_used_idx(dev, vq, nr_updated);
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

		rarp_mbuf = rte_pktmbuf_alloc(mbuf_pool);
		if (rarp_mbuf == NULL) {
			RTE_LOG(ERR, VHOST_DATA,
				"Failed to allocate memory for mbuf.\n");
			return 0;
		}

		if (make_rarp_packet(rarp_mbuf, &dev->mac)) {
			rte_pktmbuf_free(rarp_mbuf);
			rarp_mbuf = NULL;
		} else {
			count -= 1;
		}
	}

	free_entries = *((volatile uint16_t *)&vq->avail->idx) -
			vq->last_avail_idx;
	if (free_entries == 0)
		goto out;

	LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);

	/* Prefetch available and used ring */
	avail_idx = vq->last_avail_idx & (vq->size - 1);
	used_idx  = vq->last_used_idx  & (vq->size - 1);
	rte_prefetch0(&vq->avail->ring[avail_idx]);
	rte_prefetch0(&vq->used->ring[used_idx]);

	count = RTE_MIN(count, MAX_PKT_BURST);
	count = RTE_MIN(count, free_entries);
	LOG_DEBUG(VHOST_DATA, "(%d) about to dequeue %u buffers\n",
			dev->vid, count);

	/* Retrieve all of the head indexes first to avoid caching issues. */
	for (i = 0; i < count; i++) {
		avail_idx = (vq->last_avail_idx + i) & (vq->size - 1);
		used_idx  = (vq->last_used_idx  + i) & (vq->size - 1);
		desc_indexes[i] = vq->avail->ring[avail_idx];

		if (likely(dev->dequeue_zero_copy == 0))
			update_used_ring(dev, vq, used_idx, desc_indexes[i]);
	}

	/* Prefetch descriptor index. */
	rte_prefetch0(&vq->desc[desc_indexes[0]]);
	for (i = 0; i < count; i++) {
		struct vring_desc *desc;
		uint16_t sz, idx;
		int err;

		if (likely(i + 1 < count))
			rte_prefetch0(&vq->desc[desc_indexes[i + 1]]);

		if (vq->desc[desc_indexes[i]].flags & VRING_DESC_F_INDIRECT) {
			desc = (struct vring_desc *)(uintptr_t)
				rte_vhost_gpa_to_vva(dev->mem,
					vq->desc[desc_indexes[i]].addr);
			if (unlikely(!desc))
				break;

			rte_prefetch0(desc);
			sz = vq->desc[desc_indexes[i]].len / sizeof(*desc);
			idx = 0;
		} else {
			desc = vq->desc;
			sz = vq->size;
			idx = desc_indexes[i];
		}

		pkts[i] = rte_pktmbuf_alloc(mbuf_pool);
		if (unlikely(pkts[i] == NULL)) {
			RTE_LOG(ERR, VHOST_DATA,
				"Failed to allocate memory for mbuf.\n");
			break;
		}

		err = copy_desc_to_mbuf(dev, desc, sz, pkts[i], idx, mbuf_pool);
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
			zmbuf->desc_idx = desc_indexes[i];

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
		vq->last_used_idx += i;
		update_used_idx(dev, vq, i);
	}

out:
	if (unlikely(rarp_mbuf != NULL)) {
		/*
		 * Inject it to the head of "pkts" array, so that switch's mac
		 * learning table will get updated first.
		 */
		memmove(&pkts[1], pkts, i * sizeof(struct rte_mbuf *));
		pkts[0] = rarp_mbuf;
		i += 1;
	}

	return i;
}
