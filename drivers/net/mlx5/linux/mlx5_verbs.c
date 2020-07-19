/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include "mlx5_autoconf.h"

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_common.h>

#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_common_mr.h>
#include <mlx5_verbs.h>
/**
 * Register mr. Given protection domain pointer, pointer to addr and length
 * register the memory region.
 *
 * @param[in] pd
 *   Pointer to protection domain context.
 * @param[in] addr
 *   Pointer to memory start address.
 * @param[in] length
 *   Length of the memory to register.
 * @param[out] pmd_mr
 *   pmd_mr struct set with lkey, address, length and pointer to mr object
 *
 * @return
 *   0 on successful registration, -1 otherwise
 */
static int
mlx5_reg_mr(void *pd, void *addr, size_t length,
		 struct mlx5_pmd_mr *pmd_mr)
{
	return mlx5_common_verbs_reg_mr(pd, addr, length, pmd_mr);
}

/**
 * Deregister mr. Given the mlx5 pmd MR - deregister the MR
 *
 * @param[in] pmd_mr
 *   pmd_mr struct set with lkey, address, length and pointer to mr object
 *
 */
static void
mlx5_dereg_mr(struct mlx5_pmd_mr *pmd_mr)
{
	mlx5_common_verbs_dereg_mr(pmd_mr);
}

/* verbs operations. */
const struct mlx5_verbs_ops mlx5_verbs_ops = {
	.reg_mr = mlx5_reg_mr,
	.dereg_mr = mlx5_dereg_mr,
};
