/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2016 NXP
 *
 */

#include <string.h>
#include <dirent.h>
#include <stdbool.h>

#include <rte_log.h>
#include <rte_bus.h>
#include <rte_eal_memconfig.h>
#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>
#include <rte_ethdev_driver.h>

#include <rte_fslmc.h>
#include <fslmc_vfio.h>

#define FSLMC_BUS_LOG(level, fmt, args...) \
	RTE_LOG(level, EAL, fmt "\n", ##args)

#define VFIO_IOMMU_GROUP_PATH "/sys/kernel/iommu_groups"

struct rte_fslmc_bus rte_fslmc_bus;
uint8_t dpaa2_virt_mode;

uint32_t
rte_fslmc_get_device_count(enum rte_dpaa2_dev_type device_type)
{
	if (device_type > DPAA2_DEVTYPE_MAX)
		return 0;
	return rte_fslmc_bus.device_count[device_type];
}

RTE_DEFINE_PER_LCORE(struct dpaa2_portal_dqrr, dpaa2_held_bufs);

static void
cleanup_fslmc_device_list(void)
{
	struct rte_dpaa2_device *dev;
	struct rte_dpaa2_device *t_dev;

	TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next, t_dev) {
		TAILQ_REMOVE(&rte_fslmc_bus.device_list, dev, next);
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

	TAILQ_FOREACH_SAFE(dev, &rte_fslmc_bus.device_list, next, tdev) {
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

static int
scan_one_fslmc_device(char *dev_name)
{
	char *dup_dev_name, *t_ptr;
	struct rte_dpaa2_device *dev;

	if (!dev_name)
		return -1;

	/* Ignore the Container name itself */
	if (!strncmp("dprc", dev_name, 4))
		return 0;

	/* Creating a temporary copy to perform cut-parse over string */
	dup_dev_name = strdup(dev_name);
	if (!dup_dev_name) {
		FSLMC_BUS_LOG(ERR, "Out of memory.");
		return -ENOMEM;
	}

	/* For all other devices, we allocate rte_dpaa2_device.
	 * For those devices where there is no driver, probe would release
	 * the memory associated with the rte_dpaa2_device after necessary
	 * initialization.
	 */
	dev = calloc(1, sizeof(struct rte_dpaa2_device));
	if (!dev) {
		FSLMC_BUS_LOG(ERR, "Out of memory.");
		free(dup_dev_name);
		return -ENOMEM;
	}

	/* Parse the device name and ID */
	t_ptr = strtok(dup_dev_name, ".");
	if (!t_ptr) {
		FSLMC_BUS_LOG(ERR, "Incorrect device string observed.");
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
	else
		dev->dev_type = DPAA2_UNKNOWN;

	/* Update the device found into the device_count table */
	rte_fslmc_bus.device_count[dev->dev_type]++;

	t_ptr = strtok(NULL, ".");
	if (!t_ptr) {
		FSLMC_BUS_LOG(ERR, "Incorrect device string observed (%s).",
			      t_ptr);
		goto cleanup;
	}

	sscanf(t_ptr, "%hu", &dev->object_id);
	dev->device.name = strdup(dev_name);
	if (!dev->device.name) {
		FSLMC_BUS_LOG(ERR, "Out of memory.");
		goto cleanup;
	}

	/* Add device in the fslmc device list */
	insert_in_device_list(dev);

	/* Don't need the duplicated device filesystem entry anymore */
	if (dup_dev_name)
		free(dup_dev_name);

	return 0;
cleanup:
	if (dup_dev_name)
		free(dup_dev_name);
	if (dev)
		free(dev);
	return -1;
}

static int
rte_fslmc_scan(void)
{
	int ret;
	int device_count = 0;
	char fslmc_dirpath[PATH_MAX];
	DIR *dir;
	struct dirent *entry;
	static int process_once;
	int groupid;

	if (process_once) {
		FSLMC_BUS_LOG(DEBUG,
			      "Fslmc bus already scanned. Not rescanning");
		return 0;
	}
	process_once = 1;

	ret = fslmc_get_container_group(&groupid);
	if (ret != 0)
		goto scan_fail;

	/* Scan devices on the group */
	sprintf(fslmc_dirpath, "%s/%d/devices", VFIO_IOMMU_GROUP_PATH,
		groupid);
	dir = opendir(fslmc_dirpath);
	if (!dir) {
		FSLMC_BUS_LOG(ERR, "Unable to open VFIO group dir.");
		goto scan_fail;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.' || entry->d_type != DT_LNK)
			continue;

		ret = scan_one_fslmc_device(entry->d_name);
		if (ret != 0) {
			/* Error in parsing directory - exit gracefully */
			goto scan_fail_cleanup;
		}
		device_count += 1;
	}

	FSLMC_BUS_LOG(INFO, "fslmc: Bus scan completed");

	closedir(dir);
	return 0;

scan_fail_cleanup:
	closedir(dir);

	/* Remove all devices in the list */
	cleanup_fslmc_device_list();
scan_fail:
	FSLMC_BUS_LOG(DEBUG, "FSLMC Bus Not Available. Skipping.");
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
	struct rte_dpaa2_device *dev;
	struct rte_dpaa2_driver *drv;

	if (TAILQ_EMPTY(&rte_fslmc_bus.device_list))
		return 0;

	ret = fslmc_vfio_setup_group();
	if (ret) {
		FSLMC_BUS_LOG(ERR, "Unable to setup VFIO %d", ret);
		return 0;
	}

	ret = fslmc_vfio_process_group();
	if (ret) {
		FSLMC_BUS_LOG(ERR, "Unable to setup devices %d", ret);
		return 0;
	}

	TAILQ_FOREACH(dev, &rte_fslmc_bus.device_list, next) {
		TAILQ_FOREACH(drv, &rte_fslmc_bus.driver_list, next) {
			ret = rte_fslmc_match(drv, dev);
			if (ret)
				continue;

			if (!drv->probe)
				continue;

			ret = drv->probe(drv, dev);
			if (ret)
				FSLMC_BUS_LOG(ERR, "Unable to probe.\n");
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
	struct rte_dpaa2_device *dev;

	TAILQ_FOREACH(dev, &rte_fslmc_bus.device_list, next) {
		if (start && &dev->device == start) {
			start = NULL;  /* starting point found */
			continue;
		}

		if (cmp(&dev->device, data) == 0)
			return &dev->device;
	}

	return NULL;
}

/*register a fslmc bus based dpaa2 driver */
void
rte_fslmc_driver_register(struct rte_dpaa2_driver *driver)
{
	RTE_VERIFY(driver);

	TAILQ_INSERT_TAIL(&rte_fslmc_bus.driver_list, driver, next);
	/* Update Bus references */
	driver->fslmc_bus = &rte_fslmc_bus;
}

/*un-register a fslmc bus based dpaa2 driver */
void
rte_fslmc_driver_unregister(struct rte_dpaa2_driver *driver)
{
	struct rte_fslmc_bus *fslmc_bus;

	fslmc_bus = driver->fslmc_bus;

	TAILQ_REMOVE(&fslmc_bus->driver_list, driver, next);
	/* Update Bus references */
	driver->fslmc_bus = NULL;
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

struct rte_fslmc_bus rte_fslmc_bus = {
	.bus = {
		.scan = rte_fslmc_scan,
		.probe = rte_fslmc_probe,
		.find_device = rte_fslmc_find_device,
		.get_iommu_class = rte_dpaa2_get_iommu_class,
	},
	.device_list = TAILQ_HEAD_INITIALIZER(rte_fslmc_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(rte_fslmc_bus.driver_list),
	.device_count = {0},
};

RTE_REGISTER_BUS(fslmc, rte_fslmc_bus.bus);
