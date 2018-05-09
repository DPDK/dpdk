/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_compressdev_pmd.h>

struct rte_compressdev_ops isal_pmd_ops = {
		.dev_configure		= NULL,
		.dev_start		= NULL,
		.dev_stop		= NULL,
		.dev_close		= NULL,

		.stats_get		= NULL,
		.stats_reset		= NULL,

		.dev_infos_get		= NULL,

		.queue_pair_setup	= NULL,
		.queue_pair_release	= NULL,

		.private_xform_create	= NULL,
		.private_xform_free	= NULL,
};

struct rte_compressdev_ops *isal_compress_pmd_ops = &isal_pmd_ops;
