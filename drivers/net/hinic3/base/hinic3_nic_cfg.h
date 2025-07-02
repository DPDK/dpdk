/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_NIC_CFG_H_
#define _HINIC3_NIC_CFG_H_

#include "hinic3_mgmt.h"

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#define OS_VF_ID_TO_HW(os_vf_id) ((os_vf_id) + 1)
#define HW_VF_ID_TO_OS(hw_vf_id) ((hw_vf_id) - 1)

#define HINIC3_VLAN_PRIORITY_SHIFT 13

#define HINIC3_DCB_UP_MAX 0x8

#define HINIC3_MAX_NUM_RQ 256

#define HINIC3_MAX_MTU_SIZE 9600
#define HINIC3_MIN_MTU_SIZE 256

#define HINIC3_COS_NUM_MAX 8

#define HINIC3_VLAN_TAG_SIZE 4
#define HINIC3_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + HINIC3_VLAN_TAG_SIZE * 2)

#define HINIC3_MIN_FRAME_SIZE	    (HINIC3_MIN_MTU_SIZE + HINIC3_ETH_OVERHEAD)
#define HINIC3_MAX_JUMBO_FRAME_SIZE (HINIC3_MAX_MTU_SIZE + HINIC3_ETH_OVERHEAD)

#define HINIC3_MTU_TO_PKTLEN(mtu) (mtu)

#define HINIC3_PKTLEN_TO_MTU(pktlen) (pktlen)

#define HINIC3_PF_SET_VF_ALREADY 0x4
#define HINIC3_MGMT_STATUS_EXIST 0x6
#define CHECK_IPSU_15BIT	 0x8000

#define HINIC3_MGMT_STATUS_TABLE_EMPTY 0xB
#define HINIC3_MGMT_STATUS_TABLE_FULL  0xC

#define HINIC3_MGMT_CMD_UNSUPPORTED 0xFF

#define HINIC3_MAX_UC_MAC_ADDRS 128
#define HINIC3_MAX_MC_MAC_ADDRS 2048

#define CAP_INFO_MAX_LEN 512
#define VENDOR_MAX_LEN	 17

/* Structures for RSS config. */
#define HINIC3_RSS_INDIR_SIZE	   256
#define HINIC3_RSS_INDIR_CMDQ_SIZE 128
#define HINIC3_RSS_KEY_SIZE	   40
#define HINIC3_RSS_ENABLE	   0x01
#define HINIC3_RSS_DISABLE	   0x00
#define HINIC3_INVALID_QID_BASE	   0xffff

#ifndef ETH_SPEED_NUM_200G
#define ETH_SPEED_NUM_200G 200000 /**< 200 Gbps. */
#endif

struct hinic3_rss_type {
	u8 tcp_ipv6_ext;
	u8 ipv6_ext;
	u8 tcp_ipv6;
	u8 ipv6;
	u8 tcp_ipv4;
	u8 ipv4;
	u8 udp_ipv6;
	u8 udp_ipv4;
};

enum hinic3_rss_hash_type {
	HINIC3_RSS_HASH_ENGINE_TYPE_XOR = 0,
	HINIC3_RSS_HASH_ENGINE_TYPE_TOEP,
	HINIC3_RSS_HASH_ENGINE_TYPE_MAX,
};

#define MAX_FEATURE_QWORD 4
struct hinic3_cmd_feature_nego {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 opcode; /**< 1: set, 0: get. */
	u8 rsvd;
	u64 s_feature[MAX_FEATURE_QWORD];
};

/* Structures for port info. */
struct nic_port_info {
	u8 port_type;
	u8 autoneg_cap;
	u8 autoneg_state;
	u8 duplex;
	u8 speed;
	u8 fec;
};

enum hinic3_link_status { HINIC3_LINK_DOWN = 0, HINIC3_LINK_UP };

enum nic_media_type {
	MEDIA_UNKNOWN = -1,
	MEDIA_FIBRE = 0,
	MEDIA_COPPER,
	MEDIA_BACKPLANE
};

enum nic_speed_level {
	LINK_SPEED_NOT_SET = 0,
	LINK_SPEED_10MB,
	LINK_SPEED_100MB,
	LINK_SPEED_1GB,
	LINK_SPEED_10GB,
	LINK_SPEED_25GB,
	LINK_SPEED_40GB,
	LINK_SPEED_50GB,
	LINK_SPEED_100GB,
	LINK_SPEED_200GB,
	LINK_SPEED_LEVELS,
};

enum hinic3_nic_event_type {
	EVENT_NIC_LINK_DOWN,
	EVENT_NIC_LINK_UP,
	EVENT_NIC_PORT_MODULE_EVENT,
	EVENT_NIC_DCB_STATE_CHANGE,
};

enum hinic3_link_port_type {
	LINK_PORT_UNKNOWN,
	LINK_PORT_OPTICAL_MM,
	LINK_PORT_OPTICAL_SM,
	LINK_PORT_PAS_COPPER,
	LINK_PORT_ACC,
	LINK_PORT_BASET,
	LINK_PORT_AOC = 0x40,
	LINK_PORT_ELECTRIC,
	LINK_PORT_BACKBOARD_INTERFACE,
};

enum hilink_fibre_subtype {
	FIBRE_SUBTYPE_SR = 1,
	FIBRE_SUBTYPE_LR,
	FIBRE_SUBTYPE_MAX,
};

enum hilink_fec_type {
	HILINK_FEC_NOT_SET,
	HILINK_FEC_RSFEC,
	HILINK_FEC_BASEFEC,
	HILINK_FEC_NOFEC,
	HILINK_FEC_LLRSFE,
	HILINK_FEC_MAX_TYPE,
};

enum mag_cmd_port_an {
	PORT_AN_NOT_SET = 0,
	PORT_CFG_AN_ON = 1,
	PORT_CFG_AN_OFF = 2
};

enum mag_cmd_port_speed {
	PORT_SPEED_NOT_SET = 0,
	PORT_SPEED_10MB = 1,
	PORT_SPEED_100MB = 2,
	PORT_SPEED_1GB = 3,
	PORT_SPEED_10GB = 4,
	PORT_SPEED_25GB = 5,
	PORT_SPEED_40GB = 6,
	PORT_SPEED_50GB = 7,
	PORT_SPEED_100GB = 8,
	PORT_SPEED_200GB = 9,
	PORT_SPEED_UNKNOWN
};

struct hinic3_sq_attr {
	u8 dma_attr_off;
	u8 pending_limit;
	u8 coalescing_time;
	u8 intr_en;
	u16 intr_idx;
	u32 l2nic_sqn;
	u64 ci_dma_base;
};

struct hinic3_cmd_cons_idx_attr {
	struct mgmt_msg_head msg_head;

	u16 func_idx;
	u8 dma_attr_off;
	u8 pending_limit;
	u8 coalescing_time;
	u8 intr_en;
	u16 intr_idx;
	u32 l2nic_sqn;
	u32 rsvd;
	u64 ci_addr;
};

struct hinic3_port_mac_set {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 vlan_id;
	u16 rsvd1;
	u8 mac[ETH_ALEN];
};

struct hinic3_port_mac_update {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 vlan_id;
	u16 rsvd1;
	u8 old_mac[ETH_ALEN];
	u16 rsvd2;
	u8 new_mac[ETH_ALEN];
};

struct hinic3_ppa_cfg_state_cmd {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 ppa_state;
	u8 rsvd;
};

struct hinic3_ppa_cfg_mode_cmd {
	struct mgmt_msg_head msg_head;

	u16 rsvd0;
	u8 ppa_mode;
	u8 qpc_func_nums;
	u16 base_qpc_func_id;
	u16 rsvd1;
};

struct hinic3_ppa_cfg_flush_cmd {
	struct mgmt_msg_head msg_head;

	u16 rsvd0;
	u8 flush_en; /**< 0: flush done, 1: in flush operation. */
	u8 rsvd1;
};

struct hinic3_ppa_fdir_query_cmd {
	struct mgmt_msg_head msg_head;

	u32 index;
	u32 rsvd;
	u64 pkt_nums;
	u64 pkt_bytes;
};

#define HINIC3_CMD_OP_ADD 1
#define HINIC3_CMD_OP_DEL 0

struct hinic3_cmd_vlan_config {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 opcode;
	u8 rsvd1;
	u16 vlan_id;
	u16 rsvd2;
};

struct hinic3_cmd_set_vlan_filter {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 resvd[2];
	/* Bit0: vlan filter en, bit1: broadcast filter en. */
	u32 vlan_filter_ctrl;
};

struct hinic3_cmd_port_info {
	struct mgmt_msg_head msg_head;

	u8 port_id;
	u8 rsvd1[3];
	u8 port_type;
	u8 autoneg_cap;
	u8 autoneg_state;
	u8 duplex;
	u8 speed;
	u8 fec;
	u16 rsvd2;
	u32 rsvd3[4];
};

struct hinic3_cmd_link_state {
	struct mgmt_msg_head msg_head;

	u8 port_id;
	u8 state;
	u16 rsvd1;
};

struct nic_pause_config {
	u8 auto_neg;
	u8 rx_pause;
	u8 tx_pause;
};

struct hinic3_cmd_pause_config {
	struct mgmt_msg_head msg_head;

	u8 port_id;
	u8 opcode;
	u16 rsvd1;
	u8 auto_neg;
	u8 rx_pause;
	u8 tx_pause;
	u8 rsvd2[5];
};

struct hinic3_vport_state {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 rsvd1;
	u8 state; /**< 0:disable, 1:enable. */
	u8 rsvd2[3];
};

#define MAG_CMD_PORT_DISABLE 0x0
#define MAG_CMD_TX_ENABLE    0x1
#define MAG_CMD_RX_ENABLE    0x2
/**
 * The physical port is disable only when all pf of the port are set to down, if
 * any pf is enable, the port is enable.
 */
struct mag_cmd_set_port_enable {
	struct mgmt_msg_head head;
	/* function_id should not more than the max support pf_id(32). */
	u16 function_id;
	u16 rsvd0;

	/* bitmap bit0:tx_en, bit1:rx_en. */
	u8 state;
	u8 rsvd1[3];
};

struct mag_cmd_get_port_enable {
	struct mgmt_msg_head head;

	u8 port;
	u8 state; /**< bitmap bit0:tx_en, bit1:rx_en. */
	u8 rsvd0[2];
};

struct hinic3_cmd_clear_qp_resource {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 rsvd1;
};

struct hinic3_port_stats_info {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 rsvd1;
};

struct hinic3_vport_stats {
	u64 tx_unicast_pkts_vport;
	u64 tx_unicast_bytes_vport;
	u64 tx_multicast_pkts_vport;
	u64 tx_multicast_bytes_vport;
	u64 tx_broadcast_pkts_vport;
	u64 tx_broadcast_bytes_vport;

	u64 rx_unicast_pkts_vport;
	u64 rx_unicast_bytes_vport;
	u64 rx_multicast_pkts_vport;
	u64 rx_multicast_bytes_vport;
	u64 rx_broadcast_pkts_vport;
	u64 rx_broadcast_bytes_vport;

	u64 tx_discard_vport;
	u64 rx_discard_vport;
	u64 tx_err_vport;
	u64 rx_err_vport;
};

struct hinic3_cmd_vport_stats {
	struct mgmt_msg_head msg_head;

	u32 stats_size;
	u32 rsvd1;
	struct hinic3_vport_stats stats;
	u64 rsvd2[6];
};

struct hinic3_phy_port_stats {
	u64 mac_rx_total_octs_port;
	u64 mac_tx_total_octs_port;
	u64 mac_rx_under_frame_pkts_port;
	u64 mac_rx_frag_pkts_port;
	u64 mac_rx_64_oct_pkts_port;
	u64 mac_rx_127_oct_pkts_port;
	u64 mac_rx_255_oct_pkts_port;
	u64 mac_rx_511_oct_pkts_port;
	u64 mac_rx_1023_oct_pkts_port;
	u64 mac_rx_max_oct_pkts_port;
	u64 mac_rx_over_oct_pkts_port;
	u64 mac_tx_64_oct_pkts_port;
	u64 mac_tx_127_oct_pkts_port;
	u64 mac_tx_255_oct_pkts_port;
	u64 mac_tx_511_oct_pkts_port;
	u64 mac_tx_1023_oct_pkts_port;
	u64 mac_tx_max_oct_pkts_port;
	u64 mac_tx_over_oct_pkts_port;
	u64 mac_rx_good_pkts_port;
	u64 mac_rx_crc_error_pkts_port;
	u64 mac_rx_broadcast_ok_port;
	u64 mac_rx_multicast_ok_port;
	u64 mac_rx_mac_frame_ok_port;
	u64 mac_rx_length_err_pkts_port;
	u64 mac_rx_vlan_pkts_port;
	u64 mac_rx_pause_pkts_port;
	u64 mac_rx_unknown_mac_frame_port;
	u64 mac_tx_good_pkts_port;
	u64 mac_tx_broadcast_ok_port;
	u64 mac_tx_multicast_ok_port;
	u64 mac_tx_underrun_pkts_port;
	u64 mac_tx_mac_frame_ok_port;
	u64 mac_tx_vlan_pkts_port;
	u64 mac_tx_pause_pkts_port;
};

struct mag_phy_port_stats {
	u64 mac_tx_fragment_pkt_num;
	u64 mac_tx_undersize_pkt_num;
	u64 mac_tx_undermin_pkt_num;
	u64 mac_tx_64_oct_pkt_num;
	u64 mac_tx_65_127_oct_pkt_num;
	u64 mac_tx_128_255_oct_pkt_num;
	u64 mac_tx_256_511_oct_pkt_num;
	u64 mac_tx_512_1023_oct_pkt_num;
	u64 mac_tx_1024_1518_oct_pkt_num;
	u64 mac_tx_1519_2047_oct_pkt_num;
	u64 mac_tx_2048_4095_oct_pkt_num;
	u64 mac_tx_4096_8191_oct_pkt_num;
	u64 mac_tx_8192_9216_oct_pkt_num;
	u64 mac_tx_9217_12287_oct_pkt_num;
	u64 mac_tx_12288_16383_oct_pkt_num;
	u64 mac_tx_1519_max_bad_pkt_num;
	u64 mac_tx_1519_max_good_pkt_num;
	u64 mac_tx_oversize_pkt_num;
	u64 mac_tx_jabber_pkt_num;
	u64 mac_tx_bad_pkt_num;
	u64 mac_tx_bad_oct_num;
	u64 mac_tx_good_pkt_num;
	u64 mac_tx_good_oct_num;
	u64 mac_tx_total_pkt_num;
	u64 mac_tx_total_oct_num;
	u64 mac_tx_uni_pkt_num;
	u64 mac_tx_multi_pkt_num;
	u64 mac_tx_broad_pkt_num;
	u64 mac_tx_pause_num;
	u64 mac_tx_pfc_pkt_num;
	u64 mac_tx_pfc_pri0_pkt_num;
	u64 mac_tx_pfc_pri1_pkt_num;
	u64 mac_tx_pfc_pri2_pkt_num;
	u64 mac_tx_pfc_pri3_pkt_num;
	u64 mac_tx_pfc_pri4_pkt_num;
	u64 mac_tx_pfc_pri5_pkt_num;
	u64 mac_tx_pfc_pri6_pkt_num;
	u64 mac_tx_pfc_pri7_pkt_num;
	u64 mac_tx_control_pkt_num;
	u64 mac_tx_err_all_pkt_num;
	u64 mac_tx_from_app_good_pkt_num;
	u64 mac_tx_from_app_bad_pkt_num;

	u64 mac_rx_fragment_pkt_num;
	u64 mac_rx_undersize_pkt_num;
	u64 mac_rx_undermin_pkt_num;
	u64 mac_rx_64_oct_pkt_num;
	u64 mac_rx_65_127_oct_pkt_num;
	u64 mac_rx_128_255_oct_pkt_num;
	u64 mac_rx_256_511_oct_pkt_num;
	u64 mac_rx_512_1023_oct_pkt_num;
	u64 mac_rx_1024_1518_oct_pkt_num;
	u64 mac_rx_1519_2047_oct_pkt_num;
	u64 mac_rx_2048_4095_oct_pkt_num;
	u64 mac_rx_4096_8191_oct_pkt_num;
	u64 mac_rx_8192_9216_oct_pkt_num;
	u64 mac_rx_9217_12287_oct_pkt_num;
	u64 mac_rx_12288_16383_oct_pkt_num;
	u64 mac_rx_1519_max_bad_pkt_num;
	u64 mac_rx_1519_max_good_pkt_num;
	u64 mac_rx_oversize_pkt_num;
	u64 mac_rx_jabber_pkt_num;
	u64 mac_rx_bad_pkt_num;
	u64 mac_rx_bad_oct_num;
	u64 mac_rx_good_pkt_num;
	u64 mac_rx_good_oct_num;
	u64 mac_rx_total_pkt_num;
	u64 mac_rx_total_oct_num;
	u64 mac_rx_uni_pkt_num;
	u64 mac_rx_multi_pkt_num;
	u64 mac_rx_broad_pkt_num;
	u64 mac_rx_pause_num;
	u64 mac_rx_pfc_pkt_num;
	u64 mac_rx_pfc_pri0_pkt_num;
	u64 mac_rx_pfc_pri1_pkt_num;
	u64 mac_rx_pfc_pri2_pkt_num;
	u64 mac_rx_pfc_pri3_pkt_num;
	u64 mac_rx_pfc_pri4_pkt_num;
	u64 mac_rx_pfc_pri5_pkt_num;
	u64 mac_rx_pfc_pri6_pkt_num;
	u64 mac_rx_pfc_pri7_pkt_num;
	u64 mac_rx_control_pkt_num;
	u64 mac_rx_sym_err_pkt_num;
	u64 mac_rx_fcs_err_pkt_num;
	u64 mac_rx_send_app_good_pkt_num;
	u64 mac_rx_send_app_bad_pkt_num;
	u64 mac_rx_unfilter_pkt_num;
};

struct mag_cmd_port_stats_info {
	struct mgmt_msg_head head;

	u8 port_id;
	u8 rsvd0[3];
};

struct mag_cmd_get_port_stat {
	struct mgmt_msg_head head;

	struct mag_phy_port_stats counter;
	u64 rsvd1[15];
};

struct param_head {
	u8 valid_len;
	u8 info_type;
	u8 rsvd[2];
};

struct mag_port_link_param {
	struct param_head head;

	u8 an;
	u8 fec;
	u8 speed;
	u8 rsvd0;

	u32 used;
	u32 an_fec_ability;
	u32 an_speed_ability;
	u32 an_pause_ability;
};

struct mag_port_wire_info {
	struct param_head head;

	u8 status;
	u8 rsvd0[3];

	u8 wire_type;
	u8 default_fec;
	u8 speed;
	u8 rsvd1;
	u32 speed_ability;
};

struct mag_port_adapt_info {
	struct param_head head;

	u32 adapt_en;
	u32 flash_adapt;
	u32 rsvd0[2];

	u32 wire_node;
	u32 an_en;
	u32 speed;
	u32 fec;
};

struct mag_port_param_info {
	u8 parameter_cnt;
	u8 lane_id;
	u8 lane_num;
	u8 rsvd0;

	struct mag_port_link_param default_cfg;
	struct mag_port_link_param bios_cfg;
	struct mag_port_link_param tool_cfg;
	struct mag_port_link_param final_cfg;

	struct mag_port_wire_info wire_info;
	struct mag_port_adapt_info adapt_info;
};

#define XSFP_VENDOR_NAME_LEN 16
struct mag_cmd_event_port_info {
	struct mgmt_msg_head head;

	u8 port_id;
	u8 event_type;
	u8 rsvd0[2];

	/* Optical Module Related. */
	u8 vendor_name[XSFP_VENDOR_NAME_LEN];
	u32 port_type;	   /**< fiber / copper. */
	u32 port_sub_type; /**< sr / lr. */
	u32 cable_length;  /**< 1 / 3 / 5 m. */
	u8 cable_temp;	   /**< Temperature. */
	u8 max_speed;	   /**< Max rate of optical module. */
	u8 sfp_type;	   /**< sfp / qsfp. */
	u8 rsvd1;
	u32 power[4]; /**< Optical power. */

	u8 an_state;
	u8 fec;
	u16 speed;

	u8 gpio_insert; /**< 0: present, 1: absent. */
	u8 alos;
	u8 rx_los;
	u8 pma_ctrl;

	u32 pma_fifo_reg;
	u32 pma_signal_ok_reg;
	u32 pcs_64_66b_reg;
	u32 rf_lf;
	u8 pcs_link;
	u8 pcs_mac_link;
	u8 tx_enable;
	u8 rx_enable;
	u32 pcs_err_cnt;

	u8 eq_data[38];
	u8 rsvd2[2];

	u32 his_link_machine_state;
	u32 cur_link_machine_state;
	u8 his_machine_state_data[128];
	u8 cur_machine_state_data[128];
	u8 his_machine_state_length;
	u8 cur_machine_state_length;

	struct mag_port_param_info param_info;
	u8 rsvd3[360];
};

struct hinic3_port_stats {
	struct mgmt_msg_head msg_head;

	struct hinic3_phy_port_stats stats;
};

struct hinic3_cmd_clear_vport_stats {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 rsvd;
};

struct hinic3_cmd_clear_port_stats {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 rsvd;
};

struct hinic3_cmd_qpn {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 base_qpn;
};

enum hinic3_func_tbl_cfg_bitmap {
	FUNC_CFG_INIT,
	FUNC_CFG_RX_BUF_SIZE,
	FUNC_CFG_MTU,
};

struct hinic3_func_tbl_cfg {
	u16 rx_wqe_buf_size;
	u16 mtu;
	u32 rsvd[9];
};

struct hinic3_cmd_set_func_tbl {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 rsvd;

	u32 cfg_bitmap;
	struct hinic3_func_tbl_cfg tbl_cfg;
};

struct hinic3_rx_mode_config {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 rsvd1;
	u32 rx_mode;
};

struct hinic3_cmd_vlan_offload {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 vlan_offload;
	u8 rsvd1[5];
};

#define HINIC3_CMD_OP_GET 0
#define HINIC3_CMD_OP_SET 1

struct hinic3_cmd_lro_config {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 opcode;
	u8 rsvd1;
	u8 lro_ipv4_en;
	u8 lro_ipv6_en;
	u8 lro_max_pkt_len; /**< Unit size is 1K. */
	u8 resv2[13];
};

struct hinic3_cmd_lro_timer {
	struct mgmt_msg_head msg_head;

	u8 opcode; /**< 1: set timer value, 0: get timer value. */
	u8 rsvd1;
	u16 rsvd2;
	u32 timer;
};

struct hinic3_rss_template_mgmt {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 cmd;
	u8 template_id;
	u8 rsvd1[4];
};

struct hinic3_cmd_rss_hash_key {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 opcode;
	u8 rsvd1;
	u8 key[HINIC3_RSS_KEY_SIZE];
};

struct hinic3_rss_indir_table {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 rsvd1;
	u8 indir[HINIC3_RSS_INDIR_SIZE];
};

struct nic_rss_indirect_tbl {
	u32 rsvd[4]; /**< Make sure that 16B beyond entry[]. */
	u16 entry[HINIC3_RSS_INDIR_SIZE];
};

struct nic_rss_context_tbl {
	u32 rsvd[4];
	u32 ctx;
};

struct hinic3_rss_context_table {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 rsvd1;
	u32 context;
};

struct hinic3_cmd_rss_engine_type {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 opcode;
	u8 hash_engine;
	u8 rsvd1[4];
};

struct hinic3_cmd_rss_config {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 rss_en;
	u8 rq_priority_number;
	u8 prio_tc[HINIC3_DCB_UP_MAX];
	u32 rsvd1;
};

enum {
	HINIC3_IFLA_VF_LINK_STATE_AUTO,	   /**< Link state of the uplink. */
	HINIC3_IFLA_VF_LINK_STATE_ENABLE,  /**< Link always up. */
	HINIC3_IFLA_VF_LINK_STATE_DISABLE, /**< Link always down. */
};

struct hinic3_dcb_state {
	u8 dcb_on;
	u8 default_cos;
	u8 trust;
	u8 rsvd1;
	u8 up_cos[HINIC3_DCB_UP_MAX];
	u8 dscp2cos[64];
	u32 rsvd2[7];
};

struct hinic3_cmd_vf_dcb_state {
	struct mgmt_msg_head msg_head;

	struct hinic3_dcb_state state;
};

struct hinic3_cmd_register_vf {
	struct mgmt_msg_head msg_head;

	u8 op_register; /* 0: unregister, 1: register. */
	u8 rsvd[39];
};

struct hinic3_tcam_result {
	u32 qid;
	u32 rsvd;
};

#define HINIC3_TCAM_FLOW_KEY_SIZE     44
#define HINIC3_MAX_TCAM_RULES_NUM     4096
#define HINIC3_TCAM_BLOCK_ENABLE      1
#define HINIC3_TCAM_BLOCK_DISABLE     0
#define HINIC3_TCAM_BLOCK_NORMAL_TYPE 0

struct hinic3_tcam_key_x_y {
	u8 x[HINIC3_TCAM_FLOW_KEY_SIZE];
	u8 y[HINIC3_TCAM_FLOW_KEY_SIZE];
};

struct hinic3_tcam_cfg_rule {
	u32 index;
	struct hinic3_tcam_result data;
	struct hinic3_tcam_key_x_y key;
};

/* Define the TCAM type. */
#define TCAM_RULE_FDIR_TYPE 0
#define TCAM_RULE_PPA_TYPE  1

struct hinic3_fdir_add_rule {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 type;
	u8 rsvd;
	struct hinic3_tcam_cfg_rule rule;
};

struct hinic3_fdir_del_rule {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 type;
	u8 rsvd;
	u32 index_start;
	u32 index_num;
};

struct hinic3_flush_tcam_rules {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u16 rsvd;
};

struct hinic3_tcam_block {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 alloc_en; /* 0: free tcam block, 1: alloc tcam block. */
	u8 tcam_type;
	u16 tcam_block_index;
	u16 rsvd;
};

struct hinic3_port_tcam_info {
	struct mgmt_msg_head msg_head;

	u16 func_id;
	u8 tcam_enable;
	u8 rsvd1;
	u32 rsvd2;
};

struct hinic3_set_fdir_ethertype_rule {
	struct mgmt_msg_head head;

	u16 func_id;
	u16 rsvd1;
	u8 pkt_type_en;
	u8 pkt_type;
	u8 qid;
	u8 rsvd2;
};

struct hinic3_cmd_set_rq_flush {
	union {
		struct {
			u16 global_rq_id;
			u16 local_rq_id;
		};
		u32 value;
	};
};

enum hinic3_link_follow_status {
	HINIC3_LINK_FOLLOW_DEFAULT,
	HINIC3_LINK_FOLLOW_PORT,
	HINIC3_LINK_FOLLOW_SEPARATE,
	HINIC3_LINK_FOLLOW_STATUS_MAX,
};

struct mag_cmd_set_link_follow {
	struct mgmt_msg_head head;
	u16 function_id;
	u16 rsvd0;
	u8 follow;
	u8 rsvd1[3];
};

int l2nic_msg_to_mgmt_sync(void *hwdev, u16 cmd, void *buf_in, u16 in_size,
			   void *buf_out, u16 *out_size);

int hinic3_set_ci_table(void *hwdev, struct hinic3_sq_attr *attr);

/**
 * Update MAC address to hardware.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] old_mac
 * Old MAC addr to delete.
 * @param[in] new_mac
 * New MAC addr to update.
 * @param[in] vlan_id
 * Vlan id.
 * @param func_id
 * Function index.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_update_mac(void *hwdev, u8 *old_mac, u8 *new_mac, u16 vlan_id,
		      u16 func_id);

/**
 * Get the default mac address.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] mac_addr
 * Mac address from hardware.
 * @param[in] ether_len
 * The length of mac address.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_get_default_mac(void *hwdev, u8 *mac_addr, int ether_len);

/**
 * Set mac address.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] mac_addr
 * Mac address from hardware.
 * @param[in] vlan_id
 * Vlan id.
 * @param[in] func_id
 * Function index.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_mac(void *hwdev, const u8 *mac_addr, u16 vlan_id, u16 func_id);

/**
 * Delete MAC address.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] mac_addr
 * MAC address from hardware.
 * @param[in] vlan_id
 * Vlan id.
 * @param[in] func_id
 * Function index.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_del_mac(void *hwdev, const u8 *mac_addr, u16 vlan_id, u16 func_id);

/**
 * Set function mtu.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] new_mtu
 * MTU value.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_port_mtu(void *hwdev, u16 new_mtu);

/**
 * Set function valid status.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] enable
 * 0: disable, 1: enable.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_vport_enable(void *hwdev, bool enable);

/**
 * Set port status.
 *
 * @param[in] hwdev
 * Device pointer to hwdev
 * @param[in] enable
 * 0: disable, 1: enable.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_port_enable(void *hwdev, bool enable);

/**
 * Get link state.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[out] link_state
 * Link state, 0: link down, 1: link up.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_get_link_state(void *hwdev, u8 *link_state);

/**
 * Flush queue pairs resource in hardware.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_flush_qps_res(void *hwdev);

/**
 * Set pause info.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] nic_pause
 * Pause info.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_pause_info(void *hwdev, struct nic_pause_config nic_pause);

/**
 * Get pause info.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[out] nic_pause
 * Pause info.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_get_pause_info(void *hwdev, struct nic_pause_config *nic_pause);

/**
 * Get function stats.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[out] stats
 * Function stats.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_get_vport_stats(void *hwdev, struct hinic3_vport_stats *stats);

/**
 * Get port stats.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[out] stats
 * Port stats.
 *
 * @return
 * 0 on success, non-zero on failure.
 */

int hinic3_get_phy_port_stats(void *hwdev, struct mag_phy_port_stats *stats);

int hinic3_clear_vport_stats(void *hwdev);

int hinic3_clear_phy_port_stats(void *hwdev);

/**
 * Init nic hwdev.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_init_nic_hwdev(void *hwdev);

/**
 * Free nic hwdev.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 */
void hinic3_free_nic_hwdev(void *hwdev);

/**
 * Set function rx mode.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] enable
 * Rx mode state, 0-disable, 1-enable.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_rx_mode(void *hwdev, u32 enable);

/**
 * Set function vlan offload valid state.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] enable
 * Rx mode state, 0-disable, 1-enable.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_rx_vlan_offload(void *hwdev, u8 en);

/**
 * Set rx LRO configuration.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] lro_en
 * LRO enable state, 0-disable, 1-enable.
 * @param[in] lro_timer
 * LRO aggregation timeout.
 * @param[in] lro_max_pkt_len
 * LRO coalesce packet size(unit size is 1K).
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_rx_lro_state(void *hwdev, u8 lro_en, u32 lro_timer,
			    u32 lro_max_pkt_len);

/**
 * Get port info.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[out] port_info
 * Port info, including autoneg, port type, duplex, speed and fec mode.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_get_port_info(void *hwdev, struct nic_port_info *port_info);

int hinic3_init_function_table(void *hwdev, u16 rx_buff_len);

/**
 * Alloc RSS template table.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_rss_template_alloc(void *hwdev);

/**
 * Free RSS template table.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_rss_template_free(void *hwdev);

/**
 * Set RSS indirect table.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] indir_table
 * RSS indirect table.
 * @param[in] indir_table_size
 * RSS indirect table size.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_rss_set_indir_tbl(void *hwdev, const u32 *indir_table,
			     u32 indir_table_size);

/**
 * Get RSS indirect table.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[out] indir_table
 * RSS indirect table.
 * @param[in] indir_table_size
 * RSS indirect table size.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_rss_get_indir_tbl(void *hwdev, u32 *indir_table,
			     u32 indir_table_size);

/**
 * Set RSS type.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] rss_type
 * RSS type, including ipv4, tcpv4, ipv6, tcpv6 and etc.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_rss_type(void *hwdev, struct hinic3_rss_type rss_type);

/**
 * Get RSS type.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[out] rss_type
 * RSS type, including ipv4, tcpv4, ipv6, tcpv6 and etc.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_get_rss_type(void *hwdev, struct hinic3_rss_type *rss_type);

/**
 * Get RSS hash engine.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[out] type
 * RSS hash engine, pmd driver only supports Toeplitz.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_rss_get_hash_engine(void *hwdev, u8 *type);

/**
 * Set RSS hash engine.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] type
 * RSS hash engine, pmd driver only supports Toeplitz.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_rss_set_hash_engine(void *hwdev, u8 type);

/**
 * Set RSS configuration.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] rss_en
 * RSS enable lag, 0-disable, 1-enable.
 * @param[in] tc_num
 * Number of TC.
 * @param[in] prio_tc
 * Priority of TC.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_rss_cfg(void *hwdev, u8 rss_en, u8 tc_num, u8 *prio_tc);

/**
 * Set RSS hash key.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] key
 * RSS hash key.
 * @param[in] key_size
 * hash key size.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_rss_set_hash_key(void *hwdev, u8 *key, u16 key_size);

/**
 * Add vlan to hardware.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] vlan_id
 * Vlan id.
 * @param[in] func_id
 * Function id.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_add_vlan(void *hwdev, u16 vlan_id, u16 func_id);

/**
 * Delete vlan.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] vlan_id
 * Vlan id.
 * @param[in] func_id
 * Function id.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_del_vlan(void *hwdev, u16 vlan_id, u16 func_id);

/**
 * Set vlan filter.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] vlan_filter_ctrl
 * Vlan filter enable flag, 0-disable, 1-enable.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_vlan_filter(void *hwdev, u32 vlan_filter_ctrl);

/**
 * Get VF function default cos.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[out] cos_id
 * Cos id.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_vf_get_default_cos(void *hwdev, u8 *cos_id);

/**
 * Add tcam rules.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] tcam_rule
 * Tcam rule, including tcam rule index, tcam action, tcam key and etc.
 * @param[in] tcam_rule_type
 * Tcam rule type.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_add_tcam_rule(void *hwdev, struct hinic3_tcam_cfg_rule *tcam_rule,
			 u8 tcam_rule_type);

/**
 * Del tcam rules.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] index
 * Tcam rule index.
 * @param[in] tcam_rule_type
 * Tcam rule type.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_del_tcam_rule(void *hwdev, u32 index, u8 tcam_rule_type);

/**
 * Alloc tcam block.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] index
 * Tcam block index.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_alloc_tcam_block(void *hwdev, u16 *index);

/**
 * Free tcam block.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] index
 * Tcam block index.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_free_tcam_block(void *hwdev, u16 *index);

/**
 * Set fdir tcam function enable or disable.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 * @param[in] enable
 * Tcam enable flag, 1-enable, 0-disable.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_fdir_tcam_rule_filter(void *hwdev, bool enable);

/**
 * Flush fdir tcam rule.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_flush_tcam_rule(void *hwdev);

int hinic3_set_rq_flush(void *hwdev, u16 q_id);

/**
 * Get service feature HW supported.
 *
 * @param[in] dev
 * Device pointer to hwdev.
 * @param[in] size
 * s_feature's array size.
 * @param[out] s_feature
 * s_feature HW supported.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_get_feature_from_hw(void *hwdev, u64 *s_feature, u16 size);

/**
 * Set service feature driver supported to hardware.
 *
 * @param[in] dev
 * Device pointer to hwdev.
 * @param[in] size
 * s_feature's array size.
 * @param[out] s_feature
 * s_feature HW supported.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_set_feature_to_hw(void *hwdev, u64 *s_feature, u16 size);

int hinic3_set_fdir_ethertype_filter(void *hwdev, u8 pkt_type, u16 queue_id,
				     u8 en);

int hinic3_set_link_status_follow(void *hwdev,
				  enum hinic3_link_follow_status status);
#endif /* _HINIC3_NIC_CFG_H_ */
