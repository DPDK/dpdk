/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef __INCLUDE_RTE_TM_DRIVER_H__
#define __INCLUDE_RTE_TM_DRIVER_H__

/**
 * @file
 * RTE Generic Traffic Manager API (Driver Side)
 *
 * This file provides implementation helpers for internal use by PMDs, they
 * are not intended to be exposed to applications and are not subject to ABI
 * versioning.
 */

#include <stdint.h>

#include <rte_errno.h>
#include "rte_ethdev.h"
#include "rte_ethdev_driver.h"
#include "rte_tm.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @internal Traffic manager node ID validate and type get */
typedef int (*rte_tm_node_type_get_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	int *is_leaf,
	struct rte_tm_error *error);

/** @internal Traffic manager capabilities get */
typedef int (*rte_tm_capabilities_get_t)(struct rte_eth_dev *dev,
	struct rte_tm_capabilities *cap,
	struct rte_tm_error *error);

/** @internal Traffic manager level capabilities get */
typedef int (*rte_tm_level_capabilities_get_t)(struct rte_eth_dev *dev,
	uint32_t level_id,
	struct rte_tm_level_capabilities *cap,
	struct rte_tm_error *error);

/** @internal Traffic manager node capabilities get */
typedef int (*rte_tm_node_capabilities_get_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	struct rte_tm_node_capabilities *cap,
	struct rte_tm_error *error);

/** @internal Traffic manager WRED profile add */
typedef int (*rte_tm_wred_profile_add_t)(struct rte_eth_dev *dev,
	uint32_t wred_profile_id,
	struct rte_tm_wred_params *profile,
	struct rte_tm_error *error);

/** @internal Traffic manager WRED profile delete */
typedef int (*rte_tm_wred_profile_delete_t)(struct rte_eth_dev *dev,
	uint32_t wred_profile_id,
	struct rte_tm_error *error);

/** @internal Traffic manager shared WRED context add */
typedef int (*rte_tm_shared_wred_context_add_update_t)(
	struct rte_eth_dev *dev,
	uint32_t shared_wred_context_id,
	uint32_t wred_profile_id,
	struct rte_tm_error *error);

/** @internal Traffic manager shared WRED context delete */
typedef int (*rte_tm_shared_wred_context_delete_t)(
	struct rte_eth_dev *dev,
	uint32_t shared_wred_context_id,
	struct rte_tm_error *error);

/** @internal Traffic manager shaper profile add */
typedef int (*rte_tm_shaper_profile_add_t)(struct rte_eth_dev *dev,
	uint32_t shaper_profile_id,
	struct rte_tm_shaper_params *profile,
	struct rte_tm_error *error);

/** @internal Traffic manager shaper profile delete */
typedef int (*rte_tm_shaper_profile_delete_t)(struct rte_eth_dev *dev,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error);

/** @internal Traffic manager shared shaper add/update */
typedef int (*rte_tm_shared_shaper_add_update_t)(struct rte_eth_dev *dev,
	uint32_t shared_shaper_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error);

/** @internal Traffic manager shared shaper delete */
typedef int (*rte_tm_shared_shaper_delete_t)(struct rte_eth_dev *dev,
	uint32_t shared_shaper_id,
	struct rte_tm_error *error);

/** @internal Traffic manager node add */
typedef int (*rte_tm_node_add_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t parent_node_id,
	uint32_t priority,
	uint32_t weight,
	uint32_t level_id,
	struct rte_tm_node_params *params,
	struct rte_tm_error *error);

/** @internal Traffic manager node delete */
typedef int (*rte_tm_node_delete_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	struct rte_tm_error *error);

/** @internal Traffic manager node suspend */
typedef int (*rte_tm_node_suspend_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	struct rte_tm_error *error);

/** @internal Traffic manager node resume */
typedef int (*rte_tm_node_resume_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	struct rte_tm_error *error);

/** @internal Traffic manager hierarchy commit */
typedef int (*rte_tm_hierarchy_commit_t)(struct rte_eth_dev *dev,
	int clear_on_fail,
	struct rte_tm_error *error);

/** @internal Traffic manager node parent update */
typedef int (*rte_tm_node_parent_update_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t parent_node_id,
	uint32_t priority,
	uint32_t weight,
	struct rte_tm_error *error);

/** @internal Traffic manager node shaper update */
typedef int (*rte_tm_node_shaper_update_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t shaper_profile_id,
	struct rte_tm_error *error);

/** @internal Traffic manager node shaper update */
typedef int (*rte_tm_node_shared_shaper_update_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t shared_shaper_id,
	int32_t add,
	struct rte_tm_error *error);

/** @internal Traffic manager node stats update */
typedef int (*rte_tm_node_stats_update_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	uint64_t stats_mask,
	struct rte_tm_error *error);

/** @internal Traffic manager node WFQ weight mode update */
typedef int (*rte_tm_node_wfq_weight_mode_update_t)(
	struct rte_eth_dev *dev,
	uint32_t node_id,
	int *wfq_weight_mode,
	uint32_t n_sp_priorities,
	struct rte_tm_error *error);

/** @internal Traffic manager node congestion management mode update */
typedef int (*rte_tm_node_cman_update_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	enum rte_tm_cman_mode cman,
	struct rte_tm_error *error);

/** @internal Traffic manager node WRED context update */
typedef int (*rte_tm_node_wred_context_update_t)(
	struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t wred_profile_id,
	struct rte_tm_error *error);

/** @internal Traffic manager node WRED context update */
typedef int (*rte_tm_node_shared_wred_context_update_t)(
	struct rte_eth_dev *dev,
	uint32_t node_id,
	uint32_t shared_wred_context_id,
	int add,
	struct rte_tm_error *error);

/** @internal Traffic manager read stats counters for specific node */
typedef int (*rte_tm_node_stats_read_t)(struct rte_eth_dev *dev,
	uint32_t node_id,
	struct rte_tm_node_stats *stats,
	uint64_t *stats_mask,
	int clear,
	struct rte_tm_error *error);

/** @internal Traffic manager packet marking - VLAN DEI */
typedef int (*rte_tm_mark_vlan_dei_t)(struct rte_eth_dev *dev,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error);

/** @internal Traffic manager packet marking - IPv4/IPv6 ECN */
typedef int (*rte_tm_mark_ip_ecn_t)(struct rte_eth_dev *dev,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error);

/** @internal Traffic manager packet marking - IPv4/IPv6 DSCP */
typedef int (*rte_tm_mark_ip_dscp_t)(struct rte_eth_dev *dev,
	int mark_green,
	int mark_yellow,
	int mark_red,
	struct rte_tm_error *error);

struct rte_tm_ops {
	/** Traffic manager node type get */
	rte_tm_node_type_get_t node_type_get;

	/** Traffic manager capabilities_get */
	rte_tm_capabilities_get_t capabilities_get;
	/** Traffic manager level capabilities_get */
	rte_tm_level_capabilities_get_t level_capabilities_get;
	/** Traffic manager node capabilities get */
	rte_tm_node_capabilities_get_t node_capabilities_get;

	/** Traffic manager WRED profile add */
	rte_tm_wred_profile_add_t wred_profile_add;
	/** Traffic manager WRED profile delete */
	rte_tm_wred_profile_delete_t wred_profile_delete;
	/** Traffic manager shared WRED context add/update */
	rte_tm_shared_wred_context_add_update_t
		shared_wred_context_add_update;
	/** Traffic manager shared WRED context delete */
	rte_tm_shared_wred_context_delete_t
		shared_wred_context_delete;

	/** Traffic manager shaper profile add */
	rte_tm_shaper_profile_add_t shaper_profile_add;
	/** Traffic manager shaper profile delete */
	rte_tm_shaper_profile_delete_t shaper_profile_delete;
	/** Traffic manager shared shaper add/update */
	rte_tm_shared_shaper_add_update_t shared_shaper_add_update;
	/** Traffic manager shared shaper delete */
	rte_tm_shared_shaper_delete_t shared_shaper_delete;

	/** Traffic manager node add */
	rte_tm_node_add_t node_add;
	/** Traffic manager node delete */
	rte_tm_node_delete_t node_delete;
	/** Traffic manager node suspend */
	rte_tm_node_suspend_t node_suspend;
	/** Traffic manager node resume */
	rte_tm_node_resume_t node_resume;
	/** Traffic manager hierarchy commit */
	rte_tm_hierarchy_commit_t hierarchy_commit;

	/** Traffic manager node parent update */
	rte_tm_node_parent_update_t node_parent_update;
	/** Traffic manager node shaper update */
	rte_tm_node_shaper_update_t node_shaper_update;
	/** Traffic manager node shared shaper update */
	rte_tm_node_shared_shaper_update_t node_shared_shaper_update;
	/** Traffic manager node stats update */
	rte_tm_node_stats_update_t node_stats_update;
	/** Traffic manager node WFQ weight mode update */
	rte_tm_node_wfq_weight_mode_update_t node_wfq_weight_mode_update;
	/** Traffic manager node congestion management mode update */
	rte_tm_node_cman_update_t node_cman_update;
	/** Traffic manager node WRED context update */
	rte_tm_node_wred_context_update_t node_wred_context_update;
	/** Traffic manager node shared WRED context update */
	rte_tm_node_shared_wred_context_update_t
		node_shared_wred_context_update;
	/** Traffic manager read statistics counters for current node */
	rte_tm_node_stats_read_t node_stats_read;

	/** Traffic manager packet marking - VLAN DEI */
	rte_tm_mark_vlan_dei_t mark_vlan_dei;
	/** Traffic manager packet marking - IPv4/IPv6 ECN */
	rte_tm_mark_ip_ecn_t mark_ip_ecn;
	/** Traffic manager packet marking - IPv4/IPv6 DSCP */
	rte_tm_mark_ip_dscp_t mark_ip_dscp;
};

/**
 * Initialize generic error structure.
 *
 * This function also sets rte_errno to a given value.
 *
 * @param[out] error
 *   Pointer to error structure (may be NULL).
 * @param[in] code
 *   Related error code (rte_errno).
 * @param[in] type
 *   Cause field and error type.
 * @param[in] cause
 *   Object responsible for the error.
 * @param[in] message
 *   Human-readable error message.
 *
 * @return
 *   Error code.
 */
static inline int
rte_tm_error_set(struct rte_tm_error *error,
		   int code,
		   enum rte_tm_error_type type,
		   const void *cause,
		   const char *message)
{
	if (error) {
		*error = (struct rte_tm_error){
			.type = type,
			.cause = cause,
			.message = message,
		};
	}
	rte_errno = code;
	return code;
}

/**
 * Get generic traffic manager operations structure from a port
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[out] error
 *   Error details
 *
 * @return
 *   The traffic manager operations structure associated with port_id on
 *   success, NULL otherwise.
 */
const struct rte_tm_ops *
rte_tm_ops_get(uint16_t port_id, struct rte_tm_error *error);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_TM_DRIVER_H__ */
