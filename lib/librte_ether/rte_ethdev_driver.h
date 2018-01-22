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

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_DRIVER_H_ */
