/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 6WIND S.A.
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <rte_eal.h>
#include <ethdev_driver.h>
#include <rte_string_fns.h>

#include "mlx4.h"
#include "mlx4_rxtx.h"
#include "mlx4_utils.h"

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
	    enum mlx4_mp_req_type type)
{
	struct mlx4_mp_param *param = (struct mlx4_mp_param *)msg->param;

	memset(msg, 0, sizeof(*msg));
	strlcpy(msg->name, MLX4_MP_NAME, sizeof(msg->name));
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
 *   0 on success, negative errno value otherwise and rte_errno is set.
 */
static int
mp_primary_handle(const struct rte_mp_msg *mp_msg, const void *peer)
{
	struct rte_mp_msg mp_res;
	struct mlx4_mp_param *res = (struct mlx4_mp_param *)mp_res.param;
	const struct mlx4_mp_param *param =
		(const struct mlx4_mp_param *)mp_msg->param;
	struct rte_eth_dev *dev;
	struct mlx4_priv *priv;
	struct mlx4_mr_cache entry;
	uint32_t lkey;
	int ret;

	MLX4_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	if (!rte_eth_dev_is_valid_port(param->port_id)) {
		rte_errno = ENODEV;
		ERROR("port %u invalid port ID", param->port_id);
		return -rte_errno;
	}
	dev = &rte_eth_devices[param->port_id];
	priv = dev->data->dev_private;
	switch (param->type) {
	case MLX4_MP_REQ_CREATE_MR:
		mp_init_msg(dev, &mp_res, param->type);
		lkey = mlx4_mr_create_primary(dev, &entry, param->args.addr);
		if (lkey == UINT32_MAX)
			res->result = -rte_errno;
		ret = rte_mp_reply(&mp_res, peer);
		break;
	case MLX4_MP_REQ_VERBS_CMD_FD:
		mp_init_msg(dev, &mp_res, param->type);
		mp_res.num_fds = 1;
		mp_res.fds[0] = priv->ctx->cmd_fd;
		res->result = 0;
		ret = rte_mp_reply(&mp_res, peer);
		break;
	default:
		rte_errno = EINVAL;
		ERROR("port %u invalid mp request type", dev->data->port_id);
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
static int
mp_secondary_handle(const struct rte_mp_msg *mp_msg, const void *peer)
{
	struct rte_mp_msg mp_res;
	struct mlx4_mp_param *res = (struct mlx4_mp_param *)mp_res.param;
	const struct mlx4_mp_param *param =
		(const struct mlx4_mp_param *)mp_msg->param;
	struct rte_eth_dev *dev;
#ifdef HAVE_IBV_MLX4_UAR_MMAP_OFFSET
	struct mlx4_proc_priv *ppriv;
#endif
	int ret;

	MLX4_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	if (!rte_eth_dev_is_valid_port(param->port_id)) {
		rte_errno = ENODEV;
		ERROR("port %u invalid port ID", param->port_id);
		return -rte_errno;
	}
	dev = &rte_eth_devices[param->port_id];
	switch (param->type) {
	case MLX4_MP_REQ_START_RXTX:
		INFO("port %u starting datapath", dev->data->port_id);
		dev->tx_pkt_burst = mlx4_tx_burst;
		dev->rx_pkt_burst = mlx4_rx_burst;
#ifdef HAVE_IBV_MLX4_UAR_MMAP_OFFSET
		ppriv = (struct mlx4_proc_priv *)dev->process_private;
		if (ppriv->uar_table_sz != dev->data->nb_tx_queues) {
			mlx4_tx_uar_uninit_secondary(dev);
			mlx4_proc_priv_uninit(dev);
			ret = mlx4_proc_priv_init(dev);
			if (ret) {
				close(mp_msg->fds[0]);
				return -rte_errno;
			}
			ret = mlx4_tx_uar_init_secondary(dev, mp_msg->fds[0]);
			if (ret) {
				close(mp_msg->fds[0]);
				mlx4_proc_priv_uninit(dev);
				return -rte_errno;
			}
		}
#endif
		close(mp_msg->fds[0]);
		rte_mb();
		mp_init_msg(dev, &mp_res, param->type);
		res->result = 0;
		ret = rte_mp_reply(&mp_res, peer);
		break;
	case MLX4_MP_REQ_STOP_RXTX:
		INFO("port %u stopping datapath", dev->data->port_id);
		dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
		dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
		rte_mb();
		mp_init_msg(dev, &mp_res, param->type);
		res->result = 0;
		ret = rte_mp_reply(&mp_res, peer);
		break;
	default:
		rte_errno = EINVAL;
		ERROR("port %u invalid mp request type", dev->data->port_id);
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
mp_req_on_rxtx(struct rte_eth_dev *dev, enum mlx4_mp_req_type type)
{
	struct rte_mp_msg mp_req;
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mlx4_mp_param *res __rte_unused;
	struct timespec ts = {.tv_sec = MLX4_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	struct mlx4_priv *priv;
	int ret;
	int i;

	MLX4_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	if (!mlx4_shared_data->secondary_cnt)
		return;
	if (type != MLX4_MP_REQ_START_RXTX && type != MLX4_MP_REQ_STOP_RXTX) {
		ERROR("port %u unknown request (req_type %d)",
		      dev->data->port_id, type);
		return;
	}
	mp_init_msg(dev, &mp_req, type);
	if (type == MLX4_MP_REQ_START_RXTX) {
		priv = dev->data->dev_private;
		mp_req.num_fds = 1;
		mp_req.fds[0] = priv->ctx->cmd_fd;
	}
	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		if (rte_errno != ENOTSUP)
			ERROR("port %u failed to request stop/start Rx/Tx (%d)",
					dev->data->port_id, type);
		goto exit;
	}
	if (mp_rep.nb_sent != mp_rep.nb_received) {
		ERROR("port %u not all secondaries responded (req_type %d)",
		      dev->data->port_id, type);
		goto exit;
	}
	for (i = 0; i < mp_rep.nb_received; i++) {
		mp_res = &mp_rep.msgs[i];
		res = (struct mlx4_mp_param *)mp_res->param;
		if (res->result) {
			ERROR("port %u request failed on secondary #%d",
			      dev->data->port_id, i);
			goto exit;
		}
	}
exit:
	free(mp_rep.msgs);
}

/**
 * Broadcast request of starting data-path to secondary processes. The request
 * is synchronous.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 */
void
mlx4_mp_req_start_rxtx(struct rte_eth_dev *dev)
{
	mp_req_on_rxtx(dev, MLX4_MP_REQ_START_RXTX);
}

/**
 * Broadcast request of stopping data-path to secondary processes. The request
 * is synchronous.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 */
void
mlx4_mp_req_stop_rxtx(struct rte_eth_dev *dev)
{
	mp_req_on_rxtx(dev, MLX4_MP_REQ_STOP_RXTX);
}

/**
 * Request Memory Region creation to the primary process.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 * @param addr
 *   Target virtual address to register.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx4_mp_req_mr_create(struct rte_eth_dev *dev, uintptr_t addr)
{
	struct rte_mp_msg mp_req;
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mlx4_mp_param *req = (struct mlx4_mp_param *)mp_req.param;
	struct mlx4_mp_param *res;
	struct timespec ts = {.tv_sec = MLX4_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	int ret;

	MLX4_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	mp_init_msg(dev, &mp_req, MLX4_MP_REQ_CREATE_MR);
	req->args.addr = addr;
	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		ERROR("port %u request to primary process failed",
		      dev->data->port_id);
		return -rte_errno;
	}
	MLX4_ASSERT(mp_rep.nb_received == 1);
	mp_res = &mp_rep.msgs[0];
	res = (struct mlx4_mp_param *)mp_res->param;
	ret = res->result;
	if (ret)
		rte_errno = -ret;
	free(mp_rep.msgs);
	return ret;
}

/**
 * IPC message handler of primary process.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 *
 * @return
 *   fd on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx4_mp_req_verbs_cmd_fd(struct rte_eth_dev *dev)
{
	struct rte_mp_msg mp_req;
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mlx4_mp_param *res;
	struct timespec ts = {.tv_sec = MLX4_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	int ret;

	MLX4_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	mp_init_msg(dev, &mp_req, MLX4_MP_REQ_VERBS_CMD_FD);
	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		ERROR("port %u request to primary process failed",
		      dev->data->port_id);
		return -rte_errno;
	}
	MLX4_ASSERT(mp_rep.nb_received == 1);
	mp_res = &mp_rep.msgs[0];
	res = (struct mlx4_mp_param *)mp_res->param;
	if (res->result) {
		rte_errno = -res->result;
		ERROR("port %u failed to get command FD from primary process",
		      dev->data->port_id);
		ret = -rte_errno;
		goto exit;
	}
	MLX4_ASSERT(mp_res->num_fds == 1);
	ret = mp_res->fds[0];
	DEBUG("port %u command FD from primary is %d",
	      dev->data->port_id, ret);
exit:
	free(mp_rep.msgs);
	return ret;
}

/**
 * Initialize by primary process.
 */
int
mlx4_mp_init_primary(void)
{
	int ret;

	MLX4_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);

	/* primary is allowed to not support IPC */
	ret = rte_mp_action_register(MLX4_MP_NAME, mp_primary_handle);
	if (ret && rte_errno != ENOTSUP)
		return -1;
	return 0;
}

/**
 * Un-initialize by primary process.
 */
void
mlx4_mp_uninit_primary(void)
{
	MLX4_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	rte_mp_action_unregister(MLX4_MP_NAME);
}

/**
 * Initialize by secondary process.
 */
int
mlx4_mp_init_secondary(void)
{
	MLX4_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	return rte_mp_action_register(MLX4_MP_NAME, mp_secondary_handle);
}

/**
 * Un-initialize by secondary process.
 */
void
mlx4_mp_uninit_secondary(void)
{
	MLX4_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	rte_mp_action_unregister(MLX4_MP_NAME);
}
