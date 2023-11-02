/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Netronome Systems, Inc.
 * All rights reserved.
 */

#include "nfp_nsp.h"

#include <nfp_platform.h>
#include <rte_common.h>

#include "nfp_logs.h"
#include "nfp_resource.h"

/* Offsets relative to the CSR base */
#define NSP_STATUS              0x00
#define   NSP_STATUS_MAGIC      GENMASK_ULL(63, 48)
#define   NSP_STATUS_MAJOR      GENMASK_ULL(47, 44)
#define   NSP_STATUS_MINOR      GENMASK_ULL(43, 32)
#define   NSP_STATUS_CODE       GENMASK_ULL(31, 16)
#define   NSP_STATUS_RESULT     GENMASK_ULL(15, 8)
#define   NSP_STATUS_BUSY       RTE_BIT64(0)

#define NSP_COMMAND             0x08
#define   NSP_COMMAND_OPTION    GENMASK_ULL(63, 32)
#define   NSP_COMMAND_VER_MAJOR GENMASK_ULL(31, 28)
#define   NSP_COMMAND_CODE      GENMASK_ULL(27, 16)
#define   NSP_COMMAND_DMA_BUF   RTE_BIT64(1)
#define   NSP_COMMAND_START     RTE_BIT64(0)

/* CPP address to retrieve the data from */
#define NSP_BUFFER              0x10
#define   NSP_BUFFER_CPP        GENMASK_ULL(63, 40)
#define   NSP_BUFFER_ADDRESS    GENMASK_ULL(39, 0)

#define NSP_DFLT_BUFFER         0x18
#define   NSP_DFLT_BUFFER_CPP          GENMASK_ULL(63, 40)
#define   NSP_DFLT_BUFFER_ADDRESS      GENMASK_ULL(39, 0)

#define NSP_DFLT_BUFFER_CONFIG  0x20
#define   NSP_DFLT_BUFFER_SIZE_4KB     GENMASK_ULL(15, 8)
#define   NSP_DFLT_BUFFER_SIZE_MB      GENMASK_ULL(7, 0)

#define NSP_MAGIC               0xab10

/*
 * ABI major version is bumped separately without resetting minor
 * version when the change in NSP is not compatible to old driver.
 */
#define NSP_MAJOR               1

/*
 * ABI minor version is bumped when new feature is introduced
 * while old driver can still work without this new feature.
 */
#define NSP_MINOR               8

#define NSP_CODE_MAJOR          GENMASK_ULL(15, 12)
#define NSP_CODE_MINOR          GENMASK_ULL(11, 0)

#define NFP_FW_LOAD_RET_MAJOR   GENMASK_ULL(15, 8)
#define NFP_FW_LOAD_RET_MINOR   GENMASK_ULL(23, 16)

enum nfp_nsp_cmd {
	SPCODE_NOOP             = 0, /* No operation */
	SPCODE_SOFT_RESET       = 1, /* Soft reset the NFP */
	SPCODE_FW_DEFAULT       = 2, /* Load default (UNDI) FW */
	SPCODE_PHY_INIT         = 3, /* Initialize the PHY */
	SPCODE_MAC_INIT         = 4, /* Initialize the MAC */
	SPCODE_PHY_RXADAPT      = 5, /* Re-run PHY RX Adaptation */
	SPCODE_FW_LOAD          = 6, /* Load fw from buffer, len in option */
	SPCODE_ETH_RESCAN       = 7, /* Rescan ETHs, write ETH_TABLE to buf */
	SPCODE_ETH_CONTROL      = 8, /* Update media config from buffer */
	SPCODE_NSP_WRITE_FLASH  = 11, /* Load and flash image from buffer */
	SPCODE_NSP_SENSORS      = 12, /* Read NSP sensor(s) */
	SPCODE_NSP_IDENTIFY     = 13, /* Read NSP version */
	SPCODE_FW_STORED        = 16, /* If no FW loaded, load flash app FW */
	SPCODE_HWINFO_LOOKUP    = 17, /* Lookup HWinfo with overwrites etc. */
	SPCODE_HWINFO_SET       = 18, /* Set HWinfo entry */
	SPCODE_FW_LOADED        = 19, /* Is application firmware loaded */
	SPCODE_VERSIONS         = 21, /* Report FW versions */
	SPCODE_READ_SFF_EEPROM  = 22, /* Read module EEPROM */
	SPCODE_READ_MEDIA       = 23, /* Get the supported/advertised media for a port */
};

static const struct {
	uint32_t code;
	const char *msg;
} nsp_errors[] = {
	{ 6010, "could not map to phy for port" },
	{ 6011, "not an allowed rate/lanes for port" },
	{ 6012, "not an allowed rate/lanes for port" },
	{ 6013, "high/low error, change other port first" },
	{ 6014, "config not found in flash" },
};

struct nfp_nsp {
	struct nfp_cpp *cpp;
	struct nfp_resource *res;
	struct {
		uint16_t major;
		uint16_t minor;
	} ver;

	/** Eth table config state */
	bool modified;
	uint32_t idx;
	void *entries;
};

/* NFP command argument structure */
struct nfp_nsp_command_arg {
	uint16_t code;         /**< NFP SP Command Code */
	bool dma;              /**< @buf points to a host buffer, not NSP buffer */
	bool error_quiet;      /**< Don't print command error/warning */
	uint32_t timeout_sec;  /**< Timeout value to wait for completion in seconds */
	uint32_t option;       /**< NSP Command Argument */
	uint64_t buf;          /**< NSP Buffer Address */
	/** Callback for interpreting option if error occurred */
	void (*error_cb)(struct nfp_nsp *state, uint32_t ret_val);
};

/* NFP command with buffer argument structure */
struct nfp_nsp_command_buf_arg {
	struct nfp_nsp_command_arg arg;  /**< NFP command argument structure */
	const void *in_buf;              /**< Buffer with data for input */
	void *out_buf;                   /**< Buffer for output data */
	uint32_t in_size;                /**< Size of @in_buf */
	uint32_t out_size;               /**< Size of @out_buf */
};

struct nfp_cpp *
nfp_nsp_cpp(struct nfp_nsp *state)
{
	return state->cpp;
}

bool
nfp_nsp_config_modified(struct nfp_nsp *state)
{
	return state->modified;
}

void
nfp_nsp_config_set_modified(struct nfp_nsp *state,
		bool modified)
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
		if (ret_val == nsp_errors[i].code)
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

	if (state->ver.major > NSP_MAJOR || state->ver.minor < NSP_MINOR) {
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
nfp_nsp_command_real(struct nfp_nsp *state,
		const struct nfp_nsp_command_arg *arg)
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

	err = nfp_cpp_writeq(cpp, nsp_cpp, nsp_buffer, arg->buf);
	if (err < 0)
		return err;

	err = nfp_cpp_writeq(cpp, nsp_cpp, nsp_command,
			FIELD_PREP(NSP_COMMAND_OPTION, arg->option) |
			FIELD_PREP(NSP_COMMAND_VER_MAJOR, state->ver.major) |
			FIELD_PREP(NSP_COMMAND_CODE, arg->code) |
			FIELD_PREP(NSP_COMMAND_DMA_BUF, arg->dma) |
			FIELD_PREP(NSP_COMMAND_START, 1));
	if (err < 0)
		return err;

	/* Wait for NSP_COMMAND_START to go to 0 */
	err = nfp_nsp_wait_reg(cpp, &reg, nsp_cpp, nsp_command,
			NSP_COMMAND_START, 0);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Error %d waiting for code %#04x to start",
				err, arg->code);
		return err;
	}

	/* Wait for NSP_STATUS_BUSY to go to 0 */
	err = nfp_nsp_wait_reg(cpp, &reg, nsp_cpp, nsp_status,
			NSP_STATUS_BUSY, 0);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Error %d waiting for code %#04x to complete",
				err, arg->code);
		return err;
	}

	err = nfp_cpp_readq(cpp, nsp_cpp, nsp_command, &ret_val);
	if (err < 0)
		return err;

	ret_val = FIELD_GET(NSP_COMMAND_OPTION, ret_val);

	err = FIELD_GET(NSP_STATUS_RESULT, reg);
	if (err != 0) {
		if (!arg->error_quiet)
			PMD_DRV_LOG(WARNING, "Result (error) code set: %d (%d) command: %d",
					-err, (int)ret_val, arg->code);

		if (arg->error_cb != 0)
			arg->error_cb(state, ret_val);
		else
			nfp_nsp_print_extended_error(ret_val);

		return -err;
	}

	return ret_val;
}

static int
nfp_nsp_command(struct nfp_nsp *state,
		uint16_t code)
{
	const struct nfp_nsp_command_arg arg = {
		.code = code,
	};

	return nfp_nsp_command_real(state, &arg);
}

static int
nfp_nsp_command_buf_def(struct nfp_nsp *nsp,
		struct nfp_nsp_command_buf_arg *arg)
{
	int err;
	int ret;
	uint64_t reg;
	uint32_t cpp_id;
	uint64_t cpp_buf;
	struct nfp_cpp *cpp = nsp->cpp;

	err = nfp_cpp_readq(cpp, nfp_resource_cpp_id(nsp->res),
			nfp_resource_address(nsp->res) + NSP_DFLT_BUFFER,
			&reg);
	if (err < 0)
		return err;

	cpp_id = FIELD_GET(NSP_DFLT_BUFFER_CPP, reg) << 8;
	cpp_buf = FIELD_GET(NSP_DFLT_BUFFER_ADDRESS, reg);

	if (arg->in_buf != NULL && arg->in_size > 0) {
		err = nfp_cpp_write(cpp, cpp_id, cpp_buf,
				arg->in_buf, arg->in_size);
		if (err < 0)
			return err;
	}

	/* Zero out remaining part of the buffer */
	if (arg->out_buf != NULL && arg->out_size > arg->in_size) {
		err = nfp_cpp_write(cpp, cpp_id, cpp_buf + arg->in_size,
				arg->out_buf, arg->out_size - arg->in_size);
		if (err < 0)
			return err;
	}

	if (!FIELD_FIT(NSP_BUFFER_CPP, cpp_id >> 8) ||
			!FIELD_FIT(NSP_BUFFER_ADDRESS, cpp_buf)) {
		PMD_DRV_LOG(ERR, "Buffer out of reach %#08x %#016lx",
				cpp_id, cpp_buf);
		return -EINVAL;
	}

	arg->arg.buf = FIELD_PREP(NSP_BUFFER_CPP, cpp_id >> 8) |
			FIELD_PREP(NSP_BUFFER_ADDRESS, cpp_buf);
	ret = nfp_nsp_command_real(nsp, &arg->arg);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "NSP command failed");
		return ret;
	}

	if (arg->out_buf != NULL && arg->out_size > 0) {
		err = nfp_cpp_read(cpp, cpp_id, cpp_buf,
				arg->out_buf, arg->out_size);
		if (err < 0)
			return err;
	}

	return ret;
}

#define SZ_1M 0x00100000
#define SZ_4K 0x00001000

static int
nfp_nsp_command_buf(struct nfp_nsp *nsp,
		struct nfp_nsp_command_buf_arg *arg)
{
	int err;
	size_t size;
	uint64_t reg;
	size_t max_size;
	struct nfp_cpp *cpp = nsp->cpp;

	if (nsp->ver.minor < 13) {
		PMD_DRV_LOG(ERR, "NSP: Code %#04x with buffer not supported ABI %hu.%hu)",
				arg->arg.code, nsp->ver.major, nsp->ver.minor);
		return -EOPNOTSUPP;
	}

	err = nfp_cpp_readq(cpp, nfp_resource_cpp_id(nsp->res),
			nfp_resource_address(nsp->res) + NSP_DFLT_BUFFER_CONFIG,
			&reg);
	if (err < 0)
		return err;

	max_size = RTE_MAX(arg->in_size, arg->out_size);
	size = FIELD_GET(NSP_DFLT_BUFFER_SIZE_MB, reg) * SZ_1M +
			FIELD_GET(NSP_DFLT_BUFFER_SIZE_4KB, reg) * SZ_4K;
	if (size < max_size) {
		PMD_DRV_LOG(ERR, "NSP: default buffer too small for command %#04x (%zu < %zu)",
				arg->arg.code, size, max_size);
		return -EINVAL;
	}

	return nfp_nsp_command_buf_def(nsp, arg);
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
		err = nfp_nsp_command(state, SPCODE_NOOP);
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
	return nfp_nsp_command(state, SPCODE_SOFT_RESET);
}

int
nfp_nsp_mac_reinit(struct nfp_nsp *state)
{
	return nfp_nsp_command(state, SPCODE_MAC_INIT);
}

static void
nfp_nsp_load_fw_extended_msg(struct nfp_nsp *state,
		uint32_t ret_val)
{
	uint32_t minor;
	uint32_t major;
	static const char * const major_msg[] = {
		/* 0 */ "Firmware from driver loaded",
		/* 1 */ "Firmware from flash loaded",
		/* 2 */ "Firmware loading failure",
	};
	static const char * const minor_msg[] = {
		/*  0 */ "",
		/*  1 */ "no named partition on flash",
		/*  2 */ "error reading from flash",
		/*  3 */ "can not deflate",
		/*  4 */ "not a trusted file",
		/*  5 */ "can not parse FW file",
		/*  6 */ "MIP not found in FW file",
		/*  7 */ "null firmware name in MIP",
		/*  8 */ "FW version none",
		/*  9 */ "FW build number none",
		/* 10 */ "no FW selection policy HWInfo key found",
		/* 11 */ "static FW selection policy",
		/* 12 */ "FW version has precedence",
		/* 13 */ "different FW application load requested",
		/* 14 */ "development build",
	};

	major = FIELD_GET(NFP_FW_LOAD_RET_MAJOR, ret_val);
	minor = FIELD_GET(NFP_FW_LOAD_RET_MINOR, ret_val);

	if (!nfp_nsp_has_stored_fw_load(state))
		return;

	if (major >= RTE_DIM(major_msg))
		PMD_DRV_LOG(INFO, "FW loading status: %x", ret_val);
	else if (minor >= RTE_DIM(minor_msg))
		PMD_DRV_LOG(INFO, "%s, reason code: %d", major_msg[major], minor);
	else
		PMD_DRV_LOG(INFO, "%s%c %s", major_msg[major],
				minor != 0 ? ',' : '.', minor_msg[minor]);
}

int
nfp_nsp_load_fw(struct nfp_nsp *state,
		void *buf,
		size_t size)
{
	int ret;
	struct nfp_nsp_command_buf_arg load_fw = {
		{
			.code     = SPCODE_FW_LOAD,
			.option   = size,
			.error_cb = nfp_nsp_load_fw_extended_msg,
		},
		.in_buf  = buf,
		.in_size = size,
	};

	ret = nfp_nsp_command_buf(state, &load_fw);
	if (ret < 0)
		return ret;

	nfp_nsp_load_fw_extended_msg(state, ret);

	return 0;
}

bool
nfp_nsp_fw_loaded(struct nfp_nsp *state)
{
	return nfp_nsp_command(state, SPCODE_FW_LOADED) > 0;
}

int
nfp_nsp_read_eth_table(struct nfp_nsp *state,
		void *buf,
		size_t size)
{
	struct nfp_nsp_command_buf_arg eth_rescan = {
		{
			.code   = SPCODE_ETH_RESCAN,
			.option = size,
		},
		.out_buf  = buf,
		.out_size = size,
	};

	return nfp_nsp_command_buf(state, &eth_rescan);
}

int
nfp_nsp_write_eth_table(struct nfp_nsp *state,
		const void *buf,
		size_t size)
{
	struct nfp_nsp_command_buf_arg eth_ctrl = {
		{
			.code   = SPCODE_ETH_CONTROL,
			.option = size,
		},
		.in_buf  = buf,
		.in_size = size,
	};

	return nfp_nsp_command_buf(state, &eth_ctrl);
}

int
nfp_nsp_read_identify(struct nfp_nsp *state,
		void *buf,
		size_t size)
{
	struct nfp_nsp_command_buf_arg identify = {
		{
			.code   = SPCODE_NSP_IDENTIFY,
			.option = size,
		},
		.out_buf  = buf,
		.out_size = size,
	};

	return nfp_nsp_command_buf(state, &identify);
}

int
nfp_nsp_read_sensors(struct nfp_nsp *state,
		uint32_t sensor_mask,
		void *buf,
		size_t size)
{
	struct nfp_nsp_command_buf_arg sensors = {
		{
			.code   = SPCODE_NSP_SENSORS,
			.option = sensor_mask,
		},
		.out_buf  = buf,
		.out_size = size,
	};

	return nfp_nsp_command_buf(state, &sensors);
}
