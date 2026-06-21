/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2026 Marvell.
 */

#ifndef _ROC_RE_ML_TABLES_H_
#define _ROC_RE_ML_TABLES_H_

#include "roc_platform.h"

enum roc_re_ml_zeta_idx {
	ROC_RE_ML_ZETA_IDX_KEM = 0,
	ROC_RE_ML_ZETA_IDX_DSA,
	ROC_RE_ML_ZETA_IDX_MAX
};

int __roc_api roc_re_ml_zeta_get(uint64_t *tbl);
void __roc_api roc_re_ml_zeta_put(void);

#endif /* _ROC_RE_ML_TABLES_H_ */
