/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

struct idev_cfg *
idev_get_cfg(void)
{
	static const char name[] = "roc_cn10k_intra_device_conf";
	const struct plt_memzone *mz;
	struct idev_cfg *idev;

	mz = plt_memzone_lookup(name);
	if (mz != NULL)
		return mz->addr;

	/* Request for the first time */
	mz = plt_memzone_reserve_cache_align(name, sizeof(struct idev_cfg));
	if (mz != NULL) {
		idev = mz->addr;
		idev_set_defaults(idev);
		return idev;
	}
	return NULL;
}

void
idev_set_defaults(struct idev_cfg *idev)
{
	idev->lmt_pf_func = 0;
	idev->lmt_base_addr = 0;
	idev->num_lmtlines = 0;
}

uint16_t
idev_lmt_pffunc_get(void)
{
	struct idev_cfg *idev;
	uint16_t lmt_pf_func;

	idev = idev_get_cfg();
	lmt_pf_func = 0;
	if (idev != NULL)
		lmt_pf_func = idev->lmt_pf_func;

	return lmt_pf_func;
}

uint64_t
roc_idev_lmt_base_addr_get(void)
{
	uint64_t lmt_base_addr;
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	lmt_base_addr = 0;
	if (idev != NULL)
		lmt_base_addr = idev->lmt_base_addr;

	return lmt_base_addr;
}

uint16_t
roc_idev_num_lmtlines_get(void)
{
	struct idev_cfg *idev;
	uint16_t num_lmtlines;

	idev = idev_get_cfg();
	num_lmtlines = 0;
	if (idev != NULL)
		num_lmtlines = idev->num_lmtlines;

	return num_lmtlines;
}
