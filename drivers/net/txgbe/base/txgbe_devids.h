/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 */

#ifndef _TXGBE_DEVIDS_H_
#define _TXGBE_DEVIDS_H_

/*
 * Vendor ID
 */
#ifndef PCI_VENDOR_ID_WANGXUN
#define PCI_VENDOR_ID_WANGXUN                   0x8088
#endif

/*
 * Device IDs
 */
#define TXGBE_DEV_ID_SP1000			0x1001
#define TXGBE_DEV_ID_WX1820			0x2001
#define TXGBE_DEV_ID_SP1000_VF                  0x1000
#define TXGBE_DEV_ID_WX1820_VF                  0x2000
#define TXGBE_DEV_ID_AML			0x5000
#define TXGBE_DEV_ID_AML5025			0x5025
#define TXGBE_DEV_ID_AML5125			0x5125
#define TXGBE_DEV_ID_AML5040			0x5040
#define TXGBE_DEV_ID_AML5140			0x5140

#define TXGBE_DEV_ID_AML_VF			0x5001
#define TXGBE_DEV_ID_AML5024_VF			0x5024
#define TXGBE_DEV_ID_AML5124_VF			0x5124

/*
 * Subsystem IDs
 */
/* SFP */
#define TXGBE_DEV_ID_SP1000_SFP			0x0000
#define TXGBE_DEV_ID_WX1820_SFP			0x2000
#define TXGBE_DEV_ID_SFP			0x00
/* copper */
#define TXGBE_DEV_ID_SP1000_XAUI		0x1010
#define TXGBE_DEV_ID_WX1820_XAUI		0x2010
#define TXGBE_DEV_ID_XAUI			0x10
#define TXGBE_DEV_ID_SP1000_SGMII		0x1020
#define TXGBE_DEV_ID_WX1820_SGMII		0x2020
#define TXGBE_DEV_ID_SGMII			0x20
/* backplane */
#define TXGBE_DEV_ID_SP1000_KR_KX_KX4		0x1030
#define TXGBE_DEV_ID_WX1820_KR_KX_KX4		0x2030
#define TXGBE_DEV_ID_KR_KX_KX4			0x30
/* MAC Interface */
#define TXGBE_DEV_ID_SP1000_MAC_XAUI		0x1040
#define TXGBE_DEV_ID_WX1820_MAC_XAUI		0x2040
#define TXGBE_DEV_ID_MAC_XAUI			0x40
#define TXGBE_DEV_ID_SP1000_MAC_SGMII           0x1060
#define TXGBE_DEV_ID_WX1820_MAC_SGMII           0x2060
#define TXGBE_DEV_ID_MAC_SGMII                  0x60
/* combined interface*/
#define TXGBE_DEV_ID_SFI_XAUI			0x50
/* fiber qsfp*/
#define TXGBE_DEV_ID_QSFP			0x11

#define TXGBE_ETHERTYPE_FLOW_CTRL   0x8808
#define TXGBE_ETHERTYPE_IEEE_VLAN   0x8100  /* 802.1q protocol */

#define TXGBE_VXLAN_PORT 4789

#endif /* _TXGBE_DEVIDS_H_ */
