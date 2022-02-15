/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_FLOW_CLASSIFY_PARSE_H_
#define _RTE_FLOW_CLASSIFY_PARSE_H_

#include <rte_ethdev.h>
#include <rte_flow.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern enum rte_flow_classify_table_type table_type;

struct classify_action {
	/* Flow action mask */
	uint64_t action_mask;

	struct action {
		/** Integer value to return with packets */
		struct rte_flow_action_mark mark;
		/** Flow rule counter */
		struct rte_flow_query_count counter;
	} act;
};

typedef int (*parse_filter_t)(const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
			      struct rte_eth_ntuple_filter *filter,
			      struct rte_flow_error *error);

/* Skip all VOID items of the pattern */
void
classify_pattern_skip_void_item(struct rte_flow_item *items,
			    const struct rte_flow_item *pattern);

/* Find the first VOID or non-VOID item pointer */
const struct rte_flow_item *
classify_find_first_item(const struct rte_flow_item *item, bool is_void);


/* Find if there's parse filter function matched */
parse_filter_t
classify_find_parse_filter_func(struct rte_flow_item *pattern);

/* get action data */
struct classify_action *
classify_get_flow_action(void);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_FLOW_CLASSIFY_PARSE_H_ */
