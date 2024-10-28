/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
roc_rvu_lf_dev_init(struct roc_rvu_lf *roc_rvu_lf)
{
	struct plt_pci_device *pci_dev;
	struct dev *dev;
	struct rvu_lf *rvu;
	int rc;

	if (roc_rvu_lf == NULL || roc_rvu_lf->pci_dev == NULL)
		return RVU_ERR_PARAM;

	rvu = roc_rvu_lf_to_rvu_priv(roc_rvu_lf);
	pci_dev = roc_rvu_lf->pci_dev;
	dev = &rvu->dev;

	if (rvu->dev.drv_inited)
		return 0;

	if (dev->mbox_active)
		goto skip_dev_init;

	memset(rvu, 0, sizeof(*rvu));

	/* Initialize device  */
	rc = dev_init(dev, pci_dev);
	if (rc) {
		plt_err("Failed to init roc device");
		goto fail;
	}

skip_dev_init:
	dev->roc_rvu_lf = roc_rvu_lf;
	rvu->pci_dev = pci_dev;

	roc_idev_rvu_lf_set(roc_rvu_lf);
	rvu->dev.drv_inited = true;

	return 0;
fail:
	return rc;
}

int
roc_rvu_lf_dev_fini(struct roc_rvu_lf *roc_rvu_lf)
{
	struct rvu_lf *rvu = roc_rvu_lf_to_rvu_priv(roc_rvu_lf);

	if (rvu == NULL)
		return NIX_ERR_PARAM;

	rvu->dev.drv_inited = false;

	roc_idev_rvu_lf_free(roc_rvu_lf);

	return dev_fini(&rvu->dev, rvu->pci_dev);
}

uint16_t
roc_rvu_lf_pf_func_get(struct roc_rvu_lf *roc_rvu_lf)
{
	struct rvu_lf *rvu = roc_rvu_lf_to_rvu_priv(roc_rvu_lf);
	struct dev *dev = &rvu->dev;

	return dev->pf_func;
}

int
roc_rvu_lf_msg_id_range_set(struct roc_rvu_lf *roc_rvu_lf, uint16_t from, uint16_t to)
{
	struct rvu_lf *rvu = roc_rvu_lf_to_rvu_priv(roc_rvu_lf);

	if (from <= MBOX_MSG_GENERIC_MAX_ID || from > to)
		return -EINVAL;

	rvu->msg_id_from = from;
	rvu->msg_id_to = to;

	return 0;
}

bool
roc_rvu_lf_msg_id_range_check(struct roc_rvu_lf *roc_rvu_lf, uint16_t msg_id)
{
	struct rvu_lf *rvu;

	if (roc_rvu_lf == NULL)
		return 0;

	rvu = roc_rvu_lf_to_rvu_priv(roc_rvu_lf);

	if (msg_id > rvu->msg_id_from && msg_id < rvu->msg_id_to)
		return 1;

	return 0;
}

int
roc_rvu_lf_msg_process(struct roc_rvu_lf *roc_rvu_lf, uint16_t vf, uint16_t msg_id,
		       void *req_data, uint16_t req_len, void *rsp_data, uint16_t rsp_len)
{
	struct rvu_lf *rvu = roc_rvu_lf_to_rvu_priv(roc_rvu_lf);
	struct mbox *mbox;
	struct rvu_lf_msg *req;
	struct rvu_lf_msg *rsp;
	int rc = -ENOSPC;
	int devid = 0;

	if (rvu->dev.vf == -1 && roc_rvu_lf_msg_id_range_check(roc_rvu_lf, msg_id)) {
		/* This is PF context */
		if (vf >= rvu->dev.maxvf)
			return -EINVAL;
		devid = vf;
		mbox = mbox_get(&rvu->dev.mbox_vfpf_up);
	} else {
		/* This is VF context */
		devid = 0; /* VF send all message to PF */
		mbox = mbox_get(rvu->dev.mbox);
	}
	req = (struct rvu_lf_msg *)mbox_alloc_msg_rsp(mbox, devid,
						      req_len + sizeof(struct rvu_lf_msg),
						      rsp_len + sizeof(struct rvu_lf_msg));
	if (!req)
		goto fail;
	mbox_memcpy(req->data, req_data, req_len);
	req->hdr.sig = MBOX_REQ_SIG;
	req->hdr.id = msg_id;
	req->hdr.pcifunc = roc_rvu_lf_pf_func_get(roc_rvu_lf);

	if (rvu->dev.vf == -1) {
		mbox_msg_send_up(mbox, devid);
		rc = mbox_get_rsp(mbox, devid, (void *)&rsp);
		if (rc)
			goto fail;
	} else {
		rc = mbox_process_msg(mbox, (void *)&rsp);
		if (rc)
			goto fail;
	}
	if (rsp_len && rsp_data != NULL)
		mbox_memcpy(rsp_data, rsp->data, rsp_len);
fail:
	mbox_put(mbox);
	return rc;
}

int
roc_rvu_lf_irq_register(struct roc_rvu_lf *roc_rvu_lf, unsigned int irq,
			roc_rvu_lf_intr_cb_fn cb, void *data)
{
	struct rvu_lf *rvu = roc_rvu_lf_to_rvu_priv(roc_rvu_lf);
	struct plt_intr_handle *handle;

	handle = rvu->pci_dev->intr_handle;

	return dev_irq_register(handle, (plt_intr_callback_fn)cb, data, irq);
}

int
roc_rvu_lf_irq_unregister(struct roc_rvu_lf *roc_rvu_lf, unsigned int irq,
			  roc_rvu_lf_intr_cb_fn cb, void *data)
{
	struct rvu_lf *rvu = roc_rvu_lf_to_rvu_priv(roc_rvu_lf);
	struct plt_intr_handle *handle;

	handle = rvu->pci_dev->intr_handle;

	dev_irq_unregister(handle, (plt_intr_callback_fn)cb, data, irq);

	return 0;
}

int
roc_rvu_lf_msg_handler_register(struct roc_rvu_lf *roc_rvu_lf, roc_rvu_lf_msg_handler_cb_fn cb)
{
	struct rvu_lf *rvu = roc_rvu_lf_to_rvu_priv(roc_rvu_lf);
	struct dev *dev = &rvu->dev;

	if (cb == NULL)
		return -EINVAL;

	dev->ops->msg_process_cb = (msg_process_cb_t)cb;

	return 0;
}

int
roc_rvu_lf_msg_handler_unregister(struct roc_rvu_lf *roc_rvu_lf)
{
	struct rvu_lf *rvu = roc_rvu_lf_to_rvu_priv(roc_rvu_lf);
	struct dev *dev = &rvu->dev;

	dev->ops->msg_process_cb = NULL;

	return 0;
}
