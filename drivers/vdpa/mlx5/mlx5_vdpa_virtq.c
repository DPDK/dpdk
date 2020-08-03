/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_io.h>

#include <mlx5_common.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"


static void
mlx5_vdpa_virtq_handler(void *cb_arg)
{
	struct mlx5_vdpa_virtq *virtq = cb_arg;
	struct mlx5_vdpa_priv *priv = virtq->priv;
	uint64_t buf;
	int nbytes;

	do {
		nbytes = read(virtq->intr_handle.fd, &buf, 8);
		if (nbytes < 0) {
			if (errno == EINTR ||
			    errno == EWOULDBLOCK ||
			    errno == EAGAIN)
				continue;
			DRV_LOG(ERR,  "Failed to read kickfd of virtq %d: %s",
				virtq->index, strerror(errno));
		}
		break;
	} while (1);
	rte_write32(virtq->index, priv->virtq_db_addr);
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

static int
mlx5_vdpa_virtq_unset(struct mlx5_vdpa_virtq *virtq)
{
	unsigned int i;
	int retries = MLX5_VDPA_INTR_RETRIES;
	int ret = -EAGAIN;

	if (virtq->intr_handle.fd != -1) {
		while (retries-- && ret == -EAGAIN) {
			ret = rte_intr_callback_unregister(&virtq->intr_handle,
							mlx5_vdpa_virtq_handler,
							virtq);
			if (ret == -EAGAIN) {
				DRV_LOG(DEBUG, "Try again to unregister fd %d "
					"of virtq %d interrupt, retries = %d.",
					virtq->intr_handle.fd,
					(int)virtq->index, retries);
				usleep(MLX5_VDPA_INTR_RETRIES_USEC);
			}
		}
		virtq->intr_handle.fd = -1;
	}
	if (virtq->virtq) {
		ret = mlx5_vdpa_virtq_stop(virtq->priv, virtq->index);
		if (ret)
			DRV_LOG(WARNING, "Failed to stop virtq %d.",
				virtq->index);
		claim_zero(mlx5_devx_cmd_destroy(virtq->virtq));
	}
	virtq->virtq = NULL;
	for (i = 0; i < RTE_DIM(virtq->umems); ++i) {
		if (virtq->umems[i].obj)
			claim_zero(mlx5_glue->devx_umem_dereg
							 (virtq->umems[i].obj));
		if (virtq->umems[i].buf)
			rte_free(virtq->umems[i].buf);
	}
	memset(&virtq->umems, 0, sizeof(virtq->umems));
	if (virtq->counters) {
		claim_zero(mlx5_devx_cmd_destroy(virtq->counters));
		virtq->counters = NULL;
	}
	memset(&virtq->reset, 0, sizeof(virtq->reset));
	if (virtq->eqp.fw_qp)
		mlx5_vdpa_event_qp_destroy(&virtq->eqp);
	virtq->notifier_state = MLX5_VDPA_NOTIFIER_STATE_DISABLED;
	return 0;
}

void
mlx5_vdpa_virtqs_release(struct mlx5_vdpa_priv *priv)
{
	int i;

	for (i = 0; i < priv->nr_virtqs; i++)
		mlx5_vdpa_virtq_unset(&priv->virtqs[i]);
	if (priv->tis) {
		claim_zero(mlx5_devx_cmd_destroy(priv->tis));
		priv->tis = NULL;
	}
	if (priv->td) {
		claim_zero(mlx5_devx_cmd_destroy(priv->td));
		priv->td = NULL;
	}
	if (priv->virtq_db_addr) {
		claim_zero(munmap(priv->virtq_db_addr, priv->var->length));
		priv->virtq_db_addr = NULL;
	}
	priv->features = 0;
	priv->nr_virtqs = 0;
}

int
mlx5_vdpa_virtq_modify(struct mlx5_vdpa_virtq *virtq, int state)
{
	struct mlx5_devx_virtq_attr attr = {
			.type = MLX5_VIRTQ_MODIFY_TYPE_STATE,
			.state = state ? MLX5_VIRTQ_STATE_RDY :
					 MLX5_VIRTQ_STATE_SUSPEND,
			.queue_index = virtq->index,
	};

	return mlx5_devx_cmd_modify_virtq(virtq->virtq, &attr);
}

int
mlx5_vdpa_virtq_stop(struct mlx5_vdpa_priv *priv, int index)
{
	struct mlx5_devx_virtq_attr attr = {0};
	struct mlx5_vdpa_virtq *virtq = &priv->virtqs[index];
	int ret;

	if (virtq->stopped)
		return 0;
	ret = mlx5_vdpa_virtq_modify(virtq, 0);
	if (ret)
		return -1;
	virtq->stopped = true;
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
	DRV_LOG(DEBUG, "vid %u virtq %u was stopped.", priv->vid, index);
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
mlx5_vdpa_virtq_setup(struct mlx5_vdpa_priv *priv, int index)
{
	struct mlx5_vdpa_virtq *virtq = &priv->virtqs[index];
	struct rte_vhost_vring vq;
	struct mlx5_devx_virtq_attr attr = {0};
	uint64_t gpa;
	int ret;
	unsigned int i;
	uint16_t last_avail_idx;
	uint16_t last_used_idx;

	ret = rte_vhost_get_vhost_vring(priv->vid, index, &vq);
	if (ret)
		return -1;
	virtq->index = index;
	virtq->vq_size = vq.size;
	attr.tso_ipv4 = !!(priv->features & (1ULL << VIRTIO_NET_F_HOST_TSO4));
	attr.tso_ipv6 = !!(priv->features & (1ULL << VIRTIO_NET_F_HOST_TSO6));
	attr.tx_csum = !!(priv->features & (1ULL << VIRTIO_NET_F_CSUM));
	attr.rx_csum = !!(priv->features & (1ULL << VIRTIO_NET_F_GUEST_CSUM));
	attr.virtio_version_1_0 = !!(priv->features & (1ULL <<
							VIRTIO_F_VERSION_1));
	attr.type = (priv->features & (1ULL << VIRTIO_F_RING_PACKED)) ?
			MLX5_VIRTQ_TYPE_PACKED : MLX5_VIRTQ_TYPE_SPLIT;
	/*
	 * No need event QPs creation when the guest in poll mode or when the
	 * capability allows it.
	 */
	attr.event_mode = vq.callfd != -1 || !(priv->caps.event_mode & (1 <<
					       MLX5_VIRTQ_EVENT_MODE_NO_MSIX)) ?
						      MLX5_VIRTQ_EVENT_MODE_QP :
						  MLX5_VIRTQ_EVENT_MODE_NO_MSIX;
	if (attr.event_mode == MLX5_VIRTQ_EVENT_MODE_QP) {
		ret = mlx5_vdpa_event_qp_create(priv, vq.size, vq.callfd,
						&virtq->eqp);
		if (ret) {
			DRV_LOG(ERR, "Failed to create event QPs for virtq %d.",
				index);
			return -1;
		}
		attr.qp_id = virtq->eqp.fw_qp->id;
	} else {
		DRV_LOG(INFO, "Virtq %d is, for sure, working by poll mode, no"
			" need event QPs and event mechanism.", index);
	}
	if (priv->caps.queue_counters_valid) {
		virtq->counters = mlx5_devx_cmd_create_virtio_q_counters
								    (priv->ctx);
		if (!virtq->counters) {
			DRV_LOG(ERR, "Failed to create virtq couners for virtq"
				" %d.", index);
			goto error;
		}
		attr.counters_obj_id = virtq->counters->id;
	}
	/* Setup 3 UMEMs for each virtq. */
	for (i = 0; i < RTE_DIM(virtq->umems); ++i) {
		virtq->umems[i].size = priv->caps.umems[i].a * vq.size +
							  priv->caps.umems[i].b;
		virtq->umems[i].buf = rte_zmalloc(__func__,
						  virtq->umems[i].size, 4096);
		if (!virtq->umems[i].buf) {
			DRV_LOG(ERR, "Cannot allocate umem %d memory for virtq"
				" %u.", i, index);
			goto error;
		}
		virtq->umems[i].obj = mlx5_glue->devx_umem_reg(priv->ctx,
							virtq->umems[i].buf,
							virtq->umems[i].size,
							IBV_ACCESS_LOCAL_WRITE);
		if (!virtq->umems[i].obj) {
			DRV_LOG(ERR, "Failed to register umem %d for virtq %u.",
				i, index);
			goto error;
		}
		attr.umems[i].id = virtq->umems[i].obj->umem_id;
		attr.umems[i].offset = 0;
		attr.umems[i].size = virtq->umems[i].size;
	}
	if (attr.type == MLX5_VIRTQ_TYPE_SPLIT) {
		gpa = mlx5_vdpa_hva_to_gpa(priv->vmem,
					   (uint64_t)(uintptr_t)vq.desc);
		if (!gpa) {
			DRV_LOG(ERR, "Failed to get descriptor ring GPA.");
			goto error;
		}
		attr.desc_addr = gpa;
		gpa = mlx5_vdpa_hva_to_gpa(priv->vmem,
					   (uint64_t)(uintptr_t)vq.used);
		if (!gpa) {
			DRV_LOG(ERR, "Failed to get GPA for used ring.");
			goto error;
		}
		attr.used_addr = gpa;
		gpa = mlx5_vdpa_hva_to_gpa(priv->vmem,
					   (uint64_t)(uintptr_t)vq.avail);
		if (!gpa) {
			DRV_LOG(ERR, "Failed to get GPA for available ring.");
			goto error;
		}
		attr.available_addr = gpa;
	}
	ret = rte_vhost_get_vring_base(priv->vid, index, &last_avail_idx,
				 &last_used_idx);
	if (ret) {
		last_avail_idx = 0;
		last_used_idx = 0;
		DRV_LOG(WARNING, "Couldn't get vring base, idx are set to 0");
	} else {
		DRV_LOG(INFO, "vid %d: Init last_avail_idx=%d, last_used_idx=%d for "
				"virtq %d.", priv->vid, last_avail_idx,
				last_used_idx, index);
	}
	attr.hw_available_index = last_avail_idx;
	attr.hw_used_index = last_used_idx;
	attr.q_size = vq.size;
	attr.mkey = priv->gpa_mkey_index;
	attr.tis_id = priv->tis->id;
	attr.queue_index = index;
	attr.pd = priv->pdn;
	virtq->virtq = mlx5_devx_cmd_create_virtq(priv->ctx, &attr);
	virtq->priv = priv;
	if (!virtq->virtq)
		goto error;
	claim_zero(rte_vhost_enable_guest_notification(priv->vid, index, 1));
	if (mlx5_vdpa_virtq_modify(virtq, 1))
		goto error;
	virtq->priv = priv;
	rte_write32(virtq->index, priv->virtq_db_addr);
	/* Setup doorbell mapping. */
	virtq->intr_handle.fd = vq.kickfd;
	if (virtq->intr_handle.fd == -1) {
		DRV_LOG(WARNING, "Virtq %d kickfd is invalid.", index);
	} else {
		virtq->intr_handle.type = RTE_INTR_HANDLE_EXT;
		if (rte_intr_callback_register(&virtq->intr_handle,
					       mlx5_vdpa_virtq_handler,
					       virtq)) {
			virtq->intr_handle.fd = -1;
			DRV_LOG(ERR, "Failed to register virtq %d interrupt.",
				index);
			goto error;
		} else {
			DRV_LOG(DEBUG, "Register fd %d interrupt for virtq %d.",
				virtq->intr_handle.fd, index);
		}
	}
	virtq->stopped = false;
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
			DRV_LOG(ERR, "Failed to configur PACKED mode for vdev "
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

int
mlx5_vdpa_virtqs_prepare(struct mlx5_vdpa_priv *priv)
{
	struct mlx5_devx_tis_attr tis_attr = {0};
	uint32_t i;
	uint16_t nr_vring = rte_vhost_get_vring_num(priv->vid);
	int ret = rte_vhost_get_negotiated_features(priv->vid, &priv->features);

	if (ret || mlx5_vdpa_features_validate(priv)) {
		DRV_LOG(ERR, "Failed to configure negotiated features.");
		return -1;
	}
	if (nr_vring > priv->caps.max_num_virtio_queues * 2) {
		DRV_LOG(ERR, "Do not support more than %d virtqs(%d).",
			(int)priv->caps.max_num_virtio_queues * 2,
			(int)nr_vring);
		return -1;
	}
	/* Always map the entire page. */
	priv->virtq_db_addr = mmap(NULL, priv->var->length, PROT_READ |
				   PROT_WRITE, MAP_SHARED, priv->ctx->cmd_fd,
				   priv->var->mmap_off);
	if (priv->virtq_db_addr == MAP_FAILED) {
		DRV_LOG(ERR, "Failed to map doorbell page %u.", errno);
		priv->virtq_db_addr = NULL;
		goto error;
	} else {
		DRV_LOG(DEBUG, "VAR address of doorbell mapping is %p.",
			priv->virtq_db_addr);
	}
	priv->td = mlx5_devx_cmd_create_td(priv->ctx);
	if (!priv->td) {
		DRV_LOG(ERR, "Failed to create transport domain.");
		return -rte_errno;
	}
	tis_attr.transport_domain = priv->td->id;
	priv->tis = mlx5_devx_cmd_create_tis(priv->ctx, &tis_attr);
	if (!priv->tis) {
		DRV_LOG(ERR, "Failed to create TIS.");
		goto error;
	}
	priv->nr_virtqs = nr_vring;
	for (i = 0; i < nr_vring; i++)
		if (priv->virtqs[i].enable && mlx5_vdpa_virtq_setup(priv, i))
			goto error;
	return 0;
error:
	mlx5_vdpa_virtqs_release(priv);
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
	if (vq.size != virtq->vq_size || vq.kickfd != virtq->intr_handle.fd)
		return 1;
	if (virtq->eqp.cq.cq) {
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
	if (!priv->configured) {
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
	if (virtq->virtq) {
		virtq->enable = 0;
		if (is_virtq_recvq(virtq->index, priv->nr_virtqs)) {
			ret = mlx5_vdpa_steer_update(priv);
			if (ret)
				DRV_LOG(WARNING, "Failed to disable steering "
					"for virtq %d.", index);
		}
		mlx5_vdpa_virtq_unset(virtq);
	}
	if (enable) {
		ret = mlx5_vdpa_virtq_setup(priv, index);
		if (ret) {
			DRV_LOG(ERR, "Failed to setup virtq %d.", index);
			return ret;
		}
		virtq->enable = 1;
		if (is_virtq_recvq(virtq->index, priv->nr_virtqs)) {
			ret = mlx5_vdpa_steer_update(priv);
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
	struct mlx5_devx_virtio_q_couners_attr attr = {0};
	int ret;

	if (!virtq->virtq || !virtq->enable) {
		DRV_LOG(ERR, "Failed to read virtq %d statistics - virtq "
			"is invalid.", qid);
		return -EINVAL;
	}
	MLX5_ASSERT(virtq->counters);
	ret = mlx5_devx_cmd_query_virtio_q_counters(virtq->counters, &attr);
	if (ret) {
		DRV_LOG(ERR, "Failed to read virtq %d stats from HW.", qid);
		return ret;
	}
	ret = (int)RTE_MIN(n, (unsigned int)MLX5_VDPA_STATS_MAX);
	if (ret == MLX5_VDPA_STATS_RECEIVED_DESCRIPTORS)
		return ret;
	stats[MLX5_VDPA_STATS_RECEIVED_DESCRIPTORS] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_RECEIVED_DESCRIPTORS,
		.value = attr.received_desc - virtq->reset.received_desc,
	};
	if (ret == MLX5_VDPA_STATS_COMPLETED_DESCRIPTORS)
		return ret;
	stats[MLX5_VDPA_STATS_COMPLETED_DESCRIPTORS] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_COMPLETED_DESCRIPTORS,
		.value = attr.completed_desc - virtq->reset.completed_desc,
	};
	if (ret == MLX5_VDPA_STATS_BAD_DESCRIPTOR_ERRORS)
		return ret;
	stats[MLX5_VDPA_STATS_BAD_DESCRIPTOR_ERRORS] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_BAD_DESCRIPTOR_ERRORS,
		.value = attr.bad_desc_errors - virtq->reset.bad_desc_errors,
	};
	if (ret == MLX5_VDPA_STATS_EXCEED_MAX_CHAIN)
		return ret;
	stats[MLX5_VDPA_STATS_EXCEED_MAX_CHAIN] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_EXCEED_MAX_CHAIN,
		.value = attr.exceed_max_chain - virtq->reset.exceed_max_chain,
	};
	if (ret == MLX5_VDPA_STATS_INVALID_BUFFER)
		return ret;
	stats[MLX5_VDPA_STATS_INVALID_BUFFER] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_INVALID_BUFFER,
		.value = attr.invalid_buffer - virtq->reset.invalid_buffer,
	};
	if (ret == MLX5_VDPA_STATS_COMPLETION_ERRORS)
		return ret;
	stats[MLX5_VDPA_STATS_COMPLETION_ERRORS] = (struct rte_vdpa_stat) {
		.id = MLX5_VDPA_STATS_COMPLETION_ERRORS,
		.value = attr.error_cqes - virtq->reset.error_cqes,
	};
	return ret;
}

int
mlx5_vdpa_virtq_stats_reset(struct mlx5_vdpa_priv *priv, int qid)
{
	struct mlx5_vdpa_virtq *virtq = &priv->virtqs[qid];
	int ret;

	if (!virtq->virtq || !virtq->enable) {
		DRV_LOG(ERR, "Failed to read virtq %d statistics - virtq "
			"is invalid.", qid);
		return -EINVAL;
	}
	MLX5_ASSERT(virtq->counters);
	ret = mlx5_devx_cmd_query_virtio_q_counters(virtq->counters,
						    &virtq->reset);
	if (ret)
		DRV_LOG(ERR, "Failed to read virtq %d reset stats from HW.",
			qid);
	return ret;
}
