/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Marvell International Ltd.
 * All rights reserved.
 */

#ifndef _EVENT_VECTOR_ADAPTER_PMD_H_
#define _EVENT_VECTOR_ADAPTER_PMD_H_

/**
 * @file
 * Event Vector Adapter API (PMD Side)
 *
 * @note
 * This file provides implementation helpers for internal use by PMDs.  They
 * are not intended to be exposed to applications and are not subject to ABI
 * versioning.
 */

#include "eventdev_pmd.h"
#include "rte_event_vector_adapter.h"

typedef int (*rte_event_vector_adapter_create_t)(struct rte_event_vector_adapter *adapter);
/**< @internal Event vector adapter implementation setup */
typedef int (*rte_event_vector_adapter_destroy_t)(struct rte_event_vector_adapter *adapter);
/**< @internal Event vector adapter implementation teardown */
typedef int (*rte_event_vector_adapter_stats_get_t)(const struct rte_event_vector_adapter *adapter,
						    struct rte_event_vector_adapter_stats *stats);
/**< @internal Get statistics for event vector adapter */
typedef int (*rte_event_vector_adapter_stats_reset_t)(
	const struct rte_event_vector_adapter *adapter);
/**< @internal Reset statistics for event vector adapter */

/**
 * @internal Structure containing the functions exported by an event vector
 * adapter implementation.
 */
struct event_vector_adapter_ops {
	rte_event_vector_adapter_create_t create;
	/**< Set up adapter */
	rte_event_vector_adapter_destroy_t destroy;
	/**< Tear down adapter */
	rte_event_vector_adapter_stats_get_t stats_get;
	/**< Get adapter statistics */
	rte_event_vector_adapter_stats_reset_t stats_reset;
	/**< Reset adapter statistics */

	rte_event_vector_adapter_enqueue_t enqueue;
	/**< Enqueue objects into the event vector adapter */
};
/**
 * @internal Adapter data; structure to be placed in shared memory to be
 * accessible by various processes in a multi-process configuration.
 */
struct __rte_cache_aligned rte_event_vector_adapter_data {
	uint32_t id;
	/**< Event vector adapter ID */
	uint8_t event_dev_id;
	/**< Event device ID */
	uint32_t socket_id;
	/**< Socket ID where memory is allocated */
	uint8_t event_port_id;
	/**< Optional: event port ID used when the inbuilt port is absent */
	const struct rte_memzone *mz;
	/**< Event vector adapter memzone pointer */
	struct rte_event_vector_adapter_conf conf;
	/**< Configuration used to configure the adapter. */
	uint32_t caps;
	/**< Adapter capabilities */
	void *adapter_priv;
	/**< Vector adapter private data*/
	uint8_t service_inited;
	/**< Service initialization state */
	uint32_t unified_service_id;
	/**< Unified Service ID*/
};

static inline int
dummy_vector_adapter_enqueue(struct rte_event_vector_adapter *adapter, uint64_t objs[],
			     uint16_t num_events, uint64_t flags)
{
	RTE_SET_USED(adapter);
	RTE_SET_USED(objs);
	RTE_SET_USED(num_events);
	RTE_SET_USED(flags);
	return 0;
}

#endif /* _EVENT_VECTOR_ADAPTER_PMD_H_ */
