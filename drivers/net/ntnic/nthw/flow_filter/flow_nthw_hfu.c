/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "ntlog.h"
#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_hfu.h"

void nthw_hfu_set_debug_mode(struct hfu_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_hfu, n_debug_mode);
}

struct hfu_nthw *nthw_hfu_new(void)
{
	struct hfu_nthw *p = malloc(sizeof(struct hfu_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_hfu_delete(struct hfu_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

int nthw_hfu_init(struct hfu_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_HFU, n_instance);

	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: Hfu %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_hfu = nthw_fpga_query_module(p_fpga, MOD_HFU, n_instance);

	p->mp_rcp_ctrl = nthw_module_get_register(p->m_hfu, HFU_RCP_CTRL);
	p->mp_rcp_addr = nthw_register_get_field(p->mp_rcp_ctrl, HFU_RCP_CTRL_ADR);
	p->mp_rcp_cnt = nthw_register_get_field(p->mp_rcp_ctrl, HFU_RCP_CTRL_CNT);

	p->mp_rcp_data = nthw_module_get_register(p->m_hfu, HFU_RCP_DATA);
	p->mp_rcp_data_len_a_wr = nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_A_WR);
	p->mp_rcp_data_len_a_ol4len =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_A_OL4LEN);
	p->mp_rcp_data_len_a_pos_dyn =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_A_POS_DYN);
	p->mp_rcp_data_len_a_pos_ofs =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_A_POS_OFS);
	p->mp_rcp_data_len_a_add_dyn =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_A_ADD_DYN);
	p->mp_rcp_data_len_a_add_ofs =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_A_ADD_OFS);
	p->mp_rcp_data_len_a_sub_dyn =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_A_SUB_DYN);
	p->mp_rcp_data_len_b_wr = nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_B_WR);
	p->mp_rcp_data_len_b_pos_dyn =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_B_POS_DYN);
	p->mp_rcp_data_len_b_pos_ofs =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_B_POS_OFS);
	p->mp_rcp_data_len_b_add_dyn =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_B_ADD_DYN);
	p->mp_rcp_data_len_b_add_ofs =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_B_ADD_OFS);
	p->mp_rcp_data_len_b_sub_dyn =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_B_SUB_DYN);
	p->mp_rcp_data_len_c_wr = nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_C_WR);
	p->mp_rcp_data_len_c_pos_dyn =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_C_POS_DYN);
	p->mp_rcp_data_len_c_pos_ofs =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_C_POS_OFS);
	p->mp_rcp_data_len_c_add_dyn =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_C_ADD_DYN);
	p->mp_rcp_data_len_c_add_ofs =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_C_ADD_OFS);
	p->mp_rcp_data_len_c_sub_dyn =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_LEN_C_SUB_DYN);
	p->mp_rcp_data_ttl_wr = nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_TTL_WR);
	p->mp_rcp_data_ttl_pos_dyn =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_TTL_POS_DYN);
	p->mp_rcp_data_ttl_pos_ofs =
		nthw_register_get_field(p->mp_rcp_data, HFU_RCP_DATA_TTL_POS_OFS);

	return 0;
}

void nthw_hfu_rcp_select(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_addr, val);
}

void nthw_hfu_rcp_cnt(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_cnt, val);
}

void nthw_hfu_rcp_len_a_wr(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_a_wr, val);
}

void nthw_hfu_rcp_len_a_ol4len(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_a_ol4len, val);
}

void nthw_hfu_rcp_len_a_pos_dyn(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_a_pos_dyn, val);
}

void nthw_hfu_rcp_len_a_pos_ofs(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_a_pos_ofs, val);
}

void nthw_hfu_rcp_len_a_add_dyn(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_a_add_dyn, val);
}

void nthw_hfu_rcp_len_a_add_ofs(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_a_add_ofs, val);
}

void nthw_hfu_rcp_len_a_sub_dyn(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_a_sub_dyn, val);
}

void nthw_hfu_rcp_len_b_wr(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_b_wr, val);
}

void nthw_hfu_rcp_len_b_pos_dyn(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_b_pos_dyn, val);
}

void nthw_hfu_rcp_len_b_pos_ofs(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_b_pos_ofs, val);
}

void nthw_hfu_rcp_len_b_add_dyn(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_b_add_dyn, val);
}

void nthw_hfu_rcp_len_b_add_ofs(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_b_add_ofs, val);
}

void nthw_hfu_rcp_len_b_sub_dyn(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_b_sub_dyn, val);
}

void nthw_hfu_rcp_len_c_wr(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_c_wr, val);
}

void nthw_hfu_rcp_len_c_pos_dyn(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_c_pos_dyn, val);
}

void nthw_hfu_rcp_len_c_pos_ofs(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_c_pos_ofs, val);
}

void nthw_hfu_rcp_len_c_add_dyn(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_c_add_dyn, val);
}

void nthw_hfu_rcp_len_c_add_ofs(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_c_add_ofs, val);
}

void nthw_hfu_rcp_len_c_sub_dyn(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len_c_sub_dyn, val);
}

void nthw_hfu_rcp_ttl_wr(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ttl_wr, val);
}

void nthw_hfu_rcp_ttl_pos_dyn(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ttl_pos_dyn, val);
}

void nthw_hfu_rcp_ttl_pos_ofs(const struct hfu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ttl_pos_ofs, val);
}

void nthw_hfu_rcp_flush(const struct hfu_nthw *p)
{
	nthw_register_flush(p->mp_rcp_ctrl, 1);
	nthw_register_flush(p->mp_rcp_data, 1);
}
