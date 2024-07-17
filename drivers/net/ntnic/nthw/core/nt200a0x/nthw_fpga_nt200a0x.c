/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_fpga.h"
#include "ntnic_mod_reg.h"

static int nthw_fpga_nt200a0x_init(struct fpga_info_s *p_fpga_info)
{
	assert(p_fpga_info);

	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	int res = -1;

	bool included = true;

	/* reset specific */
	switch (p_fpga_info->n_fpga_prod_id) {
	case 9563:
		included = false;
		break;

	default:
		NT_LOG(ERR, NTHW, "%s: Unsupported FPGA product: %04d\n", p_adapter_id_str,
			p_fpga_info->n_fpga_prod_id);
		res = -1;
		break;
	}

	if (!included) {
		NT_LOG(ERR, NTHW, "%s: NOT INCLUDED FPGA product: %04d\n", p_adapter_id_str,
			p_fpga_info->n_fpga_prod_id);
		res = -1;
	}

	if (res) {
		NT_LOG_DBGX(ERR, NTHW, "%s: FPGA=%04d res=%d\n", p_adapter_id_str,
					p_fpga_info->n_fpga_prod_id, res);
		return res;
	}

	return res;
}

static struct nt200a0x_ops nt200a0x_ops = { .nthw_fpga_nt200a0x_init = nthw_fpga_nt200a0x_init };

void nt200a0x_ops_init(void)
{
	NT_LOG(INF, NTHW, "NT200A0X OPS INIT\n");
	register_nt200a0x_ops(&nt200a0x_ops);
}
