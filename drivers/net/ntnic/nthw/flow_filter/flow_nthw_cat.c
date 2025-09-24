/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_cat.h"

struct cat_nthw *nthw_cat_new(void)
{
	struct cat_nthw *p = malloc(sizeof(struct cat_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_cat_delete(struct cat_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

void nthw_cat_set_debug_mode(struct cat_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_cat, n_debug_mode);
}

int nthw_cat_init(struct cat_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_CAT, n_instance);

	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: Cat %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_cat = p_mod;

	p->m_km_if_cnt = nthw_fpga_get_product_param(p->mp_fpga, NT_CAT_KM_IF_CNT, -1);

	/* CFN */
	p->mp_cfn_ctrl = nthw_module_get_register(p->m_cat, CAT_CFN_CTRL);
	p->mp_cfn_addr = nthw_register_get_field(p->mp_cfn_ctrl, CAT_CFN_CTRL_ADR);
	p->mp_cfn_cnt = nthw_register_get_field(p->mp_cfn_ctrl, CAT_CFN_CTRL_CNT);
	p->mp_cfn_data = nthw_module_get_register(p->m_cat, CAT_CFN_DATA);
	p->mp_cfn_data_enable = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_ENABLE);
	p->mp_cfn_data_inv = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_INV);
	p->mp_cfn_data_ptc_inv = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_INV);
	p->mp_cfn_data_ptc_isl = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_ISL);
	p->mp_cfn_data_ptc_mac = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_MAC);
	p->mp_cfn_data_ptc_l2 = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_L2);
	p->mp_cfn_data_ptc_vn_tag =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_VNTAG);
	p->mp_cfn_data_ptc_vlan = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_VLAN);
	p->mp_cfn_data_ptc_mpls = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_MPLS);
	p->mp_cfn_data_ptc_l3 = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_L3);
	p->mp_cfn_data_ptc_frag = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_FRAG);
	p->mp_cfn_data_ptc_ip_prot =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_IP_PROT);
	p->mp_cfn_data_ptc_l4 = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_L4);
	p->mp_cfn_data_ptc_tunnel =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_TUNNEL);
	p->mp_cfn_data_ptc_tnl_l2 =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_TNL_L2);
	p->mp_cfn_data_ptc_tnl_vlan =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_TNL_VLAN);
	p->mp_cfn_data_ptc_tnl_mpls =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_TNL_MPLS);
	p->mp_cfn_data_ptc_tnl_l3 =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_TNL_L3);
	p->mp_cfn_data_ptc_tnl_frag =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_TNL_FRAG);
	p->mp_cfn_data_ptc_tnl_ip_prot =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_TNL_IP_PROT);
	p->mp_cfn_data_ptc_tnl_l4 =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_TNL_L4);
	p->mp_cfn_data_err_inv = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_ERR_INV);
	p->mp_cfn_data_err_cv = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_ERR_CV);
	p->mp_cfn_data_err_fcs = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_ERR_FCS);
	p->mp_cfn_data_err_trunc = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_ERR_TRUNC);
	p->mp_cfn_data_mac_port = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_MAC_PORT);
	p->mp_cfn_data_pm_cmp = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PM_CMP);
	p->mp_cfn_data_pm_dct = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PM_DCT);
	p->mp_cfn_data_pm_ext_inv =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PM_EXT_INV);
	p->mp_cfn_data_pm_cmb = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PM_CMB);
	p->mp_cfn_data_pm_and_inv =
		nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PM_AND_INV);
	p->mp_cfn_data_pm_or_inv = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PM_OR_INV);
	p->mp_cfn_data_pm_inv = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_PM_INV);
	p->mp_cfn_data_lc = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_LC);
	p->mp_cfn_data_lc_inv = nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_LC_INV);

	if (p->m_km_if_cnt == -1) {
		p->mp_cfn_data_km0_or =
			nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_KM_OR);

	} else {
		p->mp_cfn_data_km0_or =
			nthw_register_get_field(p->mp_cfn_data, CAT_CFN_DATA_KM0_OR);
		p->mp_cfn_data_km1_or =
			nthw_register_query_field(p->mp_cfn_data, CAT_CFN_DATA_KM1_OR);
	}

	if (p->m_km_if_cnt < 0) {
		/* KCE */
		p->mp_kce_ctrl[0] = nthw_module_get_register(p->m_cat, CAT_KCE_CTRL);
		p->mp_kce_addr[0] = nthw_register_get_field(p->mp_kce_ctrl[0], CAT_KCE_CTRL_ADR);
		p->mp_kce_cnt[0] = nthw_register_get_field(p->mp_kce_ctrl[0], CAT_KCE_CTRL_CNT);
		p->mp_kce_data[0] = nthw_module_get_register(p->m_cat, CAT_KCE_DATA);
		p->mp_kce_data_enable[0] =
			nthw_register_get_field(p->mp_kce_data[0], CAT_KCE_DATA_ENABLE);
		/* KCS */
		p->mp_kcs_ctrl[0] = nthw_module_get_register(p->m_cat, CAT_KCS_CTRL);
		p->mp_kcs_addr[0] = nthw_register_get_field(p->mp_kcs_ctrl[0], CAT_KCS_CTRL_ADR);
		p->mp_kcs_cnt[0] = nthw_register_get_field(p->mp_kcs_ctrl[0], CAT_KCS_CTRL_CNT);
		p->mp_kcs_data[0] = nthw_module_get_register(p->m_cat, CAT_KCS_DATA);
		p->mp_kcs_data_category[0] =
			nthw_register_get_field(p->mp_kcs_data[0], CAT_KCS_DATA_CATEGORY);
		/* FTE */
		p->mp_fte_ctrl[0] = nthw_module_get_register(p->m_cat, CAT_FTE_CTRL);
		p->mp_fte_addr[0] = nthw_register_get_field(p->mp_fte_ctrl[0], CAT_FTE_CTRL_ADR);
		p->mp_fte_cnt[0] = nthw_register_get_field(p->mp_fte_ctrl[0], CAT_FTE_CTRL_CNT);
		p->mp_fte_data[0] = nthw_module_get_register(p->m_cat, CAT_FTE_DATA);
		p->mp_fte_data_enable[0] =
			nthw_register_get_field(p->mp_fte_data[0], CAT_FTE_DATA_ENABLE);

	} else {
		/* KCE 0 */
		p->mp_kce_ctrl[0] = nthw_module_get_register(p->m_cat, CAT_KCE0_CTRL);
		p->mp_kce_addr[0] = nthw_register_get_field(p->mp_kce_ctrl[0], CAT_KCE0_CTRL_ADR);
		p->mp_kce_cnt[0] = nthw_register_get_field(p->mp_kce_ctrl[0], CAT_KCE0_CTRL_CNT);
		p->mp_kce_data[0] = nthw_module_get_register(p->m_cat, CAT_KCE0_DATA);
		p->mp_kce_data_enable[0] =
			nthw_register_get_field(p->mp_kce_data[0], CAT_KCE0_DATA_ENABLE);
		/* KCS 0 */
		p->mp_kcs_ctrl[0] = nthw_module_get_register(p->m_cat, CAT_KCS0_CTRL);
		p->mp_kcs_addr[0] = nthw_register_get_field(p->mp_kcs_ctrl[0], CAT_KCS0_CTRL_ADR);
		p->mp_kcs_cnt[0] = nthw_register_get_field(p->mp_kcs_ctrl[0], CAT_KCS0_CTRL_CNT);
		p->mp_kcs_data[0] = nthw_module_get_register(p->m_cat, CAT_KCS0_DATA);
		p->mp_kcs_data_category[0] =
			nthw_register_get_field(p->mp_kcs_data[0], CAT_KCS0_DATA_CATEGORY);
		/* FTE 0 */
		p->mp_fte_ctrl[0] = nthw_module_get_register(p->m_cat, CAT_FTE0_CTRL);
		p->mp_fte_addr[0] = nthw_register_get_field(p->mp_fte_ctrl[0], CAT_FTE0_CTRL_ADR);
		p->mp_fte_cnt[0] = nthw_register_get_field(p->mp_fte_ctrl[0], CAT_FTE0_CTRL_CNT);
		p->mp_fte_data[0] = nthw_module_get_register(p->m_cat, CAT_FTE0_DATA);
		p->mp_fte_data_enable[0] =
			nthw_register_get_field(p->mp_fte_data[0], CAT_FTE0_DATA_ENABLE);
		/* KCE 1 */
		p->mp_kce_ctrl[1] = nthw_module_get_register(p->m_cat, CAT_KCE1_CTRL);
		p->mp_kce_addr[1] = nthw_register_get_field(p->mp_kce_ctrl[1], CAT_KCE1_CTRL_ADR);
		p->mp_kce_cnt[1] = nthw_register_get_field(p->mp_kce_ctrl[1], CAT_KCE1_CTRL_CNT);
		p->mp_kce_data[1] = nthw_module_get_register(p->m_cat, CAT_KCE1_DATA);
		p->mp_kce_data_enable[1] =
			nthw_register_get_field(p->mp_kce_data[1], CAT_KCE1_DATA_ENABLE);
		/* KCS 1 */
		p->mp_kcs_ctrl[1] = nthw_module_get_register(p->m_cat, CAT_KCS1_CTRL);
		p->mp_kcs_addr[1] = nthw_register_get_field(p->mp_kcs_ctrl[1], CAT_KCS1_CTRL_ADR);
		p->mp_kcs_cnt[1] = nthw_register_get_field(p->mp_kcs_ctrl[1], CAT_KCS1_CTRL_CNT);
		p->mp_kcs_data[1] = nthw_module_get_register(p->m_cat, CAT_KCS1_DATA);
		p->mp_kcs_data_category[1] =
			nthw_register_get_field(p->mp_kcs_data[1], CAT_KCS1_DATA_CATEGORY);
		/* FTE 1 */
		p->mp_fte_ctrl[1] = nthw_module_get_register(p->m_cat, CAT_FTE1_CTRL);
		p->mp_fte_addr[1] = nthw_register_get_field(p->mp_fte_ctrl[1], CAT_FTE1_CTRL_ADR);
		p->mp_fte_cnt[1] = nthw_register_get_field(p->mp_fte_ctrl[1], CAT_FTE1_CTRL_CNT);
		p->mp_fte_data[1] = nthw_module_get_register(p->m_cat, CAT_FTE1_DATA);
		p->mp_fte_data_enable[1] =
			nthw_register_get_field(p->mp_fte_data[1], CAT_FTE1_DATA_ENABLE);
	}

	/* CTE */
	p->mp_cte_ctrl = nthw_module_get_register(p->m_cat, CAT_CTE_CTRL);
	p->mp_cte_addr = nthw_register_get_field(p->mp_cte_ctrl, CAT_CTE_CTRL_ADR);
	p->mp_cte_cnt = nthw_register_get_field(p->mp_cte_ctrl, CAT_CTE_CTRL_CNT);
	p->mp_cte_data = nthw_module_get_register(p->m_cat, CAT_CTE_DATA);
	p->mp_cte_data_col = nthw_register_get_field(p->mp_cte_data, CAT_CTE_DATA_COL_ENABLE);
	p->mp_cte_data_cor = nthw_register_get_field(p->mp_cte_data, CAT_CTE_DATA_COR_ENABLE);
	p->mp_cte_data_hsh = nthw_register_get_field(p->mp_cte_data, CAT_CTE_DATA_HSH_ENABLE);
	p->mp_cte_data_qsl = nthw_register_get_field(p->mp_cte_data, CAT_CTE_DATA_QSL_ENABLE);
	p->mp_cte_data_ipf = nthw_register_get_field(p->mp_cte_data, CAT_CTE_DATA_IPF_ENABLE);
	p->mp_cte_data_slc = nthw_register_get_field(p->mp_cte_data, CAT_CTE_DATA_SLC_ENABLE);
	p->mp_cte_data_pdb = nthw_register_get_field(p->mp_cte_data, CAT_CTE_DATA_PDB_ENABLE);
	p->mp_cte_data_msk = nthw_register_query_field(p->mp_cte_data, CAT_CTE_DATA_MSK_ENABLE);
	p->mp_cte_data_hst = nthw_register_query_field(p->mp_cte_data, CAT_CTE_DATA_HST_ENABLE);
	p->mp_cte_data_epp = nthw_register_query_field(p->mp_cte_data, CAT_CTE_DATA_EPP_ENABLE);
	p->mp_cte_data_tpe = nthw_register_query_field(p->mp_cte_data, CAT_CTE_DATA_TPE_ENABLE);
	/* CTS */
	p->mp_cts_ctrl = nthw_module_get_register(p->m_cat, CAT_CTS_CTRL);
	p->mp_cts_addr = nthw_register_get_field(p->mp_cts_ctrl, CAT_CTS_CTRL_ADR);
	p->mp_cts_cnt = nthw_register_get_field(p->mp_cts_ctrl, CAT_CTS_CTRL_CNT);
	p->mp_cts_data = nthw_module_get_register(p->m_cat, CAT_CTS_DATA);
	p->mp_cts_data_cat_a = nthw_register_get_field(p->mp_cts_data, CAT_CTS_DATA_CAT_A);
	p->mp_cts_data_cat_b = nthw_register_get_field(p->mp_cts_data, CAT_CTS_DATA_CAT_B);
	/* COT */
	p->mp_cot_ctrl = nthw_module_get_register(p->m_cat, CAT_COT_CTRL);
	p->mp_cot_addr = nthw_register_get_field(p->mp_cot_ctrl, CAT_COT_CTRL_ADR);
	p->mp_cot_cnt = nthw_register_get_field(p->mp_cot_ctrl, CAT_COT_CTRL_CNT);
	p->mp_cot_data = nthw_module_get_register(p->m_cat, CAT_COT_DATA);
	p->mp_cot_data_color = nthw_register_get_field(p->mp_cot_data, CAT_COT_DATA_COLOR);
	p->mp_cot_data_km = nthw_register_get_field(p->mp_cot_data, CAT_COT_DATA_KM);
	p->mp_cot_data_nfv_sb = nthw_register_query_field(p->mp_cot_data, CAT_COT_DATA_NFV_SB);
	/* CCT */
	p->mp_cct_ctrl = nthw_module_get_register(p->m_cat, CAT_CCT_CTRL);
	p->mp_cct_addr = nthw_register_get_field(p->mp_cct_ctrl, CAT_CCT_CTRL_ADR);
	p->mp_cct_cnt = nthw_register_get_field(p->mp_cct_ctrl, CAT_CCT_CTRL_CNT);
	p->mp_cct_data = nthw_module_get_register(p->m_cat, CAT_CCT_DATA);
	p->mp_cct_data_color = nthw_register_get_field(p->mp_cct_data, CAT_CCT_DATA_COLOR);
	p->mp_cct_data_km = nthw_register_get_field(p->mp_cct_data, CAT_CCT_DATA_KM);
	/* EXO */
	p->mp_exo_ctrl = nthw_module_get_register(p->m_cat, CAT_EXO_CTRL);
	p->mp_exo_addr = nthw_register_get_field(p->mp_exo_ctrl, CAT_EXO_CTRL_ADR);
	p->mp_exo_cnt = nthw_register_get_field(p->mp_exo_ctrl, CAT_EXO_CTRL_CNT);
	p->mp_exo_data = nthw_module_get_register(p->m_cat, CAT_EXO_DATA);
	p->mp_exo_data_dyn = nthw_register_get_field(p->mp_exo_data, CAT_EXO_DATA_DYN);
	p->mp_exo_data_ofs = nthw_register_get_field(p->mp_exo_data, CAT_EXO_DATA_OFS);
	/* RCK */
	p->mp_rck_ctrl = nthw_module_get_register(p->m_cat, CAT_RCK_CTRL);
	p->mp_rck_addr = nthw_register_get_field(p->mp_rck_ctrl, CAT_RCK_CTRL_ADR);
	p->mp_rck_cnt = nthw_register_get_field(p->mp_rck_ctrl, CAT_RCK_CTRL_CNT);
	p->mp_rck_data = nthw_module_get_register(p->m_cat, CAT_RCK_DATA);
	/* LEN */
	p->mp_len_ctrl = nthw_module_get_register(p->m_cat, CAT_LEN_CTRL);
	p->mp_len_addr = nthw_register_get_field(p->mp_len_ctrl, CAT_LEN_CTRL_ADR);
	p->mp_len_cnt = nthw_register_get_field(p->mp_len_ctrl, CAT_LEN_CTRL_CNT);
	p->mp_len_data = nthw_module_get_register(p->m_cat, CAT_LEN_DATA);
	p->mp_len_data_lower = nthw_register_get_field(p->mp_len_data, CAT_LEN_DATA_LOWER);
	p->mp_len_data_upper = nthw_register_get_field(p->mp_len_data, CAT_LEN_DATA_UPPER);
	p->mp_len_data_dyn1 = nthw_register_get_field(p->mp_len_data, CAT_LEN_DATA_DYN1);
	p->mp_len_data_dyn2 = nthw_register_get_field(p->mp_len_data, CAT_LEN_DATA_DYN2);
	p->mp_len_data_inv = nthw_register_get_field(p->mp_len_data, CAT_LEN_DATA_INV);

	p->mp_cfn_data_ptc_cfp = nthw_register_query_field(p->mp_cfn_data, CAT_CFN_DATA_PTC_CFP);
	p->mp_cfn_data_err_l3_cs =
		nthw_register_query_field(p->mp_cfn_data, CAT_CFN_DATA_ERR_L3_CS);
	p->mp_cfn_data_err_l4_cs =
		nthw_register_query_field(p->mp_cfn_data, CAT_CFN_DATA_ERR_L4_CS);
	p->mp_cfn_data_err_tnl_l3_cs =
		nthw_register_query_field(p->mp_cfn_data, CAT_CFN_DATA_ERR_TNL_L3_CS);
	p->mp_cfn_data_err_tnl_l4_cs =
		nthw_register_query_field(p->mp_cfn_data, CAT_CFN_DATA_ERR_TNL_L4_CS);
	p->mp_cfn_data_err_ttl_exp =
		nthw_register_query_field(p->mp_cfn_data, CAT_CFN_DATA_ERR_TTL_EXP);
	p->mp_cfn_data_err_tnl_ttl_exp =
		nthw_register_query_field(p->mp_cfn_data, CAT_CFN_DATA_ERR_TNL_TTL_EXP);

	p->mp_kcc_ctrl = nthw_module_query_register(p->m_cat, CAT_KCC_CTRL);

	if (p->mp_kcc_ctrl != NULL) {
		p->mp_kcc_addr = nthw_register_query_field(p->mp_kcc_ctrl, CAT_KCC_CTRL_ADR);
		p->mp_kcc_cnt = nthw_register_query_field(p->mp_kcc_ctrl, CAT_KCC_CTRL_CNT);
	}

	p->mp_kcc_data = nthw_module_query_register(p->m_cat, CAT_KCC_DATA);

	if (p->mp_kcc_data != NULL) {
		p->mp_kcc_data_key = nthw_register_query_field(p->mp_kcc_data, CAT_KCC_DATA_KEY);
		p->mp_kcc_data_category =
			nthw_register_query_field(p->mp_kcc_data, CAT_KCC_DATA_CATEGORY);
		p->mp_kcc_data_id = nthw_register_query_field(p->mp_kcc_data, CAT_KCC_DATA_ID);
	}

	return 0;
}

/* CFN */
void nthw_cat_cfn_select(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_addr, val);
}

void nthw_cat_cfn_cnt(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_cnt, val);
}

void nthw_cat_cfn_enable(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_enable, val);
}

void nthw_cat_cfn_inv(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_inv, val);
}

void nthw_cat_cfn_ptc_inv(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_inv, val);
}

void nthw_cat_cfn_ptc_isl(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_isl, val);
}

void nthw_cat_cfn_ptc_mac(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_mac, val);
}

void nthw_cat_cfn_ptc_l2(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_l2, val);
}

void nthw_cat_cfn_ptc_vn_tag(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_vn_tag, val);
}

void nthw_cat_cfn_ptc_vlan(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_vlan, val);
}

void nthw_cat_cfn_ptc_mpls(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_mpls, val);
}

void nthw_cat_cfn_ptc_l3(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_l3, val);
}

void nthw_cat_cfn_ptc_frag(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_frag, val);
}

void nthw_cat_cfn_ptc_ip_prot(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_ip_prot, val);
}

void nthw_cat_cfn_ptc_l4(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_l4, val);
}

void nthw_cat_cfn_ptc_tunnel(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_tunnel, val);
}

void nthw_cat_cfn_ptc_tnl_l2(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_tnl_l2, val);
}

void nthw_cat_cfn_ptc_tnl_vlan(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_tnl_vlan, val);
}

void nthw_cat_cfn_ptc_tnl_mpls(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_tnl_mpls, val);
}

void nthw_cat_cfn_ptc_tnl_l3(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_tnl_l3, val);
}

void nthw_cat_cfn_ptc_tnl_frag(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_tnl_frag, val);
}

void nthw_cat_cfn_ptc_tnl_ip_prot(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_tnl_ip_prot, val);
}

void nthw_cat_cfn_ptc_tnl_l4(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_ptc_tnl_l4, val);
}

void nthw_cat_cfn_ptc_cfp(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cfn_data_ptc_cfp);
	nthw_field_set_val32(p->mp_cfn_data_ptc_cfp, val);
}

void nthw_cat_cfn_err_l3_cs(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cfn_data_err_l3_cs);
	nthw_field_set_val32(p->mp_cfn_data_err_l3_cs, val);
}

void nthw_cat_cfn_err_l4_cs(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cfn_data_err_l4_cs);
	nthw_field_set_val32(p->mp_cfn_data_err_l4_cs, val);
}

void nthw_cat_cfn_err_tnl_l3_cs(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cfn_data_err_tnl_l3_cs);
	nthw_field_set_val32(p->mp_cfn_data_err_tnl_l3_cs, val);
}

void nthw_cat_cfn_err_tnl_l4_cs(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cfn_data_err_tnl_l4_cs);
	nthw_field_set_val32(p->mp_cfn_data_err_tnl_l4_cs, val);
}

void nthw_cat_cfn_err_ttl_exp(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cfn_data_err_ttl_exp);
	nthw_field_set_val32(p->mp_cfn_data_err_ttl_exp, val);
}

void nthw_cat_cfn_err_tnl_ttl_exp(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cfn_data_err_tnl_ttl_exp);
	nthw_field_set_val32(p->mp_cfn_data_err_tnl_ttl_exp, val);
}

void nthw_cat_cfn_err_inv(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_err_inv, val);
}

void nthw_cat_cfn_err_cv(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_err_cv, val);
}

void nthw_cat_cfn_err_fcs(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_err_fcs, val);
}

void nthw_cat_cfn_err_trunc(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_err_trunc, val);
}

void nthw_cat_cfn_mac_port(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_mac_port, val);
}

void nthw_cat_cfn_pm_cmp(const struct cat_nthw *p, const uint32_t *val)
{
	nthw_field_set_val(p->mp_cfn_data_pm_cmp, val, p->mp_cfn_data_pm_cmp->mn_words);
}

void nthw_cat_cfn_pm_dct(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_pm_dct, val);
}

void nthw_cat_cfn_pm_ext_inv(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_pm_ext_inv, val);
}

void nthw_cat_cfn_pm_cmb(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_pm_cmb, val);
}

void nthw_cat_cfn_pm_and_inv(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_pm_and_inv, val);
}

void nthw_cat_cfn_pm_or_inv(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_pm_or_inv, val);
}

void nthw_cat_cfn_pm_inv(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_pm_inv, val);
}

void nthw_cat_cfn_lc(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_lc, val);
}

void nthw_cat_cfn_lc_inv(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_lc_inv, val);
}

void nthw_cat_cfn_km0_or(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cfn_data_km0_or, val);
}

void nthw_cat_cfn_km1_or(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cfn_data_km1_or);
	nthw_field_set_val32(p->mp_cfn_data_km1_or, val);
}

void nthw_cat_cfn_flush(const struct cat_nthw *p)
{
	nthw_register_flush(p->mp_cfn_ctrl, 1);
	nthw_register_flush(p->mp_cfn_data, 1);
}

void nthw_cat_kce_select(const struct cat_nthw *p, int index, uint32_t val)
{
	nthw_field_set_val32(p->mp_kce_addr[index], val);
}

void nthw_cat_kce_cnt(const struct cat_nthw *p, int index, uint32_t val)
{
	nthw_field_set_val32(p->mp_kce_cnt[index], val);
}

void nthw_cat_kce_enable(const struct cat_nthw *p, int index, uint32_t val)
{
	nthw_field_set_val32(p->mp_kce_data_enable[index], val);
}

void nthw_cat_kce_flush(const struct cat_nthw *p, int index)
{
	nthw_register_flush(p->mp_kce_ctrl[index], 1);
	nthw_register_flush(p->mp_kce_data[index], 1);
}

void nthw_cat_kcs_select(const struct cat_nthw *p, int index, uint32_t val)
{
	nthw_field_set_val32(p->mp_kcs_addr[index], val);
}

void nthw_cat_kcs_cnt(const struct cat_nthw *p, int index, uint32_t val)
{
	nthw_field_set_val32(p->mp_kcs_cnt[index], val);
}

void nthw_cat_kcs_category(const struct cat_nthw *p, int index, uint32_t val)
{
	nthw_field_set_val32(p->mp_kcs_data_category[index], val);
}

void nthw_cat_kcs_flush(const struct cat_nthw *p, int index)
{
	nthw_register_flush(p->mp_kcs_ctrl[index], 1);
	nthw_register_flush(p->mp_kcs_data[index], 1);
}

void nthw_cat_fte_select(const struct cat_nthw *p, int index, uint32_t val)
{
	nthw_field_set_val32(p->mp_fte_addr[index], val);
}

void nthw_cat_fte_cnt(const struct cat_nthw *p, int index, uint32_t val)
{
	nthw_field_set_val32(p->mp_fte_cnt[index], val);
}

void nthw_cat_fte_enable(const struct cat_nthw *p, int index, uint32_t val)
{
	nthw_field_set_val32(p->mp_fte_data_enable[index], val);
}

void nthw_cat_fte_flush(const struct cat_nthw *p, int index)
{
	nthw_register_flush(p->mp_fte_ctrl[index], 1);
	nthw_register_flush(p->mp_fte_data[index], 1);
}

void nthw_cat_cte_select(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cte_addr, val);
}

void nthw_cat_cte_cnt(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cte_cnt, val);
}

void nthw_cat_cte_enable_col(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cte_data_col, val);
}

void nthw_cat_cte_enable_cor(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cte_data_cor, val);
}

void nthw_cat_cte_enable_hsh(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cte_data_hsh, val);
}

void nthw_cat_cte_enable_qsl(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cte_data_qsl, val);
}

void nthw_cat_cte_enable_ipf(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cte_data_ipf, val);
}

void nthw_cat_cte_enable_slc(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cte_data_slc, val);
}

void nthw_cat_cte_enable_pdb(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cte_data_pdb, val);
}

void nthw_cat_cte_enable_msk(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cte_data_msk);
	nthw_field_set_val32(p->mp_cte_data_msk, val);
}

void nthw_cat_cte_enable_hst(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cte_data_hst);
	nthw_field_set_val32(p->mp_cte_data_hst, val);
}

void nthw_cat_cte_enable_epp(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cte_data_epp);
	nthw_field_set_val32(p->mp_cte_data_epp, val);
}

void nthw_cat_cte_enable_tpe(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_cte_data_tpe);
	nthw_field_set_val32(p->mp_cte_data_tpe, val);
}

void nthw_cat_cte_flush(const struct cat_nthw *p)
{
	nthw_register_flush(p->mp_cte_ctrl, 1);
	nthw_register_flush(p->mp_cte_data, 1);
}

void nthw_cat_cts_select(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cts_addr, val);
}

void nthw_cat_cts_cnt(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cts_cnt, val);
}

void nthw_cat_cts_cat_a(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cts_data_cat_a, val);
}

void nthw_cat_cts_cat_b(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cts_data_cat_b, val);
}

void nthw_cat_cts_flush(const struct cat_nthw *p)
{
	nthw_register_flush(p->mp_cts_ctrl, 1);
	nthw_register_flush(p->mp_cts_data, 1);
}

void nthw_cat_cot_select(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cot_addr, val);
}

void nthw_cat_cot_cnt(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cot_cnt, val);
}

void nthw_cat_cot_color(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cot_data_color, val);
}

void nthw_cat_cot_km(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cot_data_km, val);
}

void nthw_cat_cot_flush(const struct cat_nthw *p)
{
	nthw_register_flush(p->mp_cot_ctrl, 1);
	nthw_register_flush(p->mp_cot_data, 1);
}

void nthw_cat_cct_select(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cct_addr, val);
}

void nthw_cat_cct_cnt(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cct_cnt, val);
}

void nthw_cat_cct_color(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cct_data_color, val);
}

void nthw_cat_cct_km(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cct_data_km, val);
}

void nthw_cat_cct_flush(const struct cat_nthw *p)
{
	nthw_register_flush(p->mp_cct_ctrl, 1);
	nthw_register_flush(p->mp_cct_data, 1);
}

void nthw_cat_exo_select(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_exo_addr, val);
}

void nthw_cat_exo_cnt(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_exo_cnt, val);
}

void nthw_cat_exo_dyn(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_exo_data_dyn, val);
}

void nthw_cat_exo_ofs(const struct cat_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_exo_data_ofs, val);
}

void nthw_cat_exo_flush(const struct cat_nthw *p)
{
	nthw_register_flush(p->mp_exo_ctrl, 1);
	nthw_register_flush(p->mp_exo_data, 1);
}

void nthw_cat_rck_select(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rck_addr, val);
}

void nthw_cat_rck_cnt(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rck_cnt, val);
}

void nthw_cat_rck_data(const struct cat_nthw *p, uint32_t val)
{
	nthw_register_set_val(p->mp_rck_data, &val, 1);
	nthw_register_make_dirty(p->mp_rck_data);
}

void nthw_cat_rck_flush(const struct cat_nthw *p)
{
	nthw_register_flush(p->mp_rck_ctrl, 1);
	nthw_register_flush(p->mp_rck_data, 1);
}

void nthw_cat_len_select(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_len_addr, val);
}

void nthw_cat_len_cnt(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_len_cnt, val);
}

void nthw_cat_len_lower(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_len_data_lower, val);
}

void nthw_cat_len_upper(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_len_data_upper, val);
}

void nthw_cat_len_dyn1(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_len_data_dyn1, val);
}

void nthw_cat_len_dyn2(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_len_data_dyn2, val);
}

void nthw_cat_len_inv(const struct cat_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_len_data_inv, val);
}

void nthw_cat_len_flush(const struct cat_nthw *p)
{
	nthw_register_flush(p->mp_len_ctrl, 1);
	nthw_register_flush(p->mp_len_data, 1);
}

void nthw_cat_kcc_select(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_kcc_addr);
	nthw_field_set_val32(p->mp_kcc_addr, val);
}

void nthw_cat_kcc_cnt(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_kcc_cnt);
	nthw_field_set_val32(p->mp_kcc_cnt, val);
}

void nthw_cat_kcc_key(const struct cat_nthw *p, uint32_t *val)
{
	RTE_ASSERT(p->mp_kcc_data_key);
	nthw_field_set_val(p->mp_kcc_data_key, val, 2);
}

void nthw_cat_kcc_category(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_kcc_data_category);
	nthw_field_set_val32(p->mp_kcc_data_category, val);
}

void nthw_cat_kcc_id(const struct cat_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_kcc_data_id);
	nthw_field_set_val32(p->mp_kcc_data_id, val);
}

void nthw_cat_kcc_flush(const struct cat_nthw *p)
{
	RTE_ASSERT(p->mp_kcc_ctrl);
	RTE_ASSERT(p->mp_kcc_data);
	nthw_register_flush(p->mp_kcc_ctrl, 1);
	nthw_register_flush(p->mp_kcc_data, 1);
}
