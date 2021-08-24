/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _RTE_BUS_IFPGA_H_
#define _RTE_BUS_IFPGA_H_

/**
 * @file
 *
 * RTE Intel FPGA Bus Interface
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <rte_bus.h>
#include <rte_pci.h>
#include <rte_interrupts.h>
#include <rte_spinlock.h>

/** Name of Intel FPGA Bus */
#define IFPGA_BUS_NAME ifpga

/* Forward declarations */
struct rte_afu_device;
struct rte_afu_driver;

/** Double linked list of Intel FPGA AFU device. */
RTE_TAILQ_HEAD(ifpga_afu_dev_list, rte_afu_device);
/** Double linked list of Intel FPGA AFU device drivers. */
RTE_TAILQ_HEAD(ifpga_afu_drv_list, rte_afu_driver);

#define IFPGA_BUS_BITSTREAM_PATH_MAX_LEN 256

struct rte_afu_uuid {
	uint64_t uuid_low;
	uint64_t uuid_high;
} __rte_packed;

#define IFPGA_BUS_DEV_PORT_MAX 4

/**
 * A structure describing an ID for a AFU driver. Each driver provides a
 * table of these IDs for each device that it supports.
 */
struct rte_afu_id {
	struct rte_afu_uuid uuid;
	int      port; /**< port number */
} __rte_packed;

/**
 * A structure PR (Partial Reconfiguration) configuration AFU driver.
 */

struct rte_afu_pr_conf {
	struct rte_afu_id afu_id;
	int pr_enable;
	char bs_path[IFPGA_BUS_BITSTREAM_PATH_MAX_LEN];
};

#define AFU_PRI_STR_SIZE (PCI_PRI_STR_SIZE + 8)

struct rte_afu_shared {
	rte_spinlock_t lock;
	void *data;
};

/**
 * A structure describing a AFU device.
 */
struct rte_afu_device {
	RTE_TAILQ_ENTRY(rte_afu_device) next;       /**< Next in device list. */
	struct rte_device device;               /**< Inherit core device */
	struct rte_rawdev *rawdev;    /**< Point Rawdev */
	struct rte_afu_id id;                   /**< AFU id within FPGA. */
	uint32_t num_region;   /**< number of regions found */
	struct rte_mem_resource mem_resource[PCI_MAX_RESOURCE];
						/**< AFU Memory Resource */
	struct rte_afu_shared shared;
	struct rte_intr_handle intr_handle;     /**< Interrupt handle */
	struct rte_afu_driver *driver;          /**< Associated driver */
	char path[IFPGA_BUS_BITSTREAM_PATH_MAX_LEN];
} __rte_packed;

/**
 * @internal
 * Helper macro for drivers that need to convert to struct rte_afu_device.
 */
#define RTE_DEV_TO_AFU(ptr) \
	container_of(ptr, struct rte_afu_device, device)

/**
 * Initialization function for the driver called during FPGA BUS probing.
 */
typedef int (afu_probe_t)(struct rte_afu_device *);

/**
 * Uninitialization function for the driver called during hotplugging.
 */
typedef int (afu_remove_t)(struct rte_afu_device *);

/**
 * A structure describing a AFU device.
 */
struct rte_afu_driver {
	RTE_TAILQ_ENTRY(rte_afu_driver) next;   /**< Next afu driver. */
	struct rte_driver driver;               /**< Inherit core driver. */
	afu_probe_t *probe;                     /**< Device Probe function. */
	afu_remove_t *remove;                   /**< Device Remove function. */
	const struct rte_afu_uuid *id_table;    /**< AFU uuid within FPGA. */
};

static inline const char *
rte_ifpga_device_name(const struct rte_afu_device *afu)
{
	if (afu && afu->device.name)
		return afu->device.name;
	return NULL;
}

/**
 * Find AFU by AFU name.
 *
 * @param name
 *   A pointer to AFU name string.
 */
struct rte_afu_device *
rte_ifpga_find_afu_by_name(const char *name);

/**
 * Register a ifpga afu device driver.
 *
 * @param driver
 *   A pointer to a rte_afu_driver structure describing the driver
 *   to be registered.
 */
void rte_ifpga_driver_register(struct rte_afu_driver *driver);

/**
 * Unregister a ifpga afu device driver.
 *
 * @param driver
 *   A pointer to a rte_afu_driver structure describing the driver
 *   to be unregistered.
 */
void rte_ifpga_driver_unregister(struct rte_afu_driver *driver);

#define RTE_PMD_REGISTER_AFU(nm, afudrv)\
static const char *afudrvinit_ ## nm ## _alias;\
RTE_INIT(afudrvinitfn_ ##afudrv)\
{\
	(afudrv).driver.name = RTE_STR(nm);\
	(afudrv).driver.alias = afudrvinit_ ## nm ## _alias;\
	rte_ifpga_driver_register(&afudrv);\
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

#define RTE_PMD_REGISTER_AFU_ALIAS(nm, alias)\
static const char *afudrvinit_ ## nm ## _alias = RTE_STR(alias)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _RTE_BUS_IFPGA_H_ */
