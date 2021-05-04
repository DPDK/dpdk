/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <errno.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <rte_malloc.h>

#include "mlx5_glue.h"
#include "../mlx5_common_log.h"
#include "mlx5_win_ext.h"

/*
 * The returned value of this API is an array of pointers to mlx5
 * devices under Windows. The interesting parameters of a device:
 * Device PCI parameters: domain, bus, device id, function.
 * Device name.
 */
static void *
mlx5_glue_devx_get_device_list(int *num_devices)
{
	struct devx_device_bdf *devx_bdf_devs = NULL;
	size_t n_devx_devx = 0;
	int32_t ret = devx_get_device_list(&n_devx_devx, &devx_bdf_devs);

	if (ret) {
		errno = ret;
		*num_devices = 0;
		return NULL;
	}
	*num_devices = (int)n_devx_devx;
	return devx_bdf_devs;
}

static void
mlx5_glue_devx_free_device_list(void *list)
{
	if (!list) {
		errno = EINVAL;
		return;
	}
	devx_free_device_list(list);
}

static int
mlx5_glue_devx_close_device(void *ctx)
{
	mlx5_context_st *mlx5_ctx;
	int rc;

	if (!ctx)
		return -EINVAL;
	mlx5_ctx = (mlx5_context_st *)ctx;
	rc = devx_close_device(mlx5_ctx->devx_ctx);
	free(mlx5_ctx);
	return rc;
}

static void *
mlx5_glue_devx_open_device(void *device)
{
	struct mlx5_context *mlx5_ctx;

	if (!device) {
		errno = EINVAL;
		return NULL;
	}
	mlx5_ctx = malloc((sizeof(struct mlx5_context)));
	if (!mlx5_ctx) {
		errno = ENOMEM;
		return NULL;
	}
	memset(mlx5_ctx, 0, sizeof(*mlx5_ctx));
	mlx5_ctx->devx_ctx = devx_open_device(device);
	if (DEVX_IS_ERR(mlx5_ctx->devx_ctx)) {
		errno = -DEVX_PTR_ERR(mlx5_ctx->devx_ctx);
		free(mlx5_ctx);
		return NULL;
	}
	return mlx5_ctx;
}

static int
mlx5_glue_devx_query_device(void *device_bdf, void *dev_inf)
{
	struct devx_device_bdf *dev_bdf;
	struct devx_device *mlx5_dev;

	if (!device_bdf)
		return -EINVAL;
	dev_bdf = (struct devx_device_bdf *)device_bdf;
	mlx5_dev = (struct devx_device *)dev_inf;
	int err = devx_query_device(dev_bdf, mlx5_dev);
	if (err)
		return -E_FAIL;
	return 0;
}

static void *
mlx5_glue_devx_query_hca_iseg_mapping(void *ctx, uint32_t *cb_iseg)
{
	struct mlx5_context *mlx5_ctx;
	void *pv_iseg;
	int err;

	if (!ctx) {
		errno = EINVAL;
		return NULL;
	}
	mlx5_ctx = (struct mlx5_context *)ctx;
	err = devx_query_hca_iseg_mapping(mlx5_ctx->devx_ctx,
						cb_iseg, &pv_iseg);
	if (err) {
		errno = err;
		return NULL;
	}
	return pv_iseg;
}

static void *
mlx5_glue_devx_obj_create(void *ctx,
			      void *in, size_t inlen,
			      void *out, size_t outlen)
{
	mlx5_devx_obj_st *devx_obj;

	if (!ctx) {
		errno = EINVAL;
		return NULL;
	}
	devx_obj = malloc((sizeof(*devx_obj)));
	if (!devx_obj) {
		errno = ENOMEM;
		return NULL;
	}
	memset(devx_obj, 0, sizeof(*devx_obj));
	devx_obj->devx_ctx = GET_DEVX_CTX(ctx);
	devx_obj->obj = devx_obj_create(devx_obj->devx_ctx,
					in, inlen, out, outlen);
	if (DEVX_IS_ERR(devx_obj->obj)) {
		errno = -DEVX_PTR_ERR(devx_obj->obj);
		free(devx_obj);
		return NULL;
	}
	return devx_obj;
}

static int
mlx5_glue_devx_obj_destroy(void *obj)
{
	mlx5_devx_obj_st *devx_obj;

	if (!obj)
		return -EINVAL;
	devx_obj = obj;
	int rc = devx_obj_destroy(devx_obj->obj);
	free(devx_obj);
	return rc;
}

static int
mlx5_glue_devx_general_cmd(void *ctx,
			   void *in, size_t inlen,
			   void *out, size_t outlen)
{
	if (!ctx)
		return -EINVAL;
	return devx_cmd(GET_DEVX_CTX(ctx), in, inlen, out, outlen);
}

static int
mlx5_glue_devx_obj_query(void *obj,
			    void *in, size_t inlen,
			    void *out, size_t outlen)
{
	return devx_cmd(GET_OBJ_CTX(obj), in, inlen, out, outlen);
}

static int
mlx5_glue_devx_obj_modify(void *obj,
			    void *in, size_t inlen,
			    void *out, size_t outlen)
{
	return devx_cmd(GET_OBJ_CTX(obj), in, inlen, out, outlen);
}

static int
mlx5_glue_devx_umem_dereg(void *pumem)
{
	struct devx_obj_handle *umem;

	if (!pumem)
		return -EINVAL;
	umem = pumem;
	return devx_umem_unreg(umem);
}

static void *
mlx5_glue_devx_umem_reg(void *ctx, void *addr, size_t size,
				  uint32_t access, uint32_t *id)
{
	struct devx_obj_handle *umem_hdl;
	int w_access = DEVX_UMEM_ACCESS_READ;

	if (!ctx) {
		errno = EINVAL;
		return NULL;
	}
	if (access)
		w_access |= DEVX_UMEM_ACCESS_WRITE;

	umem_hdl = devx_umem_reg(GET_DEVX_CTX(ctx), addr,
					size, w_access, id);
	if (DEVX_IS_ERR(umem_hdl)) {
		errno = -DEVX_PTR_ERR(umem_hdl);
		return NULL;
	}
	return umem_hdl;
}

static void *
mlx5_glue_devx_alloc_uar(void *ctx,
		uint32_t flags)
{
	devx_uar_handle *uar;

	if (!ctx) {
		errno = EINVAL;
		return NULL;
	}
	uar = devx_alloc_uar(GET_DEVX_CTX(ctx), flags);
	if (DEVX_IS_ERR(uar)) {
		errno = -DEVX_PTR_ERR(uar);
		return NULL;
	}
	return uar;
}

static int
mlx5_glue_devx_query_eqn(void *ctx,
		uint32_t cpus, uint32_t *eqn)
{
	if (!ctx)
		return -EINVAL;
	return devx_query_eqn(GET_DEVX_CTX(ctx), cpus, eqn);
}

static void
mlx5_glue_devx_free_uar(void *uar)
{
	devx_free_uar((devx_uar_handle *)uar);
}

static_assert(MLX5_ST_SZ_BYTES(fte_match_param) == 0x200,
	"PRM size of fte_match_param is broken! cannot compile Windows!");

static void*
mlx5_glue_devx_fs_rule_add(void *ctx, void *in, uint32_t inlen)

{
	struct devx_obj_handle *rule_hdl = NULL;

	if (!ctx) {
		errno = EINVAL;
		return NULL;
	}
	rule_hdl = devx_fs_rule_add(GET_DEVX_CTX(ctx), in, inlen);
	if (DEVX_IS_ERR(rule_hdl)) {
		errno = -DEVX_PTR_ERR(rule_hdl);
		return NULL;
	}
	return rule_hdl;
}

static int
mlx5_glue_devx_fs_rule_del(void *flow)
{
	return devx_fs_rule_del(flow);
}

static int
mlx5_glue_query_rt_values(void *ctx, void *devx_clock)
{
	struct mlx5_context *mlx5_ctx;
	struct mlx5_devx_clock *clock;
	int err;

	if (!ctx) {
		errno = EINVAL;
		return errno;
	}
	mlx5_ctx = (struct mlx5_context *)ctx;
	clock = (struct mlx5_devx_clock *)devx_clock;
	err = devx_hca_clock_query(
			mlx5_ctx->devx_ctx,
			&clock->p_iseg_internal_timer,
			&clock->clock_frequency_hz,
			&clock->is_stable_clock_frequency);
	if (err) {
		errno = err;
		return errno;
	}
	return 0;
}

static int
mlx5_glue_devx_init_showdown_event(void *ctx)
{
	struct mlx5_context *mlx5_ctx;
	int err;

	if (!ctx) {
		errno = EINVAL;
		return errno;
	}
	mlx5_ctx = (struct mlx5_context *)ctx;
	err = devx_query_shutdown_event(mlx5_ctx->devx_ctx,
			&mlx5_ctx->shutdown_event_obj);
	if (err) {
		errno = err;
		return errno;
	}
	return 0;
}

alignas(RTE_CACHE_LINE_SIZE)
const struct mlx5_glue *mlx5_glue = &(const struct mlx5_glue){
	.version = MLX5_GLUE_VERSION,
	.get_device_list = mlx5_glue_devx_get_device_list,
	.free_device_list = mlx5_glue_devx_free_device_list,
	.open_device = mlx5_glue_devx_open_device,
	.close_device = mlx5_glue_devx_close_device,
	.query_device = mlx5_glue_devx_query_device,
	.query_hca_iseg = mlx5_glue_devx_query_hca_iseg_mapping,
	.devx_obj_create = mlx5_glue_devx_obj_create,
	.devx_obj_destroy = mlx5_glue_devx_obj_destroy,
	.devx_obj_query = mlx5_glue_devx_obj_query,
	.devx_obj_modify = mlx5_glue_devx_obj_modify,
	.devx_general_cmd = mlx5_glue_devx_general_cmd,
	.devx_umem_reg = mlx5_glue_devx_umem_reg,
	.devx_umem_dereg = mlx5_glue_devx_umem_dereg,
	.devx_alloc_uar = mlx5_glue_devx_alloc_uar,
	.devx_free_uar = mlx5_glue_devx_free_uar,
	.devx_fs_rule_add = mlx5_glue_devx_fs_rule_add,
	.devx_fs_rule_del = mlx5_glue_devx_fs_rule_del,
	.devx_query_eqn = mlx5_glue_devx_query_eqn,
	.query_rt_values = mlx5_glue_query_rt_values,
	.devx_init_showdown_event = mlx5_glue_devx_init_showdown_event,
};
