/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_RXTX_H_
#define RTE_PMD_MLX5_RXTX_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_spinlock.h>
#include <rte_io.h>
#include <rte_cycles.h>

#include <mlx5_common.h>
#include <mlx5_common_mr.h>

#include "mlx5_utils.h"
#include "mlx5.h"
#include "mlx5_autoconf.h"

struct mlx5_priv;

/* mlx5_rxtx.c */

extern uint32_t mlx5_ptype_table[];
extern uint8_t mlx5_cksum_table[];
extern uint8_t mlx5_swp_types_table[];

void mlx5_set_ptype_table(void);
void mlx5_set_cksum_table(void);
void mlx5_set_swp_types_table(void);
void mlx5_dump_debug_information(const char *path, const char *title,
				 const void *buf, unsigned int len);
int mlx5_queue_state_modify_primary(struct rte_eth_dev *dev,
			const struct mlx5_mp_arg_queue_state_modify *sm);
int mlx5_queue_state_modify(struct rte_eth_dev *dev,
			    struct mlx5_mp_arg_queue_state_modify *sm);

#endif /* RTE_PMD_MLX5_RXTX_H_ */
