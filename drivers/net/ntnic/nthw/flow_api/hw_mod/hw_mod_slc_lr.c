/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <stdint.h>
#include <stdlib.h>

#include "hw_mod_backend.h"

#define _MOD_ "SLC_LR"
#define _VER_ be->slc_lr.ver

bool hw_mod_slc_lr_present(struct flow_api_backend_s *be)
{
	return be->iface->get_slc_lr_present(be->be_dev);
}

int hw_mod_slc_lr_alloc(struct flow_api_backend_s *be)
{
	_VER_ = be->iface->get_slc_lr_version(be->be_dev);
	NT_LOG(DBG, FILTER, "SLC LR MODULE VERSION  %i.%i", VER_MAJOR(_VER_), VER_MINOR(_VER_));

	switch (_VER_) {
	case 2:
		if (!callocate_mod((struct common_func_s *)&be->slc_lr, 1, &be->slc_lr.v2.rcp,
				be->max_categories, sizeof(struct slc_lr_v2_rcp_s)))
			return -1;

		break;

	default:
		UNSUP_VER_LOG;
		return UNSUP_VER;
	}

	return 0;
}

void hw_mod_slc_lr_free(struct flow_api_backend_s *be)
{
	if (be->slc_lr.base) {
		free(be->slc_lr.base);
		be->slc_lr.base = NULL;
	}
}

int hw_mod_slc_lr_reset(struct flow_api_backend_s *be)
{
	/* Zero entire cache area */
	zero_module_cache((struct common_func_s *)(&be->slc_lr));

	NT_LOG(DBG, FILTER, "INIT SLC LR RCP");
	return hw_mod_slc_lr_rcp_flush(be, 0, be->max_categories);
}

int hw_mod_slc_lr_rcp_flush(struct flow_api_backend_s *be, int start_idx, int count)
{
	if (count == ALL_ENTRIES)
		count = be->max_categories;

	if ((unsigned int)(start_idx + count) > be->max_categories) {
		INDEX_TOO_LARGE_LOG;
		return INDEX_TOO_LARGE;
	}

	return be->iface->slc_lr_rcp_flush(be->be_dev, &be->slc_lr, start_idx, count);
}
