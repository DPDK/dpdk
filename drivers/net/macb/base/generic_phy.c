/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */

#include "generic_phy.h"
#include "macb_hw.h"

static uint32_t genphy_get_an(struct macb *bp, uint16_t phyad, u16 addr)
{
	int advert;

	advert = macb_mdio_read(bp, phyad, addr);

	return genphy_lpa_to_ethtool_lpa_t(advert);
}

static int phy_poll_reset(struct phy_device *phydev)
{
	struct macb *bp = phydev->bp;
	uint32_t retries = 12;
	int32_t ret;
	uint16_t phyad = phydev->phyad;

	do {
		rte_delay_ms(50);
		ret = macb_mdio_read(bp, phyad, GENERIC_PHY_BMCR);
		if (ret < 0)
			return ret;
	} while (ret & BMCR_RESET && --retries);
	if (ret & BMCR_RESET)
		return -ETIMEDOUT;

	rte_delay_ms(1);
	return 0;
}

static int genphy_soft_reset(struct phy_device *phydev)
{
	struct macb *bp = phydev->bp;
	uint32_t ctrl;
	uint16_t phyad = phydev->phyad;

	/* soft reset phy */
	ctrl = macb_mdio_read(bp, phyad, GENERIC_PHY_BMCR);
	ctrl |= BMCR_RESET;
	macb_mdio_write(bp, phyad, GENERIC_PHY_BMCR, ctrl);

	return phy_poll_reset(phydev);
}

static int genphy_resume(struct phy_device *phydev)
{
	struct macb *bp = phydev->bp;
	uint32_t ctrl;
	uint16_t phyad = phydev->phyad;

	/* phy power up */
	ctrl = macb_mdio_read(bp, phyad, GENERIC_PHY_BMCR);
	ctrl &= ~BMCR_PDOWN;
	macb_mdio_write(bp, phyad, GENERIC_PHY_BMCR, ctrl);
	rte_delay_ms(100);
	return 0;
}

static void genphy_suspend(struct phy_device *phydev)
{
	struct macb *bp = phydev->bp;
	uint32_t ctrl;
	uint16_t phyad = phydev->phyad;

	/* phy power down */
	ctrl = macb_mdio_read(bp, phyad, GENERIC_PHY_BMCR);
	ctrl |= BMCR_PDOWN;
	macb_mdio_write(bp, phyad, GENERIC_PHY_BMCR, ctrl);
}

static int genphy_force_speed_duplex(struct phy_device *phydev)
{
	struct macb *bp = phydev->bp;
	uint32_t ctrl;
	uint16_t phyad = phydev->phyad;

	if (bp->autoneg) {
		ctrl = macb_mdio_read(bp, phyad, GENERIC_PHY_BMCR);
		ctrl |= BMCR_ANENABLE;
		macb_mdio_write(bp, phyad, GENERIC_PHY_BMCR, ctrl);
		rte_delay_ms(10);
	} else {
		/* disable autoneg first */
		ctrl = macb_mdio_read(bp, phyad, GENERIC_PHY_BMCR);
		ctrl &= ~BMCR_ANENABLE;

		if (bp->duplex == DUPLEX_FULL)
			ctrl |= BMCR_FULLDPLX;
		else
			ctrl &= ~BMCR_FULLDPLX;

		switch (bp->speed) {
		case SPEED_10:
			ctrl &= ~BMCR_SPEED1000;
			ctrl &= ~BMCR_SPEED100;
			break;
		case SPEED_100:
			ctrl |= BMCR_SPEED100;
			ctrl &= ~BMCR_SPEED1000;
			break;
		case SPEED_1000:
			ctrl |= BMCR_ANENABLE;
			bp->autoneg = AUTONEG_ENABLE;
			break;
		case SPEED_2500:
			ctrl |= BMCR_ANENABLE;
			bp->autoneg = AUTONEG_ENABLE;
			break;
		}
		macb_mdio_write(bp, phyad, GENERIC_PHY_BMCR, ctrl);
		phydev->autoneg = bp->autoneg;
		rte_delay_ms(10);
	}

	return 0;
}

static int genphy_check_for_link(struct phy_device *phydev)
{
	struct macb *bp = phydev->bp;
	int bmsr;

	/* Do a fake read */
	bmsr = macb_mdio_read(bp, bp->phyad, GENERIC_PHY_BMSR);
	if (bmsr < 0)
		return bmsr;

	bmsr = macb_mdio_read(bp, bp->phyad, GENERIC_PHY_BMSR);
	phydev->link = bmsr & BMSR_LSTATUS;

	return phydev->link;
}

static int genphy_read_status(struct phy_device *phydev)
{
	struct macb *bp = phydev->bp;
	uint16_t bmcr, bmsr, ctrl1000 = 0, stat1000 = 0;
	uint32_t advertising, lp_advertising;
	uint32_t nego;
	uint16_t phyad = phydev->phyad;

	/* Do a fake read */
	bmsr = macb_mdio_read(bp, phyad, GENERIC_PHY_BMSR);

	bmsr = macb_mdio_read(bp, phyad, GENERIC_PHY_BMSR);
	bmcr = macb_mdio_read(bp, phyad, GENERIC_PHY_BMCR);

	if (bmcr & BMCR_ANENABLE) {
		ctrl1000 = macb_mdio_read(bp, phyad, GENERIC_PHY_CTRL1000);
		stat1000 = macb_mdio_read(bp, phyad, GENERIC_PHY_STAT1000);

		advertising = ADVERTISED_Autoneg;
		advertising |= genphy_get_an(bp, phyad, GENERIC_PHY_ADVERISE);
		advertising |= genphy_ctrl1000_to_ethtool_adv_t(ctrl1000);

		if (bmsr & BMSR_ANEGCOMPLETE) {
			lp_advertising = genphy_get_an(bp, phyad, GENERIC_PHY_LPA);
			lp_advertising |= genphy_stat1000_to_ethtool_lpa_t(stat1000);
		} else {
			lp_advertising = 0;
		}

		nego = advertising & lp_advertising;
		if (nego & (ADVERTISED_1000baseT_Full | ADVERTISED_1000baseT_Half)) {
			phydev->speed = SPEED_1000;
			phydev->duplex = !!(nego & ADVERTISED_1000baseT_Full);
		} else if (nego &
				(ADVERTISED_100baseT_Full | ADVERTISED_100baseT_Half)) {
			phydev->speed = SPEED_100;
			phydev->duplex = !!(nego & ADVERTISED_100baseT_Full);
		} else {
			phydev->speed = SPEED_10;
			phydev->duplex = !!(nego & ADVERTISED_10baseT_Full);
		}
	} else {
		phydev->speed = ((bmcr & BMCR_SPEED1000 && (bmcr & BMCR_SPEED100) == 0)
						 ? SPEED_1000
						 : ((bmcr & BMCR_SPEED100) ? SPEED_100 : SPEED_10));
		phydev->duplex = (bmcr & BMCR_FULLDPLX) ? DUPLEX_FULL : DUPLEX_HALF;
	}

	return 0;
}

int macb_usxgmii_pcs_resume(struct phy_device *phydev)
{
	u32 config;
	struct macb *bp = phydev->bp;

	config = gem_readl(bp, USX_CONTROL);

	/* enable signal */
	config &= ~(GEM_BIT(RX_SYNC_RESET));
	config |= GEM_BIT(SIGNAL_OK) | GEM_BIT(TX_EN);
	gem_writel(bp, USX_CONTROL, config);

	return 0;
}

void macb_usxgmii_pcs_suspend(struct phy_device *phydev)
{
	uint32_t config;
	struct macb *bp = phydev->bp;

	config = gem_readl(bp, USX_CONTROL);
	config |= GEM_BIT(RX_SYNC_RESET);
	/* disable signal */
	config &= ~(GEM_BIT(SIGNAL_OK) | GEM_BIT(TX_EN));
	gem_writel(bp, USX_CONTROL, config);
	rte_delay_ms(1);
}

int macb_usxgmii_pcs_check_for_link(struct phy_device *phydev)
{
	int link;
	struct macb *bp = phydev->bp;

	link = (int)GEM_BFEXT(BLOCK_LOCK, gem_readl(bp, USX_STATUS));
	return link;
}

int macb_gbe_pcs_check_for_link(struct phy_device *phydev)
{
	int link;
	struct macb *bp = phydev->bp;

	link = (int)MACB_BFEXT(NSR_LINK, macb_readl(bp, NSR));
	return link;
}

const struct phy_driver genphy_driver = {
	.phy_id		= 0xffffffff,
	.phy_id_mask	= 0xffffffff,
	.name		= "Generic PHY",
	.soft_reset	= genphy_soft_reset,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.check_for_link		= genphy_check_for_link,
	.read_status		= genphy_read_status,
	.force_speed_duplex = genphy_force_speed_duplex,
};

const struct phy_driver macb_gbe_pcs_driver = {
	.phy_id		= 0xffffffff,
	.phy_id_mask	= 0xffffffff,
	.name		= "Macb gbe pcs PHY",
	.soft_reset	= NULL,
	.suspend	= NULL,
	.resume		= NULL,
	.check_for_link		= macb_gbe_pcs_check_for_link,
	.read_status		= NULL,
	.force_speed_duplex = NULL,
};

const struct phy_driver macb_usxgmii_pcs_driver = {
	.phy_id		= 0xffffffff,
	.phy_id_mask	= 0xffffffff,
	.name		= "Macb usxgmii pcs PHY",
	.soft_reset	= NULL,
	.suspend	= macb_usxgmii_pcs_suspend,
	.resume		= macb_usxgmii_pcs_resume,
	.check_for_link		= macb_usxgmii_pcs_check_for_link,
	.read_status		= NULL,
	.force_speed_duplex = NULL,
};
