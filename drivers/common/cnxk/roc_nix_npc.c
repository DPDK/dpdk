/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static inline struct mbox *
get_mbox(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	return dev->mbox;
}

int
roc_nix_npc_promisc_ena_dis(struct roc_nix *roc_nix, int enable)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_rx_mode *req;
	int rc = -ENOSPC;

	if (roc_nix_is_vf_or_sdp(roc_nix))
		return NIX_ERR_PARAM;

	req = mbox_alloc_msg_nix_set_rx_mode(mbox);
	if (req == NULL)
		return rc;

	if (enable)
		req->mode = NIX_RX_MODE_UCAST | NIX_RX_MODE_PROMISC;

	return mbox_process(mbox);
}

int
roc_nix_npc_mac_addr_set(struct roc_nix *roc_nix, uint8_t addr[])
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_set_mac_addr *req;

	req = mbox_alloc_msg_nix_set_mac_addr(mbox);
	mbox_memcpy(req->mac_addr, addr, PLT_ETHER_ADDR_LEN);
	return mbox_process(mbox);
}

int
roc_nix_npc_mac_addr_get(struct roc_nix *roc_nix, uint8_t *addr)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_get_mac_addr_rsp *rsp;
	int rc;

	mbox_alloc_msg_nix_get_mac_addr(mbox);
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	mbox_memcpy(addr, rsp->mac_addr, PLT_ETHER_ADDR_LEN);
	return 0;
}

int
roc_nix_npc_rx_ena_dis(struct roc_nix *roc_nix, bool enable)
{
	struct mbox *mbox = get_mbox(roc_nix);
	int rc;

	if (enable)
		mbox_alloc_msg_nix_lf_start_rx(mbox);
	else
		mbox_alloc_msg_nix_lf_stop_rx(mbox);

	rc = mbox_process(mbox);
	if (!rc)
		roc_nix->io_enabled = enable;
	return rc;
}

int
roc_nix_npc_mcast_config(struct roc_nix *roc_nix, bool mcast_enable,
			 bool prom_enable)

{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_rx_mode *req;
	int rc = -ENOSPC;

	if (roc_nix_is_vf_or_sdp(roc_nix))
		return 0;

	req = mbox_alloc_msg_nix_set_rx_mode(mbox);
	if (req == NULL)
		return rc;

	if (mcast_enable)
		req->mode = NIX_RX_MODE_ALLMULTI;
	if (prom_enable)
		req->mode = NIX_RX_MODE_PROMISC;

	return mbox_process(mbox);
}
