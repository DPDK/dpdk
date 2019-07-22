/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef __INCLUDE_RTE_TABLE_ACTION_H__
#define __INCLUDE_RTE_TABLE_ACTION_H__

/**
 * @file
 * RTE Pipeline Table Actions
 *
 * This API provides a common set of actions for pipeline tables to speed up
 * application development.
 *
 * Each match-action rule added to a pipeline table has associated data that
 * stores the action context. This data is input to the table action handler
 * called for every input packet that hits the rule as part of the table lookup
 * during the pipeline execution. The pipeline library allows the user to define
 * his own table actions by providing customized table action handlers (table
 * lookup) and complete freedom of setting the rules and their data (table rule
 * add/delete). While the user can still follow this process, this API is
 * intended to provide a quicker development alternative for a set of predefined
 * actions.
 *
 * The typical steps to use this API are:
 *  - Define a table action profile. This is a configuration template that can
 *    potentially be shared by multiple tables from the same or different
 *    pipelines, with different tables from the same pipeline likely to use
 *    different action profiles. For every table using a given action profile,
 *    the profile defines the set of actions and the action configuration to be
 *    implemented for all the table rules. API functions:
 *    rte_table_action_profile_create(),
 *    rte_table_action_profile_action_register(),
 *    rte_table_action_profile_freeze().
 *
 *  - Instantiate the table action profile to create table action objects. Each
 *    pipeline table has its own table action object. API functions:
 *    rte_table_action_create().
 *
 *  - Use the table action object to generate the pipeline table action handlers
 *    (invoked by the pipeline table lookup operation). API functions:
 *    rte_table_action_table_params_get().
 *
 *  - Use the table action object to generate the rule data (for the pipeline
 *    table rule add operation) based on given action parameters. API functions:
 *    rte_table_action_apply().
 *
 *  - Use the table action object to read action data (e.g. stats counters) for
 *    any given rule. API functions: rte_table_action_XYZ_read().
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_compat.h>
#include <rte_ether.h>
#include <rte_meter.h>
#include <rte_table_hash.h>

#include "rte_pipeline.h"

/** Table actions. */
enum rte_table_action_type {
	/** Forward to next pipeline table, output port or drop. */
	RTE_TABLE_ACTION_FWD = 0,

	/**  Load balance. */
	RTE_TABLE_ACTION_LB,

	/**  Traffic Metering and Policing. */
	RTE_TABLE_ACTION_MTR,

	/**  Traffic Management. */
	RTE_TABLE_ACTION_TM,

	/** Packet encapsulations. */
	RTE_TABLE_ACTION_ENCAP,

	/** Network Address Translation (NAT). */
	RTE_TABLE_ACTION_NAT,

	/** Time to Live (TTL) update. */
	RTE_TABLE_ACTION_TTL,

	/** Statistics. */
	RTE_TABLE_ACTION_STATS,

	/** Timestamp. */
	RTE_TABLE_ACTION_TIME,

	/** Crypto. */
	RTE_TABLE_ACTION_SYM_CRYPTO,

	/** Tag. */
	RTE_TABLE_ACTION_TAG,

	/** Packet decapsulations. */
	RTE_TABLE_ACTION_DECAP,
};

/** Common action configuration (per table action profile). */
struct rte_table_action_common_config {
	/** Input packet Internet Protocol (IP) version. Non-zero for IPv4, zero
	 * for IPv6.
	 */
	int ip_version;

	/** IP header offset within the input packet buffer. Offset 0 points to
	 * the first byte of the MBUF structure.
	 */
	uint32_t ip_offset;
};

/**
 * RTE_TABLE_ACTION_FWD
 */
/** Forward action parameters (per table rule). */
struct rte_table_action_fwd_params {
	/** Forward action. */
	enum rte_pipeline_action action;

	/** Pipeline table ID or output port ID. */
	uint32_t id;
};

/**
 * RTE_TABLE_ACTION_LB
 */
/** Load balance key size min (number of bytes). */
#define RTE_TABLE_ACTION_LB_KEY_SIZE_MIN                    8

/** Load balance key size max (number of bytes). */
#define RTE_TABLE_ACTION_LB_KEY_SIZE_MAX                    64

/** Load balance table size. */
#define RTE_TABLE_ACTION_LB_TABLE_SIZE                      8

/** Load balance action configuration (per table action profile). */
struct rte_table_action_lb_config {
	/** Key size (number of bytes). */
	uint32_t key_size;

	/** Key offset within the input packet buffer. Offset 0 points to the
	 * first byte of the MBUF structure.
	 */
	uint32_t key_offset;

	/** Key mask (*key_size* bytes are valid). */
	uint8_t key_mask[RTE_TABLE_ACTION_LB_KEY_SIZE_MAX];

	/** Hash function. */
	rte_table_hash_op_hash f_hash;

	/** Seed value for *f_hash*. */
	uint64_t seed;

	/** Output value offset within the input packet buffer. Offset 0 points
	 * to the first byte of the MBUF structure.
	 */
	uint32_t out_offset;
};

/** Load balance action parameters (per table rule). */
struct rte_table_action_lb_params {
	/** Table defining the output values and their weights. The weights are
	 * set in 1/RTE_TABLE_ACTION_LB_TABLE_SIZE increments. To assign a
	 * weight of N/RTE_TABLE_ACTION_LB_TABLE_SIZE to a given output value
	 * (0 <= N <= RTE_TABLE_ACTION_LB_TABLE_SIZE), the same output value
	 * needs to show up exactly N times in this table.
	 */
	uint32_t out[RTE_TABLE_ACTION_LB_TABLE_SIZE];
};

/**
 * RTE_TABLE_ACTION_MTR
 */
/** Max number of traffic classes (TCs). */
#define RTE_TABLE_ACTION_TC_MAX                                  16

/** Max number of queues per traffic class. */
#define RTE_TABLE_ACTION_TC_QUEUE_MAX                            16

/** Differentiated Services Code Point (DSCP) translation table entry. */
struct rte_table_action_dscp_table_entry {
	/** Traffic class. Used by the meter or the traffic management actions.
	 * Has to be strictly smaller than *RTE_TABLE_ACTION_TC_MAX*. Traffic
	 * class 0 is the highest priority.
	 */
	uint32_t tc_id;

	/** Traffic class queue. Used by the traffic management action. Has to
	 * be strictly smaller than *RTE_TABLE_ACTION_TC_QUEUE_MAX*.
	 */
	uint32_t tc_queue_id;

	/** Packet color. Used by the meter action as the packet input color
	 * for the color aware mode of the traffic metering algorithm.
	 */
	enum rte_color color;
};

/** DSCP translation table. */
struct rte_table_action_dscp_table {
	/** Array of DSCP table entries */
	struct rte_table_action_dscp_table_entry entry[64];
};

/** Supported traffic metering algorithms. */
enum rte_table_action_meter_algorithm {
	/** Single Rate Three Color Marker (srTCM) - IETF RFC 2697. */
	RTE_TABLE_ACTION_METER_SRTCM,

	/** Two Rate Three Color Marker (trTCM) - IETF RFC 2698. */
	RTE_TABLE_ACTION_METER_TRTCM,
};

/** Traffic metering profile (configuration template). */
struct rte_table_action_meter_profile {
	/** Traffic metering algorithm. */
	enum rte_table_action_meter_algorithm alg;

	RTE_STD_C11
	union {
		/** Only valid when *alg* is set to srTCM - IETF RFC 2697. */
		struct rte_meter_srtcm_params srtcm;

		/** Only valid when *alg* is set to trTCM - IETF RFC 2698. */
		struct rte_meter_trtcm_params trtcm;
	};
};

/** Policer actions. */
enum rte_table_action_policer {
	/** Recolor the packet as green. */
	RTE_TABLE_ACTION_POLICER_COLOR_GREEN = 0,

	/** Recolor the packet as yellow. */
	RTE_TABLE_ACTION_POLICER_COLOR_YELLOW,

	/** Recolor the packet as red. */
	RTE_TABLE_ACTION_POLICER_COLOR_RED,

	/** Drop the packet. */
	RTE_TABLE_ACTION_POLICER_DROP,

	/** Number of policer actions. */
	RTE_TABLE_ACTION_POLICER_MAX
};

/** Meter action configuration per traffic class. */
struct rte_table_action_mtr_tc_params {
	/** Meter profile ID. */
	uint32_t meter_profile_id;

	/** Policer actions. */
	enum rte_table_action_policer policer[RTE_COLORS];
};

/** Meter action statistics counters per traffic class. */
struct rte_table_action_mtr_counters_tc {
	/** Number of packets per color at the output of the traffic metering
	 * and before the policer actions are executed. Only valid when
	 * *n_packets_valid* is non-zero.
	 */
	uint64_t n_packets[RTE_COLORS];

	/** Number of packet bytes per color at the output of the traffic
	 * metering and before the policer actions are executed. Only valid when
	 * *n_bytes_valid* is non-zero.
	 */
	uint64_t n_bytes[RTE_COLORS];

	/** When non-zero, the *n_packets* field is valid. */
	int n_packets_valid;

	/** When non-zero, the *n_bytes* field is valid. */
	int n_bytes_valid;
};

/** Meter action configuration (per table action profile). */
struct rte_table_action_mtr_config {
	/** Meter algorithm. */
	enum rte_table_action_meter_algorithm alg;

	/** Number of traffic classes. Each traffic class has its own traffic
	 * meter and policer instances. Needs to be equal to either 1 or to
	 * *RTE_TABLE_ACTION_TC_MAX*.
	 */
	uint32_t n_tc;

	/** When non-zero, the *n_packets* meter stats counter is enabled,
	 * otherwise it is disabled.
	 *
	 * @see struct rte_table_action_mtr_counters_tc
	 */
	int n_packets_enabled;

	/** When non-zero, the *n_bytes* meter stats counter is enabled,
	 * otherwise it is disabled.
	 *
	 * @see struct rte_table_action_mtr_counters_tc
	 */
	int n_bytes_enabled;
};

/** Meter action parameters (per table rule). */
struct rte_table_action_mtr_params {
	/** Traffic meter and policer parameters for each of the *tc_mask*
	 * traffic classes.
	 */
	struct rte_table_action_mtr_tc_params mtr[RTE_TABLE_ACTION_TC_MAX];

	/** Bit mask defining which traffic class parameters are valid in *mtr*.
	 * If bit N is set in *tc_mask*, then parameters for traffic class N are
	 * valid in *mtr*.
	 */
	uint32_t tc_mask;
};

/** Meter action statistics counters (per table rule). */
struct rte_table_action_mtr_counters {
	/** Stats counters for each of the *tc_mask* traffic classes. */
	struct rte_table_action_mtr_counters_tc stats[RTE_TABLE_ACTION_TC_MAX];

	/** Bit mask defining which traffic class parameters are valid in *mtr*.
	 * If bit N is set in *tc_mask*, then parameters for traffic class N are
	 * valid in *mtr*.
	 */
	uint32_t tc_mask;
};

/**
 * RTE_TABLE_ACTION_TM
 */
/** Traffic management action configuration (per table action profile). */
struct rte_table_action_tm_config {
	/** Number of subports per port. */
	uint32_t n_subports_per_port;

	/** Number of pipes per subport. */
	uint32_t n_pipes_per_subport;
};

/** Traffic management action parameters (per table rule). */
struct rte_table_action_tm_params {
	/** Subport ID. */
	uint32_t subport_id;

	/** Pipe ID. */
	uint32_t pipe_id;
};

/**
 * RTE_TABLE_ACTION_ENCAP
 */
/** Supported packet encapsulation types. */
enum rte_table_action_encap_type {
	/** IP -> { Ether | IP } */
	RTE_TABLE_ACTION_ENCAP_ETHER = 0,

	/** IP -> { Ether | VLAN | IP } */
	RTE_TABLE_ACTION_ENCAP_VLAN,

	/** IP -> { Ether | S-VLAN | C-VLAN | IP } */
	RTE_TABLE_ACTION_ENCAP_QINQ,

	/** IP -> { Ether | MPLS | IP } */
	RTE_TABLE_ACTION_ENCAP_MPLS,

	/** IP -> { Ether | PPPoE | PPP | IP } */
	RTE_TABLE_ACTION_ENCAP_PPPOE,

	/** Ether -> { Ether | IP | UDP | VXLAN | Ether }
	 * Ether -> { Ether | VLAN | IP | UDP | VXLAN | Ether }
	 */
	RTE_TABLE_ACTION_ENCAP_VXLAN,

	/** IP -> { Ether | S-VLAN | C-VLAN | PPPoE | PPP | IP } */
	RTE_TABLE_ACTION_ENCAP_QINQ_PPPOE,
};

/** Pre-computed Ethernet header fields for encapsulation action. */
struct rte_table_action_ether_hdr {
	struct rte_ether_addr da; /**< Destination address. */
	struct rte_ether_addr sa; /**< Source address. */
};

/** Pre-computed VLAN header fields for encapsulation action. */
struct rte_table_action_vlan_hdr {
	uint8_t pcp; /**< Priority Code Point (PCP). */
	uint8_t dei; /**< Drop Eligibility Indicator (DEI). */
	uint16_t vid; /**< VLAN Identifier (VID). */
};

/** Pre-computed MPLS header fields for encapsulation action. */
struct rte_table_action_mpls_hdr {
	uint32_t label; /**< Label. */
	uint8_t tc; /**< Traffic Class (TC). */
	uint8_t ttl; /**< Time to Live (TTL). */
};

/** Pre-computed PPPoE header fields for encapsulation action. */
struct rte_table_action_pppoe_hdr {
	uint16_t session_id; /**< Session ID. */
};

/** Pre-computed IPv4 header fields for encapsulation action. */
struct rte_table_action_ipv4_header {
	uint32_t sa; /**< Source address. */
	uint32_t da; /**< Destination address. */
	uint8_t dscp; /**< DiffServ Code Point (DSCP). */
	uint8_t ttl; /**< Time To Live (TTL). */
};

/** Pre-computed IPv6 header fields for encapsulation action. */
struct rte_table_action_ipv6_header {
	uint8_t sa[16]; /**< Source address. */
	uint8_t da[16]; /**< Destination address. */
	uint32_t flow_label; /**< Flow label. */
	uint8_t dscp; /**< DiffServ Code Point (DSCP). */
	uint8_t hop_limit; /**< Hop Limit (HL). */
};

/** Pre-computed UDP header fields for encapsulation action. */
struct rte_table_action_udp_header {
	uint16_t sp; /**< Source port. */
	uint16_t dp; /**< Destination port. */
};

/** Pre-computed VXLAN header fields for encapsulation action. */
struct rte_table_action_vxlan_hdr {
	uint32_t vni; /**< VXLAN Network Identifier (VNI). */
};

/** Ether encap parameters. */
struct rte_table_action_encap_ether_params {
	struct rte_table_action_ether_hdr ether; /**< Ethernet header. */
};

/** VLAN encap parameters. */
struct rte_table_action_encap_vlan_params {
	struct rte_table_action_ether_hdr ether; /**< Ethernet header. */
	struct rte_table_action_vlan_hdr vlan; /**< VLAN header. */
};

/** QinQ encap parameters. */
struct rte_table_action_encap_qinq_params {
	struct rte_table_action_ether_hdr ether; /**< Ethernet header. */
	struct rte_table_action_vlan_hdr svlan; /**< Service VLAN header. */
	struct rte_table_action_vlan_hdr cvlan; /**< Customer VLAN header. */
};

/** Max number of MPLS labels per output packet for MPLS encapsulation. */
#ifndef RTE_TABLE_ACTION_MPLS_LABELS_MAX
#define RTE_TABLE_ACTION_MPLS_LABELS_MAX                   4
#endif

/** MPLS encap parameters. */
struct rte_table_action_encap_mpls_params {
	/** Ethernet header. */
	struct rte_table_action_ether_hdr ether;

	/** MPLS header. */
	struct rte_table_action_mpls_hdr mpls[RTE_TABLE_ACTION_MPLS_LABELS_MAX];

	/** Number of MPLS labels in MPLS header. */
	uint32_t mpls_count;

	/** Non-zero for MPLS unicast, zero for MPLS multicast. */
	int unicast;
};

/** PPPoE encap parameters. */
struct rte_table_action_encap_pppoe_params {
	struct rte_table_action_ether_hdr ether; /**< Ethernet header. */
	struct rte_table_action_pppoe_hdr pppoe; /**< PPPoE/PPP headers. */
};

/** VXLAN encap parameters. */
struct rte_table_action_encap_vxlan_params {
	struct rte_table_action_ether_hdr ether; /**< Ethernet header. */
	struct rte_table_action_vlan_hdr vlan; /**< VLAN header. */

	RTE_STD_C11
	union {
		struct rte_table_action_ipv4_header ipv4; /**< IPv4 header. */
		struct rte_table_action_ipv6_header ipv6; /**< IPv6 header. */
	};

	struct rte_table_action_udp_header udp; /**< UDP header. */
	struct rte_table_action_vxlan_hdr vxlan; /**< VXLAN header. */
};

/** Encap action configuration (per table action profile). */
struct rte_table_action_encap_config {
	/** Bit mask defining the set of packet encapsulations enabled for the
	 * current table action profile. If bit (1 << N) is set in *encap_mask*,
	 * then packet encapsulation N is enabled, otherwise it is disabled.
	 *
	 * @see enum rte_table_action_encap_type
	 */
	uint64_t encap_mask;

	/** Encapsulation type specific configuration. */
	RTE_STD_C11
	union {
		struct {
			/** Input packet to be encapsulated: offset within the
			 * input packet buffer to the start of the Ethernet
			 * frame to be encapsulated. Offset 0 points to the
			 * first byte of the MBUF structure.
			 */
			uint32_t data_offset;

			/** Encapsulation header: non-zero when encapsulation
			 * header includes a VLAN tag, zero otherwise.
			 */
			int vlan;

			/** Encapsulation header: IP version of the IP header
			 * within the encapsulation header. Non-zero for IPv4,
			 * zero for IPv6.
			 */
			int ip_version;
		} vxlan; /**< VXLAN specific configuration. */
	};
};

/** QinQ_PPPoE encap parameters. */
struct rte_table_encap_ether_qinq_pppoe {

	/** Only valid when *type* is set to QinQ. */
	struct rte_table_action_ether_hdr ether;
	struct rte_table_action_vlan_hdr svlan; /**< Service VLAN header. */
	struct rte_table_action_vlan_hdr cvlan; /**< Customer VLAN header. */
	struct rte_table_action_pppoe_hdr pppoe; /**< PPPoE/PPP headers. */
};

/** Encap action parameters (per table rule). */
struct rte_table_action_encap_params {
	/** Encapsulation type. */
	enum rte_table_action_encap_type type;

	RTE_STD_C11
	union {
		/** Only valid when *type* is set to Ether. */
		struct rte_table_action_encap_ether_params ether;

		/** Only valid when *type* is set to VLAN. */
		struct rte_table_action_encap_vlan_params vlan;

		/** Only valid when *type* is set to QinQ. */
		struct rte_table_action_encap_qinq_params qinq;

		/** Only valid when *type* is set to MPLS. */
		struct rte_table_action_encap_mpls_params mpls;

		/** Only valid when *type* is set to PPPoE. */
		struct rte_table_action_encap_pppoe_params pppoe;

		/** Only valid when *type* is set to VXLAN. */
		struct rte_table_action_encap_vxlan_params vxlan;

		/** Only valid when *type* is set to QinQ_PPPoE. */
		struct rte_table_encap_ether_qinq_pppoe qinq_pppoe;
	};
};

/**
 * RTE_TABLE_ACTION_NAT
 */
/** NAT action configuration (per table action profile). */
struct rte_table_action_nat_config {
	/** When non-zero, the IP source address and L4 protocol source port are
	 * translated. When zero, the IP destination address and L4 protocol
	 * destination port are translated.
	 */
	int source_nat;

	/** Layer 4 protocol, for example TCP (0x06) or UDP (0x11). The checksum
	 * field is computed differently and placed at different header offset
	 * by each layer 4 protocol.
	 */
	uint8_t proto;
};

/** NAT action parameters (per table rule). */
struct rte_table_action_nat_params {
	/** IP version for *addr*: non-zero for IPv4, zero for IPv6. */
	int ip_version;

	/** IP address. */
	union {
		/** IPv4 address; only valid when *ip_version* is non-zero. */
		uint32_t ipv4;

		/** IPv6 address; only valid when *ip_version* is set to 0. */
		uint8_t ipv6[16];
	} addr;

	/** Port. */
	uint16_t port;
};

/**
 * RTE_TABLE_ACTION_TTL
 */
/** TTL action configuration (per table action profile). */
struct rte_table_action_ttl_config {
	/** When non-zero, the input packets whose updated IPv4 Time to Live
	 * (TTL) field or IPv6 Hop Limit (HL) field is zero are dropped.
	 * When zero, the input packets whose updated IPv4 TTL field or IPv6 HL
	 * field is zero are forwarded as usual (typically for debugging
	 * purpose).
	 */
	int drop;

	/** When non-zero, the *n_packets* stats counter for TTL action is
	 * enabled, otherwise disabled.
	 *
	 * @see struct rte_table_action_ttl_counters
	 */
	int n_packets_enabled;
};

/** TTL action parameters (per table rule). */
struct rte_table_action_ttl_params {
	/** When non-zero, decrement the IPv4 TTL field and update the checksum
	 * field, or decrement the IPv6 HL field. When zero, the IPv4 TTL field
	 * or the IPv6 HL field is not changed.
	 */
	int decrement;
};

/** TTL action statistics packets (per table rule). */
struct rte_table_action_ttl_counters {
	/** Number of IPv4 packets whose updated TTL field is zero or IPv6
	 * packets whose updated HL field is zero.
	 */
	uint64_t n_packets;
};

/**
 * RTE_TABLE_ACTION_STATS
 */
/** Stats action configuration (per table action profile). */
struct rte_table_action_stats_config {
	/** When non-zero, the *n_packets* stats counter is enabled, otherwise
	 * disabled.
	 *
	 * @see struct rte_table_action_stats_counters
	 */
	int n_packets_enabled;

	/** When non-zero, the *n_bytes* stats counter is enabled, otherwise
	 * disabled.
	 *
	 * @see struct rte_table_action_stats_counters
	 */
	int n_bytes_enabled;
};

/** Stats action parameters (per table rule). */
struct rte_table_action_stats_params {
	/** Initial value for the *n_packets* stats counter. Typically set to 0.
	 *
	 * @see struct rte_table_action_stats_counters
	 */
	uint64_t n_packets;

	/** Initial value for the *n_bytes* stats counter. Typically set to 0.
	 *
	 * @see struct rte_table_action_stats_counters
	 */
	uint64_t n_bytes;
};

/** Stats action counters (per table rule). */
struct rte_table_action_stats_counters {
	/** Number of packets. Valid only when *n_packets_valid* is non-zero. */
	uint64_t n_packets;

	/** Number of bytes. Valid only when *n_bytes_valid* is non-zero. */
	uint64_t n_bytes;

	/** When non-zero, the *n_packets* field is valid, otherwise invalid. */
	int n_packets_valid;

	/** When non-zero, the *n_bytes* field is valid, otherwise invalid. */
	int n_bytes_valid;
};

/**
 * RTE_TABLE_ACTION_TIME
 */
/** Timestamp action parameters (per table rule). */
struct rte_table_action_time_params {
	/** Initial timestamp value. Typically set to current time. */
	uint64_t time;
};

/**
 * RTE_TABLE_ACTION_CRYPTO
 */
#ifndef RTE_TABLE_ACTION_SYM_CRYPTO_IV_SIZE_MAX
#define RTE_TABLE_ACTION_SYM_CRYPTO_IV_SIZE_MAX		(16)
#endif

#ifndef RTE_TABLE_ACTION_SYM_CRYPTO_AAD_SIZE_MAX
#define RTE_TABLE_ACTION_SYM_CRYPTO_AAD_SIZE_MAX	(16)
#endif

#ifndef RTE_TABLE_ACTION_SYM_CRYPTO_IV_OFFSET
#define RTE_TABLE_ACTION_SYM_CRYPTO_IV_OFFSET				\
	(sizeof(struct rte_crypto_op) + sizeof(struct rte_crypto_sym_op))
#endif

/** Common action structure to store the data's value, length, and offset */
struct rte_table_action_vlo {
	uint8_t *val;
	uint32_t length;
	uint32_t offset;
};

/** Symmetric crypto action configuration (per table action profile). */
struct rte_table_action_sym_crypto_config {
	/** Target Cryptodev ID. */
	uint8_t cryptodev_id;

	/**
	 * Offset to rte_crypto_op structure within the input packet buffer.
	 * Offset 0 points to the first byte of the MBUF structure.
	 */
	uint32_t op_offset;

	/** The mempool for creating cryptodev sessions. */
	struct rte_mempool *mp_create;

	/** The mempool for initializing cryptodev sessions. */
	struct rte_mempool *mp_init;
};

/** Symmetric Crypto action parameters (per table rule). */
struct rte_table_action_sym_crypto_params {

	/** Xform pointer contains all relevant information */
	struct rte_crypto_sym_xform *xform;

	/**
	 * Offset within the input packet buffer to the first byte of data
	 * to be processed by the crypto unit. Offset 0 points to the first
	 * byte of the MBUF structure.
	 */
	uint32_t data_offset;

	union {
		struct {
			/** Cipher iv data. */
			struct rte_table_action_vlo cipher_iv;

			/** Cipher iv data. */
			struct rte_table_action_vlo cipher_iv_update;

			/** Auth iv data. */
			struct rte_table_action_vlo auth_iv;

			/** Auth iv data. */
			struct rte_table_action_vlo auth_iv_update;

		} cipher_auth;

		struct {
			/** AEAD AAD data. */
			struct rte_table_action_vlo aad;

			/** AEAD iv data. */
			struct rte_table_action_vlo iv;

			/** AEAD AAD data. */
			struct rte_table_action_vlo aad_update;

			/** AEAD iv data. */
			struct rte_table_action_vlo iv_update;

		} aead;
	};
};

/**
 * RTE_TABLE_ACTION_TAG
 */
/** Tag action parameters (per table rule). */
struct rte_table_action_tag_params {
	/** Tag to be attached to the input packet. */
	uint32_t tag;
};

/**
 * RTE_TABLE_ACTION_DECAP
 */
/** Decap action parameters (per table rule). */
struct rte_table_action_decap_params {
	/** Number of bytes to be removed from the start of the packet. */
	uint16_t n;
};

/**
 * Table action profile.
 */
struct rte_table_action_profile;

/**
 * Table action profile create.
 *
 * @param[in] common
 *   Common action configuration.
 * @return
 *   Table action profile handle on success, NULL otherwise.
 */
__rte_experimental
struct rte_table_action_profile *
rte_table_action_profile_create(struct rte_table_action_common_config *common);

/**
 * Table action profile free.
 *
 * @param[in] profile
 *   Table profile action handle (needs to be valid).
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_profile_free(struct rte_table_action_profile *profile);

/**
 * Table action profile action register.
 *
 * @param[in] profile
 *   Table profile action handle (needs to be valid and not in frozen state).
 * @param[in] type
 *   Specific table action to be registered for *profile*.
 * @param[in] action_config
 *   Configuration for the *type* action.
 *   If struct rte_table_action_*type*_config is defined by the Table Action
 *   API, it needs to point to a valid instance of this structure, otherwise it
 *   needs to be set to NULL.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_profile_action_register(struct rte_table_action_profile *profile,
	enum rte_table_action_type type,
	void *action_config);

/**
 * Table action profile freeze.
 *
 * Once this function is called successfully, the given profile enters the
 * frozen state with the following immediate effects: no more actions can be
 * registered for this profile, so the profile can be instantiated to create
 * table action objects.
 *
 * @param[in] profile
 *   Table profile action handle (needs to be valid and not in frozen state).
 * @return
 *   Zero on success, non-zero error code otherwise.
 *
 * @see rte_table_action_create()
 */
__rte_experimental
int
rte_table_action_profile_freeze(struct rte_table_action_profile *profile);

/**
 * Table action.
 */
struct rte_table_action;

/**
 * Table action create.
 *
 * Instantiates the given table action profile to create a table action object.
 *
 * @param[in] profile
 *   Table profile action handle (needs to be valid and in frozen state).
 * @param[in] socket_id
 *   CPU socket ID where the internal data structures required by the new table
 *   action object should be allocated.
 * @return
 *   Handle to table action object on success, NULL on error.
 *
 * @see rte_table_action_create()
 */
__rte_experimental
struct rte_table_action *
rte_table_action_create(struct rte_table_action_profile *profile,
	uint32_t socket_id);

/**
 * Table action free.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_free(struct rte_table_action *action);

/**
 * Table action table params get.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[inout] params
 *   Pipeline table parameters (needs to be pre-allocated).
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_table_params_get(struct rte_table_action *action,
	struct rte_pipeline_table_params *params);

/**
 * Table action apply.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] data
 *   Data byte array (typically table rule data) to apply action *type* on.
 * @param[in] type
 *   Specific table action previously registered for the table action profile of
 *   the *action* object.
 * @param[in] action_params
 *   Parameters for the *type* action.
 *   If struct rte_table_action_*type*_params is defined by the Table Action
 *   API, it needs to point to a valid instance of this structure, otherwise it
 *   needs to be set to NULL.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_apply(struct rte_table_action *action,
	void *data,
	enum rte_table_action_type type,
	void *action_params);

/**
 * Table action DSCP table update.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] dscp_mask
 *   64-bit mask defining the DSCP table entries to be updated. If bit N is set
 *   in this bit mask, then DSCP table entry N is to be updated, otherwise not.
 * @param[in] table
 *   DSCP table.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_dscp_table_update(struct rte_table_action *action,
	uint64_t dscp_mask,
	struct rte_table_action_dscp_table *table);

/**
 * Table action meter profile add.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] meter_profile_id
 *   Meter profile ID to be used for the *profile* once it is successfully added
 *   to the *action* object (needs to be unused by the set of meter profiles
 *   currently registered for the *action* object).
 * @param[in] profile
 *   Meter profile to be added.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_meter_profile_add(struct rte_table_action *action,
	uint32_t meter_profile_id,
	struct rte_table_action_meter_profile *profile);

/**
 * Table action meter profile delete.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] meter_profile_id
 *   Meter profile ID of the meter profile to be deleted from the *action*
 *   object (needs to be valid for the *action* object).
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_meter_profile_delete(struct rte_table_action *action,
	uint32_t meter_profile_id);

/**
 * Table action meter read.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] data
 *   Data byte array (typically table rule data) with meter action previously
 *   applied on it.
 * @param[in] tc_mask
 *   Bit mask defining which traffic classes should have the meter stats
 *   counters read from *data* and stored into *stats*. If bit N is set in this
 *   bit mask, then traffic class N is part of this operation, otherwise it is
 *   not. If bit N is set in this bit mask, then traffic class N must be one of
 *   the traffic classes that are enabled for the meter action in the table
 *   action profile used by the *action* object.
 * @param[inout] stats
 *   When non-NULL, it points to the area where the meter stats counters read
 *   from *data* are saved. Only the meter stats counters for the *tc_mask*
 *   traffic classes are read and stored to *stats*.
 * @param[in] clear
 *   When non-zero, the meter stats counters are cleared (i.e. set to zero),
 *   otherwise the counters are not modified. When the read operation is enabled
 *   (*stats* is non-NULL), the clear operation is performed after the read
 *   operation is completed.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_meter_read(struct rte_table_action *action,
	void *data,
	uint32_t tc_mask,
	struct rte_table_action_mtr_counters *stats,
	int clear);

/**
 * Table action TTL read.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] data
 *   Data byte array (typically table rule data) with TTL action previously
 *   applied on it.
 * @param[inout] stats
 *   When non-NULL, it points to the area where the TTL stats counters read from
 *   *data* are saved.
 * @param[in] clear
 *   When non-zero, the TTL stats counters are cleared (i.e. set to zero),
 *   otherwise the counters are not modified. When the read operation is enabled
 *   (*stats* is non-NULL), the clear operation is performed after the read
 *   operation is completed.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_ttl_read(struct rte_table_action *action,
	void *data,
	struct rte_table_action_ttl_counters *stats,
	int clear);

/**
 * Table action stats read.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] data
 *   Data byte array (typically table rule data) with stats action previously
 *   applied on it.
 * @param[inout] stats
 *   When non-NULL, it points to the area where the stats counters read from
 *   *data* are saved.
 * @param[in] clear
 *   When non-zero, the stats counters are cleared (i.e. set to zero), otherwise
 *   the counters are not modified. When the read operation is enabled (*stats*
 *   is non-NULL), the clear operation is performed after the read operation is
 *   completed.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_stats_read(struct rte_table_action *action,
	void *data,
	struct rte_table_action_stats_counters *stats,
	int clear);

/**
 * Table action timestamp read.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] data
 *   Data byte array (typically table rule data) with timestamp action
 *   previously applied on it.
 * @param[inout] timestamp
 *   Pre-allocated memory where the timestamp read from *data* is saved (has to
 *   be non-NULL).
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_table_action_time_read(struct rte_table_action *action,
	void *data,
	uint64_t *timestamp);

/**
 * Table action cryptodev symmetric session get.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] data
 *   Data byte array (typically table rule data) with sym crypto action.
 * @return
 *   The pointer to the session on success, NULL otherwise.
 */
__rte_experimental
struct rte_cryptodev_sym_session *
rte_table_action_crypto_sym_session_get(struct rte_table_action *action,
	void *data);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_TABLE_ACTION_H__ */
