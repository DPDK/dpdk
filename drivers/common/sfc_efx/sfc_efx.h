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

#ifdef __cplusplus
extern "C" {
#endif

#define SFC_EFX_KVARG_DEV_CLASS	"class"

enum sfc_efx_dev_class {
	SFC_EFX_DEV_CLASS_INVALID = 0,
	SFC_EFX_DEV_CLASS_NET,
	SFC_EFX_DEV_CLASS_VDPA,

	SFC_EFX_DEV_NCLASS
};

__rte_internal
enum sfc_efx_dev_class sfc_efx_dev_class_get(struct rte_devargs *devargs);

#ifdef __cplusplus
}
#endif

#endif /* _SFC_EFX_H_ */
