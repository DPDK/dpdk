/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#include "bcm_osal.h"
#include "ecore.h"
#include "ecore_hsi_eth.h"
#include "ecore_sriov.h"
#include "ecore_l2_api.h"
#include "ecore_vf.h"
#include "ecore_vfpf_if.h"
#include "ecore_status.h"
#include "reg_addr.h"
#include "ecore_int.h"
#include "ecore_l2.h"
#include "ecore_mcp_api.h"
#include "ecore_vf_api.h"

static void *ecore_vf_pf_prep(struct ecore_hwfn *p_hwfn, u16 type, u16 length)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	void *p_tlv;

	/* This lock is released when we receive PF's response
	 * in ecore_send_msg2pf().
	 * So, ecore_vf_pf_prep() and ecore_send_msg2pf()
	 * must come in sequence.
	 */
	OSAL_MUTEX_ACQUIRE(&p_iov->mutex);

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "preparing to send %s tlv over vf pf channel\n",
		   ecore_channel_tlvs_string[type]);

	/* Reset Request offset */
	p_iov->offset = (u8 *)(p_iov->vf2pf_request);

	/* Clear mailbox - both request and reply */
	OSAL_MEMSET(p_iov->vf2pf_request, 0, sizeof(union vfpf_tlvs));
	OSAL_MEMSET(p_iov->pf2vf_reply, 0, sizeof(union pfvf_tlvs));

	/* Init type and length */
	p_tlv = ecore_add_tlv(p_hwfn, &p_iov->offset, type, length);

	/* Init first tlv header */
	((struct vfpf_first_tlv *)p_tlv)->reply_address =
	    (u64)p_iov->pf2vf_reply_phys;

	return p_tlv;
}

static int ecore_send_msg2pf(struct ecore_hwfn *p_hwfn,
			     u8 *done, u32 resp_size)
{
	struct ustorm_vf_zone *zone_data = (struct ustorm_vf_zone *)
	    ((u8 *)PXP_VF_BAR0_START_USDM_ZONE_B);
	union vfpf_tlvs *p_req = p_hwfn->vf_iov_info->vf2pf_request;
	struct ustorm_trigger_vf_zone trigger;
	int rc = ECORE_SUCCESS, time = 100;
	u8 pf_id;

	/* output tlvs list */
	ecore_dp_tlv_list(p_hwfn, p_req);

	/* need to add the END TLV to the message size */
	resp_size += sizeof(struct channel_list_end_tlv);

	if (!p_hwfn->p_dev->sriov_info.b_hw_channel) {
		rc = OSAL_VF_SEND_MSG2PF(p_hwfn->p_dev,
					 done,
					 p_req,
					 p_hwfn->vf_iov_info->pf2vf_reply,
					 sizeof(union vfpf_tlvs), resp_size);
		/* TODO - no prints about message ? */
		goto exit;
	}

	/* Send TLVs over HW channel */
	OSAL_MEMSET(&trigger, 0, sizeof(struct ustorm_trigger_vf_zone));
	trigger.vf_pf_msg_valid = 1;
	/* TODO - FW should remove this requirement */
	pf_id = GET_FIELD(p_hwfn->hw_info.concrete_fid, PXP_CONCRETE_FID_PFID);

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "VF -> PF [%02x] message: [%08x, %08x] --> %p, %08x --> %p\n",
		   pf_id,
		   U64_HI(p_hwfn->vf_iov_info->vf2pf_request_phys),
		   U64_LO(p_hwfn->vf_iov_info->vf2pf_request_phys),
		   &zone_data->non_trigger.vf_pf_msg_addr,
		   *((u32 *)&trigger), &zone_data->trigger);

	REG_WR(p_hwfn,
	       (osal_uintptr_t)&zone_data->non_trigger.vf_pf_msg_addr.lo,
	       U64_LO(p_hwfn->vf_iov_info->vf2pf_request_phys));

	REG_WR(p_hwfn,
	       (osal_uintptr_t)&zone_data->non_trigger.vf_pf_msg_addr.hi,
	       U64_HI(p_hwfn->vf_iov_info->vf2pf_request_phys));

	/* The message data must be written first, to prevent trigger before
	 * data is written.
	 */
	OSAL_WMB(p_hwfn->p_dev);

	REG_WR(p_hwfn, (osal_uintptr_t)&zone_data->trigger,
	       *((u32 *)&trigger));

	while ((!*done) && time) {
		OSAL_MSLEEP(25);
		time--;
	}

	if (!*done) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "VF <-- PF Timeout [Type %d]\n",
			   p_req->first_tlv.tl.type);
		rc = ECORE_TIMEOUT;
		goto exit;
	} else {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "PF response: %d [Type %d]\n",
			   *done, p_req->first_tlv.tl.type);
	}

exit:
	OSAL_MUTEX_RELEASE(&p_hwfn->vf_iov_info->mutex);

	return rc;
}

#define VF_ACQUIRE_THRESH 3
#define VF_ACQUIRE_MAC_FILTERS 1
#define VF_ACQUIRE_MC_FILTERS 10

static enum _ecore_status_t ecore_vf_pf_acquire(struct ecore_hwfn *p_hwfn)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct pfvf_acquire_resp_tlv *resp = &p_iov->pf2vf_reply->acquire_resp;
	struct pf_vf_pfdev_info *pfdev_info = &resp->pfdev_info;
	struct ecore_vf_acquire_sw_info vf_sw_info;
	struct vfpf_acquire_tlv *req;
	int rc = 0, attempts = 0;
	bool resources_acquired = false;

	/* @@@ TBD: MichalK take this from somewhere else... */
	u8 rx_count = 1, tx_count = 1, num_sbs = 1;
	u8 num_mac = VF_ACQUIRE_MAC_FILTERS, num_mc = VF_ACQUIRE_MC_FILTERS;

	/* clear mailbox and prep first tlv */
	req = ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_ACQUIRE, sizeof(*req));

	/* @@@ TBD: PF may not be ready bnx2x_get_vf_id... */
	req->vfdev_info.opaque_fid = p_hwfn->hw_info.opaque_fid;

	req->resc_request.num_rxqs = rx_count;
	req->resc_request.num_txqs = tx_count;
	req->resc_request.num_sbs = num_sbs;
	req->resc_request.num_mac_filters = num_mac;
	req->resc_request.num_mc_filters = num_mc;
	req->resc_request.num_vlan_filters = ECORE_ETH_VF_NUM_VLAN_FILTERS;

	OSAL_MEMSET(&vf_sw_info, 0, sizeof(vf_sw_info));
	OSAL_VF_FILL_ACQUIRE_RESC_REQ(p_hwfn, &req->resc_request, &vf_sw_info);

	req->vfdev_info.os_type = vf_sw_info.os_type;
	req->vfdev_info.driver_version = vf_sw_info.driver_version;
	req->vfdev_info.fw_major = FW_MAJOR_VERSION;
	req->vfdev_info.fw_minor = FW_MINOR_VERSION;
	req->vfdev_info.fw_revision = FW_REVISION_VERSION;
	req->vfdev_info.fw_engineering = FW_ENGINEERING_VERSION;

	if (vf_sw_info.override_fw_version)
		req->vfdev_info.capabilties |= VFPF_ACQUIRE_CAP_OVERRIDE_FW_VER;

	/* pf 2 vf bulletin board address */
	req->bulletin_addr = p_iov->bulletin.phys;
	req->bulletin_size = p_iov->bulletin.size;

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	while (!resources_acquired) {
		DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
			   "attempting to acquire resources\n");

		/* send acquire request */
		rc = ecore_send_msg2pf(p_hwfn,
				       &resp->hdr.status, sizeof(*resp));

		/* PF timeout */
		if (rc)
			return rc;

		/* copy acquire response from buffer to p_hwfn */
		OSAL_MEMCPY(&p_iov->acquire_resp,
			    resp, sizeof(p_iov->acquire_resp));

		attempts++;

		/* PF agrees to allocate our resources */
		if (resp->hdr.status == PFVF_STATUS_SUCCESS) {
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "resources acquired\n");
			resources_acquired = true;
		} /* PF refuses to allocate our resources */
		else if (resp->hdr.status ==
			 PFVF_STATUS_NO_RESOURCE &&
			 attempts < VF_ACQUIRE_THRESH) {
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "PF unwilling to fullfill resource request. Try PF recommended amount\n");

			/* humble our request */
			req->resc_request.num_txqs = resp->resc.num_txqs;
			req->resc_request.num_rxqs = resp->resc.num_rxqs;
			req->resc_request.num_sbs = resp->resc.num_sbs;
			req->resc_request.num_mac_filters =
			    resp->resc.num_mac_filters;
			req->resc_request.num_vlan_filters =
			    resp->resc.num_vlan_filters;
			req->resc_request.num_mc_filters =
			    resp->resc.num_mc_filters;

			/* Clear response buffer */
			OSAL_MEMSET(p_iov->pf2vf_reply, 0,
				    sizeof(union pfvf_tlvs));
		} else {
			DP_ERR(p_hwfn,
			       "PF returned error %d to VF acquisition request\n",
			       resp->hdr.status);
			return ECORE_AGAIN;
		}
	}

	rc = OSAL_VF_UPDATE_ACQUIRE_RESC_RESP(p_hwfn, &resp->resc);
	if (rc) {
		DP_NOTICE(p_hwfn, true,
			  "VF_UPDATE_ACQUIRE_RESC_RESP Failed: status = 0x%x.\n",
			  rc);
		return ECORE_AGAIN;
	}

	/* Update bulletin board size with response from PF */
	p_iov->bulletin.size = resp->bulletin_size;

	/* get HW info */
	p_hwfn->p_dev->type = resp->pfdev_info.dev_type;
	p_hwfn->p_dev->chip_rev = resp->pfdev_info.chip_rev;

	DP_INFO(p_hwfn, "Chip details - %s%d\n",
		ECORE_IS_BB(p_hwfn->p_dev) ? "BB" : "AH",
		CHIP_REV_IS_A0(p_hwfn->p_dev) ? 0 : 1);

	/* @@@TBD MichalK: Fw ver... */
	/* strlcpy(p_hwfn->fw_ver, p_hwfn->acquire_resp.pfdev_info.fw_ver,
	 *  sizeof(p_hwfn->fw_ver));
	 */

	p_hwfn->p_dev->chip_num = pfdev_info->chip_num & 0xffff;

	return 0;
}

enum _ecore_status_t ecore_vf_hw_prepare(struct ecore_dev *p_dev)
{
	enum _ecore_status_t rc = ECORE_NOMEM;
	struct ecore_vf_iov *p_sriov;
	struct ecore_hwfn *p_hwfn = &p_dev->hwfns[0];	/* @@@TBD CMT */

	p_dev->num_hwfns = 1;	/* @@@TBD CMT must be fixed... */

	p_hwfn->regview = p_dev->regview;
	if (p_hwfn->regview == OSAL_NULL) {
		DP_ERR(p_hwfn,
		       "regview should be initialized before"
			" ecore_vf_hw_prepare is called\n");
		return ECORE_INVAL;
	}

	/* Set the doorbell bar. Assumption: regview is set */
	p_hwfn->doorbells = (u8 OSAL_IOMEM *)p_hwfn->regview +
	    PXP_VF_BAR0_START_DQ;

	p_hwfn->hw_info.opaque_fid = (u16)REG_RD(p_hwfn,
					  PXP_VF_BAR0_ME_OPAQUE_ADDRESS);

	p_hwfn->hw_info.concrete_fid = REG_RD(p_hwfn,
				      PXP_VF_BAR0_ME_CONCRETE_ADDRESS);

	/* Allocate vf sriov info */
	p_sriov = OSAL_ZALLOC(p_hwfn->p_dev, GFP_KERNEL, sizeof(*p_sriov));
	if (!p_sriov) {
		DP_NOTICE(p_hwfn, true,
			  "Failed to allocate `struct ecore_sriov'\n");
		return ECORE_NOMEM;
	}

	OSAL_MEMSET(p_sriov, 0, sizeof(*p_sriov));

	/* Allocate vf2pf msg */
	p_sriov->vf2pf_request = OSAL_DMA_ALLOC_COHERENT(p_hwfn->p_dev,
							 &p_sriov->
							 vf2pf_request_phys,
							 sizeof(union
								vfpf_tlvs));
	if (!p_sriov->vf2pf_request) {
		DP_NOTICE(p_hwfn, true,
			  "Failed to allocate `vf2pf_request' DMA memory\n");
		goto free_p_sriov;
	}

	p_sriov->pf2vf_reply = OSAL_DMA_ALLOC_COHERENT(p_hwfn->p_dev,
						       &p_sriov->
						       pf2vf_reply_phys,
						       sizeof(union pfvf_tlvs));
	if (!p_sriov->pf2vf_reply) {
		DP_NOTICE(p_hwfn, true,
			  "Failed to allocate `pf2vf_reply' DMA memory\n");
		goto free_vf2pf_request;
	}

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "VF's Request mailbox [%p virt 0x%" PRIx64 " phys], "
		   "Response mailbox [%p virt 0x%" PRIx64 " phys]\n",
		   p_sriov->vf2pf_request,
		   (u64)p_sriov->vf2pf_request_phys,
		   p_sriov->pf2vf_reply, (u64)p_sriov->pf2vf_reply_phys);

	/* Allocate Bulletin board */
	p_sriov->bulletin.size = sizeof(struct ecore_bulletin_content);
	p_sriov->bulletin.p_virt = OSAL_DMA_ALLOC_COHERENT(p_hwfn->p_dev,
							   &p_sriov->bulletin.
							   phys,
							   p_sriov->bulletin.
							   size);
	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "VF's bulletin Board [%p virt 0x%" PRIx64 " phys 0x%08x bytes]\n",
		   p_sriov->bulletin.p_virt, (u64)p_sriov->bulletin.phys,
		   p_sriov->bulletin.size);

	OSAL_MUTEX_ALLOC(p_hwfn, &p_sriov->mutex);
	OSAL_MUTEX_INIT(&p_sriov->mutex);

	p_hwfn->vf_iov_info = p_sriov;

	p_hwfn->hw_info.personality = ECORE_PCI_ETH;

	/* First VF needs to query for information from PF */
	if (!p_hwfn->my_id)
		rc = ecore_vf_pf_acquire(p_hwfn);

	return rc;

free_vf2pf_request:
	OSAL_DMA_FREE_COHERENT(p_hwfn->p_dev, p_sriov->vf2pf_request,
			       p_sriov->vf2pf_request_phys,
			       sizeof(union vfpf_tlvs));
free_p_sriov:
	OSAL_FREE(p_hwfn->p_dev, p_sriov);

	return rc;
}

enum _ecore_status_t ecore_vf_pf_init(struct ecore_hwfn *p_hwfn)
{
	p_hwfn->b_int_enabled = 1;

	return 0;
}

/* TEMP TEMP until in HSI */
#define TSTORM_QZONE_START   PXP_VF_BAR0_START_SDM_ZONE_A
#define MSTORM_QZONE_START(dev)   (TSTORM_QZONE_START + \
				   (TSTORM_QZONE_SIZE * NUM_OF_L2_QUEUES(dev)))
#define USTORM_QZONE_START(dev)   (MSTORM_QZONE_START + \
				   (MSTORM_QZONE_SIZE * NUM_OF_L2_QUEUES(dev)))

enum _ecore_status_t ecore_vf_pf_rxq_start(struct ecore_hwfn *p_hwfn,
					   u8 rx_qid,
					   u16 sb,
					   u8 sb_index,
					   u16 bd_max_bytes,
					   dma_addr_t bd_chain_phys_addr,
					   dma_addr_t cqe_pbl_addr,
					   u16 cqe_pbl_size,
					   void OSAL_IOMEM * *pp_prod)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct vfpf_start_rxq_tlv *req;
	struct pfvf_def_resp_tlv *resp = &p_iov->pf2vf_reply->default_resp;
	int rc;
	u8 hw_qid;
	u64 init_prod_val = 0;

	/* clear mailbox and prep first tlv */
	req = ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_START_RXQ, sizeof(*req));

	/* @@@TBD MichalK TPA */

	req->rx_qid = rx_qid;
	req->cqe_pbl_addr = cqe_pbl_addr;
	req->cqe_pbl_size = cqe_pbl_size;
	req->rxq_addr = bd_chain_phys_addr;
	req->hw_sb = sb;
	req->sb_index = sb_index;
	req->hc_rate = 0;	/* @@@TBD MichalK -> host coalescing! */
	req->bd_max_bytes = bd_max_bytes;
	req->stat_id = -1;	/* No stats at the moment */

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	if (pp_prod) {
		hw_qid = p_iov->acquire_resp.resc.hw_qid[rx_qid];

		*pp_prod = (u8 OSAL_IOMEM *)p_hwfn->regview +
		    MSTORM_QZONE_START(p_hwfn->p_dev) +
		    (hw_qid) * MSTORM_QZONE_SIZE +
		    OFFSETOF(struct mstorm_eth_queue_zone, rx_producers);

		/* Init the rcq, rx bd and rx sge (if valid) producers to 0 */
		__internal_ram_wr(p_hwfn, *pp_prod, sizeof(u64),
				  (u32 *)(&init_prod_val));
	}

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		return rc;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		return ECORE_INVAL;

	return rc;
}

enum _ecore_status_t ecore_vf_pf_rxq_stop(struct ecore_hwfn *p_hwfn,
					  u16 rx_qid, bool cqe_completion)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct vfpf_stop_rxqs_tlv *req;
	struct pfvf_def_resp_tlv *resp = &p_iov->pf2vf_reply->default_resp;
	int rc;

	/* clear mailbox and prep first tlv */
	req = ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_STOP_RXQS, sizeof(*req));

	/* @@@TBD MichalK TPA */

	/* @@@TBD MichalK - relevant ???
	 * flags  VFPF_QUEUE_FLG_OV VFPF_QUEUE_FLG_VLAN
	 */
	req->rx_qid = rx_qid;
	req->num_rxqs = 1;
	req->cqe_completion = cqe_completion;

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		return rc;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		return ECORE_INVAL;

	return rc;
}

enum _ecore_status_t ecore_vf_pf_txq_start(struct ecore_hwfn *p_hwfn,
					   u16 tx_queue_id,
					   u16 sb,
					   u8 sb_index,
					   dma_addr_t pbl_addr,
					   u16 pbl_size,
					   void OSAL_IOMEM * *pp_doorbell)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct vfpf_start_txq_tlv *req;
	struct pfvf_def_resp_tlv *resp = &p_iov->pf2vf_reply->default_resp;
	int rc;

	/* clear mailbox and prep first tlv */
	req = ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_START_TXQ, sizeof(*req));

	/* @@@TBD MichalK TPA */

	req->tx_qid = tx_queue_id;

	/* Tx */
	req->pbl_addr = pbl_addr;
	req->pbl_size = pbl_size;
	req->hw_sb = sb;
	req->sb_index = sb_index;
	req->hc_rate = 0;	/* @@@TBD MichalK -> host coalescing! */
	req->flags = 0;		/* @@@TBD MichalK -> flags... */

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		return rc;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		return ECORE_INVAL;

	if (pp_doorbell) {
		u8 cid = p_iov->acquire_resp.resc.cid[tx_queue_id];

		*pp_doorbell = (u8 OSAL_IOMEM *)p_hwfn->doorbells +
		    DB_ADDR_VF(cid, DQ_DEMS_LEGACY);
	}

	return rc;
}

enum _ecore_status_t ecore_vf_pf_txq_stop(struct ecore_hwfn *p_hwfn, u16 tx_qid)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct vfpf_stop_txqs_tlv *req;
	struct pfvf_def_resp_tlv *resp = &p_iov->pf2vf_reply->default_resp;
	int rc;

	/* clear mailbox and prep first tlv */
	req = ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_STOP_TXQS, sizeof(*req));

	/* @@@TBD MichalK TPA */

	/* @@@TBD MichalK - relevant ??? flags
	 * VFPF_QUEUE_FLG_OV VFPF_QUEUE_FLG_VLAN
	 */
	req->tx_qid = tx_qid;
	req->num_txqs = 1;

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		return rc;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		return ECORE_INVAL;

	return rc;
}

enum _ecore_status_t ecore_vf_pf_rxqs_update(struct ecore_hwfn *p_hwfn,
					     u16 rx_queue_id,
					     u8 num_rxqs,
					     u8 comp_cqe_flg, u8 comp_event_flg)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct pfvf_def_resp_tlv *resp = &p_iov->pf2vf_reply->default_resp;
	struct vfpf_update_rxq_tlv *req;
	int rc;

	/* clear mailbox and prep first tlv */
	req = ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_UPDATE_RXQ, sizeof(*req));

	req->rx_qid = rx_queue_id;
	req->num_rxqs = num_rxqs;

	if (comp_cqe_flg)
		req->flags |= VFPF_RXQ_UPD_COMPLETE_CQE_FLAG;
	if (comp_event_flg)
		req->flags |= VFPF_RXQ_UPD_COMPLETE_EVENT_FLAG;

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		return rc;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		return ECORE_INVAL;

	return rc;
}

enum _ecore_status_t
ecore_vf_pf_vport_start(struct ecore_hwfn *p_hwfn, u8 vport_id,
			u16 mtu, u8 inner_vlan_removal,
			enum ecore_tpa_mode tpa_mode, u8 max_buffers_per_cqe,
			u8 only_untagged)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct vfpf_vport_start_tlv *req;
	struct pfvf_def_resp_tlv *resp = &p_iov->pf2vf_reply->default_resp;
	int rc, i;

	/* clear mailbox and prep first tlv */
	req = ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_VPORT_START, sizeof(*req));

	req->mtu = mtu;
	req->vport_id = vport_id;
	req->inner_vlan_removal = inner_vlan_removal;
	req->tpa_mode = tpa_mode;
	req->max_buffers_per_cqe = max_buffers_per_cqe;
	req->only_untagged = only_untagged;

	/* status blocks */
	for (i = 0; i < p_hwfn->vf_iov_info->acquire_resp.resc.num_sbs; i++)
		if (p_hwfn->sbs_info[i])
			req->sb_addr[i] = p_hwfn->sbs_info[i]->sb_phys;

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		return rc;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		return ECORE_INVAL;

	return rc;
}

enum _ecore_status_t ecore_vf_pf_vport_stop(struct ecore_hwfn *p_hwfn)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct pfvf_def_resp_tlv *resp = &p_iov->pf2vf_reply->default_resp;
	int rc;

	/* clear mailbox and prep first tlv */
	ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_VPORT_TEARDOWN,
			 sizeof(struct vfpf_first_tlv));

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		return rc;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		return ECORE_INVAL;

	return rc;
}

static void
ecore_vf_handle_vp_update_tlvs_resp(struct ecore_hwfn *p_hwfn,
				    struct ecore_sp_vport_update_params *p_data)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct pfvf_def_resp_tlv *p_resp;
	u16 tlv;

	if (p_data->update_vport_active_rx_flg ||
	    p_data->update_vport_active_tx_flg) {
		tlv = CHANNEL_TLV_VPORT_UPDATE_ACTIVATE;
		p_resp = (struct pfvf_def_resp_tlv *)
		    ecore_iov_search_list_tlvs(p_hwfn, p_iov->pf2vf_reply, tlv);
		if (p_resp && p_resp->hdr.status)
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VP update activate tlv configured\n");
		else
			DP_NOTICE(p_hwfn, true,
				  "VP update activate tlv config failed\n");
	}

	if (p_data->update_tx_switching_flg) {
		tlv = CHANNEL_TLV_VPORT_UPDATE_TX_SWITCH;
		p_resp = (struct pfvf_def_resp_tlv *)
		    ecore_iov_search_list_tlvs(p_hwfn, p_iov->pf2vf_reply, tlv);
		if (p_resp && p_resp->hdr.status)
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VP update tx switch tlv configured\n");
#ifndef ASIC_ONLY
		else if (CHIP_REV_IS_FPGA(p_hwfn->p_dev))
			DP_NOTICE(p_hwfn, false,
				  "FPGA: Skip checking whether PF"
				  " replied to Tx-switching request\n");
#endif
		else
			DP_NOTICE(p_hwfn, true,
				  "VP update tx switch tlv config failed\n");
	}

	if (p_data->update_inner_vlan_removal_flg) {
		tlv = CHANNEL_TLV_VPORT_UPDATE_VLAN_STRIP;
		p_resp = (struct pfvf_def_resp_tlv *)
		    ecore_iov_search_list_tlvs(p_hwfn, p_iov->pf2vf_reply, tlv);
		if (p_resp && p_resp->hdr.status)
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VP update vlan strip tlv configured\n");
		else
			DP_NOTICE(p_hwfn, true,
				  "VP update vlan strip tlv config failed\n");
	}

	if (p_data->update_approx_mcast_flg) {
		tlv = CHANNEL_TLV_VPORT_UPDATE_MCAST;
		p_resp = (struct pfvf_def_resp_tlv *)
		    ecore_iov_search_list_tlvs(p_hwfn, p_iov->pf2vf_reply, tlv);
		if (p_resp && p_resp->hdr.status)
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VP update mcast tlv configured\n");
		else
			DP_NOTICE(p_hwfn, true,
				  "VP update mcast tlv config failed\n");
	}

	if (p_data->accept_flags.update_rx_mode_config ||
	    p_data->accept_flags.update_tx_mode_config) {
		tlv = CHANNEL_TLV_VPORT_UPDATE_ACCEPT_PARAM;
		p_resp = (struct pfvf_def_resp_tlv *)
		    ecore_iov_search_list_tlvs(p_hwfn, p_iov->pf2vf_reply, tlv);
		if (p_resp && p_resp->hdr.status)
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VP update accept_mode tlv configured\n");
		else
			DP_NOTICE(p_hwfn, true,
				  "VP update accept_mode tlv config failed\n");
	}

	if (p_data->rss_params) {
		tlv = CHANNEL_TLV_VPORT_UPDATE_RSS;
		p_resp = (struct pfvf_def_resp_tlv *)
		    ecore_iov_search_list_tlvs(p_hwfn, p_iov->pf2vf_reply, tlv);
		if (p_resp && p_resp->hdr.status)
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VP update rss tlv configured\n");
		else
			DP_NOTICE(p_hwfn, true,
				  "VP update rss tlv config failed\n");
	}

	if (p_data->sge_tpa_params) {
		tlv = CHANNEL_TLV_VPORT_UPDATE_SGE_TPA;
		p_resp = (struct pfvf_def_resp_tlv *)
		    ecore_iov_search_list_tlvs(p_hwfn, p_iov->pf2vf_reply, tlv);
		if (p_resp && p_resp->hdr.status)
			DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
				   "VP update sge tpa tlv configured\n");
		else
			DP_NOTICE(p_hwfn, true,
				  "VP update sge tpa tlv config failed\n");
	}
}

enum _ecore_status_t
ecore_vf_pf_vport_update(struct ecore_hwfn *p_hwfn,
			 struct ecore_sp_vport_update_params *p_params)
{
	struct vfpf_vport_update_accept_any_vlan_tlv *p_any_vlan_tlv;
	struct vfpf_vport_update_accept_param_tlv *p_accept_tlv;
	struct vfpf_vport_update_tx_switch_tlv *p_tx_switch_tlv;
	struct vfpf_vport_update_mcast_bin_tlv *p_mcast_tlv;
	struct vfpf_vport_update_vlan_strip_tlv *p_vlan_tlv;
	struct vfpf_vport_update_sge_tpa_tlv *p_sge_tpa_tlv;
	struct vfpf_vport_update_activate_tlv *p_act_tlv;
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct vfpf_vport_update_rss_tlv *p_rss_tlv;
	struct vfpf_vport_update_tlv *req;
	struct pfvf_def_resp_tlv *resp;
	u8 update_rx, update_tx;
	u32 resp_size = 0;
	u16 size, tlv;
	int rc;

	resp = &p_iov->pf2vf_reply->default_resp;
	resp_size = sizeof(*resp);

	update_rx = p_params->update_vport_active_rx_flg;
	update_tx = p_params->update_vport_active_tx_flg;

	/* clear mailbox and prep header tlv */
	ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_VPORT_UPDATE, sizeof(*req));

	/* Prepare extended tlvs */
	if (update_rx || update_tx) {
		size = sizeof(struct vfpf_vport_update_activate_tlv);
		p_act_tlv = ecore_add_tlv(p_hwfn, &p_iov->offset,
					  CHANNEL_TLV_VPORT_UPDATE_ACTIVATE,
					  size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		if (update_rx) {
			p_act_tlv->update_rx = update_rx;
			p_act_tlv->active_rx = p_params->vport_active_rx_flg;
		}

		if (update_tx) {
			p_act_tlv->update_tx = update_tx;
			p_act_tlv->active_tx = p_params->vport_active_tx_flg;
		}
	}

	if (p_params->update_inner_vlan_removal_flg) {
		size = sizeof(struct vfpf_vport_update_vlan_strip_tlv);
		p_vlan_tlv = ecore_add_tlv(p_hwfn, &p_iov->offset,
					   CHANNEL_TLV_VPORT_UPDATE_VLAN_STRIP,
					   size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		p_vlan_tlv->remove_vlan = p_params->inner_vlan_removal_flg;
	}

	if (p_params->update_tx_switching_flg) {
		size = sizeof(struct vfpf_vport_update_tx_switch_tlv);
		tlv = CHANNEL_TLV_VPORT_UPDATE_TX_SWITCH;
		p_tx_switch_tlv = ecore_add_tlv(p_hwfn, &p_iov->offset,
						tlv, size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		p_tx_switch_tlv->tx_switching = p_params->tx_switching_flg;
	}

	if (p_params->update_approx_mcast_flg) {
		size = sizeof(struct vfpf_vport_update_mcast_bin_tlv);
		p_mcast_tlv = ecore_add_tlv(p_hwfn, &p_iov->offset,
					    CHANNEL_TLV_VPORT_UPDATE_MCAST,
					    size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		OSAL_MEMCPY(p_mcast_tlv->bins, p_params->bins,
			    sizeof(unsigned long) *
			    ETH_MULTICAST_MAC_BINS_IN_REGS);
	}

	update_rx = p_params->accept_flags.update_rx_mode_config;
	update_tx = p_params->accept_flags.update_tx_mode_config;

	if (update_rx || update_tx) {
		tlv = CHANNEL_TLV_VPORT_UPDATE_ACCEPT_PARAM;
		size = sizeof(struct vfpf_vport_update_accept_param_tlv);
		p_accept_tlv = ecore_add_tlv(p_hwfn, &p_iov->offset, tlv, size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		if (update_rx) {
			p_accept_tlv->update_rx_mode = update_rx;
			p_accept_tlv->rx_accept_filter =
			    p_params->accept_flags.rx_accept_filter;
		}

		if (update_tx) {
			p_accept_tlv->update_tx_mode = update_tx;
			p_accept_tlv->tx_accept_filter =
			    p_params->accept_flags.tx_accept_filter;
		}
	}

	if (p_params->rss_params) {
		struct ecore_rss_params *rss_params = p_params->rss_params;

		size = sizeof(struct vfpf_vport_update_rss_tlv);
		p_rss_tlv = ecore_add_tlv(p_hwfn, &p_iov->offset,
					  CHANNEL_TLV_VPORT_UPDATE_RSS, size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		if (rss_params->update_rss_config)
			p_rss_tlv->update_rss_flags |=
			    VFPF_UPDATE_RSS_CONFIG_FLAG;
		if (rss_params->update_rss_capabilities)
			p_rss_tlv->update_rss_flags |=
			    VFPF_UPDATE_RSS_CAPS_FLAG;
		if (rss_params->update_rss_ind_table)
			p_rss_tlv->update_rss_flags |=
			    VFPF_UPDATE_RSS_IND_TABLE_FLAG;
		if (rss_params->update_rss_key)
			p_rss_tlv->update_rss_flags |= VFPF_UPDATE_RSS_KEY_FLAG;

		p_rss_tlv->rss_enable = rss_params->rss_enable;
		p_rss_tlv->rss_caps = rss_params->rss_caps;
		p_rss_tlv->rss_table_size_log = rss_params->rss_table_size_log;
		OSAL_MEMCPY(p_rss_tlv->rss_ind_table, rss_params->rss_ind_table,
			    sizeof(rss_params->rss_ind_table));
		OSAL_MEMCPY(p_rss_tlv->rss_key, rss_params->rss_key,
			    sizeof(rss_params->rss_key));
	}

	if (p_params->update_accept_any_vlan_flg) {
		size = sizeof(struct vfpf_vport_update_accept_any_vlan_tlv);
		tlv = CHANNEL_TLV_VPORT_UPDATE_ACCEPT_ANY_VLAN;
		p_any_vlan_tlv = ecore_add_tlv(p_hwfn, &p_iov->offset,
					       tlv, size);

		resp_size += sizeof(struct pfvf_def_resp_tlv);
		p_any_vlan_tlv->accept_any_vlan = p_params->accept_any_vlan;
		p_any_vlan_tlv->update_accept_any_vlan_flg =
		    p_params->update_accept_any_vlan_flg;
	}

	if (p_params->sge_tpa_params) {
		struct ecore_sge_tpa_params *sge_tpa_params =
		    p_params->sge_tpa_params;

		size = sizeof(struct vfpf_vport_update_sge_tpa_tlv);
		p_sge_tpa_tlv = ecore_add_tlv(p_hwfn, &p_iov->offset,
					      CHANNEL_TLV_VPORT_UPDATE_SGE_TPA,
					      size);
		resp_size += sizeof(struct pfvf_def_resp_tlv);

		if (sge_tpa_params->update_tpa_en_flg)
			p_sge_tpa_tlv->update_sge_tpa_flags |=
			    VFPF_UPDATE_TPA_EN_FLAG;
		if (sge_tpa_params->update_tpa_param_flg)
			p_sge_tpa_tlv->update_sge_tpa_flags |=
			    VFPF_UPDATE_TPA_PARAM_FLAG;

		if (sge_tpa_params->tpa_ipv4_en_flg)
			p_sge_tpa_tlv->sge_tpa_flags |= VFPF_TPA_IPV4_EN_FLAG;
		if (sge_tpa_params->tpa_ipv6_en_flg)
			p_sge_tpa_tlv->sge_tpa_flags |= VFPF_TPA_IPV6_EN_FLAG;
		if (sge_tpa_params->tpa_pkt_split_flg)
			p_sge_tpa_tlv->sge_tpa_flags |= VFPF_TPA_PKT_SPLIT_FLAG;
		if (sge_tpa_params->tpa_hdr_data_split_flg)
			p_sge_tpa_tlv->sge_tpa_flags |=
			    VFPF_TPA_HDR_DATA_SPLIT_FLAG;
		if (sge_tpa_params->tpa_gro_consistent_flg)
			p_sge_tpa_tlv->sge_tpa_flags |=
			    VFPF_TPA_GRO_CONSIST_FLAG;

		p_sge_tpa_tlv->tpa_max_aggs_num =
		    sge_tpa_params->tpa_max_aggs_num;
		p_sge_tpa_tlv->tpa_max_size = sge_tpa_params->tpa_max_size;
		p_sge_tpa_tlv->tpa_min_size_to_start =
		    sge_tpa_params->tpa_min_size_to_start;
		p_sge_tpa_tlv->tpa_min_size_to_cont =
		    sge_tpa_params->tpa_min_size_to_cont;

		p_sge_tpa_tlv->max_buffers_per_cqe =
		    sge_tpa_params->max_buffers_per_cqe;
	}

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, resp_size);
	if (rc)
		return rc;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		return ECORE_INVAL;

	ecore_vf_handle_vp_update_tlvs_resp(p_hwfn, p_params);

	return rc;
}

enum _ecore_status_t ecore_vf_pf_reset(struct ecore_hwfn *p_hwfn)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct vfpf_first_tlv *req;
	struct pfvf_def_resp_tlv *resp = &p_iov->pf2vf_reply->default_resp;
	int rc;

	/* clear mailbox and prep first tlv */
	req = ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_CLOSE, sizeof(*req));

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		return rc;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		return ECORE_AGAIN;

	p_hwfn->b_int_enabled = 0;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_vf_pf_release(struct ecore_hwfn *p_hwfn)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct vfpf_first_tlv *req;
	struct pfvf_def_resp_tlv *resp = &p_iov->pf2vf_reply->default_resp;
	u32 size;
	int rc;

	/* clear mailbox and prep first tlv */
	req = ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_RELEASE, sizeof(*req));

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));

	if (rc == ECORE_SUCCESS && resp->hdr.status != PFVF_STATUS_SUCCESS)
		rc = ECORE_AGAIN;

	p_hwfn->b_int_enabled = 0;

	/* TODO - might need to revise this for 100g */
	if (IS_LEAD_HWFN(p_hwfn))
		OSAL_MUTEX_DEALLOC(&p_iov->mutex);

	if (p_iov->vf2pf_request)
		OSAL_DMA_FREE_COHERENT(p_hwfn->p_dev,
				       p_iov->vf2pf_request,
				       p_iov->vf2pf_request_phys,
				       sizeof(union vfpf_tlvs));
	if (p_iov->pf2vf_reply)
		OSAL_DMA_FREE_COHERENT(p_hwfn->p_dev,
				       p_iov->pf2vf_reply,
				       p_iov->pf2vf_reply_phys,
				       sizeof(union pfvf_tlvs));

	if (p_iov->bulletin.p_virt) {
		size = sizeof(struct ecore_bulletin_content);
		OSAL_DMA_FREE_COHERENT(p_hwfn->p_dev,
				       p_iov->bulletin.p_virt,
				       p_iov->bulletin.phys, size);
	}

	OSAL_FREE(p_hwfn->p_dev, p_hwfn->vf_iov_info);
	p_hwfn->vf_iov_info = OSAL_NULL;

	return rc;
}

void ecore_vf_pf_filter_mcast(struct ecore_hwfn *p_hwfn,
			      struct ecore_filter_mcast *p_filter_cmd)
{
	struct ecore_sp_vport_update_params sp_params;
	int i;

	OSAL_MEMSET(&sp_params, 0, sizeof(sp_params));
	sp_params.update_approx_mcast_flg = 1;

	if (p_filter_cmd->opcode == ECORE_FILTER_ADD) {
		for (i = 0; i < p_filter_cmd->num_mc_addrs; i++) {
			u32 bit;

			bit = ecore_mcast_bin_from_mac(p_filter_cmd->mac[i]);
			OSAL_SET_BIT(bit, sp_params.bins);
		}
	}

	ecore_vf_pf_vport_update(p_hwfn, &sp_params);
}

enum _ecore_status_t ecore_vf_pf_filter_ucast(struct ecore_hwfn *p_hwfn,
					      struct ecore_filter_ucast
					      *p_ucast)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct vfpf_ucast_filter_tlv *req;
	struct pfvf_def_resp_tlv *resp = &p_iov->pf2vf_reply->default_resp;
	int rc;

	/* Sanitize */
	if (p_ucast->opcode == ECORE_FILTER_MOVE) {
		DP_NOTICE(p_hwfn, true,
			  "VFs don't support Moving of filters\n");
		return ECORE_INVAL;
	}

	/* clear mailbox and prep first tlv */
	req = ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_UCAST_FILTER, sizeof(*req));
	req->opcode = (u8)p_ucast->opcode;
	req->type = (u8)p_ucast->type;
	OSAL_MEMCPY(req->mac, p_ucast->mac, ETH_ALEN);
	req->vlan = p_ucast->vlan;

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		return rc;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		return ECORE_AGAIN;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_vf_pf_int_cleanup(struct ecore_hwfn *p_hwfn)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	struct pfvf_def_resp_tlv *resp = &p_iov->pf2vf_reply->default_resp;
	int rc;

	/* clear mailbox and prep first tlv */
	ecore_vf_pf_prep(p_hwfn, CHANNEL_TLV_INT_CLEANUP,
			 sizeof(struct vfpf_first_tlv));

	/* add list termination tlv */
	ecore_add_tlv(p_hwfn, &p_iov->offset,
		      CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));

	rc = ecore_send_msg2pf(p_hwfn, &resp->hdr.status, sizeof(*resp));
	if (rc)
		return rc;

	if (resp->hdr.status != PFVF_STATUS_SUCCESS)
		return ECORE_INVAL;

	return ECORE_SUCCESS;
}

enum _ecore_status_t ecore_vf_read_bulletin(struct ecore_hwfn *p_hwfn,
					    u8 *p_change)
{
	struct ecore_bulletin_content shadow;
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;
	u32 crc, crc_size = sizeof(p_iov->bulletin.p_virt->crc);

	*p_change = 0;

	/* Need to guarantee PF is not in the middle of writing it */
	OSAL_MEMCPY(&shadow, p_iov->bulletin.p_virt, p_iov->bulletin.size);

	/* If version did not update, no need to do anything */
	if (shadow.version == p_iov->bulletin_shadow.version)
		return ECORE_SUCCESS;

	/* Verify the bulletin we see is valid */
	crc = ecore_crc32(0, (u8 *)&shadow + crc_size,
			  p_iov->bulletin.size - crc_size);
	if (crc != shadow.crc)
		return ECORE_AGAIN;

	/* Set the shadow bulletin and process it */
	OSAL_MEMCPY(&p_iov->bulletin_shadow, &shadow, p_iov->bulletin.size);

	DP_VERBOSE(p_hwfn, ECORE_MSG_IOV,
		   "Read a bulletin update %08x\n", shadow.version);

	*p_change = 1;

	return ECORE_SUCCESS;
}

u16 ecore_vf_get_igu_sb_id(struct ecore_hwfn *p_hwfn, u16 sb_id)
{
	struct ecore_vf_iov *p_iov = p_hwfn->vf_iov_info;

	if (!p_iov) {
		DP_NOTICE(p_hwfn, true, "vf_sriov_info isn't initialized\n");
		return 0;
	}

	return p_iov->acquire_resp.resc.hw_sbs[sb_id].hw_sb_id;
}

void __ecore_vf_get_link_params(struct ecore_hwfn *p_hwfn,
				struct ecore_mcp_link_params *p_params,
				struct ecore_bulletin_content *p_bulletin)
{
	OSAL_MEMSET(p_params, 0, sizeof(*p_params));

	p_params->speed.autoneg = p_bulletin->req_autoneg;
	p_params->speed.advertised_speeds = p_bulletin->req_adv_speed;
	p_params->speed.forced_speed = p_bulletin->req_forced_speed;
	p_params->pause.autoneg = p_bulletin->req_autoneg_pause;
	p_params->pause.forced_rx = p_bulletin->req_forced_rx;
	p_params->pause.forced_tx = p_bulletin->req_forced_tx;
	p_params->loopback_mode = p_bulletin->req_loopback;
}

void ecore_vf_get_link_params(struct ecore_hwfn *p_hwfn,
			      struct ecore_mcp_link_params *params)
{
	__ecore_vf_get_link_params(p_hwfn, params,
				   &p_hwfn->vf_iov_info->bulletin_shadow);
}

void __ecore_vf_get_link_state(struct ecore_hwfn *p_hwfn,
			       struct ecore_mcp_link_state *p_link,
			       struct ecore_bulletin_content *p_bulletin)
{
	OSAL_MEMSET(p_link, 0, sizeof(*p_link));

	p_link->link_up = p_bulletin->link_up;
	p_link->speed = p_bulletin->speed;
	p_link->full_duplex = p_bulletin->full_duplex;
	p_link->an = p_bulletin->autoneg;
	p_link->an_complete = p_bulletin->autoneg_complete;
	p_link->parallel_detection = p_bulletin->parallel_detection;
	p_link->pfc_enabled = p_bulletin->pfc_enabled;
	p_link->partner_adv_speed = p_bulletin->partner_adv_speed;
	p_link->partner_tx_flow_ctrl_en = p_bulletin->partner_tx_flow_ctrl_en;
	p_link->partner_rx_flow_ctrl_en = p_bulletin->partner_rx_flow_ctrl_en;
	p_link->partner_adv_pause = p_bulletin->partner_adv_pause;
	p_link->sfp_tx_fault = p_bulletin->sfp_tx_fault;
}

void ecore_vf_get_link_state(struct ecore_hwfn *p_hwfn,
			     struct ecore_mcp_link_state *link)
{
	__ecore_vf_get_link_state(p_hwfn, link,
				  &p_hwfn->vf_iov_info->bulletin_shadow);
}

void __ecore_vf_get_link_caps(struct ecore_hwfn *p_hwfn,
			      struct ecore_mcp_link_capabilities *p_link_caps,
			      struct ecore_bulletin_content *p_bulletin)
{
	OSAL_MEMSET(p_link_caps, 0, sizeof(*p_link_caps));
	p_link_caps->speed_capabilities = p_bulletin->capability_speed;
}

void ecore_vf_get_link_caps(struct ecore_hwfn *p_hwfn,
			    struct ecore_mcp_link_capabilities *p_link_caps)
{
	__ecore_vf_get_link_caps(p_hwfn, p_link_caps,
				 &p_hwfn->vf_iov_info->bulletin_shadow);
}

void ecore_vf_get_num_rxqs(struct ecore_hwfn *p_hwfn, u8 *num_rxqs)
{
	*num_rxqs = p_hwfn->vf_iov_info->acquire_resp.resc.num_rxqs;
}

void ecore_vf_get_port_mac(struct ecore_hwfn *p_hwfn, u8 *port_mac)
{
	OSAL_MEMCPY(port_mac,
		    p_hwfn->vf_iov_info->acquire_resp.pfdev_info.port_mac,
		    ETH_ALEN);
}

void ecore_vf_get_num_vlan_filters(struct ecore_hwfn *p_hwfn,
				   u8 *num_vlan_filters)
{
	struct ecore_vf_iov *p_vf;

	p_vf = p_hwfn->vf_iov_info;
	*num_vlan_filters = p_vf->acquire_resp.resc.num_vlan_filters;
}

/* @DPDK */
void ecore_vf_get_num_mac_filters(struct ecore_hwfn *p_hwfn,
				  u32 *num_mac)
{
	struct ecore_vf_iov *p_vf;

	p_vf = p_hwfn->vf_iov_info;
	*num_mac = p_vf->acquire_resp.resc.num_mac_filters;
}

bool ecore_vf_check_mac(struct ecore_hwfn *p_hwfn, u8 *mac)
{
	struct ecore_bulletin_content *bulletin;

	bulletin = &p_hwfn->vf_iov_info->bulletin_shadow;
	if (!(bulletin->valid_bitmap & (1 << MAC_ADDR_FORCED)))
		return true;

	/* Forbid VF from changing a MAC enforced by PF */
	if (OSAL_MEMCMP(bulletin->mac, mac, ETH_ALEN))
		return false;

	return false;
}

bool ecore_vf_bulletin_get_forced_mac(struct ecore_hwfn *hwfn, u8 *dst_mac,
				      u8 *p_is_forced)
{
	struct ecore_bulletin_content *bulletin;

	bulletin = &hwfn->vf_iov_info->bulletin_shadow;

	if (bulletin->valid_bitmap & (1 << MAC_ADDR_FORCED)) {
		if (p_is_forced)
			*p_is_forced = 1;
	} else if (bulletin->valid_bitmap & (1 << VFPF_BULLETIN_MAC_ADDR)) {
		if (p_is_forced)
			*p_is_forced = 0;
	} else {
		return false;
	}

	OSAL_MEMCPY(dst_mac, bulletin->mac, ETH_ALEN);

	return true;
}

bool ecore_vf_bulletin_get_forced_vlan(struct ecore_hwfn *hwfn, u16 *dst_pvid)
{
	struct ecore_bulletin_content *bulletin;

	bulletin = &hwfn->vf_iov_info->bulletin_shadow;

	if (!(bulletin->valid_bitmap & (1 << VLAN_ADDR_FORCED)))
		return false;

	if (dst_pvid)
		*dst_pvid = bulletin->pvid;

	return true;
}

void ecore_vf_get_fw_version(struct ecore_hwfn *p_hwfn,
			     u16 *fw_major, u16 *fw_minor, u16 *fw_rev,
			     u16 *fw_eng)
{
	struct pf_vf_pfdev_info *info;

	info = &p_hwfn->vf_iov_info->acquire_resp.pfdev_info;

	*fw_major = info->fw_major;
	*fw_minor = info->fw_minor;
	*fw_rev = info->fw_rev;
	*fw_eng = info->fw_eng;
}
