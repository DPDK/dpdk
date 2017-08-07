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

#include "cperf_test_latency.h"
#include "cperf_ops.h"


struct cperf_op_result {
	uint64_t tsc_start;
	uint64_t tsc_end;
	enum rte_crypto_op_status status;
};

struct cperf_latency_ctx {
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
	struct cperf_op_result *res;
};

struct priv_op_data {
	struct cperf_op_result *result;
};

#define max(a, b) (a > b ? (uint64_t)a : (uint64_t)b)
#define min(a, b) (a < b ? (uint64_t)a : (uint64_t)b)

static void
cperf_latency_test_free(struct cperf_latency_ctx *ctx, uint32_t mbuf_nb)
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

		rte_free(ctx->res);
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
cperf_latency_test_constructor(struct rte_mempool *sess_mp,
		uint8_t dev_id, uint16_t qp_id,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		const struct cperf_op_fns *op_fns)
{
	struct cperf_latency_ctx *ctx = NULL;
	unsigned int mbuf_idx = 0;
	char pool_name[32] = "";

	ctx = rte_malloc(NULL, sizeof(struct cperf_latency_ctx), 0);
	if (ctx == NULL)
		goto err;

	ctx->dev_id = dev_id;
	ctx->qp_id = qp_id;

	ctx->populate_ops = op_fns->populate_ops;
	ctx->options = options;
	ctx->test_vector = test_vector;

	/* IV goes at the end of the crypto operation */
	uint16_t iv_offset = sizeof(struct rte_crypto_op) +
		sizeof(struct rte_crypto_sym_op) +
		sizeof(struct cperf_op_result *);

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
			(sizeof(struct rte_mbuf *) *
			ctx->options->pool_sz), 0);

	for (mbuf_idx = 0; mbuf_idx < options->pool_sz; mbuf_idx++) {
		ctx->mbufs_in[mbuf_idx] = cperf_mbuf_create(
				ctx->pkt_mbuf_pool_in, options->segments_nb,
				options, test_vector);
		if (ctx->mbufs_in[mbuf_idx] == NULL)
			goto err;
	}

	if (options->out_of_place == 1)	{

		snprintf(pool_name, sizeof(pool_name),
				"cperf_pool_out_cdev_%d",
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

	uint16_t priv_size = sizeof(struct priv_op_data) +
			test_vector->cipher_iv.length +
			test_vector->auth_iv.length +
			test_vector->aead_iv.length;
	ctx->crypto_op_pool = rte_crypto_op_pool_create(pool_name,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC, options->pool_sz,
			512, priv_size, rte_socket_id());

	if (ctx->crypto_op_pool == NULL)
		goto err;

	ctx->res = rte_malloc(NULL, sizeof(struct cperf_op_result) *
			ctx->options->total_ops, 0);

	if (ctx->res == NULL)
		goto err;

	return ctx;
err:
	cperf_latency_test_free(ctx, mbuf_idx);

	return NULL;
}

static inline void
store_timestamp(struct rte_crypto_op *op, uint64_t timestamp)
{
	struct priv_op_data *priv_data;

	priv_data = (struct priv_op_data *) (op->sym + 1);
	priv_data->result->status = op->status;
	priv_data->result->tsc_end = timestamp;
}

int
cperf_latency_test_runner(void *arg)
{
	struct cperf_latency_ctx *ctx = arg;
	uint16_t test_burst_size;
	uint8_t burst_size_idx = 0;

	static int only_once;

	if (ctx == NULL)
		return 0;

	struct rte_crypto_op *ops[ctx->options->max_burst_size];
	struct rte_crypto_op *ops_processed[ctx->options->max_burst_size];
	uint64_t i;
	struct priv_op_data *priv_data;

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

	/* Warm up the host CPU before starting the test */
	for (i = 0; i < ctx->options->total_ops; i++)
		rte_cryptodev_enqueue_burst(ctx->dev_id, ctx->qp_id, NULL, 0);

	/* Get first size from range or list */
	if (ctx->options->inc_burst_size != 0)
		test_burst_size = ctx->options->min_burst_size;
	else
		test_burst_size = ctx->options->burst_size_list[0];

	uint16_t iv_offset = sizeof(struct rte_crypto_op) +
		sizeof(struct rte_crypto_sym_op) +
		sizeof(struct cperf_op_result *);

	while (test_burst_size <= ctx->options->max_burst_size) {
		uint64_t ops_enqd = 0, ops_deqd = 0;
		uint64_t m_idx = 0, b_idx = 0;

		uint64_t tsc_val, tsc_end, tsc_start;
		uint64_t tsc_max = 0, tsc_min = ~0UL, tsc_tot = 0, tsc_idx = 0;
		uint64_t enqd_max = 0, enqd_min = ~0UL, enqd_tot = 0;
		uint64_t deqd_max = 0, deqd_min = ~0UL, deqd_tot = 0;

		while (enqd_tot < ctx->options->total_ops) {

			uint16_t burst_size = ((enqd_tot + test_burst_size)
					<= ctx->options->total_ops) ?
							test_burst_size :
							ctx->options->total_ops -
							enqd_tot;

			/* Allocate crypto ops from pool */
			if (burst_size != rte_crypto_op_bulk_alloc(
					ctx->crypto_op_pool,
					RTE_CRYPTO_OP_TYPE_SYMMETRIC,
					ops, burst_size)) {
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
					burst_size, ctx->sess, ctx->options,
					ctx->test_vector, iv_offset);

			tsc_start = rte_rdtsc_precise();

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

			/* Dequeue processed burst of ops from crypto device */
			ops_deqd = rte_cryptodev_dequeue_burst(ctx->dev_id, ctx->qp_id,
					ops_processed, test_burst_size);

			tsc_end = rte_rdtsc_precise();

			/* Free memory for not enqueued operations */
			if (ops_enqd != burst_size)
				rte_mempool_put_bulk(ctx->crypto_op_pool,
						(void **)&ops[ops_enqd],
						burst_size - ops_enqd);

			for (i = 0; i < ops_enqd; i++) {
				ctx->res[tsc_idx].tsc_start = tsc_start;
				/*
				 * Private data structure starts after the end of the
				 * rte_crypto_sym_op structure.
				 */
				priv_data = (struct priv_op_data *) (ops[i]->sym + 1);
				priv_data->result = (void *)&ctx->res[tsc_idx];
				tsc_idx++;
			}

			if (likely(ops_deqd))  {
				/*
				 * free crypto ops so they can be reused. We don't free
				 * the mbufs here as we don't want to reuse them as
				 * the crypto operation will change the data and cause
				 * failures.
				 */
				for (i = 0; i < ops_deqd; i++)
					store_timestamp(ops_processed[i], tsc_end);

				rte_mempool_put_bulk(ctx->crypto_op_pool,
						(void **)ops_processed, ops_deqd);

				deqd_tot += ops_deqd;
				deqd_max = max(ops_deqd, deqd_max);
				deqd_min = min(ops_deqd, deqd_min);
			}

			enqd_tot += ops_enqd;
			enqd_max = max(ops_enqd, enqd_max);
			enqd_min = min(ops_enqd, enqd_min);

			m_idx += ops_enqd;
			m_idx = m_idx + test_burst_size > ctx->options->pool_sz ?
					0 : m_idx;
			b_idx++;
		}

		/* Dequeue any operations still in the crypto device */
		while (deqd_tot < ctx->options->total_ops) {
			/* Sending 0 length burst to flush sw crypto device */
			rte_cryptodev_enqueue_burst(ctx->dev_id, ctx->qp_id, NULL, 0);

			/* dequeue burst */
			ops_deqd = rte_cryptodev_dequeue_burst(ctx->dev_id, ctx->qp_id,
					ops_processed, test_burst_size);

			tsc_end = rte_rdtsc_precise();

			if (ops_deqd != 0) {
				for (i = 0; i < ops_deqd; i++)
					store_timestamp(ops_processed[i], tsc_end);

				rte_mempool_put_bulk(ctx->crypto_op_pool,
						(void **)ops_processed, ops_deqd);

				deqd_tot += ops_deqd;
				deqd_max = max(ops_deqd, deqd_max);
				deqd_min = min(ops_deqd, deqd_min);
			}
		}

		for (i = 0; i < tsc_idx; i++) {
			tsc_val = ctx->res[i].tsc_end - ctx->res[i].tsc_start;
			tsc_max = max(tsc_val, tsc_max);
			tsc_min = min(tsc_val, tsc_min);
			tsc_tot += tsc_val;
		}

		double time_tot, time_avg, time_max, time_min;

		const uint64_t tunit = 1000000; /* us */
		const uint64_t tsc_hz = rte_get_tsc_hz();

		uint64_t enqd_avg = enqd_tot / b_idx;
		uint64_t deqd_avg = deqd_tot / b_idx;
		uint64_t tsc_avg = tsc_tot / tsc_idx;

		time_tot = tunit*(double)(tsc_tot) / tsc_hz;
		time_avg = tunit*(double)(tsc_avg) / tsc_hz;
		time_max = tunit*(double)(tsc_max) / tsc_hz;
		time_min = tunit*(double)(tsc_min) / tsc_hz;

		if (ctx->options->csv) {
			if (!only_once)
				printf("\n# lcore, Buffer Size, Burst Size, Pakt Seq #, "
						"Packet Size, cycles, time (us)");

			for (i = 0; i < ctx->options->total_ops; i++) {

				printf("\n%u;%u;%u;%"PRIu64";%"PRIu64";%.3f",
					ctx->lcore_id, ctx->options->test_buffer_size,
					test_burst_size, i + 1,
					ctx->res[i].tsc_end - ctx->res[i].tsc_start,
					tunit * (double) (ctx->res[i].tsc_end
							- ctx->res[i].tsc_start)
						/ tsc_hz);

			}
			only_once = 1;
		} else {
			printf("\n# Device %d on lcore %u\n", ctx->dev_id,
				ctx->lcore_id);
			printf("\n# total operations: %u", ctx->options->total_ops);
			printf("\n# Buffer size: %u", ctx->options->test_buffer_size);
			printf("\n# Burst size: %u", test_burst_size);
			printf("\n#     Number of bursts: %"PRIu64,
					b_idx);

			printf("\n#");
			printf("\n#          \t       Total\t   Average\t   "
					"Maximum\t   Minimum");
			printf("\n#  enqueued\t%12"PRIu64"\t%10"PRIu64"\t"
					"%10"PRIu64"\t%10"PRIu64, enqd_tot,
					enqd_avg, enqd_max, enqd_min);
			printf("\n#  dequeued\t%12"PRIu64"\t%10"PRIu64"\t"
					"%10"PRIu64"\t%10"PRIu64, deqd_tot,
					deqd_avg, deqd_max, deqd_min);
			printf("\n#    cycles\t%12"PRIu64"\t%10"PRIu64"\t"
					"%10"PRIu64"\t%10"PRIu64, tsc_tot,
					tsc_avg, tsc_max, tsc_min);
			printf("\n# time [us]\t%12.0f\t%10.3f\t%10.3f\t%10.3f",
					time_tot, time_avg, time_max, time_min);
			printf("\n\n");

		}

		/* Get next size from range or list */
		if (ctx->options->inc_burst_size != 0)
			test_burst_size += ctx->options->inc_burst_size;
		else {
			if (++burst_size_idx == ctx->options->burst_size_count)
				break;
			test_burst_size =
				ctx->options->burst_size_list[burst_size_idx];
		}
	}

	return 0;
}

void
cperf_latency_test_destructor(void *arg)
{
	struct cperf_latency_ctx *ctx = arg;

	if (ctx == NULL)
		return;

	rte_cryptodev_stop(ctx->dev_id);

	cperf_latency_test_free(ctx, ctx->options->pool_sz);
}
