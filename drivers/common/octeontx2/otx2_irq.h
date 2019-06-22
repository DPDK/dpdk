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

#endif /* _OTX2_IRQ_H_ */
