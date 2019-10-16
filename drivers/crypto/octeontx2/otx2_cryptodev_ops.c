/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#include <rte_cryptodev_pmd.h>

#include "otx2_cryptodev_ops.h"

struct rte_cryptodev_ops otx2_cpt_ops = {
	/* Device control ops */
	.dev_configure = NULL,
	.dev_start = NULL,
	.dev_stop = NULL,
	.dev_close = NULL,
	.dev_infos_get = NULL,

	.stats_get = NULL,
	.stats_reset = NULL,
	.queue_pair_setup = NULL,
	.queue_pair_release = NULL,
	.queue_pair_count = NULL,

	/* Symmetric crypto ops */
	.sym_session_get_size = NULL,
	.sym_session_configure = NULL,
	.sym_session_clear = NULL,
};
