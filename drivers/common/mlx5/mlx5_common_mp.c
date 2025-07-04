/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 6WIND S.A.
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <stdio.h>
#include <time.h>

#include <eal_export.h>
#include <rte_eal.h>
#include <rte_errno.h>

#include "mlx5_common_mp.h"
#include "mlx5_common_log.h"
#include "mlx5_malloc.h"

/**
 * Request Memory Region creation to the primary process.
 *
 * @param cdev
 *   Pointer to the mlx5 common device.
 * @param addr
 *   Target virtual address to register.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_mp_req_mr_create)
int
mlx5_mp_req_mr_create(struct mlx5_common_device *cdev, uintptr_t addr)
{
	struct rte_mp_msg mp_req;
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mlx5_mp_param *req = (struct mlx5_mp_param *)mp_req.param;
	struct mlx5_mp_arg_mr_manage *arg = &req->args.mr_manage;
	struct mlx5_mp_param *res;
	struct timespec ts = {.tv_sec = MLX5_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	int ret;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	mp_init_port_agnostic_msg(&mp_req, MLX5_MP_REQ_CREATE_MR);
	arg->addr = addr;
	arg->cdev = cdev;
	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		DRV_LOG(ERR, "Create MR request to primary process failed.");
		return -rte_errno;
	}
	MLX5_ASSERT(mp_rep.nb_received == 1);
	mp_res = &mp_rep.msgs[0];
	res = (struct mlx5_mp_param *)mp_res->param;
	ret = res->result;
	if (ret)
		rte_errno = -ret;
	mlx5_free(mp_rep.msgs);
	return ret;
}

/**
 * @param cdev
 *   Pointer to the mlx5 common device.
 * @param mempool
 *   Mempool to register or unregister.
 * @param reg
 *   True to register the mempool, False to unregister.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_mp_req_mempool_reg)
int
mlx5_mp_req_mempool_reg(struct mlx5_common_device *cdev,
			struct rte_mempool *mempool, bool reg,
			bool is_extmem)
{
	struct rte_mp_msg mp_req;
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mlx5_mp_param *req = (struct mlx5_mp_param *)mp_req.param;
	struct mlx5_mp_arg_mr_manage *arg = &req->args.mr_manage;
	struct mlx5_mp_param *res;
	struct timespec ts = {.tv_sec = MLX5_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	enum mlx5_mp_req_type type;
	int ret;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	type = reg ? MLX5_MP_REQ_MEMPOOL_REGISTER :
		     MLX5_MP_REQ_MEMPOOL_UNREGISTER;
	mp_init_port_agnostic_msg(&mp_req, type);
	arg->mempool = mempool;
	arg->is_extmem = is_extmem;
	arg->cdev = cdev;
	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		DRV_LOG(ERR,
			"Mempool %sregister request to primary process failed.",
			reg ? "" : "un");
		return -rte_errno;
	}
	MLX5_ASSERT(mp_rep.nb_received == 1);
	mp_res = &mp_rep.msgs[0];
	res = (struct mlx5_mp_param *)mp_res->param;
	ret = res->result;
	if (ret)
		rte_errno = -ret;
	mlx5_free(mp_rep.msgs);
	return ret;
}

/**
 * Request Verbs queue state modification to the primary process.
 *
 * @param[in] mp_id
 *   ID of the MP process.
 * @param sm
 *   State modify parameters.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_mp_req_queue_state_modify)
int
mlx5_mp_req_queue_state_modify(struct mlx5_mp_id *mp_id,
			       struct mlx5_mp_arg_queue_state_modify *sm)
{
	struct rte_mp_msg mp_req;
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mlx5_mp_param *req = (struct mlx5_mp_param *)mp_req.param;
	struct mlx5_mp_param *res;
	struct timespec ts = {.tv_sec = MLX5_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	int ret;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	mp_init_msg(mp_id, &mp_req, MLX5_MP_REQ_QUEUE_STATE_MODIFY);
	req->args.state_modify = *sm;
	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		DRV_LOG(ERR, "port %u request to primary process failed",
			mp_id->port_id);
		return -rte_errno;
	}
	MLX5_ASSERT(mp_rep.nb_received == 1);
	mp_res = &mp_rep.msgs[0];
	res = (struct mlx5_mp_param *)mp_res->param;
	ret = res->result;
	mlx5_free(mp_rep.msgs);
	return ret;
}

/**
 * Request Verbs command file descriptor for mmap to the primary process.
 *
 * @param[in] mp_id
 *   ID of the MP process.
 *
 * @return
 *   fd on success, a negative errno value otherwise and rte_errno is set.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_mp_req_verbs_cmd_fd)
int
mlx5_mp_req_verbs_cmd_fd(struct mlx5_mp_id *mp_id)
{
	struct rte_mp_msg mp_req;
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mlx5_mp_param *res;
	struct timespec ts = {.tv_sec = MLX5_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	int ret;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	mp_init_msg(mp_id, &mp_req, MLX5_MP_REQ_VERBS_CMD_FD);
	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		DRV_LOG(ERR, "port %u request to primary process failed",
			mp_id->port_id);
		return -rte_errno;
	}
	MLX5_ASSERT(mp_rep.nb_received == 1);
	mp_res = &mp_rep.msgs[0];
	res = (struct mlx5_mp_param *)mp_res->param;
	if (res->result) {
		rte_errno = -res->result;
		DRV_LOG(ERR,
			"port %u failed to get command FD from primary process",
			mp_id->port_id);
		ret = -rte_errno;
		goto exit;
	}
	MLX5_ASSERT(mp_res->num_fds == 1);
	ret = mp_res->fds[0];
	DRV_LOG(DEBUG, "port %u command FD from primary is %d",
		mp_id->port_id, ret);
exit:
	mlx5_free(mp_rep.msgs);
	return ret;
}

/**
 * Initialize by primary process.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_mp_init_primary)
int
mlx5_mp_init_primary(const char *name, const rte_mp_t primary_action)
{
	int ret;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);

	/* primary is allowed to not support IPC */
	ret = rte_mp_action_register(name, primary_action);
	if (ret && rte_errno != ENOTSUP)
		return -1;
	return 0;
}

/**
 * Un-initialize by primary process.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_mp_uninit_primary)
void
mlx5_mp_uninit_primary(const char *name)
{
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	rte_mp_action_unregister(name);
}

/**
 * Initialize by secondary process.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_mp_init_secondary)
int
mlx5_mp_init_secondary(const char *name, const rte_mp_t secondary_action)
{
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	return rte_mp_action_register(name, secondary_action);
}

/**
 * Un-initialize by secondary process.
 */
RTE_EXPORT_INTERNAL_SYMBOL(mlx5_mp_uninit_secondary)
void
mlx5_mp_uninit_secondary(const char *name)
{
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	rte_mp_action_unregister(name);
}
