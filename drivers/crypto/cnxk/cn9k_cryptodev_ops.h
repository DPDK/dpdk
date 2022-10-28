/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CN9K_CRYPTODEV_OPS_H_
#define _CN9K_CRYPTODEV_OPS_H_

#include <rte_compat.h>
#include <cryptodev_pmd.h>

extern struct rte_cryptodev_ops cn9k_cpt_ops;

void cn9k_cpt_set_enqdeq_fns(struct rte_cryptodev *dev);

__rte_internal
uint16_t cn9k_cpt_crypto_adapter_enqueue(uintptr_t base,
					 struct rte_crypto_op *op);
__rte_internal
uintptr_t cn9k_cpt_crypto_adapter_dequeue(uintptr_t get_work1);

#endif /* _CN9K_CRYPTODEV_OPS_H_ */
