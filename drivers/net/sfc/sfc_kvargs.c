/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2016-2017 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
		SFC_KVARG_STATS_UPDATE_PERIOD_MS,
		SFC_KVARG_DEBUG_INIT,
		SFC_KVARG_MCDI_LOGGING,
		SFC_KVARG_PERF_PROFILE,
		SFC_KVARG_RX_DATAPATH,
		SFC_KVARG_TX_DATAPATH,
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
