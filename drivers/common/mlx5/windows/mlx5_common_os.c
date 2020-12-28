/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_errno.h>

#include "mlx5_devx_cmds.h"
#include "mlx5_common_utils.h"
#include "mlx5_common.h"

/**
 * Initialization routine for run-time dependency on external lib
 */
void
mlx5_glue_constructor(void)
{
}
