/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 *
 * This file implements Si5340 clock synthesizer support.
 * The implementation is generic and must be tailored to a specific use by the
 * correct initialization data.
 */

#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_si5340.h"

#define SI5340_PAGE_REG_ADDR (0x01)

nthw_si5340_t *nthw_si5340_new(void)
{
	nthw_si5340_t *p = malloc(sizeof(nthw_si5340_t));

	if (p)
		memset(p, 0, sizeof(nthw_si5340_t));

	return p;
}

int nthw_si5340_init(nthw_si5340_t *p, nthw_iic_t *p_nthw_iic, uint8_t n_iic_addr)
{
	uint8_t data;

	p->mp_nthw_iic = p_nthw_iic;
	p->mn_iic_addr = n_iic_addr;
	p->mn_clk_cfg = -1;

	p->m_si5340_page = 0;
	data = p->m_si5340_page;
	nthw_iic_write_data(p->mp_nthw_iic, p->mn_iic_addr, SI5340_PAGE_REG_ADDR, 1, &data);

	return 0;
}

void nthw_si5340_delete(nthw_si5340_t *p)
{
	free(p);
}

/*
 * Read access (via I2C) to the clock synthesizer IC. The IC is located at I2C
 * 7bit address 0x74
 */
static uint8_t nthw_si5340_read(nthw_si5340_t *p, uint16_t reg_addr)
{
	const uint8_t offset_adr = (uint8_t)(reg_addr & 0xff);
	uint8_t page = (uint8_t)((reg_addr >> 8) & 0xff);
	uint8_t data;

	/* check if we are on the right page */
	if (page != p->m_si5340_page) {
		nthw_iic_write_data(p->mp_nthw_iic, p->mn_iic_addr, SI5340_PAGE_REG_ADDR, 1,
			&page);
		p->m_si5340_page = page;
	}

	nthw_iic_read_data(p->mp_nthw_iic, p->mn_iic_addr, offset_adr, 1, &data);
	return data;
}

/*
 * Write access (via I2C) to the clock synthesizer IC. The IC is located at I2C
 * 7 bit address 0x74
 */
static int nthw_si5340_write(nthw_si5340_t *p, uint16_t reg_addr, uint8_t data)
{
	const uint8_t offset_adr = (uint8_t)(reg_addr & 0xff);
	uint8_t page = (uint8_t)((reg_addr >> 8) & 0xff);

	/* check if we are on the right page */
	if (page != p->m_si5340_page) {
		nthw_iic_write_data(p->mp_nthw_iic, p->mn_iic_addr, SI5340_PAGE_REG_ADDR, 1,
			&page);
		p->m_si5340_page = page;
	}

	nthw_iic_write_data(p->mp_nthw_iic, p->mn_iic_addr, offset_adr, 1, &data);

	return 0;
}

static int nthw_si5340_cfg(nthw_si5340_t *p, const void *p_data, int data_cnt,
	clk_profile_data_fmt_t data_format)
{
	const char *const p_adapter_id_str =
		p->mp_nthw_iic->mp_fpga->p_fpga_info->mp_adapter_id_str;
	int i;
	uint16_t addr;
	uint8_t value;
	uint8_t ctrl_value;

	NT_LOG(DBG, NTHW, "%s: data_cnt = %d, data_format = %d", p_adapter_id_str,
		data_cnt, data_format);

	for (i = 0; i < data_cnt; i++) {
		if (data_format == clk_profile_data_fmt_1) {
			addr = ((const clk_profile_data_fmt1_t *)p_data)->reg_addr;
			value = ((const clk_profile_data_fmt1_t *)p_data)->reg_val;
			p_data = ((const clk_profile_data_fmt1_t *)p_data) + 1;

		} else if (data_format == clk_profile_data_fmt_2) {
			addr = (uint16_t)(((const clk_profile_data_fmt2_t *)p_data)->reg_addr);
			value = ((const clk_profile_data_fmt2_t *)p_data)->reg_val;
			p_data = ((const clk_profile_data_fmt2_t *)p_data) + 1;

		} else {
			NT_LOG(ERR, NTHW, "%s: Unhandled Si5340 data format (%d)",
				p_adapter_id_str, data_format);
			return -1;
		}

		if (addr == 0x0006) {
			/* Wait 300ms before continuing. See NT200E3-2-PTP_U23_Si5340_adr0_v2.h */
			nt_os_wait_usec(300000);
		}

		nthw_si5340_write(p, addr, value);

		if (addr == 0x001C) {
			/* skip readback for "soft reset" register */
			continue;
		}

		ctrl_value = nthw_si5340_read(p, addr);

		if (ctrl_value != value) {
			NT_LOG(ERR, NTHW,
				"%s: Si5340 configuration readback check failed. (Addr = 0x%04X, Write = 0x%02X, Read = 0x%02X)",
				p_adapter_id_str, addr, value, ctrl_value);
			return -1;
		}
	}

	return 0;
}

static int nthw_si5340_config(nthw_si5340_t *p, const void *p_data, int data_cnt,
	clk_profile_data_fmt_t data_format)
{
	const char *const p_adapter_id_str =
		p->mp_nthw_iic->mp_fpga->p_fpga_info->mp_adapter_id_str;
	int i;
	bool success = false;
	uint8_t status, sticky;
	uint8_t design_id[9];

	(void)nthw_si5340_cfg(p, p_data, data_cnt, data_format);

	/* Check if DPLL is locked and SYS is calibrated */
	for (i = 0; i < 5; i++) {
		status = nthw_si5340_read(p, 0x0c);
		sticky = nthw_si5340_read(p, 0x11);
		nthw_si5340_write(p, 0x11, 0x00);

		if (((status & 0x09) == 0x00) && ((sticky & 0x09) == 0x00)) {
			success = true;
			break;
		}

		nt_os_wait_usec(1000000);	/* 1 sec */
	}

	if (!success) {
		NT_LOG(ERR, NTHW,
			"%s: Si5340 configuration failed. (Status = 0x%02X, Sticky = 0x%02X)",
			p_adapter_id_str, status, sticky);
		return -1;
	}

	for (i = 0; i < (int)sizeof(design_id) - 1; i++)
		design_id[i] = nthw_si5340_read(p, (uint16_t)(0x26B + i));

	design_id[sizeof(design_id) - 1] = 0;

	(void)design_id;/* Only used in debug mode */
	NT_LOG(DBG, NTHW, "%s: Si5340.Design_id = %s", p_adapter_id_str, design_id);

	return 0;
}

int nthw_si5340_config_fmt2(nthw_si5340_t *p, const clk_profile_data_fmt2_t *p_data,
	const int data_cnt)
{
	return nthw_si5340_config(p, p_data, data_cnt, clk_profile_data_fmt_2);
}
