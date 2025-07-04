/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#ifndef RTE_FLOW_DRIVER_H_
#define RTE_FLOW_DRIVER_H_

/**
 * @file
 * RTE generic flow API (driver side)
 *
 * This file provides implementation helpers for internal use by PMDs, they
 * are not intended to be exposed to applications and are not subject to ABI
 * versioning.
 */

#include <stdint.h>

#include "rte_ethdev.h"
#include "ethdev_driver.h"
#include "rte_flow.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generic flow operations structure implemented and returned by PMDs.
 *
 * These callback functions are not supposed to be used by applications
 * directly, which must rely on the API defined in rte_flow.h.
 *
 * Public-facing wrapper functions perform a few consistency checks so that
 * unimplemented (i.e. NULL) callbacks simply return -ENOTSUP. These
 * callbacks otherwise only differ by their first argument (with port ID
 * already resolved to a pointer to struct rte_eth_dev).
 */
struct rte_flow_ops {
	/** See rte_flow_validate(). */
	int (*validate)
		(struct rte_eth_dev *,
		 const struct rte_flow_attr *,
		 const struct rte_flow_item [],
		 const struct rte_flow_action [],
		 struct rte_flow_error *);
	/** See rte_flow_create(). */
	struct rte_flow *(*create)
		(struct rte_eth_dev *,
		 const struct rte_flow_attr *,
		 const struct rte_flow_item [],
		 const struct rte_flow_action [],
		 struct rte_flow_error *);
	/** See rte_flow_destroy(). */
	int (*destroy)
		(struct rte_eth_dev *,
		 struct rte_flow *,
		 struct rte_flow_error *);
	/** See rte_flow_flush(). */
	int (*flush)
		(struct rte_eth_dev *,
		 struct rte_flow_error *);
	/** See rte_flow_query(). */
	int (*query)
		(struct rte_eth_dev *,
		 struct rte_flow *,
		 const struct rte_flow_action *,
		 void *,
		 struct rte_flow_error *);
	/** See rte_flow_isolate(). */
	int (*isolate)
		(struct rte_eth_dev *,
		 int,
		 struct rte_flow_error *);
	/** See rte_flow_dev_dump(). */
	int (*dev_dump)
		(struct rte_eth_dev *dev,
		 struct rte_flow *flow,
		 FILE *file,
		 struct rte_flow_error *error);
	/** See rte_flow_get_aged_flows() */
	int (*get_aged_flows)
		(struct rte_eth_dev *dev,
		 void **context,
		 uint32_t nb_contexts,
		 struct rte_flow_error *err);
	/** See rte_flow_get_q_aged_flows() */
	int (*get_q_aged_flows)
		(struct rte_eth_dev *dev,
		 uint32_t queue_id,
		 void **contexts,
		 uint32_t nb_contexts,
		 struct rte_flow_error *error);
	/** See rte_flow_action_handle_create() */
	struct rte_flow_action_handle *(*action_handle_create)
		(struct rte_eth_dev *dev,
		 const struct rte_flow_indir_action_conf *conf,
		 const struct rte_flow_action *action,
		 struct rte_flow_error *error);
	/** See rte_flow_action_handle_destroy() */
	int (*action_handle_destroy)
		(struct rte_eth_dev *dev,
		 struct rte_flow_action_handle *handle,
		 struct rte_flow_error *error);
	/** See rte_flow_action_handle_update() */
	int (*action_handle_update)
		(struct rte_eth_dev *dev,
		 struct rte_flow_action_handle *handle,
		 const void *update,
		 struct rte_flow_error *error);
	/** See rte_flow_action_handle_query() */
	int (*action_handle_query)
		(struct rte_eth_dev *dev,
		 const struct rte_flow_action_handle *handle,
		 void *data,
		 struct rte_flow_error *error);
	/** See rte_flow_action_handle_query_update() */
	int (*action_handle_query_update)
		(struct rte_eth_dev *dev,
		 struct rte_flow_action_handle *handle,
		 const void *update, void *query,
		 enum rte_flow_query_update_mode qu_mode,
		 struct rte_flow_error *error);
	/** @see rte_flow_action_list_handle_create() */
	struct rte_flow_action_list_handle *(*action_list_handle_create)
		(struct rte_eth_dev *dev,
		 const struct rte_flow_indir_action_conf *conf,
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error);
	/** @see rte_flow_action_list_handle_destroy() */
	int (*action_list_handle_destroy)
		(struct rte_eth_dev *dev,
		 struct rte_flow_action_list_handle *handle,
		 struct rte_flow_error *error);
	/** See rte_flow_tunnel_decap_set() */
	int (*tunnel_decap_set)
		(struct rte_eth_dev *dev,
		 struct rte_flow_tunnel *tunnel,
		 struct rte_flow_action **pmd_actions,
		 uint32_t *num_of_actions,
		 struct rte_flow_error *err);
	/** See rte_flow_tunnel_match() */
	int (*tunnel_match)
		(struct rte_eth_dev *dev,
		 struct rte_flow_tunnel *tunnel,
		 struct rte_flow_item **pmd_items,
		 uint32_t *num_of_items,
		 struct rte_flow_error *err);
	/** See rte_flow_get_rte_flow_restore_info() */
	int (*get_restore_info)
		(struct rte_eth_dev *dev,
		 struct rte_mbuf *m,
		 struct rte_flow_restore_info *info,
		 struct rte_flow_error *err);
	/** See rte_flow_action_tunnel_decap_release() */
	int (*tunnel_action_decap_release)
		(struct rte_eth_dev *dev,
		 struct rte_flow_action *pmd_actions,
		 uint32_t num_of_actions,
		 struct rte_flow_error *err);
	/** See rte_flow_item_release() */
	int (*tunnel_item_release)
		(struct rte_eth_dev *dev,
		 struct rte_flow_item *pmd_items,
		 uint32_t num_of_items,
		 struct rte_flow_error *err);
	/** See rte_flow_pick_transfer_proxy() */
	int (*pick_transfer_proxy)
		(struct rte_eth_dev *dev,
		 uint16_t *proxy_port_id,
		 struct rte_flow_error *error);
	struct rte_flow_item_flex_handle *(*flex_item_create)
		(struct rte_eth_dev *dev,
		 const struct rte_flow_item_flex_conf *conf,
		 struct rte_flow_error *error);
	int (*flex_item_release)
		(struct rte_eth_dev *dev,
		 const struct rte_flow_item_flex_handle *handle,
		 struct rte_flow_error *error);
	/** See rte_flow_info_get() */
	int (*info_get)
		(struct rte_eth_dev *dev,
		 struct rte_flow_port_info *port_info,
		 struct rte_flow_queue_info *queue_info,
		 struct rte_flow_error *err);
	/** See rte_flow_configure() */
	int (*configure)
		(struct rte_eth_dev *dev,
		 const struct rte_flow_port_attr *port_attr,
		 uint16_t nb_queue,
		 const struct rte_flow_queue_attr *queue_attr[],
		 struct rte_flow_error *err);
	/** See rte_flow_pattern_template_create() */
	struct rte_flow_pattern_template *(*pattern_template_create)
		(struct rte_eth_dev *dev,
		 const struct rte_flow_pattern_template_attr *template_attr,
		 const struct rte_flow_item pattern[],
		 struct rte_flow_error *err);
	/** See rte_flow_pattern_template_destroy() */
	int (*pattern_template_destroy)
		(struct rte_eth_dev *dev,
		 struct rte_flow_pattern_template *pattern_template,
		 struct rte_flow_error *err);
	/** See rte_flow_actions_template_create() */
	struct rte_flow_actions_template *(*actions_template_create)
		(struct rte_eth_dev *dev,
		 const struct rte_flow_actions_template_attr *template_attr,
		 const struct rte_flow_action actions[],
		 const struct rte_flow_action masks[],
		 struct rte_flow_error *err);
	/** See rte_flow_actions_template_destroy() */
	int (*actions_template_destroy)
		(struct rte_eth_dev *dev,
		 struct rte_flow_actions_template *actions_template,
		 struct rte_flow_error *err);
	/** See rte_flow_template_table_create() */
	struct rte_flow_template_table *(*template_table_create)
		(struct rte_eth_dev *dev,
		 const struct rte_flow_template_table_attr *table_attr,
		 struct rte_flow_pattern_template *pattern_templates[],
		 uint8_t nb_pattern_templates,
		 struct rte_flow_actions_template *actions_templates[],
		 uint8_t nb_actions_templates,
		 struct rte_flow_error *err);
	/** See rte_flow_template_table_destroy() */
	int (*template_table_destroy)
		(struct rte_eth_dev *dev,
		 struct rte_flow_template_table *template_table,
		 struct rte_flow_error *err);
	/** See rte_flow_group_set_miss_actions() */
	int (*group_set_miss_actions)
		(struct rte_eth_dev *dev,
		 uint32_t group_id,
		 const struct rte_flow_group_attr *attr,
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *err);
	/** See rte_flow_actions_update(). */
	int (*actions_update)
		(struct rte_eth_dev *dev,
		 struct rte_flow *flow,
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error);
	/** @see rte_flow_action_list_handle_query_update() */
	int (*action_list_handle_query_update)
		(struct rte_eth_dev *dev,
		 const struct rte_flow_action_list_handle *handle,
		 const void **update, void **query,
		 enum rte_flow_query_update_mode mode,
		 struct rte_flow_error *error);
	/** @see rte_flow_calc_table_hash() */
	int (*flow_calc_table_hash)
		(struct rte_eth_dev *dev, const struct rte_flow_template_table *table,
		 const struct rte_flow_item pattern[], uint8_t pattern_template_index,
		 uint32_t *hash, struct rte_flow_error *error);
	/** @see rte_flow_calc_encap_hash() */
	int (*flow_calc_encap_hash)
		(struct rte_eth_dev *dev, const struct rte_flow_item pattern[],
		 enum rte_flow_encap_hash_field dest_field, uint8_t *hash,
		 struct rte_flow_error *error);
	/** @see rte_flow_template_table_resize() */
	int (*flow_template_table_resize)(struct rte_eth_dev *dev,
					  struct rte_flow_template_table *table,
					  uint32_t nb_rules,
					  struct rte_flow_error *error);
	/** @see rte_flow_async_update_resized() */
	int (*flow_update_resized)(struct rte_eth_dev *dev, uint32_t queue,
				   const struct rte_flow_op_attr *attr,
				   struct rte_flow *rule, void *user_data,
				   struct rte_flow_error *error);
	/** @see rte_flow_template_table_resize_complete() */
	int (*flow_template_table_resize_complete)
		(struct rte_eth_dev *dev,
		 struct rte_flow_template_table *table,
		 struct rte_flow_error *error);
};

/**
 * Get generic flow operations structure from a port.
 *
 * @param port_id
 *   Port identifier to query.
 * @param[out] error
 *   Pointer to flow error structure.
 *
 * @return
 *   The flow operations structure associated with port_id, NULL in case of
 *   error, in which case rte_errno is set and the error structure contains
 *   additional details.
 */
const struct rte_flow_ops *
rte_flow_ops_get(uint16_t port_id, struct rte_flow_error *error);

/**
 * Register mbuf dynamic flag for rte_flow_get_restore_info.
 */
int
rte_flow_restore_info_dynflag_register(void);

/** @internal Enqueue rule creation operation. */
typedef struct rte_flow *(*rte_flow_async_create_t)(struct rte_eth_dev *dev,
						    uint32_t queue,
						    const struct rte_flow_op_attr *attr,
						    struct rte_flow_template_table *table,
						    const struct rte_flow_item *items,
						    uint8_t pattern_template_index,
						    const struct rte_flow_action *actions,
						    uint8_t action_template_index,
						    void *user_data,
						    struct rte_flow_error *error);

/** @internal Enqueue rule creation by index operation. */
typedef struct rte_flow *(*rte_flow_async_create_by_index_t)(struct rte_eth_dev *dev,
							     uint32_t queue,
							     const struct rte_flow_op_attr *attr,
							     struct rte_flow_template_table *table,
							     uint32_t rule_index,
							     const struct rte_flow_action *actions,
							     uint8_t action_template_index,
							     void *user_data,
							     struct rte_flow_error *error);

/** @internal Enqueue rule creation by index with pattern operation. */
typedef struct rte_flow *(*rte_flow_async_create_by_index_with_pattern_t)(struct rte_eth_dev *dev,
							uint32_t queue,
							const struct rte_flow_op_attr *attr,
							struct rte_flow_template_table *table,
							uint32_t rule_index,
							const struct rte_flow_item *items,
							uint8_t pattern_template_index,
							const struct rte_flow_action *actions,
							uint8_t action_template_index,
							void *user_data,
							struct rte_flow_error *error);

/** @internal Enqueue rule update operation. */
typedef int (*rte_flow_async_actions_update_t)(struct rte_eth_dev *dev,
					       uint32_t queue_id,
					       const struct rte_flow_op_attr *op_attr,
					       struct rte_flow *flow,
					       const struct rte_flow_action *actions,
					       uint8_t actions_template_index,
					       void *user_data,
					       struct rte_flow_error *error);

/** @internal Enqueue rule destruction operation. */
typedef int (*rte_flow_async_destroy_t)(struct rte_eth_dev *dev,
					uint32_t queue_id,
					const struct rte_flow_op_attr *op_attr,
					struct rte_flow *flow,
					void *user_data,
					struct rte_flow_error *error);

/** @internal Push all internally stored rules to the HW. */
typedef int (*rte_flow_push_t)(struct rte_eth_dev *dev,
			       uint32_t queue_id,
			       struct rte_flow_error *error);

/** @internal Pull the flow rule operations results from the HW. */
typedef int (*rte_flow_pull_t)(struct rte_eth_dev *dev,
			       uint32_t queue_id,
			       struct rte_flow_op_result *res,
			       uint16_t n_res,
			       struct rte_flow_error *error);

/** @internal Enqueue indirect action creation operation. */
typedef struct rte_flow_action_handle *(*rte_flow_async_action_handle_create_t)(
					struct rte_eth_dev *dev,
					uint32_t queue_id,
					const struct rte_flow_op_attr *op_attr,
					const struct rte_flow_indir_action_conf *indir_action_conf,
					const struct rte_flow_action *action,
					void *user_data,
					struct rte_flow_error *error);

/** @internal Enqueue indirect action destruction operation. */
typedef int (*rte_flow_async_action_handle_destroy_t)(struct rte_eth_dev *dev,
						      uint32_t queue_id,
						      const struct rte_flow_op_attr *op_attr,
						      struct rte_flow_action_handle *action_handle,
						      void *user_data,
						      struct rte_flow_error *error);

/** @internal Enqueue indirect action update operation. */
typedef int (*rte_flow_async_action_handle_update_t)(struct rte_eth_dev *dev,
						     uint32_t queue_id,
						     const struct rte_flow_op_attr *op_attr,
						     struct rte_flow_action_handle *action_handle,
						     const void *update,
						     void *user_data,
						     struct rte_flow_error *error);

/** @internal Enqueue indirect action query operation. */
typedef int (*rte_flow_async_action_handle_query_t)
		(struct rte_eth_dev *dev,
		 uint32_t queue_id,
		 const struct rte_flow_op_attr *op_attr,
		 const struct rte_flow_action_handle *action_handle,
		 void *data,
		 void *user_data,
		 struct rte_flow_error *error);

/** @internal Enqueue indirect action query and/or update operation. */
typedef int (*rte_flow_async_action_handle_query_update_t)(struct rte_eth_dev *dev,
							   uint32_t queue_id,
							   const struct rte_flow_op_attr *attr,
							   struct rte_flow_action_handle *handle,
							   const void *update, void *query,
							   enum rte_flow_query_update_mode mode,
							   void *user_data,
							   struct rte_flow_error *error);

/** @internal Enqueue indirect action list creation operation. */
typedef struct rte_flow_action_list_handle *(*rte_flow_async_action_list_handle_create_t)(
	struct rte_eth_dev *dev,
	uint32_t queue_id,
	const struct rte_flow_op_attr *attr,
	const struct rte_flow_indir_action_conf *conf,
	const struct rte_flow_action *actions,
	void *user_data,
	struct rte_flow_error *error);

/** @internal Enqueue indirect action list destruction operation. */
typedef int (*rte_flow_async_action_list_handle_destroy_t)(
	struct rte_eth_dev *dev,
	uint32_t queue_id,
	const struct rte_flow_op_attr *op_attr,
	struct rte_flow_action_list_handle *handle,
	void *user_data,
	struct rte_flow_error *error);

/** @internal Enqueue indirect action list query and/or update operation. */
typedef int (*rte_flow_async_action_list_handle_query_update_t)(
	struct rte_eth_dev *dev,
	uint32_t queue_id,
	const struct rte_flow_op_attr *attr,
	const struct rte_flow_action_list_handle *handle,
	const void **update,
	void **query,
	enum rte_flow_query_update_mode mode,
	void *user_data,
	struct rte_flow_error *error);

/**
 * @internal
 *
 * Fast path async flow functions are held in a flat array, one entry per ethdev.
 */
struct __rte_cache_aligned rte_flow_fp_ops {
	rte_flow_async_create_t async_create;
	rte_flow_async_create_by_index_t async_create_by_index;
	rte_flow_async_actions_update_t async_actions_update;
	rte_flow_async_create_by_index_with_pattern_t async_create_by_index_with_pattern;
	rte_flow_async_destroy_t async_destroy;
	rte_flow_push_t push;
	rte_flow_pull_t pull;
	rte_flow_async_action_handle_create_t async_action_handle_create;
	rte_flow_async_action_handle_destroy_t async_action_handle_destroy;
	rte_flow_async_action_handle_update_t async_action_handle_update;
	rte_flow_async_action_handle_query_t async_action_handle_query;
	rte_flow_async_action_handle_query_update_t async_action_handle_query_update;
	rte_flow_async_action_list_handle_create_t async_action_list_handle_create;
	rte_flow_async_action_list_handle_destroy_t async_action_list_handle_destroy;
	rte_flow_async_action_list_handle_query_update_t async_action_list_handle_query_update;
};

/**
 * @internal
 * Default implementation of fast path flow API functions.
 */
extern struct rte_flow_fp_ops rte_flow_fp_default_ops;

#ifdef __cplusplus
}
#endif

#endif /* RTE_FLOW_DRIVER_H_ */
