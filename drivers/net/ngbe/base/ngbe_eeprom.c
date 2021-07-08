/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ngbe_hw.h"
#include "ngbe_mng.h"
#include "ngbe_eeprom.h"

/**
 *  ngbe_init_eeprom_params - Initialize EEPROM params
 *  @hw: pointer to hardware structure
 *
 *  Initializes the EEPROM parameters ngbe_rom_info within the
 *  ngbe_hw struct in order to set up EEPROM access.
 **/
s32 ngbe_init_eeprom_params(struct ngbe_hw *hw)
{
	struct ngbe_rom_info *eeprom = &hw->rom;
	u32 eec;
	u16 eeprom_size;

	DEBUGFUNC("ngbe_init_eeprom_params");

	if (eeprom->type != ngbe_eeprom_unknown)
		return 0;

	eeprom->type = ngbe_eeprom_none;
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
	eec = rd32(hw, NGBE_SPISTAT);
	if (!(eec & NGBE_SPISTAT_BPFLASH)) {
		eeprom->type = ngbe_eeprom_flash;

		/*
		 * SPI EEPROM is assumed here.  This code would need to
		 * change if a future EEPROM is not SPI.
		 */
		eeprom_size = 4096;
		eeprom->word_size = eeprom_size >> 1;
	}

	eeprom->address_bits = 16;
	eeprom->sw_addr = 0x80;

	DEBUGOUT("eeprom params: type = %d, size = %d, address bits: "
		  "%d %d\n", eeprom->type, eeprom->word_size,
		  eeprom->address_bits, eeprom->sw_addr);

	return 0;
}

/**
 *  ngbe_get_eeprom_semaphore - Get hardware semaphore
 *  @hw: pointer to hardware structure
 *
 *  Sets the hardware semaphores so EEPROM access can occur for bit-bang method
 **/
s32 ngbe_get_eeprom_semaphore(struct ngbe_hw *hw)
{
	s32 status = NGBE_ERR_EEPROM;
	u32 timeout = 2000;
	u32 i;
	u32 swsm;

	DEBUGFUNC("ngbe_get_eeprom_semaphore");


	/* Get SMBI software semaphore between device drivers first */
	for (i = 0; i < timeout; i++) {
		/*
		 * If the SMBI bit is 0 when we read it, then the bit will be
		 * set and we have the semaphore
		 */
		swsm = rd32(hw, NGBE_SWSEM);
		if (!(swsm & NGBE_SWSEM_PF)) {
			status = 0;
			break;
		}
		usec_delay(50);
	}

	if (i == timeout) {
		DEBUGOUT("Driver can't access the eeprom - SMBI Semaphore "
			 "not granted.\n");
		/*
		 * this release is particularly important because our attempts
		 * above to get the semaphore may have succeeded, and if there
		 * was a timeout, we should unconditionally clear the semaphore
		 * bits to free the driver to make progress
		 */
		ngbe_release_eeprom_semaphore(hw);

		usec_delay(50);
		/*
		 * one last try
		 * If the SMBI bit is 0 when we read it, then the bit will be
		 * set and we have the semaphore
		 */
		swsm = rd32(hw, NGBE_SWSEM);
		if (!(swsm & NGBE_SWSEM_PF))
			status = 0;
	}

	/* Now get the semaphore between SW/FW through the SWESMBI bit */
	if (status == 0) {
		for (i = 0; i < timeout; i++) {
			/* Set the SW EEPROM semaphore bit to request access */
			wr32m(hw, NGBE_MNGSWSYNC,
				NGBE_MNGSWSYNC_REQ, NGBE_MNGSWSYNC_REQ);

			/*
			 * If we set the bit successfully then we got the
			 * semaphore.
			 */
			swsm = rd32(hw, NGBE_MNGSWSYNC);
			if (swsm & NGBE_MNGSWSYNC_REQ)
				break;

			usec_delay(50);
		}

		/*
		 * Release semaphores and return error if SW EEPROM semaphore
		 * was not granted because we don't have access to the EEPROM
		 */
		if (i >= timeout) {
			DEBUGOUT("SWESMBI Software EEPROM semaphore not granted.\n");
			ngbe_release_eeprom_semaphore(hw);
			status = NGBE_ERR_EEPROM;
		}
	} else {
		DEBUGOUT("Software semaphore SMBI between device drivers "
			 "not granted.\n");
	}

	return status;
}

/**
 *  ngbe_release_eeprom_semaphore - Release hardware semaphore
 *  @hw: pointer to hardware structure
 *
 *  This function clears hardware semaphore bits.
 **/
void ngbe_release_eeprom_semaphore(struct ngbe_hw *hw)
{
	DEBUGFUNC("ngbe_release_eeprom_semaphore");

	wr32m(hw, NGBE_MNGSWSYNC, NGBE_MNGSWSYNC_REQ, 0);
	wr32m(hw, NGBE_SWSEM, NGBE_SWSEM_PF, 0);
	ngbe_flush(hw);
}

/**
 *  ngbe_validate_eeprom_checksum_em - Validate EEPROM checksum
 *  @hw: pointer to hardware structure
 *  @checksum_val: calculated checksum
 *
 *  Performs checksum calculation and validates the EEPROM checksum.  If the
 *  caller does not need checksum_val, the value can be NULL.
 **/
s32 ngbe_validate_eeprom_checksum_em(struct ngbe_hw *hw,
					   u16 *checksum_val)
{
	u32 eeprom_cksum_devcap = 0;
	int err = 0;

	DEBUGFUNC("ngbe_validate_eeprom_checksum_em");
	UNREFERENCED_PARAMETER(checksum_val);

	/* Check EEPROM only once */
	if (hw->bus.lan_id == 0) {
		wr32(hw, NGBE_CALSUM_CAP_STATUS, 0x0);
		wr32(hw, NGBE_EEPROM_VERSION_STORE_REG, 0x0);
	} else {
		eeprom_cksum_devcap = rd32(hw, NGBE_CALSUM_CAP_STATUS);
		hw->rom.saved_version = rd32(hw, NGBE_EEPROM_VERSION_STORE_REG);
	}

	if (hw->bus.lan_id == 0 || eeprom_cksum_devcap == 0) {
		err = ngbe_hic_check_cap(hw);
		if (err != 0) {
			PMD_INIT_LOG(ERR,
				"The EEPROM checksum is not valid: %d", err);
			return -EIO;
		}
	}

	hw->rom.cksum_devcap = eeprom_cksum_devcap & 0xffff;

	return err;
}

