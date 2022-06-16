/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_ethdev.h"

void
cnxk_nix_toggle_flag_link_cfg(struct cnxk_eth_dev *dev, bool set)
{
	if (set)
		dev->flags |= CNXK_LINK_CFG_IN_PROGRESS_F;
	else
		dev->flags &= ~CNXK_LINK_CFG_IN_PROGRESS_F;

	/* Update link info for LBK */
	if (!set && (roc_nix_is_lbk(&dev->nix) || roc_nix_is_sdp(&dev->nix))) {
		struct rte_eth_link link;

		link.link_status = RTE_ETH_LINK_UP;
		link.link_speed = RTE_ETH_SPEED_NUM_100G;
		link.link_autoneg = RTE_ETH_LINK_FIXED;
		link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		rte_eth_linkstatus_set(dev->eth_dev, &link);
	}

	rte_wmb();
}

static inline int
nix_wait_for_link_cfg(struct cnxk_eth_dev *dev)
{
	uint16_t wait = 1000;

	do {
		rte_atomic_thread_fence(__ATOMIC_ACQUIRE);
		if (!(dev->flags & CNXK_LINK_CFG_IN_PROGRESS_F))
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
		plt_info("Port %d: Link Up - speed %u Mbps - %s",
			 (int)(eth_dev->data->port_id),
			 (uint32_t)link->link_speed,
			 link->link_duplex == RTE_ETH_LINK_FULL_DUPLEX
				 ? "full-duplex"
				 : "half-duplex");
	else
		plt_info("Port %d: Link Down", (int)(eth_dev->data->port_id));
}

void
cnxk_eth_dev_link_status_get_cb(struct roc_nix *nix,
				struct roc_nix_link_info *link)
{
	struct cnxk_eth_dev *dev = (struct cnxk_eth_dev *)nix;
	struct rte_eth_link eth_link;
	struct rte_eth_dev *eth_dev;

	if (!link || !nix)
		return;

	eth_dev = dev->eth_dev;
	if (!eth_dev)
		return;

	rte_eth_linkstatus_get(eth_dev, &eth_link);

	link->status = eth_link.link_status;
	link->speed = eth_link.link_speed;
	link->autoneg = eth_link.link_autoneg;
	link->full_duplex = eth_link.link_duplex;
}

void
cnxk_eth_dev_link_status_cb(struct roc_nix *nix, struct roc_nix_link_info *link)
{
	struct cnxk_eth_dev *dev = (struct cnxk_eth_dev *)nix;
	struct rte_eth_link eth_link;
	struct rte_eth_dev *eth_dev;

	if (!link || !nix)
		return;

	eth_dev = dev->eth_dev;
	if (!eth_dev || !eth_dev->data->dev_conf.intr_conf.lsc)
		return;

	if (nix_wait_for_link_cfg(dev)) {
		plt_err("Timeout waiting for link_cfg to complete");
		return;
	}

	eth_link.link_status = link->status;
	eth_link.link_speed = link->speed;
	eth_link.link_autoneg = RTE_ETH_LINK_AUTONEG;
	eth_link.link_duplex = link->full_duplex;

	/* Print link info */
	nix_link_status_print(eth_dev, &eth_link);

	/* Update link info */
	rte_eth_linkstatus_set(eth_dev, &eth_link);

	/* Set the flag and execute application callbacks */
	rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_INTR_LSC, NULL);
}

int
cnxk_nix_link_update(struct rte_eth_dev *eth_dev, int wait_to_complete)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix_link_info info;
	struct rte_eth_link link;
	int rc;

	RTE_SET_USED(wait_to_complete);
	memset(&link, 0, sizeof(struct rte_eth_link));

	if (!eth_dev->data->dev_started)
		return 0;

	if (roc_nix_is_lbk(&dev->nix) || roc_nix_is_sdp(&dev->nix)) {
		link.link_status = RTE_ETH_LINK_UP;
		link.link_speed = RTE_ETH_SPEED_NUM_100G;
		link.link_autoneg = RTE_ETH_LINK_FIXED;
		link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	} else {
		rc = roc_nix_mac_link_info_get(&dev->nix, &info);
		if (rc)
			return rc;
		link.link_status = info.status;
		link.link_speed = info.speed;
		link.link_autoneg = RTE_ETH_LINK_AUTONEG;
		if (info.full_duplex)
			link.link_duplex = info.full_duplex;
	}

	return rte_eth_linkstatus_set(eth_dev, &link);
}
