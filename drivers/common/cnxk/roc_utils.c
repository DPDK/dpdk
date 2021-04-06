/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

const char *
roc_error_msg_get(int errorcode)
{
	const char *err_msg;

	switch (errorcode) {
	case NPA_ERR_PARAM:
	case UTIL_ERR_PARAM:
		err_msg = "Invalid parameter";
		break;
	case NPA_ERR_ALLOC:
		err_msg = "NPA alloc failed";
		break;
	case NPA_ERR_INVALID_BLOCK_SZ:
		err_msg = "NPA invalid block size";
		break;
	case NPA_ERR_AURA_ID_ALLOC:
		err_msg = "NPA aura id alloc failed";
		break;
	case NPA_ERR_AURA_POOL_INIT:
		err_msg = "NPA aura pool init failed";
		break;
	case NPA_ERR_AURA_POOL_FINI:
		err_msg = "NPA aura pool fini failed";
		break;
	case NPA_ERR_BASE_INVALID:
		err_msg = "NPA invalid base";
		break;
	case NPA_ERR_DEVICE_NOT_BOUNDED:
		err_msg = "NPA device is not bounded";
		break;
	case UTIL_ERR_FS:
		err_msg = "file operation failed";
		break;
	case UTIL_ERR_INVALID_MODEL:
		err_msg = "Invalid RoC model";
		break;
	default:
		/**
		 * Handle general error (as defined in linux errno.h)
		 */
		if (abs(errorcode) < 300)
			err_msg = strerror(abs(errorcode));
		else
			err_msg = "Unknown error code";
		break;
	}

	return err_msg;
}

void
roc_clk_freq_get(uint16_t *rclk_freq, uint16_t *sclk_freq)
{
	*rclk_freq = dev_rclk_freq;
	*sclk_freq = dev_sclk_freq;
}
