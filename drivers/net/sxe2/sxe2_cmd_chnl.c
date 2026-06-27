/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include "sxe2_ioctl_chnl_func.h"
#include "sxe2_drv_cmd.h"
#include "sxe2_cmd_chnl.h"
#include "sxe2_ethdev.h"
#include "sxe2_common_log.h"

static union sxe2_drv_trace_info sxe2_drv_trace_id;

static void sxe2_drv_trace_id_alloc(uint64_t *trace_id)
{
	union sxe2_drv_trace_info *trace = NULL;
	uint64_t trace_id_count = 0;

	trace = &sxe2_drv_trace_id;

	trace_id_count = trace->sxe2_drv_trace_id_param.count;
	++trace_id_count;
	trace->sxe2_drv_trace_id_param.count =
			(trace_id_count & SXE2_DRV_TRACE_ID_COUNT_MASK);

	*trace_id = trace->id;
}

static void __sxe2_drv_cmd_params_fill(struct sxe2_adapter *adapter,
		struct sxe2_drv_cmd_params *cmd, uint32_t opc, const char *opc_str,
		void *in_data, uint32_t in_len, void *out_data, uint32_t out_len)
{
	PMD_DEV_LOG_DEBUG(adapter, DRV, "cmd opcode:%s", opc_str);
	cmd->timeout = SXE2_DRV_CMD_DFLT_TIMEOUT;
	cmd->opcode  = opc;
	cmd->vsi_id  = adapter->vsi_ctxt.dpdk_vsi_id;
	cmd->repr_id = (adapter->repr_priv_data != NULL) ?
			adapter->repr_priv_data->repr_id : 0xFFFF;
	cmd->req_len = in_len;
	cmd->req_data = in_data;
	cmd->resp_len = out_len;
	cmd->resp_data = out_data;

	sxe2_drv_trace_id_alloc(&cmd->trace_id);
}

#define sxe2_drv_cmd_params_fill(adapter, cmd, opc, in_data, in_len, out_data, out_len) \
	__sxe2_drv_cmd_params_fill(adapter, cmd, opc, #opc, in_data, in_len, out_data, out_len)


int32_t sxe2_drv_dev_caps_get(struct sxe2_adapter *adapter, struct sxe2_drv_dev_caps_resp *dev_caps)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_DEV_GET_CAPS,
			NULL, 0, dev_caps,
			sizeof(struct sxe2_drv_dev_caps_resp));

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "get dev caps failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_dev_info_get(struct sxe2_adapter *adapter,
				struct sxe2_drv_dev_info_resp *dev_info_resp)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_DEV_GET_INFO,
			NULL, 0, dev_info_resp,
			sizeof(struct sxe2_drv_dev_info_resp));

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "get dev info failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_dev_fw_info_get(struct sxe2_adapter *adapter,
				struct sxe2_drv_dev_fw_info_resp *dev_fw_info_resp)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_DEV_GET_FW_INFO,
			NULL, 0, dev_fw_info_resp,
			sizeof(struct sxe2_drv_dev_fw_info_resp));

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "get dev fw info failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_vsi_add(struct sxe2_adapter *adapter, struct sxe2_vsi *vsi)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_vsi_create_req_resp vsi_req = {0};
	struct sxe2_drv_vsi_create_req_resp vsi_resp = {0};

	vsi_req.vsi_id = vsi->vsi_id;

	vsi_req.used_queues.queues_cnt = RTE_MIN(vsi->txqs.q_cnt, vsi->rxqs.q_cnt);
	vsi_req.used_queues.base_idx_in_pf = vsi->txqs.base_idx_in_func;
	vsi_req.used_msix.msix_vectors_cnt = vsi->irqs.avail_cnt;
	vsi_req.used_msix.base_idx_in_func = vsi->irqs.base_idx_in_pf;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_VSI_CREATE,
			&vsi_req,  sizeof(struct sxe2_drv_vsi_create_req_resp),
			&vsi_resp, sizeof(struct sxe2_drv_vsi_create_req_resp));

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "dev add vsi failed, ret=%d", ret);
		goto l_end;
	}

	vsi->vsi_id = vsi_resp.vsi_id;
	vsi->vsi_type = vsi_resp.vsi_type;

l_end:
	return ret;
}

int32_t sxe2_drv_vsi_del(struct sxe2_adapter *adapter, struct sxe2_vsi *vsi)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_vsi_free_req vsi_req = {0};

	vsi_req.vsi_id = vsi->vsi_id;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_VSI_FREE,
				&vsi_req, sizeof(struct sxe2_drv_vsi_free_req),
				NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "dev del vsi failed, ret=%d", ret);

	return ret;
}

#define SXE2_RXQ_CTXT_CFG_BUF_LEN_ALIGN  (1 << 7)
#define SXE2_RX_HDR_SIZE 256

static int32_t sxe2_rxq_ctxt_cfg_fill(struct sxe2_rx_queue *rxq,
		struct sxe2_drv_rxq_cfg_req *req, uint16_t rxq_cnt)
{
	struct sxe2_adapter *adapter = rxq->vsi->adapter;
	struct sxe2_drv_rxq_ctxt *ctxt = req->cfg;
	struct rte_eth_dev_data *dev_data = adapter->dev_info.dev_data;
	int32_t ret = 0;

	req->vsi_id = adapter->vsi_ctxt.main_vsi->vsi_id;
	req->q_cnt = rxq_cnt;
	req->max_frame_size = dev_data->mtu + SXE2_ETH_OVERHEAD;

	ctxt->queue_id = rxq->queue_id;
	ctxt->depth = rxq->ring_depth;
	ctxt->buf_len = RTE_ALIGN(rxq->rx_buf_len, SXE2_RXQ_CTXT_CFG_BUF_LEN_ALIGN);
	ctxt->dma_addr = rxq->base_addr;

	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) {
		ctxt->lro_en = 1;
		ctxt->max_lro_size = dev_data->dev_conf.rxmode.max_lro_pkt_size;
	} else {
		ctxt->lro_en = 0;
	}

	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		ctxt->keep_crc_en = 1;
	else
		ctxt->keep_crc_en = 0;

	ctxt->desc_size = sizeof(union sxe2_rx_desc);
	return ret;
}

int32_t sxe2_drv_rxq_ctxt_cfg(struct sxe2_adapter *adapter,
			      struct sxe2_rx_queue *rxq,
			      uint16_t rxq_cnt)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_rxq_cfg_req *req = NULL;
	uint16_t len = 0;

	len = sizeof(*req) + rxq_cnt * sizeof(struct sxe2_drv_rxq_ctxt);
	req = rte_zmalloc("sxe2_rxq_cfg", len, 0);
	if (req == NULL) {
		PMD_LOG_ERR(RX, "rxq cfg mem alloc failed");
		ret = -ENOMEM;
		goto l_end;
	}

	ret = sxe2_rxq_ctxt_cfg_fill(rxq, req, rxq_cnt);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "rxq cfg failed, ret=%d", ret);
		ret = -EINVAL;
		goto l_end;
	}

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_RXQ_CFG_ENABLE,
			req, len, NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "rxq cfg failed, ret=%d", ret);

l_end:
	if (req)
		rte_free(req);
	return ret;
}

static void sxe2_txq_ctxt_cfg_fill(struct sxe2_tx_queue *txq,
				   struct sxe2_drv_txq_cfg_req *req,
				   uint16_t txq_cnt)
{
	struct sxe2_drv_txq_ctxt *ctxt = req->cfg;
	uint16_t q_idx = 0;

	req->vsi_id = txq->vsi->vsi_id;
	req->q_cnt = txq_cnt;

	for (q_idx = 0; q_idx < txq_cnt; q_idx++) {
		ctxt = &req->cfg[q_idx];
		ctxt->depth = txq[q_idx].ring_depth;
		ctxt->dma_addr = txq[q_idx].base_addr;
		ctxt->queue_id = txq[q_idx].queue_id;
	}
}

int32_t sxe2_drv_txq_ctxt_cfg(struct sxe2_adapter *adapter,
			      struct sxe2_tx_queue *txq,
			      uint16_t txq_cnt)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_txq_cfg_req *req;
	uint16_t len = 0;

	len = sizeof(*req) + txq_cnt * sizeof(struct sxe2_drv_txq_ctxt);
	req = rte_zmalloc("sxe2_txq_cfg", len, 0);
	if (req == NULL) {
		PMD_LOG_ERR(TX, "txq cfg mem alloc failed");
		ret = -ENOMEM;
		goto l_end;
	}

	sxe2_txq_ctxt_cfg_fill(txq, req, txq_cnt);

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_TXQ_CFG_ENABLE,
			req, len, NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "txq cfg failed, ret=%d", ret);

l_end:
	if (req)
		rte_free(req);
	return ret;
}

int32_t sxe2_drv_rxq_switch(struct sxe2_adapter *adapter, struct sxe2_rx_queue *rxq, bool enable)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_q_switch_req req;

	req.vsi_id = rte_cpu_to_le_16(rxq->vsi->vsi_id);
	req.q_idx = rxq->queue_id;

	req.is_enable  = (uint8_t)enable;
	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_RXQ_DISABLE,
			&req, sizeof(req), NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "rxq switch failed, enable: %d, ret:%d",
			enable, ret);

	return ret;
}

int32_t sxe2_drv_txq_switch(struct sxe2_adapter *adapter, struct sxe2_tx_queue *txq, bool enable)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_q_switch_req req;

	req.vsi_id = rte_cpu_to_le_16(txq->vsi->vsi_id);
	req.q_idx = txq->queue_id;

	req.is_enable  = (uint8_t)enable;
	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_TXQ_DISABLE,
			&req, sizeof(req), NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "txq switch failed, enable: %d, ret:%d",
				enable, ret);
	}

	return ret;
}

int32_t sxe2_drv_mac_link_status_get(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_link_info_resp resp = {0};

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_LINK_STATUS_GET,
				 NULL, 0,
				 &resp, sizeof(resp));
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "link status get failed, ret=%d", ret);
		goto l_end;
	}
	adapter->link_ctxt.speed = resp.speed;
	adapter->link_ctxt.link_up = resp.status;

l_end:
	return ret;
}
