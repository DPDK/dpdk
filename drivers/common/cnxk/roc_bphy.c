/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
roc_bphy_dev_init(struct roc_bphy *roc_bphy)
{
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	if (!idev)
		return -ENODEV;

	if (!roc_bphy || !roc_bphy->pci_dev)
		return -EINVAL;

	idev->bphy = roc_bphy;

	return 0;
}

int
roc_bphy_dev_fini(struct roc_bphy *roc_bphy)
{
	struct idev_cfg *idev;

	idev = idev_get_cfg();
	if (!idev)
		return -ENODEV;

	if (!roc_bphy)
		return -EINVAL;

	idev->bphy = NULL;

	return 0;
}

uint16_t
roc_bphy_npa_pf_func_get(void)
{
	return idev_npa_pffunc_get();
}

uint16_t
roc_bphy_sso_pf_func_get(void)
{
	return idev_sso_pffunc_get();
}
