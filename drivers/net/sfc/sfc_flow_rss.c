/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Xilinx, Inc.
 */

#include <stdbool.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_flow.h>
#include <rte_tailq.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_flow_rss.h"
#include "sfc_log.h"
#include "sfc_rx.h"

int
sfc_flow_rss_attach(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	struct sfc_flow_rss *flow_rss = &sa->flow_rss;
	int rc;

	sfc_log_init(sa, "entry");

	flow_rss->qid_span_max = encp->enc_rx_scale_indirection_max_nqueues;
	flow_rss->nb_tbl_entries_min = encp->enc_rx_scale_tbl_min_nentries;
	flow_rss->nb_tbl_entries_max = encp->enc_rx_scale_tbl_max_nentries;

	sfc_log_init(sa, "allocate the bounce buffer for indirection entries");
	flow_rss->bounce_tbl = rte_calloc("sfc_flow_rss_bounce_tbl",
					  flow_rss->nb_tbl_entries_max,
					  sizeof(*flow_rss->bounce_tbl), 0);
	if (flow_rss->bounce_tbl == NULL) {
		rc = ENOMEM;
		goto fail;
	}

	TAILQ_INIT(&flow_rss->ctx_list);

	sfc_log_init(sa, "done");

	return 0;

fail:
	sfc_log_init(sa, "failed %d", rc);

	return rc;
}

void
sfc_flow_rss_detach(struct sfc_adapter *sa)
{
	struct sfc_flow_rss *flow_rss = &sa->flow_rss;

	sfc_log_init(sa, "entry");

	sfc_log_init(sa, "free the bounce buffer for indirection entries");
	rte_free(flow_rss->bounce_tbl);

	sfc_log_init(sa, "done");
}

int
sfc_flow_rss_parse_conf(struct sfc_adapter *sa,
			const struct rte_flow_action_rss *in,
			struct sfc_flow_rss_conf *out, uint16_t *sw_qid_minp)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	const struct sfc_flow_rss *flow_rss = &sa->flow_rss;
	const struct sfc_rss *ethdev_rss = &sas->rss;
	uint16_t sw_qid_min;
	uint16_t sw_qid_max;
	const uint8_t *key;
	unsigned int i;
	int rc;

	if (in->level) {
		/*
		 * The caller demands that RSS hash be computed
		 * within the given encapsulation frame / level.
		 * Per flow control for that is not implemented.
		 */
		sfc_err(sa, "flow-rss: parse: 'level' must be 0");
		return EINVAL;
	}

	if (in->types != 0) {
		rc = sfc_rx_hf_rte_to_efx(sa, in->types,
					  &out->efx_hash_types);
		if (rc != 0) {
			sfc_err(sa, "flow-rss: parse: failed to process 'types'");
			return rc;
		}
	} else {
		sfc_dbg(sa, "flow-rss: parse: 'types' is 0; proceeding with ethdev setting");
		out->efx_hash_types = ethdev_rss->hash_types;
	}

	if (in->key_len != 0) {
		if (in->key_len != sizeof(out->key)) {
			sfc_err(sa, "flow-rss: parse: 'key_len' must be either %zu or 0",
				sizeof(out->key));
			return EINVAL;
		}

		if (in->key == NULL) {
			sfc_err(sa, "flow-rss: parse: 'key' is NULL");
			return EINVAL;
		}

		key = in->key;
	} else {
		sfc_dbg(sa, "flow-rss: parse: 'key_len' is 0; proceeding with ethdev key");
		key = ethdev_rss->key;
	}

	rte_memcpy(out->key, key, sizeof(out->key));

	switch (in->func) {
	case RTE_ETH_HASH_FUNCTION_DEFAULT:
		/*
		 * DEFAULT means that conformance to a specific
		 * hash algorithm is a don't care to the caller.
		 * The driver can pick the one it deems optimal.
		 */
		break;
	case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
		if (ethdev_rss->hash_alg != EFX_RX_HASHALG_TOEPLITZ) {
			sfc_err(sa, "flow-rss: parse: 'func' TOEPLITZ is unavailable; use DEFAULT");
			return EINVAL;
		}
		break;
	default:
		sfc_err(sa, "flow-rss: parse: 'func' #%d is unsupported", in->func);
		return EINVAL;
	}

	out->rte_hash_function = in->func;

	if (in->queue_num == 0) {
		sfc_err(sa, "flow-rss: parse: 'queue_num' is 0; MIN=1");
		return EINVAL;
	}

	if (in->queue_num > flow_rss->nb_tbl_entries_max) {
		sfc_err(sa, "flow-rss: parse: 'queue_num' is too large; MAX=%u",
			flow_rss->nb_tbl_entries_max);
		return EINVAL;
	}

	if (in->queue == NULL) {
		sfc_err(sa, "flow-rss: parse: 'queue' is NULL");
		return EINVAL;
	}

	sw_qid_min = sas->ethdev_rxq_count - 1;
	sw_qid_max = 0;

	out->nb_qid_offsets = 0;

	for (i = 0; i < in->queue_num; ++i) {
		uint16_t sw_qid = in->queue[i];

		if (sw_qid >= sas->ethdev_rxq_count) {
			sfc_err(sa, "flow-rss: parse: queue=%u does not exist",
				sw_qid);
			return EINVAL;
		}

		if (sw_qid < sw_qid_min)
			sw_qid_min = sw_qid;

		if (sw_qid > sw_qid_max)
			sw_qid_max = sw_qid;

		if (sw_qid != in->queue[0] + i)
			out->nb_qid_offsets = in->queue_num;
	}

	out->qid_span = sw_qid_max - sw_qid_min + 1;

	if (out->qid_span > flow_rss->qid_span_max) {
		sfc_err(sa, "flow-rss: parse: queue ID span %u is too large; MAX=%u",
			out->qid_span, flow_rss->qid_span_max);
		return EINVAL;
	}

	if (sw_qid_minp != NULL)
		*sw_qid_minp = sw_qid_min;

	return 0;
}

struct sfc_flow_rss_ctx *
sfc_flow_rss_ctx_reuse(struct sfc_adapter *sa,
		       const struct sfc_flow_rss_conf *conf,
		       uint16_t sw_qid_min, const uint16_t *sw_qids)
{
	struct sfc_flow_rss *flow_rss = &sa->flow_rss;
	static struct sfc_flow_rss_ctx *ctx;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	TAILQ_FOREACH(ctx, &flow_rss->ctx_list, entries) {
		if (memcmp(&ctx->conf, conf, sizeof(*conf)) != 0)
			continue;

		if (conf->nb_qid_offsets != 0) {
			bool match_confirmed = true;
			unsigned int i;

			for (i = 0; i < conf->nb_qid_offsets; ++i) {
				uint16_t qid_offset = sw_qids[i] - sw_qid_min;

				if (ctx->qid_offsets[i] != qid_offset) {
					match_confirmed = false;
					break;
				}
			}

			if (!match_confirmed)
				continue;
		}

		sfc_dbg(sa, "flow-rss: reusing ctx=%p", ctx);
		++(ctx->refcnt);
		return ctx;
	}

	return NULL;
}

int
sfc_flow_rss_ctx_add(struct sfc_adapter *sa,
		     const struct sfc_flow_rss_conf *conf, uint16_t sw_qid_min,
		     const uint16_t *sw_qids, struct sfc_flow_rss_ctx **ctxp)
{
	struct sfc_flow_rss *flow_rss = &sa->flow_rss;
	struct sfc_flow_rss_ctx *ctx;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	ctx = rte_zmalloc("sfc_flow_rss_ctx", sizeof(*ctx), 0);
	if (ctx == NULL)
		return ENOMEM;

	if (conf->nb_qid_offsets != 0) {
		unsigned int i;

		ctx->qid_offsets = rte_calloc("sfc_flow_rss_ctx_qid_offsets",
					      conf->nb_qid_offsets,
					      sizeof(*ctx->qid_offsets), 0);
		if (ctx->qid_offsets == NULL) {
			rte_free(ctx);
			return ENOMEM;
		}

		for (i = 0; i < conf->nb_qid_offsets; ++i)
			ctx->qid_offsets[i] = sw_qids[i] - sw_qid_min;
	}

	ctx->conf = *conf;
	ctx->refcnt = 1;

	TAILQ_INSERT_TAIL(&flow_rss->ctx_list, ctx, entries);

	*ctxp = ctx;

	sfc_dbg(sa, "flow-rss: added ctx=%p", ctx);

	return 0;
}

void
sfc_flow_rss_ctx_del(struct sfc_adapter *sa, struct sfc_flow_rss_ctx *ctx)
{
	struct sfc_flow_rss *flow_rss = &sa->flow_rss;

	if (ctx == NULL)
		return;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (ctx->dummy)
		return;

	SFC_ASSERT(ctx->refcnt != 0);

	--(ctx->refcnt);

	if (ctx->refcnt != 0)
		return;

	if (ctx->nic_handle_refcnt != 0) {
		sfc_err(sa, "flow-rss: deleting ctx=%p abandons its NIC resource: handle=0x%08x, refcnt=%u",
			ctx, ctx->nic_handle, ctx->nic_handle_refcnt);
	}

	TAILQ_REMOVE(&flow_rss->ctx_list, ctx, entries);
	rte_free(ctx->qid_offsets);
	rte_free(ctx);

	sfc_dbg(sa, "flow-rss: deleted ctx=%p", ctx);
}

static int
sfc_flow_rss_ctx_program_tbl(struct sfc_adapter *sa,
			     unsigned int nb_tbl_entries,
			     const struct sfc_flow_rss_ctx *ctx)
{
	const struct sfc_flow_rss_conf *conf = &ctx->conf;
	unsigned int *tbl = sa->flow_rss.bounce_tbl;
	unsigned int i;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (nb_tbl_entries == 0)
		return 0;

	if (conf->nb_qid_offsets != 0) {
		SFC_ASSERT(ctx->qid_offsets != NULL);

		for (i = 0; i < nb_tbl_entries; ++i)
			tbl[i] = ctx->qid_offsets[i % conf->nb_qid_offsets];
	} else {
		for (i = 0; i < nb_tbl_entries; ++i)
			tbl[i] = i % conf->qid_span;
	}

	return efx_rx_scale_tbl_set(sa->nic, ctx->nic_handle,
				    tbl, nb_tbl_entries);
}

int
sfc_flow_rss_ctx_program(struct sfc_adapter *sa, struct sfc_flow_rss_ctx *ctx)
{
	efx_rx_scale_context_type_t ctx_type = EFX_RX_SCALE_EXCLUSIVE;
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	const struct sfc_flow_rss *flow_rss = &sa->flow_rss;
	struct sfc_rss *ethdev_rss = &sas->rss;
	struct sfc_flow_rss_conf *conf;
	bool allocation_done = B_FALSE;
	unsigned int nb_qid_offsets;
	unsigned int nb_tbl_entries;
	int rc;

	if (ctx == NULL)
		return 0;

	conf = &ctx->conf;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (conf->nb_qid_offsets != 0)
		nb_qid_offsets = conf->nb_qid_offsets;
	else
		nb_qid_offsets = conf->qid_span;

	if (!RTE_IS_POWER_OF_2(nb_qid_offsets)) {
		/*
		 * Most likely, it pays to enlarge the indirection
		 * table to facilitate better distribution quality.
		 */
		nb_qid_offsets = flow_rss->nb_tbl_entries_max;
	}

	nb_tbl_entries = RTE_MAX(flow_rss->nb_tbl_entries_min, nb_qid_offsets);

	if (conf->rte_hash_function == RTE_ETH_HASH_FUNCTION_DEFAULT &&
	    conf->nb_qid_offsets == 0 &&
	    conf->qid_span <= encp->enc_rx_scale_even_spread_max_nqueues) {
		/*
		 * Conformance to a specific hash algorithm is a don't care to
		 * the user. The queue array is contiguous and ascending. That
		 * means that the even spread context may be requested here in
		 * order to avoid wasting precious indirection table resources.
		 */
		ctx_type = EFX_RX_SCALE_EVEN_SPREAD;
		nb_tbl_entries = 0;
	}

	if (ctx->nic_handle_refcnt == 0) {
		rc = efx_rx_scale_context_alloc_v2(sa->nic, ctx_type,
						   conf->qid_span,
						   nb_tbl_entries,
						   &ctx->nic_handle);
		if (rc != 0) {
			sfc_err(sa, "flow-rss: failed to allocate NIC resource for ctx=%p: type=%d, qid_span=%u, nb_tbl_entries=%u; rc=%d",
				ctx, ctx_type, conf->qid_span, nb_tbl_entries, rc);
			goto fail;
		}

		sfc_dbg(sa, "flow-rss: allocated NIC resource for ctx=%p: type=%d, qid_span=%u, nb_tbl_entries=%u; handle=0x%08x",
			ctx, ctx_type, conf->qid_span, nb_tbl_entries,
			ctx->nic_handle);

		++(ctx->nic_handle_refcnt);
		allocation_done = B_TRUE;
	} else {
		++(ctx->nic_handle_refcnt);
		return 0;
	}

	rc = efx_rx_scale_mode_set(sa->nic, ctx->nic_handle,
				   ethdev_rss->hash_alg,
				   (ctx->dummy) ? ethdev_rss->hash_types :
						  conf->efx_hash_types,
				   B_TRUE);
	if (rc != 0) {
		sfc_err(sa, "flow-rss: failed to configure hash for ctx=%p: efx_hash_alg=%d, efx_hash_types=0x%08x; rc=%d",
			ctx, ethdev_rss->hash_alg,
			(ctx->dummy) ? ethdev_rss->hash_types :
				       conf->efx_hash_types,
			rc);
		goto fail;
	}

	rc = efx_rx_scale_key_set(sa->nic, ctx->nic_handle,
				  (ctx->dummy) ? ethdev_rss->key : conf->key,
				  RTE_DIM(conf->key));
	if (rc != 0) {
		sfc_err(sa, "flow-rss: failed to set key for ctx=%p; rc=%d",
			ctx, rc);
		goto fail;
	}

	rc = sfc_flow_rss_ctx_program_tbl(sa, nb_tbl_entries, ctx);
	if (rc != 0) {
		sfc_err(sa, "flow-rss: failed to program table for ctx=%p: nb_tbl_entries=%u; rc=%d",
			ctx, nb_tbl_entries, rc);
		goto fail;
	}

	return 0;

fail:
	if (allocation_done)
		sfc_flow_rss_ctx_terminate(sa, ctx);

	return rc;
}

void
sfc_flow_rss_ctx_terminate(struct sfc_adapter *sa, struct sfc_flow_rss_ctx *ctx)
{
	if (ctx == NULL)
		return;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	SFC_ASSERT(ctx->nic_handle_refcnt != 0);
	--(ctx->nic_handle_refcnt);

	if (ctx->nic_handle_refcnt == 0) {
		int rc;

		rc = efx_rx_scale_context_free(sa->nic, ctx->nic_handle);
		if (rc != 0) {
			sfc_err(sa, "flow-rss: failed to release NIC resource for ctx=%p: handle=0x%08x; rc=%d",
				ctx, ctx->nic_handle, rc);

			sfc_warn(sa, "flow-rss: proceeding despite the prior error");
		}

		sfc_dbg(sa, "flow-rss: released NIC resource for ctx=%p; rc=%d",
			ctx, rc);
	}
}
