/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_slc_lr.h"

void nthw_slc_lr_set_debug_mode(struct slc_lr_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_slc_lr, n_debug_mode);
}

struct slc_lr_nthw *nthw_slc_lr_new(void)
{
	struct slc_lr_nthw *p = malloc(sizeof(struct slc_lr_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_slc_lr_delete(struct slc_lr_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

int nthw_slc_lr_init(struct slc_lr_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_SLC_LR, n_instance);

	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: Slc %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_slc_lr = nthw_fpga_query_module(p_fpga, MOD_SLC_LR, n_instance);

	/* RCP */
	p->mp_rcp_ctrl = nthw_module_get_register(p->m_slc_lr, SLC_RCP_CTRL);
	p->mp_rcp_addr = nthw_register_get_field(p->mp_rcp_ctrl, SLC_RCP_CTRL_ADR);
	p->mp_rcp_cnt = nthw_register_get_field(p->mp_rcp_ctrl, SLC_RCP_CTRL_CNT);
	p->mp_rcp_data = nthw_module_get_register(p->m_slc_lr, SLC_RCP_DATA);
	p->mp_rcp_data_head_slc_en =
		nthw_register_get_field(p->mp_rcp_data, SLC_RCP_DATA_HEAD_SLC_EN);
	p->mp_rcp_data_head_dyn = nthw_register_get_field(p->mp_rcp_data, SLC_RCP_DATA_HEAD_DYN);
	p->mp_rcp_data_head_ofs = nthw_register_get_field(p->mp_rcp_data, SLC_RCP_DATA_HEAD_OFS);
	p->mp_rcp_data_tail_slc_en =
		nthw_register_get_field(p->mp_rcp_data, SLC_RCP_DATA_TAIL_SLC_EN);
	p->mp_rcp_data_tail_dyn = nthw_register_get_field(p->mp_rcp_data, SLC_RCP_DATA_TAIL_DYN);
	p->mp_rcp_data_tail_ofs = nthw_register_get_field(p->mp_rcp_data, SLC_RCP_DATA_TAIL_OFS);
	p->mp_rcp_data_pcap = nthw_register_get_field(p->mp_rcp_data, SLC_RCP_DATA_PCAP);

	return 0;
}

/* RCP */
void nthw_slc_lr_rcp_select(const struct slc_lr_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_addr, val);
}

void nthw_slc_lr_rcp_cnt(const struct slc_lr_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_cnt, val);
}

void nthw_slc_lr_rcp_head_slc_en(const struct slc_lr_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_head_slc_en, val);
}

void nthw_slc_lr_rcp_head_dyn(const struct slc_lr_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_head_dyn, val);
}

void nthw_slc_lr_rcp_head_ofs(const struct slc_lr_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_head_ofs, val);
}

void nthw_slc_lr_rcp_tail_slc_en(const struct slc_lr_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_tail_slc_en, val);
}

void nthw_slc_lr_rcp_tail_dyn(const struct slc_lr_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_tail_dyn, val);
}

void nthw_slc_lr_rcp_tail_ofs(const struct slc_lr_nthw *p, int32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_tail_ofs, val);
}

void nthw_slc_lr_rcp_pcap(const struct slc_lr_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_pcap, val);
}

void nthw_slc_lr_rcp_flush(const struct slc_lr_nthw *p)
{
	nthw_register_flush(p->mp_rcp_ctrl, 1);
	nthw_register_flush(p->mp_rcp_data, 1);
}
