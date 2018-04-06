/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#include "axgbe_ethdev.h"
#include "axgbe_common.h"
#include "axgbe_phy.h"

static void axgbe_an37_clear_interrupts(struct axgbe_port *pdata)
{
	int reg;

	reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_STAT);
	reg &= ~AXGBE_AN_CL37_INT_MASK;
	XMDIO_WRITE(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_STAT, reg);
}

static void axgbe_an37_disable_interrupts(struct axgbe_port *pdata)
{
	int reg;

	reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_CTRL);
	reg &= ~AXGBE_AN_CL37_INT_MASK;
	XMDIO_WRITE(pdata, MDIO_MMD_VEND2, MDIO_VEND2_AN_CTRL, reg);

	reg = XMDIO_READ(pdata, MDIO_MMD_PCS, MDIO_PCS_DIG_CTRL);
	reg &= ~AXGBE_PCS_CL37_BP;
	XMDIO_WRITE(pdata, MDIO_MMD_PCS, MDIO_PCS_DIG_CTRL, reg);
}

static void axgbe_an73_clear_interrupts(struct axgbe_port *pdata)
{
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INT, 0);
}

static void axgbe_an73_disable_interrupts(struct axgbe_port *pdata)
{
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INTMASK, 0);
}

static void axgbe_an73_enable_interrupts(struct axgbe_port *pdata)
{
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INTMASK,
		    AXGBE_AN_CL73_INT_MASK);
}

static void axgbe_an_enable_interrupts(struct axgbe_port *pdata)
{
	switch (pdata->an_mode) {
	case AXGBE_AN_MODE_CL73:
	case AXGBE_AN_MODE_CL73_REDRV:
		axgbe_an73_enable_interrupts(pdata);
		break;
	case AXGBE_AN_MODE_CL37:
	case AXGBE_AN_MODE_CL37_SGMII:
		PMD_DRV_LOG(ERR, "Unsupported AN_MOD_37\n");
		break;
	default:
		break;
	}
}

static void axgbe_an_clear_interrupts_all(struct axgbe_port *pdata)
{
	axgbe_an73_clear_interrupts(pdata);
	axgbe_an37_clear_interrupts(pdata);
}

static void axgbe_an73_enable_kr_training(struct axgbe_port *pdata)
{
	unsigned int reg;

	reg = XMDIO_READ(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL);

	reg |= AXGBE_KR_TRAINING_ENABLE;
	XMDIO_WRITE(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL, reg);
}

static void axgbe_an73_disable_kr_training(struct axgbe_port *pdata)
{
	unsigned int reg;

	reg = XMDIO_READ(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL);

	reg &= ~AXGBE_KR_TRAINING_ENABLE;
	XMDIO_WRITE(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL, reg);
}

static void axgbe_kr_mode(struct axgbe_port *pdata)
{
	/* Enable KR training */
	axgbe_an73_enable_kr_training(pdata);

	/* Set MAC to 10G speed */
	pdata->hw_if.set_speed(pdata, SPEED_10000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, AXGBE_MODE_KR);
}

static void axgbe_kx_2500_mode(struct axgbe_port *pdata)
{
	/* Disable KR training */
	axgbe_an73_disable_kr_training(pdata);

	/* Set MAC to 2.5G speed */
	pdata->hw_if.set_speed(pdata, SPEED_2500);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, AXGBE_MODE_KX_2500);
}

static void axgbe_kx_1000_mode(struct axgbe_port *pdata)
{
	/* Disable KR training */
	axgbe_an73_disable_kr_training(pdata);

	/* Set MAC to 1G speed */
	pdata->hw_if.set_speed(pdata, SPEED_1000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, AXGBE_MODE_KX_1000);
}

static void axgbe_sfi_mode(struct axgbe_port *pdata)
{
	/* If a KR re-driver is present, change to KR mode instead */
	if (pdata->kr_redrv)
		return axgbe_kr_mode(pdata);

	/* Disable KR training */
	axgbe_an73_disable_kr_training(pdata);

	/* Set MAC to 10G speed */
	pdata->hw_if.set_speed(pdata, SPEED_10000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, AXGBE_MODE_SFI);
}

static void axgbe_x_mode(struct axgbe_port *pdata)
{
	/* Disable KR training */
	axgbe_an73_disable_kr_training(pdata);

	/* Set MAC to 1G speed */
	pdata->hw_if.set_speed(pdata, SPEED_1000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, AXGBE_MODE_X);
}

static void axgbe_sgmii_1000_mode(struct axgbe_port *pdata)
{
	/* Disable KR training */
	axgbe_an73_disable_kr_training(pdata);

	/* Set MAC to 1G speed */
	pdata->hw_if.set_speed(pdata, SPEED_1000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, AXGBE_MODE_SGMII_1000);
}

static void axgbe_sgmii_100_mode(struct axgbe_port *pdata)
{
	/* Disable KR training */
	axgbe_an73_disable_kr_training(pdata);

	/* Set MAC to 1G speed */
	pdata->hw_if.set_speed(pdata, SPEED_1000);

	/* Call PHY implementation support to complete rate change */
	pdata->phy_if.phy_impl.set_mode(pdata, AXGBE_MODE_SGMII_100);
}

static enum axgbe_mode axgbe_cur_mode(struct axgbe_port *pdata)
{
	return pdata->phy_if.phy_impl.cur_mode(pdata);
}

static bool axgbe_in_kr_mode(struct axgbe_port *pdata)
{
	return axgbe_cur_mode(pdata) == AXGBE_MODE_KR;
}

static void axgbe_change_mode(struct axgbe_port *pdata,
			      enum axgbe_mode mode)
{
	switch (mode) {
	case AXGBE_MODE_KX_1000:
		axgbe_kx_1000_mode(pdata);
		break;
	case AXGBE_MODE_KX_2500:
		axgbe_kx_2500_mode(pdata);
		break;
	case AXGBE_MODE_KR:
		axgbe_kr_mode(pdata);
		break;
	case AXGBE_MODE_SGMII_100:
		axgbe_sgmii_100_mode(pdata);
		break;
	case AXGBE_MODE_SGMII_1000:
		axgbe_sgmii_1000_mode(pdata);
		break;
	case AXGBE_MODE_X:
		axgbe_x_mode(pdata);
		break;
	case AXGBE_MODE_SFI:
		axgbe_sfi_mode(pdata);
		break;
	case AXGBE_MODE_UNKNOWN:
		break;
	default:
		PMD_DRV_LOG(ERR, "invalid operation mode requested (%u)\n", mode);
	}
}

static void axgbe_switch_mode(struct axgbe_port *pdata)
{
	axgbe_change_mode(pdata, pdata->phy_if.phy_impl.switch_mode(pdata));
}

static void axgbe_set_mode(struct axgbe_port *pdata,
			   enum axgbe_mode mode)
{
	if (mode == axgbe_cur_mode(pdata))
		return;

	axgbe_change_mode(pdata, mode);
}

static bool axgbe_use_mode(struct axgbe_port *pdata,
			   enum axgbe_mode mode)
{
	return pdata->phy_if.phy_impl.use_mode(pdata, mode);
}

static void axgbe_an37_set(struct axgbe_port *pdata, bool enable,
			   bool restart)
{
	unsigned int reg;

	reg = XMDIO_READ(pdata, MDIO_MMD_VEND2, MDIO_CTRL1);
	reg &= ~MDIO_VEND2_CTRL1_AN_ENABLE;

	if (enable)
		reg |= MDIO_VEND2_CTRL1_AN_ENABLE;

	if (restart)
		reg |= MDIO_VEND2_CTRL1_AN_RESTART;

	XMDIO_WRITE(pdata, MDIO_MMD_VEND2, MDIO_CTRL1, reg);
}

static void axgbe_an37_disable(struct axgbe_port *pdata)
{
	axgbe_an37_set(pdata, false, false);
	axgbe_an37_disable_interrupts(pdata);
}

static void axgbe_an73_set(struct axgbe_port *pdata, bool enable,
			   bool restart)
{
	unsigned int reg;

	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_CTRL1);
	reg &= ~MDIO_AN_CTRL1_ENABLE;

	if (enable)
		reg |= MDIO_AN_CTRL1_ENABLE;

	if (restart)
		reg |= MDIO_AN_CTRL1_RESTART;

	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_CTRL1, reg);
}

static void axgbe_an73_restart(struct axgbe_port *pdata)
{
	axgbe_an73_enable_interrupts(pdata);
	axgbe_an73_set(pdata, true, true);
}

static void axgbe_an73_disable(struct axgbe_port *pdata)
{
	axgbe_an73_set(pdata, false, false);
	axgbe_an73_disable_interrupts(pdata);
	pdata->an_start = 0;
}

static void axgbe_an_restart(struct axgbe_port *pdata)
{
	if (pdata->phy_if.phy_impl.an_pre)
		pdata->phy_if.phy_impl.an_pre(pdata);

	switch (pdata->an_mode) {
	case AXGBE_AN_MODE_CL73:
	case AXGBE_AN_MODE_CL73_REDRV:
		axgbe_an73_restart(pdata);
		break;
	case AXGBE_AN_MODE_CL37:
	case AXGBE_AN_MODE_CL37_SGMII:
		PMD_DRV_LOG(ERR, "Unsupported AN_MODE_CL37\n");
		break;
	default:
		break;
	}
}

static void axgbe_an_disable(struct axgbe_port *pdata)
{
	if (pdata->phy_if.phy_impl.an_post)
		pdata->phy_if.phy_impl.an_post(pdata);

	switch (pdata->an_mode) {
	case AXGBE_AN_MODE_CL73:
	case AXGBE_AN_MODE_CL73_REDRV:
		axgbe_an73_disable(pdata);
		break;
	case AXGBE_AN_MODE_CL37:
	case AXGBE_AN_MODE_CL37_SGMII:
		PMD_DRV_LOG(ERR, "Unsupported AN_MODE_CL37\n");
		break;
	default:
		break;
	}
}

static void axgbe_an_disable_all(struct axgbe_port *pdata)
{
	axgbe_an73_disable(pdata);
	axgbe_an37_disable(pdata);
}

static enum axgbe_an axgbe_an73_tx_training(struct axgbe_port *pdata,
					    enum axgbe_rx *state)
{
	unsigned int ad_reg, lp_reg, reg;

	*state = AXGBE_RX_COMPLETE;

	/* If we're not in KR mode then we're done */
	if (!axgbe_in_kr_mode(pdata))
		return AXGBE_AN_PAGE_RECEIVED;

	/* Enable/Disable FEC */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPA + 2);

	reg = XMDIO_READ(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_FECCTRL);
	reg &= ~(MDIO_PMA_10GBR_FECABLE_ABLE | MDIO_PMA_10GBR_FECABLE_ERRABLE);
	if ((ad_reg & 0xc000) && (lp_reg & 0xc000))
		reg |= pdata->fec_ability;
	XMDIO_WRITE(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_FECCTRL, reg);

	/* Start KR training */
	reg = XMDIO_READ(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL);
	if (reg & AXGBE_KR_TRAINING_ENABLE) {
		if (pdata->phy_if.phy_impl.kr_training_pre)
			pdata->phy_if.phy_impl.kr_training_pre(pdata);

		reg |= AXGBE_KR_TRAINING_START;
		XMDIO_WRITE(pdata, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL,
			    reg);

		if (pdata->phy_if.phy_impl.kr_training_post)
			pdata->phy_if.phy_impl.kr_training_post(pdata);
	}

	return AXGBE_AN_PAGE_RECEIVED;
}

static enum axgbe_an axgbe_an73_tx_xnp(struct axgbe_port *pdata,
				       enum axgbe_rx *state)
{
	u16 msg;

	*state = AXGBE_RX_XNP;

	msg = AXGBE_XNP_MCF_NULL_MESSAGE;
	msg |= AXGBE_XNP_MP_FORMATTED;

	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_XNP + 2, 0);
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_XNP + 1, 0);
	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_XNP, msg);

	return AXGBE_AN_PAGE_RECEIVED;
}

static enum axgbe_an axgbe_an73_rx_bpa(struct axgbe_port *pdata,
				       enum axgbe_rx *state)
{
	unsigned int link_support;
	unsigned int reg, ad_reg, lp_reg;

	/* Read Base Ability register 2 first */
	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPA + 1);

	/* Check for a supported mode, otherwise restart in a different one */
	link_support = axgbe_in_kr_mode(pdata) ? 0x80 : 0x20;
	if (!(reg & link_support))
		return AXGBE_AN_INCOMPAT_LINK;

	/* Check Extended Next Page support */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPA);

	return ((ad_reg & AXGBE_XNP_NP_EXCHANGE) ||
		(lp_reg & AXGBE_XNP_NP_EXCHANGE))
		? axgbe_an73_tx_xnp(pdata, state)
		: axgbe_an73_tx_training(pdata, state);
}

static enum axgbe_an axgbe_an73_rx_xnp(struct axgbe_port *pdata,
				       enum axgbe_rx *state)
{
	unsigned int ad_reg, lp_reg;

	/* Check Extended Next Page support */
	ad_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_XNP);
	lp_reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_LPX);

	return ((ad_reg & AXGBE_XNP_NP_EXCHANGE) ||
		(lp_reg & AXGBE_XNP_NP_EXCHANGE))
		? axgbe_an73_tx_xnp(pdata, state)
		: axgbe_an73_tx_training(pdata, state);
}

static enum axgbe_an axgbe_an73_page_received(struct axgbe_port *pdata)
{
	enum axgbe_rx *state;
	unsigned long an_timeout;
	enum axgbe_an ret;
	unsigned long ticks;

	if (!pdata->an_start) {
		pdata->an_start = rte_get_timer_cycles();
	} else {
		an_timeout = pdata->an_start +
			msecs_to_timer_cycles(AXGBE_AN_MS_TIMEOUT);
		ticks = rte_get_timer_cycles();
		if (time_after(ticks, an_timeout)) {
			/* Auto-negotiation timed out, reset state */
			pdata->kr_state = AXGBE_RX_BPA;
			pdata->kx_state = AXGBE_RX_BPA;

			pdata->an_start = rte_get_timer_cycles();
		}
	}

	state = axgbe_in_kr_mode(pdata) ? &pdata->kr_state
		: &pdata->kx_state;

	switch (*state) {
	case AXGBE_RX_BPA:
		ret = axgbe_an73_rx_bpa(pdata, state);
		break;
	case AXGBE_RX_XNP:
		ret = axgbe_an73_rx_xnp(pdata, state);
		break;
	default:
		ret = AXGBE_AN_ERROR;
	}

	return ret;
}

static enum axgbe_an axgbe_an73_incompat_link(struct axgbe_port *pdata)
{
	/* Be sure we aren't looping trying to negotiate */
	if (axgbe_in_kr_mode(pdata)) {
		pdata->kr_state = AXGBE_RX_ERROR;

		if (!(pdata->phy.advertising & ADVERTISED_1000baseKX_Full) &&
		    !(pdata->phy.advertising & ADVERTISED_2500baseX_Full))
			return AXGBE_AN_NO_LINK;

		if (pdata->kx_state != AXGBE_RX_BPA)
			return AXGBE_AN_NO_LINK;
	} else {
		pdata->kx_state = AXGBE_RX_ERROR;

		if (!(pdata->phy.advertising & ADVERTISED_10000baseKR_Full))
			return AXGBE_AN_NO_LINK;

		if (pdata->kr_state != AXGBE_RX_BPA)
			return AXGBE_AN_NO_LINK;
	}

	axgbe_an_disable(pdata);
	axgbe_switch_mode(pdata);
	axgbe_an_restart(pdata);

	return AXGBE_AN_INCOMPAT_LINK;
}

static void axgbe_an73_state_machine(struct axgbe_port *pdata)
{
	enum axgbe_an cur_state = pdata->an_state;

	if (!pdata->an_int)
		return;

next_int:
	if (pdata->an_int & AXGBE_AN_CL73_PG_RCV) {
		pdata->an_state = AXGBE_AN_PAGE_RECEIVED;
		pdata->an_int &= ~AXGBE_AN_CL73_PG_RCV;
	} else if (pdata->an_int & AXGBE_AN_CL73_INC_LINK) {
		pdata->an_state = AXGBE_AN_INCOMPAT_LINK;
		pdata->an_int &= ~AXGBE_AN_CL73_INC_LINK;
	} else if (pdata->an_int & AXGBE_AN_CL73_INT_CMPLT) {
		pdata->an_state = AXGBE_AN_COMPLETE;
		pdata->an_int &= ~AXGBE_AN_CL73_INT_CMPLT;
	} else {
		pdata->an_state = AXGBE_AN_ERROR;
	}

again:
	cur_state = pdata->an_state;

	switch (pdata->an_state) {
	case AXGBE_AN_READY:
		pdata->an_supported = 0;
		break;
	case AXGBE_AN_PAGE_RECEIVED:
		pdata->an_state = axgbe_an73_page_received(pdata);
		pdata->an_supported++;
		break;
	case AXGBE_AN_INCOMPAT_LINK:
		pdata->an_supported = 0;
		pdata->parallel_detect = 0;
		pdata->an_state = axgbe_an73_incompat_link(pdata);
		break;
	case AXGBE_AN_COMPLETE:
		pdata->parallel_detect = pdata->an_supported ? 0 : 1;
		break;
	case AXGBE_AN_NO_LINK:
		break;
	default:
		pdata->an_state = AXGBE_AN_ERROR;
	}

	if (pdata->an_state == AXGBE_AN_NO_LINK) {
		pdata->an_int = 0;
		axgbe_an73_clear_interrupts(pdata);
		pdata->eth_dev->data->dev_link.link_status =
			ETH_LINK_DOWN;
	} else if (pdata->an_state == AXGBE_AN_ERROR) {
		PMD_DRV_LOG(ERR, "error during auto-negotiation, state=%u\n",
			    cur_state);
		pdata->an_int = 0;
		axgbe_an73_clear_interrupts(pdata);
	}

	if (pdata->an_state >= AXGBE_AN_COMPLETE) {
		pdata->an_result = pdata->an_state;
		pdata->an_state = AXGBE_AN_READY;
		pdata->kr_state = AXGBE_RX_BPA;
		pdata->kx_state = AXGBE_RX_BPA;
		pdata->an_start = 0;
		if (pdata->phy_if.phy_impl.an_post)
			pdata->phy_if.phy_impl.an_post(pdata);
	}

	if (cur_state != pdata->an_state)
		goto again;

	if (pdata->an_int)
		goto next_int;

	axgbe_an73_enable_interrupts(pdata);
}

static void axgbe_an73_isr(struct axgbe_port *pdata)
{
	/* Disable AN interrupts */
	axgbe_an73_disable_interrupts(pdata);

	/* Save the interrupt(s) that fired */
	pdata->an_int = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_INT);

	if (pdata->an_int) {
		/* Clear the interrupt(s) that fired and process them */
		XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_INT, ~pdata->an_int);
		pthread_mutex_lock(&pdata->an_mutex);
		axgbe_an73_state_machine(pdata);
		pthread_mutex_unlock(&pdata->an_mutex);
	} else {
		/* Enable AN interrupts */
		axgbe_an73_enable_interrupts(pdata);
	}
}

static void axgbe_an_isr(struct axgbe_port *pdata)
{
	switch (pdata->an_mode) {
	case AXGBE_AN_MODE_CL73:
	case AXGBE_AN_MODE_CL73_REDRV:
		axgbe_an73_isr(pdata);
		break;
	case AXGBE_AN_MODE_CL37:
	case AXGBE_AN_MODE_CL37_SGMII:
		PMD_DRV_LOG(ERR, "AN_MODE_37 not supported\n");
		break;
	default:
		break;
	}
}

static void axgbe_an_combined_isr(struct axgbe_port *pdata)
{
	axgbe_an_isr(pdata);
}

static void axgbe_an73_init(struct axgbe_port *pdata)
{
	unsigned int advertising, reg;

	advertising = pdata->phy_if.phy_impl.an_advertising(pdata);

	/* Set up Advertisement register 3 first */
	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2);
	if (advertising & ADVERTISED_10000baseR_FEC)
		reg |= 0xc000;
	else
		reg &= ~0xc000;

	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2, reg);

	/* Set up Advertisement register 2 next */
	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 1);
	if (advertising & ADVERTISED_10000baseKR_Full)
		reg |= 0x80;
	else
		reg &= ~0x80;

	if ((advertising & ADVERTISED_1000baseKX_Full) ||
	    (advertising & ADVERTISED_2500baseX_Full))
		reg |= 0x20;
	else
		reg &= ~0x20;

	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 1, reg);

	/* Set up Advertisement register 1 last */
	reg = XMDIO_READ(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
	if (advertising & ADVERTISED_Pause)
		reg |= 0x400;
	else
		reg &= ~0x400;

	if (advertising & ADVERTISED_Asym_Pause)
		reg |= 0x800;
	else
		reg &= ~0x800;

	/* We don't intend to perform XNP */
	reg &= ~AXGBE_XNP_NP_EXCHANGE;

	XMDIO_WRITE(pdata, MDIO_MMD_AN, MDIO_AN_ADVERTISE, reg);
}

static void axgbe_an_init(struct axgbe_port *pdata)
{
	/* Set up advertisement registers based on current settings */
	pdata->an_mode = pdata->phy_if.phy_impl.an_mode(pdata);
	switch (pdata->an_mode) {
	case AXGBE_AN_MODE_CL73:
	case AXGBE_AN_MODE_CL73_REDRV:
		axgbe_an73_init(pdata);
		break;
	case AXGBE_AN_MODE_CL37:
	case AXGBE_AN_MODE_CL37_SGMII:
		PMD_DRV_LOG(ERR, "Unsupported AN_CL37\n");
		break;
	default:
		break;
	}
}

static void axgbe_phy_adjust_link(struct axgbe_port *pdata)
{
	if (pdata->phy.link) {
		/* Flow control support */
		pdata->pause_autoneg = pdata->phy.pause_autoneg;

		if (pdata->tx_pause != (unsigned int)pdata->phy.tx_pause) {
			pdata->hw_if.config_tx_flow_control(pdata);
			pdata->tx_pause = pdata->phy.tx_pause;
		}

		if (pdata->rx_pause != (unsigned int)pdata->phy.rx_pause) {
			pdata->hw_if.config_rx_flow_control(pdata);
			pdata->rx_pause = pdata->phy.rx_pause;
		}

		/* Speed support */
		if (pdata->phy_speed != pdata->phy.speed)
			pdata->phy_speed = pdata->phy.speed;
		if (pdata->phy_link != pdata->phy.link)
			pdata->phy_link = pdata->phy.link;
	} else if (pdata->phy_link) {
		pdata->phy_link = 0;
		pdata->phy_speed = SPEED_UNKNOWN;
	}
}

static int axgbe_phy_config_fixed(struct axgbe_port *pdata)
{
	enum axgbe_mode mode;

	/* Disable auto-negotiation */
	axgbe_an_disable(pdata);

	/* Set specified mode for specified speed */
	mode = pdata->phy_if.phy_impl.get_mode(pdata, pdata->phy.speed);
	switch (mode) {
	case AXGBE_MODE_KX_1000:
	case AXGBE_MODE_KX_2500:
	case AXGBE_MODE_KR:
	case AXGBE_MODE_SGMII_100:
	case AXGBE_MODE_SGMII_1000:
	case AXGBE_MODE_X:
	case AXGBE_MODE_SFI:
		break;
	case AXGBE_MODE_UNKNOWN:
	default:
		return -EINVAL;
	}

	/* Validate duplex mode */
	if (pdata->phy.duplex != DUPLEX_FULL)
		return -EINVAL;

	axgbe_set_mode(pdata, mode);

	return 0;
}

static int __axgbe_phy_config_aneg(struct axgbe_port *pdata)
{
	int ret;

	axgbe_set_bit(AXGBE_LINK_INIT, &pdata->dev_state);
	pdata->link_check = rte_get_timer_cycles();

	ret = pdata->phy_if.phy_impl.an_config(pdata);
	if (ret)
		return ret;

	if (pdata->phy.autoneg != AUTONEG_ENABLE) {
		ret = axgbe_phy_config_fixed(pdata);
		if (ret || !pdata->kr_redrv)
			return ret;
	}

	/* Disable auto-negotiation interrupt */
	rte_intr_disable(&pdata->pci_dev->intr_handle);

	/* Start auto-negotiation in a supported mode */
	if (axgbe_use_mode(pdata, AXGBE_MODE_KR)) {
		axgbe_set_mode(pdata, AXGBE_MODE_KR);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_KX_2500)) {
		axgbe_set_mode(pdata, AXGBE_MODE_KX_2500);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_KX_1000)) {
		axgbe_set_mode(pdata, AXGBE_MODE_KX_1000);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_SFI)) {
		axgbe_set_mode(pdata, AXGBE_MODE_SFI);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_X)) {
		axgbe_set_mode(pdata, AXGBE_MODE_X);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_SGMII_1000)) {
		axgbe_set_mode(pdata, AXGBE_MODE_SGMII_1000);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_SGMII_100)) {
		axgbe_set_mode(pdata, AXGBE_MODE_SGMII_100);
	} else {
		rte_intr_enable(&pdata->pci_dev->intr_handle);
		return -EINVAL;
	}

	/* Disable and stop any in progress auto-negotiation */
	axgbe_an_disable_all(pdata);

	/* Clear any auto-negotitation interrupts */
	axgbe_an_clear_interrupts_all(pdata);

	pdata->an_result = AXGBE_AN_READY;
	pdata->an_state = AXGBE_AN_READY;
	pdata->kr_state = AXGBE_RX_BPA;
	pdata->kx_state = AXGBE_RX_BPA;

	/* Re-enable auto-negotiation interrupt */
	rte_intr_enable(&pdata->pci_dev->intr_handle);

	axgbe_an_init(pdata);
	axgbe_an_restart(pdata);

	return 0;
}

static int axgbe_phy_config_aneg(struct axgbe_port *pdata)
{
	int ret;

	pthread_mutex_lock(&pdata->an_mutex);

	ret = __axgbe_phy_config_aneg(pdata);
	if (ret)
		axgbe_set_bit(AXGBE_LINK_ERR, &pdata->dev_state);
	else
		axgbe_clear_bit(AXGBE_LINK_ERR, &pdata->dev_state);

	pthread_mutex_unlock(&pdata->an_mutex);

	return ret;
}

static bool axgbe_phy_aneg_done(struct axgbe_port *pdata)
{
	return pdata->an_result == AXGBE_AN_COMPLETE;
}

static void axgbe_check_link_timeout(struct axgbe_port *pdata)
{
	unsigned long link_timeout;
	unsigned long ticks;

	link_timeout = pdata->link_check + (AXGBE_LINK_TIMEOUT *
					    2 *  rte_get_timer_hz());
	ticks = rte_get_timer_cycles();
	if (time_after(ticks, link_timeout))
		axgbe_phy_config_aneg(pdata);
}

static enum axgbe_mode axgbe_phy_status_aneg(struct axgbe_port *pdata)
{
	return pdata->phy_if.phy_impl.an_outcome(pdata);
}

static void axgbe_phy_status_result(struct axgbe_port *pdata)
{
	enum axgbe_mode mode;

	pdata->phy.lp_advertising = 0;

	if ((pdata->phy.autoneg != AUTONEG_ENABLE) || pdata->parallel_detect)
		mode = axgbe_cur_mode(pdata);
	else
		mode = axgbe_phy_status_aneg(pdata);

	switch (mode) {
	case AXGBE_MODE_SGMII_100:
		pdata->phy.speed = SPEED_100;
		break;
	case AXGBE_MODE_X:
	case AXGBE_MODE_KX_1000:
	case AXGBE_MODE_SGMII_1000:
		pdata->phy.speed = SPEED_1000;
		break;
	case AXGBE_MODE_KX_2500:
		pdata->phy.speed = SPEED_2500;
		break;
	case AXGBE_MODE_KR:
	case AXGBE_MODE_SFI:
		pdata->phy.speed = SPEED_10000;
		break;
	case AXGBE_MODE_UNKNOWN:
	default:
		pdata->phy.speed = SPEED_UNKNOWN;
	}

	pdata->phy.duplex = DUPLEX_FULL;

	axgbe_set_mode(pdata, mode);
}

static void axgbe_phy_status(struct axgbe_port *pdata)
{
	unsigned int link_aneg;
	int an_restart;

	if (axgbe_test_bit(AXGBE_LINK_ERR, &pdata->dev_state)) {
		pdata->phy.link = 0;
		goto adjust_link;
	}

	link_aneg = (pdata->phy.autoneg == AUTONEG_ENABLE);

	pdata->phy.link = pdata->phy_if.phy_impl.link_status(pdata,
							     &an_restart);
	if (an_restart) {
		axgbe_phy_config_aneg(pdata);
		return;
	}

	if (pdata->phy.link) {
		if (link_aneg && !axgbe_phy_aneg_done(pdata)) {
			axgbe_check_link_timeout(pdata);
			return;
		}
		axgbe_phy_status_result(pdata);
		if (axgbe_test_bit(AXGBE_LINK_INIT, &pdata->dev_state))
			axgbe_clear_bit(AXGBE_LINK_INIT, &pdata->dev_state);
	} else {
		if (axgbe_test_bit(AXGBE_LINK_INIT, &pdata->dev_state)) {
			axgbe_check_link_timeout(pdata);

			if (link_aneg)
				return;
		}
		axgbe_phy_status_result(pdata);
	}

adjust_link:
	axgbe_phy_adjust_link(pdata);
}

static void axgbe_phy_stop(struct axgbe_port *pdata)
{
	if (!pdata->phy_started)
		return;
	/* Indicate the PHY is down */
	pdata->phy_started = 0;
	/* Disable auto-negotiation */
	axgbe_an_disable_all(pdata);
	pdata->phy_if.phy_impl.stop(pdata);
	pdata->phy.link = 0;
	axgbe_phy_adjust_link(pdata);
}

static int axgbe_phy_start(struct axgbe_port *pdata)
{
	int ret;

	ret = pdata->phy_if.phy_impl.start(pdata);
	if (ret)
		return ret;
	/* Set initial mode - call the mode setting routines
	 * directly to insure we are properly configured
	 */
	if (axgbe_use_mode(pdata, AXGBE_MODE_KR)) {
		axgbe_kr_mode(pdata);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_KX_2500)) {
		axgbe_kx_2500_mode(pdata);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_KX_1000)) {
		axgbe_kx_1000_mode(pdata);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_SFI)) {
		axgbe_sfi_mode(pdata);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_X)) {
		axgbe_x_mode(pdata);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_SGMII_1000)) {
		axgbe_sgmii_1000_mode(pdata);
	} else if (axgbe_use_mode(pdata, AXGBE_MODE_SGMII_100)) {
		axgbe_sgmii_100_mode(pdata);
	} else {
		ret = -EINVAL;
		goto err_stop;
	}
	/* Indicate the PHY is up and running */
	pdata->phy_started = 1;
	axgbe_an_init(pdata);
	axgbe_an_enable_interrupts(pdata);
	return axgbe_phy_config_aneg(pdata);

err_stop:
	pdata->phy_if.phy_impl.stop(pdata);

	return ret;
}

static int axgbe_phy_reset(struct axgbe_port *pdata)
{
	int ret;

	ret = pdata->phy_if.phy_impl.reset(pdata);
	if (ret)
		return ret;

	/* Disable auto-negotiation for now */
	axgbe_an_disable_all(pdata);

	/* Clear auto-negotiation interrupts */
	axgbe_an_clear_interrupts_all(pdata);

	return 0;
}

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
	phy_if->phy_reset       = axgbe_phy_reset;
	phy_if->phy_start       = axgbe_phy_start;
	phy_if->phy_stop        = axgbe_phy_stop;
	phy_if->phy_status      = axgbe_phy_status;
	phy_if->phy_config_aneg = axgbe_phy_config_aneg;
	phy_if->an_isr          = axgbe_an_combined_isr;
}
