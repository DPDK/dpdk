/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <net/if.h>
#include <sys/mman.h>
#include <linux/rtnetlink.h>
#include <fcntl.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_kvargs.h>
#include <rte_rwlock.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <rte_alarm.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_mr.h"
#include "mlx5_flow.h"
#include "rte_pmd_mlx5.h"

/**
 * Get ibv device name. Given an ibv_context pointer - return a
 * pointer to the corresponding device name.
 *
 * @param[in] ctx
 *   Pointer to ibv context.
 *
 * @return
 *   Pointer to device name if ctx is valid, NULL otherwise.
 */
const char *
mlx5_os_get_ctx_device_name(void *ctx)
{
	if (!ctx)
		return NULL;
	return ((struct ibv_context *)ctx)->device->name;
}

/**
 * Get ibv device path name. Given an ibv_context pointer - return a
 * pointer to the corresponding device path name.
 *
 * @param[in] ctx
 *   Pointer to ibv context.
 *
 * @return
 *   Pointer to device path name if ctx is valid, NULL otherwise.
 */
const char *
mlx5_os_get_ctx_device_path(void *ctx)
{
	if (!ctx)
		return NULL;

	return ((struct ibv_context *)ctx)->device->ibdev_path;
}

/**
 * Get umem id. Given a pointer to umem object of type
 * 'struct mlx5dv_devx_umem *' - return its id.
 *
 * @param[in] umem
 *   Pointer to umem object.
 *
 * @return
 *   The umem id if umem is valid, 0 otherwise.
 */
uint32_t
mlx5_os_get_umem_id(void *umem)
{
	if (!umem)
		return 0;
	return ((struct mlx5dv_devx_umem *)umem)->umem_id;
}

/**
 * Get mlx5 device attributes. The glue function query_device_ex() is called
 * with out parameter of type 'struct ibv_device_attr_ex *'. Then fill in mlx5
 * device attributes from the glue out parameter.
 *
 * @param dev
 *   Pointer to ibv context.
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
	int err;
	struct ibv_device_attr_ex attr_ex;
	memset(device_attr, 0, sizeof(*device_attr));
	err = mlx5_glue->query_device_ex(ctx, NULL, &attr_ex);
	if (err)
		return err;

	device_attr->device_cap_flags_ex = attr_ex.device_cap_flags_ex;
	device_attr->max_qp_wr = attr_ex.orig_attr.max_qp_wr;
	device_attr->max_sge = attr_ex.orig_attr.max_sge;
	device_attr->max_cq = attr_ex.orig_attr.max_cq;
	device_attr->max_qp = attr_ex.orig_attr.max_qp;
	device_attr->raw_packet_caps = attr_ex.raw_packet_caps;
	device_attr->max_rwq_indirection_table_size =
		attr_ex.rss_caps.max_rwq_indirection_table_size;
	device_attr->max_tso = attr_ex.tso_caps.max_tso;
	device_attr->tso_supported_qpts = attr_ex.tso_caps.supported_qpts;

	struct mlx5dv_context dv_attr = { .comp_mask = 0 };
	err = mlx5_glue->dv_query_device(ctx, &dv_attr);
	if (err)
		return err;

	device_attr->flags = dv_attr.flags;
	device_attr->comp_mask = dv_attr.comp_mask;
#ifdef HAVE_IBV_MLX5_MOD_SWP
	device_attr->sw_parsing_offloads =
		dv_attr.sw_parsing_caps.sw_parsing_offloads;
#endif
	device_attr->min_single_stride_log_num_of_bytes =
		dv_attr.striding_rq_caps.min_single_stride_log_num_of_bytes;
	device_attr->max_single_stride_log_num_of_bytes =
		dv_attr.striding_rq_caps.max_single_stride_log_num_of_bytes;
	device_attr->min_single_wqe_log_num_of_strides =
		dv_attr.striding_rq_caps.min_single_wqe_log_num_of_strides;
	device_attr->max_single_wqe_log_num_of_strides =
		dv_attr.striding_rq_caps.max_single_wqe_log_num_of_strides;
	device_attr->stride_supported_qpts =
		dv_attr.striding_rq_caps.supported_qpts;
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	device_attr->tunnel_offloads_caps = dv_attr.tunnel_offloads_caps;
#endif

	return err;
}
