/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "txgbe_type.h"
#include "txgbe_mng.h"

/**
 *  txgbe_calculate_checksum - Calculate checksum for buffer
 *  @buffer: pointer to EEPROM
 *  @length: size of EEPROM to calculate a checksum for
 *  Calculates the checksum for some buffer on a specified length.  The
 *  checksum calculated is returned.
 **/
static u8
txgbe_calculate_checksum(u8 *buffer, u32 length)
{
	u32 i;
	u8 sum = 0;

	for (i = 0; i < length; i++)
		sum += buffer[i];

	return (u8)(0 - sum);
}

/**
 *  txgbe_hic_unlocked - Issue command to manageability block unlocked
 *  @hw: pointer to the HW structure
 *  @buffer: command to write and where the return status will be placed
 *  @length: length of buffer, must be multiple of 4 bytes
 *  @timeout: time in ms to wait for command completion
 *
 *  Communicates with the manageability block. On success return 0
 *  else returns semaphore error when encountering an error acquiring
 *  semaphore or TXGBE_ERR_HOST_INTERFACE_COMMAND when command fails.
 *
 *  This function assumes that the TXGBE_MNGSEM_SWMBX semaphore is held
 *  by the caller.
 **/
static s32
txgbe_hic_unlocked(struct txgbe_hw *hw, u32 *buffer, u32 length, u32 timeout)
{
	u32 value, loop;
	u16 i, dword_len;

	dword_len = length >> 2;

	txgbe_flush(hw);
	/* The device driver writes the relevant command block
	 * into the ram area.
	 */
	for (i = 0; i < dword_len; i++) {
		wr32a(hw, TXGBE_MNGMBX, i, cpu_to_le32(buffer[i]));
		buffer[i] = rd32a(hw, TXGBE_MNGMBX, i);
	}
	txgbe_flush(hw);

	/* Setting this bit tells the ARC that a new command is pending. */
	wr32m(hw, TXGBE_MNGMBXCTL,
	      TXGBE_MNGMBXCTL_SWRDY, TXGBE_MNGMBXCTL_SWRDY);

	/* Check command completion */
	loop = po32m(hw, TXGBE_MNGMBXCTL,
		TXGBE_MNGMBXCTL_FWRDY, TXGBE_MNGMBXCTL_FWRDY,
		&value, timeout, 1000);
	if (!loop || !(value & TXGBE_MNGMBXCTL_FWACK)) {
		DEBUGOUT("Command has failed with no status valid.");
		return TXGBE_ERR_HOST_INTERFACE_COMMAND;
	}

	if ((rd32(hw, TXGBE_MNGMBX) & 0xff0000) >> 16 == 0x80) {
		DEBUGOUT("It's unknown command.");
		return TXGBE_ERR_MNG_ACCESS_FAILED;
	}

	return 0;
}

/**
 *  txgbe_host_interface_command - Issue command to manageability block
 *  @hw: pointer to the HW structure
 *  @buffer: contains the command to write and where the return status will
 *   be placed
 *  @length: length of buffer, must be multiple of 4 bytes
 *  @timeout: time in ms to wait for command completion
 *  @return_data: read and return data from the buffer (true) or not (false)
 *   Needed because FW structures are big endian and decoding of
 *   these fields can be 8 bit or 16 bit based on command. Decoding
 *   is not easily understood without making a table of commands.
 *   So we will leave this up to the caller to read back the data
 *   in these cases.
 *
 *  Communicates with the manageability block. On success return 0
 *  else returns semaphore error when encountering an error acquiring
 *  semaphore or TXGBE_ERR_HOST_INTERFACE_COMMAND when command fails.
 **/
static s32
txgbe_host_interface_command(struct txgbe_hw *hw, u32 *buffer,
				 u32 length, u32 timeout, bool return_data)
{
	u32 hdr_size = sizeof(struct txgbe_hic_hdr);
	struct txgbe_hic_hdr *resp = (struct txgbe_hic_hdr *)buffer;
	u16 buf_len;
	s32 err = 0;
	u32 bi;
	u32 dword_len;

	if (length == 0 || length > TXGBE_PMMBX_BSIZE) {
		DEBUGOUT("Buffer length failure buffersize=%d.", length);
		return TXGBE_ERR_HOST_INTERFACE_COMMAND;
	}

	/* Calculate length in DWORDs. We must be DWORD aligned */
	if (length % sizeof(u32)) {
		DEBUGOUT("Buffer length failure, not aligned to dword");
		return TXGBE_ERR_INVALID_ARGUMENT;
	}

	/* Take management host interface semaphore */
	err = hw->mac.acquire_swfw_sync(hw, TXGBE_MNGSEM_SWMBX);
	if (err)
		return err;

	err = txgbe_hic_unlocked(hw, buffer, length, timeout);
	if (err)
		goto rel_out;

	if (!return_data)
		goto rel_out;

	/* Calculate length in DWORDs */
	dword_len = hdr_size >> 2;

	/* first pull in the header so we know the buffer length */
	for (bi = 0; bi < dword_len; bi++)
		buffer[bi] = rd32a(hw, TXGBE_MNGMBX, bi);

	buf_len = resp->buf_len;
	if (!buf_len)
		goto rel_out;

	if (length < buf_len + hdr_size) {
		DEBUGOUT("Buffer not large enough for reply message.");
		err = TXGBE_ERR_HOST_INTERFACE_COMMAND;
		goto rel_out;
	}

	/* Calculate length in DWORDs, add 3 for odd lengths */
	dword_len = (buf_len + 3) >> 2;

	/* Pull in the rest of the buffer (bi is where we left off) */
	for (; bi <= dword_len; bi++)
		buffer[bi] = rd32a(hw, TXGBE_MNGMBX, bi);

rel_out:
	hw->mac.release_swfw_sync(hw, TXGBE_MNGSEM_SWMBX);

	return err;
}

static s32
txgbe_host_interface_command_aml(struct txgbe_hw *hw, u32 *buffer,
				 u32 length, u32 timeout, bool return_data)
{
	u32 hdr_size = sizeof(struct txgbe_hic_hdr);
	struct txgbe_hic_hdr *resp = (struct txgbe_hic_hdr *)buffer;
	struct txgbe_hic_hdr *recv_hdr;
	u16 buf_len;
	s32 err = 0;
	u32 bi, i;
	u32 dword_len;
	u8 send_cmd;

	if (length == 0 || length > TXGBE_PMMBX_BSIZE) {
		DEBUGOUT("Buffer length failure buffersize=%d.", length);
		return TXGBE_ERR_HOST_INTERFACE_COMMAND;
	}

	/* Calculate length in DWORDs. We must be DWORD aligned */
	if (length % sizeof(u32)) {
		DEBUGOUT("Buffer length failure, not aligned to dword");
		return TXGBE_ERR_INVALID_ARGUMENT;
	}

	/* try to get lock */
	while (rte_atomic32_test_and_set(&hw->swfw_busy)) {
		timeout--;
		if (!timeout)
			return TXGBE_ERR_TIMEOUT;
		usec_delay(1000);
	}

	/* index to unique seq id for each mbox message */
	resp->index = hw->swfw_index;
	send_cmd = resp->cmd;

	/* Calculate length in DWORDs */
	dword_len = length >> 2;

	/* write data to SW-FW mbox array */
	for (i = 0; i < dword_len; i++) {
		wr32a(hw, TXGBE_AML_MNG_MBOX_SW2FW,
				i, rte_cpu_to_le_32(buffer[i]));
		/* write flush */
		rd32a(hw, TXGBE_AML_MNG_MBOX_SW2FW, i);
	}

	/* amlite: generate interrupt to notify FW */
	wr32m(hw, TXGBE_AML_MNG_MBOX_CTL_SW2FW,
			  TXGBE_AML_MNG_MBOX_NOTIFY, 0);
	wr32m(hw, TXGBE_AML_MNG_MBOX_CTL_SW2FW,
			  TXGBE_AML_MNG_MBOX_NOTIFY, TXGBE_AML_MNG_MBOX_NOTIFY);

	/* Calculate length in DWORDs */
	dword_len = hdr_size >> 2;

	/* polling reply from FW */
	timeout = 50;
	do {
		timeout--;
		usec_delay(1000);

		/* read hdr */
		for (bi = 0; bi < dword_len; bi++)
			buffer[bi] = rd32a(hw, TXGBE_AML_MNG_MBOX_FW2SW, bi);

		/* check hdr */
		recv_hdr = (struct txgbe_hic_hdr *)buffer;

		if (recv_hdr->cmd == send_cmd &&
		    recv_hdr->index == hw->swfw_index)
			break;
	} while (timeout);

	if (!timeout) {
		PMD_DRV_LOG(ERR, "Polling from FW messages timeout, cmd is 0x%x, index is %d",
			send_cmd, hw->swfw_index);
		err = TXGBE_ERR_TIMEOUT;
		goto rel_out;
	}

	/* expect no reply from FW then return */
	/* release lock if return */
	if (!return_data)
		goto rel_out;

	/* If there is any thing in data position pull it in */
	buf_len = recv_hdr->buf_len;
	if (buf_len == 0)
		goto rel_out;

	if (length < buf_len + hdr_size) {
		DEBUGOUT("Buffer not large enough for reply message.");
		err = TXGBE_ERR_HOST_INTERFACE_COMMAND;
		goto rel_out;
	}

	/* Calculate length in DWORDs, add 3 for odd lengths */
	dword_len = (buf_len + 3) >> 2;
	for (; bi <= dword_len; bi++)
		buffer[bi] = rd32a(hw, TXGBE_AML_MNG_MBOX_FW2SW, bi);

rel_out:
	/* index++, index replace txgbe_hic_hdr.checksum */
	hw->swfw_index = resp->index == TXGBE_HIC_HDR_INDEX_MAX ?
					0 : resp->index + 1;
	rte_atomic32_clear(&hw->swfw_busy);

	return err;
}

/**
 *  txgbe_hic_sr_read - Read EEPROM word using a host interface cmd
 *  assuming that the semaphore is already obtained.
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to read
 *  @data: word read from the EEPROM
 *
 *  Reads a 16 bit word from the EEPROM using the hostif.
 **/
s32 txgbe_hic_sr_read(struct txgbe_hw *hw, u32 addr, u8 *buf, int len)
{
	struct txgbe_hic_read_shadow_ram command;
	u32 value;
	int err, i = 0, j = 0;
	u32 mngmbx_addr;

	if (hw->mac.type == txgbe_mac_aml || hw->mac.type == txgbe_mac_aml40)
		mngmbx_addr = TXGBE_AML_MNG_MBOX_FW2SW;
	else
		mngmbx_addr = TXGBE_MNGMBX;

	if (len > TXGBE_PMMBX_DATA_SIZE)
		return TXGBE_ERR_HOST_INTERFACE_COMMAND;

	memset(&command, 0, sizeof(command));
	command.hdr.req.cmd = FW_READ_SHADOW_RAM_CMD;
	command.hdr.req.buf_lenh = 0;
	command.hdr.req.buf_lenl = FW_READ_SHADOW_RAM_LEN;
	command.hdr.req.checksum = FW_DEFAULT_CHECKSUM;
	command.address = cpu_to_be32(addr);
	command.length = cpu_to_be16(len);

	if (hw->mac.type == txgbe_mac_aml || hw->mac.type == txgbe_mac_aml40)
		err = txgbe_host_interface_command_aml(hw, (u32 *)&command,
				sizeof(command), TXGBE_HI_COMMAND_TIMEOUT, false);
	else
		err = txgbe_hic_unlocked(hw, (u32 *)&command,
				sizeof(command), TXGBE_HI_COMMAND_TIMEOUT);
	if (err)
		return err;

	while (i < (len >> 2)) {
		value = rd32a(hw, mngmbx_addr, FW_NVM_DATA_OFFSET + i);
		((u32 *)buf)[i] = value;
		i++;
	}

	value = rd32a(hw, mngmbx_addr, FW_NVM_DATA_OFFSET + i);
	for (i <<= 2; i < len; i++)
		((u8 *)buf)[i] = ((u8 *)&value)[j++];

	return 0;
}

/**
 *  txgbe_hic_sr_write - Write EEPROM word using hostif
 *  @hw: pointer to hardware structure
 *  @offset: offset of  word in the EEPROM to write
 *  @data: word write to the EEPROM
 *
 *  Write a 16 bit word to the EEPROM using the hostif.
 **/
s32 txgbe_hic_sr_write(struct txgbe_hw *hw, u32 addr, u8 *buf, int len)
{
	struct txgbe_hic_write_shadow_ram command;
	u32 value;
	int err = 0, i = 0, j = 0;

	if (len > TXGBE_PMMBX_DATA_SIZE)
		return TXGBE_ERR_HOST_INTERFACE_COMMAND;

	memset(&command, 0, sizeof(command));
	command.hdr.req.cmd = FW_WRITE_SHADOW_RAM_CMD;
	command.hdr.req.buf_lenh = 0;
	command.hdr.req.buf_lenl = FW_WRITE_SHADOW_RAM_LEN;
	command.hdr.req.checksum = FW_DEFAULT_CHECKSUM;
	command.address = cpu_to_be32(addr);
	command.length = cpu_to_be16(len);

	while (i < (len >> 2)) {
		value = ((u32 *)buf)[i];
		wr32a(hw, TXGBE_MNGMBX, FW_NVM_DATA_OFFSET + i, value);
		i++;
	}

	for (i <<= 2; i < len; i++)
		((u8 *)&value)[j++] = ((u8 *)buf)[i];

	wr32a(hw, TXGBE_MNGMBX, FW_NVM_DATA_OFFSET + (i >> 2), value);

	UNREFERENCED_PARAMETER(&command);

	return err;
}

s32 txgbe_close_notify(struct txgbe_hw *hw)
{
	u32 tmp;
	s32 status;
	struct txgbe_hic_write_shadow_ram buffer;

	buffer.hdr.req.cmd = FW_DW_CLOSE_NOTIFY;
	buffer.hdr.req.buf_lenh = 0;
	buffer.hdr.req.buf_lenl = 0;
	buffer.hdr.req.checksum = FW_DEFAULT_CHECKSUM;

	/* one word */
	buffer.length = 0;
	buffer.address = 0;

	status = txgbe_host_interface_command(hw, (u32 *)&buffer,
					      sizeof(buffer),
					      TXGBE_HI_COMMAND_TIMEOUT, false);
	if (status)
		return status;

	tmp = rd32a(hw, TXGBE_MNGMBX, 1);
	if (tmp == TXGBE_CHECKSUM_CAP_ST_PASS)
		status = 0;
	else
		status = TXGBE_ERR_EEPROM_CHECKSUM;

	return status;
}

s32 txgbe_open_notify(struct txgbe_hw *hw)
{
	u32 tmp;
	s32 status;
	struct txgbe_hic_write_shadow_ram buffer;

	buffer.hdr.req.cmd = FW_DW_OPEN_NOTIFY;
	buffer.hdr.req.buf_lenh = 0;
	buffer.hdr.req.buf_lenl = 0;
	buffer.hdr.req.checksum = FW_DEFAULT_CHECKSUM;

	/* one word */
	buffer.length = 0;
	buffer.address = 0;

	status = txgbe_host_interface_command(hw, (u32 *)&buffer,
					      sizeof(buffer),
					      TXGBE_HI_COMMAND_TIMEOUT, false);
	if (status)
		return status;

	tmp = rd32a(hw, TXGBE_MNGMBX, 1);
	if (tmp == TXGBE_CHECKSUM_CAP_ST_PASS)
		status = 0;
	else
		status = TXGBE_ERR_EEPROM_CHECKSUM;

	return status;
}

/**
 *  txgbe_hic_set_drv_ver - Sends driver version to firmware
 *  @hw: pointer to the HW structure
 *  @maj: driver version major number
 *  @min: driver version minor number
 *  @build: driver version build number
 *  @sub: driver version sub build number
 *  @len: unused
 *  @driver_ver: unused
 *
 *  Sends driver version number to firmware through the manageability
 *  block.  On success return 0
 *  else returns TXGBE_ERR_SWFW_SYNC when encountering an error acquiring
 *  semaphore or TXGBE_ERR_HOST_INTERFACE_COMMAND when command fails.
 **/
s32 txgbe_hic_set_drv_ver(struct txgbe_hw *hw, u8 maj, u8 min,
				 u8 build, u8 sub, u16 len,
				 const char *driver_ver)
{
	struct txgbe_hic_drv_info fw_cmd;
	int i;
	s32 ret_val = 0;

	UNREFERENCED_PARAMETER(len, driver_ver);

	fw_cmd.hdr.cmd = FW_CEM_CMD_DRIVER_INFO;
	fw_cmd.hdr.buf_len = FW_CEM_CMD_DRIVER_INFO_LEN;
	fw_cmd.hdr.cmd_or_resp.cmd_resv = FW_CEM_CMD_RESERVED;
	fw_cmd.port_num = (u8)hw->bus.func;
	fw_cmd.ver_maj = maj;
	fw_cmd.ver_min = min;
	fw_cmd.ver_build = build;
	fw_cmd.ver_sub = sub;
	fw_cmd.hdr.checksum = 0;
	fw_cmd.pad = 0;
	fw_cmd.pad2 = 0;
	fw_cmd.hdr.checksum = txgbe_calculate_checksum((u8 *)&fw_cmd,
				(FW_CEM_HDR_LEN + fw_cmd.hdr.buf_len));

	for (i = 0; i <= FW_CEM_MAX_RETRIES; i++) {
		ret_val = txgbe_host_interface_command(hw, (u32 *)&fw_cmd,
						       sizeof(fw_cmd),
						       TXGBE_HI_COMMAND_TIMEOUT,
						       true);
		if (ret_val != 0)
			continue;

		if (fw_cmd.hdr.cmd_or_resp.ret_status ==
		    FW_CEM_RESP_STATUS_SUCCESS)
			ret_val = 0;
		else
			ret_val = TXGBE_ERR_HOST_INTERFACE_COMMAND;

		break;
	}

	return ret_val;
}

/**
 *  txgbe_hic_reset - send reset cmd to fw
 *  @hw: pointer to hardware structure
 *
 *  Sends reset cmd to firmware through the manageability
 *  block.  On success return 0
 *  else returns TXGBE_ERR_SWFW_SYNC when encountering an error acquiring
 *  semaphore or TXGBE_ERR_HOST_INTERFACE_COMMAND when command fails.
 **/
s32
txgbe_hic_reset(struct txgbe_hw *hw)
{
	struct txgbe_hic_reset reset_cmd;
	int i;
	s32 err = 0;

	reset_cmd.hdr.cmd = FW_RESET_CMD;
	reset_cmd.hdr.buf_len = FW_RESET_LEN;
	reset_cmd.hdr.cmd_or_resp.cmd_resv = FW_CEM_CMD_RESERVED;
	reset_cmd.lan_id = hw->bus.lan_id;
	reset_cmd.reset_type = (u16)hw->reset_type;
	reset_cmd.hdr.checksum = 0;
	reset_cmd.hdr.checksum = txgbe_calculate_checksum((u8 *)&reset_cmd,
				(FW_CEM_HDR_LEN + reset_cmd.hdr.buf_len));

	for (i = 0; i <= FW_CEM_MAX_RETRIES; i++) {
		err = txgbe_host_interface_command(hw, (u32 *)&reset_cmd,
						       sizeof(reset_cmd),
						       TXGBE_HI_COMMAND_TIMEOUT,
						       true);
		if (err != 0)
			continue;

		if (reset_cmd.hdr.cmd_or_resp.ret_status ==
		    FW_CEM_RESP_STATUS_SUCCESS)
			err = 0;
		else
			err = TXGBE_ERR_HOST_INTERFACE_COMMAND;

		break;
	}

	return err;
}

/**
 * txgbe_mng_present - returns true when management capability is present
 * @hw: pointer to hardware structure
 */
bool
txgbe_mng_present(struct txgbe_hw *hw)
{
	if (hw->mac.type == txgbe_mac_unknown)
		return false;

	return !!rd32m(hw, TXGBE_STAT, TXGBE_STAT_MNGINIT);
}

/**
 * txgbe_mng_enabled - Is the manageability engine enabled?
 * @hw: pointer to hardware structure
 *
 * Returns true if the manageability engine is enabled.
 **/
bool
txgbe_mng_enabled(struct txgbe_hw *hw)
{
	UNREFERENCED_PARAMETER(hw);
	/* firmware does not control laser */
	return false;
}

s32 txgbe_hic_get_lldp(struct txgbe_hw *hw)
{
	struct txgbe_hic_write_lldp buffer;
	s32 err = 0;

	buffer.hdr.cmd = FW_LLDP_GET_CMD;
	buffer.hdr.buf_len = 0x1;
	buffer.hdr.cmd_or_resp.cmd_resv = FW_CEM_CMD_RESERVED;
	buffer.hdr.checksum = FW_DEFAULT_CHECKSUM;
	buffer.func = hw->bus.lan_id;

	err = txgbe_host_interface_command(hw, (u32 *)&buffer, sizeof(buffer),
					   TXGBE_HI_COMMAND_TIMEOUT, true);
	if (err)
		return err;

	if (buffer.hdr.cmd_or_resp.ret_status == FW_CEM_RESP_STATUS_SUCCESS) {
		/* this field returns the status of LLDP */
		if (buffer.func)
			hw->lldp_enabled = true;
		else
			hw->lldp_enabled = false;
	} else {
		err = TXGBE_ERR_HOST_INTERFACE_COMMAND;
	}

	return err;
}

s32 txgbe_hic_set_lldp(struct txgbe_hw *hw, bool on)
{
	struct txgbe_hic_write_lldp buffer;

	if (on)
		buffer.hdr.cmd = FW_LLDP_SET_CMD_ON;
	else
		buffer.hdr.cmd = FW_LLDP_SET_CMD_OFF;
	buffer.hdr.buf_len = 0x1;
	buffer.hdr.cmd_or_resp.cmd_resv = FW_CEM_CMD_RESERVED;
	buffer.hdr.checksum = FW_DEFAULT_CHECKSUM;
	buffer.func = hw->bus.lan_id;

	return txgbe_host_interface_command(hw, (u32 *)&buffer, sizeof(buffer),
					    TXGBE_HI_COMMAND_TIMEOUT, false);
}

s32 txgbe_hic_ephy_set_link(struct txgbe_hw *hw, u8 speed, u8 autoneg, u8 duplex)
{
	struct txgbe_hic_ephy_setlink buffer;
	s32 status;
	int i;

	buffer.hdr.cmd = FW_PHY_CONFIG_LINK_CMD;
	buffer.hdr.buf_len = sizeof(struct txgbe_hic_ephy_setlink) - sizeof(struct txgbe_hic_hdr);
	buffer.hdr.cmd_or_resp.cmd_resv = FW_CEM_CMD_RESERVED;

	buffer.fec_mode = TXGBE_PHY_FEC_AUTO;
	buffer.speed = speed;
	buffer.autoneg = autoneg;
	buffer.duplex = duplex;

	for (i = 0; i <= FW_CEM_MAX_RETRIES; i++) {
		status = txgbe_host_interface_command(hw, (u32 *)&buffer,
						      sizeof(buffer),
						      TXGBE_HI_COMMAND_TIMEOUT_SHORT, true);
		if (status != 0) {
			msleep(1);
			continue;
		}

		if (buffer.hdr.cmd_or_resp.ret_status ==
				FW_CEM_RESP_STATUS_SUCCESS)
			status = 0;
		else
			status = TXGBE_ERR_HOST_INTERFACE_COMMAND;

		break;
	}

	return status;
}
