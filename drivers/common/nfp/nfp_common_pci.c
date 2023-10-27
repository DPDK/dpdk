/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_common_pci.h"

#include <string.h>

#include <rte_class.h>
#include <rte_devargs.h>
#include <rte_kvargs.h>

#include "nfp_common_log.h"

/* Reported driver name. */
#define NFP_PCI_DRIVER_NAME "nfp_common_pci"

static struct rte_pci_driver nfp_common_pci_driver;

/* PCI ID table is build dynamically based on registered nfp drivers. */
static struct rte_pci_id *nfp_pci_id_table;

/* Head of list of drivers. */
static TAILQ_HEAD(nfp_drivers, nfp_class_driver) nfp_drivers_list =
		TAILQ_HEAD_INITIALIZER(nfp_drivers_list);

static bool nfp_common_initialized;

static const struct {
	const char *name;
	enum nfp_class drv_class;
} nfp_classes[] = {
	{ .name = "eth",      .drv_class = NFP_CLASS_ETH },
	{ .name = "vdpa",     .drv_class = NFP_CLASS_VDPA },
};

static enum nfp_class
nfp_class_name_to_value(const char *class_name)
{
	uint32_t i;

	for (i = 0; i < RTE_DIM(nfp_classes); i++) {
		if (strcmp(class_name, nfp_classes[i].name) == 0)
			return nfp_classes[i].drv_class;
	}

	return NFP_CLASS_INVALID;
}

static uint32_t
nfp_pci_id_table_size_get(const struct rte_pci_id *id_table)
{
	uint32_t table_size;

	if (id_table == NULL)
		return 0;

	for (table_size = 0; id_table->vendor_id != 0; id_table++)
		table_size++;

	return table_size;
}

static bool
nfp_pci_id_exists(const struct rte_pci_id *id,
		const struct rte_pci_id *table,
		uint32_t next_idx)
{
	uint32_t i;

	if (next_idx == 0)
		return false;

	for (i = 0; i < next_idx; i++) {
		if (id->device_id == table[i].device_id &&
				id->vendor_id == table[i].vendor_id &&
				id->subsystem_vendor_id == table[i].subsystem_vendor_id &&
				id->subsystem_device_id == table[i].subsystem_device_id)
			return true;
	}

	return false;
}

static void
nfp_pci_id_insert(struct rte_pci_id *new_table,
		uint32_t *next_idx,
		const struct rte_pci_id *id_table)
{
	if (id_table == NULL)
		return;

	/* Add non duplicate entries to new table. */
	for (; id_table->vendor_id != 0; id_table++) {
		if (!nfp_pci_id_exists(id_table, new_table, *next_idx)) {
			new_table[*next_idx] = *id_table;
			(*next_idx)++;
		}
	}
}

static int
nfp_pci_id_table_update(const struct rte_pci_id *driver_id_table)
{
	uint32_t i = 0;
	uint32_t num_ids = 0;
	struct rte_pci_id *old_table;
	const struct rte_pci_id *id_iter;
	struct rte_pci_id *updated_table;

	old_table = nfp_pci_id_table;
	if (old_table != NULL)
		num_ids = nfp_pci_id_table_size_get(old_table);
	num_ids += nfp_pci_id_table_size_get(driver_id_table);

	/* Increase size by one for the termination entry of vendor_id = 0. */
	num_ids += 1;
	updated_table = calloc(num_ids, sizeof(struct rte_pci_id));
	if (updated_table == NULL)
		return -ENOMEM;

	if (old_table == NULL) {
		/* Copy the first driver's ID table. */
		for (id_iter = driver_id_table; id_iter[i].vendor_id != 0; i++)
			updated_table[i] = id_iter[i];
	} else {
		/* First copy existing table entries. */
		for (id_iter = old_table; id_iter[i].vendor_id != 0; i++)
			updated_table[i] = id_iter[i];
		/* New id to be added at the end of current ID table. */
		nfp_pci_id_insert(updated_table, &i, driver_id_table);

		free(old_table);
	}

	/* Terminate table with empty entry. */
	updated_table[i].vendor_id = 0;
	nfp_pci_id_table = updated_table;
	nfp_common_pci_driver.id_table = nfp_pci_id_table;

	return 0;
}

static int
nfp_kvarg_dev_class_handler(__rte_unused const char *key,
		const char *class_str,
		void *opaque)
{
	enum nfp_class *dev_class = opaque;

	if (class_str == NULL)
		return *dev_class;

	*dev_class = nfp_class_name_to_value(class_str);

	return 0;
}

static enum nfp_class
nfp_parse_class_options(const struct rte_devargs *devargs)
{
	struct rte_kvargs *kvargs;
	enum nfp_class dev_class = NFP_CLASS_ETH;

	if (devargs == NULL)
		return dev_class;

	kvargs = rte_kvargs_parse(devargs->args, NULL);
	if (kvargs == NULL)
		return dev_class;

	if (rte_kvargs_count(kvargs, RTE_DEVARGS_KEY_CLASS) != 0) {
		rte_kvargs_process(kvargs, RTE_DEVARGS_KEY_CLASS,
				nfp_kvarg_dev_class_handler, &dev_class);
	}

	rte_kvargs_free(kvargs);

	return dev_class;
}

static int
nfp_drivers_probe(struct rte_pci_device *pci_dev,
		enum nfp_class class)
{
	int32_t ret = 0;
	struct nfp_class_driver *driver;

	TAILQ_FOREACH(driver, &nfp_drivers_list, next) {
		if (driver->drv_class != class)
			continue;

		ret = driver->probe(pci_dev);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to load driver %s", driver->name);
			return ret;
		}
	}

	return 0;
}

static int
nfp_common_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	enum nfp_class class;
	struct rte_device *eal_dev = &pci_dev->device;

	PMD_DRV_LOG(INFO, "probe device %s.", eal_dev->name);

	class = nfp_parse_class_options(eal_dev->devargs);
	if (class == NFP_CLASS_INVALID) {
		PMD_DRV_LOG(ERR, "Unsupported nfp class type: %s",
				eal_dev->devargs->args);
		return -ENOTSUP;
	}

	return nfp_drivers_probe(pci_dev, class);
}

static int
nfp_common_pci_remove(__rte_unused struct rte_pci_device *pci_dev)
{
	return 0;
}

static struct rte_pci_driver nfp_common_pci_driver = {
	.driver = {
		.name = NFP_PCI_DRIVER_NAME,
	},
	.probe = nfp_common_pci_probe,
	.remove = nfp_common_pci_remove,
};

static void
nfp_common_init(void)
{
	const struct rte_pci_id empty_table[] = {
		{
			.vendor_id = 0
		},
	};

	if (nfp_common_initialized)
		return;

	/*
	 * All the constructor of NFP PMDs run at same priority. So any of the PMD
	 * including this one can register the PCI table first. If any other
	 * PMD(s) have registered the PCI ID table, no need to register an empty
	 * default one.
	 */
	if (nfp_pci_id_table == NULL && nfp_pci_id_table_update(empty_table) != 0)
		return;

	rte_pci_register(&nfp_common_pci_driver);
	nfp_common_initialized = true;
}

void
nfp_class_driver_register(struct nfp_class_driver *driver)
{
	nfp_common_init();

	if (driver->id_table != NULL) {
		if (nfp_pci_id_table_update(driver->id_table) != 0)
			return;
	}

	nfp_common_pci_driver.drv_flags |= driver->drv_flags;

	TAILQ_INSERT_TAIL(&nfp_drivers_list, driver, next);
}
