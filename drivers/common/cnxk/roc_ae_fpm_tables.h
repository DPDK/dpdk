/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_AE_FPM_TABLES_H_
#define _ROC_AE_FPM_TABLES_H_

#include "roc_api.h"

int __roc_api roc_ae_fpm_get(uint64_t *tbl);
void __roc_api roc_ae_fpm_put(void);

#endif /* _ROC_AE_FPM_TABLES_H_ */
