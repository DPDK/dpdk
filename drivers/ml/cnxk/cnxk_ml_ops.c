/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include "cnxk_ml_ops.h"

struct rte_ml_dev_ops cnxk_ml_ops = {
	/* Device control ops */
	.dev_info_get = cn10k_ml_dev_info_get,
	.dev_configure = cn10k_ml_dev_configure,
	.dev_close = cn10k_ml_dev_close,
	.dev_start = cn10k_ml_dev_start,
	.dev_stop = cn10k_ml_dev_stop,
	.dev_dump = cn10k_ml_dev_dump,
	.dev_selftest = cn10k_ml_dev_selftest,

	/* Queue-pair handling ops */
	.dev_queue_pair_setup = cn10k_ml_dev_queue_pair_setup,
	.dev_queue_pair_release = cn10k_ml_dev_queue_pair_release,

	/* Stats ops */
	.dev_stats_get = cn10k_ml_dev_stats_get,
	.dev_stats_reset = cn10k_ml_dev_stats_reset,
	.dev_xstats_names_get = cn10k_ml_dev_xstats_names_get,
	.dev_xstats_by_name_get = cn10k_ml_dev_xstats_by_name_get,
	.dev_xstats_get = cn10k_ml_dev_xstats_get,
	.dev_xstats_reset = cn10k_ml_dev_xstats_reset,

	/* Model ops */
	.model_load = cn10k_ml_model_load,
	.model_unload = cn10k_ml_model_unload,
	.model_start = cn10k_ml_model_start,
	.model_stop = cn10k_ml_model_stop,
	.model_info_get = cn10k_ml_model_info_get,
	.model_params_update = cn10k_ml_model_params_update,

	/* I/O ops */
	.io_quantize = cn10k_ml_io_quantize,
	.io_dequantize = cn10k_ml_io_dequantize,
};
