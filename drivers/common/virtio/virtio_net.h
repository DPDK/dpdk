/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef _VIRTIO_NET_H_
#define _VIRTIO_NET_H_

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
#define VIRTIO_NET_F_GUEST_ANNOUNCE 21	/* Guest can announce device on the network */
#define VIRTIO_NET_F_MQ		22	/* Device supports Receive Flow Steering */
#define VIRTIO_NET_F_CTRL_MAC_ADDR 23	/* Set MAC address */
#define VIRTIO_NET_F_RSS	60	/* RSS supported */

/* Device set linkspeed and duplex */
#define VIRTIO_NET_F_SPEED_DUPLEX 63

#define VIRTIO_NET_S_LINK_UP	1	/* Link is up */
#define VIRTIO_NET_S_ANNOUNCE	2	/* Announcement is needed */

/*  Virtio RSS hash types */
#define VIRTIO_NET_HASH_TYPE_IPV4	RTE_BIT32(0)
#define VIRTIO_NET_HASH_TYPE_TCPV4	RTE_BIT32(1)
#define VIRTIO_NET_HASH_TYPE_UDPV4	RTE_BIT32(2)
#define VIRTIO_NET_HASH_TYPE_IPV6	RTE_BIT32(3)
#define VIRTIO_NET_HASH_TYPE_TCPV6	RTE_BIT32(4)
#define VIRTIO_NET_HASH_TYPE_UDPV6	RTE_BIT32(5)
#define VIRTIO_NET_HASH_TYPE_IP_EX	RTE_BIT32(6)
#define VIRTIO_NET_HASH_TYPE_TCP_EX	RTE_BIT32(7)
#define VIRTIO_NET_HASH_TYPE_UDP_EX	RTE_BIT32(8)

#define VIRTIO_NET_HASH_TYPE_MASK ( \
	VIRTIO_NET_HASH_TYPE_IPV4 | \
	VIRTIO_NET_HASH_TYPE_TCPV4 | \
	VIRTIO_NET_HASH_TYPE_UDPV4 | \
	VIRTIO_NET_HASH_TYPE_IPV6 | \
	VIRTIO_NET_HASH_TYPE_TCPV6 | \
	VIRTIO_NET_HASH_TYPE_UDPV6 | \
	VIRTIO_NET_HASH_TYPE_IP_EX | \
	VIRTIO_NET_HASH_TYPE_TCP_EX | \
	VIRTIO_NET_HASH_TYPE_UDP_EX)

/**
 * virtnet rx mode fileds
 */
union virtnet_rx_mode {
	uint32_t val;
	struct {
		unsigned int promisc:1;
		unsigned int allmulti:1;
		unsigned int alluni:1;
		unsigned int nomulti:1;
		unsigned int nouni:1;
		unsigned int nobcast:1;
	};
};

/*
 * This structure is just a reference to read net device specific
 * config space; it is just a shadow structure.
 *
 */
 #ifndef VIRTIO_API_FLAG
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
	uint8_t rss_max_key_size;
	uint16_t rss_max_indirection_table_length;
	uint32_t supported_hash_types;
} __rte_packed;
#endif

#endif /* _VIRTIO_NET_H_ */
