/*******************************************************************************

Copyright (c) 2013 - 2014, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include "i40e_prototype.h"

/**
 * i40e_init_nvm_ops - Initialize NVM function pointers
 * @hw: pointer to the HW structure
 *
 * Setup the function pointers and the NVM info structure. Should be called
 * once per NVM initialization, e.g. inside the i40e_init_shared_code().
 * Please notice that the NVM term is used here (& in all methods covered
 * in this file) as an equivalent of the FLASH part mapped into the SR.
 * We are accessing FLASH always thru the Shadow RAM.
 **/
enum i40e_status_code i40e_init_nvm(struct i40e_hw *hw)
{
	struct i40e_nvm_info *nvm = &hw->nvm;
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u32 fla, gens;
	u8 sr_size;

	DEBUGFUNC("i40e_init_nvm");

	/* The SR size is stored regardless of the nvm programming mode
	 * as the blank mode may be used in the factory line.
	 */
	gens = rd32(hw, I40E_GLNVM_GENS);
	sr_size = ((gens & I40E_GLNVM_GENS_SR_SIZE_MASK) >>
			   I40E_GLNVM_GENS_SR_SIZE_SHIFT);
	/* Switching to words (sr_size contains power of 2KB) */
	nvm->sr_size = (1 << sr_size) * I40E_SR_WORDS_IN_1KB;

	/* Check if we are in the normal or blank NVM programming mode */
	fla = rd32(hw, I40E_GLNVM_FLA);
	if (fla & I40E_GLNVM_FLA_LOCKED_MASK) { /* Normal programming mode */
		/* Max NVM timeout */
		nvm->timeout = I40E_MAX_NVM_TIMEOUT;
		nvm->blank_nvm_mode = false;
	} else { /* Blank programming mode */
		nvm->blank_nvm_mode = true;
		ret_code = I40E_ERR_NVM_BLANK_MODE;
		DEBUGOUT("NVM init error: unsupported blank mode.\n");
	}

	return ret_code;
}

/**
 * i40e_acquire_nvm - Generic request for acquiring the NVM ownership
 * @hw: pointer to the HW structure
 * @access: NVM access type (read or write)
 *
 * This function will request NVM ownership for reading
 * via the proper Admin Command.
 **/
enum i40e_status_code i40e_acquire_nvm(struct i40e_hw *hw,
				       enum i40e_aq_resource_access_type access)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u64 gtime, timeout;
	u64 time = 0;

	DEBUGFUNC("i40e_acquire_nvm");

	if (hw->nvm.blank_nvm_mode)
		goto i40e_i40e_acquire_nvm_exit;

	ret_code = i40e_aq_request_resource(hw, I40E_NVM_RESOURCE_ID, access,
					    0, &time, NULL);
	/* Reading the Global Device Timer */
	gtime = rd32(hw, I40E_GLVFGEN_TIMER);

	/* Store the timeout */
	hw->nvm.hw_semaphore_timeout = I40E_MS_TO_GTIME(time) + gtime;

	if (ret_code != I40E_SUCCESS) {
		/* Set the polling timeout */
		if (time > I40E_MAX_NVM_TIMEOUT)
			timeout = I40E_MS_TO_GTIME(I40E_MAX_NVM_TIMEOUT)
				  + gtime;
		else
			timeout = hw->nvm.hw_semaphore_timeout;
		/* Poll until the current NVM owner timeouts */
		while (gtime < timeout) {
			i40e_msec_delay(10);
			ret_code = i40e_aq_request_resource(hw,
							I40E_NVM_RESOURCE_ID,
							access, 0, &time,
							NULL);
			if (ret_code == I40E_SUCCESS) {
				hw->nvm.hw_semaphore_timeout =
						I40E_MS_TO_GTIME(time) + gtime;
				break;
			}
			gtime = rd32(hw, I40E_GLVFGEN_TIMER);
		}
		if (ret_code != I40E_SUCCESS) {
			hw->nvm.hw_semaphore_timeout = 0;
			hw->nvm.hw_semaphore_wait =
						I40E_MS_TO_GTIME(time) + gtime;
			DEBUGOUT1("NVM acquire timed out, wait %llu ms before trying again.\n",
				  time);
		}
	}

i40e_i40e_acquire_nvm_exit:
	return ret_code;
}

/**
 * i40e_release_nvm - Generic request for releasing the NVM ownership
 * @hw: pointer to the HW structure
 *
 * This function will release NVM resource via the proper Admin Command.
 **/
void i40e_release_nvm(struct i40e_hw *hw)
{
	DEBUGFUNC("i40e_release_nvm");

	if (!hw->nvm.blank_nvm_mode)
		i40e_aq_release_resource(hw, I40E_NVM_RESOURCE_ID, 0, NULL);
}

/**
 * i40e_poll_sr_srctl_done_bit - Polls the GLNVM_SRCTL done bit
 * @hw: pointer to the HW structure
 *
 * Polls the SRCTL Shadow RAM register done bit.
 **/
static enum i40e_status_code i40e_poll_sr_srctl_done_bit(struct i40e_hw *hw)
{
	enum i40e_status_code ret_code = I40E_ERR_TIMEOUT;
	u32 srctl, wait_cnt;

	DEBUGFUNC("i40e_poll_sr_srctl_done_bit");

	/* Poll the I40E_GLNVM_SRCTL until the done bit is set */
	for (wait_cnt = 0; wait_cnt < I40E_SRRD_SRCTL_ATTEMPTS; wait_cnt++) {
		srctl = rd32(hw, I40E_GLNVM_SRCTL);
		if (srctl & I40E_GLNVM_SRCTL_DONE_MASK) {
			ret_code = I40E_SUCCESS;
			break;
		}
		i40e_usec_delay(5);
	}
	if (ret_code == I40E_ERR_TIMEOUT)
		DEBUGOUT("Done bit in GLNVM_SRCTL not set");
	return ret_code;
}

/**
 * i40e_read_nvm_word - Reads Shadow RAM
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF)
 * @data: word read from the Shadow RAM
 *
 * Reads one 16 bit word from the Shadow RAM using the GLNVM_SRCTL register.
 **/
enum i40e_status_code i40e_read_nvm_word(struct i40e_hw *hw, u16 offset,
					 u16 *data)
{
	enum i40e_status_code ret_code = I40E_ERR_TIMEOUT;
	u32 sr_reg;

	DEBUGFUNC("i40e_read_nvm_srctl");

	if (offset >= hw->nvm.sr_size) {
		DEBUGOUT("NVM read error: Offset beyond Shadow RAM limit.\n");
		ret_code = I40E_ERR_PARAM;
		goto read_nvm_exit;
	}

	/* Poll the done bit first */
	ret_code = i40e_poll_sr_srctl_done_bit(hw);
	if (ret_code == I40E_SUCCESS) {
		/* Write the address and start reading */
		sr_reg = (u32)(offset << I40E_GLNVM_SRCTL_ADDR_SHIFT) |
			 (1 << I40E_GLNVM_SRCTL_START_SHIFT);
		wr32(hw, I40E_GLNVM_SRCTL, sr_reg);

		/* Poll I40E_GLNVM_SRCTL until the done bit is set */
		ret_code = i40e_poll_sr_srctl_done_bit(hw);
		if (ret_code == I40E_SUCCESS) {
			sr_reg = rd32(hw, I40E_GLNVM_SRDATA);
			*data = (u16)((sr_reg &
				       I40E_GLNVM_SRDATA_RDDATA_MASK)
				    >> I40E_GLNVM_SRDATA_RDDATA_SHIFT);
		}
	}
	if (ret_code != I40E_SUCCESS)
		DEBUGOUT1("NVM read error: Couldn't access Shadow RAM address: 0x%x\n",
			  offset);

read_nvm_exit:
	return ret_code;
}

/**
 * i40e_read_nvm_buffer - Reads Shadow RAM buffer
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to read (0x000000 - 0x001FFF).
 * @words: (in) number of words to read; (out) number of words actually read
 * @data: words read from the Shadow RAM
 *
 * Reads 16 bit words (data buffer) from the SR using the i40e_read_nvm_srrd()
 * method. The buffer read is preceded by the NVM ownership take
 * and followed by the release.
 **/
enum i40e_status_code i40e_read_nvm_buffer(struct i40e_hw *hw, u16 offset,
					   u16 *words, u16 *data)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u16 index, word;

	DEBUGFUNC("i40e_read_nvm_buffer");

	/* Loop thru the selected region */
	for (word = 0; word < *words; word++) {
		index = offset + word;
		ret_code = i40e_read_nvm_word(hw, index, &data[word]);
		if (ret_code != I40E_SUCCESS)
			break;
	}

	/* Update the number of words read from the Shadow RAM */
	*words = word;

	return ret_code;
}
/**
 * i40e_write_nvm_aq - Writes Shadow RAM.
 * @hw: pointer to the HW structure.
 * @module_pointer: module pointer location in words from the NVM beginning
 * @offset: offset in words from module start
 * @words: number of words to write
 * @data: buffer with words to write to the Shadow RAM
 * @last_command: tells the AdminQ that this is the last command
 *
 * Writes a 16 bit words buffer to the Shadow RAM using the admin command.
 **/
enum i40e_status_code i40e_write_nvm_aq(struct i40e_hw *hw, u8 module_pointer,
					u32 offset, u16 words, void *data,
					bool last_command)
{
	enum i40e_status_code ret_code = I40E_ERR_NVM;

	DEBUGFUNC("i40e_write_nvm_aq");

	/* Here we are checking the SR limit only for the flat memory model.
	 * We cannot do it for the module-based model, as we did not acquire
	 * the NVM resource yet (we cannot get the module pointer value).
	 * Firmware will check the module-based model.
	 */
	if ((offset + words) > hw->nvm.sr_size)
		DEBUGOUT("NVM write error: offset beyond Shadow RAM limit.\n");
	else if (words > I40E_SR_SECTOR_SIZE_IN_WORDS)
		/* We can write only up to 4KB (one sector), in one AQ write */
		DEBUGOUT("NVM write fail error: cannot write more than 4KB in a single write.\n");
	else if (((offset + (words - 1)) / I40E_SR_SECTOR_SIZE_IN_WORDS)
		 != (offset / I40E_SR_SECTOR_SIZE_IN_WORDS))
		/* A single write cannot spread over two sectors */
		DEBUGOUT("NVM write error: cannot spread over two sectors in a single write.\n");
	else
		ret_code = i40e_aq_update_nvm(hw, module_pointer,
					      2 * offset,  /*bytes*/
					      2 * words,   /*bytes*/
					      data, last_command, NULL);

	return ret_code;
}

/**
 * i40e_write_nvm_word - Writes Shadow RAM word
 * @hw: pointer to the HW structure
 * @offset: offset of the Shadow RAM word to write
 * @data: word to write to the Shadow RAM
 *
 * Writes a 16 bit word to the SR using the i40e_write_nvm_aq() method.
 * NVM ownership have to be acquired and released (on ARQ completion event
 * reception) by caller. To commit SR to NVM update checksum function
 * should be called.
 **/
enum i40e_status_code i40e_write_nvm_word(struct i40e_hw *hw, u32 offset,
					  void *data)
{
	DEBUGFUNC("i40e_write_nvm_word");

	/* Value 0x00 below means that we treat SR as a flat mem */
	return i40e_write_nvm_aq(hw, 0x00, offset, 1, data, false);
}

/**
 * i40e_write_nvm_buffer - Writes Shadow RAM buffer
 * @hw: pointer to the HW structure
 * @module_pointer: module pointer location in words from the NVM beginning
 * @offset: offset of the Shadow RAM buffer to write
 * @words: number of words to write
 * @data: words to write to the Shadow RAM
 *
 * Writes a 16 bit words buffer to the Shadow RAM using the admin command.
 * NVM ownership must be acquired before calling this function and released
 * on ARQ completion event reception by caller. To commit SR to NVM update
 * checksum function should be called.
 **/
enum i40e_status_code i40e_write_nvm_buffer(struct i40e_hw *hw,
					    u8 module_pointer, u32 offset,
					    u16 words, void *data)
{
	DEBUGFUNC("i40e_write_nvm_buffer");

	/* Here we will only write one buffer as the size of the modules
	 * mirrored in the Shadow RAM is always less than 4K.
	 */
	return i40e_write_nvm_aq(hw, module_pointer, offset, words,
				 data, false);
}

/**
 * i40e_calc_nvm_checksum - Calculates and returns the checksum
 * @hw: pointer to hardware structure
 * @checksum: pointer to the checksum
 *
 * This function calculates SW Checksum that covers the whole 64kB shadow RAM
 * except the VPD and PCIe ALT Auto-load modules. The structure and size of VPD
 * is customer specific and unknown. Therefore, this function skips all maximum
 * possible size of VPD (1kB).
 **/
enum i40e_status_code i40e_calc_nvm_checksum(struct i40e_hw *hw, u16 *checksum)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u16 pcie_alt_module = 0;
	u16 checksum_local = 0;
	u16 vpd_module = 0;
	u16 word = 0;
	u32 i = 0;

	DEBUGFUNC("i40e_calc_nvm_checksum");

	/* read pointer to VPD area */
	ret_code = i40e_read_nvm_word(hw, I40E_SR_VPD_PTR, &vpd_module);
	if (ret_code != I40E_SUCCESS) {
		ret_code = I40E_ERR_NVM_CHECKSUM;
		goto i40e_calc_nvm_checksum_exit;
	}

	/* read pointer to PCIe Alt Auto-load module */
	ret_code = i40e_read_nvm_word(hw, I40E_SR_PCIE_ALT_AUTO_LOAD_PTR,
				       &pcie_alt_module);
	if (ret_code != I40E_SUCCESS) {
		ret_code = I40E_ERR_NVM_CHECKSUM;
		goto i40e_calc_nvm_checksum_exit;
	}

	/* Calculate SW checksum that covers the whole 64kB shadow RAM
	 * except the VPD and PCIe ALT Auto-load modules
	 */
	for (i = 0; i < hw->nvm.sr_size; i++) {
		/* Skip Checksum word */
		if (i == I40E_SR_SW_CHECKSUM_WORD)
			i++;
		/* Skip VPD module (convert byte size to word count) */
		if (i == (u32)vpd_module) {
			i += (I40E_SR_VPD_MODULE_MAX_SIZE / 2);
			if (i >= hw->nvm.sr_size)
				break;
		}
		/* Skip PCIe ALT module (convert byte size to word count) */
		if (i == (u32)pcie_alt_module) {
			i += (I40E_SR_PCIE_ALT_MODULE_MAX_SIZE / 2);
			if (i >= hw->nvm.sr_size)
				break;
		}

		ret_code = i40e_read_nvm_word(hw, (u16)i, &word);
		if (ret_code != I40E_SUCCESS) {
			ret_code = I40E_ERR_NVM_CHECKSUM;
			goto i40e_calc_nvm_checksum_exit;
		}
		checksum_local += word;
	}

	*checksum = (u16)I40E_SR_SW_CHECKSUM_BASE - checksum_local;

i40e_calc_nvm_checksum_exit:
	return ret_code;
}

/**
 * i40e_update_nvm_checksum - Updates the NVM checksum
 * @hw: pointer to hardware structure
 *
 * NVM ownership must be acquired before calling this function and released
 * on ARQ completion event reception by caller.
 * This function will commit SR to NVM.
 **/
enum i40e_status_code i40e_update_nvm_checksum(struct i40e_hw *hw)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u16 checksum;

	DEBUGFUNC("i40e_update_nvm_checksum");

	ret_code = i40e_calc_nvm_checksum(hw, &checksum);
	if (ret_code == I40E_SUCCESS)
		ret_code = i40e_write_nvm_aq(hw, 0x00, I40E_SR_SW_CHECKSUM_WORD,
					     1, &checksum, true);

	return ret_code;
}

/**
 * i40e_validate_nvm_checksum - Validate EEPROM checksum
 * @hw: pointer to hardware structure
 * @checksum: calculated checksum
 *
 * Performs checksum calculation and validates the NVM SW checksum. If the
 * caller does not need checksum, the value can be NULL.
 **/
enum i40e_status_code i40e_validate_nvm_checksum(struct i40e_hw *hw,
						 u16 *checksum)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u16 checksum_sr = 0;
	u16 checksum_local = 0;

	DEBUGFUNC("i40e_validate_nvm_checksum");

	ret_code = i40e_calc_nvm_checksum(hw, &checksum_local);
	if (ret_code != I40E_SUCCESS)
		goto i40e_validate_nvm_checksum_exit;

	/* Do not use i40e_read_nvm_word() because we do not want to take
	 * the synchronization semaphores twice here.
	 */
	i40e_read_nvm_word(hw, I40E_SR_SW_CHECKSUM_WORD, &checksum_sr);

	/* Verify read checksum from EEPROM is the same as
	 * calculated checksum
	 */
	if (checksum_local != checksum_sr)
		ret_code = I40E_ERR_NVM_CHECKSUM;

	/* If the user cares, return the calculated checksum */
	if (checksum)
		*checksum = checksum_local;

i40e_validate_nvm_checksum_exit:
	return ret_code;
}
#ifdef I40E_NVMUPD_SUPPORT

STATIC enum i40e_status_code i40e_nvmupd_state_init(struct i40e_hw *hw,
						    struct i40e_nvm_access *cmd,
						    u8 *bytes, int *errno);
STATIC enum i40e_status_code i40e_nvmupd_state_reading(struct i40e_hw *hw,
						    struct i40e_nvm_access *cmd,
						    u8 *bytes, int *errno);
STATIC enum i40e_status_code i40e_nvmupd_state_writing(struct i40e_hw *hw,
						    struct i40e_nvm_access *cmd,
						    u8 *bytes, int *errno);
STATIC enum i40e_nvmupd_cmd i40e_nvmupd_validate_command(struct i40e_hw *hw,
						    struct i40e_nvm_access *cmd,
						    int *errno);
STATIC enum i40e_status_code i40e_nvmupd_nvm_erase(struct i40e_hw *hw,
						   struct i40e_nvm_access *cmd,
						   int *errno);
STATIC enum i40e_status_code i40e_nvmupd_nvm_write(struct i40e_hw *hw,
						   struct i40e_nvm_access *cmd,
						   u8 *bytes, int *errno);
STATIC enum i40e_status_code i40e_nvmupd_nvm_read(struct i40e_hw *hw,
						  struct i40e_nvm_access *cmd,
						  u8 *bytes, int *errno);
STATIC inline u8 i40e_nvmupd_get_module(u32 val)
{
	return (u8)(val & I40E_NVM_MOD_PNT_MASK);
}
STATIC inline u8 i40e_nvmupd_get_transaction(u32 val)
{
	return (u8)((val & I40E_NVM_TRANS_MASK) >> I40E_NVM_TRANS_SHIFT);
}

/**
 * i40e_nvmupd_command - Process an NVM update command
 * @hw: pointer to hardware structure
 * @cmd: pointer to nvm update command
 * @bytes: pointer to the data buffer
 * @errno: pointer to return error code
 *
 * Dispatches command depending on what update state is current
 **/
enum i40e_status_code i40e_nvmupd_command(struct i40e_hw *hw,
					  struct i40e_nvm_access *cmd,
					  u8 *bytes, int *errno)
{
	enum i40e_status_code status;

	DEBUGFUNC("i40e_nvmupd_command");

	/* assume success */
	*errno = 0;

	switch (hw->nvmupd_state) {
	case I40E_NVMUPD_STATE_INIT:
		status = i40e_nvmupd_state_init(hw, cmd, bytes, errno);
		break;

	case I40E_NVMUPD_STATE_READING:
		status = i40e_nvmupd_state_reading(hw, cmd, bytes, errno);
		break;

	case I40E_NVMUPD_STATE_WRITING:
		status = i40e_nvmupd_state_writing(hw, cmd, bytes, errno);
		break;

	default:
		/* invalid state, should never happen */
		status = I40E_NOT_SUPPORTED;
		*errno = -ESRCH;
		break;
	}
	return status;
}

/**
 * i40e_nvmupd_state_init - Handle NVM update state Init
 * @hw: pointer to hardware structure
 * @cmd: pointer to nvm update command buffer
 * @bytes: pointer to the data buffer
 * @errno: pointer to return error code
 *
 * Process legitimate commands of the Init state and conditionally set next
 * state. Reject all other commands.
 **/
STATIC enum i40e_status_code i40e_nvmupd_state_init(struct i40e_hw *hw,
						    struct i40e_nvm_access *cmd,
						    u8 *bytes, int *errno)
{
	enum i40e_status_code status = I40E_SUCCESS;
	enum i40e_nvmupd_cmd upd_cmd;

	DEBUGFUNC("i40e_nvmupd_state_init");

	upd_cmd = i40e_nvmupd_validate_command(hw, cmd, errno);

	switch (upd_cmd) {
	case I40E_NVMUPD_READ_SA:
		status = i40e_acquire_nvm(hw, I40E_RESOURCE_READ);
		if (status) {
			*errno = i40e_aq_rc_to_posix(hw->aq.asq_last_status);
		} else {
			status = i40e_nvmupd_nvm_read(hw, cmd, bytes, errno);
			i40e_release_nvm(hw);
		}
		break;

	case I40E_NVMUPD_READ_SNT:
		status = i40e_acquire_nvm(hw, I40E_RESOURCE_READ);
		if (status) {
			*errno = i40e_aq_rc_to_posix(hw->aq.asq_last_status);
		} else {
			status = i40e_nvmupd_nvm_read(hw, cmd, bytes, errno);
			hw->nvmupd_state = I40E_NVMUPD_STATE_READING;
		}
		break;

	case I40E_NVMUPD_WRITE_ERA:
		status = i40e_acquire_nvm(hw, I40E_RESOURCE_WRITE);
		if (status) {
			*errno = i40e_aq_rc_to_posix(hw->aq.asq_last_status);
		} else {
			status = i40e_nvmupd_nvm_erase(hw, cmd, errno);
			if (status)
				i40e_release_nvm(hw);
			else
				hw->aq.nvm_release_on_done = true;
		}
		break;

	case I40E_NVMUPD_WRITE_SA:
		status = i40e_acquire_nvm(hw, I40E_RESOURCE_WRITE);
		if (status) {
			*errno = i40e_aq_rc_to_posix(hw->aq.asq_last_status);
		} else {
			status = i40e_nvmupd_nvm_write(hw, cmd, bytes, errno);
			if (status)
				i40e_release_nvm(hw);
			else
				hw->aq.nvm_release_on_done = true;
		}
		break;

	case I40E_NVMUPD_WRITE_SNT:
		status = i40e_acquire_nvm(hw, I40E_RESOURCE_WRITE);
		if (status) {
			*errno = i40e_aq_rc_to_posix(hw->aq.asq_last_status);
		} else {
			status = i40e_nvmupd_nvm_write(hw, cmd, bytes, errno);
			hw->nvmupd_state = I40E_NVMUPD_STATE_WRITING;
		}
		break;

	case I40E_NVMUPD_CSUM_SA:
		status = i40e_acquire_nvm(hw, I40E_RESOURCE_WRITE);
		if (status) {
			*errno = i40e_aq_rc_to_posix(hw->aq.asq_last_status);
		} else {
			status = i40e_update_nvm_checksum(hw);
			if (status) {
				*errno = hw->aq.asq_last_status ?
				   i40e_aq_rc_to_posix(hw->aq.asq_last_status) :
				   -EIO;
				i40e_release_nvm(hw);
			} else {
				hw->aq.nvm_release_on_done = true;
			}
		}
		break;

	default:
		status = I40E_ERR_NVM;
		*errno = -ESRCH;
		break;
	}
	return status;
}

/**
 * i40e_nvmupd_state_reading - Handle NVM update state Reading
 * @hw: pointer to hardware structure
 * @cmd: pointer to nvm update command buffer
 * @bytes: pointer to the data buffer
 * @errno: pointer to return error code
 *
 * NVM ownership is already held.  Process legitimate commands and set any
 * change in state; reject all other commands.
 **/
STATIC enum i40e_status_code i40e_nvmupd_state_reading(struct i40e_hw *hw,
						    struct i40e_nvm_access *cmd,
						    u8 *bytes, int *errno)
{
	enum i40e_status_code status;
	enum i40e_nvmupd_cmd upd_cmd;

	DEBUGFUNC("i40e_nvmupd_state_reading");

	upd_cmd = i40e_nvmupd_validate_command(hw, cmd, errno);

	switch (upd_cmd) {
	case I40E_NVMUPD_READ_SA:
	case I40E_NVMUPD_READ_CON:
		status = i40e_nvmupd_nvm_read(hw, cmd, bytes, errno);
		break;

	case I40E_NVMUPD_READ_LCB:
		status = i40e_nvmupd_nvm_read(hw, cmd, bytes, errno);
		i40e_release_nvm(hw);
		hw->nvmupd_state = I40E_NVMUPD_STATE_INIT;
		break;

	default:
		status = I40E_NOT_SUPPORTED;
		*errno = -ESRCH;
		break;
	}
	return status;
}

/**
 * i40e_nvmupd_state_writing - Handle NVM update state Writing
 * @hw: pointer to hardware structure
 * @cmd: pointer to nvm update command buffer
 * @bytes: pointer to the data buffer
 * @errno: pointer to return error code
 *
 * NVM ownership is already held.  Process legitimate commands and set any
 * change in state; reject all other commands
 **/
STATIC enum i40e_status_code i40e_nvmupd_state_writing(struct i40e_hw *hw,
						    struct i40e_nvm_access *cmd,
						    u8 *bytes, int *errno)
{
	enum i40e_status_code status;
	enum i40e_nvmupd_cmd upd_cmd;

	DEBUGFUNC("i40e_nvmupd_state_writing");

	upd_cmd = i40e_nvmupd_validate_command(hw, cmd, errno);

	switch (upd_cmd) {
	case I40E_NVMUPD_WRITE_CON:
		status = i40e_nvmupd_nvm_write(hw, cmd, bytes, errno);
		break;

	case I40E_NVMUPD_WRITE_LCB:
		status = i40e_nvmupd_nvm_write(hw, cmd, bytes, errno);
		if (!status) {
			hw->aq.nvm_release_on_done = true;
			hw->nvmupd_state = I40E_NVMUPD_STATE_INIT;
		}
		break;

	case I40E_NVMUPD_CSUM_CON:
		status = i40e_update_nvm_checksum(hw);
		if (status)
			*errno = hw->aq.asq_last_status ?
				   i40e_aq_rc_to_posix(hw->aq.asq_last_status) :
				   -EIO;
		break;

	case I40E_NVMUPD_CSUM_LCB:
		status = i40e_update_nvm_checksum(hw);
		if (status) {
			*errno = hw->aq.asq_last_status ?
				   i40e_aq_rc_to_posix(hw->aq.asq_last_status) :
				   -EIO;
		} else {
			hw->aq.nvm_release_on_done = true;
			hw->nvmupd_state = I40E_NVMUPD_STATE_INIT;
		}
		break;

	default:
		status = I40E_NOT_SUPPORTED;
		*errno = -ESRCH;
		break;
	}
	return status;
}

/**
 * i40e_nvmupd_validate_command - Validate given command
 * @hw: pointer to hardware structure
 * @cmd: pointer to nvm update command buffer
 * @errno: pointer to return error code
 *
 * Return one of the valid command types or I40E_NVMUPD_INVALID
 **/
STATIC enum i40e_nvmupd_cmd i40e_nvmupd_validate_command(struct i40e_hw *hw,
						    struct i40e_nvm_access *cmd,
						    int *errno)
{
	enum i40e_nvmupd_cmd upd_cmd;
	u8 transaction, module;

	DEBUGFUNC("i40e_nvmupd_validate_command\n");

	/* anything that doesn't match a recognized case is an error */
	upd_cmd = I40E_NVMUPD_INVALID;

	transaction = i40e_nvmupd_get_transaction(cmd->config);
	module = i40e_nvmupd_get_module(cmd->config);

	/* limits on data size */
	if ((cmd->data_size < 1) ||
	    (cmd->data_size > I40E_NVMUPD_MAX_DATA)) {
		DEBUGOUT1("i40e_nvmupd_validate_command data_size %d\n",
			cmd->data_size);
		*errno = -EFAULT;
		return I40E_NVMUPD_INVALID;
	}

	switch (cmd->command) {
	case I40E_NVM_READ:
		switch (transaction) {
		case I40E_NVM_CON:
			upd_cmd = I40E_NVMUPD_READ_CON;
			break;
		case I40E_NVM_SNT:
			upd_cmd = I40E_NVMUPD_READ_SNT;
			break;
		case I40E_NVM_LCB:
			upd_cmd = I40E_NVMUPD_READ_LCB;
			break;
		case I40E_NVM_SA:
			upd_cmd = I40E_NVMUPD_READ_SA;
			break;
		}
		break;

	case I40E_NVM_WRITE:
		switch (transaction) {
		case I40E_NVM_CON:
			upd_cmd = I40E_NVMUPD_WRITE_CON;
			break;
		case I40E_NVM_SNT:
			upd_cmd = I40E_NVMUPD_WRITE_SNT;
			break;
		case I40E_NVM_LCB:
			upd_cmd = I40E_NVMUPD_WRITE_LCB;
			break;
		case I40E_NVM_SA:
			upd_cmd = I40E_NVMUPD_WRITE_SA;
			break;
		case I40E_NVM_ERA:
			upd_cmd = I40E_NVMUPD_WRITE_ERA;
			break;
		case I40E_NVM_CSUM:
			upd_cmd = I40E_NVMUPD_CSUM_CON;
			break;
		case (I40E_NVM_CSUM|I40E_NVM_SA):
			upd_cmd = I40E_NVMUPD_CSUM_SA;
			break;
		case (I40E_NVM_CSUM|I40E_NVM_LCB):
			upd_cmd = I40E_NVMUPD_CSUM_LCB;
			break;
		}
		break;
	}

	if (upd_cmd == I40E_NVMUPD_INVALID) {
		*errno = -EFAULT;
		DEBUGOUT2(
			"i40e_nvmupd_validate_command returns %d  errno: %d\n",
			upd_cmd, *errno);
	}
	return upd_cmd;
}

/**
 * i40e_nvmupd_nvm_read - Read NVM
 * @hw: pointer to hardware structure
 * @cmd: pointer to nvm update command buffer
 * @bytes: pointer to the data buffer
 * @errno: pointer to return error code
 *
 * cmd structure contains identifiers and data buffer
 **/
STATIC enum i40e_status_code i40e_nvmupd_nvm_read(struct i40e_hw *hw,
						  struct i40e_nvm_access *cmd,
						  u8 *bytes, int *errno)
{
	enum i40e_status_code status;
	u8 module, transaction;
	bool last;

	transaction = i40e_nvmupd_get_transaction(cmd->config);
	module = i40e_nvmupd_get_module(cmd->config);
	last = (transaction == I40E_NVM_LCB) || (transaction == I40E_NVM_SA);
	DEBUGOUT3("i40e_nvmupd_nvm_read mod 0x%x  off 0x%x  len 0x%x\n",
		module, cmd->offset, cmd->data_size);

	status = i40e_aq_read_nvm(hw, module, cmd->offset, (u16)cmd->data_size,
				  bytes, last, NULL);
	DEBUGOUT1("i40e_nvmupd_nvm_read status %d\n", status);
	if (status != I40E_SUCCESS)
		*errno = i40e_aq_rc_to_posix(hw->aq.asq_last_status);

	return status;
}

/**
 * i40e_nvmupd_nvm_erase - Erase an NVM module
 * @hw: pointer to hardware structure
 * @cmd: pointer to nvm update command buffer
 * @errno: pointer to return error code
 *
 * module, offset, data_size and data are in cmd structure
 **/
STATIC enum i40e_status_code i40e_nvmupd_nvm_erase(struct i40e_hw *hw,
						   struct i40e_nvm_access *cmd,
						   int *errno)
{
	enum i40e_status_code status = I40E_SUCCESS;
	u8 module, transaction;
	bool last;

	transaction = i40e_nvmupd_get_transaction(cmd->config);
	module = i40e_nvmupd_get_module(cmd->config);
	last = (transaction & I40E_NVM_LCB);
	DEBUGOUT3("i40e_nvmupd_nvm_erase mod 0x%x  off 0x%x  len 0x%x\n",
		module, cmd->offset, cmd->data_size);
	status = i40e_aq_erase_nvm(hw, module, cmd->offset, (u16)cmd->data_size,
				   last, NULL);
	DEBUGOUT1("i40e_nvmupd_nvm_erase status %d\n", status);
	if (status != I40E_SUCCESS)
		*errno = i40e_aq_rc_to_posix(hw->aq.asq_last_status);

	return status;
}

/**
 * i40e_nvmupd_nvm_write - Write NVM
 * @hw: pointer to hardware structure
 * @cmd: pointer to nvm update command buffer
 * @bytes: pointer to the data buffer
 * @errno: pointer to return error code
 *
 * module, offset, data_size and data are in cmd structure
 **/
STATIC enum i40e_status_code i40e_nvmupd_nvm_write(struct i40e_hw *hw,
						   struct i40e_nvm_access *cmd,
						   u8 *bytes, int *errno)
{
	enum i40e_status_code status = I40E_SUCCESS;
	u8 module, transaction;
	bool last;

	transaction = i40e_nvmupd_get_transaction(cmd->config);
	module = i40e_nvmupd_get_module(cmd->config);
	last = (transaction & I40E_NVM_LCB);
	DEBUGOUT3("i40e_nvmupd_nvm_write mod 0x%x off 0x%x len 0x%x\n",
		  module, cmd->offset, cmd->data_size);
	status = i40e_aq_update_nvm(hw, module, cmd->offset,
				    (u16)cmd->data_size, bytes, last, NULL);
	DEBUGOUT1("i40e_nvmupd_nvm_write status %d\n", status);
	if (status != I40E_SUCCESS)
		*errno = i40e_aq_rc_to_posix(hw->aq.asq_last_status);

	return status;
}
#endif
