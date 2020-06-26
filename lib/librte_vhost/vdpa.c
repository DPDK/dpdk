/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

/**
 * @file
 *
 * Device specific vhost lib
 */

#include <stdbool.h>

#include <rte_class.h>
#include <rte_malloc.h>
#include "rte_vdpa.h"
#include "vhost.h"

static struct rte_vdpa_device vdpa_devices[MAX_VHOST_DEVICE];
static uint32_t vdpa_device_num;


int
rte_vdpa_find_device_id(struct rte_vdpa_device *dev)
{
	struct rte_vdpa_device *tmp_dev;
	int i;

	if (dev == NULL)
		return -1;

	for (i = 0; i < MAX_VHOST_DEVICE; ++i) {
		tmp_dev = &vdpa_devices[i];
		if (tmp_dev->ops == NULL)
			continue;

		if (tmp_dev == dev)
			return i;
	}

	return -1;
}

struct rte_vdpa_device *
rte_vdpa_find_device_by_name(const char *name)
{
	struct rte_vdpa_device *dev;
	int i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < MAX_VHOST_DEVICE; ++i) {
		dev = &vdpa_devices[i];
		if (dev->ops == NULL)
			continue;

		if (strncmp(dev->device->name, name, RTE_DEV_NAME_MAX_LEN) == 0)
			return dev;
	}

	return NULL;
}

struct rte_device *
rte_vdpa_get_rte_device(struct rte_vdpa_device *vdpa_dev)
{
	if (vdpa_dev == NULL)
		return NULL;

	return vdpa_dev->device;
}

struct rte_vdpa_device *
rte_vdpa_get_device(int did)
{
	if (did < 0 || did >= MAX_VHOST_DEVICE)
		return NULL;

	return &vdpa_devices[did];
}

struct rte_vdpa_device *
rte_vdpa_register_device(struct rte_device *rte_dev,
		struct rte_vdpa_dev_ops *ops)
{
	struct rte_vdpa_device *dev;
	int i;

	if (vdpa_device_num >= MAX_VHOST_DEVICE || ops == NULL)
		return NULL;

	for (i = 0; i < MAX_VHOST_DEVICE; i++) {
		dev = &vdpa_devices[i];
		if (dev->ops == NULL)
			continue;

		if (dev->device == rte_dev)
			return NULL;
	}

	for (i = 0; i < MAX_VHOST_DEVICE; i++) {
		if (vdpa_devices[i].ops == NULL)
			break;
	}

	if (i == MAX_VHOST_DEVICE)
		return NULL;

	dev = &vdpa_devices[i];
	dev->device = rte_dev;
	dev->ops = ops;
	vdpa_device_num++;

	return dev;
}

int
rte_vdpa_unregister_device(struct rte_vdpa_device *vdev)
{
	int did = rte_vdpa_find_device_id(vdev);

	if (did < 0 || vdpa_devices[did].ops == NULL)
		return -1;

	memset(&vdpa_devices[did], 0, sizeof(struct rte_vdpa_device));
	vdpa_device_num--;

	return 0;
}

int
rte_vdpa_get_device_num(void)
{
	return vdpa_device_num;
}

int
rte_vdpa_relay_vring_used(int vid, uint16_t qid, void *vring_m)
{
	struct virtio_net *dev = get_device(vid);
	uint16_t idx, idx_m, desc_id;
	struct vhost_virtqueue *vq;
	struct vring_desc desc;
	struct vring_desc *desc_ring;
	struct vring_desc *idesc = NULL;
	struct vring *s_vring;
	uint64_t dlen;
	uint32_t nr_descs;
	int ret;

	if (!dev || !vring_m)
		return -1;

	if (qid >= dev->nr_vring)
		return -1;

	if (vq_is_packed(dev))
		return -1;

	s_vring = (struct vring *)vring_m;
	vq = dev->virtqueue[qid];
	idx = vq->used->idx;
	idx_m = s_vring->used->idx;
	ret = (uint16_t)(idx_m - idx);

	while (idx != idx_m) {
		/* copy used entry, used ring logging is not covered here */
		vq->used->ring[idx & (vq->size - 1)] =
			s_vring->used->ring[idx & (vq->size - 1)];

		desc_id = vq->used->ring[idx & (vq->size - 1)].id;
		desc_ring = vq->desc;
		nr_descs = vq->size;

		if (unlikely(desc_id >= vq->size))
			return -1;

		if (vq->desc[desc_id].flags & VRING_DESC_F_INDIRECT) {
			dlen = vq->desc[desc_id].len;
			nr_descs = dlen / sizeof(struct vring_desc);
			if (unlikely(nr_descs > vq->size))
				return -1;

			desc_ring = (struct vring_desc *)(uintptr_t)
				vhost_iova_to_vva(dev, vq,
						vq->desc[desc_id].addr, &dlen,
						VHOST_ACCESS_RO);
			if (unlikely(!desc_ring))
				return -1;

			if (unlikely(dlen < vq->desc[desc_id].len)) {
				idesc = vhost_alloc_copy_ind_table(dev, vq,
						vq->desc[desc_id].addr,
						vq->desc[desc_id].len);
				if (unlikely(!idesc))
					return -1;

				desc_ring = idesc;
			}

			desc_id = 0;
		}

		/* dirty page logging for DMA writeable buffer */
		do {
			if (unlikely(desc_id >= vq->size))
				goto fail;
			if (unlikely(nr_descs-- == 0))
				goto fail;
			desc = desc_ring[desc_id];
			if (desc.flags & VRING_DESC_F_WRITE)
				vhost_log_write_iova(dev, vq, desc.addr,
						     desc.len);
			desc_id = desc.next;
		} while (desc.flags & VRING_DESC_F_NEXT);

		if (unlikely(idesc)) {
			free_ind_table(idesc);
			idesc = NULL;
		}

		idx++;
	}

	rte_smp_wmb();
	vq->used->idx = idx_m;

	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX))
		vring_used_event(s_vring) = idx_m;

	return ret;

fail:
	if (unlikely(idesc))
		free_ind_table(idesc);
	return -1;
}

int
rte_vdpa_get_stats_names(struct rte_vdpa_device *dev,
		struct rte_vdpa_stat_name *stats_names,
		unsigned int size)
{
	if (!dev)
		return -EINVAL;

	RTE_FUNC_PTR_OR_ERR_RET(dev->ops->get_stats_names, -ENOTSUP);

	return dev->ops->get_stats_names(dev, stats_names, size);
}

int
rte_vdpa_get_stats(struct rte_vdpa_device *dev, uint16_t qid,
		struct rte_vdpa_stat *stats, unsigned int n)
{
	if (!dev || !stats || !n)
		return -EINVAL;

	RTE_FUNC_PTR_OR_ERR_RET(dev->ops->get_stats, -ENOTSUP);

	return dev->ops->get_stats(dev, qid, stats, n);
}

int
rte_vdpa_reset_stats(struct rte_vdpa_device *dev, uint16_t qid)
{
	if (!dev)
		return -EINVAL;

	RTE_FUNC_PTR_OR_ERR_RET(dev->ops->reset_stats, -ENOTSUP);

	return dev->ops->reset_stats(dev, qid);
}

static uint16_t
vdpa_dev_to_id(const struct rte_vdpa_device *dev)
{
	if (dev == NULL)
		return MAX_VHOST_DEVICE;

	if (dev < &vdpa_devices[0] ||
			dev >= &vdpa_devices[MAX_VHOST_DEVICE])
		return MAX_VHOST_DEVICE;

	return (uint16_t)(dev - vdpa_devices);
}

static int
vdpa_dev_match(struct rte_vdpa_device *dev,
	      const struct rte_device *rte_dev)
{
	if (dev->device == rte_dev)
		return 0;

	return -1;
}

/* Generic rte_vdpa_dev comparison function. */
typedef int (*rte_vdpa_cmp_t)(struct rte_vdpa_device *,
		const struct rte_device *rte_dev);

static struct rte_vdpa_device *
vdpa_find_device(const struct rte_vdpa_device *start, rte_vdpa_cmp_t cmp,
		struct rte_device *rte_dev)
{
	struct rte_vdpa_device *dev;
	uint16_t idx;

	if (start != NULL)
		idx = vdpa_dev_to_id(start) + 1;
	else
		idx = 0;
	for (; idx < MAX_VHOST_DEVICE; idx++) {
		dev = &vdpa_devices[idx];
		/*
		 * ToDo: Certainly better to introduce a state field,
		 * but rely on ops being set for now.
		 */
		if (dev->ops == NULL)
			continue;
		if (cmp(dev, rte_dev) == 0)
			return dev;
	}
	return NULL;
}

static void *
vdpa_dev_iterate(const void *start,
		const char *str,
		const struct rte_dev_iterator *it)
{
	struct rte_vdpa_device *vdpa_dev = NULL;

	RTE_SET_USED(str);

	vdpa_dev = vdpa_find_device(start, vdpa_dev_match, it->device);

	return vdpa_dev;
}

static struct rte_class rte_class_vdpa = {
	.dev_iterate = vdpa_dev_iterate,
};

RTE_REGISTER_CLASS(vdpa, rte_class_vdpa);
