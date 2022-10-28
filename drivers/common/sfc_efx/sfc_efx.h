/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_EFX_H_
#define _SFC_EFX_H_

#include <bus_pci_driver.h>

#include "efx.h"
#include "efsys.h"

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

enum sfc_efx_dev_class {
	SFC_EFX_DEV_CLASS_INVALID = 0,
	SFC_EFX_DEV_CLASS_NET,
	SFC_EFX_DEV_CLASS_VDPA,

	SFC_EFX_DEV_NCLASS
};

__rte_internal
enum sfc_efx_dev_class sfc_efx_dev_class_get(struct rte_devargs *devargs);

__rte_internal
int sfc_efx_family(struct rte_pci_device *pci_dev,
		   efx_bar_region_t *mem_ebrp,
		   efx_family_t *family);

#ifdef __cplusplus
}
#endif

#endif /* _SFC_EFX_H_ */
