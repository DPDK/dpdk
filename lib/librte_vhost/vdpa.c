/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

/**
 * @file
 *
 * Device specific vhost lib
 */

#include <stdbool.h>

#include <rte_malloc.h>
#include "rte_vdpa.h"
#include "vhost.h"

static struct rte_vdpa_device *vdpa_devices[MAX_VHOST_DEVICE];
static uint32_t vdpa_device_num;

static bool
is_same_vdpa_device(struct rte_vdpa_dev_addr *a,
		struct rte_vdpa_dev_addr *b)
{
	bool ret = true;

	if (a->type != b->type)
		return false;

	switch (a->type) {
	case PCI_ADDR:
		if (a->pci_addr.domain != b->pci_addr.domain ||
				a->pci_addr.bus != b->pci_addr.bus ||
				a->pci_addr.devid != b->pci_addr.devid ||
				a->pci_addr.function != b->pci_addr.function)
			ret = false;
		break;
	default:
		break;
	}

	return ret;
}

int
rte_vdpa_register_device(struct rte_vdpa_dev_addr *addr,
		struct rte_vdpa_dev_ops *ops)
{
	struct rte_vdpa_device *dev;
	char device_name[MAX_VDPA_NAME_LEN];
	int i;

	if (vdpa_device_num >= MAX_VHOST_DEVICE || addr == NULL || ops == NULL)
		return -1;

	for (i = 0; i < MAX_VHOST_DEVICE; i++) {
		dev = vdpa_devices[i];
		if (dev && is_same_vdpa_device(&dev->addr, addr))
			return -1;
	}

	for (i = 0; i < MAX_VHOST_DEVICE; i++) {
		if (vdpa_devices[i] == NULL)
			break;
	}

	if (i == MAX_VHOST_DEVICE)
		return -1;

	snprintf(device_name, sizeof(device_name), "vdpa-dev-%d", i);
	dev = rte_zmalloc(device_name, sizeof(struct rte_vdpa_device),
			RTE_CACHE_LINE_SIZE);
	if (!dev)
		return -1;

	memcpy(&dev->addr, addr, sizeof(struct rte_vdpa_dev_addr));
	dev->ops = ops;
	vdpa_devices[i] = dev;
	vdpa_device_num++;

	return i;
}

int
rte_vdpa_unregister_device(int did)
{
	if (did < 0 || did >= MAX_VHOST_DEVICE || vdpa_devices[did] == NULL)
		return -1;

	rte_free(vdpa_devices[did]);
	vdpa_devices[did] = NULL;
	vdpa_device_num--;

	return did;
}

int
rte_vdpa_find_device_id(struct rte_vdpa_dev_addr *addr)
{
	struct rte_vdpa_device *dev;
	int i;

	if (addr == NULL)
		return -1;

	for (i = 0; i < MAX_VHOST_DEVICE; ++i) {
		dev = vdpa_devices[i];
		if (dev && is_same_vdpa_device(&dev->addr, addr))
			return i;
	}

	return -1;
}

struct rte_vdpa_device *
rte_vdpa_get_device(int did)
{
	if (did < 0 || did >= MAX_VHOST_DEVICE)
		return NULL;

	return vdpa_devices[did];
}

int
rte_vdpa_get_device_num(void)
{
	return vdpa_device_num;
}

int __rte_experimental
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
				idesc = alloc_copy_ind_table(dev, vq,
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
				vhost_log_write(dev, desc.addr, desc.len);
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
