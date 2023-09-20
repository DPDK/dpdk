/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <stdlib.h>

#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>

#include "cperf_test_verify.h"
#include "cperf_ops.h"
#include "cperf_test_common.h"

struct cperf_verify_ctx {
	uint8_t dev_id;
	uint16_t qp_id;
	uint8_t lcore_id;

	struct rte_mempool *pool;

	void *sess;

	cperf_populate_ops_t populate_ops;

	uint32_t src_buf_offset;
	uint32_t dst_buf_offset;

	const struct cperf_options *options;
	const struct cperf_test_vector *test_vector;
};

struct cperf_op_result {
	enum rte_crypto_op_status status;
};

static void
cperf_verify_test_free(struct cperf_verify_ctx *ctx)
{
	if (ctx == NULL)
		return;

	if (ctx->sess != NULL) {
		if (ctx->options->op_type == CPERF_ASYM_MODEX)
			rte_cryptodev_asym_session_free(ctx->dev_id, ctx->sess);
#ifdef RTE_LIB_SECURITY
		else if (ctx->options->op_type == CPERF_PDCP ||
			 ctx->options->op_type == CPERF_DOCSIS ||
			 ctx->options->op_type == CPERF_IPSEC) {
			void *sec_ctx = rte_cryptodev_get_sec_ctx(ctx->dev_id);

			rte_security_session_destroy(sec_ctx, ctx->sess);
		}
#endif
		else
			rte_cryptodev_sym_session_free(ctx->dev_id, ctx->sess);
	}

	rte_mempool_free(ctx->pool);
	rte_free(ctx);
}

void *
cperf_verify_test_constructor(struct rte_mempool *sess_mp,
		uint8_t dev_id, uint16_t qp_id,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		const struct cperf_op_fns *op_fns)
{
	struct cperf_verify_ctx *ctx = NULL;

	ctx = rte_malloc(NULL, sizeof(struct cperf_verify_ctx), 0);
	if (ctx == NULL)
		goto err;

	ctx->dev_id = dev_id;
	ctx->qp_id = qp_id;

	ctx->populate_ops = op_fns->populate_ops;
	ctx->options = options;
	ctx->test_vector = test_vector;

	/* IV goes at the end of the crypto operation */
	uint16_t iv_offset = sizeof(struct rte_crypto_op) +
		sizeof(struct rte_crypto_sym_op);

	ctx->sess = op_fns->sess_create(sess_mp, dev_id, options,
			test_vector, iv_offset);
	if (ctx->sess == NULL)
		goto err;

	if (cperf_alloc_common_memory(options, test_vector, dev_id, qp_id, 0,
			&ctx->src_buf_offset, &ctx->dst_buf_offset,
			&ctx->pool) < 0)
		goto err;

	return ctx;
err:
	cperf_verify_test_free(ctx);

	return NULL;
}

static int
cperf_verify_op(struct rte_crypto_op *op,
		const struct cperf_options *options,
		const struct cperf_test_vector *vector)
{
	const struct rte_mbuf *m;
	uint32_t len;
	uint16_t nb_segs;
	uint8_t *data;
	uint32_t cipher_offset, auth_offset;
	uint8_t	cipher, auth;
	int res = 0;

	if (op->status != RTE_CRYPTO_OP_STATUS_SUCCESS)
		return 1;

	if (op->sym->m_dst)
		m = op->sym->m_dst;
	else
		m = op->sym->m_src;
	nb_segs = m->nb_segs;
	len = 0;
	while (m && nb_segs != 0) {
		len += m->data_len;
		m = m->next;
		nb_segs--;
	}

	data = rte_malloc(NULL, len, 0);
	if (data == NULL)
		return 1;

	if (op->sym->m_dst)
		m = op->sym->m_dst;
	else
		m = op->sym->m_src;
	nb_segs = m->nb_segs;
	len = 0;
	while (m && nb_segs != 0) {
		memcpy(data + len, rte_pktmbuf_mtod(m, uint8_t *),
				m->data_len);
		len += m->data_len;
		m = m->next;
		nb_segs--;
	}

	switch (options->op_type) {
	case CPERF_CIPHER_ONLY:
		cipher = 1;
		cipher_offset = 0;
		auth = 0;
		auth_offset = 0;
		break;
	case CPERF_CIPHER_THEN_AUTH:
		cipher = 1;
		cipher_offset = 0;
		auth = 1;
		auth_offset = options->test_buffer_size;
		break;
	case CPERF_AUTH_ONLY:
		cipher = 0;
		cipher_offset = 0;
		auth = 1;
		auth_offset = options->test_buffer_size;
		break;
	case CPERF_AUTH_THEN_CIPHER:
		cipher = 1;
		cipher_offset = 0;
		auth = 1;
		auth_offset = options->test_buffer_size;
		break;
	case CPERF_AEAD:
		cipher = 1;
		cipher_offset = 0;
		auth = 1;
		auth_offset = options->test_buffer_size;
		break;
	default:
		res = 1;
		goto out;
	}

	if (cipher == 1) {
		if (options->cipher_op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
			res += memcmp(data + cipher_offset,
					vector->ciphertext.data,
					options->test_buffer_size);
		else
			res += memcmp(data + cipher_offset,
					vector->plaintext.data,
					options->test_buffer_size);
	}

	if (auth == 1) {
		if (options->auth_op == RTE_CRYPTO_AUTH_OP_GENERATE)
			res += memcmp(data + auth_offset,
					vector->digest.data,
					options->digest_sz);
	}

out:
	rte_free(data);
	return !!res;
}

int
cperf_verify_test_runner(void *test_ctx)
{
	struct cperf_verify_ctx *ctx = test_ctx;

	uint64_t ops_enqd = 0, ops_enqd_total = 0, ops_enqd_failed = 0;
	uint64_t ops_deqd = 0, ops_deqd_total = 0, ops_deqd_failed = 0;
	uint64_t ops_failed = 0;

	static uint16_t display_once;

	uint64_t i;
	uint16_t ops_unused = 0;
	uint32_t imix_idx = 0;

	struct rte_crypto_op *ops[ctx->options->max_burst_size];
	struct rte_crypto_op *ops_processed[ctx->options->max_burst_size];

	uint32_t lcore = rte_lcore_id();

#ifdef CPERF_LINEARIZATION_ENABLE
	struct rte_cryptodev_info dev_info;
	int linearize = 0;

	/* Check if source mbufs require coalescing */
	if (ctx->options->segment_sz < ctx->options->max_buffer_size) {
		rte_cryptodev_info_get(ctx->dev_id, &dev_info);
		if ((dev_info.feature_flags &
				RTE_CRYPTODEV_FF_MBUF_SCATTER_GATHER) == 0)
			linearize = 1;
	}
#endif /* CPERF_LINEARIZATION_ENABLE */

	ctx->lcore_id = lcore;

	if (!ctx->options->csv)
		printf("\n# Running verify test on device: %u, lcore: %u\n",
			ctx->dev_id, lcore);

	uint16_t iv_offset = sizeof(struct rte_crypto_op) +
		sizeof(struct rte_crypto_sym_op);

	while (ops_enqd_total < ctx->options->total_ops) {

		uint16_t burst_size = ((ops_enqd_total + ctx->options->max_burst_size)
				<= ctx->options->total_ops) ?
						ctx->options->max_burst_size :
						ctx->options->total_ops -
						ops_enqd_total;

		uint16_t ops_needed = burst_size - ops_unused;

		/* Allocate objects containing crypto operations and mbufs */
		if (rte_mempool_get_bulk(ctx->pool, (void **)ops,
					ops_needed) != 0) {
			RTE_LOG(ERR, USER1,
				"Failed to allocate more crypto operations "
				"from the crypto operation pool.\n"
				"Consider increasing the pool size "
				"with --pool-sz\n");
			return -1;
		}

		/* Setup crypto op, attach mbuf etc */
		(ctx->populate_ops)(ops, ctx->src_buf_offset,
				ctx->dst_buf_offset,
				ops_needed, ctx->sess, ctx->options,
				ctx->test_vector, iv_offset, &imix_idx, NULL);


		/* Populate the mbuf with the test vector, for verification */
		for (i = 0; i < ops_needed; i++)
			cperf_mbuf_set(ops[i]->sym->m_src,
					ctx->options,
					ctx->test_vector);

#ifdef CPERF_LINEARIZATION_ENABLE
		if (linearize) {
			/* PMD doesn't support scatter-gather and source buffer
			 * is segmented.
			 * We need to linearize it before enqueuing.
			 */
			for (i = 0; i < burst_size; i++)
				rte_pktmbuf_linearize(ops[i]->sym->m_src);
		}
#endif /* CPERF_LINEARIZATION_ENABLE */

		/* Enqueue burst of ops on crypto device */
		ops_enqd = rte_cryptodev_enqueue_burst(ctx->dev_id, ctx->qp_id,
				ops, burst_size);
		if (ops_enqd < burst_size)
			ops_enqd_failed++;

		/**
		 * Calculate number of ops not enqueued (mainly for hw
		 * accelerators whose ingress queue can fill up).
		 */
		ops_unused = burst_size - ops_enqd;
		ops_enqd_total += ops_enqd;


		/* Dequeue processed burst of ops from crypto device */
		ops_deqd = rte_cryptodev_dequeue_burst(ctx->dev_id, ctx->qp_id,
				ops_processed, ctx->options->max_burst_size);

		if (ops_deqd == 0) {
			/**
			 * Count dequeue polls which didn't return any
			 * processed operations. This statistic is mainly
			 * relevant to hw accelerators.
			 */
			ops_deqd_failed++;
			continue;
		}

		for (i = 0; i < ops_deqd; i++) {
			if (cperf_verify_op(ops_processed[i], ctx->options,
						ctx->test_vector))
				ops_failed++;
		}
		/* Free crypto ops so they can be reused. */
		rte_mempool_put_bulk(ctx->pool,
					(void **)ops_processed, ops_deqd);
		ops_deqd_total += ops_deqd;
	}

	/* Dequeue any operations still in the crypto device */

	while (ops_deqd_total < ctx->options->total_ops) {
		/* Sending 0 length burst to flush sw crypto device */
		rte_cryptodev_enqueue_burst(ctx->dev_id, ctx->qp_id, NULL, 0);

		/* dequeue burst */
		ops_deqd = rte_cryptodev_dequeue_burst(ctx->dev_id, ctx->qp_id,
				ops_processed, ctx->options->max_burst_size);
		if (ops_deqd == 0) {
			ops_deqd_failed++;
			continue;
		}

		for (i = 0; i < ops_deqd; i++) {
			if (cperf_verify_op(ops_processed[i], ctx->options,
						ctx->test_vector))
				ops_failed++;
		}
		/* Free crypto ops so they can be reused. */
		rte_mempool_put_bulk(ctx->pool,
					(void **)ops_processed, ops_deqd);
		ops_deqd_total += ops_deqd;
	}

	uint16_t exp = 0;
	if (!ctx->options->csv) {
		if (__atomic_compare_exchange_n(&display_once, &exp, 1, 0,
				__ATOMIC_RELAXED, __ATOMIC_RELAXED))
			printf("%12s%12s%12s%12s%12s%12s%12s%12s\n\n",
				"lcore id", "Buf Size", "Burst size",
				"Enqueued", "Dequeued", "Failed Enq",
				"Failed Deq", "Failed Ops");

		printf("%12u%12u%12u%12"PRIu64"%12"PRIu64"%12"PRIu64
				"%12"PRIu64"%12"PRIu64"\n",
				ctx->lcore_id,
				ctx->options->max_buffer_size,
				ctx->options->max_burst_size,
				ops_enqd_total,
				ops_deqd_total,
				ops_enqd_failed,
				ops_deqd_failed,
				ops_failed);
	} else {
		if (__atomic_compare_exchange_n(&display_once, &exp, 1, 0,
				__ATOMIC_RELAXED, __ATOMIC_RELAXED))
			printf("\n# lcore id, Buffer Size(B), "
				"Burst Size,Enqueued,Dequeued,Failed Enq,"
				"Failed Deq,Failed Ops\n");

		printf("%10u,%10u,%u,%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64","
				"%"PRIu64"\n",
				ctx->lcore_id,
				ctx->options->max_buffer_size,
				ctx->options->max_burst_size,
				ops_enqd_total,
				ops_deqd_total,
				ops_enqd_failed,
				ops_deqd_failed,
				ops_failed);
	}

	return 0;
}



void
cperf_verify_test_destructor(void *arg)
{
	struct cperf_verify_ctx *ctx = arg;

	if (ctx == NULL)
		return;

	cperf_verify_test_free(ctx);
}
