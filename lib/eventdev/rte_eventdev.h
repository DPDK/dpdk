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
 * ====================
 *
 * In a traditional DPDK application model, the application polls Ethdev port RX
 * queues to look for work, and processing is done in a run-to-completion manner,
 * after which the packets are transmitted on a Ethdev TX queue. Load is
 * distributed by statically assigning ports and queues to lcores, and NIC
 * receive-side scaling (RSS), or similar, is employed to distribute network flows
 * (and thus work) on the same port across multiple RX queues.
 *
 * In contrast, in an event-driven model, as supported by this "eventdev" library,
 * incoming packets (or other input events) are fed into an event device, which
 * schedules those packets across the available lcores, in accordance with its configuration.
 * This event-driven programming model offers applications automatic multicore scaling,
 * dynamic load balancing, pipelining, packet order maintenance, synchronization,
 * and prioritization/quality of service.
 *
 * The Event Device API is composed of two parts:
 *
 * - The application-oriented Event API that includes functions to setup
 *   an event device (configure it, setup its queues, ports and start it), to
 *   establish the links between queues and ports to receive events, and so on.
 *
 * - The driver-oriented Event API that exports a function allowing
 *   an event poll Mode Driver (PMD) to register itself as
 *   an event device driver.
 *
 * Application-oriented Event API
 * ------------------------------
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
 * **Event device**: A hardware or software-based event scheduler.
 *
 * **Event**: Represents an item of work and is the smallest unit of scheduling.
 * An event carries metadata, such as queue ID, scheduling type, and event priority,
 * and data such as one or more packets or other kinds of buffers.
 * Some examples of events are:
 * - a software-generated item of work originating from a lcore,
 *   perhaps carrying a packet to be processed.
 * - a crypto work completion notification.
 * - a timer expiry notification.
 *
 * **Event queue**: A queue containing events that are to be scheduled by the event device.
 * An event queue contains events of different flows associated with scheduling
 * types, such as atomic, ordered, or parallel.
 * Each event given to an event device must have a valid event queue id field in the metadata,
 * to specify on which event queue in the device the event must be placed,
 * for later scheduling.
 *
 * **Event port**: An application's interface into the event dev for enqueue and
 * dequeue operations. Each event port can be linked with one or more
 * event queues for dequeue operations.
 * Enqueue and dequeue from a port is not thread-safe, and the expected use-case is
 * that each port is polled by only a single lcore. [If this is not the case,
 * a suitable synchronization mechanism should be used to prevent simultaneous
 * access from multiple lcores.]
 * To schedule events to an lcore, the event device will schedule them to the event port(s)
 * being polled by that lcore.
 *
 * *NOTE*: By default, all the functions of the Event Device API exported by a PMD
 * are non-thread-safe functions, which must not be invoked on the same object in parallel on
 * different logical cores.
 * For instance, the dequeue function of a PMD cannot be invoked in parallel on two logical
 * cores to operate on same  event port. Of course, this function
 * can be invoked in parallel by different logical cores on different ports.
 * It is the responsibility of the upper level application to enforce this rule.
 *
 * In all functions of the Event API, the Event device is
 * designated by an integer >= 0 named the device identifier *dev_id*
 *
 * The functions exported by the application Event API to setup a device
 * must be invoked in the following order:
 *     - rte_event_dev_configure()
 *     - rte_event_queue_setup()
 *     - rte_event_port_setup()
 *     - rte_event_port_link()
 *     - rte_event_dev_start()
 *
 * Then, the application can invoke, in any order, the functions
 * exported by the Event API to dequeue events, enqueue events,
 * and link and unlink event queue(s) to event ports.
 *
 * Before configuring a device, an application should call rte_event_dev_info_get()
 * to determine the capabilities of the event device, and any queue or port
 * limits of that device. The parameters set in the various device configuration
 * structures may need to be adjusted based on the max values provided in the
 * device information structure returned from the rte_event_dev_info_get() API.
 * An application may use rte_event_queue_default_conf_get() or
 * rte_event_port_default_conf_get() to get the default configuration
 * to set up an event queue or event port by overriding few default values.
 *
 * If the application wants to change the configuration (i.e. call
 * rte_event_dev_configure(), rte_event_queue_setup(), or
 * rte_event_port_setup()), it must call rte_event_dev_stop() first to stop the
 * device and then do the reconfiguration before calling rte_event_dev_start()
 * again. The schedule, enqueue and dequeue functions should not be invoked
 * when the device is stopped.
 *
 * Finally, an application can close an Event device by invoking the
 * rte_event_dev_close() function. Once closed, a device cannot be
 * reconfigured or restarted.
 *
 * Driver-Oriented Event API
 * -------------------------
 *
 * At the Event driver level, Event devices are represented by a generic
 * data structure of type *rte_event_dev*.
 *
 * Event devices are dynamically registered during the PCI/SoC device probing
 * phase performed at EAL initialization time.
 * When an Event device is being probed, an *rte_event_dev* structure is allocated
 * for it and the event_dev_init() function supplied by the Event driver
 * is invoked to properly initialize the device.
 *
 * The role of the device init function is to reset the device hardware or
 * to initialize the software event driver implementation.
 *
 * If the device init operation is successful, the device is assigned a device
 * id (dev_id) for application use.
 * Otherwise, the *rte_event_dev* structure is freed.
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
 * For performance reasons, the addresses of the fast-path functions of the
 * event driver are not contained in the *event_dev_ops* structure.
 * Instead, they are directly stored at the beginning of the *rte_event_dev*
 * structure to avoid an extra indirect memory access during their invocation.
 *
 * Event Enqueue, Dequeue and Scheduling
 * -------------------------------------
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
 * The application processes the events and sends them to a downstream event queue through
 * rte_event_enqueue_burst(), if it is an intermediate stage of event processing.
 * On the final stage of processing, the application may use the Tx adapter API for maintaining
 * the event ingress order while sending the packet/event on the wire via NIC Tx.
 *
 * The point at which events are scheduled to ports depends on the device.
 * For hardware devices, scheduling occurs asynchronously without any software
 * intervention. Software schedulers can either be distributed
 * (each worker thread schedules events to its own port) or centralized
 * (a dedicated thread schedules to all ports). Distributed software schedulers
 * perform the scheduling inside the enqueue or dequeue functions, whereas centralized
 * software schedulers need a dedicated service core for scheduling.
 * The absence of the RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED capability flag
 * indicates that the device is centralized and thus needs a dedicated scheduling
 * thread (generally an RTE service that should be mapped to one or more service cores)
 * that repeatedly calls the software specific scheduling function.
 *
 * An event driven worker thread has following typical workflow on fastpath:
 * \code{.c}
 *	while (1) {
 *		rte_event_dequeue_burst(...);
 *		(event processing)
 *		rte_event_enqueue_burst(...);
 *	}
 * \endcode
 */

#include <rte_bitops.h>
#include <rte_common.h>
#include <rte_compat.h>
#include <rte_errno.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_mempool.h>

#include "rte_eventdev_trace_fp.h"

struct rte_mbuf; /* we just use mbuf pointers; no need to include rte_mbuf.h */
struct rte_event;

/* Event device capability bitmap flags */
#define RTE_EVENT_DEV_CAP_QUEUE_QOS           RTE_BIT32(0)
/**< Event scheduling prioritization is based on the priority and weight
 * associated with each event queue.
 *
 * Events from a queue with highest priority
 * are scheduled first. If the queues are of same priority, weight of the queues
 * are considered to select a queue in a weighted round robin fashion.
 * Subsequent dequeue calls from an event port could see events from the same
 * event queue, if the queue is configured with an affinity count. Affinity
 * count is the number of subsequent dequeue calls, in which an event port
 * should use the same event queue if the queue is non-empty
 *
 * NOTE: A device may use both queue prioritization and event prioritization
 * (@ref RTE_EVENT_DEV_CAP_EVENT_QOS capability) when making packet scheduling decisions.
 *
 *  @see rte_event_queue_setup()
 *  @see rte_event_queue_attr_set()
 */
#define RTE_EVENT_DEV_CAP_EVENT_QOS           RTE_BIT32(1)
/**< Event scheduling prioritization is based on the priority associated with
 *  each event.
 *
 *  Priority of each event is supplied in *rte_event* structure
 *  on each enqueue operation.
 *  If this capability is not set, the priority field of the event structure
 *  is ignored for each event.
 *
 * NOTE: A device may use both queue prioritization (@ref RTE_EVENT_DEV_CAP_QUEUE_QOS capability)
 * and event prioritization when making packet scheduling decisions.

 *  @see rte_event_enqueue_burst()
 */
#define RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED   RTE_BIT32(2)
/**< Event device operates in distributed scheduling mode.
 *
 * In distributed scheduling mode, event scheduling happens in HW or
 * rte_event_dequeue_burst() / rte_event_enqueue_burst() or the combination of these two.
 * If the flag is not set then eventdev is centralized and thus needs a
 * dedicated service core that acts as a scheduling thread.
 *
 * @see rte_event_dev_service_id_get()
 */
#define RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES     RTE_BIT32(3)
/**< Event device is capable of accepting enqueued events, of any type
 * advertised as supported by the device, to all destination queues.
 *
 * When this capability is set, and @ref RTE_EVENT_QUEUE_CFG_ALL_TYPES flag is set
 * in @ref rte_event_queue_conf.event_queue_cfg, the "schedule_type" field of the
 * @ref rte_event_queue_conf structure is ignored when a queue is being configured.
 * Instead the "sched_type" field of each event enqueued is used to
 * select the scheduling to be performed on that event.
 *
 * If this capability is not set, or the configuration flag is not set,
 * the queue only supports events of the *RTE_SCHED_TYPE_* type specified
 * in the @ref rte_event_queue_conf structure  at time of configuration.
 * The behaviour when events of other scheduling types are sent to the queue is
 * undefined.
 *
 * @see RTE_EVENT_QUEUE_CFG_ALL_TYPES
 * @see RTE_SCHED_TYPE_ATOMIC
 * @see RTE_SCHED_TYPE_ORDERED
 * @see RTE_SCHED_TYPE_PARALLEL
 * @see rte_event_queue_conf.event_queue_cfg
 * @see rte_event_queue_conf.schedule_type
 * @see rte_event_enqueue_burst()
 */
#define RTE_EVENT_DEV_CAP_BURST_MODE          RTE_BIT32(4)
/**< Event device is capable of operating in burst mode for enqueue(forward,
 * release) and dequeue operation.
 *
 * If this capability is not set, application
 * can still use the rte_event_dequeue_burst() and rte_event_enqueue_burst() but
 * PMD accepts or returns only one event at a time.
 *
 * @see rte_event_dequeue_burst()
 * @see rte_event_enqueue_burst()
 */
#define RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE    RTE_BIT32(5)
/**< Event device ports support disabling the implicit release feature, in
 * which the port will release all unreleased events in its dequeue operation.
 *
 * If this capability is set and the port is configured with implicit release
 * disabled, the application is responsible for explicitly releasing events
 * using either the @ref RTE_EVENT_OP_FORWARD or the @ref RTE_EVENT_OP_RELEASE event
 * enqueue operations.
 *
 * @see rte_event_dequeue_burst()
 * @see rte_event_enqueue_burst()
 */

#define RTE_EVENT_DEV_CAP_NONSEQ_MODE         RTE_BIT32(6)
/**< Event device is capable of operating in non-sequential mode.
 *
 * The path of the event is not necessary to be sequential. Application can change
 * the path of event at runtime and events may be sent to queues in any order.
 *
 * If the flag is not set, then event each event will follow a path from queue 0
 * to queue 1 to queue 2 etc.
 * The eventdev will return an error when the application enqueues an event for a
 * qid which is not the next in the sequence.
 */

#define RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK   RTE_BIT32(7)
/**< Event device is capable of reconfiguring the queue/port link at runtime.
 *
 * If the flag is not set, the eventdev queue/port link is only can be
 * configured during  initialization, or by stopping the device and
 * then later restarting it after reconfiguration.
 *
 * @see rte_event_port_link()
 * @see rte_event_port_unlink()
 */

#define RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT RTE_BIT32(8)
/**< Event device is capable of setting up links between multiple queues and a single port.
 *
 * If the flag is not set, each port may only be linked to a single queue, and
 * so can only receive events from that queue.
 * However, each queue may be linked to multiple ports.
 *
 * @see rte_event_port_link()
 */

#define RTE_EVENT_DEV_CAP_CARRY_FLOW_ID RTE_BIT32(9)
/**< Event device preserves the flow ID from the enqueued event to the dequeued event.
 *
 * If this flag is not set,
 * the content of the flow-id field in dequeued events is implementation dependent.
 *
 * @see rte_event_dequeue_burst()
 */

#define RTE_EVENT_DEV_CAP_MAINTENANCE_FREE RTE_BIT32(10)
/**< Event device *does not* require calls to rte_event_maintain().
 *
 * An event device that does not set this flag requires calls to
 * rte_event_maintain() during periods when neither
 * rte_event_dequeue_burst() nor rte_event_enqueue_burst() are called
 * on a port. This will allow the event device to perform internal
 * processing, such as flushing buffered events, return credits to a
 * global pool, or process signaling related to load balancing.
 *
 * @see rte_event_maintain()
 */

#define RTE_EVENT_DEV_CAP_RUNTIME_QUEUE_ATTR RTE_BIT32(11)
/**< Event device is capable of changing the queue attributes at runtime i.e
 * after rte_event_queue_setup() or rte_event_dev_start() call sequence.
 *
 * If this flag is not set, event queue attributes can only be configured during
 * rte_event_queue_setup().
 *
 * @see rte_event_queue_setup()
 */

#define RTE_EVENT_DEV_CAP_PROFILE_LINK RTE_BIT32(12)
/**< Event device is capable of supporting multiple link profiles per event port.
 *
 * When set, the value of `rte_event_dev_info::max_profiles_per_port` is greater
 * than one, and multiple profiles may be configured and then switched at runtime.
 * If not set, only a single profile may be configured, which may itself be
 * runtime adjustable (if @ref RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK is set).
 *
 * @see rte_event_port_profile_links_set()
 * @see rte_event_port_profile_links_get()
 * @see rte_event_port_profile_switch()
 * @see RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK
 */

#define RTE_EVENT_DEV_CAP_ATOMIC  RTE_BIT32(13)
/**< Event device is capable of atomic scheduling.
 * When this flag is set, the application can configure queues with scheduling type
 * atomic on this event device.
 *
 * @see RTE_SCHED_TYPE_ATOMIC
 */

#define RTE_EVENT_DEV_CAP_ORDERED  RTE_BIT32(14)
/**< Event device is capable of ordered scheduling.
 * When this flag is set, the application can configure queues with scheduling type
 * ordered on this event device.
 *
 * @see RTE_SCHED_TYPE_ORDERED
 */

#define RTE_EVENT_DEV_CAP_PARALLEL  RTE_BIT32(15)
/**< Event device is capable of parallel scheduling.
 * When this flag is set, the application can configure queues with scheduling type
 * parallel on this event device.
 *
 * @see RTE_SCHED_TYPE_PARALLEL
 */

#define RTE_EVENT_DEV_CAP_INDEPENDENT_ENQ  RTE_BIT32(16)
/**< Event device is capable of independent enqueue.
 * A new capability, RTE_EVENT_DEV_CAP_INDEPENDENT_ENQ, will indicate that Eventdev
 * supports the enqueue in any order or specifically in a different order than the
 * dequeue. Eventdev PMD can either dequeue events in the changed order in which
 * they are enqueued or restore the original order before sending them to the
 * underlying hardware device. A flag is provided during the port configuration to
 * inform Eventdev PMD that the application intends to use an independent enqueue
 * order on a particular port. Note that this capability only matters for eventdevs
 * supporting burst mode.
 *
 * When an implicit release is enabled on a port, Eventdev PMD will also handle
 * the insertion of RELEASE events in place of dropped events. The independent enqueue
 * feature only applies to FORWARD and RELEASE events. New events (op=RTE_EVENT_OP_NEW)
 * will be dequeued in the order the application enqueues them and do not maintain
 * any order relative to FORWARD/RELEASE events. FORWARD vs NEW relaxed ordering
 * only applies to ports that have enabled independent enqueue feature.
 */

#define RTE_EVENT_DEV_CAP_EVENT_PRESCHEDULE RTE_BIT32(17)
/**< Event device supports event pre-scheduling.
 *
 * When this capability is available, the application can enable event pre-scheduling on the event
 * device to pre-schedule events to a event port when `rte_event_dequeue_burst()`
 * is issued.
 * The pre-schedule process starts with the `rte_event_dequeue_burst()` call and the
 * pre-scheduled events are returned on the next `rte_event_dequeue_burst()` call.
 *
 * @see rte_event_dev_configure()
 */

#define RTE_EVENT_DEV_CAP_EVENT_PRESCHEDULE_ADAPTIVE RTE_BIT32(18)
/**< Event device supports adaptive event pre-scheduling.
 *
 * When this capability is available, the application can enable adaptive pre-scheduling
 * on the event device where the events are pre-scheduled when there are no forward
 * progress constraints with the currently held flow contexts.
 * The pre-schedule process starts with the `rte_event_dequeue_burst()` call and the
 * pre-scheduled events are returned on the next `rte_event_dequeue_burst()` call.
 *
 * @see rte_event_dev_configure()
 */

#define RTE_EVENT_DEV_CAP_PER_PORT_PRESCHEDULE RTE_BIT32(19)
/**< Event device supports event pre-scheduling per event port.
 *
 * When this flag is set, the event device allows controlling the event
 * pre-scheduling at a event port granularity.
 *
 * @see rte_event_dev_configure()
 * @see rte_event_port_preschedule_modify()
 */

#define RTE_EVENT_DEV_CAP_PRESCHEDULE_EXPLICIT RTE_BIT32(20)
/**< Event device supports explicit pre-scheduling.
 *
 * When this flag is set, the application can issue pre-schedule request on
 * a event port.
 *
 * @see rte_event_port_preschedule()
 */

/* Event device priority levels */
#define RTE_EVENT_DEV_PRIORITY_HIGHEST   0
/**< Highest priority level for events and queues.
 *
 * @see rte_event_queue_setup()
 * @see rte_event_enqueue_burst()
 * @see rte_event_port_link()
 */
#define RTE_EVENT_DEV_PRIORITY_NORMAL    128
/**< Normal priority level for events and queues.
 *
 * @see rte_event_queue_setup()
 * @see rte_event_enqueue_burst()
 * @see rte_event_port_link()
 */
#define RTE_EVENT_DEV_PRIORITY_LOWEST    255
/**< Lowest priority level for events and queues.
 *
 * @see rte_event_queue_setup()
 * @see rte_event_enqueue_burst()
 * @see rte_event_port_link()
 */

/* Event queue scheduling weights */
#define RTE_EVENT_QUEUE_WEIGHT_HIGHEST 255
/**< Highest weight of an event queue.
 *
 * @see rte_event_queue_attr_get()
 * @see rte_event_queue_attr_set()
 */
#define RTE_EVENT_QUEUE_WEIGHT_LOWEST 0
/**< Lowest weight of an event queue.
 *
 * @see rte_event_queue_attr_get()
 * @see rte_event_queue_attr_set()
 */

/* Event queue scheduling affinity */
#define RTE_EVENT_QUEUE_AFFINITY_HIGHEST 255
/**< Highest scheduling affinity of an event queue.
 *
 * @see rte_event_queue_attr_get()
 * @see rte_event_queue_attr_set()
 */
#define RTE_EVENT_QUEUE_AFFINITY_LOWEST 0
/**< Lowest scheduling affinity of an event queue.
 *
 * @see rte_event_queue_attr_get()
 * @see rte_event_queue_attr_set()
 */

/**
 * Get the total number of event devices.
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
 *   Event device identifier (dev_id >= 0) on success.
 *   Negative error code on failure:
 *   - -EINVAL - input name parameter is invalid.
 *   - -ENODEV - no event device found with that name.
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
 *   -EINVAL on error, where the given dev_id value does not
 *   correspond to any event device.
 */
int
rte_event_dev_socket_id(uint8_t dev_id);

/**
 * Event device information
 */
struct rte_event_dev_info {
	const char *driver_name;	/**< Event driver name. */
	struct rte_device *dev;	/**< Device information. */
	uint32_t min_dequeue_timeout_ns;
	/**< Minimum global dequeue timeout(ns) supported by this device. */
	uint32_t max_dequeue_timeout_ns;
	/**< Maximum global dequeue timeout(ns) supported by this device. */
	uint32_t dequeue_timeout_ns;
	/**< Configured global dequeue timeout(ns) for this device. */
	uint8_t max_event_queues;
	/**< Maximum event queues supported by this device.
	 *
	 * This count excludes any queues covered by @ref max_single_link_event_port_queue_pairs.
	 */
	uint32_t max_event_queue_flows;
	/**< Maximum number of flows within an event queue supported by this device. */
	uint8_t max_event_queue_priority_levels;
	/**< Maximum number of event queue priority levels supported by this device.
	 *
	 * Valid when the device has @ref RTE_EVENT_DEV_CAP_QUEUE_QOS capability.
	 *
	 * The implementation shall normalize priority values specified between
	 * @ref RTE_EVENT_DEV_PRIORITY_HIGHEST and @ref RTE_EVENT_DEV_PRIORITY_LOWEST
	 * to map them internally to this range of priorities.
	 * [For devices supporting a power-of-2 number of priority levels, this
	 * normalization will be done via a right-shift operation, so only the top
	 * log2(max_levels) bits will be used by the event device.]
	 *
	 * @see rte_event_queue_conf.priority
	 */
	uint8_t max_event_priority_levels;
	/**< Maximum number of event priority levels by this device.
	 *
	 * Valid when the device has @ref RTE_EVENT_DEV_CAP_EVENT_QOS capability.
	 *
	 * The implementation shall normalize priority values specified between
	 * @ref RTE_EVENT_DEV_PRIORITY_HIGHEST and @ref RTE_EVENT_DEV_PRIORITY_LOWEST
	 * to map them internally to this range of priorities.
	 * [For devices supporting a power-of-2 number of priority levels, this
	 * normalization will be done via a right-shift operation, so only the top
	 * log2(max_levels) bits will be used by the event device.]
	 *
	 * @see rte_event.priority
	 */
	uint8_t max_event_ports;
	/**< Maximum number of event ports supported by this device.
	 *
	 * This count excludes any ports covered by @ref max_single_link_event_port_queue_pairs.
	 */
	uint8_t max_event_port_dequeue_depth;
	/**< Maximum number of events that can be dequeued at a time from an event port
	 * on this device.
	 *
	 * A device that does not support burst dequeue
	 * (@ref RTE_EVENT_DEV_CAP_BURST_MODE) will set this to 1.
	 */
	uint32_t max_event_port_enqueue_depth;
	/**< Maximum number of events that can be enqueued at a time to an event port
	 * on this device.
	 *
	 * A device that does not support burst enqueue
	 * (@ref RTE_EVENT_DEV_CAP_BURST_MODE) will set this to 1.
	 */
	uint8_t max_event_port_links;
	/**< Maximum number of queues that can be linked to a single event port on this device.
	 */
	int32_t max_num_events;
	/**< A *closed system* event dev has a limit on the number of events it
	 * can manage at a time.
	 * Once the number of events tracked by an eventdev exceeds this number,
	 * any enqueues of NEW events will fail.
	 * An *open system* event dev does not have a limit and will specify this as -1.
	 */
	uint32_t event_dev_cap;
	/**< Event device capabilities flags (RTE_EVENT_DEV_CAP_*). */
	uint8_t max_single_link_event_port_queue_pairs;
	/**< Maximum number of event ports and queues, supported by this device,
	 * that are optimized for (and only capable of) single-link configurations.
	 * These ports and queues are not accounted for in @ref max_event_ports
	 * or @ref max_event_queues.
	 */
	uint8_t max_profiles_per_port;
	/**< Maximum number of event queue link profiles per event port.
	 * A device that doesn't support multiple profiles will set this as 1.
	 */
};

/**
 * Retrieve details of an event device's capabilities and configuration limits.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param[out] dev_info
 *   A pointer to a structure of type *rte_event_dev_info* to be filled with the
 *   information about the device's capabilities.
 *
 * @return
 *   - 0: Success, information about the event device is present in dev_info.
 *   - <0: Failure, error code returned by the function.
 *     - -EINVAL - invalid input parameters, e.g. incorrect device id.
 *     - -ENOTSUP - device does not support returning capabilities information.
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
#define RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT RTE_BIT32(0)
/**< Override the global *dequeue_timeout_ns* and use per dequeue timeout in ns.
 *  @see rte_event_dequeue_timeout_ticks(), rte_event_dequeue_burst()
 */

/** Event device pre-schedule type enumeration. */
enum rte_event_dev_preschedule_type {
	RTE_EVENT_PRESCHEDULE_NONE,
	/**< Disable pre-schedule across the event device or on a given event port.
	 * @ref rte_event_dev_config.preschedule_type
	 * @ref rte_event_port_preschedule_modify()
	 */
	RTE_EVENT_PRESCHEDULE,
	/**< Enable pre-schedule always across the event device or a given event port.
	 * @ref rte_event_dev_config.preschedule_type
	 * @ref rte_event_port_preschedule_modify()
	 * @see RTE_EVENT_DEV_CAP_EVENT_PRESCHEDULE
	 * @see RTE_EVENT_DEV_CAP_PER_PORT_PRESCHEDULE
	 */
	RTE_EVENT_PRESCHEDULE_ADAPTIVE,
	/**< Enable adaptive pre-schedule across the event device or a given event port.
	 * Delay issuing pre-schedule until there are no forward progress constraints with
	 * the held flow contexts.
	 * @ref rte_event_dev_config.preschedule_type
	 * @ref rte_event_port_preschedule_modify()
	 * @see RTE_EVENT_DEV_CAP_EVENT_PRESCHEDULE_ADAPTIVE
	 * @see RTE_EVENT_DEV_CAP_PER_PORT_PRESCHEDULE
	 */
};

/** Event device configuration structure */
struct rte_event_dev_config {
	uint32_t dequeue_timeout_ns;
	/**< rte_event_dequeue_burst() timeout on this device.
	 * This value should be in the range of @ref rte_event_dev_info.min_dequeue_timeout_ns and
	 * @ref rte_event_dev_info.max_dequeue_timeout_ns returned by
	 * @ref rte_event_dev_info_get()
	 * The value 0 is allowed, in which case, default dequeue timeout used.
	 * @see RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT
	 */
	int32_t nb_events_limit;
	/**< In a *closed system* this field is the limit on maximum number of
	 * events that can be inflight in the eventdev at a given time. The
	 * limit is required to ensure that the finite space in a closed system
	 * is not exhausted.
	 * The value cannot exceed @ref rte_event_dev_info.max_num_events
	 * returned by rte_event_dev_info_get().
	 *
	 * This value should be set to -1 for *open systems*, that is,
	 * those systems returning -1 in @ref rte_event_dev_info.max_num_events.
	 *
	 * @see rte_event_port_conf.new_event_threshold
	 */
	uint8_t nb_event_queues;
	/**< Number of event queues to configure on this device.
	 * This value *includes* any single-link queue-port pairs to be used.
	 * This value cannot exceed @ref rte_event_dev_info.max_event_queues +
	 * @ref rte_event_dev_info.max_single_link_event_port_queue_pairs
	 * returned by rte_event_dev_info_get().
	 * The number of non-single-link queues i.e. this value less
	 * *nb_single_link_event_port_queues* in this struct, cannot exceed
	 * @ref rte_event_dev_info.max_event_queues
	 */
	uint8_t nb_event_ports;
	/**< Number of event ports to configure on this device.
	 * This value *includes* any single-link queue-port pairs to be used.
	 * This value cannot exceed @ref rte_event_dev_info.max_event_ports +
	 * @ref rte_event_dev_info.max_single_link_event_port_queue_pairs
	 * returned by rte_event_dev_info_get().
	 * The number of non-single-link ports i.e. this value less
	 * *nb_single_link_event_port_queues* in this struct, cannot exceed
	 * @ref rte_event_dev_info.max_event_ports
	 */
	uint32_t nb_event_queue_flows;
	/**< Max number of flows needed for a single event queue on this device.
	 * This value cannot exceed @ref rte_event_dev_info.max_event_queue_flows
	 * returned by rte_event_dev_info_get()
	 */
	uint32_t nb_event_port_dequeue_depth;
	/**< Max number of events that can be dequeued at a time from an event port on this device.
	 * This value cannot exceed @ref rte_event_dev_info.max_event_port_dequeue_depth
	 * returned by rte_event_dev_info_get().
	 * Ignored when device is not RTE_EVENT_DEV_CAP_BURST_MODE capable.
	 * @see rte_event_port_setup() rte_event_dequeue_burst()
	 */
	uint32_t nb_event_port_enqueue_depth;
	/**< Maximum number of events can be enqueued at a time to an event port on this device.
	 * This value cannot exceed @ref rte_event_dev_info.max_event_port_enqueue_depth
	 * returned by rte_event_dev_info_get().
	 * Ignored when device is not RTE_EVENT_DEV_CAP_BURST_MODE capable.
	 * @see rte_event_port_setup() rte_event_enqueue_burst()
	 */
	uint32_t event_dev_cfg;
	/**< Event device config flags(RTE_EVENT_DEV_CFG_)*/
	uint8_t nb_single_link_event_port_queues;
	/**< Number of event ports and queues that will be singly-linked to
	 * each other. These are a subset of the overall event ports and
	 * queues; this value cannot exceed *nb_event_ports* or
	 * *nb_event_queues*. If the device has ports and queues that are
	 * optimized for single-link usage, this field is a hint for how many
	 * to allocate; otherwise, regular event ports and queues will be used.
	 */
	enum rte_event_dev_preschedule_type preschedule_type;
	/**< Event pre-schedule type to use across the event device, if supported.
	 * @see RTE_EVENT_DEV_CAP_EVENT_PRESCHEDULE
	 * @see RTE_EVENT_DEV_CAP_EVENT_PRESCHEDULE_ADAPTIVE
	 */
};

/**
 * Configure an event device.
 *
 * This function must be invoked before any other configuration function in the
 * API, when preparing an event device for application use.
 * This function can also be re-invoked when a device is in the stopped state.
 *
 * The caller should use rte_event_dev_info_get() to get the capabilities and
 * resource limits for this event device before calling this API.
 * Many values in the dev_conf input parameter are subject to limits given
 * in the device information returned from rte_event_dev_info_get().
 *
 * @param dev_id
 *   The identifier of the device to configure.
 * @param dev_conf
 *   The event device configuration structure.
 *
 * @return
 *   - 0: Success, device configured.
 *   - <0: Error code returned by the driver configuration function.
 *     - -ENOTSUP - device does not support configuration.
 *     - -EINVAL  - invalid input parameter.
 *     - -EBUSY   - device has already been started.
 */
int
rte_event_dev_configure(uint8_t dev_id,
			const struct rte_event_dev_config *dev_conf);

/* Event queue specific APIs */

/* Event queue configuration bitmap flags */
#define RTE_EVENT_QUEUE_CFG_ALL_TYPES          RTE_BIT32(0)
/**< Allow events with schedule types ATOMIC, ORDERED, and PARALLEL to be enqueued to this queue.
 *
 * The scheduling type to be used is that specified in each individual event.
 * This flag can only be set when configuring queues on devices reporting the
 * @ref RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES capability.
 *
 * Without this flag, only events with the specific scheduling type configured at queue setup
 * can be sent to the queue.
 *
 * @see RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES
 * @see RTE_SCHED_TYPE_ORDERED, RTE_SCHED_TYPE_ATOMIC, RTE_SCHED_TYPE_PARALLEL
 * @see rte_event_enqueue_burst()
 */
#define RTE_EVENT_QUEUE_CFG_SINGLE_LINK        RTE_BIT32(1)
/**< This event queue links only to a single event port.
 *
 * No load-balancing of events is performed, as all events
 * sent to this queue end up at the same event port.
 * The number of queues on which this flag is to be set must be
 * configured at device configuration time, by setting
 * @ref rte_event_dev_config.nb_single_link_event_port_queues
 * parameter appropriately.
 *
 * This flag serves as a hint only, any devices without specific
 * support for single-link queues can fall-back automatically to
 * using regular queues with a single destination port.
 *
 *  @see rte_event_dev_info.max_single_link_event_port_queue_pairs
 *  @see rte_event_dev_config.nb_single_link_event_port_queues
 *  @see rte_event_port_setup(), rte_event_port_link()
 */

/** Event queue configuration structure */
struct rte_event_queue_conf {
	uint32_t nb_atomic_flows;
	/**< The maximum number of active flows this queue can track at any
	 * given time.
	 *
	 * If the queue is configured for atomic scheduling (by
	 * applying the @ref RTE_EVENT_QUEUE_CFG_ALL_TYPES flag to
	 * @ref rte_event_queue_conf.event_queue_cfg
	 * or @ref RTE_SCHED_TYPE_ATOMIC flag to @ref rte_event_queue_conf.schedule_type), then the
	 * value must be in the range of [1, @ref rte_event_dev_config.nb_event_queue_flows],
	 * which was previously provided in rte_event_dev_configure().
	 *
	 * If the queue is not configured for atomic scheduling this value is ignored.
	 */
	uint32_t nb_atomic_order_sequences;
	/**< The maximum number of outstanding events waiting to be
	 * reordered by this queue. In other words, the number of entries in
	 * this queue’s reorder buffer. When the number of events in the
	 * reorder buffer reaches to *nb_atomic_order_sequences* then the
	 * scheduler cannot schedule the events from this queue and no
	 * events will be returned from dequeue until one or more entries are
	 * freed up/released.
	 *
	 * If the queue is configured for ordered scheduling (by applying the
	 * @ref RTE_EVENT_QUEUE_CFG_ALL_TYPES flag to @ref rte_event_queue_conf.event_queue_cfg or
	 * @ref RTE_SCHED_TYPE_ORDERED flag to @ref rte_event_queue_conf.schedule_type),
	 * then the value must be in the range of
	 * [1, @ref rte_event_dev_config.nb_event_queue_flows], which was
	 * previously supplied to rte_event_dev_configure().
	 *
	 * If the queue is not configured for ordered scheduling, then this value is ignored.
	 */
	uint32_t event_queue_cfg;
	/**< Queue cfg flags(EVENT_QUEUE_CFG_) */
	uint8_t schedule_type;
	/**< Queue schedule type(RTE_SCHED_TYPE_*).
	 *
	 * Valid when @ref RTE_EVENT_QUEUE_CFG_ALL_TYPES flag is not set in
	 * @ref rte_event_queue_conf.event_queue_cfg.
	 *
	 * If the @ref RTE_EVENT_QUEUE_CFG_ALL_TYPES flag is set, then this field is ignored.
	 *
	 * @see RTE_SCHED_TYPE_ORDERED, RTE_SCHED_TYPE_ATOMIC, RTE_SCHED_TYPE_PARALLEL
	 */
	uint8_t priority;
	/**< Priority for this event queue relative to other event queues.
	 *
	 * The requested priority should in the range of
	 * [@ref RTE_EVENT_DEV_PRIORITY_HIGHEST, @ref RTE_EVENT_DEV_PRIORITY_LOWEST].
	 * The implementation shall normalize the requested priority to
	 * event device supported priority value.
	 *
	 * Valid when the device has @ref RTE_EVENT_DEV_CAP_QUEUE_QOS capability,
	 * ignored otherwise
	 */
	uint8_t weight;
	/**< Weight of the event queue relative to other event queues.
	 *
	 * The requested weight should be in the range of
	 * [@ref RTE_EVENT_QUEUE_WEIGHT_HIGHEST, @ref RTE_EVENT_QUEUE_WEIGHT_LOWEST].
	 * The implementation shall normalize the requested weight to event
	 * device supported weight value.
	 *
	 * Valid when the device has @ref RTE_EVENT_DEV_CAP_QUEUE_QOS capability,
	 * ignored otherwise.
	 */
	uint8_t affinity;
	/**< Affinity of the event queue relative to other event queues.
	 *
	 * The requested affinity should be in the range of
	 * [@ref RTE_EVENT_QUEUE_AFFINITY_HIGHEST, @ref RTE_EVENT_QUEUE_AFFINITY_LOWEST].
	 * The implementation shall normalize the requested affinity to event
	 * device supported affinity value.
	 *
	 * Valid when the device has @ref RTE_EVENT_DEV_CAP_QUEUE_QOS capability,
	 * ignored otherwise.
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
 *   The value must be less than @ref rte_event_dev_config.nb_event_queues
 *   previously supplied to rte_event_dev_configure().
 * @param[out] queue_conf
 *   The pointer to the default event queue configuration data.
 * @return
 *   - 0: Success, driver updates the default event queue configuration data.
 *   - <0: Error code returned by the driver info get function.
 *
 * @see rte_event_queue_setup()
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
 *   The index of the event queue to setup. The value must be
 *   less than @ref rte_event_dev_config.nb_event_queues previously supplied to
 *   rte_event_dev_configure().
 * @param queue_conf
 *   The pointer to the configuration data to be used for the event queue.
 *   NULL value is allowed, in which case default configuration	used.
 *
 * @see rte_event_queue_default_conf_get()
 *
 * @return
 *   - 0: Success, event queue correctly set up.
 *   - <0: event queue configuration failed.
 */
int
rte_event_queue_setup(uint8_t dev_id, uint8_t queue_id,
		      const struct rte_event_queue_conf *queue_conf);

/**
 * Queue attribute id for the priority of the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_PRIORITY 0
/**
 * Queue attribute id for the number of atomic flows configured for the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_NB_ATOMIC_FLOWS 1
/**
 * Queue attribute id for the number of atomic order sequences configured for the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_NB_ATOMIC_ORDER_SEQUENCES 2
/**
 * Queue attribute id for the configuration flags for the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_EVENT_QUEUE_CFG 3
/**
 * Queue attribute id for the schedule type of the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_SCHEDULE_TYPE 4
/**
 * Queue attribute id for the weight of the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_WEIGHT 5
/**
 * Queue attribute id for the affinity of the queue.
 */
#define RTE_EVENT_QUEUE_ATTR_AFFINITY 6

/**
 * Get an attribute of an event queue.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param queue_id
 *   The index of the event queue to query. The value must be less than
 *   @ref rte_event_dev_config.nb_event_queues previously supplied to rte_event_dev_configure().
 * @param attr_id
 *   The attribute ID to retrieve (RTE_EVENT_QUEUE_ATTR_*).
 * @param[out] attr_value
 *   A pointer that will be filled in with the attribute value if successful.
 *
 * @return
 *   - 0: Successfully returned value
 *   - -EINVAL: invalid device, queue or attr_id provided, or attr_value was NULL.
 *   - -EOVERFLOW: returned when attr_id is set to
 *   @ref RTE_EVENT_QUEUE_ATTR_SCHEDULE_TYPE and @ref RTE_EVENT_QUEUE_CFG_ALL_TYPES is
 *   set in the queue configuration flags.
 */
int
rte_event_queue_attr_get(uint8_t dev_id, uint8_t queue_id, uint32_t attr_id,
			uint32_t *attr_value);

/**
 * Set an event queue attribute.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param queue_id
 *   The index of the event queue to configure. The value must be less than
 *   @ref rte_event_dev_config.nb_event_queues previously supplied to rte_event_dev_configure().
 * @param attr_id
 *   The attribute ID to set (RTE_EVENT_QUEUE_ATTR_*).
 * @param attr_value
 *   The attribute value to set.
 *
 * @return
 *   - 0: Successfully set attribute.
 *   - <0: failed to set event queue attribute.
 *   -   -EINVAL: invalid device, queue or attr_id.
 *   -   -ENOTSUP: device does not support setting the event attribute.
 */
int
rte_event_queue_attr_set(uint8_t dev_id, uint8_t queue_id, uint32_t attr_id,
			 uint64_t attr_value);

/* Event port specific APIs */

/* Event port configuration bitmap flags */
#define RTE_EVENT_PORT_CFG_DISABLE_IMPL_REL    RTE_BIT32(0)
/**< Configure the port not to release outstanding events in
 * rte_event_dev_dequeue_burst(). If set, all events received through
 * the port must be explicitly released with RTE_EVENT_OP_RELEASE or
 * RTE_EVENT_OP_FORWARD. Must be unset if the device is not
 * RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE capable.
 */
#define RTE_EVENT_PORT_CFG_SINGLE_LINK         RTE_BIT32(1)
/**< This event port links only to a single event queue.
 * The queue it links with should be similarly configured with the
 * @ref RTE_EVENT_QUEUE_CFG_SINGLE_LINK flag.
 *
 *  @see RTE_EVENT_QUEUE_CFG_SINGLE_LINK
 *  @see rte_event_port_setup(), rte_event_port_link()
 */
#define RTE_EVENT_PORT_CFG_HINT_PRODUCER       RTE_BIT32(2)
/**< Hint that this event port will primarily enqueue events to the system.
 * A PMD can optimize its internal workings by assuming that this port is
 * primarily going to enqueue NEW events.
 *
 * Note that this flag is only a hint, so PMDs must operate under the
 * assumption that any port can enqueue an event with any type of op.
 *
 *  @see rte_event_port_setup()
 */
#define RTE_EVENT_PORT_CFG_HINT_CONSUMER       RTE_BIT32(3)
/**< Hint that this event port will primarily dequeue events from the system.
 * A PMD can optimize its internal workings by assuming that this port is
 * primarily going to consume events, and not enqueue NEW or FORWARD
 * events.
 *
 * Note that this flag is only a hint, so PMDs must operate under the
 * assumption that any port can enqueue an event with any type of op.
 *
 *  @see rte_event_port_setup()
 */
#define RTE_EVENT_PORT_CFG_HINT_WORKER         RTE_BIT32(4)
/**< Hint that this event port will primarily pass existing events through.
 * A PMD can optimize its internal workings by assuming that this port is
 * primarily going to FORWARD events, and not enqueue NEW or RELEASE events
 * often.
 *
 * Note that this flag is only a hint, so PMDs must operate under the
 * assumption that any port can enqueue an event with any type of op.
 *
 *  @see rte_event_port_setup()
 */
#define RTE_EVENT_PORT_CFG_INDEPENDENT_ENQ   RTE_BIT32(5)
/**< Flag to enable independent enqueue. Must not be set if the device
 * is not RTE_EVENT_DEV_CAP_INDEPENDENT_ENQ capable. This feature
 * allows an application to enqueue RTE_EVENT_OP_FORWARD or
 * RTE_EVENT_OP_RELEASE in an order different than the order the
 * events were dequeued from the event device, while maintaining
 * RTE_SCHED_TYPE_ATOMIC or RTE_SCHED_TYPE_ORDERED semantics.
 *
 * Note that this flag only matters for Eventdevs supporting burst mode.
 *
 *  @see rte_event_port_setup()
 */

/** Event port configuration structure */
struct rte_event_port_conf {
	int32_t new_event_threshold;
	/**< A backpressure threshold for new event enqueues on this port.
	 * Use for *closed system* event dev where event capacity is limited,
	 * and cannot exceed the capacity of the event dev.
	 *
	 * Configuring ports with different thresholds can make higher priority
	 * traffic less likely to  be backpressured.
	 * For example, a port used to inject NIC Rx packets into the event dev
	 * can have a lower threshold so as not to overwhelm the device,
	 * while ports used for worker pools can have a higher threshold.
	 * This value cannot exceed the @ref rte_event_dev_config.nb_events_limit value
	 * which was previously supplied to rte_event_dev_configure().
	 *
	 * This should be set to '-1' for *open system*, i.e when
	 * @ref rte_event_dev_info.max_num_events == -1.
	 */
	uint16_t dequeue_depth;
	/**< Configure the maximum size of burst dequeues for this event port.
	 * This value cannot exceed the @ref rte_event_dev_config.nb_event_port_dequeue_depth value
	 * which was previously supplied to rte_event_dev_configure().
	 *
	 * Ignored when device does not support the @ref RTE_EVENT_DEV_CAP_BURST_MODE capability.
	 */
	uint16_t enqueue_depth;
	/**< Configure the maximum size of burst enqueues to this event port.
	 * This value cannot exceed the @ref rte_event_dev_config.nb_event_port_enqueue_depth value
	 * which was previously supplied to rte_event_dev_configure().
	 *
	 * Ignored when device does not support the @ref RTE_EVENT_DEV_CAP_BURST_MODE capability.
	 */
	uint32_t event_port_cfg; /**< Port configuration flags(EVENT_PORT_CFG_) */
};

/**
 * Retrieve the default configuration information of an event port designated
 * by its *port_id* from the event driver for an event device.
 *
 * This function is intended to be used in conjunction with rte_event_port_setup()
 * where the caller can set up the port by just overriding few default values.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The index of the event port to get the configuration information.
 *   The value must be less than @ref rte_event_dev_config.nb_event_ports
 *   previously supplied to rte_event_dev_configure().
 * @param[out] port_conf
 *   The pointer to a structure to store the default event port configuration data.
 * @return
 *   - 0: Success, driver updates the default event port configuration data.
 *   - <0: Error code returned by the driver info get function.
 *      - -EINVAL - invalid input parameter.
 *      - -ENOTSUP - function is not supported for this device.
 *
 * @see rte_event_port_setup()
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
 *   The index of the event port to setup. The value must be less than
 *   @ref rte_event_dev_config.nb_event_ports previously supplied to
 *   rte_event_dev_configure().
 * @param port_conf
 *   The pointer to the configuration data to be used for the port.
 *   NULL value is allowed, in which case the default configuration is used.
 *
 * @see rte_event_port_default_conf_get()
 *
 * @return
 *   - 0: Success, event port correctly set up.
 *   - <0: Port configuration failed.
 *     - -EINVAL - Invalid input parameter.
 *     - -EBUSY - Port already started.
 *     - -ENOTSUP - Function not supported on this device, or a NULL pointer passed
 *        as the port_conf parameter, and no default configuration function available
 *        for this device.
 *     - -EDQUOT - Application tried to link a queue configured
 *      with @ref RTE_EVENT_QUEUE_CFG_SINGLE_LINK to more than one event port.
 */
int
rte_event_port_setup(uint8_t dev_id, uint8_t port_id,
		     const struct rte_event_port_conf *port_conf);

typedef void (*rte_eventdev_port_flush_t)(uint8_t dev_id,
					  struct rte_event event, void *arg);
/**< Callback function prototype that can be passed during
 * rte_event_port_release(), invoked once per a released event.
 */

/**
 * Quiesce any core specific resources consumed by the event port.
 *
 * Event ports are generally coupled with lcores, and a given Hardware
 * implementation might require the PMD to store port specific data in the
 * lcore.
 * When the application decides to migrate the event port to another lcore
 * or teardown the current lcore it may to call `rte_event_port_quiesce`
 * to make sure that all the data associated with the event port are released
 * from the lcore, this might also include any prefetched events.
 * While releasing the event port from the lcore, this function calls the
 * user-provided flush callback once per event.
 *
 * @note Invocation of this API does not affect the existing port configuration.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The index of the event port to quiesce. The value must be less than
 *   @ref rte_event_dev_config.nb_event_ports previously supplied to rte_event_dev_configure().
 * @param release_cb
 *   Callback function invoked once per flushed event.
 * @param args
 *   Argument supplied to callback.
 */
void
rte_event_port_quiesce(uint8_t dev_id, uint8_t port_id,
		       rte_eventdev_port_flush_t release_cb, void *args);

/**
 * Port attribute id for the maximum size of a burst enqueue operation supported on a port.
 */
#define RTE_EVENT_PORT_ATTR_ENQ_DEPTH 0
/**
 * Port attribute id for the maximum size of a dequeue burst which can be returned from a port.
 */
#define RTE_EVENT_PORT_ATTR_DEQ_DEPTH 1
/**
 * Port attribute id for the new event threshold of the port.
 * Once the number of events in the system exceeds this threshold, the enqueue of NEW-type
 * events will fail.
 */
#define RTE_EVENT_PORT_ATTR_NEW_EVENT_THRESHOLD 2
/**
 * Port attribute id for the implicit release disable attribute of the port.
 */
#define RTE_EVENT_PORT_ATTR_IMPLICIT_RELEASE_DISABLE 3

/**
 * Get an attribute from a port.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The index of the event port to query. The value must be less than
 *   @ref rte_event_dev_config.nb_event_ports previously supplied to rte_event_dev_configure().
 * @param attr_id
 *   The attribute ID to retrieve (RTE_EVENT_PORT_ATTR_*)
 * @param[out] attr_value
 *   A pointer that will be filled in with the attribute value if successful
 *
 * @return
 *   - 0: Successfully returned value.
 *   - (-EINVAL) Invalid device, port or attr_id, or attr_value was NULL.
 */
int
rte_event_port_attr_get(uint8_t dev_id, uint8_t port_id, uint32_t attr_id,
			uint32_t *attr_value);

/**
 * Start an event device.
 *
 * The device start step is the last one in device setup, and enables the event
 * ports and queues to start accepting events and scheduling them to event ports.
 *
 * On success, all basic functions exported by the API (event enqueue,
 * event dequeue and so on) can be invoked.
 *
 * @param dev_id
 *   Event device identifier.
 * @return
 *   - 0: Success, device started.
 *   - -EINVAL:  Invalid device id provided.
 *   - -ENOTSUP: Device does not support this operation.
 *   - -ESTALE : Not all ports of the device are configured.
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

typedef void (*rte_eventdev_stop_flush_t)(uint8_t dev_id,
					  struct rte_event event, void *arg);
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
 * Only one callback function may be registered. Each new call replaces
 * the existing registered callback function with the new function passed in.
 *
 * To unregister a callback, call this function with a NULL callback pointer.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param callback
 *   Callback function to be invoked once per flushed event.
 *   Pass NULL to unset any previously-registered callback function.
 * @param userdata
 *   Argument supplied to callback.
 *
 * @return
 *  - 0 on success.
 *  - -EINVAL if *dev_id* is invalid.
 *
 * @see rte_event_dev_stop()
 */
int rte_event_dev_stop_flush_callback_register(uint8_t dev_id,
					       rte_eventdev_stop_flush_t callback, void *userdata);

/**
 * Close an event device. The device cannot be restarted!
 *
 * @param dev_id
 *   Event device identifier.
 *
 * @return
 *  - 0 on successfully closing device
 *  - <0 on failure to close device.
 *    - -EINVAL - invalid device id.
 *    - -ENOTSUP - operation not supported for this device.
 *    - -EAGAIN - device is busy.
 */
int
rte_event_dev_close(uint8_t dev_id);

/**
 * Event vector structure.
 */
struct __rte_aligned(16) rte_event_vector {
	uint16_t nb_elem;
	/**< Number of elements valid in this event vector. */
	uint16_t elem_offset : 12;
	/**< Offset into the vector array where valid elements start from. */
	uint16_t rsvd : 3;
	/**< Reserved for future use */
	uint16_t attr_valid : 1;
	/**< Indicates that the below union attributes have valid information.
	 */
	union {
		/* Used by Rx/Tx adapter.
		 * Indicates that all the elements in this vector belong to the
		 * same port and queue pair when originating from Rx adapter,
		 * valid only when event type is ETHDEV_VECTOR or
		 * ETH_RX_ADAPTER_VECTOR.
		 * Can also be used to indicate the Tx adapter the destination
		 * port and queue of the mbufs in the vector
		 */
		struct {
			uint16_t port;   /**< Ethernet device port id. */
			uint16_t queue;  /**< Ethernet device queue id. */
		};
	};
	/**< Union to hold common attributes of the vector array. */
	uint64_t impl_opaque;

/* empty structures do not have zero size in C++ leading to compilation errors
 * with clang about structure having different sizes in C and C++.
 * Since these are all zero-sized arrays, we can omit the "union" wrapper for
 * C++ builds, removing the warning.
 */
#ifndef __cplusplus
	/**< Implementation specific opaque value.
	 * An implementation may use this field to hold implementation specific
	 * value to share between dequeue and enqueue operation.
	 * The application should not modify this field.
	 */
	union __rte_aligned(16) {
#endif
		struct rte_mbuf *mbufs[0];
		void *ptrs[0];
		uint64_t u64s[0];
#ifndef __cplusplus
	};
#endif
	/**< Start of the vector array union. Depending upon the event type the
	 * vector array can be an array of mbufs or pointers or opaque u64
	 * values.
	 */
};

/* Scheduler type definitions */
#define RTE_SCHED_TYPE_ORDERED          0
/**< Ordered scheduling
 *
 * Events from an ordered flow of an event queue can be scheduled to multiple
 * ports for concurrent processing while maintaining the original event order,
 * i.e. the order in which they were first enqueued to that queue.
 * This scheme allows events pertaining to the same, potentially large, flow to
 * be processed in parallel on multiple cores without incurring any
 * application-level order restoration logic overhead.
 *
 * After events are dequeued from a set of ports, as those events are re-enqueued
 * to another queue (with the op field set to @ref RTE_EVENT_OP_FORWARD), the event
 * device restores the original event order - including events returned from all
 * ports in the set - before the events are placed on the destination queue,
 * for subsequent scheduling to ports.
 *
 * Any events not forwarded i.e. dropped explicitly via RELEASE or implicitly
 * released by the next dequeue operation on a port, are skipped by the reordering
 * stage and do not affect the reordering of other returned events.
 *
 * Any NEW events sent on a port are not ordered with respect to FORWARD events sent
 * on the same port, since they have no original event order. They also are not
 * ordered with respect to NEW events enqueued on other ports.
 * However, NEW events to the same destination queue from the same port are guaranteed
 * to be enqueued in the order they were submitted via rte_event_enqueue_burst().
 *
 * NOTE:
 *   In restoring event order of forwarded events, the eventdev API guarantees that
 *   all events from the same flow (i.e. same @ref rte_event.flow_id,
 *   @ref rte_event.priority and @ref rte_event.queue_id) will be put in the original
 *   order before being forwarded to the destination queue.
 *   Some eventdevs may implement stricter ordering to achieve this aim,
 *   for example, restoring the order across *all* flows dequeued from the same ORDERED
 *   queue.
 *
 * @see rte_event_queue_setup(), rte_event_dequeue_burst(), RTE_EVENT_OP_RELEASE
 */

#define RTE_SCHED_TYPE_ATOMIC           1
/**< Atomic scheduling
 *
 * Events from an atomic flow, identified by a combination of @ref rte_event.flow_id,
 * @ref rte_event.queue_id and @ref rte_event.priority, can be scheduled only to a
 * single port at a time. The port is guaranteed to have exclusive (atomic)
 * access to the associated flow context, which enables the user to avoid SW
 * synchronization. Atomic flows also maintain event ordering
 * since only one port at a time can process events from each flow of an
 * event queue, and events within a flow are not reordered within the scheduler.
 *
 * An atomic flow is locked to a port when events from that flow are first
 * scheduled to that port. That lock remains in place until the
 * application calls rte_event_dequeue_burst() from the same port,
 * which implicitly releases the lock (if @ref RTE_EVENT_PORT_CFG_DISABLE_IMPL_REL flag is not set).
 * User may allow the scheduler to release the lock earlier than that by invoking
 * rte_event_enqueue_burst() with RTE_EVENT_OP_RELEASE operation for each event from that flow.
 *
 * NOTE: Where multiple events from the same queue and atomic flow are scheduled to a port,
 * the lock for that flow is only released once the last event from the flow is released,
 * or forwarded to another queue. So long as there is at least one event from an atomic
 * flow scheduled to a port/core (including any events in the port's dequeue queue, not yet read
 * by the application), that port will hold the synchronization lock for that flow.
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
#define RTE_EVENT_TYPE_DMADEV           0x5
/**< The event generated from dma subsystem */
#define RTE_EVENT_TYPE_VECTOR           0x8
/**< Indicates that event is a vector.
 * All vector event types should be a logical OR of EVENT_TYPE_VECTOR.
 * This simplifies the pipeline design as one can split processing the events
 * between vector events and normal event across event types.
 * Example:
 *	if (ev.event_type & RTE_EVENT_TYPE_VECTOR) {
 *		// Classify and handle vector event.
 *	} else {
 *		// Classify and handle event.
 *	}
 */
#define RTE_EVENT_TYPE_ETHDEV_VECTOR                                           \
	(RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_ETHDEV)
/**< The event vector generated from ethdev subsystem */
#define RTE_EVENT_TYPE_CPU_VECTOR (RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CPU)
/**< The event vector generated from cpu for pipelining. */
#define RTE_EVENT_TYPE_ETH_RX_ADAPTER_VECTOR                                   \
	(RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_ETH_RX_ADAPTER)
/**< The event vector generated from eth Rx adapter. */
#define RTE_EVENT_TYPE_CRYPTODEV_VECTOR                                        \
	(RTE_EVENT_TYPE_VECTOR | RTE_EVENT_TYPE_CRYPTODEV)
/**< The event vector generated from cryptodev adapter. */

#define RTE_EVENT_TYPE_MAX              0x10
/**< Maximum number of event types */

/* Event enqueue operations */
#define RTE_EVENT_OP_NEW                0
/**< The @ref rte_event.op field must be set to this operation type to inject a new event,
 * i.e. one not previously dequeued, into the event device, to be scheduled
 * for processing.
 */
#define RTE_EVENT_OP_FORWARD            1
/**< The application must set the @ref rte_event.op field to this operation type to return a
 * previously dequeued event to the event device to be scheduled for further processing.
 *
 * This event *must* be enqueued to the same port that the
 * event to be forwarded was dequeued from.
 *
 * The event's fields, including (but not limited to) flow_id, scheduling type,
 * destination queue, and event payload e.g. mbuf pointer, may all be updated as
 * desired by the application, but the @ref rte_event.impl_opaque field must
 * be kept to the same value as was present when the event was dequeued.
 */
#define RTE_EVENT_OP_RELEASE            2
/**< Release the flow context associated with the schedule type.
 *
 * If current flow's scheduler type method is @ref RTE_SCHED_TYPE_ATOMIC
 * then this operation type hints the scheduler that the user has completed critical
 * section processing for this event in the current atomic context, and that the
 * scheduler may unlock any atomic locks held for this event.
 * If this is the last event from an atomic flow, i.e. all flow locks are released
 * (see @ref RTE_SCHED_TYPE_ATOMIC for details), the scheduler is now allowed to
 * schedule events from that flow from to another port.
 * However, the atomic locks may be still held until the next rte_event_dequeue_burst()
 * call; enqueuing an event with opt type @ref RTE_EVENT_OP_RELEASE is a hint only,
 * allowing the scheduler to release the atomic locks early, but not requiring it to do so.
 *
 * Early atomic lock release may increase parallelism and thus system
 * performance, but the user needs to design carefully the split into critical
 * vs non-critical sections.
 *
 * If current flow's scheduler type method is @ref RTE_SCHED_TYPE_ORDERED
 * then this operation type informs the scheduler that the current event has
 * completed processing and will not be returned to the scheduler, i.e.
 * it has been dropped, and so the reordering context for that event
 * should be considered filled.
 *
 * Events with this operation type must only be enqueued to the same port that the
 * event to be released was dequeued from. The @ref rte_event.impl_opaque
 * field in the release event must have the same value as that in the original dequeued event.
 *
 * If a dequeued event is re-enqueued with operation type of @ref RTE_EVENT_OP_RELEASE,
 * then any subsequent enqueue of that event - or a copy of it - must be done as event of type
 * @ref RTE_EVENT_OP_NEW, not @ref RTE_EVENT_OP_FORWARD. This is because any context for
 * the originally dequeued event, i.e. atomic locks, or reorder buffer entries, will have
 * been removed or invalidated by the release operation.
 */

/**
 * The generic *rte_event* structure to hold the event attributes
 * for dequeue and enqueue operation
 */
struct rte_event {
	/* WORD0 */
	union {
		uint64_t event;
		/** Event attributes for dequeue or enqueue operation */
		struct {
			uint32_t flow_id:20;
			/**< Target flow identifier for the enqueue and dequeue operation.
			 *
			 * For @ref RTE_SCHED_TYPE_ATOMIC, this field is used to identify a
			 * flow for atomicity within a queue & priority level, such that events
			 * from each individual flow will only be scheduled to one port at a time.
			 *
			 * This field is preserved between enqueue and dequeue when
			 * a device reports the @ref RTE_EVENT_DEV_CAP_CARRY_FLOW_ID
			 * capability. Otherwise the value is implementation dependent
			 * on dequeue.
			 */
			uint32_t sub_event_type:8;
			/**< Sub-event types based on the event source.
			 *
			 * This field is preserved between enqueue and dequeue.
			 *
			 * @see RTE_EVENT_TYPE_CPU
			 */
			uint32_t event_type:4;
			/**< Event type to classify the event source. (RTE_EVENT_TYPE_*)
			 *
			 * This field is preserved between enqueue and dequeue
			 */
			uint8_t op:2;
			/**< The type of event enqueue operation - new/forward/ etc.
			 *
			 * This field is *not* preserved across an instance
			 * and is implementation dependent on dequeue.
			 *
			 * @see RTE_EVENT_OP_NEW
			 * @see RTE_EVENT_OP_FORWARD
			 * @see RTE_EVENT_OP_RELEASE
			 */
			uint8_t rsvd:4;
			/**< Reserved for future use.
			 *
			 * Should be set to zero when initializing event structures.
			 *
			 * When forwarding or releasing existing events dequeued from the scheduler,
			 * this field can be ignored.
			 */
			uint8_t sched_type:2;
			/**< Scheduler synchronization type (RTE_SCHED_TYPE_*)
			 * associated with flow id on a given event queue
			 * for the enqueue and dequeue operation.
			 *
			 * This field is used to determine the scheduling type
			 * for events sent to queues where @ref RTE_EVENT_QUEUE_CFG_ALL_TYPES
			 * is configured.
			 * For queues where only a single scheduling type is available,
			 * this field must be set to match the configured scheduling type.
			 *
			 * This field is preserved between enqueue and dequeue.
			 *
			 * @see RTE_SCHED_TYPE_ORDERED
			 * @see RTE_SCHED_TYPE_ATOMIC
			 * @see RTE_SCHED_TYPE_PARALLEL
			 */
			uint8_t queue_id;
			/**< Targeted event queue identifier for the enqueue or
			 * dequeue operation.
			 * The value must be less than @ref rte_event_dev_config.nb_event_queues
			 * which was previously supplied to rte_event_dev_configure().
			 *
			 * This field is preserved between enqueue on dequeue.
			 */
			uint8_t priority;
			/**< Event priority relative to other events in the
			 * event queue. The requested priority should in the
			 * range of  [@ref RTE_EVENT_DEV_PRIORITY_HIGHEST,
			 * @ref RTE_EVENT_DEV_PRIORITY_LOWEST].
			 *
			 * The implementation shall normalize the requested
			 * priority to supported priority value.
			 * [For devices with where the supported priority range is a power-of-2, the
			 * normalization will be done via bit-shifting, so only the highest
			 * log2(num_priorities) bits will be used by the event device]
			 *
			 * Valid when the device has @ref RTE_EVENT_DEV_CAP_EVENT_QOS capability
			 * and this field is preserved between enqueue and dequeue,
			 * though with possible loss of precision due to normalization and
			 * subsequent de-normalization. (For example, if a device only supports 8
			 * priority levels, only the high 3 bits of this field will be
			 * used by that device, and hence only the value of those 3 bits are
			 * guaranteed to be preserved between enqueue and dequeue.)
			 *
			 * Ignored when device does not support @ref RTE_EVENT_DEV_CAP_EVENT_QOS
			 * capability, and it is implementation dependent if this field is preserved
			 * between enqueue and dequeue.
			 */
			uint8_t impl_opaque;
			/**< Opaque field for event device use.
			 *
			 * An event driver implementation may use this field to hold an
			 * implementation specific value to share between
			 * dequeue and enqueue operation.
			 *
			 * The application must not modify this field.
			 * Its value is implementation dependent on dequeue,
			 * and must be returned unmodified on enqueue when
			 * op type is @ref RTE_EVENT_OP_FORWARD or @ref RTE_EVENT_OP_RELEASE.
			 * This field is ignored on events with op type
			 * @ref RTE_EVENT_OP_NEW.
			 */
		};
	};
	/* WORD1 */
	union {
		uint64_t u64;
		/**< Opaque 64-bit value */
		void *event_ptr;
		/**< Opaque event pointer */
		struct rte_mbuf *mbuf;
		/**< mbuf pointer if dequeued event is associated with mbuf */
		struct rte_event_vector *vec;
		/**< Event vector pointer. */
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
 * to the adapter using the ev.flow_id member.
 * @see struct rte_event_eth_rx_adapter_queue_conf::ev
 * @see struct rte_event_eth_rx_adapter_queue_conf::rx_queue_flags
 */
#define RTE_EVENT_ETH_RX_ADAPTER_CAP_EVENT_VECTOR	0x8
/**< Adapter supports event vectorization per ethdev. */

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
 */
int
rte_event_eth_rx_adapter_caps_get(uint8_t dev_id, uint16_t eth_port_id,
				uint32_t *caps);

#define RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT RTE_BIT32(0)
/**< This flag is set when the timer mechanism is in HW. */

#define RTE_EVENT_TIMER_ADAPTER_CAP_PERIODIC      RTE_BIT32(1)
/**< This flag is set if periodic mode is supported. */

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
int
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
/**< Flag indicates HW/SW supports a mechanism to store and retrieve
 * the private data information along with the crypto session.
 */

#define RTE_EVENT_CRYPTO_ADAPTER_CAP_EVENT_VECTOR   0x10
/**< Flag indicates HW is capable of aggregating processed
 * crypto operations into rte_event_vector.
 */

/**
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
 */
int
rte_event_crypto_adapter_caps_get(uint8_t dev_id, uint8_t cdev_id,
				  uint32_t *caps);

/* DMA adapter capability bitmap flag */
#define RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_NEW 0x1
/**< Flag indicates HW is capable of generating events in
 * RTE_EVENT_OP_NEW enqueue operation. DMADEV will send
 * packets to the event device as new events using an
 * internal event port.
 */

#define RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD 0x2
/**< Flag indicates HW is capable of generating events in
 * RTE_EVENT_OP_FORWARD enqueue operation. DMADEV will send
 * packets to the event device as forwarded event using an
 * internal event port.
 */

#define RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND 0x4
/**< Flag indicates HW is capable of mapping DMA vchan to event queue. */

/**
 * Retrieve the event device's DMA adapter capabilities for the
 * specified dmadev device
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param dmadev_id
 *   The identifier of the dmadev device.
 *
 * @param[out] caps
 *   A pointer to memory filled with event adapter capabilities.
 *   It is expected to be pre-allocated & initialized by caller.
 *
 * @return
 *   - 0: Success, driver provides event adapter capabilities for the
 *     dmadev device.
 *   - <0: Error code returned by the driver function.
 *
 */
__rte_experimental
int
rte_event_dma_adapter_caps_get(uint8_t dev_id, uint8_t dmadev_id, uint32_t *caps);

/* Ethdev Tx adapter capability bitmap flags */
#define RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT	0x1
/**< This flag is sent when the PMD supports a packet transmit callback
 */
#define RTE_EVENT_ETH_TX_ADAPTER_CAP_EVENT_VECTOR	0x2
/**< Indicates that the Tx adapter is capable of handling event vector of
 * mbufs.
 */

/**
 * Retrieve the event device's eth Tx adapter capabilities
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param eth_port_id
 *   The identifier of the ethernet device.
 *
 * @param[out] caps
 *   A pointer to memory filled with eth Tx adapter capabilities.
 *
 * @return
 *   - 0: Success, driver provides eth Tx adapter capabilities.
 *   - <0: Error code returned by the driver function.
 */
int
rte_event_eth_tx_adapter_caps_get(uint8_t dev_id, uint16_t eth_port_id,
				uint32_t *caps);

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
 */
int
rte_event_dequeue_timeout_ticks(uint8_t dev_id, uint64_t ns,
					uint64_t *timeout_ticks);

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
 * When the value of ``rte_event_dev_info::max_profiles_per_port`` is greater
 * than or equal to one, this function links the event queues to the default
 * profile_id i.e. profile_id 0 of the event port.
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
 * (EDQUOT) Quota exceeded(Application tried to link the queue configured with
 *  RTE_EVENT_QUEUE_CFG_SINGLE_LINK to more than one event ports)
 * (EINVAL) Invalid parameter
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
 * The unlink call issues an async request to disable the event port *port_id*
 * from receiving events from the specified event queue *queue_id*.
 * Event queue(s) to event port unlink establishment can be changed at runtime
 * without re-configuring the device.
 *
 * When the value of ``rte_event_dev_info::max_profiles_per_port`` is greater
 * than or equal to one, this function unlinks the event queues from the default
 * profile identifier i.e. profile 0 of the event port.
 *
 * @see rte_event_port_unlinks_in_progress() to poll for completed unlinks.
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
 * The number of unlinks successfully requested. The return value can be less
 * than the value of the *nb_unlinks* parameter when the implementation has the
 * limitation on specific queue to port unlink establishment or
 * if invalid parameters are specified.
 * If the return value is less than *nb_unlinks*, the remaining queues at the
 * end of queues[] are not unlinked, and the caller has to take care of them.
 * If return value is less than *nb_unlinks* then implementation shall update
 * the rte_errno accordingly, Possible rte_errno values are
 * (EINVAL) Invalid parameter
 */
int
rte_event_port_unlink(uint8_t dev_id, uint8_t port_id,
		      uint8_t queues[], uint16_t nb_unlinks);

/**
 * Link multiple source event queues supplied in *queues* to the destination
 * event port designated by its *port_id* with associated profile identifier
 * supplied in *profile_id* with service priorities supplied in *priorities*
 * on the event device designated by its *dev_id*.
 *
 * If *profile_id* is set to 0 then, the links created by the call `rte_event_port_link`
 * will be overwritten.
 *
 * Event ports by default use profile_id 0 unless it is changed using the
 * call ``rte_event_port_profile_switch()``.
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
 * @param profile_id
 *   The profile identifier associated with the links between event queues and
 *   event port. Should be less than the max capability reported by
 *   ``rte_event_dev_info::max_profiles_per_port``
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
 * (EDQUOT) Quota exceeded(Application tried to link the queue configured with
 *  RTE_EVENT_QUEUE_CFG_SINGLE_LINK to more than one event ports)
 * (EINVAL) Invalid parameter
 *
 */
__rte_experimental
int
rte_event_port_profile_links_set(uint8_t dev_id, uint8_t port_id, const uint8_t queues[],
				 const uint8_t priorities[], uint16_t nb_links, uint8_t profile_id);

/**
 * Unlink multiple source event queues supplied in *queues* that belong to profile
 * designated by *profile_id* from the destination event port designated by its
 * *port_id* on the event device designated by its *dev_id*.
 *
 * If *profile_id* is set to 0 i.e., the default profile then, then this function
 * will act as ``rte_event_port_unlink``.
 *
 * The unlink call issues an async request to disable the event port *port_id*
 * from receiving events from the specified event queue *queue_id*.
 * Event queue(s) to event port unlink establishment can be changed at runtime
 * without re-configuring the device.
 *
 * @see rte_event_port_unlinks_in_progress() to poll for completed unlinks.
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
 * @param profile_id
 *   The profile identifier associated with the links between event queues and
 *   event port. Should be less than the max capability reported by
 *   ``rte_event_dev_info::max_profiles_per_port``
 *
 * @return
 * The number of unlinks successfully requested. The return value can be less
 * than the value of the *nb_unlinks* parameter when the implementation has the
 * limitation on specific queue to port unlink establishment or
 * if invalid parameters are specified.
 * If the return value is less than *nb_unlinks*, the remaining queues at the
 * end of queues[] are not unlinked, and the caller has to take care of them.
 * If return value is less than *nb_unlinks* then implementation shall update
 * the rte_errno accordingly, Possible rte_errno values are
 * (EINVAL) Invalid parameter
 *
 */
__rte_experimental
int
rte_event_port_profile_unlink(uint8_t dev_id, uint8_t port_id, uint8_t queues[],
			      uint16_t nb_unlinks, uint8_t profile_id);

/**
 * Returns the number of unlinks in progress.
 *
 * This function provides the application with a method to detect when an
 * unlink has been completed by the implementation.
 *
 * @see rte_event_port_unlink() to issue unlink requests.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param port_id
 *   Event port identifier to select port to check for unlinks in progress.
 *
 * @return
 * The number of unlinks that are in progress. A return of zero indicates that
 * there are no outstanding unlink requests. A positive return value indicates
 * the number of unlinks that are in progress, but are not yet complete.
 * A negative return value indicates an error, -EINVAL indicates an invalid
 * parameter passed for *dev_id* or *port_id*.
 */
int
rte_event_port_unlinks_in_progress(uint8_t dev_id, uint8_t port_id);

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
 */
int
rte_event_port_links_get(uint8_t dev_id, uint8_t port_id,
			 uint8_t queues[], uint8_t priorities[]);

/**
 * Retrieve the list of source event queues and its service priority
 * associated to a *profile_id* and linked to the destination event port
 * designated by its *port_id* on the event device designated by its *dev_id*.
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
 * @param profile_id
 *   The profile identifier associated with the links between event queues and
 *   event port. Should be less than the max capability reported by
 *   ``rte_event_dev_info::max_profiles_per_port``
 *
 * @return
 * The number of links established on the event port designated by its
 *  *port_id*.
 * - <0 on failure.
 */
__rte_experimental
int
rte_event_port_profile_links_get(uint8_t dev_id, uint8_t port_id, uint8_t queues[],
				 uint8_t priorities[], uint8_t profile_id);

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
			       uint64_t *ids,
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
 *   by using rte_event_dev_xstats_by_name_get().
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
			 const uint64_t ids[],
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
 *   requests for the stat can be got using rte_event_dev_xstats_get, which will
 *   be faster as it doesn't need to scan a list of names for the stat.
 *   If the stat cannot be found, the id returned will be (unsigned)-1.
 * @return
 *   - positive value or zero: the stat value
 *   - negative value: -EINVAL if stat not found, -ENOTSUP if not supported.
 */
uint64_t
rte_event_dev_xstats_by_name_get(uint8_t dev_id, const char *name,
				 uint64_t *id);

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
			   const uint64_t ids[],
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

/**
 * Get the memory required per event vector based on the number of elements per
 * vector.
 * This should be used to create the mempool that holds the event vectors.
 *
 * @param name
 *   The name of the vector pool.
 * @param n
 *   The number of elements in the mbuf pool.
 * @param cache_size
 *   Size of the per-core object cache. See rte_mempool_create() for
 *   details.
 * @param nb_elem
 *   The number of elements that a single event vector should be able to hold.
 * @param socket_id
 *   The socket identifier where the memory should be allocated. The
 *   value can be *SOCKET_ID_ANY* if there is no NUMA constraint for the
 *   reserved zone
 *
 * @return
 *   The pointer to the newly allocated mempool, on success. NULL on error
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - EINVAL - cache size provided is too large, or priv_size is not aligned.
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 *    - ENAMETOOLONG - mempool name requested is too long.
 */
struct rte_mempool *
rte_event_vector_pool_create(const char *name, unsigned int n,
			     unsigned int cache_size, uint16_t nb_elem,
			     int socket_id);

#include <rte_eventdev_core.h>

#ifdef __cplusplus
extern "C" {
#endif

static __rte_always_inline uint16_t
__rte_event_enqueue_burst(uint8_t dev_id, uint8_t port_id,
			  const struct rte_event ev[], uint16_t nb_events,
			  const event_enqueue_burst_t fn)
{
	const struct rte_event_fp_ops *fp_ops;
	void *port;

	fp_ops = &rte_event_fp_ops[dev_id];
	port = fp_ops->data[port_id];
#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	if (dev_id >= RTE_EVENT_MAX_DEVS ||
	    port_id >= RTE_EVENT_MAX_PORTS_PER_DEV) {
		rte_errno = EINVAL;
		return 0;
	}

	if (port == NULL) {
		rte_errno = EINVAL;
		return 0;
	}
#endif
	rte_eventdev_trace_enq_burst(dev_id, port_id, ev, nb_events, (void *)fn);

	return fn(port, ev, nb_events);
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
 *   rte_event_port_attr_get(...RTE_EVENT_PORT_ATTR_ENQ_DEPTH...)
 *   available for this port.
 *
 * @return
 *   The number of event objects actually enqueued on the event device. The
 *   return value can be less than the value of the *nb_events* parameter when
 *   the event devices queue is full or if invalid parameters are specified in a
 *   *rte_event*. If the return value is less than *nb_events*, the remaining
 *   events at the end of ev[] are not consumed and the caller has to take care
 *   of them, and rte_errno is set accordingly. Possible errno values include:
 *   - EINVAL   The port ID is invalid, device ID is invalid, an event's queue
 *              ID is invalid, or an event's sched type doesn't match the
 *              capabilities of the destination queue.
 *   - ENOSPC   The event port was backpressured and unable to enqueue
 *              one or more events. This error code is only applicable to
 *              closed systems.
 * @see rte_event_port_attr_get(), RTE_EVENT_PORT_ATTR_ENQ_DEPTH
 */
static inline uint16_t
rte_event_enqueue_burst(uint8_t dev_id, uint8_t port_id,
			const struct rte_event ev[], uint16_t nb_events)
{
	const struct rte_event_fp_ops *fp_ops;

	fp_ops = &rte_event_fp_ops[dev_id];
	return __rte_event_enqueue_burst(dev_id, port_id, ev, nb_events,
					 fp_ops->enqueue_burst);
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
 *   rte_event_port_attr_get(...RTE_EVENT_PORT_ATTR_ENQ_DEPTH...)
 *   available for this port.
 *
 * @return
 *   The number of event objects actually enqueued on the event device. The
 *   return value can be less than the value of the *nb_events* parameter when
 *   the event devices queue is full or if invalid parameters are specified in a
 *   *rte_event*. If the return value is less than *nb_events*, the remaining
 *   events at the end of ev[] are not consumed and the caller has to take care
 *   of them, and rte_errno is set accordingly. Possible errno values include:
 *   - EINVAL   The port ID is invalid, device ID is invalid, an event's queue
 *              ID is invalid, or an event's sched type doesn't match the
 *              capabilities of the destination queue.
 *   - ENOSPC   The event port was backpressured and unable to enqueue
 *              one or more events. This error code is only applicable to
 *              closed systems.
 * @see rte_event_port_attr_get(), RTE_EVENT_PORT_ATTR_ENQ_DEPTH
 * @see rte_event_enqueue_burst()
 */
static inline uint16_t
rte_event_enqueue_new_burst(uint8_t dev_id, uint8_t port_id,
			    const struct rte_event ev[], uint16_t nb_events)
{
	const struct rte_event_fp_ops *fp_ops;

	fp_ops = &rte_event_fp_ops[dev_id];
	return __rte_event_enqueue_burst(dev_id, port_id, ev, nb_events,
					 fp_ops->enqueue_new_burst);
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
 *   rte_event_port_attr_get(...RTE_EVENT_PORT_ATTR_ENQ_DEPTH...)
 *   available for this port.
 *
 * @return
 *   The number of event objects actually enqueued on the event device. The
 *   return value can be less than the value of the *nb_events* parameter when
 *   the event devices queue is full or if invalid parameters are specified in a
 *   *rte_event*. If the return value is less than *nb_events*, the remaining
 *   events at the end of ev[] are not consumed and the caller has to take care
 *   of them, and rte_errno is set accordingly. Possible errno values include:
 *   - EINVAL   The port ID is invalid, device ID is invalid, an event's queue
 *              ID is invalid, or an event's sched type doesn't match the
 *              capabilities of the destination queue.
 *   - ENOSPC   The event port was backpressured and unable to enqueue
 *              one or more events. This error code is only applicable to
 *              closed systems.
 * @see rte_event_port_attr_get(), RTE_EVENT_PORT_ATTR_ENQ_DEPTH
 * @see rte_event_enqueue_burst()
 */
static inline uint16_t
rte_event_enqueue_forward_burst(uint8_t dev_id, uint8_t port_id,
				const struct rte_event ev[], uint16_t nb_events)
{
	const struct rte_event_fp_ops *fp_ops;

	fp_ops = &rte_event_fp_ops[dev_id];
	return __rte_event_enqueue_burst(dev_id, port_id, ev, nb_events,
					 fp_ops->enqueue_forward_burst);
}

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
	const struct rte_event_fp_ops *fp_ops;
	void *port;

	fp_ops = &rte_event_fp_ops[dev_id];
	port = fp_ops->data[port_id];
#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	if (dev_id >= RTE_EVENT_MAX_DEVS ||
	    port_id >= RTE_EVENT_MAX_PORTS_PER_DEV) {
		rte_errno = EINVAL;
		return 0;
	}

	if (port == NULL) {
		rte_errno = EINVAL;
		return 0;
	}
#endif
	rte_eventdev_trace_deq_burst(dev_id, port_id, ev, nb_events);

	return (fp_ops->dequeue_burst)(port, ev, nb_events, timeout_ticks);
}

#define RTE_EVENT_DEV_MAINT_OP_FLUSH          (1 << 0)
/**< Force an immediately flush of any buffered events in the port,
 * potentially at the cost of additional overhead.
 *
 * @see rte_event_maintain()
 */

/**
 * Maintain an event device.
 *
 * This function is only relevant for event devices which do not have
 * the @ref RTE_EVENT_DEV_CAP_MAINTENANCE_FREE flag set. Such devices
 * require an application thread using a particular port to
 * periodically call rte_event_maintain() on that port during periods
 * which it is neither attempting to enqueue events to nor dequeue
 * events from the port. rte_event_maintain() is a low-overhead
 * function and should be called at a high rate (e.g., in the
 * application's poll loop).
 *
 * No port may be left unmaintained.
 *
 * At the application thread's convenience, rte_event_maintain() may
 * (but is not required to) be called even during periods when enqueue
 * or dequeue functions are being called, at the cost of a slight
 * increase in overhead.
 *
 * rte_event_maintain() may be called on event devices which have set
 * @ref RTE_EVENT_DEV_CAP_MAINTENANCE_FREE, in which case it is a
 * no-operation.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The identifier of the event port.
 * @param op
 *   0, or @ref RTE_EVENT_DEV_MAINT_OP_FLUSH.
 * @return
 *  - 0 on success.
 *  - -EINVAL if *dev_id*,  *port_id*, or *op* is invalid.
 *
 * @see RTE_EVENT_DEV_CAP_MAINTENANCE_FREE
 */
static inline int
rte_event_maintain(uint8_t dev_id, uint8_t port_id, int op)
{
	const struct rte_event_fp_ops *fp_ops;
	void *port;

	fp_ops = &rte_event_fp_ops[dev_id];
	port = fp_ops->data[port_id];
#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	if (dev_id >= RTE_EVENT_MAX_DEVS ||
	    port_id >= RTE_EVENT_MAX_PORTS_PER_DEV)
		return -EINVAL;

	if (port == NULL)
		return -EINVAL;

	if (op & (~RTE_EVENT_DEV_MAINT_OP_FLUSH))
		return -EINVAL;
#endif
	rte_eventdev_trace_maintain(dev_id, port_id, op);

	if (fp_ops->maintain != NULL)
		fp_ops->maintain(port, op);

	return 0;
}

/**
 * Change the active profile on an event port.
 *
 * This function is used to change the current active profile on an event port
 * when multiple link profiles are configured on an event port through the
 * function call ``rte_event_port_profile_links_set``.
 *
 * On the subsequent ``rte_event_dequeue_burst`` call, only the event queues
 * that were associated with the newly active profile will participate in
 * scheduling.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The identifier of the event port.
 * @param profile_id
 *   The identifier of the profile.
 * @return
 *  - 0 on success.
 *  - -EINVAL if *dev_id*,  *port_id*, or *profile_id* is invalid.
 */
static inline uint8_t
rte_event_port_profile_switch(uint8_t dev_id, uint8_t port_id, uint8_t profile_id)
{
	const struct rte_event_fp_ops *fp_ops;
	void *port;

	fp_ops = &rte_event_fp_ops[dev_id];
	port = fp_ops->data[port_id];

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	if (dev_id >= RTE_EVENT_MAX_DEVS ||
	    port_id >= RTE_EVENT_MAX_PORTS_PER_DEV)
		return -EINVAL;

	if (port == NULL)
		return -EINVAL;

	if (profile_id >= RTE_EVENT_MAX_PROFILES_PER_PORT)
		return -EINVAL;
#endif
	rte_eventdev_trace_port_profile_switch(dev_id, port_id, profile_id);

	return fp_ops->profile_switch(port, profile_id);
}

/**
 * Modify the pre-schedule type to use on an event port.
 *
 * This function is used to change the current pre-schedule type configured
 * on an event port, the pre-schedule type can be set to none to disable pre-scheduling.
 * This effects the subsequent ``rte_event_dequeue_burst`` call.
 * The event device should support RTE_EVENT_DEV_CAP_PER_PORT_PRESCHEDULE capability.
 *
 * To avoid fastpath capability checks if an event device does not support
 * RTE_EVENT_DEV_CAP_PER_PORT_PRESCHEDULE capability, then this function will
 * return -ENOTSUP.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The identifier of the event port.
 * @param type
 *   The preschedule type to use on the event port.
 * @return
 *  - 0 on success.
 *  - -EINVAL if *dev_id*,  *port_id*, or *type* is invalid.
 *  - -ENOTSUP if the device does not support per port preschedule capability.
 */
__rte_experimental
static inline int
rte_event_port_preschedule_modify(uint8_t dev_id, uint8_t port_id,
				  enum rte_event_dev_preschedule_type type)
{
	const struct rte_event_fp_ops *fp_ops;
	void *port;

	fp_ops = &rte_event_fp_ops[dev_id];
	port = fp_ops->data[port_id];

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	if (dev_id >= RTE_EVENT_MAX_DEVS || port_id >= RTE_EVENT_MAX_PORTS_PER_DEV)
		return -EINVAL;

	if (port == NULL)
		return -EINVAL;
#endif
	rte_eventdev_trace_port_preschedule_modify(dev_id, port_id, type);

	return fp_ops->preschedule_modify(port, type);
}

/**
 * Provide a hint to the event device to pre-schedule events to event port .
 *
 * Hint the event device to pre-schedule events to the event port.
 * The call doesn't not guarantee that the events will be pre-scheduleed.
 * The call doesn't release the flow context currently held by the event port.
 * The event device should support RTE_EVENT_DEV_CAP_PRESCHEDULE_EXPLICIT capability.
 *
 * When pre-scheduling is enabled at an event device/port level or if
 * the capability is not supported, then the hint is ignored.
 *
 * Subsequent calls to rte_event_dequeue_burst() will dequeue the pre-schedule
 * events but pre-schedule operation is not issued again.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param port_id
 *   The identifier of the event port.
 * @param type
 *   The pre-schedule type to use on the event port.
 */
__rte_experimental
static inline void
rte_event_port_preschedule(uint8_t dev_id, uint8_t port_id,
			   enum rte_event_dev_preschedule_type type)
{
	const struct rte_event_fp_ops *fp_ops;
	void *port;

	fp_ops = &rte_event_fp_ops[dev_id];
	port = fp_ops->data[port_id];

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	if (dev_id >= RTE_EVENT_MAX_DEVS || port_id >= RTE_EVENT_MAX_PORTS_PER_DEV)
		return;
	if (port == NULL)
		return;
#endif
	rte_eventdev_trace_port_preschedule(dev_id, port_id, type);

	fp_ops->preschedule(port, type);
}
#ifdef __cplusplus
}
#endif

#endif /* _RTE_EVENTDEV_H_ */
