/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _ETH_BOND_PRIVATE_H_
#define _ETH_BOND_PRIVATE_H_

#include <stdint.h>
#include <sys/queue.h>

#include <ethdev_driver.h>
#include <rte_flow.h>
#include <rte_spinlock.h>
#include <rte_bitmap.h>
#include <rte_flow_driver.h>

#include "rte_eth_bond.h"
#include "eth_bond_8023ad_private.h"
#include "rte_eth_bond_alb.h"

#define PMD_BOND_MEMBER_PORT_KVARG			("member")
#define PMD_BOND_PRIMARY_MEMBER_KVARG		("primary")
#define PMD_BOND_MODE_KVARG					("mode")
#define PMD_BOND_AGG_MODE_KVARG				("agg_mode")
#define PMD_BOND_XMIT_POLICY_KVARG			("xmit_policy")
#define PMD_BOND_SOCKET_ID_KVARG			("socket_id")
#define PMD_BOND_MAC_ADDR_KVARG				("mac")
#define PMD_BOND_LSC_POLL_PERIOD_KVARG		("lsc_poll_period_ms")
#define PMD_BOND_LINK_UP_PROP_DELAY_KVARG	("up_delay")
#define PMD_BOND_LINK_DOWN_PROP_DELAY_KVARG	("down_delay")

#define PMD_BOND_XMIT_POLICY_LAYER2_KVARG	("l2")
#define PMD_BOND_XMIT_POLICY_LAYER23_KVARG	("l23")
#define PMD_BOND_XMIT_POLICY_LAYER34_KVARG	("l34")

extern int bond_logtype;

#define RTE_BOND_LOG(lvl, msg, ...)		\
	rte_log(RTE_LOG_ ## lvl, bond_logtype, \
		"%s(%d) - " msg "\n", __func__, __LINE__, ##__VA_ARGS__)

#define BONDING_MODE_INVALID 0xFF

extern const char *pmd_bond_init_valid_arguments[];

extern struct rte_vdev_driver pmd_bond_drv;

extern const struct rte_flow_ops bond_flow_ops;

/** Port Queue Mapping Structure */
struct bond_rx_queue {
	uint16_t queue_id;
	/**< Next active_member to poll */
	uint16_t active_member;
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

/** Bonding member devices structure */
struct bond_ethdev_member_ports {
	uint16_t members[RTE_MAX_ETHPORTS];	/**< Member port id array */
	uint16_t member_count;				/**< Number of members */
};

struct bond_member_details {
	uint16_t port_id;

	uint8_t link_status_poll_enabled;
	uint8_t link_status_wait_to_complete;
	uint8_t last_link_status;
	/**< Port Id of member eth_dev */
	struct rte_ether_addr persisted_mac_addr;

	uint16_t reta_size;
};

struct rte_flow {
	TAILQ_ENTRY(rte_flow) next;
	/* Members flows */
	struct rte_flow *flows[RTE_MAX_ETHPORTS];
	/* Flow description for synchronization */
	struct rte_flow_conv_rule rule;
	uint8_t rule_data[];
};

typedef void (*burst_xmit_hash_t)(struct rte_mbuf **buf, uint16_t nb_pkts,
		uint16_t member_count, uint16_t *members);

/** Link Bonding PMD device private configuration Structure */
struct bond_dev_private {
	uint16_t port_id;			/**< Port Id of Bonding Port */
	uint8_t mode;						/**< Link Bonding Mode */

	rte_spinlock_t lock;
	rte_spinlock_t lsc_lock;

	uint16_t primary_port;			/**< Primary Member Port */
	uint16_t current_primary_port;		/**< Primary Member Port */
	uint16_t user_defined_primary_port;
	/**< Flag for whether primary port is user defined or not */

	uint8_t balance_xmit_policy;
	/**< Transmit policy - l2 / l23 / l34 for operation in balance mode */
	burst_xmit_hash_t burst_xmit_hash;
	/**< Transmit policy hash function */

	uint8_t user_defined_mac;
	/**< Flag for whether MAC address is user defined or not */

	uint8_t link_status_polling_enabled;
	uint32_t link_status_polling_interval_ms;

	uint32_t link_down_delay_ms;
	uint32_t link_up_delay_ms;

	uint32_t speed_capa;
	/**< Supported speeds bitmap (RTE_ETH_LINK_SPEED_). */

	uint16_t nb_rx_queues;			/**< Total number of rx queues */
	uint16_t nb_tx_queues;			/**< Total number of tx queues*/

	uint16_t active_member_count;		/**< Number of active members */
	uint16_t active_members[RTE_MAX_ETHPORTS];    /**< Active member list */

	uint16_t member_count;			/**< Number of bonding members */
	struct bond_member_details members[RTE_MAX_ETHPORTS];
	/**< Array of bonding members details */

	struct mode8023ad_private mode4;
	uint16_t tlb_members_order[RTE_MAX_ETHPORTS];
	/**< TLB active members send order */
	struct mode_alb_private mode6;

	uint64_t rx_offload_capa;       /** Rx offload capability */
	uint64_t tx_offload_capa;       /** Tx offload capability */
	uint64_t rx_queue_offload_capa; /** per queue Rx offload capability */
	uint64_t tx_queue_offload_capa; /** per queue Tx offload capability */

	/**< List of the configured flows */
	TAILQ_HEAD(sub_flows, rte_flow) flow_list;

	/**< Flow isolation state */
	int flow_isolated;
	int flow_isolated_valid;

	/** Bit mask of RSS offloads, the bit offset also means flow type */
	uint64_t flow_type_rss_offloads;

	struct rte_eth_rxconf default_rxconf;	/**< Default RxQ conf. */
	struct rte_eth_txconf default_txconf;	/**< Default TxQ conf. */
	struct rte_eth_desc_lim rx_desc_lim;	/**< Rx descriptor limits */
	struct rte_eth_desc_lim tx_desc_lim;	/**< Tx descriptor limits */

	uint16_t reta_size;
	struct rte_eth_rss_reta_entry64 reta_conf[RTE_ETH_RSS_RETA_SIZE_512 /
			RTE_ETH_RETA_GROUP_SIZE];

	uint8_t rss_key[52];				/**< 52-byte hash key buffer. */
	uint8_t rss_key_len;				/**< hash key length in bytes. */

	struct rte_kvargs *kvlist;
	uint8_t member_update_idx;

	bool kvargs_processing_is_done;

	uint32_t candidate_max_rx_pktlen;
	uint32_t max_rx_pktlen;

	void *vlan_filter_bmpmem;		/* enabled vlan filter bitmap */
	struct rte_bitmap *vlan_filter_bmp;
};

extern const struct eth_dev_ops default_dev_ops;

int
check_for_main_bonding_ethdev(const struct rte_eth_dev *eth_dev);

int
check_for_bonding_ethdev(const struct rte_eth_dev *eth_dev);

/*
 * Search given member array to find position of given id.
 * Return member pos or members_count if not found.
 */
static inline uint16_t
find_member_by_id(uint16_t *members, uint16_t members_count, uint16_t member_id) {

	uint16_t pos;
	for (pos = 0; pos < members_count; pos++) {
		if (member_id == members[pos])
			break;
	}

	return pos;
}

int
valid_port_id(uint16_t port_id);

int
valid_bonding_port_id(uint16_t port_id);

int
valid_member_port_id(struct bond_dev_private *internals, uint16_t port_id);

void
deactivate_member(struct rte_eth_dev *eth_dev, uint16_t port_id);

void
activate_member(struct rte_eth_dev *eth_dev, uint16_t port_id);

int
mac_address_set(struct rte_eth_dev *eth_dev,
		struct rte_ether_addr *new_mac_addr);

int
mac_address_get(struct rte_eth_dev *eth_dev,
		struct rte_ether_addr *dst_mac_addr);

int
mac_address_members_update(struct rte_eth_dev *bonding_eth_dev);

int
member_add_mac_addresses(struct rte_eth_dev *bonding_eth_dev,
		uint16_t member_port_id);

int
member_remove_mac_addresses(struct rte_eth_dev *bonding_eth_dev,
		uint16_t member_port_id);

int
bond_ethdev_mode_set(struct rte_eth_dev *eth_dev, uint8_t mode);

int
member_configure(struct rte_eth_dev *bonding_eth_dev,
		struct rte_eth_dev *member_eth_dev);

int
member_start(struct rte_eth_dev *bonding_eth_dev,
		struct rte_eth_dev *member_eth_dev);

void
member_remove(struct bond_dev_private *internals,
		struct rte_eth_dev *member_eth_dev);

void
member_add(struct bond_dev_private *internals,
		struct rte_eth_dev *member_eth_dev);

void
burst_xmit_l2_hash(struct rte_mbuf **buf, uint16_t nb_pkts,
		uint16_t member_count, uint16_t *members);

void
burst_xmit_l23_hash(struct rte_mbuf **buf, uint16_t nb_pkts,
		uint16_t member_count, uint16_t *members);

void
burst_xmit_l34_hash(struct rte_mbuf **buf, uint16_t nb_pkts,
		uint16_t member_count, uint16_t *members);


void
bond_ethdev_primary_set(struct bond_dev_private *internals,
		uint16_t member_port_id);

int
bond_ethdev_lsc_event_callback(uint16_t port_id, enum rte_eth_event_type type,
		void *param, void *ret_param);

int
bond_ethdev_parse_member_port_kvarg(const char *key,
		const char *value, void *extra_args);

int
bond_ethdev_parse_member_mode_kvarg(const char *key,
		const char *value, void *extra_args);

int
bond_ethdev_parse_member_agg_mode_kvarg(const char *key __rte_unused,
		const char *value, void *extra_args);

int
bond_ethdev_parse_socket_id_kvarg(const char *key,
		const char *value, void *extra_args);

int
bond_ethdev_parse_primary_member_port_id_kvarg(const char *key,
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
bond_tlb_activate_member(struct bond_dev_private *internals);

int
bond_ethdev_stop(struct rte_eth_dev *eth_dev);

int
bond_ethdev_close(struct rte_eth_dev *dev);

#endif
