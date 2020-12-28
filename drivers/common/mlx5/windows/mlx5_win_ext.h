/*
 * Copyright (C) Mellanox Technologies, Ltd. 2001-2020.
 *
 */
#ifndef __MLX5_WIN_ETX_H__
#define __MLX5_WIN_ETX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mlx5devx.h"

typedef struct mlx5_context {
	devx_device_ctx        *devx_ctx;
	struct devx_device mlx5_dev;

} mlx5_context_st;

typedef struct {
	devx_device_ctx *devx_ctx;
	struct devx_obj_handle *obj;
} mlx5_devx_obj_st;

struct mlx5_devx_umem {
	void                   *addr;
	struct devx_obj_handle *umem_hdl;
	uint32_t                umem_id;
};

#define GET_DEVX_CTX(ctx) (((mlx5_context_st *)ctx)->devx_ctx)
#define GET_OBJ_CTX(obj)  (((mlx5_devx_obj_st *)obj)->devx_ctx)

#endif /* __MLX5_WIN_ETX_H__ */
