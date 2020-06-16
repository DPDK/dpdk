/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */
#include <rte_cryptodev.h>
#include <rte_ethdev.h>

#include "otx2_cryptodev.h"
#include "otx2_cryptodev_hw_access.h"
#include "otx2_cryptodev_mbox.h"
#include "otx2_dev.h"
#include "otx2_ethdev.h"
#include "otx2_sec_idev.h"
#include "otx2_mbox.h"

#include "cpt_pmd_logs.h"

int
otx2_cpt_hardware_caps_get(const struct rte_cryptodev *dev,
			      union cpt_eng_caps *hw_caps)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	struct otx2_dev *otx2_dev = &vf->otx2_dev;
	struct cpt_caps_rsp_msg *rsp;
	int ret;

	otx2_mbox_alloc_msg_cpt_caps_get(otx2_dev->mbox);

	ret = otx2_mbox_process_msg(otx2_dev->mbox, (void *)&rsp);
	if (ret)
		return -EIO;

	memcpy(hw_caps, rsp->eng_caps,
		sizeof(union cpt_eng_caps) * CPT_MAX_ENG_TYPES);

	return 0;
}

int
otx2_cpt_available_queues_get(const struct rte_cryptodev *dev,
			      uint16_t *nb_queues)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	struct otx2_dev *otx2_dev = &vf->otx2_dev;
	struct free_rsrcs_rsp *rsp;
	int ret;

	otx2_mbox_alloc_msg_free_rsrc_cnt(otx2_dev->mbox);

	ret = otx2_mbox_process_msg(otx2_dev->mbox, (void *)&rsp);
	if (ret)
		return -EIO;

	*nb_queues = rsp->cpt;
	return 0;
}

int
otx2_cpt_queues_attach(const struct rte_cryptodev *dev, uint8_t nb_queues)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	struct otx2_mbox *mbox = vf->otx2_dev.mbox;
	struct rsrc_attach_req *req;

	/* Ask AF to attach required LFs */

	req = otx2_mbox_alloc_msg_attach_resources(mbox);

	/* 1 LF = 1 queue */
	req->cptlfs = nb_queues;

	if (otx2_mbox_process(mbox) < 0)
		return -EIO;

	/* Update number of attached queues */
	vf->nb_queues = nb_queues;

	return 0;
}

int
otx2_cpt_queues_detach(const struct rte_cryptodev *dev)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	struct otx2_mbox *mbox = vf->otx2_dev.mbox;
	struct rsrc_detach_req *req;

	req = otx2_mbox_alloc_msg_detach_resources(mbox);
	req->cptlfs = true;
	req->partial = true;
	if (otx2_mbox_process(mbox) < 0)
		return -EIO;

	/* Queues have been detached */
	vf->nb_queues = 0;

	return 0;
}

int
otx2_cpt_msix_offsets_get(const struct rte_cryptodev *dev)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	struct otx2_mbox *mbox = vf->otx2_dev.mbox;
	struct msix_offset_rsp *rsp;
	uint32_t i, ret;

	/* Get CPT MSI-X vector offsets */

	otx2_mbox_alloc_msg_msix_offset(mbox);

	ret = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (ret)
		return ret;

	for (i = 0; i < vf->nb_queues; i++)
		vf->lf_msixoff[i] = rsp->cptlf_msixoff[i];

	return 0;
}

static int
otx2_cpt_send_mbox_msg(struct otx2_cpt_vf *vf)
{
	struct otx2_mbox *mbox = vf->otx2_dev.mbox;
	int ret;

	otx2_mbox_msg_send(mbox, 0);

	ret = otx2_mbox_wait_for_rsp(mbox, 0);
	if (ret < 0) {
		CPT_LOG_ERR("Could not get mailbox response");
		return ret;
	}

	return 0;
}

int
otx2_cpt_af_reg_read(const struct rte_cryptodev *dev, uint64_t reg,
		     uint64_t *val)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	struct otx2_mbox *mbox = vf->otx2_dev.mbox;
	struct otx2_mbox_dev *mdev = &mbox->dev[0];
	struct cpt_rd_wr_reg_msg *msg;
	int ret, off;

	msg = (struct cpt_rd_wr_reg_msg *)
			otx2_mbox_alloc_msg_rsp(mbox, 0, sizeof(*msg),
						sizeof(*msg));
	if (msg == NULL) {
		CPT_LOG_ERR("Could not allocate mailbox message");
		return -EFAULT;
	}

	msg->hdr.id = MBOX_MSG_CPT_RD_WR_REGISTER;
	msg->hdr.sig = OTX2_MBOX_REQ_SIG;
	msg->hdr.pcifunc = vf->otx2_dev.pf_func;
	msg->is_write = 0;
	msg->reg_offset = reg;
	msg->ret_val = val;

	ret = otx2_cpt_send_mbox_msg(vf);
	if (ret < 0)
		return ret;

	off = mbox->rx_start +
			RTE_ALIGN(sizeof(struct mbox_hdr), MBOX_MSG_ALIGN);
	msg = (struct cpt_rd_wr_reg_msg *) ((uintptr_t)mdev->mbase + off);

	*val = msg->val;

	return 0;
}

int
otx2_cpt_af_reg_write(const struct rte_cryptodev *dev, uint64_t reg,
		      uint64_t val)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	struct otx2_mbox *mbox = vf->otx2_dev.mbox;
	struct cpt_rd_wr_reg_msg *msg;

	msg = (struct cpt_rd_wr_reg_msg *)
			otx2_mbox_alloc_msg_rsp(mbox, 0, sizeof(*msg),
						sizeof(*msg));
	if (msg == NULL) {
		CPT_LOG_ERR("Could not allocate mailbox message");
		return -EFAULT;
	}

	msg->hdr.id = MBOX_MSG_CPT_RD_WR_REGISTER;
	msg->hdr.sig = OTX2_MBOX_REQ_SIG;
	msg->hdr.pcifunc = vf->otx2_dev.pf_func;
	msg->is_write = 1;
	msg->reg_offset = reg;
	msg->val = val;

	return otx2_cpt_send_mbox_msg(vf);
}

int
otx2_cpt_inline_init(const struct rte_cryptodev *dev)
{
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	struct otx2_mbox *mbox = vf->otx2_dev.mbox;
	struct cpt_rx_inline_lf_cfg_msg *msg;
	int ret;

	msg = otx2_mbox_alloc_msg_cpt_rx_inline_lf_cfg(mbox);
	msg->sso_pf_func = otx2_sso_pf_func_get();

	otx2_mbox_msg_send(mbox, 0);
	ret = otx2_mbox_process(mbox);
	if (ret < 0)
		return -EIO;

	return 0;
}

int
otx2_cpt_qp_ethdev_bind(const struct rte_cryptodev *dev, struct otx2_cpt_qp *qp,
			uint16_t port_id)
{
	struct rte_eth_dev *eth_dev = &rte_eth_devices[port_id];
	struct otx2_cpt_vf *vf = dev->data->dev_private;
	struct otx2_mbox *mbox = vf->otx2_dev.mbox;
	struct cpt_inline_ipsec_cfg_msg *msg;
	struct otx2_eth_dev *otx2_eth_dev;
	int ret;

	if (!otx2_eth_dev_is_sec_capable(&rte_eth_devices[port_id]))
		return -EINVAL;

	otx2_eth_dev = otx2_eth_pmd_priv(eth_dev);

	msg = otx2_mbox_alloc_msg_cpt_inline_ipsec_cfg(mbox);
	msg->dir = CPT_INLINE_OUTBOUND;
	msg->enable = 1;
	msg->slot = qp->id;

	msg->nix_pf_func = otx2_eth_dev->pf_func;

	otx2_mbox_msg_send(mbox, 0);
	ret = otx2_mbox_process(mbox);
	if (ret < 0)
		return -EIO;

	return 0;
}
