/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nt_util.h"
#include "nthw_gfg.h"

int nthw_gfg_init(nthw_gfg_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_GFG, n_instance);
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_register_t *p_reg;

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: GFG %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_gfg = mod;

	p->mn_param_gfg_present = nthw_fpga_get_product_param(p_fpga, NT_GFG_PRESENT, 0);

	if (!p->mn_param_gfg_present) {
		NT_LOG(ERR,
			NTHW,
			"%s: %s: GFG %d module found - but logically not present - failing",
			__func__,
			p_adapter_id_str,
			p->mn_instance);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_CTRL0);

	if (p_reg) {
		p->mpa_fld_ctrl_enable[0] = nthw_register_query_field(p_reg, GFG_CTRL0_ENABLE);
		p->mpa_fld_ctrl_mode[0] = nthw_register_query_field(p_reg, GFG_CTRL0_MODE);
		p->mpa_fld_ctrl_prbs_en[0] = nthw_register_query_field(p_reg, GFG_CTRL0_PRBS_EN);
		p->mpa_fld_ctrl_size[0] = nthw_register_query_field(p_reg, GFG_CTRL0_SIZE);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_CTRL1);

	if (p_reg) {
		p->mpa_fld_ctrl_enable[1] = nthw_register_query_field(p_reg, GFG_CTRL1_ENABLE);
		p->mpa_fld_ctrl_mode[1] = nthw_register_query_field(p_reg, GFG_CTRL1_MODE);
		p->mpa_fld_ctrl_prbs_en[1] = nthw_register_query_field(p_reg, GFG_CTRL1_PRBS_EN);
		p->mpa_fld_ctrl_size[1] = nthw_register_query_field(p_reg, GFG_CTRL1_SIZE);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_CTRL2);

	if (p_reg) {
		p->mpa_fld_ctrl_enable[2] = nthw_register_query_field(p_reg, GFG_CTRL2_ENABLE);
		p->mpa_fld_ctrl_mode[2] = nthw_register_query_field(p_reg, GFG_CTRL2_MODE);
		p->mpa_fld_ctrl_prbs_en[2] = nthw_register_query_field(p_reg, GFG_CTRL2_PRBS_EN);
		p->mpa_fld_ctrl_size[2] = nthw_register_query_field(p_reg, GFG_CTRL2_SIZE);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_CTRL3);

	if (p_reg) {
		p->mpa_fld_ctrl_enable[3] = nthw_register_query_field(p_reg, GFG_CTRL3_ENABLE);
		p->mpa_fld_ctrl_mode[3] = nthw_register_query_field(p_reg, GFG_CTRL3_MODE);
		p->mpa_fld_ctrl_prbs_en[3] = nthw_register_query_field(p_reg, GFG_CTRL3_PRBS_EN);
		p->mpa_fld_ctrl_size[3] = nthw_register_query_field(p_reg, GFG_CTRL3_SIZE);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_CTRL4);

	if (p_reg) {
		p->mpa_fld_ctrl_enable[4] = nthw_register_query_field(p_reg, GFG_CTRL4_ENABLE);
		p->mpa_fld_ctrl_mode[4] = nthw_register_query_field(p_reg, GFG_CTRL4_MODE);
		p->mpa_fld_ctrl_prbs_en[4] = nthw_register_query_field(p_reg, GFG_CTRL4_PRBS_EN);
		p->mpa_fld_ctrl_size[4] = nthw_register_query_field(p_reg, GFG_CTRL4_SIZE);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_CTRL5);

	if (p_reg) {
		p->mpa_fld_ctrl_enable[5] = nthw_register_query_field(p_reg, GFG_CTRL5_ENABLE);
		p->mpa_fld_ctrl_mode[5] = nthw_register_query_field(p_reg, GFG_CTRL5_MODE);
		p->mpa_fld_ctrl_prbs_en[5] = nthw_register_query_field(p_reg, GFG_CTRL5_PRBS_EN);
		p->mpa_fld_ctrl_size[5] = nthw_register_query_field(p_reg, GFG_CTRL5_SIZE);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_CTRL6);

	if (p_reg) {
		p->mpa_fld_ctrl_enable[6] = nthw_register_query_field(p_reg, GFG_CTRL6_ENABLE);
		p->mpa_fld_ctrl_mode[6] = nthw_register_query_field(p_reg, GFG_CTRL6_MODE);
		p->mpa_fld_ctrl_prbs_en[6] = nthw_register_query_field(p_reg, GFG_CTRL6_PRBS_EN);
		p->mpa_fld_ctrl_size[6] = nthw_register_query_field(p_reg, GFG_CTRL6_SIZE);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_CTRL7);

	if (p_reg) {
		p->mpa_fld_ctrl_enable[7] = nthw_register_query_field(p_reg, GFG_CTRL7_ENABLE);
		p->mpa_fld_ctrl_mode[7] = nthw_register_query_field(p_reg, GFG_CTRL7_MODE);
		p->mpa_fld_ctrl_prbs_en[7] = nthw_register_query_field(p_reg, GFG_CTRL7_PRBS_EN);
		p->mpa_fld_ctrl_size[7] = nthw_register_query_field(p_reg, GFG_CTRL7_SIZE);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_RUN0);

	if (p_reg)
		p->mpa_fld_run_run[0] = nthw_register_query_field(p_reg, GFG_RUN0_RUN);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_RUN1);

	if (p_reg)
		p->mpa_fld_run_run[1] = nthw_register_query_field(p_reg, GFG_RUN1_RUN);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_RUN2);

	if (p_reg)
		p->mpa_fld_run_run[2] = nthw_register_query_field(p_reg, GFG_RUN2_RUN);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_RUN3);

	if (p_reg)
		p->mpa_fld_run_run[3] = nthw_register_query_field(p_reg, GFG_RUN3_RUN);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_RUN4);

	if (p_reg)
		p->mpa_fld_run_run[4] = nthw_register_query_field(p_reg, GFG_RUN4_RUN);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_RUN5);

	if (p_reg)
		p->mpa_fld_run_run[5] = nthw_register_query_field(p_reg, GFG_RUN5_RUN);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_RUN6);

	if (p_reg)
		p->mpa_fld_run_run[6] = nthw_register_query_field(p_reg, GFG_RUN6_RUN);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_RUN7);

	if (p_reg)
		p->mpa_fld_run_run[7] = nthw_register_query_field(p_reg, GFG_RUN7_RUN);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_STREAMID0);

	if (p_reg)
		p->mpa_fld_stream_id_val[0] = nthw_register_query_field(p_reg, GFG_STREAMID0_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_STREAMID1);

	if (p_reg)
		p->mpa_fld_stream_id_val[1] = nthw_register_query_field(p_reg, GFG_STREAMID1_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_STREAMID2);

	if (p_reg)
		p->mpa_fld_stream_id_val[2] = nthw_register_query_field(p_reg, GFG_STREAMID2_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_STREAMID3);

	if (p_reg)
		p->mpa_fld_stream_id_val[3] = nthw_register_query_field(p_reg, GFG_STREAMID3_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_STREAMID4);

	if (p_reg)
		p->mpa_fld_stream_id_val[4] = nthw_register_query_field(p_reg, GFG_STREAMID4_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_STREAMID5);

	if (p_reg)
		p->mpa_fld_stream_id_val[5] = nthw_register_query_field(p_reg, GFG_STREAMID5_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_STREAMID6);

	if (p_reg)
		p->mpa_fld_stream_id_val[6] = nthw_register_query_field(p_reg, GFG_STREAMID6_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_STREAMID7);

	if (p_reg)
		p->mpa_fld_stream_id_val[7] = nthw_register_query_field(p_reg, GFG_STREAMID7_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_SIZEMASK0);

	if (p_reg)
		p->mpa_fld_size_mask[0] = nthw_register_query_field(p_reg, GFG_SIZEMASK0_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_SIZEMASK1);

	if (p_reg)
		p->mpa_fld_size_mask[1] = nthw_register_query_field(p_reg, GFG_SIZEMASK1_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_SIZEMASK2);

	if (p_reg)
		p->mpa_fld_size_mask[2] = nthw_register_query_field(p_reg, GFG_SIZEMASK2_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_SIZEMASK3);

	if (p_reg)
		p->mpa_fld_size_mask[3] = nthw_register_query_field(p_reg, GFG_SIZEMASK3_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_SIZEMASK4);

	if (p_reg)
		p->mpa_fld_size_mask[4] = nthw_register_query_field(p_reg, GFG_SIZEMASK4_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_SIZEMASK5);

	if (p_reg)
		p->mpa_fld_size_mask[5] = nthw_register_query_field(p_reg, GFG_SIZEMASK5_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_SIZEMASK6);

	if (p_reg)
		p->mpa_fld_size_mask[6] = nthw_register_query_field(p_reg, GFG_SIZEMASK6_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_SIZEMASK7);

	if (p_reg)
		p->mpa_fld_size_mask[7] = nthw_register_query_field(p_reg, GFG_SIZEMASK7_VAL);

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_BURSTSIZE0);

	if (p_reg) {
		p->mpa_fld_burst_size_val[0] =
			nthw_register_query_field(p_reg, GFG_BURSTSIZE0_VAL);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_BURSTSIZE1);

	if (p_reg) {
		p->mpa_fld_burst_size_val[1] =
			nthw_register_query_field(p_reg, GFG_BURSTSIZE1_VAL);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_BURSTSIZE2);

	if (p_reg) {
		p->mpa_fld_burst_size_val[2] =
			nthw_register_query_field(p_reg, GFG_BURSTSIZE2_VAL);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_BURSTSIZE3);

	if (p_reg) {
		p->mpa_fld_burst_size_val[3] =
			nthw_register_query_field(p_reg, GFG_BURSTSIZE3_VAL);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_BURSTSIZE4);

	if (p_reg) {
		p->mpa_fld_burst_size_val[4] =
			nthw_register_query_field(p_reg, GFG_BURSTSIZE4_VAL);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_BURSTSIZE5);

	if (p_reg) {
		p->mpa_fld_burst_size_val[5] =
			nthw_register_query_field(p_reg, GFG_BURSTSIZE5_VAL);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_BURSTSIZE6);

	if (p_reg) {
		p->mpa_fld_burst_size_val[6] =
			nthw_register_query_field(p_reg, GFG_BURSTSIZE6_VAL);
	}

	p_reg = nthw_module_query_register(p->mp_mod_gfg, GFG_BURSTSIZE7);

	if (p_reg) {
		p->mpa_fld_burst_size_val[7] =
			nthw_register_query_field(p_reg, GFG_BURSTSIZE7_VAL);
	}

	return 0;
}

static int nthw_gfg_setup(nthw_gfg_t *p,
	const size_t n_intf_no,
	const int n_enable,
	const int n_frame_count,
	const int n_frame_size,
	const int n_frame_fill_mode,
	const int n_frame_stream_id)
{
	if ((size_t)n_intf_no >= ARRAY_SIZE(p->mpa_fld_ctrl_enable) ||
		n_intf_no >= ARRAY_SIZE(p->mpa_fld_ctrl_mode) ||
		(size_t)n_intf_no >= ARRAY_SIZE(p->mpa_fld_ctrl_size) ||
		n_intf_no >= ARRAY_SIZE(p->mpa_fld_stream_id_val) ||
		(size_t)n_intf_no >= ARRAY_SIZE(p->mpa_fld_burst_size_val)) {
		RTE_ASSERT(false);
		return -1;
	}

	if (p->mpa_fld_ctrl_enable[n_intf_no] == NULL || p->mpa_fld_ctrl_mode[n_intf_no] == NULL ||
		p->mpa_fld_ctrl_size[n_intf_no] == NULL ||
		p->mpa_fld_stream_id_val[n_intf_no] == NULL ||
		p->mpa_fld_burst_size_val[n_intf_no] == NULL) {
		RTE_ASSERT(false);
		return -1;
	}

	nthw_field_set_val_flush32(p->mpa_fld_stream_id_val[n_intf_no], n_frame_stream_id);
	nthw_field_set_val_flush32(p->mpa_fld_burst_size_val[n_intf_no], n_frame_count);

	if (p->mpa_fld_size_mask[n_intf_no])
		nthw_field_set_val_flush32(p->mpa_fld_size_mask[n_intf_no], 0);

	nthw_field_set_val32(p->mpa_fld_ctrl_mode[n_intf_no], n_frame_fill_mode);
	nthw_field_set_val32(p->mpa_fld_ctrl_size[n_intf_no], n_frame_size);

	if (p->mpa_fld_ctrl_prbs_en[n_intf_no])
		nthw_field_set_val32(p->mpa_fld_ctrl_prbs_en[n_intf_no], 0);

	nthw_field_set_val_flush32(p->mpa_fld_ctrl_enable[n_intf_no],
		n_enable);	/* write enable and flush */

	return 0;
}

int nthw_gfg_stop(nthw_gfg_t *p, const int n_intf_no)
{
	return nthw_gfg_setup(p, n_intf_no, 0, 1, 0x666, 0, n_intf_no);
}
