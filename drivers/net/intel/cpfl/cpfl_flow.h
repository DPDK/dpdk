/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _CPFL_FLOW_H_
#define _CPFL_FLOW_H_

#include <rte_flow.h>
#include "cpfl_ethdev.h"

#define CPFL_PREC_MAX 7

extern const struct rte_flow_ops cpfl_flow_ops;

enum cpfl_flow_engine_type {
	CPFL_FLOW_ENGINE_NONE = 0,
	CPFL_FLOW_ENGINE_FXP,
};

typedef int (*engine_init_t)(struct cpfl_adapter_ext *ad);
typedef void (*engine_uninit_t)(struct cpfl_adapter_ext *ad);
typedef int (*engine_create_t)(struct rte_eth_dev *dev,
			       struct rte_flow *flow,
			       void *meta,
			       struct rte_flow_error *error);
typedef int (*engine_destroy_t)(struct rte_eth_dev *dev,
				struct rte_flow *flow,
				struct rte_flow_error *error);
typedef int (*engine_query_t)(struct rte_eth_dev *dev,
			      struct rte_flow *flow,
			      struct rte_flow_query_count *count,
			      struct rte_flow_error *error);
typedef void (*engine_free_t) (struct rte_flow *flow);
typedef int (*engine_parse_pattern_action_t)(struct rte_eth_dev *dev,
					     const struct rte_flow_attr *attr,
					     const struct rte_flow_item pattern[],
					     const struct rte_flow_action actions[],
					     void **meta);

struct cpfl_flow_engine {
	TAILQ_ENTRY(cpfl_flow_engine) node;
	enum cpfl_flow_engine_type type;
	engine_init_t init;
	engine_uninit_t uninit;
	engine_create_t create;
	engine_destroy_t destroy;
	engine_query_t query_count;
	engine_free_t free;
	engine_parse_pattern_action_t parse_pattern_action;
};

struct rte_flow {
	TAILQ_ENTRY(rte_flow) next;
	struct cpfl_flow_engine *engine;
	void *rule;
};

void cpfl_flow_engine_register(struct cpfl_flow_engine *engine);
struct cpfl_flow_engine *cpfl_flow_engine_match(struct rte_eth_dev *dev,
						const struct rte_flow_attr *attr,
						const struct rte_flow_item pattern[],
						const struct rte_flow_action actions[],
						void **meta);
int cpfl_flow_engine_init(struct cpfl_adapter_ext *adapter);
void cpfl_flow_engine_uninit(struct cpfl_adapter_ext *adapter);
int cpfl_flow_init(struct cpfl_adapter_ext *ad, struct cpfl_devargs *devargs);
void cpfl_flow_uninit(struct cpfl_adapter_ext *ad);
struct rte_flow *cpfl_flow_create(struct rte_eth_dev *dev,
				  const struct rte_flow_attr *attr,
				  const struct rte_flow_item pattern[],
				  const struct rte_flow_action actions[],
				  struct rte_flow_error *error);
int cpfl_flow_validate(struct rte_eth_dev *dev,
		       const struct rte_flow_attr *attr,
		       const struct rte_flow_item pattern[],
		       const struct rte_flow_action actions[],
		       struct rte_flow_error *error);
int cpfl_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow, struct rte_flow_error *error);
int cpfl_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error);
int cpfl_flow_query(struct rte_eth_dev *dev,
		    struct rte_flow *flow,
		    const struct rte_flow_action *actions,
		    void *data,
		    struct rte_flow_error *error);
#endif
