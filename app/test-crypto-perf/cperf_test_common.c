/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_mbuf_pool_ops.h>

#include "cperf_test_common.h"

struct obj_params {
	uint32_t src_buf_offset;
	uint32_t dst_buf_offset;
	uint16_t segment_sz;
	uint16_t headroom_sz;
	uint16_t data_len;
	uint16_t segments_nb;
};

static void
fill_single_seg_mbuf(struct rte_mbuf *m, struct rte_mempool *mp,
		void *obj, uint32_t mbuf_offset, uint16_t segment_sz,
		uint16_t headroom, uint16_t data_len)
{
	uint32_t mbuf_hdr_size = sizeof(struct rte_mbuf);

	/* start of buffer is after mbuf structure and priv data */
	m->priv_size = 0;
	m->buf_addr = (char *)m + mbuf_hdr_size;
	m->buf_iova = rte_mempool_virt2iova(obj) +
		mbuf_offset + mbuf_hdr_size;
	m->buf_len = segment_sz;
	m->data_len = data_len;
	m->pkt_len = data_len;

	/* Use headroom specified for the buffer */
	m->data_off = headroom;

	/* init some constant fields */
	m->pool = mp;
	m->nb_segs = 1;
	m->port = 0xff;
	rte_mbuf_refcnt_set(m, 1);
	m->next = NULL;
}

static void
fill_multi_seg_mbuf(struct rte_mbuf *m, struct rte_mempool *mp,
		void *obj, uint32_t mbuf_offset, uint16_t segment_sz,
		uint16_t headroom, uint16_t data_len, uint16_t segments_nb)
{
	uint16_t mbuf_hdr_size = sizeof(struct rte_mbuf);
	uint16_t remaining_segments = segments_nb;
	struct rte_mbuf *next_mbuf;
	rte_iova_t next_seg_phys_addr = rte_mempool_virt2iova(obj) +
			 mbuf_offset + mbuf_hdr_size;

	do {
		/* start of buffer is after mbuf structure and priv data */
		m->priv_size = 0;
		m->buf_addr = (char *)m + mbuf_hdr_size;
		m->buf_iova = next_seg_phys_addr;
		next_seg_phys_addr += mbuf_hdr_size + segment_sz;
		m->buf_len = segment_sz;
		m->data_len = data_len;

		/* Use headroom specified for the buffer */
		m->data_off = headroom;

		/* init some constant fields */
		m->pool = mp;
		m->nb_segs = segments_nb;
		m->port = 0xff;
		rte_mbuf_refcnt_set(m, 1);
		next_mbuf = (struct rte_mbuf *) ((uint8_t *) m +
					mbuf_hdr_size + segment_sz);
		m->next = next_mbuf;
		m = next_mbuf;
		remaining_segments--;

	} while (remaining_segments > 0);

	m->next = NULL;
}

static void
mempool_obj_init(struct rte_mempool *mp,
		 void *opaque_arg,
		 void *obj,
		 __rte_unused unsigned int i)
{
	struct obj_params *params = opaque_arg;
	struct rte_crypto_op *op = obj;
	struct rte_mbuf *m = (struct rte_mbuf *) ((uint8_t *) obj +
					params->src_buf_offset);
	/* Set crypto operation */
	op->type = RTE_CRYPTO_OP_TYPE_SYMMETRIC;
	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	op->sess_type = RTE_CRYPTO_OP_WITH_SESSION;
	op->phys_addr = rte_mem_virt2iova(obj);
	op->mempool = mp;

	/* Set source buffer */
	op->sym->m_src = m;
	if (params->segments_nb == 1)
		fill_single_seg_mbuf(m, mp, obj, params->src_buf_offset,
				params->segment_sz, params->headroom_sz,
				params->data_len);
	else
		fill_multi_seg_mbuf(m, mp, obj, params->src_buf_offset,
				params->segment_sz, params->headroom_sz,
				params->data_len, params->segments_nb);


	/* Set destination buffer */
	if (params->dst_buf_offset) {
		m = (struct rte_mbuf *) ((uint8_t *) obj +
				params->dst_buf_offset);
		fill_single_seg_mbuf(m, mp, obj, params->dst_buf_offset,
				params->segment_sz, params->headroom_sz,
				params->data_len);
		op->sym->m_dst = m;
	} else
		op->sym->m_dst = NULL;
}

int
cperf_alloc_common_memory(const struct cperf_options *options,
			const struct cperf_test_vector *test_vector,
			uint8_t dev_id, uint16_t qp_id,
			size_t extra_op_priv_size,
			uint32_t *src_buf_offset,
			uint32_t *dst_buf_offset,
			struct rte_mempool **pool)
{
	const char *mp_ops_name;
	char pool_name[32] = "";
	int ret;

	/* Calculate the object size */
	uint16_t crypto_op_size = sizeof(struct rte_crypto_op) +
		sizeof(struct rte_crypto_sym_op);
	uint16_t crypto_op_private_size;
	/*
	 * If doing AES-CCM, IV field needs to be 16 bytes long,
	 * and AAD field needs to be long enough to have 18 bytes,
	 * plus the length of the AAD, and all rounded to a
	 * multiple of 16 bytes.
	 */
	if (options->aead_algo == RTE_CRYPTO_AEAD_AES_CCM) {
		crypto_op_private_size = extra_op_priv_size +
			test_vector->cipher_iv.length +
			test_vector->auth_iv.length +
			RTE_ALIGN_CEIL(test_vector->aead_iv.length, 16) +
			RTE_ALIGN_CEIL(options->aead_aad_sz + 18, 16);
	} else {
		crypto_op_private_size = extra_op_priv_size +
			test_vector->cipher_iv.length +
			test_vector->auth_iv.length +
			test_vector->aead_iv.length +
			options->aead_aad_sz;
	}

	uint16_t crypto_op_total_size = crypto_op_size +
				crypto_op_private_size;
	uint16_t crypto_op_total_size_padded =
				RTE_CACHE_LINE_ROUNDUP(crypto_op_total_size);
	uint32_t mbuf_size = sizeof(struct rte_mbuf) + options->segment_sz;
	uint32_t max_size = options->max_buffer_size + options->digest_sz;
	uint16_t segments_nb = (max_size % options->segment_sz) ?
			(max_size / options->segment_sz) + 1 :
			max_size / options->segment_sz;
	uint32_t obj_size = crypto_op_total_size_padded +
				(mbuf_size * segments_nb);

	snprintf(pool_name, sizeof(pool_name), "pool_cdev_%u_qp_%u",
			dev_id, qp_id);

	*src_buf_offset = crypto_op_total_size_padded;

	struct obj_params params = {
		.segment_sz = options->segment_sz,
		.headroom_sz = options->headroom_sz,
		/* Data len = segment size - (headroom + tailroom) */
		.data_len = options->segment_sz -
			    options->headroom_sz -
			    options->tailroom_sz,
		.segments_nb = segments_nb,
		.src_buf_offset = crypto_op_total_size_padded,
		.dst_buf_offset = 0
	};

	if (options->out_of_place) {
		*dst_buf_offset = *src_buf_offset +
				(mbuf_size * segments_nb);
		params.dst_buf_offset = *dst_buf_offset;
		/* Destination buffer will be one segment only */
		obj_size += max_size + sizeof(struct rte_mbuf);
	}

	*pool = rte_mempool_create_empty(pool_name,
			options->pool_sz, obj_size, 512, 0,
			rte_socket_id(), 0);
	if (*pool == NULL) {
		RTE_LOG(ERR, USER1,
			"Cannot allocate mempool for device %u\n",
			dev_id);
		return -1;
	}

	mp_ops_name = rte_mbuf_best_mempool_ops();

	ret = rte_mempool_set_ops_byname(*pool,
		mp_ops_name, NULL);
	if (ret != 0) {
		RTE_LOG(ERR, USER1,
			 "Error setting mempool handler for device %u\n",
			 dev_id);
		return -1;
	}

	ret = rte_mempool_populate_default(*pool);
	if (ret < 0) {
		RTE_LOG(ERR, USER1,
			 "Error populating mempool for device %u\n",
			 dev_id);
		return -1;
	}

	rte_mempool_obj_iter(*pool, mempool_obj_init, (void *)&params);

	return 0;
}
