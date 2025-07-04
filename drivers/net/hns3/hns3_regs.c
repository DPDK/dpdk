/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <ethdev_pci.h>
#include <rte_io.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"
#include "hns3_rxtx.h"
#include "hns3_regs.h"

#define HNS3_64_BIT_REG_OUTPUT_SIZE (sizeof(uint64_t) / sizeof(uint32_t))

#define HNS3_MAX_MODULES_LEN 512

struct hns3_dirt_reg_entry {
	const char *name;
	uint32_t addr;
};

static const struct hns3_dirt_reg_entry cmdq_reg_list[] = {
	{"cmdq_tx_depth",		HNS3_CMDQ_TX_DEPTH_REG},
	{"cmdq_tx_tail",		HNS3_CMDQ_TX_TAIL_REG},
	{"cmdq_tx_head",		HNS3_CMDQ_TX_HEAD_REG},
	{"cmdq_rx_depth",		HNS3_CMDQ_RX_DEPTH_REG},
	{"cmdq_rx_tail",		HNS3_CMDQ_RX_TAIL_REG},
	{"cmdq_rx_head",		HNS3_CMDQ_RX_HEAD_REG},
	{"vector0_cmdq_src",		HNS3_VECTOR0_CMDQ_SRC_REG},
	{"cmdq_intr_sts",		HNS3_CMDQ_INTR_STS_REG},
	{"cmdq_intr_en",		HNS3_CMDQ_INTR_EN_REG},
	{"cmdq_intr_gen",		HNS3_CMDQ_INTR_GEN_REG},
};

static const struct hns3_dirt_reg_entry common_reg_list[] = {
	{"misc_vector_reg_base",	HNS3_MISC_VECTOR_REG_BASE},
	{"vector0_oter_en",		HNS3_VECTOR0_OTER_EN_REG},
	{"misc_reset_sts",		HNS3_MISC_RESET_STS_REG},
	{"vector0_other_int_sts",	HNS3_VECTOR0_OTHER_INT_STS_REG},
	{"global_reset",		HNS3_GLOBAL_RESET_REG},
	{"fun_rst_ing",			HNS3_FUN_RST_ING},
	{"gro_en",			HNS3_GRO_EN_REG},
};

static const struct hns3_dirt_reg_entry common_vf_reg_list[] = {
	{"misc_vector_reg_base",	HNS3_MISC_VECTOR_REG_BASE},
	{"fun_rst_ing",			HNS3_FUN_RST_ING},
	{"gro_en",			HNS3_GRO_EN_REG},
};

static const struct hns3_dirt_reg_entry ring_reg_list[] = {
	{"ring_rx_bd_num",		HNS3_RING_RX_BD_NUM_REG},
	{"ring_rx_bd_len",		HNS3_RING_RX_BD_LEN_REG},
	{"ring_rx_en",			HNS3_RING_RX_EN_REG},
	{"ring_rx_merge_en",		HNS3_RING_RX_MERGE_EN_REG},
	{"ring_rx_tail",		HNS3_RING_RX_TAIL_REG},
	{"ring_rx_head",		HNS3_RING_RX_HEAD_REG},
	{"ring_rx_fbdnum",		HNS3_RING_RX_FBDNUM_REG},
	{"ring_rx_offset",		HNS3_RING_RX_OFFSET_REG},
	{"ring_rx_fbd_offset",		HNS3_RING_RX_FBD_OFFSET_REG},
	{"ring_rx_stash",		HNS3_RING_RX_STASH_REG},
	{"ring_rx_bd_err",		HNS3_RING_RX_BD_ERR_REG},
	{"ring_tx_bd_num",		HNS3_RING_TX_BD_NUM_REG},
	{"ring_tx_en",			HNS3_RING_TX_EN_REG},
	{"ring_tx_priority",		HNS3_RING_TX_PRIORITY_REG},
	{"ring_tx_tc",			HNS3_RING_TX_TC_REG},
	{"ring_tx_merge_en",		HNS3_RING_TX_MERGE_EN_REG},
	{"ring_tx_tail",		HNS3_RING_TX_TAIL_REG},
	{"ring_tx_head",		HNS3_RING_TX_HEAD_REG},
	{"ring_tx_fbdnum",		HNS3_RING_TX_FBDNUM_REG},
	{"ring_tx_offset",		HNS3_RING_TX_OFFSET_REG},
	{"ring_tx_ebd_num",		HNS3_RING_TX_EBD_NUM_REG},
	{"ring_tx_ebd_offset",		HNS3_RING_TX_EBD_OFFSET_REG},
	{"ring_tx_bd_err",		HNS3_RING_TX_BD_ERR_REG},
	{"ring_en",			HNS3_RING_EN_REG},
};

static const struct hns3_dirt_reg_entry tqp_intr_reg_list[] = {
	{"tqp_intr_ctrl",		HNS3_TQP_INTR_CTRL_REG},
	{"tqp_intr_gl0",		HNS3_TQP_INTR_GL0_REG},
	{"tqp_intr_gl1",		HNS3_TQP_INTR_GL1_REG},
	{"tqp_intr_gl2",		HNS3_TQP_INTR_GL2_REG},
	{"tqp_intr_rl",			HNS3_TQP_INTR_RL_REG},
};

struct hns3_dfx_reg_entry {
	/**
	 * name_v1 -- default register name for all platforms (HIP08/HIP09/newer).
	 * name_v2 -- register name different from the default for HIP09.
	 * If there are more platform with different register name, name_vXX is extended.
	 * If the platform is newer than HIP09, use default name.
	 */
	const char *name_v1;
	const char *name_v2;
};

static struct hns3_dfx_reg_entry regs_32_bit_list[] = {
	{"ssu_common_err_int"},
	{"ssu_port_based_err_int"},
	{"ssu_fifo_overflow_int"},
	{"ssu_ets_tcg_int"},
	{"ssu_bp_status_0"},
	{"ssu_bp_status_1"},

	{"ssu_bp_status_2"},
	{"ssu_bp_status_3"},
	{"ssu_bp_status_4"},
	{"ssu_bp_status_5"},
	{"ssu_mac_tx_pfc_ind"},
	{"ssu_mac_rx_pfc_ind"},

	{"ssu_rx_oq_drop_pkt_cnt"},
	{"ssu_tx_oq_drop_pkt_cnt"},
};

static struct hns3_dfx_reg_entry regs_64_bit_list[] = {
	{"ppp_get_rx_pkt_cnt_l"},
	{"ppp_get_rx_pkt_cnt_h"},
	{"ppp_get_tx_pkt_cnt_l"},
	{"ppp_get_tx_pkt_cnt_h"},
	{"ppp_send_uc_prt2host_pkt_cnt_l"},
	{"ppp_send_uc_prt2host_pkt_cnt_h"},

	{"ppp_send_uc_prt2prt_pkt_cnt_l"},
	{"ppp_send_uc_prt2prt_pkt_cnt_h"},
	{"ppp_send_uc_host2host_pkt_cnt_l"},
	{"ppp_send_uc_host2host_pkt_cnt_h"},
	{"ppp_send_uc_host2prt_pkt_cnt_l"},
	{"ppp_send_uc_host2prt_pkt_cnt_h"},
	{"ppp_send_mc_from_prt_cnt_l"},
	{"ppp_send_mc_from_prt_cnt_h"},
};

static struct hns3_dfx_reg_entry dfx_bios_common_reg_list[] = {
	{"bios_rsv0"},
	{"bp_cpu_state"},
	{"dfx_msix_info_nic_0"},
	{"dfx_msix_info_nic_1"},
	{"dfx_msix_info_nic_2"},
	{"dfx_msix_info_nic_3"},

	{"dfx_msix_info_roce_0"},
	{"dfx_msix_info_roce_1"},
	{"dfx_msix_info_roce_2"},
	{"dfx_msix_info_roce_3"},
	{"bios_rsv1"},
	{"bios_rsv2"},
};

static struct hns3_dfx_reg_entry dfx_ssu_reg_0_list[] = {
	{"dfx_ssu0_rsv0"},
	{"ssu_ets_port_status"},
	{"ssu_ets_tcg_status"},
	{"dfx_ssu0_rsv1"},
	{"dfx_ssu0_rsv2"},
	{"ssu_bp_status_0"},

	{"ssu_bp_status_1"},
	{"ssu_bp_status_2"},
	{"ssu_bp_status_3"},
	{"ssu_bp_status_4"},
	{"ssu_bp_status_5"},
	{"ssu_mac_tx_pfc_ind"},

	{"mac_ssu_rx_pfc_ind"},
	{"btmp_ageing_st_b0"},
	{"btmp_ageing_st_b1"},
	{"btmp_ageing_st_b2"},
	{"dfx_ssu0_rsv3"},
	{"dfx_ssu0_rsv4"},

	{"ssu_full_drop_num"},
	{"ssu_part_drop_num"},
	{"ppp_key_drop_num"},
	{"ppp_rlt_drop_num"},
	{"lo_pri_unicast_rlt_drop_num"},
	{"hi_pri_multicast_rlt_drop_num"},

	{"lo_pri_multicast_rlt_drop_num"},
	{"ncsi_packet_curr_buffer_cnt"},
	{"btmp_ageing_rls_cnt_bank0",		"dfx_ssu0_rsv5"},
	{"btmp_ageing_rls_cnt_bank1",		"dfx_ssu0_rsv6"},
	{"btmp_ageing_rls_cnt_bank2",		"dfx_ssu0_rsv7"},
	{"ssu_mb_rd_rlt_drop_cnt"},

	{"ssu_ppp_mac_key_num_l"},
	{"ssu_ppp_mac_key_num_h"},
	{"ssu_ppp_host_key_num_l"},
	{"ssu_ppp_host_key_num_h"},
	{"ppp_ssu_mac_rlt_num_l"},
	{"ppp_ssu_mac_rlt_num_h"},

	{"ppp_ssu_host_rlt_num_l"},
	{"ppp_ssu_host_rlt_num_h"},
	{"ncsi_rx_packet_in_cnt_l"},
	{"ncsi_rx_packet_in_cnt_h"},
	{"ncsi_tx_packet_out_cnt_l"},
	{"ncsi_tx_packet_out_cnt_h"},

	{"ssu_key_drop_num"},
	{"mb_uncopy_num"},
	{"rx_oq_drop_pkt_cnt"},
	{"tx_oq_drop_pkt_cnt"},
	{"bank_unbalance_drop_cnt"},
	{"bank_unbalance_rx_drop_cnt"},

	{"nic_l2_eer_drop_pkt_cnt"},
	{"roc_l2_eer_drop_pkt_cnt"},
	{"nic_l2_eer_drop_pkt_cnt_rx"},
	{"roc_l2_eer_drop_pkt_cnt_rx"},
	{"rx_oq_glb_drop_pkt_cnt"},
	{"dfx_ssu0_rsv8"},

	{"lo_pri_unicast_cur_cnt"},
	{"hi_pri_multicast_cur_cnt"},
	{"lo_pri_multicast_cur_cnt"},
	{"dfx_ssu0_rsv9"},
	{"dfx_ssu0_rsv10"},
	{"dfx_ssu0_rsv11"},
};

static struct hns3_dfx_reg_entry dfx_ssu_reg_1_list[] = {
	{"dfx_ssu1_prt_id"},
	{"packet_tc_curr_buffer_cnt_0"},
	{"packet_tc_curr_buffer_cnt_1"},
	{"packet_tc_curr_buffer_cnt_2"},
	{"packet_tc_curr_buffer_cnt_3"},
	{"packet_tc_curr_buffer_cnt_4"},

	{"packet_tc_curr_buffer_cnt_5"},
	{"packet_tc_curr_buffer_cnt_6"},
	{"packet_tc_curr_buffer_cnt_7"},
	{"packet_curr_buffer_cnt"},
	{"dfx_ssu1_rsv0"},
	{"dfx_ssu1_rsv1"},

	{"rx_packet_in_cnt_l"},
	{"rx_packet_in_cnt_h"},
	{"rx_packet_out_cnt_l"},
	{"rx_packet_out_cnt_h"},
	{"tx_packet_in_cnt_l"},
	{"tx_packet_in_cnt_h"},

	{"tx_packet_out_cnt_l"},
	{"tx_packet_out_cnt_h"},
	{"roc_rx_packet_in_cnt_l"},
	{"roc_rx_packet_in_cnt_h"},
	{"roc_tx_packet_in_cnt_l"},
	{"roc_tx_packet_in_cnt_h"},

	{"rx_packet_tc_in_cnt_0_l"},
	{"rx_packet_tc_in_cnt_0_h"},
	{"rx_packet_tc_in_cnt_1_l"},
	{"rx_packet_tc_in_cnt_1_h"},
	{"rx_packet_tc_in_cnt_2_l"},
	{"rx_packet_tc_in_cnt_2_h"},

	{"rx_packet_tc_in_cnt_3_l"},
	{"rx_packet_tc_in_cnt_3_h"},
	{"rx_packet_tc_in_cnt_4_l"},
	{"rx_packet_tc_in_cnt_4_h"},
	{"rx_packet_tc_in_cnt_5_l"},
	{"rx_packet_tc_in_cnt_5_h"},

	{"rx_packet_tc_in_cnt_6_l"},
	{"rx_packet_tc_in_cnt_6_h"},
	{"rx_packet_tc_in_cnt_7_l"},
	{"rx_packet_tc_in_cnt_7_h"},
	{"rx_packet_tc_out_cnt_0_l"},
	{"rx_packet_tc_out_cnt_0_h"},

	{"rx_packet_tc_out_cnt_1_l"},
	{"rx_packet_tc_out_cnt_1_h"},
	{"rx_packet_tc_out_cnt_2_l"},
	{"rx_packet_tc_out_cnt_2_h"},
	{"rx_packet_tc_out_cnt_3_l"},
	{"rx_packet_tc_out_cnt_3_h"},

	{"rx_packet_tc_out_cnt_4_l"},
	{"rx_packet_tc_out_cnt_4_h"},
	{"rx_packet_tc_out_cnt_5_l"},
	{"rx_packet_tc_out_cnt_5_h"},
	{"rx_packet_tc_out_cnt_6_l"},
	{"rx_packet_tc_out_cnt_6_h"},

	{"rx_packet_tc_out_cnt_7_l"},
	{"rx_packet_tc_out_cnt_7_h"},
	{"tx_packet_tc_in_cnt_0_l"},
	{"tx_packet_tc_in_cnt_0_h"},
	{"tx_packet_tc_in_cnt_1_l"},
	{"tx_packet_tc_in_cnt_1_h"},

	{"tx_packet_tc_in_cnt_2_l"},
	{"tx_packet_tc_in_cnt_2_h"},
	{"tx_packet_tc_in_cnt_3_l"},
	{"tx_packet_tc_in_cnt_3_h"},
	{"tx_packet_tc_in_cnt_4_l"},
	{"tx_packet_tc_in_cnt_4_h"},

	{"tx_packet_tc_in_cnt_5_l"},
	{"tx_packet_tc_in_cnt_5_h"},
	{"tx_packet_tc_in_cnt_6_l"},
	{"tx_packet_tc_in_cnt_6_h"},
	{"tx_packet_tc_in_cnt_7_l"},
	{"tx_packet_tc_in_cnt_7_h"},

	{"tx_packet_tc_out_cnt_0_l"},
	{"tx_packet_tc_out_cnt_0_h"},
	{"tx_packet_tc_out_cnt_1_l"},
	{"tx_packet_tc_out_cnt_1_h"},
	{"tx_packet_tc_out_cnt_2_l"},
	{"tx_packet_tc_out_cnt_2_h"},

	{"tx_packet_tc_out_cnt_3_l"},
	{"tx_packet_tc_out_cnt_3_h"},
	{"tx_packet_tc_out_cnt_4_l"},
	{"tx_packet_tc_out_cnt_4_h"},
	{"tx_packet_tc_out_cnt_5_l"},
	{"tx_packet_tc_out_cnt_5_h"},

	{"tx_packet_tc_out_cnt_6_l"},
	{"tx_packet_tc_out_cnt_6_h"},
	{"tx_packet_tc_out_cnt_7_l"},
	{"tx_packet_tc_out_cnt_7_h"},
	{"dfx_ssu1_rsv2"},
	{"dfx_ssu1_rsv3"},
};

static struct hns3_dfx_reg_entry dfx_igu_egu_reg_list[] = {
	{"igu_egu_prt_id"},
	{"igu_rx_err_pkt"},
	{"igu_rx_no_sof_pkt"},
	{"egu_tx_1588_short_pkt"},
	{"egu_tx_1588_pkt"},
	{"egu_tx_1588_err_pkt"},

	{"igu_rx_out_l2_pkt"},
	{"igu_rx_out_l3_pkt"},
	{"igu_rx_out_l4_pkt"},
	{"igu_rx_in_l2_pkt"},
	{"igu_rx_in_l3_pkt"},
	{"igu_rx_in_l4_pkt"},

	{"igu_rx_el3e_pkt"},
	{"igu_rx_el4e_pkt"},
	{"igu_rx_l3e_pkt"},
	{"igu_rx_l4e_pkt"},
	{"igu_rx_rocee_pkt"},
	{"igu_rx_out_udp0_pkt"},

	{"igu_rx_in_udp0_pkt"},
	{"igu_egu_rsv0",		"igu_egu_mul_car_drop_pkt_cnt_l"},
	{"igu_egu_rsv1",		"igu_egu_mul_car_drop_pkt_cnt_h"},
	{"igu_egu_rsv2",		"igu_egu_bro_car_drop_pkt_cnt_l"},
	{"igu_egu_rsv3",		"igu_egu_bro_car_drop_pkt_cnt_h"},
	{"igu_egu_rsv4",		"igu_egu_rsv0"},

	{"igu_rx_oversize_pkt_l"},
	{"igu_rx_oversize_pkt_h"},
	{"igu_rx_undersize_pkt_l"},
	{"igu_rx_undersize_pkt_h"},
	{"igu_rx_out_all_pkt_l"},
	{"igu_rx_out_all_pkt_h"},

	{"igu_tx_out_all_pkt_l"},
	{"igu_tx_out_all_pkt_h"},
	{"igu_rx_uni_pkt_l"},
	{"igu_rx_uni_pkt_h"},
	{"igu_rx_multi_pkt_l"},
	{"igu_rx_multi_pkt_h"},

	{"igu_rx_broad_pkt_l"},
	{"igu_rx_broad_pkt_h"},
	{"egu_tx_out_all_pkt_l"},
	{"egu_tx_out_all_pkt_h"},
	{"egu_tx_uni_pkt_l"},
	{"egu_tx_uni_pkt_h"},

	{"egu_tx_multi_pkt_l"},
	{"egu_tx_multi_pkt_h"},
	{"egu_tx_broad_pkt_l"},
	{"egu_tx_broad_pkt_h"},
	{"igu_tx_key_num_l"},
	{"igu_tx_key_num_h"},

	{"igu_rx_non_tun_pkt_l"},
	{"igu_rx_non_tun_pkt_h"},
	{"igu_rx_tun_pkt_l"},
	{"igu_rx_tun_pkt_h"},
	{"igu_egu_rsv5"},
	{"igu_egu_rsv6"},
};

static struct hns3_dfx_reg_entry dfx_rpu_reg_0_list[] = {
	{"rpu_tc_queue_num",		"rpu_currport_tnl_index"},
	{"rpu_fsm_dfx_st0"},
	{"rpu_fsm_dfx_st1"},
	{"rpu_rpu_rx_pkt_drop_cnt"},
	{"rpu_buf_wait_timeout"},
	{"rpu_buf_wait_timeout_qid"},
};

static struct hns3_dfx_reg_entry dfx_rpu_reg_1_list[] = {
	{"rpu_rsv0"},
	{"rpu_fifo_dfx_st0"},
	{"rpu_fifo_dfx_st1"},
	{"rpu_fifo_dfx_st2"},
	{"rpu_fifo_dfx_st3"},
	{"rpu_fifo_dfx_st4"},

	{"rpu_fifo_dfx_st5"},
	{"rpu_rsv1"},
	{"rpu_rsv2"},
	{"rpu_rsv3"},
	{"rpu_rsv4"},
	{"rpu_rsv5"},
};

static struct hns3_dfx_reg_entry dfx_ncsi_reg_list[] = {
	{"ncsi_rsv0"},
	{"ncsi_egu_tx_fifo_sts"},
	{"ncsi_pause_status"},
	{"ncsi_rx_ctrl_dmac_err_cnt"},
	{"ncsi_rx_ctrl_smac_err_cnt"},
	{"ncsi_rx_ctrl_cks_err_cnt"},

	{"ncsi_rx_ctrl_pkt_err_cnt"},
	{"ncsi_rx_pt_dmac_err_cnt"},
	{"ncsi_rx_pt_smac_err_cnt"},
	{"ncsi_rx_pt_pkt_cnt"},
	{"ncsi_rx_fcs_err_cnt"},
	{"ncsi_tx_ctrl_dmac_err_cnt"},

	{"ncsi_tx_ctrl_smac_err_cnt"},
	{"ncsi_tx_ctrl_pkt_cnt"},
	{"ncsi_tx_pt_dmac_err_cnt"},
	{"ncsi_tx_pt_smac_err_cnt"},
	{"ncsi_tx_pt_pkt_cnt"},
	{"ncsi_tx_pt_pkt_trun_cnt"},

	{"ncsi_tx_pt_pkt_err_cnt"},
	{"ncsi_tx_ctrl_pkt_err_cnt"},
	{"ncsi_rx_ctrl_pkt_trun_cnt"},
	{"ncsi_rx_ctrl_pkt_cflit_cnt"},
	{"ncsi_rsv1"},
	{"ncsi_rsv2"},

	{"ncsi_mac_rx_octets_ok"},
	{"ncsi_mac_rx_octets_bad"},
	{"ncsi_mac_rx_uc_pkts"},
	{"ncsi_mac_rx_mc_pkts"},
	{"ncsi_mac_rx_bc_pkts"},
	{"ncsi_mac_rx_pkts_64octets"},

	{"ncsi_mac_rx_pkts_64to127_octets"},
	{"ncsi_mac_rx_pkts_128to255_octets"},
	{"ncsi_mac_rx_pkts_256to511_octets"},
	{"ncsi_mac_rx_pkts_512to1023_octets"},
	{"ncsi_mac_rx_pkts_1024to1518_octets"},
	{"ncsi_mac_rx_pkts_1519tomax_octets"},

	{"ncsi_mac_rx_fcs_errors"},
	{"ncsi_mac_rx_long_errors"},
	{"ncsi_mac_rx_jabber_errors"},
	{"ncsi_mac_rx_runt_err_cnt"},
	{"ncsi_mac_rx_short_err_cnt"},
	{"ncsi_mac_rx_filt_pkt_cnt"},

	{"ncsi_mac_rx_octets_total_filt"},
	{"ncsi_mac_tx_octets_ok"},
	{"ncsi_mac_tx_octets_bad"},
	{"ncsi_mac_tx_uc_pkts"},
	{"ncsi_mac_tx_mc_pkts"},
	{"ncsi_mac_tx_bc_pkts"},

	{"ncsi_mac_tx_pkts_64octets"},
	{"ncsi_mac_tx_pkts_64to127_octets"},
	{"ncsi_mac_tx_pkts_128to255_octets"},
	{"ncsi_mac_tx_pkts_256to511_octets"},
	{"ncsi_mac_tx_pkts_512to1023_octets"},
	{"ncsi_mac_tx_pkts_1024to1518_octets"},

	{"ncsi_mac_tx_pkts_1519tomax_octets"},
	{"ncsi_mac_tx_underrun"},
	{"ncsi_mac_tx_crc_error"},
	{"ncsi_mac_tx_pause_frames"},
	{"ncsi_mac_rx_pad_pkts"},
	{"ncsi_mac_rx_pause_frames"},
};

static struct hns3_dfx_reg_entry dfx_rtc_reg_list[] = {
	{"rtc_rsv0"},
	{"lge_igu_afifo_dfx_0"},
	{"lge_igu_afifo_dfx_1"},
	{"lge_igu_afifo_dfx_2"},
	{"lge_igu_afifo_dfx_3"},
	{"lge_igu_afifo_dfx_4"},

	{"lge_igu_afifo_dfx_5"},
	{"lge_igu_afifo_dfx_6"},
	{"lge_igu_afifo_dfx_7"},
	{"lge_egu_afifo_dfx_0"},
	{"lge_egu_afifo_dfx_1"},
	{"lge_egu_afifo_dfx_2"},

	{"lge_egu_afifo_dfx_3"},
	{"lge_egu_afifo_dfx_4"},
	{"lge_egu_afifo_dfx_5"},
	{"lge_egu_afifo_dfx_6"},
	{"lge_egu_afifo_dfx_7"},
	{"cge_igu_afifo_dfx_0"},

	{"cge_igu_afifo_dfx_1"},
	{"cge_egu_afifo_dfx_0"},
	{"cge_egu_afifo_dfx_i"},
	{"rtc_rsv1"},
	{"rtc_rsv2"},
	{"rtc_rsv3"},
};

static struct hns3_dfx_reg_entry dfx_ppp_reg_list[] = {
	{"ppp_rsv0"},
	{"ppp_drop_from_prt_pkt_cnt"},
	{"ppp_drop_from_host_pkt_cnt"},
	{"ppp_drop_tx_vlan_proc_cnt"},
	{"ppp_drop_mng_cnt"},
	{"ppp_drop_fd_cnt"},

	{"ppp_drop_no_dst_cnt"},
	{"ppp_drop_mc_mbid_full_cnt"},
	{"ppp_drop_sc_filtered"},
	{"ppp_ppp_mc_drop_pkt_cnt"},
	{"ppp_drop_pt_cnt"},
	{"ppp_drop_mac_anti_spoof_cnt"},

	{"ppp_drop_ig_vfv_cnt"},
	{"ppp_drop_ig_prtv_cnt"},
	{"ppp_drop_cnm_pfc_pause_cnt"},
	{"ppp_drop_torus_tc_cnt"},
	{"ppp_drop_torus_lpbk_cnt"},
	{"ppp_ppp_hfs_sts"},

	{"ppp_mc_rslt_sts"},
	{"ppp_p3u_sts"},
	{"ppp_rslt_descr_sts",		"ppp_rsv1"},
	{"ppp_umv_sts_0"},
	{"ppp_umv_sts_1"},
	{"ppp_vfv_sts"},

	{"ppp_gro_key_cnt"},
	{"ppp_gro_info_cnt"},
	{"ppp_gro_drop_cnt"},
	{"ppp_gro_out_cnt"},
	{"ppp_gro_key_match_data_cnt"},
	{"ppp_gro_key_match_tcam_cnt"},

	{"ppp_gro_info_match_cnt"},
	{"ppp_gro_free_entry_cnt"},
	{"ppp_gro_inner_dfx_signal"},
	{"ppp_rsv2"},
	{"ppp_rsv3"},
	{"ppp_rsv4"},

	{"ppp_get_rx_pkt_cnt_l"},
	{"ppp_get_rx_pkt_cnt_h"},
	{"ppp_get_tx_pkt_cnt_l"},
	{"ppp_get_tx_pkt_cnt_h"},
	{"ppp_send_uc_prt2host_pkt_cnt_l"},
	{"ppp_send_uc_prt2host_pkt_cnt_h"},

	{"ppp_send_uc_prt2prt_pkt_cnt_l"},
	{"ppp_send_uc_prt2prt_pkt_cnt_h"},
	{"ppp_send_uc_host2host_pkt_cnt_l"},
	{"ppp_send_uc_host2host_pkt_cnt_h"},
	{"ppp_send_uc_host2prt_pkt_cnt_l"},
	{"ppp_send_uc_host2prt_pkt_cnt_h"},

	{"ppp_send_mc_from_prt_cnt_l"},
	{"ppp_send_mc_from_prt_cnt_h"},
	{"ppp_send_mc_from_host_cnt_l"},
	{"ppp_send_mc_from_host_cnt_h"},
	{"ppp_ssu_mc_rd_cnt_l"},
	{"ppp_ssu_mc_rd_cnt_h"},

	{"ppp_ssu_mc_drop_cnt_l"},
	{"ppp_ssu_mc_drop_cnt_h"},
	{"ppp_ssu_mc_rd_pkt_cnt_l"},
	{"ppp_ssu_mc_rd_pkt_cnt_h"},
	{"ppp_mc_2host_pkt_cnt_l"},
	{"ppp_mc_2host_pkt_cnt_h"},

	{"ppp_mc_2prt_pkt_cnt_l"},
	{"ppp_mc_2prt_pkt_cnt_h"},
	{"ppp_ntsnos_pkt_cnt_l"},
	{"ppp_ntsnos_pkt_cnt_h"},
	{"ppp_ntup_pkt_cnt_l"},
	{"ppp_ntup_pkt_cnt_h"},

	{"ppp_ntlcl_pkt_cnt_l"},
	{"ppp_ntlcl_pkt_cnt_h"},
	{"ppp_nttgt_pkt_cnt_l"},
	{"ppp_nttgt_pkt_cnt_h"},
	{"ppp_rtns_pkt_cnt_l"},
	{"ppp_rtns_pkt_cnt_h"},

	{"ppp_rtlpbk_pkt_cnt_l"},
	{"ppp_rtlpbk_pkt_cnt_h"},
	{"ppp_nr_pkt_cnt_l"},
	{"ppp_nr_pkt_cnt_h"},
	{"ppp_rr_pkt_cnt_l"},
	{"ppp_rr_pkt_cnt_h"},

	{"ppp_mng_tbl_hit_cnt_l"},
	{"ppp_mng_tbl_hit_cnt_h"},
	{"ppp_fd_tbl_hit_cnt_l"},
	{"ppp_fd_tbl_hit_cnt_h"},
	{"ppp_fd_lkup_cnt_l"},
	{"ppp_fd_lkup_cnt_h"},

	{"ppp_bc_hit_cnt"},
	{"ppp_bc_hit_cnt_h"},
	{"ppp_um_tbl_uc_hit_cnt"},
	{"ppp_um_tbl_uc_hit_cnt_h"},
	{"ppp_um_tbl_mc_hit_cnt"},
	{"ppp_um_tbl_mc_hit_cnt_h"},

	{"ppp_um_tbl_vmdq1_hit_cnt_l",	"ppp_um_tbl_snq_hit_cnt_l"},
	{"ppp_um_tbl_vmdq1_hit_cnt_h",	"ppp_um_tbl_snq_hit_cnt_h"},
	{"ppp_mta_tbl_hit_cnt_l",	"ppp_rsv5"},
	{"ppp_mta_tbl_hit_cnt_h",	"ppp_rsv6"},
	{"ppp_fwd_bonding_hit_cnt_l"},
	{"ppp_fwd_bonding_hit_cnt_h"},

	{"ppp_promisc_tbl_hit_cnt_l"},
	{"ppp_promisc_tbl_hit_cnt_h"},
	{"ppp_get_tunl_pkt_cnt_l"},
	{"ppp_get_tunl_pkt_cnt_h"},
	{"ppp_get_bmc_pkt_cnt_l"},
	{"ppp_get_bmc_pkt_cnt_h"},

	{"ppp_send_uc_prt2bmc_pkt_cnt_l"},
	{"ppp_send_uc_prt2bmc_pkt_cnt_h"},
	{"ppp_send_uc_host2bmc_pkt_cnt_l"},
	{"ppp_send_uc_host2bmc_pkt_cnt_h"},
	{"ppp_send_uc_bmc2host_pkt_cnt_l"},
	{"ppp_send_uc_bmc2host_pkt_cnt_h"},

	{"ppp_send_uc_bmc2prt_pkt_cnt_l"},
	{"ppp_send_uc_bmc2prt_pkt_cnt_h"},
	{"ppp_mc_2bmc_pkt_cnt_l"},
	{"ppp_mc_2bmc_pkt_cnt_h"},
	{"ppp_vlan_mirr_cnt_l",		"ppp_rsv7"},
	{"ppp_vlan_mirr_cnt_h",		"ppp_rsv8"},

	{"ppp_ig_mirr_cnt_l",		"ppp_rsv9"},
	{"ppp_ig_mirr_cnt_h",		"ppp_rsv10"},
	{"ppp_eg_mirr_cnt_l",		"ppp_rsv11"},
	{"ppp_eg_mirr_cnt_h",		"ppp_rsv12"},
	{"ppp_rx_default_host_hit_cnt_l"},
	{"ppp_rx_default_host_hit_cnt_h"},

	{"ppp_lan_pair_cnt_l"},
	{"ppp_lan_pair_cnt_h"},
	{"ppp_um_tbl_mc_hit_pkt_cnt_l"},
	{"ppp_um_tbl_mc_hit_pkt_cnt_h"},
	{"ppp_mta_tbl_hit_pkt_cnt_l"},
	{"ppp_mta_tbl_hit_pkt_cnt_h"},

	{"ppp_promisc_tbl_hit_pkt_cnt_l"},
	{"ppp_promisc_tbl_hit_pkt_cnt_h"},
	{"ppp_rsv13"},
	{"ppp_rsv14"},
	{"ppp_rsv15"},
	{"ppp_rsv16"},
};

static struct hns3_dfx_reg_entry dfx_rcb_reg_list[] = {
	{"rcb_rsv0"},
	{"rcb_fsm_dfx_st0"},
	{"rcb_fsm_dfx_st1"},
	{"rcb_fsm_dfx_st2"},
	{"rcb_fifo_dfx_st0"},
	{"rcb_fifo_dfx_st1"},

	{"rcb_fifo_dfx_st2"},
	{"rcb_fifo_dfx_st3"},
	{"rcb_fifo_dfx_st4"},
	{"rcb_fifo_dfx_st5"},
	{"rcb_fifo_dfx_st6"},
	{"rcb_fifo_dfx_st7"},

	{"rcb_fifo_dfx_st8"},
	{"rcb_fifo_dfx_st9"},
	{"rcb_fifo_dfx_st10"},
	{"rcb_fifo_dfx_st11"},
	{"rcb_q_credit_vld_0"},
	{"rcb_q_credit_vld_1"},

	{"rcb_q_credit_vld_2"},
	{"rcb_q_credit_vld_3"},
	{"rcb_q_credit_vld_4"},
	{"rcb_q_credit_vld_5"},
	{"rcb_q_credit_vld_6"},
	{"rcb_q_credit_vld_7"},

	{"rcb_q_credit_vld_8"},
	{"rcb_q_credit_vld_9"},
	{"rcb_q_credit_vld_10"},
	{"rcb_q_credit_vld_11"},
	{"rcb_q_credit_vld_12"},
	{"rcb_q_credit_vld_13"},

	{"rcb_q_credit_vld_14"},
	{"rcb_q_credit_vld_15"},
	{"rcb_q_credit_vld_16"},
	{"rcb_q_credit_vld_17"},
	{"rcb_q_credit_vld_18"},
	{"rcb_q_credit_vld_19"},

	{"rcb_q_credit_vld_20"},
	{"rcb_q_credit_vld_21"},
	{"rcb_q_credit_vld_22"},
	{"rcb_q_credit_vld_23"},
	{"rcb_q_credit_vld_24"},
	{"rcb_q_credit_vld_25"},

	{"rcb_q_credit_vld_26"},
	{"rcb_q_credit_vld_27"},
	{"rcb_q_credit_vld_28"},
	{"rcb_q_credit_vld_29"},
	{"rcb_q_credit_vld_30"},
	{"rcb_q_credit_vld_31"},

	{"rcb_gro_bd_serr_cnt"},
	{"rcb_gro_context_serr_cnt"},
	{"rcb_rx_stash_cfg_serr_cnt"},
	{"rcb_axi_rd_fbd_serr_cnt",	"rcb_rcb_tx_mem_serr_cnt"},
	{"rcb_gro_bd_merr_cnt"},
	{"rcb_gro_context_merr_cnt"},

	{"rcb_rx_stash_cfg_merr_cnt"},
	{"rcb_axi_rd_fbd_merr_cnt"},
	{"rcb_rsv1"},
	{"rcb_rsv2"},
	{"rcb_rsv3"},
	{"rcb_rsv4"},
};

static struct hns3_dfx_reg_entry dfx_tqp_reg_list[] = {
	{"dfx_tqp_q_num"},
	{"rcb_cfg_rx_ring_tail"},
	{"rcb_cfg_rx_ring_head"},
	{"rcb_cfg_rx_ring_fbdnum"},
	{"rcb_cfg_rx_ring_offset"},
	{"rcb_cfg_rx_ring_fbdoffset"},

	{"rcb_cfg_rx_ring_pktnum_record"},
	{"rcb_cfg_tx_ring_tail"},
	{"rcb_cfg_tx_ring_head"},
	{"rcb_cfg_tx_ring_fbdnum"},
	{"rcb_cfg_tx_ring_offset"},
	{"rcb_cfg_tx_ring_ebdnum"},
};

static struct hns3_dfx_reg_entry dfx_ssu_reg_2_list[] = {
	{"dfx_ssu2_oq_index"},
	{"dfx_ssu2_queue_cnt"},
	{"dfx_ssu2_rsv0"},
	{"dfx_ssu2_rsv1"},
	{"dfx_ssu2_rsv2"},
	{"dfx_ssu2_rsv3"},
};

enum hns3_reg_modules {
	HNS3_BIOS_COMMON = 0,
	HNS3_SSU_0,
	HNS3_SSU_1,
	HNS3_IGU_EGU,
	HNS3_RPU_0,
	HNS3_RPU_1,
	HNS3_NCSI,
	HNS3_RTC,
	HNS3_PPP,
	HNS3_RCB,
	HNS3_TQP,
	HNS3_SSU_2,

	HNS3_CMDQ = 12,
	HNS3_COMMON_PF,
	HNS3_COMMON_VF,
	HNS3_RING,
	HNS3_TQP_INTR,

	HNS3_32_BIT_DFX,
	HNS3_64_BIT_DFX,
};

#define HNS3_MODULE_MASK(x) RTE_BIT32(x)
#define HNS3_VF_MODULES (HNS3_MODULE_MASK(HNS3_CMDQ) | HNS3_MODULE_MASK(HNS3_COMMON_VF) | \
			 HNS3_MODULE_MASK(HNS3_RING) | HNS3_MODULE_MASK(HNS3_TQP_INTR))
#define HNS3_VF_ONLY_MODULES HNS3_MODULE_MASK(HNS3_COMMON_VF)

struct hns3_reg_list {
	const void *reg_list;
	uint32_t entry_num;
};

struct {
	const char *name;
	uint32_t module;
} hns3_module_name_map[] = {
	{ "bios",	HNS3_MODULE_MASK(HNS3_BIOS_COMMON) },
	{ "ssu",	HNS3_MODULE_MASK(HNS3_SSU_0) | HNS3_MODULE_MASK(HNS3_SSU_1) |
			HNS3_MODULE_MASK(HNS3_SSU_2) },
	{ "igu_egu",	HNS3_MODULE_MASK(HNS3_IGU_EGU) },
	{ "rpu",	HNS3_MODULE_MASK(HNS3_RPU_0) | HNS3_MODULE_MASK(HNS3_RPU_1) },
	{ "ncsi",	HNS3_MODULE_MASK(HNS3_NCSI) },
	{ "rtc",	HNS3_MODULE_MASK(HNS3_RTC) },
	{ "ppp",	HNS3_MODULE_MASK(HNS3_PPP) },
	{ "rcb",	HNS3_MODULE_MASK(HNS3_RCB) },
	{ "tqp",	HNS3_MODULE_MASK(HNS3_TQP) },
	{ "cmdq",	HNS3_MODULE_MASK(HNS3_CMDQ) },
	{ "common_pf",	HNS3_MODULE_MASK(HNS3_COMMON_PF) },
	{ "common_vf",	HNS3_MODULE_MASK(HNS3_COMMON_VF) },
	{ "ring",	HNS3_MODULE_MASK(HNS3_RING) },
	{ "tqp_intr",	HNS3_MODULE_MASK(HNS3_TQP_INTR) },
	{ "32_bit_dfx",	HNS3_MODULE_MASK(HNS3_32_BIT_DFX) },
	{ "64_bit_dfx",	HNS3_MODULE_MASK(HNS3_64_BIT_DFX) },
};

static struct hns3_reg_list hns3_reg_lists[] = {
	[HNS3_BIOS_COMMON]	= { dfx_bios_common_reg_list,	RTE_DIM(dfx_bios_common_reg_list) },
	[HNS3_SSU_0]		= { dfx_ssu_reg_0_list,		RTE_DIM(dfx_ssu_reg_0_list) },
	[HNS3_SSU_1]		= { dfx_ssu_reg_1_list,		RTE_DIM(dfx_ssu_reg_1_list) },
	[HNS3_IGU_EGU]		= { dfx_igu_egu_reg_list,	RTE_DIM(dfx_igu_egu_reg_list) },
	[HNS3_RPU_0]		= { dfx_rpu_reg_0_list,		RTE_DIM(dfx_rpu_reg_0_list) },
	[HNS3_RPU_1]		= { dfx_rpu_reg_1_list,		RTE_DIM(dfx_rpu_reg_1_list) },
	[HNS3_NCSI]		= { dfx_ncsi_reg_list,		RTE_DIM(dfx_ncsi_reg_list) },
	[HNS3_RTC]		= { dfx_rtc_reg_list,		RTE_DIM(dfx_rtc_reg_list) },
	[HNS3_PPP]		= { dfx_ppp_reg_list,		RTE_DIM(dfx_ppp_reg_list) },
	[HNS3_RCB]		= { dfx_rcb_reg_list,		RTE_DIM(dfx_rcb_reg_list) },
	[HNS3_TQP]		= { dfx_tqp_reg_list,		RTE_DIM(dfx_tqp_reg_list) },
	[HNS3_SSU_2]		= { dfx_ssu_reg_2_list,		RTE_DIM(dfx_ssu_reg_2_list) },
	[HNS3_CMDQ]		= { cmdq_reg_list,		RTE_DIM(cmdq_reg_list) },
	[HNS3_COMMON_PF]	= { common_reg_list,		RTE_DIM(common_reg_list) },
	[HNS3_COMMON_VF]	= { common_vf_reg_list,		RTE_DIM(common_vf_reg_list) },
	[HNS3_RING]		= { ring_reg_list,		RTE_DIM(ring_reg_list) },
	[HNS3_TQP_INTR]		= { tqp_intr_reg_list,		RTE_DIM(tqp_intr_reg_list) },
	[HNS3_32_BIT_DFX]	= { regs_32_bit_list,		RTE_DIM(regs_32_bit_list) },
	[HNS3_64_BIT_DFX]	= { regs_64_bit_list,		RTE_DIM(regs_64_bit_list) },
};

static const uint32_t hns3_dfx_reg_opcode_list[] = {
	[HNS3_BIOS_COMMON]	=	HNS3_OPC_DFX_BIOS_COMMON_REG,
	[HNS3_SSU_0]		=	HNS3_OPC_DFX_SSU_REG_0,
	[HNS3_SSU_1]		=	HNS3_OPC_DFX_SSU_REG_1,
	[HNS3_IGU_EGU]		=	HNS3_OPC_DFX_IGU_EGU_REG,
	[HNS3_RPU_0]		=	HNS3_OPC_DFX_RPU_REG_0,
	[HNS3_RPU_1]		=	HNS3_OPC_DFX_RPU_REG_1,
	[HNS3_NCSI]		=	HNS3_OPC_DFX_NCSI_REG,
	[HNS3_RTC]		=	HNS3_OPC_DFX_RTC_REG,
	[HNS3_PPP]		=	HNS3_OPC_DFX_PPP_REG,
	[HNS3_RCB]		=	HNS3_OPC_DFX_RCB_REG,
	[HNS3_TQP]		=	HNS3_OPC_DFX_TQP_REG,
	[HNS3_SSU_2]		=	HNS3_OPC_DFX_SSU_REG_2
};

static int
hns3_get_regs_num(struct hns3_hw *hw, uint32_t *regs_num_32_bit,
		  uint32_t *regs_num_64_bit)
{
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_REG_NUM, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "Query register number cmd failed, ret = %d",
			 ret);
		return ret;
	}

	*regs_num_32_bit = rte_le_to_cpu_32(desc.data[0]);
	*regs_num_64_bit = rte_le_to_cpu_32(desc.data[1]);
	if (*regs_num_32_bit != RTE_DIM(regs_32_bit_list) ||
	    *regs_num_64_bit * HNS3_64_BIT_REG_OUTPUT_SIZE != RTE_DIM(regs_64_bit_list)) {
		hns3_err(hw, "Query register number differ from the list!");
		return -EINVAL;
	}

	return 0;
}

static const char *
hns3_get_name_by_module(enum hns3_reg_modules module)
{
	size_t i;

	for (i = 0; i < RTE_DIM(hns3_module_name_map); i++) {
		if ((hns3_module_name_map[i].module & HNS3_MODULE_MASK(module)) != 0)
			return hns3_module_name_map[i].name;
	}
	return "unknown";
}

static void
hns3_get_module_names(char *names, uint32_t len)
{
	size_t i;

	for (i = 0; i < RTE_DIM(hns3_module_name_map); i++) {
		strlcat(names, " ", len);
		strlcat(names, hns3_module_name_map[i].name, len);
	}
}

static uint32_t
hns3_parse_modules_by_filter(struct hns3_hw *hw, const char *filter)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	char names[HNS3_MAX_MODULES_LEN] = {0};
	uint32_t modules = 0;
	size_t i;

	if (filter == NULL) {
		modules = (1 << RTE_DIM(hns3_reg_lists)) - 1;
	} else {
		for (i = 0; i < RTE_DIM(hns3_module_name_map); i++) {
			if (strcmp(filter, hns3_module_name_map[i].name) == 0) {
				modules |= hns3_module_name_map[i].module;
				break;
			}
		}
	}

	if (hns->is_vf)
		modules &= HNS3_VF_MODULES;
	else
		modules &= ~HNS3_VF_ONLY_MODULES;
	if (modules == 0) {
		hns3_get_module_names(names, HNS3_MAX_MODULES_LEN);
		hns3_err(hw, "mismatched module name! Available names are:%s.",
			 names);
	}
	return modules;
}

static int
hns3_get_dfx_reg_bd_num(struct hns3_hw *hw, uint32_t *bd_num_list,
			uint32_t list_size)
{
#define HNS3_GET_DFX_REG_BD_NUM_SIZE	4
	struct hns3_cmd_desc desc[HNS3_GET_DFX_REG_BD_NUM_SIZE];
	uint32_t index, desc_index;
	uint32_t bd_num;
	uint32_t i;
	int ret;

	for (i = 0; i < HNS3_GET_DFX_REG_BD_NUM_SIZE - 1; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_DFX_BD_NUM, true);
		desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	}
	/* The last BD does not need a next flag */
	hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_DFX_BD_NUM, true);

	ret = hns3_cmd_send(hw, desc, HNS3_GET_DFX_REG_BD_NUM_SIZE);
	if (ret) {
		hns3_err(hw, "fail to get dfx bd num, ret = %d.", ret);
		return ret;
	}

	/* The first data in the first BD is a reserved field */
	for (i = 1; i <= list_size; i++) {
		desc_index = i / HNS3_CMD_DESC_DATA_NUM;
		index = i % HNS3_CMD_DESC_DATA_NUM;
		bd_num = rte_le_to_cpu_32(desc[desc_index].data[index]);
		bd_num_list[i - 1] = bd_num;
	}

	return 0;
}

static uint32_t
hns3_get_regs_length(struct hns3_hw *hw, uint32_t modules)
{
	uint32_t reg_num = 0, length = 0;
	uint32_t i;

	for (i = 0; i < RTE_DIM(hns3_reg_lists); i++) {
		if ((RTE_BIT32(i) & modules) == 0)
			continue;
		reg_num = hns3_reg_lists[i].entry_num;
		if (i == HNS3_RING)
			reg_num *= hw->tqps_num;
		else if (i == HNS3_TQP_INTR)
			reg_num *= hw->intr_tqps_num;

		length += reg_num;
	}

	return length;
}

static void
hns3_fill_dfx_regs_name(struct hns3_hw *hw, struct rte_dev_reg_info *regs,
			const struct hns3_dfx_reg_entry *reg_list, uint32_t reg_num)
{
	uint32_t i, cnt = regs->length;
	const char *name;

	if (regs->names == NULL)
		return;

	for (i = 0; i < reg_num; i++) {
		name = reg_list[i].name_v1;
		if (hw->revision == PCI_REVISION_ID_HIP09_A && reg_list[i].name_v2 != NULL)
			name = reg_list[i].name_v2;
		snprintf(regs->names[cnt++].name, RTE_ETH_REG_NAME_SIZE, "%s", name);
	}
}

static int
hns3_get_32_bit_regs(struct hns3_hw *hw, uint32_t regs_num, struct rte_dev_reg_info *regs)
{
#define HNS3_32_BIT_REG_RTN_DATANUM 8
#define HNS3_32_BIT_DESC_NODATA_LEN 2
	uint32_t *reg_val = regs->data;
	struct hns3_cmd_desc *desc;
	uint32_t *desc_data;
	int cmd_num;
	int i, k, n;
	int ret;

	if (regs_num == 0)
		return 0;

	cmd_num = DIV_ROUND_UP(regs_num + HNS3_32_BIT_DESC_NODATA_LEN,
			       HNS3_32_BIT_REG_RTN_DATANUM);
	desc = rte_zmalloc("hns3-32bit-regs",
			   sizeof(struct hns3_cmd_desc) * cmd_num, 0);
	if (desc == NULL) {
		hns3_err(hw, "Failed to allocate %zx bytes needed to "
			 "store 32bit regs",
			 sizeof(struct hns3_cmd_desc) * cmd_num);
		return -ENOMEM;
	}

	hns3_cmd_setup_basic_desc(&desc[0], HNS3_OPC_QUERY_32_BIT_REG, true);
	ret = hns3_cmd_send(hw, desc, cmd_num);
	if (ret) {
		hns3_err(hw, "Query 32 bit register cmd failed, ret = %d",
			 ret);
		rte_free(desc);
		return ret;
	}

	hns3_fill_dfx_regs_name(hw, regs, regs_32_bit_list, regs_num);
	reg_val += regs->length;
	regs->length += regs_num;
	for (i = 0; i < cmd_num; i++) {
		if (i == 0) {
			desc_data = &desc[i].data[0];
			n = HNS3_32_BIT_REG_RTN_DATANUM -
			    HNS3_32_BIT_DESC_NODATA_LEN;
		} else {
			desc_data = (uint32_t *)(&desc[i]);
			n = HNS3_32_BIT_REG_RTN_DATANUM;
		}
		for (k = 0; k < n; k++) {
			*reg_val++ = rte_le_to_cpu_32(*desc_data++);
			regs_num--;
			if (regs_num == 0)
				break;
		}
	}

	rte_free(desc);
	return 0;
}

static int
hns3_get_64_bit_regs(struct hns3_hw *hw, uint32_t regs_num, struct rte_dev_reg_info *regs)
{
#define HNS3_64_BIT_REG_RTN_DATANUM 4
#define HNS3_64_BIT_DESC_NODATA_LEN 1
	uint32_t *reg_val = regs->data;
	struct hns3_cmd_desc *desc;
	uint64_t *desc_data;
	int cmd_num;
	int i, k, n;
	int ret;

	if (regs_num == 0)
		return 0;

	cmd_num = DIV_ROUND_UP(regs_num + HNS3_64_BIT_DESC_NODATA_LEN,
			       HNS3_64_BIT_REG_RTN_DATANUM);
	desc = rte_zmalloc("hns3-64bit-regs",
			   sizeof(struct hns3_cmd_desc) * cmd_num, 0);
	if (desc == NULL) {
		hns3_err(hw, "Failed to allocate %zx bytes needed to "
			 "store 64bit regs",
			 sizeof(struct hns3_cmd_desc) * cmd_num);
		return -ENOMEM;
	}

	hns3_cmd_setup_basic_desc(&desc[0], HNS3_OPC_QUERY_64_BIT_REG, true);
	ret = hns3_cmd_send(hw, desc, cmd_num);
	if (ret) {
		hns3_err(hw, "Query 64 bit register cmd failed, ret = %d",
			 ret);
		rte_free(desc);
		return ret;
	}

	hns3_fill_dfx_regs_name(hw, regs, regs_64_bit_list, regs_num * HNS3_64_BIT_REG_OUTPUT_SIZE);
	reg_val += regs->length;
	regs->length += regs_num * HNS3_64_BIT_REG_OUTPUT_SIZE;
	for (i = 0; i < cmd_num; i++) {
		if (i == 0) {
			desc_data = (uint64_t *)(&desc[i].data[0]);
			n = HNS3_64_BIT_REG_RTN_DATANUM -
			    HNS3_64_BIT_DESC_NODATA_LEN;
		} else {
			desc_data = (uint64_t *)(&desc[i]);
			n = HNS3_64_BIT_REG_RTN_DATANUM;
		}
		for (k = 0; k < n; k++) {
			*reg_val++ = rte_le_to_cpu_64(*desc_data++);
			regs_num--;
			if (!regs_num)
				break;
		}
	}

	rte_free(desc);
	return 0;
}

static void
hns3_direct_access_regs_help(struct hns3_hw *hw, struct rte_dev_reg_info *regs,
			     uint32_t modules, enum hns3_reg_modules idx)
{
	const struct hns3_dirt_reg_entry *reg_list;
	uint32_t *data = regs->data;
	size_t reg_num, i, cnt;

	if ((modules & HNS3_MODULE_MASK(idx)) == 0)
		return;

	data += regs->length;
	reg_num = hns3_reg_lists[idx].entry_num;
	reg_list = hns3_reg_lists[idx].reg_list;
	cnt = regs->length;
	for (i = 0; i < reg_num; i++) {
		*data++ = hns3_read_dev(hw, reg_list[i].addr);
		if (regs->names != NULL)
			snprintf(regs->names[cnt++].name, RTE_ETH_REG_NAME_SIZE,
				 "%s", reg_list[i].name);
	}

	regs->length += reg_num;
}

static uint32_t
hns3_get_module_tqp_reg_offset(enum hns3_reg_modules idx, uint16_t queue_id)
{
	if (idx == HNS3_RING)
		return hns3_get_tqp_reg_offset(queue_id);
	else if (idx == HNS3_TQP_INTR)
		return hns3_get_tqp_intr_reg_offset(queue_id);

	return 0;
}

static void
hns3_direct_access_tqp_regs_help(struct hns3_hw *hw, struct rte_dev_reg_info *regs,
				 uint32_t modules, enum hns3_reg_modules idx)
{
	const struct hns3_dirt_reg_entry *reg_list;
	uint32_t reg_num, i, j, reg_offset;
	uint32_t *data = regs->data;
	uint16_t tqp_num;

	if ((modules & HNS3_MODULE_MASK(idx)) == 0)
		return;

	tqp_num = (idx == HNS3_RING) ? hw->tqps_num : hw->intr_tqps_num;
	reg_list = hns3_reg_lists[idx].reg_list;
	reg_num = hns3_reg_lists[idx].entry_num;
	data += regs->length;
	for (i = 0; i < tqp_num; i++) {
		reg_offset = hns3_get_module_tqp_reg_offset(idx, i);
		for (j = 0; j < reg_num; j++) {
			*data++ = hns3_read_dev(hw, reg_list[j].addr + reg_offset);
			if (regs->names != NULL)
				snprintf(regs->names[regs->length].name,
					 RTE_ETH_REG_NAME_SIZE, "Q%u_%s", i, reg_list[j].name);
			regs->length++;
		}
	}
}

static void
hns3_direct_access_regs(struct hns3_hw *hw, struct rte_dev_reg_info *regs, uint32_t modules)
{
	hns3_direct_access_regs_help(hw, regs, modules, HNS3_COMMON_VF);
	hns3_direct_access_regs_help(hw, regs, modules, HNS3_COMMON_PF);
	hns3_direct_access_regs_help(hw, regs, modules, HNS3_CMDQ);
	hns3_direct_access_tqp_regs_help(hw, regs, modules, HNS3_RING);
	hns3_direct_access_tqp_regs_help(hw, regs, modules, HNS3_TQP_INTR);
}

static int
hns3_dfx_reg_cmd_send(struct hns3_hw *hw, struct hns3_cmd_desc *desc,
			int bd_num, uint32_t opcode)
{
	int ret;
	int i;

	for (i = 0; i < bd_num - 1; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], opcode, true);
		desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	}
	/* The last BD does not need a next flag */
	hns3_cmd_setup_basic_desc(&desc[i], opcode, true);

	ret = hns3_cmd_send(hw, desc, bd_num);
	if (ret)
		hns3_err(hw, "fail to query dfx registers, opcode = 0x%04X, "
			 "ret = %d.", opcode, ret);

	return ret;
}

static int
hns3_dfx_reg_fetch_data(struct hns3_cmd_desc *desc, int bd_num, uint32_t *reg)
{
	int desc_index;
	int reg_num;
	int index;
	int i;

	reg_num = bd_num * HNS3_CMD_DESC_DATA_NUM;
	for (i = 0; i < reg_num; i++) {
		desc_index = i / HNS3_CMD_DESC_DATA_NUM;
		index = i % HNS3_CMD_DESC_DATA_NUM;
		*reg++ = desc[desc_index].data[index];
	}

	return reg_num;
}

static int
hns3_get_dfx_regs(struct hns3_hw *hw, struct rte_dev_reg_info *regs, uint32_t modules)
{
	int opcode_num = RTE_DIM(hns3_dfx_reg_opcode_list);
	uint32_t max_bd_num, bd_num, opcode, regs_num;
	uint32_t bd_num_list[opcode_num];
	struct hns3_cmd_desc *cmd_descs;
	uint32_t *data = regs->data;
	int ret;
	int i;

	ret = hns3_get_dfx_reg_bd_num(hw, bd_num_list, opcode_num);
	if (ret)
		return ret;

	max_bd_num = 0;
	for (i = 0; i < opcode_num; i++)
		max_bd_num = RTE_MAX(bd_num_list[i], max_bd_num);

	cmd_descs = calloc(max_bd_num, sizeof(*cmd_descs));
	if (cmd_descs == NULL)
		return -ENOMEM;

	data += regs->length;
	for (i = 0; i < opcode_num; i++) {
		opcode = hns3_dfx_reg_opcode_list[i];
		bd_num = bd_num_list[i];
		if ((modules & HNS3_MODULE_MASK(i)) == 0)
			continue;
		if (bd_num == 0)
			continue;
		ret = hns3_dfx_reg_cmd_send(hw, cmd_descs, bd_num, opcode);
		if (ret)
			break;

		regs_num = hns3_dfx_reg_fetch_data(cmd_descs, bd_num, data);
		if (regs_num !=  hns3_reg_lists[i].entry_num) {
			hns3_err(hw, "Query register number differ from the list for module %s!",
				 hns3_get_name_by_module(i));
			free(cmd_descs);
			return -EINVAL;
		}
		hns3_fill_dfx_regs_name(hw, regs, hns3_reg_lists[i].reg_list, regs_num);
		regs->length += regs_num;
		data += regs_num;
	}
	free(cmd_descs);

	return ret;
}

static int
hns3_get_32_b4_bit_regs(struct hns3_hw *hw, struct rte_dev_reg_info *regs, uint32_t modules)
{
	uint32_t regs_num_32_bit;
	uint32_t regs_num_64_bit;
	int ret;

	if ((modules & HNS3_MODULE_MASK(HNS3_32_BIT_DFX)) == 0 &&
	    (modules & HNS3_MODULE_MASK(HNS3_64_BIT_DFX)) == 0)
		return 0;

	ret = hns3_get_regs_num(hw, &regs_num_32_bit, &regs_num_64_bit);
	if (ret) {
		hns3_err(hw, "Get register number failed, ret = %d", ret);
		return ret;
	}

	if ((modules & HNS3_MODULE_MASK(HNS3_32_BIT_DFX)) != 0) {
		ret = hns3_get_32_bit_regs(hw, regs_num_32_bit, regs);
		if (ret) {
			hns3_err(hw, "Get 32 bit register failed, ret = %d", ret);
			return ret;
		}
	}

	if ((modules & HNS3_MODULE_MASK(HNS3_64_BIT_DFX)) != 0) {
		ret = hns3_get_64_bit_regs(hw, regs_num_64_bit, regs);
		if (ret) {
			hns3_err(hw, "Get 64 bit register failed, ret = %d", ret);
			return ret;
		}
	}

	return 0;
}

static int
hns3_get_regs_from_firmware(struct hns3_hw *hw, struct rte_dev_reg_info *regs, uint32_t modules)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	int ret;

	if (hns->is_vf)
		return 0;

	ret = hns3_get_32_b4_bit_regs(hw, regs, modules);
	if (ret != 0)
		return ret;

	return hns3_get_dfx_regs(hw, regs, modules);
}

int
hns3_get_regs(struct rte_eth_dev *eth_dev, struct rte_dev_reg_info *regs)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint32_t modules;
	uint32_t length;

	modules = hns3_parse_modules_by_filter(hw, regs->filter);
	if (modules == 0)
		return -EINVAL;

	length = hns3_get_regs_length(hw, modules);
	if (regs->data == NULL) {
		regs->length = length;
		regs->width = sizeof(uint32_t);
		return 0;
	}

	/* Only full register dump is supported */
	if (regs->length && regs->length != length)
		return -ENOTSUP;

	regs->version = hw->fw_version;
	/* to count the number of filled registers */
	regs->length = 0;

	/* fetching per-PF registers values from PF PCIe register space */
	hns3_direct_access_regs(hw, regs, modules);

	/* fetching PF common registers values from firmware */
	return  hns3_get_regs_from_firmware(hw, regs, modules);
}
