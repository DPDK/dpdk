/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */
#include "nthw_drv.h"
#include "nthw_register.h"
#include "nthw_fpga.h"

#include "ntnic_nthw_fpga_rst_nt400dxx.h"
#include "ntnic_mod_reg.h"

static int nthw_fpga_rst9574_setup(nthw_fpga_t *p_fpga, struct nthw_fpga_rst_nt400dxx *const p)
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

	p_mod_rst = nthw_fpga_query_module(p_fpga, MOD_RST9574, 0);

	if (p_mod_rst == NULL) {
		NT_LOG(ERR, NTHW, "%s: RST %d: no such instance", p_adapter_id_str, 0);
		return -1;
	}

	p_mod_rst = nthw_fpga_query_module(p_fpga, MOD_RST9574, 0);

	if (p_mod_rst == NULL) {
		NT_LOG(ERR, NTHW, "%s: RST %d: no such instance", p_adapter_id_str, 0);
		return -1;
	}

	/* RST register field pointers */
	p_curr_reg = nthw_module_get_register(p_mod_rst, RST9574_RST);
	p->p_fld_rst_sys = nthw_register_get_field(p_curr_reg, RST9574_RST_SYS);
	p->p_fld_rst_ddr4 = nthw_register_get_field(p_curr_reg, RST9574_RST_DDR4);
	p->p_fld_rst_phy_ftile = nthw_register_get_field(p_curr_reg, RST9574_RST_PHY_FTILE);
	nthw_register_update(p_curr_reg);

	p_curr_reg = nthw_module_get_register(p_mod_rst, RST9574_STAT);
	p->p_fld_stat_ddr4_calib_complete =
		nthw_register_get_field(p_curr_reg, RST9574_STAT_DDR4_CALIB_COMPLETE);
	p->p_fld_stat_phy_ftile_rst_done =
		nthw_register_get_field(p_curr_reg, RST9574_STAT_PHY_FTILE_RST_DONE);
	p->p_fld_stat_phy_ftile_rdy =
		nthw_register_get_field(p_curr_reg, RST9574_STAT_PHY_FTILE_RDY);
	nthw_register_update(p_curr_reg);

	p_curr_reg = nthw_module_get_register(p_mod_rst, RST9574_LATCH);
	p->p_fld_latch_ddr4_calib_complete =
		nthw_register_get_field(p_curr_reg, RST9574_LATCH_DDR4_CALIB_COMPLETE);
	p->p_fld_latch_phy_ftile_rst_done =
		nthw_register_get_field(p_curr_reg, RST9574_LATCH_PHY_FTILE_RST_DONE);
	p->p_fld_latch_phy_ftile_rdy =
		nthw_register_get_field(p_curr_reg, RST9574_LATCH_PHY_FTILE_RDY);
	nthw_register_update(p_curr_reg);

	return 0;
};

static void nthw_fpga_rst9574_set_default_rst_values(struct nthw_fpga_rst_nt400dxx *const p)
{
	nthw_field_update_register(p->p_fld_rst_sys);
	nthw_field_set_all(p->p_fld_rst_sys);
	nthw_field_set_val32(p->p_fld_rst_ddr4, 1);
	nthw_field_set_val_flush32(p->p_fld_rst_phy_ftile, 1);
}

static void nthw_fpga_rst9574_sys_rst(struct nthw_fpga_rst_nt400dxx *const p, uint32_t val)
{
	nthw_field_update_register(p->p_fld_rst_sys);
	nthw_field_set_val_flush32(p->p_fld_rst_sys, val);
}

static void nthw_fpga_rst9574_ddr4_rst(struct nthw_fpga_rst_nt400dxx *const p, uint32_t val)
{
	nthw_field_update_register(p->p_fld_rst_ddr4);
	nthw_field_set_val_flush32(p->p_fld_rst_ddr4, val);
}

static void nthw_fpga_rst9574_phy_ftile_rst(struct nthw_fpga_rst_nt400dxx *const p, uint32_t val)
{
	nthw_field_update_register(p->p_fld_rst_phy_ftile);
	nthw_field_set_val_flush32(p->p_fld_rst_phy_ftile, val);
}

static bool nthw_fpga_rst9574_get_phy_ftile_rst(struct nthw_fpga_rst_nt400dxx *const p)
{
	return nthw_field_get_updated(p->p_fld_rst_phy_ftile) != 0;
}

static bool nthw_fpga_rst9574_get_ddr4_calib_complete_stat(struct nthw_fpga_rst_nt400dxx *const p)
{
	return nthw_field_get_updated(p->p_fld_stat_ddr4_calib_complete) != 0;
}

static bool nthw_fpga_rst9574_get_phy_ftile_rst_done_stat(struct nthw_fpga_rst_nt400dxx *const p)
{
	return nthw_field_get_updated(p->p_fld_stat_phy_ftile_rst_done) != 0;
}

static bool nthw_fpga_rst9574_get_phy_ftile_rdy_stat(struct nthw_fpga_rst_nt400dxx *const p)
{
	return nthw_field_get_updated(p->p_fld_stat_phy_ftile_rdy) != 0;
}

static bool nthw_fpga_rst9574_get_ddr4_calib_complete_latch(struct nthw_fpga_rst_nt400dxx *const p)
{
	return nthw_field_get_updated(p->p_fld_latch_ddr4_calib_complete) != 0;
}

static void nthw_fpga_rst9574_set_ddr4_calib_complete_latch(struct nthw_fpga_rst_nt400dxx *const p,
	uint32_t val)
{
	nthw_field_update_register(p->p_fld_latch_ddr4_calib_complete);
	nthw_field_set_val_flush32(p->p_fld_latch_ddr4_calib_complete, val);
}

static int nthw_fpga_rst9574_wait_ddr4_calibration_complete(struct fpga_info_s *p_fpga_info,
	struct nthw_fpga_rst_nt400dxx *p_rst)
{
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	uint32_t complete;
	uint32_t retrycount;
	uint32_t timeout;

	/* 3: wait until DDR4 CALIB COMPLETE */
	NT_LOG(DBG, NTHW, "%s: %s: DDR4 CALIB COMPLETE wait complete", p_adapter_id_str, __func__);
	/*
	 * The following retry count gives a total timeout of 1 * 5 + 5 * 8 = 45sec
	 * It has been observed that at least 21sec can be necessary
	 */
	retrycount = 1;
	timeout = 50000;/* initial timeout must be set to 5 sec. */

	do {
		complete = nthw_fpga_rst9574_get_ddr4_calib_complete_stat(p_rst);

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

			nthw_fpga_rst9574_ddr4_rst(p_rst, 1);	/* Reset DDR4 */
			nthw_fpga_rst9574_ddr4_rst(p_rst, 0);
			retrycount--;
			timeout = 90000;/* Increase timeout for second attempt to 8 sec. */
		}
	} while (!complete);

	return 0;
}

static int nthw_fpga_rst9574_wait_phy_ftile_rdy(struct fpga_info_s *p_fpga_info,
	struct nthw_fpga_rst_nt400dxx *p_rst)
{
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	uint32_t complete;
	uint32_t timeout;

	/* 5: wait until PHY_FTILE reset done */
	NT_LOG(DBG, NTHW, "%s: %s: PHY FTILE ready", p_adapter_id_str, __func__);
	timeout = 50000;/* initial timeout must be set to 5 sec. */

	do {
		complete = nthw_fpga_rst9574_get_phy_ftile_rdy_stat(p_rst);

		if (!complete) {
			nthw_os_wait_usec(100);

		} else {
			NT_LOG(DBG, NTHW, "%s: PHY FTILE ready, margin to timeout %u",
				p_adapter_id_str, timeout);
		}

		timeout--;

		if (timeout == 0) {
			NT_LOG(ERR, NTHW, "%s: %s: Timeout waiting for PHY FTILE ready",
				p_adapter_id_str, __func__);
			return -1;
		}
	} while (!complete);

	return 0;
}

static int nthw_fpga_rst9574_wait_phy_ftile_rst_done(struct fpga_info_s *p_fpga_info,
	struct nthw_fpga_rst_nt400dxx *p_rst)
{
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	uint32_t complete;
	uint32_t timeout;

	/* 5: wait until PHY_FTILE reset done */
	NT_LOG(DBG, NTHW, "%s: %s: PHY FTILE RESET done", p_adapter_id_str, __func__);
	timeout = 50000;/* initial timeout must be set to 5 sec. */

	do {
		complete = nthw_fpga_rst9574_get_phy_ftile_rst_done_stat(p_rst);

		if (!complete)
			nthw_os_wait_usec(100);

		timeout--;

		if (timeout == 0) {
			NT_LOG(ERR, NTHW, "%s: %s: Timeout waiting for PHY FTILE RESET to be done",
				p_adapter_id_str, __func__);
			return -1;
		}
	} while (!complete);

	return 0;
}

static int nthw_fpga_rst9574_product_reset(struct fpga_info_s *p_fpga_info,
	struct nthw_fpga_rst_nt400dxx *p_rst)
{
	RTE_ASSERT(p_fpga_info);
	RTE_ASSERT(p_rst);

	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	int res = -1;

	/* (0) Reset all domains / modules except peripherals: */
	NT_LOG(DBG, NTHW, "%s: %s: RST defaults", p_adapter_id_str, __func__);
	nthw_fpga_rst9574_set_default_rst_values(p_rst);

	/*
	 * Wait a while before waiting for deasserting ddr4 reset
	 */
	nthw_os_wait_usec(2000);

	/* (1) De-RTE_ASSERT DDR4 reset: */
	NT_LOG(DBG, NTHW, "%s: %s: De-asserting DDR4 reset", p_adapter_id_str, __func__);
	nthw_fpga_rst9574_ddr4_rst(p_rst, 0);

	/*
	 * Wait a while before waiting for calibration complete, since calibration complete
	 * is true while ddr4 is in reset
	 */
	nthw_os_wait_usec(2000);

	/* (2) Wait until DDR4 calibration complete */
	res = nthw_fpga_rst9574_wait_ddr4_calibration_complete(p_fpga_info, p_rst);

	if (res)
		return res;

	/* (3) Set DDR4 calib complete latched bits: */
	nthw_fpga_rst9574_set_ddr4_calib_complete_latch(p_rst, 1);

	/* Wait for phy to settle.*/
	nthw_os_wait_usec(20000);

	/* (4) Ensure all latched status bits are still set: */
	if (!nthw_fpga_rst9574_get_ddr4_calib_complete_latch(p_rst)) {
		NT_LOG(ERR, NTHW, "%s: %s: DDR4 calibration complete has toggled",
			p_adapter_id_str, __func__);
	}

	int32_t tries = 0;
	bool success = false;

	do {
		/* Only wait for ftile rst done if ftile is indeed in reset. */
		if (nthw_fpga_rst9574_get_phy_ftile_rst(p_rst)) {
			/* (5) Wait until PHY_FTILE reset done */
			NT_LOG(DBG, NTHW, "%s: %s: Wait until PHY_FTILE reset done",
				p_adapter_id_str, __func__);
			res = nthw_fpga_rst9574_wait_phy_ftile_rst_done(p_fpga_info, p_rst);

			if (res)
				return res;
		}

		/* (6) De-RTE_ASSERT PHY_FTILE reset: */
		NT_LOG(DBG, NTHW, "%s: %s: De-asserting PHY_FTILE reset", p_adapter_id_str,
			__func__);
		nthw_fpga_rst9574_phy_ftile_rst(p_rst, 0);

		nthw_os_wait_usec(10000);
		/* (7) Wait until PHY_FTILE ready */
		if (nthw_fpga_rst9574_wait_phy_ftile_rdy(p_fpga_info, p_rst) != -1) {
			success = true;
			continue;
		}

		if (++tries >= 5) {
			/* Give up */
			NT_LOG(ERR, NTHW, "%s: %s: PHY_TILE not ready after %u attempts",
				p_adapter_id_str, __func__, tries);
			return res;
		}

		/* Try to recover with as little effort as possible */
		nthw_phy_tile_t *p_phy_tile = p_fpga_info->mp_nthw_agx.p_phy_tile;

		for (uint8_t i = 0; i < nthw_phy_tile_get_no_intfs(p_phy_tile); i++) {
			/* Also consider TX_PLL_LOCKED == 0 */
			if (!nthw_phy_tile_get_port_status_tx_lanes_stable(p_phy_tile, i)) {
				success = false;
				NT_LOG(WRN, NTHW,
					"%s: %s: PHY_TILE Intf %u TX_LANES_STABLE == FALSE",
					p_adapter_id_str, __func__, i);

				/* Active low */
				nthw_phy_tile_set_port_config_rst(p_phy_tile, i, 0);
				int32_t count = 1000;

				do {
					nthw_os_wait_usec(1000);
				} while (!nthw_phy_tile_get_port_status_reset_ack(p_phy_tile, i) &&
					(--count > 0));

				if (count <= 0) {
					NT_LOG(ERR, NTHW, "%s: %s: IntfNo %u: "
					"Time-out waiting for PortStatusResetAck",
					p_adapter_id_str, __func__, i);
				}

				/* Active low */
				nthw_phy_tile_set_port_config_rst(p_phy_tile, i, 1);
				nthw_os_wait_usec(20000);
			}
		}

	} while (!success);

	/* (8) De-RTE_ASSERT SYS reset: */
	NT_LOG(DBG, NTHW, "%s: %s: De-asserting SYS reset", p_adapter_id_str, __func__);
	nthw_fpga_rst9574_sys_rst(p_rst, 0);

	return 0;
}

static int nthw_fpga_rst9574_init(struct fpga_info_s *p_fpga_info,
	struct nthw_fpga_rst_nt400dxx *p_rst)
{
	RTE_ASSERT(p_fpga_info);
	RTE_ASSERT(p_rst);
	int res = nthw_fpga_rst9574_product_reset(p_fpga_info, p_rst);

	return res;
}

static struct rst9574_ops rst9574_ops = {
	.nthw_fpga_rst9574_init = nthw_fpga_rst9574_init,
	.nthw_fpga_rst9574_setup = nthw_fpga_rst9574_setup,
};

void rst9574_ops_init(void)
{
	register_rst9574_ops(&rst9574_ops);
}
