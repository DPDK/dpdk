/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>

#include "cperf_test_verify.h"
#include "cperf_ops.h"

struct cperf_verify_ctx {
	uint8_t dev_id;
	uint16_t qp_id;
	uint8_t lcore_id;

	struct rte_mempool *pkt_mbuf_pool_in;
	struct rte_mempool *pkt_mbuf_pool_out;
	struct rte_mbuf **mbufs_in;
	struct rte_mbuf **mbufs_out;

	struct rte_mempool *crypto_op_pool;

	struct rte_cryptodev_sym_session *sess;

	cperf_populate_ops_t populate_ops;

	const struct cperf_options *options;
	const struct cperf_test_vector *test_vector;
};

struct cperf_op_result {
	enum rte_crypto_op_status status;
};

static void
cperf_verify_test_free(struct cperf_verify_ctx *ctx, uint32_t mbuf_nb)
{
	uint32_t i;

	if (ctx) {
		if (ctx->sess) {
			rte_cryptodev_sym_session_clear(ctx->dev_id, ctx->sess);
			rte_cryptodev_sym_session_free(ctx->sess);
		}

		if (ctx->mbufs_in) {
			for (i = 0; i < mbuf_nb; i++)
				rte_pktmbuf_free(ctx->mbufs_in[i]);

			rte_free(ctx->mbufs_in);
		}

		if (ctx->mbufs_out) {
			for (i = 0; i < mbuf_nb; i++) {
				if (ctx->mbufs_out[i] != NULL)
					rte_pktmbuf_free(ctx->mbufs_out[i]);
			}

			rte_free(ctx->mbufs_out);
		}

		if (ctx->pkt_mbuf_pool_in)
			rte_mempool_free(ctx->pkt_mbuf_pool_in);

		if (ctx->pkt_mbuf_pool_out)
			rte_mempool_free(ctx->pkt_mbuf_pool_out);

		if (ctx->crypto_op_pool)
			rte_mempool_free(ctx->crypto_op_pool);

		rte_free(ctx);
	}
}

static struct rte_mbuf *
cperf_mbuf_create(struct rte_mempool *mempool,
		uint32_t segments_nb,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector)
{
	struct rte_mbuf *mbuf;
	uint32_t segment_sz = options->max_buffer_size / segments_nb;
	uint32_t last_sz = options->max_buffer_size % segments_nb;
	uint8_t *mbuf_data;
	uint8_t *test_data =
			(options->cipher_op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) ?
					test_vector->plaintext.data :
					test_vector->ciphertext.data;

	mbuf = rte_pktmbuf_alloc(mempool);
	if (mbuf == NULL)
		goto error;

	mbuf_data = (uint8_t *)rte_pktmbuf_append(mbuf, segment_sz);
	if (mbuf_data == NULL)
		goto error;

	memcpy(mbuf_data, test_data, segment_sz);
	test_data += segment_sz;
	segments_nb--;

	while (segments_nb) {
		struct rte_mbuf *m;

		m = rte_pktmbuf_alloc(mempool);
		if (m == NULL)
			goto error;

		rte_pktmbuf_chain(mbuf, m);

		mbuf_data = (uint8_t *)rte_pktmbuf_append(mbuf, segment_sz);
		if (mbuf_data == NULL)
			goto error;

		memcpy(mbuf_data, test_data, segment_sz);
		test_data += segment_sz;
		segments_nb--;
	}

	if (last_sz) {
		mbuf_data = (uint8_t *)rte_pktmbuf_append(mbuf, last_sz);
		if (mbuf_data == NULL)
			goto error;

		memcpy(mbuf_data, test_data, last_sz);
	}

	if (options->op_type != CPERF_CIPHER_ONLY) {
		mbuf_data = (uint8_t *)rte_pktmbuf_append(mbuf,
				options->digest_sz);
		if (mbuf_data == NULL)
			goto error;
	}

	if (options->op_type == CPERF_AEAD) {
		uint8_t *aead = (uint8_t *)rte_pktmbuf_prepend(mbuf,
			RTE_ALIGN_CEIL(options->aead_aad_sz, 16));

		if (aead == NULL)
			goto error;

		memcpy(aead, test_vector->aad.data, test_vector->aad.length);
	}

	return mbuf;
error:
	if (mbuf != NULL)
		rte_pktmbuf_free(mbuf);

	return NULL;
}

void *
cperf_verify_test_constructor(struct rte_mempool *sess_mp,
		uint8_t dev_id, uint16_t qp_id,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		const struct cperf_op_fns *op_fns)
{
	struct cperf_verify_ctx *ctx = NULL;
	unsigned int mbuf_idx = 0;
	char pool_name[32] = "";

	ctx = rte_malloc(NULL, sizeof(struct cperf_verify_ctx), 0);
	if (ctx == NULL)
		goto err;

	ctx->dev_id = dev_id;
	ctx->qp_id = qp_id;

	ctx->populate_ops = op_fns->populate_ops;
	ctx->options = options;
	ctx->test_vector = test_vector;

	/* IV goes at the end of the cryptop operation */
	uint16_t iv_offset = sizeof(struct rte_crypto_op) +
		sizeof(struct rte_crypto_sym_op);

	ctx->sess = op_fns->sess_create(sess_mp, dev_id, options, test_vector,
			iv_offset);
	if (ctx->sess == NULL)
		goto err;

	snprintf(pool_name, sizeof(pool_name), "cperf_pool_in_cdev_%d",
			dev_id);

	ctx->pkt_mbuf_pool_in = rte_pktmbuf_pool_create(pool_name,
			options->pool_sz * options->segments_nb, 0, 0,
			RTE_PKTMBUF_HEADROOM +
			RTE_CACHE_LINE_ROUNDUP(
				(options->max_buffer_size / options->segments_nb) +
				(options->max_buffer_size % options->segments_nb) +
					options->digest_sz),
			rte_socket_id());

	if (ctx->pkt_mbuf_pool_in == NULL)
		goto err;

	/* Generate mbufs_in with plaintext populated for test */
	ctx->mbufs_in = rte_malloc(NULL,
			(sizeof(struct rte_mbuf *) * ctx->options->pool_sz), 0);

	for (mbuf_idx = 0; mbuf_idx < options->pool_sz; mbuf_idx++) {
		ctx->mbufs_in[mbuf_idx] = cperf_mbuf_create(
				ctx->pkt_mbuf_pool_in, options->segments_nb,
				options, test_vector);
		if (ctx->mbufs_in[mbuf_idx] == NULL)
			goto err;
	}

	if (options->out_of_place == 1)	{

		snprintf(pool_name, sizeof(pool_name), "cperf_pool_out_cdev_%d",
				dev_id);

		ctx->pkt_mbuf_pool_out = rte_pktmbuf_pool_create(
				pool_name, options->pool_sz, 0, 0,
				RTE_PKTMBUF_HEADROOM +
				RTE_CACHE_LINE_ROUNDUP(
					options->max_buffer_size +
					options->digest_sz),
				rte_socket_id());

		if (ctx->pkt_mbuf_pool_out == NULL)
			goto err;
	}

	ctx->mbufs_out = rte_malloc(NULL,
			(sizeof(struct rte_mbuf *) *
			ctx->options->pool_sz), 0);

	for (mbuf_idx = 0; mbuf_idx < options->pool_sz; mbuf_idx++) {
		if (options->out_of_place == 1)	{
			ctx->mbufs_out[mbuf_idx] = cperf_mbuf_create(
					ctx->pkt_mbuf_pool_out, 1,
					options, test_vector);
			if (ctx->mbufs_out[mbuf_idx] == NULL)
				goto err;
		} else {
			ctx->mbufs_out[mbuf_idx] = NULL;
		}
	}

	snprintf(pool_name, sizeof(pool_name), "cperf_op_pool_cdev_%d",
			dev_id);

	uint16_t priv_size = test_vector->cipher_iv.length +
		test_vector->auth_iv.length + test_vector->aead_iv.length;
	ctx->crypto_op_pool = rte_crypto_op_pool_create(pool_name,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC, options->pool_sz,
			512, priv_size, rte_socket_id());
	if (ctx->crypto_op_pool == NULL)
		goto err;

	return ctx;
err:
	cperf_verify_test_free(ctx, mbuf_idx);

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
		cipher_offset = vector->aad.length;
		auth = 1;
		auth_offset = vector->aad.length + options->test_buffer_size;
		break;
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

	return !!res;
}

int
cperf_verify_test_runner(void *test_ctx)
{
	struct cperf_verify_ctx *ctx = test_ctx;

	uint64_t ops_enqd = 0, ops_enqd_total = 0, ops_enqd_failed = 0;
	uint64_t ops_deqd = 0, ops_deqd_total = 0, ops_deqd_failed = 0;
	uint64_t ops_failed = 0;

	static int only_once;

	uint64_t i, m_idx = 0;
	uint16_t ops_unused = 0;

	struct rte_crypto_op *ops[ctx->options->max_burst_size];
	struct rte_crypto_op *ops_processed[ctx->options->max_burst_size];

	uint32_t lcore = rte_lcore_id();

#ifdef CPERF_LINEARIZATION_ENABLE
	struct rte_cryptodev_info dev_info;
	int linearize = 0;

	/* Check if source mbufs require coalescing */
	if (ctx->options->segments_nb > 1) {
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

		/* Allocate crypto ops from pool */
		if (ops_needed != rte_crypto_op_bulk_alloc(
				ctx->crypto_op_pool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC,
				ops, ops_needed)) {
			RTE_LOG(ERR, USER1,
				"Failed to allocate more crypto operations "
				"from the the crypto operation pool.\n"
				"Consider increasing the pool size "
				"with --pool-sz\n");
			return -1;
		}

		/* Setup crypto op, attach mbuf etc */
		(ctx->populate_ops)(ops, &ctx->mbufs_in[m_idx],
				&ctx->mbufs_out[m_idx],
				ops_needed, ctx->sess, ctx->options,
				ctx->test_vector, iv_offset);

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

		m_idx += ops_needed;
		if (m_idx + ctx->options->max_burst_size > ctx->options->pool_sz)
			m_idx = 0;

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
			/* free crypto ops so they can be reused. We don't free
			 * the mbufs here as we don't want to reuse them as
			 * the crypto operation will change the data and cause
			 * failures.
			 */
			rte_crypto_op_free(ops_processed[i]);
		}
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
			/* free crypto ops so they can be reused. We don't free
			 * the mbufs here as we don't want to reuse them as
			 * the crypto operation will change the data and cause
			 * failures.
			 */
			rte_crypto_op_free(ops_processed[i]);
		}
		ops_deqd_total += ops_deqd;
	}

	if (!ctx->options->csv) {
		if (!only_once)
			printf("%12s%12s%12s%12s%12s%12s%12s%12s\n\n",
				"lcore id", "Buf Size", "Burst size",
				"Enqueued", "Dequeued", "Failed Enq",
				"Failed Deq", "Failed Ops");
		only_once = 1;

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
		if (!only_once)
			printf("\n# lcore id, Buffer Size(B), "
				"Burst Size,Enqueued,Dequeued,Failed Enq,"
				"Failed Deq,Failed Ops\n");
		only_once = 1;

		printf("%10u;%10u;%u;%"PRIu64";%"PRIu64";%"PRIu64";%"PRIu64";"
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

	rte_cryptodev_stop(ctx->dev_id);

	cperf_verify_test_free(ctx, ctx->options->pool_sz);
}
