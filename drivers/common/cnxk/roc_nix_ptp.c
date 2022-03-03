/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

#define PTP_FREQ_ADJUST (1 << 9)

static inline struct mbox *
get_mbox(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	return dev->mbox;
}

int
roc_nix_ptp_rx_ena_dis(struct roc_nix *roc_nix, int enable)
{
	struct mbox *mbox = get_mbox(roc_nix);

	if (roc_nix_is_vf_or_sdp(roc_nix) || roc_nix_is_lbk(roc_nix))
		return NIX_ERR_PARAM;

	if (enable)
		mbox_alloc_msg_cgx_ptp_rx_enable(mbox);
	else
		mbox_alloc_msg_cgx_ptp_rx_disable(mbox);

	return mbox_process(mbox);
}

int
roc_nix_ptp_tx_ena_dis(struct roc_nix *roc_nix, int enable)
{
	struct mbox *mbox = get_mbox(roc_nix);

	if (roc_nix_is_vf_or_sdp(roc_nix) || roc_nix_is_lbk(roc_nix))
		return NIX_ERR_PARAM;

	if (enable)
		mbox_alloc_msg_nix_lf_ptp_tx_enable(mbox);
	else
		mbox_alloc_msg_nix_lf_ptp_tx_disable(mbox);

	return mbox_process(mbox);
}

int
roc_nix_ptp_clock_read(struct roc_nix *roc_nix, uint64_t *clock, uint64_t *tsc,
		       uint8_t is_pmu)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct ptp_req *req;
	struct ptp_rsp *rsp;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_ptp_op(mbox);
	if (req == NULL)
		return rc;
	req->op = PTP_OP_GET_CLOCK;
	req->is_pmu = is_pmu;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	if (clock)
		*clock = rsp->clk;

	if (tsc)
		*tsc = rsp->tsc;

	return 0;
}

int
roc_nix_ptp_sync_time_adjust(struct roc_nix *roc_nix, int64_t delta)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct ptp_req *req;
	struct ptp_rsp *rsp;
	int rc = -ENOSPC;

	if (roc_nix_is_vf_or_sdp(roc_nix) || roc_nix_is_lbk(roc_nix))
		return NIX_ERR_PARAM;

	if ((delta <= -PTP_FREQ_ADJUST) || (delta >= PTP_FREQ_ADJUST))
		return NIX_ERR_INVALID_RANGE;

	req = mbox_alloc_msg_ptp_op(mbox);
	if (req == NULL)
		return rc;
	req->op = PTP_OP_ADJFINE;
	req->scaled_ppm = delta;

	return mbox_process_msg(mbox, (void *)&rsp);
}

int
roc_nix_ptp_info_cb_register(struct roc_nix *roc_nix,
			     ptp_info_update_t ptp_update)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	if (ptp_update == NULL)
		return NIX_ERR_PARAM;

	dev->ops->ptp_info_update = (ptp_info_t)ptp_update;
	return 0;
}

void
roc_nix_ptp_info_cb_unregister(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	dev->ops->ptp_info_update = NULL;
}

bool
roc_nix_ptp_is_enable(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	return nix->ptp_en;
}
