/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 6WIND S.A.
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <assert.h>
#include <stdio.h>
#include <time.h>

#include <rte_eal.h>
#include <rte_ethdev_driver.h>
#include <rte_string_fns.h>

#include "mlx5.h"
#include "mlx5_utils.h"

/**
 * Initialize IPC message.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 * @param[out] msg
 *   Pointer to message to fill in.
 * @param[in] type
 *   Message type.
 */
static inline void
mp_init_msg(struct rte_eth_dev *dev, struct rte_mp_msg *msg,
	    enum mlx5_mp_req_type type)
{
	struct mlx5_mp_param *param = (struct mlx5_mp_param *)msg->param;

	memset(msg, 0, sizeof(*msg));
	strlcpy(msg->name, MLX5_MP_NAME, sizeof(msg->name));
	msg->len_param = sizeof(*param);
	param->type = type;
	param->port_id = dev->data->port_id;
}

/**
 * IPC message handler of primary process.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 * @param[in] peer
 *   Pointer to the peer socket path.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mp_primary_handle(const struct rte_mp_msg *mp_msg, const void *peer)
{
	struct rte_mp_msg mp_res;
	struct mlx5_mp_param *res = (struct mlx5_mp_param *)mp_res.param;
	const struct mlx5_mp_param *param =
		(const struct mlx5_mp_param *)mp_msg->param;
	struct rte_eth_dev *dev;
	struct mlx5_priv *priv;
	int ret;

	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	if (!rte_eth_dev_is_valid_port(param->port_id)) {
		rte_errno = ENODEV;
		DRV_LOG(ERR, "port %u invalid port ID", param->port_id);
		return -rte_errno;
	}
	dev = &rte_eth_devices[param->port_id];
	priv = dev->data->dev_private;
	switch (param->type) {
	case MLX5_MP_REQ_VERBS_CMD_FD:
		mp_init_msg(dev, &mp_res, param->type);
		mp_res.num_fds = 1;
		mp_res.fds[0] = priv->sh->ctx->cmd_fd;
		res->result = 0;
		ret = rte_mp_reply(&mp_res, peer);
		break;
	default:
		rte_errno = EINVAL;
		DRV_LOG(ERR, "port %u invalid mp request type",
			dev->data->port_id);
		return -rte_errno;
	}
	return ret;
}

/**
 * Request Verbs command file descriptor for mmap to the primary process.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 *
 * @return
 *   fd on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_mp_req_verbs_cmd_fd(struct rte_eth_dev *dev)
{
	struct rte_mp_msg mp_req;
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mlx5_mp_param *res;
	struct timespec ts = {.tv_sec = MLX5_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	int ret;

	assert(rte_eal_process_type() == RTE_PROC_SECONDARY);
	mp_init_msg(dev, &mp_req, MLX5_MP_REQ_VERBS_CMD_FD);
	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		DRV_LOG(ERR, "port %u request to primary process failed",
			dev->data->port_id);
		return -rte_errno;
	}
	assert(mp_rep.nb_received == 1);
	mp_res = &mp_rep.msgs[0];
	res = (struct mlx5_mp_param *)mp_res->param;
	if (res->result) {
		rte_errno = -res->result;
		DRV_LOG(ERR,
			"port %u failed to get command FD from primary process",
			dev->data->port_id);
		ret = -rte_errno;
		goto exit;
	}
	assert(mp_res->num_fds == 1);
	ret = mp_res->fds[0];
	DRV_LOG(DEBUG, "port %u command FD from primary is %d",
		dev->data->port_id, ret);
exit:
	free(mp_rep.msgs);
	return ret;
}

/**
 * Initialize by primary process.
 */
void
mlx5_mp_init_primary(void)
{
	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	rte_mp_action_register(MLX5_MP_NAME, mp_primary_handle);
}

/**
 * Un-initialize by primary process.
 */
void
mlx5_mp_uninit_primary(void)
{
	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	rte_mp_action_unregister(MLX5_MP_NAME);
}
