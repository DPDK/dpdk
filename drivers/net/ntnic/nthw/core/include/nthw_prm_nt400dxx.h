/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef NTHW_PRM_NT400DXX_H_
#define NTHW_PRM_NT400DXX_H_

#include <stdint.h>

#include "nthw_fpga_model.h"

struct nt_prm_nt400dxx {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_prm;

	int mn_instance;

	nthw_register_t *mp_reg_rst;
	nthw_field_t *mp_fld_rst_periph;
	nthw_field_t *mp_fld_rst_platform;
};

typedef struct nt_prm_nt400dxx nthw_prm_nt400dxx_t;
typedef struct nt_prm_nt400dxx nt_prm_nt400dxx;

nthw_prm_nt400dxx_t *nthw_prm_nt400dxx_new(void);
int nthw_prm_nt400dxx_init(nthw_prm_nt400dxx_t *p, nthw_fpga_t *p_fpga, int n_instance);
void nthw_prm_nt400dxx_periph_rst(nthw_prm_nt400dxx_t *p, uint32_t val);
void nthw_prm_nt400dxx_platform_rst(nthw_prm_nt400dxx_t *p, uint32_t val);

#endif	/* NTHW_PRM_NT400DXX_H_ */
