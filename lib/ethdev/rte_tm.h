/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation.
 * Copyright(c) 2017 Cavium.
 * Copyright(c) 2017 NXP.
 */

#ifndef __INCLUDE_RTE_TM_H__
#define __INCLUDE_RTE_TM_H__

/**
 * @file
 * RTE Generic Traffic Manager API
 *
 * This interface provides the ability to configure the traffic manager in a
 * generic way. It includes features such as: hierarchical scheduling,
 * traffic shaping, congestion management, packet marking, etc.
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 */

#include <stdint.h>

#include <rte_compat.h>
#include <rte_common.h>
#include <rte_meter.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Ethernet framing overhead.
 *
 * Overhead fields per Ethernet frame:
 * 1. Preamble:                                            7 bytes;
 * 2. Start of Frame Delimiter (SFD):                      1 byte;
 * 3. Inter-Frame Gap (IFG):                              12 bytes.
 *
 * One of the typical values for the *pkt_length_adjust* field of the shaper
 * profile.
 *
 * @see struct rte_tm_shaper_params
 */
#define RTE_TM_ETH_FRAMING_OVERHEAD                  20

/**
 * Ethernet framing overhead including the Frame Check Sequence (FCS) field.
 * Useful when FCS is generated and added at the end of the Ethernet frame on
 * Tx side without any SW intervention.
 *
 * One of the typical values for the pkt_length_adjust field of the shaper
 * profile.
 *
 * @see struct rte_tm_shaper_params
 */
#define RTE_TM_ETH_FRAMING_OVERHEAD_FCS              24

/**
 * Invalid WRED profile ID.
 *
 * @see struct rte_tm_node_params
 * @see rte_tm_node_add()
 * @see rte_tm_node_wred_context_update()
 */
#define RTE_TM_WRED_PROFILE_ID_NONE                  UINT32_MAX

/**
 *Invalid shaper profile ID.
 *
 * @see struct rte_tm_node_params
 * @see rte_tm_node_add()
 * @see rte_tm_node_shaper_update()
 */
#define RTE_TM_SHAPER_PROFILE_ID_NONE                UINT32_MAX

/**
 * Node ID for the parent of the root node.
 *
 * @see rte_tm_node_add()
 */
#define RTE_TM_NODE_ID_NULL                          UINT32_MAX

/**
 * Node level ID used to disable level ID checking.
 *
 * @see rte_tm_node_add()
 */
#define RTE_TM_NODE_LEVEL_ID_ANY                     UINT32_MAX

/**
 * Node statistics counter type
 */
enum rte_tm_stats_type {
	/** Number of packets scheduled from current node. */
	RTE_TM_STATS_N_PKTS = 1 << 0,

	/** Number of bytes scheduled from current node. */
	RTE_TM_STATS_N_BYTES = 1 << 1,

	/** Number of green packets dropped by current leaf node.  */
	RTE_TM_STATS_N_PKTS_GREEN_DROPPED = 1 << 2,

	/** Number of yellow packets dropped by current leaf node.  */
	RTE_TM_STATS_N_PKTS_YELLOW_DROPPED = 1 << 3,

	/** Number of red packets dropped by current leaf node.  */
	RTE_TM_STATS_N_PKTS_RED_DROPPED = 1 << 4,

	/** Number of green bytes dropped by current leaf node.  */
	RTE_TM_STATS_N_BYTES_GREEN_DROPPED = 1 << 5,

	/** Number of yellow bytes dropped by current leaf node.  */
	RTE_TM_STATS_N_BYTES_YELLOW_DROPPED = 1 << 6,

	/** Number of red bytes dropped by current leaf node.  */
	RTE_TM_STATS_N_BYTES_RED_DROPPED = 1 << 7,

	/** Number of packets currently waiting in the packet queue of current
	 * leaf node.
	 */
	RTE_TM_STATS_N_PKTS_QUEUED = 1 << 8,

	/** Number of bytes currently waiting in the packet queue of current
	 * leaf node.
	 */
	RTE_TM_STATS_N_BYTES_QUEUED = 1 << 9,
};

/**
 * Node statistics counters
 */
struct rte_tm_node_stats {
	/** Number of packets scheduled from current node. */
	uint64_t n_pkts;

	/** Number of bytes scheduled from current node. */
	uint64_t n_bytes;

	/** Statistics counters for leaf nodes only. */
	struct {
		/** Number of packets dropped by current leaf node per each
		 * color.
		 */
		uint64_t n_pkts_dropped[RTE_COLORS];

		/** Number of bytes dropped by current leaf node per each
		 * color.
		 */
		uint64_t n_bytes_dropped[RTE_COLORS];

		/** Number of packets currently waiting in the packet queue of
		 * current leaf node.
		 */
		uint64_t n_pkts_queued;

		/** Number of bytes currently waiting in the packet queue of
		 * current leaf node.
		 */
		uint64_t n_bytes_queued;
	} leaf;
};

/**
 * Traffic manager dynamic updates
 */
enum rte_tm_dynamic_update_type {
	/** Dynamic parent node update. The new parent node is located on same
	 * hierarchy level as the former parent node. Consequently, the node
	 * whose parent is changed preserves its hierarchy level.
	 */
	RTE_TM_UPDATE_NODE_PARENT_KEEP_LEVEL = 1 << 0,

	/** Dynamic parent node update. The new parent node is located on
	 * different hierarchy level than the former parent node. Consequently,
	 * the node whose parent is changed also changes its hierarchy level.
	 */
	RTE_TM_UPDATE_NODE_PARENT_CHANGE_LEVEL = 1 << 1,

	/** Dynamic node add/delete. */
	RTE_TM_UPDATE_NODE_ADD_DELETE = 1 << 2,

	/** Suspend/resume nodes. */
	RTE_TM_UPDATE_NODE_SUSPEND_RESUME = 1 << 3,

	/** Dynamic switch between byte-based and packet-based WFQ weights. */
	RTE_TM_UPDATE_NODE_WFQ_WEIGHT_MODE = 1 << 4,

	/** Dynamic update on number of SP priorities. */
	RTE_TM_UPDATE_NODE_N_SP_PRIORITIES = 1 << 5,

	/** Dynamic update of congestion management mode for leaf nodes. */
	RTE_TM_UPDATE_NODE_CMAN = 1 << 6,

	/** Dynamic update of the set of enabled stats counter types. */
	RTE_TM_UPDATE_NODE_STATS = 1 << 7,
};

/**
 * Traffic manager capabilities
 */
struct rte_tm_capabilities {
	/** Maximum number of nodes. */
	uint32_t n_nodes_max;

	/** Maximum number of levels (i.e. number of nodes connecting the root
	 * node with any leaf node, including the root and the leaf).
	 */
	uint32_t n_levels_max;

	/** When non-zero, this flag indicates that all the non-leaf nodes
	 * (with the exception of the root node) have identical capability set.
	 */
	int non_leaf_nodes_identical;

	/** When non-zero, this flag indicates that all the leaf nodes have
	 * identical capability set.
	 */
	int leaf_nodes_identical;

	/** Maximum number of shapers, either private or shared. In case the
	 * implementation does not share any resources between private and
	 * shared shapers, it is typically equal to the sum of
	 * *shaper_private_n_max* and *shaper_shared_n_max*. The
	 * value of zero indicates that traffic shaping is not supported.
	 */
	uint32_t shaper_n_max;

	/** Maximum number of private shapers. Indicates the maximum number of
	 * nodes that can concurrently have their private shaper enabled. The
	 * value of zero indicates that private shapers are not supported.
	 */
	uint32_t shaper_private_n_max;

	/** Maximum number of private shapers that support dual rate shaping.
	 * Indicates the maximum number of nodes that can concurrently have
	 * their private shaper enabled with dual rate support. Only valid when
	 * private shapers are supported. The value of zero indicates that dual
	 * rate shaping is not available for private shapers. The maximum value
	 * is *shaper_private_n_max*.
	 */
	int shaper_private_dual_rate_n_max;

	/** Minimum committed/peak rate (bytes per second) for any private
	 * shaper. Valid only when private shapers are supported.
	 */
	uint64_t shaper_private_rate_min;

	/** Maximum committed/peak rate (bytes per second) for any private
	 * shaper. Valid only when private shapers are supported.
	 */
	uint64_t shaper_private_rate_max;

	/** Shaper private packet mode supported. When non-zero, this parameter
	 * indicates that there is at least one node that can be configured
	 * with packet mode in its private shaper. When shaper is configured
	 * in packet mode, committed/peak rate provided is interpreted
	 * in packets per second.
	 */
	int shaper_private_packet_mode_supported;

	/** Shaper private byte mode supported. When non-zero, this parameter
	 * indicates that there is at least one node that can be configured
	 * with byte mode in its private shaper. When shaper is configured
	 * in byte mode, committed/peak rate provided is interpreted in
	 * bytes per second.
	 */
	int shaper_private_byte_mode_supported;


	/** Maximum number of shared shapers. The value of zero indicates that
	 * shared shapers are not supported.
	 */
	uint32_t shaper_shared_n_max;

	/** Maximum number of nodes that can share the same shared shaper.
	 * Only valid when shared shapers are supported.
	 */
	uint32_t shaper_shared_n_nodes_per_shaper_max;

	/** Maximum number of shared shapers a node can be part of. This
	 * parameter indicates that there is at least one node that can be
	 * configured with this many shared shapers, which might not be true for
	 * all the nodes. Only valid when shared shapers are supported, in which
	 * case it ranges from 1 to *shaper_shared_n_max*.
	 */
	uint32_t shaper_shared_n_shapers_per_node_max;

	/** Maximum number of shared shapers that can be configured with dual
	 * rate shaping. The value of zero indicates that dual rate shaping
	 * support is not available for shared shapers.
	 */
	uint32_t shaper_shared_dual_rate_n_max;

	/** Minimum committed/peak rate (bytes per second) for any shared
	 * shaper. Only valid when shared shapers are supported.
	 */
	uint64_t shaper_shared_rate_min;

	/** Maximum committed/peak rate (bytes per second) for any shared
	 * shaper. Only valid when shared shapers are supported.
	 */
	uint64_t shaper_shared_rate_max;

	/** Shaper shared packet mode supported. When non-zero, this parameter
	 * indicates a shared shaper can be configured with packet mode.
	 * When shared shaper is configured in packet mode, committed/peak rate
	 * provided is interpreted in packets per second.
	 */
	int shaper_shared_packet_mode_supported;

	/** Shaper shared byte mode supported. When non-zero, this parameter
	 * indicates that a shared shaper can be configured with byte mode.
	 * When shared shaper is configured in byte mode, committed/peak rate
	 * provided is interpreted in bytes per second.
	 */
	int shaper_shared_byte_mode_supported;


	/** Minimum value allowed for packet length adjustment for any private
	 * or shared shaper.
	 */
	int shaper_pkt_length_adjust_min;

	/** Maximum value allowed for packet length adjustment for any private
	 * or shared shaper.
	 */
	int shaper_pkt_length_adjust_max;

	/** Maximum number of children nodes. This parameter indicates that
	 * there is at least one non-leaf node that can be configured with this
	 * many children nodes, which might not be true for all the non-leaf
	 * nodes.
	 */
	uint32_t sched_n_children_max;

	/** Maximum number of supported priority levels. This parameter
	 * indicates that there is at least one non-leaf node that can be
	 * configured with this many priority levels for managing its children
	 * nodes, which might not be true for all the non-leaf nodes. The value
	 * of zero is invalid. The value of 1 indicates that only priority 0 is
	 * supported, which essentially means that Strict Priority (SP)
	 * algorithm is not supported.
	 */
	uint32_t sched_sp_n_priorities_max;

	/** Maximum number of sibling nodes that can have the same priority at
	 * any given time, i.e. maximum size of the WFQ sibling node group. This
	 * parameter indicates there is at least one non-leaf node that meets
	 * this condition, which might not be true for all the non-leaf nodes.
	 * The value of zero is invalid. The value of 1 indicates that WFQ
	 * algorithm is not supported. The maximum value is
	 * *sched_n_children_max*.
	 */
	uint32_t sched_wfq_n_children_per_group_max;

	/** Maximum number of priority levels that can have more than one child
	 * node at any given time, i.e. maximum number of WFQ sibling node
	 * groups that have two or more members. This parameter indicates there
	 * is at least one non-leaf node that meets this condition, which might
	 * not be true for all the non-leaf nodes. The value of zero states that
	 * WFQ algorithm is not supported. The value of 1 indicates that
	 * (*sched_sp_n_priorities_max* - 1) priority levels have at most one
	 * child node, so there can be only one priority level with two or
	 * more sibling nodes making up a WFQ group. The maximum value is:
	 * min(floor(*sched_n_children_max* / 2), *sched_sp_n_priorities_max*).
	 */
	uint32_t sched_wfq_n_groups_max;

	/** Maximum WFQ weight. The value of 1 indicates that all sibling nodes
	 * with same priority have the same WFQ weight, so WFQ is reduced to FQ.
	 */
	uint32_t sched_wfq_weight_max;

	/** WFQ packet mode supported. When non-zero, this parameter indicates
	 * that there is at least one non-leaf node that supports packet mode
	 * for WFQ among its children. WFQ weights will be applied against
	 * packet count for scheduling children when a non-leaf node
	 * is configured appropriately.
	 */
	int sched_wfq_packet_mode_supported;

	/** WFQ byte mode supported. When non-zero, this parameter indicates
	 * that there is at least one non-leaf node that supports byte mode
	 * for WFQ among its children. WFQ weights will be applied against
	 * bytes for scheduling children when a non-leaf node is configured
	 * appropriately.
	 */
	int sched_wfq_byte_mode_supported;

	/** WRED packet mode support. When non-zero, this parameter indicates
	 * that there is at least one leaf node that supports the WRED packet
	 * mode, which might not be true for all the leaf nodes. In packet
	 * mode, the WRED thresholds specify the queue length in packets, as
	 * opposed to bytes.
	 */
	int cman_wred_packet_mode_supported;

	/** WRED byte mode support. When non-zero, this parameter indicates that
	 * there is at least one leaf node that supports the WRED byte mode,
	 * which might not be true for all the leaf nodes. In byte mode, the
	 * WRED thresholds specify the queue length in bytes, as opposed to
	 * packets.
	 */
	int cman_wred_byte_mode_supported;

	/** Head drop algorithm support. When non-zero, this parameter
	 * indicates that there is at least one leaf node that supports the head
	 * drop algorithm, which might not be true for all the leaf nodes.
	 */
	int cman_head_drop_supported;

	/** Maximum number of WRED contexts, either private or shared. In case
	 * the implementation does not share any resources between private and
	 * shared WRED contexts, it is typically equal to the sum of
	 * *cman_wred_context_private_n_max* and
	 * *cman_wred_context_shared_n_max*. The value of zero indicates that
	 * WRED is not supported.
	 */
	uint32_t cman_wred_context_n_max;

	/** Maximum number of private WRED contexts. Indicates the maximum
	 * number of leaf nodes that can concurrently have their private WRED
	 * context enabled. The value of zero indicates that private WRED
	 * contexts are not supported.
	 */
	uint32_t cman_wred_context_private_n_max;

	/** Maximum number of shared WRED contexts. The value of zero
	 * indicates that shared WRED contexts are not supported.
	 */
	uint32_t cman_wred_context_shared_n_max;

	/** Maximum number of leaf nodes that can share the same WRED context.
	 * Only valid when shared WRED contexts are supported.
	 */
	uint32_t cman_wred_context_shared_n_nodes_per_context_max;

	/** Maximum number of shared WRED contexts a leaf node can be part of.
	 * This parameter indicates that there is at least one leaf node that
	 * can be configured with this many shared WRED contexts, which might
	 * not be true for all the leaf nodes. Only valid when shared WRED
	 * contexts are supported, in which case it ranges from 1 to
	 * *cman_wred_context_shared_n_max*.
	 */
	uint32_t cman_wred_context_shared_n_contexts_per_node_max;

	/** Support for VLAN DEI packet marking (per color). */
	int mark_vlan_dei_supported[RTE_COLORS];

	/** Support for IPv4/IPv6 ECN marking of TCP packets (per color). */
	int mark_ip_ecn_tcp_supported[RTE_COLORS];

	/** Support for IPv4/IPv6 ECN marking of SCTP packets (per color). */
	int mark_ip_ecn_sctp_supported[RTE_COLORS];

	/** Support for IPv4/IPv6 DSCP packet marking (per color). */
	int mark_ip_dscp_supported[RTE_COLORS];

	/** Set of supported dynamic update operations.
	 * @see enum rte_tm_dynamic_update_type
	 */
	uint64_t dynamic_update_mask;

	/** Set of supported statistics counter types.
	 * @see enum rte_tm_stats_type
	 */
	uint64_t stats_mask;
};

/**
 * Traffic manager level capabilities
 */
struct rte_tm_level_capabilities {
	/** Maximum number of nodes for the current hierarchy level. */
	uint32_t n_nodes_max;

	/** Maximum number of non-leaf nodes for the current hierarchy level.
	 * The value of 0 indicates that current level only supports leaf
	 * nodes. The maximum value is *n_nodes_max*.
	 */
	uint32_t n_nodes_nonleaf_max;

	/** Maximum number of leaf nodes for the current hierarchy level. The
	 * value of 0 indicates that current level only supports non-leaf
	 * nodes. The maximum value is *n_nodes_max*.
	 */
	uint32_t n_nodes_leaf_max;

	/** When non-zero, this flag indicates that all the non-leaf nodes on
	 * this level have identical capability set. Valid only when
	 * *n_nodes_nonleaf_max* is non-zero.
	 */
	int non_leaf_nodes_identical;

	/** When non-zero, this flag indicates that all the leaf nodes on this
	 * level have identical capability set. Valid only when
	 * *n_nodes_leaf_max* is non-zero.
	 */
	int leaf_nodes_identical;

	union {
		/** Items valid only for the non-leaf nodes on this level. */
		struct {
			/** Private shaper support. When non-zero, it indicates
			 * there is at least one non-leaf node on this level
			 * with private shaper support, which may not be the
			 * case for all the non-leaf nodes on this level.
			 */
			int shaper_private_supported;

			/** Dual rate support for private shaper. Valid only
			 * when private shaper is supported for the non-leaf
			 * nodes on the current level. When non-zero, it
			 * indicates there is at least one non-leaf node on this
			 * level with dual rate private shaper support, which
			 * may not be the case for all the non-leaf nodes on
			 * this level.
			 */
			int shaper_private_dual_rate_supported;

			/** Minimum committed/peak rate (bytes per second) for
			 * private shapers of the non-leaf nodes of this level.
			 * Valid only when private shaper is supported on this
			 * level.
			 */
			uint64_t shaper_private_rate_min;

			/** Maximum committed/peak rate (bytes per second) for
			 * private shapers of the non-leaf nodes on this level.
			 * Valid only when private shaper is supported on this
			 * level.
			 */
			uint64_t shaper_private_rate_max;

			/** Shaper private packet mode supported. When non-zero,
			 * this parameter indicates there is at least one
			 * non-leaf node at this level that can be configured
			 * with packet mode in its private shaper. When private
			 * shaper is configured in packet mode, committed/peak
			 * rate provided is interpreted in packets per second.
			 */
			int shaper_private_packet_mode_supported;

			/** Shaper private byte mode supported. When non-zero,
			 * this parameter indicates there is at least one
			 * non-leaf node at this level that can be configured
			 * with byte mode in its private shaper. When private
			 * shaper is configured in byte mode, committed/peak
			 * rate provided is interpreted in bytes per second.
			 */
			int shaper_private_byte_mode_supported;

			/** Maximum number of shared shapers that any non-leaf
			 * node on this level can be part of. The value of zero
			 * indicates that shared shapers are not supported by
			 * the non-leaf nodes on this level. When non-zero, it
			 * indicates there is at least one non-leaf node on this
			 * level that meets this condition, which may not be the
			 * case for all the non-leaf nodes on this level.
			 */
			uint32_t shaper_shared_n_max;

			/** Shaper shared packet mode supported. When non-zero,
			 * this parameter indicates that there is at least one
			 * non-leaf node on this level that can be part of
			 * shared shapers which work in packet mode.
			 */
			int shaper_shared_packet_mode_supported;

			/** Shaper shared byte mode supported. When non-zero,
			 * this parameter indicates that there is at least one
			 * non-leaf node on this level that can be part of
			 * shared shapers which work in byte mode.
			 */
			int shaper_shared_byte_mode_supported;

			/** Maximum number of children nodes. This parameter
			 * indicates that there is at least one non-leaf node on
			 * this level that can be configured with this many
			 * children nodes, which might not be true for all the
			 * non-leaf nodes on this level.
			 */
			uint32_t sched_n_children_max;

			/** Maximum number of supported priority levels. This
			 * parameter indicates that there is at least one
			 * non-leaf node on this level that can be configured
			 * with this many priority levels for managing its
			 * children nodes, which might not be true for all the
			 * non-leaf nodes on this level. The value of zero is
			 * invalid. The value of 1 indicates that only priority
			 * 0 is supported, which essentially means that Strict
			 * Priority (SP) algorithm is not supported on this
			 * level.
			 */
			uint32_t sched_sp_n_priorities_max;

			/** Maximum number of sibling nodes that can have the
			 * same priority at any given time, i.e. maximum size of
			 * the WFQ sibling node group. This parameter indicates
			 * there is at least one non-leaf node on this level
			 * that meets this condition, which may not be true for
			 * all the non-leaf nodes on this level. The value of
			 * zero is invalid. The value of 1 indicates that WFQ
			 * algorithm is not supported on this level. The maximum
			 * value is *sched_n_children_max*.
			 */
			uint32_t sched_wfq_n_children_per_group_max;

			/** Maximum number of priority levels that can have
			 * more than one child node at any given time, i.e.
			 * maximum number of WFQ sibling node groups that
			 * have two or more members. This parameter indicates
			 * there is at least one non-leaf node on this level
			 * that meets this condition, which might not be true
			 * for all the non-leaf nodes. The value of zero states
			 * that WFQ algorithm is not supported on this level.
			 * The value of 1 indicates that
			 * (*sched_sp_n_priorities_max* - 1) priority levels on
			 * this level have at most one child node, so there can
			 * be only one priority level with two or more sibling
			 * nodes making up a WFQ group on this level. The
			 * maximum value is:
			 * min(floor(*sched_n_children_max* / 2),
			 * *sched_sp_n_priorities_max*).
			 */
			uint32_t sched_wfq_n_groups_max;

			/** Maximum WFQ weight. The value of 1 indicates that
			 * all sibling nodes on this level with same priority
			 * have the same WFQ weight, so on this level WFQ is
			 * reduced to FQ.
			 */
			uint32_t sched_wfq_weight_max;

			/** WFQ packet mode supported. When non-zero, this
			 * parameter indicates that there is at least one
			 * non-leaf node at this level that supports packet
			 * mode for WFQ among its children. WFQ weights will
			 * be applied against packet count for scheduling
			 * children when a non-leaf node is configured
			 * appropriately.
			 */
			int sched_wfq_packet_mode_supported;

			/** WFQ byte mode supported. When non-zero, this
			 * parameter indicates that there is at least one
			 * non-leaf node at this level that supports byte
			 * mode for WFQ among its children. WFQ weights will
			 * be applied against bytes for scheduling children
			 * when a non-leaf node is configured appropriately.
			 */
			int sched_wfq_byte_mode_supported;

			/** Mask of statistics counter types supported by the
			 * non-leaf nodes on this level. Every supported
			 * statistics counter type is supported by at least one
			 * non-leaf node on this level, which may not be true
			 * for all the non-leaf nodes on this level.
			 * @see enum rte_tm_stats_type
			 */
			uint64_t stats_mask;
		} nonleaf;

		/** Items valid only for the leaf nodes on this level. */
		struct {
			/** Private shaper support. When non-zero, it indicates
			 * there is at least one leaf node on this level with
			 * private shaper support, which may not be the case for
			 * all the leaf nodes on this level.
			 */
			int shaper_private_supported;

			/** Dual rate support for private shaper. Valid only
			 * when private shaper is supported for the leaf nodes
			 * on this level. When non-zero, it indicates there is
			 * at least one leaf node on this level with dual rate
			 * private shaper support, which may not be the case for
			 * all the leaf nodes on this level.
			 */
			int shaper_private_dual_rate_supported;

			/** Minimum committed/peak rate (bytes per second) for
			 * private shapers of the leaf nodes of this level.
			 * Valid only when private shaper is supported for the
			 * leaf nodes on this level.
			 */
			uint64_t shaper_private_rate_min;

			/** Maximum committed/peak rate (bytes per second) for
			 * private shapers of the leaf nodes on this level.
			 * Valid only when private shaper is supported for the
			 * leaf nodes on this level.
			 */
			uint64_t shaper_private_rate_max;

			/** Shaper private packet mode supported. When non-zero,
			 * this parameter indicates there is at least one leaf
			 * node at this level that can be configured with
			 * packet mode in its private shaper. When private
			 * shaper is configured in packet mode, committed/peak
			 * rate provided is interpreted in packets per second.
			 */
			int shaper_private_packet_mode_supported;

			/** Shaper private byte mode supported. When non-zero,
			 * this parameter indicates there is at least one leaf
			 * node at this level that can be configured with
			 * byte mode in its private shaper. When private shaper
			 * is configured in byte mode, committed/peak rate
			 * provided is interpreted in bytes per second.
			 */
			int shaper_private_byte_mode_supported;

			/** Maximum number of shared shapers that any leaf node
			 * on this level can be part of. The value of zero
			 * indicates that shared shapers are not supported by
			 * the leaf nodes on this level. When non-zero, it
			 * indicates there is at least one leaf node on this
			 * level that meets this condition, which may not be the
			 * case for all the leaf nodes on this level.
			 */
			uint32_t shaper_shared_n_max;

			/** Shaper shared packet mode supported. When non-zero,
			 * this parameter indicates that there is at least one
			 * leaf node on this level that can be part of
			 * shared shapers which work in packet mode.
			 */
			int shaper_shared_packet_mode_supported;

			/** Shaper shared byte mode supported. When non-zero,
			 * this parameter indicates that there is at least one
			 * leaf node on this level that can be part of
			 * shared shapers which work in byte mode.
			 */
			int shaper_shared_byte_mode_supported;

			/** WRED packet mode support. When non-zero, this
			 * parameter indicates that there is at least one leaf
			 * node on this level that supports the WRED packet
			 * mode, which might not be true for all the leaf
			 * nodes. In packet mode, the WRED thresholds specify
			 * the queue length in packets, as opposed to bytes.
			 */
			int cman_wred_packet_mode_supported;

			/** WRED byte mode support. When non-zero, this
			 * parameter indicates that there is at least one leaf
			 * node on this level that supports the WRED byte mode,
			 * which might not be true for all the leaf nodes. In
			 * byte mode, the WRED thresholds specify the queue
			 * length in bytes, as opposed to packets.
			 */
			int cman_wred_byte_mode_supported;

			/** Head drop algorithm support. When non-zero, this
			 * parameter indicates that there is at least one leaf
			 * node on this level that supports the head drop
			 * algorithm, which might not be true for all the leaf
			 * nodes on this level.
			 */
			int cman_head_drop_supported;

			/** Private WRED context support. When non-zero, it
			 * indicates there is at least one node on this level
			 * with private WRED context support, which may not be
			 * true for all the leaf nodes on this level.
			 */
			int cman_wred_context_private_supported;

			/** Maximum number of shared WRED contexts that any
			 * leaf node on this level can be part of. The value of
			 * zero indicates that shared WRED contexts are not
			 * supported by the leaf nodes on this level. When
			 * non-zero, it indicates there is at least one leaf
			 * node on this level that meets this condition, which
			 * may not be the case for all the leaf nodes on this
			 * level.
			 */
			uint32_t cman_wred_context_shared_n_max;

			/** Mask of statistics counter types supported by the
			 * leaf nodes on this level. Every supported statistics
			 * counter type is supported by at least one leaf node
			 * on this level, which may not be true for all the leaf
			 * nodes on this level.
			 * @see enum rte_tm_stats_type
			 */
			uint64_t stats_mask;
		} leaf;
	};
};

/**
 * Traffic manager node capabilities
 */
struct rte_tm_node_capabilities {
	/** Private shaper support for the current node. */
	int shaper_private_supported;

	/** Dual rate shaping support for private shaper of current node.
	 * Valid only when private shaper is supported by the current node.
	 */
	int shaper_private_dual_rate_supported;

	/** Minimum committed/peak rate (bytes per second) for private
	 * shaper of current node. Valid only when private shaper is supported
	 * by the current node.
	 */
	uint64_t shaper_private_rate_min;

	/** Maximum committed/peak rate (bytes per second) for private
	 * shaper of current node. Valid only when private shaper is supported
	 * by the current node.
	 */
	uint64_t shaper_private_rate_max;

	/** Shaper private packet mode supported. When non-zero, this parameter
	 * indicates private shaper of current node can be configured with
	 * packet mode. When configured in packet mode, committed/peak rate
	 * provided is interpreted in packets per second.
	 */
	int shaper_private_packet_mode_supported;

	/** Shaper private byte mode supported. When non-zero, this parameter
	 * indicates private shaper of current node can be configured with
	 * byte mode. When configured in byte mode, committed/peak rate
	 * provided is interpreted in bytes per second.
	 */
	int shaper_private_byte_mode_supported;

	/** Maximum number of shared shapers the current node can be part of.
	 * The value of zero indicates that shared shapers are not supported by
	 * the current node.
	 */
	uint32_t shaper_shared_n_max;

	/** Shaper shared packet mode supported. When non-zero,
	 * this parameter indicates that current node can be part of
	 * shared shapers which work in packet mode.
	 */
	int shaper_shared_packet_mode_supported;

	/** Shaper shared byte mode supported. When non-zero,
	 * this parameter indicates that current node can be part of
	 * shared shapers which work in byte mode.
	 */
	int shaper_shared_byte_mode_supported;

	union {
		/** Items valid only for non-leaf nodes. */
		struct {
			/** Maximum number of children nodes. */
			uint32_t sched_n_children_max;

			/** Maximum number of supported priority levels. The
			 * value of zero is invalid. The value of 1 indicates
			 * that only priority 0 is supported, which essentially
			 * means that Strict Priority (SP) algorithm is not
			 * supported.
			 */
			uint32_t sched_sp_n_priorities_max;

			/** Maximum number of sibling nodes that can have the
			 * same priority at any given time, i.e. maximum size
			 * of the WFQ sibling node group. The value of zero
			 * is invalid. The value of 1 indicates that WFQ
			 * algorithm is not supported. The maximum value is
			 * *sched_n_children_max*.
			 */
			uint32_t sched_wfq_n_children_per_group_max;

			/** Maximum number of priority levels that can have
			 * more than one child node at any given time, i.e.
			 * maximum number of WFQ sibling node groups that have
			 * two or more members. The value of zero states that
			 * WFQ algorithm is not supported. The value of 1
			 * indicates that (*sched_sp_n_priorities_max* - 1)
			 * priority levels have at most one child node, so there
			 * can be only one priority level with two or more
			 * sibling nodes making up a WFQ group. The maximum
			 * value is: min(floor(*sched_n_children_max* / 2),
			 * *sched_sp_n_priorities_max*).
			 */
			uint32_t sched_wfq_n_groups_max;

			/** Maximum WFQ weight. The value of 1 indicates that
			 * all sibling nodes with same priority have the same
			 * WFQ weight, so WFQ is reduced to FQ.
			 */
			uint32_t sched_wfq_weight_max;

			/** WFQ packet mode supported. When non-zero, this
			 * parameter indicates that current node supports packet
			 * mode for WFQ among its children. WFQ weights will be
			 * applied against packet count for scheduling children
			 * when configured appropriately.
			 */
			int sched_wfq_packet_mode_supported;

			/** WFQ byte mode supported. When non-zero, this
			 * parameter indicates that current node supports byte
			 * mode for WFQ among its children. WFQ weights will be
			 * applied against  bytes for scheduling children when
			 * configured appropriately.
			 */
			int sched_wfq_byte_mode_supported;

		} nonleaf;

		/** Items valid only for leaf nodes. */
		struct {
			/** WRED packet mode support for current node. */
			int cman_wred_packet_mode_supported;

			/** WRED byte mode support for current node. */
			int cman_wred_byte_mode_supported;

			/** Head drop algorithm support for current node. */
			int cman_head_drop_supported;

			/** Private WRED context support for current node. */
			int cman_wred_context_private_supported;

			/** Maximum number of shared WRED contexts the current
			 * node can be part of. The value of zero indicates that
			 * shared WRED contexts are not supported by the current
			 * node.
			 */
			uint32_t cman_wred_context_shared_n_max;
		} leaf;
	};

	/** Mask of statistics counter types supported by the current node.
	 * @see enum rte_tm_stats_type
	 */
	uint64_t stats_mask;
};

/**
 * Congestion management (CMAN) mode
 *
 * This is used for controlling the admission of packets into a packet queue or
 * group of packet queues on congestion. On request of writing a new packet
 * into the current queue while the queue is full, the *tail drop* algorithm
 * drops the new packet while leaving the queue unmodified, as opposed to *head
 * drop* algorithm, which drops the packet at the head of the queue (the oldest
 * packet waiting in the queue) and admits the new packet at the tail of the
 * queue.
 *
 * The *Random Early Detection (RED)* algorithm works by proactively dropping
 * more and more input packets as the queue occupancy builds up. When the queue
 * is full or almost full, RED effectively works as *tail drop*. The *Weighted
 * RED* algorithm uses a separate set of RED thresholds for each packet color.
 */
enum rte_tm_cman_mode {
	RTE_TM_CMAN_TAIL_DROP = 0, /**< Tail drop */
	RTE_TM_CMAN_HEAD_DROP, /**< Head drop */
	RTE_TM_CMAN_WRED, /**< Weighted Random Early Detection (WRED) */
};

/**
 * Random Early Detection (RED) profile
 */
struct rte_tm_red_params {
	/** Minimum queue threshold */
	uint64_t min_th;

	/** Maximum queue threshold */
	uint64_t max_th;

	/** Inverse of packet marking probability maximum value (maxp), i.e.
	 * maxp_inv = 1 / maxp
	 */
	uint16_t maxp_inv;

	/** Negated log2 of queue weight (wq), i.e. wq = 1 / (2 ^ wq_log2) */
	uint16_t wq_log2;
};

/**
 * Weighted RED (WRED) profile
 *
 * Multiple WRED contexts can share the same WRED profile. Each leaf node with
 * WRED enabled as its congestion management mode has zero or one private WRED
 * context (only one leaf node using it) and/or zero, one or several shared
 * WRED contexts (multiple leaf nodes use the same WRED context). A private
 * WRED context is used to perform congestion management for a single leaf
 * node, while a shared WRED context is used to perform congestion management
 * for a group of leaf nodes.
 *
 * @see struct rte_tm_capabilities::cman_wred_packet_mode_supported
 * @see struct rte_tm_capabilities::cman_wred_byte_mode_supported
 */
struct rte_tm_wred_params {
	/** One set of RED parameters per packet color */
	struct rte_tm_red_params red_params[RTE_COLORS];

	/** When non-zero, the *min_th* and *max_th* thresholds are specified
	 * in packets (WRED packet mode). When zero, the *min_th* and *max_th*
	 * thresholds are specified in bytes (WRED byte mode)
	 */
	int packet_mode;
};

/**
 * Token bucket
 */
struct rte_tm_token_bucket {
	/** Token bucket rate (bytes per second or packets per second) */
	uint64_t rate;

	/** Token bucket size (bytes or packets), a.k.a. max burst size */
	uint64_t size;
};

/**
 * Shaper (rate limiter) profile
 *
 * Multiple shaper instances can share the same shaper profile. Each node has
 * zero or one private shaper (only one node using it) and/or zero, one or
 * several shared shapers (multiple nodes use the same shaper instance).
 * A private shaper is used to perform traffic shaping for a single node, while
 * a shared shaper is used to perform traffic shaping for a group of nodes.
 *
 * Single rate shapers use a single token bucket. A single rate shaper can be
 * configured by setting the rate of the committed bucket to zero, which
 * effectively disables this bucket. The peak bucket is used to limit the rate
 * and the burst size for the current shaper.
 *
 * Dual rate shapers use both the committed and the peak token buckets. The
 * rate of the peak bucket has to be bigger than zero, as well as greater than
 * or equal to the rate of the committed bucket.
 *
 * @see struct rte_tm_capabilities::shaper_private_packet_mode_supported
 * @see struct rte_tm_capabilities::shaper_private_byte_mode_supported
 * @see struct rte_tm_capabilities::shaper_shared_packet_mode_supported
 * @see struct rte_tm_capabilities::shaper_shared_byte_mode_supported
 */
struct rte_tm_shaper_params {
	/** Committed token bucket */
	struct rte_tm_token_bucket committed;

	/** Peak token bucket */
	struct rte_tm_token_bucket peak;

	/** Signed value to be added to the length of each packet for the
	 * purpose of shaping. Can be used to correct the packet length with
	 * the framing overhead bytes that are also consumed on the wire (e.g.
	 * RTE_TM_ETH_FRAMING_OVERHEAD_FCS).
	 * This field is ignored when the profile enables packet mode.
	 */
	int32_t pkt_length_adjust;

	/** When zero, the byte mode is enabled for the current profile, so the
	 * *rate* and *size* fields in both the committed and peak token buckets
	 * are specified in bytes per second and bytes, respectively.
	 * When non-zero, the packet mode is enabled for the current profile,
	 * so the *rate* and *size* fields in both the committed and peak token
	 * buckets are specified in packets per second and packets,
	 * respectively.
	 */
	int packet_mode;
};

/**
 * Node parameters
 *
 * Each non-leaf node has multiple inputs (its children nodes) and single output
 * (which is input to its parent node). It arbitrates its inputs using Strict
 * Priority (SP) and Weighted Fair Queuing (WFQ) algorithms to schedule input
 * packets to its output while observing its shaping (rate limiting)
 * constraints.
 *
 * Algorithms such as Weighted Round Robin (WRR), Byte-level WRR, Deficit WRR
 * (DWRR), etc. are considered approximations of the WFQ ideal and are
 * assimilated to WFQ, although an associated implementation-dependent trade-off
 * on accuracy, performance and resource usage might exist.
 *
 * Children nodes with different priorities are scheduled using the SP algorithm
 * based on their priority, with zero (0) as the highest priority. Children with
 * the same priority are scheduled using the WFQ algorithm according to their
 * weights. The WFQ weight of a given child node is relative to the sum of the
 * weights of all its sibling nodes that have the same priority, with one (1) as
 * the lowest weight. For each SP priority, the WFQ weight mode can be set as
 * either byte-based or packet-based.
 *
 * Each leaf node sits on top of a Tx queue of the current Ethernet port. Hence,
 * the leaf nodes are predefined, with their node IDs set to 0 .. (N-1), where N
 * is the number of Tx queues configured for the current Ethernet port. The
 * non-leaf nodes have their IDs generated by the application.
 */
struct rte_tm_node_params {
	/** Shaper profile for the private shaper. The absence of the private
	 * shaper for the current node is indicated by setting this parameter
	 * to RTE_TM_SHAPER_PROFILE_ID_NONE.
	 */
	uint32_t shaper_profile_id;

	/** User allocated array of valid shared shaper IDs. */
	uint32_t *shared_shaper_id;

	/** Number of shared shaper IDs in the *shared_shaper_id* array. */
	uint32_t n_shared_shapers;

	union {
		/** Parameters only valid for non-leaf nodes. */
		struct {
			/** WFQ weight mode for each SP priority. When NULL, it
			 * indicates that WFQ is to be used for all priorities.
			 * When non-NULL, it points to a pre-allocated array of
			 * *n_sp_priorities* values, with non-zero value for
			 * byte-mode and zero for packet-mode.
			 * @see struct rte_tm_node_capabilities::sched_wfq_packet_mode_supported
			 * @see struct rte_tm_node_capabilities::sched_wfq_byte_mode_supported
			 */
			int *wfq_weight_mode;

			/** Number of SP priorities. */
			uint32_t n_sp_priorities;
		} nonleaf;

		/** Parameters only valid for leaf nodes. */
		struct {
			/** Congestion management mode */
			enum rte_tm_cman_mode cman;

			/** WRED parameters (only valid when *cman* is set to
			 * WRED).
			 */
			struct {
				/** WRED profile for private WRED context. The
				 * absence of a private WRED context for the
				 * current leaf node is indicated by value
				 * RTE_TM_WRED_PROFILE_ID_NONE.
				 */
				uint32_t wred_profile_id;

				/** User allocated array of shared WRED context
				 * IDs. When set to NULL, it indicates that the
				 * current leaf node should not currently be
				 * part of any shared WRED contexts.
				 */
				uint32_t *shared_wred_context_id;

				/** Number of elements in the
				 * *shared_wred_context_id* array. Only valid
				 * when *shared_wred_context_id* is non-NULL,
				 * in which case it should be non-zero.
				 */
				uint32_t n_shared_wred_contexts;
			} wred;
		} leaf;
	};

	/** Mask of statistics counter types to be enabled for this node. This
	 * needs to be a subset of the statistics counter types available for
	 * the current node. Any statistics counter type not included in this
	 * set is to be disabled for the current node.
	 * @see enum rte_tm_stats_type
	 */
	uint64_t stats_mask;
};

/**
 * Verbose error types.
 *
 * Most of them provide the type of the object referenced by struct
 * rte_tm_error::cause.
 */
enum rte_tm_error_type {
	RTE_TM_ERROR_TYPE_NONE, /**< No error. */
	RTE_TM_ERROR_TYPE_UNSPECIFIED, /**< Cause unspecified. */
	RTE_TM_ERROR_TYPE_CAPABILITIES,
	RTE_TM_ERROR_TYPE_LEVEL_ID,
	RTE_TM_ERROR_TYPE_WRED_PROFILE,
	RTE_TM_ERROR_TYPE_WRED_PROFILE_GREEN,
	RTE_TM_ERROR_TYPE_WRED_PROFILE_YELLOW,
	RTE_TM_ERROR_TYPE_WRED_PROFILE_RED,
	RTE_TM_ERROR_TYPE_WRED_PROFILE_ID,
	RTE_TM_ERROR_TYPE_SHARED_WRED_CONTEXT_ID,
	RTE_TM_ERROR_TYPE_SHAPER_PROFILE,
	RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_RATE,
	RTE_TM_ERROR_TYPE_SHAPER_PROFILE_COMMITTED_SIZE,
	RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_RATE,
	RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PEAK_SIZE,
	RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PKT_ADJUST_LEN,
	RTE_TM_ERROR_TYPE_SHAPER_PROFILE_PACKET_MODE,
	RTE_TM_ERROR_TYPE_SHAPER_PROFILE_ID,
	RTE_TM_ERROR_TYPE_SHARED_SHAPER_ID,
	RTE_TM_ERROR_TYPE_NODE_PARENT_NODE_ID,
	RTE_TM_ERROR_TYPE_NODE_PRIORITY,
	RTE_TM_ERROR_TYPE_NODE_WEIGHT,
	RTE_TM_ERROR_TYPE_NODE_PARAMS,
	RTE_TM_ERROR_TYPE_NODE_PARAMS_SHAPER_PROFILE_ID,
	RTE_TM_ERROR_TYPE_NODE_PARAMS_SHARED_SHAPER_ID,
	RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_SHAPERS,
	RTE_TM_ERROR_TYPE_NODE_PARAMS_WFQ_WEIGHT_MODE,
	RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SP_PRIORITIES,
	RTE_TM_ERROR_TYPE_NODE_PARAMS_CMAN,
	RTE_TM_ERROR_TYPE_NODE_PARAMS_WRED_PROFILE_ID,
	RTE_TM_ERROR_TYPE_NODE_PARAMS_SHARED_WRED_CONTEXT_ID,
	RTE_TM_ERROR_TYPE_NODE_PARAMS_N_SHARED_WRED_CONTEXTS,
	RTE_TM_ERROR_TYPE_NODE_PARAMS_STATS,
	RTE_TM_ERROR_TYPE_NODE_ID,
};

/**
 * Verbose error structure definition.
 *
 * This object is normally allocated by applications and set by PMDs, the
 * message points to a constant string which does not need to be freed by
 * the application, however its pointer can be considered valid only as long
 * as its associated DPDK port remains configured. Closing the underlying
 * device or unloading the PMD invalidates it.
 *
 * Both cause and message may be NULL regardless of the error type.
 */
struct rte_tm_error {
	enum rte_tm_error_type type; /**< Cause field and error type. */
	const void *cause; /**< Object responsible for the error. */
	const char *message; /**< Human-readable error message. */
};

/**
 * Traffic manager get number of leaf nodes
 *
 * Each leaf node sits on top of a Tx queue of the current Ethernet port.
 * Therefore, the set of leaf nodes is predefined, their number is always equal
 * to N (where N is the number of Tx queues configured for the current port)
 * and their IDs are 0 .. (N-1).
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[out] n_leaf_nodes
 *   Number of leaf nodes for the current port.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_tm_get_number_of_leaf_nodes(uint16_t port_id,
	uint32_t *n_leaf_nodes,
	struct rte_tm_error *error);

/**
 * Traffic manager node ID validate and type (i.e. leaf or non-leaf) get
 *
 * The leaf nodes have predefined IDs in the range of 0 .. (N-1), where N is
 * the number of Tx queues of the current Ethernet port. The non-leaf nodes
 * have their IDs generated by the application outside of the above range,
 * which is reserved for leaf nodes.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID value. Needs to be valid.
 * @param[out] is_leaf
 *   Set to non-zero value when node is leaf and to zero otherwise (non-leaf).
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_tm_node_type_get(uint16_t port_id,
	uint32_t node_id,
	int *is_leaf,
	struct rte_tm_error *error);

/**
 * Traffic manager capabilities get
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[out] cap
 *   Traffic manager capabilities. Needs to be pre-allocated and valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_tm_capabilities_get(uint16_t port_id,
	struct rte_tm_capabilities *cap,
	struct rte_tm_error *error);

/**
 * Traffic manager level capabilities get
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] level_id
 *   The hierarchy level identifier. The value of 0 identifies the level of the
 *   root node.
 * @param[out] cap
 *   Traffic manager level capabilities. Needs to be pre-allocated and valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_tm_level_capabilities_get(uint16_t port_id,
	uint32_t level_id,
	struct rte_tm_level_capabilities *cap,
	struct rte_tm_error *error);

/**
 * Traffic manager node capabilities get
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid.
 * @param[out] cap
 *   Traffic manager node capabilities. Needs to be pre-allocated and valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_tm_node_capabilities_get(uint16_t port_id,
	uint32_t node_id,
	struct rte_tm_node_capabilities *cap,
	struct rte_tm_error *error);

/**
 * Traffic manager WRED profile add
 *
 * Create a new WRED profile with ID set to *wred_profile_id*. The new profile
 * is used to create one or several WRED contexts.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] wred_profile_id
 *   WRED profile ID for the new profile. Needs to be unused.
 * @param[in] profile
 *   WRED profile parameters. Needs to be pre-allocated and valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::cman_wred_context_n_max
 */
__rte_experimental
int
rte_tm_wred_profile_add(uint16_t port_id,
	uint32_t wred_profile_id,
	struct rte_tm_wred_params *profile,
	struct rte_tm_error *error);

/**
 * Traffic manager WRED profile delete
 *
 * Delete an existing WRED profile. This operation fails when there is
 * currently at least one user (i.e. WRED context) of this WRED profile.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] wred_profile_id
 *   WRED profile ID. Needs to be the valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::cman_wred_context_n_max
 */
__rte_experimental
int
rte_tm_wred_profile_delete(uint16_t port_id,
	uint32_t wred_profile_id,
	struct rte_tm_error *error);

/**
 * Traffic manager shared WRED context add or update
 *
 * When *shared_wred_context_id* is invalid, a new WRED context with this ID is
 * created by using the WRED profile identified by *wred_profile_id*.
 *
 * When *shared_wred_context_id* is valid, this WRED context is no longer using
 * the profile previously assigned to it and is updated to use the profile
 * identified by *wred_profile_id*.
 *
 * A valid shared WRED context can be assigned to several hierarchy leaf nodes
 * configured to use WRED as the congestion management mode.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] shared_wred_context_id
 *   Shared WRED context ID
 * @param[in] wred_profile_id
 *   WRED profile ID. Needs to be the valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::cman_wred_context_shared_n_max
 */
__rte_experimental
int
rte_tm_shared_wred_context_add_update(uint16_t port_id,
	uint32_t shared_wred_context_id,
	uint32_t wred_profile_id,
	struct rte_tm_error *error);

/**
 * Traffic manager shared WRED context delete
 *
 * Delete an existing shared WRED context. This operation fails when there is
 * currently at least one user (i.e. hierarchy leaf node) of this shared WRED
 * context.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] shared_wred_context_id
 *   Shared WRED context ID. Needs to be the valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::cman_wred_context_shared_n_max
 */
__rte_experimental
int
rte_tm_shared_wred_context_delete(uint16_t port_id,
	uint32_t shared_wred_context_id,
	struct rte_tm_error *error);

/**
 * Traffic manager shaper profile add
 *
 * Create a new shaper profile with ID set to *shaper_profile_id*. The new
 * shaper profile is used to create one or several shapers.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] shaper_profile_id
 *   Shaper profile ID for the new profile. Needs to be unused.
 * @param[in] profile
 *   Shaper profile parameters. Needs to be pre-allocated and valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::shaper_n_max
 */
__rte_experimental
int
rte_tm_shaper_profile_add(uint16_t port_id,
	uint32_t shaper_profile_id,
	struct rte_tm_shaper_params *profile,
	struct rte_tm_error *error);

/**
 * Traffic manager shaper profile delete
 *
 * Delete an existing shaper profile. This operation fails when there is
 * currently at least one user (i.e. shaper) of this shaper profile.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] shaper_profile_id
 *   Shaper profile ID. Needs to be the valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::shaper_n_max
 */
__rte_experimental
int
rte_tm_shaper_profile_delete(uint16_t port_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error);

/**
 * Traffic manager shared shaper add or update
 *
 * When *shared_shaper_id* is not a valid shared shaper ID, a new shared shaper
 * with this ID is created using the shaper profile identified by
 * *shaper_profile_id*.
 *
 * When *shared_shaper_id* is a valid shared shaper ID, this shared shaper is
 * no longer using the shaper profile previously assigned to it and is updated
 * to use the shaper profile identified by *shaper_profile_id*.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] shared_shaper_id
 *   Shared shaper ID
 * @param[in] shaper_profile_id
 *   Shaper profile ID. Needs to be the valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::shaper_shared_n_max
 */
__rte_experimental
int
rte_tm_shared_shaper_add_update(uint16_t port_id,
	uint32_t shared_shaper_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error);

/**
 * Traffic manager shared shaper delete
 *
 * Delete an existing shared shaper. This operation fails when there is
 * currently at least one user (i.e. hierarchy node) of this shared shaper.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] shared_shaper_id
 *   Shared shaper ID. Needs to be the valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::shaper_shared_n_max
 */
__rte_experimental
int
rte_tm_shared_shaper_delete(uint16_t port_id,
	uint32_t shared_shaper_id,
	struct rte_tm_error *error);

/**
 * Traffic manager node add
 *
 * Create new node and connect it as child of an existing node. The new node is
 * further identified by *node_id*, which needs to be unused by any of the
 * existing nodes. The parent node is identified by *parent_node_id*, which
 * needs to be the valid ID of an existing non-leaf node. The parent node is
 * going to use the provided SP *priority* and WFQ *weight* to schedule its new
 * child node.
 *
 * This function has to be called for both leaf and non-leaf nodes. In the case
 * of leaf nodes (i.e. *node_id* is within the range of 0 .. (N-1), with N as
 * the number of configured Tx queues of the current port), the leaf node is
 * configured rather than created (as the set of leaf nodes is predefined) and
 * it is also connected as child of an existing node.
 *
 * The first node that is added becomes the root node and all the nodes that
 * are subsequently added have to be added as descendants of the root node. The
 * parent of the root node has to be specified as RTE_TM_NODE_ID_NULL and there
 * can only be one node with this parent ID (i.e. the root node). Further
 * restrictions for root node: needs to be non-leaf, its private shaper profile
 * needs to be valid and single rate, cannot use any shared shapers.
 *
 * When called before rte_tm_hierarchy_commit() invocation, this function is
 * typically used to define the initial start-up hierarchy for the port.
 * Provided that dynamic hierarchy updates are supported by the current port (as
 * advertised in the port capability set), this function can be also called
 * after the rte_tm_hierarchy_commit() invocation.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be unused by any of the existing nodes.
 * @param[in] parent_node_id
 *   Parent node ID. Needs to be the valid.
 * @param[in] priority
 *   Node priority. The highest node priority is zero. Used by the SP algorithm
 *   running on the parent of the current node for scheduling this child node.
 * @param[in] weight
 *   Node weight. The node weight is relative to the weight sum of all siblings
 *   that have the same priority. The lowest weight is one. Used by the WFQ
 *   algorithm running on the parent of the current node for scheduling this
 *   child node.
 * @param[in] level_id
 *   Level ID that should be met by this node. The hierarchy level of the
 *   current node is already fully specified through its parent node (i.e. the
 *   level of this node is equal to the level of its parent node plus one),
 *   therefore the reason for providing this parameter is to enable the
 *   application to perform step-by-step checking of the node level during
 *   successive invocations of this function. When not desired, this check can
 *   be disabled by assigning value RTE_TM_NODE_LEVEL_ID_ANY to this parameter.
 * @param[in] params
 *   Node parameters. Needs to be pre-allocated and valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see rte_tm_hierarchy_commit()
 * @see RTE_TM_UPDATE_NODE_ADD_DELETE
 * @see RTE_TM_NODE_LEVEL_ID_ANY
 * @see struct rte_tm_capabilities
 */
__rte_experimental
int
rte_tm_node_add(uint16_t port_id,
	uint32_t node_id,
	uint32_t parent_node_id,
	uint32_t priority,
	uint32_t weight,
	uint32_t level_id,
	struct rte_tm_node_params *params,
	struct rte_tm_error *error);

/**
 * Traffic manager node delete
 *
 * Delete an existing node. This operation fails when this node currently has
 * at least one user (i.e. child node).
 *
 * When called before rte_tm_hierarchy_commit() invocation, this function is
 * typically used to define the initial start-up hierarchy for the port.
 * Provided that dynamic hierarchy updates are supported by the current port (as
 * advertised in the port capability set), this function can be also called
 * after the rte_tm_hierarchy_commit() invocation.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see RTE_TM_UPDATE_NODE_ADD_DELETE
 */
__rte_experimental
int
rte_tm_node_delete(uint16_t port_id,
	uint32_t node_id,
	struct rte_tm_error *error);

/**
 * Traffic manager node suspend
 *
 * Suspend an existing node. While the node is in suspended state, no packet is
 * scheduled from this node and its descendants. The node exits the suspended
 * state through the node resume operation.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see rte_tm_node_resume()
 * @see RTE_TM_UPDATE_NODE_SUSPEND_RESUME
 */
__rte_experimental
int
rte_tm_node_suspend(uint16_t port_id,
	uint32_t node_id,
	struct rte_tm_error *error);

/**
 * Traffic manager node resume
 *
 * Resume an existing node that is currently in suspended state. The node
 * entered the suspended state as result of a previous node suspend operation.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see rte_tm_node_suspend()
 * @see RTE_TM_UPDATE_NODE_SUSPEND_RESUME
 */
__rte_experimental
int
rte_tm_node_resume(uint16_t port_id,
	uint32_t node_id,
	struct rte_tm_error *error);

/**
 * Traffic manager hierarchy commit
 *
 * This function is called during the port initialization phase (before the
 * Ethernet port is started) to freeze the start-up hierarchy.
 *
 * This function typically performs the following steps:
 *    a) It validates the start-up hierarchy that was previously defined for the
 *       current port through successive rte_tm_node_add() invocations;
 *    b) Assuming successful validation, it performs all the necessary port
 *       specific configuration operations to install the specified hierarchy on
 *       the current port, with immediate effect once the port is started.
 *
 * This function fails when the currently configured hierarchy is not supported
 * by the Ethernet port, in which case the user can abort or try out another
 * hierarchy configuration (e.g. a hierarchy with less leaf nodes), which can be
 * build from scratch (when *clear_on_fail* is enabled) or by modifying the
 * existing hierarchy configuration (when *clear_on_fail* is disabled).
 *
 * Note that this function can still fail due to other causes (e.g. not enough
 * memory available in the system, etc), even though the specified hierarchy is
 * supported in principle by the current port.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] clear_on_fail
 *   On function call failure, hierarchy is cleared when this parameter is
 *   non-zero and preserved when this parameter is equal to zero.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see rte_tm_node_add()
 * @see rte_tm_node_delete()
 */
__rte_experimental
int
rte_tm_hierarchy_commit(uint16_t port_id,
	int clear_on_fail,
	struct rte_tm_error *error);

/**
 * Traffic manager node parent update
 *
 * This function may be used to move a node and its children to a different
 * parent.  Additionally, if the new parent is the same as the current parent,
 * this function will update the priority/weight of an existing node.
 *
 * Restriction for root node: its parent cannot be changed.
 *
 * This function can only be called after the rte_tm_hierarchy_commit()
 * invocation. Its success depends on the port support for this operation, as
 * advertised through the port capability set.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid.
 * @param[in] parent_node_id
 *   Node ID for the new parent. Needs to be valid.
 * @param[in] priority
 *   Node priority. The highest node priority is zero. Used by the SP algorithm
 *   running on the parent of the current node for scheduling this child node.
 * @param[in] weight
 *   Node weight. The node weight is relative to the weight sum of all siblings
 *   that have the same priority. The lowest weight is zero. Used by the WFQ
 *   algorithm running on the parent of the current node for scheduling this
 *   child node.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see RTE_TM_UPDATE_NODE_PARENT_KEEP_LEVEL
 * @see RTE_TM_UPDATE_NODE_PARENT_CHANGE_LEVEL
 */
__rte_experimental
int
rte_tm_node_parent_update(uint16_t port_id,
	uint32_t node_id,
	uint32_t parent_node_id,
	uint32_t priority,
	uint32_t weight,
	struct rte_tm_error *error);

/**
 * Traffic manager node private shaper update
 *
 * Restriction for the root node: its private shaper profile needs to be valid
 * and single rate.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid.
 * @param[in] shaper_profile_id
 *   Shaper profile ID for the private shaper of the current node. Needs to be
 *   either valid shaper profile ID or RTE_TM_SHAPER_PROFILE_ID_NONE, with
 *   the latter disabling the private shaper of the current node.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::shaper_private_n_max
 */
__rte_experimental
int
rte_tm_node_shaper_update(uint16_t port_id,
	uint32_t node_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error);

/**
 * Traffic manager node shared shapers update
 *
 * Restriction for root node: cannot use any shared rate shapers.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid.
 * @param[in] shared_shaper_id
 *   Shared shaper ID. Needs to be valid.
 * @param[in] add
 *   Set to non-zero value to add this shared shaper to current node or to zero
 *   to delete this shared shaper from current node.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::shaper_shared_n_max
 */
__rte_experimental
int
rte_tm_node_shared_shaper_update(uint16_t port_id,
	uint32_t node_id,
	uint32_t shared_shaper_id,
	int add,
	struct rte_tm_error *error);

/**
 * Traffic manager node enabled statistics counters update
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid.
 * @param[in] stats_mask
 *   Mask of statistics counter types to be enabled for the current node. This
 *   needs to be a subset of the statistics counter types available for the
 *   current node. Any statistics counter type not included in this set is to
 *   be disabled for the current node.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see enum rte_tm_stats_type
 * @see RTE_TM_UPDATE_NODE_STATS
 */
__rte_experimental
int
rte_tm_node_stats_update(uint16_t port_id,
	uint32_t node_id,
	uint64_t stats_mask,
	struct rte_tm_error *error);

/**
 * Traffic manager node WFQ weight mode update
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid non-leaf node ID.
 * @param[in] wfq_weight_mode
 *   WFQ weight mode for each SP priority. When NULL, it indicates that WFQ is
 *   to be used for all priorities. When non-NULL, it points to a pre-allocated
 *   array of *n_sp_priorities* values, with non-zero value for byte-mode and
 *   zero for packet-mode.
 * @param[in] n_sp_priorities
 *   Number of SP priorities.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see RTE_TM_UPDATE_NODE_WFQ_WEIGHT_MODE
 * @see RTE_TM_UPDATE_NODE_N_SP_PRIORITIES
 */
__rte_experimental
int
rte_tm_node_wfq_weight_mode_update(uint16_t port_id,
	uint32_t node_id,
	int *wfq_weight_mode,
	uint32_t n_sp_priorities,
	struct rte_tm_error *error);

/**
 * Traffic manager node congestion management mode update
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid leaf node ID.
 * @param[in] cman
 *   Congestion management mode.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see RTE_TM_UPDATE_NODE_CMAN
 */
__rte_experimental
int
rte_tm_node_cman_update(uint16_t port_id,
	uint32_t node_id,
	enum rte_tm_cman_mode cman,
	struct rte_tm_error *error);

/**
 * Traffic manager node private WRED context update
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid leaf node ID.
 * @param[in] wred_profile_id
 *   WRED profile ID for the private WRED context of the current node. Needs to
 *   be either valid WRED profile ID or RTE_TM_WRED_PROFILE_ID_NONE, with the
 *   latter disabling the private WRED context of the current node.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::cman_wred_context_private_n_max
 */
__rte_experimental
int
rte_tm_node_wred_context_update(uint16_t port_id,
	uint32_t node_id,
	uint32_t wred_profile_id,
	struct rte_tm_error *error);

/**
 * Traffic manager node shared WRED context update
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid leaf node ID.
 * @param[in] shared_wred_context_id
 *   Shared WRED context ID. Needs to be valid.
 * @param[in] add
 *   Set to non-zero value to add this shared WRED context to current node or
 *   to zero to delete this shared WRED context from current node.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::cman_wred_context_shared_n_max
 */
__rte_experimental
int
rte_tm_node_shared_wred_context_update(uint16_t port_id,
	uint32_t node_id,
	uint32_t shared_wred_context_id,
	int add,
	struct rte_tm_error *error);

/**
 * Traffic manager node statistics counters read
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] node_id
 *   Node ID. Needs to be valid.
 * @param[out] stats
 *   When non-NULL, it contains the current value for the statistics counters
 *   enabled for the current node.
 * @param[out] stats_mask
 *   When non-NULL, it contains the mask of statistics counter types that are
 *   currently enabled for this node, indicating which of the counters
 *   retrieved with the *stats* structure are valid.
 * @param[in] clear
 *   When this parameter has a non-zero value, the statistics counters are
 *   cleared (i.e. set to zero) immediately after they have been read,
 *   otherwise the statistics counters are left untouched.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see enum rte_tm_stats_type
 */
__rte_experimental
int
rte_tm_node_stats_read(uint16_t port_id,
	uint32_t node_id,
	struct rte_tm_node_stats *stats,
	uint64_t *stats_mask,
	int clear,
	struct rte_tm_error *error);

/**
 * Traffic manager packet marking - VLAN DEI (IEEE 802.1Q)
 *
 * IEEE 802.1p maps the traffic class to the VLAN Priority Code Point (PCP)
 * field (3 bits), while IEEE 802.1q maps the drop priority to the VLAN Drop
 * Eligible Indicator (DEI) field (1 bit), which was previously named Canonical
 * Format Indicator (CFI).
 *
 * All VLAN frames of a given color get their DEI bit set if marking is enabled
 * for this color; otherwise, their DEI bit is left as is (either set or not).
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mark_green
 *   Set to non-zero value to enable marking of green packets and to zero to
 *   disable it.
 * @param[in] mark_yellow
 *   Set to non-zero value to enable marking of yellow packets and to zero to
 *   disable it.
 * @param[in] mark_red
 *   Set to non-zero value to enable marking of red packets and to zero to
 *   disable it.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::mark_vlan_dei_supported
 */
__rte_experimental
int
rte_tm_mark_vlan_dei(uint16_t port_id,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error);

/**
 * Traffic manager packet marking - IPv4 / IPv6 ECN (IETF RFC 3168)
 *
 * IETF RFCs 2474 and 3168 reorganize the IPv4 Type of Service (TOS) field
 * (8 bits) and the IPv6 Traffic Class (TC) field (8 bits) into Differentiated
 * Services Codepoint (DSCP) field (6 bits) and Explicit Congestion
 * Notification (ECN) field (2 bits). The DSCP field is typically used to
 * encode the traffic class and/or drop priority (RFC 2597), while the ECN
 * field is used by RFC 3168 to implement a congestion notification mechanism
 * to be leveraged by transport layer protocols such as TCP and SCTP that have
 * congestion control mechanisms.
 *
 * When congestion is experienced, as alternative to dropping the packet,
 * routers can change the ECN field of input packets from 2'b01 or 2'b10
 * (values indicating that source endpoint is ECN-capable) to 2'b11 (meaning
 * that congestion is experienced). The destination endpoint can use the
 * ECN-Echo (ECE) TCP flag to relay the congestion indication back to the
 * source endpoint, which acknowledges it back to the destination endpoint with
 * the Congestion Window Reduced (CWR) TCP flag.
 *
 * All IPv4/IPv6 packets of a given color with ECN set to 2’b01 or 2’b10
 * carrying TCP or SCTP have their ECN set to 2’b11 if the marking feature is
 * enabled for the current color, otherwise the ECN field is left as is.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mark_green
 *   Set to non-zero value to enable marking of green packets and to zero to
 *   disable it.
 * @param[in] mark_yellow
 *   Set to non-zero value to enable marking of yellow packets and to zero to
 *   disable it.
 * @param[in] mark_red
 *   Set to non-zero value to enable marking of red packets and to zero to
 *   disable it.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::mark_ip_ecn_tcp_supported
 * @see struct rte_tm_capabilities::mark_ip_ecn_sctp_supported
 */
__rte_experimental
int
rte_tm_mark_ip_ecn(uint16_t port_id,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error);

/**
 * Traffic manager packet marking - IPv4 / IPv6 DSCP (IETF RFC 2597)
 *
 * IETF RFC 2597 maps the traffic class and the drop priority to the IPv4/IPv6
 * Differentiated Services Codepoint (DSCP) field (6 bits). Here are the DSCP
 * values proposed by this RFC:
 *
 * <pre>                   Class 1    Class 2    Class 3    Class 4   </pre>
 * <pre>                 +----------+----------+----------+----------+</pre>
 * <pre>Low Drop Prec    |  001010  |  010010  |  011010  |  100010  |</pre>
 * <pre>Medium Drop Prec |  001100  |  010100  |  011100  |  100100  |</pre>
 * <pre>High Drop Prec   |  001110  |  010110  |  011110  |  100110  |</pre>
 * <pre>                 +----------+----------+----------+----------+</pre>
 *
 * There are 4 traffic classes (classes 1 .. 4) encoded by DSCP bits 1 and 2,
 * as well as 3 drop priorities (low/medium/high) encoded by DSCP bits 3 and 4.
 *
 * All IPv4/IPv6 packets have their color marked into DSCP bits 3 and 4 as
 * follows: green mapped to Low Drop Precedence (2’b01), yellow to Medium
 * (2’b10) and red to High (2’b11). Marking needs to be explicitly enabled
 * for each color; when not enabled for a given color, the DSCP field of all
 * packets with that color is left as is.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] mark_green
 *   Set to non-zero value to enable marking of green packets and to zero to
 *   disable it.
 * @param[in] mark_yellow
 *   Set to non-zero value to enable marking of yellow packets and to zero to
 *   disable it.
 * @param[in] mark_red
 *   Set to non-zero value to enable marking of red packets and to zero to
 *   disable it.
 * @param[out] error
 *   Error details. Filled in only on error, when not NULL.
 * @return
 *   0 on success, non-zero error code otherwise.
 *
 * @see struct rte_tm_capabilities::mark_ip_dscp_supported
 */
__rte_experimental
int
rte_tm_mark_ip_dscp(uint16_t port_id,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_TM_H__ */
