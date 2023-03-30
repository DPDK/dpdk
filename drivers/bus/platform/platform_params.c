/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <string.h>
#include <errno.h>

#include <rte_bus.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
#include <rte_vfio.h>

#include "bus_platform_driver.h"
#include "private.h"

#ifdef VFIO_PRESENT

enum platform_params {
	RTE_PLATFORM_PARAM_NAME,
};

static const char * const platform_params_keys[] = {
	[RTE_PLATFORM_PARAM_NAME] = "name",
	NULL
};

static int
platform_dev_match(const struct rte_device *dev, const void *_kvlist)
{
	const char *key = platform_params_keys[RTE_PLATFORM_PARAM_NAME];
	const struct rte_kvargs *kvlist = _kvlist;
	const char *name;

	/* no kvlist arg, all devices match */
	if (kvlist == NULL)
		return 0;

	/* if key is present in kvlist and does not match, filter device */
	name = rte_kvargs_get(kvlist, key);
	if (name != NULL && strcmp(name, dev->name))
		return -1;

	return 0;
}

void *
platform_bus_dev_iterate(const void *start, const char *str,
			 const struct rte_dev_iterator *it __rte_unused)
{
	rte_bus_find_device_t find_device;
	struct rte_kvargs *kvargs = NULL;
	struct rte_device *dev;

	if (str != NULL) {
		kvargs = rte_kvargs_parse(str, platform_params_keys);
		if (!kvargs) {
			PLATFORM_LOG(ERR, "cannot parse argument list %s", str);
			rte_errno = EINVAL;
			return NULL;
		}
	}

	find_device = platform_bus.bus.find_device;
	if (find_device == NULL) {
		rte_kvargs_free(kvargs);
		return NULL;
	}

	dev = platform_bus.bus.find_device(start, platform_dev_match, kvargs);
	rte_kvargs_free(kvargs);

	return dev;
}

#endif /* VFIO_PRESENT */
