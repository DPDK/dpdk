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
#include <dev_vfio.h>

#include "private.h"
#include <fslmc_vfio.h>
#include "fslmc_logs.h"

#include <dpaax_iova_table.h>

#define VFIO_IOMMU_GROUP_PATH "/sys/kernel/iommu_groups"

struct rte_bus rte_fslmc_bus;
static int fslmc_bus_device_count[DPAA2_DEVTYPE_MAX];

/*
 * Control devices (MPORTAL, IO, CON, CI, BPOOL, MUX, DPRC) are bus infrastructure,
 * not regular devices. They are initialized during scan/setup and don't participate
 * in normal probe/unplug. Keep them in a separate list to avoid interference with
 * generic bus operations.
 */
struct fslmc_control_device_list fslmc_control_devices =
	TAILQ_HEAD_INITIALIZER(fslmc_control_devices);

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

	RTE_BUS_FOREACH_DEV(dev, &rte_fslmc_bus) {
		comp = compare_dpaa2_devname(newdev, dev);
		if (comp < 0) {
			rte_bus_insert_device(&rte_fslmc_bus, &dev->device, &newdev->device);
			inserted = 1;
			break;
		}
	}

	if (!inserted)
		rte_bus_add_device(&rte_fslmc_bus, &newdev->device);

	if (newdev->dev_type < DPAA2_DEVTYPE_MAX)
		fslmc_bus_device_count[newdev->dev_type]++;
}

static void
fslmc_free_device(struct rte_device *rte_dev)
{
	struct rte_dpaa2_device *dev = RTE_BUS_DEVICE(rte_dev, *dev);

	if (dev->dev_type < DPAA2_DEVTYPE_MAX)
		fslmc_bus_device_count[dev->dev_type]--;
	rte_intr_instance_free(dev->intr_handle);
	free(dev);
}

void
fslmc_bus_remove_device(struct rte_dpaa2_device *dev)
{
	rte_bus_remove_device(&rte_fslmc_bus, &dev->device);
	fslmc_free_device(&dev->device);
}

static void
insert_in_control_device_list(struct rte_dpaa2_device *dev)
{
	struct rte_device *rte_dev;
	struct rte_device *prev;

	prev = NULL;
	TAILQ_FOREACH(rte_dev, &fslmc_control_devices, next) {
		if (compare_dpaa2_devname(dev, RTE_BUS_DEVICE(rte_dev, *dev)) < 0)
			break;
		prev = rte_dev;
	}

	if (prev != NULL)
		TAILQ_INSERT_AFTER(&fslmc_control_devices, prev, &dev->device, next);
	else
		TAILQ_INSERT_HEAD(&fslmc_control_devices, &dev->device, next);

	if (dev->dev_type < DPAA2_DEVTYPE_MAX)
		fslmc_bus_device_count[dev->dev_type]++;
}

void
fslmc_remove_control_device(struct rte_dpaa2_device *dev)
{
	if (dev->dev_type < DPAA2_DEVTYPE_MAX)
		fslmc_bus_device_count[dev->dev_type]--;

	TAILQ_REMOVE(&fslmc_control_devices, &dev->device, next);
	rte_intr_instance_free(dev->intr_handle);
	free(dev);
}

static void
dump_device_list(void)
{
	struct rte_dpaa2_device *dev;
	struct rte_device *rte_dev;

	/* Only if the log level has been set to Debugging, print list */
	if (rte_log_can_log(dpaa2_logtype_bus, RTE_LOG_DEBUG)) {
		DPAA2_BUS_LOG(DEBUG, "List of devices scanned on bus:");
		RTE_BUS_FOREACH_DEV(dev, &rte_fslmc_bus) {
			DPAA2_BUS_LOG(DEBUG, "\t\t%s", dev->device.name);
		}
		DPAA2_BUS_LOG(DEBUG, "List of control devices:");
		TAILQ_FOREACH(rte_dev, &fslmc_control_devices, next) {
			dev = RTE_BUS_DEVICE(rte_dev, struct rte_dpaa2_device);
			DPAA2_BUS_LOG(DEBUG, "\t\t%s", dev->device.name);
		}
	}
}

static int
scan_one_fslmc_device(char *dev_name)
{
	enum rte_dpaa2_dev_type dev_type = DPAA2_UNKNOWN;
	struct rte_dpaa2_device *dev = NULL;
	static const struct {
		const char *prefix;
		enum rte_dpaa2_dev_type type;
	} dev_types[] = {
		{ "dpni.", DPAA2_ETH },
		{ "dpseci.", DPAA2_CRYPTO },
		{ "dpcon.", DPAA2_CON },
		{ "dpbp.", DPAA2_BPOOL },
		{ "dpio.", DPAA2_IO },
		{ "dpci.", DPAA2_CI },
		{ "dpmcp.", DPAA2_MPORTAL },
		{ "dpdmai.", DPAA2_QDMA },
		{ "dpdmux.", DPAA2_MUX },
		{ "dprtc.", DPAA2_DPRTC },
		{ "dprc.", DPAA2_DPRC },
	};
	char *dev_id;
	int ret = -1;

	if (dev_name == NULL)
		return ret;

	for (unsigned int i = 0; i < RTE_DIM(dev_types); i++) {
		if (strncmp(dev_types[i].prefix, dev_name, strlen(dev_types[i].prefix)) != 0)
			continue;
		dev_id = dev_name + strlen(dev_types[i].prefix);
		dev_type = dev_types[i].type;
		break;
	}

	if (dev_type == DPAA2_UNKNOWN) {
		char *ptr = strchr(dev_name, '.');

		if (ptr == NULL) {
			DPAA2_BUS_ERR("Invalid device found: (%s)", dev_name);
			ret = 0;
			goto cleanup;
		}
		dev_id = ptr + 1;
	}

	/*
	 * DPAA2_MPORTAL and DPAA2_IO types are handled separately,
	 * see fslmc_filter_control_devices()
	 *
	 * Note: Only check for explicit blocklist (RTE_DEV_BLOCKED).
	 * Control objects (dpbp, dpcon, etc.) are required even in allowlist
	 * mode as they are initialized by fslmc_vfio_process_group(), not probed.
	 */
	if (dev_type != DPAA2_MPORTAL && dev_type != DPAA2_IO) {
		struct rte_devargs *devargs = rte_bus_find_devargs(&rte_fslmc_bus, dev_name);

		if (devargs && devargs->policy == RTE_DEV_BLOCKED) {
			DPAA2_BUS_DEBUG("Skipping blocklisted device (%s)", dev_name);
			return 0;
		}
	}

	/* For secondary processes, control objects are not needed */
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		switch (dev_type) {
		case DPAA2_ETH:
		case DPAA2_CRYPTO:
		case DPAA2_QDMA:
		case DPAA2_IO:
		case DPAA2_MPORTAL:
		case DPAA2_DPRC:
			break;
		default:
			DPAA2_BUS_DEBUG("Skipping device in secondary process (%s)", dev_name);
			return 0;
		}
	}

	dev = calloc(1, sizeof(struct rte_dpaa2_device));
	if (!dev) {
		DPAA2_BUS_ERR("Unable to allocate device object");
		return -ENOMEM;
	}

	dev->device.numa_node = SOCKET_ID_ANY;
	dev->dev_type = dev_type;

	/* Allocate interrupt instance */
	dev->intr_handle =
		rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (dev->intr_handle == NULL) {
		DPAA2_BUS_ERR("Failed to allocate intr handle");
		ret = -ENOMEM;
		goto cleanup;
	}

	if (sscanf(dev_id, "%hu", &dev->object_id) != 1) {
		DPAA2_BUS_ERR("Failed to parse object ID");
		ret = -EINVAL;
		goto cleanup;
	}

	if (rte_strscpy(dev->name, dev_name, sizeof(dev->name)) < 0) {
		DPAA2_BUS_ERR("Unable to copy device name: %s", rte_strerror(rte_errno));
		ret = -rte_errno;
		goto cleanup;
	}
	dev->device.name = dev->name;
	dev->device.devargs = rte_bus_find_devargs(&rte_fslmc_bus, dev_name);

	switch (dev_type) {
	case DPAA2_ETH:
	case DPAA2_CRYPTO:
	case DPAA2_QDMA:
		insert_in_device_list(dev);
		break;
	default:
		insert_in_control_device_list(dev);
		break;
	}

	return 0;
cleanup:
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
		if (!strncmp(name, rte_fslmc_bus.name, strlen(rte_fslmc_bus.name)) ||
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
fslmc_filter_control_devices(void)
{
	bool is_dpmcp_in_blocklist = false, is_dpio_in_blocklist = false;
	int dpmcp_count = 0, dpio_count = 0;
	struct rte_dpaa2_device *dev;
	struct rte_device *rte_dev;
	struct rte_device *tmp_dev;

	/* Track MPORTAL/DPIO blocklists */
	RTE_TAILQ_FOREACH_SAFE(rte_dev, &fslmc_control_devices, next, tmp_dev) {
		dev = RTE_BUS_DEVICE(rte_dev, *dev);
		if (dev->dev_type != DPAA2_MPORTAL && dev->dev_type != DPAA2_IO)
			continue;
		/* Only check for explicit blocklist, not allowlist */
		if (dev->device.devargs && dev->device.devargs->policy == RTE_DEV_BLOCKED) {
			DPAA2_BUS_LOG(DEBUG, "%s Blocked, skipping", dev->device.name);
			if (dev->dev_type == DPAA2_MPORTAL)
				is_dpmcp_in_blocklist = true;
			else if (dev->dev_type == DPAA2_IO)
				is_dpio_in_blocklist = true;
			fslmc_remove_control_device(dev);
			continue;
		}
		if (dev->dev_type == DPAA2_MPORTAL)
			dpmcp_count++;
		else if (dev->dev_type == DPAA2_IO)
			dpio_count++;
	}

	if (dpmcp_count == 0) {
		DPAA2_BUS_ERR("No MC Portal device found");
		return -ENODEV;
	}

	/* Automatic MPORTAL split: primary keeps first, secondary keeps last */
	if (!is_dpmcp_in_blocklist) {
		int current_device = 0;
		int keep_index;

		/* Check MPORTAL availability for secondary */
		if (rte_eal_process_type() == RTE_PROC_SECONDARY && dpmcp_count < 2) {
			DPAA2_BUS_ERR("No MC Portal device found for secondary");
			return -ENODEV;
		}

		if (rte_eal_process_type() == RTE_PROC_PRIMARY)
			keep_index = 0;
		else
			keep_index = dpmcp_count - 1;

		RTE_TAILQ_FOREACH_SAFE(rte_dev, &fslmc_control_devices, next, tmp_dev) {
			dev = RTE_BUS_DEVICE(rte_dev, *dev);
			if (dev->dev_type != DPAA2_MPORTAL)
				continue;
			if (current_device != keep_index)
				fslmc_remove_control_device(dev);

			current_device++;
			if (current_device == dpmcp_count)
				break;
		}
	}

	/* Automatic DPIO split: secondary keeps last only, primary removes last */
	if (!is_dpio_in_blocklist && dpio_count > 1) {
		int last_index = dpio_count - 1;
		int current_device = 0;

		RTE_TAILQ_FOREACH_SAFE(rte_dev, &fslmc_control_devices, next, tmp_dev) {
			dev = RTE_BUS_DEVICE(rte_dev, *dev);
			if (dev->dev_type != DPAA2_IO)
				continue;

			if (rte_eal_process_type() == RTE_PROC_SECONDARY &&
					current_device != last_index) {
				fslmc_remove_control_device(dev);
			} else if (rte_eal_process_type() == RTE_PROC_PRIMARY &&
					current_device == last_index) {
				fslmc_remove_control_device(dev);
			}

			current_device++;
			if (current_device == dpio_count)
				break;
		}
	}

	return 0;
}

static int
rte_fslmc_scan(void)
{
	struct rte_dpaa2_device *dev;
	int ret;
	char fslmc_dirpath[PATH_MAX];
	DIR *dir;
	struct dirent *entry;
	static int process_once;
	int groupid;
	char *group_name;

	if (process_once) {
		DPAA2_BUS_DEBUG("Fslmc bus already scanned. Not rescanning");
		RTE_BUS_FOREACH_DEV(dev, &rte_fslmc_bus) {
			dev->device.devargs = rte_bus_find_devargs(&rte_fslmc_bus,
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

	/* for container groups to work, VFIO must be in group mode */
	if (dev_vfio_get_mode() != DEV_VFIO_MODE_GROUP) {
		DPAA2_BUS_DEBUG("DPAA2: VFIO not in group mode");
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

	process_once = 1;

	return 0;

scan_fail_cleanup:
	closedir(dir);

	while (!TAILQ_EMPTY(&fslmc_control_devices)) {
		struct rte_device *rte_dev = TAILQ_FIRST(&fslmc_control_devices);

		dev = RTE_BUS_DEVICE(rte_dev, *dev);
		fslmc_remove_control_device(dev);
	}

	RTE_BUS_FOREACH_DEV(dev, &rte_fslmc_bus)
		fslmc_bus_remove_device(dev);
scan_fail:
	DPAA2_BUS_DEBUG("FSLMC Bus Not Available. Skipping (%d)", ret);
	/* Irrespective of failure, scan only return success */
	return 0;
}

/* Bus initialization needs the DPDK heap and DMA mapping of the memory
 * segments, which EAL only sets up after the bus scan, so it is done here.
 */
static int
rte_fslmc_probe(struct rte_bus *bus)
{
	static const struct rte_mbuf_dynfield dpaa2_seqn_dynfield_desc = {
		.name = DPAA2_SEQN_DYNFIELD_NAME,
		.size = sizeof(dpaa2_seqn_t),
		.align = alignof(dpaa2_seqn_t),
	};
	int ret;

	if (TAILQ_EMPTY(&rte_fslmc_bus.device_list) && TAILQ_EMPTY(&fslmc_control_devices))
		return 0;

	dpaa2_seqn_dynfield_offset =
		rte_mbuf_dynfield_register(&dpaa2_seqn_dynfield_desc);
	if (dpaa2_seqn_dynfield_offset < 0) {
		DPAA2_BUS_ERR("Failed to register mbuf field for dpaa sequence number");
		return dpaa2_seqn_dynfield_offset;
	}

	ret = fslmc_vfio_setup_group();
	if (ret) {
		DPAA2_BUS_ERR("Unable to setup VFIO %d", ret);
		return ret;
	}

	/* Map existing segments as well as, in case of hotpluggable memory,
	 * install callback handler.
	 */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		ret = fslmc_vfio_dmamap();
		if (ret) {
			DPAA2_BUS_ERR("Unable to DMA map existing VAs: (%d)", ret);
			DPAA2_BUS_ERR("FSLMC VFIO Mapping failed");
			goto vfio_close_group;
		}
	}

	ret = fslmc_filter_control_devices();
	if (ret) {
		DPAA2_BUS_ERR("Unable to filter control devices %d", ret);
		goto vfio_dma_unmap;
	}

	ret = fslmc_vfio_process_group();
	if (ret) {
		DPAA2_BUS_ERR("Unable to setup devices %d", ret);
		goto vfio_dma_unmap;
	}

	return rte_bus_generic_probe(bus);

vfio_dma_unmap:
	fslmc_vfio_dmaunmap();
vfio_close_group:
	fslmc_vfio_close_group();
	return ret;
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
fslmc_bus_unplug_device(struct rte_device *rte_dev);

static int
rte_fslmc_close(struct rte_bus *bus)
{
	struct rte_dpaa2_device *dev;
	int ret = 0;

	RTE_BUS_FOREACH_DEV(dev, bus) {
		if (rte_dev_is_probed(&dev->device) && fslmc_bus_unplug_device(&dev->device))
			DPAA2_BUS_ERR("Unable to remove %s", dev->device.name);
	}

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

	rte_bus_add_driver(&rte_fslmc_bus, &driver->driver);
}

/*un-register a fslmc bus based dpaa2 driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_fslmc_driver_unregister)
void
rte_fslmc_driver_unregister(struct rte_dpaa2_driver *driver)
{
	rte_bus_remove_driver(&rte_fslmc_bus, &driver->driver);
}

/*
 * All device has iova as va
 */
static inline int
fslmc_all_device_support_iova(void)
{
	struct rte_dpaa2_device *dev;
	struct rte_dpaa2_driver *drv;

	RTE_BUS_FOREACH_DEV(dev, &rte_fslmc_bus) {
		RTE_BUS_FOREACH_DRV(drv, &rte_fslmc_bus) {
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

	if (TAILQ_EMPTY(&rte_fslmc_bus.device_list))
		return RTE_IOVA_DC;

	/* check if all devices on the bus support Virtual addressing or not */
	if (fslmc_all_device_support_iova() != 0 &&
			dev_vfio_get_iova_mode() == DEV_VFIO_IOVA_MODE_VA)
		return RTE_IOVA_VA;

	return RTE_IOVA_PA;
}

static int
fslmc_bus_probe_device(struct rte_driver *driver, struct rte_device *rte_dev)
{
	struct rte_dpaa2_device *dev = RTE_BUS_DEVICE(rte_dev, *dev);
	struct rte_dpaa2_driver *drv = RTE_BUS_DRIVER(driver, *drv);
	int ret = 0;

	/* FIXME: probe_device should allocate intr_handle */
	ret = drv->probe(drv, dev);
	if (ret != 0) {
		DPAA2_BUS_ERR("Unable to probe");
	} else {
		DPAA2_BUS_INFO("%s Plugged",  dev->device.name);
	}

	return ret;
}

static int
fslmc_bus_unplug_device(struct rte_device *rte_dev)
{
	struct rte_dpaa2_device *dev = RTE_BUS_DEVICE(rte_dev, *dev);
	const struct rte_dpaa2_driver *drv = RTE_BUS_DRIVER(rte_dev->driver, *drv);

	if (drv->remove != NULL) {
		int ret = drv->remove(dev);
		if (ret != 0)
			return ret;
		/* FIXME: unplug_device should free intr_handle */
		DPAA2_BUS_INFO("%s Un-Plugged",  dev->device.name);
		return 0;
	}

	return -ENODEV;
}

struct rte_bus rte_fslmc_bus = {
	.scan = rte_fslmc_scan,
	.probe = rte_fslmc_probe,
	.cleanup = rte_fslmc_close,
	.parse = rte_fslmc_parse,
	.dev_compare = fslmc_dev_compare,
	.find_device = rte_bus_generic_find_device,
	.get_iommu_class = rte_dpaa2_get_iommu_class,
	.match = fslmc_bus_match,
	.probe_device = fslmc_bus_probe_device,
	.unplug_device = fslmc_bus_unplug_device,
	.dev_iterate = rte_bus_generic_dev_iterate,
};

RTE_REGISTER_BUS(fslmc, rte_fslmc_bus);
RTE_LOG_REGISTER_DEFAULT(dpaa2_logtype_bus, NOTICE);
