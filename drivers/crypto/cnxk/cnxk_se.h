/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_SE_H_
#define _CNXK_SE_H_
#include <stdbool.h>

#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"

#define SRC_IOV_SIZE                                                           \
	(sizeof(struct roc_se_iov_ptr) +                                       \
	 (sizeof(struct roc_se_buf_ptr) * ROC_SE_MAX_SG_CNT))
#define DST_IOV_SIZE                                                           \
	(sizeof(struct roc_se_iov_ptr) +                                       \
	 (sizeof(struct roc_se_buf_ptr) * ROC_SE_MAX_SG_CNT))

struct cnxk_se_sess {
	uint16_t cpt_op : 4;
	uint16_t zsk_flag : 4;
	uint16_t aes_gcm : 1;
	uint16_t aes_ctr : 1;
	uint16_t chacha_poly : 1;
	uint16_t is_null : 1;
	uint16_t is_gmac : 1;
	uint16_t rsvd1 : 3;
	uint16_t aad_length;
	uint8_t mac_len;
	uint8_t iv_length;
	uint8_t auth_iv_length;
	uint16_t iv_offset;
	uint16_t auth_iv_offset;
	uint32_t salt;
	uint64_t cpt_inst_w7;
	struct roc_se_ctx roc_se_ctx;
} __rte_cache_aligned;

static inline void
pdcp_iv_copy(uint8_t *iv_d, uint8_t *iv_s, const uint8_t pdcp_alg_type)
{
	uint32_t *iv_s_temp, iv_temp[4];
	int j;

	if (pdcp_alg_type == ROC_SE_PDCP_ALG_TYPE_SNOW3G) {
		/*
		 * DPDK seems to provide it in form of IV3 IV2 IV1 IV0
		 * and BigEndian, MC needs it as IV0 IV1 IV2 IV3
		 */

		iv_s_temp = (uint32_t *)iv_s;

		for (j = 0; j < 4; j++)
			iv_temp[j] = iv_s_temp[3 - j];
		memcpy(iv_d, iv_temp, 16);
	} else {
		/* ZUC doesn't need a swap */
		memcpy(iv_d, iv_s, 16);
	}
}

static __rte_always_inline int
cpt_mac_len_verify(struct rte_crypto_auth_xform *auth)
{
	uint16_t mac_len = auth->digest_length;
	int ret;

	switch (auth->algo) {
	case RTE_CRYPTO_AUTH_MD5:
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		ret = (mac_len == 16) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA1:
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		ret = (mac_len == 20) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA224:
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		ret = (mac_len == 28) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA256:
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		ret = (mac_len == 32) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA384:
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		ret = (mac_len == 48) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA512:
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		ret = (mac_len == 64) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_NULL:
		ret = 0;
		break;
	default:
		ret = -1;
	}

	return ret;
}

static __rte_always_inline void
cpt_fc_salt_update(struct roc_se_ctx *se_ctx, uint8_t *salt)
{
	struct roc_se_context *fctx = &se_ctx->se_ctx.fctx;
	memcpy(fctx->enc.encr_iv, salt, 4);
}

static __rte_always_inline uint32_t
fill_sg_comp(struct roc_se_sglist_comp *list, uint32_t i, phys_addr_t dma_addr,
	     uint32_t size)
{
	struct roc_se_sglist_comp *to = &list[i >> 2];

	to->u.s.len[i % 4] = rte_cpu_to_be_16(size);
	to->ptr[i % 4] = rte_cpu_to_be_64(dma_addr);
	i++;
	return i;
}

static __rte_always_inline uint32_t
fill_sg_comp_from_buf(struct roc_se_sglist_comp *list, uint32_t i,
		      struct roc_se_buf_ptr *from)
{
	struct roc_se_sglist_comp *to = &list[i >> 2];

	to->u.s.len[i % 4] = rte_cpu_to_be_16(from->size);
	to->ptr[i % 4] = rte_cpu_to_be_64((uint64_t)from->vaddr);
	i++;
	return i;
}

static __rte_always_inline uint32_t
fill_sg_comp_from_buf_min(struct roc_se_sglist_comp *list, uint32_t i,
			  struct roc_se_buf_ptr *from, uint32_t *psize)
{
	struct roc_se_sglist_comp *to = &list[i >> 2];
	uint32_t size = *psize;
	uint32_t e_len;

	e_len = (size > from->size) ? from->size : size;
	to->u.s.len[i % 4] = rte_cpu_to_be_16(e_len);
	to->ptr[i % 4] = rte_cpu_to_be_64((uint64_t)from->vaddr);
	*psize -= e_len;
	i++;
	return i;
}

/*
 * This fills the MC expected SGIO list
 * from IOV given by user.
 */
static __rte_always_inline uint32_t
fill_sg_comp_from_iov(struct roc_se_sglist_comp *list, uint32_t i,
		      struct roc_se_iov_ptr *from, uint32_t from_offset,
		      uint32_t *psize, struct roc_se_buf_ptr *extra_buf,
		      uint32_t extra_offset)
{
	int32_t j;
	uint32_t extra_len = extra_buf ? extra_buf->size : 0;
	uint32_t size = *psize;
	struct roc_se_buf_ptr *bufs;

	bufs = from->bufs;
	for (j = 0; (j < from->buf_cnt) && size; j++) {
		uint64_t e_vaddr;
		uint32_t e_len;
		struct roc_se_sglist_comp *to = &list[i >> 2];

		if (unlikely(from_offset)) {
			if (from_offset >= bufs[j].size) {
				from_offset -= bufs[j].size;
				continue;
			}
			e_vaddr = (uint64_t)bufs[j].vaddr + from_offset;
			e_len = (size > (bufs[j].size - from_offset)) ?
					(bufs[j].size - from_offset) :
					size;
			from_offset = 0;
		} else {
			e_vaddr = (uint64_t)bufs[j].vaddr;
			e_len = (size > bufs[j].size) ? bufs[j].size : size;
		}

		to->u.s.len[i % 4] = rte_cpu_to_be_16(e_len);
		to->ptr[i % 4] = rte_cpu_to_be_64(e_vaddr);

		if (extra_len && (e_len >= extra_offset)) {
			/* Break the data at given offset */
			uint32_t next_len = e_len - extra_offset;
			uint64_t next_vaddr = e_vaddr + extra_offset;

			if (!extra_offset) {
				i--;
			} else {
				e_len = extra_offset;
				size -= e_len;
				to->u.s.len[i % 4] = rte_cpu_to_be_16(e_len);
			}

			extra_len = RTE_MIN(extra_len, size);
			/* Insert extra data ptr */
			if (extra_len) {
				i++;
				to = &list[i >> 2];
				to->u.s.len[i % 4] =
					rte_cpu_to_be_16(extra_len);
				to->ptr[i % 4] = rte_cpu_to_be_64(
					(uint64_t)extra_buf->vaddr);
				size -= extra_len;
			}

			next_len = RTE_MIN(next_len, size);
			/* insert the rest of the data */
			if (next_len) {
				i++;
				to = &list[i >> 2];
				to->u.s.len[i % 4] = rte_cpu_to_be_16(next_len);
				to->ptr[i % 4] = rte_cpu_to_be_64(next_vaddr);
				size -= next_len;
			}
			extra_len = 0;

		} else {
			size -= e_len;
		}
		if (extra_offset)
			extra_offset -= size;
		i++;
	}

	*psize = size;
	return (uint32_t)i;
}

static __rte_always_inline int
cpt_digest_gen_prep(uint32_t flags, uint64_t d_lens,
		    struct roc_se_fc_params *params, struct cpt_inst_s *inst)
{
	void *m_vaddr = params->meta_buf.vaddr;
	uint32_t size, i;
	uint16_t data_len, mac_len, key_len;
	roc_se_auth_type hash_type;
	struct roc_se_ctx *ctx;
	struct roc_se_sglist_comp *gather_comp;
	struct roc_se_sglist_comp *scatter_comp;
	uint8_t *in_buffer;
	uint32_t g_size_bytes, s_size_bytes;
	union cpt_inst_w4 cpt_inst_w4;

	ctx = params->ctx_buf.vaddr;

	hash_type = ctx->hash_type;
	mac_len = ctx->mac_len;
	key_len = ctx->auth_key_len;
	data_len = ROC_SE_AUTH_DLEN(d_lens);

	/*GP op header */
	cpt_inst_w4.s.opcode_minor = 0;
	cpt_inst_w4.s.param2 = ((uint16_t)hash_type << 8);
	if (ctx->hmac) {
		cpt_inst_w4.s.opcode_major =
			ROC_SE_MAJOR_OP_HMAC | ROC_SE_DMA_MODE;
		cpt_inst_w4.s.param1 = key_len;
		cpt_inst_w4.s.dlen = data_len + RTE_ALIGN_CEIL(key_len, 8);
	} else {
		cpt_inst_w4.s.opcode_major =
			ROC_SE_MAJOR_OP_HASH | ROC_SE_DMA_MODE;
		cpt_inst_w4.s.param1 = 0;
		cpt_inst_w4.s.dlen = data_len;
	}

	/* Null auth only case enters the if */
	if (unlikely(!hash_type && !ctx->enc_cipher)) {
		cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_MISC;
		/* Minor op is passthrough */
		cpt_inst_w4.s.opcode_minor = 0x03;
		/* Send out completion code only */
		cpt_inst_w4.s.param2 = 0x1;
	}

	/* DPTR has SG list */
	in_buffer = m_vaddr;

	((uint16_t *)in_buffer)[0] = 0;
	((uint16_t *)in_buffer)[1] = 0;

	/* TODO Add error check if space will be sufficient */
	gather_comp = (struct roc_se_sglist_comp *)((uint8_t *)m_vaddr + 8);

	/*
	 * Input gather list
	 */

	i = 0;

	if (ctx->hmac) {
		uint64_t k_vaddr = (uint64_t)ctx->auth_key;
		/* Key */
		i = fill_sg_comp(gather_comp, i, k_vaddr,
				 RTE_ALIGN_CEIL(key_len, 8));
	}

	/* input data */
	size = data_len;
	if (size) {
		i = fill_sg_comp_from_iov(gather_comp, i, params->src_iov, 0,
					  &size, NULL, 0);
		if (unlikely(size)) {
			plt_dp_err("Insufficient dst IOV size, short by %dB",
				   size);
			return -1;
		}
	} else {
		/*
		 * Looks like we need to support zero data
		 * gather ptr in case of hash & hmac
		 */
		i++;
	}
	((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);
	g_size_bytes = ((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

	/*
	 * Output Gather list
	 */

	i = 0;
	scatter_comp = (struct roc_se_sglist_comp *)((uint8_t *)gather_comp +
						     g_size_bytes);

	if (flags & ROC_SE_VALID_MAC_BUF) {
		if (unlikely(params->mac_buf.size < mac_len)) {
			plt_dp_err("Insufficient MAC size");
			return -1;
		}

		size = mac_len;
		i = fill_sg_comp_from_buf_min(scatter_comp, i, &params->mac_buf,
					      &size);
	} else {
		size = mac_len;
		i = fill_sg_comp_from_iov(scatter_comp, i, params->src_iov,
					  data_len, &size, NULL, 0);
		if (unlikely(size)) {
			plt_dp_err("Insufficient dst IOV size, short by %dB",
				   size);
			return -1;
		}
	}

	((uint16_t *)in_buffer)[3] = rte_cpu_to_be_16(i);
	s_size_bytes = ((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

	size = g_size_bytes + s_size_bytes + ROC_SE_SG_LIST_HDR_SIZE;

	/* This is DPTR len in case of SG mode */
	cpt_inst_w4.s.dlen = size;

	inst->dptr = (uint64_t)in_buffer;
	inst->w4.u64 = cpt_inst_w4.u64;

	return 0;
}

static __rte_always_inline int
cpt_enc_hmac_prep(uint32_t flags, uint64_t d_offs, uint64_t d_lens,
		  struct roc_se_fc_params *fc_params, struct cpt_inst_s *inst)
{
	uint32_t iv_offset = 0;
	int32_t inputlen, outputlen, enc_dlen, auth_dlen;
	struct roc_se_ctx *se_ctx;
	uint32_t cipher_type, hash_type;
	uint32_t mac_len, size;
	uint8_t iv_len = 16;
	struct roc_se_buf_ptr *aad_buf = NULL;
	uint32_t encr_offset, auth_offset;
	uint32_t encr_data_len, auth_data_len, aad_len = 0;
	uint32_t passthrough_len = 0;
	union cpt_inst_w4 cpt_inst_w4;
	void *offset_vaddr;
	uint8_t op_minor;

	encr_offset = ROC_SE_ENCR_OFFSET(d_offs);
	auth_offset = ROC_SE_AUTH_OFFSET(d_offs);
	encr_data_len = ROC_SE_ENCR_DLEN(d_lens);
	auth_data_len = ROC_SE_AUTH_DLEN(d_lens);
	if (unlikely(flags & ROC_SE_VALID_AAD_BUF)) {
		/* We don't support both AAD and auth data separately */
		auth_data_len = 0;
		auth_offset = 0;
		aad_len = fc_params->aad_buf.size;
		aad_buf = &fc_params->aad_buf;
	}
	se_ctx = fc_params->ctx_buf.vaddr;
	cipher_type = se_ctx->enc_cipher;
	hash_type = se_ctx->hash_type;
	mac_len = se_ctx->mac_len;
	op_minor = se_ctx->template_w4.s.opcode_minor;

	if (unlikely(!(flags & ROC_SE_VALID_IV_BUF))) {
		iv_len = 0;
		iv_offset = ROC_SE_ENCR_IV_OFFSET(d_offs);
	}

	if (unlikely(flags & ROC_SE_VALID_AAD_BUF)) {
		/*
		 * When AAD is given, data above encr_offset is pass through
		 * Since AAD is given as separate pointer and not as offset,
		 * this is a special case as we need to fragment input data
		 * into passthrough + encr_data and then insert AAD in between.
		 */
		if (hash_type != ROC_SE_GMAC_TYPE) {
			passthrough_len = encr_offset;
			auth_offset = passthrough_len + iv_len;
			encr_offset = passthrough_len + aad_len + iv_len;
			auth_data_len = aad_len + encr_data_len;
		} else {
			passthrough_len = 16 + aad_len;
			auth_offset = passthrough_len + iv_len;
			auth_data_len = aad_len;
		}
	} else {
		encr_offset += iv_len;
		auth_offset += iv_len;
	}

	/* Encryption */
	cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_FC;
	cpt_inst_w4.s.opcode_minor = ROC_SE_FC_MINOR_OP_ENCRYPT;
	cpt_inst_w4.s.opcode_minor |= (uint64_t)op_minor;

	if (hash_type == ROC_SE_GMAC_TYPE) {
		encr_offset = 0;
		encr_data_len = 0;
	}

	auth_dlen = auth_offset + auth_data_len;
	enc_dlen = encr_data_len + encr_offset;
	if (unlikely(encr_data_len & 0xf)) {
		if ((cipher_type == ROC_SE_DES3_CBC) ||
		    (cipher_type == ROC_SE_DES3_ECB))
			enc_dlen =
				RTE_ALIGN_CEIL(encr_data_len, 8) + encr_offset;
		else if (likely((cipher_type == ROC_SE_AES_CBC) ||
				(cipher_type == ROC_SE_AES_ECB)))
			enc_dlen =
				RTE_ALIGN_CEIL(encr_data_len, 8) + encr_offset;
	}

	if (unlikely(auth_dlen > enc_dlen)) {
		inputlen = auth_dlen;
		outputlen = auth_dlen + mac_len;
	} else {
		inputlen = enc_dlen;
		outputlen = enc_dlen + mac_len;
	}

	if (op_minor & ROC_SE_FC_MINOR_OP_HMAC_FIRST)
		outputlen = enc_dlen;

	/* GP op header */
	cpt_inst_w4.s.param1 = encr_data_len;
	cpt_inst_w4.s.param2 = auth_data_len;

	/*
	 * In cn9k, cn10k since we have a limitation of
	 * IV & Offset control word not part of instruction
	 * and need to be part of Data Buffer, we check if
	 * head room is there and then only do the Direct mode processing
	 */
	if (likely((flags & ROC_SE_SINGLE_BUF_INPLACE) &&
		   (flags & ROC_SE_SINGLE_BUF_HEADROOM))) {
		void *dm_vaddr = fc_params->bufs[0].vaddr;

		/* Use Direct mode */

		offset_vaddr =
			(uint8_t *)dm_vaddr - ROC_SE_OFF_CTRL_LEN - iv_len;

		/* DPTR */
		inst->dptr = (uint64_t)offset_vaddr;

		/* RPTR should just exclude offset control word */
		inst->rptr = (uint64_t)dm_vaddr - iv_len;

		cpt_inst_w4.s.dlen = inputlen + ROC_SE_OFF_CTRL_LEN;

		if (likely(iv_len)) {
			uint64_t *dest = (uint64_t *)((uint8_t *)offset_vaddr +
						      ROC_SE_OFF_CTRL_LEN);
			uint64_t *src = fc_params->iv_buf;
			dest[0] = src[0];
			dest[1] = src[1];
		}

	} else {
		void *m_vaddr = fc_params->meta_buf.vaddr;
		uint32_t i, g_size_bytes, s_size_bytes;
		struct roc_se_sglist_comp *gather_comp;
		struct roc_se_sglist_comp *scatter_comp;
		uint8_t *in_buffer;

		/* This falls under strict SG mode */
		offset_vaddr = m_vaddr;
		size = ROC_SE_OFF_CTRL_LEN + iv_len;

		m_vaddr = (uint8_t *)m_vaddr + size;

		cpt_inst_w4.s.opcode_major |= (uint64_t)ROC_SE_DMA_MODE;

		if (likely(iv_len)) {
			uint64_t *dest = (uint64_t *)((uint8_t *)offset_vaddr +
						      ROC_SE_OFF_CTRL_LEN);
			uint64_t *src = fc_params->iv_buf;
			dest[0] = src[0];
			dest[1] = src[1];
		}

		/* DPTR has SG list */
		in_buffer = m_vaddr;

		((uint16_t *)in_buffer)[0] = 0;
		((uint16_t *)in_buffer)[1] = 0;

		/* TODO Add error check if space will be sufficient */
		gather_comp =
			(struct roc_se_sglist_comp *)((uint8_t *)m_vaddr + 8);

		/*
		 * Input Gather List
		 */

		i = 0;

		/* Offset control word that includes iv */
		i = fill_sg_comp(gather_comp, i, (uint64_t)offset_vaddr,
				 ROC_SE_OFF_CTRL_LEN + iv_len);

		/* Add input data */
		size = inputlen - iv_len;
		if (likely(size)) {
			uint32_t aad_offset = aad_len ? passthrough_len : 0;

			if (unlikely(flags & ROC_SE_SINGLE_BUF_INPLACE)) {
				i = fill_sg_comp_from_buf_min(
					gather_comp, i, fc_params->bufs, &size);
			} else {
				i = fill_sg_comp_from_iov(
					gather_comp, i, fc_params->src_iov, 0,
					&size, aad_buf, aad_offset);
			}

			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer space,"
					   " size %d needed",
					   size);
				return -1;
			}
		}
		((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);
		g_size_bytes =
			((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

		/*
		 * Output Scatter list
		 */
		i = 0;
		scatter_comp =
			(struct roc_se_sglist_comp *)((uint8_t *)gather_comp +
						      g_size_bytes);

		/* Add IV */
		if (likely(iv_len)) {
			i = fill_sg_comp(scatter_comp, i,
					 (uint64_t)offset_vaddr +
						 ROC_SE_OFF_CTRL_LEN,
					 iv_len);
		}

		/* output data or output data + digest*/
		if (unlikely(flags & ROC_SE_VALID_MAC_BUF)) {
			size = outputlen - iv_len - mac_len;
			if (size) {
				uint32_t aad_offset =
					aad_len ? passthrough_len : 0;

				if (unlikely(flags &
					     ROC_SE_SINGLE_BUF_INPLACE)) {
					i = fill_sg_comp_from_buf_min(
						scatter_comp, i,
						fc_params->bufs, &size);
				} else {
					i = fill_sg_comp_from_iov(
						scatter_comp, i,
						fc_params->dst_iov, 0, &size,
						aad_buf, aad_offset);
				}
				if (unlikely(size)) {
					plt_dp_err("Insufficient buffer"
						   " space, size %d needed",
						   size);
					return -1;
				}
			}
			/* mac_data */
			if (mac_len) {
				i = fill_sg_comp_from_buf(scatter_comp, i,
							  &fc_params->mac_buf);
			}
		} else {
			/* Output including mac */
			size = outputlen - iv_len;
			if (likely(size)) {
				uint32_t aad_offset =
					aad_len ? passthrough_len : 0;

				if (unlikely(flags &
					     ROC_SE_SINGLE_BUF_INPLACE)) {
					i = fill_sg_comp_from_buf_min(
						scatter_comp, i,
						fc_params->bufs, &size);
				} else {
					i = fill_sg_comp_from_iov(
						scatter_comp, i,
						fc_params->dst_iov, 0, &size,
						aad_buf, aad_offset);
				}
				if (unlikely(size)) {
					plt_dp_err("Insufficient buffer"
						   " space, size %d needed",
						   size);
					return -1;
				}
			}
		}
		((uint16_t *)in_buffer)[3] = rte_cpu_to_be_16(i);
		s_size_bytes =
			((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

		size = g_size_bytes + s_size_bytes + ROC_SE_SG_LIST_HDR_SIZE;

		/* This is DPTR len in case of SG mode */
		cpt_inst_w4.s.dlen = size;

		inst->dptr = (uint64_t)in_buffer;
	}

	if (unlikely((encr_offset >> 16) || (iv_offset >> 8) ||
		     (auth_offset >> 8))) {
		plt_dp_err("Offset not supported");
		plt_dp_err("enc_offset: %d", encr_offset);
		plt_dp_err("iv_offset : %d", iv_offset);
		plt_dp_err("auth_offset: %d", auth_offset);
		return -1;
	}

	*(uint64_t *)offset_vaddr = rte_cpu_to_be_64(
		((uint64_t)encr_offset << 16) | ((uint64_t)iv_offset << 8) |
		((uint64_t)auth_offset));

	inst->w4.u64 = cpt_inst_w4.u64;
	return 0;
}

static __rte_always_inline int
cpt_dec_hmac_prep(uint32_t flags, uint64_t d_offs, uint64_t d_lens,
		  struct roc_se_fc_params *fc_params, struct cpt_inst_s *inst)
{
	uint32_t iv_offset = 0, size;
	int32_t inputlen, outputlen, enc_dlen, auth_dlen;
	struct roc_se_ctx *se_ctx;
	int32_t hash_type, mac_len;
	uint8_t iv_len = 16;
	struct roc_se_buf_ptr *aad_buf = NULL;
	uint32_t encr_offset, auth_offset;
	uint32_t encr_data_len, auth_data_len, aad_len = 0;
	uint32_t passthrough_len = 0;
	union cpt_inst_w4 cpt_inst_w4;
	void *offset_vaddr;
	uint8_t op_minor;

	encr_offset = ROC_SE_ENCR_OFFSET(d_offs);
	auth_offset = ROC_SE_AUTH_OFFSET(d_offs);
	encr_data_len = ROC_SE_ENCR_DLEN(d_lens);
	auth_data_len = ROC_SE_AUTH_DLEN(d_lens);

	if (unlikely(flags & ROC_SE_VALID_AAD_BUF)) {
		/* We don't support both AAD and auth data separately */
		auth_data_len = 0;
		auth_offset = 0;
		aad_len = fc_params->aad_buf.size;
		aad_buf = &fc_params->aad_buf;
	}

	se_ctx = fc_params->ctx_buf.vaddr;
	hash_type = se_ctx->hash_type;
	mac_len = se_ctx->mac_len;
	op_minor = se_ctx->template_w4.s.opcode_minor;

	if (unlikely(!(flags & ROC_SE_VALID_IV_BUF))) {
		iv_len = 0;
		iv_offset = ROC_SE_ENCR_IV_OFFSET(d_offs);
	}

	if (unlikely(flags & ROC_SE_VALID_AAD_BUF)) {
		/*
		 * When AAD is given, data above encr_offset is pass through
		 * Since AAD is given as separate pointer and not as offset,
		 * this is a special case as we need to fragment input data
		 * into passthrough + encr_data and then insert AAD in between.
		 */
		if (hash_type != ROC_SE_GMAC_TYPE) {
			passthrough_len = encr_offset;
			auth_offset = passthrough_len + iv_len;
			encr_offset = passthrough_len + aad_len + iv_len;
			auth_data_len = aad_len + encr_data_len;
		} else {
			passthrough_len = 16 + aad_len;
			auth_offset = passthrough_len + iv_len;
			auth_data_len = aad_len;
		}
	} else {
		encr_offset += iv_len;
		auth_offset += iv_len;
	}

	/* Decryption */
	cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_FC;
	cpt_inst_w4.s.opcode_minor = ROC_SE_FC_MINOR_OP_DECRYPT;
	cpt_inst_w4.s.opcode_minor |= (uint64_t)op_minor;

	if (hash_type == ROC_SE_GMAC_TYPE) {
		encr_offset = 0;
		encr_data_len = 0;
	}

	enc_dlen = encr_offset + encr_data_len;
	auth_dlen = auth_offset + auth_data_len;

	if (auth_dlen > enc_dlen) {
		inputlen = auth_dlen + mac_len;
		outputlen = auth_dlen;
	} else {
		inputlen = enc_dlen + mac_len;
		outputlen = enc_dlen;
	}

	if (op_minor & ROC_SE_FC_MINOR_OP_HMAC_FIRST)
		outputlen = inputlen = enc_dlen;

	cpt_inst_w4.s.param1 = encr_data_len;
	cpt_inst_w4.s.param2 = auth_data_len;

	/*
	 * In cn9k, cn10k since we have a limitation of
	 * IV & Offset control word not part of instruction
	 * and need to be part of Data Buffer, we check if
	 * head room is there and then only do the Direct mode processing
	 */
	if (likely((flags & ROC_SE_SINGLE_BUF_INPLACE) &&
		   (flags & ROC_SE_SINGLE_BUF_HEADROOM))) {
		void *dm_vaddr = fc_params->bufs[0].vaddr;

		/* Use Direct mode */

		offset_vaddr =
			(uint8_t *)dm_vaddr - ROC_SE_OFF_CTRL_LEN - iv_len;
		inst->dptr = (uint64_t)offset_vaddr;

		/* RPTR should just exclude offset control word */
		inst->rptr = (uint64_t)dm_vaddr - iv_len;

		cpt_inst_w4.s.dlen = inputlen + ROC_SE_OFF_CTRL_LEN;

		if (likely(iv_len)) {
			uint64_t *dest = (uint64_t *)((uint8_t *)offset_vaddr +
						      ROC_SE_OFF_CTRL_LEN);
			uint64_t *src = fc_params->iv_buf;
			dest[0] = src[0];
			dest[1] = src[1];
		}

	} else {
		void *m_vaddr = fc_params->meta_buf.vaddr;
		uint32_t g_size_bytes, s_size_bytes;
		struct roc_se_sglist_comp *gather_comp;
		struct roc_se_sglist_comp *scatter_comp;
		uint8_t *in_buffer;
		uint8_t i = 0;

		/* This falls under strict SG mode */
		offset_vaddr = m_vaddr;
		size = ROC_SE_OFF_CTRL_LEN + iv_len;

		m_vaddr = (uint8_t *)m_vaddr + size;

		cpt_inst_w4.s.opcode_major |= (uint64_t)ROC_SE_DMA_MODE;

		if (likely(iv_len)) {
			uint64_t *dest = (uint64_t *)((uint8_t *)offset_vaddr +
						      ROC_SE_OFF_CTRL_LEN);
			uint64_t *src = fc_params->iv_buf;
			dest[0] = src[0];
			dest[1] = src[1];
		}

		/* DPTR has SG list */
		in_buffer = m_vaddr;

		((uint16_t *)in_buffer)[0] = 0;
		((uint16_t *)in_buffer)[1] = 0;

		/* TODO Add error check if space will be sufficient */
		gather_comp =
			(struct roc_se_sglist_comp *)((uint8_t *)m_vaddr + 8);

		/*
		 * Input Gather List
		 */
		i = 0;

		/* Offset control word that includes iv */
		i = fill_sg_comp(gather_comp, i, (uint64_t)offset_vaddr,
				 ROC_SE_OFF_CTRL_LEN + iv_len);

		/* Add input data */
		if (flags & ROC_SE_VALID_MAC_BUF) {
			size = inputlen - iv_len - mac_len;
			if (size) {
				/* input data only */
				if (unlikely(flags &
					     ROC_SE_SINGLE_BUF_INPLACE)) {
					i = fill_sg_comp_from_buf_min(
						gather_comp, i, fc_params->bufs,
						&size);
				} else {
					uint32_t aad_offset =
						aad_len ? passthrough_len : 0;

					i = fill_sg_comp_from_iov(
						gather_comp, i,
						fc_params->src_iov, 0, &size,
						aad_buf, aad_offset);
				}
				if (unlikely(size)) {
					plt_dp_err("Insufficient buffer"
						   " space, size %d needed",
						   size);
					return -1;
				}
			}

			/* mac data */
			if (mac_len) {
				i = fill_sg_comp_from_buf(gather_comp, i,
							  &fc_params->mac_buf);
			}
		} else {
			/* input data + mac */
			size = inputlen - iv_len;
			if (size) {
				if (unlikely(flags &
					     ROC_SE_SINGLE_BUF_INPLACE)) {
					i = fill_sg_comp_from_buf_min(
						gather_comp, i, fc_params->bufs,
						&size);
				} else {
					uint32_t aad_offset =
						aad_len ? passthrough_len : 0;

					if (unlikely(!fc_params->src_iov)) {
						plt_dp_err("Bad input args");
						return -1;
					}

					i = fill_sg_comp_from_iov(
						gather_comp, i,
						fc_params->src_iov, 0, &size,
						aad_buf, aad_offset);
				}

				if (unlikely(size)) {
					plt_dp_err("Insufficient buffer"
						   " space, size %d needed",
						   size);
					return -1;
				}
			}
		}
		((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);
		g_size_bytes =
			((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

		/*
		 * Output Scatter List
		 */

		i = 0;
		scatter_comp =
			(struct roc_se_sglist_comp *)((uint8_t *)gather_comp +
						      g_size_bytes);

		/* Add iv */
		if (iv_len) {
			i = fill_sg_comp(scatter_comp, i,
					 (uint64_t)offset_vaddr +
						 ROC_SE_OFF_CTRL_LEN,
					 iv_len);
		}

		/* Add output data */
		size = outputlen - iv_len;
		if (size) {
			if (unlikely(flags & ROC_SE_SINGLE_BUF_INPLACE)) {
				/* handle single buffer here */
				i = fill_sg_comp_from_buf_min(scatter_comp, i,
							      fc_params->bufs,
							      &size);
			} else {
				uint32_t aad_offset =
					aad_len ? passthrough_len : 0;

				if (unlikely(!fc_params->dst_iov)) {
					plt_dp_err("Bad input args");
					return -1;
				}

				i = fill_sg_comp_from_iov(
					scatter_comp, i, fc_params->dst_iov, 0,
					&size, aad_buf, aad_offset);
			}

			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer space,"
					   " size %d needed",
					   size);
				return -1;
			}
		}

		((uint16_t *)in_buffer)[3] = rte_cpu_to_be_16(i);
		s_size_bytes =
			((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

		size = g_size_bytes + s_size_bytes + ROC_SE_SG_LIST_HDR_SIZE;

		/* This is DPTR len in case of SG mode */
		cpt_inst_w4.s.dlen = size;

		inst->dptr = (uint64_t)in_buffer;
	}

	if (unlikely((encr_offset >> 16) || (iv_offset >> 8) ||
		     (auth_offset >> 8))) {
		plt_dp_err("Offset not supported");
		plt_dp_err("enc_offset: %d", encr_offset);
		plt_dp_err("iv_offset : %d", iv_offset);
		plt_dp_err("auth_offset: %d", auth_offset);
		return -1;
	}

	*(uint64_t *)offset_vaddr = rte_cpu_to_be_64(
		((uint64_t)encr_offset << 16) | ((uint64_t)iv_offset << 8) |
		((uint64_t)auth_offset));

	inst->w4.u64 = cpt_inst_w4.u64;
	return 0;
}

static __rte_always_inline int
cpt_zuc_snow3g_prep(uint32_t req_flags, uint64_t d_offs, uint64_t d_lens,
		    struct roc_se_fc_params *params, struct cpt_inst_s *inst)
{
	uint32_t size;
	int32_t inputlen, outputlen;
	struct roc_se_ctx *se_ctx;
	uint32_t mac_len = 0;
	uint8_t pdcp_alg_type;
	uint32_t encr_offset, auth_offset;
	uint32_t encr_data_len, auth_data_len;
	int flags, iv_len = 16;
	uint64_t offset_ctrl;
	uint64_t *offset_vaddr;
	uint8_t *iv_s;
	union cpt_inst_w4 cpt_inst_w4;

	se_ctx = params->ctx_buf.vaddr;
	flags = se_ctx->zsk_flags;
	mac_len = se_ctx->mac_len;
	pdcp_alg_type = se_ctx->pdcp_alg_type;

	cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_ZUC_SNOW3G;

	cpt_inst_w4.s.opcode_minor = se_ctx->template_w4.s.opcode_minor;

	if (flags == 0x1) {
		/*
		 * Microcode expects offsets in bytes
		 * TODO: Rounding off
		 */
		auth_data_len = ROC_SE_AUTH_DLEN(d_lens);

		/* EIA3 or UIA2 */
		auth_offset = ROC_SE_AUTH_OFFSET(d_offs);
		auth_offset = auth_offset / 8;

		/* consider iv len */
		auth_offset += iv_len;

		inputlen = auth_offset + (RTE_ALIGN(auth_data_len, 8) / 8);
		outputlen = mac_len;

		offset_ctrl = rte_cpu_to_be_64((uint64_t)auth_offset);

		encr_data_len = 0;
		encr_offset = 0;

		iv_s = params->auth_iv_buf;
	} else {
		/* EEA3 or UEA2 */
		/*
		 * Microcode expects offsets in bytes
		 * TODO: Rounding off
		 */
		encr_data_len = ROC_SE_ENCR_DLEN(d_lens);

		encr_offset = ROC_SE_ENCR_OFFSET(d_offs);
		encr_offset = encr_offset / 8;
		/* consider iv len */
		encr_offset += iv_len;

		inputlen = encr_offset + (RTE_ALIGN(encr_data_len, 8) / 8);
		outputlen = inputlen;

		/* iv offset is 0 */
		offset_ctrl = rte_cpu_to_be_64((uint64_t)encr_offset << 16);

		auth_data_len = 0;
		auth_offset = 0;

		iv_s = params->iv_buf;
	}

	if (unlikely((encr_offset >> 16) || (auth_offset >> 8))) {
		plt_dp_err("Offset not supported");
		plt_dp_err("enc_offset: %d", encr_offset);
		plt_dp_err("auth_offset: %d", auth_offset);
		return -1;
	}

	/*
	 * GP op header, lengths are expected in bits.
	 */
	cpt_inst_w4.s.param1 = encr_data_len;
	cpt_inst_w4.s.param2 = auth_data_len;

	/*
	 * In cn9k, cn10k since we have a limitation of
	 * IV & Offset control word not part of instruction
	 * and need to be part of Data Buffer, we check if
	 * head room is there and then only do the Direct mode processing
	 */
	if (likely((req_flags & ROC_SE_SINGLE_BUF_INPLACE) &&
		   (req_flags & ROC_SE_SINGLE_BUF_HEADROOM))) {
		void *dm_vaddr = params->bufs[0].vaddr;

		/* Use Direct mode */

		offset_vaddr = (uint64_t *)((uint8_t *)dm_vaddr -
					    ROC_SE_OFF_CTRL_LEN - iv_len);

		/* DPTR */
		inst->dptr = (uint64_t)offset_vaddr;
		/* RPTR should just exclude offset control word */
		inst->rptr = (uint64_t)dm_vaddr - iv_len;

		cpt_inst_w4.s.dlen = inputlen + ROC_SE_OFF_CTRL_LEN;

		uint8_t *iv_d = ((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN);
		pdcp_iv_copy(iv_d, iv_s, pdcp_alg_type);

		*offset_vaddr = offset_ctrl;
	} else {
		void *m_vaddr = params->meta_buf.vaddr;
		uint32_t i, g_size_bytes, s_size_bytes;
		struct roc_se_sglist_comp *gather_comp;
		struct roc_se_sglist_comp *scatter_comp;
		uint8_t *in_buffer;
		uint8_t *iv_d;

		/* save space for iv */
		offset_vaddr = m_vaddr;

		m_vaddr = (uint8_t *)m_vaddr + ROC_SE_OFF_CTRL_LEN + iv_len;

		cpt_inst_w4.s.opcode_major |= (uint64_t)ROC_SE_DMA_MODE;

		/* DPTR has SG list */
		in_buffer = m_vaddr;

		((uint16_t *)in_buffer)[0] = 0;
		((uint16_t *)in_buffer)[1] = 0;

		/* TODO Add error check if space will be sufficient */
		gather_comp =
			(struct roc_se_sglist_comp *)((uint8_t *)m_vaddr + 8);

		/*
		 * Input Gather List
		 */
		i = 0;

		/* Offset control word followed by iv */

		i = fill_sg_comp(gather_comp, i, (uint64_t)offset_vaddr,
				 ROC_SE_OFF_CTRL_LEN + iv_len);

		/* iv offset is 0 */
		*offset_vaddr = offset_ctrl;

		iv_d = ((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN);
		pdcp_iv_copy(iv_d, iv_s, pdcp_alg_type);

		/* input data */
		size = inputlen - iv_len;
		if (size) {
			i = fill_sg_comp_from_iov(gather_comp, i,
						  params->src_iov, 0, &size,
						  NULL, 0);
			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer space,"
					   " size %d needed",
					   size);
				return -1;
			}
		}
		((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);
		g_size_bytes =
			((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

		/*
		 * Output Scatter List
		 */

		i = 0;
		scatter_comp =
			(struct roc_se_sglist_comp *)((uint8_t *)gather_comp +
						      g_size_bytes);

		if (flags == 0x1) {
			/* IV in SLIST only for EEA3 & UEA2 */
			iv_len = 0;
		}

		if (iv_len) {
			i = fill_sg_comp(scatter_comp, i,
					 (uint64_t)offset_vaddr +
						 ROC_SE_OFF_CTRL_LEN,
					 iv_len);
		}

		/* Add output data */
		if (req_flags & ROC_SE_VALID_MAC_BUF) {
			size = outputlen - iv_len - mac_len;
			if (size) {
				i = fill_sg_comp_from_iov(scatter_comp, i,
							  params->dst_iov, 0,
							  &size, NULL, 0);

				if (unlikely(size)) {
					plt_dp_err("Insufficient buffer space,"
						   " size %d needed",
						   size);
					return -1;
				}
			}

			/* mac data */
			if (mac_len) {
				i = fill_sg_comp_from_buf(scatter_comp, i,
							  &params->mac_buf);
			}
		} else {
			/* Output including mac */
			size = outputlen - iv_len;
			if (size) {
				i = fill_sg_comp_from_iov(scatter_comp, i,
							  params->dst_iov, 0,
							  &size, NULL, 0);

				if (unlikely(size)) {
					plt_dp_err("Insufficient buffer space,"
						   " size %d needed",
						   size);
					return -1;
				}
			}
		}
		((uint16_t *)in_buffer)[3] = rte_cpu_to_be_16(i);
		s_size_bytes =
			((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

		size = g_size_bytes + s_size_bytes + ROC_SE_SG_LIST_HDR_SIZE;

		/* This is DPTR len in case of SG mode */
		cpt_inst_w4.s.dlen = size;

		inst->dptr = (uint64_t)in_buffer;
	}

	inst->w4.u64 = cpt_inst_w4.u64;

	return 0;
}

static __rte_always_inline int
cpt_kasumi_enc_prep(uint32_t req_flags, uint64_t d_offs, uint64_t d_lens,
		    struct roc_se_fc_params *params, struct cpt_inst_s *inst)
{
	void *m_vaddr = params->meta_buf.vaddr;
	uint32_t size;
	int32_t inputlen = 0, outputlen = 0;
	struct roc_se_ctx *se_ctx;
	uint32_t mac_len = 0;
	uint8_t i = 0;
	uint32_t encr_offset, auth_offset;
	uint32_t encr_data_len, auth_data_len;
	int flags;
	uint8_t *iv_s, *iv_d, iv_len = 8;
	uint8_t dir = 0;
	uint64_t *offset_vaddr;
	union cpt_inst_w4 cpt_inst_w4;
	uint8_t *in_buffer;
	uint32_t g_size_bytes, s_size_bytes;
	struct roc_se_sglist_comp *gather_comp;
	struct roc_se_sglist_comp *scatter_comp;

	encr_offset = ROC_SE_ENCR_OFFSET(d_offs) / 8;
	auth_offset = ROC_SE_AUTH_OFFSET(d_offs) / 8;
	encr_data_len = ROC_SE_ENCR_DLEN(d_lens);
	auth_data_len = ROC_SE_AUTH_DLEN(d_lens);

	se_ctx = params->ctx_buf.vaddr;
	flags = se_ctx->zsk_flags;
	mac_len = se_ctx->mac_len;

	if (flags == 0x0)
		iv_s = params->iv_buf;
	else
		iv_s = params->auth_iv_buf;

	dir = iv_s[8] & 0x1;

	cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_KASUMI | ROC_SE_DMA_MODE;

	/* indicates ECB/CBC, direction, ctx from cptr, iv from dptr */
	cpt_inst_w4.s.opcode_minor = ((1 << 6) | (se_ctx->k_ecb << 5) |
				      (dir << 4) | (0 << 3) | (flags & 0x7));

	/*
	 * GP op header, lengths are expected in bits.
	 */
	cpt_inst_w4.s.param1 = encr_data_len;
	cpt_inst_w4.s.param2 = auth_data_len;

	/* consider iv len */
	if (flags == 0x0) {
		encr_offset += iv_len;
		auth_offset += iv_len;
	}

	/* save space for offset ctrl and iv */
	offset_vaddr = m_vaddr;

	m_vaddr = (uint8_t *)m_vaddr + ROC_SE_OFF_CTRL_LEN + iv_len;

	/* DPTR has SG list */
	in_buffer = m_vaddr;

	((uint16_t *)in_buffer)[0] = 0;
	((uint16_t *)in_buffer)[1] = 0;

	/* TODO Add error check if space will be sufficient */
	gather_comp = (struct roc_se_sglist_comp *)((uint8_t *)m_vaddr + 8);

	/*
	 * Input Gather List
	 */
	i = 0;

	/* Offset control word followed by iv */

	if (flags == 0x0) {
		inputlen = encr_offset + (RTE_ALIGN(encr_data_len, 8) / 8);
		outputlen = inputlen;
		/* iv offset is 0 */
		*offset_vaddr = rte_cpu_to_be_64((uint64_t)encr_offset << 16);
		if (unlikely((encr_offset >> 16))) {
			plt_dp_err("Offset not supported");
			plt_dp_err("enc_offset: %d", encr_offset);
			return -1;
		}
	} else {
		inputlen = auth_offset + (RTE_ALIGN(auth_data_len, 8) / 8);
		outputlen = mac_len;
		/* iv offset is 0 */
		*offset_vaddr = rte_cpu_to_be_64((uint64_t)auth_offset);
		if (unlikely((auth_offset >> 8))) {
			plt_dp_err("Offset not supported");
			plt_dp_err("auth_offset: %d", auth_offset);
			return -1;
		}
	}

	i = fill_sg_comp(gather_comp, i, (uint64_t)offset_vaddr,
			 ROC_SE_OFF_CTRL_LEN + iv_len);

	/* IV */
	iv_d = (uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN;
	memcpy(iv_d, iv_s, iv_len);

	/* input data */
	size = inputlen - iv_len;
	if (size) {
		i = fill_sg_comp_from_iov(gather_comp, i, params->src_iov, 0,
					  &size, NULL, 0);

		if (unlikely(size)) {
			plt_dp_err("Insufficient buffer space,"
				   " size %d needed",
				   size);
			return -1;
		}
	}
	((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);
	g_size_bytes = ((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

	/*
	 * Output Scatter List
	 */

	i = 0;
	scatter_comp = (struct roc_se_sglist_comp *)((uint8_t *)gather_comp +
						     g_size_bytes);

	if (flags == 0x1) {
		/* IV in SLIST only for F8 */
		iv_len = 0;
	}

	/* IV */
	if (iv_len) {
		i = fill_sg_comp(scatter_comp, i,
				 (uint64_t)offset_vaddr + ROC_SE_OFF_CTRL_LEN,
				 iv_len);
	}

	/* Add output data */
	if (req_flags & ROC_SE_VALID_MAC_BUF) {
		size = outputlen - iv_len - mac_len;
		if (size) {
			i = fill_sg_comp_from_iov(scatter_comp, i,
						  params->dst_iov, 0, &size,
						  NULL, 0);

			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer space,"
					   " size %d needed",
					   size);
				return -1;
			}
		}

		/* mac data */
		if (mac_len) {
			i = fill_sg_comp_from_buf(scatter_comp, i,
						  &params->mac_buf);
		}
	} else {
		/* Output including mac */
		size = outputlen - iv_len;
		if (size) {
			i = fill_sg_comp_from_iov(scatter_comp, i,
						  params->dst_iov, 0, &size,
						  NULL, 0);

			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer space,"
					   " size %d needed",
					   size);
				return -1;
			}
		}
	}
	((uint16_t *)in_buffer)[3] = rte_cpu_to_be_16(i);
	s_size_bytes = ((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

	size = g_size_bytes + s_size_bytes + ROC_SE_SG_LIST_HDR_SIZE;

	/* This is DPTR len in case of SG mode */
	cpt_inst_w4.s.dlen = size;

	inst->dptr = (uint64_t)in_buffer;
	inst->w4.u64 = cpt_inst_w4.u64;

	return 0;
}

static __rte_always_inline int
cpt_kasumi_dec_prep(uint64_t d_offs, uint64_t d_lens,
		    struct roc_se_fc_params *params, struct cpt_inst_s *inst)
{
	void *m_vaddr = params->meta_buf.vaddr;
	uint32_t size;
	int32_t inputlen = 0, outputlen;
	struct roc_se_ctx *se_ctx;
	uint8_t i = 0, iv_len = 8;
	uint32_t encr_offset;
	uint32_t encr_data_len;
	int flags;
	uint8_t dir = 0;
	uint64_t *offset_vaddr;
	union cpt_inst_w4 cpt_inst_w4;
	uint8_t *in_buffer;
	uint32_t g_size_bytes, s_size_bytes;
	struct roc_se_sglist_comp *gather_comp;
	struct roc_se_sglist_comp *scatter_comp;

	encr_offset = ROC_SE_ENCR_OFFSET(d_offs) / 8;
	encr_data_len = ROC_SE_ENCR_DLEN(d_lens);

	se_ctx = params->ctx_buf.vaddr;
	flags = se_ctx->zsk_flags;

	cpt_inst_w4.u64 = 0;
	cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_KASUMI | ROC_SE_DMA_MODE;

	/* indicates ECB/CBC, direction, ctx from cptr, iv from dptr */
	cpt_inst_w4.s.opcode_minor = ((1 << 6) | (se_ctx->k_ecb << 5) |
				      (dir << 4) | (0 << 3) | (flags & 0x7));

	/*
	 * GP op header, lengths are expected in bits.
	 */
	cpt_inst_w4.s.param1 = encr_data_len;

	/* consider iv len */
	encr_offset += iv_len;

	inputlen = iv_len + (RTE_ALIGN(encr_data_len, 8) / 8);
	outputlen = inputlen;

	/* save space for offset ctrl & iv */
	offset_vaddr = m_vaddr;

	m_vaddr = (uint8_t *)m_vaddr + ROC_SE_OFF_CTRL_LEN + iv_len;

	/* DPTR has SG list */
	in_buffer = m_vaddr;

	((uint16_t *)in_buffer)[0] = 0;
	((uint16_t *)in_buffer)[1] = 0;

	/* TODO Add error check if space will be sufficient */
	gather_comp = (struct roc_se_sglist_comp *)((uint8_t *)m_vaddr + 8);

	/*
	 * Input Gather List
	 */
	i = 0;

	/* Offset control word followed by iv */
	*offset_vaddr = rte_cpu_to_be_64((uint64_t)encr_offset << 16);
	if (unlikely((encr_offset >> 16))) {
		plt_dp_err("Offset not supported");
		plt_dp_err("enc_offset: %d", encr_offset);
		return -1;
	}

	i = fill_sg_comp(gather_comp, i, (uint64_t)offset_vaddr,
			 ROC_SE_OFF_CTRL_LEN + iv_len);

	/* IV */
	memcpy((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN, params->iv_buf,
	       iv_len);

	/* Add input data */
	size = inputlen - iv_len;
	if (size) {
		i = fill_sg_comp_from_iov(gather_comp, i, params->src_iov, 0,
					  &size, NULL, 0);
		if (unlikely(size)) {
			plt_dp_err("Insufficient buffer space,"
				   " size %d needed",
				   size);
			return -1;
		}
	}
	((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);
	g_size_bytes = ((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

	/*
	 * Output Scatter List
	 */

	i = 0;
	scatter_comp = (struct roc_se_sglist_comp *)((uint8_t *)gather_comp +
						     g_size_bytes);

	/* IV */
	i = fill_sg_comp(scatter_comp, i,
			 (uint64_t)offset_vaddr + ROC_SE_OFF_CTRL_LEN, iv_len);

	/* Add output data */
	size = outputlen - iv_len;
	if (size) {
		i = fill_sg_comp_from_iov(scatter_comp, i, params->dst_iov, 0,
					  &size, NULL, 0);
		if (unlikely(size)) {
			plt_dp_err("Insufficient buffer space,"
				   " size %d needed",
				   size);
			return -1;
		}
	}
	((uint16_t *)in_buffer)[3] = rte_cpu_to_be_16(i);
	s_size_bytes = ((i + 3) / 4) * sizeof(struct roc_se_sglist_comp);

	size = g_size_bytes + s_size_bytes + ROC_SE_SG_LIST_HDR_SIZE;

	/* This is DPTR len in case of SG mode */
	cpt_inst_w4.s.dlen = size;

	inst->dptr = (uint64_t)in_buffer;
	inst->w4.u64 = cpt_inst_w4.u64;

	return 0;
}

static __rte_always_inline int
cpt_fc_dec_hmac_prep(uint32_t flags, uint64_t d_offs, uint64_t d_lens,
		     struct roc_se_fc_params *fc_params,
		     struct cpt_inst_s *inst)
{
	struct roc_se_ctx *ctx = fc_params->ctx_buf.vaddr;
	uint8_t fc_type;
	int ret = -1;

	fc_type = ctx->fc_type;

	if (likely(fc_type == ROC_SE_FC_GEN)) {
		ret = cpt_dec_hmac_prep(flags, d_offs, d_lens, fc_params, inst);
	} else if (fc_type == ROC_SE_PDCP) {
		ret = cpt_zuc_snow3g_prep(flags, d_offs, d_lens, fc_params,
					  inst);
	} else if (fc_type == ROC_SE_KASUMI) {
		ret = cpt_kasumi_dec_prep(d_offs, d_lens, fc_params, inst);
	}

	/*
	 * For AUTH_ONLY case,
	 * MC only supports digest generation and verification
	 * should be done in software by memcmp()
	 */

	return ret;
}

static __rte_always_inline int
cpt_fc_enc_hmac_prep(uint32_t flags, uint64_t d_offs, uint64_t d_lens,
		     struct roc_se_fc_params *fc_params,
		     struct cpt_inst_s *inst)
{
	struct roc_se_ctx *ctx = fc_params->ctx_buf.vaddr;
	uint8_t fc_type;
	int ret = -1;

	fc_type = ctx->fc_type;

	if (likely(fc_type == ROC_SE_FC_GEN)) {
		ret = cpt_enc_hmac_prep(flags, d_offs, d_lens, fc_params, inst);
	} else if (fc_type == ROC_SE_PDCP) {
		ret = cpt_zuc_snow3g_prep(flags, d_offs, d_lens, fc_params,
					  inst);
	} else if (fc_type == ROC_SE_KASUMI) {
		ret = cpt_kasumi_enc_prep(flags, d_offs, d_lens, fc_params,
					  inst);
	} else if (fc_type == ROC_SE_HASH_HMAC) {
		ret = cpt_digest_gen_prep(flags, d_lens, fc_params, inst);
	}

	return ret;
}

static __rte_always_inline int
fill_sess_aead(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	struct rte_crypto_aead_xform *aead_form;
	roc_se_cipher_type enc_type = 0; /* NULL Cipher type */
	roc_se_auth_type auth_type = 0;	 /* NULL Auth type */
	uint32_t cipher_key_len = 0;
	uint8_t aes_gcm = 0;
	aead_form = &xform->aead;

	if (aead_form->op == RTE_CRYPTO_AEAD_OP_ENCRYPT) {
		sess->cpt_op |= ROC_SE_OP_CIPHER_ENCRYPT;
		sess->cpt_op |= ROC_SE_OP_AUTH_GENERATE;
	} else if (aead_form->op == RTE_CRYPTO_AEAD_OP_DECRYPT) {
		sess->cpt_op |= ROC_SE_OP_CIPHER_DECRYPT;
		sess->cpt_op |= ROC_SE_OP_AUTH_VERIFY;
	} else {
		plt_dp_err("Unknown aead operation\n");
		return -1;
	}
	switch (aead_form->algo) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		enc_type = ROC_SE_AES_GCM;
		cipher_key_len = 16;
		aes_gcm = 1;
		break;
	case RTE_CRYPTO_AEAD_AES_CCM:
		plt_dp_err("Crypto: Unsupported cipher algo %u",
			   aead_form->algo);
		return -1;
	case RTE_CRYPTO_AEAD_CHACHA20_POLY1305:
		enc_type = ROC_SE_CHACHA20;
		auth_type = ROC_SE_POLY1305;
		cipher_key_len = 32;
		sess->chacha_poly = 1;
		break;
	default:
		plt_dp_err("Crypto: Undefined cipher algo %u specified",
			   aead_form->algo);
		return -1;
	}
	if (aead_form->key.length < cipher_key_len) {
		plt_dp_err("Invalid cipher params keylen %u",
			   aead_form->key.length);
		return -1;
	}
	sess->zsk_flag = 0;
	sess->aes_gcm = aes_gcm;
	sess->mac_len = aead_form->digest_length;
	sess->iv_offset = aead_form->iv.offset;
	sess->iv_length = aead_form->iv.length;
	sess->aad_length = aead_form->aad_length;

	if (unlikely(roc_se_ciph_key_set(&sess->roc_se_ctx, enc_type,
					 aead_form->key.data,
					 aead_form->key.length, NULL)))
		return -1;

	if (unlikely(roc_se_auth_key_set(&sess->roc_se_ctx, auth_type, NULL, 0,
					 aead_form->digest_length)))
		return -1;

	return 0;
}

static __rte_always_inline int
fill_sess_cipher(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	struct rte_crypto_cipher_xform *c_form;
	roc_se_cipher_type enc_type = 0; /* NULL Cipher type */
	uint32_t cipher_key_len = 0;
	uint8_t zsk_flag = 0, aes_ctr = 0, is_null = 0;

	c_form = &xform->cipher;

	if (c_form->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		sess->cpt_op |= ROC_SE_OP_CIPHER_ENCRYPT;
	else if (c_form->op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
		sess->cpt_op |= ROC_SE_OP_CIPHER_DECRYPT;
		if (xform->next != NULL &&
		    xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
			/* Perform decryption followed by auth verify */
			sess->roc_se_ctx.template_w4.s.opcode_minor =
				ROC_SE_FC_MINOR_OP_HMAC_FIRST;
		}
	} else {
		plt_dp_err("Unknown cipher operation\n");
		return -1;
	}

	switch (c_form->algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		enc_type = ROC_SE_AES_CBC;
		cipher_key_len = 16;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		enc_type = ROC_SE_DES3_CBC;
		cipher_key_len = 24;
		break;
	case RTE_CRYPTO_CIPHER_DES_CBC:
		/* DES is implemented using 3DES in hardware */
		enc_type = ROC_SE_DES3_CBC;
		cipher_key_len = 8;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		enc_type = ROC_SE_AES_CTR;
		cipher_key_len = 16;
		aes_ctr = 1;
		break;
	case RTE_CRYPTO_CIPHER_NULL:
		enc_type = 0;
		is_null = 1;
		break;
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
		enc_type = ROC_SE_KASUMI_F8_ECB;
		cipher_key_len = 16;
		zsk_flag = ROC_SE_K_F8;
		break;
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
		enc_type = ROC_SE_SNOW3G_UEA2;
		cipher_key_len = 16;
		zsk_flag = ROC_SE_ZS_EA;
		break;
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
		enc_type = ROC_SE_ZUC_EEA3;
		cipher_key_len = 16;
		zsk_flag = ROC_SE_ZS_EA;
		break;
	case RTE_CRYPTO_CIPHER_AES_XTS:
		enc_type = ROC_SE_AES_XTS;
		cipher_key_len = 16;
		break;
	case RTE_CRYPTO_CIPHER_3DES_ECB:
		enc_type = ROC_SE_DES3_ECB;
		cipher_key_len = 24;
		break;
	case RTE_CRYPTO_CIPHER_AES_ECB:
		enc_type = ROC_SE_AES_ECB;
		cipher_key_len = 16;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CTR:
	case RTE_CRYPTO_CIPHER_AES_F8:
	case RTE_CRYPTO_CIPHER_ARC4:
		plt_dp_err("Crypto: Unsupported cipher algo %u", c_form->algo);
		return -1;
	default:
		plt_dp_err("Crypto: Undefined cipher algo %u specified",
			   c_form->algo);
		return -1;
	}

	if (c_form->key.length < cipher_key_len) {
		plt_dp_err("Invalid cipher params keylen %u",
			   c_form->key.length);
		return -1;
	}

	sess->zsk_flag = zsk_flag;
	sess->aes_gcm = 0;
	sess->aes_ctr = aes_ctr;
	sess->iv_offset = c_form->iv.offset;
	sess->iv_length = c_form->iv.length;
	sess->is_null = is_null;

	if (unlikely(roc_se_ciph_key_set(&sess->roc_se_ctx, enc_type,
					 c_form->key.data, c_form->key.length,
					 NULL)))
		return -1;

	if ((enc_type >= ROC_SE_ZUC_EEA3) && (enc_type <= ROC_SE_AES_CTR_EEA2))
		roc_se_ctx_swap(&sess->roc_se_ctx);
	return 0;
}

static __rte_always_inline int
fill_sess_auth(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	struct rte_crypto_auth_xform *a_form;
	roc_se_auth_type auth_type = 0; /* NULL Auth type */
	uint8_t zsk_flag = 0, aes_gcm = 0, is_null = 0;

	if (xform->next != NULL &&
	    xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
	    xform->next->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		/* Perform auth followed by encryption */
		sess->roc_se_ctx.template_w4.s.opcode_minor =
			ROC_SE_FC_MINOR_OP_HMAC_FIRST;
	}

	a_form = &xform->auth;

	if (a_form->op == RTE_CRYPTO_AUTH_OP_VERIFY)
		sess->cpt_op |= ROC_SE_OP_AUTH_VERIFY;
	else if (a_form->op == RTE_CRYPTO_AUTH_OP_GENERATE)
		sess->cpt_op |= ROC_SE_OP_AUTH_GENERATE;
	else {
		plt_dp_err("Unknown auth operation");
		return -1;
	}

	switch (a_form->algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		/* Fall through */
	case RTE_CRYPTO_AUTH_SHA1:
		auth_type = ROC_SE_SHA1_TYPE;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
	case RTE_CRYPTO_AUTH_SHA256:
		auth_type = ROC_SE_SHA2_SHA256;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
	case RTE_CRYPTO_AUTH_SHA512:
		auth_type = ROC_SE_SHA2_SHA512;
		break;
	case RTE_CRYPTO_AUTH_AES_GMAC:
		auth_type = ROC_SE_GMAC_TYPE;
		aes_gcm = 1;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
	case RTE_CRYPTO_AUTH_SHA224:
		auth_type = ROC_SE_SHA2_SHA224;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
	case RTE_CRYPTO_AUTH_SHA384:
		auth_type = ROC_SE_SHA2_SHA384;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
	case RTE_CRYPTO_AUTH_MD5:
		auth_type = ROC_SE_MD5_TYPE;
		break;
	case RTE_CRYPTO_AUTH_KASUMI_F9:
		auth_type = ROC_SE_KASUMI_F9_ECB;
		/*
		 * Indicate that direction needs to be taken out
		 * from end of src
		 */
		zsk_flag = ROC_SE_K_F9;
		break;
	case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
		auth_type = ROC_SE_SNOW3G_UIA2;
		zsk_flag = ROC_SE_ZS_IA;
		break;
	case RTE_CRYPTO_AUTH_ZUC_EIA3:
		auth_type = ROC_SE_ZUC_EIA3;
		zsk_flag = ROC_SE_ZS_IA;
		break;
	case RTE_CRYPTO_AUTH_NULL:
		auth_type = 0;
		is_null = 1;
		break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
	case RTE_CRYPTO_AUTH_AES_CMAC:
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
		plt_dp_err("Crypto: Unsupported hash algo %u", a_form->algo);
		return -1;
	default:
		plt_dp_err("Crypto: Undefined Hash algo %u specified",
			   a_form->algo);
		return -1;
	}

	sess->zsk_flag = zsk_flag;
	sess->aes_gcm = aes_gcm;
	sess->mac_len = a_form->digest_length;
	sess->is_null = is_null;
	if (zsk_flag) {
		sess->auth_iv_offset = a_form->iv.offset;
		sess->auth_iv_length = a_form->iv.length;
	}
	if (unlikely(roc_se_auth_key_set(&sess->roc_se_ctx, auth_type,
					 a_form->key.data, a_form->key.length,
					 a_form->digest_length)))
		return -1;

	if ((auth_type >= ROC_SE_ZUC_EIA3) &&
	    (auth_type <= ROC_SE_AES_CMAC_EIA2))
		roc_se_ctx_swap(&sess->roc_se_ctx);

	return 0;
}

static __rte_always_inline int
fill_sess_gmac(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	struct rte_crypto_auth_xform *a_form;
	roc_se_cipher_type enc_type = 0; /* NULL Cipher type */
	roc_se_auth_type auth_type = 0;	 /* NULL Auth type */

	a_form = &xform->auth;

	if (a_form->op == RTE_CRYPTO_AUTH_OP_GENERATE)
		sess->cpt_op |= ROC_SE_OP_ENCODE;
	else if (a_form->op == RTE_CRYPTO_AUTH_OP_VERIFY)
		sess->cpt_op |= ROC_SE_OP_DECODE;
	else {
		plt_dp_err("Unknown auth operation");
		return -1;
	}

	switch (a_form->algo) {
	case RTE_CRYPTO_AUTH_AES_GMAC:
		enc_type = ROC_SE_AES_GCM;
		auth_type = ROC_SE_GMAC_TYPE;
		break;
	default:
		plt_dp_err("Crypto: Undefined cipher algo %u specified",
			   a_form->algo);
		return -1;
	}

	sess->zsk_flag = 0;
	sess->aes_gcm = 0;
	sess->is_gmac = 1;
	sess->iv_offset = a_form->iv.offset;
	sess->iv_length = a_form->iv.length;
	sess->mac_len = a_form->digest_length;

	if (unlikely(roc_se_ciph_key_set(&sess->roc_se_ctx, enc_type,
					 a_form->key.data, a_form->key.length,
					 NULL)))
		return -1;

	if (unlikely(roc_se_auth_key_set(&sess->roc_se_ctx, auth_type, NULL, 0,
					 a_form->digest_length)))
		return -1;

	return 0;
}

static __rte_always_inline void *
alloc_op_meta(struct roc_se_buf_ptr *buf, int32_t len,
	      struct rte_mempool *cpt_meta_pool,
	      struct cpt_inflight_req *infl_req)
{
	uint8_t *mdata;

	if (unlikely(rte_mempool_get(cpt_meta_pool, (void **)&mdata) < 0))
		return NULL;

	buf->vaddr = mdata;
	buf->size = len;

	infl_req->mdata = mdata;
	infl_req->op_flags |= CPT_OP_FLAGS_METABUF;

	return mdata;
}

static __rte_always_inline uint32_t
prepare_iov_from_pkt(struct rte_mbuf *pkt, struct roc_se_iov_ptr *iovec,
		     uint32_t start_offset)
{
	uint16_t index = 0;
	void *seg_data = NULL;
	int32_t seg_size = 0;

	if (!pkt) {
		iovec->buf_cnt = 0;
		return 0;
	}

	if (!start_offset) {
		seg_data = rte_pktmbuf_mtod(pkt, void *);
		seg_size = pkt->data_len;
	} else {
		while (start_offset >= pkt->data_len) {
			start_offset -= pkt->data_len;
			pkt = pkt->next;
		}

		seg_data = rte_pktmbuf_mtod_offset(pkt, void *, start_offset);
		seg_size = pkt->data_len - start_offset;
		if (!seg_size)
			return 1;
	}

	/* first seg */
	iovec->bufs[index].vaddr = seg_data;
	iovec->bufs[index].size = seg_size;
	index++;
	pkt = pkt->next;

	while (unlikely(pkt != NULL)) {
		seg_data = rte_pktmbuf_mtod(pkt, void *);
		seg_size = pkt->data_len;
		if (!seg_size)
			break;

		iovec->bufs[index].vaddr = seg_data;
		iovec->bufs[index].size = seg_size;

		index++;

		pkt = pkt->next;
	}

	iovec->buf_cnt = index;
	return 0;
}

static __rte_always_inline uint32_t
prepare_iov_from_pkt_inplace(struct rte_mbuf *pkt,
			     struct roc_se_fc_params *param, uint32_t *flags)
{
	uint16_t index = 0;
	void *seg_data = NULL;
	uint32_t seg_size = 0;
	struct roc_se_iov_ptr *iovec;

	seg_data = rte_pktmbuf_mtod(pkt, void *);
	seg_size = pkt->data_len;

	/* first seg */
	if (likely(!pkt->next)) {
		uint32_t headroom;

		*flags |= ROC_SE_SINGLE_BUF_INPLACE;
		headroom = rte_pktmbuf_headroom(pkt);
		if (likely(headroom >= 24))
			*flags |= ROC_SE_SINGLE_BUF_HEADROOM;

		param->bufs[0].vaddr = seg_data;
		param->bufs[0].size = seg_size;
		return 0;
	}
	iovec = param->src_iov;
	iovec->bufs[index].vaddr = seg_data;
	iovec->bufs[index].size = seg_size;
	index++;
	pkt = pkt->next;

	while (unlikely(pkt != NULL)) {
		seg_data = rte_pktmbuf_mtod(pkt, void *);
		seg_size = pkt->data_len;

		if (!seg_size)
			break;

		iovec->bufs[index].vaddr = seg_data;
		iovec->bufs[index].size = seg_size;

		index++;

		pkt = pkt->next;
	}

	iovec->buf_cnt = index;
	return 0;
}

static __rte_always_inline int
fill_fc_params(struct rte_crypto_op *cop, struct cnxk_se_sess *sess,
	       struct cpt_qp_meta_info *m_info,
	       struct cpt_inflight_req *infl_req, struct cpt_inst_s *inst)
{
	struct roc_se_ctx *ctx = &sess->roc_se_ctx;
	uint8_t op_minor = ctx->template_w4.s.opcode_minor;
	struct rte_crypto_sym_op *sym_op = cop->sym;
	void *mdata = NULL;
	uint32_t mc_hash_off;
	uint32_t flags = 0;
	uint64_t d_offs, d_lens;
	struct rte_mbuf *m_src, *m_dst;
	uint8_t cpt_op = sess->cpt_op;
#ifdef CPT_ALWAYS_USE_SG_MODE
	uint8_t inplace = 0;
#else
	uint8_t inplace = 1;
#endif
	struct roc_se_fc_params fc_params;
	char src[SRC_IOV_SIZE];
	char dst[SRC_IOV_SIZE];
	uint32_t iv_buf[4];
	int ret;

	if (likely(sess->iv_length)) {
		flags |= ROC_SE_VALID_IV_BUF;
		fc_params.iv_buf = rte_crypto_op_ctod_offset(cop, uint8_t *,
							     sess->iv_offset);
		if (sess->aes_ctr && unlikely(sess->iv_length != 16)) {
			memcpy((uint8_t *)iv_buf,
			       rte_crypto_op_ctod_offset(cop, uint8_t *,
							 sess->iv_offset),
			       12);
			iv_buf[3] = rte_cpu_to_be_32(0x1);
			fc_params.iv_buf = iv_buf;
		}
	}

	if (sess->zsk_flag) {
		fc_params.auth_iv_buf = rte_crypto_op_ctod_offset(
			cop, uint8_t *, sess->auth_iv_offset);
		if (sess->zsk_flag != ROC_SE_ZS_EA)
			inplace = 0;
	}
	m_src = sym_op->m_src;
	m_dst = sym_op->m_dst;

	if (sess->aes_gcm || sess->chacha_poly) {
		uint8_t *salt;
		uint8_t *aad_data;
		uint16_t aad_len;

		d_offs = sym_op->aead.data.offset;
		d_lens = sym_op->aead.data.length;
		mc_hash_off =
			sym_op->aead.data.offset + sym_op->aead.data.length;

		aad_data = sym_op->aead.aad.data;
		aad_len = sess->aad_length;
		if (likely((aad_data + aad_len) ==
			   rte_pktmbuf_mtod_offset(m_src, uint8_t *,
						   sym_op->aead.data.offset))) {
			d_offs = (d_offs - aad_len) | (d_offs << 16);
			d_lens = (d_lens + aad_len) | (d_lens << 32);
		} else {
			fc_params.aad_buf.vaddr = sym_op->aead.aad.data;
			fc_params.aad_buf.size = aad_len;
			flags |= ROC_SE_VALID_AAD_BUF;
			inplace = 0;
			d_offs = d_offs << 16;
			d_lens = d_lens << 32;
		}

		salt = fc_params.iv_buf;
		if (unlikely(*(uint32_t *)salt != sess->salt)) {
			cpt_fc_salt_update(&sess->roc_se_ctx, salt);
			sess->salt = *(uint32_t *)salt;
		}
		fc_params.iv_buf = salt + 4;
		if (likely(sess->mac_len)) {
			struct rte_mbuf *m =
				(cpt_op & ROC_SE_OP_ENCODE) ? m_dst : m_src;

			if (!m)
				m = m_src;

			/* hmac immediately following data is best case */
			if (unlikely(rte_pktmbuf_mtod(m, uint8_t *) +
					     mc_hash_off !=
				     (uint8_t *)sym_op->aead.digest.data)) {
				flags |= ROC_SE_VALID_MAC_BUF;
				fc_params.mac_buf.size = sess->mac_len;
				fc_params.mac_buf.vaddr =
					sym_op->aead.digest.data;
				inplace = 0;
			}
		}
	} else {
		d_offs = sym_op->cipher.data.offset;
		d_lens = sym_op->cipher.data.length;
		mc_hash_off =
			sym_op->cipher.data.offset + sym_op->cipher.data.length;
		d_offs = (d_offs << 16) | sym_op->auth.data.offset;
		d_lens = (d_lens << 32) | sym_op->auth.data.length;

		if (mc_hash_off <
		    (sym_op->auth.data.offset + sym_op->auth.data.length)) {
			mc_hash_off = (sym_op->auth.data.offset +
				       sym_op->auth.data.length);
		}
		/* for gmac, salt should be updated like in gcm */
		if (unlikely(sess->is_gmac)) {
			uint8_t *salt;
			salt = fc_params.iv_buf;
			if (unlikely(*(uint32_t *)salt != sess->salt)) {
				cpt_fc_salt_update(&sess->roc_se_ctx, salt);
				sess->salt = *(uint32_t *)salt;
			}
			fc_params.iv_buf = salt + 4;
		}
		if (likely(sess->mac_len)) {
			struct rte_mbuf *m;

			m = (cpt_op & ROC_SE_OP_ENCODE) ? m_dst : m_src;
			if (!m)
				m = m_src;

			/* hmac immediately following data is best case */
			if (!(op_minor & ROC_SE_FC_MINOR_OP_HMAC_FIRST) &&
			    (unlikely(rte_pktmbuf_mtod(m, uint8_t *) +
					      mc_hash_off !=
				      (uint8_t *)sym_op->auth.digest.data))) {
				flags |= ROC_SE_VALID_MAC_BUF;
				fc_params.mac_buf.size = sess->mac_len;
				fc_params.mac_buf.vaddr =
					sym_op->auth.digest.data;
				inplace = 0;
			}
		}
	}
	fc_params.ctx_buf.vaddr = &sess->roc_se_ctx;

	if (!(op_minor & ROC_SE_FC_MINOR_OP_HMAC_FIRST) &&
	    unlikely(sess->is_null || sess->cpt_op == ROC_SE_OP_DECODE))
		inplace = 0;

	if (likely(!m_dst && inplace)) {
		/* Case of single buffer without AAD buf or
		 * separate mac buf in place and
		 * not air crypto
		 */
		fc_params.dst_iov = fc_params.src_iov = (void *)src;

		if (unlikely(prepare_iov_from_pkt_inplace(m_src, &fc_params,
							  &flags))) {
			plt_dp_err("Prepare inplace src iov failed");
			ret = -EINVAL;
			goto err_exit;
		}

	} else {
		/* Out of place processing */
		fc_params.src_iov = (void *)src;
		fc_params.dst_iov = (void *)dst;

		/* Store SG I/O in the api for reuse */
		if (prepare_iov_from_pkt(m_src, fc_params.src_iov, 0)) {
			plt_dp_err("Prepare src iov failed");
			ret = -EINVAL;
			goto err_exit;
		}

		if (unlikely(m_dst != NULL)) {
			uint32_t pkt_len;

			/* Try to make room as much as src has */
			pkt_len = rte_pktmbuf_pkt_len(m_dst);

			if (unlikely(pkt_len < rte_pktmbuf_pkt_len(m_src))) {
				pkt_len = rte_pktmbuf_pkt_len(m_src) - pkt_len;
				if (!rte_pktmbuf_append(m_dst, pkt_len)) {
					plt_dp_err("Not enough space in "
						   "m_dst %p, need %u"
						   " more",
						   m_dst, pkt_len);
					ret = -EINVAL;
					goto err_exit;
				}
			}

			if (prepare_iov_from_pkt(m_dst, fc_params.dst_iov, 0)) {
				plt_dp_err("Prepare dst iov failed for "
					   "m_dst %p",
					   m_dst);
				ret = -EINVAL;
				goto err_exit;
			}
		} else {
			fc_params.dst_iov = (void *)src;
		}
	}

	if (unlikely(!((flags & ROC_SE_SINGLE_BUF_INPLACE) &&
		       (flags & ROC_SE_SINGLE_BUF_HEADROOM) &&
		       ((ctx->fc_type == ROC_SE_FC_GEN) ||
			(ctx->fc_type == ROC_SE_PDCP))))) {
		mdata = alloc_op_meta(&fc_params.meta_buf, m_info->mlen,
				      m_info->pool, infl_req);
		if (mdata == NULL) {
			plt_dp_err("Error allocating meta buffer for request");
			return -ENOMEM;
		}
	}

	/* Finally prepare the instruction */
	if (cpt_op & ROC_SE_OP_ENCODE)
		ret = cpt_fc_enc_hmac_prep(flags, d_offs, d_lens, &fc_params,
					   inst);
	else
		ret = cpt_fc_dec_hmac_prep(flags, d_offs, d_lens, &fc_params,
					   inst);

	if (unlikely(ret)) {
		plt_dp_err("Preparing request failed due to bad input arg");
		goto free_mdata_and_exit;
	}

	return 0;

free_mdata_and_exit:
	if (infl_req->op_flags & CPT_OP_FLAGS_METABUF)
		rte_mempool_put(m_info->pool, infl_req->mdata);
err_exit:
	return ret;
}

static __rte_always_inline void
compl_auth_verify(struct rte_crypto_op *op, uint8_t *gen_mac, uint64_t mac_len)
{
	uint8_t *mac;
	struct rte_crypto_sym_op *sym_op = op->sym;

	if (sym_op->auth.digest.data)
		mac = sym_op->auth.digest.data;
	else
		mac = rte_pktmbuf_mtod_offset(sym_op->m_src, uint8_t *,
					      sym_op->auth.data.length +
						      sym_op->auth.data.offset);
	if (!mac) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return;
	}

	if (memcmp(mac, gen_mac, mac_len))
		op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
	else
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static __rte_always_inline void
find_kasumif9_direction_and_length(uint8_t *src, uint32_t counter_num_bytes,
				   uint32_t *addr_length_in_bits,
				   uint8_t *addr_direction)
{
	uint8_t found = 0;
	uint32_t pos;
	uint8_t last_byte;
	while (!found && counter_num_bytes > 0) {
		counter_num_bytes--;
		if (src[counter_num_bytes] == 0x00)
			continue;
		pos = rte_bsf32(src[counter_num_bytes]);
		if (pos == 7) {
			if (likely(counter_num_bytes > 0)) {
				last_byte = src[counter_num_bytes - 1];
				*addr_direction = last_byte & 0x1;
				*addr_length_in_bits =
					counter_num_bytes * 8 - 1;
			}
		} else {
			last_byte = src[counter_num_bytes];
			*addr_direction = (last_byte >> (pos + 1)) & 0x1;
			*addr_length_in_bits =
				counter_num_bytes * 8 + (8 - (pos + 2));
		}
		found = 1;
	}
}

/*
 * This handles all auth only except AES_GMAC
 */
static __rte_always_inline int
fill_digest_params(struct rte_crypto_op *cop, struct cnxk_se_sess *sess,
		   struct cpt_qp_meta_info *m_info,
		   struct cpt_inflight_req *infl_req, struct cpt_inst_s *inst)
{
	uint32_t space = 0;
	struct rte_crypto_sym_op *sym_op = cop->sym;
	void *mdata;
	uint32_t auth_range_off;
	uint32_t flags = 0;
	uint64_t d_offs = 0, d_lens;
	struct rte_mbuf *m_src, *m_dst;
	uint16_t auth_op = sess->cpt_op & ROC_SE_OP_AUTH_MASK;
	uint16_t mac_len = sess->mac_len;
	struct roc_se_fc_params params;
	char src[SRC_IOV_SIZE];
	uint8_t iv_buf[16];
	int ret;

	memset(&params, 0, sizeof(struct roc_se_fc_params));

	m_src = sym_op->m_src;

	mdata = alloc_op_meta(&params.meta_buf, m_info->mlen, m_info->pool,
			      infl_req);
	if (mdata == NULL) {
		ret = -ENOMEM;
		goto err_exit;
	}

	auth_range_off = sym_op->auth.data.offset;

	flags = ROC_SE_VALID_MAC_BUF;
	params.src_iov = (void *)src;
	if (unlikely(sess->zsk_flag)) {
		/*
		 * Since for Zuc, Kasumi, Snow3g offsets are in bits
		 * we will send pass through even for auth only case,
		 * let MC handle it
		 */
		d_offs = auth_range_off;
		auth_range_off = 0;
		params.auth_iv_buf = rte_crypto_op_ctod_offset(
			cop, uint8_t *, sess->auth_iv_offset);
		if (sess->zsk_flag == ROC_SE_K_F9) {
			uint32_t length_in_bits, num_bytes;
			uint8_t *src, direction = 0;

			memcpy(iv_buf,
			       rte_pktmbuf_mtod(cop->sym->m_src, uint8_t *), 8);
			/*
			 * This is kasumi f9, take direction from
			 * source buffer
			 */
			length_in_bits = cop->sym->auth.data.length;
			num_bytes = (length_in_bits >> 3);
			src = rte_pktmbuf_mtod(cop->sym->m_src, uint8_t *);
			find_kasumif9_direction_and_length(
				src, num_bytes, &length_in_bits, &direction);
			length_in_bits -= 64;
			cop->sym->auth.data.offset += 64;
			d_offs = cop->sym->auth.data.offset;
			auth_range_off = d_offs / 8;
			cop->sym->auth.data.length = length_in_bits;

			/* Store it at end of auth iv */
			iv_buf[8] = direction;
			params.auth_iv_buf = iv_buf;
		}
	}

	d_lens = sym_op->auth.data.length;

	params.ctx_buf.vaddr = &sess->roc_se_ctx;

	if (auth_op == ROC_SE_OP_AUTH_GENERATE) {
		if (sym_op->auth.digest.data) {
			/*
			 * Digest to be generated
			 * in separate buffer
			 */
			params.mac_buf.size = sess->mac_len;
			params.mac_buf.vaddr = sym_op->auth.digest.data;
		} else {
			uint32_t off = sym_op->auth.data.offset +
				       sym_op->auth.data.length;
			int32_t dlen, space;

			m_dst = sym_op->m_dst ? sym_op->m_dst : sym_op->m_src;
			dlen = rte_pktmbuf_pkt_len(m_dst);

			space = off + mac_len - dlen;
			if (space > 0)
				if (!rte_pktmbuf_append(m_dst, space)) {
					plt_dp_err("Failed to extend "
						   "mbuf by %uB",
						   space);
					ret = -EINVAL;
					goto free_mdata_and_exit;
				}

			params.mac_buf.vaddr =
				rte_pktmbuf_mtod_offset(m_dst, void *, off);
			params.mac_buf.size = mac_len;
		}
	} else {
		uint64_t *op = mdata;

		/* Need space for storing generated mac */
		space += 2 * sizeof(uint64_t);

		params.mac_buf.vaddr = (uint8_t *)mdata + space;
		params.mac_buf.size = mac_len;
		space += RTE_ALIGN_CEIL(mac_len, 8);
		op[0] = (uintptr_t)params.mac_buf.vaddr;
		op[1] = mac_len;
		infl_req->op_flags |= CPT_OP_FLAGS_AUTH_VERIFY;
	}

	params.meta_buf.vaddr = (uint8_t *)mdata + space;
	params.meta_buf.size -= space;

	/* Out of place processing */
	params.src_iov = (void *)src;

	/*Store SG I/O in the api for reuse */
	if (prepare_iov_from_pkt(m_src, params.src_iov, auth_range_off)) {
		plt_dp_err("Prepare src iov failed");
		ret = -EINVAL;
		goto free_mdata_and_exit;
	}

	ret = cpt_fc_enc_hmac_prep(flags, d_offs, d_lens, &params, inst);
	if (ret)
		goto free_mdata_and_exit;

	return 0;

free_mdata_and_exit:
	if (infl_req->op_flags & CPT_OP_FLAGS_METABUF)
		rte_mempool_put(m_info->pool, infl_req->mdata);
err_exit:
	return ret;
}
#endif /*_CNXK_SE_H_ */
