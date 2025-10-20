/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_HFU_H__
#define __FLOW_NTHW_HFU_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct hfu_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_hfu;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_addr;
	nthw_field_t *mp_rcp_cnt;

	nthw_register_t *mp_rcp_data;
	nthw_field_t *mp_rcp_data_len_a_wr;
	nthw_field_t *mp_rcp_data_len_a_ol4len;
	nthw_field_t *mp_rcp_data_len_a_pos_dyn;
	nthw_field_t *mp_rcp_data_len_a_pos_ofs;
	nthw_field_t *mp_rcp_data_len_a_add_dyn;
	nthw_field_t *mp_rcp_data_len_a_add_ofs;
	nthw_field_t *mp_rcp_data_len_a_sub_dyn;
	nthw_field_t *mp_rcp_data_len_b_wr;
	nthw_field_t *mp_rcp_data_len_b_pos_dyn;
	nthw_field_t *mp_rcp_data_len_b_pos_ofs;
	nthw_field_t *mp_rcp_data_len_b_add_dyn;
	nthw_field_t *mp_rcp_data_len_b_add_ofs;
	nthw_field_t *mp_rcp_data_len_b_sub_dyn;
	nthw_field_t *mp_rcp_data_len_c_wr;
	nthw_field_t *mp_rcp_data_len_c_pos_dyn;
	nthw_field_t *mp_rcp_data_len_c_pos_ofs;
	nthw_field_t *mp_rcp_data_len_c_add_dyn;
	nthw_field_t *mp_rcp_data_len_c_add_ofs;
	nthw_field_t *mp_rcp_data_len_c_sub_dyn;
	nthw_field_t *mp_rcp_data_ttl_wr;
	nthw_field_t *mp_rcp_data_ttl_pos_dyn;
	nthw_field_t *mp_rcp_data_ttl_pos_ofs;
};

struct hfu_nthw *nthw_hfu_new(void);
void nthw_hfu_delete(struct hfu_nthw *p);
int nthw_hfu_init(struct hfu_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int hfu_nthw_setup(struct hfu_nthw *p, int n_idx, int n_idx_cnt);
void nthw_hfu_set_debug_mode(struct hfu_nthw *p, unsigned int n_debug_mode);

/* RCP */
void nthw_hfu_rcp_select(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_cnt(const struct hfu_nthw *p, uint32_t val);

void nthw_hfu_rcp_len_a_wr(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_a_ol4len(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_a_pos_dyn(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_a_pos_ofs(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_a_add_dyn(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_a_add_ofs(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_a_sub_dyn(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_b_wr(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_b_pos_dyn(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_b_pos_ofs(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_b_add_dyn(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_b_add_ofs(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_b_sub_dyn(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_c_wr(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_c_pos_dyn(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_c_pos_ofs(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_c_add_dyn(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_c_add_ofs(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_len_c_sub_dyn(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_ttl_wr(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_ttl_pos_dyn(const struct hfu_nthw *p, uint32_t val);
void nthw_hfu_rcp_ttl_pos_ofs(const struct hfu_nthw *p, uint32_t val);

void nthw_hfu_rcp_flush(const struct hfu_nthw *p);

#endif	/* __FLOW_NTHW_HFU_H__ */
