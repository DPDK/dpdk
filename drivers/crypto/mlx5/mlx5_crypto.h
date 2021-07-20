/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5_CRYPTO_H_
#define MLX5_CRYPTO_H_

#include <stdbool.h>

#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>

#include <mlx5_common_utils.h>

struct mlx5_crypto_priv {
	TAILQ_ENTRY(mlx5_crypto_priv) next;
	struct ibv_context *ctx; /* Device context. */
	struct rte_pci_device *pci_dev;
	struct rte_cryptodev *crypto_dev;
	void *uar; /* User Access Region. */
	uint32_t pdn; /* Protection Domain number. */
	struct ibv_pd *pd;
};

#endif /* MLX5_CRYPTO_H_ */
