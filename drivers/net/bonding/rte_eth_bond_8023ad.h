/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef RTE_ETH_BOND_8023AD_H_
#define RTE_ETH_BOND_8023AD_H_

#include <rte_ether.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Actor/partner states
 */
#define STATE_LACP_ACTIVE                   0x01
#define STATE_LACP_SHORT_TIMEOUT            0x02
#define STATE_AGGREGATION                   0x04
#define STATE_SYNCHRONIZATION               0x08
#define STATE_COLLECTING                    0x10
#define STATE_DISTRIBUTING                  0x20
/** Partners parameters are defaulted */
#define STATE_DEFAULTED                     0x40
#define STATE_EXPIRED                       0x80

#define TLV_TYPE_ACTOR_INFORMATION          0x01
#define TLV_TYPE_PARTNER_INFORMATION        0x02
#define TLV_TYPE_COLLECTOR_INFORMATION      0x03
#define TLV_TYPE_TERMINATOR_INFORMATION     0x00

#define SLOW_SUBTYPE_LACP                   0x01
#define SLOW_SUBTYPE_MARKER                 0x02

#define MARKER_TLV_TYPE_INFO                0x01
#define MARKER_TLV_TYPE_RESP                0x02

typedef void (*rte_eth_bond_8023ad_ext_slowrx_fn)(uint16_t slave_id,
						  struct rte_mbuf *lacp_pkt);

enum rte_bond_8023ad_selection {
	UNSELECTED,
	STANDBY,
	SELECTED
};

enum rte_bond_8023ad_agg_selection {
	AGG_BANDWIDTH,
	AGG_COUNT,
	AGG_STABLE
};

/** Generic slow protocol structure */
struct slow_protocol {
	uint8_t subtype;
	uint8_t reserved_119[119];
} __rte_packed;

/** Generic slow protocol frame type structure */
struct slow_protocol_frame {
	struct rte_ether_hdr eth_hdr;
	struct slow_protocol slow_protocol;
} __rte_packed __rte_aligned(2);

struct port_params {
	uint16_t system_priority;
	/**< System priority (unused in current implementation) */
	struct rte_ether_addr system;
	/**< System ID - Slave MAC address, same as bonding MAC address */
	uint16_t key;
	/**< Speed information (implementation dependednt) and duplex. */
	uint16_t port_priority;
	/**< Priority of this (unused in current implementation) */
	uint16_t port_number;
	/**< Port number. It corresponds to slave port id. */
} __rte_packed __rte_aligned(2);

struct lacpdu_actor_partner_params {
	uint8_t tlv_type_info;
	uint8_t info_length;
	struct port_params port_params;
	uint8_t state;
	uint8_t reserved_3[3];
} __rte_packed __rte_aligned(2);

/** LACPDU structure (5.4.2 in 802.1AX documentation). */
struct lacpdu {
	uint8_t subtype;
	uint8_t version_number;

	struct lacpdu_actor_partner_params actor;
	struct lacpdu_actor_partner_params partner;

	uint8_t tlv_type_collector_info;
	uint8_t collector_info_length;
	uint16_t collector_max_delay;
	uint8_t reserved_12[12];

	uint8_t tlv_type_terminator;
	uint8_t terminator_length;
	uint8_t reserved_50[50];
} __rte_packed __rte_aligned(2);

/** LACPDU frame: Contains ethernet header and LACPDU. */
struct lacpdu_header {
	struct rte_ether_hdr eth_hdr;
	struct lacpdu lacpdu;
} __rte_packed __rte_aligned(2);

struct marker {
	uint8_t subtype;
	uint8_t version_number;

	uint8_t tlv_type_marker;
	uint8_t info_length;
	uint16_t requester_port;
	struct rte_ether_addr requester_system;
	uint32_t requester_transaction_id;
	uint8_t reserved_2[2];

	uint8_t tlv_type_terminator;
	uint8_t terminator_length;
	uint8_t reserved_90[90];
} __rte_packed __rte_aligned(2);

struct marker_header {
	struct rte_ether_hdr eth_hdr;
	struct marker marker;
} __rte_packed __rte_aligned(2);

struct rte_eth_bond_8023ad_conf {
	uint32_t fast_periodic_ms;
	uint32_t slow_periodic_ms;
	uint32_t short_timeout_ms;
	uint32_t long_timeout_ms;
	uint32_t aggregate_wait_timeout_ms;
	uint32_t tx_period_ms;
	uint32_t rx_marker_period_ms;
	uint32_t update_timeout_ms;
	rte_eth_bond_8023ad_ext_slowrx_fn slowrx_cb;
	enum rte_bond_8023ad_agg_selection agg_selection;
};

struct rte_eth_bond_8023ad_slave_info {
	enum rte_bond_8023ad_selection selected;
	uint8_t actor_state;
	struct port_params actor;
	uint8_t partner_state;
	struct port_params partner;
	uint16_t agg_port_id;
};

/**
 * @internal
 *
 * Function returns current configuration of 802.3AX mode.
 *
 * @param port_id   Bonding device id
 * @param conf		Pointer to timeout structure.
 *
 * @return
 *   0 - if ok
 *   -EINVAL if conf is NULL
 */
int
rte_eth_bond_8023ad_conf_get(uint16_t port_id,
		struct rte_eth_bond_8023ad_conf *conf);

/**
 * @internal
 *
 * Function set new configuration of 802.3AX mode.
 *
 * @param port_id   Bonding device id
 * @param conf		Configuration, if NULL set default configuration.
 * @return
 *   0 - if ok
 *   -EINVAL if configuration is invalid.
 */
int
rte_eth_bond_8023ad_setup(uint16_t port_id,
		struct rte_eth_bond_8023ad_conf *conf);

/**
 * @internal
 *
 * Function returns current state of given slave device.
 *
 * @param slave_id  Port id of valid slave.
 * @param conf		buffer for configuration
 * @return
 *   0 - if ok
 *   -EINVAL if conf is NULL or slave id is invalid (not a slave of given
 *       bonded device or is not inactive).
 */
int
rte_eth_bond_8023ad_slave_info(uint16_t port_id, uint16_t slave_id,
		struct rte_eth_bond_8023ad_slave_info *conf);

#ifdef __cplusplus
}
#endif

/**
 * Configure a slave port to start collecting.
 *
 * @param port_id	Bonding device id
 * @param slave_id	Port id of valid slave.
 * @param enabled	Non-zero when collection enabled.
 * @return
 *   0 - if ok
 *   -EINVAL if slave is not valid.
 */
int
rte_eth_bond_8023ad_ext_collect(uint16_t port_id, uint16_t slave_id,
				int enabled);

/**
 * Get COLLECTING flag from slave port actor state.
 *
 * @param port_id	Bonding device id
 * @param slave_id	Port id of valid slave.
 * @return
 *   0 - if not set
 *   1 - if set
 *   -EINVAL if slave is not valid.
 */
int
rte_eth_bond_8023ad_ext_collect_get(uint16_t port_id, uint16_t slave_id);

/**
 * Configure a slave port to start distributing.
 *
 * @param port_id	Bonding device id
 * @param slave_id	Port id of valid slave.
 * @param enabled	Non-zero when distribution enabled.
 * @return
 *   0 - if ok
 *   -EINVAL if slave is not valid.
 */
int
rte_eth_bond_8023ad_ext_distrib(uint16_t port_id, uint16_t slave_id,
				int enabled);

/**
 * Get DISTRIBUTING flag from slave port actor state.
 *
 * @param port_id	Bonding device id
 * @param slave_id	Port id of valid slave.
 * @return
 *   0 - if not set
 *   1 - if set
 *   -EINVAL if slave is not valid.
 */
int
rte_eth_bond_8023ad_ext_distrib_get(uint16_t port_id, uint16_t slave_id);

/**
 * LACPDU transmit path for external 802.3ad state machine.  Caller retains
 * ownership of the packet on failure.
 *
 * @param port_id	Bonding device id
 * @param slave_id	Port ID of valid slave device.
 * @param lacp_pkt	mbuf containing LACPDU.
 *
 * @return
 *   0 on success, negative value otherwise.
 */
int
rte_eth_bond_8023ad_ext_slowtx(uint16_t port_id, uint16_t slave_id,
		struct rte_mbuf *lacp_pkt);

/**
 * Enable dedicated hw queues for 802.3ad control plane traffic on on slaves
 *
 * This function creates an additional tx and rx queue on each slave for
 * dedicated 802.3ad control plane traffic . A flow filtering rule is
 * programmed on each slave to redirect all LACP slow packets to that rx queue
 * for processing in the LACP state machine, this removes the need to filter
 * these packets in the bonded devices data path. The additional tx queue is
 * used to enable the LACP state machine to enqueue LACP packets directly to
 * slave hw independently of the bonded devices data path.
 *
 * To use this feature all slaves must support the programming of the flow
 * filter rule required for rx and have enough queues that one rx and tx queue
 * can be reserved for the LACP state machines control packets.
 *
 * Bonding port must be stopped to change this configuration.
 *
 * @param port_id      Bonding device id
 *
 * @return
 *   0 on success, negative value otherwise.
 */
int
rte_eth_bond_8023ad_dedicated_queues_enable(uint16_t port_id);

/**
 * Disable slow queue on slaves
 *
 * This function disables hardware slow packet filter.
 *
 * Bonding port must be stopped to change this configuration.
 *
 * @see rte_eth_bond_8023ad_slow_pkt_hw_filter_enable
 *
 * @param port_id      Bonding device id
 * @return
 *   0 on success, negative value otherwise.
 *
 */
int
rte_eth_bond_8023ad_dedicated_queues_disable(uint16_t port_id);

/*
 * Get aggregator mode for 8023ad
 * @param port_id Bonding device id
 *
 * @return
 *   agregator mode on success, negative value otherwise
 */
int
rte_eth_bond_8023ad_agg_selection_get(uint16_t port_id);

/**
 * Set aggregator mode for 8023ad
 * @param port_id Bonding device id
 * @return
 *   0 on success, negative value otherwise
 */
int
rte_eth_bond_8023ad_agg_selection_set(uint16_t port_id,
		enum rte_bond_8023ad_agg_selection agg_selection);
#endif /* RTE_ETH_BOND_8023AD_H_ */
