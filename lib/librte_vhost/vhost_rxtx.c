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
	memset(net_hdr, 0, sizeof(struct virtio_net_hdr));

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

	return;
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
	struct vring_desc *desc, *hdr_desc;
	struct rte_mbuf *buff, *first_buff;
	/* The virtio_hdr is initialised to 0. */
	struct virtio_net_hdr_mrg_rxbuf virtio_hdr = {{0, 0, 0, 0, 0, 0}, 0};
	uint64_t buff_addr = 0;
	uint64_t buff_hdr_addr = 0;
	uint32_t head[MAX_PKT_BURST];
	uint32_t head_idx, packet_success = 0;
	uint16_t avail_idx, res_cur_idx;
	uint16_t res_base_idx, res_end_idx;
	uint16_t free_entries;
	uint8_t success = 0;

	LOG_DEBUG(VHOST_DATA, "(%"PRIu64") virtio_dev_rx()\n", dev->device_fh);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->virt_qp_nb))) {
		RTE_LOG(ERR, VHOST_DATA,
			"%s (%"PRIu64"): virtqueue idx:%d invalid.\n",
			__func__, dev->device_fh, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];
	if (unlikely(vq->enabled == 0))
		return 0;

	count = (count > MAX_PKT_BURST) ? MAX_PKT_BURST : count;

	/*
	 * As many data cores may want access to available buffers,
	 * they need to be reserved.
	 */
	do {
		res_base_idx = vq->last_used_idx_res;
		avail_idx = *((volatile uint16_t *)&vq->avail->idx);

		free_entries = (avail_idx - res_base_idx);
		/*check that we have enough buffers*/
		if (unlikely(count > free_entries))
			count = free_entries;

		if (count == 0)
			return 0;

		res_end_idx = res_base_idx + count;
		/* vq->last_used_idx_res is atomically updated. */
		/* TODO: Allow to disable cmpset if no concurrency in application. */
		success = rte_atomic16_cmpset(&vq->last_used_idx_res,
				res_base_idx, res_end_idx);
	} while (unlikely(success == 0));
	res_cur_idx = res_base_idx;
	LOG_DEBUG(VHOST_DATA, "(%"PRIu64") Current Index %d| End Index %d\n",
			dev->device_fh, res_cur_idx, res_end_idx);

	/* Prefetch available ring to retrieve indexes. */
	rte_prefetch0(&vq->avail->ring[res_cur_idx & (vq->size - 1)]);

	/* Retrieve all of the head indexes first to avoid caching issues. */
	for (head_idx = 0; head_idx < count; head_idx++)
		head[head_idx] = vq->avail->ring[(res_cur_idx + head_idx) &
					(vq->size - 1)];

	/*Prefetch descriptor index. */
	rte_prefetch0(&vq->desc[head[packet_success]]);

	while (res_cur_idx != res_end_idx) {
		uint32_t offset = 0, vb_offset = 0;
		uint32_t pkt_len, len_to_cpy, data_len, total_copied = 0;
		uint8_t hdr = 0, uncompleted_pkt = 0;
		uint16_t idx;

		/* Get descriptor from available ring */
		desc = &vq->desc[head[packet_success]];

		buff = pkts[packet_success];
		first_buff = buff;

		/* Convert from gpa to vva (guest physical addr -> vhost virtual addr) */
		buff_addr = gpa_to_vva(dev, desc->addr);
		/* Prefetch buffer address. */
		rte_prefetch0((void *)(uintptr_t)buff_addr);

		/* Copy virtio_hdr to packet and increment buffer address */
		buff_hdr_addr = buff_addr;
		hdr_desc = desc;

		/*
		 * If the descriptors are chained the header and data are
		 * placed in separate buffers.
		 */
		if ((desc->flags & VRING_DESC_F_NEXT) &&
			(desc->len == vq->vhost_hlen)) {
			desc = &vq->desc[desc->next];
			/* Buffer address translation. */
			buff_addr = gpa_to_vva(dev, desc->addr);
		} else {
			vb_offset += vq->vhost_hlen;
			hdr = 1;
		}

		pkt_len = rte_pktmbuf_pkt_len(buff);
		data_len = rte_pktmbuf_data_len(buff);
		len_to_cpy = RTE_MIN(data_len,
			hdr ? desc->len - vq->vhost_hlen : desc->len);
		while (total_copied < pkt_len) {
			/* Copy mbuf data to buffer */
			rte_memcpy((void *)(uintptr_t)(buff_addr + vb_offset),
				rte_pktmbuf_mtod_offset(buff, const void *, offset),
				len_to_cpy);
			vhost_log_write(dev, desc->addr + vb_offset, len_to_cpy);
			PRINT_PACKET(dev, (uintptr_t)(buff_addr + vb_offset),
				len_to_cpy, 0);

			offset += len_to_cpy;
			vb_offset += len_to_cpy;
			total_copied += len_to_cpy;

			/* The whole packet completes */
			if (total_copied == pkt_len)
				break;

			/* The current segment completes */
			if (offset == data_len) {
				buff = buff->next;
				offset = 0;
				data_len = rte_pktmbuf_data_len(buff);
			}

			/* The current vring descriptor done */
			if (vb_offset == desc->len) {
				if (desc->flags & VRING_DESC_F_NEXT) {
					desc = &vq->desc[desc->next];
					buff_addr = gpa_to_vva(dev, desc->addr);
					vb_offset = 0;
				} else {
					/* Room in vring buffer is not enough */
					uncompleted_pkt = 1;
					break;
				}
			}
			len_to_cpy = RTE_MIN(data_len - offset, desc->len - vb_offset);
		}

		/* Update used ring with desc information */
		idx = res_cur_idx & (vq->size - 1);
		vq->used->ring[idx].id = head[packet_success];

		/* Drop the packet if it is uncompleted */
		if (unlikely(uncompleted_pkt == 1))
			vq->used->ring[idx].len = vq->vhost_hlen;
		else
			vq->used->ring[idx].len = pkt_len + vq->vhost_hlen;

		vhost_log_used_vring(dev, vq,
			offsetof(struct vring_used, ring[idx]),
			sizeof(vq->used->ring[idx]));

		res_cur_idx++;
		packet_success++;

		if (unlikely(uncompleted_pkt == 1))
			continue;

		virtio_enqueue_offload(first_buff, &virtio_hdr.hdr);

		rte_memcpy((void *)(uintptr_t)buff_hdr_addr,
			(const void *)&virtio_hdr, vq->vhost_hlen);
		vhost_log_write(dev, hdr_desc->addr, vq->vhost_hlen);

		PRINT_PACKET(dev, (uintptr_t)buff_hdr_addr, vq->vhost_hlen, 1);

		if (res_cur_idx < res_end_idx) {
			/* Prefetch descriptor index. */
			rte_prefetch0(&vq->desc[head[packet_success]]);
		}
	}

	rte_compiler_barrier();

	/* Wait until it's our turn to add our buffer to the used ring. */
	while (unlikely(vq->last_used_idx != res_base_idx))
		rte_pause();

	*(volatile uint16_t *)&vq->used->idx += count;
	vq->last_used_idx = res_end_idx;
	vhost_log_used_vring(dev, vq,
		offsetof(struct vring_used, idx),
		sizeof(vq->used->idx));

	/* flush used->idx update before we read avail->flags. */
	rte_mb();

	/* Kick the guest if necessary. */
	if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT))
		eventfd_write(vq->callfd, (eventfd_t)1);
	return count;
}

static inline uint32_t __attribute__((always_inline))
copy_from_mbuf_to_vring(struct virtio_net *dev, uint32_t queue_id,
			uint16_t res_base_idx, uint16_t res_end_idx,
			struct rte_mbuf *pkt)
{
	uint32_t vec_idx = 0;
	uint32_t entry_success = 0;
	struct vhost_virtqueue *vq;
	/* The virtio_hdr is initialised to 0. */
	struct virtio_net_hdr_mrg_rxbuf virtio_hdr = {
		{0, 0, 0, 0, 0, 0}, 0};
	uint16_t cur_idx = res_base_idx;
	uint64_t vb_addr = 0;
	uint64_t vb_hdr_addr = 0;
	uint32_t seg_offset = 0;
	uint32_t vb_offset = 0;
	uint32_t seg_avail;
	uint32_t vb_avail;
	uint32_t cpy_len, entry_len;
	uint16_t idx;

	if (pkt == NULL)
		return 0;

	LOG_DEBUG(VHOST_DATA, "(%"PRIu64") Current Index %d| "
		"End Index %d\n",
		dev->device_fh, cur_idx, res_end_idx);

	/*
	 * Convert from gpa to vva
	 * (guest physical addr -> vhost virtual addr)
	 */
	vq = dev->virtqueue[queue_id];

	vb_addr = gpa_to_vva(dev, vq->buf_vec[vec_idx].buf_addr);
	vb_hdr_addr = vb_addr;

	/* Prefetch buffer address. */
	rte_prefetch0((void *)(uintptr_t)vb_addr);

	virtio_hdr.num_buffers = res_end_idx - res_base_idx;

	LOG_DEBUG(VHOST_DATA, "(%"PRIu64") RX: Num merge buffers %d\n",
		dev->device_fh, virtio_hdr.num_buffers);

	virtio_enqueue_offload(pkt, &virtio_hdr.hdr);

	rte_memcpy((void *)(uintptr_t)vb_hdr_addr,
		(const void *)&virtio_hdr, vq->vhost_hlen);
	vhost_log_write(dev, vq->buf_vec[vec_idx].buf_addr, vq->vhost_hlen);

	PRINT_PACKET(dev, (uintptr_t)vb_hdr_addr, vq->vhost_hlen, 1);

	seg_avail = rte_pktmbuf_data_len(pkt);
	vb_offset = vq->vhost_hlen;
	vb_avail = vq->buf_vec[vec_idx].buf_len - vq->vhost_hlen;

	entry_len = vq->vhost_hlen;

	if (vb_avail == 0) {
		uint32_t desc_idx = vq->buf_vec[vec_idx].desc_idx;

		if ((vq->desc[desc_idx].flags & VRING_DESC_F_NEXT) == 0) {
			idx = cur_idx & (vq->size - 1);

			/* Update used ring with desc information */
			vq->used->ring[idx].id = vq->buf_vec[vec_idx].desc_idx;
			vq->used->ring[idx].len = entry_len;

			vhost_log_used_vring(dev, vq,
					offsetof(struct vring_used, ring[idx]),
					sizeof(vq->used->ring[idx]));

			entry_len = 0;
			cur_idx++;
			entry_success++;
		}

		vec_idx++;
		vb_addr = gpa_to_vva(dev, vq->buf_vec[vec_idx].buf_addr);

		/* Prefetch buffer address. */
		rte_prefetch0((void *)(uintptr_t)vb_addr);
		vb_offset = 0;
		vb_avail = vq->buf_vec[vec_idx].buf_len;
	}

	cpy_len = RTE_MIN(vb_avail, seg_avail);

	while (cpy_len > 0) {
		/* Copy mbuf data to vring buffer */
		rte_memcpy((void *)(uintptr_t)(vb_addr + vb_offset),
			rte_pktmbuf_mtod_offset(pkt, const void *, seg_offset),
			cpy_len);
		vhost_log_write(dev, vq->buf_vec[vec_idx].buf_addr + vb_offset,
			cpy_len);

		PRINT_PACKET(dev,
			(uintptr_t)(vb_addr + vb_offset),
			cpy_len, 0);

		seg_offset += cpy_len;
		vb_offset += cpy_len;
		seg_avail -= cpy_len;
		vb_avail -= cpy_len;
		entry_len += cpy_len;

		if (seg_avail != 0) {
			/*
			 * The virtio buffer in this vring
			 * entry reach to its end.
			 * But the segment doesn't complete.
			 */
			if ((vq->desc[vq->buf_vec[vec_idx].desc_idx].flags &
				VRING_DESC_F_NEXT) == 0) {
				/* Update used ring with desc information */
				idx = cur_idx & (vq->size - 1);
				vq->used->ring[idx].id
					= vq->buf_vec[vec_idx].desc_idx;
				vq->used->ring[idx].len = entry_len;
				vhost_log_used_vring(dev, vq,
					offsetof(struct vring_used, ring[idx]),
					sizeof(vq->used->ring[idx]));
				entry_len = 0;
				cur_idx++;
				entry_success++;
			}

			vec_idx++;
			vb_addr = gpa_to_vva(dev,
				vq->buf_vec[vec_idx].buf_addr);
			vb_offset = 0;
			vb_avail = vq->buf_vec[vec_idx].buf_len;
			cpy_len = RTE_MIN(vb_avail, seg_avail);
		} else {
			/*
			 * This current segment complete, need continue to
			 * check if the whole packet complete or not.
			 */
			pkt = pkt->next;
			if (pkt != NULL) {
				/*
				 * There are more segments.
				 */
				if (vb_avail == 0) {
					/*
					 * This current buffer from vring is
					 * used up, need fetch next buffer
					 * from buf_vec.
					 */
					uint32_t desc_idx =
						vq->buf_vec[vec_idx].desc_idx;

					if ((vq->desc[desc_idx].flags &
						VRING_DESC_F_NEXT) == 0) {
						idx = cur_idx & (vq->size - 1);
						/*
						 * Update used ring with the
						 * descriptor information
						 */
						vq->used->ring[idx].id
							= desc_idx;
						vq->used->ring[idx].len
							= entry_len;
						vhost_log_used_vring(dev, vq,
							offsetof(struct vring_used, ring[idx]),
							sizeof(vq->used->ring[idx]));
						entry_success++;
						entry_len = 0;
						cur_idx++;
					}

					/* Get next buffer from buf_vec. */
					vec_idx++;
					vb_addr = gpa_to_vva(dev,
						vq->buf_vec[vec_idx].buf_addr);
					vb_avail =
						vq->buf_vec[vec_idx].buf_len;
					vb_offset = 0;
				}

				seg_offset = 0;
				seg_avail = rte_pktmbuf_data_len(pkt);
				cpy_len = RTE_MIN(vb_avail, seg_avail);
			} else {
				/*
				 * This whole packet completes.
				 */
				/* Update used ring with desc information */
				idx = cur_idx & (vq->size - 1);
				vq->used->ring[idx].id
					= vq->buf_vec[vec_idx].desc_idx;
				vq->used->ring[idx].len = entry_len;
				vhost_log_used_vring(dev, vq,
					offsetof(struct vring_used, ring[idx]),
					sizeof(vq->used->ring[idx]));
				entry_success++;
				break;
			}
		}
	}

	return entry_success;
}

static inline void __attribute__((always_inline))
update_secure_len(struct vhost_virtqueue *vq, uint32_t id,
	uint32_t *secure_len, uint32_t *vec_idx)
{
	uint16_t wrapped_idx = id & (vq->size - 1);
	uint32_t idx = vq->avail->ring[wrapped_idx];
	uint8_t next_desc;
	uint32_t len = *secure_len;
	uint32_t vec_id = *vec_idx;

	do {
		next_desc = 0;
		len += vq->desc[idx].len;
		vq->buf_vec[vec_id].buf_addr = vq->desc[idx].addr;
		vq->buf_vec[vec_id].buf_len = vq->desc[idx].len;
		vq->buf_vec[vec_id].desc_idx = idx;
		vec_id++;

		if (vq->desc[idx].flags & VRING_DESC_F_NEXT) {
			idx = vq->desc[idx].next;
			next_desc = 1;
		}
	} while (next_desc);

	*secure_len = len;
	*vec_idx = vec_id;
}

/*
 * This function works for mergeable RX.
 */
static inline uint32_t __attribute__((always_inline))
virtio_dev_merge_rx(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_virtqueue *vq;
	uint32_t pkt_idx = 0, entry_success = 0;
	uint16_t avail_idx;
	uint16_t res_base_idx, res_cur_idx;
	uint8_t success = 0;

	LOG_DEBUG(VHOST_DATA, "(%"PRIu64") virtio_dev_merge_rx()\n",
		dev->device_fh);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->virt_qp_nb))) {
		RTE_LOG(ERR, VHOST_DATA,
			"%s (%"PRIu64"): virtqueue idx:%d invalid.\n",
			__func__, dev->device_fh, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];
	if (unlikely(vq->enabled == 0))
		return 0;

	count = RTE_MIN((uint32_t)MAX_PKT_BURST, count);

	if (count == 0)
		return 0;

	for (pkt_idx = 0; pkt_idx < count; pkt_idx++) {
		uint32_t pkt_len = pkts[pkt_idx]->pkt_len + vq->vhost_hlen;

		do {
			/*
			 * As many data cores may want access to available
			 * buffers, they need to be reserved.
			 */
			uint32_t secure_len = 0;
			uint32_t vec_idx = 0;

			res_base_idx = vq->last_used_idx_res;
			res_cur_idx = res_base_idx;

			do {
				avail_idx = *((volatile uint16_t *)&vq->avail->idx);
				if (unlikely(res_cur_idx == avail_idx))
					goto merge_rx_exit;

				update_secure_len(vq, res_cur_idx,
						  &secure_len, &vec_idx);
				res_cur_idx++;
			} while (pkt_len > secure_len);

			/* vq->last_used_idx_res is atomically updated. */
			success = rte_atomic16_cmpset(&vq->last_used_idx_res,
							res_base_idx,
							res_cur_idx);
		} while (success == 0);

		entry_success = copy_from_mbuf_to_vring(dev, queue_id,
			res_base_idx, res_cur_idx, pkts[pkt_idx]);

		rte_compiler_barrier();

		/*
		 * Wait until it's our turn to add our buffer
		 * to the used ring.
		 */
		while (unlikely(vq->last_used_idx != res_base_idx))
			rte_pause();

		*(volatile uint16_t *)&vq->used->idx += entry_success;
		vq->last_used_idx = res_cur_idx;
	}

merge_rx_exit:
	if (likely(pkt_idx)) {
		/* flush used->idx update before we read avail->flags. */
		rte_mb();

		/* Kick the guest if necessary. */
		if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT))
			eventfd_write(vq->callfd, (eventfd_t)1);
	}

	return pkt_idx;
}

uint16_t
rte_vhost_enqueue_burst(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mbuf **pkts, uint16_t count)
{
	if (unlikely(dev->features & (1 << VIRTIO_NET_F_MRG_RXBUF)))
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

	desc = &vq->desc[desc_idx];
	desc_addr = gpa_to_vva(dev, desc->addr);
	rte_prefetch0((void *)(uintptr_t)desc_addr);

	/* Retrieve virtio net header */
	hdr = (struct virtio_net_hdr *)((uintptr_t)desc_addr);
	desc_avail  = desc->len - vq->vhost_hlen;
	desc_offset = vq->vhost_hlen;

	mbuf_offset = 0;
	mbuf_avail  = m->buf_len - RTE_PKTMBUF_HEADROOM;
	while (desc_avail != 0 || (desc->flags & VRING_DESC_F_NEXT) != 0) {
		/* This desc reaches to its end, get the next one */
		if (desc_avail == 0) {
			desc = &vq->desc[desc->next];

			desc_addr = gpa_to_vva(dev, desc->addr);
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

		cpy_len = RTE_MIN(desc_avail, mbuf_avail);
		rte_memcpy(rte_pktmbuf_mtod_offset(cur, void *, mbuf_offset),
			(void *)((uintptr_t)(desc_addr + desc_offset)),
			cpy_len);

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		desc_avail  -= cpy_len;
		desc_offset += cpy_len;
	}

	prev->data_len = mbuf_offset;
	m->pkt_len    += mbuf_offset;

	if (hdr->flags != 0 || hdr->gso_type != VIRTIO_NET_HDR_GSO_NONE)
		vhost_dequeue_offload(hdr, m);

	return 0;
}

uint16_t
rte_vhost_dequeue_burst(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
{
	struct rte_mbuf *rarp_mbuf = NULL;
	struct vhost_virtqueue *vq;
	uint32_t desc_indexes[MAX_PKT_BURST];
	uint32_t used_idx;
	uint32_t i = 0;
	uint16_t free_entries;
	uint16_t avail_idx;

	if (unlikely(!is_valid_virt_queue_idx(queue_id, 1, dev->virt_qp_nb))) {
		RTE_LOG(ERR, VHOST_DATA,
			"%s (%"PRIu64"): virtqueue idx:%d invalid.\n",
			__func__, dev->device_fh, queue_id);
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

	LOG_DEBUG(VHOST_DATA, "%s (%"PRIu64")\n", __func__, dev->device_fh);

	/* Prefetch available ring to retrieve head indexes. */
	used_idx = vq->last_used_idx & (vq->size - 1);
	rte_prefetch0(&vq->avail->ring[used_idx]);

	count = RTE_MIN(count, MAX_PKT_BURST);
	count = RTE_MIN(count, free_entries);
	LOG_DEBUG(VHOST_DATA, "(%"PRIu64") about to dequeue %u buffers\n",
			dev->device_fh, count);

	/* Retrieve all of the head indexes first to avoid caching issues. */
	for (i = 0; i < count; i++) {
		desc_indexes[i] = vq->avail->ring[(vq->last_used_idx + i) &
					(vq->size - 1)];
	}

	/* Prefetch descriptor index. */
	rte_prefetch0(&vq->desc[desc_indexes[0]]);
	rte_prefetch0(&vq->used->ring[vq->last_used_idx & (vq->size - 1)]);

	for (i = 0; i < count; i++) {
		int err;

		if (likely(i + 1 < count)) {
			rte_prefetch0(&vq->desc[desc_indexes[i + 1]]);
			rte_prefetch0(&vq->used->ring[(used_idx + 1) &
						      (vq->size - 1)]);
		}

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

		used_idx = vq->last_used_idx++ & (vq->size - 1);
		vq->used->ring[used_idx].id  = desc_indexes[i];
		vq->used->ring[used_idx].len = 0;
		vhost_log_used_vring(dev, vq,
				offsetof(struct vring_used, ring[used_idx]),
				sizeof(vq->used->ring[used_idx]));
	}

	rte_compiler_barrier();
	vq->used->idx += i;
	vhost_log_used_vring(dev, vq, offsetof(struct vring_used, idx),
			sizeof(vq->used->idx));

	/* Kick guest if required. */
	if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT))
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
