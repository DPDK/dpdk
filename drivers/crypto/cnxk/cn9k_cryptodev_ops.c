/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>

#include "cn9k_cryptodev.h"
#include "cn9k_cryptodev_ops.h"

struct rte_cryptodev_ops cn9k_cpt_ops = {
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

	/* Symmetric crypto ops */
	.sym_session_get_size = NULL,
	.sym_session_configure = NULL,
	.sym_session_clear = NULL,

	/* Asymmetric crypto ops */
	.asym_session_get_size = NULL,
	.asym_session_configure = NULL,
	.asym_session_clear = NULL,

};
