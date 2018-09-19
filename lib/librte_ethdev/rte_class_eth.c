/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Gaëtan Rivet
 */

#include <string.h>

#include <rte_class.h>
#include <rte_compat.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
#include <rte_log.h>

#include "rte_ethdev.h"
#include "rte_ethdev_core.h"
#include "ethdev_private.h"

enum eth_params {
	RTE_ETH_PARAMS_MAX,
};

static const char * const eth_params_keys[] = {
	[RTE_ETH_PARAMS_MAX] = NULL,
};

struct eth_dev_match_arg {
	struct rte_device *device;
	struct rte_kvargs *kvlist;
};

#define eth_dev_match_arg(d, k) \
	(&(const struct eth_dev_match_arg) { \
		.device = (d), \
		.kvlist = (k), \
	})

static int
eth_dev_match(const struct rte_eth_dev *edev,
	      const void *_arg)
{
	const struct eth_dev_match_arg *arg = _arg;
	const struct rte_kvargs *kvlist = arg->kvlist;

	if (edev->state == RTE_ETH_DEV_UNUSED)
		return -1;
	if (edev->device != arg->device)
		return -1;
	if (kvlist == NULL)
		/* Empty string matches everything. */
		return 0;
	return 0;
}

static void *
eth_dev_iterate(const void *start,
		const char *str,
		const struct rte_dev_iterator *it)
{
	struct rte_kvargs *kvargs = NULL;
	struct rte_eth_dev *edev = NULL;

	if (str != NULL) {
		kvargs = rte_kvargs_parse(str, eth_params_keys);
		if (kvargs == NULL) {
			RTE_LOG(ERR, EAL, "cannot parse argument list\n");
			rte_errno = EINVAL;
			return NULL;
		}
	}
	edev = eth_find_device(start, eth_dev_match,
			       eth_dev_match_arg(it->device, kvargs));
	rte_kvargs_free(kvargs);
	return edev;
}

struct rte_class rte_class_eth = {
	.dev_iterate = eth_dev_iterate,
};

RTE_REGISTER_CLASS(eth, rte_class_eth);
