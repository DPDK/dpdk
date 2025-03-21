/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_PCI_H
#define ZXDH_PCI_H

#include <stdint.h>
#include <stdbool.h>

#include <bus_pci_driver.h>

#include "zxdh_ethdev.h"

#define ZXDH_PF_PCIE_ID(pcie_id)  (((pcie_id) & 0xff00) | 1 << 11)

enum zxdh_msix_status {
	ZXDH_MSIX_NONE     = 0,
	ZXDH_MSIX_DISABLED = 1,
	ZXDH_MSIX_ENABLED  = 2
};

/* The bit of the ISR which indicates a device has an interrupt. */
#define ZXDH_PCI_ISR_INTR    0x1
/* The bit of the ISR which indicates a device configuration change. */
#define ZXDH_PCI_ISR_CONFIG  0x2
/* Vector value used to disable MSI for queue. */
#define ZXDH_MSI_NO_VECTOR   0x7F

#define ZXDH_PCI_VRING_ALIGN         4096

#define ZXDH_NET_F_CSUM              0   /* Host handles pkts w/ partial csum */
#define ZXDH_NET_F_GUEST_CSUM        1   /* Guest handles pkts w/ partial csum */
#define ZXDH_NET_F_MTU               3   /* Initial MTU advice. */
#define ZXDH_NET_F_MAC               5   /* Host has given MAC address. */
#define ZXDH_NET_F_GUEST_TSO4        7   /* Guest can handle TSOv4 in. */
#define ZXDH_NET_F_GUEST_TSO6        8   /* Guest can handle TSOv6 in. */
#define ZXDH_NET_F_GUEST_ECN         9   /* Guest can handle TSO[6] w/ ECN in. */
#define ZXDH_NET_F_GUEST_UFO         10  /* Guest can handle UFO in. */

#define ZXDH_NET_F_HOST_UFO          14  /* Host can handle UFO in. */
#define ZXDH_NET_F_HOST_TSO4         11  /* Host can handle TSOv4 in. */
#define ZXDH_NET_F_HOST_TSO6         12  /* Host can handle TSOv6 in. */
#define ZXDH_NET_F_MRG_RXBUF         15  /* Host can merge receive buffers. */
#define ZXDH_NET_F_STATUS            16  /* zxdh_net_config.status available */
#define ZXDH_NET_F_MQ                22  /* Device supports Receive Flow Steering */
#define ZXDH_F_ANY_LAYOUT            27 /* Can the device handle any descriptor layout */
#define ZXDH_F_VERSION_1             32
#define ZXDH_F_RING_PACKED           34
#define ZXDH_F_IN_ORDER              35
#define ZXDH_F_NOTIFICATION_DATA     38

#define ZXDH_PCI_CAP_COMMON_CFG  1 /* Common configuration */
#define ZXDH_PCI_CAP_NOTIFY_CFG  2 /* Notifications */
#define ZXDH_PCI_CAP_ISR_CFG     3 /* ISR Status */
#define ZXDH_PCI_CAP_DEVICE_CFG  4 /* Device specific configuration */
#define ZXDH_PCI_CAP_PCI_CFG     5 /* PCI configuration access */

/* Status byte for guest to report progress. */
#define ZXDH_CONFIG_STATUS_RESET           0x00
#define ZXDH_CONFIG_STATUS_ACK             0x01
#define ZXDH_CONFIG_STATUS_DRIVER          0x02
#define ZXDH_CONFIG_STATUS_DRIVER_OK       0x04
#define ZXDH_CONFIG_STATUS_FEATURES_OK     0x08
#define ZXDH_CONFIG_STATUS_DEV_NEED_RESET  0x40
#define ZXDH_CONFIG_STATUS_FAILED          0x80
#define ZXDH_PCI_QUEUE_ADDR_SHIFT          12

struct __rte_packed_begin zxdh_net_config {
	/* The config defining mac address (if ZXDH_NET_F_MAC) */
	uint8_t    mac[RTE_ETHER_ADDR_LEN];
	/* See ZXDH_NET_F_STATUS and ZXDH_NET_S_* above */
	uint16_t   status;
	uint16_t   max_virtqueue_pairs;
	uint16_t   mtu;
	uint32_t   speed;
	uint8_t    duplex;
} __rte_packed_end;

/* This is the PCI capability header: */
struct zxdh_pci_cap {
	uint8_t  cap_vndr;   /* Generic PCI field: PCI_CAP_ID_VNDR */
	uint8_t  cap_next;   /* Generic PCI field: next ptr. */
	uint8_t  cap_len;    /* Generic PCI field: capability length */
	uint8_t  cfg_type;   /* Identifies the structure. */
	uint8_t  bar;        /* Where to find it. */
	uint8_t  padding[3]; /* Pad to full dword. */
	uint32_t offset;     /* Offset within bar. */
	uint32_t length;     /* Length of the structure, in bytes. */
};

/* Fields in ZXDH_PCI_CAP_COMMON_CFG: */
struct zxdh_pci_common_cfg {
	/* About the whole device. */
	uint32_t device_feature_select; /* read-write */
	uint32_t device_feature;    /* read-only */
	uint32_t guest_feature_select;  /* read-write */
	uint32_t guest_feature;     /* read-write */
	uint16_t msix_config;       /* read-write */
	uint16_t num_queues;        /* read-only */
	uint8_t  device_status;     /* read-write */
	uint8_t  config_generation; /* read-only */

	/* About a specific virtqueue. */
	uint16_t queue_select;      /* read-write */
	uint16_t queue_size;        /* read-write, power of 2. */
	uint16_t queue_msix_vector; /* read-write */
	uint16_t queue_enable;      /* read-write */
	uint16_t queue_notify_off;  /* read-only */
	uint32_t queue_desc_lo;     /* read-write */
	uint32_t queue_desc_hi;     /* read-write */
	uint32_t queue_avail_lo;    /* read-write */
	uint32_t queue_avail_hi;    /* read-write */
	uint32_t queue_used_lo;     /* read-write */
	uint32_t queue_used_hi;     /* read-write */
};

static inline int32_t
zxdh_pci_with_feature(struct zxdh_hw *hw, uint64_t bit)
{
	return (hw->guest_features & (1ULL << bit)) != 0;
}

static inline int32_t
zxdh_pci_packed_queue(struct zxdh_hw *hw)
{
	return zxdh_pci_with_feature(hw, ZXDH_F_RING_PACKED);
}

struct zxdh_pci_ops {
	void     (*read_dev_cfg)(struct zxdh_hw *hw, size_t offset, void *dst, int32_t len);
	void     (*write_dev_cfg)(struct zxdh_hw *hw, size_t offset, const void *src, int32_t len);

	uint8_t  (*get_status)(struct zxdh_hw *hw);
	void     (*set_status)(struct zxdh_hw *hw, uint8_t status);

	uint64_t (*get_features)(struct zxdh_hw *hw);
	void     (*set_features)(struct zxdh_hw *hw, uint64_t features);
	uint16_t (*set_queue_irq)(struct zxdh_hw *hw, struct zxdh_virtqueue *vq, uint16_t vec);
	uint16_t (*set_config_irq)(struct zxdh_hw *hw, uint16_t vec);
	uint8_t  (*get_isr)(struct zxdh_hw *hw);
	uint16_t (*get_queue_num)(struct zxdh_hw *hw, uint16_t queue_id);
	void     (*set_queue_num)(struct zxdh_hw *hw, uint16_t queue_id, uint16_t vq_size);

	int32_t  (*setup_queue)(struct zxdh_hw *hw, struct zxdh_virtqueue *vq);
	void     (*del_queue)(struct zxdh_hw *hw, struct zxdh_virtqueue *vq);
	void     (*notify_queue)(struct zxdh_hw *hw, struct zxdh_virtqueue *vq);
};

struct zxdh_hw_internal {
	const struct zxdh_pci_ops *zxdh_vtpci_ops;
};

#define ZXDH_VTPCI_OPS(hw)  (zxdh_hw_internal[(hw)->port_id].zxdh_vtpci_ops)

extern struct zxdh_hw_internal zxdh_hw_internal[RTE_MAX_ETHPORTS];
extern const struct zxdh_pci_ops zxdh_dev_pci_ops;

void zxdh_pci_reset(struct zxdh_hw *hw);
void zxdh_pci_read_dev_config(struct zxdh_hw *hw, size_t offset,
		void *dst, int32_t length);

int32_t zxdh_read_pci_caps(struct rte_pci_device *dev, struct zxdh_hw *hw);
void zxdh_get_pci_dev_config(struct zxdh_hw *hw);
void zxdh_pci_write_dev_config(struct zxdh_hw *hw, size_t offset, const void *src, int32_t length);
uint16_t zxdh_pci_get_features(struct zxdh_hw *hw);
enum zxdh_msix_status zxdh_pci_msix_detect(struct rte_pci_device *dev);
uint8_t zxdh_pci_isr(struct zxdh_hw *hw);
void zxdh_pci_reinit_complete(struct zxdh_hw *hw);
void zxdh_pci_set_status(struct zxdh_hw *hw, uint8_t status);

#endif /* ZXDH_PCI_H */
