/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nthw_fpga.h"
#include "ntnic_mod_reg.h"


static int nthw_fpga_nt400dxx_init(struct fpga_info_s *p_fpga_info)
{
	assert(p_fpga_info);
	int res = -1;

	return res;
}

static struct nt400dxx_ops nt400dxx_ops = { .nthw_fpga_nt400dxx_init = nthw_fpga_nt400dxx_init };

void nt400dxx_ops_init(void)
{
	register_nt400dxx_ops(&nt400dxx_ops);
}
