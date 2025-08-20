/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */

#ifndef _GENERIC_PHY_H
#define _GENERIC_PHY_H

#include "macb_common.h"

/* Or MII_ADDR_C45 into regnum for read/write on mii_bus to enable the 21 bit
 * IEEE 802.3ae clause 45 addressing mode used by 10GIGE phy chips.
 */
#define MII_ADDR_C45		(1 << 30)
#define MII_DEVADDR_C45_SHIFT	16
#define MII_REGADDR_C45_MASK	0xffff

/* Generic MII registers. */
#define GENERIC_PHY_BMCR            0x0
#define GENERIC_PHY_BMSR            0x1
#define GENERIC_PHY_PHYSID1         0x2
#define GENERIC_PHY_PHYSID2         0x3
#define GENERIC_PHY_ADVERISE        0x4
#define GENERIC_PHY_LPA				0x5
#define GENERIC_PHY_CTRL1000        0x9
#define GENERIC_PHY_STAT1000		0xa

/* Basic mode control register. */
#define BMCR_RESV		0x003f	/* Unused...                   */
#define BMCR_SPEED1000		0x0040	/* MSB of Speed (1000)         */
#define BMCR_CTST		0x0080	/* Collision test              */
#define BMCR_FULLDPLX		0x0100	/* Full duplex                 */
#define BMCR_ANRESTART		0x0200	/* Auto negotiation restart    */
#define BMCR_ISOLATE		0x0400	/* Isolate data paths from MII */
#define BMCR_PDOWN		0x0800	/* Enable low power state      */
#define BMCR_ANENABLE		0x1000	/* Enable auto negotiation     */
#define BMCR_SPEED100		0x2000	/* Select 100Mbps              */
#define BMCR_LOOPBACK		0x4000	/* TXD loopback bits           */
#define BMCR_RESET		0x8000	/* Reset to default state      */
#define BMCR_SPEED10		0x0000	/* Select 10Mbps               */

/* Basic mode status register. */
#define BMSR_ERCAP		0x0001	/* Ext-reg capability          */
#define BMSR_JCD		0x0002	/* Jabber detected             */
#define BMSR_LSTATUS		0x0004	/* Link status                 */
#define BMSR_ANEGCAPABLE	0x0008	/* Able to do auto-negotiation */
#define BMSR_RFAULT		0x0010	/* Remote fault detected       */
#define BMSR_ANEGCOMPLETE	0x0020	/* Auto-negotiation complete   */
#define BMSR_RESV		0x00c0	/* Unused...                   */
#define BMSR_ESTATEN		0x0100	/* Extended Status in R15      */
#define BMSR_100HALF2		0x0200	/* Can do 100BASE-T2 HDX       */
#define BMSR_100FULL2		0x0400	/* Can do 100BASE-T2 FDX       */
#define BMSR_10HALF		0x0800	/* Can do 10mbps, half-duplex  */
#define BMSR_10FULL		0x1000	/* Can do 10mbps, full-duplex  */
#define BMSR_100HALF		0x2000	/* Can do 100mbps, half-duplex */
#define BMSR_100FULL		0x4000	/* Can do 100mbps, full-duplex */
#define BMSR_100BASE4		0x8000	/* Can do 100mbps, 4k packets  */

/* Advertisement control register. */
#define ADVERTISE_SLCT		0x001f	/* Selector bits               */
#define ADVERTISE_CSMA		0x0001	/* Only selector supported     */
#define ADVERTISE_10HALF	0x0020	/* Try for 10mbps half-duplex  */
#define ADVERTISE_1000XFULL	0x0020	/* Try for 1000BASE-X full-duplex */
#define ADVERTISE_10FULL	0x0040	/* Try for 10mbps full-duplex  */
#define ADVERTISE_1000XHALF	0x0040	/* Try for 1000BASE-X half-duplex */
#define ADVERTISE_100HALF	0x0080	/* Try for 100mbps half-duplex */
#define ADVERTISE_1000XPAUSE	0x0080	/* Try for 1000BASE-X pause    */
#define ADVERTISE_100FULL	0x0100	/* Try for 100mbps full-duplex */
#define ADVERTISE_1000XPSE_ASYM	0x0100	/* Try for 1000BASE-X asym pause */
#define ADVERTISE_100BASE4	0x0200	/* Try for 100mbps 4k packets  */
#define ADVERTISE_PAUSE_CAP	0x0400	/* Try for pause               */
#define ADVERTISE_PAUSE_ASYM	0x0800	/* Try for asymmetric pause     */
#define ADVERTISE_RESV		0x1000	/* Unused...                   */
#define ADVERTISE_RFAULT	0x2000	/* Say we can detect faults    */
#define ADVERTISE_LPACK		0x4000	/* Ack link partners response  */
#define ADVERTISE_NPAGE		0x8000	/* Next page bit               */

/* Link partner ability register. */
#define LPA_SLCT		0x001f	/* Same as advertise selector  */
#define LPA_10HALF		0x0020	/* Can do 10mbps half-duplex   */
#define LPA_1000XFULL		0x0020	/* Can do 1000BASE-X full-duplex */
#define LPA_10FULL		0x0040	/* Can do 10mbps full-duplex   */
#define LPA_1000XHALF		0x0040	/* Can do 1000BASE-X half-duplex */
#define LPA_100HALF		0x0080	/* Can do 100mbps half-duplex  */
#define LPA_1000XPAUSE		0x0080	/* Can do 1000BASE-X pause     */
#define LPA_100FULL		0x0100	/* Can do 100mbps full-duplex  */
#define LPA_1000XPAUSE_ASYM	0x0100	/* Can do 1000BASE-X pause asym*/
#define LPA_100BASE4		0x0200	/* Can do 100mbps 4k packets   */
#define LPA_PAUSE_CAP		0x0400	/* Can pause                   */
#define LPA_PAUSE_ASYM		0x0800	/* Can pause asymmetrically     */
#define LPA_RESV		0x1000	/* Unused...                   */
#define LPA_RFAULT		0x2000	/* Link partner faulted        */
#define LPA_LPACK		0x4000	/* Link partner acked us       */
#define LPA_NPAGE		0x8000	/* Next page bit               */

/* 1000BASE-T Control register */
#define ADVERTISE_1000FULL	0x0200  /* Advertise 1000BASE-T full duplex */
#define ADVERTISE_1000HALF	0x0100  /* Advertise 1000BASE-T half duplex */
#define CTL1000_AS_MASTER	0x0800
#define CTL1000_ENABLE_MASTER	0x1000

/* 1000BASE-T Status register */
#define LPA_1000MSFAIL		0x8000	/* Primary/Secondary resolution failure */
#define LPA_1000LOCALRXOK	0x2000	/* Link partner local receiver status */
#define LPA_1000REMRXOK		0x1000	/* Link partner remote receiver status */
#define LPA_1000FULL		0x0800	/* Link partner 1000BASE-T full duplex */
#define LPA_1000HALF		0x0400	/* Link partner 1000BASE-T half duplex */

struct phy_device {
	struct macb *bp;
	struct phy_driver *drv;
	uint32_t phy_id;
	uint16_t phyad;
	uint32_t speed;
	uint16_t link;
	uint16_t duplex;
	uint16_t autoneg;
	void *priv;
};

struct phy_driver {
	const char *name;
	uint32_t phy_id;
	uint32_t phy_id_mask;

	int (*config_init)(struct phy_device *phydev);
	int (*soft_reset)(struct phy_device *phydev);
	int (*probe)(struct phy_device *phydev);
	int (*resume)(struct phy_device *phydev);
	void (*suspend)(struct phy_device *phydev);
	int (*check_for_link)(struct phy_device *phydev);
	int (*read_status)(struct phy_device *phydev);
	int (*force_speed_duplex)(struct phy_device *phydev);
};

static inline uint32_t genphy_adv_to_ethtool_adv_t(uint32_t adv)
{
	uint32_t result = 0;

	if (adv & ADVERTISE_10HALF)
		result |= ADVERTISED_10baseT_Half;
	if (adv & ADVERTISE_10FULL)
		result |= ADVERTISED_10baseT_Full;
	if (adv & ADVERTISE_100HALF)
		result |= ADVERTISED_100baseT_Half;
	if (adv & ADVERTISE_100FULL)
		result |= ADVERTISED_100baseT_Full;
	if (adv & ADVERTISE_PAUSE_CAP)
		result |= ADVERTISED_Pause;
	if (adv & ADVERTISE_PAUSE_ASYM)
		result |= ADVERTISED_Asym_Pause;

	return result;
}

static inline uint32_t genphy_ctrl1000_to_ethtool_adv_t(uint32_t adv)
{
	uint32_t result = 0;

	if (adv & ADVERTISE_1000HALF)
		result |= ADVERTISED_1000baseT_Half;
	if (adv & ADVERTISE_1000FULL)
		result |= ADVERTISED_1000baseT_Full;

	return result;
}

static inline uint32_t genphy_lpa_to_ethtool_lpa_t(uint32_t lpa)
{
	uint32_t result = 0;

	if (lpa & LPA_LPACK)
		result |= ADVERTISED_Autoneg;

	return result | genphy_adv_to_ethtool_adv_t(lpa);
}

static inline uint32_t genphy_stat1000_to_ethtool_lpa_t(uint32_t lpa)
{
	uint32_t result = 0;

	if (lpa & LPA_1000HALF)
		result |= ADVERTISED_1000baseT_Half;
	if (lpa & LPA_1000FULL)
		result |= ADVERTISED_1000baseT_Full;

	return result;
}

/* for usxgmii interface */
int macb_usxgmii_pcs_resume(struct phy_device *phydev);
void macb_usxgmii_pcs_suspend(struct phy_device *phydev);
int macb_usxgmii_pcs_check_for_link(struct phy_device *phydev);
int macb_gbe_pcs_check_for_link(struct phy_device *phydev);

#endif /* _GENERIC_PHY_H */
