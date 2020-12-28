/*
 * Copyright (C) Mellanox Technologies, Ltd. 2001-2020.
 *
 */
#ifndef __MLX5_WIN_DEFS_H__
#define __MLX5_WIN_DEFS_H__

#ifdef __cplusplus
extern "C" {
#endif

enum {
	MLX5_CQE_OWNER_MASK	= 1,
	MLX5_CQE_REQ		= 0,
	MLX5_CQE_RESP_WR_IMM	= 1,
	MLX5_CQE_RESP_SEND	= 2,
	MLX5_CQE_RESP_SEND_IMM	= 3,
	MLX5_CQE_RESP_SEND_INV	= 4,
	MLX5_CQE_RESIZE_CQ	= 5,
	MLX5_CQE_NO_PACKET	= 6,
	MLX5_CQE_REQ_ERR	= 13,
	MLX5_CQE_RESP_ERR	= 14,
	MLX5_CQE_INVALID	= 15,
};
#endif /* __MLX5_WIN_DEFS_H__ */
