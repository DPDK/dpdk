/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <string.h>

#include <rte_bus.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_kvargs.h>

#include "private.h"
#include "rte_bus_auxiliary.h"

enum auxiliary_params {
	RTE_AUXILIARY_PARAM_NAME,
};

static const char * const auxiliary_params_keys[] = {
	[RTE_AUXILIARY_PARAM_NAME] = "name",
};

static int
auxiliary_dev_match(const struct rte_device *dev,
	      const void *_kvlist)
{
	const struct rte_kvargs *kvlist = _kvlist;
	const char *key = auxiliary_params_keys[RTE_AUXILIARY_PARAM_NAME];

	if (rte_kvargs_get_with_value(kvlist, key, dev->name) == NULL)
		return -1;

	return 0;
}

void *
auxiliary_dev_iterate(const void *start,
		    const char *str,
		    const struct rte_dev_iterator *it __rte_unused)
{
	rte_bus_find_device_t find_device;
	struct rte_kvargs *kvargs = NULL;
	struct rte_device *dev;

	if (str != NULL) {
		kvargs = rte_kvargs_parse(str, auxiliary_params_keys);
		if (kvargs == NULL) {
			AUXILIARY_LOG(ERR, "cannot parse argument list %s",
				      str);
			rte_errno = EINVAL;
			return NULL;
		}
	}
	find_device = auxiliary_bus.bus.find_device;
	dev = find_device(start, auxiliary_dev_match, kvargs);
	rte_kvargs_free(kvargs);
	return dev;
}
