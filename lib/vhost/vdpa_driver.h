/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _VDPA_DRIVER_H_
#define _VDPA_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include <rte_compat.h>

#include "rte_vhost.h"
#include "rte_vdpa.h"

#define RTE_VHOST_QUEUE_ALL UINT16_MAX

/**
 * vdpa device operations
 */
struct rte_vdpa_dev_ops {
	/** Get capabilities of this device (Mandatory) */
	int (*get_queue_num)(struct rte_vdpa_device *dev, uint32_t *queue_num);

	/** Get supported features of this device (Mandatory) */
	int (*get_features)(struct rte_vdpa_device *dev, uint64_t *features);

	/** Get supported protocol features of this device (Mandatory) */
	int (*get_protocol_features)(struct rte_vdpa_device *dev,
			uint64_t *protocol_features);

	/** Driver configure the device (Mandatory) */
	int (*dev_conf)(int vid);

	/** Driver close the device (Mandatory) */
	int (*dev_close)(int vid);

	/** Enable/disable this vring (Mandatory) */
	int (*set_vring_state)(int vid, int vring, int state);

	/** Set features when changed (Mandatory) */
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

	/** Get statistics name */
	int (*get_stats_names)(struct rte_vdpa_device *dev,
			struct rte_vdpa_stat_name *stats_names,
			unsigned int size);

	/** Get statistics of the queue */
	int (*get_stats)(struct rte_vdpa_device *dev, int qid,
			struct rte_vdpa_stat *stats, unsigned int n);

	/** Reset statistics of the queue */
	int (*reset_stats)(struct rte_vdpa_device *dev, int qid);

	/** Reserved for future extension */
	void *reserved[2];
};

/**
 * vdpa device structure includes device address and device operations.
 */
struct rte_vdpa_device {
	RTE_TAILQ_ENTRY(rte_vdpa_device) next;
	/** Generic device information */
	struct rte_device *device;
	/** vdpa device operations */
	struct rte_vdpa_dev_ops *ops;
};

/**
 * Register a vdpa device
 *
 * @param rte_dev
 *  the generic device pointer
 * @param ops
 *  the vdpa device operations
 * @return
 *  vDPA device pointer on success, NULL on failure
 */
__rte_internal
struct rte_vdpa_device *
rte_vdpa_register_device(struct rte_device *rte_dev,
		struct rte_vdpa_dev_ops *ops);

/**
 * Unregister a vdpa device
 *
 * @param dev
 *  vDPA device pointer
 * @return
 *  device id on success, -1 on failure
 */
__rte_internal
int
rte_vdpa_unregister_device(struct rte_vdpa_device *dev);

/**
 * Enable/Disable host notifier mapping for a vdpa port.
 *
 * @param vid
 *  vhost device id
 * @param enable
 *  true for host notifier map, false for host notifier unmap
 * @param qid
 *  vhost queue id, RTE_VHOST_QUEUE_ALL to configure all the device queues
 * @return
 *  0 on success, -1 on failure
 */
__rte_internal
int
rte_vhost_host_notifier_ctrl(int vid, uint16_t qid, bool enable);

/**
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
__rte_internal
int
rte_vdpa_relay_vring_used(int vid, uint16_t qid, void *vring_m);

#ifdef __cplusplus
}
#endif

#endif /* _VDPA_DRIVER_H_ */
