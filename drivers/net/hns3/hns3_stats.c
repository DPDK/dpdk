/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#include <stdbool.h>
#include <stdint.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_io.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>

#include "hns3_ethdev.h"
#include "hns3_rxtx.h"
#include "hns3_logs.h"
#include "hns3_regs.h"

/* MAC statistics */
static const struct hns3_xstats_name_offset hns3_mac_strings[] = {
	{"mac_tx_mac_pause_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_mac_pause_num)},
	{"mac_rx_mac_pause_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_mac_pause_num)},
	{"mac_tx_control_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_ctrl_pkt_num)},
	{"mac_rx_control_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_ctrl_pkt_num)},
	{"mac_tx_pfc_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_pfc_pause_pkt_num)},
	{"mac_tx_pfc_pri0_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_pfc_pri0_pkt_num)},
	{"mac_tx_pfc_pri1_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_pfc_pri1_pkt_num)},
	{"mac_tx_pfc_pri2_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_pfc_pri2_pkt_num)},
	{"mac_tx_pfc_pri3_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_pfc_pri3_pkt_num)},
	{"mac_tx_pfc_pri4_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_pfc_pri4_pkt_num)},
	{"mac_tx_pfc_pri5_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_pfc_pri5_pkt_num)},
	{"mac_tx_pfc_pri6_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_pfc_pri6_pkt_num)},
	{"mac_tx_pfc_pri7_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_pfc_pri7_pkt_num)},
	{"mac_rx_pfc_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_pfc_pause_pkt_num)},
	{"mac_rx_pfc_pri0_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_pfc_pri0_pkt_num)},
	{"mac_rx_pfc_pri1_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_pfc_pri1_pkt_num)},
	{"mac_rx_pfc_pri2_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_pfc_pri2_pkt_num)},
	{"mac_rx_pfc_pri3_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_pfc_pri3_pkt_num)},
	{"mac_rx_pfc_pri4_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_pfc_pri4_pkt_num)},
	{"mac_rx_pfc_pri5_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_pfc_pri5_pkt_num)},
	{"mac_rx_pfc_pri6_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_pfc_pri6_pkt_num)},
	{"mac_rx_pfc_pri7_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_pfc_pri7_pkt_num)},
	{"mac_tx_total_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_total_pkt_num)},
	{"mac_tx_total_oct_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_total_oct_num)},
	{"mac_tx_good_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_good_pkt_num)},
	{"mac_tx_bad_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_bad_pkt_num)},
	{"mac_tx_good_oct_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_good_oct_num)},
	{"mac_tx_bad_oct_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_bad_oct_num)},
	{"mac_tx_uni_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_uni_pkt_num)},
	{"mac_tx_multi_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_multi_pkt_num)},
	{"mac_tx_broad_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_broad_pkt_num)},
	{"mac_tx_undersize_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_undersize_pkt_num)},
	{"mac_tx_oversize_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_oversize_pkt_num)},
	{"mac_tx_64_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_64_oct_pkt_num)},
	{"mac_tx_65_127_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_65_127_oct_pkt_num)},
	{"mac_tx_128_255_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_128_255_oct_pkt_num)},
	{"mac_tx_256_511_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_256_511_oct_pkt_num)},
	{"mac_tx_512_1023_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_512_1023_oct_pkt_num)},
	{"mac_tx_1024_1518_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_1024_1518_oct_pkt_num)},
	{"mac_tx_1519_2047_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_1519_2047_oct_pkt_num)},
	{"mac_tx_2048_4095_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_2048_4095_oct_pkt_num)},
	{"mac_tx_4096_8191_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_4096_8191_oct_pkt_num)},
	{"mac_tx_8192_9216_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_8192_9216_oct_pkt_num)},
	{"mac_tx_9217_12287_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_9217_12287_oct_pkt_num)},
	{"mac_tx_12288_16383_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_12288_16383_oct_pkt_num)},
	{"mac_tx_1519_max_good_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_1519_max_good_oct_pkt_num)},
	{"mac_tx_1519_max_bad_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_1519_max_bad_oct_pkt_num)},
	{"mac_rx_total_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_total_pkt_num)},
	{"mac_rx_total_oct_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_total_oct_num)},
	{"mac_rx_good_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_good_pkt_num)},
	{"mac_rx_bad_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_bad_pkt_num)},
	{"mac_rx_good_oct_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_good_oct_num)},
	{"mac_rx_bad_oct_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_bad_oct_num)},
	{"mac_rx_uni_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_uni_pkt_num)},
	{"mac_rx_multi_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_multi_pkt_num)},
	{"mac_rx_broad_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_broad_pkt_num)},
	{"mac_rx_undersize_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_undersize_pkt_num)},
	{"mac_rx_oversize_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_oversize_pkt_num)},
	{"mac_rx_64_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_64_oct_pkt_num)},
	{"mac_rx_65_127_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_65_127_oct_pkt_num)},
	{"mac_rx_128_255_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_128_255_oct_pkt_num)},
	{"mac_rx_256_511_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_256_511_oct_pkt_num)},
	{"mac_rx_512_1023_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_512_1023_oct_pkt_num)},
	{"mac_rx_1024_1518_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_1024_1518_oct_pkt_num)},
	{"mac_rx_1519_2047_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_1519_2047_oct_pkt_num)},
	{"mac_rx_2048_4095_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_2048_4095_oct_pkt_num)},
	{"mac_rx_4096_8191_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_4096_8191_oct_pkt_num)},
	{"mac_rx_8192_9216_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_8192_9216_oct_pkt_num)},
	{"mac_rx_9217_12287_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_9217_12287_oct_pkt_num)},
	{"mac_rx_12288_16383_oct_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_12288_16383_oct_pkt_num)},
	{"mac_rx_1519_max_good_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_1519_max_good_oct_pkt_num)},
	{"mac_rx_1519_max_bad_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_1519_max_bad_oct_pkt_num)},
	{"mac_tx_fragment_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_fragment_pkt_num)},
	{"mac_tx_undermin_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_undermin_pkt_num)},
	{"mac_tx_jabber_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_jabber_pkt_num)},
	{"mac_tx_err_all_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_err_all_pkt_num)},
	{"mac_tx_from_app_good_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_from_app_good_pkt_num)},
	{"mac_tx_from_app_bad_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_tx_from_app_bad_pkt_num)},
	{"mac_rx_fragment_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_fragment_pkt_num)},
	{"mac_rx_undermin_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_undermin_pkt_num)},
	{"mac_rx_jabber_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_jabber_pkt_num)},
	{"mac_rx_fcs_err_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_fcs_err_pkt_num)},
	{"mac_rx_send_app_good_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_send_app_good_pkt_num)},
	{"mac_rx_send_app_bad_pkt_num",
		HNS3_MAC_STATS_OFFSET(mac_rx_send_app_bad_pkt_num)}
};

static const struct hns3_xstats_name_offset hns3_error_int_stats_strings[] = {
	{"MAC_AFIFO_TNL_INT_R",
		HNS3_ERR_INT_STATS_FIELD_OFFSET(mac_afifo_tnl_intr_cnt)},
	{"PPU_MPF_ABNORMAL_INT_ST2",
		HNS3_ERR_INT_STATS_FIELD_OFFSET(ppu_mpf_abnormal_intr_st2_cnt)},
	{"SSU_PORT_BASED_ERR_INT",
		HNS3_ERR_INT_STATS_FIELD_OFFSET(ssu_port_based_pf_intr_cnt)},
	{"PPP_PF_ABNORMAL_INT_ST0",
		HNS3_ERR_INT_STATS_FIELD_OFFSET(ppp_pf_abnormal_intr_cnt)},
	{"PPU_PF_ABNORMAL_INT_ST",
		HNS3_ERR_INT_STATS_FIELD_OFFSET(ppu_pf_abnormal_intr_cnt)}
};

/* The statistic of reset */
static const struct hns3_xstats_name_offset hns3_reset_stats_strings[] = {
	{"REQ_RESET_CNT",
		HNS3_RESET_STATS_FIELD_OFFSET(request_cnt)},
	{"GLOBAL_RESET_CNT",
		HNS3_RESET_STATS_FIELD_OFFSET(global_cnt)},
	{"IMP_RESET_CNT",
		HNS3_RESET_STATS_FIELD_OFFSET(imp_cnt)},
	{"RESET_EXEC_CNT",
		HNS3_RESET_STATS_FIELD_OFFSET(exec_cnt)},
	{"RESET_SUCCESS_CNT",
		HNS3_RESET_STATS_FIELD_OFFSET(success_cnt)},
	{"RESET_FAIL_CNT",
		HNS3_RESET_STATS_FIELD_OFFSET(fail_cnt)},
	{"RESET_MERGE_CNT",
		HNS3_RESET_STATS_FIELD_OFFSET(merge_cnt)}
};

/* The statistic of errors in Rx BD */
static const struct hns3_xstats_name_offset hns3_rx_bd_error_strings[] = {
	{"RX_PKT_LEN_ERRORS",
		HNS3_RX_BD_ERROR_STATS_FIELD_OFFSET(pkt_len_errors)},
	{"L2_RX_ERRORS",
		HNS3_RX_BD_ERROR_STATS_FIELD_OFFSET(l2_errors)},
	{"RX_L3_CHECKSUM_ERRORS",
		HNS3_RX_BD_ERROR_STATS_FIELD_OFFSET(l3_csum_erros)},
	{"RX_L4_CHECKSUM_ERRORS",
		HNS3_RX_BD_ERROR_STATS_FIELD_OFFSET(l4_csum_erros)},
	{"RX_OL3_CHECKSUM_ERRORS",
		HNS3_RX_BD_ERROR_STATS_FIELD_OFFSET(ol3_csum_erros)},
	{"RX_OL4_CHECKSUM_ERRORS",
		HNS3_RX_BD_ERROR_STATS_FIELD_OFFSET(ol4_csum_erros)}
};

/* The statistic of the Tx errors */
static const struct hns3_xstats_name_offset hns3_tx_errors_strings[] = {
	{"TX_OVER_LENGTH_PKT_CNT",
		HNS3_TX_ERROR_STATS_FIELD_OFFSET(over_length_pkt_cnt)},
	{"TX_EXCEED_LIMITED_BD_PKT_CNT",
		HNS3_TX_ERROR_STATS_FIELD_OFFSET(exceed_limit_bd_pkt_cnt)},
	{"TX_EXCEED_LIMITED_BD_PKT_REASSEMBLE_FAIL_CNT",
		HNS3_TX_ERROR_STATS_FIELD_OFFSET(exceed_limit_bd_reassem_fail)},
	{"TX_UNSUPPORTED_TUNNEL_PKT_CNT",
		HNS3_TX_ERROR_STATS_FIELD_OFFSET(unsupported_tunnel_pkt_cnt)},
	{"TX_QUEUE_FULL_CNT",
		HNS3_TX_ERROR_STATS_FIELD_OFFSET(queue_full_cnt)},
	{"TX_SHORT_PKT_PAD_FAIL_CNT",
		HNS3_TX_ERROR_STATS_FIELD_OFFSET(pkt_padding_fail_cnt)}
};

/* The statistic of rx queue */
static const struct hns3_xstats_name_offset hns3_rx_queue_strings[] = {
	{"RX_QUEUE_FBD", HNS3_RING_RX_FBDNUM_REG}
};

/* The statistic of tx queue */
static const struct hns3_xstats_name_offset hns3_tx_queue_strings[] = {
	{"TX_QUEUE_FBD", HNS3_RING_TX_FBDNUM_REG}
};

#define HNS3_NUM_MAC_STATS (sizeof(hns3_mac_strings) / \
	sizeof(hns3_mac_strings[0]))

#define HNS3_NUM_ERROR_INT_XSTATS (sizeof(hns3_error_int_stats_strings) / \
	sizeof(hns3_error_int_stats_strings[0]))

#define HNS3_NUM_RESET_XSTATS (sizeof(hns3_reset_stats_strings) / \
	sizeof(hns3_reset_stats_strings[0]))

#define HNS3_NUM_RX_BD_ERROR_XSTATS (sizeof(hns3_rx_bd_error_strings) / \
	sizeof(hns3_rx_bd_error_strings[0]))

#define HNS3_NUM_TX_ERRORS_XSTATS (sizeof(hns3_tx_errors_strings) / \
	sizeof(hns3_tx_errors_strings[0]))

#define HNS3_NUM_RX_QUEUE_STATS (sizeof(hns3_rx_queue_strings) / \
	sizeof(hns3_rx_queue_strings[0]))

#define HNS3_NUM_TX_QUEUE_STATS (sizeof(hns3_tx_queue_strings) / \
	sizeof(hns3_tx_queue_strings[0]))

#define HNS3_FIX_NUM_STATS (HNS3_NUM_MAC_STATS + HNS3_NUM_ERROR_INT_XSTATS + \
			    HNS3_NUM_RESET_XSTATS)

/*
 * Query all the MAC statistics data of Network ICL command ,opcode id: 0x0034.
 * This command is used before send 'query_mac_stat command', the descriptor
 * number of 'query_mac_stat command' must match with reg_num in this command.
 * @praram hw
 *   Pointer to structure hns3_hw.
 * @return
 *   0 on success.
 */
static int
hns3_update_mac_stats(struct hns3_hw *hw, const uint32_t desc_num)
{
	uint64_t *data = (uint64_t *)(&hw->mac_stats);
	struct hns3_cmd_desc *desc;
	uint64_t *desc_data;
	uint16_t i, k, n;
	int ret;

	desc = rte_malloc("hns3_mac_desc",
			  desc_num * sizeof(struct hns3_cmd_desc), 0);
	if (desc == NULL) {
		hns3_err(hw, "Mac_update_stats alloced desc malloc fail");
		return -ENOMEM;
	}

	hns3_cmd_setup_basic_desc(desc, HNS3_OPC_STATS_MAC_ALL, true);
	ret = hns3_cmd_send(hw, desc, desc_num);
	if (ret) {
		hns3_err(hw, "Update complete MAC pkt stats fail : %d", ret);
		rte_free(desc);
		return ret;
	}

	for (i = 0; i < desc_num; i++) {
		/* For special opcode 0034, only the first desc has the head */
		if (i == 0) {
			desc_data = (uint64_t *)(&desc[i].data[0]);
			n = HNS3_RD_FIRST_STATS_NUM;
		} else {
			desc_data = (uint64_t *)(&desc[i]);
			n = HNS3_RD_OTHER_STATS_NUM;
		}

		for (k = 0; k < n; k++) {
			*data += rte_le_to_cpu_64(*desc_data);
			data++;
			desc_data++;
		}
	}
	rte_free(desc);

	return 0;
}

/*
 * Query Mac stat reg num command ,opcode id: 0x0033.
 * This command is used before send 'query_mac_stat command', the descriptor
 * number of 'query_mac_stat command' must match with reg_num in this command.
 * @praram rte_stats
 *   Pointer to structure rte_eth_stats.
 * @return
 *   0 on success.
 */
static int
hns3_mac_query_reg_num(struct rte_eth_dev *dev, uint32_t *desc_num)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc;
	uint32_t *desc_data;
	uint32_t reg_num;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_MAC_REG_NUM, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		return ret;

	/*
	 * The num of MAC statistics registers that are provided by IMP in this
	 * version.
	 */
	desc_data = (uint32_t *)(&desc.data[0]);
	reg_num = rte_le_to_cpu_32(*desc_data);

	/*
	 * The descriptor number of 'query_additional_mac_stat command' is
	 * '1 + (reg_num-3)/4 + ((reg_num-3)%4 !=0)';
	 * This value is 83 in this version
	 */
	*desc_num = 1 + ((reg_num - 3) >> 2) +
		    (uint32_t)(((reg_num - 3) & 0x3) ? 1 : 0);

	return 0;
}

static int
hns3_query_update_mac_stats(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint32_t desc_num;
	int ret;

	ret = hns3_mac_query_reg_num(dev, &desc_num);
	if (ret == 0)
		ret = hns3_update_mac_stats(hw, desc_num);
	else
		hns3_err(hw, "Query mac reg num fail : %d", ret);
	return ret;
}

/* Get tqp stats from register */
static int
hns3_update_tqp_stats(struct hns3_hw *hw)
{
	struct hns3_tqp_stats *stats = &hw->tqp_stats;
	struct hns3_cmd_desc desc;
	uint64_t cnt;
	uint16_t i;
	int ret;

	for (i = 0; i < hw->tqps_num; i++) {
		hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_RX_STATUS,
					  true);

		desc.data[0] = rte_cpu_to_le_32((uint32_t)i &
						HNS3_QUEUE_ID_MASK);
		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			hns3_err(hw, "Failed to query RX No.%d queue stat: %d",
				 i, ret);
			return ret;
		}
		cnt = rte_le_to_cpu_32(desc.data[1]);
		stats->rcb_rx_ring_pktnum_rcd += cnt;
		stats->rcb_rx_ring_pktnum[i] += cnt;

		hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_TX_STATUS,
					  true);

		desc.data[0] = rte_cpu_to_le_32((uint32_t)i &
						HNS3_QUEUE_ID_MASK);
		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret) {
			hns3_err(hw, "Failed to query TX No.%d queue stat: %d",
				 i, ret);
			return ret;
		}
		cnt = rte_le_to_cpu_32(desc.data[1]);
		stats->rcb_tx_ring_pktnum_rcd += cnt;
		stats->rcb_tx_ring_pktnum[i] += cnt;
	}

	return 0;
}

/*
 * Query tqp tx queue statistics ,opcode id: 0x0B03.
 * Query tqp rx queue statistics ,opcode id: 0x0B13.
 * Get all statistics of a port.
 * @param eth_dev
 *   Pointer to Ethernet device.
 * @praram rte_stats
 *   Pointer to structure rte_eth_stats.
 * @return
 *   0 on success.
 */
int
hns3_stats_get(struct rte_eth_dev *eth_dev, struct rte_eth_stats *rte_stats)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_tqp_stats *stats = &hw->tqp_stats;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint64_t cnt;
	uint64_t num;
	uint16_t i;
	int ret;

	/* Update tqp stats by read register */
	ret = hns3_update_tqp_stats(hw);
	if (ret) {
		hns3_err(hw, "Update tqp stats fail : %d", ret);
		return ret;
	}

	/* Get the error stats of received packets */
	num = RTE_MIN(RTE_ETHDEV_QUEUE_STAT_CNTRS, eth_dev->data->nb_rx_queues);
	for (i = 0; i != num; ++i) {
		rxq = eth_dev->data->rx_queues[i];
		if (rxq) {
			cnt = rxq->l2_errors + rxq->pkt_len_errors;
			rte_stats->q_errors[i] = cnt;
			rte_stats->q_ipackets[i] =
				stats->rcb_rx_ring_pktnum[i] - cnt;
			rte_stats->ierrors += cnt;
		}
	}
	/* Get the error stats of transmitted packets */
	num = RTE_MIN(RTE_ETHDEV_QUEUE_STAT_CNTRS, eth_dev->data->nb_tx_queues);
	for (i = 0; i < num; i++) {
		txq = eth_dev->data->tx_queues[i];
		if (txq)
			rte_stats->q_opackets[i] = stats->rcb_tx_ring_pktnum[i];
	}

	rte_stats->oerrors = 0;
	rte_stats->ipackets  = stats->rcb_rx_ring_pktnum_rcd -
		rte_stats->ierrors;
	rte_stats->opackets  = stats->rcb_tx_ring_pktnum_rcd -
		rte_stats->oerrors;
	rte_stats->rx_nombuf = eth_dev->data->rx_mbuf_alloc_failed;

	return 0;
}

int
hns3_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_tqp_stats *stats = &hw->tqp_stats;
	struct hns3_cmd_desc desc_reset;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint16_t i;
	int ret;

	/*
	 * If this is a reset xstats is NULL, and we have cleared the
	 * registers by reading them.
	 */
	for (i = 0; i < hw->tqps_num; i++) {
		hns3_cmd_setup_basic_desc(&desc_reset, HNS3_OPC_QUERY_RX_STATUS,
					  true);
		desc_reset.data[0] = rte_cpu_to_le_32((uint32_t)i &
						      HNS3_QUEUE_ID_MASK);
		ret = hns3_cmd_send(hw, &desc_reset, 1);
		if (ret) {
			hns3_err(hw, "Failed to reset RX No.%d queue stat: %d",
				 i, ret);
			return ret;
		}

		hns3_cmd_setup_basic_desc(&desc_reset, HNS3_OPC_QUERY_TX_STATUS,
					  true);
		desc_reset.data[0] = rte_cpu_to_le_32((uint32_t)i &
						      HNS3_QUEUE_ID_MASK);
		ret = hns3_cmd_send(hw, &desc_reset, 1);
		if (ret) {
			hns3_err(hw, "Failed to reset TX No.%d queue stat: %d",
				 i, ret);
			return ret;
		}
	}

	/* Clear the Rx BD errors stats */
	for (i = 0; i != eth_dev->data->nb_rx_queues; ++i) {
		rxq = eth_dev->data->rx_queues[i];
		if (rxq) {
			rxq->pkt_len_errors = 0;
			rxq->l2_errors = 0;
			rxq->l3_csum_erros = 0;
			rxq->l4_csum_erros = 0;
			rxq->ol3_csum_erros = 0;
			rxq->ol4_csum_erros = 0;
		}
	}

	/* Clear the Tx errors stats */
	for (i = 0; i != eth_dev->data->nb_tx_queues; ++i) {
		txq = eth_dev->data->tx_queues[i];
		if (txq) {
			txq->over_length_pkt_cnt = 0;
			txq->exceed_limit_bd_pkt_cnt = 0;
			txq->exceed_limit_bd_reassem_fail = 0;
			txq->unsupported_tunnel_pkt_cnt = 0;
			txq->queue_full_cnt = 0;
			txq->pkt_padding_fail_cnt = 0;
		}
	}

	memset(stats, 0, sizeof(struct hns3_tqp_stats));

	return 0;
}

static int
hns3_mac_stats_reset(__rte_unused struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_mac_stats *mac_stats = &hw->mac_stats;
	int ret;

	ret = hns3_query_update_mac_stats(dev);
	if (ret) {
		hns3_err(hw, "Clear Mac stats fail : %d", ret);
		return ret;
	}

	memset(mac_stats, 0, sizeof(struct hns3_mac_stats));

	return 0;
}

/* This function calculates the number of xstats based on the current config */
static int
hns3_xstats_calc_num(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	int bderr_stats = dev->data->nb_rx_queues * HNS3_NUM_RX_BD_ERROR_XSTATS;
	int tx_err_stats = dev->data->nb_tx_queues * HNS3_NUM_TX_ERRORS_XSTATS;
	int rx_queue_stats = dev->data->nb_rx_queues * HNS3_NUM_RX_QUEUE_STATS;
	int tx_queue_stats = dev->data->nb_tx_queues * HNS3_NUM_TX_QUEUE_STATS;

	if (hns->is_vf)
		return bderr_stats + tx_err_stats + rx_queue_stats +
		       tx_queue_stats + HNS3_NUM_RESET_XSTATS;
	else
		return bderr_stats + tx_err_stats + rx_queue_stats +
		       tx_queue_stats + HNS3_FIX_NUM_STATS;
}

static void
hns3_get_queue_stats(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		     int *count)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint32_t reg_offset;
	uint16_t i, j;

	/* Get rx queue stats */
	for (j = 0; j < dev->data->nb_rx_queues; j++) {
		for (i = 0; i < HNS3_NUM_RX_QUEUE_STATS; i++) {
			reg_offset = HNS3_TQP_REG_OFFSET +
					HNS3_TQP_REG_SIZE * j;
			xstats[*count].value = hns3_read_dev(hw,
				reg_offset + hns3_rx_queue_strings[i].offset);
			xstats[*count].id = *count;
			(*count)++;
		}
	}

	/* Get tx queue stats */
	for (j = 0; j < dev->data->nb_tx_queues; j++) {
		for (i = 0; i < HNS3_NUM_TX_QUEUE_STATS; i++) {
			reg_offset = HNS3_TQP_REG_OFFSET +
					HNS3_TQP_REG_SIZE * j;
			xstats[*count].value = hns3_read_dev(hw,
				reg_offset + hns3_tx_queue_strings[i].offset);
			xstats[*count].id = *count;
			(*count)++;
		}
	}

}

/*
 * Retrieve extended(tqp | Mac) statistics of an Ethernet device.
 * @param dev
 *   Pointer to Ethernet device.
 * @praram xstats
 *   A pointer to a table of structure of type *rte_eth_xstat*
 *   to be filled with device statistics ids and values.
 *   This parameter can be set to NULL if n is 0.
 * @param n
 *   The size of the xstats array (number of elements).
 * @return
 *   0 on fail, count(The size of the statistics elements) on success.
 */
int
hns3_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		    unsigned int n)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_pf *pf = &hns->pf;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_mac_stats *mac_stats = &hw->mac_stats;
	struct hns3_reset_stats *reset_stats = &hw->reset.stats;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint16_t i, j;
	char *addr;
	int count;
	int ret;

	if (xstats == NULL)
		return 0;

	count = hns3_xstats_calc_num(dev);
	if ((int)n < count)
		return count;

	count = 0;

	if (!hns->is_vf) {
		/* Update Mac stats */
		ret = hns3_query_update_mac_stats(dev);
		if (ret) {
			hns3_err(hw, "Update Mac stats fail : %d", ret);
			return 0;
		}

		/* Get MAC stats from hw->hw_xstats.mac_stats struct */
		for (i = 0; i < HNS3_NUM_MAC_STATS; i++) {
			addr = (char *)mac_stats + hns3_mac_strings[i].offset;
			xstats[count].value = *(uint64_t *)addr;
			xstats[count].id = count;
			count++;
		}

		for (i = 0; i < HNS3_NUM_ERROR_INT_XSTATS; i++) {
			addr = (char *)&pf->abn_int_stats +
			       hns3_error_int_stats_strings[i].offset;
			xstats[count].value = *(uint64_t *)addr;
			xstats[count].id = count;
			count++;
		}
	}

	/* Get the reset stat */
	for (i = 0; i < HNS3_NUM_RESET_XSTATS; i++) {
		addr = (char *)reset_stats + hns3_reset_stats_strings[i].offset;
		xstats[count].value = *(uint64_t *)addr;
		xstats[count].id = count;
		count++;
	}

	/* Get the Rx BD errors stats */
	for (j = 0; j < dev->data->nb_rx_queues; j++) {
		for (i = 0; i < HNS3_NUM_RX_BD_ERROR_XSTATS; i++) {
			rxq = dev->data->rx_queues[j];
			addr = (char *)rxq + hns3_rx_bd_error_strings[i].offset;
			xstats[count].value = *(uint64_t *)addr;
			xstats[count].id = count;
			count++;
		}
	}

	/* Get the Tx errors stats */
	for (j = 0; j < dev->data->nb_tx_queues; j++) {
		for (i = 0; i < HNS3_NUM_TX_ERRORS_XSTATS; i++) {
			txq = dev->data->tx_queues[j];
			addr = (char *)txq + hns3_tx_errors_strings[i].offset;
			xstats[count].value = *(uint64_t *)addr;
			xstats[count].id = count;
			count++;
		}
	}

	hns3_get_queue_stats(dev, xstats, &count);
	return count;
}

/*
 * Retrieve names of extended statistics of an Ethernet device.
 *
 * There is an assumption that 'xstat_names' and 'xstats' arrays are matched
 * by array index:
 *  xstats_names[i].name => xstats[i].value
 *
 * And the array index is same with id field of 'struct rte_eth_xstat':
 *  xstats[i].id == i
 *
 * This assumption makes key-value pair matching less flexible but simpler.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param xstats_names
 *   An rte_eth_xstat_name array of at least *size* elements to
 *   be filled. If set to NULL, the function returns the required number
 *   of elements.
 * @param size
 *   The size of the xstats_names array (number of elements).
 * @return
 *   - A positive value lower or equal to size: success. The return value
 *     is the number of entries filled in the stats table.
 */
int
hns3_dev_xstats_get_names(struct rte_eth_dev *dev,
			  struct rte_eth_xstat_name *xstats_names,
			  __rte_unused unsigned int size)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	int cnt_stats = hns3_xstats_calc_num(dev);
	uint32_t count = 0;
	uint16_t i, j;

	if (xstats_names == NULL)
		return cnt_stats;

	/* Note: size limited checked in rte_eth_xstats_get_names() */
	if (!hns->is_vf) {
		/* Get MAC name from hw->hw_xstats.mac_stats struct */
		for (i = 0; i < HNS3_NUM_MAC_STATS; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "%s", hns3_mac_strings[i].name);
			count++;
		}

		for (i = 0; i < HNS3_NUM_ERROR_INT_XSTATS; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "%s", hns3_error_int_stats_strings[i].name);
			count++;
		}
	}
	for (i = 0; i < HNS3_NUM_RESET_XSTATS; i++) {
		snprintf(xstats_names[count].name,
			 sizeof(xstats_names[count].name),
			 "%s", hns3_reset_stats_strings[i].name);
		count++;
	}

	for (j = 0; j < dev->data->nb_rx_queues; j++) {
		for (i = 0; i < HNS3_NUM_RX_BD_ERROR_XSTATS; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "rx_q%u%s", j,
				 hns3_rx_bd_error_strings[i].name);
			count++;
		}
	}

	for (j = 0; j < dev->data->nb_tx_queues; j++) {
		for (i = 0; i < HNS3_NUM_TX_ERRORS_XSTATS; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "tx_q%u%s", j,
				 hns3_tx_errors_strings[i].name);
			count++;
		}
	}

	for (j = 0; j < dev->data->nb_rx_queues; j++) {
		for (i = 0; i < HNS3_NUM_RX_QUEUE_STATS; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "rx_q%u%s", j, hns3_rx_queue_strings[i].name);
			count++;
		}
	}

	for (j = 0; j < dev->data->nb_tx_queues; j++) {
		for (i = 0; i < HNS3_NUM_TX_QUEUE_STATS; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "tx_q%u%s", j, hns3_tx_queue_strings[i].name);
			count++;
		}
	}

	return count;
}

/*
 * Retrieve extended statistics of an Ethernet device.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param ids
 *   A pointer to an ids array passed by application. This tells which
 *   statistics values function should retrieve. This parameter
 *   can be set to NULL if size is 0. In this case function will retrieve
 *   all avalible statistics.
 * @param values
 *   A pointer to a table to be filled with device statistics values.
 * @param size
 *   The size of the ids array (number of elements).
 * @return
 *   - A positive value lower or equal to size: success. The return value
 *     is the number of entries filled in the stats table.
 *   - A positive value higher than size: error, the given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 *   - 0 on no ids.
 */
int
hns3_dev_xstats_get_by_id(struct rte_eth_dev *dev, const uint64_t *ids,
			  uint64_t *values, uint32_t size)
{
	const uint32_t cnt_stats = hns3_xstats_calc_num(dev);
	struct hns3_adapter *hns = dev->data->dev_private;
	struct rte_eth_xstat *values_copy;
	struct hns3_hw *hw = &hns->hw;
	uint32_t count_value;
	uint64_t len;
	uint32_t i;
	int ret;

	if (ids == NULL || size < cnt_stats)
		return cnt_stats;

	/* Update tqp stats by read register */
	ret = hns3_update_tqp_stats(hw);
	if (ret) {
		hns3_err(hw, "Update tqp stats fail : %d", ret);
		return ret;
	}

	len = cnt_stats * sizeof(struct rte_eth_xstat);
	values_copy = rte_zmalloc("hns3_xstats_values", len, 0);
	if (values_copy == NULL) {
		hns3_err(hw, "Failed to allocate %" PRIx64 " bytes needed "
			     "to store statistics values", len);
		return -ENOMEM;
	}

	count_value = hns3_dev_xstats_get(dev, values_copy, cnt_stats);
	if (count_value != cnt_stats) {
		rte_free(values_copy);
		return -EINVAL;
	}

	for (i = 0; i < size; i++) {
		if (ids[i] >= cnt_stats) {
			hns3_err(hw, "ids[%d] (%" PRIx64 ") is invalid, "
				     "should < %u", i, ids[i], cnt_stats);
			rte_free(values_copy);
			return -EINVAL;
		}
		memcpy(&values[i], &values_copy[ids[i]].value,
			sizeof(values[i]));
	}

	rte_free(values_copy);
	return size;
}

/*
 * Retrieve names of extended statistics of an Ethernet device.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param xstats_names
 *   An rte_eth_xstat_name array of at least *size* elements to
 *   be filled. If set to NULL, the function returns the required number
 *   of elements.
 * @param ids
 *   IDs array given by app to retrieve specific statistics
 * @param size
 *   The size of the xstats_names array (number of elements).
 * @return
 *   - A positive value lower or equal to size: success. The return value
 *     is the number of entries filled in the stats table.
 *   - A positive value higher than size: error, the given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 */
int
hns3_dev_xstats_get_names_by_id(struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				const uint64_t *ids, uint32_t size)
{
	const uint32_t cnt_stats = hns3_xstats_calc_num(dev);
	struct hns3_adapter *hns = dev->data->dev_private;
	struct rte_eth_xstat_name *names_copy;
	struct hns3_hw *hw = &hns->hw;
	uint64_t len;
	uint32_t i;

	if (ids == NULL || xstats_names == NULL)
		return cnt_stats;

	len = cnt_stats * sizeof(struct rte_eth_xstat_name);
	names_copy = rte_zmalloc("hns3_xstats_names", len, 0);
	if (names_copy == NULL) {
		hns3_err(hw, "Failed to allocate %" PRIx64 " bytes needed "
			     "to store statistics names", len);
		return -ENOMEM;
	}

	(void)hns3_dev_xstats_get_names(dev, names_copy, cnt_stats);

	for (i = 0; i < size; i++) {
		if (ids[i] >= cnt_stats) {
			hns3_err(hw, "ids[%d] (%" PRIx64 ") is invalid, "
				     "should < %u", i, ids[i], cnt_stats);
			rte_free(names_copy);
			return -EINVAL;
		}
		snprintf(xstats_names[i].name, sizeof(xstats_names[i].name),
			 "%s", names_copy[ids[i]].name);
	}

	rte_free(names_copy);
	return size;
}

int
hns3_dev_xstats_reset(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_pf *pf = &hns->pf;
	int ret;

	/* Clear tqp stats */
	ret = hns3_stats_reset(dev);
	if (ret)
		return ret;

	/* Clear reset stats */
	memset(&hns->hw.reset.stats, 0, sizeof(struct hns3_reset_stats));

	if (hns->is_vf)
		return 0;

	/* HW registers are cleared on read */
	ret = hns3_mac_stats_reset(dev);
	if (ret)
		return ret;

	/* Clear error stats */
	memset(&pf->abn_int_stats, 0, sizeof(struct hns3_err_msix_intr_stats));

	return 0;
}
