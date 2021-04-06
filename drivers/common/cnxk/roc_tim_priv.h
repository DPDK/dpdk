/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_TIM_PRIV_H_
#define _ROC_TIM_PRIV_H_

struct tim {
};

enum tim_err_status {
	TIM_ERR_PARAM = -5120,
};

static inline struct tim *
roc_tim_to_tim_priv(struct roc_tim *roc_tim)
{
	return (struct tim *)&roc_tim->reserved[0];
}

#endif /* _ROC_TIM_PRIV_H_ */
