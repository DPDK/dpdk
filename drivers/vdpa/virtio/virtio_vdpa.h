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
	bool vector_enable;
	struct rte_intr_handle *intr_handle;
	struct virtio_vdpa_priv *priv;
};

#define VIRTIO_VDPA_DRIVER_NAME vdpa_virtio

struct virtio_vdpa_priv {
	TAILQ_ENTRY(virtio_vdpa_priv) next;
	const struct rte_memzone *vdpa_dp_map;
	struct virtio_vdpa_pf_priv *pf_priv;
	struct rte_pci_device *pdev;
	struct rte_vdpa_device *vdev;
	struct virtio_pci_dev *vpdev;
	const struct rte_memzone *state_mz; /* This is used to formmat state  at local */
	const struct rte_memzone *state_mz_remote; /* This is used get state frome contoller */
	const struct virtio_vdpa_device_callback *dev_ops;
	struct rte_vhost_memory *mem;
	enum virtio_internal_status lm_status;
	int state_size;
	int vfio_container_fd;
	int vfio_group_fd;
	int vfio_dev_fd;
	int vid;
	int vf_id;
	int nvec;
	int lcore_id;
	int dev_work_flag;
	uint64_t guest_features;
	struct virtio_vdpa_vring_info **vrings;
	uint16_t hw_nr_virtqs; /* Number of vq device supported */
	bool configured;
	bool dev_conf_read;
};

#define VIRTIO_VDPA_REMOTE_STATE_DEFAULT_SIZE 8192
#define VIRTIO_VDPA_INTR_RETRIES_USEC 1000
#define VIRTIO_VDPA_INTR_RETRIES 256

struct virtio_vdpa_device_callback {
	void (*vhost_feature_get)(uint64_t *features);
	int (*dirty_desc_get)(int vid, int qix, uint64_t *desc_addr, uint32_t *write_len);
	int (*reg_dev_intr)(struct virtio_vdpa_priv* priv);
	int (*unreg_dev_intr)(struct virtio_vdpa_priv* priv);
	int (*vdpa_queue_num_unit_get)(void);
	void (*add_vdpa_feature)(uint64_t *features);
};

int virtio_vdpa_dev_pf_filter_dump(struct vdpa_vf_params *vf_info, int max_vf_num, struct virtio_vdpa_pf_priv *pf_priv);
int virtio_vdpa_dev_vf_filter_dump(const char *vf_name, struct vdpa_vf_params *vf_info);
struct virtio_vdpa_priv * virtio_vdpa_find_priv_resource_by_name(const char *vf_name);
int virtio_vdpa_max_phy_addr_get(struct virtio_vdpa_priv *priv, uint64_t *phy_addr);
int virtio_vdpa_dirty_desc_get(struct virtio_vdpa_priv *priv, int qix, uint64_t *desc_addr, uint32_t *write_len);
int virtio_vdpa_used_vring_addr_get(struct virtio_vdpa_priv *priv, int qix, uint64_t *used_vring_addr, uint32_t *used_vring_len);
const struct rte_memzone * virtio_vdpa_dev_dp_map_get(struct virtio_vdpa_priv *priv, size_t len);
uint64_t virtio_vdpa_gpa_to_hva(int vid, uint64_t gpa);
#endif /* _VIRTIO_VDPA_H_ */
