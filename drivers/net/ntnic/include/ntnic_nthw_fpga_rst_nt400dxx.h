/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#ifndef __NTNIC_NTHW_FPGA_RST_NT400DXX_H__
#define __NTNIC_NTHW_FPGA_RST_NT400DXX_H__

#include "nthw_drv.h"
#include "nthw_fpga_model.h"

struct nthw_fpga_rst_nt400dxx {
	int n_fpga_product_id;
	int n_fpga_version;
	int n_fpga_revision;
	int n_hw_id;
	bool mb_is_nt400d11;

	nthw_field_t *p_fld_rst_sys;
	nthw_field_t *p_fld_rst_ddr4;
	nthw_field_t *p_fld_rst_phy_ftile;

	nthw_field_t *p_fld_stat_ddr4_calib_complete;
	nthw_field_t *p_fld_stat_phy_ftile_rst_done;
	nthw_field_t *p_fld_stat_phy_ftile_rdy;

	nthw_field_t *p_fld_latch_ddr4_calib_complete;
	nthw_field_t *p_fld_latch_phy_ftile_rst_done;
	nthw_field_t *p_fld_latch_phy_ftile_rdy;
};

typedef struct nthw_fpga_rst_nt400dxx nthw_fpga_rst_nt400dxx_t;

#endif	/* __NTHW_FPGA_RST_NT400DXX_H__ */
