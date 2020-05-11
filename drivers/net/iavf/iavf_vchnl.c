/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>

#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_dev.h>

#include "iavf.h"
#include "iavf_rxtx.h"

#define MAX_TRY_TIMES 200
#define ASQ_DELAY_MS  10

/* Read data in admin queue to get msg from pf driver */
static enum iavf_status
iavf_read_msg_from_pf(struct iavf_adapter *adapter, uint16_t buf_len,
		     uint8_t *buf)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_arq_event_info event;
	enum virtchnl_ops opcode;
	int ret;

	event.buf_len = buf_len;
	event.msg_buf = buf;
	ret = iavf_clean_arq_element(hw, &event, NULL);
	/* Can't read any msg from adminQ */
	if (ret) {
		PMD_DRV_LOG(DEBUG, "Can't read msg from AQ");
		return ret;
	}

	opcode = (enum virtchnl_ops)rte_le_to_cpu_32(event.desc.cookie_high);
	vf->cmd_retval = (enum virtchnl_status_code)rte_le_to_cpu_32(
			event.desc.cookie_low);

	PMD_DRV_LOG(DEBUG, "AQ from pf carries opcode %u, retval %d",
		    opcode, vf->cmd_retval);

	if (opcode != vf->pend_cmd) {
		PMD_DRV_LOG(WARNING, "command mismatch, expect %u, get %u",
			    vf->pend_cmd, opcode);
		return IAVF_ERR_OPCODE_MISMATCH;
	}

	return IAVF_SUCCESS;
}

static int
iavf_execute_vf_cmd(struct iavf_adapter *adapter, struct iavf_cmd_info *args)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	enum iavf_status ret;
	int err = 0;
	int i = 0;

	if (_atomic_set_cmd(vf, args->ops))
		return -1;

	ret = iavf_aq_send_msg_to_pf(hw, args->ops, IAVF_SUCCESS,
				    args->in_args, args->in_args_size, NULL);
	if (ret) {
		PMD_DRV_LOG(ERR, "fail to send cmd %d", args->ops);
		_clear_cmd(vf);
		return err;
	}

	switch (args->ops) {
	case VIRTCHNL_OP_RESET_VF:
		/*no need to wait for response */
		_clear_cmd(vf);
		break;
	case VIRTCHNL_OP_VERSION:
	case VIRTCHNL_OP_GET_VF_RESOURCES:
	case VIRTCHNL_OP_GET_SUPPORTED_RXDIDS:
		/* for init virtchnl ops, need to poll the response */
		do {
			ret = iavf_read_msg_from_pf(adapter, args->out_size,
						   args->out_buffer);
			if (ret == IAVF_SUCCESS)
				break;
			rte_delay_ms(ASQ_DELAY_MS);
		} while (i++ < MAX_TRY_TIMES);
		if (i >= MAX_TRY_TIMES ||
		    vf->cmd_retval != VIRTCHNL_STATUS_SUCCESS) {
			err = -1;
			PMD_DRV_LOG(ERR, "No response or return failure (%d)"
				    " for cmd %d", vf->cmd_retval, args->ops);
		}
		_clear_cmd(vf);
		break;

	default:
		/* For other virtchnl ops in running time,
		 * wait for the cmd done flag.
		 */
		do {
			if (vf->pend_cmd == VIRTCHNL_OP_UNKNOWN)
				break;
			rte_delay_ms(ASQ_DELAY_MS);
			/* If don't read msg or read sys event, continue */
		} while (i++ < MAX_TRY_TIMES);
		/* If there's no response is received, clear command */
		if (i >= MAX_TRY_TIMES  ||
		    vf->cmd_retval != VIRTCHNL_STATUS_SUCCESS) {
			err = -1;
			PMD_DRV_LOG(ERR, "No response or return failure (%d)"
				    " for cmd %d", vf->cmd_retval, args->ops);
			_clear_cmd(vf);
		}
		break;
	}

	return err;
}

static uint32_t
iavf_convert_link_speed(enum virtchnl_link_speed virt_link_speed)
{
	uint32_t speed;

	switch (virt_link_speed) {
	case VIRTCHNL_LINK_SPEED_100MB:
		speed = 100;
		break;
	case VIRTCHNL_LINK_SPEED_1GB:
		speed = 1000;
		break;
	case VIRTCHNL_LINK_SPEED_10GB:
		speed = 10000;
		break;
	case VIRTCHNL_LINK_SPEED_40GB:
		speed = 40000;
		break;
	case VIRTCHNL_LINK_SPEED_20GB:
		speed = 20000;
		break;
	case VIRTCHNL_LINK_SPEED_25GB:
		speed = 25000;
		break;
	case VIRTCHNL_LINK_SPEED_2_5GB:
		speed = 2500;
		break;
	case VIRTCHNL_LINK_SPEED_5GB:
		speed = 5000;
		break;
	default:
		speed = 0;
		break;
	}

	return speed;
}

static void
iavf_handle_pf_event_msg(struct rte_eth_dev *dev, uint8_t *msg,
			uint16_t msglen)
{
	struct virtchnl_pf_event *pf_msg =
			(struct virtchnl_pf_event *)msg;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	if (msglen < sizeof(struct virtchnl_pf_event)) {
		PMD_DRV_LOG(DEBUG, "Error event");
		return;
	}
	switch (pf_msg->event) {
	case VIRTCHNL_EVENT_RESET_IMPENDING:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_RESET_IMPENDING event");
		_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_RESET,
					      NULL);
		break;
	case VIRTCHNL_EVENT_LINK_CHANGE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_LINK_CHANGE event");
		vf->link_up = pf_msg->event_data.link_event.link_status;
		if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_CAP_ADV_LINK_SPEED) {
			vf->link_speed =
				pf_msg->event_data.link_event_adv.link_speed;
		} else {
			enum virtchnl_link_speed speed;
			speed = pf_msg->event_data.link_event.link_speed;
			vf->link_speed = iavf_convert_link_speed(speed);
		}
		iavf_dev_link_update(dev, 0);
		_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC,
					      NULL);
		break;
	case VIRTCHNL_EVENT_PF_DRIVER_CLOSE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_PF_DRIVER_CLOSE event");
		break;
	default:
		PMD_DRV_LOG(ERR, " unknown event received %u", pf_msg->event);
		break;
	}
}

void
iavf_handle_virtchnl_msg(struct rte_eth_dev *dev)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_arq_event_info info;
	uint16_t pending, aq_opc;
	enum virtchnl_ops msg_opc;
	enum iavf_status msg_ret;
	int ret;

	info.buf_len = IAVF_AQ_BUF_SZ;
	if (!vf->aq_resp) {
		PMD_DRV_LOG(ERR, "Buffer for adminq resp should not be NULL");
		return;
	}
	info.msg_buf = vf->aq_resp;

	pending = 1;
	while (pending) {
		ret = iavf_clean_arq_element(hw, &info, &pending);

		if (ret != IAVF_SUCCESS) {
			PMD_DRV_LOG(INFO, "Failed to read msg from AdminQ,"
				    "ret: %d", ret);
			break;
		}
		aq_opc = rte_le_to_cpu_16(info.desc.opcode);
		/* For the message sent from pf to vf, opcode is stored in
		 * cookie_high of struct iavf_aq_desc, while return error code
		 * are stored in cookie_low, Which is done by PF driver.
		 */
		msg_opc = (enum virtchnl_ops)rte_le_to_cpu_32(
						  info.desc.cookie_high);
		msg_ret = (enum iavf_status)rte_le_to_cpu_32(
						  info.desc.cookie_low);
		switch (aq_opc) {
		case iavf_aqc_opc_send_msg_to_vf:
			if (msg_opc == VIRTCHNL_OP_EVENT) {
				iavf_handle_pf_event_msg(dev, info.msg_buf,
							info.msg_len);
			} else {
				/* read message and it's expected one */
				if (msg_opc == vf->pend_cmd)
					_notify_cmd(vf, msg_ret);
				else
					PMD_DRV_LOG(ERR, "command mismatch,"
						    "expect %u, get %u",
						    vf->pend_cmd, msg_opc);
				PMD_DRV_LOG(DEBUG,
					    "adminq response is received,"
					    " opcode = %d", msg_opc);
			}
			break;
		default:
			PMD_DRV_LOG(ERR, "Request %u is not supported yet",
				    aq_opc);
			break;
		}
	}
}

int
iavf_enable_vlan_strip(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	int ret;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL_OP_ENABLE_VLAN_STRIPPING;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	ret = iavf_execute_vf_cmd(adapter, &args);
	if (ret)
		PMD_DRV_LOG(ERR, "Failed to execute command of"
			    " OP_ENABLE_VLAN_STRIPPING");

	return ret;
}

int
iavf_disable_vlan_strip(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	int ret;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL_OP_DISABLE_VLAN_STRIPPING;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	ret = iavf_execute_vf_cmd(adapter, &args);
	if (ret)
		PMD_DRV_LOG(ERR, "Failed to execute command of"
			    " OP_DISABLE_VLAN_STRIPPING");

	return ret;
}

#define VIRTCHNL_VERSION_MAJOR_START 1
#define VIRTCHNL_VERSION_MINOR_START 1

/* Check API version with sync wait until version read from admin queue */
int
iavf_check_api_version(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_version_info version, *pver;
	struct iavf_cmd_info args;
	int err;

	version.major = VIRTCHNL_VERSION_MAJOR;
	version.minor = VIRTCHNL_VERSION_MINOR;

	args.ops = VIRTCHNL_OP_VERSION;
	args.in_args = (uint8_t *)&version;
	args.in_args_size = sizeof(version);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd(adapter, &args);
	if (err) {
		PMD_INIT_LOG(ERR, "Fail to execute command of OP_VERSION");
		return err;
	}

	pver = (struct virtchnl_version_info *)args.out_buffer;
	vf->virtchnl_version = *pver;

	if (vf->virtchnl_version.major < VIRTCHNL_VERSION_MAJOR_START ||
	    (vf->virtchnl_version.major == VIRTCHNL_VERSION_MAJOR_START &&
	     vf->virtchnl_version.minor < VIRTCHNL_VERSION_MINOR_START)) {
		PMD_INIT_LOG(ERR, "VIRTCHNL API version should not be lower"
			     " than (%u.%u) to support Adapative VF",
			     VIRTCHNL_VERSION_MAJOR_START,
			     VIRTCHNL_VERSION_MAJOR_START);
		return -1;
	} else if (vf->virtchnl_version.major > VIRTCHNL_VERSION_MAJOR ||
		   (vf->virtchnl_version.major == VIRTCHNL_VERSION_MAJOR &&
		    vf->virtchnl_version.minor > VIRTCHNL_VERSION_MINOR)) {
		PMD_INIT_LOG(ERR, "PF/VF API version mismatch:(%u.%u)-(%u.%u)",
			     vf->virtchnl_version.major,
			     vf->virtchnl_version.minor,
			     VIRTCHNL_VERSION_MAJOR,
			     VIRTCHNL_VERSION_MINOR);
		return -1;
	}

	PMD_DRV_LOG(DEBUG, "Peer is supported PF host");
	return 0;
}

int
iavf_get_vf_resource(struct iavf_adapter *adapter)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	uint32_t caps, len;
	int err, i;

	args.ops = VIRTCHNL_OP_GET_VF_RESOURCES;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	caps = IAVF_BASIC_OFFLOAD_CAPS | VIRTCHNL_VF_CAP_ADV_LINK_SPEED |
		VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC |
		VIRTCHNL_VF_OFFLOAD_FDIR_PF |
		VIRTCHNL_VF_OFFLOAD_ADV_RSS_PF;

	args.in_args = (uint8_t *)&caps;
	args.in_args_size = sizeof(caps);

	err = iavf_execute_vf_cmd(adapter, &args);

	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_GET_VF_RESOURCE");
		return -1;
	}

	len =  sizeof(struct virtchnl_vf_resource) +
		      IAVF_MAX_VF_VSI * sizeof(struct virtchnl_vsi_resource);

	rte_memcpy(vf->vf_res, args.out_buffer,
		   RTE_MIN(args.out_size, len));
	/* parse  VF config message back from PF*/
	iavf_vf_parse_hw_config(hw, vf->vf_res);
	for (i = 0; i < vf->vf_res->num_vsis; i++) {
		if (vf->vf_res->vsi_res[i].vsi_type == VIRTCHNL_VSI_SRIOV)
			vf->vsi_res = &vf->vf_res->vsi_res[i];
	}

	if (!vf->vsi_res) {
		PMD_INIT_LOG(ERR, "no LAN VSI found");
		return -1;
	}

	vf->vsi.vsi_id = vf->vsi_res->vsi_id;
	vf->vsi.nb_qps = vf->vsi_res->num_queue_pairs;
	vf->vsi.adapter = adapter;

	return 0;
}

int
iavf_get_supported_rxdid(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	int ret;

	args.ops = VIRTCHNL_OP_GET_SUPPORTED_RXDIDS;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	ret = iavf_execute_vf_cmd(adapter, &args);
	if (ret) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_GET_SUPPORTED_RXDIDS");
		return ret;
	}

	vf->supported_rxdid =
		((struct virtchnl_supported_rxdids *)args.out_buffer)->supported_rxdids;

	return 0;
}

int
iavf_enable_queues(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_queue_select queue_select;
	struct iavf_cmd_info args;
	int err;

	memset(&queue_select, 0, sizeof(queue_select));
	queue_select.vsi_id = vf->vsi_res->vsi_id;

	queue_select.rx_queues = BIT(adapter->eth_dev->data->nb_rx_queues) - 1;
	queue_select.tx_queues = BIT(adapter->eth_dev->data->nb_tx_queues) - 1;

	args.ops = VIRTCHNL_OP_ENABLE_QUEUES;
	args.in_args = (u8 *)&queue_select;
	args.in_args_size = sizeof(queue_select);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd(adapter, &args);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_ENABLE_QUEUES");
		return err;
	}
	return 0;
}

int
iavf_disable_queues(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_queue_select queue_select;
	struct iavf_cmd_info args;
	int err;

	memset(&queue_select, 0, sizeof(queue_select));
	queue_select.vsi_id = vf->vsi_res->vsi_id;

	queue_select.rx_queues = BIT(adapter->eth_dev->data->nb_rx_queues) - 1;
	queue_select.tx_queues = BIT(adapter->eth_dev->data->nb_tx_queues) - 1;

	args.ops = VIRTCHNL_OP_DISABLE_QUEUES;
	args.in_args = (u8 *)&queue_select;
	args.in_args_size = sizeof(queue_select);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd(adapter, &args);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_DISABLE_QUEUES");
		return err;
	}
	return 0;
}

int
iavf_switch_queue(struct iavf_adapter *adapter, uint16_t qid,
		 bool rx, bool on)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_queue_select queue_select;
	struct iavf_cmd_info args;
	int err;

	memset(&queue_select, 0, sizeof(queue_select));
	queue_select.vsi_id = vf->vsi_res->vsi_id;
	if (rx)
		queue_select.rx_queues |= 1 << qid;
	else
		queue_select.tx_queues |= 1 << qid;

	if (on)
		args.ops = VIRTCHNL_OP_ENABLE_QUEUES;
	else
		args.ops = VIRTCHNL_OP_DISABLE_QUEUES;
	args.in_args = (u8 *)&queue_select;
	args.in_args_size = sizeof(queue_select);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd(adapter, &args);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to execute command of %s",
			    on ? "OP_ENABLE_QUEUES" : "OP_DISABLE_QUEUES");
	return err;
}

int
iavf_configure_rss_lut(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_rss_lut *rss_lut;
	struct iavf_cmd_info args;
	int len, err = 0;

	len = sizeof(*rss_lut) + vf->vf_res->rss_lut_size - 1;
	rss_lut = rte_zmalloc("rss_lut", len, 0);
	if (!rss_lut)
		return -ENOMEM;

	rss_lut->vsi_id = vf->vsi_res->vsi_id;
	rss_lut->lut_entries = vf->vf_res->rss_lut_size;
	rte_memcpy(rss_lut->lut, vf->rss_lut, vf->vf_res->rss_lut_size);

	args.ops = VIRTCHNL_OP_CONFIG_RSS_LUT;
	args.in_args = (u8 *)rss_lut;
	args.in_args_size = len;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd(adapter, &args);
	if (err)
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_CONFIG_RSS_LUT");

	rte_free(rss_lut);
	return err;
}

int
iavf_configure_rss_key(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_rss_key *rss_key;
	struct iavf_cmd_info args;
	int len, err = 0;

	len = sizeof(*rss_key) + vf->vf_res->rss_key_size - 1;
	rss_key = rte_zmalloc("rss_key", len, 0);
	if (!rss_key)
		return -ENOMEM;

	rss_key->vsi_id = vf->vsi_res->vsi_id;
	rss_key->key_len = vf->vf_res->rss_key_size;
	rte_memcpy(rss_key->key, vf->rss_key, vf->vf_res->rss_key_size);

	args.ops = VIRTCHNL_OP_CONFIG_RSS_KEY;
	args.in_args = (u8 *)rss_key;
	args.in_args_size = len;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd(adapter, &args);
	if (err)
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_CONFIG_RSS_KEY");

	rte_free(rss_key);
	return err;
}

int
iavf_configure_queues(struct iavf_adapter *adapter)
{
	struct iavf_rx_queue **rxq =
		(struct iavf_rx_queue **)adapter->eth_dev->data->rx_queues;
	struct iavf_tx_queue **txq =
		(struct iavf_tx_queue **)adapter->eth_dev->data->tx_queues;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_vsi_queue_config_info *vc_config;
	struct virtchnl_queue_pair_info *vc_qp;
	struct iavf_cmd_info args;
	uint16_t i, size;
	int err;

	size = sizeof(*vc_config) +
	       sizeof(vc_config->qpair[0]) * vf->num_queue_pairs;
	vc_config = rte_zmalloc("cfg_queue", size, 0);
	if (!vc_config)
		return -ENOMEM;

	vc_config->vsi_id = vf->vsi_res->vsi_id;
	vc_config->num_queue_pairs = vf->num_queue_pairs;

	for (i = 0, vc_qp = vc_config->qpair;
	     i < vf->num_queue_pairs;
	     i++, vc_qp++) {
		vc_qp->txq.vsi_id = vf->vsi_res->vsi_id;
		vc_qp->txq.queue_id = i;
		/* Virtchnnl configure queues by pairs */
		if (i < adapter->eth_dev->data->nb_tx_queues) {
			vc_qp->txq.ring_len = txq[i]->nb_tx_desc;
			vc_qp->txq.dma_ring_addr = txq[i]->tx_ring_phys_addr;
		}
		vc_qp->rxq.vsi_id = vf->vsi_res->vsi_id;
		vc_qp->rxq.queue_id = i;
		vc_qp->rxq.max_pkt_size = vf->max_pkt_len;
		/* Virtchnnl configure queues by pairs */
		if (i < adapter->eth_dev->data->nb_rx_queues) {
			vc_qp->rxq.ring_len = rxq[i]->nb_rx_desc;
			vc_qp->rxq.dma_ring_addr = rxq[i]->rx_ring_phys_addr;
			vc_qp->rxq.databuffer_size = rxq[i]->rx_buf_len;
		}

#ifndef RTE_LIBRTE_IAVF_16BYTE_RX_DESC
		if (vf->vf_res->vf_cap_flags &
			VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC &&
			vf->supported_rxdid & BIT(IAVF_RXDID_COMMS_OVS_1)) {
			vc_qp->rxq.rxdid = IAVF_RXDID_COMMS_OVS_1;
			PMD_DRV_LOG(NOTICE, "request RXDID == %d in "
					"Queue[%d]", vc_qp->rxq.rxdid, i);
		} else {
			vc_qp->rxq.rxdid = IAVF_RXDID_LEGACY_1;
			PMD_DRV_LOG(NOTICE, "request RXDID == %d in "
					"Queue[%d]", vc_qp->rxq.rxdid, i);
		}
#else
		if (vf->vf_res->vf_cap_flags &
			VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC &&
			vf->supported_rxdid & BIT(IAVF_RXDID_LEGACY_0)) {
			vc_qp->rxq.rxdid = IAVF_RXDID_LEGACY_0;
			PMD_DRV_LOG(NOTICE, "request RXDID == %d in "
					"Queue[%d]", vc_qp->rxq.rxdid, i);
		} else {
			PMD_DRV_LOG(ERR, "RXDID == 0 is not supported");
			return -1;
		}
#endif
	}

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL_OP_CONFIG_VSI_QUEUES;
	args.in_args = (uint8_t *)vc_config;
	args.in_args_size = size;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd(adapter, &args);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to execute command of"
			    " VIRTCHNL_OP_CONFIG_VSI_QUEUES");

	rte_free(vc_config);
	return err;
}

int
iavf_config_irq_map(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_irq_map_info *map_info;
	struct virtchnl_vector_map *vecmap;
	struct iavf_cmd_info args;
	int len, i, err;

	len = sizeof(struct virtchnl_irq_map_info) +
	      sizeof(struct virtchnl_vector_map) * vf->nb_msix;

	map_info = rte_zmalloc("map_info", len, 0);
	if (!map_info)
		return -ENOMEM;

	map_info->num_vectors = vf->nb_msix;
	for (i = 0; i < vf->nb_msix; i++) {
		vecmap = &map_info->vecmap[i];
		vecmap->vsi_id = vf->vsi_res->vsi_id;
		vecmap->rxitr_idx = IAVF_ITR_INDEX_DEFAULT;
		vecmap->vector_id = vf->msix_base + i;
		vecmap->txq_map = 0;
		vecmap->rxq_map = vf->rxq_map[vf->msix_base + i];
	}

	args.ops = VIRTCHNL_OP_CONFIG_IRQ_MAP;
	args.in_args = (u8 *)map_info;
	args.in_args_size = len;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd(adapter, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command OP_CONFIG_IRQ_MAP");

	rte_free(map_info);
	return err;
}

void
iavf_add_del_all_mac_addr(struct iavf_adapter *adapter, bool add)
{
	struct virtchnl_ether_addr_list *list;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct rte_ether_addr *addr;
	struct iavf_cmd_info args;
	int len, err, i, j;
	int next_begin = 0;
	int begin = 0;

	do {
		j = 0;
		len = sizeof(struct virtchnl_ether_addr_list);
		for (i = begin; i < IAVF_NUM_MACADDR_MAX; i++, next_begin++) {
			addr = &adapter->eth_dev->data->mac_addrs[i];
			if (rte_is_zero_ether_addr(addr))
				continue;
			len += sizeof(struct virtchnl_ether_addr);
			if (len >= IAVF_AQ_BUF_SZ) {
				next_begin = i + 1;
				break;
			}
		}

		list = rte_zmalloc("iavf_del_mac_buffer", len, 0);
		if (!list) {
			PMD_DRV_LOG(ERR, "fail to allocate memory");
			return;
		}

		for (i = begin; i < next_begin; i++) {
			addr = &adapter->eth_dev->data->mac_addrs[i];
			if (rte_is_zero_ether_addr(addr))
				continue;
			rte_memcpy(list->list[j].addr, addr->addr_bytes,
				   sizeof(addr->addr_bytes));
			PMD_DRV_LOG(DEBUG, "add/rm mac:%x:%x:%x:%x:%x:%x",
				    addr->addr_bytes[0], addr->addr_bytes[1],
				    addr->addr_bytes[2], addr->addr_bytes[3],
				    addr->addr_bytes[4], addr->addr_bytes[5]);
			j++;
		}
		list->vsi_id = vf->vsi_res->vsi_id;
		list->num_elements = j;
		args.ops = add ? VIRTCHNL_OP_ADD_ETH_ADDR :
			   VIRTCHNL_OP_DEL_ETH_ADDR;
		args.in_args = (uint8_t *)list;
		args.in_args_size = len;
		args.out_buffer = vf->aq_resp;
		args.out_size = IAVF_AQ_BUF_SZ;
		err = iavf_execute_vf_cmd(adapter, &args);
		if (err)
			PMD_DRV_LOG(ERR, "fail to execute command %s",
				    add ? "OP_ADD_ETHER_ADDRESS" :
				    "OP_DEL_ETHER_ADDRESS");
		rte_free(list);
		begin = next_begin;
	} while (begin < IAVF_NUM_MACADDR_MAX);
}

int
iavf_query_stats(struct iavf_adapter *adapter,
		struct virtchnl_eth_stats **pstats)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_queue_select q_stats;
	struct iavf_cmd_info args;
	int err;

	memset(&q_stats, 0, sizeof(q_stats));
	q_stats.vsi_id = vf->vsi_res->vsi_id;
	args.ops = VIRTCHNL_OP_GET_STATS;
	args.in_args = (uint8_t *)&q_stats;
	args.in_args_size = sizeof(q_stats);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd(adapter, &args);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command OP_GET_STATS");
		*pstats = NULL;
		return err;
	}
	*pstats = (struct virtchnl_eth_stats *)args.out_buffer;
	return 0;
}

int
iavf_config_promisc(struct iavf_adapter *adapter,
		   bool enable_unicast,
		   bool enable_multicast)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_promisc_info promisc;
	struct iavf_cmd_info args;
	int err;

	promisc.flags = 0;
	promisc.vsi_id = vf->vsi_res->vsi_id;

	if (enable_unicast)
		promisc.flags |= FLAG_VF_UNICAST_PROMISC;

	if (enable_multicast)
		promisc.flags |= FLAG_VF_MULTICAST_PROMISC;

	args.ops = VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE;
	args.in_args = (uint8_t *)&promisc;
	args.in_args_size = sizeof(promisc);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd(adapter, &args);

	if (err)
		PMD_DRV_LOG(ERR,
			    "fail to execute command CONFIG_PROMISCUOUS_MODE");
	return err;
}

int
iavf_add_del_eth_addr(struct iavf_adapter *adapter, struct rte_ether_addr *addr,
		     bool add)
{
	struct virtchnl_ether_addr_list *list;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	uint8_t cmd_buffer[sizeof(struct virtchnl_ether_addr_list) +
			   sizeof(struct virtchnl_ether_addr)];
	struct iavf_cmd_info args;
	int err;

	list = (struct virtchnl_ether_addr_list *)cmd_buffer;
	list->vsi_id = vf->vsi_res->vsi_id;
	list->num_elements = 1;
	rte_memcpy(list->list[0].addr, addr->addr_bytes,
		   sizeof(addr->addr_bytes));

	args.ops = add ? VIRTCHNL_OP_ADD_ETH_ADDR : VIRTCHNL_OP_DEL_ETH_ADDR;
	args.in_args = cmd_buffer;
	args.in_args_size = sizeof(cmd_buffer);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd(adapter, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command %s",
			    add ? "OP_ADD_ETH_ADDR" :  "OP_DEL_ETH_ADDR");
	return err;
}

int
iavf_add_del_vlan(struct iavf_adapter *adapter, uint16_t vlanid, bool add)
{
	struct virtchnl_vlan_filter_list *vlan_list;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	uint8_t cmd_buffer[sizeof(struct virtchnl_vlan_filter_list) +
							sizeof(uint16_t)];
	struct iavf_cmd_info args;
	int err;

	vlan_list = (struct virtchnl_vlan_filter_list *)cmd_buffer;
	vlan_list->vsi_id = vf->vsi_res->vsi_id;
	vlan_list->num_elements = 1;
	vlan_list->vlan_id[0] = vlanid;

	args.ops = add ? VIRTCHNL_OP_ADD_VLAN : VIRTCHNL_OP_DEL_VLAN;
	args.in_args = cmd_buffer;
	args.in_args_size = sizeof(cmd_buffer);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd(adapter, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command %s",
			    add ? "OP_ADD_VLAN" :  "OP_DEL_VLAN");

	return err;
}

int
iavf_fdir_add(struct iavf_adapter *adapter,
	struct iavf_fdir_conf *filter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_fdir_add *fdir_ret;

	struct iavf_cmd_info args;
	int err;

	filter->add_fltr.vsi_id = vf->vsi_res->vsi_id;
	filter->add_fltr.validate_only = 0;

	args.ops = VIRTCHNL_OP_ADD_FDIR_FILTER;
	args.in_args = (uint8_t *)(&filter->add_fltr);
	args.in_args_size = sizeof(*(&filter->add_fltr));
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd(adapter, &args);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command OP_ADD_FDIR_FILTER");
		return err;
	}

	fdir_ret = (struct virtchnl_fdir_add *)args.out_buffer;
	filter->flow_id = fdir_ret->flow_id;

	if (fdir_ret->status == VIRTCHNL_FDIR_SUCCESS) {
		PMD_DRV_LOG(INFO,
			"Succeed in adding rule request by PF");
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_NORESOURCE) {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to no hw resource");
		return -1;
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_EXIST) {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to the rule is already existed");
		return -1;
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_CONFLICT) {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to the rule is conflict with existing rule");
		return -1;
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_INVALID) {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to the hw doesn't support");
		return -1;
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_TIMEOUT) {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to time out for programming");
		return -1;
	} else {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to other reasons");
		return -1;
	}

	return 0;
};

int
iavf_fdir_del(struct iavf_adapter *adapter,
	struct iavf_fdir_conf *filter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_fdir_del *fdir_ret;

	struct iavf_cmd_info args;
	int err;

	filter->del_fltr.vsi_id = vf->vsi_res->vsi_id;
	filter->del_fltr.flow_id = filter->flow_id;

	args.ops = VIRTCHNL_OP_DEL_FDIR_FILTER;
	args.in_args = (uint8_t *)(&filter->del_fltr);
	args.in_args_size = sizeof(filter->del_fltr);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd(adapter, &args);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command OP_DEL_FDIR_FILTER");
		return err;
	}

	fdir_ret = (struct virtchnl_fdir_del *)args.out_buffer;

	if (fdir_ret->status == VIRTCHNL_FDIR_SUCCESS) {
		PMD_DRV_LOG(INFO,
			"Succeed in deleting rule request by PF");
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_NONEXIST) {
		PMD_DRV_LOG(ERR,
			"Failed to delete rule request due to this rule doesn't exist");
		return -1;
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_TIMEOUT) {
		PMD_DRV_LOG(ERR,
			"Failed to delete rule request due to time out for programming");
		return -1;
	} else {
		PMD_DRV_LOG(ERR,
			"Failed to delete rule request due to other reasons");
		return -1;
	}

	return 0;
};

int
iavf_fdir_check(struct iavf_adapter *adapter,
		struct iavf_fdir_conf *filter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_fdir_add *fdir_ret;

	struct iavf_cmd_info args;
	int err;

	filter->add_fltr.vsi_id = vf->vsi_res->vsi_id;
	filter->add_fltr.validate_only = 1;

	args.ops = VIRTCHNL_OP_ADD_FDIR_FILTER;
	args.in_args = (uint8_t *)(&filter->add_fltr);
	args.in_args_size = sizeof(*(&filter->add_fltr));
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd(adapter, &args);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to check flow direcotor rule");
		return err;
	}

	fdir_ret = (struct virtchnl_fdir_add *)args.out_buffer;

	if (fdir_ret->status == VIRTCHNL_FDIR_SUCCESS) {
		PMD_DRV_LOG(INFO,
			"Succeed in checking rule request by PF");
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_INVALID) {
		PMD_DRV_LOG(ERR,
			"Failed to check rule request due to parameters validation"
			" or HW doesn't support");
		return -1;
	} else {
		PMD_DRV_LOG(ERR,
			"Failed to check rule request due to other reasons");
		return -1;
	}

	return 0;
}

int
iavf_add_del_rss_cfg(struct iavf_adapter *adapter,
		     struct virtchnl_rss_cfg *rss_cfg, bool add)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	int err;

	memset(&args, 0, sizeof(args));
	args.ops = add ? VIRTCHNL_OP_ADD_RSS_CFG :
		VIRTCHNL_OP_DEL_RSS_CFG;
	args.in_args = (u8 *)rss_cfg;
	args.in_args_size = sizeof(*rss_cfg);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd(adapter, &args);
	if (err)
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of %s",
			    add ? "OP_ADD_RSS_CFG" :
			    "OP_DEL_RSS_INPUT_CFG");

	return err;
}
