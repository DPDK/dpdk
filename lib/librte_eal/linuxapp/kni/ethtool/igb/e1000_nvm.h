/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2013 Intel Corporation.

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#ifndef _E1000_NVM_H_
#define _E1000_NVM_H_


struct e1000_fw_version {
	u32 etrack_id;
	u16 eep_major;
	u16 eep_minor;
	u16 eep_build;

	u8 invm_major;
	u8 invm_minor;
	u8 invm_img_type;

	bool or_valid;
	u16 or_major;
	u16 or_build;
	u16 or_patch;
};


void e1000_init_nvm_ops_generic(struct e1000_hw *hw);
s32  e1000_null_read_nvm(struct e1000_hw *hw, u16 a, u16 b, u16 *c);
void e1000_null_nvm_generic(struct e1000_hw *hw);
s32  e1000_null_led_default(struct e1000_hw *hw, u16 *data);
s32  e1000_null_write_nvm(struct e1000_hw *hw, u16 a, u16 b, u16 *c);
s32  e1000_acquire_nvm_generic(struct e1000_hw *hw);

s32  e1000_poll_eerd_eewr_done(struct e1000_hw *hw, int ee_reg);
s32  e1000_read_mac_addr_generic(struct e1000_hw *hw);
s32  e1000_read_pba_string_generic(struct e1000_hw *hw, u8 *pba_num,
				   u32 pba_num_size);
s32  e1000_read_pba_length_generic(struct e1000_hw *hw, u32 *pba_num_size);
s32  e1000_read_nvm_spi(struct e1000_hw *hw, u16 offset, u16 words, u16 *data);
s32  e1000_read_nvm_eerd(struct e1000_hw *hw, u16 offset, u16 words,
			 u16 *data);
s32  e1000_valid_led_default_generic(struct e1000_hw *hw, u16 *data);
s32  e1000_validate_nvm_checksum_generic(struct e1000_hw *hw);
s32  e1000_write_nvm_spi(struct e1000_hw *hw, u16 offset, u16 words,
			 u16 *data);
s32  e1000_update_nvm_checksum_generic(struct e1000_hw *hw);
void e1000_release_nvm_generic(struct e1000_hw *hw);
void e1000_get_fw_version(struct e1000_hw *hw,
			  struct e1000_fw_version *fw_vers);

#define E1000_STM_OPCODE	0xDB00

#endif
