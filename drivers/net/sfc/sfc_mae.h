/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_MAE_H
#define _SFC_MAE_H

#include <stdbool.h>

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Action set registry entry */
struct sfc_mae_action_set {
	TAILQ_ENTRY(sfc_mae_action_set)	entries;
	unsigned int			refcnt;
	efx_mae_actions_t		*spec;
};

TAILQ_HEAD(sfc_mae_action_sets, sfc_mae_action_set);

/** Options for MAE support status */
enum sfc_mae_status {
	SFC_MAE_STATUS_UNKNOWN = 0,
	SFC_MAE_STATUS_UNSUPPORTED,
	SFC_MAE_STATUS_SUPPORTED
};

struct sfc_mae {
	/** NIC support for MAE status */
	enum sfc_mae_status		status;
	/** Priority level limit for MAE action rules */
	unsigned int			nb_action_rule_prios_max;
	/** Action set registry */
	struct sfc_mae_action_sets	action_sets;
};

struct sfc_adapter;
struct sfc_flow_spec;

struct sfc_mae_parse_ctx {
	efx_mae_match_spec_t		*match_spec_action;
};

int sfc_mae_attach(struct sfc_adapter *sa);
void sfc_mae_detach(struct sfc_adapter *sa);
sfc_flow_cleanup_cb_t sfc_mae_flow_cleanup;
int sfc_mae_rule_parse_pattern(struct sfc_adapter *sa,
			       const struct rte_flow_item pattern[],
			       struct sfc_flow_spec_mae *spec,
			       struct rte_flow_error *error);
int sfc_mae_rule_parse_actions(struct sfc_adapter *sa,
			       const struct rte_flow_action actions[],
			       struct sfc_mae_action_set **action_setp,
			       struct rte_flow_error *error);
sfc_flow_verify_cb_t sfc_mae_flow_verify;

#ifdef __cplusplus
}
#endif
#endif /* _SFC_MAE_H */
