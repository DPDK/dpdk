/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <rte_windows.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_autoconf.h"

/**
 * Get mlx5 device attributes.
 *
 * @param ctx
 *   Pointer to device context.
 *
 * @param device_attr
 *   Pointer to mlx5 device attributes.
 *
 * @return
 *   0 on success, non zero error number otherwise
 */
int
mlx5_os_get_dev_attr(void *ctx, struct mlx5_dev_attr *device_attr)
{
	struct mlx5_context *mlx5_ctx;
	struct mlx5_hca_attr hca_attr;
	void *pv_iseg = NULL;
	u32 cb_iseg = 0;
	int err = 0;

	if (!ctx)
		return -EINVAL;
	mlx5_ctx = (struct mlx5_context *)ctx;
	memset(device_attr, 0, sizeof(*device_attr));
	err = mlx5_devx_cmd_query_hca_attr(mlx5_ctx, &hca_attr);
	if (err) {
		DRV_LOG(ERR, "Failed to get device hca_cap");
		return err;
	}
	device_attr->max_cq = 1 << hca_attr.log_max_cq;
	device_attr->max_qp = 1 << hca_attr.log_max_qp;
	device_attr->max_qp_wr = 1 << hca_attr.log_max_qp_sz;
	device_attr->max_cqe = 1 << hca_attr.log_max_cq_sz;
	device_attr->max_mr = 1 << hca_attr.log_max_mrw_sz;
	device_attr->max_pd = 1 << hca_attr.log_max_pd;
	device_attr->max_srq = 1 << hca_attr.log_max_srq;
	device_attr->max_srq_wr = 1 << hca_attr.log_max_srq_sz;
	if (hca_attr.rss_ind_tbl_cap) {
		device_attr->max_rwq_indirection_table_size =
			1 << hca_attr.rss_ind_tbl_cap;
	}
	pv_iseg = mlx5_glue->query_hca_iseg(mlx5_ctx, &cb_iseg);
	if (pv_iseg == NULL) {
		DRV_LOG(ERR, "Failed to get device hca_iseg");
		return errno;
	}
	if (!err) {
		snprintf(device_attr->fw_ver, 64, "%x.%x.%04x",
			MLX5_GET(initial_seg, pv_iseg, fw_rev_major),
			MLX5_GET(initial_seg, pv_iseg, fw_rev_minor),
			MLX5_GET(initial_seg, pv_iseg, fw_rev_subminor));
	}
	return err;
}
