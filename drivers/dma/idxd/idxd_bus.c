/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <libgen.h>

#include <bus_driver.h>
#include <dev_driver.h>
#include <rte_devargs.h>
#include <rte_eal.h>
#include <rte_memory.h>
#include <rte_log.h>
#include <rte_dmadev_pmd.h>
#include <rte_string_fns.h>

#include "idxd_internal.h"

/* default value for DSA paths, but allow override in environment for testing */
#define DSA_DEV_PATH "/dev/dsa"
#define DSA_SYSFS_PATH "/sys/bus/dsa/devices"

static unsigned int devcount;

/** unique identifier for a DSA device/WQ instance */
struct dsa_wq_addr {
	uint16_t device_id;
	uint16_t wq_id;
};

/** a DSA device instance */
struct rte_dsa_device {
	struct rte_device device;           /**< Inherit core device */
	TAILQ_ENTRY(rte_dsa_device) next;   /**< next dev in list */

	char wq_name[32];                   /**< the workqueue name/number e.g. wq0.1 */
	struct dsa_wq_addr addr;            /**< Identifies the specific WQ */
};

/* forward prototypes */
struct dsa_bus;
static int dsa_scan(void);
static int dsa_probe(void);
static struct rte_device *dsa_find_device(const struct rte_device *start,
		rte_dev_cmp_t cmp,  const void *data);
static enum rte_iova_mode dsa_get_iommu_class(void);
static int dsa_addr_parse(const char *name, void *addr);

/** List of devices */
TAILQ_HEAD(dsa_device_list, rte_dsa_device);

/**
 * Structure describing the DSA bus
 */
struct dsa_bus {
	struct rte_bus bus;               /**< Inherit the generic class */
	struct rte_driver driver;         /**< Driver struct for devices to point to */
	struct dsa_device_list device_list;  /**< List of PCI devices */
};

struct dsa_bus dsa_bus = {
	.bus = {
		.scan = dsa_scan,
		.probe = dsa_probe,
		.find_device = dsa_find_device,
		.get_iommu_class = dsa_get_iommu_class,
		.parse = dsa_addr_parse,
	},
	.driver = {
		.name = "dmadev_idxd"
	},
	.device_list = TAILQ_HEAD_INITIALIZER(dsa_bus.device_list),
};

static inline const char *
dsa_get_dev_path(void)
{
	const char *path = getenv("DSA_DEV_PATH");
	return path ? path : DSA_DEV_PATH;
}

static inline const char *
dsa_get_sysfs_path(void)
{
	const char *path = getenv("DSA_SYSFS_PATH");
	return path ? path : DSA_SYSFS_PATH;
}

static int
idxd_dev_close(struct rte_dma_dev *dev)
{
	struct idxd_dmadev *idxd = dev->data->dev_private;
	munmap(idxd->portal, 0x1000);
	return 0;
}

static const struct rte_dma_dev_ops idxd_bus_ops = {
		.dev_close = idxd_dev_close,
		.dev_dump = idxd_dump,
		.dev_configure = idxd_configure,
		.vchan_setup = idxd_vchan_setup,
		.dev_info_get = idxd_info_get,
		.stats_get = idxd_stats_get,
		.stats_reset = idxd_stats_reset,
		.vchan_status = idxd_vchan_status,
};

static void *
idxd_bus_mmap_wq(struct rte_dsa_device *dev)
{
	void *addr;
	char path[PATH_MAX];
	int fd;

	snprintf(path, sizeof(path), "%s/%s", dsa_get_dev_path(), dev->wq_name);
	fd = open(path, O_RDWR);
	if (fd < 0) {
		IDXD_PMD_ERR("Failed to open device path: %s", path);
		return NULL;
	}

	addr = mmap(NULL, 0x1000, PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (addr == MAP_FAILED) {
		IDXD_PMD_ERR("Failed to mmap device %s", path);
		return NULL;
	}

	return addr;
}

static int
read_wq_string(struct rte_dsa_device *dev, const char *filename,
		char *value, size_t valuelen)
{
	char sysfs_node[PATH_MAX];
	int len;
	int fd;

	snprintf(sysfs_node, sizeof(sysfs_node), "%s/%s/%s",
			dsa_get_sysfs_path(), dev->wq_name, filename);
	fd = open(sysfs_node, O_RDONLY);
	if (fd < 0) {
		IDXD_PMD_ERR("%s(): opening file '%s' failed: %s",
				__func__, sysfs_node, strerror(errno));
		return -1;
	}

	len = read(fd, value, valuelen - 1);
	close(fd);
	if (len < 0) {
		IDXD_PMD_ERR("%s(): error reading file '%s': %s",
				__func__, sysfs_node, strerror(errno));
		return -1;
	}
	value[len] = '\0';
	return 0;
}

static int
read_wq_int(struct rte_dsa_device *dev, const char *filename,
		int *value)
{
	char sysfs_node[PATH_MAX];
	FILE *f;
	int ret = 0;

	snprintf(sysfs_node, sizeof(sysfs_node), "%s/%s/%s",
			dsa_get_sysfs_path(), dev->wq_name, filename);
	f = fopen(sysfs_node, "r");
	if (f == NULL) {
		IDXD_PMD_ERR("%s(): opening file '%s' failed: %s",
				__func__, sysfs_node, strerror(errno));
		return -1;
	}

	if (fscanf(f, "%d", value) != 1) {
		IDXD_PMD_ERR("%s(): error reading file '%s': %s",
				__func__, sysfs_node, strerror(errno));
		ret = -1;
	}

	fclose(f);
	return ret;
}

static int
read_device_int(struct rte_dsa_device *dev, const char *filename,
		int *value)
{
	char sysfs_node[PATH_MAX];
	FILE *f;
	int ret = 0;

	snprintf(sysfs_node, sizeof(sysfs_node), "%s/dsa%d/%s",
			dsa_get_sysfs_path(), dev->addr.device_id, filename);
	f = fopen(sysfs_node, "r");
	if (f == NULL) {
		IDXD_PMD_ERR("%s(): opening file '%s' failed: %s",
				__func__, sysfs_node, strerror(errno));
		return -1;
	}

	if (fscanf(f, "%d", value) != 1) {
		IDXD_PMD_ERR("%s(): error reading file '%s': %s",
				__func__, sysfs_node, strerror(errno));
		ret = -1;
	}

	fclose(f);
	return ret;
}

static int
idxd_probe_dsa(struct rte_dsa_device *dev)
{
	struct idxd_dmadev idxd = {0};
	int ret = 0;

	IDXD_PMD_INFO("Probing device %s on numa node %d",
			dev->wq_name, dev->device.numa_node);
	if (read_wq_int(dev, "size", &ret) < 0)
		return -1;
	idxd.max_batches = ret;
	if (read_wq_int(dev, "max_batch_size", &ret) < 0)
		return -1;
	idxd.max_batch_size = ret;
	idxd.qid = dev->addr.wq_id;
	idxd.u.bus.dsa_id = dev->addr.device_id;
	idxd.sva_support = 1;

	idxd.portal = idxd_bus_mmap_wq(dev);
	if (idxd.portal == NULL) {
		IDXD_PMD_ERR("WQ mmap failed");
		return -ENOENT;
	}

	ret = idxd_dmadev_create(dev->wq_name, &dev->device, &idxd, &idxd_bus_ops);
	if (ret) {
		IDXD_PMD_ERR("Failed to create dmadev %s", dev->wq_name);
		return ret;
	}

	return 0;
}

static int search_devargs(const char *name)
{
	struct rte_devargs *devargs;
	RTE_EAL_DEVARGS_FOREACH(dsa_bus.bus.name, devargs) {
		if (strcmp(devargs->name, name) == 0)
			return 1;
	}
	return 0;
}

static int
is_for_this_process_use(struct rte_dsa_device *dev, const char *name)
{
	char *runtime_dir = strdup(rte_eal_get_runtime_dir());
	int retval = 0;
	int prefixlen;
	char *prefix;

	if (runtime_dir == NULL)
		return retval;

	prefix = basename(runtime_dir);
	prefixlen = strlen(prefix);

	if (strncmp(name, "dpdk_", 5) == 0)
		retval = 1;
	if (strncmp(name, prefix, prefixlen) == 0 && name[prefixlen] == '_')
		retval = 1;

	if (retval && dsa_bus.bus.conf.scan_mode != RTE_BUS_SCAN_UNDEFINED) {
		if (dsa_bus.bus.conf.scan_mode == RTE_BUS_SCAN_ALLOWLIST)
			retval = search_devargs(dev->device.name);
		else
			retval = !search_devargs(dev->device.name);
	}

	free(runtime_dir);
	return retval;
}

static int
dsa_probe(void)
{
	struct rte_dsa_device *dev;

	TAILQ_FOREACH(dev, &dsa_bus.device_list, next) {
		char type[64], name[64];

		if (read_wq_string(dev, "type", type, sizeof(type)) < 0 ||
				read_wq_string(dev, "name", name, sizeof(name)) < 0)
			continue;

		if (strncmp(type, "user", 4) == 0 &&
				is_for_this_process_use(dev, name)) {
			dev->device.driver = &dsa_bus.driver;
			idxd_probe_dsa(dev);
			continue;
		}
		IDXD_PMD_DEBUG("WQ '%s', not allocated to DPDK", dev->wq_name);
	}

	return 0;
}

static int
dsa_scan(void)
{
	const char *path = dsa_get_dev_path();
	struct dirent *wq;
	DIR *dev_dir;

	dev_dir = opendir(path);
	if (dev_dir == NULL) {
		if (errno == ENOENT)
			return 0; /* no bus, return without error */
		IDXD_PMD_ERR("%s(): opendir '%s' failed: %s",
				__func__, path, strerror(errno));
		return -1;
	}

	while ((wq = readdir(dev_dir)) != NULL) {
		struct rte_dsa_device *dev;
		int numa_node = SOCKET_ID_ANY;

		if (strncmp(wq->d_name, "wq", 2) != 0)
			continue;
		if (strnlen(wq->d_name, sizeof(dev->wq_name)) == sizeof(dev->wq_name)) {
			IDXD_PMD_ERR("%s(): wq name too long: '%s', skipping",
					__func__, wq->d_name);
			continue;
		}
		IDXD_PMD_DEBUG("%s(): found %s/%s", __func__, path, wq->d_name);

		dev = malloc(sizeof(*dev));
		if (dev == NULL) {
			closedir(dev_dir);
			return -ENOMEM;
		}
		if (dsa_addr_parse(wq->d_name, &dev->addr) < 0) {
			IDXD_PMD_ERR("Error parsing WQ name: %s", wq->d_name);
			free(dev);
			continue;
		}
		dev->device.bus = &dsa_bus.bus;
		strlcpy(dev->wq_name, wq->d_name, sizeof(dev->wq_name));
		TAILQ_INSERT_TAIL(&dsa_bus.device_list, dev, next);
		devcount++;

		read_device_int(dev, "numa_node", &numa_node);
		dev->device.numa_node = numa_node;
		dev->device.name = dev->wq_name;
	}

	closedir(dev_dir);
	return 0;
}

static struct rte_device *
dsa_find_device(const struct rte_device *start, rte_dev_cmp_t cmp,
			 const void *data)
{
	struct rte_dsa_device *dev = TAILQ_FIRST(&dsa_bus.device_list);

	/* the rte_device struct must be at start of dsa structure */
	RTE_BUILD_BUG_ON(offsetof(struct rte_dsa_device, device) != 0);

	if (start != NULL) /* jump to start point if given */
		dev = TAILQ_NEXT((const struct rte_dsa_device *)start, next);
	while (dev != NULL) {
		if (cmp(&dev->device, data) == 0)
			return &dev->device;
		dev = TAILQ_NEXT(dev, next);
	}
	return NULL;
}

static enum rte_iova_mode
dsa_get_iommu_class(void)
{
	/* if there are no devices, report don't care, otherwise VA mode */
	return devcount > 0 ? RTE_IOVA_VA : RTE_IOVA_DC;
}

static int
dsa_addr_parse(const char *name, void *addr)
{
	struct dsa_wq_addr *wq = addr;
	unsigned int device_id, wq_id;

	if (sscanf(name, "wq%u.%u", &device_id, &wq_id) != 2) {
		IDXD_PMD_DEBUG("Parsing WQ name failed: %s", name);
		return -1;
	}

	if (wq != NULL) {
		wq->device_id = device_id;
		wq->wq_id = wq_id;
	}

	return 0;
}

RTE_REGISTER_BUS(dsa, dsa_bus.bus);
