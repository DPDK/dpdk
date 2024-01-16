/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef _VIRTIO_PCI_H_
#define _VIRTIO_PCI_H_

#include <stdint.h>
#include <stdbool.h>

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <ethdev_driver.h>

#include "virtio.h"

struct virtqueue;
struct virtnet_ctl;

/* VirtIO PCI vendor/device ID. */
#define VIRTIO_PCI_VENDORID     0x1AF4
#define VIRTIO_PCI_LEGACY_DEVICEID_NET 0x1000
#define VIRTIO_PCI_MODERN_DEVICEID_NET 0x1041
#define VIRTIO_PCI_MODERN_DEVICEID_BLK 0x1042

/* VirtIO ABI version, this must match exactly. */
#define VIRTIO_PCI_ABI_VERSION 0

/*
 * VirtIO Header, located in BAR 0.
 */
#define VIRTIO_PCI_HOST_FEATURES  0  /* host's supported features (32bit, RO)*/
#define VIRTIO_PCI_GUEST_FEATURES 4  /* guest's supported features (32, RW) */
#define VIRTIO_PCI_QUEUE_PFN      8  /* physical address of VQ (32, RW) */
#define VIRTIO_PCI_QUEUE_NUM      12 /* number of ring entries (16, RO) */
#define VIRTIO_PCI_QUEUE_SEL      14 /* current VQ selection (16, RW) */
#define VIRTIO_PCI_QUEUE_NOTIFY   16 /* notify host regarding VQ (16, RW) */
#define VIRTIO_PCI_STATUS         18 /* device status register (8, RW) */
#define VIRTIO_PCI_ISR            19 /* interrupt status register, reading
				      * also clears the register (8, RO) */
/* Only if MSIX is enabled: */
#define VIRTIO_MSI_CONFIG_VECTOR  20 /* configuration change vector (16, RW) */
#define VIRTIO_MSI_QUEUE_VECTOR	  22 /* vector for selected VQ notifications
				      (16, RW) */

/* Common configuration */
#define VIRTIO_PCI_CAP_COMMON_CFG	1
/* Notifications */
#define VIRTIO_PCI_CAP_NOTIFY_CFG	2
/* ISR Status */
#define VIRTIO_PCI_CAP_ISR_CFG		3
/* Device specific configuration */
#define VIRTIO_PCI_CAP_DEVICE_CFG	4
/* PCI configuration access */
#define VIRTIO_PCI_CAP_PCI_CFG		5

/* This is the PCI capability header: */
struct virtio_pci_cap {
	uint8_t cap_vndr;		/* Generic PCI field: PCI_CAP_ID_VNDR */
	uint8_t cap_next;		/* Generic PCI field: next ptr. */
	uint8_t cap_len;		/* Generic PCI field: capability length */
	uint8_t cfg_type;		/* Identifies the structure. */
	uint8_t bar;			/* Where to find it. */
	uint8_t padding[3];		/* Pad to full dword. */
	uint32_t offset;		/* Offset within bar. */
	uint32_t length;		/* Length of the structure, in bytes. */
};

struct virtio_pci_notify_cap {
	struct virtio_pci_cap cap;
	uint32_t notify_off_multiplier;	/* Multiplier for queue_notify_off. */
};

/* Fields in VIRTIO_PCI_CAP_COMMON_CFG: */
struct virtio_pci_common_cfg {
	/* About the whole device. */
	uint32_t device_feature_select;	/* read-write */
	uint32_t device_feature;	/* read-only */
	uint32_t guest_feature_select;	/* read-write */
	uint32_t guest_feature;		/* read-write */
	uint16_t msix_config;		/* read-write */
	uint16_t num_queues;		/* read-only */
	uint8_t device_status;		/* read-write */
	uint8_t config_generation;	/* read-only */

	/* About a specific virtqueue. */
	uint16_t queue_select;		/* read-write */
	uint16_t queue_size;		/* read-write, power of 2. */
	uint16_t queue_msix_vector;	/* read-write */
	uint16_t queue_enable;		/* read-write */
	uint16_t queue_notify_off;	/* read-only */
	uint32_t queue_desc_lo;		/* read-write */
	uint32_t queue_desc_hi;		/* read-write */
	uint32_t queue_avail_lo;	/* read-write */
	uint32_t queue_avail_hi;	/* read-write */
	uint32_t queue_used_lo;		/* read-write */
	uint32_t queue_used_hi;		/* read-write */
	uint16_t queue_notify_data;		/* read-only for driver */
	uint16_t queue_reset;		/* read-write */
};

enum virtio_msix_status {
	VIRTIO_MSIX_NONE = 0,
	VIRTIO_MSIX_DISABLED = 1,
	VIRTIO_MSIX_ENABLED = 2
};

/* PCI-specific Admin command set */
#define VIRTIO_ADMIN_PCI_MIGRATION_CTRL 64
#define VIRTIO_ADMIN_PCI_MIGRATION_IDENTITY 0
#define VIRTIO_ADMIN_PCI_MIGRATION_GET_INTERNAL_STATUS 1
#define VIRTIO_ADMIN_PCI_MIGRATION_MODIFY_INTERNAL_STATUS 2
#define VIRTIO_ADMIN_PCI_MIGRATION_GET_INTERNAL_STATE_PENDING_BYTES 3
#define VIRTIO_ADMIN_PCI_MIGRATION_SAVE_INTERNAL_STATE 4
#define VIRTIO_ADMIN_PCI_MIGRATION_RESTORE_INTERNAL_STATE 5

#define VIRTIO_ADMIN_PCI_DIRTY_PAGE_TRACK_CTRL 65
#define VIRTIO_ADMIN_PCI_DIRTY_PAGE_IDENTITY 0
#define VIRTIO_ADMIN_PCI_DIRTY_PAGE_START_TRACK 1
#define VIRTIO_ADMIN_PCI_DIRTY_PAGE_STOP_TRACK 2
#define VIRTIO_ADMIN_PCI_DIRTY_PAGE_GET_MAP_PENDING_BYTES 3
#define VIRTIO_ADMIN_PCI_DIRTY_PAGE_REPORT_MAP 4

enum virtio_internal_status {
	/* Reset occurred. The device is in initial status. aka FLR state */
	VIRTIO_S_INIT = 0,
	/* The device is running (unquiesced and unfreezed) */
	VIRTIO_S_RUNNING = 1,
	/*
	* The device has been quiesced (Internal state can be changed.
	* Can't master transactions.)
	*/
	VIRTIO_S_QUIESCED = 2,
	/*
	* The device has been freezed (Internal state can't be changed.
	* Can't master transactions.)
	*/
	VIRTIO_S_FREEZED = 3,
};

struct virtio_admin_migration_identity_result {
	uint16_t major_ver;
	uint16_t minor_ver;
	uint16_t ter_ver;
	uint16_t reserved;
};

struct virtio_admin_migration_get_internal_state_pending_bytes_data {
	uint16_t vdev_id;
	uint16_t reserved;
};

struct virtio_admin_migration_get_internal_state_pending_bytes_result {
	uint64_t pending_bytes;
};

struct virtio_admin_migration_modify_internal_status_data {
	uint16_t vdev_id;
	uint16_t internal_status; /* Value from enum virtio_internal_status */
};

struct virtio_admin_migration_save_internal_state_data {
	uint16_t vdev_id;
	uint16_t reserved[3];
	uint64_t offset;
	uint64_t length; /* Num of data bytes to be returned by the device */
};

struct virtio_admin_migration_restore_internal_state_data {
	uint16_t vdev_id;
	uint16_t reserved;
	uint64_t offset;
	uint64_t length; /* Num of data bytes to be consumed by the device */
	uint8_t data[];
};

struct virtio_admin_migration_get_internal_status_data {
	uint16_t vdev_id;
	uint16_t reserved;
};

struct virtio_admin_migration_get_internal_status_result {
	uint16_t internal_status; /* Value from enum virtio_internal_status */
	uint16_t reserved;
};

struct virtio_admin_dirty_page_identity_result {
	uint16_t log_max_pages_track_pull_bitmap_mode; /* Per managed device (log) */
	uint16_t log_max_pages_track_pull_bytemap_mode; /* Per managed device (log) */
	uint32_t max_track_ranges; /* Maximum number of ranges a device can track */
};

enum virtio_dirty_track_mode {
	VIRTIO_M_DIRTY_TRACK_PUSH_BITMAP = 1, /* Use push mode with bit granularity */
	VIRTIO_M_DIRTY_TRACK_PUSH_BYTEMAP = 2, /* Use push mode with byte granularity */
	VIRTIO_M_DIRTY_TRACK_PULL_BITMAP = 3, /* Use pull mode with bit granularity */
	VIRTIO_M_DIRTY_TRACK_PULL_BYTEMAP = 4, /* Use pull mode with byte granularity */
};

struct virtio_sge {
	uint64_t addr;
	uint32_t len;
	uint32_t reserved;
};

struct virtio_admin_dirty_page_start_track_data {
	uint16_t vdev_id;
	uint16_t track_mode; /* value from enum virtio_dirty_track_mode. must be a negotiated value */
	uint32_t vdev_host_page_size; /* page size of the migrated device host */
	uint64_t vdev_host_range_addr; /* Range start address */
	uint64_t range_length; /* Range length */
	struct virtio_sge sges[]; /* Push modes only */
};

struct virtio_admin_dirty_page_stop_track_data {
	uint16_t vdev_id;
	uint16_t reserved[3];
	uint64_t vdev_host_range_addr; /* Range start address (must be same address given in the
		start tracking) */
};

struct virtio_admin_dirty_page_get_map_pending_bytes_data {
	uint16_t vdev_id;
	uint16_t reserved[3];
	uint64_t vdev_host_range_addr; /* Range start address (must be same address given in
		the start tracking) */
};

struct virtio_admin_dirty_page_get_map_pending_bytes_result {
	uint64_t pending_bytes;
};

struct virtio_admin_dirty_page_report_map_data {
	uint16_t vdev_id;
	uint16_t reserved[3];
	uint64_t offset;
	uint64_t length; /* Num of data bytes to be returned by the device */
	uint64_t vdev_host_range_addr; /* Range start address (must be same address given in the
		start tracking) */
};

struct virtio_pci_dev {
	struct virtio_hw hw;
	struct virtio_pci_common_cfg *common_cfg;
	struct virtio_net_config *dev_cfg;
	enum virtio_msix_status msix_status;
	uint8_t *isr;
	uint16_t *notify_base;
	uint32_t notify_off_multiplier;
	int vfio_dev_fd;
	uint8_t notify_bar;
	bool modern;
};

#define virtio_pci_get_dev(hwp) container_of(hwp, struct virtio_pci_dev, hw)

struct virtio_pci_internal {
	struct rte_pci_ioport io;
	struct rte_pci_device *dev;
};

#define VTPCI_IO(hw) (&((hw)->io))
#define VTPCI_DEV(hw) ((hw)->pci_dev)

/*
 * How many bits to shift physical queue address written to QUEUE_PFN.
 * 12 is historical, and due to x86 page size.
 */
#define VIRTIO_PCI_QUEUE_ADDR_SHIFT 12

/*
 * Function declaration from virtio_pci.c
 */
int virtio_pci_dev_init(struct rte_pci_device *pci_dev, struct virtio_pci_dev *dev, int dev_fd);
void virtio_pci_dev_legacy_ioport_unmap(struct virtio_hw *hw);
int virtio_pci_dev_legacy_ioport_map(struct virtio_hw *hw);

extern const struct virtio_ops legacy_ops;
extern const struct virtio_ops modern_ops;

#endif /* _VIRTIO_PCI_H_ */
