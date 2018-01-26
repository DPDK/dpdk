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
 * @param	type	Device type of this Ethernet device
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
 * Release the specified ethdev port.
 *
 * @param eth_dev
 * The *eth_dev* pointer is the address of the *rte_eth_dev* structure.
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

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_DRIVER_H_ */
