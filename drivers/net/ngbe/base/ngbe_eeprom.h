/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_EEPROM_H_
#define _NGBE_EEPROM_H_

#define NGBE_EEPROM_VERSION_L          0x1D
#define NGBE_EEPROM_VERSION_H          0x1E
#define NGBE_CALSUM_CAP_STATUS         0x10224
#define NGBE_EEPROM_VERSION_STORE_REG  0x1022C

s32 ngbe_init_eeprom_params(struct ngbe_hw *hw);
s32 ngbe_validate_eeprom_checksum_em(struct ngbe_hw *hw, u16 *checksum_val);
s32 ngbe_get_eeprom_semaphore(struct ngbe_hw *hw);
void ngbe_release_eeprom_semaphore(struct ngbe_hw *hw);
s32 ngbe_save_eeprom_version(struct ngbe_hw *hw);

s32 ngbe_ee_readw_buffer(struct ngbe_hw *hw, u32 offset, u32 words,
				void *data);
s32 ngbe_ee_read32(struct ngbe_hw *hw, u32 addr, u32 *data);

s32 ngbe_ee_writew_buffer(struct ngbe_hw *hw, u32 offset, u32 words,
				void *data);

#endif /* _NGBE_EEPROM_H_ */
