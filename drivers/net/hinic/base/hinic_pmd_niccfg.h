/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_NICCFG_H_
#define _HINIC_PMD_NICCFG_H_

#define OS_VF_ID_TO_HW(os_vf_id) ((os_vf_id) + 1)
#define HW_VF_ID_TO_OS(hw_vf_id) ((hw_vf_id) - 1)

#define HINIC_VLAN_PRIORITY_SHIFT	13

#define HINIC_RSS_INDIR_SIZE		256
#define HINIC_DCB_TC_MAX		0x8
#define HINIC_DCB_UP_MAX		0x8
#define HINIC_DCB_PG_MAX		0x8
#define HINIC_RSS_KEY_SIZE		40

#define HINIC_MAX_NUM_RQ		64

#define ANTI_ATTACK_DEFAULT_CIR		500000
#define ANTI_ATTACK_DEFAULT_XIR		600000
#define ANTI_ATTACK_DEFAULT_CBS		10000000
#define ANTI_ATTACK_DEFAULT_XBS		12000000

#define NIC_RSS_INDIR_SIZE		256
#define NIC_RSS_KEY_SIZE		40
#define NIC_RSS_CMD_TEMP_ALLOC		0x01
#define NIC_RSS_CMD_TEMP_FREE		0x02
#define NIC_DCB_UP_MAX			0x8

enum hinic_rss_hash_type {
	HINIC_RSS_HASH_ENGINE_TYPE_XOR = 0,
	HINIC_RSS_HASH_ENGINE_TYPE_TOEP,

	HINIC_RSS_HASH_ENGINE_TYPE_MAX,
};

struct nic_port_info {
	u8	port_type;
	u8	autoneg_cap;
	u8	autoneg_state;
	u8	duplex;
	u8	speed;
};

enum nic_speed_level {
	LINK_SPEED_10MB = 0,
	LINK_SPEED_100MB,
	LINK_SPEED_1GB,
	LINK_SPEED_10GB,
	LINK_SPEED_25GB,
	LINK_SPEED_40GB,
	LINK_SPEED_100GB,
	LINK_SPEED_MAX
};

enum hinic_link_status {
	HINIC_LINK_DOWN = 0,
	HINIC_LINK_UP
};

struct hinic_up_ets_cfg {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u8 port_id;
	u8 rsvd1[3];
	u8 up_tc[HINIC_DCB_UP_MAX];
	u8 pg_bw[HINIC_DCB_PG_MAX];
	u8 pgid[HINIC_DCB_UP_MAX];
	u8 up_bw[HINIC_DCB_UP_MAX];
	u8 prio[HINIC_DCB_PG_MAX];
};

struct nic_pause_config {
	u32 auto_neg;
	u32 rx_pause;
	u32 tx_pause;
};

struct nic_rss_type {
	u8 tcp_ipv6_ext;
	u8 ipv6_ext;
	u8 tcp_ipv6;
	u8 ipv6;
	u8 tcp_ipv4;
	u8 ipv4;
	u8 udp_ipv6;
	u8 udp_ipv4;
};

enum hinic_rx_mod {
	HINIC_RX_MODE_UC = 1 << 0,
	HINIC_RX_MODE_MC = 1 << 1,
	HINIC_RX_MODE_BC = 1 << 2,
	HINIC_RX_MODE_MC_ALL = 1 << 3,
	HINIC_RX_MODE_PROMISC = 1 << 4,
};

enum hinic_link_mode {
	HINIC_10GE_BASE_KR = 0,
	HINIC_40GE_BASE_KR4 = 1,
	HINIC_40GE_BASE_CR4 = 2,
	HINIC_100GE_BASE_KR4 = 3,
	HINIC_100GE_BASE_CR4 = 4,
	HINIC_25GE_BASE_KR_S = 5,
	HINIC_25GE_BASE_CR_S = 6,
	HINIC_25GE_BASE_KR = 7,
	HINIC_25GE_BASE_CR = 8,
	HINIC_GE_BASE_KX = 9,
	HINIC_LINK_MODE_NUMBERS,

	HINIC_SUPPORTED_UNKNOWN = 0xFFFF,
};

#define HINIC_DEFAULT_RX_MODE	(HINIC_RX_MODE_UC | HINIC_RX_MODE_MC |	\
				HINIC_RX_MODE_BC)

#define HINIC_MAX_MTU_SIZE		(9600)
#define HINIC_MIN_MTU_SIZE		(256)

/* MIN_MTU + ETH_HLEN + CRC (256+14+4) */
#define HINIC_MIN_FRAME_SIZE		274

/* MAX_MTU + ETH_HLEN + CRC + VLAN(9600+14+4+4) */
#define HINIC_MAX_JUMBO_FRAME_SIZE	(9622)

#define HINIC_PORT_DISABLE		0x0
#define HINIC_PORT_ENABLE		0x3

struct hinic_vport_stats {
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
	u64 rx_err_vport; /* rx checksum err pkts in ucode */
};

struct hinic_phy_port_stats {
	u64 mac_rx_total_pkt_num;
	u64 mac_rx_total_oct_num;
	u64 mac_rx_bad_pkt_num;
	u64 mac_rx_bad_oct_num;
	u64 mac_rx_good_pkt_num;
	u64 mac_rx_good_oct_num;
	u64 mac_rx_uni_pkt_num;
	u64 mac_rx_multi_pkt_num;
	u64 mac_rx_broad_pkt_num;

	u64 mac_tx_total_pkt_num;
	u64 mac_tx_total_oct_num;
	u64 mac_tx_bad_pkt_num;
	u64 mac_tx_bad_oct_num;
	u64 mac_tx_good_pkt_num;
	u64 mac_tx_good_oct_num;
	u64 mac_tx_uni_pkt_num;
	u64 mac_tx_multi_pkt_num;
	u64 mac_tx_broad_pkt_num;

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

	u64 mac_rx_mac_pause_num;
	u64 mac_rx_pfc_pkt_num;
	u64 mac_rx_pfc_pri0_pkt_num;
	u64 mac_rx_pfc_pri1_pkt_num;
	u64 mac_rx_pfc_pri2_pkt_num;
	u64 mac_rx_pfc_pri3_pkt_num;
	u64 mac_rx_pfc_pri4_pkt_num;
	u64 mac_rx_pfc_pri5_pkt_num;
	u64 mac_rx_pfc_pri6_pkt_num;
	u64 mac_rx_pfc_pri7_pkt_num;
	u64 mac_rx_mac_control_pkt_num;
	u64 mac_rx_y1731_pkt_num;
	u64 mac_rx_sym_err_pkt_num;
	u64 mac_rx_fcs_err_pkt_num;
	u64 mac_rx_send_app_good_pkt_num;
	u64 mac_rx_send_app_bad_pkt_num;

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
	u64 mac_trans_jabber_pkt_num;

	u64 mac_tx_mac_pause_num;
	u64 mac_tx_pfc_pkt_num;
	u64 mac_tx_pfc_pri0_pkt_num;
	u64 mac_tx_pfc_pri1_pkt_num;
	u64 mac_tx_pfc_pri2_pkt_num;
	u64 mac_tx_pfc_pri3_pkt_num;
	u64 mac_tx_pfc_pri4_pkt_num;
	u64 mac_tx_pfc_pri5_pkt_num;
	u64 mac_tx_pfc_pri6_pkt_num;
	u64 mac_tx_pfc_pri7_pkt_num;
	u64 mac_tx_mac_control_pkt_num;
	u64 mac_tx_y1731_pkt_num;
	u64 mac_tx_1588_pkt_num;
	u64 mac_tx_err_all_pkt_num;
	u64 mac_tx_from_app_good_pkt_num;
	u64 mac_tx_from_app_bad_pkt_num;

	u64 rx_higig2_ext_pkts_port;
	u64 rx_higig2_message_pkts_port;
	u64 rx_higig2_error_pkts_port;
	u64 rx_higig2_cpu_ctrl_pkts_port;
	u64 rx_higig2_unicast_pkts_port;
	u64 rx_higig2_broadcast_pkts_port;
	u64 rx_higig2_l2_multicast_pkts;
	u64 rx_higig2_l3_multicast_pkts;

	u64 tx_higig2_message_pkts_port;
	u64 tx_higig2_ext_pkts_port;
	u64 tx_higig2_cpu_ctrl_pkts_port;
	u64 tx_higig2_unicast_pkts_port;
	u64 tx_higig2_broadcast_pkts_port;
	u64 tx_higig2_l2_multicast_pkts;
	u64 tx_higig2_l3_multicast_pkts;
};

enum hinic_link_follow_status {
	HINIC_LINK_FOLLOW_DEFAULT,
	HINIC_LINK_FOLLOW_PORT,
	HINIC_LINK_FOLLOW_SEPARATE,
	HINIC_LINK_FOLLOW_STATUS_MAX,
};

#define HINIC_PORT_STATS_VERSION	0
struct hinic_port_stats_info {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16 func_id;
	u16 rsvd1;
	u32 stats_version;
	u32 stats_size;
};

struct hinic_port_stats {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	struct hinic_phy_port_stats stats;
};

struct hinic_cmd_vport_stats {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	struct hinic_vport_stats stats;
};

struct hinic_clear_port_stats {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16 func_id;
	u16 rsvd;
	u32  stats_version;
	u32  stats_size;
};

struct hinic_clear_vport_stats {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16 func_id;
	u16 rsvd;
	u32  stats_version;
	u32  stats_size;
};

struct hinic_fast_recycled_mode {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16 func_id;
	/*
	 * 1: enable fast recycle, available in dpdk mode,
	 * 0: normal mode, available in kernel nic mode
	 */
	u8 fast_recycled_mode;
	u8 rsvd1;
};

struct hinic_function_table {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rx_wqe_buf_size;
	u32	mtu;
};

struct hinic_cmd_qpn {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	base_qpn;
};

struct hinic_port_mac_set {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	vlan_id;
	u16	rsvd1;
	u8	mac[ETH_ALEN];
};

struct hinic_port_mac_update {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	vlan_id;
	u16	rsvd1;
	u8	old_mac[ETH_ALEN];
	u16	rsvd2;
	u8	new_mac[ETH_ALEN];
};

struct hinic_vport_state {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd1;
	u8	state;
	u8	rsvd2[3];
};

struct hinic_port_state {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u8	state;
	u8	rsvd1[3];
};

struct hinic_mtu {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd1;
	u32	mtu;
};

struct hinic_get_link {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u8	link_status;
	u8	rsvd1;
};

#define HINIC_DEFAUT_PAUSE_CONFIG 1
struct hinic_pause_config {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd1;
	u32	auto_neg;
	u32	rx_pause;
	u32	tx_pause;
};

struct hinic_port_info {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd1;
	u8	port_type;
	u8	autoneg_cap;
	u8	autoneg_state;
	u8	duplex;
	u8	speed;
	u8	resv2[3];
};

struct hinic_tso_config {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd1;
	u8	tso_en;
	u8	resv2[3];
};

struct hinic_lro_config {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd1;
	u8	lro_ipv4_en;
	u8	lro_ipv6_en;
	u8	lro_max_wqe_num;
	u8	resv2[13];
};

struct hinic_checksum_offload {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd1;
	u32	rx_csum_offload;
};

struct hinic_rx_mode_config {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd1;
	u32	rx_mode;
};

/* rss */
struct nic_rss_indirect_tbl {
	u32 group_index;
	u32 offset;
	u32 size;
	u32 rsvd;
	u8 entry[NIC_RSS_INDIR_SIZE];
};

struct nic_rss_context_tbl {
	u32 group_index;
	u32 offset;
	u32 size;
	u32 rsvd;
	u32 ctx;
};

struct hinic_rss_config {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u8	rss_en;
	u8	template_id;
	u8	rq_priority_number;
	u8	rsvd1[3];
	u8	prio_tc[NIC_DCB_UP_MAX];
};

struct hinic_rss_template_mgmt {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u8	cmd;
	u8	template_id;
	u8	rsvd1[4];
};

struct hinic_rss_indir_table {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u8	template_id;
	u8	rsvd1;
	u8	indir[NIC_RSS_INDIR_SIZE];
};

struct hinic_rss_template_key {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u8	template_id;
	u8	rsvd1;
	u8	key[NIC_RSS_KEY_SIZE];
};

struct hinic_rss_engine_type {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u8	template_id;
	u8	hash_engine;
	u8	rsvd1[4];
};

struct hinic_rss_context_table {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u8	template_id;
	u8	rsvd1;
	u32	context;
};

struct hinic_reset_link_cfg {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd1;
};

struct hinic_set_vhd_mode {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16 func_id;
	u16 vhd_type;
	u16 rx_wqe_buffer_size;
	u16 rsvd;
};

struct hinic_set_link_follow {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd0;
	u8	follow_status;
	u8	rsvd1[3];
};

struct hinic_link_mode_cmd {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd1;
	u16	supported;	/* 0xFFFF represent Invalid value */
	u16	advertised;
};

struct hinic_clear_qp_resource {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	rsvd1;
};

/* set physical port Anti-Attack rate */
struct hinic_port_anti_attack_rate {
	struct hinic_mgmt_msg_head mgmt_msg_head;

	u16	func_id;
	u16	enable; /* 1: enable rate-limiting, 0: disable rate-limiting */
	u32	cir;	/* Committed Information Rate */
	u32	xir;	/* eXtended Information Rate */
	u32	cbs;	/* Committed Burst Size */
	u32	xbs;	/* eXtended Burst Size */
};

int hinic_set_mac(void *hwdev, u8 *mac_addr, u16 vlan_id, u16 func_id);

int hinic_del_mac(void *hwdev, u8 *mac_addr, u16 vlan_id, u16 func_id);

int hinic_update_mac(void *hwdev, u8 *old_mac, u8 *new_mac, u16 vlan_id,
		     u16 func_id);

int hinic_get_default_mac(void *hwdev, u8 *mac_addr);

int hinic_set_port_mtu(void *hwdev, u32 new_mtu);

int hinic_set_vport_enable(void *hwdev, bool enable);

int hinic_set_port_enable(void *hwdev, bool enable);

int hinic_get_link_status(void *hwdev, u8 *link_state);

int hinic_get_port_info(void *hwdev, struct nic_port_info *port_info);

int hinic_set_rx_vhd_mode(void *hwdev, u16 vhd_mode, u16 rx_buf_sz);

int hinic_set_pause_config(void *hwdev, struct nic_pause_config nic_pause);

int hinic_reset_port_link_cfg(void *hwdev);

int hinic_dcb_set_ets(void *hwdev, u8 *up_tc, u8 *pg_bw, u8 *pgid, u8 *up_bw,
		      u8 *prio);

int hinic_set_anti_attack(void *hwdev, bool enable);

/* offload feature */
int hinic_set_rx_lro(void *hwdev, u8 ipv4_en, u8 ipv6_en, u8 max_wqe_num);

int hinic_get_vport_stats(void *hwdev, struct hinic_vport_stats *stats);

int hinic_get_phy_port_stats(void *hwdev, struct hinic_phy_port_stats *stats);

/* rss */
int hinic_set_rss_type(void *hwdev, u32 tmpl_idx,
		       struct nic_rss_type rss_type);

int hinic_get_rss_type(void *hwdev, u32 tmpl_idx,
		       struct nic_rss_type *rss_type);

int hinic_rss_set_template_tbl(void *hwdev, u32 tmpl_idx, u8 *temp);

int hinic_rss_get_template_tbl(void *hwdev, u32 tmpl_idx, u8 *temp);

int hinic_rss_set_hash_engine(void *hwdev, u8 tmpl_idx, u8 type);

int hinic_rss_get_indir_tbl(void *hwdev, u32 tmpl_idx, u32 *indir_table);

int hinic_rss_set_indir_tbl(void *hwdev, u32 tmpl_idx, u32 *indir_table);

int hinic_rss_cfg(void *hwdev, u8 rss_en, u8 tmpl_idx, u8 tc_num, u8 *prio_tc);

int hinic_rss_template_alloc(void *hwdev, u8 *tmpl_idx);

int hinic_rss_template_free(void *hwdev, u8 tmpl_idx);

int hinic_set_rx_mode(void *hwdev, u32 enable);

int hinic_set_rx_csum_offload(void *hwdev, u32 en);

int hinic_set_link_status_follow(void *hwdev,
				 enum hinic_link_follow_status status);

int hinic_get_link_mode(void *hwdev, u32 *supported, u32 *advertised);

int hinic_flush_qp_res(void *hwdev);

int hinic_init_function_table(void *hwdev, u16 rx_buf_sz);

int hinic_set_fast_recycle_mode(void *hwdev, u8 mode);

int hinic_get_base_qpn(void *hwdev, u16 *global_qpn);

int hinic_clear_vport_stats(struct hinic_hwdev *hwdev);

int hinic_clear_phy_port_stats(struct hinic_hwdev *hwdev);

#endif /* _HINIC_PMD_NICCFG_H_ */
