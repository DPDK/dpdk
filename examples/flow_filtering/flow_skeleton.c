/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <stdint.h>

#include <rte_errno.h>
#include <rte_flow.h>

#include "common.h"
#include "snippets/snippet_match_ipv4.h"


struct rte_flow_attr attr = { .ingress = 1 };
struct rte_flow_op_attr ops_attr = { .postpone = 0 };

static struct rte_flow *
create_flow_non_template(uint16_t port_id, struct rte_flow_attr *attr,
						struct rte_flow_item *patterns,
						struct rte_flow_action *actions,
						struct rte_flow_error *error)
{
	struct rte_flow *flow = NULL;

	/* Validate the rule and create it. */
	if (rte_flow_validate(port_id, attr, patterns, actions, error) == 0)
		flow = rte_flow_create(port_id, attr, patterns, actions, error);
	return flow;
}

static struct rte_flow *
create_flow_template(uint16_t port_id, struct rte_flow_op_attr *ops_attr,
                    struct rte_flow_item *patterns,
                    struct rte_flow_action *actions,
                    struct rte_flow_error *error)
{
    struct rte_flow_template_table *table = snippet_ipv4_flow_create_table(port_id, error);
    if (table == NULL) {
        printf("Failed to create table: %s (%s)\n",
        error->message, rte_strerror(rte_errno));
        return NULL;
    }

    #define NUM_FLOWS 10000
    struct rte_flow *flow = NULL;
    struct rte_flow *first_flow = NULL;
    struct rte_flow_op_result 	res[1];
    uint64_t start, end, cycles;
    uint64_t min_cycles = UINT64_MAX;
    uint64_t max_cycles = 0;
    int i;

    for (i = 0; i < NUM_FLOWS; i++) {
        struct rte_flow_item_ipv4 *ipv4 = (struct rte_flow_item_ipv4 *)patterns[1].spec;
        ipv4->hdr.dst_addr = rte_cpu_to_be_32(0xC0A80000 + i);  /* 192.168.0.0 + i */

        start = rte_rdtsc();
        flow = rte_flow_async_create(port_id,
            QUEUE_ID,
            ops_attr,
            table,
            patterns,
            0,
            actions,
            0,
            0,
            error);
        end = rte_rdtsc();

        cycles = end - start;
        printf("Flow %d: %lu cycles\n", i, cycles);

        if (cycles < min_cycles) min_cycles = cycles;
        if (cycles > max_cycles) max_cycles = cycles;

        if (flow && !first_flow)
            first_flow = flow;

        rte_flow_push(port_id, QUEUE_ID, error);
        rte_flow_pull(port_id, QUEUE_ID, res, 1, error);
    }

    printf("\n=== Results for %d flows ===\n", NUM_FLOWS);
    printf("Lowest: %lu cycles\n", min_cycles);
    printf("Highest: %lu cycles\n", max_cycles);

    return 0;
}

struct rte_flow *
generate_flow_skeleton(uint16_t port_id, struct rte_flow_error *error, int use_template_api)
{
	/* Set the common action and pattern structures 8< */
	struct rte_flow_action actions[MAX_ACTION_NUM] = {0};
	struct rte_flow_item patterns[MAX_PATTERN_NUM] = {0};

	/* Replace this function call with
	 * snippet_*_create_actions() function from the snippets directory
	 */
	snippet_ipv4_flow_create_actions(actions);

	/* Replace this function call with
	 * snippet_*_create_patterns() function from the snippets directory
	 */
	snippet_ipv4_flow_create_patterns(patterns);
	/* >8 End of setting the common action and pattern structures. */

	/* Create a flow rule using template API 8< */
	if (use_template_api)
		return create_flow_template(port_id, &ops_attr, patterns, actions, error);
	/* >8 End of creating a flow rule using template API. */

	/* Validate and create the rule 8< */
	return create_flow_non_template(port_id, &attr, patterns, actions, error);
	/* >8 End of validating and creating the rule. */
}
