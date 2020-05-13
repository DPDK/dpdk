/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_IRQ_H_
#define _OTX2_IRQ_H_

#include <rte_pci.h>
#include <rte_interrupts.h>

#include "otx2_common.h"

typedef struct {
/* 128 devices translate to two 64 bits dwords */
#define MAX_VFPF_DWORD_BITS 2
	uint64_t bits[MAX_VFPF_DWORD_BITS];
} otx2_intr_t;

__rte_internal
int otx2_register_irq(struct rte_intr_handle *intr_handle,
		      rte_intr_callback_fn cb, void *data, unsigned int vec);
__rte_internal
void otx2_unregister_irq(struct rte_intr_handle *intr_handle,
			 rte_intr_callback_fn cb, void *data, unsigned int vec);
__rte_internal
int otx2_disable_irqs(struct rte_intr_handle *intr_handle);

#endif /* _OTX2_IRQ_H_ */
