/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "txgbe_hw.h"
#include "txgbe_mng.h"
#include "txgbe_eeprom.h"

/**
 *  txgbe_init_eeprom_params - Initialize EEPROM params
 *  @hw: pointer to hardware structure
 *
 *  Initializes the EEPROM parameters txgbe_rom_info within the
 *  txgbe_hw struct in order to set up EEPROM access.
 **/
s32 txgbe_init_eeprom_params(struct txgbe_hw *hw)
{
	struct txgbe_rom_info *eeprom = &hw->rom;
	u32 eec;
	u16 eeprom_size;
	int err = 0;

	if (eeprom->type != txgbe_eeprom_unknown)
		return 0;

	eeprom->type = txgbe_eeprom_none;
	/* Set default semaphore delay to 10ms which is a well
	 * tested value
	 */
	eeprom->semaphore_delay = 10; /*ms*/
	/* Clear EEPROM page size, it will be initialized as needed */
	eeprom->word_page_size = 0;

	/*
	 * Check for EEPROM present first.
	 * If not present leave as none
	 */
	eec = rd32(hw, TXGBE_SPISTAT);
	if (!(eec & TXGBE_SPISTAT_BPFLASH)) {
		eeprom->type = txgbe_eeprom_flash;

		/*
		 * SPI EEPROM is assumed here.  This code would need to
		 * change if a future EEPROM is not SPI.
		 */
		eeprom_size = 4096;
		eeprom->word_size = eeprom_size >> 1;
	}

	eeprom->address_bits = 16;

	err = eeprom->read32(hw, TXGBE_SW_REGION_PTR << 1, &eeprom->sw_addr);
	if (err) {
		DEBUGOUT("EEPROM read failed.");
		return err;
	}

	DEBUGOUT("eeprom params: type = %d, size = %d, address bits: %d %d",
		  eeprom->type, eeprom->word_size,
		  eeprom->address_bits, eeprom->sw_addr);

	return 0;
}

/**
 *  txgbe_get_eeprom_semaphore - Get hardware semaphore
 *  @hw: pointer to hardware structure
 *
 *  Sets the hardware semaphores so EEPROM access can occur for bit-bang method
 **/
s32 txgbe_get_eeprom_semaphore(struct txgbe_hw *hw)
{
	s32 status = TXGBE_ERR_EEPROM;
	u32 timeout = 2000;
	u32 i;
	u32 swsm;

	/* Get SMBI software semaphore between device drivers first */
	for (i = 0; i < timeout; i++) {
		/*
		 * If the SMBI bit is 0 when we read it, then the bit will be
		 * set and we have the semaphore
		 */
		swsm = rd32(hw, TXGBE_SWSEM);
		if (!(swsm & TXGBE_SWSEM_PF)) {
			status = 0;
			break;
		}
		usec_delay(50);
	}

	if (i == timeout) {
		DEBUGOUT("Driver can't access the eeprom - SMBI Semaphore not granted.");
		/*
		 * this release is particularly important because our attempts
		 * above to get the semaphore may have succeeded, and if there
		 * was a timeout, we should unconditionally clear the semaphore
		 * bits to free the driver to make progress
		 */
		txgbe_release_eeprom_semaphore(hw);

		usec_delay(50);
		/*
		 * one last try
		 * If the SMBI bit is 0 when we read it, then the bit will be
		 * set and we have the semaphore
		 */
		swsm = rd32(hw, TXGBE_SWSEM);
		if (!(swsm & TXGBE_SWSEM_PF))
			status = 0;
	}

	/* Now get the semaphore between SW/FW through the SWESMBI bit */
	if (status == 0) {
		for (i = 0; i < timeout; i++) {
			/* Set the SW EEPROM semaphore bit to request access */
			wr32m(hw, TXGBE_MNGSWSYNC,
				TXGBE_MNGSWSYNC_REQ, TXGBE_MNGSWSYNC_REQ);

			/*
			 * If we set the bit successfully then we got the
			 * semaphore.
			 */
			swsm = rd32(hw, TXGBE_MNGSWSYNC);
			if (swsm & TXGBE_MNGSWSYNC_REQ)
				break;

			usec_delay(50);
		}

		/*
		 * Release semaphores and return error if SW EEPROM semaphore
		 * was not granted because we don't have access to the EEPROM
		 */
		if (i >= timeout) {
			DEBUGOUT("SWESMBI Software EEPROM semaphore not granted.");
			txgbe_release_eeprom_semaphore(hw);
			status = TXGBE_ERR_EEPROM;
		}
	} else {
		DEBUGOUT("Software semaphore SMBI between device drivers not granted.");
	}

	return status;
}

/**
 *  txgbe_release_eeprom_semaphore - Release hardware semaphore
 *  @hw: pointer to hardware structure
 *
 *  This function clears hardware semaphore bits.
 **/
void txgbe_release_eeprom_semaphore(struct txgbe_hw *hw)
{
	wr32m(hw, TXGBE_MNGSWSYNC, TXGBE_MNGSWSYNC_REQ, 0);
	wr32m(hw, TXGBE_SWSEM, TXGBE_SWSEM_PF, 0);
	txgbe_flush(hw);
}

/**
 *  txgbe_ee_read - Read EEPROM word using a host interface cmd
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to read
 *  @data: word read from the EEPROM
 *
 *  Reads a 16 bit word from the EEPROM using the hostif.
 **/
s32 txgbe_ee_read16(struct txgbe_hw *hw, u32 offset,
			      u16 *data)
{
	const u32 mask = TXGBE_MNGSEM_SWMBX | TXGBE_MNGSEM_SWFLASH;
	u32 addr = (offset << 1);
	int err;

	err = hw->mac.acquire_swfw_sync(hw, mask);
	if (err)
		return err;

	err = txgbe_hic_sr_read(hw, addr, (u8 *)data, 2);

	hw->mac.release_swfw_sync(hw, mask);

	return err;
}

/**
 *  txgbe_ee_readw_buffer- Read EEPROM word(s) using hostif
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to read
 *  @words: number of words
 *  @data: word(s) read from the EEPROM
 *
 *  Reads a 16 bit word(s) from the EEPROM using the hostif.
 **/
s32 txgbe_ee_readw_buffer(struct txgbe_hw *hw,
				     u32 offset, u32 words, void *data)
{
	const u32 mask = TXGBE_MNGSEM_SWMBX | TXGBE_MNGSEM_SWFLASH;
	u32 addr = (offset << 1);
	u32 len = (words << 1);
	u8 *buf = (u8 *)data;
	int err;

	err = hw->mac.acquire_swfw_sync(hw, mask);
	if (err)
		return err;

	while (len) {
		u32 seg = (len <= TXGBE_PMMBX_DATA_SIZE
				? len : TXGBE_PMMBX_DATA_SIZE);

		err = txgbe_hic_sr_read(hw, addr, buf, seg);
		if (err)
			break;

		len -= seg;
		addr += seg;
		buf += seg;
	}

	hw->mac.release_swfw_sync(hw, mask);
	return err;
}


s32 txgbe_ee_readw_sw(struct txgbe_hw *hw, u32 offset,
			      u16 *data)
{
	const u32 mask = TXGBE_MNGSEM_SWMBX | TXGBE_MNGSEM_SWFLASH;
	u32 addr = hw->rom.sw_addr + (offset << 1);
	int err;

	err = hw->mac.acquire_swfw_sync(hw, mask);
	if (err)
		return err;

	err = txgbe_hic_sr_read(hw, addr, (u8 *)data, 2);

	hw->mac.release_swfw_sync(hw, mask);

	return err;
}

/**
 *  txgbe_ee_read32 - Read EEPROM word using a host interface cmd
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to read
 *  @data: word read from the EEPROM
 *
 *  Reads a 32 bit word from the EEPROM using the hostif.
 **/
s32 txgbe_ee_read32(struct txgbe_hw *hw, u32 addr, u32 *data)
{
	const u32 mask = TXGBE_MNGSEM_SWMBX | TXGBE_MNGSEM_SWFLASH;
	int err;

	err = hw->mac.acquire_swfw_sync(hw, mask);
	if (err)
		return err;

	err = txgbe_hic_sr_read(hw, addr, (u8 *)data, 4);

	hw->mac.release_swfw_sync(hw, mask);

	return err;
}

/**
 *  txgbe_ee_write - Write EEPROM word using hostif
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to write
 *  @data: word write to the EEPROM
 *
 *  Write a 16 bit word to the EEPROM using the hostif.
 **/
s32 txgbe_ee_write16(struct txgbe_hw *hw, u32 offset,
			       u16 data)
{
	const u32 mask = TXGBE_MNGSEM_SWMBX | TXGBE_MNGSEM_SWFLASH;
	u32 addr = (offset << 1);
	int err;

	err = hw->mac.acquire_swfw_sync(hw, mask);
	if (err)
		return err;

	err = txgbe_hic_sr_write(hw, addr, (u8 *)&data, 2);

	hw->mac.release_swfw_sync(hw, mask);

	return err;
}

/**
 *  txgbe_ee_writew_buffer - Write EEPROM word(s) using hostif
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to write
 *  @words: number of words
 *  @data: word(s) write to the EEPROM
 *
 *  Write a 16 bit word(s) to the EEPROM using the hostif.
 **/
s32 txgbe_ee_writew_buffer(struct txgbe_hw *hw,
				      u32 offset, u32 words, void *data)
{
	const u32 mask = TXGBE_MNGSEM_SWMBX | TXGBE_MNGSEM_SWFLASH;
	u32 addr = (offset << 1);
	u32 len = (words << 1);
	u8 *buf = (u8 *)data;
	int err;

	err = hw->mac.acquire_swfw_sync(hw, mask);
	if (err)
		return err;

	while (len) {
		u32 seg = (len <= TXGBE_PMMBX_DATA_SIZE
				? len : TXGBE_PMMBX_DATA_SIZE);

		err = txgbe_hic_sr_write(hw, addr, buf, seg);
		if (err)
			break;

		len -= seg;
		buf += seg;
	}

	hw->mac.release_swfw_sync(hw, mask);
	return err;
}

s32 txgbe_ee_writew_sw(struct txgbe_hw *hw, u32 offset,
			       u16 data)
{
	const u32 mask = TXGBE_MNGSEM_SWMBX | TXGBE_MNGSEM_SWFLASH;
	u32 addr = hw->rom.sw_addr + (offset << 1);
	int err;

	err = hw->mac.acquire_swfw_sync(hw, mask);
	if (err)
		return err;

	err = txgbe_hic_sr_write(hw, addr, (u8 *)&data, 2);

	hw->mac.release_swfw_sync(hw, mask);

	return err;
}

/**
 *  txgbe_ee_write32 - Read EEPROM word using a host interface cmd
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to read
 *  @data: word read from the EEPROM
 *
 *  Reads a 32 bit word from the EEPROM using the hostif.
 **/
s32 txgbe_ee_write32(struct txgbe_hw *hw, u32 addr, u32 data)
{
	const u32 mask = TXGBE_MNGSEM_SWMBX | TXGBE_MNGSEM_SWFLASH;
	int err;

	err = hw->mac.acquire_swfw_sync(hw, mask);
	if (err)
		return err;

	err = txgbe_hic_sr_write(hw, addr, (u8 *)&data, 4);

	hw->mac.release_swfw_sync(hw, mask);

	return err;
}

/**
 *  txgbe_calc_eeprom_checksum - Calculates and returns the checksum
 *  @hw: pointer to hardware structure
 *
 *  Returns a negative error code on error, or the 16-bit checksum
 **/
#define BUFF_SIZE  64
s32 txgbe_calc_eeprom_checksum(struct txgbe_hw *hw)
{
	u16 checksum = 0, read_checksum = 0;
	int i, j, seg;
	int err;
	u16 buffer[BUFF_SIZE];

	err = hw->rom.readw_sw(hw, TXGBE_EEPROM_CHECKSUM, &read_checksum);
	if (err) {
		DEBUGOUT("EEPROM read failed");
		return err;
	}

	for (i = 0; i < TXGBE_EE_CSUM_MAX; i += seg) {
		seg = (i + BUFF_SIZE < TXGBE_EE_CSUM_MAX
		       ? BUFF_SIZE : TXGBE_EE_CSUM_MAX - i);
		err = hw->rom.readw_buffer(hw, i, seg, buffer);
		if (err)
			return err;
		for (j = 0; j < seg; j++)
			checksum += buffer[j];
	}

	checksum = (u16)TXGBE_EEPROM_SUM - checksum + read_checksum;

	return (s32)checksum;
}

/**
 *  txgbe_validate_eeprom_checksum - Validate EEPROM checksum
 *  @hw: pointer to hardware structure
 *  @checksum_val: calculated checksum
 *
 *  Performs checksum calculation and validates the EEPROM checksum.  If the
 *  caller does not need checksum_val, the value can be NULL.
 **/
s32 txgbe_validate_eeprom_checksum(struct txgbe_hw *hw,
					   u16 *checksum_val)
{
	u16 checksum;
	u16 read_checksum = 0;
	int err;

	/* Read the first word from the EEPROM. If this times out or fails, do
	 * not continue or we could be in for a very long wait while every
	 * EEPROM read fails
	 */
	err = hw->rom.read16(hw, 0, &checksum);
	if (err) {
		DEBUGOUT("EEPROM read failed");
		return err;
	}

	err = hw->rom.calc_checksum(hw);
	if (err < 0)
		return err;

	checksum = (u16)(err & 0xffff);

	err = hw->rom.readw_sw(hw, TXGBE_EEPROM_CHECKSUM, &read_checksum);
	if (err) {
		DEBUGOUT("EEPROM read failed");
		return err;
	}

	/* Verify read checksum from EEPROM is the same as
	 * calculated checksum
	 */
	if (read_checksum != checksum) {
		err = TXGBE_ERR_EEPROM_CHECKSUM;
		DEBUGOUT("EEPROM checksum error");
	}

	/* If the user cares, return the calculated checksum */
	if (checksum_val)
		*checksum_val = checksum;

	return err;
}

/**
 *  txgbe_update_eeprom_checksum - Updates the EEPROM checksum
 *  @hw: pointer to hardware structure
 **/
s32 txgbe_update_eeprom_checksum(struct txgbe_hw *hw)
{
	s32 status;
	u16 checksum;

	/* Read the first word from the EEPROM. If this times out or fails, do
	 * not continue or we could be in for a very long wait while every
	 * EEPROM read fails
	 */
	status = hw->rom.read16(hw, 0, &checksum);
	if (status) {
		DEBUGOUT("EEPROM read failed");
		return status;
	}

	status = hw->rom.calc_checksum(hw);
	if (status < 0)
		return status;

	checksum = (u16)(status & 0xffff);

	status = hw->rom.writew_sw(hw, TXGBE_EEPROM_CHECKSUM, checksum);

	return status;
}

