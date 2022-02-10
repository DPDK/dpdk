/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc.
 * Copyright(c) 2017-2018 Intel Corporation.
 * All rights reserved.
 */

#ifndef __RTE_EVENT_TIMER_ADAPTER_H__
#define __RTE_EVENT_TIMER_ADAPTER_H__

/**
 * @file
 *
 * RTE Event Timer Adapter
 *
 * An event timer adapter has the following abstract working model:
 *
 *                               timer_tick_ns
 *                                   +
 *                      +-------+    |
 *                      |       |    |
 *              +-------+ bkt 0 +----v---+
 *              |       |       |        |
 *              |       +-------+        |
 *          +---+---+                +---+---+  +---+---+---+---+
 *          |       |                |       |  |   |   |   |   |
 *          | bkt n |                | bkt 1 |<-> t0| t1| t2| tn|
 *          |       |                |       |  |   |   |   |   |
 *          +---+---+                +---+---+  +---+---+---+---+
 *              |     Timer adapter      |
 *          +---+---+                +---+---+
 *          |       |                |       |
 *          | bkt 4 |                | bkt 2 |<--- Current bucket
 *          |       |                |       |
 *          +---+---+                +---+---+
 *               |      +-------+       |
 *               |      |       |       |
 *               +------+ bkt 3 +-------+
 *                      |       |
 *                      +-------+
 *
 * - It has a virtual monotonically increasing 64-bit timer adapter clock based
 *   on *enum rte_event_timer_adapter_clk_src* clock source. The clock source
 *   could be a CPU clock, or a platform dependent external clock.
 *
 * - The application creates a timer adapter instance with given the clock
 *   source, the total number of event timers, and a resolution(expressed in ns)
 *   to traverse between the buckets.
 *
 * - Each timer adapter may have 0 to n buckets based on the configured
 *   max timeout(max_tmo_ns) and resolution(timer_tick_ns). Upon starting the
 *   timer adapter, the adapter starts ticking at *timer_tick_ns* resolution.
 *
 * - The application arms an event timer that will expire *timer_tick_ns*
 *   from now.
 *
 * - The application can cancel an armed timer and no timer expiry event will be
 *   generated.
 *
 * - If a timer expires then the library injects the timer expiry event in
 *   the designated event queue.
 *
 * - The timer expiry event will be received through *rte_event_dequeue_burst*.
 *
 * - The application frees the timer adapter instance.
 *
 * Multiple timer adapters can be created with a varying level of resolution
 * for various expiry use cases that run in parallel.
 *
 * Before using the timer adapter, the application has to create and configure
 * an event device along with the event port. Based on the event device
 * capability it might require creating an additional event port to be used
 * by the timer adapter.
 *
 * The application creates the event timer adapter using the
 * ``rte_event_timer_adapter_create()``. The event device id is passed to this
 * function, inside this function the event device capability is checked,
 * and if an in-built port is absent the application uses the default
 * function to create a new producer port.
 *
 * The application may also use the function
 * ``rte_event_timer_adapter_create_ext()`` to have granular control over
 * producer port creation in a case where the in-built port is absent.
 *
 * After creating the timer adapter, the application has to start it
 * using ``rte_event_timer_adapter_start()``. The buckets are traversed from
 * 0 to n; when the adapter ticks, the next bucket is visited. Each time,
 * the list per bucket is processed, and timer expiry events are sent to the
 * designated event queue.
 *
 * The application can arm one or more event timers using the
 * ``rte_event_timer_arm_burst()``. The *timeout_ticks* represents the number
 * of *timer_tick_ns* after which the timer has to expire. The timeout at
 * which the timers expire can be grouped or be independent of each
 * event timer instance. ``rte_event_timer_arm_tmo_tick_burst()`` addresses the
 * former case and ``rte_event_timer_arm_burst()`` addresses the latter case.
 *
 * The application can cancel the timers from expiring using the
 * ``rte_event_timer_cancel_burst()``.
 *
 * On the secondary process, ``rte_event_timer_adapter_lookup()`` can be used
 * to get the timer adapter pointer from its id and use it to invoke fastpath
 * operations such as arm and cancel.
 *
 * Some of the use cases of event timer adapter are Beacon Timers,
 * Generic SW Timeout, Wireless MAC Scheduling, 3G Frame Protocols,
 * Packet Scheduling, Protocol Retransmission Timers, Supervision Timers.
 * All these use cases require high resolution and low time drift.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_spinlock.h>
#include <rte_memory.h>

#include "rte_eventdev.h"
#include "rte_eventdev_trace_fp.h"

/**
 * Timer adapter clock source
 */
enum rte_event_timer_adapter_clk_src {
	RTE_EVENT_TIMER_ADAPTER_CPU_CLK,
	/**< Use CPU clock as the clock source. */
	RTE_EVENT_TIMER_ADAPTER_EXT_CLK0,
	/**< Platform dependent external clock source 0. */
	RTE_EVENT_TIMER_ADAPTER_EXT_CLK1,
	/**< Platform dependent external clock source 1. */
	RTE_EVENT_TIMER_ADAPTER_EXT_CLK2,
	/**< Platform dependent external clock source 2. */
	RTE_EVENT_TIMER_ADAPTER_EXT_CLK3,
	/**< Platform dependent external clock source 3. */
};

#define RTE_EVENT_TIMER_ADAPTER_F_ADJUST_RES	(1ULL << 0)
/**< The event timer adapter implementation may have constraints on the
 * resolution (timer_tick_ns) and maximum timer expiry timeout(max_tmo_ns)
 * based on the given timer adapter or system. If this flag is set, the
 * implementation adjusts the resolution and maximum timeout to the best
 * possible configuration. On successful timer adapter creation, the
 * application can get the configured resolution and max timeout with
 * ``rte_event_timer_adapter_get_info()``.
 *
 * @see struct rte_event_timer_adapter_info::min_resolution_ns
 * @see struct rte_event_timer_adapter_info::max_tmo_ns
 */
#define RTE_EVENT_TIMER_ADAPTER_F_SP_PUT	(1ULL << 1)
/**< ``rte_event_timer_arm_burst()`` API to be used in single producer mode.
 *
 * @see struct rte_event_timer_adapter_conf::flags
 */

#define RTE_EVENT_TIMER_ADAPTER_F_PERIODIC	(1ULL << 2)
/**< Flag to configure an event timer adapter in periodic mode; non-periodic
 * mode is the default. A timer will fire once or periodically until the timer
 * is cancelled based on the adapter mode.
 *
 * @see struct rte_event_timer_adapter_conf::flags
 */

/**
 * Timer adapter configuration structure
 */
struct rte_event_timer_adapter_conf {
	uint8_t event_dev_id;
	/**< Event device identifier */
	uint16_t timer_adapter_id;
	/**< Event timer adapter identifier */
	uint32_t socket_id;
	/**< Identifier of socket from which to allocate memory for adapter */
	enum rte_event_timer_adapter_clk_src clk_src;
	/**< Clock source for timer adapter */
	uint64_t timer_tick_ns;
	/**< Timer adapter resolution in ns */
	uint64_t max_tmo_ns;
	/**< Maximum timer timeout(expiry) in ns */
	uint64_t nb_timers;
	/**< Total number of timers per adapter */
	uint64_t flags;
	/**< Timer adapter config flags (RTE_EVENT_TIMER_ADAPTER_F_*) */
};

/**
 * Event timer adapter stats structure
 */
struct rte_event_timer_adapter_stats {
	uint64_t evtim_exp_count;
	/**< Number of event timers that have expired. */
	uint64_t ev_enq_count;
	/**< Eventdev enqueue count */
	uint64_t ev_inv_count;
	/**< Invalid expiry event count */
	uint64_t evtim_retry_count;
	/**< Event timer retry count */
	uint64_t adapter_tick_count;
	/**< Tick count for the adapter, at its resolution */
};

struct rte_event_timer_adapter;

/**
 * Callback function type for producer port creation.
 */
typedef int (*rte_event_timer_adapter_port_conf_cb_t)(uint16_t id,
						      uint8_t event_dev_id,
						      uint8_t *event_port_id,
						      void *conf_arg);

/**
 * Create an event timer adapter.
 *
 * This function must be invoked first before any other function in the API.
 *
 * @param conf
 *   The event timer adapter configuration structure.
 *
 * @return
 *   A pointer to the new allocated event timer adapter on success.
 *   NULL on error with rte_errno set appropriately.
 *   Possible rte_errno values include:
 *   - ERANGE: timer_tick_ns is not in supported range.
 *   - ENOMEM: unable to allocate sufficient memory for adapter instances
 *   - EINVAL: invalid event device identifier specified in config
 *   - ENOSPC: maximum number of adapters already created
 *   - EIO: event device reconfiguration and restart error.  The adapter
 *   reconfigures the event device with an additional port by default if it is
 *   required to use a service to manage timers. If the device had been started
 *   before this call, this error code indicates an error in restart following
 *   an error in reconfiguration, i.e., a combination of the two error codes.
 */
struct rte_event_timer_adapter *
rte_event_timer_adapter_create(const struct rte_event_timer_adapter_conf *conf);

/**
 * Create a timer adapter with the supplied callback.
 *
 * This function can be used to have a more granular control over the timer
 * adapter creation.  If a built-in port is absent, then the function uses the
 * callback provided to create and get the port id to be used as a producer
 * port.
 *
 * @param conf
 *   The timer adapter configuration structure
 * @param conf_cb
 *   The port config callback function.
 * @param conf_arg
 *   Opaque pointer to the argument for the callback function
 *
 * @return
 *   A pointer to the new allocated event timer adapter on success.
 *   NULL on error with rte_errno set appropriately.
 *   Possible rte_errno values include:
 *   - ERANGE: timer_tick_ns is not in supported range.
 *   - ENOMEM: unable to allocate sufficient memory for adapter instances
 *   - EINVAL: invalid event device identifier specified in config
 *   - ENOSPC: maximum number of adapters already created
 */
struct rte_event_timer_adapter *
rte_event_timer_adapter_create_ext(
		const struct rte_event_timer_adapter_conf *conf,
		rte_event_timer_adapter_port_conf_cb_t conf_cb,
		void *conf_arg);

/**
 * Timer adapter info structure.
 */
struct rte_event_timer_adapter_info {
	uint64_t min_resolution_ns;
	/**< Minimum timer adapter resolution in ns */
	uint64_t max_tmo_ns;
	/**< Maximum timer timeout(expire) in ns */
	struct rte_event_timer_adapter_conf conf;
	/**< Configured timer adapter attributes */
	uint32_t caps;
	/**< Event timer adapter capabilities */
	int16_t event_dev_port_id;
	/**< Event device port ID, if applicable */
};

/**
 * Retrieve the contextual information of an event timer adapter.
 *
 * @param adapter
 *   A pointer to the event timer adapter structure.
 *
 * @param[out] adapter_info
 *   A pointer to a structure of type *rte_event_timer_adapter_info* to be
 *   filled with the contextual information of the adapter.
 *
 * @return
 *   - 0: Success, driver updates the contextual information of the
 *   timer adapter
 *   - <0: Error code returned by the driver info get function.
 *   - -EINVAL: adapter identifier invalid
 *
 * @see RTE_EVENT_TIMER_ADAPTER_F_ADJUST_RES,
 *   struct rte_event_timer_adapter_info
 *
 */
int
rte_event_timer_adapter_get_info(
		const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer_adapter_info *adapter_info);

/**
 * Start a timer adapter.
 *
 * The adapter start step is the last one and consists of setting the timer
 * adapter to start accepting the timers and schedules to event queues.
 *
 * On success, all basic functions exported by the API (timer arm,
 * timer cancel and so on) can be invoked.
 *
 * @param adapter
 *   A pointer to the event timer adapter structure.
 *
 * @return
 *   - 0: Success, adapter started.
 *   - <0: Error code returned by the driver start function.
 *   - -EINVAL if adapter identifier invalid
 *   - -ENOENT if software adapter but no service core mapped
 *   - -ENOTSUP if software adapter and more than one service core mapped
 *   - -EALREADY if adapter has already been started
 *
 * @note
 *  The eventdev to which the event_timer_adapter is connected needs to
 *  be started before calling rte_event_timer_adapter_start().
 */
int
rte_event_timer_adapter_start(
		const struct rte_event_timer_adapter *adapter);

/**
 * Stop an event timer adapter.
 *
 * The adapter can be restarted with a call to
 * ``rte_event_timer_adapter_start()``.
 *
 * @param adapter
 *   A pointer to the event timer adapter structure.
 *
 * @return
 *   - 0: Success, adapter stopped.
 *   - <0: Error code returned by the driver stop function.
 *   - -EINVAL if adapter identifier invalid
 */
int
rte_event_timer_adapter_stop(const struct rte_event_timer_adapter *adapter);

/**
 * Lookup an event timer adapter using its identifier.
 *
 * If an event timer adapter was created in another process with the same
 * identifier, this function will locate its state and set up access to it
 * so that it can be used in this process.
 *
 * @param adapter_id
 *  The event timer adapter identifier.
 *
 * @return
 *  A pointer to the event timer adapter matching the identifier on success.
 *  NULL on error with rte_errno set appropriately.
 *  Possible rte_errno values include:
 *   - ENOENT - requested entry not available to return.
 */
struct rte_event_timer_adapter *
rte_event_timer_adapter_lookup(uint16_t adapter_id);

/**
 * Free an event timer adapter.
 *
 * Destroy an event timer adapter, freeing all resources.
 *
 * Before invoking this function, the application must wait for all the
 * armed timers to expire or cancel the outstanding armed timers.
 *
 * @param adapter
 *   A pointer to an event timer adapter structure.
 *
 * @return
 *   - 0: Successfully freed the event timer adapter resources.
 *   - <0: Failed to free the event timer adapter resources.
 *   - -EAGAIN:  adapter is busy; timers outstanding
 *   - -EBUSY: stop hasn't been called for this adapter yet
 *   - -EINVAL: adapter id invalid, or adapter invalid
 */
int
rte_event_timer_adapter_free(struct rte_event_timer_adapter *adapter);

/**
 * Retrieve the service ID of the event timer adapter. If the adapter doesn't
 * use an rte_service function, this function returns -ESRCH.
 *
 * @param adapter
 *   A pointer to an event timer adapter.
 *
 * @param [out] service_id
 *   A pointer to a uint32_t, to be filled in with the service id.
 *
 * @return
 *   - 0: Success
 *   - <0: Error code on failure
 *   - -ESRCH: the adapter does not require a service to operate
 */
int
rte_event_timer_adapter_service_id_get(struct rte_event_timer_adapter *adapter,
				       uint32_t *service_id);

/**
 * Retrieve statistics for an event timer adapter instance.
 *
 * @param adapter
 *   A pointer to an event timer adapter structure.
 * @param[out] stats
 *   A pointer to a structure to fill with statistics.
 *
 * @return
 *   - 0: Successfully retrieved.
 *   - <0: Failure; error code returned.
 */
int
rte_event_timer_adapter_stats_get(struct rte_event_timer_adapter *adapter,
		struct rte_event_timer_adapter_stats *stats);

/**
 * Reset statistics for an event timer adapter instance.
 *
 * @param adapter
 *   A pointer to an event timer adapter structure.
 *
 * @return
 *   - 0: Successfully reset;
 *   - <0: Failure; error code returned.
 */
int
rte_event_timer_adapter_stats_reset(struct rte_event_timer_adapter *adapter);

/**
 * Event timer state.
 */
enum rte_event_timer_state {
	RTE_EVENT_TIMER_NOT_ARMED	= 0,
	/**< Event timer not armed. */
	RTE_EVENT_TIMER_ARMED		= 1,
	/**< Event timer successfully armed. */
	RTE_EVENT_TIMER_CANCELED	= 2,
	/**< Event timer successfully canceled. */
	RTE_EVENT_TIMER_ERROR		= -1,
	/**< Generic event timer error. */
	RTE_EVENT_TIMER_ERROR_TOOEARLY	= -2,
	/**< Event timer timeout tick value is too small for the adapter to
	 * handle, given its configured resolution.
	 */
	RTE_EVENT_TIMER_ERROR_TOOLATE	= -3,
	/**< Event timer timeout tick is greater than the maximum timeout.*/
};

/**
 * The generic *rte_event_timer* structure to hold the event timer attributes
 * for arm and cancel operations.
 */
RTE_STD_C11
struct rte_event_timer {
	struct rte_event ev;
	/**<
	 * Expiry event attributes.  On successful event timer timeout,
	 * the following attributes will be used to inject the expiry event to
	 * the eventdev:
	 *  - event_queue_id: Targeted event queue id for expiry events.
	 *  - event_priority: Event priority of the event expiry event in the
	 *  event queue relative to other events.
	 *  - sched_type: Scheduling type of the expiry event.
	 *  - flow_id: Flow id of the expiry event.
	 *  - op: RTE_EVENT_OP_NEW
	 *  - event_type: RTE_EVENT_TYPE_TIMER
	 */
	uint64_t timeout_ticks;
	/**< Expiry timer ticks expressed in number of *timer_ticks_ns* from
	 * now.
	 * @see struct rte_event_timer_adapter_info::adapter_conf::timer_tick_ns
	 */
	uint64_t impl_opaque[2];
	/**< Implementation-specific opaque data.
	 * An event timer adapter implementation use this field to hold
	 * implementation specific values to share between the arm and cancel
	 * operations.  The application should not modify this field.
	 */
	enum rte_event_timer_state state;
	/**< State of the event timer. */
	uint8_t user_meta[0];
	/**< Memory to store user specific metadata.
	 * The event timer adapter implementation should not modify this area.
	 */
} __rte_cache_aligned;

typedef uint16_t (*rte_event_timer_arm_burst_t)(
		const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer **tims,
		uint16_t nb_tims);
/**< @internal Enable event timers to enqueue timer events upon expiry */
typedef uint16_t (*rte_event_timer_arm_tmo_tick_burst_t)(
		const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer **tims,
		uint64_t timeout_tick,
		uint16_t nb_tims);
/**< @internal Enable event timers with common expiration time */
typedef uint16_t (*rte_event_timer_cancel_burst_t)(
		const struct rte_event_timer_adapter *adapter,
		struct rte_event_timer **tims,
		uint16_t nb_tims);
/**< @internal Prevent event timers from enqueuing timer events */

/**
 * @internal Data structure associated with each event timer adapter.
 */
struct rte_event_timer_adapter {
	rte_event_timer_arm_burst_t arm_burst;
	/**< Pointer to driver arm_burst function. */
	rte_event_timer_arm_tmo_tick_burst_t arm_tmo_tick_burst;
	/**< Pointer to driver arm_tmo_tick_burst function. */
	rte_event_timer_cancel_burst_t cancel_burst;
	/**< Pointer to driver cancel function. */
	struct rte_event_timer_adapter_data *data;
	/**< Pointer to shared adapter data */
	const struct event_timer_adapter_ops *ops;
	/**< Functions exported by adapter driver */

	RTE_STD_C11
	uint8_t allocated : 1;
	/**< Flag to indicate that this adapter has been allocated */
} __rte_cache_aligned;

#define ADAPTER_VALID_OR_ERR_RET(adapter, retval) do {		\
	if (adapter == NULL || !adapter->allocated)		\
		return retval;					\
} while (0)

#define FUNC_PTR_OR_ERR_RET(func, errval) do { 			\
	if ((func) == NULL)					\
		return errval;					\
} while (0)

#define FUNC_PTR_OR_NULL_RET_WITH_ERRNO(func, errval) do { 	\
	if ((func) == NULL) {					\
		rte_errno = errval;				\
		return NULL;					\
	}							\
} while (0)

/**
 * Arm a burst of event timers with separate expiration timeout tick for each
 * event timer.
 *
 * Before calling this function, the application allocates
 * ``struct rte_event_timer`` objects from mempool or huge page backed
 * application buffers of desired size. On successful allocation,
 * application updates the `struct rte_event_timer`` attributes such as
 * expiry event attributes, timeout ticks from now.
 * This function submits the event timer arm requests to the event timer adapter
 * and on expiry, the events will be injected to designated event queue.
 * Timer expiry events will be generated once or periodically until cancellation
 * based on the adapter mode.
 *
 * @param adapter
 *   A pointer to an event timer adapter structure.
 * @param evtims
 *   Pointer to an array of objects of type *rte_event_timer* structure.
 * @param nb_evtims
 *   Number of event timers in the supplied array.
 *
 * @return
 *   The number of successfully armed event timers. The return value can be less
 *   than the value of the *nb_evtims* parameter. If the return value is less
 *   than *nb_evtims*, the remaining event timers at the end of *evtims*
 *   are not consumed, and the caller has to take care of them, and rte_errno
 *   is set accordingly. Possible errno values include:
 *   - EINVAL Invalid timer adapter, expiry event queue ID is invalid, or an
 *   expiry event's sched type doesn't match the capabilities of the
 *   destination event queue.
 *   - EAGAIN Specified timer adapter is not running
 *   - EALREADY A timer was encountered that was already armed
 *
 * @see RTE_EVENT_TIMER_ADAPTER_F_PERIODIC
 *
 */
static inline uint16_t
rte_event_timer_arm_burst(const struct rte_event_timer_adapter *adapter,
			  struct rte_event_timer **evtims,
			  uint16_t nb_evtims)
{
#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->arm_burst, -EINVAL);
#endif
	rte_eventdev_trace_timer_arm_burst(adapter, (void **)evtims,
		nb_evtims);
	return adapter->arm_burst(adapter, evtims, nb_evtims);
}

/**
 * Arm a burst of event timers with same expiration timeout tick.
 *
 * Provides the same functionality as ``rte_event_timer_arm_burst()``, except
 * that application can use this API when all the event timers have the
 * same timeout expiration tick. This specialized function can provide the
 * additional hint to the adapter implementation and optimize if possible.
 *
 * @param adapter
 *   A pointer to an event timer adapter structure.
 * @param evtims
 *   Points to an array of objects of type *rte_event_timer* structure.
 * @param timeout_ticks
 *   The number of ticks in which the timers should expire.
 * @param nb_evtims
 *   Number of event timers in the supplied array.
 *
 * @return
 *   The number of successfully armed event timers. The return value can be less
 *   than the value of the *nb_evtims* parameter. If the return value is less
 *   than *nb_evtims*, the remaining event timers at the end of *evtims*
 *   are not consumed, and the caller has to take care of them, and rte_errno
 *   is set accordingly. Possible errno values include:
 *   - EINVAL Invalid timer adapter, expiry event queue ID is invalid, or an
 *   expiry event's sched type doesn't match the capabilities of the
 *   destination event queue.
 *   - EAGAIN Specified event timer adapter is not running
 *   - EALREADY A timer was encountered that was already armed
 */
static inline uint16_t
rte_event_timer_arm_tmo_tick_burst(
			const struct rte_event_timer_adapter *adapter,
			struct rte_event_timer **evtims,
			const uint64_t timeout_ticks,
			const uint16_t nb_evtims)
{
#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->arm_tmo_tick_burst, -EINVAL);
#endif
	rte_eventdev_trace_timer_arm_tmo_tick_burst(adapter, timeout_ticks,
		(void **)evtims, nb_evtims);
	return adapter->arm_tmo_tick_burst(adapter, evtims, timeout_ticks,
					   nb_evtims);
}

/**
 * Cancel a burst of event timers from being scheduled to the event device.
 *
 * @param adapter
 *   A pointer to an event timer adapter structure.
 * @param evtims
 *   Points to an array of objects of type *rte_event_timer* structure
 * @param nb_evtims
 *   Number of event timer instances in the supplied array.
 *
 * @return
 *   The number of successfully canceled event timers. The return value can be
 *   less than the value of the *nb_evtims* parameter. If the return value is
 *   less than *nb_evtims*, the remaining event timers at the end of *evtims*
 *   are not consumed, and the caller has to take care of them, and rte_errno
 *   is set accordingly. Possible errno values include:
 *   - EINVAL Invalid timer adapter identifier
 *   - EAGAIN Specified timer adapter is not running
 *   - EALREADY  A timer was encountered that was already canceled
 */
static inline uint16_t
rte_event_timer_cancel_burst(const struct rte_event_timer_adapter *adapter,
			     struct rte_event_timer **evtims,
			     uint16_t nb_evtims)
{
#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
	ADAPTER_VALID_OR_ERR_RET(adapter, -EINVAL);
	FUNC_PTR_OR_ERR_RET(adapter->cancel_burst, -EINVAL);
#endif
	rte_eventdev_trace_timer_cancel_burst(adapter, (void **)evtims,
		nb_evtims);
	return adapter->cancel_burst(adapter, evtims, nb_evtims);
}

#ifdef __cplusplus
}
#endif

#endif /* __RTE_EVENT_TIMER_ADAPTER_H__ */
