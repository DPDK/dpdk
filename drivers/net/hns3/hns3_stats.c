/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <rte_ethdev.h>
#include <rte_io.h>
#include <rte_malloc.h>

#include "hns3_ethdev.h"
#include "hns3_rxtx.h"
#include "hns3_logs.h"
#include "hns3_regs.h"

/* The statistics of the per-rxq basic stats */
static const struct hns3_xstats_name_offset hns3_rxq_basic_stats_strings[] = {
	{"packets",
		HNS3_RXQ_BASIC_STATS_FIELD_OFFSET(packets)},
	{"bytes",
		HNS3_RXQ_BASIC_STATS_FIELD_OFFSET(bytes)},
	{"errors",
		HNS3_RXQ_BASIC_STATS_FIELD_OFFSET(errors)}
};

/* The statistics of the per-txq basic stats */
static const struct hns3_xstats_name_offset hns3_txq_basic_stats_strings[] = {
	{"packets",
		HNS3_TXQ_BASIC_STATS_FIELD_OFFSET(packets)},
	{"bytes",
		HNS3_TXQ_BASIC_STATS_FIELD_OFFSET(bytes)}
};

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
	{"PKT_LEN_ERRORS",
		HNS3_RX_BD_ERROR_STATS_FIELD_OFFSET(pkt_len_errors)},
	{"L2_ERRORS",
		HNS3_RX_BD_ERROR_STATS_FIELD_OFFSET(l2_errors)}
};

/* The dfx statistic in Rx datapath */
static const struct hns3_xstats_name_offset hns3_rxq_dfx_stats_strings[] = {
	{"L3_CHECKSUM_ERRORS",
		HNS3_RXQ_DFX_STATS_FIELD_OFFSET(l3_csum_errors)},
	{"L4_CHECKSUM_ERRORS",
		HNS3_RXQ_DFX_STATS_FIELD_OFFSET(l4_csum_errors)},
	{"OL3_CHECKSUM_ERRORS",
		HNS3_RXQ_DFX_STATS_FIELD_OFFSET(ol3_csum_errors)},
	{"OL4_CHECKSUM_ERRORS",
		HNS3_RXQ_DFX_STATS_FIELD_OFFSET(ol4_csum_errors)}
};

/* The dfx statistic in Tx datapath */
static const struct hns3_xstats_name_offset hns3_txq_dfx_stats_strings[] = {
	{"OVER_LENGTH_PKT_CNT",
		HNS3_TXQ_DFX_STATS_FIELD_OFFSET(over_length_pkt_cnt)},
	{"EXCEED_LIMITED_BD_PKT_CNT",
		HNS3_TXQ_DFX_STATS_FIELD_OFFSET(exceed_limit_bd_pkt_cnt)},
	{"EXCEED_LIMITED_BD_PKT_REASSEMBLE_FAIL_CNT",
		HNS3_TXQ_DFX_STATS_FIELD_OFFSET(exceed_limit_bd_reassem_fail)},
	{"UNSUPPORTED_TUNNEL_PKT_CNT",
		HNS3_TXQ_DFX_STATS_FIELD_OFFSET(unsupported_tunnel_pkt_cnt)},
	{"QUEUE_FULL_CNT",
		HNS3_TXQ_DFX_STATS_FIELD_OFFSET(queue_full_cnt)},
	{"SHORT_PKT_PAD_FAIL_CNT",
		HNS3_TXQ_DFX_STATS_FIELD_OFFSET(pkt_padding_fail_cnt)}
};

/* The statistic of rx queue */
static const struct hns3_xstats_name_offset hns3_rx_queue_strings[] = {
	{"RX_QUEUE_FBD", HNS3_RING_RX_FBDNUM_REG}
};

/* The statistic of tx queue */
static const struct hns3_xstats_name_offset hns3_tx_queue_strings[] = {
	{"TX_QUEUE_FBD", HNS3_RING_TX_FBDNUM_REG}
};

/* The statistic of imissed packet */
static const struct hns3_xstats_name_offset hns3_imissed_stats_strings[] = {
	{"RPU_DROP_CNT",
		HNS3_IMISSED_STATS_FIELD_OFFSET(rpu_rx_drop_cnt)},
	{"SSU_DROP_CNT",
		HNS3_IMISSED_STATS_FIELD_OFFSET(ssu_rx_drop_cnt)},
};

#define HNS3_NUM_MAC_STATS (sizeof(hns3_mac_strings) / \
	sizeof(hns3_mac_strings[0]))

#define HNS3_NUM_RESET_XSTATS (sizeof(hns3_reset_stats_strings) / \
	sizeof(hns3_reset_stats_strings[0]))

#define HNS3_NUM_RX_BD_ERROR_XSTATS (sizeof(hns3_rx_bd_error_strings) / \
	sizeof(hns3_rx_bd_error_strings[0]))

#define HNS3_NUM_RXQ_DFX_XSTATS (sizeof(hns3_rxq_dfx_stats_strings) / \
	sizeof(hns3_rxq_dfx_stats_strings[0]))

#define HNS3_NUM_TXQ_DFX_XSTATS (sizeof(hns3_txq_dfx_stats_strings) / \
	sizeof(hns3_txq_dfx_stats_strings[0]))

#define HNS3_NUM_RX_QUEUE_STATS (sizeof(hns3_rx_queue_strings) / \
	sizeof(hns3_rx_queue_strings[0]))

#define HNS3_NUM_TX_QUEUE_STATS (sizeof(hns3_tx_queue_strings) / \
	sizeof(hns3_tx_queue_strings[0]))

#define HNS3_NUM_RXQ_BASIC_STATS (sizeof(hns3_rxq_basic_stats_strings) / \
	sizeof(hns3_rxq_basic_stats_strings[0]))

#define HNS3_NUM_TXQ_BASIC_STATS (sizeof(hns3_txq_basic_stats_strings) / \
	sizeof(hns3_txq_basic_stats_strings[0]))

#define HNS3_NUM_IMISSED_XSTATS (sizeof(hns3_imissed_stats_strings) / \
	sizeof(hns3_imissed_stats_strings[0]))

#define HNS3_FIX_NUM_STATS (HNS3_NUM_MAC_STATS + HNS3_NUM_RESET_XSTATS)

static void hns3_tqp_stats_clear(struct hns3_hw *hw);

static int
hns3_update_mac_stats(struct hns3_hw *hw)
{
#define HNS3_MAC_STATS_REG_NUM_PER_DESC	4

	uint64_t *data = (uint64_t *)(&hw->mac_stats);
	struct hns3_cmd_desc *desc;
	uint32_t stats_iterms;
	uint64_t *desc_data;
	uint32_t desc_num;
	uint32_t i;
	int ret;

	/* The first desc has a 64-bit header, so need to consider it. */
	desc_num = hw->mac_stats_reg_num / HNS3_MAC_STATS_REG_NUM_PER_DESC + 1;
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

	stats_iterms = RTE_MIN(sizeof(hw->mac_stats) / sizeof(uint64_t),
			       hw->mac_stats_reg_num);
	desc_data = (uint64_t *)(&desc[0].data[0]);
	for (i = 0; i < stats_iterms; i++) {
		/*
		 * Data memory is continuous and only the first descriptor has a
		 * header in this command.
		 */
		*data += rte_le_to_cpu_64(*desc_data);
		data++;
		desc_data++;
	}
	rte_free(desc);

	return 0;
}

static int
hns3_mac_query_reg_num(struct hns3_hw *hw, uint32_t *reg_num)
{
#define HNS3_MAC_STATS_RSV_REG_NUM_ON_HIP08_B	3
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_MAC_REG_NUM, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "failed to query MAC statistic reg number, ret = %d",
			 ret);
		return ret;
	}

	/* The number of MAC statistics registers are provided by firmware. */
	*reg_num = rte_le_to_cpu_32(desc.data[0]);
	if (*reg_num == 0) {
		hns3_err(hw, "MAC statistic reg number is invalid!");
		return -ENODATA;
	}

	/*
	 * If driver doesn't request the firmware to report more MAC statistics
	 * iterms and the total number of MAC statistics registers by using new
	 * method, firmware will only reports the number of valid statistics
	 * registers. However, structure hns3_mac_stats in driver contains valid
	 * and reserved statistics iterms. In this case, the total register
	 * number must be added to three reserved statistics registers.
	 */
	*reg_num += HNS3_MAC_STATS_RSV_REG_NUM_ON_HIP08_B;

	return 0;
}

int
hns3_query_mac_stats_reg_num(struct hns3_hw *hw)
{
	uint32_t mac_stats_reg_num = 0;
	int ret;

	ret = hns3_mac_query_reg_num(hw, &mac_stats_reg_num);
	if (ret)
		return ret;

	hw->mac_stats_reg_num = mac_stats_reg_num;
	if (hw->mac_stats_reg_num > sizeof(hw->mac_stats) / sizeof(uint64_t))
		hns3_warn(hw, "MAC stats reg number from firmware is greater than stats iterms in driver.");

	return 0;
}

static int
hns3_update_port_rpu_drop_stats(struct hns3_hw *hw)
{
	struct hns3_rx_missed_stats *stats = &hw->imissed_stats;
	struct hns3_query_rpu_cmd *req;
	struct hns3_cmd_desc desc;
	uint64_t cnt;
	uint32_t tc_num;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_DFX_RPU_REG_0, true);
	req = (struct hns3_query_rpu_cmd *)desc.data;

	/*
	 * tc_num is 0, means rpu stats of all TC channels will be
	 * get from firmware
	 */
	tc_num = 0;
	req->tc_queue_num = rte_cpu_to_le_32(tc_num);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "failed to query RPU stats: %d", ret);
		return ret;
	}

	cnt = rte_le_to_cpu_32(req->rpu_rx_pkt_drop_cnt);
	stats->rpu_rx_drop_cnt += cnt;

	return 0;
}

static void
hns3_update_function_rpu_drop_stats(struct hns3_hw *hw)
{
	struct hns3_rx_missed_stats *stats = &hw->imissed_stats;

	stats->rpu_rx_drop_cnt += hns3_read_dev(hw, HNS3_RPU_DROP_CNT_REG);
}

static int
hns3_update_rpu_drop_stats(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	int ret = 0;

	if (hw->drop_stats_mode == HNS3_PKTS_DROP_STATS_MODE1 && !hns->is_vf)
		ret = hns3_update_port_rpu_drop_stats(hw);
	else if (hw->drop_stats_mode == HNS3_PKTS_DROP_STATS_MODE2)
		hns3_update_function_rpu_drop_stats(hw);

	return ret;
}

static int
hns3_get_ssu_drop_stats(struct hns3_hw *hw, struct hns3_cmd_desc *desc,
			int bd_num, bool is_rx)
{
	struct hns3_query_ssu_cmd *req;
	int ret;
	int i;

	for (i = 0; i < bd_num - 1; i++) {
		hns3_cmd_setup_basic_desc(&desc[i],
					  HNS3_OPC_SSU_DROP_REG, true);
		desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	}
	hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_SSU_DROP_REG, true);
	req = (struct hns3_query_ssu_cmd *)desc[0].data;
	req->rxtx = is_rx ? 0 : 1;
	ret = hns3_cmd_send(hw, desc, bd_num);

	return ret;
}

static int
hns3_update_port_rx_ssu_drop_stats(struct hns3_hw *hw)
{
	struct hns3_rx_missed_stats *stats = &hw->imissed_stats;
	struct hns3_cmd_desc desc[HNS3_OPC_SSU_DROP_REG_NUM];
	struct hns3_query_ssu_cmd *req;
	uint64_t cnt;
	int ret;

	ret = hns3_get_ssu_drop_stats(hw, desc, HNS3_OPC_SSU_DROP_REG_NUM,
				      true);
	if (ret) {
		hns3_err(hw, "failed to get Rx SSU drop stats, ret = %d", ret);
		return ret;
	}

	req = (struct hns3_query_ssu_cmd *)desc[0].data;
	cnt = rte_le_to_cpu_32(req->oq_drop_cnt) +
	      rte_le_to_cpu_32(req->full_drop_cnt) +
	      rte_le_to_cpu_32(req->part_drop_cnt);

	stats->ssu_rx_drop_cnt += cnt;

	return 0;
}

static int
hns3_update_port_tx_ssu_drop_stats(struct hns3_hw *hw)
{
	struct hns3_cmd_desc desc[HNS3_OPC_SSU_DROP_REG_NUM];
	struct hns3_query_ssu_cmd *req;
	uint64_t cnt;
	int ret;

	ret = hns3_get_ssu_drop_stats(hw, desc, HNS3_OPC_SSU_DROP_REG_NUM,
				      false);
	if (ret) {
		hns3_err(hw, "failed to get Tx SSU drop stats, ret = %d", ret);
		return ret;
	}

	req = (struct hns3_query_ssu_cmd *)desc[0].data;
	cnt = rte_le_to_cpu_32(req->oq_drop_cnt) +
	      rte_le_to_cpu_32(req->full_drop_cnt) +
	      rte_le_to_cpu_32(req->part_drop_cnt);

	hw->oerror_stats += cnt;

	return 0;
}

static int
hns3_update_imissed_stats(struct hns3_hw *hw, bool is_clear)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	int ret;

	if (hw->drop_stats_mode == HNS3_PKTS_DROP_STATS_MODE1 && hns->is_vf)
		return 0;

	if (hw->drop_stats_mode == HNS3_PKTS_DROP_STATS_MODE2 && !hns->is_vf) {
		ret = hns3_update_port_rx_ssu_drop_stats(hw);
		if (ret)
			return ret;
	}

	ret = hns3_update_rpu_drop_stats(hw);
	if (ret)
		return ret;

	if (is_clear)
		memset(&hw->imissed_stats, 0, sizeof(hw->imissed_stats));

	return 0;
}

static int
hns3_update_oerror_stats(struct hns3_hw *hw, bool is_clear)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	int ret;

	if (hw->drop_stats_mode == HNS3_PKTS_DROP_STATS_MODE1 || hns->is_vf)
		return 0;

	ret = hns3_update_port_tx_ssu_drop_stats(hw);
	if (ret)
		return ret;

	if (is_clear)
		hw->oerror_stats = 0;

	return 0;
}

static void
hns3_rcb_rx_ring_stats_get(struct hns3_rx_queue *rxq,
			   struct hns3_tqp_stats *stats)
{
	uint32_t cnt;

	cnt = hns3_read_dev(rxq, HNS3_RING_RX_PKTNUM_RECORD_REG);
	stats->rcb_rx_ring_pktnum_rcd += cnt;
	stats->rcb_rx_ring_pktnum[rxq->queue_id] += cnt;
}

static void
hns3_rcb_tx_ring_stats_get(struct hns3_tx_queue *txq,
			   struct hns3_tqp_stats *stats)
{
	uint32_t cnt;

	cnt = hns3_read_dev(txq, HNS3_RING_TX_PKTNUM_RECORD_REG);
	stats->rcb_tx_ring_pktnum_rcd += cnt;
	stats->rcb_tx_ring_pktnum[txq->queue_id] += cnt;
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
	struct hns3_rx_missed_stats *imissed_stats = &hw->imissed_stats;
	struct hns3_tqp_stats *stats = &hw->tqp_stats;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint16_t i;
	int ret;

	rte_spinlock_lock(&hw->stats_lock);
	/* Update imissed stats */
	ret = hns3_update_imissed_stats(hw, false);
	if (ret) {
		hns3_err(hw, "update imissed stats failed, ret = %d", ret);
		goto out;
	}
	rte_stats->imissed = imissed_stats->rpu_rx_drop_cnt +
				imissed_stats->ssu_rx_drop_cnt;

	/* Get the error stats and bytes of received packets */
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];
		if (rxq == NULL)
			continue;

		hns3_rcb_rx_ring_stats_get(rxq, stats);
		rte_stats->ierrors += rxq->err_stats.l2_errors +
				      rxq->err_stats.pkt_len_errors;
		rte_stats->ibytes += rxq->basic_stats.bytes;
	}

	/* Reads all the stats of a txq in a loop to keep them synchronized */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		txq = eth_dev->data->tx_queues[i];
		if (txq == NULL)
			continue;

		hns3_rcb_tx_ring_stats_get(txq, stats);
		rte_stats->obytes += txq->basic_stats.bytes;
	}

	ret = hns3_update_oerror_stats(hw, false);
	if (ret) {
		hns3_err(hw, "update oerror stats failed, ret = %d", ret);
		goto out;
	}
	rte_stats->oerrors = hw->oerror_stats;

	/*
	 * If HW statistics are reset by stats_reset, but a lot of residual
	 * packets exist in the hardware queue and these packets are error
	 * packets, flip overflow may occurred. So return 0 in this case.
	 */
	rte_stats->ipackets =
		stats->rcb_rx_ring_pktnum_rcd > rte_stats->ierrors ?
		stats->rcb_rx_ring_pktnum_rcd - rte_stats->ierrors : 0;
	rte_stats->opackets  = stats->rcb_tx_ring_pktnum_rcd -
		rte_stats->oerrors;
	rte_stats->rx_nombuf = eth_dev->data->rx_mbuf_alloc_failed;

out:
	rte_spinlock_unlock(&hw->stats_lock);

	return ret;
}

int
hns3_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint16_t i;
	int ret;

	rte_spinlock_lock(&hw->stats_lock);
	/*
	 * Note: Reading hardware statistics of imissed registers will
	 * clear them.
	 */
	ret = hns3_update_imissed_stats(hw, true);
	if (ret) {
		hns3_err(hw, "clear imissed stats failed, ret = %d", ret);
		goto out;
	}

	/*
	 * Note: Reading hardware statistics of oerror registers will
	 * clear them.
	 */
	ret = hns3_update_oerror_stats(hw, true);
	if (ret) {
		hns3_err(hw, "clear oerror stats failed, ret = %d", ret);
		goto out;
	}

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];
		if (rxq == NULL)
			continue;

		rxq->err_stats.pkt_len_errors = 0;
		rxq->err_stats.l2_errors = 0;
	}

	/* Clear all the stats of a rxq in a loop to keep them synchronized */
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];
		if (rxq == NULL)
			continue;

		memset(&rxq->basic_stats, 0,
				sizeof(struct hns3_rx_basic_stats));

		/* This register is read-clear */
		(void)hns3_read_dev(rxq, HNS3_RING_RX_PKTNUM_RECORD_REG);
		rxq->err_stats.pkt_len_errors = 0;
		rxq->err_stats.l2_errors = 0;
	}

	/* Clear all the stats of a txq in a loop to keep them synchronized */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		txq = eth_dev->data->tx_queues[i];
		if (txq == NULL)
			continue;

		memset(&txq->basic_stats, 0,
				sizeof(struct hns3_tx_basic_stats));

		/* This register is read-clear */
		(void)hns3_read_dev(txq, HNS3_RING_TX_PKTNUM_RECORD_REG);
	}

	hns3_tqp_stats_clear(hw);

out:
	rte_spinlock_unlock(&hw->stats_lock);

	return ret;
}

static int
hns3_mac_stats_reset(struct hns3_hw *hw)
{
	struct hns3_mac_stats *mac_stats = &hw->mac_stats;
	int ret;

	/* Clear hardware MAC statistics by reading it. */
	ret = hns3_update_mac_stats(hw);
	if (ret) {
		hns3_err(hw, "Clear Mac stats fail : %d", ret);
		return ret;
	}

	memset(mac_stats, 0, sizeof(struct hns3_mac_stats));

	return 0;
}

static int
hns3_get_imissed_stats_num(struct hns3_adapter *hns)
{
#define NO_IMISSED_STATS_NUM   0
#define RPU_STATS_ITEM_NUM     1
	struct hns3_hw *hw = &hns->hw;

	if (hw->drop_stats_mode == HNS3_PKTS_DROP_STATS_MODE1 && hns->is_vf)
		return NO_IMISSED_STATS_NUM;

	if (hw->drop_stats_mode == HNS3_PKTS_DROP_STATS_MODE2 && !hns->is_vf)
		return HNS3_NUM_IMISSED_XSTATS;

	return RPU_STATS_ITEM_NUM;
}

/* This function calculates the number of xstats based on the current config */
static int
hns3_xstats_calc_num(struct rte_eth_dev *dev)
{
#define HNS3_PF_VF_RX_COMM_STATS_NUM	(HNS3_NUM_RX_BD_ERROR_XSTATS + \
					 HNS3_NUM_RXQ_DFX_XSTATS + \
					 HNS3_NUM_RX_QUEUE_STATS + \
					 HNS3_NUM_RXQ_BASIC_STATS)
#define HNS3_PF_VF_TX_COMM_STATS_NUM	(HNS3_NUM_TXQ_DFX_XSTATS + \
					 HNS3_NUM_TX_QUEUE_STATS + \
					 HNS3_NUM_TXQ_BASIC_STATS)

	struct hns3_adapter *hns = dev->data->dev_private;
	uint16_t nb_rx_q = dev->data->nb_rx_queues;
	uint16_t nb_tx_q = dev->data->nb_tx_queues;
	int rx_comm_stats_num = nb_rx_q * HNS3_PF_VF_RX_COMM_STATS_NUM;
	int tx_comm_stats_num = nb_tx_q * HNS3_PF_VF_TX_COMM_STATS_NUM;
	int stats_num;

	stats_num = rx_comm_stats_num + tx_comm_stats_num;
	stats_num += hns3_get_imissed_stats_num(hns);

	if (hns->is_vf)
		stats_num += HNS3_NUM_RESET_XSTATS;
	else
		stats_num += HNS3_FIX_NUM_STATS;

	return stats_num;
}

static void
hns3_queue_stats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		     int *count)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint32_t reg_offset;
	uint16_t i, j;

	/* Get rx queue stats */
	for (j = 0; j < dev->data->nb_rx_queues; j++) {
		for (i = 0; i < HNS3_NUM_RX_QUEUE_STATS; i++) {
			reg_offset = hns3_get_tqp_reg_offset(j);
			xstats[*count].value = hns3_read_dev(hw,
				reg_offset + hns3_rx_queue_strings[i].offset);
			xstats[*count].id = *count;
			(*count)++;
		}
	}

	/* Get tx queue stats */
	for (j = 0; j < dev->data->nb_tx_queues; j++) {
		for (i = 0; i < HNS3_NUM_TX_QUEUE_STATS; i++) {
			reg_offset = hns3_get_tqp_reg_offset(j);
			xstats[*count].value = hns3_read_dev(hw,
				reg_offset + hns3_tx_queue_strings[i].offset);
			xstats[*count].id = *count;
			(*count)++;
		}
	}
}

static void
hns3_rxq_dfx_stats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		       int *count)
{
	struct hns3_rx_dfx_stats *dfx_stats;
	struct hns3_rx_queue *rxq;
	uint16_t i, j;
	char *val;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = (struct hns3_rx_queue *)dev->data->rx_queues[i];
		if (rxq == NULL)
			continue;

		dfx_stats = &rxq->dfx_stats;
		for (j = 0; j < HNS3_NUM_RXQ_DFX_XSTATS; j++) {
			val = (char *)dfx_stats +
				hns3_rxq_dfx_stats_strings[j].offset;
			xstats[*count].value = *(uint64_t *)val;
			xstats[*count].id = *count;
			(*count)++;
		}
	}
}

static void
hns3_txq_dfx_stats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		       int *count)
{
	struct hns3_tx_dfx_stats *dfx_stats;
	struct hns3_tx_queue *txq;
	uint16_t i, j;
	char *val;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = (struct hns3_tx_queue *)dev->data->tx_queues[i];
		if (txq == NULL)
			continue;

		dfx_stats = &txq->dfx_stats;
		for (j = 0; j < HNS3_NUM_TXQ_DFX_XSTATS; j++) {
			val = (char *)dfx_stats +
				hns3_txq_dfx_stats_strings[j].offset;
			xstats[*count].value = *(uint64_t *)val;
			xstats[*count].id = *count;
			(*count)++;
		}
	}
}

static void
hns3_tqp_dfx_stats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		       int *count)
{
	hns3_rxq_dfx_stats_get(dev, xstats, count);
	hns3_txq_dfx_stats_get(dev, xstats, count);
}

static void
hns3_rxq_basic_stats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
			 int *count)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_tqp_stats *stats = &hw->tqp_stats;
	struct hns3_rx_basic_stats *rxq_stats;
	struct hns3_rx_queue *rxq;
	uint16_t i, j;
	char *val;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq == NULL)
			continue;

		hns3_rcb_rx_ring_stats_get(rxq, stats);
		rxq_stats = &rxq->basic_stats;
		rxq_stats->errors = rxq->err_stats.l2_errors +
					rxq->err_stats.pkt_len_errors;

		/*
		 * If HW statistics are reset by stats_reset, but a lot of
		 * residual packets exist in the hardware queue and these
		 * packets are error packets, flip overflow may occurred.
		 * So return 0 in this case.
		 */
		rxq_stats->packets =
			stats->rcb_rx_ring_pktnum[i] > rxq_stats->errors ?
			stats->rcb_rx_ring_pktnum[i] - rxq_stats->errors : 0;
		for (j = 0; j < HNS3_NUM_RXQ_BASIC_STATS; j++) {
			val = (char *)rxq_stats +
				hns3_rxq_basic_stats_strings[j].offset;
			xstats[*count].value = *(uint64_t *)val;
			xstats[*count].id = *count;
			(*count)++;
		}
	}
}

static void
hns3_txq_basic_stats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
			 int *count)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_tqp_stats *stats = &hw->tqp_stats;
	struct hns3_tx_basic_stats *txq_stats;
	struct hns3_tx_queue *txq;
	uint16_t i, j;
	char *val;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq == NULL)
			continue;

		hns3_rcb_tx_ring_stats_get(txq, stats);

		txq_stats = &txq->basic_stats;
		txq_stats->packets = stats->rcb_tx_ring_pktnum[i];

		for (j = 0; j < HNS3_NUM_TXQ_BASIC_STATS; j++) {
			val = (char *)txq_stats +
				hns3_txq_basic_stats_strings[j].offset;
			xstats[*count].value = *(uint64_t *)val;
			xstats[*count].id = *count;
			(*count)++;
		}
	}
}

static void
hns3_tqp_basic_stats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
			 int *count)
{
	hns3_rxq_basic_stats_get(dev, xstats, count);
	hns3_txq_basic_stats_get(dev, xstats, count);
}

static void
hns3_imissed_stats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
			  int *count)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rx_missed_stats *imissed_stats = &hw->imissed_stats;
	int imissed_stats_num;
	int cnt = *count;
	char *addr;
	uint16_t i;

	imissed_stats_num = hns3_get_imissed_stats_num(hns);

	for (i = 0; i < imissed_stats_num; i++) {
		addr = (char *)imissed_stats +
			hns3_imissed_stats_strings[i].offset;
		xstats[cnt].value = *(uint64_t *)addr;
		xstats[cnt].id = cnt;
		cnt++;
	}

	*count = cnt;
}

/*
 * Retrieve extended(tqp | Mac) statistics of an Ethernet device.
 * @param dev
 *   Pointer to Ethernet device.
 * @praram xstats
 *   A pointer to a table of structure of type *rte_eth_xstat*
 *   to be filled with device statistics ids and values.
 *   This parameter can be set to NULL if and only if n is 0.
 * @param n
 *   The size of the xstats array (number of elements).
 *   If lower than the required number of elements, the function returns the
 *   required number of elements.
 *   If equal to zero, the xstats parameter must be NULL, the function returns
 *   the required number of elements.
 * @return
 *   0 on fail, count(The size of the statistics elements) on success.
 */
int
hns3_dev_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
		    unsigned int n)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_mac_stats *mac_stats = &hw->mac_stats;
	struct hns3_reset_stats *reset_stats = &hw->reset.stats;
	struct hns3_rx_bd_errors_stats *rx_err_stats;
	struct hns3_rx_queue *rxq;
	uint16_t i, j;
	char *addr;
	int count;
	int ret;

	count = hns3_xstats_calc_num(dev);
	if ((int)n < count)
		return count;

	count = 0;

	rte_spinlock_lock(&hw->stats_lock);
	hns3_tqp_basic_stats_get(dev, xstats, &count);

	if (!hns->is_vf) {
		ret = hns3_update_mac_stats(hw);
		if (ret < 0) {
			hns3_err(hw, "Update Mac stats fail : %d", ret);
			rte_spinlock_unlock(&hw->stats_lock);
			return ret;
		}

		/* Get MAC stats from hw->hw_xstats.mac_stats struct */
		for (i = 0; i < HNS3_NUM_MAC_STATS; i++) {
			addr = (char *)mac_stats + hns3_mac_strings[i].offset;
			xstats[count].value = *(uint64_t *)addr;
			xstats[count].id = count;
			count++;
		}
	}

	ret = hns3_update_imissed_stats(hw, false);
	if (ret) {
		hns3_err(hw, "update imissed stats failed, ret = %d", ret);
		rte_spinlock_unlock(&hw->stats_lock);
		return ret;
	}

	hns3_imissed_stats_get(dev, xstats, &count);

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
			if (rxq) {
				rx_err_stats = &rxq->err_stats;
				addr = (char *)rx_err_stats +
					hns3_rx_bd_error_strings[i].offset;
				xstats[count].value = *(uint64_t *)addr;
				xstats[count].id = count;
				count++;
			}
		}
	}

	hns3_tqp_dfx_stats_get(dev, xstats, &count);
	hns3_queue_stats_get(dev, xstats, &count);
	rte_spinlock_unlock(&hw->stats_lock);

	return count;
}

static void
hns3_tqp_basic_stats_name_get(struct rte_eth_dev *dev,
			      struct rte_eth_xstat_name *xstats_names,
			      uint32_t *count)
{
	uint16_t i, j;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		for (j = 0; j < HNS3_NUM_RXQ_BASIC_STATS; j++) {
			snprintf(xstats_names[*count].name,
				 sizeof(xstats_names[*count].name),
				 "rx_q%u_%s", i,
				 hns3_rxq_basic_stats_strings[j].name);
			(*count)++;
		}
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		for (j = 0; j < HNS3_NUM_TXQ_BASIC_STATS; j++) {
			snprintf(xstats_names[*count].name,
				 sizeof(xstats_names[*count].name),
				 "tx_q%u_%s", i,
				 hns3_txq_basic_stats_strings[j].name);
			(*count)++;
		}
	}
}

static void
hns3_tqp_dfx_stats_name_get(struct rte_eth_dev *dev,
			    struct rte_eth_xstat_name *xstats_names,
			    uint32_t *count)
{
	uint16_t i, j;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		for (j = 0; j < HNS3_NUM_RXQ_DFX_XSTATS; j++) {
			snprintf(xstats_names[*count].name,
				 sizeof(xstats_names[*count].name),
				 "rx_q%u_%s", i,
				 hns3_rxq_dfx_stats_strings[j].name);
			(*count)++;
		}
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		for (j = 0; j < HNS3_NUM_TXQ_DFX_XSTATS; j++) {
			snprintf(xstats_names[*count].name,
				 sizeof(xstats_names[*count].name),
				 "tx_q%u_%s", i,
				 hns3_txq_dfx_stats_strings[j].name);
			(*count)++;
		}
	}
}

static void
hns3_imissed_stats_name_get(struct rte_eth_dev *dev,
			    struct rte_eth_xstat_name *xstats_names,
			    uint32_t *count)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	uint32_t cnt = *count;
	int imissed_stats_num;
	uint16_t i;

	imissed_stats_num = hns3_get_imissed_stats_num(hns);

	for (i = 0; i < imissed_stats_num; i++) {
		snprintf(xstats_names[cnt].name,
			 sizeof(xstats_names[cnt].name),
			 "%s", hns3_imissed_stats_strings[i].name);
		cnt++;
	}

	*count = cnt;
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

	hns3_tqp_basic_stats_name_get(dev, xstats_names, &count);

	/* Note: size limited checked in rte_eth_xstats_get_names() */
	if (!hns->is_vf) {
		/* Get MAC name from hw->hw_xstats.mac_stats struct */
		for (i = 0; i < HNS3_NUM_MAC_STATS; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "%s", hns3_mac_strings[i].name);
			count++;
		}
	}

	hns3_imissed_stats_name_get(dev, xstats_names, &count);

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
				 "rx_q%u_%s", j,
				 hns3_rx_bd_error_strings[i].name);
			count++;
		}
	}

	hns3_tqp_dfx_stats_name_get(dev, xstats_names, &count);

	for (j = 0; j < dev->data->nb_rx_queues; j++) {
		for (i = 0; i < HNS3_NUM_RX_QUEUE_STATS; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "rx_q%u_%s", j, hns3_rx_queue_strings[i].name);
			count++;
		}
	}

	for (j = 0; j < dev->data->nb_tx_queues; j++) {
		for (i = 0; i < HNS3_NUM_TX_QUEUE_STATS; i++) {
			snprintf(xstats_names[count].name,
				 sizeof(xstats_names[count].name),
				 "tx_q%u_%s", j, hns3_tx_queue_strings[i].name);
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
 *   all available statistics.
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

	if (ids == NULL && values == NULL)
		return cnt_stats;

	if (ids == NULL)
		if (size < cnt_stats)
			return cnt_stats;

	len = cnt_stats * sizeof(struct rte_eth_xstat);
	values_copy = rte_zmalloc("hns3_xstats_values", len, 0);
	if (values_copy == NULL) {
		hns3_err(hw, "Failed to allocate 0x%" PRIx64 " bytes needed to store statistics values",
			 len);
		return -ENOMEM;
	}

	count_value = hns3_dev_xstats_get(dev, values_copy, cnt_stats);
	if (count_value != cnt_stats) {
		rte_free(values_copy);
		return -EINVAL;
	}

	if (ids == NULL && values != NULL) {
		for (i = 0; i < cnt_stats; i++)
			memcpy(&values[i], &values_copy[i].value,
			       sizeof(values[i]));

		rte_free(values_copy);
		return cnt_stats;
	}

	for (i = 0; i < size; i++) {
		if (ids[i] >= cnt_stats) {
			hns3_err(hw, "ids[%u] (%" PRIu64 ") is invalid, should < %u",
				 i, ids[i], cnt_stats);
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
 * @param ids
 *   IDs array given by app to retrieve specific statistics
 * @param xstats_names
 *   An rte_eth_xstat_name array of at least *size* elements to
 *   be filled. If set to NULL, the function returns the required number
 *   of elements.
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
				const uint64_t *ids,
				struct rte_eth_xstat_name *xstats_names,
				uint32_t size)
{
	const uint32_t cnt_stats = hns3_xstats_calc_num(dev);
	struct hns3_adapter *hns = dev->data->dev_private;
	struct rte_eth_xstat_name *names_copy;
	struct hns3_hw *hw = &hns->hw;
	uint64_t len;
	uint32_t i;

	if (xstats_names == NULL)
		return cnt_stats;

	if (ids == NULL) {
		if (size < cnt_stats)
			return cnt_stats;

		return hns3_dev_xstats_get_names(dev, xstats_names, cnt_stats);
	}

	len = cnt_stats * sizeof(struct rte_eth_xstat_name);
	names_copy = rte_zmalloc("hns3_xstats_names", len, 0);
	if (names_copy == NULL) {
		hns3_err(hw, "Failed to allocate 0x%" PRIx64 " bytes needed to store statistics names",
			 len);
		return -ENOMEM;
	}

	(void)hns3_dev_xstats_get_names(dev, names_copy, cnt_stats);

	for (i = 0; i < size; i++) {
		if (ids[i] >= cnt_stats) {
			hns3_err(hw, "ids[%u] (%" PRIu64 ") is invalid, should < %u",
				 i, ids[i], cnt_stats);
			rte_free(names_copy);
			return -EINVAL;
		}
		snprintf(xstats_names[i].name, sizeof(xstats_names[i].name),
			 "%s", names_copy[ids[i]].name);
	}

	rte_free(names_copy);
	return size;
}

static void
hns3_tqp_dfx_stats_clear(struct rte_eth_dev *dev)
{
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint16_t i;

	/* Clear Rx dfx stats */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq)
			memset(&rxq->dfx_stats, 0,
			       sizeof(struct hns3_rx_dfx_stats));
	}

	/* Clear Tx dfx stats */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq)
			memset(&txq->dfx_stats, 0,
			       sizeof(struct hns3_tx_dfx_stats));
	}
}

int
hns3_dev_xstats_reset(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	/* Clear tqp stats */
	ret = hns3_stats_reset(dev);
	if (ret)
		return ret;

	rte_spinlock_lock(&hw->stats_lock);
	hns3_tqp_dfx_stats_clear(dev);

	/* Clear reset stats */
	memset(&hns->hw.reset.stats, 0, sizeof(struct hns3_reset_stats));

	if (hns->is_vf)
		goto out;

	ret = hns3_mac_stats_reset(hw);

out:
	rte_spinlock_unlock(&hw->stats_lock);

	return ret;
}

static int
hns3_tqp_stats_init(struct hns3_hw *hw)
{
	struct hns3_tqp_stats *tqp_stats = &hw->tqp_stats;

	tqp_stats->rcb_rx_ring_pktnum = rte_zmalloc("hns3_rx_ring_pkt_num",
					sizeof(uint64_t) * hw->tqps_num, 0);
	if (tqp_stats->rcb_rx_ring_pktnum == NULL) {
		hns3_err(hw, "failed to allocate rx_ring pkt_num.");
		return -ENOMEM;
	}

	tqp_stats->rcb_tx_ring_pktnum = rte_zmalloc("hns3_tx_ring_pkt_num",
					sizeof(uint64_t) * hw->tqps_num, 0);
	if (tqp_stats->rcb_tx_ring_pktnum == NULL) {
		hns3_err(hw, "failed to allocate tx_ring pkt_num.");
		rte_free(tqp_stats->rcb_rx_ring_pktnum);
		tqp_stats->rcb_rx_ring_pktnum = NULL;
		return -ENOMEM;
	}

	return 0;
}

static void
hns3_tqp_stats_uninit(struct hns3_hw *hw)
{
	struct hns3_tqp_stats *tqp_stats = &hw->tqp_stats;

	rte_free(tqp_stats->rcb_rx_ring_pktnum);
	tqp_stats->rcb_rx_ring_pktnum = NULL;
	rte_free(tqp_stats->rcb_tx_ring_pktnum);
	tqp_stats->rcb_tx_ring_pktnum = NULL;
}

static void
hns3_tqp_stats_clear(struct hns3_hw *hw)
{
	struct hns3_tqp_stats *stats = &hw->tqp_stats;

	stats->rcb_rx_ring_pktnum_rcd = 0;
	stats->rcb_tx_ring_pktnum_rcd = 0;
	memset(stats->rcb_rx_ring_pktnum, 0, sizeof(uint64_t) * hw->tqps_num);
	memset(stats->rcb_tx_ring_pktnum, 0, sizeof(uint64_t) * hw->tqps_num);
}

int
hns3_stats_init(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	int ret;

	rte_spinlock_init(&hw->stats_lock);
	/* Hardware statistics of imissed registers cleared. */
	ret = hns3_update_imissed_stats(hw, true);
	if (ret) {
		hns3_err(hw, "clear imissed stats failed, ret = %d", ret);
		return ret;
	}

	if (!hns->is_vf)
		hns3_mac_stats_reset(hw);

	return hns3_tqp_stats_init(hw);
}

void
hns3_stats_uninit(struct hns3_hw *hw)
{
	hns3_tqp_stats_uninit(hw);
}

static void
hns3_update_queues_stats(struct hns3_hw *hw)
{
	struct rte_eth_dev_data *data = hw->data;
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint16_t i;

	for (i = 0; i < data->nb_rx_queues; i++) {
		rxq = data->rx_queues[i];
		if (rxq != NULL)
			hns3_rcb_rx_ring_stats_get(rxq, &hw->tqp_stats);
	}

	for (i = 0; i < data->nb_tx_queues; i++) {
		txq = data->tx_queues[i];
		if (txq != NULL)
			hns3_rcb_tx_ring_stats_get(txq, &hw->tqp_stats);
	}
}

/*
 * Some hardware statistics registers are not 64-bit. If hardware statistics are
 * not obtained for a long time, these statistics may be reversed. This function
 * is used to update these hardware statistics in periodic task.
 */
void
hns3_update_hw_stats(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);

	rte_spinlock_lock(&hw->stats_lock);
	if (!hns->is_vf)
		hns3_update_mac_stats(hw);

	hns3_update_queues_stats(hw);
	rte_spinlock_unlock(&hw->stats_lock);
}
