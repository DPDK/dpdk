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

typedef uint16_t (*event_enqueue_t)(void *port, const struct rte_event *ev);
/**< @internal Enqueue event on port of a device */

typedef uint16_t (*event_enqueue_burst_t)(void *port,
					  const struct rte_event ev[],
					  uint16_t nb_events);
/**< @internal Enqueue burst of events on port of a device */

typedef uint16_t (*event_dequeue_t)(void *port, struct rte_event *ev,
				    uint64_t timeout_ticks);
/**< @internal Dequeue event from port of a device */

typedef uint16_t (*event_dequeue_burst_t)(void *port, struct rte_event ev[],
					  uint16_t nb_events,
					  uint64_t timeout_ticks);
/**< @internal Dequeue burst of events from port of a device */

typedef uint16_t (*event_tx_adapter_enqueue_t)(void *port,
					       struct rte_event ev[],
					       uint16_t nb_events);
/**< @internal Enqueue burst of events on port of a device */

typedef uint16_t (*event_crypto_adapter_enqueue_t)(void *port,
						   struct rte_event ev[],
						   uint16_t nb_events);
/**< @internal Enqueue burst of events on crypto adapter */

#define RTE_EVENTDEV_NAME_MAX_LEN (64)
/**< @internal Max length of name of event PMD */

/**
 * @internal
 * The data part, with no function pointers, associated with each device.
 *
 * This structure is safe to place in shared memory to be common among
 * different processes in a multi-process configuration.
 */
struct rte_eventdev_data {
	int socket_id;
	/**< Socket ID where memory is allocated */
	uint8_t dev_id;
	/**< Device ID for this instance */
	uint8_t nb_queues;
	/**< Number of event queues. */
	uint8_t nb_ports;
	/**< Number of event ports. */
	void *ports[RTE_EVENT_MAX_PORTS_PER_DEV];
	/**< Array of pointers to ports. */
	struct rte_event_port_conf ports_cfg[RTE_EVENT_MAX_PORTS_PER_DEV];
	/**< Array of port configuration structures. */
	struct rte_event_queue_conf queues_cfg[RTE_EVENT_MAX_QUEUES_PER_DEV];
	/**< Array of queue configuration structures. */
	uint16_t links_map[RTE_EVENT_MAX_PORTS_PER_DEV *
			   RTE_EVENT_MAX_QUEUES_PER_DEV];
	/**< Memory to store queues to port connections. */
	void *dev_private;
	/**< PMD-specific private data */
	uint32_t event_dev_cap;
	/**< Event device capabilities(RTE_EVENT_DEV_CAP_)*/
	struct rte_event_dev_config dev_conf;
	/**< Configuration applied to device. */
	uint8_t service_inited;
	/* Service initialization state */
	uint32_t service_id;
	/* Service ID*/
	void *dev_stop_flush_arg;
	/**< User-provided argument for event flush function */

	RTE_STD_C11
	uint8_t dev_started : 1;
	/**< Device state: STARTED(1)/STOPPED(0) */

	char name[RTE_EVENTDEV_NAME_MAX_LEN];
	/**< Unique identifier name */

	uint64_t reserved_64s[4]; /**< Reserved for future fields */
	void *reserved_ptrs[4];	  /**< Reserved for future fields */
} __rte_cache_aligned;

/** @internal The data structure associated with each event device. */
struct rte_eventdev {
	event_enqueue_t enqueue;
	/**< Pointer to PMD enqueue function. */
	event_enqueue_burst_t enqueue_burst;
	/**< Pointer to PMD enqueue burst function. */
	event_enqueue_burst_t enqueue_new_burst;
	/**< Pointer to PMD enqueue burst function(op new variant) */
	event_enqueue_burst_t enqueue_forward_burst;
	/**< Pointer to PMD enqueue burst function(op forward variant) */
	event_dequeue_t dequeue;
	/**< Pointer to PMD dequeue function. */
	event_dequeue_burst_t dequeue_burst;
	/**< Pointer to PMD dequeue burst function. */
	event_tx_adapter_enqueue_t txa_enqueue_same_dest;
	/**< Pointer to PMD eth Tx adapter burst enqueue function with
	 * events destined to same Eth port & Tx queue.
	 */
	event_tx_adapter_enqueue_t txa_enqueue;
	/**< Pointer to PMD eth Tx adapter enqueue function. */
	struct rte_eventdev_data *data;
	/**< Pointer to device data */
	struct eventdev_ops *dev_ops;
	/**< Functions exported by PMD */
	struct rte_device *dev;
	/**< Device info. supplied by probing */

	RTE_STD_C11
	uint8_t attached : 1;
	/**< Flag indicating the device is attached */

	event_crypto_adapter_enqueue_t ca_enqueue;
	/**< Pointer to PMD crypto adapter enqueue function. */

	uint64_t reserved_64s[4]; /**< Reserved for future fields */
	void *reserved_ptrs[3];	  /**< Reserved for future fields */
} __rte_cache_aligned;

extern struct rte_eventdev *rte_eventdevs;
/** @internal The pool of rte_eventdev structures. */

#ifdef __cplusplus
}
#endif

#endif /*_RTE_EVENTDEV_CORE_H_*/
