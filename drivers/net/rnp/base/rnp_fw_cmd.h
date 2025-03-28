/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_FW_CMD_H_
#define _RNP_FW_CMD_H_

#include "rnp_osdep.h"
#include "rnp_hw.h"

#define RNP_FW_LINK_SYNC	(0x000c)
#define RNP_LINK_MAGIC_CODE	(0xa5a40000)
#define RNP_LINK_MAGIC_MASK	RTE_GENMASK32(31, 16)

enum RNP_GENERIC_CMD {
	/* general */
	RNP_GET_FW_VERSION		= 0x0001,
	RNP_READ_REG			= 0xFF03,
	RNP_WRITE_REG			= 0xFF04,
	RNP_MODIFY_REG			= 0xFF07,

	/* virtualization */
	RNP_IFUP_DOWN			= 0x0800,
	RNP_PTP_EVENT			= 0x0801,
	RNP_DRIVER_INSMOD		= 0x0803,
	RNP_SYSTEM_SUSPUSE		= 0x0804,
	RNP_FORCE_LINK_ON_CLOSE		= 0x0805,

	/* link configuration admin commands */
	RNP_GET_PHY_ABALITY		= 0x0601,
	RNP_GET_MAC_ADDRESS		= 0x0602,
	RNP_RESET_PHY			= 0x0603,
	RNP_LED_SET			= 0x0604,
	RNP_GET_LINK_STATUS		= 0x0607,
	RNP_LINK_STATUS_EVENT		= 0x0608,
	RNP_SET_LANE_FUN		= 0x0609,
	RNP_GET_LANE_STATUS		= 0x0610,
	RNP_SFP_SPEED_CHANGED_EVENT	= 0x0611,
	RNP_SET_EVENT_MASK		= 0x0613,
	RNP_SET_LANE_EVENT_EN		= 0x0614,
	RNP_SET_LOOPBACK_MODE		= 0x0618,
	RNP_PLUG_EVENT			= 0x0620,
	RNP_SET_PHY_REG			= 0x0628,
	RNP_GET_PHY_REG			= 0x0629,
	RNP_PHY_LINK_SET		= 0x0630,
	RNP_GET_PHY_STATISTICS		= 0x0631,
	RNP_GET_PCS_REG			= 0x0633,
	RNP_MODIFY_PCS_REG		= 0x0634,
	RNP_MODIFY_PHY_REG		= 0x0635,

	/*sfp-module*/
	RNP_SFP_MODULE_READ		= 0x0900,
	RNP_SFP_MODULE_WRITE		= 0x0901,

	/* fw update */
	RNP_FW_UPDATE			= 0x0700,
	RNP_FW_MAINTAIN			= 0x0701,
	RNP_EEPROM_OP			= 0x0705,
	RNP_EMI_SYNC			= 0x0706,

	RNP_GET_DUMP			= 0x0a00,
	RNP_SET_DUMP			= 0x0a10,
	RNP_GET_TEMP			= 0x0a11,
	RNP_SET_WOL			= 0x0a12,
	RNP_LLDP_TX_CTL			= 0x0a13,
	RNP_LLDP_STAT			= 0x0a14,
	RNP_SFC_OP			= 0x0a15,
	RNP_SRIOV_SET			= 0x0a16,
	RNP_SRIOV_STAT			= 0x0a17,

	RNP_SN_PN			= 0x0b00,

	RNP_ATU_OBOUND_SET		= 0xFF10,
	RNP_SET_DDR_CSL			= 0xFF11,
};

struct rnp_port_stat {
	u8 phy_addr;	     /* Phy MDIO address */

	u8 duplex	: 1; /* FIBRE is always 1,Twisted Pair 1 or 0 */
	u8 autoneg	: 1; /* autoned state */
	u8 fec		: 1;
	u8 an_rev	: 1;
	u8 link_traing	: 1;
	u8 is_sgmii	: 1; /* avild fw >= 0.5.0.17 */
	u8 rsvd0	: 2;
	u16 speed;	     /* cur port linked speed */

	u16 pause	: 4;
	u16 rsvd1	: 12;
};

/* firmware -> driver reply */
struct __rte_aligned(4) __rte_packed_begin rnp_phy_abilities_rep {
	u8 link_stat;
	u8 lane_mask;

	u32 speed;
	u16 phy_type;
	u16 nic_mode;
	u16 pfnum;
	u32 fw_version;
	u32 nic_clock;
	union  {
		u8 port_ids[4];
		u32 port_idf;
	};
	u32 fw_ext;
	u32 phy_id;
	u32 wol_status; /* bit0-3 wol supported . bit4-7 wol enable */
	union {
		u32 ext_ablity;
		struct {
			u32 valid			: 1; /* 0 */
			u32 wol_en			: 1; /* 1 */
			u32 pci_preset_runtime_en	: 1; /* 2 */
			u32 smbus_en			: 1; /* 3 */
			u32 ncsi_en			: 1; /* 4 */
			u32 rpu_en			: 1; /* 5 */
			u32 v2				: 1; /* 6 */
			u32 pxe_en			: 1; /* 7 */
			u32 mctp_en			: 1; /* 8 */
			u32 yt8614			: 1; /* 9 */
			u32 pci_ext_reset		: 1; /* 10 */
			u32 rpu_available		: 1; /* 11 */
			u32 fw_lldp_ablity		: 1; /* 12 */
			u32 lldp_enabled		: 1; /* 13 */
			u32 only_1g			: 1; /* 14 */
			u32 force_link_down_en		: 4; /* lane0 - lane4 */
			u32 force_link_supported	: 1;
			u32 ports_is_sgmii_valid	: 1;
			u32 lane_is_sgmii		: 4; /* 24 bit */
			u32 rsvd			: 7;
		} e;
	};
} __rte_packed_end;

struct rnp_mac_addr_rep {
	u32 lanes;
	struct rnp_mac_addr {
		/* for macaddr:01:02:03:04:05:06
		 *  mac-hi=0x01020304 mac-lo=0x05060000
		 */
		u8 mac[8];
	} addrs[4];
	u32 pcode;
};

#define RNP_SPEED_CAP_UNKNOWN    (0)
#define RNP_SPEED_CAP_10M_FULL   RTE_BIT32(2)
#define RNP_SPEED_CAP_100M_FULL  RTE_BIT32(3)
#define RNP_SPEED_CAP_1GB_FULL   RTE_BIT32(4)
#define RNP_SPEED_CAP_10GB_FULL  RTE_BIT32(5)
#define RNP_SPEED_CAP_40GB_FULL  RTE_BIT32(6)
#define RNP_SPEED_CAP_25GB_FULL  RTE_BIT32(7)
#define RNP_SPEED_CAP_50GB_FULL  RTE_BIT32(8)
#define RNP_SPEED_CAP_100GB_FULL RTE_BIT32(9)
#define RNP_SPEED_CAP_10M_HALF   RTE_BIT32(10)
#define RNP_SPEED_CAP_100M_HALF  RTE_BIT32(11)
#define RNP_SPEED_CAP_1GB_HALF   RTE_BIT32(12)

enum rnp_pma_phy_type {
	RNP_PHY_TYPE_NONE = 0,
	RNP_PHY_TYPE_1G_BASE_KX,
	RNP_PHY_TYPE_SGMII,
	RNP_PHY_TYPE_10G_BASE_KR,
	RNP_PHY_TYPE_25G_BASE_KR,
	RNP_PHY_TYPE_40G_BASE_KR4,
	RNP_PHY_TYPE_10G_BASE_SR,
	RNP_PHY_TYPE_40G_BASE_SR4,
	RNP_PHY_TYPE_40G_BASE_CR4,
	RNP_PHY_TYPE_40G_BASE_LR4,
	RNP_PHY_TYPE_10G_BASE_LR,
	RNP_PHY_TYPE_10G_BASE_ER,
	RNP_PHY_TYPE_10G_TP,
};

struct rnp_lane_stat_rep {
	u8 nr_lane;		/* 0-3 cur port correspond with hw lane */
	u8 pci_gen		: 4; /* nic cur pci speed genX: 1,2,3 */
	u8 pci_lanes		: 4; /* nic cur pci x1 x2 x4 x8 x16 */
	u8 pma_type;
	u8 phy_type;		/* interface media type */

	u16 linkup		: 1; /* cur port link state */
	u16 duplex		: 1; /* duplex state only RJ45 valid */
	u16 autoneg		: 1; /* autoneg state */
	u16 fec			: 1; /* fec state */
	u16 rev_an		: 1;
	u16 link_traing		: 1; /* link-traing state */
	u16 media_available	: 1;
	u16 is_sgmii		: 1; /* 1: Twisted Pair 0: FIBRE */
	u16 link_fault		: 4;
#define RNP_LINK_LINK_FAULT	RTE_BIT32(0)
#define RNP_LINK_TX_FAULT	RTE_BIT32(1)
#define RNP_LINK_RX_FAULT	RTE_BIT32(2)
#define RNP_LINK_REMOTE_FAULT	RTE_BIT32(3)
	u16 is_backplane	: 1;   /* Backplane Mode */
	u16 is_speed_10G_1G_auto_switch_enabled : 1;
	u16 rsvd0		: 2;
	union {
		u8 phy_addr;	/* Phy MDIO address */
		struct {
			u8 mod_abs : 1;
			u8 fault   : 1;
			u8 tx_dis  : 1;
			u8 los     : 1;
			u8 rsvd1   : 4;
		} sfp;
	};
	u8 sfp_connector;
	u32 speed;		/* Current Speed Value */

	u32 si_main;
	u32 si_pre;
	u32 si_post;
	u32 si_tx_boost;
	u32 supported_link;	/* Cur nic Support Link cap */
	u32 phy_id;
	u32 rsvd;
};

#define RNP_MBX_SYNC_MASK RTE_GENMASK32(15, 0)
/* == flags == */
#define RNP_FLAGS_DD  RTE_BIT32(0) /* driver clear 0, FW must set 1 */
#define RNP_FLAGS_CMP RTE_BIT32(1) /* driver clear 0, FW mucst set */
#define RNP_FLAGS_ERR RTE_BIT32(2) /* driver clear 0, FW must set only if it reporting an error */
#define RNP_FLAGS_LB  RTE_BIT32(9)
#define RNP_FLAGS_RD  RTE_BIT32(10) /* set if additional buffer has command parameters */
#define RNP_FLAGS_BUF RTE_BIT32(12) /* set 1 on indirect command */
#define RNP_FLAGS_SI  RTE_BIT32(13) /* not irq when command complete */
#define RNP_FLAGS_EI  RTE_BIT32(14) /* interrupt on error */
#define RNP_FLAGS_FE  RTE_BIT32(15) /* flush error */

#define RNP_FW_REP_DATA_NUM	(40)
struct rnp_mbx_fw_cmd_reply {
	u16 flags;
	u16 opcode;
	u16 error_code;
	u16 datalen;
	union {
		struct {
			u32 cookie_lo;
			u32 cookie_hi;
		};
		void *cookie;
	};
	u8 data[RNP_FW_REP_DATA_NUM];
};

struct rnp_fw_req_arg {
	u16 opcode;
	u32 param0;
	u32 param1;
	u32 param2;
	u32 param3;
	u32 param4;
	u32 param5;
};

static_assert(sizeof(struct rnp_mbx_fw_cmd_reply) == 56,
		"firmware request cmd size changed: rnp_mbx_fw_cmd_reply");

#define RNP_FW_REQ_DATA_NUM	(32)
/* driver op -> firmware */
struct rnp_mac_addr_req {
	u32 lane_mask;
	u32 pfvf_num;
	u32 rsv[2];
};

struct rnp_get_phy_ablity {
	u32 requester;
#define RNP_REQUEST_BY_DPDK (0xa1)
#define RNP_REQUEST_BY_DRV  (0xa2)
#define RNP_REQUEST_BY_PXE  (0xa3)
	u32 rsv[7];
};

struct rnp_get_lane_st_req {
	u32 nr_lane;

	u32 rsv[7];
};

#define RNP_FW_EVENT_LINK_UP	RTE_BIT32(0)
#define RNP_FW_EVENT_PLUG_IN	RTE_BIT32(1)
#define RNP_FW_EVENT_PLUG_OUT	RTE_BIT32(2)
struct rnp_set_pf_event_mask {
	u16 event_mask;
	u16 event_en;

	u32 rsv[7];
};

struct rnp_set_lane_event_mask {
	u32 nr_lane;
	u8 event_mask;
	u8 event_en;
	u8 rsvd[26];
};

/* FW op -> driver */
struct rnp_link_stat_req {
	u16 changed_lanes;
	u16 lane_status;
#define RNP_SPEED_VALID_MAGIC	(0xa4a6a8a9)
	u32 port_st_magic;
	struct rnp_port_stat states[RNP_MAX_PORT_OF_PF];
};

struct rnp_mbx_fw_cmd_req {
	u16 flags;
	u16 opcode;
	u16 datalen;
	u16 ret_value;
	union {
		struct {
			u32 cookie_lo; /* 8-11 */
			u32 cookie_hi; /* 12-15 */
		};
		void *cookie;
	};
	u32 reply_lo;
	u32 reply_hi;

	u8 data[RNP_FW_REQ_DATA_NUM];
};

static_assert(sizeof(struct rnp_mbx_fw_cmd_req) == 56,
		"firmware request cmd size changed: rnp_mbx_fw_cmd_req");

#define RNP_MBX_REQ_HDR_LEN	(24)
#define RNP_MBX_REPLYHDR_LEN	(16)
#define RNP_MAX_SHARE_MEM	(8 * 8)
struct rnp_mbx_req_cookie {
	u32 magic;
#define RNP_COOKIE_MAGIC	(0xCE)
	u32 timeout_ms;
	u32 errcode;

	/* wait_queue_head_t wait; */
	volatile u32 done;
	u32 priv_len;
	u8 priv[RNP_MAX_SHARE_MEM];
};

int rnp_build_fwcmd_req(struct rnp_mbx_fw_cmd_req *req,
			struct rnp_fw_req_arg *arg,
			void *cookie);
#endif /* _RNP_FW_CMD_H_ */
