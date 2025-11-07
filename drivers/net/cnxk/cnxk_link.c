/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_ethdev.h"

const enum roc_nix_link_mode mac_to_ethtool_mode[CGX_LMAC_TYPE_MAX][2] = {
	[CGX_LMAC_TYPE_SGMII][ROC_NIX_LINK_DUPLEX_HALF] = ROC_NIX_LINK_MODE_1000BASET_HD,
	[CGX_LMAC_TYPE_SGMII][ROC_NIX_LINK_DUPLEX_FULL] = ROC_NIX_LINK_MODE_1000BASET_FD,
	[CGX_LMAC_TYPE_10G_R][ROC_NIX_LINK_DUPLEX_FULL] = ROC_NIX_LINK_MODE_10000BASESR_FD,
	[CGX_LMAC_TYPE_QSGMII][ROC_NIX_LINK_DUPLEX_HALF] = ROC_NIX_LINK_MODE_1000BASET_HD,
	[CGX_LMAC_TYPE_QSGMII][ROC_NIX_LINK_DUPLEX_FULL] = ROC_NIX_LINK_MODE_10000BASET_FD,
};

const enum roc_nix_link_mode rte_to_ethtool_mode[ROC_NIX_LINK_SPEED_MAX] = {
	0,
	ROC_NIX_LINK_MODE_10BASET_HD,
	ROC_NIX_LINK_MODE_10BASET_FD,
	ROC_NIX_LINK_MODE_100BASET_HD,
	ROC_NIX_LINK_MODE_100BASET_FD,
	ROC_NIX_LINK_MODE_1000BASET_FD,
	ROC_NIX_LINK_MODE_2500BASEX_FD,
	0,
	ROC_NIX_LINK_MODE_10000BASESR_FD,
	0,
	ROC_NIX_LINK_MODE_25000BASESR_FD,
	ROC_NIX_LINK_MODE_40000BASELR4_FD,
	ROC_NIX_LINK_MODE_50000BASELR_ER_FR_FD,
	0,
	ROC_NIX_LINK_MODE_100000BASELR4_ER4_FD,
	0,
	0,
};

static uint64_t
nix_link_advertising_get(struct cnxk_eth_dev *dev, struct roc_nix_link_info *link_info)
{
	struct roc_nix_mac_fwdata fwdata;
	struct roc_nix_link_info linfo;
	uint64_t advertise = 0;
	int bit, rc;

	memset(&fwdata, 0, sizeof(fwdata));
	rc = roc_nix_mac_fwdata_get(&dev->nix, &fwdata);
	if (rc) {
		plt_err("Failed to get MAC firmware data");
		goto exit;
	}

	memset(&linfo, 0, sizeof(linfo));
	rc = roc_nix_mac_link_info_get(&dev->nix, &linfo);
	if (rc) {
		plt_err("Failed to get MAC link info");
		goto exit;
	}

	if (link_info->autoneg) {
		if (!fwdata.supported_an) {
			plt_err("Autoneg is not supported");
			goto exit;
		} else {
			for (bit = 0; bit < ROC_NIX_LINK_SPEED_MAX; bit++) {
				if (link_info->speed_bitmask & BIT_ULL(bit))
					advertise |= rte_to_ethtool_mode[bit];
			}
			goto exit;
		}
	}

	advertise |= mac_to_ethtool_mode[linfo.lmac_type_id][link_info->full_duplex];
exit:
	return advertise;
}

void
cnxk_nix_toggle_flag_link_cfg(struct cnxk_eth_dev *dev, bool set)
{
	if (set)
		dev->flags |= CNXK_LINK_CFG_IN_PROGRESS_F;
	else
		dev->flags &= ~CNXK_LINK_CFG_IN_PROGRESS_F;

	/* Update link info for LBK */
	if (!set &&
	    (roc_nix_is_lbk(&dev->nix) || roc_nix_is_sdp(&dev->nix) || roc_nix_is_esw(&dev->nix))) {
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

int
cnxk_nix_link_info_configure(struct rte_eth_dev *eth_dev)
{
	uint32_t speed_map[] = {
		RTE_ETH_SPEED_NUM_NONE, RTE_ETH_SPEED_NUM_10M,  RTE_ETH_SPEED_NUM_10M,
		RTE_ETH_SPEED_NUM_100M, RTE_ETH_SPEED_NUM_100M, RTE_ETH_SPEED_NUM_1G,
		RTE_ETH_SPEED_NUM_2_5G, RTE_ETH_SPEED_NUM_5G,   RTE_ETH_SPEED_NUM_10G,
		RTE_ETH_SPEED_NUM_20G,  RTE_ETH_SPEED_NUM_25G,  RTE_ETH_SPEED_NUM_40G,
		RTE_ETH_SPEED_NUM_50G,  RTE_ETH_SPEED_NUM_56G,  RTE_ETH_SPEED_NUM_100G,
		RTE_ETH_SPEED_NUM_200G, RTE_ETH_SPEED_NUM_400G
	};
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rte_eth_conf *conf = &data->dev_conf;
	uint32_t link_speeds = conf->link_speeds;
	struct roc_nix_link_info link_info = {0};
	struct roc_nix *nix = &dev->nix;
	uint32_t speed = link_speeds;
	bool fixed;

	plt_info("User passed link configuration: %x", link_speeds);

	if (!roc_nix_is_pf(nix) || link_speeds == RTE_ETH_LINK_SPEED_AUTONEG)
		return 0;

	fixed = link_speeds & RTE_ETH_LINK_SPEED_FIXED ? true : false;
	if (fixed) {
		if (rte_popcount32(link_speeds) == 1) {
			plt_err("Desired speed is not specified in FIXED mode");
			return -EINVAL;
		}

		if (rte_popcount32(link_speeds) > 2) {
			plt_err("Multiple speeds can't be configured in FIXED mode");
			return -EINVAL;
		}

		link_info.autoneg = 0;
	} else {
		link_info.autoneg = 1;
	}

	speed >>= 1;
	link_info.speed = speed_map[rte_bsf32(speed) + 1];
	link_info.speed_bitmask = link_speeds & ~RTE_ETH_LINK_SPEED_FIXED;
	link_info.full_duplex = ((link_speeds & RTE_ETH_LINK_SPEED_10M_HD) ||
				 (link_speeds & RTE_ETH_LINK_SPEED_100M_HD)) ?
				  ROC_NIX_LINK_DUPLEX_HALF :
				  ROC_NIX_LINK_DUPLEX_FULL;
	link_info.advertising = nix_link_advertising_get(dev, &link_info);
	if (link_info.advertising == 0) {
		plt_err("advertising bitmap is not set");
		return -EINVAL;
	}

	plt_info("Following link settings are sent to firmware:");
	plt_info("Advertised modes: %" PRIX64, link_info.advertising);
	plt_info("speed: %u", link_info.speed);
	plt_info("duplex: %s", link_info.full_duplex == ROC_NIX_LINK_DUPLEX_HALF ?
						"half-duplex" : "full-duplex");
	plt_info("autoneg: %s", link_info.autoneg ? "enabled" : "disabled");
	return roc_nix_mac_link_info_set(nix, &link_info);
}
