/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <rte_cryptodev_pmd.h>

struct rte_cryptodev_ops ccp_ops = {
		.dev_configure		= NULL,
		.dev_start		= NULL,
		.dev_stop		= NULL,
		.dev_close		= NULL,

		.stats_get		= NULL,
		.stats_reset		= NULL,

		.dev_infos_get		= NULL,

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
