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
