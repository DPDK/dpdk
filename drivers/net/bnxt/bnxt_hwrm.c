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

#define HWRM_CMD_TIMEOUT		2000

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

	/* Write request msg to hwrm channel */
	for (i = 0; i < msg_len; i += 4) {
		bar = (uint8_t *)bp->bar0 + i;
		*(volatile uint32_t *)bar = *data;
		data++;
	}

	/* Zero the rest of the request space */
	for (; i < bp->max_req_len; i += 4) {
		bar = (uint8_t *)bp->bar0 + i;
		*(volatile uint32_t *)bar = 0;
	}

	/* Ring channel doorbell */
	bar = (uint8_t *)bp->bar0 + 0x100;
	*(volatile uint32_t *)bar = 1;

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
		RTE_LOG(ERR, PMD, "Error sending msg %x\n",
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
			RTE_LOG(ERR, PMD, "%s error %d\n", __func__, rc); \
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

int bnxt_hwrm_cfa_l2_set_rx_mask(struct bnxt *bp, struct bnxt_vnic_info *vnic)
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
	if (vnic->flags & BNXT_VNIC_INFO_PROMISC)
		mask = HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS;
	if (vnic->flags & BNXT_VNIC_INFO_ALLMULTI)
		mask = HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST;
	req.mask = rte_cpu_to_le_32(HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST |
				    mask);

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

	HWRM_PREP(req, CFA_L2_FILTER_FREE, -1, resp);

	req.l2_filter_id = rte_cpu_to_le_64(filter->fw_l2_filter_id);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	filter->fw_l2_filter_id = -1;

	return 0;
}

int bnxt_hwrm_set_filter(struct bnxt *bp,
			 struct bnxt_vnic_info *vnic,
			 struct bnxt_filter_info *filter)
{
	int rc = 0;
	struct hwrm_cfa_l2_filter_alloc_input req = {.req_type = 0 };
	struct hwrm_cfa_l2_filter_alloc_output *resp = bp->hwrm_cmd_resp_addr;
	uint32_t enables = 0;

	HWRM_PREP(req, CFA_L2_FILTER_ALLOC, -1, resp);

	req.flags = rte_cpu_to_le_32(filter->flags);

	enables = filter->enables |
	      HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_DST_ID;
	req.dst_id = rte_cpu_to_le_16(vnic->fw_vnic_id);

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

	req.enables = rte_cpu_to_le_32(enables);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	filter->fw_l2_filter_id = rte_le_to_cpu_64(resp->l2_filter_id);

	return rc;
}

int bnxt_hwrm_exec_fwd_resp(struct bnxt *bp, void *fwd_cmd)
{
	int rc;
	struct hwrm_exec_fwd_resp_input req = {.req_type = 0 };
	struct hwrm_exec_fwd_resp_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, EXEC_FWD_RESP, -1, resp);

	memcpy(req.encap_request, fwd_cmd,
	       sizeof(req.encap_request));

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_func_qcaps(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_func_qcaps_input req = {.req_type = 0 };
	struct hwrm_func_qcaps_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, FUNC_QCAPS, -1, resp);

	req.fid = rte_cpu_to_le_16(0xffff);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	bp->max_ring_grps = rte_le_to_cpu_32(resp->max_hw_ring_grps);
	if (BNXT_PF(bp)) {
		struct bnxt_pf_info *pf = &bp->pf;

		pf->fw_fid = rte_le_to_cpu_32(resp->fid);
		pf->port_id = resp->port_id;
		memcpy(pf->mac_addr, resp->mac_address, ETHER_ADDR_LEN);
		pf->max_rsscos_ctx = rte_le_to_cpu_16(resp->max_rsscos_ctx);
		pf->max_cp_rings = rte_le_to_cpu_16(resp->max_cmpl_rings);
		pf->max_tx_rings = rte_le_to_cpu_16(resp->max_tx_rings);
		pf->max_rx_rings = rte_le_to_cpu_16(resp->max_rx_rings);
		pf->max_l2_ctx = rte_le_to_cpu_16(resp->max_l2_ctxs);
		pf->max_vnics = rte_le_to_cpu_16(resp->max_vnics);
		pf->first_vf_id = rte_le_to_cpu_16(resp->first_vf_id);
		pf->max_vfs = rte_le_to_cpu_16(resp->max_vfs);
	} else {
		struct bnxt_vf_info *vf = &bp->vf;

		vf->fw_fid = rte_le_to_cpu_32(resp->fid);
		memcpy(vf->mac_addr, &resp->mac_address, ETHER_ADDR_LEN);
		vf->max_rsscos_ctx = rte_le_to_cpu_16(resp->max_rsscos_ctx);
		vf->max_cp_rings = rte_le_to_cpu_16(resp->max_cmpl_rings);
		vf->max_tx_rings = rte_le_to_cpu_16(resp->max_tx_rings);
		vf->max_rx_rings = rte_le_to_cpu_16(resp->max_rx_rings);
		vf->max_l2_ctx = rte_le_to_cpu_16(resp->max_l2_ctxs);
		vf->max_vnics = rte_le_to_cpu_16(resp->max_vnics);
	}

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

int bnxt_hwrm_func_driver_register(struct bnxt *bp, uint32_t flags,
				   uint32_t *vf_req_fwd)
{
	int rc;
	struct hwrm_func_drv_rgtr_input req = {.req_type = 0 };
	struct hwrm_func_drv_rgtr_output *resp = bp->hwrm_cmd_resp_addr;

	if (bp->flags & BNXT_FLAG_REGISTERED)
		return 0;

	HWRM_PREP(req, FUNC_DRV_RGTR, -1, resp);
	req.flags = flags;
	req.enables = HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VER |
			HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_ASYNC_EVENT_FWD;
	req.ver_maj = RTE_VER_YEAR;
	req.ver_min = RTE_VER_MONTH;
	req.ver_upd = RTE_VER_MINOR;

	memcpy(req.vf_req_fwd, vf_req_fwd, sizeof(req.vf_req_fwd));

	req.async_event_fwd[0] |= rte_cpu_to_le_32(0x1);   /* TODO: Use MACRO */

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
	bp->max_req_len = resp->max_req_win_len;
	max_resp_len = resp->max_resp_len;
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
		bp->hwrm_cmd_resp_dma_addr =
			rte_malloc_virt2phy(bp->hwrm_cmd_resp_addr);
		bp->max_resp_len = max_resp_len;
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

	HWRM_PREP(req, PORT_PHY_CFG, -1, resp);

	if (conf->link_up) {
		req.flags = rte_cpu_to_le_32(conf->phy_flags);
		req.force_link_speed = rte_cpu_to_le_16(conf->link_speed);
		/*
		 * Note, ChiMP FW 20.2.1 and 20.2.2 return an error when we set
		 * any auto mode, even "none".
		 */
		if (!conf->link_speed) {
			req.auto_mode |= conf->auto_mode;
			enables = HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_MODE;
			req.auto_link_speed_mask = conf->auto_link_speed_mask;
			enables |=
			   HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED_MASK;
			req.auto_link_speed = bp->link_info.auto_link_speed;
			enables |=
				HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED;
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
		rte_cpu_to_le_32(HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE_LINK_DOWN);
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
	if (link_info->phy_link_status != HWRM_PORT_PHY_QCFG_OUTPUT_LINK_NO_LINK) {
		link_info->link_up = 1;
		link_info->link_speed = rte_le_to_cpu_16(resp->link_speed);
	} else {
		link_info->link_up = 0;
		link_info->link_speed = 0;
	}
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
			 uint32_t stats_ctx_id)
{
	int rc = 0;
	struct hwrm_ring_alloc_input req = {.req_type = 0 };
	struct hwrm_ring_alloc_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, RING_ALLOC, -1, resp);

	req.enables = rte_cpu_to_le_32(0);

	req.page_tbl_addr = rte_cpu_to_le_64(ring->bd_dma);
	req.fbo = rte_cpu_to_le_32(0);
	/* Association of ring index with doorbell index */
	req.logical_id = rte_cpu_to_le_16(map_index);

	switch (ring_type) {
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_TX:
		req.queue_id = bp->cos_queue[0].id;
		/* FALLTHROUGH */
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX:
		req.ring_type = ring_type;
		req.cmpl_ring_id =
		    rte_cpu_to_le_16(bp->grp_info[map_index].cp_fw_ring_id);
		req.length = rte_cpu_to_le_32(ring->ring_size);
		req.stat_ctx_id = rte_cpu_to_le_16(stats_ctx_id);
		req.enables = rte_cpu_to_le_32(rte_le_to_cpu_32(req.enables) |
			HWRM_RING_ALLOC_INPUT_ENABLES_STAT_CTX_ID_VALID);
		break;
	case HWRM_RING_ALLOC_INPUT_RING_TYPE_CMPL:
		req.ring_type = ring_type;
		/*
		 * TODO: Some HWRM versions crash with
		 * HWRM_RING_ALLOC_INPUT_INT_MODE_POLL
		 */
		req.int_mode = HWRM_RING_ALLOC_INPUT_INT_MODE_MSIX;
		req.length = rte_cpu_to_le_32(ring->ring_size);
		break;
	default:
		RTE_LOG(ERR, PMD, "hwrm alloc invalid ring type %d\n",
			ring_type);
		return -1;
	}

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	if (rc || resp->error_code) {
		if (rc == 0 && resp->error_code)
			rc = rte_le_to_cpu_16(resp->error_code);
		switch (ring_type) {
		case HWRM_RING_FREE_INPUT_RING_TYPE_CMPL:
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
		case HWRM_RING_FREE_INPUT_RING_TYPE_CMPL:
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

	HWRM_PREP(req, STAT_CTX_CLR_STATS, -1, resp);

	if (cpr->hw_stats_ctx_id == (uint32_t)HWRM_NA_SIGNATURE)
		return rc;

	req.stat_ctx_id = rte_cpu_to_le_16(cpr->hw_stats_ctx_id);
	req.seq_id = rte_cpu_to_le_16(bp->hwrm_cmd_seq++);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	return rc;
}

int bnxt_hwrm_stat_ctx_alloc(struct bnxt *bp,
			     struct bnxt_cp_ring_info *cpr, unsigned int idx)
{
	int rc;
	struct hwrm_stat_ctx_alloc_input req = {.req_type = 0 };
	struct hwrm_stat_ctx_alloc_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, STAT_CTX_ALLOC, -1, resp);

	req.update_period_ms = rte_cpu_to_le_32(1000);

	req.seq_id = rte_cpu_to_le_16(bp->hwrm_cmd_seq++);
	req.stats_dma_addr =
	    rte_cpu_to_le_64(cpr->hw_stats_map);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	cpr->hw_stats_ctx_id = rte_le_to_cpu_16(resp->stat_ctx_id);
	bp->grp_info[idx].fw_stats_ctx = cpr->hw_stats_ctx_id;

	return rc;
}

int bnxt_hwrm_stat_ctx_free(struct bnxt *bp,
			    struct bnxt_cp_ring_info *cpr, unsigned int idx)
{
	int rc;
	struct hwrm_stat_ctx_free_input req = {.req_type = 0 };
	struct hwrm_stat_ctx_free_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, STAT_CTX_FREE, -1, resp);

	req.stat_ctx_id = rte_cpu_to_le_16(cpr->hw_stats_ctx_id);
	req.seq_id = rte_cpu_to_le_16(bp->hwrm_cmd_seq++);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	cpr->hw_stats_ctx_id = HWRM_NA_SIGNATURE;
	bp->grp_info[idx].fw_stats_ctx = cpr->hw_stats_ctx_id;

	return rc;
}

int bnxt_hwrm_vnic_alloc(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0, i, j;
	struct hwrm_vnic_alloc_input req = { 0 };
	struct hwrm_vnic_alloc_output *resp = bp->hwrm_cmd_resp_addr;

	/* map ring groups to this vnic */
	for (i = vnic->start_grp_id, j = 0; i <= vnic->end_grp_id; i++, j++) {
		if (bp->grp_info[i].fw_grp_id == (uint16_t)HWRM_NA_SIGNATURE) {
			RTE_LOG(ERR, PMD,
				"Not enough ring groups avail:%x req:%x\n", j,
				(vnic->end_grp_id - vnic->start_grp_id) + 1);
			break;
		}
		vnic->fw_grp_ids[j] = bp->grp_info[i].fw_grp_id;
	}

	vnic->fw_rss_cos_lb_ctx = (uint16_t)HWRM_NA_SIGNATURE;
	vnic->ctx_is_rss_cos_lb = HW_CONTEXT_NONE;

	HWRM_PREP(req, VNIC_ALLOC, -1, resp);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	vnic->fw_vnic_id = rte_le_to_cpu_16(resp->vnic_id);
	return rc;
}

int bnxt_hwrm_vnic_cfg(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_cfg_input req = {.req_type = 0 };
	struct hwrm_vnic_cfg_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, VNIC_CFG, -1, resp);

	/* Only RSS support for now TBD: COS & LB */
	req.enables =
	    rte_cpu_to_le_32(HWRM_VNIC_CFG_INPUT_ENABLES_DFLT_RING_GRP |
			     HWRM_VNIC_CFG_INPUT_ENABLES_RSS_RULE |
			     HWRM_VNIC_CFG_INPUT_ENABLES_MRU);
	req.vnic_id = rte_cpu_to_le_16(vnic->fw_vnic_id);
	req.dflt_ring_grp =
		rte_cpu_to_le_16(bp->grp_info[vnic->start_grp_id].fw_grp_id);
	req.rss_rule = rte_cpu_to_le_16(vnic->fw_rss_cos_lb_ctx);
	req.cos_rule = rte_cpu_to_le_16(0xffff);
	req.lb_rule = rte_cpu_to_le_16(0xffff);
	req.mru = rte_cpu_to_le_16(bp->eth_dev->data->mtu + ETHER_HDR_LEN +
				   ETHER_CRC_LEN + VLAN_TAG_SIZE);
	if (vnic->func_default)
		req.flags = 1;
	if (vnic->vlan_strip)
		req.flags |=
		    rte_cpu_to_le_32(HWRM_VNIC_CFG_INPUT_FLAGS_VLAN_STRIP_MODE);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

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

	vnic->fw_rss_cos_lb_ctx = rte_le_to_cpu_16(resp->rss_cos_lb_ctx_id);

	return rc;
}

int bnxt_hwrm_vnic_ctx_free(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_rss_cos_lb_ctx_free_input req = {.req_type = 0 };
	struct hwrm_vnic_rss_cos_lb_ctx_free_output *resp =
						bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, VNIC_RSS_COS_LB_CTX_FREE, -1, resp);

	req.rss_cos_lb_ctx_id = rte_cpu_to_le_16(vnic->fw_rss_cos_lb_ctx);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	vnic->fw_rss_cos_lb_ctx = INVALID_HW_RING_ID;

	return rc;
}

int bnxt_hwrm_vnic_free(struct bnxt *bp, struct bnxt_vnic_info *vnic)
{
	int rc = 0;
	struct hwrm_vnic_free_input req = {.req_type = 0 };
	struct hwrm_vnic_free_output *resp = bp->hwrm_cmd_resp_addr;

	if (vnic->fw_vnic_id == INVALID_HW_RING_ID)
		return rc;

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
	req.rss_ctx_idx = rte_cpu_to_le_16(vnic->fw_rss_cos_lb_ctx);

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
		unsigned int idx = i + 1;

		if (i >= bp->rx_cp_nr_rings)
			cpr = bp->tx_queues[i - bp->rx_cp_nr_rings]->cp_ring;
		else
			cpr = bp->rx_queues[i]->cp_ring;
		if (cpr->hw_stats_ctx_id != HWRM_NA_SIGNATURE) {
			rc = bnxt_hwrm_stat_ctx_free(bp, cpr, idx);
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
		unsigned int idx = i + 1;

		if (i >= bp->rx_cp_nr_rings) {
			txq = bp->tx_queues[i - bp->rx_cp_nr_rings];
			cpr = txq->cp_ring;
		} else {
			rxq = bp->rx_queues[i];
			cpr = rxq->cp_ring;
		}

		rc = bnxt_hwrm_stat_ctx_alloc(bp, cpr, idx);

		if (rc)
			return rc;
	}
	return rc;
}

int bnxt_free_all_hwrm_ring_grps(struct bnxt *bp)
{
	uint16_t i;
	uint32_t rc = 0;

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		unsigned int idx = i + 1;

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

static void bnxt_free_cp_ring(struct bnxt *bp,
			      struct bnxt_cp_ring_info *cpr, unsigned int idx)
{
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;

	bnxt_hwrm_ring_free(bp, cp_ring,
			HWRM_RING_FREE_INPUT_RING_TYPE_CMPL);
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
		if (cpr->cp_ring_struct->fw_ring_id != INVALID_HW_RING_ID)
			bnxt_free_cp_ring(bp, cpr, idx);
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
		}
		if (cpr->cp_ring_struct->fw_ring_id != INVALID_HW_RING_ID)
			bnxt_free_cp_ring(bp, cpr, idx);
	}

	/* Default completion ring */
	{
		struct bnxt_cp_ring_info *cpr = bp->def_cp_ring;

		if (cpr->cp_ring_struct->fw_ring_id != INVALID_HW_RING_ID)
			bnxt_free_cp_ring(bp, cpr, 0);
	}

	return rc;
}

int bnxt_alloc_all_hwrm_ring_grps(struct bnxt *bp)
{
	uint16_t i;
	uint32_t rc = 0;

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		unsigned int idx = i + 1;

		if (bp->grp_info[idx].cp_fw_ring_id == INVALID_HW_RING_ID ||
		    bp->grp_info[idx].rx_fw_ring_id == INVALID_HW_RING_ID)
			continue;

		rc = bnxt_hwrm_ring_grp_alloc(bp, idx);

		if (rc)
			return rc;
	}
	return rc;
}

void bnxt_free_hwrm_resources(struct bnxt *bp)
{
	/* Release memzone */
	rte_free(bp->hwrm_cmd_resp_addr);
	bp->hwrm_cmd_resp_addr = NULL;
	bp->hwrm_cmd_resp_dma_addr = 0;
}

int bnxt_alloc_hwrm_resources(struct bnxt *bp)
{
	struct rte_pci_device *pdev = bp->pdev;
	char type[RTE_MEMZONE_NAMESIZE];

	sprintf(type, "bnxt_hwrm_%04x:%02x:%02x:%02x", pdev->addr.domain,
		pdev->addr.bus, pdev->addr.devid, pdev->addr.function);
	bp->max_req_len = HWRM_MAX_REQ_LEN;
	bp->max_resp_len = HWRM_MAX_RESP_LEN;
	bp->hwrm_cmd_resp_addr = rte_malloc(type, bp->max_resp_len, 0);
	if (bp->hwrm_cmd_resp_addr == NULL)
		return -ENOMEM;
	bp->hwrm_cmd_resp_dma_addr =
		rte_malloc_virt2phy(bp->hwrm_cmd_resp_addr);
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
		rc = bnxt_hwrm_set_filter(bp, vnic, filter);
		if (rc)
			break;
	}
	return rc;
}

void bnxt_free_all_hwrm_resources(struct bnxt *bp)
{
	struct bnxt_vnic_info *vnic;
	unsigned int i;

	if (bp->vnic_info == NULL)
		return;

	vnic = &bp->vnic_info[0];
	bnxt_hwrm_cfa_l2_clear_rx_mask(bp, vnic);

	/* VNIC resources */
	for (i = 0; i < bp->nr_vnics; i++) {
		struct bnxt_vnic_info *vnic = &bp->vnic_info[i];

		bnxt_clear_hwrm_vnic_filters(bp, vnic);

		bnxt_hwrm_vnic_ctx_free(bp, vnic);
		bnxt_hwrm_vnic_free(bp, vnic);
	}
	/* Ring resources */
	bnxt_free_all_hwrm_rings(bp);
	bnxt_free_all_hwrm_ring_grps(bp);
	bnxt_free_all_hwrm_stat_ctxs(bp);
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

static uint16_t bnxt_parse_eth_link_speed_mask(uint32_t link_speed)
{
	uint16_t ret = 0;

	if (link_speed == ETH_LINK_SPEED_AUTONEG)
		link_speed = BNXT_SUPPORTED_SPEEDS;

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
	if (link_info->link_up)
		link->link_speed =
			bnxt_parse_hw_link_speed(link_info->link_speed);
	else
		link->link_speed = ETH_LINK_SPEED_10M;
	link->link_duplex = bnxt_parse_hw_link_duplex(link_info->duplex);
	link->link_status = link_info->link_up;
	link->link_autoneg = link_info->auto_mode ==
		HWRM_PORT_PHY_QCFG_OUTPUT_AUTO_MODE_NONE ?
		ETH_LINK_SPEED_FIXED : ETH_LINK_SPEED_AUTONEG;
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
			bnxt_parse_eth_link_speed_mask(dev_conf->link_speeds);
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

	rte_delay_ms(BNXT_LINK_WAIT_INTERVAL);
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

	if (BNXT_VF(bp)) {
		struct bnxt_vf_info *vf = &bp->vf;

		/* Hard Coded.. 0xfff VLAN ID mask */
		vf->vlan = rte_le_to_cpu_16(resp->vlan) & 0xfff;
	}

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
