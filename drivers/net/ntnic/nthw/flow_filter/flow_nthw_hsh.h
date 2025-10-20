/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_HSH_H__
#define __FLOW_NTHW_HSH_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct hsh_nthw;

typedef struct hsh_nthw hsh_nthw_t;

struct hsh_nthw *nthw_hsh_new(void);
void nthw_hsh_delete(struct hsh_nthw *p);
int nthw_hsh_init(struct hsh_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int hsh_nthw_setup(struct hsh_nthw *p, int n_idx, int n_idx_cnt);
void nthw_hsh_set_debug_mode(struct hsh_nthw *p, unsigned int n_debug_mode);

/* RCP */
void nthw_hsh_rcp_select(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_cnt(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_load_dist_type(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_mac_port_mask(const struct hsh_nthw *p, uint32_t *val);
void nthw_hsh_rcp_sort(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_qw0_pe(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_qw0_ofs(const struct hsh_nthw *p, int32_t val);
void nthw_hsh_rcp_qw4_pe(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_qw4_ofs(const struct hsh_nthw *p, int32_t val);
void nthw_hsh_rcp_w8_pe(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_w8_ofs(const struct hsh_nthw *p, int32_t val);
void nthw_hsh_rcp_w8_sort(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_w9_pe(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_w9_ofs(const struct hsh_nthw *p, int32_t val);
void nthw_hsh_rcp_w9_sort(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_w9_p(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_p_mask(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_word_mask(const struct hsh_nthw *p, uint32_t *val);
void nthw_hsh_rcp_seed(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_tnl_p(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_hsh_valid(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_hsh_type(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_toeplitz(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_k(const struct hsh_nthw *p, uint32_t *val);
void nthw_hsh_rcp_auto_ipv4_mask(const struct hsh_nthw *p, uint32_t val);
void nthw_hsh_rcp_flush(const struct hsh_nthw *p);

struct hsh_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_hsh;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_addr;
	nthw_field_t *mp_rcp_cnt;
	nthw_register_t *mp_rcp_data;
	nthw_field_t *mp_rcp_data_load_dist_type;
	nthw_field_t *mp_rcp_data_mac_port_mask;
	nthw_field_t *mp_rcp_data_sort;
	nthw_field_t *mp_rcp_data_qw0_pe;
	nthw_field_t *mp_rcp_data_qw0_ofs;
	nthw_field_t *mp_rcp_data_qw4_pe;
	nthw_field_t *mp_rcp_data_qw4_ofs;
	nthw_field_t *mp_rcp_data_w8_pe;
	nthw_field_t *mp_rcp_data_w8_ofs;
	nthw_field_t *mp_rcp_data_w8_sort;
	nthw_field_t *mp_rcp_data_w9_pe;
	nthw_field_t *mp_rcp_data_w9_ofs;
	nthw_field_t *mp_rcp_data_w9_sort;
	nthw_field_t *mp_rcp_data_w9_p;
	nthw_field_t *mp_rcp_data_p_mask;
	nthw_field_t *mp_rcp_data_word_mask;
	nthw_field_t *mp_rcp_data_seed;
	nthw_field_t *mp_rcp_data_tnl_p;
	nthw_field_t *mp_rcp_data_hsh_valid;
	nthw_field_t *mp_rcp_data_hsh_type;
	nthw_field_t *mp_rcp_data_toeplitz;
	nthw_field_t *mp_rcp_data_k;
	nthw_field_t *mp_rcp_data_auto_ipv4_mask;
};

#endif	/* __FLOW_NTHW_HSH_H__ */
