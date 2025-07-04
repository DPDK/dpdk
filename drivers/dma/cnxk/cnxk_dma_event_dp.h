/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef _CNXK_DMA_EVENT_DP_H_
#define _CNXK_DMA_EVENT_DP_H_

#include <stdint.h>

#include <rte_common.h>
#include <rte_compat.h>
#include <rte_eventdev.h>

#ifdef __cplusplus
extern "C" {
#endif

__rte_internal
uint16_t cn10k_dma_adapter_enqueue(void *ws, struct rte_event ev[], uint16_t nb_events);

__rte_internal
uint16_t cn9k_dma_adapter_enqueue(void *ws, struct rte_event ev[], uint16_t nb_events);

__rte_internal
uint16_t cn9k_dma_adapter_dual_enqueue(void *ws, struct rte_event ev[], uint16_t nb_events);

__rte_internal
uintptr_t cnxk_dma_adapter_dequeue(uintptr_t get_work1);

#ifdef __cplusplus
}
#endif

#endif /* _CNXK_DMA_EVENT_DP_H_ */
