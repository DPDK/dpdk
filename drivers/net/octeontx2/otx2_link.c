/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_common.h>
#include <ethdev_pci.h>

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
			  link->link_duplex == RTE_ETH_LINK_FULL_DUPLEX ?
			  "full-duplex" : "half-duplex");
	else
		otx2_info("Port %d: Link Down", (int)(eth_dev->data->port_id));
}

void
otx2_eth_dev_link_status_get(struct otx2_dev *dev,
			     struct cgx_link_user_info *link)
{
	struct otx2_eth_dev *otx2_dev = (struct otx2_eth_dev *)dev;
	struct rte_eth_link eth_link;
	struct rte_eth_dev *eth_dev;

	if (!link || !dev)
		return;

	eth_dev = otx2_dev->eth_dev;
	if (!eth_dev)
		return;

	rte_eth_linkstatus_get(eth_dev, &eth_link);

	link->link_up = eth_link.link_status;
	link->speed = eth_link.link_speed;
	link->an = eth_link.link_autoneg;
	link->full_duplex = eth_link.link_duplex;
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
	eth_link.link_autoneg = RTE_ETH_LINK_AUTONEG;
	eth_link.link_duplex = link->full_duplex;

	otx2_dev->speed = link->speed;
	otx2_dev->duplex = link->full_duplex;

	/* Print link info */
	nix_link_status_print(eth_dev, &eth_link);

	/* Update link info */
	rte_eth_linkstatus_set(eth_dev, &eth_link);

	/* Set the flag and execute application callbacks */
	rte_eth_dev_callback_process(eth_dev, RTE_ETH_EVENT_INTR_LSC, NULL);
}

static int
lbk_link_update(struct rte_eth_link *link)
{
	link->link_status = RTE_ETH_LINK_UP;
	link->link_speed = RTE_ETH_SPEED_NUM_100G;
	link->link_autoneg = RTE_ETH_LINK_FIXED;
	link->link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
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
	link->link_autoneg = RTE_ETH_LINK_AUTONEG;

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

	if (!eth_dev->data->dev_started || otx2_dev_is_sdp(dev))
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

static int
cgx_change_mode(struct otx2_eth_dev *dev, struct cgx_set_link_mode_args *cfg)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct cgx_set_link_mode_req *req;

	req = otx2_mbox_alloc_msg_cgx_set_link_mode(mbox);
	req->args.speed = cfg->speed;
	req->args.duplex = cfg->duplex;
	req->args.an = cfg->an;

	return otx2_mbox_process(mbox);
}

#define SPEED_NONE 0
static inline uint32_t
nix_parse_link_speeds(struct otx2_eth_dev *dev, uint32_t link_speeds)
{
	uint32_t link_speed = SPEED_NONE;

	/* 50G and 100G to be supported for board version C0 and above */
	if (!otx2_dev_is_Ax(dev)) {
		if (link_speeds & RTE_ETH_LINK_SPEED_100G)
			link_speed = 100000;
		if (link_speeds & RTE_ETH_LINK_SPEED_50G)
			link_speed = 50000;
	}
	if (link_speeds & RTE_ETH_LINK_SPEED_40G)
		link_speed = 40000;
	if (link_speeds & RTE_ETH_LINK_SPEED_25G)
		link_speed = 25000;
	if (link_speeds & RTE_ETH_LINK_SPEED_20G)
		link_speed = 20000;
	if (link_speeds & RTE_ETH_LINK_SPEED_10G)
		link_speed = 10000;
	if (link_speeds & RTE_ETH_LINK_SPEED_5G)
		link_speed = 5000;
	if (link_speeds & RTE_ETH_LINK_SPEED_1G)
		link_speed = 1000;

	return link_speed;
}

static inline uint8_t
nix_parse_eth_link_duplex(uint32_t link_speeds)
{
	if ((link_speeds & RTE_ETH_LINK_SPEED_10M_HD) ||
			(link_speeds & RTE_ETH_LINK_SPEED_100M_HD))
		return RTE_ETH_LINK_HALF_DUPLEX;
	else
		return RTE_ETH_LINK_FULL_DUPLEX;
}

int
otx2_apply_link_speed(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_eth_conf *conf = &eth_dev->data->dev_conf;
	struct cgx_set_link_mode_args cfg;

	/* If VF/SDP/LBK, link attributes cannot be changed */
	if (otx2_dev_is_vf_or_sdp(dev) || otx2_dev_is_lbk(dev))
		return 0;

	memset(&cfg, 0, sizeof(struct cgx_set_link_mode_args));
	cfg.speed = nix_parse_link_speeds(dev, conf->link_speeds);
	if (cfg.speed != SPEED_NONE && cfg.speed != dev->speed) {
		cfg.duplex = nix_parse_eth_link_duplex(conf->link_speeds);
		cfg.an = (conf->link_speeds & RTE_ETH_LINK_SPEED_FIXED) == 0;

		return cgx_change_mode(dev, &cfg);
	}
	return 0;
}
