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

#include <stdbool.h>

#include <rte_pci.h>
#include "rte_vhost.h"

#define MAX_VDPA_NAME_LEN 128

enum vdpa_addr_type {
	PCI_ADDR,
	VDPA_ADDR_MAX
};

/**
 * vdpa device address
 */
struct rte_vdpa_dev_addr {
	/** vdpa address type */
	enum vdpa_addr_type type;

	/** vdpa pci address */
	union {
		uint8_t __dummy[64];
		struct rte_pci_addr pci_addr;
	};
};

/**
 * vdpa device operations
 */
struct rte_vdpa_dev_ops {
	/** Get capabilities of this device */
	int (*get_queue_num)(int did, uint32_t *queue_num);

	/** Get supported features of this device */
	int (*get_features)(int did, uint64_t *features);

	/** Get supported protocol features of this device */
	int (*get_protocol_features)(int did, uint64_t *protocol_features);

	/** Driver configure/close the device */
	int (*dev_conf)(int vid);
	int (*dev_close)(int vid);

	/** Enable/disable this vring */
	int (*set_vring_state)(int vid, int vring, int state);

	/** Set features when changed */
	int (*set_features)(int vid);

	/** Destination operations when migration done */
	int (*migration_done)(int vid);

	/** Get the vfio group fd */
	int (*get_vfio_group_fd)(int vid);

	/** Get the vfio device fd */
	int (*get_vfio_device_fd)(int vid);

	/** Get the notify area info of the queue */
	int (*get_notify_area)(int vid, int qid,
			uint64_t *offset, uint64_t *size);

	/** Reserved for future extension */
	void *reserved[5];
};

/**
 * vdpa device structure includes device address and device operations.
 */
struct rte_vdpa_device {
	/** vdpa device address */
	struct rte_vdpa_dev_addr addr;
	/** vdpa device operations */
	struct rte_vdpa_dev_ops *ops;
} __rte_cache_aligned;

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Register a vdpa device
 *
 * @param addr
 *  the vdpa device address
 * @param ops
 *  the vdpa device operations
 * @return
 *  device id on success, -1 on failure
 */
int __rte_experimental
rte_vdpa_register_device(struct rte_vdpa_dev_addr *addr,
		struct rte_vdpa_dev_ops *ops);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Unregister a vdpa device
 *
 * @param did
 *  vdpa device id
 * @return
 *  device id on success, -1 on failure
 */
int __rte_experimental
rte_vdpa_unregister_device(int did);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Find the device id of a vdpa device
 *
 * @param addr
 *  the vdpa device address
 * @return
 *  device id on success, -1 on failure
 */
int __rte_experimental
rte_vdpa_find_device_id(struct rte_vdpa_dev_addr *addr);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Find a vdpa device based on device id
 *
 * @param did
 *  device id
 * @return
 *  rte_vdpa_device on success, NULL on failure
 */
struct rte_vdpa_device * __rte_experimental
rte_vdpa_get_device(int did);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Get current available vdpa device number
 *
 * @return
 *  available vdpa device number
 */
int __rte_experimental
rte_vdpa_get_device_num(void);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Enable/Disable host notifier mapping for a vdpa port.
 *
 * @param vid
 *  vhost device id
 * @param enable
 *  true for host notifier map, false for host notifier unmap
 * @return
 *  0 on success, -1 on failure
 */
int __rte_experimental
rte_vhost_host_notifier_ctrl(int vid, bool enable);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Synchronize the used ring from mediated ring to guest, log dirty
 * page for each writeable buffer, caller should handle the used
 * ring logging before device stop.
 *
 * @param vid
 *  vhost device id
 * @param qid
 *  vhost queue id
 * @param vring_m
 *  mediated virtio ring pointer
 * @return
 *  number of synced used entries on success, -1 on failure
 */
int __rte_experimental
rte_vdpa_relay_vring_used(int vid, uint16_t qid, void *vring_m);
#endif /* _RTE_VDPA_H_ */
