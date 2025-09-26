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

struct hinic3_rss_type {
	uint8_t tcp_ipv6_ext;
	uint8_t ipv6_ext;
	uint8_t tcp_ipv6;
	uint8_t ipv6;
	uint8_t tcp_ipv4;
	uint8_t ipv4;
	uint8_t udp_ipv6;
	uint8_t udp_ipv4;
};

enum hinic3_rss_hash_type {
	HINIC3_RSS_HASH_ENGINE_TYPE_XOR  = 0,
	HINIC3_RSS_HASH_ENGINE_TYPE_TOEP = 1,
	HINIC3_RSS_HASH_ENGINE_TYPE_MAX  = 2,
};

#define MAX_FEATURE_QWORD 4
struct hinic3_cmd_feature_nego {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t opcode; /**< 1: set, 0: get. */
	uint8_t rsvd;
	uint64_t s_feature[MAX_FEATURE_QWORD];
};

/* Structures for port info. */
struct nic_port_info {
	uint8_t port_type;
	uint8_t autoneg_cap;
	uint8_t autoneg_state;
	uint8_t duplex;
	uint8_t speed;
	uint8_t fec;
};

enum hinic3_link_status { HINIC3_LINK_DOWN = 0, HINIC3_LINK_UP };

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
	LINK_PORT_UNKNOWN		= 0,
	LINK_PORT_OPTICAL_MM		= 1,
	LINK_PORT_OPTICAL_SM		= 2,
	LINK_PORT_PAS_COPPER		= 3,
	LINK_PORT_ACC			= 4,
	LINK_PORT_BASET			= 5,
	LINK_PORT_AOC			= 64,
	LINK_PORT_ELECTRIC		= 65,
	LINK_PORT_BACKBOARD_INTERFACE	= 66,
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
	PORT_CFG_AN_ON  = 1,
	PORT_CFG_AN_OFF = 2
};

enum mag_cmd_port_speed {
	PORT_SPEED_NOT_SET = 0,
	PORT_SPEED_10MB    = 1,
	PORT_SPEED_100MB   = 2,
	PORT_SPEED_1GB     = 3,
	PORT_SPEED_10GB    = 4,
	PORT_SPEED_25GB    = 5,
	PORT_SPEED_40GB    = 6,
	PORT_SPEED_50GB    = 7,
	PORT_SPEED_100GB   = 8,
	PORT_SPEED_200GB   = 9,
	PORT_SPEED_UNKNOWN
};

struct hinic3_sq_attr {
	uint8_t dma_attr_off;
	uint8_t pending_limit;
	uint8_t coalescing_time;
	uint8_t intr_en;
	uint16_t intr_idx;
	uint32_t l2nic_sqn;
	uint64_t ci_dma_base;
};

struct hinic3_cmd_cons_idx_attr {
	struct mgmt_msg_head msg_head;

	uint16_t func_idx;
	uint8_t dma_attr_off;
	uint8_t pending_limit;
	uint8_t coalescing_time;
	uint8_t intr_en;
	uint16_t intr_idx;
	uint32_t l2nic_sqn;
	uint32_t rsvd;
	uint64_t ci_addr;
};

struct hinic3_port_mac_set {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t vlan_id;
	uint16_t rsvd1;
	uint8_t mac[ETH_ALEN];
};

struct hinic3_port_mac_update {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t vlan_id;
	uint16_t rsvd1;
	uint8_t old_mac[ETH_ALEN];
	uint16_t rsvd2;
	uint8_t new_mac[ETH_ALEN];
};

struct hinic3_ppa_cfg_state_cmd {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t ppa_state;
	uint8_t rsvd;
};

struct hinic3_ppa_cfg_mode_cmd {
	struct mgmt_msg_head msg_head;

	uint16_t rsvd0;
	uint8_t ppa_mode;
	uint8_t qpc_func_nums;
	uint16_t base_qpc_func_id;
	uint16_t rsvd1;
};

struct hinic3_ppa_cfg_flush_cmd {
	struct mgmt_msg_head msg_head;

	uint16_t rsvd0;
	uint8_t flush_en; /**< 0: flush done, 1: in flush operation. */
	uint8_t rsvd1;
};

struct hinic3_ppa_fdir_query_cmd {
	struct mgmt_msg_head msg_head;

	uint32_t index;
	uint32_t rsvd;
	uint64_t pkt_nums;
	uint64_t pkt_bytes;
};

#define HINIC3_CMD_OP_ADD 1
#define HINIC3_CMD_OP_DEL 0

struct hinic3_cmd_vlan_config {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t opcode;
	uint8_t rsvd1;
	uint16_t vlan_id;
	uint16_t rsvd2;
};

struct hinic3_cmd_set_vlan_filter {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t rsvd[2];
	/* Bit0: vlan filter en, bit1: broadcast filter en. */
	uint32_t vlan_filter_ctrl;
};

struct hinic3_cmd_port_info {
	struct mgmt_msg_head msg_head;

	uint8_t port_id;
	uint8_t rsvd1[3];
	uint8_t port_type;
	uint8_t autoneg_cap;
	uint8_t autoneg_state;
	uint8_t duplex;
	uint8_t speed;
	uint8_t fec;
	uint16_t rsvd2;
	uint32_t rsvd3[4];
};

struct hinic3_cmd_link_state {
	struct mgmt_msg_head msg_head;

	uint8_t port_id;
	uint8_t state;
	uint16_t rsvd1;
};

struct nic_pause_config {
	uint8_t auto_neg;
	uint8_t rx_pause;
	uint8_t tx_pause;
};

struct hinic3_cmd_pause_config {
	struct mgmt_msg_head msg_head;

	uint8_t port_id;
	uint8_t opcode;
	uint16_t rsvd1;
	uint8_t auto_neg;
	uint8_t rx_pause;
	uint8_t tx_pause;
	uint8_t rsvd2[5];
};

struct hinic3_vport_state {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t rsvd1;
	uint8_t state; /**< 0:disable, 1:enable. */
	uint8_t rsvd2[3];
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
	uint16_t function_id;
	uint16_t rsvd0;

	/* bitmap bit0:tx_en, bit1:rx_en. */
	uint8_t state;
	uint8_t rsvd1[3];
};

struct mag_cmd_get_port_enable {
	struct mgmt_msg_head head;

	uint8_t port;
	uint8_t state; /**< bitmap bit0:tx_en, bit1:rx_en. */
	uint8_t rsvd0[2];
};

struct hinic3_cmd_clear_qp_resource {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t rsvd1;
};

struct hinic3_port_stats_info {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t rsvd1;
};

struct hinic3_vport_stats {
	uint64_t tx_unicast_pkts_vport;
	uint64_t tx_unicast_bytes_vport;
	uint64_t tx_multicast_pkts_vport;
	uint64_t tx_multicast_bytes_vport;
	uint64_t tx_broadcast_pkts_vport;
	uint64_t tx_broadcast_bytes_vport;

	uint64_t rx_unicast_pkts_vport;
	uint64_t rx_unicast_bytes_vport;
	uint64_t rx_multicast_pkts_vport;
	uint64_t rx_multicast_bytes_vport;
	uint64_t rx_broadcast_pkts_vport;
	uint64_t rx_broadcast_bytes_vport;

	uint64_t tx_discard_vport;
	uint64_t rx_discard_vport;
	uint64_t tx_err_vport;
	uint64_t rx_err_vport;
};

struct hinic3_cmd_vport_stats {
	struct mgmt_msg_head msg_head;

	uint32_t stats_size;
	uint32_t rsvd1;
	struct hinic3_vport_stats stats;
	uint64_t rsvd2[6];
};

struct hinic3_phy_port_stats {
	uint64_t mac_rx_total_octs_port;
	uint64_t mac_tx_total_octs_port;
	uint64_t mac_rx_under_frame_pkts_port;
	uint64_t mac_rx_frag_pkts_port;
	uint64_t mac_rx_64_oct_pkts_port;
	uint64_t mac_rx_127_oct_pkts_port;
	uint64_t mac_rx_255_oct_pkts_port;
	uint64_t mac_rx_511_oct_pkts_port;
	uint64_t mac_rx_1023_oct_pkts_port;
	uint64_t mac_rx_max_oct_pkts_port;
	uint64_t mac_rx_over_oct_pkts_port;
	uint64_t mac_tx_64_oct_pkts_port;
	uint64_t mac_tx_127_oct_pkts_port;
	uint64_t mac_tx_255_oct_pkts_port;
	uint64_t mac_tx_511_oct_pkts_port;
	uint64_t mac_tx_1023_oct_pkts_port;
	uint64_t mac_tx_max_oct_pkts_port;
	uint64_t mac_tx_over_oct_pkts_port;
	uint64_t mac_rx_good_pkts_port;
	uint64_t mac_rx_crc_error_pkts_port;
	uint64_t mac_rx_broadcast_ok_port;
	uint64_t mac_rx_multicast_ok_port;
	uint64_t mac_rx_mac_frame_ok_port;
	uint64_t mac_rx_length_err_pkts_port;
	uint64_t mac_rx_vlan_pkts_port;
	uint64_t mac_rx_pause_pkts_port;
	uint64_t mac_rx_unknown_mac_frame_port;
	uint64_t mac_tx_good_pkts_port;
	uint64_t mac_tx_broadcast_ok_port;
	uint64_t mac_tx_multicast_ok_port;
	uint64_t mac_tx_underrun_pkts_port;
	uint64_t mac_tx_mac_frame_ok_port;
	uint64_t mac_tx_vlan_pkts_port;
	uint64_t mac_tx_pause_pkts_port;
};

struct mag_phy_port_stats {
	uint64_t mac_tx_fragment_pkt_num;
	uint64_t mac_tx_undersize_pkt_num;
	uint64_t mac_tx_undermin_pkt_num;
	uint64_t mac_tx_64_oct_pkt_num;
	uint64_t mac_tx_65_127_oct_pkt_num;
	uint64_t mac_tx_128_255_oct_pkt_num;
	uint64_t mac_tx_256_511_oct_pkt_num;
	uint64_t mac_tx_512_1023_oct_pkt_num;
	uint64_t mac_tx_1024_1518_oct_pkt_num;
	uint64_t mac_tx_1519_2047_oct_pkt_num;
	uint64_t mac_tx_2048_4095_oct_pkt_num;
	uint64_t mac_tx_4096_8191_oct_pkt_num;
	uint64_t mac_tx_8192_9216_oct_pkt_num;
	uint64_t mac_tx_9217_12287_oct_pkt_num;
	uint64_t mac_tx_12288_16383_oct_pkt_num;
	uint64_t mac_tx_1519_max_bad_pkt_num;
	uint64_t mac_tx_1519_max_good_pkt_num;
	uint64_t mac_tx_oversize_pkt_num;
	uint64_t mac_tx_jabber_pkt_num;
	uint64_t mac_tx_bad_pkt_num;
	uint64_t mac_tx_bad_oct_num;
	uint64_t mac_tx_good_pkt_num;
	uint64_t mac_tx_good_oct_num;
	uint64_t mac_tx_total_pkt_num;
	uint64_t mac_tx_total_oct_num;
	uint64_t mac_tx_uni_pkt_num;
	uint64_t mac_tx_multi_pkt_num;
	uint64_t mac_tx_broad_pkt_num;
	uint64_t mac_tx_pause_num;
	uint64_t mac_tx_pfc_pkt_num;
	uint64_t mac_tx_pfc_pri0_pkt_num;
	uint64_t mac_tx_pfc_pri1_pkt_num;
	uint64_t mac_tx_pfc_pri2_pkt_num;
	uint64_t mac_tx_pfc_pri3_pkt_num;
	uint64_t mac_tx_pfc_pri4_pkt_num;
	uint64_t mac_tx_pfc_pri5_pkt_num;
	uint64_t mac_tx_pfc_pri6_pkt_num;
	uint64_t mac_tx_pfc_pri7_pkt_num;
	uint64_t mac_tx_control_pkt_num;
	uint64_t mac_tx_err_all_pkt_num;
	uint64_t mac_tx_from_app_good_pkt_num;
	uint64_t mac_tx_from_app_bad_pkt_num;

	uint64_t mac_rx_fragment_pkt_num;
	uint64_t mac_rx_undersize_pkt_num;
	uint64_t mac_rx_undermin_pkt_num;
	uint64_t mac_rx_64_oct_pkt_num;
	uint64_t mac_rx_65_127_oct_pkt_num;
	uint64_t mac_rx_128_255_oct_pkt_num;
	uint64_t mac_rx_256_511_oct_pkt_num;
	uint64_t mac_rx_512_1023_oct_pkt_num;
	uint64_t mac_rx_1024_1518_oct_pkt_num;
	uint64_t mac_rx_1519_2047_oct_pkt_num;
	uint64_t mac_rx_2048_4095_oct_pkt_num;
	uint64_t mac_rx_4096_8191_oct_pkt_num;
	uint64_t mac_rx_8192_9216_oct_pkt_num;
	uint64_t mac_rx_9217_12287_oct_pkt_num;
	uint64_t mac_rx_12288_16383_oct_pkt_num;
	uint64_t mac_rx_1519_max_bad_pkt_num;
	uint64_t mac_rx_1519_max_good_pkt_num;
	uint64_t mac_rx_oversize_pkt_num;
	uint64_t mac_rx_jabber_pkt_num;
	uint64_t mac_rx_bad_pkt_num;
	uint64_t mac_rx_bad_oct_num;
	uint64_t mac_rx_good_pkt_num;
	uint64_t mac_rx_good_oct_num;
	uint64_t mac_rx_total_pkt_num;
	uint64_t mac_rx_total_oct_num;
	uint64_t mac_rx_uni_pkt_num;
	uint64_t mac_rx_multi_pkt_num;
	uint64_t mac_rx_broad_pkt_num;
	uint64_t mac_rx_pause_num;
	uint64_t mac_rx_pfc_pkt_num;
	uint64_t mac_rx_pfc_pri0_pkt_num;
	uint64_t mac_rx_pfc_pri1_pkt_num;
	uint64_t mac_rx_pfc_pri2_pkt_num;
	uint64_t mac_rx_pfc_pri3_pkt_num;
	uint64_t mac_rx_pfc_pri4_pkt_num;
	uint64_t mac_rx_pfc_pri5_pkt_num;
	uint64_t mac_rx_pfc_pri6_pkt_num;
	uint64_t mac_rx_pfc_pri7_pkt_num;
	uint64_t mac_rx_control_pkt_num;
	uint64_t mac_rx_sym_err_pkt_num;
	uint64_t mac_rx_fcs_err_pkt_num;
	uint64_t mac_rx_send_app_good_pkt_num;
	uint64_t mac_rx_send_app_bad_pkt_num;
	uint64_t mac_rx_unfilter_pkt_num;
};

struct mag_cmd_port_stats_info {
	struct mgmt_msg_head head;

	uint8_t port_id;
	uint8_t rsvd0[3];
};

struct mag_cmd_get_port_stat {
	struct mgmt_msg_head head;

	struct mag_phy_port_stats counter;
	uint64_t rsvd1[15];
};

struct param_head {
	uint8_t valid_len;
	uint8_t info_type;
	uint8_t rsvd[2];
};

struct mag_port_link_param {
	struct param_head head;

	uint8_t an;
	uint8_t fec;
	uint8_t speed;
	uint8_t rsvd0;

	uint32_t used;
	uint32_t an_fec_ability;
	uint32_t an_speed_ability;
	uint32_t an_pause_ability;
};

struct mag_port_wire_info {
	struct param_head head;

	uint8_t status;
	uint8_t rsvd0[3];

	uint8_t wire_type;
	uint8_t default_fec;
	uint8_t speed;
	uint8_t rsvd1;
	uint32_t speed_ability;
};

struct mag_port_adapt_info {
	struct param_head head;

	uint32_t adapt_en;
	uint32_t flash_adapt;
	uint32_t rsvd0[2];

	uint32_t wire_node;
	uint32_t an_en;
	uint32_t speed;
	uint32_t fec;
};

struct mag_port_param_info {
	uint8_t parameter_cnt;
	uint8_t lane_id;
	uint8_t lane_num;
	uint8_t rsvd0;

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

	uint8_t port_id;
	uint8_t event_type;
	uint8_t rsvd0[2];

	/* Optical Module Related. */
	uint8_t vendor_name[XSFP_VENDOR_NAME_LEN];
	uint32_t port_type;	   /**< fiber / copper. */
	uint32_t port_sub_type; /**< sr / lr. */
	uint32_t cable_length;  /**< 1 / 3 / 5 m. */
	uint8_t cable_temp;	   /**< Temperature. */
	uint8_t max_speed;	   /**< Max rate of optical module. */
	uint8_t sfp_type;	   /**< sfp / qsfp. */
	uint8_t rsvd1;
	uint32_t power[4]; /**< Optical power. */

	uint8_t an_state;
	uint8_t fec;
	uint16_t speed;

	uint8_t gpio_insert; /**< 0: present, 1: absent. */
	uint8_t alos;
	uint8_t rx_los;
	uint8_t pma_ctrl;

	uint32_t pma_fifo_reg;
	uint32_t pma_signal_ok_reg;
	uint32_t pcs_64_66b_reg;
	uint32_t rf_lf;
	uint8_t pcs_link;
	uint8_t pcs_mac_link;
	uint8_t tx_enable;
	uint8_t rx_enable;
	uint32_t pcs_err_cnt;

	uint8_t eq_data[38];
	uint8_t rsvd2[2];

	uint32_t his_link_machine_state;
	uint32_t cur_link_machine_state;
	uint8_t his_machine_state_data[128];
	uint8_t cur_machine_state_data[128];
	uint8_t his_machine_state_length;
	uint8_t cur_machine_state_length;

	struct mag_port_param_info param_info;
	uint8_t rsvd3[360];
};

struct hinic3_port_stats {
	struct mgmt_msg_head msg_head;

	struct hinic3_phy_port_stats stats;
};

struct hinic3_cmd_clear_vport_stats {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t rsvd;
};

struct hinic3_cmd_clear_port_stats {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t rsvd;
};

struct hinic3_cmd_qpn {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t base_qpn;
};

enum hinic3_func_tbl_cfg_bitmap {
	FUNC_CFG_INIT,
	FUNC_CFG_RX_BUF_SIZE,
	FUNC_CFG_MTU,
};

struct hinic3_func_tbl_cfg {
	uint16_t rx_wqe_buf_size;
	uint16_t mtu;
	uint32_t rsvd[9];
};

struct hinic3_cmd_set_func_tbl {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t rsvd;

	uint32_t cfg_bitmap;
	struct hinic3_func_tbl_cfg tbl_cfg;
};

struct hinic3_rx_mode_config {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t rsvd1;
	uint32_t rx_mode;
};

struct hinic3_cmd_vlan_offload {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t vlan_offload;
	uint8_t rsvd1[5];
};

#define HINIC3_CMD_OP_GET 0
#define HINIC3_CMD_OP_SET 1

struct hinic3_cmd_lro_config {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t opcode;
	uint8_t rsvd1;
	uint8_t lro_ipv4_en;
	uint8_t lro_ipv6_en;
	uint8_t lro_max_pkt_len; /**< Unit size is 1K. */
	uint8_t rsvd2[13];
};

struct hinic3_cmd_lro_timer {
	struct mgmt_msg_head msg_head;

	uint8_t opcode; /**< 1: set timer value, 0: get timer value. */
	uint8_t rsvd1;
	uint16_t rsvd2;
	uint32_t timer;
};

struct hinic3_rss_template_mgmt {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t cmd;
	uint8_t template_id;
	uint8_t rsvd1[4];
};

struct hinic3_cmd_rss_hash_key {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t opcode;
	uint8_t rsvd1;
	uint8_t key[HINIC3_RSS_KEY_SIZE];
};

struct hinic3_rss_indir_table {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t rsvd1;
	uint8_t indir[HINIC3_RSS_INDIR_SIZE];
};

struct nic_rss_indirect_tbl {
	uint32_t rsvd[4]; /**< Make sure that 16B beyond entry[]. */
	uint16_t entry[HINIC3_RSS_INDIR_SIZE];
};

struct nic_rss_context_tbl {
	uint32_t rsvd[4];
	uint32_t ctx;
};

struct hinic3_rss_context_table {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t rsvd1;
	uint32_t context;
};

struct hinic3_cmd_rss_engine_type {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t opcode;
	uint8_t hash_engine;
	uint8_t rsvd1[4];
};

struct hinic3_cmd_rss_config {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t rss_en;
	uint8_t rq_priority_number;
	uint8_t prio_tc[HINIC3_DCB_UP_MAX];
	uint32_t rsvd1;
};

enum {
	HINIC3_IFLA_VF_LINK_STATE_AUTO,	   /**< Link state of the uplink. */
	HINIC3_IFLA_VF_LINK_STATE_ENABLE,  /**< Link always up. */
	HINIC3_IFLA_VF_LINK_STATE_DISABLE, /**< Link always down. */
};

struct hinic3_dcb_state {
	uint8_t dcb_on;
	uint8_t default_cos;
	uint8_t trust;
	uint8_t rsvd1;
	uint8_t up_cos[HINIC3_DCB_UP_MAX];
	uint8_t dscp2cos[64];
	uint32_t rsvd2[7];
};

struct hinic3_cmd_vf_dcb_state {
	struct mgmt_msg_head msg_head;

	struct hinic3_dcb_state state;
};

struct hinic3_cmd_register_vf {
	struct mgmt_msg_head msg_head;

	uint8_t op_register; /* 0: unregister, 1: register. */
	uint8_t rsvd[39];
};

struct hinic3_tcam_result {
	uint32_t qid;
	uint32_t rsvd;
};

#define HINIC3_TCAM_FLOW_KEY_SIZE     44
#define HINIC3_MAX_TCAM_RULES_NUM     4096
#define HINIC3_TCAM_BLOCK_ENABLE      1
#define HINIC3_TCAM_BLOCK_DISABLE     0
#define HINIC3_TCAM_BLOCK_NORMAL_TYPE 0

struct hinic3_tcam_key_x_y {
	uint8_t x[HINIC3_TCAM_FLOW_KEY_SIZE];
	uint8_t y[HINIC3_TCAM_FLOW_KEY_SIZE];
};

struct hinic3_tcam_cfg_rule {
	uint32_t index;
	struct hinic3_tcam_result data;
	struct hinic3_tcam_key_x_y key;
};

/* Define the TCAM type. */
#define TCAM_RULE_FDIR_TYPE 0
#define TCAM_RULE_PPA_TYPE  1

struct hinic3_fdir_add_rule {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t type;
	uint8_t rsvd;
	struct hinic3_tcam_cfg_rule rule;
};

struct hinic3_fdir_del_rule {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t type;
	uint8_t rsvd;
	uint32_t index_start;
	uint32_t index_num;
};

struct hinic3_flush_tcam_rules {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint16_t rsvd;
};

struct hinic3_tcam_block {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t alloc_en; /* 0: free tcam block, 1: alloc tcam block. */
	uint8_t tcam_type;
	uint16_t tcam_block_index;
	uint16_t rsvd;
};

struct hinic3_port_tcam_info {
	struct mgmt_msg_head msg_head;

	uint16_t func_id;
	uint8_t tcam_enable;
	uint8_t rsvd1;
	uint32_t rsvd2;
};

struct hinic3_set_fdir_ethertype_rule {
	struct mgmt_msg_head head;

	uint16_t func_id;
	uint16_t rsvd1;
	uint8_t pkt_type_en;
	uint8_t pkt_type;
	uint8_t qid;
	uint8_t rsvd2;
};

struct hinic3_cmd_set_rq_flush {
	union {
		struct {
			uint16_t global_rq_id;
			uint16_t local_rq_id;
		};
		uint32_t value;
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
	uint16_t function_id;
	uint16_t rsvd0;
	uint8_t follow;
	uint8_t rsvd1[3];
};

int hinic3_msg_to_mgmt_sync(struct hinic3_hwdev *hwdev, enum hinic3_mod_type mod,
			    uint16_t cmd, void *buf_in, uint16_t in_size,
			    void *buf_out, uint16_t *out_size);

int hinic3_set_ci_table(struct hinic3_hwdev *hwdev, struct hinic3_sq_attr *attr);

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
int hinic3_update_mac(struct hinic3_hwdev *hwdev, uint8_t *old_mac,
		      uint8_t *new_mac, uint16_t vlan_id, uint16_t func_id);

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
int hinic3_get_default_mac(struct hinic3_hwdev *hwdev, uint8_t *mac_addr, int ether_len);

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
int hinic3_set_mac(struct hinic3_hwdev *hwdev, const uint8_t *mac_addr,
		   uint16_t vlan_id, uint16_t func_id);

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
int hinic3_del_mac(struct hinic3_hwdev *hwdev, const uint8_t *mac_addr,
		   uint16_t vlan_id, uint16_t func_id);

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
int hinic3_set_port_mtu(struct hinic3_hwdev *hwdev, uint16_t new_mtu);

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
int hinic3_set_vport_enable(struct hinic3_hwdev *hwdev, bool enable);

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
int hinic3_set_port_enable(struct hinic3_hwdev *hwdev, bool enable);

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
int hinic3_get_link_state(struct hinic3_hwdev *hwdev, uint8_t *link_state);

/**
 * Flush queue pairs resource in hardware.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_flush_qps_res(struct hinic3_hwdev *hwdev);

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
int hinic3_set_pause_info(struct hinic3_hwdev *hwdev, struct nic_pause_config nic_pause);

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
int hinic3_get_pause_info(struct hinic3_hwdev *hwdev, struct nic_pause_config *nic_pause);

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
int hinic3_get_vport_stats(struct hinic3_hwdev *hwdev, struct hinic3_vport_stats *stats);

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

int hinic3_get_phy_port_stats(struct hinic3_hwdev *hwdev, struct mag_phy_port_stats *stats);

int hinic3_clear_vport_stats(struct hinic3_hwdev *hwdev);

int hinic3_clear_phy_port_stats(struct hinic3_hwdev *hwdev);

/**
 * Init nic hwdev.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_init_nic_hwdev(struct hinic3_hwdev *hwdev);

/**
 * Free nic hwdev.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 */
void hinic3_free_nic_hwdev(struct hinic3_hwdev *hwdev);

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
int hinic3_set_rx_mode(struct hinic3_hwdev *hwdev, uint32_t enable);

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
int hinic3_set_rx_vlan_offload(struct hinic3_hwdev *hwdev, uint8_t en);

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
int hinic3_set_rx_lro_state(struct hinic3_hwdev *hwdev, uint8_t lro_en, uint32_t lro_timer,
			    uint32_t lro_max_pkt_len);

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
int hinic3_get_port_info(struct hinic3_hwdev *hwdev, struct nic_port_info *port_info);

int hinic3_init_function_table(struct hinic3_hwdev *hwdev, uint16_t rx_buff_len);

/**
 * Alloc RSS template table.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_rss_template_alloc(struct hinic3_hwdev *hwdev);

/**
 * Free RSS template table.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_rss_template_free(struct hinic3_hwdev *hwdev);

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
int hinic3_rss_set_indir_tbl(struct hinic3_hwdev *hwdev, const uint32_t *indir_table,
			     uint32_t indir_table_size);

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
int hinic3_rss_get_indir_tbl(struct hinic3_hwdev *hwdev, uint32_t *indir_table,
			     uint32_t indir_table_size);

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
int hinic3_set_rss_type(struct hinic3_hwdev *hwdev, struct hinic3_rss_type rss_type);

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
int hinic3_get_rss_type(struct hinic3_hwdev *hwdev, struct hinic3_rss_type *rss_type);

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
int hinic3_rss_get_hash_engine(struct hinic3_hwdev *hwdev, uint8_t *type);

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
int hinic3_rss_set_hash_engine(struct hinic3_hwdev *hwdev, uint8_t type);

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
int hinic3_rss_cfg(struct hinic3_hwdev *hwdev, uint8_t rss_en, uint8_t tc_num, uint8_t *prio_tc);

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
int hinic3_rss_set_hash_key(struct hinic3_hwdev *hwdev, uint8_t *key, uint16_t key_size);

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
int hinic3_add_vlan(struct hinic3_hwdev *hwdev, uint16_t vlan_id, uint16_t func_id);

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
int hinic3_del_vlan(struct hinic3_hwdev *hwdev, uint16_t vlan_id, uint16_t func_id);

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
int hinic3_set_vlan_filter(struct hinic3_hwdev *hwdev, uint32_t vlan_filter_ctrl);

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
int hinic3_vf_get_default_cos(struct hinic3_hwdev *hwdev, uint8_t *cos_id);

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
int hinic3_add_tcam_rule(struct hinic3_hwdev *hwdev, struct hinic3_tcam_cfg_rule *tcam_rule,
			 uint8_t tcam_rule_type);

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
int hinic3_del_tcam_rule(struct hinic3_hwdev *hwdev, uint32_t index, uint8_t tcam_rule_type);

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
int hinic3_alloc_tcam_block(struct hinic3_hwdev *hwdev, uint16_t *index);

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
int hinic3_free_tcam_block(struct hinic3_hwdev *hwdev, uint16_t *index);

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
int hinic3_set_fdir_tcam_rule_filter(struct hinic3_hwdev *hwdev, bool enable);

/**
 * Flush fdir tcam rule.
 *
 * @param[in] hwdev
 * Device pointer to hwdev.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int hinic3_flush_tcam_rule(struct hinic3_hwdev *hwdev);

int hinic3_set_rq_flush(struct hinic3_hwdev *hwdev, uint16_t q_id);

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
int hinic3_get_feature_from_hw(struct hinic3_hwdev *hwdev, uint64_t *s_feature, uint16_t size);

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
int hinic3_set_feature_to_hw(struct hinic3_hwdev *hwdev, uint64_t *s_feature, uint16_t size);

int hinic3_set_fdir_ethertype_filter(struct hinic3_hwdev *hwdev,
				     uint8_t pkt_type, uint16_t queue_id, uint8_t en);

int hinic3_set_link_status_follow(struct hinic3_hwdev *hwdev,
				  enum hinic3_link_follow_status status);
#endif /* _HINIC3_NIC_CFG_H_ */
