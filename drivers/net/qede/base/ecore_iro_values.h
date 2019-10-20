/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2016 - 2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef __IRO_VALUES_H__
#define __IRO_VALUES_H__

/* Per-chip offsets in iro_arr in dwords */
#define E4_IRO_ARR_OFFSET 0

/* IRO Array */
static const u32 iro_arr[] = {
	/* E4 */
	/* YSTORM_FLOW_CONTROL_MODE_OFFSET */
	/* offset=0x0, size=0x8 */
	0x00000000, 0x00000000, 0x00080000,
	/* TSTORM_PORT_STAT_OFFSET(port_id), */
	/* offset=0x3288, mult1=0x88, size=0x88 */
	0x00003288, 0x00000088, 0x00880000,
	/* TSTORM_LL2_PORT_STAT_OFFSET(port_id), */
	/* offset=0x58f0, mult1=0x20, size=0x20 */
	0x000058f0, 0x00000020, 0x00200000,
	/* USTORM_VF_PF_CHANNEL_READY_OFFSET(vf_id), */
	/* offset=0xb00, mult1=0x8, size=0x4 */
	0x00000b00, 0x00000008, 0x00040000,
	/* USTORM_FLR_FINAL_ACK_OFFSET(pf_id), */
	/* offset=0xa80, mult1=0x8, size=0x4 */
	0x00000a80, 0x00000008, 0x00040000,
	/* USTORM_EQE_CONS_OFFSET(pf_id), */
	/* offset=0x0, mult1=0x8, size=0x2 */
	0x00000000, 0x00000008, 0x00020000,
	/* USTORM_ETH_QUEUE_ZONE_OFFSET(queue_zone_id), */
	/* offset=0x80, mult1=0x8, size=0x4 */
	0x00000080, 0x00000008, 0x00040000,
	/* USTORM_COMMON_QUEUE_CONS_OFFSET(queue_zone_id), */
	/* offset=0x84, mult1=0x8, size=0x2 */
	0x00000084, 0x00000008, 0x00020000,
	/* XSTORM_PQ_INFO_OFFSET(pq_id), */
	/* offset=0x5718, mult1=0x4, size=0x4 */
	0x00005718, 0x00000004, 0x00040000,
	/* XSTORM_INTEG_TEST_DATA_OFFSET, */
	/* offset=0x4dd0, size=0x78 */
	0x00004dd0, 0x00000000, 0x00780000,
	/* YSTORM_INTEG_TEST_DATA_OFFSET */
	/* offset=0x3e40, size=0x78 */
	0x00003e40, 0x00000000, 0x00780000,
	/* PSTORM_INTEG_TEST_DATA_OFFSET, */
	/* offset=0x4480, size=0x78 */
	0x00004480, 0x00000000, 0x00780000,
	/* TSTORM_INTEG_TEST_DATA_OFFSET, */
	/* offset=0x3210, size=0x78 */
	0x00003210, 0x00000000, 0x00780000,
	/* MSTORM_INTEG_TEST_DATA_OFFSET */
	/* offset=0x3b50, size=0x78 */
	0x00003b50, 0x00000000, 0x00780000,
	/* USTORM_INTEG_TEST_DATA_OFFSET */
	/* offset=0x7f58, size=0x78 */
	0x00007f58, 0x00000000, 0x00780000,
	/* XSTORM_OVERLAY_BUF_ADDR_OFFSET, */
	/* offset=0x5f58, size=0x8 */
	0x00005f58, 0x00000000, 0x00080000,
	/* YSTORM_OVERLAY_BUF_ADDR_OFFSET */
	/* offset=0x7100, size=0x8 */
	0x00007100, 0x00000000, 0x00080000,
	/* PSTORM_OVERLAY_BUF_ADDR_OFFSET, */
	/* offset=0xaea0, size=0x8 */
	0x0000aea0, 0x00000000, 0x00080000,
	/* TSTORM_OVERLAY_BUF_ADDR_OFFSET, */
	/* offset=0x4398, size=0x8 */
	0x00004398, 0x00000000, 0x00080000,
	/* MSTORM_OVERLAY_BUF_ADDR_OFFSET */
	/* offset=0xa5a0, size=0x8 */
	0x0000a5a0, 0x00000000, 0x00080000,
	/* USTORM_OVERLAY_BUF_ADDR_OFFSET */
	/* offset=0xbde8, size=0x8 */
	0x0000bde8, 0x00000000, 0x00080000,
	/* TSTORM_LL2_RX_PRODS_OFFSET(core_rx_queue_id), */
	/* offset=0x20, mult1=0x4, size=0x4 */
	0x00000020, 0x00000004, 0x00040000,
	/* CORE_LL2_TSTORM_PER_QUEUE_STAT_OFFSET(core_rx_queue_id), */
	/* offset=0x56d0, mult1=0x10, size=0x10 */
	0x000056d0, 0x00000010, 0x00100000,
	/* CORE_LL2_USTORM_PER_QUEUE_STAT_OFFSET(core_rx_queue_id), */
	/* offset=0xc210, mult1=0x30, size=0x30 */
	0x0000c210, 0x00000030, 0x00300000,
	/* CORE_LL2_PSTORM_PER_QUEUE_STAT_OFFSET(core_tx_stats_id), */
	/* offset=0xb088, mult1=0x38, size=0x38 */
	0x0000b088, 0x00000038, 0x00380000,
	/* MSTORM_QUEUE_STAT_OFFSET(stat_counter_id), */
	/* offset=0x3d20, mult1=0x80, size=0x40 */
	0x00003d20, 0x00000080, 0x00400000,
	/* MSTORM_TPA_TIMEOUT_US_OFFSET */
	/* offset=0xbf60, size=0x4 */
	0x0000bf60, 0x00000000, 0x00040000,
	/* MSTORM_ETH_VF_PRODS_OFFSET(vf_id,vf_queue_id), */
	/* offset=0x4560, mult1=0x80, mult2=0x4, size=0x4 */
	0x00004560, 0x00040080, 0x00040000,
	/* MSTORM_ETH_PF_PRODS_OFFSET(queue_id), */
	/* offset=0x1f8, mult1=0x4, size=0x4 */
	0x000001f8, 0x00000004, 0x00040000,
	/* MSTORM_ETH_PF_STAT_OFFSET(pf_id), */
	/* offset=0x3d60, mult1=0x80, size=0x20 */
	0x00003d60, 0x00000080, 0x00200000,
	/* USTORM_QUEUE_STAT_OFFSET(stat_counter_id), */
	/* offset=0x8960, mult1=0x40, size=0x30 */
	0x00008960, 0x00000040, 0x00300000,
	/* USTORM_ETH_PF_STAT_OFFSET(pf_id), */
	/* offset=0xe840, mult1=0x60, size=0x60 */
	0x0000e840, 0x00000060, 0x00600000,
	/* PSTORM_QUEUE_STAT_OFFSET(stat_counter_id), */
	/* offset=0x4618, mult1=0x80, size=0x38 */
	0x00004618, 0x00000080, 0x00380000,
	/* PSTORM_ETH_PF_STAT_OFFSET(pf_id), */
	/* offset=0x10738, mult1=0xc0, size=0xc0 */
	0x00010738, 0x000000c0, 0x00c00000,
	/* PSTORM_CTL_FRAME_ETHTYPE_OFFSET(ethType_id), */
	/* offset=0x1f8, mult1=0x2, size=0x2 */
	0x000001f8, 0x00000002, 0x00020000,
	/* TSTORM_ETH_PRS_INPUT_OFFSET, */
	/* offset=0xa2a8, size=0x108 */
	0x0000a2a8, 0x00000000, 0x01080000,
	/* ETH_RX_RATE_LIMIT_OFFSET(pf_id), */
	/* offset=0xa3b0, mult1=0x8, size=0x8 */
	0x0000a3b0, 0x00000008, 0x00080000,
	/* TSTORM_ETH_RSS_UPDATE_OFFSET(pf_id), */
	/* offset=0x1c0, mult1=0x8, size=0x8 */
	0x000001c0, 0x00000008, 0x00080000,
	/* XSTORM_ETH_QUEUE_ZONE_OFFSET(queue_id), */
	/* offset=0x1f8, mult1=0x8, size=0x8 */
	0x000001f8, 0x00000008, 0x00080000,
	/* YSTORM_TOE_CQ_PROD_OFFSET(rss_id), */
	/* offset=0xac0, mult1=0x8, size=0x8 */
	0x00000ac0, 0x00000008, 0x00080000,
	/* USTORM_TOE_CQ_PROD_OFFSET(rss_id), */
	/* offset=0x2578, mult1=0x8, size=0x8 */
	0x00002578, 0x00000008, 0x00080000,
	/* USTORM_TOE_GRQ_PROD_OFFSET(pf_id), */
	/* offset=0x24f8, mult1=0x8, size=0x8 */
	0x000024f8, 0x00000008, 0x00080000,
	/* TSTORM_SCSI_CMDQ_CONS_OFFSET(cmdq_queue_id), */
	/* offset=0x280, mult1=0x8, size=0x8 */
	0x00000280, 0x00000008, 0x00080000,
	/* TSTORM_SCSI_BDQ_EXT_PROD_OFFSET(storage_func_id,bdq_id), */
	/* offset=0x680, mult1=0x18, mult2=0x8, size=0x8 */
	0x00000680, 0x00080018, 0x00080000,
	/* MSTORM_SCSI_BDQ_EXT_PROD_OFFSET(storage_func_id,bdq_id), */
	/* offset=0xb78, mult1=0x18, mult2=0x8, size=0x2 */
	0x00000b78, 0x00080018, 0x00020000,
	/* TSTORM_ISCSI_RX_STATS_OFFSET(storage_func_id), */
	/* offset=0xc648, mult1=0x50, size=0x3c */
	0x0000c648, 0x00000050, 0x003c0000,
	/* MSTORM_ISCSI_RX_STATS_OFFSET(storage_func_id), */
	/* offset=0x12038, mult1=0x18, size=0x10 */
	0x00012038, 0x00000018, 0x00100000,
	/* USTORM_ISCSI_RX_STATS_OFFSET(storage_func_id), */
	/* offset=0x11b00, mult1=0x40, size=0x18 */
	0x00011b00, 0x00000040, 0x00180000,
	/* XSTORM_ISCSI_TX_STATS_OFFSET(storage_func_id), */
	/* offset=0x95d0, mult1=0x50, size=0x20 */
	0x000095d0, 0x00000050, 0x00200000,
	/* YSTORM_ISCSI_TX_STATS_OFFSET(storage_func_id), */
	/* offset=0x8b10, mult1=0x40, size=0x28 */
	0x00008b10, 0x00000040, 0x00280000,
	/* PSTORM_ISCSI_TX_STATS_OFFSET(storage_func_id), */
	/* offset=0x11640, mult1=0x18, size=0x10 */
	0x00011640, 0x00000018, 0x00100000,
	/* TSTORM_FCOE_RX_STATS_OFFSET(pf_id), */
	/* offset=0xc830, mult1=0x48, size=0x38 */
	0x0000c830, 0x00000048, 0x00380000,
	/* PSTORM_FCOE_TX_STATS_OFFSET(pf_id), */
	/* offset=0x11710, mult1=0x20, size=0x20 */
	0x00011710, 0x00000020, 0x00200000,
	/* PSTORM_RDMA_QUEUE_STAT_OFFSET(rdma_stat_counter_id), */
	/* offset=0x4650, mult1=0x80, size=0x10 */
	0x00004650, 0x00000080, 0x00100000,
	/* TSTORM_RDMA_QUEUE_STAT_OFFSET(rdma_stat_counter_id), */
	/* offset=0x3618, mult1=0x10, size=0x10 */
	0x00003618, 0x00000010, 0x00100000,
	/* XSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), */
	/* offset=0xa968, mult1=0x8, size=0x1 */
	0x0000a968, 0x00000008, 0x00010000,
	/* YSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), */
	/* offset=0x97a0, mult1=0x8, size=0x1 */
	0x000097a0, 0x00000008, 0x00010000,
	/* PSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), */
	/* offset=0x11990, mult1=0x8, size=0x1 */
	0x00011990, 0x00000008, 0x00010000,
	/* TSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), */
	/* offset=0xf020, mult1=0x8, size=0x1 */
	0x0000f020, 0x00000008, 0x00010000,
	/* MSTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), */
	/* offset=0x12628, mult1=0x8, size=0x1 */
	0x00012628, 0x00000008, 0x00010000,
	/* USTORM_RDMA_ASSERT_LEVEL_OFFSET(pf_id), */
	/* offset=0x11da8, mult1=0x8, size=0x1 */
	0x00011da8, 0x00000008, 0x00010000,
	/* XSTORM_IWARP_RXMIT_STATS_OFFSET(pf_id), */
	/* offset=0xaa78, mult1=0x30, size=0x10 */
	0x0000aa78, 0x00000030, 0x00100000,
	/* TSTORM_ROCE_EVENTS_STAT_OFFSET(roce_pf_id), */
	/* offset=0xd770, mult1=0x28, size=0x28 */
	0x0000d770, 0x00000028, 0x00280000,
	/* YSTORM_ROCE_DCQCN_RECEIVED_STATS_OFFSET(roce_pf_id), */
	/* offset=0x9a58, mult1=0x18, size=0x18 */
	0x00009a58, 0x00000018, 0x00180000,
	/* YSTORM_ROCE_ERROR_STATS_OFFSET(roce_pf_id), */
	/* offset=0x9bd8, mult1=0x8, size=0x8 */
	0x00009bd8, 0x00000008, 0x00080000,
	/* PSTORM_ROCE_DCQCN_SENT_STATS_OFFSET(roce_pf_id), */
	/* offset=0x13a18, mult1=0x8, size=0x8 */
	0x00013a18, 0x00000008, 0x00080000,
	/* USTORM_ROCE_CQE_STATS_OFFSET(roce_pf_id), */
	/* offset=0x126e8, mult1=0x18, size=0x18 */
	0x000126e8, 0x00000018, 0x00180000,
	/* TSTORM_NVMF_PORT_TASKPOOL_PRODUCER_CONSUMER_OFFSET(port_num_id,taskpool_index), */
	/* offset=0xe610, mult1=0x288, mult2=0x50, size=0x10 */
	0x0000e610, 0x00500288, 0x00100000,
	/* USTORM_NVMF_PORT_COUNTERS_OFFSET(port_num_id), */
	/* offset=0x12970, mult1=0x138, size=0x28 */
	0x00012970, 0x00000138, 0x00280000,
};
/* Data size: 828 bytes */


#endif /* __IRO_VALUES_H__ */
