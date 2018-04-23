/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2014 6WIND S.A.
 */

#ifndef _RTE_DEVARGS_H_
#define _RTE_DEVARGS_H_

/**
 * @file
 *
 * RTE devargs: list of devices and their user arguments
 *
 * This file stores a list of devices and their arguments given by
 * the user when a DPDK application is started. These devices can be PCI
 * devices or virtual devices. These devices are stored at startup in a
 * list of rte_devargs structures.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <sys/queue.h>
#include <rte_compat.h>
#include <rte_bus.h>

/**
 * Type of generic device
 */
enum rte_devtype {
	RTE_DEVTYPE_WHITELISTED_PCI,
	RTE_DEVTYPE_BLACKLISTED_PCI,
	RTE_DEVTYPE_VIRTUAL,
};

/**
 * Structure that stores a device given by the user with its arguments
 *
 * A user device is a physical or a virtual device given by the user to
 * the DPDK application at startup through command line arguments.
 *
 * The structure stores the configuration of the device, its PCI
 * identifier if it's a PCI device or the driver name if it's a virtual
 * device.
 */
struct rte_devargs {
	/** Next in list. */
	TAILQ_ENTRY(rte_devargs) next;
	/** Type of device. */
	enum rte_devtype type;
	/** Device policy. */
	enum rte_dev_policy policy;
	/** Bus handle for the device. */
	struct rte_bus *bus;
	/** Name of the device. */
	char name[RTE_DEV_NAME_MAX_LEN];
	/** Arguments string as given by user or "" for no argument. */
	char *args;
};

/**
 * @deprecated
 * Parse a devargs string.
 *
 * For PCI devices, the format of arguments string is "PCI_ADDR" or
 * "PCI_ADDR,key=val,key2=val2,...". Examples: "08:00.1", "0000:5:00.0",
 * "04:00.0,arg=val".
 *
 * For virtual devices, the format of arguments string is "DRIVER_NAME*"
 * or "DRIVER_NAME*,key=val,key2=val2,...". Examples: "net_ring",
 * "net_ring0", "net_pmdAnything,arg=0:arg2=1".
 *
 * The function parses the arguments string to get driver name and driver
 * arguments.
 *
 * @param devargs_str
 *   The arguments as given by the user.
 * @param drvname
 *   The pointer to the string to store parsed driver name.
 * @param drvargs
 *   The pointer to the string to store parsed driver arguments.
 *
 * @return
 *   - 0 on success
 *   - A negative value on error
 */
__rte_deprecated
int rte_eal_parse_devargs_str(const char *devargs_str,
				char **drvname, char **drvargs);

/**
 * Parse a device string.
 *
 * Verify that a bus is capable of handling the device passed
 * in argument. Store which bus will handle the device, its name
 * and the eventual device parameters.
 *
 * The device string is built with a printf-like syntax.
 *
 * The syntax is:
 *
 *     bus:device_identifier,arg1=val1,arg2=val2
 *
 * where "bus:" is the bus name followed by any character separator.
 * The bus name is optional. If no bus name is specified, each bus
 * will attempt to recognize the device identifier. The first one
 * to succeed will be used.
 *
 * Examples:
 *
 *     pci:0000:05.00.0,arg=val
 *     05.00.0,arg=val
 *     vdev:net_ring0
 *
 * @param da
 *   The devargs structure holding the device information.
 * @param format
 *   Format string describing a device.
 *
 * @return
 *   - 0 on success.
 *   - Negative errno on error.
 */
__rte_experimental
int
rte_devargs_parse(struct rte_devargs *da,
		  const char *format, ...)
__attribute__((format(printf, 2, 0)));

/**
 * Insert an rte_devargs in the global list.
 *
 * @param da
 *  The devargs structure to insert.
 *
 * @return
 *   - 0 on success
 *   - Negative on error.
 */
__rte_experimental
int
rte_devargs_insert(struct rte_devargs *da);

/**
 * Add a device to the user device list
 * See rte_devargs_parse() for details.
 *
 * @param devtype
 *   The type of the device.
 * @param devargs_str
 *   The arguments as given by the user.
 *
 * @return
 *   - 0 on success
 *   - A negative value on error
 */
__rte_experimental
int rte_devargs_add(enum rte_devtype devtype, const char *devargs_str);

/**
 * @deprecated
 * Add a device to the user device list
 * See rte_devargs_parse() for details.
 *
 * @param devtype
 *   The type of the device.
 * @param devargs_str
 *   The arguments as given by the user.
 *
 * @return
 *   - 0 on success
 *   - A negative value on error
 */
__rte_deprecated
int rte_eal_devargs_add(enum rte_devtype devtype, const char *devargs_str);

/**
 * Remove a device from the user device list.
 * Its resources are freed.
 * If the devargs cannot be found, nothing happens.
 *
 * @param busname
 *   bus name of the devargs to remove.
 *
 * @param devname
 *   device name of the devargs to remove.
 *
 * @return
 *   0 on success.
 *   <0 on error.
 *   >0 if the devargs was not within the user device list.
 */
__rte_experimental
int rte_devargs_remove(const char *busname,
		       const char *devname);

/**
 * Count the number of user devices of a specified type
 *
 * @param devtype
 *   The type of the devices to counted.
 *
 * @return
 *   The number of devices.
 */
__rte_experimental
unsigned int
rte_devargs_type_count(enum rte_devtype devtype);

/**
 * @deprecated
 * Count the number of user devices of a specified type
 *
 * @param devtype
 *   The type of the devices to counted.
 *
 * @return
 *   The number of devices.
 */
__rte_deprecated
unsigned int
rte_eal_devargs_type_count(enum rte_devtype devtype);

/**
 * This function dumps the list of user device and their arguments.
 *
 * @param f
 *   A pointer to a file for output
 */
__rte_experimental
void rte_devargs_dump(FILE *f);

/**
 * @deprecated
 * This function dumps the list of user device and their arguments.
 *
 * @param f
 *   A pointer to a file for output
 */
__rte_deprecated
void rte_eal_devargs_dump(FILE *f);

/**
 * Find next rte_devargs matching the provided bus name.
 *
 * @param busname
 *   Limit the iteration to devargs related to buses
 *   matching this name.
 *   Will return any next rte_devargs if NULL.
 *
 * @param start
 *   Starting iteration point. The iteration will start at
 *   the first rte_devargs if NULL.
 *
 * @return
 *   Next rte_devargs entry matching the requested bus,
 *   NULL if there is none.
 */
__rte_experimental
struct rte_devargs *
rte_devargs_next(const char *busname, const struct rte_devargs *start);

/**
 * Iterate over all rte_devargs for a specific bus.
 */
#define RTE_EAL_DEVARGS_FOREACH(busname, da) \
	for (da = rte_devargs_next(busname, NULL); \
	     da != NULL; \
	     da = rte_devargs_next(busname, da)) \

#ifdef __cplusplus
}
#endif

#endif /* _RTE_DEVARGS_H_ */
