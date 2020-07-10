/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_ETHDEV_DRIVER_H_
#define _RTE_ETHDEV_DRIVER_H_

/**
 * @file
 *
 * RTE Ethernet Device PMD API
 *
 * These APIs for the use from Ethernet drivers, user applications shouldn't
 * use them.
 *
 */

#include <rte_ethdev.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * RX/TX queue states
 */
#define RTE_ETH_QUEUE_STATE_STOPPED 0
#define RTE_ETH_QUEUE_STATE_STARTED 1
#define RTE_ETH_QUEUE_STATE_HAIRPIN 2

/**
 * @internal
 * Check if the selected Rx queue is hairpin queue.
 *
 * @param dev
 *  Pointer to the selected device.
 * @param queue_id
 *  The selected queue.
 *
 * @return
 *   - (1) if the queue is hairpin queue, 0 otherwise.
 */
int rte_eth_dev_is_rx_hairpin_queue(struct rte_eth_dev *dev, uint16_t queue_id);

/**
 * @internal
 * Check if the selected Tx queue is hairpin queue.
 *
 * @param dev
 *  Pointer to the selected device.
 * @param queue_id
 *  The selected queue.
 *
 * @return
 *   - (1) if the queue is hairpin queue, 0 otherwise.
 */
int rte_eth_dev_is_tx_hairpin_queue(struct rte_eth_dev *dev, uint16_t queue_id);

/**
 * @internal
 * Returns a ethdev slot specified by the unique identifier name.
 *
 * @param	name
 *  The pointer to the Unique identifier name for each Ethernet device
 * @return
 *   - The pointer to the ethdev slot, on success. NULL on error
 */
struct rte_eth_dev *rte_eth_dev_allocated(const char *name);

/**
 * @internal
 * Allocates a new ethdev slot for an ethernet device and returns the pointer
 * to that slot for the driver to use.
 *
 * @param	name	Unique identifier name for each Ethernet device
 * @return
 *   - Slot in the rte_dev_devices array for a new device;
 */
struct rte_eth_dev *rte_eth_dev_allocate(const char *name);

/**
 * @internal
 * Attach to the ethdev already initialized by the primary
 * process.
 *
 * @param       name    Ethernet device's name.
 * @return
 *   - Success: Slot in the rte_dev_devices array for attached
 *        device.
 *   - Error: Null pointer.
 */
struct rte_eth_dev *rte_eth_dev_attach_secondary(const char *name);

/**
 * @internal
 * Notify RTE_ETH_EVENT_DESTROY and release the specified ethdev port.
 *
 * The following PMD-managed data fields will be freed:
 *   - dev_private
 *   - mac_addrs
 *   - hash_mac_addrs
 * If one of these fields should not be freed,
 * it must be reset to NULL by the PMD, typically in dev_close method.
 *
 * @param eth_dev
 * Device to be detached.
 * @return
 *   - 0 on success, negative on error
 */
int rte_eth_dev_release_port(struct rte_eth_dev *eth_dev);

/**
 * @internal
 * Release device queues and clear its configuration to force the user
 * application to reconfigure it. It is for internal use only.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  void
 */
void _rte_eth_dev_reset(struct rte_eth_dev *dev);

/**
 * @internal Executes all the user application registered callbacks for
 * the specific device. It is for DPDK internal user only. User
 * application should not call it directly.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param event
 *  Eth device interrupt event type.
 * @param ret_param
 *  To pass data back to user application.
 *  This allows the user application to decide if a particular function
 *  is permitted or not.
 *
 * @return
 *  int
 */
int _rte_eth_dev_callback_process(struct rte_eth_dev *dev,
		enum rte_eth_event_type event, void *ret_param);

/**
 * @internal
 * This is the last step of device probing.
 * It must be called after a port is allocated and initialized successfully.
 *
 * The notification RTE_ETH_EVENT_NEW is sent to other entities
 * (libraries and applications).
 * The state is set as RTE_ETH_DEV_ATTACHED.
 *
 * @param dev
 *  New ethdev port.
 */
void rte_eth_dev_probing_finish(struct rte_eth_dev *dev);

/**
 * Create memzone for HW rings.
 * malloc can't be used as the physical address is needed.
 * If the memzone is already created, then this function returns a ptr
 * to the old one.
 *
 * @param eth_dev
 *   The *eth_dev* pointer is the address of the *rte_eth_dev* structure
 * @param name
 *   The name of the memory zone
 * @param queue_id
 *   The index of the queue to add to name
 * @param size
 *   The sizeof of the memory area
 * @param align
 *   Alignment for resulting memzone. Must be a power of 2.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in case of NUMA.
 */
const struct rte_memzone *
rte_eth_dma_zone_reserve(const struct rte_eth_dev *eth_dev, const char *name,
			 uint16_t queue_id, size_t size,
			 unsigned align, int socket_id);

/**
 * Free previously allocated memzone for HW rings.
 *
 * @param eth_dev
 *   The *eth_dev* pointer is the address of the *rte_eth_dev* structure
 * @param name
 *   The name of the memory zone
 * @param queue_id
 *   The index of the queue to add to name
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_internal
int
rte_eth_dma_zone_free(const struct rte_eth_dev *eth_dev, const char *name,
		 uint16_t queue_id);

/**
 * @internal
 * Atomically set the link status for the specific device.
 * It is for use by DPDK device driver use only.
 * User applications should not call it
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param link
 *  New link status value.
 * @return
 *  Same convention as eth_link_update operation.
 *  0   if link up status has changed
 *  -1  if link up status was unchanged
 */
static inline int
rte_eth_linkstatus_set(struct rte_eth_dev *dev,
		       const struct rte_eth_link *new_link)
{
	volatile uint64_t *dev_link
		 = (volatile uint64_t *)&(dev->data->dev_link);
	union {
		uint64_t val64;
		struct rte_eth_link link;
	} orig;

	RTE_BUILD_BUG_ON(sizeof(*new_link) != sizeof(uint64_t));

	orig.val64 = rte_atomic64_exchange(dev_link,
					   *(const uint64_t *)new_link);

	return (orig.link.link_status == new_link->link_status) ? -1 : 0;
}

/**
 * @internal
 * Atomically get the link speed and status.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param link
 *  link status value.
 */
static inline void
rte_eth_linkstatus_get(const struct rte_eth_dev *dev,
		       struct rte_eth_link *link)
{
	volatile uint64_t *src = (uint64_t *)&(dev->data->dev_link);
	uint64_t *dst = (uint64_t *)link;

	RTE_BUILD_BUG_ON(sizeof(*link) != sizeof(uint64_t));

#ifdef __LP64__
	/* if cpu arch has 64 bit unsigned lon then implicitly atomic */
	*dst = *src;
#else
	/* can't use rte_atomic64_read because it returns signed int */
	do {
		*dst = *src;
	} while (!rte_atomic64_cmpset(src, *dst, *dst));
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Allocate an unique switch domain identifier.
 *
 * A pool of switch domain identifiers which can be allocated on request. This
 * will enabled devices which support the concept of switch domains to request
 * a switch domain id which is guaranteed to be unique from other devices
 * running in the same process.
 *
 * @param domain_id
 *  switch domain identifier parameter to pass back to application
 *
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_experimental
int
rte_eth_switch_domain_alloc(uint16_t *domain_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Free switch domain.
 *
 * Return a switch domain identifier to the pool of free identifiers after it is
 * no longer in use by device.
 *
 * @param domain_id
 *  switch domain identifier to free
 *
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_experimental
int
rte_eth_switch_domain_free(uint16_t domain_id);

/** Generic Ethernet device arguments  */
struct rte_eth_devargs {
	uint16_t ports[RTE_MAX_ETHPORTS];
	/** port/s number to enable on a multi-port single function */
	uint16_t nb_ports;
	/** number of ports in ports field */
	uint16_t representor_ports[RTE_MAX_ETHPORTS];
	/** representor port/s identifier to enable on device */
	uint16_t nb_representor_ports;
	/** number of ports in representor port field */
};

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * PMD helper function to parse ethdev arguments
 *
 * @param devargs
 *  device arguments
 * @param eth_devargs
 *  parsed ethdev specific arguments.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_experimental
int
rte_eth_devargs_parse(const char *devargs, struct rte_eth_devargs *eth_devargs);


typedef int (*ethdev_init_t)(struct rte_eth_dev *ethdev, void *init_params);
typedef int (*ethdev_bus_specific_init)(struct rte_eth_dev *ethdev,
	void *bus_specific_init_params);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * PMD helper function for the creation of a new ethdev ports.
 *
 * @param device
 *  rte_device handle.
 * @param name
 *  port name.
 * @param priv_data_size
 *  size of private data required for port.
 * @param bus_specific_init
 *  port bus specific initialisation callback function
 * @param bus_init_params
 *  port bus specific initialisation parameters
 * @param ethdev_init
 *  device specific port initialization callback function
 * @param init_params
 *  port initialisation parameters
 *
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_experimental
int
rte_eth_dev_create(struct rte_device *device, const char *name,
	size_t priv_data_size,
	ethdev_bus_specific_init bus_specific_init, void *bus_init_params,
	ethdev_init_t ethdev_init, void *init_params);


typedef int (*ethdev_uninit_t)(struct rte_eth_dev *ethdev);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * PMD helper function for cleaning up the resources of a ethdev port on it's
 * destruction.
 *
 * @param ethdev
 *   ethdev handle of port.
 * @param ethdev_uninit
 *   device specific port un-initialise callback function
 *
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_experimental
int
rte_eth_dev_destroy(struct rte_eth_dev *ethdev, ethdev_uninit_t ethdev_uninit);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_DRIVER_H_ */
