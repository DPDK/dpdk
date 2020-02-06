/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_common.h>
#include <rte_ethdev_pci.h>

#include "otx2_ethdev.h"

void
otx2_nix_toggle_flag_link_cfg(struct otx2_eth_dev *dev, bool set)
{
	if (set)
		dev->flags |= OTX2_LINK_CFG_IN_PROGRESS_F;
	else
		dev->flags &= ~OTX2_LINK_CFG_IN_PROGRESS_F;

	rte_wmb();
}

static inline int
nix_wait_for_link_cfg(struct otx2_eth_dev *dev)
{
	uint16_t wait = 1000;

	do {
		rte_rmb();
		if (!(dev->flags & OTX2_LINK_CFG_IN_PROGRESS_F))
			break;
		wait--;
		rte_delay_ms(1);
	} while (wait);

	return wait ? 0 : -1;
}

static void
nix_link_status_print(struct rte_eth_dev *eth_dev, struct rte_eth_link *link)
{
	if (link && link->link_status)
		otx2_info("Port %d: Link Up - speed %u Mbps - %s",
			  (int)(eth_dev->data->port_id),
			  (uint32_t)link->link_speed,
			  link->link_duplex == ETH_LINK_FULL_DUPLEX ?
			  "full-duplex" : "half-duplex");
	else
		otx2_info("Port %d: Link Down", (int)(eth_dev->data->port_id));
}

void
otx2_eth_dev_link_status_update(struct otx2_dev *dev,
				struct cgx_link_user_info *link)
{
	struct otx2_eth_dev *otx2_dev = (struct otx2_eth_dev *)dev;
	struct rte_eth_link eth_link;
	struct rte_eth_dev *eth_dev;

	if (!link || !dev)
		return;

	eth_dev = otx2_dev->eth_dev;
	if (!eth_dev || !eth_dev->data->dev_conf.intr_conf.lsc)
		return;

	if (nix_wait_for_link_cfg(otx2_dev)) {
		otx2_err("Timeout waiting for link_cfg to complete");
		return;
	}

	eth_link.link_status = link->link_up;
	eth_link.link_speed = link->speed;
	eth_link.link_autoneg = ETH_LINK_AUTONEG;
	eth_link.link_duplex = link->full_duplex;

	/* Print link info */
	nix_link_status_print(eth_dev, &eth_link);

	/* Update link info */
	rte_eth_linkstatus_set(eth_dev, &eth_link);

	/* Set the flag and execute application callbacks */
	_rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_INTR_LSC, NULL);
}

static int
lbk_link_update(struct rte_eth_link *link)
{
	link->link_status = ETH_LINK_UP;
	link->link_speed = ETH_SPEED_NUM_100G;
	link->link_autoneg = ETH_LINK_FIXED;
	link->link_duplex = ETH_LINK_FULL_DUPLEX;
	return 0;
}

static int
cgx_link_update(struct otx2_eth_dev *dev, struct rte_eth_link *link)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct cgx_link_info_msg *rsp;
	int rc;
	otx2_mbox_alloc_msg_cgx_get_linkinfo(mbox);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	link->link_status = rsp->link_info.link_up;
	link->link_speed = rsp->link_info.speed;
	link->link_autoneg = ETH_LINK_AUTONEG;

	if (rsp->link_info.full_duplex)
		link->link_duplex = rsp->link_info.full_duplex;
	return 0;
}

int
otx2_nix_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_eth_link link;
	int rc;

	RTE_SET_USED(wait_to_complete);
	memset(&link, 0, sizeof(struct rte_eth_link));

	if (otx2_dev_is_sdp(dev))
		return 0;

	if (otx2_dev_is_lbk(dev))
		rc = lbk_link_update(&link);
	else
		rc = cgx_link_update(dev, &link);

	if (rc)
		return rc;

	return rte_eth_linkstatus_set(eth_dev, &link);
}

static int
nix_dev_set_link_state(struct rte_eth_dev *eth_dev, uint8_t enable)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct cgx_set_link_state_msg *req;

	req = otx2_mbox_alloc_msg_cgx_set_link_state(mbox);
	req->enable = enable;
	return otx2_mbox_process(mbox);
}

int
otx2_nix_dev_set_link_up(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc, i;

	if (otx2_dev_is_vf_or_sdp(dev))
		return -ENOTSUP;

	rc = nix_dev_set_link_state(eth_dev, 1);
	if (rc)
		goto done;

	/* Start tx queues  */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		otx2_nix_tx_queue_start(eth_dev, i);

done:
	return rc;
}

int
otx2_nix_dev_set_link_down(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int i;

	if (otx2_dev_is_vf_or_sdp(dev))
		return -ENOTSUP;

	/* Stop tx queues  */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		otx2_nix_tx_queue_stop(eth_dev, i);

	return nix_dev_set_link_state(eth_dev, 0);
}
