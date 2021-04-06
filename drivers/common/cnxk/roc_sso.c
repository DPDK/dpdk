/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

/* Private functions. */
static int
sso_lf_alloc(struct roc_sso *roc_sso, enum sso_lf_type lf_type, uint16_t nb_lf,
	     void **rsp)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	int rc = -ENOSPC;

	switch (lf_type) {
	case SSO_LF_TYPE_HWS: {
		struct ssow_lf_alloc_req *req;

		req = mbox_alloc_msg_ssow_lf_alloc(dev->mbox);
		if (req == NULL)
			return rc;
		req->hws = nb_lf;
	} break;
	case SSO_LF_TYPE_HWGRP: {
		struct sso_lf_alloc_req *req;

		req = mbox_alloc_msg_sso_lf_alloc(dev->mbox);
		if (req == NULL)
			return rc;
		req->hwgrps = nb_lf;
	} break;
	default:
		break;
	}

	rc = mbox_process_msg(dev->mbox, rsp);
	if (rc < 0)
		return rc;

	return 0;
}

static int
sso_lf_free(struct roc_sso *roc_sso, enum sso_lf_type lf_type, uint16_t nb_lf)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	int rc = -ENOSPC;

	switch (lf_type) {
	case SSO_LF_TYPE_HWS: {
		struct ssow_lf_free_req *req;

		req = mbox_alloc_msg_ssow_lf_free(dev->mbox);
		if (req == NULL)
			return rc;
		req->hws = nb_lf;
	} break;
	case SSO_LF_TYPE_HWGRP: {
		struct sso_lf_free_req *req;

		req = mbox_alloc_msg_sso_lf_free(dev->mbox);
		if (req == NULL)
			return rc;
		req->hwgrps = nb_lf;
	} break;
	default:
		break;
	}

	rc = mbox_process(dev->mbox);
	if (rc < 0)
		return rc;

	return 0;
}

static int
sso_rsrc_attach(struct roc_sso *roc_sso, enum sso_lf_type lf_type,
		uint16_t nb_lf)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	struct rsrc_attach_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_attach_resources(dev->mbox);
	if (req == NULL)
		return rc;
	switch (lf_type) {
	case SSO_LF_TYPE_HWS:
		req->ssow = nb_lf;
		break;
	case SSO_LF_TYPE_HWGRP:
		req->sso = nb_lf;
		break;
	default:
		return SSO_ERR_PARAM;
	}

	req->modify = true;
	if (mbox_process(dev->mbox) < 0)
		return -EIO;

	return 0;
}

static int
sso_rsrc_detach(struct roc_sso *roc_sso, enum sso_lf_type lf_type)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	struct rsrc_detach_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_detach_resources(dev->mbox);
	if (req == NULL)
		return rc;
	switch (lf_type) {
	case SSO_LF_TYPE_HWS:
		req->ssow = true;
		break;
	case SSO_LF_TYPE_HWGRP:
		req->sso = true;
		break;
	default:
		return SSO_ERR_PARAM;
	}

	req->partial = true;
	if (mbox_process(dev->mbox) < 0)
		return -EIO;

	return 0;
}

static int
sso_rsrc_get(struct roc_sso *roc_sso)
{
	struct dev *dev = &roc_sso_to_sso_priv(roc_sso)->dev;
	struct free_rsrcs_rsp *rsrc_cnt;
	int rc;

	mbox_alloc_msg_free_rsrc_cnt(dev->mbox);
	rc = mbox_process_msg(dev->mbox, (void **)&rsrc_cnt);
	if (rc < 0) {
		plt_err("Failed to get free resource count\n");
		return rc;
	}

	roc_sso->max_hwgrp = rsrc_cnt->sso;
	roc_sso->max_hws = rsrc_cnt->ssow;

	return 0;
}

int
roc_sso_rsrc_init(struct roc_sso *roc_sso, uint8_t nb_hws, uint16_t nb_hwgrp)
{
	struct sso_lf_alloc_rsp *rsp_hwgrp;
	int rc;

	if (roc_sso->max_hwgrp < nb_hwgrp)
		return -ENOENT;
	if (roc_sso->max_hws < nb_hws)
		return -ENOENT;

	rc = sso_rsrc_attach(roc_sso, SSO_LF_TYPE_HWS, nb_hws);
	if (rc < 0) {
		plt_err("Unable to attach SSO HWS LFs");
		return rc;
	}

	rc = sso_rsrc_attach(roc_sso, SSO_LF_TYPE_HWGRP, nb_hwgrp);
	if (rc < 0) {
		plt_err("Unable to attach SSO HWGRP LFs");
		goto hwgrp_atch_fail;
	}

	rc = sso_lf_alloc(roc_sso, SSO_LF_TYPE_HWS, nb_hws, NULL);
	if (rc < 0) {
		plt_err("Unable to alloc SSO HWS LFs");
		goto hws_alloc_fail;
	}

	rc = sso_lf_alloc(roc_sso, SSO_LF_TYPE_HWGRP, nb_hwgrp,
			  (void **)&rsp_hwgrp);
	if (rc < 0) {
		plt_err("Unable to alloc SSO HWGRP Lfs");
		goto hwgrp_alloc_fail;
	}

	roc_sso->xaq_buf_size = rsp_hwgrp->xaq_buf_size;
	roc_sso->xae_waes = rsp_hwgrp->xaq_wq_entries;
	roc_sso->iue = rsp_hwgrp->in_unit_entries;

	roc_sso->nb_hwgrp = nb_hwgrp;
	roc_sso->nb_hws = nb_hws;

	return 0;
hwgrp_alloc_fail:
	sso_lf_free(roc_sso, SSO_LF_TYPE_HWS, nb_hws);
hws_alloc_fail:
	sso_rsrc_detach(roc_sso, SSO_LF_TYPE_HWGRP);
hwgrp_atch_fail:
	sso_rsrc_detach(roc_sso, SSO_LF_TYPE_HWS);
	return rc;
}

void
roc_sso_rsrc_fini(struct roc_sso *roc_sso)
{
	if (!roc_sso->nb_hws && !roc_sso->nb_hwgrp)
		return;

	sso_lf_free(roc_sso, SSO_LF_TYPE_HWS, roc_sso->nb_hws);
	sso_lf_free(roc_sso, SSO_LF_TYPE_HWGRP, roc_sso->nb_hwgrp);

	sso_rsrc_detach(roc_sso, SSO_LF_TYPE_HWS);
	sso_rsrc_detach(roc_sso, SSO_LF_TYPE_HWGRP);

	roc_sso->nb_hwgrp = 0;
	roc_sso->nb_hws = 0;
}

int
roc_sso_dev_init(struct roc_sso *roc_sso)
{
	struct plt_pci_device *pci_dev;
	struct sso *sso;
	int rc;

	if (roc_sso == NULL || roc_sso->pci_dev == NULL)
		return SSO_ERR_PARAM;

	PLT_STATIC_ASSERT(sizeof(struct sso) <= ROC_SSO_MEM_SZ);
	sso = roc_sso_to_sso_priv(roc_sso);
	memset(sso, 0, sizeof(*sso));
	pci_dev = roc_sso->pci_dev;

	rc = dev_init(&sso->dev, pci_dev);
	if (rc < 0) {
		plt_err("Failed to init roc device");
		goto fail;
	}

	rc = sso_rsrc_get(roc_sso);
	if (rc < 0) {
		plt_err("Failed to get SSO resources");
		goto rsrc_fail;
	}
	rc = -ENOMEM;

	idev_sso_pffunc_set(sso->dev.pf_func);
	sso->pci_dev = pci_dev;
	sso->dev.drv_inited = true;
	roc_sso->lmt_base = sso->dev.lmt_base;

	return 0;
rsrc_fail:
	rc |= dev_fini(&sso->dev, pci_dev);
fail:
	return rc;
}

int
roc_sso_dev_fini(struct roc_sso *roc_sso)
{
	struct sso *sso;

	sso = roc_sso_to_sso_priv(roc_sso);
	sso->dev.drv_inited = false;

	return dev_fini(&sso->dev, sso->pci_dev);
}
