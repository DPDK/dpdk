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
 * between the ethernet device and the event device.
 *
 * The ethernet Rx event adapter's functions are:
 *  - rte_event_eth_rx_adapter_create_ext()
 *  - rte_event_eth_rx_adapter_create()
 *  - rte_event_eth_rx_adapter_create_with_params()
 *  - rte_event_eth_rx_adapter_free()
 *  - rte_event_eth_rx_adapter_queue_add()
 *  - rte_event_eth_rx_adapter_queue_del()
 *  - rte_event_eth_rx_adapter_start()
 *  - rte_event_eth_rx_adapter_stop()
 *  - rte_event_eth_rx_adapter_stats_get()
 *  - rte_event_eth_rx_adapter_stats_reset()
 *  - rte_event_eth_rx_adapter_queue_conf_get()
 *  - rte_event_eth_rx_adapter_queue_stats_get()
 *  - rte_event_eth_rx_adapter_queue_stats_reset()
 *
 * The application creates an ethernet to event adapter using
 * rte_event_eth_rx_adapter_create_ext() or rte_event_eth_rx_adapter_create()
 * or rte_event_eth_rx_adapter_create_with_params() functions.
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
 * For SW based packet transfers, i.e., when the
 * RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT is not set in the adapter's
 * capabilities flags for a particular ethernet device, the service function
 * temporarily enqueues events to an event buffer before batch enqueuing these
 * to the event device. If the buffer fills up, the service function stops
 * dequeuing packets from the ethernet device. The application may want to
 * monitor the buffer fill level and instruct the service function to
 * selectively buffer events. The application may also use some other
 * criteria to decide which packets should enter the event device even when
 * the event buffer fill level is low or may want to enqueue packets to an
 * internal event port. The rte_event_eth_rx_adapter_cb_register() function
 * allows the application to register a callback that selects which packets are
 * enqueued to the event device by the SW adapter. The callback interface is
 * event based so the callback can also modify the event data if it needs to.
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
#define RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR	0x2
/**< This flag indicates that mbufs arriving on the queue need to be vectorized
 * @see rte_event_eth_rx_adapter_queue_conf::rx_queue_flags
 */

/**
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
	uint16_t vector_sz;
	/**<
	 * Indicates the maximum number for mbufs to combine and form a vector.
	 * Should be within
	 * @see rte_event_eth_rx_adapter_vector_limits::min_vector_sz
	 * @see rte_event_eth_rx_adapter_vector_limits::max_vector_sz
	 * Valid when RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR flag is set in
	 * @see rte_event_eth_rx_adapter_queue_conf::rx_queue_flags
	 */
	uint64_t vector_timeout_ns;
	/**<
	 * Indicates the maximum number of nanoseconds to wait for receiving
	 * mbufs. Should be within vectorization limits of the
	 * adapter
	 * @see rte_event_eth_rx_adapter_vector_limits::min_vector_ns
	 * @see rte_event_eth_rx_adapter_vector_limits::max_vector_ns
	 * Valid when RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR flag is set in
	 * @see rte_event_eth_rx_adapter_queue_conf::rx_queue_flags
	 */
	struct rte_mempool *vector_mp;
	/**<
	 * Indicates the mempool that should be used for allocating
	 * rte_event_vector container.
	 * Should be created by using `rte_event_vector_pool_create`.
	 * Valid when RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR flag is set in
	 * @see rte_event_eth_rx_adapter_queue_conf::rx_queue_flags.
	 */
	uint16_t event_buf_size;
	/**< event buffer size for this queue */
};

/**
 * A structure used to retrieve statistics for an
 * eth rx adapter queue.
 */
struct rte_event_eth_rx_adapter_queue_stats {
	uint64_t rx_event_buf_count;
	/**< Rx event buffered count */
	uint64_t rx_event_buf_size;
	/**< Rx event buffer size */
	uint64_t rx_poll_count;
	/**< Receive queue poll count */
	uint64_t rx_packets;
	/**< Received packet count */
	uint64_t rx_dropped;
	/**< Received packet dropped count */
};

/**
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
	uint64_t rx_dropped;
	/**< Received packet dropped count */
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
	uint64_t rx_intr_packets;
	/**< Received packet count for interrupt mode Rx queues */
	uint64_t rx_event_buf_count;
	/**< Rx event buffered count */
	uint64_t rx_event_buf_size;
	/**< Rx event buffer size */
};

/**
 * A structure used to retrieve eth rx adapter vector limits.
 */
struct rte_event_eth_rx_adapter_vector_limits {
	uint16_t min_sz;
	/**< Minimum vector limit configurable.
	 * @see rte_event_eth_rx_adapter_event_vector_config::vector_sz
	 */
	uint16_t max_sz;
	/**< Maximum vector limit configurable.
	 * @see rte_event_eth_rx_adapter_event_vector_config::vector_sz
	 */
	uint8_t log2_sz;
	/**< True if the size configured should be in log2.
	 * @see rte_event_eth_rx_adapter_event_vector_config::vector_sz
	 */
	uint64_t min_timeout_ns;
	/**< Minimum vector timeout configurable.
	 * @see rte_event_eth_rx_adapter_event_vector_config::vector_timeout_ns
	 */
	uint64_t max_timeout_ns;
	/**< Maximum vector timeout configurable.
	 * @see rte_event_eth_rx_adapter_event_vector_config::vector_timeout_ns
	 */
};

/**
 * A structure to hold adapter config params
 */
struct rte_event_eth_rx_adapter_params {
	uint16_t event_buf_size;
	/**< size of event buffer for the adapter.
	 * This value is rounded up for better buffer utilization
	 * and performance.
	 */
	bool use_queue_event_buf;
	/**< flag to indicate that event buffer is separate for each queue */
};

/**
 *
 * Callback function invoked by the SW adapter before it continues
 * to process events. The callback is passed the size of the enqueue
 * buffer in the SW adapter and the occupancy of the buffer. The
 * callback can use these values to decide which events are
 * enqueued to the event device by the SW adapter. The callback may
 * also enqueue events internally using its own event port. The SW
 * adapter populates the event information based on the Rx queue
 * configuration in the adapter. The callback can modify the this event
 * information for the events to be enqueued by the SW adapter.
 *
 * The callback return value is the number of events from the
 * beginning of the event array that are to be enqueued by
 * the SW adapter. It is the callback's responsibility to arrange
 * these events at the beginning of the array, if these events are
 * not contiguous in the original array. The *nb_dropped* parameter is
 * a pointer to the number of events dropped by the callback, this
 * number is used by the adapter to indicate the number of dropped packets
 * as part of its statistics.
 *
 * @param eth_dev_id
 *  Port identifier of the Ethernet device.
 * @param queue_id
 *  Receive queue index.
 * @param enqueue_buf_size
 *  Total enqueue buffer size.
 * @param enqueue_buf_count
 *  Event count in enqueue buffer.
 * @param[in, out] ev
 *  Event array.
 * @param nb_event
 *  Event array length.
 * @param cb_arg
 *  Callback argument.
 * @param[out] nb_dropped
 *  Packets dropped by callback.
 * @return
 *  - The number of events to be enqueued by the SW adapter.
 */
typedef uint16_t (*rte_event_eth_rx_adapter_cb_fn)(uint16_t eth_dev_id,
						uint16_t queue_id,
						uint32_t enqueue_buf_size,
						uint32_t enqueue_buf_count,
						struct rte_event *ev,
						uint16_t nb_event,
						void *cb_arg,
						uint16_t *nb_dropped);

/**
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
 * This is a variant of rte_event_eth_rx_adapter_create() with additional
 * adapter params specified in ``struct rte_event_eth_rx_adapter_params``.
 *
 * @param id
 *  The identifier of the ethernet Rx event adapter.
 *
 * @param dev_id
 *  The identifier of the event device to configure.
 *
 * @param port_config
 *  Argument of type *rte_event_port_conf* that is passed to the conf_cb
 *  function.
 *
 * @param rxa_params
 *  Pointer to struct rte_event_eth_rx_adapter_params.
 *  In case of NULL, default values are used.
 *
 * @return
 *   - 0: Success
 *   - <0: Error code on failure
 */
__rte_experimental
int rte_event_eth_rx_adapter_create_with_params(uint8_t id, uint8_t dev_id,
			struct rte_event_port_conf *port_config,
			struct rte_event_eth_rx_adapter_params *rxa_params);

/**
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
 * Start ethernet Rx event adapter
 *
 * @param id
 *  Adapter identifier.
 *
 * @return
 *  - 0: Success, Adapter started correctly.
 *  - <0: Error code on failure.
 *
 * @note
 *  The eventdev to which the event_eth_rx_adapter is connected needs to
 *  be started before calling rte_event_eth_rx_adapter_start().
 */
int rte_event_eth_rx_adapter_start(uint8_t id);

/**
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

/**
 * Register callback to process Rx packets, this is supported for
 * SW based packet transfers.
 * @see rte_event_eth_rx_cb_fn
 *
 * @param id
 *  Adapter identifier.
 * @param eth_dev_id
 *  Port identifier of Ethernet device.
 * @param cb_fn
 *  Callback function.
 * @param cb_arg
 *  Callback arg.
 * @return
 *  - 0: Success
 *  - <0: Error code on failure.
 */
int rte_event_eth_rx_adapter_cb_register(uint8_t id, uint16_t eth_dev_id,
					 rte_event_eth_rx_adapter_cb_fn cb_fn,
					 void *cb_arg);

/**
 * Retrieve vector limits for a given event dev and eth dev pair.
 * @see rte_event_eth_rx_adapter_vector_limits
 *
 * @param dev_id
 *  Event device identifier.
 * @param eth_port_id
 *  Port identifier of the ethernet device.
 * @param [out] limits
 *  A pointer to rte_event_eth_rx_adapter_vector_limits structure that has to
 * be filled.
 *
 * @return
 *  - 0: Success.
 *  - <0: Error code on failure.
 */
int rte_event_eth_rx_adapter_vector_limits_get(
	uint8_t dev_id, uint16_t eth_port_id,
	struct rte_event_eth_rx_adapter_vector_limits *limits);

/**
 * Retrieve Rx queue config information.
 *
 * @param id
 *  Adapter identifier.

 * @param eth_dev_id
 *  Port identifier of Ethernet device.

 * @param rx_queue_id
 *  Ethernet device receive queue index.

 * @param[out] queue_conf
 *  Pointer to struct rte_event_eth_rx_adapter_queue_conf

 * @return
 *  - 0: Success, Receive queue added correctly.
 *  - <0: Error code on failure.
 */
__rte_experimental
int rte_event_eth_rx_adapter_queue_conf_get(uint8_t id,
			uint16_t eth_dev_id,
			uint16_t rx_queue_id,
			struct rte_event_eth_rx_adapter_queue_conf *queue_conf);

/**
 * Retrieve Rx queue statistics.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param eth_dev_id
 *  Port identifier of Ethernet device.
 *
 * @param rx_queue_id
 *  Ethernet device receive queue index.
 *
 * @param[out] stats
 *  Pointer to struct rte_event_eth_rx_adapter_queue_stats
 *
 * @return
 *  - 0: Success, queue buffer stats retrieved.
 *  - <0: Error code on failure.
 */
__rte_experimental
int
rte_event_eth_rx_adapter_queue_stats_get(uint8_t id,
		uint16_t eth_dev_id,
		uint16_t rx_queue_id,
		struct rte_event_eth_rx_adapter_queue_stats *stats);

/**
 * Reset Rx queue statistics.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param eth_dev_id
 *  Port identifier of Ethernet device.
 *
 * @param rx_queue_id
 *  Ethernet device receive queue index.
 *
 * @return
 *  - 0: Success, queue buffer stats retrieved.
 *  - <0: Error code on failure.
 */
__rte_experimental
int
rte_event_eth_rx_adapter_queue_stats_reset(uint8_t id,
		uint16_t eth_dev_id,
		uint16_t rx_queue_id);

#ifdef __cplusplus
}
#endif
#endif	/* _RTE_EVENT_ETH_RX_ADAPTER_ */
