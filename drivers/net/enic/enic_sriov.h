/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 Cisco Systems, Inc.  All rights reserved.
 */

#ifndef _ENIC_SRIOV_H_
#define _ENIC_SRIOV_H_

/*
 * SR-IOV VF and PF drivers exchange control messages through
 * the admin channel. Those messages are all defined here.
 *
 * VF_ prefix means the VF driver initiates the request.
 * PF_ prefix means the PF driver initiates the request.
 */
enum enic_mbox_msg_type {
	ENIC_MBOX_VF_CAPABILITY_REQUEST,
	ENIC_MBOX_VF_CAPABILITY_REPLY,
	ENIC_MBOX_VF_REGISTER_REQUEST,
	ENIC_MBOX_VF_REGISTER_REPLY,
	ENIC_MBOX_VF_UNREGISTER_REQUEST,
	ENIC_MBOX_VF_UNREGISTER_REPLY,
	ENIC_MBOX_PF_LINK_STATE_NOTIF,
	ENIC_MBOX_PF_LINK_STATE_ACK,
	ENIC_MBOX_PF_GET_STATS_REQUEST,
	ENIC_MBOX_PF_GET_STATS_REPLY,
	ENIC_MBOX_VF_ADD_DEL_MAC_REQUEST,
	ENIC_MBOX_VF_ADD_DEL_MAC_REPLY,
	ENIC_MBOX_PF_SET_ADMIN_MAC_NOTIF,
	ENIC_MBOX_PF_SET_ADMIN_MAC_ACK,
	ENIC_MBOX_VF_SET_PKT_FILTER_FLAGS_REQUEST,
	ENIC_MBOX_VF_SET_PKT_FILTER_FLAGS_REPLY,
	ENIC_MBOX_MAX
};

/*
 * Special value for {src,dst}_vnic_id. 0xffff means PF.
 * For VF, vnic_id is the VF id.
 */
#define ENIC_MBOX_DST_PF 0xffff

struct enic_mbox_hdr {
	uint16_t src_vnic_id;
	uint16_t dst_vnic_id;
	uint8_t msg_type;
	uint8_t flags;
	uint16_t msg_len;
	uint64_t msg_num;
};

#define ENIC_MBOX_ERR_GENERIC RTE_BIT32(0)

struct enic_mbox_generic_reply_msg {
	uint16_t ret_major;
	uint16_t ret_minor;
};

/*
 * ENIC_MBOX_VF_CAPABILITY_REQUEST
 * ENIC_MBOX_VF_CAPABILITY_REPLY
 */
#define ENIC_MBOX_CAP_VERSION_0  0
#define ENIC_MBOX_CAP_VERSION_1  1
#define ENIC_MBOX_CAP_VERSION_INVALID  0xffffffff

struct enic_mbox_vf_capability_msg {
	struct enic_mbox_hdr hdr;
	uint32_t version;
	uint32_t reserved[32]; /* 128B for future use */
};

struct enic_mbox_vf_capability_reply_msg {
	struct enic_mbox_hdr hdr;
	struct enic_mbox_generic_reply_msg generic_reply;
	uint32_t version;
	uint32_t reserved[32]; /* 128B for future use */
};

/*
 * ENIC_MBOX_VF_REGISTER_REQUEST
 * ENIC_MBOX_VF_REGISTER_REPLY
 * ENIC_MBOX_VF_UNREGISTER_REQUEST
 * ENIC_MBOX_VF_UNREGISTER_REPLY
 */
struct enic_mbox_vf_register_msg {
	struct enic_mbox_hdr hdr;
};

struct enic_mbox_vf_register_reply_msg {
	struct enic_mbox_hdr hdr;
	struct enic_mbox_generic_reply_msg generic_reply;
};

struct enic_mbox_vf_unregister_msg {
	struct enic_mbox_hdr hdr;
};

struct enic_mbox_vf_unregister_reply_msg {
	struct enic_mbox_hdr hdr;
	struct enic_mbox_generic_reply_msg generic_reply;
};

/*
 * ENIC_MBOX_PF_LINK_STATE_NOTIF
 * ENIC_MBOX_PF_LINK_STATE_ACK
 */
#define ENIC_MBOX_LINK_STATE_DISABLE    0
#define ENIC_MBOX_LINK_STATE_ENABLE     1
struct enic_mbox_pf_link_state_notif_msg {
	struct enic_mbox_hdr hdr;
	uint32_t link_state;
};

struct enic_mbox_pf_link_state_ack_msg {
	struct enic_mbox_hdr hdr;
	struct enic_mbox_generic_reply_msg generic_reply;
};

/*
 * ENIC_MBOX_PF_GET_STATS_REQUEST
 * ENIC_MBOX_PF_GET_STATS_REPLY
 */
#define ENIC_MBOX_GET_STATS_RX  RTE_BIT32(0)
#define ENIC_MBOX_GET_STATS_TX  RTE_BIT32(1)
#define ENIC_MBOX_GET_STATS_ALL (ENIC_MBOX_GET_STATS_RX | ENIC_MBOX_GET_STATS_TX)

struct enic_mbox_pf_get_stats_msg {
	struct enic_mbox_hdr hdr;
	uint16_t flags;
	uint16_t pad;
};

struct enic_mbox_pf_get_stats_reply {
	struct vnic_stats vnic_stats;
	/* The size of the struct vnic_stats is guaranteed to not change, but
	 * the number of counters (in the rx/tx elements of that struct) that
	 * are actually init may vary depending on the driver version (new
	 * fields may be added to the rsvd blocks).
	 * These two variables tell us how much of the tx/rx blocks inside
	 * struct vnic_stats the VF driver knows about according to its
	 * definition of that data structure.
	 */
	uint8_t num_rx_stats;
	uint8_t num_tx_stats;
	uint8_t pad[6];
};

struct enic_mbox_pf_get_stats_reply_msg {
	struct enic_mbox_hdr hdr;
	struct enic_mbox_generic_reply_msg generic_reply;
	struct enic_mbox_pf_get_stats_reply stats;
};

/*
 * ENIC_MBOX_VF_ADD_DEL_MAC_REQUEST
 * ENIC_MBOX_VF_ADD_DEL_MAC_REPLY
 */
/* enic_mac_addr.flags: Lower 8 bits are used in VF->PF direction (request) */
#define MAC_ADDR_FLAG_ADD        RTE_BIT32(0)
#define MAC_ADDR_FLAG_STATION    RTE_BIT32(1)

struct enic_mac_addr {
	uint8_t addr[RTE_ETHER_ADDR_LEN];
	uint16_t flags;
};

struct enic_mbox_vf_add_del_mac_msg {
	struct enic_mbox_hdr hdr;
	uint16_t num_addrs;
	uint16_t pad;
	/* This can be mac_addr[], but the driver only uses 1 element */
	struct enic_mac_addr mac_addr;
};

struct enic_mbox_vf_add_del_mac_reply_msg {
	struct enic_mbox_hdr hdr;
	struct enic_mbox_generic_reply_msg generic_reply;
	struct enic_mbox_vf_add_del_mac_msg detailed_reply[];
};

/*
 * ENIC_MBOX_VF_SET_PKT_FILTER_FLAGS_REQUEST
 * ENIC_MBOX_VF_SET_PKT_FILTER_FLAGS_REPLY
 */
#define ENIC_MBOX_PKT_FILTER_DIRECTED   RTE_BIT32(0)
#define ENIC_MBOX_PKT_FILTER_MULTICAST  RTE_BIT32(1)
#define ENIC_MBOX_PKT_FILTER_BROADCAST  RTE_BIT32(2)
#define ENIC_MBOX_PKT_FILTER_PROMISC    RTE_BIT32(3)
#define ENIC_MBOX_PKT_FILTER_ALLMULTI   RTE_BIT32(4)

struct enic_mbox_vf_set_pkt_filter_flags_msg {
	struct enic_mbox_hdr hdr;
	uint16_t flags;
	uint16_t pad;
};

struct enic_mbox_vf_set_pkt_filter_flags_reply_msg {
	struct enic_mbox_hdr hdr;
	struct enic_mbox_generic_reply_msg generic_reply;
};

int enic_enable_vf_admin_chan(struct enic *enic);
int enic_disable_vf_admin_chan(struct enic *enic, bool unregister);
void enic_poll_vf_admin_chan(struct enic *enic);

/* devcmds that may go through PF driver */
int enic_dev_packet_filter(struct enic *enic, int directed, int multicast,
			   int broadcast, int promisc, int allmulti);
int enic_dev_add_addr(struct enic *enic, uint8_t *addr);
int enic_dev_del_addr(struct enic *enic, uint8_t *addr);

#endif /* _ENIC_SRIOV_H_ */
