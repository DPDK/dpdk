/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */
#include <rte_flow_driver.h>
#include <rte_tailq.h>

#include "cpfl_flow.h"
#include "cpfl_flow_parser.h"

TAILQ_HEAD(cpfl_flow_engine_list, cpfl_flow_engine);

static struct cpfl_flow_engine_list engine_list = TAILQ_HEAD_INITIALIZER(engine_list);

void
cpfl_flow_engine_register(struct cpfl_flow_engine *engine)
{
	TAILQ_INSERT_TAIL(&engine_list, engine, node);
}

struct cpfl_flow_engine *
cpfl_flow_engine_match(struct rte_eth_dev *dev,
		       const struct rte_flow_attr *attr,
		       const struct rte_flow_item pattern[],
		       const struct rte_flow_action actions[],
		       void **meta)
{
	struct cpfl_flow_engine *engine = NULL;
	void *temp;

	RTE_TAILQ_FOREACH_SAFE(engine, &engine_list, node, temp) {
		if (!engine->parse_pattern_action)
			continue;

		if (engine->parse_pattern_action(dev, attr, pattern, actions, meta) < 0)
			continue;
		return engine;
	}

	return NULL;
}

int
cpfl_flow_engine_init(struct cpfl_adapter_ext *adapter)
{
	struct cpfl_flow_engine *engine = NULL;
	void *temp;
	int ret;

	RTE_TAILQ_FOREACH_SAFE(engine, &engine_list, node, temp) {
		if (!engine->init) {
			PMD_INIT_LOG(ERR, "Invalid engine type (%d)",
				     engine->type);
			return -ENOTSUP;
		}

		ret = engine->init(adapter);
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to initialize engine %d",
				     engine->type);
			return ret;
		}
	}

	return 0;
}

void
cpfl_flow_engine_uninit(struct cpfl_adapter_ext *adapter)
{
	struct cpfl_flow_engine *engine = NULL;
	void *temp;

	RTE_TAILQ_FOREACH_SAFE(engine, &engine_list, node, temp) {
		if (engine->uninit)
			engine->uninit(adapter);
	}
}

static int
cpfl_flow_attr_valid(const struct rte_flow_attr *attr,
		     struct rte_flow_error *error)
{
	if (attr->priority > CPFL_PREC_MAX) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   attr, "Only support priority 0-7.");
		return -rte_errno;
	}

	return 0;
}

static int
cpfl_flow_param_valid(const struct rte_flow_attr *attr,
		      const struct rte_flow_item pattern[],
		      const struct rte_flow_action actions[],
		      struct rte_flow_error *error)
{
	int ret;

	if (!pattern) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "NULL pattern.");
		return -rte_errno;
	}

	if (!attr) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "NULL attribute.");
		return -rte_errno;
	}

	ret = cpfl_flow_attr_valid(attr, error);
	if (ret)
		return ret;

	if (!actions || actions->type == RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "NULL action.");
		return -rte_errno;
	}

	return 0;
}

static int
__cpfl_flow_validate(struct rte_eth_dev *dev,
		     const struct rte_flow_attr *attr,
		     const struct rte_flow_item pattern[],
		     const struct rte_flow_action actions[],
		     void **meta,
		     struct cpfl_flow_engine **engine,
		     struct rte_flow_error *error)
{
	int ret;

	ret = cpfl_flow_param_valid(attr, pattern, actions, error);
	if (ret)
		return ret;

	*engine = cpfl_flow_engine_match(dev, attr, pattern, actions, meta);
	if (!*engine) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "No matched engine.");
		return -rte_errno;
	}

	return 0;
}

int
cpfl_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct cpfl_flow_engine *engine = NULL;
	int ret;

	ret = __cpfl_flow_validate(dev, attr, pattern, actions, NULL, &engine, error);

	return ret;
}

struct rte_flow *
cpfl_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct cpfl_itf *itf = CPFL_DEV_TO_ITF(dev);
	struct cpfl_flow_engine *engine = NULL;
	struct rte_flow *flow;
	void *meta;
	int ret;

	flow = rte_malloc(NULL, sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory");
		return NULL;
	}

	ret = __cpfl_flow_validate(dev, attr, pattern, actions, &meta, &engine, error);
	if (ret) {
		rte_free(flow);
		return NULL;
	}

	if (!engine->create) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "No matched flow creation function");
		rte_free(flow);
		return NULL;
	}

	ret = engine->create(dev, flow, meta, error);
	if (ret) {
		rte_free(flow);
		return NULL;
	}

	flow->engine = engine;
	TAILQ_INSERT_TAIL(&itf->flow_list, flow, next);

	return flow;
}

int
cpfl_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error)
{
	struct cpfl_itf *itf = CPFL_DEV_TO_ITF(dev);
	int ret = 0;

	if (!flow || !flow->engine || !flow->engine->destroy) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Invalid flow");
		return -rte_errno;
	}

	ret = flow->engine->destroy(dev, flow, error);
	if (!ret)
		TAILQ_REMOVE(&itf->flow_list, flow, next);
	else
		PMD_DRV_LOG(ERR, "Failed to destroy flow");

	return ret;
}

int
cpfl_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	struct cpfl_itf *itf = CPFL_DEV_TO_ITF(dev);
	struct rte_flow *p_flow;
	void *temp;
	int ret = 0;

	RTE_TAILQ_FOREACH_SAFE(p_flow, &itf->flow_list, next, temp) {
		ret = cpfl_flow_destroy(dev, p_flow, error);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to flush flows");
			return -EINVAL;
		}
	}

	return ret;
}

int
cpfl_flow_query(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_action *actions,
		void *data,
		struct rte_flow_error *error)
{
	struct rte_flow_query_count *count = data;
	int ret = -EINVAL;

	if (!flow || !flow->engine || !flow->engine->query_count) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Invalid flow");
		return -rte_errno;
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow->engine->query_count(dev, flow, count, error);
			break;
		default:
			ret = rte_flow_error_set(error, ENOTSUP,
						 RTE_FLOW_ERROR_TYPE_ACTION,
						 actions,
						 "action not supported");
			break;
		}
	}

	return ret;
}

const struct rte_flow_ops cpfl_flow_ops = {
	.validate = cpfl_flow_validate,
	.create = cpfl_flow_create,
	.destroy = cpfl_flow_destroy,
	.flush = cpfl_flow_flush,
	.query = cpfl_flow_query,
};

int
cpfl_flow_init(struct cpfl_adapter_ext *ad, struct cpfl_devargs *devargs)
{
	int ret;

	if (devargs->flow_parser[0] == '\0') {
		PMD_INIT_LOG(WARNING, "flow module is not initialized");
		return 0;
	}

	ret = cpfl_flow_engine_init(ad);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to init flow engines");
		goto err;
	}

	ret = cpfl_parser_create(&ad->flow_parser, devargs->flow_parser);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to create flow parser");
		goto err;
	}

	return ret;

err:
	cpfl_flow_engine_uninit(ad);
	return ret;
}

void
cpfl_flow_uninit(struct cpfl_adapter_ext *ad)
{
	if (ad->flow_parser == NULL)
		return;

	cpfl_parser_destroy(ad->flow_parser);
	cpfl_flow_engine_uninit(ad);
}
