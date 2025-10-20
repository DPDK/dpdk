/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Napatech A/S
 */

#include "ntnic_mod_reg.h"
#include "nthw_register.h"
#include "nthw_drv.h"


static int nthw_fpga_rst9569_setup(nthw_fpga_t *p_fpga, struct nthw_fpga_rst_nt400dxx *const p)
{
	RTE_ASSERT(p_fpga);
	RTE_ASSERT(p);

	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	const int n_fpga_product_id = p_fpga->mn_product_id;
	const int n_fpga_version = p_fpga->mn_fpga_version;
	const int n_fpga_revision = p_fpga->mn_fpga_revision;

	nthw_module_t *p_mod_rst;
	nthw_register_t *p_curr_reg;

	p->n_fpga_product_id = n_fpga_product_id;
	p->n_fpga_version = n_fpga_version;
	p->n_fpga_revision = n_fpga_revision;

	NT_LOG(DBG, NTHW, "%s: %s: FPGA reset setup: FPGA %04d-%02d-%02d", p_adapter_id_str,
		__func__, n_fpga_product_id, n_fpga_version, n_fpga_revision);

	p_mod_rst = nthw_fpga_query_module(p_fpga, MOD_RST9569, 0);

	if (p_mod_rst == NULL) {
		NT_LOG(ERR, NTHW, "%s: RST %d: no such instance", p_adapter_id_str, 0);
		return -1;
	}

	/* RST register field pointers */
	p_curr_reg = nthw_module_get_register(p_mod_rst, RST9569_RST);
	p->p_fld_rst_sys = nthw_register_get_field(p_curr_reg, RST9569_RST_SYS);
	p->p_fld_rst_ddr4 = nthw_register_get_field(p_curr_reg, RST9569_RST_DDR4);
	p->p_fld_rst_phy_ftile = nthw_register_get_field(p_curr_reg, RST9569_RST_PHY_FTILE);
	nthw_register_update(p_curr_reg);

	p_curr_reg = nthw_module_get_register(p_mod_rst, RST9569_STAT);
	p->p_fld_stat_ddr4_calib_complete =
		nthw_register_get_field(p_curr_reg, RST9569_STAT_DDR4_CALIB_COMPLETE);
	p->p_fld_stat_phy_ftile_rst_done =
		nthw_register_get_field(p_curr_reg, RST9569_STAT_PHY_FTILE_RST_DONE);
	p->p_fld_stat_phy_ftile_rdy =
		nthw_register_get_field(p_curr_reg, RST9569_STAT_PHY_FTILE_RDY);
	nthw_register_update(p_curr_reg);

	p_curr_reg = nthw_module_get_register(p_mod_rst, RST9569_LATCH);
	p->p_fld_latch_ddr4_calib_complete =
		nthw_register_get_field(p_curr_reg, RST9569_LATCH_DDR4_CALIB_COMPLETE);
	p->p_fld_latch_phy_ftile_rst_done =
		nthw_register_get_field(p_curr_reg, RST9569_LATCH_PHY_FTILE_RST_DONE);
	p->p_fld_latch_phy_ftile_rdy =
		nthw_register_get_field(p_curr_reg, RST9569_LATCH_PHY_FTILE_RDY);
	nthw_register_update(p_curr_reg);

	return 0;
};

static int nthw_fpga_rst9569_init(void)
{
	return 0;
}

static struct rst9569_ops rst9569_ops = {
	.nthw_fpga_rst9569_init = nthw_fpga_rst9569_init,
	.nthw_fpga_rst9569_setup = nthw_fpga_rst9569_setup,
};

void nthw_rst9569_ops_init(void)
{
	nthw_reg_rst9569_ops(&rst9569_ops);
}
