/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <unistd.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_version.h>
#include <rte_io.h>

#include "bnxt.h"
#include "bnxt_filter.h"
#include "bnxt_hwrm.h"
#include "bnxt_rxq.h"
#include "bnxt_rxr.h"
#include "bnxt_ring.h"
#include "bnxt_txq.h"
#include "bnxt_txr.h"
#include "bnxt_vnic.h"
#include "hsi_struct_def_dpdk.h"

#define HWRM_SPEC_CODE_1_8_3		0x10803
#define HWRM_VERSION_1_9_1		0x10901
#define HWRM_VERSION_1_9_2		0x10903
#define HWRM_VERSION_1_10_2_13		0x10a020d
struct bnxt_plcmodes_cfg {
	uint32_t	flags;
	uint16_t	jumbo_thresh;
	uint16_t	hds_offset;
	uint16_t	hds_threshold;
};

static int page_getenum(size_t size)
{
	if (size <= 1 << 4)
		return 4;
	if (size <= 1 << 12)
		return 12;
	if (size <= 1 << 13)
		return 13;
	if (size <= 1 << 16)
		return 16;
	if (size <= 1 << 21)
		return 21;
	if (size <= 1 << 22)
		return 22;
	if (size <= 1 << 30)
		return 30;
	PMD_DRV_LOG(ERR, "Page size %zu out of range\n", size);
	return sizeof(int) * 8 - 1;
}

static int page_roundup(size_t size)
{
	return 1 << page_getenum(size);
}

static void bnxt_hwrm_set_pg_attr(struct bnxt_ring_mem_info *rmem,
				  uint8_t *pg_attr,
				  uint64_t *pg_dir)
{
	if (rmem->nr_pages == 0)
		return;

	if (rmem->nr_pages > 1) {
		*pg_attr = 1;
		*pg_dir = rte_cpu_to_le_64(rmem->pg_tbl_map);
	} else {
		*pg_dir = rte_cpu_to_le_64(rmem->dma_arr[0]);
	}
}

static struct bnxt_cp_ring_info*
bnxt_get_ring_info_by_id(struct bnxt *bp, uint16_t rid, uint16_t type)
{
	struct bnxt_cp_ring_info *cp_ring = NULL;
	uint16_t i;

	switch (type) {
	case HWRM_RING_FREE_INPUT_RING_TYPE_RX:
	case HWRM_RING_FREE_INPUT_RING_TYPE_RX_AGG:
		/* FALLTHROUGH */
		for (i = 0; i < bp->rx_cp_nr_rings; i++) {
			struct bnxt_rx_queue *rxq = bp->rx_queues[i];

			if (rxq->cp_ring->cp_ring_struct->fw_ring_id ==
			    rte_cpu_to_le_16(rid)) {
				return rxq->cp_ring;
			}
		}
		break;
	case HWRM_RING_FREE_INPUT_RING_TYPE_TX:
		for (i = 0; i < bp->tx_cp_nr_rings; i++) {
			struct bnxt_tx_queue *txq = bp->tx_queues[i];

			if (txq->cp_ring->cp_ring_struct->fw_ring_id ==
			    rte_cpu_to_le_16(rid)) {
				return txq->cp_ring;
			}
		}
		break;
	default:
		return cp_ring;
	}
	return cp_ring;
}

/* Complete a sweep of the CQ ring for the corresponding Tx/Rx/AGG ring.
 * If the CMPL_BASE_TYPE_HWRM_DONE is not encountered by the last pass,
 * before timeout, we force the done bit for the cleanup to proceed.
 * Also if cpr is null, do nothing.. The HWRM command is  not for a
 * Tx/Rx/AGG ring cleanup.
 */
static int
bnxt_check_cq_hwrm_done(struct bnxt_cp_ring_info *cpr,
			bool tx, bool rx, bool timeout)
{
	int done = 0;

	if (cpr != NULL) {
		if (tx)
			done = bnxt_flush_tx_cmp(cpr);

		if (rx)
			done = bnxt_flush_rx_cmp(cpr);

		if (done)
			PMD_DRV_LOG(DEBUG, "HWRM DONE for %s ring\n",
				    rx ? "Rx" : "Tx");

		/* We are about to timeout and still haven't seen the
		 * HWRM done for the Ring free. Force the cleanup.
		 */
		if (!done && timeout) {
			done = 1;
			PMD_DRV_LOG(DEBUG, "Timing out for %s ring\n",
				    rx ? "Rx" : "Tx");
		}
	} else {
		/* This HWRM command is not for a Tx/Rx/AGG ring cleanup.
		 * Otherwise the cpr would have been valid. So do nothing.
		 */
		done = 1;
	}

	return done;
}

/*
 * HWRM Functions (sent to HWRM)
 * These are named bnxt_hwrm_*() and return 0 on success or -110 if the
 * HWRM command times out, or a negative error code if the HWRM
 * command was failed by the FW.
 */

static int bnxt_hwrm_send_message(struct bnxt *bp, void *msg,
				  uint32_t msg_len, bool use_kong_mb)
{
	unsigned int i;
	struct input *req = msg;
	struct output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t *data = msg;
	uint8_t *bar;
	uint8_t *valid;
	uint16_t max_req_len = bp->max_req_len;
	struct hwrm_short_input short_input = { 0 };
	uint16_t bar_offset = use_kong_mb ?
		GRCPF_REG_KONG_CHANNEL_OFFSET : GRCPF_REG_CHIMP_CHANNEL_OFFSET;
	uint16_t mb_trigger_offset = use_kong_mb ?
		GRCPF_REG_KONG_COMM_TRIGGER : GRCPF_REG_CHIMP_COMM_TRIGGER;
	struct bnxt_cp_ring_info *cpr = NULL;
	bool is_rx = false;
	bool is_tx = false;
	uint32_t timeout;

	/* Do not send HWRM commands to firmware in error state */
	if (bp->flags & BNXT_FLAG_FATAL_ERROR)
		return 0;

	timeout = bp->hwrm_cmd_timeout;

	/* Update the message length for backing store config for new FW. */
	if (bp->fw_ver >= HWRM_VERSION_1_10_2_13 &&
	    rte_cpu_to_le_16(req->req_type) == HWRM_FUNC_BACKING_STORE_CFG)
		msg_len = BNXT_BACKING_STORE_CFG_LEGACY_LEN;

	if (bp->flags & BNXT_FLAG_SHORT_CMD ||
	    msg_len > bp->max_req_len) {
		void *short_cmd_req = bp->hwrm_short_cmd_req_addr;

		memset(short_cmd_req, 0, bp->hwrm_max_ext_req_len);
		memcpy(short_cmd_req, req, msg_len);

		short_input.req_type = rte_cpu_to_le_16(req->req_type);
		short_input.signature = rte_cpu_to_le_16(
					HWRM_SHORT_INPUT_SIGNATURE_SHORT_CMD);
		short_input.size = rte_cpu_to_le_16(msg_len);
		short_input.req_addr =
			rte_cpu_to_le_64(bp->hwrm_short_cmd_req_dma_addr);

		data = (uint32_t *)&short_input;
		msg_len = sizeof(short_input);

		max_req_len = BNXT_HWRM_SHORT_REQ_LEN;
	}

	/* Write request msg to hwrm channel */
	for (i = 0; i < msg_len; i += 4) {
		bar = (uint8_t *)bp->bar0 + bar_offset + i;
		rte_write32(*data, bar);
		data++;
	}

	/* Zero the rest of the request space */
	for (; i < max_req_len; i += 4) {
		bar = (uint8_t *)bp->bar0 + bar_offset + i;
		rte_write32(0, bar);
	}

	/* Ring channel doorbell */
	bar = (uint8_t *)bp->bar0 + mb_trigger_offset;
	rte_write32(1, bar);
	/*
	 * Make sure the channel doorbell ring command complete before
	 * reading the response to avoid getting stale or invalid
	 * responses.
	 */
	rte_io_mb();

	/* Check ring flush is done.
	 * This is valid only for Tx and Rx rings (including AGG rings).
	 * The Tx and Rx rings should be freed once the HW confirms all
	 * the internal buffers and BDs associated with the rings are
	 * consumed and the corresponding DMA is handled.
	 */
	if (rte_cpu_to_le_16(req->cmpl_ring) != INVALID_HW_RING_ID) {
		/* Check if the TxCQ matches. If that fails check if RxCQ
		 * matches. And if neither match, is_rx = false, is_tx = false.
		 */
		cpr = bnxt_get_ring_info_by_id(bp, req->cmpl_ring,
					       HWRM_RING_FREE_INPUT_RING_TYPE_TX);
		if (cpr == NULL) {
			/* Not a TxCQ. Check if the RxCQ matches. */
			cpr =
			bnxt_get_ring_info_by_id(bp, req->cmpl_ring,
						 HWRM_RING_FREE_INPUT_RING_TYPE_RX);
			if (cpr != NULL)
				is_rx = true;
		} else {
			is_tx = true;
		}
	}

	/* Poll for the valid bit */
	for (i = 0; i < timeout; i++) {
		int done;

		done = bnxt_check_cq_hwrm_done(cpr, is_tx, is_rx,
					       i == timeout - 1);
		/* Sanity check on the resp->resp_len */
		rte_io_rmb();
		if (resp->resp_len && resp->resp_len <= bp->max_resp_len) {
			/* Last byte of resp contains the valid key */
			valid = (uint8_t *)resp + resp->resp_len - 1;
			if (*valid == HWRM_RESP_VALID_KEY && done)
				break;
		}
		rte_delay_us(1);
	}

	if (i >= timeout) {
		/* Suppress VER_GET timeout messages during reset recovery */
		if (bp->flags & BNXT_FLAG_FW_RESET &&
		    rte_cpu_to_le_16(req->req_type) == HWRM_VER_GET)
			return -ETIMEDOUT;

		PMD_DRV_LOG(ERR,
			    "Error(timeout) sending msg 0x%04x, seq_id %d\n",
			    req->req_type, req->seq_id);
		return -ETIMEDOUT;
	}
	return 0;
}

/*
 * HWRM_PREP() should be used to prepare *ALL* HWRM commands. It grabs the
 * spinlock, and does initial processing.
 *
 * HWRM_CHECK_RESULT() returns errors on failure and may not be used.  It
 * releases the spinlock only if it returns. If the regular int return codes
 * are not used by the function, HWRM_CHECK_RESULT() should not be used
 * directly, rather it should be copied and modified to suit the function.
 *
 * HWRM_UNLOCK() must be called after all response processing is completed.
 */
#define HWRM_PREP(req, type, kong) do {	\
	rte_spinlock_lock(&bp->hwrm_lock); \
	if (bp->hwrm_cmd_resp_addr == NULL) { \
		rte_spinlock_unlock(&bp->hwrm_lock); \
		return -EACCES; \
	} \
	memset(bp->hwrm_cmd_resp_addr, 0, bp->max_resp_len); \
	(req)->req_type = rte_cpu_to_le_16(type); \
	(req)->cmpl_ring = rte_cpu_to_le_16(-1); \
	(req)->seq_id = kong ? rte_cpu_to_le_16(bp->kong_cmd_seq++) :\
		rte_cpu_to_le_16(bp->chimp_cmd_seq++); \
	(req)->target_id = rte_cpu_to_le_16(0xffff); \
	(req)->resp_addr = rte_cpu_to_le_64(bp->hwrm_cmd_resp_dma_addr); \
} while (0)

#define HWRM_CHECK_RESULT_SILENT() do {\
	if (rc) { \
		rte_spinlock_unlock(&bp->hwrm_lock); \
		return rc; \
	} \
	if (resp->error_code) { \
		rc = rte_le_to_cpu_16(resp->error_code); \
		rte_spinlock_unlock(&bp->hwrm_lock); \
		return rc; \
	} \
} while (0)

#define HWRM_CHECK_RESULT() do {\
	if (rc) { \
		PMD_DRV_LOG(ERR, "failed rc:%d\n", rc); \
		rte_spinlock_unlock(&bp->hwrm_lock); \
		if (rc == HWRM_ERR_CODE_RESOURCE_ACCESS_DENIED) \
			rc = -EACCES; \
		else if (rc == HWRM_ERR_CODE_RESOURCE_ALLOC_ERROR) \
			rc = -ENOSPC; \
		else if (rc == HWRM_ERR_CODE_INVALID_PARAMS) \
			rc = -EINVAL; \
		else if (rc == HWRM_ERR_CODE_CMD_NOT_SUPPORTED) \
			rc = -ENOTSUP; \
		else if (rc == HWRM_ERR_CODE_HOT_RESET_PROGRESS) \
			rc = -EAGAIN; \
		else if (rc > 0) \
			rc = -EIO; \
		return rc; \
	} \
	if (resp->error_code) { \
		rc = rte_le_to_cpu_16(resp->error_code); \
		if (resp->resp_len >= 16) { \
			struct hwrm_err_output *tmp_hwrm_err_op = \
						(void *)resp; \
			PMD_DRV_LOG(ERR, \
				"error %d:%d:%08x:%04x\n", \
				rc, tmp_hwrm_err_op->cmd_err, \
				rte_le_to_cpu_32(\
					tmp_hwrm_err_op->opaque_0), \
				rte_le_to_cpu_16(\
					tmp_hwrm_err_op->opaque_1)); \
		} else { \
			PMD_DRV_LOG(ERR, "error %d\n", rc); \
		} \
		rte_spinlock_unlock(&bp->hwrm_lock); \
		if (rc == HWRM_ERR_CODE_RESOURCE_ACCESS_DENIED) \
			rc = -EACCES; \
		else if (rc == HWRM_ERR_CODE_RESOURCE_ALLOC_ERROR) \
			rc = -ENOSPC; \
		else if (rc == HWRM_ERR_CODE_INVALID_PARAMS) \
			rc = -EINVAL; \
		else if (rc == HWRM_ERR_CODE_CMD_NOT_SUPPORTED) \
			rc = -ENOTSUP; \
		else if (rc == HWRM_ERR_CODE_HOT_RESET_PROGRESS) \
			rc = -EAGAIN; \
		else if (rc > 0) \
			rc = -EIO; \
		return rc; \
	} \
} while (0)

#define HWRM_UNLOCK()		rte_spinlock_unlock(&bp->hwrm_lock)

int bnxt_hwrm_tf_message_direct(struct bnxt *bp,
				bool use_kong_mb,
				uint16_t msg_type,
				void *msg,
				uint32_t msg_len,
				void *resp_msg,
				uint32_t resp_len)
{
	int rc = 0;
	bool mailbox = BNXT_USE_CHIMP_MB;
	struct input *req = msg;
	struct output *resp = bp->hwrm_cmd_resp_addr;

	if (use_kong_mb)
		mailbox = BNXT_USE_KONG(bp);

	HWRM_PREP(req, msg_type, mailbox);

	rc = bnxt_hwrm_send_message(bp, req, msg_len, mailbox);

	HWRM_CHECK_RESULT();

	if (resp_msg)
		memcpy(resp_msg, resp, resp_len);

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_tf_message_tunneled(struct bnxt *bp,
				  bool use_kong_mb,
				  uint16_t tf_type,
				  uint16_t tf_subtype,
				  uint32_t *tf_response_code,
				  void *msg,
				  uint32_t msg_len,
				  void *response,
				  uint32_t response_len)
{
	int rc = 0;
	struct hwrm_cfa_tflib_input req = { .req_type = 0 };
	struct hwrm_cfa_tflib_output *resp = bp->hwrm_cmd_resp_addr;
	bool mailbox = BNXT_USE_CHIMP_MB;

	if (msg_len > sizeof(req.tf_req))
		return -ENOMEM;

	if (use_kong_mb)
		mailbox = BNXT_USE_KONG(bp);

	HWRM_PREP(&req, HWRM_TF, mailbox);
	/* Build request using the user supplied request payload.
	 * TLV request size is checked at build time against HWRM
	 * request max size, thus no checking required.
	 */
	req.tf_type = tf_type;
	req.tf_subtype = tf_subtype;
	memcpy(req.tf_req, msg, msg_len);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), mailbox);
	HWRM_CHECK_RESULT();

	/* Copy the resp to user provided response buffer */
	if (response != NULL)
		/* Post process response data. We need to copy only
		 * the 'payload' as the HWRM data structure really is
		 * HWRM header + msg header + payload and the TFLIB
		 * only provided a payload place holder.
		 */
		if (response_len != 0) {
			memcpy(response,
			       resp->tf_resp,
			       response_len);
		}

	/* Extract the internal tflib response code */
	*tf_response_code = resp->tf_resp_code;
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_cfa_l2_clear_rx_mask(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_cfa_l2_set_rx_mask_input req = {.req_type = 0 };
	struct hwrm_cfa_l2_set_rx_mask_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_CFA_L2_SET_RX_MASK, BNXT_USE_CHIMP_MB);
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);
	req.mask = 0;

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_cfa_l2_set_rx_mask(struct bnxt *bp,
				 struct bnxt_vnic_info *vnic,
				 uint16_t vlan_count,
				 struct bnxt_vlan_table_entry *vlan_table)
{
	int rc = 0;
	struct hwrm_cfa_l2_set_rx_mask_input req = {.req_type = 0 };
	struct hwrm_cfa_l2_set_rx_mask_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t mask = 0;

	if (vnic->fw_vnic_id == INVALID_HW_RING_ID)
		return rc;

	HWRM_PREP(&req, HWRM_CFA_L2_SET_RX_MASK, BNXT_USE_CHIMP_MB);
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);

	if (vnic->flags & BNXT_VNIC_INFO_BCAST)
		mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST;
	if (vnic->flags & BNXT_VNIC_INFO_UNTAGGED)
		mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLAN_NONVLAN;

	if (vnic->flags & BNXT_VNIC_INFO_PROMISC)
		mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS;

	if (vnic->flags & BNXT_VNIC_INFO_ALLMULTI) {
		mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST;
	} else if (vnic->flags & BNXT_VNIC_INFO_MCAST) {
		mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_MCAST;
		req.num_mc_entries = rte_cpu_to_le_32(bp->nb_mc_addr);
		req.mc_tbl_addr = rte_cpu_to_le_64(bp->mc_list_dma_addr);
	}
	if (vlan_table) {
		if (!(mask & HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLAN_NONVLAN))
			mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLANONLY;
		req.vlan_tag_tbl_addr =
			rte_cpu_to_le_64(rte_malloc_virt2iova(vlan_table));
		req.num_vlan_tags = rte_cpu_to_le_32((uint32_t)vlan_count);
	}
	req.mask = rte_cpu_to_le_32(mask);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_cfa_vlan_antispoof_cfg(struct bnxt *bp, uint16_t fid,
			uint16_t vlan_count,
			struct bnxt_vlan_antispoof_table_entry *vlan_table)
{
	int rc = 0;
	struct hwrm_cfa_vlan_antispoof_cfg_input req = {.req_type = 0 };
	struct hwrm_cfa_vlan_antispoof_cfg_output *resp =
						bp->hwrm_cmd_resp_addr;

	/*
	 * Older HWRM versions did not support this command, and the set_rx_mask
	 * list was used for anti-spoof. In 1.8.0, the TX path configuration was
	 * removed from set_rx_mask call, and this command was added.
	 *
	 * This command is also present from 1.7.8.11 and higher,
	 * as well as 1.7.8.0
	 */
	if (bp->fw_ver < ((1 << 24) | (8 << 16))) {
		if (bp->fw_ver != ((1 << 24) | (7 << 16) | (8 << 8))) {
			if (bp->fw_ver < ((1 << 24) | (7 << 16) | (8 << 8) |
					(11)))
				return 0;
		}
	}
	HWRM_PREP(&req, HWRM_CFA_VLAN_ANTISPOOF_CFG, BNXT_USE_CHIMP_MB);
	req.fid = rte_cpu_to_le_16(fid);

	req.vlan_tag_mask_tbl_addr =
		rte_cpu_to_le_64(rte_malloc_virt2iova(vlan_table));
	req.num_vlan_entries = rte_cpu_to_le_32((uint32_t)vlan_count);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_clear_l2_filter(struct bnxt *bp,
			     struct bnxt_filter_info *filter)
{
	int rc = 0;
	struct bnxt_filter_info *l2_filter = filter;
	struct bnxt_vnic_info *vnic = NULL;
	struct hwrm_cfa_l2_filter_free_input req = {.req_type = 0 };
	struct hwrm_cfa_l2_filter_free_output *resp = bp->hwrm_cmd_resp_addr;

	if (filter->fw_l2_filter_id == UINT64_MAX)
		return 0;

	if (filter->matching_l2_fltr_ptr)
		l2_filter = filter->matching_l2_fltr_ptr;

	PMD_DRV_LOG(DEBUG, "filter: %p l2_filter: %p ref_cnt: %d\n",
		    filter, l2_filter, l2_filter->l2_ref_cnt);

	if (l2_filter->l2_ref_cnt == 0)
		return 0;

	if (l2_filter->l2_ref_cnt > 0)
		l2_filter->l2_ref_cnt--;

	if (l2_filter->l2_ref_cnt > 0)
		return 0;

	HWRM_PREP(&req, HWRM_CFA_L2_FILTER_FREE, BNXT_USE_CHIMP_MB);

	req.l2_filter_id = rte_cpu_to_le_64(filter->fw_l2_filter_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	filter->fw_l2_filter_id = UINT64_MAX;
	if (l2_filter->l2_ref_cnt == 0) {
		vnic = l2_filter->vnic;
		if (vnic) {
			STAILQ_REMOVE(&vnic->filter, l2_filter,
				      bnxt_filter_info, next);
			bnxt_free_filter(bp, l2_filter);
		}
	}

	return 0;
}

int bnxt_hwrm_set_l2_filter(struct bnxt *bp,
			 uint16_t dst_id,
			 struct bnxt_filter_info *filter)
{
	int rc = 0;
	struct hwrm_cfa_l2_filter_alloc_input req = {.req_type = 0 };
	struct hwrm_cfa_l2_filter_alloc_output *resp = bp->hwrm_cmd_resp_addr;
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	const struct rte_eth_vmdq_rx_conf *conf =
		    &dev_conf->rx_adv_conf.vmdq_rx_conf;
	uint32_t enables = 0;
	uint16_t j = dst_id - 1;

	//TODO: Is there a better way to add VLANs to each VNIC in case of VMDQ
	if ((dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_VMDQ_FLAG) &&
	    conf->pool_map[j].pools & (1UL << j)) {
		PMD_DRV_LOG(DEBUG,
			"Add vlan %u to vmdq pool %u\n",
			conf->pool_map[j].vlan_id, j);

		filter->l2_ivlan = conf->pool_map[j].vlan_id;
		filter->enables |=
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN |
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN_MASK;
	}

	if (filter->fw_l2_filter_id != UINT64_MAX)
		bnxt_hwrm_clear_l2_filter(bp, filter);

	HWRM_PREP(&req, HWRM_CFA_L2_FILTER_ALLOC, BNXT_USE_CHIMP_MB);

	/* PMD does not support XDP and RoCE */
	filter->flags |= HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_XDP_DISABLE |
			HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_TRAFFIC_L2;
	req.flags = rte_cpu_to_le_32(filter->flags);

	enables = filter->enables |
	      HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_DST_ID;
	req.dst_id = rte_cpu_to_le_16(dst_id);

	if (enables &
	    HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR)
		memcpy(req.l2_addr, filter->l2_addr,
		       RTE_ETHER_ADDR_LEN);
	if (enables &
	    HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK)
		memcpy(req.l2_addr_mask, filter->l2_addr_mask,
		       RTE_ETHER_ADDR_LEN);
	if (enables &
	    HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN)
		req.l2_ovlan = filter->l2_ovlan;
	if (enables &
	    HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN)
		req.l2_ivlan = filter->l2_ivlan;
	if (enables &
	    HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN_MASK)
		req.l2_ovlan_mask = filter->l2_ovlan_mask;
	if (enables &
	    HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN_MASK)
		req.l2_ivlan_mask = filter->l2_ivlan_mask;
	if (enables & HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_SRC_ID)
		req.src_id = rte_cpu_to_le_32(filter->src_id);
	if (enables & HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_SRC_TYPE)
		req.src_type = filter->src_type;
	if (filter->pri_hint) {
		req.pri_hint = filter->pri_hint;
		req.l2_filter_id_hint =
			rte_cpu_to_le_64(filter->l2_filter_id_hint);
	}

	req.enables = rte_cpu_to_le_32(enables);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	filter->fw_l2_filter_id = rte_le_to_cpu_64(resp->l2_filter_id);
	filter->flow_id = rte_le_to_cpu_32(resp->flow_id);
	HWRM_UNLOCK();

	filter->l2_ref_cnt++;

	return rc;
}

int bnxt_hwrm_ptp_cfg(struct bnxt *bp)
{
	struct hwrm_port_mac_cfg_input req = {.req_type = 0};
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	uint32_t flags = 0;
	int rc;

	if (!ptp)
		return 0;

	HWRM_PREP(&req, HWRM_PORT_MAC_CFG, BNXT_USE_CHIMP_MB);

	if (ptp->rx_filter)
		flags |= HWRM_PORT_MAC_CFG_INPUT_FLAGS_PTP_RX_TS_CAPTURE_ENABLE;
	else
		flags |=
			HWRM_PORT_MAC_CFG_INPUT_FLAGS_PTP_RX_TS_CAPTURE_DISABLE;
	if (ptp->tx_tstamp_en)
		flags |= HWRM_PORT_MAC_CFG_INPUT_FLAGS_PTP_TX_TS_CAPTURE_ENABLE;
	else
		flags |=
			HWRM_PORT_MAC_CFG_INPUT_FLAGS_PTP_TX_TS_CAPTURE_DISABLE;
	req.flags = rte_cpu_to_le_32(flags);
	req.enables = rte_cpu_to_le_32
		(HWRM_PORT_MAC_CFG_INPUT_ENABLES_RX_TS_CAPTURE_PTP_MSG_TYPE);
	req.rx_ts_capture_ptp_msg_type = rte_cpu_to_le_16(ptp->rxctl);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_UNLOCK();

	return rc;
}

static int bnxt_hwrm_ptp_qcfg(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_port_mac_ptp_qcfg_input req = {.req_type = 0};
	struct hwrm_port_mac_ptp_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;

	if (ptp)
		return 0;

	HWRM_PREP(&req, HWRM_PORT_MAC_PTP_QCFG, BNXT_USE_CHIMP_MB);

	req.port_id = rte_cpu_to_le_16(bp->pf->port_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	if (BNXT_CHIP_P5(bp)) {
		if (!(resp->flags & HWRM_PORT_MAC_PTP_QCFG_OUTPUT_FLAGS_HWRM_ACCESS))
			return 0;
	} else {
		if (!(resp->flags & HWRM_PORT_MAC_PTP_QCFG_OUTPUT_FLAGS_DIRECT_ACCESS))
			return 0;
	}

	if (resp->flags & HWRM_PORT_MAC_PTP_QCFG_OUTPUT_FLAGS_ONE_STEP_TX_TS)
		bp->flags |= BNXT_FLAG_FW_CAP_ONE_STEP_TX_TS;

	ptp = rte_zmalloc("ptp_cfg", sizeof(*ptp), 0);
	if (!ptp)
		return -ENOMEM;

	if (!BNXT_CHIP_P5(bp)) {
		ptp->rx_regs[BNXT_PTP_RX_TS_L] =
			rte_le_to_cpu_32(resp->rx_ts_reg_off_lower);
		ptp->rx_regs[BNXT_PTP_RX_TS_H] =
			rte_le_to_cpu_32(resp->rx_ts_reg_off_upper);
		ptp->rx_regs[BNXT_PTP_RX_SEQ] =
			rte_le_to_cpu_32(resp->rx_ts_reg_off_seq_id);
		ptp->rx_regs[BNXT_PTP_RX_FIFO] =
			rte_le_to_cpu_32(resp->rx_ts_reg_off_fifo);
		ptp->rx_regs[BNXT_PTP_RX_FIFO_ADV] =
			rte_le_to_cpu_32(resp->rx_ts_reg_off_fifo_adv);
		ptp->tx_regs[BNXT_PTP_TX_TS_L] =
			rte_le_to_cpu_32(resp->tx_ts_reg_off_lower);
		ptp->tx_regs[BNXT_PTP_TX_TS_H] =
			rte_le_to_cpu_32(resp->tx_ts_reg_off_upper);
		ptp->tx_regs[BNXT_PTP_TX_SEQ] =
			rte_le_to_cpu_32(resp->tx_ts_reg_off_seq_id);
		ptp->tx_regs[BNXT_PTP_TX_FIFO] =
			rte_le_to_cpu_32(resp->tx_ts_reg_off_fifo);
	}

	ptp->bp = bp;
	bp->ptp_cfg = ptp;

	return 0;
}

void bnxt_free_vf_info(struct bnxt *bp)
{
	int i;

	if (bp->pf == NULL)
		return;

	if (bp->pf->vf_info == NULL)
		return;

	for (i = 0; i < bp->pf->max_vfs; i++) {
		rte_free(bp->pf->vf_info[i].vlan_table);
		bp->pf->vf_info[i].vlan_table = NULL;
		rte_free(bp->pf->vf_info[i].vlan_as_table);
		bp->pf->vf_info[i].vlan_as_table = NULL;
	}
	rte_free(bp->pf->vf_info);
	bp->pf->vf_info = NULL;
}

static int bnxt_alloc_vf_info(struct bnxt *bp, uint16_t max_vfs)
{
	struct bnxt_child_vf_info *vf_info = bp->pf->vf_info;
	int i;

	if (vf_info)
		bnxt_free_vf_info(bp);

	vf_info = rte_zmalloc("bnxt_vf_info", sizeof(*vf_info) * max_vfs, 0);
	if (vf_info == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc vf info\n");
		return -ENOMEM;
	}

	bp->pf->max_vfs = max_vfs;
	for (i = 0; i < max_vfs; i++) {
		vf_info[i].fid = bp->pf->first_vf_id + i;
		vf_info[i].vlan_table = rte_zmalloc("VF VLAN table",
						    getpagesize(), getpagesize());
		if (vf_info[i].vlan_table == NULL) {
			PMD_DRV_LOG(ERR, "Failed to alloc VLAN table for VF %d\n", i);
			goto err;
		}
		rte_mem_lock_page(vf_info[i].vlan_table);

		vf_info[i].vlan_as_table = rte_zmalloc("VF VLAN AS table",
						       getpagesize(), getpagesize());
		if (vf_info[i].vlan_as_table == NULL) {
			PMD_DRV_LOG(ERR, "Failed to alloc VLAN AS table for VF %d\n", i);
			goto err;
		}
		rte_mem_lock_page(vf_info[i].vlan_as_table);

		STAILQ_INIT(&vf_info[i].filter);
	}

	bp->pf->vf_info = vf_info;

	return 0;
err:
	bnxt_free_vf_info(bp);
	return -ENOMEM;
}

static int __bnxt_hwrm_func_qcaps(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_func_qcaps_input req = {.req_type = 0 };
	struct hwrm_func_qcaps_output *resp = bp->hwrm_cmd_resp_addr;
	uint16_t new_max_vfs;
	uint32_t flags;

	HWRM_PREP(&req, HWRM_FUNC_QCAPS, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(0xffff);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	bp->max_ring_grps = rte_le_to_cpu_32(resp->max_hw_ring_grps);
	flags = rte_le_to_cpu_32(resp->flags);
	if (BNXT_PF(bp)) {
		bp->pf->port_id = resp->port_id;
		bp->pf->first_vf_id = rte_le_to_cpu_16(resp->first_vf_id);
		bp->pf->total_vfs = rte_le_to_cpu_16(resp->max_vfs);
		new_max_vfs = bp->pdev->max_vfs;
		if (new_max_vfs != bp->pf->max_vfs) {
			rc = bnxt_alloc_vf_info(bp, new_max_vfs);
			if (rc)
				goto unlock;
		}
	}

	bp->fw_fid = rte_le_to_cpu_32(resp->fid);
	if (!bnxt_check_zero_bytes(resp->mac_address, RTE_ETHER_ADDR_LEN)) {
		bp->flags |= BNXT_FLAG_DFLT_MAC_SET;
		memcpy(bp->mac_addr, &resp->mac_address, RTE_ETHER_ADDR_LEN);
	} else {
		bp->flags &= ~BNXT_FLAG_DFLT_MAC_SET;
	}
	bp->max_rsscos_ctx = rte_le_to_cpu_16(resp->max_rsscos_ctx);
	bp->max_cp_rings = rte_le_to_cpu_16(resp->max_cmpl_rings);
	bp->max_tx_rings = rte_le_to_cpu_16(resp->max_tx_rings);
	bp->max_rx_rings = rte_le_to_cpu_16(resp->max_rx_rings);
	bp->first_vf_id = rte_le_to_cpu_16(resp->first_vf_id);
	bp->max_rx_em_flows = rte_le_to_cpu_16(resp->max_rx_em_flows);
	bp->max_l2_ctx = rte_le_to_cpu_16(resp->max_l2_ctxs);
	if (!BNXT_CHIP_P5(bp) && !bp->pdev->max_vfs)
		bp->max_l2_ctx += bp->max_rx_em_flows;
	bp->max_vnics = rte_le_to_cpu_16(resp->max_vnics);
	PMD_DRV_LOG(DEBUG, "Max l2_cntxts is %d vnics is %d\n",
		    bp->max_l2_ctx, bp->max_vnics);
	bp->max_stat_ctx = rte_le_to_cpu_16(resp->max_stat_ctx);
	bp->max_mcast_addr = rte_le_to_cpu_32(resp->max_mcast_filters);

	if (BNXT_PF(bp)) {
		bp->pf->total_vnics = rte_le_to_cpu_16(resp->max_vnics);
		if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_PTP_SUPPORTED) {
			bp->flags |= BNXT_FLAG_PTP_SUPPORTED;
			PMD_DRV_LOG(DEBUG, "PTP SUPPORTED\n");
			HWRM_UNLOCK();
			bnxt_hwrm_ptp_qcfg(bp);
		}
	}

	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_EXT_STATS_SUPPORTED)
		bp->flags |= BNXT_FLAG_EXT_STATS_SUPPORTED;

	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_ERROR_RECOVERY_CAPABLE) {
		bp->fw_cap |= BNXT_FW_CAP_ERROR_RECOVERY;
		PMD_DRV_LOG(DEBUG, "Adapter Error recovery SUPPORTED\n");
	}

	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_ERR_RECOVER_RELOAD)
		bp->fw_cap |= BNXT_FW_CAP_ERR_RECOVER_RELOAD;

	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_HOT_RESET_CAPABLE)
		bp->fw_cap |= BNXT_FW_CAP_HOT_RESET;

	if (flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_LINK_ADMIN_STATUS_SUPPORTED)
		bp->fw_cap |= BNXT_FW_CAP_LINK_ADMIN;

	if (!(flags & HWRM_FUNC_QCAPS_OUTPUT_FLAGS_VLAN_ACCELERATION_TX_DISABLED)) {
		bp->fw_cap |= BNXT_FW_CAP_VLAN_TX_INSERT;
		PMD_DRV_LOG(DEBUG, "VLAN acceleration for TX is enabled\n");
	}
unlock:
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_func_qcaps(struct bnxt *bp)
{
	int rc;

	rc = __bnxt_hwrm_func_qcaps(bp);
	if (rc == -ENOMEM)
		return rc;

	if (!rc && bp->hwrm_spec_code >= HWRM_SPEC_CODE_1_8_3) {
		rc = bnxt_alloc_ctx_mem(bp);
		if (rc)
			return rc;

		/* On older FW,
		 * bnxt_hwrm_func_resc_qcaps can fail and cause init failure.
		 * But the error can be ignored. Return success.
		 */
		rc = bnxt_hwrm_func_resc_qcaps(bp);
		if (!rc)
			bp->flags |= BNXT_FLAG_NEW_RM;
	}

	return 0;
}

/* VNIC cap covers capability of all VNICs. So no need to pass vnic_id */
int bnxt_hwrm_vnic_qcaps(struct bnxt *bp)
{
	int rc = 0;
	uint32_t flags;
	struct hwrm_vnic_qcaps_input req = {.req_type = 0 };
	struct hwrm_vnic_qcaps_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_VNIC_QCAPS, BNXT_USE_CHIMP_MB);

	req.target_id = rte_cpu_to_le_16(0xffff);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	flags = rte_le_to_cpu_32(resp->flags);

	if (flags & HWRM_VNIC_QCAPS_OUTPUT_FLAGS_COS_ASSIGNMENT_CAP) {
		bp->vnic_cap_flags |= BNXT_VNIC_CAP_COS_CLASSIFY;
		PMD_DRV_LOG(INFO, "CoS assignment capability enabled\n");
	}

	if (flags & HWRM_VNIC_QCAPS_OUTPUT_FLAGS_OUTERMOST_RSS_CAP)
		bp->vnic_cap_flags |= BNXT_VNIC_CAP_OUTER_RSS;

	if (flags & HWRM_VNIC_QCAPS_OUTPUT_FLAGS_RX_CMPL_V2_CAP)
		bp->vnic_cap_flags |= BNXT_VNIC_CAP_RX_CMPL_V2;

	if (flags & HWRM_VNIC_QCAPS_OUTPUT_FLAGS_VLAN_STRIP_CAP) {
		bp->vnic_cap_flags |= BNXT_VNIC_CAP_VLAN_RX_STRIP;
		PMD_DRV_LOG(DEBUG, "Rx VLAN strip capability enabled\n");
	}

	bp->max_tpa_v2 = rte_le_to_cpu_16(resp->max_aggs_supported);

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_func_reset(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_func_reset_input req = {.req_type = 0 };
	struct hwrm_func_reset_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_FUNC_RESET, BNXT_USE_CHIMP_MB);

	req.enables = rte_cpu_to_le_32(0);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_func_driver_register(struct bnxt *bp)
{
	int rc;
	uint32_t flags = 0;
	struct hwrm_func_drv_rgtr_input req = {.req_type = 0 };
	struct hwrm_func_drv_rgtr_output *resp = bp->hwrm_cmd_resp_addr;

	if (bp->flags & BNXT_FLAG_REGISTERED)
		return 0;

	if (bp->fw_cap & BNXT_FW_CAP_HOT_RESET)
		flags = HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_HOT_RESET_SUPPORT;
	if (bp->fw_cap & BNXT_FW_CAP_ERROR_RECOVERY)
		flags |= HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_ERROR_RECOVERY_SUPPORT;

	/* PFs and trusted VFs should indicate the support of the
	 * Master capability on non Stingray platform
	 */
	if ((BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp)) && !BNXT_STINGRAY(bp))
		flags |= HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_MASTER_SUPPORT;

	HWRM_PREP(&req, HWRM_FUNC_DRV_RGTR, BNXT_USE_CHIMP_MB);
	req.enables = rte_cpu_to_le_32(HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VER |
			HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_ASYNC_EVENT_FWD);
	req.ver_maj_8b = RTE_VER_YEAR;
	req.ver_min_8b = RTE_VER_MONTH;
	req.ver_upd_8b = RTE_VER_MINOR;

	if (BNXT_PF(bp)) {
		req.enables |= rte_cpu_to_le_32(
			HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VF_REQ_FWD);
		memcpy(req.vf_req_fwd, bp->pf->vf_req_fwd,
		       RTE_MIN(sizeof(req.vf_req_fwd),
			       sizeof(bp->pf->vf_req_fwd)));
	}

	req.flags = rte_cpu_to_le_32(flags);

	req.async_event_fwd[0] |=
		rte_cpu_to_le_32(ASYNC_CMPL_EVENT_ID_LINK_STATUS_CHANGE |
				 ASYNC_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED |
				 ASYNC_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE |
				 ASYNC_CMPL_EVENT_ID_LINK_SPEED_CHANGE |
				 ASYNC_CMPL_EVENT_ID_RESET_NOTIFY);
	if (bp->fw_cap & BNXT_FW_CAP_ERROR_RECOVERY)
		req.async_event_fwd[0] |=
			rte_cpu_to_le_32(ASYNC_CMPL_EVENT_ID_ERROR_RECOVERY);
	req.async_event_fwd[1] |=
		rte_cpu_to_le_32(ASYNC_CMPL_EVENT_ID_PF_DRVR_UNLOAD |
				 ASYNC_CMPL_EVENT_ID_VF_CFG_CHANGE);
	if (BNXT_PF(bp))
		req.async_event_fwd[1] |=
			rte_cpu_to_le_32(ASYNC_CMPL_EVENT_ID_DBG_NOTIFICATION);

	if (BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))
		req.async_event_fwd[1] |=
		rte_cpu_to_le_32(ASYNC_CMPL_EVENT_ID_DEFAULT_VNIC_CHANGE);

	req.async_event_fwd[2] |=
		rte_cpu_to_le_32(ASYNC_CMPL_EVENT_ID_ECHO_REQUEST |
				 ASYNC_CMPL_EVENT_ID_ERROR_REPORT);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	flags = rte_le_to_cpu_32(resp->flags);
	if (flags & HWRM_FUNC_DRV_RGTR_OUTPUT_FLAGS_IF_CHANGE_SUPPORTED)
		bp->fw_cap |= BNXT_FW_CAP_IF_CHANGE;

	HWRM_UNLOCK();

	bp->flags |= BNXT_FLAG_REGISTERED;

	return rc;
}

int bnxt_hwrm_check_vf_rings(struct bnxt *bp)
{
	if (!(BNXT_VF(bp) && (bp->flags & BNXT_FLAG_NEW_RM)))
		return 0;

	return bnxt_hwrm_func_reserve_vf_resc(bp, true);
}

int bnxt_hwrm_func_reserve_vf_resc(struct bnxt *bp, bool test)
{
	int rc;
	uint32_t flags = 0;
	uint32_t enables;
	struct hwrm_func_vf_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_vf_cfg_input req = {0};

	HWRM_PREP(&req, HWRM_FUNC_VF_CFG, BNXT_USE_CHIMP_MB);

	enables = HWRM_FUNC_VF_CFG_INPUT_ENABLES_NUM_RX_RINGS  |
		  HWRM_FUNC_VF_CFG_INPUT_ENABLES_NUM_TX_RINGS   |
		  HWRM_FUNC_VF_CFG_INPUT_ENABLES_NUM_STAT_CTXS  |
		  HWRM_FUNC_VF_CFG_INPUT_ENABLES_NUM_CMPL_RINGS |
		  HWRM_FUNC_VF_CFG_INPUT_ENABLES_NUM_VNICS;

	if (BNXT_HAS_RING_GRPS(bp)) {
		enables |= HWRM_FUNC_VF_CFG_INPUT_ENABLES_NUM_HW_RING_GRPS;
		req.num_hw_ring_grps = rte_cpu_to_le_16(bp->rx_nr_rings);
	}

	req.num_tx_rings = rte_cpu_to_le_16(bp->tx_nr_rings);
	req.num_rx_rings = rte_cpu_to_le_16(bp->rx_nr_rings *
					    AGG_RING_MULTIPLIER);
	req.num_stat_ctxs = rte_cpu_to_le_16(bp->rx_nr_rings + bp->tx_nr_rings);
	req.num_cmpl_rings = rte_cpu_to_le_16(bp->rx_nr_rings +
					      bp->tx_nr_rings +
					      BNXT_NUM_ASYNC_CPR(bp));
	req.num_vnics = rte_cpu_to_le_16(bp->rx_nr_rings);
	if (bp->vf_resv_strategy ==
	    HWRM_FUNC_RESOURCE_QCAPS_OUTPUT_VF_RESV_STRATEGY_MINIMAL_STATIC) {
		enables |= HWRM_FUNC_VF_CFG_INPUT_ENABLES_NUM_VNICS |
			   HWRM_FUNC_VF_CFG_INPUT_ENABLES_NUM_L2_CTXS |
			   HWRM_FUNC_VF_CFG_INPUT_ENABLES_NUM_RSSCOS_CTXS;
		req.num_rsscos_ctxs = rte_cpu_to_le_16(BNXT_VF_RSV_NUM_RSS_CTX);
		req.num_l2_ctxs = rte_cpu_to_le_16(BNXT_VF_RSV_NUM_L2_CTX);
		req.num_vnics = rte_cpu_to_le_16(BNXT_VF_RSV_NUM_VNIC);
	} else if (bp->vf_resv_strategy ==
		   HWRM_FUNC_RESOURCE_QCAPS_OUTPUT_VF_RESV_STRATEGY_MAXIMAL) {
		enables |= HWRM_FUNC_VF_CFG_INPUT_ENABLES_NUM_RSSCOS_CTXS;
		req.num_rsscos_ctxs = rte_cpu_to_le_16(bp->max_rsscos_ctx);
	}

	if (test)
		flags = HWRM_FUNC_VF_CFG_INPUT_FLAGS_TX_ASSETS_TEST |
			HWRM_FUNC_VF_CFG_INPUT_FLAGS_RX_ASSETS_TEST |
			HWRM_FUNC_VF_CFG_INPUT_FLAGS_CMPL_ASSETS_TEST |
			HWRM_FUNC_VF_CFG_INPUT_FLAGS_RING_GRP_ASSETS_TEST |
			HWRM_FUNC_VF_CFG_INPUT_FLAGS_STAT_CTX_ASSETS_TEST |
			HWRM_FUNC_VF_CFG_INPUT_FLAGS_VNIC_ASSETS_TEST;

	if (test && BNXT_HAS_RING_GRPS(bp))
		flags |= HWRM_FUNC_VF_CFG_INPUT_FLAGS_RING_GRP_ASSETS_TEST;

	req.flags = rte_cpu_to_le_32(flags);
	req.enables |= rte_cpu_to_le_32(enables);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	if (test)
		HWRM_CHECK_RESULT_SILENT();
	else
		HWRM_CHECK_RESULT();

	HWRM_UNLOCK();
	return rc;
}

int bnxt_hwrm_func_resc_qcaps(struct bnxt *bp)
{
	int rc;
	struct hwrm_func_resource_qcaps_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_resource_qcaps_input req = {0};

	HWRM_PREP(&req, HWRM_FUNC_RESOURCE_QCAPS, BNXT_USE_CHIMP_MB);
	req.fid = rte_cpu_to_le_16(0xffff);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT_SILENT();

	bp->max_rsscos_ctx = rte_le_to_cpu_16(resp->max_rsscos_ctx);
	bp->max_cp_rings = rte_le_to_cpu_16(resp->max_cmpl_rings);
	bp->max_tx_rings = rte_le_to_cpu_16(resp->max_tx_rings);
	bp->max_rx_rings = rte_le_to_cpu_16(resp->max_rx_rings);
	bp->max_ring_grps = rte_le_to_cpu_32(resp->max_hw_ring_grps);
	/* func_resource_qcaps does not return max_rx_em_flows.
	 * So use the value provided by func_qcaps.
	 */
	bp->max_l2_ctx = rte_le_to_cpu_16(resp->max_l2_ctxs);
	if (!BNXT_CHIP_P5(bp) && !bp->pdev->max_vfs)
		bp->max_l2_ctx += bp->max_rx_em_flows;
	bp->max_vnics = rte_le_to_cpu_16(resp->max_vnics);
	bp->max_stat_ctx = rte_le_to_cpu_16(resp->max_stat_ctx);
	bp->max_nq_rings = rte_le_to_cpu_16(resp->max_msix);
	bp->vf_resv_strategy = rte_le_to_cpu_16(resp->vf_reservation_strategy);
	if (bp->vf_resv_strategy >
	    HWRM_FUNC_RESOURCE_QCAPS_OUTPUT_VF_RESV_STRATEGY_MINIMAL_STATIC)
		bp->vf_resv_strategy =
		HWRM_FUNC_RESOURCE_QCAPS_OUTPUT_VF_RESERVATION_STRATEGY_MAXIMAL;

	HWRM_UNLOCK();
	return rc;
}

int bnxt_hwrm_ver_get(struct bnxt *bp, uint32_t timeout)
{
	int rc = 0;
	struct hwrm_ver_get_input req = {.req_type = 0 };
	struct hwrm_ver_get_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t fw_version;
	uint16_t max_resp_len;
	char type[RTE_MEMZONE_NAMESIZE];
	uint32_t dev_caps_cfg;

	bp->max_req_len = HWRM_MAX_REQ_LEN;
	bp->hwrm_cmd_timeout = timeout;
	HWRM_PREP(&req, HWRM_VER_GET, BNXT_USE_CHIMP_MB);

	req.hwrm_intf_maj = HWRM_VERSION_MAJOR;
	req.hwrm_intf_min = HWRM_VERSION_MINOR;
	req.hwrm_intf_upd = HWRM_VERSION_UPDATE;

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	if (bp->flags & BNXT_FLAG_FW_RESET)
		HWRM_CHECK_RESULT_SILENT();
	else
		HWRM_CHECK_RESULT();

	if (resp->flags & HWRM_VER_GET_OUTPUT_FLAGS_DEV_NOT_RDY) {
		rc = -EAGAIN;
		goto error;
	}

	PMD_DRV_LOG(INFO, "%d.%d.%d:%d.%d.%d.%d\n",
		resp->hwrm_intf_maj_8b, resp->hwrm_intf_min_8b,
		resp->hwrm_intf_upd_8b, resp->hwrm_fw_maj_8b,
		resp->hwrm_fw_min_8b, resp->hwrm_fw_bld_8b,
		resp->hwrm_fw_rsvd_8b);
	bp->fw_ver = ((uint32_t)resp->hwrm_fw_maj_8b << 24) |
		     ((uint32_t)resp->hwrm_fw_min_8b << 16) |
		     ((uint32_t)resp->hwrm_fw_bld_8b << 8) |
		     resp->hwrm_fw_rsvd_8b;
	PMD_DRV_LOG(INFO, "Driver HWRM version: %d.%d.%d\n",
		HWRM_VERSION_MAJOR, HWRM_VERSION_MINOR, HWRM_VERSION_UPDATE);

	fw_version = resp->hwrm_intf_maj_8b << 16;
	fw_version |= resp->hwrm_intf_min_8b << 8;
	fw_version |= resp->hwrm_intf_upd_8b;
	bp->hwrm_spec_code = fw_version;

	/* def_req_timeout value is in milliseconds */
	bp->hwrm_cmd_timeout = rte_le_to_cpu_16(resp->def_req_timeout);
	/* convert timeout to usec */
	bp->hwrm_cmd_timeout *= 1000;
	if (!bp->hwrm_cmd_timeout)
		bp->hwrm_cmd_timeout = DFLT_HWRM_CMD_TIMEOUT;

	if (resp->hwrm_intf_maj_8b != HWRM_VERSION_MAJOR) {
		PMD_DRV_LOG(ERR, "Unsupported firmware API version\n");
		rc = -EINVAL;
		goto error;
	}

	if (bp->max_req_len > resp->max_req_win_len) {
		PMD_DRV_LOG(ERR, "Unsupported request length\n");
		rc = -EINVAL;
		goto error;
	}

	bp->chip_num = rte_le_to_cpu_16(resp->chip_num);

	bp->max_req_len = rte_le_to_cpu_16(resp->max_req_win_len);
	bp->hwrm_max_ext_req_len = rte_le_to_cpu_16(resp->max_ext_req_len);
	if (bp->hwrm_max_ext_req_len < HWRM_MAX_REQ_LEN)
		bp->hwrm_max_ext_req_len = HWRM_MAX_REQ_LEN;

	max_resp_len = rte_le_to_cpu_16(resp->max_resp_len);
	dev_caps_cfg = rte_le_to_cpu_32(resp->dev_caps_cfg);

	RTE_VERIFY(max_resp_len <= bp->max_resp_len);
	bp->max_resp_len = max_resp_len;

	if ((dev_caps_cfg &
		HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_SUPPORTED) &&
	    (dev_caps_cfg &
	     HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_REQUIRED)) {
		PMD_DRV_LOG(DEBUG, "Short command supported\n");
		bp->flags |= BNXT_FLAG_SHORT_CMD;
	}

	if (((dev_caps_cfg &
	      HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_SUPPORTED) &&
	     (dev_caps_cfg &
	      HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_REQUIRED)) ||
	    bp->hwrm_max_ext_req_len > HWRM_MAX_REQ_LEN) {
		sprintf(type, "bnxt_hwrm_short_" PCI_PRI_FMT,
			bp->pdev->addr.domain, bp->pdev->addr.bus,
			bp->pdev->addr.devid, bp->pdev->addr.function);

		rte_free(bp->hwrm_short_cmd_req_addr);

		bp->hwrm_short_cmd_req_addr =
				rte_malloc(type, bp->hwrm_max_ext_req_len, 0);
		if (bp->hwrm_short_cmd_req_addr == NULL) {
			rc = -ENOMEM;
			goto error;
		}
		bp->hwrm_short_cmd_req_dma_addr =
			rte_malloc_virt2iova(bp->hwrm_short_cmd_req_addr);
		if (bp->hwrm_short_cmd_req_dma_addr == RTE_BAD_IOVA) {
			rte_free(bp->hwrm_short_cmd_req_addr);
			PMD_DRV_LOG(ERR,
				"Unable to map buffer to physical memory.\n");
			rc = -ENOMEM;
			goto error;
		}
	}
	if (dev_caps_cfg &
	    HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_KONG_MB_CHNL_SUPPORTED) {
		bp->flags |= BNXT_FLAG_KONG_MB_EN;
		PMD_DRV_LOG(DEBUG, "Kong mailbox channel enabled\n");
	}
	if (dev_caps_cfg &
	    HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_TRUSTED_VF_SUPPORTED)
		PMD_DRV_LOG(DEBUG, "FW supports Trusted VFs\n");
	if (dev_caps_cfg &
	    HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_CFA_ADV_FLOW_MGNT_SUPPORTED) {
		bp->fw_cap |= BNXT_FW_CAP_ADV_FLOW_MGMT;
		PMD_DRV_LOG(DEBUG, "FW supports advanced flow management\n");
	}

	if (dev_caps_cfg &
	    HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_ADV_FLOW_COUNTERS_SUPPORTED) {
		PMD_DRV_LOG(DEBUG, "FW supports advanced flow counters\n");
		bp->fw_cap |= BNXT_FW_CAP_ADV_FLOW_COUNTERS;
	}

	if (dev_caps_cfg &
	    HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_CFA_TRUFLOW_SUPPORTED) {
		PMD_DRV_LOG(DEBUG, "Host-based truflow feature enabled.\n");
		bp->fw_cap |= BNXT_FW_CAP_TRUFLOW_EN;
	}

error:
	HWRM_UNLOCK();
	return rc;
}

int bnxt_hwrm_func_driver_unregister(struct bnxt *bp)
{
	int rc;
	struct hwrm_func_drv_unrgtr_input req = {.req_type = 0 };
	struct hwrm_func_drv_unrgtr_output *resp = bp->hwrm_cmd_resp_addr;

	if (!(bp->flags & BNXT_FLAG_REGISTERED))
		return 0;

	HWRM_PREP(&req, HWRM_FUNC_DRV_UNRGTR, BNXT_USE_CHIMP_MB);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	PMD_DRV_LOG(DEBUG, "Port %u: Unregistered with fw\n",
		    bp->eth_dev->data->port_id);

	return rc;
}

static int bnxt_hwrm_port_phy_cfg(struct bnxt *bp, struct bnxt_link_info *conf)
{
	int rc = 0;
	struct hwrm_port_phy_cfg_input req = {0};
	struct hwrm_port_phy_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t enables = 0;

	HWRM_PREP(&req, HWRM_PORT_PHY_CFG, BNXT_USE_CHIMP_MB);

	if (conf->link_up) {
		/* Setting Fixed Speed. But AutoNeg is ON, So disable it */
		if (bp->link_info->auto_mode && conf->link_speed) {
			req.auto_mode = HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_NONE;
			PMD_DRV_LOG(DEBUG, "Disabling AutoNeg\n");
		}

		req.flags = rte_cpu_to_le_32(conf->phy_flags);
		/*
		 * Note, ChiMP FW 20.2.1 and 20.2.2 return an error when we set
		 * any auto mode, even "none".
		 */
		if (!conf->link_speed) {
			/* No speeds specified. Enable AutoNeg - all speeds */
			enables |= HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_MODE;
			req.auto_mode =
				HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ALL_SPEEDS;
		} else {
			if (bp->link_info->link_signal_mode) {
				enables |=
				HWRM_PORT_PHY_CFG_IN_EN_FORCE_PAM4_LINK_SPEED;
				req.force_pam4_link_speed =
					rte_cpu_to_le_16(conf->link_speed);
			} else {
				req.force_link_speed =
					rte_cpu_to_le_16(conf->link_speed);
			}
		}
		/* AutoNeg - Advertise speeds specified. */
		if (conf->auto_link_speed_mask &&
		    !(conf->phy_flags & HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE)) {
			req.auto_mode =
				HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_SPEED_MASK;
			if (conf->auto_pam4_link_speed_mask &&
			    bp->link_info->link_signal_mode) {
				enables |=
				HWRM_PORT_PHY_CFG_IN_EN_AUTO_PAM4_LINK_SPD_MASK;
				req.auto_link_pam4_speed_mask =
				rte_cpu_to_le_16(conf->auto_pam4_link_speed_mask);
			} else {
				enables |=
				HWRM_PORT_PHY_CFG_IN_EN_AUTO_LINK_SPEED_MASK;
				req.auto_link_speed_mask =
				rte_cpu_to_le_16(conf->auto_link_speed_mask);
			}
		}
		if (conf->auto_link_speed &&
		!(conf->phy_flags & HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE))
			enables |=
				HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED;

		req.auto_duplex = conf->duplex;
		enables |= HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_DUPLEX;
		req.auto_pause = conf->auto_pause;
		req.force_pause = conf->force_pause;
		/* Set force_pause if there is no auto or if there is a force */
		if (req.auto_pause && !req.force_pause)
			enables |= HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_PAUSE;
		else
			enables |= HWRM_PORT_PHY_CFG_INPUT_ENABLES_FORCE_PAUSE;

		req.enables = rte_cpu_to_le_32(enables);
	} else {
		req.flags =
		rte_cpu_to_le_32(HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE_LINK_DWN);
		PMD_DRV_LOG(INFO, "Force Link Down\n");
	}

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

static int bnxt_hwrm_port_phy_qcfg(struct bnxt *bp,
				   struct bnxt_link_info *link_info)
{
	int rc = 0;
	struct hwrm_port_phy_qcfg_input req = {0};
	struct hwrm_port_phy_qcfg_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_PORT_PHY_QCFG, BNXT_USE_CHIMP_MB);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	link_info->phy_link_status = resp->link;
	link_info->link_up =
		(link_info->phy_link_status ==
		 HWRM_PORT_PHY_QCFG_OUTPUT_LINK_LINK) ? 1 : 0;
	link_info->link_speed = rte_le_to_cpu_16(resp->link_speed);
	link_info->duplex = resp->duplex_cfg;
	link_info->pause = resp->pause;
	link_info->auto_pause = resp->auto_pause;
	link_info->force_pause = resp->force_pause;
	link_info->auto_mode = resp->auto_mode;
	link_info->phy_type = resp->phy_type;
	link_info->media_type = resp->media_type;

	link_info->support_speeds = rte_le_to_cpu_16(resp->support_speeds);
	link_info->auto_link_speed = rte_le_to_cpu_16(resp->auto_link_speed);
	link_info->auto_link_speed_mask = rte_le_to_cpu_16(resp->auto_link_speed_mask);
	link_info->preemphasis = rte_le_to_cpu_32(resp->preemphasis);
	link_info->force_link_speed = rte_le_to_cpu_16(resp->force_link_speed);
	link_info->phy_ver[0] = resp->phy_maj;
	link_info->phy_ver[1] = resp->phy_min;
	link_info->phy_ver[2] = resp->phy_bld;
	link_info->link_signal_mode =
		rte_le_to_cpu_16(resp->active_fec_signal_mode);
	link_info->force_pam4_link_speed =
			rte_le_to_cpu_16(resp->force_pam4_link_speed);
	link_info->support_pam4_speeds =
			rte_le_to_cpu_16(resp->support_pam4_speeds);
	link_info->auto_pam4_link_speed_mask =
			rte_le_to_cpu_16(resp->auto_pam4_link_speed_mask);
	link_info->module_status = resp->module_status;
	HWRM_UNLOCK();

	PMD_DRV_LOG(DEBUG, "Link Speed:%d,Auto:%d:%x:%x,Support:%x,Force:%x\n",
		    link_info->link_speed, link_info->auto_mode,
		    link_info->auto_link_speed, link_info->auto_link_speed_mask,
		    link_info->support_speeds, link_info->force_link_speed);
	PMD_DRV_LOG(DEBUG, "Link Signal:%d,PAM::Auto:%x,Support:%x,Force:%x\n",
		    link_info->link_signal_mode,
		    link_info->auto_pam4_link_speed_mask,
		    link_info->support_pam4_speeds,
		    link_info->force_pam4_link_speed);
	return rc;
}

int bnxt_hwrm_port_phy_qcaps(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_port_phy_qcaps_input req = {0};
	struct hwrm_port_phy_qcaps_output *resp = bp->hwrm_cmd_resp_addr;
	struct bnxt_link_info *link_info = bp->link_info;

	if (BNXT_VF(bp) && !BNXT_VF_IS_TRUSTED(bp))
		return 0;

	HWRM_PREP(&req, HWRM_PORT_PHY_QCAPS, BNXT_USE_CHIMP_MB);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT_SILENT();

	bp->port_cnt = resp->port_cnt;
	if (resp->supported_speeds_auto_mode)
		link_info->support_auto_speeds =
			rte_le_to_cpu_16(resp->supported_speeds_auto_mode);
	if (resp->supported_pam4_speeds_auto_mode)
		link_info->support_pam4_auto_speeds =
			rte_le_to_cpu_16(resp->supported_pam4_speeds_auto_mode);

	HWRM_UNLOCK();

	/* Older firmware does not have supported_auto_speeds, so assume
	 * that all supported speeds can be autonegotiated.
	 */
	if (link_info->auto_link_speed_mask && !link_info->support_auto_speeds)
		link_info->support_auto_speeds = link_info->support_speeds;

	return 0;
}

static bool bnxt_find_lossy_profile(struct bnxt *bp)
{
	int i = 0;

	for (i = BNXT_COS_QUEUE_COUNT - 1; i >= 0; i--) {
		if (bp->tx_cos_queue[i].profile ==
		    HWRM_QUEUE_SERVICE_PROFILE_LOSSY) {
			bp->tx_cosq_id[0] = bp->tx_cos_queue[i].id;
			return true;
		}
	}
	return false;
}

static void bnxt_find_first_valid_profile(struct bnxt *bp)
{
	int i = 0;

	for (i = BNXT_COS_QUEUE_COUNT - 1; i >= 0; i--) {
		if (bp->tx_cos_queue[i].profile !=
		    HWRM_QUEUE_SERVICE_PROFILE_UNKNOWN &&
		    bp->tx_cos_queue[i].id !=
		    HWRM_QUEUE_SERVICE_PROFILE_UNKNOWN) {
			bp->tx_cosq_id[0] = bp->tx_cos_queue[i].id;
			break;
		}
	}
}

int bnxt_hwrm_queue_qportcfg(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_queue_qportcfg_input req = {.req_type = 0 };
	struct hwrm_queue_qportcfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t dir = HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_TX;
	int i;

get_rx_info:
	HWRM_PREP(&req, HWRM_QUEUE_QPORTCFG, BNXT_USE_CHIMP_MB);

	req.flags = rte_cpu_to_le_32(dir);
	/* HWRM Version >= 1.9.1 only if COS Classification is not required. */
	if (bp->hwrm_spec_code >= HWRM_VERSION_1_9_1 &&
	    !(bp->vnic_cap_flags & BNXT_VNIC_CAP_COS_CLASSIFY))
		req.drv_qmap_cap =
			HWRM_QUEUE_QPORTCFG_INPUT_DRV_QMAP_CAP_ENABLED;
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	if (dir == HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_TX) {
		GET_TX_QUEUE_INFO(0);
		GET_TX_QUEUE_INFO(1);
		GET_TX_QUEUE_INFO(2);
		GET_TX_QUEUE_INFO(3);
		GET_TX_QUEUE_INFO(4);
		GET_TX_QUEUE_INFO(5);
		GET_TX_QUEUE_INFO(6);
		GET_TX_QUEUE_INFO(7);
	} else  {
		GET_RX_QUEUE_INFO(0);
		GET_RX_QUEUE_INFO(1);
		GET_RX_QUEUE_INFO(2);
		GET_RX_QUEUE_INFO(3);
		GET_RX_QUEUE_INFO(4);
		GET_RX_QUEUE_INFO(5);
		GET_RX_QUEUE_INFO(6);
		GET_RX_QUEUE_INFO(7);
	}

	HWRM_UNLOCK();

	if (dir == HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_RX)
		goto done;

	if (bp->hwrm_spec_code < HWRM_VERSION_1_9_1) {
		bp->tx_cosq_id[0] = bp->tx_cos_queue[0].id;
	} else {
		int j;

		/* iterate and find the COSq profile to use for Tx */
		if (bp->vnic_cap_flags & BNXT_VNIC_CAP_COS_CLASSIFY) {
			for (j = 0, i = 0; i < BNXT_COS_QUEUE_COUNT; i++) {
				if (bp->tx_cos_queue[i].id != 0xff)
					bp->tx_cosq_id[j++] =
						bp->tx_cos_queue[i].id;
			}
		} else {
			/* When CoS classification is disabled, for normal NIC
			 * operations, ideally we should look to use LOSSY.
			 * If not found, fallback to the first valid profile
			 */
			if (!bnxt_find_lossy_profile(bp))
				bnxt_find_first_valid_profile(bp);

		}
	}

	bp->max_tc = resp->max_configurable_queues;
	bp->max_lltc = resp->max_configurable_lossless_queues;
	if (bp->max_tc > BNXT_MAX_QUEUE)
		bp->max_tc = BNXT_MAX_QUEUE;
	bp->max_q = bp->max_tc;

	if (dir == HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_TX) {
		dir = HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_RX;
		goto get_rx_info;
	}

done:
	return rc;
}

int bnxt_hwrm_ring_alloc(struct bnxt *bp,
			 struct bnxt_ring *ring,
			 uint32_t ring_type, uint32_t map_index,
			 uint32_t stats_ctx_id, uint32_t cmpl_ring_id,
			 uint16_t tx_cosq_id)
{
	int rc = 0;
	uint32_t enables = 0;
	struct hwrm_ring_alloc_input req = {.req_type = 0 };
	struct hwrm_ring_alloc_output *resp = bp->hwrm_cmd_resp_addr;
	struct rte_mempool *mb_pool;
	uint16_t rx_buf_size;

	HWRM_PREP(&req, HWRM_RING_ALLOC, BNXT_USE_CHIMP_MB);

	req.page_tbl_addr = rte_cpu_to_le_64(ring->bd_dma);
	req.fbo = rte_cpu_to_le_32(0);
	/* Association of ring index with doorbell index */
	req.logical_id = rte_cpu_to_le_16(map_index);
	req.length = rte_cpu_to_le_32(ring->ring_size);

	switch (ring_type) {
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_TX:
		req.ring_type = ring_type;
		req.cmpl_ring_id = rte_cpu_to_le_16(cmpl_ring_id);
		req.stat_ctx_id = rte_cpu_to_le_32(stats_ctx_id);
		req.queue_id = rte_cpu_to_le_16(tx_cosq_id);
		if (stats_ctx_id != INVALID_STATS_CTX_ID)
			enables |=
			HWRM_RING_ALLOC_INPUT_ENABLES_STAT_CTX_ID_VALID;
		break;
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX:
		req.ring_type = ring_type;
		req.cmpl_ring_id = rte_cpu_to_le_16(cmpl_ring_id);
		req.stat_ctx_id = rte_cpu_to_le_32(stats_ctx_id);
		if (BNXT_CHIP_P5(bp)) {
			mb_pool = bp->rx_queues[0]->mb_pool;
			rx_buf_size = rte_pktmbuf_data_room_size(mb_pool) -
				      RTE_PKTMBUF_HEADROOM;
			rx_buf_size = RTE_MIN(BNXT_MAX_PKT_LEN, rx_buf_size);
			req.rx_buf_size = rte_cpu_to_le_16(rx_buf_size);
			enables |=
				HWRM_RING_ALLOC_INPUT_ENABLES_RX_BUF_SIZE_VALID;
		}
		if (stats_ctx_id != INVALID_STATS_CTX_ID)
			enables |=
				HWRM_RING_ALLOC_INPUT_ENABLES_STAT_CTX_ID_VALID;
		break;
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL:
		req.ring_type = ring_type;
		if (BNXT_HAS_NQ(bp)) {
			/* Association of cp ring with nq */
			req.nq_ring_id = rte_cpu_to_le_16(cmpl_ring_id);
			enables |=
				HWRM_RING_ALLOC_INPUT_ENABLES_NQ_RING_ID_VALID;
		}
		req.int_mode = HWRM_RING_ALLOC_INPUT_INT_MODE_MSIX;
		break;
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ:
		req.ring_type = ring_type;
		req.page_size = BNXT_PAGE_SHFT;
		req.int_mode = HWRM_RING_ALLOC_INPUT_INT_MODE_MSIX;
		break;
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX_AGG:
		req.ring_type = ring_type;
		req.rx_ring_id = rte_cpu_to_le_16(ring->fw_rx_ring_id);

		mb_pool = bp->rx_queues[0]->mb_pool;
		rx_buf_size = rte_pktmbuf_data_room_size(mb_pool) -
			      RTE_PKTMBUF_HEADROOM;
		rx_buf_size = RTE_MIN(BNXT_MAX_PKT_LEN, rx_buf_size);
		req.rx_buf_size = rte_cpu_to_le_16(rx_buf_size);

		req.stat_ctx_id = rte_cpu_to_le_32(stats_ctx_id);
		enables |= HWRM_RING_ALLOC_INPUT_ENABLES_RX_RING_ID_VALID |
			   HWRM_RING_ALLOC_INPUT_ENABLES_RX_BUF_SIZE_VALID |
			   HWRM_RING_ALLOC_INPUT_ENABLES_STAT_CTX_ID_VALID;
		break;
	default:
		PMD_DRV_LOG(ERR, "hwrm alloc invalid ring type %d\n",
			ring_type);
		HWRM_UNLOCK();
		return -EINVAL;
	}
	req.enables = rte_cpu_to_le_32(enables);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	if (rc || resp->error_code) {
		if (rc == 0 && resp->error_code)
			rc = rte_le_to_cpu_16(resp->error_code);
		switch (ring_type) {
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL:
			PMD_DRV_LOG(ERR,
				"hwrm_ring_alloc cp failed. rc:%d\n", rc);
			HWRM_UNLOCK();
			return rc;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX:
			PMD_DRV_LOG(ERR,
				    "hwrm_ring_alloc rx failed. rc:%d\n", rc);
			HWRM_UNLOCK();
			return rc;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX_AGG:
			PMD_DRV_LOG(ERR,
				    "hwrm_ring_alloc rx agg failed. rc:%d\n",
				    rc);
			HWRM_UNLOCK();
			return rc;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_TX:
			PMD_DRV_LOG(ERR,
				    "hwrm_ring_alloc tx failed. rc:%d\n", rc);
			HWRM_UNLOCK();
			return rc;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ:
			PMD_DRV_LOG(ERR,
				    "hwrm_ring_alloc nq failed. rc:%d\n", rc);
			HWRM_UNLOCK();
			return rc;
		default:
			PMD_DRV_LOG(ERR, "Invalid ring. rc:%d\n", rc);
			HWRM_UNLOCK();
			return rc;
		}
	}

	ring->fw_ring_id = rte_le_to_cpu_16(resp->ring_id);
	HWRM_UNLOCK();
	return rc;
}

int bnxt_hwrm_ring_free(struct bnxt *bp,
			struct bnxt_ring *ring, uint32_t ring_type,
			uint16_t cp_ring_id)
{
	int rc;
	struct hwrm_ring_free_input req = {.req_type = 0 };
	struct hwrm_ring_free_output *resp = bp->hwrm_cmd_resp_addr;

	if (ring->fw_ring_id == INVALID_HW_RING_ID)
		return -EINVAL;

	HWRM_PREP(&req, HWRM_RING_FREE, BNXT_USE_CHIMP_MB);

	req.ring_type = ring_type;
	req.ring_id = rte_cpu_to_le_16(ring->fw_ring_id);
	req.cmpl_ring = rte_cpu_to_le_16(cp_ring_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	ring->fw_ring_id = INVALID_HW_RING_ID;

	if (rc || resp->error_code) {
		if (rc == 0 && resp->error_code)
			rc = rte_le_to_cpu_16(resp->error_code);
		HWRM_UNLOCK();

		switch (ring_type) {
		case HWRM_RING_FREE_INPUT_RING_TYPE_L2_CMPL:
			PMD_DRV_LOG(ERR, "hwrm_ring_free cp failed. rc:%d\n",
				rc);
			return rc;
		case HWRM_RING_FREE_INPUT_RING_TYPE_RX:
			PMD_DRV_LOG(ERR, "hwrm_ring_free rx failed. rc:%d\n",
				rc);
			return rc;
		case HWRM_RING_FREE_INPUT_RING_TYPE_TX:
			PMD_DRV_LOG(ERR, "hwrm_ring_free tx failed. rc:%d\n",
				rc);
			return rc;
		case HWRM_RING_FREE_INPUT_RING_TYPE_NQ:
			PMD_DRV_LOG(ERR,
				    "hwrm_ring_free nq failed. rc:%d\n", rc);
			return rc;
		case HWRM_RING_FREE_INPUT_RING_TYPE_RX_AGG:
			PMD_DRV_LOG(ERR,
				    "hwrm_ring_free agg failed. rc:%d\n", rc);
			return rc;
		default:
			PMD_DRV_LOG(ERR, "Invalid ring, rc:%d\n", rc);
			return rc;
		}
	}
	HWRM_UNLOCK();
	return 0;
}

int bnxt_hwrm_ring_grp_alloc(struct bnxt *bp, unsigned int idx)
{
	int rc = 0;
	struct hwrm_ring_grp_alloc_input req = {.req_type = 0 };
	struct hwrm_ring_grp_alloc_output *resp = bp->hwrm_cmd_resp_addr;

	/* Don't attempt to re-create the ring group if it is already created */
	if (bp->grp_info[idx].fw_grp_id != INVALID_HW_RING_ID)
		return 0;

	HWRM_PREP(&req, HWRM_RING_GRP_ALLOC, BNXT_USE_CHIMP_MB);

	req.cr = rte_cpu_to_le_16(bp->grp_info[idx].cp_fw_ring_id);
	req.rr = rte_cpu_to_le_16(bp->grp_info[idx].rx_fw_ring_id);
	req.ar = rte_cpu_to_le_16(bp->grp_info[idx].ag_fw_ring_id);
	req.sc = rte_cpu_to_le_16(bp->grp_info[idx].fw_stats_ctx);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	bp->grp_info[idx].fw_grp_id = rte_le_to_cpu_16(resp->ring_group_id);

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_ring_grp_free(struct bnxt *bp, unsigned int idx)
{
	int rc;
	struct hwrm_ring_grp_free_input req = {.req_type = 0 };
	struct hwrm_ring_grp_free_output *resp = bp->hwrm_cmd_resp_addr;

	if (bp->grp_info[idx].fw_grp_id == INVALID_HW_RING_ID)
		return 0;

	HWRM_PREP(&req, HWRM_RING_GRP_FREE, BNXT_USE_CHIMP_MB);

	req.ring_group_id = rte_cpu_to_le_16(bp->grp_info[idx].fw_grp_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	bp->grp_info[idx].fw_grp_id = INVALID_HW_RING_ID;
	return rc;
}

int bnxt_hwrm_stat_clear(struct bnxt *bp, struct bnxt_cp_ring_info *cpr)
{
	int rc = 0;
	struct hwrm_stat_ctx_clr_stats_input req = {.req_type = 0 };
	struct hwrm_stat_ctx_clr_stats_output *resp = bp->hwrm_cmd_resp_addr;

	if (cpr->hw_stats_ctx_id == HWRM_NA_SIGNATURE)
		return rc;

	HWRM_PREP(&req, HWRM_STAT_CTX_CLR_STATS, BNXT_USE_CHIMP_MB);

	req.stat_ctx_id = rte_cpu_to_le_32(cpr->hw_stats_ctx_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_stat_ctx_alloc(struct bnxt *bp, struct bnxt_cp_ring_info *cpr)
{
	int rc;
	struct hwrm_stat_ctx_alloc_input req = {.req_type = 0 };
	struct hwrm_stat_ctx_alloc_output *resp = bp->hwrm_cmd_resp_addr;

	if (cpr->hw_stats_ctx_id != HWRM_NA_SIGNATURE)
		return 0;

	HWRM_PREP(&req, HWRM_STAT_CTX_ALLOC, BNXT_USE_CHIMP_MB);

	req.update_period_ms = rte_cpu_to_le_32(0);

	req.stats_dma_addr = rte_cpu_to_le_64(cpr->hw_stats_map);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	cpr->hw_stats_ctx_id = rte_le_to_cpu_32(resp->stat_ctx_id);

	HWRM_UNLOCK();

	return rc;
}

static int bnxt_hwrm_stat_ctx_free(struct bnxt *bp, struct bnxt_cp_ring_info *cpr)
{
	int rc;
	struct hwrm_stat_ctx_free_input req = {.req_type = 0 };
	struct hwrm_stat_ctx_free_output *resp = bp->hwrm_cmd_resp_addr;

	if (cpr->hw_stats_ctx_id == HWRM_NA_SIGNATURE)
		return 0;

	HWRM_PREP(&req, HWRM_STAT_CTX_FREE, BNXT_USE_CHIMP_MB);

	req.stat_ctx_id = rte_cpu_to_le_32(cpr->hw_stats_ctx_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	cpr->hw_stats_ctx_id = HWRM_NA_SIGNATURE;

	return rc;
}

int bnxt_hwrm_vnic_alloc(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0, i, j;
	struct hwrm_vnic_alloc_input req = { 0 };
	struct hwrm_vnic_alloc_output *resp = bp->hwrm_cmd_resp_addr;

	if (!BNXT_HAS_RING_GRPS(bp))
		goto skip_ring_grps;

	/* map ring groups to this vnic */
	PMD_DRV_LOG(DEBUG, "Alloc VNIC. Start %x, End %x\n",
		vnic->start_grp_id, vnic->end_grp_id);
	for (i = vnic->start_grp_id, j = 0; i < vnic->end_grp_id; i++, j++)
		vnic->fw_grp_ids[j] = bp->grp_info[i].fw_grp_id;

	vnic->dflt_ring_grp = bp->grp_info[vnic->start_grp_id].fw_grp_id;
	vnic->rss_rule = (uint16_t)HWRM_NA_SIGNATURE;
	vnic->cos_rule = (uint16_t)HWRM_NA_SIGNATURE;
	vnic->lb_rule = (uint16_t)HWRM_NA_SIGNATURE;

skip_ring_grps:
	vnic->mru = BNXT_VNIC_MRU(bp->eth_dev->data->mtu);
	HWRM_PREP(&req, HWRM_VNIC_ALLOC, BNXT_USE_CHIMP_MB);

	if (vnic->func_default)
		req.flags =
			rte_cpu_to_le_32(HWRM_VNIC_ALLOC_INPUT_FLAGS_DEFAULT);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	vnic->fw_vnic_id = rte_le_to_cpu_16(resp->vnic_id);
	HWRM_UNLOCK();
	PMD_DRV_LOG(DEBUG, "VNIC ID %x\n", vnic->fw_vnic_id);
	return rc;
}

static int bnxt_hwrm_vnic_plcmodes_qcfg(struct bnxt *bp,
					struct bnxt_vnic_info *vnic,
					struct bnxt_plcmodes_cfg *pmode)
{
	int rc = 0;
	struct hwrm_vnic_plcmodes_qcfg_input req = {.req_type = 0 };
	struct hwrm_vnic_plcmodes_qcfg_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_VNIC_PLCMODES_QCFG, BNXT_USE_CHIMP_MB);

	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	pmode->flags = rte_le_to_cpu_32(resp->flags);
	/* dflt_vnic bit doesn't exist in the _cfg command */
	pmode->flags &= ~(HWRM_VNIC_PLCMODES_QCFG_OUTPUT_FLAGS_DFLT_VNIC);
	pmode->jumbo_thresh = rte_le_to_cpu_16(resp->jumbo_thresh);
	pmode->hds_offset = rte_le_to_cpu_16(resp->hds_offset);
	pmode->hds_threshold = rte_le_to_cpu_16(resp->hds_threshold);

	HWRM_UNLOCK();

	return rc;
}

static int bnxt_hwrm_vnic_plcmodes_cfg(struct bnxt *bp,
				       struct bnxt_vnic_info *vnic,
				       struct bnxt_plcmodes_cfg *pmode)
{
	int rc = 0;
	struct hwrm_vnic_plcmodes_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_plcmodes_cfg_output *resp = bp->hwrm_cmd_resp_addr;

	if (vnic->fw_vnic_id == INVALID_HW_RING_ID) {
		PMD_DRV_LOG(DEBUG, "VNIC ID %x\n", vnic->fw_vnic_id);
		return rc;
	}

	HWRM_PREP(&req, HWRM_VNIC_PLCMODES_CFG, BNXT_USE_CHIMP_MB);

	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);
	req.flags = rte_cpu_to_le_32(pmode->flags);
	req.jumbo_thresh = rte_cpu_to_le_16(pmode->jumbo_thresh);
	req.hds_offset = rte_cpu_to_le_16(pmode->hds_offset);
	req.hds_threshold = rte_cpu_to_le_16(pmode->hds_threshold);
	req.enables = rte_cpu_to_le_32(
	    HWRM_VNIC_PLCMODES_CFG_INPUT_ENABLES_HDS_THRESHOLD_VALID |
	    HWRM_VNIC_PLCMODES_CFG_INPUT_ENABLES_HDS_OFFSET_VALID |
	    HWRM_VNIC_PLCMODES_CFG_INPUT_ENABLES_JUMBO_THRESH_VALID
	);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_vnic_cfg(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct bnxt_plcmodes_cfg pmodes = { 0 };
	uint32_t ctx_enable_flag = 0;
	uint32_t enables = 0;

	if (vnic->fw_vnic_id == INVALID_HW_RING_ID) {
		PMD_DRV_LOG(DEBUG, "VNIC ID %x\n", vnic->fw_vnic_id);
		return rc;
	}

	rc = bnxt_hwrm_vnic_plcmodes_qcfg(bp, vnic, &pmodes);
	if (rc)
		return rc;

	HWRM_PREP(&req, HWRM_VNIC_CFG, BNXT_USE_CHIMP_MB);

	if (BNXT_CHIP_P5(bp)) {
		int dflt_rxq = vnic->start_grp_id;
		struct bnxt_rx_ring_info *rxr;
		struct bnxt_cp_ring_info *cpr;
		struct bnxt_rx_queue *rxq;
		int i;

		/*
		 * The first active receive ring is used as the VNIC
		 * default receive ring. If there are no active receive
		 * rings (all corresponding receive queues are stopped),
		 * the first receive ring is used.
		 */
		for (i = vnic->start_grp_id; i < vnic->end_grp_id; i++) {
			rxq = bp->eth_dev->data->rx_queues[i];
			if (rxq->rx_started) {
				dflt_rxq = i;
				break;
			}
		}

		rxq = bp->eth_dev->data->rx_queues[dflt_rxq];
		rxr = rxq->rx_ring;
		cpr = rxq->cp_ring;

		req.default_rx_ring_id =
			rte_cpu_to_le_16(rxr->rx_ring_struct->fw_ring_id);
		req.default_cmpl_ring_id =
			rte_cpu_to_le_16(cpr->cp_ring_struct->fw_ring_id);
		enables = HWRM_VNIC_CFG_INPUT_ENABLES_DEFAULT_RX_RING_ID |
			  HWRM_VNIC_CFG_INPUT_ENABLES_DEFAULT_CMPL_RING_ID;
		if (bp->vnic_cap_flags & BNXT_VNIC_CAP_RX_CMPL_V2) {
			enables |= HWRM_VNIC_CFG_INPUT_ENABLES_RX_CSUM_V2_MODE;
			req.rx_csum_v2_mode =
				HWRM_VNIC_CFG_INPUT_RX_CSUM_V2_MODE_ALL_OK;
		}
		goto config_mru;
	}

	/* Only RSS support for now TBD: COS & LB */
	enables = HWRM_VNIC_CFG_INPUT_ENABLES_DFLT_RING_GRP;
	if (vnic->lb_rule != 0xffff)
		ctx_enable_flag |= HWRM_VNIC_CFG_INPUT_ENABLES_LB_RULE;
	if (vnic->cos_rule != 0xffff)
		ctx_enable_flag |= HWRM_VNIC_CFG_INPUT_ENABLES_COS_RULE;
	if (vnic->rss_rule != (uint16_t)HWRM_NA_SIGNATURE) {
		ctx_enable_flag |= HWRM_VNIC_CFG_INPUT_ENABLES_MRU;
		ctx_enable_flag |= HWRM_VNIC_CFG_INPUT_ENABLES_RSS_RULE;
	}
	if (bp->vnic_cap_flags & BNXT_VNIC_CAP_COS_CLASSIFY) {
		ctx_enable_flag |= HWRM_VNIC_CFG_INPUT_ENABLES_QUEUE_ID;
		req.queue_id = rte_cpu_to_le_16(vnic->cos_queue_id);
	}

	enables |= ctx_enable_flag;
	req.dflt_ring_grp = rte_cpu_to_le_16(vnic->dflt_ring_grp);
	req.rss_rule = rte_cpu_to_le_16(vnic->rss_rule);
	req.cos_rule = rte_cpu_to_le_16(vnic->cos_rule);
	req.lb_rule = rte_cpu_to_le_16(vnic->lb_rule);

config_mru:
	req.enables = rte_cpu_to_le_32(enables);
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);
	req.mru = rte_cpu_to_le_16(vnic->mru);
	/* Configure default VNIC only once. */
	if (vnic->func_default && !(bp->flags & BNXT_FLAG_DFLT_VNIC_SET)) {
		req.flags |=
		    rte_cpu_to_le_32(HWRM_VNIC_CFG_INPUT_FLAGS_DEFAULT);
		bp->flags |= BNXT_FLAG_DFLT_VNIC_SET;
	}
	if (vnic->vlan_strip)
		req.flags |=
		    rte_cpu_to_le_32(HWRM_VNIC_CFG_INPUT_FLAGS_VLAN_STRIP_MODE);
	if (vnic->bd_stall)
		req.flags |=
		    rte_cpu_to_le_32(HWRM_VNIC_CFG_INPUT_FLAGS_BD_STALL_MODE);
	if (vnic->rss_dflt_cr)
		req.flags |= rte_cpu_to_le_32(
			HWRM_VNIC_QCFG_OUTPUT_FLAGS_RSS_DFLT_CR_MODE);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	rc = bnxt_hwrm_vnic_plcmodes_cfg(bp, vnic, &pmodes);

	return rc;
}

int bnxt_hwrm_vnic_qcfg(struct bnxt *bp, struct bnxt_vnic_info *vnic,
		int16_t fw_vf_id)
{
	int rc = 0;
	struct hwrm_vnic_qcfg_input req = {.req_type = 0 };
	struct hwrm_vnic_qcfg_output *resp = bp->hwrm_cmd_resp_addr;

	if (vnic->fw_vnic_id == INVALID_HW_RING_ID) {
		PMD_DRV_LOG(DEBUG, "VNIC QCFG ID %d\n", vnic->fw_vnic_id);
		return rc;
	}
	HWRM_PREP(&req, HWRM_VNIC_QCFG, BNXT_USE_CHIMP_MB);

	req.enables =
		rte_cpu_to_le_32(HWRM_VNIC_QCFG_INPUT_ENABLES_VF_ID_VALID);
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);
	req.vf_id = rte_cpu_to_le_16(fw_vf_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	vnic->dflt_ring_grp = rte_le_to_cpu_16(resp->dflt_ring_grp);
	vnic->rss_rule = rte_le_to_cpu_16(resp->rss_rule);
	vnic->cos_rule = rte_le_to_cpu_16(resp->cos_rule);
	vnic->lb_rule = rte_le_to_cpu_16(resp->lb_rule);
	vnic->mru = rte_le_to_cpu_16(resp->mru);
	vnic->func_default = rte_le_to_cpu_32(
			resp->flags) & HWRM_VNIC_QCFG_OUTPUT_FLAGS_DEFAULT;
	vnic->vlan_strip = rte_le_to_cpu_32(resp->flags) &
			HWRM_VNIC_QCFG_OUTPUT_FLAGS_VLAN_STRIP_MODE;
	vnic->bd_stall = rte_le_to_cpu_32(resp->flags) &
			HWRM_VNIC_QCFG_OUTPUT_FLAGS_BD_STALL_MODE;
	vnic->rss_dflt_cr = rte_le_to_cpu_32(resp->flags) &
			HWRM_VNIC_QCFG_OUTPUT_FLAGS_RSS_DFLT_CR_MODE;

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_vnic_ctx_alloc(struct bnxt *bp,
			     struct bnxt_vnic_info *vnic, uint16_t ctx_idx)
{
	int rc = 0;
	uint16_t ctx_id;
	struct hwrm_vnic_rss_cos_lb_ctx_alloc_input req = {.req_type = 0 };
	struct hwrm_vnic_rss_cos_lb_ctx_alloc_output *resp =
						bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_VNIC_RSS_COS_LB_CTX_ALLOC, BNXT_USE_CHIMP_MB);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();

	ctx_id = rte_le_to_cpu_16(resp->rss_cos_lb_ctx_id);
	if (!BNXT_HAS_RING_GRPS(bp))
		vnic->fw_grp_ids[ctx_idx] = ctx_id;
	else if (ctx_idx == 0)
		vnic->rss_rule = ctx_id;

	HWRM_UNLOCK();

	return rc;
}

static
int _bnxt_hwrm_vnic_ctx_free(struct bnxt *bp,
			     struct bnxt_vnic_info *vnic, uint16_t ctx_idx)
{
	int rc = 0;
	struct hwrm_vnic_rss_cos_lb_ctx_free_input req = {.req_type = 0 };
	struct hwrm_vnic_rss_cos_lb_ctx_free_output *resp =
						bp->hwrm_cmd_resp_addr;

	if (ctx_idx == (uint16_t)HWRM_NA_SIGNATURE) {
		PMD_DRV_LOG(DEBUG, "VNIC RSS Rule %x\n", vnic->rss_rule);
		return rc;
	}
	HWRM_PREP(&req, HWRM_VNIC_RSS_COS_LB_CTX_FREE, BNXT_USE_CHIMP_MB);

	req.rss_cos_lb_ctx_id = rte_cpu_to_le_16(ctx_idx);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_vnic_ctx_free(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;

	if (BNXT_CHIP_P5(bp)) {
		int j;

		for (j = 0; j < vnic->num_lb_ctxts; j++) {
			rc = _bnxt_hwrm_vnic_ctx_free(bp,
						      vnic,
						      vnic->fw_grp_ids[j]);
			vnic->fw_grp_ids[j] = INVALID_HW_RING_ID;
		}
		vnic->num_lb_ctxts = 0;
	} else {
		rc = _bnxt_hwrm_vnic_ctx_free(bp, vnic, vnic->rss_rule);
		vnic->rss_rule = INVALID_HW_RING_ID;
	}

	return rc;
}

int bnxt_hwrm_vnic_free(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_free_input req = {.req_type = 0 };
	struct hwrm_vnic_free_output *resp = bp->hwrm_cmd_resp_addr;

	if (vnic->fw_vnic_id == INVALID_HW_RING_ID) {
		PMD_DRV_LOG(DEBUG, "VNIC FREE ID %x\n", vnic->fw_vnic_id);
		return rc;
	}

	HWRM_PREP(&req, HWRM_VNIC_FREE, BNXT_USE_CHIMP_MB);

	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	vnic->fw_vnic_id = INVALID_HW_RING_ID;
	/* Configure default VNIC again if necessary. */
	if (vnic->func_default && (bp->flags & BNXT_FLAG_DFLT_VNIC_SET))
		bp->flags &= ~BNXT_FLAG_DFLT_VNIC_SET;

	return rc;
}

static int
bnxt_hwrm_vnic_rss_cfg_p5(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int i;
	int rc = 0;
	int nr_ctxs = vnic->num_lb_ctxts;
	struct hwrm_vnic_rss_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_rss_cfg_output *resp = bp->hwrm_cmd_resp_addr;

	for (i = 0; i < nr_ctxs; i++) {
		HWRM_PREP(&req, HWRM_VNIC_RSS_CFG, BNXT_USE_CHIMP_MB);

		req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);
		req.hash_type = rte_cpu_to_le_32(vnic->hash_type);
		req.hash_mode_flags = vnic->hash_mode;

		req.hash_key_tbl_addr =
			rte_cpu_to_le_64(vnic->rss_hash_key_dma_addr);

		req.ring_grp_tbl_addr =
			rte_cpu_to_le_64(vnic->rss_table_dma_addr +
					 i * HW_HASH_INDEX_SIZE);
		req.ring_table_pair_index = i;
		req.rss_ctx_idx = rte_cpu_to_le_16(vnic->fw_grp_ids[i]);

		rc = bnxt_hwrm_send_message(bp, &req, sizeof(req),
					    BNXT_USE_CHIMP_MB);

		HWRM_CHECK_RESULT();
		HWRM_UNLOCK();
	}

	return rc;
}

int bnxt_hwrm_vnic_rss_cfg(struct bnxt *bp,
			   struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_rss_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_rss_cfg_output *resp = bp->hwrm_cmd_resp_addr;

	if (!vnic->rss_table)
		return 0;

	if (BNXT_CHIP_P5(bp))
		return bnxt_hwrm_vnic_rss_cfg_p5(bp, vnic);

	HWRM_PREP(&req, HWRM_VNIC_RSS_CFG, BNXT_USE_CHIMP_MB);

	req.hash_type = rte_cpu_to_le_32(vnic->hash_type);
	req.hash_mode_flags = vnic->hash_mode;

	req.ring_grp_tbl_addr =
	    rte_cpu_to_le_64(vnic->rss_table_dma_addr);
	req.hash_key_tbl_addr =
	    rte_cpu_to_le_64(vnic->rss_hash_key_dma_addr);
	req.rss_ctx_idx = rte_cpu_to_le_16(vnic->rss_rule);
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_vnic_plcmode_cfg(struct bnxt *bp,
			struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_plcmodes_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_plcmodes_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint16_t size;

	if (vnic->fw_vnic_id == INVALID_HW_RING_ID) {
		PMD_DRV_LOG(DEBUG, "VNIC ID %x\n", vnic->fw_vnic_id);
		return rc;
	}

	HWRM_PREP(&req, HWRM_VNIC_PLCMODES_CFG, BNXT_USE_CHIMP_MB);

	req.flags = rte_cpu_to_le_32(
			HWRM_VNIC_PLCMODES_CFG_INPUT_FLAGS_JUMBO_PLACEMENT);

	req.enables = rte_cpu_to_le_32(
		HWRM_VNIC_PLCMODES_CFG_INPUT_ENABLES_JUMBO_THRESH_VALID);

	size = rte_pktmbuf_data_room_size(bp->rx_queues[0]->mb_pool);
	size -= RTE_PKTMBUF_HEADROOM;
	size = RTE_MIN(BNXT_MAX_PKT_LEN, size);

	req.jumbo_thresh = rte_cpu_to_le_16(size);
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_vnic_tpa_cfg(struct bnxt *bp,
			struct bnxt_vnic_info *vnic, bool enable)
{
	int rc = 0;
	struct hwrm_vnic_tpa_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_tpa_cfg_output *resp = bp->hwrm_cmd_resp_addr;

	if (BNXT_CHIP_P5(bp) && !bp->max_tpa_v2) {
		if (enable)
			PMD_DRV_LOG(ERR, "No HW support for LRO\n");
		return -ENOTSUP;
	}

	if (vnic->fw_vnic_id == INVALID_HW_RING_ID) {
		PMD_DRV_LOG(DEBUG, "Invalid vNIC ID\n");
		return 0;
	}

	HWRM_PREP(&req, HWRM_VNIC_TPA_CFG, BNXT_USE_CHIMP_MB);

	if (enable) {
		req.enables = rte_cpu_to_le_32(
				HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MAX_AGG_SEGS |
				HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MAX_AGGS |
				HWRM_VNIC_TPA_CFG_INPUT_ENABLES_MIN_AGG_LEN);
		req.flags = rte_cpu_to_le_32(
				HWRM_VNIC_TPA_CFG_INPUT_FLAGS_TPA |
				HWRM_VNIC_TPA_CFG_INPUT_FLAGS_ENCAP_TPA |
				HWRM_VNIC_TPA_CFG_INPUT_FLAGS_RSC_WND_UPDATE |
				HWRM_VNIC_TPA_CFG_INPUT_FLAGS_GRO |
				HWRM_VNIC_TPA_CFG_INPUT_FLAGS_AGG_WITH_ECN |
			HWRM_VNIC_TPA_CFG_INPUT_FLAGS_AGG_WITH_SAME_GRE_SEQ);
		req.max_aggs = rte_cpu_to_le_16(BNXT_TPA_MAX_AGGS(bp));
		req.max_agg_segs = rte_cpu_to_le_16(BNXT_TPA_MAX_SEGS(bp));
		req.min_agg_len = rte_cpu_to_le_32(512);
	}
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_func_vf_mac(struct bnxt *bp, uint16_t vf, const uint8_t *mac_addr)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	req.flags = rte_cpu_to_le_32(bp->pf->vf_info[vf].func_cfg_flags);
	req.enables = rte_cpu_to_le_32(
			HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_MAC_ADDR);
	memcpy(req.dflt_mac_addr, mac_addr, sizeof(req.dflt_mac_addr));
	req.fid = rte_cpu_to_le_16(bp->pf->vf_info[vf].fid);

	HWRM_PREP(&req, HWRM_FUNC_CFG, BNXT_USE_CHIMP_MB);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	bp->pf->vf_info[vf].random_mac = false;

	return rc;
}

int bnxt_hwrm_func_qstats_tx_drop(struct bnxt *bp, uint16_t fid,
				  uint64_t *dropped)
{
	int rc = 0;
	struct hwrm_func_qstats_input req = {.req_type = 0};
	struct hwrm_func_qstats_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_FUNC_QSTATS, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(fid);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	if (dropped)
		*dropped = rte_le_to_cpu_64(resp->tx_drop_pkts);

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_func_qstats(struct bnxt *bp, uint16_t fid,
			  struct rte_eth_stats *stats,
			  struct hwrm_func_qstats_output *func_qstats)
{
	int rc = 0;
	struct hwrm_func_qstats_input req = {.req_type = 0};
	struct hwrm_func_qstats_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_FUNC_QSTATS, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(fid);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	if (func_qstats)
		memcpy(func_qstats, resp,
		       sizeof(struct hwrm_func_qstats_output));

	if (!stats)
		goto exit;

	stats->ipackets = rte_le_to_cpu_64(resp->rx_ucast_pkts);
	stats->ipackets += rte_le_to_cpu_64(resp->rx_mcast_pkts);
	stats->ipackets += rte_le_to_cpu_64(resp->rx_bcast_pkts);
	stats->ibytes = rte_le_to_cpu_64(resp->rx_ucast_bytes);
	stats->ibytes += rte_le_to_cpu_64(resp->rx_mcast_bytes);
	stats->ibytes += rte_le_to_cpu_64(resp->rx_bcast_bytes);

	stats->opackets = rte_le_to_cpu_64(resp->tx_ucast_pkts);
	stats->opackets += rte_le_to_cpu_64(resp->tx_mcast_pkts);
	stats->opackets += rte_le_to_cpu_64(resp->tx_bcast_pkts);
	stats->obytes = rte_le_to_cpu_64(resp->tx_ucast_bytes);
	stats->obytes += rte_le_to_cpu_64(resp->tx_mcast_bytes);
	stats->obytes += rte_le_to_cpu_64(resp->tx_bcast_bytes);

	stats->imissed = rte_le_to_cpu_64(resp->rx_discard_pkts);
	stats->ierrors = rte_le_to_cpu_64(resp->rx_drop_pkts);
	stats->oerrors = rte_le_to_cpu_64(resp->tx_discard_pkts);

exit:
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_func_clr_stats(struct bnxt *bp, uint16_t fid)
{
	int rc = 0;
	struct hwrm_func_clr_stats_input req = {.req_type = 0};
	struct hwrm_func_clr_stats_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_FUNC_CLR_STATS, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(fid);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_clear_all_hwrm_stat_ctxs(struct bnxt *bp)
{
	unsigned int i;
	int rc = 0;

	for (i = 0; i < bp->rx_cp_nr_rings + bp->tx_cp_nr_rings; i++) {
		struct bnxt_tx_queue *txq;
		struct bnxt_rx_queue *rxq;
		struct bnxt_cp_ring_info *cpr;

		if (i >= bp->rx_cp_nr_rings) {
			txq = bp->tx_queues[i - bp->rx_cp_nr_rings];
			cpr = txq->cp_ring;
		} else {
			rxq = bp->rx_queues[i];
			cpr = rxq->cp_ring;
		}

		rc = bnxt_hwrm_stat_clear(bp, cpr);
		if (rc)
			return rc;
	}
	return 0;
}

static int
bnxt_free_all_hwrm_stat_ctxs(struct bnxt *bp)
{
	int rc;
	unsigned int i;
	struct bnxt_cp_ring_info *cpr;

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {

		cpr = bp->rx_queues[i]->cp_ring;
		if (BNXT_HAS_RING_GRPS(bp))
			bp->grp_info[i].fw_stats_ctx = -1;
		if (cpr == NULL)
			continue;
		rc = bnxt_hwrm_stat_ctx_free(bp, cpr);
		if (rc)
			return rc;
	}

	for (i = 0; i < bp->tx_cp_nr_rings; i++) {
		cpr = bp->tx_queues[i]->cp_ring;
		if (cpr == NULL)
			continue;
		rc = bnxt_hwrm_stat_ctx_free(bp, cpr);
		if (rc)
			return rc;
	}

	return 0;
}

static int
bnxt_free_all_hwrm_ring_grps(struct bnxt *bp)
{
	uint16_t idx;
	uint32_t rc = 0;

	if (!BNXT_HAS_RING_GRPS(bp))
		return 0;

	for (idx = 0; idx < bp->rx_cp_nr_rings; idx++) {

		if (bp->grp_info[idx].fw_grp_id == INVALID_HW_RING_ID)
			continue;

		rc = bnxt_hwrm_ring_grp_free(bp, idx);

		if (rc)
			return rc;
	}
	return rc;
}

void bnxt_free_nq_ring(struct bnxt *bp, struct bnxt_cp_ring_info *cpr)
{
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;

	bnxt_hwrm_ring_free(bp, cp_ring,
			    HWRM_RING_FREE_INPUT_RING_TYPE_NQ,
			    INVALID_HW_RING_ID);
	memset(cpr->cp_desc_ring, 0,
	       cpr->cp_ring_struct->ring_size * sizeof(*cpr->cp_desc_ring));
	cpr->cp_raw_cons = 0;
}

void bnxt_free_cp_ring(struct bnxt *bp, struct bnxt_cp_ring_info *cpr)
{
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;

	bnxt_hwrm_ring_free(bp, cp_ring,
			    HWRM_RING_FREE_INPUT_RING_TYPE_L2_CMPL,
			    INVALID_HW_RING_ID);
	memset(cpr->cp_desc_ring, 0,
	       cpr->cp_ring_struct->ring_size * sizeof(*cpr->cp_desc_ring));
	cpr->cp_raw_cons = 0;
}

void bnxt_free_hwrm_rx_ring(struct bnxt *bp, int queue_index)
{
	struct bnxt_rx_queue *rxq = bp->rx_queues[queue_index];
	struct bnxt_rx_ring_info *rxr = rxq ? rxq->rx_ring : NULL;
	struct bnxt_ring *ring = rxr ? rxr->rx_ring_struct : NULL;
	struct bnxt_cp_ring_info *cpr = rxq ? rxq->cp_ring : NULL;

	if (BNXT_HAS_RING_GRPS(bp))
		bnxt_hwrm_ring_grp_free(bp, queue_index);

	if (ring != NULL && cpr != NULL)
		bnxt_hwrm_ring_free(bp, ring,
				    HWRM_RING_FREE_INPUT_RING_TYPE_RX,
				    cpr->cp_ring_struct->fw_ring_id);
	if (BNXT_HAS_RING_GRPS(bp))
		bp->grp_info[queue_index].rx_fw_ring_id = INVALID_HW_RING_ID;

	/* Check agg ring struct explicitly.
	 * bnxt_need_agg_ring() returns the current state of offload flags,
	 * but we may have to deal with agg ring struct before the offload
	 * flags are updated.
	 */
	if (!bnxt_need_agg_ring(bp->eth_dev) ||
	    (rxr && rxr->ag_ring_struct == NULL))
		goto no_agg;

	ring = rxr ? rxr->ag_ring_struct : NULL;
	if (ring != NULL && cpr != NULL) {
		bnxt_hwrm_ring_free(bp, ring,
				    BNXT_CHIP_P5(bp) ?
				    HWRM_RING_FREE_INPUT_RING_TYPE_RX_AGG :
				    HWRM_RING_FREE_INPUT_RING_TYPE_RX,
				    cpr->cp_ring_struct->fw_ring_id);
	}
	if (BNXT_HAS_RING_GRPS(bp))
		bp->grp_info[queue_index].ag_fw_ring_id = INVALID_HW_RING_ID;

no_agg:
	if (cpr != NULL) {
		bnxt_hwrm_stat_ctx_free(bp, cpr);
		bnxt_free_cp_ring(bp, cpr);
	}

	if (BNXT_HAS_RING_GRPS(bp))
		bp->grp_info[queue_index].cp_fw_ring_id = INVALID_HW_RING_ID;
}

int bnxt_hwrm_rx_ring_reset(struct bnxt *bp, int queue_index)
{
	int rc;
	struct hwrm_ring_reset_input req = {.req_type = 0 };
	struct hwrm_ring_reset_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_RING_RESET, BNXT_USE_CHIMP_MB);

	req.ring_type = HWRM_RING_RESET_INPUT_RING_TYPE_RX_RING_GRP;
	req.ring_id = rte_cpu_to_le_16(bp->grp_info[queue_index].fw_grp_id);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	HWRM_UNLOCK();

	return rc;
}

static int
bnxt_free_all_hwrm_rings(struct bnxt *bp)
{
	unsigned int i;

	for (i = 0; i < bp->tx_cp_nr_rings; i++)
		bnxt_free_hwrm_tx_ring(bp, i);

	for (i = 0; i < bp->rx_cp_nr_rings; i++)
		bnxt_free_hwrm_rx_ring(bp, i);

	return 0;
}

int bnxt_alloc_all_hwrm_ring_grps(struct bnxt *bp)
{
	uint16_t i;
	uint32_t rc = 0;

	if (!BNXT_HAS_RING_GRPS(bp))
		return 0;

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		rc = bnxt_hwrm_ring_grp_alloc(bp, i);
		if (rc)
			return rc;
	}
	return rc;
}

/*
 * HWRM utility functions
 */

void bnxt_free_hwrm_resources(struct bnxt *bp)
{
	/* Release memzone */
	rte_free(bp->hwrm_cmd_resp_addr);
	rte_free(bp->hwrm_short_cmd_req_addr);
	bp->hwrm_cmd_resp_addr = NULL;
	bp->hwrm_short_cmd_req_addr = NULL;
	bp->hwrm_cmd_resp_dma_addr = 0;
	bp->hwrm_short_cmd_req_dma_addr = 0;
}

int bnxt_alloc_hwrm_resources(struct bnxt *bp)
{
	struct rte_pci_device *pdev = bp->pdev;
	char type[RTE_MEMZONE_NAMESIZE];

	sprintf(type, "bnxt_hwrm_" PCI_PRI_FMT, pdev->addr.domain,
		pdev->addr.bus, pdev->addr.devid, pdev->addr.function);
	bp->max_resp_len = BNXT_PAGE_SIZE;
	bp->hwrm_cmd_resp_addr = rte_malloc(type, bp->max_resp_len, 0);
	if (bp->hwrm_cmd_resp_addr == NULL)
		return -ENOMEM;
	bp->hwrm_cmd_resp_dma_addr =
		rte_malloc_virt2iova(bp->hwrm_cmd_resp_addr);
	if (bp->hwrm_cmd_resp_dma_addr == RTE_BAD_IOVA) {
		PMD_DRV_LOG(ERR,
			"unable to map response address to physical memory\n");
		return -ENOMEM;
	}
	rte_spinlock_init(&bp->hwrm_lock);

	return 0;
}

int
bnxt_clear_one_vnic_filter(struct bnxt *bp, struct bnxt_filter_info *filter)
{
	int rc = 0;

	if (filter->filter_type == HWRM_CFA_EM_FILTER) {
		rc = bnxt_hwrm_clear_em_filter(bp, filter);
		if (rc)
			return rc;
	} else if (filter->filter_type == HWRM_CFA_NTUPLE_FILTER) {
		rc = bnxt_hwrm_clear_ntuple_filter(bp, filter);
		if (rc)
			return rc;
	}

	rc = bnxt_hwrm_clear_l2_filter(bp, filter);
	return rc;
}

static int
bnxt_clear_hwrm_vnic_filters(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	struct bnxt_filter_info *filter;
	int rc = 0;

	STAILQ_FOREACH(filter, &vnic->filter, next) {
		rc = bnxt_clear_one_vnic_filter(bp, filter);
		STAILQ_REMOVE(&vnic->filter, filter, bnxt_filter_info, next);
		bnxt_free_filter(bp, filter);
	}
	return rc;
}

static int
bnxt_clear_hwrm_vnic_flows(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	struct bnxt_filter_info *filter;
	struct rte_flow *flow;
	int rc = 0;

	while (!STAILQ_EMPTY(&vnic->flow_list)) {
		flow = STAILQ_FIRST(&vnic->flow_list);
		filter = flow->filter;
		PMD_DRV_LOG(DEBUG, "filter type %d\n", filter->filter_type);
		rc = bnxt_clear_one_vnic_filter(bp, filter);

		STAILQ_REMOVE(&vnic->flow_list, flow, rte_flow, next);
		rte_free(flow);
	}
	return rc;
}

int bnxt_set_hwrm_vnic_filters(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	struct bnxt_filter_info *filter;
	int rc = 0;

	STAILQ_FOREACH(filter, &vnic->filter, next) {
		if (filter->filter_type == HWRM_CFA_EM_FILTER)
			rc = bnxt_hwrm_set_em_filter(bp, filter->dst_id,
						     filter);
		else if (filter->filter_type == HWRM_CFA_NTUPLE_FILTER)
			rc = bnxt_hwrm_set_ntuple_filter(bp, filter->dst_id,
							 filter);
		else
			rc = bnxt_hwrm_set_l2_filter(bp, vnic->fw_vnic_id,
						     filter);
		if (rc)
			break;
	}
	return rc;
}

static void
bnxt_free_tunnel_ports(struct bnxt *bp)
{
	if (bp->vxlan_port_cnt)
		bnxt_hwrm_tunnel_dst_port_free(bp, bp->vxlan_fw_dst_port_id,
			HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_VXLAN);

	if (bp->geneve_port_cnt)
		bnxt_hwrm_tunnel_dst_port_free(bp, bp->geneve_fw_dst_port_id,
			HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_GENEVE);
}

void bnxt_free_all_hwrm_resources(struct bnxt *bp)
{
	int i;

	if (bp->vnic_info == NULL)
		return;

	/*
	 * Cleanup VNICs in reverse order, to make sure the L2 filter
	 * from vnic0 is last to be cleaned up.
	 */
	for (i = bp->max_vnics - 1; i >= 0; i--) {
		struct bnxt_vnic_info *vnic = &bp->vnic_info[i];

		if (vnic->fw_vnic_id == INVALID_HW_RING_ID)
			continue;

		bnxt_clear_hwrm_vnic_flows(bp, vnic);

		bnxt_clear_hwrm_vnic_filters(bp, vnic);

		bnxt_hwrm_vnic_ctx_free(bp, vnic);

		bnxt_hwrm_vnic_tpa_cfg(bp, vnic, false);

		bnxt_hwrm_vnic_free(bp, vnic);

		rte_free(vnic->fw_grp_ids);
	}
	/* Ring resources */
	bnxt_free_all_hwrm_rings(bp);
	bnxt_free_all_hwrm_ring_grps(bp);
	bnxt_free_all_hwrm_stat_ctxs(bp);
	bnxt_free_tunnel_ports(bp);
}

static uint16_t bnxt_parse_eth_link_duplex(uint32_t conf_link_speed)
{
	uint8_t hw_link_duplex = HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_BOTH;

	if ((conf_link_speed & RTE_ETH_LINK_SPEED_FIXED) == RTE_ETH_LINK_SPEED_AUTONEG)
		return HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_BOTH;

	switch (conf_link_speed) {
	case RTE_ETH_LINK_SPEED_10M_HD:
	case RTE_ETH_LINK_SPEED_100M_HD:
		/* FALLTHROUGH */
		return HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_HALF;
	}
	return hw_link_duplex;
}

static uint16_t bnxt_check_eth_link_autoneg(uint32_t conf_link)
{
	return !conf_link;
}

static uint16_t bnxt_parse_eth_link_speed(uint32_t conf_link_speed,
					  uint16_t pam4_link)
{
	uint16_t eth_link_speed = 0;

	if (conf_link_speed == RTE_ETH_LINK_SPEED_AUTONEG)
		return RTE_ETH_LINK_SPEED_AUTONEG;

	switch (conf_link_speed & ~RTE_ETH_LINK_SPEED_FIXED) {
	case RTE_ETH_LINK_SPEED_100M:
	case RTE_ETH_LINK_SPEED_100M_HD:
		/* FALLTHROUGH */
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_100MB;
		break;
	case RTE_ETH_LINK_SPEED_1G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_1GB;
		break;
	case RTE_ETH_LINK_SPEED_2_5G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_2_5GB;
		break;
	case RTE_ETH_LINK_SPEED_10G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_10GB;
		break;
	case RTE_ETH_LINK_SPEED_20G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_20GB;
		break;
	case RTE_ETH_LINK_SPEED_25G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_25GB;
		break;
	case RTE_ETH_LINK_SPEED_40G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_40GB;
		break;
	case RTE_ETH_LINK_SPEED_50G:
		eth_link_speed = pam4_link ?
			HWRM_PORT_PHY_CFG_INPUT_FORCE_PAM4_LINK_SPEED_50GB :
			HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_50GB;
		break;
	case RTE_ETH_LINK_SPEED_100G:
		eth_link_speed = pam4_link ?
			HWRM_PORT_PHY_CFG_INPUT_FORCE_PAM4_LINK_SPEED_100GB :
			HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_100GB;
		break;
	case RTE_ETH_LINK_SPEED_200G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_FORCE_PAM4_LINK_SPEED_200GB;
		break;
	default:
		PMD_DRV_LOG(ERR,
			"Unsupported link speed %d; default to AUTO\n",
			conf_link_speed);
		break;
	}
	return eth_link_speed;
}

#define BNXT_SUPPORTED_SPEEDS (RTE_ETH_LINK_SPEED_100M | RTE_ETH_LINK_SPEED_100M_HD | \
		RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_2_5G | \
		RTE_ETH_LINK_SPEED_10G | RTE_ETH_LINK_SPEED_20G | RTE_ETH_LINK_SPEED_25G | \
		RTE_ETH_LINK_SPEED_40G | RTE_ETH_LINK_SPEED_50G | \
		RTE_ETH_LINK_SPEED_100G | RTE_ETH_LINK_SPEED_200G)

static int bnxt_validate_link_speed(struct bnxt *bp)
{
	uint32_t link_speed = bp->eth_dev->data->dev_conf.link_speeds;
	uint16_t port_id = bp->eth_dev->data->port_id;
	uint32_t link_speed_capa;
	uint32_t one_speed;

	if (link_speed == RTE_ETH_LINK_SPEED_AUTONEG)
		return 0;

	link_speed_capa = bnxt_get_speed_capabilities(bp);

	if (link_speed & RTE_ETH_LINK_SPEED_FIXED) {
		one_speed = link_speed & ~RTE_ETH_LINK_SPEED_FIXED;

		if (one_speed & (one_speed - 1)) {
			PMD_DRV_LOG(ERR,
				"Invalid advertised speeds (%u) for port %u\n",
				link_speed, port_id);
			return -EINVAL;
		}
		if ((one_speed & link_speed_capa) != one_speed) {
			PMD_DRV_LOG(ERR,
				"Unsupported advertised speed (%u) for port %u\n",
				link_speed, port_id);
			return -EINVAL;
		}
	} else {
		if (!(link_speed & link_speed_capa)) {
			PMD_DRV_LOG(ERR,
				"Unsupported advertised speeds (%u) for port %u\n",
				link_speed, port_id);
			return -EINVAL;
		}
	}
	return 0;
}

static uint16_t
bnxt_parse_eth_link_speed_mask(struct bnxt *bp, uint32_t link_speed)
{
	uint16_t ret = 0;

	if (link_speed == RTE_ETH_LINK_SPEED_AUTONEG) {
		if (bp->link_info->support_speeds)
			return bp->link_info->support_speeds;
		link_speed = BNXT_SUPPORTED_SPEEDS;
	}

	if (link_speed & RTE_ETH_LINK_SPEED_100M)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100MB;
	if (link_speed & RTE_ETH_LINK_SPEED_100M_HD)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100MB;
	if (link_speed & RTE_ETH_LINK_SPEED_1G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_1GB;
	if (link_speed & RTE_ETH_LINK_SPEED_2_5G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_2_5GB;
	if (link_speed & RTE_ETH_LINK_SPEED_10G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10GB;
	if (link_speed & RTE_ETH_LINK_SPEED_20G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_20GB;
	if (link_speed & RTE_ETH_LINK_SPEED_25G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_25GB;
	if (link_speed & RTE_ETH_LINK_SPEED_40G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_40GB;
	if (link_speed & RTE_ETH_LINK_SPEED_50G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_50GB;
	if (link_speed & RTE_ETH_LINK_SPEED_100G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100GB;
	if (link_speed & RTE_ETH_LINK_SPEED_200G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_FORCE_PAM4_LINK_SPEED_200GB;
	return ret;
}

static uint32_t bnxt_parse_hw_link_speed(uint16_t hw_link_speed)
{
	uint32_t eth_link_speed = RTE_ETH_SPEED_NUM_NONE;

	switch (hw_link_speed) {
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100MB:
		eth_link_speed = RTE_ETH_SPEED_NUM_100M;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_1GB:
		eth_link_speed = RTE_ETH_SPEED_NUM_1G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2_5GB:
		eth_link_speed = RTE_ETH_SPEED_NUM_2_5G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_10GB:
		eth_link_speed = RTE_ETH_SPEED_NUM_10G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_20GB:
		eth_link_speed = RTE_ETH_SPEED_NUM_20G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_25GB:
		eth_link_speed = RTE_ETH_SPEED_NUM_25G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_40GB:
		eth_link_speed = RTE_ETH_SPEED_NUM_40G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_50GB:
		eth_link_speed = RTE_ETH_SPEED_NUM_50G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100GB:
		eth_link_speed = RTE_ETH_SPEED_NUM_100G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_200GB:
		eth_link_speed = RTE_ETH_SPEED_NUM_200G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2GB:
	default:
		PMD_DRV_LOG(ERR, "HWRM link speed %d not defined\n",
			hw_link_speed);
		break;
	}
	return eth_link_speed;
}

static uint16_t bnxt_parse_hw_link_duplex(uint16_t hw_link_duplex)
{
	uint16_t eth_link_duplex = RTE_ETH_LINK_FULL_DUPLEX;

	switch (hw_link_duplex) {
	case HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_BOTH:
	case HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_FULL:
		/* FALLTHROUGH */
		eth_link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		break;
	case HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_HALF:
		eth_link_duplex = RTE_ETH_LINK_HALF_DUPLEX;
		break;
	default:
		PMD_DRV_LOG(ERR, "HWRM link duplex %d not defined\n",
			hw_link_duplex);
		break;
	}
	return eth_link_duplex;
}

int bnxt_get_hwrm_link_config(struct bnxt *bp, struct rte_eth_link *link)
{
	int rc = 0;
	struct bnxt_link_info *link_info = bp->link_info;

	rc = bnxt_hwrm_port_phy_qcaps(bp);
	if (rc)
		PMD_DRV_LOG(ERR, "Get link config failed with rc %d\n", rc);

	rc = bnxt_hwrm_port_phy_qcfg(bp, link_info);
	if (rc) {
		PMD_DRV_LOG(ERR, "Get link config failed with rc %d\n", rc);
		goto exit;
	}

	if (link_info->link_speed)
		link->link_speed =
			bnxt_parse_hw_link_speed(link_info->link_speed);
	else
		link->link_speed = RTE_ETH_SPEED_NUM_NONE;
	link->link_duplex = bnxt_parse_hw_link_duplex(link_info->duplex);
	link->link_status = link_info->link_up;
	link->link_autoneg = link_info->auto_mode ==
		HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_NONE ?
		RTE_ETH_LINK_FIXED : RTE_ETH_LINK_AUTONEG;
exit:
	return rc;
}

int bnxt_set_hwrm_link_config(struct bnxt *bp, bool link_up)
{
	int rc = 0;
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	struct bnxt_link_info link_req;
	uint16_t speed, autoneg;

	if (!BNXT_SINGLE_PF(bp) || BNXT_VF(bp))
		return 0;

	rc = bnxt_validate_link_speed(bp);
	if (rc)
		goto error;

	memset(&link_req, 0, sizeof(link_req));
	link_req.link_up = link_up;
	if (!link_up)
		goto port_phy_cfg;

	autoneg = bnxt_check_eth_link_autoneg(dev_conf->link_speeds);
	if (BNXT_CHIP_P5(bp) &&
	    dev_conf->link_speeds == RTE_ETH_LINK_SPEED_40G) {
		/* 40G is not supported as part of media auto detect.
		 * The speed should be forced and autoneg disabled
		 * to configure 40G speed.
		 */
		PMD_DRV_LOG(INFO, "Disabling autoneg for 40G\n");
		autoneg = 0;
	}

	/* No auto speeds and no auto_pam4_link. Disable autoneg */
	if (bp->link_info->auto_link_speed == 0 &&
	    bp->link_info->link_signal_mode &&
	    bp->link_info->auto_pam4_link_speed_mask == 0)
		autoneg = 0;

	speed = bnxt_parse_eth_link_speed(dev_conf->link_speeds,
					  bp->link_info->link_signal_mode);
	link_req.phy_flags = HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESET_PHY;
	/* Autoneg can be done only when the FW allows. */
	if (autoneg == 1 &&
	    (bp->link_info->support_auto_speeds || bp->link_info->support_pam4_auto_speeds)) {
		link_req.phy_flags |=
				HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESTART_AUTONEG;
		link_req.auto_link_speed_mask =
			bnxt_parse_eth_link_speed_mask(bp,
						       dev_conf->link_speeds);
		link_req.auto_pam4_link_speed_mask =
			bp->link_info->auto_pam4_link_speed_mask;
	} else {
		if (bp->link_info->phy_type ==
		    HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASET ||
		    bp->link_info->phy_type ==
		    HWRM_PORT_PHY_QCFG_OUTPUT_PHY_TYPE_BASETE ||
		    bp->link_info->media_type ==
		    HWRM_PORT_PHY_QCFG_OUTPUT_MEDIA_TYPE_TP) {
			PMD_DRV_LOG(ERR, "10GBase-T devices must autoneg\n");
			return -EINVAL;
		}

		link_req.phy_flags |= HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE;
		/* If user wants a particular speed try that first. */
		if (speed)
			link_req.link_speed = speed;
		else if (bp->link_info->force_pam4_link_speed)
			link_req.link_speed =
				bp->link_info->force_pam4_link_speed;
		else if (bp->link_info->auto_pam4_link_speed_mask)
			link_req.link_speed =
				bp->link_info->auto_pam4_link_speed_mask;
		else if (bp->link_info->support_pam4_speeds)
			link_req.link_speed =
				bp->link_info->support_pam4_speeds;
		else if (bp->link_info->force_link_speed)
			link_req.link_speed = bp->link_info->force_link_speed;
		else
			link_req.link_speed = bp->link_info->auto_link_speed;
		/* Auto PAM4 link speed is zero, but auto_link_speed is not
		 * zero. Use the auto_link_speed.
		 */
		if (bp->link_info->auto_link_speed != 0 &&
		    bp->link_info->auto_pam4_link_speed_mask == 0)
			link_req.link_speed = bp->link_info->auto_link_speed;
	}
	link_req.duplex = bnxt_parse_eth_link_duplex(dev_conf->link_speeds);
	link_req.auto_pause = bp->link_info->auto_pause;
	link_req.force_pause = bp->link_info->force_pause;

port_phy_cfg:
	rc = bnxt_hwrm_port_phy_cfg(bp, &link_req);
	if (rc) {
		PMD_DRV_LOG(ERR,
			"Set link config failed with rc %d\n", rc);
	}

error:
	return rc;
}

int bnxt_hwrm_func_qcfg(struct bnxt *bp, uint16_t *mtu)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint16_t flags;
	int rc = 0;
	bp->func_svif = BNXT_SVIF_INVALID;
	uint16_t svif_info;

	HWRM_PREP(&req, HWRM_FUNC_QCFG, BNXT_USE_CHIMP_MB);
	req.fid = rte_cpu_to_le_16(0xffff);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	bp->vlan = rte_le_to_cpu_16(resp->vlan) & RTE_ETH_VLAN_ID_MAX;

	svif_info = rte_le_to_cpu_16(resp->svif_info);
	if (svif_info & HWRM_FUNC_QCFG_OUTPUT_SVIF_INFO_SVIF_VALID)
		bp->func_svif =	svif_info &
				     HWRM_FUNC_QCFG_OUTPUT_SVIF_INFO_SVIF_MASK;

	flags = rte_le_to_cpu_16(resp->flags);
	if (BNXT_PF(bp) && (flags & HWRM_FUNC_QCFG_OUTPUT_FLAGS_MULTI_HOST))
		bp->flags |= BNXT_FLAG_MULTI_HOST;

	if (BNXT_VF(bp) &&
	    !BNXT_VF_IS_TRUSTED(bp) &&
	    (flags & HWRM_FUNC_QCFG_OUTPUT_FLAGS_TRUSTED_VF)) {
		bp->flags |= BNXT_FLAG_TRUSTED_VF_EN;
		PMD_DRV_LOG(INFO, "Trusted VF cap enabled\n");
	} else if (BNXT_VF(bp) &&
		   BNXT_VF_IS_TRUSTED(bp) &&
		   !(flags & HWRM_FUNC_QCFG_OUTPUT_FLAGS_TRUSTED_VF)) {
		bp->flags &= ~BNXT_FLAG_TRUSTED_VF_EN;
		PMD_DRV_LOG(INFO, "Trusted VF cap disabled\n");
	}

	if (mtu)
		*mtu = rte_le_to_cpu_16(resp->admin_mtu);

	switch (resp->port_partition_type) {
	case HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR1_0:
	case HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR1_5:
	case HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR2_0:
		/* FALLTHROUGH */
		bp->flags |= BNXT_FLAG_NPAR_PF;
		break;
	default:
		bp->flags &= ~BNXT_FLAG_NPAR_PF;
		break;
	}

	bp->legacy_db_size =
		rte_le_to_cpu_16(resp->legacy_l2_db_size_kb) * 1024;

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_parent_pf_qcfg(struct bnxt *bp)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint16_t flags;
	int rc;

	if (!BNXT_VF_IS_TRUSTED(bp))
		return 0;

	if (!bp->parent)
		return -EINVAL;

	bp->parent->fid = BNXT_PF_FID_INVALID;

	HWRM_PREP(&req, HWRM_FUNC_QCFG, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(0xfffe); /* Request parent PF information. */

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT_SILENT();

	memcpy(bp->parent->mac_addr, resp->mac_address, RTE_ETHER_ADDR_LEN);
	bp->parent->vnic = rte_le_to_cpu_16(resp->dflt_vnic_id);
	bp->parent->fid = rte_le_to_cpu_16(resp->fid);
	bp->parent->port_id = rte_le_to_cpu_16(resp->port_id);

	flags = rte_le_to_cpu_16(resp->flags);
	/* check for the multi-root support */
	if (flags & HWRM_FUNC_QCFG_OUTPUT_FLAGS_MULTI_ROOT) {
		bp->flags2 |= BNXT_FLAGS2_MULTIROOT_EN;
		PMD_DRV_LOG(DEBUG, "PF enabled with multi root capability\n");
	}

	HWRM_UNLOCK();

	return 0;
}

int bnxt_hwrm_get_dflt_vnic_svif(struct bnxt *bp, uint16_t fid,
				 uint16_t *vnic_id, uint16_t *svif)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint16_t svif_info;
	int rc = 0;

	HWRM_PREP(&req, HWRM_FUNC_QCFG, BNXT_USE_CHIMP_MB);
	req.fid = rte_cpu_to_le_16(fid);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	if (vnic_id)
		*vnic_id = rte_le_to_cpu_16(resp->dflt_vnic_id);

	svif_info = rte_le_to_cpu_16(resp->svif_info);
	if (svif && (svif_info & HWRM_FUNC_QCFG_OUTPUT_SVIF_INFO_SVIF_VALID))
		*svif = svif_info & HWRM_FUNC_QCFG_OUTPUT_SVIF_INFO_SVIF_MASK;

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_port_mac_qcfg(struct bnxt *bp)
{
	struct hwrm_port_mac_qcfg_input req = {0};
	struct hwrm_port_mac_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint16_t port_svif_info;
	int rc;

	bp->port_svif = BNXT_SVIF_INVALID;

	if (BNXT_VF(bp) && !BNXT_VF_IS_TRUSTED(bp))
		return 0;

	HWRM_PREP(&req, HWRM_PORT_MAC_QCFG, BNXT_USE_CHIMP_MB);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT_SILENT();

	port_svif_info = rte_le_to_cpu_16(resp->port_svif_info);
	if (port_svif_info &
	    HWRM_PORT_MAC_QCFG_OUTPUT_PORT_SVIF_INFO_PORT_SVIF_VALID)
		bp->port_svif = port_svif_info &
			HWRM_PORT_MAC_QCFG_OUTPUT_PORT_SVIF_INFO_PORT_SVIF_MASK;

	HWRM_UNLOCK();

	return 0;
}

static int bnxt_hwrm_pf_func_cfg(struct bnxt *bp,
				 struct bnxt_pf_resource_info *pf_resc)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t enables;
	int rc;

	enables = HWRM_FUNC_CFG_INPUT_ENABLES_ADMIN_MTU |
		  HWRM_FUNC_CFG_INPUT_ENABLES_HOST_MTU |
		  HWRM_FUNC_CFG_INPUT_ENABLES_MRU |
		  HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RSSCOS_CTXS |
		  HWRM_FUNC_CFG_INPUT_ENABLES_NUM_STAT_CTXS |
		  HWRM_FUNC_CFG_INPUT_ENABLES_NUM_CMPL_RINGS |
		  HWRM_FUNC_CFG_INPUT_ENABLES_NUM_TX_RINGS |
		  HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RX_RINGS |
		  HWRM_FUNC_CFG_INPUT_ENABLES_NUM_L2_CTXS |
		  HWRM_FUNC_CFG_INPUT_ENABLES_NUM_VNICS;

	if (BNXT_HAS_RING_GRPS(bp)) {
		enables |= HWRM_FUNC_CFG_INPUT_ENABLES_NUM_HW_RING_GRPS;
		req.num_hw_ring_grps =
			rte_cpu_to_le_16(pf_resc->num_hw_ring_grps);
	} else if (BNXT_HAS_NQ(bp)) {
		enables |= HWRM_FUNC_CFG_INPUT_ENABLES_NUM_MSIX;
		req.num_msix = rte_cpu_to_le_16(pf_resc->num_nq_rings);
	}

	req.flags = rte_cpu_to_le_32(bp->pf->func_cfg_flags);
	req.admin_mtu = rte_cpu_to_le_16(BNXT_MAX_MTU);
	req.host_mtu = rte_cpu_to_le_16(bp->eth_dev->data->mtu);
	req.mru = rte_cpu_to_le_16(BNXT_VNIC_MRU(bp->eth_dev->data->mtu));
	req.num_rsscos_ctxs = rte_cpu_to_le_16(pf_resc->num_rsscos_ctxs);
	req.num_stat_ctxs = rte_cpu_to_le_16(pf_resc->num_stat_ctxs);
	req.num_cmpl_rings = rte_cpu_to_le_16(pf_resc->num_cp_rings);
	req.num_tx_rings = rte_cpu_to_le_16(pf_resc->num_tx_rings);
	req.num_rx_rings = rte_cpu_to_le_16(pf_resc->num_rx_rings);
	req.num_l2_ctxs = rte_cpu_to_le_16(pf_resc->num_l2_ctxs);
	req.num_vnics = rte_cpu_to_le_16(pf_resc->num_vnics);
	req.fid = rte_cpu_to_le_16(0xffff);
	req.enables = rte_cpu_to_le_32(enables);

	HWRM_PREP(&req, HWRM_FUNC_CFG, BNXT_USE_CHIMP_MB);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

/* min values are the guaranteed resources and max values are subject
 * to availability. The strategy for now is to keep both min & max
 * values the same.
 */
static void
bnxt_fill_vf_func_cfg_req_new(struct bnxt *bp,
			      struct hwrm_func_vf_resource_cfg_input *req,
			      int num_vfs)
{
	req->max_rsscos_ctx = rte_cpu_to_le_16(bp->max_rsscos_ctx /
					       (num_vfs + 1));
	req->min_rsscos_ctx = req->max_rsscos_ctx;
	req->max_stat_ctx = rte_cpu_to_le_16(bp->max_stat_ctx / (num_vfs + 1));
	req->min_stat_ctx = req->max_stat_ctx;
	req->max_cmpl_rings = rte_cpu_to_le_16(bp->max_cp_rings /
					       (num_vfs + 1));
	req->min_cmpl_rings = req->max_cmpl_rings;
	req->max_tx_rings = rte_cpu_to_le_16(bp->max_tx_rings / (num_vfs + 1));
	req->min_tx_rings = req->max_tx_rings;
	req->max_rx_rings = rte_cpu_to_le_16(bp->max_rx_rings / (num_vfs + 1));
	req->min_rx_rings = req->max_rx_rings;
	req->max_l2_ctxs = rte_cpu_to_le_16(bp->max_l2_ctx / (num_vfs + 1));
	req->min_l2_ctxs = req->max_l2_ctxs;
	req->max_vnics = rte_cpu_to_le_16(bp->max_vnics / (num_vfs + 1));
	req->min_vnics = req->max_vnics;
	req->max_hw_ring_grps = rte_cpu_to_le_16(bp->max_ring_grps /
						 (num_vfs + 1));
	req->min_hw_ring_grps = req->max_hw_ring_grps;
	req->max_msix = rte_cpu_to_le_16(bp->max_nq_rings / (num_vfs + 1));
}

static void
bnxt_fill_vf_func_cfg_req_old(struct bnxt *bp,
			      struct hwrm_func_cfg_input *req,
			      int num_vfs)
{
	req->enables = rte_cpu_to_le_32(HWRM_FUNC_CFG_INPUT_ENABLES_ADMIN_MTU |
			HWRM_FUNC_CFG_INPUT_ENABLES_MRU |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RSSCOS_CTXS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_STAT_CTXS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_CMPL_RINGS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_TX_RINGS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RX_RINGS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_L2_CTXS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_VNICS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_HW_RING_GRPS);

	req->admin_mtu = rte_cpu_to_le_16(bp->eth_dev->data->mtu + RTE_ETHER_HDR_LEN +
					  RTE_ETHER_CRC_LEN + RTE_VLAN_HLEN *
					  BNXT_NUM_VLANS);
	req->mru = rte_cpu_to_le_16(BNXT_VNIC_MRU(bp->eth_dev->data->mtu));
	req->num_rsscos_ctxs = rte_cpu_to_le_16(bp->max_rsscos_ctx /
						(num_vfs + 1));
	req->num_stat_ctxs = rte_cpu_to_le_16(bp->max_stat_ctx / (num_vfs + 1));
	req->num_cmpl_rings = rte_cpu_to_le_16(bp->max_cp_rings /
					       (num_vfs + 1));
	req->num_tx_rings = rte_cpu_to_le_16(bp->max_tx_rings / (num_vfs + 1));
	req->num_rx_rings = rte_cpu_to_le_16(bp->max_rx_rings / (num_vfs + 1));
	req->num_l2_ctxs = rte_cpu_to_le_16(bp->max_l2_ctx / (num_vfs + 1));
	/* TODO: For now, do not support VMDq/RFS on VFs. */
	req->num_vnics = rte_cpu_to_le_16(1);
	req->num_hw_ring_grps = rte_cpu_to_le_16(bp->max_ring_grps /
						 (num_vfs + 1));
}

/* Update the port wide resource values based on how many resources
 * got allocated to the VF.
 */
static int bnxt_update_max_resources(struct bnxt *bp,
				     int vf)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	/* Get the actual allocated values now */
	HWRM_PREP(&req, HWRM_FUNC_QCFG, BNXT_USE_CHIMP_MB);
	req.fid = rte_cpu_to_le_16(bp->pf->vf_info[vf].fid);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();

	bp->max_rsscos_ctx -= rte_le_to_cpu_16(resp->alloc_rsscos_ctx);
	bp->max_stat_ctx -= rte_le_to_cpu_16(resp->alloc_stat_ctx);
	bp->max_cp_rings -= rte_le_to_cpu_16(resp->alloc_cmpl_rings);
	bp->max_tx_rings -= rte_le_to_cpu_16(resp->alloc_tx_rings);
	bp->max_rx_rings -= rte_le_to_cpu_16(resp->alloc_rx_rings);
	bp->max_l2_ctx -= rte_le_to_cpu_16(resp->alloc_l2_ctx);
	bp->max_ring_grps -= rte_le_to_cpu_16(resp->alloc_hw_ring_grps);
	bp->max_nq_rings -= rte_le_to_cpu_16(resp->alloc_msix);
	bp->max_vnics -= rte_le_to_cpu_16(resp->alloc_vnics);

	HWRM_UNLOCK();

	return 0;
}

/* Update the PF resource values based on how many resources
 * got allocated to it.
 */
static int bnxt_update_max_resources_pf_only(struct bnxt *bp)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	/* Get the actual allocated values now */
	HWRM_PREP(&req, HWRM_FUNC_QCFG, BNXT_USE_CHIMP_MB);
	req.fid = rte_cpu_to_le_16(0xffff);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();

	bp->max_rsscos_ctx = rte_le_to_cpu_16(resp->alloc_rsscos_ctx);
	bp->max_stat_ctx = rte_le_to_cpu_16(resp->alloc_stat_ctx);
	bp->max_cp_rings = rte_le_to_cpu_16(resp->alloc_cmpl_rings);
	bp->max_tx_rings = rte_le_to_cpu_16(resp->alloc_tx_rings);
	bp->max_rx_rings = rte_le_to_cpu_16(resp->alloc_rx_rings);
	bp->max_l2_ctx = rte_le_to_cpu_16(resp->alloc_l2_ctx);
	bp->max_ring_grps = rte_le_to_cpu_16(resp->alloc_hw_ring_grps);
	bp->max_vnics = rte_le_to_cpu_16(resp->alloc_vnics);

	HWRM_UNLOCK();

	return 0;
}

int bnxt_hwrm_func_qcfg_current_vf_vlan(struct bnxt *bp, int vf)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	/* Check for zero MAC address */
	HWRM_PREP(&req, HWRM_FUNC_QCFG, BNXT_USE_CHIMP_MB);
	req.fid = rte_cpu_to_le_16(bp->pf->vf_info[vf].fid);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();
	rc = rte_le_to_cpu_16(resp->vlan);

	HWRM_UNLOCK();

	return rc;
}

static int bnxt_query_pf_resources(struct bnxt *bp,
				   struct bnxt_pf_resource_info *pf_resc)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	/* And copy the allocated numbers into the pf struct */
	HWRM_PREP(&req, HWRM_FUNC_QCFG, BNXT_USE_CHIMP_MB);
	req.fid = rte_cpu_to_le_16(0xffff);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();

	pf_resc->num_tx_rings = rte_le_to_cpu_16(resp->alloc_tx_rings);
	pf_resc->num_rsscos_ctxs = rte_le_to_cpu_16(resp->alloc_rsscos_ctx);
	pf_resc->num_stat_ctxs = rte_le_to_cpu_16(resp->alloc_stat_ctx);
	pf_resc->num_cp_rings = rte_le_to_cpu_16(resp->alloc_cmpl_rings);
	pf_resc->num_rx_rings = rte_le_to_cpu_16(resp->alloc_rx_rings);
	pf_resc->num_l2_ctxs = rte_le_to_cpu_16(resp->alloc_l2_ctx);
	pf_resc->num_hw_ring_grps = rte_le_to_cpu_32(resp->alloc_hw_ring_grps);
	pf_resc->num_nq_rings = rte_le_to_cpu_32(resp->alloc_msix);
	pf_resc->num_vnics = rte_le_to_cpu_16(resp->alloc_vnics);
	bp->pf->evb_mode = resp->evb_mode;

	HWRM_UNLOCK();

	return rc;
}

static void
bnxt_calculate_pf_resources(struct bnxt *bp,
			    struct bnxt_pf_resource_info *pf_resc,
			    int num_vfs)
{
	if (!num_vfs) {
		pf_resc->num_rsscos_ctxs = bp->max_rsscos_ctx;
		pf_resc->num_stat_ctxs = bp->max_stat_ctx;
		pf_resc->num_cp_rings = bp->max_cp_rings;
		pf_resc->num_tx_rings = bp->max_tx_rings;
		pf_resc->num_rx_rings = bp->max_rx_rings;
		pf_resc->num_l2_ctxs = bp->max_l2_ctx;
		pf_resc->num_hw_ring_grps = bp->max_ring_grps;
		pf_resc->num_nq_rings = bp->max_nq_rings;
		pf_resc->num_vnics = bp->max_vnics;

		return;
	}

	pf_resc->num_rsscos_ctxs = bp->max_rsscos_ctx / (num_vfs + 1) +
				   bp->max_rsscos_ctx % (num_vfs + 1);
	pf_resc->num_stat_ctxs = bp->max_stat_ctx / (num_vfs + 1) +
				 bp->max_stat_ctx % (num_vfs + 1);
	pf_resc->num_cp_rings = bp->max_cp_rings / (num_vfs + 1) +
				bp->max_cp_rings % (num_vfs + 1);
	pf_resc->num_tx_rings = bp->max_tx_rings / (num_vfs + 1) +
				bp->max_tx_rings % (num_vfs + 1);
	pf_resc->num_rx_rings = bp->max_rx_rings / (num_vfs + 1) +
				bp->max_rx_rings % (num_vfs + 1);
	pf_resc->num_l2_ctxs = bp->max_l2_ctx / (num_vfs + 1) +
			       bp->max_l2_ctx % (num_vfs + 1);
	pf_resc->num_hw_ring_grps = bp->max_ring_grps / (num_vfs + 1) +
				    bp->max_ring_grps % (num_vfs + 1);
	pf_resc->num_nq_rings = bp->max_nq_rings / (num_vfs + 1) +
				bp->max_nq_rings % (num_vfs + 1);
	pf_resc->num_vnics = bp->max_vnics / (num_vfs + 1) +
				bp->max_vnics % (num_vfs + 1);
}

int bnxt_hwrm_allocate_pf_only(struct bnxt *bp)
{
	struct bnxt_pf_resource_info pf_resc = { 0 };
	int rc;

	if (!BNXT_PF(bp)) {
		PMD_DRV_LOG(ERR, "Attempt to allocate VFs on a VF!\n");
		return -EINVAL;
	}

	rc = bnxt_hwrm_func_qcaps(bp);
	if (rc)
		return rc;

	bnxt_calculate_pf_resources(bp, &pf_resc, 0);

	bp->pf->func_cfg_flags &=
		~(HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_ENABLE |
		  HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_DISABLE);
	bp->pf->func_cfg_flags |=
		HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_DISABLE;

	rc = bnxt_hwrm_pf_func_cfg(bp, &pf_resc);
	if (rc)
		return rc;

	rc = bnxt_update_max_resources_pf_only(bp);

	return rc;
}

static int
bnxt_configure_vf_req_buf(struct bnxt *bp, int num_vfs)
{
	size_t req_buf_sz, sz;
	int i, rc;

	req_buf_sz = num_vfs * HWRM_MAX_REQ_LEN;
	bp->pf->vf_req_buf = rte_malloc("bnxt_vf_fwd", req_buf_sz,
		page_roundup(num_vfs * HWRM_MAX_REQ_LEN));
	if (bp->pf->vf_req_buf == NULL) {
		return -ENOMEM;
	}

	for (sz = 0; sz < req_buf_sz; sz += getpagesize())
		rte_mem_lock_page(((char *)bp->pf->vf_req_buf) + sz);

	for (i = 0; i < num_vfs; i++)
		bp->pf->vf_info[i].req_buf = ((char *)bp->pf->vf_req_buf) +
					     (i * HWRM_MAX_REQ_LEN);

	rc = bnxt_hwrm_func_buf_rgtr(bp, num_vfs);
	if (rc)
		rte_free(bp->pf->vf_req_buf);

	return rc;
}

static int
bnxt_process_vf_resc_config_new(struct bnxt *bp, int num_vfs)
{
	struct hwrm_func_vf_resource_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_vf_resource_cfg_input req = {0};
	int i, rc = 0;

	bnxt_fill_vf_func_cfg_req_new(bp, &req, num_vfs);
	bp->pf->active_vfs = 0;
	for (i = 0; i < num_vfs; i++) {
		HWRM_PREP(&req, HWRM_FUNC_VF_RESOURCE_CFG, BNXT_USE_CHIMP_MB);
		req.vf_id = rte_cpu_to_le_16(bp->pf->vf_info[i].fid);
		rc = bnxt_hwrm_send_message(bp,
					    &req,
					    sizeof(req),
					    BNXT_USE_CHIMP_MB);
		if (rc || resp->error_code) {
			PMD_DRV_LOG(ERR,
				"Failed to initialize VF %d\n", i);
			PMD_DRV_LOG(ERR,
				"Not all VFs available. (%d, %d)\n",
				rc, resp->error_code);
			HWRM_UNLOCK();

			/* If the first VF configuration itself fails,
			 * unregister the vf_fwd_request buffer.
			 */
			if (i == 0)
				bnxt_hwrm_func_buf_unrgtr(bp);
			break;
		}
		HWRM_UNLOCK();

		/* Update the max resource values based on the resource values
		 * allocated to the VF.
		 */
		bnxt_update_max_resources(bp, i);
		bp->pf->active_vfs++;
		bnxt_hwrm_func_clr_stats(bp, bp->pf->vf_info[i].fid);
	}

	return 0;
}

static int
bnxt_process_vf_resc_config_old(struct bnxt *bp, int num_vfs)
{
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_cfg_input req = {0};
	int i, rc;

	bnxt_fill_vf_func_cfg_req_old(bp, &req, num_vfs);

	bp->pf->active_vfs = 0;
	for (i = 0; i < num_vfs; i++) {
		HWRM_PREP(&req, HWRM_FUNC_CFG, BNXT_USE_CHIMP_MB);
		req.flags = rte_cpu_to_le_32(bp->pf->vf_info[i].func_cfg_flags);
		req.fid = rte_cpu_to_le_16(bp->pf->vf_info[i].fid);
		rc = bnxt_hwrm_send_message(bp,
					    &req,
					    sizeof(req),
					    BNXT_USE_CHIMP_MB);

		/* Clear enable flag for next pass */
		req.enables &= ~rte_cpu_to_le_32(
				HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_MAC_ADDR);

		if (rc || resp->error_code) {
			PMD_DRV_LOG(ERR,
				"Failed to initialize VF %d\n", i);
			PMD_DRV_LOG(ERR,
				"Not all VFs available. (%d, %d)\n",
				rc, resp->error_code);
			HWRM_UNLOCK();

			/* If the first VF configuration itself fails,
			 * unregister the vf_fwd_request buffer.
			 */
			if (i == 0)
				bnxt_hwrm_func_buf_unrgtr(bp);
			break;
		}

		HWRM_UNLOCK();

		/* Update the max resource values based on the resource values
		 * allocated to the VF.
		 */
		bnxt_update_max_resources(bp, i);
		bp->pf->active_vfs++;
		bnxt_hwrm_func_clr_stats(bp, bp->pf->vf_info[i].fid);
	}

	return 0;
}

static void
bnxt_configure_vf_resources(struct bnxt *bp, int num_vfs)
{
	if (bp->flags & BNXT_FLAG_NEW_RM)
		bnxt_process_vf_resc_config_new(bp, num_vfs);
	else
		bnxt_process_vf_resc_config_old(bp, num_vfs);
}

static void
bnxt_update_pf_resources(struct bnxt *bp,
			 struct bnxt_pf_resource_info *pf_resc)
{
	bp->max_rsscos_ctx = pf_resc->num_rsscos_ctxs;
	bp->max_stat_ctx = pf_resc->num_stat_ctxs;
	bp->max_cp_rings = pf_resc->num_cp_rings;
	bp->max_tx_rings = pf_resc->num_tx_rings;
	bp->max_rx_rings = pf_resc->num_rx_rings;
	bp->max_ring_grps = pf_resc->num_hw_ring_grps;
	bp->max_nq_rings = pf_resc->num_nq_rings;
	bp->max_vnics = pf_resc->num_vnics;
}

static int32_t
bnxt_configure_pf_resources(struct bnxt *bp,
			    struct bnxt_pf_resource_info *pf_resc)
{
	/*
	 * We're using STD_TX_RING_MODE here which will limit the TX
	 * rings. This will allow QoS to function properly. Not setting this
	 * will cause PF rings to break bandwidth settings.
	 */
	bp->pf->func_cfg_flags &=
		~(HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_ENABLE |
		  HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_DISABLE);
	bp->pf->func_cfg_flags |=
		HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_ENABLE;
	return bnxt_hwrm_pf_func_cfg(bp, pf_resc);
}

int bnxt_hwrm_allocate_vfs(struct bnxt *bp, int num_vfs)
{
	struct bnxt_pf_resource_info pf_resc = { 0 };
	int rc;

	if (!BNXT_PF(bp)) {
		PMD_DRV_LOG(ERR, "Attempt to allocate VFs on a VF!\n");
		return -EINVAL;
	}

	rc = bnxt_hwrm_func_qcaps(bp);
	if (rc)
		return rc;

	bnxt_calculate_pf_resources(bp, &pf_resc, num_vfs);

	rc = bnxt_configure_pf_resources(bp, &pf_resc);
	if (rc)
		return rc;

	rc = bnxt_query_pf_resources(bp, &pf_resc);
	if (rc)
		return rc;

	/*
	 * Now, create and register a buffer to hold forwarded VF requests
	 */
	rc = bnxt_configure_vf_req_buf(bp, num_vfs);
	if (rc)
		return rc;

	bnxt_configure_vf_resources(bp, num_vfs);

	bnxt_update_pf_resources(bp, &pf_resc);

	return 0;
}

int bnxt_hwrm_pf_evb_mode(struct bnxt *bp)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	HWRM_PREP(&req, HWRM_FUNC_CFG, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(0xffff);
	req.enables = rte_cpu_to_le_32(HWRM_FUNC_CFG_INPUT_ENABLES_EVB_MODE);
	req.evb_mode = bp->pf->evb_mode;

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_tunnel_dst_port_alloc(struct bnxt *bp, uint16_t port,
				uint8_t tunnel_type)
{
	struct hwrm_tunnel_dst_port_alloc_input req = {0};
	struct hwrm_tunnel_dst_port_alloc_output *resp = bp->hwrm_cmd_resp_addr;
	int rc = 0;

	HWRM_PREP(&req, HWRM_TUNNEL_DST_PORT_ALLOC, BNXT_USE_CHIMP_MB);
	req.tunnel_type = tunnel_type;
	req.tunnel_dst_port_val = rte_cpu_to_be_16(port);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();

	switch (tunnel_type) {
	case HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_VXLAN:
		bp->vxlan_fw_dst_port_id =
			rte_le_to_cpu_16(resp->tunnel_dst_port_id);
		bp->vxlan_port = port;
		break;
	case HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_GENEVE:
		bp->geneve_fw_dst_port_id =
			rte_le_to_cpu_16(resp->tunnel_dst_port_id);
		bp->geneve_port = port;
		break;
	default:
		break;
	}

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_tunnel_dst_port_free(struct bnxt *bp, uint16_t port,
				uint8_t tunnel_type)
{
	struct hwrm_tunnel_dst_port_free_input req = {0};
	struct hwrm_tunnel_dst_port_free_output *resp = bp->hwrm_cmd_resp_addr;
	int rc = 0;

	HWRM_PREP(&req, HWRM_TUNNEL_DST_PORT_FREE, BNXT_USE_CHIMP_MB);

	req.tunnel_type = tunnel_type;
	req.tunnel_dst_port_id = rte_cpu_to_be_16(port);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	if (tunnel_type ==
	    HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_VXLAN) {
		bp->vxlan_port = 0;
		bp->vxlan_port_cnt = 0;
	}

	if (tunnel_type ==
	    HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_GENEVE) {
		bp->geneve_port = 0;
		bp->geneve_port_cnt = 0;
	}

	return rc;
}

int bnxt_hwrm_func_cfg_vf_set_flags(struct bnxt *bp, uint16_t vf,
					uint32_t flags)
{
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_cfg_input req = {0};
	int rc;

	HWRM_PREP(&req, HWRM_FUNC_CFG, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(bp->pf->vf_info[vf].fid);
	req.flags = rte_cpu_to_le_32(flags);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

void vf_vnic_set_rxmask_cb(struct bnxt_vnic_info *vnic, void *flagp)
{
	uint32_t *flag = flagp;

	vnic->flags = *flag;
}

int bnxt_set_rx_mask_no_vlan(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	return bnxt_hwrm_cfa_l2_set_rx_mask(bp, vnic, 0, NULL);
}

int bnxt_hwrm_func_buf_rgtr(struct bnxt *bp, int num_vfs)
{
	struct hwrm_func_buf_rgtr_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_buf_rgtr_input req = {.req_type = 0 };
	int rc;

	HWRM_PREP(&req, HWRM_FUNC_BUF_RGTR, BNXT_USE_CHIMP_MB);

	req.req_buf_num_pages = rte_cpu_to_le_16(1);
	req.req_buf_page_size =
		rte_cpu_to_le_16(page_getenum(num_vfs * HWRM_MAX_REQ_LEN));
	req.req_buf_len = rte_cpu_to_le_16(HWRM_MAX_REQ_LEN);
	req.req_buf_page_addr0 =
		rte_cpu_to_le_64(rte_malloc_virt2iova(bp->pf->vf_req_buf));
	if (req.req_buf_page_addr0 == RTE_BAD_IOVA) {
		PMD_DRV_LOG(ERR,
			"unable to map buffer address to physical memory\n");
		HWRM_UNLOCK();
		return -ENOMEM;
	}

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_func_buf_unrgtr(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_func_buf_unrgtr_input req = {.req_type = 0 };
	struct hwrm_func_buf_unrgtr_output *resp = bp->hwrm_cmd_resp_addr;

	if (!(BNXT_PF(bp) && bp->pdev->max_vfs))
		return 0;

	HWRM_PREP(&req, HWRM_FUNC_BUF_UNRGTR, BNXT_USE_CHIMP_MB);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_func_cfg_def_cp(struct bnxt *bp)
{
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_cfg_input req = {0};
	int rc;

	HWRM_PREP(&req, HWRM_FUNC_CFG, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(0xffff);
	req.flags = rte_cpu_to_le_32(bp->pf->func_cfg_flags);
	req.enables = rte_cpu_to_le_32(
			HWRM_FUNC_CFG_INPUT_ENABLES_ASYNC_EVENT_CR);
	req.async_event_cr = rte_cpu_to_le_16(
			bp->async_cp_ring->cp_ring_struct->fw_ring_id);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_vf_func_cfg_def_cp(struct bnxt *bp)
{
	struct hwrm_func_vf_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_vf_cfg_input req = {0};
	int rc;

	HWRM_PREP(&req, HWRM_FUNC_VF_CFG, BNXT_USE_CHIMP_MB);

	req.enables = rte_cpu_to_le_32(
			HWRM_FUNC_VF_CFG_INPUT_ENABLES_ASYNC_EVENT_CR);
	req.async_event_cr = rte_cpu_to_le_16(
			bp->async_cp_ring->cp_ring_struct->fw_ring_id);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_set_default_vlan(struct bnxt *bp, int vf, uint8_t is_vf)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint16_t dflt_vlan, fid;
	uint32_t func_cfg_flags;
	int rc = 0;

	HWRM_PREP(&req, HWRM_FUNC_CFG, BNXT_USE_CHIMP_MB);

	if (is_vf) {
		dflt_vlan = bp->pf->vf_info[vf].dflt_vlan;
		fid = bp->pf->vf_info[vf].fid;
		func_cfg_flags = bp->pf->vf_info[vf].func_cfg_flags;
	} else {
		fid = rte_cpu_to_le_16(0xffff);
		func_cfg_flags = bp->pf->func_cfg_flags;
		dflt_vlan = bp->vlan;
	}

	req.flags = rte_cpu_to_le_32(func_cfg_flags);
	req.fid = rte_cpu_to_le_16(fid);
	req.enables |= rte_cpu_to_le_32(HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_VLAN);
	req.dflt_vlan = rte_cpu_to_le_16(dflt_vlan);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_func_bw_cfg(struct bnxt *bp, uint16_t vf,
			uint16_t max_bw, uint16_t enables)
{
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_cfg_input req = {0};
	int rc;

	HWRM_PREP(&req, HWRM_FUNC_CFG, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(bp->pf->vf_info[vf].fid);
	req.enables |= rte_cpu_to_le_32(enables);
	req.flags = rte_cpu_to_le_32(bp->pf->vf_info[vf].func_cfg_flags);
	req.max_bw = rte_cpu_to_le_32(max_bw);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_set_vf_vlan(struct bnxt *bp, int vf)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc = 0;

	HWRM_PREP(&req, HWRM_FUNC_CFG, BNXT_USE_CHIMP_MB);

	req.flags = rte_cpu_to_le_32(bp->pf->vf_info[vf].func_cfg_flags);
	req.fid = rte_cpu_to_le_16(bp->pf->vf_info[vf].fid);
	req.enables |= rte_cpu_to_le_32(HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_VLAN);
	req.dflt_vlan = rte_cpu_to_le_16(bp->pf->vf_info[vf].dflt_vlan);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_set_async_event_cr(struct bnxt *bp)
{
	int rc;

	if (BNXT_PF(bp))
		rc = bnxt_hwrm_func_cfg_def_cp(bp);
	else
		rc = bnxt_hwrm_vf_func_cfg_def_cp(bp);

	return rc;
}

int bnxt_hwrm_reject_fwd_resp(struct bnxt *bp, uint16_t target_id,
			      void *encaped, size_t ec_size)
{
	int rc = 0;
	struct hwrm_reject_fwd_resp_input req = {.req_type = 0};
	struct hwrm_reject_fwd_resp_output *resp = bp->hwrm_cmd_resp_addr;

	if (ec_size > sizeof(req.encap_request))
		return -1;

	HWRM_PREP(&req, HWRM_REJECT_FWD_RESP, BNXT_USE_CHIMP_MB);

	req.encap_resp_target_id = rte_cpu_to_le_16(target_id);
	memcpy(req.encap_request, encaped, ec_size);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_func_qcfg_vf_default_mac(struct bnxt *bp, uint16_t vf,
				       struct rte_ether_addr *mac)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	HWRM_PREP(&req, HWRM_FUNC_QCFG, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(bp->pf->vf_info[vf].fid);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	memcpy(mac->addr_bytes, resp->mac_address, RTE_ETHER_ADDR_LEN);

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_exec_fwd_resp(struct bnxt *bp, uint16_t target_id,
			    void *encaped, size_t ec_size)
{
	int rc = 0;
	struct hwrm_exec_fwd_resp_input req = {.req_type = 0};
	struct hwrm_exec_fwd_resp_output *resp = bp->hwrm_cmd_resp_addr;

	if (ec_size > sizeof(req.encap_request))
		return -1;

	HWRM_PREP(&req, HWRM_EXEC_FWD_RESP, BNXT_USE_CHIMP_MB);

	req.encap_resp_target_id = rte_cpu_to_le_16(target_id);
	memcpy(req.encap_request, encaped, ec_size);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

static void bnxt_update_prev_stat(uint64_t *cntr, uint64_t *prev_cntr)
{
	/* One of the HW stat values that make up this counter was zero as
	 * returned by HW in this iteration, so use the previous
	 * iteration's counter value
	 */
	if (*prev_cntr && *cntr == 0)
		*cntr = *prev_cntr;
	else
		*prev_cntr = *cntr;
}

int bnxt_hwrm_ring_stats(struct bnxt *bp, uint32_t cid, int idx,
			 struct bnxt_ring_stats *ring_stats, bool rx)
{
	int rc = 0;
	struct hwrm_stat_ctx_query_input req = {.req_type = 0};
	struct hwrm_stat_ctx_query_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_STAT_CTX_QUERY, BNXT_USE_CHIMP_MB);

	req.stat_ctx_id = rte_cpu_to_le_32(cid);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	if (rx) {
		struct bnxt_ring_stats *prev_stats = &bp->prev_rx_ring_stats[idx];

		ring_stats->rx_ucast_pkts = rte_le_to_cpu_64(resp->rx_ucast_pkts);
		bnxt_update_prev_stat(&ring_stats->rx_ucast_pkts,
				      &prev_stats->rx_ucast_pkts);

		ring_stats->rx_mcast_pkts = rte_le_to_cpu_64(resp->rx_mcast_pkts);
		bnxt_update_prev_stat(&ring_stats->rx_mcast_pkts,
				      &prev_stats->rx_mcast_pkts);

		ring_stats->rx_bcast_pkts = rte_le_to_cpu_64(resp->rx_bcast_pkts);
		bnxt_update_prev_stat(&ring_stats->rx_bcast_pkts,
				      &prev_stats->rx_bcast_pkts);

		ring_stats->rx_ucast_bytes = rte_le_to_cpu_64(resp->rx_ucast_bytes);
		bnxt_update_prev_stat(&ring_stats->rx_ucast_bytes,
				      &prev_stats->rx_ucast_bytes);

		ring_stats->rx_mcast_bytes = rte_le_to_cpu_64(resp->rx_mcast_bytes);
		bnxt_update_prev_stat(&ring_stats->rx_mcast_bytes,
				      &prev_stats->rx_mcast_bytes);

		ring_stats->rx_bcast_bytes = rte_le_to_cpu_64(resp->rx_bcast_bytes);
		bnxt_update_prev_stat(&ring_stats->rx_bcast_bytes,
				      &prev_stats->rx_bcast_bytes);

		ring_stats->rx_discard_pkts = rte_le_to_cpu_64(resp->rx_discard_pkts);
		bnxt_update_prev_stat(&ring_stats->rx_discard_pkts,
				      &prev_stats->rx_discard_pkts);

		ring_stats->rx_error_pkts = rte_le_to_cpu_64(resp->rx_error_pkts);
		bnxt_update_prev_stat(&ring_stats->rx_error_pkts,
				      &prev_stats->rx_error_pkts);

		ring_stats->rx_agg_pkts = rte_le_to_cpu_64(resp->rx_agg_pkts);
		bnxt_update_prev_stat(&ring_stats->rx_agg_pkts,
				      &prev_stats->rx_agg_pkts);

		ring_stats->rx_agg_bytes = rte_le_to_cpu_64(resp->rx_agg_bytes);
		bnxt_update_prev_stat(&ring_stats->rx_agg_bytes,
				      &prev_stats->rx_agg_bytes);

		ring_stats->rx_agg_events = rte_le_to_cpu_64(resp->rx_agg_events);
		bnxt_update_prev_stat(&ring_stats->rx_agg_events,
				      &prev_stats->rx_agg_events);

		ring_stats->rx_agg_aborts = rte_le_to_cpu_64(resp->rx_agg_aborts);
		bnxt_update_prev_stat(&ring_stats->rx_agg_aborts,
				      &prev_stats->rx_agg_aborts);
	} else {
		struct bnxt_ring_stats *prev_stats = &bp->prev_tx_ring_stats[idx];

		ring_stats->tx_ucast_pkts = rte_le_to_cpu_64(resp->tx_ucast_pkts);
		bnxt_update_prev_stat(&ring_stats->tx_ucast_pkts,
				      &prev_stats->tx_ucast_pkts);

		ring_stats->tx_mcast_pkts = rte_le_to_cpu_64(resp->tx_mcast_pkts);
		bnxt_update_prev_stat(&ring_stats->tx_mcast_pkts,
				      &prev_stats->tx_mcast_pkts);

		ring_stats->tx_bcast_pkts = rte_le_to_cpu_64(resp->tx_bcast_pkts);
		bnxt_update_prev_stat(&ring_stats->tx_bcast_pkts,
				      &prev_stats->tx_bcast_pkts);

		ring_stats->tx_ucast_bytes = rte_le_to_cpu_64(resp->tx_ucast_bytes);
		bnxt_update_prev_stat(&ring_stats->tx_ucast_bytes,
				      &prev_stats->tx_ucast_bytes);

		ring_stats->tx_mcast_bytes = rte_le_to_cpu_64(resp->tx_mcast_bytes);
		bnxt_update_prev_stat(&ring_stats->tx_mcast_bytes,
				      &prev_stats->tx_mcast_bytes);

		ring_stats->tx_bcast_bytes = rte_le_to_cpu_64(resp->tx_bcast_bytes);
		bnxt_update_prev_stat(&ring_stats->tx_bcast_bytes,
				      &prev_stats->tx_bcast_bytes);

		ring_stats->tx_discard_pkts = rte_le_to_cpu_64(resp->tx_discard_pkts);
		bnxt_update_prev_stat(&ring_stats->tx_discard_pkts,
				      &prev_stats->tx_discard_pkts);
	}

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_port_qstats(struct bnxt *bp)
{
	struct hwrm_port_qstats_input req = {0};
	struct hwrm_port_qstats_output *resp = bp->hwrm_cmd_resp_addr;
	struct bnxt_pf_info *pf = bp->pf;
	int rc;

	HWRM_PREP(&req, HWRM_PORT_QSTATS, BNXT_USE_CHIMP_MB);

	req.port_id = rte_cpu_to_le_16(pf->port_id);
	req.tx_stat_host_addr = rte_cpu_to_le_64(bp->hw_tx_port_stats_map);
	req.rx_stat_host_addr = rte_cpu_to_le_64(bp->hw_rx_port_stats_map);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_port_clr_stats(struct bnxt *bp)
{
	struct hwrm_port_clr_stats_input req = {0};
	struct hwrm_port_clr_stats_output *resp = bp->hwrm_cmd_resp_addr;
	struct bnxt_pf_info *pf = bp->pf;
	int rc;

	/* Not allowed on NS2 device, NPAR, MultiHost, VF */
	if (!(bp->flags & BNXT_FLAG_PORT_STATS) || BNXT_VF(bp) ||
	    BNXT_NPAR(bp) || BNXT_MH(bp) || BNXT_TOTAL_VFS(bp))
		return 0;

	HWRM_PREP(&req, HWRM_PORT_CLR_STATS, BNXT_USE_CHIMP_MB);

	req.port_id = rte_cpu_to_le_16(pf->port_id);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_port_led_qcaps(struct bnxt *bp)
{
	struct hwrm_port_led_qcaps_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_port_led_qcaps_input req = {0};
	int rc;

	if (BNXT_VF(bp))
		return 0;

	HWRM_PREP(&req, HWRM_PORT_LED_QCAPS, BNXT_USE_CHIMP_MB);
	req.port_id = bp->pf->port_id;
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT_SILENT();

	if (resp->num_leds > 0 && resp->num_leds < BNXT_MAX_LED) {
		unsigned int i;

		bp->leds->num_leds = resp->num_leds;
		memcpy(bp->leds, &resp->led0_id,
			sizeof(bp->leds[0]) * bp->leds->num_leds);
		for (i = 0; i < bp->leds->num_leds; i++) {
			struct bnxt_led_info *led = &bp->leds[i];

			uint16_t caps = led->led_state_caps;

			if (!led->led_group_id ||
				!BNXT_LED_ALT_BLINK_CAP(caps)) {
				bp->leds->num_leds = 0;
				break;
			}
		}
	}

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_port_led_cfg(struct bnxt *bp, bool led_on)
{
	struct hwrm_port_led_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_port_led_cfg_input req = {0};
	struct bnxt_led_cfg *led_cfg;
	uint8_t led_state = HWRM_PORT_LED_QCFG_OUTPUT_LED0_STATE_DEFAULT;
	uint16_t duration = 0;
	int rc, i;

	if (!bp->leds->num_leds || BNXT_VF(bp))
		return -EOPNOTSUPP;

	HWRM_PREP(&req, HWRM_PORT_LED_CFG, BNXT_USE_CHIMP_MB);

	if (led_on) {
		led_state = HWRM_PORT_LED_CFG_INPUT_LED0_STATE_BLINKALT;
		duration = rte_cpu_to_le_16(500);
	}
	req.port_id = bp->pf->port_id;
	req.num_leds = bp->leds->num_leds;
	led_cfg = (struct bnxt_led_cfg *)&req.led0_id;
	for (i = 0; i < bp->leds->num_leds; i++, led_cfg++) {
		req.enables |= BNXT_LED_DFLT_ENABLES(i);
		led_cfg->led_id = bp->leds[i].led_id;
		led_cfg->led_state = led_state;
		led_cfg->led_blink_on = duration;
		led_cfg->led_blink_off = duration;
		led_cfg->led_group_id = bp->leds[i].led_group_id;
	}

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_nvm_get_dir_info(struct bnxt *bp, uint32_t *entries,
			       uint32_t *length)
{
	int rc;
	struct hwrm_nvm_get_dir_info_input req = {0};
	struct hwrm_nvm_get_dir_info_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_NVM_GET_DIR_INFO, BNXT_USE_CHIMP_MB);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	*entries = rte_le_to_cpu_32(resp->entries);
	*length = rte_le_to_cpu_32(resp->entry_length);

	HWRM_UNLOCK();
	return rc;
}

int bnxt_get_nvram_directory(struct bnxt *bp, uint32_t len, uint8_t *data)
{
	int rc;
	uint32_t dir_entries;
	uint32_t entry_length;
	uint8_t *buf;
	size_t buflen;
	rte_iova_t dma_handle;
	struct hwrm_nvm_get_dir_entries_input req = {0};
	struct hwrm_nvm_get_dir_entries_output *resp = bp->hwrm_cmd_resp_addr;

	rc = bnxt_hwrm_nvm_get_dir_info(bp, &dir_entries, &entry_length);
	if (rc != 0)
		return rc;

	*data++ = dir_entries;
	*data++ = entry_length;
	len -= 2;
	memset(data, 0xff, len);

	buflen = dir_entries * entry_length;
	buf = rte_malloc("nvm_dir", buflen, 0);
	if (buf == NULL)
		return -ENOMEM;
	dma_handle = rte_malloc_virt2iova(buf);
	if (dma_handle == RTE_BAD_IOVA) {
		rte_free(buf);
		PMD_DRV_LOG(ERR,
			"unable to map response address to physical memory\n");
		return -ENOMEM;
	}
	HWRM_PREP(&req, HWRM_NVM_GET_DIR_ENTRIES, BNXT_USE_CHIMP_MB);
	req.host_dest_addr = rte_cpu_to_le_64(dma_handle);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	if (rc == 0)
		memcpy(data, buf, len > buflen ? buflen : len);

	rte_free(buf);
	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_get_nvram_item(struct bnxt *bp, uint32_t index,
			     uint32_t offset, uint32_t length,
			     uint8_t *data)
{
	int rc;
	uint8_t *buf;
	rte_iova_t dma_handle;
	struct hwrm_nvm_read_input req = {0};
	struct hwrm_nvm_read_output *resp = bp->hwrm_cmd_resp_addr;

	buf = rte_malloc("nvm_item", length, 0);
	if (!buf)
		return -ENOMEM;

	dma_handle = rte_malloc_virt2iova(buf);
	if (dma_handle == RTE_BAD_IOVA) {
		rte_free(buf);
		PMD_DRV_LOG(ERR,
			"unable to map response address to physical memory\n");
		return -ENOMEM;
	}
	HWRM_PREP(&req, HWRM_NVM_READ, BNXT_USE_CHIMP_MB);
	req.host_dest_addr = rte_cpu_to_le_64(dma_handle);
	req.dir_idx = rte_cpu_to_le_16(index);
	req.offset = rte_cpu_to_le_32(offset);
	req.len = rte_cpu_to_le_32(length);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	if (rc == 0)
		memcpy(data, buf, length);

	rte_free(buf);
	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_erase_nvram_directory(struct bnxt *bp, uint8_t index)
{
	int rc;
	struct hwrm_nvm_erase_dir_entry_input req = {0};
	struct hwrm_nvm_erase_dir_entry_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_NVM_ERASE_DIR_ENTRY, BNXT_USE_CHIMP_MB);
	req.dir_idx = rte_cpu_to_le_16(index);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_flash_nvram(struct bnxt *bp, uint16_t dir_type,
			  uint16_t dir_ordinal, uint16_t dir_ext,
			  uint16_t dir_attr, const uint8_t *data,
			  size_t data_len)
{
	int rc;
	struct hwrm_nvm_write_input req = {0};
	struct hwrm_nvm_write_output *resp = bp->hwrm_cmd_resp_addr;
	rte_iova_t dma_handle;
	uint8_t *buf;

	buf = rte_malloc("nvm_write", data_len, 0);
	if (!buf)
		return -ENOMEM;

	dma_handle = rte_malloc_virt2iova(buf);
	if (dma_handle == RTE_BAD_IOVA) {
		rte_free(buf);
		PMD_DRV_LOG(ERR,
			"unable to map response address to physical memory\n");
		return -ENOMEM;
	}
	memcpy(buf, data, data_len);

	HWRM_PREP(&req, HWRM_NVM_WRITE, BNXT_USE_CHIMP_MB);

	req.dir_type = rte_cpu_to_le_16(dir_type);
	req.dir_ordinal = rte_cpu_to_le_16(dir_ordinal);
	req.dir_ext = rte_cpu_to_le_16(dir_ext);
	req.dir_attr = rte_cpu_to_le_16(dir_attr);
	req.dir_data_length = rte_cpu_to_le_32(data_len);
	req.host_src_addr = rte_cpu_to_le_64(dma_handle);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	rte_free(buf);
	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

static void
bnxt_vnic_count(struct bnxt_vnic_info *vnic __rte_unused, void *cbdata)
{
	uint32_t *count = cbdata;

	*count = *count + 1;
}

static int bnxt_vnic_count_hwrm_stub(struct bnxt *bp __rte_unused,
				     struct bnxt_vnic_info *vnic __rte_unused)
{
	return 0;
}

int bnxt_vf_vnic_count(struct bnxt *bp, uint16_t vf)
{
	uint32_t count = 0;

	bnxt_hwrm_func_vf_vnic_query_and_config(bp, vf, bnxt_vnic_count,
	    &count, bnxt_vnic_count_hwrm_stub);

	return count;
}

static int bnxt_hwrm_func_vf_vnic_query(struct bnxt *bp, uint16_t vf,
					uint16_t *vnic_ids)
{
	struct hwrm_func_vf_vnic_ids_query_input req = {0};
	struct hwrm_func_vf_vnic_ids_query_output *resp =
						bp->hwrm_cmd_resp_addr;
	int rc;

	/* First query all VNIC ids */
	HWRM_PREP(&req, HWRM_FUNC_VF_VNIC_IDS_QUERY, BNXT_USE_CHIMP_MB);

	req.vf_id = rte_cpu_to_le_16(bp->pf->first_vf_id + vf);
	req.max_vnic_id_cnt = rte_cpu_to_le_32(bp->pf->total_vnics);
	req.vnic_id_tbl_addr = rte_cpu_to_le_64(rte_malloc_virt2iova(vnic_ids));

	if (req.vnic_id_tbl_addr == RTE_BAD_IOVA) {
		HWRM_UNLOCK();
		PMD_DRV_LOG(ERR,
		"unable to map VNIC ID table address to physical memory\n");
		return -ENOMEM;
	}
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();
	rc = rte_le_to_cpu_32(resp->vnic_id_cnt);

	HWRM_UNLOCK();

	return rc;
}

/*
 * This function queries the VNIC IDs  for a specified VF. It then calls
 * the vnic_cb to update the necessary field in vnic_info with cbdata.
 * Then it calls the hwrm_cb function to program this new vnic configuration.
 */
int bnxt_hwrm_func_vf_vnic_query_and_config(struct bnxt *bp, uint16_t vf,
	void (*vnic_cb)(struct bnxt_vnic_info *, void *), void *cbdata,
	int (*hwrm_cb)(struct bnxt *bp, struct bnxt_vnic_info *vnic))
{
	struct bnxt_vnic_info vnic;
	int rc = 0;
	int i, num_vnic_ids;
	uint16_t *vnic_ids;
	size_t vnic_id_sz;
	size_t sz;

	/* First query all VNIC ids */
	vnic_id_sz = bp->pf->total_vnics * sizeof(*vnic_ids);
	vnic_ids = rte_malloc("bnxt_hwrm_vf_vnic_ids_query", vnic_id_sz,
			RTE_CACHE_LINE_SIZE);
	if (vnic_ids == NULL)
		return -ENOMEM;

	for (sz = 0; sz < vnic_id_sz; sz += getpagesize())
		rte_mem_lock_page(((char *)vnic_ids) + sz);

	num_vnic_ids = bnxt_hwrm_func_vf_vnic_query(bp, vf, vnic_ids);

	if (num_vnic_ids < 0)
		return num_vnic_ids;

	/* Retrieve VNIC, update bd_stall then update */

	for (i = 0; i < num_vnic_ids; i++) {
		memset(&vnic, 0, sizeof(struct bnxt_vnic_info));
		vnic.fw_vnic_id = rte_le_to_cpu_16(vnic_ids[i]);
		rc = bnxt_hwrm_vnic_qcfg(bp, &vnic, bp->pf->first_vf_id + vf);
		if (rc)
			break;
		if (vnic.mru <= 4)	/* Indicates unallocated */
			continue;

		vnic_cb(&vnic, cbdata);

		rc = hwrm_cb(bp, &vnic);
		if (rc)
			break;
	}

	rte_free(vnic_ids);

	return rc;
}

int bnxt_hwrm_func_cfg_vf_set_vlan_anti_spoof(struct bnxt *bp, uint16_t vf,
					      bool on)
{
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_cfg_input req = {0};
	int rc;

	HWRM_PREP(&req, HWRM_FUNC_CFG, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(bp->pf->vf_info[vf].fid);
	req.enables |= rte_cpu_to_le_32(
			HWRM_FUNC_CFG_INPUT_ENABLES_VLAN_ANTISPOOF_MODE);
	req.vlan_antispoof_mode = on ?
		HWRM_FUNC_CFG_INPUT_VLAN_ANTISPOOF_MODE_VALIDATE_VLAN :
		HWRM_FUNC_CFG_INPUT_VLAN_ANTISPOOF_MODE_NOCHECK;
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_func_qcfg_vf_dflt_vnic_id(struct bnxt *bp, int vf)
{
	struct bnxt_vnic_info vnic;
	uint16_t *vnic_ids;
	size_t vnic_id_sz;
	int num_vnic_ids, i;
	size_t sz;
	int rc;

	vnic_id_sz = bp->pf->total_vnics * sizeof(*vnic_ids);
	vnic_ids = rte_malloc("bnxt_hwrm_vf_vnic_ids_query", vnic_id_sz,
			RTE_CACHE_LINE_SIZE);
	if (vnic_ids == NULL)
		return -ENOMEM;

	for (sz = 0; sz < vnic_id_sz; sz += getpagesize())
		rte_mem_lock_page(((char *)vnic_ids) + sz);

	rc = bnxt_hwrm_func_vf_vnic_query(bp, vf, vnic_ids);
	if (rc <= 0)
		goto exit;
	num_vnic_ids = rc;

	/*
	 * Loop through to find the default VNIC ID.
	 * TODO: The easier way would be to obtain the resp->dflt_vnic_id
	 * by sending the hwrm_func_qcfg command to the firmware.
	 */
	for (i = 0; i < num_vnic_ids; i++) {
		memset(&vnic, 0, sizeof(struct bnxt_vnic_info));
		vnic.fw_vnic_id = rte_le_to_cpu_16(vnic_ids[i]);
		rc = bnxt_hwrm_vnic_qcfg(bp, &vnic,
					bp->pf->first_vf_id + vf);
		if (rc)
			goto exit;
		if (vnic.func_default) {
			rte_free(vnic_ids);
			return vnic.fw_vnic_id;
		}
	}
	/* Could not find a default VNIC. */
	PMD_DRV_LOG(ERR, "No default VNIC\n");
exit:
	rte_free(vnic_ids);
	return rc;
}

int bnxt_hwrm_set_em_filter(struct bnxt *bp,
			 uint16_t dst_id,
			 struct bnxt_filter_info *filter)
{
	int rc = 0;
	struct hwrm_cfa_em_flow_alloc_input req = {.req_type = 0 };
	struct hwrm_cfa_em_flow_alloc_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t enables = 0;

	if (filter->fw_em_filter_id != UINT64_MAX)
		bnxt_hwrm_clear_em_filter(bp, filter);

	HWRM_PREP(&req, HWRM_CFA_EM_FLOW_ALLOC, BNXT_USE_KONG(bp));

	req.flags = rte_cpu_to_le_32(filter->flags);

	enables = filter->enables |
	      HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_DST_ID;
	req.dst_id = rte_cpu_to_le_16(dst_id);

	if (filter->ip_addr_type) {
		req.ip_addr_type = filter->ip_addr_type;
		enables |= HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_IPADDR_TYPE;
	}
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_L2_FILTER_ID)
		req.l2_filter_id = rte_cpu_to_le_64(filter->fw_l2_filter_id);
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_SRC_MACADDR)
		memcpy(req.src_macaddr, filter->src_macaddr,
		       RTE_ETHER_ADDR_LEN);
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_DST_MACADDR)
		memcpy(req.dst_macaddr, filter->dst_macaddr,
		       RTE_ETHER_ADDR_LEN);
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_OVLAN_VID)
		req.ovlan_vid = filter->l2_ovlan;
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_IVLAN_VID)
		req.ivlan_vid = filter->l2_ivlan;
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_ETHERTYPE)
		req.ethertype = rte_cpu_to_be_16(filter->ethertype);
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_IP_PROTOCOL)
		req.ip_protocol = filter->ip_protocol;
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_SRC_IPADDR)
		req.src_ipaddr[0] = rte_cpu_to_be_32(filter->src_ipaddr[0]);
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_DST_IPADDR)
		req.dst_ipaddr[0] = rte_cpu_to_be_32(filter->dst_ipaddr[0]);
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_SRC_PORT)
		req.src_port = rte_cpu_to_be_16(filter->src_port);
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_DST_PORT)
		req.dst_port = rte_cpu_to_be_16(filter->dst_port);
	if (enables &
	    HWRM_CFA_EM_FLOW_ALLOC_INPUT_ENABLES_MIRROR_VNIC_ID)
		req.mirror_vnic_id = filter->mirror_vnic_id;

	req.enables = rte_cpu_to_le_32(enables);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_KONG(bp));

	HWRM_CHECK_RESULT();

	filter->fw_em_filter_id = rte_le_to_cpu_64(resp->em_filter_id);
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_clear_em_filter(struct bnxt *bp, struct bnxt_filter_info *filter)
{
	int rc = 0;
	struct hwrm_cfa_em_flow_free_input req = {.req_type = 0 };
	struct hwrm_cfa_em_flow_free_output *resp = bp->hwrm_cmd_resp_addr;

	if (filter->fw_em_filter_id == UINT64_MAX)
		return 0;

	HWRM_PREP(&req, HWRM_CFA_EM_FLOW_FREE, BNXT_USE_KONG(bp));

	req.em_filter_id = rte_cpu_to_le_64(filter->fw_em_filter_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_KONG(bp));

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	filter->fw_em_filter_id = UINT64_MAX;
	filter->fw_l2_filter_id = UINT64_MAX;

	return 0;
}

int bnxt_hwrm_set_ntuple_filter(struct bnxt *bp,
			 uint16_t dst_id,
			 struct bnxt_filter_info *filter)
{
	int rc = 0;
	struct hwrm_cfa_ntuple_filter_alloc_input req = {.req_type = 0 };
	struct hwrm_cfa_ntuple_filter_alloc_output *resp =
						bp->hwrm_cmd_resp_addr;
	uint32_t enables = 0;

	if (filter->fw_ntuple_filter_id != UINT64_MAX)
		bnxt_hwrm_clear_ntuple_filter(bp, filter);

	HWRM_PREP(&req, HWRM_CFA_NTUPLE_FILTER_ALLOC, BNXT_USE_CHIMP_MB);

	req.flags = rte_cpu_to_le_32(filter->flags);

	enables = filter->enables |
	      HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_ID;
	req.dst_id = rte_cpu_to_le_16(dst_id);

	if (filter->ip_addr_type) {
		req.ip_addr_type = filter->ip_addr_type;
		enables |=
			HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_IPADDR_TYPE;
	}
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_L2_FILTER_ID)
		req.l2_filter_id = rte_cpu_to_le_64(filter->fw_l2_filter_id);
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_MACADDR)
		memcpy(req.src_macaddr, filter->src_macaddr,
		       RTE_ETHER_ADDR_LEN);
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_ETHERTYPE)
		req.ethertype = rte_cpu_to_be_16(filter->ethertype);
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_IP_PROTOCOL)
		req.ip_protocol = filter->ip_protocol;
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_IPADDR)
		req.src_ipaddr[0] = rte_cpu_to_le_32(filter->src_ipaddr[0]);
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_IPADDR_MASK)
		req.src_ipaddr_mask[0] =
			rte_cpu_to_le_32(filter->src_ipaddr_mask[0]);
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_IPADDR)
		req.dst_ipaddr[0] = rte_cpu_to_le_32(filter->dst_ipaddr[0]);
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_IPADDR_MASK)
		req.dst_ipaddr_mask[0] =
			rte_cpu_to_be_32(filter->dst_ipaddr_mask[0]);
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_PORT)
		req.src_port = rte_cpu_to_le_16(filter->src_port);
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_SRC_PORT_MASK)
		req.src_port_mask = rte_cpu_to_le_16(filter->src_port_mask);
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_PORT)
		req.dst_port = rte_cpu_to_le_16(filter->dst_port);
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_DST_PORT_MASK)
		req.dst_port_mask = rte_cpu_to_le_16(filter->dst_port_mask);
	if (enables &
	    HWRM_CFA_NTUPLE_FILTER_ALLOC_INPUT_ENABLES_MIRROR_VNIC_ID)
		req.mirror_vnic_id = filter->mirror_vnic_id;

	req.enables = rte_cpu_to_le_32(enables);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	filter->fw_ntuple_filter_id = rte_le_to_cpu_64(resp->ntuple_filter_id);
	filter->flow_id = rte_le_to_cpu_32(resp->flow_id);
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_clear_ntuple_filter(struct bnxt *bp,
				struct bnxt_filter_info *filter)
{
	int rc = 0;
	struct hwrm_cfa_ntuple_filter_free_input req = {.req_type = 0 };
	struct hwrm_cfa_ntuple_filter_free_output *resp =
						bp->hwrm_cmd_resp_addr;

	if (filter->fw_ntuple_filter_id == UINT64_MAX)
		return 0;

	HWRM_PREP(&req, HWRM_CFA_NTUPLE_FILTER_FREE, BNXT_USE_CHIMP_MB);

	req.ntuple_filter_id = rte_cpu_to_le_64(filter->fw_ntuple_filter_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	filter->fw_ntuple_filter_id = UINT64_MAX;

	return 0;
}

static int
bnxt_vnic_rss_configure_p5(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	struct hwrm_vnic_rss_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint8_t *rxq_state = bp->eth_dev->data->rx_queue_state;
	struct hwrm_vnic_rss_cfg_input req = {.req_type = 0 };
	struct bnxt_rx_queue **rxqs = bp->rx_queues;
	uint16_t *ring_tbl = vnic->rss_table;
	int nr_ctxs = vnic->num_lb_ctxts;
	int max_rings = bp->rx_nr_rings;
	int i, j, k, cnt;
	int rc = 0;

	for (i = 0, k = 0; i < nr_ctxs; i++) {
		struct bnxt_rx_ring_info *rxr;
		struct bnxt_cp_ring_info *cpr;

		HWRM_PREP(&req, HWRM_VNIC_RSS_CFG, BNXT_USE_CHIMP_MB);

		req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);
		req.hash_type = rte_cpu_to_le_32(vnic->hash_type);
		req.hash_mode_flags = vnic->hash_mode;

		req.ring_grp_tbl_addr =
		    rte_cpu_to_le_64(vnic->rss_table_dma_addr +
				     i * BNXT_RSS_ENTRIES_PER_CTX_P5 *
				     2 * sizeof(*ring_tbl));
		req.hash_key_tbl_addr =
		    rte_cpu_to_le_64(vnic->rss_hash_key_dma_addr);

		req.ring_table_pair_index = i;
		req.rss_ctx_idx = rte_cpu_to_le_16(vnic->fw_grp_ids[i]);

		for (j = 0; j < 64; j++) {
			uint16_t ring_id;

			/* Find next active ring. */
			for (cnt = 0; cnt < max_rings; cnt++) {
				if (rxq_state[k] != RTE_ETH_QUEUE_STATE_STOPPED)
					break;
				if (++k == max_rings)
					k = 0;
			}

			/* Return if no rings are active. */
			if (cnt == max_rings) {
				HWRM_UNLOCK();
				return 0;
			}

			/* Add rx/cp ring pair to RSS table. */
			rxr = rxqs[k]->rx_ring;
			cpr = rxqs[k]->cp_ring;

			ring_id = rxr->rx_ring_struct->fw_ring_id;
			*ring_tbl++ = rte_cpu_to_le_16(ring_id);
			ring_id = cpr->cp_ring_struct->fw_ring_id;
			*ring_tbl++ = rte_cpu_to_le_16(ring_id);

			if (++k == max_rings)
				k = 0;
		}
		rc = bnxt_hwrm_send_message(bp, &req, sizeof(req),
					    BNXT_USE_CHIMP_MB);

		HWRM_CHECK_RESULT();
		HWRM_UNLOCK();
	}

	return rc;
}

int bnxt_vnic_rss_configure(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	unsigned int rss_idx, fw_idx, i;

	if (vnic->fw_vnic_id == INVALID_HW_RING_ID)
		return 0;

	if (!(vnic->rss_table && vnic->hash_type))
		return 0;

	if (BNXT_CHIP_P5(bp))
		return bnxt_vnic_rss_configure_p5(bp, vnic);

	/*
	 * Fill the RSS hash & redirection table with
	 * ring group ids for all VNICs
	 */
	for (rss_idx = 0, fw_idx = 0; rss_idx < HW_HASH_INDEX_SIZE;
	     rss_idx++, fw_idx++) {
		for (i = 0; i < bp->rx_cp_nr_rings; i++) {
			fw_idx %= bp->rx_cp_nr_rings;
			if (vnic->fw_grp_ids[fw_idx] != INVALID_HW_RING_ID)
				break;
			fw_idx++;
		}

		if (i == bp->rx_cp_nr_rings)
			return 0;

		vnic->rss_table[rss_idx] = vnic->fw_grp_ids[fw_idx];
	}

	return bnxt_hwrm_vnic_rss_cfg(bp, vnic);
}

static void bnxt_hwrm_set_coal_params(struct bnxt_coal *hw_coal,
	struct hwrm_ring_cmpl_ring_cfg_aggint_params_input *req)
{
	uint16_t flags;

	req->num_cmpl_aggr_int = rte_cpu_to_le_16(hw_coal->num_cmpl_aggr_int);

	/* This is a 6-bit value and must not be 0, or we'll get non stop IRQ */
	req->num_cmpl_dma_aggr = rte_cpu_to_le_16(hw_coal->num_cmpl_dma_aggr);

	/* This is a 6-bit value and must not be 0, or we'll get non stop IRQ */
	req->num_cmpl_dma_aggr_during_int =
		rte_cpu_to_le_16(hw_coal->num_cmpl_dma_aggr_during_int);

	req->int_lat_tmr_max = rte_cpu_to_le_16(hw_coal->int_lat_tmr_max);

	/* min timer set to 1/2 of interrupt timer */
	req->int_lat_tmr_min = rte_cpu_to_le_16(hw_coal->int_lat_tmr_min);

	/* buf timer set to 1/4 of interrupt timer */
	req->cmpl_aggr_dma_tmr = rte_cpu_to_le_16(hw_coal->cmpl_aggr_dma_tmr);

	req->cmpl_aggr_dma_tmr_during_int =
		rte_cpu_to_le_16(hw_coal->cmpl_aggr_dma_tmr_during_int);

	flags = HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_FLAGS_TIMER_RESET |
		HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_FLAGS_RING_IDLE;
	req->flags = rte_cpu_to_le_16(flags);
}

static int bnxt_hwrm_set_coal_params_p5(struct bnxt *bp,
		struct hwrm_ring_cmpl_ring_cfg_aggint_params_input *agg_req)
{
	struct hwrm_ring_aggint_qcaps_input req = {0};
	struct hwrm_ring_aggint_qcaps_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t enables;
	uint16_t flags;
	int rc;

	HWRM_PREP(&req, HWRM_RING_AGGINT_QCAPS, BNXT_USE_CHIMP_MB);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();

	agg_req->num_cmpl_dma_aggr = resp->num_cmpl_dma_aggr_max;
	agg_req->cmpl_aggr_dma_tmr = resp->cmpl_aggr_dma_tmr_min;

	flags = HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_FLAGS_TIMER_RESET |
		HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_FLAGS_RING_IDLE;
	agg_req->flags = rte_cpu_to_le_16(flags);
	enables =
	 HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_ENABLES_CMPL_AGGR_DMA_TMR |
	 HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS_INPUT_ENABLES_NUM_CMPL_DMA_AGGR;
	agg_req->enables = rte_cpu_to_le_32(enables);

	HWRM_UNLOCK();
	return rc;
}

int bnxt_hwrm_set_ring_coal(struct bnxt *bp,
			struct bnxt_coal *coal, uint16_t ring_id)
{
	struct hwrm_ring_cmpl_ring_cfg_aggint_params_input req = {0};
	struct hwrm_ring_cmpl_ring_cfg_aggint_params_output *resp =
						bp->hwrm_cmd_resp_addr;
	int rc;

	/* Set ring coalesce parameters only for 100G NICs */
	if (BNXT_CHIP_P5(bp)) {
		if (bnxt_hwrm_set_coal_params_p5(bp, &req))
			return -1;
	} else if (bnxt_stratus_device(bp)) {
		bnxt_hwrm_set_coal_params(coal, &req);
	} else {
		return 0;
	}

	HWRM_PREP(&req,
		  HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS,
		  BNXT_USE_CHIMP_MB);
	req.ring_id = rte_cpu_to_le_16(ring_id);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();
	return 0;
}

#define BNXT_RTE_MEMZONE_FLAG  (RTE_MEMZONE_1GB | RTE_MEMZONE_IOVA_CONTIG)
int bnxt_hwrm_func_backing_store_qcaps(struct bnxt *bp)
{
	struct hwrm_func_backing_store_qcaps_input req = {0};
	struct hwrm_func_backing_store_qcaps_output *resp =
		bp->hwrm_cmd_resp_addr;
	struct bnxt_ctx_pg_info *ctx_pg;
	struct bnxt_ctx_mem_info *ctx;
	int total_alloc_len;
	int rc, i, tqm_rings;

	if (!BNXT_CHIP_P5(bp) ||
	    bp->hwrm_spec_code < HWRM_VERSION_1_9_2 ||
	    BNXT_VF(bp) ||
	    bp->ctx)
		return 0;

	HWRM_PREP(&req, HWRM_FUNC_BACKING_STORE_QCAPS, BNXT_USE_CHIMP_MB);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT_SILENT();

	total_alloc_len = sizeof(*ctx);
	ctx = rte_zmalloc("bnxt_ctx_mem", total_alloc_len,
			  RTE_CACHE_LINE_SIZE);
	if (!ctx) {
		rc = -ENOMEM;
		goto ctx_err;
	}

	ctx->qp_max_entries = rte_le_to_cpu_32(resp->qp_max_entries);
	ctx->qp_min_qp1_entries =
		rte_le_to_cpu_16(resp->qp_min_qp1_entries);
	ctx->qp_max_l2_entries =
		rte_le_to_cpu_16(resp->qp_max_l2_entries);
	ctx->qp_entry_size = rte_le_to_cpu_16(resp->qp_entry_size);
	ctx->srq_max_l2_entries =
		rte_le_to_cpu_16(resp->srq_max_l2_entries);
	ctx->srq_max_entries = rte_le_to_cpu_32(resp->srq_max_entries);
	ctx->srq_entry_size = rte_le_to_cpu_16(resp->srq_entry_size);
	ctx->cq_max_l2_entries =
		rte_le_to_cpu_16(resp->cq_max_l2_entries);
	ctx->cq_max_entries = rte_le_to_cpu_32(resp->cq_max_entries);
	ctx->cq_entry_size = rte_le_to_cpu_16(resp->cq_entry_size);
	ctx->vnic_max_vnic_entries =
		rte_le_to_cpu_16(resp->vnic_max_vnic_entries);
	ctx->vnic_max_ring_table_entries =
		rte_le_to_cpu_16(resp->vnic_max_ring_table_entries);
	ctx->vnic_entry_size = rte_le_to_cpu_16(resp->vnic_entry_size);
	ctx->stat_max_entries =
		rte_le_to_cpu_32(resp->stat_max_entries);
	ctx->stat_entry_size = rte_le_to_cpu_16(resp->stat_entry_size);
	ctx->tqm_entry_size = rte_le_to_cpu_16(resp->tqm_entry_size);
	ctx->tqm_min_entries_per_ring =
		rte_le_to_cpu_32(resp->tqm_min_entries_per_ring);
	ctx->tqm_max_entries_per_ring =
		rte_le_to_cpu_32(resp->tqm_max_entries_per_ring);
	ctx->tqm_entries_multiple = resp->tqm_entries_multiple;
	if (!ctx->tqm_entries_multiple)
		ctx->tqm_entries_multiple = 1;
	ctx->mrav_max_entries =
		rte_le_to_cpu_32(resp->mrav_max_entries);
	ctx->mrav_entry_size = rte_le_to_cpu_16(resp->mrav_entry_size);
	ctx->tim_entry_size = rte_le_to_cpu_16(resp->tim_entry_size);
	ctx->tim_max_entries = rte_le_to_cpu_32(resp->tim_max_entries);
	ctx->tqm_fp_rings_count = resp->tqm_fp_rings_count;

	ctx->tqm_fp_rings_count = ctx->tqm_fp_rings_count ?
				  RTE_MIN(ctx->tqm_fp_rings_count,
					  BNXT_MAX_TQM_FP_LEGACY_RINGS) :
				  bp->max_q;

	/* Check if the ext ring count needs to be counted.
	 * Ext ring count is available only with new FW so we should not
	 * look at the field on older FW.
	 */
	if (ctx->tqm_fp_rings_count == BNXT_MAX_TQM_FP_LEGACY_RINGS &&
	    bp->hwrm_max_ext_req_len >= BNXT_BACKING_STORE_CFG_LEN) {
		ctx->tqm_fp_rings_count += resp->tqm_fp_rings_count_ext;
		ctx->tqm_fp_rings_count = RTE_MIN(BNXT_MAX_TQM_FP_RINGS,
						  ctx->tqm_fp_rings_count);
	}

	tqm_rings = ctx->tqm_fp_rings_count + 1;

	ctx_pg = rte_malloc("bnxt_ctx_pg_mem",
			    sizeof(*ctx_pg) * tqm_rings,
			    RTE_CACHE_LINE_SIZE);
	if (!ctx_pg) {
		rc = -ENOMEM;
		goto ctx_err;
	}
	for (i = 0; i < tqm_rings; i++, ctx_pg++)
		ctx->tqm_mem[i] = ctx_pg;

	bp->ctx = ctx;
ctx_err:
	HWRM_UNLOCK();
	return rc;
}

int bnxt_hwrm_func_backing_store_cfg(struct bnxt *bp, uint32_t enables)
{
	struct hwrm_func_backing_store_cfg_input req = {0};
	struct hwrm_func_backing_store_cfg_output *resp =
		bp->hwrm_cmd_resp_addr;
	struct bnxt_ctx_mem_info *ctx = bp->ctx;
	struct bnxt_ctx_pg_info *ctx_pg;
	uint32_t *num_entries;
	uint64_t *pg_dir;
	uint8_t *pg_attr;
	uint32_t ena;
	int i, rc;

	if (!ctx)
		return 0;

	HWRM_PREP(&req, HWRM_FUNC_BACKING_STORE_CFG, BNXT_USE_CHIMP_MB);
	req.enables = rte_cpu_to_le_32(enables);

	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_QP) {
		ctx_pg = &ctx->qp_mem;
		req.qp_num_entries = rte_cpu_to_le_32(ctx_pg->entries);
		req.qp_num_qp1_entries =
			rte_cpu_to_le_16(ctx->qp_min_qp1_entries);
		req.qp_num_l2_entries =
			rte_cpu_to_le_16(ctx->qp_max_l2_entries);
		req.qp_entry_size = rte_cpu_to_le_16(ctx->qp_entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				      &req.qpc_pg_size_qpc_lvl,
				      &req.qpc_page_dir);
	}

	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_SRQ) {
		ctx_pg = &ctx->srq_mem;
		req.srq_num_entries = rte_cpu_to_le_32(ctx_pg->entries);
		req.srq_num_l2_entries =
				 rte_cpu_to_le_16(ctx->srq_max_l2_entries);
		req.srq_entry_size = rte_cpu_to_le_16(ctx->srq_entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				      &req.srq_pg_size_srq_lvl,
				      &req.srq_page_dir);
	}

	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_CQ) {
		ctx_pg = &ctx->cq_mem;
		req.cq_num_entries = rte_cpu_to_le_32(ctx_pg->entries);
		req.cq_num_l2_entries =
				rte_cpu_to_le_16(ctx->cq_max_l2_entries);
		req.cq_entry_size = rte_cpu_to_le_16(ctx->cq_entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				      &req.cq_pg_size_cq_lvl,
				      &req.cq_page_dir);
	}

	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_VNIC) {
		ctx_pg = &ctx->vnic_mem;
		req.vnic_num_vnic_entries =
			rte_cpu_to_le_16(ctx->vnic_max_vnic_entries);
		req.vnic_num_ring_table_entries =
			rte_cpu_to_le_16(ctx->vnic_max_ring_table_entries);
		req.vnic_entry_size = rte_cpu_to_le_16(ctx->vnic_entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				      &req.vnic_pg_size_vnic_lvl,
				      &req.vnic_page_dir);
	}

	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_STAT) {
		ctx_pg = &ctx->stat_mem;
		req.stat_num_entries = rte_cpu_to_le_16(ctx->stat_max_entries);
		req.stat_entry_size = rte_cpu_to_le_16(ctx->stat_entry_size);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				      &req.stat_pg_size_stat_lvl,
				      &req.stat_page_dir);
	}

	req.tqm_entry_size = rte_cpu_to_le_16(ctx->tqm_entry_size);
	num_entries = &req.tqm_sp_num_entries;
	pg_attr = &req.tqm_sp_pg_size_tqm_sp_lvl;
	pg_dir = &req.tqm_sp_page_dir;
	ena = HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TQM_SP;
	for (i = 0; i < 9; i++, num_entries++, pg_attr++, pg_dir++, ena <<= 1) {
		if (!(enables & ena))
			continue;

		req.tqm_entry_size = rte_cpu_to_le_16(ctx->tqm_entry_size);

		ctx_pg = ctx->tqm_mem[i];
		*num_entries = rte_cpu_to_le_16(ctx_pg->entries);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem, pg_attr, pg_dir);
	}

	if (enables & HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TQM_RING8) {
		/* DPDK does not need to configure MRAV and TIM type.
		 * So we are skipping over MRAV and TIM. Skip to configure
		 * HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_TQM_RING8.
		 */
		ctx_pg = ctx->tqm_mem[BNXT_MAX_TQM_LEGACY_RINGS];
		req.tqm_ring8_num_entries = rte_cpu_to_le_16(ctx_pg->entries);
		bnxt_hwrm_set_pg_attr(&ctx_pg->ring_mem,
				      &req.tqm_ring8_pg_size_tqm_ring_lvl,
				      &req.tqm_ring8_page_dir);
	}

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_ext_port_qstats(struct bnxt *bp)
{
	struct hwrm_port_qstats_ext_input req = {0};
	struct hwrm_port_qstats_ext_output *resp = bp->hwrm_cmd_resp_addr;
	struct bnxt_pf_info *pf = bp->pf;
	int rc;

	if (!(bp->flags & BNXT_FLAG_EXT_RX_PORT_STATS ||
	      bp->flags & BNXT_FLAG_EXT_TX_PORT_STATS))
		return 0;

	HWRM_PREP(&req, HWRM_PORT_QSTATS_EXT, BNXT_USE_CHIMP_MB);

	req.port_id = rte_cpu_to_le_16(pf->port_id);
	if (bp->flags & BNXT_FLAG_EXT_TX_PORT_STATS) {
		req.tx_stat_host_addr =
			rte_cpu_to_le_64(bp->hw_tx_port_stats_ext_map);
		req.tx_stat_size =
			rte_cpu_to_le_16(sizeof(struct tx_port_stats_ext));
	}
	if (bp->flags & BNXT_FLAG_EXT_RX_PORT_STATS) {
		req.rx_stat_host_addr =
			rte_cpu_to_le_64(bp->hw_rx_port_stats_ext_map);
		req.rx_stat_size =
			rte_cpu_to_le_16(sizeof(struct rx_port_stats_ext));
	}
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	if (rc) {
		bp->fw_rx_port_stats_ext_size = 0;
		bp->fw_tx_port_stats_ext_size = 0;
	} else {
		bp->fw_rx_port_stats_ext_size =
			rte_le_to_cpu_16(resp->rx_stat_size);
		bp->fw_tx_port_stats_ext_size =
			rte_le_to_cpu_16(resp->tx_stat_size);
	}

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int
bnxt_hwrm_tunnel_redirect(struct bnxt *bp, uint8_t type)
{
	struct hwrm_cfa_redirect_tunnel_type_alloc_input req = {0};
	struct hwrm_cfa_redirect_tunnel_type_alloc_output *resp =
		bp->hwrm_cmd_resp_addr;
	int rc = 0;

	HWRM_PREP(&req, HWRM_CFA_REDIRECT_TUNNEL_TYPE_ALLOC, BNXT_USE_CHIMP_MB);
	req.tunnel_type = type;
	req.dest_fid = bp->fw_fid;
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();

	HWRM_UNLOCK();

	return rc;
}

int
bnxt_hwrm_tunnel_redirect_free(struct bnxt *bp, uint8_t type)
{
	struct hwrm_cfa_redirect_tunnel_type_free_input req = {0};
	struct hwrm_cfa_redirect_tunnel_type_free_output *resp =
		bp->hwrm_cmd_resp_addr;
	int rc = 0;

	HWRM_PREP(&req, HWRM_CFA_REDIRECT_TUNNEL_TYPE_FREE, BNXT_USE_CHIMP_MB);
	req.tunnel_type = type;
	req.dest_fid = bp->fw_fid;
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_tunnel_redirect_query(struct bnxt *bp, uint32_t *type)
{
	struct hwrm_cfa_redirect_query_tunnel_type_input req = {0};
	struct hwrm_cfa_redirect_query_tunnel_type_output *resp =
		bp->hwrm_cmd_resp_addr;
	int rc = 0;

	HWRM_PREP(&req, HWRM_CFA_REDIRECT_QUERY_TUNNEL_TYPE, BNXT_USE_CHIMP_MB);
	req.src_fid = bp->fw_fid;
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();

	if (type)
		*type = rte_le_to_cpu_32(resp->tunnel_mask);

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_tunnel_redirect_info(struct bnxt *bp, uint8_t tun_type,
				   uint16_t *dst_fid)
{
	struct hwrm_cfa_redirect_tunnel_type_info_input req = {0};
	struct hwrm_cfa_redirect_tunnel_type_info_output *resp =
		bp->hwrm_cmd_resp_addr;
	int rc = 0;

	HWRM_PREP(&req, HWRM_CFA_REDIRECT_TUNNEL_TYPE_INFO, BNXT_USE_CHIMP_MB);
	req.src_fid = bp->fw_fid;
	req.tunnel_type = tun_type;
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();

	if (dst_fid)
		*dst_fid = rte_le_to_cpu_16(resp->dest_fid);

	PMD_DRV_LOG(DEBUG, "dst_fid: %x\n", resp->dest_fid);

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_set_mac(struct bnxt *bp)
{
	struct hwrm_func_vf_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_vf_cfg_input req = {0};
	int rc = 0;

	if (!BNXT_VF(bp))
		return 0;

	HWRM_PREP(&req, HWRM_FUNC_VF_CFG, BNXT_USE_CHIMP_MB);

	req.enables =
		rte_cpu_to_le_32(HWRM_FUNC_VF_CFG_INPUT_ENABLES_DFLT_MAC_ADDR);
	memcpy(req.dflt_mac_addr, bp->mac_addr, RTE_ETHER_ADDR_LEN);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_if_change(struct bnxt *bp, bool up)
{
	struct hwrm_func_drv_if_change_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_drv_if_change_input req = {0};
	uint32_t flags;
	int rc;

	if (!(bp->fw_cap & BNXT_FW_CAP_IF_CHANGE))
		return 0;

	/* Do not issue FUNC_DRV_IF_CHANGE during reset recovery.
	 * If we issue FUNC_DRV_IF_CHANGE with flags down before
	 * FUNC_DRV_UNRGTR, FW resets before FUNC_DRV_UNRGTR
	 */
	if (!up && (bp->flags & BNXT_FLAG_FW_RESET))
		return 0;

	HWRM_PREP(&req, HWRM_FUNC_DRV_IF_CHANGE, BNXT_USE_CHIMP_MB);

	if (up)
		req.flags =
		rte_cpu_to_le_32(HWRM_FUNC_DRV_IF_CHANGE_INPUT_FLAGS_UP);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	flags = rte_le_to_cpu_32(resp->flags);
	HWRM_UNLOCK();

	if (!up)
		return 0;

	if (flags & HWRM_FUNC_DRV_IF_CHANGE_OUTPUT_FLAGS_HOT_FW_RESET_DONE) {
		PMD_DRV_LOG(INFO, "FW reset happened while port was down\n");
		bp->flags |= BNXT_FLAG_IF_CHANGE_HOT_FW_RESET_DONE;
	}

	return 0;
}

int bnxt_hwrm_error_recovery_qcfg(struct bnxt *bp)
{
	struct hwrm_error_recovery_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct bnxt_error_recovery_info *info = bp->recovery_info;
	struct hwrm_error_recovery_qcfg_input req = {0};
	uint32_t flags = 0;
	unsigned int i;
	int rc;

	/* Older FW does not have error recovery support */
	if (!(bp->fw_cap & BNXT_FW_CAP_ERROR_RECOVERY))
		return 0;

	HWRM_PREP(&req, HWRM_ERROR_RECOVERY_QCFG, BNXT_USE_CHIMP_MB);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	flags = rte_le_to_cpu_32(resp->flags);
	if (flags & HWRM_ERROR_RECOVERY_QCFG_OUTPUT_FLAGS_HOST)
		info->flags |= BNXT_FLAG_ERROR_RECOVERY_HOST;
	else if (flags & HWRM_ERROR_RECOVERY_QCFG_OUTPUT_FLAGS_CO_CPU)
		info->flags |= BNXT_FLAG_ERROR_RECOVERY_CO_CPU;

	if ((info->flags & BNXT_FLAG_ERROR_RECOVERY_CO_CPU) &&
	    !(bp->flags & BNXT_FLAG_KONG_MB_EN)) {
		rc = -EINVAL;
		goto err;
	}

	/* FW returned values are in units of 100msec */
	info->driver_polling_freq =
		rte_le_to_cpu_32(resp->driver_polling_freq) * 100;
	info->primary_func_wait_period =
		rte_le_to_cpu_32(resp->master_func_wait_period) * 100;
	info->normal_func_wait_period =
		rte_le_to_cpu_32(resp->normal_func_wait_period) * 100;
	info->primary_func_wait_period_after_reset =
		rte_le_to_cpu_32(resp->master_func_wait_period_after_reset) * 100;
	info->max_bailout_time_after_reset =
		rte_le_to_cpu_32(resp->max_bailout_time_after_reset) * 100;
	info->status_regs[BNXT_FW_STATUS_REG] =
		rte_le_to_cpu_32(resp->fw_health_status_reg);
	info->status_regs[BNXT_FW_HEARTBEAT_CNT_REG] =
		rte_le_to_cpu_32(resp->fw_heartbeat_reg);
	info->status_regs[BNXT_FW_RECOVERY_CNT_REG] =
		rte_le_to_cpu_32(resp->fw_reset_cnt_reg);
	info->status_regs[BNXT_FW_RESET_INPROG_REG] =
		rte_le_to_cpu_32(resp->reset_inprogress_reg);
	info->reg_array_cnt =
		rte_le_to_cpu_32(resp->reg_array_cnt);

	if (info->reg_array_cnt >= BNXT_NUM_RESET_REG) {
		rc = -EINVAL;
		goto err;
	}

	for (i = 0; i < info->reg_array_cnt; i++) {
		info->reset_reg[i] =
			rte_le_to_cpu_32(resp->reset_reg[i]);
		info->reset_reg_val[i] =
			rte_le_to_cpu_32(resp->reset_reg_val[i]);
		info->delay_after_reset[i] =
			resp->delay_after_reset[i];
	}
err:
	HWRM_UNLOCK();

	/* Map the FW status registers */
	if (!rc)
		rc = bnxt_map_fw_health_status_regs(bp);

	if (rc) {
		rte_free(bp->recovery_info);
		bp->recovery_info = NULL;
	}
	return rc;
}

int bnxt_hwrm_fw_reset(struct bnxt *bp)
{
	struct hwrm_fw_reset_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_fw_reset_input req = {0};
	int rc;

	if (!BNXT_PF(bp))
		return -EOPNOTSUPP;

	HWRM_PREP(&req, HWRM_FW_RESET, BNXT_USE_KONG(bp));

	req.embedded_proc_type =
		HWRM_FW_RESET_INPUT_EMBEDDED_PROC_TYPE_CHIP;
	req.selfrst_status =
		HWRM_FW_RESET_INPUT_SELFRST_STATUS_SELFRSTASAP;
	req.flags = HWRM_FW_RESET_INPUT_FLAGS_RESET_GRACEFUL;

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req),
				    BNXT_USE_KONG(bp));

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_port_ts_query(struct bnxt *bp, uint8_t path, uint64_t *timestamp)
{
	struct hwrm_port_ts_query_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_port_ts_query_input req = {0};
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	uint32_t flags = 0;
	int rc;

	if (!ptp)
		return 0;

	HWRM_PREP(&req, HWRM_PORT_TS_QUERY, BNXT_USE_CHIMP_MB);

	switch (path) {
	case BNXT_PTP_FLAGS_PATH_TX:
		flags |= HWRM_PORT_TS_QUERY_INPUT_FLAGS_PATH_TX;
		break;
	case BNXT_PTP_FLAGS_PATH_RX:
		flags |= HWRM_PORT_TS_QUERY_INPUT_FLAGS_PATH_RX;
		break;
	case BNXT_PTP_FLAGS_CURRENT_TIME:
		flags |= HWRM_PORT_TS_QUERY_INPUT_FLAGS_CURRENT_TIME;
		break;
	}

	req.flags = rte_cpu_to_le_32(flags);
	req.port_id = rte_cpu_to_le_16(bp->pf->port_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	if (timestamp) {
		*timestamp = rte_le_to_cpu_32(resp->ptp_msg_ts[0]);
		*timestamp |=
			(uint64_t)(rte_le_to_cpu_32(resp->ptp_msg_ts[1])) << 32;
	}
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_cfa_counter_qcaps(struct bnxt *bp, uint16_t *max_fc)
{
	int rc = 0;

	struct hwrm_cfa_counter_qcaps_input req = {0};
	struct hwrm_cfa_counter_qcaps_output *resp = bp->hwrm_cmd_resp_addr;

	if (!(BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))) {
		PMD_DRV_LOG(DEBUG,
			    "Not a PF or trusted VF. Command not supported\n");
		return 0;
	}

	HWRM_PREP(&req, HWRM_CFA_COUNTER_QCAPS, BNXT_USE_KONG(bp));
	req.target_id = rte_cpu_to_le_16(bp->fw_fid);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_KONG(bp));

	HWRM_CHECK_RESULT();
	if (max_fc)
		*max_fc = rte_le_to_cpu_16(resp->max_rx_fc);
	HWRM_UNLOCK();

	return 0;
}

int bnxt_hwrm_ctx_rgtr(struct bnxt *bp, rte_iova_t dma_addr, uint16_t *ctx_id)
{
	int rc = 0;
	struct hwrm_cfa_ctx_mem_rgtr_input req = {.req_type = 0 };
	struct hwrm_cfa_ctx_mem_rgtr_output *resp = bp->hwrm_cmd_resp_addr;

	if (!(BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))) {
		PMD_DRV_LOG(DEBUG,
			    "Not a PF or trusted VF. Command not supported\n");
		return 0;
	}

	HWRM_PREP(&req, HWRM_CFA_CTX_MEM_RGTR, BNXT_USE_KONG(bp));

	req.page_level = HWRM_CFA_CTX_MEM_RGTR_INPUT_PAGE_LEVEL_LVL_0;
	req.page_size = HWRM_CFA_CTX_MEM_RGTR_INPUT_PAGE_SIZE_2M;
	req.page_dir = rte_cpu_to_le_64(dma_addr);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_KONG(bp));

	HWRM_CHECK_RESULT();
	if (ctx_id) {
		*ctx_id  = rte_le_to_cpu_16(resp->ctx_id);
		PMD_DRV_LOG(DEBUG, "ctx_id = %d\n", *ctx_id);
	}
	HWRM_UNLOCK();

	return 0;
}

int bnxt_hwrm_ctx_unrgtr(struct bnxt *bp, uint16_t ctx_id)
{
	int rc = 0;
	struct hwrm_cfa_ctx_mem_unrgtr_input req = {.req_type = 0 };
	struct hwrm_cfa_ctx_mem_unrgtr_output *resp = bp->hwrm_cmd_resp_addr;

	if (!(BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))) {
		PMD_DRV_LOG(DEBUG,
			    "Not a PF or trusted VF. Command not supported\n");
		return 0;
	}

	HWRM_PREP(&req, HWRM_CFA_CTX_MEM_UNRGTR, BNXT_USE_KONG(bp));

	req.ctx_id = rte_cpu_to_le_16(ctx_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_KONG(bp));

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_cfa_counter_cfg(struct bnxt *bp, enum bnxt_flow_dir dir,
			      uint16_t cntr, uint16_t ctx_id,
			      uint32_t num_entries, bool enable)
{
	struct hwrm_cfa_counter_cfg_input req = {0};
	struct hwrm_cfa_counter_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint16_t flags = 0;
	int rc;

	if (!(BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))) {
		PMD_DRV_LOG(DEBUG,
			    "Not a PF or trusted VF. Command not supported\n");
		return 0;
	}

	HWRM_PREP(&req, HWRM_CFA_COUNTER_CFG, BNXT_USE_KONG(bp));

	req.target_id = rte_cpu_to_le_16(bp->fw_fid);
	req.counter_type = rte_cpu_to_le_16(cntr);
	flags = enable ? HWRM_CFA_COUNTER_CFG_INPUT_FLAGS_CFG_MODE_ENABLE :
		HWRM_CFA_COUNTER_CFG_INPUT_FLAGS_CFG_MODE_DISABLE;
	flags |= HWRM_CFA_COUNTER_CFG_INPUT_FLAGS_DATA_TRANSFER_MODE_PULL;
	if (dir == BNXT_DIR_RX)
		flags |=  HWRM_CFA_COUNTER_CFG_INPUT_FLAGS_PATH_RX;
	else if (dir == BNXT_DIR_TX)
		flags |=  HWRM_CFA_COUNTER_CFG_INPUT_FLAGS_PATH_TX;
	req.flags = rte_cpu_to_le_16(flags);
	req.ctx_id =  rte_cpu_to_le_16(ctx_id);
	req.num_entries = rte_cpu_to_le_32(num_entries);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_KONG(bp));
	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return 0;
}

int bnxt_hwrm_cfa_counter_qstats(struct bnxt *bp,
				 enum bnxt_flow_dir dir,
				 uint16_t cntr,
				 uint16_t num_entries)
{
	struct hwrm_cfa_counter_qstats_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_cfa_counter_qstats_input req = {0};
	uint16_t flow_ctx_id = 0;
	uint16_t flags = 0;
	int rc = 0;

	if (!(BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))) {
		PMD_DRV_LOG(DEBUG,
			    "Not a PF or trusted VF. Command not supported\n");
		return 0;
	}

	if (dir == BNXT_DIR_RX) {
		flow_ctx_id = bp->flow_stat->rx_fc_in_tbl.ctx_id;
		flags = HWRM_CFA_COUNTER_QSTATS_INPUT_FLAGS_PATH_RX;
	} else if (dir == BNXT_DIR_TX) {
		flow_ctx_id = bp->flow_stat->tx_fc_in_tbl.ctx_id;
		flags = HWRM_CFA_COUNTER_QSTATS_INPUT_FLAGS_PATH_TX;
	}

	HWRM_PREP(&req, HWRM_CFA_COUNTER_QSTATS, BNXT_USE_KONG(bp));
	req.target_id = rte_cpu_to_le_16(bp->fw_fid);
	req.counter_type = rte_cpu_to_le_16(cntr);
	req.input_flow_ctx_id = rte_cpu_to_le_16(flow_ctx_id);
	req.num_entries = rte_cpu_to_le_16(num_entries);
	req.flags = rte_cpu_to_le_16(flags);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_KONG(bp));

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return 0;
}

int bnxt_hwrm_first_vf_id_query(struct bnxt *bp, uint16_t fid,
				uint16_t *first_vf_id)
{
	int rc = 0;
	struct hwrm_func_qcaps_input req = {.req_type = 0 };
	struct hwrm_func_qcaps_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(&req, HWRM_FUNC_QCAPS, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(fid);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();

	if (first_vf_id)
		*first_vf_id = rte_le_to_cpu_16(resp->first_vf_id);

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_cfa_pair_exists(struct bnxt *bp, struct bnxt_representor *rep_bp)
{
	struct hwrm_cfa_pair_info_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_cfa_pair_info_input req = {0};
	int rc = 0;

	if (!(BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))) {
		PMD_DRV_LOG(DEBUG,
			    "Not a PF or trusted VF. Command not supported\n");
		return 0;
	}

	HWRM_PREP(&req, HWRM_CFA_PAIR_INFO, BNXT_USE_CHIMP_MB);
	snprintf(req.pair_name, sizeof(req.pair_name), "%svfr%d",
		 bp->eth_dev->data->name, rep_bp->vf_id);
	req.flags =
		rte_cpu_to_le_32(HWRM_CFA_PAIR_INFO_INPUT_FLAGS_LOOKUP_TYPE);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();
	if (rc == HWRM_ERR_CODE_SUCCESS && strlen(resp->pair_name)) {
		HWRM_UNLOCK();
		return !rc;
	}
	HWRM_UNLOCK();
	return rc;
}

int bnxt_hwrm_cfa_pair_alloc(struct bnxt *bp, struct bnxt_representor *rep_bp)
{
	struct hwrm_cfa_pair_alloc_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_cfa_pair_alloc_input req = {0};
	int rc;

	if (!(BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))) {
		PMD_DRV_LOG(DEBUG,
			    "Not a PF or trusted VF. Command not supported\n");
		return 0;
	}

	HWRM_PREP(&req, HWRM_CFA_PAIR_ALLOC, BNXT_USE_CHIMP_MB);
	req.pair_mode = HWRM_CFA_PAIR_FREE_INPUT_PAIR_MODE_REP2FN_TRUFLOW;
	snprintf(req.pair_name, sizeof(req.pair_name), "%svfr%d",
		 bp->eth_dev->data->name, rep_bp->vf_id);

	req.pf_b_id = rep_bp->parent_pf_idx;
	req.vf_b_id = BNXT_REP_PF(rep_bp) ? rte_cpu_to_le_16(((uint16_t)-1)) :
						rte_cpu_to_le_16(rep_bp->vf_id);
	req.vf_a_id = rte_cpu_to_le_16(bp->fw_fid);
	req.host_b_id = 1; /* TBD - Confirm if this is OK */

	req.enables |= rep_bp->flags & BNXT_REP_Q_R2F_VALID ?
			HWRM_CFA_PAIR_ALLOC_INPUT_ENABLES_Q_AB_VALID : 0;
	req.enables |= rep_bp->flags & BNXT_REP_Q_F2R_VALID ?
			HWRM_CFA_PAIR_ALLOC_INPUT_ENABLES_Q_BA_VALID : 0;
	req.enables |= rep_bp->flags & BNXT_REP_FC_R2F_VALID ?
			HWRM_CFA_PAIR_ALLOC_INPUT_ENABLES_FC_AB_VALID : 0;
	req.enables |= rep_bp->flags & BNXT_REP_FC_F2R_VALID ?
			HWRM_CFA_PAIR_ALLOC_INPUT_ENABLES_FC_BA_VALID : 0;

	req.q_ab = rep_bp->rep_q_r2f;
	req.q_ba = rep_bp->rep_q_f2r;
	req.fc_ab = rep_bp->rep_fc_r2f;
	req.fc_ba = rep_bp->rep_fc_f2r;

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();

	HWRM_UNLOCK();
	PMD_DRV_LOG(DEBUG, "%s %d allocated\n",
		    BNXT_REP_PF(rep_bp) ? "PFR" : "VFR", rep_bp->vf_id);
	return rc;
}

int bnxt_hwrm_cfa_pair_free(struct bnxt *bp, struct bnxt_representor *rep_bp)
{
	struct hwrm_cfa_pair_free_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_cfa_pair_free_input req = {0};
	int rc;

	if (!(BNXT_PF(bp) || BNXT_VF_IS_TRUSTED(bp))) {
		PMD_DRV_LOG(DEBUG,
			    "Not a PF or trusted VF. Command not supported\n");
		return 0;
	}

	HWRM_PREP(&req, HWRM_CFA_PAIR_FREE, BNXT_USE_CHIMP_MB);
	snprintf(req.pair_name, sizeof(req.pair_name), "%svfr%d",
		 bp->eth_dev->data->name, rep_bp->vf_id);
	req.pf_b_id = rep_bp->parent_pf_idx;
	req.pair_mode = HWRM_CFA_PAIR_FREE_INPUT_PAIR_MODE_REP2FN_TRUFLOW;
	req.vf_id = BNXT_REP_PF(rep_bp) ? rte_cpu_to_le_16(((uint16_t)-1)) :
						rte_cpu_to_le_16(rep_bp->vf_id);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();
	PMD_DRV_LOG(DEBUG, "%s %d freed\n", BNXT_REP_PF(rep_bp) ? "PFR" : "VFR",
		    rep_bp->vf_id);
	return rc;
}

int bnxt_hwrm_fw_echo_reply(struct bnxt *bp, uint32_t echo_req_data1,
			    uint32_t echo_req_data2)
{
	struct hwrm_func_echo_response_input req = {0};
	struct hwrm_func_echo_response_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	HWRM_PREP(&req, HWRM_FUNC_ECHO_RESPONSE, BNXT_USE_CHIMP_MB);
	req.event_data1 = rte_cpu_to_le_32(echo_req_data1);
	req.event_data2 = rte_cpu_to_le_32(echo_req_data2);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_poll_ver_get(struct bnxt *bp)
{
	struct hwrm_ver_get_input req = {.req_type = 0 };
	struct hwrm_ver_get_output *resp = bp->hwrm_cmd_resp_addr;
	int rc = 0;

	bp->max_req_len = HWRM_MAX_REQ_LEN;
	bp->max_resp_len = BNXT_PAGE_SIZE;
	bp->hwrm_cmd_timeout = SHORT_HWRM_CMD_TIMEOUT;

	HWRM_PREP(&req, HWRM_VER_GET, BNXT_USE_CHIMP_MB);
	req.hwrm_intf_maj = HWRM_VERSION_MAJOR;
	req.hwrm_intf_min = HWRM_VERSION_MINOR;
	req.hwrm_intf_upd = HWRM_VERSION_UPDATE;

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

	HWRM_CHECK_RESULT_SILENT();

	if (resp->flags & HWRM_VER_GET_OUTPUT_FLAGS_DEV_NOT_RDY)
		rc = -EAGAIN;

	HWRM_UNLOCK();

	return rc;
}

int bnxt_hwrm_read_sfp_module_eeprom_info(struct bnxt *bp, uint16_t i2c_addr,
					  uint16_t page_number, uint16_t start_addr,
					  uint16_t data_length, uint8_t *buf)
{
	struct hwrm_port_phy_i2c_read_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_port_phy_i2c_read_input req = {0};
	uint32_t enables = HWRM_PORT_PHY_I2C_READ_INPUT_ENABLES_PAGE_OFFSET;
	int rc, byte_offset = 0;

	do {
		uint16_t xfer_size;

		HWRM_PREP(&req, HWRM_PORT_PHY_I2C_READ, BNXT_USE_CHIMP_MB);
		req.i2c_slave_addr = i2c_addr;
		req.page_number = rte_cpu_to_le_16(page_number);
		req.port_id = rte_cpu_to_le_16(bp->pf->port_id);

		xfer_size = RTE_MIN(data_length, BNXT_MAX_PHY_I2C_RESP_SIZE);
		req.page_offset = rte_cpu_to_le_16(start_addr + byte_offset);
		req.data_length = xfer_size;
		req.enables = rte_cpu_to_le_32(start_addr + byte_offset ? enables : 0);
		rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
		HWRM_CHECK_RESULT();

		memcpy(buf + byte_offset, resp->data, xfer_size);

		data_length -= xfer_size;
		byte_offset += xfer_size;

		HWRM_UNLOCK();
	} while (data_length > 0);

	return rc;
}

void bnxt_free_hwrm_tx_ring(struct bnxt *bp, int queue_index)
{
	struct bnxt_tx_queue *txq = bp->tx_queues[queue_index];
	struct bnxt_tx_ring_info *txr = txq->tx_ring;
	struct bnxt_ring *ring = txr->tx_ring_struct;
	struct bnxt_cp_ring_info *cpr = txq->cp_ring;

	bnxt_hwrm_ring_free(bp, ring,
			    HWRM_RING_FREE_INPUT_RING_TYPE_TX,
			    cpr->cp_ring_struct->fw_ring_id);
	txr->tx_raw_prod = 0;
	txr->tx_raw_cons = 0;
	memset(txr->tx_desc_ring, 0,
		txr->tx_ring_struct->ring_size * sizeof(*txr->tx_desc_ring));
	memset(txr->tx_buf_ring, 0,
		txr->tx_ring_struct->ring_size * sizeof(*txr->tx_buf_ring));

	bnxt_hwrm_stat_ctx_free(bp, cpr);

	bnxt_free_cp_ring(bp, cpr);
}

int bnxt_hwrm_config_host_mtu(struct bnxt *bp)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	if (!BNXT_PF(bp))
		return 0;

	HWRM_PREP(&req, HWRM_FUNC_CFG, BNXT_USE_CHIMP_MB);

	req.fid = rte_cpu_to_le_16(0xffff);
	req.enables = rte_cpu_to_le_32(HWRM_FUNC_CFG_INPUT_ENABLES_HOST_MTU);
	req.host_mtu = rte_cpu_to_le_16(bp->eth_dev->data->mtu);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);
	HWRM_CHECK_RESULT();
	HWRM_UNLOCK();

	return rc;
}

int
bnxt_vnic_rss_clear_p5(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	struct hwrm_vnic_rss_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_vnic_rss_cfg_input req = {0};
	int nr_ctxs = vnic->num_lb_ctxts;
	int i, rc = 0;

	for (i = 0; i < nr_ctxs; i++) {
		HWRM_PREP(&req, HWRM_VNIC_RSS_CFG, BNXT_USE_CHIMP_MB);

		req.rss_ctx_idx = rte_cpu_to_le_16(vnic->fw_grp_ids[i]);
		req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);

		rc = bnxt_hwrm_send_message(bp, &req, sizeof(req), BNXT_USE_CHIMP_MB);

		HWRM_CHECK_RESULT();
		HWRM_UNLOCK();
	}

	return rc;
}
