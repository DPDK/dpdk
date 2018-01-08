/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2017-2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_FLOW_H
#define _SFC_FLOW_H

#include <rte_tailq.h>
#include <rte_flow_driver.h>

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

#if EFSYS_OPT_RX_SCALE
/* RSS configuration storage */
struct sfc_flow_rss {
	unsigned int	rxq_hw_index_min;
	unsigned int	rxq_hw_index_max;
	unsigned int	rss_hash_types;
	uint8_t		rss_key[EFX_RSS_KEY_SIZE];
	unsigned int	rss_tbl[EFX_RSS_TBL_SIZE];
};
#endif /* EFSYS_OPT_RX_SCALE */

/* PMD-specific definition of the opaque type from rte_flow.h */
struct rte_flow {
	efx_filter_spec_t spec;		/* filter specification */
#if EFSYS_OPT_RX_SCALE
	boolean_t rss;			/* RSS toggle */
	struct sfc_flow_rss rss_conf;	/* RSS configuration */
#endif /* EFSYS_OPT_RX_SCALE */
	TAILQ_ENTRY(rte_flow) entries;	/* flow list entries */
};

TAILQ_HEAD(sfc_flow_list, rte_flow);

extern const struct rte_flow_ops sfc_flow_ops;

struct sfc_adapter;

void sfc_flow_init(struct sfc_adapter *sa);
void sfc_flow_fini(struct sfc_adapter *sa);
int sfc_flow_start(struct sfc_adapter *sa);
void sfc_flow_stop(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_FLOW_H */
