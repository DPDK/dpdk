/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_rmc.h"

nthw_rmc_t *nthw_rmc_new(void)
{
	nthw_rmc_t *p = malloc(sizeof(nthw_rmc_t));

	if (p)
		memset(p, 0, sizeof(nthw_rmc_t));

	return p;
}

int nthw_rmc_init(nthw_rmc_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_RMC, n_instance);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: RMC %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_rmc = p_mod;

	/* Params */
	p->mn_ports =
		nthw_fpga_get_product_param(p_fpga, NT_RX_PORTS,
			nthw_fpga_get_product_param(p_fpga, NT_PORTS, 0));
	p->mn_nims = nthw_fpga_get_product_param(p_fpga, NT_NIMS, 0);
	p->mb_administrative_block = false;

	NT_LOG(DBG, NTHW, "%s: RMC %d", p_adapter_id_str, p->mn_instance);

	p->mp_reg_ctrl = nthw_module_get_register(p->mp_mod_rmc, RMC_CTRL);

	p->mp_fld_ctrl_block_stat_drop =
		nthw_register_get_field(p->mp_reg_ctrl, RMC_CTRL_BLOCK_STATT);
	p->mp_fld_ctrl_block_keep_alive =
		nthw_register_get_field(p->mp_reg_ctrl, RMC_CTRL_BLOCK_KEEPA);
	p->mp_fld_ctrl_block_mac_port =
		nthw_register_get_field(p->mp_reg_ctrl, RMC_CTRL_BLOCK_MAC_PORT);

	p->mp_reg_status = nthw_module_query_register(p->mp_mod_rmc, RMC_STATUS);

	if (p->mp_reg_status) {
		p->mp_fld_sf_ram_of =
			nthw_register_get_field(p->mp_reg_status, RMC_STATUS_SF_RAM_OF);
		p->mp_fld_descr_fifo_of =
			nthw_register_get_field(p->mp_reg_status, RMC_STATUS_DESCR_FIFO_OF);
	}

	p->mp_reg_dbg = nthw_module_query_register(p->mp_mod_rmc, RMC_DBG);

	if (p->mp_reg_dbg)
		p->mp_fld_dbg_merge = nthw_register_get_field(p->mp_reg_dbg, RMC_DBG_MERGE);

	p->mp_reg_mac_if = nthw_module_query_register(p->mp_mod_rmc, RMC_MAC_IF);

	if (p->mp_reg_mac_if)
		p->mp_fld_mac_if_err = nthw_register_get_field(p->mp_reg_mac_if, RMC_MAC_IF_ERR);

	return 0;
}

uint32_t nthw_rmc_get_status_sf_ram_of(nthw_rmc_t *p)
{
	return (p->mp_reg_status) ? nthw_field_get_updated(p->mp_fld_sf_ram_of) : 0xffffffff;
}

uint32_t nthw_rmc_get_status_descr_fifo_of(nthw_rmc_t *p)
{
	return (p->mp_reg_status) ? nthw_field_get_updated(p->mp_fld_descr_fifo_of) : 0xffffffff;
}

uint32_t nthw_rmc_get_dbg_merge(nthw_rmc_t *p)
{
	return (p->mp_reg_dbg) ? nthw_field_get_updated(p->mp_fld_dbg_merge) : 0xffffffff;
}

uint32_t nthw_rmc_get_mac_if_err(nthw_rmc_t *p)
{
	return (p->mp_reg_mac_if) ? nthw_field_get_updated(p->mp_fld_mac_if_err) : 0xffffffff;
}

void nthw_rmc_block(nthw_rmc_t *p)
{
	/* BLOCK_STATT(0)=1 BLOCK_KEEPA(1)=1 BLOCK_MAC_PORT(8:11)=~0 */
	if (!p->mb_administrative_block) {
		nthw_field_set_flush(p->mp_fld_ctrl_block_stat_drop);
		nthw_field_set_flush(p->mp_fld_ctrl_block_keep_alive);
		nthw_field_set_flush(p->mp_fld_ctrl_block_mac_port);
	}
}

void nthw_rmc_unblock(nthw_rmc_t *p, bool b_is_secondary)
{
	uint32_t n_block_mask = ~0U << (b_is_secondary ? p->mn_nims : p->mn_ports);

	/* BLOCK_STATT(0)=0 BLOCK_KEEPA(1)=0 BLOCK_MAC_PORT(8:11)=0 */
	if (!p->mb_administrative_block) {
		nthw_field_clr_flush(p->mp_fld_ctrl_block_stat_drop);
		nthw_field_clr_flush(p->mp_fld_ctrl_block_keep_alive);
		nthw_field_set_val_flush32(p->mp_fld_ctrl_block_mac_port, n_block_mask);
	}
}
