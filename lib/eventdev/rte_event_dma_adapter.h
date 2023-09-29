/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#ifndef RTE_EVENT_DMA_ADAPTER
#define RTE_EVENT_DMA_ADAPTER

/**
 * @file rte_event_dma_adapter.h
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * DMA Event Adapter API.
 *
 * Eventdev library provides adapters to bridge between various components for providing new
 * event source. The event DMA adapter is one of those adapters which is intended to bridge
 * between event devices and DMA devices.
 *
 * The DMA adapter adds support to enqueue / dequeue DMA operations to / from event device. The
 * packet flow between DMA device and the event device can be accomplished using both SW and HW
 * based transfer mechanisms. The adapter uses an EAL service core function for SW based packet
 * transfer and uses the eventdev PMD functions to configure HW based packet transfer between the
 * DMA device and the event device.
 *
 * The application can choose to submit a DMA operation directly to an DMA device or send it to the
 * DMA adapter via eventdev based on RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD capability. The
 * first mode is known as the event new (RTE_EVENT_DMA_ADAPTER_OP_NEW) mode and the second as the
 * event forward (RTE_EVENT_DMA_ADAPTER_OP_FORWARD) mode. The choice of mode can be specified while
 * creating the adapter. In the former mode, it is an application responsibility to enable ingress
 * packet ordering. In the latter mode, it is the adapter responsibility to enable the ingress
 * packet ordering.
 *
 *
 * Working model of RTE_EVENT_DMA_ADAPTER_OP_NEW mode:
 *
 *                +--------------+         +--------------+
 *                |              |         |   DMA stage  |
 *                | Application  |---[2]-->| + enqueue to |
 *                |              |         |     dmadev   |
 *                +--------------+         +--------------+
 *                    ^   ^                       |
 *                    |   |                      [3]
 *                   [6] [1]                      |
 *                    |   |                       |
 *                +--------------+                |
 *                |              |                |
 *                | Event device |                |
 *                |              |                |
 *                +--------------+                |
 *                       ^                        |
 *                       |                        |
 *                      [5]                       |
 *                       |                        v
 *                +--------------+         +--------------+
 *                |              |         |              |
 *                |  DMA adapter |<--[4]---|    dmadev    |
 *                |              |         |              |
 *                +--------------+         +--------------+
 *
 *
 *         [1] Application dequeues events from the previous stage.
 *         [2] Application prepares the DMA operations.
 *         [3] DMA operations are submitted to dmadev by application.
 *         [4] DMA adapter dequeues DMA completions from dmadev.
 *         [5] DMA adapter enqueues events to the eventdev.
 *         [6] Application dequeues from eventdev for further processing.
 *
 * In the RTE_EVENT_DMA_ADAPTER_OP_NEW mode, application submits DMA operations directly to DMA
 * device. The DMA adapter then dequeues DMA completions from DMA device and enqueue events to the
 * event device. This mode does not ensure ingress ordering, if the application directly enqueues
 * to dmadev without going through DMA / atomic stage i.e. removing item [1] and [2].
 *
 * Events dequeued from the adapter will be treated as new events. In this mode, application needs
 * to specify event information (response information) which is needed to enqueue an event after the
 * DMA operation is completed.
 *
 *
 * Working model of RTE_EVENT_DMA_ADAPTER_OP_FORWARD mode:
 *
 *                +--------------+         +--------------+
 *        --[1]-->|              |---[2]-->|  Application |
 *                | Event device |         |      in      |
 *        <--[8]--|              |<--[3]---| Ordered stage|
 *                +--------------+         +--------------+
 *                    ^      |
 *                    |     [4]
 *                   [7]     |
 *                    |      v
 *               +----------------+       +--------------+
 *               |                |--[5]->|              |
 *               |   DMA adapter  |       |     dmadev   |
 *               |                |<-[6]--|              |
 *               +----------------+       +--------------+
 *
 *
 *         [1] Events from the previous stage.
 *         [2] Application in ordered stage dequeues events from eventdev.
 *         [3] Application enqueues DMA operations as events to eventdev.
 *         [4] DMA adapter dequeues event from eventdev.
 *         [5] DMA adapter submits DMA operations to dmadev (Atomic stage).
 *         [6] DMA adapter dequeues DMA completions from dmadev
 *         [7] DMA adapter enqueues events to the eventdev
 *         [8] Events to the next stage
 *
 * In the event forward (RTE_EVENT_DMA_ADAPTER_OP_FORWARD) mode, if the HW supports the capability
 * RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD, application can directly submit the DMA
 * operations to the dmadev. If not, application retrieves the event port of the DMA adapter
 * through the API, rte_event_DMA_adapter_event_port_get(). Then, links its event queue to this
 * port and starts enqueuing DMA operations as events to the eventdev. The adapter then dequeues
 * the events and submits the DMA operations to the dmadev. After the DMA completions, the adapter
 * enqueues events to the event device.
 *
 * Application can use this mode, when ingress packet ordering is needed. Events dequeued from the
 * adapter will be treated as forwarded events. In this mode, the application needs to specify the
 * dmadev ID and queue pair ID (request information) needed to enqueue an DMA operation in addition
 * to the event information (response information) needed to enqueue an event after the DMA
 * operation has completed.
 *
 * The event DMA adapter provides common APIs to configure the packet flow from the DMA device to
 * event devices for both SW and HW based transfers. The DMA event adapter's functions are:
 *
 *  - rte_event_dma_adapter_create_ext()
 *  - rte_event_dma_adapter_create()
 *  - rte_event_dma_adapter_free()
 *  - rte_event_dma_adapter_vchan_add()
 *  - rte_event_dma_adapter_vchan_del()
 *  - rte_event_dma_adapter_start()
 *  - rte_event_dma_adapter_stop()
 *  - rte_event_dma_adapter_stats_get()
 *  - rte_event_dma_adapter_stats_reset()
 *
 * The application creates an instance using rte_event_dma_adapter_create() or
 * rte_event_dma_adapter_create_ext().
 *
 * dmadev queue pair addition / deletion is done using the rte_event_dma_adapter_vchan_add() /
 * rte_event_dma_adapter_vchan_del() APIs. If HW supports the capability
 * RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND, event information must be passed to the
 * add API.
 *
 */

#include <stdint.h>

#include <rte_common.h>
#include <rte_dmadev_pmd.h>
#include <rte_eventdev.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A structure used to hold event based DMA operation entry. All the information
 * required for a DMA transfer shall be populated in "struct rte_event_dma_adapter_op"
 * instance.
 */
struct rte_event_dma_adapter_op {
	struct rte_dma_sge *src_seg;
	/**< Source segments. */
	struct rte_dma_sge *dst_seg;
	/**< Destination segments. */
	uint16_t nb_src;
	/**< Number of source segments. */
	uint16_t nb_dst;
	/**< Number of destination segments. */
	uint64_t flags;
	/**< Flags related to the operation.
	 * @see RTE_DMA_OP_FLAG_*
	 */
	int16_t dma_dev_id;
	/**< DMA device ID to be used */
	uint16_t vchan;
	/**< DMA vchan ID to be used */
	struct rte_mempool *op_mp;
	/**< Mempool from which op is allocated. */
};

/**
 *  DMA event adapter mode
 */
enum rte_event_dma_adapter_mode {
	RTE_EVENT_DMA_ADAPTER_OP_NEW,
	/**< Start the DMA adapter in event new mode.
	 * @see RTE_EVENT_OP_NEW.
	 *
	 * Application submits DMA operations to the dmadev. Adapter only dequeues the DMA
	 * completions from dmadev and enqueue events to the eventdev.
	 */

	RTE_EVENT_DMA_ADAPTER_OP_FORWARD,
	/**< Start the DMA adapter in event forward mode.
	 * @see RTE_EVENT_OP_FORWARD.
	 *
	 * Application submits DMA requests as events to the DMA adapter or DMA device based on
	 * RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD capability. DMA completions are enqueued
	 * back to the eventdev by DMA adapter.
	 */
};

/**
 * Adapter configuration structure that the adapter configuration callback function is expected to
 * fill out.
 *
 * @see rte_event_dma_adapter_conf_cb
 */
struct rte_event_dma_adapter_conf {
	uint8_t event_port_id;
	/** < Event port identifier, the adapter enqueues events to this port and dequeues DMA
	 * request events in RTE_EVENT_DMA_ADAPTER_OP_FORWARD mode.
	 */

	uint32_t max_nb;
	/**< The adapter can return early if it has processed at least max_nb DMA ops. This isn't
	 * treated as a requirement; batching may cause the adapter to process more than max_nb DMA
	 * ops.
	 */
};

/**
 * Adapter runtime configuration parameters
 */
struct rte_event_dma_adapter_runtime_params {
	uint32_t max_nb;
	/**< The adapter can return early if it has processed at least max_nb DMA ops. This isn't
	 * treated as a requirement; batching may cause the adapter to process more than max_nb DMA
	 * ops.
	 *
	 * Callback function passed to rte_event_dma_adapter_create_ext() configures the adapter
	 * with default value of max_nb.
	 * rte_event_dma_adapter_runtime_params_set() allows to re-configure max_nb during runtime
	 * (after adding at least one queue pair)
	 *
	 * This is valid for the devices without RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD or
	 * RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_NEW capability.
	 */

	uint32_t rsvd[15];
	/**< Reserved fields for future expansion */
};

/**
 * Function type used for adapter configuration callback. The callback is used to fill in members of
 * the struct rte_event_dma_adapter_conf, this callback is invoked when creating a SW service for
 * packet transfer from dmadev vchan to the event device. The SW service is created within the
 * function, rte_event_dma_adapter_vchan_add(), if SW based packet transfers from dmadev vchan
 * to the event device are required.
 *
 * @param id
 *     Adapter identifier.
 * @param evdev_id
 *     Event device identifier.
 * @param conf
 *     Structure that needs to be populated by this callback.
 * @param arg
 *     Argument to the callback. This is the same as the conf_arg passed to the
 * rte_event_dma_adapter_create_ext().
 */
typedef int (*rte_event_dma_adapter_conf_cb)(uint8_t id, uint8_t evdev_id,
					     struct rte_event_dma_adapter_conf *conf, void *arg);

/**
 * A structure used to retrieve statistics for an event DMA adapter instance.
 */
struct rte_event_dma_adapter_stats {
	uint64_t event_poll_count;
	/**< Event port poll count */

	uint64_t event_deq_count;
	/**< Event dequeue count */

	uint64_t dma_enq_count;
	/**< dmadev enqueue count */

	uint64_t dma_enq_fail_count;
	/**< dmadev enqueue failed count */

	uint64_t dma_deq_count;
	/**< dmadev dequeue count */

	uint64_t event_enq_count;
	/**< Event enqueue count */

	uint64_t event_enq_retry_count;
	/**< Event enqueue retry count */

	uint64_t event_enq_fail_count;
	/**< Event enqueue fail count */
};

/**
 * Create a new event DMA adapter with the specified identifier.
 *
 * @param id
 *     Adapter identifier.
 * @param evdev_id
 *     Event device identifier.
 * @param conf_cb
 *     Callback function that fills in members of a struct rte_event_dma_adapter_conf struct passed
 * into it.
 * @param mode
 *     Flag to indicate the mode of the adapter.
 *     @see rte_event_dma_adapter_mode
 * @param conf_arg
 *     Argument that is passed to the conf_cb function.
 *
 * @return
 *     - 0: Success
 *     - <0: Error code on failure
 */
__rte_experimental
int rte_event_dma_adapter_create_ext(uint8_t id, uint8_t evdev_id,
				     rte_event_dma_adapter_conf_cb conf_cb,
				     enum rte_event_dma_adapter_mode mode, void *conf_arg);

/**
 * Create a new event DMA adapter with the specified identifier. This function uses an internal
 * configuration function that creates an event port. This default function reconfigures the event
 * device with an additional event port and set up the event port using the port_config parameter
 * passed into this function. In case the application needs more control in configuration of the
 * service, it should use the rte_event_dma_adapter_create_ext() version.
 *
 * @param id
 *     Adapter identifier.
 * @param evdev_id
 *     Event device identifier.
 * @param port_config
 *     Argument of type *rte_event_port_conf* that is passed to the conf_cb function.
 * @param mode
 *     Flag to indicate the mode of the adapter.
 *     @see rte_event_dma_adapter_mode
 *
 * @return
 *     - 0: Success
 *     - <0: Error code on failure
 */
__rte_experimental
int rte_event_dma_adapter_create(uint8_t id, uint8_t evdev_id,
				 struct rte_event_port_conf *port_config,
				 enum rte_event_dma_adapter_mode mode);

/**
 * Free an event DMA adapter
 *
 * @param id
 *     Adapter identifier.
 * @return
 *     - 0: Success
 *     - <0: Error code on failure, If the adapter still has queue pairs added to it, the function
 * returns -EBUSY.
 */
__rte_experimental
int rte_event_dma_adapter_free(uint8_t id);

/**
 * Retrieve the event port of an adapter.
 *
 * @param id
 *     Adapter identifier.
 *
 * @param [out] event_port_id
 *     Application links its event queue to this adapter port which is used in
 * RTE_EVENT_DMA_ADAPTER_OP_FORWARD mode.
 *
 * @return
 *     - 0: Success
 *     - <0: Error code on failure.
 */
__rte_experimental
int rte_event_dma_adapter_event_port_get(uint8_t id, uint8_t *event_port_id);

/**
 * Add a vchan to an event DMA adapter.
 *
 * @param id
 *     Adapter identifier.
 * @param dmadev_id
 *     dmadev identifier.
 * @param vchan
 *     DMA device vchan identifier. If vchan is set -1, adapter adds all the
 * preconfigured vchan to the instance.
 * @param event
 *     If HW supports dmadev vchan to event queue binding, application is expected to fill in
 * event information, else it will be NULL.
 *     @see RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND
 *
 * @return
 *     - 0: Success, vchan added correctly.
 *     - <0: Error code on failure.
 */
__rte_experimental
int rte_event_dma_adapter_vchan_add(uint8_t id, int16_t dmadev_id, uint16_t vchan,
				    const struct rte_event *event);

/**
 * Delete a vchan from an event DMA adapter.
 *
 * @param id
 *     Adapter identifier.
 * @param dmadev_id
 *     DMA device identifier.
 * @param vchan
 *     DMA device vchan identifier.
 *
 * @return
 *     - 0: Success, vchan deleted successfully.
 *     - <0: Error code on failure.
 */
__rte_experimental
int rte_event_dma_adapter_vchan_del(uint8_t id, int16_t dmadev_id, uint16_t vchan);

/**
 * Retrieve the service ID of an adapter. If the adapter doesn't use a rte_service function, this
 * function returns -ESRCH.
 *
 * @param id
 *     Adapter identifier.
 * @param [out] service_id
 *     A pointer to a uint32_t, to be filled in with the service id.
 *
 * @return
 *     - 0: Success
 *     - <0: Error code on failure, if the adapter doesn't use a rte_service function, this function
 * returns -ESRCH.
 */
__rte_experimental
int rte_event_dma_adapter_service_id_get(uint8_t id, uint32_t *service_id);

/**
 * Start event DMA adapter
 *
 * @param id
 *     Adapter identifier.
 *
 * @return
 *     - 0: Success, adapter started successfully.
 *     - <0: Error code on failure.
 *
 * @note The eventdev and dmadev to which the event_dma_adapter is connected should be started
 * before calling rte_event_dma_adapter_start().
 */
__rte_experimental
int rte_event_dma_adapter_start(uint8_t id);

/**
 * Stop event DMA adapter
 *
 * @param id
 *  Adapter identifier.
 *
 * @return
 *  - 0: Success, adapter stopped successfully.
 *  - <0: Error code on failure.
 */
__rte_experimental
int rte_event_dma_adapter_stop(uint8_t id);

/**
 * Initialize the adapter runtime configuration parameters
 *
 * @param params
 *  A pointer to structure of type struct rte_event_dma_adapter_runtime_params
 *
 * @return
 *  -  0: Success
 *  - <0: Error code on failure
 */
__rte_experimental
int rte_event_dma_adapter_runtime_params_init(struct rte_event_dma_adapter_runtime_params *params);

/**
 * Set the adapter runtime configuration parameters
 *
 * @param id
 *  Adapter identifier
 *
 * @param params
 *  A pointer to structure of type struct rte_event_dma_adapter_runtime_params with configuration
 * parameter values. The reserved fields of this structure must be initialized to zero and the valid
 * fields need to be set appropriately. This struct can be initialized using
 * rte_event_dma_adapter_runtime_params_init() API to default values or application may reset this
 * struct and update required fields.
 *
 * @return
 *  -  0: Success
 *  - <0: Error code on failure
 */
__rte_experimental
int rte_event_dma_adapter_runtime_params_set(uint8_t id,
					     struct rte_event_dma_adapter_runtime_params *params);

/**
 * Get the adapter runtime configuration parameters
 *
 * @param id
 *  Adapter identifier
 *
 * @param[out] params
 *  A pointer to structure of type struct rte_event_dma_adapter_runtime_params containing valid
 * adapter parameters when return value is 0.
 *
 * @return
 *  -  0: Success
 *  - <0: Error code on failure
 */
__rte_experimental
int rte_event_dma_adapter_runtime_params_get(uint8_t id,
					     struct rte_event_dma_adapter_runtime_params *params);

/**
 * Retrieve statistics for an adapter
 *
 * @param id
 *     Adapter identifier.
 * @param [out] stats
 *     A pointer to structure used to retrieve statistics for an adapter.
 *
 * @return
 *     - 0: Success, retrieved successfully.
 *     - <0: Error code on failure.
 */
__rte_experimental
int rte_event_dma_adapter_stats_get(uint8_t id, struct rte_event_dma_adapter_stats *stats);

/**
 * Reset statistics for an adapter.
 *
 * @param id
 *     Adapter identifier.
 *
 * @return
 *     - 0: Success, statistics reset successfully.
 *     - <0: Error code on failure.
 */
__rte_experimental
int rte_event_dma_adapter_stats_reset(uint8_t id);

/**
 * Enqueue a burst of DMA operations as event objects supplied in *rte_event* structure on an event
 * DMA adapter designated by its event *evdev_id* through the event port specified by *port_id*.
 * This function is supported if the eventdev PMD has the
 * #RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_OP_FWD capability flag set.
 *
 * The *nb_events* parameter is the number of event objects to enqueue that are supplied in the
 * *ev* array of *rte_event* structure.
 *
 * The rte_event_dma_adapter_enqueue() function returns the number of event objects it actually
 * enqueued. A return value equal to *nb_events* means that all event objects have been enqueued.
 *
 * @param evdev_id
 *     The identifier of the device.
 * @param port_id
 *     The identifier of the event port.
 * @param ev
 *     Points to an array of *nb_events* objects of type *rte_event* structure which contain the
 * event object enqueue operations to be processed.
 * @param nb_events
 *     The number of event objects to enqueue, typically number of
 * rte_event_port_attr_get(...RTE_EVENT_PORT_ATTR_ENQ_DEPTH...) available for this port.
 *
 * @return
 *     The number of event objects actually enqueued on the event device. The return value can be
 * less than the value of the *nb_events* parameter when the event devices queue is full or if
 * invalid parameters are specified in a *rte_event*. If the return value is less than *nb_events*,
 * the remaining events at the end of ev[] are not consumed and the caller has to take care of them,
 * and rte_errno is set accordingly. Possible errno values include:
 *     - EINVAL: The port ID is invalid, device ID is invalid, an event's queue ID is invalid, or an
 * event's sched type doesn't match the capabilities of the destination queue.
 *     - ENOSPC: The event port was backpressured and unable to enqueue one or more events. This
 * error code is only applicable to closed systems.
 */
__rte_experimental
uint16_t rte_event_dma_adapter_enqueue(uint8_t evdev_id, uint8_t port_id, struct rte_event ev[],
				       uint16_t nb_events);

#ifdef __cplusplus
}
#endif

#endif /* RTE_EVENT_DMA_ADAPTER */
