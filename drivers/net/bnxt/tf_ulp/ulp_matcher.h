/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#ifndef ULP_MATCHER_H_
#define ULP_MATCHER_H_

#include <rte_log.h>
#include "bnxt.h"
#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "bnxt_tf_common.h"

/*
 * Function to handle the matching of RTE Flows and validating
 * the pattern masks against the flow templates.
 */
int32_t
ulp_matcher_pattern_match(struct ulp_rte_parser_params *params,
			  uint32_t *class_id);

/*
 * Function to handle the matching of RTE Flows and validating
 * the action against the flow templates.
 */
int32_t
ulp_matcher_action_match(struct ulp_rte_parser_params *params,
			 uint32_t *act_id);

#endif /* ULP_MATCHER_H_ */
