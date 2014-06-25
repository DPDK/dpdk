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

#ifndef _RTE_ETH_BOND_PRIVATE_H_
#define _RTE_ETH_BOND_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_ethdev.h>

#include "rte_eth_bond.h"

#define PMD_BOND_SLAVE_PORT_KVARG		("slave")
#define PMD_BOND_PRIMARY_SLAVE_KVARG	("primary")
#define PMD_BOND_MODE_KVARG				("mode")
#define PMD_BOND_XMIT_POLICY_KVARG		("xmit_policy")
#define PMD_BOND_SOCKET_ID_KVARG		("socket_id")
#define PMD_BOND_MAC_ADDR_KVARG			("mac")

#define PMD_BOND_XMIT_POLICY_LAYER2_KVARG	("l2")
#define PMD_BOND_XMIT_POLICY_LAYER23_KVARG	("l23")
#define PMD_BOND_XMIT_POLICY_LAYER34_KVARG	("l34")

extern const char *pmd_bond_init_valid_arguments[];

extern const char *driver_name;

/** Port Queue Mapping Structure */
struct bond_rx_queue {
	int queue_id;
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
	int queue_id;
	/**< Queue Id */
	struct bond_dev_private *dev_private;
	/**< Reference to dev private structure */
	uint16_t nb_tx_desc;
	/**< Number of TX descriptors available for the queue */
	struct rte_eth_txconf tx_conf;
	/**< Copy of TX configuration structure for queue */
};

/** Persisted Slave Configuration Structure */
struct slave_conf {
	uint8_t port_id;
	/**< Port Id of slave eth_dev */
	struct ether_addr mac_addr;
	/**< Slave eth_dev original MAC address */
};
/** Bonded slave devices structure */
struct bond_ethdev_slave_ports {
	uint8_t slaves[RTE_MAX_ETHPORTS];	/**< Slave port id array */
	uint8_t slave_count;				/**< Number of slaves */
};

/** Link Bonding PMD device private configuration Structure */
struct bond_dev_private {
	uint8_t mode;						/**< Link Bonding Mode */

	uint8_t primary_port;				/**< Primary Slave Port */
	uint8_t current_primary_port;		/**< Primary Slave Port */
	uint8_t user_defined_primary_port;
	/**< Flag for whether primary port is user defined or not */
	uint8_t balance_xmit_policy;
	/**< Transmit policy - l2 / l23 / l34 for operation in balance mode */
	uint8_t user_defined_mac;
	/**< Flag for whether MAC address is user defined or not */
	uint8_t promiscuous_en;
	/**< Enabled/disable promiscuous mode on slave devices */
	uint8_t link_props_set;
	/**< Bonded eth_dev link properties set */

	uint16_t nb_rx_queues;			/**< Total number of rx queues */
	uint16_t nb_tx_queues;			/**< Total number of tx queues*/

	uint8_t slave_count;			/**< Number of active slaves */
	uint8_t active_slave_count;		/**< Number of slaves */

	uint8_t active_slaves[RTE_MAX_ETHPORTS];	/**< Active slave list */
	uint8_t slaves[RTE_MAX_ETHPORTS];			/**< Slave list */

	/** Persisted configuration of slaves */
	struct slave_conf presisted_slaves_conf[RTE_MAX_ETHPORTS];
};

extern struct eth_dev_ops default_dev_ops;

int
valid_bonded_ethdev(struct rte_eth_dev *eth_dev);

int
valid_port_id(uint8_t port_id);

int
valid_bonded_port_id(uint8_t port_id);

int
valid_slave_port_id(uint8_t port_id);

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
mac_address_slaves_update(struct rte_eth_dev *bonded_eth_dev);

uint8_t
number_of_sockets(void);

int
bond_ethdev_mode_set(struct rte_eth_dev *eth_dev, int mode);

int
slave_configure(struct rte_eth_dev *bonded_eth_dev,
		struct rte_eth_dev *slave_eth_dev);

void
slave_config_clear(struct bond_dev_private *internals,
		struct rte_eth_dev *slave_eth_dev);

void
slave_config_store(struct bond_dev_private *internals,
		struct rte_eth_dev *slave_eth_dev);

struct slave_conf *
slave_config_get(struct bond_dev_private *internals, uint8_t slave_port_id);

void
bond_ethdev_primary_set(struct bond_dev_private *internals,
		uint8_t slave_port_id);

void
bond_ethdev_lsc_event_callback(uint8_t port_id, enum rte_eth_event_type type,
		void *param);

int
bond_ethdev_parse_slave_port_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args);

int
bond_ethdev_parse_slave_mode_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args);

int
bond_ethdev_parse_socket_id_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args);

int
bond_ethdev_parse_primary_slave_port_id_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args);

int
bond_ethdev_parse_balance_xmit_policy_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args);

int
bond_ethdev_parse_bond_mac_addr_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args);

#ifdef __cplusplus
}
#endif

#endif
