/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <string.h>

#include <rte_common.h>
#include <rte_cryptodev_pmd.h>
#include <rte_malloc.h>

#include "ccp_pmd_private.h"
#include "ccp_dev.h"

static const struct rte_cryptodev_capabilities ccp_pmd_capabilities[] = {
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static int
ccp_pmd_config(struct rte_cryptodev *dev __rte_unused,
	       struct rte_cryptodev_config *config __rte_unused)
{
	return 0;
}

static int
ccp_pmd_start(struct rte_cryptodev *dev)
{
	return ccp_dev_start(dev);
}

static void
ccp_pmd_stop(struct rte_cryptodev *dev __rte_unused)
{

}

static int
ccp_pmd_close(struct rte_cryptodev *dev __rte_unused)
{
	return 0;
}

static void
ccp_pmd_info_get(struct rte_cryptodev *dev,
		 struct rte_cryptodev_info *dev_info)
{
	struct ccp_private *internals = dev->data->dev_private;

	if (dev_info != NULL) {
		dev_info->driver_id = dev->driver_id;
		dev_info->feature_flags = dev->feature_flags;
		dev_info->capabilities = ccp_pmd_capabilities;
		dev_info->max_nb_queue_pairs = internals->max_nb_qpairs;
		dev_info->sym.max_nb_sessions = internals->max_nb_sessions;
	}
}

struct rte_cryptodev_ops ccp_ops = {
		.dev_configure		= ccp_pmd_config,
		.dev_start		= ccp_pmd_start,
		.dev_stop		= ccp_pmd_stop,
		.dev_close		= ccp_pmd_close,

		.stats_get		= NULL,
		.stats_reset		= NULL,

		.dev_infos_get		= ccp_pmd_info_get,

		.queue_pair_setup	= NULL,
		.queue_pair_release	= NULL,
		.queue_pair_start	= NULL,
		.queue_pair_stop	= NULL,
		.queue_pair_count	= NULL,

		.session_get_size	= NULL,
		.session_configure	= NULL,
		.session_clear		= NULL,
};

struct rte_cryptodev_ops *ccp_pmd_ops = &ccp_ops;
