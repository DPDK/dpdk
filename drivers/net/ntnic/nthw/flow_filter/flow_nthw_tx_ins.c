/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "ntlog.h"
#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_tx_ins.h"

struct tx_ins_nthw *tx_ins_nthw_new(void)
{
	struct tx_ins_nthw *p = malloc(sizeof(struct tx_ins_nthw));

	if (p)
		(void)memset(p, 0, sizeof(*p));

	return p;
}

void tx_ins_nthw_delete(struct tx_ins_nthw *p)
{
	if (p) {
		(void)memset(p, 0, sizeof(*p));
		free(p);
	}
}

int tx_ins_nthw_init(struct tx_ins_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_TX_INS, n_instance);

	assert(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: TxIns %d: no such instance", p_adapter_id_str,
			n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_tx_ins = nthw_fpga_query_module(p_fpga, MOD_TX_INS, n_instance);

	p->mp_rcp_ctrl = nthw_module_get_register(p->m_tx_ins, INS_RCP_CTRL);
	p->mp_rcp_addr = nthw_register_get_field(p->mp_rcp_ctrl, INS_RCP_CTRL_ADR);
	p->mp_rcp_cnt = nthw_register_get_field(p->mp_rcp_ctrl, INS_RCP_CTRL_CNT);
	p->mp_rcp_data = nthw_module_get_register(p->m_tx_ins, INS_RCP_DATA);
	p->mp_rcp_data_dyn = nthw_register_get_field(p->mp_rcp_data, INS_RCP_DATA_DYN);
	p->mp_rcp_data_ofs = nthw_register_get_field(p->mp_rcp_data, INS_RCP_DATA_OFS);
	p->mp_rcp_data_len = nthw_register_get_field(p->mp_rcp_data, INS_RCP_DATA_LEN);

	return 0;
}
