/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies Ltd
 */

#include <stdlib.h>
#include <rte_malloc.h>
#include "mlx5_common_utils.h"
#include "mlx5_common_pci.h"

struct mlx5_pci_device {
	struct rte_pci_device *pci_dev;
	TAILQ_ENTRY(mlx5_pci_device) next;
	uint32_t classes_loaded;
};

/* Head of list of drivers. */
static TAILQ_HEAD(mlx5_pci_bus_drv_head, mlx5_pci_driver) drv_list =
				TAILQ_HEAD_INITIALIZER(drv_list);

/* Head of mlx5 pci devices. */
static TAILQ_HEAD(mlx5_pci_devices_head, mlx5_pci_device) devices_list =
				TAILQ_HEAD_INITIALIZER(devices_list);

static const struct {
	const char *name;
	unsigned int driver_class;
} mlx5_classes[] = {
	{ .name = "vdpa", .driver_class = MLX5_CLASS_VDPA },
	{ .name = "net", .driver_class = MLX5_CLASS_NET },
	{ .name = "regex", .driver_class = MLX5_CLASS_REGEX },
};

static const unsigned int mlx5_class_combinations[] = {
	MLX5_CLASS_NET,
	MLX5_CLASS_VDPA,
	MLX5_CLASS_REGEX,
	MLX5_CLASS_NET | MLX5_CLASS_REGEX,
	MLX5_CLASS_VDPA | MLX5_CLASS_REGEX,
	/* New class combination should be added here. */
};

static int
class_name_to_value(const char *class_name)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(mlx5_classes); i++) {
		if (strcmp(class_name, mlx5_classes[i].name) == 0)
			return mlx5_classes[i].driver_class;
	}
	return -EINVAL;
}

static struct mlx5_pci_driver *
driver_get(uint32_t class)
{
	struct mlx5_pci_driver *driver;

	TAILQ_FOREACH(driver, &drv_list, next) {
		if (driver->driver_class == class)
			return driver;
	}
	return NULL;
}

static int
bus_cmdline_options_handler(__rte_unused const char *key,
			    const char *class_names, void *opaque)
{
	int *ret = opaque;
	char *nstr_org;
	int class_val;
	char *found;
	char *nstr;
	char *refstr = NULL;

	*ret = 0;
	nstr = strdup(class_names);
	if (!nstr) {
		*ret = -ENOMEM;
		return *ret;
	}
	nstr_org = nstr;
	found = strtok_r(nstr, ":", &refstr);
	if (!found)
		goto err;
	do {
		/* Extract each individual class name. Multiple
		 * class key,value is supplied as class=net:vdpa:foo:bar.
		 */
		class_val = class_name_to_value(found);
		/* Check if its a valid class. */
		if (class_val < 0) {
			*ret = -EINVAL;
			goto err;
		}
		*ret |= class_val;
		found = strtok_r(NULL, ":", &refstr);
	} while (found);
err:
	free(nstr_org);
	if (*ret < 0)
		DRV_LOG(ERR, "Invalid mlx5 class options %s."
			" Maybe typo in device class argument setting?",
			class_names);
	return *ret;
}

static int
parse_class_options(const struct rte_devargs *devargs)
{
	const char *key = MLX5_CLASS_ARG_NAME;
	struct rte_kvargs *kvlist;
	int ret = 0;

	if (devargs == NULL)
		return 0;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return 0;
	if (rte_kvargs_count(kvlist, key))
		rte_kvargs_process(kvlist, key, bus_cmdline_options_handler,
				   &ret);
	rte_kvargs_free(kvlist);
	return ret;
}

static bool
mlx5_bus_match(const struct mlx5_pci_driver *drv,
	       const struct rte_pci_device *pci_dev)
{
	const struct rte_pci_id *id_table;

	for (id_table = drv->pci_driver.id_table; id_table->vendor_id != 0;
	     id_table++) {
		/* Check if device's ids match the class driver's ids. */
		if (id_table->vendor_id != pci_dev->id.vendor_id &&
		    id_table->vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->device_id != pci_dev->id.device_id &&
		    id_table->device_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_vendor_id !=
		    pci_dev->id.subsystem_vendor_id &&
		    id_table->subsystem_vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_device_id !=
		    pci_dev->id.subsystem_device_id &&
		    id_table->subsystem_device_id != PCI_ANY_ID)
			continue;
		if (id_table->class_id != pci_dev->id.class_id &&
		    id_table->class_id != RTE_CLASS_ANY_ID)
			continue;
		return true;
	}
	return false;
}

static int
is_valid_class_combination(uint32_t user_classes)
{
	unsigned int i;

	/* Verify if user specified valid supported combination. */
	for (i = 0; i < RTE_DIM(mlx5_class_combinations); i++) {
		if (mlx5_class_combinations[i] == user_classes)
			return 0;
	}
	/* Not found any valid class combination. */
	return -EINVAL;
}

static struct mlx5_pci_device *
pci_to_mlx5_device(const struct rte_pci_device *pci_dev)
{
	struct mlx5_pci_device *dev;

	TAILQ_FOREACH(dev, &devices_list, next) {
		if (dev->pci_dev == pci_dev)
			return dev;
	}
	return NULL;
}

static bool
device_class_enabled(const struct mlx5_pci_device *device, uint32_t class)
{
	return (device->classes_loaded & class) ? true : false;
}

static void
dev_release(struct mlx5_pci_device *dev)
{
	TAILQ_REMOVE(&devices_list, dev, next);
	rte_free(dev);
}

static int
drivers_remove(struct mlx5_pci_device *dev, uint32_t enabled_classes)
{
	struct mlx5_pci_driver *driver;
	int local_ret = -ENODEV;
	unsigned int i = 0;
	int ret = 0;

	enabled_classes &= dev->classes_loaded;
	while (enabled_classes) {
		driver = driver_get(RTE_BIT64(i));
		if (driver) {
			local_ret = driver->pci_driver.remove(dev->pci_dev);
			if (!local_ret)
				dev->classes_loaded &= ~RTE_BIT64(i);
			else if (ret == 0)
				ret = local_ret;
		}
		enabled_classes &= ~RTE_BIT64(i);
		i++;
	}
	if (local_ret)
		ret = local_ret;
	return ret;
}

static int
drivers_probe(struct mlx5_pci_device *dev, struct rte_pci_driver *pci_drv,
	      struct rte_pci_device *pci_dev, uint32_t user_classes)
{
	struct mlx5_pci_driver *driver;
	uint32_t enabled_classes = 0;
	bool already_loaded;
	int ret;

	TAILQ_FOREACH(driver, &drv_list, next) {
		if ((driver->driver_class & user_classes) == 0)
			continue;
		if (!mlx5_bus_match(driver, pci_dev))
			continue;
		already_loaded = dev->classes_loaded & driver->driver_class;
		if (already_loaded &&
		    !(driver->pci_driver.drv_flags & RTE_PCI_DRV_PROBE_AGAIN)) {
			DRV_LOG(ERR, "Device %s is already probed\n",
				pci_dev->device.name);
			ret = -EEXIST;
			goto probe_err;
		}
		ret = driver->pci_driver.probe(pci_drv, pci_dev);
		if (ret < 0) {
			DRV_LOG(ERR, "Failed to load driver = %s.\n",
				driver->pci_driver.driver.name);
			goto probe_err;
		}
		enabled_classes |= driver->driver_class;
	}
	dev->classes_loaded |= enabled_classes;
	return 0;
probe_err:
	/* Only unload drivers which are enabled which were enabled
	 * in this probe instance.
	 */
	drivers_remove(dev, enabled_classes);
	return ret;
}

/**
 * DPDK callback to register to probe multiple drivers for a PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure.
 * @param[in] dev
 *   PCI device information.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_common_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		      struct rte_pci_device *pci_dev)
{
	struct mlx5_pci_device *dev;
	uint32_t user_classes = 0;
	bool new_device = false;
	int ret;

	ret = parse_class_options(pci_dev->device.devargs);
	if (ret < 0)
		return ret;
	user_classes = ret;
	if (user_classes) {
		/* Validate combination here. */
		ret = is_valid_class_combination(user_classes);
		if (ret) {
			DRV_LOG(ERR, "Unsupported mlx5 classes supplied.");
			return ret;
		}
	} else {
		/* Default to net class. */
		user_classes = MLX5_CLASS_NET;
	}
	dev = pci_to_mlx5_device(pci_dev);
	if (!dev) {
		dev = rte_zmalloc("mlx5_pci_device", sizeof(*dev), 0);
		if (!dev)
			return -ENOMEM;
		dev->pci_dev = pci_dev;
		TAILQ_INSERT_HEAD(&devices_list, dev, next);
		new_device = true;
	}
	ret = drivers_probe(dev, pci_drv, pci_dev, user_classes);
	if (ret)
		goto class_err;
	return 0;
class_err:
	if (new_device)
		dev_release(dev);
	return ret;
}

/**
 * DPDK callback to remove one or more drivers for a PCI device.
 *
 * This function removes all drivers probed for a given PCI device.
 *
 * @param[in] pci_dev
 *   Pointer to the PCI device.
 *
 * @return
 *   0 on success, the function cannot fail.
 */
static int
mlx5_common_pci_remove(struct rte_pci_device *pci_dev)
{
	struct mlx5_pci_device *dev;
	int ret;

	dev = pci_to_mlx5_device(pci_dev);
	if (!dev)
		return -ENODEV;
	/* Matching device found, cleanup and unload drivers. */
	ret = drivers_remove(dev, dev->classes_loaded);
	if (!ret)
		dev_release(dev);
	return ret;
}

static int
mlx5_common_pci_dma_map(struct rte_pci_device *pci_dev, void *addr,
			uint64_t iova, size_t len)
{
	struct mlx5_pci_driver *driver = NULL;
	struct mlx5_pci_driver *temp;
	struct mlx5_pci_device *dev;
	int ret = -EINVAL;

	dev = pci_to_mlx5_device(pci_dev);
	if (!dev)
		return -ENODEV;
	TAILQ_FOREACH(driver, &drv_list, next) {
		if (device_class_enabled(dev, driver->driver_class) &&
		    driver->pci_driver.dma_map) {
			ret = driver->pci_driver.dma_map(pci_dev, addr,
							 iova, len);
			if (ret)
				goto map_err;
		}
	}
	return ret;
map_err:
	TAILQ_FOREACH(temp, &drv_list, next) {
		if (temp == driver)
			break;
		if (device_class_enabled(dev, temp->driver_class) &&
		    temp->pci_driver.dma_map && temp->pci_driver.dma_unmap)
			temp->pci_driver.dma_unmap(pci_dev, addr, iova, len);
	}
	return ret;
}

static int
mlx5_common_pci_dma_unmap(struct rte_pci_device *pci_dev, void *addr,
			  uint64_t iova, size_t len)
{
	struct mlx5_pci_driver *driver;
	struct mlx5_pci_device *dev;
	int local_ret = -EINVAL;
	int ret;

	dev = pci_to_mlx5_device(pci_dev);
	if (!dev)
		return -ENODEV;
	ret = 0;
	/* There is no unmap error recovery in current implementation. */
	TAILQ_FOREACH_REVERSE(driver, &drv_list, mlx5_pci_bus_drv_head, next) {
		if (device_class_enabled(dev, driver->driver_class) &&
		    driver->pci_driver.dma_unmap) {
			local_ret = driver->pci_driver.dma_unmap(pci_dev, addr,
								 iova, len);
			if (local_ret && (ret == 0))
				ret = local_ret;
		}
	}
	if (local_ret)
		ret = local_ret;
	return ret;
}

/* PCI ID table is build dynamically based on registered mlx5 drivers. */
static struct rte_pci_id *mlx5_pci_id_table;

static struct rte_pci_driver mlx5_pci_driver = {
	.driver = {
		.name = MLX5_DRIVER_NAME,
	},
	.probe = mlx5_common_pci_probe,
	.remove = mlx5_common_pci_remove,
	.dma_map = mlx5_common_pci_dma_map,
	.dma_unmap = mlx5_common_pci_dma_unmap,
};

static int
pci_id_table_size_get(const struct rte_pci_id *id_table)
{
	int table_size = 0;

	for (; id_table->vendor_id != 0; id_table++)
		table_size++;
	return table_size;
}

static bool
pci_id_exists(const struct rte_pci_id *id, const struct rte_pci_id *table,
	      int next_idx)
{
	int current_size = next_idx - 1;
	int i;

	for (i = 0; i < current_size; i++) {
		if (id->device_id == table[i].device_id &&
		    id->vendor_id == table[i].vendor_id &&
		    id->subsystem_vendor_id == table[i].subsystem_vendor_id &&
		    id->subsystem_device_id == table[i].subsystem_device_id)
			return true;
	}
	return false;
}

static void
pci_id_insert(struct rte_pci_id *new_table, int *next_idx,
	      const struct rte_pci_id *id_table)
{
	/* Traverse the id_table, check if entry exists in new_table;
	 * Add non duplicate entries to new table.
	 */
	for (; id_table->vendor_id != 0; id_table++) {
		if (!pci_id_exists(id_table, new_table, *next_idx)) {
			/* New entry; add to the table. */
			new_table[*next_idx] = *id_table;
			(*next_idx)++;
		}
	}
}

static int
pci_ids_table_update(const struct rte_pci_id *driver_id_table)
{
	const struct rte_pci_id *id_iter;
	struct rte_pci_id *updated_table;
	struct rte_pci_id *old_table;
	int num_ids = 0;
	int i = 0;

	old_table = mlx5_pci_id_table;
	if (old_table)
		num_ids = pci_id_table_size_get(old_table);
	num_ids += pci_id_table_size_get(driver_id_table);
	/* Increase size by one for the termination entry of vendor_id = 0. */
	num_ids += 1;
	updated_table = calloc(num_ids, sizeof(*updated_table));
	if (!updated_table)
		return -ENOMEM;
	if (TAILQ_EMPTY(&drv_list)) {
		/* Copy the first driver's ID table. */
		for (id_iter = driver_id_table; id_iter->vendor_id != 0;
		     id_iter++, i++)
			updated_table[i] = *id_iter;
	} else {
		/* First copy existing table entries. */
		for (id_iter = old_table; id_iter->vendor_id != 0;
		     id_iter++, i++)
			updated_table[i] = *id_iter;
		/* New id to be added at the end of current ID table. */
		pci_id_insert(updated_table, &i, driver_id_table);
	}
	/* Terminate table with empty entry. */
	updated_table[i].vendor_id = 0;
	mlx5_pci_driver.id_table = updated_table;
	mlx5_pci_id_table = updated_table;
	if (old_table)
		free(old_table);
	return 0;
}

void
mlx5_pci_driver_register(struct mlx5_pci_driver *driver)
{
	int ret;

	ret = pci_ids_table_update(driver->pci_driver.id_table);
	if (ret)
		return;
	mlx5_pci_driver.drv_flags |= driver->pci_driver.drv_flags;
	TAILQ_INSERT_TAIL(&drv_list, driver, next);
}

void mlx5_common_pci_init(void)
{
	const struct rte_pci_id empty_table[] = {
		{
			.vendor_id = 0
		},
	};

	/* All mlx5 PMDs constructor runs at same priority. So any of the PMD
	 * including this one can register the PCI table first. If any other
	 * PMD(s) have registered the PCI ID table, No need to register an empty
	 * default one.
	 */
	if (mlx5_pci_id_table == NULL && pci_ids_table_update(empty_table))
		return;
	rte_pci_register(&mlx5_pci_driver);
}

RTE_FINI(mlx5_common_pci_finish)
{
	if (mlx5_pci_id_table != NULL) {
		/* Constructor doesn't register with PCI bus if it failed
		 * to build the table.
		 */
		rte_pci_unregister(&mlx5_pci_driver);
		free(mlx5_pci_id_table);
	}
}
RTE_PMD_EXPORT_NAME(mlx5_common_pci, __COUNTER__);
