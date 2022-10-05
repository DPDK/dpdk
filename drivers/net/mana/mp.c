/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2022 Microsoft Corporation
 */

#include <rte_malloc.h>
#include <ethdev_driver.h>
#include <rte_log.h>

#include <infiniband/verbs.h>

#include "mana.h"

extern struct mana_shared_data *mana_shared_data;

static void
mp_init_msg(struct rte_mp_msg *msg, enum mana_mp_req_type type, int port_id)
{
	struct mana_mp_param *param;

	strlcpy(msg->name, MANA_MP_NAME, sizeof(msg->name));
	msg->len_param = sizeof(*param);

	param = (struct mana_mp_param *)msg->param;
	param->type = type;
	param->port_id = port_id;
}

static int
mana_mp_primary_handle(const struct rte_mp_msg *mp_msg, const void *peer)
{
	struct rte_eth_dev *dev;
	const struct mana_mp_param *param =
		(const struct mana_mp_param *)mp_msg->param;
	struct rte_mp_msg mp_res = { 0 };
	struct mana_mp_param *res = (struct mana_mp_param *)mp_res.param;
	int ret;
	struct mana_priv *priv;

	if (!rte_eth_dev_is_valid_port(param->port_id)) {
		DRV_LOG(ERR, "MP handle port ID %u invalid", param->port_id);
		return -ENODEV;
	}

	dev = &rte_eth_devices[param->port_id];
	priv = dev->data->dev_private;

	mp_init_msg(&mp_res, param->type, param->port_id);

	switch (param->type) {
	case MANA_MP_REQ_VERBS_CMD_FD:
		mp_res.num_fds = 1;
		mp_res.fds[0] = priv->ib_ctx->cmd_fd;
		res->result = 0;
		ret = rte_mp_reply(&mp_res, peer);
		break;

	default:
		DRV_LOG(ERR, "Port %u unknown primary MP type %u",
			param->port_id, param->type);
		ret = -EINVAL;
	}

	return ret;
}

static int
mana_mp_secondary_handle(const struct rte_mp_msg *mp_msg, const void *peer)
{
	struct rte_mp_msg mp_res = { 0 };
	struct mana_mp_param *res = (struct mana_mp_param *)mp_res.param;
	const struct mana_mp_param *param =
		(const struct mana_mp_param *)mp_msg->param;
	struct rte_eth_dev *dev;
	int ret;

	if (!rte_eth_dev_is_valid_port(param->port_id)) {
		DRV_LOG(ERR, "MP handle port ID %u invalid", param->port_id);
		return -ENODEV;
	}

	dev = &rte_eth_devices[param->port_id];

	mp_init_msg(&mp_res, param->type, param->port_id);

	switch (param->type) {
	case MANA_MP_REQ_START_RXTX:
		DRV_LOG(INFO, "Port %u starting datapath", dev->data->port_id);

		rte_mb();

		res->result = 0;
		ret = rte_mp_reply(&mp_res, peer);
		break;

	case MANA_MP_REQ_STOP_RXTX:
		DRV_LOG(INFO, "Port %u stopping datapath", dev->data->port_id);

		dev->tx_pkt_burst = mana_tx_burst_removed;
		dev->rx_pkt_burst = mana_rx_burst_removed;

		rte_mb();

		res->result = 0;
		ret = rte_mp_reply(&mp_res, peer);
		break;

	default:
		DRV_LOG(ERR, "Port %u unknown secondary MP type %u",
			param->port_id, param->type);
		ret = -EINVAL;
	}

	return ret;
}

int
mana_mp_init_primary(void)
{
	int ret;

	ret = rte_mp_action_register(MANA_MP_NAME, mana_mp_primary_handle);
	if (ret && rte_errno != ENOTSUP) {
		DRV_LOG(ERR, "Failed to register primary handler %d %d",
			ret, rte_errno);
		return -1;
	}

	return 0;
}

void
mana_mp_uninit_primary(void)
{
	rte_mp_action_unregister(MANA_MP_NAME);
}

int
mana_mp_init_secondary(void)
{
	return rte_mp_action_register(MANA_MP_NAME, mana_mp_secondary_handle);
}

void
mana_mp_uninit_secondary(void)
{
	rte_mp_action_unregister(MANA_MP_NAME);
}

int
mana_mp_req_verbs_cmd_fd(struct rte_eth_dev *dev)
{
	struct rte_mp_msg mp_req = { 0 };
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mana_mp_param *res;
	struct timespec ts = {.tv_sec = MANA_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	int ret;

	mp_init_msg(&mp_req, MANA_MP_REQ_VERBS_CMD_FD, dev->data->port_id);

	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		DRV_LOG(ERR, "port %u request to primary process failed",
			dev->data->port_id);
		return ret;
	}

	if (mp_rep.nb_received != 1) {
		DRV_LOG(ERR, "primary replied %u messages", mp_rep.nb_received);
		ret = -EPROTO;
		goto exit;
	}

	mp_res = &mp_rep.msgs[0];
	res = (struct mana_mp_param *)mp_res->param;
	if (res->result) {
		DRV_LOG(ERR, "failed to get CMD FD, port %u",
			dev->data->port_id);
		ret = res->result;
		goto exit;
	}

	if (mp_res->num_fds != 1) {
		DRV_LOG(ERR, "got FDs %d unexpected", mp_res->num_fds);
		ret = -EPROTO;
		goto exit;
	}

	ret = mp_res->fds[0];
	DRV_LOG(ERR, "port %u command FD from primary is %d",
		dev->data->port_id, ret);
exit:
	free(mp_rep.msgs);
	return ret;
}

void
mana_mp_req_on_rxtx(struct rte_eth_dev *dev, enum mana_mp_req_type type)
{
	struct rte_mp_msg mp_req = { 0 };
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct mana_mp_param *res;
	struct timespec ts = {.tv_sec = MANA_MP_REQ_TIMEOUT_SEC, .tv_nsec = 0};
	int i, ret;

	if (type != MANA_MP_REQ_START_RXTX && type != MANA_MP_REQ_STOP_RXTX) {
		DRV_LOG(ERR, "port %u unknown request (req_type %d)",
			dev->data->port_id, type);
		return;
	}

	if (!mana_shared_data->secondary_cnt)
		return;

	mp_init_msg(&mp_req, type, dev->data->port_id);

	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		if (rte_errno != ENOTSUP)
			DRV_LOG(ERR, "port %u failed to request Rx/Tx (%d)",
				dev->data->port_id, type);
		goto exit;
	}
	if (mp_rep.nb_sent != mp_rep.nb_received) {
		DRV_LOG(ERR, "port %u not all secondaries responded (%d)",
			dev->data->port_id, type);
		goto exit;
	}
	for (i = 0; i < mp_rep.nb_received; i++) {
		mp_res = &mp_rep.msgs[i];
		res = (struct mana_mp_param *)mp_res->param;
		if (res->result) {
			DRV_LOG(ERR, "port %u request failed on secondary %d",
				dev->data->port_id, i);
			goto exit;
		}
	}
exit:
	free(mp_rep.msgs);
}
