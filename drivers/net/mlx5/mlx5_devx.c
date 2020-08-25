/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_malloc.h>
#include <rte_common.h>
#include <rte_eal_paging.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_common_os.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"
#include "mlx5_devx.h"

/**
 * Modify RQ vlan stripping offload
 *
 * @param rxq_obj
 *   Rx queue object.
 *
 * @return 0 on success, non-0 otherwise
 */
static int
mlx5_rxq_obj_modify_rq_vlan_strip(struct mlx5_rxq_obj *rxq_obj, int on)
{
	struct mlx5_devx_modify_rq_attr rq_attr;

	memset(&rq_attr, 0, sizeof(rq_attr));
	rq_attr.rq_state = MLX5_RQC_STATE_RDY;
	rq_attr.state = MLX5_RQC_STATE_RDY;
	rq_attr.vsd = (on ? 0 : 1);
	rq_attr.modify_bitmask = MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_VSD;
	return mlx5_devx_cmd_modify_rq(rxq_obj->rq, &rq_attr);
}

struct mlx5_obj_ops devx_obj_ops = {
	.rxq_obj_modify_vlan_strip = mlx5_rxq_obj_modify_rq_vlan_strip,
};
