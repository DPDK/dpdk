/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_EEPROM_H_
#define _TXGBE_EEPROM_H_

/* Checksum and EEPROM pointers */
#define TXGBE_PBANUM_PTR_GUARD		0xFAFA
#define TXGBE_EEPROM_SUM		0xBABA

#define TXGBE_FW_PTR			0x0F
#define TXGBE_PBANUM0_PTR		0x05
#define TXGBE_PBANUM1_PTR		0x06
#define TXGBE_SW_REGION_PTR             0x1C

#define TXGBE_EE_CSUM_MAX		0x800
#define TXGBE_EEPROM_CHECKSUM		0x2F

#define TXGBE_SAN_MAC_ADDR_PTR		0x18
#define TXGBE_DEVICE_CAPS		0x1C
#define TXGBE_EEPROM_VERSION_L          0x1D
#define TXGBE_EEPROM_VERSION_H          0x1E
#define TXGBE_ISCSI_BOOT_CONFIG         0x07

#define TXGBE_DEVICE_CAPS_ALLOW_ANY_SFP		0x1
#define TXGBE_FW_LESM_PARAMETERS_PTR		0x2
#define TXGBE_FW_LESM_STATE_1			0x1
#define TXGBE_FW_LESM_STATE_ENABLED		0x8000 /* LESM Enable bit */

s32 txgbe_init_eeprom_params(struct txgbe_hw *hw);
s32 txgbe_calc_eeprom_checksum(struct txgbe_hw *hw);
s32 txgbe_validate_eeprom_checksum(struct txgbe_hw *hw, u16 *checksum_val);
s32 txgbe_update_eeprom_checksum(struct txgbe_hw *hw);
s32 txgbe_get_eeprom_semaphore(struct txgbe_hw *hw);
void txgbe_release_eeprom_semaphore(struct txgbe_hw *hw);

s32 txgbe_ee_read16(struct txgbe_hw *hw, u32 offset, u16 *data);
s32 txgbe_ee_readw_sw(struct txgbe_hw *hw, u32 offset, u16 *data);
s32 txgbe_ee_readw_buffer(struct txgbe_hw *hw, u32 offset, u32 words,
				void *data);
s32 txgbe_ee_read32(struct txgbe_hw *hw, u32 addr, u32 *data);
s32 txgbe_ee_read_buffer(struct txgbe_hw *hw, u32 addr, u32 len, void *data);

s32 txgbe_ee_write16(struct txgbe_hw *hw, u32 offset, u16 data);
s32 txgbe_ee_writew_sw(struct txgbe_hw *hw, u32 offset, u16 data);
s32 txgbe_ee_writew_buffer(struct txgbe_hw *hw, u32 offset, u32 words,
				void *data);
s32 txgbe_ee_write32(struct txgbe_hw *hw, u32 addr, u32 data);
s32 txgbe_ee_write_buffer(struct txgbe_hw *hw, u32 addr, u32 len, void *data);


#endif /* _TXGBE_EEPROM_H_ */
