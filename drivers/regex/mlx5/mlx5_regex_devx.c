/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <rte_errno.h>
#include <rte_log.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>

#include "mlx5_regex.h"
#include "mlx5_regex_utils.h"

int
mlx5_devx_regex_rules_program(void *ctx, uint8_t engine, uint32_t rof_mkey,
				uint32_t rof_size, uint64_t rof_mkey_va)
{
	uint32_t out[MLX5_ST_SZ_DW(set_regexp_params_out)] = {0};
	uint32_t in[MLX5_ST_SZ_DW(set_regexp_params_in)] = {0};
	int ret;

	MLX5_SET(set_regexp_params_in, in, opcode, MLX5_CMD_SET_REGEX_PARAMS);
	MLX5_SET(set_regexp_params_in, in, engine_id, engine);
	MLX5_SET(set_regexp_params_in, in, regexp_params.rof_mkey, rof_mkey);
	MLX5_SET(set_regexp_params_in, in, regexp_params.rof_size, rof_size);
	MLX5_SET64(set_regexp_params_in, in, regexp_params.rof_mkey_va,
		   rof_mkey_va);
	MLX5_SET(set_regexp_params_in, in, field_select.rof_mkey, 1);
	ret = mlx5_glue->devx_general_cmd(ctx, in, sizeof(in), out,
					  sizeof(out));
	if (ret) {
		DRV_LOG(ERR, "Rules program failed %d", ret);
		rte_errno = errno;
		return -errno;
	}
	return 0;
}
