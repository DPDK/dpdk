/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_devargs.h>

#include "cnxk_cryptodev.h"

#define CNXK_MAX_QPS_LIMIT     "max_qps_limit"
#define CNXK_MAX_QPS_LIMIT_MIN 1
#define CNXK_MAX_QPS_LIMIT_MAX (ROC_CPT_MAX_LFS - 1)
#define CNXK_RX_INJECT_QP      "rx_inject_qp"

static int
parse_rx_inject_qp(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	if (val < CNXK_MAX_QPS_LIMIT_MIN || val > CNXK_MAX_QPS_LIMIT_MAX)
		return -EINVAL;

	*(uint16_t *)extra_args = val;

	return 0;
}

static int
parse_max_qps_limit(const char *key, const char *value, void *extra_args)
{
	RTE_SET_USED(key);
	uint32_t val;

	val = atoi(value);

	if (val < CNXK_MAX_QPS_LIMIT_MIN || val > CNXK_MAX_QPS_LIMIT_MAX)
		return -EINVAL;

	*(uint16_t *)extra_args = val;

	return 0;
}

int
cnxk_cpt_parse_devargs(struct rte_devargs *devargs, struct cnxk_cpt_vf *vf)
{
	uint16_t max_qps_limit = CNXK_MAX_QPS_LIMIT_MAX;
	struct rte_kvargs *kvlist;
	uint16_t rx_inject_qp;
	int rc;

	/* Set to max value as default so that the feature is disabled by default. */
	rx_inject_qp = CNXK_MAX_QPS_LIMIT_MAX;

	if (devargs == NULL)
		goto null_devargs;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		goto exit;

	rc = rte_kvargs_process(kvlist, CNXK_MAX_QPS_LIMIT,
				&parse_max_qps_limit, &max_qps_limit);
	if (rc < 0) {
		plt_err("max_qps_limit should in the range <%d-%d>",
			CNXK_MAX_QPS_LIMIT_MIN, CNXK_MAX_QPS_LIMIT_MAX);
		rte_kvargs_free(kvlist);
		goto exit;
	}

	rc = rte_kvargs_process(kvlist, CNXK_RX_INJECT_QP, parse_rx_inject_qp, &rx_inject_qp);
	if (rc < 0) {
		plt_err("rx_inject_qp should in the range <%d-%d>", CNXK_MAX_QPS_LIMIT_MIN,
			max_qps_limit - 1);
		rte_kvargs_free(kvlist);
		goto exit;
	}

	rte_kvargs_free(kvlist);

null_devargs:
	vf->max_qps_limit = max_qps_limit;
	vf->rx_inject_qp = rx_inject_qp;
	return 0;

exit:
	return -EINVAL;
}

RTE_PMD_REGISTER_PARAM_STRING(crypto_cnxk, CNXK_MAX_QPS_LIMIT "=<1-63>");
