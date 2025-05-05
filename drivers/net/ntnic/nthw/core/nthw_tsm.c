/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_tsm.h"

nthw_tsm_t *nthw_tsm_new(void)
{
	nthw_tsm_t *p = malloc(sizeof(nthw_tsm_t));

	if (p)
		memset(p, 0, sizeof(nthw_tsm_t));

	return p;
}

int nthw_tsm_init(nthw_tsm_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_TSM, n_instance);

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: TSM %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_tsm = mod;

	{
		nthw_register_t *p_reg;

		p_reg = nthw_module_get_register(p->mp_mod_tsm, TSM_CONFIG);
		p->mp_fld_config_ts_format = nthw_register_get_field(p_reg, TSM_CONFIG_TS_FORMAT);

		p_reg = nthw_module_get_register(p->mp_mod_tsm, TSM_TIMER_CTRL);
		p->mp_fld_timer_ctrl_timer_en_t0 =
			nthw_register_get_field(p_reg, TSM_TIMER_CTRL_TIMER_EN_T0);
		p->mp_fld_timer_ctrl_timer_en_t1 =
			nthw_register_get_field(p_reg, TSM_TIMER_CTRL_TIMER_EN_T1);

		p_reg = nthw_module_get_register(p->mp_mod_tsm, TSM_TIMER_T0);
		p->mp_fld_timer_timer_t0_max_count =
			nthw_register_get_field(p_reg, TSM_TIMER_T0_MAX_COUNT);

		p_reg = nthw_module_get_register(p->mp_mod_tsm, TSM_TIMER_T1);
		p->mp_fld_timer_timer_t1_max_count =
			nthw_register_get_field(p_reg, TSM_TIMER_T1_MAX_COUNT);

		p->mp_reg_time_lo = nthw_module_get_register(p->mp_mod_tsm, TSM_TIME_LO);
		p_reg = p->mp_reg_time_lo;
		p->mp_fld_time_lo = nthw_register_get_field(p_reg, TSM_TIME_LO_NS);

		p->mp_reg_time_hi = nthw_module_get_register(p->mp_mod_tsm, TSM_TIME_HI);
		p_reg = p->mp_reg_time_hi;
		p->mp_fld_time_hi = nthw_register_get_field(p_reg, TSM_TIME_HI_SEC);

		p->mp_reg_ts_lo = nthw_module_get_register(p->mp_mod_tsm, TSM_TS_LO);
		p_reg = p->mp_reg_ts_lo;
		p->mp_fld_ts_lo = nthw_register_get_field(p_reg, TSM_TS_LO_TIME);

		p->mp_reg_ts_hi = nthw_module_get_register(p->mp_mod_tsm, TSM_TS_HI);
		p_reg = p->mp_reg_ts_hi;
		p->mp_fld_ts_hi = nthw_register_get_field(p_reg, TSM_TS_HI_TIME);
	}
	return 0;
}

int nthw_tsm_get_ts(nthw_tsm_t *p, uint64_t *p_ts)
{
	uint32_t n_ts_lo, n_ts_hi;
	uint64_t val;

	if (!p_ts)
		return -1;

	n_ts_lo = nthw_field_get_updated(p->mp_fld_ts_lo);
	n_ts_hi = nthw_field_get_updated(p->mp_fld_ts_hi);

	val = ((((uint64_t)n_ts_hi) << 32UL) | n_ts_lo);

	*p_ts = val;

	return 0;
}

int nthw_tsm_get_time(nthw_tsm_t *p, uint64_t *p_time)
{
	uint32_t n_time_lo, n_time_hi;
	uint64_t val;

	if (!p_time)
		return -1;

	n_time_lo = nthw_field_get_updated(p->mp_fld_time_lo);
	n_time_hi = nthw_field_get_updated(p->mp_fld_time_hi);

	val = ((((uint64_t)n_time_hi) << 32UL) | n_time_lo);

	*p_time = val;

	return 0;
}

int nthw_tsm_set_timer_t0_enable(nthw_tsm_t *p, bool b_enable)
{
	nthw_field_update_register(p->mp_fld_timer_ctrl_timer_en_t0);

	if (b_enable)
		nthw_field_set_flush(p->mp_fld_timer_ctrl_timer_en_t0);

	else
		nthw_field_clr_flush(p->mp_fld_timer_ctrl_timer_en_t0);

	return 0;
}

int nthw_tsm_set_timer_t0_max_count(nthw_tsm_t *p, uint32_t n_timer_val)
{
	/* Timer T0 - stat toggle timer */
	nthw_field_update_register(p->mp_fld_timer_timer_t0_max_count);
	nthw_field_set_val_flush32(p->mp_fld_timer_timer_t0_max_count,
		n_timer_val);	/* ns (50*1000*1000) */
	return 0;
}

int nthw_tsm_set_timer_t1_enable(nthw_tsm_t *p, bool b_enable)
{
	nthw_field_update_register(p->mp_fld_timer_ctrl_timer_en_t1);

	if (b_enable)
		nthw_field_set_flush(p->mp_fld_timer_ctrl_timer_en_t1);

	else
		nthw_field_clr_flush(p->mp_fld_timer_ctrl_timer_en_t1);

	return 0;
}

int nthw_tsm_set_timer_t1_max_count(nthw_tsm_t *p, uint32_t n_timer_val)
{
	/* Timer T1 - keep alive timer */
	nthw_field_update_register(p->mp_fld_timer_timer_t1_max_count);
	nthw_field_set_val_flush32(p->mp_fld_timer_timer_t1_max_count,
		n_timer_val);	/* ns (100*1000*1000) */
	return 0;
}

int nthw_tsm_set_config_ts_format(nthw_tsm_t *p, uint32_t n_val)
{
	nthw_field_update_register(p->mp_fld_config_ts_format);
	/* 0x1: Native - 10ns units, start date: 1970-01-01. */
	nthw_field_set_val_flush32(p->mp_fld_config_ts_format, n_val);
	return 0;
}
