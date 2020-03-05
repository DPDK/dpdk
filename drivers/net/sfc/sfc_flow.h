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

/*
 * The maximum number of fully elaborated hardware filter specifications
 * which can be produced from a template by means of multiplication, if
 * missing match flags are needed to be taken into account
 */
#define SF_FLOW_SPEC_NB_FILTERS_MAX 8

/* RSS configuration storage */
struct sfc_flow_rss {
	unsigned int	rxq_hw_index_min;
	unsigned int	rxq_hw_index_max;
	unsigned int	rss_hash_types;
	uint8_t		rss_key[EFX_RSS_KEY_SIZE];
	unsigned int	rss_tbl[EFX_RSS_TBL_SIZE];
};

/* Flow engines supported by the implementation */
enum sfc_flow_spec_type {
	SFC_FLOW_SPEC_FILTER = 0,

	SFC_FLOW_SPEC_NTYPES
};

/* VNIC-specific flow specification */
struct sfc_flow_spec_filter {
	/* partial specification from flow rule */
	efx_filter_spec_t template;
	/* fully elaborated hardware filters specifications */
	efx_filter_spec_t filters[SF_FLOW_SPEC_NB_FILTERS_MAX];
	/* number of complete specifications */
	unsigned int count;
	/* RSS toggle */
	boolean_t rss;
	/* RSS configuration */
	struct sfc_flow_rss rss_conf;
};

/* Flow specification */
struct sfc_flow_spec {
	/* Flow specification type (engine-based) */
	enum sfc_flow_spec_type type;

	RTE_STD_C11
	union {
		/* Filter-based (VNIC level flows) specification */
		struct sfc_flow_spec_filter filter;
	};
};

/* PMD-specific definition of the opaque type from rte_flow.h */
struct rte_flow {
	struct sfc_flow_spec spec;	/* flow specification */
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
