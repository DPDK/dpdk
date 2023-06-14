/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
roc_mcs_hw_info_get(struct roc_mcs_hw_info *hw_info)
{
	struct mcs_hw_info *hw;
	struct npa_lf *npa;
	int rc;

	MCS_SUPPORT_CHECK;

	if (hw_info == NULL)
		return -EINVAL;

	/* Use mbox handler of first probed pci_func for
	 * initial mcs mbox communication.
	 */
	npa = idev_npa_obj_get();
	if (!npa)
		return MCS_ERR_DEVICE_NOT_FOUND;

	mbox_alloc_msg_mcs_get_hw_info(npa->mbox);
	rc = mbox_process_msg(npa->mbox, (void *)&hw);
	if (rc)
		return rc;

	hw_info->num_mcs_blks = hw->num_mcs_blks;
	hw_info->tcam_entries = hw->tcam_entries;
	hw_info->secy_entries = hw->secy_entries;
	hw_info->sc_entries = hw->sc_entries;
	hw_info->sa_entries = hw->sa_entries;

	return rc;
}

static int
mcs_alloc_bmap(uint16_t entries, void **mem, struct plt_bitmap **bmap)
{
	size_t bmap_sz;
	int rc = 0;

	bmap_sz = plt_bitmap_get_memory_footprint(entries);
	*mem = plt_zmalloc(bmap_sz, PLT_CACHE_LINE_SIZE);
	if (*mem == NULL)
		rc = -ENOMEM;

	*bmap = plt_bitmap_init(entries, *mem, bmap_sz);
	if (!*bmap) {
		plt_free(*mem);
		*mem = NULL;
		rc = -ENOMEM;
	}

	return rc;
}

static void
rsrc_bmap_free(struct mcs_rsrc *rsrc)
{
	plt_bitmap_free(rsrc->tcam_bmap);
	plt_free(rsrc->tcam_bmap_mem);
	plt_bitmap_free(rsrc->secy_bmap);
	plt_free(rsrc->secy_bmap_mem);
	plt_bitmap_free(rsrc->sc_bmap);
	plt_free(rsrc->sc_bmap_mem);
	plt_bitmap_free(rsrc->sa_bmap);
	plt_free(rsrc->sa_bmap_mem);
}

static int
rsrc_bmap_alloc(struct mcs_priv *priv, struct mcs_rsrc *rsrc)
{
	int rc;

	rc = mcs_alloc_bmap(priv->tcam_entries << 1, &rsrc->tcam_bmap_mem, &rsrc->tcam_bmap);
	if (rc)
		goto exit;

	rc = mcs_alloc_bmap(priv->secy_entries << 1, &rsrc->secy_bmap_mem, &rsrc->secy_bmap);
	if (rc)
		goto exit;

	rc = mcs_alloc_bmap(priv->sc_entries << 1, &rsrc->sc_bmap_mem, &rsrc->sc_bmap);
	if (rc)
		goto exit;

	rc = mcs_alloc_bmap(priv->sa_entries << 1, &rsrc->sa_bmap_mem, &rsrc->sa_bmap);
	if (rc)
		goto exit;

	return rc;
exit:
	rsrc_bmap_free(rsrc);

	return rc;
}

static int
mcs_alloc_rsrc_bmap(struct roc_mcs *mcs)
{
	struct mcs_priv *priv = roc_mcs_to_mcs_priv(mcs);
	struct mcs_hw_info *hw;
	int i, rc;

	mbox_alloc_msg_mcs_get_hw_info(mcs->mbox);
	rc = mbox_process_msg(mcs->mbox, (void *)&hw);
	if (rc)
		return rc;

	priv->num_mcs_blks = hw->num_mcs_blks;
	priv->tcam_entries = hw->tcam_entries;
	priv->secy_entries = hw->secy_entries;
	priv->sc_entries = hw->sc_entries;
	priv->sa_entries = hw->sa_entries;

	rc = rsrc_bmap_alloc(priv, &priv->dev_rsrc);
	if (rc)
		return rc;

	priv->port_rsrc = plt_zmalloc(sizeof(struct mcs_rsrc) * 4, 0);
	if (priv->port_rsrc == NULL) {
		rsrc_bmap_free(&priv->dev_rsrc);
		return -ENOMEM;
	}

	for (i = 0; i < MAX_PORTS_PER_MCS; i++) {
		rc = rsrc_bmap_alloc(priv, &priv->port_rsrc[i]);
		if (rc)
			goto exit;

		priv->port_rsrc[i].sc_conf =
			plt_zmalloc(priv->sc_entries * sizeof(struct mcs_sc_conf), 0);
		if (priv->port_rsrc[i].sc_conf == NULL) {
			rsrc_bmap_free(&priv->port_rsrc[i]);
			goto exit;
		}
	}

	return rc;

exit:
	while (i--) {
		rsrc_bmap_free(&priv->port_rsrc[i]);
		plt_free(priv->port_rsrc[i].sc_conf);
	}
	plt_free(priv->port_rsrc);

	return -ENOMEM;
}

struct roc_mcs *
roc_mcs_dev_init(uint8_t mcs_idx)
{
	struct roc_mcs *mcs;
	struct npa_lf *npa;

	if (!(roc_feature_bphy_has_macsec() || roc_feature_nix_has_macsec()))
		return NULL;

	mcs = roc_idev_mcs_get(mcs_idx);
	if (mcs) {
		plt_info("Skipping device, mcs device already probed");
		mcs->refcount++;
		return mcs;
	}

	mcs = plt_zmalloc(sizeof(struct roc_mcs), PLT_CACHE_LINE_SIZE);
	if (!mcs)
		return NULL;

	npa = idev_npa_obj_get();
	if (!npa)
		goto exit;

	mcs->mbox = npa->mbox;
	mcs->idx = mcs_idx;

	/* Add any per mcsv initialization */
	if (mcs_alloc_rsrc_bmap(mcs))
		goto exit;

	roc_idev_mcs_set(mcs);
	mcs->refcount++;

	return mcs;
exit:
	plt_free(mcs);
	return NULL;
}

void
roc_mcs_dev_fini(struct roc_mcs *mcs)
{
	struct mcs_priv *priv;
	int i;

	mcs->refcount--;
	if (mcs->refcount > 0)
		return;

	priv = roc_mcs_to_mcs_priv(mcs);

	rsrc_bmap_free(&priv->dev_rsrc);

	for (i = 0; i < MAX_PORTS_PER_MCS; i++) {
		rsrc_bmap_free(&priv->port_rsrc[i]);
		plt_free(priv->port_rsrc[i].sc_conf);
	}

	plt_free(priv->port_rsrc);

	roc_idev_mcs_free(mcs);

	plt_free(mcs);
}
