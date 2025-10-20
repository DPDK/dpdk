/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */
#include "ntlog.h"
#include "nthw_drv.h"
#include "nthw_register.h"
#include "nthw_fpga.h"

#include "nthw_pcm_nt400dxx.h"

nthw_pcm_nt400dxx_t *nthw_pcm_nt400dxx_new(void)
{
	nthw_pcm_nt400dxx_t *p = malloc(sizeof(nthw_pcm_nt400dxx_t));

	if (p)
		memset(p, 0, sizeof(nthw_pcm_nt400dxx_t));

	return p;
}

int nthw_pcm_nt400dxx_init(nthw_pcm_nt400dxx_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_PCM_NT400DXX, n_instance);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: PCM_NT400DXX %d: no such instance",
			p->mp_fpga->p_fpga_info->mp_adapter_id_str, p->mn_instance);
		return -1;
	}

	p->mp_mod_pcm = p_mod;

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;

	p->mn_module_major_version = nthw_module_get_major_version(p->mp_mod_pcm);
	p->mn_module_minor_version = nthw_module_get_minor_version(p->mp_mod_pcm);

	p->mp_reg_ctrl = nthw_module_get_register(p->mp_mod_pcm, PCM_NT400DXX_CTRL);
	p->mp_fld_ctrl_ts_pll_recal =
		nthw_register_query_field(p->mp_reg_ctrl, PCM_NT400DXX_CTRL_TS_PLL_RECAL);

	p->mp_reg_stat = nthw_module_get_register(p->mp_mod_pcm, PCM_NT400DXX_STAT);
	p->mp_fld_stat_ts_pll_locked =
		nthw_register_get_field(p->mp_reg_stat, PCM_NT400DXX_STAT_TS_PLL_LOCKED);

	p->mp_reg_latch = nthw_module_get_register(p->mp_mod_pcm, PCM_NT400DXX_LATCH);
	p->mp_fld_latch_ts_pll_locked =
		nthw_register_get_field(p->mp_reg_latch, PCM_NT400DXX_LATCH_TS_PLL_LOCKED);

	return 0;
}

void nthw_pcm_nt400dxx_set_ts_pll_recal(nthw_pcm_nt400dxx_t *p, uint32_t val)
{
	if (p->mp_fld_ctrl_ts_pll_recal) {
		nthw_field_update_register(p->mp_fld_ctrl_ts_pll_recal);
		nthw_field_set_val_flush32(p->mp_fld_ctrl_ts_pll_recal, val);
	}
}

bool nthw_pcm_nt400dxx_get_ts_pll_locked_stat(nthw_pcm_nt400dxx_t *p)
{
	return nthw_field_get_updated(p->mp_fld_stat_ts_pll_locked) != 0;
}

void nthw_pcm_nt400dxx_set_ts_pll_locked_latch(nthw_pcm_nt400dxx_t *p, uint32_t val)
{
	nthw_field_update_register(p->mp_fld_latch_ts_pll_locked);
	nthw_field_set_val_flush32(p->mp_fld_latch_ts_pll_locked, val);
}
