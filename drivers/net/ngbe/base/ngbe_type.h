/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_TYPE_H_
#define _NGBE_TYPE_H_

#define NGBE_LINK_UP_TIME	90 /* 9.0 Seconds */

#define NGBE_FRAME_SIZE_MAX       (9728) /* Maximum frame size, +FCS */
#define NGBE_FRAME_SIZE_DFT       (1518) /* Default frame size, +FCS */
#define NGBE_NUM_POOL             (32)
#define NGBE_PBRXSIZE_MAX         0x00080000 /* 512KB Packet Buffer */
#define NGBE_PBTXSIZE_MAX         0x00005000 /* 20KB Packet Buffer */
#define NGBE_TXPKT_SIZE_MAX       0xA /* Max Tx Packet size */
#define NGBE_MAX_QP               (8)
#define NGBE_MAX_UTA              128

#define NGBE_PCI_MASTER_DISABLE_TIMEOUT	800
#define NGBE_SPI_TIMEOUT	10000

#define NGBE_ALIGN		128 /* as intel did */
#define NGBE_ISB_SIZE		16

#include "ngbe_status.h"
#include "ngbe_osdep.h"
#include "ngbe_devids.h"

struct ngbe_thermal_diode_data {
	s16 temp;
	s16 alarm_thresh;
	s16 dalarm_thresh;
};

struct ngbe_thermal_sensor_data {
	struct ngbe_thermal_diode_data sensor[1];
};

enum ngbe_eeprom_type {
	ngbe_eeprom_unknown = 0,
	ngbe_eeprom_spi,
	ngbe_eeprom_flash,
	ngbe_eeprom_none /* No NVM support */
};

enum ngbe_link_type {
	ngbe_link_type_unknown = 0,
	ngbe_link_fiber,
	ngbe_link_copper
};

enum ngbe_mac_type {
	ngbe_mac_unknown = 0,
	ngbe_mac_em,
	ngbe_mac_em_vf,
	ngbe_num_macs
};

enum ngbe_phy_type {
	ngbe_phy_unknown = 0,
	ngbe_phy_none,
	ngbe_phy_rtl,
	ngbe_phy_mvl,
	ngbe_phy_mvl_sfi,
	ngbe_phy_yt8521s,
	ngbe_phy_yt8521s_sfi,
	ngbe_phy_zte,
	ngbe_phy_cu_mtd,
};

enum ngbe_media_type {
	ngbe_media_type_unknown = 0,
	ngbe_media_type_fiber,
	ngbe_media_type_fiber_qsfp,
	ngbe_media_type_copper,
	ngbe_media_type_backplane,
	ngbe_media_type_cx4,
	ngbe_media_type_virtual
};

/* Flow Control Settings */
enum ngbe_fc_mode {
	ngbe_fc_none = 0,
	ngbe_fc_rx_pause,
	ngbe_fc_tx_pause,
	ngbe_fc_full,
	ngbe_fc_default
};

struct ngbe_hw;

struct ngbe_addr_filter_info {
	u32 num_mc_addrs;
	u32 mta_in_use;
};

/* Bus parameters */
struct ngbe_bus_info {
	void (*set_lan_id)(struct ngbe_hw *hw);

	u16 func;
	u8 lan_id;
};

/* Flow control parameters */
struct ngbe_fc_info {
	u32 high_water; /* Flow Ctrl High-water */
	u32 low_water; /* Flow Ctrl Low-water */
	u16 pause_time; /* Flow Control Pause timer */
	bool send_xon; /* Flow control send XON */
	bool strict_ieee; /* Strict IEEE mode */
	bool disable_fc_autoneg; /* Do not autonegotiate FC */
	bool fc_was_autonegged; /* Is current_mode the result of autonegging? */
	enum ngbe_fc_mode current_mode; /* FC mode in effect */
	enum ngbe_fc_mode requested_mode; /* FC mode requested by caller */
};

/* Flow Control Data Sheet defined values
 * Calculation and defines taken from 802.1bb Annex O
 */
/* BitTimes (BT) conversion */
#define NGBE_BT2KB(BT)         (((BT) + (8 * 1024 - 1)) / (8 * 1024))
#define NGBE_B2BT(BT)          ((BT) * 8)

/* Calculate Delay to respond to PFC */
#define NGBE_PFC_D     672

/* Calculate Cable Delay */
#define NGBE_CABLE_DC  5556 /* Delay Copper */

/* Calculate Interface Delay */
#define NGBE_PHY_D     12800
#define NGBE_MAC_D     4096
#define NGBE_XAUI_D    (2 * 1024)

#define NGBE_ID        (NGBE_MAC_D + NGBE_XAUI_D + NGBE_PHY_D)

/* Calculate Delay incurred from higher layer */
#define NGBE_HD        6144

/* Calculate PCI Bus delay for low thresholds */
#define NGBE_PCI_DELAY 10000

/* Calculate delay value in bit times */
#define NGBE_DV(_max_frame_link, _max_frame_tc) \
			((36 * \
			  (NGBE_B2BT(_max_frame_link) + \
			   NGBE_PFC_D + \
			   (2 * NGBE_CABLE_DC) + \
			   (2 * NGBE_ID) + \
			   NGBE_HD) / 25 + 1) + \
			 2 * NGBE_B2BT(_max_frame_tc))

#define NGBE_LOW_DV(_max_frame_tc) \
			(2 * ((2 * NGBE_B2BT(_max_frame_tc) + \
			      (36 * NGBE_PCI_DELAY / 25) + 1)))

/* Statistics counters collected by the MAC */
/* PB[] RxTx */
struct ngbe_pb_stats {
	u64 tx_pb_xon_packets;
	u64 rx_pb_xon_packets;
	u64 tx_pb_xoff_packets;
	u64 rx_pb_xoff_packets;
	u64 rx_pb_dropped;
	u64 rx_pb_mbuf_alloc_errors;
	u64 tx_pb_xon2off_packets;
};

/* QP[] RxTx */
struct ngbe_qp_stats {
	u64 rx_qp_packets;
	u64 tx_qp_packets;
	u64 rx_qp_bytes;
	u64 tx_qp_bytes;
	u64 rx_qp_mc_packets;
};

struct ngbe_hw_stats {
	/* MNG RxTx */
	u64 mng_bmc2host_packets;
	u64 mng_host2bmc_packets;
	/* Basix RxTx */
	u64 rx_dma_drop;
	u64 tx_dma_drop;
	u64 tx_secdrp_packets;
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

	/* Basic Error */
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

	u64 rx_up_dropped;

	u64 rdb_pkt_cnt;
	u64 rdb_repli_cnt;
	u64 rdb_drp_cnt;

	/* QP[] RxTx */
	struct {
		u64 rx_qp_packets;
		u64 tx_qp_packets;
		u64 rx_qp_bytes;
		u64 tx_qp_bytes;
		u64 rx_qp_mc_packets;
		u64 tx_qp_mc_packets;
		u64 rx_qp_bc_packets;
		u64 tx_qp_bc_packets;
	} qp[NGBE_MAX_QP];

};

/* iterator type for walking multicast address lists */
typedef u8* (*ngbe_mc_addr_itr) (struct ngbe_hw *hw, u8 **mc_addr_ptr,
				  u32 *vmdq);

struct ngbe_rom_info {
	s32 (*init_params)(struct ngbe_hw *hw);
	s32 (*readw_buffer)(struct ngbe_hw *hw, u32 offset, u32 words,
			    void *data);
	s32 (*read32)(struct ngbe_hw *hw, u32 addr, u32 *data);
	s32 (*writew_buffer)(struct ngbe_hw *hw, u32 offset, u32 words,
			     void *data);
	s32 (*validate_checksum)(struct ngbe_hw *hw, u16 *checksum_val);

	enum ngbe_eeprom_type type;
	u32 semaphore_delay;
	u16 word_size;
	u16 address_bits;
	u16 word_page_size;
	u32 sw_addr;
	u32 saved_version;
	u16 cksum_devcap;
};

struct ngbe_mac_info {
	s32 (*init_hw)(struct ngbe_hw *hw);
	s32 (*reset_hw)(struct ngbe_hw *hw);
	s32 (*start_hw)(struct ngbe_hw *hw);
	s32 (*stop_hw)(struct ngbe_hw *hw);
	s32 (*clear_hw_cntrs)(struct ngbe_hw *hw);
	s32 (*get_mac_addr)(struct ngbe_hw *hw, u8 *mac_addr);
	s32 (*enable_rx_dma)(struct ngbe_hw *hw, u32 regval);
	s32 (*disable_sec_rx_path)(struct ngbe_hw *hw);
	s32 (*enable_sec_rx_path)(struct ngbe_hw *hw);
	s32 (*acquire_swfw_sync)(struct ngbe_hw *hw, u32 mask);
	void (*release_swfw_sync)(struct ngbe_hw *hw, u32 mask);

	/* Link */
	s32 (*setup_link)(struct ngbe_hw *hw, u32 speed,
			       bool autoneg_wait_to_complete);
	s32 (*check_link)(struct ngbe_hw *hw, u32 *speed,
			       bool *link_up, bool link_up_wait_to_complete);
	s32 (*get_link_capabilities)(struct ngbe_hw *hw,
				      u32 *speed, bool *autoneg);

	/* Packet Buffer manipulation */
	void (*setup_pba)(struct ngbe_hw *hw);

	/* LED */
	s32 (*led_on)(struct ngbe_hw *hw, u32 index);
	s32 (*led_off)(struct ngbe_hw *hw, u32 index);

	/* RAR */
	s32 (*set_rar)(struct ngbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
			  u32 enable_addr);
	s32 (*clear_rar)(struct ngbe_hw *hw, u32 index);
	s32 (*set_vmdq)(struct ngbe_hw *hw, u32 rar, u32 vmdq);
	s32 (*clear_vmdq)(struct ngbe_hw *hw, u32 rar, u32 vmdq);
	s32 (*init_rx_addrs)(struct ngbe_hw *hw);
	s32 (*update_mc_addr_list)(struct ngbe_hw *hw, u8 *mc_addr_list,
				      u32 mc_addr_count,
				      ngbe_mc_addr_itr func, bool clear);
	s32 (*clear_vfta)(struct ngbe_hw *hw);
	s32 (*set_vfta)(struct ngbe_hw *hw, u32 vlan,
			 u32 vind, bool vlan_on, bool vlvf_bypass);
	s32 (*set_vlvf)(struct ngbe_hw *hw, u32 vlan, u32 vind,
			   bool vlan_on, u32 *vfta_delta, u32 vfta,
			   bool vlvf_bypass);
	void (*set_mac_anti_spoofing)(struct ngbe_hw *hw, bool enable, int vf);
	void (*set_vlan_anti_spoofing)(struct ngbe_hw *hw,
					bool enable, int vf);

	/* Flow Control */
	s32 (*fc_enable)(struct ngbe_hw *hw);
	s32 (*setup_fc)(struct ngbe_hw *hw);
	void (*fc_autoneg)(struct ngbe_hw *hw);

	/* Manageability interface */
	s32 (*init_thermal_sensor_thresh)(struct ngbe_hw *hw);
	s32 (*check_overtemp)(struct ngbe_hw *hw);

	enum ngbe_mac_type type;
	enum ngbe_link_type link_type;
	u8 addr[ETH_ADDR_LEN];
	u8 perm_addr[ETH_ADDR_LEN];
#define NGBE_MAX_MTA			128
	u32 mta_shadow[NGBE_MAX_MTA];
	s32 mc_filter_type;
	u32 mcft_size;
	u32 vft_size;
	u32 num_rar_entries;
	u32 rx_pb_size;
	u32 max_tx_queues;
	u32 max_rx_queues;
	bool get_link_status;
	struct ngbe_thermal_sensor_data  thermal_sensor_data;
	bool set_lben;
	u32  max_link_up_time;

	u32 default_speeds;
	bool autoneg;
};

struct ngbe_phy_info {
	s32 (*identify)(struct ngbe_hw *hw);
	s32 (*init_hw)(struct ngbe_hw *hw);
	s32 (*reset_hw)(struct ngbe_hw *hw);
	s32 (*read_reg)(struct ngbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 *phy_data);
	s32 (*write_reg)(struct ngbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data);
	s32 (*read_reg_unlocked)(struct ngbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 *phy_data);
	s32 (*write_reg_unlocked)(struct ngbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data);
	s32 (*setup_link)(struct ngbe_hw *hw, u32 speed,
				bool autoneg_wait_to_complete);
	s32 (*check_link)(struct ngbe_hw *hw, u32 *speed, bool *link_up);
	s32 (*set_phy_power)(struct ngbe_hw *hw, bool on);
	s32 (*led_oem_chk)(struct ngbe_hw *hw, u32 *data);
	s32 (*get_adv_pause)(struct ngbe_hw *hw, u8 *pause_bit);
	s32 (*get_lp_adv_pause)(struct ngbe_hw *hw, u8 *pause_bit);
	s32 (*set_pause_adv)(struct ngbe_hw *hw, u16 pause_bit);

	enum ngbe_media_type media_type;
	enum ngbe_phy_type type;
	u32 addr;
	u32 id;
	u32 revision;
	u32 phy_semaphore_mask;
	bool reset_disable;
	u32 autoneg_advertised;
};

struct ngbe_mbx_stats {
	u32 msgs_tx;
	u32 msgs_rx;

	u32 acks;
	u32 reqs;
	u32 rsts;
};

struct ngbe_mbx_info {
	void (*init_params)(struct ngbe_hw *hw);
	s32  (*read)(struct ngbe_hw *hw, u32 *msg, u16 size, u16 vf_number);
	s32  (*write)(struct ngbe_hw *hw, u32 *msg, u16 size, u16 vf_number);
	s32  (*check_for_msg)(struct ngbe_hw *hw, u16 mbx_id);
	s32  (*check_for_ack)(struct ngbe_hw *hw, u16 mbx_id);
	s32  (*check_for_rst)(struct ngbe_hw *hw, u16 mbx_id);

	struct ngbe_mbx_stats stats;
	u32 timeout;
	u32 usec_delay;
	u16 size;
};

enum ngbe_isb_idx {
	NGBE_ISB_HEADER,
	NGBE_ISB_MISC,
	NGBE_ISB_VEC0,
	NGBE_ISB_VEC1,
	NGBE_ISB_MAX
};

struct ngbe_hw {
	void IOMEM *hw_addr;
	void *back;
	struct ngbe_mac_info mac;
	struct ngbe_addr_filter_info addr_ctrl;
	struct ngbe_fc_info fc;
	struct ngbe_phy_info phy;
	struct ngbe_rom_info rom;
	struct ngbe_bus_info bus;
	struct ngbe_mbx_info mbx;
	u16 device_id;
	u16 vendor_id;
	u16 sub_device_id;
	u16 sub_system_id;
	u32 eeprom_id;
	u8 revision_id;
	bool adapter_stopped;

	uint64_t isb_dma;
	void IOMEM *isb_mem;
	u16 nb_rx_queues;
	u16 nb_tx_queues;

	u32 mode;

	u32 q_rx_regs[8 * 4];
	u32 q_tx_regs[8 * 4];
	u32 gphy_efuse[2];
	bool offset_loaded;
	bool is_pf;
	bool gpio_ctl;
	bool lsc;
	u32 led_conf;
	bool init_phy;
	rte_spinlock_t phy_lock;
	struct {
		u64 rx_qp_packets;
		u64 tx_qp_packets;
		u64 rx_qp_bytes;
		u64 tx_qp_bytes;
		u64 rx_qp_mc_packets;
		u64 tx_qp_mc_packets;
		u64 rx_qp_bc_packets;
		u64 tx_qp_bc_packets;
	} qp_last[NGBE_MAX_QP];
};

#include "ngbe_regs.h"
#include "ngbe_dummy.h"

#endif /* _NGBE_TYPE_H_ */
