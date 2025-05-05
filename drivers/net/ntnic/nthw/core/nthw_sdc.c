/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_sdc.h"

nthw_sdc_t *nthw_sdc_new(void)
{
	nthw_sdc_t *p = malloc(sizeof(nthw_sdc_t));

	if (p)
		memset(p, 0, sizeof(nthw_sdc_t));

	return p;
}

void nthw_sdc_delete(nthw_sdc_t *p)
{
	free(p);
}

int nthw_sdc_init(nthw_sdc_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_SDC, n_instance);

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: SDC %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_sdc = mod;

	{
		nthw_register_t *p_reg;

		p_reg = nthw_module_get_register(p->mp_mod_sdc, SDC_CTRL);
		p->mp_fld_ctrl_init = nthw_register_get_field(p_reg, SDC_CTRL_INIT);
		p->mp_fld_ctrl_run_test = nthw_register_get_field(p_reg, SDC_CTRL_RUN_TEST);
		p->mp_fld_ctrl_stop_client = nthw_register_get_field(p_reg, SDC_CTRL_STOP_CLIENT);
		p->mp_fld_ctrl_test_enable = nthw_register_get_field(p_reg, SDC_CTRL_TEST_EN);

		p_reg = nthw_module_get_register(p->mp_mod_sdc, SDC_STAT);
		p->mp_fld_stat_calib = nthw_register_get_field(p_reg, SDC_STAT_CALIB);
		p->mp_fld_stat_cell_cnt_stopped =
			nthw_register_get_field(p_reg, SDC_STAT_CELL_CNT_STOPPED);
		p->mp_fld_stat_err_found = nthw_register_get_field(p_reg, SDC_STAT_ERR_FOUND);
		p->mp_fld_stat_init_done = nthw_register_get_field(p_reg, SDC_STAT_INIT_DONE);
		p->mp_fld_stat_mmcm_lock = nthw_register_get_field(p_reg, SDC_STAT_MMCM_LOCK);
		p->mp_fld_stat_pll_lock = nthw_register_get_field(p_reg, SDC_STAT_PLL_LOCK);
		p->mp_fld_stat_resetting = nthw_register_get_field(p_reg, SDC_STAT_RESETTING);

		p_reg = nthw_module_get_register(p->mp_mod_sdc, SDC_CELL_CNT);
		p->mp_fld_cell_cnt = nthw_register_get_field(p_reg, SDC_CELL_CNT_CELL_CNT);

		p_reg = nthw_module_get_register(p->mp_mod_sdc, SDC_CELL_CNT_PERIOD);
		p->mp_fld_cell_cnt_period =
			nthw_register_get_field(p_reg, SDC_CELL_CNT_PERIOD_CELL_CNT_PERIOD);

		p_reg = nthw_module_get_register(p->mp_mod_sdc, SDC_FILL_LVL);
		p->mp_fld_fill_level = nthw_register_get_field(p_reg, SDC_FILL_LVL_FILL_LVL);

		p_reg = nthw_module_get_register(p->mp_mod_sdc, SDC_MAX_FILL_LVL);
		p->mp_fld_max_fill_level =
			nthw_register_get_field(p_reg, SDC_MAX_FILL_LVL_MAX_FILL_LVL);
	}
	return 0;
}

int nthw_sdc_get_states(nthw_sdc_t *p, uint64_t *pn_result_mask)
{
	int n_err_cnt = 0;
	uint64_t n_mask = 0;
	uint32_t val;
	uint32_t val_mask;
	int n_val_width;

	if (!p || !pn_result_mask)
		return -1;

	val = nthw_field_get_updated(p->mp_fld_stat_calib);
	n_val_width = nthw_field_get_bit_width(p->mp_fld_stat_calib);
	val_mask = ((1 << n_val_width) - 1);
	n_mask = (n_mask << n_val_width) | (val & val_mask);

	if (val != val_mask)
		n_err_cnt++;

	val = nthw_field_get_updated(p->mp_fld_stat_init_done);
	n_val_width = nthw_field_get_bit_width(p->mp_fld_stat_init_done);
	val_mask = ((1 << n_val_width) - 1);
	n_mask = (n_mask << n_val_width) | (val & val_mask);

	if (val != val_mask)
		n_err_cnt++;

	val = nthw_field_get_updated(p->mp_fld_stat_mmcm_lock);
	n_val_width = nthw_field_get_bit_width(p->mp_fld_stat_mmcm_lock);
	val_mask = ((1 << n_val_width) - 1);
	n_mask = (n_mask << n_val_width) | (val & val_mask);

	if (val != val_mask)
		n_err_cnt++;

	val = nthw_field_get_updated(p->mp_fld_stat_pll_lock);
	n_val_width = nthw_field_get_bit_width(p->mp_fld_stat_pll_lock);
	val_mask = ((1 << n_val_width) - 1);
	n_mask = (n_mask << n_val_width) | (val & val_mask);

	if (val != val_mask)
		n_err_cnt++;

	val = nthw_field_get_updated(p->mp_fld_stat_resetting);
	n_val_width = nthw_field_get_bit_width(p->mp_fld_stat_resetting);
	val_mask = ((1 << n_val_width) - 1);
	n_mask = (n_mask << n_val_width) | (val & val_mask);

	if (val != 0)
		n_err_cnt++;

	*pn_result_mask = n_mask;

	return n_err_cnt;	/* 0 = all ok */
}

int nthw_sdc_wait_states(nthw_sdc_t *p, const int n_poll_iterations, const int n_poll_interval)
{
	int res;
	int n_err_cnt = 0;

	res = nthw_field_wait_set_all32(p->mp_fld_stat_calib, n_poll_iterations, n_poll_interval);

	if (res)
		n_err_cnt++;

	res = nthw_field_wait_set_all32(p->mp_fld_stat_init_done, n_poll_iterations,
			n_poll_interval);

	if (res)
		n_err_cnt++;

	res = nthw_field_wait_set_all32(p->mp_fld_stat_mmcm_lock, n_poll_iterations,
			n_poll_interval);

	if (res)
		n_err_cnt++;

	res = nthw_field_wait_set_all32(p->mp_fld_stat_pll_lock, n_poll_iterations,
			n_poll_interval);

	if (res)
		n_err_cnt++;

	res = nthw_field_wait_clr_all32(p->mp_fld_stat_resetting, n_poll_iterations,
			n_poll_interval);

	if (res)
		n_err_cnt++;

	return n_err_cnt;	/* 0 = all ok */
}
