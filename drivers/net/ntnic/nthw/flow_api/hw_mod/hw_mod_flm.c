/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hw_mod_backend.h"

#define _MOD_ "FLM"
#define _VER_ be->flm.ver

bool nthw_mod_flm_present(struct flow_api_backend_s *be)
{
	return be->iface->get_flm_present(be->be_dev);
}

int nthw_mod_flm_alloc(struct flow_api_backend_s *be)
{
	int nb;
	_VER_ = be->iface->get_flm_version(be->be_dev);
	NT_LOG(DBG, FILTER, "FLM MODULE VERSION  %i.%i", VER_MAJOR(_VER_), VER_MINOR(_VER_));

	nb = be->iface->get_nb_flm_categories(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(km_categories);
		return COUNT_ERROR;
	}
	be->flm.nb_categories = (uint32_t)nb;

	nb = be->iface->get_nb_flm_size_mb(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(flm_size_mb);
		return COUNT_ERROR;
	}
	be->flm.nb_size_mb = (uint32_t)nb;

	nb = be->iface->get_nb_flm_entry_size(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(flm_entry_size);
		return COUNT_ERROR;
	}

	be->flm.nb_entry_size = (uint32_t)nb;

	nb = be->iface->get_nb_flm_variant(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(flm_variant);
		return COUNT_ERROR;
	}

	be->flm.nb_variant = (uint32_t)nb;

	nb = be->iface->get_nb_flm_prios(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(flm_prios);
		return COUNT_ERROR;
	}

	be->flm.nb_prios = (uint32_t)nb;

	nb = be->iface->get_nb_flm_pst_profiles(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(flm_pst_profiles);
		return COUNT_ERROR;
	}

	be->flm.nb_pst_profiles = (uint32_t)nb;

	if (_VER_ >= 22) {
		nb = be->iface->get_nb_flm_scrub_profiles(be->be_dev);

		if (nb <= 0) {
			COUNT_ERROR_LOG(flm_scrub_profiles);
			return COUNT_ERROR;
		}

		be->flm.nb_scrub_profiles = (uint32_t)nb;
	}

	nb = be->iface->get_nb_rpp_per_ps(be->be_dev);

	if (nb <= 0) {
		COUNT_ERROR_LOG(flm_rpp_clock);
		return COUNT_ERROR;
	}

	be->flm.nb_rpp_clock_in_ps = (uint32_t)nb;

	nb = be->iface->get_nb_flm_load_aps_max(be->be_dev);

	if (nb <= 0)  {
		COUNT_ERROR_LOG(flm_load_aps_max);
		return COUNT_ERROR;
	}

	be->flm.nb_load_aps_max = (uint32_t)nb;

	switch (_VER_) {
	case 25:
		if (!nthw_callocate_mod((struct common_func_s *)&be->flm, 38,
				&be->flm.v25.control, 1,
				sizeof(struct flm_v25_control_s), &be->flm.v25.status, 1,
				sizeof(struct flm_v25_status_s), &be->flm.v25.load_bin, 1,
				sizeof(struct flm_v25_load_bin_s), &be->flm.v25.load_pps, 1,
				sizeof(struct flm_v25_load_pps_s), &be->flm.v25.load_lps, 1,
				sizeof(struct flm_v25_load_lps_s), &be->flm.v25.load_aps, 1,
				sizeof(struct flm_v25_load_aps_s), &be->flm.v25.prio, 1,
				sizeof(struct flm_v25_prio_s), &be->flm.v25.pst,
				be->flm.nb_pst_profiles, sizeof(struct flm_v25_pst_s),
				&be->flm.v25.rcp, be->flm.nb_categories,
				sizeof(struct flm_v25_rcp_s),
				&be->flm.v25.buf_ctrl, 1, sizeof(struct flm_v25_buf_ctrl_s),
				&be->flm.v25.lrn_done, 1, sizeof(struct flm_v25_stat_lrn_done_s),
				&be->flm.v25.lrn_ignore, 1,
				sizeof(struct flm_v25_stat_lrn_ignore_s),
				&be->flm.v25.lrn_fail, 1, sizeof(struct flm_v25_stat_lrn_fail_s),
				&be->flm.v25.unl_done, 1, sizeof(struct flm_v25_stat_unl_done_s),
				&be->flm.v25.unl_ignore, 1,
				sizeof(struct flm_v25_stat_unl_ignore_s),
				&be->flm.v25.rel_done, 1, sizeof(struct flm_v25_stat_rel_done_s),
				&be->flm.v25.rel_ignore, 1,
				sizeof(struct flm_v25_stat_rel_ignore_s),
				&be->flm.v25.aul_done, 1, sizeof(struct flm_v25_stat_aul_done_s),
				&be->flm.v25.aul_ignore, 1,
				sizeof(struct flm_v25_stat_aul_ignore_s),
				&be->flm.v25.aul_fail, 1, sizeof(struct flm_v25_stat_aul_fail_s),
				&be->flm.v25.tul_done, 1, sizeof(struct flm_v25_stat_tul_done_s),
				&be->flm.v25.flows, 1, sizeof(struct flm_v25_stat_flows_s),
				&be->flm.v25.prb_done, 1, sizeof(struct flm_v25_stat_prb_done_s),
				&be->flm.v25.prb_ignore, 1,
				sizeof(struct flm_v25_stat_prb_ignore_s),
				&be->flm.v25.sta_done, 1, sizeof(struct flm_v25_stat_sta_done_s),
				&be->flm.v25.inf_done, 1, sizeof(struct flm_v25_stat_inf_done_s),
				&be->flm.v25.inf_skip, 1, sizeof(struct flm_v25_stat_inf_skip_s),
				&be->flm.v25.pck_hit, 1, sizeof(struct flm_v25_stat_pck_hit_s),
				&be->flm.v25.pck_miss, 1, sizeof(struct flm_v25_stat_pck_miss_s),
				&be->flm.v25.pck_unh, 1, sizeof(struct flm_v25_stat_pck_unh_s),
				&be->flm.v25.pck_dis, 1, sizeof(struct flm_v25_stat_pck_dis_s),
				&be->flm.v25.csh_hit, 1, sizeof(struct flm_v25_stat_csh_hit_s),
				&be->flm.v25.csh_miss, 1, sizeof(struct flm_v25_stat_csh_miss_s),
				&be->flm.v25.csh_unh, 1, sizeof(struct flm_v25_stat_csh_unh_s),
				&be->flm.v25.cuc_start, 1, sizeof(struct flm_v25_stat_cuc_start_s),
				&be->flm.v25.cuc_move, 1, sizeof(struct flm_v25_stat_cuc_move_s),
				&be->flm.v25.scan, 1, sizeof(struct flm_v25_scan_s),
				&be->flm.v25.scrub, be->flm.nb_scrub_profiles,
				sizeof(struct flm_v25_scrub_s)))
			return -1;

		break;

	default:
		UNSUP_VER_LOG;
		return  UNSUP_VER;
	}

	return 0;
}

void nthw_mod_flm_free(struct flow_api_backend_s *be)
{
	if (be->flm.base) {
		free(be->flm.base);
		be->flm.base = NULL;
	}
}

int nthw_mod_flm_reset(struct flow_api_backend_s *be)
{
	/* Zero entire cache area */
	nthw_zero_module_cache((struct common_func_s *)(&be->flm));

	NT_LOG(DBG, FILTER, "INIT FLM");
	nthw_mod_flm_control_set(be, HW_FLM_CONTROL_SPLIT_SDRAM_USAGE, 0x10);

	nthw_mod_flm_control_flush(be);
	nthw_mod_flm_scrub_flush(be, 0, ALL_ENTRIES);
	nthw_mod_flm_scan_flush(be);
	nthw_mod_flm_rcp_flush(be, 0, ALL_ENTRIES);

	return 0;
}

int nthw_mod_flm_control_flush(struct flow_api_backend_s *be)
{
	return be->iface->flm_control_flush(be->be_dev, &be->flm);
}

static int hw_mod_flm_control_mod(struct flow_api_backend_s *be, enum hw_flm_e field,
	uint32_t *value, int get)
{
	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_CONTROL_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(be->flm.v25.control, (uint8_t)*value,
				sizeof(struct flm_v25_control_s));
			break;

		case HW_FLM_CONTROL_ENABLE:
			GET_SET(be->flm.v25.control->enable, value);
			break;

		case HW_FLM_CONTROL_INIT:
			GET_SET(be->flm.v25.control->init, value);
			break;

		case HW_FLM_CONTROL_LDS:
			GET_SET(be->flm.v25.control->lds, value);
			break;

		case HW_FLM_CONTROL_LFS:
			GET_SET(be->flm.v25.control->lfs, value);
			break;

		case HW_FLM_CONTROL_LIS:
			GET_SET(be->flm.v25.control->lis, value);
			break;

		case HW_FLM_CONTROL_UDS:
			GET_SET(be->flm.v25.control->uds, value);
			break;

		case HW_FLM_CONTROL_UIS:
			GET_SET(be->flm.v25.control->uis, value);
			break;

		case HW_FLM_CONTROL_RDS:
			GET_SET(be->flm.v25.control->rds, value);
			break;

		case HW_FLM_CONTROL_RIS:
			GET_SET(be->flm.v25.control->ris, value);
			break;

		case HW_FLM_CONTROL_PDS:
			GET_SET(be->flm.v25.control->pds, value);
			break;

		case HW_FLM_CONTROL_PIS:
			GET_SET(be->flm.v25.control->pis, value);
			break;

		case HW_FLM_CONTROL_CRCWR:
			GET_SET(be->flm.v25.control->crcwr, value);
			break;

		case HW_FLM_CONTROL_CRCRD:
			GET_SET(be->flm.v25.control->crcrd, value);
			break;

		case HW_FLM_CONTROL_RBL:
			GET_SET(be->flm.v25.control->rbl, value);
			break;

		case HW_FLM_CONTROL_EAB:
			GET_SET(be->flm.v25.control->eab, value);
			break;

		case HW_FLM_CONTROL_SPLIT_SDRAM_USAGE:
			GET_SET(be->flm.v25.control->split_sdram_usage, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_flm_control_set(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t value)
{
	return hw_mod_flm_control_mod(be, field, &value, 0);
}

int nthw_mod_flm_status_update(struct flow_api_backend_s *be)
{
	return be->iface->flm_status_update(be->be_dev, &be->flm);
}

static int hw_mod_flm_status_mod(struct flow_api_backend_s *be, enum hw_flm_e field,
	uint32_t *value, int get)
{
	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_STATUS_CALIB_SUCCESS:
			GET_SET(be->flm.v25.status->calib_success, value);
			break;

		case HW_FLM_STATUS_CALIB_FAIL:
			GET_SET(be->flm.v25.status->calib_fail, value);
			break;

		case HW_FLM_STATUS_INITDONE:
			GET_SET(be->flm.v25.status->initdone, value);
			break;

		case HW_FLM_STATUS_IDLE:
			GET_SET(be->flm.v25.status->idle, value);
			break;

		case HW_FLM_STATUS_CRITICAL:
			GET_SET(be->flm.v25.status->critical, value);
			break;

		case HW_FLM_STATUS_PANIC:
			GET_SET(be->flm.v25.status->panic, value);
			break;

		case HW_FLM_STATUS_CRCERR:
			GET_SET(be->flm.v25.status->crcerr, value);
			break;

		case HW_FLM_STATUS_EFT_BP:
			GET_SET(be->flm.v25.status->eft_bp, value);
			break;

		case HW_FLM_STATUS_CACHE_BUFFER_CRITICAL:
			GET_SET(be->flm.v25.status->cache_buf_critical, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_flm_status_get(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t *value)
{
	return hw_mod_flm_status_mod(be, field, value, 1);
}

int nthw_mod_flm_scan_flush(struct flow_api_backend_s *be)
{
	return be->iface->flm_scan_flush(be->be_dev, &be->flm);
}

static int hw_mod_flm_scan_mod(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t *value,
	int get)
{
	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_SCAN_I:
			GET_SET(be->flm.v25.scan->i, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_flm_scan_set(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t value)
{
	return hw_mod_flm_scan_mod(be, field, &value, 0);
}

int nthw_mod_flm_load_bin_flush(struct flow_api_backend_s *be)
{
	return be->iface->flm_load_bin_flush(be->be_dev, &be->flm);
}

static int hw_mod_flm_load_bin_mod(struct flow_api_backend_s *be, enum hw_flm_e field,
	uint32_t *value, int get)
{
	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_LOAD_BIN:
			GET_SET(be->flm.v25.load_bin->bin, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_flm_load_bin_set(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t value)
{
	return hw_mod_flm_load_bin_mod(be, field, &value, 0);
}

int nthw_mod_flm_prio_flush(struct flow_api_backend_s *be)
{
	return be->iface->flm_prio_flush(be->be_dev, &be->flm);
}

static int hw_mod_flm_prio_mod(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t *value,
	int get)
{
	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_PRIO_LIMIT0:
			GET_SET(be->flm.v25.prio->limit0, value);
			break;

		case HW_FLM_PRIO_FT0:
			GET_SET(be->flm.v25.prio->ft0, value);
			break;

		case HW_FLM_PRIO_LIMIT1:
			GET_SET(be->flm.v25.prio->limit1, value);
			break;

		case HW_FLM_PRIO_FT1:
			GET_SET(be->flm.v25.prio->ft1, value);
			break;

		case HW_FLM_PRIO_LIMIT2:
			GET_SET(be->flm.v25.prio->limit2, value);
			break;

		case HW_FLM_PRIO_FT2:
			GET_SET(be->flm.v25.prio->ft2, value);
			break;

		case HW_FLM_PRIO_LIMIT3:
			GET_SET(be->flm.v25.prio->limit3, value);
			break;

		case HW_FLM_PRIO_FT3:
			GET_SET(be->flm.v25.prio->ft3, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_flm_prio_set(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t value)
{
	return hw_mod_flm_prio_mod(be, field, &value, 0);
}

int nthw_mod_flm_pst_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->flm.nb_pst_profiles;

	if ((unsigned int)(start_idx + count) > be->flm.nb_pst_profiles) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->flm_pst_flush(be->be_dev, &be->flm, start_idx, count);
}

static int hw_mod_flm_pst_mod(struct flow_api_backend_s *be, enum hw_flm_e field, int index,
	uint32_t *value, int get)
{
	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_PST_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->flm.v25.pst[index], (uint8_t)*value,
				sizeof(struct flm_v25_pst_s));
			break;

		case HW_FLM_PST_BP:
			GET_SET(be->flm.v25.pst[index].bp, value);
			break;

		case HW_FLM_PST_PP:
			GET_SET(be->flm.v25.pst[index].pp, value);
			break;

		case HW_FLM_PST_TP:
			GET_SET(be->flm.v25.pst[index].tp, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_flm_pst_set(struct flow_api_backend_s *be, enum hw_flm_e field, int index,
	uint32_t value)
{
	return hw_mod_flm_pst_mod(be, field, index, &value, 0);
}

int nthw_mod_flm_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->flm.nb_categories;

	if ((unsigned int)(start_idx + count) > be->flm.nb_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->flm_rcp_flush(be->be_dev, &be->flm, start_idx, count);
}

int nthw_mod_flm_scrub_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->flm.nb_scrub_profiles;

	if ((unsigned int)(start_idx + count) > be->flm.nb_scrub_profiles) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}
	return be->iface->flm_scrub_flush(be->be_dev, &be->flm, start_idx, count);
}

static int hw_mod_flm_rcp_mod(struct flow_api_backend_s *be, enum hw_flm_e field, int index,
	uint32_t *value, int get)
{
	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_RCP_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->flm.v25.rcp[index], (uint8_t)*value,
				sizeof(struct flm_v25_rcp_s));
			break;

		case HW_FLM_RCP_LOOKUP:
			GET_SET(be->flm.v25.rcp[index].lookup, value);
			break;

		case HW_FLM_RCP_QW0_DYN:
			GET_SET(be->flm.v25.rcp[index].qw0_dyn, value);
			break;

		case HW_FLM_RCP_QW0_OFS:
			GET_SET(be->flm.v25.rcp[index].qw0_ofs, value);
			break;

		case HW_FLM_RCP_QW0_SEL:
			GET_SET(be->flm.v25.rcp[index].qw0_sel, value);
			break;

		case HW_FLM_RCP_QW4_DYN:
			GET_SET(be->flm.v25.rcp[index].qw4_dyn, value);
			break;

		case HW_FLM_RCP_QW4_OFS:
			GET_SET(be->flm.v25.rcp[index].qw4_ofs, value);
			break;

		case HW_FLM_RCP_SW8_DYN:
			GET_SET(be->flm.v25.rcp[index].sw8_dyn, value);
			break;

		case HW_FLM_RCP_SW8_OFS:
			GET_SET(be->flm.v25.rcp[index].sw8_ofs, value);
			break;

		case HW_FLM_RCP_SW8_SEL:
			GET_SET(be->flm.v25.rcp[index].sw8_sel, value);
			break;

		case HW_FLM_RCP_SW9_DYN:
			GET_SET(be->flm.v25.rcp[index].sw9_dyn, value);
			break;

		case HW_FLM_RCP_SW9_OFS:
			GET_SET(be->flm.v25.rcp[index].sw9_ofs, value);
			break;

		case HW_FLM_RCP_MASK:
			if (get) {
				memcpy(value, be->flm.v25.rcp[index].mask,
					sizeof(((struct flm_v25_rcp_s *)0)->mask));

			} else {
				memcpy(be->flm.v25.rcp[index].mask, value,
					sizeof(((struct flm_v25_rcp_s *)0)->mask));
			}

			break;

		case HW_FLM_RCP_KID:
			GET_SET(be->flm.v25.rcp[index].kid, value);
			break;

		case HW_FLM_RCP_OPN:
			GET_SET(be->flm.v25.rcp[index].opn, value);
			break;

		case HW_FLM_RCP_IPN:
			GET_SET(be->flm.v25.rcp[index].ipn, value);
			break;

		case HW_FLM_RCP_BYT_DYN:
			GET_SET(be->flm.v25.rcp[index].byt_dyn, value);
			break;

		case HW_FLM_RCP_BYT_OFS:
			GET_SET(be->flm.v25.rcp[index].byt_ofs, value);
			break;

		case HW_FLM_RCP_TXPLM:
			GET_SET(be->flm.v25.rcp[index].txplm, value);
			break;

		case HW_FLM_RCP_AUTO_IPV4_MASK:
			GET_SET(be->flm.v25.rcp[index].auto_ipv4_mask, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_flm_rcp_set_mask(struct flow_api_backend_s *be, enum hw_flm_e field, int index,
	uint32_t *value)
{
	if (field != HW_FLM_RCP_MASK)
		return UNSUP_VER;

	return hw_mod_flm_rcp_mod(be, field, index, value, 0);
}

int nthw_mod_flm_rcp_set(struct flow_api_backend_s *be, enum hw_flm_e field, int index,
	uint32_t value)
{
	if (field == HW_FLM_RCP_MASK)
		return UNSUP_VER;

	return hw_mod_flm_rcp_mod(be, field, index, &value, 0);
}

int nthw_mod_flm_buf_ctrl_update(struct flow_api_backend_s *be)
{
	return be->iface->flm_buf_ctrl_update(be->be_dev, &be->flm);
}

static int hw_mod_flm_buf_ctrl_mod_get(struct flow_api_backend_s *be, enum hw_flm_e field,
	uint32_t *value)
{
	int get = 1;	/* Only get supported */

	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_BUF_CTRL_LRN_FREE:
			GET_SET(be->flm.v25.buf_ctrl->lrn_free, value);
			break;

		case HW_FLM_BUF_CTRL_INF_AVAIL:
			GET_SET(be->flm.v25.buf_ctrl->inf_avail, value);
			break;

		case HW_FLM_BUF_CTRL_STA_AVAIL:
			GET_SET(be->flm.v25.buf_ctrl->sta_avail, value);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_flm_buf_ctrl_get(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t *value)
{
	return hw_mod_flm_buf_ctrl_mod_get(be, field, value);
}

int nthw_mod_flm_stat_update(struct flow_api_backend_s *be)
{
	return be->iface->flm_stat_update(be->be_dev, &be->flm);
}

int nthw_mod_flm_stat_get(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t *value)
{
	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_STAT_LRN_DONE:
			*value = be->flm.v25.lrn_done->cnt;
			break;

		case HW_FLM_STAT_LRN_IGNORE:
			*value = be->flm.v25.lrn_ignore->cnt;
			break;

		case HW_FLM_STAT_LRN_FAIL:
			*value = be->flm.v25.lrn_fail->cnt;
			break;

		case HW_FLM_STAT_UNL_DONE:
			*value = be->flm.v25.unl_done->cnt;
			break;

		case HW_FLM_STAT_UNL_IGNORE:
			*value = be->flm.v25.unl_ignore->cnt;
			break;

		case HW_FLM_STAT_REL_DONE:
			*value = be->flm.v25.rel_done->cnt;
			break;

		case HW_FLM_STAT_REL_IGNORE:
			*value = be->flm.v25.rel_ignore->cnt;
			break;

		case HW_FLM_STAT_PRB_DONE:
			*value = be->flm.v25.prb_done->cnt;
			break;

		case HW_FLM_STAT_PRB_IGNORE:
			*value = be->flm.v25.prb_ignore->cnt;
			break;

		case HW_FLM_STAT_AUL_DONE:
			*value = be->flm.v25.aul_done->cnt;
			break;

		case HW_FLM_STAT_AUL_IGNORE:
			*value = be->flm.v25.aul_ignore->cnt;
			break;

		case HW_FLM_STAT_AUL_FAIL:
			*value = be->flm.v25.aul_fail->cnt;
			break;

		case HW_FLM_STAT_TUL_DONE:
			*value = be->flm.v25.tul_done->cnt;
			break;

		case HW_FLM_STAT_FLOWS:
			*value = be->flm.v25.flows->cnt;
			break;

		case HW_FLM_LOAD_LPS:
			*value = be->flm.v25.load_lps->lps;
			break;

		case HW_FLM_LOAD_APS:
			*value = be->flm.v25.load_aps->aps;
			break;

		default: {
			switch (field) {
			case HW_FLM_STAT_STA_DONE:
				*value = be->flm.v25.sta_done->cnt;
				break;

			case HW_FLM_STAT_INF_DONE:
				*value = be->flm.v25.inf_done->cnt;
				break;

			case HW_FLM_STAT_INF_SKIP:
				*value = be->flm.v25.inf_skip->cnt;
				break;

			case HW_FLM_STAT_PCK_HIT:
				*value = be->flm.v25.pck_hit->cnt;
				break;

			case HW_FLM_STAT_PCK_MISS:
				*value = be->flm.v25.pck_miss->cnt;
				break;

			case HW_FLM_STAT_PCK_UNH:
				*value = be->flm.v25.pck_unh->cnt;
				break;

			case HW_FLM_STAT_PCK_DIS:
				*value = be->flm.v25.pck_dis->cnt;
				break;

			case HW_FLM_STAT_CSH_HIT:
				*value = be->flm.v25.csh_hit->cnt;
				break;

			case HW_FLM_STAT_CSH_MISS:
				*value = be->flm.v25.csh_miss->cnt;
				break;

			case HW_FLM_STAT_CSH_UNH:
				*value = be->flm.v25.csh_unh->cnt;
				break;

			case HW_FLM_STAT_CUC_START:
				*value = be->flm.v25.cuc_start->cnt;
				break;

			case HW_FLM_STAT_CUC_MOVE:
				*value = be->flm.v25.cuc_move->cnt;
				break;

			default:
				return UNSUP_FIELD;
			}
		}
		break;
		}

		break;

	default:
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_flm_lrn_data_set_flush(struct flow_api_backend_s *be, enum hw_flm_e field,
	const uint32_t *value, uint32_t records,
	uint32_t *handled_records, uint32_t *inf_word_cnt,
	uint32_t *sta_word_cnt)
{
	int ret = 0;

	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_FLOW_LRN_DATA:
			ret = be->iface->flm_lrn_data_flush(be->be_dev, &be->flm, value, records,
					handled_records,
					(sizeof(struct flm_v25_lrn_data_s) /
						sizeof(uint32_t)),
					inf_word_cnt, sta_word_cnt);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return ret;
}

int nthw_mod_flm_inf_sta_data_update_get(struct flow_api_backend_s *be, enum hw_flm_e field,
	uint32_t *inf_value, uint32_t inf_size,
	uint32_t *inf_word_cnt, uint32_t *sta_value,
	uint32_t sta_size, uint32_t *sta_word_cnt)
{
	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_FLOW_INF_STA_DATA:
			be->iface->flm_inf_sta_data_update(be->be_dev, &be->flm, inf_value,
				inf_size, inf_word_cnt, sta_value,
				sta_size, sta_word_cnt);
			break;

		default:
			UNSUP_FIELD_LOG;
			return UNSUP_FIELD;
		}

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

/*
 * SCRUB timeout support functions to encode users' input into FPGA 8-bit time format:
 * Timeout in seconds (2^30 nanoseconds); zero means disabled. Value is:
 *
 * (T[7:3] != 0) ? ((8 + T[2:0]) shift-left (T[7:3] - 1)) : T[2:0]
 *
 * The maximum allowed value is 0xEF (137 years).
 *
 * Note that this represents a lower bound on the timeout, depending on the flow
 * scanner interval and overall load, the timeout can be substantially longer.
 */
uint32_t nthw_mod_flm_scrub_timeout_decode(uint32_t t_enc)
{
	uint32_t t_bits_2_0 = t_enc & 0x07;
	uint32_t t_bits_7_3 = (t_enc >> 3) & 0x1F;
	uint32_t t_dec = t_bits_7_3 != 0 ? ((8U + t_bits_2_0) << (t_bits_7_3 - 1U))
		: t_bits_2_0;
	/* convert internal FPGA scrub time unit into seconds, i.e. 2^30/1e9 is
	 * approx 1.074 sec per internal unit
	 */
	uint64_t t_sec = (uint64_t)t_dec * 1074UL / 1000UL;
	return t_sec > UINT32_MAX ? UINT32_MAX : (uint32_t)t_sec;
}

uint32_t nthw_mod_flm_scrub_timeout_encode(uint32_t t)
{
	uint32_t t_enc = 0;

	if (t > 0) {
		uint32_t t_dec = 0;

		do {
			t_enc++;
			t_dec = nthw_mod_flm_scrub_timeout_decode(t_enc);
		} while (t_enc <= 0xEF && t_dec < t);
	}

	return t_enc;
}

static int hw_mod_flm_scrub_mod(struct flow_api_backend_s *be, enum hw_flm_e field, int index,
	uint32_t *value, int get)
{
	switch (_VER_) {
	case 25:
		switch (field) {
		case HW_FLM_SCRUB_PRESET_ALL:
			if (get)
				return UNSUP_FIELD;

			memset(&be->flm.v25.scrub[index], (uint8_t)*value,
				sizeof(struct flm_v25_scrub_s));
			break;

		case HW_FLM_SCRUB_T:
			GET_SET(be->flm.v25.scrub[index].t, value);
			break;

		case HW_FLM_SCRUB_R:
			GET_SET(be->flm.v25.scrub[index].r, value);
			break;

		case HW_FLM_SCRUB_DEL:
			GET_SET(be->flm.v25.scrub[index].del, value);
			break;

		case HW_FLM_SCRUB_INF:
			GET_SET(be->flm.v25.scrub[index].inf, value);
			break;

		default:
			return UNSUP_FIELD;
		}

		break;

	default:
		return UNSUP_VER;
	}

	return 0;
}

int nthw_mod_flm_scrub_set(struct flow_api_backend_s *be, enum hw_flm_e field, int index,
	uint32_t value)
{
	return hw_mod_flm_scrub_mod(be, field, index, &value, 0);
}
