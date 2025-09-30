/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include "cpfl_ethdev.h"
#include <idpf_common_virtchnl.h>

int
cpfl_cc_vport_list_get(struct cpfl_adapter_ext *adapter,
		       struct cpfl_vport_id *vi,
		       struct cpchnl2_get_vport_list_response *response)
{
	struct cpchnl2_get_vport_list_request request;
	struct idpf_cmd_info args;
	int err;

	memset(&request, 0, sizeof(request));
	request.func_type = vi->func_type;
	request.pf_id = vi->pf_id;
	request.vf_id = vi->vf_id;

	memset(&args, 0, sizeof(args));
	args.ops = CPCHNL2_OP_GET_VPORT_LIST;
	args.in_args = (uint8_t *)&request;
	args.in_args_size = sizeof(struct cpchnl2_get_vport_list_request);
	args.out_buffer = adapter->base.mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(&adapter->base, &args);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to execute command of CPCHNL2_OP_GET_VPORT_LIST");
		return err;
	}

	rte_memcpy(response, args.out_buffer, IDPF_DFLT_MBX_BUF_SIZE);

	return 0;
}

int
cpfl_cc_vport_info_get(struct cpfl_adapter_ext *adapter,
		       struct cpchnl2_vport_id *vport_id,
		       struct cpfl_vport_id *vi,
		       struct cpchnl2_get_vport_info_response *response)
{
	struct cpchnl2_get_vport_info_request request;
	struct idpf_cmd_info args;
	int err;

	request.vport.vport_id = vport_id->vport_id;
	request.vport.vport_type = vport_id->vport_type;
	request.func.func_type = vi->func_type;
	request.func.pf_id = vi->pf_id;
	request.func.vf_id = vi->vf_id;

	memset(&args, 0, sizeof(args));
	args.ops = CPCHNL2_OP_GET_VPORT_INFO;
	args.in_args = (uint8_t *)&request;
	args.in_args_size = sizeof(struct cpchnl2_get_vport_info_request);
	args.out_buffer = adapter->base.mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(&adapter->base, &args);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to execute command of CPCHNL2_OP_GET_VPORT_INFO");
		return err;
	}

	rte_memcpy(response, args.out_buffer, sizeof(*response));

	return 0;
}

int
cpfl_vc_create_ctrl_vport(struct cpfl_adapter_ext *adapter)
{
	struct virtchnl2_create_vport vport_msg;
	struct idpf_cmd_info args;
	int err = -1;

	memset(&vport_msg, 0, sizeof(struct virtchnl2_create_vport));
	vport_msg.vport_type = rte_cpu_to_le_16(VIRTCHNL2_VPORT_TYPE_DEFAULT);
	vport_msg.txq_model = rte_cpu_to_le_16(VIRTCHNL2_QUEUE_MODEL_SINGLE);
	vport_msg.rxq_model = rte_cpu_to_le_16(VIRTCHNL2_QUEUE_MODEL_SINGLE);
	vport_msg.num_tx_q = CPFL_TX_CFGQ_NUM;
	vport_msg.num_tx_complq = 0;
	vport_msg.num_rx_q = CPFL_RX_CFGQ_NUM;
	vport_msg.num_rx_bufq = 0;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_CREATE_VPORT;
	args.in_args = (uint8_t *)&vport_msg;
	args.in_args_size = sizeof(vport_msg);
	args.out_buffer = adapter->base.mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(&adapter->base, &args);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of VIRTCHNL2_OP_CREATE_VPORT");
		return err;
	}

	memcpy(adapter->ctrl_vport_recv_info, args.out_buffer,
	       IDPF_DFLT_MBX_BUF_SIZE);
	return err;
}

#define VCPF_CFQ_MB_INDEX                0xFF
int
vcpf_add_queues(struct cpfl_adapter_ext *adapter)
{
	struct virtchnl2_add_queues add_cfgq;
	struct idpf_cmd_info args;
	int err;

	memset(&add_cfgq, 0, sizeof(struct virtchnl2_add_queues));
	u16 num_cfgq = 1;

	add_cfgq.num_tx_q = rte_cpu_to_le_16(num_cfgq);
	add_cfgq.num_rx_q = rte_cpu_to_le_16(num_cfgq);
	add_cfgq.mbx_q_index = VCPF_CFQ_MB_INDEX;

	add_cfgq.vport_id = rte_cpu_to_le_32(VCPF_CFGQ_VPORT_ID);
	add_cfgq.num_tx_complq = 0;
	add_cfgq.num_rx_bufq = 0;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_ADD_QUEUES;
	args.in_args = (uint8_t *)&add_cfgq;
	args.in_args_size = sizeof(add_cfgq);
	args.out_buffer = adapter->base.mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(&adapter->base, &args);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command VIRTCHNL2_OP_ADD_QUEUES");
		return err;
	}

	rte_memcpy(adapter->addq_recv_info, args.out_buffer, IDPF_DFLT_MBX_BUF_SIZE);

	return err;
}

int
vcpf_del_queues(struct cpfl_adapter_ext *adapter)
{
	struct virtchnl2_del_ena_dis_queues *del_cfgq;
	u16 num_chunks;
	struct idpf_cmd_info args;
	int i, err, size;

	num_chunks = adapter->cfgq_in.cfgq_add->chunks.num_chunks;
	size = idpf_struct_size(del_cfgq, chunks.chunks, (num_chunks - 1));
	del_cfgq = rte_zmalloc("del_cfgq", size, 0);
	if (!del_cfgq) {
		PMD_DRV_LOG(ERR, "Failed to allocate virtchnl2_del_ena_dis_queues");
		err = -ENOMEM;
		return err;
	}

	del_cfgq->vport_id = rte_cpu_to_le_32(VCPF_CFGQ_VPORT_ID);
	del_cfgq->chunks.num_chunks = num_chunks;

	/* fill config queue chunk data */
	for (i = 0; i < num_chunks; i++) {
		del_cfgq->chunks.chunks[i].type =
			adapter->cfgq_in.cfgq_add->chunks.chunks[i].type;
		del_cfgq->chunks.chunks[i].start_queue_id =
			adapter->cfgq_in.cfgq_add->chunks.chunks[i].start_queue_id;
		del_cfgq->chunks.chunks[i].num_queues =
			adapter->cfgq_in.cfgq_add->chunks.chunks[i].num_queues;
	}

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_DEL_QUEUES;
	args.in_args = (uint8_t *)del_cfgq;
	args.in_args_size = idpf_struct_size(del_cfgq, chunks.chunks,
						(del_cfgq->chunks.num_chunks - 1));
	args.out_buffer = adapter->base.mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(&adapter->base, &args);
	rte_free(del_cfgq);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command VIRTCHNL2_OP_DEL_QUEUES");
		return err;
	}

	if (adapter->cfgq_info) {
		rte_free(adapter->cfgq_info);
		adapter->cfgq_info = NULL;
	}
	adapter->cfgq_in.num_cfgq = 0;
	if (adapter->cfgq_in.cfgq_add) {
		rte_free(adapter->cfgq_in.cfgq_add);
		adapter->cfgq_in.cfgq_add = NULL;
	}
	if (adapter->cfgq_in.cfgq) {
		rte_free(adapter->cfgq_in.cfgq);
		adapter->cfgq_in.cfgq = NULL;
	}
	return err;
}

int
cpfl_config_ctlq_rx(struct cpfl_adapter_ext *adapter)
{
	struct cpfl_vport *vport = &adapter->ctrl_vport;
	struct virtchnl2_config_rx_queues *vc_rxqs = NULL;
	struct virtchnl2_rxq_info *rxq_info;
	struct idpf_cmd_info args;
	uint16_t num_qs;
	int size, err, i;

	if (adapter->base.hw.device_id != IXD_DEV_ID_VCPF) {
		if (vport->base.rxq_model != VIRTCHNL2_QUEUE_MODEL_SINGLE) {
			PMD_DRV_LOG(ERR, "This rxq model isn't supported.");
			err = -EINVAL;
			return err;
		}
	}

	num_qs = adapter->num_rx_cfgq;

	size = sizeof(*vc_rxqs) + (num_qs - 1) *
		sizeof(struct virtchnl2_rxq_info);
	vc_rxqs = rte_zmalloc("cfg_rxqs", size, 0);
	if (!vc_rxqs) {
		PMD_DRV_LOG(ERR, "Failed to allocate virtchnl2_config_rx_queues");
		err = -ENOMEM;
		return err;
	}

	if (adapter->base.hw.device_id == IXD_DEV_ID_VCPF)
		vc_rxqs->vport_id = rte_cpu_to_le_32(VCPF_CFGQ_VPORT_ID);
	else
		vc_rxqs->vport_id = vport->base.vport_id;

	vc_rxqs->num_qinfo = num_qs;

	for (i = 0; i < num_qs; i++) {
		rxq_info = &vc_rxqs->qinfo[i];
		rxq_info->dma_ring_addr = adapter->ctlqp[2 * i + 1]->desc_ring.pa;
		rxq_info->type = VIRTCHNL2_QUEUE_TYPE_CONFIG_RX;
		rxq_info->queue_id = adapter->cfgq_info[2 * i + 1].id;
		rxq_info->model = VIRTCHNL2_QUEUE_MODEL_SINGLE;
		rxq_info->data_buffer_size = adapter->cfgq_info[2 * i + 1].buf_size;
		if (adapter->base.hw.device_id != IXD_DEV_ID_VCPF)
			rxq_info->max_pkt_size = vport->base.max_pkt_len;
		rxq_info->desc_ids = VIRTCHNL2_RXDID_2_FLEX_SQ_NIC_M;
		rxq_info->qflags |= VIRTCHNL2_RX_DESC_SIZE_32BYTE;
		rxq_info->ring_len = adapter->cfgq_info[2 * i + 1].len;
	}

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_CONFIG_RX_QUEUES;
	args.in_args = (uint8_t *)vc_rxqs;
	args.in_args_size = size;
	args.out_buffer = adapter->base.mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(&adapter->base, &args);
	rte_free(vc_rxqs);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_RX_QUEUES");

	return err;
}

int
cpfl_config_ctlq_tx(struct cpfl_adapter_ext *adapter)
{
	struct cpfl_vport *vport = &adapter->ctrl_vport;
	struct virtchnl2_config_tx_queues *vc_txqs = NULL;
	struct virtchnl2_txq_info *txq_info;
	struct idpf_cmd_info args;
	uint16_t num_qs;
	int size, err, i;

	if (adapter->base.hw.device_id != IXD_DEV_ID_VCPF) {
		if (vport->base.txq_model != VIRTCHNL2_QUEUE_MODEL_SINGLE) {
			PMD_DRV_LOG(ERR, "This txq model isn't supported.");
			err = -EINVAL;
			return err;
		}
	}

	num_qs = adapter->num_tx_cfgq;

	size = sizeof(*vc_txqs) + (num_qs - 1) *
		sizeof(struct virtchnl2_txq_info);
	vc_txqs = rte_zmalloc("cfg_txqs", size, 0);
	if (!vc_txqs) {
		PMD_DRV_LOG(ERR, "Failed to allocate virtchnl2_config_tx_queues");
		err = -ENOMEM;
		return err;
	}

	if (adapter->base.hw.device_id == IXD_DEV_ID_VCPF)
		vc_txqs->vport_id = rte_cpu_to_le_32(VCPF_CFGQ_VPORT_ID);
	else
		vc_txqs->vport_id = vport->base.vport_id;

	vc_txqs->num_qinfo = num_qs;

	for (i = 0; i < num_qs; i++) {
		txq_info = &vc_txqs->qinfo[i];
		txq_info->dma_ring_addr = adapter->ctlqp[2 * i]->desc_ring.pa;
		txq_info->type = VIRTCHNL2_QUEUE_TYPE_CONFIG_TX;
		txq_info->queue_id = adapter->cfgq_info[2 * i].id;
		txq_info->model = VIRTCHNL2_QUEUE_MODEL_SINGLE;
		txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_QUEUE;
		txq_info->ring_len = adapter->cfgq_info[2 * i].len;
	}

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_CONFIG_TX_QUEUES;
	args.in_args = (uint8_t *)vc_txqs;
	args.in_args_size = size;
	args.out_buffer = adapter->base.mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_vc_cmd_execute(&adapter->base, &args);
	rte_free(vc_txqs);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_TX_QUEUES");

	return err;
}
