/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#ifndef XSC_DEFS_H_
#define XSC_DEFS_H_

#define XSC_PCI_VENDOR_ID		0x1f67
#define XSC_PCI_DEV_ID_MS		0x1111
#define XSC_PCI_DEV_ID_MSVF		0x1112
#define XSC_PCI_DEV_ID_MVH		0x1151
#define XSC_PCI_DEV_ID_MVHVF		0x1152
#define XSC_PCI_DEV_ID_MVS		0x1153

#define XSC_VFREP_BASE_LOGICAL_PORT	1081

enum xsc_nic_mode {
	XSC_NIC_MODE_LEGACY,
	XSC_NIC_MODE_SWITCHDEV,
	XSC_NIC_MODE_SOC,
};

enum xsc_pph_type {
	XSC_PPH_NONE	= 0,
	XSC_RX_PPH	= 0x1,
	XSC_TX_PPH	= 0x2,
	XSC_VFREP_PPH	= 0x4,
	XSC_UPLINK_PPH	= 0x8,
};

#endif /* XSC_DEFS_H_ */
