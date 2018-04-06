/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#include "axgbe_ethdev.h"
#include "axgbe_common.h"
#include "axgbe_phy.h"

static int axgbe_phy_best_advertised_speed(struct axgbe_port *pdata)
{
	if (pdata->phy.advertising & ADVERTISED_10000baseKR_Full)
		return SPEED_10000;
	else if (pdata->phy.advertising & ADVERTISED_10000baseT_Full)
		return SPEED_10000;
	else if (pdata->phy.advertising & ADVERTISED_2500baseX_Full)
		return SPEED_2500;
	else if (pdata->phy.advertising & ADVERTISED_1000baseKX_Full)
		return SPEED_1000;
	else if (pdata->phy.advertising & ADVERTISED_1000baseT_Full)
		return SPEED_1000;
	else if (pdata->phy.advertising & ADVERTISED_100baseT_Full)
		return SPEED_100;

	return SPEED_UNKNOWN;
}

static int axgbe_phy_init(struct axgbe_port *pdata)
{
	int ret;

	pdata->mdio_mmd = MDIO_MMD_PCS;

	/* Check for FEC support */
	pdata->fec_ability = XMDIO_READ(pdata, MDIO_MMD_PMAPMD,
					MDIO_PMA_10GBR_FECABLE);
	pdata->fec_ability &= (MDIO_PMA_10GBR_FECABLE_ABLE |
			       MDIO_PMA_10GBR_FECABLE_ERRABLE);

	/* Setup the phy (including supported features) */
	ret = pdata->phy_if.phy_impl.init(pdata);
	if (ret)
		return ret;
	pdata->phy.advertising = pdata->phy.supported;

	pdata->phy.address = 0;

	if (pdata->phy.advertising & ADVERTISED_Autoneg) {
		pdata->phy.autoneg = AUTONEG_ENABLE;
		pdata->phy.speed = SPEED_UNKNOWN;
		pdata->phy.duplex = DUPLEX_UNKNOWN;
	} else {
		pdata->phy.autoneg = AUTONEG_DISABLE;
		pdata->phy.speed = axgbe_phy_best_advertised_speed(pdata);
		pdata->phy.duplex = DUPLEX_FULL;
	}

	pdata->phy.link = 0;

	pdata->phy.pause_autoneg = pdata->pause_autoneg;
	pdata->phy.tx_pause = pdata->tx_pause;
	pdata->phy.rx_pause = pdata->rx_pause;

	/* Fix up Flow Control advertising */
	pdata->phy.advertising &= ~ADVERTISED_Pause;
	pdata->phy.advertising &= ~ADVERTISED_Asym_Pause;

	if (pdata->rx_pause) {
		pdata->phy.advertising |= ADVERTISED_Pause;
		pdata->phy.advertising |= ADVERTISED_Asym_Pause;
	}

	if (pdata->tx_pause)
		pdata->phy.advertising ^= ADVERTISED_Asym_Pause;
	return 0;
}

void axgbe_init_function_ptrs_phy(struct axgbe_phy_if *phy_if)
{
	phy_if->phy_init        = axgbe_phy_init;
}
