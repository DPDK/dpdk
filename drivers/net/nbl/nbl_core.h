/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_CORE_H_
#define _NBL_CORE_H_

#include "nbl_include.h"

#define NBL_VENDOR_ID				(0x1F0F)
#define NBL_DEVICE_ID_M18110			(0x3403)
#define NBL_DEVICE_ID_M18110_LX			(0x3404)
#define NBL_DEVICE_ID_M18110_BASE_T		(0x3405)
#define NBL_DEVICE_ID_M18110_LX_BASE_T		(0x3406)
#define NBL_DEVICE_ID_M18110_OCP		(0x3407)
#define NBL_DEVICE_ID_M18110_LX_OCP		(0x3408)
#define NBL_DEVICE_ID_M18110_BASE_T_OCP		(0x3409)
#define NBL_DEVICE_ID_M18110_LX_BASE_T_OCP	(0x340a)
#define NBL_DEVICE_ID_M18120			(0x340b)
#define NBL_DEVICE_ID_M18120_LX			(0x340c)
#define NBL_DEVICE_ID_M18120_BASE_T		(0x340d)
#define NBL_DEVICE_ID_M18120_LX_BASE_T		(0x340e)
#define NBL_DEVICE_ID_M18120_OCP		(0x340f)
#define NBL_DEVICE_ID_M18120_LX_OCP		(0x3410)
#define NBL_DEVICE_ID_M18120_BASE_T_OCP		(0x3411)
#define NBL_DEVICE_ID_M18120_LX_BASE_T_OCP	(0x3412)
#define NBL_DEVICE_ID_M18100_VF			(0x3413)

#define NBL_MAX_INSTANCE_CNT 516
struct nbl_adapter {
	TAILQ_ENTRY(nbl_adapter) next;
	struct rte_pci_device *pci_dev;
};

int nbl_core_init(struct nbl_adapter *adapter, const struct rte_eth_dev *eth_dev);
void nbl_core_remove(struct nbl_adapter *adapter);
int nbl_core_start(struct nbl_adapter *adapter);
void nbl_core_stop(struct nbl_adapter *adapter);

#endif
