/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _MVTVM_ML_STUBS_H_
#define _MVTVM_ML_STUBS_H_

#include <rte_mldev.h>

struct cnxk_ml_dev;

int mvtvm_ml_dev_info_get(struct cnxk_ml_dev *cnxk_mldev, struct rte_ml_dev_info *dev_info);
int mvtvm_ml_dev_dump(struct cnxk_ml_dev *cnxk_mldev, FILE *fp);

#endif /* _MVTVM_ML_STUBS_H_ */
