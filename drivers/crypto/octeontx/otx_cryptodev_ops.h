/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _OTX_CRYPTODEV_OPS_H_
#define _OTX_CRYPTODEV_OPS_H_

#include <rte_compat.h>
#include <cryptodev_pmd.h>

#define OTX_CPT_MIN_HEADROOM_REQ	(24)
#define OTX_CPT_MIN_TAILROOM_REQ	(8)
#define CPT_NUM_QS_PER_VF		(1)

int
otx_cpt_dev_create(struct rte_cryptodev *c_dev);

__rte_internal
uint16_t __rte_hot
otx_crypto_adapter_enqueue(void *port, struct rte_crypto_op *op);

__rte_internal
uintptr_t __rte_hot
otx_crypto_adapter_dequeue(uintptr_t get_work1);

#endif /* _OTX_CRYPTODEV_OPS_H_ */
