/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_IDEV_PRIV_H_
#define _ROC_IDEV_PRIV_H_

/* Intra device related functions */
struct idev_cfg {
	uint16_t lmt_pf_func;
	uint16_t num_lmtlines;
	uint64_t lmt_base_addr;
};

/* Generic */
struct idev_cfg *idev_get_cfg(void);
void idev_set_defaults(struct idev_cfg *idev);

/* idev lmt */
uint16_t idev_lmt_pffunc_get(void);

#endif /* _ROC_IDEV_PRIV_H_ */
