/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _IGC_HW_H_
#define _IGC_HW_H_

#include "igc_osdep.h"
#include "igc_regs.h"
#include "igc_defines.h"

struct igc_hw;

#define IGC_DEV_ID_82542			0x1000
#define IGC_DEV_ID_82543GC_FIBER		0x1001
#define IGC_DEV_ID_82543GC_COPPER		0x1004
#define IGC_DEV_ID_82544EI_COPPER		0x1008
#define IGC_DEV_ID_82544EI_FIBER		0x1009
#define IGC_DEV_ID_82544GC_COPPER		0x100C
#define IGC_DEV_ID_82544GC_LOM		0x100D
#define IGC_DEV_ID_82540EM			0x100E
#define IGC_DEV_ID_82540EM_LOM		0x1015
#define IGC_DEV_ID_82540EP_LOM		0x1016
#define IGC_DEV_ID_82540EP			0x1017
#define IGC_DEV_ID_82540EP_LP			0x101E
#define IGC_DEV_ID_82545EM_COPPER		0x100F
#define IGC_DEV_ID_82545EM_FIBER		0x1011
#define IGC_DEV_ID_82545GM_COPPER		0x1026
#define IGC_DEV_ID_82545GM_FIBER		0x1027
#define IGC_DEV_ID_82545GM_SERDES		0x1028
#define IGC_DEV_ID_82546EB_COPPER		0x1010
#define IGC_DEV_ID_82546EB_FIBER		0x1012
#define IGC_DEV_ID_82546EB_QUAD_COPPER	0x101D
#define IGC_DEV_ID_82546GB_COPPER		0x1079
#define IGC_DEV_ID_82546GB_FIBER		0x107A
#define IGC_DEV_ID_82546GB_SERDES		0x107B
#define IGC_DEV_ID_82546GB_PCIE		0x108A
#define IGC_DEV_ID_82546GB_QUAD_COPPER	0x1099
#define IGC_DEV_ID_82546GB_QUAD_COPPER_KSP3	0x10B5
#define IGC_DEV_ID_82541EI			0x1013
#define IGC_DEV_ID_82541EI_MOBILE		0x1018
#define IGC_DEV_ID_82541ER_LOM		0x1014
#define IGC_DEV_ID_82541ER			0x1078
#define IGC_DEV_ID_82541GI			0x1076
#define IGC_DEV_ID_82541GI_LF			0x107C
#define IGC_DEV_ID_82541GI_MOBILE		0x1077
#define IGC_DEV_ID_82547EI			0x1019
#define IGC_DEV_ID_82547EI_MOBILE		0x101A
#define IGC_DEV_ID_82547GI			0x1075
#define IGC_DEV_ID_82571EB_COPPER		0x105E
#define IGC_DEV_ID_82571EB_FIBER		0x105F
#define IGC_DEV_ID_82571EB_SERDES		0x1060
#define IGC_DEV_ID_82571EB_SERDES_DUAL	0x10D9
#define IGC_DEV_ID_82571EB_SERDES_QUAD	0x10DA
#define IGC_DEV_ID_82571EB_QUAD_COPPER	0x10A4
#define IGC_DEV_ID_82571PT_QUAD_COPPER	0x10D5
#define IGC_DEV_ID_82571EB_QUAD_FIBER		0x10A5
#define IGC_DEV_ID_82571EB_QUAD_COPPER_LP	0x10BC
#define IGC_DEV_ID_82572EI_COPPER		0x107D
#define IGC_DEV_ID_82572EI_FIBER		0x107E
#define IGC_DEV_ID_82572EI_SERDES		0x107F
#define IGC_DEV_ID_82572EI			0x10B9
#define IGC_DEV_ID_82573E			0x108B
#define IGC_DEV_ID_82573E_IAMT		0x108C
#define IGC_DEV_ID_82573L			0x109A
#define IGC_DEV_ID_82574L			0x10D3
#define IGC_DEV_ID_82574LA			0x10F6
#define IGC_DEV_ID_82583V			0x150C
#define IGC_DEV_ID_80003ES2LAN_COPPER_DPT	0x1096
#define IGC_DEV_ID_80003ES2LAN_SERDES_DPT	0x1098
#define IGC_DEV_ID_80003ES2LAN_COPPER_SPT	0x10BA
#define IGC_DEV_ID_80003ES2LAN_SERDES_SPT	0x10BB
#define IGC_DEV_ID_ICH8_82567V_3		0x1501
#define IGC_DEV_ID_ICH8_IGP_M_AMT		0x1049
#define IGC_DEV_ID_ICH8_IGP_AMT		0x104A
#define IGC_DEV_ID_ICH8_IGP_C			0x104B
#define IGC_DEV_ID_ICH8_IFE			0x104C
#define IGC_DEV_ID_ICH8_IFE_GT		0x10C4
#define IGC_DEV_ID_ICH8_IFE_G			0x10C5
#define IGC_DEV_ID_ICH8_IGP_M			0x104D
#define IGC_DEV_ID_ICH9_IGP_M			0x10BF
#define IGC_DEV_ID_ICH9_IGP_M_AMT		0x10F5
#define IGC_DEV_ID_ICH9_IGP_M_V		0x10CB
#define IGC_DEV_ID_ICH9_IGP_AMT		0x10BD
#define IGC_DEV_ID_ICH9_BM			0x10E5
#define IGC_DEV_ID_ICH9_IGP_C			0x294C
#define IGC_DEV_ID_ICH9_IFE			0x10C0
#define IGC_DEV_ID_ICH9_IFE_GT		0x10C3
#define IGC_DEV_ID_ICH9_IFE_G			0x10C2
#define IGC_DEV_ID_ICH10_R_BM_LM		0x10CC
#define IGC_DEV_ID_ICH10_R_BM_LF		0x10CD
#define IGC_DEV_ID_ICH10_R_BM_V		0x10CE
#define IGC_DEV_ID_ICH10_D_BM_LM		0x10DE
#define IGC_DEV_ID_ICH10_D_BM_LF		0x10DF
#define IGC_DEV_ID_ICH10_D_BM_V		0x1525
#define IGC_DEV_ID_PCH_M_HV_LM		0x10EA
#define IGC_DEV_ID_PCH_M_HV_LC		0x10EB
#define IGC_DEV_ID_PCH_D_HV_DM		0x10EF
#define IGC_DEV_ID_PCH_D_HV_DC		0x10F0
#define IGC_DEV_ID_PCH2_LV_LM			0x1502
#define IGC_DEV_ID_PCH2_LV_V			0x1503
#define IGC_DEV_ID_PCH_LPT_I217_LM		0x153A
#define IGC_DEV_ID_PCH_LPT_I217_V		0x153B
#define IGC_DEV_ID_PCH_LPTLP_I218_LM		0x155A
#define IGC_DEV_ID_PCH_LPTLP_I218_V		0x1559
#define IGC_DEV_ID_PCH_I218_LM2		0x15A0
#define IGC_DEV_ID_PCH_I218_V2		0x15A1
#define IGC_DEV_ID_PCH_I218_LM3		0x15A2 /* Wildcat Point PCH */
#define IGC_DEV_ID_PCH_I218_V3		0x15A3 /* Wildcat Point PCH */
#define IGC_DEV_ID_PCH_SPT_I219_LM		0x156F /* Sunrise Point PCH */
#define IGC_DEV_ID_PCH_SPT_I219_V		0x1570 /* Sunrise Point PCH */
#define IGC_DEV_ID_PCH_SPT_I219_LM2		0x15B7 /* Sunrise Point-H PCH */
#define IGC_DEV_ID_PCH_SPT_I219_V2		0x15B8 /* Sunrise Point-H PCH */
#define IGC_DEV_ID_PCH_LBG_I219_LM3		0x15B9 /* LEWISBURG PCH */
#define IGC_DEV_ID_PCH_SPT_I219_LM4		0x15D7
#define IGC_DEV_ID_PCH_SPT_I219_V4		0x15D8
#define IGC_DEV_ID_PCH_SPT_I219_LM5		0x15E3
#define IGC_DEV_ID_PCH_SPT_I219_V5		0x15D6
#define IGC_DEV_ID_PCH_CNP_I219_LM6		0x15BD
#define IGC_DEV_ID_PCH_CNP_I219_V6		0x15BE
#define IGC_DEV_ID_PCH_CNP_I219_LM7		0x15BB
#define IGC_DEV_ID_PCH_CNP_I219_V7		0x15BC
#define IGC_DEV_ID_PCH_ICP_I219_LM8		0x15DF
#define IGC_DEV_ID_PCH_ICP_I219_V8		0x15E0
#define IGC_DEV_ID_PCH_ICP_I219_LM9		0x15E1
#define IGC_DEV_ID_PCH_ICP_I219_V9		0x15E2
#define IGC_DEV_ID_82576			0x10C9
#define IGC_DEV_ID_82576_FIBER		0x10E6
#define IGC_DEV_ID_82576_SERDES		0x10E7
#define IGC_DEV_ID_82576_QUAD_COPPER		0x10E8
#define IGC_DEV_ID_82576_QUAD_COPPER_ET2	0x1526
#define IGC_DEV_ID_82576_NS			0x150A
#define IGC_DEV_ID_82576_NS_SERDES		0x1518
#define IGC_DEV_ID_82576_SERDES_QUAD		0x150D
#define IGC_DEV_ID_82576_VF			0x10CA
#define IGC_DEV_ID_82576_VF_HV		0x152D
#define IGC_DEV_ID_I350_VF			0x1520
#define IGC_DEV_ID_I350_VF_HV			0x152F
#define IGC_DEV_ID_82575EB_COPPER		0x10A7
#define IGC_DEV_ID_82575EB_FIBER_SERDES	0x10A9
#define IGC_DEV_ID_82575GB_QUAD_COPPER	0x10D6
#define IGC_DEV_ID_82580_COPPER		0x150E
#define IGC_DEV_ID_82580_FIBER		0x150F
#define IGC_DEV_ID_82580_SERDES		0x1510
#define IGC_DEV_ID_82580_SGMII		0x1511
#define IGC_DEV_ID_82580_COPPER_DUAL		0x1516
#define IGC_DEV_ID_82580_QUAD_FIBER		0x1527
#define IGC_DEV_ID_I350_COPPER		0x1521
#define IGC_DEV_ID_I350_FIBER			0x1522
#define IGC_DEV_ID_I350_SERDES		0x1523
#define IGC_DEV_ID_I350_SGMII			0x1524
#define IGC_DEV_ID_I350_DA4			0x1546
#define IGC_DEV_ID_I210_COPPER		0x1533
#define IGC_DEV_ID_I210_COPPER_OEM1		0x1534
#define IGC_DEV_ID_I210_COPPER_IT		0x1535
#define IGC_DEV_ID_I210_FIBER			0x1536
#define IGC_DEV_ID_I210_SERDES		0x1537
#define IGC_DEV_ID_I210_SGMII			0x1538
#define IGC_DEV_ID_I210_COPPER_FLASHLESS	0x157B
#define IGC_DEV_ID_I210_SERDES_FLASHLESS	0x157C
#define IGC_DEV_ID_I210_SGMII_FLASHLESS	0x15F6
#define IGC_DEV_ID_I211_COPPER		0x1539
#define IGC_DEV_ID_I225_LM			0x15F2
#define IGC_DEV_ID_I225_V			0x15F3
#define IGC_DEV_ID_I225_K			0x3100
#define IGC_DEV_ID_I225_I			0x15F8
#define IGC_DEV_ID_I220_V			0x15F7
#define IGC_DEV_ID_I225_BLANK_NVM		0x15FD
#define IGC_DEV_ID_I354_BACKPLANE_1GBPS	0x1F40
#define IGC_DEV_ID_I354_SGMII			0x1F41
#define IGC_DEV_ID_I354_BACKPLANE_2_5GBPS	0x1F45
#define IGC_DEV_ID_DH89XXCC_SGMII		0x0438
#define IGC_DEV_ID_DH89XXCC_SERDES		0x043A
#define IGC_DEV_ID_DH89XXCC_BACKPLANE		0x043C
#define IGC_DEV_ID_DH89XXCC_SFP		0x0440

#define IGC_REVISION_0	0
#define IGC_REVISION_1	1
#define IGC_REVISION_2	2
#define IGC_REVISION_3	3
#define IGC_REVISION_4	4

#define IGC_FUNC_0		0
#define IGC_FUNC_1		1
#define IGC_FUNC_2		2
#define IGC_FUNC_3		3

#define IGC_ALT_MAC_ADDRESS_OFFSET_LAN0	0
#define IGC_ALT_MAC_ADDRESS_OFFSET_LAN1	3
#define IGC_ALT_MAC_ADDRESS_OFFSET_LAN2	6
#define IGC_ALT_MAC_ADDRESS_OFFSET_LAN3	9

enum igc_mac_type {
	igc_undefined = 0,
	igc_82542,
	igc_82543,
	igc_82544,
	igc_82540,
	igc_82545,
	igc_82545_rev_3,
	igc_82546,
	igc_82546_rev_3,
	igc_82541,
	igc_82541_rev_2,
	igc_82547,
	igc_82547_rev_2,
	igc_82571,
	igc_82572,
	igc_82573,
	igc_82574,
	igc_82583,
	igc_80003es2lan,
	igc_ich8lan,
	igc_ich9lan,
	igc_ich10lan,
	igc_pchlan,
	igc_pch2lan,
	igc_pch_lpt,
	igc_pch_spt,
	igc_pch_cnp,
	igc_82575,
	igc_82576,
	igc_82580,
	igc_i350,
	igc_i354,
	igc_i210,
	igc_i211,
	igc_i225,
	igc_vfadapt,
	igc_vfadapt_i350,
	igc_num_macs  /* List is 1-based, so subtract 1 for true count. */
};

enum igc_media_type {
	igc_media_type_unknown = 0,
	igc_media_type_copper = 1,
	igc_media_type_fiber = 2,
	igc_media_type_internal_serdes = 3,
	igc_num_media_types
};

enum igc_nvm_type {
	igc_nvm_unknown = 0,
	igc_nvm_none,
	igc_nvm_eeprom_spi,
	igc_nvm_eeprom_microwire,
	igc_nvm_flash_hw,
	igc_nvm_invm,
	igc_nvm_flash_sw
};

enum igc_nvm_override {
	igc_nvm_override_none = 0,
	igc_nvm_override_spi_small,
	igc_nvm_override_spi_large,
	igc_nvm_override_microwire_small,
	igc_nvm_override_microwire_large
};

enum igc_phy_type {
	igc_phy_unknown = 0,
	igc_phy_none,
	igc_phy_m88,
	igc_phy_igp,
	igc_phy_igp_2,
	igc_phy_gg82563,
	igc_phy_igp_3,
	igc_phy_ife,
	igc_phy_bm,
	igc_phy_82578,
	igc_phy_82577,
	igc_phy_82579,
	igc_phy_i217,
	igc_phy_82580,
	igc_phy_vf,
	igc_phy_i210,
	igc_phy_i225,
};

enum igc_bus_type {
	igc_bus_type_unknown = 0,
	igc_bus_type_pci,
	igc_bus_type_pcix,
	igc_bus_type_pci_express,
	igc_bus_type_reserved
};

enum igc_bus_speed {
	igc_bus_speed_unknown = 0,
	igc_bus_speed_33,
	igc_bus_speed_66,
	igc_bus_speed_100,
	igc_bus_speed_120,
	igc_bus_speed_133,
	igc_bus_speed_2500,
	igc_bus_speed_5000,
	igc_bus_speed_reserved
};

enum igc_bus_width {
	igc_bus_width_unknown = 0,
	igc_bus_width_pcie_x1,
	igc_bus_width_pcie_x2,
	igc_bus_width_pcie_x4 = 4,
	igc_bus_width_pcie_x8 = 8,
	igc_bus_width_32,
	igc_bus_width_64,
	igc_bus_width_reserved
};

enum igc_1000t_rx_status {
	igc_1000t_rx_status_not_ok = 0,
	igc_1000t_rx_status_ok,
	igc_1000t_rx_status_undefined = 0xFF
};

enum igc_rev_polarity {
	igc_rev_polarity_normal = 0,
	igc_rev_polarity_reversed,
	igc_rev_polarity_undefined = 0xFF
};

enum igc_fc_mode {
	igc_fc_none = 0,
	igc_fc_rx_pause,
	igc_fc_tx_pause,
	igc_fc_full,
	igc_fc_default = 0xFF
};

enum igc_ffe_config {
	igc_ffe_config_enabled = 0,
	igc_ffe_config_active,
	igc_ffe_config_blocked
};

enum igc_dsp_config {
	igc_dsp_config_disabled = 0,
	igc_dsp_config_enabled,
	igc_dsp_config_activated,
	igc_dsp_config_undefined = 0xFF
};

enum igc_ms_type {
	igc_ms_hw_default = 0,
	igc_ms_force_master,
	igc_ms_force_slave,
	igc_ms_auto
};

enum igc_smart_speed {
	igc_smart_speed_default = 0,
	igc_smart_speed_on,
	igc_smart_speed_off
};

enum igc_serdes_link_state {
	igc_serdes_link_down = 0,
	igc_serdes_link_autoneg_progress,
	igc_serdes_link_autoneg_complete,
	igc_serdes_link_forced_up
};

enum igc_invm_structure_type {
	igc_invm_uninitialized_structure		= 0x00,
	igc_invm_word_autoload_structure		= 0x01,
	igc_invm_csr_autoload_structure		= 0x02,
	igc_invm_phy_register_autoload_structure	= 0x03,
	igc_invm_rsa_key_sha256_structure		= 0x04,
	igc_invm_invalidated_structure		= 0x0f,
};

#define __le16 u16
#define __le32 u32
#define __le64 u64
/* Receive Descriptor */
struct igc_rx_desc {
	__le64 buffer_addr; /* Address of the descriptor's data buffer */
	__le16 length;      /* Length of data DMAed into data buffer */
	__le16 csum; /* Packet checksum */
	u8  status;  /* Descriptor status */
	u8  errors;  /* Descriptor Errors */
	__le16 special;
};

/* Receive Descriptor - Extended */
union igc_rx_desc_extended {
	struct {
		__le64 buffer_addr;
		__le64 reserved;
	} read;
	struct {
		struct {
			__le32 mrq; /* Multiple Rx Queues */
			union {
				__le32 rss; /* RSS Hash */
				struct {
					__le16 ip_id;  /* IP id */
					__le16 csum;   /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;  /* ext status/error */
			__le16 length;
			__le16 vlan; /* VLAN tag */
		} upper;
	} wb;  /* writeback */
};

#define MAX_PS_BUFFERS 4

/* Number of packet split data buffers (not including the header buffer) */
#define PS_PAGE_BUFFERS	(MAX_PS_BUFFERS - 1)

/* Receive Descriptor - Packet Split */
union igc_rx_desc_packet_split {
	struct {
		/* one buffer for protocol header(s), three data buffers */
		__le64 buffer_addr[MAX_PS_BUFFERS];
	} read;
	struct {
		struct {
			__le32 mrq;  /* Multiple Rx Queues */
			union {
				__le32 rss; /* RSS Hash */
				struct {
					__le16 ip_id;    /* IP id */
					__le16 csum;     /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;  /* ext status/error */
			__le16 length0;  /* length of buffer 0 */
			__le16 vlan;  /* VLAN tag */
		} middle;
		struct {
			__le16 header_status;
			/* length of buffers 1-3 */
			__le16 length[PS_PAGE_BUFFERS];
		} upper;
		__le64 reserved;
	} wb; /* writeback */
};

/* Transmit Descriptor */
struct igc_tx_desc {
	__le64 buffer_addr;   /* Address of the descriptor's data buffer */
	union {
		__le32 data;
		struct {
			__le16 length;  /* Data buffer length */
			u8 cso;  /* Checksum offset */
			u8 cmd;  /* Descriptor control */
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status; /* Descriptor status */
			u8 css;  /* Checksum start */
			__le16 special;
		} fields;
	} upper;
};

/* Offload Context Descriptor */
struct igc_context_desc {
	union {
		__le32 ip_config;
		struct {
			u8 ipcss;  /* IP checksum start */
			u8 ipcso;  /* IP checksum offset */
			__le16 ipcse;  /* IP checksum end */
		} ip_fields;
	} lower_setup;
	union {
		__le32 tcp_config;
		struct {
			u8 tucss;  /* TCP checksum start */
			u8 tucso;  /* TCP checksum offset */
			__le16 tucse;  /* TCP checksum end */
		} tcp_fields;
	} upper_setup;
	__le32 cmd_and_length;
	union {
		__le32 data;
		struct {
			u8 status;  /* Descriptor status */
			u8 hdr_len;  /* Header length */
			__le16 mss;  /* Maximum segment size */
		} fields;
	} tcp_seg_setup;
};

/* Offload data descriptor */
struct igc_data_desc {
	__le64 buffer_addr;  /* Address of the descriptor's buffer address */
	union {
		__le32 data;
		struct {
			__le16 length;  /* Data buffer length */
			u8 typ_len_ext;
			u8 cmd;
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status;  /* Descriptor status */
			u8 popts;  /* Packet Options */
			__le16 special;
		} fields;
	} upper;
};

/* Statistics counters collected by the MAC */
struct igc_hw_stats {
	u64 crcerrs;
	u64 algnerrc;
	u64 symerrs;
	u64 rxerrc;
	u64 mpc;
	u64 scc;
	u64 ecol;
	u64 mcc;
	u64 latecol;
	u64 colc;
	u64 dc;
	u64 tncrs;
	u64 sec;
	u64 cexterr;
	u64 rlec;
	u64 xonrxc;
	u64 xontxc;
	u64 xoffrxc;
	u64 xofftxc;
	u64 fcruc;
	u64 prc64;
	u64 prc127;
	u64 prc255;
	u64 prc511;
	u64 prc1023;
	u64 prc1522;
	u64 gprc;
	u64 bprc;
	u64 mprc;
	u64 gptc;
	u64 gorc;
	u64 gotc;
	u64 rnbc;
	u64 ruc;
	u64 rfc;
	u64 roc;
	u64 rjc;
	u64 mgprc;
	u64 mgpdc;
	u64 mgptc;
	u64 tor;
	u64 tot;
	u64 tpr;
	u64 tpt;
	u64 ptc64;
	u64 ptc127;
	u64 ptc255;
	u64 ptc511;
	u64 ptc1023;
	u64 ptc1522;
	u64 mptc;
	u64 bptc;
	u64 tsctc;
	u64 tsctfc;
	u64 iac;
	u64 icrxptc;
	u64 icrxatc;
	u64 ictxptc;
	u64 ictxatc;
	u64 ictxqec;
	u64 ictxqmtc;
	u64 icrxdmtc;
	u64 icrxoc;
	u64 cbtmpc;
	u64 htdpmc;
	u64 cbrdpc;
	u64 cbrmpc;
	u64 rpthc;
	u64 hgptc;
	u64 htcbdpc;
	u64 hgorc;
	u64 hgotc;
	u64 lenerrs;
	u64 scvpc;
	u64 hrmpc;
	u64 doosync;
	u64 o2bgptc;
	u64 o2bspc;
	u64 b2ospc;
	u64 b2ogprc;
};

struct igc_vf_stats {
	u64 base_gprc;
	u64 base_gptc;
	u64 base_gorc;
	u64 base_gotc;
	u64 base_mprc;
	u64 base_gotlbc;
	u64 base_gptlbc;
	u64 base_gorlbc;
	u64 base_gprlbc;

	u32 last_gprc;
	u32 last_gptc;
	u32 last_gorc;
	u32 last_gotc;
	u32 last_mprc;
	u32 last_gotlbc;
	u32 last_gptlbc;
	u32 last_gorlbc;
	u32 last_gprlbc;

	u64 gprc;
	u64 gptc;
	u64 gorc;
	u64 gotc;
	u64 mprc;
	u64 gotlbc;
	u64 gptlbc;
	u64 gorlbc;
	u64 gprlbc;
};

struct igc_phy_stats {
	u32 idle_errors;
	u32 receive_errors;
};

struct igc_host_mng_dhcp_cookie {
	u32 signature;
	u8  status;
	u8  reserved0;
	u16 vlan_id;
	u32 reserved1;
	u16 reserved2;
	u8  reserved3;
	u8  checksum;
};

/* Host Interface "Rev 1" */
struct igc_host_command_header {
	u8 command_id;
	u8 command_length;
	u8 command_options;
	u8 checksum;
};

#define IGC_HI_MAX_DATA_LENGTH	252
struct igc_host_command_info {
	struct igc_host_command_header command_header;
	u8 command_data[IGC_HI_MAX_DATA_LENGTH];
};

/* Host Interface "Rev 2" */
struct igc_host_mng_command_header {
	u8  command_id;
	u8  checksum;
	u16 reserved1;
	u16 reserved2;
	u16 command_length;
};

#define IGC_HI_MAX_MNG_DATA_LENGTH	0x6F8
struct igc_host_mng_command_info {
	struct igc_host_mng_command_header command_header;
	u8 command_data[IGC_HI_MAX_MNG_DATA_LENGTH];
};

#include "igc_mac.h"
#include "igc_phy.h"
#include "igc_nvm.h"
#include "igc_manage.h"

/* Function pointers for the MAC. */
struct igc_mac_operations {
	s32  (*init_params)(struct igc_hw *hw);
	s32  (*id_led_init)(struct igc_hw *hw);
	s32  (*blink_led)(struct igc_hw *hw);
	bool (*check_mng_mode)(struct igc_hw *hw);
	s32  (*check_for_link)(struct igc_hw *hw);
	s32  (*cleanup_led)(struct igc_hw *hw);
	void (*clear_hw_cntrs)(struct igc_hw *hw);
	void (*clear_vfta)(struct igc_hw *hw);
	s32  (*get_bus_info)(struct igc_hw *hw);
	void (*set_lan_id)(struct igc_hw *hw);
	s32  (*get_link_up_info)(struct igc_hw *hw, u16 *speed, u16 *duplex);
	s32  (*led_on)(struct igc_hw *hw);
	s32  (*led_off)(struct igc_hw *hw);
	void (*update_mc_addr_list)(struct igc_hw *hw,
			u8 *mc_addr_list, u32 count);
	s32  (*reset_hw)(struct igc_hw *hw);
	s32  (*init_hw)(struct igc_hw *hw);
	void (*shutdown_serdes)(struct igc_hw *hw);
	void (*power_up_serdes)(struct igc_hw *hw);
	s32  (*setup_link)(struct igc_hw *hw);
	s32  (*setup_physical_interface)(struct igc_hw *hw);
	s32  (*setup_led)(struct igc_hw *hw);
	void (*write_vfta)(struct igc_hw *hw, u32 offset, u32 value);
	void (*config_collision_dist)(struct igc_hw *hw);
	int  (*rar_set)(struct igc_hw *hw, u8 *addr, u32 index);
	s32  (*read_mac_addr)(struct igc_hw *hw);
	s32  (*validate_mdi_setting)(struct igc_hw *hw);
	s32  (*acquire_swfw_sync)(struct igc_hw *hw, u16 mask);
	void (*release_swfw_sync)(struct igc_hw *hw, u16 mask);
};

/* When to use various PHY register access functions:
 *
 *                 Func   Caller
 *   Function      Does   Does    When to use
 *   ~~~~~~~~~~~~  ~~~~~  ~~~~~~  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   X_reg         L,P,A  n/a     for simple PHY reg accesses
 *   X_reg_locked  P,A    L       for multiple accesses of different regs
 *                                on different pages
 *   X_reg_page    A      L,P     for multiple accesses of different regs
 *                                on the same page
 *
 * Where X=[read|write], L=locking, P=sets page, A=register access
 *
 */
struct igc_phy_operations {
	s32  (*init_params)(struct igc_hw *hw);
	s32  (*acquire)(struct igc_hw *hw);
	s32  (*cfg_on_link_up)(struct igc_hw *hw);
	s32  (*check_polarity)(struct igc_hw *hw);
	s32  (*check_reset_block)(struct igc_hw *hw);
	s32  (*commit)(struct igc_hw *hw);
	s32  (*force_speed_duplex)(struct igc_hw *hw);
	s32  (*get_cfg_done)(struct igc_hw *hw);
	s32  (*get_cable_length)(struct igc_hw *hw);
	s32  (*get_info)(struct igc_hw *hw);
	s32  (*set_page)(struct igc_hw *hw, u16 page);
	s32  (*read_reg)(struct igc_hw *hw, u32 offset, u16 *data);
	s32  (*read_reg_locked)(struct igc_hw *hw, u32 offset, u16 *data);
	s32  (*read_reg_page)(struct igc_hw *hw, u32 offset, u16 *data);
	void (*release)(struct igc_hw *hw);
	s32  (*reset)(struct igc_hw *hw);
	s32  (*set_d0_lplu_state)(struct igc_hw *hw, bool active);
	s32  (*set_d3_lplu_state)(struct igc_hw *hw, bool active);
	s32  (*write_reg)(struct igc_hw *hw, u32 offset, u16 data);
	s32  (*write_reg_locked)(struct igc_hw *hw, u32 offset, u16 data);
	s32  (*write_reg_page)(struct igc_hw *hw, u32 offset, u16 data);
	void (*power_up)(struct igc_hw *hw);
	void (*power_down)(struct igc_hw *hw);
	s32 (*read_i2c_byte)(struct igc_hw *hw, u8 byte_offset,
			u8 dev_addr, u8 *data);
	s32 (*write_i2c_byte)(struct igc_hw *hw, u8 byte_offset,
			u8 dev_addr, u8 data);
};

/* Function pointers for the NVM. */
struct igc_nvm_operations {
	s32  (*init_params)(struct igc_hw *hw);
	s32  (*acquire)(struct igc_hw *hw);
	s32  (*read)(struct igc_hw *hw, u16 offset, u16 words, u16 *data);
	void (*release)(struct igc_hw *hw);
	void (*reload)(struct igc_hw *hw);
	s32  (*update)(struct igc_hw *hw);
	s32  (*valid_led_default)(struct igc_hw *hw, u16 *data);
	s32  (*validate)(struct igc_hw *hw);
	s32  (*write)(struct igc_hw *hw, u16 offset, u16 words, u16 *data);
};

struct igc_info {
	s32 (*get_invariants)(struct igc_hw *hw);
	struct igc_mac_operations *mac_ops;
	const struct igc_phy_operations *phy_ops;
	struct igc_nvm_operations *nvm_ops;
};

extern const struct igc_info igc_i225_info;

struct igc_mac_info {
	struct igc_mac_operations ops;
	u8 addr[ETH_ADDR_LEN];
	u8 perm_addr[ETH_ADDR_LEN];

	enum igc_mac_type type;

	u32 collision_delta;
	u32 ledctl_default;
	u32 ledctl_mode1;
	u32 ledctl_mode2;
	u32 mc_filter_type;
	u32 tx_packet_delta;
	u32 txcw;

	u16 current_ifs_val;
	u16 ifs_max_val;
	u16 ifs_min_val;
	u16 ifs_ratio;
	u16 ifs_step_size;
	u16 mta_reg_count;
	u16 uta_reg_count;

	/* Maximum size of the MTA register table in all supported adapters */
#define MAX_MTA_REG 128
	u32 mta_shadow[MAX_MTA_REG];
	u16 rar_entry_count;

	u8  forced_speed_duplex;

	bool adaptive_ifs;
	bool has_fwsm;
	bool arc_subsystem_valid;
	bool asf_firmware_present;
	bool autoneg;
	bool autoneg_failed;
	bool get_link_status;
	bool in_ifs_mode;
	bool report_tx_early;
	enum igc_serdes_link_state serdes_link_state;
	bool serdes_has_link;
	bool tx_pkt_filtering;
};

struct igc_phy_info {
	struct igc_phy_operations ops;
	enum igc_phy_type type;

	enum igc_1000t_rx_status local_rx;
	enum igc_1000t_rx_status remote_rx;
	enum igc_ms_type ms_type;
	enum igc_ms_type original_ms_type;
	enum igc_rev_polarity cable_polarity;
	enum igc_smart_speed smart_speed;

	u32 addr;
	u32 id;
	u32 reset_delay_us; /* in usec */
	u32 revision;

	enum igc_media_type media_type;

	u16 autoneg_advertised;
	u16 autoneg_mask;
	u16 cable_length;
	u16 max_cable_length;
	u16 min_cable_length;

	u8 mdix;

	bool disable_polarity_correction;
	bool is_mdix;
	bool polarity_correction;
	bool speed_downgraded;
	bool autoneg_wait_to_complete;
};

struct igc_nvm_info {
	struct igc_nvm_operations ops;
	enum igc_nvm_type type;
	enum igc_nvm_override override;

	u32 flash_bank_size;
	u32 flash_base_addr;

	u16 word_size;
	u16 delay_usec;
	u16 address_bits;
	u16 opcode_bits;
	u16 page_size;
};

struct igc_bus_info {
	enum igc_bus_type type;
	enum igc_bus_speed speed;
	enum igc_bus_width width;

	u16 func;
	u16 pci_cmd_word;
};

struct igc_fc_info {
	u32 high_water;  /* Flow control high-water mark */
	u32 low_water;  /* Flow control low-water mark */
	u16 pause_time;  /* Flow control pause timer */
	u16 refresh_time;  /* Flow control refresh timer */
	bool send_xon;  /* Flow control send XON */
	bool strict_ieee;  /* Strict IEEE mode */
	enum igc_fc_mode current_mode;  /* FC mode in effect */
	enum igc_fc_mode requested_mode;  /* FC mode requested by caller */
};

struct igc_mbx_operations {
	s32 (*init_params)(struct igc_hw *hw);
};

struct igc_mbx_stats {
	u32 msgs_tx;
	u32 msgs_rx;

	u32 acks;
	u32 reqs;
	u32 rsts;
};

struct igc_mbx_info {
	struct igc_mbx_operations ops;
	struct igc_mbx_stats stats;
	u32 timeout;
	u32 usec_delay;
	u16 size;
};

struct igc_dev_spec_82541 {
	enum igc_dsp_config dsp_config;
	enum igc_ffe_config ffe_config;
	u16 spd_default;
	bool phy_init_script;
};

struct igc_dev_spec_82542 {
	bool dma_fairness;
};

struct igc_dev_spec_82543 {
	u32  tbi_compatibility;
	bool dma_fairness;
	bool init_phy_disabled;
};

struct igc_dev_spec_82571 {
	bool laa_is_present;
	u32 smb_counter;
	IGC_MUTEX swflag_mutex;
};

struct igc_dev_spec_80003es2lan {
	bool  mdic_wa_enable;
};

struct igc_shadow_ram {
	u16  value;
	bool modified;
};

#define IGC_SHADOW_RAM_WORDS		2048

/* I218 PHY Ultra Low Power (ULP) states */
enum igc_ulp_state {
	igc_ulp_state_unknown,
	igc_ulp_state_off,
	igc_ulp_state_on,
};

struct igc_dev_spec_ich8lan {
	bool kmrn_lock_loss_workaround_enabled;
	struct igc_shadow_ram shadow_ram[IGC_SHADOW_RAM_WORDS];
	IGC_MUTEX nvm_mutex;
	IGC_MUTEX swflag_mutex;
	bool nvm_k1_enabled;
	bool disable_k1_off;
	bool eee_disable;
	u16 eee_lp_ability;
	enum igc_ulp_state ulp_state;
	bool ulp_capability_disabled;
	bool during_suspend_flow;
	bool during_dpg_exit;
	u16 lat_enc;
	u16 max_ltr_enc;
	bool smbus_disable;
};

struct igc_dev_spec_82575 {
	bool sgmii_active;
	bool global_device_reset;
	bool eee_disable;
	bool module_plugged;
	bool clear_semaphore_once;
	u32 mtu;
	struct sfp_igc_flags eth_flags;
	u8 media_port;
	bool media_changed;
};

struct igc_dev_spec_vf {
	u32 vf_number;
	u32 v2p_mailbox;
};

struct igc_dev_spec_i225 {
	bool global_device_reset;
	bool eee_disable;
	bool clear_semaphore_once;
	bool module_plugged;
	u8 media_port;
	bool mas_capable;
	u32 mtu;
};

struct igc_hw {
	void *back;

	u8 *hw_addr;
	u8 *flash_address;
	unsigned long io_base;

	struct igc_mac_info  mac;
	struct igc_fc_info   fc;
	struct igc_phy_info  phy;
	struct igc_nvm_info  nvm;
	struct igc_bus_info  bus;
	struct igc_mbx_info mbx;
	struct igc_host_mng_dhcp_cookie mng_cookie;

	union {
		struct igc_dev_spec_82541 _82541;
		struct igc_dev_spec_82542 _82542;
		struct igc_dev_spec_82543 _82543;
		struct igc_dev_spec_82571 _82571;
		struct igc_dev_spec_80003es2lan _80003es2lan;
		struct igc_dev_spec_ich8lan ich8lan;
		struct igc_dev_spec_82575 _82575;
		struct igc_dev_spec_vf vf;
		struct igc_dev_spec_i225 _i225;
	} dev_spec;

	u16 device_id;
	u16 subsystem_vendor_id;
	u16 subsystem_device_id;
	u16 vendor_id;

	u8  revision_id;
};

#include "igc_82571.h"
#include "igc_ich8lan.h"
#include "igc_82575.h"
#include "igc_i225.h"
#include "igc_base.h"

/* These functions must be implemented by drivers */
void igc_pci_clear_mwi(struct igc_hw *hw);
void igc_pci_set_mwi(struct igc_hw *hw);
s32  igc_read_pcie_cap_reg(struct igc_hw *hw, u32 reg, u16 *value);
s32  igc_write_pcie_cap_reg(struct igc_hw *hw, u32 reg, u16 *value);
void igc_read_pci_cfg(struct igc_hw *hw, u32 reg, u16 *value);
void igc_write_pci_cfg(struct igc_hw *hw, u32 reg, u16 *value);

#endif
