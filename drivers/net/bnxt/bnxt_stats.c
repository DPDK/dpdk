/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>

#include <rte_string_fns.h>
#include <rte_byteorder.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_filter.h"
#include "bnxt_hwrm.h"
#include "bnxt_rxq.h"
#include "bnxt_stats.h"
#include "bnxt_txq.h"
#include "bnxt_vnic.h"
#include "hsi_struct_def_dpdk.h"

static const struct bnxt_xstats_name_off bnxt_rx_stats_strings[] = {
	{"rx_64b_frames", offsetof(struct rx_port_stats,
				rx_64b_frames)},
	{"rx_65b_127b_frames", offsetof(struct rx_port_stats,
				rx_65b_127b_frames)},
	{"rx_128b_255b_frames", offsetof(struct rx_port_stats,
				rx_128b_255b_frames)},
	{"rx_256b_511b_frames", offsetof(struct rx_port_stats,
				rx_256b_511b_frames)},
	{"rx_512b_1023b_frames", offsetof(struct rx_port_stats,
				rx_512b_1023b_frames)},
	{"rx_1024b_1518b_frames", offsetof(struct rx_port_stats,
				rx_1024b_1518b_frames)},
	{"rx_good_vlan_frames", offsetof(struct rx_port_stats,
				rx_good_vlan_frames)},
	{"rx_1519b_2047b_frames", offsetof(struct rx_port_stats,
				rx_1519b_2047b_frames)},
	{"rx_2048b_4095b_frames", offsetof(struct rx_port_stats,
				rx_2048b_4095b_frames)},
	{"rx_4096b_9216b_frames", offsetof(struct rx_port_stats,
				rx_4096b_9216b_frames)},
	{"rx_9217b_16383b_frames", offsetof(struct rx_port_stats,
				rx_9217b_16383b_frames)},
	{"rx_total_frames", offsetof(struct rx_port_stats,
				rx_total_frames)},
	{"rx_ucast_frames", offsetof(struct rx_port_stats,
				rx_ucast_frames)},
	{"rx_mcast_frames", offsetof(struct rx_port_stats,
				rx_mcast_frames)},
	{"rx_bcast_frames", offsetof(struct rx_port_stats,
				rx_bcast_frames)},
	{"rx_fcs_err_frames", offsetof(struct rx_port_stats,
				rx_fcs_err_frames)},
	{"rx_ctrl_frames", offsetof(struct rx_port_stats,
				rx_ctrl_frames)},
	{"rx_pause_frames", offsetof(struct rx_port_stats,
				rx_pause_frames)},
	{"rx_pfc_frames", offsetof(struct rx_port_stats,
				rx_pfc_frames)},
	{"rx_unsupported_opcode_frames", offsetof(struct rx_port_stats,
				rx_unsupported_opcode_frames)},
	{"rx_unsupported_da_pausepfc_frames", offsetof(struct rx_port_stats,
				rx_unsupported_da_pausepfc_frames)},
	{"rx_wrong_sa_frames", offsetof(struct rx_port_stats,
				rx_wrong_sa_frames)},
	{"rx_align_err_frames", offsetof(struct rx_port_stats,
				rx_align_err_frames)},
	{"rx_oor_len_frames", offsetof(struct rx_port_stats,
				rx_oor_len_frames)},
	{"rx_code_err_frames", offsetof(struct rx_port_stats,
				rx_code_err_frames)},
	{"rx_false_carrier_frames", offsetof(struct rx_port_stats,
				rx_false_carrier_frames)},
	{"rx_ovrsz_frames", offsetof(struct rx_port_stats,
				rx_ovrsz_frames)},
	{"rx_jbr_frames", offsetof(struct rx_port_stats,
				rx_jbr_frames)},
	{"rx_mtu_err_frames", offsetof(struct rx_port_stats,
				rx_mtu_err_frames)},
	{"rx_match_crc_frames", offsetof(struct rx_port_stats,
				rx_match_crc_frames)},
	{"rx_promiscuous_frames", offsetof(struct rx_port_stats,
				rx_promiscuous_frames)},
	{"rx_tagged_frames", offsetof(struct rx_port_stats,
				rx_tagged_frames)},
	{"rx_double_tagged_frames", offsetof(struct rx_port_stats,
				rx_double_tagged_frames)},
	{"rx_trunc_frames", offsetof(struct rx_port_stats,
				rx_trunc_frames)},
	{"rx_good_frames", offsetof(struct rx_port_stats,
				rx_good_frames)},
	{"rx_sch_crc_err_frames", offsetof(struct rx_port_stats,
				rx_sch_crc_err_frames)},
	{"rx_undrsz_frames", offsetof(struct rx_port_stats,
				rx_undrsz_frames)},
	{"rx_frag_frames", offsetof(struct rx_port_stats,
				rx_frag_frames)},
	{"rx_eee_lpi_events", offsetof(struct rx_port_stats,
				rx_eee_lpi_events)},
	{"rx_eee_lpi_duration", offsetof(struct rx_port_stats,
				rx_eee_lpi_duration)},
	{"rx_llfc_physical_msgs", offsetof(struct rx_port_stats,
				rx_llfc_physical_msgs)},
	{"rx_llfc_logical_msgs", offsetof(struct rx_port_stats,
				rx_llfc_logical_msgs)},
	{"rx_llfc_msgs_with_crc_err", offsetof(struct rx_port_stats,
				rx_llfc_msgs_with_crc_err)},
	{"rx_hcfc_msgs", offsetof(struct rx_port_stats,
				rx_hcfc_msgs)},
	{"rx_hcfc_msgs_with_crc_err", offsetof(struct rx_port_stats,
				rx_hcfc_msgs_with_crc_err)},
	{"rx_bytes", offsetof(struct rx_port_stats,
				rx_bytes)},
	{"rx_runt_bytes", offsetof(struct rx_port_stats,
				rx_runt_bytes)},
	{"rx_runt_frames", offsetof(struct rx_port_stats,
				rx_runt_frames)},
	{"rx_pfc_xon2xoff_frames_pri0", offsetof(struct rx_port_stats,
				rx_pfc_xon2xoff_frames_pri0)},
	{"rx_pfc_xon2xoff_frames_pri1", offsetof(struct rx_port_stats,
				rx_pfc_xon2xoff_frames_pri1)},
	{"rx_pfc_xon2xoff_frames_pri2", offsetof(struct rx_port_stats,
				rx_pfc_xon2xoff_frames_pri2)},
	{"rx_pfc_xon2xoff_frames_pri3", offsetof(struct rx_port_stats,
				rx_pfc_xon2xoff_frames_pri3)},
	{"rx_pfc_xon2xoff_frames_pri4", offsetof(struct rx_port_stats,
				rx_pfc_xon2xoff_frames_pri4)},
	{"rx_pfc_xon2xoff_frames_pri5", offsetof(struct rx_port_stats,
				rx_pfc_xon2xoff_frames_pri5)},
	{"rx_pfc_xon2xoff_frames_pri6", offsetof(struct rx_port_stats,
				rx_pfc_xon2xoff_frames_pri6)},
	{"rx_pfc_xon2xoff_frames_pri7", offsetof(struct rx_port_stats,
				rx_pfc_xon2xoff_frames_pri7)},
	{"rx_pfc_ena_frames_pri0", offsetof(struct rx_port_stats,
				rx_pfc_ena_frames_pri0)},
	{"rx_pfc_ena_frames_pri1", offsetof(struct rx_port_stats,
				rx_pfc_ena_frames_pri1)},
	{"rx_pfc_ena_frames_pri2", offsetof(struct rx_port_stats,
				rx_pfc_ena_frames_pri2)},
	{"rx_pfc_ena_frames_pri3", offsetof(struct rx_port_stats,
				rx_pfc_ena_frames_pri3)},
	{"rx_pfc_ena_frames_pri4", offsetof(struct rx_port_stats,
				rx_pfc_ena_frames_pri4)},
	{"rx_pfc_ena_frames_pri5", offsetof(struct rx_port_stats,
				rx_pfc_ena_frames_pri5)},
	{"rx_pfc_ena_frames_pri6", offsetof(struct rx_port_stats,
				rx_pfc_ena_frames_pri6)},
	{"rx_pfc_ena_frames_pri7", offsetof(struct rx_port_stats,
				rx_pfc_ena_frames_pri7)},
	{"rx_stat_discard", offsetof(struct rx_port_stats,
				rx_stat_discard)},
	{"rx_stat_err", offsetof(struct rx_port_stats,
				rx_stat_err)},
};

static const struct bnxt_xstats_name_off bnxt_tx_stats_strings[] = {
	{"tx_64b_frames", offsetof(struct tx_port_stats,
				tx_64b_frames)},
	{"tx_65b_127b_frames", offsetof(struct tx_port_stats,
				tx_65b_127b_frames)},
	{"tx_128b_255b_frames", offsetof(struct tx_port_stats,
				tx_128b_255b_frames)},
	{"tx_256b_511b_frames", offsetof(struct tx_port_stats,
				tx_256b_511b_frames)},
	{"tx_512b_1023b_frames", offsetof(struct tx_port_stats,
				tx_512b_1023b_frames)},
	{"tx_1024b_1518b_frames", offsetof(struct tx_port_stats,
				tx_1024b_1518b_frames)},
	{"tx_good_vlan_frames", offsetof(struct tx_port_stats,
				tx_good_vlan_frames)},
	{"tx_1519b_2047b_frames", offsetof(struct tx_port_stats,
				tx_1519b_2047b_frames)},
	{"tx_2048b_4095b_frames", offsetof(struct tx_port_stats,
				tx_2048b_4095b_frames)},
	{"tx_4096b_9216b_frames", offsetof(struct tx_port_stats,
				tx_4096b_9216b_frames)},
	{"tx_9217b_16383b_frames", offsetof(struct tx_port_stats,
				tx_9217b_16383b_frames)},
	{"tx_good_frames", offsetof(struct tx_port_stats,
				tx_good_frames)},
	{"tx_total_frames", offsetof(struct tx_port_stats,
				tx_total_frames)},
	{"tx_ucast_frames", offsetof(struct tx_port_stats,
				tx_ucast_frames)},
	{"tx_mcast_frames", offsetof(struct tx_port_stats,
				tx_mcast_frames)},
	{"tx_bcast_frames", offsetof(struct tx_port_stats,
				tx_bcast_frames)},
	{"tx_pause_frames", offsetof(struct tx_port_stats,
				tx_pause_frames)},
	{"tx_pfc_frames", offsetof(struct tx_port_stats,
				tx_pfc_frames)},
	{"tx_jabber_frames", offsetof(struct tx_port_stats,
				tx_jabber_frames)},
	{"tx_fcs_err_frames", offsetof(struct tx_port_stats,
				tx_fcs_err_frames)},
	{"tx_control_frames", offsetof(struct tx_port_stats,
				tx_control_frames)},
	{"tx_oversz_frames", offsetof(struct tx_port_stats,
				tx_oversz_frames)},
	{"tx_single_dfrl_frames", offsetof(struct tx_port_stats,
				tx_single_dfrl_frames)},
	{"tx_multi_dfrl_frames", offsetof(struct tx_port_stats,
				tx_multi_dfrl_frames)},
	{"tx_single_coll_frames", offsetof(struct tx_port_stats,
				tx_single_coll_frames)},
	{"tx_multi_coll_frames", offsetof(struct tx_port_stats,
				tx_multi_coll_frames)},
	{"tx_late_coll_frames", offsetof(struct tx_port_stats,
				tx_late_coll_frames)},
	{"tx_excessive_coll_frames", offsetof(struct tx_port_stats,
				tx_excessive_coll_frames)},
	{"tx_frag_frames", offsetof(struct tx_port_stats,
				tx_frag_frames)},
	{"tx_err", offsetof(struct tx_port_stats,
				tx_err)},
	{"tx_tagged_frames", offsetof(struct tx_port_stats,
				tx_tagged_frames)},
	{"tx_dbl_tagged_frames", offsetof(struct tx_port_stats,
				tx_dbl_tagged_frames)},
	{"tx_runt_frames", offsetof(struct tx_port_stats,
				tx_runt_frames)},
	{"tx_fifo_underruns", offsetof(struct tx_port_stats,
				tx_fifo_underruns)},
	{"tx_eee_lpi_events", offsetof(struct tx_port_stats,
				tx_eee_lpi_events)},
	{"tx_eee_lpi_duration", offsetof(struct tx_port_stats,
				tx_eee_lpi_duration)},
	{"tx_total_collisions", offsetof(struct tx_port_stats,
				tx_total_collisions)},
	{"tx_bytes", offsetof(struct tx_port_stats,
				tx_bytes)},
	{"tx_pfc_ena_frames_pri0", offsetof(struct tx_port_stats,
				tx_pfc_ena_frames_pri0)},
	{"tx_pfc_ena_frames_pri1", offsetof(struct tx_port_stats,
				tx_pfc_ena_frames_pri1)},
	{"tx_pfc_ena_frames_pri2", offsetof(struct tx_port_stats,
				tx_pfc_ena_frames_pri2)},
	{"tx_pfc_ena_frames_pri3", offsetof(struct tx_port_stats,
				tx_pfc_ena_frames_pri3)},
	{"tx_pfc_ena_frames_pri4", offsetof(struct tx_port_stats,
				tx_pfc_ena_frames_pri4)},
	{"tx_pfc_ena_frames_pri5", offsetof(struct tx_port_stats,
				tx_pfc_ena_frames_pri5)},
	{"tx_pfc_ena_frames_pri6", offsetof(struct tx_port_stats,
				tx_pfc_ena_frames_pri6)},
	{"tx_pfc_ena_frames_pri7", offsetof(struct tx_port_stats,
				tx_pfc_ena_frames_pri7)},
	{"tx_llfc_logical_msgs", offsetof(struct tx_port_stats,
				tx_llfc_logical_msgs)},
	{"tx_hcfc_msgs", offsetof(struct tx_port_stats,
				tx_hcfc_msgs)},
	{"tx_xthol_frames", offsetof(struct tx_port_stats,
				tx_xthol_frames)},
	{"tx_stat_discard", offsetof(struct tx_port_stats,
				tx_stat_discard)},
	{"tx_stat_error", offsetof(struct tx_port_stats,
				tx_stat_error)},
};

static const struct bnxt_xstats_name_off bnxt_func_stats_strings[] = {
	{"tx_ucast_pkts", offsetof(struct hwrm_func_qstats_output,
				tx_ucast_pkts)},
	{"tx_mcast_pkts", offsetof(struct hwrm_func_qstats_output,
				tx_mcast_pkts)},
	{"tx_bcast_pkts", offsetof(struct hwrm_func_qstats_output,
				tx_bcast_pkts)},
	{"tx_discard_pkts", offsetof(struct hwrm_func_qstats_output,
				tx_discard_pkts)},
	{"tx_drop_pkts", offsetof(struct hwrm_func_qstats_output,
				tx_drop_pkts)},
	{"tx_ucast_bytes", offsetof(struct hwrm_func_qstats_output,
				tx_ucast_bytes)},
	{"tx_mcast_bytes", offsetof(struct hwrm_func_qstats_output,
				tx_mcast_bytes)},
	{"tx_bcast_bytes", offsetof(struct hwrm_func_qstats_output,
				tx_bcast_bytes)},
	{"rx_ucast_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_ucast_pkts)},
	{"rx_mcast_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_mcast_pkts)},
	{"rx_bcast_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_bcast_pkts)},
	{"rx_discard_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_discard_pkts)},
	{"rx_drop_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_drop_pkts)},
	{"rx_ucast_bytes", offsetof(struct hwrm_func_qstats_output,
				rx_ucast_bytes)},
	{"rx_mcast_bytes", offsetof(struct hwrm_func_qstats_output,
				rx_mcast_bytes)},
	{"rx_bcast_bytes", offsetof(struct hwrm_func_qstats_output,
				rx_bcast_bytes)},
	{"rx_agg_pkts", offsetof(struct hwrm_func_qstats_output,
				rx_agg_pkts)},
	{"rx_agg_bytes", offsetof(struct hwrm_func_qstats_output,
				rx_agg_bytes)},
	{"rx_agg_events", offsetof(struct hwrm_func_qstats_output,
				rx_agg_events)},
	{"rx_agg_aborts", offsetof(struct hwrm_func_qstats_output,
				rx_agg_aborts)},
};


static const struct bnxt_xstats_name_off bnxt_rx_ext_stats_strings[] = {
	{"link_down_events", offsetof(struct rx_port_stats_ext,
				link_down_events)},
	{"continuous_pause_events", offsetof(struct rx_port_stats_ext,
				continuous_pause_events)},
	{"resume_pause_events", offsetof(struct rx_port_stats_ext,
				resume_pause_events)},
	{"continuous_roce_pause_events", offsetof(struct rx_port_stats_ext,
				continuous_roce_pause_events)},
	{"resume_roce_pause_events", offsetof(struct rx_port_stats_ext,
				resume_roce_pause_events)},
	{"rx_bytes_cos0", offsetof(struct rx_port_stats_ext,
				rx_bytes_cos0)},
	{"rx_bytes_cos1", offsetof(struct rx_port_stats_ext,
				rx_bytes_cos1)},
	{"rx_bytes_cos2", offsetof(struct rx_port_stats_ext,
				rx_bytes_cos2)},
	{"rx_bytes_cos3", offsetof(struct rx_port_stats_ext,
				rx_bytes_cos3)},
	{"rx_bytes_cos4", offsetof(struct rx_port_stats_ext,
				rx_bytes_cos4)},
	{"rx_bytes_cos5", offsetof(struct rx_port_stats_ext,
				rx_bytes_cos5)},
	{"rx_bytes_cos6", offsetof(struct rx_port_stats_ext,
				rx_bytes_cos6)},
	{"rx_bytes_cos7", offsetof(struct rx_port_stats_ext,
				rx_bytes_cos7)},
	{"rx_packets_cos0", offsetof(struct rx_port_stats_ext,
				rx_packets_cos0)},
	{"rx_packets_cos1", offsetof(struct rx_port_stats_ext,
				rx_packets_cos1)},
	{"rx_packets_cos2", offsetof(struct rx_port_stats_ext,
				rx_packets_cos2)},
	{"rx_packets_cos3", offsetof(struct rx_port_stats_ext,
				rx_packets_cos3)},
	{"rx_packets_cos4", offsetof(struct rx_port_stats_ext,
				rx_packets_cos4)},
	{"rx_packets_cos5", offsetof(struct rx_port_stats_ext,
				rx_packets_cos5)},
	{"rx_packets_cos6", offsetof(struct rx_port_stats_ext,
				rx_packets_cos6)},
	{"rx_packets_cos7", offsetof(struct rx_port_stats_ext,
				rx_packets_cos7)},
	{"pfc_pri0_rx_duration_us", offsetof(struct rx_port_stats_ext,
				pfc_pri0_rx_duration_us)},
	{"pfc_pri0_rx_transitions", offsetof(struct rx_port_stats_ext,
				pfc_pri0_rx_transitions)},
	{"pfc_pri1_rx_duration_us", offsetof(struct rx_port_stats_ext,
				pfc_pri1_rx_duration_us)},
	{"pfc_pri1_rx_transitions", offsetof(struct rx_port_stats_ext,
				pfc_pri1_rx_transitions)},
	{"pfc_pri2_rx_duration_us", offsetof(struct rx_port_stats_ext,
				pfc_pri2_rx_duration_us)},
	{"pfc_pri2_rx_transitions", offsetof(struct rx_port_stats_ext,
				pfc_pri2_rx_transitions)},
	{"pfc_pri3_rx_duration_us", offsetof(struct rx_port_stats_ext,
				pfc_pri3_rx_duration_us)},
	{"pfc_pri3_rx_transitions", offsetof(struct rx_port_stats_ext,
				pfc_pri3_rx_transitions)},
	{"pfc_pri4_rx_duration_us", offsetof(struct rx_port_stats_ext,
				pfc_pri4_rx_duration_us)},
	{"pfc_pri4_rx_transitions", offsetof(struct rx_port_stats_ext,
				pfc_pri4_rx_transitions)},
	{"pfc_pri5_rx_duration_us", offsetof(struct rx_port_stats_ext,
				pfc_pri5_rx_duration_us)},
	{"pfc_pri5_rx_transitions", offsetof(struct rx_port_stats_ext,
				pfc_pri5_rx_transitions)},
	{"pfc_pri6_rx_duration_us", offsetof(struct rx_port_stats_ext,
				pfc_pri6_rx_duration_us)},
	{"pfc_pri6_rx_transitions", offsetof(struct rx_port_stats_ext,
				pfc_pri6_rx_transitions)},
	{"pfc_pri7_rx_duration_us", offsetof(struct rx_port_stats_ext,
				pfc_pri7_rx_duration_us)},
	{"pfc_pri7_rx_transitions", offsetof(struct rx_port_stats_ext,
				pfc_pri7_rx_transitions)},
	{"rx_bits",		offsetof(struct rx_port_stats_ext,
				rx_bits)},
	{"rx_buffer_passed_threshold", offsetof(struct rx_port_stats_ext,
				rx_buffer_passed_threshold)},
	{"rx_pcs_symbol_err",	offsetof(struct rx_port_stats_ext,
				rx_pcs_symbol_err)},
	{"rx_corrected_bits",	offsetof(struct rx_port_stats_ext,
				rx_corrected_bits)},
	{"rx_discard_bytes_cos0", offsetof(struct rx_port_stats_ext,
				rx_discard_bytes_cos0)},
	{"rx_discard_bytes_cos1", offsetof(struct rx_port_stats_ext,
				rx_discard_bytes_cos1)},
	{"rx_discard_bytes_cos2", offsetof(struct rx_port_stats_ext,
				rx_discard_bytes_cos2)},
	{"rx_discard_bytes_cos3", offsetof(struct rx_port_stats_ext,
				rx_discard_bytes_cos3)},
	{"rx_discard_bytes_cos4", offsetof(struct rx_port_stats_ext,
				rx_discard_bytes_cos4)},
	{"rx_discard_bytes_cos5", offsetof(struct rx_port_stats_ext,
				rx_discard_bytes_cos5)},
	{"rx_discard_bytes_cos6", offsetof(struct rx_port_stats_ext,
				rx_discard_bytes_cos6)},
	{"rx_discard_bytes_cos7", offsetof(struct rx_port_stats_ext,
				rx_discard_bytes_cos7)},
	{"rx_discard_packets_cos0", offsetof(struct rx_port_stats_ext,
				rx_discard_packets_cos0)},
	{"rx_discard_packets_cos1", offsetof(struct rx_port_stats_ext,
				rx_discard_packets_cos1)},
	{"rx_discard_packets_cos2", offsetof(struct rx_port_stats_ext,
				rx_discard_packets_cos2)},
	{"rx_discard_packets_cos3", offsetof(struct rx_port_stats_ext,
				rx_discard_packets_cos3)},
	{"rx_discard_packets_cos4", offsetof(struct rx_port_stats_ext,
				rx_discard_packets_cos4)},
	{"rx_discard_packets_cos5", offsetof(struct rx_port_stats_ext,
				rx_discard_packets_cos5)},
	{"rx_discard_packets_cos6", offsetof(struct rx_port_stats_ext,
				rx_discard_packets_cos6)},
	{"rx_discard_packets_cos7", offsetof(struct rx_port_stats_ext,
				rx_discard_packets_cos7)},
};

static const struct bnxt_xstats_name_off bnxt_tx_ext_stats_strings[] = {
	{"tx_bytes_cos0", offsetof(struct tx_port_stats_ext,
				tx_bytes_cos0)},
	{"tx_bytes_cos1", offsetof(struct tx_port_stats_ext,
				tx_bytes_cos1)},
	{"tx_bytes_cos2", offsetof(struct tx_port_stats_ext,
				tx_bytes_cos2)},
	{"tx_bytes_cos3", offsetof(struct tx_port_stats_ext,
				tx_bytes_cos3)},
	{"tx_bytes_cos4", offsetof(struct tx_port_stats_ext,
				tx_bytes_cos4)},
	{"tx_bytes_cos5", offsetof(struct tx_port_stats_ext,
				tx_bytes_cos5)},
	{"tx_bytes_cos6", offsetof(struct tx_port_stats_ext,
				tx_bytes_cos6)},
	{"tx_bytes_cos7", offsetof(struct tx_port_stats_ext,
				tx_bytes_cos7)},
	{"tx_packets_cos0", offsetof(struct tx_port_stats_ext,
				tx_packets_cos0)},
	{"tx_packets_cos1", offsetof(struct tx_port_stats_ext,
				tx_packets_cos1)},
	{"tx_packets_cos2", offsetof(struct tx_port_stats_ext,
				tx_packets_cos2)},
	{"tx_packets_cos3", offsetof(struct tx_port_stats_ext,
				tx_packets_cos3)},
	{"tx_packets_cos4", offsetof(struct tx_port_stats_ext,
				tx_packets_cos4)},
	{"tx_packets_cos5", offsetof(struct tx_port_stats_ext,
				tx_packets_cos5)},
	{"tx_packets_cos6", offsetof(struct tx_port_stats_ext,
				tx_packets_cos6)},
	{"tx_packets_cos7", offsetof(struct tx_port_stats_ext,
				tx_packets_cos7)},
	{"pfc_pri0_tx_duration_us", offsetof(struct tx_port_stats_ext,
				pfc_pri0_tx_duration_us)},
	{"pfc_pri0_tx_transitions", offsetof(struct tx_port_stats_ext,
				pfc_pri0_tx_transitions)},
	{"pfc_pri1_tx_duration_us", offsetof(struct tx_port_stats_ext,
				pfc_pri1_tx_duration_us)},
	{"pfc_pri1_tx_transitions", offsetof(struct tx_port_stats_ext,
				pfc_pri1_tx_transitions)},
	{"pfc_pri2_tx_duration_us", offsetof(struct tx_port_stats_ext,
				pfc_pri2_tx_duration_us)},
	{"pfc_pri2_tx_transitions", offsetof(struct tx_port_stats_ext,
				pfc_pri2_tx_transitions)},
	{"pfc_pri3_tx_duration_us", offsetof(struct tx_port_stats_ext,
				pfc_pri3_tx_duration_us)},
	{"pfc_pri3_tx_transitions", offsetof(struct tx_port_stats_ext,
				pfc_pri3_tx_transitions)},
	{"pfc_pri4_tx_duration_us", offsetof(struct tx_port_stats_ext,
				pfc_pri4_tx_duration_us)},
	{"pfc_pri4_tx_transitions", offsetof(struct tx_port_stats_ext,
				pfc_pri4_tx_transitions)},
	{"pfc_pri5_tx_duration_us", offsetof(struct tx_port_stats_ext,
				pfc_pri5_tx_duration_us)},
	{"pfc_pri5_tx_transitions", offsetof(struct tx_port_stats_ext,
				pfc_pri5_tx_transitions)},
	{"pfc_pri6_tx_duration_us", offsetof(struct tx_port_stats_ext,
				pfc_pri6_tx_duration_us)},
	{"pfc_pri6_tx_transitions", offsetof(struct tx_port_stats_ext,
				pfc_pri6_tx_transitions)},
	{"pfc_pri7_tx_duration_us", offsetof(struct tx_port_stats_ext,
				pfc_pri7_tx_duration_us)},
	{"pfc_pri7_tx_transitions", offsetof(struct tx_port_stats_ext,
				pfc_pri7_tx_transitions)},
};

/*
 * Statistics functions
 */

void bnxt_free_stats(struct bnxt *bp)
{
	int i;

	for (i = 0; i < (int)bp->tx_cp_nr_rings; i++) {
		struct bnxt_tx_queue *txq = bp->tx_queues[i];

		bnxt_free_txq_stats(txq);
	}
	for (i = 0; i < (int)bp->rx_cp_nr_rings; i++) {
		struct bnxt_rx_queue *rxq = bp->rx_queues[i];

		bnxt_free_rxq_stats(rxq);
	}
}

static void bnxt_fill_rte_eth_stats(struct rte_eth_stats *stats,
				    struct bnxt_ring_stats *ring_stats,
				    unsigned int i, bool rx)
{
	if (rx) {
		stats->q_ipackets[i] = ring_stats->rx_ucast_pkts;
		stats->q_ipackets[i] += ring_stats->rx_mcast_pkts;
		stats->q_ipackets[i] += ring_stats->rx_bcast_pkts;

		stats->ipackets += stats->q_ipackets[i];

		stats->q_ibytes[i] = ring_stats->rx_ucast_bytes;
		stats->q_ibytes[i] += ring_stats->rx_mcast_bytes;
		stats->q_ibytes[i] += ring_stats->rx_bcast_bytes;

		stats->ibytes += stats->q_ibytes[i];

		stats->q_errors[i] = ring_stats->rx_discard_pkts;
		stats->q_errors[i] += ring_stats->rx_error_pkts;

		stats->imissed += ring_stats->rx_discard_pkts;
		stats->ierrors += ring_stats->rx_error_pkts;
	} else {
		stats->q_opackets[i] = ring_stats->tx_ucast_pkts;
		stats->q_opackets[i] += ring_stats->tx_mcast_pkts;
		stats->q_opackets[i] += ring_stats->tx_bcast_pkts;

		stats->opackets += stats->q_opackets[i];

		stats->q_obytes[i] = ring_stats->tx_ucast_bytes;
		stats->q_obytes[i] += ring_stats->tx_mcast_bytes;
		stats->q_obytes[i] += ring_stats->tx_bcast_bytes;

		stats->obytes += stats->q_obytes[i];

		stats->oerrors += ring_stats->tx_discard_pkts;
	}
}

int bnxt_stats_get_op(struct rte_eth_dev *eth_dev,
		      struct rte_eth_stats *bnxt_stats)
{
	int rc = 0;
	unsigned int i;
	struct bnxt *bp = eth_dev->data->dev_private;
	unsigned int num_q_stats;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	if (!eth_dev->data->dev_started)
		return -EIO;

	num_q_stats = RTE_MIN(bp->rx_cp_nr_rings,
			      (unsigned int)RTE_ETHDEV_QUEUE_STAT_CNTRS);

	for (i = 0; i < num_q_stats; i++) {
		struct bnxt_rx_queue *rxq = bp->rx_queues[i];
		struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
		struct bnxt_ring_stats ring_stats = {0};

		if (!rxq->rx_started)
			continue;

		rc = bnxt_hwrm_ring_stats(bp, cpr->hw_stats_ctx_id, i,
					  &ring_stats, true);
		if (unlikely(rc))
			return rc;

		bnxt_fill_rte_eth_stats(bnxt_stats, &ring_stats, i, true);
		bnxt_stats->rx_nombuf +=
				rte_atomic64_read(&rxq->rx_mbuf_alloc_fail);
	}

	num_q_stats = RTE_MIN(bp->tx_cp_nr_rings,
			      (unsigned int)RTE_ETHDEV_QUEUE_STAT_CNTRS);

	for (i = 0; i < num_q_stats; i++) {
		struct bnxt_tx_queue *txq = bp->tx_queues[i];
		struct bnxt_cp_ring_info *cpr = txq->cp_ring;
		struct bnxt_ring_stats ring_stats = {0};

		if (!txq->tx_started)
			continue;

		rc = bnxt_hwrm_ring_stats(bp, cpr->hw_stats_ctx_id, i,
					  &ring_stats, false);
		if (unlikely(rc))
			return rc;

		bnxt_fill_rte_eth_stats(bnxt_stats, &ring_stats, i, false);
	}

	return rc;
}

static void bnxt_clear_prev_stat(struct bnxt *bp)
{
	/*
	 * Clear the cached values of stats returned by HW in the previous
	 * get operation.
	 */
	memset(bp->prev_rx_ring_stats, 0, sizeof(struct bnxt_ring_stats) * bp->rx_cp_nr_rings);
	memset(bp->prev_tx_ring_stats, 0, sizeof(struct bnxt_ring_stats) * bp->tx_cp_nr_rings);
}

int bnxt_stats_reset_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = eth_dev->data->dev_private;
	unsigned int i;
	int ret;

	ret = is_bnxt_in_error(bp);
	if (ret)
		return ret;

	if (!eth_dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "Device Initialization not complete!\n");
		return -EINVAL;
	}

	ret = bnxt_clear_all_hwrm_stat_ctxs(bp);
	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		struct bnxt_rx_queue *rxq = bp->rx_queues[i];

		rte_atomic64_clear(&rxq->rx_mbuf_alloc_fail);
	}

	bnxt_clear_prev_stat(bp);

	return ret;
}

static void bnxt_fill_func_qstats(struct hwrm_func_qstats_output *func_qstats,
				  struct bnxt_ring_stats *ring_stats,
				  bool rx)
{
	if (rx) {
		func_qstats->rx_ucast_pkts += ring_stats->rx_ucast_pkts;
		func_qstats->rx_mcast_pkts += ring_stats->rx_mcast_pkts;
		func_qstats->rx_bcast_pkts += ring_stats->rx_bcast_pkts;

		func_qstats->rx_ucast_bytes += ring_stats->rx_ucast_bytes;
		func_qstats->rx_mcast_bytes += ring_stats->rx_mcast_bytes;
		func_qstats->rx_bcast_bytes += ring_stats->rx_bcast_bytes;

		func_qstats->rx_discard_pkts += ring_stats->rx_discard_pkts;
		func_qstats->rx_drop_pkts += ring_stats->rx_error_pkts;

		func_qstats->rx_agg_pkts += ring_stats->rx_agg_pkts;
		func_qstats->rx_agg_bytes += ring_stats->rx_agg_bytes;
		func_qstats->rx_agg_events += ring_stats->rx_agg_events;
		func_qstats->rx_agg_aborts += ring_stats->rx_agg_aborts;
	} else {
		func_qstats->tx_ucast_pkts += ring_stats->tx_ucast_pkts;
		func_qstats->tx_mcast_pkts += ring_stats->tx_mcast_pkts;
		func_qstats->tx_bcast_pkts += ring_stats->tx_bcast_pkts;

		func_qstats->tx_ucast_bytes += ring_stats->tx_ucast_bytes;
		func_qstats->tx_mcast_bytes += ring_stats->tx_mcast_bytes;
		func_qstats->tx_bcast_bytes += ring_stats->tx_bcast_bytes;

		func_qstats->tx_drop_pkts += ring_stats->tx_error_pkts;
		func_qstats->tx_discard_pkts += ring_stats->tx_discard_pkts;
	}
}

int bnxt_dev_xstats_get_op(struct rte_eth_dev *eth_dev,
			   struct rte_eth_xstat *xstats, unsigned int n)
{
	struct bnxt *bp = eth_dev->data->dev_private;
	unsigned int count, i;
	unsigned int rx_port_stats_ext_cnt;
	unsigned int tx_port_stats_ext_cnt;
	unsigned int stat_size = sizeof(uint64_t);
	struct hwrm_func_qstats_output func_qstats = {0};
	unsigned int stat_count;
	int rc;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	stat_count = RTE_DIM(bnxt_rx_stats_strings) +
		RTE_DIM(bnxt_tx_stats_strings) +
		RTE_DIM(bnxt_func_stats_strings) +
		RTE_DIM(bnxt_rx_ext_stats_strings) +
		RTE_DIM(bnxt_tx_ext_stats_strings) +
		bnxt_flow_stats_cnt(bp);

	if (n < stat_count || xstats == NULL)
		return stat_count;

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		struct bnxt_rx_queue *rxq = bp->rx_queues[i];
		struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
		struct bnxt_ring_stats ring_stats = {0};

		if (!rxq->rx_started)
			continue;

		rc = bnxt_hwrm_ring_stats(bp, cpr->hw_stats_ctx_id, i,
					  &ring_stats, true);
		if (unlikely(rc))
			return rc;

		bnxt_fill_func_qstats(&func_qstats, &ring_stats, true);
	}

	for (i = 0; i < bp->tx_cp_nr_rings; i++) {
		struct bnxt_tx_queue *txq = bp->tx_queues[i];
		struct bnxt_cp_ring_info *cpr = txq->cp_ring;
		struct bnxt_ring_stats ring_stats = {0};

		if (!txq->tx_started)
			continue;

		rc = bnxt_hwrm_ring_stats(bp, cpr->hw_stats_ctx_id, i,
					  &ring_stats, false);
		if (unlikely(rc))
			return rc;

		bnxt_fill_func_qstats(&func_qstats, &ring_stats, false);
	}

	bnxt_hwrm_port_qstats(bp);
	bnxt_hwrm_ext_port_qstats(bp);
	rx_port_stats_ext_cnt = RTE_MIN(RTE_DIM(bnxt_rx_ext_stats_strings),
					(bp->fw_rx_port_stats_ext_size /
					 stat_size));
	tx_port_stats_ext_cnt = RTE_MIN(RTE_DIM(bnxt_tx_ext_stats_strings),
					(bp->fw_tx_port_stats_ext_size /
					 stat_size));

	memset(xstats, 0, sizeof(*xstats) * n);

	count = 0;
	for (i = 0; i < RTE_DIM(bnxt_rx_stats_strings); i++) {
		uint64_t *rx_stats = (uint64_t *)bp->hw_rx_port_stats;
		xstats[count].id = count;
		xstats[count].value = rte_le_to_cpu_64(
				*(uint64_t *)((char *)rx_stats +
				bnxt_rx_stats_strings[i].offset));
		count++;
	}

	for (i = 0; i < RTE_DIM(bnxt_tx_stats_strings); i++) {
		uint64_t *tx_stats = (uint64_t *)bp->hw_tx_port_stats;
		xstats[count].id = count;
		xstats[count].value = rte_le_to_cpu_64(
				 *(uint64_t *)((char *)tx_stats +
				bnxt_tx_stats_strings[i].offset));
		count++;
	}

	for (i = 0; i < RTE_DIM(bnxt_func_stats_strings); i++) {
		xstats[count].id = count;
		xstats[count].value = *(uint64_t *)((char *)&func_qstats +
					 bnxt_func_stats_strings[i].offset);
		count++;
	}

	for (i = 0; i < rx_port_stats_ext_cnt; i++) {
		uint64_t *rx_stats_ext = (uint64_t *)bp->hw_rx_port_stats_ext;

		xstats[count].value = rte_le_to_cpu_64
					(*(uint64_t *)((char *)rx_stats_ext +
					 bnxt_rx_ext_stats_strings[i].offset));

		count++;
	}

	for (i = 0; i < tx_port_stats_ext_cnt; i++) {
		uint64_t *tx_stats_ext = (uint64_t *)bp->hw_tx_port_stats_ext;

		xstats[count].value = rte_le_to_cpu_64
					(*(uint64_t *)((char *)tx_stats_ext +
					 bnxt_tx_ext_stats_strings[i].offset));
		count++;
	}

	if (bp->fw_cap & BNXT_FW_CAP_ADV_FLOW_COUNTERS &&
	    bp->fw_cap & BNXT_FW_CAP_ADV_FLOW_MGMT &&
	    BNXT_FLOW_XSTATS_EN(bp)) {
		int j;

		i = 0;
		for (j = 0; j < bp->max_vnics; j++) {
			struct bnxt_filter_info *filter;
			struct bnxt_vnic_info *vnic;
			struct rte_flow *flow;

			vnic = &bp->vnic_info[j];
			if (vnic && vnic->fw_vnic_id == INVALID_VNIC_ID)
				continue;

			if (STAILQ_EMPTY(&vnic->flow_list))
				continue;

			STAILQ_FOREACH(flow, &vnic->flow_list, next) {
				if (!flow || !flow->filter)
					continue;

				filter = flow->filter;
				xstats[count].id = count;
				xstats[count].value =
					filter->hw_stats.bytes;
				count++;
				xstats[count].id = count;
				xstats[count].value =
					filter->hw_stats.packets;
				count++;
				if (++i > bp->max_l2_ctx)
					break;
			}
			if (i > bp->max_l2_ctx)
				break;
		}
	}

	return stat_count;
}

int bnxt_flow_stats_cnt(struct bnxt *bp)
{
	if (bp->fw_cap & BNXT_FW_CAP_ADV_FLOW_COUNTERS &&
	    bp->fw_cap & BNXT_FW_CAP_ADV_FLOW_MGMT &&
	    BNXT_FLOW_XSTATS_EN(bp)) {
		struct bnxt_xstats_name_off flow_bytes[bp->max_l2_ctx];
		struct bnxt_xstats_name_off flow_pkts[bp->max_l2_ctx];

		return RTE_DIM(flow_bytes) + RTE_DIM(flow_pkts);
	}

	return 0;
}

int bnxt_dev_xstats_get_names_op(struct rte_eth_dev *eth_dev,
		struct rte_eth_xstat_name *xstats_names,
		unsigned int size)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	const unsigned int stat_cnt = RTE_DIM(bnxt_rx_stats_strings) +
				RTE_DIM(bnxt_tx_stats_strings) +
				RTE_DIM(bnxt_func_stats_strings) +
				RTE_DIM(bnxt_rx_ext_stats_strings) +
				RTE_DIM(bnxt_tx_ext_stats_strings) +
				bnxt_flow_stats_cnt(bp);
	unsigned int i, count = 0;
	int rc;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	if (xstats_names == NULL || size < stat_cnt)
		return stat_cnt;

	for (i = 0; i < RTE_DIM(bnxt_rx_stats_strings); i++) {
		strlcpy(xstats_names[count].name,
			bnxt_rx_stats_strings[i].name,
			sizeof(xstats_names[count].name));
		count++;
	}

	for (i = 0; i < RTE_DIM(bnxt_tx_stats_strings); i++) {
		strlcpy(xstats_names[count].name,
			bnxt_tx_stats_strings[i].name,
			sizeof(xstats_names[count].name));
		count++;
	}

	for (i = 0; i < RTE_DIM(bnxt_func_stats_strings); i++) {
		strlcpy(xstats_names[count].name,
			bnxt_func_stats_strings[i].name,
			sizeof(xstats_names[count].name));
		count++;
	}

	for (i = 0; i < RTE_DIM(bnxt_rx_ext_stats_strings); i++) {
		strlcpy(xstats_names[count].name,
			bnxt_rx_ext_stats_strings[i].name,
			sizeof(xstats_names[count].name));

		count++;
	}

	for (i = 0; i < RTE_DIM(bnxt_tx_ext_stats_strings); i++) {
		strlcpy(xstats_names[count].name,
			bnxt_tx_ext_stats_strings[i].name,
			sizeof(xstats_names[count].name));

		count++;
	}

	if (bp->fw_cap & BNXT_FW_CAP_ADV_FLOW_COUNTERS &&
	    bp->fw_cap & BNXT_FW_CAP_ADV_FLOW_MGMT &&
	    BNXT_FLOW_XSTATS_EN(bp)) {
		for (i = 0; i < bp->max_l2_ctx; i++) {
			char buf[RTE_ETH_XSTATS_NAME_SIZE];

			sprintf(buf, "flow_%d_bytes", i);
			strlcpy(xstats_names[count].name, buf,
				sizeof(xstats_names[count].name));
			count++;

			sprintf(buf, "flow_%d_packets", i);
			strlcpy(xstats_names[count].name, buf,
				sizeof(xstats_names[count].name));

			count++;
		}
	}

	return stat_cnt;
}

int bnxt_dev_xstats_reset_op(struct rte_eth_dev *eth_dev)
{
	struct bnxt *bp = eth_dev->data->dev_private;
	int ret;

	ret = is_bnxt_in_error(bp);
	if (ret)
		return ret;

	if (BNXT_VF(bp) || !BNXT_SINGLE_PF(bp) ||
	    !(bp->flags & BNXT_FLAG_PORT_STATS)) {
		PMD_DRV_LOG(ERR, "Operation not supported\n");
		return -ENOTSUP;
	}

	ret = bnxt_hwrm_port_clr_stats(bp);
	if (ret != 0)
		PMD_DRV_LOG(ERR, "Failed to reset xstats: %s\n",
			    strerror(-ret));

	bnxt_clear_prev_stat(bp);

	return ret;
}

/* Update the input context memory with the flow counter IDs
 * of the flows that we are interested in.
 * Also, update the output tables with the current local values
 * since that is what will be used by FW to accumulate
 */
static void bnxt_update_fc_pre_qstat(uint32_t *in_tbl,
				     uint64_t *out_tbl,
				     struct bnxt_filter_info *filter,
				     uint32_t *ptbl_cnt)
{
	uint32_t in_tbl_cnt = *ptbl_cnt;

	in_tbl[in_tbl_cnt] = filter->flow_id;
	out_tbl[2 * in_tbl_cnt] = filter->hw_stats.packets;
	out_tbl[2 * in_tbl_cnt + 1] = filter->hw_stats.bytes;
	in_tbl_cnt++;
	*ptbl_cnt = in_tbl_cnt;
}

/* Post issuing counter_qstats cmd, update the driver's local stat
 * entries with the values DMA-ed by FW in the output table
 */
static void bnxt_update_fc_post_qstat(struct bnxt_filter_info *filter,
				      uint64_t *out_tbl,
				      uint32_t out_tbl_idx)
{
	filter->hw_stats.packets = out_tbl[2 * out_tbl_idx];
	filter->hw_stats.bytes = out_tbl[(2 * out_tbl_idx) + 1];
}

static int bnxt_update_fc_tbl(struct bnxt *bp, uint16_t ctr,
			      struct bnxt_filter_info *en_tbl[],
			      uint16_t in_flow_cnt)
{
	uint32_t *in_rx_tbl;
	uint64_t *out_rx_tbl;
	uint32_t in_rx_tbl_cnt = 0;
	uint32_t out_rx_tbl_cnt = 0;
	int i, rc = 0;

	in_rx_tbl = (uint32_t *)bp->flow_stat->rx_fc_in_tbl.va;
	out_rx_tbl = (uint64_t *)bp->flow_stat->rx_fc_out_tbl.va;

	for (i = 0; i < in_flow_cnt; i++) {
		if (!en_tbl[i])
			continue;

		/* Currently only ingress/Rx flows are supported anyway. */
		bnxt_update_fc_pre_qstat(in_rx_tbl, out_rx_tbl,
					 en_tbl[i], &in_rx_tbl_cnt);
	}

	/* Currently only ingress/Rx flows are supported */
	if (in_rx_tbl_cnt) {
		rc = bnxt_hwrm_cfa_counter_qstats(bp, BNXT_DIR_RX, ctr,
						  in_rx_tbl_cnt);
		if (rc)
			return rc;
	}

	for (i = 0; i < in_flow_cnt; i++) {
		if (!en_tbl[i])
			continue;

		/* Currently only ingress/Rx flows are supported */
		bnxt_update_fc_post_qstat(en_tbl[i], out_rx_tbl,
					  out_rx_tbl_cnt);
		out_rx_tbl_cnt++;
	}

	return rc;
}

/* Walks through the list which has all the flows
 * requesting for explicit flow counters.
 */
int bnxt_flow_stats_req(struct bnxt *bp)
{
	int i;
	int rc = 0;
	struct rte_flow *flow;
	uint16_t in_flow_tbl_cnt = 0;
	struct bnxt_vnic_info *vnic = NULL;
	struct bnxt_filter_info *valid_en_tbl[bp->flow_stat->max_fc];
	uint16_t counter_type = CFA_COUNTER_CFG_IN_COUNTER_TYPE_FC;

	bnxt_acquire_flow_lock(bp);
	for (i = 0; i < bp->max_vnics; i++) {
		vnic = &bp->vnic_info[i];
		if (vnic && vnic->fw_vnic_id == INVALID_VNIC_ID)
			continue;

		if (STAILQ_EMPTY(&vnic->flow_list))
			continue;

		STAILQ_FOREACH(flow, &vnic->flow_list, next) {
			if (!flow || !flow->filter)
				continue;

			valid_en_tbl[in_flow_tbl_cnt++] = flow->filter;
			if (in_flow_tbl_cnt >= bp->flow_stat->max_fc) {
				rc = bnxt_update_fc_tbl(bp, counter_type,
							valid_en_tbl,
							in_flow_tbl_cnt);
				if (rc)
					goto err;
				in_flow_tbl_cnt = 0;
				continue;
			}
		}
	}

	if (!in_flow_tbl_cnt) {
		bnxt_release_flow_lock(bp);
		goto out;
	}

	rc = bnxt_update_fc_tbl(bp, counter_type, valid_en_tbl,
				in_flow_tbl_cnt);
	if (!rc) {
		bnxt_release_flow_lock(bp);
		return 0;
	}

err:
	/* If cmd fails once, no need of
	 * invoking again every second
	 */
	bnxt_release_flow_lock(bp);
	bnxt_cancel_fc_thread(bp);
out:
	return rc;
}
