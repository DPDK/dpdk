/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright(c) 2021 Red Hat, Inc.
 */

#ifndef _VIRTIO_H_
#define _VIRTIO_H_

#include <rte_ether.h>

struct virtio_hw {
	struct virtqueue **vqs;
	uint64_t guest_features;
	uint16_t vtnet_hdr_size;
	uint8_t started;
	uint8_t weak_barriers;
	uint8_t vlan_strip;
	uint8_t has_tx_offload;
	uint8_t has_rx_offload;
	uint8_t use_vec_rx;
	uint8_t use_vec_tx;
	uint8_t use_inorder_rx;
	uint8_t use_inorder_tx;
	uint8_t opened;
	uint16_t port_id;
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	uint32_t speed;  /* link speed in MB */
	uint8_t duplex;
	uint8_t use_msix;
	uint16_t max_mtu;
	/*
	 * App management thread and virtio interrupt handler thread
	 * both can change device state, this lock is meant to avoid
	 * such a contention.
	 */
	rte_spinlock_t state_lock;
	struct rte_mbuf **inject_pkts;
	uint16_t max_queue_pairs;
	uint64_t req_guest_features;
	struct virtnet_ctl *cvq;
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
	uint16_t (*get_queue_num)(struct virtio_hw *hw, uint16_t queue_id);
	int (*setup_queue)(struct virtio_hw *hw, struct virtqueue *vq);
	void (*del_queue)(struct virtio_hw *hw, struct virtqueue *vq);
	void (*notify_queue)(struct virtio_hw *hw, struct virtqueue *vq);
	void (*intr_detect)(struct virtio_hw *hw);
	int (*dev_close)(struct virtio_hw *hw);
};

/*
 * This structure stores per-process data. Only virtio_ops for now.
 */
struct virtio_hw_internal {
	const struct virtio_ops *virtio_ops;
};

#define VIRTIO_OPS(hw)	(virtio_hw_internal[(hw)->port_id].virtio_ops)

extern struct virtio_hw_internal virtio_hw_internal[RTE_MAX_ETHPORTS];


#endif /* _VIRTIO_H_ */
