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
#include "nim_defines.h"

#define NIM_READ false
#define NIM_WRITE true
#define NIM_PAGE_SEL_REGISTER 127
#define NIM_I2C_0XA0 0xA0	/* Basic I2C address */

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

const char *nim_id_to_text(uint8_t nim_id)
{
	switch (nim_id) {
	case 0x0:
		return "UNKNOWN";

	default:
		return "ILLEGAL!";
	}
}

int construct_and_preinit_nim(nim_i2c_ctx_p ctx)
{
	int res = i2c_nim_common_construct(ctx);

	return res;
}
