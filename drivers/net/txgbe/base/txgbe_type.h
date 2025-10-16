/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _TXGBE_TYPE_H_
#define _TXGBE_TYPE_H_

#define TXGBE_DCB_TC_MAX	TXGBE_MAX_UP
#define TXGBE_DCB_UP_MAX	TXGBE_MAX_UP
#define TXGBE_DCB_BWG_MAX	TXGBE_MAX_UP
#define TXGBE_LINK_UP_TIME	90 /* 9.0 Seconds */
#define TXGBE_AUTO_NEG_TIME	45 /* 4.5 Seconds */

#define TXGBE_RX_HDR_SIZE	256
#define TXGBE_RX_BUF_SIZE	2048

#define TXGBE_FRAME_SIZE_MAX	(9728) /* Maximum frame size, +FCS */
#define TXGBE_FRAME_SIZE_DFT	(1518) /* Default frame size, +FCS */
#define TXGBE_NUM_POOL		(64)
#define TXGBE_PBTXSIZE_MAX	0x00028000 /* 160KB Packet Buffer */
#define TXGBE_MAX_FTQF_FILTERS	128
#define TXGBE_TXPKT_SIZE_MAX	0xA /* Max Tx Packet size */
#define TXGBE_MAX_UP		8
#define TXGBE_MAX_QP		(128)
#define TXGBE_MAX_UTA		128

#define TXGBE_FDIR_INIT_DONE_POLL		10
#define TXGBE_FDIRCMD_CMD_POLL			10
#define TXGBE_VF_INIT_TIMEOUT	200 /* Number of retries to clear RSTI */
#define TXGBE_SPI_TIMEOUT	10000
#define TXGBE_PCI_MASTER_DISABLE_TIMEOUT	800

#define TXGBE_ALIGN		128 /* as intel did */

#include "txgbe_status.h"
#include "txgbe_osdep.h"
#include "txgbe_devids.h"

struct txgbe_thermal_diode_data {
	s16 temp;
	s16 alarm_thresh;
	s16 dalarm_thresh;
};

struct txgbe_thermal_sensor_data {
	struct txgbe_thermal_diode_data sensor[1];
};

/* Packet buffer allocation strategies */
enum {
	PBA_STRATEGY_EQUAL	= 0, /* Distribute PB space equally */
#define PBA_STRATEGY_EQUAL	PBA_STRATEGY_EQUAL
	PBA_STRATEGY_WEIGHTED	= 1, /* Weight front half of TCs */
#define PBA_STRATEGY_WEIGHTED	PBA_STRATEGY_WEIGHTED
};

/* Physical layer type */
#define TXGBE_PHYSICAL_LAYER_UNKNOWN		0
#define TXGBE_PHYSICAL_LAYER_10GBASE_T		0x00001
#define TXGBE_PHYSICAL_LAYER_1000BASE_T		0x00002
#define TXGBE_PHYSICAL_LAYER_100BASE_TX		0x00004
#define TXGBE_PHYSICAL_LAYER_SFP_PLUS_CU	0x00008
#define TXGBE_PHYSICAL_LAYER_10GBASE_LR		0x00010
#define TXGBE_PHYSICAL_LAYER_10GBASE_LRM	0x00020
#define TXGBE_PHYSICAL_LAYER_10GBASE_SR		0x00040
#define TXGBE_PHYSICAL_LAYER_10GBASE_KX4	0x00080
#define TXGBE_PHYSICAL_LAYER_10GBASE_CX4	0x00100
#define TXGBE_PHYSICAL_LAYER_1000BASE_KX	0x00200
#define TXGBE_PHYSICAL_LAYER_1000BASE_BX	0x00400
#define TXGBE_PHYSICAL_LAYER_10GBASE_KR		0x00800
#define TXGBE_PHYSICAL_LAYER_10GBASE_XAUI	0x01000
#define TXGBE_PHYSICAL_LAYER_SFP_ACTIVE_DA	0x02000
#define TXGBE_PHYSICAL_LAYER_1000BASE_SX	0x04000
#define TXGBE_PHYSICAL_LAYER_10BASE_T		0x08000
#define TXGBE_PHYSICAL_LAYER_2500BASE_KX	0x10000

/* Software ATR hash keys */
#define TXGBE_ATR_BUCKET_HASH_KEY		0x3DAD14E2
#define TXGBE_ATR_SIGNATURE_HASH_KEY		0x174D3614

/* Software ATR input stream values and masks */
#define TXGBE_ATR_HASH_MASK			0x7fff
#define TXGBE_ATR_L3TYPE_MASK			0x4
#define TXGBE_ATR_L3TYPE_IPV4			0x0
#define TXGBE_ATR_L3TYPE_IPV6			0x4
#define TXGBE_ATR_L4TYPE_MASK			0x3
#define TXGBE_ATR_L4TYPE_UDP			0x1
#define TXGBE_ATR_L4TYPE_TCP			0x2
#define TXGBE_ATR_L4TYPE_SCTP			0x3
#define TXGBE_ATR_TYPE_MASK_TUN			0x80
#define TXGBE_ATR_TYPE_MASK_TUN_OUTIP		0x40
#define TXGBE_ATR_TYPE_MASK_TUN_TYPE		0x20
#define TXGBE_ATR_TYPE_MASK_L3P			0x10
#define TXGBE_ATR_TYPE_MASK_L4P			0x08
enum txgbe_atr_flow_type {
	TXGBE_ATR_FLOW_TYPE_IPV4		= 0x0,
	TXGBE_ATR_FLOW_TYPE_UDPV4		= 0x1,
	TXGBE_ATR_FLOW_TYPE_TCPV4		= 0x2,
	TXGBE_ATR_FLOW_TYPE_SCTPV4		= 0x3,
	TXGBE_ATR_FLOW_TYPE_IPV6		= 0x4,
	TXGBE_ATR_FLOW_TYPE_UDPV6		= 0x5,
	TXGBE_ATR_FLOW_TYPE_TCPV6		= 0x6,
	TXGBE_ATR_FLOW_TYPE_SCTPV6		= 0x7,
};

/* Flow Director ATR input struct. */
struct txgbe_atr_input {
	/*
	 * Byte layout in order, all values with MSB first:
	 *
	 * vm_pool	- 1 byte
	 * flow_type	- 1 byte
	 * pkt_type	- 2 bytes
	 * src_ip	- 16 bytes
	 * dst_ip	- 16 bytes
	 * src_port	- 2 bytes
	 * dst_port	- 2 bytes
	 * flex_bytes	- 2 bytes
	 * bkt_hash	- 2 bytes
	 */
	u8 vm_pool;
	u8 flow_type;
	__be16 pkt_type;
	__be32 dst_ip[4];
	__be32 src_ip[4];
	__be16 src_port;
	__be16 dst_port;
	__be16 flex_bytes;
	__be16 bkt_hash;
};

enum txgbe_eeprom_type {
	txgbe_eeprom_unknown = 0,
	txgbe_eeprom_spi,
	txgbe_eeprom_flash,
	txgbe_eeprom_none /* No NVM support */
};

enum txgbe_mac_type {
	txgbe_mac_unknown = 0,
	txgbe_mac_sp,
	txgbe_mac_aml,
	txgbe_mac_aml40,
	txgbe_mac_sp_vf,
	txgbe_mac_aml_vf,
	txgbe_num_macs
};

enum txgbe_phy_type {
	txgbe_phy_unknown = 0,
	txgbe_phy_none,
	txgbe_phy_tn,
	txgbe_phy_aq,
	txgbe_phy_ext_1g_t,
	txgbe_phy_cu_mtd,
	txgbe_phy_cu_unknown,
	txgbe_phy_qt,
	txgbe_phy_xaui,
	txgbe_phy_nl,
	txgbe_phy_sfp_tyco_passive,
	txgbe_phy_sfp_unknown_passive,
	txgbe_phy_sfp_unknown_active,
	txgbe_phy_sfp_avago,
	txgbe_phy_sfp_ftl,
	txgbe_phy_sfp_ftl_active,
	txgbe_phy_sfp_unknown,
	txgbe_phy_sfp_intel,
	txgbe_phy_qsfp_unknown_passive,
	txgbe_phy_qsfp_unknown_active,
	txgbe_phy_qsfp_intel,
	txgbe_phy_qsfp_unknown,
	txgbe_phy_sfp_unsupported, /* Enforce bit set with unsupported module */
	txgbe_phy_sgmii,
	txgbe_phy_fw,
	txgbe_phy_generic
};

/*
 * SFP+ module type IDs:
 *
 * ID	Module Type
 * =============
 * 0	SFP_DA_CU
 * 1	SFP_SR
 * 2	SFP_LR
 * 3	SFP_DA_CU_CORE0 - chip-specific
 * 4	SFP_DA_CU_CORE1 - chip-specific
 * 5	SFP_SR/LR_CORE0 - chip-specific
 * 6	SFP_SR/LR_CORE1 - chip-specific
 */
enum txgbe_sfp_type {
	txgbe_sfp_type_unknown = 0,
	txgbe_sfp_type_da_cu,
	txgbe_sfp_type_sr,
	txgbe_sfp_type_lr,
	txgbe_sfp_type_da_cu_core0,
	txgbe_sfp_type_da_cu_core1,
	txgbe_sfp_type_srlr_core0,
	txgbe_sfp_type_srlr_core1,
	txgbe_sfp_type_da_act_lmt_core0,
	txgbe_sfp_type_da_act_lmt_core1,
	txgbe_sfp_type_1g_cu_core0,
	txgbe_sfp_type_1g_cu_core1,
	txgbe_sfp_type_1g_sx_core0,
	txgbe_sfp_type_1g_sx_core1,
	txgbe_sfp_type_1g_lx_core0,
	txgbe_sfp_type_1g_lx_core1,
	txgbe_sfp_type_25g_sr_core0,
	txgbe_sfp_type_25g_sr_core1,
	txgbe_sfp_type_25g_lr_core0,
	txgbe_sfp_type_25g_lr_core1,
	txgbe_sfp_type_25g_aoc_core0,
	txgbe_sfp_type_25g_aoc_core1,
	txgbe_qsfp_type_40g_cu_core0,
	txgbe_qsfp_type_40g_cu_core1,
	txgbe_qsfp_type_40g_sr_core0,
	txgbe_qsfp_type_40g_sr_core1,
	txgbe_qsfp_type_40g_lr_core0,
	txgbe_qsfp_type_40g_lr_core1,
	txgbe_sfp_type_not_present = 0xFFFE,
	txgbe_sfp_type_not_known = 0xFFFF
};

enum txgbe_media_type {
	txgbe_media_type_unknown = 0,
	txgbe_media_type_fiber,
	txgbe_media_type_fiber_qsfp,
	txgbe_media_type_copper,
	txgbe_media_type_backplane,
	txgbe_media_type_cx4,
	txgbe_media_type_virtual
};

/* Flow Control Settings */
enum txgbe_fc_mode {
	txgbe_fc_none = 0,
	txgbe_fc_rx_pause,
	txgbe_fc_tx_pause,
	txgbe_fc_full,
	txgbe_fc_default
};

/* Smart Speed Settings */
#define TXGBE_SMARTSPEED_MAX_RETRIES	3
enum txgbe_smart_speed {
	txgbe_smart_speed_auto = 0,
	txgbe_smart_speed_on,
	txgbe_smart_speed_off
};

/* PCI bus types */
enum txgbe_bus_type {
	txgbe_bus_type_unknown = 0,
	txgbe_bus_type_pci,
	txgbe_bus_type_pcix,
	txgbe_bus_type_pci_express,
	txgbe_bus_type_internal,
	txgbe_bus_type_reserved
};

/* PCI bus speeds */
enum txgbe_bus_speed {
	txgbe_bus_speed_unknown	= 0,
	txgbe_bus_speed_33	= 33,
	txgbe_bus_speed_66	= 66,
	txgbe_bus_speed_100	= 100,
	txgbe_bus_speed_120	= 120,
	txgbe_bus_speed_133	= 133,
	txgbe_bus_speed_2500	= 2500,
	txgbe_bus_speed_5000	= 5000,
	txgbe_bus_speed_8000	= 8000,
	txgbe_bus_speed_reserved
};

/* PCI bus widths */
enum txgbe_bus_width {
	txgbe_bus_width_unknown	= 0,
	txgbe_bus_width_pcie_x1	= 1,
	txgbe_bus_width_pcie_x2	= 2,
	txgbe_bus_width_pcie_x4	= 4,
	txgbe_bus_width_pcie_x8	= 8,
	txgbe_bus_width_32	= 32,
	txgbe_bus_width_64	= 64,
	txgbe_bus_width_reserved
};

struct txgbe_hw;

struct txgbe_addr_filter_info {
	u32 num_mc_addrs;
	u32 rar_used_count;
	u32 mta_in_use;
	u32 overflow_promisc;
	bool user_set_promisc;
};

/* Bus parameters */
struct txgbe_bus_info {
	s32 (*get_bus_info)(struct txgbe_hw *hw);
	void (*set_lan_id)(struct txgbe_hw *hw);

	enum txgbe_bus_speed speed;
	enum txgbe_bus_width width;
	enum txgbe_bus_type type;

	u16 func;
	u8 lan_id;
	u16 instance_id;
};

/* Flow control parameters */
struct txgbe_fc_info {
	u32 high_water[TXGBE_DCB_TC_MAX]; /* Flow Ctrl High-water */
	u32 low_water[TXGBE_DCB_TC_MAX]; /* Flow Ctrl Low-water */
	u16 pause_time; /* Flow Control Pause timer */
	u8 mac_ctrl_frame_fwd; /* Forward MAC control frames */
	bool send_xon; /* Flow control send XON */
	bool strict_ieee; /* Strict IEEE mode */
	bool disable_fc_autoneg; /* Do not autonegotiate FC */
	bool fc_was_autonegged; /* Is current_mode the result of autonegging? */
	enum txgbe_fc_mode current_mode; /* FC mode in effect */
	enum txgbe_fc_mode requested_mode; /* FC mode requested by caller */
};

/* Statistics counters collected by the MAC */
/* PB[] RxTx */
struct txgbe_pb_stats {
	u64 tx_pb_xon_packets;
	u64 rx_pb_xon_packets;
	u64 tx_pb_xoff_packets;
	u64 rx_pb_xoff_packets;
	u64 rx_pb_dropped;
	u64 rx_pb_mbuf_alloc_errors;
	u64 tx_pb_xon2off_packets;
};

/* QP[] RxTx */
struct txgbe_qp_stats {
	u64 rx_qp_packets;
	u64 tx_qp_packets;
	u64 rx_qp_bytes;
	u64 tx_qp_bytes;
	u64 rx_qp_mc_packets;
};

struct txgbe_hw_stats {
	/* MNG RxTx */
	u64 mng_bmc2host_packets;
	u64 mng_host2bmc_packets;
	/* Basix RxTx */
	u64 rx_packets;
	u64 tx_packets;
	u64 rx_bytes;
	u64 tx_bytes;
	u64 rx_total_bytes;
	u64 rx_total_packets;
	u64 tx_total_packets;
	u64 rx_total_missed_packets;
	u64 rx_broadcast_packets;
	u64 tx_broadcast_packets;
	u64 rx_multicast_packets;
	u64 tx_multicast_packets;
	u64 rx_management_packets;
	u64 tx_management_packets;
	u64 rx_management_dropped;
	u64 rx_dma_drop;

	/* Basic Error */
	u64 rx_rdb_drop;
	u64 rx_crc_errors;
	u64 rx_illegal_byte_errors;
	u64 rx_error_bytes;
	u64 rx_mac_short_packet_dropped;
	u64 rx_length_errors;
	u64 rx_undersize_errors;
	u64 rx_fragment_errors;
	u64 rx_oversize_cnt;
	u64 rx_jabber_errors;
	u64 rx_l3_l4_xsum_error;
	u64 mac_local_errors;
	u64 mac_remote_errors;

	/* Flow Director */
	u64 flow_director_added_filters;
	u64 flow_director_removed_filters;
	u64 flow_director_filter_add_errors;
	u64 flow_director_filter_remove_errors;
	u64 flow_director_matched_filters;
	u64 flow_director_missed_filters;

	/* FCoE */
	u64 rx_fcoe_crc_errors;
	u64 rx_fcoe_mbuf_allocation_errors;
	u64 rx_fcoe_dropped;
	u64 rx_fcoe_packets;
	u64 tx_fcoe_packets;
	u64 rx_fcoe_bytes;
	u64 tx_fcoe_bytes;
	u64 rx_fcoe_no_ddp;
	u64 rx_fcoe_no_ddp_ext_buff;

	/* MACSEC */
	u64 tx_macsec_pkts_untagged;
	u64 tx_macsec_pkts_encrypted;
	u64 tx_macsec_pkts_protected;
	u64 tx_macsec_octets_encrypted;
	u64 tx_macsec_octets_protected;
	u64 rx_macsec_pkts_untagged;
	u64 rx_macsec_pkts_badtag;
	u64 rx_macsec_pkts_nosci;
	u64 rx_macsec_pkts_unknownsci;
	u64 rx_macsec_octets_decrypted;
	u64 rx_macsec_octets_validated;
	u64 rx_macsec_sc_pkts_unchecked;
	u64 rx_macsec_sc_pkts_delayed;
	u64 rx_macsec_sc_pkts_late;
	u64 rx_macsec_sa_pkts_ok;
	u64 rx_macsec_sa_pkts_invalid;
	u64 rx_macsec_sa_pkts_notvalid;
	u64 rx_macsec_sa_pkts_unusedsa;
	u64 rx_macsec_sa_pkts_notusingsa;

	/* MAC RxTx */
	u64 rx_size_64_packets;
	u64 rx_size_65_to_127_packets;
	u64 rx_size_128_to_255_packets;
	u64 rx_size_256_to_511_packets;
	u64 rx_size_512_to_1023_packets;
	u64 rx_size_1024_to_max_packets;
	u64 tx_size_64_packets;
	u64 tx_size_65_to_127_packets;
	u64 tx_size_128_to_255_packets;
	u64 tx_size_256_to_511_packets;
	u64 tx_size_512_to_1023_packets;
	u64 tx_size_1024_to_max_packets;

	/* Flow Control */
	u64 tx_xon_packets;
	u64 rx_xon_packets;
	u64 tx_xoff_packets;
	u64 rx_xoff_packets;

	/* PB[] RxTx */
	struct {
		u64 rx_up_packets;
		u64 tx_up_packets;
		u64 rx_up_bytes;
		u64 tx_up_bytes;
		u64 rx_up_drop_packets;

		u64 tx_up_xon_packets;
		u64 rx_up_xon_packets;
		u64 tx_up_xoff_packets;
		u64 rx_up_xoff_packets;
		u64 rx_up_dropped;
		u64 rx_up_mbuf_alloc_errors;
		u64 tx_up_xon2off_packets;
	} up[TXGBE_MAX_UP];

	/* QP[] RxTx */
	struct {
		u64 rx_qp_packets;
		u64 tx_qp_packets;
		u64 rx_qp_bytes;
		u64 tx_qp_bytes;
		u64 rx_qp_mc_packets;
	} qp[TXGBE_MAX_QP];

};

/* iterator type for walking multicast address lists */
typedef u8* (*txgbe_mc_addr_itr) (struct txgbe_hw *hw, u8 **mc_addr_ptr,
				  u32 *vmdq);

struct txgbe_link_info {
	s32 (*read_link)(struct txgbe_hw *hw, u8 addr, u16 reg, u16 *val);
	s32 (*read_link_unlocked)(struct txgbe_hw *hw, u8 addr, u16 reg,
				  u16 *val);
	s32 (*write_link)(struct txgbe_hw *hw, u8 addr, u16 reg, u16 val);
	s32 (*write_link_unlocked)(struct txgbe_hw *hw, u8 addr, u16 reg,
				   u16 val);

	u8 addr;
};

struct txgbe_rom_info {
	s32 (*init_params)(struct txgbe_hw *hw);
	s32 (*read16)(struct txgbe_hw *hw, u32 offset, u16 *data);
	s32 (*readw_sw)(struct txgbe_hw *hw, u32 offset, u16 *data);
	s32 (*readw_buffer)(struct txgbe_hw *hw, u32 offset, u32 words,
			    void *data);
	s32 (*read32)(struct txgbe_hw *hw, u32 addr, u32 *data);
	s32 (*read_buffer)(struct txgbe_hw *hw, u32 addr, u32 len, void *data);
	s32 (*write16)(struct txgbe_hw *hw, u32 offset, u16 data);
	s32 (*writew_sw)(struct txgbe_hw *hw, u32 offset, u16 data);
	s32 (*writew_buffer)(struct txgbe_hw *hw, u32 offset, u32 words,
			     void *data);
	s32 (*write32)(struct txgbe_hw *hw, u32 addr, u32 data);
	s32 (*write_buffer)(struct txgbe_hw *hw, u32 addr, u32 len, void *data);
	s32 (*validate_checksum)(struct txgbe_hw *hw, u16 *checksum_val);
	s32 (*update_checksum)(struct txgbe_hw *hw);
	s32 (*calc_checksum)(struct txgbe_hw *hw);

	enum txgbe_eeprom_type type;
	u32 semaphore_delay;
	u16 word_size;
	u16 address_bits;
	u16 word_page_size;
	u16 ctrl_word_3;

	u32 sw_addr;
};

struct txgbe_flash_info {
	u32 semaphore_delay;
	u32 dword_size;
	u16 address_bits;
};

#define TXGBE_FLAGS_DOUBLE_RESET_REQUIRED	0x01
struct txgbe_mac_info {
	s32 (*init_hw)(struct txgbe_hw *hw);
	s32 (*reset_hw)(struct txgbe_hw *hw);
	s32 (*start_hw)(struct txgbe_hw *hw);
	s32 (*stop_hw)(struct txgbe_hw *hw);
	s32 (*clear_hw_cntrs)(struct txgbe_hw *hw);
	s32 (*get_mac_addr)(struct txgbe_hw *hw, u8 *mac_addr);
	s32 (*get_san_mac_addr)(struct txgbe_hw *hw, u8 *san_mac_addr);
	s32 (*set_san_mac_addr)(struct txgbe_hw *hw, u8 *san_mac_addr);
	s32 (*get_device_caps)(struct txgbe_hw *hw, u16 *device_caps);
	s32 (*get_wwn_prefix)(struct txgbe_hw *hw, u16 *wwnn_prefix,
				 u16 *wwpn_prefix);
	s32 (*setup_sfp)(struct txgbe_hw *hw);
	s32 (*enable_rx_dma)(struct txgbe_hw *hw, u32 regval);
	s32 (*disable_sec_rx_path)(struct txgbe_hw *hw);
	s32 (*enable_sec_rx_path)(struct txgbe_hw *hw);
	s32 (*disable_sec_tx_path)(struct txgbe_hw *hw);
	s32 (*enable_sec_tx_path)(struct txgbe_hw *hw);
	s32 (*acquire_swfw_sync)(struct txgbe_hw *hw, u32 mask);
	void (*release_swfw_sync)(struct txgbe_hw *hw, u32 mask);
	u64 (*autoc_read)(struct txgbe_hw *hw);
	void (*autoc_write)(struct txgbe_hw *hw, u64 value);
	s32 (*prot_autoc_read)(struct txgbe_hw *hw, bool *locked, u64 *value);
	s32 (*prot_autoc_write)(struct txgbe_hw *hw, bool locked, u64 value);
	s32 (*negotiate_api_version)(struct txgbe_hw *hw, int api);
	void (*init_mac_link_ops)(struct txgbe_hw *hw);

	/* Link */
	void (*disable_tx_laser)(struct txgbe_hw *hw);
	void (*enable_tx_laser)(struct txgbe_hw *hw);
	void (*flap_tx_laser)(struct txgbe_hw *hw);
	s32 (*setup_link)(struct txgbe_hw *hw, u32 speed,
			       bool autoneg_wait_to_complete);
	s32 (*setup_mac_link)(struct txgbe_hw *hw, u32 speed,
			       bool autoneg_wait_to_complete);
	s32 (*check_link)(struct txgbe_hw *hw, u32 *speed,
			       bool *link_up, bool link_up_wait_to_complete);
	s32 (*get_link_capabilities)(struct txgbe_hw *hw,
				      u32 *speed, bool *autoneg);
	void (*set_rate_select_speed)(struct txgbe_hw *hw, u32 speed);

	/* Packet Buffer manipulation */
	void (*setup_pba)(struct txgbe_hw *hw, int num_pb, u32 headroom,
			     int strategy);

	/* LED */
	s32 (*led_on)(struct txgbe_hw *hw, u32 index);
	s32 (*led_off)(struct txgbe_hw *hw, u32 index);

	/* RAR, Multicast, VLAN */
	s32 (*set_rar)(struct txgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
			  u32 enable_addr);
	s32 (*set_uc_addr)(struct txgbe_hw *hw, u32 index, u8 *addr);
	s32 (*clear_rar)(struct txgbe_hw *hw, u32 index);
	s32 (*set_vmdq)(struct txgbe_hw *hw, u32 rar, u32 vmdq);
	s32 (*clear_vmdq)(struct txgbe_hw *hw, u32 rar, u32 vmdq);
	s32 (*init_rx_addrs)(struct txgbe_hw *hw);
	s32 (*update_mc_addr_list)(struct txgbe_hw *hw, u8 *mc_addr_list,
				      u32 mc_addr_count,
				      txgbe_mc_addr_itr func, bool clear);
	s32 (*clear_vfta)(struct txgbe_hw *hw);
	s32 (*set_vfta)(struct txgbe_hw *hw, u32 vlan,
			 u32 vind, bool vlan_on, bool vlvf_bypass);
	s32 (*set_vlvf)(struct txgbe_hw *hw, u32 vlan, u32 vind,
			   bool vlan_on, u32 *vfta_delta, u32 vfta,
			   bool vlvf_bypass);
	s32 (*init_uta_tables)(struct txgbe_hw *hw);
	void (*set_mac_anti_spoofing)(struct txgbe_hw *hw, bool enable, int vf);
	void (*set_vlan_anti_spoofing)(struct txgbe_hw *hw,
					bool enable, int vf);
	s32 (*update_xcast_mode)(struct txgbe_hw *hw, int xcast_mode);
	s32 (*set_rlpml)(struct txgbe_hw *hw, u16 max_size);

	/* Flow Control */
	s32 (*fc_enable)(struct txgbe_hw *hw);
	s32 (*setup_fc)(struct txgbe_hw *hw);
	void (*fc_autoneg)(struct txgbe_hw *hw);

	/* Manageability interface */
	s32 (*set_fw_drv_ver)(struct txgbe_hw *hw, u8 maj, u8 min, u8 build,
			 u8 ver, u16 len, const char *driver_ver);
	s32 (*get_thermal_sensor_data)(struct txgbe_hw *hw);
	s32 (*init_thermal_sensor_thresh)(struct txgbe_hw *hw);
	void (*get_rtrup2tc)(struct txgbe_hw *hw, u8 *map);
	void (*disable_rx)(struct txgbe_hw *hw);
	void (*enable_rx)(struct txgbe_hw *hw);
	void (*set_ethertype_anti_spoofing)(struct txgbe_hw *hw,
						bool enable, int vf);
	s32 (*dmac_update_tcs)(struct txgbe_hw *hw);
	s32 (*dmac_config_tcs)(struct txgbe_hw *hw);
	s32 (*dmac_config)(struct txgbe_hw *hw);
	s32 (*setup_eee)(struct txgbe_hw *hw, bool enable_eee);

	s32 (*kr_handle)(struct txgbe_hw *hw);
	void (*bp_down_event)(struct txgbe_hw *hw);

	enum txgbe_mac_type type;
	u8 addr[ETH_ADDR_LEN];
	u8 perm_addr[ETH_ADDR_LEN];
	u8 san_addr[ETH_ADDR_LEN];
	/* prefix for World Wide Node Name (WWNN) */
	u16 wwnn_prefix;
	/* prefix for World Wide Port Name (WWPN) */
	u16 wwpn_prefix;
#define TXGBE_MAX_MTA			128
	u32 mta_shadow[TXGBE_MAX_MTA];
	s32 mc_filter_type;
	u32 mcft_size;
	u32 vft_size;
	u32 num_rar_entries;
	u32 rx_pb_size;
	u32 max_tx_queues;
	u32 max_rx_queues;
	u64 autoc;
	u64 orig_autoc;  /* cached value of AUTOC */
	u8  san_mac_rar_index;
	bool get_link_status;
	bool orig_link_settings_stored;
	bool autotry_restart;
	u8 flags;
	struct txgbe_thermal_sensor_data  thermal_sensor_data;
	bool set_lben;
	u32  max_link_up_time;
};

struct txgbe_phy_info {
	u32 (*get_media_type)(struct txgbe_hw *hw);
	s32 (*identify)(struct txgbe_hw *hw);
	s32 (*identify_sfp)(struct txgbe_hw *hw);
	s32 (*init)(struct txgbe_hw *hw);
	s32 (*reset)(struct txgbe_hw *hw);
	s32 (*read_reg)(struct txgbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 *phy_data);
	s32 (*write_reg)(struct txgbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data);
	s32 (*read_reg_mdi)(struct txgbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 *phy_data);
	s32 (*write_reg_mdi)(struct txgbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data);
	s32 (*setup_link)(struct txgbe_hw *hw);
	s32 (*setup_internal_link)(struct txgbe_hw *hw);
	s32 (*setup_link_speed)(struct txgbe_hw *hw, u32 speed,
				bool autoneg_wait_to_complete);
	s32 (*check_link)(struct txgbe_hw *hw, u32 *speed, bool *link_up);
	s32 (*get_fw_version)(struct txgbe_hw *hw, u32 *fw_version);
	s32 (*read_i2c_byte)(struct txgbe_hw *hw, u8 byte_offset,
				u8 dev_addr, u8 *data);
	s32 (*write_i2c_byte)(struct txgbe_hw *hw, u8 byte_offset,
				u8 dev_addr, u8 data);
	s32 (*read_i2c_sff8472)(struct txgbe_hw *hw, u8 byte_offset,
				u8 *sff8472_data);
	s32 (*read_i2c_eeprom)(struct txgbe_hw *hw, u8 byte_offset,
				u8 *eeprom_data);
	s32 (*write_i2c_eeprom)(struct txgbe_hw *hw, u8 byte_offset,
				u8 eeprom_data);
	s32 (*check_overtemp)(struct txgbe_hw *hw);
	s32 (*set_phy_power)(struct txgbe_hw *hw, bool on);
	s32 (*handle_lasi)(struct txgbe_hw *hw);
	s32 (*read_i2c_byte_unlocked)(struct txgbe_hw *hw, u8 offset, u8 addr,
				      u8 *value);
	s32 (*write_i2c_byte_unlocked)(struct txgbe_hw *hw, u8 offset, u8 addr,
				       u8 value);
	s32 (*set_link_hostif)(struct txgbe_hw *hw, u8 speed, u8 autoneg, u8 duplex);

	enum txgbe_phy_type type;
	u32 addr;
	u32 id;
	enum txgbe_sfp_type sfp_type;
	bool sfp_setup_needed;
	u32 revision;
	u32 media_type;
	u32 phy_semaphore_mask;
	bool reset_disable;
	u32 autoneg_advertised;
	u32 speeds_supported;
	enum txgbe_smart_speed smart_speed;
	bool smart_speed_active;
	bool multispeed_fiber;
	bool qsfp_shared_i2c_bus;
	u32 nw_mng_if_sel;
	u32 link_mode;

	/* Some features need tri-state capability */
	u16 ffe_set;
	u16 ffe_main;
	u16 ffe_pre;
	u16 ffe_post;
};

#define TXGBE_DEVARG_BP_AUTO		"auto_neg"
#define TXGBE_DEVARG_KR_POLL		"poll"
#define TXGBE_DEVARG_KR_PRESENT		"present"
#define TXGBE_DEVARG_KX_SGMII		"sgmii"
#define TXGBE_DEVARG_FFE_SET		"ffe_set"
#define TXGBE_DEVARG_FFE_MAIN		"ffe_main"
#define TXGBE_DEVARG_FFE_PRE		"ffe_pre"
#define TXGBE_DEVARG_FFE_POST		"ffe_post"
#define TXGBE_DEVARG_TX_HEAD_WB		"tx_headwb"
#define TXGBE_DEVARG_TX_HEAD_WB_SIZE	"tx_headwb_size"

static const char * const txgbe_valid_arguments[] = {
	TXGBE_DEVARG_BP_AUTO,
	TXGBE_DEVARG_KR_POLL,
	TXGBE_DEVARG_KR_PRESENT,
	TXGBE_DEVARG_KX_SGMII,
	TXGBE_DEVARG_FFE_SET,
	TXGBE_DEVARG_FFE_MAIN,
	TXGBE_DEVARG_FFE_PRE,
	TXGBE_DEVARG_FFE_POST,
	TXGBE_DEVARG_TX_HEAD_WB,
	TXGBE_DEVARG_TX_HEAD_WB_SIZE,
	NULL
};

struct txgbe_mbx_stats {
	u32 msgs_tx;
	u32 msgs_rx;

	u32 acks;
	u32 reqs;
	u32 rsts;
};

struct txgbe_mbx_info {
	void (*init_params)(struct txgbe_hw *hw);
	s32  (*read)(struct txgbe_hw *hw, u32 *msg, u16 size, u16 vf_number);
	s32  (*write)(struct txgbe_hw *hw, u32 *msg, u16 size, u16 vf_number);
	s32  (*read_posted)(struct txgbe_hw *hw, u32 *msg, u16 size,
				u16 mbx_id);
	s32  (*write_posted)(struct txgbe_hw *hw, u32 *msg, u16 size,
				u16 mbx_id);
	s32  (*check_for_msg)(struct txgbe_hw *hw, u16 mbx_id);
	s32  (*check_for_ack)(struct txgbe_hw *hw, u16 mbx_id);
	s32  (*check_for_rst)(struct txgbe_hw *hw, u16 mbx_id);

	struct txgbe_mbx_stats stats;
	u32 timeout;
	u32 usec_delay;
	u32 v2p_mailbox;
	u16 size;
};

enum txgbe_isb_idx {
	TXGBE_ISB_HEADER,
	TXGBE_ISB_MISC,
	TXGBE_ISB_VEC0,
	TXGBE_ISB_VEC1,
	TXGBE_ISB_MAX
};

#define TXGBE_PHY_FEC_RS	MS(0, 0x1)
#define TXGBE_PHY_FEC_BASER	MS(1, 0x1)
#define TXGBE_PHY_FEC_OFF	MS(2, 0x1)
#define TXGBE_PHY_FEC_AUTO	(TXGBE_PHY_FEC_OFF | TXGBE_PHY_FEC_BASER |\
				 TXGBE_PHY_FEC_RS)

struct txgbe_devargs {
	u16 auto_neg;
	u16 poll;
	u16 present;
	u16 sgmii;
	u16 tx_headwb;
	u16 tx_headwb_size;
};

struct txgbe_hw {
	void IOMEM *hw_addr;
	void *back;
	struct txgbe_mac_info mac;
	struct txgbe_addr_filter_info addr_ctrl;
	struct txgbe_fc_info fc;
	struct txgbe_phy_info phy;
	struct txgbe_link_info link;
	struct txgbe_rom_info rom;
	struct txgbe_flash_info flash;
	struct txgbe_bus_info bus;
	struct txgbe_mbx_info mbx;
	u16 device_id;
	u16 vendor_id;
	u16 subsystem_device_id;
	u16 subsystem_vendor_id;
	u8 port_id;
	u8 revision_id;
	bool adapter_stopped;
	int api_version;
	bool allow_unsupported_sfp;
	bool lldp_enabled;
	bool need_crosstalk_fix;
	bool dev_start;
	bool autoneg;
	struct txgbe_devargs devarg;

	uint64_t isb_dma;
	void IOMEM *isb_mem;
	u16 nb_rx_queues;
	u16 nb_tx_queues;

	u32 fw_version;
	u32 mode;
	enum txgbe_link_status {
		TXGBE_LINK_STATUS_NONE = 0,
		TXGBE_LINK_STATUS_KX,
		TXGBE_LINK_STATUS_KX4
	} link_status;
	enum txgbe_reset_type {
		TXGBE_LAN_RESET = 0,
		TXGBE_SW_RESET,
		TXGBE_GLOBAL_RESET
	} reset_type;

	u32 q_rx_regs[128 * 4];
	u32 q_tx_regs[128 * 4];
	bool offset_loaded;
	bool rx_loaded;
	struct {
		u64 rx_qp_packets;
		u64 tx_qp_packets;
		u64 rx_qp_bytes;
		u64 tx_qp_bytes;
		u64 rx_qp_mc_packets;
	} qp_last[TXGBE_MAX_QP];

	rte_spinlock_t phy_lock;
	/*amlite: new SW-FW mbox */
	u8 swfw_index;
	rte_atomic32_t swfw_busy;
};

struct txgbe_backplane_ability {
	u32 next_page;	  /* Next Page (bit0) */
	u32 link_ability; /* Link Ability (bit[7:0]) */
	u32 fec_ability;  /* FEC Request (bit1), FEC Enable (bit0) */
	u32 current_link_mode; /* current link mode for local device */
};

#include "txgbe_regs.h"
#include "txgbe_dummy.h"

#endif /* _TXGBE_TYPE_H_ */
