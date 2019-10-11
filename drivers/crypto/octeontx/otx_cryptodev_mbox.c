/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#include <unistd.h>

#include "otx_cryptodev_hw_access.h"
#include "otx_cryptodev_mbox.h"

void
otx_cpt_handle_mbox_intr(struct cpt_vf *cptvf)
{
	struct cpt_mbox mbx = {0, 0};

	/*
	 * MBOX[0] contains msg
	 * MBOX[1] contains data
	 */
	mbx.msg  = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				CPTX_VFX_PF_MBOXX(0, 0, 0));
	mbx.data = CPT_READ_CSR(CPT_CSR_REG_BASE(cptvf),
				CPTX_VFX_PF_MBOXX(0, 0, 1));

	CPT_LOG_DP_DEBUG("%s: Mailbox msg 0x%lx from PF",
		    cptvf->dev_name, (unsigned int long)mbx.msg);
	switch (mbx.msg) {
	case OTX_CPT_MSG_VF_UP:
		cptvf->pf_acked = true;
		break;
	case OTX_CPT_MSG_READY:
		{
			otx_cpt_chipid_vfid_t cid;

			cid.u64 = mbx.data;
			cptvf->pf_acked = true;
			cptvf->vfid = cid.s.vfid;
			CPT_LOG_DP_DEBUG("%s: Received VFID %d chip_id %d",
					 cptvf->dev_name,
					 cptvf->vfid, cid.s.chip_id);
		}
		break;
	case OTX_CPT_MSG_QBIND_GRP:
		cptvf->pf_acked = true;
		cptvf->vftype = mbx.data;
		CPT_LOG_DP_DEBUG("%s: VF %d group %d",
				 cptvf->dev_name, cptvf->vfid,
				 cptvf->vfgrp);
		break;
	case OTX_CPT_MSG_PF_TYPE:
		cptvf->pf_acked = true;
		if (mbx.data == OTX_CPT_PF_TYPE_AE)
			cptvf->vftype = OTX_CPT_VF_TYPE_AE;
		else if (mbx.data == OTX_CPT_PF_TYPE_SE)
			cptvf->vftype = OTX_CPT_VF_TYPE_SE;
		else
			cptvf->vftype = OTX_CPT_VF_TYPE_INVALID;
		break;
	case OTX_CPT_MBOX_MSG_TYPE_ACK:
		cptvf->pf_acked = true;
		break;
	case OTX_CPT_MBOX_MSG_TYPE_NACK:
		cptvf->pf_nacked = true;
		break;
	default:
		CPT_LOG_DP_DEBUG("%s: Invalid msg from PF, msg 0x%lx",
				 cptvf->dev_name, (unsigned int long)mbx.msg);
		break;
	}
}

/* Send a mailbox message to PF
 * @vf: vf from which this message to be sent
 * @mbx: Message to be sent
 */
static void
otx_cpt_send_msg_to_pf(struct cpt_vf *cptvf, struct cpt_mbox *mbx)
{
	/* Writing mbox(1) causes interrupt */
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VFX_PF_MBOXX(0, 0, 0), mbx->msg);
	CPT_WRITE_CSR(CPT_CSR_REG_BASE(cptvf),
		      CPTX_VFX_PF_MBOXX(0, 0, 1), mbx->data);
}

static int32_t
otx_cpt_send_msg_to_pf_timeout(struct cpt_vf *cptvf, struct cpt_mbox *mbx)
{
	int timeout = OTX_CPT_MBOX_MSG_TIMEOUT;
	int sleep_ms = 10;

	cptvf->pf_acked = false;
	cptvf->pf_nacked = false;

	otx_cpt_send_msg_to_pf(cptvf, mbx);

	/* Wait for previous message to be acked, timeout 2sec */
	while (!cptvf->pf_acked) {
		if (cptvf->pf_nacked)
			return -EINVAL;
		usleep(sleep_ms * 1000);
		otx_cpt_poll_misc(cptvf);
		if (cptvf->pf_acked)
			break;
		timeout -= sleep_ms;
		if (!timeout) {
			CPT_LOG_ERR("%s: PF didn't ack mbox msg %lx(vfid %u)",
				    cptvf->dev_name,
				    (unsigned int long)(mbx->msg & 0xFF),
				    cptvf->vfid);
			return -EBUSY;
		}
	}
	return 0;
}

int
otx_cpt_check_pf_ready(struct cpt_vf *cptvf)
{
	struct cpt_mbox mbx = {0, 0};

	mbx.msg = OTX_CPT_MSG_READY;
	if (otx_cpt_send_msg_to_pf_timeout(cptvf, &mbx)) {
		CPT_LOG_ERR("%s: PF didn't respond to READY msg",
			    cptvf->dev_name);
		return 1;
	}
	return 0;
}

int
otx_cpt_get_dev_type(struct cpt_vf *cptvf)
{
	struct cpt_mbox mbx = {0, 0};

	mbx.msg = OTX_CPT_MSG_PF_TYPE;
	if (otx_cpt_send_msg_to_pf_timeout(cptvf, &mbx)) {
		CPT_LOG_ERR("%s: PF didn't respond to query msg",
			    cptvf->dev_name);
		return 1;
	}
	return 0;
}

int
otx_cpt_send_vq_size_msg(struct cpt_vf *cptvf)
{
	struct cpt_mbox mbx = {0, 0};

	mbx.msg = OTX_CPT_MSG_QLEN;

	mbx.data = cptvf->qsize;
	if (otx_cpt_send_msg_to_pf_timeout(cptvf, &mbx)) {
		CPT_LOG_ERR("%s: PF didn't respond to vq_size msg",
			    cptvf->dev_name);
		return 1;
	}
	return 0;
}

int
otx_cpt_send_vf_grp_msg(struct cpt_vf *cptvf, uint32_t group)
{
	struct cpt_mbox mbx = {0, 0};

	mbx.msg = OTX_CPT_MSG_QBIND_GRP;

	/* Convey group of the VF */
	mbx.data = group;
	if (otx_cpt_send_msg_to_pf_timeout(cptvf, &mbx)) {
		CPT_LOG_ERR("%s: PF didn't respond to vf_type msg",
			    cptvf->dev_name);
		return 1;
	}
	return 0;
}

int
otx_cpt_send_vf_up(struct cpt_vf *cptvf)
{
	struct cpt_mbox mbx = {0, 0};

	mbx.msg = OTX_CPT_MSG_VF_UP;
	if (otx_cpt_send_msg_to_pf_timeout(cptvf, &mbx)) {
		CPT_LOG_ERR("%s: PF didn't respond to UP msg",
			    cptvf->dev_name);
		return 1;
	}
	return 0;
}

int
otx_cpt_send_vf_down(struct cpt_vf *cptvf)
{
	struct cpt_mbox mbx = {0, 0};

	mbx.msg = OTX_CPT_MSG_VF_DOWN;
	if (otx_cpt_send_msg_to_pf_timeout(cptvf, &mbx)) {
		CPT_LOG_ERR("%s: PF didn't respond to DOWN msg",
			    cptvf->dev_name);
		return 1;
	}
	return 0;
}
