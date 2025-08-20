/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */

#include <linux/mii.h>
#include <ctype.h>
#include "macb_uio.h"

#define MACB_MDIO_TIMEOUT 1000000 /* in usecs */

bool macb_is_gem(struct macb *bp)
{
	return !!(bp->caps & MACB_CAPS_MACB_IS_GEM);
}

static bool hw_is_gem(struct macb *bp, bool native_io)
{
	u32 id;
	id = macb_readl(bp, MID);
	return MACB_BFEXT(IDNUM, id) >= 0x2;
}

bool macb_hw_is_native_io(struct macb *bp)
{
	u32 value = MACB_BIT(LLB);

	macb_writel(bp, NCR, value);
	value = macb_readl(bp, NCR);
	macb_writel(bp, NCR, 0);

	return value == MACB_BIT(LLB);
}

u32 macb_dbw(struct macb *bp)
{
	switch (GEM_BFEXT(DBWDEF, gem_readl(bp, DCFG1))) {
	case 4:
		bp->data_bus_width = 128;
		return GEM_BF(DBW, GEM_DBW128);
	case 2:
		bp->data_bus_width = 64;
		return GEM_BF(DBW, GEM_DBW64);
	case 1:
	default:
		bp->data_bus_width = 32;
		return GEM_BF(DBW, GEM_DBW32);
	}
}

void macb_probe_queues(uintptr_t base, bool native_io, unsigned int *queue_mask,
					   unsigned int *num_queues)
{
	unsigned int hw_q;

	*queue_mask = 0x1;
	*num_queues = 1;

	/* bit 0 is never set but queue 0 always exists */
	*queue_mask =
		(rte_le_to_cpu_32(rte_read32((void *)(base + GEM_DCFG6)))) & 0xff;

	*queue_mask |= 0x1;

	for (hw_q = 1; hw_q < MACB_MAX_QUEUES; ++hw_q)
		if (*queue_mask & (1 << hw_q))
			(*num_queues)++;
}

void macb_configure_caps(struct macb *bp, const struct macb_config *dt_conf)
{
	u32 dcfg;

	if (dt_conf)
		bp->caps = dt_conf->caps;

	if (hw_is_gem(bp, bp->native_io)) {
		bp->caps |= MACB_CAPS_MACB_IS_GEM;

		dcfg = gem_readl(bp, DCFG1);
		if (GEM_BFEXT(IRQCOR, dcfg) == 0)
			bp->caps |= MACB_CAPS_ISR_CLEAR_ON_WRITE;

		dcfg = gem_readl(bp, DCFG2);
		if ((dcfg & (GEM_BIT(RX_PKT_BUFF) | GEM_BIT(TX_PKT_BUFF))) == 0)
			bp->caps |= MACB_CAPS_FIFO_MODE;
	}
}

int macb_iomem_init(const char *name, struct macb *bp, phys_addr_t paddr)
{
	int ret;

	if (macb_uio_exist(name)) {
		ret = macb_uio_init(name, &bp->iomem);
		if (ret) {
			MACB_LOG(ERR, "failed to init uio device.");
			return -EFAULT;
		}
	} else {
		MACB_LOG(ERR, "uio device %s not exist.", name);
		return -EFAULT;
	}

	ret = macb_uio_map(bp->iomem, &bp->paddr, (void **)(&bp->base), paddr);
	if (ret) {
		MACB_LOG(ERR, "Failed to remap macb uio device.");
		macb_uio_deinit(bp->iomem);
		return -EFAULT;
	}

	return 0;
}

int macb_iomem_deinit(struct macb *bp)
{
	macb_uio_unmap(bp->iomem);
	macb_uio_deinit(bp->iomem);
	return 0;
}

void macb_get_stats(struct macb *bp)
{
	struct macb_queue *queue;
	unsigned int i, q, idx;
	unsigned long *stat;

	u64 *p = &bp->hw_stats.gem.tx_octets_31_0;

	for (i = 0; i < GEM_STATS_LEN; ++i, ++p) {
		u32 offset = gem_statistics[i].offset;
		u64 val = macb_reg_readl(bp, offset);

		*p += val;

		if (offset == GEM_OCTTXL || offset == GEM_OCTRXL) {
			/* Add GEM_OCTTXH, GEM_OCTRXH */
			val = macb_reg_readl(bp, offset + 4);
			*(++p) += val;
		}
	}
}

static int macb_mdio_wait_for_idle(struct macb *bp)
{
	uint32_t val;
	uint64_t timeout = 0;
	for (;;) {
		val = macb_readl(bp, NSR);
		if (val & MACB_BIT(IDLE))
			break;
		if (timeout >= MACB_MDIO_TIMEOUT)
			break;
		timeout++;
		usleep(1);
	}
	return (val & MACB_BIT(IDLE)) ? 0 : -ETIMEDOUT;
}

int macb_mdio_read(struct macb *bp, uint16_t phy_id, uint32_t regnum)
{
	int32_t status;

	status = macb_mdio_wait_for_idle(bp);
	if (status < 0)
		return status;

	if (regnum & MII_ADDR_C45) {
		macb_writel(bp, MAN,
					(MACB_BF(SOF, MACB_MAN_C45_SOF) |
					 MACB_BF(RW, MACB_MAN_C45_ADDR) | MACB_BF(PHYA, phy_id) |
					 MACB_BF(REGA, (regnum >> 16) & 0x1F) |
					 MACB_BF(DATA, regnum & 0xFFFF) |
					 MACB_BF(CODE, MACB_MAN_C45_CODE)));

		status = macb_mdio_wait_for_idle(bp);
		if (status < 0)
			return status;

		macb_writel(bp, MAN,
					(MACB_BF(SOF, MACB_MAN_C45_SOF) |
					 MACB_BF(RW, MACB_MAN_C45_READ) | MACB_BF(PHYA, phy_id) |
					 MACB_BF(REGA, (regnum >> 16) & 0x1F) |
					 MACB_BF(CODE, MACB_MAN_C45_CODE)));
	} else {
		macb_writel(bp, MAN,
					(MACB_BF(SOF, MACB_MAN_C22_SOF) |
					 MACB_BF(RW, MACB_MAN_C22_READ) | MACB_BF(PHYA, phy_id) |
					 MACB_BF(REGA, regnum) | MACB_BF(CODE, MACB_MAN_C22_CODE)));
	}

	/* wait for end of transfer */
	while (!MACB_BFEXT(IDLE, macb_readl(bp, NSR)))
		;

	status = MACB_BFEXT(DATA, macb_readl(bp, MAN));

	return status;
}

int macb_mdio_write(struct macb *bp, uint16_t phy_id, uint32_t regnum,
					uint16_t value)
{
	int32_t status;
	status = macb_mdio_wait_for_idle(bp);
	if (status < 0)
		return status;

	if (regnum & MII_ADDR_C45) {
		macb_writel(bp, MAN,
					(MACB_BF(SOF, MACB_MAN_C45_SOF) |
					 MACB_BF(RW, MACB_MAN_C45_ADDR) | MACB_BF(PHYA, phy_id) |
					 MACB_BF(REGA, (regnum >> 16) & 0x1F) |
					 MACB_BF(DATA, regnum & 0xFFFF) |
					 MACB_BF(CODE, MACB_MAN_C45_CODE)));

		status = macb_mdio_wait_for_idle(bp);
		if (status < 0)
			return status;

		macb_writel(bp, MAN,
					(MACB_BF(SOF, MACB_MAN_C45_SOF) |
					 MACB_BF(RW, MACB_MAN_C45_WRITE) | MACB_BF(PHYA, phy_id) |
					 MACB_BF(REGA, (regnum >> 16) & 0x1F) |
					 MACB_BF(CODE, MACB_MAN_C45_CODE) | MACB_BF(DATA, value)));

	} else {
		macb_writel(bp, MAN,
					(MACB_BF(SOF, MACB_MAN_C22_SOF) |
					 MACB_BF(RW, MACB_MAN_C22_WRITE) | MACB_BF(PHYA, phy_id) |
					 MACB_BF(REGA, regnum) | MACB_BF(CODE, MACB_MAN_C22_CODE) |
					 MACB_BF(DATA, value)));
	}

	/* wait for end of transfer */
	while (!MACB_BFEXT(IDLE, macb_readl(bp, NSR)))
		;

	return 0;
}

void macb_gem1p0_sel_clk(struct macb *bp)
{
	int speed = 0;

	if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_SGMII) {
		if (bp->speed == SPEED_2500) {
			gem_writel(bp, DIV_SEL0_LN, 0x1);		/*0x1c08*/
			gem_writel(bp, DIV_SEL1_LN, 0x2);		/*0x1c0c*/
			gem_writel(bp, PMA_XCVR_POWER_STATE, 0x1);	/*0x1c10*/
			gem_writel(bp, TX_CLK_SEL0, 0x0);		/*0x1c20*/
			gem_writel(bp, TX_CLK_SEL1, 0x1);		/*0x1c24*/
			gem_writel(bp, TX_CLK_SEL2, 0x1);		/*0x1c28*/
			gem_writel(bp, TX_CLK_SEL3, 0x1);		/*0x1c2c*/
			gem_writel(bp, RX_CLK_SEL0, 0x1);		/*0x1c30*/
			gem_writel(bp, RX_CLK_SEL1, 0x0);		/*0x1c34*/
			gem_writel(bp, TX_CLK_SEL3_0, 0x0);		/*0x1c70*/
			gem_writel(bp, TX_CLK_SEL4_0, 0x0);		/*0x1c74*/
			gem_writel(bp, RX_CLK_SEL3_0, 0x0);		/*0x1c78*/
			gem_writel(bp, RX_CLK_SEL4_0, 0x0);		/*0x1c7c*/
			speed = GEM_SPEED_2500;
		} else if (bp->speed == SPEED_1000) {
			gem_writel(bp, DIV_SEL0_LN, 0x4);		/*0x1c08*/
			gem_writel(bp, DIV_SEL1_LN, 0x8);		/*0x1c0c*/
			gem_writel(bp, PMA_XCVR_POWER_STATE, 0x1);	/*0x1c10*/
			gem_writel(bp, TX_CLK_SEL0, 0x0);		/*0x1c20*/
			gem_writel(bp, TX_CLK_SEL1, 0x0);		/*0x1c24*/
			gem_writel(bp, TX_CLK_SEL2, 0x0);		/*0x1c28*/
			gem_writel(bp, TX_CLK_SEL3, 0x1);		/*0x1c2c*/
			gem_writel(bp, RX_CLK_SEL0, 0x1);		/*0x1c30*/
			gem_writel(bp, RX_CLK_SEL1, 0x0);		/*0x1c34*/
			gem_writel(bp, TX_CLK_SEL3_0, 0x0);		/*0x1c70*/
			gem_writel(bp, TX_CLK_SEL4_0, 0x0);		/*0x1c74*/
			gem_writel(bp, RX_CLK_SEL3_0, 0x0);		/*0x1c78*/
			gem_writel(bp, RX_CLK_SEL4_0, 0x0);		/*0x1c7c*/
			speed = GEM_SPEED_1000;
		} else if (bp->speed == SPEED_100 || bp->speed == SPEED_10) {
			gem_writel(bp, DIV_SEL0_LN, 0x4);		/*0x1c08*/
			gem_writel(bp, DIV_SEL1_LN, 0x8);		/*0x1c0c*/
			gem_writel(bp, PMA_XCVR_POWER_STATE, 0x1);	/*0x1c10*/
			gem_writel(bp, TX_CLK_SEL0, 0x0);		/*0x1c20*/
			gem_writel(bp, TX_CLK_SEL1, 0x0);		/*0x1c24*/
			gem_writel(bp, TX_CLK_SEL2, 0x1);		/*0x1c28*/
			gem_writel(bp, TX_CLK_SEL3, 0x1);		/*0x1c2c*/
			gem_writel(bp, RX_CLK_SEL0, 0x1);		/*0x1c30*/
			gem_writel(bp, RX_CLK_SEL1, 0x0);		/*0x1c34*/
			gem_writel(bp, TX_CLK_SEL3_0, 0x1);		/*0x1c70*/
			gem_writel(bp, TX_CLK_SEL4_0, 0x0);		/*0x1c74*/
			gem_writel(bp, RX_CLK_SEL3_0, 0x0);		/*0x1c78*/
			gem_writel(bp, RX_CLK_SEL4_0, 0x1);		/*0x1c7c*/
			speed = GEM_SPEED_100;
		}
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_RGMII) {
		if (bp->speed == SPEED_1000) {
			gem_writel(bp, MII_SELECT, 0x1);		/*0x1c18*/
			gem_writel(bp, SEL_MII_ON_RGMII, 0x0);		/*0x1c1c*/
			gem_writel(bp, TX_CLK_SEL0, 0x0);		/*0x1c20*/
			gem_writel(bp, TX_CLK_SEL1, 0x1);		/*0x1c24*/
			gem_writel(bp, TX_CLK_SEL2, 0x0);		/*0x1c28*/
			gem_writel(bp, TX_CLK_SEL3, 0x0);		/*0x1c2c*/
			gem_writel(bp, RX_CLK_SEL0, 0x0);		/*0x1c30*/
			gem_writel(bp, RX_CLK_SEL1, 0x1);		/*0x1c34*/
			gem_writel(bp, CLK_250M_DIV10_DIV100_SEL, 0x0); /*0x1c38*/
			gem_writel(bp, RX_CLK_SEL5, 0x1);		/*0x1c48*/
			gem_writel(bp, RGMII_TX_CLK_SEL0, 0x1);		/*0x1c80*/
			gem_writel(bp, RGMII_TX_CLK_SEL1, 0x0);		/*0x1c84*/
			speed = GEM_SPEED_1000;
		} else if (bp->speed == SPEED_100) {
			gem_writel(bp, MII_SELECT, 0x1);		/*0x1c18*/
			gem_writel(bp, SEL_MII_ON_RGMII, 0x0);		/*0x1c1c*/
			gem_writel(bp, TX_CLK_SEL0, 0x0);		/*0x1c20*/
			gem_writel(bp, TX_CLK_SEL1, 0x1);		/*0x1c24*/
			gem_writel(bp, TX_CLK_SEL2, 0x0);		/*0x1c28*/
			gem_writel(bp, TX_CLK_SEL3, 0x0);		/*0x1c2c*/
			gem_writel(bp, RX_CLK_SEL0, 0x0);		/*0x1c30*/
			gem_writel(bp, RX_CLK_SEL1, 0x1);		/*0x1c34*/
			gem_writel(bp, CLK_250M_DIV10_DIV100_SEL, 0x0); /*0x1c38*/
			gem_writel(bp, RX_CLK_SEL5, 0x1);		/*0x1c48*/
			gem_writel(bp, RGMII_TX_CLK_SEL0, 0x0);		/*0x1c80*/
			gem_writel(bp, RGMII_TX_CLK_SEL1, 0x0);		/*0x1c84*/
			speed = GEM_SPEED_100;
		} else {
			gem_writel(bp, MII_SELECT, 0x1);		/*0x1c18*/
			gem_writel(bp, SEL_MII_ON_RGMII, 0x0);		/*0x1c1c*/
			gem_writel(bp, TX_CLK_SEL0, 0x0);		/*0x1c20*/
			gem_writel(bp, TX_CLK_SEL1, 0x1);		/*0x1c24*/
			gem_writel(bp, TX_CLK_SEL2, 0x0);		/*0x1c28*/
			gem_writel(bp, TX_CLK_SEL3, 0x0);		/*0x1c2c*/
			gem_writel(bp, RX_CLK_SEL0, 0x0);		/*0x1c30*/
			gem_writel(bp, RX_CLK_SEL1, 0x1);		/*0x1c34*/
			gem_writel(bp, CLK_250M_DIV10_DIV100_SEL, 0x1);	/*0x1c38*/
			gem_writel(bp, RX_CLK_SEL5, 0x1);		/*0x1c48*/
			gem_writel(bp, RGMII_TX_CLK_SEL0, 0x0);		/*0x1c80*/
			gem_writel(bp, RGMII_TX_CLK_SEL1, 0x0);		/*0x1c84*/
			speed = GEM_SPEED_100;
		}
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_RMII) {
		speed = GEM_SPEED_100;
		gem_writel(bp, RX_CLK_SEL5, 0x1);			/*0x1c48*/
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_100BASEX) {
		gem_writel(bp, DIV_SEL0_LN, 0x4);			/*0x1c08*/
		gem_writel(bp, DIV_SEL1_LN, 0x8);			/*0x1c0c*/
		gem_writel(bp, PMA_XCVR_POWER_STATE, 0x1);		/*0x1c10*/
		gem_writel(bp, TX_CLK_SEL0, 0x0);			/*0x1c20*/
		gem_writel(bp, TX_CLK_SEL1, 0x0);			/*0x1c24*/
		gem_writel(bp, TX_CLK_SEL2, 0x1);			/*0x1c28*/
		gem_writel(bp, TX_CLK_SEL3, 0x1);			/*0x1c2c*/
		gem_writel(bp, RX_CLK_SEL0, 0x1);			/*0x1c30*/
		gem_writel(bp, RX_CLK_SEL1, 0x0);			/*0x1c34*/
		gem_writel(bp, TX_CLK_SEL3_0, 0x1);			/*0x1c70*/
		gem_writel(bp, TX_CLK_SEL4_0, 0x0);			/*0x1c74*/
		gem_writel(bp, RX_CLK_SEL3_0, 0x0);			/*0x1c78*/
		gem_writel(bp, RX_CLK_SEL4_0, 0x1);			/*0x1c7c*/
		speed = GEM_SPEED_100;
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_1000BASEX) {
		gem_writel(bp, DIV_SEL0_LN, 0x4);			/*0x1c08*/
		gem_writel(bp, DIV_SEL1_LN, 0x8);			/*0x1c0c*/
		gem_writel(bp, PMA_XCVR_POWER_STATE, 0x1);		/*0x1c10*/
		gem_writel(bp, TX_CLK_SEL0, 0x0);			/*0x1c20*/
		gem_writel(bp, TX_CLK_SEL1, 0x0);			/*0x1c24*/
		gem_writel(bp, TX_CLK_SEL2, 0x0);			/*0x1c28*/
		gem_writel(bp, TX_CLK_SEL3, 0x1);			/*0x1c2c*/
		gem_writel(bp, RX_CLK_SEL0, 0x1);			/*0x1c30*/
		gem_writel(bp, RX_CLK_SEL1, 0x0);			/*0x1c34*/
		gem_writel(bp, TX_CLK_SEL3_0, 0x0);			/*0x1c70*/
		gem_writel(bp, TX_CLK_SEL4_0, 0x0);			/*0x1c74*/
		gem_writel(bp, RX_CLK_SEL3_0, 0x0);			/*0x1c78*/
		gem_writel(bp, RX_CLK_SEL4_0, 0x0);			/*0x1c7c*/
		speed = GEM_SPEED_1000;
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_2500BASEX) {
		gem_writel(bp, DIV_SEL0_LN, 0x1);			/*0x1c08*/
		gem_writel(bp, DIV_SEL1_LN, 0x2);			/*0x1c0c*/
		gem_writel(bp, PMA_XCVR_POWER_STATE, 0x1);		/*0x1c10*/
		gem_writel(bp, TX_CLK_SEL0, 0x0);			/*0x1c20*/
		gem_writel(bp, TX_CLK_SEL1, 0x1);			/*0x1c24*/
		gem_writel(bp, TX_CLK_SEL2, 0x1);			/*0x1c28*/
		gem_writel(bp, TX_CLK_SEL3, 0x1);			/*0x1c2c*/
		gem_writel(bp, RX_CLK_SEL0, 0x1);			/*0x1c30*/
		gem_writel(bp, RX_CLK_SEL1, 0x0);			/*0x1c34*/
		gem_writel(bp, TX_CLK_SEL3_0, 0x0);			/*0x1c70*/
		gem_writel(bp, TX_CLK_SEL4_0, 0x0);			/*0x1c74*/
		gem_writel(bp, RX_CLK_SEL3_0, 0x0);			/*0x1c78*/
		gem_writel(bp, RX_CLK_SEL4_0, 0x0);			/*0x1c7c*/
		speed = GEM_SPEED_2500;
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_USXGMII) {
		gem_writel(bp, SRC_SEL_LN, 0x1);			/*0x1c04*/
		if (bp->speed == SPEED_5000) {
			gem_writel(bp, DIV_SEL0_LN, 0x8);		/*0x1c08*/
			gem_writel(bp, DIV_SEL1_LN, 0x2);		/*0x1cc*/
			speed = GEM_SPEED_5000;
		} else {
			gem_writel(bp, DIV_SEL0_LN, 0x4);		/*0x1c08*/
			gem_writel(bp, DIV_SEL1_LN, 0x1);		/*0x1c0c*/
			gem_writel(bp, TX_CLK_SEL3_0, 0x0);		/*0x1c70*/
			gem_writel(bp, RX_CLK_SEL4_0, 0x0);		/*0x1c7c*/
			speed = GEM_SPEED_10000;
		}
		gem_writel(bp, PMA_XCVR_POWER_STATE, 0x1);		/*0x1c10*/
	}

	/*HS_MAC_CONFIG(0x0050) provide rate to the external*/
	gem_writel(bp, HS_MAC_CONFIG, GEM_BFINS(HS_MAC_SPEED, speed, gem_readl(bp, HS_MAC_CONFIG)));
}

void macb_gem2p0_sel_clk(struct macb *bp)
{
	if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_SGMII) {
		if (bp->speed == SPEED_100 || bp->speed == SPEED_10) {
			gem_writel(bp, DIV_SEL0_LN, 0x4); /*0x1c08*/
			gem_writel(bp, DIV_SEL1_LN, 0x8); /*0x1c0c*/
		}
	}

	if (bp->speed == SPEED_100 || bp->speed == SPEED_10)
		gem_writel(bp, HS_MAC_CONFIG, GEM_BFINS(HS_MAC_SPEED, GEM_SPEED_100,
							gem_readl(bp, HS_MAC_CONFIG)));
	else if (bp->speed == SPEED_1000)
		gem_writel(bp, HS_MAC_CONFIG, GEM_BFINS(HS_MAC_SPEED, GEM_SPEED_1000,
							gem_readl(bp, HS_MAC_CONFIG)));
	else if (bp->speed == SPEED_2500)
		gem_writel(bp, HS_MAC_CONFIG, GEM_BFINS(HS_MAC_SPEED, GEM_SPEED_2500,
							gem_readl(bp, HS_MAC_CONFIG)));
	else if (bp->speed == SPEED_5000)
		gem_writel(bp, HS_MAC_CONFIG, GEM_BFINS(HS_MAC_SPEED, GEM_SPEED_5000,
							gem_readl(bp, HS_MAC_CONFIG)));
	else if (bp->speed == SPEED_10000)
		gem_writel(bp, HS_MAC_CONFIG, GEM_BFINS(HS_MAC_SPEED, GEM_SPEED_10000,
							gem_readl(bp, HS_MAC_CONFIG)));
}

/* When PCSSEL is set to 1, PCS will be in a soft reset state,
 * The auto negotiation configuration must be done after
 * pcs soft reset is completed.
 */
static int macb_mac_pcssel_config(struct macb *bp)
{
	u32 old_ctrl, ctrl;

	ctrl = macb_or_gem_readl(bp, NCFGR);
	old_ctrl = ctrl;

	ctrl |= GEM_BIT(PCSSEL);

	if (old_ctrl ^ ctrl)
		macb_or_gem_writel(bp, NCFGR, ctrl);

	rte_delay_ms(1);
	return 0;
}

int macb_mac_with_pcs_config(struct macb *bp)
{
	u32 old_ctrl, ctrl;
	u32 old_ncr, ncr;
	u32 config;
	u32 pcsctrl;

	macb_mac_pcssel_config(bp);

	ncr = macb_readl(bp, NCR);
	old_ncr = ncr;
	ctrl = macb_or_gem_readl(bp, NCFGR);
	old_ctrl = ctrl;

	ncr &= ~(GEM_BIT(ENABLE_HS_MAC) | MACB_BIT(2PT5G));
	ctrl &= ~(GEM_BIT(SGMIIEN) | MACB_BIT(SPD) | MACB_BIT(FD));
	if (macb_is_gem(bp))
		ctrl &= ~GEM_BIT(GBE);

	if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_2500BASEX) {
		ctrl |= GEM_BIT(GBE);
		ncr |= MACB_BIT(2PT5G);
		pcsctrl = gem_readl(bp, PCSCTRL);
		pcsctrl &= ~GEM_BIT(PCS_AUTO_NEG_ENB);
		gem_writel(bp, PCSCTRL, pcsctrl);
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_USXGMII) {
		ncr |= GEM_BIT(ENABLE_HS_MAC);
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_1000BASEX) {
		ctrl |= GEM_BIT(GBE);
		pcsctrl = gem_readl(bp, PCSCTRL);
		pcsctrl |= GEM_BIT(PCS_AUTO_NEG_ENB);
		gem_writel(bp, PCSCTRL, pcsctrl);
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_100BASEX) {
		ctrl |= MACB_BIT(SPD);
		pcsctrl = gem_readl(bp, PCSCTRL);
		pcsctrl |= GEM_BIT(PCS_AUTO_NEG_ENB);
		gem_writel(bp, PCSCTRL, pcsctrl);
	} else if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_SGMII && bp->fixed_link) {
		ctrl |= GEM_BIT(SGMIIEN) | GEM_BIT(GBE);
		pcsctrl = gem_readl(bp, PCSCTRL);
		pcsctrl |= GEM_BIT(PCS_AUTO_NEG_ENB);
		gem_writel(bp, PCSCTRL, pcsctrl);
	}

	if (bp->duplex)
		ctrl |= MACB_BIT(FD);

	/* Apply the new configuration, if any */
	if (old_ctrl ^ ctrl)
		macb_or_gem_writel(bp, NCFGR, ctrl);

	if (old_ncr ^ ncr)
		macb_or_gem_writel(bp, NCR, ncr);

	/*config usx control*/
	if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_USXGMII) {
		config = gem_readl(bp, USX_CONTROL);
		if (bp->speed == SPEED_10000) {
			config = GEM_BFINS(SERDES_RATE, MACB_SERDES_RATE_10G, config);
			config = GEM_BFINS(USX_CTRL_SPEED, GEM_SPEED_10000, config);
		} else if (bp->speed == SPEED_5000) {
			config = GEM_BFINS(SERDES_RATE, MACB_SERDES_RATE_5G, config);
			config = GEM_BFINS(USX_CTRL_SPEED, GEM_SPEED_5000, config);
		}

		config &= ~(GEM_BIT(TX_SCR_BYPASS) | GEM_BIT(RX_SCR_BYPASS));
		/* enable rx and tx */
		config &= ~(GEM_BIT(RX_SYNC_RESET));
		config |= GEM_BIT(SIGNAL_OK) | GEM_BIT(TX_EN);
		gem_writel(bp, USX_CONTROL, config);
	}

	return 0;
}

int macb_link_change(struct macb *bp)
{
	struct phy_device *phydev = bp->phydev;
	uint32_t config, ncr, pcsctrl;
	bool sync_link_info = true;

	if (!bp->link)
		return 0;

	if (bp->phy_interface == MACB_PHY_INTERFACE_MODE_USXGMII ||
		bp->phy_interface == MACB_PHY_INTERFACE_MODE_2500BASEX ||
		bp->phy_interface == MACB_PHY_INTERFACE_MODE_1000BASEX ||
		bp->phy_interface == MACB_PHY_INTERFACE_MODE_100BASEX ||
		bp->fixed_link)
		sync_link_info = false;

	if (sync_link_info) {
		/* sync phy link info to mac */
		if (bp->phydrv_used) {
			bp->duplex = phydev->duplex;
			bp->speed = phydev->speed;
		}

		config = macb_readl(bp, NCFGR);
		config &= ~(MACB_BIT(FD) | MACB_BIT(SPD) | GEM_BIT(GBE));

		if (bp->duplex)
			config |= MACB_BIT(FD);

		if (bp->speed == SPEED_100)
			config |= MACB_BIT(SPD);
		else if (bp->speed == SPEED_1000 || bp->speed == SPEED_2500)
			config |= GEM_BIT(GBE);

		macb_writel(bp, NCFGR, config);

		if (bp->speed == SPEED_2500) {
			ncr = macb_readl(bp, NCR);
			ncr |= MACB_BIT(2PT5G);
			macb_writel(bp, NCR, ncr);
			pcsctrl = gem_readl(bp, PCSCTRL);
			pcsctrl &= ~GEM_BIT(PCS_AUTO_NEG_ENB);
			gem_writel(bp, PCSCTRL, pcsctrl);
		}
	}

	if ((bp->caps & MACB_CAPS_SEL_CLK_HW) && bp->sel_clk_hw)
		bp->sel_clk_hw(bp);

	return 0;
}

void macb_check_for_link(struct macb *bp)
{
	int link_flag;
	struct phy_device *phydev = bp->phydev;

	if (phydev->drv && phydev->drv->check_for_link) {
		link_flag = phydev->drv->check_for_link(phydev);
		if (link_flag > 0)
			bp->link = (uint16_t)link_flag;
		else
			bp->link = 0;
	}
}

void macb_setup_link(struct macb *bp)
{
	struct phy_device *phydev = bp->phydev;

	/* phy setup link */
	if (phydev->drv && phydev->drv->force_speed_duplex)
		phydev->drv->force_speed_duplex(phydev);
}

void macb_reset_hw(struct macb *bp)
{
	u32 i;
	u32 ISR;
	u32 IDR;
	u32 TBQP;
	u32 TBQPH;
	u32 RBQP;
	u32 RBQPH;

	u32 ctrl = macb_readl(bp, NCR);

	/* Disable RX and TX (XXX: Should we halt the transmission
	 * more gracefully?)
	 */
	ctrl &= ~(MACB_BIT(RE) | MACB_BIT(TE));

	/* Clear the stats registers (XXX: Update stats first?) */
	ctrl |= MACB_BIT(CLRSTAT);

	macb_writel(bp, NCR, ctrl);
	rte_delay_ms(1);

	/* Clear all status flags */
	macb_writel(bp, TSR, -1);
	macb_writel(bp, RSR, -1);

	/* queue0 uses legacy registers */
	macb_queue_flush(bp, MACB_TBQP, 1);
	macb_queue_flush(bp, MACB_TBQPH, 0);
	macb_queue_flush(bp, MACB_RBQP, 1);
	macb_queue_flush(bp, MACB_RBQPH, 0);

	/* clear all queue register */
	for (i = 1; i < bp->num_queues; i++) {
		ISR = GEM_ISR(i - 1);
		IDR = GEM_IDR(i - 1);
		TBQP = GEM_TBQP(i - 1);
		TBQPH = GEM_TBQPH(i - 1);
		RBQP = GEM_RBQP(i - 1);
		RBQPH = GEM_RBQPH(i - 1);

		macb_queue_flush(bp, IDR, -1);
		if (bp->caps & MACB_CAPS_ISR_CLEAR_ON_WRITE)
			macb_queue_flush(bp, ISR, -1);
		macb_queue_flush(bp, TBQP, 1);
		macb_queue_flush(bp, TBQPH, 0);
		macb_queue_flush(bp, RBQP, 1);
		macb_queue_flush(bp, RBQPH, 0);
	}
}
