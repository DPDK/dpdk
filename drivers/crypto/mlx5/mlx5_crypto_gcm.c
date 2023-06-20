/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <bus_pci_driver.h>
#include <rte_memory.h>

#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common_os.h>

#include "mlx5_crypto_utils.h"
#include "mlx5_crypto.h"

/*
 * AES-GCM uses indirect KLM mode. The UMR WQE comprises of WQE control +
 * UMR control + mkey context + indirect KLM. The WQE size is aligned to
 * be 3 WQEBBS.
 */
#define MLX5_UMR_GCM_WQE_SIZE \
	(RTE_ALIGN(sizeof(struct mlx5_umr_wqe) + sizeof(struct mlx5_wqe_dseg), \
			MLX5_SEND_WQE_BB))

#define MLX5_UMR_GCM_WQE_SET_SIZE \
	(MLX5_UMR_GCM_WQE_SIZE + \
	 RTE_ALIGN(sizeof(struct mlx5_wqe_send_en_wqe), \
	 MLX5_SEND_WQE_BB))

static struct rte_cryptodev_capabilities mlx5_crypto_gcm_caps[] = {
	{
		.op = RTE_CRYPTO_OP_TYPE_UNDEFINED,
	},
	{
		.op = RTE_CRYPTO_OP_TYPE_UNDEFINED,
	}
};

int
mlx5_crypto_dek_fill_gcm_attr(struct mlx5_crypto_dek *dek,
			      struct mlx5_devx_dek_attr *dek_attr,
			      void *cb_ctx)
{
	uint32_t offset = 0;
	struct mlx5_crypto_dek_ctx *ctx = cb_ctx;
	struct rte_crypto_aead_xform *aead_ctx = &ctx->xform->aead;

	if (aead_ctx->algo != RTE_CRYPTO_AEAD_AES_GCM) {
		DRV_LOG(ERR, "Only AES-GCM algo supported.");
		return -EINVAL;
	}
	dek_attr->key_purpose = MLX5_CRYPTO_KEY_PURPOSE_GCM;
	switch (aead_ctx->key.length) {
	case 16:
		offset = 16;
		dek->size = 16;
		dek_attr->key_size = MLX5_CRYPTO_KEY_SIZE_128b;
		break;
	case 32:
		dek->size = 32;
		dek_attr->key_size = MLX5_CRYPTO_KEY_SIZE_256b;
		break;
	default:
		DRV_LOG(ERR, "Wrapped key size not supported.");
		return -EINVAL;
	}
	memcpy(&dek_attr->key[offset], aead_ctx->key.data, aead_ctx->key.length);
	memcpy(&dek->data, aead_ctx->key.data, aead_ctx->key.length);
	return 0;
}

static int
mlx5_crypto_sym_gcm_session_configure(struct rte_cryptodev *dev,
				  struct rte_crypto_sym_xform *xform,
				  struct rte_cryptodev_sym_session *session)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_crypto_session *sess_private_data = CRYPTODEV_GET_SYM_SESS_PRIV(session);
	struct rte_crypto_aead_xform *aead = &xform->aead;
	uint32_t op_type;

	if (unlikely(xform->next != NULL)) {
		DRV_LOG(ERR, "Xform next is not supported.");
		return -ENOTSUP;
	}
	if (aead->algo != RTE_CRYPTO_AEAD_AES_GCM) {
		DRV_LOG(ERR, "Only AES-GCM algorithm is supported.");
		return -ENOTSUP;
	}
	if (aead->op == RTE_CRYPTO_AEAD_OP_ENCRYPT)
		op_type = MLX5_CRYPTO_OP_TYPE_ENCRYPTION;
	else
		op_type = MLX5_CRYPTO_OP_TYPE_DECRYPTION;
	sess_private_data->op_type = op_type;
	sess_private_data->mmo_ctrl = rte_cpu_to_be_32
			(op_type << MLX5_CRYPTO_MMO_OP_OFFSET |
			 MLX5_ENCRYPTION_TYPE_AES_GCM << MLX5_CRYPTO_MMO_TYPE_OFFSET);
	sess_private_data->wqe_aad_len = rte_cpu_to_be_32((uint32_t)aead->aad_length);
	sess_private_data->wqe_tag_len = rte_cpu_to_be_32((uint32_t)aead->digest_length);
	sess_private_data->aad_len = aead->aad_length;
	sess_private_data->tag_len = aead->digest_length;
	sess_private_data->iv_offset = aead->iv.offset;
	sess_private_data->iv_len = aead->iv.length;
	sess_private_data->dek = mlx5_crypto_dek_prepare(priv, xform);
	if (sess_private_data->dek == NULL) {
		DRV_LOG(ERR, "Failed to prepare dek.");
		return -ENOMEM;
	}
	sess_private_data->dek_id =
			rte_cpu_to_be_32(sess_private_data->dek->obj->id &
					 0xffffff);
	DRV_LOG(DEBUG, "Session %p was configured.", sess_private_data);
	return 0;
}

static void *
mlx5_crypto_gcm_mkey_klm_update(struct mlx5_crypto_priv *priv,
				struct mlx5_crypto_qp *qp __rte_unused,
				uint32_t idx)
{
	return &qp->klm_array[idx * priv->max_klm_num];
}

static int
mlx5_crypto_gcm_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_crypto_qp *qp = dev->data->queue_pairs[qp_id];

	if (qp->umr_qp_obj.qp != NULL)
		mlx5_devx_qp_destroy(&qp->umr_qp_obj);
	if (qp->qp_obj.qp != NULL)
		mlx5_devx_qp_destroy(&qp->qp_obj);
	if (qp->cq_obj.cq != NULL)
		mlx5_devx_cq_destroy(&qp->cq_obj);
	if (qp->mr.obj != NULL) {
		void *opaq = qp->mr.addr;

		priv->dereg_mr_cb(&qp->mr);
		rte_free(opaq);
	}
	mlx5_crypto_indirect_mkeys_release(qp, qp->entries_n);
	mlx5_mr_btree_free(&qp->mr_ctrl.cache_bh);
	rte_free(qp);
	dev->data->queue_pairs[qp_id] = NULL;
	return 0;
}

static void
mlx5_crypto_gcm_init_qp(struct mlx5_crypto_qp *qp)
{
	volatile struct mlx5_gga_wqe *restrict wqe =
				    (volatile struct mlx5_gga_wqe *)qp->qp_obj.wqes;
	volatile union mlx5_gga_crypto_opaque *opaq = qp->opaque_addr;
	const uint32_t sq_ds = rte_cpu_to_be_32((qp->qp_obj.qp->id << 8) | 4u);
	const uint32_t flags = RTE_BE32(MLX5_COMP_ALWAYS <<
					MLX5_COMP_MODE_OFFSET);
	const uint32_t opaq_lkey = rte_cpu_to_be_32(qp->mr.lkey);
	int i;

	/* All the next fields state should stay constant. */
	for (i = 0; i < qp->entries_n; ++i, ++wqe) {
		wqe->sq_ds = sq_ds;
		wqe->flags = flags;
		wqe->opaque_lkey = opaq_lkey;
		wqe->opaque_vaddr = rte_cpu_to_be_64((uint64_t)(uintptr_t)&opaq[i]);
	}
}

static inline int
mlx5_crypto_gcm_umr_qp_setup(struct rte_cryptodev *dev, struct mlx5_crypto_qp *qp,
			     int socket_id)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_devx_qp_attr attr = {0};
	uint32_t ret;
	uint32_t log_wqbb_n;

	/* Set UMR + SEND_EN WQE as maximum same with crypto. */
	log_wqbb_n = rte_log2_u32(qp->entries_n *
			(MLX5_UMR_GCM_WQE_SET_SIZE / MLX5_SEND_WQE_BB));
	attr.pd = priv->cdev->pdn;
	attr.uar_index = mlx5_os_get_devx_uar_page_id(priv->uar.obj);
	attr.cqn = qp->cq_obj.cq->id;
	attr.num_of_receive_wqes = 0;
	attr.num_of_send_wqbbs = RTE_BIT32(log_wqbb_n);
	attr.ts_format =
		mlx5_ts_format_conv(priv->cdev->config.hca_attr.qp_ts_format);
	attr.cd_master = 1;
	ret = mlx5_devx_qp_create(priv->cdev->ctx, &qp->umr_qp_obj,
				  attr.num_of_send_wqbbs * MLX5_SEND_WQE_BB,
				  &attr, socket_id);
	if (ret) {
		DRV_LOG(ERR, "Failed to create UMR QP.");
		return -1;
	}
	if (mlx5_devx_qp2rts(&qp->umr_qp_obj, qp->umr_qp_obj.qp->id)) {
		DRV_LOG(ERR, "Failed to change UMR QP state to RTS.");
		return -1;
	}
	/* Save the UMR WQEBBS for checking the WQE boundary. */
	qp->umr_wqbbs = attr.num_of_send_wqbbs;
	return 0;
}

static int
mlx5_crypto_gcm_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
			 const struct rte_cryptodev_qp_conf *qp_conf,
			 int socket_id)
{
	struct mlx5_crypto_priv *priv = dev->data->dev_private;
	struct mlx5_hca_attr *attr = &priv->cdev->config.hca_attr;
	struct mlx5_crypto_qp *qp;
	struct mlx5_devx_cq_attr cq_attr = {
		.uar_page_id = mlx5_os_get_devx_uar_page_id(priv->uar.obj),
	};
	struct mlx5_devx_qp_attr qp_attr = {
		.pd = priv->cdev->pdn,
		.uar_index = mlx5_os_get_devx_uar_page_id(priv->uar.obj),
		.user_index = qp_id,
	};
	struct mlx5_devx_mkey_attr mkey_attr = {
		.pd = priv->cdev->pdn,
		.umr_en = 1,
		.klm_num = priv->max_klm_num,
	};
	uint32_t log_ops_n = rte_log2_u32(qp_conf->nb_descriptors);
	uint32_t entries = RTE_BIT32(log_ops_n);
	uint32_t alloc_size = sizeof(*qp);
	size_t mr_size, opaq_size;
	void *mr_buf;
	int ret;

	alloc_size = RTE_ALIGN(alloc_size, RTE_CACHE_LINE_SIZE);
	alloc_size += (sizeof(struct rte_crypto_op *) +
		       sizeof(struct mlx5_devx_obj *)) * entries;
	qp = rte_zmalloc_socket(__func__, alloc_size, RTE_CACHE_LINE_SIZE,
				socket_id);
	if (qp == NULL) {
		DRV_LOG(ERR, "Failed to allocate qp memory.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	qp->priv = priv;
	qp->entries_n = entries;
	if (mlx5_mr_ctrl_init(&qp->mr_ctrl, &priv->cdev->mr_scache.dev_gen,
				  priv->dev_config.socket_id)) {
		DRV_LOG(ERR, "Cannot allocate MR Btree for qp %u.",
			(uint32_t)qp_id);
		rte_errno = ENOMEM;
		goto err;
	}
	/*
	 * The following KLM pointer must be aligned with
	 * MLX5_UMR_KLM_PTR_ALIGN. Aligned opaq_size here
	 * to make the KLM pointer with offset be aligned.
	 */
	opaq_size = RTE_ALIGN(sizeof(union mlx5_gga_crypto_opaque) * entries,
			      MLX5_UMR_KLM_PTR_ALIGN);
	mr_size = (priv->max_klm_num * sizeof(struct mlx5_klm) * entries) + opaq_size;
	mr_buf = rte_calloc(__func__, (size_t)1, mr_size, MLX5_UMR_KLM_PTR_ALIGN);
	if (mr_buf == NULL) {
		DRV_LOG(ERR, "Failed to allocate mr memory.");
		rte_errno = ENOMEM;
		goto err;
	}
	if (priv->reg_mr_cb(priv->cdev->pd, mr_buf, mr_size, &qp->mr) != 0) {
		rte_free(mr_buf);
		DRV_LOG(ERR, "Failed to register opaque MR.");
		rte_errno = ENOMEM;
		goto err;
	}
	qp->opaque_addr = qp->mr.addr;
	qp->klm_array = RTE_PTR_ADD(qp->opaque_addr, opaq_size);
	/*
	 * Triple the CQ size as UMR QP which contains UMR and SEND_EN WQE
	 * will share this CQ .
	 */
	qp->cq_entries_n = rte_align32pow2(entries * 3);
	ret = mlx5_devx_cq_create(priv->cdev->ctx, &qp->cq_obj,
				  rte_log2_u32(qp->cq_entries_n),
				  &cq_attr, socket_id);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create CQ.");
		goto err;
	}
	qp_attr.cqn = qp->cq_obj.cq->id;
	qp_attr.ts_format = mlx5_ts_format_conv(attr->qp_ts_format);
	qp_attr.num_of_receive_wqes = 0;
	qp_attr.num_of_send_wqbbs = entries;
	qp_attr.mmo = attr->crypto_mmo.crypto_mmo_qp;
	/* Set MMO QP as follower as the input data may depend on UMR. */
	qp_attr.cd_slave_send = 1;
	ret = mlx5_devx_qp_create(priv->cdev->ctx, &qp->qp_obj,
				  qp_attr.num_of_send_wqbbs * MLX5_WQE_SIZE,
				  &qp_attr, socket_id);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create QP.");
		goto err;
	}
	mlx5_crypto_gcm_init_qp(qp);
	ret = mlx5_devx_qp2rts(&qp->qp_obj, 0);
	if (ret)
		goto err;
	qp->ops = (struct rte_crypto_op **)(qp + 1);
	qp->mkey = (struct mlx5_devx_obj **)(qp->ops + entries);
	if (mlx5_crypto_gcm_umr_qp_setup(dev, qp, socket_id)) {
		DRV_LOG(ERR, "Failed to setup UMR QP.");
		goto err;
	}
	DRV_LOG(INFO, "QP %u: SQN=0x%X CQN=0x%X entries num = %u",
		(uint32_t)qp_id, qp->qp_obj.qp->id, qp->cq_obj.cq->id, entries);
	if (mlx5_crypto_indirect_mkeys_prepare(priv, qp, &mkey_attr,
					       mlx5_crypto_gcm_mkey_klm_update)) {
		DRV_LOG(ERR, "Cannot allocate indirect memory regions.");
		rte_errno = ENOMEM;
		goto err;
	}
	dev->data->queue_pairs[qp_id] = qp;
	return 0;
err:
	mlx5_crypto_gcm_qp_release(dev, qp_id);
	return -1;
}

int
mlx5_crypto_gcm_init(struct mlx5_crypto_priv *priv)
{
	struct rte_cryptodev *crypto_dev = priv->crypto_dev;
	struct rte_cryptodev_ops *dev_ops = crypto_dev->dev_ops;

	/* Override AES-GCM specified ops. */
	dev_ops->sym_session_configure = mlx5_crypto_sym_gcm_session_configure;
	mlx5_os_set_reg_mr_cb(&priv->reg_mr_cb, &priv->dereg_mr_cb);
	dev_ops->queue_pair_setup = mlx5_crypto_gcm_qp_setup;
	dev_ops->queue_pair_release = mlx5_crypto_gcm_qp_release;
	priv->max_klm_num = RTE_ALIGN((priv->max_segs_num + 1) * 2 + 1, MLX5_UMR_KLM_NUM_ALIGN);
	priv->caps = mlx5_crypto_gcm_caps;
	return 0;
}
