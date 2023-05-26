/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#include "igc_api.h"

/**
 *  igc_get_i2c_data - Reads the I2C SDA data bit
 *  @i2cctl: Current value of I2CCTL register
 *
 *  Returns the I2C data bit value
 **/
static bool igc_get_i2c_data(u32 *i2cctl)
{
	bool data;

	DEBUGFUNC("igc_get_i2c_data");

	if (*i2cctl & IGC_I2C_DATA_IN)
		data = 1;
	else
		data = 0;

	return data;
}

/**
 *  igc_set_i2c_data - Sets the I2C data bit
 *  @hw: pointer to hardware structure
 *  @i2cctl: Current value of I2CCTL register
 *  @data: I2C data value (0 or 1) to set
 *
 *  Sets the I2C data bit
 **/
static s32 igc_set_i2c_data(struct igc_hw *hw, u32 *i2cctl, bool data)
{
	s32 status = IGC_SUCCESS;

	DEBUGFUNC("igc_set_i2c_data");

	if (data)
		*i2cctl |= IGC_I2C_DATA_OUT;
	else
		*i2cctl &= ~IGC_I2C_DATA_OUT;

	*i2cctl &= ~IGC_I2C_DATA_OE_N;
	*i2cctl |= IGC_I2C_CLK_OE_N;
	IGC_WRITE_REG(hw, IGC_I2CPARAMS, *i2cctl);
	IGC_WRITE_FLUSH(hw);

	/* Data rise/fall (1000ns/300ns) and set-up time (250ns) */
	usec_delay(IGC_I2C_T_RISE + IGC_I2C_T_FALL + IGC_I2C_T_SU_DATA);

	*i2cctl = IGC_READ_REG(hw, IGC_I2CPARAMS);
	if (data != igc_get_i2c_data(i2cctl)) {
		status = IGC_ERR_I2C;
		DEBUGOUT1("Error - I2C data was not set to %X.\n", data);
	}

	return status;
}

/**
 *  igc_raise_i2c_clk - Raises the I2C SCL clock
 *  @hw: pointer to hardware structure
 *  @i2cctl: Current value of I2CCTL register
 *
 *  Raises the I2C clock line '0'->'1'
 **/
static void igc_raise_i2c_clk(struct igc_hw *hw, u32 *i2cctl)
{
	DEBUGFUNC("igc_raise_i2c_clk");

	*i2cctl |= IGC_I2C_CLK_OUT;
	*i2cctl &= ~IGC_I2C_CLK_OE_N;
	IGC_WRITE_REG(hw, IGC_I2CPARAMS, *i2cctl);
	IGC_WRITE_FLUSH(hw);

	/* SCL rise time (1000ns) */
	usec_delay(IGC_I2C_T_RISE);
}

/**
 *  igc_lower_i2c_clk - Lowers the I2C SCL clock
 *  @hw: pointer to hardware structure
 *  @i2cctl: Current value of I2CCTL register
 *
 *  Lowers the I2C clock line '1'->'0'
 **/
static void igc_lower_i2c_clk(struct igc_hw *hw, u32 *i2cctl)
{
	DEBUGFUNC("igc_lower_i2c_clk");

	*i2cctl &= ~IGC_I2C_CLK_OUT;
	*i2cctl &= ~IGC_I2C_CLK_OE_N;
	IGC_WRITE_REG(hw, IGC_I2CPARAMS, *i2cctl);
	IGC_WRITE_FLUSH(hw);

	/* SCL fall time (300ns) */
	usec_delay(IGC_I2C_T_FALL);
}

/**
 *  igc_i2c_start - Sets I2C start condition
 *  @hw: pointer to hardware structure
 *
 *  Sets I2C start condition (High -> Low on SDA while SCL is High)
 **/
static void igc_i2c_start(struct igc_hw *hw)
{
	u32 i2cctl = IGC_READ_REG(hw, IGC_I2CPARAMS);

	DEBUGFUNC("igc_i2c_start");

	/* Start condition must begin with data and clock high */
	igc_set_i2c_data(hw, &i2cctl, 1);
	igc_raise_i2c_clk(hw, &i2cctl);

	/* Setup time for start condition (4.7us) */
	usec_delay(IGC_I2C_T_SU_STA);

	igc_set_i2c_data(hw, &i2cctl, 0);

	/* Hold time for start condition (4us) */
	usec_delay(IGC_I2C_T_HD_STA);

	igc_lower_i2c_clk(hw, &i2cctl);

	/* Minimum low period of clock is 4.7 us */
	usec_delay(IGC_I2C_T_LOW);
}

/**
 *  igc_i2c_stop - Sets I2C stop condition
 *  @hw: pointer to hardware structure
 *
 *  Sets I2C stop condition (Low -> High on SDA while SCL is High)
 **/
static void igc_i2c_stop(struct igc_hw *hw)
{
	u32 i2cctl = IGC_READ_REG(hw, IGC_I2CPARAMS);

	DEBUGFUNC("igc_i2c_stop");

	/* Stop condition must begin with data low and clock high */
	igc_set_i2c_data(hw, &i2cctl, 0);
	igc_raise_i2c_clk(hw, &i2cctl);

	/* Setup time for stop condition (4us) */
	usec_delay(IGC_I2C_T_SU_STO);

	igc_set_i2c_data(hw, &i2cctl, 1);

	/* bus free time between stop and start (4.7us)*/
	usec_delay(IGC_I2C_T_BUF);
}

/**
 *  igc_clock_in_i2c_bit - Clocks in one bit via I2C data/clock
 *  @hw: pointer to hardware structure
 *  @data: read data value
 *
 *  Clocks in one bit via I2C data/clock
 **/
static void igc_clock_in_i2c_bit(struct igc_hw *hw, bool *data)
{
	u32 i2cctl = IGC_READ_REG(hw, IGC_I2CPARAMS);

	DEBUGFUNC("igc_clock_in_i2c_bit");

	igc_raise_i2c_clk(hw, &i2cctl);

	/* Minimum high period of clock is 4us */
	usec_delay(IGC_I2C_T_HIGH);

	i2cctl = IGC_READ_REG(hw, IGC_I2CPARAMS);
	*data = igc_get_i2c_data(&i2cctl);

	igc_lower_i2c_clk(hw, &i2cctl);

	/* Minimum low period of clock is 4.7 us */
	usec_delay(IGC_I2C_T_LOW);
}

/**
 *  igc_clock_in_i2c_byte - Clocks in one byte via I2C
 *  @hw: pointer to hardware structure
 *  @data: data byte to clock in
 *
 *  Clocks in one byte data via I2C data/clock
 **/
static void igc_clock_in_i2c_byte(struct igc_hw *hw, u8 *data)
{
	s32 i;
	bool bit = 0;

	DEBUGFUNC("igc_clock_in_i2c_byte");

	*data = 0;
	for (i = 7; i >= 0; i--) {
		igc_clock_in_i2c_bit(hw, &bit);
		*data |= bit << i;
	}
}

/**
 *  igc_clock_out_i2c_bit - Clocks in/out one bit via I2C data/clock
 *  @hw: pointer to hardware structure
 *  @data: data value to write
 *
 *  Clocks out one bit via I2C data/clock
 **/
static s32 igc_clock_out_i2c_bit(struct igc_hw *hw, bool data)
{
	s32 status;
	u32 i2cctl = IGC_READ_REG(hw, IGC_I2CPARAMS);

	DEBUGFUNC("igc_clock_out_i2c_bit");

	status = igc_set_i2c_data(hw, &i2cctl, data);
	if (status == IGC_SUCCESS) {
		igc_raise_i2c_clk(hw, &i2cctl);

		/* Minimum high period of clock is 4us */
		usec_delay(IGC_I2C_T_HIGH);

		igc_lower_i2c_clk(hw, &i2cctl);

		/* Minimum low period of clock is 4.7 us.
		 * This also takes care of the data hold time.
		 */
		usec_delay(IGC_I2C_T_LOW);
	} else {
		status = IGC_ERR_I2C;
		DEBUGOUT1("I2C data was not set to %X\n", data);
	}

	return status;
}

/**
 *  igc_clock_out_i2c_byte - Clocks out one byte via I2C
 *  @hw: pointer to hardware structure
 *  @data: data byte clocked out
 *
 *  Clocks out one byte data via I2C data/clock
 **/
static s32 igc_clock_out_i2c_byte(struct igc_hw *hw, u8 data)
{
	s32 status = IGC_SUCCESS;
	s32 i;
	u32 i2cctl;
	bool bit = 0;

	DEBUGFUNC("igc_clock_out_i2c_byte");

	for (i = 7; i >= 0; i--) {
		bit = (data >> i) & 0x1;
		status = igc_clock_out_i2c_bit(hw, bit);

		if (status != IGC_SUCCESS)
			break;
	}

	/* Release SDA line (set high) */
	i2cctl = IGC_READ_REG(hw, IGC_I2CPARAMS);

	i2cctl |= IGC_I2C_DATA_OE_N;
	IGC_WRITE_REG(hw, IGC_I2CPARAMS, i2cctl);
	IGC_WRITE_FLUSH(hw);

	return status;
}

/**
 *  igc_get_i2c_ack - Polls for I2C ACK
 *  @hw: pointer to hardware structure
 *
 *  Clocks in/out one bit via I2C data/clock
 **/
static s32 igc_get_i2c_ack(struct igc_hw *hw)
{
	s32 status = IGC_SUCCESS;
	u32 i = 0;
	u32 i2cctl = IGC_READ_REG(hw, IGC_I2CPARAMS);
	u32 timeout = 10;
	bool ack = true;

	DEBUGFUNC("igc_get_i2c_ack");

	igc_raise_i2c_clk(hw, &i2cctl);

	/* Minimum high period of clock is 4us */
	usec_delay(IGC_I2C_T_HIGH);

	/* Wait until SCL returns high */
	for (i = 0; i < timeout; i++) {
		usec_delay(1);
		i2cctl = IGC_READ_REG(hw, IGC_I2CPARAMS);
		if (i2cctl & IGC_I2C_CLK_IN)
			break;
	}
	if (!(i2cctl & IGC_I2C_CLK_IN))
		return IGC_ERR_I2C;

	ack = igc_get_i2c_data(&i2cctl);
	if (ack) {
		DEBUGOUT("I2C ack was not received.\n");
		status = IGC_ERR_I2C;
	}

	igc_lower_i2c_clk(hw, &i2cctl);

	/* Minimum low period of clock is 4.7 us */
	usec_delay(IGC_I2C_T_LOW);

	return status;
}

/**
 *  igc_set_i2c_bb - Enable I2C bit-bang
 *  @hw: pointer to the HW structure
 *
 *  Enable I2C bit-bang interface
 *
 **/
s32 igc_set_i2c_bb(struct igc_hw *hw)
{
	s32 ret_val = IGC_SUCCESS;
	u32 ctrl_ext, i2cparams;

	DEBUGFUNC("igc_set_i2c_bb");

	ctrl_ext = IGC_READ_REG(hw, IGC_CTRL_EXT);
	ctrl_ext |= IGC_CTRL_I2C_ENA;
	IGC_WRITE_REG(hw, IGC_CTRL_EXT, ctrl_ext);
	IGC_WRITE_FLUSH(hw);

	i2cparams = IGC_READ_REG(hw, IGC_I2CPARAMS);
	i2cparams |= IGC_I2CBB_EN;
	i2cparams |= IGC_I2C_DATA_OE_N;
	i2cparams |= IGC_I2C_CLK_OE_N;
	IGC_WRITE_REG(hw, IGC_I2CPARAMS, i2cparams);
	IGC_WRITE_FLUSH(hw);

	return ret_val;
}

/**
 *  igc_read_i2c_byte_generic - Reads 8 bit word over I2C
 *  @hw: pointer to hardware structure
 *  @byte_offset: byte offset to read
 *  @dev_addr: device address
 *  @data: value read
 *
 *  Performs byte read operation over I2C interface at
 *  a specified device address.
 **/
s32 igc_read_i2c_byte_generic(struct igc_hw *hw, u8 byte_offset,
				u8 dev_addr, u8 *data)
{
	s32 status = IGC_SUCCESS;
	u32 max_retry = 10;
	u32 retry = 1;
	u16 swfw_mask = 0;

	bool nack = true;

	DEBUGFUNC("igc_read_i2c_byte_generic");

	swfw_mask = IGC_SWFW_PHY0_SM;

	do {
		if (hw->mac.ops.acquire_swfw_sync(hw, swfw_mask)
		    != IGC_SUCCESS) {
			status = IGC_ERR_SWFW_SYNC;
			goto read_byte_out;
		}

		igc_i2c_start(hw);

		/* Device Address and write indication */
		status = igc_clock_out_i2c_byte(hw, dev_addr);
		if (status != IGC_SUCCESS)
			goto fail;

		status = igc_get_i2c_ack(hw);
		if (status != IGC_SUCCESS)
			goto fail;

		status = igc_clock_out_i2c_byte(hw, byte_offset);
		if (status != IGC_SUCCESS)
			goto fail;

		status = igc_get_i2c_ack(hw);
		if (status != IGC_SUCCESS)
			goto fail;

		igc_i2c_start(hw);

		/* Device Address and read indication */
		status = igc_clock_out_i2c_byte(hw, (dev_addr | 0x1));
		if (status != IGC_SUCCESS)
			goto fail;

		status = igc_get_i2c_ack(hw);
		if (status != IGC_SUCCESS)
			goto fail;

		igc_clock_in_i2c_byte(hw, data);

		status = igc_clock_out_i2c_bit(hw, nack);
		if (status != IGC_SUCCESS)
			goto fail;

		igc_i2c_stop(hw);
		break;

fail:
		hw->mac.ops.release_swfw_sync(hw, swfw_mask);
		msec_delay(100);
		igc_i2c_bus_clear(hw);
		retry++;
		if (retry < max_retry)
			DEBUGOUT("I2C byte read error - Retrying.\n");
		else
			DEBUGOUT("I2C byte read error.\n");

	} while (retry < max_retry);

	hw->mac.ops.release_swfw_sync(hw, swfw_mask);

read_byte_out:

	return status;
}

/**
 *  igc_write_i2c_byte_generic - Writes 8 bit word over I2C
 *  @hw: pointer to hardware structure
 *  @byte_offset: byte offset to write
 *  @dev_addr: device address
 *  @data: value to write
 *
 *  Performs byte write operation over I2C interface at
 *  a specified device address.
 **/
s32 igc_write_i2c_byte_generic(struct igc_hw *hw, u8 byte_offset,
				 u8 dev_addr, u8 data)
{
	s32 status = IGC_SUCCESS;
	u32 max_retry = 1;
	u32 retry = 0;
	u16 swfw_mask = 0;

	DEBUGFUNC("igc_write_i2c_byte_generic");

	swfw_mask = IGC_SWFW_PHY0_SM;

	if (hw->mac.ops.acquire_swfw_sync(hw, swfw_mask) != IGC_SUCCESS) {
		status = IGC_ERR_SWFW_SYNC;
		goto write_byte_out;
	}

	do {
		igc_i2c_start(hw);

		status = igc_clock_out_i2c_byte(hw, dev_addr);
		if (status != IGC_SUCCESS)
			goto fail;

		status = igc_get_i2c_ack(hw);
		if (status != IGC_SUCCESS)
			goto fail;

		status = igc_clock_out_i2c_byte(hw, byte_offset);
		if (status != IGC_SUCCESS)
			goto fail;

		status = igc_get_i2c_ack(hw);
		if (status != IGC_SUCCESS)
			goto fail;

		status = igc_clock_out_i2c_byte(hw, data);
		if (status != IGC_SUCCESS)
			goto fail;

		status = igc_get_i2c_ack(hw);
		if (status != IGC_SUCCESS)
			goto fail;

		igc_i2c_stop(hw);
		break;

fail:
		igc_i2c_bus_clear(hw);
		retry++;
		if (retry < max_retry)
			DEBUGOUT("I2C byte write error - Retrying.\n");
		else
			DEBUGOUT("I2C byte write error.\n");
	} while (retry < max_retry);

	hw->mac.ops.release_swfw_sync(hw, swfw_mask);

write_byte_out:

	return status;
}

/**
 *  igc_i2c_bus_clear - Clears the I2C bus
 *  @hw: pointer to hardware structure
 *
 *  Clears the I2C bus by sending nine clock pulses.
 *  Used when data line is stuck low.
 **/
void igc_i2c_bus_clear(struct igc_hw *hw)
{
	u32 i2cctl = IGC_READ_REG(hw, IGC_I2CPARAMS);
	u32 i;

	DEBUGFUNC("igc_i2c_bus_clear");

	igc_i2c_start(hw);

	igc_set_i2c_data(hw, &i2cctl, 1);

	for (i = 0; i < 9; i++) {
		igc_raise_i2c_clk(hw, &i2cctl);

		/* Min high period of clock is 4us */
		usec_delay(IGC_I2C_T_HIGH);

		igc_lower_i2c_clk(hw, &i2cctl);

		/* Min low period of clock is 4.7us*/
		usec_delay(IGC_I2C_T_LOW);
	}

	igc_i2c_start(hw);

	/* Put the i2c bus back to default state */
	igc_i2c_stop(hw);
}

/**
 *  igc_init_mac_params - Initialize MAC function pointers
 *  @hw: pointer to the HW structure
 *
 *  This function initializes the function pointers for the MAC
 *  set of functions.  Called by drivers or by igc_setup_init_funcs.
 **/
s32 igc_init_mac_params(struct igc_hw *hw)
{
	s32 ret_val = IGC_SUCCESS;

	if (hw->mac.ops.init_params) {
		ret_val = hw->mac.ops.init_params(hw);
		if (ret_val) {
			DEBUGOUT("MAC Initialization Error\n");
			goto out;
		}
	} else {
		DEBUGOUT("mac.init_mac_params was NULL\n");
		ret_val = -IGC_ERR_CONFIG;
	}

out:
	return ret_val;
}

/**
 *  igc_init_nvm_params - Initialize NVM function pointers
 *  @hw: pointer to the HW structure
 *
 *  This function initializes the function pointers for the NVM
 *  set of functions.  Called by drivers or by igc_setup_init_funcs.
 **/
s32 igc_init_nvm_params(struct igc_hw *hw)
{
	s32 ret_val = IGC_SUCCESS;

	if (hw->nvm.ops.init_params) {
		ret_val = hw->nvm.ops.init_params(hw);
		if (ret_val) {
			DEBUGOUT("NVM Initialization Error\n");
			goto out;
		}
	} else {
		DEBUGOUT("nvm.init_nvm_params was NULL\n");
		ret_val = -IGC_ERR_CONFIG;
	}

out:
	return ret_val;
}

/**
 *  igc_init_phy_params - Initialize PHY function pointers
 *  @hw: pointer to the HW structure
 *
 *  This function initializes the function pointers for the PHY
 *  set of functions.  Called by drivers or by igc_setup_init_funcs.
 **/
s32 igc_init_phy_params(struct igc_hw *hw)
{
	s32 ret_val = IGC_SUCCESS;

	if (hw->phy.ops.init_params) {
		ret_val = hw->phy.ops.init_params(hw);
		if (ret_val) {
			DEBUGOUT("PHY Initialization Error\n");
			goto out;
		}
	} else {
		DEBUGOUT("phy.init_phy_params was NULL\n");
		ret_val =  -IGC_ERR_CONFIG;
	}

out:
	return ret_val;
}

/**
 *  igc_init_mbx_params - Initialize mailbox function pointers
 *  @hw: pointer to the HW structure
 *
 *  This function initializes the function pointers for the PHY
 *  set of functions.  Called by drivers or by igc_setup_init_funcs.
 **/
s32 igc_init_mbx_params(struct igc_hw *hw)
{
	s32 ret_val = IGC_SUCCESS;

	if (hw->mbx.ops.init_params) {
		ret_val = hw->mbx.ops.init_params(hw);
		if (ret_val) {
			DEBUGOUT("Mailbox Initialization Error\n");
			goto out;
		}
	} else {
		DEBUGOUT("mbx.init_mbx_params was NULL\n");
		ret_val =  -IGC_ERR_CONFIG;
	}

out:
	return ret_val;
}

/**
 *  igc_set_mac_type - Sets MAC type
 *  @hw: pointer to the HW structure
 *
 *  This function sets the mac type of the adapter based on the
 *  device ID stored in the hw structure.
 *  MUST BE FIRST FUNCTION CALLED (explicitly or through
 *  igc_setup_init_funcs()).
 **/
s32 igc_set_mac_type(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	s32 ret_val = IGC_SUCCESS;

	DEBUGFUNC("igc_set_mac_type");

	switch (hw->device_id) {
	case IGC_DEV_ID_82542:
		mac->type = igc_82542;
		break;
	case IGC_DEV_ID_82543GC_FIBER:
	case IGC_DEV_ID_82543GC_COPPER:
		mac->type = igc_82543;
		break;
	case IGC_DEV_ID_82544EI_COPPER:
	case IGC_DEV_ID_82544EI_FIBER:
	case IGC_DEV_ID_82544GC_COPPER:
	case IGC_DEV_ID_82544GC_LOM:
		mac->type = igc_82544;
		break;
	case IGC_DEV_ID_82540EM:
	case IGC_DEV_ID_82540EM_LOM:
	case IGC_DEV_ID_82540EP:
	case IGC_DEV_ID_82540EP_LOM:
	case IGC_DEV_ID_82540EP_LP:
		mac->type = igc_82540;
		break;
	case IGC_DEV_ID_82545EM_COPPER:
	case IGC_DEV_ID_82545EM_FIBER:
		mac->type = igc_82545;
		break;
	case IGC_DEV_ID_82545GM_COPPER:
	case IGC_DEV_ID_82545GM_FIBER:
	case IGC_DEV_ID_82545GM_SERDES:
		mac->type = igc_82545_rev_3;
		break;
	case IGC_DEV_ID_82546EB_COPPER:
	case IGC_DEV_ID_82546EB_FIBER:
	case IGC_DEV_ID_82546EB_QUAD_COPPER:
		mac->type = igc_82546;
		break;
	case IGC_DEV_ID_82546GB_COPPER:
	case IGC_DEV_ID_82546GB_FIBER:
	case IGC_DEV_ID_82546GB_SERDES:
	case IGC_DEV_ID_82546GB_PCIE:
	case IGC_DEV_ID_82546GB_QUAD_COPPER:
	case IGC_DEV_ID_82546GB_QUAD_COPPER_KSP3:
		mac->type = igc_82546_rev_3;
		break;
	case IGC_DEV_ID_82541EI:
	case IGC_DEV_ID_82541EI_MOBILE:
	case IGC_DEV_ID_82541ER_LOM:
		mac->type = igc_82541;
		break;
	case IGC_DEV_ID_82541ER:
	case IGC_DEV_ID_82541GI:
	case IGC_DEV_ID_82541GI_LF:
	case IGC_DEV_ID_82541GI_MOBILE:
		mac->type = igc_82541_rev_2;
		break;
	case IGC_DEV_ID_82547EI:
	case IGC_DEV_ID_82547EI_MOBILE:
		mac->type = igc_82547;
		break;
	case IGC_DEV_ID_82547GI:
		mac->type = igc_82547_rev_2;
		break;
	case IGC_DEV_ID_82571EB_COPPER:
	case IGC_DEV_ID_82571EB_FIBER:
	case IGC_DEV_ID_82571EB_SERDES:
	case IGC_DEV_ID_82571EB_SERDES_DUAL:
	case IGC_DEV_ID_82571EB_SERDES_QUAD:
	case IGC_DEV_ID_82571EB_QUAD_COPPER:
	case IGC_DEV_ID_82571PT_QUAD_COPPER:
	case IGC_DEV_ID_82571EB_QUAD_FIBER:
	case IGC_DEV_ID_82571EB_QUAD_COPPER_LP:
		mac->type = igc_82571;
		break;
	case IGC_DEV_ID_82572EI:
	case IGC_DEV_ID_82572EI_COPPER:
	case IGC_DEV_ID_82572EI_FIBER:
	case IGC_DEV_ID_82572EI_SERDES:
		mac->type = igc_82572;
		break;
	case IGC_DEV_ID_82573E:
	case IGC_DEV_ID_82573E_IAMT:
	case IGC_DEV_ID_82573L:
		mac->type = igc_82573;
		break;
	case IGC_DEV_ID_82574L:
	case IGC_DEV_ID_82574LA:
		mac->type = igc_82574;
		break;
	case IGC_DEV_ID_82583V:
		mac->type = igc_82583;
		break;
	case IGC_DEV_ID_80003ES2LAN_COPPER_DPT:
	case IGC_DEV_ID_80003ES2LAN_SERDES_DPT:
	case IGC_DEV_ID_80003ES2LAN_COPPER_SPT:
	case IGC_DEV_ID_80003ES2LAN_SERDES_SPT:
		mac->type = igc_80003es2lan;
		break;
	case IGC_DEV_ID_ICH8_IFE:
	case IGC_DEV_ID_ICH8_IFE_GT:
	case IGC_DEV_ID_ICH8_IFE_G:
	case IGC_DEV_ID_ICH8_IGP_M:
	case IGC_DEV_ID_ICH8_IGP_M_AMT:
	case IGC_DEV_ID_ICH8_IGP_AMT:
	case IGC_DEV_ID_ICH8_IGP_C:
	case IGC_DEV_ID_ICH8_82567V_3:
		mac->type = igc_ich8lan;
		break;
	case IGC_DEV_ID_ICH9_IFE:
	case IGC_DEV_ID_ICH9_IFE_GT:
	case IGC_DEV_ID_ICH9_IFE_G:
	case IGC_DEV_ID_ICH9_IGP_M:
	case IGC_DEV_ID_ICH9_IGP_M_AMT:
	case IGC_DEV_ID_ICH9_IGP_M_V:
	case IGC_DEV_ID_ICH9_IGP_AMT:
	case IGC_DEV_ID_ICH9_BM:
	case IGC_DEV_ID_ICH9_IGP_C:
	case IGC_DEV_ID_ICH10_R_BM_LM:
	case IGC_DEV_ID_ICH10_R_BM_LF:
	case IGC_DEV_ID_ICH10_R_BM_V:
		mac->type = igc_ich9lan;
		break;
	case IGC_DEV_ID_ICH10_D_BM_LM:
	case IGC_DEV_ID_ICH10_D_BM_LF:
	case IGC_DEV_ID_ICH10_D_BM_V:
		mac->type = igc_ich10lan;
		break;
	case IGC_DEV_ID_PCH_D_HV_DM:
	case IGC_DEV_ID_PCH_D_HV_DC:
	case IGC_DEV_ID_PCH_M_HV_LM:
	case IGC_DEV_ID_PCH_M_HV_LC:
		mac->type = igc_pchlan;
		break;
	case IGC_DEV_ID_PCH2_LV_LM:
	case IGC_DEV_ID_PCH2_LV_V:
		mac->type = igc_pch2lan;
		break;
	case IGC_DEV_ID_PCH_LPT_I217_LM:
	case IGC_DEV_ID_PCH_LPT_I217_V:
	case IGC_DEV_ID_PCH_LPTLP_I218_LM:
	case IGC_DEV_ID_PCH_LPTLP_I218_V:
	case IGC_DEV_ID_PCH_I218_LM2:
	case IGC_DEV_ID_PCH_I218_V2:
	case IGC_DEV_ID_PCH_I218_LM3:
	case IGC_DEV_ID_PCH_I218_V3:
		mac->type = igc_pch_lpt;
		break;
	case IGC_DEV_ID_PCH_SPT_I219_LM:
	case IGC_DEV_ID_PCH_SPT_I219_V:
	case IGC_DEV_ID_PCH_SPT_I219_LM2:
	case IGC_DEV_ID_PCH_SPT_I219_V2:
	case IGC_DEV_ID_PCH_LBG_I219_LM3:
	case IGC_DEV_ID_PCH_SPT_I219_LM4:
	case IGC_DEV_ID_PCH_SPT_I219_V4:
	case IGC_DEV_ID_PCH_SPT_I219_LM5:
	case IGC_DEV_ID_PCH_SPT_I219_V5:
		mac->type = igc_pch_spt;
		break;
	case IGC_DEV_ID_PCH_CNP_I219_LM6:
	case IGC_DEV_ID_PCH_CNP_I219_V6:
	case IGC_DEV_ID_PCH_CNP_I219_LM7:
	case IGC_DEV_ID_PCH_CNP_I219_V7:
	case IGC_DEV_ID_PCH_ICP_I219_LM8:
	case IGC_DEV_ID_PCH_ICP_I219_V8:
	case IGC_DEV_ID_PCH_ICP_I219_LM9:
	case IGC_DEV_ID_PCH_ICP_I219_V9:
		mac->type = igc_pch_cnp;
		break;
	case IGC_DEV_ID_82575EB_COPPER:
	case IGC_DEV_ID_82575EB_FIBER_SERDES:
	case IGC_DEV_ID_82575GB_QUAD_COPPER:
		mac->type = igc_82575;
		break;
	case IGC_DEV_ID_82576:
	case IGC_DEV_ID_82576_FIBER:
	case IGC_DEV_ID_82576_SERDES:
	case IGC_DEV_ID_82576_QUAD_COPPER:
	case IGC_DEV_ID_82576_QUAD_COPPER_ET2:
	case IGC_DEV_ID_82576_NS:
	case IGC_DEV_ID_82576_NS_SERDES:
	case IGC_DEV_ID_82576_SERDES_QUAD:
		mac->type = igc_82576;
		break;
	case IGC_DEV_ID_82576_VF:
	case IGC_DEV_ID_82576_VF_HV:
		mac->type = igc_vfadapt;
		break;
	case IGC_DEV_ID_82580_COPPER:
	case IGC_DEV_ID_82580_FIBER:
	case IGC_DEV_ID_82580_SERDES:
	case IGC_DEV_ID_82580_SGMII:
	case IGC_DEV_ID_82580_COPPER_DUAL:
	case IGC_DEV_ID_82580_QUAD_FIBER:
	case IGC_DEV_ID_DH89XXCC_SGMII:
	case IGC_DEV_ID_DH89XXCC_SERDES:
	case IGC_DEV_ID_DH89XXCC_BACKPLANE:
	case IGC_DEV_ID_DH89XXCC_SFP:
		mac->type = igc_82580;
		break;
	case IGC_DEV_ID_I350_COPPER:
	case IGC_DEV_ID_I350_FIBER:
	case IGC_DEV_ID_I350_SERDES:
	case IGC_DEV_ID_I350_SGMII:
	case IGC_DEV_ID_I350_DA4:
		mac->type = igc_i350;
		break;
	case IGC_DEV_ID_I210_COPPER_FLASHLESS:
	case IGC_DEV_ID_I210_SERDES_FLASHLESS:
	case IGC_DEV_ID_I210_SGMII_FLASHLESS:
	case IGC_DEV_ID_I210_COPPER:
	case IGC_DEV_ID_I210_COPPER_OEM1:
	case IGC_DEV_ID_I210_COPPER_IT:
	case IGC_DEV_ID_I210_FIBER:
	case IGC_DEV_ID_I210_SERDES:
	case IGC_DEV_ID_I210_SGMII:
		mac->type = igc_i210;
		break;
	case IGC_DEV_ID_I211_COPPER:
		mac->type = igc_i211;
		break;
	case IGC_DEV_ID_I225_LM:
	case IGC_DEV_ID_I225_LMVP:
	case IGC_DEV_ID_I225_V:
	case IGC_DEV_ID_I225_K:
	case IGC_DEV_ID_I225_I:
	case IGC_DEV_ID_I225_IT:
	case IGC_DEV_ID_I220_V:
	case IGC_DEV_ID_I225_BLANK_NVM:
	case IGC_DEV_ID_I226_K:
	case IGC_DEV_ID_I226_LMVP:
	case IGC_DEV_ID_I226_LM:
	case IGC_DEV_ID_I226_V:
	case IGC_DEV_ID_I226_IT:
	case IGC_DEV_ID_I226_BLANK_NVM:
		mac->type = igc_i225;
		break;
	case IGC_DEV_ID_I350_VF:
	case IGC_DEV_ID_I350_VF_HV:
		mac->type = igc_vfadapt_i350;
		break;
	case IGC_DEV_ID_I354_BACKPLANE_1GBPS:
	case IGC_DEV_ID_I354_SGMII:
	case IGC_DEV_ID_I354_BACKPLANE_2_5GBPS:
		mac->type = igc_i354;
		break;
	default:
		/* Should never have loaded on this device */
		ret_val = -IGC_ERR_MAC_INIT;
		break;
	}

	return ret_val;
}

/**
 *  igc_setup_init_funcs - Initializes function pointers
 *  @hw: pointer to the HW structure
 *  @init_device: true will initialize the rest of the function pointers
 *		  getting the device ready for use.  false will only set
 *		  MAC type and the function pointers for the other init
 *		  functions.  Passing false will not generate any hardware
 *		  reads or writes.
 *
 *  This function must be called by a driver in order to use the rest
 *  of the 'shared' code files. Called by drivers only.
 **/
s32 igc_setup_init_funcs(struct igc_hw *hw, bool init_device)
{
	s32 ret_val;

	/* Can't do much good without knowing the MAC type. */
	ret_val = igc_set_mac_type(hw);
	if (ret_val) {
		DEBUGOUT("ERROR: MAC type could not be set properly.\n");
		goto out;
	}

	if (!hw->hw_addr) {
		DEBUGOUT("ERROR: Registers not mapped\n");
		ret_val = -IGC_ERR_CONFIG;
		goto out;
	}

	/*
	 * Init function pointers to generic implementations. We do this first
	 * allowing a driver module to override it afterward.
	 */
	igc_init_mac_ops_generic(hw);
	igc_init_phy_ops_generic(hw);
	igc_init_nvm_ops_generic(hw);

	/*
	 * Set up the init function pointers. These are functions within the
	 * adapter family file that sets up function pointers for the rest of
	 * the functions in that family.
	 */
	switch (hw->mac.type) {
	case igc_i225:
		igc_init_function_pointers_i225(hw);
		break;
	default:
		DEBUGOUT("Hardware not supported\n");
		ret_val = -IGC_ERR_CONFIG;
		break;
	}

	/*
	 * Initialize the rest of the function pointers. These require some
	 * register reads/writes in some cases.
	 */
	if (!(ret_val) && init_device) {
		ret_val = igc_init_mac_params(hw);
		if (ret_val)
			goto out;

		ret_val = igc_init_nvm_params(hw);
		if (ret_val)
			goto out;

		ret_val = igc_init_phy_params(hw);
		if (ret_val)
			goto out;
	}

out:
	return ret_val;
}

/**
 *  igc_get_bus_info - Obtain bus information for adapter
 *  @hw: pointer to the HW structure
 *
 *  This will obtain information about the HW bus for which the
 *  adapter is attached and stores it in the hw structure. This is a
 *  function pointer entry point called by drivers.
 **/
s32 igc_get_bus_info(struct igc_hw *hw)
{
	if (hw->mac.ops.get_bus_info)
		return hw->mac.ops.get_bus_info(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_clear_vfta - Clear VLAN filter table
 *  @hw: pointer to the HW structure
 *
 *  This clears the VLAN filter table on the adapter. This is a function
 *  pointer entry point called by drivers.
 **/
void igc_clear_vfta(struct igc_hw *hw)
{
	if (hw->mac.ops.clear_vfta)
		hw->mac.ops.clear_vfta(hw);
}

/**
 *  igc_write_vfta - Write value to VLAN filter table
 *  @hw: pointer to the HW structure
 *  @offset: the 32-bit offset in which to write the value to.
 *  @value: the 32-bit value to write at location offset.
 *
 *  This writes a 32-bit value to a 32-bit offset in the VLAN filter
 *  table. This is a function pointer entry point called by drivers.
 **/
void igc_write_vfta(struct igc_hw *hw, u32 offset, u32 value)
{
	if (hw->mac.ops.write_vfta)
		hw->mac.ops.write_vfta(hw, offset, value);
}

/**
 *  igc_update_mc_addr_list - Update Multicast addresses
 *  @hw: pointer to the HW structure
 *  @mc_addr_list: array of multicast addresses to program
 *  @mc_addr_count: number of multicast addresses to program
 *
 *  Updates the Multicast Table Array.
 *  The caller must have a packed mc_addr_list of multicast addresses.
 **/
void igc_update_mc_addr_list(struct igc_hw *hw, u8 *mc_addr_list,
			       u32 mc_addr_count)
{
	if (hw->mac.ops.update_mc_addr_list)
		hw->mac.ops.update_mc_addr_list(hw, mc_addr_list,
						mc_addr_count);
}

/**
 *  igc_force_mac_fc - Force MAC flow control
 *  @hw: pointer to the HW structure
 *
 *  Force the MAC's flow control settings. Currently no func pointer exists
 *  and all implementations are handled in the generic version of this
 *  function.
 **/
s32 igc_force_mac_fc(struct igc_hw *hw)
{
	return igc_force_mac_fc_generic(hw);
}

/**
 *  igc_check_for_link - Check/Store link connection
 *  @hw: pointer to the HW structure
 *
 *  This checks the link condition of the adapter and stores the
 *  results in the hw->mac structure. This is a function pointer entry
 *  point called by drivers.
 **/
s32 igc_check_for_link(struct igc_hw *hw)
{
	if (hw->mac.ops.check_for_link)
		return hw->mac.ops.check_for_link(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_check_mng_mode - Check management mode
 *  @hw: pointer to the HW structure
 *
 *  This checks if the adapter has manageability enabled.
 *  This is a function pointer entry point called by drivers.
 **/
bool igc_check_mng_mode(struct igc_hw *hw)
{
	if (hw->mac.ops.check_mng_mode)
		return hw->mac.ops.check_mng_mode(hw);

	return false;
}

/**
 *  igc_mng_write_dhcp_info - Writes DHCP info to host interface
 *  @hw: pointer to the HW structure
 *  @buffer: pointer to the host interface
 *  @length: size of the buffer
 *
 *  Writes the DHCP information to the host interface.
 **/
s32 igc_mng_write_dhcp_info(struct igc_hw *hw, u8 *buffer, u16 length)
{
	return igc_mng_write_dhcp_info_generic(hw, buffer, length);
}

/**
 *  igc_reset_hw - Reset hardware
 *  @hw: pointer to the HW structure
 *
 *  This resets the hardware into a known state. This is a function pointer
 *  entry point called by drivers.
 **/
s32 igc_reset_hw(struct igc_hw *hw)
{
	if (hw->mac.ops.reset_hw)
		return hw->mac.ops.reset_hw(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_init_hw - Initialize hardware
 *  @hw: pointer to the HW structure
 *
 *  This inits the hardware readying it for operation. This is a function
 *  pointer entry point called by drivers.
 **/
s32 igc_init_hw(struct igc_hw *hw)
{
	if (hw->mac.ops.init_hw)
		return hw->mac.ops.init_hw(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_setup_link - Configures link and flow control
 *  @hw: pointer to the HW structure
 *
 *  This configures link and flow control settings for the adapter. This
 *  is a function pointer entry point called by drivers. While modules can
 *  also call this, they probably call their own version of this function.
 **/
s32 igc_setup_link(struct igc_hw *hw)
{
	if (hw->mac.ops.setup_link)
		return hw->mac.ops.setup_link(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_get_speed_and_duplex - Returns current speed and duplex
 *  @hw: pointer to the HW structure
 *  @speed: pointer to a 16-bit value to store the speed
 *  @duplex: pointer to a 16-bit value to store the duplex.
 *
 *  This returns the speed and duplex of the adapter in the two 'out'
 *  variables passed in. This is a function pointer entry point called
 *  by drivers.
 **/
s32 igc_get_speed_and_duplex(struct igc_hw *hw, u16 *speed, u16 *duplex)
{
	if (hw->mac.ops.get_link_up_info)
		return hw->mac.ops.get_link_up_info(hw, speed, duplex);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_setup_led - Configures SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  This prepares the SW controllable LED for use and saves the current state
 *  of the LED so it can be later restored. This is a function pointer entry
 *  point called by drivers.
 **/
s32 igc_setup_led(struct igc_hw *hw)
{
	if (hw->mac.ops.setup_led)
		return hw->mac.ops.setup_led(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_cleanup_led - Restores SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  This restores the SW controllable LED to the value saved off by
 *  igc_setup_led. This is a function pointer entry point called by drivers.
 **/
s32 igc_cleanup_led(struct igc_hw *hw)
{
	if (hw->mac.ops.cleanup_led)
		return hw->mac.ops.cleanup_led(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_blink_led - Blink SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  This starts the adapter LED blinking. Request the LED to be setup first
 *  and cleaned up after. This is a function pointer entry point called by
 *  drivers.
 **/
s32 igc_blink_led(struct igc_hw *hw)
{
	if (hw->mac.ops.blink_led)
		return hw->mac.ops.blink_led(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_id_led_init - store LED configurations in SW
 *  @hw: pointer to the HW structure
 *
 *  Initializes the LED config in SW. This is a function pointer entry point
 *  called by drivers.
 **/
s32 igc_id_led_init(struct igc_hw *hw)
{
	if (hw->mac.ops.id_led_init)
		return hw->mac.ops.id_led_init(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_led_on - Turn on SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  Turns the SW defined LED on. This is a function pointer entry point
 *  called by drivers.
 **/
s32 igc_led_on(struct igc_hw *hw)
{
	if (hw->mac.ops.led_on)
		return hw->mac.ops.led_on(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_led_off - Turn off SW controllable LED
 *  @hw: pointer to the HW structure
 *
 *  Turns the SW defined LED off. This is a function pointer entry point
 *  called by drivers.
 **/
s32 igc_led_off(struct igc_hw *hw)
{
	if (hw->mac.ops.led_off)
		return hw->mac.ops.led_off(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_reset_adaptive - Reset adaptive IFS
 *  @hw: pointer to the HW structure
 *
 *  Resets the adaptive IFS. Currently no func pointer exists and all
 *  implementations are handled in the generic version of this function.
 **/
void igc_reset_adaptive(struct igc_hw *hw)
{
	igc_reset_adaptive_generic(hw);
}

/**
 *  igc_update_adaptive - Update adaptive IFS
 *  @hw: pointer to the HW structure
 *
 *  Updates adapter IFS. Currently no func pointer exists and all
 *  implementations are handled in the generic version of this function.
 **/
void igc_update_adaptive(struct igc_hw *hw)
{
	igc_update_adaptive_generic(hw);
}

/**
 *  igc_disable_pcie_master - Disable PCI-Express master access
 *  @hw: pointer to the HW structure
 *
 *  Disables PCI-Express master access and verifies there are no pending
 *  requests. Currently no func pointer exists and all implementations are
 *  handled in the generic version of this function.
 **/
s32 igc_disable_pcie_master(struct igc_hw *hw)
{
	return igc_disable_pcie_master_generic(hw);
}

/**
 *  igc_config_collision_dist - Configure collision distance
 *  @hw: pointer to the HW structure
 *
 *  Configures the collision distance to the default value and is used
 *  during link setup.
 **/
void igc_config_collision_dist(struct igc_hw *hw)
{
	if (hw->mac.ops.config_collision_dist)
		hw->mac.ops.config_collision_dist(hw);
}

/**
 *  igc_rar_set - Sets a receive address register
 *  @hw: pointer to the HW structure
 *  @addr: address to set the RAR to
 *  @index: the RAR to set
 *
 *  Sets a Receive Address Register (RAR) to the specified address.
 **/
int igc_rar_set(struct igc_hw *hw, u8 *addr, u32 index)
{
	if (hw->mac.ops.rar_set)
		return hw->mac.ops.rar_set(hw, addr, index);

	return IGC_SUCCESS;
}

/**
 *  igc_validate_mdi_setting - Ensures valid MDI/MDIX SW state
 *  @hw: pointer to the HW structure
 *
 *  Ensures that the MDI/MDIX SW state is valid.
 **/
s32 igc_validate_mdi_setting(struct igc_hw *hw)
{
	if (hw->mac.ops.validate_mdi_setting)
		return hw->mac.ops.validate_mdi_setting(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_hash_mc_addr - Determines address location in multicast table
 *  @hw: pointer to the HW structure
 *  @mc_addr: Multicast address to hash.
 *
 *  This hashes an address to determine its location in the multicast
 *  table. Currently no func pointer exists and all implementations
 *  are handled in the generic version of this function.
 **/
u32 igc_hash_mc_addr(struct igc_hw *hw, u8 *mc_addr)
{
	return igc_hash_mc_addr_generic(hw, mc_addr);
}

/**
 *  igc_enable_tx_pkt_filtering - Enable packet filtering on TX
 *  @hw: pointer to the HW structure
 *
 *  Enables packet filtering on transmit packets if manageability is enabled
 *  and host interface is enabled.
 *  Currently no func pointer exists and all implementations are handled in the
 *  generic version of this function.
 **/
bool igc_enable_tx_pkt_filtering(struct igc_hw *hw)
{
	return igc_enable_tx_pkt_filtering_generic(hw);
}

/**
 *  igc_mng_host_if_write - Writes to the manageability host interface
 *  @hw: pointer to the HW structure
 *  @buffer: pointer to the host interface buffer
 *  @length: size of the buffer
 *  @offset: location in the buffer to write to
 *  @sum: sum of the data (not checksum)
 *
 *  This function writes the buffer content at the offset given on the host if.
 *  It also does alignment considerations to do the writes in most efficient
 *  way.  Also fills up the sum of the buffer in *buffer parameter.
 **/
s32 igc_mng_host_if_write(struct igc_hw *hw, u8 *buffer, u16 length,
			    u16 offset, u8 *sum)
{
	return igc_mng_host_if_write_generic(hw, buffer, length, offset, sum);
}

/**
 *  igc_mng_write_cmd_header - Writes manageability command header
 *  @hw: pointer to the HW structure
 *  @hdr: pointer to the host interface command header
 *
 *  Writes the command header after does the checksum calculation.
 **/
s32 igc_mng_write_cmd_header(struct igc_hw *hw,
			       struct igc_host_mng_command_header *hdr)
{
	return igc_mng_write_cmd_header_generic(hw, hdr);
}

/**
 *  igc_mng_enable_host_if - Checks host interface is enabled
 *  @hw: pointer to the HW structure
 *
 *  Returns IGC_success upon success, else IGC_ERR_HOST_INTERFACE_COMMAND
 *
 *  This function checks whether the HOST IF is enabled for command operation
 *  and also checks whether the previous command is completed.  It busy waits
 *  in case of previous command is not completed.
 **/
s32 igc_mng_enable_host_if(struct igc_hw *hw)
{
	return igc_mng_enable_host_if_generic(hw);
}

/**
 *  igc_check_reset_block - Verifies PHY can be reset
 *  @hw: pointer to the HW structure
 *
 *  Checks if the PHY is in a state that can be reset or if manageability
 *  has it tied up. This is a function pointer entry point called by drivers.
 **/
s32 igc_check_reset_block(struct igc_hw *hw)
{
	if (hw->phy.ops.check_reset_block)
		return hw->phy.ops.check_reset_block(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_read_phy_reg - Reads PHY register
 *  @hw: pointer to the HW structure
 *  @offset: the register to read
 *  @data: the buffer to store the 16-bit read.
 *
 *  Reads the PHY register and returns the value in data.
 *  This is a function pointer entry point called by drivers.
 **/
s32 igc_read_phy_reg(struct igc_hw *hw, u32 offset, u16 *data)
{
	if (hw->phy.ops.read_reg)
		return hw->phy.ops.read_reg(hw, offset, data);

	return IGC_SUCCESS;
}

/**
 *  igc_write_phy_reg - Writes PHY register
 *  @hw: pointer to the HW structure
 *  @offset: the register to write
 *  @data: the value to write.
 *
 *  Writes the PHY register at offset with the value in data.
 *  This is a function pointer entry point called by drivers.
 **/
s32 igc_write_phy_reg(struct igc_hw *hw, u32 offset, u16 data)
{
	if (hw->phy.ops.write_reg)
		return hw->phy.ops.write_reg(hw, offset, data);

	return IGC_SUCCESS;
}

/**
 *  igc_release_phy - Generic release PHY
 *  @hw: pointer to the HW structure
 *
 *  Return if silicon family does not require a semaphore when accessing the
 *  PHY.
 **/
void igc_release_phy(struct igc_hw *hw)
{
	if (hw->phy.ops.release)
		hw->phy.ops.release(hw);
}

/**
 *  igc_acquire_phy - Generic acquire PHY
 *  @hw: pointer to the HW structure
 *
 *  Return success if silicon family does not require a semaphore when
 *  accessing the PHY.
 **/
s32 igc_acquire_phy(struct igc_hw *hw)
{
	if (hw->phy.ops.acquire)
		return hw->phy.ops.acquire(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_cfg_on_link_up - Configure PHY upon link up
 *  @hw: pointer to the HW structure
 **/
s32 igc_cfg_on_link_up(struct igc_hw *hw)
{
	if (hw->phy.ops.cfg_on_link_up)
		return hw->phy.ops.cfg_on_link_up(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_read_kmrn_reg - Reads register using Kumeran interface
 *  @hw: pointer to the HW structure
 *  @offset: the register to read
 *  @data: the location to store the 16-bit value read.
 *
 *  Reads a register out of the Kumeran interface. Currently no func pointer
 *  exists and all implementations are handled in the generic version of
 *  this function.
 **/
s32 igc_read_kmrn_reg(struct igc_hw *hw, u32 offset, u16 *data)
{
	return igc_read_kmrn_reg_generic(hw, offset, data);
}

/**
 *  igc_write_kmrn_reg - Writes register using Kumeran interface
 *  @hw: pointer to the HW structure
 *  @offset: the register to write
 *  @data: the value to write.
 *
 *  Writes a register to the Kumeran interface. Currently no func pointer
 *  exists and all implementations are handled in the generic version of
 *  this function.
 **/
s32 igc_write_kmrn_reg(struct igc_hw *hw, u32 offset, u16 data)
{
	return igc_write_kmrn_reg_generic(hw, offset, data);
}

/**
 *  igc_get_cable_length - Retrieves cable length estimation
 *  @hw: pointer to the HW structure
 *
 *  This function estimates the cable length and stores them in
 *  hw->phy.min_length and hw->phy.max_length. This is a function pointer
 *  entry point called by drivers.
 **/
s32 igc_get_cable_length(struct igc_hw *hw)
{
	if (hw->phy.ops.get_cable_length)
		return hw->phy.ops.get_cable_length(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_get_phy_info - Retrieves PHY information from registers
 *  @hw: pointer to the HW structure
 *
 *  This function gets some information from various PHY registers and
 *  populates hw->phy values with it. This is a function pointer entry
 *  point called by drivers.
 **/
s32 igc_get_phy_info(struct igc_hw *hw)
{
	if (hw->phy.ops.get_info)
		return hw->phy.ops.get_info(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_phy_hw_reset - Hard PHY reset
 *  @hw: pointer to the HW structure
 *
 *  Performs a hard PHY reset. This is a function pointer entry point called
 *  by drivers.
 **/
s32 igc_phy_hw_reset(struct igc_hw *hw)
{
	if (hw->phy.ops.reset)
		return hw->phy.ops.reset(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_phy_commit - Soft PHY reset
 *  @hw: pointer to the HW structure
 *
 *  Performs a soft PHY reset on those that apply. This is a function pointer
 *  entry point called by drivers.
 **/
s32 igc_phy_commit(struct igc_hw *hw)
{
	if (hw->phy.ops.commit)
		return hw->phy.ops.commit(hw);

	return IGC_SUCCESS;
}

/**
 *  igc_set_d0_lplu_state - Sets low power link up state for D0
 *  @hw: pointer to the HW structure
 *  @active: boolean used to enable/disable lplu
 *
 *  Success returns 0, Failure returns 1
 *
 *  The low power link up (lplu) state is set to the power management level D0
 *  and SmartSpeed is disabled when active is true, else clear lplu for D0
 *  and enable Smartspeed.  LPLU and Smartspeed are mutually exclusive.  LPLU
 *  is used during Dx states where the power conservation is most important.
 *  During driver activity, SmartSpeed should be enabled so performance is
 *  maintained.  This is a function pointer entry point called by drivers.
 **/
s32 igc_set_d0_lplu_state(struct igc_hw *hw, bool active)
{
	if (hw->phy.ops.set_d0_lplu_state)
		return hw->phy.ops.set_d0_lplu_state(hw, active);

	return IGC_SUCCESS;
}

/**
 *  igc_set_d3_lplu_state - Sets low power link up state for D3
 *  @hw: pointer to the HW structure
 *  @active: boolean used to enable/disable lplu
 *
 *  Success returns 0, Failure returns 1
 *
 *  The low power link up (lplu) state is set to the power management level D3
 *  and SmartSpeed is disabled when active is true, else clear lplu for D3
 *  and enable Smartspeed.  LPLU and Smartspeed are mutually exclusive.  LPLU
 *  is used during Dx states where the power conservation is most important.
 *  During driver activity, SmartSpeed should be enabled so performance is
 *  maintained.  This is a function pointer entry point called by drivers.
 **/
s32 igc_set_d3_lplu_state(struct igc_hw *hw, bool active)
{
	if (hw->phy.ops.set_d3_lplu_state)
		return hw->phy.ops.set_d3_lplu_state(hw, active);

	return IGC_SUCCESS;
}

/**
 *  igc_read_mac_addr - Reads MAC address
 *  @hw: pointer to the HW structure
 *
 *  Reads the MAC address out of the adapter and stores it in the HW structure.
 *  Currently no func pointer exists and all implementations are handled in the
 *  generic version of this function.
 **/
s32 igc_read_mac_addr(struct igc_hw *hw)
{
	if (hw->mac.ops.read_mac_addr)
		return hw->mac.ops.read_mac_addr(hw);

	return igc_read_mac_addr_generic(hw);
}

/**
 *  igc_read_pba_string - Read device part number string
 *  @hw: pointer to the HW structure
 *  @pba_num: pointer to device part number
 *  @pba_num_size: size of part number buffer
 *
 *  Reads the product board assembly (PBA) number from the EEPROM and stores
 *  the value in pba_num.
 *  Currently no func pointer exists and all implementations are handled in the
 *  generic version of this function.
 **/
s32 igc_read_pba_string(struct igc_hw *hw, u8 *pba_num, u32 pba_num_size)
{
	return igc_read_pba_string_generic(hw, pba_num, pba_num_size);
}

/**
 *  igc_read_pba_length - Read device part number string length
 *  @hw: pointer to the HW structure
 *  @pba_num_size: size of part number buffer
 *
 *  Reads the product board assembly (PBA) number length from the EEPROM and
 *  stores the value in pba_num.
 *  Currently no func pointer exists and all implementations are handled in the
 *  generic version of this function.
 **/
s32 igc_read_pba_length(struct igc_hw *hw, u32 *pba_num_size)
{
	return igc_read_pba_length_generic(hw, pba_num_size);
}

/**
 *  igc_read_pba_num - Read device part number
 *  @hw: pointer to the HW structure
 *  @pba_num: pointer to device part number
 *
 *  Reads the product board assembly (PBA) number from the EEPROM and stores
 *  the value in pba_num.
 *  Currently no func pointer exists and all implementations are handled in the
 *  generic version of this function.
 **/
s32 igc_read_pba_num(struct igc_hw *hw, u32 *pba_num)
{
	return igc_read_pba_num_generic(hw, pba_num);
}

/**
 *  igc_validate_nvm_checksum - Verifies NVM (EEPROM) checksum
 *  @hw: pointer to the HW structure
 *
 *  Validates the NVM checksum is correct. This is a function pointer entry
 *  point called by drivers.
 **/
s32 igc_validate_nvm_checksum(struct igc_hw *hw)
{
	if (hw->nvm.ops.validate)
		return hw->nvm.ops.validate(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_update_nvm_checksum - Updates NVM (EEPROM) checksum
 *  @hw: pointer to the HW structure
 *
 *  Updates the NVM checksum. Currently no func pointer exists and all
 *  implementations are handled in the generic version of this function.
 **/
s32 igc_update_nvm_checksum(struct igc_hw *hw)
{
	if (hw->nvm.ops.update)
		return hw->nvm.ops.update(hw);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_reload_nvm - Reloads EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Reloads the EEPROM by setting the "Reinitialize from EEPROM" bit in the
 *  extended control register.
 **/
void igc_reload_nvm(struct igc_hw *hw)
{
	if (hw->nvm.ops.reload)
		hw->nvm.ops.reload(hw);
}

/**
 *  igc_read_nvm - Reads NVM (EEPROM)
 *  @hw: pointer to the HW structure
 *  @offset: the word offset to read
 *  @words: number of 16-bit words to read
 *  @data: pointer to the properly sized buffer for the data.
 *
 *  Reads 16-bit chunks of data from the NVM (EEPROM). This is a function
 *  pointer entry point called by drivers.
 **/
s32 igc_read_nvm(struct igc_hw *hw, u16 offset, u16 words, u16 *data)
{
	if (hw->nvm.ops.read)
		return hw->nvm.ops.read(hw, offset, words, data);

	return -IGC_ERR_CONFIG;
}

/**
 *  igc_write_nvm - Writes to NVM (EEPROM)
 *  @hw: pointer to the HW structure
 *  @offset: the word offset to read
 *  @words: number of 16-bit words to write
 *  @data: pointer to the properly sized buffer for the data.
 *
 *  Writes 16-bit chunks of data to the NVM (EEPROM). This is a function
 *  pointer entry point called by drivers.
 **/
s32 igc_write_nvm(struct igc_hw *hw, u16 offset, u16 words, u16 *data)
{
	if (hw->nvm.ops.write)
		return hw->nvm.ops.write(hw, offset, words, data);

	return IGC_SUCCESS;
}

/**
 *  igc_write_8bit_ctrl_reg - Writes 8bit Control register
 *  @hw: pointer to the HW structure
 *  @reg: 32bit register offset
 *  @offset: the register to write
 *  @data: the value to write.
 *
 *  Writes the PHY register at offset with the value in data.
 *  This is a function pointer entry point called by drivers.
 **/
s32 igc_write_8bit_ctrl_reg(struct igc_hw *hw, u32 reg, u32 offset,
			      u8 data)
{
	return igc_write_8bit_ctrl_reg_generic(hw, reg, offset, data);
}

/**
 * igc_power_up_phy - Restores link in case of PHY power down
 * @hw: pointer to the HW structure
 *
 * The phy may be powered down to save power, to turn off link when the
 * driver is unloaded, or wake on lan is not enabled (among others).
 **/
void igc_power_up_phy(struct igc_hw *hw)
{
	if (hw->phy.ops.power_up)
		hw->phy.ops.power_up(hw);

	igc_setup_link(hw);
}

/**
 * igc_power_down_phy - Power down PHY
 * @hw: pointer to the HW structure
 *
 * The phy may be powered down to save power, to turn off link when the
 * driver is unloaded, or wake on lan is not enabled (among others).
 **/
void igc_power_down_phy(struct igc_hw *hw)
{
	if (hw->phy.ops.power_down)
		hw->phy.ops.power_down(hw);
}

/**
 *  igc_power_up_fiber_serdes_link - Power up serdes link
 *  @hw: pointer to the HW structure
 *
 *  Power on the optics and PCS.
 **/
void igc_power_up_fiber_serdes_link(struct igc_hw *hw)
{
	if (hw->mac.ops.power_up_serdes)
		hw->mac.ops.power_up_serdes(hw);
}

/**
 *  igc_shutdown_fiber_serdes_link - Remove link during power down
 *  @hw: pointer to the HW structure
 *
 *  Shutdown the optics and PCS on driver unload.
 **/
void igc_shutdown_fiber_serdes_link(struct igc_hw *hw)
{
	if (hw->mac.ops.shutdown_serdes)
		hw->mac.ops.shutdown_serdes(hw);
}
