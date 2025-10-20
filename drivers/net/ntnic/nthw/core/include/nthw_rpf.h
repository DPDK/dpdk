/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef NTHW_RPF_HPP_
#define NTHW_RPF_HPP_

#include <rte_spinlock.h>

#include "nthw_fpga_model.h"

struct nthw_rpf {
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_mod_rpf;

	int mn_instance;

	nthw_register_t *mp_reg_control;
	nthw_field_t *mp_fld_control_pen;
	nthw_field_t *mp_fld_control_rpp_en;
	nthw_field_t *mp_fld_control_st_tgl_en;
	nthw_field_t *mp_fld_control_keep_alive_en;

	nthw_register_t *mp_ts_sort_prg;
	nthw_field_t *mp_fld_ts_sort_prg_maturing_delay;
	nthw_field_t *mp_fld_ts_sort_prg_ts_at_eof;

	int m_default_maturing_delay;
	bool m_administrative_block;	/* used to enforce license expiry */

	rte_spinlock_t rpf_mutex;
};

typedef struct nthw_rpf nthw_rpf_t;
typedef struct nthw_rpf nt_rpf;

nthw_rpf_t *nthw_rpf_new(void);
int nthw_rpf_init(nthw_rpf_t *p, nthw_fpga_t *p_fpga, int n_instance);
void nthw_rpf_block(nthw_rpf_t *p);
void nthw_rpf_unblock(nthw_rpf_t *p);
void nthw_rpf_set_maturing_delay(nthw_rpf_t *p, int32_t delay);
int32_t nthw_rpf_get_maturing_delay(nthw_rpf_t *p);
void nthw_rpf_set_ts_at_eof(nthw_rpf_t *p, bool enable);
bool nthw_rpf_get_ts_at_eof(nthw_rpf_t *p);

#endif
