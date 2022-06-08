/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef _VIRTIO_VDPA_H_
#define _VIRTIO_VDPA_H_

enum {
	VIRTIO_VDPA_NOTIFIER_STATE_DISABLED,
	VIRTIO_VDPA_NOTIFIER_STATE_ENABLED,
	VIRTIO_VDPA_NOTIFIER_STATE_ERR
};

struct virtio_vdpa_vring_info {
	uint64_t desc;
	uint64_t avail;
	uint64_t used;
	uint16_t size;
	uint16_t index;
	uint8_t notifier_state;
	bool enable;
	struct rte_intr_handle *intr_handle;
	struct virtio_vdpa_priv *priv;
};

#define VIRTIO_VDPA_DRIVER_NAME vdpa_virtio

struct virtio_vdpa_device_callback {
	void (*vhost_feature_get)(uint64_t *features);
};

struct virtio_vdpa_priv {
	TAILQ_ENTRY(virtio_vdpa_priv) next;
	struct rte_pci_device *pdev;
	struct rte_vdpa_device *vdev;
	struct virtio_pci_dev *vpdev;
	const struct virtio_vdpa_device_callback *dev_ops;
	int vfio_container_fd;
	int vfio_group_fd;
	int vfio_dev_fd;
	int vid;
	int nvec;
	uint64_t guest_features;
	struct virtio_vdpa_vring_info **vrings;
	uint16_t nr_virtqs;   /* Number of vq vhost enabled */
	uint16_t hw_nr_virtqs; /* Number of vq device supported */
	bool configured;
};

#endif /* _VIRTIO_VDPA_H_ */
