/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
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

#ifndef _RTE_ETH_BOND_PRIVATE_H_
#define _RTE_ETH_BOND_PRIVATE_H_

#include <rte_ethdev.h>
#include <rte_spinlock.h>
#include <rte_bitmap.h>

#include "rte_eth_bond.h"
#include "rte_eth_bond_8023ad_private.h"
#include "rte_eth_bond_alb.h"

#define PMD_BOND_SLAVE_PORT_KVARG			("slave")
#define PMD_BOND_PRIMARY_SLAVE_KVARG		("primary")
#define PMD_BOND_MODE_KVARG					("mode")
#define PMD_BOND_XMIT_POLICY_KVARG			("xmit_policy")
#define PMD_BOND_SOCKET_ID_KVARG			("socket_id")
#define PMD_BOND_MAC_ADDR_KVARG				("mac")
#define PMD_BOND_LSC_POLL_PERIOD_KVARG		("lsc_poll_period_ms")
#define PMD_BOND_LINK_UP_PROP_DELAY_KVARG	("up_delay")
#define PMD_BOND_LINK_DOWN_PROP_DELAY_KVARG	("down_delay")

#define PMD_BOND_XMIT_POLICY_LAYER2_KVARG	("l2")
#define PMD_BOND_XMIT_POLICY_LAYER23_KVARG	("l23")
#define PMD_BOND_XMIT_POLICY_LAYER34_KVARG	("l34")

#define RTE_BOND_LOG(lvl, msg, ...)		\
	RTE_LOG(lvl, PMD, "%s(%d) - " msg "\n", __func__, __LINE__, ##__VA_ARGS__)

#define BONDING_MODE_INVALID 0xFF

extern const char *pmd_bond_init_valid_arguments[];

extern struct rte_vdev_driver pmd_bond_drv;

/** Port Queue Mapping Structure */
struct bond_rx_queue {
	uint16_t queue_id;
	/**< Queue Id */
	struct bond_dev_private *dev_private;
	/**< Reference to eth_dev private structure */
	uint16_t nb_rx_desc;
	/**< Number of RX descriptors available for the queue */
	struct rte_eth_rxconf rx_conf;
	/**< Copy of RX configuration structure for queue */
	struct rte_mempool *mb_pool;
	/**< Reference to mbuf pool to use for RX queue */
};

struct bond_tx_queue {
	uint16_t queue_id;
	/**< Queue Id */
	struct bond_dev_private *dev_private;
	/**< Reference to dev private structure */
	uint16_t nb_tx_desc;
	/**< Number of TX descriptors available for the queue */
	struct rte_eth_txconf tx_conf;
	/**< Copy of TX configuration structure for queue */
};

/** Bonded slave devices structure */
struct bond_ethdev_slave_ports {
	uint8_t slaves[RTE_MAX_ETHPORTS];	/**< Slave port id array */
	uint8_t slave_count;				/**< Number of slaves */
};

struct bond_slave_details {
	uint8_t port_id;

	uint8_t link_status_poll_enabled;
	uint8_t link_status_wait_to_complete;
	uint8_t last_link_status;
	/**< Port Id of slave eth_dev */
	struct ether_addr persisted_mac_addr;

	uint16_t reta_size;
};


typedef uint16_t (*xmit_hash_t)(const struct rte_mbuf *buf, uint8_t slave_count);

/** Link Bonding PMD device private configuration Structure */
struct bond_dev_private {
	uint8_t port_id;					/**< Port Id of Bonded Port */
	uint8_t mode;						/**< Link Bonding Mode */

	rte_spinlock_t lock;

	uint8_t primary_port;				/**< Primary Slave Port */
	uint8_t current_primary_port;		/**< Primary Slave Port */
	uint8_t user_defined_primary_port;
	/**< Flag for whether primary port is user defined or not */

	uint8_t balance_xmit_policy;
	/**< Transmit policy - l2 / l23 / l34 for operation in balance mode */
	xmit_hash_t xmit_hash;
	/**< Transmit policy hash function */

	uint8_t user_defined_mac;
	/**< Flag for whether MAC address is user defined or not */
	uint8_t promiscuous_en;
	/**< Enabled/disable promiscuous mode on bonding device */
	uint8_t link_props_set;
	/**< flag to denote if the link properties are set */

	uint8_t link_status_polling_enabled;
	uint32_t link_status_polling_interval_ms;

	uint32_t link_down_delay_ms;
	uint32_t link_up_delay_ms;

	uint16_t nb_rx_queues;			/**< Total number of rx queues */
	uint16_t nb_tx_queues;			/**< Total number of tx queues*/

	uint8_t active_slave;		/**< Next active_slave to poll */
	uint8_t active_slave_count;		/**< Number of active slaves */
	uint8_t active_slaves[RTE_MAX_ETHPORTS];	/**< Active slave list */

	uint8_t slave_count;			/**< Number of bonded slaves */
	struct bond_slave_details slaves[RTE_MAX_ETHPORTS];
	/**< Arary of bonded slaves details */

	struct mode8023ad_private mode4;
	uint8_t tlb_slaves_order[RTE_MAX_ETHPORTS]; /* TLB active slaves send order */
	struct mode_alb_private mode6;

	uint32_t rx_offload_capa;            /** Rx offload capability */
	uint32_t tx_offload_capa;            /** Tx offload capability */

	/** Bit mask of RSS offloads, the bit offset also means flow type */
	uint64_t flow_type_rss_offloads;

	uint16_t reta_size;
	struct rte_eth_rss_reta_entry64 reta_conf[ETH_RSS_RETA_SIZE_512 /
			RTE_RETA_GROUP_SIZE];

	uint8_t rss_key[52];				/**< 52-byte hash key buffer. */
	uint8_t rss_key_len;				/**< hash key length in bytes. */

	struct rte_kvargs *kvlist;
	uint8_t slave_update_idx;

	uint32_t candidate_max_rx_pktlen;
	uint32_t max_rx_pktlen;

	void *vlan_filter_bmpmem;		/* enabled vlan filter bitmap */
	struct rte_bitmap *vlan_filter_bmp;
};

extern const struct eth_dev_ops default_dev_ops;

int
check_for_bonded_ethdev(const struct rte_eth_dev *eth_dev);

/* Search given slave array to find position of given id.
 * Return slave pos or slaves_count if not found. */
static inline uint8_t
find_slave_by_id(uint8_t *slaves, uint8_t slaves_count, uint8_t slave_id) {

	uint8_t pos;
	for (pos = 0; pos < slaves_count; pos++) {
		if (slave_id == slaves[pos])
			break;
	}

	return pos;
}

int
valid_port_id(uint8_t port_id);

int
valid_bonded_port_id(uint8_t port_id);

int
valid_slave_port_id(uint8_t port_id);

void
deactivate_slave(struct rte_eth_dev *eth_dev, uint8_t port_id);

void
activate_slave(struct rte_eth_dev *eth_dev, uint8_t port_id);

void
link_properties_set(struct rte_eth_dev *bonded_eth_dev,
		struct rte_eth_link *slave_dev_link);
void
link_properties_reset(struct rte_eth_dev *bonded_eth_dev);

int
link_properties_valid(struct rte_eth_link *bonded_dev_link,
		struct rte_eth_link *slave_dev_link);

int
mac_address_set(struct rte_eth_dev *eth_dev, struct ether_addr *new_mac_addr);

int
mac_address_get(struct rte_eth_dev *eth_dev, struct ether_addr *dst_mac_addr);

int
mac_address_slaves_update(struct rte_eth_dev *bonded_eth_dev);

int
bond_ethdev_mode_set(struct rte_eth_dev *eth_dev, int mode);

int
slave_configure(struct rte_eth_dev *bonded_eth_dev,
		struct rte_eth_dev *slave_eth_dev);

void
slave_remove(struct bond_dev_private *internals,
		struct rte_eth_dev *slave_eth_dev);

void
slave_add(struct bond_dev_private *internals,
		struct rte_eth_dev *slave_eth_dev);

uint16_t
xmit_l2_hash(const struct rte_mbuf *buf, uint8_t slave_count);

uint16_t
xmit_l23_hash(const struct rte_mbuf *buf, uint8_t slave_count);

uint16_t
xmit_l34_hash(const struct rte_mbuf *buf, uint8_t slave_count);

void
bond_ethdev_primary_set(struct bond_dev_private *internals,
		uint8_t slave_port_id);

void
bond_ethdev_lsc_event_callback(uint8_t port_id, enum rte_eth_event_type type,
		void *param);

int
bond_ethdev_parse_slave_port_kvarg(const char *key,
		const char *value, void *extra_args);

int
bond_ethdev_parse_slave_mode_kvarg(const char *key,
		const char *value, void *extra_args);

int
bond_ethdev_parse_socket_id_kvarg(const char *key,
		const char *value, void *extra_args);

int
bond_ethdev_parse_primary_slave_port_id_kvarg(const char *key,
		const char *value, void *extra_args);

int
bond_ethdev_parse_balance_xmit_policy_kvarg(const char *key,
		const char *value, void *extra_args);

int
bond_ethdev_parse_bond_mac_addr_kvarg(const char *key,
		const char *value, void *extra_args);

int
bond_ethdev_parse_time_ms_kvarg(const char *key,
		const char *value, void *extra_args);

void
bond_tlb_disable(struct bond_dev_private *internals);

void
bond_tlb_enable(struct bond_dev_private *internals);

void
bond_tlb_activate_slave(struct bond_dev_private *internals);

void
bond_ethdev_stop(struct rte_eth_dev *eth_dev);

void
bond_ethdev_close(struct rte_eth_dev *dev);

#endif
