/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation.
 * Copyright(C) 2021 Marvell.
 * Copyright 2016 NXP
 * All rights reserved.
 */

#ifndef _RTE_EVENTDEV_CORE_H_
#define _RTE_EVENTDEV_CORE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t (*event_enqueue_burst_t)(void *port,
					  const struct rte_event ev[],
					  uint16_t nb_events);
/**< @internal Enqueue burst of events on port of a device */

typedef uint16_t (*event_dequeue_burst_t)(void *port, struct rte_event ev[],
					  uint16_t nb_events,
					  uint64_t timeout_ticks);
/**< @internal Dequeue burst of events from port of a device */

typedef void (*event_maintain_t)(void *port, int op);
/**< @internal Maintains a port */

typedef uint16_t (*event_tx_adapter_enqueue_t)(void *port,
					       struct rte_event ev[],
					       uint16_t nb_events);
/**< @internal Enqueue burst of events on port of a device */

typedef uint16_t (*event_crypto_adapter_enqueue_t)(void *port,
						   struct rte_event ev[],
						   uint16_t nb_events);
/**< @internal Enqueue burst of events on crypto adapter */

typedef uint16_t (*event_dma_adapter_enqueue_t)(void *port, struct rte_event ev[],
						uint16_t nb_events);
/**< @internal Enqueue burst of events on DMA adapter */

typedef int (*event_profile_switch_t)(void *port, uint8_t profile);
/**< @internal Switch active link profile on the event port. */

typedef int (*event_preschedule_modify_t)(void *port,
					  enum rte_event_dev_preschedule_type preschedule_type);
/**< @internal Modify pre-schedule type on the event port. */

typedef void (*event_preschedule_t)(void *port,
				    enum rte_event_dev_preschedule_type preschedule_type);
/**< @internal Issue pre-schedule on an event port. */

struct __rte_cache_aligned rte_event_fp_ops {
	void **data;
	/**< points to array of internal port data pointers */
	event_enqueue_burst_t enqueue_burst;
	/**< PMD enqueue burst function. */
	event_enqueue_burst_t enqueue_new_burst;
	/**< PMD enqueue burst new function. */
	event_enqueue_burst_t enqueue_forward_burst;
	/**< PMD enqueue burst fwd function. */
	event_dequeue_burst_t dequeue_burst;
	/**< PMD dequeue burst function. */
	event_maintain_t maintain;
	/**< PMD port maintenance function. */
	event_tx_adapter_enqueue_t txa_enqueue;
	/**< PMD Tx adapter enqueue function. */
	event_tx_adapter_enqueue_t txa_enqueue_same_dest;
	/**< PMD Tx adapter enqueue same destination function. */
	event_crypto_adapter_enqueue_t ca_enqueue;
	/**< PMD Crypto adapter enqueue function. */
	event_dma_adapter_enqueue_t dma_enqueue;
	/**< PMD DMA adapter enqueue function. */
	event_profile_switch_t profile_switch;
	/**< PMD Event switch profile function. */
	event_preschedule_modify_t preschedule_modify;
	/**< PMD Event port pre-schedule switch. */
	event_preschedule_t preschedule;
	/**< PMD Event port pre-schedule. */
	uintptr_t reserved[2];
};

extern struct rte_event_fp_ops rte_event_fp_ops[RTE_EVENT_MAX_DEVS];

#ifdef __cplusplus
}
#endif

#endif /*_RTE_EVENTDEV_CORE_H_*/
