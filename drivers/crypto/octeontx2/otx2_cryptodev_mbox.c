/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */
#include <rte_cryptodev.h>

#include "otx2_cryptodev.h"
#include "otx2_cryptodev_mbox.h"
#include "otx2_dev.h"
#include "otx2_mbox.h"

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
