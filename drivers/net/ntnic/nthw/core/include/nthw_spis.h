/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_SPIS_H__
#define __NTHW_SPIS_H__

#include "nthw_fpga.h"

struct nthw_spis {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_spis;
	int mn_instance;

	nthw_register_t *mp_reg_srr;
	nthw_field_t *mp_fld_srr_rst;

	nthw_register_t *mp_reg_cr;
	nthw_field_t *mp_fld_cr_loop;
	nthw_field_t *mp_fld_cr_en;
	nthw_field_t *mp_fld_cr_txrst;
	nthw_field_t *mp_fld_cr_rxrst;
	nthw_field_t *mp_fld_cr_debug;

	nthw_register_t *mp_reg_sr;
	nthw_field_t *mp_fld_sr_done;
	nthw_field_t *mp_fld_sr_txempty;
	nthw_field_t *mp_fld_sr_rxempty;
	nthw_field_t *mp_fld_sr_txfull;
	nthw_field_t *mp_fld_sr_rxfull;
	nthw_field_t *mp_fld_sr_txlvl;
	nthw_field_t *mp_fld_sr_rxlvl;
	nthw_field_t *mp_fld_sr_frame_err;
	nthw_field_t *mp_fld_sr_read_err;
	nthw_field_t *mp_fld_sr_write_err;

	nthw_register_t *mp_reg_dtr;
	nthw_field_t *mp_fld_dtr_dtr;

	nthw_register_t *mp_reg_drr;
	nthw_field_t *mp_fld_drr_drr;

	nthw_register_t *mp_reg_ram_ctrl;
	nthw_field_t *mp_fld_ram_ctrl_adr;
	nthw_field_t *mp_fld_ram_ctrl_cnt;

	nthw_register_t *mp_reg_ram_data;
	nthw_field_t *mp_fld_ram_data_data;
};

typedef struct nthw_spis nthw_spis_t;
typedef struct nthw_spis nthw_spis;

nthw_spis_t *nthw_spis_new(void);
int nthw_spis_init(nthw_spis_t *p, nthw_fpga_t *p_fpga, int n_instance);

uint32_t nthw_spis_reset(nthw_spis_t *p);
uint32_t nthw_spis_enable(nthw_spis_t *p, bool b_enable);
uint32_t nthw_spis_get_rx_fifo_empty(nthw_spis_t *p, bool *pb_empty);
uint32_t nthw_spis_read_rx_fifo(nthw_spis_t *p, uint32_t *p_data);

#endif	/* __NTHW_SPIS_H__ */
