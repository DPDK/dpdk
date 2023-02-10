/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation.
 * All rights reserved.
 */

#ifndef _RTE_EVENT_CRYPTO_ADAPTER_
#define _RTE_EVENT_CRYPTO_ADAPTER_

/**
 * @file
 *
 * RTE Event crypto adapter
 *
 * Eventdev library provides couple of adapters to bridge between various
 * components for providing new event source. The event crypto adapter is
 * one of those adapters which is intended to bridge between event devices
 * and crypto devices.
 *
 * The crypto adapter adds support to enqueue/dequeue crypto operations to/
 * from event device. The packet flow between crypto device and the event
 * device can be accomplished using both SW and HW based transfer mechanisms.
 * The adapter uses an EAL service core function for SW based packet transfer
 * and uses the eventdev PMD functions to configure HW based packet transfer
 * between the crypto device and the event device.
 *
 * The application can choose to submit a crypto operation directly to
 * crypto device or send it to the crypto adapter via eventdev based on
 * RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD capability.
 * The first mode is known as the event new(RTE_EVENT_CRYPTO_ADAPTER_OP_NEW)
 * mode and the second as the event forward(RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD)
 * mode. The choice of mode can be specified while creating the adapter.
 * In the former mode, it is an application responsibility to enable ingress
 * packet ordering. In the latter mode, it is the adapter responsibility to
 * enable the ingress packet ordering.
 *
 *
 * Working model of RTE_EVENT_CRYPTO_ADAPTER_OP_NEW mode:
 *
 *                +--------------+         +--------------+
 *                |              |         | Crypto stage |
 *                | Application  |---[2]-->| + enqueue to |
 *                |              |         |   cryptodev  |
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
 *                |Crypto adapter|<--[4]---|  Cryptodev   |
 *                |              |         |              |
 *                +--------------+         +--------------+
 *
 *
 *         [1] Application dequeues events from the previous stage.
 *         [2] Application prepares the crypto operations.
 *         [3] Crypto operations are submitted to cryptodev by application.
 *         [4] Crypto adapter dequeues crypto completions from cryptodev.
 *         [5] Crypto adapter enqueues events to the eventdev.
 *         [6] Application dequeues from eventdev and prepare for further
 *             processing.
 *
 * In the RTE_EVENT_CRYPTO_ADAPTER_OP_NEW mode, application submits crypto
 * operations directly to crypto device. The adapter then dequeues crypto
 * completions from crypto device and enqueue events to the event device.
 * This mode does not ensure ingress ordering, if the application directly
 * enqueues to cryptodev without going through crypto/atomic stage i.e.
 * removing item [1] and [2].
 * Events dequeued from the adapter will be treated as new events.
 * In this mode, application needs to specify event information (response
 * information) which is needed to enqueue an event after the crypto operation
 * is completed.
 *
 *
 * Working model of RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD mode:
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
 *               | Crypto adapter |       |   Cryptodev  |
 *               |                |<-[6]--|              |
 *               +----------------+       +--------------+
 *
 *
 *         [1] Events from the previous stage.
 *         [2] Application in ordered stage dequeues events from eventdev.
 *         [3] Application enqueues crypto operations as events to eventdev.
 *         [4] Crypto adapter dequeues event from eventdev.
 *         [5] Crypto adapter submits crypto operations to cryptodev
 *             (Atomic stage).
 *         [6] Crypto adapter dequeues crypto completions from cryptodev
 *         [7] Crypto adapter enqueues events to the eventdev
 *         [8] Events to the next stage
 *
 * In the RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD mode, if HW supports
 * RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD capability the application
 * can directly submit the crypto operations to the cryptodev.
 * If not, application retrieves crypto adapter's event port using
 * rte_event_crypto_adapter_event_port_get() API. Then, links its event
 * queue to this port and starts enqueuing crypto operations as events
 * to the eventdev. The adapter then dequeues the events and submits the
 * crypto operations to the cryptodev. After the crypto completions, the
 * adapter enqueues events to the event device.
 * Application can use this mode, when ingress packet ordering is needed.
 * Events dequeued from the adapter will be treated as forwarded events.
 * In this mode, the application needs to specify the cryptodev ID
 * and queue pair ID (request information) needed to enqueue a crypto
 * operation in addition to the event information (response information)
 * needed to enqueue an event after the crypto operation has completed.
 *
 *
 * The event crypto adapter provides common APIs to configure the packet flow
 * from the crypto device to event devices for both SW and HW based transfers.
 * The crypto event adapter's functions are:
 *  - rte_event_crypto_adapter_create_ext()
 *  - rte_event_crypto_adapter_create()
 *  - rte_event_crypto_adapter_free()
 *  - rte_event_crypto_adapter_queue_pair_add()
 *  - rte_event_crypto_adapter_queue_pair_del()
 *  - rte_event_crypto_adapter_start()
 *  - rte_event_crypto_adapter_stop()
 *  - rte_event_crypto_adapter_stats_get()
 *  - rte_event_crypto_adapter_stats_reset()
 *  - rte_event_crypto_adapter_runtime_params_get()
 *  - rte_event_crypto_adapter_runtime_params_init()
 *  - rte_event_crypto_adapter_runtime_params_set()

 * The application creates an instance using rte_event_crypto_adapter_create()
 * or rte_event_crypto_adapter_create_ext().
 *
 * Cryptodev queue pair addition/deletion is done using the
 * rte_event_crypto_adapter_queue_pair_xxx() APIs. If HW supports
 * RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND capability, event
 * information must be passed to the add API.
 *
 * The SW adapter or HW PMD uses rte_crypto_op::sess_type to decide whether
 * request/response(private) data is located in the crypto/security session
 * or at an offset in the rte_crypto_op.
 *
 * For session-based operations, the set and get API provides a mechanism for
 * an application to store and retrieve the data information stored
 * along with the crypto session.
 * The RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA capability indicates
 * whether HW or SW supports this feature.
 *
 * For session-less mode, the adapter gets the private data information placed
 * along with the ``struct rte_crypto_op``.
 * The rte_crypto_op::private_data_offset provides an offset to locate the
 * request/response information in the rte_crypto_op. This offset is counted
 * from the start of the rte_crypto_op including initialization vector (IV).
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "rte_eventdev.h"

/**
 * Crypto event adapter mode
 */
enum rte_event_crypto_adapter_mode {
	RTE_EVENT_CRYPTO_ADAPTER_OP_NEW,
	/**< Start the crypto adapter in event new mode.
	 * @see RTE_EVENT_OP_NEW.
	 * Application submits crypto operations to the cryptodev.
	 * Adapter only dequeues the crypto completions from cryptodev
	 * and enqueue events to the eventdev.
	 */
	RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD,
	/**< Start the crypto adapter in event forward mode.
	 * @see RTE_EVENT_OP_FORWARD.
	 * Application submits crypto requests as events to the crypto
	 * adapter or crypto device based on
	 * RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD capability.
	 * Crypto completions are enqueued back to the eventdev by
	 * crypto adapter.
	 */
};

/**
 * Crypto event request structure will be filled by application to
 * provide event request information to the adapter.
 */
struct rte_event_crypto_request {
	uint8_t resv[8];
	/**< Overlaps with first 8 bytes of struct rte_event
	 * that encode the response event information. Application
	 * is expected to fill in struct rte_event response_info.
	 */
	uint16_t cdev_id;
	/**< cryptodev ID to be used */
	uint16_t queue_pair_id;
	/**< cryptodev queue pair ID to be used */
	uint32_t resv1;
	/**< Reserved bits */
};

/**
 * Crypto event metadata structure will be filled by application
 * to provide crypto request and event response information.
 *
 * If crypto events are enqueued using a HW mechanism, the cryptodev
 * PMD will use the event response information to set up the event
 * that is enqueued back to eventdev after completion of the crypto
 * operation. If the transfer is done by SW, event response information
 * will be used by the adapter.
 */
union rte_event_crypto_metadata {
	struct rte_event_crypto_request request_info;
	/**< Request information to be filled in by application
	 * for RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD mode.
	 * First 8 bytes of request_info is reserved for response_info.
	 */
	struct rte_event response_info;
	/**< Response information to be filled in by application
	 * for RTE_EVENT_CRYPTO_ADAPTER_OP_NEW and
	 * RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD mode.
	 */
};

/**
 * Adapter configuration structure that the adapter configuration callback
 * function is expected to fill out
 * @see rte_event_crypto_adapter_conf_cb
 */
struct rte_event_crypto_adapter_conf {
	uint8_t event_port_id;
	/**< Event port identifier, the adapter enqueues events to this
	 * port and dequeues crypto request events in
	 * RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD mode.
	 */
	uint32_t max_nb;
	/**< The adapter can return early if it has processed at least
	 * max_nb crypto ops. This isn't treated as a requirement; batching
	 * may cause the adapter to process more than max_nb crypto ops.
	 */
};

/**
 * Adapter runtime configuration parameters
 */
struct rte_event_crypto_adapter_runtime_params {
	uint32_t max_nb;
	/**< The adapter can return early if it has processed at least
	 * max_nb crypto ops. This isn't treated as a requirement; batching
	 * may cause the adapter to process more than max_nb crypto ops.
	 *
	 * rte_event_crypto_adapter_create() configures the
	 * adapter with default value of max_nb.
	 * rte_event_crypto_adapter_create_ext() configures the adapter with
	 * user provided value of max_nb through
	 * rte_event_crypto_adapter_conf::max_nb parameter.
	 * rte_event_cryptoadapter_runtime_params_set() allows to re-configure
	 * max_nb during runtime (after adding at least one queue pair)
	 *
	 * This is valid for the devices without
	 * RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD or
	 * RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_NEW capability.
	 */
	uint32_t rsvd[15];
	/**< Reserved fields for future expansion */
};

#define RTE_EVENT_CRYPTO_ADAPTER_EVENT_VECTOR	0x1
/**< This flag indicates that crypto operations processed on the crypto
 * adapter need to be vectorized
 * @see rte_event_crypto_adapter_queue_conf::flags
 */

/**
 * Adapter queue configuration structure
 */
struct rte_event_crypto_adapter_queue_conf {
	uint32_t flags;
	/**< Flags for handling crypto operations
	 * @see RTE_EVENT_CRYPTO_ADAPTER_EVENT_VECTOR
	 */
	struct rte_event ev;
	/**< If HW supports cryptodev queue pair to event queue binding,
	 * application is expected to fill in event information.
	 * @see RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND
	 */
	uint16_t vector_sz;
	/**< Indicates the maximum number for crypto operations to combine and
	 * form a vector.
	 * @see rte_event_crypto_adapter_vector_limits::min_sz
	 * @see rte_event_crypto_adapter_vector_limits::max_sz
	 * Valid when RTE_EVENT_CRYPTO_ADAPTER_EVENT_VECTOR flag is set in
	 * @see rte_event_crypto_adapter_queue_conf::flags
	 */
	uint64_t vector_timeout_ns;
	/**<
	 * Indicates the maximum number of nanoseconds to wait for aggregating
	 * crypto operations. Should be within vectorization limits of the
	 * adapter
	 * @see rte_event_crypto_adapter_vector_limits::min_timeout_ns
	 * @see rte_event_crypto_adapter_vector_limits::max_timeout_ns
	 * Valid when RTE_EVENT_CRYPTO_ADAPTER_EVENT_VECTOR flag is set in
	 * @see rte_event_crypto_adapter_queue_conf::flags
	 */
	struct rte_mempool *vector_mp;
	/**< Indicates the mempool that should be used for allocating
	 * rte_event_vector container.
	 * Should be created by using `rte_event_vector_pool_create`.
	 * Valid when RTE_EVENT_CRYPTO_ADAPTER_EVENT_VECTOR flag is set in
	 * @see rte_event_crypto_adapter_queue_conf::flags.
	 */
};

/**
 * A structure used to retrieve event crypto adapter vector limits.
 */
struct rte_event_crypto_adapter_vector_limits {
	uint16_t min_sz;
	/**< Minimum vector limit configurable.
	 * @see rte_event_crypto_adapter_queue_conf::vector_sz
	 */
	uint16_t max_sz;
	/**< Maximum vector limit configurable.
	 * @see rte_event_crypto_adapter_queue_conf::vector_sz
	 */
	uint8_t log2_sz;
	/**< True if the size configured should be in log2.
	 * @see rte_event_crypto_adapter_queue_conf::vector_sz
	 */
	uint64_t min_timeout_ns;
	/**< Minimum vector timeout configurable.
	 * @see rte_event_crypto_adapter_queue_conf::vector_timeout_ns
	 */
	uint64_t max_timeout_ns;
	/**< Maximum vector timeout configurable.
	 * @see rte_event_crypto_adapter_queue_conf::vector_timeout_ns
	 */
};

/**
 * Function type used for adapter configuration callback. The callback is
 * used to fill in members of the struct rte_event_crypto_adapter_conf, this
 * callback is invoked when creating a SW service for packet transfer from
 * cryptodev queue pair to the event device. The SW service is created within
 * the rte_event_crypto_adapter_queue_pair_add() function if SW based packet
 * transfers from cryptodev queue pair to the event device are required.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param dev_id
 *  Event device identifier.
 *
 * @param conf
 *  Structure that needs to be populated by this callback.
 *
 * @param arg
 *  Argument to the callback. This is the same as the conf_arg passed to the
 *  rte_event_crypto_adapter_create_ext().
 */
typedef int (*rte_event_crypto_adapter_conf_cb) (uint8_t id, uint8_t dev_id,
			struct rte_event_crypto_adapter_conf *conf,
			void *arg);

/**
 * A structure used to retrieve statistics for an event crypto adapter
 * instance.
 */

struct rte_event_crypto_adapter_stats {
	uint64_t event_poll_count;
	/**< Event port poll count */
	uint64_t event_deq_count;
	/**< Event dequeue count */
	uint64_t crypto_enq_count;
	/**< Cryptodev enqueue count */
	uint64_t crypto_enq_fail;
	/**< Cryptodev enqueue failed count */
	uint64_t crypto_deq_count;
	/**< Cryptodev dequeue count */
	uint64_t event_enq_count;
	/**< Event enqueue count */
	uint64_t event_enq_retry_count;
	/**< Event enqueue retry count */
	uint64_t event_enq_fail_count;
	/**< Event enqueue fail count */
};

/**
 * Create a new event crypto adapter with the specified identifier.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param dev_id
 *  Event device identifier.
 *
 * @param conf_cb
 *  Callback function that fills in members of a
 *  struct rte_event_crypto_adapter_conf struct passed into
 *  it.
 *
 * @param mode
 *  Flag to indicate the mode of the adapter.
 *  @see rte_event_crypto_adapter_mode
 *
 * @param conf_arg
 *  Argument that is passed to the conf_cb function.
 *
 * @return
 *   - 0: Success
 *   - <0: Error code on failure
 */
int
rte_event_crypto_adapter_create_ext(uint8_t id, uint8_t dev_id,
				    rte_event_crypto_adapter_conf_cb conf_cb,
				    enum rte_event_crypto_adapter_mode mode,
				    void *conf_arg);

/**
 * Create a new event crypto adapter with the specified identifier.
 * This function uses an internal configuration function that creates an event
 * port. This default function reconfigures the event device with an
 * additional event port and set up the event port using the port_config
 * parameter passed into this function. In case the application needs more
 * control in configuration of the service, it should use the
 * rte_event_crypto_adapter_create_ext() version.
 *
 * When this API is used for creating adapter instance,
 * ``rte_event_dev_config::nb_event_ports`` is automatically incremented,
 * and the event device is reconfigured with additional event port during
 * service initialization. This event device reconfigure logic also increments
 * the ``rte_event_dev_config::nb_single_link_event_port_queues``
 * parameter if the adapter event port config is of type
 * ``RTE_EVENT_PORT_CFG_SINGLE_LINK``.
 *
 * Application no longer needs to account for
 * ``rte_event_dev_config::nb_event_ports`` and
 * ``rte_event_dev_config::nb_single_link_event_port_queues``
 * parameters required for crypto adapter in event device configuration
 * when the adapter is created with this API.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param dev_id
 *  Event device identifier.
 *
 * @param port_config
 *  Argument of type *rte_event_port_conf* that is passed to the conf_cb
 *  function.
 *
 * @param mode
 *  Flag to indicate the mode of the adapter.
 *  @see rte_event_crypto_adapter_mode
 *
 * @return
 *   - 0: Success
 *   - <0: Error code on failure
 */
int
rte_event_crypto_adapter_create(uint8_t id, uint8_t dev_id,
				struct rte_event_port_conf *port_config,
				enum rte_event_crypto_adapter_mode mode);

/**
 * Free an event crypto adapter
 *
 * @param id
 *  Adapter identifier.
 *
 * @return
 *   - 0: Success
 *   - <0: Error code on failure, If the adapter still has queue pairs
 *      added to it, the function returns -EBUSY.
 */
int
rte_event_crypto_adapter_free(uint8_t id);

/**
 * Add a queue pair to an event crypto adapter.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param cdev_id
 *  Cryptodev identifier.
 *
 * @param queue_pair_id
 *  Cryptodev queue pair identifier. If queue_pair_id is set -1,
 *  adapter adds all the pre configured queue pairs to the instance.
 *
 * @param conf
 *  Additional configuration structure of type
 *  *rte_event_crypto_adapter_queue_conf*
 *
 * @return
 *  - 0: Success, queue pair added correctly.
 *  - <0: Error code on failure.
 */
int
rte_event_crypto_adapter_queue_pair_add(uint8_t id,
			uint8_t cdev_id,
			int32_t queue_pair_id,
			const struct rte_event_crypto_adapter_queue_conf *conf);

/**
 * Delete a queue pair from an event crypto adapter.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param cdev_id
 *  Cryptodev identifier.
 *
 * @param queue_pair_id
 *  Cryptodev queue pair identifier.
 *
 * @return
 *  - 0: Success, queue pair deleted successfully.
 *  - <0: Error code on failure.
 */
int
rte_event_crypto_adapter_queue_pair_del(uint8_t id, uint8_t cdev_id,
					int32_t queue_pair_id);

/**
 * Start event crypto adapter
 *
 * @param id
 *  Adapter identifier.
 *
 *
 * @return
 *  - 0: Success, adapter started successfully.
 *  - <0: Error code on failure.
 *
 * @note
 *  The eventdev and cryptodev to which the event_crypto_adapter is connected
 *  needs to be started before calling rte_event_crypto_adapter_start().
 */
int
rte_event_crypto_adapter_start(uint8_t id);

/**
 * Stop event crypto adapter
 *
 * @param id
 *  Adapter identifier.
 *
 * @return
 *  - 0: Success, adapter stopped successfully.
 *  - <0: Error code on failure.
 */
int
rte_event_crypto_adapter_stop(uint8_t id);

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
int
rte_event_crypto_adapter_stats_get(uint8_t id,
				struct rte_event_crypto_adapter_stats *stats);

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
int
rte_event_crypto_adapter_stats_reset(uint8_t id);

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
int
rte_event_crypto_adapter_service_id_get(uint8_t id, uint32_t *service_id);

/**
 * Retrieve the event port of an adapter.
 *
 * @param id
 *  Adapter identifier.
 *
 * @param [out] event_port_id
 *  Application links its event queue to this adapter port which is used
 *  in RTE_EVENT_CRYPTO_ADAPTER_OP_FORWARD mode.
 *
 * @return
 *  - 0: Success
 *  - <0: Error code on failure.
 */
int
rte_event_crypto_adapter_event_port_get(uint8_t id, uint8_t *event_port_id);

/**
 * Initialize the adapter runtime configuration parameters
 *
 * @param params
 *  A pointer to structure of type struct rte_event_crypto_adapter_runtime_params
 *
 * @return
 *  -  0: Success
 *  - <0: Error code on failure
 */
__rte_experimental
int
rte_event_crypto_adapter_runtime_params_init(
		struct rte_event_crypto_adapter_runtime_params *params);

/**
 * Set the adapter runtime configuration parameters
 *
 * @param id
 *  Adapter identifier
 *
 * @param params
 *  A pointer to structure of type struct rte_event_crypto_adapter_runtime_params
 *  with configuration parameter values. The reserved fields of this structure
 *  must be initialized to zero and the valid fields need to be set appropriately.
 *  This struct can be initialized using
 *  rte_event_crypto_adapter_runtime_params_init() API to default values or
 *  application may reset this struct and update required fields.
 *
 * @return
 *  -  0: Success
 *  - <0: Error code on failure
 */
__rte_experimental
int
rte_event_crypto_adapter_runtime_params_set(uint8_t id,
		struct rte_event_crypto_adapter_runtime_params *params);

/**
 * Get the adapter runtime configuration parameters
 *
 * @param id
 *  Adapter identifier
 *
 * @param[out] params
 *  A pointer to structure of type struct rte_event_crypto_adapter_runtime_params
 *  containing valid adapter parameters when return value is 0.
 *
 * @return
 *  -  0: Success
 *  - <0: Error code on failure
 */
__rte_experimental
int
rte_event_crypto_adapter_runtime_params_get(uint8_t id,
		struct rte_event_crypto_adapter_runtime_params *params);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Retrieve vector limits for a given event dev and crypto dev pair.
 * @see rte_event_crypto_adapter_vector_limits
 *
 * @param dev_id
 *  Event device identifier.
 * @param cdev_id
 *  Crypto device identifier.
 * @param [out] limits
 *  A pointer to rte_event_crypto_adapter_vector_limits structure that has to
 *  be filled.
 *
 * @return
 *  - 0: Success.
 *  - <0: Error code on failure.
 */
__rte_experimental
int rte_event_crypto_adapter_vector_limits_get(
	uint8_t dev_id, uint16_t cdev_id,
	struct rte_event_crypto_adapter_vector_limits *limits);

/**
 * Enqueue a burst of crypto operations as event objects supplied in *rte_event*
 * structure on an event crypto adapter designated by its event *dev_id* through
 * the event port specified by *port_id*. This function is supported if the
 * eventdev PMD has the #RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_OP_FWD
 * capability flag set.
 *
 * The *nb_events* parameter is the number of event objects to enqueue which are
 * supplied in the *ev* array of *rte_event* structure.
 *
 * The rte_event_crypto_adapter_enqueue() function returns the number of
 * event objects it actually enqueued. A return value equal to *nb_events*
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
rte_event_crypto_adapter_enqueue(uint8_t dev_id,
				uint8_t port_id,
				struct rte_event ev[],
				uint16_t nb_events)
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
	rte_eventdev_trace_crypto_adapter_enqueue(dev_id, port_id, ev,
		nb_events);

	return fp_ops->ca_enqueue(port, ev, nb_events);
}

#ifdef __cplusplus
}
#endif
#endif	/* _RTE_EVENT_CRYPTO_ADAPTER_ */
