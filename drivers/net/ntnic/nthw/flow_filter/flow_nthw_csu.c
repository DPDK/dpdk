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

void nthw_csu_set_debug_mode(struct csu_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_csu, n_debug_mode);
}

struct csu_nthw *nthw_csu_new(void)
{
	struct csu_nthw *p = malloc(sizeof(struct csu_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_csu_delete(struct csu_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

int nthw_csu_init(struct csu_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_CSU, n_instance);

	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

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

void nthw_csu_rcp_select(const struct csu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_ctrl_adr, val);
}

void nthw_csu_rcp_cnt(const struct csu_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_ctrl_cnt, val);
}

void nthw_csu_rcp_outer_l3_cmd(const struct csu_nthw *p, uint32_t val)
{
	/*
	 * Select L3 calc method for outer layer3.
	 * 0: Do not touch checksum field.
	 * 1: Check, but do not touch checksum field.
	 * 2: Insert checksum header value for BAD checksum.
	 * 3: Insert checksum header value for GOOD checksum.
	 */
	nthw_field_set_val32(p->mp_rcp_data_ol3_cmd, val);
}

void nthw_csu_rcp_outer_l4_cmd(const struct csu_nthw *p, uint32_t val)
{
	/*
	 * Select L4 calc method for outer layer4.
	 * 0: Do not touch checksum field.
	 * 1: Check, but do not touch checksum field.
	 * 2: Insert checksum header value for BAD checksum.
	 * 3: Insert checksum header value for GOOD checksum.
	 * 4: Set UDP checksum value of ZERO for both IPv4/IPv6, set good checksum for TCP.
	 * 5: Set UDP checksum value of ZERO for IPv4, set good checksum for TCP.
	 * 6: Set UDP checksum value of ZERO for outer tunnel when tunnel is IPv4/IPv6 and UDP,
	 * otherwise GOOD checksum. 7: Set UDP checksum value of ZERO for outer tunnel when tunnel
	 * is IPv4 and UDP, otherwise GOOD checksum.
	 */
	nthw_field_set_val32(p->mp_rcp_data_ol4_cmd, val);
}

void nthw_csu_rcp_inner_l3_cmd(const struct csu_nthw *p, uint32_t val)
{
	/*
	 * Select L3 calc method for inner layer3 (tunneled).
	 * 0: Do not touch checksum field.
	 * 1: Check, but do not touch checksum field.
	 * 2: Insert checksum header value for BAD checksum.
	 * 3: Insert checksum header value for GOOD checksum.
	 */
	nthw_field_set_val32(p->mp_rcp_data_il3_cmd, val);
}

void nthw_csu_rcp_inner_l4_cmd(const struct csu_nthw *p, uint32_t val)
{
	/*
	 * Select L4 calc method for inner layer4 (tunneled).
	 * 0: Do not touch checksum field.
	 * 1: Check, but do not touch checksum field.
	 * 2: Insert checksum header value for BAD checksum.
	 * 3: Insert checksum header value for GOOD checksum.
	 * 4: Set UDP checksum value of ZERO for both IPv4/IPv6, set good checksum for TCP.
	 * 5: Set UDP checksum value of ZERO for IPv4, set good checksum for TCP.
	 * 6: Set UDP checksum value of ZERO for outer tunnel when tunnel is IPv4/IPv6 and UDP,
	 * otherwise GOOD checksum. 7: Set UDP checksum value of ZERO for outer tunnel when tunnel
	 * is IPv4 and UDP, otherwise GOOD checksum.
	 */
	nthw_field_set_val32(p->mp_rcp_data_il4_cmd, val);
}

void nthw_csu_rcp_flush(const struct csu_nthw *p)
{
	nthw_register_flush(p->mp_rcp_ctrl, 1);
	nthw_register_flush(p->mp_rcp_data, 1);
}
