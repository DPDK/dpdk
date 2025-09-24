/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"
#include "nthw_drv.h"
#include "nthw_register.h"
#include "nthw_fpga.h"
#include "nthw_hif.h"
#include "ntnic_mod_reg.h"
#include "nthw_igam.h"

static int nthw_fpga_rst_nt400dxx_init(struct fpga_info_s *p_fpga_info)
{
	RTE_ASSERT(p_fpga_info);
	int res = -1;
	nthw_fpga_t *p_fpga = NULL;

	p_fpga = p_fpga_info->mp_fpga;

	nthw_hif_t *p_nthw_hif = nthw_hif_new();
	res = nthw_hif_init(p_nthw_hif, p_fpga, 0);

	if (res == 0)
		NT_LOG(DBG, NTHW, "%s: Hif module found", p_fpga_info->mp_adapter_id_str);

	/* (A) Test HIF clock is running by performing simple write/read test of HIF registers */
	const uint32_t test_pattern[2] = { 0x11223344, 0x55667788 };

	for (uint8_t i = 0; i < 2; ++i) {
		uint32_t test_data = 0;
		nthw_hif_write_test_reg(p_nthw_hif, i, test_pattern[i]);
		nthw_hif_read_test_reg(p_nthw_hif, i, &test_data);

		if (test_data != test_pattern[i]) {
			NT_LOG(ERR,
				NTHW,
				"%s: %s: Test sys 250 clock failed",
				p_fpga_info->mp_adapter_id_str,
				__func__);
			return -1;
		}
	}

	nthw_hif_delete(p_nthw_hif);

	/* (b) Init RAB0 */
	nthw_rac_rab_init(p_fpga_info->mp_nthw_rac, 0x7);
	nthw_rac_rab_init(p_fpga_info->mp_nthw_rac, 0x6);

	/* Create PRM */
	p_fpga_info->mp_nthw_agx.p_prm = nthw_prm_nt400dxx_new();
	res = nthw_prm_nt400dxx_init(p_fpga_info->mp_nthw_agx.p_prm, p_fpga, 0);

	if (res != 0)
		return res;

	/* (b1) Reset platform. It is released later */
	nthw_prm_nt400dxx_platform_rst(p_fpga_info->mp_nthw_agx.p_prm, 1);
	nthw_os_wait_usec(10000);

	/* (C) Reset peripherals and release the reset */
	nthw_prm_nt400dxx_periph_rst(p_fpga_info->mp_nthw_agx.p_prm, 1);
	nthw_os_wait_usec(10000);
	nthw_prm_nt400dxx_periph_rst(p_fpga_info->mp_nthw_agx.p_prm, 0);
	nthw_os_wait_usec(10000);

	res = nthw_fpga_avr_probe(p_fpga, 0);

	if (res != 0)
		return res;

	/* Create I2C */
	p_fpga_info->mp_nthw_agx.p_i2cm = nthw_i2cm_new();
	res = nthw_i2cm_init(p_fpga_info->mp_nthw_agx.p_i2cm, p_fpga, 0);

	if (res != 0)
		return res;

	/* Create pca9849 i2c mux */
	p_fpga_info->mp_nthw_agx.p_pca9849 = nthw_pca9849_new();
	res = nthw_pca9849_init(p_fpga_info->mp_nthw_agx.p_pca9849,
			p_fpga_info->mp_nthw_agx.p_i2cm,
			0x71);

	if (res != 0)
		return res;

	/* Create PCAL6416A I/O expander for controlling Nim */
	p_fpga_info->mp_nthw_agx.p_io_nim = nthw_pcal6416a_new();
	res = nthw_pcal6416a_init(p_fpga_info->mp_nthw_agx.p_io_nim,
			p_fpga_info->mp_nthw_agx.p_i2cm,
			0x20,
			p_fpga_info->mp_nthw_agx.p_pca9849,
			3);

	if (res != 0)
		return res;

	/* Create PCAL6416A I/O expander for controlling TS */
	p_fpga_info->mp_nthw_agx.p_io_ts = nthw_pcal6416a_new();
	res = nthw_pcal6416a_init(p_fpga_info->mp_nthw_agx.p_io_ts,
			p_fpga_info->mp_nthw_agx.p_i2cm,
			0x20,
			p_fpga_info->mp_nthw_agx.p_pca9849,
			2);

	if (res != 0)
		return res;

	/*
	 * disable pps out
	 * TS_EXT1_OUT_EN : Low
	 */
	nthw_pcal6416a_write(p_fpga_info->mp_nthw_agx.p_io_ts, 0, 0);

	/*
	 * Enable DC termination
	 * TS_EXT1_TERM_DC_E : High
	 */
	nthw_pcal6416a_write(p_fpga_info->mp_nthw_agx.p_io_ts, 3, 1);

	/*
	 * enable 50 ohm termination
	 * TS_EXT1_TERM50_DIS : low
	 */
	nthw_pcal6416a_write(p_fpga_info->mp_nthw_agx.p_io_ts, 2, 0);

	/*
	 * Normal Threshold
	 * TS_EXT1_LOW_THRESHOLD: Low
	 */
	nthw_pcal6416a_write(p_fpga_info->mp_nthw_agx.p_io_ts, 1, 0);

	/*
	 * Select FPGA I/O - we want to use TSM Bresenham for time adjustments against PPS time
	 * reference TS_EXT_ZL30733_EnB: High
	 */
	nthw_pcal6416a_write(p_fpga_info->mp_nthw_agx.p_io_ts, 8, 1);

	/* Create LED driver */
	p_fpga_info->mp_nthw_agx.p_pca9532_led = nthw_pca9532_new();
	res = nthw_pca9532_init(p_fpga_info->mp_nthw_agx.p_pca9532_led,
			p_fpga_info->mp_nthw_agx.p_i2cm,
			0x60,
			p_fpga_info->mp_nthw_agx.p_pca9849,
			3);

	if (res != 0)
		return res;

	for (uint8_t i = 0; i < 16; i++)
		nthw_pca9532_set_led_on(p_fpga_info->mp_nthw_agx.p_pca9532_led, i, false);

	/* Enable power supply to NIMs */
	nthw_pcal6416a_write(p_fpga_info->mp_nthw_agx.p_io_nim, 8, 1);
	nthw_os_wait_usec(100000);/* 100ms */

	/* Check that power supply turned on. Warn if it didn't. */
	uint8_t port_power_ok;
	nthw_pcal6416a_read(p_fpga_info->mp_nthw_agx.p_io_nim, 9, &port_power_ok);

	if (port_power_ok != 1) {
		NT_LOG(ERR,
			NTHW,
			"Warning: Power supply for QSFP modules did not turn on - Fuse might be blown.");
		return -1;
	}

	NT_LOG(INF, NTHW, "Status of power supply for QSFP modules %u", port_power_ok);

	/* Create PCM */
	p_fpga_info->mp_nthw_agx.p_pcm = nthw_pcm_nt400dxx_new();
	res = nthw_pcm_nt400dxx_init(p_fpga_info->mp_nthw_agx.p_pcm, p_fpga, 0);

	if (res != 0)
		return res;

	return 0;
}

static int nthw_fpga_rst_nt400dxx_reset(struct fpga_info_s *p_fpga_info)
{
	RTE_ASSERT(p_fpga_info);

	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	nthw_fpga_t *p_fpga = NULL;
	int res = -1;

	p_fpga = p_fpga_info->mp_fpga;

	nthw_pcm_nt400dxx_t *p_pcm = p_fpga_info->mp_nthw_agx.p_pcm;
	nthw_prm_nt400dxx_t *p_prm = p_fpga_info->mp_nthw_agx.p_prm;

	NT_LOG(DBG, NTHW, "%s: %s: BEGIN", p_adapter_id_str, __PRETTY_FUNCTION__);

	/* Create Phy Tile module */
	p_fpga_info->mp_nthw_agx.p_phy_tile = nthw_phy_tile_new();

	if (nthw_phy_tile_init(p_fpga_info->mp_nthw_agx.p_phy_tile, p_fpga, 0)) {
		NT_LOG(ERR, NTHW, "%s: Failed to create Phy Tile Module", p_adapter_id_str);
		return -1;
	}

	nthw_phy_tile_t *p_phy_tile = p_fpga_info->mp_nthw_agx.p_phy_tile;

	nthw_igam_t *p_igam = nthw_igam_new();
	nthw_igam_init(p_igam, p_fpga, 0);

	if (p_igam) {
		if (!nthw_phy_tile_use_phy_tile_pll_check(p_fpga_info->mp_nthw_agx.p_phy_tile)) {
			NT_LOG(DBG, NTHW, "%s: IGAM module present.", p_adapter_id_str);
			uint32_t data;

			p_fpga_info->mp_nthw_agx.p_igam = p_igam;

			/*
			 * (E) When the reference clock for the F-tile system PLL is started (and
			 * stable) it must be enabled using the Intel Global Avalon Memory-Mapped
			 * Mailbox (IGAM) module.
			 */

			nthw_igam_write(p_igam, 0xffff8, 0x90000000);

			/* (F) Check that the system PLL is ready. */
			for (int i = 1000; i >= 0; i--) {
				nthw_os_wait_usec(1000);
				data = nthw_igam_read(p_igam, 0xffff4);

				if (data == 0x80000000 || data == 0xA0000000) {
					NT_LOG(INF,
						NTHW,
						"%s: All enabled system PLLs are locked. Response: %#08x",
						p_adapter_id_str,
						data);
					break;
				}

				if (i == 0) {
					NT_LOG(ERR,
						NTHW,
						"%s: Timeout waiting for all system PLLs to lock. Response: %#08x",
						p_adapter_id_str,
						data);
					return -1;
				}
			}

		} else {
			nthw_igam_set_ctrl_forward_rst(p_igam,
				0);	/* Ensure that the Avalon bus is not */
			/* reset at every driver re-load. */
			nthw_os_wait_usec(1000000);
			NT_LOG(DBG, NTHW, "%s: IGAM module not used.", p_adapter_id_str);
		}

	} else {
		NT_LOG(DBG, NTHW, "%s: No IGAM module present.", p_adapter_id_str);
	}

	/*
	 * (G) Reset TS PLL and select Time-of-Day clock (tod_fpga_clk). Source is either
	 * from ZL clock device or SiTime tunable XO. NOTE that the PLL must be held in
	 * RESET during clock switchover.
	 */

	if (p_fpga_info->mp_nthw_agx.tcxo_present && p_fpga_info->mp_nthw_agx.tcxo_capable) {
		nthw_pcm_nt400dxx_set_ts_pll_recal(p_pcm, 1);
		nthw_os_wait_usec(1000);
		nthw_pcm_nt400dxx_set_ts_pll_recal(p_pcm, 0);
		nthw_os_wait_usec(1000);
	}

	/* (I) Wait for TS PLL locked. */
	for (int i = 1000; i >= 0; i--) {
		nthw_os_wait_usec(1000);

		if (nthw_pcm_nt400dxx_get_ts_pll_locked_stat(p_pcm))
			break;

		if (i == 0) {
			NT_LOG(ERR,
				NTHW,
				"%s: %s: Time out waiting for TS PLL locked",
				p_adapter_id_str,
				__func__);
			return -1;
		}
	}

	/* NT_RAB0_REG.PCM_NT400D1X.STAT.TS_PLL_LOCKED==1 */

	/* (J) Set latched TS PLL locked. */
	nthw_pcm_nt400dxx_set_ts_pll_locked_latch(p_pcm, 1);
	/* NT_RAB0_REG.PCM_NT400D1X.LATCH.TS_PLL_LOCKED=1 */

	/* (K) Ensure TS latched status bit is still set. */
	if (!nthw_pcm_nt400dxx_get_ts_pll_locked_stat(p_pcm)) {
		NT_LOG(ERR,
			NTHW,
			"%s: %s: TS latched status bit toggled",
			p_adapter_id_str,
			__func__);
		return -1;
	}

	/* NT_RAB0_REG.PCM_NT400D1X.LATCH.TS_PLL_LOCKED==1 */

	/*
	 * At this point all system clocks and TS clocks are running.
	 * Last thing to do before proceeding to product reset is to
	 * de-RTE_ASSERT the platform reset and enable the RAB buses.
	 */

	/* (K1) Force HIF soft reset. */
	nthw_hif_t *p_nthw_hif = nthw_hif_new();
	res = nthw_hif_init(p_nthw_hif, p_fpga, 0);

	if (res == 0)
		NT_LOG(DBG, NTHW, "%s: Hif module found", p_fpga_info->mp_adapter_id_str);

	nthw_os_wait_usec(1000);
	nthw_hif_force_soft_reset(p_nthw_hif);
	nthw_os_wait_usec(1000);
	nthw_hif_delete(p_nthw_hif);

	/* (L) De-RTE_ASSERT platform reset. */
	nthw_prm_nt400dxx_platform_rst(p_prm, 0);

	/*
	 * (M) Enable RAB1 and RAB2.
	 * NT_BAR_REG.RAC.RAB_INIT.RAB=0
	 */
	NT_LOG_DBGX(DBG, NTHW, "%s: RAB Init", p_adapter_id_str);
	nthw_rac_rab_init(p_fpga_info->mp_nthw_rac, 0);
	nthw_rac_rab_setup(p_fpga_info->mp_nthw_rac);

	NT_LOG_DBGX(DBG, NTHW, "%s: RAB Flush", p_adapter_id_str);
	nthw_rac_rab_flush(p_fpga_info->mp_nthw_rac);

	/*
	 * FPGAs with newer PHY_TILE versions must use PhyTile to check that system PLL is ready.
	 * It has been added here after consultations with ORA since the test must be performed
	 * after de-assertion of platform reset.
	 */

	if (nthw_phy_tile_use_phy_tile_pll_check(p_phy_tile)) {
		/* Ensure that the Avalon bus is not reset at every driver re-load. */
		nthw_phy_tile_set_sys_pll_forward_rst(p_phy_tile, 0);

		nthw_phy_tile_set_sys_pll_set_rdy(p_phy_tile, 0x07);
		NT_LOG_DBGX(DBG, NTHW, "%s: setSysPllSetRdy.", p_adapter_id_str);

		/* (F) Check that the system PLL is ready. */
		for (int i = 1000; i >= 0; i--) {
			nthw_os_wait_usec(1000);

			if (nthw_phy_tile_get_sys_pll_get_rdy(p_phy_tile) &&
				nthw_phy_tile_get_sys_pll_system_pll_lock(p_phy_tile)) {
				break;
			}

			if (i == 500) {
				nthw_phy_tile_set_sys_pll_force_rst(p_phy_tile, 1);
				nthw_os_wait_usec(1000);
				nthw_phy_tile_set_sys_pll_force_rst(p_phy_tile, 0);
				NT_LOG_DBGX(DBG,
					NTHW,
					"%s: setSysPllForceRst due to system PLL not ready.",
					p_adapter_id_str);
			}

			if (i == 0) {
				NT_LOG_DBGX(DBG,
					NTHW,
					"%s: Timeout waiting for all system PLLs to lock",
					p_adapter_id_str);
				return -1;
			}
		}

		nthw_os_wait_usec(100000);/* 100 ms */

		uint32_t fgt_enable = 0x0d;	/* FGT 0, 2 & 3 */
		nthw_phy_tile_set_sys_pll_en_ref_clk_fgt(p_phy_tile, fgt_enable);

		for (int i = 1000; i >= 0; i--) {
			nthw_os_wait_usec(1000);

			if (nthw_phy_tile_get_sys_pll_ref_clk_fgt_enabled(p_phy_tile) ==
				fgt_enable) {
				break;
			}

			if (i == 500) {
				nthw_phy_tile_set_sys_pll_force_rst(p_phy_tile, 1);
				nthw_os_wait_usec(1000);
				nthw_phy_tile_set_sys_pll_force_rst(p_phy_tile, 0);
				NT_LOG_DBGX(DBG,
					NTHW,
					"%s: setSysPllForceRst due to FGTs not ready.",
					p_adapter_id_str);
			}

			if (i == 0) {
				NT_LOG_DBGX(DBG,
					NTHW,
					"%s: Timeout waiting for FGTs to lock",
					p_adapter_id_str);
				return -1;
			}
		}
	}

	return 0;
}

static struct rst_nt400dxx_ops rst_nt400dxx_ops = {
	.nthw_fpga_rst_nt400dxx_init = nthw_fpga_rst_nt400dxx_init,
	.nthw_fpga_rst_nt400dxx_reset = nthw_fpga_rst_nt400dxx_reset
};

void rst_nt400dxx_ops_init(void)
{
	nthw_reg_rst_nt400dxx_ops(&rst_nt400dxx_ops);
}
