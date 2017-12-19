/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef RTE_ETH_BOND_ALB_H_
#define RTE_ETH_BOND_ALB_H_

#include <rte_ether.h>
#include <rte_arp.h>

#define ALB_HASH_TABLE_SIZE	256
#define ALB_NULL_INDEX		0xFFFFFFFF

struct client_data {
	/** ARP data of single client */
	struct ether_addr app_mac;
	/**< MAC address of application running DPDK */
	uint32_t app_ip;
	/**< IP address of application running DPDK */
	struct ether_addr cli_mac;
	/**< Client MAC address */
	uint32_t cli_ip;
	/**< Client IP address */

	uint16_t slave_idx;
	/**< Index of slave on which we connect with that client */
	uint8_t in_use;
	/**< Flag indicating if entry in client table is currently used */
	uint8_t ntt;
	/**< Flag indicating if we need to send update to this client on next tx */

	struct vlan_hdr vlan[2];
	/**< Content of vlan headers */
	uint8_t vlan_count;
	/**< Number of nested vlan headers */
};

struct mode_alb_private {
	struct client_data client_table[ALB_HASH_TABLE_SIZE];
	/**< Hash table storing ARP data of every client connected */
	struct rte_mempool *mempool;
	/**< Mempool for creating ARP update packets */
	uint8_t ntt;
	/**< Flag indicating if we need to send update to any client on next tx */
	uint32_t last_slave;
	/**< Index of last used slave in client table */
	rte_spinlock_t lock;
};

/**
 * ALB mode initialization.
 *
 * @param bond_dev		Pointer to bonding device.
 *
 * @return
 * Error code - 0 on success.
 */
int
bond_mode_alb_enable(struct rte_eth_dev *bond_dev);

/**
 * Function handles ARP packet reception. If received ARP request, it is
 * forwarded to application without changes. If it is ARP reply, client table
 * is updated.
 *
 * @param eth_h			ETH header of received packet.
 * @param offset		Vlan header offset.
 * @param internals		Bonding data.
 */
void
bond_mode_alb_arp_recv(struct ether_hdr *eth_h, uint16_t offset,
		struct bond_dev_private *internals);

/**
 * Function handles ARP packet transmission. It also decides on which slave
 * send that packet. If packet is ARP Request, it is send on primary slave.
 * If it is ARP Reply, it is send on slave stored in client table for that
 * connection. On Reply function also updates data in client table.
 *
 * @param eth_h			ETH header of transmitted packet.
 * @param offset		Vlan header offset.
 * @param internals		Bonding data.
 *
 * @return
 * Index of slave on which packet should be sent.
 */
uint16_t
bond_mode_alb_arp_xmit(struct ether_hdr *eth_h, uint16_t offset,
		struct bond_dev_private *internals);

/**
 * Function fills packet with ARP data from client_info.
 *
 * @param client_info	Data of client to which packet is sent.
 * @param pkt			Pointer to packet which is sent.
 * @param internals		Bonding data.
 *
 * @return
 * Index of slawe on which packet should be sent.
 */
uint16_t
bond_mode_alb_arp_upd(struct client_data *client_info,
		struct rte_mbuf *pkt, struct bond_dev_private *internals);

/**
 * Function updates slave indexes of active connections.
 *
 * @param bond_dev		Pointer to bonded device struct.
 */
void
bond_mode_alb_client_list_upd(struct rte_eth_dev *bond_dev);

#endif /* RTE_ETH_BOND_ALB_H_ */
