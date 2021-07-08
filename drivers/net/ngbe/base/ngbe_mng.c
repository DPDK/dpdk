/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ngbe_type.h"
#include "ngbe_mng.h"

/**
 *  ngbe_hic_unlocked - Issue command to manageability block unlocked
 *  @hw: pointer to the HW structure
 *  @buffer: command to write and where the return status will be placed
 *  @length: length of buffer, must be multiple of 4 bytes
 *  @timeout: time in ms to wait for command completion
 *
 *  Communicates with the manageability block. On success return 0
 *  else returns semaphore error when encountering an error acquiring
 *  semaphore or NGBE_ERR_HOST_INTERFACE_COMMAND when command fails.
 *
 *  This function assumes that the NGBE_MNGSEM_SWMBX semaphore is held
 *  by the caller.
 **/
static s32
ngbe_hic_unlocked(struct ngbe_hw *hw, u32 *buffer, u32 length, u32 timeout)
{
	u32 value, loop;
	u16 i, dword_len;

	DEBUGFUNC("ngbe_hic_unlocked");

	if (!length || length > NGBE_PMMBX_BSIZE) {
		DEBUGOUT("Buffer length failure buffersize=%d.\n", length);
		return NGBE_ERR_HOST_INTERFACE_COMMAND;
	}

	/* Calculate length in DWORDs. We must be DWORD aligned */
	if (length % sizeof(u32)) {
		DEBUGOUT("Buffer length failure, not aligned to dword");
		return NGBE_ERR_INVALID_ARGUMENT;
	}

	dword_len = length >> 2;

	/* The device driver writes the relevant command block
	 * into the ram area.
	 */
	for (i = 0; i < dword_len; i++) {
		wr32a(hw, NGBE_MNGMBX, i, cpu_to_le32(buffer[i]));
		buffer[i] = rd32a(hw, NGBE_MNGMBX, i);
	}
	ngbe_flush(hw);

	/* Setting this bit tells the ARC that a new command is pending. */
	wr32m(hw, NGBE_MNGMBXCTL,
	      NGBE_MNGMBXCTL_SWRDY, NGBE_MNGMBXCTL_SWRDY);

	/* Check command completion */
	loop = po32m(hw, NGBE_MNGMBXCTL,
		NGBE_MNGMBXCTL_FWRDY, NGBE_MNGMBXCTL_FWRDY,
		&value, timeout, 1000);
	if (!loop || !(value & NGBE_MNGMBXCTL_FWACK)) {
		DEBUGOUT("Command has failed with no status valid.\n");
		return NGBE_ERR_HOST_INTERFACE_COMMAND;
	}

	return 0;
}

/**
 *  ngbe_host_interface_command - Issue command to manageability block
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
 *  semaphore or NGBE_ERR_HOST_INTERFACE_COMMAND when command fails.
 **/
static s32
ngbe_host_interface_command(struct ngbe_hw *hw, u32 *buffer,
				 u32 length, u32 timeout, bool return_data)
{
	u32 hdr_size = sizeof(struct ngbe_hic_hdr);
	struct ngbe_hic_hdr *resp = (struct ngbe_hic_hdr *)buffer;
	u16 buf_len;
	s32 err;
	u32 bi;
	u32 dword_len;

	DEBUGFUNC("ngbe_host_interface_command");

	if (length == 0 || length > NGBE_PMMBX_BSIZE) {
		DEBUGOUT("Buffer length failure buffersize=%d.\n", length);
		return NGBE_ERR_HOST_INTERFACE_COMMAND;
	}

	/* Take management host interface semaphore */
	err = hw->mac.acquire_swfw_sync(hw, NGBE_MNGSEM_SWMBX);
	if (err)
		return err;

	err = ngbe_hic_unlocked(hw, buffer, length, timeout);
	if (err)
		goto rel_out;

	if (!return_data)
		goto rel_out;

	/* Calculate length in DWORDs */
	dword_len = hdr_size >> 2;

	/* first pull in the header so we know the buffer length */
	for (bi = 0; bi < dword_len; bi++)
		buffer[bi] = rd32a(hw, NGBE_MNGMBX, bi);

	/*
	 * If there is any thing in data position pull it in
	 * Read Flash command requires reading buffer length from
	 * two byes instead of one byte
	 */
	if (resp->cmd == 0x30) {
		for (; bi < dword_len + 2; bi++)
			buffer[bi] = rd32a(hw, NGBE_MNGMBX, bi);

		buf_len = (((u16)(resp->cmd_or_resp.ret_status) << 3)
				  & 0xF00) | resp->buf_len;
		hdr_size += (2 << 2);
	} else {
		buf_len = resp->buf_len;
	}
	if (!buf_len)
		goto rel_out;

	if (length < buf_len + hdr_size) {
		DEBUGOUT("Buffer not large enough for reply message.\n");
		err = NGBE_ERR_HOST_INTERFACE_COMMAND;
		goto rel_out;
	}

	/* Calculate length in DWORDs, add 3 for odd lengths */
	dword_len = (buf_len + 3) >> 2;

	/* Pull in the rest of the buffer (bi is where we left off) */
	for (; bi <= dword_len; bi++)
		buffer[bi] = rd32a(hw, NGBE_MNGMBX, bi);

rel_out:
	hw->mac.release_swfw_sync(hw, NGBE_MNGSEM_SWMBX);

	return err;
}

s32 ngbe_hic_check_cap(struct ngbe_hw *hw)
{
	struct ngbe_hic_read_shadow_ram command;
	s32 err;
	int i;

	DEBUGFUNC("\n");

	command.hdr.req.cmd = FW_EEPROM_CHECK_STATUS;
	command.hdr.req.buf_lenh = 0;
	command.hdr.req.buf_lenl = 0;
	command.hdr.req.checksum = FW_DEFAULT_CHECKSUM;

	/* convert offset from words to bytes */
	command.address = 0;
	/* one word */
	command.length = 0;

	for (i = 0; i <= FW_CEM_MAX_RETRIES; i++) {
		err = ngbe_host_interface_command(hw, (u32 *)&command,
				sizeof(command),
				NGBE_HI_COMMAND_TIMEOUT, true);
		if (err)
			continue;

		command.hdr.rsp.ret_status &= 0x1F;
		if (command.hdr.rsp.ret_status !=
			FW_CEM_RESP_STATUS_SUCCESS)
			err = NGBE_ERR_HOST_INTERFACE_COMMAND;

		break;
	}

	if (!err && command.address != FW_CHECKSUM_CAP_ST_PASS)
		err = NGBE_ERR_EEPROM_CHECKSUM;

	return err;
}
