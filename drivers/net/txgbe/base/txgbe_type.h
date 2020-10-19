/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_TYPE_H_
#define _TXGBE_TYPE_H_

#define TXGBE_LINK_UP_TIME	90 /* 9.0 Seconds */

#define TXGBE_ALIGN				128 /* as intel did */

#include "txgbe_status.h"
#include "txgbe_osdep.h"
#include "txgbe_devids.h"

enum txgbe_mac_type {
	txgbe_mac_unknown = 0,
	txgbe_mac_raptor,
	txgbe_mac_raptor_vf,
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

struct txgbe_mac_info {
	enum txgbe_mac_type type;
	u8 perm_addr[ETH_ADDR_LEN];
	u32 num_rar_entries;
	u32  max_link_up_time;
};

struct txgbe_phy_info {
	enum txgbe_phy_type type;
	enum txgbe_sfp_type sfp_type;
	u32 media_type;
};

struct txgbe_hw {
	void IOMEM *hw_addr;
	void *back;
	struct txgbe_mac_info mac;
	struct txgbe_phy_info phy;

	struct txgbe_bus_info bus;
	u16 device_id;
	u16 vendor_id;
	u16 subsystem_device_id;
	u16 subsystem_vendor_id;

	bool allow_unsupported_sfp;

	uint64_t isb_dma;
	void IOMEM *isb_mem;
};

#include "txgbe_regs.h"
#endif /* _TXGBE_TYPE_H_ */
