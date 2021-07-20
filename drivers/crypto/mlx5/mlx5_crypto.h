/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5_CRYPTO_H_
#define MLX5_CRYPTO_H_

#include <stdbool.h>

#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>

#include <mlx5_common_utils.h>

#define MLX5_CRYPTO_DEK_HTABLE_SZ (1 << 11)
#define MLX5_CRYPTO_KEY_LENGTH 80

struct mlx5_crypto_priv {
	TAILQ_ENTRY(mlx5_crypto_priv) next;
	struct ibv_context *ctx; /* Device context. */
	struct rte_pci_device *pci_dev;
	struct rte_cryptodev *crypto_dev;
	void *uar; /* User Access Region. */
	uint32_t pdn; /* Protection Domain number. */
	struct ibv_pd *pd;
	struct mlx5_hlist *dek_hlist; /* Dek hash list. */
	struct rte_cryptodev_config dev_config;
};

struct mlx5_crypto_dek {
	struct mlx5_list_entry entry; /* Pointer to DEK hash list entry. */
	struct mlx5_devx_obj *obj; /* Pointer to DEK DevX object. */
	uint8_t data[MLX5_CRYPTO_KEY_LENGTH]; /* DEK key data. */
	bool size_is_48; /* Whether the key\data size is 48 bytes or not. */
} __rte_cache_aligned;

int
mlx5_crypto_dek_destroy(struct mlx5_crypto_priv *priv,
			struct mlx5_crypto_dek *dek);

struct mlx5_crypto_dek *
mlx5_crypto_dek_prepare(struct mlx5_crypto_priv *priv,
			struct rte_crypto_cipher_xform *cipher);

int
mlx5_crypto_dek_setup(struct mlx5_crypto_priv *priv);

void
mlx5_crypto_dek_unset(struct mlx5_crypto_priv *priv);

#endif /* MLX5_CRYPTO_H_ */
