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
	case NIX_AF_ERR_PARAM:
	case NIX_ERR_PARAM:
	case NPA_ERR_PARAM:
	case UTIL_ERR_PARAM:
		err_msg = "Invalid parameter";
		break;
	case NIX_ERR_NO_MEM:
		err_msg = "Out of memory";
		break;
	case NIX_ERR_INVALID_RANGE:
		err_msg = "Range is not supported";
		break;
	case NIX_ERR_INTERNAL:
		err_msg = "Internal error";
		break;
	case NIX_ERR_OP_NOTSUP:
		err_msg = "Operation not supported";
		break;
	case NIX_ERR_QUEUE_INVALID_RANGE:
		err_msg = "Invalid Queue range";
		break;
	case NIX_ERR_AQ_READ_FAILED:
		err_msg = "AQ read failed";
		break;
	case NIX_ERR_AQ_WRITE_FAILED:
		err_msg = "AQ write failed";
		break;
	case NIX_ERR_NDC_SYNC:
		err_msg = "NDC Sync failed";
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
	case NIX_AF_ERR_AQ_FULL:
		err_msg = "AQ full";
		break;
	case NIX_AF_ERR_AQ_ENQUEUE:
		err_msg = "AQ enqueue failed";
		break;
	case NIX_AF_ERR_AF_LF_INVALID:
		err_msg = "Invalid NIX LF";
		break;
	case NIX_AF_ERR_AF_LF_ALLOC:
		err_msg = "NIX LF alloc failed";
		break;
	case NIX_AF_ERR_LF_RESET:
		err_msg = "NIX LF reset failed";
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
