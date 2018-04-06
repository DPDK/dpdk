/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#ifndef __AXGBE_PHY_H__
#define __AXGBE_PHY_H__

#define SPEED_10    10
#define SPEED_100   100
#define SPEED_1000  1000
#define SPEED_2500  2500
#define SPEED_10000 10000


/* Or MII_ADDR_C45 into regnum for read/write on mii_bus to enable the 21 bit
 * IEEE 802.3ae clause 45 addressing mode used by 10GIGE phy chips.
 */
#define MII_ADDR_C45 (1 << 30)

/* Basic mode status register. */
#define BMSR_LSTATUS            0x0004  /* Link status */

/* Status register 1. */
#define MDIO_STAT1_LSTATUS              BMSR_LSTATUS

/* Generic MII registers. */
#define MII_BMCR		0x00	/* Basic mode control register */
#define MII_BMSR		0x01	/* Basic mode status register  */
#define MII_PHYSID1		0x02	/* PHYS ID 1                   */
#define MII_PHYSID2		0x03	/* PHYS ID 2                   */
#define MII_ADVERTISE		0x04	/* Advertisement control reg   */
#define MII_LPA			0x05	/* Link partner ability reg    */
#define MII_EXPANSION		0x06	/* Expansion register          */
#define MII_CTRL1000		0x09	/* 1000BASE-T control          */
#define MII_STAT1000		0x0a	/* 1000BASE-T status           */
#define	MII_MMD_CTRL		0x0d	/* MMD Access Control Register */
#define	MII_MMD_DATA		0x0e	/* MMD Access Data Register */
#define MII_ESTATUS		0x0f	/* Extended Status             */
#define MII_DCOUNTER		0x12	/* Disconnect counter          */
#define MII_FCSCOUNTER		0x13	/* False carrier counter       */
#define MII_NWAYTEST		0x14	/* N-way auto-neg test reg     */
#define MII_RERRCOUNTER		0x15	/* Receive error counter       */
#define MII_SREVISION		0x16	/* Silicon revision            */
#define MII_RESV1		0x17	/* Reserved...                 */
#define MII_LBRERROR		0x18	/* Lpback, rx, bypass error    */
#define MII_PHYADDR		0x19	/* PHY address                 */
#define MII_RESV2		0x1a	/* Reserved...                 */
#define MII_TPISTATUS		0x1b	/* TPI status for 10mbps       */
#define MII_NCONFIG		0x1c	/* Network interface config    */

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


/* MDIO Manageable Devices (MMDs). */
#define MDIO_MMD_PMAPMD		1	/* Physical Medium Attachment
					 * Physical Medium Dependent
					 */
#define MDIO_MMD_WIS		2	/* WAN Interface Sublayer */
#define MDIO_MMD_PCS		3	/* Physical Coding Sublayer */
#define MDIO_MMD_PHYXS		4	/* PHY Extender Sublayer */
#define MDIO_MMD_DTEXS		5	/* DTE Extender Sublayer */
#define MDIO_MMD_TC		6	/* Transmission Convergence */
#define MDIO_MMD_AN		7	/* Auto-Negotiation */
#define MDIO_MMD_C22EXT		29	/* Clause 22 extension */
#define MDIO_MMD_VEND1		30	/* Vendor specific 1 */
#define MDIO_MMD_VEND2		31	/* Vendor specific 2 */

/* Generic MDIO registers. */
#define MDIO_CTRL1		MII_BMCR
#define MDIO_STAT1		MII_BMSR
#define MDIO_DEVID1		MII_PHYSID1
#define MDIO_DEVID2		MII_PHYSID2
#define MDIO_SPEED		4	/* Speed ability */
#define MDIO_DEVS1		5	/* Devices in package */
#define MDIO_DEVS2		6
#define MDIO_CTRL2		7	/* 10G control 2 */
#define MDIO_STAT2		8	/* 10G status 2 */
#define MDIO_PMA_TXDIS		9	/* 10G PMA/PMD transmit disable */
#define MDIO_PMA_RXDET		10	/* 10G PMA/PMD receive signal detect */
#define MDIO_PMA_EXTABLE	11	/* 10G PMA/PMD extended ability */
#define MDIO_PKGID1		14	/* Package identifier */
#define MDIO_PKGID2		15
#define MDIO_AN_ADVERTISE	16	/* AN advertising (base page) */
#define MDIO_AN_LPA		19	/* AN LP abilities (base page) */
#define MDIO_PCS_EEE_ABLE	20	/* EEE Capability register */
#define MDIO_PCS_EEE_WK_ERR	22	/* EEE wake error counter */
#define MDIO_PHYXS_LNSTAT	24	/* PHY XGXS lane state */
#define MDIO_AN_EEE_ADV		60	/* EEE advertisement */
#define MDIO_AN_EEE_LPABLE	61	/* EEE link partner ability */

/* Media-dependent registers. */
#define MDIO_PMA_10GBT_SWAPPOL	130	/* 10GBASE-T pair swap & polarity */
#define MDIO_PMA_10GBT_TXPWR	131	/* 10GBASE-T TX power control */
#define MDIO_PMA_10GBT_SNR	133	/* 10GBASE-T SNR margin, lane A.
					 * Lanes B-D are numbered 134-136.
					 */
#define MDIO_PMA_10GBR_FECABLE	170	/* 10GBASE-R FEC ability */
#define MDIO_PCS_10GBX_STAT1	24	/* 10GBASE-X PCS status 1 */
#define MDIO_PCS_10GBRT_STAT1	32	/* 10GBASE-R/-T PCS status 1 */
#define MDIO_PCS_10GBRT_STAT2	33	/* 10GBASE-R/-T PCS status 2 */
#define MDIO_AN_10GBT_CTRL	32	/* 10GBASE-T auto-negotiation control */
#define MDIO_AN_10GBT_STAT	33	/* 10GBASE-T auto-negotiation status */

/* Control register 1. */
/* Enable extended speed selection */
#define MDIO_CTRL1_SPEEDSELEXT		(BMCR_SPEED1000 | BMCR_SPEED100)
/* All speed selection bits */
#define MDIO_CTRL1_SPEEDSEL		(MDIO_CTRL1_SPEEDSELEXT | 0x003c)
#define MDIO_CTRL1_FULLDPLX		BMCR_FULLDPLX
#define MDIO_CTRL1_LPOWER		BMCR_PDOWN
#define MDIO_CTRL1_RESET		BMCR_RESET
#define MDIO_PMA_CTRL1_LOOPBACK		0x0001
#define MDIO_PMA_CTRL1_SPEED1000	BMCR_SPEED1000
#define MDIO_PMA_CTRL1_SPEED100		BMCR_SPEED100
#define MDIO_PCS_CTRL1_LOOPBACK		BMCR_LOOPBACK
#define MDIO_PHYXS_CTRL1_LOOPBACK	BMCR_LOOPBACK
#define MDIO_AN_CTRL1_RESTART		BMCR_ANRESTART
#define MDIO_AN_CTRL1_ENABLE		BMCR_ANENABLE
#define MDIO_AN_CTRL1_XNP		0x2000	/* Enable extended next page */
#define MDIO_PCS_CTRL1_CLKSTOP_EN	0x400	/* Stop the clock during LPI */





/* PMA 10GBASE-R FEC ability register. */
#define MDIO_PMA_10GBR_FECABLE_ABLE     0x0001  /* FEC ability */
#define MDIO_PMA_10GBR_FECABLE_ERRABLE  0x0002  /* FEC error indic. ability */


/* Autoneg related */
#define ADVERTISED_Autoneg		(1 << 6)
#define SUPPORTED_Autoneg		(1 << 6)
#define AUTONEG_DISABLE			0x00
#define AUTONEG_ENABLE			0x01

#define ADVERTISED_Pause		(1 << 13)
#define ADVERTISED_Asym_Pause		(1 << 14)

#define SUPPORTED_Pause			(1 << 13)
#define SUPPORTED_Asym_Pause		(1 << 14)

#define SUPPORTED_Backplane		(1 << 16)
#define SUPPORTED_TP                    (1 << 7)

#define ADVERTISED_10000baseR_FEC       (1 << 20)

#define SUPPORTED_10000baseR_FEC	(1 << 20)

#define SUPPORTED_FIBRE                 (1 << 10)

#define ADVERTISED_10000baseKR_Full	(1 << 19)
#define ADVERTISED_10000baseT_Full	(1 << 12)
#define ADVERTISED_2500baseX_Full	(1 << 15)
#define ADVERTISED_1000baseKX_Full	(1 << 17)
#define ADVERTISED_1000baseT_Full	(1 << 5)
#define ADVERTISED_100baseT_Full	(1 << 3)
#define ADVERTISED_TP			(1 << 7)
#define ADVERTISED_FIBRE		(1 << 10)
#define ADVERTISED_Backplane            (1 << 16)

#define SUPPORTED_1000baseKX_Full       (1 << 17)
#define SUPPORTED_10000baseKR_Full      (1 << 19)
#define SUPPORTED_2500baseX_Full	(1 << 15)
#define SUPPORTED_100baseT_Full         (1 << 2)
#define SUPPORTED_1000baseT_Full        (1 << 5)
#define SUPPORTED_10000baseT_Full       (1 << 12)
#define SUPPORTED_2500baseX_Full        (1 << 15)


#define SPEED_UNKNOWN           -1

/* Duplex, half or full. */
#define DUPLEX_HALF             0x00
#define DUPLEX_FULL             0x01
#define DUPLEX_UNKNOWN          0xff

#endif
/* PHY */
