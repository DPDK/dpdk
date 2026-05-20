/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_version.h>
#include <rte_pci.h>
#include <rte_dev.h>
#include <rte_devargs.h>
#include <rte_class.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_fbarray.h>
#include <rte_eal.h>
#include <eal_private.h>
#include <eal_memcfg.h>
#include <bus_driver.h>
#include <bus_pci_driver.h>
#include <eal_export.h>
#include <pthread.h>

#include "sxe2_common.h"
#include "sxe2_common_log.h"
#include "sxe2_ioctl_chnl_func.h"

static TAILQ_HEAD(sxe2_class_drivers, sxe2_class_driver) sxe2_class_drivers_list =
				TAILQ_HEAD_INITIALIZER(sxe2_class_drivers_list);

static TAILQ_HEAD(sxe2_common_devices, sxe2_common_device) sxe2_common_devices_list =
				TAILQ_HEAD_INITIALIZER(sxe2_common_devices_list);

static pthread_mutex_t sxe2_common_devices_list_lock;

static struct rte_pci_id *sxe2_common_pci_id_table;

static const struct {
	const char *name;
	uint32_t class_type;
} sxe2_class_types[] = {
	{ .name = "eth", .class_type = SXE2_CLASS_TYPE_ETH },
	{ .name = "vdpa", .class_type = SXE2_CLASS_TYPE_VDPA },
};

static uint32_t sxe2_class_name_to_value(const char *class_name)
{
	uint32_t class_type = SXE2_CLASS_TYPE_INVALID;
	uint32_t i;

	for (i = 0; i < RTE_DIM(sxe2_class_types); i++) {
		if (strcmp(class_name, sxe2_class_types[i].name) == 0)
			class_type = sxe2_class_types[i].class_type;
	}

	return class_type;
}

static struct sxe2_common_device *sxe2_rtedev_to_cdev(struct rte_device *rte_dev)
{
	struct sxe2_common_device *cdev = NULL;

	TAILQ_FOREACH(cdev, &sxe2_common_devices_list, next) {
		if (rte_dev == cdev->dev)
			goto l_end;
	}

	cdev = NULL;
l_end:
	return cdev;
}

static struct sxe2_class_driver *sxe2_class_driver_get(uint32_t class_type)
{
	struct sxe2_class_driver *cdrv = NULL;

	TAILQ_FOREACH(cdrv, &sxe2_class_drivers_list, next) {
		if (cdrv->drv_class == class_type)
			goto l_end;
	}

	cdrv = NULL;
l_end:
	return cdrv;
}

static int32_t sxe2_kvargs_preprocessing(struct sxe2_dev_kvargs_info *kv_info,
			const struct rte_devargs *devargs)
{
	const struct rte_kvargs_pair *pair;
	struct rte_kvargs *kvlist;
	int32_t ret = -1;
	uint32_t i;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL) {
		ret = -EINVAL;
		goto l_end;
	}

	for (i = 0; i < kvlist->count; i++) {
		pair = &kvlist->pairs[i];
		if (pair->value == NULL || *(pair->value) == '\0') {
			PMD_LOG_ERR(COM, "Key %s has no value.", pair->key);
			rte_kvargs_free(kvlist);
			ret = -EINVAL;
			goto l_end;
		}
	}

	kv_info->kvlist = kvlist;
	ret = 0;
	PMD_LOG_DEBUG(COM, "kvargs %d preprocessing success.",
			kv_info->kvlist->count);
l_end:
	return ret;
}

static void sxe2_kvargs_free(struct sxe2_dev_kvargs_info *kv_info)
{
	if ((kv_info != NULL) && (kv_info->kvlist != NULL)) {
		rte_kvargs_free(kv_info->kvlist);
		kv_info->kvlist = NULL;
	}
}

RTE_EXPORT_INTERNAL_SYMBOL(sxe2_kvargs_process)
int32_t
sxe2_kvargs_process(struct sxe2_dev_kvargs_info *kv_info,
		const char *const key_match, arg_handler_t handler, void *opaque_arg)
{
	const struct rte_kvargs_pair *pair;
	struct rte_kvargs *kvlist;
	uint32_t i;
	int32_t ret = 0;

	if ((kv_info == NULL) || (kv_info->kvlist == NULL) ||
			(key_match == NULL)) {
		PMD_LOG_ERR(COM, "Failed to process kvargs, NULL parameter.");
		ret = -EINVAL;
		goto l_end;
	}
	kvlist = kv_info->kvlist;

	for (i = 0; i < kvlist->count; i++) {
		pair = &kvlist->pairs[i];
		if (strcmp(pair->key, key_match) == 0) {
			ret = (*handler)(pair->key, pair->value, opaque_arg);
			if (ret)
				goto l_end;

			kv_info->is_used[i] = true;
			break;
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_parse_class_type(const char *key, const char *value, void *args)
{
	uint32_t *class_type = (uint32_t *)args;
	int32_t ret = 0;

	*class_type = sxe2_class_name_to_value(value);
	if (*class_type == SXE2_CLASS_TYPE_INVALID) {
		ret = -EINVAL;
		PMD_LOG_ERR(COM, "Unsupported %s type: %s", key, value);
	}

	return ret;
}

static int32_t sxe2_common_device_setup(struct sxe2_common_device *cdev)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(cdev->dev);
	int32_t ret = 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		goto l_end;

	ret = sxe2_drv_dev_open(cdev, pci_dev);
	if (ret != 0) {
		PMD_LOG_ERR(COM, "Open pmd chrdev failed, ret=%d", ret);
		goto l_end;
	}

	ret = sxe2_drv_dev_handshake(cdev);
	if (ret != 0) {
		PMD_LOG_ERR(COM, "Handshake failed, ret=%d", ret);
		goto l_close_dev;
	}

	goto l_end;

l_close_dev:
	sxe2_drv_dev_close(cdev);
l_end:
	return ret;
}

static void sxe2_common_device_cleanup(struct sxe2_common_device *cdev)
{
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;

	if (TAILQ_EMPTY(&sxe2_common_devices_list))
		(void)rte_mem_event_callback_unregister("SXE2_MEM_ENVENT_CB", NULL);

	sxe2_drv_dev_close(cdev);
}

static struct sxe2_common_device *sxe2_common_device_alloc(
		struct rte_device *rte_dev, uint32_t class_type)
{
	struct sxe2_common_device *cdev = NULL;

	cdev = rte_zmalloc("sxe2_common_device", sizeof(*cdev), 0);
	if (cdev == NULL) {
		PMD_LOG_ERR(COM, "Fail to alloc sxe2 common device.");
		goto l_end;
	}
	cdev->dev = rte_dev;
	cdev->class_type = class_type;
	cdev->config.kernel_reset = false;
	pthread_mutex_init(&cdev->config.lock, NULL);

	pthread_mutex_lock(&sxe2_common_devices_list_lock);
	TAILQ_INSERT_TAIL(&sxe2_common_devices_list, cdev, next);
	pthread_mutex_unlock(&sxe2_common_devices_list_lock);

l_end:
	return cdev;
}

static void sxe2_common_device_free(struct sxe2_common_device *cdev)
{

	pthread_mutex_lock(&sxe2_common_devices_list_lock);
	TAILQ_REMOVE(&sxe2_common_devices_list, cdev, next);
	pthread_mutex_unlock(&sxe2_common_devices_list_lock);

	rte_free(cdev);
}

static bool sxe2_dev_is_pci(const struct rte_device *dev)
{
	return strcmp(dev->bus->name, "pci") == 0;
}

static bool sxe2_dev_pci_id_match(const struct sxe2_class_driver *cdrv,
			const struct rte_device *dev)
{
	const struct rte_pci_device *pci_dev;
	const struct rte_pci_id *id_table;
	bool ret = false;

	if (!sxe2_dev_is_pci(dev)) {
		PMD_LOG_ERR(COM, "Device %s is not a PCI device", dev->name);
		goto l_end;
	}

	pci_dev = RTE_DEV_TO_PCI_CONST(dev);
	for (id_table = cdrv->id_table; id_table->vendor_id != 0;
			id_table++) {

		if (id_table->vendor_id != pci_dev->id.vendor_id &&
				id_table->vendor_id != RTE_PCI_ANY_ID) {
			continue;
		}
		if (id_table->device_id != pci_dev->id.device_id &&
				id_table->device_id != RTE_PCI_ANY_ID) {
			continue;
		}
		if (id_table->subsystem_vendor_id !=
				pci_dev->id.subsystem_vendor_id &&
				id_table->subsystem_vendor_id != RTE_PCI_ANY_ID) {
			continue;
		}
		if (id_table->subsystem_device_id !=
				pci_dev->id.subsystem_device_id &&
				id_table->subsystem_device_id != RTE_PCI_ANY_ID) {

			continue;
		}
		if (id_table->class_id != pci_dev->id.class_id &&
				id_table->class_id != RTE_CLASS_ANY_ID) {
			continue;
		}
		ret = true;
		break;
	}

l_end:
	return ret;
}

static int32_t sxe2_classes_driver_probe(struct sxe2_common_device *cdev,
		struct sxe2_dev_kvargs_info *kv_info, uint32_t class_type)
{
	struct sxe2_class_driver *cdrv = NULL;
	int32_t ret = -1;

	cdrv = sxe2_class_driver_get(class_type);
	if (cdrv == NULL) {
		PMD_LOG_ERR(COM, "Fail to get class type[%u] driver.", class_type);
		goto l_end;
	}

	if (!sxe2_dev_pci_id_match(cdrv, cdev->dev)) {
		PMD_LOG_ERR(COM, "Fail to match pci id for driver:%s.", cdrv->name);
		goto l_end;
	}

	ret = cdrv->probe(cdev, kv_info);
	if (ret) {

		PMD_LOG_DEBUG(COM, "Fail to probe driver:%s.", cdrv->name);
		goto l_end;
	}

	cdev->cdrv = cdrv;
l_end:
	return ret;
}

static int32_t sxe2_classes_driver_remove(struct sxe2_common_device *cdev)
{
	struct sxe2_class_driver *cdrv = cdev->cdrv;

	return cdrv->remove(cdev);
}

static int32_t sxe2_kvargs_validate(struct sxe2_dev_kvargs_info *kv_info)
{
	int32_t ret = 0;
	uint32_t i;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		goto l_end;

	if (kv_info == NULL)
		goto l_end;

	for (i = 0; i < kv_info->kvlist->count; i++) {
		if (kv_info->is_used[i] == 0) {
			PMD_LOG_ERR(COM, "Key \"%s\" is unsupported for the class driver.",
					kv_info->kvlist->pairs[i].key);
			ret = -EINVAL;
			goto l_end;
		}
	}

l_end:
	return ret;
}

static int32_t sxe2_common_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	struct rte_device *rte_dev =  &pci_dev->device;
	struct sxe2_common_device *cdev;
	struct sxe2_dev_kvargs_info *kv_info_p = NULL;

	uint32_t class_type = SXE2_CLASS_TYPE_ETH;
	int32_t ret = -1;

	PMD_LOG_INFO(COM, "Probe pci device: %s", pci_dev->name);

	cdev = sxe2_rtedev_to_cdev(rte_dev);
	if (cdev != NULL) {
		PMD_LOG_ERR(COM, "Device %s already probed.", rte_dev->name);
		ret = -EBUSY;
		goto l_end;
	}

	if ((rte_dev->devargs != NULL) && (rte_dev->devargs->args != NULL)) {
		kv_info_p = calloc(1, sizeof(struct sxe2_dev_kvargs_info));
		if (!kv_info_p) {
			PMD_LOG_ERR(COM, "Failed to allocate memory for kv_info");
			goto l_end;
		}

		ret = sxe2_kvargs_preprocessing(kv_info_p, rte_dev->devargs);
		if (ret < 0) {
			PMD_LOG_ERR(COM, "Unsupported device args: %s",
				rte_dev->devargs->args);
			goto l_free_kvargs;
		}

		ret = sxe2_kvargs_process(kv_info_p, SXE2_DEVARGS_KEY_CLASS,
				sxe2_parse_class_type, &class_type);
		if (ret < 0) {
			PMD_LOG_ERR(COM, "Unsupported sxe2 driver class: %s",
				rte_dev->devargs->args);
			goto l_free_args;
		}

	}

	cdev = sxe2_common_device_alloc(rte_dev, class_type);
	if (cdev == NULL) {
		ret = -ENOMEM;
		goto l_free_args;
	}

	ret = sxe2_common_device_setup(cdev);
	if (ret != 0)
		goto l_err_setup;

	ret = sxe2_classes_driver_probe(cdev, kv_info_p, class_type);
	if (ret != 0)
		goto l_err_probe;

	ret = sxe2_kvargs_validate(kv_info_p);
	if (ret != 0) {
		PMD_LOG_ERR(COM, "Device args validate failed: %s",
			rte_dev->devargs->args);
		goto l_err_valid;
	}
	cdev->kvargs = kv_info_p;

	goto l_end;
l_err_valid:
	(void)sxe2_classes_driver_remove(cdev);
l_err_probe:
	sxe2_common_device_cleanup(cdev);
l_err_setup:
	sxe2_common_device_free(cdev);
l_free_args:
	sxe2_kvargs_free(kv_info_p);
l_free_kvargs:
	free(kv_info_p);
l_end:
	return ret;
}

static int32_t sxe2_common_pci_remove(struct rte_pci_device *pci_dev)
{
	struct sxe2_common_device *cdev;
	int32_t ret = -1;

	PMD_LOG_INFO(COM, "Remove pci device: %s", pci_dev->name);
	cdev = sxe2_rtedev_to_cdev(&pci_dev->device);
	if (cdev == NULL) {
		ret = -ENODEV;
		PMD_LOG_ERR(COM, "Fail to get device when remove.");
		goto l_end;
	}

	ret = sxe2_classes_driver_remove(cdev);
	if (ret != 0) {
		PMD_LOG_ERR(COM, "Fail to remove device: %s", pci_dev->name);
		goto l_end;
	}

	sxe2_common_device_cleanup(cdev);

	if (cdev->kvargs != NULL) {
		sxe2_kvargs_free(cdev->kvargs);
		free(cdev->kvargs);
		cdev->kvargs = NULL;
	}

	sxe2_common_device_free(cdev);

l_end:
	return ret;
}

static int32_t sxe2_common_pci_dma_map(struct rte_pci_device *pci_dev,
		void *addr,	uint64_t iova, size_t len)
{
	struct sxe2_common_device *cdev;
	int32_t ret = -1;

	cdev = sxe2_rtedev_to_cdev(&pci_dev->device);
	if (cdev == NULL) {
		ret = -ENODEV;
		PMD_LOG_ERR(COM, "Fail to get device when dma map.");
		goto l_end;
	}

	ret = sxe2_drv_dev_dma_map(cdev, (uint64_t)(uintptr_t)addr, iova, len);
	if (ret) {
		PMD_LOG_ERR(COM, "Fail to map dma map, ret=%d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static int32_t sxe2_common_pci_dma_unmap(struct rte_pci_device *pci_dev,
		void *addr __rte_unused, uint64_t iova, size_t len __rte_unused)
{
	struct sxe2_common_device *cdev;
	int32_t ret = -1;

	cdev = sxe2_rtedev_to_cdev(&pci_dev->device);
	if (cdev == NULL) {
		ret = -ENODEV;
		PMD_LOG_ERR(COM, "Fail to get device when dma unmap.");
		goto l_end;
	}

	ret = sxe2_drv_dev_dma_unmap(cdev, iova);
	if (ret) {
		PMD_LOG_ERR(COM, "Fail to unmap dma map, ret=%d", ret);
		goto l_end;
	}

l_end:
	return ret;
}

static struct rte_pci_driver sxe2_common_pci_driver = {
	.driver = {
		   .name = SXE2_COMMON_PCI_DRIVER_NAME,
	},
	.probe = sxe2_common_pci_probe,
	.remove = sxe2_common_pci_remove,
	.dma_map = sxe2_common_pci_dma_map,
	.dma_unmap = sxe2_common_pci_dma_unmap,
};

static uint32_t sxe2_common_pci_id_table_size_get(const struct rte_pci_id *id_table)
{
	uint32_t table_size = 0;

	while (id_table->vendor_id != 0) {
		table_size++;
		id_table++;
	}

	return table_size;
}

static bool sxe2_common_pci_id_exists(const struct rte_pci_id *id,
		const struct rte_pci_id *id_table, uint32_t next_idx)
{
	int32_t current_size = next_idx - 1;
	int32_t i;
	bool exists = false;

	for (i = 0; i < current_size; i++) {
		if ((id->device_id == id_table[i].device_id) &&
				(id->vendor_id == id_table[i].vendor_id) &&
				(id->subsystem_vendor_id == id_table[i].subsystem_vendor_id) &&
				(id->subsystem_device_id == id_table[i].subsystem_device_id)) {
			exists = true;
			break;
		}
	}

	return exists;
}

static void sxe2_common_pci_id_insert(struct rte_pci_id *id_table,
		uint32_t *next_idx, const struct rte_pci_id *insert_table)
{
	for (; insert_table->vendor_id != 0; insert_table++) {
		if (!sxe2_common_pci_id_exists(insert_table, id_table, *next_idx)) {

			id_table[*next_idx] = *insert_table;
			(*next_idx)++;
		}
	}
}

static int32_t sxe2_common_pci_id_table_update(const struct rte_pci_id *id_table)
{
	const struct rte_pci_id *id_iter;
	struct rte_pci_id *updated_table;
	struct rte_pci_id *old_table;
	uint32_t num_ids = 0;
	uint32_t i = 0;
	int32_t ret = 0;

	old_table = sxe2_common_pci_id_table;
	if (old_table)
		num_ids = sxe2_common_pci_id_table_size_get(old_table);

	num_ids += sxe2_common_pci_id_table_size_get(id_table);

	num_ids += 1;

	updated_table = calloc(num_ids, sizeof(*updated_table));
	if (!updated_table) {
		PMD_LOG_ERR(COM, "Failed to allocate memory for PCI ID table");
		goto l_end;
	}

	if (old_table == NULL) {

		for (id_iter = id_table; id_iter->vendor_id != 0;
				id_iter++, i++)
			updated_table[i] = *id_iter;
	} else {

		for (id_iter = old_table; id_iter->vendor_id != 0;
				id_iter++, i++)
			updated_table[i] = *id_iter;

		sxe2_common_pci_id_insert(updated_table, &i, id_table);
	}

	updated_table[i].vendor_id = 0;
	sxe2_common_pci_driver.id_table = updated_table;
	sxe2_common_pci_id_table = updated_table;
	free(old_table);

l_end:
	return ret;
}

static void sxe2_common_driver_on_register_pci(struct sxe2_class_driver *driver)
{
	if (driver->id_table != NULL) {
		if (sxe2_common_pci_id_table_update(driver->id_table) != 0)
			return;
	}

	if (driver->intr_lsc)
		sxe2_common_pci_driver.drv_flags |= RTE_PCI_DRV_INTR_LSC;
	if (driver->intr_rmv)
		sxe2_common_pci_driver.drv_flags |= RTE_PCI_DRV_INTR_RMV;
}

RTE_EXPORT_INTERNAL_SYMBOL(sxe2_class_driver_register)
void
sxe2_class_driver_register(struct sxe2_class_driver *driver)
{
	sxe2_common_driver_on_register_pci(driver);
	TAILQ_INSERT_TAIL(&sxe2_class_drivers_list, driver, next);
}

static void sxe2_common_pci_init(void)
{
	const struct rte_pci_id empty_table[] = {
		{
			.vendor_id = 0
		},
	};
	int32_t ret = -1;

	if (sxe2_common_pci_id_table == NULL) {
		ret = sxe2_common_pci_id_table_update(empty_table);
		if (ret != 0)
			goto l_end;
	}
	rte_pci_register(&sxe2_common_pci_driver);

l_end:
	return;
}

static bool sxe2_common_inited;

RTE_EXPORT_INTERNAL_SYMBOL(sxe2_common_init)
void
sxe2_common_init(void)
{
	if (sxe2_common_inited)
		goto l_end;

	pthread_mutex_init(&sxe2_common_devices_list_lock, NULL);
	sxe2_common_pci_init();
	sxe2_common_inited = true;

l_end:
	return;
}

RTE_FINI(sxe2_common_pci_finish)
{
	if (sxe2_common_pci_id_table != NULL) {
		rte_pci_unregister(&sxe2_common_pci_driver);
		free(sxe2_common_pci_id_table);
	}
}

RTE_PMD_EXPORT_NAME(sxe2_common_pci);

RTE_LOG_REGISTER_SUFFIX(sxe2_common_log, com, NOTICE);
