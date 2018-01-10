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
#include <rte_ethdev.h>
#include <rte_dev.h>

#include "avf_log.h"
#include "base/avf_prototype.h"
#include "base/avf_adminq_cmd.h"
#include "base/avf_type.h"

#include "avf.h"

#define MAX_TRY_TIMES 200
#define ASQ_DELAY_MS  10

/* Read data in admin queue to get msg from pf driver */
static enum avf_status_code
avf_read_msg_from_pf(struct avf_adapter *adapter, uint16_t buf_len,
		     uint8_t *buf)
{
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(adapter);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	struct avf_arq_event_info event;
	enum virtchnl_ops opcode;
	int ret;

	event.buf_len = buf_len;
	event.msg_buf = buf;
	ret = avf_clean_arq_element(hw, &event, NULL);
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

	if (opcode != vf->pend_cmd)
		PMD_DRV_LOG(WARNING, "command mismatch, expect %u, get %u",
			    vf->pend_cmd, opcode);

	return AVF_SUCCESS;
}

static int
avf_execute_vf_cmd(struct avf_adapter *adapter, struct avf_cmd_info *args)
{
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(adapter);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	struct avf_arq_event_info event_info;
	enum avf_status_code ret;
	int err = 0;
	int i = 0;

	if (_atomic_set_cmd(vf, args->ops))
		return -1;

	ret = avf_aq_send_msg_to_pf(hw, args->ops, AVF_SUCCESS,
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
		/* for init virtchnl ops, need to poll the response */
		do {
			ret = avf_read_msg_from_pf(adapter, args->out_size,
						   args->out_buffer);
			if (ret == AVF_SUCCESS)
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

void
avf_handle_virtchnl_msg(struct rte_eth_dev *dev)
{
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct avf_arq_event_info info;
	uint16_t pending, aq_opc;
	enum virtchnl_ops msg_opc;
	enum avf_status_code msg_ret;
	int ret;

	info.buf_len = AVF_AQ_BUF_SZ;
	if (!vf->aq_resp) {
		PMD_DRV_LOG(ERR, "Buffer for adminq resp should not be NULL");
		return;
	}
	info.msg_buf = vf->aq_resp;

	pending = 1;
	while (pending) {
		ret = avf_clean_arq_element(hw, &info, &pending);

		if (ret != AVF_SUCCESS) {
			PMD_DRV_LOG(INFO, "Failed to read msg from AdminQ,"
				    "ret: %d", ret);
			break;
		}
		aq_opc = rte_le_to_cpu_16(info.desc.opcode);
		/* For the message sent from pf to vf, opcode is stored in
		 * cookie_high of struct avf_aq_desc, while return error code
		 * are stored in cookie_low, Which is done by PF driver.
		 */
		msg_opc = (enum virtchnl_ops)rte_le_to_cpu_32(
						  info.desc.cookie_high);
		msg_ret = (enum avf_status_code)rte_le_to_cpu_32(
						  info.desc.cookie_low);
		switch (aq_opc) {
		case avf_aqc_opc_send_msg_to_vf:
			if (msg_opc == VIRTCHNL_OP_EVENT) {
				/* TODO */
			} else {
				/* read message and it's expected one */
				if (msg_opc == vf->pend_cmd) {
					vf->cmd_retval = msg_ret;
					/* prevent compiler reordering */
					rte_compiler_barrier();
					_clear_cmd(vf);
				} else
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

#define VIRTCHNL_VERSION_MAJOR_START 1
#define VIRTCHNL_VERSION_MINOR_START 1

/* Check API version with sync wait until version read from admin queue */
int
avf_check_api_version(struct avf_adapter *adapter)
{
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_version_info version, *pver;
	struct avf_cmd_info args;
	int err;

	version.major = VIRTCHNL_VERSION_MAJOR;
	version.minor = VIRTCHNL_VERSION_MINOR;

	args.ops = VIRTCHNL_OP_VERSION;
	args.in_args = (uint8_t *)&version;
	args.in_args_size = sizeof(version);
	args.out_buffer = vf->aq_resp;
	args.out_size = AVF_AQ_BUF_SZ;

	err = avf_execute_vf_cmd(adapter, &args);
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
avf_get_vf_resource(struct avf_adapter *adapter)
{
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(adapter);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	struct avf_cmd_info args;
	uint32_t caps, len;
	int err, i;

	args.ops = VIRTCHNL_OP_GET_VF_RESOURCES;
	args.out_buffer = vf->aq_resp;
	args.out_size = AVF_AQ_BUF_SZ;

	/* TODO: basic offload capabilities, need to
	 * add advanced/optional offload capabilities
	 */

	caps = AVF_BASIC_OFFLOAD_CAPS;

	args.in_args = (uint8_t *)&caps;
	args.in_args_size = sizeof(caps);

	err = avf_execute_vf_cmd(adapter, &args);

	if (err) {
		PMD_DRV_LOG(ERR, "Failed to execute command of "
				 "OP_GET_VF_RESOURCE");
		return -1;
	}

	len =  sizeof(struct virtchnl_vf_resource) +
		      AVF_MAX_VF_VSI * sizeof(struct virtchnl_vsi_resource);

	rte_memcpy(vf->vf_res, args.out_buffer,
		   RTE_MIN(args.out_size, len));
	/* parse  VF config message back from PF*/
	avf_parse_hw_config(hw, vf->vf_res);
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
