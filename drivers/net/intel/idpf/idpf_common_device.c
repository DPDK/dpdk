/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <eal_export.h>
#include <rte_log.h>
#include "idpf_common_device.h"
#include "idpf_common_virtchnl.h"

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

static int
idpf_check_vf_reset_done(struct idpf_hw *hw)
{
	uint32_t reg;
	int i;

	for (i = 0; i < IDPF_RESET_WAIT_CNT; i++) {
		reg = IDPF_READ_REG(hw, VFGEN_RSTAT);
		if (reg != 0xFFFFFFFF && (reg & VFGEN_RSTAT_VFR_STATE_M))
			return 0;
		rte_delay_ms(1000);
	}

	DRV_LOG(ERR, "VF reset timeout");
	return -EBUSY;
}

#define IDPF_CTLQ_NUM 2

struct idpf_ctlq_create_info pf_ctlq_info[IDPF_CTLQ_NUM] = {
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

struct idpf_ctlq_create_info vf_ctlq_info[IDPF_CTLQ_NUM] = {
	{
		.type = IDPF_CTLQ_TYPE_MAILBOX_TX,
		.id = IDPF_CTLQ_ID,
		.len = IDPF_CTLQ_LEN,
		.buf_size = IDPF_DFLT_MBX_BUF_SIZE,
		.reg = {
			.head = VF_ATQH,
			.tail = VF_ATQT,
			.len = VF_ATQLEN,
			.bah = VF_ATQBAH,
			.bal = VF_ATQBAL,
			.len_mask = VF_ATQLEN_ATQLEN_M,
			.len_ena_mask = VF_ATQLEN_ATQENABLE_M,
			.head_mask = VF_ATQH_ATQH_M,
		}
	},
	{
		.type = IDPF_CTLQ_TYPE_MAILBOX_RX,
		.id = IDPF_CTLQ_ID,
		.len = IDPF_CTLQ_LEN,
		.buf_size = IDPF_DFLT_MBX_BUF_SIZE,
		.reg = {
			.head = VF_ARQH,
			.tail = VF_ARQT,
			.len = VF_ARQLEN,
			.bah = VF_ARQBAH,
			.bal = VF_ARQBAL,
			.len_mask = VF_ARQLEN_ARQLEN_M,
			.len_ena_mask = VF_ARQLEN_ARQENABLE_M,
			.head_mask = VF_ARQH_ARQH_M,
		}
	}
};

static int
idpf_init_mbx(struct idpf_hw *hw)
{
	struct idpf_ctlq_info *ctlq;
	int ret = 0;

	if (idpf_is_vf_device(hw))
		ret = idpf_ctlq_init(hw, IDPF_CTLQ_NUM, vf_ctlq_info);
	else
		ret = idpf_ctlq_init(hw, IDPF_CTLQ_NUM, pf_ctlq_info);
	if (ret != 0)
		return ret;

	LIST_FOR_EACH_ENTRY(ctlq, &hw->cq_list_head, struct idpf_ctlq_info, cq_list) {
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

static int
idpf_get_pkt_type(struct idpf_adapter *adapter)
{
	struct virtchnl2_get_ptype_info *req_ptype_info;
	struct virtchnl2_get_ptype_info *recv_ptype_info;
	uint16_t recv_num_ptypes = 0;
	uint16_t ptype_offset, i, j;
	uint16_t start_ptype_id = 0;
	int ret;

	req_ptype_info = rte_zmalloc("req_ptype_info", IDPF_DFLT_MBX_BUF_SIZE, 0);
	if (req_ptype_info == NULL)
		return -ENOMEM;

	recv_ptype_info = rte_zmalloc("recv_ptype_info", IDPF_DFLT_MBX_BUF_SIZE, 0);
	if (recv_ptype_info == NULL) {
		ret = -ENOMEM;
		goto free_req_ptype_info;
	}

	while (start_ptype_id < IDPF_MAX_PKT_TYPE) {
		memset(req_ptype_info, 0, sizeof(*req_ptype_info));
		memset(recv_ptype_info, 0, sizeof(*recv_ptype_info));

		if ((start_ptype_id + IDPF_RX_MAX_PTYPES_PER_BUF) > IDPF_MAX_PKT_TYPE)
			req_ptype_info->num_ptypes =
				rte_cpu_to_le_16(IDPF_MAX_PKT_TYPE - start_ptype_id);
		else
			req_ptype_info->num_ptypes = rte_cpu_to_le_16(IDPF_RX_MAX_PTYPES_PER_BUF);
		req_ptype_info->start_ptype_id = start_ptype_id;

		ret = idpf_vc_ptype_info_query(adapter, req_ptype_info, recv_ptype_info);
		if (ret != 0) {
			DRV_LOG(ERR, "Fail to query packet type information");
			goto free_recv_ptype_info;
		}

		recv_num_ptypes += rte_le_to_cpu_16(recv_ptype_info->num_ptypes);
		if (recv_num_ptypes > IDPF_MAX_PKT_TYPE) {
			ret = -EINVAL;
			goto free_recv_ptype_info;
		}

		start_ptype_id = rte_le_to_cpu_16(req_ptype_info->start_ptype_id) +
			rte_le_to_cpu_16(req_ptype_info->num_ptypes);

		ptype_offset = sizeof(struct virtchnl2_get_ptype_info) -
						sizeof(struct virtchnl2_ptype);

		for (i = 0; i < rte_le_to_cpu_16(recv_ptype_info->num_ptypes); i++) {
			bool is_inner = false, is_ip = false;
			struct virtchnl2_ptype *ptype;
			uint32_t proto_hdr = 0;

			ptype = (struct virtchnl2_ptype *)
					((uint8_t *)recv_ptype_info + ptype_offset);
			ptype_offset += IDPF_GET_PTYPE_SIZE(ptype);
			if (ptype_offset > IDPF_DFLT_MBX_BUF_SIZE) {
				ret = -EINVAL;
				goto free_recv_ptype_info;
			}

			for (j = 0; j < ptype->proto_id_count; j++) {
				switch (rte_le_to_cpu_16(ptype->proto_id[j])) {
				case VIRTCHNL2_PROTO_HDR_GRE:
				case VIRTCHNL2_PROTO_HDR_VXLAN:
					proto_hdr &= ~RTE_PTYPE_L4_MASK;
					proto_hdr |= RTE_PTYPE_TUNNEL_GRENAT;
					is_inner = true;
					break;
				case VIRTCHNL2_PROTO_HDR_MAC:
					if (is_inner) {
						proto_hdr &= ~RTE_PTYPE_INNER_L2_MASK;
						proto_hdr |= RTE_PTYPE_INNER_L2_ETHER;
					} else {
						proto_hdr &= ~RTE_PTYPE_L2_MASK;
						proto_hdr |= RTE_PTYPE_L2_ETHER;
					}
					break;
				case VIRTCHNL2_PROTO_HDR_VLAN:
					if (is_inner) {
						proto_hdr &= ~RTE_PTYPE_INNER_L2_MASK;
						proto_hdr |= RTE_PTYPE_INNER_L2_ETHER_VLAN;
					}
					break;
				case VIRTCHNL2_PROTO_HDR_PTP:
					proto_hdr &= ~RTE_PTYPE_L2_MASK;
					proto_hdr |= RTE_PTYPE_L2_ETHER_TIMESYNC;
					break;
				case VIRTCHNL2_PROTO_HDR_LLDP:
					proto_hdr &= ~RTE_PTYPE_L2_MASK;
					proto_hdr |= RTE_PTYPE_L2_ETHER_LLDP;
					break;
				case VIRTCHNL2_PROTO_HDR_ARP:
					proto_hdr &= ~RTE_PTYPE_L2_MASK;
					proto_hdr |= RTE_PTYPE_L2_ETHER_ARP;
					break;
				case VIRTCHNL2_PROTO_HDR_PPPOE:
					proto_hdr &= ~RTE_PTYPE_L2_MASK;
					proto_hdr |= RTE_PTYPE_L2_ETHER_PPPOE;
					break;
				case VIRTCHNL2_PROTO_HDR_IPV4:
					if (!is_ip) {
						proto_hdr |= RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
						is_ip = true;
					} else {
						proto_hdr |= RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
							     RTE_PTYPE_TUNNEL_IP;
						is_inner = true;
					}
						break;
				case VIRTCHNL2_PROTO_HDR_IPV6:
					if (!is_ip) {
						proto_hdr |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
						is_ip = true;
					} else {
						proto_hdr |= RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
							     RTE_PTYPE_TUNNEL_IP;
						is_inner = true;
					}
					break;
				case VIRTCHNL2_PROTO_HDR_IPV4_FRAG:
				case VIRTCHNL2_PROTO_HDR_IPV6_FRAG:
					if (is_inner)
						proto_hdr |= RTE_PTYPE_INNER_L4_FRAG;
					else
						proto_hdr |= RTE_PTYPE_L4_FRAG;
					break;
				case VIRTCHNL2_PROTO_HDR_UDP:
					if (is_inner)
						proto_hdr |= RTE_PTYPE_INNER_L4_UDP;
					else
						proto_hdr |= RTE_PTYPE_L4_UDP;
					break;
				case VIRTCHNL2_PROTO_HDR_TCP:
					if (is_inner)
						proto_hdr |= RTE_PTYPE_INNER_L4_TCP;
					else
						proto_hdr |= RTE_PTYPE_L4_TCP;
					break;
				case VIRTCHNL2_PROTO_HDR_SCTP:
					if (is_inner)
						proto_hdr |= RTE_PTYPE_INNER_L4_SCTP;
					else
						proto_hdr |= RTE_PTYPE_L4_SCTP;
					break;
				case VIRTCHNL2_PROTO_HDR_ICMP:
					if (is_inner)
						proto_hdr |= RTE_PTYPE_INNER_L4_ICMP;
					else
						proto_hdr |= RTE_PTYPE_L4_ICMP;
					break;
				case VIRTCHNL2_PROTO_HDR_ICMPV6:
					if (is_inner)
						proto_hdr |= RTE_PTYPE_INNER_L4_ICMP;
					else
						proto_hdr |= RTE_PTYPE_L4_ICMP;
					break;
				case VIRTCHNL2_PROTO_HDR_L2TPV2:
				case VIRTCHNL2_PROTO_HDR_L2TPV2_CONTROL:
				case VIRTCHNL2_PROTO_HDR_L2TPV3:
					is_inner = true;
					proto_hdr |= RTE_PTYPE_TUNNEL_L2TP;
					break;
				case VIRTCHNL2_PROTO_HDR_NVGRE:
					is_inner = true;
					proto_hdr |= RTE_PTYPE_TUNNEL_NVGRE;
					break;
				case VIRTCHNL2_PROTO_HDR_GTPC_TEID:
					is_inner = true;
					proto_hdr |= RTE_PTYPE_TUNNEL_GTPC;
					break;
				case VIRTCHNL2_PROTO_HDR_GTPU:
				case VIRTCHNL2_PROTO_HDR_GTPU_UL:
				case VIRTCHNL2_PROTO_HDR_GTPU_DL:
					is_inner = true;
					proto_hdr |= RTE_PTYPE_TUNNEL_GTPU;
					break;
				case VIRTCHNL2_PROTO_HDR_PAY:
				case VIRTCHNL2_PROTO_HDR_IPV6_EH:
				case VIRTCHNL2_PROTO_HDR_PRE_MAC:
				case VIRTCHNL2_PROTO_HDR_POST_MAC:
				case VIRTCHNL2_PROTO_HDR_ETHERTYPE:
				case VIRTCHNL2_PROTO_HDR_SVLAN:
				case VIRTCHNL2_PROTO_HDR_CVLAN:
				case VIRTCHNL2_PROTO_HDR_MPLS:
				case VIRTCHNL2_PROTO_HDR_MMPLS:
				case VIRTCHNL2_PROTO_HDR_CTRL:
				case VIRTCHNL2_PROTO_HDR_ECP:
				case VIRTCHNL2_PROTO_HDR_EAPOL:
				case VIRTCHNL2_PROTO_HDR_PPPOD:
				case VIRTCHNL2_PROTO_HDR_IGMP:
				case VIRTCHNL2_PROTO_HDR_AH:
				case VIRTCHNL2_PROTO_HDR_ESP:
				case VIRTCHNL2_PROTO_HDR_IKE:
				case VIRTCHNL2_PROTO_HDR_NATT_KEEP:
				case VIRTCHNL2_PROTO_HDR_GTP:
				case VIRTCHNL2_PROTO_HDR_GTP_EH:
				case VIRTCHNL2_PROTO_HDR_GTPCV2:
				case VIRTCHNL2_PROTO_HDR_ECPRI:
				case VIRTCHNL2_PROTO_HDR_VRRP:
				case VIRTCHNL2_PROTO_HDR_OSPF:
				case VIRTCHNL2_PROTO_HDR_TUN:
				case VIRTCHNL2_PROTO_HDR_VXLAN_GPE:
				case VIRTCHNL2_PROTO_HDR_GENEVE:
				case VIRTCHNL2_PROTO_HDR_NSH:
				case VIRTCHNL2_PROTO_HDR_QUIC:
				case VIRTCHNL2_PROTO_HDR_PFCP:
				case VIRTCHNL2_PROTO_HDR_PFCP_NODE:
				case VIRTCHNL2_PROTO_HDR_PFCP_SESSION:
				case VIRTCHNL2_PROTO_HDR_RTP:
				case VIRTCHNL2_PROTO_HDR_NO_PROTO:
				default:
					continue;
				}
				adapter->ptype_tbl[ptype->ptype_id_10] = proto_hdr;
			}
		}
	}

free_recv_ptype_info:
	rte_free(recv_ptype_info);
free_req_ptype_info:
	rte_free(req_ptype_info);
	clear_cmd(adapter);
	return ret;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_adapter_init)
int
idpf_adapter_init(struct idpf_adapter *adapter)
{
	struct idpf_hw *hw = &adapter->hw;
	int ret;

	if (idpf_is_vf_device(hw)) {
		ret = idpf_check_vf_reset_done(hw);
	} else {
		idpf_reset_pf(hw);
		ret = idpf_check_pf_reset_done(hw);
	}
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

	ret = idpf_vc_api_version_check(adapter);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to check api version");
		goto err_check_api;
	}

	ret = idpf_vc_caps_get(adapter);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to get capabilities");
		goto err_check_api;
	}

	ret = idpf_get_pkt_type(adapter);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to set ptype table");
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

#define IDPF_VF_TEST_VAL		0xFEED0000

/**
 * idpf_is_vf_device - Helper to find if it is a VF/PF device
 * @hw: idpf_hw struct
 *
 * Return: 1 for VF device, 0 for PF device.
 */
bool idpf_is_vf_device(struct idpf_hw *hw)
{
	if (hw->device_id == IDPF_DEV_ID_SRIOV  || hw->device_id == IXD_DEV_ID_VCPF)
		return 1;
	IDPF_WRITE_REG(hw, VF_ARQBAL, IDPF_VF_TEST_VAL);
	return IDPF_READ_REG(hw, VF_ARQBAL) == IDPF_VF_TEST_VAL;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_adapter_deinit)
int
idpf_adapter_deinit(struct idpf_adapter *adapter)
{
	struct idpf_hw *hw = &adapter->hw;

	idpf_ctlq_deinit(hw);
	rte_free(adapter->mbx_resp);
	adapter->mbx_resp = NULL;

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vport_init)
int
idpf_vport_init(struct idpf_vport *vport,
		struct virtchnl2_create_vport *create_vport_info,
		void *dev_data)
{
	struct virtchnl2_create_vport *vport_info;
	int i, type, ret;

	ret = idpf_vc_vport_create(vport, create_vport_info);
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

	/* recv_vectors is used for VIRTCHNL2_OP_ALLOC_VECTORS response,
	 * reserve maximum size for it now, may need optimization in future.
	 */
	vport->recv_vectors = rte_zmalloc("recv_vectors", IDPF_DFLT_MBX_BUF_SIZE, 0);
	if (vport->recv_vectors == NULL) {
		DRV_LOG(ERR, "Failed to allocate recv_vectors");
		ret = -ENOMEM;
		goto err_recv_vec;
	}

	return 0;

err_recv_vec:
	rte_free(vport->rss_lut);
	vport->rss_lut = NULL;
err_rss_lut:
	vport->dev_data = NULL;
	rte_free(vport->rss_key);
	vport->rss_key = NULL;
err_rss_key:
	idpf_vc_vport_destroy(vport);
err_create_vport:
	return ret;
}
RTE_EXPORT_INTERNAL_SYMBOL(idpf_vport_deinit)
int
idpf_vport_deinit(struct idpf_vport *vport)
{
	rte_free(vport->recv_vectors);
	vport->recv_vectors = NULL;
	rte_free(vport->rss_lut);
	vport->rss_lut = NULL;

	rte_free(vport->rss_key);
	vport->rss_key = NULL;

	vport->dev_data = NULL;

	idpf_vc_vport_destroy(vport);

	return 0;
}
RTE_EXPORT_INTERNAL_SYMBOL(idpf_vport_rss_config)
int
idpf_vport_rss_config(struct idpf_vport *vport)
{
	int ret;

	ret = idpf_vc_rss_key_set(vport);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to configure RSS key");
		return ret;
	}

	ret = idpf_vc_rss_lut_set(vport);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to configure RSS lut");
		return ret;
	}

	ret = idpf_vc_rss_hash_set(vport);
	if (ret != 0) {
		DRV_LOG(ERR, "Failed to configure RSS hash");
		return ret;
	}

	return ret;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vport_irq_map_config)
int
idpf_vport_irq_map_config(struct idpf_vport *vport, uint16_t nb_rx_queues)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_queue_vector *qv_map;
	struct idpf_hw *hw = &adapter->hw;
	uint32_t dynctl_val, itrn_val;
	uint32_t dynctl_reg_start;
	uint32_t itrn_reg_start;
	uint16_t i;
	int ret;

	qv_map = rte_zmalloc("qv_map",
			     nb_rx_queues *
			     sizeof(struct virtchnl2_queue_vector), 0);
	if (qv_map == NULL) {
		DRV_LOG(ERR, "Failed to allocate %d queue-vector map",
			nb_rx_queues);
		ret = -ENOMEM;
		goto qv_map_alloc_err;
	}

	/* Rx interrupt disabled, Map interrupt only for writeback */

	/* The capability flags adapter->caps.other_caps should be
	 * compared with bit VIRTCHNL2_CAP_WB_ON_ITR here. The if
	 * condition should be updated when the FW can return the
	 * correct flag bits.
	 */
	dynctl_reg_start =
		vport->recv_vectors->vchunks.vchunks->dynctl_reg_start;
	itrn_reg_start =
		vport->recv_vectors->vchunks.vchunks->itrn_reg_start;
	dynctl_val = IDPF_READ_REG(hw, dynctl_reg_start);
	DRV_LOG(DEBUG, "Value of dynctl_reg_start is 0x%x", dynctl_val);
	itrn_val = IDPF_READ_REG(hw, itrn_reg_start);
	DRV_LOG(DEBUG, "Value of itrn_reg_start is 0x%x", itrn_val);
	/* Force write-backs by setting WB_ON_ITR bit in DYN_CTL
	 * register. WB_ON_ITR and INTENA are mutually exclusive
	 * bits. Setting WB_ON_ITR bits means TX and RX Descs
	 * are written back based on ITR expiration irrespective
	 * of INTENA setting.
	 */
	/* TBD: need to tune INTERVAL value for better performance. */
	itrn_val = (itrn_val == 0) ? IDPF_DFLT_INTERVAL : itrn_val;
	dynctl_val = VIRTCHNL2_ITR_IDX_0  <<
		     PF_GLINT_DYN_CTL_ITR_INDX_S |
		     PF_GLINT_DYN_CTL_WB_ON_ITR_M |
		     itrn_val << PF_GLINT_DYN_CTL_INTERVAL_S;
	IDPF_WRITE_REG(hw, dynctl_reg_start, dynctl_val);

	for (i = 0; i < nb_rx_queues; i++) {
		/* map all queues to the same vector */
		qv_map[i].queue_id = vport->chunks_info.rx_start_qid + i;
		qv_map[i].vector_id =
			vport->recv_vectors->vchunks.vchunks->start_vector_id;
	}
	vport->qv_map = qv_map;

	ret = idpf_vc_irq_map_unmap_config(vport, nb_rx_queues, true);
	if (ret != 0) {
		DRV_LOG(ERR, "config interrupt mapping failed");
		goto config_irq_map_err;
	}

	return 0;

config_irq_map_err:
	rte_free(vport->qv_map);
	vport->qv_map = NULL;

qv_map_alloc_err:
	return ret;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vport_irq_map_config_by_qids)
int
idpf_vport_irq_map_config_by_qids(struct idpf_vport *vport, uint32_t *qids, uint16_t nb_rx_queues)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_queue_vector *qv_map;
	struct idpf_hw *hw = &adapter->hw;
	uint32_t dynctl_val, itrn_val;
	uint32_t dynctl_reg_start;
	uint32_t itrn_reg_start;
	uint16_t i;
	int ret;

	qv_map = rte_zmalloc("qv_map",
			     nb_rx_queues *
			     sizeof(struct virtchnl2_queue_vector), 0);
	if (qv_map == NULL) {
		DRV_LOG(ERR, "Failed to allocate %d queue-vector map",
			nb_rx_queues);
		ret = -ENOMEM;
		goto qv_map_alloc_err;
	}

	/* Rx interrupt disabled, Map interrupt only for writeback */

	/* The capability flags adapter->caps.other_caps should be
	 * compared with bit VIRTCHNL2_CAP_WB_ON_ITR here. The if
	 * condition should be updated when the FW can return the
	 * correct flag bits.
	 */
	dynctl_reg_start =
		vport->recv_vectors->vchunks.vchunks->dynctl_reg_start;
	itrn_reg_start =
		vport->recv_vectors->vchunks.vchunks->itrn_reg_start;
	dynctl_val = IDPF_READ_REG(hw, dynctl_reg_start);
	DRV_LOG(DEBUG, "Value of dynctl_reg_start is 0x%x", dynctl_val);
	itrn_val = IDPF_READ_REG(hw, itrn_reg_start);
	DRV_LOG(DEBUG, "Value of itrn_reg_start is 0x%x", itrn_val);
	/* Force write-backs by setting WB_ON_ITR bit in DYN_CTL
	 * register. WB_ON_ITR and INTENA are mutually exclusive
	 * bits. Setting WB_ON_ITR bits means TX and RX Descs
	 * are written back based on ITR expiration irrespective
	 * of INTENA setting.
	 */
	/* TBD: need to tune INTERVAL value for better performance. */
	itrn_val = (itrn_val == 0) ? IDPF_DFLT_INTERVAL : itrn_val;
	dynctl_val = VIRTCHNL2_ITR_IDX_0  <<
		     PF_GLINT_DYN_CTL_ITR_INDX_S |
		     PF_GLINT_DYN_CTL_WB_ON_ITR_M |
		     itrn_val << PF_GLINT_DYN_CTL_INTERVAL_S;
	IDPF_WRITE_REG(hw, dynctl_reg_start, dynctl_val);

	for (i = 0; i < nb_rx_queues; i++) {
		/* map all queues to the same vector */
		qv_map[i].queue_id = qids[i];
		qv_map[i].vector_id =
			vport->recv_vectors->vchunks.vchunks->start_vector_id;
	}
	vport->qv_map = qv_map;

	ret = idpf_vc_irq_map_unmap_config(vport, nb_rx_queues, true);
	if (ret != 0) {
		DRV_LOG(ERR, "config interrupt mapping failed");
		goto config_irq_map_err;
	}

	return 0;

config_irq_map_err:
	rte_free(vport->qv_map);
	vport->qv_map = NULL;

qv_map_alloc_err:
	return ret;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vport_irq_unmap_config)
int
idpf_vport_irq_unmap_config(struct idpf_vport *vport, uint16_t nb_rx_queues)
{
	idpf_vc_irq_map_unmap_config(vport, nb_rx_queues, false);

	rte_free(vport->qv_map);
	vport->qv_map = NULL;

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vport_info_init)
int
idpf_vport_info_init(struct idpf_vport *vport,
			    struct virtchnl2_create_vport *vport_info)
{
	struct idpf_adapter *adapter = vport->adapter;

	vport_info->vport_type = rte_cpu_to_le_16(VIRTCHNL2_VPORT_TYPE_DEFAULT);
	if (!adapter->is_tx_singleq) {
		vport_info->txq_model =
			rte_cpu_to_le_16(VIRTCHNL2_QUEUE_MODEL_SPLIT);
		vport_info->num_tx_q =
			rte_cpu_to_le_16(IDPF_DEFAULT_TXQ_NUM);
		vport_info->num_tx_complq =
			rte_cpu_to_le_16(IDPF_DEFAULT_TXQ_NUM * IDPF_TX_COMPLQ_PER_GRP);
	} else {
		vport_info->txq_model =
			rte_cpu_to_le_16(VIRTCHNL2_QUEUE_MODEL_SINGLE);
		vport_info->num_tx_q = rte_cpu_to_le_16(IDPF_DEFAULT_TXQ_NUM);
		vport_info->num_tx_complq = 0;
	}
	if (!adapter->is_rx_singleq) {
		vport_info->rxq_model =
			rte_cpu_to_le_16(VIRTCHNL2_QUEUE_MODEL_SPLIT);
		vport_info->num_rx_q = rte_cpu_to_le_16(IDPF_DEFAULT_RXQ_NUM);
		vport_info->num_rx_bufq =
			rte_cpu_to_le_16(IDPF_DEFAULT_RXQ_NUM * IDPF_RX_BUFQ_PER_GRP);
	} else {
		vport_info->rxq_model =
			rte_cpu_to_le_16(VIRTCHNL2_QUEUE_MODEL_SINGLE);
		vport_info->num_rx_q = rte_cpu_to_le_16(IDPF_DEFAULT_RXQ_NUM);
		vport_info->num_rx_bufq = 0;
	}

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_vport_stats_update)
void
idpf_vport_stats_update(struct virtchnl2_vport_stats *oes, struct virtchnl2_vport_stats *nes)
{
	nes->rx_bytes = nes->rx_bytes - oes->rx_bytes;
	nes->rx_unicast = nes->rx_unicast - oes->rx_unicast;
	nes->rx_multicast = nes->rx_multicast - oes->rx_multicast;
	nes->rx_broadcast = nes->rx_broadcast - oes->rx_broadcast;
	nes->rx_errors = nes->rx_errors - oes->rx_errors;
	nes->rx_discards = nes->rx_discards - oes->rx_discards;
	nes->tx_bytes = nes->tx_bytes - oes->tx_bytes;
	nes->tx_unicast = nes->tx_unicast - oes->tx_unicast;
	nes->tx_multicast = nes->tx_multicast - oes->tx_multicast;
	nes->tx_broadcast = nes->tx_broadcast - oes->tx_broadcast;
	nes->tx_errors = nes->tx_errors - oes->tx_errors;
	nes->tx_discards = nes->tx_discards - oes->tx_discards;
}

RTE_LOG_REGISTER_SUFFIX(idpf_common_logtype, common, NOTICE);
