/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <stdlib.h>

#include <rte_eal.h>
#include <ethdev_driver.h>
#include <rte_string_fns.h>
#include <rte_io.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"
#include "hns3_rxtx.h"
#include "hns3_mp.h"

/* local data for primary or secondary process. */
static struct hns3_process_local_data process_data;

/*
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
	    enum hns3_mp_req_type type)
{
	struct hns3_mp_param *param = (struct hns3_mp_param *)msg->param;

	memset(msg, 0, sizeof(*msg));
	strlcpy(msg->name, HNS3_MP_NAME, sizeof(msg->name));
	msg->len_param = sizeof(*param);
	param->type = type;
	param->port_id = dev->data->port_id;
}

/*
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
mp_primary_handle(const struct rte_mp_msg *mp_msg __rte_unused,
		  const void *peer __rte_unused)
{
	return 0;
}

/*
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
	struct hns3_mp_param *res = (struct hns3_mp_param *)mp_res.param;
	const struct hns3_mp_param *param =
		(const struct hns3_mp_param *)mp_msg->param;
	struct rte_eth_dev *dev;
	int ret;

	if (!rte_eth_dev_is_valid_port(param->port_id)) {
		rte_errno = ENODEV;
		PMD_INIT_LOG(ERR, "port %d invalid port ID", param->port_id);
		return -rte_errno;
	}
	dev = &rte_eth_devices[param->port_id];
	switch (param->type) {
	case HNS3_MP_REQ_START_RXTX:
		PMD_INIT_LOG(INFO, "port %u starting datapath",
			     dev->data->port_id);
		hns3_start_rxtx_datapath(dev);
		break;
	case HNS3_MP_REQ_STOP_RXTX:
		PMD_INIT_LOG(INFO, "port %u stopping datapath",
			     dev->data->port_id);
		hns3_stop_rxtx_datapath(dev);
		break;
	case HNS3_MP_REQ_START_TX:
		PMD_INIT_LOG(INFO, "port %u starting Tx datapath",
			     dev->data->port_id);
		hns3_start_tx_datapath(dev);
		break;
	case HNS3_MP_REQ_STOP_TX:
		PMD_INIT_LOG(INFO, "port %u stopping Tx datapath",
			     dev->data->port_id);
		hns3_stop_tx_datapath(dev);
		break;
	default:
		rte_errno = EINVAL;
		PMD_INIT_LOG(ERR, "port %u invalid mp request type",
			     dev->data->port_id);
		return -rte_errno;
	}

	rte_mb();
	mp_init_msg(dev, &mp_res, param->type);
	res->result = 0;
	ret = rte_mp_reply(&mp_res, peer);

	return ret;
}

static bool
mp_req_type_is_valid(enum hns3_mp_req_type type)
{
	return type == HNS3_MP_REQ_START_RXTX ||
		type == HNS3_MP_REQ_STOP_RXTX ||
		type == HNS3_MP_REQ_START_TX ||
		type == HNS3_MP_REQ_STOP_TX;
}

/*
 * Broadcast request of stopping/starting data-path to secondary processes.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 * @param[in] type
 *   Request type.
 */
static void
mp_req_on_rxtx(struct rte_eth_dev *dev, enum hns3_mp_req_type type)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_mp_msg mp_req;
	struct rte_mp_msg *mp_res;
	struct rte_mp_reply mp_rep;
	struct hns3_mp_param *res;
	struct timespec ts;
	int ret;
	int i;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY ||
		__atomic_load_n(&hw->secondary_cnt, __ATOMIC_RELAXED) == 0)
		return;

	if (!mp_req_type_is_valid(type)) {
		hns3_err(hw, "port %u unknown request (req_type %d)",
			 dev->data->port_id, type);
		return;
	}
	mp_init_msg(dev, &mp_req, type);
	ts.tv_sec = HNS3_MP_REQ_TIMEOUT_SEC;
	ts.tv_nsec = 0;
	ret = rte_mp_request_sync(&mp_req, &mp_rep, &ts);
	if (ret) {
		hns3_err(hw, "port %u failed to request stop/start Rx/Tx (%d)",
			 dev->data->port_id, type);
		goto exit;
	}
	if (mp_rep.nb_sent != mp_rep.nb_received) {
		PMD_INIT_LOG(ERR,
			"port %u not all secondaries responded (req_type %d)",
			dev->data->port_id, type);
		goto exit;
	}
	for (i = 0; i < mp_rep.nb_received; i++) {
		mp_res = &mp_rep.msgs[i];
		res = (struct hns3_mp_param *)mp_res->param;
		if (res->result) {
			hns3_err(hw, "port %u request failed on secondary #%d",
				 dev->data->port_id, i);
			goto exit;
		}
	}
exit:
	free(mp_rep.msgs);
}

/*
 * Broadcast request of starting data-path to secondary processes. The request
 * is synchronous.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 */
void hns3_mp_req_start_rxtx(struct rte_eth_dev *dev)
{
	mp_req_on_rxtx(dev, HNS3_MP_REQ_START_RXTX);
}

/*
 * Broadcast request of stopping data-path to secondary processes. The request
 * is synchronous.
 *
 * @param[in] dev
 *   Pointer to Ethernet structure.
 */
void hns3_mp_req_stop_rxtx(struct rte_eth_dev *dev)
{
	mp_req_on_rxtx(dev, HNS3_MP_REQ_STOP_RXTX);
}

void
hns3_mp_req_stop_tx(struct rte_eth_dev *dev)
{
	mp_req_on_rxtx(dev, HNS3_MP_REQ_STOP_TX);
}

void
hns3_mp_req_start_tx(struct rte_eth_dev *dev)
{
	mp_req_on_rxtx(dev, HNS3_MP_REQ_START_TX);
}

/*
 * Initialize by primary process.
 */
static int
hns3_mp_init_primary(void)
{
	int ret;

	if (process_data.init_done)
		return 0;

	/* primary is allowed to not support IPC */
	ret = rte_mp_action_register(HNS3_MP_NAME, mp_primary_handle);
	if (ret && rte_errno != ENOTSUP)
		return ret;

	process_data.init_done = true;

	return 0;
}

/*
 * Initialize by secondary process.
 */
static int
hns3_mp_init_secondary(void)
{
	int ret;

	if (process_data.init_done)
		return 0;

	ret = rte_mp_action_register(HNS3_MP_NAME, mp_secondary_handle);
	if (ret && rte_errno != ENOTSUP)
		return ret;

	process_data.init_done = true;

	return 0;
}

int
hns3_mp_init(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		ret = hns3_mp_init_secondary();
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to init for secondary process, ret = %d",
				     ret);
			return ret;
		}
		__atomic_fetch_add(&hw->secondary_cnt, 1, __ATOMIC_RELAXED);
	} else {
		ret = hns3_mp_init_primary();
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to init for primary process, ret = %d",
				     ret);
			return ret;
		}
	}

	process_data.eth_dev_cnt++;

	return 0;
}

void hns3_mp_uninit(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		__atomic_fetch_sub(&hw->secondary_cnt, 1, __ATOMIC_RELAXED);

	process_data.eth_dev_cnt--;
	if (process_data.eth_dev_cnt == 0) {
		rte_mp_action_unregister(HNS3_MP_NAME);
		process_data.init_done = false;
	}
}
