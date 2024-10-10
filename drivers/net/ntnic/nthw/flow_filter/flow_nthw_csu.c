/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "ntlog.h"
#include "nthw_drv.h"
#include "nthw_register.h"

#include "flow_nthw_csu.h"

struct csu_nthw *csu_nthw_new(void)
{
	struct csu_nthw *p = malloc(sizeof(struct csu_nthw));

	if (p)
		(void)memset(p, 0, sizeof(*p));

	return p;
}

void csu_nthw_delete(struct csu_nthw *p)
{
	if (p) {
		(void)memset(p, 0, sizeof(*p));
		free(p);
	}
}

int csu_nthw_init(struct csu_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_CSU, n_instance);

	assert(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: Csu %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_csu = p_mod;

	p->mp_rcp_ctrl = nthw_module_get_register(p->m_csu, CSU_RCP_CTRL);
	p->mp_rcp_ctrl_adr = nthw_register_get_field(p->mp_rcp_ctrl, CSU_RCP_CTRL_ADR);
	p->mp_rcp_ctrl_cnt = nthw_register_get_field(p->mp_rcp_ctrl, CSU_RCP_CTRL_CNT);
	p->mp_rcp_data = nthw_module_get_register(p->m_csu, CSU_RCP_DATA);
	p->mp_rcp_data_ol3_cmd = nthw_register_get_field(p->mp_rcp_data, CSU_RCP_DATA_OL3_CMD);
	p->mp_rcp_data_ol4_cmd = nthw_register_get_field(p->mp_rcp_data, CSU_RCP_DATA_OL4_CMD);
	p->mp_rcp_data_il3_cmd = nthw_register_get_field(p->mp_rcp_data, CSU_RCP_DATA_IL3_CMD);
	p->mp_rcp_data_il4_cmd = nthw_register_get_field(p->mp_rcp_data, CSU_RCP_DATA_IL4_CMD);

	return 0;
}
