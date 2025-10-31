/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _HW_MOD_KM_V7_H_
#define _HW_MOD_KM_V7_H_

#include <stdint.h>

struct km_v7_rcp_s {
	uint32_t qw0_dyn;
	int32_t qw0_ofs;
	uint32_t qw0_sel_a;
	uint32_t qw0_sel_b;
	uint32_t qw4_dyn;
	int32_t qw4_ofs;
	uint32_t qw4_sel_a;
	uint32_t qw4_sel_b;
	uint32_t dw8_dyn;
	int32_t dw8_ofs;
	uint32_t dw8_sel_a;
	uint32_t dw8_sel_b;
	uint32_t dw10_dyn;
	int32_t dw10_ofs;
	uint32_t dw10_sel_a;
	uint32_t dw10_sel_b;
	uint32_t swx_cch;
	uint32_t swx_sel_a;
	uint32_t swx_sel_b;
	uint32_t mask_d_a[12];
	uint32_t mask_b[6];
	uint32_t dual;
	uint32_t paired;
	uint32_t el_a;
	uint32_t el_b;
	uint32_t info_a;
	uint32_t info_b;
	uint32_t ftm_a;
	uint32_t ftm_b;
	uint32_t bank_a;
	uint32_t bank_b;
	uint32_t kl_a;
	uint32_t kl_b;
	uint32_t keyway_a;
	uint32_t keyway_b;
	uint32_t synergy_mode;
	uint32_t dw0_b_dyn;
	int32_t dw0_b_ofs;
	uint32_t dw2_b_dyn;
	int32_t dw2_b_ofs;
	uint32_t sw4_b_dyn;
	int32_t sw4_b_ofs;
	uint32_t sw5_b_dyn;
	int32_t sw5_b_ofs;
};

struct km_v7_cam_s {
	uint32_t w0;
	uint32_t w1;
	uint32_t w2;
	uint32_t w3;
	uint32_t w4;
	uint32_t w5;
	uint32_t ft0;
	uint32_t ft1;
	uint32_t ft2;
	uint32_t ft3;
	uint32_t ft4;
	uint32_t ft5;
};

struct km_v7_tcam_s {
	uint32_t t[3];
	uint32_t dirty;
};

struct km_v7_tci_s {
	uint32_t color;
	uint32_t ft;
};

struct km_v7_tcq_s {
	uint32_t bank_mask;
	uint32_t qual;
};

struct hw_mod_km_v7_s {
	struct km_v7_rcp_s *rcp;
	struct km_v7_cam_s *cam;
	struct km_v7_tcam_s *tcam;
	struct km_v7_tci_s *tci;
	struct km_v7_tcq_s *tcq;
};

#endif	/* _HW_MOD_KM_V7_H_ */
