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
#include <linux/virtio_net.h>

#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_virtio_net.h>

#include "vhost-net.h"

#define MAX_PKT_BURST 32

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
	struct vring_desc *desc;
	struct rte_mbuf *buff;
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
	if (unlikely(queue_id != VIRTIO_RXQ)) {
		LOG_DEBUG(VHOST_DATA, "mq isn't supported in this version.\n");
		return 0;
	}

	vq = dev->virtqueue[VIRTIO_RXQ];
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

		/* Get descriptor from available ring */
		desc = &vq->desc[head[packet_success]];

		buff = pkts[packet_success];

		/* Convert from gpa to vva (guest physical addr -> vhost virtual addr) */
		buff_addr = gpa_to_vva(dev, desc->addr);
		/* Prefetch buffer address. */
		rte_prefetch0((void *)(uintptr_t)buff_addr);

		/* Copy virtio_hdr to packet and increment buffer address */
		buff_hdr_addr = buff_addr;

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
		};

		/* Update used ring with desc information */
		vq->used->ring[res_cur_idx & (vq->size - 1)].id =
							head[packet_success];

		/* Drop the packet if it is uncompleted */
		if (unlikely(uncompleted_pkt == 1))
			vq->used->ring[res_cur_idx & (vq->size - 1)].len =
							vq->vhost_hlen;
		else
			vq->used->ring[res_cur_idx & (vq->size - 1)].len =
							pkt_len + vq->vhost_hlen;

		res_cur_idx++;
		packet_success++;

		if (unlikely(uncompleted_pkt == 1))
			continue;

		rte_memcpy((void *)(uintptr_t)buff_hdr_addr,
			(const void *)&virtio_hdr, vq->vhost_hlen);

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

	/* flush used->idx update before we read avail->flags. */
	rte_mb();

	/* Kick the guest if necessary. */
	if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT))
		eventfd_write((int)vq->callfd, 1);
	return count;
}

static inline uint32_t __attribute__((always_inline))
copy_from_mbuf_to_vring(struct virtio_net *dev, uint16_t res_base_idx,
	uint16_t res_end_idx, struct rte_mbuf *pkt)
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

	if (pkt == NULL)
		return 0;

	LOG_DEBUG(VHOST_DATA, "(%"PRIu64") Current Index %d| "
		"End Index %d\n",
		dev->device_fh, cur_idx, res_end_idx);

	/*
	 * Convert from gpa to vva
	 * (guest physical addr -> vhost virtual addr)
	 */
	vq = dev->virtqueue[VIRTIO_RXQ];
	vb_addr = gpa_to_vva(dev, vq->buf_vec[vec_idx].buf_addr);
	vb_hdr_addr = vb_addr;

	/* Prefetch buffer address. */
	rte_prefetch0((void *)(uintptr_t)vb_addr);

	virtio_hdr.num_buffers = res_end_idx - res_base_idx;

	LOG_DEBUG(VHOST_DATA, "(%"PRIu64") RX: Num merge buffers %d\n",
		dev->device_fh, virtio_hdr.num_buffers);

	rte_memcpy((void *)(uintptr_t)vb_hdr_addr,
		(const void *)&virtio_hdr, vq->vhost_hlen);

	PRINT_PACKET(dev, (uintptr_t)vb_hdr_addr, vq->vhost_hlen, 1);

	seg_avail = rte_pktmbuf_data_len(pkt);
	vb_offset = vq->vhost_hlen;
	vb_avail = vq->buf_vec[vec_idx].buf_len - vq->vhost_hlen;

	entry_len = vq->vhost_hlen;

	if (vb_avail == 0) {
		uint32_t desc_idx =
			vq->buf_vec[vec_idx].desc_idx;

		if ((vq->desc[desc_idx].flags
			& VRING_DESC_F_NEXT) == 0) {
			/* Update used ring with desc information */
			vq->used->ring[cur_idx & (vq->size - 1)].id
				= vq->buf_vec[vec_idx].desc_idx;
			vq->used->ring[cur_idx & (vq->size - 1)].len
				= entry_len;

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
				vq->used->ring[cur_idx & (vq->size - 1)].id
					= vq->buf_vec[vec_idx].desc_idx;
				vq->used->ring[cur_idx & (vq->size - 1)].len
					= entry_len;
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
						uint16_t wrapped_idx =
							cur_idx & (vq->size - 1);
						/*
						 * Update used ring with the
						 * descriptor information
						 */
						vq->used->ring[wrapped_idx].id
							= desc_idx;
						vq->used->ring[wrapped_idx].len
							= entry_len;
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
				vq->used->ring[cur_idx & (vq->size - 1)].id
					= vq->buf_vec[vec_idx].desc_idx;
				vq->used->ring[cur_idx & (vq->size - 1)].len
					= entry_len;
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
	if (unlikely(queue_id != VIRTIO_RXQ)) {
		LOG_DEBUG(VHOST_DATA, "mq isn't supported in this version.\n");
	}

	vq = dev->virtqueue[VIRTIO_RXQ];
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
				if (unlikely(res_cur_idx == avail_idx)) {
					LOG_DEBUG(VHOST_DATA,
						"(%"PRIu64") Failed "
						"to get enough desc from "
						"vring\n",
						dev->device_fh);
					return pkt_idx;
				} else {
					update_secure_len(vq, res_cur_idx, &secure_len, &vec_idx);
					res_cur_idx++;
				}
			} while (pkt_len > secure_len);

			/* vq->last_used_idx_res is atomically updated. */
			success = rte_atomic16_cmpset(&vq->last_used_idx_res,
							res_base_idx,
							res_cur_idx);
		} while (success == 0);

		entry_success = copy_from_mbuf_to_vring(dev, res_base_idx,
			res_cur_idx, pkts[pkt_idx]);

		rte_compiler_barrier();

		/*
		 * Wait until it's our turn to add our buffer
		 * to the used ring.
		 */
		while (unlikely(vq->last_used_idx != res_base_idx))
			rte_pause();

		*(volatile uint16_t *)&vq->used->idx += entry_success;
		vq->last_used_idx = res_cur_idx;

		/* flush used->idx update before we read avail->flags. */
		rte_mb();

		/* Kick the guest if necessary. */
		if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT))
			eventfd_write((int)vq->callfd, 1);
	}

	return count;
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

uint16_t
rte_vhost_dequeue_burst(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
{
	struct rte_mbuf *m, *prev;
	struct vhost_virtqueue *vq;
	struct vring_desc *desc;
	uint64_t vb_addr = 0;
	uint32_t head[MAX_PKT_BURST];
	uint32_t used_idx;
	uint32_t i;
	uint16_t free_entries, entry_success = 0;
	uint16_t avail_idx;

	if (unlikely(queue_id != VIRTIO_TXQ)) {
		LOG_DEBUG(VHOST_DATA, "mq isn't supported in this version.\n");
		return 0;
	}

	vq = dev->virtqueue[VIRTIO_TXQ];
	avail_idx =  *((volatile uint16_t *)&vq->avail->idx);

	/* If there are no available buffers then return. */
	if (vq->last_used_idx == avail_idx)
		return 0;

	LOG_DEBUG(VHOST_DATA, "%s (%"PRIu64")\n", __func__,
		dev->device_fh);

	/* Prefetch available ring to retrieve head indexes. */
	rte_prefetch0(&vq->avail->ring[vq->last_used_idx & (vq->size - 1)]);

	/*get the number of free entries in the ring*/
	free_entries = (avail_idx - vq->last_used_idx);

	free_entries = RTE_MIN(free_entries, count);
	/* Limit to MAX_PKT_BURST. */
	free_entries = RTE_MIN(free_entries, MAX_PKT_BURST);

	LOG_DEBUG(VHOST_DATA, "(%"PRIu64") Buffers available %d\n",
			dev->device_fh, free_entries);
	/* Retrieve all of the head indexes first to avoid caching issues. */
	for (i = 0; i < free_entries; i++)
		head[i] = vq->avail->ring[(vq->last_used_idx + i) & (vq->size - 1)];

	/* Prefetch descriptor index. */
	rte_prefetch0(&vq->desc[head[entry_success]]);
	rte_prefetch0(&vq->used->ring[vq->last_used_idx & (vq->size - 1)]);

	while (entry_success < free_entries) {
		uint32_t vb_avail, vb_offset;
		uint32_t seg_avail, seg_offset;
		uint32_t cpy_len;
		uint32_t seg_num = 0;
		struct rte_mbuf *cur;
		uint8_t alloc_err = 0;

		desc = &vq->desc[head[entry_success]];

		/* Discard first buffer as it is the virtio header */
		if (desc->flags & VRING_DESC_F_NEXT) {
			desc = &vq->desc[desc->next];
			vb_offset = 0;
			vb_avail = desc->len;
		} else {
			vb_offset = vq->vhost_hlen;
			vb_avail = desc->len - vb_offset;
		}

		/* Buffer address translation. */
		vb_addr = gpa_to_vva(dev, desc->addr);
		/* Prefetch buffer address. */
		rte_prefetch0((void *)(uintptr_t)vb_addr);

		used_idx = vq->last_used_idx & (vq->size - 1);

		if (entry_success < (free_entries - 1)) {
			/* Prefetch descriptor index. */
			rte_prefetch0(&vq->desc[head[entry_success+1]]);
			rte_prefetch0(&vq->used->ring[(used_idx + 1) & (vq->size - 1)]);
		}

		/* Update used index buffer information. */
		vq->used->ring[used_idx].id = head[entry_success];
		vq->used->ring[used_idx].len = 0;

		/* Allocate an mbuf and populate the structure. */
		m = rte_pktmbuf_alloc(mbuf_pool);
		if (unlikely(m == NULL)) {
			RTE_LOG(ERR, VHOST_DATA,
				"Failed to allocate memory for mbuf.\n");
			break;
		}
		seg_offset = 0;
		seg_avail = m->buf_len - RTE_PKTMBUF_HEADROOM;
		cpy_len = RTE_MIN(vb_avail, seg_avail);

		PRINT_PACKET(dev, (uintptr_t)vb_addr, desc->len, 0);

		seg_num++;
		cur = m;
		prev = m;
		while (cpy_len != 0) {
			rte_memcpy(rte_pktmbuf_mtod_offset(cur, void *, seg_offset),
				(void *)((uintptr_t)(vb_addr + vb_offset)),
				cpy_len);

			seg_offset += cpy_len;
			vb_offset += cpy_len;
			vb_avail -= cpy_len;
			seg_avail -= cpy_len;

			if (vb_avail != 0) {
				/*
				 * The segment reachs to its end,
				 * while the virtio buffer in TX vring has
				 * more data to be copied.
				 */
				cur->data_len = seg_offset;
				m->pkt_len += seg_offset;
				/* Allocate mbuf and populate the structure. */
				cur = rte_pktmbuf_alloc(mbuf_pool);
				if (unlikely(cur == NULL)) {
					RTE_LOG(ERR, VHOST_DATA, "Failed to "
						"allocate memory for mbuf.\n");
					rte_pktmbuf_free(m);
					alloc_err = 1;
					break;
				}

				seg_num++;
				prev->next = cur;
				prev = cur;
				seg_offset = 0;
				seg_avail = cur->buf_len - RTE_PKTMBUF_HEADROOM;
			} else {
				if (desc->flags & VRING_DESC_F_NEXT) {
					/*
					 * There are more virtio buffers in
					 * same vring entry need to be copied.
					 */
					if (seg_avail == 0) {
						/*
						 * The current segment hasn't
						 * room to accomodate more
						 * data.
						 */
						cur->data_len = seg_offset;
						m->pkt_len += seg_offset;
						/*
						 * Allocate an mbuf and
						 * populate the structure.
						 */
						cur = rte_pktmbuf_alloc(mbuf_pool);
						if (unlikely(cur == NULL)) {
							RTE_LOG(ERR,
								VHOST_DATA,
								"Failed to "
								"allocate memory "
								"for mbuf\n");
							rte_pktmbuf_free(m);
							alloc_err = 1;
							break;
						}
						seg_num++;
						prev->next = cur;
						prev = cur;
						seg_offset = 0;
						seg_avail = cur->buf_len - RTE_PKTMBUF_HEADROOM;
					}

					desc = &vq->desc[desc->next];

					/* Buffer address translation. */
					vb_addr = gpa_to_vva(dev, desc->addr);
					/* Prefetch buffer address. */
					rte_prefetch0((void *)(uintptr_t)vb_addr);
					vb_offset = 0;
					vb_avail = desc->len;

					PRINT_PACKET(dev, (uintptr_t)vb_addr,
						desc->len, 0);
				} else {
					/* The whole packet completes. */
					cur->data_len = seg_offset;
					m->pkt_len += seg_offset;
					vb_avail = 0;
				}
			}

			cpy_len = RTE_MIN(vb_avail, seg_avail);
		}

		if (unlikely(alloc_err == 1))
			break;

		m->nb_segs = seg_num;

		pkts[entry_success] = m;
		vq->last_used_idx++;
		entry_success++;
	}

	rte_compiler_barrier();
	vq->used->idx += entry_success;
	/* Kick guest if required. */
	if (!(vq->avail->flags & VRING_AVAIL_F_NO_INTERRUPT))
		eventfd_write((int)vq->callfd, 1);
	return entry_success;
}
