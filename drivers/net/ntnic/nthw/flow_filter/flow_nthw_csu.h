/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FLOW_NTHW_CSU_H_
#define _FLOW_NTHW_CSU_H_

#include <stdint.h>

#include "nthw_fpga_model.h"

struct csu_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_csu;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_ctrl_adr;
	nthw_field_t *mp_rcp_ctrl_cnt;
	nthw_register_t *mp_rcp_data;
	nthw_field_t *mp_rcp_data_ol3_cmd;
	nthw_field_t *mp_rcp_data_ol4_cmd;
	nthw_field_t *mp_rcp_data_il3_cmd;
	nthw_field_t *mp_rcp_data_il4_cmd;
};

struct csu_nthw *nthw_csu_new(void);
void nthw_csu_delete(struct csu_nthw *p);
int nthw_csu_init(struct csu_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int csu_nthw_setup(struct csu_nthw *p, int n_idx, int n_idx_cnt);
void nthw_csu_set_debug_mode(struct csu_nthw *p, unsigned int n_debug_mode);

void nthw_csu_rcp_select(const struct csu_nthw *p, uint32_t val);
void nthw_csu_rcp_cnt(const struct csu_nthw *p, uint32_t val);
void nthw_csu_rcp_outer_l3_cmd(const struct csu_nthw *p, uint32_t val);
void nthw_csu_rcp_outer_l4_cmd(const struct csu_nthw *p, uint32_t val);
void nthw_csu_rcp_inner_l3_cmd(const struct csu_nthw *p, uint32_t val);
void nthw_csu_rcp_inner_l4_cmd(const struct csu_nthw *p, uint32_t val);
void nthw_csu_rcp_flush(const struct csu_nthw *p);

#endif	/* _FLOW_NTHW_CSU_H_ */
