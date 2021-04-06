/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _ROC_NIX_XSTAT_H_
#define _ROC_NIX_XSTAT_H_

#include <inttypes.h>

struct cnxk_nix_xstats_name {
	char name[ROC_NIX_XSTATS_NAME_SIZE];
	uint32_t offset;
};

static const struct cnxk_nix_xstats_name nix_tx_xstats[] = {
	{"tx_ucast", NIX_STAT_LF_TX_TX_UCAST},
	{"tx_bcast", NIX_STAT_LF_TX_TX_BCAST},
	{"tx_mcast", NIX_STAT_LF_TX_TX_MCAST},
	{"tx_drop", NIX_STAT_LF_TX_TX_DROP},
	{"tx_octs", NIX_STAT_LF_TX_TX_OCTS},
};

static const struct cnxk_nix_xstats_name nix_rx_xstats[] = {
	{"rx_octs", NIX_STAT_LF_RX_RX_OCTS},
	{"rx_ucast", NIX_STAT_LF_RX_RX_UCAST},
	{"rx_bcast", NIX_STAT_LF_RX_RX_BCAST},
	{"rx_mcast", NIX_STAT_LF_RX_RX_MCAST},
	{"rx_drop", NIX_STAT_LF_RX_RX_DROP},
	{"rx_drop_octs", NIX_STAT_LF_RX_RX_DROP_OCTS},
	{"rx_fcs", NIX_STAT_LF_RX_RX_FCS},
	{"rx_err", NIX_STAT_LF_RX_RX_ERR},
	{"rx_drp_bcast", NIX_STAT_LF_RX_RX_DRP_BCAST},
	{"rx_drp_mcast", NIX_STAT_LF_RX_RX_DRP_MCAST},
	{"rx_drp_l3bcast", NIX_STAT_LF_RX_RX_DRP_L3BCAST},
	{"rx_drp_l3mcast", NIX_STAT_LF_RX_RX_DRP_L3MCAST},
};

static const struct cnxk_nix_xstats_name nix_q_xstats[] = {
	{"rq_op_re_pkts", NIX_LF_RQ_OP_RE_PKTS},
};

static const struct cnxk_nix_xstats_name nix_rx_xstats_rpm[] = {
	{"rpm_rx_etherStatsOctets", RPM_MTI_STAT_RX_OCT_CNT},
	{"rpm_rx_OctetsReceivedOK", RPM_MTI_STAT_RX_OCT_RECV_OK},
	{"rpm_rx_aAlignmentErrors", RPM_MTI_STAT_RX_ALIG_ERR},
	{"rpm_rx_aPAUSEMACCtrlFramesReceived", RPM_MTI_STAT_RX_CTRL_FRM_RECV},
	{"rpm_rx_aFrameTooLongErrors", RPM_MTI_STAT_RX_FRM_LONG},
	{"rpm_rx_aInRangeLengthErrors", RPM_MTI_STAT_RX_LEN_ERR},
	{"rpm_rx_aFramesReceivedOK", RPM_MTI_STAT_RX_FRM_RECV},
	{"rpm_rx_aFrameCheckSequenceErrors", RPM_MTI_STAT_RX_FRM_SEQ_ERR},
	{"rpm_rx_VLANReceivedOK", RPM_MTI_STAT_RX_VLAN_OK},
	{"rpm_rx_ifInErrors", RPM_MTI_STAT_RX_IN_ERR},
	{"rpm_rx_ifInUcastPkts", RPM_MTI_STAT_RX_IN_UCAST_PKT},
	{"rpm_rx_ifInMulticastPkts", RPM_MTI_STAT_RX_IN_MCAST_PKT},
	{"rpm_rx_ifInBroadcastPkts", RPM_MTI_STAT_RX_IN_BCAST_PKT},
	{"rpm_rx_etherStatsDropEvents", RPM_MTI_STAT_RX_DRP_EVENTS},
	{"rpm_rx_etherStatsPkts", RPM_MTI_STAT_RX_PKT},
	{"rpm_rx_etherStatsUndersizePkts", RPM_MTI_STAT_RX_UNDER_SIZE},
	{"rpm_rx_etherStatsPkts64Octets", RPM_MTI_STAT_RX_1_64_PKT_CNT},
	{"rpm_rx_etherStatsPkts65to127Octets", RPM_MTI_STAT_RX_65_127_PKT_CNT},
	{"rpm_rx_etherStatsPkts128to255Octets",
	 RPM_MTI_STAT_RX_128_255_PKT_CNT},
	{"rpm_rx_etherStatsPkts256to511Octets",
	 RPM_MTI_STAT_RX_256_511_PKT_CNT},
	{"rpm_rx_etherStatsPkts512to1023Octets",
	 RPM_MTI_STAT_RX_512_1023_PKT_CNT},
	{"rpm_rx_etherStatsPkts1024to1518Octets",
	 RPM_MTI_STAT_RX_1024_1518_PKT_CNT},
	{"rpm_rx_etherStatsPkts1519toMaxOctets",
	 RPM_MTI_STAT_RX_1519_MAX_PKT_CNT},
	{"rpm_rx_etherStatsOversizePkts", RPM_MTI_STAT_RX_OVER_SIZE},
	{"rpm_rx_etherStatsJabbers", RPM_MTI_STAT_RX_JABBER},
	{"rpm_rx_etherStatsFragments", RPM_MTI_STAT_RX_ETH_FRAGS},
	{"rpm_rx_CBFC_pause_frames_class_0", RPM_MTI_STAT_RX_CBFC_CLASS_0},
	{"rpm_rx_CBFC_pause_frames_class_1", RPM_MTI_STAT_RX_CBFC_CLASS_1},
	{"rpm_rx_CBFC_pause_frames_class_2", RPM_MTI_STAT_RX_CBFC_CLASS_2},
	{"rpm_rx_CBFC_pause_frames_class_3", RPM_MTI_STAT_RX_CBFC_CLASS_3},
	{"rpm_rx_CBFC_pause_frames_class_4", RPM_MTI_STAT_RX_CBFC_CLASS_4},
	{"rpm_rx_CBFC_pause_frames_class_5", RPM_MTI_STAT_RX_CBFC_CLASS_5},
	{"rpm_rx_CBFC_pause_frames_class_6", RPM_MTI_STAT_RX_CBFC_CLASS_6},
	{"rpm_rx_CBFC_pause_frames_class_7", RPM_MTI_STAT_RX_CBFC_CLASS_7},
	{"rpm_rx_CBFC_pause_frames_class_8", RPM_MTI_STAT_RX_CBFC_CLASS_8},
	{"rpm_rx_CBFC_pause_frames_class_9", RPM_MTI_STAT_RX_CBFC_CLASS_9},
	{"rpm_rx_CBFC_pause_frames_class_10", RPM_MTI_STAT_RX_CBFC_CLASS_10},
	{"rpm_rx_CBFC_pause_frames_class_11", RPM_MTI_STAT_RX_CBFC_CLASS_11},
	{"rpm_rx_CBFC_pause_frames_class_12", RPM_MTI_STAT_RX_CBFC_CLASS_12},
	{"rpm_rx_CBFC_pause_frames_class_13", RPM_MTI_STAT_RX_CBFC_CLASS_13},
	{"rpm_rx_CBFC_pause_frames_class_14", RPM_MTI_STAT_RX_CBFC_CLASS_14},
	{"rpm_rx_CBFC_pause_frames_class_15", RPM_MTI_STAT_RX_CBFC_CLASS_15},
	{"rpm_rx_aMACControlFramesReceived", RPM_MTI_STAT_RX_MAC_CONTROL},
};

static const struct cnxk_nix_xstats_name nix_tx_xstats_rpm[] = {
	{"rpm_tx_etherStatsOctets", RPM_MTI_STAT_TX_OCT_CNT},
	{"rpm_tx_OctetsTransmittedOK", RPM_MTI_STAT_TX_OCT_TX_OK},
	{"rpm_tx_aPAUSEMACCtrlFramesTransmitted",
	 RPM_MTI_STAT_TX_PAUSE_MAC_CTRL},
	{"rpm_tx_aFramesTransmittedOK", RPM_MTI_STAT_TX_FRAMES_OK},
	{"rpm_tx_VLANTransmittedOK", RPM_MTI_STAT_TX_VLAN_OK},
	{"rpm_tx_ifOutErrors", RPM_MTI_STAT_TX_OUT_ERR},
	{"rpm_tx_ifOutUcastPkts", RPM_MTI_STAT_TX_UCAST_PKT_CNT},
	{"rpm_tx_ifOutMulticastPkts", RPM_MTI_STAT_TX_MCAST_PKT_CNT},
	{"rpm_tx_ifOutBroadcastPkts", RPM_MTI_STAT_TX_BCAST_PKT_CNT},
	{"rpm_tx_etherStatsPkts64Octets", RPM_MTI_STAT_TX_1_64_PKT_CNT},
	{"rpm_tx_etherStatsPkts65to127Octets", RPM_MTI_STAT_TX_65_127_PKT_CNT},
	{"rpm_tx_etherStatsPkts128to255Octets",
	 RPM_MTI_STAT_TX_128_255_PKT_CNT},
	{"rpm_tx_etherStatsPkts256to511Octets",
	 RPM_MTI_STAT_TX_256_511_PKT_CNT},
	{"rpm_tx_etherStatsPkts512to1023Octets",
	 RPM_MTI_STAT_TX_512_1023_PKT_CNT},
	{"rpm_tx_etherStatsPkts1024to1518Octets",
	 RPM_MTI_STAT_TX_1024_1518_PKT_CNT},
	{"rpm_tx_etherStatsPkts1519toMaxOctets",
	 RPM_MTI_STAT_TX_1519_MAX_PKT_CNT},
	{"rpm_tx_CBFC_pause_frames_class_0", RPM_MTI_STAT_TX_CBFC_CLASS_0},
	{"rpm_tx_CBFC_pause_frames_class_1", RPM_MTI_STAT_TX_CBFC_CLASS_1},
	{"rpm_tx_CBFC_pause_frames_class_2", RPM_MTI_STAT_TX_CBFC_CLASS_2},
	{"rpm_tx_CBFC_pause_frames_class_3", RPM_MTI_STAT_TX_CBFC_CLASS_3},
	{"rpm_tx_CBFC_pause_frames_class_4", RPM_MTI_STAT_TX_CBFC_CLASS_4},
	{"rpm_tx_CBFC_pause_frames_class_5", RPM_MTI_STAT_TX_CBFC_CLASS_5},
	{"rpm_tx_CBFC_pause_frames_class_6", RPM_MTI_STAT_TX_CBFC_CLASS_6},
	{"rpm_tx_CBFC_pause_frames_class_7", RPM_MTI_STAT_TX_CBFC_CLASS_7},
	{"rpm_tx_CBFC_pause_frames_class_8", RPM_MTI_STAT_TX_CBFC_CLASS_8},
	{"rpm_tx_CBFC_pause_frames_class_9", RPM_MTI_STAT_TX_CBFC_CLASS_9},
	{"rpm_tx_CBFC_pause_frames_class_10", RPM_MTI_STAT_TX_CBFC_CLASS_10},
	{"rpm_tx_CBFC_pause_frames_class_11", RPM_MTI_STAT_TX_CBFC_CLASS_11},
	{"rpm_tx_CBFC_pause_frames_class_12", RPM_MTI_STAT_TX_CBFC_CLASS_12},
	{"rpm_tx_CBFC_pause_frames_class_13", RPM_MTI_STAT_TX_CBFC_CLASS_13},
	{"rpm_tx_CBFC_pause_frames_class_14", RPM_MTI_STAT_TX_CBFC_CLASS_14},
	{"rpm_tx_CBFC_pause_frames_class_15", RPM_MTI_STAT_TX_CBFC_CLASS_15},
	{"rpm_tx_aMACControlFramesTransmitted",
	 RPM_MTI_STAT_TX_MAC_CONTROL_FRAMES},
	{"rpm_tx_etherStatsPkts", RPM_MTI_STAT_TX_PKT_CNT},
};

static const struct cnxk_nix_xstats_name nix_rx_xstats_cgx[] = {
	{"cgx_rx_pkts", CGX_RX_PKT_CNT},
	{"cgx_rx_octs", CGX_RX_OCT_CNT},
	{"cgx_rx_pause_pkts", CGX_RX_PAUSE_PKT_CNT},
	{"cgx_rx_pause_octs", CGX_RX_PAUSE_OCT_CNT},
	{"cgx_rx_dmac_filt_pkts", CGX_RX_DMAC_FILT_PKT_CNT},
	{"cgx_rx_dmac_filt_octs", CGX_RX_DMAC_FILT_OCT_CNT},
	{"cgx_rx_fifo_drop_pkts", CGX_RX_FIFO_DROP_PKT_CNT},
	{"cgx_rx_fifo_drop_octs", CGX_RX_FIFO_DROP_OCT_CNT},
	{"cgx_rx_errors", CGX_RX_ERR_CNT},
};

static const struct cnxk_nix_xstats_name nix_tx_xstats_cgx[] = {
	{"cgx_tx_collision_drop", CGX_TX_COLLISION_DROP},
	{"cgx_tx_frame_deferred_cnt", CGX_TX_FRAME_DEFER_CNT},
	{"cgx_tx_multiple_collision", CGX_TX_MULTIPLE_COLLISION},
	{"cgx_tx_single_collision", CGX_TX_SINGLE_COLLISION},
	{"cgx_tx_octs", CGX_TX_OCT_CNT},
	{"cgx_tx_pkts", CGX_TX_PKT_CNT},
	{"cgx_tx_1_to_63_oct_frames", CGX_TX_1_63_PKT_CNT},
	{"cgx_tx_64_oct_frames", CGX_TX_64_PKT_CNT},
	{"cgx_tx_65_to_127_oct_frames", CGX_TX_65_127_PKT_CNT},
	{"cgx_tx_128_to_255_oct_frames", CGX_TX_128_255_PKT_CNT},
	{"cgx_tx_256_to_511_oct_frames", CGX_TX_256_511_PKT_CNT},
	{"cgx_tx_512_to_1023_oct_frames", CGX_TX_512_1023_PKT_CNT},
	{"cgx_tx_1024_to_1518_oct_frames", CGX_TX_1024_1518_PKT_CNT},
	{"cgx_tx_1519_to_max_oct_frames", CGX_TX_1519_MAX_PKT_CNT},
	{"cgx_tx_broadcast_packets", CGX_TX_BCAST_PKTS},
	{"cgx_tx_multicast_packets", CGX_TX_MCAST_PKTS},
	{"cgx_tx_underflow_packets", CGX_TX_UFLOW_PKTS},
	{"cgx_tx_pause_packets", CGX_TX_PAUSE_PKTS},
};

#define CNXK_NIX_NUM_RX_XSTATS	   PLT_DIM(nix_rx_xstats)
#define CNXK_NIX_NUM_TX_XSTATS	   PLT_DIM(nix_tx_xstats)
#define CNXK_NIX_NUM_QUEUE_XSTATS  PLT_DIM(nix_q_xstats)
#define CNXK_NIX_NUM_RX_XSTATS_CGX PLT_DIM(nix_rx_xstats_cgx)
#define CNXK_NIX_NUM_TX_XSTATS_CGX PLT_DIM(nix_tx_xstats_cgx)
#define CNXK_NIX_NUM_RX_XSTATS_RPM PLT_DIM(nix_rx_xstats_rpm)
#define CNXK_NIX_NUM_TX_XSTATS_RPM PLT_DIM(nix_tx_xstats_rpm)

#define CNXK_NIX_NUM_XSTATS_REG                                                \
	(CNXK_NIX_NUM_RX_XSTATS + CNXK_NIX_NUM_TX_XSTATS +                     \
	 CNXK_NIX_NUM_QUEUE_XSTATS)
#define CNXK_NIX_NUM_XSTATS_CGX                                                \
	(CNXK_NIX_NUM_XSTATS_REG + CNXK_NIX_NUM_RX_XSTATS_CGX +                \
	 CNXK_NIX_NUM_TX_XSTATS_CGX)
#define CNXK_NIX_NUM_XSTATS_RPM                                                \
	(CNXK_NIX_NUM_XSTATS_REG + CNXK_NIX_NUM_RX_XSTATS_RPM +                \
	 CNXK_NIX_NUM_TX_XSTATS_RPM)

static inline uint64_t
roc_nix_num_rx_xstats(void)
{
	if (roc_model_is_cn9k())
		return CNXK_NIX_NUM_RX_XSTATS_CGX;

	return CNXK_NIX_NUM_RX_XSTATS_RPM;
}

static inline uint64_t
roc_nix_num_tx_xstats(void)
{
	if (roc_model_is_cn9k())
		return CNXK_NIX_NUM_TX_XSTATS_CGX;

	return CNXK_NIX_NUM_TX_XSTATS_RPM;
}
#endif /* _ROC_NIX_XSTAT_H_ */
