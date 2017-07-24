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

typedef u32 offsize_t;      /* In DWORDS !!! */
/* Offset from the beginning of the MCP scratchpad */
#define OFFSIZE_OFFSET_SHIFT	0
#define OFFSIZE_OFFSET_MASK	0x0000ffff
/* Size of specific element (not the whole array if any) */
#define OFFSIZE_SIZE_SHIFT	16
#define OFFSIZE_SIZE_MASK	0xffff0000

/* SECTION_OFFSET is calculating the offset in bytes out of offsize */
#define SECTION_OFFSET(_offsize)	\
	((((_offsize & OFFSIZE_OFFSET_MASK) >> OFFSIZE_OFFSET_SHIFT) << 2))

/* SECTION_SIZE is calculating the size in bytes out of offsize */
#define SECTION_SIZE(_offsize)		\
	(((_offsize & OFFSIZE_SIZE_MASK) >> OFFSIZE_SIZE_SHIFT) << 2)

/* SECTION_ADDR returns the GRC addr of a section, given offsize and index
 * within section
 */
#define SECTION_ADDR(_offsize, idx)	\
	(MCP_REG_SCRATCH +		\
	 SECTION_OFFSET(_offsize) + (SECTION_SIZE(_offsize) * idx))

/* SECTION_OFFSIZE_ADDR returns the GRC addr to the offsize address. Use
 * offsetof, since the OFFSETUP collide with the firmware definition
 */
#define SECTION_OFFSIZE_ADDR(_pub_base, _section) \
	(_pub_base + offsetof(struct mcp_public_data, sections[_section]))
/* PHY configuration */
struct eth_phy_cfg {
/* 0 = autoneg, 1000/10000/20000/25000/40000/50000/100000 */
	u32 speed;
#define ETH_SPEED_AUTONEG   0
#define ETH_SPEED_SMARTLINQ  0x8

	u32 pause;      /* bitmask */
#define ETH_PAUSE_NONE		0x0
#define ETH_PAUSE_AUTONEG	0x1
#define ETH_PAUSE_RX		0x2
#define ETH_PAUSE_TX		0x4

	u32 adv_speed;      /* Default should be the speed_cap_mask */
	u32 loopback_mode;
#define ETH_LOOPBACK_NONE		 (0)
/* Serdes loopback. In AH, it refers to Near End */
#define ETH_LOOPBACK_INT_PHY		 (1)
#define ETH_LOOPBACK_EXT_PHY		 (2) /* External PHY Loopback */
/* External Loopback (Require loopback plug) */
#define ETH_LOOPBACK_EXT		 (3)
#define ETH_LOOPBACK_MAC		 (4) /* MAC Loopback - not supported */
#define ETH_LOOPBACK_CNIG_AH_ONLY_0123	 (5) /* Port to itself */
#define ETH_LOOPBACK_CNIG_AH_ONLY_2301	 (6) /* Port to Port */
#define ETH_LOOPBACK_PCS_AH_ONLY	 (7) /* PCS loopback (TX to RX) */
/* Loop RX packet from PCS to TX */
#define ETH_LOOPBACK_REVERSE_MAC_AH_ONLY (8)
/* Remote Serdes Loopback (RX to TX) */
#define ETH_LOOPBACK_INT_PHY_FEA_AH_ONLY (9)

	/* Used to configure the EEE Tx LPI timer, has several modes of
	 * operation, according to bits 29:28
	 * 2'b00: Timer will be configured by nvram, output will be the value
	 *        from nvram.
	 * 2'b01: Timer will be configured by nvram, output will be in
	 *        16xmicroseconds.
	 * 2'b10: bits 1:0 contain an nvram value which will be used instead
	 *        of the one located in the nvram. Output will be that value.
	 * 2'b11: bits 19:0 contain the idle timer in microseconds; output
	 *        will be in 16xmicroseconds.
	 * Bits 31:30 should be 2'b11 in order for EEE to be enabled.
	 */
	u32 eee_mode;
#define EEE_MODE_TIMER_USEC_MASK	(0x000fffff)
#define EEE_MODE_TIMER_USEC_OFFSET	(0)
#define EEE_MODE_TIMER_USEC_BALANCED_TIME	(0xa00)
#define EEE_MODE_TIMER_USEC_AGGRESSIVE_TIME	(0x100)
#define EEE_MODE_TIMER_USEC_LATENCY_TIME	(0x6000)
/* Set by the driver to request status timer will be in microseconds and and not
 * in EEE policy definition
 */
#define EEE_MODE_OUTPUT_TIME		(1 << 28)
/* Set by the driver to override default nvm timer */
#define EEE_MODE_OVERRIDE_NVRAM		(1 << 29)
#define EEE_MODE_ENABLE_LPI		(1 << 30) /* Set when */
#define EEE_MODE_ADV_LPI		(1 << 31) /* Set when EEE is enabled */
};

struct port_mf_cfg {
	u32 dynamic_cfg;    /* device control channel */
#define PORT_MF_CFG_OV_TAG_MASK              0x0000ffff
#define PORT_MF_CFG_OV_TAG_SHIFT             0
#define PORT_MF_CFG_OV_TAG_DEFAULT         PORT_MF_CFG_OV_TAG_MASK

	u32 reserved[1];
};

/* DO NOT add new fields in the middle
 * MUST be synced with struct pmm_stats_map
 */
struct eth_stats {
	u64 r64;        /* 0x00 (Offset 0x00 ) RX 64-byte frame counter*/
	u64 r127; /* 0x01 (Offset 0x08 ) RX 65 to 127 byte frame counter*/
	u64 r255; /* 0x02 (Offset 0x10 ) RX 128 to 255 byte frame counter*/
	u64 r511; /* 0x03 (Offset 0x18 ) RX 256 to 511 byte frame counter*/
	u64 r1023; /* 0x04 (Offset 0x20 ) RX 512 to 1023 byte frame counter*/
/* 0x05 (Offset 0x28 ) RX 1024 to 1518 byte frame counter */
	u64 r1518;
	union {
		struct { /* bb */
/* 0x06 (Offset 0x30 ) RX 1519 to 1522 byte VLAN-tagged frame counter */
			u64 r1522;
/* 0x07 (Offset 0x38 ) RX 1519 to 2047 byte frame counter*/
			u64 r2047;
/* 0x08 (Offset 0x40 ) RX 2048 to 4095 byte frame counter*/
			u64 r4095;
/* 0x09 (Offset 0x48 ) RX 4096 to 9216 byte frame counter*/
			u64 r9216;
/* 0x0A (Offset 0x50 ) RX 9217 to 16383 byte frame counter */
			u64 r16383;
		} bb0;
		struct { /* ah */
			u64 unused1;
/* 0x07 (Offset 0x38 ) RX 1519 to max byte frame counter*/
			u64 r1519_to_max;
			u64 unused2;
			u64 unused3;
			u64 unused4;
		} ah0;
	} u0;
	u64 rfcs;       /* 0x0F (Offset 0x58 ) RX FCS error frame counter*/
	u64 rxcf;       /* 0x10 (Offset 0x60 ) RX control frame counter*/
	u64 rxpf;       /* 0x11 (Offset 0x68 ) RX pause frame counter*/
	u64 rxpp;       /* 0x12 (Offset 0x70 ) RX PFC frame counter*/
	u64 raln;       /* 0x16 (Offset 0x78 ) RX alignment error counter*/
	u64 rfcr;       /* 0x19 (Offset 0x80 ) RX false carrier counter */
	u64 rovr;       /* 0x1A (Offset 0x88 ) RX oversized frame counter*/
	u64 rjbr;       /* 0x1B (Offset 0x90 ) RX jabber frame counter */
	u64 rund;       /* 0x34 (Offset 0x98 ) RX undersized frame counter */
	u64 rfrg;       /* 0x35 (Offset 0xa0 ) RX fragment counter */
	u64 t64;        /* 0x40 (Offset 0xa8 ) TX 64-byte frame counter */
	u64 t127; /* 0x41 (Offset 0xb0 ) TX 65 to 127 byte frame counter */
	u64 t255; /* 0x42 (Offset 0xb8 ) TX 128 to 255 byte frame counter*/
	u64 t511; /* 0x43 (Offset 0xc0 ) TX 256 to 511 byte frame counter*/
	u64 t1023; /* 0x44 (Offset 0xc8 ) TX 512 to 1023 byte frame counter*/
/* 0x45 (Offset 0xd0 ) TX 1024 to 1518 byte frame counter */
	u64 t1518;
	union {
		struct { /* bb */
/* 0x47 (Offset 0xd8 ) TX 1519 to 2047 byte frame counter */
			u64 t2047;
/* 0x48 (Offset 0xe0 ) TX 2048 to 4095 byte frame counter */
			u64 t4095;
/* 0x49 (Offset 0xe8 ) TX 4096 to 9216 byte frame counter */
			u64 t9216;
/* 0x4A (Offset 0xf0 ) TX 9217 to 16383 byte frame counter */
			u64 t16383;
		} bb1;
		struct { /* ah */
/* 0x47 (Offset 0xd8 ) TX 1519 to max byte frame counter */
			u64 t1519_to_max;
			u64 unused6;
			u64 unused7;
			u64 unused8;
		} ah1;
	} u1;
	u64 txpf;       /* 0x50 (Offset 0xf8 ) TX pause frame counter */
	u64 txpp;       /* 0x51 (Offset 0x100) TX PFC frame counter */
/* 0x6C (Offset 0x108) Transmit Logical Type LLFC message counter */
	union {
		struct { /* bb */
/* 0x6C (Offset 0x108) Transmit Logical Type LLFC message counter */
			u64 tlpiec;
/* 0x6E (Offset 0x110) Transmit Total Collision Counter */
			u64 tncl;
		} bb2;
		struct { /* ah */
			u64 unused9;
			u64 unused10;
		} ah2;
	} u2;
	u64 rbyte;      /* 0x3d (Offset 0x118) RX byte counter */
	u64 rxuca;      /* 0x0c (Offset 0x120) RX UC frame counter */
	u64 rxmca;      /* 0x0d (Offset 0x128) RX MC frame counter */
	u64 rxbca;      /* 0x0e (Offset 0x130) RX BC frame counter */
/* 0x22 (Offset 0x138) RX good frame (good CRC, not oversized, no ERROR) */
	u64 rxpok;
	u64 tbyte;      /* 0x6f (Offset 0x140) TX byte counter */
	u64 txuca;      /* 0x4d (Offset 0x148) TX UC frame counter */
	u64 txmca;      /* 0x4e (Offset 0x150) TX MC frame counter */
	u64 txbca;      /* 0x4f (Offset 0x158) TX BC frame counter */
	u64 txcf;       /* 0x54 (Offset 0x160) TX control frame counter */
/* HSI - Cannot add more stats to this struct. If needed, then need to open new
 * struct
 */

};

struct brb_stats {
	u64 brb_truncate[8];
	u64 brb_discard[8];
};

struct port_stats {
	struct brb_stats brb;
	struct eth_stats eth;
};

/*----+------------------------------------------------------------------------
 * C  | Number and | Ports in| Ports in|2 PHY-s |# of ports|# of engines
 * h  | rate of    | team #1 | team #2 |are used|per path  | (paths)
 * i  | physical   |         |         |        |          | enabled
 * p  | ports      |         |         |        |          |
 *====+============+=========+=========+========+==========+===================
 * BB | 1x100G     | This is special mode, where there are actually 2 HW func
 * BB | 2x10/20Gbps| 0,1     | NA      |  No    | 1        | 1
 * BB | 2x40 Gbps  | 0,1     | NA      |  Yes   | 1        | 1
 * BB | 2x50Gbps   | 0,1     | NA      |  No    | 1        | 1
 * BB | 4x10Gbps   | 0,2     | 1,3     |  No    | 1/2      | 1,2 (2 is optional)
 * BB | 4x10Gbps   | 0,1     | 2,3     |  No    | 1/2      | 1,2 (2 is optional)
 * BB | 4x10Gbps   | 0,3     | 1,2     |  No    | 1/2      | 1,2 (2 is optional)
 * BB | 4x10Gbps   | 0,1,2,3 | NA      |  No    | 1        | 1
 * AH | 2x10/20Gbps| 0,1     | NA      |  NA    | 1        | NA
 * AH | 4x10Gbps   | 0,1     | 2,3     |  NA    | 2        | NA
 * AH | 4x10Gbps   | 0,2     | 1,3     |  NA    | 2        | NA
 * AH | 4x10Gbps   | 0,3     | 1,2     |  NA    | 2        | NA
 * AH | 4x10Gbps   | 0,1,2,3 | NA      |  NA    | 1        | NA
 *====+============+=========+=========+========+==========+===================
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
	u32 status; /* TBD */
	/* Holds remote Chassis ID TLV header, subtype and 9B of payload. */
	u32 peer_chassis_id[LLDP_CHASSIS_ID_STAT_LEN];
	/* Holds remote Port ID TLV header, subtype and 9B of payload. */
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
#define DCBX_OOO_TC_MASK                        0x00000f00
#define DCBX_OOO_TC_SHIFT                       8
/* Entries in tc table are orginized that the left most is pri 0, right most is
 * prio 7
 */

	u32  pri_tc_tbl[1];
/* Fixed TCP OOO TC usage is deprecated and used only for driver backward
 * compatibility
 */
#define DCBX_TCP_OOO_TC				(4)
#define DCBX_TCP_OOO_K2_4PORT_TC		(3)

#define NIG_ETS_ISCSI_OOO_CLIENT_OFFSET		(DCBX_TCP_OOO_TC + 1)
#define DCBX_CEE_STRICT_PRIORITY		0xf
/* Entries in tc table are orginized that the left most is pri 0, right most is
 * prio 7
 */

	u32  tc_bw_tbl[2];
/* Entries in tc table are orginized that the left most is pri 0, right most is
 * prio 7
 */

	u32  tc_tsa_tbl[2];
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
#define DCBX_APP_SF_IEEE_MASK       0x0000f000
#define DCBX_APP_SF_IEEE_SHIFT      12
#define DCBX_APP_SF_IEEE_RESERVED   0
#define DCBX_APP_SF_IEEE_ETHTYPE    1
#define DCBX_APP_SF_IEEE_TCP_PORT   2
#define DCBX_APP_SF_IEEE_UDP_PORT   3
#define DCBX_APP_SF_IEEE_TCP_UDP_PORT 4

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
	#define DCBX_APP_DEFAULT_PRI_MASK       0x00000f00
	#define DCBX_APP_DEFAULT_PRI_SHIFT      8
	*/
#define DCBX_APP_MAX_TCS_MASK           0x0000f000
#define DCBX_APP_MAX_TCS_SHIFT          12
#define DCBX_APP_NUM_ENTRIES_MASK       0x00ff0000
#define DCBX_APP_NUM_ENTRIES_SHIFT      16
	struct dcbx_app_priority_entry  app_pri_tbl[DCBX_MAX_APP_PROTOCOL];
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
#define DCBX_CONFIG_VERSION_MASK            0x00000007
#define DCBX_CONFIG_VERSION_SHIFT           0
#define DCBX_CONFIG_VERSION_DISABLED        0
#define DCBX_CONFIG_VERSION_IEEE            1
#define DCBX_CONFIG_VERSION_CEE             2
#define DCBX_CONFIG_VERSION_STATIC          4

	u32 flags;
	struct dcbx_features features;
};

struct dcbx_mib {
	u32 prefix_seq_num;
	u32 flags;
	/*
	#define DCBX_CONFIG_VERSION_MASK            0x00000007
	#define DCBX_CONFIG_VERSION_SHIFT           0
	#define DCBX_CONFIG_VERSION_DISABLED        0
	#define DCBX_CONFIG_VERSION_IEEE            1
	#define DCBX_CONFIG_VERSION_CEE             2
	#define DCBX_CONFIG_VERSION_STATIC          4
	*/
	struct dcbx_features features;
	u32 suffix_seq_num;
};

struct lldp_system_tlvs_buffer_s {
	u16 valid;
	u16 length;
	u32 data[MAX_SYSTEM_LLDP_TLV_DATA];
};

struct dcb_dscp_map {
	u32 flags;
#define DCB_DSCP_ENABLE_MASK			0x1
#define DCB_DSCP_ENABLE_SHIFT			0
#define DCB_DSCP_ENABLE				1
	u32 dscp_pri_map[8];
};

/**************************************/
/*                                    */
/*     P U B L I C      G L O B A L   */
/*                                    */
/**************************************/
struct public_global {
	u32 max_path;       /* 32bit is wasty, but this will be used often */
/* (Global) 32bit is wasty, but this will be used often */
	u32 max_ports;
#define MODE_1P	1		/* TBD - NEED TO THINK OF A BETTER NAME */
#define MODE_2P	2
#define MODE_3P	3
#define MODE_4P	4
	u32 debug_mb_offset;
	u32 phymod_dbg_mb_offset;
	struct couple_mode_teaming cmt;
/* Temperature in Celcius (-255C / +255C), measured every second. */
	s32 internal_temperature;
	u32 mfw_ver;
	u32 running_bundle_id;
	s32 external_temperature;
	u32 mdump_reason;
#define MDUMP_REASON_INTERNAL_ERROR	(1 << 0)
#define MDUMP_REASON_EXTERNAL_TRIGGER	(1 << 1)
#define MDUMP_REASON_DUMP_AGED		(1 << 2)
	u32 ext_phy_upgrade_fw;
#define EXT_PHY_FW_UPGRADE_STATUS_MASK		(0x0000ffff)
#define EXT_PHY_FW_UPGRADE_STATUS_SHIFT		(0)
#define EXT_PHY_FW_UPGRADE_STATUS_IN_PROGRESS	(1)
#define EXT_PHY_FW_UPGRADE_STATUS_FAILED	(2)
#define EXT_PHY_FW_UPGRADE_STATUS_SUCCESS	(3)
#define EXT_PHY_FW_UPGRADE_TYPE_MASK		(0xffff0000)
#define EXT_PHY_FW_UPGRADE_TYPE_SHIFT		(16)
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
	u32 accum_ack;      /* 0..15:PF, 16..207:VF, 256..271:IOV_DIS */
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
	u32 mcp_vf_disabled[VF_MAX_STATIC / 32];    /* 0x003c */

/* Reset on mcp reset, and incremented for eveny process kill event. */
	u32 process_kill;
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
	u32 validity_map;   /* 0x0 (4*2 = 0x8) */

	/* validity bits */
#define MCP_VALIDITY_PCI_CFG                    0x00100000
#define MCP_VALIDITY_MB                         0x00200000
#define MCP_VALIDITY_DEV_INFO                   0x00400000
#define MCP_VALIDITY_RESERVED                   0x00000007

	/* One licensing bit should be set */
/* yaniv - tbd ? license */
#define MCP_VALIDITY_LIC_KEY_IN_EFFECT_MASK     0x00000038
#define MCP_VALIDITY_LIC_MANUF_KEY_IN_EFFECT    0x00000008
#define MCP_VALIDITY_LIC_UPGRADE_KEY_IN_EFFECT  0x00000010
#define MCP_VALIDITY_LIC_NO_KEY_IN_EFFECT       0x00000020

	/* Active MFW */
#define MCP_VALIDITY_ACTIVE_MFW_UNKNOWN         0x00000000
#define MCP_VALIDITY_ACTIVE_MFW_MASK            0x000001c0
#define MCP_VALIDITY_ACTIVE_MFW_NCSI            0x00000040
#define MCP_VALIDITY_ACTIVE_MFW_NONE            0x000001c0

	u32 link_status;
#define LINK_STATUS_LINK_UP				0x00000001
#define LINK_STATUS_SPEED_AND_DUPLEX_MASK		0x0000001e
#define LINK_STATUS_SPEED_AND_DUPLEX_1000THD		(1 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_1000TFD		(2 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_10G		(3 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_20G		(4 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_40G		(5 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_50G		(6 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_100G		(7 << 1)
#define LINK_STATUS_SPEED_AND_DUPLEX_25G		(8 << 1)
#define LINK_STATUS_AUTO_NEGOTIATE_ENABLED		0x00000020
#define LINK_STATUS_AUTO_NEGOTIATE_COMPLETE		0x00000040
#define LINK_STATUS_PARALLEL_DETECTION_USED		0x00000080
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
#define LINK_STATUS_LINK_PARTNER_BOTH_PAUSE		(3 << 18)
#define LINK_STATUS_SFP_TX_FAULT			0x00100000
#define LINK_STATUS_TX_FLOW_CONTROL_ENABLED		0x00200000
#define LINK_STATUS_RX_FLOW_CONTROL_ENABLED		0x00400000
#define LINK_STATUS_RX_SIGNAL_PRESENT			0x00800000
#define LINK_STATUS_MAC_LOCAL_FAULT			0x01000000
#define LINK_STATUS_MAC_REMOTE_FAULT			0x02000000
#define LINK_STATUS_UNSUPPORTED_SPD_REQ			0x04000000
#define LINK_STATUS_FEC_MODE_MASK			0x38000000
#define LINK_STATUS_FEC_MODE_NONE			(0 << 27)
#define LINK_STATUS_FEC_MODE_FIRECODE_CL74		(1 << 27)
#define LINK_STATUS_FEC_MODE_RS_CL91			(2 << 27)
#define LINK_STATUS_EXT_PHY_LINK_UP			0x40000000

	u32 link_status1;
	u32 ext_phy_fw_version;
/* Points to struct eth_phy_cfg (For READ-ONLY) */
	u32 drv_phy_cfg_addr;

	u32 port_stx;

	u32 stat_nig_timer;

	struct port_mf_cfg port_mf_config;
	struct port_stats stats;

	u32 media_type;
#define	MEDIA_UNSPECIFIED	0x0
#define	MEDIA_SFPP_10G_FIBER	0x1	/* Use MEDIA_MODULE_FIBER instead */
#define	MEDIA_XFP_FIBER		0x2	/* Use MEDIA_MODULE_FIBER instead */
#define	MEDIA_DA_TWINAX		0x3
#define	MEDIA_BASE_T		0x4
#define MEDIA_SFP_1G_FIBER	0x5	/* Use MEDIA_MODULE_FIBER instead */
#define MEDIA_MODULE_FIBER	0x6
#define	MEDIA_KR		0xf0
#define	MEDIA_NOT_PRESENT	0xff

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
/* offset: 536 bytes? */
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
#define ETH_TRANSCEIVER_STATE_MASK			0x000000FF
#define ETH_TRANSCEIVER_STATE_SHIFT			0x00000000
#define ETH_TRANSCEIVER_STATE_UNPLUGGED			0x00000000
#define ETH_TRANSCEIVER_STATE_PRESENT			0x00000001
#define ETH_TRANSCEIVER_STATE_VALID			0x00000003
#define ETH_TRANSCEIVER_STATE_UPDATING			0x00000008
#define ETH_TRANSCEIVER_TYPE_MASK			0x0000FF00
#define ETH_TRANSCEIVER_TYPE_SHIFT			0x00000008
#define ETH_TRANSCEIVER_TYPE_NONE			0x00000000
#define ETH_TRANSCEIVER_TYPE_UNKNOWN			0x000000FF
/* 1G Passive copper cable */
#define ETH_TRANSCEIVER_TYPE_1G_PCC			0x01
/* 1G Active copper cable  */
#define ETH_TRANSCEIVER_TYPE_1G_ACC			0x02
#define ETH_TRANSCEIVER_TYPE_1G_LX			0x03
#define ETH_TRANSCEIVER_TYPE_1G_SX			0x04
#define ETH_TRANSCEIVER_TYPE_10G_SR			0x05
#define ETH_TRANSCEIVER_TYPE_10G_LR			0x06
#define ETH_TRANSCEIVER_TYPE_10G_LRM			0x07
#define ETH_TRANSCEIVER_TYPE_10G_ER			0x08
/* 10G Passive copper cable */
#define ETH_TRANSCEIVER_TYPE_10G_PCC			0x09
/* 10G Active copper cable  */
#define ETH_TRANSCEIVER_TYPE_10G_ACC			0x0a
#define ETH_TRANSCEIVER_TYPE_XLPPI			0x0b
#define ETH_TRANSCEIVER_TYPE_40G_LR4			0x0c
#define ETH_TRANSCEIVER_TYPE_40G_SR4			0x0d
#define ETH_TRANSCEIVER_TYPE_40G_CR4			0x0e
/* Active optical cable */
#define ETH_TRANSCEIVER_TYPE_100G_AOC			0x0f
#define ETH_TRANSCEIVER_TYPE_100G_SR4			0x10
#define ETH_TRANSCEIVER_TYPE_100G_LR4			0x11
#define ETH_TRANSCEIVER_TYPE_100G_ER4			0x12
/* Active copper cable */
#define ETH_TRANSCEIVER_TYPE_100G_ACC			0x13
#define ETH_TRANSCEIVER_TYPE_100G_CR4			0x14
#define ETH_TRANSCEIVER_TYPE_4x10G_SR			0x15
/* 25G Passive copper cable - short */
#define ETH_TRANSCEIVER_TYPE_25G_CA_N			0x16
/* 25G Active copper cable  - short */
#define ETH_TRANSCEIVER_TYPE_25G_ACC_S			0x17
/* 25G Passive copper cable - medium */
#define ETH_TRANSCEIVER_TYPE_25G_CA_S			0x18
/* 25G Active copper cable  - medium */
#define ETH_TRANSCEIVER_TYPE_25G_ACC_M			0x19
/* 25G Passive copper cable - long */
#define ETH_TRANSCEIVER_TYPE_25G_CA_L			0x1a
/* 25G Active copper cable  - long */
#define ETH_TRANSCEIVER_TYPE_25G_ACC_L			0x1b
#define ETH_TRANSCEIVER_TYPE_25G_SR			0x1c
#define ETH_TRANSCEIVER_TYPE_25G_LR			0x1d
#define ETH_TRANSCEIVER_TYPE_25G_AOC			0x1e

#define ETH_TRANSCEIVER_TYPE_4x10G			0x1f
#define ETH_TRANSCEIVER_TYPE_4x25G_CR			0x20
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_SR	0x30
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_CR	0x31
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_10G_40G_LR	0x32
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_SR	0x33
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_CR	0x34
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_LR	0x35
#define ETH_TRANSCEIVER_TYPE_MULTI_RATE_40G_100G_AOC	0x36
	u32 wol_info;
	u32 wol_pkt_len;
	u32 wol_pkt_details;
	struct dcb_dscp_map dcb_dscp_map;

	/* the status of EEE auto-negotiation
	 * bits 19:0 the configured tx-lpi entry timer value. Depends on bit 31.
	 * bits 23:20 the speeds advertised for EEE.
	 * bits 27:24 the speeds the Link partner advertised for EEE.
	 * The supported/adv. modes in bits 27:19 originate from the
	 * SHMEM_EEE_XXX_ADV definitions (where XXX is replaced by speed).
	 * bit 28 when 1'b1 EEE was requested.
	 * bit 29 when 1'b1 tx lpi was requested.
	 * bit 30 when 1'b1 EEE was negotiated. Tx lpi will be asserted if 30:29
	 *        are 2'b11.
	 * bit 31 - When 1'b0 bits 15:0 contain
	 *          NVM_CFG1_PORT_EEE_POWER_SAVING_MODE_XXX define as value.
	 *          When 1'b1 those bits contains a value times 16 microseconds.
	 */
	u32 eee_status;
#define EEE_TIMER_MASK		0x000fffff
#define EEE_ADV_STATUS_MASK	0x00f00000
#define EEE_1G_ADV	(1 << 1)
#define EEE_10G_ADV	(1 << 2)
#define EEE_ADV_STATUS_SHIFT	20
#define	EEE_LP_ADV_STATUS_MASK	0x0f000000
#define EEE_LP_ADV_STATUS_SHIFT	24
#define EEE_REQUESTED_BIT	0x10000000
#define EEE_LPI_REQUESTED_BIT	0x20000000
#define EEE_ACTIVE_BIT		0x40000000
#define EEE_TIME_OUTPUT_BIT	0x80000000

	u32 eee_remote;	/* Used for EEE in LLDP */
#define EEE_REMOTE_TW_TX_MASK	0x0000ffff
#define EEE_REMOTE_TW_TX_SHIFT	0
#define EEE_REMOTE_TW_RX_MASK	0xffff0000
#define EEE_REMOTE_TW_RX_SHIFT	16
};

/**************************************/
/*                                    */
/*     P U B L I C      F U N C       */
/*                                    */
/**************************************/

struct public_func {
	u32 iscsi_boot_signature;
	u32 iscsi_boot_block_offset;

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
#define FUNC_MF_CFG_PROTOCOL_ISCSI              0x00000010
#define FUNC_MF_CFG_PROTOCOL_FCOE		0x00000020
#define FUNC_MF_CFG_PROTOCOL_ROCE               0x00000030
#define FUNC_MF_CFG_PROTOCOL_MAX	        0x00000030

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

	u32 mac_upper;      /* MAC */
#define FUNC_MF_CFG_UPPERMAC_MASK               0x0000ffff
#define FUNC_MF_CFG_UPPERMAC_SHIFT              0
#define FUNC_MF_CFG_UPPERMAC_DEFAULT            FUNC_MF_CFG_UPPERMAC_MASK
	u32 mac_lower;
#define FUNC_MF_CFG_LOWERMAC_DEFAULT            0xffffffff

	u32 fcoe_wwn_port_name_upper;
	u32 fcoe_wwn_port_name_lower;

	u32 fcoe_wwn_node_name_upper;
	u32 fcoe_wwn_node_name_lower;

	u32 ovlan_stag;     /* tags */
#define FUNC_MF_CFG_OV_STAG_MASK              0x0000ffff
#define FUNC_MF_CFG_OV_STAG_SHIFT             0
#define FUNC_MF_CFG_OV_STAG_DEFAULT           FUNC_MF_CFG_OV_STAG_MASK

	u32 pf_allocation; /* vf per pf */

	u32 preserve_data; /* Will be used bt CCM */

	u32 driver_last_activity_ts;

	/*
	 * drv_ack_vf_disabled is set by the PF driver to ack handled disabled
	 * VFs
	 */
	u32 drv_ack_vf_disabled[VF_MAX_STATIC / 32];    /* 0x0044 */

	u32 drv_id;
#define DRV_ID_PDA_COMP_VER_MASK	0x0000ffff
#define DRV_ID_PDA_COMP_VER_SHIFT	0

#define LOAD_REQ_HSI_VERSION		2
#define DRV_ID_MCP_HSI_VER_MASK		0x00ff0000
#define DRV_ID_MCP_HSI_VER_SHIFT	16
#define DRV_ID_MCP_HSI_VER_CURRENT	(LOAD_REQ_HSI_VERSION << \
					 DRV_ID_MCP_HSI_VER_SHIFT)

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
	u32 mac_upper;      /* Upper 16 bits are always zeroes */
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

struct bist_nvm_image_att {
	u32 return_code;
	u32 image_type;		/* Image type */
	u32 nvm_start_addr;	/* NVM address of the image */
	u32 len;		/* Include CRC */
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

struct fcoe_stats_stc {
	u64 rx_pkts;
	u64 tx_pkts;
	u32 fcs_err;
	u32 login_failure;
};

struct iscsi_stats_stc {
	u64 rx_pdus;
	u64 tx_pdus;
	u64 rx_bytes;
	u64 tx_bytes;
};

struct rdma_stats_stc {
	u64 rx_pkts;
	u64 tx_pkts;
	u64 rx_bytes;
	u64 tx_bytes;
};

struct ocbb_data_stc {
	u32 ocbb_host_addr;
	u32 ocsd_host_addr;
	u32 ocsd_req_update_interval;
};

#define MAX_NUM_OF_SENSORS			7
#define MFW_SENSOR_LOCATION_INTERNAL		1
#define MFW_SENSOR_LOCATION_EXTERNAL		2
#define MFW_SENSOR_LOCATION_SFP			3

#define SENSOR_LOCATION_SHIFT			0
#define SENSOR_LOCATION_MASK			0x000000ff
#define THRESHOLD_HIGH_SHIFT			8
#define THRESHOLD_HIGH_MASK			0x0000ff00
#define CRITICAL_TEMPERATURE_SHIFT		16
#define CRITICAL_TEMPERATURE_MASK		0x00ff0000
#define CURRENT_TEMP_SHIFT			24
#define CURRENT_TEMP_MASK			0xff000000
struct temperature_status_stc {
	u32 num_of_sensors;
	u32 sensor[MAX_NUM_OF_SENSORS];
};

/* crash dump configuration header */
struct mdump_config_stc {
	u32 version;
	u32 config;
	u32 epoc;
	u32 num_of_logs;
	u32 valid_logs;
};

enum resource_id_enum {
	RESOURCE_NUM_SB_E		=	0,
	RESOURCE_NUM_L2_QUEUE_E		=	1,
	RESOURCE_NUM_VPORT_E		=	2,
	RESOURCE_NUM_VMQ_E		=	3,
/* Not a real resource!! it's a factor used to calculate others */
	RESOURCE_FACTOR_NUM_RSS_PF_E	=	4,
/* Not a real resource!! it's a factor used to calculate others */
	RESOURCE_FACTOR_RSS_PER_VF_E	=	5,
	RESOURCE_NUM_RL_E		=	6,
	RESOURCE_NUM_PQ_E		=	7,
	RESOURCE_NUM_VF_E		=	8,
	RESOURCE_VFC_FILTER_E		=	9,
	RESOURCE_ILT_E			=	10,
	RESOURCE_CQS_E			=	11,
	RESOURCE_GFT_PROFILES_E		=	12,
	RESOURCE_NUM_TC_E		=	13,
	RESOURCE_NUM_RSS_ENGINES_E	=	14,
	RESOURCE_LL2_QUEUE_E		=	15,
	RESOURCE_RDMA_STATS_QUEUE_E	=	16,
	RESOURCE_BDQ_E			=	17,
	RESOURCE_MAX_NUM,
	RESOURCE_NUM_INVALID		=	0xFFFFFFFF
};

/* Resource ID is to be filled by the driver in the MB request
 * Size, offset & flags to be filled by the MFW in the MB response
 */
struct resource_info {
	enum resource_id_enum res_id;
	u32 size; /* number of allocated resources */
	u32 offset; /* Offset of the 1st resource */
	u32 vf_size;
	u32 vf_offset;
	u32 flags;
#define RESOURCE_ELEMENT_STRICT (1 << 0)
};

#define DRV_ROLE_NONE		0
#define DRV_ROLE_PREBOOT	1
#define DRV_ROLE_OS		2
#define DRV_ROLE_KDUMP		3

struct load_req_stc {
	u32 drv_ver_0;
	u32 drv_ver_1;
	u32 fw_ver;
	u32 misc0;
#define LOAD_REQ_ROLE_MASK		0x000000FF
#define LOAD_REQ_ROLE_SHIFT		0
#define LOAD_REQ_LOCK_TO_MASK		0x0000FF00
#define LOAD_REQ_LOCK_TO_SHIFT		8
#define LOAD_REQ_LOCK_TO_DEFAULT	0
#define LOAD_REQ_LOCK_TO_NONE		255
#define LOAD_REQ_FORCE_MASK		0x000F0000
#define LOAD_REQ_FORCE_SHIFT		16
#define LOAD_REQ_FORCE_NONE		0
#define LOAD_REQ_FORCE_PF		1
#define LOAD_REQ_FORCE_ALL		2
#define LOAD_REQ_FLAGS0_MASK		0x00F00000
#define LOAD_REQ_FLAGS0_SHIFT		20
#define LOAD_REQ_FLAGS0_AVOID_RESET	(0x1 << 0)
};

struct load_rsp_stc {
	u32 drv_ver_0;
	u32 drv_ver_1;
	u32 fw_ver;
	u32 misc0;
#define LOAD_RSP_ROLE_MASK		0x000000FF
#define LOAD_RSP_ROLE_SHIFT		0
#define LOAD_RSP_HSI_MASK		0x0000FF00
#define LOAD_RSP_HSI_SHIFT		8
#define LOAD_RSP_FLAGS0_MASK		0x000F0000
#define LOAD_RSP_FLAGS0_SHIFT		16
#define LOAD_RSP_FLAGS0_DRV_EXISTS	(0x1 << 0)
};

union drv_union_data {
	struct mcp_mac wol_mac; /* UNLOAD_DONE */

/* This configuration should be set by the driver for the LINK_SET command. */

	struct eth_phy_cfg drv_phy_cfg;

	struct mcp_val64 val64; /* For PHY / AVS commands */

	u8 raw_data[MCP_DRV_NVM_BUF_LEN];

	struct mcp_file_att file_att;

	u32 ack_vf_disabled[VF_MAX_STATIC / 32];

	struct drv_version_stc drv_version;

	struct lan_stats_stc lan_stats;
	struct fcoe_stats_stc fcoe_stats;
	struct iscsi_stats_stc iscsi_stats;
	struct rdma_stats_stc rdma_stats;
	struct ocbb_data_stc ocbb_info;
	struct temperature_status_stc temp_info;
	struct resource_info resource;
	struct bist_nvm_image_att nvm_image_att;
	struct mdump_config_stc mdump_config;
	u32 dword;

	struct load_req_stc load_req;
	struct load_rsp_stc load_rsp;
	/* ... */
};

struct public_drv_mb {
	u32 drv_mb_header;
#define DRV_MSG_CODE_MASK                       0xffff0000
#define DRV_MSG_CODE_LOAD_REQ                   0x10000000
#define DRV_MSG_CODE_LOAD_DONE                  0x11000000
#define DRV_MSG_CODE_INIT_HW                    0x12000000
#define DRV_MSG_CODE_CANCEL_LOAD_REQ            0x13000000
#define DRV_MSG_CODE_UNLOAD_REQ		        0x20000000
#define DRV_MSG_CODE_UNLOAD_DONE                0x21000000
#define DRV_MSG_CODE_INIT_PHY			0x22000000
	/* Params - FORCE - Reinitialize the link regardless of LFA */
	/*        - DONT_CARE - Don't flap the link if up */
#define DRV_MSG_CODE_LINK_RESET			0x23000000

	/* Vitaly: LLDP commands */
#define DRV_MSG_CODE_SET_LLDP                   0x24000000
#define DRV_MSG_CODE_SET_DCBX                   0x25000000
	/* OneView feature driver HSI*/
#define DRV_MSG_CODE_OV_UPDATE_CURR_CFG		0x26000000
#define DRV_MSG_CODE_OV_UPDATE_BUS_NUM		0x27000000
#define DRV_MSG_CODE_OV_UPDATE_BOOT_PROGRESS	0x28000000
#define DRV_MSG_CODE_OV_UPDATE_STORM_FW_VER	0x29000000
#define DRV_MSG_CODE_NIG_DRAIN			0x30000000
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE	0x31000000
#define DRV_MSG_CODE_BW_UPDATE_ACK		0x32000000
#define DRV_MSG_CODE_OV_UPDATE_MTU		0x33000000
/* DRV_MB Param: driver version supp, FW_MB param: MFW version supp,
 * data: struct resource_info
 */
#define DRV_MSG_GET_RESOURCE_ALLOC_MSG		0x34000000
#define DRV_MSG_SET_RESOURCE_VALUE_MSG		0x35000000

/*deprecated don't use*/
#define DRV_MSG_CODE_INITIATE_FLR_DEPRECATED    0x02000000
#define DRV_MSG_CODE_INITIATE_PF_FLR            0x02010000
#define DRV_MSG_CODE_VF_DISABLED_DONE           0xc0000000
#define DRV_MSG_CODE_CFG_VF_MSIX                0xc0010000
/* Param is either DRV_MB_PARAM_NVM_PUT_FILE_BEGIN_MFW/IMAGE */
#define DRV_MSG_CODE_NVM_PUT_FILE_BEGIN		0x00010000
/* Param should be set to the transaction size (up to 64 bytes) */
#define DRV_MSG_CODE_NVM_PUT_FILE_DATA		0x00020000
/* MFW will place the file offset and len in file_att struct */
#define DRV_MSG_CODE_NVM_GET_FILE_ATT		0x00030000
/* Read 32bytes of nvram data. Param is [0:23] – Offset [24:31] –
 * Len in Bytes
 */
#define DRV_MSG_CODE_NVM_READ_NVRAM		0x00050000
/* Writes up to 32Bytes to nvram. Param is [0:23] – Offset [24:31] –
 * Len in Bytes. In case this address is in the range of secured file in
 * secured mode, the operation will fail
 */
#define DRV_MSG_CODE_NVM_WRITE_NVRAM		0x00060000
/* Delete a file from nvram. Param is image_type. */
#define DRV_MSG_CODE_NVM_DEL_FILE		0x00080000
/* Reset MCP when no NVM operation is going on, and no drivers are loaded.
 * In case operation succeed, MCP will not ack back.
 */
#define DRV_MSG_CODE_MCP_RESET			0x00090000
/* Temporary command to set secure mode, where the param is 0 (None secure) /
 * 1 (Secure) / 2 (Full-Secure)
 */
#define DRV_MSG_CODE_SET_SECURE_MODE		0x000a0000
/* Param: [0:15] - Address, [16:18] - lane# (0/1/2/3 - for single lane,
 * 4/5 - for dual lanes, 6 - for all lanes, [28] - PMD reg, [29] - select port,
 * [30:31] - port
 */
#define DRV_MSG_CODE_PHY_RAW_READ		0x000b0000
/* Param: [0:15] - Address, [16:18] - lane# (0/1/2/3 - for single lane,
 * 4/5 - for dual lanes, 6 - for all lanes, [28] - PMD reg, [29] - select port,
 * [30:31] - port
 */
#define DRV_MSG_CODE_PHY_RAW_WRITE		0x000c0000
/* Param: [0:15] - Address, [30:31] - port */
#define DRV_MSG_CODE_PHY_CORE_READ		0x000d0000
/* Param: [0:15] - Address, [30:31] - port */
#define DRV_MSG_CODE_PHY_CORE_WRITE		0x000e0000
/* Param: [0:3] - version, [4:15] - name (null terminated) */
#define DRV_MSG_CODE_SET_VERSION		0x000f0000
/* Halts the MCP. To resume MCP, user will need to use
 * MCP_REG_CPU_STATE/MCP_REG_CPU_MODE registers.
 */
#define DRV_MSG_CODE_MCP_HALT			0x00100000
/* Set virtual mac address, params [31:6] - reserved, [5:4] - type,
 * [3:0] - func, drv_data[7:0] - MAC/WWNN/WWPN
 */
#define DRV_MSG_CODE_SET_VMAC                   0x00110000
/* Set virtual mac address, params [31:6] - reserved, [5:4] - type,
 * [3:0] - func, drv_data[7:0] - MAC/WWNN/WWPN
 */
#define DRV_MSG_CODE_GET_VMAC                   0x00120000
#define DRV_MSG_CODE_VMAC_TYPE_SHIFT            4
#define DRV_MSG_CODE_VMAC_TYPE_MASK             0x30
#define DRV_MSG_CODE_VMAC_TYPE_MAC              1
#define DRV_MSG_CODE_VMAC_TYPE_WWNN             2
#define DRV_MSG_CODE_VMAC_TYPE_WWPN             3
/* Get statistics from pf, params [31:4] - reserved, [3:0] - stats type */
#define DRV_MSG_CODE_GET_STATS                  0x00130000
#define DRV_MSG_CODE_STATS_TYPE_LAN             1
#define DRV_MSG_CODE_STATS_TYPE_FCOE            2
#define DRV_MSG_CODE_STATS_TYPE_ISCSI           3
#define DRV_MSG_CODE_STATS_TYPE_RDMA            4
/* Host shall provide buffer and size for MFW  */
#define DRV_MSG_CODE_PMD_DIAG_DUMP		0x00140000
/* Host shall provide buffer and size for MFW  */
#define DRV_MSG_CODE_PMD_DIAG_EYE		0x00150000
/* Param: [0:1] - Port, [2:7] - read size, [8:15] - I2C address,
 * [16:31] - offset
 */
#define DRV_MSG_CODE_TRANSCEIVER_READ		0x00160000
/* Param: [0:1] - Port, [2:7] - write size, [8:15] - I2C address,
 * [16:31] - offset
 */
#define DRV_MSG_CODE_TRANSCEIVER_WRITE		0x00170000
/* indicate OCBB related information */
#define DRV_MSG_CODE_OCBB_DATA			0x00180000
/* Set function BW, params[15:8] - min, params[7:0] - max */
#define DRV_MSG_CODE_SET_BW			0x00190000
#define BW_MAX_MASK				0x000000ff
#define BW_MAX_SHIFT				0
#define BW_MIN_MASK				0x0000ff00
#define BW_MIN_SHIFT				8

/* When param is set to 1, all parities will be masked(disabled). When params
 * are set to 0, parities will be unmasked again.
 */
#define DRV_MSG_CODE_MASK_PARITIES		0x001a0000
/* param[0] - Simulate fan failure,  param[1] - simulate over temp. */
#define DRV_MSG_CODE_INDUCE_FAILURE		0x001b0000
#define DRV_MSG_FAN_FAILURE_TYPE		(1 << 0)
#define DRV_MSG_TEMPERATURE_FAILURE_TYPE	(1 << 1)
/* Param: [0:15] - gpio number */
#define DRV_MSG_CODE_GPIO_READ			0x001c0000
/* Param: [0:15] - gpio number, [16:31] - gpio value */
#define DRV_MSG_CODE_GPIO_WRITE			0x001d0000
/* Param: [0:7] - test enum, [8:15] - image index, [16:31] - reserved */
#define DRV_MSG_CODE_BIST_TEST			0x001e0000
#define DRV_MSG_CODE_GET_TEMPERATURE            0x001f0000

/* Set LED mode  params :0 operational, 1 LED turn ON, 2 LED turn OFF */
#define DRV_MSG_CODE_SET_LED_MODE		0x00200000
/* drv_data[7:0] - EPOC in seconds, drv_data[15:8] -
 * driver version (MAJ MIN BUILD SUB)
 */
#define DRV_MSG_CODE_TIMESTAMP                  0x00210000
/* This is an empty mailbox just return OK*/
#define DRV_MSG_CODE_EMPTY_MB			0x00220000

/* Param[0:4] - resource number (0-31), Param[5:7] - opcode,
 * param[15:8] - age
 */
#define DRV_MSG_CODE_RESOURCE_CMD		0x00230000

#define RESOURCE_CMD_REQ_RESC_MASK		0x0000001F
#define RESOURCE_CMD_REQ_RESC_SHIFT		0
#define RESOURCE_CMD_REQ_OPCODE_MASK		0x000000E0
#define RESOURCE_CMD_REQ_OPCODE_SHIFT		5
/* request resource ownership with default aging */
#define RESOURCE_OPCODE_REQ			1
/* request resource ownership without aging */
#define RESOURCE_OPCODE_REQ_WO_AGING		2
/* request resource ownership with specific aging timer (in seconds) */
#define RESOURCE_OPCODE_REQ_W_AGING		3
#define RESOURCE_OPCODE_RELEASE			4 /* release resource */
/* force resource release */
#define RESOURCE_OPCODE_FORCE_RELEASE		5
#define RESOURCE_CMD_REQ_AGE_MASK		0x0000FF00
#define RESOURCE_CMD_REQ_AGE_SHIFT		8

#define RESOURCE_CMD_RSP_OWNER_MASK		0x000000FF
#define RESOURCE_CMD_RSP_OWNER_SHIFT		0
#define RESOURCE_CMD_RSP_OPCODE_MASK		0x00000700
#define RESOURCE_CMD_RSP_OPCODE_SHIFT		8
/* resource is free and granted to requester */
#define RESOURCE_OPCODE_GNT			1
/* resource is busy, param[7:0] indicates owner as follow 0-15 = PF0-15,
 * 16 = MFW, 17 = diag over serial
 */
#define RESOURCE_OPCODE_BUSY			2
/* indicate release request was acknowledged */
#define RESOURCE_OPCODE_RELEASED		3
/* indicate release request was previously received by other owner */
#define RESOURCE_OPCODE_RELEASED_PREVIOUS	4
/* indicate wrong owner during release */
#define RESOURCE_OPCODE_WRONG_OWNER		5
#define RESOURCE_OPCODE_UNKNOWN_CMD		255

/* dedicate resource 0 for dump */
#define RESOURCE_DUMP				0

#define DRV_MSG_CODE_GET_MBA_VERSION		0x00240000 /* Get MBA version */
/* Send crash dump commands with param[3:0] - opcode */
#define DRV_MSG_CODE_MDUMP_CMD			0x00250000
#define MDUMP_DRV_PARAM_OPCODE_MASK		0x0000000f
/* acknowledge reception of error indication */
#define DRV_MSG_CODE_MDUMP_ACK			0x01
/* set epoc and personality as follow: drv_data[3:0] - epoch,
 * drv_data[7:4] - personality
 */
#define DRV_MSG_CODE_MDUMP_SET_VALUES		0x02
/* trigger crash dump procedure */
#define DRV_MSG_CODE_MDUMP_TRIGGER		0x03
/* Request valid logs and config words */
#define DRV_MSG_CODE_MDUMP_GET_CONFIG		0x04
/* Set triggers mask. drv_mb_param should indicate (bitwise) which
 * trigger enabled
 */
#define DRV_MSG_CODE_MDUMP_SET_ENABLE		0x05
/* Clear all logs */
#define DRV_MSG_CODE_MDUMP_CLEAR_LOGS		0x06
#define DRV_MSG_CODE_MEM_ECC_EVENTS		0x00260000 /* Param: None */
/* Param: [0:15] - gpio number */
#define DRV_MSG_CODE_GPIO_INFO			0x00270000
/* Value will be placed in union */
#define DRV_MSG_CODE_EXT_PHY_READ		0x00280000
/* Value should be placed in union */
#define DRV_MSG_CODE_EXT_PHY_WRITE		0x00290000
#define DRV_MB_PARAM_ADDR_SHIFT			0
#define DRV_MB_PARAM_ADDR_MASK			0x0000FFFF
#define DRV_MB_PARAM_DEVAD_SHIFT		16
#define DRV_MB_PARAM_DEVAD_MASK			0x001F0000
#define DRV_MB_PARAM_PORT_SHIFT			21
#define DRV_MB_PARAM_PORT_MASK			0x00600000
#define DRV_MSG_CODE_EXT_PHY_FW_UPGRADE		0x002a0000

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

	/* LLDP / DCBX params*/
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
	/* configure vf MSIX params*/
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

#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_SHIFT				0
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_MASK			0x000000FF
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_NONE				(1 << 0)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_ISCSI_IP_ACQUIRED		(1 << 1)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_FCOE_FABRIC_LOGIN_SUCCESS	(1 << 1)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_TRARGET_FOUND			(1 << 2)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_ISCSI_CHAP_SUCCESS		(1 << 3)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_FCOE_LUN_FOUND			(1 << 3)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_LOGGED_INTO_TGT		(1 << 4)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_IMG_DOWNLOADED			(1 << 5)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_PROG_OS_HANDOFF			(1 << 6)
#define DRV_MB_PARAM_OV_UPDATE_BOOT_COMPLETED				0

#define DRV_MB_PARAM_OV_PCI_BUS_NUM_SHIFT				0
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
/* Not Installed*/
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_NOT_LOADED	0x2
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_LOADING		0x3
/* installed but disabled by user/admin/OS */
#define DRV_MSG_CODE_OV_UPDATE_DRIVER_STATE_DISABLED	0x4
/* installed and active */
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
#define DRV_MB_PARAM_GPIO_DIRECTION_SHIFT	16
#define DRV_MB_PARAM_GPIO_DIRECTION_MASK	0x00FF0000
#define DRV_MB_PARAM_GPIO_CTRL_SHIFT		24
#define DRV_MB_PARAM_GPIO_CTRL_MASK		0xFF000000

	/* Resource Allocation params - Driver version support*/
#define DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR_MASK	0xFFFF0000
#define DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR_SHIFT		16
#define DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR_MASK	0x0000FFFF
#define DRV_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR_SHIFT		0

#define DRV_MB_PARAM_BIST_UNKNOWN_TEST		0
#define DRV_MB_PARAM_BIST_REGISTER_TEST		1
#define DRV_MB_PARAM_BIST_CLOCK_TEST		2
#define DRV_MB_PARAM_BIST_NVM_TEST_NUM_IMAGES		3
#define DRV_MB_PARAM_BIST_NVM_TEST_IMAGE_BY_INDEX	4

#define DRV_MB_PARAM_BIST_RC_UNKNOWN		0
#define DRV_MB_PARAM_BIST_RC_PASSED		1
#define DRV_MB_PARAM_BIST_RC_FAILED		2
#define DRV_MB_PARAM_BIST_RC_INVALID_PARAMETER		3

#define DRV_MB_PARAM_BIST_TEST_INDEX_SHIFT      0
#define DRV_MB_PARAM_BIST_TEST_INDEX_MASK       0x000000FF
#define DRV_MB_PARAM_BIST_TEST_IMAGE_INDEX_SHIFT      8
#define DRV_MB_PARAM_BIST_TEST_IMAGE_INDEX_MASK       0x0000FF00

	u32 fw_mb_header;
#define FW_MSG_CODE_MASK                        0xffff0000
#define FW_MSG_CODE_UNSUPPORTED			0x00000000
#define FW_MSG_CODE_DRV_LOAD_ENGINE		0x10100000
#define FW_MSG_CODE_DRV_LOAD_PORT               0x10110000
#define FW_MSG_CODE_DRV_LOAD_FUNCTION           0x10120000
#define FW_MSG_CODE_DRV_LOAD_REFUSED_PDA        0x10200000
#define FW_MSG_CODE_DRV_LOAD_REFUSED_HSI_1      0x10210000
#define FW_MSG_CODE_DRV_LOAD_REFUSED_DIAG       0x10220000
#define FW_MSG_CODE_DRV_LOAD_REFUSED_HSI        0x10230000
#define FW_MSG_CODE_DRV_LOAD_REFUSED_REQUIRES_FORCE 0x10300000
#define FW_MSG_CODE_DRV_LOAD_REFUSED_REJECT     0x10310000
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
#define FW_MSG_CODE_RESOURCE_ALLOC_OK           0x34000000
#define FW_MSG_CODE_RESOURCE_ALLOC_UNKNOWN      0x35000000
#define FW_MSG_CODE_RESOURCE_ALLOC_DEPRECATED   0x36000000
#define FW_MSG_CODE_RESOURCE_ALLOC_GEN_ERR      0x37000000
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
/* MFW reject "mcp reset" command if one of the drivers is up */
#define FW_MSG_CODE_MCP_RESET_REJECT		0x00600000
#define FW_MSG_CODE_NVM_FAILED_CALC_HASH	0x00310000
#define FW_MSG_CODE_NVM_PUBLIC_KEY_MISSING	0x00320000
#define FW_MSG_CODE_NVM_INVALID_PUBLIC_KEY	0x00330000

#define FW_MSG_CODE_PHY_OK			0x00110000
#define FW_MSG_CODE_PHY_ERROR			0x00120000
#define FW_MSG_CODE_SET_SECURE_MODE_ERROR	0x00130000
#define FW_MSG_CODE_SET_SECURE_MODE_OK		0x00140000
#define FW_MSG_MODE_PHY_PRIVILEGE_ERROR		0x00150000
#define FW_MSG_CODE_OK				0x00160000
#define FW_MSG_CODE_LED_MODE_INVALID		0x00170000
#define FW_MSG_CODE_PHY_DIAG_OK			0x00160000
#define FW_MSG_CODE_PHY_DIAG_ERROR		0x00170000
#define FW_MSG_CODE_INIT_HW_FAILED_TO_ALLOCATE_PAGE	0x00040000
#define FW_MSG_CODE_INIT_HW_FAILED_BAD_STATE    0x00170000
#define FW_MSG_CODE_INIT_HW_FAILED_TO_SET_WINDOW 0x000d0000
#define FW_MSG_CODE_INIT_HW_FAILED_NO_IMAGE	0x000c0000
#define FW_MSG_CODE_INIT_HW_FAILED_VERSION_MISMATCH	0x00100000
#define FW_MSG_CODE_TRANSCEIVER_DIAG_OK			0x00160000
#define FW_MSG_CODE_TRANSCEIVER_DIAG_ERROR		0x00170000
#define FW_MSG_CODE_TRANSCEIVER_NOT_PRESENT		0x00020000
#define FW_MSG_CODE_TRANSCEIVER_BAD_BUFFER_SIZE		0x000f0000
#define FW_MSG_CODE_GPIO_OK			0x00160000
#define FW_MSG_CODE_GPIO_DIRECTION_ERR		0x00170000
#define FW_MSG_CODE_GPIO_CTRL_ERR		0x00020000
#define FW_MSG_CODE_GPIO_INVALID		0x000f0000
#define FW_MSG_CODE_GPIO_INVALID_VALUE		0x00050000
#define FW_MSG_CODE_BIST_TEST_INVALID		0x000f0000
#define FW_MSG_CODE_EXTPHY_INVALID_IMAGE_HEADER	0x00700000
#define FW_MSG_CODE_EXTPHY_INVALID_PHY_TYPE	0x00710000
#define FW_MSG_CODE_EXTPHY_OPERATION_FAILED	0x00720000
#define FW_MSG_CODE_EXTPHY_NO_PHY_DETECTED	0x00730000
#define FW_MSG_CODE_RECOVERY_MODE		0x00740000

	/* mdump related response codes */
#define FW_MSG_CODE_MDUMP_NO_IMAGE_FOUND	0x00010000
#define FW_MSG_CODE_MDUMP_ALLOC_FAILED		0x00020000
#define FW_MSG_CODE_MDUMP_INVALID_CMD		0x00030000
#define FW_MSG_CODE_MDUMP_IN_PROGRESS		0x00040000
#define FW_MSG_CODE_MDUMP_WRITE_FAILED		0x00050000

#define FW_MSG_SEQ_NUMBER_MASK                  0x0000ffff


	u32 fw_mb_param;
/* Resource Allocation params - MFW  version support */
#define FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR_MASK	0xFFFF0000
#define FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MAJOR_SHIFT		16
#define FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR_MASK	0x0000FFFF
#define FW_MB_PARAM_RESOURCE_ALLOC_VERSION_MINOR_SHIFT		0


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

/* The union data is used by the driver to pass parameters to the scratchpad. */

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
	MFW_DRV_MSG_CRITICAL_ERROR_OCCURRED,
	MFW_DRV_MSG_EEE_NEGOTIATION_COMPLETE,
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
	u32 sup_msgs;       /* Assigend with MFW_DRV_MSG_MAX */
/* Incremented by the MFW */
	u32 msg[MFW_DRV_MSG_MAX_DWORDS(MFW_DRV_MSG_MAX)];
/* Incremented by the driver */
	u32 ack[MFW_DRV_MSG_MAX_DWORDS(MFW_DRV_MSG_MAX)];
};

/**************************************/
/*                                    */
/*     P U B L I C       D A T A      */
/*                                    */
/**************************************/
enum public_sections {
	PUBLIC_DRV_MB,      /* Points to the first drv_mb of path0 */
	PUBLIC_MFW_MB,      /* Points to the first mfw_mb of path0 */
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

#endif				/* MCP_PUBLIC_H */
