/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_MP_H_
#define RTE_PMD_MLX5_COMMON_MP_H_

#include <mlx5_glue.h>
#include <rte_eal.h>
#include <rte_string_fns.h>

/* Request types for IPC. */
enum mlx5_mp_req_type {
	MLX5_MP_REQ_VERBS_CMD_FD = 1,
	MLX5_MP_REQ_CREATE_MR,
	MLX5_MP_REQ_START_RXTX,
	MLX5_MP_REQ_STOP_RXTX,
	MLX5_MP_REQ_QUEUE_STATE_MODIFY,
	MLX5_MP_REQ_QUEUE_RX_STOP,
	MLX5_MP_REQ_QUEUE_RX_START,
	MLX5_MP_REQ_QUEUE_TX_STOP,
	MLX5_MP_REQ_QUEUE_TX_START,
};

struct mlx5_mp_arg_queue_state_modify {
	uint8_t is_wq; /* Set if WQ. */
	uint16_t queue_id; /* DPDK queue ID. */
	enum ibv_wq_state state; /* WQ requested state. */
};

struct mlx5_mp_arg_queue_id {
	uint16_t queue_id; /* DPDK queue ID. */
};

/* Pameters for IPC. */
struct mlx5_mp_param {
	enum mlx5_mp_req_type type;
	int port_id;
	int result;
	RTE_STD_C11
	union {
		uintptr_t addr; /* MLX5_MP_REQ_CREATE_MR */
		struct mlx5_mp_arg_queue_state_modify state_modify;
		/* MLX5_MP_REQ_QUEUE_STATE_MODIFY */
		struct mlx5_mp_arg_queue_id queue_id;
		/* MLX5_MP_REQ_QUEUE_RX/TX_START/STOP */
	} args;
};

/*  Identifier of a MP process */
struct mlx5_mp_id {
	char name[RTE_MP_MAX_NAME_LEN];
	uint16_t port_id;
};

/** Request timeout for IPC. */
#define MLX5_MP_REQ_TIMEOUT_SEC 5

/**
 * Initialize IPC message.
 *
 * @param[in] port_id
 *   Port ID of the device.
 * @param[out] msg
 *   Pointer to message to fill in.
 * @param[in] type
 *   Message type.
 */
static inline void
mp_init_msg(struct mlx5_mp_id *mp_id, struct rte_mp_msg *msg,
	    enum mlx5_mp_req_type type)
{
	struct mlx5_mp_param *param = (struct mlx5_mp_param *)msg->param;

	memset(msg, 0, sizeof(*msg));
	strlcpy(msg->name, mp_id->name, sizeof(msg->name));
	msg->len_param = sizeof(*param);
	param->type = type;
	param->port_id = mp_id->port_id;
}

__rte_internal
int mlx5_mp_init_primary(const char *name, const rte_mp_t primary_action);
__rte_internal
void mlx5_mp_uninit_primary(const char *name);
__rte_internal
int mlx5_mp_init_secondary(const char *name, const rte_mp_t secondary_action);
__rte_internal
void mlx5_mp_uninit_secondary(const char *name);
__rte_internal
int mlx5_mp_req_mr_create(struct mlx5_mp_id *mp_id, uintptr_t addr);
__rte_internal
int mlx5_mp_req_queue_state_modify(struct mlx5_mp_id *mp_id,
				   struct mlx5_mp_arg_queue_state_modify *sm);
__rte_internal
int mlx5_mp_req_verbs_cmd_fd(struct mlx5_mp_id *mp_id);

#endif /* RTE_PMD_MLX5_COMMON_MP_H_ */
