/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Napatech A/S
 */

#include "ntnic_mod_reg.h"
#include "nthw_register.h"
#include "nthw_drv.h"
#include "nt_util.h"
#include "ntlog.h"


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

static void nthw_fpga_rst9569_set_default_rst_values(struct nthw_fpga_rst_nt400dxx *const p)
{
	nthw_field_update_register(p->p_fld_rst_sys);
	nthw_field_set_all(p->p_fld_rst_sys);
	nthw_field_set_val32(p->p_fld_rst_ddr4, 1);
	nthw_field_set_val_flush32(p->p_fld_rst_phy_ftile, 1);
}

static void nthw_fpga_rst9569_ddr4_rst(struct nthw_fpga_rst_nt400dxx *const p, uint32_t val)
{
	nthw_field_update_register(p->p_fld_rst_ddr4);
	nthw_field_set_val_flush32(p->p_fld_rst_ddr4, val);
}

static bool nthw_fpga_rst9569_get_ddr4_calib_complete_stat(struct nthw_fpga_rst_nt400dxx *const p)
{
	return nthw_field_get_updated(p->p_fld_stat_ddr4_calib_complete) != 0;
}

static void nthw_fpga_rst9569_set_ddr4_calib_complete_latch(struct nthw_fpga_rst_nt400dxx *const p,
	uint32_t val)
{
	nthw_field_update_register(p->p_fld_latch_ddr4_calib_complete);
	nthw_field_set_val_flush32(p->p_fld_latch_ddr4_calib_complete, val);
}

static int nthw_fpga_rst9569_wait_ddr4_calibration_complete(struct fpga_info_s *p_fpga_info,
	struct nthw_fpga_rst_nt400dxx *p_rst)
{
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	uint32_t complete;
	uint32_t retrycount;
	uint32_t timeout;

	/* 3: wait until DDR4 CALIB COMPLETE */
	NT_LOG_DBGX(DBG, NTHW, "%s: DDR4 CALIB COMPLETE wait complete", p_adapter_id_str);
	/*
	 * The following retry count gives a total timeout of 1 * 5 + 5 * 8 = 45sec
	 * It has been observed that at least 21sec can be necessary
	 */
	retrycount = 1;
	timeout = 50000;/* initial timeout must be set to 5 sec. */

	do {
		complete = nthw_fpga_rst9569_get_ddr4_calib_complete_stat(p_rst);

		if (!complete)
			nthw_os_wait_usec(100);

		timeout--;

		if (timeout == 0) {
			if (retrycount == 0) {
				NT_LOG(ERR, NTHW,
					"%s: %s: Timeout waiting for DDR4 CALIB COMPLETE to be complete",
					p_adapter_id_str, __func__);
				return -1;
			}

			nthw_fpga_rst9569_ddr4_rst(p_rst, 1);	/* Reset DDR4 */
			nthw_fpga_rst9569_ddr4_rst(p_rst, 0);
			retrycount--;
			timeout = 90000;/* Increase timeout for second attempt to 8 sec. */
		}
	} while (!complete);

	return 0;
}

static bool nthw_fpga_rst9569_get_ddr4_calib_complete_latch(struct nthw_fpga_rst_nt400dxx *const p)
{
	return nthw_field_get_updated(p->p_fld_latch_ddr4_calib_complete) != 0;
}

static int nthw_fpga_rst9569_product_reset(struct fpga_info_s *p_fpga_info,
	struct nthw_fpga_rst_nt400dxx *p_rst)
{
	RTE_ASSERT(p_fpga_info);
	RTE_ASSERT(p_rst);

	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;

	/* (0) Reset all domains / modules except peripherals: */
	NT_LOG_DBGX(DBG, NTHW, "%s: RST defaults", p_adapter_id_str);
	nthw_fpga_rst9569_set_default_rst_values(p_rst);

	/*
	 * Wait a while before waiting for deasserting ddr4 reset
	 */
	nthw_os_wait_usec(2000);

	/* (1) De-assert DDR4 reset: */
	NT_LOG_DBGX(DBG, NTHW, "%s: De-asserting DDR4 reset", p_adapter_id_str);
	nthw_fpga_rst9569_ddr4_rst(p_rst, 0);

	/*
	 * Wait a while before waiting for calibration complete, since calibration complete
	 * is true while ddr4 is in reset
	 */
	nthw_os_wait_usec(2000);

	/* (2) Wait until DDR4 calibration complete */
	NT_LOG_DBGX(DBG, NTHW, "%s: DDR4 calibration", p_adapter_id_str);
	int res = nthw_fpga_rst9569_wait_ddr4_calibration_complete(p_fpga_info, p_rst);

	if (res) {
		NT_LOG(ERR, NTHW, "%s: DDR4 calibration failed", p_adapter_id_str);
		return res;
	}

	/* (3) Set DDR4 calib complete latched bits: */
	nthw_fpga_rst9569_set_ddr4_calib_complete_latch(p_rst, 1);

	/* Wait for phy to settle. */
	nthw_os_wait_usec(20000);

	/* (4) Ensure all latched status bits are still set: */
	if (!nthw_fpga_rst9569_get_ddr4_calib_complete_latch(p_rst)) {
		NT_LOG(ERR, NTHW, "%s: %s: DDR4 calibration complete has toggled",
			p_adapter_id_str, __func__);
	}


	return 0;
}

static int nthw_fpga_rst9569_init(struct fpga_info_s *p_fpga_info,
	struct nthw_fpga_rst_nt400dxx *p_rst)
{
	RTE_ASSERT(p_fpga_info);
	RTE_ASSERT(p_rst);

	int res = nthw_fpga_rst9569_product_reset(p_fpga_info, p_rst);

	return res;
}

static struct rst9569_ops rst9569_ops = {
	.nthw_fpga_rst9569_init = nthw_fpga_rst9569_init,
	.nthw_fpga_rst9569_setup = nthw_fpga_rst9569_setup,
};

void nthw_rst9569_ops_init(void)
{
	nthw_reg_rst9569_ops(&rst9569_ops);
}
