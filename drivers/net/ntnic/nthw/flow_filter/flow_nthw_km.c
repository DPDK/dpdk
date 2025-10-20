/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_km.h"

#define CHECK_AND_SET_VALUE(a, val)                                                               \
	do {                                                                                      \
		nthw_field_t *temp_a = (a);                                                       \
		if (temp_a) {                                                                     \
			nthw_field_set_val32(temp_a, val);                                        \
		}                                                                                 \
	} while (0)

void nthw_km_set_debug_mode(struct km_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_km, n_debug_mode);
}

struct km_nthw *nthw_km_new(void)
{
	struct km_nthw *p = malloc(sizeof(struct km_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_km_delete(struct km_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

int nthw_km_init(struct km_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_KM, n_instance);

	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: Km %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_km = p_mod;

	/* RCP */
	p->mp_rcp_ctrl = nthw_module_get_register(p->m_km, KM_RCP_CTRL);
	p->mp_rcp_addr = nthw_register_get_field(p->mp_rcp_ctrl, KM_RCP_CTRL_ADR);
	p->mp_rcp_cnt = nthw_register_get_field(p->mp_rcp_ctrl, KM_RCP_CTRL_CNT);
	p->mp_rcp_data = nthw_module_get_register(p->m_km, KM_RCP_DATA);
	p->mp_rcp_data_qw0_dyn = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_QW0_DYN);
	p->mp_rcp_data_qw0_ofs = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_QW0_OFS);
	p->mp_rcp_data_qw0_sel_a = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_QW0_SEL_A);
	p->mp_rcp_data_qw0_sel_b = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_QW0_SEL_B);
	p->mp_rcp_data_qw4_dyn = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_QW4_DYN);
	p->mp_rcp_data_qw4_ofs = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_QW4_OFS);
	p->mp_rcp_data_qw4_sel_a = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_QW4_SEL_A);
	p->mp_rcp_data_qw4_sel_b = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_QW4_SEL_B);

	p->mp_rcp_data_sw8_dyn = nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW8_DYN);
	p->mp_rcp_data_dw8_dyn = nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW8_DYN);

	p->mp_rcp_data_swx_ovs_sb =
		nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SWX_OVS_SB);
	p->mp_rcp_data_swx_cch = nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SWX_CCH);
	p->mp_rcp_data_swx_sel_a = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_SWX_SEL_A);
	p->mp_rcp_data_swx_sel_b = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_SWX_SEL_B);
	p->mp_rcp_data_mask_a = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_MASK_A);
	p->mp_rcp_data_mask_b = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_MASK_B);
	p->mp_rcp_data_dual = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_DUAL);
	p->mp_rcp_data_paired = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_PAIRED);
	p->mp_rcp_data_el_a = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_EL_A);
	p->mp_rcp_data_el_b = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_EL_B);
	p->mp_rcp_data_info_a = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_INFO_A);
	p->mp_rcp_data_info_b = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_INFO_B);
	p->mp_rcp_data_ftm_a = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_FTM_A);
	p->mp_rcp_data_ftm_b = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_FTM_B);
	p->mp_rcp_data_bank_a = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_BANK_A);
	p->mp_rcp_data_bank_b = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_BANK_B);
	p->mp_rcp_data_kl_a = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_KL_A);
	p->mp_rcp_data_kl_b = nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_KL_B);
	p->mp_rcp_data_flow_set = nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_FLOW_SET);
	p->mp_rcp_data_keyway_a = nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_KEYWAY_A);
	p->mp_rcp_data_keyway_b = nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_KEYWAY_B);
	p->mp_rcp_data_synergy_mode =
		nthw_register_get_field(p->mp_rcp_data, KM_RCP_DATA_SYNERGY_MODE);

	/* CAM */
	p->mp_cam_ctrl = nthw_module_get_register(p->m_km, KM_CAM_CTRL);
	p->mp_cam_addr = nthw_register_get_field(p->mp_cam_ctrl, KM_CAM_CTRL_ADR);
	p->mp_cam_cnt = nthw_register_get_field(p->mp_cam_ctrl, KM_CAM_CTRL_CNT);
	p->mp_cam_data = nthw_module_get_register(p->m_km, KM_CAM_DATA);
	p->mp_cam_data_w0 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_W0);
	p->mp_cam_data_w1 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_W1);
	p->mp_cam_data_w2 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_W2);
	p->mp_cam_data_w3 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_W3);
	p->mp_cam_data_w4 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_W4);
	p->mp_cam_data_w5 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_W5);
	p->mp_cam_data_ft0 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_FT0);
	p->mp_cam_data_ft1 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_FT1);
	p->mp_cam_data_ft2 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_FT2);
	p->mp_cam_data_ft3 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_FT3);
	p->mp_cam_data_ft4 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_FT4);
	p->mp_cam_data_ft5 = nthw_register_get_field(p->mp_cam_data, KM_CAM_DATA_FT5);
	/* TCAM */
	p->mp_tcam_ctrl = nthw_module_get_register(p->m_km, KM_TCAM_CTRL);
	p->mp_tcam_addr = nthw_register_get_field(p->mp_tcam_ctrl, KM_TCAM_CTRL_ADR);
	p->mp_tcam_cnt = nthw_register_get_field(p->mp_tcam_ctrl, KM_TCAM_CTRL_CNT);
	p->mp_tcam_data = nthw_module_get_register(p->m_km, KM_TCAM_DATA);
	p->mp_tcam_data_t = nthw_register_get_field(p->mp_tcam_data, KM_TCAM_DATA_T);
	/* TCI */
	p->mp_tci_ctrl = nthw_module_get_register(p->m_km, KM_TCI_CTRL);
	p->mp_tci_addr = nthw_register_get_field(p->mp_tci_ctrl, KM_TCI_CTRL_ADR);
	p->mp_tci_cnt = nthw_register_get_field(p->mp_tci_ctrl, KM_TCI_CTRL_CNT);
	p->mp_tci_data = nthw_module_get_register(p->m_km, KM_TCI_DATA);
	p->mp_tci_data_color = nthw_register_get_field(p->mp_tci_data, KM_TCI_DATA_COLOR);
	p->mp_tci_data_ft = nthw_register_get_field(p->mp_tci_data, KM_TCI_DATA_FT);
	/* TCQ */
	p->mp_tcq_ctrl = nthw_module_get_register(p->m_km, KM_TCQ_CTRL);
	p->mp_tcq_addr = nthw_register_get_field(p->mp_tcq_ctrl, KM_TCQ_CTRL_ADR);
	p->mp_tcq_cnt = nthw_register_get_field(p->mp_tcq_ctrl, KM_TCQ_CTRL_CNT);
	p->mp_tcq_data = nthw_module_get_register(p->m_km, KM_TCQ_DATA);
	p->mp_tcq_data_bank_mask =
		nthw_register_query_field(p->mp_tcq_data, KM_TCQ_DATA_BANK_MASK);
	p->mp_tcq_data_qual = nthw_register_get_field(p->mp_tcq_data, KM_TCQ_DATA_QUAL);

	p->mp_rcp_data_dw0_b_dyn =
		nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW0_B_DYN);
	p->mp_rcp_data_dw0_b_ofs =
		nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW0_B_OFS);
	p->mp_rcp_data_dw2_b_dyn =
		nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW2_B_DYN);
	p->mp_rcp_data_dw2_b_ofs =
		nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW2_B_OFS);
	p->mp_rcp_data_sw4_b_dyn =
		nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW4_B_DYN);
	p->mp_rcp_data_sw4_b_ofs =
		nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW4_B_OFS);
	p->mp_rcp_data_sw5_b_dyn =
		nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW5_B_DYN);
	p->mp_rcp_data_sw5_b_ofs =
		nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW5_B_OFS);

	if (!p->mp_rcp_data_dw0_b_dyn) {
		/* old field defines */
		p->mp_rcp_data_dw0_b_dyn =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_QW0_B_DYN);
		p->mp_rcp_data_dw0_b_ofs =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_QW0_B_OFS);
		p->mp_rcp_data_dw2_b_dyn =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_QW4_B_DYN);
		p->mp_rcp_data_dw2_b_ofs =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_QW4_B_OFS);
		p->mp_rcp_data_sw4_b_dyn =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW8_B_DYN);
		p->mp_rcp_data_sw4_b_ofs =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW8_B_OFS);
		p->mp_rcp_data_sw5_b_dyn =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW9_B_DYN);
		p->mp_rcp_data_sw5_b_ofs =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW9_B_OFS);
	}

	/* v0.6+ */
	if (p->mp_rcp_data_dw8_dyn) {
		p->mp_rcp_data_dw8_ofs =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW8_OFS);
		p->mp_rcp_data_dw8_sel_a =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW8_SEL_A);
		p->mp_rcp_data_dw8_sel_b =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW8_SEL_B);
		p->mp_rcp_data_dw10_dyn =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW10_DYN);
		p->mp_rcp_data_dw10_ofs =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW10_OFS);
		p->mp_rcp_data_dw10_sel_a =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW10_SEL_A);
		p->mp_rcp_data_dw10_sel_b =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_DW10_SEL_B);

	} else if (p->mp_rcp_data_sw8_dyn) {
		p->mp_rcp_data_sw8_ofs =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW8_OFS);
		p->mp_rcp_data_sw8_sel_a =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW8_SEL_A);
		p->mp_rcp_data_sw8_sel_b =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW8_SEL_B);
		p->mp_rcp_data_sw9_dyn =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW9_DYN);
		p->mp_rcp_data_sw9_ofs =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW9_OFS);
		p->mp_rcp_data_sw9_sel_a =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW9_SEL_A);
		p->mp_rcp_data_sw9_sel_b =
			nthw_register_query_field(p->mp_rcp_data, KM_RCP_DATA_SW9_SEL_B);
	}

	return 0;
}

/* RCP */
void nthw_km_rcp_select(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_addr, val);
};

void nthw_km_rcp_cnt(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_cnt, val);
};

void nthw_km_rcp_qw0_dyn(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw0_dyn, val);
};

void nthw_km_rcp_qw0_ofs(const struct km_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw0_ofs, val);
};

void nthw_km_rcp_qw0_sel_a(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw0_sel_a, val);
};

void nthw_km_rcp_qw0_sel_b(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw0_sel_b, val);
};

void nthw_km_rcp_qw4_dyn(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw4_dyn, val);
};

void nthw_km_rcp_qw4_ofs(const struct km_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw4_ofs, val);
};

void nthw_km_rcp_qw4_sel_a(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw4_sel_a, val);
};

void nthw_km_rcp_qw4_sel_b(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw4_sel_b, val);
};

void nthw_km_rcp_dw8_dyn(const struct km_nthw *p, uint32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_rcp_data_dw8_dyn, val);
};

void nthw_km_rcp_swx_cch(const struct km_nthw *p, uint32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_rcp_data_swx_cch, val);
};

void nthw_km_rcp_dw8_ofs(const struct km_nthw *p, int32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_rcp_data_dw8_ofs, val);
};

void nthw_km_rcp_dw8_sel_a(const struct km_nthw *p, uint32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_rcp_data_dw8_sel_a, val);
};

void nthw_km_rcp_dw8_sel_b(const struct km_nthw *p, uint32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_rcp_data_dw8_sel_b, val);
};

void nthw_km_rcp_dw10_dyn(const struct km_nthw *p, uint32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_rcp_data_dw10_dyn, val);
};

void nthw_km_rcp_dw10_ofs(const struct km_nthw *p, int32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_rcp_data_dw10_ofs, val);
};

void nthw_km_rcp_dw10_sel_a(const struct km_nthw *p, uint32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_rcp_data_dw10_sel_a, val);
};

void nthw_km_rcp_dw10_sel_b(const struct km_nthw *p, uint32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_rcp_data_dw10_sel_b, val);
};

void nthw_km_rcp_swx_sel_a(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_swx_sel_a, val);
};

void nthw_km_rcp_swx_sel_b(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_swx_sel_b, val);
};

void nthw_km_rcp_mask_b(const struct km_nthw *p, const uint32_t *val)
{
	nthw_field_set_val(p->mp_rcp_data_mask_b, val, p->mp_rcp_data_mask_b->mn_words);
};

void nthw_km_rcp_mask_da(const struct km_nthw *p, const uint32_t *val)
{
	nthw_field_set_val(p->mp_rcp_data_mask_a, val, p->mp_rcp_data_mask_a->mn_words);
};	/* for DW8/DW10 from v6+ */

void nthw_km_rcp_dual(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_dual, val);
};

void nthw_km_rcp_paired(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_paired, val);
};

void nthw_km_rcp_el_a(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_el_a, val);
};

void nthw_km_rcp_el_b(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_el_b, val);
};

void nthw_km_rcp_info_a(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_info_a, val);
};

void nthw_km_rcp_info_b(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_info_b, val);
};

void nthw_km_rcp_ftm_a(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ftm_a, val);
};

void nthw_km_rcp_ftm_b(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ftm_b, val);
};

void nthw_km_rcp_bank_a(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_bank_a, val);
};

void nthw_km_rcp_bank_b(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_bank_b, val);
};

void nthw_km_rcp_kl_a(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_kl_a, val);
};

void nthw_km_rcp_kl_b(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_kl_b, val);
};

void nthw_km_rcp_keyway_a(const struct km_nthw *p, uint32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_rcp_data_keyway_a, val);
};

void nthw_km_rcp_keyway_b(const struct km_nthw *p, uint32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_rcp_data_keyway_b, val);
};

void nthw_km_rcp_synergy_mode(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_synergy_mode, val);
};

void nthw_km_rcp_dw0_b_dyn(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_dw0_b_dyn, val);
};

void nthw_km_rcp_dw0_b_ofs(const struct km_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_dw0_b_ofs, val);
};

void nthw_km_rcp_dw2_b_dyn(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_dw2_b_dyn, val);
};

void nthw_km_rcp_dw2_b_ofs(const struct km_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_dw2_b_ofs, val);
};

void nthw_km_rcp_sw4_b_dyn(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_sw4_b_dyn, val);
};

void nthw_km_rcp_sw4_b_ofs(const struct km_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_sw4_b_ofs, val);
};

void nthw_km_rcp_sw5_b_dyn(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_sw5_b_dyn, val);
};

void nthw_km_rcp_sw5_b_ofs(const struct km_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_sw5_b_ofs, val);
};

void nthw_km_rcp_flush(const struct km_nthw *p)
{
	nthw_register_flush(p->mp_rcp_ctrl, 1);
	nthw_register_flush(p->mp_rcp_data, 1);
};

/* CAM */
void nthw_km_cam_select(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_addr, val);
};

void nthw_km_cam_cnt(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_cnt, val);
};

void nthw_km_cam_w0(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_w0, val);
};

void nthw_km_cam_w1(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_w1, val);
};

void nthw_km_cam_w2(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_w2, val);
};

void nthw_km_cam_w3(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_w3, val);
};

void nthw_km_cam_w4(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_w4, val);
};

void nthw_km_cam_w5(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_w5, val);
};

void nthw_km_cam_ft0(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_ft0, val);
};

void nthw_km_cam_ft1(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_ft1, val);
};

void nthw_km_cam_ft2(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_ft2, val);
};

void nthw_km_cam_ft3(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_ft3, val);
};

void nthw_km_cam_ft4(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_ft4, val);
};

void nthw_km_cam_ft5(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_cam_data_ft5, val);
};

void nthw_km_cam_flush(const struct km_nthw *p)
{
	nthw_register_flush(p->mp_cam_ctrl, 1);
	nthw_register_flush(p->mp_cam_data, 1);
};

/* TCAM */
void nthw_km_tcam_select(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_tcam_addr, val);
};

void nthw_km_tcam_cnt(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_tcam_cnt, val);
};

void nthw_km_tcam_t(const struct km_nthw *p, uint32_t *val)
{
	nthw_field_set_val(p->mp_tcam_data_t, val, 3);
};

void nthw_km_tcam_flush(const struct km_nthw *p)
{
	nthw_register_flush(p->mp_tcam_ctrl, 1);
	nthw_register_flush(p->mp_tcam_data, 1);
};

/* TCI */
void nthw_km_tci_select(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_tci_addr, val);
};

void nthw_km_tci_cnt(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_tci_cnt, val);
};

void nthw_km_tci_color(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_tci_data_color, val);
};

void nthw_km_tci_ft(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_tci_data_ft, val);
};

void nthw_km_tci_flush(const struct km_nthw *p)
{
	nthw_register_flush(p->mp_tci_ctrl, 1);
	nthw_register_flush(p->mp_tci_data, 1);
};

/* TCQ */
void nthw_km_tcq_select(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_tcq_addr, val);
};

void nthw_km_tcq_cnt(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_tcq_cnt, val);
};

void nthw_km_tcq_bank_mask(const struct km_nthw *p, uint32_t val)
{
	CHECK_AND_SET_VALUE(p->mp_tcq_data_bank_mask, val);
};

void nthw_km_tcq_qual(const struct km_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_tcq_data_qual, val);
};

void nthw_km_tcq_flush(const struct km_nthw *p)
{
	nthw_register_flush(p->mp_tcq_ctrl, 1);
	nthw_register_flush(p->mp_tcq_data, 1);
};
