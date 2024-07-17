/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <string.h>

#include "nthw_drv.h"
#include "i2c_nim.h"
#include "ntlog.h"
#include "nt_util.h"
#include "ntnic_mod_reg.h"
#include "qsfp_registers.h"
#include "nim_defines.h"

#define NIM_READ false
#define NIM_WRITE true
#define NIM_PAGE_SEL_REGISTER 127
#define NIM_I2C_0XA0 0xA0	/* Basic I2C address */


static bool page_addressing(nt_nim_identifier_t id)
{
	switch (id) {
	case NT_NIM_QSFP:
	case NT_NIM_QSFP_PLUS:
		return true;

	default:
		NT_LOG(DBG, NTNIC, "Unknown NIM identifier %d\n", id);
		return false;
	}
}

static nt_nim_identifier_t translate_nimid(const nim_i2c_ctx_t *ctx)
{
	return (nt_nim_identifier_t)ctx->nim_id;
}

static int nim_read_write_i2c_data(nim_i2c_ctx_p ctx, bool do_write, uint16_t lin_addr,
	uint8_t i2c_addr, uint8_t a_reg_addr, uint8_t seq_cnt,
	uint8_t *p_data)
{
	/* Divide i2c_addr by 2 because nthw_iic_read/writeData multiplies by 2 */
	const uint8_t i2c_devaddr = i2c_addr / 2U;
	(void)lin_addr;	/* Unused */

	if (do_write) {
		if (ctx->type == I2C_HWIIC) {
			return nthw_iic_write_data(&ctx->hwiic, i2c_devaddr, a_reg_addr, seq_cnt,
					p_data);

		} else {
			return 0;
		}

	} else if (ctx->type == I2C_HWIIC) {
		return nthw_iic_read_data(&ctx->hwiic, i2c_devaddr, a_reg_addr, seq_cnt, p_data);

	} else {
		return 0;
	}
}

/*
 * ------------------------------------------------------------------------------
 * Selects a new page for page addressing. This is only relevant if the NIM
 * supports this. Since page switching can take substantial time the current page
 * select is read and subsequently only changed if necessary.
 * Important:
 * XFP Standard 8077, Ver 4.5, Page 61 states that:
 * If the host attempts to write a table select value which is not supported in
 * a particular module, the table select byte will revert to 01h.
 * This can lead to some surprising result that some pages seems to be duplicated.
 * ------------------------------------------------------------------------------
 */

static int nim_setup_page(nim_i2c_ctx_p ctx, uint8_t page_sel)
{
	uint8_t curr_page_sel;

	/* Read the current page select value */
	if (nim_read_write_i2c_data(ctx, NIM_READ, NIM_PAGE_SEL_REGISTER, NIM_I2C_0XA0,
			NIM_PAGE_SEL_REGISTER, sizeof(curr_page_sel),
			&curr_page_sel) != 0) {
		return -1;
	}

	/* Only write new page select value if necessary */
	if (page_sel != curr_page_sel) {
		if (nim_read_write_i2c_data(ctx, NIM_WRITE, NIM_PAGE_SEL_REGISTER, NIM_I2C_0XA0,
				NIM_PAGE_SEL_REGISTER, sizeof(page_sel),
				&page_sel) != 0) {
			return -1;
		}
	}

	return 0;
}

static int nim_read_write_data_lin(nim_i2c_ctx_p ctx, bool m_page_addressing, uint16_t lin_addr,
	uint16_t length, uint8_t *p_data, bool do_write)
{
	uint16_t i;
	uint8_t a_reg_addr;	/* The actual register address in I2C device */
	uint8_t i2c_addr;
	int block_size = 128;	/* Equal to size of MSA pages */
	int seq_cnt;
	int max_seq_cnt = 1;
	int multi_byte = 1;	/* One byte per I2C register is default */

	for (i = 0; i < length;) {
		bool use_page_select = false;

		/*
		 * Find out how much can be read from the current block in case of
		 * single byte access
		 */
		if (multi_byte == 1)
			max_seq_cnt = block_size - (lin_addr % block_size);

		if (m_page_addressing) {
			if (lin_addr >= 128) {	/* Only page setup above this address */
				use_page_select = true;

				/* Map to [128..255] of 0xA0 device */
				a_reg_addr = (uint8_t)(block_size + (lin_addr % block_size));

			} else {
				a_reg_addr = (uint8_t)lin_addr;
			}

			i2c_addr = NIM_I2C_0XA0;/* Base I2C address */

		} else if (lin_addr >= 256) {
			/* Map to address [0..255] of 0xA2 device */
			a_reg_addr = (uint8_t)(lin_addr - 256);
			i2c_addr = NIM_I2C_0XA2;

		} else {
			a_reg_addr = (uint8_t)lin_addr;
			i2c_addr = NIM_I2C_0XA0;/* Base I2C address */
		}

		/* Now actually do the reading/writing */
		seq_cnt = length - i;	/* Number of remaining bytes */

		if (seq_cnt > max_seq_cnt)
			seq_cnt = max_seq_cnt;

		/*
		 * Read a number of bytes without explicitly specifying a new address.
		 * This can speed up I2C access since automatic incrementation of the
		 * I2C device internal address counter can be used. It also allows
		 * a HW implementation, that can deal with block access.
		 * Furthermore it also allows for access to data that must be accessed
		 * as 16bit words reading two bytes at each address eg PHYs.
		 */
		if (use_page_select) {
			if (nim_setup_page(ctx, (uint8_t)((lin_addr / 128) - 1)) != 0) {
				NT_LOG(ERR, NTNIC,
					"Cannot set up page for linear address %u\n", lin_addr);
				return -1;
			}
		}

		if (nim_read_write_i2c_data(ctx, do_write, lin_addr, i2c_addr, a_reg_addr,
				(uint8_t)seq_cnt, p_data) != 0) {
			NT_LOG(ERR, NTNIC, " Call to nim_read_write_i2c_data failed\n");
			return -1;
		}

		p_data += seq_cnt;
		i = (uint16_t)(i + seq_cnt);
		lin_addr = (uint16_t)(lin_addr + (seq_cnt / multi_byte));
	}

	return 0;
}

static int read_data_lin(nim_i2c_ctx_p ctx, uint16_t lin_addr, uint16_t length, void *data)
{
	/* Wrapper for using Mutex for QSFP TODO */
	return nim_read_write_data_lin(ctx, page_addressing(ctx->nim_id), lin_addr, length, data,
			NIM_READ);
}

static int nim_read_id(nim_i2c_ctx_t *ctx)
{
	/* We are only reading the first byte so we don't care about pages here. */
	const bool USE_PAGE_ADDRESSING = false;

	if (nim_read_write_data_lin(ctx, USE_PAGE_ADDRESSING, NIM_IDENTIFIER_ADDR,
			sizeof(ctx->nim_id), &ctx->nim_id, NIM_READ) != 0) {
		return -1;
	}

	return 0;
}

static int i2c_nim_common_construct(nim_i2c_ctx_p ctx)
{
	ctx->nim_id = 0;
	int res;

	if (ctx->type == I2C_HWIIC)
		res = nim_read_id(ctx);

	else
		res = -1;

	if (res) {
		NT_LOG(ERR, PMD, "Can't read NIM id.");
		return res;
	}

	memset(ctx->vendor_name, 0, sizeof(ctx->vendor_name));
	memset(ctx->prod_no, 0, sizeof(ctx->prod_no));
	memset(ctx->serial_no, 0, sizeof(ctx->serial_no));
	memset(ctx->date, 0, sizeof(ctx->date));
	memset(ctx->rev, 0, sizeof(ctx->rev));

	ctx->content_valid = false;
	memset(ctx->len_info, 0, sizeof(ctx->len_info));
	ctx->pwr_level_req = 0;
	ctx->pwr_level_cur = 0;
	ctx->avg_pwr = false;
	ctx->tx_disable = false;
	ctx->lane_idx = -1;
	ctx->lane_count = 1;
	ctx->options = 0;
	return 0;
}

/*
 * Read vendor information at a certain address. Any trailing whitespace is
 * removed and a missing string termination in the NIM data is handled.
 */
static int nim_read_vendor_info(nim_i2c_ctx_p ctx, uint16_t addr, uint8_t max_len, char *p_data)
{
	const bool pg_addr = page_addressing(ctx->nim_id);
	int i;
	/* Subtract "1" from max_len that includes a terminating "0" */

	if (nim_read_write_data_lin(ctx, pg_addr, addr, (uint8_t)(max_len - 1), (uint8_t *)p_data,
			NIM_READ) != 0) {
		return -1;
	}

	/* Terminate at first found white space */
	for (i = 0; i < max_len - 1; i++) {
		if (*p_data == ' ' || *p_data == '\n' || *p_data == '\t' || *p_data == '\v' ||
			*p_data == '\f' || *p_data == '\r') {
			*p_data = '\0';
			return 0;
		}

		p_data++;
	}

	/*
	 * Add line termination as the very last character, if it was missing in the
	 * NIM data
	 */
	*p_data = '\0';
	return 0;
}

static void qsfp_read_vendor_info(nim_i2c_ctx_t *ctx)
{
	nim_read_vendor_info(ctx, QSFP_VENDOR_NAME_LIN_ADDR, sizeof(ctx->vendor_name),
		ctx->vendor_name);
	nim_read_vendor_info(ctx, QSFP_VENDOR_PN_LIN_ADDR, sizeof(ctx->prod_no), ctx->prod_no);
	nim_read_vendor_info(ctx, QSFP_VENDOR_SN_LIN_ADDR, sizeof(ctx->serial_no), ctx->serial_no);
	nim_read_vendor_info(ctx, QSFP_VENDOR_DATE_LIN_ADDR, sizeof(ctx->date), ctx->date);
	nim_read_vendor_info(ctx, QSFP_VENDOR_REV_LIN_ADDR, (uint8_t)(sizeof(ctx->rev) - 2),
		ctx->rev);	/*OBS Only two bytes*/
}
static int qsfp_nim_state_build(nim_i2c_ctx_t *ctx, sfp_nim_state_t *state)
{
	int res = 0;	/* unused due to no readings from HW */

	assert(ctx && state);
	assert(ctx->nim_id != NT_NIM_UNKNOWN && "Nim is not initialized");

	(void)memset(state, 0, sizeof(*state));

	switch (ctx->nim_id) {
	case 12U:
		state->br = 10U;/* QSFP: 4 x 1G = 4G */
		break;

	case 13U:
		state->br = 103U;	/* QSFP+: 4 x 10G = 40G */
		break;

	default:
		NT_LOG(INF, NIM, "nim_id = %u is not an QSFP/QSFP+ module\n", ctx->nim_id);
		res = -1;
	}

	return res;
}

int nim_state_build(nim_i2c_ctx_t *ctx, sfp_nim_state_t *state)
{
	return qsfp_nim_state_build(ctx, state);
}

const char *nim_id_to_text(uint8_t nim_id)
{
	switch (nim_id) {
	case 0x0:
		return "UNKNOWN";

	case 0x0C:
		return "QSFP";

	case 0x0D:
		return "QSFP+";

	default:
		return "ILLEGAL!";
	}
}

/*
 * Disable laser for specific lane or all lanes
 */
int nim_qsfp_plus_nim_set_tx_laser_disable(nim_i2c_ctx_p ctx, bool disable, int lane_idx)
{
	uint8_t value;
	uint8_t mask;
	const bool pg_addr = page_addressing(ctx->nim_id);

	if (lane_idx < 0)	/* If no lane is specified then all lanes */
		mask = QSFP_SOFT_TX_ALL_DISABLE_BITS;

	else
		mask = (uint8_t)(1U << lane_idx);

	if (nim_read_write_data_lin(ctx, pg_addr, QSFP_CONTROL_STATUS_LIN_ADDR, sizeof(value),
			&value, NIM_READ) != 0) {
		return -1;
	}

	if (disable)
		value |= mask;

	else
		value &= (uint8_t)(~mask);

	if (nim_read_write_data_lin(ctx, pg_addr, QSFP_CONTROL_STATUS_LIN_ADDR, sizeof(value),
			&value, NIM_WRITE) != 0) {
		return -1;
	}

	return 0;
}

/*
 * Import length info in various units from NIM module data and convert to meters
 */
static void nim_import_len_info(nim_i2c_ctx_p ctx, uint8_t *p_nim_len_info, uint16_t *p_nim_units)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(ctx->len_info); i++)
		if (*(p_nim_len_info + i) == 255) {
			ctx->len_info[i] = 65535;

		} else {
			uint32_t len = *(p_nim_len_info + i) * *(p_nim_units + i);

			if (len > 65535)
				ctx->len_info[i] = 65535;

			else
				ctx->len_info[i] = (uint16_t)len;
		}
}

static int qsfpplus_read_basic_data(nim_i2c_ctx_t *ctx)
{
	const bool pg_addr = page_addressing(ctx->nim_id);
	uint8_t options;
	uint8_t value;
	uint8_t nim_len_info[5];
	uint16_t nim_units[5] = { 1000, 2, 1, 1, 1 };	/* QSFP MSA units in meters */
	const char *yes_no[2] = { "No", "Yes" };
	(void)yes_no;
	NT_LOG(DBG, NTNIC, "Instance %d: NIM id: %s (%d)\n", ctx->instance,
		nim_id_to_text(ctx->nim_id), ctx->nim_id);

	/* Read DMI options */
	if (nim_read_write_data_lin(ctx, pg_addr, QSFP_DMI_OPTION_LIN_ADDR, sizeof(options),
			&options, NIM_READ) != 0) {
		return -1;
	}

	ctx->avg_pwr = options & QSFP_DMI_AVG_PWR_BIT;
	NT_LOG(DBG, NTNIC, "Instance %d: NIM options: (DMI: Yes, AvgPwr: %s)\n", ctx->instance,
		yes_no[ctx->avg_pwr]);

	qsfp_read_vendor_info(ctx);
	NT_LOG(DBG, PMD,
		"Instance %d: NIM info: (Vendor: %s, PN: %s, SN: %s, Date: %s, Rev: %s)\n",
		ctx->instance, ctx->vendor_name, ctx->prod_no, ctx->serial_no, ctx->date, ctx->rev);

	if (nim_read_write_data_lin(ctx, pg_addr, QSFP_SUP_LEN_INFO_LIN_ADDR, sizeof(nim_len_info),
			nim_len_info, NIM_READ) != 0) {
		return -1;
	}

	/*
	 * Returns supported length information in meters for various fibers as 5 indivi-
	 * dual values: [SM(9um), EBW(50um), MM(50um), MM(62.5um), Copper]
	 * If no length information is available for a certain entry, the returned value
	 * will be zero. This will be the case for SFP modules - EBW entry.
	 * If the MSBit is set the returned value in the lower 31 bits indicates that the
	 * supported length is greater than this.
	 */

	nim_import_len_info(ctx, nim_len_info, nim_units);

	/* Read required power level */
	if (nim_read_write_data_lin(ctx, pg_addr, QSFP_EXTENDED_IDENTIFIER, sizeof(value), &value,
			NIM_READ) != 0) {
		return -1;
	}

	/*
	 * Get power class according to SFF-8636 Rev 2.7, Table 6-16, Page 43:
	 * If power class >= 5 setHighPower must be called for the module to be fully
	 * functional
	 */
	if ((value & QSFP_POWER_CLASS_BITS_5_7) == 0) {
		/* NIM in power class 1 - 4 */
		ctx->pwr_level_req = (uint8_t)(((value & QSFP_POWER_CLASS_BITS_1_4) >> 6) + 1);

	} else {
		/* NIM in power class 5 - 7 */
		ctx->pwr_level_req = (uint8_t)((value & QSFP_POWER_CLASS_BITS_5_7) + 4);
	}

	return 0;
}

static void qsfpplus_find_port_params(nim_i2c_ctx_p ctx)
{
	uint8_t device_tech;
	read_data_lin(ctx, QSFP_TRANSMITTER_TYPE_LIN_ADDR, sizeof(device_tech), &device_tech);

	switch (device_tech & 0xF0) {
	case 0xA0:	/* Copper cable unequalized */
		break;

	case 0xC0:	/* Copper cable, near and far end limiting active equalizers */
	case 0xD0:	/* Copper cable, far end limiting active equalizers */
	case 0xE0:	/* Copper cable, near end limiting active equalizers */
		break;

	default:/* Optical */
		ctx->port_type = NT_PORT_TYPE_QSFP_PLUS;
		break;
	}
}

static void qsfpplus_set_speed_mask(nim_i2c_ctx_p ctx)
{
	ctx->speed_mask = (ctx->lane_idx < 0) ? NT_LINK_SPEED_40G : (NT_LINK_SPEED_10G);
}

static void qsfpplus_construct(nim_i2c_ctx_p ctx, int8_t lane_idx)
{
	assert(lane_idx < 4);
	ctx->lane_idx = lane_idx;
	ctx->lane_count = 4;
}

static int qsfpplus_preinit(nim_i2c_ctx_p ctx, int8_t lane_idx)
{
	qsfpplus_construct(ctx, lane_idx);
	int res = qsfpplus_read_basic_data(ctx);

	if (!res) {
		qsfpplus_find_port_params(ctx);

		/*
		 * Read if TX_DISABLE has been implemented
		 * For passive optical modules this is required while it for copper and active
		 * optical modules is optional. Under all circumstances register 195.4 will
		 * indicate, if TX_DISABLE has been implemented in register 86.0-3
		 */
		uint8_t value;
		read_data_lin(ctx, QSFP_OPTION3_LIN_ADDR, sizeof(value), &value);

		ctx->tx_disable = (value & QSFP_OPTION3_TX_DISABLE_BIT) != 0;

		if (ctx->tx_disable)
			ctx->options |= (1 << NIM_OPTION_TX_DISABLE);

		/*
		 * Previously - considering AFBR-89BRDZ - code tried to establish if a module was
		 * RxOnly by testing the state of the lasers after reset. Lasers were for this
		 * module default disabled.
		 * However that code did not work for GigaLight, GQS-MPO400-SR4C so it was
		 * decided that this option should not be detected automatically but from PN
		 */
		ctx->specific_u.qsfp.rx_only = (ctx->options & (1 << NIM_OPTION_RX_ONLY)) != 0;
		qsfpplus_set_speed_mask(ctx);
	}

	return res;
}

int construct_and_preinit_nim(nim_i2c_ctx_p ctx, void *extra)
{
	int res = i2c_nim_common_construct(ctx);

	switch (translate_nimid(ctx)) {
	case NT_NIM_QSFP_PLUS:
		qsfpplus_preinit(ctx, extra ? *(int8_t *)extra : (int8_t)-1);
		break;

	default:
		res = 1;
		NT_LOG(ERR, NTHW, "NIM type %s is not supported.\n", nim_id_to_text(ctx->nim_id));
	}

	return res;
}
