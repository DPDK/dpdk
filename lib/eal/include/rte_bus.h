/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 NXP
 */

#ifndef _RTE_BUS_H_
#define _RTE_BUS_H_

/**
 * @file
 *
 * DPDK device bus interface
 *
 * This file exposes API and interfaces for bus abstraction
 * over the devices and drivers in EAL.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include <rte_eal.h>

struct rte_bus;
struct rte_device;

/**
 * Retrieve a bus name.
 *
 * @param bus
 *   A pointer to a rte_bus structure.
 * @return
 *   A pointer to the bus name string.
 */
const char *rte_bus_name(const struct rte_bus *bus);

/**
 * Scan all the buses.
 *
 * @return
 *   0 in case of success in scanning all buses
 *  !0 in case of failure to scan
 */
int rte_bus_scan(void);

/**
 * For each device on the buses, perform a driver 'match' and call the
 * driver-specific probe for device initialization.
 *
 * @return
 *	 0 for successful match/probe
 *	!0 otherwise
 */
int rte_bus_probe(void);

/**
 * Dump information of all the buses registered with EAL.
 *
 * @param f
 *	 A valid and open output stream handle
 */
void rte_bus_dump(FILE *f);

/**
 * Bus comparison function.
 *
 * @param bus
 *	Bus under test.
 *
 * @param data
 *	Data to compare against.
 *
 * @return
 *	0 if the bus matches the data.
 *	!0 if the bus does not match.
 *	<0 if ordering is possible and the bus is lower than the data.
 *	>0 if ordering is possible and the bus is greater than the data.
 */
typedef int (*rte_bus_cmp_t)(const struct rte_bus *bus, const void *data);

/**
 * Bus iterator to find a particular bus.
 *
 * This function compares each registered bus to find one that matches
 * the data passed as parameter.
 *
 * If the comparison function returns zero this function will stop iterating
 * over any more buses. To continue a search the bus of a previous search can
 * be passed via the start parameter.
 *
 * @param start
 *	Starting point for the iteration.
 *
 * @param cmp
 *	Comparison function.
 *
 * @param data
 *	 Data to pass to comparison function.
 *
 * @return
 *	 A pointer to a rte_bus structure or NULL in case no bus matches
 */
struct rte_bus *rte_bus_find(const struct rte_bus *start, rte_bus_cmp_t cmp,
			     const void *data);

/**
 * Find the registered bus for a particular device.
 */
struct rte_bus *rte_bus_find_by_device(const struct rte_device *dev);

/**
 * Find the registered bus for a given name.
 */
struct rte_bus *rte_bus_find_by_name(const char *busname);


/**
 * Get the common iommu class of devices bound on to buses available in the
 * system. RTE_IOVA_DC means that no preference has been expressed.
 *
 * @return
 *     enum rte_iova_mode value.
 */
enum rte_iova_mode rte_bus_get_iommu_class(void);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BUS_H */
