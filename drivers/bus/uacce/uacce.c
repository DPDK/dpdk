/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 HiSilicon Limited
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <eal_export.h>
#include <rte_common.h>
#include <rte_devargs.h>
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_kvargs.h>
#include <bus_driver.h>

#include "bus_uacce_driver.h"

#define UACCE_BUS_CLASS_PATH	"/sys/class/uacce"

/* Support -a uacce:device-name when start DPDK application. */
#define UACCE_DEV_PREFIX	"uacce:"

/* Forward declaration of UACCE bus. */
static struct rte_bus uacce_bus;


extern int uacce_bus_logtype;
#define RTE_LOGTYPE_UACCE_BUS uacce_bus_logtype
#define UACCE_BUS_LOG(level, ...) \
	RTE_LOG_LINE(level, UACCE_BUS, __VA_ARGS__)
#define UACCE_BUS_ERR(fmt, ...) UACCE_BUS_LOG(ERR, fmt, ##__VA_ARGS__)
#define UACCE_BUS_WARN(fmt, ...) UACCE_BUS_LOG(WARNING, fmt, ##__VA_ARGS__)
#define UACCE_BUS_INFO(fmt, ...) UACCE_BUS_LOG(INFO, fmt, ##__VA_ARGS__)
#define UACCE_BUS_DEBUG(fmt, ...) UACCE_BUS_LOG(DEBUG, fmt, ##__VA_ARGS__)


/*
 * Returns the number of bytes read (removed last newline) on success.
 * Otherwise negative value is returned.
 */
static int
uacce_read_attr(const char *dev_root, const char *attr, char *buf, uint32_t sz)
{
	char filename[PATH_MAX] = {0};
	int ret;
	int fd;

	snprintf(filename, sizeof(filename), "%s/%s", dev_root, attr);
	fd = open(filename, O_RDONLY, 0);
	if (fd < 0) {
		UACCE_BUS_ERR("failed to open %s", filename);
		return -EIO;
	}

	ret = read(fd, buf, sz);
	if (ret > 0) {
		/* Remove the last new line character. */
		if (buf[ret - 1] == '\n') {
			buf[ret - 1] = '\0';
			ret--;
		}
	}
	if (ret <= 0) {
		UACCE_BUS_ERR("failed to read %s", filename);
		ret = -EIO;
	}

	close(fd);

	return ret;
}

/* 0 on success. Otherwise negative value is returned. */
static int
uacce_read_attr_int(const char *dev_root, const char *attr, int *val)
{
	char buf[RTE_UACCE_ATTR_MAX_SIZE] = {0};
	char *s = NULL;
	int ret;

	ret = uacce_read_attr(dev_root, attr, buf, sizeof(buf) - 1);
	if (ret < 0)
		return ret;

	*val = strtol(buf, &s, 0);
	if (s[0] != '\0') {
		UACCE_BUS_ERR("read attr %s/%s expect an integer value", dev_root, attr);
		return -EINVAL;
	}

	return 0;
}

/* 0 on success. Otherwise negative value is returned. */
static int
uacce_read_attr_u32(const char *dev_root, const char *attr, uint32_t *val)
{
	char buf[RTE_UACCE_ATTR_MAX_SIZE] = {0};
	char *s = NULL;
	int ret;

	ret = uacce_read_attr(dev_root, attr, buf, sizeof(buf) - 1);
	if (ret < 0)
		return ret;

	*val = strtoul(buf, &s, 0);
	if (s[0] != '\0') {
		UACCE_BUS_ERR("read attr %s/%s expect an uint32 value", dev_root, attr);
		return -EINVAL;
	}

	return 0;
}

static uint32_t
uacce_calc_api_ver(const char *api, int *offset)
{
	int len = strlen(api);
	int end = len - 1;
	unsigned long ver;

	while (end >= 0 && isdigit(api[end]))
		end--;

	if (end <= 0 || end == len - 1 || api[end] != 'v')
		return 0;

	ver = strtoul(api + end + 1, NULL, 10);
	if (ver > UINT32_MAX)
		return 0;

	if (offset != NULL)
		*offset = end + 1;
	return (uint32_t)ver;
}

static int
uacce_read_api(struct rte_uacce_device *dev)
{
#define NOIOMMU_SUFFIX		"_noiommu"
	size_t api_len, sub_len;
	int ret;

	ret = uacce_read_attr(dev->dev_root, "api", dev->api, sizeof(dev->api) - 1);
	if (ret < 0)
		return ret;

	/*
	 * If the device is in no-iommu mode, the UACCE_DEV_FLAG_NOIOMMU flag
	 * in its "flags" will be set, and its "api" will contain the suffix
	 * _noiommu. To facilitate matching with the driver, the suffix is
	 * removed here.
	 */
	api_len = strlen(dev->api);
	sub_len = strlen(NOIOMMU_SUFFIX);
	if (api_len > sub_len && strcmp(dev->api + api_len - sub_len, NOIOMMU_SUFFIX) == 0)
		dev->api[api_len - sub_len] = 0;

	dev->api_ver = uacce_calc_api_ver(dev->api, NULL);

	return 0;
}

static int
uacce_read_algs(struct rte_uacce_device *dev)
{
	int ret = uacce_read_attr(dev->dev_root, "algorithms", dev->algs, sizeof(dev->algs) - 1);
	if (ret < 0)
		return ret;
	return 0;
}

static int
uacce_read_flags(struct rte_uacce_device *dev)
{
	return uacce_read_attr_u32(dev->dev_root, "flags", &dev->flags);
}

static void
uacce_read_numa_node(struct rte_uacce_device *dev)
{
	int ret = uacce_read_attr_int(dev->dev_root, "device/numa_node", &dev->numa_node);
	if (ret != 0) {
		UACCE_BUS_WARN("read attr numa_node failed! set to default");
		dev->numa_node = -1;
	}
}

static int
uacce_read_qfrt_sz(struct rte_uacce_device *dev)
{
	int ret = uacce_read_attr_u32(dev->dev_root, "region_mmio_size",
				      &dev->qfrt_sz[RTE_UACCE_QFRT_MMIO]);
	if (ret != 0)
		return ret;
	return uacce_read_attr_u32(dev->dev_root, "region_dus_size",
				   &dev->qfrt_sz[RTE_UACCE_QFRT_DUS]);
}

static int
uacce_verify(struct rte_uacce_device *dev)
{
	if (!(dev->flags & (UACCE_DEV_FLAG_SVA | UACCE_DEV_FLAG_NOIOMMU))) {
		UACCE_BUS_WARN("device %s don't support SVA or NOIOMMU, skip it!", dev->name);
		return 1; /* >0 will skip this device. */
	}

	return 0;
}

/*
 * Scan one UACCE sysfs entry, and fill the devices list from it.
 * It reads api/algs/flags/numa_node/region-size (please refer Linux kernel:
 * Documentation/ABI/testing/sysfs-driver-uacce) and stores them for later
 * device-driver matching, driver init...
 */
static int
uacce_scan_one(const char *dev_name)
{
	struct rte_uacce_device *dev;
	int ret;

	dev = calloc(1, sizeof(*dev));
	if (!dev)
		return -ENOMEM;

	dev->device.name = dev->name;
	dev->device.devargs = rte_bus_find_devargs(&uacce_bus, dev_name);
	snprintf(dev->name, sizeof(dev->name), "%s", dev_name);
	snprintf(dev->dev_root, sizeof(dev->dev_root), "%s/%s",
		 UACCE_BUS_CLASS_PATH, dev_name);
	snprintf(dev->cdev_path, sizeof(dev->cdev_path), "/dev/%s", dev_name);

	ret = uacce_read_api(dev);
	if (ret != 0)
		goto err;
	ret = uacce_read_algs(dev);
	if (ret != 0)
		goto err;
	ret = uacce_read_flags(dev);
	if (ret != 0)
		goto err;
	uacce_read_numa_node(dev);
	ret = uacce_read_qfrt_sz(dev);
	if (ret != 0)
		goto err;

	ret = uacce_verify(dev);
	if (ret != 0)
		goto err;

	rte_bus_add_device(&uacce_bus, &dev->device);
	return 0;

err:
	free(dev);
	return ret;
}

static int
uacce_scan(void)
{
	struct dirent *e;
	DIR *dir;

	dir = opendir(UACCE_BUS_CLASS_PATH);
	if (dir == NULL) {
		UACCE_BUS_LOG(INFO, "open %s failed!", UACCE_BUS_CLASS_PATH);
		return 0;
	}

	while ((e = readdir(dir)) != NULL) {
		if (e->d_name[0] == '.')
			continue;

		if (strlen(e->d_name) >= RTE_DEV_NAME_MAX_LEN) {
			UACCE_BUS_LOG(WARNING, "uacce device name %s too long, skip it!",
				      e->d_name);
			continue;
		}

		if (rte_bus_device_is_ignored(&uacce_bus, e->d_name))
			continue;

		if (uacce_scan_one(e->d_name) < 0)
			goto error;
	}
	closedir(dir);
	return 0;

error:
	closedir(dir);
	return -1;
}

static bool
uacce_match_api(const struct rte_uacce_device *dev, bool forward_compat,
		const struct rte_uacce_id *id_table)
{
	int dev_ver_off = 0, id_ver_off = 0;
	uint32_t dev_ver, id_ver;

	if (!forward_compat)
		return strcmp(id_table->dev_api, dev->api) == 0;

	dev_ver = uacce_calc_api_ver(dev->api, &dev_ver_off);
	id_ver = uacce_calc_api_ver(id_table->dev_api, &id_ver_off);
	return dev_ver > 0 && id_ver > 0 && dev_ver_off == id_ver_off &&
		strncmp(id_table->dev_api, dev->api, dev_ver_off) == 0 &&
		dev_ver >= id_ver;
}

static bool
uacce_bus_match(const struct rte_driver *drv, const struct rte_device *dev)
{
	const struct rte_uacce_driver *dr = RTE_BUS_DRIVER(drv, *dr);
	const struct rte_uacce_device *uacce_dev = RTE_BUS_DEVICE(dev, *uacce_dev);
	bool forward_compat = !!(dr->drv_flags & RTE_UACCE_DRV_FORWARD_COMPATIBILITY_DEV);
	bool drv_support_noiommu = !!(dr->drv_flags & RTE_UACCE_DRV_SUPPORT_NOIOMMU_MODE);
	bool dev_in_noiommu = !!(uacce_dev->flags & UACCE_DEV_FLAG_NOIOMMU);
	const struct rte_uacce_id *id_table;
	const char *map;
	uint32_t len;

	if (dev_in_noiommu && !drv_support_noiommu)
		return false;

	for (id_table = dr->id_table; id_table->dev_api != NULL; id_table++) {
		if (!uacce_match_api(uacce_dev, forward_compat, id_table))
			continue;

		if (id_table->dev_alg == NULL)
			return true;

		/* The dev->algs's algrothims is separated by new line, for
		 * example: dev->algs could be: aaa\nbbbb\ncc, which has three
		 * algorithms: aaa, bbbb and cc.
		 * The id_table->dev_alg should be a single algrithm, e.g. bbbb.
		 */
		map = strstr(uacce_dev->algs, id_table->dev_alg);
		if (map == NULL)
			continue;
		if (map != uacce_dev->algs && map[-1] != '\n')
			continue;
		len = strlen(id_table->dev_alg);
		if (map[len] != '\0' && map[len] != '\n')
			continue;

		return true;
	}

	return false;
}

static int
uacce_probe_device(struct rte_driver *drv, struct rte_device *dev)
{
	struct rte_uacce_device *uacce_dev = RTE_BUS_DEVICE(dev, *uacce_dev);
	struct rte_uacce_driver *uacce_drv = RTE_BUS_DRIVER(drv, *uacce_drv);
	const char *dev_name = uacce_dev->name;
	int ret;

	UACCE_BUS_DEBUG("probe device %s using driver %s", dev_name, uacce_drv->driver.name);

	ret = uacce_drv->probe(uacce_drv, uacce_dev);
	if (ret != 0) {
		UACCE_BUS_ERR("probe device %s with driver %s failed %d",
			      dev_name, uacce_drv->driver.name, ret);
	} else {
		UACCE_BUS_DEBUG("probe device %s with driver %s success",
				dev_name, uacce_drv->driver.name);
	}

	return ret;
}

static int
uacce_unplug_device(struct rte_device *rte_dev)
{
	const struct rte_uacce_driver *dr = RTE_BUS_DRIVER(rte_dev->driver, *dr);
	struct rte_uacce_device *dev = RTE_BUS_DEVICE(rte_dev, *dev);
	int ret = 0;

	UACCE_BUS_DEBUG("detach device %s using driver: %s", dev->device.name, dr->driver.name);

	if (dr->remove != NULL) {
		ret = dr->remove(dev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int
uacce_cleanup(void)
{
	struct rte_uacce_device *dev;
	int error = 0;

	RTE_BUS_FOREACH_DEV(dev, &uacce_bus) {
		int ret = 0;

		if (rte_dev_is_probed(&dev->device)) {
			ret = uacce_unplug_device(&dev->device);
			if (ret < 0) {
				rte_errno = errno;
				error = -1;
			}
		}

		rte_devargs_remove(dev->device.devargs);
		rte_bus_remove_device(&uacce_bus, &dev->device);
		free(dev);
	}

	return error;
}

static int
uacce_parse(const char *name, void *addr)
{
	const char **out = addr;
	int ret;

	ret = strncmp(name, UACCE_DEV_PREFIX, strlen(UACCE_DEV_PREFIX));

	if (ret == 0 && addr)
		*out = name;

	return ret;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_uacce_avail_queues)
int
rte_uacce_avail_queues(struct rte_uacce_device *dev)
{
	int avails = 0;
	int ret;

	ret = uacce_read_attr_int(dev->dev_root, "available_instances", &avails);
	if (ret == 0)
		ret = avails;

	return ret;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_uacce_queue_alloc)
int
rte_uacce_queue_alloc(struct rte_uacce_device *dev, struct rte_uacce_qcontex *qctx)
{
	memset(qctx, 0, sizeof(*qctx));

	qctx->fd = open(dev->cdev_path, O_RDWR | O_CLOEXEC);
	if (qctx->fd >= 0) {
		qctx->dev = dev;
		return 0;
	}

	return -EIO;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_uacce_queue_free)
void
rte_uacce_queue_free(struct rte_uacce_qcontex *qctx)
{
	if (qctx->fd >= 0)
		close(qctx->fd);
	memset(qctx, 0, sizeof(*qctx));
	qctx->fd = -1;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_uacce_queue_start)
int
rte_uacce_queue_start(struct rte_uacce_qcontex *qctx)
{
#define UACCE_CMD_START_Q	_IO('W', 0)
	return ioctl(qctx->fd, UACCE_CMD_START_Q);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_uacce_queue_ioctl)
int
rte_uacce_queue_ioctl(struct rte_uacce_qcontex *qctx, unsigned long cmd, void *arg)
{
	if (arg == NULL)
		return ioctl(qctx->fd, cmd);

	return ioctl(qctx->fd, cmd, arg);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_uacce_queue_mmap)
void *
rte_uacce_queue_mmap(struct rte_uacce_qcontex *qctx, enum rte_uacce_qfrt qfrt)
{
	size_t size = qctx->dev->qfrt_sz[qfrt];
	off_t off = qfrt * getpagesize();
	void *addr;

	if (size == 0 || qctx->qfrt_base[qfrt] != NULL) {
		UACCE_BUS_ERR("failed to mmap for %s, size is zero or already mmapped!",
			      qctx->dev->name);
		return NULL;
	}

	addr = rte_mem_map(NULL, size, RTE_PROT_READ | RTE_PROT_WRITE, RTE_MAP_SHARED,
			   qctx->fd, off);
	if (addr == NULL) {
		UACCE_BUS_ERR("failed to mmap for %s, qfrt %d err %s!",
			      qctx->dev->name, qfrt, rte_strerror(rte_errno));
		return NULL;
	}
	qctx->qfrt_base[qfrt] = addr;

	return addr;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_uacce_queue_unmap)
void
rte_uacce_queue_unmap(struct rte_uacce_qcontex *qctx, enum rte_uacce_qfrt qfrt)
{
	if (qctx->qfrt_base[qfrt] != NULL) {
		rte_mem_unmap(qctx->qfrt_base[qfrt], qctx->dev->qfrt_sz[qfrt]);
		qctx->qfrt_base[qfrt] = NULL;
	}
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_uacce_register)
void
rte_uacce_register(struct rte_uacce_driver *driver)
{
	rte_bus_add_driver(&uacce_bus, &driver->driver);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_uacce_unregister)
void
rte_uacce_unregister(struct rte_uacce_driver *driver)
{
	rte_bus_remove_driver(&uacce_bus, &driver->driver);
}

static struct rte_bus uacce_bus = {
	.scan = uacce_scan,
	.probe = rte_bus_generic_probe,
	.cleanup = uacce_cleanup,
	.match = uacce_bus_match,
	.probe_device = uacce_probe_device,
	.unplug_device = uacce_unplug_device,
	.find_device = rte_bus_generic_find_device,
	.parse = uacce_parse,
	.dev_iterate = rte_bus_generic_dev_iterate,
};

RTE_REGISTER_BUS(uacce, uacce_bus);
RTE_LOG_REGISTER_DEFAULT(uacce_bus_logtype, NOTICE);
