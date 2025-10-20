/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"
#include "nthw_fpga.h"

#include "ntnic_mod_reg.h"

static const uint8_t si5338_u23_i2c_addr_7bit = 0x70;
static const uint8_t si5340_u23_i2c_addr_7bit = 0x74;

/*
 * Wait until DDR4 PLL LOCKED
 */
static int nthw_fpga_rst_nt200a0x_wait_ddr4_pll_locked(nthw_fpga_t *p_fpga,
	const struct nthw_fpga_rst_nt200a0x *p)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	uint32_t locked;
	uint32_t retrycount = 5;
	uint32_t timeout = 50000;	/* initial timeout must be set to 5 sec. */
	/* 14: wait until DDR4 PLL LOCKED */
	NT_LOG(DBG, NTHW, "%s: Waiting for DDR4 PLL to lock", p_adapter_id_str);

	/*
	 * The following retry count gives a total timeout of 1 * 5 + 5 * 8 = 45sec
	 * It has been observed that at least 21sec can be necessary
	 */
	while (true) {
		int locked =
			nthw_field_wait_set_any32(p->mp_fld_stat_ddr4_pll_locked, timeout, 100);

		if (locked == 0) {
			break;

		} else {
			NT_LOG(DBG, NTHW, "%s: Waiting for DDR4 PLL to lock - timeout",
				p_adapter_id_str);

			if (retrycount <= 0) {
				NT_LOG(ERR, NTHW, "%s: Waiting for DDR4 PLL to lock failed (%d)",
					p_adapter_id_str, locked);
				break;
			}

			nthw_field_set_flush(p->mp_fld_rst_ddr4);	/* Reset DDR PLL */
			nthw_field_clr_flush(p->mp_fld_rst_ddr4);	/* Reset DDR PLL */
			retrycount--;
			timeout = 80000;/* Increase timeout for second attempt to 8 sec. */
		}
	}

	NT_LOG(DBG, NTHW, "%s: Waiting for DDR4 MMCM to lock", p_adapter_id_str);
	locked = nthw_field_wait_set_any32(p->mp_fld_stat_ddr4_mmcm_locked, -1, -1);

	if (locked != 0) {
		NT_LOG(ERR, NTHW, "%s: Waiting for DDR4 MMCM to lock failed (%d)",
			p_adapter_id_str, locked);
		return -1;
	}

	if (true && p->mp_fld_stat_tsm_ref_mmcm_locked) {
		NT_LOG(DBG, NTHW, "%s: Waiting for TSM REF MMCM to lock", p_adapter_id_str);
		locked = nthw_field_wait_set_any32(p->mp_fld_stat_tsm_ref_mmcm_locked, -1, -1);

		if (locked != 0) {
			NT_LOG(ERR, NTHW, "%s: Waiting for TSM REF MMCM to lock failed (%d)",
				p_adapter_id_str, locked);
			return -1;
		}
	}

	/* 10: Clear all MMCM/PLL lock sticky bits before testing them */
	NT_LOG(DBG, NTHW, "%s: Clear sticky MMCM unlock bits", p_adapter_id_str);
	nthw_field_update_register(p->mp_fld_sticky_ptp_mmcm_unlocked);
	/* Clear all sticky bits */
	nthw_field_set_flush(p->mp_fld_sticky_ptp_mmcm_unlocked);
	nthw_field_set_flush(p->mp_fld_sticky_ts_mmcm_unlocked);
	nthw_field_set_flush(p->mp_fld_sticky_ddr4_mmcm_unlocked);
	nthw_field_set_flush(p->mp_fld_sticky_ddr4_pll_unlocked);
	nthw_field_set_flush(p->mp_fld_sticky_core_mmcm_unlocked);

	if (p->mp_fld_sticky_tsm_ref_mmcm_unlocked)
		nthw_field_set_flush(p->mp_fld_sticky_tsm_ref_mmcm_unlocked);

	if (p->mp_fld_sticky_pci_sys_mmcm_unlocked)
		nthw_field_set_flush(p->mp_fld_sticky_pci_sys_mmcm_unlocked);

	/* 11: Ensure sticky bits are not unlocked except PTP MMCM and TS MMCM */
	if (nthw_field_get_updated(p->mp_fld_sticky_ddr4_mmcm_unlocked)) {
		NT_LOG(ERR, NTHW, "%s: get_sticky_ddr4_mmcm_unlocked() returned true",
			p_adapter_id_str);
	}

	if (nthw_field_get_updated(p->mp_fld_sticky_ddr4_pll_unlocked)) {
		NT_LOG(ERR, NTHW, "%s: get_sticky_ddr4_pll_unlocked() returned true",
			p_adapter_id_str);
	}

	return 0;
}

/*
 * Wait for SDRAM controller has been calibrated - On some adapters we have seen
 * calibration time of 2.3 seconds
 */
static int nthw_fpga_rst_nt200a0x_wait_sdc_calibrated(nthw_fpga_t *p_fpga,
	const struct nthw_fpga_rst_nt200a0x *p)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_sdc_t *p_nthw_sdc = NULL;
	const int n_retry_cnt_max = 5;
	int n_retry_cnt;
	int res;

	res = nthw_sdc_init(NULL, p_fpga, 0);	/* probe for module */

	if (res == 0) {
		p_nthw_sdc = nthw_sdc_new();

		if (p_nthw_sdc) {
			res = nthw_sdc_init(p_nthw_sdc, p_fpga, 0);

			if (res) {
				NT_LOG_DBGX(ERR, NTHW, "%s: SDC init failed: res=%d",
					p_adapter_id_str, res);
				nthw_sdc_delete(p_nthw_sdc);
				p_nthw_sdc = NULL;
				return -1;
			}

		} else {
			nthw_sdc_delete(p_nthw_sdc);
			p_nthw_sdc = NULL;
		}

	} else {
		NT_LOG(DBG, NTHW, "%s: No SDC found", p_adapter_id_str);
	}

	n_retry_cnt = 0;
	res = -1;

	while ((res != 0) && (n_retry_cnt <= n_retry_cnt_max)) {
		/* wait until DDR4 PLL LOCKED */
		res = nthw_fpga_rst_nt200a0x_wait_ddr4_pll_locked(p_fpga, p);

		if (res == 0) {
			if (p_nthw_sdc) {
				/*
				 * Wait for SDRAM controller has been calibrated
				 * On some adapters we have seen calibration time of 2.3 seconds
				 */
				NT_LOG(DBG, NTHW, "%s: Waiting for SDRAM to calibrate",
					p_adapter_id_str);
				res = nthw_sdc_wait_states(p_nthw_sdc, 10000, 1000);
				{
					uint64_t n_result_mask;
					int n_state_code =
						nthw_sdc_get_states(p_nthw_sdc, &n_result_mask);
					(void)n_state_code;
					NT_LOG(DBG, NTHW,
						"%s: SDRAM state=0x%08lX state_code=%d retry=%d code=%d",
						p_adapter_id_str, n_result_mask, n_state_code,
						n_retry_cnt, res);
				}

				if (res == 0)
					break;
			}

			if (n_retry_cnt >= n_retry_cnt_max) {
				uint64_t n_result_mask;
				int n_state_code = nthw_sdc_get_states(p_nthw_sdc, &n_result_mask);
				(void)n_state_code;

				NT_LOG(DBG, NTHW,
					"%s: SDRAM state=0x%08lX state_code=%d retry=%d code=%d",
					p_adapter_id_str, n_result_mask, n_state_code, n_retry_cnt,
					res);

				if (res != 0) {
					NT_LOG(ERR, NTHW,
						"%s: Timeout waiting for SDRAM controller calibration",
						p_adapter_id_str);
				}
			}
		}

		/*
		 * SDRAM controller is not calibrated with DDR4 ram blocks:
		 * reset DDR and perform calibration retry
		 */
		nthw_field_set_flush(p->mp_fld_rst_ddr4);	/* Reset DDR PLL */
		nthw_os_wait_usec(100);
		nthw_field_clr_flush(p->mp_fld_rst_ddr4);

		n_retry_cnt++;
	}

	nthw_sdc_delete(p_nthw_sdc);

	return res;
}

static int nthw_fpga_rst_nt200a0x_reset(nthw_fpga_t *p_fpga,
	const struct nthw_fpga_rst_nt200a0x *p)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	const fpga_info_t *const p_fpga_info = p_fpga->p_fpga_info;

	const int n_fpga_product_id = p->mn_fpga_product_id;
	const int n_fpga_version = p->mn_fpga_version;
	const int n_fpga_revision = p->mn_fpga_revision;
	const int n_hw_id = p_fpga_info->nthw_hw_info.hw_id;
	int locked;
	int res = -1;

	NT_LOG_DBGX(DBG, NTHW, "%s: FPGA reset sequence: FPGA %04d-%02d-%02d @ HWId%d",
		p_adapter_id_str, n_fpga_product_id, n_fpga_version, n_fpga_revision,
		n_hw_id);
	RTE_ASSERT(n_fpga_product_id == p_fpga->mn_product_id);

	/*
	 * Reset all domains / modules except peripherals
	 * Set default reset values to ensure that all modules are reset correctly
	 * no matter if nic has been powercycled or ntservice has been reloaded
	 */

	/*
	 * reset to defaults
	 * 1: Reset all domains
	 */
	NT_LOG(DBG, NTHW, "%s: RST defaults", p_adapter_id_str);

	nthw_field_update_register(p->mp_fld_rst_sys);
	nthw_field_set_flush(p->mp_fld_rst_sys);

	if (p->mp_fld_rst_tmc)
		nthw_field_set_flush(p->mp_fld_rst_tmc);

	nthw_field_set_flush(p->mp_fld_rst_rpp);
	nthw_field_set_flush(p->mp_fld_rst_ddr4);	/* 0x07 3 banks */
	nthw_field_set_flush(p->mp_fld_rst_sdc);

	/* Reset port 0 and 1 in the following registers: */
	nthw_field_set_flush(p->mp_fld_rst_phy);/* 0x03 2 ports */

	if (p->mp_fld_rst_mac_rx)
		nthw_field_set_flush(p->mp_fld_rst_mac_rx);	/* 0x03 2 ports */

	if (p->mp_fld_rst_mac_tx)
		nthw_field_set_flush(p->mp_fld_rst_mac_tx);	/* 0x03 2 ports */

	if (p->mp_fld_rst_pcs_rx)
		nthw_field_set_flush(p->mp_fld_rst_pcs_rx);	/* 0x03 2 ports */

	if (p->mp_fld_rst_serdes_rx)
		nthw_field_set_flush(p->mp_fld_rst_serdes_rx);	/* 0x03 2 ports */

	if (p->mp_fld_rst_serdes_rx_datapath && p->mp_fld_rst_serdes_rx) {
		nthw_field_set_flush(p->mp_fld_rst_serdes_rx_datapath);
		nthw_field_clr_flush(p->mp_fld_rst_serdes_rx);
	}

	if (p->mp_fld_rst_serdes_tx)
		nthw_field_set_flush(p->mp_fld_rst_serdes_tx);

	nthw_field_set_flush(p->mp_fld_rst_ptp);
	nthw_field_set_flush(p->mp_fld_rst_ts);
	nthw_field_set_flush(p->mp_fld_rst_sys_mmcm);
	nthw_field_set_flush(p->mp_fld_rst_core_mmcm);
	nthw_field_set_flush(p->mp_fld_rst_ptp_mmcm);
	nthw_field_set_flush(p->mp_fld_rst_ts_mmcm);

	if (true && p->mp_fld_rst_tsm_ref_mmcm)
		nthw_field_set_flush(p->mp_fld_rst_tsm_ref_mmcm);

	/* Write all changes to register */
	nthw_field_flush_register(p->mp_fld_rst_sys);

	/*
	 * 2: Force use of 50 MHz reference clock for timesync;
	 * NOTE: From 9508-05-18 this is a 20 MHz clock
	 */
	NT_LOG(DBG, NTHW, "%s: Setting TS CLK SEL OVERRIDE", p_adapter_id_str);
	nthw_field_update_register(p->mp_fld_ctrl_ts_clk_sel_override);
	nthw_field_set_flush(p->mp_fld_ctrl_ts_clk_sel_override);

	NT_LOG(DBG, NTHW, "%s: Setting TS CLK SEL", p_adapter_id_str);
	nthw_field_update_register(p->mp_fld_ctrl_ts_clk_sel);
	nthw_field_set_flush(p->mp_fld_ctrl_ts_clk_sel);

	/* 4: De-RTE_ASSERT sys reset, CORE and SYS MMCM resets */
	NT_LOG(DBG, NTHW, "%s: De-asserting SYS, CORE and SYS MMCM resets", p_adapter_id_str);
	nthw_field_update_register(p->mp_fld_rst_sys);
	nthw_field_clr_flush(p->mp_fld_rst_sys);
	nthw_field_clr_flush(p->mp_fld_rst_sys_mmcm);
	nthw_field_clr_flush(p->mp_fld_rst_core_mmcm);

	/* 5: wait until CORE MMCM and SYS MMCM are LOCKED */
	NT_LOG(DBG, NTHW, "%s: Waiting for SYS MMCM to lock", p_adapter_id_str);
	locked = nthw_field_wait_set_any32(p->mp_fld_stat_sys_mmcm_locked, -1, -1);

	if (locked != 0) {
		NT_LOG(ERR, NTHW, "%s: Waiting for SYS MMCM to lock failed (%d)",
			p_adapter_id_str, locked);
	}

	NT_LOG(DBG, NTHW, "%s: Waiting for CORE MMCM to lock", p_adapter_id_str);
	locked = nthw_field_wait_set_any32(p->mp_fld_stat_core_mmcm_locked, -1, -1);

	if (locked != 0) {
		NT_LOG(ERR, NTHW, "%s: Waiting for CORE MMCM to lock failed (%d)",
			p_adapter_id_str, locked);
	}

	/*
	 * RAC RAB bus "flip/flip" reset second stage - new impl (ref RMT#37020)
	 * RAC/RAB init - SYS/CORE MMCM is locked - pull the remaining RAB buses out of reset
	 */
	{
		nthw_rac_t *p_nthw_rac = p_fpga_info->mp_nthw_rac;
		NT_LOG(DBG, NTHW, "%s: De-asserting remaining RAB buses", p_adapter_id_str);
		nthw_rac_rab_init(p_nthw_rac, 0);
	}

	if (true && p->mp_fld_rst_tsm_ref_mmcm) {
		NT_LOG(DBG, NTHW, "%s: De-asserting TSM REF MMCM", p_adapter_id_str);
		nthw_field_clr_flush(p->mp_fld_rst_tsm_ref_mmcm);

		if (p->mp_fld_stat_tsm_ref_mmcm_locked) {
			NT_LOG(DBG, NTHW, "%s: Waiting for TSM REF MMCM to lock",
				p_adapter_id_str);
			locked = nthw_field_wait_set_any32(p->mp_fld_stat_tsm_ref_mmcm_locked, -1,
					-1);

			if (locked != 0) {
				NT_LOG(ERR, NTHW,
					"%s: Waiting for TSM REF MMCM to lock failed (%d)",
					p_adapter_id_str, locked);
			}
		}
	}

	NT_LOG(DBG, NTHW, "%s: De-asserting all PHY resets", p_adapter_id_str);
	nthw_field_update_register(p->mp_fld_rst_phy);
	nthw_field_clr_flush(p->mp_fld_rst_phy);

	/*
	 * 8: De-RTE_ASSERT reset for remaining domains/modules resets except
	 * TS, PTP, PTP_MMCM and TS_MMCM
	 */
	NT_LOG(DBG, NTHW, "%s: De-asserting TMC RST", p_adapter_id_str);

	if (p->mp_fld_rst_tmc) {
		nthw_field_update_register(p->mp_fld_rst_tmc);
		nthw_field_clr_flush(p->mp_fld_rst_tmc);
	}

	NT_LOG(DBG, NTHW, "%s: De-asserting RPP RST", p_adapter_id_str);
	nthw_field_update_register(p->mp_fld_rst_rpp);
	nthw_field_clr_flush(p->mp_fld_rst_rpp);

	NT_LOG(DBG, NTHW, "%s: De-asserting DDR4 RST", p_adapter_id_str);
	nthw_field_update_register(p->mp_fld_rst_ddr4);
	nthw_field_clr_flush(p->mp_fld_rst_ddr4);

	NT_LOG(DBG, NTHW, "%s: De-asserting SDC RST", p_adapter_id_str);
	nthw_field_update_register(p->mp_fld_rst_sdc);
	nthw_field_clr_flush(p->mp_fld_rst_sdc);

	/* NOTE: 9522 implements PHY10G_QPLL reset and lock at this stage in mac_rx_rst() */
	NT_LOG(DBG, NTHW, "%s: De-asserting MAC RX RST", p_adapter_id_str);

	if (p->mp_fld_rst_mac_rx) {
		nthw_field_update_register(p->mp_fld_rst_mac_rx);
		nthw_field_clr_flush(p->mp_fld_rst_mac_rx);
	}

	/* await until DDR4 PLL LOCKED and SDRAM controller has been calibrated */
	res = nthw_fpga_rst_nt200a0x_wait_sdc_calibrated(p_fpga, p);

	if (res) {
		NT_LOG(ERR, NTHW,
			"%s: nthw_fpga_rst_nt200a0x_wait_sdc_calibrated() returned true",
			p_adapter_id_str);
		return -1;
	}

	if (nthw_field_get_updated(p->mp_fld_sticky_core_mmcm_unlocked)) {
		NT_LOG(ERR, NTHW, "%s: get_sticky_core_mmcm_unlocked() returned true",
			p_adapter_id_str);
		return -1;
	}

	if (p->mp_fld_sticky_pci_sys_mmcm_unlocked &&
		nthw_field_get_updated(p->mp_fld_sticky_pci_sys_mmcm_unlocked)) {
		NT_LOG(ERR, NTHW, "%s: get_sticky_pci_sys_mmcm_unlocked() returned true",
			p_adapter_id_str);
		return -1;
	}

	/*
	 * Timesync/PTP reset sequence
	 * De-RTE_ASSERT TS_MMCM reset
	 */
	NT_LOG(DBG, NTHW, "%s: De-asserting TS MMCM RST", p_adapter_id_str);
	nthw_field_clr_flush(p->mp_fld_rst_ts_mmcm);

	/* Wait until TS_MMCM LOCKED (NT_RAB0_REG_P9508_RST9508_STAT_TS_MMCM_LOCKED=1); */
	NT_LOG(DBG, NTHW, "%s: Waiting for TS MMCM to lock", p_adapter_id_str);
	locked = nthw_field_wait_set_any32(p->mp_fld_stat_ts_mmcm_locked, -1, -1);

	if (locked != 0) {
		NT_LOG(ERR, NTHW, "%s: Waiting for TS MMCM to lock failed (%d)",
			p_adapter_id_str, locked);
	}

	NT_LOG(DBG, NTHW, "%s: Calling clear_sticky_mmcm_unlock_bits()", p_adapter_id_str);
	nthw_field_update_register(p->mp_fld_sticky_ptp_mmcm_unlocked);
	/* Clear all sticky bits */
	nthw_field_set_flush(p->mp_fld_sticky_ptp_mmcm_unlocked);
	nthw_field_set_flush(p->mp_fld_sticky_ts_mmcm_unlocked);
	nthw_field_set_flush(p->mp_fld_sticky_ddr4_mmcm_unlocked);
	nthw_field_set_flush(p->mp_fld_sticky_ddr4_pll_unlocked);
	nthw_field_set_flush(p->mp_fld_sticky_core_mmcm_unlocked);

	if (p->mp_fld_sticky_tsm_ref_mmcm_unlocked)
		nthw_field_set_flush(p->mp_fld_sticky_tsm_ref_mmcm_unlocked);

	if (p->mp_fld_sticky_pci_sys_mmcm_unlocked)
		nthw_field_set_flush(p->mp_fld_sticky_pci_sys_mmcm_unlocked);

	/* De-RTE_ASSERT TS reset bit */
	NT_LOG(DBG, NTHW, "%s: De-asserting TS RST", p_adapter_id_str);
	nthw_field_clr_flush(p->mp_fld_rst_ts);

	if (nthw_field_get_updated(p->mp_fld_sticky_ts_mmcm_unlocked)) {
		NT_LOG(ERR, NTHW, "%s: get_sticky_ts_mmcm_unlocked() returned true",
			p_adapter_id_str);
		return -1;
	}

	if (nthw_field_get_updated(p->mp_fld_sticky_ddr4_mmcm_unlocked)) {
		NT_LOG(ERR, NTHW, "%s: get_sticky_ddr4_mmcm_unlocked() returned true",
			p_adapter_id_str);
		return -1;
	}

	if (nthw_field_get_updated(p->mp_fld_sticky_ddr4_pll_unlocked)) {
		NT_LOG(ERR, NTHW, "%s: get_sticky_ddr4_pll_unlocked() returned true",
			p_adapter_id_str);
		return -1;
	}

	if (nthw_field_get_updated(p->mp_fld_sticky_core_mmcm_unlocked)) {
		NT_LOG(ERR, NTHW, "%s: get_sticky_core_mmcm_unlocked() returned true",
			p_adapter_id_str);
		return -1;
	}

	if (p->mp_fld_sticky_pci_sys_mmcm_unlocked &&
		nthw_field_get_updated(p->mp_fld_sticky_pci_sys_mmcm_unlocked)) {
		NT_LOG(ERR, NTHW, "%s: get_sticky_pci_sys_mmcm_unlocked() returned true",
			p_adapter_id_str);
		return -1;
	}


	if (false) {
		/* Deassert PTP_MMCM */
		NT_LOG(DBG, NTHW, "%s: De-asserting PTP MMCM RST", p_adapter_id_str);
		nthw_field_clr_flush(p->mp_fld_rst_ptp_mmcm);

		/* Switch PTP MMCM sel to use ptp clk */
		NT_LOG(DBG, NTHW, "%s: Setting PTP MMCM CLK SEL", p_adapter_id_str);
		nthw_field_set_flush(p->mp_fld_ctrl_ptp_mmcm_clk_sel);

		/* Wait until TS_MMCM LOCKED (NT_RAB0_REG_P9508_RST9508_STAT_TS_MMCM_LOCKED=1); */
		NT_LOG(DBG, NTHW, "%s: Waiting for TS MMCM to re-lock", p_adapter_id_str);
		locked = nthw_field_wait_set_any32(p->mp_fld_stat_ts_mmcm_locked, -1, -1);

		if (locked != 0) {
			NT_LOG(ERR, NTHW, "%s: Waiting for TS MMCM to re-lock failed (%d)",
				p_adapter_id_str, locked);
		}
	}

	NT_LOG(DBG, NTHW, "%s: De-asserting PTP RST", p_adapter_id_str);
	nthw_field_clr_flush(p->mp_fld_rst_ptp);

	/* POWER staging introduced in 9508-05-09 and always for 9512 */
	if (n_fpga_product_id == 9508 && n_fpga_version <= 5 && n_fpga_revision <= 8) {
		NT_LOG(DBG, NTHW, "%s: No power staging", p_adapter_id_str);

	} else {
		NT_LOG(DBG, NTHW, "%s: Staging power", p_adapter_id_str);
		nthw_field_set_flush(p->mp_fld_power_pu_phy);	/* PHY power up */
		nthw_field_clr_flush(p->mp_fld_power_pu_nseb);	/* NSEB power down */
	}

	NT_LOG_DBGX(DBG, NTHW, "%s END", p_adapter_id_str);

	return 0;
}

static int nthw_fpga_rst_nt200a0x_init(struct fpga_info_s *p_fpga_info,
	struct nthw_fpga_rst_nt200a0x *p_rst)
{
	RTE_ASSERT(p_fpga_info);

	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	int res = -1;
	int n_si_labs_clock_synth_model = -1;
	uint8_t n_si_labs_clock_synth_i2c_addr = 0;
	nthw_fpga_t *p_fpga = NULL;

	p_fpga = p_fpga_info->mp_fpga;

	NT_LOG_DBGX(DBG, NTHW, "%s: RAB init/reset", p_adapter_id_str);
	nthw_rac_rab_reset(p_fpga_info->mp_nthw_rac);
	nthw_rac_rab_setup(p_fpga_info->mp_nthw_rac);

	res = nthw_fpga_iic_scan(p_fpga, 0, 0);
	if (res == -1) {
		NT_LOG(ERR, NTHW, "Failed to init iic instance %d - %d for NT200 (%d)", 0, 0, res);
		return -1;
	}

	res = nthw_fpga_iic_scan(p_fpga, 2, 3);
	if (res == -1) {
		NT_LOG(ERR, NTHW, "Failed to init iic instance %d - %d for NT200 (%d)", 2, 3, res);
		return -1;
	}

	/*
	 * Detect clock synth model
	 * check for NT200A02/NT200A01 HW-build2 - most commonly seen
	 */
	n_si_labs_clock_synth_i2c_addr = si5340_u23_i2c_addr_7bit;
	n_si_labs_clock_synth_model =
		nthw_fpga_silabs_detect(p_fpga, 0, n_si_labs_clock_synth_i2c_addr, 1);

	if (n_si_labs_clock_synth_model == -1) {
		/* check for old NT200A01 HW-build1 */
		n_si_labs_clock_synth_i2c_addr = si5338_u23_i2c_addr_7bit;
		n_si_labs_clock_synth_model =
			nthw_fpga_silabs_detect(p_fpga, 0, n_si_labs_clock_synth_i2c_addr, 255);

		if (n_si_labs_clock_synth_model == -1) {
			NT_LOG(ERR, NTHW, "%s: Failed to detect clock synth model (%d)",
				p_adapter_id_str, n_si_labs_clock_synth_model);
			return -1;
		}
	}

	p_rst->mn_si_labs_clock_synth_model = n_si_labs_clock_synth_model;
	p_rst->mn_si_labs_clock_synth_i2c_addr = n_si_labs_clock_synth_i2c_addr;
	p_rst->mn_hw_id = p_fpga_info->nthw_hw_info.hw_id;
	NT_LOG_DBGX(DBG, NTHW, "%s: Si%04d @ 0x%02x", p_adapter_id_str,
		p_rst->mn_si_labs_clock_synth_model, p_rst->mn_si_labs_clock_synth_i2c_addr);

	return res;
}

static struct rst_nt200a0x_ops rst_nt200a0x_ops = { .nthw_fpga_rst_nt200a0x_init =
		nthw_fpga_rst_nt200a0x_init,
		.nthw_fpga_rst_nt200a0x_reset =
			nthw_fpga_rst_nt200a0x_reset
};

void nthw_rst_nt200a0x_ops_init(void)
{
	NT_LOG(DBG, NTHW, "RST NT200A0X OPS INIT");
	nthw_reg_rst_nt200a0x_ops(&rst_nt200a0x_ops);
}
