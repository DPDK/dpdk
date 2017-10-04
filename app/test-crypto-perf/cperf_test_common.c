/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#include "cperf_test_common.h"

static struct rte_mbuf *
cperf_mbuf_create(struct rte_mempool *mempool,
		uint32_t segment_sz,
		uint32_t segments_nb,
		const struct cperf_options *options)
{
	struct rte_mbuf *mbuf;
	uint8_t *mbuf_data;
	uint32_t remaining_bytes = options->max_buffer_size;

	mbuf = rte_pktmbuf_alloc(mempool);
	if (mbuf == NULL)
		goto error;

	mbuf_data = (uint8_t *)rte_pktmbuf_append(mbuf, segment_sz);
	if (mbuf_data == NULL)
		goto error;

	if (options->max_buffer_size <= segment_sz)
		remaining_bytes = 0;
	else
		remaining_bytes -= segment_sz;

	segments_nb--;

	while (remaining_bytes) {
		struct rte_mbuf *m;

		m = rte_pktmbuf_alloc(mempool);
		if (m == NULL)
			goto error;

		rte_pktmbuf_chain(mbuf, m);

		mbuf_data = (uint8_t *)rte_pktmbuf_append(mbuf, segment_sz);
		if (mbuf_data == NULL)
			goto error;

		if (remaining_bytes <= segment_sz)
			remaining_bytes = 0;
		else
			remaining_bytes -= segment_sz;

		segments_nb--;
	}

	/*
	 * If there was not enough room for the digest at the end
	 * of the last segment, allocate a new one
	 */
	if (segments_nb != 0) {
		struct rte_mbuf *m;
		m = rte_pktmbuf_alloc(mempool);

		if (m == NULL)
			goto error;

		rte_pktmbuf_chain(mbuf, m);
		mbuf_data = (uint8_t *)rte_pktmbuf_append(mbuf,	segment_sz);
		if (mbuf_data == NULL)
			goto error;
	}

	return mbuf;
error:
	if (mbuf != NULL)
		rte_pktmbuf_free(mbuf);

	return NULL;
}

int
cperf_alloc_common_memory(const struct cperf_options *options,
			const struct cperf_test_vector *test_vector,
			uint8_t dev_id, uint16_t qp_id,
			size_t extra_op_priv_size,
			struct rte_mempool **pkt_mbuf_pool_in,
			struct rte_mempool **pkt_mbuf_pool_out,
			struct rte_mbuf ***mbufs_in,
			struct rte_mbuf ***mbufs_out,
			struct rte_mempool **crypto_op_pool)
{
	unsigned int mbuf_idx = 0;
	char pool_name[32] = "";

	snprintf(pool_name, sizeof(pool_name), "cperf_pool_in_cdev_%u_qp_%u",
			dev_id, qp_id);

	uint32_t max_size = options->max_buffer_size + options->digest_sz;
	uint16_t segments_nb = (max_size % options->segment_sz) ?
			(max_size / options->segment_sz) + 1 :
			max_size / options->segment_sz;

	*pkt_mbuf_pool_in = rte_pktmbuf_pool_create(pool_name,
			options->pool_sz * segments_nb, 0, 0,
			RTE_PKTMBUF_HEADROOM + options->segment_sz,
			rte_socket_id());

	if (*pkt_mbuf_pool_in == NULL)
		return -1;

	/* Generate mbufs_in with plaintext populated for test */
	*mbufs_in = (struct rte_mbuf **)rte_malloc(NULL,
			(sizeof(struct rte_mbuf *) * options->pool_sz), 0);

	for (mbuf_idx = 0; mbuf_idx < options->pool_sz; mbuf_idx++) {
		(*mbufs_in)[mbuf_idx] = cperf_mbuf_create(
				*pkt_mbuf_pool_in,
				options->segment_sz,
				segments_nb,
				options);
		if ((*mbufs_in)[mbuf_idx] == NULL)
			return -1;
	}

	*mbufs_out = (struct rte_mbuf **)rte_zmalloc(NULL,
			(sizeof(struct rte_mbuf *) *
			options->pool_sz), 0);

	if (options->out_of_place == 1)	{
		snprintf(pool_name, sizeof(pool_name), "cperf_pool_out_cdev_%u_qp_%u",
				dev_id, qp_id);

		*pkt_mbuf_pool_out = rte_pktmbuf_pool_create(
				pool_name, options->pool_sz, 0, 0,
				RTE_PKTMBUF_HEADROOM + max_size,
				rte_socket_id());

		if (*pkt_mbuf_pool_out == NULL)
			return -1;

		for (mbuf_idx = 0; mbuf_idx < options->pool_sz; mbuf_idx++) {
			(*mbufs_out)[mbuf_idx] = cperf_mbuf_create(
					*pkt_mbuf_pool_out, max_size,
					1, options);
			if ((*mbufs_out)[mbuf_idx] == NULL)
				return -1;
		}
	}

	snprintf(pool_name, sizeof(pool_name), "cperf_op_pool_cdev_%u_qp_%u",
			dev_id, qp_id);

	uint16_t priv_size = RTE_ALIGN_CEIL(test_vector->cipher_iv.length +
		test_vector->auth_iv.length + test_vector->aead_iv.length +
		extra_op_priv_size, 16) +
		RTE_ALIGN_CEIL(options->aead_aad_sz, 16);

	*crypto_op_pool = rte_crypto_op_pool_create(pool_name,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC, options->pool_sz,
			512, priv_size, rte_socket_id());
	if (*crypto_op_pool == NULL)
		return -1;

	return 0;
}

void
cperf_free_common_memory(const struct cperf_options *options,
			struct rte_mempool *pkt_mbuf_pool_in,
			struct rte_mempool *pkt_mbuf_pool_out,
			struct rte_mbuf **mbufs_in,
			struct rte_mbuf **mbufs_out,
			struct rte_mempool *crypto_op_pool)
{
	uint32_t i = 0;

	if (mbufs_in) {
		while (mbufs_in[i] != NULL &&
				i < options->pool_sz)
			rte_pktmbuf_free(mbufs_in[i++]);

		rte_free(mbufs_in);
	}

	if (mbufs_out) {
		i = 0;
		while (mbufs_out[i] != NULL
				&& i < options->pool_sz)
			rte_pktmbuf_free(mbufs_out[i++]);

		rte_free(mbufs_out);
	}

	if (pkt_mbuf_pool_in)
		rte_mempool_free(pkt_mbuf_pool_in);

	if (pkt_mbuf_pool_out)
		rte_mempool_free(pkt_mbuf_pool_out);

	if (crypto_op_pool)
		rte_mempool_free(crypto_op_pool);
}
