/*******************************************************************************

Copyright (c) 2013 - 2015, Intel Corporation
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

#include "i40e_diag.h"
#include "i40e_prototype.h"

/**
 * i40e_diag_set_loopback
 * @hw: pointer to the hw struct
 * @mode: loopback mode
 *
 * Set chosen loopback mode
 **/
enum i40e_status_code i40e_diag_set_loopback(struct i40e_hw *hw,
					     enum i40e_lb_mode mode)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;

	if (i40e_aq_set_lb_modes(hw, mode, NULL))
		ret_code = I40E_ERR_DIAG_TEST_FAILED;

	return ret_code;
}

/**
 * i40e_diag_reg_pattern_test
 * @hw: pointer to the hw struct
 * @reg: reg to be tested
 * @mask: bits to be touched
 **/
static enum i40e_status_code i40e_diag_reg_pattern_test(struct i40e_hw *hw,
							u32 reg, u32 mask)
{
	const u32 patterns[] = {0x5A5A5A5A, 0xA5A5A5A5, 0x00000000, 0xFFFFFFFF};
	u32 pat, val, orig_val;
	int i;

	orig_val = rd32(hw, reg);
	for (i = 0; i < ARRAY_SIZE(patterns); i++) {
		pat = patterns[i];
		wr32(hw, reg, (pat & mask));
		val = rd32(hw, reg);
		if ((val & mask) != (pat & mask)) {
			return I40E_ERR_DIAG_TEST_FAILED;
		}
	}

	wr32(hw, reg, orig_val);
	val = rd32(hw, reg);
	if (val != orig_val) {
		return I40E_ERR_DIAG_TEST_FAILED;
	}

	return I40E_SUCCESS;
}

struct i40e_diag_reg_test_info i40e_reg_list[] = {
	/* offset               mask         elements   stride */
	{I40E_QTX_CTL(0),       0x0000FFBF, 1, I40E_QTX_CTL(1) - I40E_QTX_CTL(0)},
	{I40E_PFINT_ITR0(0),    0x00000FFF, 3, I40E_PFINT_ITR0(1) - I40E_PFINT_ITR0(0)},
	{I40E_PFINT_ITRN(0, 0), 0x00000FFF, 1, I40E_PFINT_ITRN(0, 1) - I40E_PFINT_ITRN(0, 0)},
	{I40E_PFINT_ITRN(1, 0), 0x00000FFF, 1, I40E_PFINT_ITRN(1, 1) - I40E_PFINT_ITRN(1, 0)},
	{I40E_PFINT_ITRN(2, 0), 0x00000FFF, 1, I40E_PFINT_ITRN(2, 1) - I40E_PFINT_ITRN(2, 0)},
	{I40E_PFINT_STAT_CTL0,  0x0000000C, 1, 0},
	{I40E_PFINT_LNKLST0,    0x00001FFF, 1, 0},
	{I40E_PFINT_LNKLSTN(0), 0x000007FF, 1, I40E_PFINT_LNKLSTN(1) - I40E_PFINT_LNKLSTN(0)},
	{I40E_QINT_TQCTL(0),    0x000000FF, 1, I40E_QINT_TQCTL(1) - I40E_QINT_TQCTL(0)},
	{I40E_QINT_RQCTL(0),    0x000000FF, 1, I40E_QINT_RQCTL(1) - I40E_QINT_RQCTL(0)},
	{I40E_PFINT_ICR0_ENA,   0xF7F20000, 1, 0},
	{ 0 }
};

/**
 * i40e_diag_reg_test
 * @hw: pointer to the hw struct
 *
 * Perform registers diagnostic test
 **/
enum i40e_status_code i40e_diag_reg_test(struct i40e_hw *hw)
{
	enum i40e_status_code ret_code = I40E_SUCCESS;
	u32 reg, mask;
	u32 i, j;

	for (i = 0; i40e_reg_list[i].offset != 0 &&
					     ret_code == I40E_SUCCESS; i++) {

		/* set actual reg range for dynamically allocated resources */
		if (i40e_reg_list[i].offset == I40E_QTX_CTL(0) &&
		    hw->func_caps.num_tx_qp != 0)
			i40e_reg_list[i].elements = hw->func_caps.num_tx_qp;
		if ((i40e_reg_list[i].offset == I40E_PFINT_ITRN(0, 0) ||
		     i40e_reg_list[i].offset == I40E_PFINT_ITRN(1, 0) ||
		     i40e_reg_list[i].offset == I40E_PFINT_ITRN(2, 0) ||
		     i40e_reg_list[i].offset == I40E_QINT_TQCTL(0) ||
		     i40e_reg_list[i].offset == I40E_QINT_RQCTL(0)) &&
		    hw->func_caps.num_msix_vectors != 0)
			i40e_reg_list[i].elements =
				hw->func_caps.num_msix_vectors - 1;

		/* test register access */
		mask = i40e_reg_list[i].mask;
		for (j = 0; j < i40e_reg_list[i].elements &&
			    ret_code == I40E_SUCCESS; j++) {
			reg = i40e_reg_list[i].offset
				+ (j * i40e_reg_list[i].stride);
			ret_code = i40e_diag_reg_pattern_test(hw, reg, mask);
		}
	}

	return ret_code;
}

/**
 * i40e_diag_eeprom_test
 * @hw: pointer to the hw struct
 *
 * Perform EEPROM diagnostic test
 **/
enum i40e_status_code i40e_diag_eeprom_test(struct i40e_hw *hw)
{
	enum i40e_status_code ret_code;
	u16 reg_val;

	/* read NVM control word and if NVM valid, validate EEPROM checksum*/
	ret_code = i40e_read_nvm_word(hw, I40E_SR_NVM_CONTROL_WORD, &reg_val);
	if ((ret_code == I40E_SUCCESS) &&
	    ((reg_val & I40E_SR_CONTROL_WORD_1_MASK) ==
	     (0x01 << I40E_SR_CONTROL_WORD_1_SHIFT))) {
		ret_code = i40e_validate_nvm_checksum(hw, NULL);
	} else {
		ret_code = I40E_ERR_DIAG_TEST_FAILED;
	}

	return ret_code;
}

/**
 * i40e_diag_fw_alive_test
 * @hw: pointer to the hw struct
 *
 * Perform FW alive diagnostic test
 **/
enum i40e_status_code i40e_diag_fw_alive_test(struct i40e_hw *hw)
{
	UNREFERENCED_1PARAMETER(hw);
	return I40E_SUCCESS;
}
