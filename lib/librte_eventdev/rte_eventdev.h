/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc.
 * Copyright(c) 2016-2018 Intel Corporation.
 * Copyright 2016 NXP
 * All rights reserved.
 */

#ifndef _RTE_EVENTDEV_H_
#define _RTE_EVENTDEV_H_

/**
 * @file
 *
 * RTE Event Device API
 *
 * In a polling model, lcores poll ethdev ports and associated rx queues
 * directly to look for packet. In an event driven model, by contrast, lcores
 * call the scheduler that selects packets for them based on programmer
 * specified criteria. Eventdev library adds support for event driven
 * programming model, which offer applications automatic multicore scaling,
 * dynamic load balancing, pipelining, packet ingress order maintenance and
 * synchronization services to simplify application packet processing.
 *
 * The Event Device API is composed of two parts:
 *
 * - The application-oriented Event API that includes functions to setup
 *   an event device (configure it, setup its queues, ports and start it), to
 *   establish the link between queues to port and to receive events, and so on.
 *
 * - The driver-oriented Event API that exports a function allowing
 *   an event poll Mode Driver (PMD) to simultaneously register itself as
 *   an event device driver.
 *
 * Event device components:
 *
 *                     +-----------------+
 *                     | +-------------+ |
 *        +-------+    | |    flow 0   | |
 *        |Packet |    | +-------------+ |
 *        |event  |    | +-------------+ |
 *        |       |    | |    flow 1   | |port_link(port0, queue0)
 *        +-------+    | +-------------+ |     |     +--------+
 *        +-------+    | +-------------+ o-----v-----o        |dequeue +------+
 *        |Crypto |    | |    flow n   | |           | event  +------->|Core 0|
 *        |work   |    | +-------------+ o----+      | port 0 |        |      |
 *        |done ev|    |  event queue 0  |    |      +--------+        +------+
 *        +-------+    +-----------------+    |
 *        +-------+                           |
 *        |Timer  |    +-----------------+    |      +--------+
 *        |expiry |    | +-------------+ |    +------o        |dequeue +------+
 *        |event  |    | |    flow 0   | o-----------o event  +------->|Core 1|
 *        +-------+    | +-------------+ |      +----o port 1 |        |      |
 *       Event enqueue | +-------------+ |      |    +--------+        +------+
 *     o-------------> | |    flow 1   | |      |
 *        enqueue(     | +-------------+ |      |
 *        queue_id,    |                 |      |    +--------+        +------+
 *        flow_id,     | +-------------+ |      |    |        |dequeue |Core 2|
 *        sched_type,  | |    flow n   | o-----------o event  +------->|      |
 *        event_type,  | +-------------+ |      |    | port 2 |        +------+
 *        subev_type,  |  event queue 1  |      |    +--------+
 *        event)       +-----------------+      |    +--------+
 *                                              |    |        |dequeue +------+
 *        +-------+    +-----------------+      |    | event  +------->|Core n|
 *        |Core   |    | +-------------+ o-----------o port n |        |      |
 *        |(SW)   |    | |    flow 0   | |      |    +--------+        +--+---+
 *        |event  |    | +-------------+ |      |                         |
 *        +-------+    | +-------------+ |      |                         |
 *            ^        | |    flow 1   | |      |                         |
 *            |        | +-------------+ o------+                         |
 *            |        | +-------------+ |                                |
 *            |        | |    flow n   | |                                |
 *            |        | +-------------+ |                                |
 *            |        |  event queue n  |                                |
 *            |        +-----------------+                                |
 *            |                                                           |
 *            +-----------------------------------------------------------+
 *
 * Event device: A hardware or software-based event scheduler.
 *
 * Event: A unit of scheduling that encapsulates a packet or other datatype
 * like SW generated event from the CPU, Crypto work completion notification,
 * Timer expiry event notification etc as well as metadata.
 * The metadata includes flow ID, scheduling type, event priority, event_type,
 * sub_event_type etc.
 *
 * Event queue: A queue containing events that are scheduled by the event dev.
 * An event queue contains events of different flows associated with scheduling
 * types, such as atomic, ordered, or parallel.
 *
 * Event port: An application's interface into the event dev for enqueue and
 * dequeue operations. Each event port can be linked with one or more
 * event queues for dequeue operations.
 *
 * By default, all the functions of the Event Device API exported by a PMD
 * are lock-free functions which assume to not be invoked in parallel on
 * different logical cores to work on the same target object. For instance,
 * the dequeue function of a PMD cannot be invoked in parallel on two logical
 * cores to operates on same  event port. Of course, this function
 * can be invoked in parallel by different logical cores on different ports.
 * It is the responsibility of the upper level application to enforce this rule.
 *
 * In all functions of the Event API, the Event device is
 * designated by an integer >= 0 named the device identifier *dev_id*
 *
 * At the Event driver level, Event devices are represented by a generic
 * data structure of type *rte_event_dev*.
 *
 * Event devices are dynamically registered during the PCI/SoC device probing
 * phase performed at EAL initialization time.
 * When an Event device is being probed, a *rte_event_dev* structure and
 * a new device identifier are allocated for that device. Then, the
 * event_dev_init() function supplied by the Event driver matching the probed
 * device is invoked to properly initialize the device.
 *
 * The role of the device init function consists of resetting the hardware or
 * software event driver implementations.
 *
 * If the device init operation is successful, the correspondence between
 * the device identifier assigned to the new device and its associated
 * *rte_event_dev* structure is effectively registered.
 * Otherwise, both the *rte_event_dev* structure and the device identifier are
 * freed.
 *
 * The functions exported by the application Event API to setup a device
 * designated by its device identifier must be invoked in the following order:
 *     - rte_event_dev_configure()
 *     - rte_event_queue_setup()
 *     - rte_event_port_setup()
 *     - rte_event_port_link()
 *     - rte_event_dev_start()
 *
 * Then, the application can invoke, in any order, the functions
 * exported by the Event API to schedule events, dequeue events, enqueue events,
 * change event queue(s) to event port [un]link establishment and so on.
 *
 * Application may use rte_event_[queue/port]_default_conf_get() to get the
 * default configuration to set up an event queue or event port by
 * overriding few default values.
 *
 * If the application wants to change the configuration (i.e. call
 * rte_event_dev_configure(), rte_event_queue_setup(), or
 * rte_event_port_setup()), it must call rte_event_dev_stop() first to stop the
 * device and then do the reconfiguration before calling rte_event_dev_start()
 * again. The schedule, enqueue and dequeue functions should not be invoked
 * when the device is stopped.
 *
 * Finally, an application can close an Event device by invoking the
 * rte_event_dev_close() function.
 *
 * Each function of the application Event API invokes a specific function
 * of the PMD that controls the target device designated by its device
 * identifier.
 *
 * For this purpose, all device-specific functions of an Event driver are
 * supplied through a set of pointers contained in a generic structure of type
 * *event_dev_ops*.
 * The address of the *event_dev_ops* structure is stored in the *rte_event_dev*
 * structure by the device init function of the Event driver, which is
 * invoked during the PCI/SoC device probing phase, as explained earlier.
 *
 * In other words, each function of the Event API simply retrieves the
 * *rte_event_dev* structure associated with the device identifier and
 * performs an indirect invocation of the corresponding driver function
 * supplied in the *event_dev_ops* structure of the *rte_event_dev* structure.
 *
 * For performance reasons, the address of the fast-path functions of the
 * Event driver is not contained in the *event_dev_ops* structure.
 * Instead, they are directly stored at the beginning of the *rte_event_dev*
 * structure to avoid an extra indirect memory access during their invocation.
 *
 * RTE event device drivers do not use interrupts for enqueue or dequeue
 * operation. Instead, Event drivers export Poll-Mode enqueue and dequeue
 * functions to applications.
 *
 * The events are injected to event device through *enqueue* operation by
 * event producers in the system. The typical event producers are ethdev
 * subsystem for generating packet events, CPU(SW) for generating events based
 * on different stages of application processing, cryptodev for generating
 * crypto work completion notification etc
 *
 * The *dequeue* operation gets one or more events from the event ports.
 * The application process the events and send to downstream event queue through
 * rte_event_enqueue_burst() if it is an intermediate stage of event processing,
 * on the final stage, the application may send to different subsystem like
 * ethdev to send the packet/event on the wire using ethdev
 * rte_eth_tx_burst() API.
 *
 * The point at which events are scheduled to ports depends on the device.
 * For hardware devices, scheduling occurs asynchronously without any software
 * intervention. Software schedulers can either be distributed
 * (each worker thread schedules events to its own port) or centralized
 * (a dedicated thread schedules to all ports). Distributed software schedulers
 * perform the scheduling in rte_event_dequeue_burst(), whereas centralized
 * scheduler logic need a dedicated service core for scheduling.
 * The RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED capability flag is not set
 * indicates the device is centralized and thus needs a dedicated scheduling
 * thread that repeatedly calls software specific scheduling function.
 *
 * An event driven worker thread has following typical workflow on fastpath:
 * \code{.c}
 *	while (1) {
 *		rte_event_dequeue_burst(...);
 *		(event processing)
 *		rte_event_enqueue_burst(...);
 *	}
 * \endcode
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_config.h>
#include <rte_memory.h>
#include <rte_errno.h>

struct rte_mbuf; /* we just use mbuf pointers; no need to include rte_mbuf.h */
struct rte_event;

/* Event device capability bitmap flags */
#define RTE_EVENT_DEV_CAP_QUEUE_QOS           (1ULL << 0)
/**< Event scheduling prioritization is based on the priority associated with
 *  each event queue.
 *
 *  @see rte_event_queue_setup()
 */
#define RTE_EVENT_DEV_CAP_EVENT_QOS           (1ULL << 1)
/**< Event scheduling prioritization is based on the priority associated with
 *  each event. Priority of each event is supplied in *rte_event* structure
 *  on each enqueue operation.
 *
 *  @see rte_event_enqueue_burst()
 */
#define RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED   (1ULL << 2)
/**< Event device operates in distributed scheduling mode.
 * In distributed scheduling mode, event scheduling happens in HW or
 * rte_event_dequeue_burst() or the combination of these two.
 * If the flag is not set then eventdev is centralized and thus needs a
 * dedicated service core that acts as a scheduling thread .
 *
 * @see rte_event_dequeue_burst()
 */
#define RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES     (1ULL << 3)
/**< Event device is capable of enqueuing events of any type to any queue.
 * If this capability is not set, the queue only supports events of the
 *  *RTE_SCHED_TYPE_* type that it was created with.
 *
 * @see RTE_SCHED_TYPE_* values
 */
#define RTE_EVENT_DEV_CAP_BURST_MODE          (1ULL << 4)
/**< Event device is capable of operating in burst mode for enqueue(forward,
 * release) and dequeue operation. If this capability is not set, application
 * still uses the rte_event_dequeue_burst() and rte_event_enqueue_burst() but
 * PMD accepts only one event at a time.
 *
 * @see rte_event_dequeue_burst() rte_event_enqueue_burst()
 */
#define RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE    (1ULL << 5)
/**< Event device ports support disabling the implicit release feature, in
 * which the port will release all unreleased events in its dequeue operation.
 * If this capability is set and the port is configured with implicit release
 * disabled, the application is responsible for explicitly releasing events
 * using either the RTE_EVENT_OP_FORWARD or the RTE_EVENT_OP_RELEASE event
 * enqueue operations.
 *
 * @see rte_event_dequeue_burst() rte_event_enqueue_burst()
 */

#define RTE_EVENT_DEV_CAP_NONSEQ_MODE         (1ULL << 6)
/**< Event device is capable of operating in none sequential mode. The path
 * of the event is not necessary to be sequential. Application can change
 * the path of event at runtime. If the flag is not set, then event each event
 * will follow a path from queue 0 to queue 1 to queue 2 etc. If the flag is
 * set, events may be sent to queues in any order. If the flag is not set, the
 * eventdev will return an error when the application enqueues an event for a
 * qid which is not the next in the sequence.
 */

#define RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK   (1ULL << 7)
/**< Event device is capable of configuring the queue/port link at runtime.
 * If the flag is not set, the eventdev queue/port link is only can be
 * configured during  initialization.
 */

#define RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT (1ULL << 8)
/**< Event device is capable of setting up the link between multiple queue
 * with single port. If the flag is not set, the eventdev can only map a
 * single queue to each port or map a single queue to many port.
 */

/* Event device priority levels */
#define RTE_EVENT_DEV_PRIORITY_HIGHEST   0
/**< Highest priority expressed across eventdev subsystem
 * @see rte_event_queue_setup(), rte_event_enqueue_burst()
 * @see rte_event_port_link()
 */
#define RTE_EVENT_DEV_PRIORITY_NORMAL    128
/**< Normal priority expressed across eventdev subsystem
 * @see rte_event_queue_setup(), rte_event_enqueue_burst()
 * @see rte_event_port_link()
 */
#define RTE_EVENT_DEV_PRIORITY_LOWEST    255
/**< Lowest priority expressed across eventdev subsystem
 * @see rte_event_queue_setup(), rte_event_enqueue_burst()
 * @see rte_event_port_link()
 */

/**
 * Get the total number of event devices that have been successfully
 * initialised.
 *
 * @return
 *   The total number of usable event devices.
 */
uint8_t
rte_event_dev_count(void);

/**
 * Get the device identifier for the named event device.
 *
 * @param name
 *   Event device name to select the event device identifier.
 *
 * @return
 *   Returns event device identifier on success.
 *   - <0: Failure to find named event device.
 */
int
rte_event_dev_get_dev_id(const char *name);

/**
 * Return the NUMA socket to which a device is connected.
 *
 * @param dev_id
 *   The identifier of the device.
 * @return
 *   The NUMA socket id to which the device is connected or
 *   a default of zero if the socket could not be determined.
 *   -(-EINVAL)  dev_id value is out of range.
 */
int
rte_event_dev_socket_id(uint8_t dev_id);

/**
 * Event device information
 */
struct rte_event_dev_info {
	const char *driver_name;	/**< Event driver name */
	struct rte_device *dev;	/**< Device information */
	uint32_t min_dequeue_timeout_ns;
	/**< Minimum supported global dequeue timeout(ns) by this device */
	uint32_t max_dequeue_timeout_ns;
	/**< Maximum supported global dequeue timeout(ns) by this device */
	uint32_t dequeue_timeout_ns;
	/**< Configured global dequeue timeout(ns) for this device */
	uint8_t max_event_queues;
	/**< Maximum event_queues supported by this device */
	uint32_t max_event_queue_flows;
	/**< Maximum supported flows in an event queue by this device*/
	uint8_t max_event_queue_priority_levels;
	/**< Maximum number of event queue priority levels by this device.
	 * Valid when the device has RTE_EVENT_DEV_CAP_QUEUE_QOS capability
	 */
	uint8_t max_event_priority_levels;
	/**< Maximum number of event priority levels by this device.
	 * Valid when the device has RTE_EVENT_DEV_CAP_EVENT_QOS capability
	 */
	uint8_t max_event_ports;
	/**< Maximum number of event ports supported by this device */
	uint8_t max_event_port_dequeue_depth;
	/**< Maximum number of events can be dequeued at a time from an
	 * event port by this device.
	 * A device that does not support bulk dequeue will set this as 1.
	 */
	uint32_t max_event_port_enqueue_depth;
	/**< Maximum number of events can be enqueued at a time from an
	 * event port by this device.
	 * A device that does not support bulk enqueue will set this as 1.
	 */
	int32_t max_num_events;
	/**< A *closed system* event dev has a limit on the number of events it
	 * can manage at a time. An *open system* event dev does not have a
	 * limit and will specify this as -1.
	 */
	uint32_t event_dev_cap;
	/**< Event device capabilities(RTE_EVENT_DEV_CAP_)*/
};

/**
 * Retrieve the contextual information of an event device.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param[out] dev_info
 *   A pointer to a structure of type *rte_event_dev_info* to be filled with the
 *   contextual information of the device.
 *
 * @return
 *   - 0: Success, driver updates the contextual information of the event device
 *   - <0: Error code returned by the driver info get function.
 *
 */
int
rte_event_dev_info_get(uint8_t dev_id, struct rte_event_dev_info *dev_info);

/**
 * The count of ports.
 */
#define RTE_EVENT_DEV_ATTR_PORT_COUNT 0
/**
 * The count of queues.
 */
#define RTE_EVENT_DEV_ATTR_QUEUE_COUNT 1
/**
 * The status of the device, zero for stopped, non-zero for started.
 */
#define RTE_EVENT_DEV_ATTR_STARTED 2

/**
 * Get an attribute from a device.
 *
 * @param dev_id Eventdev id
 * @param attr_id The attribute ID to retrieve
 * @param[out] attr_value A pointer that will be filled in with the attribute
 *             value if successful.
 *
 * @return
 *   - 0: Successfully retrieved attribute value
 *   - -EINVAL: Invalid device or  *attr_id* provided, or *attr_value* is NULL
 */
int
rte_event_dev_attr_get(uint8_t dev_id, uint32_t attr_id,
		       uint32_t *attr_value);


/* Event device configuration bitmap flags */
#define RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT (1ULL << 0)
/**< Override the global *dequeue_timeout_ns* and use per dequeue timeout in ns.
 *  @see rte_event_dequeue_timeout_ticks(), rte_event_dequeue_burst()
 */

/** Event device configuration structure */
struct rte_event_dev_config {
	uint32_t dequeue_timeout_ns;
	/**< rte_event_dequeue_burst() timeout on this device.
	 * This value should be in the range of *min_dequeue_timeout_ns* and
	 * *max_dequeue_timeout_ns* which previously provided in
	 * rte_event_dev_info_get()
	 * The value 0 is allowed, in which case, default dequeue timeout used.
	 * @see RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT
	 */
	int32_t nb_events_limit;
	/**< In a *closed system* this field is the limit on maximum number of
	 * events that can be inflight in the eventdev at a given time. The
	 * limit is required to ensure that the finite space in a closed system
	 * is not overwhelmed. The value cannot exceed the *max_num_events*
	 * as provided by rte_event_dev_info_get().
	 * This value should be set to -1 for *open system*.
	 */
	uint8_t nb_event_queues;
	/**< Number of event queues to configure on this device.
	 * This value cannot exceed the *max_event_queues* which previously
	 * provided in rte_event_dev_info_get()
	 */
	uint8_t nb_event_ports;
	/**< Number of event ports to configure on this device.
	 * This value cannot exceed the *max_event_ports* which previously
	 * provided in rte_event_dev_info_get()
	 */
	uint32_t nb_event_queue_flows;
	/**< Number of flows for any event queue on this device.
	 * This value cannot exceed the *max_event_queue_flows* which previously
	 * provided in rte_event_dev_info_get()
	 */
	uint32_t nb_event_port_dequeue_depth;
	/**< Maximum number of events can be dequeued at a time from an
	 * event port by this device.
	 * This value cannot exceed the *max_event_port_dequeue_depth*
	 * which previously provided in rte_event_dev_info_get().
	 * Ignored when device is not RTE_EVENT_DEV_CAP_BURST_MODE capable.
	 * @see rte_event_port_setup()
	 */
	uint32_t nb_event_port_enqueue_depth;
	/**< Maximum number of events can be enqueued at a time from an
	 * event port by this device.
	 * This value cannot exceed the *max_event_port_enqueue_depth*
	 * which previously provided in rte_event_dev_info_get().
	 * Ignored when device is not RTE_EVENT_DEV_CAP_BURST_MODE capable.
	 * @see rte_event_port_setup()
	 */
	uint32_t event_dev_cfg;
	/**< Event device config flags(RTE_EVENT_DEV_CFG_)*/
};

/**
 * Configure an event device.
 *
 * This function must be invoked first before any other function in the
 * API. This function can also be re-invoked when a device is in the
 * stopped state.
 *
 * The caller may use rte_event_dev_info_get() to get the capability of each
 * resources available for this event device.
 *
 * @param dev_id
 *   The identifier of the device to configure.
 * @param dev_conf
 *   The event device configuration structure.
 *
 * @return
 *   - 0: Success, device configured.
 *   - <0: Error code returned by the driver configuration function.
 */
int
rte_event_dev_configure(uint8_t dev_id,
			const struct rte_event_dev_config *dev_conf);


/* Event queue specific APIs */

/* Event queue configuration bitmap flags */
#define RTE_EVENT_QUEUE_CFG_ALL_TYPES          (1ULL << 0)
/**< Allow ATOMIC,ORDERED,PARALLEL schedule type enqueue
 *
 * @see RTE_SCHED_TYPE_ORDERED, RTE_SCHED_TYPE_ATOMIC, RTE_SCHED_TYPE_PARALLEL
 * @see rte_event_enqueue_burst()
 */
#define RTE_EVENT_QUEUE_CFG_SINGLE_LINK        (1ULL << 1)
/**< This event queue links only to a single event port.
 *
 *  @see rte_event_port_setup(), rte_event_port_link()
 */

/** Event queue configuration structure */
struct rte_event_queue_conf {
	uint32_t nb_atomic_flows;
	/**< The maximum number of active flows this queue can track at any
	 * given time. If the queue is configured for atomic scheduling (by
	 * applying the RTE_EVENT_QUEUE_CFG_ALL_TYPES flag to event_queue_cfg
	 * or RTE_SCHED_TYPE_ATOMIC flag to schedule_type), then the
	 * value must be in the range of [1, nb_event_queue_flows], which was
	 * previously provided in rte_event_dev_configure().
	 */
	uint32_t nb_atomic_order_sequences;
	/**< The maximum number of outstanding events waiting to be
	 * reordered by this queue. In other words, the number of entries in
	 * this queue’s reorder buffer.When the number of events in the
	 * reorder buffer reaches to *nb_atomic_order_sequences* then the
	 * scheduler cannot schedule the events from this queue and invalid
	 * event will be returned from dequeue until one or more entries are
	 * freed up/released.
	 * If the queue is configured for ordered scheduling (by applying the
	 * RTE_EVENT_QUEUE_CFG_ALL_TYPES flag to event_queue_cfg or
	 * RTE_SCHED_TYPE_ORDERED flag to schedule_type), then the value must
	 * be in the range of [1, nb_event_queue_flows], which was
	 * previously supplied to rte_event_dev_configure().
	 */
	uint32_t event_queue_cfg;
	/**< Queue cfg flags(EVENT_QUEUE_CFG_) */
	uint8_t schedule_type;
	/**< Queue schedule type(RTE_SCHED_TYPE_*).
	 * Valid when RTE_EVENT_QUEUE_CFG_ALL_TYPES bit is not set in
	 * event_queue_cfg.
	 */
	uint8_t priority;
	/**< Priority for this event queue relative to other event queues.
	 * The requested priority should in the range of
	 * [RTE_EVENT_DEV_PRIORITY_HIGHEST, RTE_EVENT_DEV_PRIORITY_LOWEST].
	 * The implementation shall normalize the requested priority to
	 * event device supported priority value.
	 * Valid when the device has RTE_EVENT_DEV_CAP_QUEUE_QOS capability
	 */
};

/**
 * Retrieve the default configuration information of an event queue designated
 * by its *queue_id* from the event driver for an event device.
 *
 * This function intended to be used in conjunction with rte_event_queue_setup()
 * where caller needs to set up the queue by overriding few default values.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param queue_id
 *   The index of the event queue to get the configuration information.
 *   The value must be in the range [0, nb_event_queues - 1]
 *   previously supplied to rte_event_dev_configure().
 * @param[out] queue_conf
 *   The pointer to the default event queue configuration data.
 * @return
 *   - 0: Success, driver updates the default event queue configuration data.
 *   - <0: Error code returned by the driver info get function.
 *
 * @see rte_event_queue_setup()
 *
 */
int
rte_event_queue_default_conf_get(uint8_t dev_id, uint8_t queue_id,
				 struct rte_event_queue_conf *queue_conf);

/**
 * Allocate and set up an event queue for an event device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param queue_id
 *   The index of the event queue to setup. The value must be in the range
 *   [0, nb_event_queues - 1] previously supplied to rte_event_dev_configure().
 * @param queue_conf
 *   The pointer to the configuration data to be used for the event queue.
 *   NULL value is allowed, in which case default configuration	used.
 *
 * @see rte_event_queue_default_conf_get()
 *
 * @return
 *   - 0: Success, event queue correctly set up.
 *   - <0: event queue configuration failed
 */
int
rte_event_queue_setup(uint8_t dev_id, uint8_t queue_id,
		      const struct rte_event_queue_conf *queue_conf);

/**
 * The priority of the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_PRIORITY 0
/**
 * The number of atomic flows configured for the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_NB_ATOMIC_FLOWS 1
/**
 * The number of atomic order sequences configured for the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_NB_ATOMIC_ORDER_SEQUENCES 2
/**
 * The cfg flags for the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_EVENT_QUEUE_CFG 3
/**
 * The schedule type of the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_SCHEDULE_TYPE 4

/**
 * Get an attribute from a queue.
 *
 * @param dev_id
 *   Eventdev id
 * @param queue_id
 *   Eventdev queue id
 * @param attr_id
 *   The attribute ID to retrieve
 * @param[out] attr_value
 *   A pointer that will be filled in with the attribute value if successful
 *
 * @return
 *   - 0: Successfully returned value
 *   - -EINVAL: invalid device, queue or attr_id provided, or attr_value was
 *		NULL
 *   - -EOVERFLOW: returned when attr_id is set to
 *   RTE_EVENT_QUEUE_ATTR_SCHEDULE_TYPE and event_queue_cfg is set to
 *   RTE_EVENT_QUEUE_CFG_ALL_TYPES
 */
int
rte_event_queue_attr_get(uint8_t dev_id, uint8_t queue_id, uint32_t attr_id,
			uint32_t *attr_value);

/* Event port specific APIs */

/** Event port configuration structure */
struct rte_event_port_conf {
	int32_t new_event_threshold;
	/**< A backpressure threshold for new event enqueues on this port.
	 * Use for *closed system* event dev where event capacity is limited,
	 * and cannot exceed the capacity of the event dev.
	 * Configuring ports with different thresholds can make higher priority
	 * traffic less likely to  be backpressured.
	 * For example, a port used to inject NIC Rx packets into the event dev
	 * can have a lower threshold so as not to overwhelm the device,
	 * while ports used for worker pools can have a higher threshold.
	 * This value cannot exceed the *nb_events_limit*
	 * which was previously supplied to rte_event_dev_configure().
	 * This should be set to '-1' for *open system*.
	 */
	uint16_t dequeue_depth;
	/**< Configure number of bulk dequeues for this event port.
	 * This value cannot exceed the *nb_event_port_dequeue_depth*
	 * which previously supplied to rte_event_dev_configure().
	 * Ignored when device is not RTE_EVENT_DEV_CAP_BURST_MODE capable.
	 */
	uint16_t enqueue_depth;
	/**< Configure number of bulk enqueues for this event port.
	 * This value cannot exceed the *nb_event_port_enqueue_depth*
	 * which previously supplied to rte_event_dev_configure().
	 * Ignored when device is not RTE_EVENT_DEV_CAP_BURST_MODE capable.
	 */
	uint8_t disable_implicit_release;
	/**< Configure the port not to release outstanding events in
	 * rte_event_dev_dequeue_burst(). If true, all events received through
	 * the port must be explicitly released with RTE_EVENT_OP_RELEASE or
	 * RTE_EVENT_OP_FORWARD. Must be false when the device is not
	 * RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE capable.
	 */
};

/**
 * Retrieve the default configuration information of an event port designated
 * by its *port_id* from the event driver for an event device.
 *
 * This function intended to be used in conjunction with rte_event_port_setup()
 * where caller needs to set up the port by overriding few default values.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The index of the event port to get the configuration information.
 *   The value must be in the range [0, nb_event_ports - 1]
 *   previously supplied to rte_event_dev_configure().
 * @param[out] port_conf
 *   The pointer to the default event port configuration data
 * @return
 *   - 0: Success, driver updates the default event port configuration data.
 *   - <0: Error code returned by the driver info get function.
 *
 * @see rte_event_port_setup()
 *
 */
int
rte_event_port_default_conf_get(uint8_t dev_id, uint8_t port_id,
				struct rte_event_port_conf *port_conf);

/**
 * Allocate and set up an event port for an event device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The index of the event port to setup. The value must be in the range
 *   [0, nb_event_ports - 1] previously supplied to rte_event_dev_configure().
 * @param port_conf
 *   The pointer to the configuration data to be used for the queue.
 *   NULL value is allowed, in which case default configuration	used.
 *
 * @see rte_event_port_default_conf_get()
 *
 * @return
 *   - 0: Success, event port correctly set up.
 *   - <0: Port configuration failed
 *   - (-EDQUOT) Quota exceeded(Application tried to link the queue configured
 *   with RTE_EVENT_QUEUE_CFG_SINGLE_LINK to more than one event ports)
 */
int
rte_event_port_setup(uint8_t dev_id, uint8_t port_id,
		     const struct rte_event_port_conf *port_conf);

/**
 * The queue depth of the port on the enqueue side
 */
#define RTE_EVENT_PORT_ATTR_ENQ_DEPTH 0
/**
 * The queue depth of the port on the dequeue side
 */
#define RTE_EVENT_PORT_ATTR_DEQ_DEPTH 1
/**
 * The new event threshold of the port
 */
#define RTE_EVENT_PORT_ATTR_NEW_EVENT_THRESHOLD 2

/**
 * Get an attribute from a port.
 *
 * @param dev_id
 *   Eventdev id
 * @param port_id
 *   Eventdev port id
 * @param attr_id
 *   The attribute ID to retrieve
 * @param[out] attr_value
 *   A pointer that will be filled in with the attribute value if successful
 *
 * @return
 *   - 0: Successfully returned value
 *   - (-EINVAL) Invalid device, port or attr_id, or attr_value was NULL
 */
int
rte_event_port_attr_get(uint8_t dev_id, uint8_t port_id, uint32_t attr_id,
			uint32_t *attr_value);

/**
 * Start an event device.
 *
 * The device start step is the last one and consists of setting the event
 * queues to start accepting the events and schedules to event ports.
 *
 * On success, all basic functions exported by the API (event enqueue,
 * event dequeue and so on) can be invoked.
 *
 * @param dev_id
 *   Event device identifier
 * @return
 *   - 0: Success, device started.
 *   - -ESTALE : Not all ports of the device are configured
 *   - -ENOLINK: Not all queues are linked, which could lead to deadlock.
 */
int
rte_event_dev_start(uint8_t dev_id);

/**
 * Stop an event device.
 *
 * This function causes all queued events to be drained, including those
 * residing in event ports. While draining events out of the device, this
 * function calls the user-provided flush callback (if one was registered) once
 * per event.
 *
 * The device can be restarted with a call to rte_event_dev_start(). Threads
 * that continue to enqueue/dequeue while the device is stopped, or being
 * stopped, will result in undefined behavior. This includes event adapters,
 * which must be stopped prior to stopping the eventdev.
 *
 * @param dev_id
 *   Event device identifier.
 *
 * @see rte_event_dev_stop_flush_callback_register()
 */
void
rte_event_dev_stop(uint8_t dev_id);

typedef void (*eventdev_stop_flush_t)(uint8_t dev_id, struct rte_event event,
		void *arg);
/**< Callback function called during rte_event_dev_stop(), invoked once per
 * flushed event.
 */

/**
 * Registers a callback function to be invoked during rte_event_dev_stop() for
 * each flushed event. This function can be used to properly dispose of queued
 * events, for example events containing memory pointers.
 *
 * The callback function is only registered for the calling process. The
 * callback function must be registered in every process that can call
 * rte_event_dev_stop().
 *
 * To unregister a callback, call this function with a NULL callback pointer.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param callback
 *   Callback function invoked once per flushed event.
 * @param userdata
 *   Argument supplied to callback.
 *
 * @return
 *  - 0 on success.
 *  - -EINVAL if *dev_id* is invalid
 *
 * @see rte_event_dev_stop()
 */
int
rte_event_dev_stop_flush_callback_register(uint8_t dev_id,
		eventdev_stop_flush_t callback, void *userdata);

/**
 * Close an event device. The device cannot be restarted!
 *
 * @param dev_id
 *   Event device identifier
 *
 * @return
 *  - 0 on successfully closing device
 *  - <0 on failure to close device
 *  - (-EAGAIN) if device is busy
 */
int
rte_event_dev_close(uint8_t dev_id);

/* Scheduler type definitions */
#define RTE_SCHED_TYPE_ORDERED          0
/**< Ordered scheduling
 *
 * Events from an ordered flow of an event queue can be scheduled to multiple
 * ports for concurrent processing while maintaining the original event order.
 * This scheme enables the user to achieve high single flow throughput by
 * avoiding SW synchronization for ordering between ports which bound to cores.
 *
 * The source flow ordering from an event queue is maintained when events are
 * enqueued to their destination queue within the same ordered flow context.
 * An event port holds the context until application call
 * rte_event_dequeue_burst() from the same port, which implicitly releases
 * the context.
 * User may allow the scheduler to release the context earlier than that
 * by invoking rte_event_enqueue_burst() with RTE_EVENT_OP_RELEASE operation.
 *
 * Events from the source queue appear in their original order when dequeued
 * from a destination queue.
 * Event ordering is based on the received event(s), but also other
 * (newly allocated or stored) events are ordered when enqueued within the same
 * ordered context. Events not enqueued (e.g. released or stored) within the
 * context are  considered missing from reordering and are skipped at this time
 * (but can be ordered again within another context).
 *
 * @see rte_event_queue_setup(), rte_event_dequeue_burst(), RTE_EVENT_OP_RELEASE
 */

#define RTE_SCHED_TYPE_ATOMIC           1
/**< Atomic scheduling
 *
 * Events from an atomic flow of an event queue can be scheduled only to a
 * single port at a time. The port is guaranteed to have exclusive (atomic)
 * access to the associated flow context, which enables the user to avoid SW
 * synchronization. Atomic flows also help to maintain event ordering
 * since only one port at a time can process events from a flow of an
 * event queue.
 *
 * The atomic queue synchronization context is dedicated to the port until
 * application call rte_event_dequeue_burst() from the same port,
 * which implicitly releases the context. User may allow the scheduler to
 * release the context earlier than that by invoking rte_event_enqueue_burst()
 * with RTE_EVENT_OP_RELEASE operation.
 *
 * @see rte_event_queue_setup(), rte_event_dequeue_burst(), RTE_EVENT_OP_RELEASE
 */

#define RTE_SCHED_TYPE_PARALLEL         2
/**< Parallel scheduling
 *
 * The scheduler performs priority scheduling, load balancing, etc. functions
 * but does not provide additional event synchronization or ordering.
 * It is free to schedule events from a single parallel flow of an event queue
 * to multiple events ports for concurrent processing.
 * The application is responsible for flow context synchronization and
 * event ordering (SW synchronization).
 *
 * @see rte_event_queue_setup(), rte_event_dequeue_burst()
 */

/* Event types to classify the event source */
#define RTE_EVENT_TYPE_ETHDEV           0x0
/**< The event generated from ethdev subsystem */
#define RTE_EVENT_TYPE_CRYPTODEV        0x1
/**< The event generated from crypodev subsystem */
#define RTE_EVENT_TYPE_TIMER		0x2
/**< The event generated from event timer adapter */
#define RTE_EVENT_TYPE_CPU              0x3
/**< The event generated from cpu for pipelining.
 * Application may use *sub_event_type* to further classify the event
 */
#define RTE_EVENT_TYPE_ETH_RX_ADAPTER   0x4
/**< The event generated from event eth Rx adapter */
#define RTE_EVENT_TYPE_MAX              0x10
/**< Maximum number of event types */

/* Event enqueue operations */
#define RTE_EVENT_OP_NEW                0
/**< The event producers use this operation to inject a new event to the
 * event device.
 */
#define RTE_EVENT_OP_FORWARD            1
/**< The CPU use this operation to forward the event to different event queue or
 * change to new application specific flow or schedule type to enable
 * pipelining.
 *
 * This operation must only be enqueued to the same port that the
 * event to be forwarded was dequeued from.
 */
#define RTE_EVENT_OP_RELEASE            2
/**< Release the flow context associated with the schedule type.
 *
 * If current flow's scheduler type method is *RTE_SCHED_TYPE_ATOMIC*
 * then this function hints the scheduler that the user has completed critical
 * section processing in the current atomic context.
 * The scheduler is now allowed to schedule events from the same flow from
 * an event queue to another port. However, the context may be still held
 * until the next rte_event_dequeue_burst() call, this call allows but does not
 * force the scheduler to release the context early.
 *
 * Early atomic context release may increase parallelism and thus system
 * performance, but the user needs to design carefully the split into critical
 * vs non-critical sections.
 *
 * If current flow's scheduler type method is *RTE_SCHED_TYPE_ORDERED*
 * then this function hints the scheduler that the user has done all that need
 * to maintain event order in the current ordered context.
 * The scheduler is allowed to release the ordered context of this port and
 * avoid reordering any following enqueues.
 *
 * Early ordered context release may increase parallelism and thus system
 * performance.
 *
 * If current flow's scheduler type method is *RTE_SCHED_TYPE_PARALLEL*
 * or no scheduling context is held then this function may be an NOOP,
 * depending on the implementation.
 *
 * This operation must only be enqueued to the same port that the
 * event to be released was dequeued from.
 *
 */

/**
 * The generic *rte_event* structure to hold the event attributes
 * for dequeue and enqueue operation
 */
RTE_STD_C11
struct rte_event {
	/** WORD0 */
	union {
		uint64_t event;
		/** Event attributes for dequeue or enqueue operation */
		struct {
			uint32_t flow_id:20;
			/**< Targeted flow identifier for the enqueue and
			 * dequeue operation.
			 * The value must be in the range of
			 * [0, nb_event_queue_flows - 1] which
			 * previously supplied to rte_event_dev_configure().
			 */
			uint32_t sub_event_type:8;
			/**< Sub-event types based on the event source.
			 * @see RTE_EVENT_TYPE_CPU
			 */
			uint32_t event_type:4;
			/**< Event type to classify the event source.
			 * @see RTE_EVENT_TYPE_ETHDEV, (RTE_EVENT_TYPE_*)
			 */
			uint8_t op:2;
			/**< The type of event enqueue operation - new/forward/
			 * etc.This field is not preserved across an instance
			 * and is undefined on dequeue.
			 * @see RTE_EVENT_OP_NEW, (RTE_EVENT_OP_*)
			 */
			uint8_t rsvd:4;
			/**< Reserved for future use */
			uint8_t sched_type:2;
			/**< Scheduler synchronization type (RTE_SCHED_TYPE_*)
			 * associated with flow id on a given event queue
			 * for the enqueue and dequeue operation.
			 */
			uint8_t queue_id;
			/**< Targeted event queue identifier for the enqueue or
			 * dequeue operation.
			 * The value must be in the range of
			 * [0, nb_event_queues - 1] which previously supplied to
			 * rte_event_dev_configure().
			 */
			uint8_t priority;
			/**< Event priority relative to other events in the
			 * event queue. The requested priority should in the
			 * range of  [RTE_EVENT_DEV_PRIORITY_HIGHEST,
			 * RTE_EVENT_DEV_PRIORITY_LOWEST].
			 * The implementation shall normalize the requested
			 * priority to supported priority value.
			 * Valid when the device has
			 * RTE_EVENT_DEV_CAP_EVENT_QOS capability.
			 */
			uint8_t impl_opaque;
			/**< Implementation specific opaque value.
			 * An implementation may use this field to hold
			 * implementation specific value to share between
			 * dequeue and enqueue operation.
			 * The application should not modify this field.
			 */
		};
	};
	/** WORD1 */
	union {
		uint64_t u64;
		/**< Opaque 64-bit value */
		void *event_ptr;
		/**< Opaque event pointer */
		struct rte_mbuf *mbuf;
		/**< mbuf pointer if dequeued event is associated with mbuf */
	};
};

/* Ethdev Rx adapter capability bitmap flags */
#define RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT	0x1
/**< This flag is sent when the packet transfer mechanism is in HW.
 * Ethdev can send packets to the event device using internal event port.
 */
#define RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ	0x2
/**< Adapter supports multiple event queues per ethdev. Every ethdev
 * Rx queue can be connected to a unique event queue.
 */
#define RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID	0x4
/**< The application can override the adapter generated flow ID in the
 * event. This flow ID can be specified when adding an ethdev Rx queue
 * to the adapter using the ev member of struct rte_event_eth_rx_adapter
 * @see struct rte_event_eth_rx_adapter_queue_conf::ev
 * @see struct rte_event_eth_rx_adapter_queue_conf::rx_queue_flags
 */

/**
 * Retrieve the event device's ethdev Rx adapter capabilities for the
 * specified ethernet port
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param eth_port_id
 *   The identifier of the ethernet device.
 *
 * @param[out] caps
 *   A pointer to memory filled with Rx event adapter capabilities.
 *
 * @return
 *   - 0: Success, driver provides Rx event adapter capabilities for the
 *	ethernet device.
 *   - <0: Error code returned by the driver function.
 *
 */
int
rte_event_eth_rx_adapter_caps_get(uint8_t dev_id, uint8_t eth_port_id,
				uint32_t *caps);

#define RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT (1ULL << 0)
/**< This flag is set when the timer mechanism is in HW. */

/**
 * Retrieve the event device's timer adapter capabilities.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param[out] caps
 *   A pointer to memory to be filled with event timer adapter capabilities.
 *
 * @return
 *   - 0: Success, driver provided event timer adapter capabilities.
 *   - <0: Error code returned by the driver function.
 */
int __rte_experimental
rte_event_timer_adapter_caps_get(uint8_t dev_id, uint32_t *caps);

/* Crypto adapter capability bitmap flag */
#define RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW   0x1
/**< Flag indicates HW is capable of generating events in
 * RTE_EVENT_OP_NEW enqueue operation. Cryptodev will send
 * packets to the event device as new events using an internal
 * event port.
 */

#define RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD   0x2
/**< Flag indicates HW is capable of generating events in
 * RTE_EVENT_OP_FORWARD enqueue operation. Cryptodev will send
 * packets to the event device as forwarded event using an
 * internal event port.
 */

#define RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND  0x4
/**< Flag indicates HW is capable of mapping crypto queue pair to
 * event queue.
 */

#define RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA   0x8
/**< Flag indicates HW/SW suports a mechanism to store and retrieve
 * the private data information along with the crypto session.
 */

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Retrieve the event device's crypto adapter capabilities for the
 * specified cryptodev device
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param cdev_id
 *   The identifier of the cryptodev device.
 *
 * @param[out] caps
 *   A pointer to memory filled with event adapter capabilities.
 *   It is expected to be pre-allocated & initialized by caller.
 *
 * @return
 *   - 0: Success, driver provides event adapter capabilities for the
 *     cryptodev device.
 *   - <0: Error code returned by the driver function.
 *
 */
int __rte_experimental
rte_event_crypto_adapter_caps_get(uint8_t dev_id, uint8_t cdev_id,
				  uint32_t *caps);

struct rte_eventdev_ops;
struct rte_eventdev;

typedef uint16_t (*event_enqueue_t)(void *port, const struct rte_event *ev);
/**< @internal Enqueue event on port of a device */

typedef uint16_t (*event_enqueue_burst_t)(void *port,
			const struct rte_event ev[], uint16_t nb_events);
/**< @internal Enqueue burst of events on port of a device */

typedef uint16_t (*event_dequeue_t)(void *port, struct rte_event *ev,
		uint64_t timeout_ticks);
/**< @internal Dequeue event from port of a device */

typedef uint16_t (*event_dequeue_burst_t)(void *port, struct rte_event ev[],
		uint16_t nb_events, uint64_t timeout_ticks);
/**< @internal Dequeue burst of events from port of a device */

#define RTE_EVENTDEV_NAME_MAX_LEN	(64)
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
	void **ports;
	/**< Array of pointers to ports. */
	struct rte_event_port_conf *ports_cfg;
	/**< Array of port configuration structures. */
	struct rte_event_queue_conf *queues_cfg;
	/**< Array of queue configuration structures. */
	uint16_t *links_map;
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

	struct rte_eventdev_data *data;
	/**< Pointer to device data */
	struct rte_eventdev_ops *dev_ops;
	/**< Functions exported by PMD */
	struct rte_device *dev;
	/**< Device info. supplied by probing */

	RTE_STD_C11
	uint8_t attached : 1;
	/**< Flag indicating the device is attached */
} __rte_cache_aligned;

extern struct rte_eventdev *rte_eventdevs;
/** @internal The pool of rte_eventdev structures. */

static __rte_always_inline uint16_t
__rte_event_enqueue_burst(uint8_t dev_id, uint8_t port_id,
			const struct rte_event ev[], uint16_t nb_events,
			const event_enqueue_burst_t fn)
{
	const struct rte_eventdev *dev = &rte_eventdevs[dev_id];

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	if (dev_id >= RTE_EVENT_MAX_DEVS || !rte_eventdevs[dev_id].attached) {
		rte_errno = -EINVAL;
		return 0;
	}

	if (port_id >= dev->data->nb_ports) {
		rte_errno = -EINVAL;
		return 0;
	}
#endif
	/*
	 * Allow zero cost non burst mode routine invocation if application
	 * requests nb_events as const one
	 */
	if (nb_events == 1)
		return (*dev->enqueue)(dev->data->ports[port_id], ev);
	else
		return fn(dev->data->ports[port_id], ev, nb_events);
}

/**
 * Enqueue a burst of events objects or an event object supplied in *rte_event*
 * structure on an  event device designated by its *dev_id* through the event
 * port specified by *port_id*. Each event object specifies the event queue on
 * which it will be enqueued.
 *
 * The *nb_events* parameter is the number of event objects to enqueue which are
 * supplied in the *ev* array of *rte_event* structure.
 *
 * Event operations RTE_EVENT_OP_FORWARD and RTE_EVENT_OP_RELEASE must only be
 * enqueued to the same port that their associated events were dequeued from.
 *
 * The rte_event_enqueue_burst() function returns the number of
 * events objects it actually enqueued. A return value equal to *nb_events*
 * means that all event objects have been enqueued.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The identifier of the event port.
 * @param ev
 *   Points to an array of *nb_events* objects of type *rte_event* structure
 *   which contain the event object enqueue operations to be processed.
 * @param nb_events
 *   The number of event objects to enqueue, typically number of
 *   rte_event_port_enqueue_depth() available for this port.
 *
 * @return
 *   The number of event objects actually enqueued on the event device. The
 *   return value can be less than the value of the *nb_events* parameter when
 *   the event devices queue is full or if invalid parameters are specified in a
 *   *rte_event*. If the return value is less than *nb_events*, the remaining
 *   events at the end of ev[] are not consumed and the caller has to take care
 *   of them, and rte_errno is set accordingly. Possible errno values include:
 *   - -EINVAL  The port ID is invalid, device ID is invalid, an event's queue
 *              ID is invalid, or an event's sched type doesn't match the
 *              capabilities of the destination queue.
 *   - -ENOSPC  The event port was backpressured and unable to enqueue
 *              one or more events. This error code is only applicable to
 *              closed systems.
 * @see rte_event_port_enqueue_depth()
 */
static inline uint16_t
rte_event_enqueue_burst(uint8_t dev_id, uint8_t port_id,
			const struct rte_event ev[], uint16_t nb_events)
{
	const struct rte_eventdev *dev = &rte_eventdevs[dev_id];

	return __rte_event_enqueue_burst(dev_id, port_id, ev, nb_events,
			dev->enqueue_burst);
}

/**
 * Enqueue a burst of events objects of operation type *RTE_EVENT_OP_NEW* on
 * an event device designated by its *dev_id* through the event port specified
 * by *port_id*.
 *
 * Provides the same functionality as rte_event_enqueue_burst(), expect that
 * application can use this API when the all objects in the burst contains
 * the enqueue operation of the type *RTE_EVENT_OP_NEW*. This specialized
 * function can provide the additional hint to the PMD and optimize if possible.
 *
 * The rte_event_enqueue_new_burst() result is undefined if the enqueue burst
 * has event object of operation type != RTE_EVENT_OP_NEW.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The identifier of the event port.
 * @param ev
 *   Points to an array of *nb_events* objects of type *rte_event* structure
 *   which contain the event object enqueue operations to be processed.
 * @param nb_events
 *   The number of event objects to enqueue, typically number of
 *   rte_event_port_enqueue_depth() available for this port.
 *
 * @return
 *   The number of event objects actually enqueued on the event device. The
 *   return value can be less than the value of the *nb_events* parameter when
 *   the event devices queue is full or if invalid parameters are specified in a
 *   *rte_event*. If the return value is less than *nb_events*, the remaining
 *   events at the end of ev[] are not consumed and the caller has to take care
 *   of them, and rte_errno is set accordingly. Possible errno values include:
 *   - -EINVAL  The port ID is invalid, device ID is invalid, an event's queue
 *              ID is invalid, or an event's sched type doesn't match the
 *              capabilities of the destination queue.
 *   - -ENOSPC  The event port was backpressured and unable to enqueue
 *              one or more events. This error code is only applicable to
 *              closed systems.
 * @see rte_event_port_enqueue_depth() rte_event_enqueue_burst()
 */
static inline uint16_t
rte_event_enqueue_new_burst(uint8_t dev_id, uint8_t port_id,
			const struct rte_event ev[], uint16_t nb_events)
{
	const struct rte_eventdev *dev = &rte_eventdevs[dev_id];

	return __rte_event_enqueue_burst(dev_id, port_id, ev, nb_events,
			dev->enqueue_new_burst);
}

/**
 * Enqueue a burst of events objects of operation type *RTE_EVENT_OP_FORWARD*
 * on an event device designated by its *dev_id* through the event port
 * specified by *port_id*.
 *
 * Provides the same functionality as rte_event_enqueue_burst(), expect that
 * application can use this API when the all objects in the burst contains
 * the enqueue operation of the type *RTE_EVENT_OP_FORWARD*. This specialized
 * function can provide the additional hint to the PMD and optimize if possible.
 *
 * The rte_event_enqueue_new_burst() result is undefined if the enqueue burst
 * has event object of operation type != RTE_EVENT_OP_FORWARD.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The identifier of the event port.
 * @param ev
 *   Points to an array of *nb_events* objects of type *rte_event* structure
 *   which contain the event object enqueue operations to be processed.
 * @param nb_events
 *   The number of event objects to enqueue, typically number of
 *   rte_event_port_enqueue_depth() available for this port.
 *
 * @return
 *   The number of event objects actually enqueued on the event device. The
 *   return value can be less than the value of the *nb_events* parameter when
 *   the event devices queue is full or if invalid parameters are specified in a
 *   *rte_event*. If the return value is less than *nb_events*, the remaining
 *   events at the end of ev[] are not consumed and the caller has to take care
 *   of them, and rte_errno is set accordingly. Possible errno values include:
 *   - -EINVAL  The port ID is invalid, device ID is invalid, an event's queue
 *              ID is invalid, or an event's sched type doesn't match the
 *              capabilities of the destination queue.
 *   - -ENOSPC  The event port was backpressured and unable to enqueue
 *              one or more events. This error code is only applicable to
 *              closed systems.
 * @see rte_event_port_enqueue_depth() rte_event_enqueue_burst()
 */
static inline uint16_t
rte_event_enqueue_forward_burst(uint8_t dev_id, uint8_t port_id,
			const struct rte_event ev[], uint16_t nb_events)
{
	const struct rte_eventdev *dev = &rte_eventdevs[dev_id];

	return __rte_event_enqueue_burst(dev_id, port_id, ev, nb_events,
			dev->enqueue_forward_burst);
}

/**
 * Converts nanoseconds to *timeout_ticks* value for rte_event_dequeue_burst()
 *
 * If the device is configured with RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT flag
 * then application can use this function to convert timeout value in
 * nanoseconds to implementations specific timeout value supplied in
 * rte_event_dequeue_burst()
 *
 * @param dev_id
 *   The identifier of the device.
 * @param ns
 *   Wait time in nanosecond
 * @param[out] timeout_ticks
 *   Value for the *timeout_ticks* parameter in rte_event_dequeue_burst()
 *
 * @return
 *  - 0 on success.
 *  - -ENOTSUP if the device doesn't support timeouts
 *  - -EINVAL if *dev_id* is invalid or *timeout_ticks* is NULL
 *  - other values < 0 on failure.
 *
 * @see rte_event_dequeue_burst(), RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT
 * @see rte_event_dev_configure()
 *
 */
int
rte_event_dequeue_timeout_ticks(uint8_t dev_id, uint64_t ns,
					uint64_t *timeout_ticks);

/**
 * Dequeue a burst of events objects or an event object from the event port
 * designated by its *event_port_id*, on an event device designated
 * by its *dev_id*.
 *
 * rte_event_dequeue_burst() does not dictate the specifics of scheduling
 * algorithm as each eventdev driver may have different criteria to schedule
 * an event. However, in general, from an application perspective scheduler may
 * use the following scheme to dispatch an event to the port.
 *
 * 1) Selection of event queue based on
 *   a) The list of event queues are linked to the event port.
 *   b) If the device has RTE_EVENT_DEV_CAP_QUEUE_QOS capability then event
 *   queue selection from list is based on event queue priority relative to
 *   other event queue supplied as *priority* in rte_event_queue_setup()
 *   c) If the device has RTE_EVENT_DEV_CAP_EVENT_QOS capability then event
 *   queue selection from the list is based on event priority supplied as
 *   *priority* in rte_event_enqueue_burst()
 * 2) Selection of event
 *   a) The number of flows available in selected event queue.
 *   b) Schedule type method associated with the event
 *
 * The *nb_events* parameter is the maximum number of event objects to dequeue
 * which are returned in the *ev* array of *rte_event* structure.
 *
 * The rte_event_dequeue_burst() function returns the number of events objects
 * it actually dequeued. A return value equal to *nb_events* means that all
 * event objects have been dequeued.
 *
 * The number of events dequeued is the number of scheduler contexts held by
 * this port. These contexts are automatically released in the next
 * rte_event_dequeue_burst() invocation if the port supports implicit
 * releases, or invoking rte_event_enqueue_burst() with RTE_EVENT_OP_RELEASE
 * operation can be used to release the contexts early.
 *
 * Event operations RTE_EVENT_OP_FORWARD and RTE_EVENT_OP_RELEASE must only be
 * enqueued to the same port that their associated events were dequeued from.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The identifier of the event port.
 * @param[out] ev
 *   Points to an array of *nb_events* objects of type *rte_event* structure
 *   for output to be populated with the dequeued event objects.
 * @param nb_events
 *   The maximum number of event objects to dequeue, typically number of
 *   rte_event_port_dequeue_depth() available for this port.
 *
 * @param timeout_ticks
 *   - 0 no-wait, returns immediately if there is no event.
 *   - >0 wait for the event, if the device is configured with
 *   RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT then this function will wait until
 *   at least one event is available or *timeout_ticks* time.
 *   if the device is not configured with RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT
 *   then this function will wait until the event available or
 *   *dequeue_timeout_ns* ns which was previously supplied to
 *   rte_event_dev_configure()
 *
 * @return
 * The number of event objects actually dequeued from the port. The return
 * value can be less than the value of the *nb_events* parameter when the
 * event port's queue is not full.
 *
 * @see rte_event_port_dequeue_depth()
 */
static inline uint16_t
rte_event_dequeue_burst(uint8_t dev_id, uint8_t port_id, struct rte_event ev[],
			uint16_t nb_events, uint64_t timeout_ticks)
{
	struct rte_eventdev *dev = &rte_eventdevs[dev_id];

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	if (dev_id >= RTE_EVENT_MAX_DEVS || !rte_eventdevs[dev_id].attached) {
		rte_errno = -EINVAL;
		return 0;
	}

	if (port_id >= dev->data->nb_ports) {
		rte_errno = -EINVAL;
		return 0;
	}
#endif

	/*
	 * Allow zero cost non burst mode routine invocation if application
	 * requests nb_events as const one
	 */
	if (nb_events == 1)
		return (*dev->dequeue)(
			dev->data->ports[port_id], ev, timeout_ticks);
	else
		return (*dev->dequeue_burst)(
			dev->data->ports[port_id], ev, nb_events,
				timeout_ticks);
}

/**
 * Link multiple source event queues supplied in *queues* to the destination
 * event port designated by its *port_id* with associated service priority
 * supplied in *priorities* on the event device designated by its *dev_id*.
 *
 * The link establishment shall enable the event port *port_id* from
 * receiving events from the specified event queue(s) supplied in *queues*
 *
 * An event queue may link to one or more event ports.
 * The number of links can be established from an event queue to event port is
 * implementation defined.
 *
 * Event queue(s) to event port link establishment can be changed at runtime
 * without re-configuring the device to support scaling and to reduce the
 * latency of critical work by establishing the link with more event ports
 * at runtime.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param port_id
 *   Event port identifier to select the destination port to link.
 *
 * @param queues
 *   Points to an array of *nb_links* event queues to be linked
 *   to the event port.
 *   NULL value is allowed, in which case this function links all the configured
 *   event queues *nb_event_queues* which previously supplied to
 *   rte_event_dev_configure() to the event port *port_id*
 *
 * @param priorities
 *   Points to an array of *nb_links* service priorities associated with each
 *   event queue link to event port.
 *   The priority defines the event port's servicing priority for
 *   event queue, which may be ignored by an implementation.
 *   The requested priority should in the range of
 *   [RTE_EVENT_DEV_PRIORITY_HIGHEST, RTE_EVENT_DEV_PRIORITY_LOWEST].
 *   The implementation shall normalize the requested priority to
 *   implementation supported priority value.
 *   NULL value is allowed, in which case this function links the event queues
 *   with RTE_EVENT_DEV_PRIORITY_NORMAL servicing priority
 *
 * @param nb_links
 *   The number of links to establish. This parameter is ignored if queues is
 *   NULL.
 *
 * @return
 * The number of links actually established. The return value can be less than
 * the value of the *nb_links* parameter when the implementation has the
 * limitation on specific queue to port link establishment or if invalid
 * parameters are specified in *queues*
 * If the return value is less than *nb_links*, the remaining links at the end
 * of link[] are not established, and the caller has to take care of them.
 * If return value is less than *nb_links* then implementation shall update the
 * rte_errno accordingly, Possible rte_errno values are
 * (-EDQUOT) Quota exceeded(Application tried to link the queue configured with
 *  RTE_EVENT_QUEUE_CFG_SINGLE_LINK to more than one event ports)
 * (-EINVAL) Invalid parameter
 *
 */
int
rte_event_port_link(uint8_t dev_id, uint8_t port_id,
		    const uint8_t queues[], const uint8_t priorities[],
		    uint16_t nb_links);

/**
 * Unlink multiple source event queues supplied in *queues* from the destination
 * event port designated by its *port_id* on the event device designated
 * by its *dev_id*.
 *
 * The unlink establishment shall disable the event port *port_id* from
 * receiving events from the specified event queue *queue_id*
 *
 * Event queue(s) to event port unlink establishment can be changed at runtime
 * without re-configuring the device.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param port_id
 *   Event port identifier to select the destination port to unlink.
 *
 * @param queues
 *   Points to an array of *nb_unlinks* event queues to be unlinked
 *   from the event port.
 *   NULL value is allowed, in which case this function unlinks all the
 *   event queue(s) from the event port *port_id*.
 *
 * @param nb_unlinks
 *   The number of unlinks to establish. This parameter is ignored if queues is
 *   NULL.
 *
 * @return
 * The number of unlinks actually established. The return value can be less
 * than the value of the *nb_unlinks* parameter when the implementation has the
 * limitation on specific queue to port unlink establishment or
 * if invalid parameters are specified.
 * If the return value is less than *nb_unlinks*, the remaining queues at the
 * end of queues[] are not established, and the caller has to take care of them.
 * If return value is less than *nb_unlinks* then implementation shall update
 * the rte_errno accordingly, Possible rte_errno values are
 * (-EINVAL) Invalid parameter
 *
 */
int
rte_event_port_unlink(uint8_t dev_id, uint8_t port_id,
		      uint8_t queues[], uint16_t nb_unlinks);

/**
 * Retrieve the list of source event queues and its associated service priority
 * linked to the destination event port designated by its *port_id*
 * on the event device designated by its *dev_id*.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param port_id
 *   Event port identifier.
 *
 * @param[out] queues
 *   Points to an array of *queues* for output.
 *   The caller has to allocate *RTE_EVENT_MAX_QUEUES_PER_DEV* bytes to
 *   store the event queue(s) linked with event port *port_id*
 *
 * @param[out] priorities
 *   Points to an array of *priorities* for output.
 *   The caller has to allocate *RTE_EVENT_MAX_QUEUES_PER_DEV* bytes to
 *   store the service priority associated with each event queue linked
 *
 * @return
 * The number of links established on the event port designated by its
 *  *port_id*.
 * - <0 on failure.
 *
 */
int
rte_event_port_links_get(uint8_t dev_id, uint8_t port_id,
			 uint8_t queues[], uint8_t priorities[]);

/**
 * Retrieve the service ID of the event dev. If the adapter doesn't use
 * a rte_service function, this function returns -ESRCH.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param [out] service_id
 *   A pointer to a uint32_t, to be filled in with the service id.
 *
 * @return
 *   - 0: Success
 *   - <0: Error code on failure, if the event dev doesn't use a rte_service
 *   function, this function returns -ESRCH.
 */
int
rte_event_dev_service_id_get(uint8_t dev_id, uint32_t *service_id);

/**
 * Dump internal information about *dev_id* to the FILE* provided in *f*.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param f
 *   A pointer to a file for output
 *
 * @return
 *   - 0: on success
 *   - <0: on failure.
 */
int
rte_event_dev_dump(uint8_t dev_id, FILE *f);

/** Maximum name length for extended statistics counters */
#define RTE_EVENT_DEV_XSTATS_NAME_SIZE 64

/**
 * Selects the component of the eventdev to retrieve statistics from.
 */
enum rte_event_dev_xstats_mode {
	RTE_EVENT_DEV_XSTATS_DEVICE,
	RTE_EVENT_DEV_XSTATS_PORT,
	RTE_EVENT_DEV_XSTATS_QUEUE,
};

/**
 * A name-key lookup element for extended statistics.
 *
 * This structure is used to map between names and ID numbers
 * for extended ethdev statistics.
 */
struct rte_event_dev_xstats_name {
	char name[RTE_EVENT_DEV_XSTATS_NAME_SIZE];
};

/**
 * Retrieve names of extended statistics of an event device.
 *
 * @param dev_id
 *   The identifier of the event device.
 * @param mode
 *   The mode of statistics to retrieve. Choices include the device statistics,
 *   port statistics or queue statistics.
 * @param queue_port_id
 *   Used to specify the port or queue number in queue or port mode, and is
 *   ignored in device mode.
 * @param[out] xstats_names
 *   Block of memory to insert names into. Must be at least size in capacity.
 *   If set to NULL, function returns required capacity.
 * @param[out] ids
 *   Block of memory to insert ids into. Must be at least size in capacity.
 *   If set to NULL, function returns required capacity. The id values returned
 *   can be passed to *rte_event_dev_xstats_get* to select statistics.
 * @param size
 *   Capacity of xstats_names (number of names).
 * @return
 *   - positive value lower or equal to size: success. The return value
 *     is the number of entries filled in the stats table.
 *   - positive value higher than size: error, the given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 *   - negative value on error:
 *        -ENODEV for invalid *dev_id*
 *        -EINVAL for invalid mode, queue port or id parameters
 *        -ENOTSUP if the device doesn't support this function.
 */
int
rte_event_dev_xstats_names_get(uint8_t dev_id,
			       enum rte_event_dev_xstats_mode mode,
			       uint8_t queue_port_id,
			       struct rte_event_dev_xstats_name *xstats_names,
			       unsigned int *ids,
			       unsigned int size);

/**
 * Retrieve extended statistics of an event device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param mode
 *  The mode of statistics to retrieve. Choices include the device statistics,
 *  port statistics or queue statistics.
 * @param queue_port_id
 *   Used to specify the port or queue number in queue or port mode, and is
 *   ignored in device mode.
 * @param ids
 *   The id numbers of the stats to get. The ids can be got from the stat
 *   position in the stat list from rte_event_dev_get_xstats_names(), or
 *   by using rte_eventdev_get_xstats_by_name()
 * @param[out] values
 *   The values for each stats request by ID.
 * @param n
 *   The number of stats requested
 * @return
 *   - positive value: number of stat entries filled into the values array
 *   - negative value on error:
 *        -ENODEV for invalid *dev_id*
 *        -EINVAL for invalid mode, queue port or id parameters
 *        -ENOTSUP if the device doesn't support this function.
 */
int
rte_event_dev_xstats_get(uint8_t dev_id,
			 enum rte_event_dev_xstats_mode mode,
			 uint8_t queue_port_id,
			 const unsigned int ids[],
			 uint64_t values[], unsigned int n);

/**
 * Retrieve the value of a single stat by requesting it by name.
 *
 * @param dev_id
 *   The identifier of the device
 * @param name
 *   The stat name to retrieve
 * @param[out] id
 *   If non-NULL, the numerical id of the stat will be returned, so that further
 *   requests for the stat can be got using rte_eventdev_xstats_get, which will
 *   be faster as it doesn't need to scan a list of names for the stat.
 *   If the stat cannot be found, the id returned will be (unsigned)-1.
 * @return
 *   - positive value or zero: the stat value
 *   - negative value: -EINVAL if stat not found, -ENOTSUP if not supported.
 */
uint64_t
rte_event_dev_xstats_by_name_get(uint8_t dev_id, const char *name,
				 unsigned int *id);

/**
 * Reset the values of the xstats of the selected component in the device.
 *
 * @param dev_id
 *   The identifier of the device
 * @param mode
 *   The mode of the statistics to reset. Choose from device, queue or port.
 * @param queue_port_id
 *   The queue or port to reset. 0 and positive values select ports and queues,
 *   while -1 indicates all ports or queues.
 * @param ids
 *   Selects specific statistics to be reset. When NULL, all statistics selected
 *   by *mode* will be reset. If non-NULL, must point to array of at least
 *   *nb_ids* size.
 * @param nb_ids
 *   The number of ids available from the *ids* array. Ignored when ids is NULL.
 * @return
 *   - zero: successfully reset the statistics to zero
 *   - negative value: -EINVAL invalid parameters, -ENOTSUP if not supported.
 */
int
rte_event_dev_xstats_reset(uint8_t dev_id,
			   enum rte_event_dev_xstats_mode mode,
			   int16_t queue_port_id,
			   const uint32_t ids[],
			   uint32_t nb_ids);

/**
 * Trigger the eventdev self test.
 *
 * @param dev_id
 *   The identifier of the device
 * @return
 *   - 0: Selftest successful
 *   - -ENOTSUP if the device doesn't support selftest
 *   - other values < 0 on failure.
 */
int rte_event_dev_selftest(uint8_t dev_id);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EVENTDEV_H_ */
