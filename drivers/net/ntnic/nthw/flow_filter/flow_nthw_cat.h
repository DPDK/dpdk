/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_CAT_H__
#define __FLOW_NTHW_CAT_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct cat_nthw;

typedef struct cat_nthw cat_nthw_t;

struct cat_nthw *nthw_cat_new(void);
void nthw_cat_delete(struct cat_nthw *p);
int nthw_cat_init(struct cat_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int cat_nthw_setup(struct cat_nthw *p, int n_idx, int n_idx_cnt);
void nthw_cat_set_debug_mode(struct cat_nthw *p, unsigned int n_debug_mode);

/* CFN */
void nthw_cat_cfn_select(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_cnt(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_enable(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_inv(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_inv(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_isl(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_cfp(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_mac(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_l2(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_vn_tag(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_vlan(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_mpls(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_l3(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_frag(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_ip_prot(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_l4(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_tunnel(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_tnl_l2(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_tnl_vlan(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_tnl_mpls(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_tnl_l3(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_tnl_frag(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_tnl_ip_prot(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_ptc_tnl_l4(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_err_inv(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_err_cv(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_err_fcs(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_err_trunc(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_err_l3_cs(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_err_l4_cs(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_err_tnl_l3_cs(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_err_tnl_l4_cs(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_err_ttl_exp(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_err_tnl_ttl_exp(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_mac_port(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_pm_cmp(const struct cat_nthw *p, const uint32_t *val);
void nthw_cat_cfn_pm_dct(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_pm_ext_inv(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_pm_cmb(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_pm_and_inv(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_pm_or_inv(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_pm_inv(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_lc(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_lc_inv(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_km0_or(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_km1_or(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cfn_flush(const struct cat_nthw *p);
/* KCE 0/1 */
void nthw_cat_kce_select(const struct cat_nthw *p, int index, uint32_t val);
void nthw_cat_kce_cnt(const struct cat_nthw *p, int index, uint32_t val);
void nthw_cat_kce_enable(const struct cat_nthw *p, int index, uint32_t val);
void nthw_cat_kce_flush(const struct cat_nthw *p, int index);
/* KCS 0/1 */
void nthw_cat_kcs_select(const struct cat_nthw *p, int index, uint32_t val);
void nthw_cat_kcs_cnt(const struct cat_nthw *p, int index, uint32_t val);
void nthw_cat_kcs_category(const struct cat_nthw *p, int index, uint32_t val);
void nthw_cat_kcs_flush(const struct cat_nthw *p, int index);
/* FTE 0/1 */
void nthw_cat_fte_select(const struct cat_nthw *p, int index, uint32_t val);
void nthw_cat_fte_cnt(const struct cat_nthw *p, int index, uint32_t val);
void nthw_cat_fte_enable(const struct cat_nthw *p, int index, uint32_t val);
void nthw_cat_fte_flush(const struct cat_nthw *p, int index);
/* CTE */
void nthw_cat_cte_select(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_cnt(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_enable_col(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_enable_cor(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_enable_hsh(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_enable_qsl(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_enable_ipf(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_enable_slc(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_enable_pdb(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_enable_msk(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_enable_hst(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_enable_epp(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_enable_tpe(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cte_flush(const struct cat_nthw *p);
/* CTS */
void nthw_cat_cts_select(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cts_cnt(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cts_flush(const struct cat_nthw *p);
void nthw_cat_cts_cat_a(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cts_cat_b(const struct cat_nthw *p, uint32_t val);
/* COT */
void nthw_cat_cot_select(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cot_cnt(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cot_color(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cot_km(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cot_flush(const struct cat_nthw *p);
/* CCT */
void nthw_cat_cct_select(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cct_cnt(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cct_color(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cct_km(const struct cat_nthw *p, uint32_t val);
void nthw_cat_cct_flush(const struct cat_nthw *p);
/* EXO */
void nthw_cat_exo_select(const struct cat_nthw *p, uint32_t val);
void nthw_cat_exo_cnt(const struct cat_nthw *p, uint32_t val);
void nthw_cat_exo_dyn(const struct cat_nthw *p, uint32_t val);
void nthw_cat_exo_ofs(const struct cat_nthw *p, int32_t val);
void nthw_cat_exo_flush(const struct cat_nthw *p);
/* RCK */
void nthw_cat_rck_select(const struct cat_nthw *p, uint32_t val);
void nthw_cat_rck_cnt(const struct cat_nthw *p, uint32_t val);
void nthw_cat_rck_data(const struct cat_nthw *p, uint32_t val);
void nthw_cat_rck_flush(const struct cat_nthw *p);
/* LEN */
void nthw_cat_len_select(const struct cat_nthw *p, uint32_t val);
void nthw_cat_len_cnt(const struct cat_nthw *p, uint32_t val);
void nthw_cat_len_lower(const struct cat_nthw *p, uint32_t val);
void nthw_cat_len_upper(const struct cat_nthw *p, uint32_t val);
void nthw_cat_len_dyn1(const struct cat_nthw *p, uint32_t val);
void nthw_cat_len_dyn2(const struct cat_nthw *p, uint32_t val);
void nthw_cat_len_inv(const struct cat_nthw *p, uint32_t val);
void nthw_cat_len_flush(const struct cat_nthw *p);
/* KCC */
void nthw_cat_kcc_select(const struct cat_nthw *p, uint32_t val);
void nthw_cat_kcc_cnt(const struct cat_nthw *p, uint32_t val);
void nthw_cat_kcc_key(const struct cat_nthw *p, uint32_t *val);
void nthw_cat_kcc_category(const struct cat_nthw *p, uint32_t val);
void nthw_cat_kcc_id(const struct cat_nthw *p, uint32_t val);
void nthw_cat_kcc_flush(const struct cat_nthw *p);

struct cat_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;
	nthw_module_t *m_cat;
	int m_km_if_cnt;

	nthw_register_t *mp_cfn_ctrl;
	nthw_field_t *mp_cfn_addr;
	nthw_field_t *mp_cfn_cnt;
	nthw_register_t *mp_cfn_data;
	nthw_field_t *mp_cfn_data_enable;
	nthw_field_t *mp_cfn_data_inv;
	nthw_field_t *mp_cfn_data_ptc_inv;
	nthw_field_t *mp_cfn_data_ptc_isl;
	nthw_field_t *mp_cfn_data_ptc_cfp;
	nthw_field_t *mp_cfn_data_ptc_mac;
	nthw_field_t *mp_cfn_data_ptc_l2;
	nthw_field_t *mp_cfn_data_ptc_vn_tag;
	nthw_field_t *mp_cfn_data_ptc_vlan;
	nthw_field_t *mp_cfn_data_ptc_mpls;
	nthw_field_t *mp_cfn_data_ptc_l3;
	nthw_field_t *mp_cfn_data_ptc_frag;
	nthw_field_t *mp_cfn_data_ptc_ip_prot;
	nthw_field_t *mp_cfn_data_ptc_l4;
	nthw_field_t *mp_cfn_data_ptc_tunnel;
	nthw_field_t *mp_cfn_data_ptc_tnl_l2;
	nthw_field_t *mp_cfn_data_ptc_tnl_vlan;
	nthw_field_t *mp_cfn_data_ptc_tnl_mpls;
	nthw_field_t *mp_cfn_data_ptc_tnl_l3;
	nthw_field_t *mp_cfn_data_ptc_tnl_frag;
	nthw_field_t *mp_cfn_data_ptc_tnl_ip_prot;
	nthw_field_t *mp_cfn_data_ptc_tnl_l4;
	nthw_field_t *mp_cfn_data_err_inv;
	nthw_field_t *mp_cfn_data_err_cv;
	nthw_field_t *mp_cfn_data_err_fcs;
	nthw_field_t *mp_cfn_data_err_trunc;
	nthw_field_t *mp_cfn_data_err_l3_cs;
	nthw_field_t *mp_cfn_data_err_l4_cs;
	nthw_field_t *mp_cfn_data_err_tnl_l3_cs;
	nthw_field_t *mp_cfn_data_err_tnl_l4_cs;
	nthw_field_t *mp_cfn_data_err_ttl_exp;
	nthw_field_t *mp_cfn_data_err_tnl_ttl_exp;
	nthw_field_t *mp_cfn_data_mac_port;
	nthw_field_t *mp_cfn_data_pm_cmp;
	nthw_field_t *mp_cfn_data_pm_dct;
	nthw_field_t *mp_cfn_data_pm_ext_inv;
	nthw_field_t *mp_cfn_data_pm_cmb;
	nthw_field_t *mp_cfn_data_pm_and_inv;
	nthw_field_t *mp_cfn_data_pm_or_inv;
	nthw_field_t *mp_cfn_data_pm_inv;
	nthw_field_t *mp_cfn_data_lc;
	nthw_field_t *mp_cfn_data_lc_inv;
	nthw_field_t *mp_cfn_data_km0_or;
	nthw_field_t *mp_cfn_data_km1_or;

	nthw_register_t *mp_kce_ctrl[2];
	nthw_field_t *mp_kce_addr[2];
	nthw_field_t *mp_kce_cnt[2];
	nthw_register_t *mp_kce_data[2];
	nthw_field_t *mp_kce_data_enable[2];

	nthw_register_t *mp_kcs_ctrl[2];
	nthw_field_t *mp_kcs_addr[2];
	nthw_field_t *mp_kcs_cnt[2];
	nthw_register_t *mp_kcs_data[2];
	nthw_field_t *mp_kcs_data_category[2];

	nthw_register_t *mp_fte_ctrl[2];
	nthw_field_t *mp_fte_addr[2];
	nthw_field_t *mp_fte_cnt[2];
	nthw_register_t *mp_fte_data[2];
	nthw_field_t *mp_fte_data_enable[2];

	nthw_register_t *mp_cte_ctrl;
	nthw_field_t *mp_cte_addr;
	nthw_field_t *mp_cte_cnt;
	nthw_register_t *mp_cte_data;
	nthw_field_t *mp_cte_data_col;
	nthw_field_t *mp_cte_data_cor;
	nthw_field_t *mp_cte_data_hsh;
	nthw_field_t *mp_cte_data_qsl;
	nthw_field_t *mp_cte_data_ipf;
	nthw_field_t *mp_cte_data_slc;
	nthw_field_t *mp_cte_data_pdb;
	nthw_field_t *mp_cte_data_msk;
	nthw_field_t *mp_cte_data_hst;
	nthw_field_t *mp_cte_data_epp;
	nthw_field_t *mp_cte_data_tpe;
	nthw_field_t *mp_cte_data_rrb;

	nthw_register_t *mp_cts_ctrl;
	nthw_field_t *mp_cts_addr;
	nthw_field_t *mp_cts_cnt;
	nthw_register_t *mp_cts_data;
	nthw_field_t *mp_cts_data_cat_a;
	nthw_field_t *mp_cts_data_cat_b;

	nthw_register_t *mp_cot_ctrl;
	nthw_field_t *mp_cot_addr;
	nthw_field_t *mp_cot_cnt;
	nthw_register_t *mp_cot_data;
	nthw_field_t *mp_cot_data_color;
	nthw_field_t *mp_cot_data_km;
	nthw_field_t *mp_cot_data_nfv_sb;

	nthw_register_t *mp_cct_ctrl;
	nthw_field_t *mp_cct_addr;
	nthw_field_t *mp_cct_cnt;
	nthw_register_t *mp_cct_data;
	nthw_field_t *mp_cct_data_color;
	nthw_field_t *mp_cct_data_km;

	nthw_register_t *mp_exo_ctrl;
	nthw_field_t *mp_exo_addr;
	nthw_field_t *mp_exo_cnt;
	nthw_register_t *mp_exo_data;
	nthw_field_t *mp_exo_data_dyn;
	nthw_field_t *mp_exo_data_ofs;

	nthw_register_t *mp_rck_ctrl;
	nthw_field_t *mp_rck_addr;
	nthw_field_t *mp_rck_cnt;
	nthw_register_t *mp_rck_data;

	nthw_register_t *mp_len_ctrl;
	nthw_field_t *mp_len_addr;
	nthw_field_t *mp_len_cnt;
	nthw_register_t *mp_len_data;
	nthw_field_t *mp_len_data_lower;
	nthw_field_t *mp_len_data_upper;
	nthw_field_t *mp_len_data_dyn1;
	nthw_field_t *mp_len_data_dyn2;
	nthw_field_t *mp_len_data_inv;
	nthw_register_t *mp_kcc_ctrl;
	nthw_field_t *mp_kcc_addr;
	nthw_field_t *mp_kcc_cnt;

	nthw_register_t *mp_kcc_data;
	nthw_field_t *mp_kcc_data_key;
	nthw_field_t *mp_kcc_data_category;
	nthw_field_t *mp_kcc_data_id;
};

#endif	/* __FLOW_NTHW_CAT_H__ */
