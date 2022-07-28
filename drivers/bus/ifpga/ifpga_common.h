/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_COMMON_H_
#define _IFPGA_COMMON_H_

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <bus_ifpga_driver.h>
#include <rte_common.h>

static inline int
ifpga_get_string_arg(const char *key __rte_unused, const char *value,
	void *extra_args)
{
	if (!value || !extra_args)
		return -EINVAL;

	*(char **)extra_args = strdup(value);

	if (!*(char **)extra_args)
		return -ENOMEM;

	return 0;
}

static inline int
ifpga_get_integer32_arg(const char *key __rte_unused, const char *value,
	void *extra_args)
{
	if (!value || !extra_args)
		return -EINVAL;

	*(int *)extra_args = strtoull(value, NULL, 0);

	return 0;
}

static inline int
ifpga_afu_id_cmp(const struct rte_afu_id *afu_id0,
	const struct rte_afu_id *afu_id1)
{
	if ((afu_id0->uuid.uuid_low == afu_id1->uuid.uuid_low) &&
		(afu_id0->uuid.uuid_high == afu_id1->uuid.uuid_high) &&
		(afu_id0->port == afu_id1->port)) {
		return 0;
	} else
		return 1;
}

#endif /* _IFPGA_COMMON_H_ */
