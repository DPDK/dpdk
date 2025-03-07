/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_spim.h"

nthw_spim_t *nthw_spim_new(void)
{
	nthw_spim_t *p = malloc(sizeof(nthw_spim_t));

	if (p)
		memset(p, 0, sizeof(nthw_spim_t));

	return p;
}

int nthw_spim_init(nthw_spim_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_SPIM, n_instance);

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: SPIM %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_spim = mod;

	/* SPIM is a primary communication channel - turn off debug by default */
	nthw_module_set_debug_mode(p->mp_mod_spim, 0x00);

	p->mp_reg_srr = nthw_module_get_register(p->mp_mod_spim, SPIM_SRR);
	p->mp_fld_srr_rst = nthw_register_get_field(p->mp_reg_srr, SPIM_SRR_RST);

	p->mp_reg_cr = nthw_module_get_register(p->mp_mod_spim, SPIM_CR);
	p->mp_fld_cr_loop = nthw_register_get_field(p->mp_reg_cr, SPIM_CR_LOOP);
	p->mp_fld_cr_en = nthw_register_get_field(p->mp_reg_cr, SPIM_CR_EN);
	p->mp_fld_cr_txrst = nthw_register_get_field(p->mp_reg_cr, SPIM_CR_TXRST);
	p->mp_fld_cr_rxrst = nthw_register_get_field(p->mp_reg_cr, SPIM_CR_RXRST);

	p->mp_reg_sr = nthw_module_get_register(p->mp_mod_spim, SPIM_SR);
	p->mp_fld_sr_done = nthw_register_get_field(p->mp_reg_sr, SPIM_SR_DONE);
	p->mp_fld_sr_txempty = nthw_register_get_field(p->mp_reg_sr, SPIM_SR_TXEMPTY);
	p->mp_fld_sr_rxempty = nthw_register_get_field(p->mp_reg_sr, SPIM_SR_RXEMPTY);
	p->mp_fld_sr_txfull = nthw_register_get_field(p->mp_reg_sr, SPIM_SR_TXFULL);
	p->mp_fld_sr_rxfull = nthw_register_get_field(p->mp_reg_sr, SPIM_SR_RXFULL);
	p->mp_fld_sr_txlvl = nthw_register_get_field(p->mp_reg_sr, SPIM_SR_TXLVL);
	p->mp_fld_sr_rxlvl = nthw_register_get_field(p->mp_reg_sr, SPIM_SR_RXLVL);

	p->mp_reg_dtr = nthw_module_get_register(p->mp_mod_spim, SPIM_DTR);
	p->mp_fld_dtr_dtr = nthw_register_get_field(p->mp_reg_dtr, SPIM_DTR_DTR);

	p->mp_reg_drr = nthw_module_get_register(p->mp_mod_spim, SPIM_DRR);
	p->mp_fld_drr_drr = nthw_register_get_field(p->mp_reg_drr, SPIM_DRR_DRR);

	p->mp_reg_cfg = nthw_module_get_register(p->mp_mod_spim, SPIM_CFG);
	p->mp_fld_cfg_pre = nthw_register_get_field(p->mp_reg_cfg, SPIM_CFG_PRE);

	p->mp_reg_cfg_clk = nthw_module_query_register(p->mp_mod_spim, SPIM_CFG_CLK);
	p->mp_fld_cfg_clk_mode = nthw_register_query_field(p->mp_reg_cfg, SPIM_CFG_CLK_MODE);

	return 0;
}

uint32_t nthw_spim_reset(nthw_spim_t *p)
{
	nthw_register_update(p->mp_reg_srr);
	nthw_field_set_val32(p->mp_fld_srr_rst, 0x0A);	/* 0x0A hardcoded value - see doc */
	nthw_register_flush(p->mp_reg_srr, 1);

	return 0;
}

uint32_t nthw_spim_enable(nthw_spim_t *p, bool b_enable)
{
	nthw_field_update_register(p->mp_fld_cr_en);

	if (b_enable)
		nthw_field_set_all(p->mp_fld_cr_en);

	else
		nthw_field_clr_all(p->mp_fld_cr_en);

	nthw_field_flush_register(p->mp_fld_cr_en);

	return 0;
}

uint32_t nthw_spim_write_tx_fifo(nthw_spim_t *p, uint32_t n_data)
{
	nthw_field_set_val_flush32(p->mp_fld_dtr_dtr, n_data);
	return 0;
}

uint32_t nthw_spim_get_tx_fifo_empty(nthw_spim_t *p, bool *pb_empty)
{
	RTE_ASSERT(pb_empty);

	*pb_empty = nthw_field_get_updated(p->mp_fld_sr_txempty) ? true : false;

	return 0;
}
