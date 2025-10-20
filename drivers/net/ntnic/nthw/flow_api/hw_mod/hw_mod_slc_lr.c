/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>

#include "hw_mod_backend.h"

#define _MOD_ "SLC_LR"
#define _VER_ be->slc_lr.ver

bool nthw_mod_slc_lr_present(struct flow_api_backend_s *be)
{
	return be->iface->get_slc_lr_present(be->be_dev);
}

int nthw_mod_slc_lr_alloc(struct flow_api_backend_s *be)
{
	_VER_ = be->iface->get_slc_lr_version(be->be_dev);
	NT_LOG(DBG, FILTER, "SLC LR MODULE VERSION  %i.%i", VER_MAJOR(_VER_), VER_MINOR(_VER_));

	switch (_VER_) {
	case 2:
		if (!nthw_callocate_mod((struct common_func_s *)&be->slc_lr, 1, &be->slc_lr.v2.rcp,
				be->max_categories, sizeof(struct slc_lr_v2_rcp_s)))
			return -1;

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

void nthw_mod_slc_lr_free(struct flow_api_backend_s *be)
{
	if (be->slc_lr.base) {
		free(be->slc_lr.base);
		be->slc_lr.base = NULL;
	}
}

int nthw_mod_slc_lr_reset(struct flow_api_backend_s *be)
{
	/* Zero entire cache area */
	nthw_zero_module_cache((struct common_func_s *)(&be->slc_lr));

	NT_LOG(DBG, FILTER, "INIT SLC LR RCP");
	return nthw_mod_slc_lr_rcp_flush(be, 0, be->max_categories);
}

int nthw_mod_slc_lr_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->max_categories;

	if ((unsigned int)(start_idx + count) > be->max_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->slc_lr_rcp_flush(be->be_dev, &be->slc_lr, start_idx, count);
}

static int hw_mod_slc_lr_rcp_mod(struct flow_api_backend_s *be, enum hw_slc_lr_e field,
	uint32_t index, uint32_t *value, int get)
{
	if (index >= be->max_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	switch (_VER_) {
	case 2:
		switch (field) {
		case HW_SLC_LR_RCP_PRESET_ALL:
			if (get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			memset(&be->slc_lr.v2.rcp[index], (uint8_t)*value,
				sizeof(struct hw_mod_slc_lr_v2_s));
			break;

		case HW_SLC_LR_RCP_FIND:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->max_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			FIND_EQUAL_INDEX(be->slc_lr.v2.rcp, struct hw_mod_slc_lr_v2_s, index,
				*value, be->max_categories);
			break;

		case HW_SLC_LR_RCP_COMPARE:
			if (!get) {
				UNSUP_FIELD_LOG;
				return UNSUP_FIELD;
			}

			if (*value >= be->max_categories) {
				INDEX_TOO_LARGE_LOG;
				return INDEX_TOO_LARGE;
			}

			DO_COMPARE_INDEXS(be->slc_lr.v2.rcp, struct hw_mod_slc_lr_v2_s, index,
				*value);
			break;

		case HW_SLC_LR_RCP_HEAD_SLC_EN:
			GET_SET(be->slc_lr.v2.rcp[index].head_slc_en, value);
			break;

		case HW_SLC_LR_RCP_HEAD_DYN:
			GET_SET(be->slc_lr.v2.rcp[index].head_dyn, value);
			break;

		case HW_SLC_LR_RCP_HEAD_OFS:
			GET_SET_SIGNED(be->slc_lr.v2.rcp[index].head_ofs, value);
			break;

		case HW_SLC_LR_RCP_TAIL_SLC_EN:
			GET_SET(be->slc_lr.v2.rcp[index].tail_slc_en, value);
			break;

		case HW_SLC_LR_RCP_TAIL_DYN:
			GET_SET(be->slc_lr.v2.rcp[index].tail_dyn, value);
			break;

		case HW_SLC_LR_RCP_TAIL_OFS:
			GET_SET_SIGNED(be->slc_lr.v2.rcp[index].tail_ofs, value);
			break;

		case HW_SLC_LR_RCP_PCAP:
			GET_SET(be->slc_lr.v2.rcp[index].pcap, value);
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

int nthw_mod_slc_lr_rcp_set(struct flow_api_backend_s *be, enum hw_slc_lr_e field, uint32_t index,
	uint32_t value)
{
	return hw_mod_slc_lr_rcp_mod(be, field, index, &value, 0);
}
