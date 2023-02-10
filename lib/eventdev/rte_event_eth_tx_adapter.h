/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 */

#ifndef _RTE_EVENT_ETH_TX_ADAPTER_
#define _RTE_EVENT_ETH_TX_ADAPTER_

/**
 * @file
 *
 * RTE Event Ethernet Tx Adapter
 *
 * The event ethernet Tx adapter provides configuration and data path APIs
 * for the ethernet transmit stage of an event driven packet processing
 * application. These APIs abstract the implementation of the transmit stage
 * and allow the application to use eventdev PMD support or a common
 * implementation.
 *
 * In the common implementation, the application enqueues mbufs to the adapter
 * which runs as a rte_service function. The service function dequeues events
 * from its event port and transmits the mbufs referenced by these events.
 *
 * The ethernet Tx event adapter APIs are:
 *
 *  - rte_event_eth_tx_adapter_create()
 *  - rte_event_eth_tx_adapter_create_ext()
 *  - rte_event_eth_tx_adapter_free()
 *  - rte_event_eth_tx_adapter_start()
 *  - rte_event_eth_tx_adapter_stop()
 *  - rte_event_eth_tx_adapter_queue_add()
 *  - rte_event_eth_tx_adapter_queue_del()
 *  - rte_event_eth_tx_adapter_stats_get()
 *  - rte_event_eth_tx_adapter_stats_reset()
 *  - rte_event_eth_tx_adapter_enqueue()
 *  - rte_event_eth_tx_adapter_event_port_get()
 *  - rte_event_eth_tx_adapter_service_id_get()
 *  - rte_event_eth_tx_adapter_instance_get()
 *  - rte_event_eth_tx_adapter_queue_start()
 *  - rte_event_eth_tx_adapter_queue_stop()
 *  - rte_event_eth_tx_adapter_runtime_params_get()
 *  - rte_event_eth_tx_adapter_runtime_params_init()
 *  - rte_event_eth_tx_adapter_runtime_params_set()
 *
 * The application creates the adapter using
 * rte_event_eth_tx_adapter_create() or rte_event_eth_tx_adapter_create_ext().
 *
 * The adapter will use the common implementation when the eventdev PMD
 * does not have the #RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT capability.
 * The common implementation uses an event port that is created using the port
 * configuration parameter passed to rte_event_eth_tx_adapter_create(). The
 * application can get the port identifier using
 * rte_event_eth_tx_adapter_event_port_get() and must link an event queue to
 * this port.
 *
 * If the eventdev PMD has the #RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT
 * flags set, Tx adapter events should be enqueued using the
 * rte_event_eth_tx_adapter_enqueue() function, else the application should
 * use rte_event_enqueue_burst().
 *
 * Transmit queues can be added and deleted from the adapter using
 * rte_event_eth_tx_adapter_queue_add()/del() APIs respectively.
 *
 * The application can start and stop the adapter using the
 * rte_event_eth_tx_adapter_start/stop() calls.
 *
 * The common adapter implementation uses an EAL service function as described
 * before and its execution is controlled using the rte_service APIs. The
 * rte_event_eth_tx_adapter_service_id_get()
 * function can be used to retrieve the adapter's service function ID.
 *
 * The ethernet port and transmit queue index to transmit the mbuf on are
 * specified using the mbuf port struct rte_mbuf::hash::txadapter:txq.
 * The application should use the rte_event_eth_tx_adapter_txq_set()
 * and rte_event_eth_tx_adapter_txq_get() functions to access the transmit
 * queue index, using these macros will help with minimizing application
 * impact due to a change in how the transmit queue index is specified.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_compat.h>
#include <rte_mbuf.h>

#include "rte_eventdev.h"

/**
 * Adapter configuration structure
 *
 * @see rte_event_eth_tx_adapter_create_ext
 * @see rte_event_eth_tx_adapter_conf_cb
 */
struct rte_event_eth_tx_adapter_conf {
	uint8_t event_port_id;
	/**< Event port identifier, the adapter service function dequeues mbuf
	 * events from this port.
	 * @see RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT
	 */
	uint32_t max_nb_tx;
	/**< The adapter can return early if it has processed at least
	 * max_nb_tx mbufs. This isn't treated as a requirement; batching may
	 * cause the adapter to process more than max_nb_tx mbufs.
	 */
};

/**
 * Adapter runtime configuration parameters
 */
struct rte_event_eth_tx_adapter_runtime_params {
	uint32_t max_nb_tx;
	/**< The adapter can return early if it has processed at least
	 * max_nb_tx mbufs. This isn't treated as a requirement; batching may
	 * cause the adapter to process more than max_nb_tx mbufs.
	 *
	 * rte_event_eth_tx_adapter_create() configures the
	 * adapter with default value of max_nb_tx.
	 * rte_event_eth_tx_adapter_create_ext() configures the adapter with
	 * user provided value of max_nb_tx through
	 * rte_event_eth_tx_adapter_conf::max_nb_tx parameter.
	 * rte_event_eth_tx_adapter_runtime_params_set() allows to re-configure
	 * max_nb_tx during runtime (after adding at least one queue)
	 *
	 * This is valid for the devices without
	 * RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT capability.
	 */
	uint16_t flush_threshold;
	/**< the number of service function iteration count to
	 * flush buffered packets.
	 *
	 * This is valid for the devices without
	 * RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT capability.
	 */
	uint16_t rsvd[29];
	/**< Reserved fields for future expansion */
};

/**
 * Function type used for adapter configuration callback. The callback is
 * used to fill in members of the struct rte_event_eth_tx_adapter_conf, this
 * callback is invoked when creating a RTE service function based
 * adapter implementation.
 *
 * @param id
 *  Adapter identifier.
 * @param dev_id
 *  Event device identifier.
 * @param [out] conf
 *  Structure that needs to be populated by this callback.
 * @param arg
 *  Argument to the callback. This is the same as the conf_arg passed to the
 *  rte_event_eth_tx_adapter_create_ext().
 *
 * @return
 *   - 0: Success
 *   - <0: Error code on failure
 */
typedef int (*rte_event_eth_tx_adapter_conf_cb) (uint8_t id, uint8_t dev_id,
				struct rte_event_eth_tx_adapter_conf *conf,
				void *arg);

/**
 * A structure used to retrieve statistics for an ethernet Tx adapter instance.
 */
struct rte_event_eth_tx_adapter_stats {
	uint64_t tx_retry;
	/**< Number of transmit retries */
	uint64_t tx_packets;
	/**< Number of packets transmitted */
	uint64_t tx_dropped;
	/**< Number of packets dropped */
};

/**
 * Create a new ethernet Tx adapter with the specified identifier.
 *
 * When this API is used for creating adapter instance,
 * ``rte_event_dev_config::nb_event_ports`` is automatically incremented,
 * and event device is reconfigured with additional event port during service
 * initialization. This event device reconfigure logic also increments the
 * ``rte_event_dev_config::nb_single_link_event_port_queues``
 * parameter if the adapter event port config is of type
 * ``RTE_EVENT_PORT_CFG_SINGLE_LINK``.
 *
 * Application no longer needs to account for the
 * ``rte_event_dev_config::nb_event_ports`` and
 * ``rte_event_dev_config::nb_single_link_event_port_queues``
 * parameters required for eth Tx adapter in event device configure when
 * the adapter is created with this API.
 *
 * @param id
 *  The identifier of the ethernet Tx adapter.
 * @param dev_id
 *  The event device identifier.
 * @param port_config
 *  Event port configuration, the adapter uses this configuration to
 *  create an event port if needed.
 * @return
 *   - 0: Success
 *   - <0: Error code on failure
 */
int
rte_event_eth_tx_adapter_create(uint8_t id, uint8_t dev_id,
				struct rte_event_port_conf *port_config);

/**
 * Create a new ethernet Tx adapter with the specified identifier.
 *
 * @param id
 *  The identifier of the ethernet Tx adapter.
 * @param dev_id
 *  The event device identifier.
 * @param conf_cb
 *  Callback function that initializes members of the
 *  struct rte_event_eth_tx_adapter_conf struct passed into
 *  it.
 * @param conf_arg
 *  Argument that is passed to the conf_cb function.
 * @return
 *   - 0: Success
 *   - <0: Error code on failure
 */
int
rte_event_eth_tx_adapter_create_ext(uint8_t id, uint8_t dev_id,
				rte_event_eth_tx_adapter_conf_cb conf_cb,
				void *conf_arg);

/**
 * Free an ethernet Tx adapter
 *
 * @param id
 *  Adapter identifier.
 * @return
 *   - 0: Success
 *   - <0: Error code on failure, If the adapter still has Tx queues
 *      added to it, the function returns -EBUSY.
 */
int
rte_event_eth_tx_adapter_free(uint8_t id);

/**
 * Start ethernet Tx adapter
 *
 * @param id
 *  Adapter identifier.
 * @return
 *  - 0: Success, Adapter started correctly.
 *  - <0: Error code on failure.
 */
int
rte_event_eth_tx_adapter_start(uint8_t id);

/**
 * Stop ethernet Tx adapter
 *
 * @param id
 *  Adapter identifier.
 * @return
 *  - 0: Success.
 *  - <0: Error code on failure.
 */
int
rte_event_eth_tx_adapter_stop(uint8_t id);

/**
 * Add a Tx queue to the adapter.
 * A queue value of -1 is used to indicate all
 * queues within the device.
 *
 * @param id
 *  Adapter identifier.
 * @param eth_dev_id
 *  Ethernet Port Identifier.
 * @param queue
 *  Tx queue index.
 * @return
 *  - 0: Success, Queues added successfully.
 *  - <0: Error code on failure.
 */
int
rte_event_eth_tx_adapter_queue_add(uint8_t id,
				uint16_t eth_dev_id,
				int32_t queue);

/**
 * Delete a Tx queue from the adapter.
 * A queue value of -1 is used to indicate all
 * queues within the device, that have been added to this
 * adapter.
 *
 * @param id
 *  Adapter identifier.
 * @param eth_dev_id
 *  Ethernet Port Identifier.
 * @param queue
 *  Tx queue index.
 * @return
 *  - 0: Success, Queues deleted successfully.
 *  - <0: Error code on failure.
 */
int
rte_event_eth_tx_adapter_queue_del(uint8_t id,
				uint16_t eth_dev_id,
				int32_t queue);

/**
 * Set Tx queue in the mbuf. This queue is used by the adapter
 * to transmit the mbuf.
 *
 * @param pkt
 *  Pointer to the mbuf.
 * @param queue
 *  Tx queue index.
 */
static __rte_always_inline void
rte_event_eth_tx_adapter_txq_set(struct rte_mbuf *pkt, uint16_t queue)
{
	pkt->hash.txadapter.txq = queue;
}

/**
 * Retrieve Tx queue from the mbuf.
 *
 * @param pkt
 *  Pointer to the mbuf.
 * @return
 *  Tx queue identifier.
 *
 * @see rte_event_eth_tx_adapter_txq_set()
 */
static __rte_always_inline uint16_t
rte_event_eth_tx_adapter_txq_get(struct rte_mbuf *pkt)
{
	return pkt->hash.txadapter.txq;
}

/**
 * Retrieve the adapter event port. The adapter creates an event port if
 * the #RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT is not set in the
 * ethernet Tx capabilities of the event device.
 *
 * @param id
 *  Adapter Identifier.
 * @param[out] event_port_id
 *  Event port pointer.
 * @return
 *   - 0: Success.
 *   - <0: Error code on failure.
 */
int
rte_event_eth_tx_adapter_event_port_get(uint8_t id, uint8_t *event_port_id);

#define RTE_EVENT_ETH_TX_ADAPTER_ENQUEUE_SAME_DEST	0x1
/**< This flag is used when all the packets enqueued in the tx adapter are
 * destined for the same Ethernet port & Tx queue.
 */

/**
 * Enqueue a burst of events objects or an event object supplied in *rte_event*
 * structure on an  event device designated by its *dev_id* through the event
 * port specified by *port_id*. This function is supported if the eventdev PMD
 * has the #RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT capability flag set.
 *
 * The *nb_events* parameter is the number of event objects to enqueue which are
 * supplied in the *ev* array of *rte_event* structure.
 *
 * The rte_event_eth_tx_adapter_enqueue() function returns the number of
 * events objects it actually enqueued. A return value equal to *nb_events*
 * means that all event objects have been enqueued.
 *
 * @param dev_id
 *  The identifier of the device.
 * @param port_id
 *  The identifier of the event port.
 * @param ev
 *  Points to an array of *nb_events* objects of type *rte_event* structure
 *  which contain the event object enqueue operations to be processed.
 * @param nb_events
 *  The number of event objects to enqueue, typically number of
 *  rte_event_port_attr_get(...RTE_EVENT_PORT_ATTR_ENQ_DEPTH...)
 *  available for this port.
 * @param flags
 *  RTE_EVENT_ETH_TX_ADAPTER_ENQUEUE_ flags.
 *  #RTE_EVENT_ETH_TX_ADAPTER_ENQUEUE_SAME_DEST signifies that all the packets
 *  which are enqueued are destined for the same Ethernet port & Tx queue.
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
 */
static inline uint16_t
rte_event_eth_tx_adapter_enqueue(uint8_t dev_id,
				uint8_t port_id,
				struct rte_event ev[],
				uint16_t nb_events,
				const uint8_t flags)
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
	rte_eventdev_trace_eth_tx_adapter_enqueue(dev_id, port_id, ev,
		nb_events, flags);
	if (flags)
		return fp_ops->txa_enqueue_same_dest(port, ev, nb_events);
	else
		return fp_ops->txa_enqueue(port, ev, nb_events);
}

/**
 * Retrieve statistics for an adapter
 *
 * @param id
 *  Adapter identifier.
 * @param [out] stats
 *  A pointer to structure used to retrieve statistics for an adapter.
 * @return
 *  - 0: Success, statistics retrieved successfully.
 *  - <0: Error code on failure.
 */
int
rte_event_eth_tx_adapter_stats_get(uint8_t id,
				struct rte_event_eth_tx_adapter_stats *stats);

/**
 * Reset statistics for an adapter.
 *
 * @param id
 *  Adapter identifier.
 * @return
 *  - 0: Success, statistics reset successfully.
 *  - <0: Error code on failure.
 */
int
rte_event_eth_tx_adapter_stats_reset(uint8_t id);

/**
 * Retrieve the service ID of an adapter. If the adapter doesn't use
 * a rte_service function, this function returns -ESRCH.
 *
 * @param id
 *  Adapter identifier.
 * @param [out] service_id
 *  A pointer to a uint32_t, to be filled in with the service id.
 * @return
 *  - 0: Success
 *  - <0: Error code on failure, if the adapter doesn't use a rte_service
 * function, this function returns -ESRCH.
 */
int
rte_event_eth_tx_adapter_service_id_get(uint8_t id, uint32_t *service_id);

/**
 * Get TX adapter instance id for TX queue
 *
 * @param eth_dev_id
 *  Port identifier of Ethernet device
 *
 * @param tx_queue_id
 *  Etherdev device TX queue index
 *
 * @param[out] txa_inst_id
 *  Pointer to TX adapter instance identifier
 *  Contains valid Tx adapter instance id when return value is 0
 *
 * @return
 *  -  0: Success
 *  - <0: Error code on failure
 */
__rte_experimental
int
rte_event_eth_tx_adapter_instance_get(uint16_t eth_dev_id,
				      uint16_t tx_queue_id,
				      uint8_t *txa_inst_id);
/**
 * Enables the adapter to start enqueueing of packets to the
 * Tx queue.
 *
 * This function is provided so that the application can
 * resume enqueueing packets that reference packets for
 * <eth_dev_id, tx_queue_id> after calling
 * rte_event_eth_tx_adapter_queue_stop().
 * @see rte_event_eth_tx_adapter_queue_stop
 *
 * Use case:
 * --------
 * The queue start/stop APIs help avoid some unexpected behavior with
 * application stopping ethdev Tx queues and adapter being unaware of it.
 * With these APIs, the application can call stop API to notify adapter
 * that corresponding ethdev Tx queue is stopped and any in-flight
 * packets are freed by adapter dataplane code. Adapter queue stop API
 * is called before stopping the ethdev Tx queue. When ethdev Tx queue
 * is enabled, application can notify adapter to resume processing of
 * the packets for that queue by calling the start API. The ethdev Tx
 * queue is started before calling adapter start API.
 *
 * @param eth_dev_id
 *  Port identifier of Ethernet device.
 * @param tx_queue_id
 *  Ethernet device transmit queue index.
 * @return
 *   - 0: Success
 *   - <0: Error code on failure
 */
__rte_experimental
int
rte_event_eth_tx_adapter_queue_start(uint16_t eth_dev_id, uint16_t tx_queue_id);

/**
 * Stops the adapter runtime function from enqueueing any packets
 * to the associated Tx queue. This API also frees any packets that may
 * have been buffered for this queue. All inflight packets destined to the
 * queue are freed by the adapter runtime until the queue is started again.
 * @see rte_event_eth_tx_adapter_queue_start
 *
 * @param eth_dev_id
 *  Port identifier of Ethernet device.
 * @param tx_queue_id
 *  Ethernet device transmit queue index.
 * @return
 *   - 0: Success
 *   - <0: Error code on failure
 */
__rte_experimental
int
rte_event_eth_tx_adapter_queue_stop(uint16_t eth_dev_id, uint16_t tx_queue_id);

/**
 * Initialize the adapter runtime configuration parameters with default values
 *
 * @param txa_params
 *  A pointer to structure of type struct rte_event_eth_tx_adapter_runtime_params
 *
 * @return
 *  -  0: Success
 *  - <0: Error code on failure
 */
__rte_experimental
int
rte_event_eth_tx_adapter_runtime_params_init(
		struct rte_event_eth_tx_adapter_runtime_params *txa_params);

/**
 * Set the runtime configuration parameters for adapter.
 *
 * @param id
 *  Adapter identifier
 * @param params
 *  A pointer to structure of type struct rte_event_eth_tx_adapter_runtime_params
 *  with configuration parameter values. The reserved fields of this structure
 *  must be initialized to zero and the valid fields need to be set appropriately.
 *  This structure can be initialized using
 *  rte_event_eth_tx_adapter_runtime_params_init() API to default values or
 *  application may reset this structure and update required fields.
 *
 * @return
 * -  0: Success
 * - <0: Error code on failure
 */
__rte_experimental
int
rte_event_eth_tx_adapter_runtime_params_set(uint8_t id,
		struct rte_event_eth_tx_adapter_runtime_params *params);

/**
 * Get the runtime configuration parameters of adapter.
 *
 * @param id
 *  Adapter identifier
 * @param[out] params
 *  A pointer to structure of type struct rte_event_eth_tx_adapter_runtime_params
 *  containing valid Tx adapter parameters when return value is 0.
 *
 * @return
 * -  0: Success
 * - <0: Error code on failure
 */
__rte_experimental
int
rte_event_eth_tx_adapter_runtime_params_get(uint8_t id,
		struct rte_event_eth_tx_adapter_runtime_params *params);

#ifdef __cplusplus
}
#endif
#endif	/* _RTE_EVENT_ETH_TX_ADAPTER_ */
