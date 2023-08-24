/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <unistd.h>

#include <bus_driver.h>
#include <bus_platform_driver.h>
#include <eal_filesystem.h>
#include <rte_bus.h>
#include <rte_devargs.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_string_fns.h>
#include <rte_vfio.h>

#include "private.h"

#ifdef VFIO_PRESENT

#define PLATFORM_BUS_DEVICES_PATH "/sys/bus/platform/devices"

void
rte_platform_register(struct rte_platform_driver *pdrv)
{
	TAILQ_INSERT_TAIL(&platform_bus.driver_list, pdrv, next);
}

void
rte_platform_unregister(struct rte_platform_driver *pdrv)
{
	TAILQ_REMOVE(&platform_bus.driver_list, pdrv, next);
}

static struct rte_devargs *
dev_devargs(const char *dev_name)
{
	struct rte_devargs *devargs;

	RTE_EAL_DEVARGS_FOREACH("platform", devargs) {
		if (!strcmp(devargs->name, dev_name))
			return devargs;
	}

	return NULL;
}

static bool
dev_allowed(const char *dev_name)
{
	struct rte_devargs *devargs;

	devargs = dev_devargs(dev_name);
	if (devargs == NULL)
		return true;

	switch (platform_bus.bus.conf.scan_mode) {
	case RTE_BUS_SCAN_UNDEFINED:
	case RTE_BUS_SCAN_ALLOWLIST:
		if (devargs->policy == RTE_DEV_ALLOWED)
			return true;
		break;
	case RTE_BUS_SCAN_BLOCKLIST:
		if (devargs->policy == RTE_DEV_BLOCKED)
			return false;
		break;
	}

	return true;
}

static int
dev_add(const char *dev_name)
{
	struct rte_platform_device *pdev, *tmp;
	char path[PATH_MAX];
	unsigned long val;

	pdev = calloc(1, sizeof(*pdev));
	if (pdev == NULL)
		return -ENOMEM;

	rte_strscpy(pdev->name, dev_name, sizeof(pdev->name));
	pdev->device.name = pdev->name;
	pdev->device.devargs = dev_devargs(dev_name);
	pdev->device.bus = &platform_bus.bus;
	snprintf(path, sizeof(path), PLATFORM_BUS_DEVICES_PATH "/%s/numa_node", dev_name);
	pdev->device.numa_node = eal_parse_sysfs_value(path, &val) ? rte_socket_id() : val;

	FOREACH_DEVICE_ON_PLATFORM_BUS(tmp) {
		if (!strcmp(tmp->name, pdev->name)) {
			PLATFORM_LOG(INFO, "device %s already added\n", pdev->name);

			if (tmp->device.devargs != pdev->device.devargs)
				rte_devargs_remove(pdev->device.devargs);

			free(pdev);
			return -EEXIST;
		}
	}

	TAILQ_INSERT_HEAD(&platform_bus.device_list, pdev, next);

	PLATFORM_LOG(INFO, "adding device %s to the list\n", dev_name);

	return 0;
}

static char *
dev_kernel_driver_name(const char *dev_name)
{
	char path[PATH_MAX], buf[BUFSIZ] = { };
	char *kdrv;
	int ret;

	snprintf(path, sizeof(path), PLATFORM_BUS_DEVICES_PATH "/%s/driver", dev_name);
	/* save space for NUL */
	ret = readlink(path, buf, sizeof(buf) - 1);
	if (ret <= 0)
		return NULL;

	/* last token is kernel driver name */
	kdrv = strrchr(buf, '/');
	if (kdrv != NULL)
		return strdup(kdrv + 1);

	return NULL;
}

static bool
dev_is_bound_vfio_platform(const char *dev_name)
{
	char *kdrv;
	int ret;

	kdrv = dev_kernel_driver_name(dev_name);
	if (!kdrv)
		return false;

	ret = strcmp(kdrv, "vfio-platform");
	free(kdrv);

	return ret == 0;
}

static int
platform_bus_scan(void)
{
	const struct dirent *ent;
	const char *dev_name;
	int ret = 0;
	DIR *dp;

	dp = opendir(PLATFORM_BUS_DEVICES_PATH);
	if (dp == NULL) {
		PLATFORM_LOG(INFO, "failed to open %s\n", PLATFORM_BUS_DEVICES_PATH);
		return -errno;
	}

	while ((ent = readdir(dp))) {
		dev_name = ent->d_name;
		if (dev_name[0] == '.')
			continue;

		if (!dev_allowed(dev_name))
			continue;

		if (!dev_is_bound_vfio_platform(dev_name))
			continue;

		ret = dev_add(dev_name);
		if (ret)
			break;
	}

	closedir(dp);

	return ret;
}

static int
device_map_resource_offset(struct rte_platform_device *pdev, struct rte_platform_resource *res,
			   size_t offset)
{
	res->mem.addr = mmap(NULL, res->mem.len, PROT_READ | PROT_WRITE, MAP_SHARED, pdev->dev_fd,
			     offset);
	if (res->mem.addr == MAP_FAILED)
		return -errno;

	PLATFORM_LOG(DEBUG, "adding resource va = %p len = %"PRIu64" name = %s\n", res->mem.addr,
		     res->mem.len, res->name);

	return 0;
}

static void
device_unmap_resources(struct rte_platform_device *pdev)
{
	struct rte_platform_resource *res;
	unsigned int i;

	for (i = 0; i < pdev->num_resource; i++) {
		res = &pdev->resource[i];
		munmap(res->mem.addr, res->mem.len);
		free(res->name);
	}

	free(pdev->resource);
	pdev->resource = NULL;
	pdev->num_resource = 0;
}

static int
read_sysfs_string(const char *path, char *buf, size_t size)
{
	FILE *f;
	char *p;

	f = fopen(path, "r");
	if (f == NULL)
		return -errno;

	if (fgets(buf, size, f) == NULL) {
		fclose(f);
		return -ENODATA;
	}

	fclose(f);

	p = strrchr(buf, '\n');
	if (p != NULL)
		*p = '\0';

	return 0;
}

static char *
of_resource_name(const char *dev_name, int index)
{
	char path[PATH_MAX], buf[BUFSIZ] = { };
	int num = 0, ret;
	char *name;

	snprintf(path, sizeof(path), PLATFORM_BUS_DEVICES_PATH "/%s/of_node/reg-names", dev_name);
	ret = read_sysfs_string(path, buf, sizeof(buf) - 1);
	if (ret)
		return NULL;

	for (name = buf; *name != 0; name += strlen(name) + 1) {
		if (num++ != index)
			continue;
		return strdup(name);
	}

	return NULL;
}

static int
device_map_resources(struct rte_platform_device *pdev, unsigned int num)
{
	struct rte_platform_resource *res;
	unsigned int i;
	int ret;

	if (num == 0) {
		PLATFORM_LOG(WARNING, "device %s has no resources\n", pdev->name);
		return 0;
	}

	pdev->resource = calloc(num, sizeof(*pdev->resource));
	if (pdev->resource == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		struct vfio_region_info reg_info = {
			.argsz = sizeof(reg_info),
			.index = i,
		};

		ret = ioctl(pdev->dev_fd, VFIO_DEVICE_GET_REGION_INFO, &reg_info);
		if (ret) {
			PLATFORM_LOG(ERR, "failed to get region info at %d\n", i);
			ret = -errno;
			goto out;
		}

		res = &pdev->resource[i];
		res->name = of_resource_name(pdev->name, reg_info.index);
		res->mem.len = reg_info.size;
		ret = device_map_resource_offset(pdev, res, reg_info.offset);
		if (ret) {
			PLATFORM_LOG(ERR, "failed to ioremap resource at %d\n", i);
			goto out;
		}

		pdev->num_resource++;
	}

	return 0;
out:
	device_unmap_resources(pdev);

	return ret;
}

static void
device_cleanup(struct rte_platform_device *pdev)
{
	device_unmap_resources(pdev);
	rte_vfio_release_device(PLATFORM_BUS_DEVICES_PATH, pdev->name, pdev->dev_fd);
}

static int
device_setup(struct rte_platform_device *pdev)
{
	struct vfio_device_info dev_info = { .argsz = sizeof(dev_info), };
	const char *name = pdev->name;
	int ret;

	ret = rte_vfio_setup_device(PLATFORM_BUS_DEVICES_PATH, name, &pdev->dev_fd, &dev_info);
	if (ret) {
		PLATFORM_LOG(ERR, "failed to setup %s\n", name);
		return -ENODEV;
	}

	/* This is an extra check to confirm that platform device was initialized
	 * by a kernel vfio-platform driver. On kernels that predate vfio-platform
	 * driver this flag obviously does not exist. In such scenarios this
	 * check needs to be removed otherwise compilation fails.
	 *
	 * Now, on such old kernels code will never reach here because
	 * there is another check much earlier which verifies whether
	 * device has been bound to vfio-platform driver.
	 */
#ifdef VFIO_DEVICE_FLAGS_PLATFORM
	if (!(dev_info.flags & VFIO_DEVICE_FLAGS_PLATFORM)) {
		PLATFORM_LOG(ERR, "device not backed by vfio-platform\n");
		ret = -ENOTSUP;
		goto out;
	}
#endif

	ret = device_map_resources(pdev, dev_info.num_regions);
	if (ret) {
		PLATFORM_LOG(ERR, "failed to setup platform resources\n");
		goto out;
	}

	return 0;
out:
	device_cleanup(pdev);

	return ret;
}

static int
driver_call_probe(struct rte_platform_driver *pdrv, struct rte_platform_device *pdev)
{
	int ret;

	if (rte_dev_is_probed(&pdev->device))
		return -EBUSY;

	if (pdrv->probe != NULL) {
		pdev->driver = pdrv;
		ret = pdrv->probe(pdev);
		if (ret)
			return ret;
	}

	pdev->device.driver = &pdrv->driver;

	return 0;
}

static int
driver_probe_device(struct rte_platform_driver *pdrv, struct rte_platform_device *pdev)
{
	enum rte_iova_mode iova_mode;
	int ret;

	iova_mode = rte_eal_iova_mode();
	if (pdrv->drv_flags & RTE_PLATFORM_DRV_NEED_IOVA_AS_VA && iova_mode != RTE_IOVA_VA) {
		PLATFORM_LOG(ERR, "driver %s expects VA IOVA mode but current mode is PA\n",
			     pdrv->driver.name);
		return -EINVAL;
	}

	ret = device_setup(pdev);
	if (ret)
		return ret;

	ret = driver_call_probe(pdrv, pdev);
	if (ret)
		device_cleanup(pdev);

	return ret;
}

static bool
driver_match_device(struct rte_platform_driver *pdrv, struct rte_platform_device *pdev)
{
	bool match = false;
	char *kdrv;

	kdrv = dev_kernel_driver_name(pdev->name);
	if (!kdrv)
		return false;

	/* match by driver name */
	if (!strcmp(kdrv, pdrv->driver.name)) {
		match = true;
		goto out;
	}

	/* match by driver alias */
	if (pdrv->driver.alias != NULL && !strcmp(kdrv, pdrv->driver.alias)) {
		match = true;
		goto out;
	}

	/* match by device name */
	if (!strcmp(pdev->name, pdrv->driver.name))
		match = true;

out:
	free(kdrv);

	return match;
}

static int
device_attach(struct rte_platform_device *pdev)
{
	struct rte_platform_driver *pdrv;

	FOREACH_DRIVER_ON_PLATFORM_BUS(pdrv) {
		if (driver_match_device(pdrv, pdev))
			break;
	}

	if (pdrv == NULL)
		return -ENODEV;

	return driver_probe_device(pdrv, pdev);
}

static int
platform_bus_probe(void)
{
	struct rte_platform_device *pdev;
	int ret;

	FOREACH_DEVICE_ON_PLATFORM_BUS(pdev) {
		ret = device_attach(pdev);
		if (ret == -EBUSY) {
			PLATFORM_LOG(DEBUG, "device %s already probed\n", pdev->name);
			continue;
		}
		if (ret)
			PLATFORM_LOG(ERR, "failed to probe %s\n", pdev->name);
	}

	return 0;
}

static struct rte_device *
platform_bus_find_device(const struct rte_device *start, rte_dev_cmp_t cmp, const void *data)
{
	struct rte_platform_device *pdev;

	pdev = start ? RTE_TAILQ_NEXT(RTE_DEV_TO_PLATFORM_DEV_CONST(start), next) :
		       RTE_TAILQ_FIRST(&platform_bus.device_list);
	while (pdev) {
		if (cmp(&pdev->device, data) == 0)
			return &pdev->device;

		pdev = RTE_TAILQ_NEXT(pdev, next);
	}

	return NULL;
}

static int
platform_bus_plug(struct rte_device *dev)
{
	struct rte_platform_device *pdev;

	if (!dev_allowed(dev->name))
		return -EPERM;

	if (!dev_is_bound_vfio_platform(dev->name))
		return -EPERM;

	pdev = RTE_DEV_TO_PLATFORM_DEV(dev);
	if (pdev == NULL)
		return -EINVAL;

	return device_attach(pdev);
}

static void
device_release_driver(struct rte_platform_device *pdev)
{
	struct rte_platform_driver *pdrv;
	int ret;

	pdrv = pdev->driver;
	if (pdrv != NULL && pdrv->remove != NULL) {
		ret = pdrv->remove(pdev);
		if (ret)
			PLATFORM_LOG(WARNING, "failed to remove %s\n", pdev->name);
	}

	pdev->device.driver = NULL;
	pdev->driver = NULL;
}

static int
platform_bus_unplug(struct rte_device *dev)
{
	struct rte_platform_device *pdev;

	pdev = RTE_DEV_TO_PLATFORM_DEV(dev);
	if (pdev == NULL)
		return -EINVAL;

	device_release_driver(pdev);
	device_cleanup(pdev);
	rte_devargs_remove(pdev->device.devargs);
	free(pdev);

	return 0;
}

static int
platform_bus_parse(const char *name, void *addr)
{
	struct rte_platform_device pdev = { };
	struct rte_platform_driver *pdrv;
	const char **out = addr;

	rte_strscpy(pdev.name, name, sizeof(pdev.name));

	FOREACH_DRIVER_ON_PLATFORM_BUS(pdrv) {
		if (driver_match_device(pdrv, &pdev))
			break;
	}

	if (pdrv != NULL && addr != NULL)
		*out = name;

	return pdrv != NULL ? 0 : -ENODEV;
}

static int
platform_bus_dma_map(struct rte_device *dev, void *addr, uint64_t iova, size_t len)
{
	struct rte_platform_device *pdev;

	pdev = RTE_DEV_TO_PLATFORM_DEV(dev);
	if (pdev == NULL || pdev->driver == NULL) {
		rte_errno = EINVAL;
		return -1;
	}

	if (pdev->driver->dma_map != NULL)
		return pdev->driver->dma_map(pdev, addr, iova, len);

	return rte_vfio_container_dma_map(RTE_VFIO_DEFAULT_CONTAINER_FD, (uint64_t)addr, iova, len);
}

static int
platform_bus_dma_unmap(struct rte_device *dev, void *addr, uint64_t iova, size_t len)
{
	struct rte_platform_device *pdev;

	pdev = RTE_DEV_TO_PLATFORM_DEV(dev);
	if (pdev == NULL || pdev->driver == NULL) {
		rte_errno = EINVAL;
		return -1;
	}

	if (pdev->driver->dma_unmap != NULL)
		return pdev->driver->dma_unmap(pdev, addr, iova, len);

	return rte_vfio_container_dma_unmap(RTE_VFIO_DEFAULT_CONTAINER_FD, (uint64_t)addr, iova,
					    len);
}

static enum rte_iova_mode
platform_bus_get_iommu_class(void)
{
	struct rte_platform_driver *pdrv;
	struct rte_platform_device *pdev;

	FOREACH_DEVICE_ON_PLATFORM_BUS(pdev) {
		pdrv = pdev->driver;
		if (pdrv != NULL && pdrv->drv_flags & RTE_PLATFORM_DRV_NEED_IOVA_AS_VA)
			return RTE_IOVA_VA;
	}

	return RTE_IOVA_DC;
}

static int
platform_bus_cleanup(void)
{
	struct rte_platform_device *pdev, *tmp;

	RTE_TAILQ_FOREACH_SAFE(pdev, &platform_bus.device_list, next, tmp) {
		TAILQ_REMOVE(&platform_bus.device_list, pdev, next);
		platform_bus_unplug(&pdev->device);
	}

	return 0;
}

struct rte_platform_bus platform_bus = {
	.bus = {
		.scan = platform_bus_scan,
		.probe = platform_bus_probe,
		.find_device = platform_bus_find_device,
		.plug = platform_bus_plug,
		.unplug = platform_bus_unplug,
		.parse = platform_bus_parse,
		.dma_map = platform_bus_dma_map,
		.dma_unmap = platform_bus_dma_unmap,
		.get_iommu_class = platform_bus_get_iommu_class,
		.dev_iterate = platform_bus_dev_iterate,
		.cleanup = platform_bus_cleanup,
	},
	.device_list = TAILQ_HEAD_INITIALIZER(platform_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(platform_bus.driver_list),
};

RTE_REGISTER_BUS(platform, platform_bus.bus);
RTE_LOG_REGISTER_DEFAULT(platform_bus_logtype, NOTICE);

#endif /* VFIO_PRESENT */
