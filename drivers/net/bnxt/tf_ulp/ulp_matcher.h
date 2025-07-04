/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef ULP_MATCHER_H_
#define ULP_MATCHER_H_

#include <rte_log.h>
#include <rte_hash.h>
#include "bnxt.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "bnxt_tf_common.h"

#define BNXT_ULP_ACT_HASH_LIST_SIZE 1024

struct ulp_matcher_hash_db_key {
	struct ulp_rte_hdr_bitmap hdr_bitmap;
	uint8_t app_id;
};

struct ulp_matcher_class_db_node {
	uint8_t in_use;
	uint16_t match_info_idx;
};

struct ulp_matcher_action_hash_db_key {
	struct ulp_rte_act_bitmap act_bitmap;
};

struct ulp_matcher_act_db_node {
	uint16_t act_tmpl_idx;
};

struct bnxt_ulp_matcher_data {
	struct rte_hash *class_matcher_db;
	uint16_t class_list_size;
	struct ulp_matcher_class_db_node *class_list;
	struct rte_hash *action_matcher_db;
	struct ulp_matcher_act_db_node *act_list;
};

int32_t
ulp_matcher_pattern_match(struct ulp_rte_parser_params *params,
			  uint32_t *class_id);

int32_t
ulp_matcher_action_match(struct ulp_rte_parser_params *params,
			 uint32_t *act_id);

int32_t ulp_matcher_init(struct bnxt_ulp_context *ulp_ctx);

void ulp_matcher_deinit(struct bnxt_ulp_context *ulp_ctx);

#endif /* ULP_MATCHER_H_ */
