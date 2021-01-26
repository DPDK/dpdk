/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
#define VIRTIO_PCI_ISR		  19 /* interrupt status register, reading
				      * also clears the register (8, RO) */
/* Only if MSIX is enabled: */
#define VIRTIO_MSI_CONFIG_VECTOR  20 /* configuration change vector (16, RW) */
#define VIRTIO_MSI_QUEUE_VECTOR	  22 /* vector for selected VQ notifications
				      (16, RW) */

/* The bit of the ISR which indicates a device has an interrupt. */
#define VIRTIO_PCI_ISR_INTR   0x1
/* The bit of the ISR which indicates a device configuration change. */
#define VIRTIO_PCI_ISR_CONFIG 0x2
/* Vector value used to disable MSI for queue. */
#define VIRTIO_MSI_NO_VECTOR 0xFFFF

/* VirtIO device IDs. */
#define VIRTIO_ID_NETWORK  0x01
#define VIRTIO_ID_BLOCK    0x02
#define VIRTIO_ID_CONSOLE  0x03
#define VIRTIO_ID_ENTROPY  0x04
#define VIRTIO_ID_BALLOON  0x05
#define VIRTIO_ID_IOMEMORY 0x06
#define VIRTIO_ID_9P       0x09

/* Status byte for guest to report progress. */
#define VIRTIO_CONFIG_STATUS_RESET		0x00
#define VIRTIO_CONFIG_STATUS_ACK		0x01
#define VIRTIO_CONFIG_STATUS_DRIVER		0x02
#define VIRTIO_CONFIG_STATUS_DRIVER_OK		0x04
#define VIRTIO_CONFIG_STATUS_FEATURES_OK	0x08
#define VIRTIO_CONFIG_STATUS_DEV_NEED_RESET	0x40
#define VIRTIO_CONFIG_STATUS_FAILED		0x80


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
};

struct virtio_pci_dev {
	struct virtio_hw hw;
	struct rte_pci_device *pci_dev;
	struct virtio_pci_common_cfg *common_cfg;
	struct virtio_net_config *dev_cfg;
	uint8_t *isr;
	uint16_t *notify_base;
	uint32_t notify_off_multiplier;
	bool modern;
};

#define virtio_pci_get_dev(hwp) container_of(hwp, struct virtio_pci_dev, hw)

/*
 * This structure is just a reference to read
 * net device specific config space; it just a chodu structure
 *
 */
struct virtio_net_config {
	/* The config defining mac address (if VIRTIO_NET_F_MAC) */
	uint8_t    mac[RTE_ETHER_ADDR_LEN];
	/* See VIRTIO_NET_F_STATUS and VIRTIO_NET_S_* above */
	uint16_t   status;
	uint16_t   max_virtqueue_pairs;
	uint16_t   mtu;
	/*
	 * speed, in units of 1Mb. All values 0 to INT_MAX are legal.
	 * Any other value stands for unknown.
	 */
	uint32_t speed;
	/*
	 * 0x00 - half duplex
	 * 0x01 - full duplex
	 * Any other value stands for unknown.
	 */
	uint8_t duplex;

} __rte_packed;

/*
 * How many bits to shift physical queue address written to QUEUE_PFN.
 * 12 is historical, and due to x86 page size.
 */
#define VIRTIO_PCI_QUEUE_ADDR_SHIFT 12

/* The alignment to use between consumer and producer parts of vring. */
#define VIRTIO_PCI_VRING_ALIGN 4096

enum virtio_msix_status {
	VIRTIO_MSIX_NONE = 0,
	VIRTIO_MSIX_DISABLED = 1,
	VIRTIO_MSIX_ENABLED = 2
};


/*
 * Function declaration from virtio_pci.c
 */
int vtpci_init(struct rte_pci_device *pci_dev, struct virtio_pci_dev *dev);
void vtpci_reset(struct virtio_hw *);

void vtpci_reinit_complete(struct virtio_hw *);

uint8_t vtpci_get_status(struct virtio_hw *);
void vtpci_set_status(struct virtio_hw *, uint8_t);

void vtpci_write_dev_config(struct virtio_hw *, size_t, const void *, int);

void vtpci_read_dev_config(struct virtio_hw *, size_t, void *, int);

uint8_t vtpci_isr(struct virtio_hw *);

void vtpci_legacy_ioport_unmap(struct virtio_hw *hw);
int vtpci_legacy_ioport_map(struct virtio_hw *hw);

extern const struct virtio_ops legacy_ops;
extern const struct virtio_ops modern_ops;

#endif /* _VIRTIO_PCI_H_ */
