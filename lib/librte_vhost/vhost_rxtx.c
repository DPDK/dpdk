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
#include <stdbool.h>
#include <linux/virtio_net.h>

#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_virtio_net.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_sctp.h>
#include <rte_arp.h>

#include "vhost-net.h"

#define MAX_PKT_BURST 32
#define VHOST_LOG_PAGE	4096

static inline void __attribute__((always_inline))
vhost_log_page(uint8_t *log_base, uint64_t page)
{
	log_base[page / 8] |= 1 << (page % 8);
}

static inline void __attribute__((always_inline))
vhost_log_write(struct virtio_net *dev, uint64_t addr, uint64_t len)
{
	uint64_t page;

	if (likely(((dev->features & (1ULL << VHOST_F_LOG_ALL)) == 0) ||
		   !dev->log_base || !len))
		return;

	if (unlikely(dev->log_size <= ((addr + len - 1) / VHOST_LOG_PAGE / 8)))
		return;

	/* To make sure guest memory updates are committed before logging */
	rte_smp_wmb();

	page = addr / VHOST_LOG_PAGE;
	while (page * VHOST_LOG_PAGE < addr + len) {
		vhost_log_page((uint8_t *)(uintptr_t)dev->log_base, page);
		page += 1;
	}
}

static inline void __attribute__((always_inline))
vhost_log_used_vring(struct virtio_net *dev, struct vhost_virtqueue *vq,
		     uint64_t offset, uint64_t len)
{
	vhost_log_write(dev, vq->log_guest_addr + offset, len);
}

static bool
is_valid_virt_queue_idx(uint32_t idx, int is_tx, uint32_t qp_nb)
{
	return (is_tx ^ (idx & 1)) == 0 && idx < qp_nb * VIRTIO_QNUM;
}

static void
virtio_enqueue_offload(struct rte_mbuf *m_buf, struct virtio_net_hdr *net_hdr)
{
	if (m_buf->ol_flags & PKT_TX_L4_MASK) {
		net_hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
		net_hdr->csum_start = m_buf->l2_len + m_buf->l3_len;

		switch (m_buf->ol_flags & PKT_TX_L4_MASK) {
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
	}

	if (m_buf->ol_flags & PKT_TX_TCP_SEG) {
		if (m_buf->ol_flags & PKT_TX_IPV4)
			net_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
		else
			net_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV6;
		net_hdr->gso_size = m_buf->tso_segsz;
		net_hdr->hdr_len = m_buf->l2_len + m_buf->l3_len
					+ m_buf->l4_len;
	}
}

static inline void
copy_virtio_net_hdr(struct virtio_net *dev, uint64_t desc_addr,
		    struct virtio_net_hdr_mrg_rxbuf hdr)
{
	if (dev->vhost_hlen == sizeof(struct virtio_net_hdr_mrg_rxbuf))
		*(struct virtio_net_hdr_mrg_rxbuf *)(uintptr_t)desc_addr = hdr;
	else
		*(struct virtio_net_hdr *)(uintptr_t)desc_addr = hdr.hdr;
}

static inline int __attribute__((always_inline))
copy_mbuf_to_desc(struct virtio_net *dev, struct vhost_virtqueue *vq,
		  struct rte_mbuf *m, uint16_t desc_idx)
{
	uint32_t desc_avail, desc_offset;
	uint32_t mbuf_avail, mbuf_offset;
	uint32_t cpy_len;
	struct vring_desc *desc;
	uint64_t desc_addr;
	struct virtio_net_hdr_mrg_rxbuf virtio_hdr = {{0, 0, 0, 0, 0, 0}, 0};

	desc = &vq->desc[desc_idx];
	desc_addr = gpa_to_vva(dev, desc->addr);
	/*
	 * Checking of 'desc_addr' placed outside of 'unlikely' macro to avoid
	 * performance issue with some versions of gcc (4.8.4 and 5.3.0) which
	 * otherwise stores offset on the stack instead of in a register.
	 */
	if (unlikely(desc->len < dev->vhost_hlen) || !desc_addr)
		return -1;

	rte_prefetch0((void *)(uintptr_t)desc_addr);

	virtio_enqueue_offload(m, &virtio_hdr.hdr);
	copy_virtio_net_hdr(dev, desc_addr, virtio_hdr);
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
			if (unlikely(desc->next >= vq->size))
				return -1;

			desc = &vq->desc[desc->next];
			desc_addr = gpa_to_vva(dev, desc->addr);
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
 * count is returned to indicate the number of packets that are succesfully
 * added to the RX queue. This function works when the mbuf is scattered, but
 * it doesn't support the mergeable feature.
 */
static inline uint32_t __attribute__((always_inline))
virtio_dev_rx(struct virtio_net *dev, uint16_t queue_id,
	      struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_virtqueue *vq;
	uint16_t avail_idx, free_entries, start_idx;
	uint16_t desc_indexes[MAX_PKT_BURST];
	uint16_t used_idx;
	uint32_t i;

	LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->virt_qp_nb))) {
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

		err = copy_mbuf_to_desc(dev, vq, pkts[i], desc_idx);
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

static inline int
fill_vec_buf(struct vhost_virtqueue *vq, uint32_t avail_idx,
	     uint32_t *allocated, uint32_t *vec_idx,
	     struct buf_vector *buf_vec)
{
	uint16_t idx = vq->avail->ring[avail_idx & (vq->size - 1)];
	uint32_t vec_id = *vec_idx;
	uint32_t len    = *allocated;

	while (1) {
		if (unlikely(vec_id >= BUF_VECTOR_MAX || idx >= vq->size))
			return -1;

		len += vq->desc[idx].len;
		buf_vec[vec_id].buf_addr = vq->desc[idx].addr;
		buf_vec[vec_id].buf_len  = vq->desc[idx].len;
		buf_vec[vec_id].desc_idx = idx;
		vec_id++;

		if ((vq->desc[idx].flags & VRING_DESC_F_NEXT) == 0)
			break;

		idx = vq->desc[idx].next;
	}

	*allocated = len;
	*vec_idx   = vec_id;

	return 0;
}

/*
 * Returns -1 on fail, 0 on success
 */
static inline int
reserve_avail_buf_mergeable(struct vhost_virtqueue *vq, uint32_t size,
			    uint16_t *end, struct buf_vector *buf_vec)
{
	uint16_t cur_idx;
	uint16_t avail_idx;
	uint32_t allocated = 0;
	uint32_t vec_idx = 0;
	uint16_t tries = 0;

	cur_idx  = vq->last_used_idx;

	while (1) {
		avail_idx = *((volatile uint16_t *)&vq->avail->idx);
		if (unlikely(cur_idx == avail_idx))
			return -1;

		if (unlikely(fill_vec_buf(vq, cur_idx, &allocated,
					  &vec_idx, buf_vec) < 0))
			return -1;

		cur_idx++;
		tries++;

		if (allocated >= size)
			break;

		/*
		 * if we tried all available ring items, and still
		 * can't get enough buf, it means something abnormal
		 * happened.
		 */
		if (unlikely(tries >= vq->size))
			return -1;
	}

	*end = cur_idx;
	return 0;
}

static inline uint32_t __attribute__((always_inline))
copy_mbuf_to_desc_mergeable(struct virtio_net *dev, struct vhost_virtqueue *vq,
			    uint16_t end_idx, struct rte_mbuf *m,
			    struct buf_vector *buf_vec)
{
	struct virtio_net_hdr_mrg_rxbuf virtio_hdr = {{0, 0, 0, 0, 0, 0}, 0};
	uint32_t vec_idx = 0;
	uint16_t start_idx = vq->last_used_idx;
	uint16_t cur_idx = start_idx;
	uint64_t desc_addr;
	uint32_t mbuf_offset, mbuf_avail;
	uint32_t desc_offset, desc_avail;
	uint32_t cpy_len;
	uint16_t desc_idx, used_idx;

	if (unlikely(m == NULL))
		return 0;

	LOG_DEBUG(VHOST_DATA, "(%d) current index %d | end index %d\n",
		dev->vid, cur_idx, end_idx);

	desc_addr = gpa_to_vva(dev, buf_vec[vec_idx].buf_addr);
	if (buf_vec[vec_idx].buf_len < dev->vhost_hlen || !desc_addr)
		return 0;

	rte_prefetch0((void *)(uintptr_t)desc_addr);

	virtio_hdr.num_buffers = end_idx - start_idx;
	LOG_DEBUG(VHOST_DATA, "(%d) RX: num merge buffers %d\n",
		dev->vid, virtio_hdr.num_buffers);

	virtio_enqueue_offload(m, &virtio_hdr.hdr);
	copy_virtio_net_hdr(dev, desc_addr, virtio_hdr);
	vhost_log_write(dev, buf_vec[vec_idx].buf_addr, dev->vhost_hlen);
	PRINT_PACKET(dev, (uintptr_t)desc_addr, dev->vhost_hlen, 0);

	desc_avail  = buf_vec[vec_idx].buf_len - dev->vhost_hlen;
	desc_offset = dev->vhost_hlen;

	mbuf_avail  = rte_pktmbuf_data_len(m);
	mbuf_offset = 0;
	while (mbuf_avail != 0 || m->next != NULL) {
		/* done with current desc buf, get the next one */
		if (desc_avail == 0) {
			desc_idx = buf_vec[vec_idx].desc_idx;

			if (!(vq->desc[desc_idx].flags & VRING_DESC_F_NEXT)) {
				/* Update used ring with desc information */
				used_idx = cur_idx++ & (vq->size - 1);
				vq->used->ring[used_idx].id  = desc_idx;
				vq->used->ring[used_idx].len = desc_offset;
				vhost_log_used_vring(dev, vq,
					offsetof(struct vring_used,
						 ring[used_idx]),
					sizeof(vq->used->ring[used_idx]));
			}

			vec_idx++;
			desc_addr = gpa_to_vva(dev, buf_vec[vec_idx].buf_addr);
			if (unlikely(!desc_addr))
				return 0;

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

	used_idx = cur_idx & (vq->size - 1);
	vq->used->ring[used_idx].id = buf_vec[vec_idx].desc_idx;
	vq->used->ring[used_idx].len = desc_offset;
	vhost_log_used_vring(dev, vq,
		offsetof(struct vring_used, ring[used_idx]),
		sizeof(vq->used->ring[used_idx]));

	return end_idx - start_idx;
}

static inline uint32_t __attribute__((always_inline))
virtio_dev_merge_rx(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_virtqueue *vq;
	uint32_t pkt_idx = 0, nr_used = 0;
	uint16_t end;
	struct buf_vector buf_vec[BUF_VECTOR_MAX];

	LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->virt_qp_nb))) {
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

	for (pkt_idx = 0; pkt_idx < count; pkt_idx++) {
		uint32_t pkt_len = pkts[pkt_idx]->pkt_len + dev->vhost_hlen;

		if (unlikely(reserve_avail_buf_mergeable(vq, pkt_len,
							 &end, buf_vec) < 0)) {
			LOG_DEBUG(VHOST_DATA,
				"(%d) failed to get enough desc from vring\n",
				dev->vid);
			break;
		}

		nr_used = copy_mbuf_to_desc_mergeable(dev, vq, end,
						      pkts[pkt_idx], buf_vec);
		rte_smp_wmb();

		*(volatile uint16_t *)&vq->used->idx += nr_used;
		vhost_log_used_vring(dev, vq, offsetof(struct vring_used, idx),
			sizeof(vq->used->idx));
		vq->last_used_idx += nr_used;
	}

	if (likely(pkt_idx)) {
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
		ipv4_hdr = (struct ipv4_hdr *)l3_hdr;
		*l4_proto = ipv4_hdr->next_proto_id;
		m->l3_len = (ipv4_hdr->version_ihl & 0x0f) * 4;
		*l4_hdr = (char *)l3_hdr + m->l3_len;
		m->ol_flags |= PKT_TX_IPV4;
		break;
	case ETHER_TYPE_IPv6:
		ipv6_hdr = (struct ipv6_hdr *)l3_hdr;
		*l4_proto = ipv6_hdr->proto;
		m->l3_len = sizeof(struct ipv6_hdr);
		*l4_hdr = (char *)l3_hdr + m->l3_len;
		m->ol_flags |= PKT_TX_IPV6;
		break;
	default:
		m->l3_len = 0;
		*l4_proto = 0;
		break;
	}
}

static inline void __attribute__((always_inline))
vhost_dequeue_offload(struct virtio_net_hdr *hdr, struct rte_mbuf *m)
{
	uint16_t l4_proto = 0;
	void *l4_hdr = NULL;
	struct tcp_hdr *tcp_hdr = NULL;

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

	if (hdr->gso_type != VIRTIO_NET_HDR_GSO_NONE) {
		switch (hdr->gso_type & ~VIRTIO_NET_HDR_GSO_ECN) {
		case VIRTIO_NET_HDR_GSO_TCPV4:
		case VIRTIO_NET_HDR_GSO_TCPV6:
			tcp_hdr = (struct tcp_hdr *)l4_hdr;
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

static inline int __attribute__((always_inline))
copy_desc_to_mbuf(struct virtio_net *dev, struct vhost_virtqueue *vq,
		  struct rte_mbuf *m, uint16_t desc_idx,
		  struct rte_mempool *mbuf_pool)
{
	struct vring_desc *desc;
	uint64_t desc_addr;
	uint32_t desc_avail, desc_offset;
	uint32_t mbuf_avail, mbuf_offset;
	uint32_t cpy_len;
	struct rte_mbuf *cur = m, *prev = m;
	struct virtio_net_hdr *hdr;
	/* A counter to avoid desc dead loop chain */
	uint32_t nr_desc = 1;

	desc = &vq->desc[desc_idx];
	if (unlikely(desc->len < dev->vhost_hlen))
		return -1;

	desc_addr = gpa_to_vva(dev, desc->addr);
	if (unlikely(!desc_addr))
		return -1;

	hdr = (struct virtio_net_hdr *)((uintptr_t)desc_addr);
	rte_prefetch0(hdr);

	/*
	 * A virtio driver normally uses at least 2 desc buffers
	 * for Tx: the first for storing the header, and others
	 * for storing the data.
	 */
	if (likely((desc->len == dev->vhost_hlen) &&
		   (desc->flags & VRING_DESC_F_NEXT) != 0)) {
		desc = &vq->desc[desc->next];

		desc_addr = gpa_to_vva(dev, desc->addr);
		if (unlikely(!desc_addr))
			return -1;

		rte_prefetch0((void *)(uintptr_t)desc_addr);

		desc_offset = 0;
		desc_avail  = desc->len;
		nr_desc    += 1;

		PRINT_PACKET(dev, (uintptr_t)desc_addr, desc->len, 0);
	} else {
		desc_avail  = desc->len - dev->vhost_hlen;
		desc_offset = dev->vhost_hlen;
	}

	mbuf_offset = 0;
	mbuf_avail  = m->buf_len - RTE_PKTMBUF_HEADROOM;
	while (1) {
		cpy_len = RTE_MIN(desc_avail, mbuf_avail);
		rte_memcpy(rte_pktmbuf_mtod_offset(cur, void *, mbuf_offset),
			(void *)((uintptr_t)(desc_addr + desc_offset)),
			cpy_len);

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		desc_avail  -= cpy_len;
		desc_offset += cpy_len;

		/* This desc reaches to its end, get the next one */
		if (desc_avail == 0) {
			if ((desc->flags & VRING_DESC_F_NEXT) == 0)
				break;

			if (unlikely(desc->next >= vq->size ||
				     ++nr_desc > vq->size))
				return -1;
			desc = &vq->desc[desc->next];

			desc_addr = gpa_to_vva(dev, desc->addr);
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

	if (hdr->flags != 0 || hdr->gso_type != VIRTIO_NET_HDR_GSO_NONE)
		vhost_dequeue_offload(hdr, m);

	return 0;
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

	if (unlikely(!is_valid_virt_queue_idx(queue_id, 1, dev->virt_qp_nb))) {
		RTE_LOG(ERR, VHOST_DATA, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];
	if (unlikely(vq->enabled == 0))
		return 0;

	/*
	 * Construct a RARP broadcast packet, and inject it to the "pkts"
	 * array, to looks like that guest actually send such packet.
	 *
	 * Check user_send_rarp() for more information.
	 */
	if (unlikely(rte_atomic16_cmpset((volatile uint16_t *)
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

	avail_idx =  *((volatile uint16_t *)&vq->avail->idx);
	free_entries = avail_idx - vq->last_used_idx;
	if (free_entries == 0)
		goto out;

	LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);

	/* Prefetch available ring to retrieve head indexes. */
	used_idx = vq->last_used_idx & (vq->size - 1);
	rte_prefetch0(&vq->avail->ring[used_idx]);
	rte_prefetch0(&vq->used->ring[used_idx]);

	count = RTE_MIN(count, MAX_PKT_BURST);
	count = RTE_MIN(count, free_entries);
	LOG_DEBUG(VHOST_DATA, "(%d) about to dequeue %u buffers\n",
			dev->vid, count);

	/* Retrieve all of the head indexes first to avoid caching issues. */
	for (i = 0; i < count; i++) {
		used_idx = (vq->last_used_idx + i) & (vq->size - 1);
		desc_indexes[i] = vq->avail->ring[used_idx];

		vq->used->ring[used_idx].id  = desc_indexes[i];
		vq->used->ring[used_idx].len = 0;
		vhost_log_used_vring(dev, vq,
				offsetof(struct vring_used, ring[used_idx]),
				sizeof(vq->used->ring[used_idx]));
	}

	/* Prefetch descriptor index. */
	rte_prefetch0(&vq->desc[desc_indexes[0]]);
	for (i = 0; i < count; i++) {
		int err;

		if (likely(i + 1 < count))
			rte_prefetch0(&vq->desc[desc_indexes[i + 1]]);

		pkts[i] = rte_pktmbuf_alloc(mbuf_pool);
		if (unlikely(pkts[i] == NULL)) {
			RTE_LOG(ERR, VHOST_DATA,
				"Failed to allocate memory for mbuf.\n");
			break;
		}
		err = copy_desc_to_mbuf(dev, vq, pkts[i], desc_indexes[i],
					mbuf_pool);
		if (unlikely(err)) {
			rte_pktmbuf_free(pkts[i]);
			break;
		}
	}

	rte_smp_wmb();
	rte_smp_rmb();
	vq->used->idx += i;
	vq->last_used_idx += i;
	vhost_log_used_vring(dev, vq, offsetof(struct vring_used, idx),
			sizeof(vq->used->idx));

	/* Kick guest if required. */
	if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT)
			&& (vq->callfd >= 0))
		eventfd_write(vq->callfd, (eventfd_t)1);

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
