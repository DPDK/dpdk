/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef _VIRTIO_PCI_STATE_H_
#define _VIRTIO_PCI_STATE_H_

/*tlv used by virtio_dev_state*/
struct virtio_field_hdr {
	uint32_t type;	/* contains the value of enum virtio_dev_field_type */
	uint32_t size;	/* size of the data field */
//	uint8_t data[];
}__rte_packed;

/* generic virtio device data blob */
struct virtio_dev_state_hdr {
	uint32_t virtio_field_count;	/*num of tlv*/
//	struct virtio_field fields[virtio_field_count];
} __rte_packed;

/*The TLV above the optional section, is deemed as mandatory.*/
enum virtio_dev_field_type {
	VIRTIO_DEV_PCI_COMMON_CFG,	/*	struct virtio_dev_common_cfg */
	VIRTIO_DEV_CFG_SPACE,		/* config space fields, for net  struct virtio_net_config etc */
	VIRTIO_DEV_QUEUE_CFG,		/* struct virtio_dev_q_cfg */

   /*optional start here*/
	VIRTIO_DEV_SPLIT_Q_RUN_STATE, /*virtio_dev_split_q_run_state*/
	VIRTIO_NET_RX_MODE_CFG,
	VIRTIO_NET_VLAN_CFG,
	VIRTIO_NET_MAC_UNICAST_CFG,
	VIRTIO_NET_MAC_MULTICAST_CFG,
	VIRTIO_DEV_IN_FLIGHT_DESC, /* OPTIONAL , list of descriptor heads still ‘in-flight’. This is needed if we want to support out of order state save/restore without doing full suspend. In block case we can have ‘slow’ ios to the backend. At the moment we wait until they complete (controller suspended) but in the future we should be able  to quiesce/freeze controller with ios still in flight */
	VIRTIO_DEV_COMPLETED_DESC, /*list of descriptor that has been completed , but not yet marked as used to used ring, should be set as used in dest side*/
	VIRTIO_DEV_SUBVENDOR_SPECIFIC_CFG, /*subvender specific data, sub vendor know how to parse and explain the content*/
};

struct virtio_pci_state_common_cfg {
	uint32_t device_feature_select;
	uint64_t device_feature;
	uint32_t driver_feature_select;
	uint64_t driver_feature;
	uint16_t msix_config;
	uint16_t num_queues;
	uint16_t queue_select;
	uint8_t	 device_status;
	uint8_t	 config_generation;
} __rte_packed;

struct virtio_dev_q_cfg {
	uint16_t queue_index;	/* queue number whose config is contained below */
	uint16_t queue_size;
	uint16_t queue_msix_vector;
	uint16_t queue_enable;
	uint16_t queue_notify_off;
	uint64_t queue_desc;
	uint64_t queue_driver;
	uint64_t queue_device;
	uint16_t queue_notify_data;
	uint16_t queue_reset;
} __rte_packed;

/*the statistics counter may be expanded*/
struct virtio_dev_vq_statistic_state   {
	uint16_t queue_index;
	uint64_t received_desc;
	uint64_t completed_desc;
	uint64_t bad_desc_errors;
	uint64_t error_cqes;
	uint64_t exceed_max_chain;
	uint64_t invalid_buffer;
} __rte_packed;

struct virtio_dev_net_q_statistic_state   {
	uint16_t queue_index;
	uint64_t pkt_count;
	uint64_t byte_cout;
	uint64_t tso_pkt_count;
	uint64_t tso_byte_count;
	uint64_t rx_l2_err_count;
	uint64_t rx_l3_err_count;
	uint64_t rx_l4_err_count;
	uint64_t exceed_rx_buffer_size_count;
} __rte_packed;

struct virtio_dev_blk_q_statistic_state   {
	uint16_t queue_index;
} __rte_packed;

struct virtio_dev_split_q_run_state {
	uint16_t queue_index;
	uint16_t last_avail_idx;
	uint16_t last_used_idx;
} __rte_packed;

/*to be defined*/
struct virtio_dev_packed_q_run_state {
	uint16_t queue_index;
} __rte_packed;

/* array of the descriptors to re-play towards the backend */
struct virtio_dev_inflight_desc {
	uint16_t queue_index;
	uint16_t inflight_desc_hdr_count;
	uint16_t desc_idx[]; /*array of desc hdr index */
} __rte_packed;

/* array of the descriptors to be completed at dest side , indexes of ios that data buffer has already copy to host mem, the only thing not done is update used ring/used element, those desc go here*/
struct virtio_dev_completed_desc {
	uint16_t queue_index;
	uint16_t completed_desc_hdr_count;
	uint16_t desc_idx[]; /*array of desc hdr index*/
} __rte_packed;

struct virtio_dev_queue_info {
	struct virtio_field_hdr q_cfg_hdr;
	struct virtio_dev_q_cfg q_cfg;
	struct virtio_field_hdr q_run_state_hdr;
	struct virtio_dev_split_q_run_state q_run_state;
} __rte_packed;

struct virtio_dev_common_state {
	struct virtio_dev_state_hdr hdr;
	struct virtio_field_hdr common_cfg_hdr;
	struct virtio_pci_state_common_cfg common_cfg;
	struct virtio_field_hdr dev_cfg_hdr;
} __rte_packed;

#define VIRTIO_DEV_STATE_COMMON_FIELD_CNT 2
#define VIRTIO_DEV_STATE_PER_QUEUE_FIELD_CNT 2

#endif /* _VIRTIO_PCI_STATE_H_ */
