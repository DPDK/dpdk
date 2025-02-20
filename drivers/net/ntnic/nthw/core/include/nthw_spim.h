/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_SPIM_H__
#define __NTHW_SPIM_H__

#include "nthw_fpga.h"

struct nthw_spim {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_spim;
	int mn_instance;

	nthw_register_t *mp_reg_srr;
	nthw_field_t *mp_fld_srr_rst;

	nthw_register_t *mp_reg_cr;
	nthw_field_t *mp_fld_cr_loop;
	nthw_field_t *mp_fld_cr_en;
	nthw_field_t *mp_fld_cr_txrst;
	nthw_field_t *mp_fld_cr_rxrst;

	nthw_register_t *mp_reg_sr;
	nthw_field_t *mp_fld_sr_done;
	nthw_field_t *mp_fld_sr_txempty;
	nthw_field_t *mp_fld_sr_rxempty;
	nthw_field_t *mp_fld_sr_txfull;
	nthw_field_t *mp_fld_sr_rxfull;
	nthw_field_t *mp_fld_sr_txlvl;
	nthw_field_t *mp_fld_sr_rxlvl;

	nthw_register_t *mp_reg_dtr;
	nthw_field_t *mp_fld_dtr_dtr;

	nthw_register_t *mp_reg_drr;
	nthw_field_t *mp_fld_drr_drr;

	nthw_register_t *mp_reg_cfg;
	nthw_field_t *mp_fld_cfg_pre;

	nthw_register_t *mp_reg_cfg_clk;
	nthw_field_t *mp_fld_cfg_clk_mode;
};

typedef struct nthw_spim nthw_spim_t;
typedef struct nthw_spim nthw_spim;

nthw_spim_t *nthw_spim_new(void);
int nthw_spim_init(nthw_spim_t *p, nthw_fpga_t *p_fpga, int n_instance);

uint32_t nthw_spim_reset(nthw_spim_t *p);
uint32_t nthw_spim_enable(nthw_spim_t *p, bool b_enable);
uint32_t nthw_spim_get_tx_fifo_empty(nthw_spim_t *p, bool *pb_empty);
uint32_t nthw_spim_write_tx_fifo(nthw_spim_t *p, uint32_t n_data);

#endif	/* __NTHW_SPIM_H__ */
