/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"
#include "nthw_fpga.h"

#include "ntnic_mod_reg.h"

static int nthw_fpga_rst9563_setup(nthw_fpga_t *p_fpga, struct nthw_fpga_rst_nt200a0x *const p)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	const int n_fpga_product_id = p_fpga->mn_product_id;
	const int n_fpga_version = p_fpga->mn_fpga_version;
	const int n_fpga_revision = p_fpga->mn_fpga_revision;

	nthw_module_t *p_mod_rst;
	nthw_register_t *p_curr_reg;

	RTE_ASSERT(p);
	p->mn_fpga_product_id = n_fpga_product_id;
	p->mn_fpga_version = n_fpga_version;
	p->mn_fpga_revision = n_fpga_revision;

	NT_LOG_DBGX(DBG, NTHW, "%s: FPGA reset setup: FPGA %04d-%02d-%02d", p_adapter_id_str,
				n_fpga_product_id, n_fpga_version, n_fpga_revision);

	p_mod_rst = nthw_fpga_query_module(p_fpga, MOD_RST9563, 0);

	if (p_mod_rst == NULL) {
		NT_LOG(ERR, NTHW, "%s: RST %d: no such instance", p_adapter_id_str, 0);
		return -1;
	}

	p_mod_rst = nthw_fpga_query_module(p_fpga, MOD_RST9563, 0);

	if (p_mod_rst == NULL) {
		NT_LOG(ERR, NTHW, "%s: RST %d: no such instance", p_adapter_id_str, 0);
		return -1;
	}

	/* RST register field pointers */
	p_curr_reg = nthw_module_get_register(p_mod_rst, RST9563_RST);
	p->mp_fld_rst_sys = nthw_register_get_field(p_curr_reg, RST9563_RST_SYS);
	p->mp_fld_rst_sys_mmcm = nthw_register_get_field(p_curr_reg, RST9563_RST_SYS_MMCM);
	p->mp_fld_rst_core_mmcm = nthw_register_get_field(p_curr_reg, RST9563_RST_CORE_MMCM);
	p->mp_fld_rst_rpp = nthw_register_get_field(p_curr_reg, RST9563_RST_RPP);
	p->mp_fld_rst_ddr4 = nthw_register_get_field(p_curr_reg, RST9563_RST_DDR4);
	p->mp_fld_rst_sdc = nthw_register_get_field(p_curr_reg, RST9563_RST_SDC);
	p->mp_fld_rst_phy = nthw_register_get_field(p_curr_reg, RST9563_RST_PHY);
	p->mp_fld_rst_serdes_rx = NULL;	/* Field not present on 9563 */
	p->mp_fld_rst_serdes_tx = NULL;	/* Field not present on 9563 */
	p->mp_fld_rst_serdes_rx_datapath = NULL;/* Field not present on 9563 */
	p->mp_fld_rst_pcs_rx = NULL;	/* Field not present on 9563 */
	p->mp_fld_rst_mac_rx = nthw_register_get_field(p_curr_reg, RST9563_RST_MAC_RX);
	p->mp_fld_rst_mac_tx = NULL;
	p->mp_fld_rst_ptp = nthw_register_get_field(p_curr_reg, RST9563_RST_PTP);
	p->mp_fld_rst_ts = nthw_register_get_field(p_curr_reg, RST9563_RST_TS);
	p->mp_fld_rst_ptp_mmcm = nthw_register_get_field(p_curr_reg, RST9563_RST_PTP_MMCM);
	p->mp_fld_rst_ts_mmcm = nthw_register_get_field(p_curr_reg, RST9563_RST_TS_MMCM);
	/* referenced in separate function */
	p->mp_fld_rst_periph = nthw_register_get_field(p_curr_reg, RST9563_RST_PERIPH);
	p->mp_fld_rst_tsm_ref_mmcm =
		nthw_register_query_field(p_curr_reg, RST9563_RST_TSM_REF_MMCM);
	p->mp_fld_rst_tmc = nthw_register_query_field(p_curr_reg, RST9563_RST_TMC);

	if (!p->mp_fld_rst_tsm_ref_mmcm)
		NT_LOG(DBG, NTHW, "%s: No RST9563_RST_TSM_REF_MMCM found", p_adapter_id_str);

	if (!p->mp_fld_rst_tmc)
		NT_LOG(DBG, NTHW, "%s: No RST9563_RST_TMC found", p_adapter_id_str);

	nthw_register_update(p_curr_reg);

	/* CTRL register field pointers */
	p_curr_reg = nthw_module_get_register(p_mod_rst, RST9563_CTRL);
	p->mp_fld_ctrl_ts_clk_sel_override =
		nthw_register_get_field(p_curr_reg, RST9563_CTRL_TS_CLKSEL_OVERRIDE);
	/* Field not present on 9563 */
	p->mp_fld_ctrl_ts_clk_sel = nthw_register_get_field(p_curr_reg, RST9563_CTRL_TS_CLKSEL);
	p->mp_fld_ctrl_ts_clk_sel_ref = NULL;	/* Field not present on 9563 */
	p->mp_fld_ctrl_ptp_mmcm_clk_sel =
		nthw_register_get_field(p_curr_reg, RST9563_CTRL_PTP_MMCM_CLKSEL);
	nthw_register_update(p_curr_reg);

	/* STAT register field pointers */
	p_curr_reg = nthw_module_get_register(p_mod_rst, RST9563_STAT);
	p->mp_fld_stat_ddr4_mmcm_locked =
		nthw_register_get_field(p_curr_reg, RST9563_STAT_DDR4_MMCM_LOCKED);
	p->mp_fld_stat_sys_mmcm_locked =
		nthw_register_get_field(p_curr_reg, RST9563_STAT_SYS_MMCM_LOCKED);
	p->mp_fld_stat_core_mmcm_locked =
		nthw_register_get_field(p_curr_reg, RST9563_STAT_CORE_MMCM_LOCKED);
	p->mp_fld_stat_ddr4_pll_locked =
		nthw_register_get_field(p_curr_reg, RST9563_STAT_DDR4_PLL_LOCKED);
	p->mp_fld_stat_ptp_mmcm_locked =
		nthw_register_get_field(p_curr_reg, RST9563_STAT_PTP_MMCM_LOCKED);
	p->mp_fld_stat_ts_mmcm_locked =
		nthw_register_get_field(p_curr_reg, RST9563_STAT_TS_MMCM_LOCKED);
	p->mp_fld_stat_tsm_ref_mmcm_locked = NULL;	/* Field not present on 9563 */

	nthw_register_update(p_curr_reg);

	/* STICKY register field pointers */
	p_curr_reg = nthw_module_get_register(p_mod_rst, RST9563_STICKY);
	p->mp_fld_sticky_ptp_mmcm_unlocked =
		nthw_register_get_field(p_curr_reg, RST9563_STICKY_PTP_MMCM_UNLOCKED);
	p->mp_fld_sticky_ts_mmcm_unlocked =
		nthw_register_get_field(p_curr_reg, RST9563_STICKY_TS_MMCM_UNLOCKED);
	p->mp_fld_sticky_ddr4_mmcm_unlocked =
		nthw_register_get_field(p_curr_reg, RST9563_STICKY_DDR4_MMCM_UNLOCKED);
	p->mp_fld_sticky_ddr4_pll_unlocked =
		nthw_register_get_field(p_curr_reg, RST9563_STICKY_DDR4_PLL_UNLOCKED);
	p->mp_fld_sticky_core_mmcm_unlocked =
		nthw_register_get_field(p_curr_reg, RST9563_STICKY_CORE_MMCM_UNLOCKED);
	p->mp_fld_sticky_pci_sys_mmcm_unlocked = NULL;	/* Field not present on 9563 */
	p->mp_fld_sticky_tsm_ref_mmcm_unlocked = NULL;	/* Field not present on 9563 */

	nthw_register_update(p_curr_reg);

	/* POWER register field pointers */
	p_curr_reg = nthw_module_get_register(p_mod_rst, RST9563_POWER);
	p->mp_fld_power_pu_phy = nthw_register_get_field(p_curr_reg, RST9563_POWER_PU_PHY);
	p->mp_fld_power_pu_nseb = nthw_register_get_field(p_curr_reg, RST9563_POWER_PU_NSEB);
	nthw_register_update(p_curr_reg);

	return 0;
}

static int nthw_fpga_rst9563_periph_reset(nthw_fpga_t *p_fpga)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	(void)p_adapter_id_str;
	nthw_module_t *p_mod_rst = nthw_fpga_query_module(p_fpga, MOD_RST9563, 0);

	if (p_mod_rst) {
		nthw_register_t *p_reg_rst;
		nthw_field_t *p_fld_rst_periph;
		NT_LOG(DBG, NTHW, "%s: PERIPH RST", p_adapter_id_str);
		p_reg_rst = nthw_module_get_register(p_mod_rst, RST9563_RST);
		p_fld_rst_periph = nthw_register_get_field(p_reg_rst, RST9563_RST_PERIPH);
		nthw_field_set_flush(p_fld_rst_periph);
		nthw_field_clr_flush(p_fld_rst_periph);

	} else {
		return -1;
	}

	return 0;
}

static int nthw_fpga_rst9563_clock_synth_init(nthw_fpga_t *p_fpga,
	const int n_si_labs_clock_synth_model,
	const uint8_t n_si_labs_clock_synth_i2c_addr)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	const int n_fpga_product_id = p_fpga->mn_product_id;
	int res;
	const struct clk9563_ops *clk9563_ops = nthw_get_clk9563_ops();

	if (clk9563_ops == NULL) {
		NT_LOG(INF, NTNIC, "CLK9563 module not included");
		return -1;
	}

	if (n_si_labs_clock_synth_model == 5340) {
		res = nthw_fpga_si5340_clock_synth_init_fmt2(p_fpga, n_si_labs_clock_synth_i2c_addr,
				clk9563_ops->get_p_data_9563_si5340_nt200a02_u23_v5(),
				*clk9563_ops->get_n_data_9563_si5340_nt200a02_u23_v5());

	} else {
		NT_LOG(ERR, NTHW, "%s: Fpga %d: Unsupported clock synth model (%d)",
			p_adapter_id_str, n_fpga_product_id, n_si_labs_clock_synth_model);
		res = -1;
	}

	return res;
}

static int nthw_fpga_rst9563_init(struct fpga_info_s *p_fpga_info,
	struct nthw_fpga_rst_nt200a0x *p_rst)
{
	RTE_ASSERT(p_fpga_info);
	RTE_ASSERT(p_rst);

	const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
	(void)p_adapter_id_str;
	int res = -1;
	int n_si_labs_clock_synth_model;
	uint8_t n_si_labs_clock_synth_i2c_addr;
	nthw_fpga_t *p_fpga = NULL;

	p_fpga = p_fpga_info->mp_fpga;
	n_si_labs_clock_synth_model = p_rst->mn_si_labs_clock_synth_model;
	n_si_labs_clock_synth_i2c_addr = p_rst->mn_si_labs_clock_synth_i2c_addr;

	res = nthw_fpga_rst9563_periph_reset(p_fpga);

	if (res) {
		NT_LOG_DBGX(DBG, NTHW, "%s: ERROR: res=%d", p_adapter_id_str, res);
		return res;
	}

	res = nthw_fpga_rst9563_clock_synth_init(p_fpga, n_si_labs_clock_synth_model,
			n_si_labs_clock_synth_i2c_addr);

	if (res) {
		NT_LOG_DBGX(DBG, NTHW, "%s: ERROR: res=%d", p_adapter_id_str, res);
		return res;
	}

	res = nthw_fpga_rst9563_setup(p_fpga, p_rst);

	if (res) {
		NT_LOG_DBGX(DBG, NTHW, "%s: ERROR: res=%d", p_adapter_id_str, res);
		return res;
	}

	const struct rst_nt200a0x_ops *rst_ops = nthw_get_rst_nt200a0x_ops();
	res = rst_ops != NULL ? rst_ops->nthw_fpga_rst_nt200a0x_reset(p_fpga, p_rst) : -1;

	if (res) {
		NT_LOG_DBGX(DBG, NTHW, "%s: ERROR: res=%d", p_adapter_id_str, res);
		return res;
	}

	return res;
}

static struct rst9563_ops rst9563_ops = { .nthw_fpga_rst9563_init = nthw_fpga_rst9563_init };

void rst9563_ops_init(void)
{
	NT_LOG(DBG, NTHW, "RST9563 OPS INIT");
	nthw_reg_rst9563_ops(&rst9563_ops);
}
