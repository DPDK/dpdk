/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_hsh.h"

void nthw_hsh_set_debug_mode(struct hsh_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_hsh, n_debug_mode);
}

struct hsh_nthw *nthw_hsh_new(void)
{
	struct hsh_nthw *p = malloc(sizeof(struct hsh_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_hsh_delete(struct hsh_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

int nthw_hsh_init(struct hsh_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_HSH, n_instance);
	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: Hsh %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_hsh = p_mod;

	/* RCP */
	p->mp_rcp_ctrl = nthw_module_get_register(p->m_hsh, HSH_RCP_CTRL);
	p->mp_rcp_addr = nthw_register_get_field(p->mp_rcp_ctrl, HSH_RCP_CTRL_ADR);
	p->mp_rcp_cnt = nthw_register_get_field(p->mp_rcp_ctrl, HSH_RCP_CTRL_CNT);
	p->mp_rcp_data = nthw_module_get_register(p->m_hsh, HSH_RCP_DATA);
	p->mp_rcp_data_load_dist_type =
		nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_LOAD_DIST_TYPE);
	p->mp_rcp_data_mac_port_mask =
		nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_MAC_PORT_MASK);
	p->mp_rcp_data_sort = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_SORT);
	p->mp_rcp_data_qw0_pe = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_QW0_PE);
	p->mp_rcp_data_qw0_ofs = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_QW0_OFS);
	p->mp_rcp_data_qw4_pe = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_QW4_PE);
	p->mp_rcp_data_qw4_ofs = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_QW4_OFS);
	p->mp_rcp_data_w8_pe = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_W8_PE);
	p->mp_rcp_data_w8_ofs = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_W8_OFS);
	p->mp_rcp_data_w8_sort = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_W8_SORT);
	p->mp_rcp_data_w9_pe = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_W9_PE);
	p->mp_rcp_data_w9_ofs = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_W9_OFS);
	p->mp_rcp_data_w9_sort = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_W9_SORT);
	p->mp_rcp_data_w9_p = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_W9_P);
	p->mp_rcp_data_p_mask = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_P_MASK);
	p->mp_rcp_data_word_mask = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_WORD_MASK);
	p->mp_rcp_data_seed = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_SEED);
	p->mp_rcp_data_tnl_p = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_TNL_P);
	p->mp_rcp_data_hsh_valid = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_HSH_VALID);
	p->mp_rcp_data_hsh_type = nthw_register_get_field(p->mp_rcp_data, HSH_RCP_DATA_HSH_TYPE);
	p->mp_rcp_data_toeplitz = nthw_register_query_field(p->mp_rcp_data, HSH_RCP_DATA_TOEPLITZ);
	p->mp_rcp_data_k = nthw_register_query_field(p->mp_rcp_data, HSH_RCP_DATA_K);
	p->mp_rcp_data_auto_ipv4_mask =
		nthw_register_query_field(p->mp_rcp_data, HSH_RCP_DATA_AUTO_IPV4_MASK);

	/* Init */
	uint32_t val[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	nthw_field_set_val32(p->mp_rcp_addr, 0);
	nthw_field_set_val32(p->mp_rcp_cnt, 1);

	nthw_field_set_val32(p->mp_rcp_data_load_dist_type, 0);
	nthw_field_set_val(p->mp_rcp_data_mac_port_mask, val,
		p->mp_rcp_data_mac_port_mask->mn_words);
	nthw_field_set_val32(p->mp_rcp_data_sort, 0);
	nthw_field_set_val32(p->mp_rcp_data_qw0_pe, 0);
	nthw_field_set_val32(p->mp_rcp_data_qw0_ofs, 0);
	nthw_field_set_val32(p->mp_rcp_data_qw4_pe, 0);
	nthw_field_set_val32(p->mp_rcp_data_qw4_ofs, 0);
	nthw_field_set_val32(p->mp_rcp_data_w8_pe, 0);
	nthw_field_set_val32(p->mp_rcp_data_w8_ofs, 0);
	nthw_field_set_val32(p->mp_rcp_data_w8_sort, 0);
	nthw_field_set_val32(p->mp_rcp_data_w9_pe, 0);
	nthw_field_set_val32(p->mp_rcp_data_w9_ofs, 0);
	nthw_field_set_val32(p->mp_rcp_data_w9_sort, 0);
	nthw_field_set_val32(p->mp_rcp_data_w9_p, 0);
	nthw_field_set_val(p->mp_rcp_data_word_mask, val, 10);
	nthw_field_set_val32(p->mp_rcp_data_seed, 0);
	nthw_field_set_val32(p->mp_rcp_data_tnl_p, 0);
	nthw_field_set_val32(p->mp_rcp_data_hsh_valid, 0);
	nthw_field_set_val32(p->mp_rcp_data_hsh_type, 31);

	if (p->mp_rcp_data_toeplitz)
		nthw_field_set_val32(p->mp_rcp_data_toeplitz, 0);

	if (p->mp_rcp_data_k)
		nthw_field_set_val(p->mp_rcp_data_k, val, 10);

	nthw_register_flush(p->mp_rcp_ctrl, 1);
	nthw_register_flush(p->mp_rcp_data, 1);

	return 0;
}

void nthw_hsh_rcp_select(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_addr, val);
}

void nthw_hsh_rcp_cnt(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_cnt, val);
}

void nthw_hsh_rcp_load_dist_type(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_load_dist_type, val);
}

void nthw_hsh_rcp_mac_port_mask(const struct hsh_nthw *p, uint32_t *val)
{
	nthw_field_set_val(p->mp_rcp_data_mac_port_mask, val,
		p->mp_rcp_data_mac_port_mask->mn_words);
}

void nthw_hsh_rcp_sort(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_sort, val);
}

void nthw_hsh_rcp_qw0_pe(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw0_pe, val);
}

void nthw_hsh_rcp_qw0_ofs(const struct hsh_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw0_ofs, val);
}

void nthw_hsh_rcp_qw4_pe(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw4_pe, val);
}

void nthw_hsh_rcp_qw4_ofs(const struct hsh_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw4_ofs, val);
}

void nthw_hsh_rcp_w8_pe(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_w8_pe, val);
}

void nthw_hsh_rcp_w8_ofs(const struct hsh_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_w8_ofs, val);
}

void nthw_hsh_rcp_w8_sort(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_w8_sort, val);
}

void nthw_hsh_rcp_w9_pe(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_w9_pe, val);
}

void nthw_hsh_rcp_w9_ofs(const struct hsh_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_w9_ofs, val);
}

void nthw_hsh_rcp_w9_sort(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_w9_sort, val);
}

void nthw_hsh_rcp_w9_p(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_w9_p, val);
}

void nthw_hsh_rcp_p_mask(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_p_mask, val);
}

void nthw_hsh_rcp_word_mask(const struct hsh_nthw *p, uint32_t *val)
{
	nthw_field_set_val(p->mp_rcp_data_word_mask, val, 10);
}

void nthw_hsh_rcp_seed(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_seed, val);
}

void nthw_hsh_rcp_tnl_p(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_tnl_p, val);
}

void nthw_hsh_rcp_hsh_valid(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_hsh_valid, val);
}

void nthw_hsh_rcp_hsh_type(const struct hsh_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_hsh_type, val);
}

void nthw_hsh_rcp_toeplitz(const struct hsh_nthw *p, uint32_t val)
{
	if (p->mp_rcp_data_toeplitz)
		nthw_field_set_val32(p->mp_rcp_data_toeplitz, val);
}

void nthw_hsh_rcp_k(const struct hsh_nthw *p, uint32_t *val)
{
	if (p->mp_rcp_data_k)
		nthw_field_set_val(p->mp_rcp_data_k, val, 10);
}

void nthw_hsh_rcp_auto_ipv4_mask(const struct hsh_nthw *p, uint32_t val)
{
	if (p->mp_rcp_data_auto_ipv4_mask)
		nthw_field_set_val32(p->mp_rcp_data_auto_ipv4_mask, val);
}

void nthw_hsh_rcp_flush(const struct hsh_nthw *p)
{
	nthw_register_flush(p->mp_rcp_ctrl, 1);
	nthw_register_flush(p->mp_rcp_data, 1);
}
