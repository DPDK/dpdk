/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Gaëtan Rivet
 */

#include <string.h>

#include <rte_dev.h>
#include <rte_bus.h>
#include <rte_kvargs.h>
#include <rte_errno.h>

#include "vdev_logs.h"
#include "vdev_private.h"

enum vdev_params {
	RTE_VDEV_PARAM_NAME,
	RTE_VDEV_PARAM_MAX,
};

static const char * const vdev_params_keys[] = {
	[RTE_VDEV_PARAM_NAME] = "name",
	[RTE_VDEV_PARAM_MAX] = NULL,
};

static int
vdev_dev_match(const struct rte_device *dev,
	       const void *_kvlist)
{
	int ret;
	const struct rte_kvargs *kvlist = _kvlist;
	char *name;

	/* cannot pass const dev->name to rte_kvargs_process() */
	name = strdup(dev->name);
	if (name == NULL)
		return -1;
	ret = rte_kvargs_process(kvlist,
		vdev_params_keys[RTE_VDEV_PARAM_NAME],
		rte_kvargs_strcmp, name);
	free(name);
	if (ret != 0)
		return -1;

	return 0;
}

void *
rte_vdev_dev_iterate(const void *start,
		     const char *str,
		     const struct rte_dev_iterator *it __rte_unused)
{
	struct rte_kvargs *kvargs = NULL;
	struct rte_device *dev;

	if (str != NULL) {
		kvargs = rte_kvargs_parse(str, vdev_params_keys);
		if (kvargs == NULL) {
			VDEV_LOG(ERR, "cannot parse argument list\n");
			rte_errno = EINVAL;
			return NULL;
		}
	}
	dev = rte_vdev_find_device(start, vdev_dev_match, kvargs);
	rte_kvargs_free(kvargs);
	return dev;
}
