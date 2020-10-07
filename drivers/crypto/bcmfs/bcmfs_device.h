/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Broadcom.
 * All rights reserved.
 */

#ifndef _BCMFS_DEVICE_H_
#define _BCMFS_DEVICE_H_

#include <sys/queue.h>

#include <rte_spinlock.h>
#include <rte_bus_vdev.h>

#include "bcmfs_logs.h"
#include "bcmfs_qp.h"

/* max number of dev nodes */
#define BCMFS_MAX_NODES		4
#define BCMFS_MAX_PATH_LEN	512
#define BCMFS_DEV_NAME_LEN	64

/* Path for BCM-Platform device directory */
#define SYSFS_BCM_PLTFORM_DEVICES    "/sys/bus/platform/devices"

#define BCMFS_SYM_FS4_VERSION	0x76303031
#define BCMFS_SYM_FS5_VERSION	0x76303032

/* Supported devices */
enum bcmfs_device_type {
	BCMFS_SYM_FS4,
	BCMFS_SYM_FS5,
	BCMFS_UNKNOWN
};

/* A table to store registered queue pair opertations */
struct bcmfs_hw_queue_pair_ops_table {
	rte_spinlock_t tl;
	/* Number of used ops structs in the table. */
	uint32_t num_ops;
	 /*  Storage for all possible ops structs. */
	struct bcmfs_hw_queue_pair_ops qp_ops[BCMFS_MAX_NODES];
};

/* HW queue pair ops register function */
int
bcmfs_hw_queue_pair_register_ops(const struct bcmfs_hw_queue_pair_ops *qp_ops);

struct bcmfs_device {
	TAILQ_ENTRY(bcmfs_device) next;
	/* Directory path for vfio */
	char dirname[BCMFS_MAX_PATH_LEN];
	/* BCMFS device name */
	char name[BCMFS_DEV_NAME_LEN];
	/* Parent vdev */
	struct rte_vdev_device *vdev;
	/* vfio handle */
	int vfio_dev_fd;
	/* mapped address */
	uint8_t *mmap_addr;
	/* mapped size */
	uint32_t mmap_size;
	/* max number of h/w queue pairs detected */
	uint16_t max_hw_qps;
	/* current qpairs in use */
	struct bcmfs_qp *qps_in_use[BCMFS_MAX_HW_QUEUES];
	/* queue pair ops exported by symmetric crypto hw */
	struct bcmfs_hw_queue_pair_ops *sym_hw_qp_ops;
	/* a cryptodevice attached to bcmfs device */
	struct rte_cryptodev *cdev;
	/* a rte_device to register with cryptodev */
	struct rte_device sym_rte_dev;
	/* private info to keep with cryptodev */
	struct bcmfs_sym_dev_private *sym_dev;
};

#endif /* _BCMFS_DEVICE_H_ */
