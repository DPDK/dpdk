/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 6WIND S.A.
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <stdio.h>
#include <time.h>

#include <rte_eal.h>
#include <rte_ethdev_driver.h>
#include <rte_string_fns.h>

#include <mlx5_common_mp.h>
#include <mlx5_common_mr.h>
#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"

int
mlx5_mp_os_primary_handle(const struct rte_mp_msg *mp_msg, const void *peer)
{
	struct rte_mp_msg mp_res;
	struct mlx5_mp_param *res = (struct mlx5_mp_param *)mp_res.param;
	const struct mlx5_mp_param *param =
		(const struct mlx5_mp_param *)mp_msg->param;
	struct rte_eth_dev *dev;
	struct mlx5_priv *priv;
	struct mr_cache_entry entry;
	uint32_t lkey;
	int ret;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	if (!rte_eth_dev_is_valid_port(param->port_id)) {
		rte_errno = ENODEV;
		DRV_LOG(ERR, "port %u invalid port ID", param->port_id);
		return -rte_errno;
	}
	dev = &rte_eth_devices[param->port_id];
	priv = dev->data->dev_private;
	switch (param->type) {
	case MLX5_MP_REQ_CREATE_MR:
		mp_init_msg(&priv->mp_id, &mp_res, param->type);
		lkey = mlx5_mr_create_primary(priv->sh->pd,
					      &priv->sh->share_cache,
					      &entry, param->args.addr,
					      priv->config.mr_ext_memseg_en);
		if (lkey == UINT32_MAX)
			res->result = -rte_errno;
		ret = rte_mp_reply(&mp_res, peer);
		break;
	case MLX5_MP_REQ_VERBS_CMD_FD:
		mp_init_msg(&priv->mp_id, &mp_res, param->type);
		mp_res.num_fds = 1;
		mp_res.fds[0] = ((struct ibv_context *)priv->sh->ctx)->cmd_fd;
		res->result = 0;
		ret = rte_mp_reply(&mp_res, peer);
		break;
	case MLX5_MP_REQ_QUEUE_STATE_MODIFY:
		mp_init_msg(&priv->mp_id, &mp_res, param->type);
		res->result = mlx5_queue_state_modify_primary
					(dev, &param->args.state_modify);
		ret = rte_mp_reply(&mp_res, peer);
		break;
	case MLX5_MP_REQ_QUEUE_RX_STOP:
		mp_init_msg(&priv->mp_id, &mp_res, param->type);
		res->result = mlx5_rx_queue_stop_primary
					(dev, param->args.queue_id.queue_id);
		ret = rte_mp_reply(&mp_res, peer);
		break;
	case MLX5_MP_REQ_QUEUE_RX_START:
		mp_init_msg(&priv->mp_id, &mp_res, param->type);
		res->result = mlx5_rx_queue_start_primary
					(dev, param->args.queue_id.queue_id);
		ret = rte_mp_reply(&mp_res, peer);
		break;
	case MLX5_MP_REQ_QUEUE_TX_STOP:
		mp_init_msg(&priv->mp_id, &mp_res, param->type);
		res->result = mlx5_tx_queue_stop_primary
					(dev, param->args.queue_id.queue_id);
		ret = rte_mp_reply(&mp_res, peer);
		break;
	case MLX5_MP_REQ_QUEUE_TX_START:
		mp_init_msg(&priv->mp_id, &mp_res, param->type);
		res->result = mlx5_tx_queue_start_primary
					(dev, param->args.queue_id.queue_id);
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
 * IPC message handler of a secondary process.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 * @param[in] peer
 *   Pointer to the peer socket path.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_mp_os_secondary_handle(const struct rte_mp_msg *mp_msg, const void *peer)
{
struct rte_mp_msg mp_res;
	struct mlx5_mp_param *res = (struct mlx5_mp_param *)mp_res.param;
	const struct mlx5_mp_param *param =
		(const struct mlx5_mp_param *)mp_msg->param;
	struct rte_eth_dev *dev;
	struct mlx5_priv *priv;
	int ret;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	if (!rte_eth_dev_is_valid_port(param->port_id)) {
		rte_errno = ENODEV;
		DRV_LOG(ERR, "port %u invalid port ID", param->port_id);
		return -rte_errno;
	}
	dev = &rte_eth_devices[param->port_id];
	priv = dev->data->dev_private;
	switch (param->type) {
	case MLX5_MP_REQ_START_RXTX:
		DRV_LOG(INFO, "port %u starting datapath", dev->data->port_id);
		rte_mb();
		dev->rx_pkt_burst = mlx5_select_rx_function(dev);
		dev->tx_pkt_burst = mlx5_select_tx_function(dev);
		mp_init_msg(&priv->mp_id, &mp_res, param->type);
		res->result = 0;
		ret = rte_mp_reply(&mp_res, peer);
		break;
	case MLX5_MP_REQ_STOP_RXTX:
		DRV_LOG(INFO, "port %u stopping datapath", dev->data->port_id);
		dev->rx_pkt_burst = removed_rx_burst;
		dev->tx_pkt_burst = removed_tx_burst;
		rte_mb();
		mp_init_msg(&priv->mp_id, &mp_res, param->type);
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
 * Broadcast request of stopping/starting data-path to secondary processes.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 * @param[in] type
 *   Request type.
 */
static void
mp_req_on_rxtx(struct rte_eth_dev *dev, enum mlx5_mp_req_type type)
{
	struct rte_mp_msg mp_req;
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mlx5_mp_param *res;
	struct timespec ts = {.tv_sec = MLX5_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;
	int i;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	if (!mlx5_shared_data->secondary_cnt)
		return;
	if (type != MLX5_MP_REQ_START_RXTX && type != MLX5_MP_REQ_STOP_RXTX) {
		DRV_LOG(ERR, "port %u unknown request (req_type %d)",
			dev->data->port_id, type);
		return;
	}
	mp_init_msg(&priv->mp_id, &mp_req, type);
	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		if (rte_errno != ENOTSUP)
			DRV_LOG(ERR, "port %u failed to request stop/start Rx/Tx (%d)",
				dev->data->port_id, type);
		goto exit;
	}
	if (mp_rep.nb_sent != mp_rep.nb_received) {
		DRV_LOG(ERR,
			"port %u not all secondaries responded (req_type %d)",
			dev->data->port_id, type);
		goto exit;
	}
	for (i = 0; i < mp_rep.nb_received; i++) {
		mp_res = &mp_rep.msgs[i];
		res = (struct mlx5_mp_param *)mp_res->param;
		if (res->result) {
			DRV_LOG(ERR, "port %u request failed on secondary #%d",
				dev->data->port_id, i);
			goto exit;
		}
	}
exit:
	mlx5_free(mp_rep.msgs);
}

/**
 * Broadcast request of starting data-path to secondary processes. The request
 * is synchronous.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 */
void
mlx5_mp_os_req_start_rxtx(struct rte_eth_dev *dev)
{
	mp_req_on_rxtx(dev, MLX5_MP_REQ_START_RXTX);
}

/**
 * Broadcast request of stopping data-path to secondary processes. The request
 * is synchronous.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 */
void
mlx5_mp_os_req_stop_rxtx(struct rte_eth_dev *dev)
{
	mp_req_on_rxtx(dev, MLX5_MP_REQ_STOP_RXTX);
}

/**
 * Request Verbs Rx/Tx queue stop or start to the primary process.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 * @param queue_id
 *   Queue ID to control.
 * @param req_type
 *   request type
 *     MLX5_MP_REQ_QUEUE_RX_START - start Rx queue
 *     MLX5_MP_REQ_QUEUE_TX_START - stop Tx queue
 *     MLX5_MP_REQ_QUEUE_RX_STOP - stop Rx queue
 *     MLX5_MP_REQ_QUEUE_TX_STOP - stop Tx queue
 * @return
 *   0 on success, a negative errno value otherwise and
 *     rte_errno is set.
 */
int
mlx5_mp_os_req_queue_control(struct rte_eth_dev *dev, uint16_t queue_id,
			  enum mlx5_mp_req_type req_type)
{
	struct rte_mp_msg mp_req;
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mlx5_mp_param *req = (struct mlx5_mp_param *)mp_req.param;
	struct mlx5_mp_param *res;
	struct timespec ts = {.tv_sec = MLX5_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	struct mlx5_priv *priv;
	int ret;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	priv = dev->data->dev_private;
	mp_init_msg(&priv->mp_id, &mp_req, req_type);
	req->args.queue_id.queue_id = queue_id;
	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		DRV_LOG(ERR, "port %u request to primary process failed",
			dev->data->port_id);
		return -rte_errno;
	}
	MLX5_ASSERT(mp_rep.nb_received == 1);
	mp_res = &mp_rep.msgs[0];
	res = (struct mlx5_mp_param *)mp_res->param;
	ret = res->result;
	free(mp_rep.msgs);
	return ret;
}
