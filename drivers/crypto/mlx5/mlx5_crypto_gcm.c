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

int
mlx5_crypto_gcm_init(struct mlx5_crypto_priv *priv)
{
	priv->caps = mlx5_crypto_gcm_caps;
	return 0;
}
