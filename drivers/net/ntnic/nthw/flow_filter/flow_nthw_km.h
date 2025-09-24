/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_KM_H__
#define __FLOW_NTHW_KM_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct km_nthw;

typedef struct km_nthw km_nthw_t;

struct km_nthw *nthw_km_new(void);
void nthw_km_delete(struct km_nthw *p);
int nthw_km_init(struct km_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int km_nthw_setup(struct km_nthw *p, int n_idx, int n_idx_cnt);
void nthw_km_set_debug_mode(struct km_nthw *p, unsigned int n_debug_mode);

/* RCP initial v3 */
void nthw_km_rcp_select(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_cnt(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_qw0_dyn(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_qw0_ofs(const struct km_nthw *p, int32_t val);
void nthw_km_rcp_qw0_sel_a(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_qw0_sel_b(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_qw4_dyn(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_qw4_ofs(const struct km_nthw *p, int32_t val);
void nthw_km_rcp_qw4_sel_a(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_qw4_sel_b(const struct km_nthw *p, uint32_t val);
/* subst in v6 */
void nthw_km_rcp_dw8_dyn(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_dw8_ofs(const struct km_nthw *p, int32_t val);
void nthw_km_rcp_dw8_sel_a(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_dw8_sel_b(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_dw10_dyn(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_dw10_ofs(const struct km_nthw *p, int32_t val);
void nthw_km_rcp_dw10_sel_a(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_dw10_sel_b(const struct km_nthw *p, uint32_t val);

void nthw_km_rcp_swx_cch(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_swx_sel_a(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_swx_sel_b(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_mask_da(const struct km_nthw *p, const uint32_t *val);
void nthw_km_rcp_mask_b(const struct km_nthw *p, const uint32_t *val);
void nthw_km_rcp_dual(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_paired(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_el_a(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_el_b(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_info_a(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_info_b(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_ftm_a(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_ftm_b(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_bank_a(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_bank_b(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_kl_a(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_kl_b(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_keyway_a(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_keyway_b(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_synergy_mode(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_dw0_b_dyn(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_dw0_b_ofs(const struct km_nthw *p, int32_t val);
void nthw_km_rcp_dw2_b_dyn(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_dw2_b_ofs(const struct km_nthw *p, int32_t val);
void nthw_km_rcp_sw4_b_dyn(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_sw4_b_ofs(const struct km_nthw *p, int32_t val);
void nthw_km_rcp_sw5_b_dyn(const struct km_nthw *p, uint32_t val);
void nthw_km_rcp_sw5_b_ofs(const struct km_nthw *p, int32_t val);
void nthw_km_rcp_flush(const struct km_nthw *p);
/* CAM */
void nthw_km_cam_select(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_cnt(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_w0(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_w1(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_w2(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_w3(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_w4(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_w5(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_ft0(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_ft1(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_ft2(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_ft3(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_ft4(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_ft5(const struct km_nthw *p, uint32_t val);
void nthw_km_cam_flush(const struct km_nthw *p);
/* TCAM */
void nthw_km_tcam_select(const struct km_nthw *p, uint32_t val);
void nthw_km_tcam_cnt(const struct km_nthw *p, uint32_t val);
void nthw_km_tcam_t(const struct km_nthw *p, uint32_t *val);
void nthw_km_tcam_flush(const struct km_nthw *p);
/* TCI */
void nthw_km_tci_select(const struct km_nthw *p, uint32_t val);
void nthw_km_tci_cnt(const struct km_nthw *p, uint32_t val);
void nthw_km_tci_color(const struct km_nthw *p, uint32_t val);
void nthw_km_tci_ft(const struct km_nthw *p, uint32_t val);
void nthw_km_tci_flush(const struct km_nthw *p);
/* TCQ */
void nthw_km_tcq_select(const struct km_nthw *p, uint32_t val);
void nthw_km_tcq_cnt(const struct km_nthw *p, uint32_t val);
void nthw_km_tcq_bank_mask(const struct km_nthw *p, uint32_t val);
void nthw_km_tcq_qual(const struct km_nthw *p, uint32_t val);

void nthw_km_tcq_flush(const struct km_nthw *p);

struct km_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_km;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_addr;
	nthw_field_t *mp_rcp_cnt;
	nthw_register_t *mp_rcp_data;
	nthw_field_t *mp_rcp_data_qw0_dyn;
	nthw_field_t *mp_rcp_data_qw0_ofs;
	nthw_field_t *mp_rcp_data_qw0_sel_a;
	nthw_field_t *mp_rcp_data_qw0_sel_b;
	nthw_field_t *mp_rcp_data_qw4_dyn;
	nthw_field_t *mp_rcp_data_qw4_ofs;
	nthw_field_t *mp_rcp_data_qw4_sel_a;
	nthw_field_t *mp_rcp_data_qw4_sel_b;
	nthw_field_t *mp_rcp_data_sw8_dyn;
	nthw_field_t *mp_rcp_data_sw8_ofs;
	nthw_field_t *mp_rcp_data_sw8_sel_a;
	nthw_field_t *mp_rcp_data_sw8_sel_b;
	nthw_field_t *mp_rcp_data_sw9_dyn;
	nthw_field_t *mp_rcp_data_sw9_ofs;
	nthw_field_t *mp_rcp_data_sw9_sel_a;
	nthw_field_t *mp_rcp_data_sw9_sel_b;

	nthw_field_t *mp_rcp_data_dw8_dyn;	/* substituted Sw<x> from v6+ */
	nthw_field_t *mp_rcp_data_dw8_ofs;	/* substituted Sw<x> from v6+ */
	nthw_field_t *mp_rcp_data_dw8_sel_a;	/* substituted Sw<x> from v6+ */
	nthw_field_t *mp_rcp_data_dw8_sel_b;	/* substituted Sw<x> from v6+ */
	nthw_field_t *mp_rcp_data_dw10_dyn;	/* substituted Sw<x> from v6+ */
	nthw_field_t *mp_rcp_data_dw10_ofs;	/* substituted Sw<x> from v6+ */
	nthw_field_t *mp_rcp_data_dw10_sel_a;	/* substituted Sw<x> from v6+ */
	nthw_field_t *mp_rcp_data_dw10_sel_b;	/* substituted Sw<x> from v6+ */

	nthw_field_t *mp_rcp_data_swx_ovs_sb;
	nthw_field_t *mp_rcp_data_swx_cch;
	nthw_field_t *mp_rcp_data_swx_sel_a;
	nthw_field_t *mp_rcp_data_swx_sel_b;
	nthw_field_t *mp_rcp_data_mask_a;
	nthw_field_t *mp_rcp_data_mask_b;
	nthw_field_t *mp_rcp_data_dual;
	nthw_field_t *mp_rcp_data_paired;
	nthw_field_t *mp_rcp_data_el_a;
	nthw_field_t *mp_rcp_data_el_b;
	nthw_field_t *mp_rcp_data_info_a;
	nthw_field_t *mp_rcp_data_info_b;
	nthw_field_t *mp_rcp_data_ftm_a;
	nthw_field_t *mp_rcp_data_ftm_b;
	nthw_field_t *mp_rcp_data_bank_a;
	nthw_field_t *mp_rcp_data_bank_b;
	nthw_field_t *mp_rcp_data_kl_a;
	nthw_field_t *mp_rcp_data_kl_b;
	nthw_field_t *mp_rcp_data_flow_set;
	nthw_field_t *mp_rcp_data_keyway_a;
	nthw_field_t *mp_rcp_data_keyway_b;
	nthw_field_t *mp_rcp_data_synergy_mode;
	nthw_field_t *mp_rcp_data_dw0_b_dyn;
	nthw_field_t *mp_rcp_data_dw0_b_ofs;
	nthw_field_t *mp_rcp_data_dw2_b_dyn;
	nthw_field_t *mp_rcp_data_dw2_b_ofs;
	nthw_field_t *mp_rcp_data_sw4_b_dyn;
	nthw_field_t *mp_rcp_data_sw4_b_ofs;
	nthw_field_t *mp_rcp_data_sw5_b_dyn;
	nthw_field_t *mp_rcp_data_sw5_b_ofs;

	nthw_register_t *mp_cam_ctrl;
	nthw_field_t *mp_cam_addr;
	nthw_field_t *mp_cam_cnt;
	nthw_register_t *mp_cam_data;
	nthw_field_t *mp_cam_data_w0;
	nthw_field_t *mp_cam_data_w1;
	nthw_field_t *mp_cam_data_w2;
	nthw_field_t *mp_cam_data_w3;
	nthw_field_t *mp_cam_data_w4;
	nthw_field_t *mp_cam_data_w5;
	nthw_field_t *mp_cam_data_ft0;
	nthw_field_t *mp_cam_data_ft1;
	nthw_field_t *mp_cam_data_ft2;
	nthw_field_t *mp_cam_data_ft3;
	nthw_field_t *mp_cam_data_ft4;
	nthw_field_t *mp_cam_data_ft5;

	nthw_register_t *mp_tcam_ctrl;
	nthw_field_t *mp_tcam_addr;
	nthw_field_t *mp_tcam_cnt;
	nthw_register_t *mp_tcam_data;
	nthw_field_t *mp_tcam_data_t;

	nthw_register_t *mp_tci_ctrl;
	nthw_field_t *mp_tci_addr;
	nthw_field_t *mp_tci_cnt;
	nthw_register_t *mp_tci_data;
	nthw_field_t *mp_tci_data_color;
	nthw_field_t *mp_tci_data_ft;

	nthw_register_t *mp_tcq_ctrl;
	nthw_field_t *mp_tcq_addr;
	nthw_field_t *mp_tcq_cnt;
	nthw_register_t *mp_tcq_data;
	nthw_field_t *mp_tcq_data_bank_mask;
	nthw_field_t *mp_tcq_data_qual;
};

#endif	/* __FLOW_NTHW_KM_H__ */
