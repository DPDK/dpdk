/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Napatech A/S
 */

#include "ntnic_mod_reg.h"


static int nthw_fpga_rst9569_setup(void)
{
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
