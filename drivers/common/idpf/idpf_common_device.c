/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <rte_log.h>
#include <idpf_common_device.h>
#include <idpf_common_virtchnl.h>

static void
idpf_reset_pf(struct idpf_hw *hw)
{
	uint32_t reg;

	reg = IDPF_READ_REG(hw, PFGEN_CTRL);
	IDPF_WRITE_REG(hw, PFGEN_CTRL, (reg | PFGEN_CTRL_PFSWR));
}

#define IDPF_RESET_WAIT_CNT 100
static int
idpf_check_pf_reset_done(struct idpf_hw *hw)
{
	uint32_t reg;
	int i;

	for (i = 0; i < IDPF_RESET_WAIT_CNT; i++) {
		reg = IDPF_READ_REG(hw, PFGEN_RSTAT);
		if (reg != 0xFFFFFFFF && (reg & PFGEN_RSTAT_PFR_STATE_M))
			return 0;
		rte_delay_ms(1000);
	}

	DRV_LOG(ERR, "IDPF reset timeout");
	return -EBUSY;
}

#define CTLQ_NUM 2
static int
idpf_init_mbx(struct idpf_hw *hw)
{
	struct idpf_ctlq_create_info ctlq_info[CTLQ_NUM] = {
		{
			.type = IDPF_CTLQ_TYPE_MAILBOX_TX,
			.id = IDPF_CTLQ_ID,
			.len = IDPF_CTLQ_LEN,
			.buf_size = IDPF_DFLT_MBX_BUF_SIZE,
			.reg = {
				.head = PF_FW_ATQH,
				.tail = PF_FW_ATQT,
				.len = PF_FW_ATQLEN,
				.bah = PF_FW_ATQBAH,
				.bal = PF_FW_ATQBAL,
				.len_mask = PF_FW_ATQLEN_ATQLEN_M,
				.len_ena_mask = PF_FW_ATQLEN_ATQENABLE_M,
				.head_mask = PF_FW_ATQH_ATQH_M,
			}
		},
		{
			.type = IDPF_CTLQ_TYPE_MAILBOX_RX,
			.id = IDPF_CTLQ_ID,
			.len = IDPF_CTLQ_LEN,
			.buf_size = IDPF_DFLT_MBX_BUF_SIZE,
			.reg = {
				.head = PF_FW_ARQH,
				.tail = PF_FW_ARQT,
				.len = PF_FW_ARQLEN,
				.bah = PF_FW_ARQBAH,
				.bal = PF_FW_ARQBAL,
				.len_mask = PF_FW_ARQLEN_ARQLEN_M,
				.len_ena_mask = PF_FW_ARQLEN_ARQENABLE_M,
				.head_mask = PF_FW_ARQH_ARQH_M,
			}
		}
	};
	struct idpf_ctlq_info *ctlq;
	int ret;

	ret = idpf_ctlq_init(hw, CTLQ_NUM, ctlq_info);
	if (ret != 0)
		return ret;

	LIST_FOR_EACH_ENTRY_SAFE(ctlq, NULL, &hw->cq_list_head,
				 struct idpf_ctlq_info, cq_list) {
		if (ctlq->q_id == IDPF_CTLQ_ID &&
		    ctlq->cq_type == IDPF_CTLQ_TYPE_MAILBOX_TX)
			hw->asq = ctlq;
		if (ctlq->q_id == IDPF_CTLQ_ID &&
		    ctlq->cq_type == IDPF_CTLQ_TYPE_MAILBOX_RX)
			hw->arq = ctlq;
	}

	if (hw->asq == NULL || hw->arq == NULL) {
		idpf_ctlq_deinit(hw);
		ret = -ENOENT;
	}

	return ret;
}

int
idpf_adapter_init(struct idpf_adapter *adapter)
{
	struct idpf_hw *hw = &adapter->hw;
	int ret;

	idpf_reset_pf(hw);
	ret = idpf_check_pf_reset_done(hw);
	if (ret != 0) {
		DRV_LOG(ERR, "IDPF is still resetting");
		goto err_check_reset;
	}

	ret = idpf_init_mbx(hw);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to init mailbox");
		goto err_check_reset;
	}

	adapter->mbx_resp = rte_zmalloc("idpf_adapter_mbx_resp",
					IDPF_DFLT_MBX_BUF_SIZE, 0);
	if (adapter->mbx_resp == NULL) {
		DRV_LOG(ERR, "Failed to allocate idpf_adapter_mbx_resp memory");
		ret = -ENOMEM;
		goto err_mbx_resp;
	}

	ret = idpf_vc_check_api_version(adapter);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to check api version");
		goto err_check_api;
	}

	ret = idpf_vc_get_caps(adapter);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to get capabilities");
		goto err_check_api;
	}

	return 0;

err_check_api:
	rte_free(adapter->mbx_resp);
	adapter->mbx_resp = NULL;
err_mbx_resp:
	idpf_ctlq_deinit(hw);
err_check_reset:
	return ret;
}

int
idpf_adapter_deinit(struct idpf_adapter *adapter)
{
	struct idpf_hw *hw = &adapter->hw;

	idpf_ctlq_deinit(hw);
	rte_free(adapter->mbx_resp);
	adapter->mbx_resp = NULL;

	return 0;
}

int
idpf_vport_init(struct idpf_vport *vport,
		struct virtchnl2_create_vport *create_vport_info,
		void *dev_data)
{
	struct virtchnl2_create_vport *vport_info;
	int i, type, ret;

	ret = idpf_vc_create_vport(vport, create_vport_info);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to create vport.");
		goto err_create_vport;
	}

	vport_info = &(vport->vport_info.info);
	vport->vport_id = vport_info->vport_id;
	vport->txq_model = vport_info->txq_model;
	vport->rxq_model = vport_info->rxq_model;
	vport->num_tx_q = vport_info->num_tx_q;
	vport->num_tx_complq = vport_info->num_tx_complq;
	vport->num_rx_q = vport_info->num_rx_q;
	vport->num_rx_bufq = vport_info->num_rx_bufq;
	vport->max_mtu = vport_info->max_mtu;
	rte_memcpy(vport->default_mac_addr,
		   vport_info->default_mac_addr, ETH_ALEN);
	vport->rss_algorithm = vport_info->rss_algorithm;
	vport->rss_key_size = RTE_MIN(IDPF_RSS_KEY_LEN,
				      vport_info->rss_key_size);
	vport->rss_lut_size = vport_info->rss_lut_size;

	for (i = 0; i < vport_info->chunks.num_chunks; i++) {
		type = vport_info->chunks.chunks[i].type;
		switch (type) {
		case VIRTCHNL2_QUEUE_TYPE_TX:
			vport->chunks_info.tx_start_qid =
				vport_info->chunks.chunks[i].start_queue_id;
			vport->chunks_info.tx_qtail_start =
				vport_info->chunks.chunks[i].qtail_reg_start;
			vport->chunks_info.tx_qtail_spacing =
				vport_info->chunks.chunks[i].qtail_reg_spacing;
			break;
		case VIRTCHNL2_QUEUE_TYPE_RX:
			vport->chunks_info.rx_start_qid =
				vport_info->chunks.chunks[i].start_queue_id;
			vport->chunks_info.rx_qtail_start =
				vport_info->chunks.chunks[i].qtail_reg_start;
			vport->chunks_info.rx_qtail_spacing =
				vport_info->chunks.chunks[i].qtail_reg_spacing;
			break;
		case VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION:
			vport->chunks_info.tx_compl_start_qid =
				vport_info->chunks.chunks[i].start_queue_id;
			vport->chunks_info.tx_compl_qtail_start =
				vport_info->chunks.chunks[i].qtail_reg_start;
			vport->chunks_info.tx_compl_qtail_spacing =
				vport_info->chunks.chunks[i].qtail_reg_spacing;
			break;
		case VIRTCHNL2_QUEUE_TYPE_RX_BUFFER:
			vport->chunks_info.rx_buf_start_qid =
				vport_info->chunks.chunks[i].start_queue_id;
			vport->chunks_info.rx_buf_qtail_start =
				vport_info->chunks.chunks[i].qtail_reg_start;
			vport->chunks_info.rx_buf_qtail_spacing =
				vport_info->chunks.chunks[i].qtail_reg_spacing;
			break;
		default:
			DRV_LOG(ERR, "Unsupported queue type");
			break;
		}
	}

	vport->dev_data = dev_data;

	vport->rss_key = rte_zmalloc("rss_key",
				     vport->rss_key_size, 0);
	if (vport->rss_key == NULL) {
		DRV_LOG(ERR, "Failed to allocate RSS key");
		ret = -ENOMEM;
		goto err_rss_key;
	}

	vport->rss_lut = rte_zmalloc("rss_lut",
				     sizeof(uint32_t) * vport->rss_lut_size, 0);
	if (vport->rss_lut == NULL) {
		DRV_LOG(ERR, "Failed to allocate RSS lut");
		ret = -ENOMEM;
		goto err_rss_lut;
	}

	return 0;

err_rss_lut:
	vport->dev_data = NULL;
	rte_free(vport->rss_key);
	vport->rss_key = NULL;
err_rss_key:
	idpf_vc_destroy_vport(vport);
err_create_vport:
	return ret;
}
int
idpf_vport_deinit(struct idpf_vport *vport)
{
	rte_free(vport->rss_lut);
	vport->rss_lut = NULL;

	rte_free(vport->rss_key);
	vport->rss_key = NULL;

	vport->dev_data = NULL;

	idpf_vc_destroy_vport(vport);

	return 0;
}
RTE_LOG_REGISTER_SUFFIX(idpf_common_logtype, common, NOTICE);
