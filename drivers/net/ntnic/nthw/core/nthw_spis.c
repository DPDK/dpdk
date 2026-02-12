/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_spis.h"

nthw_spis_t *nthw_spis_new(void)
{
	nthw_spis_t *p = malloc(sizeof(nthw_spis_t));

	if (p)
		memset(p, 0, sizeof(nthw_spis_t));

	return p;
}

int nthw_spis_init(nthw_spis_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_SPIS, n_instance);

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: SPIS %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_spis = mod;

	/* SPIS is a primary communication channel - turn off debug by default */
	nthw_module_set_debug_mode(p->mp_mod_spis, 0x00);

	p->mp_reg_srr = nthw_module_get_register(p->mp_mod_spis, SPIS_SRR);
	p->mp_fld_srr_rst = nthw_register_get_field(p->mp_reg_srr, SPIS_SRR_RST);

	p->mp_reg_cr = nthw_module_get_register(p->mp_mod_spis, SPIS_CR);
	p->mp_fld_cr_loop = nthw_register_get_field(p->mp_reg_cr, SPIS_CR_LOOP);
	p->mp_fld_cr_en = nthw_register_get_field(p->mp_reg_cr, SPIS_CR_EN);
	p->mp_fld_cr_txrst = nthw_register_get_field(p->mp_reg_cr, SPIS_CR_TXRST);
	p->mp_fld_cr_rxrst = nthw_register_get_field(p->mp_reg_cr, SPIS_CR_RXRST);
	p->mp_fld_cr_debug = nthw_register_get_field(p->mp_reg_cr, SPIS_CR_DEBUG);

	p->mp_reg_sr = nthw_module_get_register(p->mp_mod_spis, SPIS_SR);
	p->mp_fld_sr_done = nthw_register_get_field(p->mp_reg_sr, SPIS_SR_DONE);
	p->mp_fld_sr_txempty = nthw_register_get_field(p->mp_reg_sr, SPIS_SR_TXEMPTY);
	p->mp_fld_sr_rxempty = nthw_register_get_field(p->mp_reg_sr, SPIS_SR_RXEMPTY);
	p->mp_fld_sr_txfull = nthw_register_get_field(p->mp_reg_sr, SPIS_SR_TXFULL);
	p->mp_fld_sr_rxfull = nthw_register_get_field(p->mp_reg_sr, SPIS_SR_RXFULL);
	p->mp_fld_sr_txlvl = nthw_register_get_field(p->mp_reg_sr, SPIS_SR_TXLVL);
	p->mp_fld_sr_rxlvl = nthw_register_get_field(p->mp_reg_sr, SPIS_SR_RXLVL);
	p->mp_fld_sr_frame_err = nthw_register_get_field(p->mp_reg_sr, SPIS_SR_FRAME_ERR);
	p->mp_fld_sr_read_err = nthw_register_get_field(p->mp_reg_sr, SPIS_SR_READ_ERR);
	p->mp_fld_sr_write_err = nthw_register_get_field(p->mp_reg_sr, SPIS_SR_WRITE_ERR);

	p->mp_reg_dtr = nthw_module_get_register(p->mp_mod_spis, SPIS_DTR);
	p->mp_fld_dtr_dtr = nthw_register_get_field(p->mp_reg_dtr, SPIS_DTR_DTR);

	p->mp_reg_drr = nthw_module_get_register(p->mp_mod_spis, SPIS_DRR);
	p->mp_fld_drr_drr = nthw_register_get_field(p->mp_reg_drr, SPIS_DRR_DRR);

	p->mp_reg_ram_ctrl = nthw_module_get_register(p->mp_mod_spis, SPIS_RAM_CTRL);
	p->mp_fld_ram_ctrl_adr = nthw_register_get_field(p->mp_reg_ram_ctrl, SPIS_RAM_CTRL_ADR);
	p->mp_fld_ram_ctrl_cnt = nthw_register_get_field(p->mp_reg_ram_ctrl, SPIS_RAM_CTRL_CNT);

	p->mp_reg_ram_data = nthw_module_get_register(p->mp_mod_spis, SPIS_RAM_DATA);
	p->mp_fld_ram_data_data = nthw_register_get_field(p->mp_reg_ram_data, SPIS_RAM_DATA_DATA);

	return 0;
}

uint32_t nthw_spis_reset(nthw_spis_t *p)
{
	nthw_register_update(p->mp_reg_srr);
	nthw_field_set_val32(p->mp_fld_srr_rst, 0x0A);	/* 0x0A hardcoded value - see doc */
	nthw_register_flush(p->mp_reg_srr, 1);

	return 0;
}

uint32_t nthw_spis_enable(nthw_spis_t *p, bool b_enable)
{
	nthw_field_update_register(p->mp_fld_cr_en);

	if (b_enable)
		nthw_field_set_all(p->mp_fld_cr_en);

	else
		nthw_field_clr_all(p->mp_fld_cr_en);

	nthw_field_flush_register(p->mp_fld_cr_en);

	return 0;
}

uint32_t nthw_spis_get_rx_fifo_empty(nthw_spis_t *p, bool *pb_empty)
{
	RTE_ASSERT(pb_empty);

	*pb_empty = nthw_field_get_updated(p->mp_fld_sr_rxempty) ? true : false;

	return 0;
}

uint32_t nthw_spis_read_rx_fifo(nthw_spis_t *p, uint32_t *p_data)
{
	RTE_ASSERT(p_data);

	*p_data = nthw_field_get_updated(p->mp_fld_drr_drr);

	return 0;
}
