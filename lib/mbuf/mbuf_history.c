/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 NVIDIA Corporation & Affiliates
 */

#include <rte_errno.h>
#include <eal_export.h>
#include <rte_bitops.h>
#include <rte_mempool.h>

#include "rte_mbuf_history.h"
#include "rte_mbuf_dyn.h"
#include "rte_mbuf.h"
#include "mbuf_log.h"

/* Dynamic field offset */
RTE_EXPORT_SYMBOL(rte_mbuf_history_field_offset);
int rte_mbuf_history_field_offset = -1;

#ifdef RTE_MBUF_HISTORY_DEBUG

#define HISTORY_LAST_MASK (RTE_BIT64(RTE_MBUF_HISTORY_BITS) - 1)

/* Dynamic field definition for mbuf history */
static const struct rte_mbuf_dynfield mbuf_dynfield_history = {
	.name = RTE_MBUF_DYNFIELD_HISTORY_NAME,
	.size = sizeof(rte_mbuf_history_t),
	.align = RTE_ALIGN(sizeof(rte_mbuf_history_t), 8),
};

/* Context structure for statistics counting and history printing */
struct count_and_print_ctx {
	uint64_t *stats;
	FILE *f;
};

static uint64_t
mbuf_history_get(const struct rte_mbuf *m)
{
	if (rte_mbuf_history_field_offset < 0 || m == NULL)
		return 0;

	rte_mbuf_history_t *history = RTE_MBUF_DYNFIELD(m,
			rte_mbuf_history_field_offset, rte_mbuf_history_t *);

	return rte_atomic_load_explicit(history, rte_memory_order_acquire);
}

static int
mbuf_history_print(FILE *f, const struct rte_mbuf *m, uint64_t history)
{
	return fprintf(f, "mbuf %p: %016" PRIx64 "\n", m, history);
}

static void
mbuf_history_count_stats_and_print(struct rte_mempool *mp __rte_unused,
		void *opaque, void *obj, unsigned obj_idx __rte_unused)
{
	struct count_and_print_ctx *ctx = (struct count_and_print_ctx *)opaque;
	struct rte_mbuf *m = (struct rte_mbuf *)obj;
	uint64_t history;
	enum rte_mbuf_history_op last_op;

	if (obj == NULL || ctx == NULL || ctx->stats == NULL || ctx->f == NULL)
		return;

	history = mbuf_history_get(m);
	/* Extract the most recent operation */
	last_op = history & HISTORY_LAST_MASK;
	RTE_ASSERT(last_op < RTE_MBUF_HISTORY_OP_MAX);
	RTE_BUILD_BUG_ON(HISTORY_LAST_MASK + 1 < RTE_MBUF_HISTORY_OP_MAX);

	ctx->stats[last_op]++;
	ctx->stats[RTE_MBUF_HISTORY_OP_MAX]++; /* total */

	if (history != 0)
		mbuf_history_print(ctx->f, m, history);
}

static void
mbuf_history_get_stats(struct rte_mempool *mp, FILE *f)
{
	uint64_t stats[RTE_MBUF_HISTORY_OP_MAX + 1] = {0};
	struct count_and_print_ctx ctx = {
		.stats = stats,
		.f = f
	};

	if (mp->elt_size < sizeof(struct rte_mbuf)) {
		MBUF_LOG(ERR, "Invalid mempool element size (less than mbuf)");
		return;
	}

	if (f == NULL)
		return;

	/* Output mempool header */
	fprintf(f, "mempool <%s>@%p\n", mp->name, mp);

	/* Single pass: collect statistics and print mbuf history */
	rte_mempool_obj_iter(mp, mbuf_history_count_stats_and_print, &ctx);

	/* Calculate total allocated mbufs */
	uint64_t total_allocated = 0;
	for (enum rte_mbuf_history_op op = RTE_MBUF_HISTORY_OP_LIB_ALLOC;
			op < RTE_MBUF_HISTORY_OP_MAX; op++)
		total_allocated += stats[op];

	/* Print statistics summary */
	fprintf(f, "  populated = %u\n", mp->populated_size);
	fprintf(f, "  never allocated = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_NEVER]);
	fprintf(f, "  lib free = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_LIB_FREE]);
	fprintf(f, "  PMD free = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_PMD_FREE]);
	fprintf(f, "  app free = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_APP_FREE]);
	fprintf(f, "  allocated = %" PRIu64 "\n", total_allocated);
	fprintf(f, "  lib alloc = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_LIB_ALLOC]);
	fprintf(f, "  PMD alloc = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_PMD_ALLOC]);
	fprintf(f, "  app alloc = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_APP_ALLOC]);
	fprintf(f, "  Rx = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_RX]);
	fprintf(f, "  Tx = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_TX]);
	fprintf(f, "  Tx prep = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_TX_PREP]);
	fprintf(f, "  Tx busy = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_TX_BUSY]);
	fprintf(f, "  enqueue = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_ENQUEUE]);
	fprintf(f, "  dequeue = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_DEQUEUE]);
	fprintf(f, "  user defined 1 = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_USR1]);
	fprintf(f, "  user defined 2 = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_USR2]);
	fprintf(f, "  counted total = %" PRIu64 "\n", stats[RTE_MBUF_HISTORY_OP_MAX]);
}

static void
mbuf_history_get_stats_walking(struct rte_mempool *mp, void *arg)
{
	if (mp->elt_size < sizeof(struct rte_mbuf))
		return; /* silently ignore while walking in all mempools */

	mbuf_history_get_stats(mp, arg);
}

#endif /* RTE_MBUF_HISTORY_DEBUG */

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mbuf_history_dump, 25.11)
void rte_mbuf_history_dump(FILE *f, const struct rte_mbuf *m)
{
#ifndef RTE_MBUF_HISTORY_DEBUG
	RTE_SET_USED(f);
	RTE_SET_USED(m);
	MBUF_LOG(INFO, "mbuf history recorder is not enabled");
#else
	if (f == NULL) {
		MBUF_LOG(ERR, "Invalid mbuf dump file");
		return;
	}
	if (m == NULL) {
		MBUF_LOG(ERR, "Invalid mbuf pointer");
		return;
	}
	if (rte_mbuf_history_field_offset < 0) {
		MBUF_LOG(WARNING, "mbuf history not initialized, call rte_mbuf_history_init()");
		return;
	}
	mbuf_history_print(f, m, mbuf_history_get(m));
#endif
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mbuf_history_dump_mempool, 25.11)
void rte_mbuf_history_dump_mempool(FILE *f, struct rte_mempool *mp)
{
#ifndef RTE_MBUF_HISTORY_DEBUG
	RTE_SET_USED(f);
	RTE_SET_USED(mp);
	MBUF_LOG(INFO, "mbuf history recorder is not enabled");
#else
	if (f == NULL) {
		MBUF_LOG(ERR, "Invalid mbuf dump file");
		return;
	}
	if (mp == NULL) {
		MBUF_LOG(ERR, "Invalid mempool pointer");
		return;
	}
	if (rte_mbuf_history_field_offset < 0) {
		MBUF_LOG(WARNING, "mbuf history not initialized, call rte_mbuf_history_init()");
		return;
	}
	mbuf_history_get_stats(mp, f);
#endif
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mbuf_history_dump_all, 25.11)
void rte_mbuf_history_dump_all(FILE *f)
{
#ifndef RTE_MBUF_HISTORY_DEBUG
	RTE_SET_USED(f);
	MBUF_LOG(INFO, "mbuf history recorder is not enabled");
#else
	if (f == NULL) {
		MBUF_LOG(ERR, "Invalid mbuf dump file");
		return;
	}
	if (rte_mbuf_history_field_offset < 0) {
		MBUF_LOG(WARNING, "mbuf history not initialized, call rte_mbuf_history_init()");
		return;
	}
	fprintf(f, "mbuf history statistics:\n");
	rte_mempool_walk(mbuf_history_get_stats_walking, f);
#endif
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_mbuf_history_init, 25.11)
void rte_mbuf_history_init(void)
{
#ifdef RTE_MBUF_HISTORY_DEBUG
	if (rte_mbuf_history_field_offset >= 0) {
		/* already initialized */
		return;
	}

	rte_mbuf_history_field_offset = rte_mbuf_dynfield_register(&mbuf_dynfield_history);
	if (rte_mbuf_history_field_offset < 0) {
		MBUF_LOG(ERR, "Failed to register mbuf history dynamic field: %s\n",
			rte_strerror(rte_errno));
	}
#endif
}
