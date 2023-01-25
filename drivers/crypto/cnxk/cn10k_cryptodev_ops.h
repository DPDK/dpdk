/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CN10K_CRYPTODEV_OPS_H_
#define _CN10K_CRYPTODEV_OPS_H_

#include <rte_compat.h>
#include <cryptodev_pmd.h>
#include <rte_cryptodev.h>
#include <rte_eventdev.h>

#include "cnxk_cryptodev.h"

extern struct rte_cryptodev_ops cn10k_cpt_ops;

void cn10k_cpt_set_enqdeq_fns(struct rte_cryptodev *dev, struct cnxk_cpt_vf *vf);

__rte_internal
uint16_t __rte_hot cn10k_cpt_sg_ver1_crypto_adapter_enqueue(void *ws, struct rte_event ev[],
		uint16_t nb_events);
__rte_internal
uint16_t __rte_hot cn10k_cpt_sg_ver2_crypto_adapter_enqueue(void *ws, struct rte_event ev[],
		uint16_t nb_events);

#endif /* _CN10K_CRYPTODEV_OPS_H_ */
