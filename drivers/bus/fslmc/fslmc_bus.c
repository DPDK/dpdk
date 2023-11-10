/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2016,2018-2021 NXP
 *
 */

#include <string.h>
#include <dirent.h>
#include <stdbool.h>

#include <rte_log.h>
#include <bus_driver.h>
#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>
#include <ethdev_driver.h>
#include <rte_mbuf_dyn.h>

#include "private.h"
#include <fslmc_vfio.h>
#include "fslmc_logs.h"

#include <dpaax_iova_table.h>

#define VFIO_IOMMU_GROUP_PATH "/sys/kernel/iommu_groups"
#define FSLMC_BUS_NAME	fslmc

struct rte_fslmc_bus rte_fslmc_bus;
uint8_t dpaa2_virt_mode;

#define DPAA2_SEQN_DYNFIELD_NAME "dpaa2_seqn_dynfield"
int dpaa2_seqn_dynfield_offset = -1;

uint32_t
rte_fslmc_get_device_count(enum rte_dpaa2_dev_type device_type)
{
	if (device_type >= DPAA2_DEVTYPE_MAX)
		return 0;
	return rte_fslmc_bus.device_count[device_type];
}

static void
cleanup_fslmc_device_list(void)
{
	struct rte_dpaa2_device *dev;
	struct rte_dpaa2_device *t_dev;

	RTE_TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next, t_dev) {
		TAILQ_REMOVE(&rte_fslmc_bus.device_list, dev, next);
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
	struct rte_dpaa2_device *tdev = NULL;

	RTE_TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next, tdev) {
		comp = compare_dpaa2_devname(newdev, dev);
		if (comp < 0) {
			TAILQ_INSERT_BEFORE(dev, newdev, next);
			inserted = 1;
			break;
		}
	}

	if (!inserted)
		TAILQ_INSERT_TAIL(&rte_fslmc_bus.device_list, newdev, next);
}

static struct rte_devargs *
fslmc_devargs_lookup(struct rte_dpaa2_device *dev)
{
	struct rte_devargs *devargs;
	char dev_name[32];

	RTE_EAL_DEVARGS_FOREACH("fslmc", devargs) {
		devargs->bus->parse(devargs->name, &dev_name);
		if (strcmp(dev_name, dev->device.name) == 0) {
			DPAA2_BUS_INFO("**Devargs matched %s", dev_name);
			return devargs;
		}
	}
	return NULL;
}

static void
dump_device_list(void)
{
	struct rte_dpaa2_device *dev;

	/* Only if the log level has been set to Debugging, print list */
	if (rte_log_can_log(dpaa2_logtype_bus, RTE_LOG_DEBUG)) {
		DPAA2_BUS_LOG(DEBUG, "List of devices scanned on bus:");
		TAILQ_FOREACH(dev, &rte_fslmc_bus.device_list, next) {
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

	dev->device.bus = &rte_fslmc_bus.bus;
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
	dev->device.devargs = fslmc_devargs_lookup(dev);

	/* Update the device found into the device_count table */
	rte_fslmc_bus.device_count[dev->dev_type]++;

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
	char *t_ptr;
	const char *sep;
	uint8_t sep_exists = 0;
	int ret = -1;

	DPAA2_BUS_DEBUG("Parsing dev=(%s)", name);

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
		if (!strncmp(name, RTE_STR(FSLMC_BUS_NAME),
			     strlen(RTE_STR(FSLMC_BUS_NAME))) ||
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
rte_fslmc_scan(void)
{
	int ret;
	char fslmc_dirpath[PATH_MAX];
	DIR *dir;
	struct dirent *entry;
	static int process_once;
	int groupid;

	if (process_once) {
		DPAA2_BUS_DEBUG("Fslmc bus already scanned. Not rescanning");
		return 0;
	}
	process_once = 1;

	ret = fslmc_get_container_group(&groupid);
	if (ret != 0)
		goto scan_fail;

	/* Scan devices on the group */
	sprintf(fslmc_dirpath, "%s/%s", SYSFS_FSL_MC_DEVICES, fslmc_container);
	dir = opendir(fslmc_dirpath);
	if (!dir) {
		DPAA2_BUS_ERR("Unable to open VFIO group directory");
		goto scan_fail;
	}

	/* Scan the DPRC container object */
	ret = scan_one_fslmc_device(fslmc_container);
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

static int
rte_fslmc_match(struct rte_dpaa2_driver *dpaa2_drv,
		struct rte_dpaa2_device *dpaa2_dev)
{
	if (dpaa2_drv->drv_type == dpaa2_dev->dev_type)
		return 0;

	return 1;
}

static int
rte_fslmc_probe(void)
{
	int ret = 0;
	int probe_all;

	struct rte_dpaa2_device *dev;
	struct rte_dpaa2_driver *drv;

	static const struct rte_mbuf_dynfield dpaa2_seqn_dynfield_desc = {
		.name = DPAA2_SEQN_DYNFIELD_NAME,
		.size = sizeof(dpaa2_seqn_t),
		.align = __alignof__(dpaa2_seqn_t),
	};

	if (TAILQ_EMPTY(&rte_fslmc_bus.device_list))
		return 0;

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
		ret = rte_fslmc_vfio_dmamap();
		if (ret) {
			DPAA2_BUS_ERR("Unable to DMA map existing VAs: (%d)",
				      ret);
			/* Not continuing ahead */
			DPAA2_BUS_ERR("FSLMC VFIO Mapping failed");
			return 0;
		}
	}

	ret = fslmc_vfio_process_group();
	if (ret) {
		DPAA2_BUS_ERR("Unable to setup devices %d", ret);
		return 0;
	}

	probe_all = rte_fslmc_bus.bus.conf.scan_mode != RTE_BUS_SCAN_ALLOWLIST;

	/* In case of PA, the FD addresses returned by qbman APIs are physical
	 * addresses, which need conversion into equivalent VA address for
	 * rte_mbuf. For that, a table (a serial array, in memory) is used to
	 * increase translation efficiency.
	 * This has to be done before probe as some device initialization
	 * (during) probe allocate memory (dpaa2_sec) which needs to be pinned
	 * to this table.
	 *
	 * Error is ignored as relevant logs are handled within dpaax and
	 * handling for unavailable dpaax table too is transparent to caller.
	 *
	 * And, the IOVA table is only applicable in case of PA mode.
	 */
	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		dpaax_iova_table_populate();

	TAILQ_FOREACH(dev, &rte_fslmc_bus.device_list, next) {
		TAILQ_FOREACH(drv, &rte_fslmc_bus.driver_list, next) {
			ret = rte_fslmc_match(drv, dev);
			if (ret)
				continue;

			if (!drv->probe)
				continue;

			if (rte_dev_is_probed(&dev->device))
				continue;

			if (dev->device.devargs &&
			    dev->device.devargs->policy == RTE_DEV_BLOCKED) {
				DPAA2_BUS_LOG(DEBUG, "%s Blocked, skipping",
					      dev->device.name);
				continue;
			}

			if (probe_all ||
			   (dev->device.devargs &&
			    dev->device.devargs->policy == RTE_DEV_ALLOWED)) {
				ret = drv->probe(drv, dev);
				if (ret) {
					DPAA2_BUS_ERR("Unable to probe");
				} else {
					dev->driver = drv;
					dev->device.driver = &drv->driver;
				}
			}
			break;
		}
	}

	if (rte_eal_iova_mode() == RTE_IOVA_VA)
		dpaa2_virt_mode = 1;

	return 0;
}

static struct rte_device *
rte_fslmc_find_device(const struct rte_device *start, rte_dev_cmp_t cmp,
		      const void *data)
{
	const struct rte_dpaa2_device *dstart;
	struct rte_dpaa2_device *dev;

	DPAA2_BUS_DEBUG("Finding a device named %s\n", (const char *)data);

	/* find_device is always called with an opaque object which should be
	 * passed along to the 'cmp' function iterating over all device obj
	 * on the bus.
	 */

	if (start != NULL) {
		dstart = RTE_DEV_TO_FSLMC_CONST(start);
		dev = TAILQ_NEXT(dstart, next);
	} else {
		dev = TAILQ_FIRST(&rte_fslmc_bus.device_list);
	}
	while (dev != NULL) {
		if (cmp(&dev->device, data) == 0) {
			DPAA2_BUS_DEBUG("Found device (%s)\n",
					dev->device.name);
			return &dev->device;
		}
		dev = TAILQ_NEXT(dev, next);
	}

	return NULL;
}

/*register a fslmc bus based dpaa2 driver */
void
rte_fslmc_driver_register(struct rte_dpaa2_driver *driver)
{
	RTE_VERIFY(driver);

	TAILQ_INSERT_TAIL(&rte_fslmc_bus.driver_list, driver, next);
}

/*un-register a fslmc bus based dpaa2 driver */
void
rte_fslmc_driver_unregister(struct rte_dpaa2_driver *driver)
{
	/* Cleanup the PA->VA Translation table; From wherever this function
	 * is called from.
	 */
	if (rte_eal_iova_mode() == RTE_IOVA_PA)
		dpaax_iova_table_depopulate();

	TAILQ_REMOVE(&rte_fslmc_bus.driver_list, driver, next);
}

/*
 * All device has iova as va
 */
static inline int
fslmc_all_device_support_iova(void)
{
	int ret = 0;
	struct rte_dpaa2_device *dev;
	struct rte_dpaa2_driver *drv;

	TAILQ_FOREACH(dev, &rte_fslmc_bus.device_list, next) {
		TAILQ_FOREACH(drv, &rte_fslmc_bus.driver_list, next) {
			ret = rte_fslmc_match(drv, dev);
			if (ret)
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
	bool is_vfio_noiommu_enabled = 1;
	bool has_iova_va;

	if (TAILQ_EMPTY(&rte_fslmc_bus.device_list))
		return RTE_IOVA_DC;

#ifdef RTE_LIBRTE_DPAA2_USE_PHYS_IOVA
	return RTE_IOVA_PA;
#endif

	/* check if all devices on the bus support Virtual addressing or not */
	has_iova_va = fslmc_all_device_support_iova();

#ifdef VFIO_PRESENT
	is_vfio_noiommu_enabled = rte_vfio_noiommu_is_enabled() == true ?
						true : false;
#endif

	if (has_iova_va && !is_vfio_noiommu_enabled)
		return RTE_IOVA_VA;

	return RTE_IOVA_PA;
}

static int
fslmc_bus_plug(struct rte_device *dev __rte_unused)
{
	/* No operation is performed while plugging the device */
	return 0;
}

static int
fslmc_bus_unplug(struct rte_device *dev __rte_unused)
{
	/* No operation is performed while unplugging the device */
	return 0;
}

static void *
fslmc_bus_dev_iterate(const void *start, const char *str,
		      const struct rte_dev_iterator *it __rte_unused)
{
	const struct rte_dpaa2_device *dstart;
	struct rte_dpaa2_device *dev;
	char *dup, *dev_name = NULL;

	if (str == NULL) {
		DPAA2_BUS_DEBUG("No device string");
		return NULL;
	}

	/* Expectation is that device would be name=device_name */
	if (strncmp(str, "name=", 5) != 0) {
		DPAA2_BUS_DEBUG("Invalid device string (%s)\n", str);
		return NULL;
	}

	/* Now that name=device_name format is available, split */
	dup = strdup(str);
	if (dup == NULL) {
		DPAA2_BUS_DEBUG("Dup string (%s) failed!\n", str);
		return NULL;
	}
	dev_name = dup + strlen("name=");

	if (start != NULL) {
		dstart = RTE_DEV_TO_FSLMC_CONST(start);
		dev = TAILQ_NEXT(dstart, next);
	} else {
		dev = TAILQ_FIRST(&rte_fslmc_bus.device_list);
	}

	while (dev != NULL) {
		if (strcmp(dev->device.name, dev_name) == 0) {
			free(dup);
			return &dev->device;
		}
		dev = TAILQ_NEXT(dev, next);
	}

	free(dup);
	return NULL;
}

struct rte_fslmc_bus rte_fslmc_bus = {
	.bus = {
		.scan = rte_fslmc_scan,
		.probe = rte_fslmc_probe,
		.parse = rte_fslmc_parse,
		.find_device = rte_fslmc_find_device,
		.get_iommu_class = rte_dpaa2_get_iommu_class,
		.plug = fslmc_bus_plug,
		.unplug = fslmc_bus_unplug,
		.dev_iterate = fslmc_bus_dev_iterate,
	},
	.device_list = TAILQ_HEAD_INITIALIZER(rte_fslmc_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(rte_fslmc_bus.driver_list),
	.device_count = {0},
};

RTE_REGISTER_BUS(FSLMC_BUS_NAME, rte_fslmc_bus.bus);
RTE_LOG_REGISTER_DEFAULT(dpaa2_logtype_bus, NOTICE);
