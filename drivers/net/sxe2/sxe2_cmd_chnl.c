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

int32_t sxe2_drv_switchdev_info_get(struct sxe2_adapter *adapter,
				    struct sxe2_switchdev_info *switchdev_info)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_DEV_GET_SWITCHDEV_INFO,
				 NULL, 0, switchdev_info,
				 sizeof(struct sxe2_switchdev_info));
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "get switchdev info failed, ret=%d", ret);

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

	if (adapter->is_dev_repr)
		ctxt->queue_id = adapter->repr_priv_data->repr_q_id;
	else
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
	struct sxe2_adapter *adapter = txq->vsi->adapter;
	struct sxe2_drv_txq_ctxt *ctxt = req->cfg;
	uint16_t q_idx = 0;

	req->vsi_id = txq->vsi->vsi_id;
	req->q_cnt = txq_cnt;

	for (q_idx = 0; q_idx < txq_cnt; q_idx++) {
		ctxt = &req->cfg[q_idx];
		ctxt->depth = txq[q_idx].ring_depth;
		ctxt->dma_addr = txq[q_idx].base_addr;
		if (adapter->is_dev_repr)
			ctxt->queue_id = adapter->repr_priv_data->repr_q_id;
		else
			ctxt->queue_id = txq[q_idx].queue_id;

		ctxt->sched_mode = sxe2_sched_mode_get(adapter);
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
	if (adapter->is_dev_repr)
		req.q_idx = adapter->repr_priv_data->repr_q_id;
	else
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
	if (adapter->is_dev_repr)
		req.q_idx = adapter->repr_priv_data->repr_q_id;
	else
		req.q_idx = txq->queue_id;

	req.is_enable  = (uint8_t)enable;
	req.sched_mode = sxe2_sched_mode_get(adapter);
	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_TXQ_DISABLE,
			&req, sizeof(req), NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "txq switch failed, enable: %d, ret:%d",
				enable, ret);
	}

	return ret;
}

int32_t sxe2_drv_vsi_info_get(struct sxe2_adapter *adapter, struct sxe2_vsi *vsi)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_vsi_info_get_req vsi_info_get_req = {0};
	struct sxe2_drv_vsi_info_get_resp vsi_info_get_resp = {0};

	vsi_info_get_req.vsi_id = vsi->vsi_id;
	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_VSI_INFO_GET,
			&vsi_info_get_req, sizeof(vsi_info_get_req),
			&vsi_info_get_resp, sizeof(vsi_info_get_resp));
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "switchdev cpvsi info get failed, ret=%d", ret);
		goto l_end;
	}

	vsi->txqs.q_cnt = vsi_info_get_resp.used_queues.queues_cnt;
	vsi->txqs.base_idx_in_func = vsi_info_get_resp.used_queues.base_idx_in_pf;

	vsi->rxqs.q_cnt = vsi_info_get_resp.used_queues.queues_cnt;
	vsi->rxqs.base_idx_in_func = vsi_info_get_resp.used_queues.base_idx_in_pf;

	vsi->irqs.avail_cnt = vsi_info_get_resp.used_msix.msix_vectors_cnt;
	vsi->irqs.base_idx_in_pf = vsi_info_get_resp.used_msix.base_idx_in_func;

l_end:
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

int32_t sxe2_drv_rxq_bind_irq(struct sxe2_adapter *adapter, uint16_t rxq_idx, uint16_t msix_idx)
{
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_queue_irq_bind_req req = {0};
	int32_t ret = 0;

	req.msix_idx = msix_idx;
	req.q_idx = rxq_idx;
	req.itr_idx = 0;
	req.bind = true;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_EVT_IRQ_BAND_RXQ,
				 &req, sizeof(req),
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "rxq bind irq failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_rxq_unbind_irq(struct sxe2_adapter *adapter, uint16_t rxq_idx)
{
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_queue_irq_bind_req req = {0};
	int32_t ret = 0;

	req.bind = false;
	req.q_idx = rxq_idx;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_EVT_IRQ_BAND_RXQ,
				 &req, sizeof(req),
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "rxq unbind irq failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_get_mac_stats(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	uint8_t i = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_vsi *vsi = adapter->vsi_ctxt.main_vsi;
	struct sxe2_stats *stats = &vsi->vsi_stats.vsi_hw_stats;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_mac_stats_resp resp = {0};

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_MAC_STATS_GET,
				 NULL, 0,
				 &resp, sizeof(resp));
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "vsi stats get failed, ret=%d", ret);
		goto l_end;
	}

	stats->rx_out_of_buffer        = rte_le_to_cpu_64(resp.rx_out_of_buffer);
	stats->rx_qblock_drop          = rte_le_to_cpu_64(resp.rx_qblock_drop);
	stats->tx_frame_good           = rte_le_to_cpu_64(resp.tx_frame_good);
	stats->rx_frame_good           = rte_le_to_cpu_64(resp.rx_frame_good);
	stats->rx_crc_errors           = rte_le_to_cpu_64(resp.rx_crc_errors);
	stats->tx_bytes_good           = rte_le_to_cpu_64(resp.tx_bytes_good);
	stats->rx_bytes_good           = rte_le_to_cpu_64(resp.rx_bytes_good);
	stats->tx_multicast_good       = rte_le_to_cpu_64(resp.tx_multicast_good);
	stats->tx_broadcast_good       = rte_le_to_cpu_64(resp.tx_broadcast_good);
	stats->rx_multicast_good       = rte_le_to_cpu_64(resp.rx_multicast_good);
	stats->rx_broadcast_good       = rte_le_to_cpu_64(resp.rx_broadcast_good);
	stats->rx_len_errors           = rte_le_to_cpu_64(resp.rx_len_errors);
	stats->rx_out_of_range_errors  = rte_le_to_cpu_64(resp.rx_out_of_range_errors);
	stats->rx_oversize_pkts_phy    = rte_le_to_cpu_64(resp.rx_oversize_pkts_phy);
	stats->rx_symbol_err	       = rte_le_to_cpu_64(resp.rx_symbol_err);
	stats->rx_pause_frame	       = rte_le_to_cpu_64(resp.rx_pause_frame);
	stats->tx_pause_frame	       = rte_le_to_cpu_64(resp.tx_pause_frame);
	stats->rx_discards_phy	       = rte_le_to_cpu_64(resp.rx_discards_phy);
	stats->rx_discards_ips_phy     = rte_le_to_cpu_64(resp.rx_discards_ips_phy);
	stats->tx_dropped_link_down    = rte_le_to_cpu_64(resp.tx_dropped_link_down);
	stats->rx_undersize_good       = rte_le_to_cpu_64(resp.rx_undersize_good);
	stats->rx_runt_error	       = rte_le_to_cpu_64(resp.rx_runt_error);
	stats->tx_bytes_good_bad       = rte_le_to_cpu_64(resp.tx_bytes_good_bad);
	stats->tx_frame_good_bad       = rte_le_to_cpu_64(resp.tx_frame_good_bad);
	stats->rx_jabbers              = rte_le_to_cpu_64(resp.rx_jabbers);
	stats->rx_size_64              = rte_le_to_cpu_64(resp.rx_size_64);
	stats->rx_size_65_127          = rte_le_to_cpu_64(resp.rx_size_65_127);
	stats->rx_size_128_255         = rte_le_to_cpu_64(resp.rx_size_128_255);
	stats->rx_size_256_511         = rte_le_to_cpu_64(resp.rx_size_256_511);
	stats->rx_size_512_1023        = rte_le_to_cpu_64(resp.rx_size_512_1023);
	stats->rx_size_1024_1522       = rte_le_to_cpu_64(resp.rx_size_1024_1522);
	stats->rx_size_1523_max        = rte_le_to_cpu_64(resp.rx_size_1523_max);
	stats->rx_pcs_symbol_err_phy   = rte_le_to_cpu_64(resp.rx_pcs_symbol_err_phy);
	stats->rx_corrected_bits_phy   = rte_le_to_cpu_64(resp.rx_corrected_bits_phy);
	stats->rx_err_lane_0_phy       = rte_le_to_cpu_64(resp.rx_err_lane_0_phy);
	stats->rx_err_lane_1_phy       = rte_le_to_cpu_64(resp.rx_err_lane_1_phy);
	stats->rx_err_lane_2_phy       = rte_le_to_cpu_64(resp.rx_err_lane_2_phy);
	stats->rx_err_lane_3_phy       = rte_le_to_cpu_64(resp.rx_err_lane_3_phy);
	stats->rx_illegal_bytes        = rte_le_to_cpu_64(resp.rx_illegal_bytes);
	stats->rx_oversize_good        = rte_le_to_cpu_64(resp.rx_oversize_good);
	stats->tx_unicast              = rte_le_to_cpu_64(resp.tx_unicast);
	stats->tx_broadcast            = rte_le_to_cpu_64(resp.tx_broadcast);
	stats->tx_multicast            = rte_le_to_cpu_64(resp.tx_multicast);
	stats->tx_vlan_packet_good     = rte_le_to_cpu_64(resp.tx_vlan_packet_good);
	stats->tx_size_64              = rte_le_to_cpu_64(resp.tx_size_64);
	stats->tx_size_65_127          = rte_le_to_cpu_64(resp.tx_size_65_127);
	stats->tx_size_128_255         = rte_le_to_cpu_64(resp.tx_size_128_255);
	stats->tx_size_256_511         = rte_le_to_cpu_64(resp.tx_size_256_511);
	stats->tx_size_512_1023        = rte_le_to_cpu_64(resp.tx_size_512_1023);
	stats->tx_size_1024_1522       = rte_le_to_cpu_64(resp.tx_size_1024_1522);
	stats->tx_size_1523_max        = rte_le_to_cpu_64(resp.tx_size_1523_max);
	stats->tx_underflow_error      = rte_le_to_cpu_64(resp.tx_underflow_error);
	stats->rx_byte_good_bad        = rte_le_to_cpu_64(resp.rx_byte_good_bad);
	stats->rx_frame_good_bad       = rte_le_to_cpu_64(resp.rx_frame_good_bad);
	stats->rx_unicast_good         = rte_le_to_cpu_64(resp.rx_unicast_good);
	stats->rx_vlan_packets         = rte_le_to_cpu_64(resp.rx_vlan_packets);

	for (i = 0; i < SXE2_MAX_USER_PRIORITY; i++) {
		stats->rx_prio_buf_discard[i]  =
			rte_le_to_cpu_64(resp.rx_prio_buf_discard[i]);
		stats->prio_xoff_rx[i]  =
			rte_le_to_cpu_64(resp.prio_xoff_rx[i]);
		stats->prio_xoff_tx[i]  =
			rte_le_to_cpu_64(resp.prio_xoff_tx[i]);
		stats->prio_xon_rx[i]  =
			rte_le_to_cpu_64(resp.prio_xon_rx[i]);
		stats->prio_xon_tx[i]  =
			rte_le_to_cpu_64(resp.prio_xon_tx[i]);
		stats->prio_xon_2_xoff[i]  =
			rte_le_to_cpu_64(resp.prio_xon_2_xoff[i]);
	}

l_end:
	return ret;
}

int32_t sxe2_drv_mac_stats_reset(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_MAC_STATS_CLEAR,
				 NULL, 0,
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "mac stats reset failed, ret=%d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

int32_t sxe2_drv_get_vsi_stats(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_vsi *vsi = adapter->vsi_ctxt.main_vsi;
	struct sxe2_stats *new_stats = &vsi->vsi_stats.vsi_hw_stats;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_vsi_stats_req req = {0};
	struct sxe2_drv_vsi_stats_resp resp = {0};

	req.vsi_id = rte_cpu_to_le_16(vsi->vsi_id);
	req.sw_stats.rx_bytes = rte_cpu_to_le_64(vsi->vsi_stats.stats.ibytes);
	req.sw_stats.rx_packets = rte_cpu_to_le_64(vsi->vsi_stats.stats.ipackets);
	req.sw_stats.tx_bytes = rte_cpu_to_le_64(vsi->vsi_stats.stats.obytes);
	req.sw_stats.tx_packets = rte_cpu_to_le_64(vsi->vsi_stats.stats.opackets);

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_VSI_STATS_GET,
				 &req, sizeof(req),
				 &resp, sizeof(resp));
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "vsi stats get failed, ret=%d", ret);
		goto l_end;
	}

	new_stats->rx_vsi_unicast_packets   = rte_le_to_cpu_64(resp.rx_vsi_unicast_packets);
	new_stats->rx_vsi_bytes             = rte_le_to_cpu_64(resp.rx_vsi_bytes);
	new_stats->tx_vsi_unicast_packets   = rte_le_to_cpu_64(resp.tx_vsi_unicast_packets);
	new_stats->tx_vsi_bytes             = rte_le_to_cpu_64(resp.tx_vsi_bytes);
	new_stats->rx_vsi_multicast_packets = rte_le_to_cpu_64(resp.rx_vsi_multicast_packets);
	new_stats->tx_vsi_multicast_packets = rte_le_to_cpu_64(resp.tx_vsi_multicast_packets);
	new_stats->rx_vsi_broadcast_packets = rte_le_to_cpu_64(resp.rx_vsi_broadcast_packets);
	new_stats->tx_vsi_broadcast_packets = rte_le_to_cpu_64(resp.tx_vsi_broadcast_packets);
	new_stats->opackets = new_stats->tx_vsi_unicast_packets +
			      new_stats->tx_vsi_multicast_packets +
			      new_stats->tx_vsi_broadcast_packets;
	new_stats->obytes = new_stats->tx_vsi_bytes;
	new_stats->ipackets = new_stats->rx_vsi_unicast_packets +
			      new_stats->rx_vsi_multicast_packets +
			      new_stats->rx_vsi_broadcast_packets;
	new_stats->ibytes = new_stats->rx_vsi_bytes;

l_end:
	return ret;
}

int32_t sxe2_drv_vsi_stats_reset(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_VSI_STATS_CLEAR,
				 NULL, 0, NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "vsi stats reset failed, ret=%d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

int32_t sxe2_drv_promisc_config(struct sxe2_adapter *adapter, bool set)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_promisc_filter_cfg_req promisc_filter_cfg_req = {0};

	promisc_filter_cfg_req.vsi_id = adapter->vsi_ctxt.dpdk_vsi_id;
	promisc_filter_cfg_req.is_add = set;
	promisc_filter_cfg_req.type = SXE2_PROMISC_FILTER_TYPE_PROMISC;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_PROMISC_CFG,
				 &promisc_filter_cfg_req,
				 sizeof(promisc_filter_cfg_req),
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "promic config failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_allmulti_config(struct sxe2_adapter *adapter, bool set)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_promisc_filter_cfg_req promisc_filter_cfg_req = {0};

	promisc_filter_cfg_req.vsi_id = adapter->vsi_ctxt.dpdk_vsi_id;
	promisc_filter_cfg_req.is_add = set;
	promisc_filter_cfg_req.type = SXE2_PROMISC_FILTER_TYPE_ALLMULTI;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_ALLMULTI_CFG,
				 &promisc_filter_cfg_req,
				 sizeof(promisc_filter_cfg_req),
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "allmulti config failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_switchdev_uplink_config(struct sxe2_adapter *adapter,  bool set)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_switchdev_uplink_info switchdev_uplink_info_req = {0};

	switchdev_uplink_info_req.pf_id = adapter->pf_idx;
	switchdev_uplink_info_req.is_set = set;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_SWITCH_UPLINK,
				 &switchdev_uplink_info_req,
				 sizeof(switchdev_uplink_info_req),
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "switchdev uplink info config failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_switchdev_repr_vf_config(struct sxe2_adapter *adapter,
				      struct sxe2_switchdev_repr_info *repr_vf,
				      bool set)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_switchdev_repr_info switchdev_repr_info_req = {0};

	switchdev_repr_info_req.pf_id = adapter->pf_idx;
	switchdev_repr_info_req.is_set = set;
	switchdev_repr_info_req.cp_vsi_id = repr_vf->cp_vsi_id;
	switchdev_repr_info_req.repr_pf_id = repr_vf->repr_pf_id;
	switchdev_repr_info_req.repr_vf_id = repr_vf->repr_vf_id;
	switchdev_repr_info_req.repr_q_id = repr_vf->repr_q_id;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_SWITCH_REPR,
			&switchdev_repr_info_req,
			sizeof(switchdev_repr_info_req),
			NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "switchdev repr info config failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_switchdev_mode_get(struct sxe2_adapter *adapter, bool *is_switchdev)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_switchdev_mode_info switchdev_mode_info_req = {0};
	struct sxe2_switchdev_mode_info switchdev_mode_info_resp = {0};

	switchdev_mode_info_req.pf_id = adapter->pf_idx;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_SWITCH_MODE,
				 &switchdev_mode_info_req,
				 sizeof(switchdev_mode_info_req),
				 &switchdev_mode_info_resp,
				 sizeof(switchdev_mode_info_resp));

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "switchdev mode info get failed, ret=%d", ret);
	else
		*is_switchdev = (bool)switchdev_mode_info_resp.is_switchdev;

	return ret;
}

int32_t sxe2_drv_switchdev_cpvsi_get(struct sxe2_adapter *adapter, uint16_t *cp_vsi_id)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_switchdev_cpvsi_info switchdev_cpvsi_resp = {0};

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_SWITCH_CPVSI,
				 NULL, 0,
				 &switchdev_cpvsi_resp,
				 sizeof(switchdev_cpvsi_resp));
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "switchdev cpvsi info get failed, ret=%d", ret);
	else
		*cp_vsi_id = switchdev_cpvsi_resp.cp_vsi_id;

	return ret;
}

int32_t sxe2_drv_uc_config(struct sxe2_adapter *adapter, struct rte_ether_addr *addr, bool add)
{
	int32_t ret = 0;
	int32_t i;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_mac_filter_cfg_req mac_filter_cfg_req = {0};

	mac_filter_cfg_req.vsi_id = adapter->vsi_ctxt.dpdk_vsi_id;
	for (i = 0; i < SXE2_ETH_ALEN; i++)
		mac_filter_cfg_req.addr[i] = addr->addr_bytes[i];
	mac_filter_cfg_req.is_add = add;
	mac_filter_cfg_req.type = SXE2_MAC_FILTER_TYPE_UC;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_MAC_ADDR_UC,
				 &mac_filter_cfg_req,
				 sizeof(mac_filter_cfg_req),
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "uc config query failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_mc_config(struct sxe2_adapter *adapter, struct rte_ether_addr *addr, bool add)
{
	int32_t ret = 0;
	int32_t i;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_mac_filter_cfg_req mac_filter_cfg_req = {0};

	mac_filter_cfg_req.vsi_id = adapter->vsi_ctxt.dpdk_vsi_id;
	for (i = 0; i < SXE2_ETH_ALEN; i++)
		mac_filter_cfg_req.addr[i] = addr->addr_bytes[i];

	mac_filter_cfg_req.is_add = add;
	mac_filter_cfg_req.type = SXE2_MAC_FILTER_TYPE_MC;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_MAC_ADDR_MC,
				  &mac_filter_cfg_req, sizeof(mac_filter_cfg_req),
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "mac config query failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_vlan_config_query(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_vlan_cfg_query_resp vlan_cfg_query_resp = {0};

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_VLAN_CFG_QUERY,
				 NULL, 0,
				 &vlan_cfg_query_resp,
				 sizeof(vlan_cfg_query_resp));

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "vlan config query failed, ret=%d", ret);

	adapter->filter_ctxt.vlan_info.port_vlan_exist = vlan_cfg_query_resp.port_vlan_exist;
	adapter->filter_ctxt.vlan_info.is_switchdev = vlan_cfg_query_resp.is_switchdev;


	adapter->filter_ctxt.vlan_info.tpid = vlan_cfg_query_resp.tpid;
	adapter->filter_ctxt.vlan_info.vid = vlan_cfg_query_resp.vid;

	adapter->filter_ctxt.vlan_info.outer_insert = vlan_cfg_query_resp.outer_insert;
	adapter->filter_ctxt.vlan_info.outer_strip = vlan_cfg_query_resp.outer_strip;
	adapter->filter_ctxt.vlan_info.inner_insert = vlan_cfg_query_resp.inner_insert;
	adapter->filter_ctxt.vlan_info.inner_strip = vlan_cfg_query_resp.inner_strip;

	return ret;
}

int32_t sxe2_drv_vlan_filter_id_config(struct sxe2_adapter *adapter,
				       struct sxe2_vlan *vlan, bool on)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_vlan_filter_cfg_req vlan_filter_cfg_req = {0};

	vlan_filter_cfg_req.vsi_id = adapter->vsi_ctxt.dpdk_vsi_id;
	vlan_filter_cfg_req.tpid_id = vlan->tpid;
	vlan_filter_cfg_req.vlan_id = vlan->vid;
	vlan_filter_cfg_req.prio = vlan->prio;
	vlan_filter_cfg_req.is_add = on;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_VLAN_FILTER_ADD_DEL,
				 &vlan_filter_cfg_req, sizeof(vlan_filter_cfg_req),
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "vlan config failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_vlan_insert_strip_cfg(struct sxe2_adapter *adapter)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_vlan_offload_cfg_req vlan_offload_cfg_req = {0};

	vlan_offload_cfg_req.vsi_id = adapter->vsi_ctxt.dpdk_vsi_id;
	vlan_offload_cfg_req.tpid = adapter->filter_ctxt.vlan_info.tpid;
	vlan_offload_cfg_req.outer_insert = adapter->filter_ctxt.vlan_info.outer_insert;
	vlan_offload_cfg_req.outer_strip = adapter->filter_ctxt.vlan_info.outer_strip;
	vlan_offload_cfg_req.inner_insert = adapter->filter_ctxt.vlan_info.inner_insert;
	vlan_offload_cfg_req.inner_strip = adapter->filter_ctxt.vlan_info.inner_strip;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_VLAN_OFFLOAD_CFG,
				 &vlan_offload_cfg_req,
				 sizeof(vlan_offload_cfg_req),
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "vlan config query failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_vlan_filter_switch(struct sxe2_adapter *adapter, bool on)
{
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_vlan_filter_switch_req vlan_filter_switch_req = {0};

	vlan_filter_switch_req.vsi_id = adapter->vsi_ctxt.dpdk_vsi_id;
	vlan_filter_switch_req.is_oper_enable = on;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_VLAN_FILTER_SWITCH,
				 &vlan_filter_switch_req,
				 sizeof(vlan_filter_switch_req),
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "vlan config filter failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_rss_key_set(struct sxe2_adapter *adapter, uint8_t *key, uint16_t key_size)
{
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_rss_key_req *req = NULL;
	int32_t ret = 0;
	uint16_t buf_size = sizeof(*req) + key_size;

	req = rte_zmalloc("drv_cmd_rss_key", buf_size, 0);
	if (!req) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to alloc rss key");
		ret = -ENOMEM;
		goto l_end;
	}

	req->vsi_id = rte_cpu_to_le_16(adapter->vsi_ctxt.dpdk_vsi_id);
	req->key_size = rte_cpu_to_le_16(key_size);
	rte_memcpy(req->key, key, key_size);

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_RSS_KEY_SET,
				 req, buf_size,
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to cmd set rss key, ret=%d", ret);
		goto l_end;
	}

l_end:
	if (req) {
		rte_free(req);
		req = NULL;
	}
	return ret;
}

int32_t sxe2_drv_rss_lut_set(struct sxe2_adapter *adapter, uint8_t *lut, uint16_t lut_size)
{
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_rss_lut_req *req = NULL;
	int32_t ret = 0;
	uint16_t buf_size = sizeof(struct sxe2_rss_lut_req) + lut_size;

	req = rte_zmalloc("drv_cmd_rss_lut", buf_size, 0);
	if (!req) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to alloc rss lut");
		ret = -ENOMEM;
		goto l_end;
	}

	req->vsi_id = rte_cpu_to_le_16(adapter->vsi_ctxt.dpdk_vsi_id);
	req->lut_size = rte_cpu_to_le_16(lut_size);
	rte_memcpy(req->lut, lut, lut_size);

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_RSS_LUT_SET,
				 req, buf_size,
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to cmd set rss lut, ret=%d", ret);
		goto l_end;
	}

l_end:
	if (req) {
		rte_free(req);
		req = NULL;
	}
	return ret;
}

int32_t sxe2_drv_rss_hash_ctrl_func(struct sxe2_adapter *adapter, enum sxe2_rss_hash_key_func func)
{
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_rss_func_req req = {0};
	int32_t ret = 0;

	req.vsi_id = rte_cpu_to_le_16(adapter->vsi_ctxt.dpdk_vsi_id);
	req.func = func;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_RSS_FUNC_SET,
				 &req, sizeof(req),
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to cmd set rss func, ret=%d", ret);
	return ret;
}

static void sxe2_drv_flow_bitmap_fill(uint32_t *bitmap, uint16_t *list)
{
	uint16_t index = 0;
	uint16_t i = 0;
	uint16_t map_size = sizeof(*bitmap) * SXE2_BITS_PER_BYTE;

	while (list[i] != SXE2_FLOW_END) {
		index = list[i] / map_size;
		bitmap[index] |= (1UL << (list[i] % map_size));
		i++;
	}
}

int32_t sxe2_drv_rss_hf_add(struct sxe2_adapter *adapter,
			struct sxe2_rss_hf_config *rss_conf)
{
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_rss_hf_req req = {0};
	int32_t ret = 0;

	req.vsi_id = rte_cpu_to_le_16(adapter->vsi_ctxt.dpdk_vsi_id);
	req.symm = rss_conf->symm;
	req.hdr_type = rte_cpu_to_le_32(SXE2_RSS_OUTER_HEADERS);
	sxe2_drv_flow_bitmap_fill(req.headers, rss_conf->hdrs);
	sxe2_drv_flow_bitmap_fill(req.hash_flds, rss_conf->flds);

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_RSS_HF_ADD,
				 &req, sizeof(req),
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to cmd add rss hf, ret=%d", ret);
	return ret;
}

int32_t sxe2_drv_rss_hf_del(struct sxe2_adapter *adapter,
				struct sxe2_rss_hf_config *rss_conf)
{
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_rss_hf_req req = {0};
	int32_t ret = 0;

	req.vsi_id = rte_cpu_to_le_16(adapter->vsi_ctxt.dpdk_vsi_id);
	req.symm = rss_conf->symm;
	req.hdr_type = rte_cpu_to_le_32(SXE2_RSS_OUTER_HEADERS);
	sxe2_drv_flow_bitmap_fill(req.headers, rss_conf->hdrs);
	sxe2_drv_flow_bitmap_fill(req.hash_flds, rss_conf->flds);

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_RSS_HF_DEL,
		&req, sizeof(req), NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to cmd del rss hf, ret=%d", ret);
	return ret;
}

int32_t sxe2_drv_rss_hf_clear(struct sxe2_adapter *adapter)
{
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	int32_t ret = 0;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_RSS_HF_CLEAR,
				 NULL, 0,
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to cmd clear rss hf, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_ptp_gettime(struct sxe2_adapter *adapter, struct sxe2_rx_queue *rxq)
{
	(void)adapter;
	(void)rxq;
	return 0;
}

int32_t sxe2_drv_root_tree_alloc(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_tm_context *tm_ctxt = &adapter->tm_ctxt;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_tm_res tm_resp;
	int32_t ret;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_SCHED_ROOT_TREE_ALLOC,
				 NULL, 0,
				 &tm_resp, sizeof(tm_resp));
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_LOG_ERR(DRV, "add sched root failed, ret:%d", ret);
		goto l_end;
	}

	tm_ctxt->root_teid = tm_resp.teid;

l_end:
	return ret;
}

int32_t sxe2_drv_root_tree_release(struct rte_eth_dev *dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(dev);
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_tm_res tm_res = {0};
	int32_t ret;

	tm_res.teid = adapter->tm_ctxt.root_teid;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_SCHED_ROOT_TREE_RELEASE,
				 &tm_res, sizeof(tm_res),
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_LOG_ERR(DRV, "release sched root failed, ret:%d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static void sxe2_drv_tm_node_to_info(struct sxe2_adapter *adapter,
		struct sxe2_tm_node *node, struct sxe2_tm_info *info)
{
	uint32_t rate = 0;

	if (node->shaper_profile->profile.committed.rate == UINT64_MAX)
		rate = UINT32_MAX;
	else
		rate = (uint32_t)(node->shaper_profile->profile.committed.rate * 8 / 1000);

	info->committed = rte_cpu_to_le_32(rate);

	if (node->shaper_profile->profile.peak.rate == UINT64_MAX)
		rate = UINT32_MAX;
	else
		rate = (uint32_t)(node->shaper_profile->profile.peak.rate * 8 / 1000);

	info->peak = rte_cpu_to_le_32(rate);

	info->priority = (adapter->tm_ctxt.prio_max - 1 - node->priority);

	info->weight = rte_cpu_to_le_16(node->hw_weight);
}

static int32_t sxe2_drv_tm_commit_node(struct sxe2_adapter *adapter,
		struct sxe2_tm_node *tm_node)
{
	struct sxe2_drv_cmd_params cmd = { 0 };
	struct sxe2_tm_add_mid_msg msg_mid = {0};
	struct sxe2_tm_add_queue_msg msg_queue = {0};
	struct sxe2_tm_res res = {0};
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret;
	uint32_t i;

	if (tm_node->type == SXE2_TM_NODE_TYPE_VSIG) {
		goto l_add;
	} else if (tm_node->type == SXE2_TM_NODE_TYPE_MID) {
		sxe2_drv_tm_node_to_info(adapter, tm_node, &msg_mid.info);
		msg_mid.parent_teid = rte_cpu_to_le_16(tm_node->parent->teid);
		msg_mid.adj_lvl = adapter->sched_ctxt.adj_lvl;

		sxe2_drv_cmd_params_fill(adapter, &cmd,
					 SXE2_DRV_CMD_SCHED_TM_ADD_MID_NODE,
					 &msg_mid, sizeof(msg_mid),
					 &res, sizeof(res));
		ret = sxe2_drv_cmd_exec(cdev, &cmd);
		if (ret) {
			PMD_LOG_ERR(DRV, "add tm mid node failed, ret:%d", ret);
			goto l_end;
		}
	} else if (tm_node->type == SXE2_TM_NODE_TYPE_QUEUE) {
		sxe2_drv_tm_node_to_info(adapter, tm_node, &msg_queue.info);
		msg_queue.parent_teid = rte_cpu_to_le_16(tm_node->parent->teid);
		msg_queue.queue_id = rte_cpu_to_le_16(tm_node->id);
		msg_queue.adj_lvl = adapter->sched_ctxt.adj_lvl;

		sxe2_drv_cmd_params_fill(adapter, &cmd,
					 SXE2_DRV_CMD_SCHED_TM_ADD_QUEUE_NODE,
					 &msg_queue, sizeof(msg_queue),
					 &res, sizeof(res));
		ret = sxe2_drv_cmd_exec(cdev, &cmd);
		if (ret) {
			PMD_LOG_ERR(DRV, "add tm queue failed, ret:%d", ret);
			goto l_end;
		}
	} else {
		PMD_LOG_ERR(DRV, "commit tm node failed, type:%d", tm_node->type);
		ret = -EINVAL;
		goto l_end;
	}

	tm_node->teid = rte_le_to_cpu_16(res.teid);

l_add:
	for (i = 0; i < tm_node->child_cnt; i++) {
		ret = sxe2_drv_tm_commit_node(adapter, tm_node->children[i]);
		if (ret)
			goto l_end;
	}
l_end:
	return ret;
}

int32_t sxe2_drv_tm_commit(struct sxe2_adapter *adapter)
{
	struct sxe2_drv_cmd_params cmd = { 0 };
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_tm_res tm_res = {0};
	int32_t ret;

	tm_res.teid = adapter->tm_ctxt.root_teid;
	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_SCHED_ROOT_CHILDREN_DELETE,
				 &tm_res, sizeof(tm_res),
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret) {
		PMD_LOG_ERR(DRV, "del tm root failed, ret:%d", ret);
		goto l_end;
	}

	ret = sxe2_drv_tm_commit_node(adapter, adapter->tm_ctxt.root);
	if (ret) {
		PMD_LOG_ERR(DRV, "commit tm node failed, ret:%d", ret);
		goto l_end;
	}

	PMD_LOG_DEBUG(DRV, "commit tm success");
l_end:
	return ret;
}

int32_t sxe2_drv_ipsec_get_capa(struct sxe2_adapter *adapter)
{
	int32_t ret = -1;
	struct sxe2_drv_cmd_params cmd = { 0 };
	struct sxe2_drv_ipsec_capa_resq resp;
	struct sxe2_common_device *cdev = adapter->cdev;

	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_IPSEC_CAP_GET,
				 NULL, 0,
				 &resp, sizeof(resp));
	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to get ipsec specifications, ret=%d", ret);
		goto l_end;
	}

	adapter->security_ctx.ipsec_ctx.max_tx_sa = rte_le_to_cpu_16(resp.tx_sa_cnt);
	adapter->security_ctx.ipsec_ctx.max_rx_sa = rte_le_to_cpu_16(resp.rx_sa_cnt);
	adapter->security_ctx.ipsec_ctx.max_tcam = rte_le_to_cpu_16(resp.ip_id_cnt);
	adapter->security_ctx.ipsec_ctx.max_udp_group = rte_le_to_cpu_16(resp.udp_group_cnt);

	PMD_DEV_LOG_INFO(adapter, DRV, "Max tx sa:%u, max rx sa:%u, max tcam:%u, udp group:%u.",
			 rte_le_to_cpu_16(resp.tx_sa_cnt),
			 rte_le_to_cpu_16(resp.rx_sa_cnt),
			 rte_le_to_cpu_16(resp.ip_id_cnt),
			 rte_le_to_cpu_16(resp.udp_group_cnt));

l_end:
	return ret;
}

int32_t sxe2_drv_ipsec_resource_clear(struct sxe2_adapter *adapter)
{
	int32_t ret = -1;
	struct sxe2_drv_cmd_params cmd = { 0 };
	struct sxe2_common_device *cdev = adapter->cdev;

	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_IPSEC_RESOURCE_CLEAR,
				 NULL, 0,
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to clear ipsec resource, ret=%d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

int32_t sxe2_drv_ipsec_txsa_add(struct sxe2_adapter *adapter,
		struct sxe2_ipsec_tx_sa *tx_sa)
{
	struct sxe2_drv_cmd_params cmd               = { 0 };
	struct sxe2_drv_ipsec_txsa_add_req req   = { 0 };
	struct sxe2_drv_ipsec_txsa_add_resp resp = { 0 };
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret                                   = -1;
	uint32_t mode                                  = 0;
	uint32_t i                                     = 0;

	if (tx_sa->algo == SXE2_IPSEC_ALGO_SM4_CBC_AND_SM3_96_HMAC)
		mode |= IPSEC_TX_ENGINE_SM4;
	if (tx_sa->mode == SXE2_IPSEC_MODE_ENC_AND_AUTH)
		mode |= IPSEC_TX_ENCRYPT;
	req.mode = rte_cpu_to_le_32(mode);
	for (i = 0; i < SXE2_IPSEC_KEY_LEN; i++) {
		req.encrypt_keys[i] = tx_sa->enc_key[i];
		req.auth_keys[i] = tx_sa->auth_key[i];
	}

	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_IPSEC_TXSA_ADD,
				 &req, sizeof(req),
				 &resp, sizeof(resp));

	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "failed to add tx sa, ret=%d", ret);
		goto l_end;
	}
	tx_sa->hw_sa_id = rte_le_to_cpu_16(resp.index);

l_end:
	return ret;
}

int32_t sxe2_drv_ipsec_rxsa_add(struct sxe2_adapter *adapter,
		struct sxe2_ipsec_rx_sa *rx_sa,
		struct sxe2_ipsec_rx_tcam *rx_tcam,
		struct sxe2_ipsec_rx_udp_group *rx_udp_group)
{
	struct sxe2_drv_cmd_params cmd               = { 0 };
	struct sxe2_drv_ipsec_rxsa_add_req req   = { 0 };
	struct sxe2_drv_ipsec_rxsa_add_resp resp = { 0 };
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret                                   = -1;
	uint32_t mode                                  = 0;
	uint32_t i                                     = 0;

	if (rx_sa->algo == SXE2_IPSEC_ALGO_SM4_CBC_AND_SM3_96_HMAC)
		mode |= IPSEC_RX_ENGINE_SM4;
	if (rx_sa->mode == SXE2_IPSEC_MODE_ENC_AND_AUTH)
		mode |= IPSEC_RX_DECRYPT;
	if (rx_tcam->ip_addr.type == RTE_SECURITY_IPSEC_TUNNEL_IPV6) {
		mode |= IPSEC_RX_IPV6;
		memcpy(req.ipaddr, rx_tcam->ip_addr.dst_ipv6, sizeof(req.ipaddr));
	} else {
		req.ipaddr[0] = rx_tcam->ip_addr.dst_ipv4;
	}
	req.mode = rte_cpu_to_le_32(mode);
	req.spi = rte_cpu_to_le_32(rx_sa->spi);
	if (rx_udp_group != NULL) {
		req.udp_port = rte_cpu_to_le_32((uint32_t)rx_udp_group->udp_port);
		req.sport_en = rx_udp_group->sport_en;
		req.dport_en = rx_udp_group->dport_en;
	}

	PMD_DEV_LOG_INFO(adapter, DRV, "Add rx sa, mode: 0x%x, spi: 0x%x, udp_port: %u, "
			 "sport_en: %u, dport_en: %u.",
			 req.mode, req.spi, req.udp_port, req.sport_en, req.dport_en);

	/* encrypt and auth keys */
	for (i = 0; i < SXE2_IPSEC_KEY_LEN; i++) {
		req.encrypt_keys[i] = rx_sa->enc_key[i];
		req.auth_keys[i] = rx_sa->auth_key[i];
	}

	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_IPSEC_RXSA_ADD,
				 &req, sizeof(req),
				 &resp, sizeof(resp));

	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add rx sa, ret=%d", ret);
		goto l_end;
	}
	rx_sa->hw_sa_id = rte_le_to_cpu_16(resp.sa_idx);
	rx_sa->hw_ip_id = resp.ip_id;
	rx_tcam->hw_ip_id = resp.ip_id;
	rx_sa->hw_udp_group_id = resp.udp_group_id;
	if (rx_udp_group != NULL)
		rx_udp_group->hw_group_id = resp.udp_group_id;

l_end:
	return ret;
}

int32_t sxe2_drv_ipsec_rxsa_delete(struct sxe2_adapter *adapter,
					struct sxe2_ipsec_rx_sa *rx_sa)
{
	struct sxe2_drv_ipsec_rxsa_del_req req = { 0 };
	struct sxe2_drv_cmd_params cmd             = { 0 };
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret                                 = -1;

	req.sa_idx = rte_cpu_to_le_16(rx_sa->hw_sa_id);
	req.spi = rte_cpu_to_le_32(rx_sa->spi);
	req.ip_id = rx_sa->hw_ip_id;
	req.group_id = rx_sa->hw_udp_group_id;

	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_IPSEC_RXSA_DEL,
				 &req, sizeof(req),
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV,
				"Failed to delete rx sa, sa id: %u, spi: %u, "
				"ip id: %u, udp group id: %u, ret: %d.",
				rx_sa->hw_sa_id, rx_sa->spi, rx_sa->hw_ip_id,
				rx_sa->hw_udp_group_id, ret);

	return ret;
}

int32_t sxe2_drv_ipsec_txsa_delete(struct sxe2_adapter *adapter,
					   uint16_t sa_id)
{
	struct sxe2_drv_ipsec_txsa_del_req req = { 0 };
	struct sxe2_drv_cmd_params cmd             = { 0 };
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret                                 = -1;

	req.sa_idx = rte_cpu_to_le_16(sa_id);
	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_IPSEC_TXSA_DEL,
				 &req, sizeof(req),
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV,
				"Failed to delete tx sa, sa id: %u, ret: %d.",
				sa_id, ret);

	return ret;
}

int32_t sxe2_drv_queue_info_get_update(struct sxe2_adapter *adapter, struct eth_queue_stats *qstats)
{
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_queue_map_info resp = {0};
	struct sxe2_common_device *cdev = adapter->cdev;
	uint8_t pool_idx;
	uint8_t index;
	int32_t ret;

	if (!(adapter->cap_flags & SXE2_DEV_CAPS_OFFLOAD_Q_MAP)) {
		ret = 0;
		goto l_end;
	}

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_TX_RX_MAP_GET,
				 NULL, 0,
				 &resp, sizeof(resp));
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret) {
		PMD_LOG_ERR(DRV, "get queue info map failed, ret=%d", ret);
		goto l_end;
	}

	for (pool_idx = 0; pool_idx < SXE2_RXQ_STATS_MAP_MAX_NUM; pool_idx++) {
		qstats->q_ipackets[pool_idx] = resp.rxq_stats_map_info[pool_idx].rxq_lan_in_pkt_cnt;
		qstats->q_ibytes[pool_idx] = resp.rxq_stats_map_info[pool_idx].rxq_lan_in_byte_cnt;
	}

	for (index = 0; index < SXE2_TXQ_STATS_MAP_MAX_NUM; index++) {
		qstats->q_opackets[index] = resp.txq_stats_map_info[index].txq_lan_pkt_cnt;
		qstats->q_obytes[index] = resp.txq_stats_map_info[index].txq_lan_byte_cnt;
	}

l_end:
	return ret;
}

int32_t sxe2_drv_rxq_mapping_set(struct rte_eth_dev *eth_dev, uint16_t queue_id, uint8_t pool_idx)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(eth_dev);
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_rx_map_req req = {0};
	struct sxe2_rx_queue *rxq = NULL;

	rxq = eth_dev->data->rx_queues[queue_id];
	if (rxq == NULL) {
		PMD_LOG_ERR(DRV, "Rx queue %u is not available or setup",
				queue_id);
		ret = -EINVAL;
		goto l_end;
	}

	req.queue_id = rxq->queue_id;
	req.pool_idx = pool_idx;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_RX_MAP_SET,
				 &req, sizeof(req),
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_LOG_ERR(DRV, "get dev caps failed, ret=%d", ret);

l_end:
	return ret;
}

int32_t sxe2_drv_txq_mapping_set(struct rte_eth_dev *eth_dev, uint16_t queue_id, uint8_t pool_idx)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(eth_dev);
	int32_t ret = 0;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_drv_tx_map_req req = {0};
	struct sxe2_tx_queue *txq = NULL;

	txq = eth_dev->data->tx_queues[queue_id];
	if (txq == NULL) {
		PMD_LOG_ERR(DRV, "Rx queue %u is not available or setup", queue_id);
		ret = -EINVAL;
		goto l_end;
	}

	req.queue_id = txq->queue_id;
	req.pool_idx = pool_idx;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_TX_MAP_SET,
				 &req, sizeof(req),
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_LOG_ERR(DRV, "get dev caps failed, ret=%d", ret);

l_end:
	return ret;
}

int32_t sxe2_drv_mapping_reset(struct rte_eth_dev *eth_dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(eth_dev);
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_TX_RX_MAP_RESET,
				 NULL, 0,
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_LOG_ERR(DRV, "Reset queue mapping failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_mapping_stats_info_clear(struct rte_eth_dev *eth_dev)
{
	struct sxe2_adapter *adapter = SXE2_DEV_PRIVATE_TO_ADAPTER(eth_dev);
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret;

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_TX_RX_MAP_INFO_CLEAR,
				 NULL, 0,
				 NULL, 0);

	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_LOG_ERR(DRV, "Clear map stats info failed, ret=%d", ret);

	return ret;
}

int32_t sxe2_drv_flow_filter_add(struct sxe2_adapter *adapter, struct sxe2_flow *flow)
{
	struct sxe2_drv_flow_filter_req req = { 0 };
	struct sxe2_drv_flow_filter_resp resp = { 0 };
	struct sxe2_drv_cmd_params cmd             = { 0 };
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret                                 = -1;

	memcpy(&req.pattern_inner, &flow->pattern_inner, sizeof(req.pattern_inner));
	memcpy(&req.pattern_outer, &flow->pattern_outer, sizeof(req.pattern_outer));
	memcpy(&req.action, &flow->action, sizeof(req.action));
	memcpy(&req.meta, &flow->meta, sizeof(req.meta));
	req.engine_type = flow->engine_type;
	req.flow_id = 0;

	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_FLOW_FILTER_ADD, &req,
			   sizeof(req), &resp, sizeof(resp));
	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to add flow filter, ret: %d.", ret);
	flow->flow_id = resp.flow_id;
	flow->create_err = ret;
	return ret;
}

int32_t sxe2_drv_flow_filter_del(struct sxe2_adapter *adapter, struct sxe2_flow *flow)
{
	struct sxe2_drv_cmd_params cmd             = { 0 };
	struct sxe2_drv_flow_filter_req req = { 0 };
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret                                 = -1;
	memcpy(&req.pattern_inner, &flow->pattern_inner, sizeof(req.pattern_inner));
	memcpy(&req.pattern_outer, &flow->pattern_outer, sizeof(req.pattern_outer));
	memcpy(&req.action, &flow->action, sizeof(req.action));
	memcpy(&req.meta, &flow->meta, sizeof(req.meta));
	req.engine_type = flow->engine_type;
	req.flow_id = flow->flow_id;

	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_FLOW_FILTER_DEL, &req,
			   sizeof(req), NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret)
		PMD_DEV_LOG_ERR(adapter, DRV,
			"Failed to delete flow filter, flow id: %u, ret: %d.",
			flow->flow_id, ret);
	return ret;
}

int32_t sxe2_drv_flow_fnav_get_stat_id(struct sxe2_adapter *adapter, uint32_t *stat_id)
{
	struct sxe2_drv_flow_fnav_get_stat_id_req req = { 0 };
	struct sxe2_drv_flow_fnav_get_stat_id_resp resp = { 0 };
	struct sxe2_drv_cmd_params cmd             = { 0 };
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret                                 = -1;

	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_FLOW_FNAV_STAT_ALLOC,
				 &req, sizeof(req),
				 &resp, sizeof(resp));
	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to get fnav stat id, ret: %d.", ret);
		goto l_end;
	}
	*stat_id = resp.stat_id;

l_end:
	return ret;
}

int32_t sxe2_drv_flow_fnav_free_stat(struct sxe2_adapter *adapter, uint32_t stat_id)
{
	struct sxe2_drv_flow_fnav_free_stat_id_req req = { 0 };
	struct sxe2_drv_cmd_params cmd             = { 0 };
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret                                 = -1;

	req.stat_id = stat_id;
	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_FLOW_FNAV_STAT_FREE,
				 &req, sizeof(req),
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV, "Failed to free fnav stat id, ret: %d.", ret);
		goto l_end;
	}

l_end:
	return ret;
}

int32_t sxe2_drv_flow_fnav_query_stat(struct sxe2_adapter *adapter,
		struct sxe2_fnav_cid_mgr *mgr)
{
	struct sxe2_drv_flow_fnav_query_stat_req req = { 0 };
	struct sxe2_drv_flow_fnav_query_stat_resp resp = { 0 };
	struct sxe2_drv_cmd_params cmd             = { 0 };
	struct sxe2_common_device *cdev = adapter->cdev;
	int32_t ret                                 = -1;

	req.stat_id = mgr->stat_index;
	req.stat_ctrl = mgr->count_type;
	req.is_clear = 1;

	sxe2_drv_cmd_params_fill(adapter, &cmd, SXE2_DRV_CMD_FLOW_FNAV_STAT_QUERY,
				 &req, sizeof(req),
				 &resp, sizeof(resp));
	ret = sxe2_drv_cmd_exec(cdev, &cmd);
	if (ret) {
		PMD_DEV_LOG_ERR(adapter, DRV,
			"Failed to query fnav stat, stat id: %u, ret: %d.",
			req.stat_id, ret);
		goto l_end;
	}
	mgr->hits += resp.stat_hits;
	mgr->bytes += resp.stat_bytes;

l_end:
	return ret;
}

int32_t sxe2_drv_srcvsi_prune_config(struct sxe2_adapter *adapter,
			uint16_t *vsi_list, uint16_t vsi_cnt, bool set)
{
	int32_t ret = 0;
	uint16_t idx;
	struct sxe2_common_device *cdev = adapter->cdev;
	struct sxe2_drv_cmd_params param = {0};
	struct sxe2_srcvsi_ext_cfg_req srcvsi_list_prune_cfg_req = {0};

	srcvsi_list_prune_cfg_req.vsi_id = adapter->vsi_ctxt.dpdk_vsi_id;
	srcvsi_list_prune_cfg_req.is_add = set;
	srcvsi_list_prune_cfg_req.srcvsi_cnt = vsi_cnt;
	for (idx = 0; idx < vsi_cnt; idx++)
		srcvsi_list_prune_cfg_req.srcvsi_list[idx] = vsi_list[idx];

	sxe2_drv_cmd_params_fill(adapter, &param, SXE2_DRV_CMD_VSI_SRCVSI_PRUNE,
				 &srcvsi_list_prune_cfg_req,
				 sizeof(srcvsi_list_prune_cfg_req),
				 NULL, 0);
	ret = sxe2_drv_cmd_exec(cdev, &param);
	if (ret)
		PMD_DEV_LOG_WARN(adapter, DRV, "srcvsi prune config failed, ret=%d", ret);

	return ret;
}
