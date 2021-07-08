/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_TYPE_H_
#define _NGBE_TYPE_H_

#define NGBE_LINK_UP_TIME	90 /* 9.0 Seconds */

#define NGBE_FRAME_SIZE_DFT       (1522) /* Default frame size, +FCS */

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

struct ngbe_hw;

struct ngbe_addr_filter_info {
	u32 mta_in_use;
};

/* Bus parameters */
struct ngbe_bus_info {
	void (*set_lan_id)(struct ngbe_hw *hw);

	u16 func;
	u8 lan_id;
};

struct ngbe_rom_info {
	s32 (*init_params)(struct ngbe_hw *hw);
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

	/* RAR */
	s32 (*set_rar)(struct ngbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
			  u32 enable_addr);
	s32 (*clear_rar)(struct ngbe_hw *hw, u32 index);
	s32 (*set_vmdq)(struct ngbe_hw *hw, u32 rar, u32 vmdq);
	s32 (*clear_vmdq)(struct ngbe_hw *hw, u32 rar, u32 vmdq);
	s32 (*init_rx_addrs)(struct ngbe_hw *hw);

	/* Manageability interface */
	s32 (*init_thermal_sensor_thresh)(struct ngbe_hw *hw);
	s32 (*check_overtemp)(struct ngbe_hw *hw);

	enum ngbe_mac_type type;
	u8 addr[ETH_ADDR_LEN];
	u8 perm_addr[ETH_ADDR_LEN];
	s32 mc_filter_type;
	u32 mcft_size;
	u32 num_rar_entries;
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

	enum ngbe_media_type media_type;
	enum ngbe_phy_type type;
	u32 addr;
	u32 id;
	u32 revision;
	u32 phy_semaphore_mask;
	bool reset_disable;
	u32 autoneg_advertised;
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
	struct ngbe_phy_info phy;
	struct ngbe_rom_info rom;
	struct ngbe_bus_info bus;
	u16 device_id;
	u16 vendor_id;
	u16 sub_device_id;
	u16 sub_system_id;
	bool adapter_stopped;

	uint64_t isb_dma;
	void IOMEM *isb_mem;
	u16 nb_rx_queues;
	u16 nb_tx_queues;

	u32 q_rx_regs[8 * 4];
	u32 q_tx_regs[8 * 4];
	bool is_pf;
};

#include "ngbe_regs.h"
#include "ngbe_dummy.h"

#endif /* _NGBE_TYPE_H_ */
