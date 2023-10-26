/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _MVTVM_ML_STUBS_H_
#define _MVTVM_ML_STUBS_H_

#include <rte_mldev.h>

struct cnxk_ml_dev;

int mvtvm_ml_dev_configure(struct cnxk_ml_dev *cnxk_mldev, const struct rte_ml_dev_config *conf);
int mvtvm_ml_dev_close(struct cnxk_ml_dev *cnxk_mldev);

#endif /* _MVTVM_ML_STUBS_H_ */
