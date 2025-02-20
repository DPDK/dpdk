/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nthw_fpga.h"
#include "ntnic_mod_reg.h"
#include "ntlog.h"

static int nthw_fpga_nt400dxx_init(struct fpga_info_s *p_fpga_info)
{
	assert(p_fpga_info);
	struct rst9574_ops *rst9574_ops = NULL;

	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	struct nthw_fpga_rst_nt400dxx rst;
	int res = -1;

	nthw_fpga_t *p_fpga = p_fpga_info->mp_fpga;
	assert(p_fpga);

	switch (p_fpga_info->n_fpga_prod_id) {
	case 9574:
		rst9574_ops = get_rst9574_ops();

		if (rst9574_ops == NULL) {
			NT_LOG(ERR, NTHW, "%s: RST 9574 NOT INCLUDED", p_adapter_id_str);
			return -1;
		}

		res = rst9574_ops->nthw_fpga_rst9574_setup(p_fpga, &rst);

		if (res) {
			NT_LOG(ERR, NTHW,
				"%s: %s: FPGA=%04d Failed to create reset module res=%d",
				p_adapter_id_str, __func__, p_fpga_info->n_fpga_prod_id, res);
			return res;
		}

		break;

	default:
		NT_LOG(ERR, NTHW, "%s: Unsupported FPGA product: %04d",
			p_adapter_id_str, p_fpga_info->n_fpga_prod_id);
		return -1;
	}

	struct rst_nt400dxx_ops *rst_nt400dxx_ops = get_rst_nt400dxx_ops();

	if (rst_nt400dxx_ops == NULL) {
		NT_LOG(ERR, NTHW, "RST NT400DXX NOT INCLUDED");
		return -1;
	}

	/* reset common */
	res = rst_nt400dxx_ops->nthw_fpga_rst_nt400dxx_init(p_fpga_info);

	if (res) {
		NT_LOG(ERR, NTHW, "%s: %s: FPGA=%04d - Failed to init common modules res=%d",
			p_adapter_id_str, __func__, p_fpga_info->n_fpga_prod_id, res);
		return res;
	}

	res = rst_nt400dxx_ops->nthw_fpga_rst_nt400dxx_reset(p_fpga_info);

	if (res) {
		NT_LOG(ERR, NTHW,
			"%s: %s: FPGA=%04d - Failed to reset common modules res=%d",
			p_adapter_id_str, __func__, p_fpga_info->n_fpga_prod_id, res);
		return res;
	}

		/* reset specific */
	switch (p_fpga_info->n_fpga_prod_id) {
	case 9574:
		if (rst9574_ops)
			res = rst9574_ops->nthw_fpga_rst9574_init(p_fpga_info, &rst);

		if (res) {
			NT_LOG(ERR, NTHW,
				"%s: %s: FPGA=%04d - Failed to reset 9574 modules res=%d",
				p_adapter_id_str, __func__, p_fpga_info->n_fpga_prod_id, res);
			return res;
		}

		break;

	default:
		NT_LOG(ERR, NTHW, "%s: Unsupported FPGA product: %04d",
			p_adapter_id_str, p_fpga_info->n_fpga_prod_id);
		res = -1;
		break;
	}

	if (res) {
		NT_LOG(ERR, NTHW, "%s: %s: loc=%u: FPGA=%04d res=%d", p_adapter_id_str,
			__func__, __LINE__, p_fpga_info->n_fpga_prod_id, res);
		return res;
	}

	return res;
}

static struct nt400dxx_ops nt400dxx_ops = { .nthw_fpga_nt400dxx_init = nthw_fpga_nt400dxx_init };

void nt400dxx_ops_init(void)
{
	register_nt400dxx_ops(&nt400dxx_ops);
}
