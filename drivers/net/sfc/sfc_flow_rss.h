/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Xilinx, Inc.
 */

#ifndef _SFC_FLOW_RSS_H
#define _SFC_FLOW_RSS_H

#include <stdbool.h>
#include <stdint.h>

#include <rte_flow.h>
#include <rte_tailq.h>

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sfc_flow_rss_conf {
	uint8_t				key[EFX_RSS_KEY_SIZE];
	enum rte_eth_hash_function	rte_hash_function;
	efx_rx_hash_type_t		efx_hash_types;
	unsigned int			nb_qid_offsets;
	unsigned int			qid_span;
};

struct sfc_flow_rss_ctx {
	TAILQ_ENTRY(sfc_flow_rss_ctx)	entries;

	unsigned int			refcnt;
	bool				dummy;

	unsigned int			nic_handle_refcnt;
	uint32_t			nic_handle;

	struct sfc_flow_rss_conf	conf;

	uint16_t			*qid_offsets;
};

TAILQ_HEAD(sfc_flow_rss_ctx_list, sfc_flow_rss_ctx);

struct sfc_flow_rss {
	unsigned int			nb_tbl_entries_min;
	unsigned int			nb_tbl_entries_max;
	unsigned int			qid_span_max;

	unsigned int			*bounce_tbl; /* MAX */

	struct sfc_flow_rss_ctx_list	ctx_list;
};

struct sfc_adapter;

int sfc_flow_rss_attach(struct sfc_adapter *sa);

void sfc_flow_rss_detach(struct sfc_adapter *sa);

int sfc_flow_rss_parse_conf(struct sfc_adapter *sa,
			    const struct rte_flow_action_rss *in,
			    struct sfc_flow_rss_conf *out,
			    uint16_t *sw_qid_minp);

struct sfc_flow_rss_ctx *sfc_flow_rss_ctx_reuse(struct sfc_adapter *sa,
				const struct sfc_flow_rss_conf *conf,
				uint16_t sw_qid_min, const uint16_t *sw_qids);

int sfc_flow_rss_ctx_add(struct sfc_adapter *sa,
			 const struct sfc_flow_rss_conf *conf,
			 uint16_t sw_qid_min, const uint16_t *sw_qids,
			 struct sfc_flow_rss_ctx **ctxp);

void sfc_flow_rss_ctx_del(struct sfc_adapter *sa, struct sfc_flow_rss_ctx *ctx);

int sfc_flow_rss_ctx_program(struct sfc_adapter *sa,
			     struct sfc_flow_rss_ctx *ctx);

void sfc_flow_rss_ctx_terminate(struct sfc_adapter *sa,
				struct sfc_flow_rss_ctx *ctx);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_FLOW_RSS_H */
