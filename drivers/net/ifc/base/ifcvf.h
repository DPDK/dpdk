/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _IFCVF_H_
#define _IFCVF_H_

#include "ifcvf_osdep.h"

#define IFCVF_VENDOR_ID		0x1AF4
#define IFCVF_DEVICE_ID		0x1041
#define IFCVF_SUBSYS_VENDOR_ID	0x8086
#define IFCVF_SUBSYS_DEVICE_ID	0x001A

#define IFCVF_MAX_QUEUES		1
#define VIRTIO_F_IOMMU_PLATFORM		33

/* Common configuration */
#define IFCVF_PCI_CAP_COMMON_CFG	1
/* Notifications */
#define IFCVF_PCI_CAP_NOTIFY_CFG	2
/* ISR Status */
#define IFCVF_PCI_CAP_ISR_CFG		3
/* Device specific configuration */
#define IFCVF_PCI_CAP_DEVICE_CFG	4
/* PCI configuration access */
#define IFCVF_PCI_CAP_PCI_CFG		5

#define IFCVF_CONFIG_STATUS_RESET     0x00
#define IFCVF_CONFIG_STATUS_ACK       0x01
#define IFCVF_CONFIG_STATUS_DRIVER    0x02
#define IFCVF_CONFIG_STATUS_DRIVER_OK 0x04
#define IFCVF_CONFIG_STATUS_FEATURES_OK 0x08
#define IFCVF_CONFIG_STATUS_FAILED    0x80

#define IFCVF_MSI_NO_VECTOR	0xffff
#define IFCVF_PCI_MAX_RESOURCE	6

#define IFCVF_LM_CFG_SIZE		0x40
#define IFCVF_LM_RING_STATE_OFFSET	0x20

#define IFCVF_LM_LOGGING_CTRL		0x0

#define IFCVF_LM_BASE_ADDR_LOW		0x10
#define IFCVF_LM_BASE_ADDR_HIGH		0x14
#define IFCVF_LM_END_ADDR_LOW		0x18
#define IFCVF_LM_END_ADDR_HIGH		0x1c

#define IFCVF_LM_DISABLE		0x0
#define IFCVF_LM_ENABLE_VF		0x1
#define IFCVF_LM_ENABLE_PF		0x3
#define IFCVF_LOG_BASE			0x100000000000
#define IFCVF_MEDIATED_VRING		0x200000000000

#define IFCVF_32_BIT_MASK		0xffffffff


struct ifcvf_pci_cap {
	u8 cap_vndr;            /* Generic PCI field: PCI_CAP_ID_VNDR */
	u8 cap_next;            /* Generic PCI field: next ptr. */
	u8 cap_len;             /* Generic PCI field: capability length */
	u8 cfg_type;            /* Identifies the structure. */
	u8 bar;                 /* Where to find it. */
	u8 padding[3];          /* Pad to full dword. */
	u32 offset;             /* Offset within bar. */
	u32 length;             /* Length of the structure, in bytes. */
};

struct ifcvf_pci_notify_cap {
	struct ifcvf_pci_cap cap;
	u32 notify_off_multiplier;  /* Multiplier for queue_notify_off. */
};

struct ifcvf_pci_common_cfg {
	/* About the whole device. */
	u32 device_feature_select;
	u32 device_feature;
	u32 guest_feature_select;
	u32 guest_feature;
	u16 msix_config;
	u16 num_queues;
	u8 device_status;
	u8 config_generation;

	/* About a specific virtqueue. */
	u16 queue_select;
	u16 queue_size;
	u16 queue_msix_vector;
	u16 queue_enable;
	u16 queue_notify_off;
	u32 queue_desc_lo;
	u32 queue_desc_hi;
	u32 queue_avail_lo;
	u32 queue_avail_hi;
	u32 queue_used_lo;
	u32 queue_used_hi;
};

struct ifcvf_net_config {
	u8    mac[6];
	u16   status;
	u16   max_virtqueue_pairs;
} __attribute__((packed));

struct ifcvf_pci_mem_resource {
	u64      phys_addr; /**< Physical address, 0 if not resource. */
	u64      len;       /**< Length of the resource. */
	u8       *addr;     /**< Virtual address, NULL when not mapped. */
};

struct vring_info {
	u64 desc;
	u64 avail;
	u64 used;
	u16 size;
	u16 last_avail_idx;
	u16 last_used_idx;
};

struct ifcvf_hw {
	u64    req_features;
	u8     notify_region;
	u32    notify_off_multiplier;
	struct ifcvf_pci_common_cfg *common_cfg;
	struct ifcvf_net_config *dev_cfg;
	u8     *isr;
	u16    *notify_base;
	u16    *notify_addr[IFCVF_MAX_QUEUES * 2];
	u8     *lm_cfg;
	struct vring_info vring[IFCVF_MAX_QUEUES * 2];
	u8 nr_vring;
	struct ifcvf_pci_mem_resource mem_resource[IFCVF_PCI_MAX_RESOURCE];
};

int
ifcvf_init_hw(struct ifcvf_hw *hw, PCI_DEV *dev);

u64
ifcvf_get_features(struct ifcvf_hw *hw);

int
ifcvf_start_hw(struct ifcvf_hw *hw);

void
ifcvf_stop_hw(struct ifcvf_hw *hw);

void
ifcvf_enable_logging(struct ifcvf_hw *hw, u64 log_base, u64 log_size);

void
ifcvf_disable_logging(struct ifcvf_hw *hw);

void
ifcvf_notify_queue(struct ifcvf_hw *hw, u16 qid);

u8
ifcvf_get_notify_region(struct ifcvf_hw *hw);

u64
ifcvf_get_queue_notify_off(struct ifcvf_hw *hw, int qid);

#endif /* _IFCVF_H_ */
