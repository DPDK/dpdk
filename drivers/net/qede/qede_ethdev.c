/*
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#include "qede_ethdev.h"
#include <rte_alarm.h>
#include <rte_version.h>
#include <rte_kvargs.h>

/* Globals */
int qede_logtype_init;
int qede_logtype_driver;

static const struct qed_eth_ops *qed_ops;
#define QEDE_SP_TIMER_PERIOD	10000 /* 100ms */

/* VXLAN tunnel classification mapping */
const struct _qede_udp_tunn_types {
	uint16_t rte_filter_type;
	enum ecore_filter_ucast_type qede_type;
	enum ecore_tunn_clss qede_tunn_clss;
	const char *string;
} qede_tunn_types[] = {
	{
		ETH_TUNNEL_FILTER_OMAC,
		ECORE_FILTER_MAC,
		ECORE_TUNN_CLSS_MAC_VLAN,
		"outer-mac"
	},
	{
		ETH_TUNNEL_FILTER_TENID,
		ECORE_FILTER_VNI,
		ECORE_TUNN_CLSS_MAC_VNI,
		"vni"
	},
	{
		ETH_TUNNEL_FILTER_IMAC,
		ECORE_FILTER_INNER_MAC,
		ECORE_TUNN_CLSS_INNER_MAC_VLAN,
		"inner-mac"
	},
	{
		ETH_TUNNEL_FILTER_IVLAN,
		ECORE_FILTER_INNER_VLAN,
		ECORE_TUNN_CLSS_INNER_MAC_VLAN,
		"inner-vlan"
	},
	{
		ETH_TUNNEL_FILTER_OMAC | ETH_TUNNEL_FILTER_TENID,
		ECORE_FILTER_MAC_VNI_PAIR,
		ECORE_TUNN_CLSS_MAC_VNI,
		"outer-mac and vni"
	},
	{
		ETH_TUNNEL_FILTER_OMAC | ETH_TUNNEL_FILTER_IMAC,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"outer-mac and inner-mac"
	},
	{
		ETH_TUNNEL_FILTER_OMAC | ETH_TUNNEL_FILTER_IVLAN,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"outer-mac and inner-vlan"
	},
	{
		ETH_TUNNEL_FILTER_TENID | ETH_TUNNEL_FILTER_IMAC,
		ECORE_FILTER_INNER_MAC_VNI_PAIR,
		ECORE_TUNN_CLSS_INNER_MAC_VNI,
		"vni and inner-mac",
	},
	{
		ETH_TUNNEL_FILTER_TENID | ETH_TUNNEL_FILTER_IVLAN,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"vni and inner-vlan",
	},
	{
		ETH_TUNNEL_FILTER_IMAC | ETH_TUNNEL_FILTER_IVLAN,
		ECORE_FILTER_INNER_PAIR,
		ECORE_TUNN_CLSS_INNER_MAC_VLAN,
		"inner-mac and inner-vlan",
	},
	{
		ETH_TUNNEL_FILTER_OIP,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"outer-IP"
	},
	{
		ETH_TUNNEL_FILTER_IIP,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"inner-IP"
	},
	{
		RTE_TUNNEL_FILTER_IMAC_IVLAN,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"IMAC_IVLAN"
	},
	{
		RTE_TUNNEL_FILTER_IMAC_IVLAN_TENID,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"IMAC_IVLAN_TENID"
	},
	{
		RTE_TUNNEL_FILTER_IMAC_TENID,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"IMAC_TENID"
	},
	{
		RTE_TUNNEL_FILTER_OMAC_TENID_IMAC,
		ECORE_FILTER_UNUSED,
		MAX_ECORE_TUNN_CLSS,
		"OMAC_TENID_IMAC"
	},
};

struct rte_qede_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint64_t offset;
};

static const struct rte_qede_xstats_name_off qede_xstats_strings[] = {
	{"rx_unicast_bytes",
		offsetof(struct ecore_eth_stats_common, rx_ucast_bytes)},
	{"rx_multicast_bytes",
		offsetof(struct ecore_eth_stats_common, rx_mcast_bytes)},
	{"rx_broadcast_bytes",
		offsetof(struct ecore_eth_stats_common, rx_bcast_bytes)},
	{"rx_unicast_packets",
		offsetof(struct ecore_eth_stats_common, rx_ucast_pkts)},
	{"rx_multicast_packets",
		offsetof(struct ecore_eth_stats_common, rx_mcast_pkts)},
	{"rx_broadcast_packets",
		offsetof(struct ecore_eth_stats_common, rx_bcast_pkts)},

	{"tx_unicast_bytes",
		offsetof(struct ecore_eth_stats_common, tx_ucast_bytes)},
	{"tx_multicast_bytes",
		offsetof(struct ecore_eth_stats_common, tx_mcast_bytes)},
	{"tx_broadcast_bytes",
		offsetof(struct ecore_eth_stats_common, tx_bcast_bytes)},
	{"tx_unicast_packets",
		offsetof(struct ecore_eth_stats_common, tx_ucast_pkts)},
	{"tx_multicast_packets",
		offsetof(struct ecore_eth_stats_common, tx_mcast_pkts)},
	{"tx_broadcast_packets",
		offsetof(struct ecore_eth_stats_common, tx_bcast_pkts)},

	{"rx_64_byte_packets",
		offsetof(struct ecore_eth_stats_common, rx_64_byte_packets)},
	{"rx_65_to_127_byte_packets",
		offsetof(struct ecore_eth_stats_common,
			 rx_65_to_127_byte_packets)},
	{"rx_128_to_255_byte_packets",
		offsetof(struct ecore_eth_stats_common,
			 rx_128_to_255_byte_packets)},
	{"rx_256_to_511_byte_packets",
		offsetof(struct ecore_eth_stats_common,
			 rx_256_to_511_byte_packets)},
	{"rx_512_to_1023_byte_packets",
		offsetof(struct ecore_eth_stats_common,
			 rx_512_to_1023_byte_packets)},
	{"rx_1024_to_1518_byte_packets",
		offsetof(struct ecore_eth_stats_common,
			 rx_1024_to_1518_byte_packets)},
	{"tx_64_byte_packets",
		offsetof(struct ecore_eth_stats_common, tx_64_byte_packets)},
	{"tx_65_to_127_byte_packets",
		offsetof(struct ecore_eth_stats_common,
			 tx_65_to_127_byte_packets)},
	{"tx_128_to_255_byte_packets",
		offsetof(struct ecore_eth_stats_common,
			 tx_128_to_255_byte_packets)},
	{"tx_256_to_511_byte_packets",
		offsetof(struct ecore_eth_stats_common,
			 tx_256_to_511_byte_packets)},
	{"tx_512_to_1023_byte_packets",
		offsetof(struct ecore_eth_stats_common,
			 tx_512_to_1023_byte_packets)},
	{"tx_1024_to_1518_byte_packets",
		offsetof(struct ecore_eth_stats_common,
			 tx_1024_to_1518_byte_packets)},

	{"rx_mac_crtl_frames",
		offsetof(struct ecore_eth_stats_common, rx_mac_crtl_frames)},
	{"tx_mac_control_frames",
		offsetof(struct ecore_eth_stats_common, tx_mac_ctrl_frames)},
	{"rx_pause_frames",
		offsetof(struct ecore_eth_stats_common, rx_pause_frames)},
	{"tx_pause_frames",
		offsetof(struct ecore_eth_stats_common, tx_pause_frames)},
	{"rx_priority_flow_control_frames",
		offsetof(struct ecore_eth_stats_common, rx_pfc_frames)},
	{"tx_priority_flow_control_frames",
		offsetof(struct ecore_eth_stats_common, tx_pfc_frames)},

	{"rx_crc_errors",
		offsetof(struct ecore_eth_stats_common, rx_crc_errors)},
	{"rx_align_errors",
		offsetof(struct ecore_eth_stats_common, rx_align_errors)},
	{"rx_carrier_errors",
		offsetof(struct ecore_eth_stats_common, rx_carrier_errors)},
	{"rx_oversize_packet_errors",
		offsetof(struct ecore_eth_stats_common, rx_oversize_packets)},
	{"rx_jabber_errors",
		offsetof(struct ecore_eth_stats_common, rx_jabbers)},
	{"rx_undersize_packet_errors",
		offsetof(struct ecore_eth_stats_common, rx_undersize_packets)},
	{"rx_fragments", offsetof(struct ecore_eth_stats_common, rx_fragments)},
	{"rx_host_buffer_not_available",
		offsetof(struct ecore_eth_stats_common, no_buff_discards)},
	/* Number of packets discarded because they are bigger than MTU */
	{"rx_packet_too_big_discards",
		offsetof(struct ecore_eth_stats_common,
			 packet_too_big_discard)},
	{"rx_ttl_zero_discards",
		offsetof(struct ecore_eth_stats_common, ttl0_discard)},
	{"rx_multi_function_tag_filter_discards",
		offsetof(struct ecore_eth_stats_common, mftag_filter_discards)},
	{"rx_mac_filter_discards",
		offsetof(struct ecore_eth_stats_common, mac_filter_discards)},
	{"rx_hw_buffer_truncates",
		offsetof(struct ecore_eth_stats_common, brb_truncates)},
	{"rx_hw_buffer_discards",
		offsetof(struct ecore_eth_stats_common, brb_discards)},
	{"tx_error_drop_packets",
		offsetof(struct ecore_eth_stats_common, tx_err_drop_pkts)},

	{"rx_mac_bytes", offsetof(struct ecore_eth_stats_common, rx_mac_bytes)},
	{"rx_mac_unicast_packets",
		offsetof(struct ecore_eth_stats_common, rx_mac_uc_packets)},
	{"rx_mac_multicast_packets",
		offsetof(struct ecore_eth_stats_common, rx_mac_mc_packets)},
	{"rx_mac_broadcast_packets",
		offsetof(struct ecore_eth_stats_common, rx_mac_bc_packets)},
	{"rx_mac_frames_ok",
		offsetof(struct ecore_eth_stats_common, rx_mac_frames_ok)},
	{"tx_mac_bytes", offsetof(struct ecore_eth_stats_common, tx_mac_bytes)},
	{"tx_mac_unicast_packets",
		offsetof(struct ecore_eth_stats_common, tx_mac_uc_packets)},
	{"tx_mac_multicast_packets",
		offsetof(struct ecore_eth_stats_common, tx_mac_mc_packets)},
	{"tx_mac_broadcast_packets",
		offsetof(struct ecore_eth_stats_common, tx_mac_bc_packets)},

	{"lro_coalesced_packets",
		offsetof(struct ecore_eth_stats_common, tpa_coalesced_pkts)},
	{"lro_coalesced_events",
		offsetof(struct ecore_eth_stats_common, tpa_coalesced_events)},
	{"lro_aborts_num",
		offsetof(struct ecore_eth_stats_common, tpa_aborts_num)},
	{"lro_not_coalesced_packets",
		offsetof(struct ecore_eth_stats_common,
			 tpa_not_coalesced_pkts)},
	{"lro_coalesced_bytes",
		offsetof(struct ecore_eth_stats_common,
			 tpa_coalesced_bytes)},
};

static const struct rte_qede_xstats_name_off qede_bb_xstats_strings[] = {
	{"rx_1519_to_1522_byte_packets",
		offsetof(struct ecore_eth_stats, bb) +
		offsetof(struct ecore_eth_stats_bb,
			 rx_1519_to_1522_byte_packets)},
	{"rx_1519_to_2047_byte_packets",
		offsetof(struct ecore_eth_stats, bb) +
		offsetof(struct ecore_eth_stats_bb,
			 rx_1519_to_2047_byte_packets)},
	{"rx_2048_to_4095_byte_packets",
		offsetof(struct ecore_eth_stats, bb) +
		offsetof(struct ecore_eth_stats_bb,
			 rx_2048_to_4095_byte_packets)},
	{"rx_4096_to_9216_byte_packets",
		offsetof(struct ecore_eth_stats, bb) +
		offsetof(struct ecore_eth_stats_bb,
			 rx_4096_to_9216_byte_packets)},
	{"rx_9217_to_16383_byte_packets",
		offsetof(struct ecore_eth_stats, bb) +
		offsetof(struct ecore_eth_stats_bb,
			 rx_9217_to_16383_byte_packets)},

	{"tx_1519_to_2047_byte_packets",
		offsetof(struct ecore_eth_stats, bb) +
		offsetof(struct ecore_eth_stats_bb,
			 tx_1519_to_2047_byte_packets)},
	{"tx_2048_to_4095_byte_packets",
		offsetof(struct ecore_eth_stats, bb) +
		offsetof(struct ecore_eth_stats_bb,
			 tx_2048_to_4095_byte_packets)},
	{"tx_4096_to_9216_byte_packets",
		offsetof(struct ecore_eth_stats, bb) +
		offsetof(struct ecore_eth_stats_bb,
			 tx_4096_to_9216_byte_packets)},
	{"tx_9217_to_16383_byte_packets",
		offsetof(struct ecore_eth_stats, bb) +
		offsetof(struct ecore_eth_stats_bb,
			 tx_9217_to_16383_byte_packets)},

	{"tx_lpi_entry_count",
		offsetof(struct ecore_eth_stats, bb) +
		offsetof(struct ecore_eth_stats_bb, tx_lpi_entry_count)},
	{"tx_total_collisions",
		offsetof(struct ecore_eth_stats, bb) +
		offsetof(struct ecore_eth_stats_bb, tx_total_collisions)},
};

static const struct rte_qede_xstats_name_off qede_ah_xstats_strings[] = {
	{"rx_1519_to_max_byte_packets",
		offsetof(struct ecore_eth_stats, ah) +
		offsetof(struct ecore_eth_stats_ah,
			 rx_1519_to_max_byte_packets)},
	{"tx_1519_to_max_byte_packets",
		offsetof(struct ecore_eth_stats, ah) +
		offsetof(struct ecore_eth_stats_ah,
			 tx_1519_to_max_byte_packets)},
};

static const struct rte_qede_xstats_name_off qede_rxq_xstats_strings[] = {
	{"rx_q_segments",
		offsetof(struct qede_rx_queue, rx_segs)},
	{"rx_q_hw_errors",
		offsetof(struct qede_rx_queue, rx_hw_errors)},
	{"rx_q_allocation_errors",
		offsetof(struct qede_rx_queue, rx_alloc_errors)}
};

static void qede_interrupt_action(struct ecore_hwfn *p_hwfn)
{
	ecore_int_sp_dpc((osal_int_ptr_t)(p_hwfn));
}

static void
qede_interrupt_handler(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;

	qede_interrupt_action(ECORE_LEADING_HWFN(edev));
	if (rte_intr_enable(eth_dev->intr_handle))
		DP_ERR(edev, "rte_intr_enable failed\n");
}

static void
qede_alloc_etherdev(struct qede_dev *qdev, struct qed_dev_eth_info *info)
{
	rte_memcpy(&qdev->dev_info, info, sizeof(*info));
	qdev->ops = qed_ops;
}

static void qede_print_adapter_info(struct qede_dev *qdev)
{
	struct ecore_dev *edev = &qdev->edev;
	struct qed_dev_info *info = &qdev->dev_info.common;
	static char drv_ver[QEDE_PMD_DRV_VER_STR_SIZE];
	static char ver_str[QEDE_PMD_DRV_VER_STR_SIZE];

	DP_INFO(edev, "*********************************\n");
	DP_INFO(edev, " DPDK version:%s\n", rte_version());
	DP_INFO(edev, " Chip details : %s %c%d\n",
		  ECORE_IS_BB(edev) ? "BB" : "AH",
		  'A' + edev->chip_rev,
		  (int)edev->chip_metal);
	snprintf(ver_str, QEDE_PMD_DRV_VER_STR_SIZE, "%d.%d.%d.%d",
		 info->fw_major, info->fw_minor, info->fw_rev, info->fw_eng);
	snprintf(drv_ver, QEDE_PMD_DRV_VER_STR_SIZE, "%s_%s",
		 ver_str, QEDE_PMD_VERSION);
	DP_INFO(edev, " Driver version : %s\n", drv_ver);
	DP_INFO(edev, " Firmware version : %s\n", ver_str);

	snprintf(ver_str, MCP_DRV_VER_STR_SIZE,
		 "%d.%d.%d.%d",
		(info->mfw_rev >> 24) & 0xff,
		(info->mfw_rev >> 16) & 0xff,
		(info->mfw_rev >> 8) & 0xff, (info->mfw_rev) & 0xff);
	DP_INFO(edev, " Management Firmware version : %s\n", ver_str);
	DP_INFO(edev, " Firmware file : %s\n", fw_file);
	DP_INFO(edev, "*********************************\n");
}

static void qede_reset_queue_stats(struct qede_dev *qdev, bool xstats)
{
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	unsigned int i = 0, j = 0, qid;
	unsigned int rxq_stat_cntrs, txq_stat_cntrs;
	struct qede_tx_queue *txq;

	DP_VERBOSE(edev, ECORE_MSG_DEBUG, "Clearing queue stats\n");

	rxq_stat_cntrs = RTE_MIN(QEDE_RSS_COUNT(qdev),
			       RTE_ETHDEV_QUEUE_STAT_CNTRS);
	txq_stat_cntrs = RTE_MIN(QEDE_TSS_COUNT(qdev),
			       RTE_ETHDEV_QUEUE_STAT_CNTRS);

	for_each_rss(qid) {
		OSAL_MEMSET(((char *)(qdev->fp_array[qid].rxq)) +
			     offsetof(struct qede_rx_queue, rcv_pkts), 0,
			    sizeof(uint64_t));
		OSAL_MEMSET(((char *)(qdev->fp_array[qid].rxq)) +
			     offsetof(struct qede_rx_queue, rx_hw_errors), 0,
			    sizeof(uint64_t));
		OSAL_MEMSET(((char *)(qdev->fp_array[qid].rxq)) +
			     offsetof(struct qede_rx_queue, rx_alloc_errors), 0,
			    sizeof(uint64_t));

		if (xstats)
			for (j = 0; j < RTE_DIM(qede_rxq_xstats_strings); j++)
				OSAL_MEMSET((((char *)
					      (qdev->fp_array[qid].rxq)) +
					     qede_rxq_xstats_strings[j].offset),
					    0,
					    sizeof(uint64_t));

		i++;
		if (i == rxq_stat_cntrs)
			break;
	}

	i = 0;

	for_each_tss(qid) {
		txq = qdev->fp_array[qid].txq;

		OSAL_MEMSET((uint64_t *)(uintptr_t)
				(((uint64_t)(uintptr_t)(txq)) +
				 offsetof(struct qede_tx_queue, xmit_pkts)), 0,
			    sizeof(uint64_t));

		i++;
		if (i == txq_stat_cntrs)
			break;
	}
}

static int
qede_stop_vport(struct ecore_dev *edev)
{
	struct ecore_hwfn *p_hwfn;
	uint8_t vport_id;
	int rc;
	int i;

	vport_id = 0;
	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		rc = ecore_sp_vport_stop(p_hwfn, p_hwfn->hw_info.opaque_fid,
					 vport_id);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Stop V-PORT failed rc = %d\n", rc);
			return rc;
		}
	}

	DP_INFO(edev, "vport stopped\n");

	return 0;
}

static int
qede_start_vport(struct qede_dev *qdev, uint16_t mtu)
{
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_sp_vport_start_params params;
	struct ecore_hwfn *p_hwfn;
	int rc;
	int i;

	if (qdev->vport_started)
		qede_stop_vport(edev);

	memset(&params, 0, sizeof(params));
	params.vport_id = 0;
	params.mtu = mtu;
	/* @DPDK - Disable FW placement */
	params.zero_placement_offset = 1;
	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		params.concrete_fid = p_hwfn->hw_info.concrete_fid;
		params.opaque_fid = p_hwfn->hw_info.opaque_fid;
		rc = ecore_sp_vport_start(p_hwfn, &params);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Start V-PORT failed %d\n", rc);
			return rc;
		}
	}
	ecore_reset_vport_stats(edev);
	qdev->vport_started = true;
	DP_INFO(edev, "VPORT started with MTU = %u\n", mtu);

	return 0;
}

#define QEDE_NPAR_TX_SWITCHING		"npar_tx_switching"
#define QEDE_VF_TX_SWITCHING		"vf_tx_switching"

/* Activate or deactivate vport via vport-update */
int qede_activate_vport(struct rte_eth_dev *eth_dev, bool flg)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_sp_vport_update_params params;
	struct ecore_hwfn *p_hwfn;
	uint8_t i;
	int rc = -1;

	memset(&params, 0, sizeof(struct ecore_sp_vport_update_params));
	params.vport_id = 0;
	params.update_vport_active_rx_flg = 1;
	params.update_vport_active_tx_flg = 1;
	params.vport_active_rx_flg = flg;
	params.vport_active_tx_flg = flg;
	if (!qdev->enable_tx_switching) {
		if ((QEDE_NPAR_TX_SWITCHING != NULL) ||
		    ((QEDE_VF_TX_SWITCHING != NULL) && IS_VF(edev))) {
			params.update_tx_switching_flg = 1;
			params.tx_switching_flg = !flg;
			DP_INFO(edev, "%s tx-switching is disabled\n",
				QEDE_NPAR_TX_SWITCHING ? "NPAR" : "VF");
		}
	}
	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		params.opaque_fid = p_hwfn->hw_info.opaque_fid;
		rc = ecore_sp_vport_update(p_hwfn, &params,
				ECORE_SPQ_MODE_EBLOCK, NULL);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Failed to update vport\n");
			break;
		}
	}
	DP_INFO(edev, "vport is %s\n", flg ? "activated" : "deactivated");

	return rc;
}

static void
qede_update_sge_tpa_params(struct ecore_sge_tpa_params *sge_tpa_params,
			   uint16_t mtu, bool enable)
{
	/* Enable LRO in split mode */
	sge_tpa_params->tpa_ipv4_en_flg = enable;
	sge_tpa_params->tpa_ipv6_en_flg = enable;
	sge_tpa_params->tpa_ipv4_tunn_en_flg = enable;
	sge_tpa_params->tpa_ipv6_tunn_en_flg = enable;
	/* set if tpa enable changes */
	sge_tpa_params->update_tpa_en_flg = 1;
	/* set if tpa parameters should be handled */
	sge_tpa_params->update_tpa_param_flg = enable;

	sge_tpa_params->max_buffers_per_cqe = 20;
	/* Enable TPA in split mode. In this mode each TPA segment
	 * starts on the new BD, so there is one BD per segment.
	 */
	sge_tpa_params->tpa_pkt_split_flg = 1;
	sge_tpa_params->tpa_hdr_data_split_flg = 0;
	sge_tpa_params->tpa_gro_consistent_flg = 0;
	sge_tpa_params->tpa_max_aggs_num = ETH_TPA_MAX_AGGS_NUM;
	sge_tpa_params->tpa_max_size = 0x7FFF;
	sge_tpa_params->tpa_min_size_to_start = mtu / 2;
	sge_tpa_params->tpa_min_size_to_cont = mtu / 2;
}

/* Enable/disable LRO via vport-update */
int qede_enable_tpa(struct rte_eth_dev *eth_dev, bool flg)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_sp_vport_update_params params;
	struct ecore_sge_tpa_params tpa_params;
	struct ecore_hwfn *p_hwfn;
	int rc;
	int i;

	memset(&params, 0, sizeof(struct ecore_sp_vport_update_params));
	memset(&tpa_params, 0, sizeof(struct ecore_sge_tpa_params));
	qede_update_sge_tpa_params(&tpa_params, qdev->mtu, flg);
	params.vport_id = 0;
	params.sge_tpa_params = &tpa_params;
	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		params.opaque_fid = p_hwfn->hw_info.opaque_fid;
		rc = ecore_sp_vport_update(p_hwfn, &params,
				ECORE_SPQ_MODE_EBLOCK, NULL);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Failed to update LRO\n");
			return -1;
		}
	}
	qdev->enable_lro = flg;
	eth_dev->data->lro = flg;

	DP_INFO(edev, "LRO is %s\n", flg ? "enabled" : "disabled");

	return 0;
}

/* Update MTU via vport-update without doing port restart.
 * The vport must be deactivated before calling this API.
 */
int qede_update_mtu(struct rte_eth_dev *eth_dev, uint16_t mtu)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_sp_vport_update_params params;
	struct ecore_hwfn *p_hwfn;
	int rc;
	int i;

	memset(&params, 0, sizeof(struct ecore_sp_vport_update_params));
	params.vport_id = 0;
	params.mtu = mtu;
	params.vport_id = 0;
	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		params.opaque_fid = p_hwfn->hw_info.opaque_fid;
		rc = ecore_sp_vport_update(p_hwfn, &params,
				ECORE_SPQ_MODE_EBLOCK, NULL);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Failed to update MTU\n");
			return -1;
		}
	}
	DP_INFO(edev, "MTU updated to %u\n", mtu);

	return 0;
}

static void qede_set_ucast_cmn_params(struct ecore_filter_ucast *ucast)
{
	memset(ucast, 0, sizeof(struct ecore_filter_ucast));
	ucast->is_rx_filter = true;
	ucast->is_tx_filter = true;
	/* ucast->assert_on_error = true; - For debug */
}

static int
qed_configure_filter_rx_mode(struct rte_eth_dev *eth_dev,
			     enum qed_filter_rx_mode_type type)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_filter_accept_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.update_rx_mode_config = 1;
	flags.update_tx_mode_config = 1;
	flags.rx_accept_filter = ECORE_ACCEPT_UCAST_MATCHED |
		ECORE_ACCEPT_MCAST_MATCHED |
		ECORE_ACCEPT_BCAST;

	flags.tx_accept_filter = ECORE_ACCEPT_UCAST_MATCHED |
		ECORE_ACCEPT_MCAST_MATCHED |
		ECORE_ACCEPT_BCAST;

	if (type == QED_FILTER_RX_MODE_TYPE_PROMISC) {
		flags.rx_accept_filter |= ECORE_ACCEPT_UCAST_UNMATCHED;
		if (IS_VF(edev)) {
			flags.tx_accept_filter |= ECORE_ACCEPT_UCAST_UNMATCHED;
			DP_INFO(edev, "Enabling Tx unmatched flag for VF\n");
		}
	} else if (type == QED_FILTER_RX_MODE_TYPE_MULTI_PROMISC) {
		flags.rx_accept_filter |= ECORE_ACCEPT_MCAST_UNMATCHED;
	} else if (type == (QED_FILTER_RX_MODE_TYPE_MULTI_PROMISC |
				QED_FILTER_RX_MODE_TYPE_PROMISC)) {
		flags.rx_accept_filter |= ECORE_ACCEPT_UCAST_UNMATCHED |
			ECORE_ACCEPT_MCAST_UNMATCHED;
	}

	return ecore_filter_accept_cmd(edev, 0, flags, false, false,
			ECORE_SPQ_MODE_CB, NULL);
}

static int
qede_tunnel_update(struct qede_dev *qdev,
		   struct ecore_tunnel_info *tunn_info)
{
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	enum _ecore_status_t rc = ECORE_INVAL;
	struct ecore_hwfn *p_hwfn;
	struct ecore_ptt *p_ptt;
	int i;

	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		if (IS_PF(edev)) {
			p_ptt = ecore_ptt_acquire(p_hwfn);
			if (!p_ptt) {
				DP_ERR(p_hwfn, "Can't acquire PTT\n");
				return -EAGAIN;
			}
		} else {
			p_ptt = NULL;
		}

		rc = ecore_sp_pf_update_tunn_cfg(p_hwfn, p_ptt,
				tunn_info, ECORE_SPQ_MODE_CB, NULL);
		if (IS_PF(edev))
			ecore_ptt_release(p_hwfn, p_ptt);

		if (rc != ECORE_SUCCESS)
			break;
	}

	return rc;
}

static int
qede_vxlan_enable(struct rte_eth_dev *eth_dev, uint8_t clss,
		  bool enable)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	enum _ecore_status_t rc = ECORE_INVAL;
	struct ecore_tunnel_info tunn;

	if (qdev->vxlan.enable == enable)
		return ECORE_SUCCESS;

	memset(&tunn, 0, sizeof(struct ecore_tunnel_info));
	tunn.vxlan.b_update_mode = true;
	tunn.vxlan.b_mode_enabled = enable;
	tunn.b_update_rx_cls = true;
	tunn.b_update_tx_cls = true;
	tunn.vxlan.tun_cls = clss;

	tunn.vxlan_port.b_update_port = true;
	tunn.vxlan_port.port = enable ? QEDE_VXLAN_DEF_PORT : 0;

	rc = qede_tunnel_update(qdev, &tunn);
	if (rc == ECORE_SUCCESS) {
		qdev->vxlan.enable = enable;
		qdev->vxlan.udp_port = (enable) ? QEDE_VXLAN_DEF_PORT : 0;
		DP_INFO(edev, "vxlan is %s, UDP port = %d\n",
			enable ? "enabled" : "disabled", qdev->vxlan.udp_port);
	} else {
		DP_ERR(edev, "Failed to update tunn_clss %u\n",
		       tunn.vxlan.tun_cls);
	}

	return rc;
}

static int
qede_geneve_enable(struct rte_eth_dev *eth_dev, uint8_t clss,
		  bool enable)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	enum _ecore_status_t rc = ECORE_INVAL;
	struct ecore_tunnel_info tunn;

	memset(&tunn, 0, sizeof(struct ecore_tunnel_info));
	tunn.l2_geneve.b_update_mode = true;
	tunn.l2_geneve.b_mode_enabled = enable;
	tunn.ip_geneve.b_update_mode = true;
	tunn.ip_geneve.b_mode_enabled = enable;
	tunn.l2_geneve.tun_cls = clss;
	tunn.ip_geneve.tun_cls = clss;
	tunn.b_update_rx_cls = true;
	tunn.b_update_tx_cls = true;

	tunn.geneve_port.b_update_port = true;
	tunn.geneve_port.port = enable ? QEDE_GENEVE_DEF_PORT : 0;

	rc = qede_tunnel_update(qdev, &tunn);
	if (rc == ECORE_SUCCESS) {
		qdev->geneve.enable = enable;
		qdev->geneve.udp_port = (enable) ? QEDE_GENEVE_DEF_PORT : 0;
		DP_INFO(edev, "GENEVE is %s, UDP port = %d\n",
			enable ? "enabled" : "disabled", qdev->geneve.udp_port);
	} else {
		DP_ERR(edev, "Failed to update tunn_clss %u\n",
		       clss);
	}

	return rc;
}

static int
qede_ipgre_enable(struct rte_eth_dev *eth_dev, uint8_t clss,
		  bool enable)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	enum _ecore_status_t rc = ECORE_INVAL;
	struct ecore_tunnel_info tunn;

	memset(&tunn, 0, sizeof(struct ecore_tunnel_info));
	tunn.ip_gre.b_update_mode = true;
	tunn.ip_gre.b_mode_enabled = enable;
	tunn.ip_gre.tun_cls = clss;
	tunn.ip_gre.tun_cls = clss;
	tunn.b_update_rx_cls = true;
	tunn.b_update_tx_cls = true;

	rc = qede_tunnel_update(qdev, &tunn);
	if (rc == ECORE_SUCCESS) {
		qdev->ipgre.enable = enable;
		DP_INFO(edev, "IPGRE is %s\n",
			enable ? "enabled" : "disabled");
	} else {
		DP_ERR(edev, "Failed to update tunn_clss %u\n",
		       clss);
	}

	return rc;
}

static int
qede_tunn_enable(struct rte_eth_dev *eth_dev, uint8_t clss,
		 enum rte_eth_tunnel_type tunn_type, bool enable)
{
	int rc = -EINVAL;

	switch (tunn_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		rc = qede_vxlan_enable(eth_dev, clss, enable);
		break;
	case RTE_TUNNEL_TYPE_GENEVE:
		rc = qede_geneve_enable(eth_dev, clss, enable);
		break;
	case RTE_TUNNEL_TYPE_IP_IN_GRE:
		rc = qede_ipgre_enable(eth_dev, clss, enable);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int
qede_ucast_filter(struct rte_eth_dev *eth_dev, struct ecore_filter_ucast *ucast,
		  bool add)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct qede_ucast_entry *tmp = NULL;
	struct qede_ucast_entry *u;
	struct ether_addr *mac_addr;

	mac_addr  = (struct ether_addr *)ucast->mac;
	if (add) {
		SLIST_FOREACH(tmp, &qdev->uc_list_head, list) {
			if ((memcmp(mac_addr, &tmp->mac,
				    ETHER_ADDR_LEN) == 0) &&
			     ucast->vni == tmp->vni &&
			     ucast->vlan == tmp->vlan) {
				DP_INFO(edev, "Unicast MAC is already added"
					" with vlan = %u, vni = %u\n",
					ucast->vlan,  ucast->vni);
					return 0;
			}
		}
		u = rte_malloc(NULL, sizeof(struct qede_ucast_entry),
			       RTE_CACHE_LINE_SIZE);
		if (!u) {
			DP_ERR(edev, "Did not allocate memory for ucast\n");
			return -ENOMEM;
		}
		ether_addr_copy(mac_addr, &u->mac);
		u->vlan = ucast->vlan;
		u->vni = ucast->vni;
		SLIST_INSERT_HEAD(&qdev->uc_list_head, u, list);
		qdev->num_uc_addr++;
	} else {
		SLIST_FOREACH(tmp, &qdev->uc_list_head, list) {
			if ((memcmp(mac_addr, &tmp->mac,
				    ETHER_ADDR_LEN) == 0) &&
			    ucast->vlan == tmp->vlan	  &&
			    ucast->vni == tmp->vni)
			break;
		}
		if (tmp == NULL) {
			DP_INFO(edev, "Unicast MAC is not found\n");
			return -EINVAL;
		}
		SLIST_REMOVE(&qdev->uc_list_head, tmp, qede_ucast_entry, list);
		qdev->num_uc_addr--;
	}

	return 0;
}

static int
qede_add_mcast_filters(struct rte_eth_dev *eth_dev, struct ether_addr *mc_addrs,
		       uint32_t mc_addrs_num)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_filter_mcast mcast;
	struct qede_mcast_entry *m = NULL;
	uint8_t i;
	int rc;

	for (i = 0; i < mc_addrs_num; i++) {
		m = rte_malloc(NULL, sizeof(struct qede_mcast_entry),
			       RTE_CACHE_LINE_SIZE);
		if (!m) {
			DP_ERR(edev, "Did not allocate memory for mcast\n");
			return -ENOMEM;
		}
		ether_addr_copy(&mc_addrs[i], &m->mac);
		SLIST_INSERT_HEAD(&qdev->mc_list_head, m, list);
	}
	memset(&mcast, 0, sizeof(mcast));
	mcast.num_mc_addrs = mc_addrs_num;
	mcast.opcode = ECORE_FILTER_ADD;
	for (i = 0; i < mc_addrs_num; i++)
		ether_addr_copy(&mc_addrs[i], (struct ether_addr *)
							&mcast.mac[i]);
	rc = ecore_filter_mcast_cmd(edev, &mcast, ECORE_SPQ_MODE_CB, NULL);
	if (rc != ECORE_SUCCESS) {
		DP_ERR(edev, "Failed to add multicast filter (rc = %d\n)", rc);
		return -1;
	}

	return 0;
}

static int qede_del_mcast_filters(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct qede_mcast_entry *tmp = NULL;
	struct ecore_filter_mcast mcast;
	int j;
	int rc;

	memset(&mcast, 0, sizeof(mcast));
	mcast.num_mc_addrs = qdev->num_mc_addr;
	mcast.opcode = ECORE_FILTER_REMOVE;
	j = 0;
	SLIST_FOREACH(tmp, &qdev->mc_list_head, list) {
		ether_addr_copy(&tmp->mac, (struct ether_addr *)&mcast.mac[j]);
		j++;
	}
	rc = ecore_filter_mcast_cmd(edev, &mcast, ECORE_SPQ_MODE_CB, NULL);
	if (rc != ECORE_SUCCESS) {
		DP_ERR(edev, "Failed to delete multicast filter\n");
		return -1;
	}
	/* Init the list */
	while (!SLIST_EMPTY(&qdev->mc_list_head)) {
		tmp = SLIST_FIRST(&qdev->mc_list_head);
		SLIST_REMOVE_HEAD(&qdev->mc_list_head, list);
	}
	SLIST_INIT(&qdev->mc_list_head);

	return 0;
}

static enum _ecore_status_t
qede_mac_int_ops(struct rte_eth_dev *eth_dev, struct ecore_filter_ucast *ucast,
		 bool add)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	enum _ecore_status_t rc = ECORE_INVAL;

	if (add && (qdev->num_uc_addr >= qdev->dev_info.num_mac_filters)) {
		DP_ERR(edev, "Ucast filter table limit exceeded,"
			      " Please enable promisc mode\n");
			return ECORE_INVAL;
	}

	rc = qede_ucast_filter(eth_dev, ucast, add);
	if (rc == 0)
		rc = ecore_filter_ucast_cmd(edev, ucast,
					    ECORE_SPQ_MODE_CB, NULL);
	if (rc != ECORE_SUCCESS)
		DP_ERR(edev, "MAC filter failed, rc = %d, op = %d\n",
		       rc, add);

	return rc;
}

static int
qede_mac_addr_add(struct rte_eth_dev *eth_dev, struct ether_addr *mac_addr,
		  __rte_unused uint32_t index, __rte_unused uint32_t pool)
{
	struct ecore_filter_ucast ucast;
	int re;

	qede_set_ucast_cmn_params(&ucast);
	ucast.type = ECORE_FILTER_MAC;
	ether_addr_copy(mac_addr, (struct ether_addr *)&ucast.mac);
	re = (int)qede_mac_int_ops(eth_dev, &ucast, 1);
	return re;
}

static void
qede_mac_addr_remove(struct rte_eth_dev *eth_dev, uint32_t index)
{
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;
	struct ecore_filter_ucast ucast;

	PMD_INIT_FUNC_TRACE(edev);

	if (index >= qdev->dev_info.num_mac_filters) {
		DP_ERR(edev, "Index %u is above MAC filter limit %u\n",
		       index, qdev->dev_info.num_mac_filters);
		return;
	}

	qede_set_ucast_cmn_params(&ucast);
	ucast.opcode = ECORE_FILTER_REMOVE;
	ucast.type = ECORE_FILTER_MAC;

	/* Use the index maintained by rte */
	ether_addr_copy(&eth_dev->data->mac_addrs[index],
			(struct ether_addr *)&ucast.mac);

	qede_mac_int_ops(eth_dev, &ucast, false);
}

static int
qede_mac_addr_set(struct rte_eth_dev *eth_dev, struct ether_addr *mac_addr)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);

	if (IS_VF(edev) && !ecore_vf_check_mac(ECORE_LEADING_HWFN(edev),
					       mac_addr->addr_bytes)) {
		DP_ERR(edev, "Setting MAC address is not allowed\n");
		return -EPERM;
	}

	qede_mac_addr_add(eth_dev, mac_addr, 0, 0);
	return 0;
}

static void qede_config_accept_any_vlan(struct qede_dev *qdev, bool flg)
{
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_sp_vport_update_params params;
	struct ecore_hwfn *p_hwfn;
	uint8_t i;
	int rc;

	memset(&params, 0, sizeof(struct ecore_sp_vport_update_params));
	params.vport_id = 0;
	params.update_accept_any_vlan_flg = 1;
	params.accept_any_vlan = flg;
	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		params.opaque_fid = p_hwfn->hw_info.opaque_fid;
		rc = ecore_sp_vport_update(p_hwfn, &params,
				ECORE_SPQ_MODE_EBLOCK, NULL);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Failed to configure accept-any-vlan\n");
			return;
		}
	}

	DP_INFO(edev, "%s accept-any-vlan\n", flg ? "enabled" : "disabled");
}

static int qede_vlan_stripping(struct rte_eth_dev *eth_dev, bool flg)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_sp_vport_update_params params;
	struct ecore_hwfn *p_hwfn;
	uint8_t i;
	int rc;

	memset(&params, 0, sizeof(struct ecore_sp_vport_update_params));
	params.vport_id = 0;
	params.update_inner_vlan_removal_flg = 1;
	params.inner_vlan_removal_flg = flg;
	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		params.opaque_fid = p_hwfn->hw_info.opaque_fid;
		rc = ecore_sp_vport_update(p_hwfn, &params,
				ECORE_SPQ_MODE_EBLOCK, NULL);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Failed to update vport\n");
			return -1;
		}
	}

	DP_INFO(edev, "VLAN stripping %s\n", flg ? "enabled" : "disabled");
	return 0;
}

static int qede_vlan_filter_set(struct rte_eth_dev *eth_dev,
				uint16_t vlan_id, int on)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct qed_dev_eth_info *dev_info = &qdev->dev_info;
	struct qede_vlan_entry *tmp = NULL;
	struct qede_vlan_entry *vlan;
	struct ecore_filter_ucast ucast;
	int rc;

	if (on) {
		if (qdev->configured_vlans == dev_info->num_vlan_filters) {
			DP_ERR(edev, "Reached max VLAN filter limit"
				      " enabling accept_any_vlan\n");
			qede_config_accept_any_vlan(qdev, true);
			return 0;
		}

		SLIST_FOREACH(tmp, &qdev->vlan_list_head, list) {
			if (tmp->vid == vlan_id) {
				DP_INFO(edev, "VLAN %u already configured\n",
					vlan_id);
				return 0;
			}
		}

		vlan = rte_malloc(NULL, sizeof(struct qede_vlan_entry),
				  RTE_CACHE_LINE_SIZE);

		if (!vlan) {
			DP_ERR(edev, "Did not allocate memory for VLAN\n");
			return -ENOMEM;
		}

		qede_set_ucast_cmn_params(&ucast);
		ucast.opcode = ECORE_FILTER_ADD;
		ucast.type = ECORE_FILTER_VLAN;
		ucast.vlan = vlan_id;
		rc = ecore_filter_ucast_cmd(edev, &ucast, ECORE_SPQ_MODE_CB,
					    NULL);
		if (rc != 0) {
			DP_ERR(edev, "Failed to add VLAN %u rc %d\n", vlan_id,
			       rc);
			rte_free(vlan);
		} else {
			vlan->vid = vlan_id;
			SLIST_INSERT_HEAD(&qdev->vlan_list_head, vlan, list);
			qdev->configured_vlans++;
			DP_INFO(edev, "VLAN %u added, configured_vlans %u\n",
				vlan_id, qdev->configured_vlans);
		}
	} else {
		SLIST_FOREACH(tmp, &qdev->vlan_list_head, list) {
			if (tmp->vid == vlan_id)
				break;
		}

		if (!tmp) {
			if (qdev->configured_vlans == 0) {
				DP_INFO(edev,
					"No VLAN filters configured yet\n");
				return 0;
			}

			DP_ERR(edev, "VLAN %u not configured\n", vlan_id);
			return -EINVAL;
		}

		SLIST_REMOVE(&qdev->vlan_list_head, tmp, qede_vlan_entry, list);

		qede_set_ucast_cmn_params(&ucast);
		ucast.opcode = ECORE_FILTER_REMOVE;
		ucast.type = ECORE_FILTER_VLAN;
		ucast.vlan = vlan_id;
		rc = ecore_filter_ucast_cmd(edev, &ucast, ECORE_SPQ_MODE_CB,
					    NULL);
		if (rc != 0) {
			DP_ERR(edev, "Failed to delete VLAN %u rc %d\n",
			       vlan_id, rc);
		} else {
			qdev->configured_vlans--;
			DP_INFO(edev, "VLAN %u removed configured_vlans %u\n",
				vlan_id, qdev->configured_vlans);
		}
	}

	return rc;
}

static int qede_vlan_offload_set(struct rte_eth_dev *eth_dev, int mask)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	uint64_t rx_offloads = eth_dev->data->dev_conf.rxmode.offloads;

	if (mask & ETH_VLAN_STRIP_MASK) {
		if (rx_offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
			(void)qede_vlan_stripping(eth_dev, 1);
		else
			(void)qede_vlan_stripping(eth_dev, 0);
	}

	if (mask & ETH_VLAN_FILTER_MASK) {
		/* VLAN filtering kicks in when a VLAN is added */
		if (rx_offloads & DEV_RX_OFFLOAD_VLAN_FILTER) {
			qede_vlan_filter_set(eth_dev, 0, 1);
		} else {
			if (qdev->configured_vlans > 1) { /* Excluding VLAN0 */
				DP_ERR(edev,
				  " Please remove existing VLAN filters"
				  " before disabling VLAN filtering\n");
				/* Signal app that VLAN filtering is still
				 * enabled
				 */
				eth_dev->data->dev_conf.rxmode.offloads |=
						DEV_RX_OFFLOAD_VLAN_FILTER;
			} else {
				qede_vlan_filter_set(eth_dev, 0, 0);
			}
		}
	}

	if (mask & ETH_VLAN_EXTEND_MASK)
		DP_ERR(edev, "Extend VLAN not supported\n");

	qdev->vlan_offload_mask = mask;

	DP_INFO(edev, "VLAN offload mask %d\n", mask);

	return 0;
}

static void qede_prandom_bytes(uint32_t *buff)
{
	uint8_t i;

	srand((unsigned int)time(NULL));
	for (i = 0; i < ECORE_RSS_KEY_SIZE; i++)
		buff[i] = rand();
}

int qede_config_rss(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	uint32_t def_rss_key[ECORE_RSS_KEY_SIZE];
	struct rte_eth_rss_reta_entry64 reta_conf[2];
	struct rte_eth_rss_conf rss_conf;
	uint32_t i, id, pos, q;

	rss_conf = eth_dev->data->dev_conf.rx_adv_conf.rss_conf;
	if (!rss_conf.rss_key) {
		DP_INFO(edev, "Applying driver default key\n");
		rss_conf.rss_key_len = ECORE_RSS_KEY_SIZE * sizeof(uint32_t);
		qede_prandom_bytes(&def_rss_key[0]);
		rss_conf.rss_key = (uint8_t *)&def_rss_key[0];
	}

	/* Configure RSS hash */
	if (qede_rss_hash_update(eth_dev, &rss_conf))
		return -EINVAL;

	/* Configure default RETA */
	memset(reta_conf, 0, sizeof(reta_conf));
	for (i = 0; i < ECORE_RSS_IND_TABLE_SIZE; i++)
		reta_conf[i / RTE_RETA_GROUP_SIZE].mask = UINT64_MAX;

	for (i = 0; i < ECORE_RSS_IND_TABLE_SIZE; i++) {
		id = i / RTE_RETA_GROUP_SIZE;
		pos = i % RTE_RETA_GROUP_SIZE;
		q = i % QEDE_RSS_COUNT(qdev);
		reta_conf[id].reta[pos] = q;
	}
	if (qede_rss_reta_update(eth_dev, &reta_conf[0],
				 ECORE_RSS_IND_TABLE_SIZE))
		return -EINVAL;

	return 0;
}

static void qede_fastpath_start(struct ecore_dev *edev)
{
	struct ecore_hwfn *p_hwfn;
	int i;

	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		ecore_hw_start_fastpath(p_hwfn);
	}
}

static int qede_dev_start(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct rte_eth_rxmode *rxmode = &eth_dev->data->dev_conf.rxmode;

	PMD_INIT_FUNC_TRACE(edev);

	/* Configure TPA parameters */
	if (rxmode->offloads & DEV_RX_OFFLOAD_TCP_LRO) {
		if (qede_enable_tpa(eth_dev, true))
			return -EINVAL;
		/* Enable scatter mode for LRO */
		if (!eth_dev->data->scattered_rx)
			rxmode->offloads |= DEV_RX_OFFLOAD_SCATTER;
	}

	/* Start queues */
	if (qede_start_queues(eth_dev))
		goto err;

	if (IS_PF(edev))
		qede_reset_queue_stats(qdev, true);

	/* Newer SR-IOV PF driver expects RX/TX queues to be started before
	 * enabling RSS. Hence RSS configuration is deferred upto this point.
	 * Also, we would like to retain similar behavior in PF case, so we
	 * don't do PF/VF specific check here.
	 */
	if (eth_dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_RSS)
		if (qede_config_rss(eth_dev))
			goto err;

	/* Enable vport*/
	if (qede_activate_vport(eth_dev, true))
		goto err;

	/* Update link status */
	qede_link_update(eth_dev, 0);

	/* Start/resume traffic */
	qede_fastpath_start(edev);

	DP_INFO(edev, "Device started\n");

	return 0;
err:
	DP_ERR(edev, "Device start fails\n");
	return -1; /* common error code is < 0 */
}

static void qede_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);

	PMD_INIT_FUNC_TRACE(edev);

	/* Disable vport */
	if (qede_activate_vport(eth_dev, false))
		return;

	if (qdev->enable_lro)
		qede_enable_tpa(eth_dev, false);

	/* Stop queues */
	qede_stop_queues(eth_dev);

	/* Disable traffic */
	ecore_hw_stop_fastpath(edev); /* TBD - loop */

	if (IS_PF(edev))
		qede_mac_addr_remove(eth_dev, 0);

	DP_INFO(edev, "Device is stopped\n");
}

const char *valid_args[] = {
	QEDE_NPAR_TX_SWITCHING,
	QEDE_VF_TX_SWITCHING,
	NULL,
};

static int qede_args_check(const char *key, const char *val, void *opaque)
{
	unsigned long tmp;
	int ret = 0;
	struct rte_eth_dev *eth_dev = opaque;
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);

	errno = 0;
	tmp = strtoul(val, NULL, 0);
	if (errno) {
		DP_INFO(edev, "%s: \"%s\" is not a valid integer", key, val);
		return errno;
	}

	if ((strcmp(QEDE_NPAR_TX_SWITCHING, key) == 0) ||
	    (strcmp(QEDE_VF_TX_SWITCHING, key) == 0))
		qdev->enable_tx_switching = !!tmp;

	return ret;
}

static int qede_args(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(eth_dev->device);
	struct rte_kvargs *kvlist;
	struct rte_devargs *devargs;
	int ret;
	int i;

	devargs = pci_dev->device.devargs;
	if (!devargs)
		return 0; /* return success */

	kvlist = rte_kvargs_parse(devargs->args, valid_args);
	if (kvlist == NULL)
		return -EINVAL;

	 /* Process parameters. */
	for (i = 0; (valid_args[i] != NULL); ++i) {
		if (rte_kvargs_count(kvlist, valid_args[i])) {
			ret = rte_kvargs_process(kvlist, valid_args[i],
						 qede_args_check, eth_dev);
			if (ret != ECORE_SUCCESS) {
				rte_kvargs_free(kvlist);
				return ret;
			}
		}
	}
	rte_kvargs_free(kvlist);

	return 0;
}

static int qede_dev_configure(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct rte_eth_rxmode *rxmode = &eth_dev->data->dev_conf.rxmode;
	int ret;

	PMD_INIT_FUNC_TRACE(edev);

	/* Check requirements for 100G mode */
	if (ECORE_IS_CMT(edev)) {
		if (eth_dev->data->nb_rx_queues < 2 ||
		    eth_dev->data->nb_tx_queues < 2) {
			DP_ERR(edev, "100G mode needs min. 2 RX/TX queues\n");
			return -EINVAL;
		}

		if ((eth_dev->data->nb_rx_queues % 2 != 0) ||
		    (eth_dev->data->nb_tx_queues % 2 != 0)) {
			DP_ERR(edev,
			       "100G mode needs even no. of RX/TX queues\n");
			return -EINVAL;
		}
	}

	/* We need to have min 1 RX queue.There is no min check in
	 * rte_eth_dev_configure(), so we are checking it here.
	 */
	if (eth_dev->data->nb_rx_queues == 0) {
		DP_ERR(edev, "Minimum one RX queue is required\n");
		return -EINVAL;
	}

	/* Enable Tx switching by default */
	qdev->enable_tx_switching = 1;

	/* Parse devargs and fix up rxmode */
	if (qede_args(eth_dev))
		return -ENOTSUP;

	if (!(rxmode->mq_mode == ETH_MQ_RX_NONE ||
	      rxmode->mq_mode == ETH_MQ_RX_RSS)) {
		DP_ERR(edev, "Unsupported multi-queue mode\n");
		return -ENOTSUP;
	}
	/* Flow director mode check */
	if (qede_check_fdir_support(eth_dev))
		return -ENOTSUP;

	qede_dealloc_fp_resc(eth_dev);
	qdev->num_tx_queues = eth_dev->data->nb_tx_queues;
	qdev->num_rx_queues = eth_dev->data->nb_rx_queues;
	if (qede_alloc_fp_resc(qdev))
		return -ENOMEM;

	/* If jumbo enabled adjust MTU */
	if (rxmode->offloads & DEV_RX_OFFLOAD_JUMBO_FRAME)
		eth_dev->data->mtu =
			eth_dev->data->dev_conf.rxmode.max_rx_pkt_len -
			ETHER_HDR_LEN - ETHER_CRC_LEN;

	if (rxmode->offloads & DEV_RX_OFFLOAD_SCATTER)
		eth_dev->data->scattered_rx = 1;

	if (qede_start_vport(qdev, eth_dev->data->mtu))
		return -1;

	qdev->mtu = eth_dev->data->mtu;

	/* Enable VLAN offloads by default */
	ret = qede_vlan_offload_set(eth_dev, ETH_VLAN_STRIP_MASK  |
					     ETH_VLAN_FILTER_MASK |
					     ETH_VLAN_EXTEND_MASK);
	if (ret)
		return ret;

	DP_INFO(edev, "Device configured with RSS=%d TSS=%d\n",
			QEDE_RSS_COUNT(qdev), QEDE_TSS_COUNT(qdev));

	return 0;
}

/* Info about HW descriptor ring limitations */
static const struct rte_eth_desc_lim qede_rx_desc_lim = {
	.nb_max = 0x8000, /* 32K */
	.nb_min = 128,
	.nb_align = 128 /* lowest common multiple */
};

static const struct rte_eth_desc_lim qede_tx_desc_lim = {
	.nb_max = 0x8000, /* 32K */
	.nb_min = 256,
	.nb_align = 256,
	.nb_seg_max = ETH_TX_MAX_BDS_PER_LSO_PACKET,
	.nb_mtu_seg_max = ETH_TX_MAX_BDS_PER_NON_LSO_PACKET
};

static void
qede_dev_info_get(struct rte_eth_dev *eth_dev,
		  struct rte_eth_dev_info *dev_info)
{
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;
	struct qed_link_output link;
	uint32_t speed_cap = 0;

	PMD_INIT_FUNC_TRACE(edev);

	dev_info->min_rx_bufsize = (uint32_t)QEDE_MIN_RX_BUFF_SIZE;
	dev_info->max_rx_pktlen = (uint32_t)ETH_TX_MAX_NON_LSO_PKT_LEN;
	dev_info->rx_desc_lim = qede_rx_desc_lim;
	dev_info->tx_desc_lim = qede_tx_desc_lim;

	if (IS_PF(edev))
		dev_info->max_rx_queues = (uint16_t)RTE_MIN(
			QEDE_MAX_RSS_CNT(qdev), QEDE_PF_NUM_CONNS / 2);
	else
		dev_info->max_rx_queues = (uint16_t)RTE_MIN(
			QEDE_MAX_RSS_CNT(qdev), ECORE_MAX_VF_CHAINS_PER_PF);
	dev_info->max_tx_queues = dev_info->max_rx_queues;

	dev_info->max_mac_addrs = qdev->dev_info.num_mac_filters;
	dev_info->max_vfs = 0;
	dev_info->reta_size = ECORE_RSS_IND_TABLE_SIZE;
	dev_info->hash_key_size = ECORE_RSS_KEY_SIZE * sizeof(uint32_t);
	dev_info->flow_type_rss_offloads = (uint64_t)QEDE_RSS_OFFLOAD_ALL;
	dev_info->rx_offload_capa = (DEV_RX_OFFLOAD_IPV4_CKSUM	|
				     DEV_RX_OFFLOAD_UDP_CKSUM	|
				     DEV_RX_OFFLOAD_TCP_CKSUM	|
				     DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM |
				     DEV_RX_OFFLOAD_TCP_LRO	|
				     DEV_RX_OFFLOAD_CRC_STRIP	|
				     DEV_RX_OFFLOAD_SCATTER	|
				     DEV_RX_OFFLOAD_JUMBO_FRAME |
				     DEV_RX_OFFLOAD_VLAN_FILTER |
				     DEV_RX_OFFLOAD_VLAN_STRIP);
	dev_info->rx_queue_offload_capa = 0;

	/* TX offloads are on a per-packet basis, so it is applicable
	 * to both at port and queue levels.
	 */
	dev_info->tx_offload_capa = (DEV_TX_OFFLOAD_VLAN_INSERT	|
				     DEV_TX_OFFLOAD_IPV4_CKSUM	|
				     DEV_TX_OFFLOAD_UDP_CKSUM	|
				     DEV_TX_OFFLOAD_TCP_CKSUM	|
				     DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
				     DEV_TX_OFFLOAD_QINQ_INSERT |
				     DEV_TX_OFFLOAD_MULTI_SEGS  |
				     DEV_TX_OFFLOAD_TCP_TSO	|
				     DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
				     DEV_TX_OFFLOAD_GENEVE_TNL_TSO);
	dev_info->tx_queue_offload_capa = dev_info->tx_offload_capa;

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.offloads = DEV_TX_OFFLOAD_MULTI_SEGS,
	};

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		/* Packets are always dropped if no descriptors are available */
		.rx_drop_en = 1,
		/* The below RX offloads are always enabled */
		.offloads = (DEV_RX_OFFLOAD_CRC_STRIP  |
			     DEV_RX_OFFLOAD_IPV4_CKSUM |
			     DEV_RX_OFFLOAD_TCP_CKSUM  |
			     DEV_RX_OFFLOAD_UDP_CKSUM),
	};

	memset(&link, 0, sizeof(struct qed_link_output));
	qdev->ops->common->get_link(edev, &link);
	if (link.adv_speed & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_1G)
		speed_cap |= ETH_LINK_SPEED_1G;
	if (link.adv_speed & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_10G)
		speed_cap |= ETH_LINK_SPEED_10G;
	if (link.adv_speed & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_25G)
		speed_cap |= ETH_LINK_SPEED_25G;
	if (link.adv_speed & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_40G)
		speed_cap |= ETH_LINK_SPEED_40G;
	if (link.adv_speed & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_50G)
		speed_cap |= ETH_LINK_SPEED_50G;
	if (link.adv_speed & NVM_CFG1_PORT_DRV_SPEED_CAPABILITY_MASK_BB_100G)
		speed_cap |= ETH_LINK_SPEED_100G;
	dev_info->speed_capa = speed_cap;
}

/* return 0 means link status changed, -1 means not changed */
int
qede_link_update(struct rte_eth_dev *eth_dev, __rte_unused int wait_to_complete)
{
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;
	uint16_t link_duplex;
	struct qed_link_output link;
	struct rte_eth_link *curr = &eth_dev->data->dev_link;

	memset(&link, 0, sizeof(struct qed_link_output));
	qdev->ops->common->get_link(edev, &link);

	/* Link Speed */
	curr->link_speed = link.speed;

	/* Link Mode */
	switch (link.duplex) {
	case QEDE_DUPLEX_HALF:
		link_duplex = ETH_LINK_HALF_DUPLEX;
		break;
	case QEDE_DUPLEX_FULL:
		link_duplex = ETH_LINK_FULL_DUPLEX;
		break;
	case QEDE_DUPLEX_UNKNOWN:
	default:
		link_duplex = -1;
	}
	curr->link_duplex = link_duplex;

	/* Link Status */
	curr->link_status = (link.link_up) ? ETH_LINK_UP : ETH_LINK_DOWN;

	/* AN */
	curr->link_autoneg = (link.supported_caps & QEDE_SUPPORTED_AUTONEG) ?
			     ETH_LINK_AUTONEG : ETH_LINK_FIXED;

	DP_INFO(edev, "Link - Speed %u Mode %u AN %u Status %u\n",
		curr->link_speed, curr->link_duplex,
		curr->link_autoneg, curr->link_status);

	/* return 0 means link status changed, -1 means not changed */
	return ((curr->link_status == link.link_up) ? -1 : 0);
}

static void qede_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
#ifdef RTE_LIBRTE_QEDE_DEBUG_INIT
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;

	PMD_INIT_FUNC_TRACE(edev);
#endif

	enum qed_filter_rx_mode_type type = QED_FILTER_RX_MODE_TYPE_PROMISC;

	if (rte_eth_allmulticast_get(eth_dev->data->port_id) == 1)
		type |= QED_FILTER_RX_MODE_TYPE_MULTI_PROMISC;

	qed_configure_filter_rx_mode(eth_dev, type);
}

static void qede_promiscuous_disable(struct rte_eth_dev *eth_dev)
{
#ifdef RTE_LIBRTE_QEDE_DEBUG_INIT
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;

	PMD_INIT_FUNC_TRACE(edev);
#endif

	if (rte_eth_allmulticast_get(eth_dev->data->port_id) == 1)
		qed_configure_filter_rx_mode(eth_dev,
				QED_FILTER_RX_MODE_TYPE_MULTI_PROMISC);
	else
		qed_configure_filter_rx_mode(eth_dev,
				QED_FILTER_RX_MODE_TYPE_REGULAR);
}

static void qede_poll_sp_sb_cb(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	int rc;

	qede_interrupt_action(ECORE_LEADING_HWFN(edev));
	qede_interrupt_action(&edev->hwfns[1]);

	rc = rte_eal_alarm_set(QEDE_SP_TIMER_PERIOD,
			       qede_poll_sp_sb_cb,
			       (void *)eth_dev);
	if (rc != 0) {
		DP_ERR(edev, "Unable to start periodic"
			     " timer rc %d\n", rc);
		assert(false && "Unable to start periodic timer");
	}
}

static void qede_dev_close(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);

	PMD_INIT_FUNC_TRACE(edev);

	/* dev_stop() shall cleanup fp resources in hw but without releasing
	 * dma memories and sw structures so that dev_start() can be called
	 * by the app without reconfiguration. However, in dev_close() we
	 * can release all the resources and device can be brought up newly
	 */
	if (eth_dev->data->dev_started)
		qede_dev_stop(eth_dev);

	qede_stop_vport(edev);
	qdev->vport_started = false;
	qede_fdir_dealloc_resc(eth_dev);
	qede_dealloc_fp_resc(eth_dev);

	eth_dev->data->nb_rx_queues = 0;
	eth_dev->data->nb_tx_queues = 0;

	/* Bring the link down */
	qede_dev_set_link_state(eth_dev, false);
	qdev->ops->common->slowpath_stop(edev);
	qdev->ops->common->remove(edev);
	rte_intr_disable(&pci_dev->intr_handle);
	rte_intr_callback_unregister(&pci_dev->intr_handle,
				     qede_interrupt_handler, (void *)eth_dev);
	if (ECORE_IS_CMT(edev))
		rte_eal_alarm_cancel(qede_poll_sp_sb_cb, (void *)eth_dev);
}

static int
qede_get_stats(struct rte_eth_dev *eth_dev, struct rte_eth_stats *eth_stats)
{
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;
	struct ecore_eth_stats stats;
	unsigned int i = 0, j = 0, qid;
	unsigned int rxq_stat_cntrs, txq_stat_cntrs;
	struct qede_tx_queue *txq;

	ecore_get_vport_stats(edev, &stats);

	/* RX Stats */
	eth_stats->ipackets = stats.common.rx_ucast_pkts +
	    stats.common.rx_mcast_pkts + stats.common.rx_bcast_pkts;

	eth_stats->ibytes = stats.common.rx_ucast_bytes +
	    stats.common.rx_mcast_bytes + stats.common.rx_bcast_bytes;

	eth_stats->ierrors = stats.common.rx_crc_errors +
	    stats.common.rx_align_errors +
	    stats.common.rx_carrier_errors +
	    stats.common.rx_oversize_packets +
	    stats.common.rx_jabbers + stats.common.rx_undersize_packets;

	eth_stats->rx_nombuf = stats.common.no_buff_discards;

	eth_stats->imissed = stats.common.mftag_filter_discards +
	    stats.common.mac_filter_discards +
	    stats.common.no_buff_discards +
	    stats.common.brb_truncates + stats.common.brb_discards;

	/* TX stats */
	eth_stats->opackets = stats.common.tx_ucast_pkts +
	    stats.common.tx_mcast_pkts + stats.common.tx_bcast_pkts;

	eth_stats->obytes = stats.common.tx_ucast_bytes +
	    stats.common.tx_mcast_bytes + stats.common.tx_bcast_bytes;

	eth_stats->oerrors = stats.common.tx_err_drop_pkts;

	/* Queue stats */
	rxq_stat_cntrs = RTE_MIN(QEDE_RSS_COUNT(qdev),
			       RTE_ETHDEV_QUEUE_STAT_CNTRS);
	txq_stat_cntrs = RTE_MIN(QEDE_TSS_COUNT(qdev),
			       RTE_ETHDEV_QUEUE_STAT_CNTRS);
	if ((rxq_stat_cntrs != (unsigned int)QEDE_RSS_COUNT(qdev)) ||
	    (txq_stat_cntrs != (unsigned int)QEDE_TSS_COUNT(qdev)))
		DP_VERBOSE(edev, ECORE_MSG_DEBUG,
		       "Not all the queue stats will be displayed. Set"
		       " RTE_ETHDEV_QUEUE_STAT_CNTRS config param"
		       " appropriately and retry.\n");

	for_each_rss(qid) {
		eth_stats->q_ipackets[i] =
			*(uint64_t *)(
				((char *)(qdev->fp_array[qid].rxq)) +
				offsetof(struct qede_rx_queue,
				rcv_pkts));
		eth_stats->q_errors[i] =
			*(uint64_t *)(
				((char *)(qdev->fp_array[qid].rxq)) +
				offsetof(struct qede_rx_queue,
				rx_hw_errors)) +
			*(uint64_t *)(
				((char *)(qdev->fp_array[qid].rxq)) +
				offsetof(struct qede_rx_queue,
				rx_alloc_errors));
		i++;
		if (i == rxq_stat_cntrs)
			break;
	}

	for_each_tss(qid) {
		txq = qdev->fp_array[qid].txq;
		eth_stats->q_opackets[j] =
			*((uint64_t *)(uintptr_t)
				(((uint64_t)(uintptr_t)(txq)) +
				 offsetof(struct qede_tx_queue,
					  xmit_pkts)));
		j++;
		if (j == txq_stat_cntrs)
			break;
	}

	return 0;
}

static unsigned
qede_get_xstats_count(struct qede_dev *qdev) {
	if (ECORE_IS_BB(&qdev->edev))
		return RTE_DIM(qede_xstats_strings) +
		       RTE_DIM(qede_bb_xstats_strings) +
		       (RTE_DIM(qede_rxq_xstats_strings) *
			RTE_MIN(QEDE_RSS_COUNT(qdev),
				RTE_ETHDEV_QUEUE_STAT_CNTRS));
	else
		return RTE_DIM(qede_xstats_strings) +
		       RTE_DIM(qede_ah_xstats_strings) +
		       (RTE_DIM(qede_rxq_xstats_strings) *
			RTE_MIN(QEDE_RSS_COUNT(qdev),
				RTE_ETHDEV_QUEUE_STAT_CNTRS));
}

static int
qede_get_xstats_names(struct rte_eth_dev *dev,
		      struct rte_eth_xstat_name *xstats_names,
		      __rte_unused unsigned int limit)
{
	struct qede_dev *qdev = dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;
	const unsigned int stat_cnt = qede_get_xstats_count(qdev);
	unsigned int i, qid, stat_idx = 0;
	unsigned int rxq_stat_cntrs;

	if (xstats_names != NULL) {
		for (i = 0; i < RTE_DIM(qede_xstats_strings); i++) {
			snprintf(xstats_names[stat_idx].name,
				sizeof(xstats_names[stat_idx].name),
				"%s",
				qede_xstats_strings[i].name);
			stat_idx++;
		}

		if (ECORE_IS_BB(edev)) {
			for (i = 0; i < RTE_DIM(qede_bb_xstats_strings); i++) {
				snprintf(xstats_names[stat_idx].name,
					sizeof(xstats_names[stat_idx].name),
					"%s",
					qede_bb_xstats_strings[i].name);
				stat_idx++;
			}
		} else {
			for (i = 0; i < RTE_DIM(qede_ah_xstats_strings); i++) {
				snprintf(xstats_names[stat_idx].name,
					sizeof(xstats_names[stat_idx].name),
					"%s",
					qede_ah_xstats_strings[i].name);
				stat_idx++;
			}
		}

		rxq_stat_cntrs = RTE_MIN(QEDE_RSS_COUNT(qdev),
					 RTE_ETHDEV_QUEUE_STAT_CNTRS);
		for (qid = 0; qid < rxq_stat_cntrs; qid++) {
			for (i = 0; i < RTE_DIM(qede_rxq_xstats_strings); i++) {
				snprintf(xstats_names[stat_idx].name,
					sizeof(xstats_names[stat_idx].name),
					"%.4s%d%s",
					qede_rxq_xstats_strings[i].name, qid,
					qede_rxq_xstats_strings[i].name + 4);
				stat_idx++;
			}
		}
	}

	return stat_cnt;
}

static int
qede_get_xstats(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		unsigned int n)
{
	struct qede_dev *qdev = dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;
	struct ecore_eth_stats stats;
	const unsigned int num = qede_get_xstats_count(qdev);
	unsigned int i, qid, stat_idx = 0;
	unsigned int rxq_stat_cntrs;

	if (n < num)
		return num;

	ecore_get_vport_stats(edev, &stats);

	for (i = 0; i < RTE_DIM(qede_xstats_strings); i++) {
		xstats[stat_idx].value = *(uint64_t *)(((char *)&stats) +
					     qede_xstats_strings[i].offset);
		xstats[stat_idx].id = stat_idx;
		stat_idx++;
	}

	if (ECORE_IS_BB(edev)) {
		for (i = 0; i < RTE_DIM(qede_bb_xstats_strings); i++) {
			xstats[stat_idx].value =
					*(uint64_t *)(((char *)&stats) +
					qede_bb_xstats_strings[i].offset);
			xstats[stat_idx].id = stat_idx;
			stat_idx++;
		}
	} else {
		for (i = 0; i < RTE_DIM(qede_ah_xstats_strings); i++) {
			xstats[stat_idx].value =
					*(uint64_t *)(((char *)&stats) +
					qede_ah_xstats_strings[i].offset);
			xstats[stat_idx].id = stat_idx;
			stat_idx++;
		}
	}

	rxq_stat_cntrs = RTE_MIN(QEDE_RSS_COUNT(qdev),
				 RTE_ETHDEV_QUEUE_STAT_CNTRS);
	for (qid = 0; qid < rxq_stat_cntrs; qid++) {
		for_each_rss(qid) {
			for (i = 0; i < RTE_DIM(qede_rxq_xstats_strings); i++) {
				xstats[stat_idx].value = *(uint64_t *)(
					((char *)(qdev->fp_array[qid].rxq)) +
					 qede_rxq_xstats_strings[i].offset);
				xstats[stat_idx].id = stat_idx;
				stat_idx++;
			}
		}
	}

	return stat_idx;
}

static void
qede_reset_xstats(struct rte_eth_dev *dev)
{
	struct qede_dev *qdev = dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;

	ecore_reset_vport_stats(edev);
	qede_reset_queue_stats(qdev, true);
}

int qede_dev_set_link_state(struct rte_eth_dev *eth_dev, bool link_up)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct qed_link_params link_params;
	int rc;

	DP_INFO(edev, "setting link state %d\n", link_up);
	memset(&link_params, 0, sizeof(link_params));
	link_params.link_up = link_up;
	rc = qdev->ops->common->set_link(edev, &link_params);
	if (rc != ECORE_SUCCESS)
		DP_ERR(edev, "Unable to set link state %d\n", link_up);

	return rc;
}

static int qede_dev_set_link_up(struct rte_eth_dev *eth_dev)
{
	return qede_dev_set_link_state(eth_dev, true);
}

static int qede_dev_set_link_down(struct rte_eth_dev *eth_dev)
{
	return qede_dev_set_link_state(eth_dev, false);
}

static void qede_reset_stats(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;

	ecore_reset_vport_stats(edev);
	qede_reset_queue_stats(qdev, false);
}

static void qede_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	enum qed_filter_rx_mode_type type =
	    QED_FILTER_RX_MODE_TYPE_MULTI_PROMISC;

	if (rte_eth_promiscuous_get(eth_dev->data->port_id) == 1)
		type |= QED_FILTER_RX_MODE_TYPE_PROMISC;

	qed_configure_filter_rx_mode(eth_dev, type);
}

static void qede_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	if (rte_eth_promiscuous_get(eth_dev->data->port_id) == 1)
		qed_configure_filter_rx_mode(eth_dev,
				QED_FILTER_RX_MODE_TYPE_PROMISC);
	else
		qed_configure_filter_rx_mode(eth_dev,
				QED_FILTER_RX_MODE_TYPE_REGULAR);
}

static int
qede_set_mc_addr_list(struct rte_eth_dev *eth_dev, struct ether_addr *mc_addrs,
		      uint32_t mc_addrs_num)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	uint8_t i;

	if (mc_addrs_num > ECORE_MAX_MC_ADDRS) {
		DP_ERR(edev, "Reached max multicast filters limit,"
			     "Please enable multicast promisc mode\n");
		return -ENOSPC;
	}

	for (i = 0; i < mc_addrs_num; i++) {
		if (!is_multicast_ether_addr(&mc_addrs[i])) {
			DP_ERR(edev, "Not a valid multicast MAC\n");
			return -EINVAL;
		}
	}

	/* Flush all existing entries */
	if (qede_del_mcast_filters(eth_dev))
		return -1;

	/* Set new mcast list */
	return qede_add_mcast_filters(eth_dev, mc_addrs, mc_addrs_num);
}

static int qede_flow_ctrl_set(struct rte_eth_dev *eth_dev,
			      struct rte_eth_fc_conf *fc_conf)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct qed_link_output current_link;
	struct qed_link_params params;

	memset(&current_link, 0, sizeof(current_link));
	qdev->ops->common->get_link(edev, &current_link);

	memset(&params, 0, sizeof(params));
	params.override_flags |= QED_LINK_OVERRIDE_PAUSE_CONFIG;
	if (fc_conf->autoneg) {
		if (!(current_link.supported_caps & QEDE_SUPPORTED_AUTONEG)) {
			DP_ERR(edev, "Autoneg not supported\n");
			return -EINVAL;
		}
		params.pause_config |= QED_LINK_PAUSE_AUTONEG_ENABLE;
	}

	/* Pause is assumed to be supported (SUPPORTED_Pause) */
	if (fc_conf->mode == RTE_FC_FULL)
		params.pause_config |= (QED_LINK_PAUSE_TX_ENABLE |
					QED_LINK_PAUSE_RX_ENABLE);
	if (fc_conf->mode == RTE_FC_TX_PAUSE)
		params.pause_config |= QED_LINK_PAUSE_TX_ENABLE;
	if (fc_conf->mode == RTE_FC_RX_PAUSE)
		params.pause_config |= QED_LINK_PAUSE_RX_ENABLE;

	params.link_up = true;
	(void)qdev->ops->common->set_link(edev, &params);

	return 0;
}

static int qede_flow_ctrl_get(struct rte_eth_dev *eth_dev,
			      struct rte_eth_fc_conf *fc_conf)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct qed_link_output current_link;

	memset(&current_link, 0, sizeof(current_link));
	qdev->ops->common->get_link(edev, &current_link);

	if (current_link.pause_config & QED_LINK_PAUSE_AUTONEG_ENABLE)
		fc_conf->autoneg = true;

	if (current_link.pause_config & (QED_LINK_PAUSE_RX_ENABLE |
					 QED_LINK_PAUSE_TX_ENABLE))
		fc_conf->mode = RTE_FC_FULL;
	else if (current_link.pause_config & QED_LINK_PAUSE_RX_ENABLE)
		fc_conf->mode = RTE_FC_RX_PAUSE;
	else if (current_link.pause_config & QED_LINK_PAUSE_TX_ENABLE)
		fc_conf->mode = RTE_FC_TX_PAUSE;
	else
		fc_conf->mode = RTE_FC_NONE;

	return 0;
}

static const uint32_t *
qede_dev_supported_ptypes_get(struct rte_eth_dev *eth_dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L2_ETHER_VLAN,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_TUNNEL_VXLAN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_TUNNEL_GENEVE,
		RTE_PTYPE_TUNNEL_GRE,
		/* Inner */
		RTE_PTYPE_INNER_L2_ETHER,
		RTE_PTYPE_INNER_L2_ETHER_VLAN,
		RTE_PTYPE_INNER_L3_IPV4,
		RTE_PTYPE_INNER_L3_IPV6,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_UNKNOWN
	};

	if (eth_dev->rx_pkt_burst == qede_recv_pkts)
		return ptypes;

	return NULL;
}

static void qede_init_rss_caps(uint8_t *rss_caps, uint64_t hf)
{
	*rss_caps = 0;
	*rss_caps |= (hf & ETH_RSS_IPV4)              ? ECORE_RSS_IPV4 : 0;
	*rss_caps |= (hf & ETH_RSS_IPV6)              ? ECORE_RSS_IPV6 : 0;
	*rss_caps |= (hf & ETH_RSS_IPV6_EX)           ? ECORE_RSS_IPV6 : 0;
	*rss_caps |= (hf & ETH_RSS_NONFRAG_IPV4_TCP)  ? ECORE_RSS_IPV4_TCP : 0;
	*rss_caps |= (hf & ETH_RSS_NONFRAG_IPV6_TCP)  ? ECORE_RSS_IPV6_TCP : 0;
	*rss_caps |= (hf & ETH_RSS_IPV6_TCP_EX)       ? ECORE_RSS_IPV6_TCP : 0;
	*rss_caps |= (hf & ETH_RSS_NONFRAG_IPV4_UDP)  ? ECORE_RSS_IPV4_UDP : 0;
	*rss_caps |= (hf & ETH_RSS_NONFRAG_IPV6_UDP)  ? ECORE_RSS_IPV6_UDP : 0;
}

int qede_rss_hash_update(struct rte_eth_dev *eth_dev,
			 struct rte_eth_rss_conf *rss_conf)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_sp_vport_update_params vport_update_params;
	struct ecore_rss_params rss_params;
	struct ecore_hwfn *p_hwfn;
	uint32_t *key = (uint32_t *)rss_conf->rss_key;
	uint64_t hf = rss_conf->rss_hf;
	uint8_t len = rss_conf->rss_key_len;
	uint8_t idx;
	uint8_t i;
	int rc;

	memset(&vport_update_params, 0, sizeof(vport_update_params));
	memset(&rss_params, 0, sizeof(rss_params));

	DP_INFO(edev, "RSS hf = 0x%lx len = %u key = %p\n",
		(unsigned long)hf, len, key);

	if (hf != 0) {
		/* Enabling RSS */
		DP_INFO(edev, "Enabling rss\n");

		/* RSS caps */
		qede_init_rss_caps(&rss_params.rss_caps, hf);
		rss_params.update_rss_capabilities = 1;

		/* RSS hash key */
		if (key) {
			if (len > (ECORE_RSS_KEY_SIZE * sizeof(uint32_t))) {
				DP_ERR(edev, "RSS key length exceeds limit\n");
				return -EINVAL;
			}
			DP_INFO(edev, "Applying user supplied hash key\n");
			rss_params.update_rss_key = 1;
			memcpy(&rss_params.rss_key, key, len);
		}
		rss_params.rss_enable = 1;
	}

	rss_params.update_rss_config = 1;
	/* tbl_size has to be set with capabilities */
	rss_params.rss_table_size_log = 7;
	vport_update_params.vport_id = 0;
	/* pass the L2 handles instead of qids */
	for (i = 0 ; i < ECORE_RSS_IND_TABLE_SIZE ; i++) {
		idx = qdev->rss_ind_table[i];
		rss_params.rss_ind_table[i] = qdev->fp_array[idx].rxq->handle;
	}
	vport_update_params.rss_params = &rss_params;

	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		vport_update_params.opaque_fid = p_hwfn->hw_info.opaque_fid;
		rc = ecore_sp_vport_update(p_hwfn, &vport_update_params,
					   ECORE_SPQ_MODE_EBLOCK, NULL);
		if (rc) {
			DP_ERR(edev, "vport-update for RSS failed\n");
			return rc;
		}
	}
	qdev->rss_enable = rss_params.rss_enable;

	/* Update local structure for hash query */
	qdev->rss_conf.rss_hf = hf;
	qdev->rss_conf.rss_key_len = len;
	if (qdev->rss_enable) {
		if  (qdev->rss_conf.rss_key == NULL) {
			qdev->rss_conf.rss_key = (uint8_t *)malloc(len);
			if (qdev->rss_conf.rss_key == NULL) {
				DP_ERR(edev, "No memory to store RSS key\n");
				return -ENOMEM;
			}
		}
		if (key && len) {
			DP_INFO(edev, "Storing RSS key\n");
			memcpy(qdev->rss_conf.rss_key, key, len);
		}
	} else if (!qdev->rss_enable && len == 0) {
		if (qdev->rss_conf.rss_key) {
			free(qdev->rss_conf.rss_key);
			qdev->rss_conf.rss_key = NULL;
			DP_INFO(edev, "Free RSS key\n");
		}
	}

	return 0;
}

static int qede_rss_hash_conf_get(struct rte_eth_dev *eth_dev,
			   struct rte_eth_rss_conf *rss_conf)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);

	rss_conf->rss_hf = qdev->rss_conf.rss_hf;
	rss_conf->rss_key_len = qdev->rss_conf.rss_key_len;

	if (rss_conf->rss_key && qdev->rss_conf.rss_key)
		memcpy(rss_conf->rss_key, qdev->rss_conf.rss_key,
		       rss_conf->rss_key_len);
	return 0;
}

static bool qede_update_rss_parm_cmt(struct ecore_dev *edev,
				    struct ecore_rss_params *rss)
{
	int i, fn;
	bool rss_mode = 1; /* enable */
	struct ecore_queue_cid *cid;
	struct ecore_rss_params *t_rss;

	/* In regular scenario, we'd simply need to take input handlers.
	 * But in CMT, we'd have to split the handlers according to the
	 * engine they were configured on. We'd then have to understand
	 * whether RSS is really required, since 2-queues on CMT doesn't
	 * require RSS.
	 */

	/* CMT should be round-robin */
	for (i = 0; i < ECORE_RSS_IND_TABLE_SIZE; i++) {
		cid = rss->rss_ind_table[i];

		if (cid->p_owner == ECORE_LEADING_HWFN(edev))
			t_rss = &rss[0];
		else
			t_rss = &rss[1];

		t_rss->rss_ind_table[i / edev->num_hwfns] = cid;
	}

	t_rss = &rss[1];
	t_rss->update_rss_ind_table = 1;
	t_rss->rss_table_size_log = 7;
	t_rss->update_rss_config = 1;

	/* Make sure RSS is actually required */
	for_each_hwfn(edev, fn) {
		for (i = 1; i < ECORE_RSS_IND_TABLE_SIZE / edev->num_hwfns;
		     i++) {
			if (rss[fn].rss_ind_table[i] !=
			    rss[fn].rss_ind_table[0])
				break;
		}

		if (i == ECORE_RSS_IND_TABLE_SIZE / edev->num_hwfns) {
			DP_INFO(edev,
				"CMT - 1 queue per-hwfn; Disabling RSS\n");
			rss_mode = 0;
			goto out;
		}
	}

out:
	t_rss->rss_enable = rss_mode;

	return rss_mode;
}

int qede_rss_reta_update(struct rte_eth_dev *eth_dev,
			 struct rte_eth_rss_reta_entry64 *reta_conf,
			 uint16_t reta_size)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_sp_vport_update_params vport_update_params;
	struct ecore_rss_params *params;
	struct ecore_hwfn *p_hwfn;
	uint16_t i, idx, shift;
	uint8_t entry;
	int rc = 0;

	if (reta_size > ETH_RSS_RETA_SIZE_128) {
		DP_ERR(edev, "reta_size %d is not supported by hardware\n",
		       reta_size);
		return -EINVAL;
	}

	memset(&vport_update_params, 0, sizeof(vport_update_params));
	params = rte_zmalloc("qede_rss", sizeof(*params) * edev->num_hwfns,
			     RTE_CACHE_LINE_SIZE);
	if (params == NULL) {
		DP_ERR(edev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift)) {
			entry = reta_conf[idx].reta[shift];
			/* Pass rxq handles to ecore */
			params->rss_ind_table[i] =
					qdev->fp_array[entry].rxq->handle;
			/* Update the local copy for RETA query command */
			qdev->rss_ind_table[i] = entry;
		}
	}

	params->update_rss_ind_table = 1;
	params->rss_table_size_log = 7;
	params->update_rss_config = 1;

	/* Fix up RETA for CMT mode device */
	if (ECORE_IS_CMT(edev))
		qdev->rss_enable = qede_update_rss_parm_cmt(edev,
							    params);
	vport_update_params.vport_id = 0;
	/* Use the current value of rss_enable */
	params->rss_enable = qdev->rss_enable;
	vport_update_params.rss_params = params;

	for_each_hwfn(edev, i) {
		p_hwfn = &edev->hwfns[i];
		vport_update_params.opaque_fid = p_hwfn->hw_info.opaque_fid;
		rc = ecore_sp_vport_update(p_hwfn, &vport_update_params,
					   ECORE_SPQ_MODE_EBLOCK, NULL);
		if (rc) {
			DP_ERR(edev, "vport-update for RSS failed\n");
			goto out;
		}
	}

out:
	rte_free(params);
	return rc;
}

static int qede_rss_reta_query(struct rte_eth_dev *eth_dev,
			       struct rte_eth_rss_reta_entry64 *reta_conf,
			       uint16_t reta_size)
{
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;
	uint16_t i, idx, shift;
	uint8_t entry;

	if (reta_size > ETH_RSS_RETA_SIZE_128) {
		DP_ERR(edev, "reta_size %d is not supported\n",
		       reta_size);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift)) {
			entry = qdev->rss_ind_table[i];
			reta_conf[idx].reta[shift] = entry;
		}
	}

	return 0;
}



static int qede_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct rte_eth_dev_info dev_info = {0};
	struct qede_fastpath *fp;
	uint32_t max_rx_pkt_len;
	uint32_t frame_size;
	uint16_t rx_buf_size;
	uint16_t bufsz;
	bool restart = false;
	int i;

	PMD_INIT_FUNC_TRACE(edev);
	qede_dev_info_get(dev, &dev_info);
	max_rx_pkt_len = mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
	frame_size = max_rx_pkt_len + QEDE_ETH_OVERHEAD;
	if ((mtu < ETHER_MIN_MTU) || (frame_size > dev_info.max_rx_pktlen)) {
		DP_ERR(edev, "MTU %u out of range, %u is maximum allowable\n",
		       mtu, dev_info.max_rx_pktlen - ETHER_HDR_LEN -
			ETHER_CRC_LEN - QEDE_ETH_OVERHEAD);
		return -EINVAL;
	}
	if (!dev->data->scattered_rx &&
	    frame_size > dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM) {
		DP_INFO(edev, "MTU greater than minimum RX buffer size of %u\n",
			dev->data->min_rx_buf_size);
		return -EINVAL;
	}
	/* Temporarily replace I/O functions with dummy ones. It cannot
	 * be set to NULL because rte_eth_rx_burst() doesn't check for NULL.
	 */
	dev->rx_pkt_burst = qede_rxtx_pkts_dummy;
	dev->tx_pkt_burst = qede_rxtx_pkts_dummy;
	if (dev->data->dev_started) {
		dev->data->dev_started = 0;
		qede_dev_stop(dev);
		restart = true;
	} else {
		if (IS_PF(edev))
			qede_mac_addr_remove(dev, 0);
	}
	rte_delay_ms(1000);
	qede_start_vport(qdev, mtu); /* Recreate vport */
	qdev->mtu = mtu;

	/* Fix up RX buf size for all queues of the port */
	for_each_rss(i) {
		fp = &qdev->fp_array[i];
		if (fp->rxq != NULL) {
			bufsz = (uint16_t)rte_pktmbuf_data_room_size(
				fp->rxq->mb_pool) - RTE_PKTMBUF_HEADROOM;
			if (dev->data->scattered_rx)
				rx_buf_size = bufsz + ETHER_HDR_LEN +
					      ETHER_CRC_LEN + QEDE_ETH_OVERHEAD;
			else
				rx_buf_size = frame_size;
			rx_buf_size = QEDE_CEIL_TO_CACHE_LINE_SIZE(rx_buf_size);
			fp->rxq->rx_buf_size = rx_buf_size;
			DP_INFO(edev, "RX buffer size %u\n", rx_buf_size);
		}
	}
	if (max_rx_pkt_len > ETHER_MAX_LEN)
		dev->data->dev_conf.rxmode.jumbo_frame = 1;
	else
		dev->data->dev_conf.rxmode.jumbo_frame = 0;

	/* Restore config lost due to vport stop */
	if (IS_PF(edev))
		qede_mac_addr_set(dev, &qdev->primary_mac);

	if (dev->data->promiscuous)
		qede_promiscuous_enable(dev);
	else
		qede_promiscuous_disable(dev);

	if (dev->data->all_multicast)
		qede_allmulticast_enable(dev);
	else
		qede_allmulticast_disable(dev);

	qede_vlan_offload_set(dev, qdev->vlan_offload_mask);

	if (!dev->data->dev_started && restart) {
		qede_dev_start(dev);
		dev->data->dev_started = 1;
	}

	/* update max frame size */
	dev->data->dev_conf.rxmode.max_rx_pkt_len = max_rx_pkt_len;
	/* Reassign back */
	dev->rx_pkt_burst = qede_recv_pkts;
	dev->tx_pkt_burst = qede_xmit_pkts;

	return 0;
}

static int
qede_udp_dst_port_del(struct rte_eth_dev *eth_dev,
		      struct rte_eth_udp_tunnel *tunnel_udp)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_tunnel_info tunn; /* @DPDK */
	uint16_t udp_port;
	int rc;

	PMD_INIT_FUNC_TRACE(edev);

	memset(&tunn, 0, sizeof(tunn));

	switch (tunnel_udp->prot_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		if (qdev->vxlan.udp_port != tunnel_udp->udp_port) {
			DP_ERR(edev, "UDP port %u doesn't exist\n",
				tunnel_udp->udp_port);
			return ECORE_INVAL;
		}
		udp_port = 0;

		tunn.vxlan_port.b_update_port = true;
		tunn.vxlan_port.port = udp_port;

		rc = qede_tunnel_update(qdev, &tunn);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Unable to config UDP port %u\n",
			       tunn.vxlan_port.port);
			return rc;
		}

		qdev->vxlan.udp_port = udp_port;
		/* If the request is to delete UDP port and if the number of
		 * VXLAN filters have reached 0 then VxLAN offload can be be
		 * disabled.
		 */
		if (qdev->vxlan.enable && qdev->vxlan.num_filters == 0)
			return qede_vxlan_enable(eth_dev,
					ECORE_TUNN_CLSS_MAC_VLAN, false);

		break;
	case RTE_TUNNEL_TYPE_GENEVE:
		if (qdev->geneve.udp_port != tunnel_udp->udp_port) {
			DP_ERR(edev, "UDP port %u doesn't exist\n",
				tunnel_udp->udp_port);
			return ECORE_INVAL;
		}

		udp_port = 0;

		tunn.geneve_port.b_update_port = true;
		tunn.geneve_port.port = udp_port;

		rc = qede_tunnel_update(qdev, &tunn);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Unable to config UDP port %u\n",
			       tunn.vxlan_port.port);
			return rc;
		}

		qdev->vxlan.udp_port = udp_port;
		/* If the request is to delete UDP port and if the number of
		 * GENEVE filters have reached 0 then GENEVE offload can be be
		 * disabled.
		 */
		if (qdev->geneve.enable && qdev->geneve.num_filters == 0)
			return qede_geneve_enable(eth_dev,
					ECORE_TUNN_CLSS_MAC_VLAN, false);

		break;

	default:
		return ECORE_INVAL;
	}

	return 0;

}
static int
qede_udp_dst_port_add(struct rte_eth_dev *eth_dev,
		      struct rte_eth_udp_tunnel *tunnel_udp)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_tunnel_info tunn; /* @DPDK */
	uint16_t udp_port;
	int rc;

	PMD_INIT_FUNC_TRACE(edev);

	memset(&tunn, 0, sizeof(tunn));

	switch (tunnel_udp->prot_type) {
	case RTE_TUNNEL_TYPE_VXLAN:
		if (qdev->vxlan.udp_port == tunnel_udp->udp_port) {
			DP_INFO(edev,
				"UDP port %u for VXLAN was already configured\n",
				tunnel_udp->udp_port);
			return ECORE_SUCCESS;
		}

		/* Enable VxLAN tunnel with default MAC/VLAN classification if
		 * it was not enabled while adding VXLAN filter before UDP port
		 * update.
		 */
		if (!qdev->vxlan.enable) {
			rc = qede_vxlan_enable(eth_dev,
				ECORE_TUNN_CLSS_MAC_VLAN, true);
			if (rc != ECORE_SUCCESS) {
				DP_ERR(edev, "Failed to enable VXLAN "
					"prior to updating UDP port\n");
				return rc;
			}
		}
		udp_port = tunnel_udp->udp_port;

		tunn.vxlan_port.b_update_port = true;
		tunn.vxlan_port.port = udp_port;

		rc = qede_tunnel_update(qdev, &tunn);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Unable to config UDP port %u for VXLAN\n",
			       udp_port);
			return rc;
		}

		DP_INFO(edev, "Updated UDP port %u for VXLAN\n", udp_port);

		qdev->vxlan.udp_port = udp_port;
		break;
	case RTE_TUNNEL_TYPE_GENEVE:
		if (qdev->geneve.udp_port == tunnel_udp->udp_port) {
			DP_INFO(edev,
				"UDP port %u for GENEVE was already configured\n",
				tunnel_udp->udp_port);
			return ECORE_SUCCESS;
		}

		/* Enable GENEVE tunnel with default MAC/VLAN classification if
		 * it was not enabled while adding GENEVE filter before UDP port
		 * update.
		 */
		if (!qdev->geneve.enable) {
			rc = qede_geneve_enable(eth_dev,
				ECORE_TUNN_CLSS_MAC_VLAN, true);
			if (rc != ECORE_SUCCESS) {
				DP_ERR(edev, "Failed to enable GENEVE "
					"prior to updating UDP port\n");
				return rc;
			}
		}
		udp_port = tunnel_udp->udp_port;

		tunn.geneve_port.b_update_port = true;
		tunn.geneve_port.port = udp_port;

		rc = qede_tunnel_update(qdev, &tunn);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "Unable to config UDP port %u for GENEVE\n",
			       udp_port);
			return rc;
		}

		DP_INFO(edev, "Updated UDP port %u for GENEVE\n", udp_port);

		qdev->geneve.udp_port = udp_port;
		break;
	default:
		return ECORE_INVAL;
	}

	return 0;
}

static void qede_get_ecore_tunn_params(uint32_t filter, uint32_t *type,
				       uint32_t *clss, char *str)
{
	uint16_t j;
	*clss = MAX_ECORE_TUNN_CLSS;

	for (j = 0; j < RTE_DIM(qede_tunn_types); j++) {
		if (filter == qede_tunn_types[j].rte_filter_type) {
			*type = qede_tunn_types[j].qede_type;
			*clss = qede_tunn_types[j].qede_tunn_clss;
			strcpy(str, qede_tunn_types[j].string);
			return;
		}
	}
}

static int
qede_set_ucast_tunn_cmn_param(struct ecore_filter_ucast *ucast,
			      const struct rte_eth_tunnel_filter_conf *conf,
			      uint32_t type)
{
	/* Init commmon ucast params first */
	qede_set_ucast_cmn_params(ucast);

	/* Copy out the required fields based on classification type */
	ucast->type = type;

	switch (type) {
	case ECORE_FILTER_VNI:
		ucast->vni = conf->tenant_id;
	break;
	case ECORE_FILTER_INNER_VLAN:
		ucast->vlan = conf->inner_vlan;
	break;
	case ECORE_FILTER_MAC:
		memcpy(ucast->mac, conf->outer_mac.addr_bytes,
		       ETHER_ADDR_LEN);
	break;
	case ECORE_FILTER_INNER_MAC:
		memcpy(ucast->mac, conf->inner_mac.addr_bytes,
		       ETHER_ADDR_LEN);
	break;
	case ECORE_FILTER_MAC_VNI_PAIR:
		memcpy(ucast->mac, conf->outer_mac.addr_bytes,
			ETHER_ADDR_LEN);
		ucast->vni = conf->tenant_id;
	break;
	case ECORE_FILTER_INNER_MAC_VNI_PAIR:
		memcpy(ucast->mac, conf->inner_mac.addr_bytes,
			ETHER_ADDR_LEN);
		ucast->vni = conf->tenant_id;
	break;
	case ECORE_FILTER_INNER_PAIR:
		memcpy(ucast->mac, conf->inner_mac.addr_bytes,
			ETHER_ADDR_LEN);
		ucast->vlan = conf->inner_vlan;
	break;
	default:
		return -EINVAL;
	}

	return ECORE_SUCCESS;
}

static int
_qede_tunn_filter_config(struct rte_eth_dev *eth_dev,
			 const struct rte_eth_tunnel_filter_conf *conf,
			 __attribute__((unused)) enum rte_filter_op filter_op,
			 enum ecore_tunn_clss *clss,
			 bool add)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_filter_ucast ucast = {0};
	enum ecore_filter_ucast_type type;
	uint16_t filter_type = 0;
	char str[80];
	int rc;

	filter_type = conf->filter_type;
	/* Determine if the given filter classification is supported */
	qede_get_ecore_tunn_params(filter_type, &type, clss, str);
	if (*clss == MAX_ECORE_TUNN_CLSS) {
		DP_ERR(edev, "Unsupported filter type\n");
		return -EINVAL;
	}
	/* Init tunnel ucast params */
	rc = qede_set_ucast_tunn_cmn_param(&ucast, conf, type);
	if (rc != ECORE_SUCCESS) {
		DP_ERR(edev, "Unsupported Tunnel filter type 0x%x\n",
		conf->filter_type);
		return rc;
	}
	DP_INFO(edev, "Rule: \"%s\", op %d, type 0x%x\n",
		str, filter_op, ucast.type);

	ucast.opcode = add ? ECORE_FILTER_ADD : ECORE_FILTER_REMOVE;

	/* Skip MAC/VLAN if filter is based on VNI */
	if (!(filter_type & ETH_TUNNEL_FILTER_TENID)) {
		rc = qede_mac_int_ops(eth_dev, &ucast, add);
		if ((rc == 0) && add) {
			/* Enable accept anyvlan */
			qede_config_accept_any_vlan(qdev, true);
		}
	} else {
		rc = qede_ucast_filter(eth_dev, &ucast, add);
		if (rc == 0)
			rc = ecore_filter_ucast_cmd(edev, &ucast,
					    ECORE_SPQ_MODE_CB, NULL);
	}

	return rc;
}

static int
qede_tunn_filter_config(struct rte_eth_dev *eth_dev,
			enum rte_filter_op filter_op,
			const struct rte_eth_tunnel_filter_conf *conf)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	enum ecore_tunn_clss clss = MAX_ECORE_TUNN_CLSS;
	bool add;
	int rc;

	PMD_INIT_FUNC_TRACE(edev);

	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
		add = true;
		break;
	case RTE_ETH_FILTER_DELETE:
		add = false;
		break;
	default:
		DP_ERR(edev, "Unsupported operation %d\n", filter_op);
		return -EINVAL;
	}

	if (IS_VF(edev))
		return qede_tunn_enable(eth_dev,
					ECORE_TUNN_CLSS_MAC_VLAN,
					conf->tunnel_type, add);

	rc = _qede_tunn_filter_config(eth_dev, conf, filter_op, &clss, add);
	if (rc != ECORE_SUCCESS)
		return rc;

	if (add) {
		if (conf->tunnel_type == RTE_TUNNEL_TYPE_VXLAN) {
			qdev->vxlan.num_filters++;
			qdev->vxlan.filter_type = conf->filter_type;
		} else { /* GENEVE */
			qdev->geneve.num_filters++;
			qdev->geneve.filter_type = conf->filter_type;
		}

		if (!qdev->vxlan.enable || !qdev->geneve.enable ||
		    !qdev->ipgre.enable)
			return qede_tunn_enable(eth_dev, clss,
						conf->tunnel_type,
						true);
	} else {
		if (conf->tunnel_type == RTE_TUNNEL_TYPE_VXLAN)
			qdev->vxlan.num_filters--;
		else /*GENEVE*/
			qdev->geneve.num_filters--;

		/* Disable VXLAN if VXLAN filters become 0 */
		if ((qdev->vxlan.num_filters == 0) ||
		    (qdev->geneve.num_filters == 0))
			return qede_tunn_enable(eth_dev, clss,
						conf->tunnel_type,
						false);
	}

	return 0;
}

int qede_dev_filter_ctrl(struct rte_eth_dev *eth_dev,
			 enum rte_filter_type filter_type,
			 enum rte_filter_op filter_op,
			 void *arg)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct rte_eth_tunnel_filter_conf *filter_conf =
			(struct rte_eth_tunnel_filter_conf *)arg;

	switch (filter_type) {
	case RTE_ETH_FILTER_TUNNEL:
		switch (filter_conf->tunnel_type) {
		case RTE_TUNNEL_TYPE_VXLAN:
		case RTE_TUNNEL_TYPE_GENEVE:
		case RTE_TUNNEL_TYPE_IP_IN_GRE:
			DP_INFO(edev,
				"Packet steering to the specified Rx queue"
				" is not supported with UDP tunneling");
			return(qede_tunn_filter_config(eth_dev, filter_op,
						      filter_conf));
		case RTE_TUNNEL_TYPE_TEREDO:
		case RTE_TUNNEL_TYPE_NVGRE:
		case RTE_L2_TUNNEL_TYPE_E_TAG:
			DP_ERR(edev, "Unsupported tunnel type %d\n",
				filter_conf->tunnel_type);
			return -EINVAL;
		case RTE_TUNNEL_TYPE_NONE:
		default:
			return 0;
		}
		break;
	case RTE_ETH_FILTER_FDIR:
		return qede_fdir_filter_conf(eth_dev, filter_op, arg);
	case RTE_ETH_FILTER_NTUPLE:
		return qede_ntuple_filter_conf(eth_dev, filter_op, arg);
	case RTE_ETH_FILTER_MACVLAN:
	case RTE_ETH_FILTER_ETHERTYPE:
	case RTE_ETH_FILTER_FLEXIBLE:
	case RTE_ETH_FILTER_SYN:
	case RTE_ETH_FILTER_HASH:
	case RTE_ETH_FILTER_L2_TUNNEL:
	case RTE_ETH_FILTER_MAX:
	default:
		DP_ERR(edev, "Unsupported filter type %d\n",
			filter_type);
		return -EINVAL;
	}

	return 0;
}

static const struct eth_dev_ops qede_eth_dev_ops = {
	.dev_configure = qede_dev_configure,
	.dev_infos_get = qede_dev_info_get,
	.rx_queue_setup = qede_rx_queue_setup,
	.rx_queue_release = qede_rx_queue_release,
	.tx_queue_setup = qede_tx_queue_setup,
	.tx_queue_release = qede_tx_queue_release,
	.dev_start = qede_dev_start,
	.dev_set_link_up = qede_dev_set_link_up,
	.dev_set_link_down = qede_dev_set_link_down,
	.link_update = qede_link_update,
	.promiscuous_enable = qede_promiscuous_enable,
	.promiscuous_disable = qede_promiscuous_disable,
	.allmulticast_enable = qede_allmulticast_enable,
	.allmulticast_disable = qede_allmulticast_disable,
	.set_mc_addr_list = qede_set_mc_addr_list,
	.dev_stop = qede_dev_stop,
	.dev_close = qede_dev_close,
	.stats_get = qede_get_stats,
	.stats_reset = qede_reset_stats,
	.xstats_get = qede_get_xstats,
	.xstats_reset = qede_reset_xstats,
	.xstats_get_names = qede_get_xstats_names,
	.mac_addr_add = qede_mac_addr_add,
	.mac_addr_remove = qede_mac_addr_remove,
	.mac_addr_set = qede_mac_addr_set,
	.vlan_offload_set = qede_vlan_offload_set,
	.vlan_filter_set = qede_vlan_filter_set,
	.flow_ctrl_set = qede_flow_ctrl_set,
	.flow_ctrl_get = qede_flow_ctrl_get,
	.dev_supported_ptypes_get = qede_dev_supported_ptypes_get,
	.rss_hash_update = qede_rss_hash_update,
	.rss_hash_conf_get = qede_rss_hash_conf_get,
	.reta_update  = qede_rss_reta_update,
	.reta_query  = qede_rss_reta_query,
	.mtu_set = qede_set_mtu,
	.filter_ctrl = qede_dev_filter_ctrl,
	.udp_tunnel_port_add = qede_udp_dst_port_add,
	.udp_tunnel_port_del = qede_udp_dst_port_del,
};

static const struct eth_dev_ops qede_eth_vf_dev_ops = {
	.dev_configure = qede_dev_configure,
	.dev_infos_get = qede_dev_info_get,
	.rx_queue_setup = qede_rx_queue_setup,
	.rx_queue_release = qede_rx_queue_release,
	.tx_queue_setup = qede_tx_queue_setup,
	.tx_queue_release = qede_tx_queue_release,
	.dev_start = qede_dev_start,
	.dev_set_link_up = qede_dev_set_link_up,
	.dev_set_link_down = qede_dev_set_link_down,
	.link_update = qede_link_update,
	.promiscuous_enable = qede_promiscuous_enable,
	.promiscuous_disable = qede_promiscuous_disable,
	.allmulticast_enable = qede_allmulticast_enable,
	.allmulticast_disable = qede_allmulticast_disable,
	.set_mc_addr_list = qede_set_mc_addr_list,
	.dev_stop = qede_dev_stop,
	.dev_close = qede_dev_close,
	.stats_get = qede_get_stats,
	.stats_reset = qede_reset_stats,
	.xstats_get = qede_get_xstats,
	.xstats_reset = qede_reset_xstats,
	.xstats_get_names = qede_get_xstats_names,
	.vlan_offload_set = qede_vlan_offload_set,
	.vlan_filter_set = qede_vlan_filter_set,
	.dev_supported_ptypes_get = qede_dev_supported_ptypes_get,
	.rss_hash_update = qede_rss_hash_update,
	.rss_hash_conf_get = qede_rss_hash_conf_get,
	.reta_update  = qede_rss_reta_update,
	.reta_query  = qede_rss_reta_query,
	.mtu_set = qede_set_mtu,
	.udp_tunnel_port_add = qede_udp_dst_port_add,
	.udp_tunnel_port_del = qede_udp_dst_port_del,
};

static void qede_update_pf_params(struct ecore_dev *edev)
{
	struct ecore_pf_params pf_params;

	memset(&pf_params, 0, sizeof(struct ecore_pf_params));
	pf_params.eth_pf_params.num_cons = QEDE_PF_NUM_CONNS;
	pf_params.eth_pf_params.num_arfs_filters = QEDE_RFS_MAX_FLTR;
	qed_ops->common->update_pf_params(edev, &pf_params);
}

static int qede_common_dev_init(struct rte_eth_dev *eth_dev, bool is_vf)
{
	struct rte_pci_device *pci_dev;
	struct rte_pci_addr pci_addr;
	struct qede_dev *adapter;
	struct ecore_dev *edev;
	struct qed_dev_eth_info dev_info;
	struct qed_slowpath_params params;
	static bool do_once = true;
	uint8_t bulletin_change;
	uint8_t vf_mac[ETHER_ADDR_LEN];
	uint8_t is_mac_forced;
	bool is_mac_exist;
	/* Fix up ecore debug level */
	uint32_t dp_module = ~0 & ~ECORE_MSG_HW;
	uint8_t dp_level = ECORE_LEVEL_VERBOSE;
	int rc;

	/* Extract key data structures */
	adapter = eth_dev->data->dev_private;
	adapter->ethdev = eth_dev;
	edev = &adapter->edev;
	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	pci_addr = pci_dev->addr;

	PMD_INIT_FUNC_TRACE(edev);

	snprintf(edev->name, NAME_SIZE, PCI_SHORT_PRI_FMT ":dpdk-port-%u",
		 pci_addr.bus, pci_addr.devid, pci_addr.function,
		 eth_dev->data->port_id);

	eth_dev->rx_pkt_burst = qede_recv_pkts;
	eth_dev->tx_pkt_burst = qede_xmit_pkts;
	eth_dev->tx_pkt_prepare = qede_xmit_prep_pkts;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		DP_ERR(edev, "Skipping device init from secondary process\n");
		return 0;
	}

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	/* @DPDK */
	edev->vendor_id = pci_dev->id.vendor_id;
	edev->device_id = pci_dev->id.device_id;

	qed_ops = qed_get_eth_ops();
	if (!qed_ops) {
		DP_ERR(edev, "Failed to get qed_eth_ops_pass\n");
		return -EINVAL;
	}

	DP_INFO(edev, "Starting qede probe\n");
	rc = qed_ops->common->probe(edev, pci_dev, dp_module,
				    dp_level, is_vf);
	if (rc != 0) {
		DP_ERR(edev, "qede probe failed rc %d\n", rc);
		return -ENODEV;
	}
	qede_update_pf_params(edev);
	rte_intr_callback_register(&pci_dev->intr_handle,
				   qede_interrupt_handler, (void *)eth_dev);
	if (rte_intr_enable(&pci_dev->intr_handle)) {
		DP_ERR(edev, "rte_intr_enable() failed\n");
		return -ENODEV;
	}

	/* Start the Slowpath-process */
	memset(&params, 0, sizeof(struct qed_slowpath_params));
	params.int_mode = ECORE_INT_MODE_MSIX;
	params.drv_major = QEDE_PMD_VERSION_MAJOR;
	params.drv_minor = QEDE_PMD_VERSION_MINOR;
	params.drv_rev = QEDE_PMD_VERSION_REVISION;
	params.drv_eng = QEDE_PMD_VERSION_PATCH;
	strncpy((char *)params.name, QEDE_PMD_VER_PREFIX,
		QEDE_PMD_DRV_VER_STR_SIZE);

	/* For CMT mode device do periodic polling for slowpath events.
	 * This is required since uio device uses only one MSI-x
	 * interrupt vector but we need one for each engine.
	 */
	if (ECORE_IS_CMT(edev) && IS_PF(edev)) {
		rc = rte_eal_alarm_set(QEDE_SP_TIMER_PERIOD,
				       qede_poll_sp_sb_cb,
				       (void *)eth_dev);
		if (rc != 0) {
			DP_ERR(edev, "Unable to start periodic"
				     " timer rc %d\n", rc);
			return -EINVAL;
		}
	}

	rc = qed_ops->common->slowpath_start(edev, &params);
	if (rc) {
		DP_ERR(edev, "Cannot start slowpath rc = %d\n", rc);
		rte_eal_alarm_cancel(qede_poll_sp_sb_cb,
				     (void *)eth_dev);
		return -ENODEV;
	}

	rc = qed_ops->fill_dev_info(edev, &dev_info);
	if (rc) {
		DP_ERR(edev, "Cannot get device_info rc %d\n", rc);
		qed_ops->common->slowpath_stop(edev);
		qed_ops->common->remove(edev);
		rte_eal_alarm_cancel(qede_poll_sp_sb_cb,
				     (void *)eth_dev);
		return -ENODEV;
	}

	qede_alloc_etherdev(adapter, &dev_info);

	adapter->ops->common->set_name(edev, edev->name);

	if (!is_vf)
		adapter->dev_info.num_mac_filters =
			(uint32_t)RESC_NUM(ECORE_LEADING_HWFN(edev),
					    ECORE_MAC);
	else
		ecore_vf_get_num_mac_filters(ECORE_LEADING_HWFN(edev),
				(uint32_t *)&adapter->dev_info.num_mac_filters);

	/* Allocate memory for storing MAC addr */
	eth_dev->data->mac_addrs = rte_zmalloc(edev->name,
					(ETHER_ADDR_LEN *
					adapter->dev_info.num_mac_filters),
					RTE_CACHE_LINE_SIZE);

	if (eth_dev->data->mac_addrs == NULL) {
		DP_ERR(edev, "Failed to allocate MAC address\n");
		qed_ops->common->slowpath_stop(edev);
		qed_ops->common->remove(edev);
		rte_eal_alarm_cancel(qede_poll_sp_sb_cb,
				     (void *)eth_dev);
		return -ENOMEM;
	}

	if (!is_vf) {
		ether_addr_copy((struct ether_addr *)edev->hwfns[0].
				hw_info.hw_mac_addr,
				&eth_dev->data->mac_addrs[0]);
		ether_addr_copy(&eth_dev->data->mac_addrs[0],
				&adapter->primary_mac);
	} else {
		ecore_vf_read_bulletin(ECORE_LEADING_HWFN(edev),
				       &bulletin_change);
		if (bulletin_change) {
			is_mac_exist =
			    ecore_vf_bulletin_get_forced_mac(
						ECORE_LEADING_HWFN(edev),
						vf_mac,
						&is_mac_forced);
			if (is_mac_exist && is_mac_forced) {
				DP_INFO(edev, "VF macaddr received from PF\n");
				ether_addr_copy((struct ether_addr *)&vf_mac,
						&eth_dev->data->mac_addrs[0]);
				ether_addr_copy(&eth_dev->data->mac_addrs[0],
						&adapter->primary_mac);
			} else {
				DP_ERR(edev, "No VF macaddr assigned\n");
			}
		}
	}

	eth_dev->dev_ops = (is_vf) ? &qede_eth_vf_dev_ops : &qede_eth_dev_ops;

	if (do_once) {
		qede_print_adapter_info(adapter);
		do_once = false;
	}

	/* Bring-up the link */
	qede_dev_set_link_state(eth_dev, true);

	adapter->num_tx_queues = 0;
	adapter->num_rx_queues = 0;
	SLIST_INIT(&adapter->fdir_info.fdir_list_head);
	SLIST_INIT(&adapter->vlan_list_head);
	SLIST_INIT(&adapter->uc_list_head);
	SLIST_INIT(&adapter->mc_list_head);
	adapter->mtu = ETHER_MTU;
	adapter->vport_started = false;

	/* VF tunnel offloads is enabled by default in PF driver */
	adapter->vxlan.num_filters = 0;
	adapter->geneve.num_filters = 0;
	adapter->ipgre.num_filters = 0;
	if (is_vf) {
		adapter->vxlan.enable = true;
		adapter->vxlan.filter_type = ETH_TUNNEL_FILTER_IMAC |
					     ETH_TUNNEL_FILTER_IVLAN;
		adapter->vxlan.udp_port = QEDE_VXLAN_DEF_PORT;
		adapter->geneve.enable = true;
		adapter->geneve.filter_type = ETH_TUNNEL_FILTER_IMAC |
					      ETH_TUNNEL_FILTER_IVLAN;
		adapter->geneve.udp_port = QEDE_GENEVE_DEF_PORT;
		adapter->ipgre.enable = true;
		adapter->ipgre.filter_type = ETH_TUNNEL_FILTER_IMAC |
					     ETH_TUNNEL_FILTER_IVLAN;
	} else {
		adapter->vxlan.enable = false;
		adapter->geneve.enable = false;
		adapter->ipgre.enable = false;
	}

	DP_INFO(edev, "MAC address : %02x:%02x:%02x:%02x:%02x:%02x\n",
		adapter->primary_mac.addr_bytes[0],
		adapter->primary_mac.addr_bytes[1],
		adapter->primary_mac.addr_bytes[2],
		adapter->primary_mac.addr_bytes[3],
		adapter->primary_mac.addr_bytes[4],
		adapter->primary_mac.addr_bytes[5]);

	DP_INFO(edev, "Device initialized\n");

	return 0;
}

static int qedevf_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	return qede_common_dev_init(eth_dev, 1);
}

static int qede_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	return qede_common_dev_init(eth_dev, 0);
}

static int qede_dev_common_uninit(struct rte_eth_dev *eth_dev)
{
#ifdef RTE_LIBRTE_QEDE_DEBUG_INIT
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;

	PMD_INIT_FUNC_TRACE(edev);
#endif

	/* only uninitialize in the primary process */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* safe to close dev here */
	qede_dev_close(eth_dev);

	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;

	if (eth_dev->data->mac_addrs)
		rte_free(eth_dev->data->mac_addrs);

	eth_dev->data->mac_addrs = NULL;

	return 0;
}

static int qede_eth_dev_uninit(struct rte_eth_dev *eth_dev)
{
	return qede_dev_common_uninit(eth_dev);
}

static int qedevf_eth_dev_uninit(struct rte_eth_dev *eth_dev)
{
	return qede_dev_common_uninit(eth_dev);
}

static const struct rte_pci_id pci_id_qedevf_map[] = {
#define QEDEVF_RTE_PCI_DEVICE(dev) RTE_PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, dev)
	{
		QEDEVF_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_NX2_VF)
	},
	{
		QEDEVF_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_57980S_IOV)
	},
	{
		QEDEVF_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_AH_IOV)
	},
	{.vendor_id = 0,}
};

static const struct rte_pci_id pci_id_qede_map[] = {
#define QEDE_RTE_PCI_DEVICE(dev) RTE_PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, dev)
	{
		QEDE_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_NX2_57980E)
	},
	{
		QEDE_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_NX2_57980S)
	},
	{
		QEDE_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_57980S_40)
	},
	{
		QEDE_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_57980S_25)
	},
	{
		QEDE_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_57980S_100)
	},
	{
		QEDE_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_57980S_50)
	},
	{
		QEDE_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_AH_50G)
	},
	{
		QEDE_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_AH_10G)
	},
	{
		QEDE_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_AH_40G)
	},
	{
		QEDE_RTE_PCI_DEVICE(PCI_DEVICE_ID_QLOGIC_AH_25G)
	},
	{.vendor_id = 0,}
};

static int qedevf_eth_dev_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct qede_dev), qedevf_eth_dev_init);
}

static int qedevf_eth_dev_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, qedevf_eth_dev_uninit);
}

static struct rte_pci_driver rte_qedevf_pmd = {
	.id_table = pci_id_qedevf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = qedevf_eth_dev_pci_probe,
	.remove = qedevf_eth_dev_pci_remove,
};

static int qede_eth_dev_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct qede_dev), qede_eth_dev_init);
}

static int qede_eth_dev_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, qede_eth_dev_uninit);
}

static struct rte_pci_driver rte_qede_pmd = {
	.id_table = pci_id_qede_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = qede_eth_dev_pci_probe,
	.remove = qede_eth_dev_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_qede, rte_qede_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_qede, pci_id_qede_map);
RTE_PMD_REGISTER_KMOD_DEP(net_qede, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_REGISTER_PCI(net_qede_vf, rte_qedevf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_qede_vf, pci_id_qedevf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_qede_vf, "* igb_uio | vfio-pci");

RTE_INIT(qede_init_log);
static void
qede_init_log(void)
{
	qede_logtype_init = rte_log_register("pmd.net.qede.init");
	if (qede_logtype_init >= 0)
		rte_log_set_level(qede_logtype_init, RTE_LOG_NOTICE);
	qede_logtype_driver = rte_log_register("pmd.net.qede.driver");
	if (qede_logtype_driver >= 0)
		rte_log_set_level(qede_logtype_driver, RTE_LOG_NOTICE);
}
