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
	case NPC_ERR_PARAM:
	case SSO_ERR_PARAM:
	case UTIL_ERR_PARAM:
		err_msg = "Invalid parameter";
		break;
	case NIX_ERR_NO_MEM:
	case NPC_ERR_NO_MEM:
		err_msg = "Out of memory";
		break;
	case NIX_ERR_INVALID_RANGE:
	case NPC_ERR_INVALID_RANGE:
		err_msg = "Range is not supported";
		break;
	case NIX_ERR_INTERNAL:
	case NPC_ERR_INTERNAL:
		err_msg = "Internal error";
		break;
	case NIX_ERR_OP_NOTSUP:
		err_msg = "Operation not supported";
		break;
	case NIX_ERR_HW_NOTSUP:
		err_msg = "Hardware does not support";
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
	case NIX_ERR_TM_LEAF_NODE_GET:
		err_msg = "TM leaf node get failed";
		break;
	case NIX_ERR_TM_INVALID_LVL:
		err_msg = "TM node level invalid";
		break;
	case NIX_ERR_TM_INVALID_PRIO:
		err_msg = "TM node priority invalid";
		break;
	case NIX_ERR_TM_INVALID_PARENT:
		err_msg = "TM parent id invalid";
		break;
	case NIX_ERR_TM_NODE_EXISTS:
		err_msg = "TM Node Exists";
		break;
	case NIX_ERR_TM_INVALID_NODE:
		err_msg = "TM node id invalid";
		break;
	case NIX_ERR_TM_INVALID_SHAPER_PROFILE:
		err_msg = "TM shaper profile invalid";
		break;
	case NIX_ERR_TM_PKT_MODE_MISMATCH:
		err_msg = "shaper profile pkt mode mismatch";
		break;
	case NIX_ERR_TM_WEIGHT_EXCEED:
		err_msg = "TM DWRR weight exceeded";
		break;
	case NIX_ERR_TM_CHILD_EXISTS:
		err_msg = "TM node children exists";
		break;
	case NIX_ERR_TM_INVALID_PEAK_SZ:
		err_msg = "TM peak size invalid";
		break;
	case NIX_ERR_TM_INVALID_PEAK_RATE:
		err_msg = "TM peak rate invalid";
		break;
	case NIX_ERR_TM_INVALID_COMMIT_SZ:
		err_msg = "TM commit size invalid";
		break;
	case NIX_ERR_TM_INVALID_COMMIT_RATE:
		err_msg = "TM commit rate invalid";
		break;
	case NIX_ERR_TM_SHAPER_PROFILE_IN_USE:
		err_msg = "TM shaper profile in use";
		break;
	case NIX_ERR_TM_SHAPER_PROFILE_EXISTS:
		err_msg = "TM shaper profile exists";
		break;
	case NIX_ERR_TM_SHAPER_PKT_LEN_ADJUST:
		err_msg = "length adjust invalid";
		break;
	case NIX_ERR_TM_INVALID_TREE:
		err_msg = "TM tree invalid";
		break;
	case NIX_ERR_TM_PARENT_PRIO_UPDATE:
		err_msg = "TM node parent and prio update failed";
		break;
	case NIX_ERR_TM_PRIO_EXCEEDED:
		err_msg = "TM node priority exceeded";
		break;
	case NIX_ERR_TM_PRIO_ORDER:
		err_msg = "TM node priority not in order";
		break;
	case NIX_ERR_TM_MULTIPLE_RR_GROUPS:
		err_msg = "TM multiple rr groups";
		break;
	case NIX_ERR_TM_SQ_UPDATE_FAIL:
		err_msg = "TM SQ update failed";
		break;
	case NIX_ERR_NDC_SYNC:
		err_msg = "NDC Sync failed";
		break;
	case NPC_ERR_INVALID_SPEC:
		err_msg = "NPC invalid spec";
		break;
	case NPC_ERR_INVALID_MASK:
		err_msg = "NPC invalid mask";
		break;
	case NPC_ERR_INVALID_KEX:
		err_msg = "NPC invalid key";
		break;
	case NPC_ERR_INVALID_SIZE:
		err_msg = "NPC invalid key size";
		break;
	case NPC_ERR_ACTION_NOTSUP:
		err_msg = "NPC action not supported";
		break;
	case NPC_ERR_PATTERN_NOTSUP:
		err_msg = "NPC pattern not supported";
		break;
	case NPC_ERR_MCAM_ALLOC:
		err_msg = "MCAM entry alloc failed";
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
	case NIX_AF_ERR_TLX_INVALID:
		err_msg = "Invalid NIX TLX";
		break;
	case NIX_AF_ERR_TLX_ALLOC_FAIL:
		err_msg = "NIX TLX alloc failed";
		break;
	case NIX_AF_ERR_RSS_SIZE_INVALID:
		err_msg = "Invalid RSS size";
		break;
	case NIX_AF_ERR_RSS_GRPS_INVALID:
		err_msg = "Invalid RSS groups";
		break;
	case NIX_AF_ERR_FRS_INVALID:
		err_msg = "Invalid frame size";
		break;
	case NIX_AF_ERR_RX_LINK_INVALID:
		err_msg = "Invalid Rx link";
		break;
	case NIX_AF_INVAL_TXSCHQ_CFG:
		err_msg = "Invalid Tx scheduling config";
		break;
	case NIX_AF_SMQ_FLUSH_FAILED:
		err_msg = "SMQ flush failed";
		break;
	case NIX_AF_ERR_LF_RESET:
		err_msg = "NIX LF reset failed";
		break;
	case NIX_AF_ERR_MARK_CFG_FAIL:
		err_msg = "Marking config failed";
		break;
	case NIX_AF_ERR_LSO_CFG_FAIL:
		err_msg = "LSO config failed";
		break;
	case NIX_AF_INVAL_NPA_PF_FUNC:
		err_msg = "Invalid NPA pf_func";
		break;
	case NIX_AF_INVAL_SSO_PF_FUNC:
		err_msg = "Invalid SSO pf_func";
		break;
	case NIX_AF_ERR_TX_VTAG_NOSPC:
		err_msg = "No space for Tx VTAG";
		break;
	case NIX_AF_ERR_RX_VTAG_INUSE:
		err_msg = "Rx VTAG is in use";
		break;
	case NIX_AF_ERR_PTP_CONFIG_FAIL:
		err_msg = "PTP config failed";
		break;
	case SSO_ERR_DEVICE_NOT_BOUNDED:
		err_msg = "SSO pf/vf not found";
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
