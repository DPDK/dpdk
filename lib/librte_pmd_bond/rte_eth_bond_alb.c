/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#include "rte_eth_bond_private.h"
#include "rte_eth_bond_alb.h"

static inline uint8_t
simple_hash(uint8_t *hash_start, int hash_size)
{
	int i;
	uint8_t hash;

	hash = 0;
	for (i = 0; i < hash_size; ++i)
		hash ^= hash_start[i];

	return hash;
}

static uint8_t
calculate_slave(struct bond_dev_private *internals)
{
	uint8_t idx;

	idx = (internals->mode6.last_slave + 1) % internals->active_slave_count;
	internals->mode6.last_slave = idx;
	return internals->active_slaves[idx];
}

int
bond_mode_alb_enable(struct rte_eth_dev *bond_dev)
{
	struct bond_dev_private *internals = bond_dev->data->dev_private;
	struct client_data *hash_table = internals->mode6.client_table;

	uint16_t data_size;
	char mem_name[RTE_ETH_NAME_MAX_LEN];
	int socket_id = bond_dev->pci_dev->numa_node;

	/* Fill hash table with initial values */
	memset(hash_table, 0, sizeof(struct client_data) * ALB_HASH_TABLE_SIZE);
	rte_spinlock_init(&internals->mode6.lock);
	internals->mode6.last_slave = ALB_NULL_INDEX;
	internals->mode6.ntt = 0;

	/* Initialize memory pool for ARP packets to send */
	if (internals->mode6.mempool == NULL) {
		/*
		 * 256 is size of ETH header, ARP header and nested VLAN headers.
		 * The value is chosen to be cache aligned.
		 */
		data_size = 256 + RTE_PKTMBUF_HEADROOM;
		snprintf(mem_name, sizeof(mem_name), "%s_MODE6", bond_dev->data->name);
		internals->mode6.mempool = rte_pktmbuf_pool_create(mem_name,
			512 * RTE_MAX_ETHPORTS,
			RTE_MEMPOOL_CACHE_MAX_SIZE >= 32 ?
				32 : RTE_MEMPOOL_CACHE_MAX_SIZE,
			0, data_size, socket_id);

		if (internals->mode6.mempool == NULL) {
			RTE_LOG(ERR, PMD, "%s: Failed to initialize ALB mempool.\n",
					bond_dev->data->name);
			rte_panic(
					"Failed to allocate memory pool ('%s')\n"
					"for bond device '%s'\n",
					mem_name, bond_dev->data->name);
		}
	}

	return 0;
}

void bond_mode_alb_arp_recv(struct ether_hdr *eth_h, uint16_t offset,
		struct bond_dev_private *internals) {
	struct arp_hdr *arp;

	struct client_data *hash_table = internals->mode6.client_table;
	struct client_data *client_info;

	uint8_t hash_index;

	arp = (struct arp_hdr *) ((char *) (eth_h + 1) + offset);

	/* ARP Requests are forwarded to the application with no changes */
	if (arp->arp_op != rte_cpu_to_be_16(ARP_OP_REPLY))
		return;

	/* From now on, we analyze only ARP Reply packets */
	hash_index = simple_hash((uint8_t *) &arp->arp_data.arp_sip,
			sizeof(arp->arp_data.arp_sip));
	client_info = &hash_table[hash_index];

	/*
	 * We got reply for ARP Request send by the application. We need to
	 * update client table when received data differ from what is stored
	 * in ALB table and issue sending update packet to that slave.
	 */
	rte_spinlock_lock(&internals->mode6.lock);
	if (client_info->in_use == 0 ||
			client_info->app_ip != arp->arp_data.arp_tip ||
			client_info->cli_ip != arp->arp_data.arp_sip ||
			!is_same_ether_addr(&client_info->cli_mac, &arp->arp_data.arp_sha) ||
			client_info->vlan_count != offset / sizeof(struct vlan_hdr) ||
			memcmp(client_info->vlan, eth_h + 1, offset) != 0
	) {
		client_info->in_use = 1;
		client_info->app_ip = arp->arp_data.arp_tip;
		client_info->cli_ip = arp->arp_data.arp_sip;
		ether_addr_copy(&arp->arp_data.arp_sha, &client_info->cli_mac);
		client_info->slave_idx = calculate_slave(internals);
		rte_eth_macaddr_get(client_info->slave_idx, &client_info->app_mac);
		ether_addr_copy(&client_info->app_mac, &arp->arp_data.arp_tha);
		memcpy(client_info->vlan, eth_h + 1, offset);
		client_info->vlan_count = offset / sizeof(struct vlan_hdr);
	}
	internals->mode6.ntt = 1;
	rte_spinlock_unlock(&internals->mode6.lock);
}

uint8_t
bond_mode_alb_arp_xmit(struct ether_hdr *eth_h, uint16_t offset,
		struct bond_dev_private *internals)
{
	struct arp_hdr *arp;

	struct client_data *hash_table = internals->mode6.client_table;
	struct client_data *client_info;

	uint8_t hash_index;

	struct ether_addr bonding_mac;

	arp = (struct arp_hdr *)((char *)(eth_h + 1) + offset);

	/*
	 * Traffic with src MAC other than bonding should be sent on
	 * current primary port.
	 */
	rte_eth_macaddr_get(internals->port_id, &bonding_mac);
	if (!is_same_ether_addr(&bonding_mac, &arp->arp_data.arp_sha)) {
		rte_eth_macaddr_get(internals->current_primary_port,
				&arp->arp_data.arp_sha);
		return internals->current_primary_port;
	}

	hash_index = simple_hash((uint8_t *)&arp->arp_data.arp_tip,
			sizeof(uint32_t));
	client_info = &hash_table[hash_index];

	rte_spinlock_lock(&internals->mode6.lock);
	if (arp->arp_op == rte_cpu_to_be_16(ARP_OP_REPLY)) {
		if (client_info->in_use) {
			if (client_info->app_ip == arp->arp_data.arp_sip &&
				client_info->cli_ip == arp->arp_data.arp_tip) {
				/* Entry is already assigned to this client */
				if (!is_broadcast_ether_addr(&arp->arp_data.arp_tha)) {
					ether_addr_copy(&arp->arp_data.arp_tha,
							&client_info->cli_mac);
				}
				rte_eth_macaddr_get(client_info->slave_idx,
						&client_info->app_mac);
				ether_addr_copy(&client_info->app_mac, &arp->arp_data.arp_sha);
				memcpy(client_info->vlan, eth_h + 1, offset);
				client_info->vlan_count = offset / sizeof(struct vlan_hdr);
				rte_spinlock_unlock(&internals->mode6.lock);
				return client_info->slave_idx;
			}
		}

		/* Assign new slave to this client and update src mac in ARP */
		client_info->in_use = 1;
		client_info->ntt = 0;
		client_info->app_ip = arp->arp_data.arp_sip;
		ether_addr_copy(&arp->arp_data.arp_tha, &client_info->cli_mac);
		client_info->cli_ip = arp->arp_data.arp_tip;
		client_info->slave_idx = calculate_slave(internals);
		rte_eth_macaddr_get(client_info->slave_idx, &client_info->app_mac);
		ether_addr_copy(&client_info->app_mac, &arp->arp_data.arp_sha);
		memcpy(client_info->vlan, eth_h + 1, offset);
		client_info->vlan_count = offset / sizeof(struct vlan_hdr);
		rte_spinlock_unlock(&internals->mode6.lock);
		return client_info->slave_idx;
	}

	/* If packet is not ARP Reply, send it on current primary port. */
	rte_spinlock_unlock(&internals->mode6.lock);
	rte_eth_macaddr_get(internals->current_primary_port,
			&arp->arp_data.arp_sha);
	return internals->current_primary_port;
}

uint8_t
bond_mode_alb_arp_upd(struct client_data *client_info,
		struct rte_mbuf *pkt, struct bond_dev_private *internals)
{
	struct ether_hdr *eth_h;
	struct arp_hdr *arp_h;
	uint8_t slave_idx;

	rte_spinlock_lock(&internals->mode6.lock);
	eth_h = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

	ether_addr_copy(&client_info->app_mac, &eth_h->s_addr);
	ether_addr_copy(&client_info->cli_mac, &eth_h->d_addr);
	if (client_info->vlan_count > 0)
		eth_h->ether_type = rte_cpu_to_be_16(ETHER_TYPE_VLAN);
	else
		eth_h->ether_type = rte_cpu_to_be_16(ETHER_TYPE_ARP);

	arp_h = (struct arp_hdr *)((char *)eth_h + sizeof(struct ether_hdr)
			+ client_info->vlan_count * sizeof(struct vlan_hdr));

	memcpy(eth_h + 1, client_info->vlan,
			client_info->vlan_count * sizeof(struct vlan_hdr));

	ether_addr_copy(&client_info->app_mac, &arp_h->arp_data.arp_sha);
	arp_h->arp_data.arp_sip = client_info->app_ip;
	ether_addr_copy(&client_info->cli_mac, &arp_h->arp_data.arp_tha);
	arp_h->arp_data.arp_tip = client_info->cli_ip;

	arp_h->arp_hrd = rte_cpu_to_be_16(ARP_HRD_ETHER);
	arp_h->arp_pro = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
	arp_h->arp_hln = ETHER_ADDR_LEN;
	arp_h->arp_pln = sizeof(uint32_t);
	arp_h->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);

	slave_idx = client_info->slave_idx;
	rte_spinlock_unlock(&internals->mode6.lock);

	return slave_idx;
}

void
bond_mode_alb_client_list_upd(struct rte_eth_dev *bond_dev)
{
	struct bond_dev_private *internals = bond_dev->data->dev_private;
	struct client_data *client_info;

	int i;

	/* If active slave count is 0, it's pointless to refresh alb table */
	if (internals->active_slave_count <= 0)
		return;

	rte_spinlock_lock(&internals->mode6.lock);
	internals->mode6.last_slave = ALB_NULL_INDEX;

	for (i = 0; i < ALB_HASH_TABLE_SIZE; i++) {
		client_info = &internals->mode6.client_table[i];
		if (client_info->in_use) {
			client_info->slave_idx = calculate_slave(internals);
			rte_eth_macaddr_get(client_info->slave_idx, &client_info->app_mac);
			internals->mode6.ntt = 1;
		}
	}
	rte_spinlock_unlock(&internals->mode6.lock);
}
