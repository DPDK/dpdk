/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CNXK_RTE_FLOW_H__
#define __CNXK_RTE_FLOW_H__

#include <rte_flow_driver.h>
#include <rte_malloc.h>

#include "cnxk_ethdev.h"
#include "roc_api.h"
#include "roc_npc_priv.h"

struct cnxk_rte_flow_term_info {
	uint16_t item_type;
	uint16_t item_size;
};

struct cnxk_rte_flow_action_info {
	uint16_t conf_size;
};

#define CNXK_FLOW_NO_SEC_ACTION BIT(0)
#define CNXK_FLOW_NON_INPLACE	BIT(1)

extern const struct cnxk_rte_flow_term_info term[];

int cnxk_flow_destroy(struct rte_eth_dev *dev, struct roc_npc_flow *flow,
		      struct rte_flow_error *error);

struct roc_npc_flow *cnxk_flow_create_common(struct rte_eth_dev *eth_dev,
					     const struct rte_flow_attr *attr,
					     const struct rte_flow_item pattern[],
					     const struct rte_flow_action actions[],
					     struct rte_flow_error *error, bool is_rep,
					     uint32_t flow_flags);
int cnxk_flow_validate_common(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[], struct rte_flow_error *error,
			      bool is_rep, uint32_t flow_flags);
int cnxk_flow_destroy_common(struct rte_eth_dev *eth_dev, struct roc_npc_flow *flow,
			     struct rte_flow_error *error, bool is_rep);
int cnxk_flow_flush_common(struct rte_eth_dev *eth_dev, struct rte_flow_error *error, bool is_rep);
int cnxk_flow_query_common(struct rte_eth_dev *eth_dev, struct rte_flow *flow,
			   const struct rte_flow_action *action, void *data,
			   struct rte_flow_error *error, bool is_rep);
int cnxk_flow_dev_dump_common(struct rte_eth_dev *eth_dev, struct rte_flow *flow, FILE *file,
			      struct rte_flow_error *error, bool is_rep);
int cnxk_mtr_destroy(struct rte_eth_dev *eth_dev, uint32_t mtr_id);

int cnxk_flow_configure(struct rte_eth_dev *eth_dev, const struct rte_flow_port_attr *port_attr,
			uint16_t nb_queue, const struct rte_flow_queue_attr *queue_attr[],
			struct rte_flow_error *err);

/* clang-format off */
struct rte_flow_pattern_template *cnxk_flow_pattern_template_create(struct rte_eth_dev *eth_dev,
		const struct rte_flow_pattern_template_attr *attr,
		const struct rte_flow_item pattern[],
		struct rte_flow_error *error);
/* clang-format on */

int cnxk_flow_pattern_template_destroy(struct rte_eth_dev *eth_dev,
				       struct rte_flow_pattern_template *templ,
				       struct rte_flow_error *error);

/* clang-format off */
struct rte_flow_actions_template *cnxk_flow_actions_template_create(struct rte_eth_dev *eth_dev,
		const struct rte_flow_actions_template_attr *attr,
		const struct rte_flow_action actions[],
		const struct rte_flow_action masks[],
		struct rte_flow_error *error);
/* clang-format on */

int cnxk_flow_actions_template_destroy(struct rte_eth_dev *eth_dev,
				       struct rte_flow_actions_template *templ,
				       struct rte_flow_error *error);

/* clang-format off */
struct rte_flow_template_table *cnxk_flow_template_table_create(struct rte_eth_dev *eth_dev,
		const struct rte_flow_template_table_attr *table_attr,
		struct rte_flow_pattern_template *pattern_templates[],
		uint8_t nb_pattern_templates,
		struct rte_flow_actions_template *actions_templates[],
		uint8_t nb_actions_templates,
		struct rte_flow_error *error);
/* clang-format on */

int cnxk_flow_template_table_destroy(struct rte_eth_dev *eth_dev,
				     struct rte_flow_template_table *tbl_handle,
				     struct rte_flow_error *error);
struct rte_flow *
cnxk_flow_async_create(struct rte_eth_dev *eth_dev, uint32_t queue,
		       const struct rte_flow_op_attr *attr, struct rte_flow_template_table *table,
		       const struct rte_flow_item items[], uint8_t pattern_template_index,
		       const struct rte_flow_action actions[], uint8_t action_template_index,
		       void *user_data, struct rte_flow_error *error);
int cnxk_flow_async_destroy(struct rte_eth_dev *eth_dev, uint32_t queue,
			    const struct rte_flow_op_attr *attr, struct rte_flow *flow,
			    void *user_data, struct rte_flow_error *error);

int cnxk_flow_push(struct rte_eth_dev *eth_dev, uint32_t queue, struct rte_flow_error *error);
int cnxk_flow_pull(struct rte_eth_dev *eth_dev, uint32_t queue, struct rte_flow_op_result res[],
		   uint16_t n_res, struct rte_flow_error *error);

struct rte_flow *cnxk_flow_async_create_by_index(struct rte_eth_dev *eth_dev, uint32_t queue,
						 const struct rte_flow_op_attr *attr,
						 struct rte_flow_template_table *table,
						 uint32_t rule_index,
						 const struct rte_flow_action actions[],
						 uint8_t action_template_index, void *user_data,
						 struct rte_flow_error *error);

/* clang-format off */
struct rte_flow *cnxk_flow_async_create_by_index_with_pattern(struct rte_eth_dev *eth_dev,
		uint32_t queue, const struct rte_flow_op_attr *attr,
		struct rte_flow_template_table *tbl_handle, uint32_t rule_index,
		const struct rte_flow_item items[], uint8_t pattern_template_index,
		const struct rte_flow_action actions[],
		uint8_t action_template_index, void *user_data,
		struct rte_flow_error *error);
/* clang-format on */

int cnxk_flow_async_actions_update(struct rte_eth_dev *eth_dev, uint32_t queue,
				   const struct rte_flow_op_attr *attr, struct rte_flow *flow,
				   const struct rte_flow_action actions[],
				   uint8_t action_template_index, void *user_data,
				   struct rte_flow_error *error);
#endif /* __CNXK_RTE_FLOW_H__ */
