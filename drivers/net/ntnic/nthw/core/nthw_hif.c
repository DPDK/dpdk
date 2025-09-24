/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nt_util.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

#include "nthw_hif.h"

nthw_hif_t *nthw_hif_new(void)
{
	nthw_hif_t *p = malloc(sizeof(nthw_hif_t));

	if (p)
		memset(p, 0, sizeof(nthw_hif_t));

	return p;
}

void nthw_hif_delete(nthw_hif_t *p)
{
	free(p);
}

int nthw_hif_init(nthw_hif_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	(void)p_adapter_id_str;
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_HIF, n_instance);

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: HIF %d: no such instance",
			p_fpga->p_fpga_info->mp_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_hif = mod;

	/* default for (Xilinx-based) products until august 2022: (1e6/4000 = 250 MHz) */
	p->mn_fpga_param_hif_per_ps = nthw_fpga_get_product_param(p->mp_fpga, NT_HIF_PER_PS, 4000);
	p->mn_fpga_hif_ref_clk_freq =
		(uint32_t)(1000000000000ULL / (unsigned int)p->mn_fpga_param_hif_per_ps);

	p->mp_reg_prod_id_lsb = nthw_module_get_register(p->mp_mod_hif, HIF_PROD_ID_LSB);
	p->mp_fld_prod_id_lsb_rev_id =
		nthw_register_get_field(p->mp_reg_prod_id_lsb, HIF_PROD_ID_LSB_REV_ID);
	p->mp_fld_prod_id_lsb_ver_id =
		nthw_register_get_field(p->mp_reg_prod_id_lsb, HIF_PROD_ID_LSB_VER_ID);
	p->mp_fld_prod_id_lsb_group_id =
		nthw_register_get_field(p->mp_reg_prod_id_lsb, HIF_PROD_ID_LSB_GROUP_ID);

	p->mp_reg_prod_id_msb = nthw_module_get_register(p->mp_mod_hif, HIF_PROD_ID_MSB);
	p->mp_fld_prod_id_msb_type_id =
		nthw_register_get_field(p->mp_reg_prod_id_msb, HIF_PROD_ID_MSB_TYPE_ID);
	p->mp_fld_prod_id_msb_build_no =
		nthw_register_get_field(p->mp_reg_prod_id_msb, HIF_PROD_ID_MSB_BUILD_NO);

	p->mp_reg_build_time = nthw_module_get_register(p->mp_mod_hif, HIF_BUILD_TIME);
	p->mp_fld_build_time = nthw_register_get_field(p->mp_reg_build_time, HIF_BUILD_TIME_TIME);

	p->mn_fpga_id_prod = nthw_field_get_updated(p->mp_fld_prod_id_lsb_group_id);
	p->mn_fpga_id_ver = nthw_field_get_updated(p->mp_fld_prod_id_lsb_ver_id);
	p->mn_fpga_id_rev = nthw_field_get_updated(p->mp_fld_prod_id_lsb_rev_id);
	p->mn_fpga_id_build_no = nthw_field_get_updated(p->mp_fld_prod_id_msb_build_no);
	p->mn_fpga_id_item = nthw_field_get_updated(p->mp_fld_prod_id_msb_type_id);

	NT_LOG(DBG, NTHW, "%s: HIF %d: %d-%d-%d-%d-%d", p_adapter_id_str, p->mn_instance,
		p->mn_fpga_id_item, p->mn_fpga_id_prod, p->mn_fpga_id_ver,
		p->mn_fpga_id_rev, p->mn_fpga_id_build_no);
	NT_LOG(DBG, NTHW, "%s: HIF %d: HIF ref clock: %" PRIu32 " Hz (%d ticks/ps)",
		p_adapter_id_str, p->mn_instance, p->mn_fpga_hif_ref_clk_freq,
		p->mn_fpga_param_hif_per_ps);

	p->mp_reg_build_seed = NULL;	/* Reg/Fld not present on HIF */
	p->mp_fld_build_seed = NULL;	/* Reg/Fld not present on HIF */

	p->mp_reg_core_speed = NULL;	/* Reg/Fld not present on HIF */
	p->mp_fld_core_speed = NULL;	/* Reg/Fld not present on HIF */
	p->mp_fld_ddr3_speed = NULL;	/* Reg/Fld not present on HIF */

	/* Optional registers since: 2018-04-25 */
	p->mp_reg_int_mask = NULL;	/* Reg/Fld not present on HIF */
	p->mp_reg_int_clr = NULL;	/* Reg/Fld not present on HIF */
	p->mp_reg_int_force = NULL;	/* Reg/Fld not present on HIF */

	p->mp_fld_int_mask_timer = NULL;
	p->mp_fld_int_clr_timer = NULL;
	p->mp_fld_int_force_timer = NULL;

	p->mp_fld_int_mask_port = NULL;
	p->mp_fld_int_clr_port = NULL;
	p->mp_fld_int_force_port = NULL;

	p->mp_fld_int_mask_pps = NULL;
	p->mp_fld_int_clr_pps = NULL;
	p->mp_fld_int_force_pps = NULL;

	p->mp_reg_ctrl = nthw_module_get_register(p->mp_mod_hif, HIF_CONTROL);
	p->mp_fld_ctrl_fsr = nthw_register_query_field(p->mp_reg_ctrl, HIF_CONTROL_FSR);

	p->mp_reg_stat_ctrl = nthw_module_get_register(p->mp_mod_hif, HIF_STAT_CTRL);
	p->mp_fld_stat_ctrl_ena =
		nthw_register_get_field(p->mp_reg_stat_ctrl, HIF_STAT_CTRL_STAT_ENA);
	p->mp_fld_stat_ctrl_req =
		nthw_register_get_field(p->mp_reg_stat_ctrl, HIF_STAT_CTRL_STAT_REQ);

	p->mp_reg_stat_rx = nthw_module_get_register(p->mp_mod_hif, HIF_STAT_RX);
	p->mp_fld_stat_rx_counter =
		nthw_register_get_field(p->mp_reg_stat_rx, HIF_STAT_RX_COUNTER);

	p->mp_reg_stat_tx = nthw_module_get_register(p->mp_mod_hif, HIF_STAT_TX);
	p->mp_fld_stat_tx_counter =
		nthw_register_get_field(p->mp_reg_stat_tx, HIF_STAT_TX_COUNTER);

	p->mp_reg_stat_ref_clk = nthw_module_get_register(p->mp_mod_hif, HIF_STAT_REFCLK);
	p->mp_fld_stat_ref_clk_ref_clk =
		nthw_register_get_field(p->mp_reg_stat_ref_clk, HIF_STAT_REFCLK_REFCLK250);

	p->mp_reg_status = nthw_module_query_register(p->mp_mod_hif, HIF_STATUS);

	if (p->mp_reg_status) {
		p->mp_fld_status_tags_in_use =
			nthw_register_query_field(p->mp_reg_status, HIF_STATUS_TAGS_IN_USE);
		p->mp_fld_status_wr_err =
			nthw_register_query_field(p->mp_reg_status, HIF_STATUS_WR_ERR);
		p->mp_fld_status_rd_err =
			nthw_register_query_field(p->mp_reg_status, HIF_STATUS_RD_ERR);

	} else {
		p->mp_reg_status = nthw_module_query_register(p->mp_mod_hif, HIF_STATUS);
		p->mp_fld_status_tags_in_use =
			nthw_register_query_field(p->mp_reg_status, HIF_STATUS_TAGS_IN_USE);
		p->mp_fld_status_wr_err = NULL;
		p->mp_fld_status_rd_err = NULL;
	}

	p->mp_reg_pci_test0 = nthw_module_get_register(p->mp_mod_hif, HIF_TEST0);
	p->mp_fld_pci_test0 = nthw_register_get_field(p->mp_reg_pci_test0, HIF_TEST0_DATA);

	p->mp_reg_pci_test1 = nthw_module_get_register(p->mp_mod_hif, HIF_TEST1);
	p->mp_fld_pci_test1 = nthw_register_get_field(p->mp_reg_pci_test1, HIF_TEST1_DATA);

	/* Module::Version({2, 0})+ */
	p->mp_reg_pci_test2 = nthw_module_query_register(p->mp_mod_hif, HIF_TEST2);

	if (p->mp_reg_pci_test2)
		p->mp_fld_pci_test2 = nthw_register_get_field(p->mp_reg_pci_test2, HIF_TEST2_DATA);

	else
		p->mp_fld_pci_test2 = NULL;

	/* Module::Version({1, 2})+ */
	p->mp_reg_pci_test3 = nthw_module_query_register(p->mp_mod_hif, HIF_TEST3);

	if (p->mp_reg_pci_test3)
		p->mp_fld_pci_test3 = nthw_register_get_field(p->mp_reg_pci_test3, HIF_TEST3_DATA);

	else
		p->mp_fld_pci_test3 = NULL;

	/* Required to run TSM */
	p->mp_reg_sample_time = nthw_module_get_register(p->mp_mod_hif, HIF_SAMPLE_TIME);

	if (p->mp_reg_sample_time) {
		p->mp_fld_sample_time = nthw_register_get_field(p->mp_reg_sample_time,
				HIF_SAMPLE_TIME_SAMPLE_TIME);

	} else {
		p->mp_fld_sample_time = NULL;
	}

	/* We need to optimize PCIe3 TLP-size read-request and extended tag usage */
	{
		p->mp_reg_config = nthw_module_query_register(p->mp_mod_hif, HIF_CONFIG);

		if (p->mp_reg_config) {
			p->mp_fld_max_tlp =
				nthw_register_get_field(p->mp_reg_config, HIF_CONFIG_MAX_TLP);
			p->mp_fld_max_read =
				nthw_register_get_field(p->mp_reg_config, HIF_CONFIG_MAX_READ);
			p->mp_fld_ext_tag =
				nthw_register_get_field(p->mp_reg_config, HIF_CONFIG_EXT_TAG);

		} else {
			p->mp_fld_max_tlp = NULL;
			p->mp_fld_max_read = NULL;
			p->mp_fld_ext_tag = NULL;
		}
	}

	return 0;
}

int nthw_hif_force_soft_reset(nthw_hif_t *p)
{
	if (p->mp_fld_ctrl_fsr) {
		nthw_field_update_register(p->mp_fld_ctrl_fsr);
		nthw_field_set_flush(p->mp_fld_ctrl_fsr);
	}

	return 0;
}

int nthw_hif_trigger_sample_time(nthw_hif_t *p)
{
	nthw_field_set_val_flush32(p->mp_fld_sample_time, 0xfee1dead);

	return 0;
}

int nthw_hif_read_test_reg(nthw_hif_t *p, uint8_t test_reg, uint32_t *p_value)
{
	uint32_t data;

	switch (test_reg) {
	case 0:
		data = nthw_field_get_updated(p->mp_fld_pci_test0);
		break;

	case 1:
		data = nthw_field_get_updated(p->mp_fld_pci_test1);
		break;

	case 2:
		if (p->mp_fld_pci_test2)
			data = nthw_field_get_updated(p->mp_fld_pci_test2);

		else
			return -1;

		break;

	case 3:
		if (p->mp_fld_pci_test3)
			data = nthw_field_get_updated(p->mp_fld_pci_test3);

		else
			return -1;

		break;

	default:
		RTE_ASSERT(false);
		return -1;
	}

	if (p_value)
		*p_value = data;

	else
		return -1;

	return 0;
}

int nthw_hif_write_test_reg(nthw_hif_t *p, uint8_t test_reg, uint32_t value)
{
	switch (test_reg) {
	case 0:
		nthw_field_set_val_flush32(p->mp_fld_pci_test0, value);
		break;

	case 1:
		nthw_field_set_val_flush32(p->mp_fld_pci_test1, value);
		break;

	case 2:
		if (p->mp_fld_pci_test2)
			nthw_field_set_val_flush32(p->mp_fld_pci_test2, value);

		else
			return -1;

		break;

	case 3:
		if (p->mp_fld_pci_test3)
			nthw_field_set_val_flush32(p->mp_fld_pci_test3, value);

		else
			return -1;

		break;

	default:
		RTE_ASSERT(false);
		return -1;
	}

	return 0;
}
