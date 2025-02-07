/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#include "e1000_api.h"

static s32 e1000_init_nvm_params_i225(struct e1000_hw *hw);
static s32 e1000_init_mac_params_i225(struct e1000_hw *hw);
static s32 e1000_init_phy_params_i225(struct e1000_hw *hw);
static s32 e1000_reset_hw_i225(struct e1000_hw *hw);
static s32 e1000_acquire_nvm_i225(struct e1000_hw *hw);
static void e1000_release_nvm_i225(struct e1000_hw *hw);
static s32 e1000_get_hw_semaphore_i225(struct e1000_hw *hw);
static s32 __e1000_write_nvm_srwr(struct e1000_hw *hw, u16 offset, u16 words,
				  u16 *data);
static s32 e1000_pool_flash_update_done_i225(struct e1000_hw *hw);
static s32 e1000_valid_led_default_i225(struct e1000_hw *hw, u16 *data);

/**
 *  e1000_init_nvm_params_i225 - Init NVM func ptrs.
 *  @hw: pointer to the HW structure
 **/
static s32 e1000_init_nvm_params_i225(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = E1000_READ_REG(hw, E1000_EECD);
	u16 size;

	DEBUGFUNC("e1000_init_nvm_params_i225");

	size = (u16)((eecd & E1000_EECD_SIZE_EX_MASK) >>
		     E1000_EECD_SIZE_EX_SHIFT);
	/*
	 * Added to a constant, "size" becomes the left-shift value
	 * for setting word_size.
	 */
	size += NVM_WORD_SIZE_BASE_SHIFT;

	/* Just in case size is out of range, cap it to the largest
	 * EEPROM size supported
	 */
	if (size > 15)
		size = 15;

	nvm->word_size = 1 << size;
	nvm->opcode_bits = 8;
	nvm->delay_usec = 1;
	nvm->type = e1000_nvm_eeprom_spi;


	nvm->page_size = eecd & E1000_EECD_ADDR_BITS ? 32 : 8;
	nvm->address_bits = eecd & E1000_EECD_ADDR_BITS ?
			    16 : 8;

	if (nvm->word_size == (1 << 15))
		nvm->page_size = 128;

	nvm->ops.acquire = e1000_acquire_nvm_i225;
	nvm->ops.release = e1000_release_nvm_i225;
	nvm->ops.valid_led_default = e1000_valid_led_default_i225;
	if (e1000_get_flash_presence_i225(hw)) {
		hw->nvm.type = e1000_nvm_flash_hw;
		nvm->ops.read    = e1000_read_nvm_srrd_i225;
		nvm->ops.write   = e1000_write_nvm_srwr_i225;
		nvm->ops.validate = e1000_validate_nvm_checksum_i225;
		nvm->ops.update   = e1000_update_nvm_checksum_i225;
	} else {
		hw->nvm.type = e1000_nvm_none;
		nvm->ops.write    = e1000_null_write_nvm;
		nvm->ops.validate = e1000_null_ops_generic;
		nvm->ops.update   = e1000_null_ops_generic;
	}

	return E1000_SUCCESS;
}

/**
 *  e1000_init_mac_params_i225 - Init MAC func ptrs.
 *  @hw: pointer to the HW structure
 **/
static s32 e1000_init_mac_params_i225(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	struct e1000_dev_spec_i225 *dev_spec = &hw->dev_spec._i225;

	DEBUGFUNC("e1000_init_mac_params_i225");

	/* Initialize function pointer */
	e1000_init_mac_ops_generic(hw);

	/* Set media type */
	hw->phy.media_type = e1000_media_type_copper;
	/* Set mta register count */
	mac->mta_reg_count = 128;
	/* Set rar entry count */
	mac->rar_entry_count = E1000_RAR_ENTRIES_BASE;
	/* Set EEE */
	mac->ops.set_eee = e1000_set_eee_i225;
	/* bus type/speed/width */
	mac->ops.get_bus_info = e1000_get_bus_info_pcie_generic;
	/* reset */
	mac->ops.reset_hw = e1000_reset_hw_i225;
	/* hw initialization */
	mac->ops.init_hw = e1000_init_hw_i225;
	/* link setup */
	mac->ops.setup_link = e1000_setup_link_generic;
	/* check for link */
	mac->ops.check_for_link = e1000_check_for_link_i225;
	/* link info */
	mac->ops.get_link_up_info = e1000_get_speed_and_duplex_copper_generic;
	/* acquire SW_FW sync */
	mac->ops.acquire_swfw_sync = e1000_acquire_swfw_sync_i225;
	/* release SW_FW sync */
	mac->ops.release_swfw_sync = e1000_release_swfw_sync_i225;

	/* Allow a single clear of the SW semaphore on I225 */
	dev_spec->clear_semaphore_once = true;
	mac->ops.setup_physical_interface = e1000_setup_copper_link_i225;

	/* Set if part includes ASF firmware */
	mac->asf_firmware_present = true;

	/* multicast address update */
	mac->ops.update_mc_addr_list = e1000_update_mc_addr_list_generic;

	mac->ops.write_vfta = e1000_write_vfta_generic;

	/* LED */
	mac->ops.cleanup_led = e1000_cleanup_led_generic;
	mac->ops.id_led_init = e1000_id_led_init_i225;
	mac->ops.blink_led = e1000_blink_led_i225;

	/* Disable EEE by default */
	dev_spec->eee_disable = true;

	return E1000_SUCCESS;
}

/**
 *  e1000_init_phy_params_i225 - Init PHY func ptrs.
 *  @hw: pointer to the HW structure
 **/
static s32 e1000_init_phy_params_i225(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	u16 phy_id_retries = 1, phy_id_retries_interval_ms = 100;
	s32 ret_val = E1000_SUCCESS;
	u32 ctrl_ext;
	u16 i = 0;

	DEBUGFUNC("e1000_init_phy_params_i225");

	phy->ops.read_i2c_byte = e1000_read_i2c_byte_generic;
	phy->ops.write_i2c_byte = e1000_write_i2c_byte_generic;

	if (hw->phy.media_type != e1000_media_type_copper) {
		phy->type = e1000_phy_none;
		goto out;
	}

	phy->ops.power_up   = e1000_power_up_phy_copper;
	phy->ops.power_down = e1000_power_down_phy_copper_base;

	phy->autoneg_mask = AUTONEG_ADVERTISE_SPEED_DEFAULT_2500;

	phy->reset_delay_us	= 100;

	phy->ops.acquire	= e1000_acquire_phy_base;
	phy->ops.check_reset_block = e1000_check_reset_block_generic;
	phy->ops.commit		= e1000_phy_sw_reset_generic;
	phy->ops.release	= e1000_release_phy_base;
	phy->ops.reset		= e1000_phy_hw_reset_generic;

	ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);

	/* Make sure the PHY is in a good state. Several people have reported
	 * firmware leaving the PHY's page select register set to something
	 * other than the default of zero, which causes the PHY ID read to
	 * access something other than the intended register.
	 */
	ret_val = hw->phy.ops.reset(hw);
	if (ret_val)
		goto out;

	E1000_WRITE_REG(hw, E1000_CTRL_EXT, ctrl_ext);
	phy->ops.read_reg = e1000_read_phy_reg_gpy;
	phy->ops.write_reg = e1000_write_phy_reg_gpy;

	if (hw->dev_spec._i225.wait_for_valid_phy_id_read) {
		phy_id_retries = 10;
		DEBUGOUT1("Going to wait %d ms for a valid PHY ID read...\n",
			  phy_id_retries * phy_id_retries_interval_ms);
	}

	for (i = 0; i < phy_id_retries; i++) {
		ret_val = e1000_get_phy_id(hw);
		phy->id &= I225_I_PHY_ID_MASK;
		DEBUGOUT1("PHY ID value = 0x%08x\n", phy->id);

		/*
		* IGC/IGB merge note: in base code, there was a PHY ID check for
		* I225 at this point. However, in DPDK version of IGC this check
		* was removed because it interfered with some i225-based NICs,
		* and it was deemed unnecessary because only the i225 NIC
		* would've called this code anyway because it was in the IGC
		* driver.
		*
		* This code was then amended apparently because there were
		* issues wih reading PHY ID on some platforms, and PHY ID read
		* was to be retried multiple times. However, the original fix in
		* the base code still had the PHY ID check, and it is now
		* necessary because there is a chance to read an invalid PHY ID.
		* So, in backport, it was decided to replace the equality check
		* with a non-zero condition, because the "invalid" PHY ID as
		* reported in the original issue, was 0. So, as long as we read
		* a non-zero PHY ID, we consider it valid.
		*/
		if (phy->id != 0) {
			phy->type = e1000_phy_i225;
			phy->ops.set_d0_lplu_state = e1000_set_d0_lplu_state_i225;
			phy->ops.set_d3_lplu_state = e1000_set_d3_lplu_state_i225;

			return ret_val;
		}

		msec_delay(phy_id_retries_interval_ms);
	}

	DEBUGOUT("Failed to read PHY ID\n");
	ret_val = E1000_ERR_PHY;
out:
	return ret_val;
}

/**
 *  e1000_reset_hw_i225 - Reset hardware
 *  @hw: pointer to the HW structure
 *
 *  This resets the hardware into a known state.
 **/
static s32 e1000_reset_hw_i225(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;

	DEBUGFUNC("e1000_reset_hw_i225");

	/*
	 * Prevent the PCI-E bus from sticking if there is no TLP connection
	 * on the last TLP read/write transaction when MAC is reset.
	 */
	ret_val = e1000_disable_pcie_primary_generic(hw);
	if (ret_val)
		DEBUGOUT("PCI-E Primary disable polling has failed.\n");

	DEBUGOUT("Masking off all interrupts\n");
	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);

	E1000_WRITE_REG(hw, E1000_RCTL, 0);
	E1000_WRITE_REG(hw, E1000_TCTL, E1000_TCTL_PSP);
	E1000_WRITE_FLUSH(hw);

	msec_delay(10);

	ctrl = E1000_READ_REG(hw, E1000_CTRL);

	DEBUGOUT("Issuing a global reset to MAC\n");
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl | E1000_CTRL_RST);

	ret_val = e1000_get_auto_rd_done_generic(hw);
	if (ret_val) {
		/*
		 * When auto config read does not complete, do not
		 * return with an error. This can happen in situations
		 * where there is no eeprom and prevents getting link.
		 */
		DEBUGOUT("Auto Read Done did not complete\n");
	}

	/* Clear any pending interrupt events. */
	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);
	E1000_READ_REG(hw, E1000_ICR);

	/* Install any alternate MAC address into RAR0 */
	ret_val = e1000_check_alt_mac_addr_generic(hw);

	return ret_val;
}

/* e1000_acquire_nvm_i225 - Request for access to EEPROM
 * @hw: pointer to the HW structure
 *
 * Acquire the necessary semaphores for exclusive access to the EEPROM.
 * Set the EEPROM access request bit and wait for EEPROM access grant bit.
 * Return successful if access grant bit set, else clear the request for
 * EEPROM access and return -E1000_ERR_NVM (-1).
 */
static s32 e1000_acquire_nvm_i225(struct e1000_hw *hw)
{
	s32 ret_val;

	DEBUGFUNC("e1000_acquire_nvm_i225");

	ret_val = e1000_acquire_swfw_sync_i225(hw, E1000_SWFW_EEP_SM);

	return ret_val;
}

/* e1000_release_nvm_i225 - Release exclusive access to EEPROM
 * @hw: pointer to the HW structure
 *
 * Stop any current commands to the EEPROM and clear the EEPROM request bit,
 * then release the semaphores acquired.
 */
static void e1000_release_nvm_i225(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_release_nvm_i225");

	e1000_release_swfw_sync_i225(hw, E1000_SWFW_EEP_SM);
}

/* e1000_acquire_swfw_sync_i225 - Acquire SW/FW semaphore
 * @hw: pointer to the HW structure
 * @mask: specifies which semaphore to acquire
 *
 * Acquire the SW/FW semaphore to access the PHY or NVM.  The mask
 * will also specify which port we're acquiring the lock for.
 */
s32 e1000_acquire_swfw_sync_i225(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;
	u32 swmask = mask;
	u32 fwmask = mask << 16;
	s32 ret_val = E1000_SUCCESS;
	s32 i = 0, timeout = 200; /* FIXME: find real value to use here */

	DEBUGFUNC("e1000_acquire_swfw_sync_i225");

	while (i < timeout) {
		if (e1000_get_hw_semaphore_i225(hw)) {
			ret_val = -E1000_ERR_SWFW_SYNC;
			goto out;
		}

		swfw_sync = E1000_READ_REG(hw, E1000_SW_FW_SYNC);
		if (!(swfw_sync & (fwmask | swmask)))
			break;

		/* Firmware currently using resource (fwmask)
		 * or other software thread using resource (swmask)
		 */
		e1000_put_hw_semaphore_generic(hw);
		msec_delay_irq(5);
		i++;
	}

	if (i == timeout) {
		DEBUGOUT("Driver can't access resource, SW_FW_SYNC timeout.\n");
		ret_val = -E1000_ERR_SWFW_SYNC;
		goto out;
	}

	swfw_sync |= swmask;
	E1000_WRITE_REG(hw, E1000_SW_FW_SYNC, swfw_sync);

	e1000_put_hw_semaphore_generic(hw);

out:
	return ret_val;
}

/* e1000_release_swfw_sync_i225 - Release SW/FW semaphore
 * @hw: pointer to the HW structure
 * @mask: specifies which semaphore to acquire
 *
 * Release the SW/FW semaphore used to access the PHY or NVM.  The mask
 * will also specify which port we're releasing the lock for.
 */
void e1000_release_swfw_sync_i225(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;

	DEBUGFUNC("e1000_release_swfw_sync_i225");

	/* Releasing the resource requires first getting the HW semaphore.
	 * If we fail to get the semaphore, there is nothing we can do,
	 * except log an error and quit. We are not allowed to hang here
	 * indefinitely, as it may cause denial of service or system crash.
	 */
	if (e1000_get_hw_semaphore_i225(hw) != E1000_SUCCESS) {
		DEBUGOUT("Failed to release SW_FW_SYNC.\n");
		return;
	}

	swfw_sync = E1000_READ_REG(hw, E1000_SW_FW_SYNC);
	swfw_sync &= ~(u32)mask;
	E1000_WRITE_REG(hw, E1000_SW_FW_SYNC, swfw_sync);

	e1000_put_hw_semaphore_generic(hw);
}

/*
 * e1000_setup_copper_link_i225 - Configure copper link settings
 * @hw: pointer to the HW structure
 *
 * Configures the link for auto-neg or forced speed and duplex.  Then we check
 * for link, once link is established calls to configure collision distance
 * and flow control are called.
 */
s32 e1000_setup_copper_link_i225(struct e1000_hw *hw)
{
	u32 phpm_reg;
	s32 ret_val;
	u32 ctrl;

	DEBUGFUNC("e1000_setup_copper_link_i225");

	ctrl = E1000_READ_REG(hw, E1000_CTRL);
	ctrl |= E1000_CTRL_SLU;
	ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	E1000_WRITE_REG(hw, E1000_CTRL, ctrl);

	phpm_reg = E1000_READ_REG(hw, E1000_I225_PHPM);
	phpm_reg &= ~E1000_I225_PHPM_GO_LINKD;
	E1000_WRITE_REG(hw, E1000_I225_PHPM, phpm_reg);

	ret_val = e1000_setup_copper_link_generic(hw);

	return ret_val;
}

/* e1000_get_hw_semaphore_i225 - Acquire hardware semaphore
 * @hw: pointer to the HW structure
 *
 * Acquire the HW semaphore to access the PHY or NVM
 */
static s32 e1000_get_hw_semaphore_i225(struct e1000_hw *hw)
{
	u32 swsm;
	s32 timeout = hw->nvm.word_size + 1;
	s32 i = 0;

	DEBUGFUNC("e1000_get_hw_semaphore_i225");

	/* Get the SW semaphore */
	while (i < timeout) {
		swsm = E1000_READ_REG(hw, E1000_SWSM);
		if (!(swsm & E1000_SWSM_SMBI))
			break;

		usec_delay(50);
		i++;
	}

	if (i == timeout) {
		/* In rare circumstances, the SW semaphore may already be held
		 * unintentionally. Clear the semaphore once before giving up.
		 */
		if (hw->dev_spec._i225.clear_semaphore_once) {
			hw->dev_spec._i225.clear_semaphore_once = false;
			e1000_put_hw_semaphore_generic(hw);
			for (i = 0; i < timeout; i++) {
				swsm = E1000_READ_REG(hw, E1000_SWSM);
				if (!(swsm & E1000_SWSM_SMBI))
					break;

				usec_delay(50);
			}
		}

		/* If we do not have the semaphore here, we have to give up. */
		if (i == timeout) {
			DEBUGOUT("Driver can't access device -\n");
			DEBUGOUT("SMBI bit is set.\n");
			return -E1000_ERR_NVM;
		}
	}

	/* Get the FW semaphore. */
	for (i = 0; i < timeout; i++) {
		swsm = E1000_READ_REG(hw, E1000_SWSM);
		E1000_WRITE_REG(hw, E1000_SWSM, swsm | E1000_SWSM_SWESMBI);

		/* Semaphore acquired if bit latched */
		if (E1000_READ_REG(hw, E1000_SWSM) & E1000_SWSM_SWESMBI)
			break;

		usec_delay(50);
	}

	if (i == timeout) {
		/* Release semaphores */
		e1000_put_hw_semaphore_generic(hw);
		DEBUGOUT("Driver can't access the NVM\n");
		return -E1000_ERR_NVM;
	}

	return E1000_SUCCESS;
}

/* e1000_read_nvm_srrd_i225 - Reads Shadow Ram using EERD register
 * @hw: pointer to the HW structure
 * @offset: offset of word in the Shadow Ram to read
 * @words: number of words to read
 * @data: word read from the Shadow Ram
 *
 * Reads a 16 bit word from the Shadow Ram using the EERD register.
 * Uses necessary synchronization semaphores.
 */
s32 e1000_read_nvm_srrd_i225(struct e1000_hw *hw, u16 offset, u16 words,
			     u16 *data)
{
	s32 status = E1000_SUCCESS;
	u16 i, count;

	DEBUGFUNC("e1000_read_nvm_srrd_i225");

	/* We cannot hold synchronization semaphores for too long,
	 * because of forceful takeover procedure. However it is more efficient
	 * to read in bursts than synchronizing access for each word.
	 */
	for (i = 0; i < words; i += E1000_EERD_EEWR_MAX_COUNT) {
		count = (words - i) / E1000_EERD_EEWR_MAX_COUNT > 0 ?
			E1000_EERD_EEWR_MAX_COUNT : (words - i);
		if (hw->nvm.ops.acquire(hw) == E1000_SUCCESS) {
			status = e1000_read_nvm_eerd(hw, offset, count,
						     data + i);
			hw->nvm.ops.release(hw);
		} else {
			status = E1000_ERR_SWFW_SYNC;
		}

		if (status != E1000_SUCCESS)
			break;
	}

	return status;
}

/* e1000_write_nvm_srwr_i225 - Write to Shadow RAM using EEWR
 * @hw: pointer to the HW structure
 * @offset: offset within the Shadow RAM to be written to
 * @words: number of words to write
 * @data: 16 bit word(s) to be written to the Shadow RAM
 *
 * Writes data to Shadow RAM at offset using EEWR register.
 *
 * If e1000_update_nvm_checksum is not called after this function , the
 * data will not be committed to FLASH and also Shadow RAM will most likely
 * contain an invalid checksum.
 *
 * If error code is returned, data and Shadow RAM may be inconsistent - buffer
 * partially written.
 */
s32 e1000_write_nvm_srwr_i225(struct e1000_hw *hw, u16 offset, u16 words,
			      u16 *data)
{
	s32 status = E1000_SUCCESS;
	u16 i, count;

	DEBUGFUNC("e1000_write_nvm_srwr_i225");

	/* We cannot hold synchronization semaphores for too long,
	 * because of forceful takeover procedure. However it is more efficient
	 * to write in bursts than synchronizing access for each word.
	 */
	for (i = 0; i < words; i += E1000_EERD_EEWR_MAX_COUNT) {
		count = (words - i) / E1000_EERD_EEWR_MAX_COUNT > 0 ?
			E1000_EERD_EEWR_MAX_COUNT : (words - i);
		if (hw->nvm.ops.acquire(hw) == E1000_SUCCESS) {
			status = __e1000_write_nvm_srwr(hw, offset, count,
							data + i);
			hw->nvm.ops.release(hw);
		} else {
			status = E1000_ERR_SWFW_SYNC;
		}

		if (status != E1000_SUCCESS)
			break;
	}

	return status;
}

/* __e1000_write_nvm_srwr - Write to Shadow Ram using EEWR
 * @hw: pointer to the HW structure
 * @offset: offset within the Shadow Ram to be written to
 * @words: number of words to write
 * @data: 16 bit word(s) to be written to the Shadow Ram
 *
 * Writes data to Shadow Ram at offset using EEWR register.
 *
 * If e1000_update_nvm_checksum is not called after this function , the
 * Shadow Ram will most likely contain an invalid checksum.
 */
static s32 __e1000_write_nvm_srwr(struct e1000_hw *hw, u16 offset, u16 words,
				  u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 i, k, eewr = 0;
	u32 attempts = 100000;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("__e1000_write_nvm_srwr");

	/* A check for invalid values:  offset too large, too many words,
	 * too many words for the offset, and not enough words.
	 */
	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		DEBUGOUT("nvm parameter(s) out of bounds\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	for (i = 0; i < words; i++) {
		eewr = ((offset + i) << E1000_NVM_RW_ADDR_SHIFT) |
			(data[i] << E1000_NVM_RW_REG_DATA) |
			E1000_NVM_RW_REG_START;

		E1000_WRITE_REG(hw, E1000_SRWR, eewr);

		for (k = 0; k < attempts; k++) {
			if (E1000_NVM_RW_REG_DONE &
			    E1000_READ_REG(hw, E1000_SRWR)) {
				ret_val = E1000_SUCCESS;
				break;
			}
			usec_delay(5);
		}

		if (ret_val != E1000_SUCCESS) {
			DEBUGOUT("Shadow RAM write EEWR timed out\n");
			break;
		}
	}

out:
	return ret_val;
}

/* e1000_validate_nvm_checksum_i225 - Validate EEPROM checksum
 * @hw: pointer to the HW structure
 *
 * Calculates the EEPROM checksum by reading/adding each word of the EEPROM
 * and then verifies that the sum of the EEPROM is equal to 0xBABA.
 */
s32 e1000_validate_nvm_checksum_i225(struct e1000_hw *hw)
{
	s32 status = E1000_SUCCESS;
	s32 (*read_op_ptr)(struct e1000_hw *, u16, u16, u16 *);

	DEBUGFUNC("e1000_validate_nvm_checksum_i225");

	if (hw->nvm.ops.acquire(hw) == E1000_SUCCESS) {
		/* Replace the read function with semaphore grabbing with
		 * the one that skips this for a while.
		 * We have semaphore taken already here.
		 */
		read_op_ptr = hw->nvm.ops.read;
		hw->nvm.ops.read = e1000_read_nvm_eerd;

		status = e1000_validate_nvm_checksum_generic(hw);

		/* Revert original read operation. */
		hw->nvm.ops.read = read_op_ptr;

		hw->nvm.ops.release(hw);
	} else {
		status = E1000_ERR_SWFW_SYNC;
	}

	return status;
}

/* e1000_update_nvm_checksum_i225 - Update EEPROM checksum
 * @hw: pointer to the HW structure
 *
 * Updates the EEPROM checksum by reading/adding each word of the EEPROM
 * up to the checksum.  Then calculates the EEPROM checksum and writes the
 * value to the EEPROM. Next commit EEPROM data onto the Flash.
 */
s32 e1000_update_nvm_checksum_i225(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 checksum = 0;
	u16 i, nvm_data;

	DEBUGFUNC("e1000_update_nvm_checksum_i225");

	/* Read the first word from the EEPROM. If this times out or fails, do
	 * not continue or we could be in for a very long wait while every
	 * EEPROM read fails
	 */
	ret_val = e1000_read_nvm_eerd(hw, 0, 1, &nvm_data);
	if (ret_val != E1000_SUCCESS) {
		DEBUGOUT("EEPROM read failed\n");
		goto out;
	}

	if (hw->nvm.ops.acquire(hw) == E1000_SUCCESS) {
		/* Do not use hw->nvm.ops.write, hw->nvm.ops.read
		 * because we do not want to take the synchronization
		 * semaphores twice here.
		 */

		for (i = 0; i < NVM_CHECKSUM_REG; i++) {
			ret_val = e1000_read_nvm_eerd(hw, i, 1, &nvm_data);
			if (ret_val) {
				hw->nvm.ops.release(hw);
				DEBUGOUT("NVM Read Error while updating\n");
				DEBUGOUT("checksum.\n");
				goto out;
			}
			checksum += nvm_data;
		}
		checksum = (u16)NVM_SUM - checksum;
		ret_val = __e1000_write_nvm_srwr(hw, NVM_CHECKSUM_REG, 1,
						 &checksum);
		if (ret_val != E1000_SUCCESS) {
			hw->nvm.ops.release(hw);
			DEBUGOUT("NVM Write Error while updating checksum.\n");
			goto out;
		}

		hw->nvm.ops.release(hw);

		ret_val = e1000_update_flash_i225(hw);
	} else {
		ret_val = E1000_ERR_SWFW_SYNC;
	}
out:
	return ret_val;
}

/* e1000_get_flash_presence_i225 - Check if flash device is detected.
 * @hw: pointer to the HW structure
 */
bool e1000_get_flash_presence_i225(struct e1000_hw *hw)
{
	u32 eec = 0;
	bool ret_val = false;

	DEBUGFUNC("e1000_get_flash_presence_i225");

	eec = E1000_READ_REG(hw, E1000_EECD);

	if (eec & E1000_EECD_FLASH_DETECTED_I225)
		ret_val = true;

	return ret_val;
}

/* e1000_set_flsw_flash_burst_counter_i225 - sets FLSW NVM Burst
 * Counter in FLSWCNT register.
 *
 * @hw: pointer to the HW structure
 * @burst_counter: size in bytes of the Flash burst to read or write
 */
s32 e1000_set_flsw_flash_burst_counter_i225(struct e1000_hw *hw,
					    u32 burst_counter)
{
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_set_flsw_flash_burst_counter_i225");

	/* Validate input data */
	if (burst_counter < E1000_I225_SHADOW_RAM_SIZE) {
		/* Write FLSWCNT - burst counter */
		E1000_WRITE_REG(hw, E1000_I225_FLSWCNT, burst_counter);
	} else {
		ret_val = E1000_ERR_INVALID_ARGUMENT;
	}

	return ret_val;
}

/* e1000_write_erase_flash_command_i225 - write/erase to a sector
 * region on a given address.
 *
 * @hw: pointer to the HW structure
 * @opcode: opcode to be used for the write command
 * @address: the offset to write into the FLASH image
 */
s32 e1000_write_erase_flash_command_i225(struct e1000_hw *hw, u32 opcode,
					 u32 address)
{
	u32 flswctl = 0;
	s32 timeout = E1000_NVM_GRANT_ATTEMPTS;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_write_erase_flash_command_i225");

	flswctl = E1000_READ_REG(hw, E1000_I225_FLSWCTL);
	/* Polling done bit on FLSWCTL register */
	while (timeout) {
		if (flswctl & E1000_FLSWCTL_DONE)
			break;
		usec_delay(5);
		flswctl = E1000_READ_REG(hw, E1000_I225_FLSWCTL);
		timeout--;
	}

	if (!timeout) {
		DEBUGOUT("Flash transaction was not done\n");
		return -E1000_ERR_NVM;
	}

	/* Build and issue command on FLSWCTL register */
	flswctl = address | opcode;
	E1000_WRITE_REG(hw, E1000_I225_FLSWCTL, flswctl);

	/* Check if issued command is valid on FLSWCTL register */
	flswctl = E1000_READ_REG(hw, E1000_I225_FLSWCTL);
	if (!(flswctl & E1000_FLSWCTL_CMDV)) {
		DEBUGOUT("Write flash command failed\n");
		ret_val = E1000_ERR_INVALID_ARGUMENT;
	}

	return ret_val;
}

/* e1000_update_flash_i225 - Commit EEPROM to the flash
 * if fw_valid_bit is set, FW is active. setting FLUPD bit in EEC
 * register makes the FW load the internal shadow RAM into the flash.
 * Otherwise, fw_valid_bit is 0. if FL_SECU.block_prtotected_sw = 0
 * then FW is not active so the SW is responsible shadow RAM dump.
 *
 * @hw: pointer to the HW structure
 */
s32 e1000_update_flash_i225(struct e1000_hw *hw)
{
	u16 current_offset_data = 0;
	u32 block_sw_protect = 1;
	u16 base_address = 0x0;
	u32 i, fw_valid_bit;
	u16 current_offset;
	s32 ret_val = 0;
	u32 flup;

	DEBUGFUNC("e1000_update_flash_i225");

	block_sw_protect = E1000_READ_REG(hw, E1000_I225_FLSECU) &
					  E1000_FLSECU_BLK_SW_ACCESS_I225;
	fw_valid_bit = E1000_READ_REG(hw, E1000_FWSM) &
				      E1000_FWSM_FW_VALID_I225;
	if (fw_valid_bit) {
		ret_val = e1000_pool_flash_update_done_i225(hw);
		if (ret_val == -E1000_ERR_NVM) {
			DEBUGOUT("Flash update time out\n");
			goto out;
		}

		flup = E1000_READ_REG(hw, E1000_EECD) | E1000_EECD_FLUPD_I225;
		E1000_WRITE_REG(hw, E1000_EECD, flup);

		ret_val = e1000_pool_flash_update_done_i225(hw);
		if (ret_val == E1000_SUCCESS)
			DEBUGOUT("Flash update complete\n");
		else
			DEBUGOUT("Flash update time out\n");
	} else if (!block_sw_protect) {
		/* FW is not active and security protection is disabled.
		 * therefore, SW is in charge of shadow RAM dump.
		 * Check which sector is valid. if sector 0 is valid,
		 * base address remains 0x0. otherwise, sector 1 is
		 * valid and it's base address is 0x1000
		 */
		if (E1000_READ_REG(hw, E1000_EECD) & E1000_EECD_SEC1VAL_I225)
			base_address = 0x1000;

		/* Valid sector erase */
		ret_val = e1000_write_erase_flash_command_i225(hw,
						  E1000_I225_ERASE_CMD_OPCODE,
						  base_address);
		if (!ret_val) {
			DEBUGOUT("Sector erase failed\n");
			goto out;
		}

		current_offset = base_address;

		/* Write */
		for (i = 0; i < E1000_I225_SHADOW_RAM_SIZE / 2; i++) {
			/* Set burst write length */
			ret_val = e1000_set_flsw_flash_burst_counter_i225(hw,
									  0x2);
			if (ret_val != E1000_SUCCESS)
				break;

			/* Set address and opcode */
			ret_val = e1000_write_erase_flash_command_i225(hw,
						E1000_I225_WRITE_CMD_OPCODE,
						2 * current_offset);
			if (ret_val != E1000_SUCCESS)
				break;

			ret_val = e1000_read_nvm_eerd(hw, current_offset,
						      1, &current_offset_data);
			if (ret_val) {
				DEBUGOUT("Failed to read from EEPROM\n");
				goto out;
			}

			/* Write CurrentOffseData to FLSWDATA register */
			E1000_WRITE_REG(hw, E1000_I225_FLSWDATA,
					current_offset_data);
			current_offset++;

			/* Wait till operation has finished */
			ret_val = e1000_poll_eerd_eewr_done(hw,
						E1000_NVM_POLL_READ);
			if (ret_val)
				break;

			usec_delay(1000);
		}
	}
out:
	return ret_val;
}

/* e1000_pool_flash_update_done_i225 - Pool FLUDONE status.
 * @hw: pointer to the HW structure
 */
s32 e1000_pool_flash_update_done_i225(struct e1000_hw *hw)
{
	s32 ret_val = -E1000_ERR_NVM;
	u32 i, reg;

	DEBUGFUNC("e1000_pool_flash_update_done_i225");

	for (i = 0; i < E1000_FLUDONE_ATTEMPTS; i++) {
		reg = E1000_READ_REG(hw, E1000_EECD);
		if (reg & E1000_EECD_FLUDONE_I225) {
			ret_val = E1000_SUCCESS;
			break;
		}
		usec_delay(5);
	}

	return ret_val;
}

/* e1000_set_ltr_i225 - Set Latency Tolerance Reporting thresholds.
 * @hw: pointer to the HW structure
 * @link: bool indicating link status
 *
 * Set the LTR thresholds based on the link speed (Mbps), EEE, and DMAC
 * settings, otherwise specify that there is no LTR requirement.
 */
s32 e1000_set_ltr_i225(struct e1000_hw *hw, bool link)
{
	u16 speed, duplex;
	u32 tw_system, ltrc, ltrv, ltr_min, ltr_max, scale_min, scale_max;
	s32 size;

	DEBUGFUNC("e1000_set_ltr_i225");

	/* If we do not have link, LTR thresholds are zero. */
	if (link) {
		hw->mac.ops.get_link_up_info(hw, &speed, &duplex);

		/* Check if using copper interface with EEE enabled or if the
		 * link speed is 10 Mbps.
		 */
		if ((hw->phy.media_type == e1000_media_type_copper) &&
		    !(hw->dev_spec._i225.eee_disable) &&
		     (speed != SPEED_10)) {
			/* EEE enabled, so send LTRMAX threshold. */
			ltrc = E1000_READ_REG(hw, E1000_LTRC) |
				E1000_LTRC_EEEMS_EN;
			E1000_WRITE_REG(hw, E1000_LTRC, ltrc);

			/* Calculate tw_system (nsec). */
			if (speed == SPEED_100) {
				tw_system = ((E1000_READ_REG(hw, E1000_EEE_SU) &
					     E1000_TW_SYSTEM_100_MASK) >>
					     E1000_TW_SYSTEM_100_SHIFT) * 500;
			} else {
				tw_system = (E1000_READ_REG(hw, E1000_EEE_SU) &
					     E1000_TW_SYSTEM_1000_MASK) * 500;
			}
		} else {
			tw_system = 0;
		}

		/* Get the Rx packet buffer size. */
		size = E1000_READ_REG(hw, E1000_RXPBS) &
			E1000_RXPBS_SIZE_I225_MASK;

		/* Calculations vary based on DMAC settings. */
		if (E1000_READ_REG(hw, E1000_DMACR) & E1000_DMACR_DMAC_EN) {
			size -= (E1000_READ_REG(hw, E1000_DMACR) &
				 E1000_DMACR_DMACTHR_MASK) >>
				 E1000_DMACR_DMACTHR_SHIFT;
			/* Convert size to bits. */
			size *= 1024 * 8;
		} else {
			/* Convert size to bytes, subtract the MTU, and then
			 * convert the size to bits.
			 */
			size *= 1024;
			size -= hw->dev_spec._i225.mtu;
			size *= 8;
		}

		if (size < 0) {
			DEBUGOUT1("Invalid effective Rx buffer size %d\n",
				  size);
			return -E1000_ERR_CONFIG;
		}

		/* Calculate the thresholds. Since speed is in Mbps, simplify
		 * the calculation by multiplying size/speed by 1000 for result
		 * to be in nsec before dividing by the scale in nsec. Set the
		 * scale such that the LTR threshold fits in the register.
		 */
		ltr_min = (1000 * size) / speed;
		ltr_max = ltr_min + tw_system;
		scale_min = (ltr_min / 1024) < 1024 ? E1000_LTRMINV_SCALE_1024 :
			    E1000_LTRMINV_SCALE_32768;
		scale_max = (ltr_max / 1024) < 1024 ? E1000_LTRMAXV_SCALE_1024 :
			    E1000_LTRMAXV_SCALE_32768;
		ltr_min /= scale_min == E1000_LTRMINV_SCALE_1024 ? 1024 : 32768;
		ltr_max /= scale_max == E1000_LTRMAXV_SCALE_1024 ? 1024 : 32768;

		/* Only write the LTR thresholds if they differ from before. */
		ltrv = E1000_READ_REG(hw, E1000_LTRMINV);
		if (ltr_min != (ltrv & E1000_LTRMINV_LTRV_MASK)) {
			ltrv = E1000_LTRMINV_LSNP_REQ | ltr_min |
			      (scale_min << E1000_LTRMINV_SCALE_SHIFT);
			E1000_WRITE_REG(hw, E1000_LTRMINV, ltrv);
		}

		ltrv = E1000_READ_REG(hw, E1000_LTRMAXV);
		if (ltr_max != (ltrv & E1000_LTRMAXV_LTRV_MASK)) {
			ltrv = E1000_LTRMAXV_LSNP_REQ | ltr_max |
			      (scale_min << E1000_LTRMAXV_SCALE_SHIFT);
			E1000_WRITE_REG(hw, E1000_LTRMAXV, ltrv);
		}
	}

	return E1000_SUCCESS;
}

/* e1000_check_for_link_i225 - Check for link
 * @hw: pointer to the HW structure
 *
 * Checks to see of the link status of the hardware has changed.  If a
 * change in link status has been detected, then we read the PHY registers
 * to get the current speed/duplex if link exists.
 */
s32 e1000_check_for_link_i225(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	bool link = false;

	DEBUGFUNC("e1000_check_for_link_i225");

	/* We only want to go out to the PHY registers to see if
	 * Auto-Neg has completed and/or if our link status has
	 * changed.  The get_link_status flag is set upon receiving
	 * a Link Status Change or Rx Sequence Error interrupt.
	 */
	if (!mac->get_link_status) {
		ret_val = E1000_SUCCESS;
		goto out;
	}

	/* First we want to see if the MII Status Register reports
	 * link.  If so, then we want to get the current speed/duplex
	 * of the PHY.
	 */
	ret_val = e1000_phy_has_link_generic(hw, 1, 0, &link);
	if (ret_val)
		goto out;

	if (!link)
		goto out; /* No link detected */

	mac->get_link_status = false;

	/* Check if there was DownShift, must be checked
	 * immediately after link-up
	 */
	e1000_check_downshift_generic(hw);

	/* If we are forcing speed/duplex, then we simply return since
	 * we have already determined whether we have link or not.
	 */
	if (!mac->autoneg)
		goto out;

	/* Auto-Neg is enabled.  Auto Speed Detection takes care
	 * of MAC speed/duplex configuration.  So we only need to
	 * configure Collision Distance in the MAC.
	 */
	mac->ops.config_collision_dist(hw);

	/* Configure Flow Control now that Auto-Neg has completed.
	 * First, we need to restore the desired flow control
	 * settings because we may have had to re-autoneg with a
	 * different link partner.
	 */
	ret_val = e1000_config_fc_after_link_up_generic(hw);
	if (ret_val)
		DEBUGOUT("Error configuring flow control\n");
out:
	/* Now that we are aware of our link settings, we can set the LTR
	 * thresholds.
	 */
	ret_val = e1000_set_ltr_i225(hw, link);

	return ret_val;
}

/* e1000_init_function_pointers_i225 - Init func ptrs.
 * @hw: pointer to the HW structure
 *
 * Called to initialize all function pointers and parameters.
 */
void e1000_init_function_pointers_i225(struct e1000_hw *hw)
{
	e1000_init_mac_ops_generic(hw);
	e1000_init_phy_ops_generic(hw);
	e1000_init_nvm_ops_generic(hw);
	hw->mac.ops.init_params = e1000_init_mac_params_i225;
	hw->nvm.ops.init_params = e1000_init_nvm_params_i225;
	hw->phy.ops.init_params = e1000_init_phy_params_i225;
}

/* e1000_valid_led_default_i225 - Verify a valid default LED config
 * @hw: pointer to the HW structure
 * @data: pointer to the NVM (EEPROM)
 *
 * Read the EEPROM for the current default LED configuration.  If the
 * LED configuration is not valid, set to a valid LED configuration.
 */
static s32 e1000_valid_led_default_i225(struct e1000_hw *hw, u16 *data)
{
	s32 ret_val;

	DEBUGFUNC("e1000_valid_led_default_i225");

	ret_val = hw->nvm.ops.read(hw, NVM_ID_LED_SETTINGS, 1, data);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		goto out;
	}

	if (*data == ID_LED_RESERVED_0000 || *data == ID_LED_RESERVED_FFFF) {
		switch (hw->phy.media_type) {
		case e1000_media_type_internal_serdes:
			*data = ID_LED_DEFAULT_I225_SERDES;
			break;
		case e1000_media_type_copper:
		default:
			*data = ID_LED_DEFAULT_I225;
			break;
		}
	}
out:
	return ret_val;
}

/* e1000_get_cfg_done_i225 - Read config done bit
 * @hw: pointer to the HW structure
 *
 * Read the management control register for the config done bit for
 * completion status.  NOTE: silicon which is EEPROM-less will fail trying
 * to read the config done bit, so an error is *ONLY* logged and returns
 * E1000_SUCCESS.  If we were to return with error, EEPROM-less silicon
 * would not be able to be reset or change link.
 */
static s32 e1000_get_cfg_done_i225(struct e1000_hw *hw)
{
	s32 timeout = PHY_CFG_TIMEOUT;
	u32 mask = E1000_NVM_CFG_DONE_PORT_0;

	DEBUGFUNC("e1000_get_cfg_done_i225");

	while (timeout) {
		if (E1000_READ_REG(hw, E1000_EEMNGCTL_I225) & mask)
			break;
		msec_delay(1);
		timeout--;
	}
	if (!timeout)
		DEBUGOUT("MNG configuration cycle has not completed.\n");

	return E1000_SUCCESS;
}

/* e1000_init_hw_i225 - Init hw for I225
 * @hw: pointer to the HW structure
 *
 * Called to initialize hw for i225 hw family.
 */
s32 e1000_init_hw_i225(struct e1000_hw *hw)
{
	s32 ret_val;

	DEBUGFUNC("e1000_init_hw_i225");

	hw->phy.ops.get_cfg_done = e1000_get_cfg_done_i225;
	ret_val = e1000_init_hw_base(hw);
	e1000_set_eee_i225(hw, false, false, false);
	return ret_val;
}

/*
 * e1000_set_d0_lplu_state_i225 - Set Low-Power-Link-Up (LPLU) D0 state
 * @hw: pointer to the HW structure
 * @active: true to enable LPLU, false to disable
 *
 * Note: since I225 does not actually support LPLU, this function
 * simply enables/disables 1G and 2.5G speeds in D0.
 */
s32 e1000_set_d0_lplu_state_i225(struct e1000_hw *hw, bool active)
{
	u32 data;

	DEBUGFUNC("e1000_set_d0_lplu_state_i225");

	data = E1000_READ_REG(hw, E1000_I225_PHPM);

	if (active) {
		data |= E1000_I225_PHPM_DIS_1000;
		data |= E1000_I225_PHPM_DIS_2500;
	} else {
		data &= ~E1000_I225_PHPM_DIS_1000;
		data &= ~E1000_I225_PHPM_DIS_2500;
	}

	E1000_WRITE_REG(hw, E1000_I225_PHPM, data);
	return E1000_SUCCESS;
}

/*
 * e1000_set_d3_lplu_state_i225 - Set Low-Power-Link-Up (LPLU) D3 state
 * @hw: pointer to the HW structure
 * @active: true to enable LPLU, false to disable
 *
 * Note: since I225 does not actually support LPLU, this function
 * simply enables/disables 100M, 1G and 2.5G speeds in D3.
 */
s32 e1000_set_d3_lplu_state_i225(struct e1000_hw *hw, bool active)
{
	u32 data;

	DEBUGFUNC("e1000_set_d3_lplu_state_i225");

	data = E1000_READ_REG(hw, E1000_I225_PHPM);

	if (active) {
		data |= E1000_I225_PHPM_DIS_100_D3;
		data |= E1000_I225_PHPM_DIS_1000_D3;
		data |= E1000_I225_PHPM_DIS_2500_D3;
	} else {
		data &= ~E1000_I225_PHPM_DIS_100_D3;
		data &= ~E1000_I225_PHPM_DIS_1000_D3;
		data &= ~E1000_I225_PHPM_DIS_2500_D3;
	}

	E1000_WRITE_REG(hw, E1000_I225_PHPM, data);
	return E1000_SUCCESS;
}

/**
 *  e1000_blink_led_i225 - Blink SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  This starts the adapter LED blinking.
 *  Request the LED to be setup first.
 **/
s32 e1000_blink_led_i225(struct e1000_hw *hw)
{
	u32 blink = 0;

	DEBUGFUNC("e1000_blink_led_i225");

	e1000_id_led_init_i225(hw);

	blink = hw->mac.ledctl_default;
	blink &= ~(E1000_GLOBAL_BLINK_MODE | E1000_LED1_MODE_MASK | E1000_LED2_MODE_MASK);
	blink |= E1000_LED1_BLINK;

	E1000_WRITE_REG(hw, E1000_LEDCTL, blink);

	return E1000_SUCCESS;
}

/**
 *  e1000_id_led_init_i225 - store LED configurations in SW
 *  @hw: pointer to the HW structure
 *
 *  Initializes the LED config in SW.
 **/
s32 e1000_id_led_init_i225(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_id_led_init_i225");

	hw->mac.ledctl_default = E1000_READ_REG(hw, E1000_LEDCTL);

	return E1000_SUCCESS;
}

/**
 *  e1000_set_eee_i225 - Enable/disable EEE support
 *  @hw: pointer to the HW structure
 *  @adv2p5G: boolean flag enabling 2.5G EEE advertisement
 *  @adv1G: boolean flag enabling 1G EEE advertisement
 *  @adv100M: boolean flag enabling 100M EEE advertisement
 *
 *  Enable/disable EEE based on setting in dev_spec structure.
 *
 **/
s32 e1000_set_eee_i225(struct e1000_hw *hw, bool adv2p5G, bool adv1G,
		       bool adv100M)
{
	u32 ipcnfg, eeer;

	DEBUGFUNC("e1000_set_eee_i225");

	if (hw->mac.type != e1000_i225 ||
	    hw->phy.media_type != e1000_media_type_copper)
		goto out;
	ipcnfg = E1000_READ_REG(hw, E1000_IPCNFG);
	eeer = E1000_READ_REG(hw, E1000_EEER);

	/* enable or disable per user setting */
	if (!(hw->dev_spec._i225.eee_disable)) {
		u32 eee_su = E1000_READ_REG(hw, E1000_EEE_SU);

		if (adv100M)
			ipcnfg |= E1000_IPCNFG_EEE_100M_AN;
		else
			ipcnfg &= ~E1000_IPCNFG_EEE_100M_AN;

		if (adv1G)
			ipcnfg |= E1000_IPCNFG_EEE_1G_AN;
		else
			ipcnfg &= ~E1000_IPCNFG_EEE_1G_AN;

		if (adv2p5G)
			ipcnfg |= E1000_IPCNFG_EEE_2_5G_AN;
		else
			ipcnfg &= ~E1000_IPCNFG_EEE_2_5G_AN;

		eeer |= (E1000_EEER_TX_LPI_EN | E1000_EEER_RX_LPI_EN |
			E1000_EEER_LPI_FC);

		/* This bit should not be set in normal operation. */
		if (eee_su & E1000_EEE_SU_LPI_CLK_STP)
			DEBUGOUT("LPI Clock Stop Bit should not be set!\n");
	} else {
		ipcnfg &= ~(E1000_IPCNFG_EEE_2_5G_AN | E1000_IPCNFG_EEE_1G_AN |
			E1000_IPCNFG_EEE_100M_AN);
		eeer &= ~(E1000_EEER_TX_LPI_EN | E1000_EEER_RX_LPI_EN |
			E1000_EEER_LPI_FC);
	}
	E1000_WRITE_REG(hw, E1000_IPCNFG, ipcnfg);
	E1000_WRITE_REG(hw, E1000_EEER, eeer);
	E1000_READ_REG(hw, E1000_IPCNFG);
	E1000_READ_REG(hw, E1000_EEER);
out:

	return E1000_SUCCESS;
}
