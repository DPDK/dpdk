/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_IGAM_H__
#define __NTHW_IGAM_H__

#include "nthw_fpga_model.h"

struct nt_igam {
	nthw_fpga_t *mp_fpga;

	nthw_module_t *mp_mod_igam;

	int mn_igam_instance;

	nthw_register_t *mp_reg_base;
	nthw_field_t *mp_fld_base_ptr;
	nthw_field_t *mp_fld_base_busy;
	nthw_field_t *mp_fld_base_cmd;

	nthw_register_t *mp_reg_data;
	nthw_field_t *mp_fld_data_data;

	/* CTRL From version 0.1 */
	nthw_register_t *mp_reg_ctrl;
	nthw_field_t *mp_fld_ctrl_forward_rst;
};

typedef struct nt_igam nthw_igam_t;
typedef struct nt_igam nthw_igam;

nthw_igam_t *nthw_igam_new(void);
int nthw_igam_init(nthw_igam_t *p, nthw_fpga_t *p_fpga, int mn_igam_instance);
uint32_t nthw_igam_read(nthw_igam_t *p, uint32_t address);
void nthw_igam_write(nthw_igam_t *p, uint32_t address, uint32_t data);
void nthw_igam_set_ctrl_forward_rst(nthw_igam_t *p, uint32_t value);

#endif	/*  __NTHW_IGAM_H__ */
