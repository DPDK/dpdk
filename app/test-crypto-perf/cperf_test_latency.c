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


struct cperf_latency_results {

	uint64_t ops_failed;

	uint64_t enqd_tot;
	uint64_t enqd_max;
	uint64_t enqd_min;

	uint64_t deqd_tot;
	uint64_t deqd_max;
	uint64_t deqd_min;

	uint64_t cycles_tot;
	uint64_t cycles_max;
	uint64_t cycles_min;

	uint64_t burst_num;
	uint64_t num;
};

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
	cperf_verify_crypto_op_t verify_op_output;

	const struct cperf_options *options;
	const struct cperf_test_vector *test_vector;
	struct cperf_op_result *res;
	struct cperf_latency_results results;
};

#define max(a, b) (a > b ? (uint64_t)a : (uint64_t)b)
#define min(a, b) (a < b ? (uint64_t)a : (uint64_t)b)

static void
cperf_latency_test_free(struct cperf_latency_ctx *ctx, uint32_t mbuf_nb)
{
	uint32_t i;

	if (ctx) {
		if (ctx->sess)
			rte_cryptodev_sym_session_free(ctx->dev_id, ctx->sess);

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
	uint32_t segment_sz = options->buffer_sz / segments_nb;
	uint32_t last_sz = options->buffer_sz % segments_nb;
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

	mbuf_data = (uint8_t *)rte_pktmbuf_append(mbuf,
			options->auth_digest_sz);
	if (mbuf_data == NULL)
		goto error;

	if (options->op_type == CPERF_AEAD) {
		uint8_t *aead = (uint8_t *)rte_pktmbuf_prepend(mbuf,
			RTE_ALIGN_CEIL(options->auth_aad_sz, 16));

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
cperf_latency_test_constructor(uint8_t dev_id, uint16_t qp_id,
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

	ctx->sess = op_fns->sess_create(dev_id, options, test_vector);
	if (ctx->sess == NULL)
		goto err;

	snprintf(pool_name, sizeof(pool_name), "cperf_pool_in_cdev_%d",
				dev_id);

	ctx->pkt_mbuf_pool_in = rte_pktmbuf_pool_create(pool_name,
			options->pool_sz * options->segments_nb, 0, 0,
			RTE_PKTMBUF_HEADROOM +
			RTE_CACHE_LINE_ROUNDUP(
				(options->buffer_sz / options->segments_nb) +
				(options->buffer_sz % options->segments_nb) +
					options->auth_digest_sz),
			rte_socket_id());

	if (ctx->pkt_mbuf_pool_in == NULL)
		goto err;

	/* Generate mbufs_in with plaintext populated for test */
	if (ctx->options->pool_sz % ctx->options->burst_sz)
		goto err;

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
					options->buffer_sz +
					options->auth_digest_sz),
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

	ctx->crypto_op_pool = rte_crypto_op_pool_create(pool_name,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC, options->pool_sz, 0, 0,
			rte_socket_id());
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

static int
cperf_latency_test_verifier(struct rte_mbuf *mbuf,
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

	m = mbuf;
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

	m = mbuf;
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
		auth_offset = vector->plaintext.length;
		break;
	case CPERF_AUTH_ONLY:
		cipher = 0;
		cipher_offset = 0;
		auth = 1;
		auth_offset = vector->plaintext.length;
		break;
	case CPERF_AUTH_THEN_CIPHER:
		cipher = 1;
		cipher_offset = 0;
		auth = 1;
		auth_offset = vector->plaintext.length;
		break;
	case CPERF_AEAD:
		cipher = 1;
		cipher_offset = vector->aad.length;
		auth = 1;
		auth_offset = vector->aad.length + vector->plaintext.length;
		break;
	}

	if (cipher == 1) {
		if (options->cipher_op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
			res += memcmp(data + cipher_offset,
					vector->ciphertext.data,
					vector->ciphertext.length);
		else
			res += memcmp(data + cipher_offset,
					vector->plaintext.data,
					vector->plaintext.length);
	}

	if (auth == 1) {
		if (options->auth_op == RTE_CRYPTO_AUTH_OP_GENERATE)
			res += memcmp(data + auth_offset,
					vector->digest.data,
					vector->digest.length);
	}

	if (res != 0)
		res = 1;

	return res;
}

int
cperf_latency_test_runner(void *arg)
{
	struct cperf_latency_ctx *ctx = arg;
	struct cperf_op_result *pres;

	if (ctx == NULL)
		return 0;

	struct rte_crypto_op *ops[ctx->options->burst_sz];
	struct rte_crypto_op *ops_processed[ctx->options->burst_sz];
	uint64_t ops_enqd = 0, ops_deqd = 0;
	uint16_t ops_unused = 0;
	uint64_t m_idx = 0, b_idx = 0, i;

	uint64_t tsc_val, tsc_end, tsc_start;
	uint64_t tsc_max = 0, tsc_min = ~0UL, tsc_tot = 0, tsc_idx = 0;
	uint64_t enqd_max = 0, enqd_min = ~0UL, enqd_tot = 0;
	uint64_t deqd_max = 0, deqd_min = ~0UL, deqd_tot = 0;

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

	while (enqd_tot < ctx->options->total_ops) {

		uint16_t burst_size = ((enqd_tot + ctx->options->burst_sz)
				<= ctx->options->total_ops) ?
						ctx->options->burst_sz :
						ctx->options->total_ops -
						enqd_tot;
		uint16_t ops_needed = burst_size - ops_unused;

		/* Allocate crypto ops from pool */
		if (ops_needed != rte_crypto_op_bulk_alloc(
				ctx->crypto_op_pool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC,
				ops, ops_needed))
			return -1;

		/* Setup crypto op, attach mbuf etc */
		(ctx->populate_ops)(ops, &ctx->mbufs_in[m_idx],
				&ctx->mbufs_out[m_idx],
				ops_needed, ctx->sess, ctx->options,
				ctx->test_vector);

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
				ops_processed, ctx->options->burst_sz);

		tsc_end = rte_rdtsc_precise();

		for (i = 0; i < ops_needed; i++) {
			ctx->res[tsc_idx].tsc_start = tsc_start;
			ops[i]->opaque_data = (void *)&ctx->res[tsc_idx];
			tsc_idx++;
		}

		/*
		 * Calculate number of ops not enqueued (mainly for hw
		 * accelerators whose ingress queue can fill up).
		 */
		ops_unused = burst_size - ops_enqd;

		if (likely(ops_deqd))  {
			/*
			 * free crypto ops so they can be reused. We don't free
			 * the mbufs here as we don't want to reuse them as
			 * the crypto operation will change the data and cause
			 * failures.
			 */
			for (i = 0; i < ops_deqd; i++) {
				pres = (struct cperf_op_result *)
						(ops_processed[i]->opaque_data);
				pres->status = ops_processed[i]->status;
				pres->tsc_end = tsc_end;

				rte_crypto_op_free(ops_processed[i]);
			}

			deqd_tot += ops_deqd;
			deqd_max = max(ops_deqd, deqd_max);
			deqd_min = min(ops_deqd, deqd_min);
		}

		enqd_tot += ops_enqd;
		enqd_max = max(ops_enqd, enqd_max);
		enqd_min = min(ops_enqd, enqd_min);

		m_idx += ops_needed;
		m_idx = m_idx + ctx->options->burst_sz > ctx->options->pool_sz ?
				0 : m_idx;
		b_idx++;
	}

	/* Dequeue any operations still in the crypto device */
	while (deqd_tot < ctx->options->total_ops) {
		/* Sending 0 length burst to flush sw crypto device */
		rte_cryptodev_enqueue_burst(ctx->dev_id, ctx->qp_id, NULL, 0);

		/* dequeue burst */
		ops_deqd = rte_cryptodev_dequeue_burst(ctx->dev_id, ctx->qp_id,
				ops_processed, ctx->options->burst_sz);

		tsc_end = rte_rdtsc_precise();

		if (ops_deqd != 0) {
			for (i = 0; i < ops_deqd; i++) {
				pres = (struct cperf_op_result *)
						(ops_processed[i]->opaque_data);
				pres->status = ops_processed[i]->status;
				pres->tsc_end = tsc_end;

				rte_crypto_op_free(ops_processed[i]);
			}

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

	if (ctx->options->verify) {
		struct rte_mbuf **mbufs;

		if (ctx->options->out_of_place == 1)
			mbufs = ctx->mbufs_out;
		else
			mbufs = ctx->mbufs_in;

		for (i = 0; i < ctx->options->total_ops; i++) {

			if (ctx->res[i].status != RTE_CRYPTO_OP_STATUS_SUCCESS
					|| cperf_latency_test_verifier(mbufs[i],
							ctx->options,
							ctx->test_vector)) {

				ctx->results.ops_failed++;
			}
		}
	}

	ctx->results.enqd_tot = enqd_tot;
	ctx->results.enqd_max = enqd_max;
	ctx->results.enqd_min = enqd_min;

	ctx->results.deqd_tot = deqd_tot;
	ctx->results.deqd_max = deqd_max;
	ctx->results.deqd_min = deqd_min;

	ctx->results.cycles_tot = tsc_tot;
	ctx->results.cycles_max = tsc_max;
	ctx->results.cycles_min = tsc_min;

	ctx->results.burst_num = b_idx;
	ctx->results.num = tsc_idx;

	return 0;
}

void
cperf_latency_test_destructor(void *arg)
{
	struct cperf_latency_ctx *ctx = arg;
	uint64_t i;
	if (ctx == NULL)
		return;
	static int only_once;
	uint64_t etot, eavg, emax, emin;
	uint64_t dtot, davg, dmax, dmin;
	uint64_t ctot, cavg, cmax, cmin;
	double ttot, tavg, tmax, tmin;

	const uint64_t tunit = 1000000; /* us */
	const uint64_t tsc_hz = rte_get_tsc_hz();

	etot = ctx->results.enqd_tot;
	eavg = ctx->results.enqd_tot / ctx->results.burst_num;
	emax = ctx->results.enqd_max;
	emin = ctx->results.enqd_min;

	dtot = ctx->results.deqd_tot;
	davg = ctx->results.deqd_tot / ctx->results.burst_num;
	dmax = ctx->results.deqd_max;
	dmin = ctx->results.deqd_min;

	ctot = ctx->results.cycles_tot;
	cavg = ctx->results.cycles_tot / ctx->results.num;
	cmax = ctx->results.cycles_max;
	cmin = ctx->results.cycles_min;

	ttot = tunit*(double)(ctot) / tsc_hz;
	tavg = tunit*(double)(cavg) / tsc_hz;
	tmax = tunit*(double)(cmax) / tsc_hz;
	tmin = tunit*(double)(cmin) / tsc_hz;

	if (ctx->options->csv) {
		if (!only_once)
			printf("\n# lcore, Pakt Seq #, Packet Size, cycles,"
					" time (us)");

		for (i = 0; i < ctx->options->total_ops; i++) {

			printf("\n%u;%"PRIu64";%"PRIu64";%.3f",
				ctx->lcore_id, i + 1,
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
		printf("\n#  verified failed: %"PRIu64,
				ctx->results.ops_failed);
		printf("\n#     burst number: %"PRIu64,
				ctx->results.burst_num);
		printf("\n#");
		printf("\n#          \t       Total\t   Average\t   Maximum\t "
				"  Minimum");
		printf("\n#  enqueued\t%12"PRIu64"\t%10"PRIu64"\t%10"PRIu64"\t"
				"%10"PRIu64, etot, eavg, emax, emin);
		printf("\n#  dequeued\t%12"PRIu64"\t%10"PRIu64"\t%10"PRIu64"\t"
				"%10"PRIu64, dtot, davg, dmax, dmin);
		printf("\n#    cycles\t%12"PRIu64"\t%10"PRIu64"\t%10"PRIu64"\t"
				"%10"PRIu64, ctot, cavg, cmax, cmin);
		printf("\n# time [us]\t%12.0f\t%10.3f\t%10.3f\t%10.3f", ttot,
			tavg, tmax, tmin);
		printf("\n\n");

	}
	cperf_latency_test_free(ctx, ctx->options->pool_sz);

}
