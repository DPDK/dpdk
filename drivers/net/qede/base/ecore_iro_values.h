/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef __IRO_VALUES_H__
#define __IRO_VALUES_H__

static const struct iro iro_arr[60] = {
/* YSTORM_FLOW_CONTROL_MODE_OFFSET */
	{      0x0,      0x0,      0x0,      0x0,      0x8},
/* TSTORM_PORT_STAT_OFFSET(port_id) */
	{   0x4cb8,     0x88,      0x0,      0x0,     0x88},
/* TSTORM_LL2_PORT_STAT_OFFSET(port_id) */
	{   0x6530,     0x20,      0x0,      0x0,     0x20},
/* USTORM_VF_PF_CHANNEL_READY_OFFSET(vf_id) */
	{    0xb00,      0x8,      0x0,      0x0,      0x4},
/* USTORM_FLR_FINAL_ACK_OFFSET(pf_id) */
	{    0xa80,      0x8,      0x0,      0x0,      0x4},
/* USTORM_EQE_CONS_OFFSET(pf_id) */
	{      0x0,      0x8,      0x0,      0x0,      0x2},
/* USTORM_ETH_QUEUE_ZONE_OFFSET(queue_zone_id) */
	{     0x80,      0x8,      0x0,      0x0,      0x4},
/* USTORM_COMMON_QUEUE_CONS_OFFSET(queue_zone_id) */
	{     0x84,      0x8,      0x0,      0x0,      0x2},
/* XSTORM_INTEG_TEST_DATA_OFFSET */
	{   0x4c48,      0x0,      0x0,      0x0,     0x78},
/* YSTORM_INTEG_TEST_DATA_OFFSET */
	{   0x3e38,      0x0,      0x0,      0x0,     0x78},
/* PSTORM_INTEG_TEST_DATA_OFFSET */
	{   0x3ef8,      0x0,      0x0,      0x0,     0x78},
/* TSTORM_INTEG_TEST_DATA_OFFSET */
	{   0x4c40,      0x0,      0x0,      0x0,     0x78},
/* MSTORM_INTEG_TEST_DATA_OFFSET */
	{   0x4998,      0x0,      0x0,      0x0,     0x78},
/* USTORM_INTEG_TEST_DATA_OFFSET */
	{   0x7f50,      0x0,      0x0,      0x0,     0x78},
/* TSTORM_LL2_RX_PRODS_OFFSET(core_rx_queue_id) */
	{    0xa28,      0x8,      0x0,      0x0,      0x8},
/* CORE_LL2_TSTORM_PER_QUEUE_STAT_OFFSET(core_rx_queue_id) */
	{   0x6210,     0x10,      0x0,      0x0,     0x10},
/* CORE_LL2_USTORM_PER_QUEUE_STAT_OFFSET(core_rx_queue_id) */
	{   0xb820,     0x30,      0x0,      0x0,     0x30},
/* CORE_LL2_PSTORM_PER_QUEUE_STAT_OFFSET(core_tx_stats_id) */
	{   0xa990,     0x30,      0x0,      0x0,     0x30},
/* MSTORM_QUEUE_STAT_OFFSET(stat_counter_id) */
	{   0x4b68,     0x80,      0x0,      0x0,     0x40},
/* MSTORM_ETH_PF_PRODS_OFFSET(queue_id) */
	{    0x1f8,      0x4,      0x0,      0x0,      0x4},
/* MSTORM_ETH_VF_PRODS_OFFSET(vf_id,vf_queue_id) */
	{   0x53a8,     0x80,      0x4,      0x0,      0x4},
/* MSTORM_TPA_TIMEOUT_US_OFFSET */
	{   0xc7d0,      0x0,      0x0,      0x0,      0x4},
/* MSTORM_ETH_PF_STAT_OFFSET(pf_id) */
	{   0x4ba8,     0x80,      0x0,      0x0,     0x20},
/* USTORM_QUEUE_STAT_OFFSET(stat_counter_id) */
	{   0x8158,     0x40,      0x0,      0x0,     0x30},
/* USTORM_ETH_PF_STAT_OFFSET(pf_id) */
	{   0xe770,     0x60,      0x0,      0x0,     0x60},
/* PSTORM_QUEUE_STAT_OFFSET(stat_counter_id) */
	{   0x4090,     0x80,      0x0,      0x0,     0x38},
/* PSTORM_ETH_PF_STAT_OFFSET(pf_id) */
	{   0xfea8,     0x78,      0x0,      0x0,     0x78},
/* PSTORM_CTL_FRAME_ETHTYPE_OFFSET(ethType_id) */
	{    0x1f8,      0x4,      0x0,      0x0,      0x4},
/* TSTORM_ETH_PRS_INPUT_OFFSET */
	{   0xaf20,      0x0,      0x0,      0x0,     0xf0},
/* ETH_RX_RATE_LIMIT_OFFSET(pf_id) */
	{   0xb010,      0x8,      0x0,      0x0,      0x8},
/* TSTORM_ETH_RSS_UPDATE_OFFSET(pf_id) */
	{    0xc00,      0x8,      0x0,      0x0,      0x8},
/* XSTORM_ETH_QUEUE_ZONE_OFFSET(queue_id) */
	{    0x1f8,      0x8,      0x0,      0x0,      0x8},
/* YSTORM_TOE_CQ_PROD_OFFSET(rss_id) */
	{    0xac0,      0x8,      0x0,      0x0,      0x8},
/* USTORM_TOE_CQ_PROD_OFFSET(rss_id) */
	{   0x2578,      0x8,      0x0,      0x0,      0x8},
/* USTORM_TOE_GRQ_PROD_OFFSET(pf_id) */
	{   0x24f8,      0x8,      0x0,      0x0,      0x8},
/* TSTORM_SCSI_CMDQ_CONS_OFFSET(cmdq_queue_id) */
	{      0x0,      0x8,      0x0,      0x0,      0x8},
/* TSTORM_SCSI_BDQ_EXT_PROD_OFFSET(func_id,bdq_id) */
	{    0x400,     0x18,      0x8,      0x0,      0x8},
/* MSTORM_SCSI_BDQ_EXT_PROD_OFFSET(func_id,bdq_id) */
	{    0xb78,     0x18,      0x8,      0x0,      0x2},
/* TSTORM_ISCSI_RX_STATS_OFFSET(pf_id) */
	{   0xd898,     0x50,      0x0,      0x0,     0x3c},
/* MSTORM_ISCSI_RX_STATS_OFFSET(pf_id) */
	{  0x12908,     0x18,      0x0,      0x0,     0x10},
/* USTORM_ISCSI_RX_STATS_OFFSET(pf_id) */
	{  0x11aa8,     0x40,      0x0,      0x0,     0x18},
/* XSTORM_ISCSI_TX_STATS_OFFSET(pf_id) */
	{   0xa588,     0x50,      0x0,      0x0,     0x20},
/* YSTORM_ISCSI_TX_STATS_OFFSET(pf_id) */
	{   0x8f00,     0x40,      0x0,      0x0,     0x28},
/* PSTORM_ISCSI_TX_STATS_OFFSET(pf_id) */
	{  0x10e30,     0x18,      0x0,      0x0,     0x10},
/* TSTORM_FCOE_RX_STATS_OFFSET(pf_id) */
	{   0xde48,     0x48,      0x0,      0x0,     0x38},
/* PSTORM_FCOE_TX_STATS_OFFSET(pf_id) */
	{  0x11298,     0x20,      0x0,      0x0,     0x20},
/* PSTORM_RDMA_QUEUE_STAT_OFFSET(rdma_stat_counter_id) */
	{   0x40c8,     0x80,      0x0,      0x0,     0x10},
/* TSTORM_RDMA_QUEUE_STAT_OFFSET(rdma_stat_counter_id) */
	{   0x5048,     0x10,      0x0,      0x0,     0x10},
/* XSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id) */
	{   0xa928,      0x8,      0x0,      0x0,      0x1},
/* YSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id) */
	{   0xa128,      0x8,      0x0,      0x0,      0x1},
/* PSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id) */
	{  0x11a30,      0x8,      0x0,      0x0,      0x1},
/* TSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id) */
	{   0xf030,      0x8,      0x0,      0x0,      0x1},
/* MSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id) */
	{  0x13028,      0x8,      0x0,      0x0,      0x1},
/* USTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id) */
	{  0x12c58,      0x8,      0x0,      0x0,      0x1},
/* XSTORM_IWARP_RXMIT_STATS_OFFSET(pf_id) */
	{   0xc9b8,     0x30,      0x0,      0x0,     0x10},
/* TSTORM_ROCE_EVENTS_STAT_OFFSET(roce_pf_id) */
	{   0xed90,     0x28,      0x0,      0x0,     0x28},
/* YSTORM_ROCE_DCQCN_RECEIVED_STATS_OFFSET(roce_pf_id) */
	{   0xad20,     0x18,      0x0,      0x0,     0x18},
/* YSTORM_ROCE_ERROR_STATS_OFFSET(roce_pf_id) */
	{   0xaea0,      0x8,      0x0,      0x0,      0x8},
/* PSTORM_ROCE_DCQCN_SENT_STATS_OFFSET(roce_pf_id) */
	{  0x13c38,      0x8,      0x0,      0x0,      0x8},
/* USTORM_ROCE_CQE_STATS_OFFSET(roce_pf_id) */
	{  0x13c50,     0x18,      0x0,      0x0,     0x18},
};

#endif /* __IRO_VALUES_H__ */
