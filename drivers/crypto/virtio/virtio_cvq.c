/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Marvell
 */

#include <unistd.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_errno.h>

#include "virtio_cvq.h"
#include "virtqueue.h"

static struct virtio_pmd_ctrl *
virtio_send_command(struct virtcrypto_ctl *cvq,
			  struct virtio_pmd_ctrl *ctrl,
			  int *dlen, int dnum)
{
	struct virtqueue *vq = virtcrypto_cq_to_vq(cvq);
	struct virtio_pmd_ctrl *result;
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
	vq->vq_split.ring.desc[head].len = sizeof(struct virtio_crypto_op_ctrl_req);
	vq->vq_free_cnt--;
	i = vq->vq_split.ring.desc[head].next;

	for (k = 0; k < dnum; k++) {
		vq->vq_split.ring.desc[i].flags = VRING_DESC_F_NEXT;
		vq->vq_split.ring.desc[i].addr = cvq->hdr_mem
			+ sizeof(struct virtio_crypto_op_ctrl_req)
			+ sizeof(ctrl->input) + sizeof(uint8_t) * sum;
		vq->vq_split.ring.desc[i].len = dlen[k];
		sum += dlen[k];
		vq->vq_free_cnt--;
		i = vq->vq_split.ring.desc[i].next;
	}

	vq->vq_split.ring.desc[i].flags = VRING_DESC_F_WRITE;
	vq->vq_split.ring.desc[i].addr = cvq->hdr_mem
			+ sizeof(struct virtio_crypto_op_ctrl_req);
	vq->vq_split.ring.desc[i].len = sizeof(ctrl->input);
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

	PMD_INIT_LOG(DEBUG, "vq->vq_free_cnt=%d vq->vq_desc_head_idx=%d",
			vq->vq_free_cnt, vq->vq_desc_head_idx);

	result = cvq->hdr_mz->addr;
	return result;
}

int
virtio_crypto_send_command(struct virtcrypto_ctl *cvq, struct virtio_pmd_ctrl *ctrl,
	int *dlen, int dnum)
{
	struct virtio_pmd_ctrl *result;
	struct virtqueue *vq;
	uint8_t status = ~0;

	ctrl->input.status = status;

	if (!cvq) {
		PMD_INIT_LOG(ERR, "Control queue is not supported.");
		return -1;
	}

	rte_spinlock_lock(&cvq->lock);
	vq = virtcrypto_cq_to_vq(cvq);

	PMD_INIT_LOG(DEBUG, "vq->vq_desc_head_idx = %d, status = %d, "
		"vq->hw->cvq = %p vq = %p",
		vq->vq_desc_head_idx, status, vq->hw->cvq, vq);

	if (vq->vq_free_cnt < dnum + 2 || dnum < 1) {
		rte_spinlock_unlock(&cvq->lock);
		return -1;
	}

	memcpy(cvq->hdr_mz->addr, ctrl, sizeof(struct virtio_pmd_ctrl));
	result = virtio_send_command(cvq, ctrl, dlen, dnum);

	rte_spinlock_unlock(&cvq->lock);
	return result->input.status;
}
