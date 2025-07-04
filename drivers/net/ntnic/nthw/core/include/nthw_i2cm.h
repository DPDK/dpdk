/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_II2CM_H__
#define __NTHW_II2CM_H__

#include "nthw_fpga_model.h"
#include "rte_spinlock.h"

struct nt_i2cm {
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_mod_i2cm;

	int mn_i2c_instance;

	nthw_register_t *mp_reg_prer_low;
	nthw_field_t *mp_fld_prer_low_prer_low;

	nthw_register_t *mp_reg_prer_high;
	nthw_field_t *mp_fld_prer_high_prer_high;

	nthw_register_t *mp_reg_ctrl;
	nthw_field_t *mp_fld_ctrl_ien;
	nthw_field_t *mp_fld_ctrl_en;

	nthw_register_t *mp_reg_data;
	nthw_field_t *mp_fld_data_data;

	nthw_register_t *mp_reg_cmd_status;
	nthw_field_t *mp_fld_cmd_status_cmd_status;

	nthw_register_t *mp_reg_select;
	nthw_field_t *mp_fld_select_select;

	nthw_register_t *mp_reg_io_exp;
	nthw_field_t *mp_fld_io_exp_rst;
	nthw_field_t *mp_fld_io_exp_int_b;

	rte_spinlock_t i2cmmutex;
};

typedef struct nt_i2cm nthw_i2cm_t;

nthw_i2cm_t *nthw_i2cm_new(void);
int nthw_i2cm_init(nthw_i2cm_t *p, nthw_fpga_t *p_fpga, int n_i2c_instance);
int nthw_i2cm_read(nthw_i2cm_t *p, uint8_t dev_addr, uint8_t reg_addr, uint8_t *value);
int nthw_i2cm_write(nthw_i2cm_t *p, uint8_t dev_addr, uint8_t reg_addr, uint8_t value);
int nthw_i2cm_write16(nthw_i2cm_t *p, uint8_t dev_addr, uint8_t reg_addr, uint16_t value);

#endif	/* __NTHW_II2CM_H__ */
