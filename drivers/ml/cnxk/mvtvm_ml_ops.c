/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <stdio.h>

#include "cnxk_ml_dev.h"
#include "mvtvm_ml_ops.h"

int
mvtvm_ml_dev_info_get(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_dev_info *dev_info)
{
	struct mvtvm_ml_dev *tvmrt_mldev;

	tvmrt_mldev = &cnxk_mldev->mvtvm_mldev;

	dev_info->max_queue_pairs = tvmrt_mldev->max_nb_qpairs;
	dev_info->max_desc = ML_MVTVM_MAX_DESC_PER_QP;
	dev_info->max_io = ML_CNXK_MODEL_MAX_INPUT_OUTPUT;
	dev_info->max_segments = ML_MVTVM_MAX_SEGMENTS;
	dev_info->align_size = RTE_CACHE_LINE_SIZE;

	return 0;
}

int
mvtvm_ml_dev_dump(struct cnxk_ml_dev *cnxk_mldev, FILE *fp)
{
	RTE_SET_USED(cnxk_mldev);
	RTE_SET_USED(fp);

	return 0;
}
