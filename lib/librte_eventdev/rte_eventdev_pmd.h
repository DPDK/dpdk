/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef _RTE_EVENTDEV_PMD_H_
#define _RTE_EVENTDEV_PMD_H_

/** @file
 * RTE Event PMD APIs
 *
 * @note
 * These API are from event PMD only and user applications should not call
 * them directly.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include <rte_common.h>
#include <rte_compat.h>
#include <rte_config.h>
#include <rte_dev.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>

#include "rte_eventdev.h"
#include "rte_event_timer_adapter_pmd.h"

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

#define RTE_EVENT_ETH_RX_ADAPTER_SW_CAP \
		((RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID) | \
			(RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ))

#define RTE_EVENT_CRYPTO_ADAPTER_SW_CAP \
		RTE_EVENT_CRYPTO_ADAPTER_CAP_SESSION_PRIVATE_DATA

/**< Ethernet Rx adapter cap to return If the packet transfers from
 * the ethdev to eventdev use a SW service function
 */

#define RTE_EVENTDEV_DETACHED  (0)
#define RTE_EVENTDEV_ATTACHED  (1)

struct rte_eth_dev;

/** Global structure used for maintaining state of allocated event devices */
struct rte_eventdev_global {
	uint8_t nb_devs;	/**< Number of devices found */
};

extern struct rte_eventdev *rte_eventdevs;
/** The pool of rte_eventdev structures. */

/**
 * Get the rte_eventdev structure device pointer for the named device.
 *
 * @param name
 *   device name to select the device structure.
 *
 * @return
 *   - The rte_eventdev structure pointer for the given device ID.
 */
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
 *
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
 *
 */
typedef void (*eventdev_queue_release_t)(struct rte_eventdev *dev,
		uint8_t queue_id);

/**
 * Retrieve the default event port configuration.
 *
 * @param dev
 *   Event device pointer
 * @param port_id
 *   Event port index
 * @param[out] port_conf
 *   Event port configuration structure
 *
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
 *
 */
typedef void (*eventdev_port_release_t)(void *port);

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
 *
 */
typedef int (*eventdev_port_link_t)(struct rte_eventdev *dev, void *port,
		const uint8_t queues[], const uint8_t priorities[],
		uint16_t nb_links);

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
 *
 */
typedef int (*eventdev_port_unlink_t)(struct rte_eventdev *dev, void *port,
		uint8_t queues[], uint16_t nb_unlinks);

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
 *
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
 *
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
		const unsigned int ids[], uint64_t values[], unsigned int n);

/**
 * Resets the statistic values in xstats for the device, based on mode.
 */
typedef int (*eventdev_xstats_reset_t)(struct rte_eventdev *dev,
		enum rte_event_dev_xstats_mode mode,
		int16_t queue_port_id,
		const uint32_t ids[],
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
		unsigned int *ids, unsigned int size);

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
		const char *name, unsigned int *id);


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
 *
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
 *
 */
typedef int (*eventdev_timer_adapter_caps_get_t)(
				const struct rte_eventdev *dev,
				uint64_t flags,
				uint32_t *caps,
				const struct rte_event_timer_adapter_ops **ops);

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
 *
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
 *
 */
typedef int (*eventdev_eth_rx_adapter_queue_del_t)
					(const struct rte_eventdev *dev,
					const struct rte_eth_dev *eth_dev,
					int32_t rx_queue_id);

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
/**
 * Start eventdev selftest.
 *
 * @return
 *   Return 0 on success.
 */
typedef int (*eventdev_selftest)(void);

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
 *
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
 *
 */
typedef int (*eventdev_crypto_adapter_queue_pair_add_t)
			(const struct rte_eventdev *dev,
			 const struct rte_cryptodev *cdev,
			 int32_t queue_pair_id,
			 const struct rte_event *event);


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
 *
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
 *
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

/** Event device operations function pointer table */
struct rte_eventdev_ops {
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

	eventdev_port_default_conf_get_t port_def_conf;
	/**< Get default port configuration. */
	eventdev_port_setup_t port_setup;
	/**< Set up an event port. */
	eventdev_port_release_t port_release;
	/**< Release an event port. */

	eventdev_port_link_t port_link;
	/**< Link event queues to an event port. */
	eventdev_port_unlink_t port_unlink;
	/**< Unlink event queues from an event port. */
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
	eventdev_eth_rx_adapter_start_t eth_rx_adapter_start;
	/**< Start ethernet Rx adapter */
	eventdev_eth_rx_adapter_stop_t eth_rx_adapter_stop;
	/**< Stop ethernet Rx adapter */
	eventdev_eth_rx_adapter_stats_get eth_rx_adapter_stats_get;
	/**< Get ethernet Rx stats */
	eventdev_eth_rx_adapter_stats_reset eth_rx_adapter_stats_reset;
	/**< Reset ethernet Rx stats */

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
int
rte_event_pmd_release(struct rte_eventdev *eventdev);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EVENTDEV_PMD_H_ */
