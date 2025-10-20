/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_PCIE3_H__
#define __NTHW_PCIE3_H__

struct nthw_pcie3 {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_pcie3;
	int mn_instance;

	nthw_register_t *mp_reg_stat_ctrl;
	nthw_field_t *mp_fld_stat_ctrl_req;
	nthw_field_t *mp_fld_stat_ctrl_ena;

	nthw_register_t *mp_reg_stat_rx;
	nthw_field_t *mp_fld_stat_rx_counter;

	nthw_register_t *mp_reg_stat_tx;
	nthw_field_t *mp_fld_stat_tx_counter;

	nthw_register_t *mp_reg_stat_rq_rdy;
	nthw_field_t *mp_fld_stat_rq_rdy_counter;

	nthw_register_t *mp_reg_stat_rq_vld;
	nthw_field_t *mp_fld_stat_rq_vld_counter;

	nthw_register_t *mp_reg_status0;
	nthw_field_t *mp_fld_status0_tags_in_use;

	nthw_register_t *mp_reg_stat_ref_clk;
	nthw_field_t *mp_fld_stat_ref_clk_ref_clk;

	nthw_register_t *mp_reg_rp_to_ep_err;
	nthw_field_t *mp_fld_rp_to_ep_err_cor;
	nthw_field_t *mp_fld_rp_to_ep_err_non_fatal;
	nthw_field_t *mp_fld_rp_to_ep_err_fatal;

	nthw_register_t *mp_reg_ep_to_rp_err;
	nthw_field_t *mp_fld_ep_to_rp_err_cor;
	nthw_field_t *mp_fld_ep_to_rp_err_non_fatal;
	nthw_field_t *mp_fld_ep_to_rp_err_fatal;

	nthw_register_t *mp_reg_sample_time;
	nthw_field_t *mp_fld_sample_time;

	nthw_register_t *mp_reg_pci_end_point;
	nthw_field_t *mp_fld_pci_end_point_if_id;
	nthw_field_t *mp_fld_pci_end_point_send_msg;
	nthw_field_t *mp_fld_pci_end_point_get_msg;
	nthw_field_t *mp_fld_pci_end_point_dmaep0_allow_mask;
	nthw_field_t *mp_fld_pci_end_point_dmaep1_allow_mask;

	nthw_register_t *mp_reg_pci_e3_mark_adr_lsb;
	nthw_field_t *mp_fld_pci_e3_mark_adr_lsb_adr;

	nthw_register_t *mp_reg_pci_e3_mark_adr_msb;
	nthw_field_t *mp_fld_pci_e3_mark_adr_msb_adr;

	nthw_register_t *mp_reg_pci_test0;
	nthw_field_t *mp_fld_pci_test0;

	nthw_register_t *mp_reg_pci_test1;
	nthw_field_t *mp_fld_pci_test1;

	nthw_register_t *mp_reg_pci_test2;
	nthw_field_t *mp_fld_pci_test2;

	nthw_register_t *mp_reg_pci_test3;
	nthw_field_t *mp_fld_pci_test3;
};

typedef struct nthw_pcie3 nthw_pcie3_t;

nthw_pcie3_t *nthw_pcie3_new(void);
void nthw_pcie3_delete(nthw_pcie3_t *p);
int nthw_pcie3_init(nthw_pcie3_t *p, nthw_fpga_t *p_fpga, int n_instance);

int nthw_pcie3_trigger_sample_time(nthw_pcie3_t *p);

#endif	/* __NTHW_PCIE3_H__ */
