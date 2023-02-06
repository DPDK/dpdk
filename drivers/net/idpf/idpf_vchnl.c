/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
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
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_dev.h>

#include "idpf_ethdev.h"
#include "idpf_rxtx.h"

int __rte_cold
idpf_get_pkt_type(struct idpf_adapter_ext *adapter)
{
	struct virtchnl2_get_ptype_info *ptype_info;
	struct idpf_adapter *base;
	uint16_t ptype_offset, i, j;
	uint16_t ptype_recvd = 0;
	int ret;

	base = &adapter->base;

	ret = idpf_vc_query_ptype_info(base);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Fail to query packet type information");
		return ret;
	}

	ptype_info = rte_zmalloc("ptype_info", IDPF_DFLT_MBX_BUF_SIZE, 0);
		if (ptype_info == NULL)
			return -ENOMEM;

	while (ptype_recvd < IDPF_MAX_PKT_TYPE) {
		ret = idpf_vc_read_one_msg(base, VIRTCHNL2_OP_GET_PTYPE_INFO,
					   IDPF_DFLT_MBX_BUF_SIZE, (uint8_t *)ptype_info);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Fail to get packet type information");
			goto free_ptype_info;
		}

		ptype_recvd += ptype_info->num_ptypes;
		ptype_offset = sizeof(struct virtchnl2_get_ptype_info) -
						sizeof(struct virtchnl2_ptype);

		for (i = 0; i < rte_cpu_to_le_16(ptype_info->num_ptypes); i++) {
			bool is_inner = false, is_ip = false;
			struct virtchnl2_ptype *ptype;
			uint32_t proto_hdr = 0;

			ptype = (struct virtchnl2_ptype *)
					((uint8_t *)ptype_info + ptype_offset);
			ptype_offset += IDPF_GET_PTYPE_SIZE(ptype);
			if (ptype_offset > IDPF_DFLT_MBX_BUF_SIZE) {
				ret = -EINVAL;
				goto free_ptype_info;
			}

			if (rte_cpu_to_le_16(ptype->ptype_id_10) == 0xFFFF)
				goto free_ptype_info;

			for (j = 0; j < ptype->proto_id_count; j++) {
				switch (rte_cpu_to_le_16(ptype->proto_id[j])) {
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

free_ptype_info:
	rte_free(ptype_info);
	clear_cmd(base);
	return ret;
}

#define IDPF_RX_BUF_STRIDE		64
int
idpf_vc_config_rxq(struct idpf_vport *vport, struct idpf_rx_queue *rxq)
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
		PMD_DRV_LOG(ERR, "Failed to allocate virtchnl2_config_rx_queues");
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

	err = idpf_execute_vc_cmd(adapter, &args);
	rte_free(vc_rxqs);
	if (err != 0)
		PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_RX_QUEUES");

	return err;
}

int
idpf_vc_config_txq(struct idpf_vport *vport, struct idpf_tx_queue *txq)
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
		PMD_DRV_LOG(ERR, "Failed to allocate virtchnl2_config_tx_queues");
		err = -ENOMEM;
		return err;
	}
	vc_txqs->vport_id = vport->vport_id;
	vc_txqs->num_qinfo = num_qs;

	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		txq_info = &vc_txqs->qinfo[0];
		txq_info->dma_ring_addr = txq->tx_ring_phys_addr;
		txq_info->type = VIRTCHNL2_QUEUE_TYPE_TX;
		txq_info->queue_id = txq->queue_id;
		txq_info->model = VIRTCHNL2_QUEUE_MODEL_SINGLE;
		txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_QUEUE;
		txq_info->ring_len = txq->nb_tx_desc;
	} else {
		/* txq info */
		txq_info = &vc_txqs->qinfo[0];
		txq_info->dma_ring_addr = txq->tx_ring_phys_addr;
		txq_info->type = VIRTCHNL2_QUEUE_TYPE_TX;
		txq_info->queue_id = txq->queue_id;
		txq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
		txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_FLOW;
		txq_info->ring_len = txq->nb_tx_desc;
		txq_info->tx_compl_queue_id = txq->complq->queue_id;
		txq_info->relative_queue_id = txq_info->queue_id;

		/* tx completion queue info */
		txq_info = &vc_txqs->qinfo[1];
		txq_info->dma_ring_addr = txq->complq->tx_ring_phys_addr;
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

	err = idpf_execute_vc_cmd(adapter, &args);
	rte_free(vc_txqs);
	if (err != 0)
		PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_TX_QUEUES");

	return err;
}
