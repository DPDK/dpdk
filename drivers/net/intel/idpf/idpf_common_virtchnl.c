/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include "idpf_common_virtchnl.h"
#include "idpf_common_logs.h"

#include <eal_export.h>

static int
idpf_vc_clean(struct idpf_adapter *adapter)
{
	struct idpf_ctlq_msg *q_msg[IDPF_CTLQ_LEN];
	uint16_t num_q_msg = IDPF_CTLQ_LEN;
	struct idpf_dma_mem *dma_mem;
	int err;
	uint32_t i;

	for (i = 0; i < 10; i++) {
		err = idpf_ctlq_clean_sq(adapter->hw.asq, &num_q_msg, q_msg);
		msleep(20);
		if (num_q_msg > 0)
			break;
	}
	if (err != 0)
		return err;

	/* Empty queue is not an error */
	for (i = 0; i < num_q_msg; i++) {
		dma_mem = q_msg[i]->ctx.indirect.payload;
		if (dma_mem != NULL) {
			idpf_free_dma_mem(&adapter->hw, dma_mem);
			rte_free(dma_mem);
		}
		rte_free(q_msg[i]);
	}

	return 0;
}

static int
idpf_send_vc_msg(struct idpf_adapter *adapter, uint32_t op,
		 uint16_t msg_size, uint8_t *msg)
{
	struct idpf_ctlq_msg *ctlq_msg;
	struct idpf_dma_mem *dma_mem;
	int err;

	err = idpf_vc_clean(adapter);
	if (err != 0)
		goto err;

	ctlq_msg = rte_zmalloc(NULL, sizeof(struct idpf_ctlq_msg), 0);
	if (ctlq_msg == NULL) {
		err = -ENOMEM;
		goto err;
	}

	dma_mem = rte_zmalloc(NULL, sizeof(struct idpf_dma_mem), 0);
	if (dma_mem == NULL) {
		err = -ENOMEM;
		goto dma_mem_error;
	}

	dma_mem->size = IDPF_DFLT_MBX_BUF_SIZE;
	idpf_alloc_dma_mem(&adapter->hw, dma_mem, dma_mem->size);
	if (dma_mem->va == NULL) {
		err = -ENOMEM;
		goto dma_alloc_error;
	}

	memcpy(dma_mem->va, msg, msg_size);

	ctlq_msg->opcode = idpf_mbq_opc_send_msg_to_pf;
	ctlq_msg->func_id = 0;
	ctlq_msg->data_len = msg_size;
	ctlq_msg->cookie.mbx.chnl_opcode = op;
	ctlq_msg->cookie.mbx.chnl_retval = VIRTCHNL_STATUS_SUCCESS;
	ctlq_msg->ctx.indirect.payload = dma_mem;

	err = idpf_ctlq_send(&adapter->hw, adapter->hw.asq, 1, ctlq_msg);
	if (err != 0)
		goto send_error;

	return 0;

send_error:
	idpf_free_dma_mem(&adapter->hw, dma_mem);
dma_alloc_error:
	rte_free(dma_mem);
dma_mem_error:
	rte_free(ctlq_msg);
err:
	return err;
}

static enum idpf_vc_result
idpf_read_msg_from_cp(struct idpf_adapter *adapter, uint16_t buf_len,
		      uint8_t *buf)
{
	struct idpf_hw *hw = &adapter->hw;
	struct idpf_ctlq_msg ctlq_msg;
	struct idpf_dma_mem *dma_mem = NULL;
	enum idpf_vc_result result = IDPF_MSG_NON;
	uint32_t opcode;
	uint16_t pending = 1;
	int ret;

	ret = idpf_ctlq_recv(hw->arq, &pending, &ctlq_msg);
	if (ret != 0) {
		DRV_LOG(DEBUG, "Can't read msg from AQ");
		if (ret != -ENOMSG)
			result = IDPF_MSG_ERR;
		return result;
	}

	rte_memcpy(buf, ctlq_msg.ctx.indirect.payload->va, buf_len);

	opcode = rte_le_to_cpu_32(ctlq_msg.cookie.mbx.chnl_opcode);
	adapter->cmd_retval = rte_le_to_cpu_32(ctlq_msg.cookie.mbx.chnl_retval);

	DRV_LOG(DEBUG, "CQ from CP carries opcode %u, retval %d",
		opcode, adapter->cmd_retval);

	if (opcode == VIRTCHNL2_OP_EVENT) {
		struct virtchnl2_event *ve = ctlq_msg.ctx.indirect.payload->va;

		result = IDPF_MSG_SYS;
		switch (ve->event) {
		case VIRTCHNL2_EVENT_LINK_CHANGE:
			/* TBD */
			break;
		default:
			DRV_LOG(ERR, "%s: Unknown event %d from CP",
				__func__, ve->event);
			break;
		}
	} else {
		/* async reply msg on command issued by pf previously */
		result = IDPF_MSG_CMD;
		if (opcode != adapter->pend_cmd) {
			DRV_LOG(WARNING, "command mismatch, expect %u, get %u",
				adapter->pend_cmd, opcode);
			result = IDPF_MSG_ERR;
		}
	}

	if (ctlq_msg.data_len != 0)
		dma_mem = ctlq_msg.ctx.indirect.payload;
	else
		pending = 0;

	ret = idpf_ctlq_post_rx_buffs(hw, hw->arq, &pending, &dma_mem);
	if (ret != 0 && dma_mem != NULL)
		idpf_free_dma_mem(hw, dma_mem);

	return result;
}

#define MAX_TRY_TIMES 200
#define ASQ_DELAY_MS  10

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_one_msg_read)
int
idpf_vc_one_msg_read(struct idpf_adapter *adapter, uint32_t ops, uint16_t buf_len,
		     uint8_t *buf)
{
	int err = 0;
	int i = 0;
	int ret;

	do {
		ret = idpf_read_msg_from_cp(adapter, buf_len, buf);
		if (ret == IDPF_MSG_CMD)
			break;
		rte_delay_ms(ASQ_DELAY_MS);
	} while (i++ < MAX_TRY_TIMES);
	if (i >= MAX_TRY_TIMES ||
	    adapter->cmd_retval != VIRTCHNL_STATUS_SUCCESS) {
		err = -EBUSY;
		DRV_LOG(ERR, "No response or return failure (%d) for cmd %d",
			adapter->cmd_retval, ops);
	}

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_cmd_execute)
int
idpf_vc_cmd_execute(struct idpf_adapter *adapter, struct idpf_cmd_info *args)
{
	int err = 0;
	int i = 0;
	int ret;

	if (atomic_set_cmd(adapter, args->ops))
		return -EINVAL;

	ret = idpf_send_vc_msg(adapter, args->ops, args->in_args_size, args->in_args);
	if (ret != 0) {
		DRV_LOG(ERR, "fail to send cmd %d", args->ops);
		clear_cmd(adapter);
		return ret;
	}

	switch (args->ops) {
	case VIRTCHNL_OP_VERSION:
	case VIRTCHNL2_OP_GET_CAPS:
	case VIRTCHNL2_OP_GET_PTYPE_INFO:
		/* for init virtchnl ops, need to poll the response */
		err = idpf_vc_one_msg_read(adapter, args->ops, args->out_size, args->out_buffer);
		clear_cmd(adapter);
		break;
	default:
		/* For other virtchnl ops in running time,
		 * wait for the cmd done flag.
		 */
		do {
			if (adapter->pend_cmd == VIRTCHNL_OP_UNKNOWN)
				break;
			rte_delay_ms(ASQ_DELAY_MS);
			/* If don't read msg or read sys event, continue */
		} while (i++ < MAX_TRY_TIMES);
		/* If there's no response is received, clear command */
		if (i >= MAX_TRY_TIMES  ||
		    adapter->cmd_retval != VIRTCHNL_STATUS_SUCCESS) {
			err = -EBUSY;
			DRV_LOG(ERR, "No response or return failure (%d) for cmd %d",
				adapter->cmd_retval, args->ops);
			clear_cmd(adapter);
		}
		break;
	}

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_api_version_check)
int
idpf_vc_api_version_check(struct idpf_adapter *adapter)
{
	struct virtchnl2_version_info version, *pver;
	struct idpf_cmd_info args;
	int err;

	memset(&version, 0, sizeof(struct virtchnl_version_info));
	version.major = VIRTCHNL2_VERSION_MAJOR_2;
	version.minor = VIRTCHNL2_VERSION_MINOR_0;

	args.ops = VIRTCHNL_OP_VERSION;
	args.in_args = (uint8_t *)&version;
	args.in_args_size = sizeof(version);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0) {
		DRV_LOG(ERR,
			"Failed to execute command of VIRTCHNL_OP_VERSION");
		return err;
	}

	pver = (struct virtchnl2_version_info *)args.out_buffer;
	adapter->virtchnl_version = *pver;

	if (adapter->virtchnl_version.major != VIRTCHNL2_VERSION_MAJOR_2 ||
	    adapter->virtchnl_version.minor != VIRTCHNL2_VERSION_MINOR_0) {
		DRV_LOG(ERR, "VIRTCHNL API version mismatch:(%u.%u)-(%u.%u)",
			adapter->virtchnl_version.major,
			adapter->virtchnl_version.minor,
			VIRTCHNL2_VERSION_MAJOR_2,
			VIRTCHNL2_VERSION_MINOR_0);
		return -EINVAL;
	}

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_caps_get)
int
idpf_vc_caps_get(struct idpf_adapter *adapter)
{
	struct idpf_cmd_info args;
	int err;

	args.ops = VIRTCHNL2_OP_GET_CAPS;
	args.in_args = (uint8_t *)&adapter->caps;
	args.in_args_size = sizeof(struct virtchnl2_get_capabilities);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0) {
		DRV_LOG(ERR,
			"Failed to execute command of VIRTCHNL2_OP_GET_CAPS");
		return err;
	}

	rte_memcpy(&adapter->caps, args.out_buffer, sizeof(struct virtchnl2_get_capabilities));

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_vport_create)
int
idpf_vc_vport_create(struct idpf_vport *vport,
		     struct virtchnl2_create_vport *create_vport_info)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_create_vport vport_msg;
	struct idpf_cmd_info args;
	int err = -1;

	memset(&vport_msg, 0, sizeof(struct virtchnl2_create_vport));
	vport_msg.vport_type = create_vport_info->vport_type;
	vport_msg.txq_model = create_vport_info->txq_model;
	vport_msg.rxq_model = create_vport_info->rxq_model;
	vport_msg.num_tx_q = create_vport_info->num_tx_q;
	vport_msg.num_tx_complq = create_vport_info->num_tx_complq;
	vport_msg.num_rx_q = create_vport_info->num_rx_q;
	vport_msg.num_rx_bufq = create_vport_info->num_rx_bufq;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_CREATE_VPORT;
	args.in_args = (uint8_t *)&vport_msg;
	args.in_args_size = sizeof(vport_msg);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0) {
		DRV_LOG(ERR,
			"Failed to execute command of VIRTCHNL2_OP_CREATE_VPORT");
		return err;
	}

	rte_memcpy(&(vport->vport_info.info), args.out_buffer, IDPF_DFLT_MBX_BUF_SIZE);
	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_vport_destroy)
int
idpf_vc_vport_destroy(struct idpf_vport *vport)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_vport vc_vport;
	struct idpf_cmd_info args;
	int err;

	vc_vport.vport_id = vport->vport_id;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_DESTROY_VPORT;
	args.in_args = (uint8_t *)&vc_vport;
	args.in_args_size = sizeof(vc_vport);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_DESTROY_VPORT");

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_queue_grps_add)
int
idpf_vc_queue_grps_add(struct idpf_vport *vport,
		       struct virtchnl2_add_queue_groups *p2p_queue_grps_info,
		       uint8_t *p2p_queue_grps_out)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct idpf_cmd_info args;
	int size;
	int err = -1;

	size = sizeof(*p2p_queue_grps_info) +
	       (p2p_queue_grps_info->num_queue_groups - 1) *
		   sizeof(struct virtchnl2_queue_group_info);

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_ADD_QUEUE_GROUPS;
	args.in_args = (uint8_t *)p2p_queue_grps_info;
	args.in_args_size = size;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0) {
		DRV_LOG(ERR,
			"Failed to execute command of VIRTCHNL2_OP_ADD_QUEUE_GROUPS");
		return err;
	}

	rte_memcpy(p2p_queue_grps_out, args.out_buffer, IDPF_DFLT_MBX_BUF_SIZE);
	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_queue_grps_del)
int idpf_vc_queue_grps_del(struct idpf_vport *vport,
			  uint16_t num_q_grps,
			  struct virtchnl2_queue_group_id *qg_ids)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_delete_queue_groups *vc_del_q_grps;
	struct idpf_cmd_info args;
	int size;
	int err;

	size = sizeof(*vc_del_q_grps) +
	       (num_q_grps - 1) * sizeof(struct virtchnl2_queue_group_id);
	vc_del_q_grps = rte_zmalloc("vc_del_q_grps", size, 0);

	vc_del_q_grps->vport_id = vport->vport_id;
	vc_del_q_grps->num_queue_groups = num_q_grps;
	memcpy(vc_del_q_grps->qg_ids, qg_ids,
	       num_q_grps * sizeof(struct virtchnl2_queue_group_id));

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_DEL_QUEUE_GROUPS;
	args.in_args = (uint8_t *)vc_del_q_grps;
	args.in_args_size = size;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_DEL_QUEUE_GROUPS");

	rte_free(vc_del_q_grps);
	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_rss_key_set)
int
idpf_vc_rss_key_set(struct idpf_vport *vport)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_rss_key *rss_key;
	struct idpf_cmd_info args;
	int len, err;

	len = sizeof(*rss_key) + sizeof(rss_key->key[0]) *
		(vport->rss_key_size - 1);
	rss_key = rte_zmalloc("rss_key", len, 0);
	if (rss_key == NULL)
		return -ENOMEM;

	rss_key->vport_id = vport->vport_id;
	rss_key->key_len = vport->rss_key_size;
	rte_memcpy(rss_key->key, vport->rss_key,
		   sizeof(rss_key->key[0]) * vport->rss_key_size);

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_SET_RSS_KEY;
	args.in_args = (uint8_t *)rss_key;
	args.in_args_size = len;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_SET_RSS_KEY");

	rte_free(rss_key);
	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_rss_key_get)
int idpf_vc_rss_key_get(struct idpf_vport *vport)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_rss_key *rss_key_ret;
	struct virtchnl2_rss_key rss_key;
	struct idpf_cmd_info args;
	int err;

	memset(&rss_key, 0, sizeof(rss_key));
	rss_key.vport_id = vport->vport_id;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_GET_RSS_KEY;
	args.in_args = (uint8_t *)&rss_key;
	args.in_args_size = sizeof(rss_key);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);

	if (!err) {
		rss_key_ret = (struct virtchnl2_rss_key *)args.out_buffer;
		if (rss_key_ret->key_len != vport->rss_key_size) {
			rte_free(vport->rss_key);
			vport->rss_key = NULL;
			vport->rss_key_size = RTE_MIN(IDPF_RSS_KEY_LEN,
						      rss_key_ret->key_len);
			vport->rss_key = rte_zmalloc("rss_key", vport->rss_key_size, 0);
			if (!vport->rss_key) {
				vport->rss_key_size = 0;
				DRV_LOG(ERR, "Failed to allocate RSS key");
				return -ENOMEM;
			}
		}
		rte_memcpy(vport->rss_key, rss_key_ret->key, vport->rss_key_size);
	} else {
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_GET_RSS_KEY");
	}

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_rss_lut_set)
int
idpf_vc_rss_lut_set(struct idpf_vport *vport)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_rss_lut *rss_lut;
	struct idpf_cmd_info args;
	int len, err;

	len = sizeof(*rss_lut) + sizeof(rss_lut->lut[0]) *
		(vport->rss_lut_size - 1);
	rss_lut = rte_zmalloc("rss_lut", len, 0);
	if (rss_lut == NULL)
		return -ENOMEM;

	rss_lut->vport_id = vport->vport_id;
	rss_lut->lut_entries = vport->rss_lut_size;
	rte_memcpy(rss_lut->lut, vport->rss_lut,
		   sizeof(rss_lut->lut[0]) * vport->rss_lut_size);

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_SET_RSS_LUT;
	args.in_args = (uint8_t *)rss_lut;
	args.in_args_size = len;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_SET_RSS_LUT");

	rte_free(rss_lut);
	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_rss_lut_get)
int
idpf_vc_rss_lut_get(struct idpf_vport *vport)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_rss_lut *rss_lut_ret;
	struct virtchnl2_rss_lut rss_lut;
	struct idpf_cmd_info args;
	int err;

	memset(&rss_lut, 0, sizeof(rss_lut));
	rss_lut.vport_id = vport->vport_id;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_GET_RSS_LUT;
	args.in_args = (uint8_t *)&rss_lut;
	args.in_args_size = sizeof(rss_lut);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);

	if (!err) {
		rss_lut_ret = (struct virtchnl2_rss_lut *)args.out_buffer;
		if (rss_lut_ret->lut_entries != vport->rss_lut_size) {
			rte_free(vport->rss_lut);
			vport->rss_lut = NULL;
			vport->rss_lut = rte_zmalloc("rss_lut",
				     sizeof(uint32_t) * rss_lut_ret->lut_entries, 0);
			if (vport->rss_lut == NULL) {
				DRV_LOG(ERR, "Failed to allocate RSS lut");
				return -ENOMEM;
			}
		}
		rte_memcpy(vport->rss_lut, rss_lut_ret->lut, rss_lut_ret->lut_entries);
		vport->rss_lut_size = rss_lut_ret->lut_entries;
	} else {
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_GET_RSS_LUT");
	}

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_rss_hash_get)
int
idpf_vc_rss_hash_get(struct idpf_vport *vport)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_rss_hash *rss_hash_ret;
	struct virtchnl2_rss_hash rss_hash;
	struct idpf_cmd_info args;
	int err;

	memset(&rss_hash, 0, sizeof(rss_hash));
	rss_hash.ptype_groups = vport->rss_hf;
	rss_hash.vport_id = vport->vport_id;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_GET_RSS_HASH;
	args.in_args = (uint8_t *)&rss_hash;
	args.in_args_size = sizeof(rss_hash);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);

	if (!err) {
		rss_hash_ret = (struct virtchnl2_rss_hash *)args.out_buffer;
		vport->rss_hf = rss_hash_ret->ptype_groups;
	} else {
		DRV_LOG(ERR, "Failed to execute command of OP_GET_RSS_HASH");
	}

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_rss_hash_set)
int
idpf_vc_rss_hash_set(struct idpf_vport *vport)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_rss_hash rss_hash;
	struct idpf_cmd_info args;
	int err;

	memset(&rss_hash, 0, sizeof(rss_hash));
	rss_hash.ptype_groups = vport->rss_hf;
	rss_hash.vport_id = vport->vport_id;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_SET_RSS_HASH;
	args.in_args = (uint8_t *)&rss_hash;
	args.in_args_size = sizeof(rss_hash);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of OP_SET_RSS_HASH");

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_irq_map_unmap_config)
int
idpf_vc_irq_map_unmap_config(struct idpf_vport *vport, uint16_t nb_rxq, bool map)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_queue_vector_maps *map_info;
	struct virtchnl2_queue_vector *vecmap;
	struct idpf_cmd_info args;
	int len, i, err = 0;

	len = sizeof(struct virtchnl2_queue_vector_maps) +
		(nb_rxq - 1) * sizeof(struct virtchnl2_queue_vector);

	map_info = rte_zmalloc("map_info", len, 0);
	if (map_info == NULL)
		return -ENOMEM;

	map_info->vport_id = vport->vport_id;
	map_info->num_qv_maps = nb_rxq;
	for (i = 0; i < nb_rxq; i++) {
		vecmap = &map_info->qv_maps[i];
		vecmap->queue_id = vport->qv_map[i].queue_id;
		vecmap->vector_id = vport->qv_map[i].vector_id;
		vecmap->itr_idx = VIRTCHNL2_ITR_IDX_0;
		vecmap->queue_type = VIRTCHNL2_QUEUE_TYPE_RX;
	}

	args.ops = map ? VIRTCHNL2_OP_MAP_QUEUE_VECTOR :
		VIRTCHNL2_OP_UNMAP_QUEUE_VECTOR;
	args.in_args = (uint8_t *)map_info;
	args.in_args_size = len;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;
	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_%s_QUEUE_VECTOR",
			map ? "MAP" : "UNMAP");

	rte_free(map_info);
	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_vectors_alloc)
int
idpf_vc_vectors_alloc(struct idpf_vport *vport, uint16_t num_vectors)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_alloc_vectors *alloc_vec;
	struct idpf_cmd_info args;
	int err, len;

	len = sizeof(struct virtchnl2_alloc_vectors) +
		(num_vectors - 1) * sizeof(struct virtchnl2_vector_chunk);
	alloc_vec = rte_zmalloc("alloc_vec", len, 0);
	if (alloc_vec == NULL)
		return -ENOMEM;

	alloc_vec->num_vectors = num_vectors;

	args.ops = VIRTCHNL2_OP_ALLOC_VECTORS;
	args.in_args = (uint8_t *)alloc_vec;
	args.in_args_size = len;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;
	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command VIRTCHNL2_OP_ALLOC_VECTORS");

	rte_memcpy(vport->recv_vectors, args.out_buffer, len);
	rte_free(alloc_vec);
	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_vectors_dealloc)
int
idpf_vc_vectors_dealloc(struct idpf_vport *vport)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_alloc_vectors *alloc_vec;
	struct virtchnl2_vector_chunks *vcs;
	struct idpf_cmd_info args;
	int err, len;

	alloc_vec = vport->recv_vectors;
	vcs = &alloc_vec->vchunks;

	len = sizeof(struct virtchnl2_vector_chunks) +
		(vcs->num_vchunks - 1) * sizeof(struct virtchnl2_vector_chunk);

	args.ops = VIRTCHNL2_OP_DEALLOC_VECTORS;
	args.in_args = (uint8_t *)vcs;
	args.in_args_size = len;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;
	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command VIRTCHNL2_OP_DEALLOC_VECTORS");

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_ena_dis_one_queue)
int
idpf_vc_ena_dis_one_queue(struct idpf_vport *vport, uint16_t qid,
			  uint32_t type, bool on)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_del_ena_dis_queues *queue_select;
	struct virtchnl2_queue_chunk *queue_chunk;
	struct idpf_cmd_info args;
	int err, len;

	len = sizeof(struct virtchnl2_del_ena_dis_queues);
	queue_select = rte_zmalloc("queue_select", len, 0);
	if (queue_select == NULL)
		return -ENOMEM;

	queue_chunk = queue_select->chunks.chunks;
	queue_select->chunks.num_chunks = 1;
	queue_select->vport_id = vport->vport_id;

	queue_chunk->type = type;
	queue_chunk->start_queue_id = qid;
	queue_chunk->num_queues = 1;

	args.ops = on ? VIRTCHNL2_OP_ENABLE_QUEUES :
		VIRTCHNL2_OP_DISABLE_QUEUES;
	args.in_args = (uint8_t *)queue_select;
	args.in_args_size = len;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;
	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_%s_QUEUES",
			on ? "ENABLE" : "DISABLE");

	rte_free(queue_select);
	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_ena_dis_one_queue_vcpf)
int
idpf_vc_ena_dis_one_queue_vcpf(struct idpf_adapter *adapter, uint16_t qid,
			  uint32_t type, bool on)
{
	struct virtchnl2_del_ena_dis_queues *queue_select;
	struct virtchnl2_queue_chunk *queue_chunk;
	struct idpf_cmd_info args;
	int err, len;

	len = sizeof(struct virtchnl2_del_ena_dis_queues);
	queue_select = rte_zmalloc("queue_select", len, 0);
	if (queue_select == NULL)
		return -ENOMEM;

	queue_chunk = queue_select->chunks.chunks;
	queue_select->chunks.num_chunks = 1;
	queue_select->vport_id = rte_cpu_to_le_32(VCPF_CFGQ_VPORT_ID);

	queue_chunk->type = type;
	queue_chunk->start_queue_id = qid;
	queue_chunk->num_queues = 1;

	args.ops = on ? VIRTCHNL2_OP_ENABLE_QUEUES :
		VIRTCHNL2_OP_DISABLE_QUEUES;
	args.in_args = (uint8_t *)queue_select;
	args.in_args_size = len;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;
	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_%s_QUEUES",
			on ? "ENABLE" : "DISABLE");

	rte_free(queue_select);
	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_queue_switch)
int
idpf_vc_queue_switch(struct idpf_vport *vport, uint16_t qid,
		     bool rx, bool on, uint32_t type)
{
	int err, queue_id;

	if (rx)
		queue_id = vport->chunks_info.rx_start_qid + qid;
	else
		queue_id = vport->chunks_info.tx_start_qid + qid;
	err = idpf_vc_ena_dis_one_queue(vport, queue_id, type, on);
	if (err != 0)
		return err;

	/* switch tx completion queue */
	if (!rx && vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		type = VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION;
		queue_id = vport->chunks_info.tx_compl_start_qid + qid;
		err = idpf_vc_ena_dis_one_queue(vport, queue_id, type, on);
		if (err != 0)
			return err;
	}

	/* switch rx buffer queue */
	if (rx && vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		type = VIRTCHNL2_QUEUE_TYPE_RX_BUFFER;
		queue_id = vport->chunks_info.rx_buf_start_qid + 2 * qid;
		err = idpf_vc_ena_dis_one_queue(vport, queue_id, type, on);
		if (err != 0)
			return err;
		queue_id++;
		err = idpf_vc_ena_dis_one_queue(vport, queue_id, type, on);
		if (err != 0)
			return err;
	}

	return err;
}

#define IDPF_RXTX_QUEUE_CHUNKS_NUM	2
RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_queues_ena_dis)
int
idpf_vc_queues_ena_dis(struct idpf_vport *vport, bool enable)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_del_ena_dis_queues *queue_select;
	struct virtchnl2_queue_chunk *queue_chunk;
	uint32_t type;
	struct idpf_cmd_info args;
	uint16_t num_chunks;
	int err, len;

	num_chunks = IDPF_RXTX_QUEUE_CHUNKS_NUM;
	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT)
		num_chunks++;
	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT)
		num_chunks++;

	len = sizeof(struct virtchnl2_del_ena_dis_queues) +
		sizeof(struct virtchnl2_queue_chunk) * (num_chunks - 1);
	queue_select = rte_zmalloc("queue_select", len, 0);
	if (queue_select == NULL)
		return -ENOMEM;

	queue_chunk = queue_select->chunks.chunks;
	queue_select->chunks.num_chunks = num_chunks;
	queue_select->vport_id = vport->vport_id;

	type = VIRTCHNL_QUEUE_TYPE_RX;
	queue_chunk[type].type = type;
	queue_chunk[type].start_queue_id = vport->chunks_info.rx_start_qid;
	queue_chunk[type].num_queues = vport->num_rx_q;

	type = VIRTCHNL2_QUEUE_TYPE_TX;
	queue_chunk[type].type = type;
	queue_chunk[type].start_queue_id = vport->chunks_info.tx_start_qid;
	queue_chunk[type].num_queues = vport->num_tx_q;

	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		type = VIRTCHNL2_QUEUE_TYPE_RX_BUFFER;
		queue_chunk[type].type = type;
		queue_chunk[type].start_queue_id =
			vport->chunks_info.rx_buf_start_qid;
		queue_chunk[type].num_queues = vport->num_rx_bufq;
	}

	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		type = VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION;
		queue_chunk[type].type = type;
		queue_chunk[type].start_queue_id =
			vport->chunks_info.tx_compl_start_qid;
		queue_chunk[type].num_queues = vport->num_tx_complq;
	}

	args.ops = enable ? VIRTCHNL2_OP_ENABLE_QUEUES :
		VIRTCHNL2_OP_DISABLE_QUEUES;
	args.in_args = (uint8_t *)queue_select;
	args.in_args_size = len;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;
	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_%s_QUEUES",
			enable ? "ENABLE" : "DISABLE");

	rte_free(queue_select);
	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_vport_ena_dis)
int
idpf_vc_vport_ena_dis(struct idpf_vport *vport, bool enable)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_vport vc_vport;
	struct idpf_cmd_info args;
	int err;

	vc_vport.vport_id = vport->vport_id;
	args.ops = enable ? VIRTCHNL2_OP_ENABLE_VPORT :
		VIRTCHNL2_OP_DISABLE_VPORT;
	args.in_args = (uint8_t *)&vc_vport;
	args.in_args_size = sizeof(vc_vport);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0) {
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_%s_VPORT",
			enable ? "ENABLE" : "DISABLE");
	}

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_ptype_info_query)
int
idpf_vc_ptype_info_query(struct idpf_adapter *adapter,
			 struct virtchnl2_get_ptype_info *req_ptype_info,
			 struct virtchnl2_get_ptype_info *recv_ptype_info)
{
	struct idpf_cmd_info args;
	int err;

	args.ops = VIRTCHNL2_OP_GET_PTYPE_INFO;
	args.in_args = (uint8_t *)req_ptype_info;
	args.in_args_size = sizeof(struct virtchnl2_get_ptype_info);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_GET_PTYPE_INFO");

	rte_memcpy(recv_ptype_info, args.out_buffer, IDPF_DFLT_MBX_BUF_SIZE);
	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_stats_query)
int
idpf_vc_stats_query(struct idpf_vport *vport,
		struct virtchnl2_vport_stats **pstats)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_vport_stats vport_stats;
	struct idpf_cmd_info args;
	int err;

	vport_stats.vport_id = vport->vport_id;
	args.ops = VIRTCHNL2_OP_GET_STATS;
	args.in_args = (u8 *)&vport_stats;
	args.in_args_size = sizeof(vport_stats);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	if (err) {
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_GET_STATS");
		*pstats = NULL;
		return err;
	}
	*pstats = (struct virtchnl2_vport_stats *)args.out_buffer;
	return 0;
}

#define IDPF_RX_BUF_STRIDE		64
RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_rxq_config)
int
idpf_vc_rxq_config(struct idpf_vport *vport, struct idpf_rx_queue *rxq)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_config_rx_queues *vc_rxqs = NULL;
	struct virtchnl2_rxq_info *rxq_info;
	struct idpf_cmd_info args;
	uint16_t num_qs;
	int size, err, i;

	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE)
		num_qs = IDPF_RXQ_PER_GRP;
	else
		num_qs = IDPF_RXQ_PER_GRP + IDPF_RX_BUFQ_PER_GRP;

	size = sizeof(*vc_rxqs) + (num_qs - 1) *
		sizeof(struct virtchnl2_rxq_info);
	vc_rxqs = rte_zmalloc("cfg_rxqs", size, 0);
	if (vc_rxqs == NULL) {
		DRV_LOG(ERR, "Failed to allocate virtchnl2_config_rx_queues");
		err = -ENOMEM;
		return err;
	}
	vc_rxqs->vport_id = vport->vport_id;
	vc_rxqs->num_qinfo = num_qs;
	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		rxq_info = &vc_rxqs->qinfo[0];
		rxq_info->dma_ring_addr = rxq->rx_ring_phys_addr;
		rxq_info->type = VIRTCHNL2_QUEUE_TYPE_RX;
		rxq_info->queue_id = rxq->queue_id;
		rxq_info->model = VIRTCHNL2_QUEUE_MODEL_SINGLE;
		rxq_info->data_buffer_size = rxq->rx_buf_len;
		rxq_info->max_pkt_size = vport->max_pkt_len;

		rxq_info->desc_ids = VIRTCHNL2_RXDID_2_FLEX_SQ_NIC_M;
		rxq_info->qflags |= VIRTCHNL2_RX_DESC_SIZE_32BYTE;

		rxq_info->ring_len = rxq->nb_rx_desc;
	}  else {
		/* Rx queue */
		rxq_info = &vc_rxqs->qinfo[0];
		rxq_info->dma_ring_addr = rxq->rx_ring_phys_addr;
		rxq_info->type = VIRTCHNL2_QUEUE_TYPE_RX;
		rxq_info->queue_id = rxq->queue_id;
		rxq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
		rxq_info->data_buffer_size = rxq->rx_buf_len;
		rxq_info->max_pkt_size = vport->max_pkt_len;

		rxq_info->desc_ids = VIRTCHNL2_RXDID_2_FLEX_SPLITQ_M;
		rxq_info->qflags |= VIRTCHNL2_RX_DESC_SIZE_32BYTE;

		rxq_info->ring_len = rxq->nb_rx_desc;
		rxq_info->rx_bufq1_id = rxq->bufq1->queue_id;
		rxq_info->bufq2_ena = 1;
		rxq_info->rx_bufq2_id = rxq->bufq2->queue_id;
		rxq_info->rx_buffer_low_watermark = 64;

		/* Buffer queue */
		for (i = 1; i <= IDPF_RX_BUFQ_PER_GRP; i++) {
			struct idpf_rx_queue *bufq = i == 1 ? rxq->bufq1 : rxq->bufq2;
			rxq_info = &vc_rxqs->qinfo[i];
			rxq_info->dma_ring_addr = bufq->rx_ring_phys_addr;
			rxq_info->type = VIRTCHNL2_QUEUE_TYPE_RX_BUFFER;
			rxq_info->queue_id = bufq->queue_id;
			rxq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
			rxq_info->data_buffer_size = bufq->rx_buf_len;
			rxq_info->desc_ids = VIRTCHNL2_RXDID_2_FLEX_SPLITQ_M;
			rxq_info->ring_len = bufq->nb_rx_desc;

			rxq_info->buffer_notif_stride = IDPF_RX_BUF_STRIDE;
			rxq_info->rx_buffer_low_watermark = 64;
		}
	}

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_CONFIG_RX_QUEUES;
	args.in_args = (uint8_t *)vc_rxqs;
	args.in_args_size = size;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	rte_free(vc_rxqs);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_RX_QUEUES");

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_rxq_config_by_info)
int idpf_vc_rxq_config_by_info(struct idpf_vport *vport, struct virtchnl2_rxq_info *rxq_info,
			       uint16_t num_qs)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_config_rx_queues *vc_rxqs = NULL;
	struct idpf_cmd_info args;
	int size, err;

	size = sizeof(*vc_rxqs) + (num_qs - 1) *
		sizeof(struct virtchnl2_rxq_info);
	vc_rxqs = rte_zmalloc("cfg_rxqs", size, 0);
	if (vc_rxqs == NULL) {
		DRV_LOG(ERR, "Failed to allocate virtchnl2_config_rx_queues");
		err = -ENOMEM;
		return err;
	}
	vc_rxqs->vport_id = vport->vport_id;
	vc_rxqs->num_qinfo = num_qs;
	memcpy(vc_rxqs->qinfo, rxq_info, num_qs * sizeof(struct virtchnl2_rxq_info));

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_CONFIG_RX_QUEUES;
	args.in_args = (uint8_t *)vc_rxqs;
	args.in_args_size = size;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	rte_free(vc_rxqs);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_RX_QUEUES");

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_txq_config)
int
idpf_vc_txq_config(struct idpf_vport *vport, struct ci_tx_queue *txq)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_config_tx_queues *vc_txqs = NULL;
	struct virtchnl2_txq_info *txq_info;
	struct idpf_cmd_info args;
	uint16_t num_qs;
	int size, err;

	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE)
		num_qs = IDPF_TXQ_PER_GRP;
	else
		num_qs = IDPF_TXQ_PER_GRP + IDPF_TX_COMPLQ_PER_GRP;

	size = sizeof(*vc_txqs) + (num_qs - 1) *
		sizeof(struct virtchnl2_txq_info);
	vc_txqs = rte_zmalloc("cfg_txqs", size, 0);
	if (vc_txqs == NULL) {
		DRV_LOG(ERR, "Failed to allocate virtchnl2_config_tx_queues");
		err = -ENOMEM;
		return err;
	}
	vc_txqs->vport_id = vport->vport_id;
	vc_txqs->num_qinfo = num_qs;

	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		txq_info = &vc_txqs->qinfo[0];
		txq_info->dma_ring_addr = txq->tx_ring_dma;
		txq_info->type = VIRTCHNL2_QUEUE_TYPE_TX;
		txq_info->queue_id = txq->queue_id;
		txq_info->model = VIRTCHNL2_QUEUE_MODEL_SINGLE;
		txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_QUEUE;
		txq_info->ring_len = txq->nb_tx_desc;
	} else {
		/* txq info */
		txq_info = &vc_txqs->qinfo[0];
		txq_info->dma_ring_addr = txq->tx_ring_dma;
		txq_info->type = VIRTCHNL2_QUEUE_TYPE_TX;
		txq_info->queue_id = txq->queue_id;
		txq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
		txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_FLOW;
		txq_info->ring_len = txq->nb_tx_desc;
		txq_info->tx_compl_queue_id = txq->complq->queue_id;
		txq_info->relative_queue_id = txq_info->queue_id;

		/* tx completion queue info */
		txq_info = &vc_txqs->qinfo[1];
		txq_info->dma_ring_addr = txq->complq->tx_ring_dma;
		txq_info->type = VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION;
		txq_info->queue_id = txq->complq->queue_id;
		txq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
		txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_FLOW;
		txq_info->ring_len = txq->complq->nb_tx_desc;
	}

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_CONFIG_TX_QUEUES;
	args.in_args = (uint8_t *)vc_txqs;
	args.in_args_size = size;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	rte_free(vc_txqs);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_TX_QUEUES");

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_txq_config_by_info)
int
idpf_vc_txq_config_by_info(struct idpf_vport *vport, struct virtchnl2_txq_info *txq_info,
		       uint16_t num_qs)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_config_tx_queues *vc_txqs = NULL;
	struct idpf_cmd_info args;
	int size, err;

	size = sizeof(*vc_txqs) + (num_qs - 1) * sizeof(struct virtchnl2_txq_info);
	vc_txqs = rte_zmalloc("cfg_txqs", size, 0);
	if (vc_txqs == NULL) {
		DRV_LOG(ERR, "Failed to allocate virtchnl2_config_tx_queues");
		err = -ENOMEM;
		return err;
	}
	vc_txqs->vport_id = vport->vport_id;
	vc_txqs->num_qinfo = num_qs;
	memcpy(vc_txqs->qinfo, txq_info, num_qs * sizeof(struct virtchnl2_txq_info));

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_CONFIG_TX_QUEUES;
	args.in_args = (uint8_t *)vc_txqs;
	args.in_args_size = size;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(adapter, &args);
	rte_free(vc_txqs);
	if (err != 0)
		DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_TX_QUEUES");

	return err;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_ctlq_recv)
int
idpf_vc_ctlq_recv(struct idpf_ctlq_info *cq, u16 *num_q_msg,
		  struct idpf_ctlq_msg *q_msg)
{
	return idpf_ctlq_recv(cq, num_q_msg, q_msg);
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vc_ctlq_post_rx_buffs)
int
idpf_vc_ctlq_post_rx_buffs(struct idpf_hw *hw, struct idpf_ctlq_info *cq,
			   u16 *buff_count, struct idpf_dma_mem **buffs)
{
	return idpf_ctlq_post_rx_buffs(hw, cq, buff_count, buffs);
}
