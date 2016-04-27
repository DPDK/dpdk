/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

/****************************************************************************
 *
 * Name:        mcp_public.h
 *
 * Description: MCP public data
 *
 * Created:     13/01/2013 yanivr
 *
 ****************************************************************************/

#ifndef MCP_PUBLIC_H
#define MCP_PUBLIC_H

#define VF_MAX_STATIC 192	/* In case of AH */

#define MCP_GLOB_PATH_MAX	2
#define MCP_PORT_MAX		2	/* Global */
#define MCP_GLOB_PORT_MAX	4	/* Global */
#define MCP_GLOB_FUNC_MAX	16	/* Global */

typedef u32 offsize_t;		/* In DWORDS !!! */
/* Offset from the beginning of the MCP scratchpad */
#define OFFSIZE_OFFSET_SHIFT	0
#define OFFSIZE_OFFSET_MASK	0x0000ffff
/* Size of specific element (not the whole array if any) */
#define OFFSIZE_SIZE_SHIFT	16
#define OFFSIZE_SIZE_MASK	0xffff0000

/* SECTION_OFFSET is calculating the offset in bytes out of offsize */
#define SECTION_OFFSET(_offsize) \
((((_offsize & OFFSIZE_OFFSET_MASK) >> OFFSIZE_OFFSET_SHIFT) << 2))

/* SECTION_SIZE is calculating the size in bytes out of offsize */
#define SECTION_SIZE(_offsize) \
(((_offsize & OFFSIZE_SIZE_MASK) >> OFFSIZE_SIZE_SHIFT) << 2)

#define SECTION_ADDR(_offsize, idx) \
(MCP_REG_SCRATCH + SECTION_OFFSET(_offsize) + (SECTION_SIZE(_offsize) * idx))

#define SECTION_OFFSIZE_ADDR(_pub_base, _section) \
(_pub_base + offsetof(struct mcp_public_data, sections[_section]))

/* PHY configuration */
struct pmm_phy_cfg {
	u32 speed; /* 0 = autoneg, 1000/10000/20000/25000/40000/50000/100000 */
#define PMM_SPEED_AUTONEG   0
#define PMM_SPEED_SMARTLINQ  0x8

	u32 pause;		/* bitmask */
#define PMM_PAUSE_NONE		0x0
#define PMM_PAUSE_AUTONEG	0x1
#define PMM_PAUSE_RX		0x2
#define PMM_PAUSE_TX		0x4

	u32 adv_speed;		/* Default should be the speed_cap_mask */
	u32 loopback_mode;
#define PMM_LOOPBACK_NONE		0
#define PMM_LOOPBACK_INT_PHY		1
#define PMM_LOOPBACK_EXT_PHY		2
#define PMM_LOOPBACK_EXT		3
#define PMM_LOOPBACK_MAC		4
#define PMM_LOOPBACK_CNIG_AH_ONLY_0123	5	/* Port to itself */
#define PMM_LOOPBACK_CNIG_AH_ONLY_2301	6	/* Port to Port */

	/* features */
	u32 feature_config_flags;

};

struct port_mf_cfg {
	u32 dynamic_cfg;	/* device control channel */
#define PORT_MF_CFG_OV_TAG_MASK              0x0000ffff
#define PORT_MF_CFG_OV_TAG_SHIFT             0
#define PORT_MF_CFG_OV_TAG_DEFAULT         PORT_MF_CFG_OV_TAG_MASK

	u32 reserved[1];
};

/* DO NOT add new fields in the middle
 * MUST be synced with struct pmm_stats_map
 */
struct pmm_stats {
	u64 r64; /* 0x00 (Offset 0x00 ) RX 64-byte frame counter */
	u64 r127; /* 0x01 (Offset 0x08 ) RX 65 to 127 byte frame counter */
	u64 r255; /* 0x02 (Offset 0x10 ) RX 128 to 255 byte frame counter */
	u64 r511; /* 0x03 (Offset 0x18 ) RX 256 to 511 byte frame counter */
	u64 r1023; /* 0x04 (Offset 0x20 ) RX 512 to 1023 byte frame counter */
	u64 r1518; /* 0x05 (Offset 0x28 ) RX 1024 to 1518 byte frame counter */
	u64 r1522; /* 0x06 (Offset 0x30 ) RX 1519 to 1522 byte VLAN-tagged  */
	u64 r2047; /* 0x07 (Offset 0x38 ) RX 1519 to 2047 byte frame counter */
	u64 r4095; /* 0x08 (Offset 0x40 ) RX 2048 to 4095 byte frame counter */
	u64 r9216; /* 0x09 (Offset 0x48 ) RX 4096 to 9216 byte frame counter */
	u64 r16383; /* 0x0A (Offset 0x50 ) RX 9217 to 16383 byte frame ctr */
	u64 rfcs; /* 0x0F (Offset 0x58 ) RX FCS error frame counter */
	u64 rxcf; /* 0x10 (Offset 0x60 ) RX control frame counter */
	u64 rxpf; /* 0x11 (Offset 0x68 ) RX pause frame counter */
	u64 rxpp; /* 0x12 (Offset 0x70 ) RX PFC frame counter */
	u64 raln; /* 0x16 (Offset 0x78 ) RX alignment error counter */
	u64 rfcr; /* 0x19 (Offset 0x80 ) RX false carrier counter */
	u64 rovr; /* 0x1A (Offset 0x88 ) RX oversized frame counter */
	u64 rjbr; /* 0x1B (Offset 0x90 ) RX jabber frame counter */
	u64 rund; /* 0x34 (Offset 0x98 ) RX undersized frame counter */
	u64 rfrg; /* 0x35 (Offset 0xa0 ) RX fragment counter */
	u64 t64; /* 0x40 (Offset 0xa8 ) TX 64-byte frame counter */
	u64 t127; /* 0x41 (Offset 0xb0 ) TX 65 to 127 byte frame counter */
	u64 t255; /* 0x42 (Offset 0xb8 ) TX 128 to 255 byte frame counter */
	u64 t511; /* 0x43 (Offset 0xc0 ) TX 256 to 511 byte frame counter */
	u64 t1023; /* 0x44 (Offset 0xc8 ) TX 512 to 1023 byte frame counter */
	u64 t1518; /* 0x45 (Offset 0xd0 ) TX 1024 to 1518 byte frame counter */
	u64 t2047; /* 0x47 (Offset 0xd8 ) TX 1519 to 2047 byte frame counter */
	u64 t4095; /* 0x48 (Offset 0xe0 ) TX 2048 to 4095 byte frame counter */
	u64 t9216; /* 0x49 (Offset 0xe8 ) TX 4096 to 9216 byte frame counter */
	u64 t16383; /* 0x4A (Offset 0xf0 ) TX 9217 to 16383 byte frame ctr */
	u64 txpf; /* 0x50 (Offset 0xf8 ) TX pause frame counter */
	u64 txpp; /* 0x51 (Offset 0x100) TX PFC frame counter */
	u64 tlpiec; /* 0x6C (Offset 0x108) Transmit Logical Type LLFC */
	u64 tncl; /* 0x6E (Offset 0x110) Transmit Total Collision Counter */
	u64 rbyte; /* 0x3d (Offset 0x118) RX byte counter */
	u64 rxuca; /* 0x0c (Offset 0x120) RX UC frame counter */
	u64 rxmca; /* 0x0d (Offset 0x128) RX MC frame counter */
	u64 rxbca; /* 0x0e (Offset 0x130) RX BC frame counter */
	u64 rxpok; /* 0x22 (Offset 0x138) RX good frame */
	u64 tbyte; /* 0x6f (Offset 0x140) TX byte counter */
	u64 txuca; /* 0x4d (Offset 0x148) TX UC frame counter */
	u64 txmca; /* 0x4e (Offset 0x150) TX MC frame counter */
	u64 txbca; /* 0x4f (Offset 0x158) TX BC frame counter */
	u64 txcf; /* 0x54 (Offset 0x160) TX control frame counter */
};

struct brb_stats {
	u64 brb_truncate[8];
	u64 brb_discard[8];
};

struct port_stats {
	struct brb_stats brb;
	struct pmm_stats pmm;
};

/*-----+-----------------------------------------------------------------------
 * Chip | Number and       | Ports in| Ports in|2 PHY-s |# of ports|# of engines
 *      | rate of physical | team #1 | team #2 |are used|per path  | (paths)
 *      | ports            |         |         |        |          |
 *======+==================+=========+=========+========+======================
 * BB   | 1x100G           | This is special mode, where there are 2 HW func
 * BB   | 2x10/20Gbps      | 0,1     | NA      |  No    | 1        | 1
 * BB   | 2x40 Gbps        | 0,1     | NA      |  Yes   | 1        | 1
 * BB   | 2x50Gbps         | 0,1     | NA      |  No    | 1        | 1
 * BB   | 4x10Gbps         | 0,2     | 1,3     |  No    | 1/2      | 1,2
 * BB   | 4x10Gbps         | 0,1     | 2,3     |  No    | 1/2      | 1,2
 * BB   | 4x10Gbps         | 0,3     | 1,2     |  No    | 1/2      | 1,2
 * BB   | 4x10Gbps         | 0,1,2,3 | NA      |  No    | 1        | 1
 * AH   | 2x10/20Gbps      | 0,1     | NA      |  NA    | 1        | NA
 * AH   | 4x10Gbps         | 0,1     | 2,3     |  NA    | 2        | NA
 * AH   | 4x10Gbps         | 0,2     | 1,3     |  NA    | 2        | NA
 * AH   | 4x10Gbps         | 0,3     | 1,2     |  NA    | 2        | NA
 * AH   | 4x10Gbps         | 0,1,2,3 | NA      |  NA    | 1        | NA
 *======+==================+=========+=========+========+=======================
 */

#define CMT_TEAM0 0
#define CMT_TEAM1 1
#define CMT_TEAM_MAX 2

struct couple_mode_teaming {
	u8 port_cmt[MCP_GLOB_PORT_MAX];
#define PORT_CMT_IN_TEAM            (1 << 0)

#define PORT_CMT_PORT_ROLE          (1 << 1)
#define PORT_CMT_PORT_INACTIVE      (0 << 1)
#define PORT_CMT_PORT_ACTIVE        (1 << 1)

#define PORT_CMT_TEAM_MASK          (1 << 2)
#define PORT_CMT_TEAM0              (0 << 2)
#define PORT_CMT_TEAM1              (1 << 2)
};

/**************************************
 *     LLDP and DCBX HSI structures
 **************************************/
#define LLDP_CHASSIS_ID_STAT_LEN 4
#define LLDP_PORT_ID_STAT_LEN 4
#define DCBX_MAX_APP_PROTOCOL		32
#define MAX_SYSTEM_LLDP_TLV_DATA    32

typedef enum _lldp_agent_e {
	LLDP_NEAREST_BRIDGE = 0,
	LLDP_NEAREST_NON_TPMR_BRIDGE,
	LLDP_NEAREST_CUSTOMER_BRIDGE,
	LLDP_MAX_LLDP_AGENTS
} lldp_agent_e;

struct lldp_config_params_s {
	u32 config;
#define LLDP_CONFIG_TX_INTERVAL_MASK        0x000000ff
#define LLDP_CONFIG_TX_INTERVAL_SHIFT       0
#define LLDP_CONFIG_HOLD_MASK               0x00000f00
#define LLDP_CONFIG_HOLD_SHIFT              8
#define LLDP_CONFIG_MAX_CREDIT_MASK         0x0000f000
#define LLDP_CONFIG_MAX_CREDIT_SHIFT        12
#define LLDP_CONFIG_ENABLE_RX_MASK          0x40000000
#define LLDP_CONFIG_ENABLE_RX_SHIFT         30
#define LLDP_CONFIG_ENABLE_TX_MASK          0x80000000
#define LLDP_CONFIG_ENABLE_TX_SHIFT         31
	/* Holds local Chassis ID TLV header, subtype and 9B of payload.
	 * If firtst byte is 0, then we will use default chassis ID
	 */
	u32 local_chassis_id[LLDP_CHASSIS_ID_STAT_LEN];
	/* Holds local Port ID TLV header, subtype and 9B of payload.
	 * If firtst byte is 0, then we will use default port ID
	 */
	u32 local_port_id[LLDP_PORT_ID_STAT_LEN];
};

struct lldp_status_params_s {
	u32 prefix_seq_num;
	u32 status;		/* TBD */
	/* Holds remote Chassis ID TLV header, subtype and 9B of payload.
	 */
	u32 local_port_id[LLDP_PORT_ID_STAT_LEN];
	u32 peer_chassis_id[LLDP_CHASSIS_ID_STAT_LEN];
	/* Holds remote Port ID TLV header, subtype and 9B of payload.
	 */
	u32 peer_port_id[LLDP_PORT_ID_STAT_LEN];
	u32 suffix_seq_num;
};

struct dcbx_ets_feature {
	u32 flags;
#define DCBX_ETS_ENABLED_MASK                   0x00000001
#define DCBX_ETS_ENABLED_SHIFT                  0
#define DCBX_ETS_WILLING_MASK                   0x00000002
#define DCBX_ETS_WILLING_SHIFT                  1
#define DCBX_ETS_ERROR_MASK                     0x00000004
#define DCBX_ETS_ERROR_SHIFT                    2
#define DCBX_ETS_CBS_MASK                       0x00000008
#define DCBX_ETS_CBS_SHIFT                      3
#define DCBX_ETS_MAX_TCS_MASK                   0x000000f0
#define DCBX_ETS_MAX_TCS_SHIFT                  4
	u32 pri_tc_tbl[1];
#define DCBX_CEE_STRICT_PRIORITY		0xf
#define DCBX_CEE_STRICT_PRIORITY_TC		0x7
	u32 tc_bw_tbl[2];
	u32 tc_tsa_tbl[2];
#define DCBX_ETS_TSA_STRICT			0
#define DCBX_ETS_TSA_CBS			1
#define DCBX_ETS_TSA_ETS			2
};

struct dcbx_app_priority_entry {
	u32 entry;
#define DCBX_APP_PRI_MAP_MASK       0x000000ff
#define DCBX_APP_PRI_MAP_SHIFT      0
#define DCBX_APP_PRI_0              0x01
#define DCBX_APP_PRI_1              0x02
#define DCBX_APP_PRI_2              0x04
#define DCBX_APP_PRI_3              0x08
#define DCBX_APP_PRI_4              0x10
#define DCBX_APP_PRI_5              0x20
#define DCBX_APP_PRI_6              0x40
#define DCBX_APP_PRI_7              0x80
#define DCBX_APP_SF_MASK            0x00000300
#define DCBX_APP_SF_SHIFT           8
#define DCBX_APP_SF_ETHTYPE         0
#define DCBX_APP_SF_PORT            1
#define DCBX_APP_PROTOCOL_ID_MASK   0xffff0000
#define DCBX_APP_PROTOCOL_ID_SHIFT  16
};

/* FW structure in BE */
struct dcbx_app_priority_feature {
	u32 flags;
#define DCBX_APP_ENABLED_MASK           0x00000001
#define DCBX_APP_ENABLED_SHIFT          0
#define DCBX_APP_WILLING_MASK           0x00000002
#define DCBX_APP_WILLING_SHIFT          1
#define DCBX_APP_ERROR_MASK             0x00000004
#define DCBX_APP_ERROR_SHIFT            2
	/* Not in use
	 * #define DCBX_APP_DEFAULT_PRI_MASK       0x00000f00
	 * #define DCBX_APP_DEFAULT_PRI_SHIFT      8
	 */
#define DCBX_APP_MAX_TCS_MASK           0x0000f000
#define DCBX_APP_MAX_TCS_SHIFT          12
#define DCBX_APP_NUM_ENTRIES_MASK       0x00ff0000
#define DCBX_APP_NUM_ENTRIES_SHIFT      16
	struct dcbx_app_priority_entry app_pri_tbl[DCBX_MAX_APP_PROTOCOL];
};

/* FW structure in BE */
struct dcbx_features {
	/* PG feature */
	struct dcbx_ets_feature ets;
	/* PFC feature */
	u32 pfc;
#define DCBX_PFC_PRI_EN_BITMAP_MASK             0x000000ff
#define DCBX_PFC_PRI_EN_BITMAP_SHIFT            0
#define DCBX_PFC_PRI_EN_BITMAP_PRI_0            0x01
#define DCBX_PFC_PRI_EN_BITMAP_PRI_1            0x02
#define DCBX_PFC_PRI_EN_BITMAP_PRI_2            0x04
#define DCBX_PFC_PRI_EN_BITMAP_PRI_3            0x08
#define DCBX_PFC_PRI_EN_BITMAP_PRI_4            0x10
#define DCBX_PFC_PRI_EN_BITMAP_PRI_5            0x20
#define DCBX_PFC_PRI_EN_BITMAP_PRI_6            0x40
#define DCBX_PFC_PRI_EN_BITMAP_PRI_7            0x80

#define DCBX_PFC_FLAGS_MASK                     0x0000ff00
#define DCBX_PFC_FLAGS_SHIFT                    8
#define DCBX_PFC_CAPS_MASK                      0x00000f00
#define DCBX_PFC_CAPS_SHIFT                     8
#define DCBX_PFC_MBC_MASK                       0x00004000
#define DCBX_PFC_MBC_SHIFT                      14
#define DCBX_PFC_WILLING_MASK                   0x00008000
#define DCBX_PFC_WILLING_SHIFT                  15
#define DCBX_PFC_ENABLED_MASK                   0x00010000
#define DCBX_PFC_ENABLED_SHIFT                  16
#define DCBX_PFC_ERROR_MASK                     0x00020000
#define DCBX_PFC_ERROR_SHIFT                    17

	/* APP feature */
	struct dcbx_app_priority_feature app;
};

struct dcbx_local_params {
	u32 config;
#define DCBX_CONFIG_VERSION_MASK            0x00000003
#define DCBX_CONFIG_VERSION_SHIFT           0
#define DCBX_CONFIG_VERSION_DISABLED        0
#define DCBX_CONFIG_VERSION_IEEE            1
#define DCBX_CONFIG_VERSION_CEE             2

	u32 flags;
	struct dcbx_features features;
};

struct dcbx_mib {
	u32 prefix_seq_num;
	u32 flags;
	/*
	 * #define DCBX_CONFIG_VERSION_MASK            0x00000003
	 * #define DCBX_CONFIG_VERSION_SHIFT           0
	 * #define DCBX_CONFIG_VERSION_DISABLED        0
	 * #define DCBX_CONFIG_VERSION_IEEE            1
	 * #define DCBX_CONFIG_VERSION_CEE             2
	 */
	struct dcbx_features features;
	u32 suffix_seq_num;
};

struct lldp_system_tlvs_buffer_s {
	u16 valid;
	u16 length;
	u32 data[MAX_SYSTEM_LLDP_TLV_DATA];
};

/**************************************/
/*                                    */
/*     P U B L I C      G L O B A L   */
/*                                    */
/**************************************/
struct public_global {
	u32 max_path; /* 32bit is wasty, but this will be used often */
	u32 max_ports; /* (Global) 32bit is wasty, this will be used often */
#define MODE_1P	1 /* TBD - NEED TO THINK OF A BETTER NAME */
#define MODE_2P	2
#define MODE_3P	3
#define MODE_4P	4
	u32 debug_mb_offset;
	u32 phymod_dbg_mb_offset;
	struct couple_mode_teaming cmt;
	s32 internal_temperature;
	u32 mfw_ver;
	u32 running_bundle_id;
	s32 external_temperature;
};

/**************************************/
/*                                    */
/*     P U B L I C      P A T H       */
/*                                    */
/**************************************/

/****************************************************************************
 * Shared Memory 2 Region                                                   *
 ****************************************************************************/
/* The fw_flr_ack is actually built in the following way:                   */
/* 8 bit:  PF ack                                                           */
/* 128 bit: VF ack                                                           */
/* 8 bit:  ios_dis_ack                                                      */
/* In order to maintain endianity in the mailbox hsi, we want to keep using */
/* u32. The fw must have the VF right after the PF since this is how it     */
/* access arrays(it expects always the VF to reside after the PF, and that  */
/* makes the calculation much easier for it. )                              */
/* In order to answer both limitations, and keep the struct small, the code */
/* will abuse the structure defined here to achieve the actual partition    */
/* above                                                                    */
/****************************************************************************/
struct fw_flr_mb {
	u32 aggint;
	u32 opgen_addr;
	u32 accum_ack;		/* 0..15:PF, 16..207:VF, 256..271:IOV_DIS */
#define ACCUM_ACK_PF_BASE	0
#define ACCUM_ACK_PF_SHIFT	0

#define ACCUM_ACK_VF_BASE	8
#define ACCUM_ACK_VF_SHIFT	3

#define ACCUM_ACK_IOV_DIS_BASE	256
#define ACCUM_ACK_IOV_DIS_SHIFT	8

};

struct public_path {
	struct fw_flr_mb flr_mb;
	/*
	 * mcp_vf_disabled is set by the MCP to indicate the driver about VFs
	 * which were disabled/flred
	 */
	u32 mcp_vf_disabled[VF_MAX_STATIC / 32];	/* 0x003c */

	u32 process_kill;
	/* Reset on mcp reset, and incremented for eveny process kill event. */
#define PROCESS_KILL_COUNTER_MASK		0x0000ffff
#define PROCESS_KILL_COUNTER_SHIFT		0
#define PROCESS_KILL_GLOB_AEU_BIT_MASK		0xffff0000
#define PROCESS_KILL_GLOB_AEU_BIT_SHIFT		16
#define GLOBAL_AEU_BIT(aeu_reg_id, aeu_bit) (aeu_reg_id * 32 + aeu_bit)
};

/**************************************/
/*                                    */
/*     P U B L I C      P O R T       */
/*                                    */
/**************************************/
#define FC_NPIV_WWPN_SIZE 8
#define FC_NPIV_WWNN_SIZE 8
struct dci_npiv_settings {
	u8 npiv_wwpn[FC_NPIV_WWPN_SIZE];
	u8 npiv_wwnn[FC_NPIV_WWNN_SIZE];
};

struct dci_fc_npiv_cfg {
	/* hdr used internally by the MFW */
	u32 hdr;
	u32 num_of_npiv;
};

#define MAX_NUMBER_NPIV 64
struct dci_fc_npiv_tbl {
	struct dci_fc_npiv_cfg fc_npiv_cfg;
	struct dci_npiv_settings settings[MAX_NUMBER_NPIV];
};

/****************************************************************************
 * Driver <-> FW Mailbox                                                    *
 ****************************************************************************/

struct public_port {
	u32 validity_map;	/* 0x0 (4*2 = 0x8) */

	/* validity bits */
#define MCP_VALIDITY_PCI_CFG                    0x00100000
#define MCP_VALIDITY_MB                         0x00200000
#define MCP_VALIDITY_DEV_INFO                   0x00400000
#define MCP_VALIDITY_RESERVED                   0x00000007

	/* One licensing bit should be set */
#define MCP_VALIDITY_LIC_KEY_IN_EFFECT_MASK     0x00000038 /* yaniv - tbd  */
#define MCP_VALIDITY_LIC_MANUF_KEY_IN_EFFECT    0x00000008
#define MCP_VALIDITY_LIC_UPGRADE_KEY_IN_EFFECT  0x00000010
#define MCP_VALIDITY_LIC_NO_KEY_IN_EFFECT       0x00000020

	/* Active MFW */
#define MCP_VALIDITY_ACTIVE_MFW_UNKNOWN         0x00000000
#define MCP_VALIDITY_ACTIVE_MFW_MASK            0x000001c0
#define MCP_VALIDITY_ACTIVE_MFW_NCSI            0x00000040
#define MCP_VALIDITY_ACTIVE_MFW_NONE            0x000001c0

	u32 link_status;
#define LINK_STATUS_LINK_UP			0x00000001
#define LINK_STATUS_SPEED_AND_DUPLEX_MASK			0x0000001e
#define LINK_STATUS_SPEED_AND_DUPLEX_1000THD		(1 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_1000TFD		(2 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_10G			(3 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_20G			(4 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_40G			(5 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_50G			(6 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_100G			(7 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_25G			(8 << 1)

#define LINK_STATUS_AUTO_NEGOTIATE_ENABLED			0x00000020

#define LINK_STATUS_AUTO_NEGOTIATE_COMPLETE			0x00000040
#define LINK_STATUS_PARALLEL_DETECTION_USED			0x00000080

#define LINK_STATUS_PFC_ENABLED				0x00000100
#define LINK_STATUS_LINK_PARTNER_1000TFD_CAPABLE	0x00000200
#define LINK_STATUS_LINK_PARTNER_1000THD_CAPABLE	0x00000400
#define LINK_STATUS_LINK_PARTNER_10G_CAPABLE		0x00000800
#define LINK_STATUS_LINK_PARTNER_20G_CAPABLE		0x00001000
#define LINK_STATUS_LINK_PARTNER_40G_CAPABLE		0x00002000
#define LINK_STATUS_LINK_PARTNER_50G_CAPABLE		0x00004000
#define LINK_STATUS_LINK_PARTNER_100G_CAPABLE		0x00008000
#define LINK_STATUS_LINK_PARTNER_25G_CAPABLE		0x00010000

#define LINK_STATUS_LINK_PARTNER_FLOW_CONTROL_MASK	0x000C0000
#define LINK_STATUS_LINK_PARTNER_NOT_PAUSE_CAPABLE	(0 << 18)
#define LINK_STATUS_LINK_PARTNER_SYMMETRIC_PAUSE	(1 << 18)
#define LINK_STATUS_LINK_PARTNER_ASYMMETRIC_PAUSE	(2 << 18)
#define LINK_STATUS_LINK_PARTNER_BOTH_PAUSE			(3 << 18)

#define LINK_STATUS_SFP_TX_FAULT				0x00100000
#define LINK_STATUS_TX_FLOW_CONTROL_ENABLED			0x00200000
#define LINK_STATUS_RX_FLOW_CONTROL_ENABLED			0x00400000
#define LINK_STATUS_RX_SIGNAL_PRESENT               0x00800000
#define LINK_STATUS_MAC_LOCAL_FAULT                 0x01000000
#define LINK_STATUS_MAC_REMOTE_FAULT                0x02000000
#define LINK_STATUS_UNSUPPORTED_SPD_REQ				0x04000000

	u32 link_status1;
	u32 ext_phy_fw_version;
	u32 drv_phy_cfg_addr;	/* Points to pmm_phy_cfg (For READ-ONLY) */

	u32 port_stx;

	u32 stat_nig_timer;

	struct port_mf_cfg port_mf_config;
	struct port_stats stats;

	u32 media_type;
#define	MEDIA_UNSPECIFIED		0x0
#define	MEDIA_SFPP_10G_FIBER	0x1
#define	MEDIA_XFP_FIBER			0x2
#define	MEDIA_DA_TWINAX			0x3
#define	MEDIA_BASE_T			0x4
#define MEDIA_SFP_1G_FIBER		0x5
#define MEDIA_MODULE_FIBER		0x6
#define	MEDIA_KR				0xf0
#define	MEDIA_NOT_PRESENT		0xff

	u32 lfa_status;
#define LFA_LINK_FLAP_REASON_OFFSET		0
#define LFA_LINK_FLAP_REASON_MASK		0x000000ff
#define LFA_NO_REASON					(0 << 0)
#define LFA_LINK_DOWN					(1 << 0)
#define LFA_FORCE_INIT					(1 << 1)
#define LFA_LOOPBACK_MISMATCH				(1 << 2)
#define LFA_SPEED_MISMATCH				(1 << 3)
#define LFA_FLOW_CTRL_MISMATCH				(1 << 4)
#define LFA_ADV_SPEED_MISMATCH				(1 << 5)
#define LINK_FLAP_AVOIDANCE_COUNT_OFFSET	8
#define LINK_FLAP_AVOIDANCE_COUNT_MASK		0x0000ff00
#define LINK_FLAP_COUNT_OFFSET			16
#define LINK_FLAP_COUNT_MASK			0x00ff0000

	u32 link_change_count;

	/* LLDP params */
	struct lldp_config_params_s lldp_config_params[LLDP_MAX_LLDP_AGENTS];
	struct lldp_status_params_s lldp_status_params[LLDP_MAX_LLDP_AGENTS];
	struct lldp_system_tlvs_buffer_s system_lldp_tlvs_buf;

	/* DCBX related MIB */
	struct dcbx_local_params local_admin_dcbx_mib;
	struct dcbx_mib remote_dcbx_mib;
	struct dcbx_mib operational_dcbx_mib;

	/* FC_NPIV table offset & size in NVRAM value of 0 means not present */
	u32 fc_npiv_nvram_tbl_addr;
	u32 fc_npiv_nvram_tbl_size;
	u32 transceiver_data;
#define PMM_TRANSCEIVER_STATE_MASK		0x000000FF
#define PMM_TRANSCEIVER_STATE_SHIFT		0x00000000
#define PMM_TRANSCEIVER_STATE_UNPLUGGED		0x00000000
#define PMM_TRANSCEIVER_STATE_PRESENT		0x00000001
#define PMM_TRANSCEIVER_STATE_VALID		0x00000003
#define PMM_TRANSCEIVER_STATE_UPDATING		0x00000008
#define PMM_TRANSCEIVER_TYPE_MASK		0x0000FF00
#define PMM_TRANSCEIVER_TYPE_SHIFT		0x00000008
#define PMM_TRANSCEIVER_TYPE_NONE		0x00000000
#define PMM_TRANSCEIVER_TYPE_UNKNOWN		0x000000FF
#define PMM_TRANSCEIVER_TYPE_1G_PCC	0x01	/* 1G Passive copper cable */
#define PMM_TRANSCEIVER_TYPE_1G_ACC	0x02	/* 1G Active copper cable  */
#define PMM_TRANSCEIVER_TYPE_1G_LX				0x03
#define PMM_TRANSCEIVER_TYPE_1G_SX				0x04
#define PMM_TRANSCEIVER_TYPE_10G_SR				0x05
#define PMM_TRANSCEIVER_TYPE_10G_LR				0x06
#define PMM_TRANSCEIVER_TYPE_10G_LRM			0x07
#define PMM_TRANSCEIVER_TYPE_10G_ER				0x08
#define PMM_TRANSCEIVER_TYPE_10G_PCC	0x09	/* 10G Passive copper cable */
#define PMM_TRANSCEIVER_TYPE_10G_ACC	0x0a	/* 10G Active copper cable  */
#define PMM_TRANSCEIVER_TYPE_XLPPI				0x0b
#define PMM_TRANSCEIVER_TYPE_40G_LR4			0x0c
#define PMM_TRANSCEIVER_TYPE_40G_SR4			0x0d
#define PMM_TRANSCEIVER_TYPE_40G_CR4			0x0e
#define PMM_TRANSCEIVER_TYPE_100G_AOC	0x0f	/* Active optical cable */
#define PMM_TRANSCEIVER_TYPE_100G_SR4			0x10
#define PMM_TRANSCEIVER_TYPE_100G_LR4			0x11
#define PMM_TRANSCEIVER_TYPE_100G_ER4			0x12
#define PMM_TRANSCEIVER_TYPE_100G_ACC	0x13	/* Active copper cable */
#define PMM_TRANSCEIVER_TYPE_100G_CR4			0x14
#define PMM_TRANSCEIVER_TYPE_4x10G_SR			0x15
#define PMM_TRANSCEIVER_TYPE_25G_PCC_S	0x16
#define PMM_TRANSCEIVER_TYPE_25G_ACC_S	0x17
#define PMM_TRANSCEIVER_TYPE_25G_PCC_M	0x18
#define PMM_TRANSCEIVER_TYPE_25G_ACC_M	0x19
#define PMM_TRANSCEIVER_TYPE_25G_PCC_L	0x1a
#define PMM_TRANSCEIVER_TYPE_25G_ACC_L	0x1b
#define PMM_TRANSCEIVER_TYPE_25G_SR				0x1c
#define PMM_TRANSCEIVER_TYPE_25G_LR				0x1d
#define PMM_TRANSCEIVER_TYPE_25G_AOC			0x1e

#define PMM_TRANSCEIVER_TYPE_4x10G					0x1d
#define PMM_TRANSCEIVER_TYPE_4x25G_CR					0x1e
#define PMM_TRANSCEIVER_TYPE_MULTI_RATE_10G_40GR			0x30
#define PMM_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_CR			0x31
#define PMM_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_LR			0x32
#define PMM_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_SR			0x33
#define PMM_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_CR			0x34
#define PMM_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_LR			0x35
#define PMM_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_AOC			0x36
};

/**************************************/
/*                                    */
/*     P U B L I C      F U N C       */
/*                                    */
/**************************************/

struct public_func {
	u32 dpdk_rsvd1[2];

	/* MTU size per funciton is needed for the OV feature */
	u32 mtu_size;
	/* 9 entires for the C2S PCP map for each inner VLAN PCP + 1 default */
	/* For PCP values 0-3 use the map lower */
	/* 0xFF000000 - PCP 0, 0x00FF0000 - PCP 1,
	 * 0x0000FF00 - PCP 2, 0x000000FF PCP 3
	 */
	u32 c2s_pcp_map_lower;
	/* For PCP values 4-7 use the map upper */
	/* 0xFF000000 - PCP 4, 0x00FF0000 - PCP 5,
	 * 0x0000FF00 - PCP 6, 0x000000FF PCP 7
	 */
	u32 c2s_pcp_map_upper;

	/* For PCP default value get the MSB byte of the map default */
	u32 c2s_pcp_map_default;

	u32 reserved[4];

	/* replace old mf_cfg */
	u32 config;
	/* E/R/I/D */
	/* function 0 of each port cannot be hidden */
#define FUNC_MF_CFG_FUNC_HIDE                   0x00000001
#define FUNC_MF_CFG_PAUSE_ON_HOST_RING          0x00000002
#define FUNC_MF_CFG_PAUSE_ON_HOST_RING_SHIFT    0x00000001

#define FUNC_MF_CFG_PROTOCOL_MASK               0x000000f0
#define FUNC_MF_CFG_PROTOCOL_SHIFT              4
#define FUNC_MF_CFG_PROTOCOL_ETHERNET           0x00000000
#define FUNC_MF_CFG_PROTOCOL_MAX	        0x00000000

	/* MINBW, MAXBW */
	/* value range - 0..100, increments in 1 %  */
#define FUNC_MF_CFG_MIN_BW_MASK                 0x0000ff00
#define FUNC_MF_CFG_MIN_BW_SHIFT                8
#define FUNC_MF_CFG_MIN_BW_DEFAULT              0x00000000
#define FUNC_MF_CFG_MAX_BW_MASK                 0x00ff0000
#define FUNC_MF_CFG_MAX_BW_SHIFT                16
#define FUNC_MF_CFG_MAX_BW_DEFAULT              0x00640000

	u32 status;
#define FUNC_STATUS_VLINK_DOWN			0x00000001

	u32 mac_upper;		/* MAC */
#define FUNC_MF_CFG_UPPERMAC_MASK               0x0000ffff
#define FUNC_MF_CFG_UPPERMAC_SHIFT              0
#define FUNC_MF_CFG_UPPERMAC_DEFAULT            FUNC_MF_CFG_UPPERMAC_MASK
	u32 mac_lower;
#define FUNC_MF_CFG_LOWERMAC_DEFAULT            0xffffffff

	u32 dpdk_rsvd2[4];

	u32 ovlan_stag;		/* tags */
#define FUNC_MF_CFG_OV_STAG_MASK              0x0000ffff
#define FUNC_MF_CFG_OV_STAG_SHIFT             0
#define FUNC_MF_CFG_OV_STAG_DEFAULT           FUNC_MF_CFG_OV_STAG_MASK

	u32 pf_allocation;	/* vf per pf */

	u32 preserve_data;	/* Will be used bt CCM */

	u32 driver_last_activity_ts;

	/*
	 * drv_ack_vf_disabled is set by the PF driver to ack handled disabled
	 * VFs
	 */
	u32 drv_ack_vf_disabled[VF_MAX_STATIC / 32];	/* 0x0044 */

	u32 drv_id;
#define DRV_ID_PDA_COMP_VER_MASK	0x0000ffff
#define DRV_ID_PDA_COMP_VER_SHIFT	0

#define DRV_ID_MCP_HSI_VER_MASK		0x00ff0000
#define DRV_ID_MCP_HSI_VER_SHIFT	16
#define DRV_ID_MCP_HSI_VER_CURRENT	(1 << DRV_ID_MCP_HSI_VER_SHIFT)

#define DRV_ID_DRV_TYPE_MASK		0x7f000000
#define DRV_ID_DRV_TYPE_SHIFT		24
#define DRV_ID_DRV_TYPE_UNKNOWN		(0 << DRV_ID_DRV_TYPE_SHIFT)
#define DRV_ID_DRV_TYPE_LINUX		(1 << DRV_ID_DRV_TYPE_SHIFT)
#define DRV_ID_DRV_TYPE_WINDOWS		(2 << DRV_ID_DRV_TYPE_SHIFT)
#define DRV_ID_DRV_TYPE_DIAG		(3 << DRV_ID_DRV_TYPE_SHIFT)
#define DRV_ID_DRV_TYPE_PREBOOT		(4 << DRV_ID_DRV_TYPE_SHIFT)
#define DRV_ID_DRV_TYPE_SOLARIS		(5 << DRV_ID_DRV_TYPE_SHIFT)
#define DRV_ID_DRV_TYPE_VMWARE		(6 << DRV_ID_DRV_TYPE_SHIFT)
#define DRV_ID_DRV_TYPE_FREEBSD		(7 << DRV_ID_DRV_TYPE_SHIFT)
#define DRV_ID_DRV_TYPE_AIX		(8 << DRV_ID_DRV_TYPE_SHIFT)

#define DRV_ID_DRV_INIT_HW_MASK		0x80000000
#define DRV_ID_DRV_INIT_HW_SHIFT	31
#define DRV_ID_DRV_INIT_HW_FLAG		(1 << DRV_ID_DRV_INIT_HW_SHIFT)
};

/**************************************/
/*                                    */
/*     P U B L I C       M B          */
/*                                    */
/**************************************/
/* This is the only section that the driver can write to, and each */
/* Basically each driver request to set feature parameters,
 * will be done using a different command, which will be linked
 * to a specific data structure from the union below.
 * For huge strucuture, the common blank structure should be used.
 */

struct mcp_mac {
	u32 mac_upper;		/* Upper 16 bits are always zeroes */
	u32 mac_lower;
};

struct mcp_val64 {
	u32 lo;
	u32 hi;
};

struct mcp_file_att {
	u32 nvm_start_addr;
	u32 len;
};

#define MCP_DRV_VER_STR_SIZE 16
#define MCP_DRV_VER_STR_SIZE_DWORD (MCP_DRV_VER_STR_SIZE / sizeof(u32))
#define MCP_DRV_NVM_BUF_LEN 32
struct drv_version_stc {
	u32 version;
	u8 name[MCP_DRV_VER_STR_SIZE - 4];
};

/* statistics for ncsi */
struct lan_stats_stc {
	u64 ucast_rx_pkts;
	u64 ucast_tx_pkts;
	u32 fcs_err;
	u32 rserved;
};

struct ocbb_data_stc {
	u32 ocbb_host_addr;
	u32 ocsd_host_addr;
	u32 ocsd_req_update_interval;
};

union drv_union_data {
	u32 ver_str[MCP_DRV_VER_STR_SIZE_DWORD];	/* LOAD_REQ */
	struct mcp_mac wol_mac;	/* UNLOAD_DONE */

	struct pmm_phy_cfg drv_phy_cfg;

	struct mcp_val64 val64;	/* For PHY / AVS commands */

	u8 raw_data[MCP_DRV_NVM_BUF_LEN];

	struct mcp_file_att file_att;

	u32 ack_vf_disabled[VF_MAX_STATIC / 32];

	struct drv_version_stc drv_version;

	struct lan_stats_stc lan_stats;
	u32 dpdk_rsvd[3];
	struct ocbb_data_stc ocbb_info;

	/* ... */
};

struct public_drv_mb {
	u32 drv_mb_header;
#define DRV_MSG_CODE_MASK                       0xffff0000
#define DRV_MSG_CODE_LOAD_REQ                   0x10000000
#define DRV_MSG_CODE_LOAD_DONE                  0x11000000
#define DRV_MSG_CODE_INIT_HW                    0x12000000
#define DRV_MSG_CODE_UNLOAD_REQ		        0x20000000
#define DRV_MSG_CODE_UNLOAD_DONE                0x21000000
#define DRV_MSG_CODE_INIT_PHY			0x22000000
	/* Params - FORCE - Reinitialize the link regardless of LFA */
	/*        - DONT_CARE - Don't flap the link if up */
#define DRV_MSG_CODE_LINK_RESET			0x23000000

	/* Vitaly: LLDP commands */
#define DRV_MSG_CODE_SET_LLDP                   0x24000000
#define DRV_MSG_CODE_SET_DCBX                   0x25000000
	/* OneView feature driver HSI */
#define DRV_MSG_CODE_OV_UPDATE_CURR_CFG		0x26000000
#define DRV_MSG_CODE_OV_UPDATE_BUS_NUM		0x27000000
#define DRV_MSG_CODE_OV_UPDATE_BOOT_PROGRESS	0x28000000
#define DRV_MSG_CODE_OV_UPDATE_STORM_FW_VER	0x29000000
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE	0x31000000
#define DRV_MSG_CODE_BW_UPDATE_ACK		0x32000000
#define DRV_MSG_CODE_OV_UPDATE_MTU		0x33000000

#define DRV_MSG_CODE_NIG_DRAIN			0x30000000

#define DRV_MSG_CODE_INITIATE_FLR               0x02000000
#define DRV_MSG_CODE_VF_DISABLED_DONE           0xc0000000
#define DRV_MSG_CODE_CFG_VF_MSIX                0xc0010000
#define DRV_MSG_CODE_NVM_PUT_FILE_BEGIN		0x00010000
#define DRV_MSG_CODE_NVM_PUT_FILE_DATA		0x00020000
#define DRV_MSG_CODE_NVM_GET_FILE_ATT		0x00030000
#define DRV_MSG_CODE_NVM_READ_NVRAM		0x00050000
#define DRV_MSG_CODE_NVM_WRITE_NVRAM		0x00060000
#define DRV_MSG_CODE_NVM_DEL_FILE		0x00080000
#define DRV_MSG_CODE_MCP_RESET			0x00090000
#define DRV_MSG_CODE_SET_SECURE_MODE		0x000a0000
#define DRV_MSG_CODE_PHY_RAW_READ		0x000b0000
#define DRV_MSG_CODE_PHY_RAW_WRITE		0x000c0000
#define DRV_MSG_CODE_PHY_CORE_READ		0x000d0000
#define DRV_MSG_CODE_PHY_CORE_WRITE		0x000e0000
#define DRV_MSG_CODE_SET_VERSION		0x000f0000
#define DRV_MSG_CODE_MCP_HALT			0x00100000
#define DRV_MSG_CODE_PMD_DIAG_DUMP		0x00140000
#define DRV_MSG_CODE_PMD_DIAG_EYE		0x00150000
#define DRV_MSG_CODE_TRANSCEIVER_READ		0x00160000
#define DRV_MSG_CODE_TRANSCEIVER_WRITE		0x00170000

#define DRV_MSG_CODE_SET_VMAC                   0x00110000
#define DRV_MSG_CODE_GET_VMAC                   0x00120000
#define DRV_MSG_CODE_VMAC_TYPE_MAC              1
#define DRV_MSG_CODE_VMAC_TYPE_WWNN             2
#define DRV_MSG_CODE_VMAC_TYPE_WWPN             3

#define DRV_MSG_CODE_GET_STATS                  0x00130000
#define DRV_MSG_CODE_STATS_TYPE_LAN             1

#define DRV_MSG_CODE_OCBB_DATA			0x00180000
#define DRV_MSG_CODE_SET_BW			0x00190000
#define DRV_MSG_CODE_MASK_PARITIES		0x001a0000
#define DRV_MSG_CODE_INDUCE_FAILURE		0x001b0000
#define DRV_MSG_FAN_FAILURE_TYPE		(1 << 0)
#define DRV_MSG_TEMPERATURE_FAILURE_TYPE	(1 << 1)

#define DRV_MSG_CODE_GPIO_READ			0x001c0000
#define DRV_MSG_CODE_GPIO_WRITE			0x001d0000

#define DRV_MSG_CODE_SET_LED_MODE		0x00200000
#define DRV_MSG_CODE_EMPTY_MB			0x00220000

#define DRV_MSG_SEQ_NUMBER_MASK                 0x0000ffff

	u32 drv_mb_param;
	/* UNLOAD_REQ params */
#define DRV_MB_PARAM_UNLOAD_WOL_UNKNOWN         0x00000000
#define DRV_MB_PARAM_UNLOAD_WOL_MCP		0x00000001
#define DRV_MB_PARAM_UNLOAD_WOL_DISABLED        0x00000002
#define DRV_MB_PARAM_UNLOAD_WOL_ENABLED         0x00000003

	/* UNLOAD_DONE_params */
#define DRV_MB_PARAM_UNLOAD_NON_D3_POWER        0x00000001

	/* INIT_PHY params */
#define DRV_MB_PARAM_INIT_PHY_FORCE		0x00000001
#define DRV_MB_PARAM_INIT_PHY_DONT_CARE		0x00000002

	/* LLDP / DCBX params */
#define DRV_MB_PARAM_LLDP_SEND_MASK		0x00000001
#define DRV_MB_PARAM_LLDP_SEND_SHIFT		0
#define DRV_MB_PARAM_LLDP_AGENT_MASK		0x00000006
#define DRV_MB_PARAM_LLDP_AGENT_SHIFT		1
#define DRV_MB_PARAM_DCBX_NOTIFY_MASK		0x00000008
#define DRV_MB_PARAM_DCBX_NOTIFY_SHIFT		3

#define DRV_MB_PARAM_NIG_DRAIN_PERIOD_MS_MASK	0x000000FF
#define DRV_MB_PARAM_NIG_DRAIN_PERIOD_MS_SHIFT	0

#define DRV_MB_PARAM_NVM_PUT_FILE_BEGIN_MFW	0x1
#define DRV_MB_PARAM_NVM_PUT_FILE_BEGIN_IMAGE	0x2

#define DRV_MB_PARAM_NVM_OFFSET_SHIFT		0
#define DRV_MB_PARAM_NVM_OFFSET_MASK		0x00FFFFFF
#define DRV_MB_PARAM_NVM_LEN_SHIFT		24
#define DRV_MB_PARAM_NVM_LEN_MASK		0xFF000000

#define DRV_MB_PARAM_PHY_ADDR_SHIFT		0
#define DRV_MB_PARAM_PHY_ADDR_MASK		0x1FF0FFFF
#define DRV_MB_PARAM_PHY_LANE_SHIFT		16
#define DRV_MB_PARAM_PHY_LANE_MASK		0x000F0000
#define DRV_MB_PARAM_PHY_SELECT_PORT_SHIFT	29
#define DRV_MB_PARAM_PHY_SELECT_PORT_MASK	0x20000000
#define DRV_MB_PARAM_PHY_PORT_SHIFT		30
#define DRV_MB_PARAM_PHY_PORT_MASK		0xc0000000

#define DRV_MB_PARAM_PHYMOD_LANE_SHIFT		0
#define DRV_MB_PARAM_PHYMOD_LANE_MASK		0x000000FF
#define DRV_MB_PARAM_PHYMOD_SIZE_SHIFT		8
#define DRV_MB_PARAM_PHYMOD_SIZE_MASK		0x000FFF00
	/* configure vf MSIX params */
#define DRV_MB_PARAM_CFG_VF_MSIX_VF_ID_SHIFT	0
#define DRV_MB_PARAM_CFG_VF_MSIX_VF_ID_MASK	0x000000FF
#define DRV_MB_PARAM_CFG_VF_MSIX_SB_NUM_SHIFT	8
#define DRV_MB_PARAM_CFG_VF_MSIX_SB_NUM_MASK	0x0000FF00

	/* OneView configuration parametres */
#define DRV_MB_PARAM_OV_CURR_CFG_SHIFT		0
#define DRV_MB_PARAM_OV_CURR_CFG_MASK		0x0000000F
#define DRV_MB_PARAM_OV_CURR_CFG_NONE		0
#define DRV_MB_PARAM_OV_CURR_CFG_OS			1
#define DRV_MB_PARAM_OV_CURR_CFG_VENDOR_SPEC	2
#define DRV_MB_PARAM_OV_CURR_CFG_OTHER		3
#define DRV_MB_PARAM_OV_CURR_CFG_VC_CLP		4
#define DRV_MB_PARAM_OV_CURR_CFG_CNU		5
#define DRV_MB_PARAM_OV_CURR_CFG_DCI		6
#define DRV_MB_PARAM_OV_CURR_CFG_HII		7

#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_SHIFT			0
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_MASK			0x000000FF
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_NONE			(1 << 0)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_TRARGET_FOUND			(1 << 2)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_LOGGED_INTO_TGT		(1 << 4)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_IMG_DOWNLOADED			(1 << 5)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_OS_HANDOFF			(1 << 6)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_COMPLETED				0

#define DRV_MB_PARAM_OV_PCI_BUS_NUM_SHIFT		0
#define DRV_MB_PARAM_OV_PCI_BUS_NUM_MASK		0x000000FF

#define DRV_MB_PARAM_OV_STORM_FW_VER_SHIFT		0
#define DRV_MB_PARAM_OV_STORM_FW_VER_MASK			0xFFFFFFFF
#define DRV_MB_PARAM_OV_STORM_FW_VER_MAJOR_MASK		0xFF000000
#define DRV_MB_PARAM_OV_STORM_FW_VER_MINOR_MASK		0x00FF0000
#define DRV_MB_PARAM_OV_STORM_FW_VER_BUILD_MASK		0x0000FF00
#define DRV_MB_PARAM_OV_STORM_FW_VER_DROP_MASK		0x000000FF

#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_SHIFT		0
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_MASK		0xF
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_UNKNOWN		0x1
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_NOT_LOADED	0x2
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_LOADING		0x3
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_DISABLED	0x4
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_ACTIVE		0x5

#define DRV_MB_PARAM_OV_MTU_SIZE_SHIFT		0
#define DRV_MB_PARAM_OV_MTU_SIZE_MASK		0xFFFFFFFF

#define DRV_MB_PARAM_SET_LED_MODE_OPER		0x0
#define DRV_MB_PARAM_SET_LED_MODE_ON		0x1
#define DRV_MB_PARAM_SET_LED_MODE_OFF		0x2

#define DRV_MB_PARAM_TRANSCEIVER_PORT_SHIFT		0
#define DRV_MB_PARAM_TRANSCEIVER_PORT_MASK		0x00000003
#define DRV_MB_PARAM_TRANSCEIVER_SIZE_SHIFT		2
#define DRV_MB_PARAM_TRANSCEIVER_SIZE_MASK		0x000000FC
#define DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS_SHIFT	8
#define DRV_MB_PARAM_TRANSCEIVER_I2C_ADDRESS_MASK	0x0000FF00
#define DRV_MB_PARAM_TRANSCEIVER_OFFSET_SHIFT		16
#define DRV_MB_PARAM_TRANSCEIVER_OFFSET_MASK		0xFFFF0000

#define DRV_MB_PARAM_GPIO_NUMBER_SHIFT		0
#define DRV_MB_PARAM_GPIO_NUMBER_MASK		0x0000FFFF
#define DRV_MB_PARAM_GPIO_VALUE_SHIFT		16
#define DRV_MB_PARAM_GPIO_VALUE_MASK		0xFFFF0000

	u32 fw_mb_header;
#define FW_MSG_CODE_MASK                        0xffff0000
#define FW_MSG_CODE_DRV_LOAD_ENGINE		0x10100000
#define FW_MSG_CODE_DRV_LOAD_PORT               0x10110000
#define FW_MSG_CODE_DRV_LOAD_FUNCTION           0x10120000
#define FW_MSG_CODE_DRV_LOAD_REFUSED_PDA        0x10200000
#define FW_MSG_CODE_DRV_LOAD_REFUSED_HSI        0x10210000
#define FW_MSG_CODE_DRV_LOAD_REFUSED_DIAG       0x10220000
#define FW_MSG_CODE_DRV_LOAD_DONE               0x11100000
#define FW_MSG_CODE_DRV_UNLOAD_ENGINE           0x20110000
#define FW_MSG_CODE_DRV_UNLOAD_PORT             0x20120000
#define FW_MSG_CODE_DRV_UNLOAD_FUNCTION         0x20130000
#define FW_MSG_CODE_DRV_UNLOAD_DONE             0x21100000
#define FW_MSG_CODE_INIT_PHY_DONE		0x21200000
#define FW_MSG_CODE_INIT_PHY_ERR_INVALID_ARGS	0x21300000
#define FW_MSG_CODE_LINK_RESET_DONE		0x23000000
#define FW_MSG_CODE_SET_LLDP_DONE               0x24000000
#define FW_MSG_CODE_SET_LLDP_UNSUPPORTED_AGENT  0x24010000
#define FW_MSG_CODE_SET_DCBX_DONE               0x25000000
#define FW_MSG_CODE_UPDATE_CURR_CFG_DONE        0x26000000
#define FW_MSG_CODE_UPDATE_BUS_NUM_DONE         0x27000000
#define FW_MSG_CODE_UPDATE_BOOT_PROGRESS_DONE   0x28000000
#define FW_MSG_CODE_UPDATE_STORM_FW_VER_DONE    0x29000000
#define FW_MSG_CODE_UPDATE_DRIVER_STATE_DONE    0x31000000
#define FW_MSG_CODE_DRV_MSG_CODE_BW_UPDATE_DONE 0x32000000
#define FW_MSG_CODE_DRV_MSG_CODE_MTU_SIZE_DONE  0x33000000
#define FW_MSG_CODE_NIG_DRAIN_DONE              0x30000000
#define FW_MSG_CODE_VF_DISABLED_DONE            0xb0000000
#define FW_MSG_CODE_DRV_CFG_VF_MSIX_DONE        0xb0010000
#define FW_MSG_CODE_FLR_ACK                     0x02000000
#define FW_MSG_CODE_FLR_NACK                    0x02100000
#define FW_MSG_CODE_SET_DRIVER_DONE		0x02200000
#define FW_MSG_CODE_SET_VMAC_SUCCESS            0x02300000
#define FW_MSG_CODE_SET_VMAC_FAIL               0x02400000

#define FW_MSG_CODE_NVM_OK			0x00010000
#define FW_MSG_CODE_NVM_INVALID_MODE		0x00020000
#define FW_MSG_CODE_NVM_PREV_CMD_WAS_NOT_FINISHED	0x00030000
#define FW_MSG_CODE_NVM_FAILED_TO_ALLOCATE_PAGE	0x00040000
#define FW_MSG_CODE_NVM_INVALID_DIR_FOUND	0x00050000
#define FW_MSG_CODE_NVM_PAGE_NOT_FOUND		0x00060000
#define FW_MSG_CODE_NVM_FAILED_PARSING_BNDLE_HEADER 0x00070000
#define FW_MSG_CODE_NVM_FAILED_PARSING_IMAGE_HEADER 0x00080000
#define FW_MSG_CODE_NVM_PARSING_OUT_OF_SYNC	0x00090000
#define FW_MSG_CODE_NVM_FAILED_UPDATING_DIR	0x000a0000
#define FW_MSG_CODE_NVM_FAILED_TO_FREE_PAGE	0x000b0000
#define FW_MSG_CODE_NVM_FILE_NOT_FOUND		0x000c0000
#define FW_MSG_CODE_NVM_OPERATION_FAILED	0x000d0000
#define FW_MSG_CODE_NVM_FAILED_UNALIGNED	0x000e0000
#define FW_MSG_CODE_NVM_BAD_OFFSET		0x000f0000
#define FW_MSG_CODE_NVM_BAD_SIGNATURE		0x00100000
#define FW_MSG_CODE_NVM_FILE_READ_ONLY		0x00200000
#define FW_MSG_CODE_NVM_UNKNOWN_FILE		0x00300000
#define FW_MSG_CODE_NVM_PUT_FILE_FINISH_OK	0x00400000
#define FW_MSG_CODE_MCP_RESET_REJECT		0x00600000
#define FW_MSG_CODE_PHY_OK			0x00110000
#define FW_MSG_CODE_PHY_ERROR			0x00120000
#define FW_MSG_CODE_SET_SECURE_MODE_ERROR	0x00130000
#define FW_MSG_CODE_SET_SECURE_MODE_OK		0x00140000
#define FW_MSG_MODE_PHY_PRIVILEGE_ERROR		0x00150000
#define FW_MSG_CODE_OK				0x00160000
#define FW_MSG_CODE_LED_MODE_INVALID		0x00170000
#define FW_MSG_CODE_PHY_DIAG_OK           0x00160000
#define FW_MSG_CODE_PHY_DIAG_ERROR        0x00170000
#define FW_MSG_CODE_INIT_HW_FAILED_TO_ALLOCATE_PAGE	0x00040000
#define FW_MSG_CODE_INIT_HW_FAILED_BAD_STATE    0x00170000
#define FW_MSG_CODE_INIT_HW_FAILED_TO_SET_WINDOW 0x000d0000
#define FW_MSG_CODE_INIT_HW_FAILED_NO_IMAGE	0x000c0000
#define FW_MSG_CODE_INIT_HW_FAILED_VERSION_MISMATCH	0x00100000
#define FW_MSG_CODE_TRANSCEIVER_DIAG_OK           0x00160000
#define FW_MSG_CODE_TRANSCEIVER_DIAG_ERROR        0x00170000
#define FW_MSG_CODE_TRANSCEIVER_NOT_PRESENT		0x00020000
#define FW_MSG_CODE_TRANSCEIVER_BAD_BUFFER_SIZE		0x000f0000
#define FW_MSG_CODE_GPIO_OK           0x00160000
#define FW_MSG_CODE_GPIO_DIRECTION_ERR        0x00170000
#define FW_MSG_CODE_GPIO_CTRL_ERR		0x00020000
#define FW_MSG_CODE_GPIO_INVALID		0x000f0000
#define FW_MSG_CODE_GPIO_INVALID_VALUE	0x00050000

#define FW_MSG_SEQ_NUMBER_MASK                  0x0000ffff

	u32 fw_mb_param;

	u32 drv_pulse_mb;
#define DRV_PULSE_SEQ_MASK                      0x00007fff
#define DRV_PULSE_SYSTEM_TIME_MASK              0xffff0000
	/*
	 * The system time is in the format of
	 * (year-2001)*12*32 + month*32 + day.
	 */
#define DRV_PULSE_ALWAYS_ALIVE                  0x00008000
	/*
	 * Indicate to the firmware not to go into the
	 * OS-absent when it is not getting driver pulse.
	 * This is used for debugging as well for PXE(MBA).
	 */

	u32 mcp_pulse_mb;
#define MCP_PULSE_SEQ_MASK                      0x00007fff
#define MCP_PULSE_ALWAYS_ALIVE                  0x00008000
	/* Indicates to the driver not to assert due to lack
	 * of MCP response
	 */
#define MCP_EVENT_MASK                          0xffff0000
#define MCP_EVENT_OTHER_DRIVER_RESET_REQ        0x00010000

	union drv_union_data union_data;
};

/* MFW - DRV MB */
/**********************************************************************
 * Description
 *   Incremental Aggregative
 *   8-bit MFW counter per message
 *   8-bit ack-counter per message
 * Capabilities
 *   Provides up to 256 aggregative message per type
 *   Provides 4 message types in dword
 *   Message type pointers to byte offset
 *   Backward Compatibility by using sizeof for the counters.
 *   No lock requires for 32bit messages
 * Limitations:
 * In case of messages greater than 32bit, a dedicated mechanism(e.g lock)
 * is required to prevent data corruption.
 **********************************************************************/
enum MFW_DRV_MSG_TYPE {
	MFW_DRV_MSG_LINK_CHANGE,
	MFW_DRV_MSG_FLR_FW_ACK_FAILED,
	MFW_DRV_MSG_VF_DISABLED,
	MFW_DRV_MSG_LLDP_DATA_UPDATED,
	MFW_DRV_MSG_DCBX_REMOTE_MIB_UPDATED,
	MFW_DRV_MSG_DCBX_OPERATIONAL_MIB_UPDATED,
	MFW_DRV_MSG_ERROR_RECOVERY,
	MFW_DRV_MSG_BW_UPDATE,
	MFW_DRV_MSG_S_TAG_UPDATE,
	MFW_DRV_MSG_GET_LAN_STATS,
	MFW_DRV_MSG_GET_FCOE_STATS,
	MFW_DRV_MSG_GET_ISCSI_STATS,
	MFW_DRV_MSG_GET_RDMA_STATS,
	MFW_DRV_MSG_FAILURE_DETECTED,
	MFW_DRV_MSG_TRANSCEIVER_STATE_CHANGE,
	MFW_DRV_MSG_MAX
};

#define MFW_DRV_MSG_MAX_DWORDS(msgs)	(((msgs - 1) >> 2) + 1)
#define MFW_DRV_MSG_DWORD(msg_id)	(msg_id >> 2)
#define MFW_DRV_MSG_OFFSET(msg_id)	((msg_id & 0x3) << 3)
#define MFW_DRV_MSG_MASK(msg_id)	(0xff << MFW_DRV_MSG_OFFSET(msg_id))

#ifdef BIG_ENDIAN		/* Like MFW */
#define DRV_ACK_MSG(msg_p, msg_id) \
((u8)((u8 *)msg_p)[msg_id]++;)
#else
#define DRV_ACK_MSG(msg_p, msg_id) \
((u8)((u8 *)msg_p)[((msg_id & ~3) | ((~msg_id) & 3))]++;)
#endif

#define MFW_DRV_UPDATE(shmem_func, msg_id) \
((u8)((u8 *)(MFW_MB_P(shmem_func)->msg))[msg_id]++;)

struct public_mfw_mb {
	u32 sup_msgs;		/* Assigend with MFW_DRV_MSG_MAX */
	u32 msg[MFW_DRV_MSG_MAX_DWORDS(MFW_DRV_MSG_MAX)];
	u32 ack[MFW_DRV_MSG_MAX_DWORDS(MFW_DRV_MSG_MAX)];
};

/**************************************/
/*                                    */
/*     P U B L I C       D A T A      */
/*                                    */
/**************************************/
enum public_sections {
	PUBLIC_DRV_MB,		/* Points to the first drv_mb of path0 */
	PUBLIC_MFW_MB,		/* Points to the first mfw_mb of path0 */
	PUBLIC_GLOBAL,
	PUBLIC_PATH,
	PUBLIC_PORT,
	PUBLIC_FUNC,
	PUBLIC_MAX_SECTIONS
};

struct drv_ver_info_stc {
	u32 ver;
	u8 name[32];
};

/* Runtime data needs about 1/2K. We use 2K to be on the safe side.
 * Please make sure data does not exceed this size.
 */
#define NUM_RUNTIME_DWORDS 16
struct drv_init_hw_stc {
	u32 init_hw_bitmask[NUM_RUNTIME_DWORDS];
	u32 init_hw_data[NUM_RUNTIME_DWORDS * 32];
};

struct mcp_public_data {
	/* The sections fields is an array */
	u32 num_sections;
	offsize_t sections[PUBLIC_MAX_SECTIONS];
	struct public_drv_mb drv_mb[MCP_GLOB_FUNC_MAX];
	struct public_mfw_mb mfw_mb[MCP_GLOB_FUNC_MAX];
	struct public_global global;
	struct public_path path[MCP_GLOB_PATH_MAX];
	struct public_port port[MCP_GLOB_PORT_MAX];
	struct public_func func[MCP_GLOB_FUNC_MAX];
};

#define I2C_TRANSCEIVER_ADDR	0xa0
#define MAX_I2C_TRANSACTION_SIZE	16
#define MAX_I2C_TRANSCEIVER_PAGE_SIZE	256

#endif /* MCP_PUBLIC_H */
