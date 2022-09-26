/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies Ltd
 */

#include <stdlib.h>
#include <dirent.h>
#include <libgen.h>

#include <rte_malloc.h>
#include <rte_errno.h>
#include <bus_auxiliary_driver.h>
#include <rte_common.h>
#include "eal_filesystem.h"

#include "mlx5_common_utils.h"
#include "mlx5_common_private.h"

#define AUXILIARY_SYSFS_PATH "/sys/bus/auxiliary/devices"
#define MLX5_AUXILIARY_PREFIX "mlx5_core.sf."

int
mlx5_auxiliary_get_child_name(const char *dev, const char *node,
			      char *child, size_t size)
{
	DIR *dir;
	struct dirent *dent;
	MKSTR(path, "%s/%s%s", AUXILIARY_SYSFS_PATH, dev, node);

	dir = opendir(path);
	if (dir == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	/* Get the first file name. */
	while ((dent = readdir(dir)) != NULL) {
		if (dent->d_name[0] != '.')
			break;
	}
	closedir(dir);
	if (dent == NULL) {
		rte_errno = ENOENT;
		return -rte_errno;
	}
	if (rte_strscpy(child, dent->d_name, size) < 0)
		return -rte_errno;
	return 0;
}

static int
mlx5_auxiliary_get_pci_path(const struct rte_auxiliary_device *dev,
			    char *sysfs_pci, size_t size)
{
	char sysfs_real[PATH_MAX] = { 0 };
	MKSTR(sysfs_aux, "%s/%s", AUXILIARY_SYSFS_PATH, dev->name);
	char *dir;

	if (realpath(sysfs_aux, sysfs_real) == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	dir = dirname(sysfs_real);
	if (dir == NULL) {
		rte_errno = errno;
		return -rte_errno;
	}
	if (rte_strscpy(sysfs_pci, dir, size) < 0)
		return -rte_errno;
	return 0;
}

int
mlx5_auxiliary_get_pci_str(const struct rte_auxiliary_device *dev,
			   char *addr, size_t size)
{
	char sysfs_pci[PATH_MAX];
	char *base;

	if (mlx5_auxiliary_get_pci_path(dev, sysfs_pci, sizeof(sysfs_pci)) != 0)
		return -ENODEV;
	base = basename(sysfs_pci);
	if (base == NULL)
		return -errno;
	if (rte_strscpy(addr, base, size) < 0)
		return -rte_errno;
	return 0;
}

static int
mlx5_auxiliary_get_numa(const struct rte_auxiliary_device *dev)
{
	unsigned long numa;
	char numa_path[PATH_MAX];

	if (mlx5_auxiliary_get_pci_path(dev, numa_path, sizeof(numa_path)) != 0)
		return SOCKET_ID_ANY;
	if (strcat(numa_path, "/numa_node") == NULL) {
		rte_errno = ENAMETOOLONG;
		return SOCKET_ID_ANY;
	}
	if (eal_parse_sysfs_value(numa_path, &numa) != 0) {
		rte_errno = EINVAL;
		return SOCKET_ID_ANY;
	}
	return (int)numa;
}

struct ibv_device *
mlx5_get_aux_ibv_device(const struct rte_auxiliary_device *dev)
{
	int n;
	char ib_name[64] = { 0 };
	struct ibv_device **ibv_list = mlx5_glue->get_device_list(&n);
	struct ibv_device *ibv_match = NULL;

	if (!ibv_list) {
		rte_errno = ENOSYS;
		return NULL;
	}
	if (mlx5_auxiliary_get_child_name(dev->name, "/infiniband",
					  ib_name, sizeof(ib_name)) != 0)
		goto out;
	while (n-- > 0) {
		if (strcmp(ibv_list[n]->name, ib_name) != 0)
			continue;
		ibv_match = ibv_list[n];
		break;
	}
	if (ibv_match == NULL)
		rte_errno = ENOENT;
out:
	mlx5_glue->free_device_list(ibv_list);
	return ibv_match;
}

static bool
mlx5_common_auxiliary_match(const char *name)
{
	return strncmp(name, MLX5_AUXILIARY_PREFIX,
		       strlen(MLX5_AUXILIARY_PREFIX)) == 0;
}

static int
mlx5_common_auxiliary_probe(struct rte_auxiliary_driver *drv __rte_unused,
			    struct rte_auxiliary_device *dev)
{
	dev->device.numa_node = mlx5_auxiliary_get_numa(dev);
	return mlx5_common_dev_probe(&dev->device);
}

static int
mlx5_common_auxiliary_remove(struct rte_auxiliary_device *auxiliary_dev)
{
	return mlx5_common_dev_remove(&auxiliary_dev->device);
}

static int
mlx5_common_auxiliary_dma_map(struct rte_auxiliary_device *auxiliary_dev,
			      void *addr, uint64_t iova, size_t len)
{
	return mlx5_common_dev_dma_map(&auxiliary_dev->device, addr, iova, len);
}

static int
mlx5_common_auxiliary_dma_unmap(struct rte_auxiliary_device *auxiliary_dev,
				void *addr, uint64_t iova, size_t len)
{
	return mlx5_common_dev_dma_unmap(&auxiliary_dev->device, addr, iova,
					 len);
}

static struct rte_auxiliary_driver mlx5_auxiliary_driver = {
	.driver = {
		   .name = MLX5_AUXILIARY_DRIVER_NAME,
	},
	.match = mlx5_common_auxiliary_match,
	.probe = mlx5_common_auxiliary_probe,
	.remove = mlx5_common_auxiliary_remove,
	.dma_map = mlx5_common_auxiliary_dma_map,
	.dma_unmap = mlx5_common_auxiliary_dma_unmap,
};

static bool mlx5_common_auxiliary_initialized;

void mlx5_common_auxiliary_init(void)
{
	if (!mlx5_common_auxiliary_initialized) {
		rte_auxiliary_register(&mlx5_auxiliary_driver);
		mlx5_common_auxiliary_initialized = true;
	}
}

RTE_FINI(mlx5_common_auxiliary_driver_finish)
{
	if (mlx5_common_auxiliary_initialized) {
		rte_auxiliary_unregister(&mlx5_auxiliary_driver);
		mlx5_common_auxiliary_initialized = false;
	}
}
