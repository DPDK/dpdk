/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */
#include <unistd.h>
#include <rte_common.h>
#include <rte_vhost.h>
#include "rte_vf_rpc.h"
#include "virtio_api.h"
#include "virtio_vdpa.h"
#include "virtio_blk.h"

#define VIRTIO_VDPA_BLK_PROTOCOL_FEATURES \
				((1ULL << VHOST_USER_PROTOCOL_F_SLAVE_REQ) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_SLAVE_SEND_FD) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_CONFIG) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_STATUS) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_MQ) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK) | \
				 (1ULL << VHOST_USER_PROTOCOL_F_PRESETUP))

extern int virtio_vdpa_logtype;
#define BLK_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, virtio_vdpa_logtype, \
		"VIRTIO VDPA BLK %s(): " fmt "\n", __func__, ##args)

#define VIRTIO_VDPA_BLK_QUEUE_NUM_UNIT 1

static void
virtio_vdpa_blk_vhost_feature_get(uint64_t *features)
{
	*features = VIRTIO_VDPA_BLK_PROTOCOL_FEATURES;
}

static int
virtio_vdpa_blk_dirty_desc_get(int vid, int qix, uint64_t *desc_addr, uint32_t *write_len)
{
	struct rte_vhost_vring vq;
	uint32_t desc_id, desc_len;
	struct virtio_blk_outhdr *blk_hdr;
	int ret;

	ret = rte_vhost_get_vhost_vring(vid, qix, &vq);
	if (ret) {
		BLK_LOG(ERR, "VID: %d qix:%d fail get vhost ring", vid, qix);
		return -ENODEV;
	}

	desc_id = vq.used->ring[(vq.used->idx -1) & (vq.size -1)].id;
	*desc_addr = vq.desc[desc_id].addr;
	*write_len = RTE_MIN(vq.used->ring[(vq.used->idx -1) & (vq.size -1)].len, vq.desc[desc_id].len);

	desc_len = vq.desc[desc_id].len;

	blk_hdr = (struct virtio_blk_outhdr *)virtio_vdpa_gpa_to_hva(vid, *desc_addr);
	if (blk_hdr->type != VIRTIO_BLK_T_IN) {
		BLK_LOG(ERR, "VID: %d qix:%d last desc is not read", vid, qix);
		return -EINVAL;
	}
	if (desc_len > sizeof(struct virtio_blk_outhdr)) {
		*desc_addr = *desc_addr + sizeof(struct virtio_blk_outhdr);
	} else {
		if (!(vq.desc[desc_id].flags & VRING_DESC_F_NEXT)) {
			BLK_LOG(ERR, "VID: %d qix:%d last desc is too short", vid, qix);
			return -EINVAL;
		}

		*desc_addr = vq.desc[vq.desc[desc_id].next].addr;
		*write_len = vq.desc[vq.desc[desc_id].next].len;
	}

	return 0;
}

static void
virtio_vdpa_blk_dev_intr_handler(void *cb_arg)
{
	struct virtio_blk_config vb_cfg;
	struct virtio_vdpa_priv *priv;
	uint8_t isr;
	int ret;

	priv = cb_arg;

	/* Read interrupt status which clears interrupt */
	isr = virtio_pci_dev_isr_get(priv->vpdev);
	BLK_LOG(INFO, "%s interrupt status = 0x%x", priv->pdev->device.name, isr);

	ret = rte_intr_ack(priv->pdev->intr_handle);
	if (ret < 0)
		BLK_LOG(ERR, "%s interrupt ack failed ret %d", priv->pdev->device.name, ret);

	if (isr & VIRTIO_ISR_CONFIG) {
		virtio_pci_dev_config_read(priv->vpdev, 0, &vb_cfg, sizeof(struct virtio_blk_config));
		virtio_pci_dev_state_config_write(priv->vpdev, &vb_cfg,
										sizeof(struct virtio_blk_config),
										priv->state_mz->addr);
		BLK_LOG(INFO, "%s config change capacity=0x%" PRIx64, priv->pdev->device.name, vb_cfg.capacity);
	}

	if (priv->dev_conf_read && priv->configured) {
		BLK_LOG(INFO, "%s send vhost config change msg", priv->pdev->device.name);
		ret = rte_vhost_slave_config_change(priv->vid, false);
		if (ret)
			BLK_LOG(ERR, "%s failed to send vhost config change ret:%d",
						priv->pdev->device.name, ret);
	}
}

static int
virtio_vdpa_blk_unreg_dev_interrupt(struct virtio_vdpa_priv *priv)
{
	int ret = -EAGAIN;
	struct rte_intr_handle *intr_handle;
	int retries = VIRTIO_VDPA_INTR_RETRIES;

	intr_handle = priv->pdev->intr_handle;
	if (rte_intr_fd_get(intr_handle) != -1) {
		while (retries-- && ret == -EAGAIN) {
			ret = rte_intr_callback_unregister(intr_handle,
							virtio_vdpa_blk_dev_intr_handler,
							priv);
			if (ret == -EAGAIN) {
				BLK_LOG(DEBUG, "%s try again to unregister fd %d "
				"for dev interrupt, retries = %d",
				priv->pdev->device.name,
				rte_intr_fd_get(intr_handle),
				retries);
				usleep(VIRTIO_VDPA_INTR_RETRIES_USEC);
			}
		}
	}

	return 0;
}

static int
virtio_vdpa_blk_reg_dev_interrupt(struct virtio_vdpa_priv *priv)
{
	int ret;
	struct rte_intr_handle *intr_handle;

	intr_handle = priv->pdev->intr_handle;

	ret = rte_intr_callback_register(intr_handle,
					   virtio_vdpa_blk_dev_intr_handler,
					   priv);
	if (ret) {
		BLK_LOG(ERR, "%s failed to register dev interrupt ret:%d",
					priv->pdev->device.name, ret);
		return -EINVAL;
	}
	BLK_LOG(DEBUG, "%s register fd %d for dev interrupt",
		priv->pdev->device.name,
		rte_intr_fd_get(intr_handle));

	return 0;
}

static int
virtio_vdpa_blk_queue_num_unit_get(void)
{
	return VIRTIO_VDPA_BLK_QUEUE_NUM_UNIT;
}

struct virtio_vdpa_device_callback virtio_vdpa_blk_callback = {
	.vhost_feature_get = virtio_vdpa_blk_vhost_feature_get,
	.dirty_desc_get = virtio_vdpa_blk_dirty_desc_get,
	.reg_dev_intr = virtio_vdpa_blk_reg_dev_interrupt,
	.unreg_dev_intr = virtio_vdpa_blk_unreg_dev_interrupt,
	.vdpa_queue_num_unit_get = virtio_vdpa_blk_queue_num_unit_get,
	.add_vdpa_feature = NULL,
};

