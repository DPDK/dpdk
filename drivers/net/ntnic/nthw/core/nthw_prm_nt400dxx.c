/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"
#include "nthw_drv.h"
#include "nthw_register.h"
#include "nthw_prm_nt400dxx.h"

nthw_prm_nt400dxx_t *nthw_prm_nt400dxx_new(void)
{
	nthw_prm_nt400dxx_t *p = malloc(sizeof(nthw_prm_nt400dxx_t));

	if (p)
		memset(p, 0, sizeof(nthw_prm_nt400dxx_t));

	return p;
}

int nthw_prm_nt400dxx_init(nthw_prm_nt400dxx_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_PRM_NT400DXX, n_instance);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: PRM_NT400DXX %d: no such instance",
			p->mp_fpga->p_fpga_info->mp_adapter_id_str, p->mn_instance);
		return -1;
	}

	p->mp_mod_prm = p_mod;

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;

	p->mp_reg_rst = nthw_module_get_register(p->mp_mod_prm, PRM_NT400DXX_RST);
	p->mp_fld_rst_periph = nthw_register_get_field(p->mp_reg_rst, PRM_NT400DXX_RST_PERIPH);
	p->mp_fld_rst_platform = nthw_register_get_field(p->mp_reg_rst, PRM_NT400DXX_RST_PLATFORM);
	return 0;
}

void nthw_prm_nt400dxx_periph_rst(nthw_prm_nt400dxx_t *p, uint32_t val)
{
	nthw_field_update_register(p->mp_fld_rst_periph);
	nthw_field_set_val_flush32(p->mp_fld_rst_periph, val);
}

void nthw_prm_nt400dxx_platform_rst(nthw_prm_nt400dxx_t *p, uint32_t val)
{
	nthw_field_update_register(p->mp_fld_rst_platform);
	nthw_field_set_val_flush32(p->mp_fld_rst_platform, val);
}
