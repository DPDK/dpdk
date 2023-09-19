/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#define NFP_SUBSYS "nfp_nsp"

#include <stdio.h>
#include <time.h>

#include <rte_common.h>

#include "nfp_cpp.h"
#include "nfp_logs.h"
#include "nfp_nsp.h"
#include "nfp_platform.h"
#include "nfp_resource.h"

int
nfp_nsp_config_modified(struct nfp_nsp *state)
{
	return state->modified;
}

void
nfp_nsp_config_set_modified(struct nfp_nsp *state,
		int modified)
{
	state->modified = modified;
}

void *
nfp_nsp_config_entries(struct nfp_nsp *state)
{
	return state->entries;
}

uint32_t
nfp_nsp_config_idx(struct nfp_nsp *state)
{
	return state->idx;
}

void
nfp_nsp_config_set_state(struct nfp_nsp *state,
		void *entries,
		uint32_t idx)
{
	state->entries = entries;
	state->idx = idx;
}

void
nfp_nsp_config_clear_state(struct nfp_nsp *state)
{
	state->entries = NULL;
	state->idx = 0;
}

static void
nfp_nsp_print_extended_error(uint32_t ret_val)
{
	uint32_t i;

	if (ret_val == 0)
		return;

	for (i = 0; i < RTE_DIM(nsp_errors); i++)
		if (ret_val == (uint32_t)nsp_errors[i].code)
			PMD_DRV_LOG(ERR, "err msg: %s", nsp_errors[i].msg);
}

static int
nfp_nsp_check(struct nfp_nsp *state)
{
	int err;
	uint64_t reg;
	uint32_t nsp_cpp;
	uint64_t nsp_status;
	struct nfp_cpp *cpp = state->cpp;

	nsp_cpp = nfp_resource_cpp_id(state->res);
	nsp_status = nfp_resource_address(state->res) + NSP_STATUS;

	err = nfp_cpp_readq(cpp, nsp_cpp, nsp_status, &reg);
	if (err < 0) {
		PMD_DRV_LOG(ERR, "NSP - CPP readq failed %d", err);
		return err;
	}

	if (FIELD_GET(NSP_STATUS_MAGIC, reg) != NSP_MAGIC) {
		PMD_DRV_LOG(ERR, "Cannot detect NFP Service Processor");
		return -ENODEV;
	}

	state->ver.major = FIELD_GET(NSP_STATUS_MAJOR, reg);
	state->ver.minor = FIELD_GET(NSP_STATUS_MINOR, reg);

	if (state->ver.major != NSP_MAJOR || state->ver.minor < NSP_MINOR) {
		PMD_DRV_LOG(ERR, "Unsupported ABI %hu.%hu", state->ver.major,
				state->ver.minor);
		return -EINVAL;
	}

	if ((reg & NSP_STATUS_BUSY) != 0) {
		PMD_DRV_LOG(ERR, "Service processor busy!");
		return -EBUSY;
	}

	return 0;
}

/**
 * Prepare for communication and lock the NSP resource.
 *
 * @param cpp
 *   NFP CPP Handle
 */
struct nfp_nsp *
nfp_nsp_open(struct nfp_cpp *cpp)
{
	int err;
	struct nfp_nsp *state;
	struct nfp_resource *res;

	res = nfp_resource_acquire(cpp, NFP_RESOURCE_NSP);
	if (res == NULL) {
		PMD_DRV_LOG(ERR, "NSP - resource acquire failed");
		return NULL;
	}

	state = malloc(sizeof(*state));
	if (state == NULL) {
		nfp_resource_release(res);
		return NULL;
	}
	memset(state, 0, sizeof(*state));
	state->cpp = cpp;
	state->res = res;

	err = nfp_nsp_check(state);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "NSP - check failed");
		nfp_nsp_close(state);
		return NULL;
	}

	return state;
}

/**
 * Clean up and unlock the NSP resource.
 *
 * @param state
 *   NFP SP state
 */
void
nfp_nsp_close(struct nfp_nsp *state)
{
	nfp_resource_release(state->res);
	free(state);
}

uint16_t
nfp_nsp_get_abi_ver_major(struct nfp_nsp *state)
{
	return state->ver.major;
}

uint16_t
nfp_nsp_get_abi_ver_minor(struct nfp_nsp *state)
{
	return state->ver.minor;
}

static int
nfp_nsp_wait_reg(struct nfp_cpp *cpp,
		uint64_t *reg,
		uint32_t nsp_cpp,
		uint64_t addr,
		uint64_t mask,
		uint64_t val)
{
	int err;
	uint32_t count = 0;
	struct timespec wait;

	wait.tv_sec = 0;
	wait.tv_nsec = 25000000;     /* 25ms */

	for (;;) {
		err = nfp_cpp_readq(cpp, nsp_cpp, addr, reg);
		if (err < 0) {
			PMD_DRV_LOG(ERR, "NSP - CPP readq failed");
			return err;
		}

		if ((*reg & mask) == val)
			return 0;

		nanosleep(&wait, 0);
		if (count++ > 1000)     /* 25ms * 1000 = 25s */
			return -ETIMEDOUT;
	}
}

/**
 * Execute a command on the NFP Service Processor
 *
 * @param state
 *   NFP SP state
 * @param arg
 *   NFP command argument structure
 *
 * @return
 *   - 0 for success with no result
 *   - Positive value for NSP completion with a result code
 *   - -EAGAIN if the NSP is not yet present
 *   - -ENODEV if the NSP is not a supported model
 *   - -EBUSY if the NSP is stuck
 *   - -EINTR if interrupted while waiting for completion
 *   - -ETIMEDOUT if the NSP took longer than @timeout_sec seconds to complete
 */
static int
nfp_nsp_command(struct nfp_nsp *state,
		uint16_t code,
		uint32_t option,
		uint32_t buff_cpp,
		uint64_t buff_addr)
{
	int err;
	uint64_t reg;
	uint32_t nsp_cpp;
	uint64_t ret_val;
	uint64_t nsp_base;
	uint64_t nsp_buffer;
	uint64_t nsp_status;
	uint64_t nsp_command;
	struct nfp_cpp *cpp = state->cpp;

	nsp_cpp = nfp_resource_cpp_id(state->res);
	nsp_base = nfp_resource_address(state->res);
	nsp_status = nsp_base + NSP_STATUS;
	nsp_command = nsp_base + NSP_COMMAND;
	nsp_buffer = nsp_base + NSP_BUFFER;

	err = nfp_nsp_check(state);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Check NSP command failed");
		return err;
	}

	if (!FIELD_FIT(NSP_BUFFER_CPP, buff_cpp >> 8) ||
			!FIELD_FIT(NSP_BUFFER_ADDRESS, buff_addr)) {
		PMD_DRV_LOG(ERR, "Host buffer out of reach %08x %" PRIx64,
				buff_cpp, buff_addr);
		return -EINVAL;
	}

	err = nfp_cpp_writeq(cpp, nsp_cpp, nsp_buffer,
			FIELD_PREP(NSP_BUFFER_CPP, buff_cpp >> 8) |
			FIELD_PREP(NSP_BUFFER_ADDRESS, buff_addr));
	if (err < 0)
		return err;

	err = nfp_cpp_writeq(cpp, nsp_cpp, nsp_command,
			FIELD_PREP(NSP_COMMAND_OPTION, option) |
			FIELD_PREP(NSP_COMMAND_CODE, code) |
			FIELD_PREP(NSP_COMMAND_START, 1));
	if (err < 0)
		return err;

	/* Wait for NSP_COMMAND_START to go to 0 */
	err = nfp_nsp_wait_reg(cpp, &reg, nsp_cpp, nsp_command,
			NSP_COMMAND_START, 0);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Error %d waiting for code %#04x to start",
				err, code);
		return err;
	}

	/* Wait for NSP_STATUS_BUSY to go to 0 */
	err = nfp_nsp_wait_reg(cpp, &reg, nsp_cpp, nsp_status,
			NSP_STATUS_BUSY, 0);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Error %d waiting for code %#04x to complete",
				err, code);
		return err;
	}

	err = nfp_cpp_readq(cpp, nsp_cpp, nsp_command, &ret_val);
	if (err < 0)
		return err;

	ret_val = FIELD_GET(NSP_COMMAND_OPTION, ret_val);

	err = FIELD_GET(NSP_STATUS_RESULT, reg);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Result (error) code set: %d (%d) command: %d",
				-err, (int)ret_val, code);
		nfp_nsp_print_extended_error(ret_val);
		return -err;
	}

	return ret_val;
}

#define SZ_1M 0x00100000

static int
nfp_nsp_command_buf(struct nfp_nsp *nsp,
		uint16_t code, uint32_t option,
		const void *in_buf,
		unsigned int in_size,
		void *out_buf,
		unsigned int out_size)
{
	int err;
	int ret;
	uint64_t reg;
	size_t max_size;
	uint32_t cpp_id;
	uint64_t cpp_buf;
	struct nfp_cpp *cpp = nsp->cpp;

	if (nsp->ver.minor < 13) {
		PMD_DRV_LOG(ERR, "NSP: Code 0x%04x with buffer not supported ABI %hu.%hu)",
				code, nsp->ver.major, nsp->ver.minor);
		return -EOPNOTSUPP;
	}

	err = nfp_cpp_readq(cpp, nfp_resource_cpp_id(nsp->res),
			nfp_resource_address(nsp->res) + NSP_DFLT_BUFFER_CONFIG,
			&reg);
	if (err < 0)
		return err;

	max_size = RTE_MAX(in_size, out_size);
	if (FIELD_GET(NSP_DFLT_BUFFER_SIZE_MB, reg) * SZ_1M < max_size) {
		PMD_DRV_LOG(ERR, "NSP: default buffer too small for command 0x%04x (%llu < %zu)",
				code, FIELD_GET(NSP_DFLT_BUFFER_SIZE_MB, reg) * SZ_1M, max_size);
		return -EINVAL;
	}

	err = nfp_cpp_readq(cpp, nfp_resource_cpp_id(nsp->res),
			nfp_resource_address(nsp->res) + NSP_DFLT_BUFFER,
			&reg);
	if (err < 0)
		return err;

	cpp_id = FIELD_GET(NSP_BUFFER_CPP, reg) << 8;
	cpp_buf = FIELD_GET(NSP_BUFFER_ADDRESS, reg);

	if (in_buf != NULL && in_size > 0) {
		err = nfp_cpp_write(cpp, cpp_id, cpp_buf, in_buf, in_size);
		if (err < 0)
			return err;
	}

	/* Zero out remaining part of the buffer */
	if (out_buf != NULL && out_size > 0 && out_size > in_size) {
		memset(out_buf, 0, out_size - in_size);
		err = nfp_cpp_write(cpp, cpp_id, cpp_buf + in_size, out_buf,
				out_size - in_size);
		if (err < 0)
			return err;
	}

	ret = nfp_nsp_command(nsp, code, option, cpp_id, cpp_buf);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "NSP command failed");
		return ret;
	}

	if (out_buf != NULL && out_size > 0) {
		err = nfp_cpp_read(cpp, cpp_id, cpp_buf, out_buf, out_size);
		if (err < 0)
			return err;
	}

	return ret;
}

int
nfp_nsp_wait(struct nfp_nsp *state)
{
	int err;
	int count = 0;
	struct timespec wait;

	wait.tv_sec = 0;
	wait.tv_nsec = 25000000;    /* 25ms */

	for (;;) {
		err = nfp_nsp_command(state, SPCODE_NOOP, 0, 0, 0);
		if (err != -EAGAIN)
			break;

		nanosleep(&wait, 0);

		if (count++ > 1000) {    /* 25ms * 1000 = 25s */
			err = -ETIMEDOUT;
			break;
		}
	}

	if (err != 0)
		PMD_DRV_LOG(ERR, "NSP failed to respond %d", err);

	return err;
}

int
nfp_nsp_device_soft_reset(struct nfp_nsp *state)
{
	return nfp_nsp_command(state, SPCODE_SOFT_RESET, 0, 0, 0);
}

int
nfp_nsp_mac_reinit(struct nfp_nsp *state)
{
	return nfp_nsp_command(state, SPCODE_MAC_INIT, 0, 0, 0);
}

int
nfp_nsp_load_fw(struct nfp_nsp *state,
		void *buf,
		size_t size)
{
	return nfp_nsp_command_buf(state, SPCODE_FW_LOAD, size, buf, size,
			NULL, 0);
}

int
nfp_nsp_read_eth_table(struct nfp_nsp *state,
		void *buf,
		size_t size)
{
	return nfp_nsp_command_buf(state, SPCODE_ETH_RESCAN, size, NULL, 0,
			buf, size);
}

int
nfp_nsp_write_eth_table(struct nfp_nsp *state,
		const void *buf,
		size_t size)
{
	return nfp_nsp_command_buf(state, SPCODE_ETH_CONTROL, size, buf, size,
			NULL, 0);
}

int
nfp_nsp_read_identify(struct nfp_nsp *state,
		void *buf,
		size_t size)
{
	return nfp_nsp_command_buf(state, SPCODE_NSP_IDENTIFY, size, NULL, 0,
			buf, size);
}

int
nfp_nsp_read_sensors(struct nfp_nsp *state,
		uint32_t sensor_mask,
		void *buf,
		size_t size)
{
	return nfp_nsp_command_buf(state, SPCODE_NSP_SENSORS, sensor_mask, NULL,
			0, buf, size);
}
