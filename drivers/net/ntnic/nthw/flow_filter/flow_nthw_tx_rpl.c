/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "ntlog.h"
#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_tx_rpl.h"

void tx_rpl_nthw_set_debug_mode(struct tx_rpl_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_tx_rpl, n_debug_mode);
}

struct tx_rpl_nthw *tx_rpl_nthw_new(void)
{
	struct tx_rpl_nthw *p = malloc(sizeof(struct tx_rpl_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void tx_rpl_nthw_delete(struct tx_rpl_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

int tx_rpl_nthw_init(struct tx_rpl_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_TX_RPL, n_instance);

	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: TxRpl %d: no such instance", p_adapter_id_str,
			n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_tx_rpl = nthw_fpga_query_module(p_fpga, MOD_TX_RPL, n_instance);

	p->mp_rcp_ctrl = nthw_module_get_register(p->m_tx_rpl, RPL_RCP_CTRL);
	p->mp_rcp_ctrl_addr = nthw_register_get_field(p->mp_rcp_ctrl, RPL_RCP_CTRL_ADR);
	p->mp_rcp_ctrl_cnt = nthw_register_get_field(p->mp_rcp_ctrl, RPL_RCP_CTRL_CNT);
	p->mp_rcp_data = nthw_module_get_register(p->m_tx_rpl, RPL_RCP_DATA);
	p->mp_rcp_data_dyn = nthw_register_get_field(p->mp_rcp_data, RPL_RCP_DATA_DYN);
	p->mp_rcp_data_ofs = nthw_register_get_field(p->mp_rcp_data, RPL_RCP_DATA_OFS);
	p->mp_rcp_data_len = nthw_register_get_field(p->mp_rcp_data, RPL_RCP_DATA_LEN);
	p->mp_rcp_data_rpl_ptr = nthw_register_get_field(p->mp_rcp_data, RPL_RCP_DATA_RPL_PTR);
	p->mp_rcp_data_ext_prio = nthw_register_get_field(p->mp_rcp_data, RPL_RCP_DATA_EXT_PRIO);
	p->mp_rcp_data_eth_type_wr =
		nthw_register_query_field(p->mp_rcp_data, RPL_RCP_DATA_ETH_TYPE_WR);

	p->mp_ext_ctrl = nthw_module_get_register(p->m_tx_rpl, RPL_EXT_CTRL);
	p->mp_ext_ctrl_addr = nthw_register_get_field(p->mp_ext_ctrl, RPL_EXT_CTRL_ADR);
	p->mp_ext_ctrl_cnt = nthw_register_get_field(p->mp_ext_ctrl, RPL_EXT_CTRL_CNT);
	p->mp_ext_data = nthw_module_get_register(p->m_tx_rpl, RPL_EXT_DATA);
	p->mp_ext_data_rpl_ptr = nthw_register_get_field(p->mp_ext_data, RPL_EXT_DATA_RPL_PTR);

	p->mp_rpl_ctrl = nthw_module_get_register(p->m_tx_rpl, RPL_RPL_CTRL);
	p->mp_rpl_ctrl_addr = nthw_register_get_field(p->mp_rpl_ctrl, RPL_RPL_CTRL_ADR);
	p->mp_rpl_ctrl_cnt = nthw_register_get_field(p->mp_rpl_ctrl, RPL_RPL_CTRL_CNT);
	p->mp_rpl_data = nthw_module_get_register(p->m_tx_rpl, RPL_RPL_DATA);
	p->mp_rpl_data_value = nthw_register_get_field(p->mp_rpl_data, RPL_RPL_DATA_VALUE);

	return 0;
}

void tx_rpl_nthw_rcp_select(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_ctrl_addr, val);
}

void tx_rpl_nthw_rcp_cnt(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_ctrl_cnt, val);
}

void tx_rpl_nthw_rcp_dyn(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_dyn, val);
}

void tx_rpl_nthw_rcp_ofs(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ofs, val);
}

void tx_rpl_nthw_rcp_len(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_len, val);
}

void tx_rpl_nthw_rcp_rpl_ptr(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_rpl_ptr, val);
}

void tx_rpl_nthw_rcp_ext_prio(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ext_prio, val);
}

void tx_rpl_nthw_rcp_eth_type_wr(const struct tx_rpl_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_rcp_data_eth_type_wr);
	nthw_field_set_val32(p->mp_rcp_data_eth_type_wr, val);
}

void tx_rpl_nthw_rcp_flush(const struct tx_rpl_nthw *p)
{
	nthw_register_flush(p->mp_rcp_ctrl, 1);
	nthw_register_flush(p->mp_rcp_data, 1);
}

void tx_rpl_nthw_ext_select(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_ext_ctrl_addr, val);
}

void tx_rpl_nthw_ext_cnt(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_ext_ctrl_cnt, val);
}

void tx_rpl_nthw_ext_rpl_ptr(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_ext_data_rpl_ptr, val);
}

void tx_rpl_nthw_ext_flush(const struct tx_rpl_nthw *p)
{
	nthw_register_flush(p->mp_ext_ctrl, 1);
	nthw_register_flush(p->mp_ext_data, 1);
}

void tx_rpl_nthw_rpl_select(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rpl_ctrl_addr, val);
}

void tx_rpl_nthw_rpl_cnt(const struct tx_rpl_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rpl_ctrl_cnt, val);
}

void tx_rpl_nthw_rpl_value(const struct tx_rpl_nthw *p, const uint32_t *val)
{
	nthw_field_set_val(p->mp_rpl_data_value, val, 4);
}

void tx_rpl_nthw_rpl_flush(const struct tx_rpl_nthw *p)
{
	nthw_register_flush(p->mp_rpl_ctrl, 1);
	nthw_register_flush(p->mp_rpl_data, 1);
}
