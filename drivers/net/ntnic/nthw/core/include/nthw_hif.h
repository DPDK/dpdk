/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_HIF_H__
#define __NTHW_HIF_H__

#define NTHW_TG_CNT_SIZE (4ULL)

struct nthw_hif {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_hif;
	int mn_instance;

	nthw_register_t *mp_reg_ctrl;
	nthw_field_t *mp_fld_ctrl_fsr;

	nthw_register_t *mp_reg_prod_id_lsb;
	nthw_field_t *mp_fld_prod_id_lsb_rev_id;
	nthw_field_t *mp_fld_prod_id_lsb_ver_id;
	nthw_field_t *mp_fld_prod_id_lsb_group_id;

	nthw_register_t *mp_reg_prod_id_msb;
	nthw_field_t *mp_fld_prod_id_msb_type_id;
	nthw_field_t *mp_fld_prod_id_msb_build_no;

	nthw_register_t *mp_reg_build_time;
	nthw_field_t *mp_fld_build_time;

	nthw_register_t *mp_reg_build_seed;
	nthw_field_t *mp_fld_build_seed;

	nthw_register_t *mp_reg_core_speed;
	nthw_field_t *mp_fld_core_speed;
	nthw_field_t *mp_fld_ddr3_speed;

	nthw_register_t *mp_reg_int_mask;
	nthw_field_t *mp_fld_int_mask_timer;
	nthw_field_t *mp_fld_int_mask_port;
	nthw_field_t *mp_fld_int_mask_pps;

	nthw_register_t *mp_reg_int_clr;
	nthw_field_t *mp_fld_int_clr_timer;
	nthw_field_t *mp_fld_int_clr_port;
	nthw_field_t *mp_fld_int_clr_pps;

	nthw_register_t *mp_reg_int_force;
	nthw_field_t *mp_fld_int_force_timer;
	nthw_field_t *mp_fld_int_force_port;
	nthw_field_t *mp_fld_int_force_pps;

	nthw_register_t *mp_reg_sample_time;
	nthw_field_t *mp_fld_sample_time;

	nthw_register_t *mp_reg_status;
	nthw_field_t *mp_fld_status_tags_in_use;
	nthw_field_t *mp_fld_status_wr_err;
	nthw_field_t *mp_fld_status_rd_err;

	nthw_register_t *mp_reg_stat_ctrl;
	nthw_field_t *mp_fld_stat_ctrl_ena;
	nthw_field_t *mp_fld_stat_ctrl_req;

	nthw_register_t *mp_reg_stat_rx;
	nthw_field_t *mp_fld_stat_rx_counter;

	nthw_register_t *mp_reg_stat_tx;
	nthw_field_t *mp_fld_stat_tx_counter;

	nthw_register_t *mp_reg_stat_ref_clk;
	nthw_field_t *mp_fld_stat_ref_clk_ref_clk;

	nthw_register_t *mp_reg_pci_test0;
	nthw_field_t *mp_fld_pci_test0;

	nthw_register_t *mp_reg_pci_test1;
	nthw_field_t *mp_fld_pci_test1;

	nthw_register_t *mp_reg_pci_test2;
	nthw_field_t *mp_fld_pci_test2;

	nthw_register_t *mp_reg_pci_test3;
	nthw_field_t *mp_fld_pci_test3;

	nthw_register_t *mp_reg_config;
	nthw_field_t *mp_fld_max_tlp;
	nthw_field_t *mp_fld_max_read;
	nthw_field_t *mp_fld_ext_tag;

	int mn_fpga_id_item;
	int mn_fpga_id_prod;
	int mn_fpga_id_ver;
	int mn_fpga_id_rev;
	int mn_fpga_id_build_no;

	int mn_fpga_param_hif_per_ps;
	uint32_t mn_fpga_hif_ref_clk_freq;
};

typedef struct nthw_hif nthw_hif_t;

struct nthw_hif_end_point_err_counters {
	uint32_t n_err_correctable, n_err_non_fatal, n_err_fatal;
};

struct nthw_hif_end_point_counters {
	int n_numa_node;

	int n_tg_direction;
	int n_tg_pkt_size;
	int n_tg_num_pkts;
	int n_tg_delay;

	uint64_t cur_rx, cur_tx;
	uint64_t cur_pci_nt_util, cur_pci_xil_util;
	uint64_t n_ref_clk_cnt;

	uint64_t n_tags_in_use;
	uint64_t n_rd_err;
	uint64_t n_wr_err;

	struct nthw_hif_end_point_err_counters s_rc_ep_pre, s_rc_ep_post, s_rc_ep_delta;
	struct nthw_hif_end_point_err_counters s_ep_rc_pre, s_ep_rc_post, s_ep_rc_delta;

	int bo_error;
};

struct nthw_hif_end_points {
	struct nthw_hif_end_point_counters pri, sla;
};

nthw_hif_t *nthw_hif_new(void);
void nthw_hif_delete(nthw_hif_t *p);
int nthw_hif_init(nthw_hif_t *p, nthw_fpga_t *p_fpga, int n_instance);

int nthw_hif_trigger_sample_time(nthw_hif_t *p);
int nthw_hif_force_soft_reset(nthw_hif_t *p);

int nthw_hif_read_test_reg(nthw_hif_t *p, uint8_t test_reg, uint32_t *p_value);
int nthw_hif_write_test_reg(nthw_hif_t *p, uint8_t test_reg, uint32_t value);

#endif	/* __NTHW_HIF_H__ */
