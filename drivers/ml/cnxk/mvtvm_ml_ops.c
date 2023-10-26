/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include "cnxk_ml_dev.h"
#include "cnxk_ml_ops.h"

int
mvtvm_ml_dev_configure(struct cnxk_ml_dev *cnxk_mldev, const struct rte_ml_dev_config *conf)
{
	int ret;

	RTE_SET_USED(conf);

	/* Configure TVMDP library */
	ret = tvmdp_configure(cnxk_mldev->mldev->data->nb_models, rte_get_tsc_cycles);
	if (ret != 0)
		plt_err("TVMDP configuration failed, error = %d\n", ret);

	return ret;
}

int
mvtvm_ml_dev_close(struct cnxk_ml_dev *cnxk_mldev)
{
	int ret;

	RTE_SET_USED(cnxk_mldev);

	/* Close TVMDP library configuration */
	ret = tvmdp_close();
	if (ret != 0)
		plt_err("TVMDP close failed, error = %d\n", ret);

	return ret;
}
