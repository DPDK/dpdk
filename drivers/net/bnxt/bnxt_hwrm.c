/*-
 *   BSD LICENSE
 *
 *   Copyright(c) Broadcom Limited.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Broadcom Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_version.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_filter.h"
#include "bnxt_hwrm.h"
#include "bnxt_rxq.h"
#include "bnxt_rxr.h"
#include "bnxt_ring.h"
#include "bnxt_txq.h"
#include "bnxt_txr.h"
#include "bnxt_vnic.h"
#include "hsi_struct_def_dpdk.h"

#include <rte_io.h>

#define HWRM_CMD_TIMEOUT		2000

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
	RTE_LOG(ERR, PMD, "Page size %zu out of range\n", size);
	return sizeof(void *) * 8 - 1;
}

static int page_roundup(size_t size)
{
	return 1 << page_getenum(size);
}

/*
 * HWRM Functions (sent to HWRM)
 * These are named bnxt_hwrm_*() and return -1 if bnxt_hwrm_send_message()
 * fails (ie: a timeout), and a positive non-zero HWRM error code if the HWRM
 * command was failed by the ChiMP.
 */

static int bnxt_hwrm_send_message_locked(struct bnxt *bp, void *msg,
					uint32_t msg_len)
{
	unsigned int i;
	struct input *req = msg;
	struct output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t *data = msg;
	uint8_t *bar;
	uint8_t *valid;
	uint16_t max_req_len = bp->max_req_len;
	struct hwrm_short_input short_input = { 0 };

	if (bp->flags & BNXT_FLAG_SHORT_CMD) {
		void *short_cmd_req = bp->hwrm_short_cmd_req_addr;

		memset(short_cmd_req, 0, bp->max_req_len);
		memcpy(short_cmd_req, req, msg_len);

		short_input.req_type = rte_cpu_to_le_16(req->req_type);
		short_input.signature = rte_cpu_to_le_16(
					HWRM_SHORT_REQ_SIGNATURE_SHORT_CMD);
		short_input.size = rte_cpu_to_le_16(msg_len);
		short_input.req_addr =
			rte_cpu_to_le_64(bp->hwrm_short_cmd_req_dma_addr);

		data = (uint32_t *)&short_input;
		msg_len = sizeof(short_input);

		/* Sync memory write before updating doorbell */
		rte_wmb();

		max_req_len = BNXT_HWRM_SHORT_REQ_LEN;
	}

	/* Write request msg to hwrm channel */
	for (i = 0; i < msg_len; i += 4) {
		bar = (uint8_t *)bp->bar0 + i;
		rte_write32(*data, bar);
		data++;
	}

	/* Zero the rest of the request space */
	for (; i < max_req_len; i += 4) {
		bar = (uint8_t *)bp->bar0 + i;
		rte_write32(0, bar);
	}

	/* Ring channel doorbell */
	bar = (uint8_t *)bp->bar0 + 0x100;
	rte_write32(1, bar);

	/* Poll for the valid bit */
	for (i = 0; i < HWRM_CMD_TIMEOUT; i++) {
		/* Sanity check on the resp->resp_len */
		rte_rmb();
		if (resp->resp_len && resp->resp_len <=
				bp->max_resp_len) {
			/* Last byte of resp contains the valid key */
			valid = (uint8_t *)resp + resp->resp_len - 1;
			if (*valid == HWRM_RESP_VALID_KEY)
				break;
		}
		rte_delay_us(600);
	}

	if (i >= HWRM_CMD_TIMEOUT) {
		RTE_LOG(ERR, PMD, "Error sending msg 0x%04x\n",
			req->req_type);
		goto err_ret;
	}
	return 0;

err_ret:
	return -1;
}

static int bnxt_hwrm_send_message(struct bnxt *bp, void *msg, uint32_t msg_len)
{
	int rc;

	rte_spinlock_lock(&bp->hwrm_lock);
	rc = bnxt_hwrm_send_message_locked(bp, msg, msg_len);
	rte_spinlock_unlock(&bp->hwrm_lock);
	return rc;
}

#define HWRM_PREP(req, type, cr, resp) \
	memset(bp->hwrm_cmd_resp_addr, 0, bp->max_resp_len); \
	req.req_type = rte_cpu_to_le_16(HWRM_##type); \
	req.cmpl_ring = rte_cpu_to_le_16(cr); \
	req.seq_id = rte_cpu_to_le_16(bp->hwrm_cmd_seq++); \
	req.target_id = rte_cpu_to_le_16(0xffff); \
	req.resp_addr = rte_cpu_to_le_64(bp->hwrm_cmd_resp_dma_addr)

#define HWRM_CHECK_RESULT \
	{ \
		if (rc) { \
			RTE_LOG(ERR, PMD, "%s failed rc:%d\n", \
				__func__, rc); \
			return rc; \
		} \
		if (resp->error_code) { \
			rc = rte_le_to_cpu_16(resp->error_code); \
			if (resp->resp_len >= 16) { \
				struct hwrm_err_output *tmp_hwrm_err_op = \
							(void *)resp; \
				RTE_LOG(ERR, PMD, \
					"%s error %d:%d:%08x:%04x\n", \
					__func__, \
					rc, tmp_hwrm_err_op->cmd_err, \
					rte_le_to_cpu_32(\
						tmp_hwrm_err_op->opaque_0), \
					rte_le_to_cpu_16(\
						tmp_hwrm_err_op->opaque_1)); \
			} \
			else { \
				RTE_LOG(ERR, PMD, \
					"%s error %d\n", __func__, rc); \
			} \
			return rc; \
		} \
	}

int bnxt_hwrm_cfa_l2_clear_rx_mask(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_cfa_l2_set_rx_mask_input req = {.req_type = 0 };
	struct hwrm_cfa_l2_set_rx_mask_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, CFA_L2_SET_RX_MASK, -1, resp);
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);
	req.mask = 0;

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

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

	HWRM_PREP(req, CFA_L2_SET_RX_MASK, -1, resp);
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);

	/* FIXME add multicast flag, when multicast adding options is supported
	 * by ethtool.
	 */
	if (vnic->flags & BNXT_VNIC_INFO_BCAST)
		mask = HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST;
	if (vnic->flags & BNXT_VNIC_INFO_UNTAGGED)
		mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLAN_NONVLAN;
	if (vnic->flags & BNXT_VNIC_INFO_PROMISC)
		mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS;
	if (vnic->flags & BNXT_VNIC_INFO_ALLMULTI)
		mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST;
	if (vnic->flags & BNXT_VNIC_INFO_MCAST)
		mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_MCAST;
	if (vnic->mc_addr_cnt) {
		mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_MCAST;
		req.num_mc_entries = rte_cpu_to_le_32(vnic->mc_addr_cnt);
		req.mc_tbl_addr = rte_cpu_to_le_64(vnic->mc_list_dma_addr);
	}
	if (vlan_table) {
		if (!(mask & HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLAN_NONVLAN))
			mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_VLANONLY;
		req.vlan_tag_tbl_addr = rte_cpu_to_le_16(
			 rte_mem_virt2phy(vlan_table));
		req.num_vlan_tags = rte_cpu_to_le_32((uint32_t)vlan_count);
	}
	req.mask = rte_cpu_to_le_32(HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST |
				    mask);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

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
	HWRM_PREP(req, CFA_VLAN_ANTISPOOF_CFG, -1, resp);
	req.fid = rte_cpu_to_le_16(fid);

	req.vlan_tag_mask_tbl_addr =
		rte_cpu_to_le_64(rte_mem_virt2phy(vlan_table));
	req.num_vlan_entries = rte_cpu_to_le_32((uint32_t)vlan_count);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_clear_filter(struct bnxt *bp,
			   struct bnxt_filter_info *filter)
{
	int rc = 0;
	struct hwrm_cfa_l2_filter_free_input req = {.req_type = 0 };
	struct hwrm_cfa_l2_filter_free_output *resp = bp->hwrm_cmd_resp_addr;

	if (filter->fw_l2_filter_id == UINT64_MAX)
		return 0;

	HWRM_PREP(req, CFA_L2_FILTER_FREE, -1, resp);

	req.l2_filter_id = rte_cpu_to_le_64(filter->fw_l2_filter_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	filter->fw_l2_filter_id = -1;

	return 0;
}

int bnxt_hwrm_set_filter(struct bnxt *bp,
			 uint16_t dst_id,
			 struct bnxt_filter_info *filter)
{
	int rc = 0;
	struct hwrm_cfa_l2_filter_alloc_input req = {.req_type = 0 };
	struct hwrm_cfa_l2_filter_alloc_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t enables = 0;

	if (filter->fw_l2_filter_id != UINT64_MAX)
		bnxt_hwrm_clear_filter(bp, filter);

	HWRM_PREP(req, CFA_L2_FILTER_ALLOC, -1, resp);

	req.flags = rte_cpu_to_le_32(filter->flags);

	enables = filter->enables |
	      HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_DST_ID;
	req.dst_id = rte_cpu_to_le_16(dst_id);

	if (enables &
	    HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR)
		memcpy(req.l2_addr, filter->l2_addr,
		       ETHER_ADDR_LEN);
	if (enables &
	    HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK)
		memcpy(req.l2_addr_mask, filter->l2_addr_mask,
		       ETHER_ADDR_LEN);
	if (enables &
	    HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN)
		req.l2_ovlan = filter->l2_ovlan;
	if (enables &
	    HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN_MASK)
		req.l2_ovlan_mask = filter->l2_ovlan_mask;
	if (enables & HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_SRC_ID)
		req.src_id = rte_cpu_to_le_32(filter->src_id);
	if (enables & HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_SRC_TYPE)
		req.src_type = filter->src_type;

	req.enables = rte_cpu_to_le_32(enables);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	filter->fw_l2_filter_id = rte_le_to_cpu_64(resp->l2_filter_id);

	return rc;
}

int bnxt_hwrm_func_qcaps(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_func_qcaps_input req = {.req_type = 0 };
	struct hwrm_func_qcaps_output *resp = bp->hwrm_cmd_resp_addr;
	uint16_t new_max_vfs;
	int i;

	HWRM_PREP(req, FUNC_QCAPS, -1, resp);

	req.fid = rte_cpu_to_le_16(0xffff);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	bp->max_ring_grps = rte_le_to_cpu_32(resp->max_hw_ring_grps);
	if (BNXT_PF(bp)) {
		bp->pf.port_id = resp->port_id;
		bp->pf.first_vf_id = rte_le_to_cpu_16(resp->first_vf_id);
		new_max_vfs = bp->pdev->max_vfs;
		if (new_max_vfs != bp->pf.max_vfs) {
			if (bp->pf.vf_info)
				rte_free(bp->pf.vf_info);
			bp->pf.vf_info = rte_malloc("bnxt_vf_info",
			    sizeof(bp->pf.vf_info[0]) * new_max_vfs, 0);
			bp->pf.max_vfs = new_max_vfs;
			for (i = 0; i < new_max_vfs; i++) {
				bp->pf.vf_info[i].fid = bp->pf.first_vf_id + i;
				bp->pf.vf_info[i].vlan_table =
					rte_zmalloc("VF VLAN table",
						    getpagesize(),
						    getpagesize());
				if (bp->pf.vf_info[i].vlan_table == NULL)
					RTE_LOG(ERR, PMD,
					"Fail to alloc VLAN table for VF %d\n",
					i);
				else
					rte_mem_lock_page(
						bp->pf.vf_info[i].vlan_table);
				bp->pf.vf_info[i].vlan_as_table =
					rte_zmalloc("VF VLAN AS table",
						    getpagesize(),
						    getpagesize());
				if (bp->pf.vf_info[i].vlan_as_table == NULL)
					RTE_LOG(ERR, PMD,
					"Alloc VLAN AS table for VF %d fail\n",
					i);
				else
					rte_mem_lock_page(
					       bp->pf.vf_info[i].vlan_as_table);
				STAILQ_INIT(&bp->pf.vf_info[i].filter);
			}
		}
	}

	bp->fw_fid = rte_le_to_cpu_32(resp->fid);
	memcpy(bp->dflt_mac_addr, &resp->mac_address, ETHER_ADDR_LEN);
	bp->max_rsscos_ctx = rte_le_to_cpu_16(resp->max_rsscos_ctx);
	bp->max_cp_rings = rte_le_to_cpu_16(resp->max_cmpl_rings);
	bp->max_tx_rings = rte_le_to_cpu_16(resp->max_tx_rings);
	bp->max_rx_rings = rte_le_to_cpu_16(resp->max_rx_rings);
	bp->max_l2_ctx = rte_le_to_cpu_16(resp->max_l2_ctxs);
	/* TODO: For now, do not support VMDq/RFS on VFs. */
	if (BNXT_PF(bp)) {
		if (bp->pf.max_vfs)
			bp->max_vnics = 1;
		else
			bp->max_vnics = rte_le_to_cpu_16(resp->max_vnics);
	} else {
		bp->max_vnics = 1;
	}
	bp->max_stat_ctx = rte_le_to_cpu_16(resp->max_stat_ctx);
	if (BNXT_PF(bp))
		bp->pf.total_vnics = rte_le_to_cpu_16(resp->max_vnics);

	return rc;
}

int bnxt_hwrm_func_reset(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_func_reset_input req = {.req_type = 0 };
	struct hwrm_func_reset_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, FUNC_RESET, -1, resp);

	req.enables = rte_cpu_to_le_32(0);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_func_driver_register(struct bnxt *bp)
{
	int rc;
	struct hwrm_func_drv_rgtr_input req = {.req_type = 0 };
	struct hwrm_func_drv_rgtr_output *resp = bp->hwrm_cmd_resp_addr;

	if (bp->flags & BNXT_FLAG_REGISTERED)
		return 0;

	HWRM_PREP(req, FUNC_DRV_RGTR, -1, resp);
	req.enables = rte_cpu_to_le_32(HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VER |
			HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_ASYNC_EVENT_FWD);
	req.ver_maj = RTE_VER_YEAR;
	req.ver_min = RTE_VER_MONTH;
	req.ver_upd = RTE_VER_MINOR;

	if (BNXT_PF(bp)) {
		req.enables |= rte_cpu_to_le_32(
			HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VF_INPUT_FWD);
		memcpy(req.vf_req_fwd, bp->pf.vf_req_fwd,
		       RTE_MIN(sizeof(req.vf_req_fwd),
			       sizeof(bp->pf.vf_req_fwd)));
	}

	req.async_event_fwd[0] |= rte_cpu_to_le_32(0x1);   /* TODO: Use MACRO */
	memset(req.async_event_fwd, 0xff, sizeof(req.async_event_fwd));

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	bp->flags |= BNXT_FLAG_REGISTERED;

	return rc;
}

int bnxt_hwrm_ver_get(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_ver_get_input req = {.req_type = 0 };
	struct hwrm_ver_get_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t my_version;
	uint32_t fw_version;
	uint16_t max_resp_len;
	char type[RTE_MEMZONE_NAMESIZE];
	uint32_t dev_caps_cfg;

	bp->max_req_len = HWRM_MAX_REQ_LEN;
	HWRM_PREP(req, VER_GET, -1, resp);

	req.hwrm_intf_maj = HWRM_VERSION_MAJOR;
	req.hwrm_intf_min = HWRM_VERSION_MINOR;
	req.hwrm_intf_upd = HWRM_VERSION_UPDATE;

	/*
	 * Hold the lock since we may be adjusting the response pointers.
	 */
	rte_spinlock_lock(&bp->hwrm_lock);
	rc = bnxt_hwrm_send_message_locked(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	RTE_LOG(INFO, PMD, "%d.%d.%d:%d.%d.%d\n",
		resp->hwrm_intf_maj, resp->hwrm_intf_min,
		resp->hwrm_intf_upd,
		resp->hwrm_fw_maj, resp->hwrm_fw_min, resp->hwrm_fw_bld);
	bp->fw_ver = (resp->hwrm_fw_maj << 24) | (resp->hwrm_fw_min << 16) |
			(resp->hwrm_fw_bld << 8) | resp->hwrm_fw_rsvd;
	RTE_LOG(INFO, PMD, "Driver HWRM version: %d.%d.%d\n",
		HWRM_VERSION_MAJOR, HWRM_VERSION_MINOR, HWRM_VERSION_UPDATE);

	my_version = HWRM_VERSION_MAJOR << 16;
	my_version |= HWRM_VERSION_MINOR << 8;
	my_version |= HWRM_VERSION_UPDATE;

	fw_version = resp->hwrm_intf_maj << 16;
	fw_version |= resp->hwrm_intf_min << 8;
	fw_version |= resp->hwrm_intf_upd;

	if (resp->hwrm_intf_maj != HWRM_VERSION_MAJOR) {
		RTE_LOG(ERR, PMD, "Unsupported firmware API version\n");
		rc = -EINVAL;
		goto error;
	}

	if (my_version != fw_version) {
		RTE_LOG(INFO, PMD, "BNXT Driver/HWRM API mismatch.\n");
		if (my_version < fw_version) {
			RTE_LOG(INFO, PMD,
				"Firmware API version is newer than driver.\n");
			RTE_LOG(INFO, PMD,
				"The driver may be missing features.\n");
		} else {
			RTE_LOG(INFO, PMD,
				"Firmware API version is older than driver.\n");
			RTE_LOG(INFO, PMD,
				"Not all driver features may be functional.\n");
		}
	}

	if (bp->max_req_len > resp->max_req_win_len) {
		RTE_LOG(ERR, PMD, "Unsupported request length\n");
		rc = -EINVAL;
	}
	bp->max_req_len = rte_le_to_cpu_16(resp->max_req_win_len);
	max_resp_len = resp->max_resp_len;
	dev_caps_cfg = rte_le_to_cpu_32(resp->dev_caps_cfg);

	if (bp->max_resp_len != max_resp_len) {
		sprintf(type, "bnxt_hwrm_%04x:%02x:%02x:%02x",
			bp->pdev->addr.domain, bp->pdev->addr.bus,
			bp->pdev->addr.devid, bp->pdev->addr.function);

		rte_free(bp->hwrm_cmd_resp_addr);

		bp->hwrm_cmd_resp_addr = rte_malloc(type, max_resp_len, 0);
		if (bp->hwrm_cmd_resp_addr == NULL) {
			rc = -ENOMEM;
			goto error;
		}
		rte_mem_lock_page(bp->hwrm_cmd_resp_addr);
		bp->hwrm_cmd_resp_dma_addr =
			rte_mem_virt2phy(bp->hwrm_cmd_resp_addr);
		if (bp->hwrm_cmd_resp_dma_addr == 0) {
			RTE_LOG(ERR, PMD,
			"Unable to map response buffer to physical memory.\n");
			rc = -ENOMEM;
			goto error;
		}
		bp->max_resp_len = max_resp_len;
	}

	if ((dev_caps_cfg &
		HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_SUPPORTED) &&
	    (dev_caps_cfg &
	     HWRM_VER_GET_OUTPUT_DEV_CAPS_CFG_SHORT_CMD_INPUTUIRED)) {
		RTE_LOG(DEBUG, PMD, "Short command supported\n");

		rte_free(bp->hwrm_short_cmd_req_addr);

		bp->hwrm_short_cmd_req_addr = rte_malloc(type,
							bp->max_req_len, 0);
		if (bp->hwrm_short_cmd_req_addr == NULL) {
			rc = -ENOMEM;
			goto error;
		}
		rte_mem_lock_page(bp->hwrm_short_cmd_req_addr);
		bp->hwrm_short_cmd_req_dma_addr =
			rte_mem_virt2phy(bp->hwrm_short_cmd_req_addr);
		if (bp->hwrm_short_cmd_req_dma_addr == 0) {
			rte_free(bp->hwrm_short_cmd_req_addr);
			RTE_LOG(ERR, PMD,
				"Unable to map buffer to physical memory.\n");
			rc = -ENOMEM;
			goto error;
		}

		bp->flags |= BNXT_FLAG_SHORT_CMD;
	}

error:
	rte_spinlock_unlock(&bp->hwrm_lock);
	return rc;
}

int bnxt_hwrm_func_driver_unregister(struct bnxt *bp, uint32_t flags)
{
	int rc;
	struct hwrm_func_drv_unrgtr_input req = {.req_type = 0 };
	struct hwrm_func_drv_unrgtr_output *resp = bp->hwrm_cmd_resp_addr;

	if (!(bp->flags & BNXT_FLAG_REGISTERED))
		return 0;

	HWRM_PREP(req, FUNC_DRV_UNRGTR, -1, resp);
	req.flags = flags;

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	bp->flags &= ~BNXT_FLAG_REGISTERED;

	return rc;
}

static int bnxt_hwrm_port_phy_cfg(struct bnxt *bp, struct bnxt_link_info *conf)
{
	int rc = 0;
	struct hwrm_port_phy_cfg_input req = {0};
	struct hwrm_port_phy_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t enables = 0;
	uint32_t link_speed_mask =
		HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED_MASK;

	HWRM_PREP(req, PORT_PHY_CFG, -1, resp);

	if (conf->link_up) {
		req.flags = rte_cpu_to_le_32(conf->phy_flags);
		req.force_link_speed = rte_cpu_to_le_16(conf->link_speed);
		/*
		 * Note, ChiMP FW 20.2.1 and 20.2.2 return an error when we set
		 * any auto mode, even "none".
		 */
		if (!conf->link_speed) {
			req.auto_mode = conf->auto_mode;
			enables |= HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_MODE;
			if (conf->auto_mode ==
			    HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_SPEED_MASK) {
				req.auto_link_speed_mask =
					conf->auto_link_speed_mask;
				enables |= link_speed_mask;
			}
			if (bp->link_info.auto_link_speed) {
				req.auto_link_speed =
					bp->link_info.auto_link_speed;
				enables |=
				HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED;
			}
		}
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
		RTE_LOG(INFO, PMD, "Force Link Down\n");
	}

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

static int bnxt_hwrm_port_phy_qcfg(struct bnxt *bp,
				   struct bnxt_link_info *link_info)
{
	int rc = 0;
	struct hwrm_port_phy_qcfg_input req = {0};
	struct hwrm_port_phy_qcfg_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, PORT_PHY_QCFG, -1, resp);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	link_info->phy_link_status = resp->link;
	link_info->link_up =
		(link_info->phy_link_status ==
		 HWRM_PORT_PHY_QCFG_OUTPUT_LINK_LINK) ? 1 : 0;
	link_info->link_speed = rte_le_to_cpu_16(resp->link_speed);
	link_info->duplex = resp->duplex;
	link_info->pause = resp->pause;
	link_info->auto_pause = resp->auto_pause;
	link_info->force_pause = resp->force_pause;
	link_info->auto_mode = resp->auto_mode;

	link_info->support_speeds = rte_le_to_cpu_16(resp->support_speeds);
	link_info->auto_link_speed = rte_le_to_cpu_16(resp->auto_link_speed);
	link_info->preemphasis = rte_le_to_cpu_32(resp->preemphasis);
	link_info->phy_ver[0] = resp->phy_maj;
	link_info->phy_ver[1] = resp->phy_min;
	link_info->phy_ver[2] = resp->phy_bld;

	return rc;
}

int bnxt_hwrm_queue_qportcfg(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_queue_qportcfg_input req = {.req_type = 0 };
	struct hwrm_queue_qportcfg_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, QUEUE_QPORTCFG, -1, resp);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

#define GET_QUEUE_INFO(x) \
	bp->cos_queue[x].id = resp->queue_id##x; \
	bp->cos_queue[x].profile = resp->queue_id##x##_service_profile

	GET_QUEUE_INFO(0);
	GET_QUEUE_INFO(1);
	GET_QUEUE_INFO(2);
	GET_QUEUE_INFO(3);
	GET_QUEUE_INFO(4);
	GET_QUEUE_INFO(5);
	GET_QUEUE_INFO(6);
	GET_QUEUE_INFO(7);

	return rc;
}

int bnxt_hwrm_ring_alloc(struct bnxt *bp,
			 struct bnxt_ring *ring,
			 uint32_t ring_type, uint32_t map_index,
			 uint32_t stats_ctx_id, uint32_t cmpl_ring_id)
{
	int rc = 0;
	uint32_t enables = 0;
	struct hwrm_ring_alloc_input req = {.req_type = 0 };
	struct hwrm_ring_alloc_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, RING_ALLOC, -1, resp);

	req.page_tbl_addr = rte_cpu_to_le_64(ring->bd_dma);
	req.fbo = rte_cpu_to_le_32(0);
	/* Association of ring index with doorbell index */
	req.logical_id = rte_cpu_to_le_16(map_index);
	req.length = rte_cpu_to_le_32(ring->ring_size);

	switch (ring_type) {
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_TX:
		req.queue_id = bp->cos_queue[0].id;
		/* FALLTHROUGH */
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX:
		req.ring_type = ring_type;
		req.cmpl_ring_id = rte_cpu_to_le_16(cmpl_ring_id);
		req.stat_ctx_id = rte_cpu_to_le_16(stats_ctx_id);
		if (stats_ctx_id != INVALID_STATS_CTX_ID)
			enables |=
			HWRM_RING_ALLOC_INPUT_ENABLES_STAT_CTX_ID_VALID;
		break;
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL:
		req.ring_type = ring_type;
		/*
		 * TODO: Some HWRM versions crash with
		 * HWRM_RING_ALLOC_INPUT_INT_MODE_POLL
		 */
		req.int_mode = HWRM_RING_ALLOC_INPUT_INT_MODE_MSIX;
		break;
	default:
		RTE_LOG(ERR, PMD, "hwrm alloc invalid ring type %d\n",
			ring_type);
		return -1;
	}
	req.enables = rte_cpu_to_le_32(enables);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	if (rc || resp->error_code) {
		if (rc == 0 && resp->error_code)
			rc = rte_le_to_cpu_16(resp->error_code);
		switch (ring_type) {
		case HWRM_RING_FREE_INPUT_RING_TYPE_L2_CMPL:
			RTE_LOG(ERR, PMD,
				"hwrm_ring_alloc cp failed. rc:%d\n", rc);
			return rc;
		case HWRM_RING_FREE_INPUT_RING_TYPE_RX:
			RTE_LOG(ERR, PMD,
				"hwrm_ring_alloc rx failed. rc:%d\n", rc);
			return rc;
		case HWRM_RING_FREE_INPUT_RING_TYPE_TX:
			RTE_LOG(ERR, PMD,
				"hwrm_ring_alloc tx failed. rc:%d\n", rc);
			return rc;
		default:
			RTE_LOG(ERR, PMD, "Invalid ring. rc:%d\n", rc);
			return rc;
		}
	}

	ring->fw_ring_id = rte_le_to_cpu_16(resp->ring_id);
	return rc;
}

int bnxt_hwrm_ring_free(struct bnxt *bp,
			struct bnxt_ring *ring, uint32_t ring_type)
{
	int rc;
	struct hwrm_ring_free_input req = {.req_type = 0 };
	struct hwrm_ring_free_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, RING_FREE, -1, resp);

	req.ring_type = ring_type;
	req.ring_id = rte_cpu_to_le_16(ring->fw_ring_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	if (rc || resp->error_code) {
		if (rc == 0 && resp->error_code)
			rc = rte_le_to_cpu_16(resp->error_code);

		switch (ring_type) {
		case HWRM_RING_FREE_INPUT_RING_TYPE_L2_CMPL:
			RTE_LOG(ERR, PMD, "hwrm_ring_free cp failed. rc:%d\n",
				rc);
			return rc;
		case HWRM_RING_FREE_INPUT_RING_TYPE_RX:
			RTE_LOG(ERR, PMD, "hwrm_ring_free rx failed. rc:%d\n",
				rc);
			return rc;
		case HWRM_RING_FREE_INPUT_RING_TYPE_TX:
			RTE_LOG(ERR, PMD, "hwrm_ring_free tx failed. rc:%d\n",
				rc);
			return rc;
		default:
			RTE_LOG(ERR, PMD, "Invalid ring, rc:%d\n", rc);
			return rc;
		}
	}
	return 0;
}

int bnxt_hwrm_ring_grp_alloc(struct bnxt *bp, unsigned int idx)
{
	int rc = 0;
	struct hwrm_ring_grp_alloc_input req = {.req_type = 0 };
	struct hwrm_ring_grp_alloc_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, RING_GRP_ALLOC, -1, resp);

	req.cr = rte_cpu_to_le_16(bp->grp_info[idx].cp_fw_ring_id);
	req.rr = rte_cpu_to_le_16(bp->grp_info[idx].rx_fw_ring_id);
	req.ar = rte_cpu_to_le_16(bp->grp_info[idx].ag_fw_ring_id);
	req.sc = rte_cpu_to_le_16(bp->grp_info[idx].fw_stats_ctx);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	bp->grp_info[idx].fw_grp_id =
	    rte_le_to_cpu_16(resp->ring_group_id);

	return rc;
}

int bnxt_hwrm_ring_grp_free(struct bnxt *bp, unsigned int idx)
{
	int rc;
	struct hwrm_ring_grp_free_input req = {.req_type = 0 };
	struct hwrm_ring_grp_free_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, RING_GRP_FREE, -1, resp);

	req.ring_group_id = rte_cpu_to_le_16(bp->grp_info[idx].fw_grp_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	bp->grp_info[idx].fw_grp_id = INVALID_HW_RING_ID;
	return rc;
}

int bnxt_hwrm_stat_clear(struct bnxt *bp, struct bnxt_cp_ring_info *cpr)
{
	int rc = 0;
	struct hwrm_stat_ctx_clr_stats_input req = {.req_type = 0 };
	struct hwrm_stat_ctx_clr_stats_output *resp = bp->hwrm_cmd_resp_addr;

	if (cpr->hw_stats_ctx_id == (uint32_t)HWRM_NA_SIGNATURE)
		return rc;

	HWRM_PREP(req, STAT_CTX_CLR_STATS, -1, resp);

	req.stat_ctx_id = rte_cpu_to_le_16(cpr->hw_stats_ctx_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_stat_ctx_alloc(struct bnxt *bp, struct bnxt_cp_ring_info *cpr,
				unsigned int idx __rte_unused)
{
	int rc;
	struct hwrm_stat_ctx_alloc_input req = {.req_type = 0 };
	struct hwrm_stat_ctx_alloc_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, STAT_CTX_ALLOC, -1, resp);

	req.update_period_ms = rte_cpu_to_le_32(0);

	req.stats_dma_addr =
	    rte_cpu_to_le_64(cpr->hw_stats_map);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	cpr->hw_stats_ctx_id = rte_le_to_cpu_16(resp->stat_ctx_id);

	return rc;
}

int bnxt_hwrm_stat_ctx_free(struct bnxt *bp, struct bnxt_cp_ring_info *cpr,
				unsigned int idx __rte_unused)
{
	int rc;
	struct hwrm_stat_ctx_free_input req = {.req_type = 0 };
	struct hwrm_stat_ctx_free_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, STAT_CTX_FREE, -1, resp);

	req.stat_ctx_id = rte_cpu_to_le_16(cpr->hw_stats_ctx_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_vnic_alloc(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0, i, j;
	struct hwrm_vnic_alloc_input req = { 0 };
	struct hwrm_vnic_alloc_output *resp = bp->hwrm_cmd_resp_addr;

	/* map ring groups to this vnic */
	RTE_LOG(DEBUG, PMD, "Alloc VNIC. Start %x, End %x\n",
		vnic->start_grp_id, vnic->end_grp_id);
	for (i = vnic->start_grp_id, j = 0; i <= vnic->end_grp_id; i++, j++)
		vnic->fw_grp_ids[j] = bp->grp_info[i].fw_grp_id;
	vnic->dflt_ring_grp = bp->grp_info[vnic->start_grp_id].fw_grp_id;
	vnic->rss_rule = (uint16_t)HWRM_NA_SIGNATURE;
	vnic->cos_rule = (uint16_t)HWRM_NA_SIGNATURE;
	vnic->lb_rule = (uint16_t)HWRM_NA_SIGNATURE;
	vnic->mru = bp->eth_dev->data->mtu + ETHER_HDR_LEN +
				ETHER_CRC_LEN + VLAN_TAG_SIZE;
	HWRM_PREP(req, VNIC_ALLOC, -1, resp);

	if (vnic->func_default)
		req.flags = HWRM_VNIC_ALLOC_INPUT_FLAGS_DEFAULT;
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	vnic->fw_vnic_id = rte_le_to_cpu_16(resp->vnic_id);
	RTE_LOG(DEBUG, PMD, "VNIC ID %x\n", vnic->fw_vnic_id);
	return rc;
}

static int bnxt_hwrm_vnic_plcmodes_qcfg(struct bnxt *bp,
					struct bnxt_vnic_info *vnic,
					struct bnxt_plcmodes_cfg *pmode)
{
	int rc = 0;
	struct hwrm_vnic_plcmodes_qcfg_input req = {.req_type = 0 };
	struct hwrm_vnic_plcmodes_qcfg_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, VNIC_PLCMODES_QCFG, -1, resp);

	req.vnic_id = rte_cpu_to_le_32(vnic->fw_vnic_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	pmode->flags = rte_le_to_cpu_32(resp->flags);
	/* dflt_vnic bit doesn't exist in the _cfg command */
	pmode->flags &= ~(HWRM_VNIC_PLCMODES_QCFG_OUTPUT_FLAGS_DFLT_VNIC);
	pmode->jumbo_thresh = rte_le_to_cpu_16(resp->jumbo_thresh);
	pmode->hds_offset = rte_le_to_cpu_16(resp->hds_offset);
	pmode->hds_threshold = rte_le_to_cpu_16(resp->hds_threshold);

	return rc;
}

static int bnxt_hwrm_vnic_plcmodes_cfg(struct bnxt *bp,
				       struct bnxt_vnic_info *vnic,
				       struct bnxt_plcmodes_cfg *pmode)
{
	int rc = 0;
	struct hwrm_vnic_plcmodes_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_plcmodes_cfg_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, VNIC_PLCMODES_CFG, -1, resp);

	req.vnic_id = rte_cpu_to_le_32(vnic->fw_vnic_id);
	req.flags = rte_cpu_to_le_32(pmode->flags);
	req.jumbo_thresh = rte_cpu_to_le_16(pmode->jumbo_thresh);
	req.hds_offset = rte_cpu_to_le_16(pmode->hds_offset);
	req.hds_threshold = rte_cpu_to_le_16(pmode->hds_threshold);
	req.enables = rte_cpu_to_le_32(
	    HWRM_VNIC_PLCMODES_CFG_INPUT_ENABLES_HDS_THRESHOLD_VALID |
	    HWRM_VNIC_PLCMODES_CFG_INPUT_ENABLES_HDS_OFFSET_VALID |
	    HWRM_VNIC_PLCMODES_CFG_INPUT_ENABLES_JUMBO_THRESH_VALID
	);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_vnic_cfg(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t ctx_enable_flag = HWRM_VNIC_CFG_INPUT_ENABLES_RSS_RULE;
	struct bnxt_plcmodes_cfg pmodes;

	if (vnic->fw_vnic_id == INVALID_HW_RING_ID) {
		RTE_LOG(DEBUG, PMD, "VNIC ID %x\n", vnic->fw_vnic_id);
		return rc;
	}

	rc = bnxt_hwrm_vnic_plcmodes_qcfg(bp, vnic, &pmodes);
	if (rc)
		return rc;

	HWRM_PREP(req, VNIC_CFG, -1, resp);

	/* Only RSS support for now TBD: COS & LB */
	req.enables =
	    rte_cpu_to_le_32(HWRM_VNIC_CFG_INPUT_ENABLES_DFLT_RING_GRP |
			     HWRM_VNIC_CFG_INPUT_ENABLES_MRU);
	if (vnic->lb_rule != 0xffff)
		ctx_enable_flag = HWRM_VNIC_CFG_INPUT_ENABLES_LB_RULE;
	if (vnic->cos_rule != 0xffff)
		ctx_enable_flag = HWRM_VNIC_CFG_INPUT_ENABLES_COS_RULE;
	if (vnic->rss_rule != 0xffff)
		ctx_enable_flag = HWRM_VNIC_CFG_INPUT_ENABLES_RSS_RULE;
	req.enables |= rte_cpu_to_le_32(ctx_enable_flag);
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);
	req.dflt_ring_grp = rte_cpu_to_le_16(vnic->dflt_ring_grp);
	req.rss_rule = rte_cpu_to_le_16(vnic->rss_rule);
	req.cos_rule = rte_cpu_to_le_16(vnic->cos_rule);
	req.lb_rule = rte_cpu_to_le_16(vnic->lb_rule);
	req.mru = rte_cpu_to_le_16(vnic->mru);
	if (vnic->func_default)
		req.flags |=
		    rte_cpu_to_le_32(HWRM_VNIC_CFG_INPUT_FLAGS_DEFAULT);
	if (vnic->vlan_strip)
		req.flags |=
		    rte_cpu_to_le_32(HWRM_VNIC_CFG_INPUT_FLAGS_VLAN_STRIP_MODE);
	if (vnic->bd_stall)
		req.flags |=
		    rte_cpu_to_le_32(HWRM_VNIC_CFG_INPUT_FLAGS_BD_STALL_MODE);
	if (vnic->roce_dual)
		req.flags |= rte_cpu_to_le_32(
			HWRM_VNIC_QCFG_OUTPUT_FLAGS_ROCE_DUAL_VNIC_MODE);
	if (vnic->roce_only)
		req.flags |= rte_cpu_to_le_32(
			HWRM_VNIC_QCFG_OUTPUT_FLAGS_ROCE_ONLY_VNIC_MODE);
	if (vnic->rss_dflt_cr)
		req.flags |= rte_cpu_to_le_32(
			HWRM_VNIC_QCFG_OUTPUT_FLAGS_RSS_DFLT_CR_MODE);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

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
		RTE_LOG(DEBUG, PMD, "VNIC QCFG ID %d\n", vnic->fw_vnic_id);
		return rc;
	}
	HWRM_PREP(req, VNIC_QCFG, -1, resp);

	req.enables =
		rte_cpu_to_le_32(HWRM_VNIC_QCFG_INPUT_ENABLES_VF_ID_VALID);
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);
	req.vf_id = rte_cpu_to_le_16(fw_vf_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

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
	vnic->roce_dual = rte_le_to_cpu_32(resp->flags) &
			HWRM_VNIC_QCFG_OUTPUT_FLAGS_ROCE_DUAL_VNIC_MODE;
	vnic->roce_only = rte_le_to_cpu_32(resp->flags) &
			HWRM_VNIC_QCFG_OUTPUT_FLAGS_ROCE_ONLY_VNIC_MODE;
	vnic->rss_dflt_cr = rte_le_to_cpu_32(resp->flags) &
			HWRM_VNIC_QCFG_OUTPUT_FLAGS_RSS_DFLT_CR_MODE;

	return rc;
}

int bnxt_hwrm_vnic_ctx_alloc(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_rss_cos_lb_ctx_alloc_input req = {.req_type = 0 };
	struct hwrm_vnic_rss_cos_lb_ctx_alloc_output *resp =
						bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, VNIC_RSS_COS_LB_CTX_ALLOC, -1, resp);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	vnic->rss_rule = rte_le_to_cpu_16(resp->rss_cos_lb_ctx_id);
	RTE_LOG(DEBUG, PMD, "VNIC RSS Rule %x\n", vnic->rss_rule);

	return rc;
}

int bnxt_hwrm_vnic_ctx_free(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_rss_cos_lb_ctx_free_input req = {.req_type = 0 };
	struct hwrm_vnic_rss_cos_lb_ctx_free_output *resp =
						bp->hwrm_cmd_resp_addr;

	if (vnic->rss_rule == 0xffff) {
		RTE_LOG(DEBUG, PMD, "VNIC RSS Rule %x\n", vnic->rss_rule);
		return rc;
	}
	HWRM_PREP(req, VNIC_RSS_COS_LB_CTX_FREE, -1, resp);

	req.rss_cos_lb_ctx_id = rte_cpu_to_le_16(vnic->rss_rule);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	vnic->rss_rule = INVALID_HW_RING_ID;

	return rc;
}

int bnxt_hwrm_vnic_free(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_free_input req = {.req_type = 0 };
	struct hwrm_vnic_free_output *resp = bp->hwrm_cmd_resp_addr;

	if (vnic->fw_vnic_id == INVALID_HW_RING_ID) {
		RTE_LOG(DEBUG, PMD, "VNIC FREE ID %x\n", vnic->fw_vnic_id);
		return rc;
	}

	HWRM_PREP(req, VNIC_FREE, -1, resp);

	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	vnic->fw_vnic_id = INVALID_HW_RING_ID;
	return rc;
}

int bnxt_hwrm_vnic_rss_cfg(struct bnxt *bp,
			   struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_rss_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_rss_cfg_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, VNIC_RSS_CFG, -1, resp);

	req.hash_type = rte_cpu_to_le_32(vnic->hash_type);

	req.ring_grp_tbl_addr =
	    rte_cpu_to_le_64(vnic->rss_table_dma_addr);
	req.hash_key_tbl_addr =
	    rte_cpu_to_le_64(vnic->rss_hash_key_dma_addr);
	req.rss_ctx_idx = rte_cpu_to_le_16(vnic->rss_rule);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_vnic_plcmode_cfg(struct bnxt *bp,
			struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_plcmodes_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_plcmodes_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint16_t size;

	HWRM_PREP(req, VNIC_PLCMODES_CFG, -1, resp);

	req.flags = rte_cpu_to_le_32(
			HWRM_VNIC_PLCMODES_CFG_INPUT_FLAGS_JUMBO_PLACEMENT);

	req.enables = rte_cpu_to_le_32(
		HWRM_VNIC_PLCMODES_CFG_INPUT_ENABLES_JUMBO_THRESH_VALID);

	size = rte_pktmbuf_data_room_size(bp->rx_queues[0]->mb_pool);
	size -= RTE_PKTMBUF_HEADROOM;

	req.jumbo_thresh = rte_cpu_to_le_16(size);
	req.vnic_id = rte_cpu_to_le_32(vnic->fw_vnic_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_vnic_tpa_cfg(struct bnxt *bp,
			struct bnxt_vnic_info *vnic, bool enable)
{
	int rc = 0;
	struct hwrm_vnic_tpa_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_tpa_cfg_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, VNIC_TPA_CFG, -1, resp);

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
		req.vnic_id = rte_cpu_to_le_32(vnic->fw_vnic_id);
		req.max_agg_segs = rte_cpu_to_le_16(5);
		req.max_aggs =
			rte_cpu_to_le_16(HWRM_VNIC_TPA_CFG_INPUT_MAX_AGGS_MAX);
		req.min_agg_len = rte_cpu_to_le_32(512);
	}

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_func_vf_mac(struct bnxt *bp, uint16_t vf, const uint8_t *mac_addr)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	req.flags = rte_cpu_to_le_32(bp->pf.vf_info[vf].func_cfg_flags);
	req.enables = rte_cpu_to_le_32(
			HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_MAC_ADDR);
	memcpy(req.dflt_mac_addr, mac_addr, sizeof(req.dflt_mac_addr));
	req.fid = rte_cpu_to_le_16(bp->pf.vf_info[vf].fid);

	HWRM_PREP(req, FUNC_CFG, -1, resp);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

	bp->pf.vf_info[vf].random_mac = false;

	return rc;
}

int bnxt_hwrm_func_qstats_tx_drop(struct bnxt *bp, uint16_t fid,
				  uint64_t *dropped)
{
	int rc = 0;
	struct hwrm_func_qstats_input req = {.req_type = 0};
	struct hwrm_func_qstats_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, FUNC_QSTATS, -1, resp);

	req.fid = rte_cpu_to_le_16(fid);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	if (dropped)
		*dropped = rte_le_to_cpu_64(resp->tx_drop_pkts);

	return rc;
}

int bnxt_hwrm_func_qstats(struct bnxt *bp, uint16_t fid,
			  struct rte_eth_stats *stats)
{
	int rc = 0;
	struct hwrm_func_qstats_input req = {.req_type = 0};
	struct hwrm_func_qstats_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, FUNC_QSTATS, -1, resp);

	req.fid = rte_cpu_to_le_16(fid);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

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

	stats->ierrors = rte_le_to_cpu_64(resp->rx_err_pkts);
	stats->oerrors = rte_le_to_cpu_64(resp->tx_err_pkts);

	stats->imissed = rte_le_to_cpu_64(resp->rx_drop_pkts);

	return rc;
}

int bnxt_hwrm_func_clr_stats(struct bnxt *bp, uint16_t fid)
{
	int rc = 0;
	struct hwrm_func_clr_stats_input req = {.req_type = 0};
	struct hwrm_func_clr_stats_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, FUNC_CLR_STATS, -1, resp);

	req.fid = rte_cpu_to_le_16(fid);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

/*
 * HWRM utility functions
 */

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

int bnxt_free_all_hwrm_stat_ctxs(struct bnxt *bp)
{
	int rc;
	unsigned int i;
	struct bnxt_cp_ring_info *cpr;

	for (i = 0; i < bp->rx_cp_nr_rings + bp->tx_cp_nr_rings; i++) {

		if (i >= bp->rx_cp_nr_rings)
			cpr = bp->tx_queues[i - bp->rx_cp_nr_rings]->cp_ring;
		else
			cpr = bp->rx_queues[i]->cp_ring;
		if (cpr->hw_stats_ctx_id != HWRM_NA_SIGNATURE) {
			rc = bnxt_hwrm_stat_ctx_free(bp, cpr, i);
			cpr->hw_stats_ctx_id = HWRM_NA_SIGNATURE;
			/*
			 * TODO. Need a better way to reset grp_info.stats_ctx
			 * for Rx rings only. stats_ctx is not saved for Tx
			 * in grp_info.
			 */
			bp->grp_info[i].fw_stats_ctx = cpr->hw_stats_ctx_id;
			if (rc)
				return rc;
		}
	}
	return 0;
}

int bnxt_alloc_all_hwrm_stat_ctxs(struct bnxt *bp)
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

		rc = bnxt_hwrm_stat_ctx_alloc(bp, cpr, i);

		if (rc)
			return rc;
	}
	return rc;
}

int bnxt_free_all_hwrm_ring_grps(struct bnxt *bp)
{
	uint16_t idx;
	uint32_t rc = 0;

	for (idx = 0; idx < bp->rx_cp_nr_rings; idx++) {

		if (bp->grp_info[idx].fw_grp_id == INVALID_HW_RING_ID) {
			RTE_LOG(ERR, PMD,
				"Attempt to free invalid ring group %d\n",
				idx);
			continue;
		}

		rc = bnxt_hwrm_ring_grp_free(bp, idx);

		if (rc)
			return rc;
	}
	return rc;
}

static void bnxt_free_cp_ring(struct bnxt *bp, struct bnxt_cp_ring_info *cpr,
				unsigned int idx __rte_unused)
{
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;

	bnxt_hwrm_ring_free(bp, cp_ring,
			HWRM_RING_FREE_INPUT_RING_TYPE_L2_CMPL);
	cp_ring->fw_ring_id = INVALID_HW_RING_ID;
	bp->grp_info[idx].cp_fw_ring_id = INVALID_HW_RING_ID;
	memset(cpr->cp_desc_ring, 0, cpr->cp_ring_struct->ring_size *
			sizeof(*cpr->cp_desc_ring));
	cpr->cp_raw_cons = 0;
}

int bnxt_free_all_hwrm_rings(struct bnxt *bp)
{
	unsigned int i;
	int rc = 0;

	for (i = 0; i < bp->tx_cp_nr_rings; i++) {
		struct bnxt_tx_queue *txq = bp->tx_queues[i];
		struct bnxt_tx_ring_info *txr = txq->tx_ring;
		struct bnxt_ring *ring = txr->tx_ring_struct;
		struct bnxt_cp_ring_info *cpr = txq->cp_ring;
		unsigned int idx = bp->rx_cp_nr_rings + i + 1;

		if (ring->fw_ring_id != INVALID_HW_RING_ID) {
			bnxt_hwrm_ring_free(bp, ring,
					HWRM_RING_FREE_INPUT_RING_TYPE_TX);
			ring->fw_ring_id = INVALID_HW_RING_ID;
			memset(txr->tx_desc_ring, 0,
					txr->tx_ring_struct->ring_size *
					sizeof(*txr->tx_desc_ring));
			memset(txr->tx_buf_ring, 0,
					txr->tx_ring_struct->ring_size *
					sizeof(*txr->tx_buf_ring));
			txr->tx_prod = 0;
			txr->tx_cons = 0;
		}
		if (cpr->cp_ring_struct->fw_ring_id != INVALID_HW_RING_ID) {
			bnxt_free_cp_ring(bp, cpr, idx);
			cpr->cp_ring_struct->fw_ring_id = INVALID_HW_RING_ID;
		}
	}

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		struct bnxt_rx_queue *rxq = bp->rx_queues[i];
		struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
		struct bnxt_ring *ring = rxr->rx_ring_struct;
		struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
		unsigned int idx = i + 1;

		if (ring->fw_ring_id != INVALID_HW_RING_ID) {
			bnxt_hwrm_ring_free(bp, ring,
					HWRM_RING_FREE_INPUT_RING_TYPE_RX);
			ring->fw_ring_id = INVALID_HW_RING_ID;
			bp->grp_info[idx].rx_fw_ring_id = INVALID_HW_RING_ID;
			memset(rxr->rx_desc_ring, 0,
					rxr->rx_ring_struct->ring_size *
					sizeof(*rxr->rx_desc_ring));
			memset(rxr->rx_buf_ring, 0,
					rxr->rx_ring_struct->ring_size *
					sizeof(*rxr->rx_buf_ring));
			rxr->rx_prod = 0;
			memset(rxr->ag_buf_ring, 0,
					rxr->ag_ring_struct->ring_size *
					sizeof(*rxr->ag_buf_ring));
			rxr->ag_prod = 0;
		}
		if (cpr->cp_ring_struct->fw_ring_id != INVALID_HW_RING_ID) {
			bnxt_free_cp_ring(bp, cpr, idx);
			bp->grp_info[i].cp_fw_ring_id = INVALID_HW_RING_ID;
			cpr->cp_ring_struct->fw_ring_id = INVALID_HW_RING_ID;
		}
	}

	/* Default completion ring */
	{
		struct bnxt_cp_ring_info *cpr = bp->def_cp_ring;

		if (cpr->cp_ring_struct->fw_ring_id != INVALID_HW_RING_ID) {
			bnxt_free_cp_ring(bp, cpr, 0);
			cpr->cp_ring_struct->fw_ring_id = INVALID_HW_RING_ID;
		}
	}

	return rc;
}

int bnxt_alloc_all_hwrm_ring_grps(struct bnxt *bp)
{
	uint16_t i;
	uint32_t rc = 0;

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		rc = bnxt_hwrm_ring_grp_alloc(bp, i);
		if (rc)
			return rc;
	}
	return rc;
}

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

	sprintf(type, "bnxt_hwrm_%04x:%02x:%02x:%02x", pdev->addr.domain,
		pdev->addr.bus, pdev->addr.devid, pdev->addr.function);
	bp->max_resp_len = HWRM_MAX_RESP_LEN;
	bp->hwrm_cmd_resp_addr = rte_malloc(type, bp->max_resp_len, 0);
	rte_mem_lock_page(bp->hwrm_cmd_resp_addr);
	if (bp->hwrm_cmd_resp_addr == NULL)
		return -ENOMEM;
	bp->hwrm_cmd_resp_dma_addr =
		rte_mem_virt2phy(bp->hwrm_cmd_resp_addr);
	if (bp->hwrm_cmd_resp_dma_addr == 0) {
		RTE_LOG(ERR, PMD,
			"unable to map response address to physical memory\n");
		return -ENOMEM;
	}
	rte_spinlock_init(&bp->hwrm_lock);

	return 0;
}

int bnxt_clear_hwrm_vnic_filters(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	struct bnxt_filter_info *filter;
	int rc = 0;

	STAILQ_FOREACH(filter, &vnic->filter, next) {
		rc = bnxt_hwrm_clear_filter(bp, filter);
		if (rc)
			break;
	}
	return rc;
}

int bnxt_set_hwrm_vnic_filters(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	struct bnxt_filter_info *filter;
	int rc = 0;

	STAILQ_FOREACH(filter, &vnic->filter, next) {
		rc = bnxt_hwrm_set_filter(bp, vnic->fw_vnic_id, filter);
		if (rc)
			break;
	}
	return rc;
}

void bnxt_free_tunnel_ports(struct bnxt *bp)
{
	if (bp->vxlan_port_cnt)
		bnxt_hwrm_tunnel_dst_port_free(bp, bp->vxlan_fw_dst_port_id,
			HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_VXLAN);
	bp->vxlan_port = 0;
	if (bp->geneve_port_cnt)
		bnxt_hwrm_tunnel_dst_port_free(bp, bp->geneve_fw_dst_port_id,
			HWRM_TUNNEL_DST_PORT_FREE_INPUT_TUNNEL_TYPE_GENEVE);
	bp->geneve_port = 0;
}

void bnxt_free_all_hwrm_resources(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;
	unsigned int i;

	if (bp->vnic_info == NULL)
		return;

	vnic = &bp->vnic_info[0];
	if (BNXT_PF(bp))
		bnxt_hwrm_cfa_l2_clear_rx_mask(bp, vnic);

	/* VNIC resources */
	for (i = 0; i < bp->nr_vnics; i++) {
		struct bnxt_vnic_info *vnic = &bp->vnic_info[i];

		bnxt_clear_hwrm_vnic_filters(bp, vnic);

		bnxt_hwrm_vnic_ctx_free(bp, vnic);

		bnxt_hwrm_vnic_tpa_cfg(bp, vnic, false);

		bnxt_hwrm_vnic_free(bp, vnic);
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

	if ((conf_link_speed & ETH_LINK_SPEED_FIXED) == ETH_LINK_SPEED_AUTONEG)
		return HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_BOTH;

	switch (conf_link_speed) {
	case ETH_LINK_SPEED_10M_HD:
	case ETH_LINK_SPEED_100M_HD:
		return HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_HALF;
	}
	return hw_link_duplex;
}

static uint16_t bnxt_parse_eth_link_speed(uint32_t conf_link_speed)
{
	uint16_t eth_link_speed = 0;

	if (conf_link_speed == ETH_LINK_SPEED_AUTONEG)
		return ETH_LINK_SPEED_AUTONEG;

	switch (conf_link_speed & ~ETH_LINK_SPEED_FIXED) {
	case ETH_LINK_SPEED_100M:
	case ETH_LINK_SPEED_100M_HD:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_100MB;
		break;
	case ETH_LINK_SPEED_1G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_1GB;
		break;
	case ETH_LINK_SPEED_2_5G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_2_5GB;
		break;
	case ETH_LINK_SPEED_10G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_10GB;
		break;
	case ETH_LINK_SPEED_20G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_20GB;
		break;
	case ETH_LINK_SPEED_25G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_25GB;
		break;
	case ETH_LINK_SPEED_40G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_40GB;
		break;
	case ETH_LINK_SPEED_50G:
		eth_link_speed =
			HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_50GB;
		break;
	default:
		RTE_LOG(ERR, PMD,
			"Unsupported link speed %d; default to AUTO\n",
			conf_link_speed);
		break;
	}
	return eth_link_speed;
}

#define BNXT_SUPPORTED_SPEEDS (ETH_LINK_SPEED_100M | ETH_LINK_SPEED_100M_HD | \
		ETH_LINK_SPEED_1G | ETH_LINK_SPEED_2_5G | \
		ETH_LINK_SPEED_10G | ETH_LINK_SPEED_20G | ETH_LINK_SPEED_25G | \
		ETH_LINK_SPEED_40G | ETH_LINK_SPEED_50G)

static int bnxt_valid_link_speed(uint32_t link_speed, uint8_t port_id)
{
	uint32_t one_speed;

	if (link_speed == ETH_LINK_SPEED_AUTONEG)
		return 0;

	if (link_speed & ETH_LINK_SPEED_FIXED) {
		one_speed = link_speed & ~ETH_LINK_SPEED_FIXED;

		if (one_speed & (one_speed - 1)) {
			RTE_LOG(ERR, PMD,
				"Invalid advertised speeds (%u) for port %u\n",
				link_speed, port_id);
			return -EINVAL;
		}
		if ((one_speed & BNXT_SUPPORTED_SPEEDS) != one_speed) {
			RTE_LOG(ERR, PMD,
				"Unsupported advertised speed (%u) for port %u\n",
				link_speed, port_id);
			return -EINVAL;
		}
	} else {
		if (!(link_speed & BNXT_SUPPORTED_SPEEDS)) {
			RTE_LOG(ERR, PMD,
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

	if (link_speed == ETH_LINK_SPEED_AUTONEG) {
		if (bp->link_info.support_speeds)
			return bp->link_info.support_speeds;
		link_speed = BNXT_SUPPORTED_SPEEDS;
	}

	if (link_speed & ETH_LINK_SPEED_100M)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100MB;
	if (link_speed & ETH_LINK_SPEED_100M_HD)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100MB;
	if (link_speed & ETH_LINK_SPEED_1G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_1GB;
	if (link_speed & ETH_LINK_SPEED_2_5G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_2_5GB;
	if (link_speed & ETH_LINK_SPEED_10G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10GB;
	if (link_speed & ETH_LINK_SPEED_20G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_20GB;
	if (link_speed & ETH_LINK_SPEED_25G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_25GB;
	if (link_speed & ETH_LINK_SPEED_40G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_40GB;
	if (link_speed & ETH_LINK_SPEED_50G)
		ret |= HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_50GB;
	return ret;
}

static uint32_t bnxt_parse_hw_link_speed(uint16_t hw_link_speed)
{
	uint32_t eth_link_speed = ETH_SPEED_NUM_NONE;

	switch (hw_link_speed) {
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_100MB:
		eth_link_speed = ETH_SPEED_NUM_100M;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_1GB:
		eth_link_speed = ETH_SPEED_NUM_1G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2_5GB:
		eth_link_speed = ETH_SPEED_NUM_2_5G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_10GB:
		eth_link_speed = ETH_SPEED_NUM_10G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_20GB:
		eth_link_speed = ETH_SPEED_NUM_20G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_25GB:
		eth_link_speed = ETH_SPEED_NUM_25G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_40GB:
		eth_link_speed = ETH_SPEED_NUM_40G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_50GB:
		eth_link_speed = ETH_SPEED_NUM_50G;
		break;
	case HWRM_PORT_PHY_QCFG_OUTPUT_LINK_SPEED_2GB:
	default:
		RTE_LOG(ERR, PMD, "HWRM link speed %d not defined\n",
			hw_link_speed);
		break;
	}
	return eth_link_speed;
}

static uint16_t bnxt_parse_hw_link_duplex(uint16_t hw_link_duplex)
{
	uint16_t eth_link_duplex = ETH_LINK_FULL_DUPLEX;

	switch (hw_link_duplex) {
	case HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_BOTH:
	case HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_FULL:
		eth_link_duplex = ETH_LINK_FULL_DUPLEX;
		break;
	case HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_HALF:
		eth_link_duplex = ETH_LINK_HALF_DUPLEX;
		break;
	default:
		RTE_LOG(ERR, PMD, "HWRM link duplex %d not defined\n",
			hw_link_duplex);
		break;
	}
	return eth_link_duplex;
}

int bnxt_get_hwrm_link_config(struct bnxt *bp, struct rte_eth_link *link)
{
	int rc = 0;
	struct bnxt_link_info *link_info = &bp->link_info;

	rc = bnxt_hwrm_port_phy_qcfg(bp, link_info);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"Get link config failed with rc %d\n", rc);
		goto exit;
	}
	if (link_info->link_speed)
		link->link_speed =
			bnxt_parse_hw_link_speed(link_info->link_speed);
	else
		link->link_speed = ETH_SPEED_NUM_NONE;
	link->link_duplex = bnxt_parse_hw_link_duplex(link_info->duplex);
	link->link_status = link_info->link_up;
	link->link_autoneg = link_info->auto_mode ==
		HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_NONE ?
		ETH_LINK_FIXED : ETH_LINK_AUTONEG;
exit:
	return rc;
}

int bnxt_set_hwrm_link_config(struct bnxt *bp, bool link_up)
{
	int rc = 0;
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	struct bnxt_link_info link_req;
	uint16_t speed;

	if (BNXT_NPAR_PF(bp) || BNXT_VF(bp))
		return 0;

	rc = bnxt_valid_link_speed(dev_conf->link_speeds,
			bp->eth_dev->data->port_id);
	if (rc)
		goto error;

	memset(&link_req, 0, sizeof(link_req));
	link_req.link_up = link_up;
	if (!link_up)
		goto port_phy_cfg;

	speed = bnxt_parse_eth_link_speed(dev_conf->link_speeds);
	link_req.phy_flags = HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESET_PHY;
	if (speed == 0) {
		link_req.phy_flags |=
				HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESTART_AUTONEG;
		link_req.auto_mode =
				HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_SPEED_MASK;
		link_req.auto_link_speed_mask =
			bnxt_parse_eth_link_speed_mask(bp,
						       dev_conf->link_speeds);
	} else {
		link_req.phy_flags |= HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE;
		link_req.link_speed = speed;
		RTE_LOG(INFO, PMD, "Set Link Speed %x\n", speed);
	}
	link_req.duplex = bnxt_parse_eth_link_duplex(dev_conf->link_speeds);
	link_req.auto_pause = bp->link_info.auto_pause;
	link_req.force_pause = bp->link_info.force_pause;

port_phy_cfg:
	rc = bnxt_hwrm_port_phy_cfg(bp, &link_req);
	if (rc) {
		RTE_LOG(ERR, PMD,
			"Set link config failed with rc %d\n", rc);
	}

error:
	return rc;
}

/* JIRA 22088 */
int bnxt_hwrm_func_qcfg(struct bnxt *bp)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc = 0;

	HWRM_PREP(req, FUNC_QCFG, -1, resp);
	req.fid = rte_cpu_to_le_16(0xffff);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	/* Hard Coded.. 0xfff VLAN ID mask */
	bp->vlan = rte_le_to_cpu_16(resp->vlan) & 0xfff;

	switch (resp->port_partition_type) {
	case HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR1_0:
	case HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR1_5:
	case HWRM_FUNC_QCFG_OUTPUT_PORT_PARTITION_TYPE_NPAR2_0:
		bp->port_partition_type = resp->port_partition_type;
		break;
	default:
		bp->port_partition_type = 0;
		break;
	}

	return rc;
}

static void copy_func_cfg_to_qcaps(struct hwrm_func_cfg_input *fcfg,
				   struct hwrm_func_qcaps_output *qcaps)
{
	qcaps->max_rsscos_ctx = fcfg->num_rsscos_ctxs;
	memcpy(qcaps->mac_address, fcfg->dflt_mac_addr,
	       sizeof(qcaps->mac_address));
	qcaps->max_l2_ctxs = fcfg->num_l2_ctxs;
	qcaps->max_rx_rings = fcfg->num_rx_rings;
	qcaps->max_tx_rings = fcfg->num_tx_rings;
	qcaps->max_cmpl_rings = fcfg->num_cmpl_rings;
	qcaps->max_stat_ctx = fcfg->num_stat_ctxs;
	qcaps->max_vfs = 0;
	qcaps->first_vf_id = 0;
	qcaps->max_vnics = fcfg->num_vnics;
	qcaps->max_decap_records = 0;
	qcaps->max_encap_records = 0;
	qcaps->max_tx_wm_flows = 0;
	qcaps->max_tx_em_flows = 0;
	qcaps->max_rx_wm_flows = 0;
	qcaps->max_rx_em_flows = 0;
	qcaps->max_flow_id = 0;
	qcaps->max_mcast_filters = fcfg->num_mcast_filters;
	qcaps->max_sp_tx_rings = 0;
	qcaps->max_hw_ring_grps = fcfg->num_hw_ring_grps;
}

static int bnxt_hwrm_pf_func_cfg(struct bnxt *bp, int tx_rings)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	req.enables = rte_cpu_to_le_32(HWRM_FUNC_CFG_INPUT_ENABLES_MTU |
			HWRM_FUNC_CFG_INPUT_ENABLES_MRU |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RSSCOS_CTXS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_STAT_CTXS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_CMPL_RINGS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_TX_RINGS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RX_RINGS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_L2_CTXS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_VNICS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_HW_RING_GRPS);
	req.flags = rte_cpu_to_le_32(bp->pf.func_cfg_flags);
	req.mtu = rte_cpu_to_le_16(BNXT_MAX_MTU);
	req.mru = rte_cpu_to_le_16(bp->eth_dev->data->mtu + ETHER_HDR_LEN +
				   ETHER_CRC_LEN + VLAN_TAG_SIZE);
	req.num_rsscos_ctxs = rte_cpu_to_le_16(bp->max_rsscos_ctx);
	req.num_stat_ctxs = rte_cpu_to_le_16(bp->max_stat_ctx);
	req.num_cmpl_rings = rte_cpu_to_le_16(bp->max_cp_rings);
	req.num_tx_rings = rte_cpu_to_le_16(tx_rings);
	req.num_rx_rings = rte_cpu_to_le_16(bp->max_rx_rings);
	req.num_l2_ctxs = rte_cpu_to_le_16(bp->max_l2_ctx);
	req.num_vnics = rte_cpu_to_le_16(bp->max_vnics);
	req.num_hw_ring_grps = rte_cpu_to_le_16(bp->max_ring_grps);
	req.fid = rte_cpu_to_le_16(0xffff);

	HWRM_PREP(req, FUNC_CFG, -1, resp);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

	return rc;
}

static void populate_vf_func_cfg_req(struct bnxt *bp,
				     struct hwrm_func_cfg_input *req,
				     int num_vfs)
{
	req->enables = rte_cpu_to_le_32(HWRM_FUNC_CFG_INPUT_ENABLES_MTU |
			HWRM_FUNC_CFG_INPUT_ENABLES_MRU |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RSSCOS_CTXS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_STAT_CTXS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_CMPL_RINGS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_TX_RINGS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_RX_RINGS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_L2_CTXS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_VNICS |
			HWRM_FUNC_CFG_INPUT_ENABLES_NUM_HW_RING_GRPS);

	req->mtu = rte_cpu_to_le_16(bp->eth_dev->data->mtu + ETHER_HDR_LEN +
				    ETHER_CRC_LEN + VLAN_TAG_SIZE);
	req->mru = rte_cpu_to_le_16(bp->eth_dev->data->mtu + ETHER_HDR_LEN +
				    ETHER_CRC_LEN + VLAN_TAG_SIZE);
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

static void add_random_mac_if_needed(struct bnxt *bp,
				     struct hwrm_func_cfg_input *cfg_req,
				     int vf)
{
	struct ether_addr mac;

	if (bnxt_hwrm_func_qcfg_vf_default_mac(bp, vf, &mac))
		return;

	if (memcmp(mac.addr_bytes, "\x00\x00\x00\x00\x00", 6) == 0) {
		cfg_req->enables |=
		rte_cpu_to_le_32(HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_MAC_ADDR);
		eth_random_addr(cfg_req->dflt_mac_addr);
		bp->pf.vf_info[vf].random_mac = true;
	} else {
		memcpy(cfg_req->dflt_mac_addr, mac.addr_bytes, ETHER_ADDR_LEN);
	}
}

static void reserve_resources_from_vf(struct bnxt *bp,
				      struct hwrm_func_cfg_input *cfg_req,
				      int vf)
{
	struct hwrm_func_qcaps_input req = {0};
	struct hwrm_func_qcaps_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	/* Get the actual allocated values now */
	HWRM_PREP(req, FUNC_QCAPS, -1, resp);
	req.fid = rte_cpu_to_le_16(bp->pf.vf_info[vf].fid);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	if (rc) {
		RTE_LOG(ERR, PMD, "hwrm_func_qcaps failed rc:%d\n", rc);
		copy_func_cfg_to_qcaps(cfg_req, resp);
	} else if (resp->error_code) {
		rc = rte_le_to_cpu_16(resp->error_code);
		RTE_LOG(ERR, PMD, "hwrm_func_qcaps error %d\n", rc);
		copy_func_cfg_to_qcaps(cfg_req, resp);
	}

	bp->max_rsscos_ctx -= rte_le_to_cpu_16(resp->max_rsscos_ctx);
	bp->max_stat_ctx -= rte_le_to_cpu_16(resp->max_stat_ctx);
	bp->max_cp_rings -= rte_le_to_cpu_16(resp->max_cmpl_rings);
	bp->max_tx_rings -= rte_le_to_cpu_16(resp->max_tx_rings);
	bp->max_rx_rings -= rte_le_to_cpu_16(resp->max_rx_rings);
	bp->max_l2_ctx -= rte_le_to_cpu_16(resp->max_l2_ctxs);
	/*
	 * TODO: While not supporting VMDq with VFs, max_vnics is always
	 * forced to 1 in this case
	 */
	//bp->max_vnics -= rte_le_to_cpu_16(esp->max_vnics);
	bp->max_ring_grps -= rte_le_to_cpu_16(resp->max_hw_ring_grps);
}

int bnxt_hwrm_func_qcfg_current_vf_vlan(struct bnxt *bp, int vf)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	/* Check for zero MAC address */
	HWRM_PREP(req, FUNC_QCFG, -1, resp);
	req.fid = rte_cpu_to_le_16(bp->pf.vf_info[vf].fid);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	if (rc) {
		RTE_LOG(ERR, PMD, "hwrm_func_qcfg failed rc:%d\n", rc);
		return -1;
	} else if (resp->error_code) {
		rc = rte_le_to_cpu_16(resp->error_code);
		RTE_LOG(ERR, PMD, "hwrm_func_qcfg error %d\n", rc);
		return -1;
	}
	return rte_le_to_cpu_16(resp->vlan);
}

static int update_pf_resource_max(struct bnxt *bp)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	/* And copy the allocated numbers into the pf struct */
	HWRM_PREP(req, FUNC_QCFG, -1, resp);
	req.fid = rte_cpu_to_le_16(0xffff);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

	/* Only TX ring value reflects actual allocation? TODO */
	bp->max_tx_rings = rte_le_to_cpu_16(resp->alloc_tx_rings);
	bp->pf.evb_mode = resp->evb_mode;

	return rc;
}

int bnxt_hwrm_allocate_pf_only(struct bnxt *bp)
{
	int rc;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD, "Attempt to allcoate VFs on a VF!\n");
		return -1;
	}

	rc = bnxt_hwrm_func_qcaps(bp);
	if (rc)
		return rc;

	bp->pf.func_cfg_flags &=
		~(HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_ENABLE |
		  HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_DISABLE);
	bp->pf.func_cfg_flags |=
		HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_DISABLE;
	rc = bnxt_hwrm_pf_func_cfg(bp, bp->max_tx_rings);
	return rc;
}

int bnxt_hwrm_allocate_vfs(struct bnxt *bp, int num_vfs)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	int i;
	size_t sz;
	int rc = 0;
	size_t req_buf_sz;

	if (!BNXT_PF(bp)) {
		RTE_LOG(ERR, PMD, "Attempt to allcoate VFs on a VF!\n");
		return -1;
	}

	rc = bnxt_hwrm_func_qcaps(bp);

	if (rc)
		return rc;

	bp->pf.active_vfs = num_vfs;

	/*
	 * First, configure the PF to only use one TX ring.  This ensures that
	 * there are enough rings for all VFs.
	 *
	 * If we don't do this, when we call func_alloc() later, we will lock
	 * extra rings to the PF that won't be available during func_cfg() of
	 * the VFs.
	 *
	 * This has been fixed with firmware versions above 20.6.54
	 */
	bp->pf.func_cfg_flags &=
		~(HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_ENABLE |
		  HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_DISABLE);
	bp->pf.func_cfg_flags |=
		HWRM_FUNC_CFG_INPUT_FLAGS_STD_TX_RING_MODE_ENABLE;
	rc = bnxt_hwrm_pf_func_cfg(bp, 1);
	if (rc)
		return rc;

	/*
	 * Now, create and register a buffer to hold forwarded VF requests
	 */
	req_buf_sz = num_vfs * HWRM_MAX_REQ_LEN;
	bp->pf.vf_req_buf = rte_malloc("bnxt_vf_fwd", req_buf_sz,
		page_roundup(num_vfs * HWRM_MAX_REQ_LEN));
	if (bp->pf.vf_req_buf == NULL) {
		rc = -ENOMEM;
		goto error_free;
	}
	for (sz = 0; sz < req_buf_sz; sz += getpagesize())
		rte_mem_lock_page(((char *)bp->pf.vf_req_buf) + sz);
	for (i = 0; i < num_vfs; i++)
		bp->pf.vf_info[i].req_buf = ((char *)bp->pf.vf_req_buf) +
					(i * HWRM_MAX_REQ_LEN);

	rc = bnxt_hwrm_func_buf_rgtr(bp);
	if (rc)
		goto error_free;

	populate_vf_func_cfg_req(bp, &req, num_vfs);

	bp->pf.active_vfs = 0;
	for (i = 0; i < num_vfs; i++) {
		add_random_mac_if_needed(bp, &req, i);

		HWRM_PREP(req, FUNC_CFG, -1, resp);
		req.flags = rte_cpu_to_le_32(bp->pf.vf_info[i].func_cfg_flags);
		req.fid = rte_cpu_to_le_16(bp->pf.vf_info[i].fid);
		rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

		/* Clear enable flag for next pass */
		req.enables &= ~rte_cpu_to_le_32(
				HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_MAC_ADDR);

		if (rc || resp->error_code) {
			RTE_LOG(ERR, PMD,
				"Failed to initizlie VF %d\n", i);
			RTE_LOG(ERR, PMD,
				"Not all VFs available. (%d, %d)\n",
				rc, resp->error_code);
			break;
		}

		reserve_resources_from_vf(bp, &req, i);
		bp->pf.active_vfs++;
		bnxt_hwrm_func_clr_stats(bp, bp->pf.vf_info[i].fid);
	}

	/*
	 * Now configure the PF to use "the rest" of the resources
	 * We're using STD_TX_RING_MODE here though which will limit the TX
	 * rings.  This will allow QoS to function properly.  Not setting this
	 * will cause PF rings to break bandwidth settings.
	 */
	rc = bnxt_hwrm_pf_func_cfg(bp, bp->max_tx_rings);
	if (rc)
		goto error_free;

	rc = update_pf_resource_max(bp);
	if (rc)
		goto error_free;

	return rc;

error_free:
	bnxt_hwrm_func_buf_unrgtr(bp);
	return rc;
}

int bnxt_hwrm_pf_evb_mode(struct bnxt *bp)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	HWRM_PREP(req, FUNC_CFG, -1, resp);

	req.fid = rte_cpu_to_le_16(0xffff);
	req.enables = rte_cpu_to_le_32(HWRM_FUNC_CFG_INPUT_ENABLES_EVB_MODE);
	req.evb_mode = bp->pf.evb_mode;

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_tunnel_dst_port_alloc(struct bnxt *bp, uint16_t port,
				uint8_t tunnel_type)
{
	struct hwrm_tunnel_dst_port_alloc_input req = {0};
	struct hwrm_tunnel_dst_port_alloc_output *resp = bp->hwrm_cmd_resp_addr;
	int rc = 0;

	HWRM_PREP(req, TUNNEL_DST_PORT_ALLOC, -1, resp);
	req.tunnel_type = tunnel_type;
	req.tunnel_dst_port_val = port;
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

	switch (tunnel_type) {
	case HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_VXLAN:
		bp->vxlan_fw_dst_port_id = resp->tunnel_dst_port_id;
		bp->vxlan_port = port;
		break;
	case HWRM_TUNNEL_DST_PORT_ALLOC_INPUT_TUNNEL_TYPE_GENEVE:
		bp->geneve_fw_dst_port_id = resp->tunnel_dst_port_id;
		bp->geneve_port = port;
		break;
	default:
		break;
	}
	return rc;
}

int bnxt_hwrm_tunnel_dst_port_free(struct bnxt *bp, uint16_t port,
				uint8_t tunnel_type)
{
	struct hwrm_tunnel_dst_port_free_input req = {0};
	struct hwrm_tunnel_dst_port_free_output *resp = bp->hwrm_cmd_resp_addr;
	int rc = 0;

	HWRM_PREP(req, TUNNEL_DST_PORT_FREE, -1, resp);
	req.tunnel_type = tunnel_type;
	req.tunnel_dst_port_id = rte_cpu_to_be_16(port);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_func_cfg_vf_set_flags(struct bnxt *bp, uint16_t vf,
					uint32_t flags)
{
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_cfg_input req = {0};
	int rc;

	HWRM_PREP(req, FUNC_CFG, -1, resp);
	req.fid = rte_cpu_to_le_16(bp->pf.vf_info[vf].fid);
	req.flags = rte_cpu_to_le_32(flags);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

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

int bnxt_hwrm_func_buf_rgtr(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_func_buf_rgtr_input req = {.req_type = 0 };
	struct hwrm_func_buf_rgtr_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, FUNC_BUF_RGTR, -1, resp);

	req.req_buf_num_pages = rte_cpu_to_le_16(1);
	req.req_buf_page_size = rte_cpu_to_le_16(
			 page_getenum(bp->pf.active_vfs * HWRM_MAX_REQ_LEN));
	req.req_buf_len = rte_cpu_to_le_16(HWRM_MAX_REQ_LEN);
	req.req_buf_page_addr[0] =
		rte_cpu_to_le_64(rte_mem_virt2phy(bp->pf.vf_req_buf));
	if (req.req_buf_page_addr[0] == 0) {
		RTE_LOG(ERR, PMD,
			"unable to map buffer address to physical memory\n");
		return -ENOMEM;
	}

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_func_buf_unrgtr(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_func_buf_unrgtr_input req = {.req_type = 0 };
	struct hwrm_func_buf_unrgtr_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, FUNC_BUF_UNRGTR, -1, resp);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_func_cfg_def_cp(struct bnxt *bp)
{
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_cfg_input req = {0};
	int rc;

	HWRM_PREP(req, FUNC_CFG, -1, resp);
	req.fid = rte_cpu_to_le_16(0xffff);
	req.flags = rte_cpu_to_le_32(bp->pf.func_cfg_flags);
	req.enables = rte_cpu_to_le_32(
			HWRM_FUNC_CFG_INPUT_ENABLES_ASYNC_EVENT_CR);
	req.async_event_cr = rte_cpu_to_le_16(
			bp->def_cp_ring->cp_ring_struct->fw_ring_id);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_vf_func_cfg_def_cp(struct bnxt *bp)
{
	struct hwrm_func_vf_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_vf_cfg_input req = {0};
	int rc;

	HWRM_PREP(req, FUNC_VF_CFG, -1, resp);
	req.enables = rte_cpu_to_le_32(
			HWRM_FUNC_CFG_INPUT_ENABLES_ASYNC_EVENT_CR);
	req.async_event_cr = rte_cpu_to_le_16(
			bp->def_cp_ring->cp_ring_struct->fw_ring_id);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_set_default_vlan(struct bnxt *bp, int vf, uint8_t is_vf)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	uint16_t dflt_vlan, fid;
	uint32_t func_cfg_flags;
	int rc = 0;

	HWRM_PREP(req, FUNC_CFG, -1, resp);

	if (is_vf) {
		dflt_vlan = bp->pf.vf_info[vf].dflt_vlan;
		fid = bp->pf.vf_info[vf].fid;
		func_cfg_flags = bp->pf.vf_info[vf].func_cfg_flags;
	} else {
		fid = rte_cpu_to_le_16(0xffff);
		func_cfg_flags = bp->pf.func_cfg_flags;
		dflt_vlan = bp->vlan;
	}

	req.flags = rte_cpu_to_le_32(func_cfg_flags);
	req.fid = rte_cpu_to_le_16(fid);
	req.enables |= rte_cpu_to_le_32(HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_VLAN);
	req.dflt_vlan = rte_cpu_to_le_16(dflt_vlan);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_func_bw_cfg(struct bnxt *bp, uint16_t vf,
			uint16_t max_bw, uint16_t enables)
{
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_func_cfg_input req = {0};
	int rc;

	HWRM_PREP(req, FUNC_CFG, -1, resp);
	req.fid = rte_cpu_to_le_16(bp->pf.vf_info[vf].fid);
	req.enables |= rte_cpu_to_le_32(enables);
	req.flags = rte_cpu_to_le_32(bp->pf.vf_info[vf].func_cfg_flags);
	req.max_bw = rte_cpu_to_le_32(max_bw);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_set_vf_vlan(struct bnxt *bp, int vf)
{
	struct hwrm_func_cfg_input req = {0};
	struct hwrm_func_cfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc = 0;

	HWRM_PREP(req, FUNC_CFG, -1, resp);
	req.flags = rte_cpu_to_le_32(bp->pf.vf_info[vf].func_cfg_flags);
	req.fid = rte_cpu_to_le_16(bp->pf.vf_info[vf].fid);
	req.enables |= rte_cpu_to_le_32(HWRM_FUNC_CFG_INPUT_ENABLES_DFLT_VLAN);
	req.dflt_vlan = rte_cpu_to_le_16(bp->pf.vf_info[vf].dflt_vlan);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

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

	HWRM_PREP(req, REJECT_FWD_RESP, -1, resp);

	req.encap_resp_target_id = rte_cpu_to_le_16(target_id);
	memcpy(req.encap_request, encaped, ec_size);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_func_qcfg_vf_default_mac(struct bnxt *bp, uint16_t vf,
				       struct ether_addr *mac)
{
	struct hwrm_func_qcfg_input req = {0};
	struct hwrm_func_qcfg_output *resp = bp->hwrm_cmd_resp_addr;
	int rc;

	HWRM_PREP(req, FUNC_QCFG, -1, resp);
	req.fid = rte_cpu_to_le_16(bp->pf.vf_info[vf].fid);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	memcpy(mac->addr_bytes, resp->mac_address, ETHER_ADDR_LEN);
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

	HWRM_PREP(req, EXEC_FWD_RESP, -1, resp);

	req.encap_resp_target_id = rte_cpu_to_le_16(target_id);
	memcpy(req.encap_request, encaped, ec_size);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_ctx_qstats(struct bnxt *bp, uint32_t cid, int idx,
			 struct rte_eth_stats *stats)
{
	int rc = 0;
	struct hwrm_stat_ctx_query_input req = {.req_type = 0};
	struct hwrm_stat_ctx_query_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, STAT_CTX_QUERY, -1, resp);

	req.stat_ctx_id = rte_cpu_to_le_32(cid);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	stats->q_ipackets[idx] = rte_le_to_cpu_64(resp->rx_ucast_pkts);
	stats->q_ipackets[idx] += rte_le_to_cpu_64(resp->rx_mcast_pkts);
	stats->q_ipackets[idx] += rte_le_to_cpu_64(resp->rx_bcast_pkts);
	stats->q_ibytes[idx] = rte_le_to_cpu_64(resp->rx_ucast_bytes);
	stats->q_ibytes[idx] += rte_le_to_cpu_64(resp->rx_mcast_bytes);
	stats->q_ibytes[idx] += rte_le_to_cpu_64(resp->rx_bcast_bytes);

	stats->q_opackets[idx] = rte_le_to_cpu_64(resp->tx_ucast_pkts);
	stats->q_opackets[idx] += rte_le_to_cpu_64(resp->tx_mcast_pkts);
	stats->q_opackets[idx] += rte_le_to_cpu_64(resp->tx_bcast_pkts);
	stats->q_obytes[idx] = rte_le_to_cpu_64(resp->tx_ucast_bytes);
	stats->q_obytes[idx] += rte_le_to_cpu_64(resp->tx_mcast_bytes);
	stats->q_obytes[idx] += rte_le_to_cpu_64(resp->tx_bcast_bytes);

	stats->q_errors[idx] = rte_le_to_cpu_64(resp->rx_err_pkts);
	stats->q_errors[idx] += rte_le_to_cpu_64(resp->tx_err_pkts);
	stats->q_errors[idx] += rte_le_to_cpu_64(resp->rx_drop_pkts);

	return rc;
}

int bnxt_hwrm_port_qstats(struct bnxt *bp)
{
	struct hwrm_port_qstats_input req = {0};
	struct hwrm_port_qstats_output *resp = bp->hwrm_cmd_resp_addr;
	struct bnxt_pf_info *pf = &bp->pf;
	int rc;

	if (!(bp->flags & BNXT_FLAG_PORT_STATS))
		return 0;

	HWRM_PREP(req, PORT_QSTATS, -1, resp);
	req.port_id = rte_cpu_to_le_16(pf->port_id);
	req.tx_stat_host_addr = rte_cpu_to_le_64(bp->hw_tx_port_stats_map);
	req.rx_stat_host_addr = rte_cpu_to_le_64(bp->hw_rx_port_stats_map);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;
	return rc;
}

int bnxt_hwrm_port_clr_stats(struct bnxt *bp)
{
	struct hwrm_port_clr_stats_input req = {0};
	struct hwrm_port_clr_stats_output *resp = bp->hwrm_cmd_resp_addr;
	struct bnxt_pf_info *pf = &bp->pf;
	int rc;

	if (!(bp->flags & BNXT_FLAG_PORT_STATS))
		return 0;

	HWRM_PREP(req, PORT_CLR_STATS, -1, resp);
	req.port_id = rte_cpu_to_le_16(pf->port_id);
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;
	return rc;
}

int bnxt_hwrm_port_led_qcaps(struct bnxt *bp)
{
	struct hwrm_port_led_qcaps_output *resp = bp->hwrm_cmd_resp_addr;
	struct hwrm_port_led_qcaps_input req = {0};
	int rc;

	if (BNXT_VF(bp))
		return 0;

	HWRM_PREP(req, PORT_LED_QCAPS, -1, resp);
	req.port_id = bp->pf.port_id;
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

	if (resp->num_leds > 0 && resp->num_leds < BNXT_MAX_LED) {
		unsigned int i;

		bp->num_leds = resp->num_leds;
		memcpy(bp->leds, &resp->led0_id,
			sizeof(bp->leds[0]) * bp->num_leds);
		for (i = 0; i < bp->num_leds; i++) {
			struct bnxt_led_info *led = &bp->leds[i];

			uint16_t caps = led->led_state_caps;

			if (!led->led_group_id ||
				!BNXT_LED_ALT_BLINK_CAP(caps)) {
				bp->num_leds = 0;
				break;
			}
		}
	}
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

	if (!bp->num_leds || BNXT_VF(bp))
		return -EOPNOTSUPP;

	HWRM_PREP(req, PORT_LED_CFG, -1, resp);
	if (led_on) {
		led_state = HWRM_PORT_LED_CFG_INPUT_LED0_STATE_BLINKALT;
		duration = rte_cpu_to_le_16(500);
	}
	req.port_id = bp->pf.port_id;
	req.num_leds = bp->num_leds;
	led_cfg = (struct bnxt_led_cfg *)&req.led0_id;
	for (i = 0; i < bp->num_leds; i++, led_cfg++) {
		req.enables |= BNXT_LED_DFLT_ENABLES(i);
		led_cfg->led_id = bp->leds[i].led_id;
		led_cfg->led_state = led_state;
		led_cfg->led_blink_on = duration;
		led_cfg->led_blink_off = duration;
		led_cfg->led_group_id = bp->leds[i].led_group_id;
	}

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

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
	HWRM_PREP(req, FUNC_VF_VNIC_IDS_QUERY, -1, resp_vf_vnic_ids);

	req.vf_id = rte_cpu_to_le_16(bp->pf.first_vf_id + vf);
	req.max_vnic_id_cnt = rte_cpu_to_le_32(bp->pf.total_vnics);
	req.vnic_id_tbl_addr = rte_cpu_to_le_64(rte_mem_virt2phy(vnic_ids));

	if (req.vnic_id_tbl_addr == 0) {
		RTE_LOG(ERR, PMD,
		"unable to map VNIC ID table address to physical memory\n");
		return -ENOMEM;
	}
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	if (rc) {
		RTE_LOG(ERR, PMD, "hwrm_func_vf_vnic_query failed rc:%d\n", rc);
		return -1;
	} else if (resp->error_code) {
		rc = rte_le_to_cpu_16(resp->error_code);
		RTE_LOG(ERR, PMD, "hwrm_func_vf_vnic_query error %d\n", rc);
		return -1;
	}

	return rte_le_to_cpu_32(resp->vnic_id_cnt);
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
	vnic_id_sz = bp->pf.total_vnics * sizeof(*vnic_ids);
	vnic_ids = rte_malloc("bnxt_hwrm_vf_vnic_ids_query", vnic_id_sz,
			RTE_CACHE_LINE_SIZE);
	if (vnic_ids == NULL) {
		rc = -ENOMEM;
		return rc;
	}
	for (sz = 0; sz < vnic_id_sz; sz += getpagesize())
		rte_mem_lock_page(((char *)vnic_ids) + sz);

	num_vnic_ids = bnxt_hwrm_func_vf_vnic_query(bp, vf, vnic_ids);

	if (num_vnic_ids < 0)
		return num_vnic_ids;

	/* Retrieve VNIC, update bd_stall then update */

	for (i = 0; i < num_vnic_ids; i++) {
		memset(&vnic, 0, sizeof(struct bnxt_vnic_info));
		vnic.fw_vnic_id = rte_le_to_cpu_16(vnic_ids[i]);
		rc = bnxt_hwrm_vnic_qcfg(bp, &vnic, bp->pf.first_vf_id + vf);
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

	HWRM_PREP(req, FUNC_CFG, -1, resp);
	req.fid = rte_cpu_to_le_16(bp->pf.vf_info[vf].fid);
	req.enables |= rte_cpu_to_le_32(
			HWRM_FUNC_CFG_INPUT_ENABLES_VLAN_ANTISPOOF_MODE);
	req.vlan_antispoof_mode = on ?
		HWRM_FUNC_CFG_INPUT_VLAN_ANTISPOOF_MODE_VALIDATE_VLAN :
		HWRM_FUNC_CFG_INPUT_VLAN_ANTISPOOF_MODE_NOCHECK;
	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));
	HWRM_CHECK_RESULT;

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

	vnic_id_sz = bp->pf.total_vnics * sizeof(*vnic_ids);
	vnic_ids = rte_malloc("bnxt_hwrm_vf_vnic_ids_query", vnic_id_sz,
			RTE_CACHE_LINE_SIZE);
	if (vnic_ids == NULL) {
		rc = -ENOMEM;
		return rc;
	}

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
					bp->pf.first_vf_id + vf);
		if (rc)
			goto exit;
		if (vnic.func_default) {
			rte_free(vnic_ids);
			return vnic.fw_vnic_id;
		}
	}
	/* Could not find a default VNIC. */
	RTE_LOG(ERR, PMD, "No default VNIC\n");
exit:
	rte_free(vnic_ids);
	return -1;
}
