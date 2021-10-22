/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _VIRTIO_PCI_H_
#define _VIRTIO_PCI_H_

#include <stdint.h>
#include <stdbool.h>

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ethdev_driver.h>

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
#define VIRTIO_MAX_INDIRECT ((int) (PAGE_SIZE / 16))

/* The feature bitmap for virtio net */
#define VIRTIO_NET_F_CSUM	0	/* Host handles pkts w/ partial csum */
#define VIRTIO_NET_F_GUEST_CSUM	1	/* Guest handles pkts w/ partial csum */
#define VIRTIO_NET_F_MTU	3	/* Initial MTU advice. */
#define VIRTIO_NET_F_MAC	5	/* Host has given MAC address. */
#define VIRTIO_NET_F_GUEST_TSO4	7	/* Guest can handle TSOv4 in. */
#define VIRTIO_NET_F_GUEST_TSO6	8	/* Guest can handle TSOv6 in. */
#define VIRTIO_NET_F_GUEST_ECN	9	/* Guest can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_GUEST_UFO	10	/* Guest can handle UFO in. */
#define VIRTIO_NET_F_HOST_TSO4	11	/* Host can handle TSOv4 in. */
#define VIRTIO_NET_F_HOST_TSO6	12	/* Host can handle TSOv6 in. */
#define VIRTIO_NET_F_HOST_ECN	13	/* Host can handle TSO[6] w/ ECN in. */
#define VIRTIO_NET_F_HOST_UFO	14	/* Host can handle UFO in. */
#define VIRTIO_NET_F_MRG_RXBUF	15	/* Host can merge receive buffers. */
#define VIRTIO_NET_F_STATUS	16	/* virtio_net_config.status available */
#define VIRTIO_NET_F_CTRL_VQ	17	/* Control channel available */
#define VIRTIO_NET_F_CTRL_RX	18	/* Control channel RX mode support */
#define VIRTIO_NET_F_CTRL_VLAN	19	/* Control channel VLAN filtering */
#define VIRTIO_NET_F_CTRL_RX_EXTRA 20	/* Extra RX mode control support */
#define VIRTIO_NET_F_GUEST_ANNOUNCE 21	/* Guest can announce device on the
					 * network */
#define VIRTIO_NET_F_MQ		22	/* Device supports Receive Flow
					 * Steering */
#define VIRTIO_NET_F_CTRL_MAC_ADDR 23	/* Set MAC address */

/* Do we get callbacks when the ring is completely used, even if we've
 * suppressed them? */
#define VIRTIO_F_NOTIFY_ON_EMPTY	24

/* Can the device handle any descriptor layout? */
#define VIRTIO_F_ANY_LAYOUT		27

/* We support indirect buffer descriptors */
#define VIRTIO_RING_F_INDIRECT_DESC	28

#define VIRTIO_F_VERSION_1		32
#define VIRTIO_F_IOMMU_PLATFORM	33
#define VIRTIO_F_RING_PACKED		34

/*
 * Some VirtIO feature bits (currently bits 28 through 31) are
 * reserved for the transport being used (eg. virtio_ring), the
 * rest are per-device feature bits.
 */
#define VIRTIO_TRANSPORT_F_START 28
#define VIRTIO_TRANSPORT_F_END   34

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

/* Device set linkspeed and duplex */
#define VIRTIO_NET_F_SPEED_DUPLEX 63

/* The Guest publishes the used index for which it expects an interrupt
 * at the end of the avail ring. Host should ignore the avail->flags field. */
/* The Host publishes the avail index for which it expects a kick
 * at the end of the used ring. Guest should ignore the used->flags field. */
#define VIRTIO_RING_F_EVENT_IDX		29

#define VIRTIO_NET_S_LINK_UP	1	/* Link is up */
#define VIRTIO_NET_S_ANNOUNCE	2	/* Announcement is needed */

/*
 * Maximum number of virtqueues per device.
 */
#define VIRTIO_MAX_VIRTQUEUE_PAIRS 8
#define VIRTIO_MAX_VIRTQUEUES (VIRTIO_MAX_VIRTQUEUE_PAIRS * 2 + 1)

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

struct virtio_hw;

struct virtio_pci_ops {
	void (*read_dev_cfg)(struct virtio_hw *hw, size_t offset,
			     void *dst, int len);
	void (*write_dev_cfg)(struct virtio_hw *hw, size_t offset,
			      const void *src, int len);

	uint8_t (*get_status)(struct virtio_hw *hw);
	void    (*set_status)(struct virtio_hw *hw, uint8_t status);

	uint64_t (*get_features)(struct virtio_hw *hw);
	void     (*set_features)(struct virtio_hw *hw, uint64_t features);

	uint8_t (*get_isr)(struct virtio_hw *hw);

	uint16_t (*set_config_irq)(struct virtio_hw *hw, uint16_t vec);

	uint16_t (*set_queue_irq)(struct virtio_hw *hw, struct virtqueue *vq,
			uint16_t vec);

	uint16_t (*get_queue_num)(struct virtio_hw *hw, uint16_t queue_id);
	int (*setup_queue)(struct virtio_hw *hw, struct virtqueue *vq);
	void (*del_queue)(struct virtio_hw *hw, struct virtqueue *vq);
	void (*notify_queue)(struct virtio_hw *hw, struct virtqueue *vq);
};

struct virtio_net_config;

struct virtio_hw {
	struct virtnet_ctl *cvq;
	uint64_t    req_guest_features;
	uint64_t    guest_features;
	uint32_t    max_queue_pairs;
	bool        started;
	uint16_t	max_mtu;
	uint16_t    vtnet_hdr_size;
	uint8_t	    vlan_strip;
	uint8_t	    use_msix;
	uint8_t     modern;
	uint8_t     use_vec_rx;
	uint8_t     use_vec_tx;
	uint8_t     use_inorder_rx;
	uint8_t     use_inorder_tx;
	uint8_t     weak_barriers;
	bool        rx_ol_scatter;
	bool        has_tx_offload;
	bool        has_rx_offload;
	uint16_t    port_id;
	uint8_t     mac_addr[RTE_ETHER_ADDR_LEN];
	/*
	 * Speed is specified via 'speed' devarg or
	 * negotiated via VIRTIO_NET_F_SPEED_DUPLEX
	 */
	bool get_speed_via_feat;
	uint32_t    notify_off_multiplier;
	uint32_t    speed;  /* link speed in MB */
	uint8_t     duplex;
	uint8_t     *isr;
	uint16_t    *notify_base;
	size_t      max_rx_pkt_len;
	struct virtio_pci_common_cfg *common_cfg;
	struct virtio_net_config *dev_cfg;
	void	    *virtio_user_dev;
	/*
	 * App management thread and virtio interrupt handler thread
	 * both can change device state, this lock is meant to avoid
	 * such a contention.
	 */
	rte_spinlock_t state_lock;
	struct rte_mbuf **inject_pkts;
	bool        opened;

	struct virtqueue **vqs;
};


/*
 * While virtio_hw is stored in shared memory, this structure stores
 * some infos that may vary in the multiple process model locally.
 * For example, the vtpci_ops pointer.
 */
struct virtio_hw_internal {
	const struct virtio_pci_ops *vtpci_ops;
	struct rte_pci_ioport io;
};

#define VTPCI_OPS(hw)	(virtio_hw_internal[(hw)->port_id].vtpci_ops)
#define VTPCI_IO(hw)	(&virtio_hw_internal[(hw)->port_id].io)

extern struct virtio_hw_internal virtio_hw_internal[RTE_MAX_ETHPORTS];


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

static inline int
vtpci_with_feature(struct virtio_hw *hw, uint64_t bit)
{
	return (hw->guest_features & (1ULL << bit)) != 0;
}

static inline int
vtpci_packed_queue(struct virtio_hw *hw)
{
	return vtpci_with_feature(hw, VIRTIO_F_RING_PACKED);
}

/*
 * Function declaration from virtio_pci.c
 */
int vtpci_init(struct rte_pci_device *dev, struct virtio_hw *hw);
void vtpci_reset(struct virtio_hw *);

void vtpci_reinit_complete(struct virtio_hw *);

uint8_t vtpci_get_status(struct virtio_hw *);
void vtpci_set_status(struct virtio_hw *, uint8_t);

uint64_t vtpci_negotiate_features(struct virtio_hw *, uint64_t);

void vtpci_write_dev_config(struct virtio_hw *, size_t, const void *, int);

void vtpci_read_dev_config(struct virtio_hw *, size_t, void *, int);

uint8_t vtpci_isr(struct virtio_hw *);

enum virtio_msix_status vtpci_msix_detect(struct rte_pci_device *dev);

extern const struct virtio_pci_ops legacy_ops;
extern const struct virtio_pci_ops modern_ops;
extern const struct virtio_pci_ops virtio_user_ops;

#endif /* _VIRTIO_PCI_H_ */
