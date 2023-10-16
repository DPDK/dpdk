/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
roc_nix_mac_rxtx_start_stop(struct roc_nix *roc_nix, bool start)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	if (roc_nix_is_vf_or_sdp(roc_nix)) {
		rc = NIX_ERR_OP_NOTSUP;
		goto exit;
	}

	if (start)
		mbox_alloc_msg_cgx_start_rxtx(mbox);
	else
		mbox_alloc_msg_cgx_stop_rxtx(mbox);

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_link_event_start_stop(struct roc_nix *roc_nix, bool start)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	if (roc_nix_is_vf_or_sdp(roc_nix)) {
		rc = NIX_ERR_OP_NOTSUP;
		goto exit;
	}

	if (start)
		mbox_alloc_msg_cgx_start_linkevents(mbox);
	else
		mbox_alloc_msg_cgx_stop_linkevents(mbox);

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_loopback_enable(struct roc_nix *roc_nix, bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	if (enable && roc_nix_is_vf_or_sdp(roc_nix)) {
		rc = NIX_ERR_OP_NOTSUP;
		goto exit;
	}

	if (enable)
		mbox_alloc_msg_cgx_intlbk_enable(mbox);
	else
		mbox_alloc_msg_cgx_intlbk_disable(mbox);

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_addr_set(struct roc_nix *roc_nix, const uint8_t addr[])
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct cgx_mac_addr_set_or_get *req, *rsp;
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc = -ENOSPC;

	if (roc_nix_is_vf_or_sdp(roc_nix)) {
		rc = NIX_ERR_OP_NOTSUP;
		goto exit;
	}

	if (dev_active_vfs(&nix->dev)) {
		rc = NIX_ERR_OP_NOTSUP;
		goto exit;
	}

	req = mbox_alloc_msg_cgx_mac_addr_set(mbox);
	if (req == NULL)
		goto exit;

	req->index = nix->dmac_flt_idx;
	mbox_memcpy(req->mac_addr, addr, PLT_ETHER_ADDR_LEN);

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;
	nix->dmac_flt_idx = rsp->index;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_max_entries_get(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct cgx_max_dmac_entries_get_rsp *rsp;
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	if (roc_nix_is_vf_or_sdp(roc_nix)) {
		rc = NIX_ERR_OP_NOTSUP;
		goto exit;
	}

	mbox_alloc_msg_cgx_mac_max_entries_get(mbox);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	rc = rsp->max_dmac_filters ? rsp->max_dmac_filters : 1;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_addr_add(struct roc_nix *roc_nix, uint8_t addr[])
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct cgx_mac_addr_add_req *req;
	struct cgx_mac_addr_add_rsp *rsp;
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	if (roc_nix_is_vf_or_sdp(roc_nix)) {
		rc = NIX_ERR_OP_NOTSUP;
		goto exit;
	}

	if (dev_active_vfs(&nix->dev)) {
		rc = NIX_ERR_OP_NOTSUP;
		goto exit;
	}

	req = mbox_alloc_msg_cgx_mac_addr_add(mbox);
	mbox_memcpy(req->mac_addr, addr, PLT_ETHER_ADDR_LEN);

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc < 0)
		goto exit;

	rc = rsp->index;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_addr_del(struct roc_nix *roc_nix, uint32_t index)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct cgx_mac_addr_del_req *req;
	int rc = -ENOSPC;

	if (roc_nix_is_vf_or_sdp(roc_nix)) {
		rc = NIX_ERR_OP_NOTSUP;
		goto exit;
	}

	req = mbox_alloc_msg_cgx_mac_addr_del(mbox);
	if (req == NULL)
		goto exit;
	req->index = index;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_promisc_mode_enable(struct roc_nix *roc_nix, int enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	if (roc_nix_is_vf_or_sdp(roc_nix)) {
		rc = NIX_ERR_OP_NOTSUP;
		goto exit;
	}

	if (enable)
		mbox_alloc_msg_cgx_promisc_enable(mbox);
	else
		mbox_alloc_msg_cgx_promisc_disable(mbox);

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_link_info_get(struct roc_nix *roc_nix,
			  struct roc_nix_link_info *link_info)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct cgx_link_info_msg *rsp;
	int rc;

	mbox_alloc_msg_cgx_get_linkinfo(mbox);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	link_info->status = rsp->link_info.link_up;
	link_info->full_duplex = rsp->link_info.full_duplex;
	link_info->lmac_type_id = rsp->link_info.lmac_type_id;
	link_info->speed = rsp->link_info.speed;
	link_info->autoneg = rsp->link_info.an;
	link_info->fec = rsp->link_info.fec;
	link_info->port = rsp->link_info.port;

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_link_state_set(struct roc_nix *roc_nix, uint8_t up)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct cgx_set_link_state_msg *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_cgx_set_link_state(mbox);
	if (req == NULL)
		goto exit;
	req->enable = up;
	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_link_info_set(struct roc_nix *roc_nix,
			  struct roc_nix_link_info *link_info)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct cgx_set_link_mode_req *req;
	int rc;

	rc = roc_nix_mac_link_state_set(roc_nix, link_info->status);
	if (rc)
		goto exit;

	req = mbox_alloc_msg_cgx_set_link_mode(mbox);
	if (req == NULL) {
		rc =  -ENOSPC;
		goto exit;
	}
	req->args.speed = link_info->speed;
	req->args.duplex = link_info->full_duplex;
	req->args.an = link_info->autoneg;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;

}

int
roc_nix_mac_mtu_set(struct roc_nix *roc_nix, uint16_t mtu)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct nix_frs_cfg *req;
	bool sdp_link = false;
	int rc = -ENOSPC;

	if (roc_nix_is_sdp(roc_nix))
		sdp_link = true;

	req = mbox_alloc_msg_nix_set_hw_frs(mbox);
	if (req == NULL)
		goto exit;
	req->maxlen = mtu;
	req->update_smq = true;
	req->sdp_link = sdp_link;

	rc = mbox_process(mbox);
	if (rc)
		goto exit;

	/* Save MTU for later use */
	nix->mtu = mtu;
	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_max_rx_len_set(struct roc_nix *roc_nix, uint16_t maxlen)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct nix_frs_cfg *req;
	bool sdp_link = false;
	int rc = -ENOSPC;

	if (roc_nix_is_sdp(roc_nix))
		sdp_link = true;

	req = mbox_alloc_msg_nix_set_hw_frs(mbox);
	if (req == NULL)
		goto exit;
	req->sdp_link = sdp_link;
	req->maxlen = maxlen;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mac_link_cb_register(struct roc_nix *roc_nix, link_status_t link_update)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	if (link_update == NULL)
		return NIX_ERR_PARAM;

	dev->ops->link_status_update = (link_info_t)link_update;
	return 0;
}

void
roc_nix_mac_link_cb_unregister(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	dev->ops->link_status_update = NULL;
}

int
roc_nix_mac_link_info_get_cb_register(struct roc_nix *roc_nix,
				      link_info_get_t link_info_get)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	if (link_info_get == NULL)
		return NIX_ERR_PARAM;

	dev->ops->link_status_get = (link_info_t)link_info_get;
	return 0;
}

void
roc_nix_mac_link_info_get_cb_unregister(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	dev->ops->link_status_get = NULL;
}
