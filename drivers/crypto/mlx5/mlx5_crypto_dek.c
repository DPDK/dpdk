/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <rte_ip.h>
#include <rte_common.h>
#include <rte_errno.h>
#include <rte_log.h>

#include <mlx5_prm.h>
#include <mlx5_devx_cmds.h>

#include "mlx5_crypto_utils.h"
#include "mlx5_crypto.h"

struct mlx5_crypto_dek_ctx {
	struct rte_crypto_cipher_xform *cipher;
	struct mlx5_crypto_priv *priv;
};

int
mlx5_crypto_dek_destroy(struct mlx5_crypto_priv *priv,
			struct mlx5_crypto_dek *dek)
{
	return mlx5_hlist_unregister(priv->dek_hlist, &dek->entry);
}

struct mlx5_crypto_dek *
mlx5_crypto_dek_prepare(struct mlx5_crypto_priv *priv,
			struct rte_crypto_cipher_xform *cipher)
{
	struct mlx5_hlist *dek_hlist = priv->dek_hlist;
	struct mlx5_crypto_dek_ctx dek_ctx = {
		.cipher = cipher,
		.priv = priv,
	};
	struct rte_crypto_cipher_xform *cipher_ctx = cipher;
	uint64_t key64 = __rte_raw_cksum(cipher_ctx->key.data,
					 cipher_ctx->key.length, 0);
	struct mlx5_list_entry *entry = mlx5_hlist_register(dek_hlist,
							     key64, &dek_ctx);

	return entry == NULL ? NULL :
			     container_of(entry, struct mlx5_crypto_dek, entry);
}

static struct mlx5_list_entry *
mlx5_crypto_dek_clone_cb(void *tool_ctx __rte_unused,
			 struct mlx5_list_entry *oentry,
			 void *cb_ctx __rte_unused)
{
	struct mlx5_crypto_dek *entry = rte_zmalloc(__func__, sizeof(*entry),
						    RTE_CACHE_LINE_SIZE);

	if (!entry) {
		DRV_LOG(ERR, "Cannot allocate dek resource memory.");
		rte_errno = ENOMEM;
		return NULL;
	}
	memcpy(entry, oentry, sizeof(*entry));
	return &entry->entry;
}

static void
mlx5_crypto_dek_clone_free_cb(void *tool_ctx __rte_unused,
			      struct mlx5_list_entry *entry)
{
	struct mlx5_crypto_dek *dek = container_of(entry,
						struct mlx5_crypto_dek, entry);

	rte_free(dek);
}

static int
mlx5_crypto_dek_match_cb(void *tool_ctx __rte_unused,
			 struct mlx5_list_entry *entry, void *cb_ctx)
{
	struct mlx5_crypto_dek_ctx *ctx = cb_ctx;
	struct rte_crypto_cipher_xform *cipher_ctx = ctx->cipher;
	struct mlx5_crypto_dek *dek =
			container_of(entry, typeof(*dek), entry);
	uint32_t key_len = dek->size_is_48 ? 48 : 80;

	if (key_len != cipher_ctx->key.length)
		return -1;
	return memcmp(cipher_ctx->key.data, dek->data, key_len);
}

static struct mlx5_list_entry *
mlx5_crypto_dek_create_cb(void *tool_ctx __rte_unused, void *cb_ctx)
{
	struct mlx5_crypto_dek_ctx *ctx = cb_ctx;
	struct rte_crypto_cipher_xform *cipher_ctx = ctx->cipher;
	struct mlx5_crypto_dek *dek = rte_zmalloc(__func__, sizeof(*dek),
						  RTE_CACHE_LINE_SIZE);
	struct mlx5_devx_dek_attr dek_attr = {
		.pd = ctx->priv->cdev->pdn,
		.key_purpose = MLX5_CRYPTO_KEY_PURPOSE_AES_XTS,
		.has_keytag = 1,
	};

	if (dek == NULL) {
		DRV_LOG(ERR, "Failed to allocate dek memory.");
		return NULL;
	}
	switch (cipher_ctx->key.length) {
	case 48:
		dek->size_is_48 = true;
		dek_attr.key_size = MLX5_CRYPTO_KEY_SIZE_128b;
		break;
	case 80:
		dek->size_is_48 = false;
		dek_attr.key_size = MLX5_CRYPTO_KEY_SIZE_256b;
		break;
	default:
		DRV_LOG(ERR, "Key size not supported.");
		return NULL;
	}
	memcpy(&dek_attr.key, cipher_ctx->key.data, cipher_ctx->key.length);
	dek->obj = mlx5_devx_cmd_create_dek_obj(ctx->priv->cdev->ctx,
						&dek_attr);
	if (dek->obj == NULL) {
		rte_free(dek);
		return NULL;
	}
	memcpy(&dek->data, cipher_ctx->key.data, cipher_ctx->key.length);
	return &dek->entry;
}

static void
mlx5_crypto_dek_remove_cb(void *tool_ctx __rte_unused,
			  struct mlx5_list_entry *entry)
{
	struct mlx5_crypto_dek *dek =
		container_of(entry, typeof(*dek), entry);

	claim_zero(mlx5_devx_cmd_destroy(dek->obj));
	rte_free(dek);
}

int
mlx5_crypto_dek_setup(struct mlx5_crypto_priv *priv)
{
	priv->dek_hlist = mlx5_hlist_create("dek_hlist",
				 MLX5_CRYPTO_DEK_HTABLE_SZ,
				 0, 1, NULL, mlx5_crypto_dek_create_cb,
				 mlx5_crypto_dek_match_cb,
				 mlx5_crypto_dek_remove_cb,
				 mlx5_crypto_dek_clone_cb,
				 mlx5_crypto_dek_clone_free_cb);
	if (priv->dek_hlist == NULL)
		return -1;
	return 0;
}

void
mlx5_crypto_dek_unset(struct mlx5_crypto_priv *priv)
{
	if (priv->dek_hlist) {
		mlx5_hlist_destroy(priv->dek_hlist);
		priv->dek_hlist = NULL;
	}
}
