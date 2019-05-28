/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <inttypes.h>
#include <math.h>

#include "otx2_ethdev.h"

static int
parse_flow_max_priority(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint16_t val;

	val = atoi(value);

	/* Limit the max priority to 32 */
	if (val < 1 || val > 32)
		return -EINVAL;

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_flow_prealloc_size(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint16_t val;

	val = atoi(value);

	/* Limit the prealloc size to 32 */
	if (val < 1 || val > 32)
		return -EINVAL;

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_reta_size(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	if (val <= ETH_RSS_RETA_SIZE_64)
		val = ETH_RSS_RETA_SIZE_64;
	else if (val > ETH_RSS_RETA_SIZE_64 && val <= ETH_RSS_RETA_SIZE_128)
		val = ETH_RSS_RETA_SIZE_128;
	else if (val > ETH_RSS_RETA_SIZE_128 && val <= ETH_RSS_RETA_SIZE_256)
		val = ETH_RSS_RETA_SIZE_256;
	else
		val = NIX_RSS_RETA_SIZE;

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_ptype_flag(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);
	if (val)
		val = 0; /* Disable NIX_RX_OFFLOAD_PTYPE_F */

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_flag(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);

	*(uint16_t *)extra_args = atoi(value);

	return 0;
}

static int
parse_sqb_count(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	if (val < NIX_MIN_SQB || val > NIX_MAX_SQB)
		return -EINVAL;

	*(uint16_t *)extra_args = val;

	return 0;
}

#define OTX2_RSS_RETA_SIZE "reta_size"
#define OTX2_PTYPE_DISABLE "ptype_disable"
#define OTX2_SCL_ENABLE "scalar_enable"
#define OTX2_MAX_SQB_COUNT "max_sqb_count"
#define OTX2_FLOW_PREALLOC_SIZE "flow_prealloc_size"
#define OTX2_FLOW_MAX_PRIORITY "flow_max_priority"

int
otx2_ethdev_parse_devargs(struct rte_devargs *devargs, struct otx2_eth_dev *dev)
{
	uint16_t offload_flag = NIX_RX_OFFLOAD_PTYPE_F;
	uint16_t rss_size = NIX_RSS_RETA_SIZE;
	uint16_t sqb_count = NIX_MAX_SQB;
	uint16_t flow_prealloc_size = 8;
	uint16_t flow_max_priority = 3;
	uint16_t scalar_enable = 0;
	struct rte_kvargs *kvlist;

	if (devargs == NULL)
		goto null_devargs;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		goto exit;

	rte_kvargs_process(kvlist, OTX2_PTYPE_DISABLE,
			   &parse_ptype_flag, &offload_flag);
	rte_kvargs_process(kvlist, OTX2_RSS_RETA_SIZE,
			   &parse_reta_size, &rss_size);
	rte_kvargs_process(kvlist, OTX2_SCL_ENABLE,
			   &parse_flag, &scalar_enable);
	rte_kvargs_process(kvlist, OTX2_MAX_SQB_COUNT,
			   &parse_sqb_count, &sqb_count);
	rte_kvargs_process(kvlist, OTX2_FLOW_PREALLOC_SIZE,
			   &parse_flow_prealloc_size, &flow_prealloc_size);
	rte_kvargs_process(kvlist, OTX2_FLOW_MAX_PRIORITY,
			   &parse_flow_max_priority, &flow_max_priority);
	rte_kvargs_free(kvlist);

null_devargs:
	dev->rx_offload_flags = offload_flag;
	dev->scalar_ena = scalar_enable;
	dev->max_sqb_count = sqb_count;
	dev->rss_info.rss_size = rss_size;
	dev->npc_flow.flow_prealloc_size = flow_prealloc_size;
	dev->npc_flow.flow_max_priority = flow_max_priority;
	return 0;

exit:
	return -EINVAL;
}

RTE_PMD_REGISTER_PARAM_STRING(net_octeontx2,
			      OTX2_RSS_RETA_SIZE "=<64|128|256>"
			      OTX2_PTYPE_DISABLE "=1"
			      OTX2_SCL_ENABLE "=1"
			      OTX2_MAX_SQB_COUNT "=<32-512>"
			      OTX2_FLOW_PREALLOC_SIZE "=<1-32>"
			      OTX2_FLOW_MAX_PRIORITY "=<1-32>");
