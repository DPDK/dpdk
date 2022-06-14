/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef _VIRTIO_H_
#define _VIRTIO_H_
#include "virtio_net.h"
#include "virtio_blk.h"

/*
 * Do we get callbacks when the ring is completely used,
 * even if we've suppressed them?
 */
#define VIRTIO_F_NOTIFY_ON_EMPTY	24

/* Can the device handle any descriptor layout? */
#define VIRTIO_F_ANY_LAYOUT		27

/* We support indirect buffer descriptors */
#define VIRTIO_RING_F_INDIRECT_DESC	28

#define VIRTIO_F_VERSION_1		32
#ifndef VIRTIO_F_IOMMU_PLATFORM
#define VIRTIO_F_IOMMU_PLATFORM	33
#endif
#define VIRTIO_F_RING_PACKED		34

/*
 * Some VirtIO feature bits (currently bits 28 through 31) are
 * reserved for the transport being used (eg. virtio_ring), the
 * rest are per-device feature bits.
 */
#define VIRTIO_TRANSPORT_F_START 28
#ifndef VIRTIO_TRANSPORT_F_END
#define VIRTIO_TRANSPORT_F_END   34
#endif
/*
 * Inorder feature indicates that all buffers are used by the device
 * in the same order in which they have been made available.
 */
#define VIRTIO_F_IN_ORDER 35

/*
 * This feature indicates that memory accesses by the driver and the device
 * are ordered in a way described by the platform.
 */
#define VIRTIO_F_ORDER_PLATFORM 36

/*
 * This feature indicates that the driver passes extra data (besides
 * identifying the virtqueue) in its device notifications.
 */
#define VIRTIO_F_NOTIFICATION_DATA 38

/*
 * The Guest publishes the used index for which it expects an interrupt
 * at the end of the avail ring. Host should ignore the avail->flags field
 *
 * The Host publishes the avail index for which it expects a kick
 * at the end of the used ring. Guest should ignore the used->flags field.
 */
#define VIRTIO_RING_F_EVENT_IDX		29

/*
 * When VIRTIO_F_RING_RESET is negotiated, the driver can reset a virtqueue
 * individually. The way to reset the virtqueue is transport specific.
 * Virtqueue reset is divided into two parts. The driver first resets a queue
 * and can afterwards optionally reenable it.
 */
#define VIRTIO_F_RING_RESET 40

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
#define VIRTIO_MAX_INDIRECT ((int)(rte_mem_page_size() / 16))

/* VirtIO device IDs. */
#define VIRTIO_ID_NETWORK  0x01
#ifndef VIRTIO_ID_BLOCK
#define VIRTIO_ID_BLOCK    0x02
#endif
#ifndef VIRTIO_ID_CONSOLE
#define VIRTIO_ID_CONSOLE  0x03
#endif
#define VIRTIO_ID_ENTROPY  0x04
#ifndef VIRTIO_ID_BALLOON
#define VIRTIO_ID_BALLOON  0x05
#endif
#define VIRTIO_ID_IOMEMORY 0x06
#ifndef VIRTIO_ID_9P
#define VIRTIO_ID_9P       0x09
#endif

#define VP_DEV_NAME(vpdev) ((vpdev)->hw.pci_dev->device.name)

/* The bit of the ISR which indicates a device has an interrupt. */
#define VIRTIO_ISR_INTR   0x1
/* The bit of the ISR which indicates a device configuration change. */
#define VIRTIO_ISR_CONFIG 0x2
/* Vector value used to disable MSI for queue. */
#define VIRTIO_MSI_NO_VECTOR 0xFFFF

/* The alignment to use between consumer and producer parts of vring. */
#define VIRTIO_VRING_ALIGN 4096

struct virtadmin_ctl {
	/**< memzone to populate hdr. */
	const struct rte_memzone *virtio_admin_hdr_mz;
	rte_iova_t virtio_admin_hdr_mem;/**< hdr for each xmit packet */
	uint16_t port_id;	       /**< Device port identifier. */
	const struct rte_memzone *mz;   /**< mem zone to populate CTL ring. */
	rte_spinlock_t lock;	    /**< spinlock for control queue. */
};

struct virtio_hw {
	struct virtqueue **vqs;
	uint64_t guest_features;
	uint8_t weak_barriers;
	uint8_t intr_lsc;
	uint16_t max_queue_pairs;
	uint16_t num_queues_blk;
	struct virtnet_ctl *cvq;
	struct virtadmin_ctl *avq;
	struct rte_pci_device *pci_dev;
	const struct virtio_ops *virtio_ops;
	const struct virtio_dev_specific_ops *virtio_dev_sp_ops;
	struct rte_pci_ioport io;
};

struct virtio_ops {
	void (*read_dev_cfg)(struct virtio_hw *hw, size_t offset, void *dst, int len);
	void (*write_dev_cfg)(struct virtio_hw *hw, size_t offset, const void *src, int len);
	uint8_t (*get_status)(struct virtio_hw *hw);
	void (*set_status)(struct virtio_hw *hw, uint8_t status);
	uint64_t (*get_features)(struct virtio_hw *hw);
	void (*set_features)(struct virtio_hw *hw, uint64_t features);
	int (*features_ok)(struct virtio_hw *hw);
	uint8_t (*get_isr)(struct virtio_hw *hw);
	uint16_t (*set_config_irq)(struct virtio_hw *hw, uint16_t vec);
	uint16_t (*set_queue_irq)(struct virtio_hw *hw, struct virtqueue *vq, uint16_t vec);
	uint16_t (*get_queue_num)(struct virtio_hw *hw);
	uint16_t (*get_queue_size)(struct virtio_hw *hw, uint16_t queue_id);
	int (*setup_queue)(struct virtio_hw *hw, struct virtqueue *vq);
	void (*del_queue)(struct virtio_hw *hw, struct virtqueue *vq);
	void (*notify_queue)(struct virtio_hw *hw, struct virtqueue *vq);
	void (*intr_detect)(struct virtio_hw *hw);
	int (*dev_close)(struct virtio_hw *hw);
};

struct virtio_dev_specific_ops {
	uint16_t (*get_queue_num)(struct virtio_hw *hw);
};

/*
 * This structure stores per-process data. Only virtio_ops for now.
 */
struct virtio_hw_internal {
	const struct virtio_ops *virtio_ops;
};

#define VIRTIO_OPS(hw)	((hw)->virtio_ops)

static inline int
virtio_with_feature(struct virtio_hw *hw, uint64_t bit)
{
	return (hw->guest_features & (1ULL << bit)) != 0;
}

static inline int
virtio_with_packed_queue(struct virtio_hw *hw)
{
	return virtio_with_feature(hw, VIRTIO_F_RING_PACKED);
}

uint64_t virtio_pci_dev_negotiate_features(struct virtio_hw *hw, uint64_t host_features);
uint8_t virtio_pci_dev_isr_get(struct virtio_hw *hw);
#endif /* _VIRTIO_H_ */
