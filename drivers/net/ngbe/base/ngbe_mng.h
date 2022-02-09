/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_MNG_H_
#define _NGBE_MNG_H_

#include "ngbe_type.h"

#define NGBE_PMMBX_QSIZE       64 /* Num of dwords in range */
#define NGBE_PMMBX_BSIZE       (NGBE_PMMBX_QSIZE * 4)
#define NGBE_PMMBX_DATA_SIZE   (NGBE_PMMBX_BSIZE - FW_NVM_DATA_OFFSET * 4)
#define NGBE_HI_COMMAND_TIMEOUT        5000 /* Process HI command limit */

/* CEM Support */
#define FW_CEM_MAX_RETRIES              3
#define FW_CEM_RESP_STATUS_SUCCESS      0x1
#define FW_READ_SHADOW_RAM_CMD          0x31
#define FW_READ_SHADOW_RAM_LEN          0x6
#define FW_WRITE_SHADOW_RAM_CMD         0x33
#define FW_WRITE_SHADOW_RAM_LEN         0xA /* 8 plus 1 WORD to write */
#define FW_PCIE_READ_CMD		0xEC
#define FW_PCIE_WRITE_CMD		0xED
#define FW_PCIE_BUSMASTER_OFFSET        2
#define FW_DEFAULT_CHECKSUM             0xFF /* checksum always 0xFF */
#define FW_NVM_DATA_OFFSET              3
#define FW_EEPROM_CHECK_STATUS		0xE9
#define FW_PHY_LED_CONF			0xF1

#define FW_CHECKSUM_CAP_ST_PASS	0x80658383
#define FW_CHECKSUM_CAP_ST_FAIL	0x70657376

/* Host Interface Command Structures */
struct ngbe_hic_hdr {
	u8 cmd;
	u8 buf_len;
	union {
		u8 cmd_resv;
		u8 ret_status;
	} cmd_or_resp;
	u8 checksum;
};

struct ngbe_hic_hdr2_req {
	u8 cmd;
	u8 buf_lenh;
	u8 buf_lenl;
	u8 checksum;
};

struct ngbe_hic_hdr2_rsp {
	u8 cmd;
	u8 buf_lenl;
	u8 ret_status;     /* 7-5: high bits of buf_len, 4-0: status */
	u8 checksum;
};

union ngbe_hic_hdr2 {
	struct ngbe_hic_hdr2_req req;
	struct ngbe_hic_hdr2_rsp rsp;
};

/* These need to be dword aligned */
struct ngbe_hic_read_shadow_ram {
	union ngbe_hic_hdr2 hdr;
	u32 address;
	u16 length;
	u16 pad2;
	u16 data;
	u16 pad3;
};

struct ngbe_hic_write_shadow_ram {
	union ngbe_hic_hdr2 hdr;
	u32 address;
	u16 length;
	u16 pad2;
	u16 data;
	u16 pad3;
};

struct ngbe_hic_read_pcie {
	struct ngbe_hic_hdr hdr;
	u8 lan_id;
	u8 rsvd;
	u16 addr;
	u32 data;
};

struct ngbe_hic_write_pcie {
	struct ngbe_hic_hdr hdr;
	u8 lan_id;
	u8 rsvd;
	u16 addr;
	u32 data;
};

s32 ngbe_hic_sr_read(struct ngbe_hw *hw, u32 addr, u8 *buf, int len);
s32 ngbe_hic_sr_write(struct ngbe_hw *hw, u32 addr, u8 *buf, int len);
s32 ngbe_hic_pcie_read(struct ngbe_hw *hw, u16 addr, u32 *buf, int len);
s32 ngbe_hic_pcie_write(struct ngbe_hw *hw, u16 addr, u32 *buf, int len);

s32 ngbe_hic_check_cap(struct ngbe_hw *hw);
s32 ngbe_phy_led_oem_chk(struct ngbe_hw *hw, u32 *data);

#endif /* _NGBE_MNG_H_ */
