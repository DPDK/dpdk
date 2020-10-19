/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
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
#define TXGBE_DEV_ID_RAPTOR_VF                  0x1000
#define TXGBE_DEV_ID_RAPTOR_SFP                 0x1001 /* fiber */
#define TXGBE_DEV_ID_RAPTOR_KR_KX_KX4           0x1002 /* backplane */
#define TXGBE_DEV_ID_RAPTOR_XAUI                0x1003 /* copper */
#define TXGBE_DEV_ID_RAPTOR_SGMII               0x1004 /* copper */
#define TXGBE_DEV_ID_RAPTOR_QSFP                0x1011 /* fiber */
#define TXGBE_DEV_ID_RAPTOR_VF_HV               0x2000
#define TXGBE_DEV_ID_RAPTOR_T3_LOM              0x2001

#define TXGBE_DEV_ID_WX1820_SFP                 0x2001

/*
 * Subdevice IDs
 */
#define TXGBE_SUBDEV_ID_RAPTOR			0x0000
#define TXGBE_SUBDEV_ID_MPW			0x0001

#define TXGBE_ETHERTYPE_FLOW_CTRL   0x8808
#define TXGBE_ETHERTYPE_IEEE_VLAN   0x8100  /* 802.1q protocol */

#define TXGBE_VXLAN_PORT 4789

#endif /* _TXGBE_DEVIDS_H_ */
