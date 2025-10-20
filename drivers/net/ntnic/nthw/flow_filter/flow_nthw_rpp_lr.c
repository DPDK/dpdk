/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "ntlog.h"
#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_rpp_lr.h"

void nthw_rpp_lr_set_debug_mode(struct rpp_lr_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_rpp_lr, n_debug_mode);
}

struct rpp_lr_nthw *nthw_rpp_lr_new(void)
{
	struct rpp_lr_nthw *p = malloc(sizeof(struct rpp_lr_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_rpp_lr_delete(struct rpp_lr_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

int nthw_rpp_lr_init(struct rpp_lr_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_RPP_LR, n_instance);

	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: RppLr %d: no such instance", p_adapter_id_str,
			n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_rpp_lr = nthw_fpga_query_module(p_fpga, MOD_RPP_LR, n_instance);
	if (p->m_rpp_lr)
		p->mp_rcp_ctrl = nthw_module_get_register(p->m_rpp_lr, RPP_LR_RCP_CTRL);
	p->mp_rcp_addr = nthw_register_get_field(p->mp_rcp_ctrl, RPP_LR_RCP_CTRL_ADR);
	p->mp_rcp_cnt = nthw_register_get_field(p->mp_rcp_ctrl, RPP_LR_RCP_CTRL_CNT);
	if (p->m_rpp_lr)
		p->mp_rcp_data = nthw_module_get_register(p->m_rpp_lr, RPP_LR_RCP_DATA);
	p->mp_rcp_data_exp = nthw_register_get_field(p->mp_rcp_data, RPP_LR_RCP_DATA_EXP);

	if (p->m_rpp_lr)
		p->mp_ifr_rcp_ctrl = nthw_module_query_register(p->m_rpp_lr, RPP_LR_IFR_RCP_CTRL);
	p->mp_ifr_rcp_addr =
		nthw_register_query_field(p->mp_ifr_rcp_ctrl, RPP_LR_IFR_RCP_CTRL_ADR);
	p->mp_ifr_rcp_cnt = nthw_register_query_field(p->mp_ifr_rcp_ctrl, RPP_LR_IFR_RCP_CTRL_CNT);
	if (p->m_rpp_lr)
		p->mp_ifr_rcp_data = nthw_module_query_register(p->m_rpp_lr, RPP_LR_IFR_RCP_DATA);
	p->mp_ifr_rcp_data_ipv4_en =
		nthw_register_query_field(p->mp_ifr_rcp_data, RPP_LR_IFR_RCP_DATA_IPV4_EN);
	p->mp_ifr_rcp_data_ipv6_en =
		nthw_register_query_field(p->mp_ifr_rcp_data, RPP_LR_IFR_RCP_DATA_IPV6_EN);
	p->mp_ifr_rcp_data_mtu =
		nthw_register_query_field(p->mp_ifr_rcp_data, RPP_LR_IFR_RCP_DATA_MTU);
	p->mp_ifr_rcp_data_ipv4_df_drop =
		nthw_register_query_field(p->mp_ifr_rcp_data, RPP_LR_IFR_RCP_DATA_IPV4_DF_DROP);
	p->mp_ifr_rcp_data_ipv6_drop =
		nthw_register_query_field(p->mp_ifr_rcp_data, RPP_LR_IFR_RCP_DATA_IPV6_DROP);

	return 0;
}

void nthw_rpp_lr_rcp_select(const struct rpp_lr_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_rcp_addr);
	nthw_field_set_val32(p->mp_rcp_addr, val);
}

void nthw_rpp_lr_rcp_cnt(const struct rpp_lr_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_rcp_cnt);
	nthw_field_set_val32(p->mp_rcp_cnt, val);
}

void nthw_rpp_lr_rcp_exp(const struct rpp_lr_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_rcp_data_exp);
	nthw_field_set_val32(p->mp_rcp_data_exp, val);
}

void nthw_rpp_lr_rcp_flush(const struct rpp_lr_nthw *p)
{
	RTE_ASSERT(p->mp_rcp_ctrl);
	RTE_ASSERT(p->mp_rcp_data);
	nthw_register_flush(p->mp_rcp_ctrl, 1);
	nthw_register_flush(p->mp_rcp_data, 1);
}

void nthw_rpp_lr_ifr_rcp_select(const struct rpp_lr_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_ifr_rcp_addr);
	nthw_field_set_val32(p->mp_ifr_rcp_addr, val);
}

void nthw_rpp_lr_ifr_rcp_cnt(const struct rpp_lr_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_ifr_rcp_cnt);
	nthw_field_set_val32(p->mp_ifr_rcp_cnt, val);
}

void nthw_rpp_lr_ifr_rcp_ipv4_en(const struct rpp_lr_nthw *p, uint32_t val)
{
	if (p->mp_ifr_rcp_data_ipv4_en)
		nthw_field_set_val32(p->mp_ifr_rcp_data_ipv4_en, val);
}

void nthw_rpp_lr_ifr_rcp_ipv4_df_drop(const struct rpp_lr_nthw *p, uint32_t val)
{
	if (p->mp_ifr_rcp_data_ipv4_df_drop)
		nthw_field_set_val32(p->mp_ifr_rcp_data_ipv4_df_drop, val);
}

void nthw_rpp_lr_ifr_rcp_ipv6_en(const struct rpp_lr_nthw *p, uint32_t val)
{
	if (p->mp_ifr_rcp_data_ipv6_en)
		nthw_field_set_val32(p->mp_ifr_rcp_data_ipv6_en, val);
}

void nthw_rpp_lr_ifr_rcp_ipv6_drop(const struct rpp_lr_nthw *p, uint32_t val)
{
	if (p->mp_ifr_rcp_data_ipv6_drop)
		nthw_field_set_val32(p->mp_ifr_rcp_data_ipv6_drop, val);
}

void nthw_rpp_lr_ifr_rcp_mtu(const struct rpp_lr_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_ifr_rcp_data_mtu);
	nthw_field_set_val32(p->mp_ifr_rcp_data_mtu, val);
}

void nthw_rpp_lr_ifr_rcp_flush(const struct rpp_lr_nthw *p)
{
	RTE_ASSERT(p->mp_ifr_rcp_ctrl);
	RTE_ASSERT(p->mp_ifr_rcp_data);
	nthw_register_flush(p->mp_ifr_rcp_ctrl, 1);
	nthw_register_flush(p->mp_ifr_rcp_data, 1);
}
