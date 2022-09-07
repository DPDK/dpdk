/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_flow_driver.h>
#include <rte_flow.h>
#include <iavf.h>
#include "iavf_generic_flow.h"


static struct iavf_flow_parser iavf_fsub_parser;

static struct iavf_pattern_match_item iavf_fsub_pattern_list[] = {};

static int
iavf_fsub_create(__rte_unused struct iavf_adapter *ad,
		 __rte_unused struct rte_flow *flow,
		 __rte_unused void *meta,
		 __rte_unused struct rte_flow_error *error)
{
	return -rte_errno;
}

static int
iavf_fsub_destroy(__rte_unused struct iavf_adapter *ad,
		  __rte_unused struct rte_flow *flow,
		  __rte_unused struct rte_flow_error *error)
{
	return -rte_errno;
}

static int
iavf_fsub_validation(__rte_unused struct iavf_adapter *ad,
		     __rte_unused struct rte_flow *flow,
		     __rte_unused void *meta,
		     __rte_unused struct rte_flow_error *error)
{
	return -rte_errno;
};

static int
iavf_fsub_parse(__rte_unused struct iavf_adapter *ad,
		__rte_unused struct iavf_pattern_match_item *array,
		__rte_unused uint32_t array_len,
		__rte_unused const struct rte_flow_item pattern[],
		__rte_unused const struct rte_flow_action actions[],
		__rte_unused void **meta,
		__rte_unused struct rte_flow_error *error)
{
	return -rte_errno;
}

static int
iavf_fsub_init(struct iavf_adapter *ad)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	struct iavf_flow_parser *parser;

	if (!vf->vf_res)
		return -EINVAL;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_FSUB_PF)
		parser = &iavf_fsub_parser;
	else
		return -ENOTSUP;

	return iavf_register_parser(parser, ad);
}

static void
iavf_fsub_uninit(struct iavf_adapter *ad)
{
	iavf_unregister_parser(&iavf_fsub_parser, ad);
}

static struct
iavf_flow_engine iavf_fsub_engine = {
	.init = iavf_fsub_init,
	.uninit = iavf_fsub_uninit,
	.create = iavf_fsub_create,
	.destroy = iavf_fsub_destroy,
	.validation = iavf_fsub_validation,
	.type = IAVF_FLOW_ENGINE_FSUB,
};

static struct
iavf_flow_parser iavf_fsub_parser = {
	.engine = &iavf_fsub_engine,
	.array = iavf_fsub_pattern_list,
	.array_len = RTE_DIM(iavf_fsub_pattern_list),
	.parse_pattern_action = iavf_fsub_parse,
	.stage = IAVF_FLOW_STAGE_DISTRIBUTOR,
};

RTE_INIT(iavf_fsub_engine_init)
{
	iavf_register_flow_engine(&iavf_fsub_engine);
}
