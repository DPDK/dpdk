/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies Ltd
 */

#include <stdlib.h>

#include <rte_malloc.h>
#include <rte_devargs.h>
#include <rte_errno.h>
#include <rte_class.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>

#include "mlx5_common_log.h"
#include "mlx5_common_private.h"

static struct rte_pci_driver mlx5_common_pci_driver;

/* PCI ID table is build dynamically based on registered mlx5 drivers. */
static struct rte_pci_id *mlx5_pci_id_table;

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
	if (old_table == NULL) {
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
	mlx5_common_pci_driver.id_table = updated_table;
	mlx5_pci_id_table = updated_table;
	free(old_table);
	return 0;
}

bool
mlx5_dev_is_pci(const struct rte_device *dev)
{
	return strcmp(dev->bus->name, "pci") == 0;
}

bool
mlx5_dev_is_vf_pci(struct rte_pci_device *pci_dev)
{
	switch (pci_dev->id.device_id) {
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX4LXVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5EXVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX5BFVF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTX6VF:
	case PCI_DEVICE_ID_MELLANOX_CONNECTXVF:
		return true;
	default:
		break;
	}
	return false;
}

bool
mlx5_dev_pci_match(const struct mlx5_class_driver *drv,
		   const struct rte_device *dev)
{
	const struct rte_pci_device *pci_dev;
	const struct rte_pci_id *id_table;

	if (!mlx5_dev_is_pci(dev))
		return false;
	pci_dev = RTE_DEV_TO_PCI_CONST(dev);
	for (id_table = drv->id_table; id_table->vendor_id != 0;
	     id_table++) {
		/* Check if device's ids match the class driver's ids. */
		if (id_table->vendor_id != pci_dev->id.vendor_id &&
		    id_table->vendor_id != RTE_PCI_ANY_ID)
			continue;
		if (id_table->device_id != pci_dev->id.device_id &&
		    id_table->device_id != RTE_PCI_ANY_ID)
			continue;
		if (id_table->subsystem_vendor_id !=
		    pci_dev->id.subsystem_vendor_id &&
		    id_table->subsystem_vendor_id != RTE_PCI_ANY_ID)
			continue;
		if (id_table->subsystem_device_id !=
		    pci_dev->id.subsystem_device_id &&
		    id_table->subsystem_device_id != RTE_PCI_ANY_ID)
			continue;
		if (id_table->class_id != pci_dev->id.class_id &&
		    id_table->class_id != RTE_CLASS_ANY_ID)
			continue;
		return true;
	}
	return false;
}

static int
mlx5_common_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		      struct rte_pci_device *pci_dev)
{
	return mlx5_common_dev_probe(&pci_dev->device);
}

static int
mlx5_common_pci_remove(struct rte_pci_device *pci_dev)
{
	return mlx5_common_dev_remove(&pci_dev->device);
}

static int
mlx5_common_pci_dma_map(struct rte_pci_device *pci_dev, void *addr,
			uint64_t iova, size_t len)
{
	return mlx5_common_dev_dma_map(&pci_dev->device, addr, iova, len);
}

static int
mlx5_common_pci_dma_unmap(struct rte_pci_device *pci_dev, void *addr,
			  uint64_t iova, size_t len)
{
	return mlx5_common_dev_dma_unmap(&pci_dev->device, addr, iova, len);
}

void
mlx5_common_driver_on_register_pci(struct mlx5_class_driver *driver)
{
	if (driver->id_table != NULL) {
		if (pci_ids_table_update(driver->id_table) != 0)
			return;
	}
	if (driver->probe_again)
		mlx5_common_pci_driver.drv_flags |= RTE_PCI_DRV_PROBE_AGAIN;
	if (driver->intr_lsc)
		mlx5_common_pci_driver.drv_flags |= RTE_PCI_DRV_INTR_LSC;
	if (driver->intr_rmv)
		mlx5_common_pci_driver.drv_flags |= RTE_PCI_DRV_INTR_RMV;
}

static struct rte_pci_driver mlx5_common_pci_driver = {
	.driver = {
		   .name = MLX5_PCI_DRIVER_NAME,
	},
	.probe = mlx5_common_pci_probe,
	.remove = mlx5_common_pci_remove,
	.dma_map = mlx5_common_pci_dma_map,
	.dma_unmap = mlx5_common_pci_dma_unmap,
};

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
	rte_pci_register(&mlx5_common_pci_driver);
}

RTE_FINI(mlx5_common_pci_finish)
{
	if (mlx5_pci_id_table != NULL) {
		/* Constructor doesn't register with PCI bus if it failed
		 * to build the table.
		 */
		rte_pci_unregister(&mlx5_common_pci_driver);
		free(mlx5_pci_id_table);
	}
}

RTE_PMD_EXPORT_NAME(mlx5_common_pci, __COUNTER__);
