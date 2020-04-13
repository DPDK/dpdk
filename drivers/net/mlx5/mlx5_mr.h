/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_MR_H_
#define RTE_PMD_MLX5_MR_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_ethdev.h>
#include <rte_rwlock.h>
#include <rte_bitmap.h>
#include <rte_memory.h>

#include <mlx5_common_mr.h>

/* First entry must be NULL for comparison. */
#define mlx5_mr_btree_len(bt) ((bt)->len - 1)

void mlx5_mr_mem_event_cb(enum rte_mem_event event_type, const void *addr,
			  size_t len, void *arg);
int mlx5_mr_update_mp(struct rte_eth_dev *dev, struct mlx5_mr_ctrl *mr_ctrl,
		      struct rte_mempool *mp);

#endif /* RTE_PMD_MLX5_MR_H_ */
