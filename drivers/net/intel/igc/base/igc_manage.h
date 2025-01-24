/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _IGC_MANAGE_H_
#define _IGC_MANAGE_H_

bool igc_check_mng_mode_generic(struct igc_hw *hw);
bool igc_enable_tx_pkt_filtering_generic(struct igc_hw *hw);
s32  igc_mng_enable_host_if_generic(struct igc_hw *hw);
s32  igc_mng_host_if_write_generic(struct igc_hw *hw, u8 *buffer,
				     u16 length, u16 offset, u8 *sum);
s32  igc_mng_write_cmd_header_generic(struct igc_hw *hw,
				     struct igc_host_mng_command_header *hdr);
s32  igc_mng_write_dhcp_info_generic(struct igc_hw *hw,
				       u8 *buffer, u16 length);
bool igc_enable_mng_pass_thru(struct igc_hw *hw);
u8 igc_calculate_checksum(u8 *buffer, u32 length);
s32 igc_host_interface_command(struct igc_hw *hw, u8 *buffer, u32 length);
s32 igc_load_firmware(struct igc_hw *hw, u8 *buffer, u32 length);

enum igc_mng_mode {
	igc_mng_mode_none = 0,
	igc_mng_mode_asf,
	igc_mng_mode_pt,
	igc_mng_mode_ipmi,
	igc_mng_mode_host_if_only
};

#define IGC_FACTPS_MNGCG			0x20000000

#define IGC_FWSM_MODE_MASK			0xE
#define IGC_FWSM_MODE_SHIFT			1
#define IGC_FWSM_FW_VALID			0x00008000
#define IGC_FWSM_HI_EN_ONLY_MODE		0x4

#define IGC_MNG_IAMT_MODE			0x3
#define IGC_MNG_DHCP_COOKIE_LENGTH		0x10
#define IGC_MNG_DHCP_COOKIE_OFFSET		0x6F0
#define IGC_MNG_DHCP_COMMAND_TIMEOUT		10
#define IGC_MNG_DHCP_TX_PAYLOAD_CMD		64
#define IGC_MNG_DHCP_COOKIE_STATUS_PARSING	0x1
#define IGC_MNG_DHCP_COOKIE_STATUS_VLAN	0x2

#define IGC_VFTA_ENTRY_SHIFT			5
#define IGC_VFTA_ENTRY_MASK			0x7F
#define IGC_VFTA_ENTRY_BIT_SHIFT_MASK		0x1F

#define IGC_HI_MAX_BLOCK_BYTE_LENGTH		1792 /* Num of bytes in range */
#define IGC_HI_MAX_BLOCK_DWORD_LENGTH		448 /* Num of dwords in range */
#define IGC_HI_COMMAND_TIMEOUT		500 /* Process HI cmd limit */
#define IGC_HI_FW_BASE_ADDRESS		0x10000
#define IGC_HI_FW_MAX_LENGTH			(64 * 1024) /* Num of bytes */
#define IGC_HI_FW_BLOCK_DWORD_LENGTH		256 /* Num of DWORDs per page */
#define IGC_HICR_MEMORY_BASE_EN		0x200 /* MB Enable bit - RO */
#define IGC_HICR_EN			0x01  /* Enable bit - RO */
/* Driver sets this bit when done to put command in RAM */
#define IGC_HICR_C			0x02
#define IGC_HICR_SV			0x04  /* Status Validity */
#define IGC_HICR_FW_RESET_ENABLE	0x40
#define IGC_HICR_FW_RESET		0x80

/* Intel(R) Active Management Technology signature */
#define IGC_IAMT_SIGNATURE		0x544D4149
#endif
