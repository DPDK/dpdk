/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_FLM_H__
#define __FLOW_NTHW_FLM_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct flm_nthw;

typedef struct flm_nthw flm_nthw_t;

struct flm_nthw *nthw_flm_new(void);
void nthw_flm_delete(struct flm_nthw *p);
int nthw_flm_init(struct flm_nthw *p, nthw_fpga_t *p_fpga, int n_instance);
void nthw_flm_set_debug_mode(struct flm_nthw *p, unsigned int n_debug_mode);

/* Control */
void nthw_flm_control_enable(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_init(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_lds(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_lfs(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_lis(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_uds(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_uis(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_rds(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_ris(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_pds(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_pis(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_crcwr(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_crcrd(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_rbl(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_eab(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_split_sdram_usage(const struct flm_nthw *p, uint32_t val);
void nthw_flm_control_flush(const struct flm_nthw *p);

/* Status */
void nthw_flm_status_calib_success(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_status_calib_fail(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_status_initdone(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_status_idle(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_status_critical(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_status_panic(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_status_crcerr(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_status_eft_bp(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_status_cache_buf_crit(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_status_flush(const struct flm_nthw *p);
void nthw_flm_status_update(const struct flm_nthw *p);

/* Scan */
void nthw_flm_scan_i(const struct flm_nthw *p, uint32_t val);
void nthw_flm_scan_flush(const struct flm_nthw *p);

/* Load BIN */
void nthw_flm_load_bin(const struct flm_nthw *p, uint32_t val);
void nthw_flm_load_bin_flush(const struct flm_nthw *p);

/* Load LPS */
void nthw_flm_load_lps_update(const struct flm_nthw *p);
void nthw_flm_load_lps_cnt(const struct flm_nthw *p, uint32_t *val, int get);

/* Load APS */
void nthw_flm_load_aps_update(const struct flm_nthw *p);
void nthw_flm_load_aps_cnt(const struct flm_nthw *p, uint32_t *val, int get);

/* Prio */
void nthw_flm_prio_limit0(const struct flm_nthw *p, uint32_t val);
void nthw_flm_prio_ft0(const struct flm_nthw *p, uint32_t val);
void nthw_flm_prio_limit1(const struct flm_nthw *p, uint32_t val);
void nthw_flm_prio_ft1(const struct flm_nthw *p, uint32_t val);
void nthw_flm_prio_limit2(const struct flm_nthw *p, uint32_t val);
void nthw_flm_prio_ft2(const struct flm_nthw *p, uint32_t val);
void nthw_flm_prio_limit3(const struct flm_nthw *p, uint32_t val);
void nthw_flm_prio_ft3(const struct flm_nthw *p, uint32_t val);
void nthw_flm_prio_flush(const struct flm_nthw *p);

/* PST */
void nthw_flm_pst_select(const struct flm_nthw *p, uint32_t val);
void nthw_flm_pst_cnt(const struct flm_nthw *p, uint32_t val);
void nthw_flm_pst_bp(const struct flm_nthw *p, uint32_t val);
void nthw_flm_pst_pp(const struct flm_nthw *p, uint32_t val);
void nthw_flm_pst_tp(const struct flm_nthw *p, uint32_t val);
void nthw_flm_pst_flush(const struct flm_nthw *p);

/* RCP */
void nthw_flm_rcp_select(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_cnt(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_lookup(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_qw0_dyn(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_qw0_ofs(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_qw0_sel(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_qw4_dyn(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_qw4_ofs(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_sw8_dyn(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_sw8_ofs(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_sw8_sel(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_sw9_dyn(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_sw9_ofs(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_mask(const struct flm_nthw *p, const uint32_t *val);
void nthw_flm_rcp_kid(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_opn(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_ipn(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_byt_dyn(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_byt_ofs(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_txplm(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_auto_ipv4_mask(const struct flm_nthw *p, uint32_t val);
void nthw_flm_rcp_flush(const struct flm_nthw *p);

/* Buf Ctrl */
int nthw_flm_buf_ctrl_update(const struct flm_nthw *p, uint32_t *lrn_free, uint32_t *inf_avail,
	uint32_t *sta_avail);

/* Lrn Data */
int nthw_flm_lrn_data_flush(const struct flm_nthw *p, const uint32_t *data, uint32_t records,
	uint32_t words_per_record, uint32_t *handled_records,
	uint32_t *lrn_free, uint32_t *inf_avail, uint32_t *sta_avail);

/* Inf - Sta Data */
int nthw_flm_inf_sta_data_update(const struct flm_nthw *p, uint32_t *inf_data,
	uint32_t inf_word_count, uint32_t *sta_data,
	uint32_t sta_word_count, uint32_t *lrn_free, uint32_t *inf_avail,
	uint32_t *sta_avail);

/* Stat Lrn Done */
void nthw_flm_stat_lrn_done_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_lrn_done_update(const struct flm_nthw *p);

/* Stat Lrn Ignore */
void nthw_flm_stat_lrn_ignore_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_lrn_ignore_update(const struct flm_nthw *p);

/* Stat Lrn Fail */
void nthw_flm_stat_lrn_fail_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_lrn_fail_update(const struct flm_nthw *p);

/* Stat Unl Done */
void nthw_flm_stat_unl_done_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_unl_done_update(const struct flm_nthw *p);

/* Stat Unl Ignore */
void nthw_flm_stat_unl_ignore_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_unl_ignore_update(const struct flm_nthw *p);

/* Stat Prb Done */
void nthw_flm_stat_prb_done_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_prb_done_update(const struct flm_nthw *p);

/* Stat Prb Ignore */
void nthw_flm_stat_prb_ignore_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_prb_ignore_update(const struct flm_nthw *p);

/* Stat Rel Done */
void nthw_flm_stat_rel_done_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_rel_done_update(const struct flm_nthw *p);

/* Stat Rel Ignore */
void nthw_flm_stat_rel_ignore_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_rel_ignore_update(const struct flm_nthw *p);

/* Stat Aul Done */
void nthw_flm_stat_aul_done_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_aul_done_update(const struct flm_nthw *p);

/* Stat Aul Ignore */
void nthw_flm_stat_aul_ignore_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_aul_ignore_update(const struct flm_nthw *p);

/* Stat Aul Fail */
void nthw_flm_stat_aul_fail_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_aul_fail_update(const struct flm_nthw *p);

/* Stat Tul Done */
void nthw_flm_stat_tul_done_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_tul_done_update(const struct flm_nthw *p);

/* Stat Flows */
void nthw_flm_stat_flows_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_flows_update(const struct flm_nthw *p);

/* Stat Sta Done */
void nthw_flm_stat_sta_done_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_sta_done_update(const struct flm_nthw *p);

/* Stat Inf Done */
void nthw_flm_stat_inf_done_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_inf_done_update(const struct flm_nthw *p);

/* Stat Inf Skip */
void nthw_flm_stat_inf_skip_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_inf_skip_update(const struct flm_nthw *p);

/* Stat Pck Hit */
void nthw_flm_stat_pck_hit_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_pck_hit_update(const struct flm_nthw *p);

/* Stat Pck Miss */
void nthw_flm_stat_pck_miss_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_pck_miss_update(const struct flm_nthw *p);

/* Stat Pck Unh */
void nthw_flm_stat_pck_unh_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_pck_unh_update(const struct flm_nthw *p);

/* Stat Pck Dis */
void nthw_flm_stat_pck_dis_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_pck_dis_update(const struct flm_nthw *p);

/* Stat Csh Hit */
void nthw_flm_stat_csh_hit_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_csh_hit_update(const struct flm_nthw *p);

/* Stat Csh Miss */
void nthw_flm_stat_csh_miss_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_csh_miss_update(const struct flm_nthw *p);

/* Stat Csh Unh */
void nthw_flm_stat_csh_unh_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_csh_unh_update(const struct flm_nthw *p);

/* Stat Cuc Start */
void nthw_flm_stat_cuc_start_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_cuc_start_update(const struct flm_nthw *p);

/* Stat Cuc Move */
void nthw_flm_stat_cuc_move_cnt(const struct flm_nthw *p, uint32_t *val, int get);
void nthw_flm_stat_cuc_move_update(const struct flm_nthw *p);

/* Scrubber profile memory */
void nthw_flm_scrub_select(const struct flm_nthw *p, uint32_t val);
void nthw_flm_scrub_cnt(const struct flm_nthw *p, uint32_t val);
void nthw_flm_scrub_t(const struct flm_nthw *p, uint32_t val);
void nthw_flm_scrub_r(const struct flm_nthw *p, uint32_t val);
void nthw_flm_scrub_del(const struct flm_nthw *p, uint32_t val);
void nthw_flm_scrub_inf(const struct flm_nthw *p, uint32_t val);
void nthw_flm_scrub_flush(const struct flm_nthw *p);

struct flm_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;
	void *mp_rac;

	nthw_module_t *m_flm;

	nthw_register_t *mp_control;
	nthw_field_t *mp_control_enable;
	nthw_field_t *mp_control_init;
	nthw_field_t *mp_control_lds;
	nthw_field_t *mp_control_lfs;
	nthw_field_t *mp_control_lis;
	nthw_field_t *mp_control_uds;
	nthw_field_t *mp_control_uis;
	nthw_field_t *mp_control_rds;
	nthw_field_t *mp_control_ris;
	nthw_field_t *mp_control_pds;
	nthw_field_t *mp_control_pis;
	nthw_field_t *mp_control_crcwr;
	nthw_field_t *mp_control_crcrd;
	nthw_field_t *mp_control_rbl;
	nthw_field_t *mp_control_eab;
	nthw_field_t *mp_control_split_sdram_usage;
	nthw_field_t *mp_control_calib_recalibrate;

	nthw_register_t *mp_status;
	nthw_field_t *mp_status_calib_success;
	nthw_field_t *mp_status_calib_fail;
	nthw_field_t *mp_status_initdone;
	nthw_field_t *mp_status_idle;
	nthw_field_t *mp_status_critical;
	nthw_field_t *mp_status_panic;
	nthw_field_t *mp_status_crcerr;
	nthw_field_t *mp_status_eft_bp;
	nthw_field_t *mp_status_cache_buf_critical;

	nthw_register_t *mp_timeout;
	nthw_field_t *mp_timeout_t;

	nthw_register_t *mp_scan;
	nthw_field_t *mp_scan_i;

	nthw_register_t *mp_load_bin;
	nthw_field_t *mp_load_bin_bin;

	nthw_register_t *mp_load_lps;
	nthw_field_t *mp_load_lps_lps;

	nthw_register_t *mp_load_aps;
	nthw_field_t *mp_load_aps_aps;

	nthw_register_t *mp_prio;
	nthw_field_t *mp_prio_limit0;
	nthw_field_t *mp_prio_ft0;
	nthw_field_t *mp_prio_limit1;
	nthw_field_t *mp_prio_ft1;
	nthw_field_t *mp_prio_limit2;
	nthw_field_t *mp_prio_ft2;
	nthw_field_t *mp_prio_limit3;
	nthw_field_t *mp_prio_ft3;

	nthw_register_t *mp_pst_ctrl;
	nthw_field_t *mp_pst_ctrl_adr;
	nthw_field_t *mp_pst_ctrl_cnt;
	nthw_register_t *mp_pst_data;
	nthw_field_t *mp_pst_data_bp;
	nthw_field_t *mp_pst_data_pp;
	nthw_field_t *mp_pst_data_tp;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_ctrl_adr;
	nthw_field_t *mp_rcp_ctrl_cnt;
	nthw_register_t *mp_rcp_data;
	nthw_field_t *mp_rcp_data_lookup;
	nthw_field_t *mp_rcp_data_qw0_dyn;
	nthw_field_t *mp_rcp_data_qw0_ofs;
	nthw_field_t *mp_rcp_data_qw0_sel;
	nthw_field_t *mp_rcp_data_qw4_dyn;
	nthw_field_t *mp_rcp_data_qw4_ofs;
	nthw_field_t *mp_rcp_data_sw8_dyn;
	nthw_field_t *mp_rcp_data_sw8_ofs;
	nthw_field_t *mp_rcp_data_sw8_sel;
	nthw_field_t *mp_rcp_data_sw9_dyn;
	nthw_field_t *mp_rcp_data_sw9_ofs;
	nthw_field_t *mp_rcp_data_mask;
	nthw_field_t *mp_rcp_data_kid;
	nthw_field_t *mp_rcp_data_opn;
	nthw_field_t *mp_rcp_data_ipn;
	nthw_field_t *mp_rcp_data_byt_dyn;
	nthw_field_t *mp_rcp_data_byt_ofs;
	nthw_field_t *mp_rcp_data_txplm;
	nthw_field_t *mp_rcp_data_auto_ipv4_mask;

	nthw_register_t *mp_buf_ctrl;
	nthw_field_t *mp_buf_ctrl_lrn_free;
	nthw_field_t *mp_buf_ctrl_inf_avail;
	nthw_field_t *mp_buf_ctrl_sta_avail;

	nthw_register_t *mp_lrn_data;
	nthw_register_t *mp_inf_data;
	nthw_register_t *mp_sta_data;

	nthw_register_t *mp_stat_lrn_done;
	nthw_field_t *mp_stat_lrn_done_cnt;

	nthw_register_t *mp_stat_lrn_ignore;
	nthw_field_t *mp_stat_lrn_ignore_cnt;

	nthw_register_t *mp_stat_lrn_fail;
	nthw_field_t *mp_stat_lrn_fail_cnt;

	nthw_register_t *mp_stat_unl_done;
	nthw_field_t *mp_stat_unl_done_cnt;

	nthw_register_t *mp_stat_unl_ignore;
	nthw_field_t *mp_stat_unl_ignore_cnt;

	nthw_register_t *mp_stat_prb_done;
	nthw_field_t *mp_stat_prb_done_cnt;

	nthw_register_t *mp_stat_prb_ignore;
	nthw_field_t *mp_stat_prb_ignore_cnt;

	nthw_register_t *mp_stat_rel_done;
	nthw_field_t *mp_stat_rel_done_cnt;

	nthw_register_t *mp_stat_rel_ignore;
	nthw_field_t *mp_stat_rel_ignore_cnt;

	nthw_register_t *mp_stat_aul_done;
	nthw_field_t *mp_stat_aul_done_cnt;

	nthw_register_t *mp_stat_aul_ignore;
	nthw_field_t *mp_stat_aul_ignore_cnt;

	nthw_register_t *mp_stat_aul_fail;
	nthw_field_t *mp_stat_aul_fail_cnt;

	nthw_register_t *mp_stat_tul_done;
	nthw_field_t *mp_stat_tul_done_cnt;

	nthw_register_t *mp_stat_flows;
	nthw_field_t *mp_stat_flows_cnt;

	nthw_register_t *mp_stat_sta_done;
	nthw_field_t *mp_stat_sta_done_cnt;

	nthw_register_t *mp_stat_inf_done;
	nthw_field_t *mp_stat_inf_done_cnt;

	nthw_register_t *mp_stat_inf_skip;
	nthw_field_t *mp_stat_inf_skip_cnt;

	nthw_register_t *mp_stat_pck_hit;
	nthw_field_t *mp_stat_pck_hit_cnt;

	nthw_register_t *mp_stat_pck_miss;
	nthw_field_t *mp_stat_pck_miss_cnt;

	nthw_register_t *mp_stat_pck_unh;
	nthw_field_t *mp_stat_pck_unh_cnt;

	nthw_register_t *mp_stat_pck_dis;
	nthw_field_t *mp_stat_pck_dis_cnt;

	nthw_register_t *mp_stat_csh_hit;
	nthw_field_t *mp_stat_csh_hit_cnt;

	nthw_register_t *mp_stat_csh_miss;
	nthw_field_t *mp_stat_csh_miss_cnt;

	nthw_register_t *mp_stat_csh_unh;
	nthw_field_t *mp_stat_csh_unh_cnt;

	nthw_register_t *mp_stat_cuc_start;
	nthw_field_t *mp_stat_cuc_start_cnt;

	nthw_register_t *mp_stat_cuc_move;
	nthw_field_t *mp_stat_cuc_move_cnt;

	nthw_register_t *mp_scrub_ctrl;
	nthw_field_t *mp_scrub_ctrl_adr;
	nthw_field_t *mp_scrub_ctrl_cnt;

	nthw_register_t *mp_scrub_data;
	nthw_field_t *mp_scrub_data_t;
	nthw_field_t *mp_scrub_data_r;
	nthw_field_t *mp_scrub_data_del;
	nthw_field_t *mp_scrub_data_inf;
};

#endif	/* __FLOW_NTHW_FLM_H__ */
