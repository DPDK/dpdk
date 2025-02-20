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
	assert(p_fpga);
	assert(p);

	return 0;
};



static int nthw_fpga_rst9574_init(struct fpga_info_s *p_fpga_info,
	struct nthw_fpga_rst_nt400dxx *p_rst)
{
	assert(p_fpga_info);
	assert(p_rst);
	int res = -1;

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
