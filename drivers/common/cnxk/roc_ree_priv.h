/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_REE_PRIV_H_
#define _ROC_REE_PRIV_H_

struct ree {
	struct dev dev;
} __plt_cache_aligned;

static inline struct ree *
roc_ree_to_ree_priv(struct roc_ree_vf *roc_ree)
{
	return (struct ree *)&roc_ree->reserved[0];
}

#endif /* _ROC_REE_PRIV_H_ */
