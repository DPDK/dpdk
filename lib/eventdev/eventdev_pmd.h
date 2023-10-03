/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef _RTE_EVENTDEV_PMD_H_
#define _RTE_EVENTDEV_PMD_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * RTE Event PMD APIs
 *
 * @note
 * These API are from event PMD only and user applications should not call
 * them directly.
 */

#include <string.h>

#include <rte_common.h>
#include <rte_compat.h>
#include <rte_config.h>
#include <dev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#include "event_timer_adapter_pmd.h"
#include "rte_eventdev.h"

/* Logging Macros */
#define RTE_EDEV_LOG_ERR(...) \
	RTE_LOG(ERR, EVENTDEV, \
		RTE_FMT("%s() line %u: " RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, __LINE__, RTE_FMT_TAIL(__VA_ARGS__,)))

#ifdef RTE_LIBRTE_EVENTDEV_DEBUG
#define RTE_EDEV_LOG_DEBUG(...) \
	RTE_LOG(DEBUG, EVENTDEV, \
		RTE_FMT("%s() line %u: " RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, __LINE__, RTE_FMT_TAIL(__VA_ARGS__,)))
#else
#define RTE_EDEV_LOG_DEBUG(...) (void)0
#endif

/* Macros to check for valid device */
#define RTE_EVENTDEV_VALID_DEVID_OR_ERR_RET(dev_id, retval) do { \
	if (!rte_event_pmd_is_valid_dev((dev_id))) { \
		RTE_EDEV_LOG_ERR("Invalid dev_id=%d\n", dev_id); \
		return retval; \
	} \
} while (0)

#define RTE_EVENTDEV_VALID_DEVID_OR_ERRNO_RET(dev_id, errno, retval) do { \
	if (!rte_event_pmd_is_valid_dev((dev_id))) { \
		RTE_EDEV_LOG_ERR("Invalid dev_id=%d\n", dev_id); \
		rte_errno = errno; \
		return retval; \
	} \
} while (0)

#define RTE_EVENTDEV_VALID_DEVID_OR_RET(dev_id) do { \
	if (!rte_event_pmd_is_valid_dev((dev_id))) { \
		RTE_EDEV_LOG_ERR("Invalid dev_id=%d\n", dev_id); \
		return; \
	} \
} while (0)

#define RTE_EVENT_ETH_RX_ADAPTER_SW_CAP                                        \
	((RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID) |                     \
	 (RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ) |                         \
	 (RTE_EVENT_ETH_RX_ADAPTER_CAP_EVENT_VECTOR))

#define RTE_EVENT_CRYPTO_ADAPTER_SW_CAP \
		RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA

/**< Ethernet Rx adapter cap to return If the packet transfers from
 * the ethdev to eventdev use a SW service function
 */

#define RTE_EVENT_TIMER_ADAPTER_SW_CAP \
		RTE_EVENT_TIMER_ADAPTER_CAP_PERIODIC

#define RTE_EVENTDEV_DETACHED  (0)
#define RTE_EVENTDEV_ATTACHED  (1)

#define RTE_EVENTDEV_NAME_MAX_LEN (64)
/**< @internal Max length of name of event PMD */

struct rte_eth_dev;

/** Global structure used for maintaining state of allocated event devices */
struct rte_eventdev_global {
	uint8_t nb_devs;	/**< Number of devices found */
};

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
	uint16_t links_map[RTE_EVENT_MAX_PROFILES_PER_PORT]
			  [RTE_EVENT_MAX_PORTS_PER_DEV * RTE_EVENT_MAX_QUEUES_PER_DEV];
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

	uint8_t dev_started : 1;
	/**< Device state: STARTED(1)/STOPPED(0) */

	char name[RTE_EVENTDEV_NAME_MAX_LEN];
	/**< Unique identifier name */

	uint64_t reserved_64s[4]; /**< Reserved for future fields */
	void *reserved_ptrs[4];	  /**< Reserved for future fields */
} __rte_cache_aligned;

/** @internal The data structure associated with each event device. */
struct rte_eventdev {
	struct rte_eventdev_data *data;
	/**< Pointer to device data */
	struct eventdev_ops *dev_ops;
	/**< Functions exported by PMD */
	struct rte_device *dev;
	/**< Device info. supplied by probing */

	uint8_t attached : 1;
	/**< Flag indicating the device is attached */

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
	event_maintain_t maintain;
	/**< Pointer to PMD port maintenance function. */
	event_tx_adapter_enqueue_t txa_enqueue_same_dest;
	/**< Pointer to PMD eth Tx adapter burst enqueue function with
	 * events destined to same Eth port & Tx queue.
	 */
	event_tx_adapter_enqueue_t txa_enqueue;
	/**< Pointer to PMD eth Tx adapter enqueue function. */
	event_crypto_adapter_enqueue_t ca_enqueue;
	/**< Pointer to PMD crypto adapter enqueue function. */
	event_dma_adapter_enqueue_t dma_enqueue;
	/**< Pointer to PMD DMA adapter enqueue function. */
	event_profile_switch_t profile_switch;
	/**< Pointer to PMD Event switch profile function. */

	uint64_t reserved_64s[3]; /**< Reserved for future fields */
	void *reserved_ptrs[3];	  /**< Reserved for future fields */
} __rte_cache_aligned;

extern struct rte_eventdev *rte_eventdevs;
/** @internal The pool of rte_eventdev structures. */

/**
 * Get the rte_eventdev structure device pointer for the named device.
 *
 * @param name
 *   device name to select the device structure.
 *
 * @return
 *   - The rte_eventdev structure pointer for the given device ID.
 */
__rte_internal
static inline struct rte_eventdev *
rte_event_pmd_get_named_dev(const char *name)
{
	struct rte_eventdev *dev;
	unsigned int i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < RTE_EVENT_MAX_DEVS; i++) {
		dev = &rte_eventdevs[i];
		if ((dev->attached == RTE_EVENTDEV_ATTACHED) &&
				(strcmp(dev->data->name, name) == 0))
			return dev;
	}

	return NULL;
}

/**
 * Validate if the event device index is valid attached event device.
 *
 * @param dev_id
 *   Event device index.
 *
 * @return
 *   - If the device index is valid (1) or not (0).
 */
__rte_internal
static inline unsigned
rte_event_pmd_is_valid_dev(uint8_t dev_id)
{
	struct rte_eventdev *dev;

	if (dev_id >= RTE_EVENT_MAX_DEVS)
		return 0;

	dev = &rte_eventdevs[dev_id];
	if (dev->attached != RTE_EVENTDEV_ATTACHED)
		return 0;
	else
		return 1;
}

/**
 * Definitions of all functions exported by a driver through the
 * generic structure of type *event_dev_ops* supplied in the
 * *rte_eventdev* structure associated with a device.
 */

/**
 * Get device information of a device.
 *
 * @param dev
 *   Event device pointer
 * @param dev_info
 *   Event device information structure
 */
typedef void (*eventdev_info_get_t)(struct rte_eventdev *dev,
		struct rte_event_dev_info *dev_info);

/**
 * Configure a device.
 *
 * @param dev
 *   Event device pointer
 *
 * @return
 *   Returns 0 on success
 */
typedef int (*eventdev_configure_t)(const struct rte_eventdev *dev);

/**
 * Start a configured device.
 *
 * @param dev
 *   Event device pointer
 *
 * @return
 *   Returns 0 on success
 */
typedef int (*eventdev_start_t)(struct rte_eventdev *dev);

/**
 * Stop a configured device.
 *
 * @param dev
 *   Event device pointer
 */
typedef void (*eventdev_stop_t)(struct rte_eventdev *dev);

/**
 * Close a configured device.
 *
 * @param dev
 *   Event device pointer
 *
 * @return
 * - 0 on success
 * - (-EAGAIN) if can't close as device is busy
 */
typedef int (*eventdev_close_t)(struct rte_eventdev *dev);

/**
 * Retrieve the default event queue configuration.
 *
 * @param dev
 *   Event device pointer
 * @param queue_id
 *   Event queue index
 * @param[out] queue_conf
 *   Event queue configuration structure
 */
typedef void (*eventdev_queue_default_conf_get_t)(struct rte_eventdev *dev,
		uint8_t queue_id, struct rte_event_queue_conf *queue_conf);

/**
 * Setup an event queue.
 *
 * @param dev
 *   Event device pointer
 * @param queue_id
 *   Event queue index
 * @param queue_conf
 *   Event queue configuration structure
 *
 * @return
 *   Returns 0 on success.
 */
typedef int (*eventdev_queue_setup_t)(struct rte_eventdev *dev,
		uint8_t queue_id,
		const struct rte_event_queue_conf *queue_conf);

/**
 * Release resources allocated by given event queue.
 *
 * @param dev
 *   Event device pointer
 * @param queue_id
 *   Event queue index
 */
typedef void (*eventdev_queue_release_t)(struct rte_eventdev *dev,
		uint8_t queue_id);

/**
 * Set an event queue attribute at runtime.
 *
 * @param dev
 *   Event device pointer
 * @param queue_id
 *   Event queue index
 * @param attr_id
 *   Event queue attribute id
 * @param attr_value
 *   Event queue attribute value
 *
 * @return
 *  - 0: Success.
 *  - <0: Error code on failure.
 */
typedef int (*eventdev_queue_attr_set_t)(struct rte_eventdev *dev,
					 uint8_t queue_id, uint32_t attr_id,
					 uint64_t attr_value);

/**
 * Retrieve the default event port configuration.
 *
 * @param dev
 *   Event device pointer
 * @param port_id
 *   Event port index
 * @param[out] port_conf
 *   Event port configuration structure
 */
typedef void (*eventdev_port_default_conf_get_t)(struct rte_eventdev *dev,
		uint8_t port_id, struct rte_event_port_conf *port_conf);

/**
 * Setup an event port.
 *
 * @param dev
 *   Event device pointer
 * @param port_id
 *   Event port index
 * @param port_conf
 *   Event port configuration structure
 *
 * @return
 *   Returns 0 on success.
 */
typedef int (*eventdev_port_setup_t)(struct rte_eventdev *dev,
		uint8_t port_id,
		const struct rte_event_port_conf *port_conf);

/**
 * Release memory resources allocated by given event port.
 *
 * @param port
 *   Event port pointer
 */
typedef void (*eventdev_port_release_t)(void *port);

/**
 * Quiesce any core specific resources consumed by the event port
 *
 * @param dev
 *   Event device pointer.
 * @param port
 *   Event port pointer.
 * @param flush_cb
 *   User-provided event flush function.
 * @param args
 *   Arguments to be passed to the user-provided event flush function.
 */
typedef void (*eventdev_port_quiesce_t)(struct rte_eventdev *dev, void *port,
					rte_eventdev_port_flush_t flush_cb,
					void *args);

/**
 * Link multiple source event queues to destination event port.
 *
 * @param dev
 *   Event device pointer
 * @param port
 *   Event port pointer
 * @param queues
 *   Points to an array of *nb_links* event queues to be linked
 *   to the event port.
 * @param priorities
 *   Points to an array of *nb_links* service priorities associated with each
 *   event queue link to event port.
 * @param nb_links
 *   The number of links to establish
 *
 * @return
 *   Returns 0 on success.
 */
typedef int (*eventdev_port_link_t)(struct rte_eventdev *dev, void *port,
		const uint8_t queues[], const uint8_t priorities[],
		uint16_t nb_links);

/**
 * Link multiple source event queues associated with a link profile to a
 * destination event port.
 *
 * @param dev
 *   Event device pointer
 * @param port
 *   Event port pointer
 * @param queues
 *   Points to an array of *nb_links* event queues to be linked
 *   to the event port.
 * @param priorities
 *   Points to an array of *nb_links* service priorities associated with each
 *   event queue link to event port.
 * @param nb_links
 *   The number of links to establish.
 * @param profile_id
 *   The profile ID to associate the links.
 *
 * @return
 *   Returns 0 on success.
 */
typedef int (*eventdev_port_link_profile_t)(struct rte_eventdev *dev, void *port,
					    const uint8_t queues[], const uint8_t priorities[],
					    uint16_t nb_links, uint8_t profile_id);

/**
 * Unlink multiple source event queues from destination event port.
 *
 * @param dev
 *   Event device pointer
 * @param port
 *   Event port pointer
 * @param queues
 *   An array of *nb_unlinks* event queues to be unlinked from the event port.
 * @param nb_unlinks
 *   The number of unlinks to establish
 *
 * @return
 *   Returns 0 on success.
 */
typedef int (*eventdev_port_unlink_t)(struct rte_eventdev *dev, void *port,
		uint8_t queues[], uint16_t nb_unlinks);

/**
 * Unlink multiple source event queues associated with a link profile from
 * destination event port.
 *
 * @param dev
 *   Event device pointer
 * @param port
 *   Event port pointer
 * @param queues
 *   An array of *nb_unlinks* event queues to be unlinked from the event port.
 * @param nb_unlinks
 *   The number of unlinks to establish
 * @param profile_id
 *   The profile ID of the associated links.
 *
 * @return
 *   Returns 0 on success.
 */
typedef int (*eventdev_port_unlink_profile_t)(struct rte_eventdev *dev, void *port,
					      uint8_t queues[], uint16_t nb_unlinks,
					      uint8_t profile_id);

/**
 * Unlinks in progress. Returns number of unlinks that the PMD is currently
 * performing, but have not yet been completed.
 *
 * @param dev
 *   Event device pointer
 *
 * @param port
 *   Event port pointer
 *
 * @return
 *   Returns the number of in-progress unlinks. Zero is returned if none are
 *   in progress.
 */
typedef int (*eventdev_port_unlinks_in_progress_t)(struct rte_eventdev *dev,
		void *port);

/**
 * Converts nanoseconds to *timeout_ticks* value for rte_event_dequeue()
 *
 * @param dev
 *   Event device pointer
 * @param ns
 *   Wait time in nanosecond
 * @param[out] timeout_ticks
 *   Value for the *timeout_ticks* parameter in rte_event_dequeue() function
 *
 * @return
 *   Returns 0 on success.
 */
typedef int (*eventdev_dequeue_timeout_ticks_t)(struct rte_eventdev *dev,
		uint64_t ns, uint64_t *timeout_ticks);

/**
 * Dump internal information
 *
 * @param dev
 *   Event device pointer
 * @param f
 *   A pointer to a file for output
 */
typedef void (*eventdev_dump_t)(struct rte_eventdev *dev, FILE *f);

/**
 * Retrieve a set of statistics from device
 *
 * @param dev
 *   Event device pointer
 * @param mode
 *   Level (device, port or queue)
 * @param queue_port_id
 *   Queue or port number depending on mode
 * @param ids
 *   The stat ids to retrieve
 * @param values
 *   The returned stat values
 * @param n
 *   The number of id values and entries in the values array
 * @return
 *   The number of stat values successfully filled into the values array
 */
typedef int (*eventdev_xstats_get_t)(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		const uint64_t ids[], uint64_t values[], unsigned int n);

/**
 * Resets the statistic values in xstats for the device, based on mode.
 */
typedef int (*eventdev_xstats_reset_t)(struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode,
		int16_t queue_port_id,
		const uint64_t ids[],
		uint32_t nb_ids);

/**
 * Get names of extended stats of an event device
 *
 * @param dev
 *   Event device pointer
 * @param mode
 *   Level (device, port or queue)
 * @param queue_port_id
 *   Queue or port number depending on mode
 * @param xstats_names
 *   Array of name values to be filled in
 * @param ids
 *   The stat ids to retrieve
 * @param size
 *   Number of values in the xstats_names array
 * @return
 *   When size >= the number of stats, return the number of stat values filled
 *   into the array.
 *   When size < the number of available stats, return the number of stats
 *   values, and do not fill in any data into xstats_names.
 */
typedef int (*eventdev_xstats_get_names_t)(const struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		struct rte_event_dev_xstats_name *xstats_names,
		uint64_t *ids, unsigned int size);

/**
 * Get value of one stats and optionally return its id
 *
 * @param dev
 *   Event device pointer
 * @param name
 *   The name of the stat to retrieve
 * @param id
 *   Pointer to an unsigned int where we store the stat-id for future reference.
 *   This pointer may be null if the id is not required.
 * @return
 *   The value of the stat, or (uint64_t)-1 if the stat is not found.
 *   If the stat is not found, the id value will be returned as (unsigned)-1,
 *   if id pointer is non-NULL
 */
typedef uint64_t (*eventdev_xstats_get_by_name)(const struct rte_eventdev *dev,
		const char *name, uint64_t *id);


/**
 * Retrieve the event device's ethdev Rx adapter capabilities for the
 * specified ethernet port
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @param[out] caps
 *   A pointer to memory filled with Rx event adapter capabilities.
 *
 * @return
 *   - 0: Success, driver provides Rx event adapter capabilities for the
 *	ethernet device.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_eth_rx_adapter_caps_get_t)
					(const struct rte_eventdev *dev,
					const struct rte_eth_dev *eth_dev,
					uint32_t *caps);

struct rte_event_eth_rx_adapter_queue_conf;

/**
 * Retrieve the event device's timer adapter capabilities, as well as the ops
 * structure that an event timer adapter should call through to enter the
 * driver
 *
 * @param dev
 *   Event device pointer
 *
 * @param flags
 *   Flags that can be used to determine how to select an event timer
 *   adapter ops structure
 *
 * @param[out] caps
 *   A pointer to memory filled with Rx event adapter capabilities.
 *
 * @param[out] ops
 *   A pointer to the ops pointer to set with the address of the desired ops
 *   structure
 *
 * @return
 *   - 0: Success, driver provides Rx event adapter capabilities for the
 *	ethernet device.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_timer_adapter_caps_get_t)(
	const struct rte_eventdev *dev, uint64_t flags, uint32_t *caps,
	const struct event_timer_adapter_ops **ops);

/**
 * Add ethernet Rx queues to event device. This callback is invoked if
 * the caps returned from rte_eventdev_eth_rx_adapter_caps_get(, eth_port_id)
 * has RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT set.
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @param rx_queue_id
 *   Ethernet device receive queue index
 *
 * @param queue_conf
 *  Additional configuration structure

 * @return
 *   - 0: Success, ethernet receive queue added successfully.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_eth_rx_adapter_queue_add_t)(
		const struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev,
		int32_t rx_queue_id,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf);

/**
 * Delete ethernet Rx queues from event device. This callback is invoked if
 * the caps returned from eventdev_eth_rx_adapter_caps_get(, eth_port_id)
 * has RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT set.
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @param rx_queue_id
 *   Ethernet device receive queue index
 *
 * @return
 *   - 0: Success, ethernet receive queue deleted successfully.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_eth_rx_adapter_queue_del_t)
					(const struct rte_eventdev *dev,
					const struct rte_eth_dev *eth_dev,
					int32_t rx_queue_id);

/**
 * Retrieve Rx adapter queue config information for the specified
 * rx queue ID.
 *
 * @param dev
 *  Event device pointer
 *
 * @param eth_dev
 *  Ethernet device pointer
 *
 * @param rx_queue_id
 *  Ethernet device receive queue index.
 *
 * @param[out] queue_conf
 *  Pointer to rte_event_eth_rx_adapter_queue_conf structure
 *
 * @return
 *  - 0: Success
 *  - <0: Error code on failure.
 */
typedef int (*eventdev_eth_rx_adapter_queue_conf_get_t)
			(const struct rte_eventdev *dev,
			const struct rte_eth_dev *eth_dev,
			uint16_t rx_queue_id,
			struct rte_event_eth_rx_adapter_queue_conf *queue_conf);

/**
 * Start ethernet Rx adapter. This callback is invoked if
 * the caps returned from eventdev_eth_rx_adapter_caps_get(.., eth_port_id)
 * has RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT set and Rx queues
 * from eth_port_id have been added to the event device.
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @return
 *   - 0: Success, ethernet Rx adapter started successfully.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_eth_rx_adapter_start_t)
					(const struct rte_eventdev *dev,
					const struct rte_eth_dev *eth_dev);

/**
 * Stop ethernet Rx adapter. This callback is invoked if
 * the caps returned from eventdev_eth_rx_adapter_caps_get(..,eth_port_id)
 * has RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT set and Rx queues
 * from eth_port_id have been added to the event device.
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @return
 *   - 0: Success, ethernet Rx adapter stopped successfully.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_eth_rx_adapter_stop_t)
					(const struct rte_eventdev *dev,
					const struct rte_eth_dev *eth_dev);

struct rte_event_eth_rx_adapter_stats;

/**
 * Retrieve ethernet Rx adapter statistics.
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @param[out] stats
 *   Pointer to stats structure
 *
 * @return
 *   Return 0 on success.
 */

typedef int (*eventdev_eth_rx_adapter_stats_get)
			(const struct rte_eventdev *dev,
			const struct rte_eth_dev *eth_dev,
			struct rte_event_eth_rx_adapter_stats *stats);
/**
 * Reset ethernet Rx adapter statistics.
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @return
 *   Return 0 on success.
 */
typedef int (*eventdev_eth_rx_adapter_stats_reset)
			(const struct rte_eventdev *dev,
			const struct rte_eth_dev *eth_dev);

struct rte_event_eth_rx_adapter_queue_stats;

/**
 * Retrieve ethernet Rx adapter queue statistics.
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @param rx_queue_id
 *  Ethernet device receive queue index.
 *
 * @param[out] q_stats
 *   Pointer to queue stats structure
 *
 * @return
 *   Return 0 on success.
 */
typedef int (*eventdev_eth_rx_adapter_q_stats_get)
			(const struct rte_eventdev *dev,
			 const struct rte_eth_dev *eth_dev,
			 uint16_t rx_queue_id,
			 struct rte_event_eth_rx_adapter_queue_stats *q_stats);

/**
 * Reset ethernet Rx adapter queue statistics.
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @param rx_queue_id
 *  Ethernet device receive queue index.
 *
 * @return
 *   Return 0 on success.
 */
typedef int (*eventdev_eth_rx_adapter_q_stats_reset)
			(const struct rte_eventdev *dev,
			 const struct rte_eth_dev *eth_dev,
			 uint16_t rx_queue_id);

/**
 * Start eventdev selftest.
 *
 * @return
 *   Return 0 on success.
 */
typedef int (*eventdev_selftest)(void);

struct rte_event_eth_rx_adapter_vector_limits;
/**
 * Get event vector limits for a given event, ethernet device pair.
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @param[out] limits
 *   Pointer to the limits structure to be filled.
 *
 * @return
 *   - 0: Success.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_eth_rx_adapter_vector_limits_get_t)(
	const struct rte_eventdev *dev, const struct rte_eth_dev *eth_dev,
	struct rte_event_eth_rx_adapter_vector_limits *limits);

/**
 * Get Rx adapter instance ID for Rx queue
 *
 * @param eth_dev_id
 *  Port identifier of ethernet device
 *
 * @param rx_queue_id
 *  Ethernet device Rx queue index
 *
 * @param[out] rxa_inst_id
 *  Pointer to Rx adapter instance identifier.
 *  Contains valid Rx adapter instance ID when return value is 0
 *
 * @return
 *   -  0: Success
 *   - <0: Error code on failure
 */
typedef int (*eventdev_eth_rx_adapter_instance_get_t)
	(uint16_t eth_dev_id, uint16_t rx_queue_id, uint8_t *rxa_inst_id);

typedef uint32_t rte_event_pmd_selftest_seqn_t;
extern int rte_event_pmd_selftest_seqn_dynfield_offset;

/**
 * Read test sequence number from mbuf.
 *
 * @param mbuf Structure to read from.
 * @return pointer to test sequence number.
 */
__rte_internal
static inline rte_event_pmd_selftest_seqn_t *
rte_event_pmd_selftest_seqn(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf,
		rte_event_pmd_selftest_seqn_dynfield_offset,
		rte_event_pmd_selftest_seqn_t *);
}

struct rte_cryptodev;
struct rte_event_crypto_adapter_queue_conf;

/**
 * This API may change without prior notice
 *
 * Retrieve the event device's crypto adapter capabilities for the
 * specified cryptodev
 *
 * @param dev
 *   Event device pointer
 *
 * @param cdev
 *   cryptodev pointer
 *
 * @param[out] caps
 *   A pointer to memory filled with event adapter capabilities.
 *   It is expected to be pre-allocated & initialized by caller.
 *
 * @return
 *   - 0: Success, driver provides event adapter capabilities for the
 *	cryptodev.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_crypto_adapter_caps_get_t)
					(const struct rte_eventdev *dev,
					 const struct rte_cryptodev *cdev,
					 uint32_t *caps);

/**
 * This API may change without prior notice
 *
 * Add crypto queue pair to event device. This callback is invoked if
 * the caps returned from rte_event_crypto_adapter_caps_get(, cdev_id)
 * has RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_* set.
 *
 * @param dev
 *   Event device pointer
 *
 * @param cdev
 *   cryptodev pointer
 *
 * @param queue_pair_id
 *   cryptodev queue pair identifier.
 *
 * @param event
 *  Event information required for binding cryptodev queue pair to event queue.
 *  This structure will have a valid value for only those HW PMDs supporting
 *  @see RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_QP_EV_BIND capability.
 *
 * @return
 *   - 0: Success, cryptodev queue pair added successfully.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_crypto_adapter_queue_pair_add_t)(
		const struct rte_eventdev *dev,
		const struct rte_cryptodev *cdev,
		int32_t queue_pair_id,
		const struct rte_event_crypto_adapter_queue_conf *queue_conf);


/**
 * This API may change without prior notice
 *
 * Delete crypto queue pair to event device. This callback is invoked if
 * the caps returned from rte_event_crypto_adapter_caps_get(, cdev_id)
 * has RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_* set.
 *
 * @param dev
 *   Event device pointer
 *
 * @param cdev
 *   cryptodev pointer
 *
 * @param queue_pair_id
 *   cryptodev queue pair identifier.
 *
 * @return
 *   - 0: Success, cryptodev queue pair deleted successfully.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_crypto_adapter_queue_pair_del_t)
					(const struct rte_eventdev *dev,
					 const struct rte_cryptodev *cdev,
					 int32_t queue_pair_id);

/**
 * Start crypto adapter. This callback is invoked if
 * the caps returned from rte_event_crypto_adapter_caps_get(.., cdev_id)
 * has RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_* set and queue pairs
 * from cdev_id have been added to the event device.
 *
 * @param dev
 *   Event device pointer
 *
 * @param cdev
 *   Crypto device pointer
 *
 * @return
 *   - 0: Success, crypto adapter started successfully.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_crypto_adapter_start_t)
					(const struct rte_eventdev *dev,
					 const struct rte_cryptodev *cdev);

/**
 * Stop crypto adapter. This callback is invoked if
 * the caps returned from rte_event_crypto_adapter_caps_get(.., cdev_id)
 * has RTE_EVENT_CRYPTO_ADAPTER_CAP_INTERNAL_PORT_* set and queue pairs
 * from cdev_id have been added to the event device.
 *
 * @param dev
 *   Event device pointer
 *
 * @param cdev
 *   Crypto device pointer
 *
 * @return
 *   - 0: Success, crypto adapter stopped successfully.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_crypto_adapter_stop_t)
					(const struct rte_eventdev *dev,
					 const struct rte_cryptodev *cdev);

struct rte_event_crypto_adapter_stats;

/**
 * Retrieve crypto adapter statistics.
 *
 * @param dev
 *   Event device pointer
 *
 * @param cdev
 *   Crypto device pointer
 *
 * @param[out] stats
 *   Pointer to stats structure
 *
 * @return
 *   Return 0 on success.
 */

typedef int (*eventdev_crypto_adapter_stats_get)
			(const struct rte_eventdev *dev,
			 const struct rte_cryptodev *cdev,
			 struct rte_event_crypto_adapter_stats *stats);

/**
 * Reset crypto adapter statistics.
 *
 * @param dev
 *   Event device pointer
 *
 * @param cdev
 *   Crypto device pointer
 *
 * @return
 *   Return 0 on success.
 */

typedef int (*eventdev_crypto_adapter_stats_reset)
			(const struct rte_eventdev *dev,
			 const struct rte_cryptodev *cdev);

struct rte_event_crypto_adapter_vector_limits;
/**
 * Get event vector limits for a given event, crypto device pair.
 *
 * @param dev
 *   Event device pointer
 *
 * @param cdev
 *   Crypto device pointer
 *
 * @param[out] limits
 *   Pointer to the limits structure to be filled.
 *
 * @return
 *   - 0: Success.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_crypto_adapter_vector_limits_get_t)(
	const struct rte_eventdev *dev, const struct rte_cryptodev *cdev,
	struct rte_event_crypto_adapter_vector_limits *limits);

/**
 * Retrieve the event device's eth Tx adapter capabilities.
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @param[out] caps
 *   A pointer to memory filled with eth Tx adapter capabilities.
 *
 * @return
 *   - 0: Success, driver provides eth Tx adapter capabilities
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_eth_tx_adapter_caps_get_t)
					(const struct rte_eventdev *dev,
					const struct rte_eth_dev *eth_dev,
					uint32_t *caps);

/**
 * Create adapter callback.
 *
 * @param id
 *   Adapter identifier
 *
 * @param dev
 *   Event device pointer
 *
 * @return
 *   - 0: Success.
 *   - <0: Error code on failure.
 */
typedef int (*eventdev_eth_tx_adapter_create_t)(uint8_t id,
					const struct rte_eventdev *dev);

/**
 * Free adapter callback.
 *
 * @param id
 *   Adapter identifier
 *
 * @param dev
 *   Event device pointer
 *
 * @return
 *   - 0: Success.
 *   - <0: Error code on failure.
 */
typedef int (*eventdev_eth_tx_adapter_free_t)(uint8_t id,
					const struct rte_eventdev *dev);

/**
 * Add a Tx queue to the adapter.
 * A queue value of -1 is used to indicate all
 * queues within the device.
 *
 * @param id
 *   Adapter identifier
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @param tx_queue_id
 *   Transmit queue index
 *
 * @return
 *   - 0: Success.
 *   - <0: Error code on failure.
 */
typedef int (*eventdev_eth_tx_adapter_queue_add_t)(
					uint8_t id,
					const struct rte_eventdev *dev,
					const struct rte_eth_dev *eth_dev,
					int32_t tx_queue_id);

/**
 * Delete a Tx queue from the adapter.
 * A queue value of -1 is used to indicate all
 * queues within the device, that have been added to this
 * adapter.
 *
 * @param id
 *   Adapter identifier
 *
 * @param dev
 *   Event device pointer
 *
 * @param eth_dev
 *   Ethernet device pointer
 *
 * @param tx_queue_id
 *   Transmit queue index
 *
 * @return
 *  - 0: Success, Queues deleted successfully.
 *  - <0: Error code on failure.
 */
typedef int (*eventdev_eth_tx_adapter_queue_del_t)(
					uint8_t id,
					const struct rte_eventdev *dev,
					const struct rte_eth_dev *eth_dev,
					int32_t tx_queue_id);

/**
 * Start the adapter.
 *
 * @param id
 *   Adapter identifier
 *
 * @param dev
 *   Event device pointer
 *
 * @return
 *  - 0: Success, Adapter started correctly.
 *  - <0: Error code on failure.
 */
typedef int (*eventdev_eth_tx_adapter_start_t)(uint8_t id,
					const struct rte_eventdev *dev);

/**
 * Stop the adapter.
 *
 * @param id
 *  Adapter identifier
 *
 * @param dev
 *   Event device pointer
 *
 * @return
 *  - 0: Success.
 *  - <0: Error code on failure.
 */
typedef int (*eventdev_eth_tx_adapter_stop_t)(uint8_t id,
					const struct rte_eventdev *dev);

struct rte_event_eth_tx_adapter_stats;

/**
 * Retrieve statistics for an adapter
 *
 * @param id
 *  Adapter identifier
 *
 * @param dev
 *   Event device pointer
 *
 * @param [out] stats
 *  A pointer to structure used to retrieve statistics for an adapter
 *
 * @return
 *  - 0: Success, statistics retrieved successfully.
 *  - <0: Error code on failure.
 */
typedef int (*eventdev_eth_tx_adapter_stats_get_t)(
				uint8_t id,
				const struct rte_eventdev *dev,
				struct rte_event_eth_tx_adapter_stats *stats);

/**
 * Reset statistics for an adapter
 *
 * @param id
 *  Adapter identifier
 *
 * @param dev
 *   Event device pointer
 *
 * @return
 *  - 0: Success, statistics retrieved successfully.
 *  - <0: Error code on failure.
 */
typedef int (*eventdev_eth_tx_adapter_stats_reset_t)(uint8_t id,
					const struct rte_eventdev *dev);

/**
 * Get TX adapter instance ID for Tx queue
 *
 * @param eth_dev_id
 *  Port identifier of Ethernet device
 *
 * @param tx_queue_id
 *  Ethernet device Tx queue index
 *
 * @param[out] txa_inst_id
 *  Pointer to Tx adapter instance identifier
 *  Contains valid Tx adapter instance ID when return value is 0
 *
 * @return
 *  -  0: Success
 *  - <0: Error code on failure
 */
typedef int (*eventdev_eth_tx_adapter_instance_get_t)
	(uint16_t eth_dev_id, uint16_t tx_queue_id, uint8_t *txa_inst_id);

/**
 * Start a Tx queue that is assigned to Tx adapter instance
 *
 * @param id
 *  Adapter identifier
 *
 * @param eth_dev_id
 *  Port identifier of Ethernet device
 *
 * @param tx_queue_id
 *  Ethernet device Tx queue index
 *
 * @return
 *  -  0: Success
 *  - <0: Error code on failure
 */
typedef int (*eventdev_eth_tx_adapter_queue_start)
	(uint8_t id, uint16_t eth_dev_id, uint16_t tx_queue_id);

/**
 * Stop a Tx queue that is assigned to Tx adapter instance
 *
 * @param id
 *  Adapter identifier
 *
 * @param eth_dev_id
 *  Port identifier of Ethernet device
 *
 * @param tx_queue_id
 *  Ethernet device Tx queue index
 *
 * @return
 *  -  0: Success
 *  - <0: Error code on failure
 */
typedef int (*eventdev_eth_tx_adapter_queue_stop)
	(uint8_t id, uint16_t eth_dev_id, uint16_t tx_queue_id);

#define eventdev_stop_flush_t rte_eventdev_stop_flush_t

/**
 * Retrieve the event device's DMA adapter capabilities for the
 * specified DMA device
 *
 * @param dev
 *   Event device pointer
 *
 * @param dma_dev_id
 *   DMA device identifier
 *
 * @param[out] caps
 *   A pointer to memory filled with event adapter capabilities.
 *   It is expected to be pre-allocated & initialized by caller.
 *
 * @return
 *   - 0: Success, driver provides event adapter capabilities for the
 *	dmadev.
 *   - <0: Error code returned by the driver function.
 *
 */
typedef int (*eventdev_dma_adapter_caps_get_t)(const struct rte_eventdev *dev,
					       const int16_t dma_dev_id, uint32_t *caps);

/**
 * Add DMA vchan queue to event device. This callback is invoked if
 * the caps returned from rte_event_dma_adapter_caps_get(, dmadev_id)
 * has RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_* set.
 *
 * @param dev
 *   Event device pointer
 *
 * @param dma_dev_id
 *   DMA device identifier
 *
 * @param vchan_id
 *   dmadev vchan queue identifier.
 *
 * @param event
 *   Event information required for binding dmadev vchan to event queue.
 *   This structure will have a valid value for only those HW PMDs supporting
 *   @see RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_VCHAN_EV_BIND capability.
 *
 * @return
 *   - 0: Success, dmadev vchan added successfully.
 *   - <0: Error code returned by the driver function.
 *
 */
typedef int (*eventdev_dma_adapter_vchan_add_t)(const struct rte_eventdev *dev,
						const int16_t dma_dev_id,
						uint16_t vchan_id,
						const struct rte_event *event);

/**
 * Delete DMA vhcan to event device. This callback is invoked if
 * the caps returned from rte_event_dma_adapter_caps_get(, dmadev_id)
 * has RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_* set.
 *
 * @param dev
 *   Event device pointer
 *
 * @param dma_dev_id
 *   DMA device identifier
 *
 * @param vchan_id
 *   dmadev vchan identifier.
 *
 * @return
 *   - 0: Success, dmadev vchan deleted successfully.
 *   - <0: Error code returned by the driver function.
 *
 */
typedef int (*eventdev_dma_adapter_vchan_del_t)(const struct rte_eventdev *dev,
						const int16_t dma_dev_id,
						uint16_t vchan_id);

/**
 * Start DMA adapter. This callback is invoked if
 * the caps returned from rte_event_dma_adapter_caps_get(.., dmadev_id)
 * has RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_* set and vchan for dmadev_id
 * have been added to the event device.
 *
 * @param dev
 *   Event device pointer
 *
 * @param dma_dev_id
 *   DMA device identifier
 *
 * @return
 *   - 0: Success, DMA adapter started successfully.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_dma_adapter_start_t)(const struct rte_eventdev *dev,
					    const int16_t dma_dev_id);

/**
 * Stop DMA adapter. This callback is invoked if
 * the caps returned from rte_event_dma_adapter_caps_get(.., dmadev_id)
 * has RTE_EVENT_DMA_ADAPTER_CAP_INTERNAL_PORT_* set and vchan for dmadev_id
 * have been added to the event device.
 *
 * @param dev
 *   Event device pointer
 *
 * @param dma_dev_id
 *   DMA device identifier
 *
 * @return
 *   - 0: Success, DMA adapter stopped successfully.
 *   - <0: Error code returned by the driver function.
 */
typedef int (*eventdev_dma_adapter_stop_t)(const struct rte_eventdev *dev,
					   const int16_t dma_dev_id);

struct rte_event_dma_adapter_stats;

/**
 * Retrieve DMA adapter statistics.
 *
 * @param dev
 *   Event device pointer
 *
 * @param dma_dev_id
 *   DMA device identifier
 *
 * @param[out] stats
 *   Pointer to stats structure
 *
 * @return
 *   Return 0 on success.
 */
typedef int (*eventdev_dma_adapter_stats_get)(const struct rte_eventdev *dev,
					      const int16_t dma_dev_id,
					      struct rte_event_dma_adapter_stats *stats);

/**
 * Reset DMA adapter statistics.
 *
 * @param dev
 *   Event device pointer
 *
 * @param dma_dev_id
 *   DMA device identifier
 *
 * @return
 *   Return 0 on success.
 */
typedef int (*eventdev_dma_adapter_stats_reset)(const struct rte_eventdev *dev,
						const int16_t dma_dev_id);


/** Event device operations function pointer table */
struct eventdev_ops {
	eventdev_info_get_t dev_infos_get;	/**< Get device info. */
	eventdev_configure_t dev_configure;	/**< Configure device. */
	eventdev_start_t dev_start;		/**< Start device. */
	eventdev_stop_t dev_stop;		/**< Stop device. */
	eventdev_close_t dev_close;		/**< Close device. */

	eventdev_queue_default_conf_get_t queue_def_conf;
	/**< Get default queue configuration. */
	eventdev_queue_setup_t queue_setup;
	/**< Set up an event queue. */
	eventdev_queue_release_t queue_release;
	/**< Release an event queue. */
	eventdev_queue_attr_set_t queue_attr_set;
	/**< Set an event queue attribute. */

	eventdev_port_default_conf_get_t port_def_conf;
	/**< Get default port configuration. */
	eventdev_port_setup_t port_setup;
	/**< Set up an event port. */
	eventdev_port_release_t port_release;
	/**< Release an event port. */
	eventdev_port_quiesce_t port_quiesce;
	/**< Quiesce an event port. */

	eventdev_port_link_t port_link;
	/**< Link event queues to an event port. */
	eventdev_port_link_profile_t port_link_profile;
	/**< Link event queues associated with a profile to an event port. */
	eventdev_port_unlink_t port_unlink;
	/**< Unlink event queues from an event port. */
	eventdev_port_unlink_profile_t port_unlink_profile;
	/**< Unlink event queues associated with a profile from an event port. */
	eventdev_port_unlinks_in_progress_t port_unlinks_in_progress;
	/**< Unlinks in progress on an event port. */
	eventdev_dequeue_timeout_ticks_t timeout_ticks;
	/**< Converts ns to *timeout_ticks* value for rte_event_dequeue() */
	eventdev_dump_t dump;
	/* Dump internal information */

	eventdev_xstats_get_t xstats_get;
	/**< Get extended device statistics. */
	eventdev_xstats_get_names_t xstats_get_names;
	/**< Get names of extended stats. */
	eventdev_xstats_get_by_name xstats_get_by_name;
	/**< Get one value by name. */
	eventdev_xstats_reset_t xstats_reset;
	/**< Reset the statistics values in xstats. */

	eventdev_eth_rx_adapter_caps_get_t eth_rx_adapter_caps_get;
	/**< Get ethernet Rx adapter capabilities */
	eventdev_eth_rx_adapter_queue_add_t eth_rx_adapter_queue_add;
	/**< Add Rx queues to ethernet Rx adapter */
	eventdev_eth_rx_adapter_queue_del_t eth_rx_adapter_queue_del;
	/**< Delete Rx queues from ethernet Rx adapter */
	eventdev_eth_rx_adapter_queue_conf_get_t eth_rx_adapter_queue_conf_get;
	/**< Get Rx adapter queue info */
	eventdev_eth_rx_adapter_start_t eth_rx_adapter_start;
	/**< Start ethernet Rx adapter */
	eventdev_eth_rx_adapter_stop_t eth_rx_adapter_stop;
	/**< Stop ethernet Rx adapter */
	eventdev_eth_rx_adapter_stats_get eth_rx_adapter_stats_get;
	/**< Get ethernet Rx stats */
	eventdev_eth_rx_adapter_stats_reset eth_rx_adapter_stats_reset;
	/**< Reset ethernet Rx stats */
	eventdev_eth_rx_adapter_vector_limits_get_t
		eth_rx_adapter_vector_limits_get;
	/**< Get event vector limits for the Rx adapter */
	eventdev_eth_rx_adapter_instance_get_t eth_rx_adapter_instance_get;
	/**< Get Rx adapter instance ID for Rx queue */

	eventdev_timer_adapter_caps_get_t timer_adapter_caps_get;
	/**< Get timer adapter capabilities */

	eventdev_crypto_adapter_caps_get_t crypto_adapter_caps_get;
	/**< Get crypto adapter capabilities */
	eventdev_crypto_adapter_queue_pair_add_t crypto_adapter_queue_pair_add;
	/**< Add queue pair to crypto adapter */
	eventdev_crypto_adapter_queue_pair_del_t crypto_adapter_queue_pair_del;
	/**< Delete queue pair from crypto adapter */
	eventdev_crypto_adapter_start_t crypto_adapter_start;
	/**< Start crypto adapter */
	eventdev_crypto_adapter_stop_t crypto_adapter_stop;
	/**< Stop crypto adapter */
	eventdev_crypto_adapter_stats_get crypto_adapter_stats_get;
	/**< Get crypto stats */
	eventdev_crypto_adapter_stats_reset crypto_adapter_stats_reset;
	/**< Reset crypto stats */
	eventdev_crypto_adapter_vector_limits_get_t
		crypto_adapter_vector_limits_get;
	/**< Get event vector limits for the crypto adapter */

	eventdev_eth_rx_adapter_q_stats_get eth_rx_adapter_queue_stats_get;
	/**< Get ethernet Rx queue stats */
	eventdev_eth_rx_adapter_q_stats_reset eth_rx_adapter_queue_stats_reset;
	/**< Reset ethernet Rx queue stats */

	eventdev_eth_tx_adapter_caps_get_t eth_tx_adapter_caps_get;
	/**< Get ethernet Tx adapter capabilities */

	eventdev_eth_tx_adapter_create_t eth_tx_adapter_create;
	/**< Create adapter callback */
	eventdev_eth_tx_adapter_free_t eth_tx_adapter_free;
	/**< Free adapter callback */
	eventdev_eth_tx_adapter_queue_add_t eth_tx_adapter_queue_add;
	/**< Add Tx queues to the eth Tx adapter */
	eventdev_eth_tx_adapter_queue_del_t eth_tx_adapter_queue_del;
	/**< Delete Tx queues from the eth Tx adapter */
	eventdev_eth_tx_adapter_start_t eth_tx_adapter_start;
	/**< Start eth Tx adapter */
	eventdev_eth_tx_adapter_stop_t eth_tx_adapter_stop;
	/**< Stop eth Tx adapter */
	eventdev_eth_tx_adapter_stats_get_t eth_tx_adapter_stats_get;
	/**< Get eth Tx adapter statistics */
	eventdev_eth_tx_adapter_stats_reset_t eth_tx_adapter_stats_reset;
	/**< Reset eth Tx adapter statistics */
	eventdev_eth_tx_adapter_instance_get_t eth_tx_adapter_instance_get;
	/**< Get Tx adapter instance ID for Tx queue */
	eventdev_eth_tx_adapter_queue_start eth_tx_adapter_queue_start;
	/**< Start Tx queue assigned to Tx adapter instance */
	eventdev_eth_tx_adapter_queue_stop eth_tx_adapter_queue_stop;
	/**< Stop Tx queue assigned to Tx adapter instance */

	eventdev_dma_adapter_caps_get_t dma_adapter_caps_get;
	/**< Get DMA adapter capabilities */
	eventdev_dma_adapter_vchan_add_t dma_adapter_vchan_add;
	/**< Add vchan queue to DMA adapter */
	eventdev_dma_adapter_vchan_del_t dma_adapter_vchan_del;
	/**< Delete vchan queue from DMA adapter */
	eventdev_dma_adapter_start_t dma_adapter_start;
	/**< Start DMA adapter */
	eventdev_dma_adapter_stop_t dma_adapter_stop;
	/**< Stop DMA adapter */
	eventdev_dma_adapter_stats_get dma_adapter_stats_get;
	/**< Get DMA stats */
	eventdev_dma_adapter_stats_reset dma_adapter_stats_reset;
	/**< Reset DMA stats */

	eventdev_selftest dev_selftest;
	/**< Start eventdev Selftest */

	eventdev_stop_flush_t dev_stop_flush;
	/**< User-provided event flush function */
};

/**
 * Allocates a new eventdev slot for an event device and returns the pointer
 * to that slot for the driver to use.
 *
 * @param name
 *   Unique identifier name for each device
 * @param socket_id
 *   Socket to allocate resources on.
 * @return
 *   - Slot in the rte_dev_devices array for a new device;
 */
__rte_internal
struct rte_eventdev *
rte_event_pmd_allocate(const char *name, int socket_id);

/**
 * Release the specified eventdev device.
 *
 * @param eventdev
 * The *eventdev* pointer is the address of the *rte_eventdev* structure.
 * @return
 *   - 0 on success, negative on error
 */
__rte_internal
int
rte_event_pmd_release(struct rte_eventdev *eventdev);

/**
 *
 * @internal
 * This is the last step of device probing.
 * It must be called after a port is allocated and initialized successfully.
 *
 * @param eventdev
 *  New event device.
 */
__rte_internal
void
event_dev_probing_finish(struct rte_eventdev *eventdev);

/**
 * Reset eventdevice fastpath APIs to dummy values.
 *
 * @param fp_ops
 * The *fp_ops* pointer to reset.
 */
__rte_internal
void
event_dev_fp_ops_reset(struct rte_event_fp_ops *fp_op);

/**
 * Set eventdevice fastpath APIs to event device values.
 *
 * @param fp_ops
 * The *fp_ops* pointer to set.
 */
__rte_internal
void
event_dev_fp_ops_set(struct rte_event_fp_ops *fp_ops,
		     const struct rte_eventdev *dev);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EVENTDEV_PMD_H_ */
