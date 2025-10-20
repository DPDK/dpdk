/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_igam.h"

nthw_igam_t *nthw_igam_new(void)
{
	nthw_igam_t *p = malloc(sizeof(nthw_igam_t));

	if (p)
		memset(p, 0, sizeof(nthw_igam_t));

	return p;
}

int nthw_igam_init(nthw_igam_t *p, nthw_fpga_t *p_fpga, int mn_igam_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_IGAM, mn_igam_instance);

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: IGAM %d: no such instance", p_adapter_id_str,
			mn_igam_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_igam_instance = mn_igam_instance;

	p->mp_mod_igam = mod;

	p->mp_reg_base = nthw_module_get_register(p->mp_mod_igam, IGAM_BASE);
	p->mp_fld_base_ptr = nthw_register_get_field(p->mp_reg_base, IGAM_BASE_PTR);
	p->mp_fld_base_busy = nthw_register_get_field(p->mp_reg_base, IGAM_BASE_BUSY);
	p->mp_fld_base_cmd = nthw_register_get_field(p->mp_reg_base, IGAM_BASE_CMD);

	p->mp_reg_data = nthw_module_get_register(p->mp_mod_igam, IGAM_DATA);
	p->mp_fld_data_data = nthw_register_get_field(p->mp_reg_data, IGAM_DATA_DATA);

	p->mp_reg_ctrl = nthw_module_query_register(p->mp_mod_igam, IGAM_CTRL);

	if (p->mp_reg_ctrl) {
		p->mp_fld_ctrl_forward_rst =
			nthw_register_get_field(p->mp_reg_ctrl, IGAM_CTRL_FORWARD_RST);

	} else {
		p->mp_fld_ctrl_forward_rst = NULL;
	}

	return 0;
}

uint32_t nthw_igam_read(nthw_igam_t *p, uint32_t address)
{
	nthw_register_update(p->mp_reg_base);
	nthw_field_set_val32(p->mp_fld_base_cmd, 0);
	nthw_field_set_val_flush32(p->mp_fld_base_ptr, address);

	while (nthw_field_get_updated(p->mp_fld_base_busy) == 1)
		nthw_os_wait_usec(100);

	return nthw_field_get_updated(p->mp_fld_data_data);
}

void nthw_igam_write(nthw_igam_t *p, uint32_t address, uint32_t data)
{
	nthw_field_set_val_flush32(p->mp_fld_data_data, data);
	nthw_register_update(p->mp_reg_base);
	nthw_field_set_val32(p->mp_fld_base_ptr, address);
	nthw_field_set_val_flush32(p->mp_fld_base_cmd, 1);

	while (nthw_field_get_updated(p->mp_fld_base_busy) == 1)
		nthw_os_wait_usec(100);
}

void nthw_igam_set_ctrl_forward_rst(nthw_igam_t *p, uint32_t value)
{
	if (p->mp_fld_ctrl_forward_rst) {
		nthw_field_get_updated(p->mp_fld_ctrl_forward_rst);
		nthw_field_set_val_flush32(p->mp_fld_ctrl_forward_rst, value);
	}
}
