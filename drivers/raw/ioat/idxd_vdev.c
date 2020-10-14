/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>

#include <rte_memzone.h>
#include <rte_bus_vdev.h>
#include <rte_kvargs.h>
#include <rte_string_fns.h>
#include <rte_rawdev_pmd.h>

#include "ioat_private.h"

/** Name of the device driver */
#define IDXD_PMD_RAWDEV_NAME rawdev_idxd
/* takes a work queue(WQ) as parameter */
#define IDXD_ARG_WQ		"wq"

static const char * const valid_args[] = {
	IDXD_ARG_WQ,
	NULL
};

struct idxd_vdev_args {
	uint8_t device_id;
	uint8_t wq_id;
};

static const struct rte_rawdev_ops idxd_vdev_ops = {
		.dev_close = idxd_rawdev_close,
		.dev_selftest = ioat_rawdev_test,
		.dump = idxd_dev_dump,
		.dev_configure = idxd_dev_configure,
		.dev_info_get = idxd_dev_info_get,
		.xstats_get = ioat_xstats_get,
		.xstats_get_names = ioat_xstats_get_names,
		.xstats_reset = ioat_xstats_reset,
};

static void *
idxd_vdev_mmap_wq(struct idxd_vdev_args *args)
{
	void *addr;
	char path[PATH_MAX];
	int fd;

	snprintf(path, sizeof(path), "/dev/dsa/wq%u.%u",
			args->device_id, args->wq_id);
	fd = open(path, O_RDWR);
	if (fd < 0) {
		IOAT_PMD_ERR("Failed to open device path");
		return NULL;
	}

	addr = mmap(NULL, 0x1000, PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (addr == MAP_FAILED) {
		IOAT_PMD_ERR("Failed to mmap device");
		return NULL;
	}

	return addr;
}

static int
idxd_rawdev_parse_wq(const char *key __rte_unused, const char *value,
			  void *extra_args)
{
	struct idxd_vdev_args *args = (struct idxd_vdev_args *)extra_args;
	int dev, wq, bytes = -1;
	int read = sscanf(value, "%d.%d%n", &dev, &wq, &bytes);

	if (read != 2 || bytes != (int)strlen(value)) {
		IOAT_PMD_ERR("Error parsing work-queue id. Must be in <dev_id>.<queue_id> format");
		return -EINVAL;
	}

	if (dev >= UINT8_MAX || wq >= UINT8_MAX) {
		IOAT_PMD_ERR("Device or work queue id out of range");
		return -EINVAL;
	}

	args->device_id = dev;
	args->wq_id = wq;

	return 0;
}

static int
idxd_vdev_parse_params(struct rte_kvargs *kvlist, struct idxd_vdev_args *args)
{
	int ret = 0;

	if (rte_kvargs_count(kvlist, IDXD_ARG_WQ) == 1) {
		if (rte_kvargs_process(kvlist, IDXD_ARG_WQ,
				&idxd_rawdev_parse_wq, args) < 0) {
			IOAT_PMD_ERR("Error parsing %s", IDXD_ARG_WQ);
			ret = -EINVAL;
		}
	} else {
		IOAT_PMD_ERR("%s is a mandatory arg", IDXD_ARG_WQ);
		ret = -EINVAL;
	}

	rte_kvargs_free(kvlist);
	return ret;
}

static int
idxd_vdev_get_max_batches(struct idxd_vdev_args *args)
{
	char sysfs_path[PATH_MAX];
	FILE *f;
	int ret;

	snprintf(sysfs_path, sizeof(sysfs_path),
			"/sys/bus/dsa/devices/wq%u.%u/size",
			args->device_id, args->wq_id);
	f = fopen(sysfs_path, "r");
	if (f == NULL)
		return -1;

	if (fscanf(f, "%d", &ret) != 1)
		ret = -1;

	fclose(f);
	return ret;
}

static int
idxd_rawdev_probe_vdev(struct rte_vdev_device *vdev)
{
	struct rte_kvargs *kvlist;
	struct idxd_rawdev idxd = {{0}}; /* double {} to avoid error on BSD12 */
	struct idxd_vdev_args vdev_args;
	const char *name;
	int ret = 0;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	IOAT_PMD_INFO("Initializing pmd_idxd for %s", name);

	kvlist = rte_kvargs_parse(rte_vdev_device_args(vdev), valid_args);
	if (kvlist == NULL) {
		IOAT_PMD_ERR("Invalid kvargs key");
		return -EINVAL;
	}

	ret = idxd_vdev_parse_params(kvlist, &vdev_args);
	if (ret) {
		IOAT_PMD_ERR("Failed to parse kvargs");
		return -EINVAL;
	}

	idxd.qid = vdev_args.wq_id;
	idxd.u.vdev.dsa_id = vdev_args.device_id;
	idxd.max_batches = idxd_vdev_get_max_batches(&vdev_args);

	idxd.public.portal = idxd_vdev_mmap_wq(&vdev_args);
	if (idxd.public.portal == NULL) {
		IOAT_PMD_ERR("WQ mmap failed");
		return -ENOENT;
	}

	ret = idxd_rawdev_create(name, &vdev->device, &idxd, &idxd_vdev_ops);
	if (ret) {
		IOAT_PMD_ERR("Failed to create rawdev %s", name);
		return ret;
	}

	return 0;
}

static int
idxd_rawdev_remove_vdev(struct rte_vdev_device *vdev)
{
	struct idxd_rawdev *idxd;
	const char *name;
	struct rte_rawdev *rdev;
	int ret = 0;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	IOAT_PMD_INFO("Remove DSA vdev %p", name);

	rdev = rte_rawdev_pmd_get_named_dev(name);
	if (!rdev) {
		IOAT_PMD_ERR("Invalid device name (%s)", name);
		return -EINVAL;
	}

	idxd = rdev->dev_private;

	/* free context and memory */
	if (rdev->dev_private != NULL) {
		IOAT_PMD_DEBUG("Freeing device driver memory");
		rdev->dev_private = NULL;

		if (munmap(idxd->public.portal, 0x1000) < 0) {
			IOAT_PMD_ERR("Error unmapping portal");
			ret = -errno;
		}

		rte_free(idxd->public.batch_ring);
		rte_free(idxd->public.hdl_ring);

		rte_memzone_free(idxd->mz);
	}

	if (rte_rawdev_pmd_release(rdev))
		IOAT_PMD_ERR("Device cleanup failed");

	return ret;
}

struct rte_vdev_driver idxd_rawdev_drv_vdev = {
	.probe = idxd_rawdev_probe_vdev,
	.remove = idxd_rawdev_remove_vdev,
};

RTE_PMD_REGISTER_VDEV(IDXD_PMD_RAWDEV_NAME, idxd_rawdev_drv_vdev);
RTE_PMD_REGISTER_PARAM_STRING(IDXD_PMD_RAWDEV_NAME,
			      "wq=<string>");
