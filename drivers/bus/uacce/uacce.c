/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 HiSilicon Limited
 */

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
#include <rte_bitops.h>
#include <rte_common.h>
#include <rte_devargs.h>
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_kvargs.h>
#include <bus_driver.h>

#include "bus_uacce_driver.h"

#define UACCE_BUS_CLASS_PATH	"/sys/class/uacce"

/* UACCE device flag of SVA. */
#define UACCE_DEV_FLGA_SVA	RTE_BIT32(0)

/* Support -a uacce:device-name when start DPDK application. */
#define UACCE_DEV_PREFIX	"uacce:"

/*
 * Structure describing the UACCE bus.
 */
struct rte_uacce_bus {
	struct rte_bus bus;		            /* Inherit the generic class. */
	TAILQ_HEAD(, rte_uacce_device) device_list; /* List of devices. */
	TAILQ_HEAD(, rte_uacce_driver) driver_list; /* List of drivers. */
};

/* Forward declaration of UACCE bus. */
static struct rte_uacce_bus uacce_bus;

enum uacce_params {
	RTE_UACCE_PARAM_NAME,
};

static const char *const uacce_params_keys[] = {
	[RTE_UACCE_PARAM_NAME] = "name",
	NULL,
};

#define FOREACH_DEVICE_ON_UACCEBUS(p)	\
		RTE_TAILQ_FOREACH(p, &uacce_bus.device_list, next)
#define FOREACH_DRIVER_ON_UACCEBUS(p)	\
		RTE_TAILQ_FOREACH(p, &uacce_bus.driver_list, next)

extern int uacce_bus_logtype;
#define RTE_LOGTYPE_UACCE_BUS uacce_bus_logtype
#define UACCE_BUS_LOG(level, ...) \
	RTE_LOG_LINE(level, UACCE_BUS, __VA_ARGS__)
#define UACCE_BUS_ERR(fmt, ...) UACCE_BUS_LOG(ERR, fmt, ##__VA_ARGS__)
#define UACCE_BUS_WARN(fmt, ...) UACCE_BUS_LOG(WARNING, fmt, ##__VA_ARGS__)
#define UACCE_BUS_INFO(fmt, ...) UACCE_BUS_LOG(INFO, fmt, ##__VA_ARGS__)
#define UACCE_BUS_DEBUG(fmt, ...) UACCE_BUS_LOG(DEBUG, fmt, ##__VA_ARGS__)


static struct rte_devargs *
uacce_devargs_lookup(const char *dev_name)
{
	char name[RTE_UACCE_DEV_PATH_SIZE] = {0};
	struct rte_devargs *devargs;

	snprintf(name, sizeof(name), "%s%s", UACCE_DEV_PREFIX, dev_name);
	RTE_EAL_DEVARGS_FOREACH("uacce", devargs) {
		if (strcmp(devargs->name, name) == 0)
			return devargs;
	}

	return NULL;
}

static bool
uacce_ignore_device(const char *dev_name)
{
	struct rte_devargs *devargs = uacce_devargs_lookup(dev_name);

	switch (uacce_bus.bus.conf.scan_mode) {
	case RTE_BUS_SCAN_ALLOWLIST:
		if (devargs && devargs->policy == RTE_DEV_ALLOWED)
			return false;
		break;
	case RTE_BUS_SCAN_UNDEFINED:
	case RTE_BUS_SCAN_BLOCKLIST:
		if (devargs == NULL || devargs->policy != RTE_DEV_BLOCKED)
			return false;
		break;
	}

	return true;
}

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

static int
uacce_read_api(struct rte_uacce_device *dev)
{
	int ret = uacce_read_attr(dev->dev_root, "api", dev->api, sizeof(dev->api) - 1);
	if (ret < 0)
		return ret;
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
	if (!(dev->flags & UACCE_DEV_FLGA_SVA)) {
		UACCE_BUS_WARN("device %s don't support SVA, skip it!", dev->name);
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

	dev->device.bus = &uacce_bus.bus;
	dev->device.name = dev->name;
	dev->device.devargs = uacce_devargs_lookup(dev_name);
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

	TAILQ_INSERT_TAIL(&uacce_bus.device_list, dev, next);
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

		if (uacce_ignore_device(e->d_name))
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
uacce_match(const struct rte_uacce_driver *dr, const struct rte_uacce_device *dev)
{
	const struct rte_uacce_id *id_table;
	uint32_t len;
	char *map;

	for (id_table = dr->id_table; id_table->dev_api != NULL; id_table++) {
		if (strcmp(id_table->dev_api, dev->api) != 0)
			continue;

		if (id_table->dev_alg == NULL)
			return true;

		/* The dev->algs's algrothims is separated by new line, for
		 * example: dev->algs could be: aaa\nbbbb\ncc, which has three
		 * algorithms: aaa, bbbb and cc.
		 * The id_table->dev_alg should be a single algrithm, e.g. bbbb.
		 */
		map = strstr(dev->algs, id_table->dev_alg);
		if (map == NULL)
			continue;
		if (map != dev->algs && map[-1] != '\n')
			continue;
		len = strlen(id_table->dev_alg);
		if (map[len] != '\0' && map[len] != '\n')
			continue;

		return true;
	}

	return false;
}

static int
uacce_probe_one_driver(struct rte_uacce_driver *dr, struct rte_uacce_device *dev)
{
	const char *dev_name = dev->name;
	bool already_probed;
	int ret;

	if (!uacce_match(dr, dev))
		/* Match of device and driver failed */
		return 1;

	already_probed = rte_dev_is_probed(&dev->device);
	if (already_probed) {
		UACCE_BUS_INFO("device %s is already probed", dev_name);
		return -EEXIST;
	}

	UACCE_BUS_DEBUG("probe device %s using driver %s", dev_name, dr->driver.name);

	ret = dr->probe(dr, dev);
	if (ret != 0) {
		UACCE_BUS_ERR("probe device %s with driver %s failed %d",
			      dev_name, dr->driver.name, ret);
	} else {
		dev->device.driver = &dr->driver;
		dev->driver = dr;
		UACCE_BUS_DEBUG("probe device %s with driver %s success",
				dev_name, dr->driver.name);
	}

	return ret;
}

static int
uacce_probe_all_drivers(struct rte_uacce_device *dev)
{
	struct rte_uacce_driver *dr;
	int rc;

	FOREACH_DRIVER_ON_UACCEBUS(dr) {
		rc = uacce_probe_one_driver(dr, dev);
		if (rc < 0)
			/* negative value is an error */
			return rc;
		if (rc > 0)
			/* positive value means driver doesn't support it */
			continue;
		return 0;
	}

	return 1;
}

static int
uacce_probe(void)
{
	size_t probed = 0, failed = 0;
	struct rte_uacce_device *dev;
	int ret;

	FOREACH_DEVICE_ON_UACCEBUS(dev) {
		probed++;

		ret = uacce_probe_all_drivers(dev);
		if (ret < 0) {
			UACCE_BUS_LOG(ERR, "Requested device %s cannot be used",
				dev->name);
			rte_errno = errno;
			failed++;
		}
	}

	return (probed && probed == failed) ? -1 : 0;
}

static int
uacce_cleanup(void)
{
	struct rte_uacce_device *dev, *tmp_dev;
	int error = 0;

	RTE_TAILQ_FOREACH_SAFE(dev, &uacce_bus.device_list, next, tmp_dev) {
		struct rte_uacce_driver *dr = dev->driver;
		int ret = 0;

		if (dr == NULL || dr->remove == NULL)
			goto free;

		ret = dr->remove(dev);
		if (ret < 0) {
			rte_errno = errno;
			error = -1;
		}
		dev->driver = NULL;
		dev->device.driver = NULL;

free:
		TAILQ_REMOVE(&uacce_bus.device_list, dev, next);
		memset(dev, 0, sizeof(*dev));
		free(dev);
	}

	return error;
}

static int
uacce_plug(struct rte_device *dev)
{
	return uacce_probe_all_drivers(RTE_DEV_TO_UACCE_DEV(dev));
}

static int
uacce_detach_dev(struct rte_uacce_device *dev)
{
	struct rte_uacce_driver *dr;
	int ret = 0;

	dr = dev->driver;

	UACCE_BUS_DEBUG("detach device %s using driver: %s", dev->device.name, dr->driver.name);

	if (dr->remove) {
		ret = dr->remove(dev);
		if (ret < 0)
			return ret;
	}

	dev->driver = NULL;
	dev->device.driver = NULL;

	return 0;
}

static int
uacce_unplug(struct rte_device *dev)
{
	struct rte_uacce_device *uacce_dev;
	int ret;

	uacce_dev = RTE_DEV_TO_UACCE_DEV(dev);
	ret = uacce_detach_dev(uacce_dev);
	if (ret == 0) {
		TAILQ_REMOVE(&uacce_bus.device_list, uacce_dev, next);
		rte_devargs_remove(dev->devargs);
		free(uacce_dev);
	}

	return ret;
}

static struct rte_device *
uacce_find_device(const struct rte_device *start, rte_dev_cmp_t cmp,  const void *data)
{
	const struct rte_uacce_device *uacce_start;
	struct rte_uacce_device *uacce_dev;

	if (start != NULL) {
		uacce_start = RTE_DEV_TO_UACCE_DEV_CONST(start);
		uacce_dev = TAILQ_NEXT(uacce_start, next);
	} else {
		uacce_dev = TAILQ_FIRST(&uacce_bus.device_list);
	}

	while (uacce_dev != NULL) {
		if (cmp(&uacce_dev->device, data) == 0)
			return &uacce_dev->device;
		uacce_dev = TAILQ_NEXT(uacce_dev, next);
	}

	return NULL;
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

static int
uacce_dev_match(const struct rte_device *dev, const void *_kvlist)
{
	const char *key = uacce_params_keys[RTE_UACCE_PARAM_NAME];
	const struct rte_kvargs *kvlist = _kvlist;
	const char *name;

	/* no kvlist arg, all devices match. */
	if (kvlist == NULL)
		return 0;

	/* if key is present in kvlist and does not match, filter device. */
	name = rte_kvargs_get(kvlist, key);
	if (name != NULL && strcmp(name, dev->name))
		return -1;

	return 0;
}

static void *
uacce_dev_iterate(const void *start, const char *str,
		  const struct rte_dev_iterator *it __rte_unused)
{
	rte_bus_find_device_t find_device;
	struct rte_kvargs *kvargs = NULL;
	struct rte_device *dev;

	if (str != NULL) {
		kvargs = rte_kvargs_parse(str, uacce_params_keys);
		if (kvargs == NULL) {
			UACCE_BUS_ERR("cannot parse argument list %s", str);
			return NULL;
		}
	}
	find_device = uacce_bus.bus.find_device;
	dev = find_device(start, uacce_dev_match, kvargs);
	rte_kvargs_free(kvargs);
	return dev;
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
	TAILQ_INSERT_TAIL(&uacce_bus.driver_list, driver, next);
	driver->bus = &uacce_bus;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_uacce_unregister)
void
rte_uacce_unregister(struct rte_uacce_driver *driver)
{
	TAILQ_REMOVE(&uacce_bus.driver_list, driver, next);
	driver->bus = NULL;
}

static struct rte_uacce_bus uacce_bus = {
	.bus = {
		.scan = uacce_scan,
		.probe = uacce_probe,
		.cleanup = uacce_cleanup,
		.plug = uacce_plug,
		.unplug = uacce_unplug,
		.find_device = uacce_find_device,
		.parse = uacce_parse,
		.dev_iterate = uacce_dev_iterate,
	},
	.device_list = TAILQ_HEAD_INITIALIZER(uacce_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(uacce_bus.driver_list),
};

RTE_REGISTER_BUS(uacce, uacce_bus.bus);
RTE_LOG_REGISTER_DEFAULT(uacce_bus_logtype, NOTICE);
