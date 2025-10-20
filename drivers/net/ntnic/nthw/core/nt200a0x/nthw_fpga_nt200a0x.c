/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_fpga.h"
#include "ntnic_mod_reg.h"

static int nthw_fpga_nt200a0x_init(struct fpga_info_s *p_fpga_info)
{
	RTE_ASSERT(p_fpga_info);

	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	struct nthw_fpga_rst_nt200a0x rst;
	int res = -1;
	const struct rst_nt200a0x_ops *rst_nt200a0x_ops = get_rst_nt200a0x_ops();

	if (rst_nt200a0x_ops == NULL) {
		NT_LOG(ERR, NTHW, "RST NT200A0X NOT INCLUDED");
		return -1;
	}

	/* reset common */
	res = rst_nt200a0x_ops->nthw_fpga_rst_nt200a0x_init(p_fpga_info, &rst);

	if (res) {
		NT_LOG_DBGX(ERR, NTHW, "%s: FPGA=%04d res=%d", p_adapter_id_str,
					p_fpga_info->n_fpga_prod_id, res);
		return res;
	}

	bool included = true;
	struct rst9563_ops *rst9563_ops = get_rst9563_ops();

	/* reset specific */
	switch (p_fpga_info->n_fpga_prod_id) {
	case 9563:
		if (rst9563_ops != NULL)
			res = rst9563_ops->nthw_fpga_rst9563_init(p_fpga_info, &rst);

		else
			included = false;

		break;

	default:
		NT_LOG(ERR, NTHW, "%s: Unsupported FPGA product: %04d", p_adapter_id_str,
			p_fpga_info->n_fpga_prod_id);
		res = -1;
		break;
	}

	if (!included) {
		NT_LOG(ERR, NTHW, "%s: NOT INCLUDED FPGA product: %04d", p_adapter_id_str,
			p_fpga_info->n_fpga_prod_id);
		res = -1;
	}

	if (res) {
		NT_LOG_DBGX(ERR, NTHW, "%s: FPGA=%04d res=%d", p_adapter_id_str,
					p_fpga_info->n_fpga_prod_id, res);
		return res;
	}

	return res;
}

static struct nt200a0x_ops nt200a0x_ops = { .nthw_fpga_nt200a0x_init = nthw_fpga_nt200a0x_init };

void nt200a0x_ops_init(void)
{
	NT_LOG(DBG, NTHW, "NT200A0X OPS INIT");
	nthw_reg_nt200a0x_ops(&nt200a0x_ops);
}
