/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_COMMON_H
#define SXE2_COMMON_H

#include <rte_bitops.h>
#include <rte_kvargs.h>
#include <rte_compat.h>
#include <rte_memory.h>
#include <rte_ticketlock.h>
#include <pthread.h>

#define SXE2_COMMON_PCI_DRIVER_NAME "sxe2_pci"

#define SXE2_CMD_FD_INVALID (-1)

#define SXE2_CDEV_TO_CMD_FD(cdev) \
	((cdev)->config.cmd_fd)

#define SXE2_DEVARGS_KEY_CLASS "class"
#define SXE2_DEVARGS_KEY_DRIVER "driver"
#define SXE2_DEVARGS_KEY_REPR "representor"

struct sxe2_class_driver;

enum sxe2_class_type {
	SXE2_CLASS_TYPE_ETH = 0,
	SXE2_CLASS_TYPE_VDPA,
	SXE2_CLASS_TYPE_INVALID,
};

struct sxe2_common_dev_config {
	int32_t cmd_fd;
	bool support_iommu;
	bool kernel_reset;
	pthread_mutex_t lock;
};

struct sxe2_common_device {
	struct rte_device *dev;
	TAILQ_ENTRY(sxe2_common_device) next;
	struct sxe2_class_driver *cdrv;
	enum sxe2_class_type class_type;
	struct sxe2_common_dev_config config;
	struct sxe2_dev_kvargs_info *kvargs;
};

struct sxe2_dev_kvargs_info {
	struct rte_kvargs *kvlist;
	bool is_used[RTE_KVARGS_MAX];
};

typedef int32_t (sxe2_class_driver_probe_t)(struct sxe2_common_device *scdev,
					struct sxe2_dev_kvargs_info *kvargs);

typedef int32_t (sxe2_class_driver_remove_t)(struct sxe2_common_device *scdev);

struct sxe2_class_driver {
	TAILQ_ENTRY(sxe2_class_driver) next;
	enum sxe2_class_type drv_class;
	const char *name;
	sxe2_class_driver_probe_t *probe;
	sxe2_class_driver_remove_t *remove;
	const struct rte_pci_id *id_table;
	uint32_t intr_lsc;
	uint32_t intr_rmv;
};

__rte_internal
void
sxe2_common_mem_event_cb(enum rte_mem_event type,
		const void *addr, size_t size, void *arg __rte_unused);

__rte_internal
void
sxe2_class_driver_register(struct sxe2_class_driver *driver);

__rte_internal
void
sxe2_common_init(void);

__rte_internal
int32_t
sxe2_kvargs_process(struct sxe2_dev_kvargs_info *kv_info,
		const char *const key_match, arg_handler_t handler, void *opaque_arg);

#endif /* SXE2_COMMON_H */
