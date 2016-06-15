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
#include "bnxt_hwrm.h"
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

int bnxt_hwrm_func_qcaps(struct bnxt *bp)
{
	int rc = 0;
	struct hwrm_func_qcaps_input req = {.req_type = 0 };
	struct hwrm_func_qcaps_output *resp = bp->hwrm_cmd_resp_addr;

	HWRM_PREP(req, FUNC_QCAPS, -1, resp);

	req.fid = rte_cpu_to_le_16(0xffff);

	rc = bnxt_hwrm_send_message(bp, &req, sizeof(req));

	HWRM_CHECK_RESULT;

	if (BNXT_PF(bp)) {
		struct bnxt_pf_info *pf = &bp->pf;

		pf->fw_fid = rte_le_to_cpu_32(resp->fid);
		pf->port_id = resp->port_id;
		memcpy(pf->mac_addr, resp->perm_mac_address, ETHER_ADDR_LEN);
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
		memcpy(vf->mac_addr, &resp->perm_mac_address, ETHER_ADDR_LEN);
		vf->max_rsscos_ctx = rte_le_to_cpu_16(resp->max_rsscos_ctx);
		vf->max_cp_rings = rte_le_to_cpu_16(resp->max_cmpl_rings);
		vf->max_tx_rings = rte_le_to_cpu_16(resp->max_tx_rings);
		vf->max_rx_rings = rte_le_to_cpu_16(resp->max_rx_rings);
		vf->max_l2_ctx = rte_le_to_cpu_16(resp->max_l2_ctxs);
		vf->max_vnics = rte_le_to_cpu_16(resp->max_vnics);
	}

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
	req.enables = HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VER;
	req.ver_maj = RTE_VER_YEAR;
	req.ver_min = RTE_VER_MONTH;
	req.ver_upd = RTE_VER_MINOR;

	memcpy(req.vf_req_fwd, vf_req_fwd, sizeof(req.vf_req_fwd));

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

/*
 * HWRM utility functions
 */

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
