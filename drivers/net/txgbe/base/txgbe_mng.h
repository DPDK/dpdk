/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _TXGBE_MNG_H_
#define _TXGBE_MNG_H_

#include "txgbe_type.h"


#define TXGBE_PMMBX_QSIZE       64 /* Num of dwords in range */
#define TXGBE_PMMBX_BSIZE       (TXGBE_PMMBX_QSIZE * 4)
#define TXGBE_PMMBX_DATA_SIZE   (TXGBE_PMMBX_BSIZE - FW_NVM_DATA_OFFSET * 4)
#define TXGBE_HI_COMMAND_TIMEOUT        5000 /* Process HI command limit */
#define TXGBE_HI_FLASH_ERASE_TIMEOUT    5000 /* Process Erase command limit */
#define TXGBE_HI_FLASH_UPDATE_TIMEOUT   5000 /* Process Update command limit */
#define TXGBE_HI_FLASH_VERIFY_TIMEOUT   60000 /* Process Apply command limit */
#define TXGBE_HI_PHY_MGMT_REQ_TIMEOUT   2000 /* Wait up to 2 seconds */

/* CEM Support */
#define FW_CEM_HDR_LEN                  0x4
#define FW_CEM_CMD_DRIVER_INFO          0xDD
#define FW_CEM_CMD_DRIVER_INFO_LEN      0x5
#define FW_CEM_CMD_RESERVED             0X0
#define FW_CEM_UNUSED_VER               0x0
#define FW_CEM_MAX_RETRIES              3
#define FW_CEM_RESP_STATUS_SUCCESS      0x1
#define FW_READ_SHADOW_RAM_CMD          0x31
#define FW_READ_SHADOW_RAM_LEN          0x6
#define FW_WRITE_SHADOW_RAM_CMD         0x33
#define FW_WRITE_SHADOW_RAM_LEN         0xA /* 8 plus 1 WORD to write */
#define FW_SHADOW_RAM_DUMP_CMD          0x36
#define FW_SHADOW_RAM_DUMP_LEN          0
#define FW_DEFAULT_CHECKSUM             0xFF /* checksum always 0xFF */
#define FW_NVM_DATA_OFFSET              3
#define FW_MAX_READ_BUFFER_SIZE         244
#define FW_DISABLE_RXEN_CMD             0xDE
#define FW_DISABLE_RXEN_LEN             0x1
#define FW_PHY_MGMT_REQ_CMD             0x20
#define FW_RESET_CMD                    0xDF
#define FW_RESET_LEN                    0x2
#define FW_SETUP_MAC_LINK_CMD           0xE0
#define FW_SETUP_MAC_LINK_LEN           0x2
#define FW_FLASH_UPGRADE_START_CMD      0xE3
#define FW_FLASH_UPGRADE_START_LEN      0x1
#define FW_FLASH_UPGRADE_WRITE_CMD      0xE4
#define FW_FLASH_UPGRADE_VERIFY_CMD     0xE5
#define FW_FLASH_UPGRADE_VERIFY_LEN     0x4
#define FW_PHY_ACT_DATA_COUNT		4
#define FW_PHY_TOKEN_DELAY		5	/* milliseconds */
#define FW_PHY_TOKEN_WAIT		5	/* seconds */
#define FW_PHY_TOKEN_RETRIES ((FW_PHY_TOKEN_WAIT * 1000) / FW_PHY_TOKEN_DELAY)

/* Host Interface Command Structures */
struct txgbe_hic_hdr {
	u8 cmd;
	u8 buf_len;
	union {
		u8 cmd_resv;
		u8 ret_status;
	} cmd_or_resp;
	u8 checksum;
};

struct txgbe_hic_hdr2_req {
	u8 cmd;
	u8 buf_lenh;
	u8 buf_lenl;
	u8 checksum;
};

struct txgbe_hic_hdr2_rsp {
	u8 cmd;
	u8 buf_lenl;
	u8 buf_lenh_status;     /* 7-5: high bits of buf_len, 4-0: status */
	u8 checksum;
};

union txgbe_hic_hdr2 {
	struct txgbe_hic_hdr2_req req;
	struct txgbe_hic_hdr2_rsp rsp;
};

struct txgbe_hic_drv_info {
	struct txgbe_hic_hdr hdr;
	u8 port_num;
	u8 ver_sub;
	u8 ver_build;
	u8 ver_min;
	u8 ver_maj;
	u8 pad; /* end spacing to ensure length is mult. of dword */
	u16 pad2; /* end spacing to ensure length is mult. of dword2 */
};

/* These need to be dword aligned */
struct txgbe_hic_read_shadow_ram {
	union txgbe_hic_hdr2 hdr;
	u32 address;
	u16 length;
	u16 pad2;
	u16 data;
	u16 pad3;
};

struct txgbe_hic_write_shadow_ram {
	union txgbe_hic_hdr2 hdr;
	u32 address;
	u16 length;
	u16 pad2;
	u16 data;
	u16 pad3;
};

struct txgbe_hic_disable_rxen {
	struct txgbe_hic_hdr hdr;
	u8  port_number;
	u8  pad2;
	u16 pad3;
};

struct txgbe_hic_reset {
	struct txgbe_hic_hdr hdr;
	u16 lan_id;
	u16 reset_type;
};

struct txgbe_hic_phy_cfg {
	struct txgbe_hic_hdr hdr;
	u8 lan_id;
	u8 phy_mode;
	u16 phy_speed;
};

enum txgbe_module_id {
	TXGBE_MODULE_EEPROM = 0,
	TXGBE_MODULE_FIRMWARE,
	TXGBE_MODULE_HARDWARE,
	TXGBE_MODULE_PCIE
};

struct txgbe_hic_upg_start {
	struct txgbe_hic_hdr hdr;
	u8 module_id;
	u8  pad2;
	u16 pad3;
};

struct txgbe_hic_upg_write {
	struct txgbe_hic_hdr hdr;
	u8 data_len;
	u8 eof_flag;
	u16 check_sum;
	u32 data[62];
};

enum txgbe_upg_flag {
	TXGBE_RESET_NONE = 0,
	TXGBE_RESET_FIRMWARE,
	TXGBE_RELOAD_EEPROM,
	TXGBE_RESET_LAN
};

struct txgbe_hic_upg_verify {
	struct txgbe_hic_hdr hdr;
	u32 action_flag;
};

s32 txgbe_hic_sr_read(struct txgbe_hw *hw, u32 addr, u8 *buf, int len);
s32 txgbe_hic_sr_write(struct txgbe_hw *hw, u32 addr, u8 *buf, int len);

s32 txgbe_hic_set_drv_ver(struct txgbe_hw *hw, u8 maj, u8 min, u8 build,
			u8 ver, u16 len, const char *str);
s32 txgbe_hic_reset(struct txgbe_hw *hw);
bool txgbe_mng_present(struct txgbe_hw *hw);
bool txgbe_mng_enabled(struct txgbe_hw *hw);
#endif /* _TXGBE_MNG_H_ */
