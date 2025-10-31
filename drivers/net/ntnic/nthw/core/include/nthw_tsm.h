/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_TSM_H__
#define __NTHW_TSM_H__

#include "stdint.h"

#include "nthw_fpga_model.h"

struct nthw_tsm {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_tsm;
	int mn_instance;

	nthw_field_t *mp_fld_config_ts_format;

	nthw_field_t *mp_fld_timer_ctrl_timer_en_t0;
	nthw_field_t *mp_fld_timer_ctrl_timer_en_t1;

	nthw_field_t *mp_fld_timer_timer_t0_max_count;

	nthw_field_t *mp_fld_timer_timer_t1_max_count;

	nthw_register_t *mp_reg_ts_lo;
	nthw_field_t *mp_fld_ts_lo;

	nthw_register_t *mp_reg_ts_hi;
	nthw_field_t *mp_fld_ts_hi;

	nthw_register_t *mp_reg_time_lo;
	nthw_field_t *mp_fld_time_lo;

	nthw_register_t *mp_reg_time_hi;
	nthw_field_t *mp_fld_time_hi;
};

typedef struct nthw_tsm nthw_tsm_t;
typedef struct nthw_tsm nthw_tsm;

nthw_tsm_t *nthw_tsm_new(void);
int nthw_tsm_init(nthw_tsm_t *p, nthw_fpga_t *p_fpga, int n_instance);

int nthw_tsm_get_ts(nthw_tsm_t *p, uint64_t *p_ts);
int nthw_tsm_get_time(nthw_tsm_t *p, uint64_t *p_time);

int nthw_tsm_set_timer_t0_enable(nthw_tsm_t *p, bool b_enable);
int nthw_tsm_set_timer_t0_max_count(nthw_tsm_t *p, uint32_t n_timer_val);
int nthw_tsm_set_timer_t1_enable(nthw_tsm_t *p, bool b_enable);
int nthw_tsm_set_timer_t1_max_count(nthw_tsm_t *p, uint32_t n_timer_val);

int nthw_tsm_set_config_ts_format(nthw_tsm_t *p, uint32_t n_val);

#endif	/* __NTHW_TSM_H__ */
