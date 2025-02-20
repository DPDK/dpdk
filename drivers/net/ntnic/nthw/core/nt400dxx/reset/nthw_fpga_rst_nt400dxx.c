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
	int res = -1;
	nthw_fpga_t *p_fpga = NULL;

	p_fpga = p_fpga_info->mp_fpga;

	nthw_hif_t *p_nthw_hif = nthw_hif_new();
	res = nthw_hif_init(p_nthw_hif, p_fpga, 0);

	if (res == 0)
		NT_LOG(DBG, NTHW, "%s: Hif module found", p_fpga_info->mp_adapter_id_str);

	/* Create PCM */
	p_fpga_info->mp_nthw_agx.p_pcm = nthw_pcm_nt400dxx_new();
	res = nthw_pcm_nt400dxx_init(p_fpga_info->mp_nthw_agx.p_pcm, p_fpga, 0);

	if (res != 0)
		return res;

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
