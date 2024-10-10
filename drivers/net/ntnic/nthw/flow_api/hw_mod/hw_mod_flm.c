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

bool hw_mod_flm_present(struct flow_api_backend_s *be)
{
	return be->iface->get_flm_present(be->be_dev);
}

int hw_mod_flm_alloc(struct flow_api_backend_s *be)
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
		if (!callocate_mod((struct common_func_s *)&be->flm, 38, &be->flm.v25.control, 1,
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

void hw_mod_flm_free(struct flow_api_backend_s *be)
{
	if (be->flm.base) {
		free(be->flm.base);
		be->flm.base = NULL;
	}
}

int hw_mod_flm_reset(struct flow_api_backend_s *be)
{
	/* Zero entire cache area */
	zero_module_cache((struct common_func_s *)(&be->flm));

	NT_LOG(DBG, FILTER, "INIT FLM");
	hw_mod_flm_control_set(be, HW_FLM_CONTROL_SPLIT_SDRAM_USAGE, 0x10);

	hw_mod_flm_control_flush(be);
	hw_mod_flm_scrub_flush(be, 0, ALL_ENTRIES);
	hw_mod_flm_scan_flush(be);
	hw_mod_flm_rcp_flush(be, 0, ALL_ENTRIES);

	return 0;
}

int hw_mod_flm_control_flush(struct flow_api_backend_s *be)
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

int hw_mod_flm_control_set(struct flow_api_backend_s *be, enum hw_flm_e field, uint32_t value)
{
	return hw_mod_flm_control_mod(be, field, &value, 0);
}

int hw_mod_flm_scan_flush(struct flow_api_backend_s *be)
{
	return be->iface->flm_scan_flush(be->be_dev, &be->flm);
}

int hw_mod_flm_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->flm.nb_categories;

	if ((unsigned int)(start_idx + count) > be->flm.nb_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->flm_rcp_flush(be->be_dev, &be->flm, start_idx, count);
}

int hw_mod_flm_scrub_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->flm.nb_scrub_profiles;

	if ((unsigned int)(start_idx + count) > be->flm.nb_scrub_profiles) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}
	return be->iface->flm_scrub_flush(be->be_dev, &be->flm, start_idx, count);
}
