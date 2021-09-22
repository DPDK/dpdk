/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <rte_log.h>
#include <rte_tm_driver.h>

#include "roc_api.h"
#include "roc_priv.h"

#include "cnxk_utils.h"

int
roc_nix_tm_err_to_rte_err(int errorcode)
{
	int err_type;

	switch (errorcode) {
	case NIX_ERR_TM_SHAPER_PKT_LEN_ADJUST:
		err_type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PKT_ADJUST_LEN;
		break;
	case NIX_ERR_TM_INVALID_COMMIT_SZ:
		err_type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_SIZE;
		break;
	case NIX_ERR_TM_INVALID_COMMIT_RATE:
		err_type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE;
		break;
	case NIX_ERR_TM_INVALID_PEAK_SZ:
		err_type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE;
		break;
	case NIX_ERR_TM_INVALID_PEAK_RATE:
		err_type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE;
		break;
	case NIX_ERR_TM_INVALID_SHAPER_PROFILE:
		err_type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		break;
	case NIX_ERR_TM_SHAPER_PROFILE_IN_USE:
		err_type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE;
		break;
	case NIX_ERR_TM_INVALID_NODE:
		err_type = RTE_TM_ERROR_TYPE_NODE_ID;
		break;
	case NIX_ERR_TM_PKT_MODE_MISMATCH:
		err_type = RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID;
		break;
	case NIX_ERR_TM_INVALID_PARENT:
	case NIX_ERR_TM_PARENT_PRIO_UPDATE:
		err_type = RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID;
		break;
	case NIX_ERR_TM_PRIO_ORDER:
	case NIX_ERR_TM_MULTIPLE_RR_GROUPS:
		err_type = RTE_TM_ERROR_TYPE_NODE_PRIORITY;
		break;
	case NIX_ERR_TM_PRIO_EXCEEDED:
		err_type = RTE_TM_ERROR_TYPE_CAPABILITIES;
		break;
	default:
		/**
		 * Handle general error (as defined in linux errno.h)
		 */
		if (abs(errorcode) < 300)
			err_type = errorcode;
		else
			err_type = RTE_TM_ERROR_TYPE_UNSPECIFIED;
		break;
	}

	return err_type;
}
