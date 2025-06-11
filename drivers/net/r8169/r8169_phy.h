/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef R8169_PHY_H
#define R8169_PHY_H

#include <stdint.h>
#include <stdbool.h>

#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

#include "r8169_compat.h"
#include "r8169_ethdev.h"

/* Generic MII registers. */
#define MII_BMCR                0x00	/* Basic mode control register */
#define MII_BMSR                0x01	/* Basic mode status register  */
#define MII_PHYSID1             0x02	/* PHYS ID 1                   */
#define MII_PHYSID2             0x03	/* PHYS ID 2                   */
#define MII_ADVERTISE           0x04	/* Advertisement control reg   */
#define MII_LPA                 0x05	/* Link partner ability reg    */
#define MII_EXPANSION           0x06	/* Expansion register          */
#define MII_CTRL1000            0x09	/* 1000BASE-T control          */
#define MII_STAT1000            0x0a	/* 1000BASE-T status           */
#define MII_MMD_CTRL            0x0d	/* MMD Access Control Register */
#define MII_MMD_DATA            0x0e	/* MMD Access Data Register    */
#define MII_ESTATUS             0x0f	/* Extended Status             */
#define MII_DCOUNTER            0x12	/* Disconnect counter          */
#define MII_FCSCOUNTER          0x13	/* False carrier counter       */
#define MII_NWAYTEST            0x14	/* N-way auto-neg test reg     */
#define MII_RERRCOUNTER         0x15	/* Receive error counter       */
#define MII_SREVISION           0x16	/* Silicon revision            */
#define MII_RESV1               0x17	/* Reserved...                 */
#define MII_LBRERROR            0x18	/* Lpback, rx, bypass error    */
#define MII_PHYADDR             0x19	/* PHY address                 */
#define MII_RESV2               0x1a	/* Reserved...                 */
#define MII_TPISTATUS           0x1b	/* TPI status for 10mbps       */
#define MII_NCONFIG             0x1c	/* Network interface config    */

/* Basic mode control register. */
#define BMCR_RESV               0x003f	/* Unused...                   */
#define BMCR_SPEED1000          0x0040	/* MSB of Speed (1000)         */
#define BMCR_CTST               0x0080	/* Collision test              */
#define BMCR_FULLDPLX           0x0100	/* Full duplex                 */
#define BMCR_ANRESTART          0x0200	/* Auto negotiation restart    */
#define BMCR_ISOLATE            0x0400	/* Isolate data paths from MII */
#define BMCR_PDOWN              0x0800	/* Enable low power state      */
#define BMCR_ANENABLE           0x1000	/* Enable auto negotiation     */
#define BMCR_SPEED100           0x2000	/* Select 100Mbps              */
#define BMCR_LOOPBACK           0x4000	/* TXD loopback bits           */
#define BMCR_RESET              0x8000	/* Reset to default state      */
#define BMCR_SPEED10            0x0000	/* Select 10Mbps               */

/* Basic mode status register. */
#define BMSR_ERCAP              0x0001	/* Ext-reg capability          */
#define BMSR_JCD                0x0002	/* Jabber detected             */
#define BMSR_LSTATUS            0x0004	/* Link status                 */
#define BMSR_ANEGCAPABLE        0x0008	/* Able to do auto-negotiation */
#define BMSR_RFAULT             0x0010	/* Remote fault detected       */
#define BMSR_ANEGCOMPLETE       0x0020	/* Auto-negotiation complete   */
#define BMSR_RESV               0x00c0	/* Unused...                   */
#define BMSR_ESTATEN            0x0100	/* Extended Status in R15      */
#define BMSR_100HALF2           0x0200	/* Can do 100BASE-T2 HDX       */
#define BMSR_100FULL2           0x0400	/* Can do 100BASE-T2 FDX       */
#define BMSR_10HALF             0x0800	/* Can do 10mbps, half-duplex  */
#define BMSR_10FULL             0x1000	/* Can do 10mbps, full-duplex  */
#define BMSR_100HALF            0x2000	/* Can do 100mbps, half-duplex */
#define BMSR_100FULL            0x4000	/* Can do 100mbps, full-duplex */
#define BMSR_100BASE4           0x8000	/* Can do 100mbps, 4k packets  */

/* Advertisement control register. */
#define ADVERTISE_SLCT          0x001f	/* Selector bits               */
#define ADVERTISE_CSMA          0x0001	/* Only selector supported     */
#define ADVERTISE_10HALF        0x0020	/* Try for 10mbps half-duplex  */
#define ADVERTISE_1000XFULL     0x0020	/* Try for 1000BASE-X full-duplex */
#define ADVERTISE_10FULL        0x0040	/* Try for 10mbps full-duplex  */
#define ADVERTISE_1000XHALF     0x0040	/* Try for 1000BASE-X half-duplex */
#define ADVERTISE_100HALF       0x0080	/* Try for 100mbps half-duplex */
#define ADVERTISE_1000XPAUSE    0x0080	/* Try for 1000BASE-X pause    */
#define ADVERTISE_100FULL       0x0100	/* Try for 100mbps full-duplex */
#define ADVERTISE_1000XPSE_ASYM 0x0100	/* Try for 1000BASE-X asym pause */
#define ADVERTISE_100BASE4      0x0200	/* Try for 100mbps 4k packets  */
#define ADVERTISE_PAUSE_CAP     0x0400	/* Try for pause               */
#define ADVERTISE_PAUSE_ASYM    0x0800	/* Try for asymmetric pause    */
#define ADVERTISE_RESV          0x1000	/* Unused...                   */
#define ADVERTISE_RFAULT        0x2000	/* Say we can detect faults    */
#define ADVERTISE_LPACK         0x4000	/* Ack link partners response  */
#define ADVERTISE_NPAGE         0x8000	/* Next page bit               */

/* 1000BASE-T Control register */
#define ADVERTISE_1000FULL      0x0200  /* Advertise 1000BASE-T full duplex */
#define ADVERTISE_1000HALF      0x0100  /* Advertise 1000BASE-T half duplex */

#define RTK_ADVERTISE_2500FULL          0x80
#define RTK_ADVERTISE_5000FULL          0x100
#define RTK_ADVERTISE_10000FULL         0x1000
#define RTK_LPA_ADVERTISE_2500FULL      0x20
#define RTK_LPA_ADVERTISE_5000FULL      0x40
#define RTK_LPA_ADVERTISE_10000FULL     0x800

#define HW_SUPPORT_CHECK_PHY_DISABLE_MODE(_M) ((_M)->HwSuppCheckPhyDisableModeVer > 0)

#define HW_SUPP_PHY_LINK_SPEED_10000M(_M)     ((_M)->HwSuppMaxPhyLinkSpeed >= 10000)
#define HW_SUPP_PHY_LINK_SPEED_5000M(_M)      ((_M)->HwSuppMaxPhyLinkSpeed >= 5000)

#define MDIO_EEE_100TX  0x0002
#define MDIO_EEE_1000T  0x0004
#define MDIO_EEE_2_5GT  0x0001
#define MDIO_EEE_5GT    0x0002
#define MDIO_EEE_10GT   0x0008  /* 10GT EEE cap */

#define HW_SUPP_SERDES_PHY(_M)  ((_M)->HwSuppSerDesPhyVer > 0)

u32 rtl_mdio_direct_read_phy_ocp(struct rtl_hw *hw, u32 RegAddr);
void rtl_mdio_direct_write_phy_ocp(struct rtl_hw *hw, u32 RegAddr, u32 value);

u32 rtl_mdio_read(struct rtl_hw *hw, u32 RegAddr);
void rtl_mdio_write(struct rtl_hw *hw, u32 RegAddr, u32 value);

void rtl_clear_and_set_eth_phy_ocp_bit(struct rtl_hw *hw, u16 addr,
				       u16 clearmask, u16 setmask);
void rtl_clear_eth_phy_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);
void rtl_set_eth_phy_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);

void rtl_ephy_write(struct rtl_hw *hw, int addr, int value);
u16 rtl_ephy_read(struct rtl_hw *hw, int addr);

void rtl_clear_and_set_pcie_phy_bit(struct rtl_hw *hw, u8 addr, u16 clearmask,
				    u16 setmask);
void rtl_clear_pcie_phy_bit(struct rtl_hw *hw, u8 addr, u16 mask);
void rtl_set_pcie_phy_bit(struct rtl_hw *hw, u8 addr, u16 mask);

bool rtl_set_phy_mcu_patch_request(struct rtl_hw *hw);
bool rtl_clear_phy_mcu_patch_request(struct rtl_hw *hw);

void rtl_set_phy_mcu_ram_code(struct rtl_hw *hw, const u16 *ramcode,
			      u16 codesize);

void rtl_powerup_pll(struct rtl_hw *hw);
void rtl_powerdown_pll(struct rtl_hw *hw);

void rtl_hw_ephy_config(struct rtl_hw *hw);
void rtl_hw_phy_config(struct rtl_hw *hw);

int rtl_set_speed(struct rtl_hw *hw);

u32 rtl_mdio_real_read_phy_ocp(struct rtl_hw *hw, u16 PageNum, u32 RegAddr);
void rtl_mdio_real_write_phy_ocp(struct rtl_hw *hw, u16 PageNum, u32 RegAddr,
				 u32 value);
void rtl_clear_and_set_eth_phy_bit(struct rtl_hw *hw, u8 addr, u16 clearmask,
				   u16 setmask);
void rtl_clear_eth_phy_bit(struct rtl_hw *hw, u8 addr, u16 mask);
void rtl_set_eth_phy_bit(struct rtl_hw *hw, u8 addr, u16 mask);

void rtl8127_clear_ephy_ext_addr(struct rtl_hw *hw);

#endif /* R8169_PHY_H */
