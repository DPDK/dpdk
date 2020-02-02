/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <string.h>

#include <rte_malloc.h>
#include <rte_errno.h>

#include <mlx5_common.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"


static int
mlx5_vdpa_virtq_unset(struct mlx5_vdpa_virtq *virtq)
{
	int i;

	if (virtq->virtq) {
		claim_zero(mlx5_devx_cmd_destroy(virtq->virtq));
		virtq->virtq = NULL;
	}
	for (i = 0; i < 3; ++i) {
		if (virtq->umems[i].obj)
			claim_zero(mlx5_glue->devx_umem_dereg
							 (virtq->umems[i].obj));
		if (virtq->umems[i].buf)
			rte_free(virtq->umems[i].buf);
	}
	memset(&virtq->umems, 0, sizeof(virtq->umems));
	if (virtq->eqp.fw_qp)
		mlx5_vdpa_event_qp_destroy(&virtq->eqp);
	return 0;
}

void
mlx5_vdpa_virtqs_release(struct mlx5_vdpa_priv *priv)
{
	struct mlx5_vdpa_virtq *entry;
	struct mlx5_vdpa_virtq *next;

	entry = SLIST_FIRST(&priv->virtq_list);
	while (entry) {
		next = SLIST_NEXT(entry, next);
		mlx5_vdpa_virtq_unset(entry);
		SLIST_REMOVE(&priv->virtq_list, entry, mlx5_vdpa_virtq, next);
		rte_free(entry);
		entry = next;
	}
	SLIST_INIT(&priv->virtq_list);
	if (priv->tis) {
		claim_zero(mlx5_devx_cmd_destroy(priv->tis));
		priv->tis = NULL;
	}
	if (priv->td) {
		claim_zero(mlx5_devx_cmd_destroy(priv->td));
		priv->td = NULL;
	}
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
mlx5_vdpa_virtq_setup(struct mlx5_vdpa_priv *priv,
		      struct mlx5_vdpa_virtq *virtq, int index)
{
	struct rte_vhost_vring vq;
	struct mlx5_devx_virtq_attr attr = {0};
	uint64_t gpa;
	int ret;
	int i;
	uint16_t last_avail_idx;
	uint16_t last_used_idx;

	ret = rte_vhost_get_vhost_vring(priv->vid, index, &vq);
	if (ret)
		return -1;
	virtq->index = index;
	virtq->vq_size = vq.size;
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
	/* Setup 3 UMEMs for each virtq. */
	for (i = 0; i < 3; ++i) {
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
	gpa = mlx5_vdpa_hva_to_gpa(priv->vmem, (uint64_t)(uintptr_t)vq.desc);
	if (!gpa) {
		DRV_LOG(ERR, "Fail to get GPA for descriptor ring.");
		goto error;
	}
	attr.desc_addr = gpa;
	gpa = mlx5_vdpa_hva_to_gpa(priv->vmem, (uint64_t)(uintptr_t)vq.used);
	if (!gpa) {
		DRV_LOG(ERR, "Fail to get GPA for used ring.");
		goto error;
	}
	attr.used_addr = gpa;
	gpa = mlx5_vdpa_hva_to_gpa(priv->vmem, (uint64_t)(uintptr_t)vq.avail);
	if (!gpa) {
		DRV_LOG(ERR, "Fail to get GPA for available ring.");
		goto error;
	}
	attr.available_addr = gpa;
	rte_vhost_get_vring_base(priv->vid, index, &last_avail_idx,
				 &last_used_idx);
	DRV_LOG(INFO, "vid %d: Init last_avail_idx=%d, last_used_idx=%d for "
		"virtq %d.", priv->vid, last_avail_idx, last_used_idx, index);
	attr.hw_available_index = last_avail_idx;
	attr.hw_used_index = last_used_idx;
	attr.q_size = vq.size;
	attr.mkey = priv->gpa_mkey_index;
	attr.tis_id = priv->tis->id;
	attr.queue_index = index;
	virtq->virtq = mlx5_devx_cmd_create_virtq(priv->ctx, &attr);
	if (!virtq->virtq)
		goto error;
	return 0;
error:
	mlx5_vdpa_virtq_unset(virtq);
	return -1;
}

int
mlx5_vdpa_virtqs_prepare(struct mlx5_vdpa_priv *priv)
{
	struct mlx5_devx_tis_attr tis_attr = {0};
	struct mlx5_vdpa_virtq *virtq;
	uint32_t i;
	uint16_t nr_vring = rte_vhost_get_vring_num(priv->vid);

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
	for (i = 0; i < nr_vring; i++) {
		virtq = rte_zmalloc(__func__, sizeof(*virtq), 0);
		if (!virtq || mlx5_vdpa_virtq_setup(priv, virtq, i)) {
			if (virtq)
				rte_free(virtq);
			goto error;
		}
		SLIST_INSERT_HEAD(&priv->virtq_list, virtq, next);
	}
	priv->nr_virtqs = nr_vring;
	return 0;
error:
	mlx5_vdpa_virtqs_release(priv);
	return -1;
}
