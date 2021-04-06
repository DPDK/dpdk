/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_UTILS_H_
#define _ROC_UTILS_H_

#include "roc_platform.h"

/* Utils */
const char *__roc_api roc_error_msg_get(int errorcode);

void __roc_api roc_clk_freq_get(uint16_t *rclk_freq, uint16_t *sclk_freq);

#endif /* _ROC_UTILS_H_ */
