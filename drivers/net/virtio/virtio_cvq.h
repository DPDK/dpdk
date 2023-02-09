/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _VIRTIO_CVQ_H_
#define _VIRTIO_CVQ_H_

#include <rte_ether.h>

struct virtqueue;

/**
 * Control the RX mode, ie. promiscuous, allmulti, etc...
 * All commands require an "out" sg entry containing a 1 byte
 * state value, zero = disable, non-zero = enable.  Commands
 * 0 and 1 are supported with the VIRTIO_NET_F_CTRL_RX feature.
 * Commands 2-5 are added with VIRTIO_NET_F_CTRL_RX_EXTRA.
 */
#define VIRTIO_NET_CTRL_RX              0
#define VIRTIO_NET_CTRL_RX_PROMISC      0
#define VIRTIO_NET_CTRL_RX_ALLMULTI     1
#define VIRTIO_NET_CTRL_RX_ALLUNI       2
#define VIRTIO_NET_CTRL_RX_NOMULTI      3
#define VIRTIO_NET_CTRL_RX_NOUNI        4
#define VIRTIO_NET_CTRL_RX_NOBCAST      5

/**
 * Control the MAC
 *
 * The MAC filter table is managed by the hypervisor, the guest should
 * assume the size is infinite.  Filtering should be considered
 * non-perfect, ie. based on hypervisor resources, the guest may
 * received packets from sources not specified in the filter list.
 *
 * In addition to the class/cmd header, the TABLE_SET command requires
 * two out scatterlists.  Each contains a 4 byte count of entries followed
 * by a concatenated byte stream of the ETH_ALEN MAC addresses.  The
 * first sg list contains unicast addresses, the second is for multicast.
 * This functionality is present if the VIRTIO_NET_F_CTRL_RX feature
 * is available.
 *
 * The ADDR_SET command requests one out scatterlist, it contains a
 * 6 bytes MAC address. This functionality is present if the
 * VIRTIO_NET_F_CTRL_MAC_ADDR feature is available.
 */
struct virtio_net_ctrl_mac {
	uint32_t entries;
	uint8_t macs[][RTE_ETHER_ADDR_LEN];
} __rte_packed;

#define VIRTIO_NET_CTRL_MAC    1
#define VIRTIO_NET_CTRL_MAC_TABLE_SET        0
#define VIRTIO_NET_CTRL_MAC_ADDR_SET         1

/**
 * Control VLAN filtering
 *
 * The VLAN filter table is controlled via a simple ADD/DEL interface.
 * VLAN IDs not added may be filtered by the hypervisor.  Del is the
 * opposite of add.  Both commands expect an out entry containing a 2
 * byte VLAN ID.  VLAN filtering is available with the
 * VIRTIO_NET_F_CTRL_VLAN feature bit.
 */
#define VIRTIO_NET_CTRL_VLAN     2
#define VIRTIO_NET_CTRL_VLAN_ADD 0
#define VIRTIO_NET_CTRL_VLAN_DEL 1

/**
 * RSS control
 *
 * The RSS feature configuration message is sent by the driver when
 * VIRTIO_NET_F_RSS has been negotiated. It provides the device with
 * hash types to use, hash key and indirection table. In this
 * implementation, the driver only supports fixed key length (40B)
 * and indirection table size (128 entries).
 */
#define VIRTIO_NET_RSS_RETA_SIZE 128
#define VIRTIO_NET_RSS_KEY_SIZE 40

struct virtio_net_ctrl_rss {
	uint32_t hash_types;
	uint16_t indirection_table_mask;
	uint16_t unclassified_queue;
	uint16_t indirection_table[VIRTIO_NET_RSS_RETA_SIZE];
	uint16_t max_tx_vq;
	uint8_t hash_key_length;
	uint8_t hash_key_data[VIRTIO_NET_RSS_KEY_SIZE];
};

/*
 * Control link announce acknowledgment
 *
 * The command VIRTIO_NET_CTRL_ANNOUNCE_ACK is used to indicate that
 * driver has received the notification; device would clear the
 * VIRTIO_NET_S_ANNOUNCE bit in the status field after it receives
 * this command.
 */
#define VIRTIO_NET_CTRL_ANNOUNCE     3
#define VIRTIO_NET_CTRL_ANNOUNCE_ACK 0

struct virtio_net_ctrl_hdr {
	uint8_t class;
	uint8_t cmd;
} __rte_packed;

typedef uint8_t virtio_net_ctrl_ack;

struct virtnet_ctl {
	const struct rte_memzone *hdr_mz; /**< memzone to populate hdr. */
	rte_iova_t hdr_mem;               /**< hdr for each xmit packet */
	rte_spinlock_t lock;              /**< spinlock for control queue. */
	void (*notify_queue)(struct virtqueue *vq, void *cookie); /**< notify ops. */
	void *notify_cookie;              /**< cookie for notify ops */
};

#define VIRTIO_MAX_CTRL_DATA 2048

struct virtio_pmd_ctrl {
	struct virtio_net_ctrl_hdr hdr;
	virtio_net_ctrl_ack status;
	uint8_t data[VIRTIO_MAX_CTRL_DATA];
};

int
virtio_send_command(struct virtnet_ctl *cvq, struct virtio_pmd_ctrl *ctrl, int *dlen, int pkt_num);

#endif /* _VIRTIO_RXTX_H_ */
