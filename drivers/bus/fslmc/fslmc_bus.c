/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2016,2018-2021 NXP
 *
 */

#include <string.h>
#include <dirent.h>
#include <stdalign.h>
#include <stdbool.h>

#include <eal_export.h>
#include <rte_log.h>
#include <bus_driver.h>
#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>
#include <ethdev_driver.h>
#include <rte_mbuf_dyn.h>
#include <rte_vfio.h>

#include "private.h"
#include <fslmc_vfio.h>
#include "fslmc_logs.h"

#include <dpaax_iova_table.h>

#define VFIO_IOMMU_GROUP_PATH "/sys/kernel/iommu_groups"

struct rte_fslmc_bus rte_fslmc_bus;
static int fslmc_bus_device_count[DPAA2_DEVTYPE_MAX];

#define DPAA2_SEQN_DYNFIELD_NAME "dpaa2_seqn_dynfield"
RTE_EXPORT_INTERNAL_SYMBOL(dpaa2_seqn_dynfield_offset)
int dpaa2_seqn_dynfield_offset = -1;

RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_get_device_count)
uint32_t
rte_fslmc_get_device_count(enum rte_dpaa2_dev_type device_type)
{
	if (device_type >= DPAA2_DEVTYPE_MAX)
		return 0;
	return fslmc_bus_device_count[device_type];
}

static void
cleanup_fslmc_device_list(void)
{
	struct rte_dpaa2_device *dev;

	RTE_BUS_FOREACH_DEV(dev, &rte_fslmc_bus.bus) {
		rte_bus_remove_device(&rte_fslmc_bus.bus, &dev->device);
		rte_intr_instance_free(dev->intr_handle);
		free(dev);
		dev = NULL;
	}
}

static int
compare_dpaa2_devname(struct rte_dpaa2_device *dev1,
		      struct rte_dpaa2_device *dev2)
{
	int comp;

	if (dev1->dev_type > dev2->dev_type) {
		comp = 1;
	} else if (dev1->dev_type < dev2->dev_type) {
		comp = -1;
	} else {
		/* Check the ID as types match */
		if (dev1->object_id > dev2->object_id)
			comp = 1;
		else if (dev1->object_id < dev2->object_id)
			comp = -1;
		else
			comp = 0; /* Duplicate device name */
	}

	return comp;
}

static void
insert_in_device_list(struct rte_dpaa2_device *newdev)
{
	int comp, inserted = 0;
	struct rte_dpaa2_device *dev = NULL;

	RTE_BUS_FOREACH_DEV(dev, &rte_fslmc_bus.bus) {
		comp = compare_dpaa2_devname(newdev, dev);
		if (comp < 0) {
			rte_bus_insert_device(&rte_fslmc_bus.bus, &dev->device, &newdev->device);
			inserted = 1;
			break;
		}
	}

	if (!inserted)
		rte_bus_add_device(&rte_fslmc_bus.bus, &newdev->device);
}

static void
dump_device_list(void)
{
	struct rte_dpaa2_device *dev;

	/* Only if the log level has been set to Debugging, print list */
	if (rte_log_can_log(dpaa2_logtype_bus, RTE_LOG_DEBUG)) {
		DPAA2_BUS_LOG(DEBUG, "List of devices scanned on bus:");
		RTE_BUS_FOREACH_DEV(dev, &rte_fslmc_bus.bus) {
			DPAA2_BUS_LOG(DEBUG, "\t\t%s", dev->device.name);
		}
	}
}

static int
scan_one_fslmc_device(char *dev_name)
{
	char *dup_dev_name, *t_ptr;
	struct rte_dpaa2_device *dev = NULL;
	int ret = -1;

	if (!dev_name)
		return ret;

	/* Creating a temporary copy to perform cut-parse over string */
	dup_dev_name = strdup(dev_name);
	if (!dup_dev_name) {
		DPAA2_BUS_ERR("Unable to allocate device name memory");
		return -ENOMEM;
	}

	/* For all other devices, we allocate rte_dpaa2_device.
	 * For those devices where there is no driver, probe would release
	 * the memory associated with the rte_dpaa2_device after necessary
	 * initialization.
	 */
	dev = calloc(1, sizeof(struct rte_dpaa2_device));
	if (!dev) {
		DPAA2_BUS_ERR("Unable to allocate device object");
		free(dup_dev_name);
		return -ENOMEM;
	}

	dev->device.numa_node = SOCKET_ID_ANY;

	/* Allocate interrupt instance */
	dev->intr_handle =
		rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (dev->intr_handle == NULL) {
		DPAA2_BUS_ERR("Failed to allocate intr handle");
		ret = -ENOMEM;
		goto cleanup;
	}

	/* Parse the device name and ID */
	t_ptr = strtok(dup_dev_name, ".");
	if (!t_ptr) {
		DPAA2_BUS_ERR("Invalid device found: (%s)", dup_dev_name);
		ret = -EINVAL;
		goto cleanup;
	}
	if (!strncmp("dpni", t_ptr, 4))
		dev->dev_type = DPAA2_ETH;
	else if (!strncmp("dpseci", t_ptr, 6))
		dev->dev_type = DPAA2_CRYPTO;
	else if (!strncmp("dpcon", t_ptr, 5))
		dev->dev_type = DPAA2_CON;
	else if (!strncmp("dpbp", t_ptr, 4))
		dev->dev_type = DPAA2_BPOOL;
	else if (!strncmp("dpio", t_ptr, 4))
		dev->dev_type = DPAA2_IO;
	else if (!strncmp("dpci", t_ptr, 4))
		dev->dev_type = DPAA2_CI;
	else if (!strncmp("dpmcp", t_ptr, 5))
		dev->dev_type = DPAA2_MPORTAL;
	else if (!strncmp("dpdmai", t_ptr, 6))
		dev->dev_type = DPAA2_QDMA;
	else if (!strncmp("dpdmux", t_ptr, 6))
		dev->dev_type = DPAA2_MUX;
	else if (!strncmp("dprtc", t_ptr, 5))
		dev->dev_type = DPAA2_DPRTC;
	else if (!strncmp("dprc", t_ptr, 4))
		dev->dev_type = DPAA2_DPRC;
	else
		dev->dev_type = DPAA2_UNKNOWN;

	t_ptr = strtok(NULL, ".");
	if (!t_ptr) {
		DPAA2_BUS_ERR("Skipping invalid device (%s)", dup_dev_name);
		ret = 0;
		goto cleanup;
	}

	sscanf(t_ptr, "%hu", &dev->object_id);
	dev->device.name = strdup(dev_name);
	if (!dev->device.name) {
		DPAA2_BUS_ERR("Unable to clone device name. Out of memory");
		ret = -ENOMEM;
		goto cleanup;
	}
	dev->device.devargs = rte_bus_find_devargs(&rte_fslmc_bus.bus, dev_name);

	/* Update the device found into the device_count table */
	fslmc_bus_device_count[dev->dev_type]++;

	/* Add device in the fslmc device list */
	insert_in_device_list(dev);

	/* Don't need the duplicated device filesystem entry anymore */
	free(dup_dev_name);

	return 0;
cleanup:
	free(dup_dev_name);
	if (dev) {
		rte_intr_instance_free(dev->intr_handle);
		free(dev);
	}
	return ret;
}

static int
rte_fslmc_parse(const char *name, void *addr)
{
	uint16_t dev_id;
	const char *t_ptr;
	const char *sep;
	uint8_t sep_exists = 0;
	int ret = -1;

	/* There are multiple ways this can be called, with bus:dev, name=dev
	 * or just dev. In all cases, the 'addr' is actually a string.
	 */
	sep = strchr(name, ':');
	if (!sep) {
		/* check for '=' */
		sep = strchr(name, '=');
		if (!sep)
			sep_exists = 0;
		else
			sep_exists = 1;
	} else
		sep_exists = 1;

	/* Check if starting part if either of 'fslmc:' or 'name=', separator
	 * exists.
	 */
	if (sep_exists) {
		/* If either of "fslmc" or "name" are starting part */
		if (!strncmp(name, rte_fslmc_bus.bus.name, strlen(rte_fslmc_bus.bus.name)) ||
		   (!strncmp(name, "name", strlen("name")))) {
			goto jump_out;
		} else {
			DPAA2_BUS_DEBUG("Invalid device for matching (%s).",
					name);
			ret = -EINVAL;
			goto err_out;
		}
	} else
		sep = name;

jump_out:
	/* Validate device name */
	if (strncmp("dpni", sep, 4) &&
	    strncmp("dpseci", sep, 6) &&
	    strncmp("dpcon", sep, 5) &&
	    strncmp("dpbp", sep, 4) &&
	    strncmp("dpio", sep, 4) &&
	    strncmp("dpci", sep, 4) &&
	    strncmp("dpmcp", sep, 5) &&
	    strncmp("dpdmai", sep, 6) &&
	    strncmp("dpdmux", sep, 6)) {
		DPAA2_BUS_DEBUG("Unknown or unsupported device (%s)", sep);
		ret = -EINVAL;
		goto err_out;
	}

	t_ptr = strchr(sep, '.');
	if (!t_ptr || sscanf(t_ptr + 1, "%hu", &dev_id) != 1) {
		DPAA2_BUS_ERR("Missing device id in device name (%s)", sep);
		ret = -EINVAL;
		goto err_out;
	}

	if (addr)
		strcpy(addr, sep);

	ret = 0;
err_out:
	return ret;
}

static int
fslmc_dev_compare(const char *name1, const char *name2)
{
	char devname1[32], devname2[32];

	if (rte_fslmc_parse(name1, devname1) != 0 ||
			rte_fslmc_parse(name2, devname2) != 0)
		return 1;

	return strncmp(devname1, devname2, sizeof(devname1));
}

static int
rte_fslmc_scan(void)
{
	int ret;
	char fslmc_dirpath[PATH_MAX];
	DIR *dir;
	struct dirent *entry;
	static int process_once;
	int groupid;
	char *group_name;

	if (process_once) {
		struct rte_dpaa2_device *dev;

		DPAA2_BUS_DEBUG("Fslmc bus already scanned. Not rescanning");
		RTE_BUS_FOREACH_DEV(dev, &rte_fslmc_bus.bus) {
			dev->device.devargs = rte_bus_find_devargs(&rte_fslmc_bus.bus,
				dev->device.name);
		}
		return 0;
	}

	/* Now we only support single group per process.*/
	group_name = getenv("DPRC");
	if (!group_name) {
		DPAA2_BUS_DEBUG("DPAA2: DPRC not available");
		ret = -EINVAL;
		goto scan_fail;
	}

	ret = fslmc_get_container_group(group_name, &groupid);
	if (ret != 0)
		goto scan_fail;

	/* Scan devices on the group */
	sprintf(fslmc_dirpath, "%s/%s", SYSFS_FSL_MC_DEVICES, group_name);
	dir = opendir(fslmc_dirpath);
	if (!dir) {
		DPAA2_BUS_ERR("Unable to open VFIO group directory");
		goto scan_fail;
	}

	/* Scan the DPRC container object */
	ret = scan_one_fslmc_device(group_name);
	if (ret != 0) {
		/* Error in parsing directory - exit gracefully */
		goto scan_fail_cleanup;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.' || entry->d_type != DT_DIR)
			continue;

		ret = scan_one_fslmc_device(entry->d_name);
		if (ret != 0) {
			/* Error in parsing directory - exit gracefully */
			goto scan_fail_cleanup;
		}
	}

	closedir(dir);

	DPAA2_BUS_INFO("FSLMC Bus scan completed");
	/* If debugging is enabled, device list is dumped to log output */
	dump_device_list();

	/* Bus initialization - only if devices were found */
	if (!TAILQ_EMPTY(&rte_fslmc_bus.bus.device_list)) {
		static const struct rte_mbuf_dynfield dpaa2_seqn_dynfield_desc = {
			.name = DPAA2_SEQN_DYNFIELD_NAME,
			.size = sizeof(dpaa2_seqn_t),
			.align = alignof(dpaa2_seqn_t),
		};

		dpaa2_seqn_dynfield_offset =
			rte_mbuf_dynfield_register(&dpaa2_seqn_dynfield_desc);
		if (dpaa2_seqn_dynfield_offset < 0) {
			DPAA2_BUS_ERR("Failed to register mbuf field for dpaa sequence number");
			return 0;
		}

		ret = fslmc_vfio_setup_group();
		if (ret) {
			DPAA2_BUS_ERR("Unable to setup VFIO %d", ret);
			return 0;
		}

		/* Map existing segments as well as, in case of hotpluggable memory,
		 * install callback handler.
		 */
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			ret = fslmc_vfio_dmamap();
			if (ret) {
				DPAA2_BUS_ERR("Unable to DMA map existing VAs: (%d)", ret);
				DPAA2_BUS_ERR("FSLMC VFIO Mapping failed");
				return 0;
			}
		}

		ret = fslmc_vfio_process_group();
		if (ret) {
			DPAA2_BUS_ERR("Unable to setup devices %d", ret);
			return 0;
		}
	}

	process_once = 1;

	return 0;

scan_fail_cleanup:
	closedir(dir);

	/* Remove all devices in the list */
	cleanup_fslmc_device_list();
scan_fail:
	DPAA2_BUS_DEBUG("FSLMC Bus Not Available. Skipping (%d)", ret);
	/* Irrespective of failure, scan only return success */
	return 0;
}

static bool
fslmc_bus_match(const struct rte_driver *drv, const struct rte_device *dev)
{
	const struct rte_dpaa2_driver *dpaa2_drv = RTE_BUS_DRIVER(drv, *dpaa2_drv);
	const struct rte_dpaa2_device *dpaa2_dev = RTE_BUS_DEVICE(dev, *dpaa2_dev);

	if (dpaa2_drv->drv_type == dpaa2_dev->dev_type)
		return true;

	return false;
}

static int
rte_fslmc_close(void)
{
	int ret = 0;

	ret = fslmc_vfio_close_group();
	if (ret)
		DPAA2_BUS_INFO("Unable to close devices %d", ret);

	return 0;
}

/*register a fslmc bus based dpaa2 driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_driver_register)
void
rte_fslmc_driver_register(struct rte_dpaa2_driver *driver)
{
	RTE_VERIFY(driver);
	RTE_VERIFY(driver->probe != NULL);

	rte_bus_add_driver(&rte_fslmc_bus.bus, &driver->driver);
}

/*un-register a fslmc bus based dpaa2 driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_driver_unregister)
void
rte_fslmc_driver_unregister(struct rte_dpaa2_driver *driver)
{
	rte_bus_remove_driver(&rte_fslmc_bus.bus, &driver->driver);
}

/*
 * All device has iova as va
 */
static inline int
fslmc_all_device_support_iova(void)
{
	struct rte_dpaa2_device *dev;
	struct rte_dpaa2_driver *drv;

	RTE_BUS_FOREACH_DEV(dev, &rte_fslmc_bus.bus) {
		RTE_BUS_FOREACH_DRV(drv, &rte_fslmc_bus.bus) {
			if (!fslmc_bus_match(&drv->driver, &dev->device))
				continue;
			/* if the driver is not supporting IOVA */
			if (!(drv->drv_flags & RTE_DPAA2_DRV_IOVA_AS_VA))
				return 0;
		}
	}
	return 1;
}

/*
 * Get iommu class of DPAA2 devices on the bus.
 */
static enum rte_iova_mode
rte_dpaa2_get_iommu_class(void)
{
	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		return RTE_IOVA_PA;

	if (TAILQ_EMPTY(&rte_fslmc_bus.bus.device_list))
		return RTE_IOVA_DC;

	/* check if all devices on the bus support Virtual addressing or not */
	if (fslmc_all_device_support_iova() != 0 && rte_vfio_noiommu_is_enabled() == 0)
		return RTE_IOVA_VA;

	return RTE_IOVA_PA;
}

static int
fslmc_bus_probe_device(struct rte_driver *driver, struct rte_device *rte_dev)
{
	struct rte_dpaa2_device *dev = RTE_BUS_DEVICE(rte_dev, *dev);
	struct rte_dpaa2_driver *drv = RTE_BUS_DRIVER(driver, *drv);
	int ret = 0;

	if (dev->device.devargs &&
	    dev->device.devargs->policy == RTE_DEV_BLOCKED) {
		DPAA2_BUS_DEBUG("%s Blocked, skipping",
			      dev->device.name);
		return 0;
	}

	ret = drv->probe(drv, dev);
	if (ret != 0) {
		DPAA2_BUS_ERR("Unable to probe");
	} else {
		DPAA2_BUS_INFO("%s Plugged",  dev->device.name);
	}

	return ret;
}

static int
fslmc_bus_unplug(struct rte_device *rte_dev)
{
	struct rte_dpaa2_device *dev = RTE_BUS_DEVICE(rte_dev, *dev);
	const struct rte_dpaa2_driver *drv = RTE_BUS_DRIVER(rte_dev->driver, *drv);

	if (drv->remove != NULL) {
		drv->remove(dev);
		dev->device.driver = NULL;
		DPAA2_BUS_INFO("%s Un-Plugged",  dev->device.name);
		return 0;
	}

	return -ENODEV;
}

struct rte_fslmc_bus rte_fslmc_bus = {
	.bus = {
		.scan = rte_fslmc_scan,
		.probe = rte_bus_generic_probe,
		.cleanup = rte_fslmc_close,
		.parse = rte_fslmc_parse,
		.dev_compare = fslmc_dev_compare,
		.find_device = rte_bus_generic_find_device,
		.get_iommu_class = rte_dpaa2_get_iommu_class,
		.match = fslmc_bus_match,
		.probe_device = fslmc_bus_probe_device,
		.unplug = fslmc_bus_unplug,
		.dev_iterate = rte_bus_generic_dev_iterate,
	},
};

RTE_REGISTER_BUS(fslmc, rte_fslmc_bus.bus);
RTE_LOG_REGISTER_DEFAULT(dpaa2_logtype_bus, NOTICE);
