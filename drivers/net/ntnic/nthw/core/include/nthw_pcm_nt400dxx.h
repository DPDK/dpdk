/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */
#ifndef __NTHW_PCM_NT400DXX_H__
#define __NTHW_PCM_NT400DXX_H__

#include "nthw_fpga_model.h"

struct nthw_pcm_nt400_dxx {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_pcm;
	int mn_instance;

	int mn_module_major_version;
	int mn_module_minor_version;

	nthw_register_t *mp_reg_ctrl;
	nthw_field_t *mp_fld_ctrl_ts_pll_recal;	/* Dunite HW version 3 */
	nthw_field_t *mp_fld_ctrl_ts_clksel;
	nthw_field_t *mp_fld_ctrl_ts_pll_rst;

	nthw_register_t *mp_reg_stat;
	nthw_field_t *mp_fld_stat_ts_pll_locked;

	nthw_register_t *mp_reg_latch;
	nthw_field_t *mp_fld_latch_ts_pll_locked;
};

typedef struct nthw_pcm_nt400_dxx nthw_pcm_nt400dxx_t;
typedef struct nthw_pcm_nt400_dxx nthw_pcm_nt400_dxx;

nthw_pcm_nt400dxx_t *nthw_pcm_nt400dxx_new(void);
int nthw_pcm_nt400dxx_init(nthw_pcm_nt400dxx_t *p, nthw_fpga_t *p_fpga, int n_instance);
void nthw_pcm_nt400dxx_set_ts_pll_recal(nthw_pcm_nt400dxx_t *p, uint32_t val);
bool nthw_pcm_nt400dxx_get_ts_pll_locked_stat(nthw_pcm_nt400dxx_t *p);
void nthw_pcm_nt400dxx_set_ts_pll_locked_latch(nthw_pcm_nt400dxx_t *p, uint32_t val);

#endif	/* __NTHW_PCM_NT400DXX_H__ */
