/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <stdbool.h>
#include <strings.h>

#include <rte_devargs.h>
#include <rte_kvargs.h>

#include "sfc.h"
#include "sfc_kvargs.h"

int
sfc_kvargs_parse(struct sfc_adapter *sa)
{
	struct rte_eth_dev *eth_dev = (sa)->eth_dev;
	struct rte_devargs *devargs = eth_dev->device->devargs;
	const char **params = (const char *[]){
		SFC_KVARG_SWITCH_MODE,
		SFC_KVARG_REPRESENTOR,
		SFC_KVARG_STATS_UPDATE_PERIOD_MS,
		SFC_KVARG_PERF_PROFILE,
		SFC_KVARG_RX_DATAPATH,
		SFC_KVARG_TX_DATAPATH,
		SFC_KVARG_FW_VARIANT,
		SFC_KVARG_RXD_WAIT_TIMEOUT_NS,
		RTE_DEVARGS_KEY_CLASS,
		NULL,
	};

	if (devargs == NULL)
		return 0;

	sa->kvargs = rte_kvargs_parse(devargs->args, params);
	if (sa->kvargs == NULL)
		return EINVAL;

	return 0;
}

void
sfc_kvargs_cleanup(struct sfc_adapter *sa)
{
	rte_kvargs_free(sa->kvargs);
}

static int
sfc_kvarg_match_value(const char *value, const char * const *values,
		      unsigned int n_values)
{
	unsigned int i;

	for (i = 0; i < n_values; ++i)
		if (strcasecmp(value, values[i]) == 0)
			return 1;

	return 0;
}

int
sfc_kvargs_process(struct sfc_adapter *sa, const char *key_match,
		   arg_handler_t handler, void *opaque_arg)
{
	if (sa->kvargs == NULL)
		return 0;

	return -rte_kvargs_process(sa->kvargs, key_match, handler, opaque_arg);
}

int
sfc_kvarg_bool_handler(__rte_unused const char *key,
		       const char *value_str, void *opaque)
{
	const char * const true_strs[] = {
		"1", "y", "yes", "on", "true"
	};
	const char * const false_strs[] = {
		"0", "n", "no", "off", "false"
	};
	bool *value = opaque;

	if (sfc_kvarg_match_value(value_str, true_strs,
				  RTE_DIM(true_strs)))
		*value = true;
	else if (sfc_kvarg_match_value(value_str, false_strs,
				       RTE_DIM(false_strs)))
		*value = false;
	else
		return -EINVAL;

	return 0;
}

int
sfc_kvarg_long_handler(__rte_unused const char *key,
		       const char *value_str, void *opaque)
{
	long value;
	char *endptr;

	if (!value_str || !opaque)
		return -EINVAL;

	value = strtol(value_str, &endptr, 0);
	if (endptr == value_str)
		return -EINVAL;

	*(long *)opaque = value;

	return 0;
}

int
sfc_kvarg_string_handler(__rte_unused const char *key,
			 const char *value_str, void *opaque)
{
	*(const char **)opaque = value_str;

	return 0;
}
