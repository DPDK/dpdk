/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
roc_nix_npc_promisc_ena_dis(struct roc_nix *roc_nix, int enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct nix_rx_mode *req;
	int rc = -ENOSPC;

	if (roc_nix_is_vf_or_sdp(roc_nix)) {
		rc = NIX_ERR_PARAM;
		goto exit;
	}

	req = mbox_alloc_msg_nix_set_rx_mode(mbox);
	if (req == NULL)
		goto exit;

	if (enable)
		req->mode = NIX_RX_MODE_UCAST | NIX_RX_MODE_PROMISC;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_npc_mac_addr_set(struct roc_nix *roc_nix, uint8_t addr[])
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct nix_set_mac_addr *req;
	int rc;

	req = mbox_alloc_msg_nix_set_mac_addr(mbox);
	mbox_memcpy(req->mac_addr, addr, PLT_ETHER_ADDR_LEN);
	rc = mbox_process(mbox);
	mbox_put(mbox);
	return rc;
}

int
roc_nix_npc_mac_addr_get(struct roc_nix *roc_nix, uint8_t *addr)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct nix_get_mac_addr_rsp *rsp;
	int rc;

	mbox_alloc_msg_nix_get_mac_addr(mbox);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	mbox_memcpy(addr, rsp->mac_addr, PLT_ETHER_ADDR_LEN);
	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_npc_rx_ena_dis(struct roc_nix *roc_nix, bool enable)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc;

	if (enable)
		mbox_alloc_msg_nix_lf_start_rx(mbox);
	else
		mbox_alloc_msg_nix_lf_stop_rx(mbox);

	rc = mbox_process(mbox);
	if (!rc)
		roc_nix->io_enabled = enable;

	mbox_put(mbox);
	return rc;
}

int
roc_nix_npc_mcast_config(struct roc_nix *roc_nix, bool mcast_enable,
			 bool prom_enable)

{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct nix_rx_mode *req;
	int rc = -ENOSPC;

	if (roc_nix_is_vf_or_sdp(roc_nix)) {
		rc = 0;
		goto exit;
	}

	req = mbox_alloc_msg_nix_set_rx_mode(mbox);
	if (req == NULL)
		goto exit;

	if (mcast_enable)
		req->mode = NIX_RX_MODE_ALLMULTI;
	if (prom_enable)
		req->mode = NIX_RX_MODE_PROMISC;

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}
