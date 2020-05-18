/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _I40E_DEVIDS_H_
#define _I40E_DEVIDS_H_

/* Vendor ID */
#define I40E_INTEL_VENDOR_ID		0x8086

/* Device IDs */
#define I40E_DEV_ID_SFP_XL710		0x1572
#define I40E_DEV_ID_QEMU		0x1574
#define I40E_DEV_ID_KX_B		0x1580
#define I40E_DEV_ID_KX_C		0x1581
#define I40E_DEV_ID_QSFP_A		0x1583
#define I40E_DEV_ID_QSFP_B		0x1584
#define I40E_DEV_ID_QSFP_C		0x1585
#define I40E_DEV_ID_10G_BASE_T		0x1586
#define I40E_DEV_ID_20G_KR2		0x1587
#define I40E_DEV_ID_20G_KR2_A		0x1588
#define I40E_DEV_ID_10G_BASE_T4		0x1589
#define I40E_DEV_ID_25G_B		0x158A
#define I40E_DEV_ID_25G_SFP28		0x158B
#define I40E_DEV_ID_X710_N3000      0x0CF8
#define I40E_DEV_ID_XXV710_N3000	0x0D58
#define I40E_DEV_ID_10G_BASE_T_BC	0x15FF
#define I40E_DEV_ID_5G_BASE_T_BC	0x101F
#if defined(INTEGRATED_VF) || defined(VF_DRIVER) || defined(I40E_NDIS_SUPPORT)
#define I40E_DEV_ID_VF			0x154C
#define I40E_DEV_ID_VF_HV		0x1571
#define I40E_DEV_ID_ADAPTIVE_VF		0x1889
#endif /* VF_DRIVER */
#ifdef X722_A0_SUPPORT
#define I40E_DEV_ID_X722_A0		0x374C
#if defined(INTEGRATED_VF) || defined(VF_DRIVER)
#define I40E_DEV_ID_X722_A0_VF		0x374D
#endif
#endif
#define I40E_DEV_ID_10G_B		0x104F
#define I40E_DEV_ID_10G_SFP		0x104E
#define I40E_IS_X710TL_DEVICE(d) \
	(((d) == I40E_DEV_ID_10G_BASE_T_BC) || \
	((d) == I40E_DEV_ID_5G_BASE_T_BC))
#define I40E_DEV_ID_KX_X722		0x37CE
#define I40E_DEV_ID_QSFP_X722		0x37CF
#define I40E_DEV_ID_SFP_X722		0x37D0
#define I40E_DEV_ID_1G_BASE_T_X722	0x37D1
#define I40E_DEV_ID_10G_BASE_T_X722	0x37D2
#define I40E_DEV_ID_SFP_I_X722		0x37D3
#if defined(INTEGRATED_VF) || defined(VF_DRIVER) || defined(I40E_NDIS_SUPPORT)
#define I40E_DEV_ID_X722_VF		0x37CD
#endif /* VF_DRIVER */

#define i40e_is_40G_device(d)		((d) == I40E_DEV_ID_QSFP_A  || \
					 (d) == I40E_DEV_ID_QSFP_B  || \
					 (d) == I40E_DEV_ID_QSFP_C)

#define i40e_is_25G_device(d)		((d) == I40E_DEV_ID_25G_B  || \
					 (d) == I40E_DEV_ID_25G_SFP28)

#endif /* _I40E_DEVIDS_H_ */
