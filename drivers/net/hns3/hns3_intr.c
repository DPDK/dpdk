/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <rte_alarm.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_io.h>
#include <rte_malloc.h>

#include "hns3_common.h"
#include "hns3_logs.h"
#include "hns3_regs.h"
#include "hns3_rxtx.h"
#include "hns3_intr.h"

#define SWITCH_CONTEXT_US	10

static const char *reset_string[HNS3_MAX_RESET] = {
	"flr", "vf_func", "vf_pf_func", "vf_full", "vf_global",
	"pf_func", "global", "IMP", "none",
};

static const struct hns3_hw_error mac_afifo_tnl_int[] = {
	{
		.int_msk = BIT(0),
		.msg = "egu_cge_afifo_ecc_1bit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(1),
		.msg = "egu_cge_afifo_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "egu_lge_afifo_ecc_1bit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "egu_lge_afifo_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(4),
		.msg = "cge_igu_afifo_ecc_1bit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "cge_igu_afifo_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(6),
		.msg = "lge_igu_afifo_ecc_1bit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "lge_igu_afifo_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(8),
		.msg = "cge_igu_afifo_overflow_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(9),
		.msg = "lge_igu_afifo_overflow_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(10),
		.msg = "egu_cge_afifo_underrun_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(11),
		.msg = "egu_lge_afifo_underrun_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(12),
		.msg = "egu_ge_afifo_underrun_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(13),
		.msg = "ge_igu_afifo_overflow_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ppu_mpf_abnormal_int_st1[] = {
	{
		.int_msk = 0xFFFFFFFF,
		.msg = "rpu_rx_pkt_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ppu_mpf_abnormal_int_st2_ras[] = {
	{
		.int_msk = BIT(13),
		.msg = "rpu_rx_pkt_bit32_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(14),
		.msg = "rpu_rx_pkt_bit33_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(15),
		.msg = "rpu_rx_pkt_bit34_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(16),
		.msg = "rpu_rx_pkt_bit35_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(17),
		.msg = "rcb_tx_ring_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(18),
		.msg = "rcb_rx_ring_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(19),
		.msg = "rcb_tx_fbd_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(20),
		.msg = "rcb_rx_ebd_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(21),
		.msg = "rcb_tso_info_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(22),
		.msg = "rcb_tx_int_info_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(23),
		.msg = "rcb_rx_int_info_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(24),
		.msg = "tpu_tx_pkt_0_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(25),
		.msg = "tpu_tx_pkt_1_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(26),
		.msg = "rd_bus_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(27),
		.msg = "wr_bus_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(30),
		.msg = "ooo_ecc_err_detect",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(31),
		.msg = "ooo_ecc_err_multpl",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ppu_mpf_abnormal_int_st2_msix[] = {
	{
		.int_msk = BIT(29),
		.msg = "rx_q_search_miss",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ssu_port_based_pf_int[] = {
	{
		.int_msk = BIT(0),
		.msg = "roc_pkt_without_key_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(9),
		.msg = "low_water_line_err_port",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ppp_pf_abnormal_int[] = {
	{
		.int_msk = BIT(0),
		.msg = "tx_vlan_tag_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(1),
		.msg = "rss_list_tc_unassigned_queue_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ppu_pf_abnormal_int_ras[] = {
	{
		.int_msk = BIT(3),
		.msg = "tx_rd_fbd_poison",
		.reset_level = HNS3_FUNC_RESET
	}, {
		.int_msk = BIT(4),
		.msg = "rx_rd_ebd_poison",
		.reset_level = HNS3_FUNC_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ppu_pf_abnormal_int_msix[] = {
	{
		.int_msk = BIT(0),
		.msg = "over_8bd_no_fe",
		.reset_level = HNS3_FUNC_RESET
	}, {
		.int_msk = BIT(1),
		.msg = "tso_mss_cmp_min_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "tso_mss_cmp_max_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "buf_wait_timeout",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error imp_tcm_ecc_int[] = {
	{
		.int_msk = BIT(1),
		.msg = "imp_itcm0_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "imp_itcm1_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "imp_itcm2_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "imp_itcm3_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(9),
		.msg = "imp_dtcm0_mem0_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(11),
		.msg = "imp_dtcm0_mem1_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(13),
		.msg = "imp_dtcm1_mem0_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(15),
		.msg = "imp_dtcm1_mem1_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(17),
		.msg = "imp_itcm4_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error cmdq_mem_ecc_int[] = {
	{
		.int_msk = BIT(1),
		.msg = "cmdq_nic_rx_depth_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "cmdq_nic_tx_depth_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "cmdq_nic_rx_tail_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "cmdq_nic_tx_tail_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(9),
		.msg = "cmdq_nic_rx_head_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(11),
		.msg = "cmdq_nic_tx_head_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(13),
		.msg = "cmdq_nic_rx_addr_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(15),
		.msg = "cmdq_nic_tx_addr_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error tqp_int_ecc_int[] = {
	{
		.int_msk = BIT(6),
		.msg = "tqp_int_cfg_even_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "tqp_int_cfg_odd_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(8),
		.msg = "tqp_int_ctrl_even_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(9),
		.msg = "tqp_int_ctrl_odd_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(10),
		.msg = "tx_queue_scan_int_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(11),
		.msg = "rx_queue_scan_int_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error imp_rd_poison_int[] = {
	{
		.int_msk = BIT(0),
		.msg = "imp_rd_poison_int",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

#define HNS3_SSU_MEM_ECC_ERR(x) \
{ \
	.int_msk = BIT(x), \
	.msg = "ssu_mem" #x "_ecc_mbit_err", \
	.reset_level = HNS3_GLOBAL_RESET \
}

static const struct hns3_hw_error ssu_ecc_multi_bit_int_0[] = {
	HNS3_SSU_MEM_ECC_ERR(0),
	HNS3_SSU_MEM_ECC_ERR(1),
	HNS3_SSU_MEM_ECC_ERR(2),
	HNS3_SSU_MEM_ECC_ERR(3),
	HNS3_SSU_MEM_ECC_ERR(4),
	HNS3_SSU_MEM_ECC_ERR(5),
	HNS3_SSU_MEM_ECC_ERR(6),
	HNS3_SSU_MEM_ECC_ERR(7),
	HNS3_SSU_MEM_ECC_ERR(8),
	HNS3_SSU_MEM_ECC_ERR(9),
	HNS3_SSU_MEM_ECC_ERR(10),
	HNS3_SSU_MEM_ECC_ERR(11),
	HNS3_SSU_MEM_ECC_ERR(12),
	HNS3_SSU_MEM_ECC_ERR(13),
	HNS3_SSU_MEM_ECC_ERR(14),
	HNS3_SSU_MEM_ECC_ERR(15),
	HNS3_SSU_MEM_ECC_ERR(16),
	HNS3_SSU_MEM_ECC_ERR(17),
	HNS3_SSU_MEM_ECC_ERR(18),
	HNS3_SSU_MEM_ECC_ERR(19),
	HNS3_SSU_MEM_ECC_ERR(20),
	HNS3_SSU_MEM_ECC_ERR(21),
	HNS3_SSU_MEM_ECC_ERR(22),
	HNS3_SSU_MEM_ECC_ERR(23),
	HNS3_SSU_MEM_ECC_ERR(24),
	HNS3_SSU_MEM_ECC_ERR(25),
	HNS3_SSU_MEM_ECC_ERR(26),
	HNS3_SSU_MEM_ECC_ERR(27),
	HNS3_SSU_MEM_ECC_ERR(28),
	HNS3_SSU_MEM_ECC_ERR(29),
	HNS3_SSU_MEM_ECC_ERR(30),
	HNS3_SSU_MEM_ECC_ERR(31),
	{ .int_msk = 0,
	  .msg = NULL,
	  .reset_level = HNS3_NONE_RESET}
};

static const struct hns3_hw_error ssu_ecc_multi_bit_int_1[] = {
	{
		.int_msk = BIT(0),
		.msg = "ssu_mem32_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ssu_common_ecc_int[] = {
	{
		.int_msk = BIT(0),
		.msg = "buf_sum_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(1),
		.msg = "ppp_mb_num_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "ppp_mbid_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "ppp_rlt_mac_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(4),
		.msg = "ppp_rlt_host_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "cks_edit_position_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(6),
		.msg = "cks_edit_condition_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "vlan_edit_condition_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(8),
		.msg = "vlan_num_ot_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(9),
		.msg = "vlan_num_in_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error igu_int[] = {
	{
		.int_msk = BIT(0),
		.msg = "igu_rx_buf0_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "igu_rx_buf1_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error msix_ecc_int[] = {
	{
		.int_msk = BIT(1),
		.msg = "msix_nic_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ppp_mpf_abnormal_int_st1[] = {
	{
		.int_msk = BIT(0),
		.msg = "vf_vlan_ad_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(1),
		.msg = "umv_mcast_group_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "umv_key_mem0_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "umv_key_mem1_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(4),
		.msg = "umv_key_mem2_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "umv_key_mem3_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(6),
		.msg = "umv_ad_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "rss_tc_mode_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(8),
		.msg = "rss_idt_mem0_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(9),
		.msg = "rss_idt_mem1_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(10),
		.msg = "rss_idt_mem2_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(11),
		.msg = "rss_idt_mem3_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(12),
		.msg = "rss_idt_mem4_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(13),
		.msg = "rss_idt_mem5_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(14),
		.msg = "rss_idt_mem6_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(15),
		.msg = "rss_idt_mem7_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(16),
		.msg = "rss_idt_mem8_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(17),
		.msg = "rss_idt_mem9_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(18),
		.msg = "rss_idt_mem10_ecc_m1bit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(19),
		.msg = "rss_idt_mem11_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(20),
		.msg = "rss_idt_mem12_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(21),
		.msg = "rss_idt_mem13_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(22),
		.msg = "rss_idt_mem14_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(23),
		.msg = "rss_idt_mem15_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(24),
		.msg = "port_vlan_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(25),
		.msg = "mcast_linear_table_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(26),
		.msg = "mcast_result_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(27),
		.msg = "flow_director_ad_mem0_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(28),
		.msg = "flow_director_ad_mem1_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(29),
		.msg = "rx_vlan_tag_memory_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(30),
		.msg = "Tx_UP_mapping_config_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ppp_mpf_abnormal_int_st3[] = {
	{
		.int_msk = BIT(0),
		.msg = "hfs_fifo_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(1),
		.msg = "rslt_descr_fifo_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "tx_vlan_tag_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "FD_CN0_memory_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(4),
		.msg = "FD_CN1_memory_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "GRO_AD_memory_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ppu_mpf_abnormal_int_st3[] = {
	{
		.int_msk = BIT(4),
		.msg = "gro_bd_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "gro_context_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(6),
		.msg = "rx_stash_cfg_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "axi_rd_fbd_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error tm_sch_int[] = {
	{
		.int_msk = BIT(1),
		.msg = "tm_sch_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "tm_sch_port_shap_sub_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "tm_sch_port_shap_sub_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(4),
		.msg = "tm_sch_pg_pshap_sub_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "tm_sch_pg_pshap_sub_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(6),
		.msg = "tm_sch_pg_cshap_sub_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "tm_sch_pg_cshap_sub_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(8),
		.msg = "tm_sch_pri_pshap_sub_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(9),
		.msg = "tm_sch_pri_pshap_sub_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(10),
		.msg = "tm_sch_pri_cshap_sub_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(11),
		.msg = "tm_sch_pri_cshap_sub_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(12),
		.msg = "tm_sch_port_shap_offset_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(13),
		.msg = "tm_sch_port_shap_offset_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(14),
		.msg = "tm_sch_pg_pshap_offset_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(15),
		.msg = "tm_sch_pg_pshap_offset_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(16),
		.msg = "tm_sch_pg_cshap_offset_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(17),
		.msg = "tm_sch_pg_cshap_offset_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(18),
		.msg = "tm_sch_pri_pshap_offset_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(19),
		.msg = "tm_sch_pri_pshap_offset_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(20),
		.msg = "tm_sch_pri_cshap_offset_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(21),
		.msg = "tm_sch_pri_cshap_offset_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(22),
		.msg = "tm_sch_rq_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(23),
		.msg = "tm_sch_rq_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(24),
		.msg = "tm_sch_nq_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(25),
		.msg = "tm_sch_nq_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(26),
		.msg = "tm_sch_roce_up_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(27),
		.msg = "tm_sch_roce_up_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(28),
		.msg = "tm_sch_rcb_byte_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(29),
		.msg = "tm_sch_rcb_byte_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(30),
		.msg = "tm_sch_ssu_byte_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(31),
		.msg = "tm_sch_ssu_byte_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error qcn_fifo_int[] = {
	{
		.int_msk = BIT(0),
		.msg = "qcn_shap_gp0_sch_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(1),
		.msg = "qcn_shap_gp0_sch_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "qcn_shap_gp1_sch_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "qcn_shap_gp1_sch_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(4),
		.msg = "qcn_shap_gp2_sch_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "qcn_shap_gp2_sch_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(6),
		.msg = "qcn_shap_gp3_sch_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "qcn_shap_gp3_sch_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(8),
		.msg = "qcn_shap_gp0_offset_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(9),
		.msg = "qcn_shap_gp0_offset_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(10),
		.msg = "qcn_shap_gp1_offset_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(11),
		.msg = "qcn_shap_gp1_offset_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(12),
		.msg = "qcn_shap_gp2_offset_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(13),
		.msg = "qcn_shap_gp2_offset_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(14),
		.msg = "qcn_shap_gp3_offset_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(15),
		.msg = "qcn_shap_gp3_offset_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(16),
		.msg = "qcn_byte_info_fifo_rd_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(17),
		.msg = "qcn_byte_info_fifo_wr_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error qcn_ecc_int[] = {
	{
		.int_msk = BIT(1),
		.msg = "qcn_byte_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "qcn_time_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "qcn_fb_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "qcn_link_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(9),
		.msg = "qcn_rate_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(11),
		.msg = "qcn_tmplt_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(13),
		.msg = "qcn_shap_cfg_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(15),
		.msg = "qcn_gp0_barrel_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(17),
		.msg = "qcn_gp1_barrel_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(19),
		.msg = "qcn_gp2_barrel_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(21),
		.msg = "qcn_gp3_barral_mem_ecc_mbit_err",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ncsi_ecc_int[] = {
	{
		.int_msk = BIT(1),
		.msg = "ncsi_tx_ecc_mbit_err",
		.reset_level = HNS3_NONE_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ssu_fifo_overflow_int[] = {
	{
		.int_msk = BIT(0),
		.msg = "ig_mac_inf_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(1),
		.msg = "ig_host_inf_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "ig_roc_buf_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "ig_host_data_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(4),
		.msg = "ig_host_key_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "tx_qcn_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(6),
		.msg = "rx_qcn_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "tx_pf_rd_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(8),
		.msg = "rx_pf_rd_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(9),
		.msg = "qm_eof_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(10),
		.msg = "mb_rlt_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(11),
		.msg = "dup_uncopy_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(12),
		.msg = "dup_cnt_rd_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(13),
		.msg = "dup_cnt_drop_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(14),
		.msg = "dup_cnt_wrb_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(15),
		.msg = "host_cmd_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(16),
		.msg = "mac_cmd_fifo_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(17),
		.msg = "host_cmd_bitmap_empty_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(18),
		.msg = "mac_cmd_bitmap_empty_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(19),
		.msg = "dup_bitmap_empty_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(20),
		.msg = "out_queue_bitmap_empty_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(21),
		.msg = "bank2_bitmap_empty_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(22),
		.msg = "bank1_bitmap_empty_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(23),
		.msg = "bank0_bitmap_empty_int",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ssu_ets_tcg_int[] = {
	{
		.int_msk = BIT(0),
		.msg = "ets_rd_int_rx_tcg",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(1),
		.msg = "ets_wr_int_rx_tcg",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "ets_rd_int_tx_tcg",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "ets_wr_int_tx_tcg",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error igu_egu_tnl_int[] = {
	{
		.int_msk = BIT(0),
		.msg = "rx_buf_overflow",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(1),
		.msg = "rx_stp_fifo_overflow",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "rx_stp_fifo_underflow",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "tx_buf_overflow",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(4),
		.msg = "tx_buf_underrun",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "rx_stp_buf_overflow",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error ssu_port_based_err_int[] = {
	{
		.int_msk = BIT(0),
		.msg = "roc_pkt_without_key_port",
		.reset_level = HNS3_FUNC_RESET
	}, {
		.int_msk = BIT(1),
		.msg = "tpu_pkt_without_key_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(2),
		.msg = "igu_pkt_without_key_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(3),
		.msg = "roc_eof_mis_match_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(4),
		.msg = "tpu_eof_mis_match_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(5),
		.msg = "igu_eof_mis_match_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(6),
		.msg = "roc_sof_mis_match_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(7),
		.msg = "tpu_sof_mis_match_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(8),
		.msg = "igu_sof_mis_match_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(11),
		.msg = "ets_rd_int_rx_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(12),
		.msg = "ets_wr_int_rx_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(13),
		.msg = "ets_rd_int_tx_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = BIT(14),
		.msg = "ets_wr_int_tx_port",
		.reset_level = HNS3_GLOBAL_RESET
	}, {
		.int_msk = 0,
		.msg = NULL,
		.reset_level = HNS3_NONE_RESET
	}
};

static const struct hns3_hw_error_desc mpf_ras_err_tbl[] = {
	{
		.desc_offset = 0,
		.data_offset = 0,
		.msg = "IMP_TCM_ECC_INT_STS",
		.hw_err = imp_tcm_ecc_int
	}, {
		.desc_offset = 0,
		.data_offset = 1,
		.msg = "CMDQ_MEM_ECC_INT_STS",
		.hw_err = cmdq_mem_ecc_int
	}, {
		.desc_offset = 0,
		.data_offset = 2,
		.msg = "IMP_RD_POISON_INT_STS",
		.hw_err = imp_rd_poison_int
	}, {
		.desc_offset = 0,
		.data_offset = 3,
		.msg = "TQP_INT_ECC_INT_STS",
		.hw_err = tqp_int_ecc_int
	}, {
		.desc_offset = 0,
		.data_offset = 4,
		.msg = "MSIX_ECC_INT_STS",
		.hw_err = msix_ecc_int
	}, {
		.desc_offset = 2,
		.data_offset = 2,
		.msg = "SSU_ECC_MULTI_BIT_INT_0",
		.hw_err = ssu_ecc_multi_bit_int_0
	}, {
		.desc_offset = 2,
		.data_offset = 3,
		.msg = "SSU_ECC_MULTI_BIT_INT_1",
		.hw_err = ssu_ecc_multi_bit_int_1
	}, {
		.desc_offset = 2,
		.data_offset = 4,
		.msg = "SSU_COMMON_ERR_INT",
		.hw_err = ssu_common_ecc_int
	}, {
		.desc_offset = 3,
		.data_offset = 0,
		.msg = "IGU_INT_STS",
		.hw_err = igu_int
	}, {
		.desc_offset = 4,
		.data_offset = 1,
		.msg = "PPP_MPF_ABNORMAL_INT_ST1",
		.hw_err = ppp_mpf_abnormal_int_st1
	}, {
		.desc_offset = 4,
		.data_offset = 3,
		.msg = "PPP_MPF_ABNORMAL_INT_ST3",
		.hw_err = ppp_mpf_abnormal_int_st3
	}, {
		.desc_offset = 5,
		.data_offset = 1,
		.msg = "PPU_MPF_ABNORMAL_INT_ST1",
		.hw_err = ppu_mpf_abnormal_int_st1
	}, {
		.desc_offset = 5,
		.data_offset = 2,
		.msg = "PPU_MPF_ABNORMAL_INT_ST2_RAS",
		.hw_err = ppu_mpf_abnormal_int_st2_ras
	}, {
		.desc_offset = 5,
		.data_offset = 3,
		.msg = "PPU_MPF_ABNORMAL_INT_ST3",
		.hw_err = ppu_mpf_abnormal_int_st3
	}, {
		.desc_offset = 6,
		.data_offset = 0,
		.msg = "TM_SCH_RINT",
		.hw_err = tm_sch_int
	}, {
		.desc_offset = 7,
		.data_offset = 0,
		.msg = "QCN_FIFO_RINT",
		.hw_err = qcn_fifo_int
	}, {
		.desc_offset = 7,
		.data_offset = 1,
		.msg = "QCN_ECC_RINT",
		.hw_err = qcn_ecc_int
	}, {
		.desc_offset = 9,
		.data_offset = 0,
		.msg = "NCSI_ECC_INT_RPT",
		.hw_err = ncsi_ecc_int
	}, {
		.desc_offset = 0,
		.data_offset = 0,
		.msg = NULL,
		.hw_err = NULL
	}
};

static const struct hns3_hw_error_desc pf_ras_err_tbl[] = {
	{
		.desc_offset = 0,
		.data_offset = 0,
		.msg = "SSU_PORT_BASED_ERR_INT_RAS",
		.hw_err = ssu_port_based_err_int
	}, {
		.desc_offset = 0,
		.data_offset = 1,
		.msg = "SSU_FIFO_OVERFLOW_INT",
		.hw_err = ssu_fifo_overflow_int
	}, {
		.desc_offset = 0,
		.data_offset = 2,
		.msg = "SSU_ETS_TCG_INT",
		.hw_err = ssu_ets_tcg_int
	}, {
		.desc_offset = 1,
		.data_offset = 0,
		.msg = "IGU_EGU_TNL_INT_STS",
		.hw_err = igu_egu_tnl_int
	}, {
		.desc_offset = 3,
		.data_offset = 0,
		.msg = "PPU_PF_ABNORMAL_INT_ST_RAS",
		.hw_err = ppu_pf_abnormal_int_ras
	}, {
		.desc_offset = 0,
		.data_offset = 0,
		.msg = NULL,
		.hw_err = NULL
	}
};

static const struct hns3_hw_error_desc mpf_msix_err_tbl[] = {
	{
		.desc_offset = 1,
		.data_offset = 0,
		.msg = "MAC_AFIFO_TNL_INT_R",
		.hw_err = mac_afifo_tnl_int
	}, {
		.desc_offset = 5,
		.data_offset = 2,
		.msg = "PPU_MPF_ABNORMAL_INT_ST2_MSIX",
		.hw_err = ppu_mpf_abnormal_int_st2_msix
	}, {
		.desc_offset = 0,
		.data_offset = 0,
		.msg = NULL,
		.hw_err = NULL
	}
};

static const struct hns3_hw_error_desc pf_msix_err_tbl[] = {
	{
		.desc_offset = 0,
		.data_offset = 0,
		.msg = "SSU_PORT_BASED_ERR_INT_MSIX",
		.hw_err = ssu_port_based_pf_int
	}, {
		.desc_offset = 2,
		.data_offset = 0,
		.msg = "PPP_PF_ABNORMAL_INT_ST0",
		.hw_err = ppp_pf_abnormal_int
	}, {
		.desc_offset = 3,
		.data_offset = 0,
		.msg = "PPU_PF_ABNORMAL_INT_ST_MSIX",
		.hw_err = ppu_pf_abnormal_int_msix
	}, {
		.desc_offset = 0,
		.data_offset = 0,
		.msg = NULL,
		.hw_err = NULL
	}
};

enum hns3_hw_err_report_type {
	MPF_MSIX_ERR,
	PF_MSIX_ERR,
	MPF_RAS_ERR,
	PF_RAS_ERR,
};

static const struct hns3_hw_mod_name hns3_hw_module_name[] = {
	{
		.module_name = MODULE_NONE,
		.msg = "MODULE_NONE"
	}, {
		.module_name = MODULE_BIOS_COMMON,
		.msg = "MODULE_BIOS_COMMON"
	}, {
		.module_name = MODULE_GE,
		.msg = "MODULE_GE"
	}, {
		.module_name = MODULE_IGU_EGU,
		.msg = "MODULE_IGU_EGU"
	}, {
		.module_name = MODULE_LGE,
		.msg = "MODULE_LGE"
	}, {
		.module_name = MODULE_NCSI,
		.msg = "MODULE_NCSI"
	}, {
		.module_name = MODULE_PPP,
		.msg = "MODULE_PPP"
	}, {
		.module_name = MODULE_QCN,
		.msg = "MODULE_QCN"
	}, {
		.module_name = MODULE_RCB_RX,
		.msg = "MODULE_RCB_RX"
	}, {
		.module_name = MODULE_RTC,
		.msg = "MODULE_RTC"
	}, {
		.module_name = MODULE_SSU,
		.msg = "MODULE_SSU"
	}, {
		.module_name = MODULE_TM,
		.msg = "MODULE_TM"
	}, {
		.module_name = MODULE_RCB_TX,
		.msg = "MODULE_RCB_TX"
	}, {
		.module_name = MODULE_TXDMA,
		.msg = "MODULE_TXDMA"
	}, {
		.module_name = MODULE_MASTER,
		.msg = "MODULE_MASTER"
	}, {
		.module_name = MODULE_ROH_MAC,
		.msg = "MODULE_ROH_MAC"
	}
};

static const struct hns3_hw_err_type hns3_hw_error_type[] = {
	{
		.error_type = NONE_ERROR,
		.msg = "none_error"
	}, {
		.error_type = FIFO_ERROR,
		.msg = "fifo_error"
	}, {
		.error_type = MEMORY_ERROR,
		.msg = "memory_error"
	}, {
		.error_type = POISION_ERROR,
		.msg = "poision_error"
	}, {
		.error_type = MSIX_ECC_ERROR,
		.msg = "msix_ecc_error"
	}, {
		.error_type = TQP_INT_ECC_ERROR,
		.msg = "tqp_int_ecc_error"
	}, {
		.error_type = PF_ABNORMAL_INT_ERROR,
		.msg = "pf_abnormal_int_error"
	}, {
		.error_type = MPF_ABNORMAL_INT_ERROR,
		.msg = "mpf_abnormal_int_error"
	}, {
		.error_type = COMMON_ERROR,
		.msg = "common_error"
	}, {
		.error_type = PORT_ERROR,
		.msg = "port_error"
	}, {
		.error_type = ETS_ERROR,
		.msg = "ets_error"
	}, {
		.error_type = NCSI_ERROR,
		.msg = "ncsi_error"
	}, {
		.error_type = GLB_ERROR,
		.msg = "glb_error"
	}
};

static void
hns3_report_reset_begin(struct hns3_hw *hw)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hw->data->port_id];
	rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_ERR_RECOVERING, NULL);
}

static void
hns3_report_reset_success(struct hns3_hw *hw)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hw->data->port_id];
	rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_RECOVERY_SUCCESS, NULL);
}

static void
hns3_report_reset_failed(struct hns3_hw *hw)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hw->data->port_id];
	rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_RECOVERY_FAILED, NULL);
}

static int
hns3_config_ncsi_hw_err_int(struct hns3_adapter *hns, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc;
	int ret;

	/* configure NCSI error interrupts */
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_NCSI_INT_EN, false);
	if (en)
		desc.data[0] = rte_cpu_to_le_32(HNS3_NCSI_ERR_INT_EN);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "fail to %s NCSI error interrupts, ret = %d",
			 en ? "enable" : "disable", ret);

	return ret;
}

static int
enable_igu_egu_err_intr(struct hns3_adapter *hns, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc;
	int ret;

	/* configure IGU,EGU error interrupts */
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_IGU_COMMON_INT_EN, false);
	if (en)
		desc.data[0] = rte_cpu_to_le_32(HNS3_IGU_ERR_INT_ENABLE);
	else
		desc.data[0] = rte_cpu_to_le_32(HNS3_IGU_ERR_INT_DISABLE);

	desc.data[1] = rte_cpu_to_le_32(HNS3_IGU_ERR_INT_EN_MASK);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "fail to %s IGU common interrupts, ret = %d",
			 en ? "enable" : "disable", ret);
		return ret;
	}

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_IGU_EGU_TNL_INT_EN, false);
	if (en)
		desc.data[0] = rte_cpu_to_le_32(HNS3_IGU_TNL_ERR_INT_EN);

	desc.data[1] = rte_cpu_to_le_32(HNS3_IGU_TNL_ERR_INT_EN_MASK);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "fail to %s IGU-EGU TNL interrupts, ret = %d",
			 en ? "enable" : "disable", ret);
		return ret;
	}

	return hns3_config_ncsi_hw_err_int(hns, en);
}

static int
config_ppp_err_intr(struct hns3_adapter *hns, uint32_t cmd, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc[2];
	int ret;

	/* configure PPP error interrupts */
	hns3_cmd_setup_basic_desc(&desc[0], cmd, false);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	hns3_cmd_setup_basic_desc(&desc[1], cmd, false);

	if (cmd == HNS3_OPC_PPP_CMD0_INT_CMD) {
		if (en) {
			desc[0].data[0] =
				rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT0_EN);
			desc[0].data[1] =
				rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT1_EN);
			desc[0].data[4] =
				rte_cpu_to_le_32(HNS3_PPP_PF_ERR_INT_EN);
		}

		desc[1].data[0] =
			rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT0_EN_MASK);
		desc[1].data[1] =
			rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT1_EN_MASK);
		desc[1].data[2] =
			rte_cpu_to_le_32(HNS3_PPP_PF_ERR_INT_EN_MASK);
	} else if (cmd == HNS3_OPC_PPP_CMD1_INT_CMD) {
		if (en) {
			desc[0].data[0] =
				rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT2_EN);
			desc[0].data[1] =
				rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT3_EN);
		}

		desc[1].data[0] =
			rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT2_EN_MASK);
		desc[1].data[1] =
			rte_cpu_to_le_32(HNS3_PPP_MPF_ECC_ERR_INT3_EN_MASK);
	}

	ret = hns3_cmd_send(hw, &desc[0], 2);
	if (ret)
		hns3_err(hw, "fail to %s PPP error int, ret = %d",
		en ? "enable" : "disable", ret);

	return ret;
}

static int
enable_ppp_err_intr(struct hns3_adapter *hns, bool en)
{
	int ret;

	ret = config_ppp_err_intr(hns, HNS3_OPC_PPP_CMD0_INT_CMD, en);
	if (ret)
		return ret;

	return config_ppp_err_intr(hns, HNS3_OPC_PPP_CMD1_INT_CMD, en);
}

static int
enable_ssu_err_intr(struct hns3_adapter *hns, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc[2];
	int ret;

	/* configure SSU ecc error interrupts */
	hns3_cmd_setup_basic_desc(&desc[0], HNS3_OPC_SSU_ECC_INT_CMD, false);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	hns3_cmd_setup_basic_desc(&desc[1], HNS3_OPC_SSU_ECC_INT_CMD, false);
	if (en) {
		desc[0].data[0] =
			rte_cpu_to_le_32(HNS3_SSU_1BIT_ECC_ERR_INT_EN);
		desc[0].data[1] =
			rte_cpu_to_le_32(HNS3_SSU_MULTI_BIT_ECC_ERR_INT_EN);
		desc[0].data[4] =
			rte_cpu_to_le_32(HNS3_SSU_BIT32_ECC_ERR_INT_EN);
	}

	desc[1].data[0] = rte_cpu_to_le_32(HNS3_SSU_1BIT_ECC_ERR_INT_EN_MASK);
	desc[1].data[1] =
		rte_cpu_to_le_32(HNS3_SSU_MULTI_BIT_ECC_ERR_INT_EN_MASK);
	desc[1].data[2] = rte_cpu_to_le_32(HNS3_SSU_BIT32_ECC_ERR_INT_EN_MASK);

	ret = hns3_cmd_send(hw, &desc[0], 2);
	if (ret) {
		hns3_err(hw, "fail to %s SSU ECC error interrupt, ret = %d",
			 en ? "enable" : "disable", ret);
		return ret;
	}

	/* configure SSU common error interrupts */
	hns3_cmd_setup_basic_desc(&desc[0], HNS3_OPC_SSU_COMMON_INT_CMD, false);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	hns3_cmd_setup_basic_desc(&desc[1], HNS3_OPC_SSU_COMMON_INT_CMD, false);

	if (en) {
		desc[0].data[0] = rte_cpu_to_le_32(HNS3_SSU_COMMON_INT_EN);
		desc[0].data[1] =
			rte_cpu_to_le_32(HNS3_SSU_PORT_BASED_ERR_INT_EN);
		desc[0].data[2] =
			rte_cpu_to_le_32(HNS3_SSU_FIFO_OVERFLOW_ERR_INT_EN);
	}

	desc[1].data[0] = rte_cpu_to_le_32(HNS3_SSU_COMMON_INT_EN_MASK |
					   HNS3_SSU_PORT_BASED_ERR_INT_EN_MASK);
	desc[1].data[1] =
		rte_cpu_to_le_32(HNS3_SSU_FIFO_OVERFLOW_ERR_INT_EN_MASK);

	ret = hns3_cmd_send(hw, &desc[0], 2);
	if (ret)
		hns3_err(hw, "fail to %s SSU COMMON error intr, ret = %d",
			 en ? "enable" : "disable", ret);

	return ret;
}

void
hns3_config_mac_tnl_int(struct hns3_hw *hw, bool en)
{
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MAC_TNL_INT_EN, false);
	if (en)
		desc.data[0] = rte_cpu_to_le_32(HNS3_MAC_TNL_INT_EN);
	else
		desc.data[0] = 0;

	desc.data[1] = rte_cpu_to_le_32(HNS3_MAC_TNL_INT_EN_MASK);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "fail to %s mac tnl intr, ret = %d",
			 en ? "enable" : "disable", ret);
}

static int
config_ppu_err_intrs(struct hns3_adapter *hns, uint32_t cmd, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc[2];
	int num = 1;

	/* configure PPU error interrupts */
	switch (cmd) {
	case HNS3_OPC_PPU_MPF_ECC_INT_CMD:
		hns3_cmd_setup_basic_desc(&desc[0], cmd, false);
		desc[0].flag |= HNS3_CMD_FLAG_NEXT;
		hns3_cmd_setup_basic_desc(&desc[1], cmd, false);
		if (en) {
			desc[0].data[0] = HNS3_PPU_MPF_ABNORMAL_INT0_EN;
			desc[0].data[1] = HNS3_PPU_MPF_ABNORMAL_INT1_EN;
			desc[1].data[3] = HNS3_PPU_MPF_ABNORMAL_INT3_EN;
			desc[1].data[4] = HNS3_PPU_MPF_ABNORMAL_INT2_EN;
		}

		desc[1].data[0] = HNS3_PPU_MPF_ABNORMAL_INT0_EN_MASK;
		desc[1].data[1] = HNS3_PPU_MPF_ABNORMAL_INT1_EN_MASK;
		desc[1].data[2] = HNS3_PPU_MPF_ABNORMAL_INT2_EN_MASK;
		desc[1].data[3] |= HNS3_PPU_MPF_ABNORMAL_INT3_EN_MASK;
		num = 2;
		break;
	case HNS3_OPC_PPU_MPF_OTHER_INT_CMD:
		hns3_cmd_setup_basic_desc(&desc[0], cmd, false);
		if (en)
			desc[0].data[0] = HNS3_PPU_MPF_ABNORMAL_INT2_EN2;

		desc[0].data[2] = HNS3_PPU_MPF_ABNORMAL_INT2_EN2_MASK;
		break;
	case HNS3_OPC_PPU_PF_OTHER_INT_CMD:
		hns3_cmd_setup_basic_desc(&desc[0], cmd, false);
		if (en)
			desc[0].data[0] = HNS3_PPU_PF_ABNORMAL_INT_EN;

		desc[0].data[2] = HNS3_PPU_PF_ABNORMAL_INT_EN_MASK;
		break;
	default:
		hns3_err(hw,
			 "Invalid cmd(%u) to configure PPU error interrupts.",
			 cmd);
		return -EINVAL;
	}

	return hns3_cmd_send(hw, &desc[0], num);
}

static int
enable_ppu_err_intr(struct hns3_adapter *hns, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = config_ppu_err_intrs(hns, HNS3_OPC_PPU_MPF_ECC_INT_CMD, en);
	if (ret) {
		hns3_err(hw, "fail to %s PPU MPF ECC error intr, ret = %d",
			 en ? "enable" : "disable", ret);
		return ret;
	}

	ret = config_ppu_err_intrs(hns, HNS3_OPC_PPU_MPF_OTHER_INT_CMD, en);
	if (ret) {
		hns3_err(hw, "fail to %s PPU MPF other intr, ret = %d",
			 en ? "enable" : "disable", ret);
		return ret;
	}

	ret = config_ppu_err_intrs(hns, HNS3_OPC_PPU_PF_OTHER_INT_CMD, en);
	if (ret)
		hns3_err(hw, "fail to %s PPU PF error interrupts, ret = %d",
			 en ? "enable" : "disable", ret);
	return ret;
}

static int
enable_tm_err_intr(struct hns3_adapter *hns, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc;
	int ret;

	/* configure TM SCH error interrupts */
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_SCH_ECC_INT_EN, false);
	if (en)
		desc.data[0] = rte_cpu_to_le_32(HNS3_TM_SCH_ECC_ERR_INT_EN);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "fail to %s TM SCH interrupts, ret = %d",
			 en ? "enable" : "disable", ret);
		return ret;
	}

	/* configure TM QCN hw errors */
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_TM_QCN_MEM_INT_CFG, false);
	desc.data[0] = rte_cpu_to_le_32(HNS3_TM_QCN_ERR_INT_TYPE);
	if (en) {
		desc.data[0] |= rte_cpu_to_le_32(HNS3_TM_QCN_FIFO_INT_EN);
		desc.data[1] = rte_cpu_to_le_32(HNS3_TM_QCN_MEM_ERR_INT_EN);
	}

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "fail to %s TM QCN mem errors, ret = %d\n",
			 en ? "enable" : "disable", ret);

	return ret;
}

static int
enable_common_err_intr(struct hns3_adapter *hns, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc[2];
	int ret;

	/* configure common error interrupts */
	hns3_cmd_setup_basic_desc(&desc[0], HNS3_OPC_COMMON_ECC_INT_CFG, false);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	hns3_cmd_setup_basic_desc(&desc[1], HNS3_OPC_COMMON_ECC_INT_CFG, false);

	if (en) {
		desc[0].data[0] =
			rte_cpu_to_le_32(HNS3_IMP_TCM_ECC_ERR_INT_EN);
		desc[0].data[2] =
			rte_cpu_to_le_32(HNS3_CMDQ_NIC_ECC_ERR_INT_EN);
		desc[0].data[3] =
			rte_cpu_to_le_32(HNS3_IMP_RD_POISON_ERR_INT_EN);
		desc[0].data[4] =
			rte_cpu_to_le_32(HNS3_TQP_ECC_ERR_INT_EN |
					 HNS3_MSIX_SRAM_ECC_ERR_INT_EN);
		desc[0].data[5] =
			rte_cpu_to_le_32(HNS3_IMP_ITCM4_ECC_ERR_INT_EN);
	}

	desc[1].data[0] = rte_cpu_to_le_32(HNS3_IMP_TCM_ECC_ERR_INT_EN_MASK);
	desc[1].data[2] = rte_cpu_to_le_32(HNS3_CMDQ_NIC_ECC_ERR_INT_EN_MASK);
	desc[1].data[3] = rte_cpu_to_le_32(HNS3_IMP_RD_POISON_ERR_INT_EN_MASK);
	desc[1].data[4] = rte_cpu_to_le_32(HNS3_TQP_ECC_ERR_INT_EN_MASK |
				      HNS3_MSIX_SRAM_ECC_ERR_INT_EN_MASK);
	desc[1].data[5] = rte_cpu_to_le_32(HNS3_IMP_ITCM4_ECC_ERR_INT_EN_MASK);

	ret = hns3_cmd_send(hw, &desc[0], RTE_DIM(desc));
	if (ret)
		hns3_err(hw, "fail to %s common err interrupts, ret = %d\n",
			 en ? "enable" : "disable", ret);

	return ret;
}

static int
enable_mac_err_intr(struct hns3_adapter *hns, bool en)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc;
	int ret;

	/* configure MAC common error interrupts */
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MAC_COMMON_INT_EN, false);
	if (en)
		desc.data[0] = rte_cpu_to_le_32(HNS3_MAC_COMMON_ERR_INT_EN);

	desc.data[1] = rte_cpu_to_le_32(HNS3_MAC_COMMON_ERR_INT_EN_MASK);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "fail to %s MAC COMMON error intr: %d",
			 en ? "enable" : "disable", ret);

	return ret;
}

static const struct hns3_hw_blk hw_blk[] = {
	{
		.name = "IGU_EGU",
		.enable_err_intr = enable_igu_egu_err_intr,
	},
	{
		.name = "PPP",
		.enable_err_intr = enable_ppp_err_intr,
	},
	{
		.name = "SSU",
		.enable_err_intr = enable_ssu_err_intr,
	},
	{
		.name = "PPU",
		.enable_err_intr = enable_ppu_err_intr,
	},
	{
		.name = "TM",
		.enable_err_intr = enable_tm_err_intr,
	},
	{
		.name = "COMMON",
		.enable_err_intr = enable_common_err_intr,
	},
	{
		.name = "MAC",
		.enable_err_intr = enable_mac_err_intr,
	},
	{
		.name = NULL,
		.enable_err_intr = NULL,
	}
};

int
hns3_enable_hw_error_intr(struct hns3_adapter *hns, bool en)
{
	const struct hns3_hw_blk *module = hw_blk;
	int ret = 0;

	while (module->enable_err_intr) {
		ret = module->enable_err_intr(hns, en);
		if (ret)
			return ret;

		module++;
	}

	return ret;
}

static enum hns3_reset_level
hns3_find_highest_level(struct hns3_adapter *hns, const char *reg,
			const struct hns3_hw_error *err, uint32_t err_sts)
{
	enum hns3_reset_level reset_level = HNS3_FUNC_RESET;
	struct hns3_hw *hw = &hns->hw;
	bool need_reset = false;

	while (err->msg) {
		if (err->int_msk & err_sts) {
			hns3_warn(hw, "%s %s found [error status=0x%x]",
				  reg, err->msg, err_sts);
			if (err->reset_level != HNS3_NONE_RESET &&
			    err->reset_level >= reset_level) {
				reset_level = err->reset_level;
				need_reset = true;
			}
		}
		err++;
	}
	if (need_reset)
		return reset_level;
	else
		return HNS3_NONE_RESET;
}

static int
query_num_bds(struct hns3_hw *hw, bool is_ras, uint32_t *mpf_bd_num,
	      uint32_t *pf_bd_num)
{
	uint32_t mpf_min_bd_num, pf_min_bd_num;
	uint32_t mpf_bd_num_val, pf_bd_num_val;
	enum hns3_opcode_type opcode;
	struct hns3_cmd_desc desc;
	int ret;

	if (is_ras) {
		opcode = HNS3_OPC_QUERY_RAS_INT_STS_BD_NUM;
		mpf_min_bd_num = HNS3_MPF_RAS_INT_MIN_BD_NUM;
		pf_min_bd_num = HNS3_PF_RAS_INT_MIN_BD_NUM;
	} else {
		opcode = HNS3_OPC_QUERY_MSIX_INT_STS_BD_NUM;
		mpf_min_bd_num = HNS3_MPF_MSIX_INT_MIN_BD_NUM;
		pf_min_bd_num = HNS3_PF_MSIX_INT_MIN_BD_NUM;
	}

	hns3_cmd_setup_basic_desc(&desc, opcode, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "query num bds in msix failed, ret = %d", ret);
		return ret;
	}

	mpf_bd_num_val = rte_le_to_cpu_32(desc.data[0]);
	pf_bd_num_val = rte_le_to_cpu_32(desc.data[1]);
	if (mpf_bd_num_val < mpf_min_bd_num || pf_bd_num_val < pf_min_bd_num) {
		hns3_err(hw, "error bd num: mpf(%u), min_mpf(%u), "
			 "pf(%u), min_pf(%u)\n", mpf_bd_num_val, mpf_min_bd_num,
			 pf_bd_num_val, pf_min_bd_num);
		return -EINVAL;
	}

	*mpf_bd_num = mpf_bd_num_val;
	*pf_bd_num = pf_bd_num_val;

	return 0;
}

void
hns3_intr_unregister(const struct rte_intr_handle *hdl,
		     rte_intr_callback_fn cb_fn, void *cb_arg)
{
	int retry_cnt = 0;
	int ret;

	do {
		ret = rte_intr_callback_unregister(hdl, cb_fn, cb_arg);
		if (ret >= 0) {
			break;
		} else if (ret != -EAGAIN) {
			PMD_INIT_LOG(ERR, "Failed to unregister intr: %d", ret);
			break;
		}
		rte_delay_ms(HNS3_INTR_UNREG_FAIL_DELAY_MS);
	} while (retry_cnt++ < HNS3_INTR_UNREG_FAIL_RETRY_CNT);
}

static uint32_t
hns3_get_hw_error_status(struct hns3_cmd_desc *desc, uint8_t desc_offset,
			 uint8_t data_offset)
{
	uint32_t status;
	uint32_t *desc_data;

	if (desc_offset == 0)
		status = rte_le_to_cpu_32(desc[desc_offset].data[data_offset]);
	else {
		desc_data = (uint32_t *)&desc[desc_offset];
		status = rte_le_to_cpu_32(*(desc_data + data_offset));
	}

	return status;
}

static int
hns3_handle_hw_error(struct hns3_adapter *hns, struct hns3_cmd_desc *desc,
		     int num, uint64_t *levels,
		     enum hns3_hw_err_report_type err_type)
{
	const struct hns3_hw_error_desc *err = pf_ras_err_tbl;
	enum hns3_opcode_type opcode;
	enum hns3_reset_level req_level;
	struct hns3_hw *hw = &hns->hw;
	uint32_t status;
	int ret;

	switch (err_type) {
	case MPF_MSIX_ERR:
		err = mpf_msix_err_tbl;
		opcode = HNS3_OPC_QUERY_CLEAR_ALL_MPF_MSIX_INT;
		break;
	case PF_MSIX_ERR:
		err = pf_msix_err_tbl;
		opcode = HNS3_OPC_QUERY_CLEAR_ALL_PF_MSIX_INT;
		break;
	case MPF_RAS_ERR:
		err = mpf_ras_err_tbl;
		opcode = HNS3_OPC_QUERY_CLEAR_MPF_RAS_INT;
		break;
	case PF_RAS_ERR:
		err = pf_ras_err_tbl;
		opcode = HNS3_OPC_QUERY_CLEAR_PF_RAS_INT;
		break;
	default:
		hns3_err(hw, "error hardware err_type = %d\n", err_type);
		return -EINVAL;
	}

	/* query all hardware errors */
	hns3_cmd_setup_basic_desc(&desc[0], opcode, true);
	ret = hns3_cmd_send(hw, &desc[0], num);
	if (ret) {
		hns3_err(hw, "query hw err int 0x%x cmd failed, ret = %d\n",
			 opcode, ret);
		return ret;
	}

	/* traverses the error table and process based on the error type */
	while (err->msg) {
		status = hns3_get_hw_error_status(desc, err->desc_offset,
						  err->data_offset);
		if (status) {
			/*
			 * set the reset_level or non_reset flag based on
			 * the error type and add error statistics. here just
			 * set the flag, the actual reset action is in
			 * hns3_msix_process.
			 */
			req_level = hns3_find_highest_level(hns, err->msg,
							    err->hw_err,
							    status);
			hns3_atomic_set_bit(req_level, levels);
		}
		err++;
	}

	/* clear all hardware errors */
	hns3_cmd_reuse_desc(&desc[0], false);
	ret = hns3_cmd_send(hw, &desc[0], num);
	if (ret)
		hns3_err(hw, "clear all hw err int cmd failed, ret = %d\n",
			 ret);

	return ret;
}

void
hns3_handle_msix_error(struct hns3_adapter *hns, uint64_t *levels)
{
	uint32_t mpf_bd_num, pf_bd_num, bd_num;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc *desc;
	int ret;

	/* query the number of bds for the MSIx int status */
	ret = query_num_bds(hw, false, &mpf_bd_num, &pf_bd_num);
	if (ret) {
		hns3_err(hw, "fail to query msix int status bd num: ret = %d",
			 ret);
		return;
	}

	bd_num = RTE_MAX(mpf_bd_num, pf_bd_num);
	desc = rte_zmalloc(NULL, bd_num * sizeof(struct hns3_cmd_desc), 0);
	if (desc == NULL) {
		hns3_err(hw,
			 "fail to zmalloc desc for handling msix error, size = %zu",
			 bd_num * sizeof(struct hns3_cmd_desc));
		return;
	}

	/* handle all main PF MSIx errors */
	ret = hns3_handle_hw_error(hns, desc, mpf_bd_num, levels, MPF_MSIX_ERR);
	if (ret) {
		hns3_err(hw, "fail to handle all main pf msix errors, ret = %d",
			 ret);
		goto out;
	}

	memset(desc, 0, bd_num * sizeof(struct hns3_cmd_desc));

	/* handle all PF MSIx errors */
	ret = hns3_handle_hw_error(hns, desc, pf_bd_num, levels, PF_MSIX_ERR);
	if (ret) {
		hns3_err(hw, "fail to handle all pf msix errors, ret = %d",
			 ret);
		goto out;
	}

out:
	rte_free(desc);
}

void
hns3_handle_ras_error(struct hns3_adapter *hns, uint64_t *levels)
{
	uint32_t mpf_bd_num, pf_bd_num, bd_num;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc *desc;
	uint32_t status;
	int ret;

	status = hns3_read_dev(hw, HNS3_RAS_PF_OTHER_INT_STS_REG);
	if ((status & HNS3_RAS_REG_NFE_MASK) == 0)
		return;

	/* query the number of bds for the RAS int status */
	ret = query_num_bds(hw, true, &mpf_bd_num, &pf_bd_num);
	if (ret) {
		hns3_err(hw, "fail to query ras int status bd num: ret = %d",
			 ret);
		return;
	}

	bd_num = RTE_MAX(mpf_bd_num, pf_bd_num);
	desc = rte_zmalloc(NULL, bd_num * sizeof(struct hns3_cmd_desc), 0);
	if (desc == NULL) {
		hns3_err(hw,
			 "fail to zmalloc desc for handing ras error, size = %zu",
			 bd_num * sizeof(struct hns3_cmd_desc));
		return;
	}

	/* handle all main PF RAS errors */
	ret = hns3_handle_hw_error(hns, desc, mpf_bd_num, levels, MPF_RAS_ERR);
	if (ret) {
		hns3_err(hw, "fail to handle all main pf ras errors, ret = %d",
			 ret);
		goto out;
	}

	memset(desc, 0, bd_num * sizeof(struct hns3_cmd_desc));

	/* handle all PF RAS errors */
	ret = hns3_handle_hw_error(hns, desc, pf_bd_num, levels, PF_RAS_ERR);
	if (ret) {
		hns3_err(hw, "fail to handle all pf ras errors, ret = %d", ret);
		goto out;
	}

out:
	rte_free(desc);
}

static void
hns3_handle_type_reg_error_data(struct hns3_hw *hw,
				struct hns3_mod_err_info *mod_err_info,
				struct hns3_type_reg_err_info *err_info)
{
#define HNS3_ERR_TYPE_MASK 0x7F
#define HNS3_ERR_TYPE_IS_RAS_OFFSET 7

	uint8_t mod_id, total_module, type_id, total_type;
	uint8_t is_ras;
	uint8_t i;

	mod_id = mod_err_info->mod_id;
	type_id = err_info->type_id & HNS3_ERR_TYPE_MASK;
	is_ras = err_info->type_id >> HNS3_ERR_TYPE_IS_RAS_OFFSET;

	total_module = RTE_DIM(hns3_hw_module_name);
	total_type = RTE_DIM(hns3_hw_error_type);

	hns3_err(hw, "total_module:%u, total_type:%u",
		 total_module, total_type);

	if (mod_id < total_module && type_id < total_type)
		hns3_err(hw, "found %s %s, is %s error.",
			 hns3_hw_module_name[mod_id].msg,
			 hns3_hw_error_type[type_id].msg,
			 is_ras ? "ras" : "msix");
	else
		hns3_err(hw, "unknown module[%u] or type[%u].",
			 mod_id, type_id);

	hns3_err(hw, "reg_value:");
	for (i = 0; i < err_info->reg_num; i++)
		hns3_err(hw, "0x%08x", err_info->reg[i]);
}

static void
hns3_handle_module_error_data(struct hns3_hw *hw, uint32_t *buf,
			      uint32_t buf_size)
{
	struct hns3_type_reg_err_info *type_reg_err_info;
	struct hns3_mod_err_info *mod_err_info;
	struct hns3_sum_err_info *sum_err_info;
	uint8_t mod_num, reset_type;
	uint32_t offset = 0;
	uint8_t err_num;
	uint8_t i;

	sum_err_info = (struct hns3_sum_err_info *)&buf[offset++];
	mod_num = sum_err_info->mod_num;
	reset_type = sum_err_info->reset_type;
	if (reset_type && reset_type != HNS3_NONE_RESET)
		hns3_atomic_set_bit(reset_type, &hw->reset.request);

	hns3_err(hw, "reset_type = %s, mod_num = %u.",
		 reset_string[reset_type], mod_num);

	while (mod_num--) {
		if (offset >= buf_size) {
			hns3_err(hw, "offset(%u) exceeds buf's size(%u).",
				 offset, buf_size);
			return;
		}
		mod_err_info = (struct hns3_mod_err_info *)&buf[offset++];
		err_num = mod_err_info->err_num;
		for (i = 0; i < err_num; i++) {
			if (offset >= buf_size) {
				hns3_err(hw,
					 "offset(%u) exceeds buf size(%u).",
					 offset, buf_size);
				return;
			}

			type_reg_err_info = (struct hns3_type_reg_err_info *)
					     &buf[offset++];
			hns3_handle_type_reg_error_data(hw, mod_err_info,
							type_reg_err_info);

			offset += type_reg_err_info->reg_num;
		}
	}
}

static int
hns3_query_all_err_bd_num(struct hns3_hw *hw, uint32_t *bd_num)
{
	struct hns3_cmd_desc desc;
	uint32_t bd_num_data;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_ALL_ERR_BD_NUM, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "failed to query error bd_num, ret = %d.", ret);
		return ret;
	}

	bd_num_data = rte_le_to_cpu_32(desc.data[0]);
	*bd_num = bd_num_data;
	if (bd_num_data == 0) {
		hns3_err(hw, "the value of bd_num is 0!");
		return -EINVAL;
	}

	return 0;
}

static int
hns3_query_all_err_info(struct hns3_hw *hw, struct hns3_cmd_desc *desc,
			uint32_t bd_num)
{
	int ret;

	hns3_cmd_setup_basic_desc(desc, HNS3_OPC_QUERY_ALL_ERR_INFO, true);
	ret = hns3_cmd_send(hw, desc, bd_num);
	if (ret) {
		hns3_err(hw, "failed to query error info, ret = %d.", ret);
		return ret;
	}

	return ret;
}

static void
hns3_handle_hw_error_v2(struct hns3_hw *hw)
{
	uint32_t bd_num, buf_len, i, buf_size;
	struct hns3_cmd_desc *desc;
	uint32_t *desc_data;
	uint32_t *buf;
	int ret;

	ret = hns3_query_all_err_bd_num(hw, &bd_num);
	if (ret)
		goto out;

	desc = rte_zmalloc("hns3_ras", bd_num * sizeof(struct hns3_cmd_desc),
					   0);
	if (desc == NULL) {
		hns3_err(hw, "failed to malloc hns3 ras cmd desc.");
		goto out;
	}

	ret = hns3_query_all_err_info(hw, desc, bd_num);
	if (ret)
		goto err_desc;

	buf_len = bd_num * sizeof(struct hns3_cmd_desc) - HNS3_DESC_NO_DATA_LEN;
	buf_size = buf_len / HNS3_DESC_DATA_UNIT_SIZE;

	desc_data = rte_zmalloc("hns3_ras", buf_len, 0);
	if (desc_data == NULL) {
		hns3_err(hw, "failed to malloc hns3 ras desc data.");
		goto err_desc;
	}

	buf = rte_zmalloc("hns3_ras", buf_len, 0);
	if (buf == NULL) {
		hns3_err(hw, "failed to malloc hns3 ras buf data.");
		goto err_buf_alloc;
	}

	memcpy(desc_data, &desc[0].data[0], buf_len);
	for (i = 0; i < buf_size; i++)
		buf[i] = rte_le_to_cpu_32(desc_data[i]);

	hns3_handle_module_error_data(hw, buf, buf_size);
	rte_free(buf);

err_buf_alloc:
	rte_free(desc_data);
err_desc:
	rte_free(desc);
out:
	return;
}

void
hns3_handle_error(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;

	if (hns3_dev_get_support(hw, RAS_IMP)) {
		hns3_handle_hw_error_v2(hw);
		hns3_schedule_reset(hns);
	} else {
		hns3_handle_msix_error(hns, &hw->reset.request);
		hns3_handle_ras_error(hns, &hw->reset.request);
		hns3_schedule_reset(hns);
	}
}

int
hns3_reset_init(struct hns3_hw *hw)
{
	rte_spinlock_init(&hw->lock);
	hw->reset.level = HNS3_NONE_RESET;
	hw->reset.stage = RESET_STAGE_NONE;
	hw->reset.request = 0;
	hw->reset.pending = 0;
	hw->reset.resetting = 0;
	__atomic_store_n(&hw->reset.disable_cmd, 0, __ATOMIC_RELAXED);
	hw->reset.wait_data = rte_zmalloc("wait_data",
					  sizeof(struct hns3_wait_data), 0);
	if (!hw->reset.wait_data) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for wait_data");
		return -ENOMEM;
	}
	return 0;
}

void
hns3_schedule_reset(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;

	/* Reschedule the reset process after successful initialization */
	if (hw->adapter_state == HNS3_NIC_UNINITIALIZED) {
		__atomic_store_n(&hw->reset.schedule, SCHEDULE_PENDING,
				 __ATOMIC_RELAXED);
		return;
	}

	if (hw->adapter_state >= HNS3_NIC_CLOSED)
		return;

	/* Schedule restart alarm if it is not scheduled yet */
	if (__atomic_load_n(&hw->reset.schedule, __ATOMIC_RELAXED) ==
			SCHEDULE_REQUESTED)
		return;
	if (__atomic_load_n(&hw->reset.schedule, __ATOMIC_RELAXED) ==
			    SCHEDULE_DEFERRED)
		rte_eal_alarm_cancel(hw->reset.ops->reset_service, hns);

	__atomic_store_n(&hw->reset.schedule, SCHEDULE_REQUESTED,
				 __ATOMIC_RELAXED);

	rte_eal_alarm_set(SWITCH_CONTEXT_US, hw->reset.ops->reset_service, hns);
}

void
hns3_schedule_delayed_reset(struct hns3_adapter *hns)
{
#define DEFERRED_SCHED_US (3 * MSEC_PER_SEC * USEC_PER_MSEC)
	struct hns3_hw *hw = &hns->hw;

	/* Do nothing if it is uninited or closed */
	if (hw->adapter_state == HNS3_NIC_UNINITIALIZED ||
	    hw->adapter_state >= HNS3_NIC_CLOSED) {
		return;
	}

	if (__atomic_load_n(&hw->reset.schedule, __ATOMIC_RELAXED) !=
			    SCHEDULE_NONE)
		return;
	__atomic_store_n(&hw->reset.schedule, SCHEDULE_DEFERRED,
			 __ATOMIC_RELAXED);
	rte_eal_alarm_set(DEFERRED_SCHED_US, hw->reset.ops->reset_service, hns);
}

void
hns3_wait_callback(void *param)
{
	struct hns3_wait_data *data = (struct hns3_wait_data *)param;
	struct hns3_adapter *hns = data->hns;
	struct hns3_hw *hw = &hns->hw;
	uint64_t msec;
	bool done;

	data->count--;
	if (data->check_completion) {
		/*
		 * Check if the current time exceeds the deadline
		 * or a pending reset coming, or reset during close.
		 */
		msec = hns3_clock_gettime_ms();
		if (msec > data->end_ms || is_reset_pending(hns) ||
		    hw->adapter_state == HNS3_NIC_CLOSING) {
			done = false;
			data->count = 0;
		} else
			done = data->check_completion(hw);
	} else
		done = true;

	if (!done && data->count > 0) {
		rte_eal_alarm_set(data->interval, hns3_wait_callback, data);
		return;
	}
	if (done)
		data->result = HNS3_WAIT_SUCCESS;
	else {
		hns3_err(hw, "%s wait timeout at stage %d",
			 reset_string[hw->reset.level], hw->reset.stage);
		data->result = HNS3_WAIT_TIMEOUT;
	}
	hns3_schedule_reset(hns);
}

void
hns3_notify_reset_ready(struct hns3_hw *hw, bool enable)
{
	uint32_t reg_val;

	reg_val = hns3_read_dev(hw, HNS3_CMDQ_TX_DEPTH_REG);
	if (enable)
		reg_val |= HNS3_NIC_SW_RST_RDY;
	else
		reg_val &= ~HNS3_NIC_SW_RST_RDY;

	hns3_write_dev(hw, HNS3_CMDQ_TX_DEPTH_REG, reg_val);
}

int
hns3_reset_req_hw_reset(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;

	if (hw->reset.wait_data->result == HNS3_WAIT_UNKNOWN) {
		hw->reset.wait_data->hns = hns;
		hw->reset.wait_data->check_completion = NULL;
		hw->reset.wait_data->interval = HNS3_RESET_SYNC_US;
		hw->reset.wait_data->count = 1;
		hw->reset.wait_data->result = HNS3_WAIT_REQUEST;
		rte_eal_alarm_set(hw->reset.wait_data->interval,
				  hns3_wait_callback, hw->reset.wait_data);
		return -EAGAIN;
	} else if (hw->reset.wait_data->result == HNS3_WAIT_REQUEST)
		return -EAGAIN;

	/* inform hardware that preparatory work is done */
	hns3_notify_reset_ready(hw, true);
	return 0;
}

static void
hns3_clear_reset_level(struct hns3_hw *hw, uint64_t *levels)
{
	uint64_t merge_cnt = hw->reset.stats.merge_cnt;
	uint64_t tmp;

	switch (hw->reset.level) {
	case HNS3_IMP_RESET:
		hns3_atomic_clear_bit(HNS3_IMP_RESET, levels);
		tmp = hns3_test_and_clear_bit(HNS3_GLOBAL_RESET, levels);
		merge_cnt = tmp > 0 ? merge_cnt + 1 : merge_cnt;
		tmp = hns3_test_and_clear_bit(HNS3_FUNC_RESET, levels);
		merge_cnt = tmp > 0 ? merge_cnt + 1 : merge_cnt;
		break;
	case HNS3_GLOBAL_RESET:
		hns3_atomic_clear_bit(HNS3_GLOBAL_RESET, levels);
		tmp = hns3_test_and_clear_bit(HNS3_FUNC_RESET, levels);
		merge_cnt = tmp > 0 ? merge_cnt + 1 : merge_cnt;
		break;
	case HNS3_FUNC_RESET:
		hns3_atomic_clear_bit(HNS3_FUNC_RESET, levels);
		break;
	case HNS3_VF_RESET:
		hns3_atomic_clear_bit(HNS3_VF_RESET, levels);
		tmp = hns3_test_and_clear_bit(HNS3_VF_PF_FUNC_RESET, levels);
		merge_cnt = tmp > 0 ? merge_cnt + 1 : merge_cnt;
		tmp = hns3_test_and_clear_bit(HNS3_VF_FUNC_RESET, levels);
		merge_cnt = tmp > 0 ? merge_cnt + 1 : merge_cnt;
		break;
	case HNS3_VF_FULL_RESET:
		hns3_atomic_clear_bit(HNS3_VF_FULL_RESET, levels);
		tmp = hns3_test_and_clear_bit(HNS3_VF_FUNC_RESET, levels);
		merge_cnt = tmp > 0 ? merge_cnt + 1 : merge_cnt;
		break;
	case HNS3_VF_PF_FUNC_RESET:
		hns3_atomic_clear_bit(HNS3_VF_PF_FUNC_RESET, levels);
		tmp = hns3_test_and_clear_bit(HNS3_VF_FUNC_RESET, levels);
		merge_cnt = tmp > 0 ? merge_cnt + 1 : merge_cnt;
		break;
	case HNS3_VF_FUNC_RESET:
		hns3_atomic_clear_bit(HNS3_VF_FUNC_RESET, levels);
		break;
	case HNS3_FLR_RESET:
		hns3_atomic_clear_bit(HNS3_FLR_RESET, levels);
		break;
	case HNS3_NONE_RESET:
	default:
		return;
	};

	if (merge_cnt != hw->reset.stats.merge_cnt) {
		hns3_warn(hw,
			  "No need to do low-level reset after %s reset. "
			  "merge cnt: %" PRIu64 " total merge cnt: %" PRIu64,
			  reset_string[hw->reset.level],
			  hw->reset.stats.merge_cnt - merge_cnt,
			  hw->reset.stats.merge_cnt);
		hw->reset.stats.merge_cnt = merge_cnt;
	}
}

static bool
hns3_reset_err_handle(struct hns3_adapter *hns)
{
#define MAX_RESET_FAIL_CNT 30

	struct hns3_hw *hw = &hns->hw;

	if (hw->adapter_state == HNS3_NIC_CLOSING)
		goto reset_fail;

	if (is_reset_pending(hns)) {
		hw->reset.attempts = 0;
		hw->reset.stats.fail_cnt++;
		hns3_warn(hw, "%s reset fail because new Reset is pending "
			      "attempts:%" PRIu64,
			  reset_string[hw->reset.level],
			  hw->reset.stats.fail_cnt);
		hw->reset.level = HNS3_NONE_RESET;
		return true;
	}

	hw->reset.attempts++;
	if (hw->reset.attempts < MAX_RESET_FAIL_CNT) {
		hns3_atomic_set_bit(hw->reset.level, &hw->reset.pending);
		hns3_warn(hw, "%s retry to reset attempts: %d",
			  reset_string[hw->reset.level],
			  hw->reset.attempts);
		return true;
	}

	/*
	 * Failure to reset does not mean that the network port is
	 * completely unavailable, so cmd still needs to be initialized.
	 * Regardless of whether the execution is successful or not, the
	 * flow after execution must be continued.
	 */
	if (__atomic_load_n(&hw->reset.disable_cmd, __ATOMIC_RELAXED))
		(void)hns3_cmd_init(hw);
reset_fail:
	hw->reset.attempts = 0;
	hw->reset.stats.fail_cnt++;
	hns3_warn(hw, "%s reset fail fail_cnt:%" PRIu64 " success_cnt:%" PRIu64
		  " global_cnt:%" PRIu64 " imp_cnt:%" PRIu64
		  " request_cnt:%" PRIu64 " exec_cnt:%" PRIu64
		  " merge_cnt:%" PRIu64 "adapter_state:%d",
		  reset_string[hw->reset.level], hw->reset.stats.fail_cnt,
		  hw->reset.stats.success_cnt, hw->reset.stats.global_cnt,
		  hw->reset.stats.imp_cnt, hw->reset.stats.request_cnt,
		  hw->reset.stats.exec_cnt, hw->reset.stats.merge_cnt,
		  hw->adapter_state);

	/* IMP no longer waiting the ready flag */
	hns3_notify_reset_ready(hw, true);
	return false;
}

static int
hns3_reset_pre(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct timeval tv;
	int ret;

	if (hw->reset.stage == RESET_STAGE_NONE) {
		__atomic_store_n(&hns->hw.reset.resetting, 1, __ATOMIC_RELAXED);
		hw->reset.stage = RESET_STAGE_DOWN;
		hns3_report_reset_begin(hw);
		ret = hw->reset.ops->stop_service(hns);
		hns3_clock_gettime(&tv);
		if (ret) {
			hns3_warn(hw, "Reset step1 down fail=%d time=%ld.%.6ld",
				  ret, tv.tv_sec, tv.tv_usec);
			return ret;
		}
		hns3_warn(hw, "Reset step1 down success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.stage = RESET_STAGE_PREWAIT;
	}
	if (hw->reset.stage == RESET_STAGE_PREWAIT) {
		ret = hw->reset.ops->prepare_reset(hns);
		hns3_clock_gettime(&tv);
		if (ret) {
			hns3_warn(hw,
				  "Reset step2 prepare wait fail=%d time=%ld.%.6ld",
				  ret, tv.tv_sec, tv.tv_usec);
			return ret;
		}
		hns3_warn(hw, "Reset step2 prepare wait success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.stage = RESET_STAGE_REQ_HW_RESET;
		hw->reset.wait_data->result = HNS3_WAIT_UNKNOWN;
	}
	return 0;
}

static int
hns3_reset_post(struct hns3_adapter *hns)
{
#define TIMEOUT_RETRIES_CNT	30
	struct hns3_hw *hw = &hns->hw;
	struct timeval tv_delta;
	struct timeval tv;
	int ret = 0;

	if (hw->adapter_state == HNS3_NIC_CLOSING) {
		hns3_warn(hw, "Don't do reset_post during closing, just uninit cmd");
		hns3_cmd_uninit(hw);
		return -EPERM;
	}

	if (hw->reset.stage == RESET_STAGE_DEV_INIT) {
		rte_spinlock_lock(&hw->lock);
		if (hw->reset.mbuf_deferred_free) {
			hns3_dev_release_mbufs(hns);
			hw->reset.mbuf_deferred_free = false;
		}
		ret = hw->reset.ops->reinit_dev(hns);
		rte_spinlock_unlock(&hw->lock);
		hns3_clock_gettime(&tv);
		if (ret) {
			hns3_warn(hw, "Reset step5 devinit fail=%d retries=%d",
				  ret, hw->reset.retries);
			goto err;
		}
		hns3_warn(hw, "Reset step5 devinit success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.retries = 0;
		hw->reset.stage = RESET_STAGE_RESTORE;
		rte_eal_alarm_set(SWITCH_CONTEXT_US,
				  hw->reset.ops->reset_service, hns);
		return -EAGAIN;
	}
	if (hw->reset.stage == RESET_STAGE_RESTORE) {
		rte_spinlock_lock(&hw->lock);
		ret = hw->reset.ops->restore_conf(hns);
		rte_spinlock_unlock(&hw->lock);
		hns3_clock_gettime(&tv);
		if (ret) {
			hns3_warn(hw,
				  "Reset step6 restore fail=%d retries=%d",
				  ret, hw->reset.retries);
			goto err;
		}
		hns3_warn(hw, "Reset step6 restore success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.retries = 0;
		hw->reset.stage = RESET_STAGE_DONE;
	}
	if (hw->reset.stage == RESET_STAGE_DONE) {
		/* IMP will wait ready flag before reset */
		hns3_notify_reset_ready(hw, false);
		hns3_clear_reset_level(hw, &hw->reset.pending);
		hns3_clear_reset_event(hw);
		__atomic_store_n(&hns->hw.reset.resetting, 0, __ATOMIC_RELAXED);
		hw->reset.attempts = 0;
		hw->reset.stats.success_cnt++;
		hw->reset.stage = RESET_STAGE_NONE;
		rte_spinlock_lock(&hw->lock);
		hw->reset.ops->start_service(hns);
		rte_spinlock_unlock(&hw->lock);
		hns3_clock_gettime(&tv);
		timersub(&tv, &hw->reset.start_time, &tv_delta);
		hns3_warn(hw, "%s reset done fail_cnt:%" PRIu64
			  " success_cnt:%" PRIu64 " global_cnt:%" PRIu64
			  " imp_cnt:%" PRIu64 " request_cnt:%" PRIu64
			  " exec_cnt:%" PRIu64 " merge_cnt:%" PRIu64,
			  reset_string[hw->reset.level],
			  hw->reset.stats.fail_cnt, hw->reset.stats.success_cnt,
			  hw->reset.stats.global_cnt, hw->reset.stats.imp_cnt,
			  hw->reset.stats.request_cnt, hw->reset.stats.exec_cnt,
			  hw->reset.stats.merge_cnt);
		hns3_warn(hw,
			  "%s reset done delta %" PRIu64 " ms time=%ld.%.6ld",
			  reset_string[hw->reset.level],
			  hns3_clock_calctime_ms(&tv_delta),
			  tv.tv_sec, tv.tv_usec);
		hw->reset.level = HNS3_NONE_RESET;
		hns3_report_reset_success(hw);
	}
	return 0;

err:
	if (ret == -ETIME) {
		hw->reset.retries++;
		if (hw->reset.retries < TIMEOUT_RETRIES_CNT) {
			rte_eal_alarm_set(HNS3_RESET_SYNC_US,
					  hw->reset.ops->reset_service, hns);
			return -EAGAIN;
		}
	}
	hw->reset.retries = 0;
	return -EIO;
}

static void
hns3_reset_fail_handle(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct timeval tv_delta;
	struct timeval tv;

	hns3_clear_reset_level(hw, &hw->reset.pending);
	hns3_clear_reset_event(hw);
	if (hns3_reset_err_handle(hns)) {
		hw->reset.stage = RESET_STAGE_PREWAIT;
		hns3_schedule_reset(hns);
		return;
	}

	rte_spinlock_lock(&hw->lock);
	if (hw->reset.mbuf_deferred_free) {
		hns3_dev_release_mbufs(hns);
		hw->reset.mbuf_deferred_free = false;
	}
	rte_spinlock_unlock(&hw->lock);
	__atomic_store_n(&hns->hw.reset.resetting, 0, __ATOMIC_RELAXED);
	hw->reset.stage = RESET_STAGE_NONE;
	hns3_clock_gettime(&tv);
	timersub(&tv, &hw->reset.start_time, &tv_delta);
	hns3_warn(hw, "%s reset fail delta %" PRIu64 " ms time=%ld.%.6ld",
		  reset_string[hw->reset.level],
		  hns3_clock_calctime_ms(&tv_delta),
		  tv.tv_sec, tv.tv_usec);
	hw->reset.level = HNS3_NONE_RESET;
	hns3_report_reset_failed(hw);
}

/*
 * There are three scenarios as follows:
 * When the reset is not in progress, the reset process starts.
 * During the reset process, if the reset level has not changed,
 * the reset process continues; otherwise, the reset process is aborted.
 *	hw->reset.level   new_level          action
 *	HNS3_NONE_RESET	 HNS3_XXXX_RESET    start reset
 *	HNS3_XXXX_RESET  HNS3_XXXX_RESET    continue reset
 *	HNS3_LOW_RESET   HNS3_HIGH_RESET    abort
 */
int
hns3_reset_process(struct hns3_adapter *hns, enum hns3_reset_level new_level)
{
	struct hns3_hw *hw = &hns->hw;
	struct timeval tv;
	int ret;

	if (hw->reset.level == HNS3_NONE_RESET) {
		hw->reset.level = new_level;
		hw->reset.stats.exec_cnt++;
		hns3_clock_gettime(&hw->reset.start_time);
		hns3_warn(hw, "Start %s reset time=%ld.%.6ld",
			  reset_string[hw->reset.level],
			  hw->reset.start_time.tv_sec,
			  hw->reset.start_time.tv_usec);
	}

	if (is_reset_pending(hns)) {
		hns3_clock_gettime(&tv);
		hns3_warn(hw,
			  "%s reset is aborted by high level time=%ld.%.6ld",
			  reset_string[hw->reset.level], tv.tv_sec, tv.tv_usec);
		if (hw->reset.wait_data->result == HNS3_WAIT_REQUEST)
			rte_eal_alarm_cancel(hns3_wait_callback,
					     hw->reset.wait_data);
		goto err;
	}

	ret = hns3_reset_pre(hns);
	if (ret)
		goto err;

	if (hw->reset.stage == RESET_STAGE_REQ_HW_RESET) {
		ret = hns3_reset_req_hw_reset(hns);
		if (ret == -EAGAIN)
			return ret;
		hns3_clock_gettime(&tv);
		hns3_warn(hw,
			  "Reset step3 request IMP reset success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.stage = RESET_STAGE_WAIT;
		hw->reset.wait_data->result = HNS3_WAIT_UNKNOWN;
	}
	if (hw->reset.stage == RESET_STAGE_WAIT) {
		ret = hw->reset.ops->wait_hardware_ready(hns);
		if (ret)
			goto retry;
		hns3_clock_gettime(&tv);
		hns3_warn(hw, "Reset step4 reset wait success time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		hw->reset.stage = RESET_STAGE_DEV_INIT;
	}

	ret = hns3_reset_post(hns);
	if (ret)
		goto retry;

	return 0;
retry:
	if (ret == -EAGAIN)
		return ret;
err:
	hns3_reset_fail_handle(hns);

	return -EIO;
}

/*
 * The reset process can only be terminated after handshake with IMP(step3),
 * so that IMP can complete the reset process normally.
 */
void
hns3_reset_abort(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct timeval tv;
	int i;

	for (i = 0; i < HNS3_QUIT_RESET_CNT; i++) {
		if (hw->reset.level == HNS3_NONE_RESET)
			break;
		rte_delay_ms(HNS3_QUIT_RESET_DELAY_MS);
	}

	/* IMP no longer waiting the ready flag */
	hns3_notify_reset_ready(hw, true);

	rte_eal_alarm_cancel(hw->reset.ops->reset_service, hns);
	rte_eal_alarm_cancel(hns3_wait_callback, hw->reset.wait_data);

	if (hw->reset.level != HNS3_NONE_RESET) {
		hns3_clock_gettime(&tv);
		hns3_err(hw, "Failed to terminate reset: %s time=%ld.%.6ld",
			 reset_string[hw->reset.level], tv.tv_sec, tv.tv_usec);
	}
}

static void
hns3_report_lse(void *arg)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)arg;
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (hw->adapter_state == HNS3_NIC_STARTED)
		rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
}

void
hns3_start_report_lse(struct rte_eth_dev *dev)
{
#define DELAY_REPORT_LSE_US	1
	/*
	 * When this function called, the context may hold hns3_hw.lock, if
	 * report lse right now, in some application such as bonding, it will
	 * trigger call driver's ops which may acquire hns3_hw.lock again, so
	 * lead to deadlock.
	 * Here we use delay report to avoid the deadlock.
	 */
	rte_eal_alarm_set(DELAY_REPORT_LSE_US, hns3_report_lse, dev);
}

void
hns3_stop_report_lse(struct rte_eth_dev *dev)
{
	rte_eal_alarm_cancel(hns3_report_lse, dev);
}
