/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_SDC_H__
#define __NTHW_SDC_H__

struct nthw_sdc {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_sdc;
	int mn_instance;

	nthw_field_t *mp_fld_ctrl_init;
	nthw_field_t *mp_fld_ctrl_run_test;
	nthw_field_t *mp_fld_ctrl_stop_client;
	nthw_field_t *mp_fld_ctrl_test_enable;

	nthw_field_t *mp_fld_stat_calib;
	nthw_field_t *mp_fld_stat_cell_cnt_stopped;
	nthw_field_t *mp_fld_stat_err_found;
	nthw_field_t *mp_fld_stat_init_done;
	nthw_field_t *mp_fld_stat_mmcm_lock;
	nthw_field_t *mp_fld_stat_pll_lock;
	nthw_field_t *mp_fld_stat_resetting;

	nthw_field_t *mp_fld_cell_cnt;
	nthw_field_t *mp_fld_cell_cnt_period;
	nthw_field_t *mp_fld_fill_level;
	nthw_field_t *mp_fld_max_fill_level;
};

typedef struct nthw_sdc nthw_sdc_t;

nthw_sdc_t *nthw_sdc_new(void);
int nthw_sdc_init(nthw_sdc_t *p, nthw_fpga_t *p_fpga, int n_instance);
void nthw_sdc_delete(nthw_sdc_t *p);

int nthw_sdc_wait_states(nthw_sdc_t *p, const int n_poll_iterations, const int n_poll_interval);
int nthw_sdc_get_states(nthw_sdc_t *p, uint64_t *pn_result_mask);

#endif	/* __NTHW_SDC_H__ */
