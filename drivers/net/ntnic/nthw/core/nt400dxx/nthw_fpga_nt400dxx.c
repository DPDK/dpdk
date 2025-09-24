/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */
#include "ntlog.h"
#include "nthw_fpga.h"
#include "ntnic_mod_reg.h"
#include "nt400d13_u62_si5332_gm2_revd_1_v5_registers.h"

static int nthw_fpga_nt400dxx_init_clock_synthesizers(struct fpga_info_s *p_fpga_info)
{
	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	int res = -1;

	/* Clock synthesizer on address 0x6a on channel 2 */
	p_fpga_info->mp_nthw_agx.p_si5332 = nthw_si5332_new();
	res = nthw_si5332_init(p_fpga_info->mp_nthw_agx.p_si5332,
			p_fpga_info->mp_nthw_agx.p_i2cm,
			0x6A,
			p_fpga_info->mp_nthw_agx.p_pca9849,
			2);

	if (res) {
		NT_LOG(ERR, NTHW, "%s: %s: Failed to initialize Si5332 clock - res=%d",
			p_adapter_id_str, __func__, res);
		return res;
	}

	p_fpga_info->mp_nthw_agx.p_si5156 = nthw_si5156_new();
	res = nthw_si5156_init(p_fpga_info->mp_nthw_agx.p_si5156,
			p_fpga_info->mp_nthw_agx.p_i2cm,
			0x60,
			p_fpga_info->mp_nthw_agx.p_pca9849,
			2);

	if (res) {
		NT_LOG(ERR, NTHW, "%s: %s: Failed to initialize Si5156 clock - res=%d",
			p_adapter_id_str, __func__, res);
		return res;
	}

	if (nthw_si5332_clock_active(p_fpga_info->mp_nthw_agx.p_si5332)) {
		NT_LOG(INF,
			NTHW,
			"%s: Fpga clock already active, skipping clock initialisation.",
			p_adapter_id_str);

	} else {
		NT_LOG(INF, NTHW,
			"%s: Fpga clock not active, performing full clock initialisation.",
			p_adapter_id_str);

		for (int i = 0; i < SI5332_GM2_REVD_REG_CONFIG_NUM_REGS; i++) {
			nthw_si5332_write(p_fpga_info->mp_nthw_agx.p_si5332,
				(uint8_t)si5332_gm2_revd_registers[i].address,
				(uint8_t)si5332_gm2_revd_registers[i].value);
		}
	}

	/*
	 * TCXO capable PCM version if minor version >= 3
	 * Unfortunately, the module version is not readily
	 * available in the FPGA_400D1x class.
	 */
	bool tcxo_capable = p_fpga_info->mp_nthw_agx.p_pcm->mn_module_minor_version >= 3;

	/*
	 * This method of determining the presence of a TCXO
	 *  will only work until non SI5156 equipped boards
	 *  use the vacant I2C address for something else...
	 *
	 * There's no other way, there's no other way
	 * All that you can do is watch them play
	 * Clean-up when GA HW is readily available
	 */
	if (nthw_si5156_write16(p_fpga_info->mp_nthw_agx.p_si5156, 0x1, 0x0400) != 0) {
		p_fpga_info->mp_nthw_agx.tcxo_capable = false;
		p_fpga_info->mp_nthw_agx.tcxo_present = false;

	} else {
		p_fpga_info->mp_nthw_agx.tcxo_capable = tcxo_capable;
		p_fpga_info->mp_nthw_agx.tcxo_present = true;
	}

	return 0;
}

static int nthw_fpga_nt400dxx_init_sub_systems(struct fpga_info_s *p_fpga_info)
{
	int res;
	NT_LOG(INF, NTHW, "%s: Initializing NT4GA subsystems...", p_fpga_info->mp_adapter_id_str);

	/* RAB subsystem */
	NT_LOG(DBG, NTHW, "%s: Initializing RAB subsystem: flush", p_fpga_info->mp_adapter_id_str);
	res = nthw_rac_rab_flush(p_fpga_info->mp_nthw_rac);

	if (res)
		return res;

	/* clock synthesizer subsystem */
	NT_LOG(DBG,
		NTHW,
		"%s: Initializing clock synthesizer subsystem",
		p_fpga_info->mp_adapter_id_str);
	res = nthw_fpga_nt400dxx_init_clock_synthesizers(p_fpga_info);

	if (res)
		return res;

	return 0;
}

static int nthw_fpga_nt400dxx_init(struct fpga_info_s *p_fpga_info)
{
	RTE_ASSERT(p_fpga_info);
	struct rst9574_ops *rst9574_ops = NULL;

	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	struct nthw_fpga_rst_nt400dxx rst;
	int res = -1;

	nthw_fpga_t *p_fpga = p_fpga_info->mp_fpga;
	RTE_ASSERT(p_fpga);

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

	res = nthw_fpga_nt400dxx_init_sub_systems(p_fpga_info);

	if (res) {
		NT_LOG(ERR, NTHW, "%s: %s: FPGA=%04d Failed to init subsystems res=%d",
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
	nthw_reg_nt400dxx_ops(&nt400dxx_ops);
}
