/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Red Hat, Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "iotlb.h"
#include "vhost.h"
#include "virtio_net_ctrl.h"

struct virtio_net_ctrl {
	uint8_t class;
	uint8_t command;
	uint8_t command_data[];
};

struct virtio_net_ctrl_elem {
	struct virtio_net_ctrl *ctrl_req;
	uint16_t head_idx;
	uint16_t n_descs;
	uint8_t *desc_ack;
};

static int
virtio_net_ctrl_pop(struct virtio_net *dev, struct vhost_virtqueue *cvq,
		struct virtio_net_ctrl_elem *ctrl_elem)
	__rte_shared_locks_required(&cvq->iotlb_lock)
{
	uint16_t avail_idx, desc_idx, n_descs = 0;
	uint64_t desc_len, desc_addr, desc_iova, data_len = 0;
	uint8_t *ctrl_req;
	struct vring_desc *descs;

	avail_idx = rte_atomic_load_explicit((unsigned short __rte_atomic *)&cvq->avail->idx,
		rte_memory_order_acquire);
	if (avail_idx == cvq->last_avail_idx) {
		VHOST_LOG_CONFIG(dev->ifname, DEBUG, "Control queue empty\n");
		return 0;
	}

	desc_idx = cvq->avail->ring[cvq->last_avail_idx];
	if (desc_idx >= cvq->size) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Out of range desc index, dropping\n");
		goto err;
	}

	ctrl_elem->head_idx = desc_idx;

	if (cvq->desc[desc_idx].flags & VRING_DESC_F_INDIRECT) {
		desc_len = cvq->desc[desc_idx].len;
		desc_iova = cvq->desc[desc_idx].addr;

		descs = (struct vring_desc *)(uintptr_t)vhost_iova_to_vva(dev, cvq,
					desc_iova, &desc_len, VHOST_ACCESS_RO);
		if (!descs || desc_len != cvq->desc[desc_idx].len) {
			VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to map ctrl indirect descs\n");
			goto err;
		}

		desc_idx = 0;
	} else {
		descs = cvq->desc;
	}

	while (1) {
		desc_len = descs[desc_idx].len;
		desc_iova = descs[desc_idx].addr;

		n_descs++;

		if (descs[desc_idx].flags & VRING_DESC_F_WRITE) {
			if (ctrl_elem->desc_ack) {
				VHOST_LOG_CONFIG(dev->ifname, ERR,
						"Unexpected ctrl chain layout\n");
				goto err;
			}

			if (desc_len != sizeof(uint8_t)) {
				VHOST_LOG_CONFIG(dev->ifname, ERR,
						"Invalid ack size for ctrl req, dropping\n");
				goto err;
			}

			ctrl_elem->desc_ack = (uint8_t *)(uintptr_t)vhost_iova_to_vva(dev, cvq,
					desc_iova, &desc_len, VHOST_ACCESS_WO);
			if (!ctrl_elem->desc_ack || desc_len != sizeof(uint8_t)) {
				VHOST_LOG_CONFIG(dev->ifname, ERR,
						"Failed to map ctrl ack descriptor\n");
				goto err;
			}
		} else {
			if (ctrl_elem->desc_ack) {
				VHOST_LOG_CONFIG(dev->ifname, ERR,
						"Unexpected ctrl chain layout\n");
				goto err;
			}

			data_len += desc_len;
		}

		if (!(descs[desc_idx].flags & VRING_DESC_F_NEXT))
			break;

		desc_idx = descs[desc_idx].next;
	}

	desc_idx = ctrl_elem->head_idx;

	if (cvq->desc[desc_idx].flags & VRING_DESC_F_INDIRECT)
		ctrl_elem->n_descs = 1;
	else
		ctrl_elem->n_descs = n_descs;

	if (!ctrl_elem->desc_ack) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Missing ctrl ack descriptor\n");
		goto err;
	}

	if (data_len < sizeof(ctrl_elem->ctrl_req->class) + sizeof(ctrl_elem->ctrl_req->command)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Invalid control header size\n");
		goto err;
	}

	ctrl_elem->ctrl_req = malloc(data_len);
	if (!ctrl_elem->ctrl_req) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to alloc ctrl request\n");
		goto err;
	}

	ctrl_req = (uint8_t *)ctrl_elem->ctrl_req;

	if (cvq->desc[desc_idx].flags & VRING_DESC_F_INDIRECT) {
		desc_len = cvq->desc[desc_idx].len;
		desc_iova = cvq->desc[desc_idx].addr;

		descs = (struct vring_desc *)(uintptr_t)vhost_iova_to_vva(dev, cvq,
					desc_iova, &desc_len, VHOST_ACCESS_RO);
		if (!descs || desc_len != cvq->desc[desc_idx].len) {
			VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to map ctrl indirect descs\n");
			goto free_err;
		}

		desc_idx = 0;
	} else {
		descs = cvq->desc;
	}

	while (!(descs[desc_idx].flags & VRING_DESC_F_WRITE)) {
		desc_len = descs[desc_idx].len;
		desc_iova = descs[desc_idx].addr;

		desc_addr = vhost_iova_to_vva(dev, cvq, desc_iova, &desc_len, VHOST_ACCESS_RO);
		if (!desc_addr || desc_len < descs[desc_idx].len) {
			VHOST_LOG_CONFIG(dev->ifname, ERR, "Failed to map ctrl descriptor\n");
			goto free_err;
		}

		memcpy(ctrl_req, (void *)(uintptr_t)desc_addr, desc_len);
		ctrl_req += desc_len;

		if (!(descs[desc_idx].flags & VRING_DESC_F_NEXT))
			break;

		desc_idx = descs[desc_idx].next;
	}

	cvq->last_avail_idx++;
	if (cvq->last_avail_idx >= cvq->size)
		cvq->last_avail_idx -= cvq->size;

	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX))
		vhost_avail_event(cvq) = cvq->last_avail_idx;

	return 1;

free_err:
	free(ctrl_elem->ctrl_req);
err:
	cvq->last_avail_idx++;
	if (cvq->last_avail_idx >= cvq->size)
		cvq->last_avail_idx -= cvq->size;

	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX))
		vhost_avail_event(cvq) = cvq->last_avail_idx;

	return -1;
}

static uint8_t
virtio_net_ctrl_handle_req(struct virtio_net *dev, struct virtio_net_ctrl *ctrl_req)
{
	uint8_t ret = VIRTIO_NET_ERR;

	if (ctrl_req->class == VIRTIO_NET_CTRL_MQ &&
			ctrl_req->command == VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET) {
		uint16_t queue_pairs;
		uint32_t i;

		queue_pairs = *(uint16_t *)(uintptr_t)ctrl_req->command_data;
		VHOST_LOG_CONFIG(dev->ifname, INFO, "Ctrl req: MQ %u queue pairs\n", queue_pairs);
		ret = VIRTIO_NET_OK;

		for (i = 0; i < dev->nr_vring; i++) {
			struct vhost_virtqueue *vq = dev->virtqueue[i];
			bool enable;

			if (vq == dev->cvq)
				continue;

			if (i < queue_pairs * 2)
				enable = true;
			else
				enable = false;

			vq->enabled = enable;
			if (dev->notify_ops->vring_state_changed)
				dev->notify_ops->vring_state_changed(dev->vid, i, enable);
		}
	}

	return ret;
}

static int
virtio_net_ctrl_push(struct virtio_net *dev, struct virtio_net_ctrl_elem *ctrl_elem)
{
	struct vhost_virtqueue *cvq = dev->cvq;
	struct vring_used_elem *used_elem;

	used_elem = &cvq->used->ring[cvq->last_used_idx];
	used_elem->id = ctrl_elem->head_idx;
	used_elem->len = ctrl_elem->n_descs;

	cvq->last_used_idx++;
	if (cvq->last_used_idx >= cvq->size)
		cvq->last_used_idx -= cvq->size;

	rte_atomic_store_explicit((unsigned short __rte_atomic *)&cvq->used->idx,
		cvq->last_used_idx, rte_memory_order_release);

	vhost_vring_call_split(dev, dev->cvq);

	free(ctrl_elem->ctrl_req);

	return 0;
}

int
virtio_net_ctrl_handle(struct virtio_net *dev)
{
	int ret = 0;

	if (dev->features & (1ULL << VIRTIO_F_RING_PACKED)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Packed ring not supported yet\n");
		return -1;
	}

	if (!dev->cvq) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "missing control queue\n");
		return -1;
	}

	rte_rwlock_read_lock(&dev->cvq->access_lock);
	vhost_user_iotlb_rd_lock(dev->cvq);

	while (1) {
		struct virtio_net_ctrl_elem ctrl_elem;

		memset(&ctrl_elem, 0, sizeof(struct virtio_net_ctrl_elem));

		ret = virtio_net_ctrl_pop(dev, dev->cvq, &ctrl_elem);
		if (ret <= 0)
			break;

		*ctrl_elem.desc_ack = virtio_net_ctrl_handle_req(dev, ctrl_elem.ctrl_req);

		ret = virtio_net_ctrl_push(dev, &ctrl_elem);
		if (ret < 0)
			break;
	}

	vhost_user_iotlb_rd_unlock(dev->cvq);
	rte_rwlock_read_unlock(&dev->cvq->access_lock);

	return ret;
}
