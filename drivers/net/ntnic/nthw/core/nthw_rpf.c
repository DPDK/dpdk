/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"
#include "nthw_rpf.h"
#include "rte_spinlock.h"

nthw_rpf_t *nthw_rpf_new(void)
{
	nthw_rpf_t *p = malloc(sizeof(nthw_rpf_t));

	if (p)
		memset(p, 0, sizeof(nthw_rpf_t));

	return p;
}

int nthw_rpf_init(nthw_rpf_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_RPF, n_instance);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: MOD_RPF %d: no such instance",
			p->mp_fpga->p_fpga_info->mp_adapter_id_str, p->mn_instance);
		return -1;
	}

	p->m_mod_rpf = p_mod;

	p->mp_fpga = p_fpga;

	p->m_administrative_block = false;

	/* CONTROL */
	p->mp_reg_control = nthw_module_get_register(p->m_mod_rpf, RPF_CONTROL);
	p->mp_fld_control_pen = nthw_register_get_field(p->mp_reg_control, RPF_CONTROL_PEN);
	p->mp_fld_control_rpp_en = nthw_register_get_field(p->mp_reg_control, RPF_CONTROL_RPP_EN);
	p->mp_fld_control_st_tgl_en =
		nthw_register_get_field(p->mp_reg_control, RPF_CONTROL_ST_TGL_EN);
	p->mp_fld_control_keep_alive_en =
		nthw_register_get_field(p->mp_reg_control, RPF_CONTROL_KEEP_ALIVE_EN);

	/* TS_SORT_PRG */
	p->mp_ts_sort_prg = nthw_module_get_register(p->m_mod_rpf, RPF_TS_SORT_PRG);
	p->mp_fld_ts_sort_prg_maturing_delay =
		nthw_register_get_field(p->mp_ts_sort_prg, RPF_TS_SORT_PRG_MATURING_DELAY);
	p->mp_fld_ts_sort_prg_ts_at_eof =
		nthw_register_get_field(p->mp_ts_sort_prg, RPF_TS_SORT_PRG_TS_AT_EOF);
	p->m_default_maturing_delay =
		nthw_fpga_get_product_param(p_fpga, NT_RPF_MATURING_DEL_DEFAULT, 0);

	/* Initialize mutex */
	rte_spinlock_init(&p->rpf_mutex);
	return 0;
}

void nthw_rpf_block(nthw_rpf_t *p)
{
	nthw_register_update(p->mp_reg_control);
	nthw_field_set_val_flush32(p->mp_fld_control_pen, 0);
}

void nthw_rpf_unblock(nthw_rpf_t *p)
{
	nthw_register_update(p->mp_reg_control);

	nthw_field_set_val32(p->mp_fld_control_pen, ~0U);
	nthw_field_set_val32(p->mp_fld_control_rpp_en, ~0U);
	nthw_field_set_val32(p->mp_fld_control_st_tgl_en, 1);
	nthw_field_set_val_flush32(p->mp_fld_control_keep_alive_en, 1);
}

void nthw_rpf_set_maturing_delay(nthw_rpf_t *p, int32_t delay)
{
	nthw_register_update(p->mp_ts_sort_prg);
	nthw_field_set_val_flush32(p->mp_fld_ts_sort_prg_maturing_delay, (uint32_t)delay);
}

int32_t nthw_rpf_get_maturing_delay(nthw_rpf_t *p)
{
	nthw_register_update(p->mp_ts_sort_prg);
	/* Maturing delay is a two's complement 18 bit value, so we retrieve it as signed */
	return nthw_field_get_signed(p->mp_fld_ts_sort_prg_maturing_delay);
}

void nthw_rpf_set_ts_at_eof(nthw_rpf_t *p, bool enable)
{
	nthw_register_update(p->mp_ts_sort_prg);
	nthw_field_set_val_flush32(p->mp_fld_ts_sort_prg_ts_at_eof, enable);
}

bool nthw_rpf_get_ts_at_eof(nthw_rpf_t *p)
{
	return nthw_field_get_updated(p->mp_fld_ts_sort_prg_ts_at_eof);
}
