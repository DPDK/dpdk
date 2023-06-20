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

int
mlx5_crypto_gcm_init(struct mlx5_crypto_priv *priv)
{
	struct rte_cryptodev *crypto_dev = priv->crypto_dev;
	struct rte_cryptodev_ops *dev_ops = crypto_dev->dev_ops;

	/* Override AES-GCM specified ops. */
	dev_ops->sym_session_configure = mlx5_crypto_sym_gcm_session_configure;
	priv->caps = mlx5_crypto_gcm_caps;
	return 0;
}
