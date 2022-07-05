/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_io.h>

#include <mlx5_common.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"


static void
mlx5_vdpa_virtq_kick_handler(void *cb_arg)
{
	struct mlx5_vdpa_virtq *virtq = cb_arg;
	struct mlx5_vdpa_priv *priv = virtq->priv;
	uint64_t buf;
	int nbytes;
	int retry;

	pthread_mutex_lock(&virtq->virtq_lock);
	if (priv->state != MLX5_VDPA_STATE_CONFIGURED && !virtq->enable) {
		pthread_mutex_unlock(&virtq->virtq_lock);
		DRV_LOG(ERR,  "device %d queue %d down, skip kick handling",
			priv->vid, virtq->index);
		return;
	}
	if (rte_intr_fd_get(virtq->intr_handle) < 0) {
		pthread_mutex_unlock(&virtq->virtq_lock);
		return;
	}
	for (retry = 0; retry < 3; ++retry) {
		nbytes = read(rte_intr_fd_get(virtq->intr_handle), &buf,
			      8);
		if (nbytes < 0) {
			if (errno == EINTR ||
			    errno == EWOULDBLOCK ||
			    errno == EAGAIN)
				continue;
			DRV_LOG(ERR,  "Failed to read kickfd of virtq %d: %s.",
				virtq->index, strerror(errno));
		}
		break;
	}
	if (nbytes < 0) {
		pthread_mutex_unlock(&virtq->virtq_lock);
		return;
	}
	rte_spinlock_lock(&priv->db_lock);
	rte_write32(virtq->index, priv->virtq_db_addr);
	rte_spinlock_unlock(&priv->db_lock);
	pthread_mutex_unlock(&virtq->virtq_lock);
	if (priv->state != MLX5_VDPA_STATE_CONFIGURED && !virtq->enable) {
		DRV_LOG(ERR,  "device %d queue %d down, skip kick handling.",
			priv->vid, virtq->index);
		return;
	}
	if (virtq->notifier_state == MLX5_VDPA_NOTIFIER_STATE_DISABLED) {
		if (rte_vhost_host_notifier_ctrl(priv->vid, virtq->index, true))
			virtq->notifier_state = MLX5_VDPA_NOTIFIER_STATE_ERR;
		else
			virtq->notifier_state =
					       MLX5_VDPA_NOTIFIER_STATE_ENABLED;
		DRV_LOG(INFO, "Virtq %u notifier state is %s.", virtq->index,
			virtq->notifier_state ==
				MLX5_VDPA_NOTIFIER_STATE_ENABLED ? "enabled" :
								    "disabled");
	}
	DRV_LOG(DEBUG, "Ring virtq %u doorbell.", virtq->index);
}

/* Virtq must be locked before calling this function. */
static void
mlx5_vdpa_virtq_unregister_intr_handle(struct mlx5_vdpa_virtq *virtq)
{
	int ret = -EAGAIN;

	if (!virtq->intr_handle)
		return;
	if (rte_intr_fd_get(virtq->intr_handle) >= 0) {
		while (ret == -EAGAIN) {
			ret = rte_intr_callback_unregister(virtq->intr_handle,
					mlx5_vdpa_virtq_kick_handler, virtq);
			if (ret == -EAGAIN) {
				DRV_LOG(DEBUG, "Try again to unregister fd %d of virtq %hu interrupt",
					rte_intr_fd_get(virtq->intr_handle),
					virtq->index);
				pthread_mutex_unlock(&virtq->virtq_lock);
				usleep(MLX5_VDPA_INTR_RETRIES_USEC);
				pthread_mutex_lock(&virtq->virtq_lock);
			}
		}
		(void)rte_intr_fd_set(virtq->intr_handle, -1);
	}
	rte_intr_instance_free(virtq->intr_handle);
	virtq->intr_handle = NULL;
}

void
mlx5_vdpa_virtq_unreg_intr_handle_all(struct mlx5_vdpa_priv *priv)
{
	uint32_t i;
	struct mlx5_vdpa_virtq *virtq;

	for (i = 0; i < priv->nr_virtqs; i++) {
		virtq = &priv->virtqs[i];
		pthread_mutex_lock(&virtq->virtq_lock);
		mlx5_vdpa_virtq_unregister_intr_handle(virtq);
		pthread_mutex_unlock(&virtq->virtq_lock);
	}
}

static void
mlx5_vdpa_vq_destroy(struct mlx5_vdpa_virtq *virtq)
{
	/* Clean pre-created resource in dev removal only */
	claim_zero(mlx5_devx_cmd_destroy(virtq->virtq));
	virtq->index = 0;
	virtq->virtq = NULL;
	virtq->configured = 0;
}

/* Release cached VQ resources. */
void
mlx5_vdpa_virtqs_cleanup(struct mlx5_vdpa_priv *priv)
{
	unsigned int i, j;

	mlx5_vdpa_steer_unset(priv);
	for (i = 0; i < priv->caps.max_num_virtio_queues; i++) {
		struct mlx5_vdpa_virtq *virtq = &priv->virtqs[i];

		pthread_mutex_lock(&virtq->virtq_lock);
		if (virtq->virtq)
			mlx5_vdpa_vq_destroy(virtq);
		for (j = 0; j < RTE_DIM(virtq->umems); ++j) {
			if (virtq->umems[j].obj) {
				claim_zero(mlx5_glue->devx_umem_dereg
							(virtq->umems[j].obj));
				virtq->umems[j].obj = NULL;
			}
			if (virtq->umems[j].buf) {
				rte_free(virtq->umems[j].buf);
				virtq->umems[j].buf = NULL;
			}
			virtq->umems[j].size = 0;
		}
		if (virtq->eqp.fw_qp)
			mlx5_vdpa_event_qp_destroy(&virtq->eqp);
		pthread_mutex_unlock(&virtq->virtq_lock);
	}
}

void
mlx5_vdpa_virtq_unset(struct mlx5_vdpa_virtq *virtq)
{
	int ret;

	mlx5_vdpa_virtq_unregister_intr_handle(virtq);
	if (virtq->configured) {
		ret = mlx5_vdpa_virtq_stop(virtq->priv, virtq->index);
		if (ret)
			DRV_LOG(WARNING, "Failed to stop virtq %d.",
				virtq->index);
	}
	mlx5_vdpa_vq_destroy(virtq);
	virtq->notifier_state = MLX5_VDPA_NOTIFIER_STATE_DISABLED;
}

void
mlx5_vdpa_virtqs_release(struct mlx5_vdpa_priv *priv,
	bool release_resource)
{
	struct mlx5_vdpa_virtq *virtq;
	uint32_t i, max_virtq, valid_vq_num;

	valid_vq_num = ((priv->queues * 2) < priv->caps.max_num_virtio_queues) ?
		(priv->queues * 2) : priv->caps.max_num_virtio_queues;
	max_virtq = (release_resource &&
		(valid_vq_num) > priv->nr_virtqs) ?
		(valid_vq_num) : priv->nr_virtqs;
	for (i = 0; i < max_virtq; i++) {
		virtq = &priv->virtqs[i];
		pthread_mutex_lock(&virtq->virtq_lock);
		mlx5_vdpa_virtq_unset(virtq);
		virtq->enable = 0;
		if (!release_resource && i < valid_vq_num)
			mlx5_vdpa_virtq_single_resource_prepare(
					priv, i);
		pthread_mutex_unlock(&virtq->virtq_lock);
	}
	if (!release_resource && priv->queues &&
		mlx5_vdpa_is_modify_virtq_supported(priv))
		if (mlx5_vdpa_steer_update(priv, true))
			mlx5_vdpa_steer_unset(priv);
	priv->features = 0;
	priv->nr_virtqs = 0;
}

int
mlx5_vdpa_virtq_modify(struct mlx5_vdpa_virtq *virtq, int state)
{
	struct mlx5_devx_virtq_attr attr = {
			.mod_fields_bitmap = MLX5_VIRTQ_MODIFY_TYPE_STATE,
			.state = state ? MLX5_VIRTQ_STATE_RDY :
					 MLX5_VIRTQ_STATE_SUSPEND,
			.queue_index = virtq->index,
	};

	return mlx5_devx_cmd_modify_virtq(virtq->virtq, &attr);
}

int
mlx5_vdpa_virtq_stop(struct mlx5_vdpa_priv *priv, int index)
{
	struct mlx5_vdpa_virtq *virtq = &priv->virtqs[index];
	int ret;

	if (virtq->stopped || !virtq->configured)
		return 0;
	ret = mlx5_vdpa_virtq_modify(virtq, 0);
	if (ret)
		return -1;
	virtq->stopped = 1;
	DRV_LOG(DEBUG, "vid %u virtq %u was stopped.", priv->vid, index);
	return mlx5_vdpa_virtq_query(priv, index);
}

int
mlx5_vdpa_virtq_query(struct mlx5_vdpa_priv *priv, int index)
{
	struct mlx5_devx_virtq_attr attr = {0};
	struct mlx5_vdpa_virtq *virtq = &priv->virtqs[index];
	int ret;

	if (mlx5_devx_cmd_query_virtq(virtq->virtq, &attr)) {
		DRV_LOG(ERR, "Failed to query virtq %d.", index);
		return -1;
	}
	DRV_LOG(INFO, "Query vid %d vring %d: hw_available_idx=%d, "
		"hw_used_index=%d", priv->vid, index,
		attr.hw_available_index, attr.hw_used_index);
	ret = rte_vhost_set_vring_base(priv->vid, index,
				       attr.hw_available_index,
				       attr.hw_used_index);
	if (ret) {
		DRV_LOG(ERR, "Failed to set virtq %d base.", index);
		return -1;
	}
	if (attr.state == MLX5_VIRTQ_STATE_ERROR)
		DRV_LOG(WARNING, "vid %d vring %d hw error=%hhu.",
			priv->vid, index, attr.error_type);
	return 0;
}

static uint64_t
mlx5_vdpa_hva_to_gpa(struct rte_vhost_memory *mem, uint64_t hva)
{
	struct rte_vhost_mem_region *reg;
	uint32_t i;
	uint64_t gpa = 0;

	for (i = 0; i < mem->nregions; i++) {
		reg = &mem->regions[i];
		if (hva >= reg->host_user_addr &&
		    hva < reg->host_user_addr + reg->size) {
			gpa = hva - reg->host_user_addr + reg->guest_phys_addr;
			break;
		}
	}
	return gpa;
}

static int
mlx5_vdpa_virtq_sub_objs_prepare(struct mlx5_vdpa_priv *priv,
		struct mlx5_devx_virtq_attr *attr,
		struct rte_vhost_vring *vq,
		int index, bool is_prepare)
{
	struct mlx5_vdpa_virtq *virtq = &priv->virtqs[index];
	uint64_t gpa;
	int ret;
	unsigned int i;
	uint16_t last_avail_idx = 0;
	uint16_t last_used_idx = 0;

	if (virtq->virtq)
		attr->mod_fields_bitmap = MLX5_VIRTQ_MODIFY_TYPE_STATE |
			MLX5_VIRTQ_MODIFY_TYPE_ADDR |
			MLX5_VIRTQ_MODIFY_TYPE_HW_AVAILABLE_INDEX |
			MLX5_VIRTQ_MODIFY_TYPE_HW_USED_INDEX |
			MLX5_VIRTQ_MODIFY_TYPE_VERSION_1_0 |
			MLX5_VIRTQ_MODIFY_TYPE_Q_TYPE |
			MLX5_VIRTQ_MODIFY_TYPE_Q_MKEY |
			MLX5_VIRTQ_MODIFY_TYPE_QUEUE_FEATURE_BIT_MASK |
			MLX5_VIRTQ_MODIFY_TYPE_EVENT_MODE;
	attr->tso_ipv4 = is_prepare ? 1 :
		!!(priv->features & (1ULL << VIRTIO_NET_F_HOST_TSO4));
	attr->tso_ipv6 = is_prepare ? 1 :
		!!(priv->features & (1ULL << VIRTIO_NET_F_HOST_TSO6));
	attr->tx_csum = is_prepare ? 1 :
		!!(priv->features & (1ULL << VIRTIO_NET_F_CSUM));
	attr->rx_csum = is_prepare ? 1 :
		!!(priv->features & (1ULL << VIRTIO_NET_F_GUEST_CSUM));
	attr->virtio_version_1_0 = is_prepare ? 1 :
		!!(priv->features & (1ULL << VIRTIO_F_VERSION_1));
	attr->q_type =
		(priv->features & (1ULL << VIRTIO_F_RING_PACKED)) ?
			MLX5_VIRTQ_TYPE_PACKED : MLX5_VIRTQ_TYPE_SPLIT;
	/*
	 * No need event QPs creation when the guest in poll mode or when the
	 * capability allows it.
	 */
	attr->event_mode = is_prepare || vq->callfd != -1 ||
	!(priv->caps.event_mode & (1 << MLX5_VIRTQ_EVENT_MODE_NO_MSIX)) ?
	MLX5_VIRTQ_EVENT_MODE_QP : MLX5_VIRTQ_EVENT_MODE_NO_MSIX;
	if (attr->event_mode == MLX5_VIRTQ_EVENT_MODE_QP) {
		ret = mlx5_vdpa_event_qp_prepare(priv, vq->size,
				vq->callfd, virtq, !virtq->virtq);
		if (ret) {
			DRV_LOG(ERR,
				"Failed to create event QPs for virtq %d.",
				index);
			return -1;
		}
		attr->mod_fields_bitmap |= MLX5_VIRTQ_MODIFY_TYPE_EVENT_MODE;
		attr->qp_id = virtq->eqp.fw_qp->id;
	} else {
		DRV_LOG(INFO, "Virtq %d is, for sure, working by poll mode, no"
			" need event QPs and event mechanism.", index);
	}
	if (priv->caps.queue_counters_valid) {
		if (!virtq->counters)
			virtq->counters = mlx5_devx_cmd_create_virtio_q_counters
							      (priv->cdev->ctx);
		if (!virtq->counters) {
			DRV_LOG(ERR, "Failed to create virtq couners for virtq"
				" %d.", index);
			return -1;
		}
		attr->counters_obj_id = virtq->counters->id;
	}
	/* Setup 3 UMEMs for each virtq. */
	if (!virtq->virtq) {
		for (i = 0; i < RTE_DIM(virtq->umems); ++i) {
			uint32_t size;
			void *buf;
			struct mlx5dv_devx_umem *obj;

			size =
		priv->caps.umems[i].a * vq->size + priv->caps.umems[i].b;
			if (virtq->umems[i].size == size &&
				virtq->umems[i].obj != NULL) {
				/* Reuse registered memory. */
				memset(virtq->umems[i].buf, 0, size);
				goto reuse;
			}
			if (virtq->umems[i].obj)
				claim_zero(mlx5_glue->devx_umem_dereg
				   (virtq->umems[i].obj));
			rte_free(virtq->umems[i].buf);
			virtq->umems[i].size = 0;
			virtq->umems[i].obj = NULL;
			virtq->umems[i].buf = NULL;
			buf = rte_zmalloc(__func__,
				size, 4096);
			if (buf == NULL) {
				DRV_LOG(ERR, "Cannot allocate umem %d memory for virtq."
				" %u.", i, index);
				return -1;
			}
			obj = mlx5_glue->devx_umem_reg(priv->cdev->ctx,
				buf, size, IBV_ACCESS_LOCAL_WRITE);
			if (obj == NULL) {
				DRV_LOG(ERR, "Failed to register umem %d for virtq %u.",
				i, index);
				rte_free(buf);
				return -1;
			}
			virtq->umems[i].size = size;
			virtq->umems[i].buf = buf;
			virtq->umems[i].obj = obj;
reuse:
			attr->umems[i].id = virtq->umems[i].obj->umem_id;
			attr->umems[i].offset = 0;
			attr->umems[i].size = virtq->umems[i].size;
		}
	}
	if (!is_prepare && attr->q_type == MLX5_VIRTQ_TYPE_SPLIT) {
		gpa = mlx5_vdpa_hva_to_gpa(priv->vmem_info.vmem,
					   (uint64_t)(uintptr_t)vq->desc);
		if (!gpa) {
			DRV_LOG(ERR, "Failed to get descriptor ring GPA.");
			return -1;
		}
		attr->desc_addr = gpa;
		gpa = mlx5_vdpa_hva_to_gpa(priv->vmem_info.vmem,
					   (uint64_t)(uintptr_t)vq->used);
		if (!gpa) {
			DRV_LOG(ERR, "Failed to get GPA for used ring.");
			return -1;
		}
		attr->used_addr = gpa;
		gpa = mlx5_vdpa_hva_to_gpa(priv->vmem_info.vmem,
					   (uint64_t)(uintptr_t)vq->avail);
		if (!gpa) {
			DRV_LOG(ERR, "Failed to get GPA for available ring.");
			return -1;
		}
		attr->available_addr = gpa;
	}
	if (!is_prepare) {
		ret = rte_vhost_get_vring_base(priv->vid,
			index, &last_avail_idx, &last_used_idx);
		if (ret) {
			last_avail_idx = 0;
			last_used_idx = 0;
			DRV_LOG(WARNING, "Couldn't get vring base, idx are set to 0.");
		} else {
			DRV_LOG(INFO, "vid %d: Init last_avail_idx=%d, last_used_idx=%d for "
				"virtq %d.", priv->vid, last_avail_idx,
				last_used_idx, index);
		}
	}
	attr->hw_available_index = last_avail_idx;
	attr->hw_used_index = last_used_idx;
	attr->q_size = vq->size;
	attr->mkey = is_prepare ? 0 : priv->gpa_mkey_index;
	attr->tis_id = priv->tiss[(index / 2) % priv->num_lag_ports]->id;
	attr->queue_index = index;
	attr->pd = priv->cdev->pdn;
	attr->hw_latency_mode = priv->hw_latency_mode;
	attr->hw_max_latency_us = priv->hw_max_latency_us;
	attr->hw_max_pending_comp = priv->hw_max_pending_comp;
	if (attr->hw_latency_mode || attr->hw_max_latency_us ||
		attr->hw_max_pending_comp)
		attr->mod_fields_bitmap |= MLX5_VIRTQ_MODIFY_TYPE_QUEUE_PERIOD;
	return 0;
}

bool
mlx5_vdpa_virtq_single_resource_prepare(struct mlx5_vdpa_priv *priv,
		int index)
{
	struct mlx5_devx_virtq_attr attr = {0};
	struct mlx5_vdpa_virtq *virtq;
	struct rte_vhost_vring vq = {
		.size = priv->queue_size,
		.callfd = -1,
	};
	int ret;

	virtq = &priv->virtqs[index];
	virtq->index = index;
	virtq->vq_size = vq.size;
	virtq->configured = 0;
	virtq->virtq = NULL;
	ret = mlx5_vdpa_virtq_sub_objs_prepare(priv, &attr, &vq, index, true);
	if (ret) {
		DRV_LOG(ERR,
		"Cannot prepare setup resource for virtq %d.", index);
		return true;
	}
	if (mlx5_vdpa_is_modify_virtq_supported(priv)) {
		virtq->virtq =
		mlx5_devx_cmd_create_virtq(priv->cdev->ctx, &attr);
		virtq->priv = priv;
		if (!virtq->virtq)
			return true;
		virtq->rx_csum = attr.rx_csum;
		virtq->virtio_version_1_0 = attr.virtio_version_1_0;
		virtq->event_mode = attr.event_mode;
	}
	return false;
}

bool
mlx5_vdpa_is_modify_virtq_supported(struct mlx5_vdpa_priv *priv)
{
	return (priv->caps.vnet_modify_ext &&
			priv->caps.virtio_net_q_addr_modify &&
			priv->caps.virtio_q_index_modify) ? true : false;
}

static int
mlx5_vdpa_virtq_doorbell_setup(struct mlx5_vdpa_virtq *virtq,
		struct rte_vhost_vring *vq, int index)
{
	virtq->intr_handle = mlx5_os_interrupt_handler_create(
				  RTE_INTR_INSTANCE_F_SHARED, false,
				  vq->kickfd, mlx5_vdpa_virtq_kick_handler, virtq);
	if (virtq->intr_handle == NULL) {
		DRV_LOG(ERR, "Fail to allocate intr_handle for virtq %d.", index);
		return -1;
	}
	return 0;
}

int
mlx5_vdpa_virtq_setup(struct mlx5_vdpa_priv *priv, int index, bool reg_kick)
{
	struct mlx5_vdpa_virtq *virtq = &priv->virtqs[index];
	struct rte_vhost_vring vq;
	struct mlx5_devx_virtq_attr attr = {0};
	int ret;
	uint16_t event_num = MLX5_EVENT_TYPE_OBJECT_CHANGE;
	uint64_t cookie;

	ret = rte_vhost_get_vhost_vring(priv->vid, index, &vq);
	if (ret)
		return -1;
	if (vq.size == 0)
		return 0;
	virtq->priv = priv;
	virtq->stopped = 0;
	ret = mlx5_vdpa_virtq_sub_objs_prepare(priv, &attr,
				&vq, index, false);
	if (ret) {
		DRV_LOG(ERR, "Failed to setup update virtq attr %d.",
			index);
		goto error;
	}
	if (!virtq->virtq) {
		virtq->index = index;
		virtq->vq_size = vq.size;
		virtq->virtq = mlx5_devx_cmd_create_virtq(priv->cdev->ctx,
			&attr);
		if (!virtq->virtq)
			goto error;
		attr.mod_fields_bitmap = MLX5_VIRTQ_MODIFY_TYPE_STATE;
	}
	attr.state = MLX5_VIRTQ_STATE_RDY;
	ret = mlx5_devx_cmd_modify_virtq(virtq->virtq, &attr);
	if (ret) {
		DRV_LOG(ERR, "Failed to modify virtq %d.", index);
		goto error;
	}
	claim_zero(rte_vhost_enable_guest_notification(priv->vid, index, 1));
	virtq->rx_csum = attr.rx_csum;
	virtq->virtio_version_1_0 = attr.virtio_version_1_0;
	virtq->event_mode = attr.event_mode;
	virtq->configured = 1;
	rte_spinlock_lock(&priv->db_lock);
	rte_write32(virtq->index, priv->virtq_db_addr);
	rte_spinlock_unlock(&priv->db_lock);
	/* Setup doorbell mapping. */
	if (reg_kick) {
		if (mlx5_vdpa_virtq_doorbell_setup(virtq, &vq, index)) {
			DRV_LOG(ERR, "Failed to register virtq %d interrupt.",
				index);
			goto error;
		}
	}
	/* Subscribe virtq error event. */
	virtq->version++;
	cookie = ((uint64_t)virtq->version << 32) + index;
	ret = mlx5_glue->devx_subscribe_devx_event(priv->err_chnl,
						   virtq->virtq->obj,
						   sizeof(event_num),
						   &event_num, cookie);
	if (ret) {
		DRV_LOG(ERR, "Failed to subscribe device %d virtq %d error event.",
			priv->vid, index);
		rte_errno = errno;
		goto error;
	}
	/* Initial notification to ask Qemu handling completed buffers. */
	if (virtq->eqp.cq.callfd != -1)
		eventfd_write(virtq->eqp.cq.callfd, (eventfd_t)1);
	DRV_LOG(DEBUG, "vid %u virtq %u was created successfully.", priv->vid,
		index);
	return 0;
error:
	mlx5_vdpa_virtq_unset(virtq);
	return -1;
}

static int
mlx5_vdpa_features_validate(struct mlx5_vdpa_priv *priv)
{
	if (priv->features & (1ULL << VIRTIO_F_RING_PACKED)) {
		if (!(priv->caps.virtio_queue_type & (1 <<
						     MLX5_VIRTQ_TYPE_PACKED))) {
			DRV_LOG(ERR, "Failed to configure PACKED mode for vdev "
				"%d - it was not reported by HW/driver"
				" capability.", priv->vid);
			return -ENOTSUP;
		}
	}
	if (priv->features & (1ULL << VIRTIO_NET_F_HOST_TSO4)) {
		if (!priv->caps.tso_ipv4) {
			DRV_LOG(ERR, "Failed to enable TSO4 for vdev %d - TSO4"
				" was not reported by HW/driver capability.",
				priv->vid);
			return -ENOTSUP;
		}
	}
	if (priv->features & (1ULL << VIRTIO_NET_F_HOST_TSO6)) {
		if (!priv->caps.tso_ipv6) {
			DRV_LOG(ERR, "Failed to enable TSO6 for vdev %d - TSO6"
				" was not reported by HW/driver capability.",
				priv->vid);
			return -ENOTSUP;
		}
	}
	if (priv->features & (1ULL << VIRTIO_NET_F_CSUM)) {
		if (!priv->caps.tx_csum) {
			DRV_LOG(ERR, "Failed to enable CSUM for vdev %d - CSUM"
				" was not reported by HW/driver capability.",
				priv->vid);
			return -ENOTSUP;
		}
	}
	if (priv->features & (1ULL << VIRTIO_NET_F_GUEST_CSUM)) {
		if (!priv->caps.rx_csum) {
			DRV_LOG(ERR, "Failed to enable GUEST CSUM for vdev %d"
				" GUEST CSUM was not reported by HW/driver "
				"capability.", priv->vid);
			return -ENOTSUP;
		}
	}
	if (priv->features & (1ULL << VIRTIO_F_VERSION_1)) {
		if (!priv->caps.virtio_version_1_0) {
			DRV_LOG(ERR, "Failed to enable version 1 for vdev %d "
				"version 1 was not reported by HW/driver"
				" capability.", priv->vid);
			return -ENOTSUP;
		}
	}
	return 0;
}

static bool
mlx5_vdpa_is_pre_created_vq_mismatch(struct mlx5_vdpa_priv *priv,
		struct mlx5_vdpa_virtq *virtq)
{
	struct rte_vhost_vring vq;
	uint32_t event_mode;

	if (virtq->rx_csum !=
		!!(priv->features & (1ULL << VIRTIO_NET_F_GUEST_CSUM)))
		return true;
	if (virtq->virtio_version_1_0 !=
		!!(priv->features & (1ULL << VIRTIO_F_VERSION_1)))
		return true;
	if (rte_vhost_get_vhost_vring(priv->vid, virtq->index, &vq))
		return true;
	if (vq.size != virtq->vq_size)
		return true;
	event_mode = vq.callfd != -1 || !(priv->caps.event_mode &
		(1 << MLX5_VIRTQ_EVENT_MODE_NO_MSIX)) ?
		MLX5_VIRTQ_EVENT_MODE_QP : MLX5_VIRTQ_EVENT_MODE_NO_MSIX;
	if (virtq->event_mode != event_mode)
		return true;
	return false;
}

int
mlx5_vdpa_virtqs_prepare(struct mlx5_vdpa_priv *priv)
{
	int ret = rte_vhost_get_negotiated_features(priv->vid, &priv->features);
	uint16_t nr_vring = rte_vhost_get_vring_num(priv->vid);
	uint32_t remaining_cnt = 0, err_cnt = 0, task_num = 0;
	uint32_t i, thrd_idx, data[1];
	struct mlx5_vdpa_virtq *virtq;
	struct rte_vhost_vring vq;

	if (ret || mlx5_vdpa_features_validate(priv)) {
		DRV_LOG(ERR, "Failed to configure negotiated features.");
		return -1;
	}
	if ((priv->features & (1ULL << VIRTIO_NET_F_CSUM)) == 0 &&
	    ((priv->features & (1ULL << VIRTIO_NET_F_HOST_TSO4)) > 0 ||
	     (priv->features & (1ULL << VIRTIO_NET_F_HOST_TSO6)) > 0)) {
		/* Packet may be corrupted if TSO is enabled without CSUM. */
		DRV_LOG(INFO, "TSO is enabled without CSUM, force CSUM.");
		priv->features |= (1ULL << VIRTIO_NET_F_CSUM);
	}
	if (nr_vring > priv->caps.max_num_virtio_queues) {
		DRV_LOG(ERR, "Do not support more than %d virtqs(%d).",
			(int)priv->caps.max_num_virtio_queues,
			(int)nr_vring);
		return -1;
	}
	priv->nr_virtqs = nr_vring;
	if (priv->use_c_thread) {
		uint32_t main_task_idx[nr_vring];

		for (i = 0; i < nr_vring; i++) {
			virtq = &priv->virtqs[i];
			if (!virtq->enable)
				continue;
			if (priv->queues && virtq->virtq) {
				if (mlx5_vdpa_is_pre_created_vq_mismatch(priv, virtq)) {
					mlx5_vdpa_prepare_virtq_destroy(priv);
					i = 0;
					virtq = &priv->virtqs[i];
					if (!virtq->enable)
						continue;
				}
			}
			thrd_idx = i % (conf_thread_mng.max_thrds + 1);
			if (!thrd_idx) {
				main_task_idx[task_num] = i;
				task_num++;
				continue;
			}
			thrd_idx = priv->last_c_thrd_idx + 1;
			if (thrd_idx >= conf_thread_mng.max_thrds)
				thrd_idx = 0;
			priv->last_c_thrd_idx = thrd_idx;
			data[0] = i;
			if (mlx5_vdpa_task_add(priv, thrd_idx,
				MLX5_VDPA_TASK_SETUP_VIRTQ,
				&remaining_cnt, &err_cnt,
				(void **)&data, 1)) {
				DRV_LOG(ERR, "Fail to add "
						"task setup virtq (%d).", i);
				main_task_idx[task_num] = i;
				task_num++;
			}
		}
		for (i = 0; i < task_num; i++) {
			virtq = &priv->virtqs[main_task_idx[i]];
			pthread_mutex_lock(&virtq->virtq_lock);
			if (mlx5_vdpa_virtq_setup(priv,
				main_task_idx[i], false)) {
				pthread_mutex_unlock(&virtq->virtq_lock);
				goto error;
			}
			virtq->enable = 1;
			pthread_mutex_unlock(&virtq->virtq_lock);
		}
		if (mlx5_vdpa_c_thread_wait_bulk_tasks_done(&remaining_cnt,
			&err_cnt, 2000)) {
			DRV_LOG(ERR,
			"Failed to wait virt-queue setup tasks ready.");
			goto error;
		}
		for (i = 0; i < nr_vring; i++) {
			/* Setup doorbell mapping in order for Qume. */
			virtq = &priv->virtqs[i];
			pthread_mutex_lock(&virtq->virtq_lock);
			if (!virtq->enable || !virtq->configured) {
				pthread_mutex_unlock(&virtq->virtq_lock);
				continue;
			}
			if (rte_vhost_get_vhost_vring(priv->vid, i, &vq)) {
				pthread_mutex_unlock(&virtq->virtq_lock);
				goto error;
			}
			if (mlx5_vdpa_virtq_doorbell_setup(virtq, &vq, i)) {
				pthread_mutex_unlock(&virtq->virtq_lock);
				DRV_LOG(ERR,
				"Failed to register virtq %d interrupt.", i);
				goto error;
			}
			pthread_mutex_unlock(&virtq->virtq_lock);
		}
	} else {
		for (i = 0; i < nr_vring; i++) {
			virtq = &priv->virtqs[i];
			if (!virtq->enable)
				continue;
			if (priv->queues && virtq->virtq) {
				if (mlx5_vdpa_is_pre_created_vq_mismatch(priv,
					virtq)) {
					mlx5_vdpa_prepare_virtq_destroy(
					priv);
					i = 0;
					virtq = &priv->virtqs[i];
					if (!virtq->enable)
						continue;
				}
			}
			pthread_mutex_lock(&virtq->virtq_lock);
			if (mlx5_vdpa_virtq_setup(priv, i, true)) {
				pthread_mutex_unlock(
						&virtq->virtq_lock);
				goto error;
			}
			virtq->enable = 1;
			pthread_mutex_unlock(&virtq->virtq_lock);
		}
	}
	return 0;
error:
	mlx5_vdpa_virtqs_release(priv, true);
	return -1;
}

static int
mlx5_vdpa_virtq_is_modified(struct mlx5_vdpa_priv *priv,
			    struct mlx5_vdpa_virtq *virtq)
{
	struct rte_vhost_vring vq;
	int ret = rte_vhost_get_vhost_vring(priv->vid, virtq->index, &vq);

	if (ret)
		return -1;
	if (vq.size != virtq->vq_size || vq.kickfd !=
	    rte_intr_fd_get(virtq->intr_handle))
		return 1;
	if (virtq->eqp.cq.cq_obj.cq) {
		if (vq.callfd != virtq->eqp.cq.callfd)
			return 1;
	} else if (vq.callfd != -1) {
		return 1;
	}
	return 0;
}

int
mlx5_vdpa_virtq_enable(struct mlx5_vdpa_priv *priv, int index, int enable)
{
	struct mlx5_vdpa_virtq *virtq = &priv->virtqs[index];
	int ret;

	DRV_LOG(INFO, "Update virtq %d status %sable -> %sable.", index,
		virtq->enable ? "en" : "dis", enable ? "en" : "dis");
	if (priv->state == MLX5_VDPA_STATE_PROBED) {
		virtq->enable = !!enable;
		return 0;
	}
	if (virtq->enable == !!enable) {
		if (!enable)
			return 0;
		ret = mlx5_vdpa_virtq_is_modified(priv, virtq);
		if (ret < 0) {
			DRV_LOG(ERR, "Virtq %d modify check failed.", index);
			return -1;
		}
		if (ret == 0)
			return 0;
		DRV_LOG(INFO, "Virtq %d was modified, recreate it.", index);
	}
	if (virtq->configured) {
		virtq->enable = 0;
		if (is_virtq_recvq(virtq->index, priv->nr_virtqs)) {
			ret = mlx5_vdpa_steer_update(priv, false);
			if (ret)
				DRV_LOG(WARNING, "Failed to disable steering "
					"for virtq %d.", index);
		}
		mlx5_vdpa_virtq_unset(virtq);
	} else {
		if (virtq->virtq &&
			mlx5_vdpa_is_pre_created_vq_mismatch(priv, virtq))
			DRV_LOG(WARNING,
			"Configuration mismatch dummy virtq %d.", index);
	}
	if (enable) {
		ret = mlx5_vdpa_virtq_setup(priv, index, true);
		if (ret) {
			DRV_LOG(ERR, "Failed to setup virtq %d.", index);
			return ret;
		}
		virtq->enable = 1;
		if (is_virtq_recvq(virtq->index, priv->nr_virtqs)) {
			ret = mlx5_vdpa_steer_update(priv, false);
			if (ret)
				DRV_LOG(WARNING, "Failed to enable steering "
					"for virtq %d.", index);
		}
	}
	return 0;
}

int
mlx5_vdpa_virtq_stats_get(struct mlx5_vdpa_priv *priv, int qid,
			  struct rte_vdpa_stat *stats, unsigned int n)
{
	struct mlx5_vdpa_virtq *virtq = &priv->virtqs[qid];
	struct mlx5_devx_virtio_q_couners_attr *attr = &virtq->stats;
	int ret;

	if (!virtq->counters) {
		DRV_LOG(ERR, "Failed to read virtq %d statistics - virtq "
			"is invalid.", qid);
		return -EINVAL;
	}
	ret = mlx5_devx_cmd_query_virtio_q_counters(virtq->counters, attr);
	if (ret) {
		DRV_LOG(ERR, "Failed to read virtq %d stats from HW.", qid);
		return ret;
	}
	ret = (int)RTE_MIN(n, (unsigned int)MLX5_VDPA_STATS_MAX);
	if (ret == MLX5_VDPA_STATS_RECEIVED_DESCRIPTORS)
		return ret;
	stats[MLX5_VDPA_STATS_RECEIVED_DESCRIPTORS] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_RECEIVED_DESCRIPTORS,
		.value = attr->received_desc - virtq->reset.received_desc,
	};
	if (ret == MLX5_VDPA_STATS_COMPLETED_DESCRIPTORS)
		return ret;
	stats[MLX5_VDPA_STATS_COMPLETED_DESCRIPTORS] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_COMPLETED_DESCRIPTORS,
		.value = attr->completed_desc - virtq->reset.completed_desc,
	};
	if (ret == MLX5_VDPA_STATS_BAD_DESCRIPTOR_ERRORS)
		return ret;
	stats[MLX5_VDPA_STATS_BAD_DESCRIPTOR_ERRORS] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_BAD_DESCRIPTOR_ERRORS,
		.value = attr->bad_desc_errors - virtq->reset.bad_desc_errors,
	};
	if (ret == MLX5_VDPA_STATS_EXCEED_MAX_CHAIN)
		return ret;
	stats[MLX5_VDPA_STATS_EXCEED_MAX_CHAIN] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_EXCEED_MAX_CHAIN,
		.value = attr->exceed_max_chain - virtq->reset.exceed_max_chain,
	};
	if (ret == MLX5_VDPA_STATS_INVALID_BUFFER)
		return ret;
	stats[MLX5_VDPA_STATS_INVALID_BUFFER] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_INVALID_BUFFER,
		.value = attr->invalid_buffer - virtq->reset.invalid_buffer,
	};
	if (ret == MLX5_VDPA_STATS_COMPLETION_ERRORS)
		return ret;
	stats[MLX5_VDPA_STATS_COMPLETION_ERRORS] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_COMPLETION_ERRORS,
		.value = attr->error_cqes - virtq->reset.error_cqes,
	};
	return ret;
}

int
mlx5_vdpa_virtq_stats_reset(struct mlx5_vdpa_priv *priv, int qid)
{
	struct mlx5_vdpa_virtq *virtq = &priv->virtqs[qid];
	int ret;

	if (virtq->counters == NULL) /* VQ not enabled. */
		return 0;
	ret = mlx5_devx_cmd_query_virtio_q_counters(virtq->counters,
						    &virtq->reset);
	if (ret)
		DRV_LOG(ERR, "Failed to read virtq %d reset stats from HW.",
			qid);
	return ret;
}
