/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _RTE_VDPA_H_
#define _RTE_VDPA_H_

/**
 * @file
 *
 * Device specific vhost lib
 */

#include <rte_pci.h>
#include "rte_vhost.h"

#define MAX_VDPA_NAME_LEN 128

enum vdpa_addr_type {
	PCI_ADDR,
	VDPA_ADDR_MAX
};

struct rte_vdpa_dev_addr {
	enum vdpa_addr_type type;
	union {
		uint8_t __dummy[64];
		struct rte_pci_addr pci_addr;
	};
};

struct rte_vdpa_dev_ops {
	/* Get capabilities of this device */
	int (*get_queue_num)(int did, uint32_t *queue_num);
	int (*get_features)(int did, uint64_t *features);
	int (*get_protocol_features)(int did, uint64_t *protocol_features);

	/* Driver configure/close the device */
	int (*dev_conf)(int vid);
	int (*dev_close)(int vid);

	/* Enable/disable this vring */
	int (*set_vring_state)(int vid, int vring, int state);

	/* Set features when changed */
	int (*set_features)(int vid);

	/* Destination operations when migration done */
	int (*migration_done)(int vid);

	/* Get the vfio group fd */
	int (*get_vfio_group_fd)(int vid);

	/* Get the vfio device fd */
	int (*get_vfio_device_fd)(int vid);

	/* Get the notify area info of the queue */
	int (*get_notify_area)(int vid, int qid,
			uint64_t *offset, uint64_t *size);

	/* Reserved for future extension */
	void *reserved[5];
};

struct rte_vdpa_device {
	struct rte_vdpa_dev_addr addr;
	struct rte_vdpa_dev_ops *ops;
} __rte_cache_aligned;

/* Register a vdpa device, return did if successful, -1 on failure */
int __rte_experimental
rte_vdpa_register_device(struct rte_vdpa_dev_addr *addr,
		struct rte_vdpa_dev_ops *ops);

/* Unregister a vdpa device, return -1 on failure */
int __rte_experimental
rte_vdpa_unregister_device(int did);

/* Find did of a vdpa device, return -1 on failure */
int __rte_experimental
rte_vdpa_find_device_id(struct rte_vdpa_dev_addr *addr);

/* Find a vdpa device based on did */
struct rte_vdpa_device * __rte_experimental
rte_vdpa_get_device(int did);

#endif /* _RTE_VDPA_H_ */
