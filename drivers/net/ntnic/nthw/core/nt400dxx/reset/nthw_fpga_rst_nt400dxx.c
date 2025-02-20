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

static int nthw_fpga_rst_nt400dxx_init(struct fpga_info_s *p_fpga_info)
{
	assert(p_fpga_info);
	return 0;
}

static int nthw_fpga_rst_nt400dxx_reset(struct fpga_info_s *p_fpga_info)
{
	assert(p_fpga_info);
	return 0;
}

static struct rst_nt400dxx_ops rst_nt400dxx_ops = {
	.nthw_fpga_rst_nt400dxx_init = nthw_fpga_rst_nt400dxx_init,
	.nthw_fpga_rst_nt400dxx_reset = nthw_fpga_rst_nt400dxx_reset
};

void rst_nt400dxx_ops_init(void)
{
	register_rst_nt400dxx_ops(&rst_nt400dxx_ops);
}
