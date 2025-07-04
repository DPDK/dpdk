/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <eal_export.h>
#include "rte_sched_log.h"
#include "rte_pie.h"

RTE_EXPORT_SYMBOL(rte_pie_rt_data_init)
int
rte_pie_rt_data_init(struct rte_pie *pie)
{
	if (pie == NULL) {
		SCHED_LOG(ERR, "%s: Invalid addr for pie", __func__);
		return -EINVAL;
	}

	memset(pie, 0, sizeof(*pie));

	return 0;
}

RTE_EXPORT_SYMBOL(rte_pie_config_init)
int
rte_pie_config_init(struct rte_pie_config *pie_cfg,
	const uint16_t qdelay_ref,
	const uint16_t dp_update_interval,
	const uint16_t max_burst,
	const uint16_t tailq_th)
{
	uint64_t tsc_hz = rte_get_tsc_hz();

	if (pie_cfg == NULL)
		return -1;

	if (qdelay_ref <= 0) {
		SCHED_LOG(ERR,
			"%s: Incorrect value for qdelay_ref", __func__);
		return -EINVAL;
	}

	if (dp_update_interval <= 0) {
		SCHED_LOG(ERR,
			"%s: Incorrect value for dp_update_interval", __func__);
		return -EINVAL;
	}

	if (max_burst <= 0) {
		SCHED_LOG(ERR,
			"%s: Incorrect value for max_burst", __func__);
		return -EINVAL;
	}

	if (tailq_th <= 0) {
		SCHED_LOG(ERR,
			"%s: Incorrect value for tailq_th", __func__);
		return -EINVAL;
	}

	pie_cfg->qdelay_ref = (tsc_hz * qdelay_ref) / 1000;
	pie_cfg->dp_update_interval = (tsc_hz * dp_update_interval) / 1000;
	pie_cfg->max_burst = (tsc_hz * max_burst) / 1000;
	pie_cfg->tailq_th = tailq_th;

	return 0;
}
