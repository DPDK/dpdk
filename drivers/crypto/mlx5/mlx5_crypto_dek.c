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

static int
mlx5_crypto_dek_get_key(struct rte_crypto_sym_xform *xform,
			const uint8_t **key,
			uint16_t *key_len)
{
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		*key = xform->cipher.key.data;
		*key_len = xform->cipher.key.length;
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		*key = xform->aead.key.data;
		*key_len = xform->aead.key.length;
	} else {
		*key = NULL;
		*key_len = 0;
		DRV_LOG(ERR, "Xform dek type not supported.");
		rte_errno = -EINVAL;
		return -1;
	}
	return 0;
}

int
mlx5_crypto_dek_destroy(struct mlx5_crypto_priv *priv,
			struct mlx5_crypto_dek *dek)
{
	return mlx5_hlist_unregister(priv->dek_hlist, &dek->entry);
}

struct mlx5_crypto_dek *
mlx5_crypto_dek_prepare(struct mlx5_crypto_priv *priv,
			struct rte_crypto_sym_xform *xform)
{
	const uint8_t *key;
	uint16_t key_len;
	struct mlx5_hlist *dek_hlist = priv->dek_hlist;
	struct mlx5_crypto_dek_ctx dek_ctx = {
		.xform = xform,
		.priv = priv,
	};
	uint64_t key64;
	struct mlx5_list_entry *entry;

	if (mlx5_crypto_dek_get_key(xform, &key, &key_len))
		return NULL;
	key64 = __rte_raw_cksum(key, key_len, 0);
	entry = mlx5_hlist_register(dek_hlist, key64, &dek_ctx);
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
	struct rte_crypto_sym_xform *xform = ctx->xform;
	struct mlx5_crypto_dek *dek =
			container_of(entry, typeof(*dek), entry);
	uint32_t key_len = dek->size;
	uint16_t xkey_len;
	const uint8_t *key;

	if (mlx5_crypto_dek_get_key(xform, &key, &xkey_len))
		return -1;
	if (key_len != xkey_len)
		return -1;
	return memcmp(key, dek->data, xkey_len);
}

static struct mlx5_list_entry *
mlx5_crypto_dek_create_cb(void *tool_ctx __rte_unused, void *cb_ctx)
{
	struct mlx5_crypto_dek_ctx *ctx = cb_ctx;
	struct rte_crypto_sym_xform *xform = ctx->xform;
	struct mlx5_crypto_dek *dek = rte_zmalloc(__func__, sizeof(*dek),
						  RTE_CACHE_LINE_SIZE);
	struct mlx5_devx_dek_attr dek_attr = {
		.pd = ctx->priv->cdev->pdn,
	};
	int ret = -1;

	if (dek == NULL) {
		DRV_LOG(ERR, "Failed to allocate dek memory.");
		return NULL;
	}
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
		ret = mlx5_crypto_dek_fill_xts_attr(dek, &dek_attr, cb_ctx);
	else if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD)
		ret = mlx5_crypto_dek_fill_gcm_attr(dek, &dek_attr, cb_ctx);
	if (ret)
		goto fail;
	dek->obj = mlx5_devx_cmd_create_dek_obj(ctx->priv->cdev->ctx,
						&dek_attr);
	if (dek->obj == NULL) {
		DRV_LOG(ERR, "Failed to create dek obj.");
		goto fail;
	}
	return &dek->entry;
fail:
	rte_free(dek);
	return NULL;
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
