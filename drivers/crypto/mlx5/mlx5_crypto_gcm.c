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
mlx5_crypto_gcm_init(struct mlx5_crypto_priv *priv)
{
	priv->caps = mlx5_crypto_gcm_caps;
	return 0;
}
