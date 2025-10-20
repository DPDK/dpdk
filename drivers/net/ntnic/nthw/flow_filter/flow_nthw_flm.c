/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include "rte_debug.h"
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"
#include "nthw_rac.h"

#include "flow_nthw_flm.h"

struct flm_nthw *nthw_flm_new(void)
{
	struct flm_nthw *p = malloc(sizeof(struct flm_nthw));

	if (p)
		memset(p, 0, sizeof(*p));

	return p;
}

void nthw_flm_delete(struct flm_nthw *p)
{
	if (p) {
		memset(p, 0, sizeof(*p));
		free(p);
	}
}

void nthw_flm_set_debug_mode(struct flm_nthw *p, unsigned int n_debug_mode)
{
	nthw_module_set_debug_mode(p->m_flm, n_debug_mode);
}

int nthw_flm_init(struct flm_nthw *p, nthw_fpga_t *p_fpga, int n_instance)
{
	const char *const p_adapter_id_str = p_fpga->p_fpga_info->mp_adapter_id_str;
	nthw_module_t *p_mod = nthw_fpga_query_module(p_fpga, MOD_FLM, n_instance);

	RTE_ASSERT(n_instance >= 0 && n_instance < 256);

	if (p == NULL)
		return p_mod == NULL ? -1 : 0;

	if (p_mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: Flm %d: no such instance", p_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_rac = p_fpga->p_fpga_info->mp_nthw_rac;

	p->mp_fpga = p_fpga;
	p->m_physical_adapter_no = (uint8_t)n_instance;
	p->m_flm = p_mod;

	p->mp_control = nthw_module_get_register(p->m_flm, FLM_CONTROL);
	p->mp_control_enable = nthw_register_get_field(p->mp_control, FLM_CONTROL_ENABLE);
	p->mp_control_init = nthw_register_get_field(p->mp_control, FLM_CONTROL_INIT);
	p->mp_control_lds = nthw_register_get_field(p->mp_control, FLM_CONTROL_LDS);
	p->mp_control_lfs = nthw_register_get_field(p->mp_control, FLM_CONTROL_LFS);
	p->mp_control_lis = nthw_register_get_field(p->mp_control, FLM_CONTROL_LIS);
	p->mp_control_uds = nthw_register_get_field(p->mp_control, FLM_CONTROL_UDS);
	p->mp_control_uis = nthw_register_get_field(p->mp_control, FLM_CONTROL_UIS);
	p->mp_control_rds = nthw_register_get_field(p->mp_control, FLM_CONTROL_RDS);
	p->mp_control_ris = nthw_register_get_field(p->mp_control, FLM_CONTROL_RIS);
	p->mp_control_pds = nthw_register_query_field(p->mp_control, FLM_CONTROL_PDS);
	p->mp_control_pis = nthw_register_query_field(p->mp_control, FLM_CONTROL_PIS);
	p->mp_control_crcwr = nthw_register_get_field(p->mp_control, FLM_CONTROL_CRCWR);
	p->mp_control_crcrd = nthw_register_get_field(p->mp_control, FLM_CONTROL_CRCRD);
	p->mp_control_rbl = nthw_register_get_field(p->mp_control, FLM_CONTROL_RBL);
	p->mp_control_eab = nthw_register_get_field(p->mp_control, FLM_CONTROL_EAB);
	p->mp_control_split_sdram_usage =
		nthw_register_get_field(p->mp_control, FLM_CONTROL_SPLIT_SDRAM_USAGE);
	p->mp_control_calib_recalibrate =
		nthw_register_query_field(p->mp_control, FLM_CONTROL_CALIB_RECALIBRATE);

	p->mp_status = nthw_module_get_register(p->m_flm, FLM_STATUS);
	p->mp_status_calib_success =
		nthw_register_get_field(p->mp_status, FLM_STATUS_CALIB_SUCCESS);
	p->mp_status_calib_fail = nthw_register_get_field(p->mp_status, FLM_STATUS_CALIB_FAIL);
	p->mp_status_initdone = nthw_register_get_field(p->mp_status, FLM_STATUS_INITDONE);
	p->mp_status_idle = nthw_register_get_field(p->mp_status, FLM_STATUS_IDLE);
	p->mp_status_critical = nthw_register_get_field(p->mp_status, FLM_STATUS_CRITICAL);
	p->mp_status_panic = nthw_register_get_field(p->mp_status, FLM_STATUS_PANIC);
	p->mp_status_crcerr = nthw_register_get_field(p->mp_status, FLM_STATUS_CRCERR);
	p->mp_status_eft_bp = nthw_register_get_field(p->mp_status, FLM_STATUS_EFT_BP);
	p->mp_status_cache_buf_critical =
		nthw_register_get_field(p->mp_status, FLM_STATUS_CACHE_BUFFER_CRITICAL);

	p->mp_scan = nthw_module_get_register(p->m_flm, FLM_SCAN);
	p->mp_scan_i = nthw_register_get_field(p->mp_scan, FLM_SCAN_I);

	p->mp_load_bin = nthw_module_get_register(p->m_flm, FLM_LOAD_BIN);
	p->mp_load_bin_bin = nthw_register_get_field(p->mp_load_bin, FLM_LOAD_BIN_BIN);

	p->mp_load_lps = nthw_module_get_register(p->m_flm, FLM_LOAD_LPS);
	p->mp_load_lps_lps = nthw_register_get_field(p->mp_load_lps, FLM_LOAD_LPS_LPS);

	p->mp_load_aps = nthw_module_get_register(p->m_flm, FLM_LOAD_APS);
	p->mp_load_aps_aps = nthw_register_get_field(p->mp_load_aps, FLM_LOAD_APS_APS);

	p->mp_prio = nthw_module_get_register(p->m_flm, FLM_PRIO);
	p->mp_prio_limit0 = nthw_register_get_field(p->mp_prio, FLM_PRIO_LIMIT0);
	p->mp_prio_ft0 = nthw_register_get_field(p->mp_prio, FLM_PRIO_FT0);
	p->mp_prio_limit1 = nthw_register_get_field(p->mp_prio, FLM_PRIO_LIMIT1);
	p->mp_prio_ft1 = nthw_register_get_field(p->mp_prio, FLM_PRIO_FT1);
	p->mp_prio_limit2 = nthw_register_get_field(p->mp_prio, FLM_PRIO_LIMIT2);
	p->mp_prio_ft2 = nthw_register_get_field(p->mp_prio, FLM_PRIO_FT2);
	p->mp_prio_limit3 = nthw_register_get_field(p->mp_prio, FLM_PRIO_LIMIT3);
	p->mp_prio_ft3 = nthw_register_get_field(p->mp_prio, FLM_PRIO_FT3);

	p->mp_pst_ctrl = nthw_module_get_register(p->m_flm, FLM_PST_CTRL);
	p->mp_pst_ctrl_adr = nthw_register_get_field(p->mp_pst_ctrl, FLM_PST_CTRL_ADR);
	p->mp_pst_ctrl_cnt = nthw_register_get_field(p->mp_pst_ctrl, FLM_PST_CTRL_CNT);
	p->mp_pst_data = nthw_module_get_register(p->m_flm, FLM_PST_DATA);
	p->mp_pst_data_bp = nthw_register_get_field(p->mp_pst_data, FLM_PST_DATA_BP);
	p->mp_pst_data_pp = nthw_register_get_field(p->mp_pst_data, FLM_PST_DATA_PP);
	p->mp_pst_data_tp = nthw_register_get_field(p->mp_pst_data, FLM_PST_DATA_TP);

	p->mp_rcp_ctrl = nthw_module_get_register(p->m_flm, FLM_RCP_CTRL);
	p->mp_rcp_ctrl_adr = nthw_register_get_field(p->mp_rcp_ctrl, FLM_RCP_CTRL_ADR);
	p->mp_rcp_ctrl_cnt = nthw_register_get_field(p->mp_rcp_ctrl, FLM_RCP_CTRL_CNT);
	p->mp_rcp_data = nthw_module_get_register(p->m_flm, FLM_RCP_DATA);
	p->mp_rcp_data_lookup = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_LOOKUP);
	p->mp_rcp_data_qw0_dyn = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_QW0_DYN);
	p->mp_rcp_data_qw0_ofs = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_QW0_OFS);
	p->mp_rcp_data_qw0_sel = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_QW0_SEL);
	p->mp_rcp_data_qw4_dyn = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_QW4_DYN);
	p->mp_rcp_data_qw4_ofs = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_QW4_OFS);
	p->mp_rcp_data_sw8_dyn = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_SW8_DYN);
	p->mp_rcp_data_sw8_ofs = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_SW8_OFS);
	p->mp_rcp_data_sw8_sel = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_SW8_SEL);
	p->mp_rcp_data_sw9_dyn = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_SW9_DYN);
	p->mp_rcp_data_sw9_ofs = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_SW9_OFS);
	p->mp_rcp_data_mask = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_MASK);
	p->mp_rcp_data_kid = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_KID);
	p->mp_rcp_data_opn = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_OPN);
	p->mp_rcp_data_ipn = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_IPN);
	p->mp_rcp_data_byt_dyn = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_BYT_DYN);
	p->mp_rcp_data_byt_ofs = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_BYT_OFS);
	p->mp_rcp_data_txplm = nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_TXPLM);
	p->mp_rcp_data_auto_ipv4_mask =
		nthw_register_get_field(p->mp_rcp_data, FLM_RCP_DATA_AUTO_IPV4_MASK);

	p->mp_buf_ctrl = nthw_module_get_register(p->m_flm, FLM_BUF_CTRL);

	p->mp_lrn_data = nthw_module_get_register(p->m_flm, FLM_LRN_DATA);
	p->mp_inf_data = nthw_module_get_register(p->m_flm, FLM_INF_DATA);
	p->mp_sta_data = nthw_module_get_register(p->m_flm, FLM_STA_DATA);

	p->mp_stat_lrn_done = nthw_module_get_register(p->m_flm, FLM_STAT_LRN_DONE);
	p->mp_stat_lrn_done_cnt =
		nthw_register_get_field(p->mp_stat_lrn_done, FLM_STAT_LRN_DONE_CNT);

	p->mp_stat_lrn_ignore = nthw_module_get_register(p->m_flm, FLM_STAT_LRN_IGNORE);
	p->mp_stat_lrn_ignore_cnt =
		nthw_register_get_field(p->mp_stat_lrn_ignore, FLM_STAT_LRN_IGNORE_CNT);

	p->mp_stat_lrn_fail = nthw_module_get_register(p->m_flm, FLM_STAT_LRN_FAIL);
	p->mp_stat_lrn_fail_cnt =
		nthw_register_get_field(p->mp_stat_lrn_fail, FLM_STAT_LRN_FAIL_CNT);

	p->mp_stat_unl_done = nthw_module_get_register(p->m_flm, FLM_STAT_UNL_DONE);
	p->mp_stat_unl_done_cnt =
		nthw_register_get_field(p->mp_stat_unl_done, FLM_STAT_UNL_DONE_CNT);

	p->mp_stat_unl_ignore = nthw_module_get_register(p->m_flm, FLM_STAT_UNL_IGNORE);
	p->mp_stat_unl_ignore_cnt =
		nthw_register_get_field(p->mp_stat_unl_ignore, FLM_STAT_UNL_IGNORE_CNT);

	p->mp_stat_prb_done = nthw_module_query_register(p->m_flm, FLM_STAT_PRB_DONE);
	p->mp_stat_prb_done_cnt =
		nthw_register_query_field(p->mp_stat_prb_done, FLM_STAT_PRB_DONE_CNT);

	p->mp_stat_prb_ignore = nthw_module_query_register(p->m_flm, FLM_STAT_PRB_IGNORE);
	p->mp_stat_prb_ignore_cnt =
		nthw_register_query_field(p->mp_stat_prb_ignore, FLM_STAT_PRB_IGNORE_CNT);

	p->mp_stat_rel_done = nthw_module_get_register(p->m_flm, FLM_STAT_REL_DONE);
	p->mp_stat_rel_done_cnt =
		nthw_register_get_field(p->mp_stat_rel_done, FLM_STAT_REL_DONE_CNT);

	p->mp_stat_rel_ignore = nthw_module_get_register(p->m_flm, FLM_STAT_REL_IGNORE);
	p->mp_stat_rel_ignore_cnt =
		nthw_register_get_field(p->mp_stat_rel_ignore, FLM_STAT_REL_IGNORE_CNT);

	p->mp_stat_aul_done = nthw_module_get_register(p->m_flm, FLM_STAT_AUL_DONE);
	p->mp_stat_aul_done_cnt =
		nthw_register_get_field(p->mp_stat_aul_done, FLM_STAT_AUL_DONE_CNT);

	p->mp_stat_aul_ignore = nthw_module_get_register(p->m_flm, FLM_STAT_AUL_IGNORE);
	p->mp_stat_aul_ignore_cnt =
		nthw_register_get_field(p->mp_stat_aul_ignore, FLM_STAT_AUL_IGNORE_CNT);

	p->mp_stat_aul_fail = nthw_module_get_register(p->m_flm, FLM_STAT_AUL_FAIL);
	p->mp_stat_aul_fail_cnt =
		nthw_register_get_field(p->mp_stat_aul_fail, FLM_STAT_AUL_FAIL_CNT);

	p->mp_stat_tul_done = nthw_module_get_register(p->m_flm, FLM_STAT_TUL_DONE);
	p->mp_stat_tul_done_cnt =
		nthw_register_get_field(p->mp_stat_tul_done, FLM_STAT_TUL_DONE_CNT);

	p->mp_stat_flows = nthw_module_get_register(p->m_flm, FLM_STAT_FLOWS);
	p->mp_stat_flows_cnt = nthw_register_get_field(p->mp_stat_flows, FLM_STAT_FLOWS_CNT);

	p->mp_stat_sta_done = nthw_module_query_register(p->m_flm, FLM_STAT_STA_DONE);
	p->mp_stat_sta_done_cnt =
		nthw_register_query_field(p->mp_stat_sta_done, FLM_STAT_STA_DONE_CNT);

	p->mp_stat_inf_done = nthw_module_query_register(p->m_flm, FLM_STAT_INF_DONE);
	p->mp_stat_inf_done_cnt =
		nthw_register_query_field(p->mp_stat_inf_done, FLM_STAT_INF_DONE_CNT);

	p->mp_stat_inf_skip = nthw_module_query_register(p->m_flm, FLM_STAT_INF_SKIP);
	p->mp_stat_inf_skip_cnt =
		nthw_register_query_field(p->mp_stat_inf_skip, FLM_STAT_INF_SKIP_CNT);

	p->mp_stat_pck_hit = nthw_module_query_register(p->m_flm, FLM_STAT_PCK_HIT);
	p->mp_stat_pck_hit_cnt =
		nthw_register_query_field(p->mp_stat_pck_hit, FLM_STAT_PCK_HIT_CNT);

	p->mp_stat_pck_miss = nthw_module_query_register(p->m_flm, FLM_STAT_PCK_MISS);
	p->mp_stat_pck_miss_cnt =
		nthw_register_query_field(p->mp_stat_pck_miss, FLM_STAT_PCK_MISS_CNT);

	p->mp_stat_pck_unh = nthw_module_query_register(p->m_flm, FLM_STAT_PCK_UNH);
	p->mp_stat_pck_unh_cnt =
		nthw_register_query_field(p->mp_stat_pck_unh, FLM_STAT_PCK_UNH_CNT);

	p->mp_stat_pck_dis = nthw_module_query_register(p->m_flm, FLM_STAT_PCK_DIS);
	p->mp_stat_pck_dis_cnt =
		nthw_register_query_field(p->mp_stat_pck_dis, FLM_STAT_PCK_DIS_CNT);

	p->mp_stat_csh_hit = nthw_module_query_register(p->m_flm, FLM_STAT_CSH_HIT);
	p->mp_stat_csh_hit_cnt =
		nthw_register_query_field(p->mp_stat_csh_hit, FLM_STAT_CSH_HIT_CNT);

	p->mp_stat_csh_miss = nthw_module_query_register(p->m_flm, FLM_STAT_CSH_MISS);
	p->mp_stat_csh_miss_cnt =
		nthw_register_query_field(p->mp_stat_csh_miss, FLM_STAT_CSH_MISS_CNT);

	p->mp_stat_csh_unh = nthw_module_query_register(p->m_flm, FLM_STAT_CSH_UNH);
	p->mp_stat_csh_unh_cnt =
		nthw_register_query_field(p->mp_stat_csh_unh, FLM_STAT_CSH_UNH_CNT);

	p->mp_stat_cuc_start = nthw_module_query_register(p->m_flm, FLM_STAT_CUC_START);
	p->mp_stat_cuc_start_cnt =
		nthw_register_query_field(p->mp_stat_cuc_start, FLM_STAT_CUC_START_CNT);

	p->mp_stat_cuc_move = nthw_module_query_register(p->m_flm, FLM_STAT_CUC_MOVE);
	p->mp_stat_cuc_move_cnt =
		nthw_register_query_field(p->mp_stat_cuc_move, FLM_STAT_CUC_MOVE_CNT);

	p->mp_scrub_ctrl = nthw_module_query_register(p->m_flm, FLM_SCRUB_CTRL);
	p->mp_scrub_ctrl_adr = nthw_register_query_field(p->mp_scrub_ctrl, FLM_SCRUB_CTRL_ADR);
	p->mp_scrub_ctrl_cnt = nthw_register_query_field(p->mp_scrub_ctrl, FLM_SCRUB_CTRL_CNT);

	p->mp_scrub_data = nthw_module_query_register(p->m_flm, FLM_SCRUB_DATA);
	p->mp_scrub_data_t = nthw_register_query_field(p->mp_scrub_data, FLM_SCRUB_DATA_T);
	p->mp_scrub_data_r = nthw_register_query_field(p->mp_scrub_data, FLM_SCRUB_DATA_R);
	p->mp_scrub_data_del = nthw_register_query_field(p->mp_scrub_data, FLM_SCRUB_DATA_DEL);
	p->mp_scrub_data_inf = nthw_register_query_field(p->mp_scrub_data, FLM_SCRUB_DATA_INF);

	return 0;
}

void nthw_flm_control_enable(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_enable, val);
}

void nthw_flm_control_init(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_init, val);
}

void nthw_flm_control_lds(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_lds, val);
}

void nthw_flm_control_lfs(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_lfs, val);
}

void nthw_flm_control_lis(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_lis, val);
}

void nthw_flm_control_uds(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_uds, val);
}

void nthw_flm_control_uis(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_uis, val);
}

void nthw_flm_control_rds(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_rds, val);
}

void nthw_flm_control_ris(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_ris, val);
}

void nthw_flm_control_pds(const struct flm_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_control_pds);
	nthw_field_set_val32(p->mp_control_pds, val);
}

void nthw_flm_control_pis(const struct flm_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_control_pis);
	nthw_field_set_val32(p->mp_control_pis, val);
}

void nthw_flm_control_crcwr(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_crcwr, val);
}

void nthw_flm_control_crcrd(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_crcrd, val);
}

void nthw_flm_control_rbl(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_rbl, val);
}

void nthw_flm_control_eab(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_eab, val);
}

void nthw_flm_control_split_sdram_usage(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_control_split_sdram_usage, val);
}

void nthw_flm_control_flush(const struct flm_nthw *p)
{
	nthw_register_flush(p->mp_control, 1);
}

void nthw_flm_status_calib_success(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get) {
		uint32_t w = nthw_field_get_bit_width(p->mp_status_calib_success);
		uint32_t all_ones = (1 << w) - 1;
		*val = nthw_field_get_val32(p->mp_status_calib_success);

		if (all_ones == *val) {
			/* Mark all calibrated */
			*val |= 0x80000000;
		}
	}
}

void nthw_flm_status_calib_fail(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_status_calib_fail);
}

void nthw_flm_status_initdone(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_status_initdone);
}

void nthw_flm_status_idle(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_status_idle);
}

void nthw_flm_status_critical(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_status_critical);

	else
		nthw_field_set_val32(p->mp_status_critical, *val);
}

void nthw_flm_status_panic(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_status_panic);

	else
		nthw_field_set_val32(p->mp_status_panic, *val);
}

void nthw_flm_status_crcerr(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_status_crcerr);

	else
		nthw_field_set_val32(p->mp_status_crcerr, *val);
}

void nthw_flm_status_eft_bp(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_status_eft_bp);
}

void nthw_flm_status_cache_buf_crit(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_status_cache_buf_critical);

	else
		nthw_field_set_val32(p->mp_status_cache_buf_critical, *val);
}

void nthw_flm_status_flush(const struct flm_nthw *p)
{
	nthw_register_flush(p->mp_status, 1);
}

void nthw_flm_status_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_status);
}

void nthw_flm_scan_i(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_scan_i, val);
}

void nthw_flm_scan_flush(const struct flm_nthw *p)
{
	nthw_register_flush(p->mp_scan, 1);
}

void nthw_flm_load_bin(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_load_bin_bin, val);
}

void nthw_flm_load_bin_flush(const struct flm_nthw *p)
{
	nthw_register_flush(p->mp_load_bin, 1);
}

void nthw_flm_load_lps_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_load_lps);
}

void nthw_flm_load_lps_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_load_lps_lps);
}

void nthw_flm_load_aps_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_load_aps);
}

void nthw_flm_load_aps_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_load_aps_aps);
}

void nthw_flm_prio_limit0(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_prio_limit0, val);
}

void nthw_flm_prio_ft0(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_prio_ft0, val);
}

void nthw_flm_prio_limit1(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_prio_limit1, val);
}

void nthw_flm_prio_ft1(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_prio_ft1, val);
}

void nthw_flm_prio_limit2(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_prio_limit2, val);
}

void nthw_flm_prio_ft2(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_prio_ft2, val);
}

void nthw_flm_prio_limit3(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_prio_limit3, val);
}

void nthw_flm_prio_ft3(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_prio_ft3, val);
}

void nthw_flm_prio_flush(const struct flm_nthw *p)
{
	nthw_register_flush(p->mp_prio, 1);
}

void nthw_flm_pst_select(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_pst_ctrl_adr, val);
}

void nthw_flm_pst_cnt(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_pst_ctrl_cnt, val);
}

void nthw_flm_pst_bp(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_pst_data_bp, val);
}

void nthw_flm_pst_pp(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_pst_data_pp, val);
}

void nthw_flm_pst_tp(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_pst_data_tp, val);
}

void nthw_flm_pst_flush(const struct flm_nthw *p)
{
	nthw_register_flush(p->mp_pst_ctrl, 1);
	nthw_register_flush(p->mp_pst_data, 1);
}

void nthw_flm_rcp_select(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_ctrl_adr, val);
}

void nthw_flm_rcp_cnt(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_ctrl_cnt, val);
}

void nthw_flm_rcp_lookup(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_lookup, val);
}

void nthw_flm_rcp_qw0_dyn(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw0_dyn, val);
}

void nthw_flm_rcp_qw0_ofs(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw0_ofs, val);
}

void nthw_flm_rcp_qw0_sel(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw0_sel, val);
}

void nthw_flm_rcp_qw4_dyn(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw4_dyn, val);
}

void nthw_flm_rcp_qw4_ofs(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_qw4_ofs, val);
}

void nthw_flm_rcp_sw8_dyn(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_sw8_dyn, val);
}

void nthw_flm_rcp_sw8_ofs(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_sw8_ofs, val);
}

void nthw_flm_rcp_sw8_sel(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_sw8_sel, val);
}

void nthw_flm_rcp_sw9_dyn(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_sw9_dyn, val);
}

void nthw_flm_rcp_sw9_ofs(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_sw9_ofs, val);
}

void nthw_flm_rcp_mask(const struct flm_nthw *p, const uint32_t *val)
{
	nthw_field_set_val(p->mp_rcp_data_mask, val, 10);
}

void nthw_flm_rcp_kid(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_kid, val);
}

void nthw_flm_rcp_opn(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_opn, val);
}

void nthw_flm_rcp_ipn(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_ipn, val);
}

void nthw_flm_rcp_byt_dyn(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_byt_dyn, val);
}

void nthw_flm_rcp_byt_ofs(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_byt_ofs, val);
}

void nthw_flm_rcp_txplm(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_txplm, val);
}

void nthw_flm_rcp_auto_ipv4_mask(const struct flm_nthw *p, uint32_t val)
{
	nthw_field_set_val32(p->mp_rcp_data_auto_ipv4_mask, val);
}

void nthw_flm_rcp_flush(const struct flm_nthw *p)
{
	nthw_register_flush(p->mp_rcp_ctrl, 1);
	nthw_register_flush(p->mp_rcp_data, 1);
}

int nthw_flm_buf_ctrl_update(const struct flm_nthw *p, uint32_t *lrn_free, uint32_t *inf_avail,
	uint32_t *sta_avail)
{
	int ret = -1;

	struct nthw_rac *rac = (struct nthw_rac *)p->mp_rac;
	uint32_t address_bufctrl = nthw_register_get_address(p->mp_buf_ctrl);
	nthw_rab_bus_id_t bus_id = 1;
	struct dma_buf_ptr bc_buf;
	rte_spinlock_lock(&rac->m_mutex);
	ret = !rac->m_dma_active ? nthw_rac_rab_dma_begin(rac) : -1;

	if (ret == 0) {
		nthw_rac_rab_read32_dma(rac, bus_id, address_bufctrl, 2, &bc_buf);
		ret = rac->m_dma_active ? nthw_rac_rab_dma_commit(rac) : -1;
		RTE_ASSERT(ret != -1);
		rte_spinlock_unlock(&rac->m_mutex);

		if (ret != 0)
			return ret;

		uint32_t bc_mask = bc_buf.size - 1;
		uint32_t bc_index = bc_buf.index;
		*lrn_free = bc_buf.base[bc_index & bc_mask] & 0xffff;
		*inf_avail = (bc_buf.base[bc_index & bc_mask] >> 16) & 0xffff;
		*sta_avail = bc_buf.base[(bc_index + 1) & bc_mask] & 0xffff;
	} else {
		rte_spinlock_unlock(&rac->m_mutex);
		const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
		const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
		NT_LOG(ERR, NTHW,
				"%s: DMA begin requested, but a DMA transaction is already active",
				p_adapter_id_str);
	}

	return ret;
}

int nthw_flm_lrn_data_flush(const struct flm_nthw *p, const uint32_t *data, uint32_t records,
	uint32_t words_per_record, uint32_t *handled_records,
	uint32_t *lrn_free, uint32_t *inf_avail, uint32_t *sta_avail)
{
	struct nthw_rac *rac = (struct nthw_rac *)p->mp_rac;
	uint32_t address = nthw_register_get_address(p->mp_lrn_data);
	uint32_t address_bufctrl = nthw_register_get_address(p->mp_buf_ctrl);
	nthw_rab_bus_id_t bus_id = 1;
	struct dma_buf_ptr bc_buf;

	uint32_t max_records_per_write = 256 / words_per_record;

	/* Check for dma overhead */
	if ((256 % words_per_record) < (max_records_per_write + 3 + 1))
		--max_records_per_write;

	*handled_records = 0;
	int max_tries = 10000;

	while (*inf_avail == 0 && *sta_avail == 0 && records != 0 && --max_tries > 0) {
		rte_spinlock_lock(&rac->m_mutex);
		int ret = !rac->m_dma_active ? nthw_rac_rab_dma_begin(rac) : -1;
		if (ret == 0) {
			uint32_t dma_free = nthw_rac_rab_get_free(rac);

			if (dma_free != RAB_DMA_BUF_CNT) {
				RTE_ASSERT(0);	/* alert developer that something is wrong */
				rte_spinlock_unlock(&rac->m_mutex);
				return -1;
			}

			uint32_t max_writes_from_learn_free =
				*lrn_free / (max_records_per_write * words_per_record);

			/*
			 * Not strictly needed as dma_free will always
			 * (per design) be much larger than lrn_free
			 */
			uint32_t max_writes_from_dma_free = dma_free / 256;
			uint32_t max_writes =
				(max_writes_from_learn_free < max_writes_from_dma_free)
				? max_writes_from_learn_free
				: max_writes_from_dma_free;

			uint32_t records_per_write = (records < max_records_per_write)
				? records
				: max_records_per_write;

			while (records != 0 && max_writes != 0) {
				/*
				 * Announce the number of words to write
				 * to LRN_DATA in next write operation
				 */
				uint32_t bufctrl_data[2];
				bufctrl_data[0] = records_per_write * words_per_record;
				bufctrl_data[1] = 0;
				nthw_rac_rab_write32_dma(rac, bus_id, address_bufctrl, 2,
					bufctrl_data);

				/* Write data */
				nthw_rac_rab_write32_dma(rac, bus_id, address, bufctrl_data[0],
					data);

				/* Prepare next write operation */
				data += bufctrl_data[0];
				records -= records_per_write;
				*handled_records += records_per_write;
				records_per_write = (records < max_records_per_write)
					? records
					: max_records_per_write;
				--max_writes;
			}

			/* Read buf ctrl */
			nthw_rac_rab_read32_dma(rac, bus_id, address_bufctrl, 2, &bc_buf);

			int ret = rac->m_dma_active ? nthw_rac_rab_dma_commit(rac) : -1;
			RTE_ASSERT(ret != -1);
			rte_spinlock_unlock(&rac->m_mutex);
			if (ret != 0)
				return -1;

			uint32_t bc_mask = bc_buf.size - 1;
			uint32_t bc_index = bc_buf.index;
			*lrn_free = bc_buf.base[bc_index & bc_mask] & 0xffff;
			*inf_avail = (bc_buf.base[bc_index & bc_mask] >> 16) & 0xffff;
			*sta_avail = bc_buf.base[(bc_index + 1) & bc_mask] & 0xffff;
		} else {
			rte_spinlock_unlock(&rac->m_mutex);
			const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
			const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
			NT_LOG(ERR, NTHW,
					"%s: DMA begin requested, but a DMA transaction is already active",
					p_adapter_id_str);
		}
	}
	return 0;
}

int nthw_flm_inf_sta_data_update(const struct flm_nthw *p, uint32_t *inf_data,
	uint32_t inf_word_count, uint32_t *sta_data,
	uint32_t sta_word_count, uint32_t *lrn_free, uint32_t *inf_avail,
	uint32_t *sta_avail)
{
	int ret = -1;

	struct nthw_rac *rac = (struct nthw_rac *)p->mp_rac;
	uint32_t address_inf_data = nthw_register_get_address(p->mp_inf_data);
	uint32_t address_sta_data = nthw_register_get_address(p->mp_sta_data);
	uint32_t address_bufctrl = nthw_register_get_address(p->mp_buf_ctrl);
	nthw_rab_bus_id_t bus_id = 1;
	struct dma_buf_ptr inf_buf;
	struct dma_buf_ptr sta_buf;
	struct dma_buf_ptr bc_buf;
	uint32_t mask;
	uint32_t index;

	rte_spinlock_lock(&rac->m_mutex);
	ret = !rac->m_dma_active ? nthw_rac_rab_dma_begin(rac) : -1;

	if (ret == 0) {
		/* Announce the number of words to read from INF_DATA */
		uint32_t bufctrl_data[2];
		bufctrl_data[0] = inf_word_count << 16;
		bufctrl_data[1] = sta_word_count;
		nthw_rac_rab_write32_dma(rac, bus_id, address_bufctrl, 2, bufctrl_data);

		if (inf_word_count > 0) {
			nthw_rac_rab_read32_dma(rac, bus_id, address_inf_data, inf_word_count,
				&inf_buf);
		}

		if (sta_word_count > 0) {
			nthw_rac_rab_read32_dma(rac, bus_id, address_sta_data, sta_word_count,
				&sta_buf);
		}

		nthw_rac_rab_read32_dma(rac, bus_id, address_bufctrl, 2, &bc_buf);
		ret = rac->m_dma_active ? nthw_rac_rab_dma_commit(rac) : -1;
		RTE_ASSERT(ret != -1);
		rte_spinlock_unlock(&rac->m_mutex);

		if (ret != 0)
			return ret;

		if (inf_word_count > 0) {
			mask = inf_buf.size - 1;
			index = inf_buf.index;

			for (uint32_t i = 0; i < inf_word_count; ++index, ++i)
				inf_data[i] = inf_buf.base[index & mask];
		}

		if (sta_word_count > 0) {
			mask = sta_buf.size - 1;
			index = sta_buf.index;

			for (uint32_t i = 0; i < sta_word_count; ++index, ++i)
				sta_data[i] = sta_buf.base[index & mask];
		}

		mask = bc_buf.size - 1;
		index = bc_buf.index;
		*lrn_free = bc_buf.base[index & mask] & 0xffff;
		*inf_avail = (bc_buf.base[index & mask] >> 16) & 0xffff;
		*sta_avail = bc_buf.base[(index + 1) & mask] & 0xffff;
	} else {
		rte_spinlock_unlock(&rac->m_mutex);
		const struct fpga_info_s *const p_fpga_info = p->mp_fpga->p_fpga_info;
		const char *const p_adapter_id_str = p_fpga_info->mp_adapter_id_str;
		NT_LOG(ERR, NTHW,
				"%s: DMA begin requested, but a DMA transaction is already active",
				p_adapter_id_str);
	}

	return ret;
}

void nthw_flm_stat_lrn_done_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_lrn_done_cnt);
}

void nthw_flm_stat_lrn_done_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_lrn_done);
}

void nthw_flm_stat_lrn_ignore_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_lrn_ignore_cnt);
}

void nthw_flm_stat_lrn_ignore_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_lrn_ignore);
}

void nthw_flm_stat_lrn_fail_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_lrn_fail_cnt);
}

void nthw_flm_stat_lrn_fail_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_lrn_fail);
}

void nthw_flm_stat_unl_done_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_unl_done_cnt);
}

void nthw_flm_stat_unl_done_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_unl_done);
}

void nthw_flm_stat_unl_ignore_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_unl_ignore_cnt);
}

void nthw_flm_stat_unl_ignore_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_unl_ignore);
}

void nthw_flm_stat_prb_done_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_prb_done_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_prb_done_cnt);
}

void nthw_flm_stat_prb_done_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_prb_done);
	nthw_register_update(p->mp_stat_prb_done);
}

void nthw_flm_stat_prb_ignore_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_prb_ignore_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_prb_ignore_cnt);
}

void nthw_flm_stat_prb_ignore_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_prb_ignore);
	nthw_register_update(p->mp_stat_prb_ignore);
}

void nthw_flm_stat_rel_done_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_rel_done_cnt);
}

void nthw_flm_stat_rel_done_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_rel_done);
}

void nthw_flm_stat_rel_ignore_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_rel_ignore_cnt);
}

void nthw_flm_stat_rel_ignore_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_rel_ignore);
}

void nthw_flm_stat_aul_done_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_aul_done_cnt);
}

void nthw_flm_stat_aul_done_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_aul_done);
}

void nthw_flm_stat_aul_ignore_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_aul_ignore_cnt);
}

void nthw_flm_stat_aul_ignore_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_aul_ignore);
}

void nthw_flm_stat_aul_fail_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_aul_fail_cnt);
}

void nthw_flm_stat_aul_fail_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_aul_fail);
}

void nthw_flm_stat_tul_done_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_tul_done_cnt);
}

void nthw_flm_stat_tul_done_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_tul_done);
}

void nthw_flm_stat_flows_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	if (get)
		*val = nthw_field_get_val32(p->mp_stat_flows_cnt);
}

void nthw_flm_stat_flows_update(const struct flm_nthw *p)
{
	nthw_register_update(p->mp_stat_flows);
}

void nthw_flm_stat_sta_done_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_sta_done_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_sta_done_cnt);
}

void nthw_flm_stat_sta_done_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_sta_done);
	nthw_register_update(p->mp_stat_sta_done);
}

void nthw_flm_stat_inf_done_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_inf_done_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_inf_done_cnt);
}

void nthw_flm_stat_inf_done_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_inf_done);
	nthw_register_update(p->mp_stat_inf_done);
}

void nthw_flm_stat_inf_skip_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_inf_skip_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_inf_skip_cnt);
}

void nthw_flm_stat_inf_skip_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_inf_skip);
	nthw_register_update(p->mp_stat_inf_skip);
}

void nthw_flm_stat_pck_hit_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_pck_hit_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_pck_hit_cnt);
}

void nthw_flm_stat_pck_hit_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_pck_hit);
	nthw_register_update(p->mp_stat_pck_hit);
}

void nthw_flm_stat_pck_miss_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_pck_miss_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_pck_miss_cnt);
}

void nthw_flm_stat_pck_miss_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_pck_miss);
	nthw_register_update(p->mp_stat_pck_miss);
}

void nthw_flm_stat_pck_unh_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_pck_unh_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_pck_unh_cnt);
}

void nthw_flm_stat_pck_unh_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_pck_unh);
	nthw_register_update(p->mp_stat_pck_unh);
}

void nthw_flm_stat_pck_dis_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_pck_dis_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_pck_dis_cnt);
}

void nthw_flm_stat_pck_dis_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_pck_dis);
	nthw_register_update(p->mp_stat_pck_dis);
}

void nthw_flm_stat_csh_hit_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_csh_hit_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_csh_hit_cnt);
}

void nthw_flm_stat_csh_hit_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_csh_hit);
	nthw_register_update(p->mp_stat_csh_hit);
}

void nthw_flm_stat_csh_miss_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_csh_miss_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_csh_miss_cnt);
}

void nthw_flm_stat_csh_miss_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_csh_miss);
	nthw_register_update(p->mp_stat_csh_miss);
}

void nthw_flm_stat_csh_unh_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_csh_unh_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_csh_unh_cnt);
}

void nthw_flm_stat_csh_unh_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_csh_unh);
	nthw_register_update(p->mp_stat_csh_unh);
}

void nthw_flm_stat_cuc_start_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_cuc_start_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_cuc_start_cnt);
}

void nthw_flm_stat_cuc_start_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_cuc_start);
	nthw_register_update(p->mp_stat_cuc_start);
}

void nthw_flm_stat_cuc_move_cnt(const struct flm_nthw *p, uint32_t *val, int get)
{
	RTE_ASSERT(p->mp_stat_cuc_move_cnt);

	if (get)
		*val = nthw_field_get_val32(p->mp_stat_cuc_move_cnt);
}

void nthw_flm_stat_cuc_move_update(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_stat_cuc_move);
	nthw_register_update(p->mp_stat_cuc_move);
}

void nthw_flm_scrub_select(const struct flm_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_scrub_ctrl_adr);
	nthw_field_set_val32(p->mp_scrub_ctrl_adr, val);
}

void nthw_flm_scrub_cnt(const struct flm_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_scrub_ctrl_cnt);
	nthw_field_set_val32(p->mp_scrub_ctrl_cnt, val);
}

void nthw_flm_scrub_t(const struct flm_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_scrub_data_t);
	nthw_field_set_val32(p->mp_scrub_data_t, val);
}

void nthw_flm_scrub_r(const struct flm_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_scrub_data_r);
	nthw_field_set_val32(p->mp_scrub_data_r, val);
}

void nthw_flm_scrub_del(const struct flm_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_scrub_data_del);
	nthw_field_set_val32(p->mp_scrub_data_del, val);
}

void nthw_flm_scrub_inf(const struct flm_nthw *p, uint32_t val)
{
	RTE_ASSERT(p->mp_scrub_data_inf);
	nthw_field_set_val32(p->mp_scrub_data_inf, val);
}

void nthw_flm_scrub_flush(const struct flm_nthw *p)
{
	RTE_ASSERT(p->mp_scrub_ctrl);
	RTE_ASSERT(p->mp_scrub_data);
	nthw_register_flush(p->mp_scrub_ctrl, 1);
	nthw_register_flush(p->mp_scrub_data, 1);
}
