/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation.
 * All rights reserved.
 */

#ifndef __EVENT_TIMER_ADAPTER_PMD_H__
#define __EVENT_TIMER_ADAPTER_PMD_H__

/**
 * @file
 * RTE Event Timer Adapter API (PMD Side)
 *
 * @note
 * This file provides implementation helpers for internal use by PMDs.  They
 * are not intended to be exposed to applications and are not subject to ABI
 * versioning.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "rte_event_timer_adapter.h"

/*
 * Definitions of functions exported by an event timer adapter implementation
 * through *rte_event_timer_adapter_ops* structure supplied in the
 * *rte_event_timer_adapter* structure associated with an event timer adapter.
 */

typedef int (*rte_event_timer_adapter_init_t)(
		struct rte_event_timer_adapter *adapter);
/**< @internal Event timer adapter implementation setup */
typedef int (*rte_event_timer_adapter_uninit_t)(
		struct rte_event_timer_adapter *adapter);
/**< @internal Event timer adapter implementation teardown */
typedef int (*rte_event_timer_adapter_start_t)(
		const struct rte_event_timer_adapter *adapter);
/**< @internal Start running event timer adapter */
typedef int (*rte_event_timer_adapter_stop_t)(
		const struct rte_event_timer_adapter *adapter);
/**< @internal Stop running event timer adapter */
typedef void (*rte_event_timer_adapter_get_info_t)(
		const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer_adapter_info *adapter_info);
/**< @internal Get contextual information for event timer adapter */
typedef int (*rte_event_timer_adapter_stats_get_t)(
		const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer_adapter_stats *stats);
/**< @internal Get statistics for event timer adapter */
typedef int (*rte_event_timer_adapter_stats_reset_t)(
		const struct rte_event_timer_adapter *adapter);
/**< @internal Reset statistics for event timer adapter */
typedef int (*rte_event_timer_remaining_ticks_get_t)(
		const struct rte_event_timer_adapter *adapter,
		const struct rte_event_timer *evtim,
		uint64_t *ticks_remaining);
/**< @internal Get remaining ticks for event timer */

/**
 * @internal Structure containing the functions exported by an event timer
 * adapter implementation.
 */
struct event_timer_adapter_ops {
	rte_event_timer_adapter_init_t		init;  /**< Set up adapter */
	rte_event_timer_adapter_uninit_t	uninit;/**< Tear down adapter */
	rte_event_timer_adapter_start_t		start; /**< Start adapter */
	rte_event_timer_adapter_stop_t		stop;  /**< Stop adapter */
	rte_event_timer_adapter_get_info_t	get_info;
	/**< Get info from driver */
	rte_event_timer_adapter_stats_get_t	stats_get;
	/**< Get adapter statistics */
	rte_event_timer_adapter_stats_reset_t	stats_reset;
	/**< Reset adapter statistics */
	rte_event_timer_arm_burst_t		arm_burst;
	/**< Arm one or more event timers */
	rte_event_timer_arm_tmo_tick_burst_t	arm_tmo_tick_burst;
	/**< Arm event timers with same expiration time */
	rte_event_timer_cancel_burst_t		cancel_burst;
	/**< Cancel one or more event timers */
	rte_event_timer_remaining_ticks_get_t	remaining_ticks_get;
	/**< Get remaining ticks for event timer */
};

/**
 * @internal Adapter data; structure to be placed in shared memory to be
 * accessible by various processes in a multi-process configuration.
 */
struct rte_event_timer_adapter_data {
	uint8_t id;
	/**< Event timer adapter ID */
	uint8_t event_dev_id;
	/**< Event device ID */
	uint32_t socket_id;
	/**< Socket ID where memory is allocated */
	uint8_t event_port_id;
	/**< Optional: event port ID used when the inbuilt port is absent */
	const struct rte_memzone *mz;
	/**< Event timer adapter memzone pointer */
	struct rte_event_timer_adapter_conf conf;
	/**< Configuration used to configure the adapter. */
	uint32_t caps;
	/**< Adapter capabilities */
	void *adapter_priv;
	/**< Timer adapter private data*/
	uint8_t service_inited;
	/**< Service initialization state */
	uint32_t service_id;
	/**< Service ID*/

	uint8_t started : 1;
	/**< Flag to indicate adapter started. */
} __rte_cache_aligned;

#ifdef __cplusplus
}
#endif

#endif /* __EVENT_TIMER_ADAPTER_PMD_H__ */
