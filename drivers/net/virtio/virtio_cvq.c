/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 * Copyright(c) 2022 Red Hat Inc,
 */

#include <unistd.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_errno.h>

#include "virtio_cvq.h"
#include "virtqueue.h"

static struct virtio_pmd_ctrl *
virtio_send_command_packed(struct virtnet_ctl *cvq,
			   struct virtio_pmd_ctrl *ctrl,
			   int *dlen, int pkt_num)
{
	struct virtqueue *vq = virtnet_cq_to_vq(cvq);
	int head;
	struct vring_packed_desc *desc = vq->vq_packed.ring.desc;
	struct virtio_pmd_ctrl *result;
	uint16_t flags;
	int sum = 0;
	int nb_descs = 0;
	int k;

	/*
	 * Format is enforced in qemu code:
	 * One TX packet for header;
	 * At least one TX packet per argument;
	 * One RX packet for ACK.
	 */
	head = vq->vq_avail_idx;
	flags = vq->vq_packed.cached_flags;
	desc[head].addr = cvq->hdr_mem;
	desc[head].len = sizeof(struct virtio_net_ctrl_hdr);
	vq->vq_free_cnt--;
	nb_descs++;
	if (++vq->vq_avail_idx >= vq->vq_nentries) {
		vq->vq_avail_idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^= VRING_PACKED_DESC_F_AVAIL_USED;
	}

	for (k = 0; k < pkt_num; k++) {
		desc[vq->vq_avail_idx].addr = cvq->hdr_mem
			+ sizeof(struct virtio_net_ctrl_hdr)
			+ sizeof(ctrl->status) + sizeof(uint8_t) * sum;
		desc[vq->vq_avail_idx].len = dlen[k];
		desc[vq->vq_avail_idx].flags = VRING_DESC_F_NEXT |
			vq->vq_packed.cached_flags;
		sum += dlen[k];
		vq->vq_free_cnt--;
		nb_descs++;
		if (++vq->vq_avail_idx >= vq->vq_nentries) {
			vq->vq_avail_idx -= vq->vq_nentries;
			vq->vq_packed.cached_flags ^=
				VRING_PACKED_DESC_F_AVAIL_USED;
		}
	}

	desc[vq->vq_avail_idx].addr = cvq->hdr_mem
		+ sizeof(struct virtio_net_ctrl_hdr);
	desc[vq->vq_avail_idx].len = sizeof(ctrl->status);
	desc[vq->vq_avail_idx].flags = VRING_DESC_F_WRITE |
		vq->vq_packed.cached_flags;
	vq->vq_free_cnt--;
	nb_descs++;
	if (++vq->vq_avail_idx >= vq->vq_nentries) {
		vq->vq_avail_idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^= VRING_PACKED_DESC_F_AVAIL_USED;
	}

	virtqueue_store_flags_packed(&desc[head], VRING_DESC_F_NEXT | flags,
			vq->hw->weak_barriers);

	virtio_wmb(vq->hw->weak_barriers);
	cvq->notify_queue(vq, cvq->notify_cookie);

	/* wait for used desc in virtqueue
	 * desc_is_used has a load-acquire or rte_io_rmb inside
	 */
	while (!desc_is_used(&desc[head], vq))
		usleep(100);

	/* now get used descriptors */
	vq->vq_free_cnt += nb_descs;
	vq->vq_used_cons_idx += nb_descs;
	if (vq->vq_used_cons_idx >= vq->vq_nentries) {
		vq->vq_used_cons_idx -= vq->vq_nentries;
		vq->vq_packed.used_wrap_counter ^= 1;
	}

	PMD_INIT_LOG(DEBUG, "vq->vq_free_cnt=%d\n"
			"vq->vq_avail_idx=%d\n"
			"vq->vq_used_cons_idx=%d\n"
			"vq->vq_packed.cached_flags=0x%x\n"
			"vq->vq_packed.used_wrap_counter=%d",
			vq->vq_free_cnt,
			vq->vq_avail_idx,
			vq->vq_used_cons_idx,
			vq->vq_packed.cached_flags,
			vq->vq_packed.used_wrap_counter);

	result = cvq->hdr_mz->addr;
	return result;
}

static struct virtio_pmd_ctrl *
virtio_send_command_split(struct virtnet_ctl *cvq,
			  struct virtio_pmd_ctrl *ctrl,
			  int *dlen, int pkt_num)
{
	struct virtio_pmd_ctrl *result;
	struct virtqueue *vq = virtnet_cq_to_vq(cvq);
	uint32_t head, i;
	int k, sum = 0;

	head = vq->vq_desc_head_idx;

	/*
	 * Format is enforced in qemu code:
	 * One TX packet for header;
	 * At least one TX packet per argument;
	 * One RX packet for ACK.
	 */
	vq->vq_split.ring.desc[head].flags = VRING_DESC_F_NEXT;
	vq->vq_split.ring.desc[head].addr = cvq->hdr_mem;
	vq->vq_split.ring.desc[head].len = sizeof(struct virtio_net_ctrl_hdr);
	vq->vq_free_cnt--;
	i = vq->vq_split.ring.desc[head].next;

	for (k = 0; k < pkt_num; k++) {
		vq->vq_split.ring.desc[i].flags = VRING_DESC_F_NEXT;
		vq->vq_split.ring.desc[i].addr = cvq->hdr_mem
			+ sizeof(struct virtio_net_ctrl_hdr)
			+ sizeof(ctrl->status) + sizeof(uint8_t) * sum;
		vq->vq_split.ring.desc[i].len = dlen[k];
		sum += dlen[k];
		vq->vq_free_cnt--;
		i = vq->vq_split.ring.desc[i].next;
	}

	vq->vq_split.ring.desc[i].flags = VRING_DESC_F_WRITE;
	vq->vq_split.ring.desc[i].addr = cvq->hdr_mem
			+ sizeof(struct virtio_net_ctrl_hdr);
	vq->vq_split.ring.desc[i].len = sizeof(ctrl->status);
	vq->vq_free_cnt--;

	vq->vq_desc_head_idx = vq->vq_split.ring.desc[i].next;

	vq_update_avail_ring(vq, head);
	vq_update_avail_idx(vq);

	PMD_INIT_LOG(DEBUG, "vq->vq_queue_index = %d", vq->vq_queue_index);

	cvq->notify_queue(vq, cvq->notify_cookie);

	while (virtqueue_nused(vq) == 0)
		usleep(100);

	while (virtqueue_nused(vq)) {
		uint32_t idx, desc_idx, used_idx;
		struct vring_used_elem *uep;

		used_idx = (uint32_t)(vq->vq_used_cons_idx
				& (vq->vq_nentries - 1));
		uep = &vq->vq_split.ring.used->ring[used_idx];
		idx = (uint32_t)uep->id;
		desc_idx = idx;

		while (vq->vq_split.ring.desc[desc_idx].flags &
				VRING_DESC_F_NEXT) {
			desc_idx = vq->vq_split.ring.desc[desc_idx].next;
			vq->vq_free_cnt++;
		}

		vq->vq_split.ring.desc[desc_idx].next = vq->vq_desc_head_idx;
		vq->vq_desc_head_idx = idx;

		vq->vq_used_cons_idx++;
		vq->vq_free_cnt++;
	}

	PMD_INIT_LOG(DEBUG, "vq->vq_free_cnt=%d\nvq->vq_desc_head_idx=%d",
			vq->vq_free_cnt, vq->vq_desc_head_idx);

	result = cvq->hdr_mz->addr;
	return result;
}

int
virtio_send_command(struct virtnet_ctl *cvq, struct virtio_pmd_ctrl *ctrl, int *dlen, int pkt_num)
{
	virtio_net_ctrl_ack status = ~0;
	struct virtio_pmd_ctrl *result;
	struct virtqueue *vq;

	ctrl->status = status;

	if (!cvq) {
		PMD_INIT_LOG(ERR, "Control queue is not supported.");
		return -1;
	}

	rte_spinlock_lock(&cvq->lock);
	vq = virtnet_cq_to_vq(cvq);

	PMD_INIT_LOG(DEBUG, "vq->vq_desc_head_idx = %d, status = %d, "
		"vq->hw->cvq = %p vq = %p",
		vq->vq_desc_head_idx, status, vq->hw->cvq, vq);

	if (vq->vq_free_cnt < pkt_num + 2 || pkt_num < 1) {
		rte_spinlock_unlock(&cvq->lock);
		return -1;
	}

	memcpy(cvq->hdr_mz->addr, ctrl, sizeof(struct virtio_pmd_ctrl));

	if (virtio_with_packed_queue(vq->hw))
		result = virtio_send_command_packed(cvq, ctrl, dlen, pkt_num);
	else
		result = virtio_send_command_split(cvq, ctrl, dlen, pkt_num);

	rte_spinlock_unlock(&cvq->lock);
	return result->status;
}

