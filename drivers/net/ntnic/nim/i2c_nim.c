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

static int nim_agx_read_id(struct nim_i2c_ctx *ctx);
static void nim_agx_read(struct nim_i2c_ctx *ctx, uint8_t dev_addr, uint8_t reg_addr,
	uint8_t data_len, void *p_data);
static void nim_agx_write(struct nim_i2c_ctx *ctx, uint8_t dev_addr, uint8_t reg_addr,
	uint8_t data_len, void *p_data);

#define NIM_READ false
#define NIM_WRITE true
#define NIM_PAGE_SEL_REGISTER 127
#define NIM_I2C_0XA0 0xA0	/* Basic I2C address */


static bool page_addressing(nt_nim_identifier_t id)
{
	switch (id) {
	case NT_NIM_QSFP:
	case NT_NIM_QSFP_PLUS:
	case NT_NIM_QSFP28:
		return true;

	default:
		NT_LOG(DBG, NTNIC, "Unknown NIM identifier %d", id);
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
		}

		nim_agx_write(ctx, i2c_addr, a_reg_addr, seq_cnt, p_data);
		return 0;
	}

	if (ctx->type == I2C_HWIIC)
		return nthw_iic_read_data(&ctx->hwiic, i2c_devaddr, a_reg_addr, seq_cnt, p_data);

	nim_agx_read(ctx, i2c_addr, a_reg_addr, seq_cnt, p_data);
	return 0;
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
					"Cannot set up page for linear address %u", lin_addr);
				return -1;
			}
		}

		if (nim_read_write_i2c_data(ctx, do_write, lin_addr, i2c_addr, a_reg_addr,
				(uint8_t)seq_cnt, p_data) != 0) {
			NT_LOG(ERR, NTNIC, " Call to nim_read_write_i2c_data failed");
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

static int write_data_lin(nim_i2c_ctx_p ctx, uint16_t lin_addr, uint16_t length, void *data)
{
	/* Wrapper for using Mutex for QSFP TODO */
	return nim_read_write_data_lin(ctx, page_addressing(ctx->nim_id), lin_addr, length, data,
		NIM_WRITE);
}

/* Read and return a single byte */
static uint8_t read_byte(nim_i2c_ctx_p ctx, uint16_t addr)
{
	uint8_t data;
	read_data_lin(ctx, addr, sizeof(data), &data);
	return data;
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
		res = nim_agx_read_id(ctx);

	if (res) {
		NT_LOG(ERR, NTNIC, "Can't read NIM id.");
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

	RTE_ASSERT(ctx && state);
	RTE_ASSERT(ctx->nim_id != NT_NIM_UNKNOWN && "Nim is not initialized");

	memset(state, 0, sizeof(*state));

	switch (ctx->nim_id) {
	case 12U:
		state->br = 10U;/* QSFP: 4 x 1G = 4G */
		break;

	case 13U:
		state->br = 103U;	/* QSFP+: 4 x 10G = 40G */
		break;

	case 17U:
		state->br = 255U;	/* QSFP28: 4 x 25G = 100G */
		break;

	default:
		NT_LOG(INF, NTNIC, "nim_id = %u is not an QSFP/QSFP+/QSFP28 module", ctx->nim_id);
		res = -1;
	}

	return res;
}

int nthw_nim_state_build(nim_i2c_ctx_t *ctx, sfp_nim_state_t *state)
{
	return qsfp_nim_state_build(ctx, state);
}

const char *nthw_nim_id_to_text(uint8_t nim_id)
{
	switch (nim_id) {
	case 0x0:
		return "UNKNOWN";

	case 0x0C:
		return "QSFP";

	case 0x0D:
		return "QSFP+";

	case 0x11:
		return "QSFP28";

	default:
		return "ILLEGAL!";
	}
}

/*
 * Disable laser for specific lane or all lanes
 */
int nthw_nim_qsfp_plus_nim_set_tx_laser_disable(nim_i2c_ctx_p ctx, bool disable, int lane_idx)
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
	NT_LOG(DBG, NTNIC, "Instance %d: NIM id: %s (%d)", ctx->instance,
		nthw_nim_id_to_text(ctx->nim_id), ctx->nim_id);

	/* Read DMI options */
	if (nim_read_write_data_lin(ctx, pg_addr, QSFP_DMI_OPTION_LIN_ADDR, sizeof(options),
			&options, NIM_READ) != 0) {
		return -1;
	}

	ctx->avg_pwr = options & QSFP_DMI_AVG_PWR_BIT;
	NT_LOG(DBG, NTNIC, "Instance %d: NIM options: (DMI: Yes, AvgPwr: %s)", ctx->instance,
		yes_no[ctx->avg_pwr]);

	qsfp_read_vendor_info(ctx);
	NT_LOG(DBG, NTNIC,
		"Instance %d: NIM info: (Vendor: %s, PN: %s, SN: %s, Date: %s, Rev: %s)",
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

static void qsfp28_find_port_params(nim_i2c_ctx_p ctx)
{
	uint8_t fiber_chan_speed;

	/* Table 6-17 SFF-8636 */
	read_data_lin(ctx, QSFP_SPEC_COMPLIANCE_CODES_ADDR, 1, &fiber_chan_speed);

	if (fiber_chan_speed & (1 << 7)) {
		/* SFF-8024, Rev 4.7, Table 4-4 */
		uint8_t extended_specification_compliance_code = 0;
		read_data_lin(ctx, QSFP_EXT_SPEC_COMPLIANCE_CODES_ADDR, 1,
			&extended_specification_compliance_code);

		switch (extended_specification_compliance_code) {
		case 0x02:
			ctx->port_type = NT_PORT_TYPE_QSFP28_SR4;
			break;

		case 0x03:
			ctx->port_type = NT_PORT_TYPE_QSFP28_LR4;
			break;

		case 0x0B:
			ctx->port_type = NT_PORT_TYPE_QSFP28_CR_CA_L;
			break;

		case 0x0C:
			ctx->port_type = NT_PORT_TYPE_QSFP28_CR_CA_S;
			break;

		case 0x0D:
			ctx->port_type = NT_PORT_TYPE_QSFP28_CR_CA_N;
			break;

		case 0x25:
			ctx->port_type = NT_PORT_TYPE_QSFP28_DR;
			break;

		case 0x26:
			ctx->port_type = NT_PORT_TYPE_QSFP28_FR;
			break;

		case 0x27:
			ctx->port_type = NT_PORT_TYPE_QSFP28_LR;
			break;

		default:
			ctx->port_type = NT_PORT_TYPE_QSFP28;
		}

	} else {
		ctx->port_type = NT_PORT_TYPE_QSFP28;
	}
}

/*
 * If true the user must actively select the desired rate. If false the module
 * however can still support several rates without the user is required to select
 * one of them. Supported rates must then be deduced from the product number.
 * SFF-8636, Rev 2.10a:
 * p40: 6.2.7 Rate Select
 * p85: A.2 Rate Select
 */
static bool qsfp28_is_rate_selection_enabled(nim_i2c_ctx_p ctx)
{
	const uint8_t ext_rate_select_compl_reg_addr = 141;
	const uint8_t options_reg_addr = 195;
	const uint8_t enh_options_reg_addr = 221;

	uint8_t rate_select_ena = (read_byte(ctx, options_reg_addr) >> 5) & 0x01;	/* bit: 5 */

	if (rate_select_ena == 0)
		return false;

	uint8_t rate_select_type =
		(read_byte(ctx, enh_options_reg_addr) >> 2) & 0x03;	/* bit 3..2 */

	if (rate_select_type != 2) {
		NT_LOG(DBG, NTNIC, "NIM has unhandled rate select type (%d)", rate_select_type);
		return false;
	}

	uint8_t ext_rate_select_ver =
		read_byte(ctx, ext_rate_select_compl_reg_addr) & 0x03;	/* bit 1..0 */

	if (ext_rate_select_ver != 0x02) {
		NT_LOG(DBG, NTNIC, "NIM has unhandled extended rate select version (%d)",
			ext_rate_select_ver);
		return false;
	}

	return true;	/* When true selectRate() can be used */
}

static void qsfp28_set_speed_mask(nim_i2c_ctx_p ctx)
{
	if (ctx->port_type == NT_PORT_TYPE_QSFP28_FR || ctx->port_type == NT_PORT_TYPE_QSFP28_DR ||
		ctx->port_type == NT_PORT_TYPE_QSFP28_LR) {
		if (ctx->lane_idx < 0)
			ctx->speed_mask = NT_LINK_SPEED_100G;

		else
			/* PAM-4 modules can only run on all lanes together */
			ctx->speed_mask = 0;

	} else {
		if (ctx->lane_idx < 0)
			ctx->speed_mask = NT_LINK_SPEED_100G;

		else
			ctx->speed_mask = NT_LINK_SPEED_25G;

		if (qsfp28_is_rate_selection_enabled(ctx)) {
			/*
			 * It is assumed that if the module supports dual rates then the other rate
			 * is 10G per lane or 40G for all lanes.
			 */
			if (ctx->lane_idx < 0)
				ctx->speed_mask |= NT_LINK_SPEED_40G;

			else
				ctx->speed_mask = NT_LINK_SPEED_10G;
		}
	}
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
	RTE_ASSERT(lane_idx < 4);
	ctx->specific_u.qsfp.qsfp28 = false;
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

static void qsfp28_wait_for_ready_after_reset(nim_i2c_ctx_p ctx)
{
	uint8_t data;
	bool init_complete_flag_present = false;

	/*
	 * Revision compliance
	 * 7: SFF-8636 Rev 2.5, 2.6 and 2.7
	 * 8: SFF-8636 Rev 2.8, 2.9 and 2.10
	 */
	read_data_lin(ctx, 1, sizeof(ctx->specific_u.qsfp.specific_u.qsfp28.rev_compliance),
		&ctx->specific_u.qsfp.specific_u.qsfp28.rev_compliance);
	NT_LOG(DBG, NTHW, "NIM RevCompliance = %d",
		ctx->specific_u.qsfp.specific_u.qsfp28.rev_compliance);

	/* Wait if lane_idx == -1 (all lanes are used) or lane_idx == 0 (the first lane) */
	if (ctx->lane_idx > 0)
		return;

	if (ctx->specific_u.qsfp.specific_u.qsfp28.rev_compliance >= 7) {
		/* Check if init complete flag is implemented */
		read_data_lin(ctx, 221, sizeof(data), &data);
		init_complete_flag_present = (data & (1 << 4)) != 0;
	}

	NT_LOG(DBG, NTHW, "NIM InitCompleteFlagPresent = %d", init_complete_flag_present);

	/*
	 * If the init complete flag is not present then wait 500ms that together with 500ms
	 * after reset (in the adapter code) should be enough to read data from upper pages
	 * that otherwise would not be ready. Especially BiDi modules AFBR-89BDDZ have been
	 * prone to this when trying to read sensor options using getQsfpOptionsFromData()
	 * Probably because access to the paged address space is required.
	 */
	if (!init_complete_flag_present) {
		nthw_os_wait_usec(500000);
		return;
	}

	/* Otherwise wait for the init complete flag to be set */
	int count = 0;

	while (true) {
		if (count > 10) {	/* 1 s timeout */
			NT_LOG(WRN, NTHW, "Timeout waiting for module ready");
			break;
		}

		read_data_lin(ctx, 6, sizeof(data), &data);

		if (data & 0x01) {
			NT_LOG(DBG, NTHW, "Module ready after %dms", count * 100);
			break;
		}

		nthw_os_wait_usec(100000);/* 100 ms */
		count++;
	}
}

static void qsfp28_get_fec_options(nim_i2c_ctx_p ctx)
{
	const char *const nim_list[] = {
		"AFBR-89BDDZ",	/* Avago BiDi */
		"AFBR-89BRDZ",	/* Avago BiDi, RxOnly */
		"FTLC4352RKPL",	/* Finisar QSFP28-LR */
		"FTLC4352RHPL",	/* Finisar QSFP28-DR */
		"FTLC4352RJPL",	/* Finisar QSFP28-FR */
		"SFBR-89BDDZ-CS4",	/* Foxconn, QSFP28 100G/40G BiDi */
	};

	for (size_t i = 0; i < ARRAY_SIZE(nim_list); i++) {
		if (ctx->prod_no == nim_list[i]) {
			ctx->options |= (1 << NIM_OPTION_MEDIA_SIDE_FEC);
			ctx->specific_u.qsfp.specific_u.qsfp28.media_side_fec_ena = true;
			NT_LOG(DBG, NTHW, "Found FEC info via PN list");
			return;
		}
	}

	/*
	 * For modules not in the list find FEC info via registers
	 * Read if the module has controllable FEC
	 * SFF-8636, Rev 2.10a TABLE 6-28 Equalizer, Emphasis, Amplitude and Timing)
	 * (Page 03h, Bytes 224-229)
	 */
	uint8_t data;
	uint16_t addr = 227 + 3 * 128;
	read_data_lin(ctx, addr, sizeof(data), &data);

	/* Check if the module has FEC support that can be controlled */
	ctx->specific_u.qsfp.specific_u.qsfp28.media_side_fec_ctrl = (data & (1 << 6)) != 0;
	ctx->specific_u.qsfp.specific_u.qsfp28.host_side_fec_ctrl = (data & (1 << 7)) != 0;

	if (ctx->specific_u.qsfp.specific_u.qsfp28.media_side_fec_ctrl)
		ctx->options |= (1 << NIM_OPTION_MEDIA_SIDE_FEC);

	if (ctx->specific_u.qsfp.specific_u.qsfp28.host_side_fec_ctrl)
		ctx->options |= (1 << NIM_OPTION_HOST_SIDE_FEC);
}

static int qsfp28_preinit(nim_i2c_ctx_p ctx, int8_t lane_idx)
{
	int res = qsfpplus_preinit(ctx, lane_idx);

	if (!res) {
		qsfp28_wait_for_ready_after_reset(ctx);
		memset(&ctx->specific_u.qsfp.specific_u.qsfp28, 0,
			sizeof(ctx->specific_u.qsfp.specific_u.qsfp28));
		ctx->specific_u.qsfp.qsfp28 = true;
		qsfp28_find_port_params(ctx);
		qsfp28_get_fec_options(ctx);
		qsfp28_set_speed_mask(ctx);
	}

	return res;
}

int nthw_construct_and_preinit_nim(nim_i2c_ctx_p ctx, void *extra)
{
	int res = i2c_nim_common_construct(ctx);

	switch (translate_nimid(ctx)) {
	case NT_NIM_QSFP_PLUS:
		qsfpplus_preinit(ctx, extra ? *(int8_t *)extra : (int8_t)-1);
		break;

	case NT_NIM_QSFP28:
		qsfp28_preinit(ctx, extra ? *(int8_t *)extra : (int8_t)-1);
		break;

	default:
		res = 1;
		NT_LOG(ERR, NTHW, "NIM type %s is not supported", nthw_nim_id_to_text(ctx->nim_id));
	}

	return res;
}

/*
 * Enables high power according to SFF-8636 Rev 2.7, Table 6-9, Page 35:
 * When set (= 1b) enables Power Classes 5 to 7 in Byte 129 to exceed 3.5W.
 * When cleared (= 0b), modules with power classes 5 to 7 must dissipate less
 * than 3.5W (but are not required to be fully functional). Default 0.
 */
void nthw_qsfp28_set_high_power(nim_i2c_ctx_p ctx)
{
	const uint16_t addr = 93;
	uint8_t data;

	/* Enable high power class; Set Page 00h, Byte 93 bit 2 to 1 */
	read_data_lin(ctx, addr, sizeof(data), &data);
	data |= (1 << 2);
	write_data_lin(ctx, addr, sizeof(data), &data);
}

/*
 * Enable FEC on media and/or host side. If the operation could be carried out
 * return true. For some modules media side FEC is enabled but cannot be changed
 *  while others allow changing the FEC state.
 * For LR4 modules write operations (which are also not necessary) to the control
 *  register must be avoided as this introduces I2C errors on NT200A01.
 */

bool nthw_qsfp28_set_fec_enable(nim_i2c_ctx_p ctx, bool media_side_fec, bool host_side_fec)
{
	/*
	 * If the current FEC state does not match the wanted and the FEC cannot be
	 * controlled then the operation cannot be carried out
	 */
	if (ctx->specific_u.qsfp.specific_u.qsfp28.media_side_fec_ena != media_side_fec &&
		!ctx->specific_u.qsfp.specific_u.qsfp28.media_side_fec_ctrl)
		return false;

	if (ctx->specific_u.qsfp.specific_u.qsfp28.host_side_fec_ena != host_side_fec &&
		!ctx->specific_u.qsfp.specific_u.qsfp28.host_side_fec_ctrl)
		return false;

	/*
	 * If the current media and host side FEC state matches the wanted states then
	 * no need to do more
	 */
	if (ctx->specific_u.qsfp.specific_u.qsfp28.media_side_fec_ena == media_side_fec &&
		ctx->specific_u.qsfp.specific_u.qsfp28.host_side_fec_ena == host_side_fec)
		return true;

	/*
	 * SFF-8636, Rev 2.10a TABLE 6-29 Optional Channel Control)
	 * (Page 03h, Bytes 230-241)
	 */
	const uint16_t addr = 230 + 3 * 128;
	uint8_t data = 0;
	read_data_lin(ctx, addr, sizeof(data), &data);

	if (media_side_fec)
		data &= (uint8_t)(~(1 << 6));

	else
		data |= (uint8_t)(1 << 6);

	if (host_side_fec)
		data |= (uint8_t)(1 << 7);

	else
		data &= (uint8_t)(~(1 << 7));

	write_data_lin(ctx, addr, sizeof(data), &data);
	ctx->specific_u.qsfp.specific_u.qsfp28.media_side_fec_ena = media_side_fec;
	ctx->specific_u.qsfp.specific_u.qsfp28.host_side_fec_ena = host_side_fec;
	return true;
}

void nthw_nim_agx_setup(struct nim_i2c_ctx *ctx, nthw_pcal6416a_t *p_io_nim, nthw_i2cm_t *p_nt_i2cm,
	nthw_pca9849_t *p_ca9849)
{
	ctx->hwagx.p_nt_i2cm = p_nt_i2cm;
	ctx->hwagx.p_ca9849 = p_ca9849;
	ctx->hwagx.p_io_nim = p_io_nim;
}

static void nim_agx_read(struct nim_i2c_ctx *ctx,
	uint8_t dev_addr,
	uint8_t reg_addr,
	uint8_t data_len,
	void *p_data)
{
	nthw_i2cm_t *p_nt_i2cm = ctx->hwagx.p_nt_i2cm;
	nthw_pca9849_t *p_ca9849 = ctx->hwagx.p_ca9849;
	uint8_t *data = (uint8_t *)p_data;

	rte_spinlock_lock(&p_nt_i2cm->i2cmmutex);
	nthw_pca9849_set_channel(p_ca9849, ctx->hwagx.mux_channel);

	for (uint8_t i = 0; i < data_len; i++) {
		nthw_i2cm_read(p_nt_i2cm, (uint8_t)(dev_addr >> 1), (uint8_t)(reg_addr + i), data);
		data++;
	}

	rte_spinlock_unlock(&p_nt_i2cm->i2cmmutex);
}

static void nim_agx_write(struct nim_i2c_ctx *ctx,
	uint8_t dev_addr,
	uint8_t reg_addr,
	uint8_t data_len,
	void *p_data)
{
	nthw_i2cm_t *p_nt_i2cm = ctx->hwagx.p_nt_i2cm;
	nthw_pca9849_t *p_ca9849 = ctx->hwagx.p_ca9849;
	uint8_t *data = (uint8_t *)p_data;

	rte_spinlock_lock(&p_nt_i2cm->i2cmmutex);
	nthw_pca9849_set_channel(p_ca9849, ctx->hwagx.mux_channel);

	for (uint8_t i = 0; i < data_len; i++) {
		nthw_i2cm_write(p_nt_i2cm, (uint8_t)(dev_addr >> 1), (uint8_t)(reg_addr + i),
			*data++);
	}

	rte_spinlock_unlock(&p_nt_i2cm->i2cmmutex);
}

static int nim_agx_read_id(struct nim_i2c_ctx *ctx)
{
	nim_agx_read(ctx, 0xA0, 0, sizeof(ctx->nim_id), &ctx->nim_id);
	return 0;
}
