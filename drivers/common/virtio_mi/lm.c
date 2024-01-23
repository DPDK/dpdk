/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */
#include <unistd.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_bus_pci.h>
#include <rte_vfio.h>
#include <rte_kvargs.h>
#include <rte_eal_paging.h>
#include <rte_ether.h>

#include <virtqueue.h>
#include <virtio_admin.h>
#include <virtio_api.h>
#include <virtio_lm.h>
#include <virtio_ha.h>

#define VIRTIO_VDPA_MI_SUPPORTED_NET_FEATURES \
										(1ULL << VIRTIO_F_ADMIN_VQ | \
										1ULL << VIRTIO_F_IOMMU_PLATFORM | \
										1ULL << VIRTIO_F_VERSION_1)

#define VIRTIO_VDPA_MI_SUPPORTED_BLK_FEATURES \
										(1ULL << VIRTIO_F_ADMIN_VQ | \
										1ULL << VIRTIO_F_IOMMU_PLATFORM | \
										1ULL << VIRTIO_F_VERSION_1)

#define VIRTIO_VDPA_MI_MAX_SGES 32
#define VIRTIO_VDPA_MI_GET_GROUP_RETRIES 120

struct virtio_vdpa_pf_priv;
struct virtio_vdpa_dev_ops {
	uint64_t (*get_required_features)(void);
	uint16_t (*get_adminq_idx)(struct virtio_vdpa_pf_priv *priv);
};

struct virtio_vdpa_pf_priv {
	TAILQ_ENTRY(virtio_vdpa_pf_priv) next;
	struct rte_pci_device *pdev;
	struct virtio_pci_dev *vpdev;
	struct virtio_vdpa_dev_ops *dev_ops;
	uint64_t device_features;
	int vfio_dev_fd;
	uint16_t hw_nr_virtqs; /* number of vq device supported*/
	struct virtio_dev_name pf_name;
};

struct sge_iova {
	rte_iova_t iova;
	uint32_t len;
};

struct virtio_admin_data_ctrl{
	uint16_t num_in_data;
	uint16_t num_out_data;
	struct sge_iova in_data[VIRTIO_VDPA_MI_MAX_SGES];
	struct sge_iova out_data[VIRTIO_VDPA_MI_MAX_SGES];
};

struct virtio_ha_pf_drv_ctx {
	struct virtio_dev_name pf_name;
	int vfio_group_fd;
	int vfio_device_fd;
};

RTE_LOG_REGISTER(virtio_vdpa_mi_logtype, pmd.vdpa.virtio, NOTICE);
#define DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_vdpa_mi_logtype, \
		"VIRTIO VDPA MI %s(): " fmt "\n", __func__, ##args)

RTE_LOG_REGISTER(virtio_vdpa_cmd_logtype, pmd.vdpa.virtio, NOTICE);
#define CMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_vdpa_cmd_logtype, \
		"VIRTIO VDPA CMD %s(): " fmt "\n", __func__, ##args)

TAILQ_HEAD(virtio_vdpa_mi_privs, virtio_vdpa_pf_priv) virtio_mi_priv_list =
						TAILQ_HEAD_INITIALIZER(virtio_mi_priv_list);
static pthread_mutex_t mi_priv_list_lock = PTHREAD_MUTEX_INITIALIZER;
static struct virtio_ha_pf_drv_ctx cached_ctx;
static bool ctx_remove_enabled;

static struct virtio_admin_ctrl *
virtio_vdpa_send_admin_command_split(struct virtadmin_ctl *avq,
		struct virtio_admin_ctrl *ctrl,
		struct virtio_admin_data_ctrl *dat_ctrl,
		int *dlen, int pkt_num)
{
	struct virtqueue *vq = virtnet_aq_to_vq(avq);
	struct virtio_admin_ctrl *result;
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
	vq->vq_split.ring.desc[head].addr = avq->virtio_admin_hdr_mem;
	vq->vq_split.ring.desc[head].len = sizeof(struct virtio_admin_ctrl_hdr);
	vq->vq_free_cnt--;
	i = vq->vq_split.ring.desc[head].next;

	for (k = 0; k < pkt_num; k++) {
		vq->vq_split.ring.desc[i].flags = VRING_DESC_F_NEXT;
		vq->vq_split.ring.desc[i].addr = avq->virtio_admin_hdr_mem
			+ sizeof(struct virtio_admin_ctrl_hdr)
			+ sizeof(ctrl->status) + sizeof(uint8_t)*sum;
		vq->vq_split.ring.desc[i].len = dlen[k];
		sum += dlen[k];
		vq->vq_free_cnt--;
		i = vq->vq_split.ring.desc[i].next;
	}

	for (k = 0; k < dat_ctrl->num_in_data; k++) {
		vq->vq_split.ring.desc[i].flags = VRING_DESC_F_NEXT;
		vq->vq_split.ring.desc[i].addr = dat_ctrl->in_data[k].iova;
		vq->vq_split.ring.desc[i].len = dat_ctrl->in_data[k].len;
		vq->vq_free_cnt--;
		i = vq->vq_split.ring.desc[i].next;
	}

	for (k = 0; k < dat_ctrl->num_out_data; k++) {
		vq->vq_split.ring.desc[i].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
		vq->vq_split.ring.desc[i].addr = dat_ctrl->out_data[k].iova;
		vq->vq_split.ring.desc[i].len = dat_ctrl->out_data[k].len;
		vq->vq_free_cnt--;
		i = vq->vq_split.ring.desc[i].next;
	}

	vq->vq_split.ring.desc[i].flags = VRING_DESC_F_WRITE;
	vq->vq_split.ring.desc[i].addr = avq->virtio_admin_hdr_mem
			+ sizeof(struct virtio_admin_ctrl_hdr);
	vq->vq_split.ring.desc[i].len = sizeof(ctrl->status);
	vq->vq_free_cnt--;

	vq->vq_desc_head_idx = vq->vq_split.ring.desc[i].next;

	vq_update_avail_ring(vq, head);
	vq_update_avail_idx(vq);

	virtqueue_notify(vq);

	while (virtqueue_nused(vq) == 0) {
		usleep(100);
	}

	while (virtqueue_nused(vq)) {
		uint32_t idx, desc_idx, used_idx;
		struct vring_used_elem *uep;

		used_idx = (uint32_t)(vq->vq_used_cons_idx
				& (vq->vq_nentries - 1));
		uep = &vq->vq_split.ring.used->ring[used_idx];
		idx = (uint32_t) uep->id;
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

	DRV_LOG(DEBUG, "vq->vq_free_cnt=%d\nvq->vq_desc_head_idx=%d",
			vq->vq_free_cnt, vq->vq_desc_head_idx);

	result = avq->virtio_admin_hdr_mz->addr;
	return result;
}

static int
virtio_vdpa_send_admin_command(struct virtadmin_ctl *avq,
		struct virtio_admin_ctrl *ctrl,
		struct virtio_admin_data_ctrl *dat_ctrl,
		int *dlen,
		int pkt_num)
{
	virtio_admin_ctrl_ack status = ~0;
	struct virtio_admin_ctrl *result;
	struct virtqueue *vq;
	int i;

	if (!avq) {
		DRV_LOG(ERR, "Admin queue is not supported");
		return -1;
	}

	ctrl->status = status;
	vq = virtnet_aq_to_vq(avq);

	DRV_LOG(DEBUG, "vq->vq_desc_head_idx = %d, status = %d, "
		"vq->hw->avq = %p vq = %p",
		vq->vq_desc_head_idx, status, vq->hw->avq, vq);

	if (vq->vq_free_cnt < pkt_num + 2) {
		return -1;
	}

	for (i = 0; i < VIRTIO_ADMIN_CMD_RETRY_CNT; i++) {
		result = virtio_vdpa_send_admin_command_split(avq, ctrl, dat_ctrl,
			dlen, pkt_num);
		if(result->status && (!(result->status & VIRTIO_ADMIN_CMD_STATUS_DNR_BIT))) {
			DRV_LOG(DEBUG, "No:%d cmd status:0x%x, submit again after 100ms", i, result->status);
			usleep(100000);
		}
		else
			break;
	}

	return result->status;
}

int
virtio_vdpa_cmd_identity(struct virtio_vdpa_pf_priv *priv,
		struct virtio_admin_migration_identity_result *result)
{
	struct virtio_admin_migration_identity_result *res;
	struct virtio_hw *hw = &priv->vpdev->hw;
	struct virtio_admin_data_ctrl dat_ctrl;
	struct virtio_admin_ctrl *ctrl;
	int dlen[1];
	int ret;

	RTE_VERIFY(priv);

	if (!virtio_with_feature(hw, VIRTIO_F_ADMIN_VQ)) {
		CMD_LOG(INFO, "host does not support admin queue");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&hw->avq->lock);

	ctrl = virtnet_get_aq_hdr_addr(hw->avq);
	ctrl->hdr.class = VIRTIO_ADMIN_PCI_MIGRATION_CTRL;
	ctrl->hdr.cmd = VIRTIO_ADMIN_PCI_MIGRATION_IDENTITY;
	dlen[0] = 0;
	dat_ctrl.num_in_data = 0;
	dat_ctrl.num_out_data = 1;
	dat_ctrl.out_data[0].iova = hw->avq->virtio_admin_hdr_mem
				+ sizeof(struct virtio_admin_ctrl_hdr)
				+ sizeof(ctrl->status);
	dat_ctrl.out_data[0].len = sizeof(*result);

	ret = virtio_vdpa_send_admin_command(hw->avq, ctrl, &dat_ctrl, dlen, 0);
	if (ret) {
		CMD_LOG(ERR, "Failed to run class %u, cmd %u, status %d", ctrl->hdr.class, ctrl->hdr.cmd, ret);
		rte_spinlock_unlock(&hw->avq->lock);
		return ret;
	}

	res = (struct virtio_admin_migration_identity_result *)
			rte_mem_iova2virt(dat_ctrl.out_data[0].iova);
	result->major_ver = rte_le_to_cpu_16(res->major_ver);
	result->minor_ver = rte_le_to_cpu_16(res->minor_ver);
	result->ter_ver   = rte_le_to_cpu_16(res->ter_ver);

	rte_spinlock_unlock(&hw->avq->lock);
	return ret;
}

int
virtio_vdpa_cmd_get_status(struct virtio_vdpa_pf_priv *priv, uint16_t vdev_id,
		enum virtio_internal_status *status)
{
	struct virtio_admin_migration_get_internal_status_data *sd;
	struct virtio_admin_migration_get_internal_status_result *result;
	struct virtio_admin_data_ctrl dat_ctrl;
	struct virtio_admin_ctrl *ctrl;
	struct virtio_hw *hw;
	int dlen[1];
	int ret;

	RTE_VERIFY(priv);
	hw = &priv->vpdev->hw;
	if (!virtio_with_feature(hw, VIRTIO_F_ADMIN_VQ)) {
		CMD_LOG(INFO, "host does not support admin queue");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&hw->avq->lock);

	ctrl = virtnet_get_aq_hdr_addr(hw->avq);
	ctrl->hdr.class = VIRTIO_ADMIN_PCI_MIGRATION_CTRL;
	ctrl->hdr.cmd = VIRTIO_ADMIN_PCI_MIGRATION_GET_INTERNAL_STATUS;
	sd = (struct virtio_admin_migration_get_internal_status_data *)&ctrl->data[0];
	sd->vdev_id = rte_cpu_to_le_16(vdev_id);
	dlen[0] = sizeof(*sd);
	dat_ctrl.num_in_data = 0;
	dat_ctrl.num_out_data = 1;
	dat_ctrl.out_data[0].iova = hw->avq->virtio_admin_hdr_mem
			+ sizeof(struct virtio_admin_ctrl_hdr)
			+ sizeof(ctrl->status) + dlen[0];
	dat_ctrl.out_data[0].len = sizeof(*result);
	ret = virtio_vdpa_send_admin_command(hw->avq, ctrl, &dat_ctrl, dlen, 1);
	if (ret) {
		CMD_LOG(ERR, "Failed to run class %u, cmd %u, status %d, vdev_id: %u",
				ctrl->hdr.class, ctrl->hdr.cmd, ret, vdev_id);
		rte_spinlock_unlock(&hw->avq->lock);
		return ret;
	}

	result = (struct virtio_admin_migration_get_internal_status_result *)
			rte_mem_iova2virt(dat_ctrl.out_data[0].iova);
	result->internal_status = rte_le_to_cpu_16(result->internal_status);
	RTE_VERIFY(result->internal_status <= VIRTIO_S_FREEZED);
	*status = (enum virtio_internal_status)result->internal_status;

	rte_spinlock_unlock(&hw->avq->lock);
	return 0;
}

int
virtio_vdpa_cmd_set_status(struct virtio_vdpa_pf_priv *priv, uint16_t vdev_id,
		enum virtio_internal_status status)
{
	struct virtio_admin_migration_modify_internal_status_data *sd;
	struct virtio_admin_data_ctrl dat_ctrl;
	struct virtio_admin_ctrl *ctrl;
	struct virtio_hw *hw;
	int dlen[1];
	int ret;

	RTE_VERIFY(priv);
	hw = &priv->vpdev->hw;
	if (!virtio_with_feature(hw, VIRTIO_F_ADMIN_VQ)) {
		CMD_LOG(INFO, "host does not support admin queue");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&hw->avq->lock);

	ctrl = virtnet_get_aq_hdr_addr(hw->avq);
	ctrl->hdr.class = VIRTIO_ADMIN_PCI_MIGRATION_CTRL;
	ctrl->hdr.cmd = VIRTIO_ADMIN_PCI_MIGRATION_MODIFY_INTERNAL_STATUS;
	sd = (struct virtio_admin_migration_modify_internal_status_data *)&ctrl->data[0];
	sd->vdev_id = rte_cpu_to_le_16(vdev_id);
	sd->internal_status = rte_cpu_to_le_16((uint16_t)status);
	dlen[0] = sizeof(*sd);
	dat_ctrl.num_in_data = 0;
	dat_ctrl.num_out_data = 0;

	ret = virtio_vdpa_send_admin_command(hw->avq, ctrl, &dat_ctrl, dlen, 1);
	if (ret) {
		CMD_LOG(ERR, "Failed to run class %u, cmd %u, status %d, vdev_id: %u",
				ctrl->hdr.class, ctrl->hdr.cmd, ret, vdev_id);
		rte_spinlock_unlock(&hw->avq->lock);
		return ret;
	}

	rte_spinlock_unlock(&hw->avq->lock);
	return 0;
}

int
virtio_vdpa_cmd_save_state(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id, uint64_t offset, uint64_t length,
		rte_iova_t out_data)
{
	struct virtio_admin_migration_save_internal_state_data *sd;
	struct virtio_hw *hw = &priv->vpdev->hw;
	struct virtio_admin_data_ctrl dat_ctrl;
	struct virtio_admin_ctrl *ctrl;
	int dlen[1];
	int ret;

	RTE_VERIFY(priv);

	if (!virtio_with_feature(hw, VIRTIO_F_ADMIN_VQ)) {
		CMD_LOG(INFO, "host does not support admin queue");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&hw->avq->lock);

	ctrl = virtnet_get_aq_hdr_addr(hw->avq);
	ctrl->hdr.class = VIRTIO_ADMIN_PCI_MIGRATION_CTRL;
	ctrl->hdr.cmd = VIRTIO_ADMIN_PCI_MIGRATION_SAVE_INTERNAL_STATE;
	sd = (struct virtio_admin_migration_save_internal_state_data *)&ctrl->data[0];
	sd->vdev_id = rte_cpu_to_le_16(vdev_id);
	sd->offset = rte_cpu_to_le_64(offset);
	sd->length = rte_cpu_to_le_64(length);
	dlen[0] = sizeof(*sd);
	dat_ctrl.num_in_data = 0;
	dat_ctrl.num_out_data = 1;
	dat_ctrl.out_data[0].iova = out_data;
	dat_ctrl.out_data[0].len = length;
	ret = virtio_vdpa_send_admin_command(hw->avq, ctrl, &dat_ctrl, dlen, 1);
	if (ret) {
		CMD_LOG(ERR, "Failed to run class %u, cmd %u, status %d, vdev_id: %u",
				ctrl->hdr.class, ctrl->hdr.cmd, ret, vdev_id);
		rte_spinlock_unlock(&hw->avq->lock);
		return ret;
	}

	rte_spinlock_unlock(&hw->avq->lock);
	return 0;
}

int
virtio_vdpa_cmd_restore_state(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id, uint64_t offset, uint64_t length,
		rte_iova_t data)
{
	struct virtio_admin_migration_restore_internal_state_data *sd;
	struct virtio_hw *hw = &priv->vpdev->hw;
	struct virtio_admin_data_ctrl dat_ctrl;
	struct virtio_admin_ctrl *ctrl;
	int dlen[1];
	int ret;

	RTE_VERIFY(priv);

	if (!virtio_with_feature(hw, VIRTIO_F_ADMIN_VQ)) {
		CMD_LOG(INFO, "host does not support admin queue");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&hw->avq->lock);

	ctrl = virtnet_get_aq_hdr_addr(hw->avq);
	ctrl->hdr.class = VIRTIO_ADMIN_PCI_MIGRATION_CTRL;
	ctrl->hdr.cmd = VIRTIO_ADMIN_PCI_MIGRATION_RESTORE_INTERNAL_STATE;
	sd = (struct virtio_admin_migration_restore_internal_state_data *)&ctrl->data[0];
	sd->vdev_id = rte_cpu_to_le_16(vdev_id);
	sd->offset = rte_cpu_to_le_64(offset);
	sd->length = rte_cpu_to_le_64(length);
	dlen[0] = sizeof(*sd);
	dat_ctrl.num_in_data = 1;
	dat_ctrl.in_data[0].iova = data;
	dat_ctrl.in_data[0].len = length;
	dat_ctrl.num_out_data = 0;
	ret = virtio_vdpa_send_admin_command(hw->avq, ctrl, &dat_ctrl, dlen, 1);
	if (ret) {
		CMD_LOG(ERR, "Failed to run class %u, cmd %u, status %d, vdev_id: %u",
				ctrl->hdr.class, ctrl->hdr.cmd, ret, vdev_id);
		rte_spinlock_unlock(&hw->avq->lock);
		return ret;
	}

	rte_spinlock_unlock(&hw->avq->lock);
	return 0;
}

int
virtio_vdpa_cmd_get_internal_pending_bytes(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id,
		struct virtio_admin_migration_get_internal_state_pending_bytes_result *result)
{
	struct virtio_admin_migration_get_internal_state_pending_bytes_result *res;
	struct virtio_admin_migration_get_internal_state_pending_bytes_data *sd;
	struct virtio_hw *hw = &priv->vpdev->hw;
	struct virtio_admin_data_ctrl dat_ctrl;
	struct virtio_admin_ctrl *ctrl;
	int dlen[1];
	int ret;

	RTE_VERIFY(priv);

	if (!virtio_with_feature(hw, VIRTIO_F_ADMIN_VQ)) {
		CMD_LOG(INFO, "host does not support admin queue");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&hw->avq->lock);

	ctrl = virtnet_get_aq_hdr_addr(hw->avq);
	ctrl->hdr.class = VIRTIO_ADMIN_PCI_MIGRATION_CTRL;
	ctrl->hdr.cmd = VIRTIO_ADMIN_PCI_MIGRATION_GET_INTERNAL_STATE_PENDING_BYTES;
	sd = (struct virtio_admin_migration_get_internal_state_pending_bytes_data *)&ctrl->data[0];
	sd->vdev_id = rte_cpu_to_le_16(vdev_id);
	dlen[0] = sizeof(*sd);

	dat_ctrl.num_in_data = 0;
	dat_ctrl.num_out_data = 1;
	dat_ctrl.out_data[0].iova = hw->avq->virtio_admin_hdr_mem
			+ sizeof(struct virtio_admin_ctrl_hdr)
			+ sizeof(ctrl->status) + dlen[0];
	dat_ctrl.out_data[0].len = sizeof(*result);
	ret = virtio_vdpa_send_admin_command(hw->avq, ctrl, &dat_ctrl, dlen, 1);
	if (ret) {
		CMD_LOG(ERR, "Failed to run class %u, cmd %u, status %d, vdev_id: %u",
				ctrl->hdr.class, ctrl->hdr.cmd, ret, vdev_id);
		rte_spinlock_unlock(&hw->avq->lock);
		return ret;
	}

	res = (struct virtio_admin_migration_get_internal_state_pending_bytes_result *)
			rte_mem_iova2virt(dat_ctrl.out_data[0].iova);
	result->pending_bytes = rte_le_to_cpu_64(res->pending_bytes);

	rte_spinlock_unlock(&hw->avq->lock);
	return 0;
}

int
virtio_vdpa_cmd_dirty_page_identity(struct virtio_vdpa_pf_priv *priv,
		struct virtio_admin_dirty_page_identity_result *result)
{
	struct virtio_admin_dirty_page_identity_result *res;
	struct virtio_hw *hw = &priv->vpdev->hw;
	struct virtio_admin_data_ctrl dat_ctrl;
	struct virtio_admin_ctrl *ctrl;
	int dlen[1];
	int ret;

	RTE_VERIFY(priv);

	if (!virtio_with_feature(hw, VIRTIO_F_ADMIN_VQ)) {
		CMD_LOG(INFO, "host does not support admin queue");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&hw->avq->lock);

	ctrl = virtnet_get_aq_hdr_addr(hw->avq);
	ctrl->hdr.class = VIRTIO_ADMIN_PCI_DIRTY_PAGE_TRACK_CTRL;
	ctrl->hdr.cmd = VIRTIO_ADMIN_PCI_DIRTY_PAGE_IDENTITY;
	dlen[0] = 0;
	dat_ctrl.num_in_data = 0;
	dat_ctrl.num_out_data = 1;
	dat_ctrl.out_data[0].iova = hw->avq->virtio_admin_hdr_mem
				+ sizeof(struct virtio_admin_ctrl_hdr)
				+ sizeof(ctrl->status);
	dat_ctrl.out_data[0].len = sizeof(*result);

	ret = virtio_vdpa_send_admin_command(hw->avq, ctrl, &dat_ctrl, dlen, 0);
	if (ret) {
		CMD_LOG(ERR, "Failed to run class %u, cmd %u, status %d",
				ctrl->hdr.class, ctrl->hdr.cmd, ret);
		rte_spinlock_unlock(&hw->avq->lock);
		return ret;
	}

	res = (struct virtio_admin_dirty_page_identity_result *)
			rte_mem_iova2virt(dat_ctrl.out_data[0].iova);
	result->log_max_pages_track_pull_bitmap_mode =
			rte_le_to_cpu_16(res->log_max_pages_track_pull_bitmap_mode);
	result->log_max_pages_track_pull_bytemap_mode =
			rte_le_to_cpu_16(res->log_max_pages_track_pull_bytemap_mode);
	result->max_track_ranges = rte_le_to_cpu_32(res->max_track_ranges);

	rte_spinlock_unlock(&hw->avq->lock);
	return 0;
}

int
virtio_vdpa_cmd_dirty_page_start_track(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id,
		enum virtio_dirty_track_mode track_mode,
		uint32_t vdev_host_page_size,
		uint64_t vdev_host_range_addr,
		uint64_t range_length,
		int num_sges,
		struct virtio_sge data[])
{
	struct virtio_admin_dirty_page_start_track_data *sd;
	struct virtio_hw *hw = &priv->vpdev->hw;
	struct virtio_admin_ctrl *ctrl;
	struct virtio_admin_data_ctrl dat_ctrl;
	int dlen[1];
	int ret, i;

	RTE_VERIFY(priv);
	RTE_VERIFY(num_sges <= VIRTIO_VDPA_MI_MAX_SGES);

	if (!virtio_with_feature(hw, VIRTIO_F_ADMIN_VQ)) {
		CMD_LOG(INFO, "host does not support admin queue");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&hw->avq->lock);

	ctrl = virtnet_get_aq_hdr_addr(hw->avq);
	ctrl->hdr.class = VIRTIO_ADMIN_PCI_DIRTY_PAGE_TRACK_CTRL;
	ctrl->hdr.cmd = VIRTIO_ADMIN_PCI_DIRTY_PAGE_START_TRACK;
	sd = (struct virtio_admin_dirty_page_start_track_data *)&ctrl->data[0];
	sd->vdev_id = rte_cpu_to_le_16(vdev_id);
	sd->track_mode = rte_cpu_to_le_16((uint16_t)track_mode);
	sd->vdev_host_page_size = rte_cpu_to_le_32(vdev_host_page_size);
	sd->vdev_host_range_addr = rte_cpu_to_le_64(vdev_host_range_addr);
	sd->range_length = rte_cpu_to_le_64(range_length);
	for (i = 0; i < num_sges; i++) {
		sd->sges[i].addr = rte_cpu_to_le_64((uint64_t)rte_mem_virt2iova((const void *)data[i].addr));
		sd->sges[i].len = rte_cpu_to_le_32(data[i].len);
		sd->sges[i].reserved = 0;
	}
	dat_ctrl.num_in_data = 0;
	dat_ctrl.num_out_data = 0;
	dlen[0] = sizeof(*sd) + sizeof(struct virtio_sge) * num_sges;

	ret = virtio_vdpa_send_admin_command(hw->avq, ctrl, &dat_ctrl, dlen, 1);
	if (ret) {
		CMD_LOG(ERR, "Failed to run class %u, cmd %u, status %d, vdev id: %u",
				ctrl->hdr.class, ctrl->hdr.cmd, ret, vdev_id);
		rte_spinlock_unlock(&hw->avq->lock);
		return ret;
	}

	rte_spinlock_unlock(&hw->avq->lock);
	return 0;
}

int
virtio_vdpa_cmd_dirty_page_stop_track(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id, uint64_t vdev_host_range_addr)
{
	struct virtio_admin_dirty_page_stop_track_data *sd;
	struct virtio_hw *hw = &priv->vpdev->hw;
	struct virtio_admin_data_ctrl dat_ctrl;
	struct virtio_admin_ctrl *ctrl;
	int dlen[1];
	int ret;

	RTE_VERIFY(priv);

	if (!virtio_with_feature(hw, VIRTIO_F_ADMIN_VQ)) {
		CMD_LOG(INFO, "host does not support admin queue");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&hw->avq->lock);

	ctrl = virtnet_get_aq_hdr_addr(hw->avq);
	ctrl->hdr.class = VIRTIO_ADMIN_PCI_DIRTY_PAGE_TRACK_CTRL;
	ctrl->hdr.cmd = VIRTIO_ADMIN_PCI_DIRTY_PAGE_STOP_TRACK;
	sd = (struct virtio_admin_dirty_page_stop_track_data *)&ctrl->data[0];
	sd->vdev_id = rte_cpu_to_le_16(vdev_id);
	sd->vdev_host_range_addr = rte_cpu_to_le_64(vdev_host_range_addr);
	dlen[0] = sizeof(*sd);
	dat_ctrl.num_in_data = 0;
	dat_ctrl.num_out_data = 0;

	ret = virtio_vdpa_send_admin_command(hw->avq, ctrl, &dat_ctrl, dlen, 1);
	if (ret) {
		CMD_LOG(ERR, "Failed to run class %u, cmd %u, status %d, vdev id: %u",
				ctrl->hdr.class, ctrl->hdr.cmd, ret, vdev_id);
		rte_spinlock_unlock(&hw->avq->lock);
		return ret;
	}

	rte_spinlock_unlock(&hw->avq->lock);
	return 0;
}

int
virtio_vdpa_cmd_dirty_page_get_map_pending_bytes(
		struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id,
		uint64_t vdev_host_range_addr,
		struct virtio_admin_dirty_page_get_map_pending_bytes_result *result)
{
	struct virtio_admin_dirty_page_get_map_pending_bytes_result *res;
	struct virtio_admin_dirty_page_get_map_pending_bytes_data *sd;
	struct virtio_hw *hw = &priv->vpdev->hw;
	struct virtio_admin_data_ctrl dat_ctrl;
	struct virtio_admin_ctrl *ctrl;
	int dlen[1];
	int ret;

	RTE_VERIFY(priv);

	if (!virtio_with_feature(hw, VIRTIO_F_ADMIN_VQ)) {
		CMD_LOG(INFO, "host does not support admin queue");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&hw->avq->lock);

	ctrl = virtnet_get_aq_hdr_addr(hw->avq);
	ctrl->hdr.class = VIRTIO_ADMIN_PCI_DIRTY_PAGE_TRACK_CTRL;
	ctrl->hdr.cmd = VIRTIO_ADMIN_PCI_DIRTY_PAGE_GET_MAP_PENDING_BYTES;
	sd = (struct virtio_admin_dirty_page_get_map_pending_bytes_data *)&ctrl->data[0];
	sd->vdev_id = rte_cpu_to_le_16(vdev_id);
	sd->vdev_host_range_addr = rte_cpu_to_le_64(vdev_host_range_addr);
	dlen[0] = sizeof(*sd);

	dat_ctrl.num_in_data = 0;
	dat_ctrl.num_out_data = 1;
	dat_ctrl.out_data[0].iova = hw->avq->virtio_admin_hdr_mem
			+ sizeof(struct virtio_admin_ctrl_hdr)
			+ sizeof(ctrl->status) + dlen[0];
	dat_ctrl.out_data[0].len = sizeof(*result);
	ret = virtio_vdpa_send_admin_command(hw->avq, ctrl, &dat_ctrl, dlen, 1);
	if (ret) {
		CMD_LOG(ERR, "Failed to run class %u, cmd %u, status %d, vdev_id: %u",
				ctrl->hdr.class, ctrl->hdr.cmd, ret, vdev_id);
		rte_spinlock_unlock(&hw->avq->lock);
		return ret;
	}

	res = (struct virtio_admin_dirty_page_get_map_pending_bytes_result *)
			rte_mem_iova2virt(dat_ctrl.out_data[0].iova);
	result->pending_bytes = rte_le_to_cpu_64(res->pending_bytes);

	rte_spinlock_unlock(&hw->avq->lock);
	return 0;
}

int
virtio_vdpa_cmd_dirty_page_report_map(struct virtio_vdpa_pf_priv *priv,
		uint16_t vdev_id,
		uint64_t offset,
		uint64_t length,
		uint64_t vdev_host_range_addr,
		rte_iova_t data)
{
	struct virtio_admin_dirty_page_report_map_data *sd;
	struct virtio_hw *hw = &priv->vpdev->hw;
	struct virtio_admin_data_ctrl dat_ctrl;
	struct virtio_admin_ctrl *ctrl;
	int dlen[1];
	int ret;

	RTE_VERIFY(priv);

	if (!virtio_with_feature(hw, VIRTIO_F_ADMIN_VQ)) {
		CMD_LOG(INFO, "host does not support admin queue");
		return -ENOTSUP;
	}

	rte_spinlock_lock(&hw->avq->lock);

	ctrl = virtnet_get_aq_hdr_addr(hw->avq);
	ctrl->hdr.class = VIRTIO_ADMIN_PCI_DIRTY_PAGE_TRACK_CTRL;
	ctrl->hdr.cmd = VIRTIO_ADMIN_PCI_DIRTY_PAGE_REPORT_MAP;
	sd = (struct virtio_admin_dirty_page_report_map_data *)&ctrl->data[0];
	sd->vdev_id = rte_cpu_to_le_16(vdev_id);
	sd->offset = rte_cpu_to_le_64(offset);
	sd->length = rte_cpu_to_le_64(length);
	sd->vdev_host_range_addr = rte_cpu_to_le_64(vdev_host_range_addr);
	dlen[0] = sizeof(*sd);
	dat_ctrl.num_in_data = 0;
	dat_ctrl.num_out_data = 1;
	dat_ctrl.out_data[0].iova = data;
	dat_ctrl.out_data[0].len = length;

	ret = virtio_vdpa_send_admin_command(hw->avq, ctrl, &dat_ctrl, dlen, 1);
	if (ret) {
		CMD_LOG(ERR, "Failed to run class %u, cmd %u, status %d, vdev id: %u",
				ctrl->hdr.class, ctrl->hdr.cmd, ret, vdev_id);
		rte_spinlock_unlock(&hw->avq->lock);
		return ret;
	}

	rte_spinlock_unlock(&hw->avq->lock);
	return 0;
}

static void
virtio_vdpa_init_vring(struct virtqueue *vq)
{
	uint8_t *ring_mem = vq->vq_ring_virt_mem;
	int size = vq->vq_nentries;
	struct vring *vr;

	DRV_LOG(DEBUG, ">>");

	memset(ring_mem, 0, vq->vq_ring_size);

	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;
	memset(vq->vq_descx, 0, sizeof(struct vq_desc_extra) * vq->vq_nentries);
	vr = &vq->vq_split.ring;

	vring_init_split(vr, ring_mem, VIRTIO_VRING_ALIGN, size);
	vring_desc_init_split(vr->desc, size);
	/*
	 * Disable device(host) interrupting guest
	 */
	virtqueue_disable_intr_split(vq);
}

static void
virtio_vdpa_destroy_aq_ctl(struct virtadmin_ctl *ctl)
{
	rte_memzone_free(ctl->mz);
	rte_memzone_free(ctl->virtio_admin_hdr_mz);
}

static int
virtio_vdpa_init_admin_queue(struct virtio_vdpa_pf_priv *priv)
{
	const struct rte_memzone *mz = NULL, *hdr_mz = NULL;
	int numa_node = priv->pdev->device.numa_node;
	struct virtio_pci_dev *vpdev = priv->vpdev;
	struct virtio_pci_dev_vring_info vr_info;
	char vq_hdr_name[VIRTQUEUE_MAX_NAME_SZ];
	char vq_name[VIRTQUEUE_MAX_NAME_SZ];
	struct virtio_hw *hw = &vpdev->hw;
	struct virtadmin_ctl *avq = NULL;
	unsigned int vq_size, size;
	struct virtqueue *vq;
	size_t sz_hdr_mz = 0;
	uint16_t queue_idx;
	int ret;

	DRV_LOG(INFO, "setting up admin queue on NUMA node %d", numa_node);

	queue_idx = priv->dev_ops->get_adminq_idx(priv);
	vq_size = virtio_pci_dev_queue_size_get(vpdev, queue_idx);
	DRV_LOG(INFO, "admin queue idx %u, queue size %u", queue_idx, vq_size);

	snprintf(vq_name, sizeof(vq_name), "vdev%d_aq%u",
		 vpdev->vfio_dev_fd, queue_idx);

	size = RTE_ALIGN_CEIL(sizeof(*vq) +
				vq_size * sizeof(struct vq_desc_extra),
				RTE_CACHE_LINE_SIZE);
	vq = rte_zmalloc_socket(vq_name, size, RTE_CACHE_LINE_SIZE,
				numa_node);
	if (vq == NULL) {
		DRV_LOG(ERR, "can not allocate admin q %u", queue_idx);
		return -ENOMEM;
	}
	hw->vqs[priv->dev_ops->get_adminq_idx(priv)] = vq;

	vq->hw = hw;
	vq->vq_queue_index = queue_idx;
	vq->vq_nentries = vq_size;

	/*
	 * Reserve a memzone for vring elements
	 */
	size = vring_size(hw, vq_size, VIRTIO_VRING_ALIGN);
	vq->vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_VRING_ALIGN);

	mz = rte_memzone_reserve_aligned(vq_name, vq->vq_ring_size,
			numa_node, RTE_MEMZONE_IOVA_CONTIG,
			VIRTIO_VRING_ALIGN);
	if (mz == NULL) {
		if (rte_errno == EEXIST)
			mz = rte_memzone_lookup(vq_name);
		if (mz == NULL) {
			ret = -ENOMEM;
			goto err_ret;
		}
	}

	memset(mz->addr, 0, mz->len);

	vq->vq_ring_mem = mz->iova;
	vq->vq_ring_virt_mem = mz->addr;

	virtio_vdpa_init_vring(vq);

	avq = &vq->aq;
	avq->mz = mz;

	/* Allocate a page for admin vq command, data and status */
	sz_hdr_mz = rte_mem_page_size();

	if (sz_hdr_mz) {
		snprintf(vq_hdr_name, sizeof(vq_hdr_name), "vdev%d_aq%u_hdr",
				vpdev->vfio_dev_fd, queue_idx);
		hdr_mz = rte_memzone_reserve_aligned(vq_hdr_name, sz_hdr_mz,
				numa_node, RTE_MEMZONE_IOVA_CONTIG,
				RTE_CACHE_LINE_SIZE);
		if (hdr_mz == NULL) {
			if (rte_errno == EEXIST)
				hdr_mz = rte_memzone_lookup(vq_hdr_name);
			if (hdr_mz == NULL) {
				ret = -ENOMEM;
				goto err_free_mz;
			}
		}
		avq->virtio_admin_hdr_mz = hdr_mz;
		avq->virtio_admin_hdr_mem = hdr_mz->iova;
		memset(avq->virtio_admin_hdr_mz->addr, 0, rte_mem_page_size());
	} else {
		DRV_LOG(ERR, "rte mem page size is zero");
		ret = -EINVAL;
		goto err_free_mz;
	}

	hw->avq = avq;

	vr_info.size  = vq_size;
	vr_info.desc  = (uint64_t)(uintptr_t)vq->vq_split.ring.desc;
	vr_info.avail = (uint64_t)(uintptr_t)vq->vq_split.ring.avail;
	vr_info.used  = (uint64_t)(uintptr_t)vq->vq_split.ring.used;
	ret = virtio_pci_dev_queue_set(vpdev, queue_idx, &vr_info);
	if (ret) {
		DRV_LOG(ERR, "setup_queue %u failed", queue_idx);
		ret = -EINVAL;
		goto err_clean_avq;
	}

	return 0;

err_clean_avq:
	hw->avq = NULL;
	rte_memzone_free(hdr_mz);
err_free_mz:
	rte_memzone_free(mz);
err_ret:
	hw->vqs[0] = NULL;
	rte_free(vq);
	return ret;
}

static void
virtio_vdpa_admin_queue_free(struct virtio_vdpa_pf_priv *priv)
{
	uint16_t nr_vq = priv->hw_nr_virtqs;
	struct virtio_hw *hw = &priv->vpdev->hw;
	struct virtqueue *vq;
	uint16_t i;

	if (hw->vqs == NULL)
		return;

	if (hw->avq) {
		virtio_vdpa_destroy_aq_ctl(hw->avq);
		hw->avq = NULL;
	}

	for (i = 0; i < nr_vq; i++) {
		vq = hw->vqs[i];
		if (vq) {
			rte_free(vq);
			hw->vqs[i] = NULL;
		}
	}
	rte_free(hw->vqs);
	hw->vqs = NULL;
}

static int
virtio_vdpa_admin_queue_alloc(struct virtio_vdpa_pf_priv *priv)
{
	struct virtio_hw *hw = &priv->vpdev->hw;
	int ret;

	priv->hw_nr_virtqs = 1;
	hw->vqs = rte_zmalloc(NULL, sizeof(struct virtqueue *) * (priv->dev_ops->get_adminq_idx(priv) + 1), 0);
	if (!hw->vqs) {
		DRV_LOG(ERR, "failed to allocate vqs");
		return -ENOMEM;
	}

	ret = virtio_vdpa_init_admin_queue(priv);
	if (ret) {
		DRV_LOG(ERR, "Failed to init admin queue for virtio device");
		rte_free(hw->vqs);
		hw->vqs = NULL;
		return ret;
	}

	return 0;
}

static int vdpa_mi_check_handler(__rte_unused const char *key,
		const char *value, void *ret_val)
{
	if (strcmp(value, VIRTIO_ARG_VDPA_VALUE_PF) == 0)
		*(int *)ret_val = 1;
	else
		*(int *)ret_val = 0;

	return 0;
}

static int
virtio_pci_devargs_parse(struct rte_devargs *devargs, int *vdpa)
{
	struct rte_kvargs *kvlist;
	int ret = 0;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL) {
		DRV_LOG(ERR, "Error when parsing param");
		return 0;
	}

	if (rte_kvargs_count(kvlist, VIRTIO_ARG_VDPA) == 1) {
		/* vdpa mode selected when there's a key-value pair:
		 * vdpa=1
		 */
		ret = rte_kvargs_process(kvlist, VIRTIO_ARG_VDPA,
					 vdpa_mi_check_handler, vdpa);
		if (ret < 0)
			DRV_LOG(ERR, "Failed to parse %s", VIRTIO_ARG_VDPA);
	}

	rte_kvargs_free(kvlist);

	return ret;
}

static uint64_t
virtio_vdpa_get_net_dev_required_features(void)
{
	return VIRTIO_VDPA_MI_SUPPORTED_NET_FEATURES;
}

static uint16_t
virtio_vdpa_net_dev_get_adminq_idx(struct virtio_vdpa_pf_priv *priv)
{
	uint16_t have_ctrlq = (VIRTIO_VDPA_MI_SUPPORTED_NET_FEATURES & VIRTIO_NET_F_CTRL_VQ) ?
		1 : 0;
	return virtio_pci_dev_nr_vq_get(priv->vpdev) + have_ctrlq;
}

static uint64_t
virtio_vdpa_blk_dev_get_required_features(void)
{
	return VIRTIO_VDPA_MI_SUPPORTED_BLK_FEATURES;
}

static uint16_t
virtio_vdpa_blk_dev_get_adminq_idx(struct virtio_vdpa_pf_priv *priv __rte_unused)
{
	return 0;
}

void
rte_vdpa_pf_ctrl_ctx_remove(bool enable)
{
	ctx_remove_enabled = enable;
}

static struct virtio_vdpa_dev_ops virtio_vdpa_net_dev_ops = {
	.get_required_features = virtio_vdpa_get_net_dev_required_features,
	.get_adminq_idx = virtio_vdpa_net_dev_get_adminq_idx,
};

static struct virtio_vdpa_dev_ops virtio_vdpa_blk_dev_ops = {
	.get_required_features = virtio_vdpa_blk_dev_get_required_features,
	.get_adminq_idx = virtio_vdpa_blk_dev_get_adminq_idx,
};

static int
virtio_vdpa_mi_dev_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	char devname[RTE_DEV_NAME_MAX_LEN] = {0};
	struct virtio_vdpa_pf_priv *priv = NULL;
	int vdpa = 0, container_fd = -1, group_fd = -1, device_fd = -1, ret, iommu_group;
	int retries = VIRTIO_VDPA_MI_GET_GROUP_RETRIES;
	struct virtio_pf_ctx ctx;
	uint64_t features;
	bool ctx_restore = false;

	RTE_VERIFY(rte_eal_iova_mode() == RTE_IOVA_VA);

	ret = virtio_pci_devargs_parse(pci_dev->device.devargs, &vdpa);
	if (ret < 0) {
		DRV_LOG(ERR, "Devargs parsing is failed");
		return -VFE_VDPA_ERR_DEVARGS_PARSE;
	}
	/* virtio vdpa pmd skips probe if device needs to work in none vdpa mode */
	if (vdpa != 1)
		return 1;

	priv = rte_zmalloc("virtio vdpa pf device private", sizeof(*priv), RTE_CACHE_LINE_SIZE);
	if (!priv) {
		DRV_LOG(ERR, "Failed to allocate private memory");
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	rte_pci_device_name(&pci_dev->addr, devname, RTE_DEV_NAME_MAX_LEN);
	strcpy(priv->pf_name.dev_bdf, devname);

	priv->pdev = pci_dev;

	ret = rte_vfio_get_group_num(rte_pci_get_sysfs_path(), devname,
			&iommu_group);
	if (ret <= 0) {
		DRV_LOG(ERR, "%s failed to get IOMMU group ret:%d", devname, ret);
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}

	if (!strcmp(cached_ctx.pf_name.dev_bdf, devname)) {
		ctx_restore = true;
		container_fd = rte_vfio_get_default_cfd();
		group_fd = cached_ctx.vfio_group_fd;
		device_fd = cached_ctx.vfio_device_fd;

		do {
			DRV_LOG(INFO, "%s iommu_group_num:%d retries:%d", devname, iommu_group, retries);

			ret = rte_vfio_container_group_set_bind(container_fd, iommu_group, group_fd);
			if (ret < 0) {
				DRV_LOG(ERR, "%s failed to get group fd", devname);
				sleep(1);
				retries--;
			} else
				break;
			if (!retries)
				goto error;
		} while(retries);
	}

	priv->vpdev = virtio_pci_dev_alloc(pci_dev, device_fd);
	if (priv->vpdev == NULL) {
		DRV_LOG(ERR, "%s failed to alloc virtio pci dev", devname);
		ret = rte_errno ? -rte_errno : -VFE_VDPA_ERR_ADD_PF_PROBE_FAIL;
		goto error;
	}

	priv->vfio_dev_fd = rte_intr_dev_fd_get(pci_dev->intr_handle);
	if (priv->vfio_dev_fd < 0) {
		DRV_LOG(ERR, "%s failed to get vfio dev fd", devname);
		ret = -VFE_VDPA_ERR_VFIO_DEV_FD;
		goto err_free_pci_dev;
	}

	if (!ctx_restore) {
		group_fd = rte_vfio_get_group_fd(iommu_group);
		if (group_fd < 0) {
			DRV_LOG(ERR, "%s failed to get vfio group fd", devname);
			goto err_free_pci_dev;
		}

		ctx.vfio_group_fd = group_fd;
		ctx.vfio_device_fd = priv->vfio_dev_fd;
		ret = virtio_ha_pf_ctx_store(&priv->pf_name, &ctx);
		if (ret < 0) {
			DRV_LOG(ERR, "%s failed to store pf ctx", devname);
			goto err_free_pci_dev;			
		}
	}

	if (priv->pdev->id.device_id == VIRTIO_PCI_MODERN_DEVICEID_NET) {
		priv->dev_ops = &virtio_vdpa_net_dev_ops;
	}
	else if (priv->pdev->id.device_id == VIRTIO_PCI_MODERN_DEVICEID_BLK) {
		priv->dev_ops = &virtio_vdpa_blk_dev_ops;;
	}
	else {
		DRV_LOG(ERR, "PCI device: %s device id 0x%x is not supported",
					priv->pdev->device.name,
					priv->pdev->id.device_id);
		ret = -VFE_VDPA_ERR_ADD_PF_DEVICEID_NOT_SUPPORT;
		goto err_free_pci_dev;
	}

	virtio_pci_dev_features_get(priv->vpdev, &priv->device_features);
	features = priv->dev_ops->get_required_features();
	if ((priv->device_features & features) != features) {
		DRV_LOG(ERR, "Device does not support feature required: device 0x%" PRIx64 \
				", required: 0x%" PRIx64, priv->device_features,
				features);
		ret = -VFE_VDPA_ERR_ADD_PF_FEATURE_NOT_MEET;
		goto err_free_pci_dev;
	}
	features = virtio_pci_dev_features_set(priv->vpdev, features);
	priv->vpdev->hw.weak_barriers = !virtio_with_feature(&priv->vpdev->hw, VIRTIO_F_ORDER_PLATFORM);
	virtio_pci_dev_set_status(priv->vpdev, VIRTIO_CONFIG_STATUS_FEATURES_OK);

	ret = virtio_vdpa_admin_queue_alloc(priv);
	if (ret) {
		DRV_LOG(ERR, "Failed to alloc admin queue for vDPA device");
		ret = -VFE_VDPA_ERR_ADD_PF_ALLOC_ADMIN_QUEUE;
		goto err_free_pci_dev;
	}

	/* Start the device */
	virtio_pci_dev_set_status(priv->vpdev, VIRTIO_CONFIG_STATUS_DRIVER_OK);

	pthread_mutex_lock(&mi_priv_list_lock);
	TAILQ_INSERT_TAIL(&virtio_mi_priv_list, priv, next);
	pthread_mutex_unlock(&mi_priv_list_lock);
	return 0;

err_free_pci_dev:
	virtio_pci_dev_free(priv->vpdev);
error:
	rte_free(priv);
	return ret;
}

static int
virtio_vdpa_mi_dev_remove(struct rte_pci_device *pci_dev)
{
	struct virtio_vdpa_pf_priv *priv = NULL;
	int found = 0;

	pthread_mutex_lock(&mi_priv_list_lock);
	TAILQ_FOREACH(priv, &virtio_mi_priv_list, next) {
		if (priv->pdev == pci_dev) {
			found = 1;
			TAILQ_REMOVE(&virtio_mi_priv_list, priv, next);
			break;
		}
	}
	pthread_mutex_unlock(&mi_priv_list_lock);

	if (found) {
		if (ctx_remove_enabled)
			virtio_ha_pf_ctx_remove(&priv->pf_name);
		virtio_vdpa_admin_queue_free(priv);
		virtio_pci_dev_reset(priv->vpdev,VIRTIO_VDPA_REMOVE_RESET_TIME_OUT);
		virtio_pci_dev_free(priv->vpdev);
		rte_free(priv);
	}
	return 0;
}

static void
virtio_ha_pf_drv_ctx_set(const struct virtio_dev_name *pf, const void *ctx)
{
	const struct virtio_pf_ctx *pf_ctx = (const struct virtio_pf_ctx *)ctx;

	memcpy(&cached_ctx.pf_name, pf, sizeof(struct virtio_dev_name));
	cached_ctx.vfio_group_fd = pf_ctx->vfio_group_fd;
	cached_ctx.vfio_device_fd = pf_ctx->vfio_device_fd;
}

static void
virtio_ha_pf_drv_ctx_unset(const struct virtio_dev_name *pf)
{
	if (strcmp(pf->dev_bdf, cached_ctx.pf_name.dev_bdf))
		return;
	memset(&cached_ctx.pf_name, 0, sizeof(struct virtio_dev_name));
	cached_ctx.vfio_group_fd = -1;
	cached_ctx.vfio_device_fd = -1;
}

struct virtio_vdpa_pf_priv *
rte_vdpa_get_mi_by_bdf(const char *bdf)
{
	struct virtio_vdpa_pf_priv *priv;
	struct rte_pci_addr dev_addr;
	int found = 0;

	if (rte_pci_addr_parse(bdf, &dev_addr))
		return NULL;

	pthread_mutex_lock(&mi_priv_list_lock);
	TAILQ_FOREACH(priv, &virtio_mi_priv_list, next) {
		if (!rte_pci_addr_cmp(&priv->pdev->addr, &dev_addr)) {
			found = 1;
			break;
		}
	}
	pthread_mutex_unlock(&mi_priv_list_lock);

	if (found)
		return priv;
	return NULL;
}

int
rte_vdpa_pf_dev_add(const char *pf_name)
{
	if (!pf_name)
		return -VFE_VDPA_ERR_NO_PF_NAME;

	if (rte_vdpa_get_mi_by_bdf(pf_name))
		return -VFE_VDPA_ERR_ADD_PF_ALREADY_ADD;

	return rte_eal_hotplug_add("pci", pf_name, "vdpa=2");
}

int
rte_vdpa_pf_dev_remove(const char *pf_name)
{
	if (!pf_name)
		return -VFE_VDPA_ERR_NO_PF_NAME;

	if (!rte_vdpa_get_mi_by_bdf(pf_name))
		return -VFE_VDPA_ERR_NO_PF_DEVICE;

	/* Fixme: no vf count checking */
	return rte_eal_hotplug_remove("pci", pf_name);
}

int
rte_vdpa_get_pf_list(struct virtio_vdpa_pf_info *pf_info, int max_pf_num)
{
	struct virtio_vdpa_pf_info *info = pf_info;
	struct virtio_vdpa_pf_priv *priv;
	int count = 0;

	pthread_mutex_lock(&mi_priv_list_lock);
	TAILQ_FOREACH(priv, &virtio_mi_priv_list, next) {
		rte_pci_device_name(&priv->pdev->addr, info->pf_name,
				sizeof(info->pf_name));
		count++;
		if (count >= max_pf_num)
			break;
		info++;
	}
	pthread_mutex_unlock(&mi_priv_list_lock);
	return count;
}

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_virtio_mi_map[] = {
	{ RTE_PCI_DEVICE(VIRTIO_PCI_VENDORID, VIRTIO_PCI_MODERN_DEVICEID_NET) },
	{ RTE_PCI_DEVICE(VIRTIO_PCI_VENDORID, VIRTIO_PCI_MODERN_DEVICEID_BLK) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver virtio_vdpa_mi_driver = {
	.id_table = pci_id_virtio_mi_map,
	.drv_flags = 0,
	.probe = virtio_vdpa_mi_dev_probe,
	.remove = virtio_vdpa_mi_dev_remove,
};

#define VIRTIO_VDPA_MI_DRIVER_NAME vdpa_virtio_mi

static struct virtio_ha_dev_ctx_cb virtio_ha_pf_drv_cb = {
	.set = virtio_ha_pf_drv_ctx_set,
	.unset = virtio_ha_pf_drv_ctx_unset,
};

RTE_INIT(common_virtio_mi_init)
{
	virtio_ha_pf_register_ctx_cb(&virtio_ha_pf_drv_cb);
}

RTE_PMD_REGISTER_PCI(VIRTIO_VDPA_MI_DRIVER_NAME, virtio_vdpa_mi_driver);
RTE_PMD_REGISTER_PCI_TABLE(VIRTIO_VDPA_MI_DRIVER_NAME, pci_id_virtio_mi_map);
RTE_PMD_REGISTER_KMOD_DEP(VIRTIO_VDPA_MI_DRIVER_NAME, "* vfio-pci");
