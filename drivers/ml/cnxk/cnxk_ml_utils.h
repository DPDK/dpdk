/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef _CNXK_ML_UTILS_H_
#define _CNXK_ML_UTILS_H_

#include <rte_mldev.h>

/* Debug print width */
#define STR_LEN	  12
#define FIELD_LEN 16
#define LINE_LEN  72

void cnxk_ml_print_line(FILE *fp, int len);

#endif /* _CNXK_ML_UTILS_H_ */
