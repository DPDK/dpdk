/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef _VIRTIO_LM_H_
#define _VIRTIO_LM_H_

struct virtio_vdpa_pf_priv;

struct virtio_vdpa_pf_info {
	char pf_name[RTE_DEV_NAME_MAX_LEN];
};

__rte_internal int
virtio_vdpa_cmd_identity(struct virtio_vdpa_pf_priv *priv,
		struct virtio_admin_migration_identity_result *result);
__rte_internal int
virtio_vdpa_cmd_set_status(struct virtio_vdpa_pf_priv *priv, uint16_t vdev_id,
		enum virtio_internal_status status);
__rte_internal int
virtio_vdpa_cmd_get_status(struct virtio_vdpa_pf_priv *priv, uint16_t vdev_id,
		enum virtio_internal_status *status);
__rte_internal int
virtio_vdpa_cmd_save_state(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id, uint64_t offset, uint64_t length,
		rte_iova_t out_data);
__rte_internal int
virtio_vdpa_cmd_restore_state(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id, uint64_t offset, uint64_t length,
		rte_iova_t data);
__rte_internal int
virtio_vdpa_cmd_get_internal_pending_bytes(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id,
		struct virtio_admin_migration_get_internal_state_pending_bytes_result *result);
__rte_internal int
virtio_vdpa_cmd_dirty_page_identity(struct virtio_vdpa_pf_priv *priv,
		struct virtio_admin_dirty_page_identity_result *result);
__rte_internal int
virtio_vdpa_cmd_dirty_page_start_track(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id,
		enum virtio_dirty_track_mode track_mode,
		uint32_t vdev_host_page_size,
		uint64_t vdev_host_range_addr,
		uint64_t range_length,
		int num_sges,
		struct virtio_sge data[]);
__rte_internal int
virtio_vdpa_cmd_dirty_page_stop_track(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id, uint64_t vdev_host_range_addr);
__rte_internal int
virtio_vdpa_cmd_dirty_page_get_map_pending_bytes(
		struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id,
		uint64_t vdev_host_range_addr,
		struct virtio_admin_dirty_page_get_map_pending_bytes_result *result);
__rte_internal int
virtio_vdpa_cmd_dirty_page_report_map(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id,
		uint64_t offset,
		uint64_t length,
		uint64_t vdev_host_range_addr,
		rte_iova_t data);
struct virtio_vdpa_pf_priv *
rte_vdpa_get_mi_by_bdf(const char *bdf);
int
rte_vdpa_pf_dev_add(const char *pf_name);
int
rte_vdpa_pf_dev_remove(const char *pf_name);
int
rte_vdpa_get_pf_list(struct virtio_vdpa_pf_info *pf_info, int max_pf_num);
void
rte_vdpa_pf_ctrl_ctx_remove(bool enable);

#endif /* _VIRTIO_LM_H_ */
