/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CN10K_CRYPTODEV_OPS_H_
#define _CN10K_CRYPTODEV_OPS_H_

#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>

extern struct rte_cryptodev_ops cn10k_cpt_ops;

void cn10k_cpt_set_enqdeq_fns(struct rte_cryptodev *dev);

#endif /* _CN10K_CRYPTODEV_OPS_H_ */
