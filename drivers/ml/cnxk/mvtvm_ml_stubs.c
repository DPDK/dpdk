/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_mldev.h>

#include "mvtvm_ml_stubs.h"

#include "cnxk_ml_dev.h"

int
mvtvm_ml_dev_info_get(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_dev_info *dev_info)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(dev_info);

	return -ENOTSUP;
}

int
mvtvm_ml_dev_dump(struct cnxk_ml_dev *cnxk_mldev, FILE *fp)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(fp);

	return -EINVAL;
}
