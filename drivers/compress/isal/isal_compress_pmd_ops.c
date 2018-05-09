/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_common.h>
#include <rte_compressdev_pmd.h>

#include "isal_compress_pmd_private.h"

static const struct rte_compressdev_capabilities isal_pmd_capabilities[] = {
	RTE_COMP_END_OF_CAPABILITIES_LIST()
};

/** Configure device */
static int
isal_comp_pmd_config(struct rte_compressdev *dev __rte_unused,
		struct rte_compressdev_config *config __rte_unused)
{
	return 0;
}

/** Start device */
static int
isal_comp_pmd_start(__rte_unused struct rte_compressdev *dev)
{
	return 0;
}

/** Stop device */
static void
isal_comp_pmd_stop(__rte_unused struct rte_compressdev *dev)
{
}

/** Close device */
static int
isal_comp_pmd_close(struct rte_compressdev *dev)
{
	/* Free private data */
	struct isal_comp_private *internals = dev->data->dev_private;

	rte_mempool_free(internals->priv_xform_mp);
	return 0;
}

/** Get device info */
static void
isal_comp_pmd_info_get(struct rte_compressdev *dev __rte_unused,
		struct rte_compressdev_info *dev_info)
{
	if (dev_info != NULL) {
		dev_info->capabilities = isal_pmd_capabilities;
		dev_info->feature_flags = RTE_COMPDEV_FF_CPU_AVX512 |
				RTE_COMPDEV_FF_CPU_AVX2 |
				RTE_COMPDEV_FF_CPU_AVX |
				RTE_COMPDEV_FF_CPU_SSE;
	}
}

struct rte_compressdev_ops isal_pmd_ops = {
		.dev_configure		= isal_comp_pmd_config,
		.dev_start		= isal_comp_pmd_start,
		.dev_stop		= isal_comp_pmd_stop,
		.dev_close		= isal_comp_pmd_close,

		.stats_get		= NULL,
		.stats_reset		= NULL,

		.dev_infos_get		= isal_comp_pmd_info_get,

		.queue_pair_setup	= NULL,
		.queue_pair_release	= NULL,

		.private_xform_create	= NULL,
		.private_xform_free	= NULL,
};

struct rte_compressdev_ops *isal_compress_pmd_ops = &isal_pmd_ops;
