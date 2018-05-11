/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <rte_errno.h>
#include <rte_bus.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_common.h>

#include <rte_devargs.h>
#include <rte_kvargs.h>
#include <rte_alarm.h>

#include "rte_bus_ifpga.h"
#include "ifpga_logs.h"
#include "ifpga_common.h"

int rte_ifpga_get_string_arg(const char *key __rte_unused,
	const char *value, void *extra_args)
{
	if (!value || !extra_args)
		return -EINVAL;

	*(char **)extra_args = strdup(value);

	if (!*(char **)extra_args)
		return -ENOMEM;

	return 0;
}
int rte_ifpga_get_integer32_arg(const char *key __rte_unused,
	const char *value, void *extra_args)
{
	if (!value || !extra_args)
		return -EINVAL;

	*(int *)extra_args = strtoull(value, NULL, 0);

	return 0;
}
int ifpga_get_integer64_arg(const char *key __rte_unused,
	const char *value, void *extra_args)
{
	if (!value || !extra_args)
		return -EINVAL;

	*(uint64_t *)extra_args = strtoull(value, NULL, 0);

	return 0;
}
int ifpga_get_unsigned_long(const char *str, int base)
{
	unsigned long num;
	char *end = NULL;

	errno = 0;

	num = strtoul(str, &end, base);
	if ((str[0] == '\0') || (end == NULL) || (*end != '\0') || (errno != 0))
		return -1;

	return num;
}

int ifpga_afu_id_cmp(const struct rte_afu_id *afu_id0,
	const struct rte_afu_id *afu_id1)
{
	if ((afu_id0->uuid.uuid_low == afu_id1->uuid.uuid_low) &&
		(afu_id0->uuid.uuid_high == afu_id1->uuid.uuid_high) &&
		(afu_id0->port == afu_id1->port)) {
		return 0;
	} else
		return 1;
}
