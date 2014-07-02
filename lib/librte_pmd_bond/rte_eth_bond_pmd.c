/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_devargs.h>
#include <rte_kvargs.h>
#include <rte_dev.h>

#include "rte_eth_bond.h"
#include "rte_eth_bond_private.h"

static uint16_t
bond_ethdev_rx_burst(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct bond_dev_private *internals;

	uint16_t num_rx_slave = 0;
	uint16_t num_rx_total = 0;

	int i;

	/* Cast to structure, containing bonded device's port id and queue id */
	struct bond_rx_queue *bd_rx_q = (struct bond_rx_queue *)queue;

	internals = bd_rx_q->dev_private;

	switch (internals->mode) {
	case BONDING_MODE_ROUND_ROBIN:
	case BONDING_MODE_BROADCAST:
	case BONDING_MODE_BALANCE:
		for (i = 0; i < internals->active_slave_count && nb_pkts; i++) {
			/* Offset of pointer to *bufs increases as packets are received
			 * from other slaves */
			num_rx_slave = rte_eth_rx_burst(internals->active_slaves[i],
					bd_rx_q->queue_id, bufs + num_rx_total, nb_pkts);
			if (num_rx_slave) {
				num_rx_total += num_rx_slave;
				nb_pkts -= num_rx_slave;
			}
		}
		break;
	case BONDING_MODE_ACTIVE_BACKUP:
		num_rx_slave = rte_eth_rx_burst(internals->current_primary_port,
				bd_rx_q->queue_id, bufs, nb_pkts);
		if (num_rx_slave)
			num_rx_total = num_rx_slave;
		break;
	}
	return num_rx_total;
}

static uint16_t
bond_ethdev_tx_round_robin(void *queue, struct rte_mbuf **bufs,
		uint16_t nb_pkts)
{
	struct bond_dev_private *dev_private;
	struct bond_tx_queue *bd_tx_q;

	struct rte_mbuf *slave_bufs[RTE_MAX_ETHPORTS][nb_pkts];
	uint16_t slave_nb_pkts[RTE_MAX_ETHPORTS] = { 0 };

	uint8_t num_of_slaves;
	uint8_t slaves[RTE_MAX_ETHPORTS];

	uint16_t num_tx_total = 0;

	static int slave_idx = 0;
	int i, cs_idx = 0;

	bd_tx_q = (struct bond_tx_queue *)queue;
	dev_private = bd_tx_q->dev_private;

	/* Copy slave list to protect against slave up/down changes during tx
	 * bursting */
	num_of_slaves = dev_private->active_slave_count;
	memcpy(slaves, dev_private->active_slaves,
			sizeof(dev_private->active_slaves[0]) * num_of_slaves);

	if (num_of_slaves < 1)
		return num_tx_total;

	/* Populate slaves mbuf with which packets are to be sent on it  */
	for (i = 0; i < nb_pkts; i++) {
		cs_idx = (slave_idx + i) % num_of_slaves;
		slave_bufs[cs_idx][(slave_nb_pkts[cs_idx])++] = bufs[i];
	}

	/* increment current slave index so the next call to tx burst starts on the
	 * next slave */
	slave_idx = ++cs_idx;

	/* Send packet burst on each slave device */
	for (i = 0; i < num_of_slaves; i++)
		if (slave_nb_pkts[i] > 0)
			num_tx_total += rte_eth_tx_burst(slaves[i],
					bd_tx_q->queue_id, slave_bufs[i], slave_nb_pkts[i]);

	return num_tx_total;
}

static uint16_t
bond_ethdev_tx_active_backup(void *queue,
		struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct bond_dev_private *internals;
	struct bond_tx_queue *bd_tx_q;

	bd_tx_q = (struct bond_tx_queue *)queue;
	internals = bd_tx_q->dev_private;

	if (internals->active_slave_count < 1)
		return 0;

	return rte_eth_tx_burst(internals->current_primary_port, bd_tx_q->queue_id,
			bufs, nb_pkts);
}

static inline uint16_t
ether_hash(struct ether_hdr *eth_hdr)
{
	uint16_t *word_src_addr = (uint16_t *)eth_hdr->s_addr.addr_bytes;
	uint16_t *word_dst_addr = (uint16_t *)eth_hdr->d_addr.addr_bytes;

	return (word_src_addr[0] ^ word_dst_addr[0]) ^
			(word_src_addr[1] ^ word_dst_addr[1]) ^
			(word_src_addr[2] ^ word_dst_addr[2]);
}

static inline uint32_t
ipv4_hash(struct ipv4_hdr *ipv4_hdr)
{
	return (ipv4_hdr->src_addr ^ ipv4_hdr->dst_addr);
}

static inline uint32_t
ipv6_hash(struct ipv6_hdr *ipv6_hdr)
{
	uint32_t *word_src_addr = (uint32_t *)&(ipv6_hdr->src_addr[0]);
	uint32_t *word_dst_addr = (uint32_t *)&(ipv6_hdr->dst_addr[0]);

	return (word_src_addr[0] ^ word_dst_addr[0]) ^
			(word_src_addr[1] ^ word_dst_addr[1]) ^
			(word_src_addr[2] ^ word_dst_addr[2]) ^
			(word_src_addr[3] ^ word_dst_addr[3]);
}

static uint32_t
udp_hash(struct udp_hdr *hdr)
{
	return hdr->src_port ^ hdr->dst_port;
}

static inline uint16_t
xmit_slave_hash(const struct rte_mbuf *buf, uint8_t slave_count, uint8_t policy)
{
	struct ether_hdr *eth_hdr;
	struct udp_hdr *udp_hdr;
	size_t eth_offset = 0;
	uint32_t hash = 0;

	if (slave_count == 1)
		return 0;

	switch (policy) {
	case BALANCE_XMIT_POLICY_LAYER2:
		eth_hdr = (struct ether_hdr *)buf->pkt.data;

		hash = ether_hash(eth_hdr);
		hash ^= hash >> 8;
		return hash % slave_count;

	case BALANCE_XMIT_POLICY_LAYER23:
		eth_hdr = (struct ether_hdr *)buf->pkt.data;

		if (buf->ol_flags & PKT_RX_VLAN_PKT)
			eth_offset = sizeof(struct ether_hdr) + sizeof(struct vlan_hdr);
		else
			eth_offset = sizeof(struct ether_hdr);

		if (buf->ol_flags & PKT_RX_IPV4_HDR) {
			struct ipv4_hdr *ipv4_hdr;
			ipv4_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(buf,
					unsigned char *) + eth_offset);

			hash = ether_hash(eth_hdr) ^ ipv4_hash(ipv4_hdr);

		} else {
			struct ipv6_hdr *ipv6_hdr;

			ipv6_hdr = (struct ipv6_hdr *)(rte_pktmbuf_mtod(buf,
					unsigned char *) + eth_offset);

			hash = ether_hash(eth_hdr) ^ ipv6_hash(ipv6_hdr);
		}
		break;

	case BALANCE_XMIT_POLICY_LAYER34:
		if (buf->ol_flags & PKT_RX_VLAN_PKT)
			eth_offset = sizeof(struct ether_hdr) + sizeof(struct vlan_hdr);
		else
			eth_offset = sizeof(struct ether_hdr);

		if (buf->ol_flags & PKT_RX_IPV4_HDR) {
			struct ipv4_hdr *ipv4_hdr = (struct ipv4_hdr *)
					(rte_pktmbuf_mtod(buf, unsigned char *) + eth_offset);

			if (ipv4_hdr->next_proto_id == IPPROTO_UDP) {
				udp_hdr = (struct udp_hdr *)
						(rte_pktmbuf_mtod(buf, unsigned char *) + eth_offset +
								sizeof(struct ipv4_hdr));
				hash = ipv4_hash(ipv4_hdr) ^ udp_hash(udp_hdr);
			} else {
				hash = ipv4_hash(ipv4_hdr);
			}
		} else {
			struct ipv6_hdr *ipv6_hdr = (struct ipv6_hdr *)
					(rte_pktmbuf_mtod(buf, unsigned char *) + eth_offset);

			if (ipv6_hdr->proto == IPPROTO_UDP) {
				udp_hdr = (struct udp_hdr *)
						(rte_pktmbuf_mtod(buf, unsigned char *) + eth_offset +
								sizeof(struct ipv6_hdr));
				hash = ipv6_hash(ipv6_hdr) ^ udp_hash(udp_hdr);
			} else {
				hash = ipv6_hash(ipv6_hdr);
			}
		}
		break;
	}

	hash ^= hash >> 16;
	hash ^= hash >> 8;

	return hash % slave_count;
}

static uint16_t
bond_ethdev_tx_balance(void *queue, struct rte_mbuf **bufs, uint16_t nb_pkts)
{
	struct bond_dev_private *internals;
	struct bond_tx_queue *bd_tx_q;

	uint8_t num_of_slaves;
	uint8_t slaves[RTE_MAX_ETHPORTS];

	uint16_t num_tx_total = 0;

	int i, op_slave_id;

	struct rte_mbuf *slave_bufs[RTE_MAX_ETHPORTS][nb_pkts];
	uint16_t slave_nb_pkts[RTE_MAX_ETHPORTS] = { 0 };

	bd_tx_q = (struct bond_tx_queue *)queue;
	internals = bd_tx_q->dev_private;

	/* Copy slave list to protect against slave up/down changes during tx
	 * bursting */
	num_of_slaves = internals->active_slave_count;
	memcpy(slaves, internals->active_slaves,
			sizeof(internals->active_slaves[0]) * num_of_slaves);

	if (num_of_slaves < 1)
		return num_tx_total;

	/* Populate slaves mbuf with the packets which are to be sent on it  */
	for (i = 0; i < nb_pkts; i++) {
		/* Select output slave using hash based on xmit policy */
		op_slave_id = xmit_slave_hash(bufs[i], num_of_slaves,
				internals->balance_xmit_policy);

		/* Populate slave mbuf arrays with mbufs for that slave */
		slave_bufs[op_slave_id][slave_nb_pkts[op_slave_id]++] = bufs[i];
	}

	/* Send packet burst on each slave device */
	for (i = 0; i < num_of_slaves; i++) {
		if (slave_nb_pkts[i] > 0) {
			num_tx_total += rte_eth_tx_burst(slaves[i], bd_tx_q->queue_id,
					slave_bufs[i], slave_nb_pkts[i]);
		}
	}

	return num_tx_total;
}

static uint16_t
bond_ethdev_tx_burst_broadcast(void *queue, struct rte_mbuf **bufs,
		uint16_t nb_pkts)
{
	struct bond_dev_private *internals;
	struct bond_tx_queue *bd_tx_q;

	uint8_t num_of_slaves;
	uint8_t slaves[RTE_MAX_ETHPORTS];

	uint16_t num_tx_total = 0;

	int i;

	bd_tx_q = (struct bond_tx_queue *)queue;
	internals = bd_tx_q->dev_private;

	/* Copy slave list to protect against slave up/down changes during tx
	 * bursting */
	num_of_slaves = internals->active_slave_count;
	memcpy(slaves, internals->active_slaves,
			sizeof(internals->active_slaves[0]) * num_of_slaves);

	if (num_of_slaves < 1)
		return 0;

	/* Increment reference count on mbufs */
	for (i = 0; i < nb_pkts; i++)
		rte_mbuf_refcnt_update(bufs[i], num_of_slaves - 1);

	/* Transmit burst on each active slave */
	for (i = 0; i < num_of_slaves; i++)
		num_tx_total += rte_eth_tx_burst(slaves[i], bd_tx_q->queue_id,
				bufs, nb_pkts);

	return num_tx_total;
}

void
link_properties_set(struct rte_eth_dev *bonded_eth_dev,
		struct rte_eth_link *slave_dev_link)
{
	struct rte_eth_link *bonded_dev_link = &bonded_eth_dev->data->dev_link;
	struct bond_dev_private *internals = bonded_eth_dev->data->dev_private;

	if (slave_dev_link->link_status &&
		bonded_eth_dev->data->dev_started) {
		bonded_dev_link->link_duplex = slave_dev_link->link_duplex;
		bonded_dev_link->link_speed = slave_dev_link->link_speed;

		internals->link_props_set = 1;
	}
}

void
link_properties_reset(struct rte_eth_dev *bonded_eth_dev)
{
	struct bond_dev_private *internals = bonded_eth_dev->data->dev_private;

	memset(&(bonded_eth_dev->data->dev_link), 0,
			sizeof(bonded_eth_dev->data->dev_link));

	internals->link_props_set = 0;
}

int
link_properties_valid(struct rte_eth_link *bonded_dev_link,
		struct rte_eth_link *slave_dev_link)
{
	if (bonded_dev_link->link_duplex != slave_dev_link->link_duplex ||
		bonded_dev_link->link_speed !=  slave_dev_link->link_speed)
		return -1;

	return 0;
}

int
mac_address_set(struct rte_eth_dev *eth_dev, struct ether_addr *new_mac_addr)
{
	struct ether_addr *mac_addr;

	mac_addr = eth_dev->data->mac_addrs;

	if (eth_dev == NULL) {
		RTE_LOG(ERR, PMD, "%s: NULL pointer eth_dev specified\n", __func__);
		return -1;
	}

	if (new_mac_addr == NULL) {
		RTE_LOG(ERR, PMD, "%s: NULL pointer MAC specified\n", __func__);
		return -1;
	}

	/* if new MAC is different to current MAC then update */
	if (memcmp(mac_addr, new_mac_addr, sizeof(*mac_addr)) != 0)
		memcpy(mac_addr, new_mac_addr, sizeof(*mac_addr));

	return 0;
}

int
mac_address_slaves_update(struct rte_eth_dev *bonded_eth_dev)
{
	struct bond_dev_private *internals = bonded_eth_dev->data->dev_private;
	int i;

	/* Update slave devices MAC addresses */
	if (internals->slave_count < 1)
		return -1;

	switch (internals->mode) {
	case BONDING_MODE_ROUND_ROBIN:
	case BONDING_MODE_BALANCE:
	case BONDING_MODE_BROADCAST:
		for (i = 0; i < internals->slave_count; i++) {
			if (mac_address_set(&rte_eth_devices[internals->slaves[i]],
					bonded_eth_dev->data->mac_addrs)) {
				RTE_LOG(ERR, PMD,
						"%s: Failed to update port Id %d MAC address\n",
						__func__, internals->slaves[i]);
				return -1;
			}
		}
		break;
	case BONDING_MODE_ACTIVE_BACKUP:
	default:
		for (i = 0; i < internals->slave_count; i++) {
			if (internals->slaves[i] == internals->current_primary_port) {
				if (mac_address_set(&rte_eth_devices[internals->primary_port],
						bonded_eth_dev->data->mac_addrs)) {
					RTE_LOG(ERR, PMD,
							"%s: Failed to update port Id %d MAC address\n",
							__func__, internals->current_primary_port);
				}
			} else {
				struct slave_conf *conf =
						slave_config_get(internals, internals->slaves[i]);

				if (mac_address_set(&rte_eth_devices[internals->slaves[i]],
						&conf->mac_addr)) {
					RTE_LOG(ERR, PMD,
							"%s: Failed to update port Id %d MAC address\n",
							__func__, internals->slaves[i]);

					return -1;
				}
			}
		}
	}

	return 0;
}

int
bond_ethdev_mode_set(struct rte_eth_dev *eth_dev, int mode)
{
	struct bond_dev_private *internals;

	internals = eth_dev->data->dev_private;

	switch (mode) {
	case BONDING_MODE_ROUND_ROBIN:
		eth_dev->tx_pkt_burst = bond_ethdev_tx_round_robin;
		break;
	case BONDING_MODE_ACTIVE_BACKUP:
		eth_dev->tx_pkt_burst = bond_ethdev_tx_active_backup;
		break;
	case BONDING_MODE_BALANCE:
		eth_dev->tx_pkt_burst = bond_ethdev_tx_balance;
		break;
	case BONDING_MODE_BROADCAST:
		eth_dev->tx_pkt_burst = bond_ethdev_tx_burst_broadcast;
		break;
	default:
		return -1;
	}

	eth_dev->rx_pkt_burst = bond_ethdev_rx_burst;
	internals->mode = mode;

	return 0;
}

int
slave_configure(struct rte_eth_dev *bonded_eth_dev,
		struct rte_eth_dev *slave_eth_dev)
{
	struct bond_rx_queue *bd_rx_q;
	struct bond_tx_queue *bd_tx_q;

	int q_id;

	/* Stop slave */
	rte_eth_dev_stop(slave_eth_dev->data->port_id);

	/* Enable interrupts on slave device */
	slave_eth_dev->data->dev_conf.intr_conf.lsc = 1;

	if (rte_eth_dev_configure(slave_eth_dev->data->port_id,
			bonded_eth_dev->data->nb_rx_queues,
			bonded_eth_dev->data->nb_tx_queues,
			&(slave_eth_dev->data->dev_conf)) != 0) {
		RTE_LOG(ERR, PMD, "Cannot configure slave device: port=%u\n",
				slave_eth_dev->data->port_id);
		return -1;
	}

	/* Setup Rx Queues */
	for (q_id = 0; q_id < bonded_eth_dev->data->nb_rx_queues; q_id++) {
		bd_rx_q = (struct bond_rx_queue *)bonded_eth_dev->data->rx_queues[q_id];

		if (rte_eth_rx_queue_setup(slave_eth_dev->data->port_id, q_id,
				bd_rx_q->nb_rx_desc,
				rte_eth_dev_socket_id(slave_eth_dev->data->port_id),
				&(bd_rx_q->rx_conf), bd_rx_q->mb_pool) != 0) {
			RTE_LOG(ERR, PMD, "rte_eth_rx_queue_setup: port=%d queue_id %d\n",
					slave_eth_dev->data->port_id, q_id);
			return -1;
		}
	}

	/* Setup Tx Queues */
	for (q_id = 0; q_id < bonded_eth_dev->data->nb_tx_queues; q_id++) {
		bd_tx_q = (struct bond_tx_queue *)bonded_eth_dev->data->tx_queues[q_id];

		if (rte_eth_tx_queue_setup(slave_eth_dev->data->port_id, q_id,
				bd_tx_q->nb_tx_desc,
				rte_eth_dev_socket_id(slave_eth_dev->data->port_id),
				&bd_tx_q->tx_conf) != 0) {
			RTE_LOG(ERR, PMD, "rte_eth_tx_queue_setup: port=%d queue_id %d\n",
					slave_eth_dev->data->port_id, q_id);
			return -1;
		}
	}

	/* Start device */
	if (rte_eth_dev_start(slave_eth_dev->data->port_id) != 0) {
		RTE_LOG(ERR, PMD, "rte_eth_dev_start: port=%u\n",
				slave_eth_dev->data->port_id);
		return -1;
	}

	return 0;
}

struct slave_conf *
slave_config_get(struct bond_dev_private *internals, uint8_t slave_port_id)
{
	int i;

	for (i = 0; i < internals->slave_count; i++) {
		if (internals->presisted_slaves_conf[i].port_id == slave_port_id)
			return &internals->presisted_slaves_conf[i];
	}
	return NULL;
}

void
slave_config_clear(struct bond_dev_private *internals,
		struct rte_eth_dev *slave_eth_dev)
{
	int i, found = 0;

	for (i = 0; i < internals->slave_count; i++) {
		if (internals->presisted_slaves_conf[i].port_id ==
				slave_eth_dev->data->port_id) {
			found = 1;
			memset(&internals->presisted_slaves_conf[i], 0,
					sizeof(internals->presisted_slaves_conf[i]));
		}
		if (found && i < (internals->slave_count - 1)) {
			memcpy(&internals->presisted_slaves_conf[i],
					&internals->presisted_slaves_conf[i+1],
					sizeof(internals->presisted_slaves_conf[i]));
		}
	}
}

void
slave_config_store(struct bond_dev_private *internals,
		struct rte_eth_dev *slave_eth_dev)
{
	struct slave_conf *presisted_slave_conf =
			&internals->presisted_slaves_conf[internals->slave_count];

	presisted_slave_conf->port_id = slave_eth_dev->data->port_id;

	memcpy(&(presisted_slave_conf->mac_addr), slave_eth_dev->data->mac_addrs,
			sizeof(struct ether_addr));
}

void
bond_ethdev_primary_set(struct bond_dev_private *internals,
		uint8_t slave_port_id)
{
	int i;

	if (internals->active_slave_count < 1)
		internals->current_primary_port = slave_port_id;
	else
		/* Search bonded device slave ports for new proposed primary port */
		for (i = 0; i < internals->active_slave_count; i++) {
			if (internals->active_slaves[i] == slave_port_id)
				internals->current_primary_port = slave_port_id;
		}
}

static void
bond_ethdev_promiscuous_enable(struct rte_eth_dev *eth_dev);

static int
bond_ethdev_start(struct rte_eth_dev *eth_dev)
{
	struct bond_dev_private *internals;
	int i;

	/* slave eth dev will be started by bonded device */
	if (valid_bonded_ethdev(eth_dev)) {
		RTE_LOG(ERR, PMD,
				"%s: user tried to explicitly start a slave eth_dev (%d) of the bonded eth_dev\n",
				__func__, eth_dev->data->port_id);
		return -1;
	}

	eth_dev->data->dev_link.link_status = 1;
	eth_dev->data->dev_started = 1;

	internals = eth_dev->data->dev_private;

	if (internals->slave_count == 0) {
		RTE_LOG(ERR, PMD,
				"%s: Cannot start port since there are no slave devices\n",
				__func__);
		return -1;
	}

	if (internals->user_defined_mac == 0) {
		struct slave_conf *conf = slave_config_get(internals,
				internals->primary_port);

		if (mac_address_set(eth_dev, &(conf->mac_addr)) != 0) {
			RTE_LOG(ERR, PMD,
					"bonded port (%d) failed to update mac address",
					eth_dev->data->port_id);
			return -1;
		}
	}

	/* Update all slave devices MACs*/
	if (mac_address_slaves_update(eth_dev) != 0)
		return -1;

	/* If bonded device is configure in promiscuous mode then re-apply config */
	if (internals->promiscuous_en)
		bond_ethdev_promiscuous_enable(eth_dev);

	/* Reconfigure each slave device if starting bonded device */
	for (i = 0; i < internals->slave_count; i++) {
		if (slave_configure(eth_dev, &(rte_eth_devices[internals->slaves[i]]))
				!= 0) {
			RTE_LOG(ERR, PMD,
					"bonded port (%d) failed to reconfigure slave device %d)",
					eth_dev->data->port_id, internals->slaves[i]);
			return -1;
		}
	}

	if (internals->user_defined_primary_port)
		bond_ethdev_primary_set(internals, internals->primary_port);

	return 0;
}

static void
bond_ethdev_stop(struct rte_eth_dev *eth_dev)
{
	struct bond_dev_private *internals = eth_dev->data->dev_private;

	internals->active_slave_count = 0;

	eth_dev->data->dev_link.link_status = 0;
	eth_dev->data->dev_started = 0;
}

static void
bond_ethdev_close(struct rte_eth_dev *dev __rte_unused)
{
}

static int
bond_ethdev_configure(struct rte_eth_dev *dev __rte_unused)
{
	return 0;
}

static void
bond_ethdev_info(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	dev_info->driver_name = driver_name;
	dev_info->max_mac_addrs = 1;

	dev_info->max_rx_pktlen = (uint32_t)2048;

	dev_info->max_rx_queues = (uint16_t)128;
	dev_info->max_tx_queues = (uint16_t)512;

	dev_info->min_rx_bufsize = 0;
	dev_info->pci_dev = dev->pci_dev;
}

static int
bond_ethdev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id __rte_unused,
		const struct rte_eth_rxconf *rx_conf, struct rte_mempool *mb_pool)
{
	struct bond_rx_queue *bd_rx_q = (struct bond_rx_queue *)
			rte_zmalloc_socket(NULL, sizeof(struct bond_rx_queue),
					0, dev->pci_dev->numa_node);
	if (bd_rx_q == NULL)
		return -1;

	bd_rx_q->queue_id = rx_queue_id;
	bd_rx_q->dev_private = dev->data->dev_private;

	bd_rx_q->nb_rx_desc = nb_rx_desc;

	memcpy(&(bd_rx_q->rx_conf), rx_conf, sizeof(struct rte_eth_rxconf));
	bd_rx_q->mb_pool = mb_pool;

	dev->data->rx_queues[rx_queue_id] = bd_rx_q;

	return 0;
}

static int
bond_ethdev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id __rte_unused,
		const struct rte_eth_txconf *tx_conf)
{
	struct bond_tx_queue *bd_tx_q  = (struct bond_tx_queue *)
			rte_zmalloc_socket(NULL, sizeof(struct bond_tx_queue),
					0, dev->pci_dev->numa_node);

	if (bd_tx_q == NULL)
			return -1;

	bd_tx_q->queue_id = tx_queue_id;
	bd_tx_q->dev_private = dev->data->dev_private;

	bd_tx_q->nb_tx_desc = nb_tx_desc;
	memcpy(&(bd_tx_q->tx_conf), tx_conf, sizeof(bd_tx_q->tx_conf));

	dev->data->tx_queues[tx_queue_id] = bd_tx_q;

	return 0;
}

static void
bond_ethdev_rx_queue_release(void *queue)
{
	if (queue == NULL)
		return;

	rte_free(queue);
}

static void
bond_ethdev_tx_queue_release(void *queue)
{
	if (queue == NULL)
		return;

	rte_free(queue);
}

static int
bond_ethdev_link_update(struct rte_eth_dev *bonded_eth_dev,
		int wait_to_complete)
{
	struct bond_dev_private *internals = bonded_eth_dev->data->dev_private;

	if (!bonded_eth_dev->data->dev_started ||
		internals->active_slave_count == 0) {
		bonded_eth_dev->data->dev_link.link_status = 0;
		return 0;
	} else {
		struct rte_eth_dev *slave_eth_dev;
		int i, link_up = 0;

		for (i = 0; i < internals->active_slave_count; i++) {
			slave_eth_dev = &rte_eth_devices[internals->active_slaves[i]];

			(*slave_eth_dev->dev_ops->link_update)(slave_eth_dev,
					wait_to_complete);
			if (slave_eth_dev->data->dev_link.link_status == 1) {
				link_up = 1;
				break;
			}
		}

		bonded_eth_dev->data->dev_link.link_status = link_up;
	}

	return 0;
}

static void
bond_ethdev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct bond_dev_private *internals = dev->data->dev_private;
	struct rte_eth_stats slave_stats;

	int i;

	/* clear bonded stats before populating from slaves */
	memset(stats, 0, sizeof(*stats));

	for (i = 0; i < internals->slave_count; i++) {
		rte_eth_stats_get(internals->slaves[i], &slave_stats);

		stats->ipackets += slave_stats.ipackets;
		stats->opackets += slave_stats.opackets;
		stats->ibytes += slave_stats.ibytes;
		stats->obytes += slave_stats.obytes;
		stats->ierrors += slave_stats.ierrors;
		stats->oerrors += slave_stats.oerrors;
		stats->imcasts += slave_stats.imcasts;
		stats->rx_nombuf += slave_stats.rx_nombuf;
		stats->fdirmatch += slave_stats.fdirmatch;
		stats->fdirmiss += slave_stats.fdirmiss;
		stats->tx_pause_xon += slave_stats.tx_pause_xon;
		stats->rx_pause_xon += slave_stats.rx_pause_xon;
		stats->tx_pause_xoff += slave_stats.tx_pause_xoff;
		stats->rx_pause_xoff += slave_stats.rx_pause_xoff;
	}
}

static void
bond_ethdev_stats_reset(struct rte_eth_dev *dev)
{
	struct bond_dev_private *internals = dev->data->dev_private;
	int i;

	for (i = 0; i < internals->slave_count; i++)
		rte_eth_stats_reset(internals->slaves[i]);
}

static void
bond_ethdev_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
	struct bond_dev_private *internals = eth_dev->data->dev_private;
	int i;

	internals->promiscuous_en = 1;

	switch (internals->mode) {
	/* Promiscuous mode is propagated to all slaves */
	case BONDING_MODE_ROUND_ROBIN:
	case BONDING_MODE_BALANCE:
	case BONDING_MODE_BROADCAST:
		for (i = 0; i < internals->slave_count; i++)
			rte_eth_promiscuous_enable(internals->slaves[i]);
		break;
	/* Promiscuous mode is propagated only to primary slave */
	case BONDING_MODE_ACTIVE_BACKUP:
	default:
		rte_eth_promiscuous_enable(internals->current_primary_port);

	}
}

static void
bond_ethdev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct bond_dev_private *internals = dev->data->dev_private;
	int i;

	internals->promiscuous_en = 0;

	switch (internals->mode) {
	/* Promiscuous mode is propagated to all slaves */
	case BONDING_MODE_ROUND_ROBIN:
	case BONDING_MODE_BALANCE:
	case BONDING_MODE_BROADCAST:
		for (i = 0; i < internals->slave_count; i++)
			rte_eth_promiscuous_disable(internals->slaves[i]);
		break;
	/* Promiscuous mode is propagated only to primary slave */
	case BONDING_MODE_ACTIVE_BACKUP:
	default:
		rte_eth_promiscuous_disable(internals->current_primary_port);
	}
}

void
bond_ethdev_lsc_event_callback(uint8_t port_id, enum rte_eth_event_type type,
		void *param)
{
	struct rte_eth_dev *bonded_eth_dev, *slave_eth_dev;
	struct bond_dev_private *internals;
	struct rte_eth_link link;

	int i, bonded_port_id, valid_slave, active_pos = -1;

	if (type != RTE_ETH_EVENT_INTR_LSC)
		return;

	if (param == NULL)
		return;

	bonded_port_id = *(uint8_t *)param;

	bonded_eth_dev = &rte_eth_devices[bonded_port_id];
	slave_eth_dev = &rte_eth_devices[port_id];

	if (valid_bonded_ethdev(bonded_eth_dev))
		return;

	internals = bonded_eth_dev->data->dev_private;

	/* If the device isn't started don't handle interrupts */
	if (!bonded_eth_dev->data->dev_started)
		return;

	/* verify that port_id is a valid slave of bonded port */
	for (i = 0; i < internals->slave_count; i++) {
		if (internals->slaves[i] == port_id) {
			valid_slave = 1;
			break;
		}
	}

	if (!valid_slave)
		return;

	/* Search for port in active port list */
	for (i = 0; i < internals->active_slave_count; i++) {
		if (port_id == internals->active_slaves[i]) {
			active_pos = i;
			break;
		}
	}

	rte_eth_link_get_nowait(port_id, &link);
	if (link.link_status) {
		if (active_pos == -1) {
			/* if no active slave ports then set this port to be primary port */
			if (internals->active_slave_count == 0) {
				/* If first active slave, then change link status */
				bonded_eth_dev->data->dev_link.link_status = 1;
				internals->current_primary_port = port_id;

				/* Inherit eth dev link properties from first active slave */
				link_properties_set(bonded_eth_dev,
						&(slave_eth_dev->data->dev_link));

			}
			internals->active_slaves[internals->active_slave_count++] = port_id;

			/* If user has defined the primary port then default to using it */
			if (internals->user_defined_primary_port &&
					internals->primary_port == port_id)
				bond_ethdev_primary_set(internals, port_id);

		}
	} else {
		if (active_pos != -1) {
			/* Remove from active slave list */
			for (i = active_pos; i < (internals->active_slave_count - 1); i++)
				internals->active_slaves[i] = internals->active_slaves[i+1];

			internals->active_slave_count--;

			/* No active slaves, change link status to down and reset other
			 * link properties */
			if (internals->active_slave_count == 0)
				link_properties_reset(bonded_eth_dev);

			/* Update primary id, take first active slave from list or if none
			 * available set to -1 */
			if (port_id == internals->current_primary_port) {
				if (internals->active_slave_count > 0)
					bond_ethdev_primary_set(internals,
							internals->active_slaves[0]);
				else
					internals->current_primary_port = internals->primary_port;
			}
		}
	}
}

struct eth_dev_ops default_dev_ops = {
		.dev_start = bond_ethdev_start,
		.dev_stop = bond_ethdev_stop,
		.dev_close = bond_ethdev_close,
		.dev_configure = bond_ethdev_configure,
		.dev_infos_get = bond_ethdev_info,
		.rx_queue_setup = bond_ethdev_rx_queue_setup,
		.tx_queue_setup = bond_ethdev_tx_queue_setup,
		.rx_queue_release = bond_ethdev_rx_queue_release,
		.tx_queue_release = bond_ethdev_tx_queue_release,
		.link_update = bond_ethdev_link_update,
		.stats_get = bond_ethdev_stats_get,
		.stats_reset = bond_ethdev_stats_reset,
		.promiscuous_enable = bond_ethdev_promiscuous_enable,
		.promiscuous_disable = bond_ethdev_promiscuous_disable
};

static int
bond_init(const char *name, const char *params)
{
	struct rte_kvargs *kvlist;
	uint8_t bonding_mode, socket_id;
	int  arg_count, port_id;

	RTE_LOG(INFO, EAL, "Initializing pmd_bond for %s\n", name);

	kvlist = rte_kvargs_parse(params, pmd_bond_init_valid_arguments);
	if (kvlist == NULL)
		return -1;

	/* Parse link bonding mode */
	if (rte_kvargs_count(kvlist, PMD_BOND_MODE_KVARG) == 1) {
		if (rte_kvargs_process(kvlist, PMD_BOND_MODE_KVARG,
				&bond_ethdev_parse_slave_mode_kvarg, &bonding_mode) != 0) {
			RTE_LOG(ERR, EAL, "Invalid mode for bonded device %s\n", name);
			return -1;
		}
	} else {
		RTE_LOG(ERR, EAL,
				"Mode must be specified only once for bonded device %s\n",
				name);
		return -1;
	}

	/* Parse socket id to create bonding device on */
	arg_count = rte_kvargs_count(kvlist, PMD_BOND_SOCKET_ID_KVARG);
	if (arg_count == 1) {
		if (rte_kvargs_process(kvlist, PMD_BOND_SOCKET_ID_KVARG,
				&bond_ethdev_parse_socket_id_kvarg, &socket_id) != 0) {
			RTE_LOG(ERR, EAL,
					"Invalid socket Id specified for bonded device %s\n",
					name);
			return -1;
		}
	} else if (arg_count > 1) {
		RTE_LOG(ERR, EAL,
				"Socket Id can be specified only once for bonded device %s\n",
				name);
		return -1;
	} else {
		socket_id = rte_socket_id();
	}

	/* Create link bonding eth device */
	port_id = rte_eth_bond_create(name, bonding_mode, socket_id);
	if (port_id < 0) {
		RTE_LOG(ERR, EAL,
				"Failed to create socket %s in mode %u on socket %u.\n",
				name, bonding_mode, socket_id);
		return -1;
	}

	RTE_LOG(INFO, EAL,
			"Create bonded device %s on port %d in mode %u on socket %u.\n",
			name, port_id, bonding_mode, socket_id);

	/* Parse MAC address for bonded device */
	arg_count = rte_kvargs_count(kvlist, PMD_BOND_MAC_ADDR_KVARG);
	if (arg_count == 1) {
		struct ether_addr bond_mac;

		if (rte_kvargs_process(kvlist, PMD_BOND_MAC_ADDR_KVARG,
				&bond_ethdev_parse_bond_mac_addr_kvarg, &bond_mac) < 0) {
			RTE_LOG(INFO, EAL, "Invalid mac address for bonded device %s\n",
					name);
			return -1;
		}

		/* Set MAC address */
		if (rte_eth_bond_mac_address_set(port_id, &bond_mac) != 0) {
			RTE_LOG(ERR, EAL,
					"Failed to set mac address on bonded device %s\n",
					name);
			return -1;
		}
	} else if (arg_count > 1) {
		RTE_LOG(ERR, EAL,
				"MAC address can be specified only once for bonded device %s\n",
				name);
		return -1;
	}

	/* Parse/set balance mode transmit policy */
	arg_count = rte_kvargs_count(kvlist, PMD_BOND_XMIT_POLICY_KVARG);
	if (arg_count == 1) {
		uint8_t xmit_policy;

		if (rte_kvargs_process(kvlist, PMD_BOND_XMIT_POLICY_KVARG,
				&bond_ethdev_parse_balance_xmit_policy_kvarg, &xmit_policy) !=
						0) {
			RTE_LOG(INFO, EAL,
					"Invalid xmit policy specified for bonded device %s\n",
					name);
			return -1;
		}

		/* Set balance mode transmit policy*/
		if (rte_eth_bond_xmit_policy_set(port_id, xmit_policy) != 0) {
			RTE_LOG(ERR, EAL,
					"Failed to set balance xmit policy on bonded device %s\n",
					name);
			return -1;
		}
	} else if (arg_count > 1) {
		RTE_LOG(ERR, EAL,
				"Transmit policy can be specified only once for bonded device %s\n",
				name);
		return -1;
	}

	/* Parse/add slave ports to bonded device */
	if (rte_kvargs_count(kvlist, PMD_BOND_SLAVE_PORT_KVARG) > 0) {
		struct bond_ethdev_slave_ports slave_ports;
		unsigned i;

		memset(&slave_ports, 0, sizeof(slave_ports));

		if (rte_kvargs_process(kvlist, PMD_BOND_SLAVE_PORT_KVARG,
				&bond_ethdev_parse_slave_port_kvarg, &slave_ports) != 0) {
			RTE_LOG(ERR, EAL,
					"Failed to parse slave ports for bonded device %s\n",
					name);
			return -1;
		}

		for (i = 0; i < slave_ports.slave_count; i++) {
			if (rte_eth_bond_slave_add(port_id, slave_ports.slaves[i]) != 0) {
				RTE_LOG(ERR, EAL,
						"Failed to add port %d as slave to bonded device %s\n",
						slave_ports.slaves[i], name);
			}
		}

	} else {
		RTE_LOG(INFO, EAL, "No slaves specified for bonded device %s\n", name);
		return -1;
	}

	/* Parse/set primary slave port id*/
	arg_count = rte_kvargs_count(kvlist, PMD_BOND_PRIMARY_SLAVE_KVARG);
	if (arg_count == 1) {
		uint8_t primary_slave_port_id;

		if (rte_kvargs_process(kvlist,
				PMD_BOND_PRIMARY_SLAVE_KVARG,
				&bond_ethdev_parse_primary_slave_port_id_kvarg,
				&primary_slave_port_id) < 0) {
			RTE_LOG(INFO, EAL,
					"Invalid primary slave port id specified for bonded device %s\n",
					name);
			return -1;
		}

		/* Set balance mode transmit policy*/
		if (rte_eth_bond_primary_set(port_id, (uint8_t)primary_slave_port_id)
				!= 0) {
			RTE_LOG(ERR, EAL,
					"Failed to set primary slave port %d on bonded device %s\n",
					primary_slave_port_id, name);
			return -1;
		}
	} else if (arg_count > 1) {
		RTE_LOG(INFO, EAL,
				"Primary slave can be specified only once for bonded device %s\n",
				name);
		return -1;
	}

	return 0;
}

static struct rte_driver bond_drv = {
	.name = PMD_BOND_NAME,
	.type = PMD_BDEV,
	.init = bond_init,
};

PMD_REGISTER_DRIVER(bond_drv);
