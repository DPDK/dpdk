/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */

#ifndef _VIRTIO_PCI_H_
#define _VIRTIO_PCI_H_

#include <stdint.h>

#include <rte_eal_paging.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_cryptodev.h>

#include "virtio_crypto.h"

struct virtqueue;

/* VirtIO PCI vendor/device ID. */
#define VIRTIO_CRYPTO_PCI_VENDORID 0x1AF4
#define VIRTIO_CRYPTO_PCI_DEVICEID 0x1054

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
				      * also clears the register (8, RO)
				      */
/* Only if MSIX is enabled: */

/* configuration change vector (16, RW) */
#define VIRTIO_MSI_CONFIG_VECTOR  20
/* vector for selected VQ notifications */
#define VIRTIO_MSI_QUEUE_VECTOR	  22

/* The bit of the ISR which indicates a device has an interrupt. */
#define VIRTIO_PCI_ISR_INTR   0x1
/* The bit of the ISR which indicates a device configuration change. */
#define VIRTIO_PCI_ISR_CONFIG 0x2
/* Vector value used to disable MSI for queue. */
#define VIRTIO_MSI_NO_VECTOR 0xFFFF

/* Status byte for guest to report progress. */
#define VIRTIO_CONFIG_STATUS_RESET     0x00
#define VIRTIO_CONFIG_STATUS_ACK       0x01
#define VIRTIO_CONFIG_STATUS_DRIVER    0x02
#define VIRTIO_CONFIG_STATUS_DRIVER_OK 0x04
#define VIRTIO_CONFIG_STATUS_FEATURES_OK 0x08
#define VIRTIO_CONFIG_STATUS_FAILED    0x80

/*
 * Each virtqueue indirect descriptor list must be physically contiguous.
 * To allow us to malloc(9) each list individually, limit the number
 * supported to what will fit in one page. With 4KB pages, this is a limit
 * of 256 descriptors. If there is ever a need for more, we can switch to
 * contigmalloc(9) for the larger allocations, similar to what
 * bus_dmamem_alloc(9) does.
 *
 * Note the sizeof(struct vring_desc) is 16 bytes.
 */
#define VIRTIO_MAX_INDIRECT ((int) (rte_mem_page_size() / 16))

/* Do we get callbacks when the ring is completely used, even if we've
 * suppressed them?
 */
#define VIRTIO_F_NOTIFY_ON_EMPTY	24

/* Can the device handle any descriptor layout? */
#define VIRTIO_F_ANY_LAYOUT		27

/* We support indirect buffer descriptors */
#define VIRTIO_RING_F_INDIRECT_DESC	28

#define VIRTIO_F_VERSION_1		32
#define VIRTIO_F_IOMMU_PLATFORM	33

/* The Guest publishes the used index for which it expects an interrupt
 * at the end of the avail ring. Host should ignore the avail->flags field.
 */
/* The Host publishes the avail index for which it expects a kick
 * at the end of the used ring. Guest should ignore the used->flags field.
 */
#define VIRTIO_RING_F_EVENT_IDX		29

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
	uint8_t cap_vndr;	/* Generic PCI field: PCI_CAP_ID_VNDR */
	uint8_t cap_next;	/* Generic PCI field: next ptr. */
	uint8_t cap_len;	/* Generic PCI field: capability length */
	uint8_t cfg_type;	/* Identifies the structure. */
	uint8_t bar;		/* Where to find it. */
	uint8_t padding[3];	/* Pad to full dword. */
	uint32_t offset;	/* Offset within bar. */
	uint32_t length;	/* Length of the structure, in bytes. */
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

struct virtio_crypto_hw;

struct virtio_pci_ops {
	void (*read_dev_cfg)(struct virtio_crypto_hw *hw, size_t offset,
			     void *dst, int len);
	void (*write_dev_cfg)(struct virtio_crypto_hw *hw, size_t offset,
			      const void *src, int len);
	void (*reset)(struct virtio_crypto_hw *hw);

	uint8_t (*get_status)(struct virtio_crypto_hw *hw);
	void (*set_status)(struct virtio_crypto_hw *hw, uint8_t status);

	uint64_t (*get_features)(struct virtio_crypto_hw *hw);
	void (*set_features)(struct virtio_crypto_hw *hw, uint64_t features);

	uint8_t (*get_isr)(struct virtio_crypto_hw *hw);

	uint16_t (*set_config_irq)(struct virtio_crypto_hw *hw, uint16_t vec);

	uint16_t (*set_queue_irq)(struct virtio_crypto_hw *hw,
			struct virtqueue *vq, uint16_t vec);

	uint16_t (*get_queue_num)(struct virtio_crypto_hw *hw,
			uint16_t queue_id);
	int (*setup_queue)(struct virtio_crypto_hw *hw, struct virtqueue *vq);
	void (*del_queue)(struct virtio_crypto_hw *hw, struct virtqueue *vq);
	void (*notify_queue)(struct virtio_crypto_hw *hw, struct virtqueue *vq);
};

struct virtio_crypto_hw {
	/* control queue */
	struct virtqueue *cvq;
	uint16_t    dev_id;
	uint16_t    max_dataqueues;
	uint64_t    req_guest_features;
	uint64_t    guest_features;
	uint8_t	    use_msix;
	uint8_t     modern;
	uint32_t    notify_off_multiplier;
	uint8_t     *isr;
	uint16_t    *notify_base;
	struct virtio_pci_common_cfg *common_cfg;
	struct virtio_crypto_config *dev_cfg;
	const struct rte_cryptodev_capabilities *virtio_dev_capabilities;
};

/*
 * While virtio_crypto_hw is stored in shared memory, this structure stores
 * some infos that may vary in the multiple process model locally.
 * For example, the vtpci_ops pointer.
 */
struct virtio_hw_internal {
	const struct virtio_pci_ops *vtpci_ops;
	struct rte_pci_ioport io;
};

#define VTPCI_OPS(hw)	(crypto_virtio_hw_internal[(hw)->dev_id].vtpci_ops)
#define VTPCI_IO(hw)	(&crypto_virtio_hw_internal[(hw)->dev_id].io)

extern struct virtio_hw_internal crypto_virtio_hw_internal[RTE_MAX_VIRTIO_CRYPTO];

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

static inline int
vtpci_with_feature(struct virtio_crypto_hw *hw, uint64_t bit)
{
	return (hw->guest_features & (1ULL << bit)) != 0;
}

/*
 * Function declaration from virtio_pci.c
 */
int vtpci_cryptodev_init(struct rte_pci_device *dev,
	struct virtio_crypto_hw *hw);
void vtpci_cryptodev_reset(struct virtio_crypto_hw *hw);

void vtpci_cryptodev_reinit_complete(struct virtio_crypto_hw *hw);

uint8_t vtpci_cryptodev_get_status(struct virtio_crypto_hw *hw);
void vtpci_cryptodev_set_status(struct virtio_crypto_hw *hw, uint8_t status);

uint64_t vtpci_cryptodev_negotiate_features(struct virtio_crypto_hw *hw,
	uint64_t host_features);

void vtpci_write_cryptodev_config(struct virtio_crypto_hw *hw, size_t offset,
	const void *src, int length);

void vtpci_read_cryptodev_config(struct virtio_crypto_hw *hw, size_t offset,
	void *dst, int length);

uint8_t vtpci_cryptodev_isr(struct virtio_crypto_hw *hw);

#endif /* _VIRTIO_PCI_H_ */
