/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation.
 * All rights reserved.
 */

#ifndef _RTE_EVENT_ETH_RX_ADAPTER_
#define _RTE_EVENT_ETH_RX_ADAPTER_

/**
 * @file
 *
 * RTE Event Ethernet Rx Adapter
 *
 * An eventdev-based packet processing application enqueues/dequeues mbufs
 * to/from the event device. Packet flow from the ethernet device to the event
 * device can be accomplished using either HW or SW mechanisms depending on the
 * platform and the particular combination of ethernet and event devices. The
 * event ethernet Rx adapter provides common APIs to configure the packet flow
 * from the ethernet devices to event devices across both these transfer
 * mechanisms.
 *
 * The adapter uses a EAL service core function for SW based packet transfer
 * and uses the eventdev PMD functions to configure HW based packet transfer
 * between the ethernet device and the event device. For SW based packet
 * transfer, if the mbuf does not have a timestamp set, the adapter adds a
 * timestamp to the mbuf using rte_get_tsc_cycles(), this provides a more
 * accurate timestamp as compared to if the application were to set the time
 * stamp since it avoids event device schedule latency.
 *
 * The ethernet Rx event adapter's functions are:
 *  - rte_event_eth_rx_adapter_create_ext()
 *  - rte_event_eth_rx_adapter_create()
 *  - rte_event_eth_rx_adapter_free()
 *  - rte_event_eth_rx_adapter_queue_add()
 *  - rte_event_eth_rx_adapter_queue_del()
 *  - rte_event_eth_rx_adapter_start()
 *  - rte_event_eth_rx_adapter_stop()
 *  - rte_event_eth_rx_adapter_stats_get()
 *  - rte_event_eth_rx_adapter_stats_reset()
 *
 * The application creates an ethernet to event adapter using
 * rte_event_eth_rx_adapter_create_ext() or rte_event_eth_rx_adapter_create()
 * functions.
 * The adapter needs to know which ethernet rx queues to poll for mbufs as well
 * as event device parameters such as the event queue identifier, event
 * priority and scheduling type that the adapter should use when constructing
 * events. The rte_event_eth_rx_adapter_queue_add() function is provided for
 * this purpose.
 * The servicing weight parameter in the rte_event_eth_rx_adapter_queue_conf
 * is applicable when the Rx adapter uses a service core function and is
 * intended to provide application control of the frequency of polling ethernet
 * device receive queues, for example, the application may want to poll higher
 * priority queues with a higher frequency but at the same time not starve
 * lower priority queues completely. If this parameter is zero and the receive
 * interrupt is enabled when configuring the device, the receive queue is
 * interrupt driven; else, the queue is assigned a servicing weight of one.
 *
 * The application can start/stop the adapter using the
 * rte_event_eth_rx_adapter_start() and the rte_event_eth_rx_adapter_stop()
 * functions. If the adapter uses a rte_service function, then the application
 * is also required to assign a core to the service function and control the
 * service core using the rte_service APIs. The
 * rte_event_eth_rx_adapter_service_id_get() function can be used to retrieve
 * the service function ID of the adapter in this case.
 *
 * Note:
 * 1) Interrupt driven receive queues are currently unimplemented.
 * 2) Devices created after an instance of rte_event_eth_rx_adapter_create
 *  should be added to a new instance of the rx adapter.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_service.h>

#include "rte_eventdev.h"

#define RTE_EVENT_ETH_RX_ADAPTER_MAX_INSTANCE 32

/* struct rte_event_eth_rx_adapter_queue_conf flags definitions */
#define RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID	0x1
/**< This flag indicates the flow identifier is valid
 * @see rte_event_eth_rx_adapter_queue_conf::rx_queue_flags
 */

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Adapter configuration structure that the adapter configuration callback
 * function is expected to fill out
 * @see rte_event_eth_rx_adapter_conf_cb
 */
struct rte_event_eth_rx_adapter_conf {
	uint8_t event_port_id;
	/**< Event port identifier, the adapter enqueues mbuf events to this
	 * port.
	 */
	uint32_t max_nb_rx;
	/**< The adapter can return early if it has processed at least
	 * max_nb_rx mbufs. This isn't treated as a requirement; batching may
	 * cause the adapter to process more than max_nb_rx mbufs.
	 */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Function type used for adapter configuration callback. The callback is
 * used to fill in members of the struct rte_event_eth_rx_adapter_conf, this
 * callback is invoked when creating a SW service for packet transfer from
 * ethdev queues to the event device. The SW service is created within the
 * rte_event_eth_rx_adapter_queue_add() function if SW based packet transfers
 * from ethdev queues to the event device are required.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param dev_id
 *  Event device identifier.
 *
 * @param [out] conf
 *  Structure that needs to be populated by this callback.
 *
 * @param arg
 *  Argument to the callback. This is the same as the conf_arg passed to the
 *  rte_event_eth_rx_adapter_create_ext().
 */
typedef int (*rte_event_eth_rx_adapter_conf_cb) (uint8_t id, uint8_t dev_id,
			struct rte_event_eth_rx_adapter_conf *conf,
			void *arg);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Rx queue configuration structure
 */
struct rte_event_eth_rx_adapter_queue_conf {
	uint32_t rx_queue_flags;
	 /**< Flags for handling received packets
	  * @see RTE_EVENT_ETH_RX_ADAPTER_QUEUE_FLOW_ID_VALID
	  */
	uint16_t servicing_weight;
	/**< Relative polling frequency of ethernet receive queue when the
	 * adapter uses a service core function for ethernet to event device
	 * transfers. If it is set to zero, the Rx queue is interrupt driven
	 * (unless rx queue interrupts are not enabled for the ethernet
	 * device).
	 */
	struct rte_event ev;
	/**<
	 *  The values from the following event fields will be used when
	 *  queuing mbuf events:
	 *   - event_queue_id: Targeted event queue ID for received packets.
	 *   - event_priority: Event priority of packets from this Rx queue in
	 *                     the event queue relative to other events.
	 *   - sched_type: Scheduling type for packets from this Rx queue.
	 *   - flow_id: If the RTE_ETH_RX_EVENT_ADAPTER_QUEUE_FLOW_ID_VALID bit
	 *		is set in rx_queue_flags, this flow_id is used for all
	 *		packets received from this queue. Otherwise the flow ID
	 *		is set to the RSS hash of the src and dst IPv4/6
	 *		addresses.
	 *
	 * The event adapter sets ev.event_type to RTE_EVENT_TYPE_ETHDEV in the
	 * enqueued event.
	 */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * A structure used to retrieve statistics for an eth rx adapter instance.
 */
struct rte_event_eth_rx_adapter_stats {
	uint64_t rx_poll_count;
	/**< Receive queue poll count */
	uint64_t rx_packets;
	/**< Received packet count */
	uint64_t rx_enq_count;
	/**< Eventdev enqueue count */
	uint64_t rx_enq_retry;
	/**< Eventdev enqueue retry count */
	uint64_t rx_enq_start_ts;
	/**< Rx enqueue start timestamp */
	uint64_t rx_enq_block_cycles;
	/**< Cycles for which the service is blocked by the event device,
	 * i.e, the service fails to enqueue to the event device.
	 */
	uint64_t rx_enq_end_ts;
	/**< Latest timestamp at which the service is unblocked
	 * by the event device. The start, end timestamps and
	 * block cycles can be used to compute the percentage of
	 * cycles the service is blocked by the event device.
	 */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Create a new ethernet Rx event adapter with the specified identifier.
 *
 * @param id
 *  The identifier of the ethernet Rx event adapter.
 *
 * @param dev_id
 *  The identifier of the device to configure.
 *
 * @param conf_cb
 *  Callback function that fills in members of a
 *  struct rte_event_eth_rx_adapter_conf struct passed into
 *  it.
 *
 * @param conf_arg
 *  Argument that is passed to the conf_cb function.
 *
 * @return
 *   - 0: Success
 *   - <0: Error code on failure
 */
int rte_event_eth_rx_adapter_create_ext(uint8_t id, uint8_t dev_id,
				rte_event_eth_rx_adapter_conf_cb conf_cb,
				void *conf_arg);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Create a new ethernet Rx event adapter with the specified identifier.
 * This function uses an internal configuration function that creates an event
 * port. This default function reconfigures the event device with an
 * additional event port and setups up the event port using the port_config
 * parameter passed into this function. In case the application needs more
 * control in configuration of the service, it should use the
 * rte_event_eth_rx_adapter_create_ext() version.
 *
 * @param id
 *  The identifier of the ethernet Rx event adapter.
 *
 * @param dev_id
 *  The identifier of the device to configure.
 *
 * @param port_config
 *  Argument of type *rte_event_port_conf* that is passed to the conf_cb
 *  function.
 *
 * @return
 *   - 0: Success
 *   - <0: Error code on failure
 */
int rte_event_eth_rx_adapter_create(uint8_t id, uint8_t dev_id,
				struct rte_event_port_conf *port_config);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Free an event adapter
 *
 * @param id
 *  Adapter identifier.
 *
 * @return
 *   - 0: Success
 *   - <0: Error code on failure, If the adapter still has Rx queues
 *      added to it, the function returns -EBUSY.
 */
int rte_event_eth_rx_adapter_free(uint8_t id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Add receive queue to an event adapter. After a queue has been
 * added to the event adapter, the result of the application calling
 * rte_eth_rx_burst(eth_dev_id, rx_queue_id, ..) is undefined.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param eth_dev_id
 *  Port identifier of Ethernet device.
 *
 * @param rx_queue_id
 *  Ethernet device receive queue index.
 *  If rx_queue_id is -1, then all Rx queues configured for
 *  the device are added. If the ethdev Rx queues can only be
 *  connected to a single event queue then rx_queue_id is
 *  required to be -1.
 * @see RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ
 *
 * @param conf
 *  Additional configuration structure of type *rte_event_eth_rx_adapter_conf*
 *
 * @return
 *  - 0: Success, Receive queue added correctly.
 *  - <0: Error code on failure.
 *  - (-EIO) device reconfiguration and restart error. The adapter reconfigures
 *  the event device with an additional port if it is required to use a service
 *  function for packet transfer from the ethernet device to the event device.
 *  If the device had been started before this call, this error code indicates
 *  an error in restart following an error in reconfiguration, i.e., a
 *  combination of the two error codes.
 */
int rte_event_eth_rx_adapter_queue_add(uint8_t id,
			uint16_t eth_dev_id,
			int32_t rx_queue_id,
			const struct rte_event_eth_rx_adapter_queue_conf *conf);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Delete receive queue from an event adapter.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param eth_dev_id
 *  Port identifier of Ethernet device.
 *
 * @param rx_queue_id
 *  Ethernet device receive queue index.
 *  If rx_queue_id is -1, then all Rx queues configured for
 *  the device are deleted. If the ethdev Rx queues can only be
 *  connected to a single event queue then rx_queue_id is
 *  required to be -1.
 * @see RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ
 *
 * @return
 *  - 0: Success, Receive queue deleted correctly.
 *  - <0: Error code on failure.
 */
int rte_event_eth_rx_adapter_queue_del(uint8_t id, uint16_t eth_dev_id,
				       int32_t rx_queue_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Start ethernet Rx event adapter
 *
 * @param id
 *  Adapter identifier.
 *
 * @return
 *  - 0: Success, Adapter started correctly.
 *  - <0: Error code on failure.
 */
int rte_event_eth_rx_adapter_start(uint8_t id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Stop  ethernet Rx event adapter
 *
 * @param id
 *  Adapter identifier.
 *
 * @return
 *  - 0: Success, Adapter started correctly.
 *  - <0: Error code on failure.
 */
int rte_event_eth_rx_adapter_stop(uint8_t id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Retrieve statistics for an adapter
 *
 * @param id
 *  Adapter identifier.
 *
 * @param [out] stats
 *  A pointer to structure used to retrieve statistics for an adapter.
 *
 * @return
 *  - 0: Success, retrieved successfully.
 *  - <0: Error code on failure.
 */
int rte_event_eth_rx_adapter_stats_get(uint8_t id,
				struct rte_event_eth_rx_adapter_stats *stats);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Reset statistics for an adapter.
 *
 * @param id
 *  Adapter identifier.
 *
 * @return
 *  - 0: Success, statistics reset successfully.
 *  - <0: Error code on failure.
 */
int rte_event_eth_rx_adapter_stats_reset(uint8_t id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Retrieve the service ID of an adapter. If the adapter doesn't use
 * a rte_service function, this function returns -ESRCH.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param [out] service_id
 *  A pointer to a uint32_t, to be filled in with the service id.
 *
 * @return
 *  - 0: Success
 *  - <0: Error code on failure, if the adapter doesn't use a rte_service
 * function, this function returns -ESRCH.
 */
int rte_event_eth_rx_adapter_service_id_get(uint8_t id, uint32_t *service_id);

#ifdef __cplusplus
}
#endif
#endif	/* _RTE_EVENT_ETH_RX_ADAPTER_ */
