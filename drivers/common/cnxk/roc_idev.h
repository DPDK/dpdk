/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_IDEV_H_
#define _ROC_IDEV_H_

/* LMT */
uint64_t __roc_api roc_idev_lmt_base_addr_get(void);
uint16_t __roc_api roc_idev_num_lmtlines_get(void);

#endif /* _ROC_IDEV_H_ */
