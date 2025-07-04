/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTNIC_NTHW_FPGA_RST_NT200A0X_H__
#define __NTNIC_NTHW_FPGA_RST_NT200A0X_H__

#include "nthw_drv.h"
#include "nthw_fpga_model.h"

struct nthw_fpga_rst_nt200a0x {
	int mn_fpga_product_id;
	int mn_fpga_version;
	int mn_fpga_revision;

	int mn_hw_id;

	int mn_si_labs_clock_synth_model;
	uint8_t mn_si_labs_clock_synth_i2c_addr;

	nthw_field_t *mp_fld_rst_sys;
	nthw_field_t *mp_fld_rst_sys_mmcm;
	nthw_field_t *mp_fld_rst_core_mmcm;
	nthw_field_t *mp_fld_rst_rpp;
	nthw_field_t *mp_fld_rst_ddr4;
	nthw_field_t *mp_fld_rst_sdc;
	nthw_field_t *mp_fld_rst_phy;
	nthw_field_t *mp_fld_rst_serdes_rx;
	nthw_field_t *mp_fld_rst_serdes_tx;
	nthw_field_t *mp_fld_rst_serdes_rx_datapath;
	nthw_field_t *mp_fld_rst_pcs_rx;
	nthw_field_t *mp_fld_rst_mac_rx;
	nthw_field_t *mp_fld_rst_mac_tx;
	nthw_field_t *mp_fld_rst_ptp;
	nthw_field_t *mp_fld_rst_ts;
	nthw_field_t *mp_fld_rst_ptp_mmcm;
	nthw_field_t *mp_fld_rst_ts_mmcm;
	nthw_field_t *mp_fld_rst_periph;
	nthw_field_t *mp_fld_rst_tsm_ref_mmcm;
	nthw_field_t *mp_fld_rst_tmc;

	/* CTRL register field pointers */
	nthw_field_t *mp_fld_ctrl_ts_clk_sel_override;
	nthw_field_t *mp_fld_ctrl_ts_clk_sel;
	nthw_field_t *mp_fld_ctrl_ts_clk_sel_ref;
	nthw_field_t *mp_fld_ctrl_ptp_mmcm_clk_sel;

	/* STAT register field pointers */
	nthw_field_t *mp_fld_stat_ddr4_mmcm_locked;
	nthw_field_t *mp_fld_stat_sys_mmcm_locked;
	nthw_field_t *mp_fld_stat_core_mmcm_locked;
	nthw_field_t *mp_fld_stat_ddr4_pll_locked;
	nthw_field_t *mp_fld_stat_ptp_mmcm_locked;
	nthw_field_t *mp_fld_stat_ts_mmcm_locked;
	nthw_field_t *mp_fld_stat_tsm_ref_mmcm_locked;

	/* STICKY register field pointers */
	nthw_field_t *mp_fld_sticky_ptp_mmcm_unlocked;
	nthw_field_t *mp_fld_sticky_ts_mmcm_unlocked;
	nthw_field_t *mp_fld_sticky_ddr4_mmcm_unlocked;
	nthw_field_t *mp_fld_sticky_ddr4_pll_unlocked;
	nthw_field_t *mp_fld_sticky_core_mmcm_unlocked;
	nthw_field_t *mp_fld_sticky_pci_sys_mmcm_unlocked;
	nthw_field_t *mp_fld_sticky_tsm_ref_mmcm_unlocked;

	/* POWER register field pointers */
	nthw_field_t *mp_fld_power_pu_phy;
	nthw_field_t *mp_fld_power_pu_nseb;

	void (*reset_serdes_rx)(struct nthw_fpga_rst_nt200a0x *p, uint32_t intf_no, uint32_t rst);
	void (*pcs_rx_rst)(struct nthw_fpga_rst_nt200a0x *p, uint32_t intf_no, uint32_t rst);
	void (*get_serdes_rx_rst)(struct nthw_fpga_rst_nt200a0x *p, uint32_t intf_no,
		uint32_t *p_set);
	void (*get_pcs_rx_rst)(struct nthw_fpga_rst_nt200a0x *p, uint32_t intf_no,
		uint32_t *p_set);
	bool (*is_rst_serdes_rx_datapath_implemented)(struct nthw_fpga_rst_nt200a0x *p);
};

typedef struct nthw_fpga_rst_nt200a0x nthw_fpga_rst_nt200a0x_t;

#endif	/* __NTHW_FPGA_RST_NT200A0X_H__ */
