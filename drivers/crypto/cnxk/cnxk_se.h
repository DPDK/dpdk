/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_SE_H_
#define _CNXK_SE_H_
#include <stdbool.h>

#include <rte_cryptodev.h>

#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_sg.h"

#define SRC_IOV_SIZE                                                                               \
	(sizeof(struct roc_se_iov_ptr) + (sizeof(struct roc_se_buf_ptr) * ROC_MAX_SG_CNT))
#define DST_IOV_SIZE                                                                               \
	(sizeof(struct roc_se_iov_ptr) + (sizeof(struct roc_se_buf_ptr) * ROC_MAX_SG_CNT))

enum cpt_dp_thread_type {
	CPT_DP_THREAD_TYPE_FC_CHAIN = 0x1,
	CPT_DP_THREAD_TYPE_FC_AEAD,
	CPT_DP_THREAD_TYPE_PDCP,
	CPT_DP_THREAD_TYPE_PDCP_CHAIN,
	CPT_DP_THREAD_TYPE_KASUMI,
	CPT_DP_THREAD_TYPE_SM,
	CPT_DP_THREAD_AUTH_ONLY,
	CPT_DP_THREAD_GENERIC,
	CPT_DP_THREAD_TYPE_PT,
};

struct cnxk_se_sess {
	struct rte_cryptodev_sym_session rte_sess;
	uint16_t cpt_op : 4;
	uint16_t zsk_flag : 4;
	uint16_t aes_gcm : 1;
	uint16_t aes_ccm : 1;
	uint16_t aes_ctr : 1;
	uint16_t chacha_poly : 1;
	uint16_t is_null : 1;
	uint16_t is_gmac : 1;
	uint16_t chained_op : 1;
	uint16_t auth_first : 1;
	uint16_t aes_ctr_eea2 : 1;
	uint16_t zs_cipher : 4;
	uint16_t zs_auth : 4;
	uint16_t dp_thr_type : 8;
	uint16_t aad_length;
	uint8_t is_sha3 : 1;
	uint8_t short_iv : 1;
	uint8_t is_sm3 : 1;
	uint8_t passthrough : 1;
	uint8_t is_sm4 : 1;
	uint8_t rsvd : 3;
	uint8_t mac_len;
	uint8_t iv_length;
	uint8_t auth_iv_length;
	uint16_t iv_offset;
	uint16_t auth_iv_offset;
	uint32_t salt;
	uint64_t cpt_inst_w7;
	uint64_t cpt_inst_w2;
	struct cnxk_cpt_qp *qp;
	struct roc_se_ctx roc_se_ctx;
	struct roc_cpt_lf *lf;
} __rte_aligned(ROC_ALIGN);

static __rte_always_inline int fill_sess_gmac(struct rte_crypto_sym_xform *xform,
					      struct cnxk_se_sess *sess);

static inline void
cpt_pack_iv(uint8_t *iv_src, uint8_t *iv_dst)
{
	/* pack the first 8 bytes of IV to 6 bytes.
	 * discard the 2 MSB bits of each byte
	 */
	iv_dst[0] = (((iv_src[0] & 0x3f) << 2) | ((iv_src[1] >> 4) & 0x3));
	iv_dst[1] = (((iv_src[1] & 0xf) << 4) | ((iv_src[2] >> 2) & 0xf));
	iv_dst[2] = (((iv_src[2] & 0x3) << 6) | (iv_src[3] & 0x3f));

	iv_dst[3] = (((iv_src[4] & 0x3f) << 2) | ((iv_src[5] >> 4) & 0x3));
	iv_dst[4] = (((iv_src[5] & 0xf) << 4) | ((iv_src[6] >> 2) & 0xf));
	iv_dst[5] = (((iv_src[6] & 0x3) << 6) | (iv_src[7] & 0x3f));
}

static inline void
pdcp_iv_copy(uint8_t *iv_d, uint8_t *iv_s, const uint8_t pdcp_alg_type,
	     uint8_t pack_iv)
{
	uint32_t *iv_s_temp, iv_temp[4];
	int j;

	if (unlikely(iv_s == NULL)) {
		memset(iv_d, 0, 16);
		return;
	}

	if (pdcp_alg_type == ROC_SE_PDCP_ALG_TYPE_SNOW3G) {
		/*
		 * DPDK seems to provide it in form of IV3 IV2 IV1 IV0
		 * and BigEndian, MC needs it as IV0 IV1 IV2 IV3
		 */

		iv_s_temp = (uint32_t *)iv_s;

		for (j = 0; j < 4; j++)
			iv_temp[j] = iv_s_temp[3 - j];
		memcpy(iv_d, iv_temp, 16);
	} else if ((pdcp_alg_type == ROC_SE_PDCP_ALG_TYPE_ZUC) ||
		   pdcp_alg_type == ROC_SE_PDCP_ALG_TYPE_AES_CTR) {
		if (pack_iv) {
			cpt_pack_iv(iv_s, iv_d);
			memcpy(iv_d + 6, iv_s + 8, 17);
		} else
			memcpy(iv_d, iv_s, 16);
	}
}

/*
 * Digest immediately at the end of the data is the best case. Switch to SG if
 * that cannot be ensured.
 */
static inline void
cpt_digest_buf_lb_check(const struct cnxk_se_sess *sess, struct rte_mbuf *m,
			struct roc_se_fc_params *fc_params, uint32_t *flags,
			struct rte_crypto_sym_op *sym_op, bool *inplace, uint32_t a_data_off,
			uint32_t a_data_len, uint32_t c_data_off, uint32_t c_data_len,
			const bool is_pdcp_chain)
{
	const uint32_t auth_end = a_data_off + a_data_len;
	uint32_t mc_hash_off;

	/* PDCP_CHAIN only supports auth_first */

	if (is_pdcp_chain || sess->auth_first)
		mc_hash_off = auth_end;
	else
		mc_hash_off = RTE_MAX(c_data_off + c_data_len, auth_end);

	/* Digest immediately following data is best case */

	if (unlikely(rte_pktmbuf_mtod_offset(m, uint8_t *, mc_hash_off) !=
		     sym_op->auth.digest.data)) {
		*flags |= ROC_SE_VALID_MAC_BUF;
		fc_params->mac_buf.size = sess->mac_len;
		fc_params->mac_buf.vaddr = sym_op->auth.digest.data;
		*inplace = false;
	}
}

static inline struct rte_mbuf *
cpt_m_dst_get(uint8_t cpt_op, struct rte_mbuf *m_src, struct rte_mbuf *m_dst)
{
	if (m_dst != NULL && (cpt_op & ROC_SE_OP_ENCODE))
		return m_dst;
	else
		return m_src;
}

static __rte_always_inline int
cpt_mac_len_verify(struct rte_crypto_auth_xform *auth)
{
	uint16_t mac_len = auth->digest_length;
	int ret;

	if ((auth->algo != RTE_CRYPTO_AUTH_NULL) && (mac_len == 0))
		return -1;

	switch (auth->algo) {
	case RTE_CRYPTO_AUTH_MD5:
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		ret = (mac_len <= 16) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA1:
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		ret = (mac_len <= 20) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA224:
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
	case RTE_CRYPTO_AUTH_SHA3_224:
	case RTE_CRYPTO_AUTH_SHA3_224_HMAC:
		ret = (mac_len <= 28) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA256:
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
	case RTE_CRYPTO_AUTH_SHA3_256:
	case RTE_CRYPTO_AUTH_SHA3_256_HMAC:
		ret = (mac_len <= 32) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA384:
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
	case RTE_CRYPTO_AUTH_SHA3_384:
	case RTE_CRYPTO_AUTH_SHA3_384_HMAC:
		ret = (mac_len <= 48) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SHA512:
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
	case RTE_CRYPTO_AUTH_SHA3_512:
	case RTE_CRYPTO_AUTH_SHA3_512_HMAC:
		ret = (mac_len <= 64) ? 0 : -1;
		break;
	/* SHAKE itself doesn't have limitation of digest length,
	 * but in microcode size of length field is limited to 8 bits
	 */
	case RTE_CRYPTO_AUTH_SHAKE_128:
	case RTE_CRYPTO_AUTH_SHAKE_256:
		ret = (mac_len <= UINT8_MAX) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_SM3:
		ret = (mac_len <= 32) ? 0 : -1;
		break;
	case RTE_CRYPTO_AUTH_NULL:
		ret = 0;
		break;
	default:
		ret = -1;
	}

	return ret;
}

static __rte_always_inline int
sg_inst_prep(struct roc_se_fc_params *params, struct cpt_inst_s *inst, uint64_t offset_ctrl,
	     uint8_t *iv_s, int iv_len, uint8_t pack_iv, uint8_t pdcp_alg_type, int32_t inputlen,
	     int32_t outputlen, uint32_t passthrough_len, uint32_t req_flags, int pdcp_flag,
	     int decrypt)
{
	struct roc_sglist_comp *gather_comp, *scatter_comp;
	void *m_vaddr = params->meta_buf.vaddr;
	struct roc_se_buf_ptr *aad_buf = NULL;
	uint32_t mac_len = 0, aad_len = 0;
	struct roc_se_ctx *se_ctx;
	uint32_t i, g_size_bytes;
	int zsk_flags, ret = 0;
	uint64_t *offset_vaddr;
	uint32_t s_size_bytes;
	uint8_t *in_buffer;
	uint32_t size;
	uint8_t *iv_d;

	se_ctx = params->ctx;
	zsk_flags = se_ctx->zsk_flags;
	mac_len = se_ctx->mac_len;

	if (unlikely(req_flags & ROC_SE_VALID_AAD_BUF)) {
		/* We don't support both AAD and auth data separately */
		aad_len = params->aad_buf.size;
		aad_buf = &params->aad_buf;
	}

	/* save space for iv */
	offset_vaddr = m_vaddr;

	m_vaddr = (uint8_t *)m_vaddr + ROC_SE_OFF_CTRL_LEN + RTE_ALIGN_CEIL(iv_len, 8);

	inst->w4.s.opcode_major |= (uint64_t)ROC_DMA_MODE_SG;

	/* iv offset is 0 */
	*offset_vaddr = offset_ctrl;

	iv_d = ((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN);

	if (pdcp_flag) {
		if (likely(iv_len))
			pdcp_iv_copy(iv_d, iv_s, pdcp_alg_type, pack_iv);
	} else {
		if (likely(iv_len))
			memcpy(iv_d, iv_s, iv_len);
	}

	/* DPTR has SG list */

	/* TODO Add error check if space will be sufficient */
	gather_comp = (struct roc_sglist_comp *)((uint8_t *)m_vaddr + 8);

	/*
	 * Input Gather List
	 */
	i = 0;

	/* Offset control word followed by iv */

	i = fill_sg_comp(gather_comp, i, (uint64_t)offset_vaddr, ROC_SE_OFF_CTRL_LEN + iv_len);

	/* Add input data */
	if (decrypt && (req_flags & ROC_SE_VALID_MAC_BUF)) {
		size = inputlen - iv_len - mac_len;
		if (likely(size)) {
			uint32_t aad_offset = aad_len ? passthrough_len : 0;
			/* input data only */
			if (unlikely(req_flags & ROC_SE_SINGLE_BUF_INPLACE)) {
				i = fill_sg_comp_from_buf_min(gather_comp, i, params->bufs, &size);
			} else {
				i = fill_sg_comp_from_iov(gather_comp, i, params->src_iov, 0, &size,
							  aad_buf, aad_offset);
			}
			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer"
					   " space, size %d needed",
					   size);
				return -1;
			}
		}

		if (mac_len)
			i = fill_sg_comp_from_buf(gather_comp, i, &params->mac_buf);
	} else {
		/* input data */
		size = inputlen - iv_len;
		if (size) {
			uint32_t aad_offset = aad_len ? passthrough_len : 0;
			if (unlikely(req_flags & ROC_SE_SINGLE_BUF_INPLACE)) {
				i = fill_sg_comp_from_buf_min(gather_comp, i, params->bufs, &size);
			} else {
				i = fill_sg_comp_from_iov(gather_comp, i, params->src_iov, 0, &size,
							  aad_buf, aad_offset);
			}
			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer space,"
					   " size %d needed",
					   size);
				return -1;
			}
		}
	}

	in_buffer = m_vaddr;

	((uint16_t *)in_buffer)[0] = 0;
	((uint16_t *)in_buffer)[1] = 0;
	((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);

	g_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);
	/*
	 * Output Scatter List
	 */

	i = 0;
	scatter_comp = (struct roc_sglist_comp *)((uint8_t *)gather_comp + g_size_bytes);

	if (zsk_flags == 0x1) {
		/* IV in SLIST only for EEA3 & UEA2 or for F8 */
		iv_len = 0;
	}

	if (iv_len) {
		i = fill_sg_comp(scatter_comp, i, (uint64_t)offset_vaddr + ROC_SE_OFF_CTRL_LEN,
				 iv_len);
	}

	/* Add output data */
	if ((!decrypt) && (req_flags & ROC_SE_VALID_MAC_BUF)) {
		size = outputlen - iv_len - mac_len;
		if (size) {

			uint32_t aad_offset = aad_len ? passthrough_len : 0;

			if (unlikely(req_flags & ROC_SE_SINGLE_BUF_INPLACE)) {
				i = fill_sg_comp_from_buf_min(scatter_comp, i, params->bufs, &size);
			} else {
				i = fill_sg_comp_from_iov(scatter_comp, i, params->dst_iov, 0,
							  &size, aad_buf, aad_offset);
			}
			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer space,"
					   " size %d needed",
					   size);
				return -1;
			}
		}

		/* mac data */
		if (mac_len)
			i = fill_sg_comp_from_buf(scatter_comp, i, &params->mac_buf);
	} else {
		/* Output including mac */
		size = outputlen - iv_len;
		if (size) {
			uint32_t aad_offset = aad_len ? passthrough_len : 0;

			if (unlikely(req_flags & ROC_SE_SINGLE_BUF_INPLACE)) {
				i = fill_sg_comp_from_buf_min(scatter_comp, i, params->bufs, &size);
			} else {
				i = fill_sg_comp_from_iov(scatter_comp, i, params->dst_iov, 0,
							  &size, aad_buf, aad_offset);
			}

			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer space,"
					   " size %d needed",
					   size);
				return -1;
			}
		}
	}
	((uint16_t *)in_buffer)[3] = rte_cpu_to_be_16(i);
	s_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

	size = g_size_bytes + s_size_bytes + ROC_SG_LIST_HDR_SIZE;

	/* This is DPTR len in case of SG mode */
	inst->w4.s.dlen = size;

	if (unlikely(size > ROC_SG_MAX_DLEN_SIZE)) {
		plt_dp_err("Exceeds max supported components. Reduce segments");
		ret = -1;
	}

	inst->dptr = (uint64_t)in_buffer;
	return ret;
}

static __rte_always_inline int
sg2_inst_prep(struct roc_se_fc_params *params, struct cpt_inst_s *inst, uint64_t offset_ctrl,
	      uint8_t *iv_s, int iv_len, uint8_t pack_iv, uint8_t pdcp_alg_type, int32_t inputlen,
	      int32_t outputlen, uint32_t passthrough_len, uint32_t req_flags, int pdcp_flag,
	      int decrypt)
{
	struct roc_sg2list_comp *gather_comp, *scatter_comp;
	void *m_vaddr = params->meta_buf.vaddr;
	struct roc_se_buf_ptr *aad_buf = NULL;
	uint32_t mac_len = 0, aad_len = 0;
	uint16_t scatter_sz, gather_sz;
	union cpt_inst_w5 cpt_inst_w5;
	union cpt_inst_w6 cpt_inst_w6;
	struct roc_se_ctx *se_ctx;
	uint32_t i, g_size_bytes;
	uint64_t *offset_vaddr;
	int zsk_flags, ret = 0;
	uint32_t size;
	uint8_t *iv_d;

	se_ctx = params->ctx;
	zsk_flags = se_ctx->zsk_flags;
	mac_len = se_ctx->mac_len;

	if (unlikely(req_flags & ROC_SE_VALID_AAD_BUF)) {
		/* We don't support both AAD and auth data separately */
		aad_len = params->aad_buf.size;
		aad_buf = &params->aad_buf;
	}

	/* save space for iv */
	offset_vaddr = m_vaddr;

	m_vaddr = (uint8_t *)m_vaddr + ROC_SE_OFF_CTRL_LEN + RTE_ALIGN_CEIL(iv_len, 8);

	inst->w4.s.opcode_major |= (uint64_t)ROC_DMA_MODE_SG;

	/* This is DPTR len in case of SG mode */
	inst->w4.s.dlen = inputlen + ROC_SE_OFF_CTRL_LEN;

	/* iv offset is 0 */
	*offset_vaddr = offset_ctrl;

	iv_d = ((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN);
	if (pdcp_flag) {
		if (likely(iv_len))
			pdcp_iv_copy(iv_d, iv_s, pdcp_alg_type, pack_iv);
	} else {
		if (likely(iv_len))
			memcpy(iv_d, iv_s, iv_len);
	}

	/* DPTR has SG list */

	/* TODO Add error check if space will be sufficient */
	gather_comp = (struct roc_sg2list_comp *)((uint8_t *)m_vaddr);

	/*
	 * Input Gather List
	 */
	i = 0;

	/* Offset control word followed by iv */

	i = fill_sg2_comp(gather_comp, i, (uint64_t)offset_vaddr, ROC_SE_OFF_CTRL_LEN + iv_len);

	/* Add input data */
	if (decrypt && (req_flags & ROC_SE_VALID_MAC_BUF)) {
		size = inputlen - iv_len - mac_len;
		if (size) {
			/* input data only */
			if (unlikely(req_flags & ROC_SE_SINGLE_BUF_INPLACE)) {
				i = fill_sg2_comp_from_buf_min(gather_comp, i, params->bufs, &size);
			} else {
				uint32_t aad_offset = aad_len ? passthrough_len : 0;

				i = fill_sg2_comp_from_iov(gather_comp, i, params->src_iov, 0,
							   &size, aad_buf, aad_offset);
			}
			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer"
					   " space, size %d needed",
					   size);
				return -1;
			}
		}

		/* mac data */
		if (mac_len)
			i = fill_sg2_comp_from_buf(gather_comp, i, &params->mac_buf);
	} else {
		/* input data */
		size = inputlen - iv_len;
		if (size) {
			uint32_t aad_offset = aad_len ? passthrough_len : 0;
			if (unlikely(req_flags & ROC_SE_SINGLE_BUF_INPLACE)) {
				i = fill_sg2_comp_from_buf_min(gather_comp, i, params->bufs, &size);
			} else {
				i = fill_sg2_comp_from_iov(gather_comp, i, params->src_iov, 0,
							   &size, aad_buf, aad_offset);
			}
			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer space,"
					   " size %d needed",
					   size);
				return -1;
			}
		}
	}

	gather_sz = (i + 2) / 3;
	g_size_bytes = gather_sz * sizeof(struct roc_sg2list_comp);

	/*
	 * Output Scatter List
	 */

	i = 0;
	scatter_comp = (struct roc_sg2list_comp *)((uint8_t *)gather_comp + g_size_bytes);

	if (zsk_flags == 0x1) {
		/* IV in SLIST only for EEA3 & UEA2 or for F8 */
		iv_len = 0;
	}

	if (iv_len) {
		i = fill_sg2_comp(scatter_comp, i, (uint64_t)offset_vaddr + ROC_SE_OFF_CTRL_LEN,
				  iv_len);
	}

	/* Add output data */
	if ((!decrypt) && (req_flags & ROC_SE_VALID_MAC_BUF)) {
		size = outputlen - iv_len - mac_len;
		if (size) {

			uint32_t aad_offset = aad_len ? passthrough_len : 0;

			if (unlikely(req_flags & ROC_SE_SINGLE_BUF_INPLACE)) {
				i = fill_sg2_comp_from_buf_min(scatter_comp, i, params->bufs,
							       &size);
			} else {
				i = fill_sg2_comp_from_iov(scatter_comp, i, params->dst_iov, 0,
							   &size, aad_buf, aad_offset);
			}
			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer space,"
					   " size %d needed",
					   size);
				return -1;
			}
		}

		/* mac data */
		if (mac_len)
			i = fill_sg2_comp_from_buf(scatter_comp, i, &params->mac_buf);
	} else {
		/* Output including mac */
		size = outputlen - iv_len;
		if (size) {
			uint32_t aad_offset = aad_len ? passthrough_len : 0;

			if (unlikely(req_flags & ROC_SE_SINGLE_BUF_INPLACE)) {
				i = fill_sg2_comp_from_buf_min(scatter_comp, i, params->bufs,
							       &size);
			} else {
				i = fill_sg2_comp_from_iov(scatter_comp, i, params->dst_iov, 0,
							   &size, aad_buf, aad_offset);
			}

			if (unlikely(size)) {
				plt_dp_err("Insufficient buffer space,"
					   " size %d needed",
					   size);
				return -1;
			}
		}
	}

	scatter_sz = (i + 2) / 3;

	cpt_inst_w5.s.gather_sz = gather_sz;
	cpt_inst_w6.s.scatter_sz = scatter_sz;

	cpt_inst_w5.s.dptr = (uint64_t)gather_comp;
	cpt_inst_w6.s.rptr = (uint64_t)scatter_comp;

	inst->w5.u64 = cpt_inst_w5.u64;
	inst->w6.u64 = cpt_inst_w6.u64;

	if (unlikely((scatter_sz >> 4) || (gather_sz >> 4))) {
		plt_dp_err("Exceeds max supported components. Reduce segments");
		ret = -1;
	}

	return ret;
}

static __rte_always_inline int
cpt_digest_gen_sg_ver1_prep(uint32_t flags, uint64_t d_lens, struct roc_se_fc_params *params,
			    struct cpt_inst_s *inst)
{
	struct roc_sglist_comp *gather_comp, *scatter_comp;
	void *m_vaddr = params->meta_buf.vaddr;
	uint32_t g_size_bytes, s_size_bytes;
	uint16_t data_len, mac_len, key_len;
	union cpt_inst_w4 cpt_inst_w4;
	roc_se_auth_type hash_type;
	struct roc_se_ctx *ctx;
	uint8_t *in_buffer;
	uint32_t size, i;
	int ret = 0;

	ctx = params->ctx;

	hash_type = ctx->hash_type;
	mac_len = ctx->mac_len;
	key_len = ctx->auth_key_len;
	data_len = ROC_SE_AUTH_DLEN(d_lens);

	cpt_inst_w4.u64 = ctx->template_w4.u64;
	cpt_inst_w4.s.param2 = ((uint16_t)hash_type << 8) | mac_len;
	if (ctx->hmac) {
		cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_HMAC | ROC_DMA_MODE_SG;
		cpt_inst_w4.s.param1 = key_len;
		cpt_inst_w4.s.dlen = data_len + RTE_ALIGN_CEIL(key_len, 8);
	} else {
		cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_HASH | ROC_DMA_MODE_SG;
		cpt_inst_w4.s.param1 = 0;
		cpt_inst_w4.s.dlen = data_len;
	}

	/* DPTR has SG list */
	in_buffer = m_vaddr;

	((uint16_t *)in_buffer)[0] = 0;
	((uint16_t *)in_buffer)[1] = 0;

	/* TODO Add error check if space will be sufficient */
	gather_comp = (struct roc_sglist_comp *)((uint8_t *)m_vaddr + 8);

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
	i = fill_sg_comp_from_iov(gather_comp, i, params->src_iov, 0, &size, NULL, 0);
	if (unlikely(size)) {
		plt_dp_err("Insufficient dst IOV size, short by %dB", size);
		return -1;
	}
	((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);
	g_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

	/*
	 * Output Gather list
	 */

	i = 0;
	scatter_comp = (struct roc_sglist_comp *)((uint8_t *)gather_comp + g_size_bytes);

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
	s_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

	size = g_size_bytes + s_size_bytes + ROC_SG_LIST_HDR_SIZE;

	if (unlikely(size > ROC_SG_MAX_DLEN_SIZE)) {
		plt_dp_err("Exceeds max supported components. Reduce segments");
		ret = -1;
	}

	/* This is DPTR len in case of SG mode */
	cpt_inst_w4.s.dlen = size;

	inst->dptr = (uint64_t)in_buffer;
	inst->w4.u64 = cpt_inst_w4.u64;

	return ret;
}

static __rte_always_inline int
cpt_digest_gen_sg_ver2_prep(uint32_t flags, uint64_t d_lens, struct roc_se_fc_params *params,
			    struct cpt_inst_s *inst)
{
	uint16_t data_len, mac_len, key_len, scatter_sz, gather_sz;
	struct roc_sg2list_comp *gather_comp, *scatter_comp;
	void *m_vaddr = params->meta_buf.vaddr;
	union cpt_inst_w4 cpt_inst_w4;
	union cpt_inst_w5 cpt_inst_w5;
	union cpt_inst_w6 cpt_inst_w6;
	roc_se_auth_type hash_type;
	struct roc_se_ctx *ctx;
	uint32_t g_size_bytes;
	uint32_t size, i;
	int ret = 0;

	ctx = params->ctx;

	hash_type = ctx->hash_type;
	mac_len = ctx->mac_len;
	key_len = ctx->auth_key_len;
	data_len = ROC_SE_AUTH_DLEN(d_lens);

	cpt_inst_w4.u64 = ctx->template_w4.u64;
	cpt_inst_w4.s.param2 = ((uint16_t)hash_type << 8) | mac_len;
	if (ctx->hmac) {
		cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_HMAC;
		cpt_inst_w4.s.param1 = key_len;
		cpt_inst_w4.s.dlen = data_len + RTE_ALIGN_CEIL(key_len, 8);
	} else {
		cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_HASH;
		cpt_inst_w4.s.param1 = 0;
		cpt_inst_w4.s.dlen = data_len;
	}

	/* DPTR has SG list */

	/* TODO Add error check if space will be sufficient */
	gather_comp = (struct roc_sg2list_comp *)((uint8_t *)m_vaddr + 0);

	/*
	 * Input gather list
	 */

	i = 0;

	if (ctx->hmac) {
		uint64_t k_vaddr = (uint64_t)ctx->auth_key;
		/* Key */
		i = fill_sg2_comp(gather_comp, i, k_vaddr, RTE_ALIGN_CEIL(key_len, 8));
	}

	/* input data */
	size = data_len;
	i = fill_sg2_comp_from_iov(gather_comp, i, params->src_iov, 0, &size, NULL, 0);
	if (unlikely(size)) {
		plt_dp_err("Insufficient dst IOV size, short by %dB", size);
		return -1;
	}

	gather_sz = (i + 2) / 3;
	g_size_bytes = gather_sz * sizeof(struct roc_sg2list_comp);

	/*
	 * Output Gather list
	 */

	i = 0;
	scatter_comp = (struct roc_sg2list_comp *)((uint8_t *)gather_comp + g_size_bytes);

	if (flags & ROC_SE_VALID_MAC_BUF) {
		if (unlikely(params->mac_buf.size < mac_len)) {
			plt_dp_err("Insufficient MAC size");
			return -1;
		}

		size = mac_len;
		i = fill_sg2_comp_from_buf_min(scatter_comp, i, &params->mac_buf, &size);
	} else {
		size = mac_len;
		i = fill_sg2_comp_from_iov(scatter_comp, i, params->src_iov, data_len, &size, NULL,
					   0);
		if (unlikely(size)) {
			plt_dp_err("Insufficient dst IOV size, short by %dB", size);
			return -1;
		}
	}

	scatter_sz = (i + 2) / 3;

	cpt_inst_w5.s.gather_sz = gather_sz;
	cpt_inst_w6.s.scatter_sz = scatter_sz;

	cpt_inst_w5.s.dptr = (uint64_t)gather_comp;
	cpt_inst_w6.s.rptr = (uint64_t)scatter_comp;

	inst->w5.u64 = cpt_inst_w5.u64;
	inst->w6.u64 = cpt_inst_w6.u64;

	inst->w4.u64 = cpt_inst_w4.u64;

	if (unlikely((scatter_sz >> 4) || (gather_sz >> 4))) {
		plt_dp_err("Exceeds max supported components. Reduce segments");
		ret = -1;
	}

	return ret;
}

static inline int
pdcp_chain_sg1_prep(struct roc_se_fc_params *params, struct roc_se_ctx *cpt_ctx,
		    struct cpt_inst_s *inst, union cpt_inst_w4 w4, int32_t inputlen,
		    uint8_t hdr_len, uint64_t offset_ctrl, uint32_t req_flags, uint8_t *cipher_iv,
		    uint8_t *auth_iv, const int pack_iv, const uint8_t pdcp_ci_alg,
		    const uint8_t pdcp_auth_alg)
{
	struct roc_sglist_comp *scatter_comp, *gather_comp;
	void *m_vaddr = params->meta_buf.vaddr;
	uint32_t i, g_size_bytes, s_size_bytes;
	const uint32_t mac_len = 4;
	uint8_t *iv_d, *in_buffer;
	uint64_t *offset_vaddr;
	uint32_t size;
	int ret = 0;

	/* save space for IV */
	offset_vaddr = m_vaddr;

	m_vaddr = PLT_PTR_ADD(m_vaddr, ROC_SE_OFF_CTRL_LEN + PLT_ALIGN_CEIL(hdr_len, 8));

	w4.s.opcode_major |= (uint64_t)ROC_DMA_MODE_SG;

	/* DPTR has SG list */
	in_buffer = m_vaddr;

	((uint16_t *)in_buffer)[0] = 0;
	((uint16_t *)in_buffer)[1] = 0;

	gather_comp = PLT_PTR_ADD(m_vaddr, 8);

	/* Input Gather List */
	i = 0;

	/* Offset control word followed by IV */

	i = fill_sg_comp(gather_comp, i, (uint64_t)offset_vaddr, ROC_SE_OFF_CTRL_LEN + hdr_len);

	*(uint64_t *)offset_vaddr = offset_ctrl;

	/* Cipher IV */
	iv_d = ((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN);
	pdcp_iv_copy(iv_d, cipher_iv, pdcp_ci_alg, pack_iv);

	/* Auth IV */
	iv_d = ((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN + 16);
	pdcp_iv_copy(iv_d, auth_iv, pdcp_auth_alg, pack_iv);

	/* input data */
	size = inputlen - hdr_len;
	if (size) {
		i = fill_sg_comp_from_iov(gather_comp, i, params->src_iov, 0, &size, NULL, 0);
		if (unlikely(size)) {
			plt_dp_err("Insufficient buffer space, size %d needed", size);
			return -1;
		}
	}
	((uint16_t *)in_buffer)[2] = rte_cpu_to_be_16(i);
	g_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

	/*
	 * Output Scatter List
	 */

	i = 0;
	scatter_comp = PLT_PTR_ADD(gather_comp, g_size_bytes);

	if ((hdr_len)) {
		i = fill_sg_comp(scatter_comp, i, (uint64_t)offset_vaddr + ROC_SE_OFF_CTRL_LEN,
				 hdr_len);
	}

	/* Add output data */
	if (cpt_ctx->ciph_then_auth && (req_flags & ROC_SE_VALID_MAC_BUF))
		size = inputlen;
	else
		/* Output including mac */
		size = inputlen + mac_len;

	size -= hdr_len;

	if (size) {
		i = fill_sg_comp_from_iov(scatter_comp, i, params->dst_iov, 0, &size, NULL, 0);

		if (unlikely(size)) {
			plt_dp_err("Insufficient buffer space, size %d needed", size);
			return -1;
		}
	}

	((uint16_t *)in_buffer)[3] = rte_cpu_to_be_16(i);
	s_size_bytes = ((i + 3) / 4) * sizeof(struct roc_sglist_comp);

	size = g_size_bytes + s_size_bytes + ROC_SG_LIST_HDR_SIZE;

	if (unlikely(size > ROC_SG_MAX_DLEN_SIZE)) {
		plt_dp_err("Exceeds max supported components. Reduce segments");
		ret = -1;
	}

	/* This is DPTR len in case of SG mode */
	w4.s.dlen = size;
	inst->w4.u64 = w4.u64;

	inst->dptr = (uint64_t)in_buffer;

	return ret;
}

static inline int
pdcp_chain_sg2_prep(struct roc_se_fc_params *params, struct roc_se_ctx *cpt_ctx,
		    struct cpt_inst_s *inst, union cpt_inst_w4 w4, int32_t inputlen,
		    uint8_t hdr_len, uint64_t offset_ctrl, uint32_t req_flags, uint8_t *cipher_iv,
		    uint8_t *auth_iv, const int pack_iv, const uint8_t pdcp_ci_alg,
		    const uint8_t pdcp_auth_alg)
{
	struct roc_sg2list_comp *gather_comp, *scatter_comp;
	void *m_vaddr = params->meta_buf.vaddr;
	uint16_t scatter_sz, gather_sz;
	const uint32_t mac_len = 4;
	uint32_t i, g_size_bytes;
	uint64_t *offset_vaddr;
	union cpt_inst_w5 w5;
	union cpt_inst_w6 w6;
	uint8_t *iv_d;
	uint32_t size;
	int ret = 0;

	/* save space for IV */
	offset_vaddr = m_vaddr;

	m_vaddr = PLT_PTR_ADD(m_vaddr, ROC_SE_OFF_CTRL_LEN + RTE_ALIGN_CEIL(hdr_len, 8));

	w4.s.opcode_major |= (uint64_t)ROC_DMA_MODE_SG;
	w4.s.dlen = inputlen + ROC_SE_OFF_CTRL_LEN;

	gather_comp = m_vaddr;

	/* Input Gather List */
	i = 0;

	/* Offset control word followed by IV */
	*(uint64_t *)offset_vaddr = offset_ctrl;

	i = fill_sg2_comp(gather_comp, i, (uint64_t)offset_vaddr, ROC_SE_OFF_CTRL_LEN + hdr_len);

	/* Cipher IV */
	iv_d = ((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN);
	pdcp_iv_copy(iv_d, cipher_iv, pdcp_ci_alg, pack_iv);

	/* Auth IV */
	iv_d = ((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN + 16);
	pdcp_iv_copy(iv_d, auth_iv, pdcp_auth_alg, pack_iv);

	/* input data */
	size = inputlen - hdr_len;
	if (size) {
		i = fill_sg2_comp_from_iov(gather_comp, i, params->src_iov, 0, &size, NULL, 0);
		if (unlikely(size)) {
			plt_dp_err("Insufficient buffer space, size %d needed", size);
			return -1;
		}
	}

	gather_sz = (i + 2) / 3;
	g_size_bytes = gather_sz * sizeof(struct roc_sg2list_comp);

	/*
	 * Output Scatter List
	 */

	i = 0;
	scatter_comp = PLT_PTR_ADD(gather_comp, g_size_bytes);

	if ((hdr_len))
		i = fill_sg2_comp(scatter_comp, i, (uint64_t)(offset_vaddr) + ROC_SE_OFF_CTRL_LEN,
				  hdr_len);

	/* Add output data */
	if (cpt_ctx->ciph_then_auth && (req_flags & ROC_SE_VALID_MAC_BUF))
		size = inputlen;
	else
		/* Output including mac */
		size = inputlen + mac_len;

	size -= hdr_len;

	if (size) {
		i = fill_sg2_comp_from_iov(scatter_comp, i, params->dst_iov, 0, &size, NULL, 0);

		if (unlikely(size)) {
			plt_dp_err("Insufficient buffer space, size %d needed", size);
			return -1;
		}
	}

	scatter_sz = (i + 2) / 3;

	w5.s.gather_sz = gather_sz;
	w6.s.scatter_sz = scatter_sz;

	w5.s.dptr = (uint64_t)gather_comp;
	w6.s.rptr = (uint64_t)scatter_comp;

	inst->w4.u64 = w4.u64;
	inst->w5.u64 = w5.u64;
	inst->w6.u64 = w6.u64;

	if (unlikely((scatter_sz >> 4) || (gather_sz >> 4))) {
		plt_dp_err("Exceeds max supported components. Reduce segments");
		ret = -1;
	}

	return ret;
}

static __rte_always_inline int
cpt_sm_prep(uint32_t flags, uint64_t d_offs, uint64_t d_lens, struct roc_se_fc_params *fc_params,
	    struct cpt_inst_s *inst, const bool is_sg_ver2, int decrypt)
{
	int32_t inputlen, outputlen, enc_dlen;
	union cpt_inst_w4 cpt_inst_w4;
	uint32_t passthrough_len = 0;
	struct roc_se_ctx *se_ctx;
	uint32_t encr_data_len;
	uint32_t encr_offset;
	uint64_t offset_ctrl;
	uint8_t iv_len = 16;
	uint8_t *src = NULL;
	void *offset_vaddr;
	int ret;

	encr_offset = ROC_SE_ENCR_OFFSET(d_offs);
	encr_data_len = ROC_SE_ENCR_DLEN(d_lens);

	se_ctx = fc_params->ctx;
	cpt_inst_w4.u64 = se_ctx->template_w4.u64;

	if (unlikely(!(flags & ROC_SE_VALID_IV_BUF)))
		iv_len = 0;

	encr_offset += iv_len;
	enc_dlen = encr_data_len + encr_offset;
	enc_dlen = RTE_ALIGN_CEIL(encr_data_len, 8) + encr_offset;

	inputlen = enc_dlen;
	outputlen = enc_dlen;

	cpt_inst_w4.s.param1 = encr_data_len;

	if (unlikely(encr_offset >> 8)) {
		plt_dp_err("Offset not supported");
		plt_dp_err("enc_offset: %d", encr_offset);
		return -1;
	}

	offset_ctrl = rte_cpu_to_be_64((uint64_t)encr_offset);

	/*
	 * In cn9k, cn10k since we have a limitation of
	 * IV & Offset control word not part of instruction
	 * and need to be part of Data Buffer, we check if
	 * head room is there and then only do the Direct mode processing
	 */
	if (likely((flags & ROC_SE_SINGLE_BUF_INPLACE) && (flags & ROC_SE_SINGLE_BUF_HEADROOM))) {
		void *dm_vaddr = fc_params->bufs[0].vaddr;

		/* Use Direct mode */

		offset_vaddr = PLT_PTR_SUB(dm_vaddr, ROC_SE_OFF_CTRL_LEN + iv_len);
		*(uint64_t *)offset_vaddr = offset_ctrl;

		/* DPTR */
		inst->dptr = (uint64_t)offset_vaddr;

		/* RPTR should just exclude offset control word */
		inst->rptr = (uint64_t)dm_vaddr - iv_len;

		cpt_inst_w4.s.dlen = inputlen + ROC_SE_OFF_CTRL_LEN;

		if (likely(iv_len)) {
			void *dst = PLT_PTR_ADD(offset_vaddr, ROC_SE_OFF_CTRL_LEN);
			uint64_t *src = fc_params->iv_buf;

			rte_memcpy(dst, src, 16);
		}
		inst->w4.u64 = cpt_inst_w4.u64;
	} else {
		if (likely(iv_len))
			src = fc_params->iv_buf;

		inst->w4.u64 = cpt_inst_w4.u64;

		if (is_sg_ver2)
			ret = sg2_inst_prep(fc_params, inst, offset_ctrl, src, iv_len, 0, 0,
					    inputlen, outputlen, passthrough_len, flags, 0,
					    decrypt);
		else
			ret = sg_inst_prep(fc_params, inst, offset_ctrl, src, iv_len, 0, 0,
					   inputlen, outputlen, passthrough_len, flags, 0, decrypt);

		if (unlikely(ret)) {
			plt_dp_err("sg prep failed");
			return -1;
		}
	}

	return 0;
}

static __rte_always_inline int
cpt_enc_hmac_prep(uint32_t flags, uint64_t d_offs, uint64_t d_lens,
		  struct roc_se_fc_params *fc_params, struct cpt_inst_s *inst,
		  const bool is_sg_ver2)
{
	uint32_t iv_offset = 0;
	int32_t inputlen, outputlen, enc_dlen, auth_dlen;
	struct roc_se_ctx *se_ctx;
	uint32_t cipher_type, hash_type;
	uint32_t mac_len;
	uint8_t iv_len = 16;
	uint32_t encr_offset, auth_offset;
	uint64_t offset_ctrl;
	uint32_t encr_data_len, auth_data_len, aad_len = 0;
	uint32_t passthrough_len = 0;
	union cpt_inst_w4 cpt_inst_w4;
	void *offset_vaddr;
	uint8_t op_minor;
	uint8_t *src = NULL;
	int ret;

	encr_offset = ROC_SE_ENCR_OFFSET(d_offs);
	auth_offset = ROC_SE_AUTH_OFFSET(d_offs);
	encr_data_len = ROC_SE_ENCR_DLEN(d_lens);
	auth_data_len = ROC_SE_AUTH_DLEN(d_lens);
	if (unlikely(flags & ROC_SE_VALID_AAD_BUF)) {
		/* We don't support both AAD and auth data separately */
		auth_data_len = 0;
		auth_offset = 0;
		aad_len = fc_params->aad_buf.size;
	}

	se_ctx = fc_params->ctx;
	cipher_type = se_ctx->enc_cipher;
	hash_type = se_ctx->hash_type;
	mac_len = se_ctx->mac_len;
	cpt_inst_w4.u64 = se_ctx->template_w4.u64;
	op_minor = cpt_inst_w4.s.opcode_minor;

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
	cpt_inst_w4.s.opcode_minor |= ROC_SE_FC_MINOR_OP_ENCRYPT;

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

	cpt_inst_w4.s.param1 = encr_data_len;
	cpt_inst_w4.s.param2 = auth_data_len;

	if (unlikely((encr_offset >> 16) || (iv_offset >> 8) || (auth_offset >> 8))) {
		plt_dp_err("Offset not supported");
		plt_dp_err("enc_offset: %d", encr_offset);
		plt_dp_err("iv_offset : %d", iv_offset);
		plt_dp_err("auth_offset: %d", auth_offset);
		return -1;
	}

	offset_ctrl = rte_cpu_to_be_64(((uint64_t)encr_offset << 16) | ((uint64_t)iv_offset << 8) |
				       ((uint64_t)auth_offset));

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

		offset_vaddr = (uint8_t *)dm_vaddr - ROC_SE_OFF_CTRL_LEN - iv_len;

		*(uint64_t *)offset_vaddr =
			rte_cpu_to_be_64(((uint64_t)encr_offset << 16) |
					 ((uint64_t)iv_offset << 8) | ((uint64_t)auth_offset));

		/* DPTR */
		inst->dptr = (uint64_t)offset_vaddr;

		/* RPTR should just exclude offset control word */
		inst->rptr = (uint64_t)dm_vaddr - iv_len;

		cpt_inst_w4.s.dlen = inputlen + ROC_SE_OFF_CTRL_LEN;

		if (likely(iv_len)) {
			uint64_t *dest =
				(uint64_t *)((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN);
			uint64_t *src = fc_params->iv_buf;
			dest[0] = src[0];
			dest[1] = src[1];
		}

		inst->w4.u64 = cpt_inst_w4.u64;
	} else {
		if (likely(iv_len))
			src = fc_params->iv_buf;

		inst->w4.u64 = cpt_inst_w4.u64;

		if (is_sg_ver2)
			ret = sg2_inst_prep(fc_params, inst, offset_ctrl, src, iv_len, 0, 0,
					    inputlen, outputlen, passthrough_len, flags, 0, 0);
		else
			ret = sg_inst_prep(fc_params, inst, offset_ctrl, src, iv_len, 0, 0,
					   inputlen, outputlen, passthrough_len, flags, 0, 0);

		if (unlikely(ret)) {
			plt_dp_err("sg prep failed");
			return -1;
		}
	}

	return 0;
}

static __rte_always_inline int
cpt_dec_hmac_prep(uint32_t flags, uint64_t d_offs, uint64_t d_lens,
		  struct roc_se_fc_params *fc_params, struct cpt_inst_s *inst,
		  const bool is_sg_ver2)
{
	uint32_t iv_offset = 0;
	int32_t inputlen, outputlen, enc_dlen, auth_dlen;
	struct roc_se_ctx *se_ctx;
	int32_t hash_type, mac_len;
	uint8_t iv_len = 16;
	uint32_t encr_offset, auth_offset;
	uint32_t encr_data_len, auth_data_len, aad_len = 0;
	uint32_t passthrough_len = 0;
	union cpt_inst_w4 cpt_inst_w4;
	void *offset_vaddr;
	uint8_t op_minor;
	uint64_t offset_ctrl;
	uint8_t *src = NULL;
	int ret;

	encr_offset = ROC_SE_ENCR_OFFSET(d_offs);
	auth_offset = ROC_SE_AUTH_OFFSET(d_offs);
	encr_data_len = ROC_SE_ENCR_DLEN(d_lens);
	auth_data_len = ROC_SE_AUTH_DLEN(d_lens);

	if (unlikely(flags & ROC_SE_VALID_AAD_BUF)) {
		/* We don't support both AAD and auth data separately */
		auth_data_len = 0;
		auth_offset = 0;
		aad_len = fc_params->aad_buf.size;
	}

	se_ctx = fc_params->ctx;
	hash_type = se_ctx->hash_type;
	mac_len = se_ctx->mac_len;
	cpt_inst_w4.u64 = se_ctx->template_w4.u64;
	op_minor = cpt_inst_w4.s.opcode_minor;

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

	if (unlikely((encr_offset >> 16) || (iv_offset >> 8) || (auth_offset >> 8))) {
		plt_dp_err("Offset not supported");
		plt_dp_err("enc_offset: %d", encr_offset);
		plt_dp_err("iv_offset : %d", iv_offset);
		plt_dp_err("auth_offset: %d", auth_offset);
		return -1;
	}

	offset_ctrl = rte_cpu_to_be_64(((uint64_t)encr_offset << 16) | ((uint64_t)iv_offset << 8) |
				       ((uint64_t)auth_offset));

	/*
	 * In cn9k, cn10k since we have a limitation of
	 * IV & Offset control word not part of instruction
	 * and need to be part of Data Buffer, we check if
	 * head room is there and then only do the Direct mode processing
	 */
	if (likely((flags & ROC_SE_SINGLE_BUF_INPLACE) && (flags & ROC_SE_SINGLE_BUF_HEADROOM))) {
		void *dm_vaddr = fc_params->bufs[0].vaddr;

		/* Use Direct mode */

		offset_vaddr = (uint8_t *)dm_vaddr - ROC_SE_OFF_CTRL_LEN - iv_len;

		*(uint64_t *)offset_vaddr =
			rte_cpu_to_be_64(((uint64_t)encr_offset << 16) |
					 ((uint64_t)iv_offset << 8) | ((uint64_t)auth_offset));

		inst->dptr = (uint64_t)offset_vaddr;

		/* RPTR should just exclude offset control word */
		inst->rptr = (uint64_t)dm_vaddr - iv_len;

		cpt_inst_w4.s.dlen = inputlen + ROC_SE_OFF_CTRL_LEN;

		if (likely(iv_len)) {
			uint64_t *dest =
				(uint64_t *)((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN);
			uint64_t *src = fc_params->iv_buf;
			dest[0] = src[0];
			dest[1] = src[1];
		}
		inst->w4.u64 = cpt_inst_w4.u64;

	} else {
		if (likely(iv_len)) {
			src = fc_params->iv_buf;
		}

		inst->w4.u64 = cpt_inst_w4.u64;

		if (is_sg_ver2)
			ret = sg2_inst_prep(fc_params, inst, offset_ctrl, src, iv_len, 0, 0,
					    inputlen, outputlen, passthrough_len, flags, 0, 1);
		else
			ret = sg_inst_prep(fc_params, inst, offset_ctrl, src, iv_len, 0, 0,
					   inputlen, outputlen, passthrough_len, flags, 0, 1);
		if (unlikely(ret)) {
			plt_dp_err("sg prep failed");
			return -1;
		}
	}

	return 0;
}

static __rte_always_inline int
cpt_pdcp_chain_alg_prep(uint32_t req_flags, uint64_t d_offs, uint64_t d_lens,
			struct roc_se_fc_params *params, struct cpt_inst_s *inst,
			const bool is_sg_ver2)
{
	uint32_t encr_data_len, auth_data_len, aad_len, passthr_len, pad_len, hdr_len;
	uint32_t encr_offset, auth_offset, iv_offset = 0;
	uint8_t *auth_iv = NULL, *cipher_iv = NULL;
	uint8_t pdcp_ci_alg, pdcp_auth_alg;
	union cpt_inst_w4 cpt_inst_w4;
	struct roc_se_ctx *se_ctx;
	uint64_t *offset_vaddr;
	const int iv_len = 32;
	uint64_t offset_ctrl;
	uint8_t pack_iv = 0;
	int32_t inputlen;
	void *dm_vaddr;
	uint8_t *iv_d;

	encr_offset = ROC_SE_ENCR_OFFSET(d_offs);
	auth_offset = ROC_SE_AUTH_OFFSET(d_offs);

	aad_len = encr_offset - auth_offset;

	if (unlikely(encr_offset >> 16)) {
		plt_dp_err("Offset not supported");
		plt_dp_err("enc_offset: %d", encr_offset);
		return -1;
	}

	se_ctx = params->ctx;
	pdcp_ci_alg = se_ctx->pdcp_ci_alg;
	pdcp_auth_alg = se_ctx->pdcp_auth_alg;

	encr_data_len = ROC_SE_ENCR_DLEN(d_lens);
	auth_data_len = ROC_SE_AUTH_DLEN(d_lens);
	auth_data_len -= aad_len;

	encr_offset += iv_len;
	auth_offset = encr_offset - aad_len;
	passthr_len = RTE_ALIGN_CEIL(auth_offset, 8);

	if (unlikely((aad_len >> 16) || (passthr_len >> 8))) {
		plt_dp_err("Length not supported");
		plt_dp_err("AAD_len: %d", aad_len);
		plt_dp_err("Passthrough_len: %d", passthr_len);
		return -1;
	}

	cpt_inst_w4.u64 = se_ctx->template_w4.u64;
	cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_PDCP_CHAIN;

	cpt_inst_w4.s.param1 = auth_data_len;
	cpt_inst_w4.s.param2 = 0;

	if (likely(params->auth_iv_len))
		auth_iv = params->auth_iv_buf;

	if (likely(params->cipher_iv_len))
		cipher_iv = params->iv_buf;

	pad_len = passthr_len - auth_offset;
	hdr_len = iv_len + pad_len;

	if (se_ctx->auth_then_ciph)
		inputlen = auth_data_len;
	else
		inputlen = encr_data_len;

	inputlen += (encr_offset + pad_len);

	offset_ctrl = rte_cpu_to_be_64(((uint64_t)(aad_len) << 16) | ((uint64_t)(iv_offset) << 8) |
				       ((uint64_t)(passthr_len)));

	if (likely(((req_flags & ROC_SE_SINGLE_BUF_INPLACE)) &&
		   ((req_flags & ROC_SE_SINGLE_BUF_HEADROOM)))) {

		dm_vaddr = params->bufs[0].vaddr;

		/* Use Direct mode */

		offset_vaddr = PLT_PTR_SUB(dm_vaddr, ROC_SE_OFF_CTRL_LEN + hdr_len);
		*offset_vaddr = offset_ctrl;

		/* DPTR */
		inst->dptr = (uint64_t)offset_vaddr;
		/* RPTR should just exclude offset control word */
		inst->rptr = (uint64_t)PLT_PTR_SUB(dm_vaddr, hdr_len);

		cpt_inst_w4.s.dlen = inputlen + ROC_SE_OFF_CTRL_LEN;

		iv_d = ((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN);
		pdcp_iv_copy(iv_d, cipher_iv, pdcp_ci_alg, pack_iv);

		iv_d = ((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN + 16);
		pdcp_iv_copy(iv_d, auth_iv, pdcp_auth_alg, pack_iv);

		inst->w4.u64 = cpt_inst_w4.u64;
		return 0;

	} else {
		if (is_sg_ver2)
			return pdcp_chain_sg2_prep(params, se_ctx, inst, cpt_inst_w4, inputlen,
						   hdr_len, offset_ctrl, req_flags, cipher_iv,
						   auth_iv, pack_iv, pdcp_ci_alg, pdcp_auth_alg);
		else
			return pdcp_chain_sg1_prep(params, se_ctx, inst, cpt_inst_w4, inputlen,
						   hdr_len, offset_ctrl, req_flags, cipher_iv,
						   auth_iv, pack_iv, pdcp_ci_alg, pdcp_auth_alg);
	}
}

static __rte_always_inline int
cpt_pdcp_alg_prep(uint32_t req_flags, uint64_t d_offs, uint64_t d_lens,
		  struct roc_se_fc_params *params, struct cpt_inst_s *inst, const bool is_sg_ver2)
{
	int32_t inputlen, outputlen;
	struct roc_se_ctx *se_ctx;
	uint32_t mac_len = 0;
	uint8_t pdcp_alg_type;
	uint32_t encr_offset, auth_offset;
	uint32_t encr_data_len, auth_data_len;
	int flags, iv_len;
	uint64_t offset_ctrl;
	uint64_t *offset_vaddr;
	uint8_t *iv_s;
	uint8_t pack_iv = 0;
	union cpt_inst_w4 cpt_inst_w4;
	int ret;

	se_ctx = params->ctx;
	flags = se_ctx->zsk_flags;
	mac_len = se_ctx->mac_len;

	cpt_inst_w4.u64 = se_ctx->template_w4.u64;
	cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_PDCP;

	if (flags == 0x1) {
		iv_s = params->auth_iv_buf;

		/*
		 * Microcode expects offsets in bytes
		 * TODO: Rounding off
		 */
		auth_data_len = ROC_SE_AUTH_DLEN(d_lens);
		auth_offset = ROC_SE_AUTH_OFFSET(d_offs);
		pdcp_alg_type = se_ctx->pdcp_auth_alg;

		if (pdcp_alg_type != ROC_SE_PDCP_ALG_TYPE_AES_CMAC) {
			iv_len = params->auth_iv_len;

			if (iv_len == 25) {
				roc_se_zuc_bytes_swap(iv_s, iv_len);
				iv_len -= 2;
				pack_iv = 1;
			}

			auth_offset = auth_offset / 8;

			/* consider iv len */
			auth_offset += iv_len;

			inputlen =
				auth_offset + (RTE_ALIGN(auth_data_len, 8) / 8);
		} else {
			iv_len = 16;

			/* consider iv len */
			auth_offset += iv_len;

			inputlen = auth_offset + auth_data_len;

			/* length should be in bits */
			auth_data_len *= 8;
		}

		outputlen = mac_len;

		offset_ctrl = rte_cpu_to_be_64((uint64_t)auth_offset);

		encr_data_len = 0;
		encr_offset = 0;
	} else {
		iv_s = params->iv_buf;
		iv_len = params->cipher_iv_len;
		pdcp_alg_type = se_ctx->pdcp_ci_alg;

		if (iv_len == 25) {
			roc_se_zuc_bytes_swap(iv_s, iv_len);
			iv_len -= 2;
			pack_iv = 1;
		}

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
	}

	if (unlikely((encr_offset >> 16) || (auth_offset >> 8))) {
		plt_dp_err("Offset not supported");
		plt_dp_err("enc_offset: %d", encr_offset);
		plt_dp_err("auth_offset: %d", auth_offset);
		return -1;
	}

	/*
	 * Lengths are expected in bits.
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

		offset_vaddr = (uint64_t *)((uint8_t *)dm_vaddr - ROC_SE_OFF_CTRL_LEN - iv_len);

		/* DPTR */
		inst->dptr = (uint64_t)offset_vaddr;
		/* RPTR should just exclude offset control word */
		inst->rptr = (uint64_t)dm_vaddr - iv_len;

		cpt_inst_w4.s.dlen = inputlen + ROC_SE_OFF_CTRL_LEN;

		uint8_t *iv_d = ((uint8_t *)offset_vaddr + ROC_SE_OFF_CTRL_LEN);
		pdcp_iv_copy(iv_d, iv_s, pdcp_alg_type, pack_iv);

		*offset_vaddr = offset_ctrl;
		inst->w4.u64 = cpt_inst_w4.u64;
	} else {
		inst->w4.u64 = cpt_inst_w4.u64;
		if (is_sg_ver2)
			ret = sg2_inst_prep(params, inst, offset_ctrl, iv_s, iv_len, pack_iv,
					    pdcp_alg_type, inputlen, outputlen, 0, req_flags, 1, 0);
		else
			ret = sg_inst_prep(params, inst, offset_ctrl, iv_s, iv_len, pack_iv,
					   pdcp_alg_type, inputlen, outputlen, 0, req_flags, 1, 0);
		if (unlikely(ret)) {
			plt_dp_err("sg prep failed");
			return -1;
		}
	}

	return 0;
}

static __rte_always_inline int
cpt_kasumi_enc_prep(uint32_t req_flags, uint64_t d_offs, uint64_t d_lens,
		    struct roc_se_fc_params *params, struct cpt_inst_s *inst, const bool is_sg_ver2)
{
	int32_t inputlen = 0, outputlen = 0;
	struct roc_se_ctx *se_ctx;
	uint32_t mac_len = 0;
	uint32_t encr_offset, auth_offset;
	uint32_t encr_data_len, auth_data_len;
	int flags;
	uint8_t *iv_s, iv_len = 8;
	uint8_t dir = 0;
	uint64_t offset_ctrl;
	union cpt_inst_w4 cpt_inst_w4;

	encr_offset = ROC_SE_ENCR_OFFSET(d_offs) / 8;
	auth_offset = ROC_SE_AUTH_OFFSET(d_offs) / 8;
	encr_data_len = ROC_SE_ENCR_DLEN(d_lens);
	auth_data_len = ROC_SE_AUTH_DLEN(d_lens);

	se_ctx = params->ctx;
	iv_s = params->iv_buf;
	flags = se_ctx->zsk_flags;
	mac_len = se_ctx->mac_len;

	dir = iv_s[8] & 0x1;
	cpt_inst_w4.u64 = se_ctx->template_w4.u64;

	if (flags == 0x0) {
		/* Consider IV len */
		encr_offset += iv_len;
		auth_offset += iv_len;

		inputlen = encr_offset + (RTE_ALIGN(encr_data_len, 8) / 8);
		outputlen = inputlen;
		/* iv offset is 0 */
		offset_ctrl = rte_cpu_to_be_64((uint64_t)encr_offset << 16);
		if (unlikely((encr_offset >> 16))) {
			plt_dp_err("Offset not supported");
			plt_dp_err("enc_offset: %d", encr_offset);
			return -1;
		}
	} else {
		inputlen = auth_offset + (RTE_ALIGN(auth_data_len, 8) / 8);
		outputlen = mac_len;
		/* iv offset is 0 */
		offset_ctrl = rte_cpu_to_be_64((uint64_t)auth_offset);
		if (unlikely((auth_offset >> 8))) {
			plt_dp_err("Offset not supported");
			plt_dp_err("auth_offset: %d", auth_offset);
			return -1;
		}
	}

	cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_KASUMI | ROC_DMA_MODE_SG;

	/* Indicate ECB/CBC, direction, CTX from CPTR, IV from DPTR */
	cpt_inst_w4.s.opcode_minor =
		((1 << 6) | (se_ctx->k_ecb << 5) | (dir << 4) | (0 << 3) | (flags & 0x7));

	cpt_inst_w4.s.param1 = encr_data_len;
	cpt_inst_w4.s.param2 = auth_data_len;

	inst->w4.u64 = cpt_inst_w4.u64;
	if (is_sg_ver2)
		sg2_inst_prep(params, inst, offset_ctrl, iv_s, iv_len, 0, 0, inputlen, outputlen, 0,
			      req_flags, 0, 0);
	else
		sg_inst_prep(params, inst, offset_ctrl, iv_s, iv_len, 0, 0, inputlen, outputlen, 0,
			     req_flags, 0, 0);

	return 0;
}

static __rte_always_inline int
cpt_kasumi_dec_prep(uint64_t d_offs, uint64_t d_lens, struct roc_se_fc_params *params,
		    struct cpt_inst_s *inst, const bool is_sg_ver2)
{
	int32_t inputlen = 0, outputlen;
	struct roc_se_ctx *se_ctx;
	uint8_t iv_len = 8;
	uint32_t encr_offset;
	uint32_t encr_data_len;
	int flags;
	uint8_t dir = 0;
	union cpt_inst_w4 cpt_inst_w4;
	uint64_t offset_ctrl;

	encr_offset = ROC_SE_ENCR_OFFSET(d_offs) / 8;
	encr_data_len = ROC_SE_ENCR_DLEN(d_lens);

	se_ctx = params->ctx;
	flags = se_ctx->zsk_flags;

	cpt_inst_w4.u64 = 0;
	cpt_inst_w4.s.opcode_major = ROC_SE_MAJOR_OP_KASUMI | ROC_DMA_MODE_SG;

	/* indicates ECB/CBC, direction, ctx from cptr, iv from dptr */
	cpt_inst_w4.s.opcode_minor =
		((1 << 6) | (se_ctx->k_ecb << 5) | (dir << 4) | (0 << 3) | (flags & 0x7));

	/*
	 * Lengths are expected in bits.
	 */
	cpt_inst_w4.s.param1 = encr_data_len;

	/* consider iv len */
	encr_offset += iv_len;

	inputlen = encr_offset + (RTE_ALIGN(encr_data_len, 8) / 8);
	outputlen = inputlen;

	offset_ctrl = rte_cpu_to_be_64((uint64_t)encr_offset << 16);
	if (unlikely((encr_offset >> 16))) {
		plt_dp_err("Offset not supported");
		plt_dp_err("enc_offset: %d", encr_offset);
		return -1;
	}

	inst->w4.u64 = cpt_inst_w4.u64;
	if (is_sg_ver2)
		sg2_inst_prep(params, inst, offset_ctrl, params->iv_buf, iv_len, 0, 0, inputlen,
			      outputlen, 0, 0, 0, 1);
	else
		sg_inst_prep(params, inst, offset_ctrl, params->iv_buf, iv_len, 0, 0, inputlen,
			     outputlen, 0, 0, 0, 1);

	return 0;
}

static __rte_always_inline int
cpt_fc_enc_hmac_prep(uint32_t flags, uint64_t d_offs, uint64_t d_lens,
		     struct roc_se_fc_params *fc_params, struct cpt_inst_s *inst,
		     const bool is_sg_ver2)
{
	struct roc_se_ctx *ctx = fc_params->ctx;
	uint8_t fc_type;
	int ret = -1;

	fc_type = ctx->fc_type;

	if (likely(fc_type == ROC_SE_FC_GEN)) {
		ret = cpt_enc_hmac_prep(flags, d_offs, d_lens, fc_params, inst, is_sg_ver2);
	} else if (fc_type == ROC_SE_PDCP) {
		ret = cpt_pdcp_alg_prep(flags, d_offs, d_lens, fc_params, inst, is_sg_ver2);
	} else if (fc_type == ROC_SE_KASUMI) {
		ret = cpt_kasumi_enc_prep(flags, d_offs, d_lens, fc_params, inst, is_sg_ver2);
	} else if (fc_type == ROC_SE_HASH_HMAC) {
		if (is_sg_ver2)
			ret = cpt_digest_gen_sg_ver2_prep(flags, d_lens, fc_params, inst);
		else
			ret = cpt_digest_gen_sg_ver1_prep(flags, d_lens, fc_params, inst);
	}

	return ret;
}

static __rte_always_inline int
fill_sess_aead(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	struct rte_crypto_aead_xform *aead_form;
	uint8_t aes_gcm = 0, aes_ccm = 0;
	roc_se_cipher_type enc_type = 0; /* NULL Cipher type */
	roc_se_auth_type auth_type = 0;	 /* NULL Auth type */
	uint32_t cipher_key_len = 0;
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
		enc_type = ROC_SE_AES_CCM;
		cipher_key_len = 16;
		aes_ccm = 1;
		break;
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
	sess->aes_ccm = aes_ccm;
	sess->mac_len = aead_form->digest_length;
	sess->iv_offset = aead_form->iv.offset;
	sess->iv_length = aead_form->iv.length;
	sess->aad_length = aead_form->aad_length;

	if (aes_ccm) {
		if ((sess->iv_length < 11) || (sess->iv_length > 13)) {
			plt_dp_err("Crypto: Unsupported IV length %u", sess->iv_length);
			return -1;
		}
	} else {
		switch (sess->iv_length) {
		case 12:
			sess->short_iv = 1;
		case 16:
			break;
		default:
			plt_dp_err("Crypto: Unsupported IV length %u", sess->iv_length);
			return -1;
		}
	}

	if (unlikely(roc_se_ciph_key_set(&sess->roc_se_ctx, enc_type, aead_form->key.data,
					 aead_form->key.length)))
		return -1;

	if (unlikely(roc_se_auth_key_set(&sess->roc_se_ctx, auth_type, NULL, 0,
					 aead_form->digest_length)))
		return -1;

	if (enc_type == ROC_SE_CHACHA20)
		sess->roc_se_ctx.template_w4.s.opcode_minor |= BIT(5);
	return 0;
}

static __rte_always_inline int
fill_sm_sess_cipher(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	struct roc_se_sm_context *sm_ctx = &sess->roc_se_ctx.se_ctx.sm_ctx;
	struct rte_crypto_cipher_xform *c_form;
	roc_sm_cipher_type enc_type = 0;

	c_form = &xform->cipher;

	if (c_form->op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		sess->cpt_op |= ROC_SE_OP_CIPHER_ENCRYPT;
		sess->roc_se_ctx.template_w4.s.opcode_minor = ROC_SE_FC_MINOR_OP_ENCRYPT;
	} else if (c_form->op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
		sess->cpt_op |= ROC_SE_OP_CIPHER_DECRYPT;
		sess->roc_se_ctx.template_w4.s.opcode_minor = ROC_SE_FC_MINOR_OP_DECRYPT;
	} else {
		plt_dp_err("Unknown cipher operation\n");
		return -1;
	}

	switch (c_form->algo) {
	case RTE_CRYPTO_CIPHER_SM4_CBC:
		enc_type = ROC_SM4_CBC;
		break;
	case RTE_CRYPTO_CIPHER_SM4_ECB:
		enc_type = ROC_SM4_ECB;
		break;
	case RTE_CRYPTO_CIPHER_SM4_CTR:
		enc_type = ROC_SM4_CTR;
		break;
	case RTE_CRYPTO_CIPHER_SM4_CFB:
		enc_type = ROC_SM4_CFB;
		break;
	case RTE_CRYPTO_CIPHER_SM4_OFB:
		enc_type = ROC_SM4_OFB;
		break;
	default:
		plt_dp_err("Crypto: Undefined cipher algo %u specified", c_form->algo);
		return -1;
	}

	sess->iv_offset = c_form->iv.offset;
	sess->iv_length = c_form->iv.length;

	if (c_form->key.length != ROC_SE_SM4_KEY_LEN) {
		plt_dp_err("Invalid cipher params keylen %u", c_form->key.length);
		return -1;
	}

	sess->zsk_flag = 0;
	sess->zs_cipher = 0;
	sess->aes_gcm = 0;
	sess->aes_ctr = 0;
	sess->is_null = 0;
	sess->is_sm4 = 1;
	sess->roc_se_ctx.fc_type = ROC_SE_SM;

	sess->roc_se_ctx.template_w4.s.opcode_major = ROC_SE_MAJOR_OP_SM;

	memcpy(sm_ctx->encr_key, c_form->key.data, ROC_SE_SM4_KEY_LEN);
	sm_ctx->enc_cipher = enc_type;

	return 0;
}

static __rte_always_inline int
fill_sess_cipher(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	uint8_t zsk_flag = 0, zs_cipher = 0, aes_ctr = 0, is_null = 0;
	struct rte_crypto_cipher_xform *c_form;
	roc_se_cipher_type enc_type = 0; /* NULL Cipher type */
	uint32_t cipher_key_len = 0;

	c_form = &xform->cipher;

	if ((c_form->algo == RTE_CRYPTO_CIPHER_SM4_CBC) ||
	    (c_form->algo == RTE_CRYPTO_CIPHER_SM4_ECB) ||
	    (c_form->algo == RTE_CRYPTO_CIPHER_SM4_CTR) ||
	    (c_form->algo == RTE_CRYPTO_CIPHER_SM4_CFB) ||
	    (c_form->algo == RTE_CRYPTO_CIPHER_SM4_OFB))
		return fill_sm_sess_cipher(xform, sess);

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
		if (sess->aes_ctr_eea2) {
			enc_type = ROC_SE_AES_CTR_EEA2;
		} else {
			enc_type = ROC_SE_AES_CTR;
			aes_ctr = 1;
		}
		cipher_key_len = 16;
		break;
	case RTE_CRYPTO_CIPHER_NULL:
		enc_type = 0;
		is_null = 1;
		break;
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
		if (sess->chained_op)
			return -ENOTSUP;
		if (c_form->iv.length != 8)
			return -EINVAL;
		enc_type = ROC_SE_KASUMI_F8_ECB;
		cipher_key_len = 16;
		zsk_flag = ROC_SE_K_F8;
		zs_cipher = ROC_SE_K_F8;
		break;
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
		enc_type = ROC_SE_SNOW3G_UEA2;
		cipher_key_len = 16;
		zsk_flag = ROC_SE_ZS_EA;
		zs_cipher = ROC_SE_ZS_EA;
		break;
	case RTE_CRYPTO_CIPHER_ZUC_EEA3:
		enc_type = ROC_SE_ZUC_EEA3;
		cipher_key_len = c_form->key.length;
		zsk_flag = ROC_SE_ZS_EA;
		zs_cipher = ROC_SE_ZS_EA;
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
	case RTE_CRYPTO_CIPHER_AES_DOCSISBPI:
		/* Set DOCSIS flag */
		sess->roc_se_ctx.template_w4.s.opcode_minor |= ROC_SE_FC_MINOR_OP_DOCSIS;
		enc_type = ROC_SE_AES_DOCSISBPI;
		cipher_key_len = 16;
		break;
	case RTE_CRYPTO_CIPHER_DES_DOCSISBPI:
		/* Set DOCSIS flag */
		sess->roc_se_ctx.template_w4.s.opcode_minor |= ROC_SE_FC_MINOR_OP_DOCSIS;
		enc_type = ROC_SE_DES_DOCSISBPI;
		cipher_key_len = 8;
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

	if (zsk_flag && sess->roc_se_ctx.ciph_then_auth) {
		struct rte_crypto_auth_xform *a_form;
		a_form = &xform->next->auth;
		if (c_form->op != RTE_CRYPTO_CIPHER_OP_DECRYPT &&
		    a_form->op != RTE_CRYPTO_AUTH_OP_VERIFY) {
			plt_dp_err("Crypto: PDCP cipher then auth must use"
				   " options: decrypt and verify");
			return -EINVAL;
		}
	}

	sess->zsk_flag = zsk_flag;
	sess->zs_cipher = zs_cipher;
	sess->aes_gcm = 0;
	sess->aes_ccm = 0;
	sess->aes_ctr = aes_ctr;
	sess->iv_offset = c_form->iv.offset;
	sess->iv_length = c_form->iv.length;
	sess->is_null = is_null;

	if (aes_ctr)
		switch (sess->iv_length) {
		case 12:
			sess->short_iv = 1;
		case 16:
			break;
		default:
			plt_dp_err("Crypto: Unsupported IV length %u", sess->iv_length);
			return -1;
		}

	if (unlikely(roc_se_ciph_key_set(&sess->roc_se_ctx, enc_type, c_form->key.data,
					 c_form->key.length)))
		return -1;

	if ((enc_type >= ROC_SE_ZUC_EEA3) && (enc_type <= ROC_SE_AES_CTR_EEA2))
		roc_se_ctx_swap(&sess->roc_se_ctx);
	return 0;
}

static __rte_always_inline int
fill_sess_auth(struct rte_crypto_sym_xform *xform, struct cnxk_se_sess *sess)
{
	uint8_t zsk_flag = 0, zs_auth = 0, aes_gcm = 0, is_null = 0, is_sha3 = 0;
	struct rte_crypto_auth_xform *a_form;
	roc_se_auth_type auth_type = 0; /* NULL Auth type */
	uint8_t is_sm3 = 0;

	if (xform->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC)
		return fill_sess_gmac(xform, sess);

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
	case RTE_CRYPTO_AUTH_SHA3_224_HMAC:
	case RTE_CRYPTO_AUTH_SHA3_224:
		is_sha3 = 1;
		auth_type = ROC_SE_SHA3_SHA224;
		break;
	case RTE_CRYPTO_AUTH_SHA3_256_HMAC:
	case RTE_CRYPTO_AUTH_SHA3_256:
		is_sha3 = 1;
		auth_type = ROC_SE_SHA3_SHA256;
		break;
	case RTE_CRYPTO_AUTH_SHA3_384_HMAC:
	case RTE_CRYPTO_AUTH_SHA3_384:
		is_sha3 = 1;
		auth_type = ROC_SE_SHA3_SHA384;
		break;
	case RTE_CRYPTO_AUTH_SHA3_512_HMAC:
	case RTE_CRYPTO_AUTH_SHA3_512:
		is_sha3 = 1;
		auth_type = ROC_SE_SHA3_SHA512;
		break;
	case RTE_CRYPTO_AUTH_SHAKE_128:
		is_sha3 = 1;
		auth_type = ROC_SE_SHA3_SHAKE128;
		break;
	case RTE_CRYPTO_AUTH_SHAKE_256:
		is_sha3 = 1;
		auth_type = ROC_SE_SHA3_SHAKE256;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
	case RTE_CRYPTO_AUTH_MD5:
		auth_type = ROC_SE_MD5_TYPE;
		break;
	case RTE_CRYPTO_AUTH_KASUMI_F9:
		if (sess->chained_op)
			return -ENOTSUP;
		auth_type = ROC_SE_KASUMI_F9_ECB;
		/*
		 * Indicate that direction needs to be taken out
		 * from end of src
		 */
		zsk_flag = ROC_SE_K_F9;
		zs_auth = ROC_SE_K_F9;
		break;
	case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
		auth_type = ROC_SE_SNOW3G_UIA2;
		zsk_flag = ROC_SE_ZS_IA;
		zs_auth = ROC_SE_ZS_IA;
		break;
	case RTE_CRYPTO_AUTH_ZUC_EIA3:
		auth_type = ROC_SE_ZUC_EIA3;
		zsk_flag = ROC_SE_ZS_IA;
		zs_auth = ROC_SE_ZS_IA;
		break;
	case RTE_CRYPTO_AUTH_NULL:
		auth_type = 0;
		is_null = 1;
		break;
	case RTE_CRYPTO_AUTH_AES_CMAC:
		auth_type = ROC_SE_AES_CMAC_EIA2;
		zsk_flag = ROC_SE_ZS_IA;
		break;
	case RTE_CRYPTO_AUTH_SM3:
		auth_type = ROC_SE_SM3;
		is_sm3 = 1;
		break;
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
		plt_dp_err("Crypto: Unsupported hash algo %u", a_form->algo);
		return -1;
	default:
		plt_dp_err("Crypto: Undefined Hash algo %u specified",
			   a_form->algo);
		return -1;
	}

	if (zsk_flag && sess->roc_se_ctx.auth_then_ciph) {
		struct rte_crypto_cipher_xform *c_form;
		if (xform->next != NULL) {
			c_form = &xform->next->cipher;
			if ((c_form != NULL) && (c_form->op != RTE_CRYPTO_CIPHER_OP_ENCRYPT) &&
			    a_form->op != RTE_CRYPTO_AUTH_OP_GENERATE) {
				plt_dp_err("Crypto: PDCP auth then cipher must use"
					   " options: encrypt and generate");
				return -EINVAL;
			}
		}
	}

	sess->zsk_flag = zsk_flag;
	sess->zs_auth = zs_auth;
	sess->aes_gcm = aes_gcm;
	sess->mac_len = a_form->digest_length;
	sess->is_null = is_null;
	sess->is_sha3 = is_sha3;
	sess->is_sm3 = is_sm3;
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

	switch (sess->iv_length) {
	case 12:
		sess->short_iv = 1;
	case 16:
		break;
	default:
		plt_dp_err("Crypto: Unsupported IV length %u", sess->iv_length);
		return -1;
	}

	if (unlikely(roc_se_ciph_key_set(&sess->roc_se_ctx, enc_type, a_form->key.data,
					 a_form->key.length)))
		return -1;

	if (unlikely(roc_se_auth_key_set(&sess->roc_se_ctx, auth_type, NULL, 0,
					 a_form->digest_length)))
		return -1;

	return 0;
}

static __rte_always_inline uint32_t
prepare_iov_from_pkt(struct rte_mbuf *pkt, struct roc_se_iov_ptr *iovec, uint32_t start_offset)
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

static __rte_always_inline void
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
		if (likely(headroom >= CNXK_CPT_MIN_HEADROOM_REQ))
			*flags |= ROC_SE_SINGLE_BUF_HEADROOM;

		param->bufs[0].vaddr = seg_data;
		param->bufs[0].size = seg_size;
		return;
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
	return;
}

static __rte_always_inline int
fill_sm_params(struct rte_crypto_op *cop, struct cnxk_se_sess *sess,
	       struct cpt_qp_meta_info *m_info, struct cpt_inflight_req *infl_req,
	       struct cpt_inst_s *inst, const bool is_sg_ver2)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct roc_se_fc_params fc_params;
	struct rte_mbuf *m_src, *m_dst;
	uint8_t cpt_op = sess->cpt_op;
	uint64_t d_offs, d_lens;
	char src[SRC_IOV_SIZE];
	char dst[SRC_IOV_SIZE];
	void *mdata = NULL;
#ifdef CPT_ALWAYS_USE_SG_MODE
	uint8_t inplace = 0;
#else
	uint8_t inplace = 1;
#endif
	uint32_t flags = 0;
	int ret;

	uint32_t ci_data_length = sym_op->cipher.data.length;
	uint32_t ci_data_offset = sym_op->cipher.data.offset;

	fc_params.cipher_iv_len = sess->iv_length;
	fc_params.auth_iv_len = 0;
	fc_params.auth_iv_buf = NULL;
	fc_params.iv_buf = NULL;
	fc_params.mac_buf.size = 0;
	fc_params.mac_buf.vaddr = 0;

	if (likely(sess->iv_length)) {
		flags |= ROC_SE_VALID_IV_BUF;
		fc_params.iv_buf = rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset);
	}

	m_src = sym_op->m_src;
	m_dst = sym_op->m_dst;

	d_offs = ci_data_offset;
	d_offs = (d_offs << 16);

	d_lens = ci_data_length;
	d_lens = (d_lens << 32);

	fc_params.ctx = &sess->roc_se_ctx;

	if (likely(!m_dst && inplace)) {
		fc_params.dst_iov = fc_params.src_iov = (void *)src;

		prepare_iov_from_pkt_inplace(m_src, &fc_params, &flags);

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
			if (prepare_iov_from_pkt(m_dst, fc_params.dst_iov, 0)) {
				plt_dp_err("Prepare dst iov failed for m_dst %p", m_dst);
				ret = -EINVAL;
				goto err_exit;
			}
		} else {
			fc_params.dst_iov = (void *)src;
		}
	}

	fc_params.meta_buf.vaddr = NULL;

	if (unlikely(!((flags & ROC_SE_SINGLE_BUF_INPLACE) &&
		       (flags & ROC_SE_SINGLE_BUF_HEADROOM)))) {
		mdata = alloc_op_meta(&fc_params.meta_buf, m_info->mlen, m_info->pool, infl_req);
		if (mdata == NULL) {
			plt_dp_err("Error allocating meta buffer for request");
			return -ENOMEM;
		}
	}

	/* Finally prepare the instruction */
	ret = cpt_sm_prep(flags, d_offs, d_lens, &fc_params, inst, is_sg_ver2,
			  !(cpt_op & ROC_SE_OP_ENCODE));

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

static __rte_always_inline int
fill_fc_params(struct rte_crypto_op *cop, struct cnxk_se_sess *sess,
	       struct cpt_qp_meta_info *m_info, struct cpt_inflight_req *infl_req,
	       struct cpt_inst_s *inst, const bool is_kasumi, const bool is_aead,
	       const bool is_sg_ver2)
{
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
	uint8_t ccm_iv_buf[16];
	uint32_t iv_buf[4];
	int ret;

	fc_params.cipher_iv_len = sess->iv_length;
	fc_params.auth_iv_len = 0;
	fc_params.auth_iv_buf = NULL;
	fc_params.iv_buf = NULL;
	fc_params.mac_buf.size = 0;
	fc_params.mac_buf.vaddr = 0;

	if (likely(is_kasumi || sess->iv_length)) {
		flags |= ROC_SE_VALID_IV_BUF;
		fc_params.iv_buf = rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset);
		if (sess->short_iv) {
			memcpy((uint8_t *)iv_buf,
			       rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset), 12);
			iv_buf[3] = rte_cpu_to_be_32(0x1);
			fc_params.iv_buf = iv_buf;
		}
		if (sess->aes_ccm) {
			memcpy((uint8_t *)ccm_iv_buf,
			       rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset),
			       sess->iv_length + 1);
			ccm_iv_buf[0] = 14 - sess->iv_length;
			fc_params.iv_buf = ccm_iv_buf;
		}
	}

	/* Kasumi would need SG mode */
	if (is_kasumi)
		inplace = 0;

	m_src = sym_op->m_src;
	m_dst = sym_op->m_dst;

	if (is_aead) {
		struct rte_mbuf *m;
		uint8_t *aad_data;
		uint16_t aad_len;

		d_offs = sym_op->aead.data.offset;
		d_lens = sym_op->aead.data.length;
		mc_hash_off =
			sym_op->aead.data.offset + sym_op->aead.data.length;

		aad_data = sym_op->aead.aad.data;
		aad_len = sess->aad_length;
		if (likely((aad_len == 0) ||
			   ((aad_data + aad_len) ==
			    rte_pktmbuf_mtod_offset(m_src, uint8_t *, sym_op->aead.data.offset)))) {
			d_offs = (d_offs - aad_len) | (d_offs << 16);
			d_lens = (d_lens + aad_len) | (d_lens << 32);
		} else {
			/* For AES CCM, AAD is written 18B after aad.data as per API */
			if (sess->aes_ccm)
				fc_params.aad_buf.vaddr = PLT_PTR_ADD(sym_op->aead.aad.data, 18);
			else
				fc_params.aad_buf.vaddr = sym_op->aead.aad.data;
			fc_params.aad_buf.size = aad_len;
			flags |= ROC_SE_VALID_AAD_BUF;
			inplace = 0;
			d_offs = d_offs << 16;
			d_lens = d_lens << 32;
		}

		m = cpt_m_dst_get(cpt_op, m_src, m_dst);

		/* Digest immediately following data is best case */
		if (unlikely(rte_pktmbuf_mtod(m, uint8_t *) + mc_hash_off !=
			     (uint8_t *)sym_op->aead.digest.data)) {
			flags |= ROC_SE_VALID_MAC_BUF;
			fc_params.mac_buf.size = sess->mac_len;
			fc_params.mac_buf.vaddr = sym_op->aead.digest.data;
			inplace = 0;
		}
	} else {
		uint32_t ci_data_length = sym_op->cipher.data.length;
		uint32_t ci_data_offset = sym_op->cipher.data.offset;
		uint32_t a_data_length = sym_op->auth.data.length;
		uint32_t a_data_offset = sym_op->auth.data.offset;
		struct roc_se_ctx *ctx = &sess->roc_se_ctx;

		const uint8_t op_minor = ctx->template_w4.s.opcode_minor;

		d_offs = ci_data_offset;
		d_offs = (d_offs << 16) | a_data_offset;

		d_lens = ci_data_length;
		d_lens = (d_lens << 32) | a_data_length;

		if (likely(sess->mac_len)) {
			struct rte_mbuf *m = cpt_m_dst_get(cpt_op, m_src, m_dst);

			if (sess->auth_first)
				mc_hash_off = a_data_offset + a_data_length;
			else
				mc_hash_off = ci_data_offset + ci_data_length;

			if (mc_hash_off < (a_data_offset + a_data_length))
				mc_hash_off = (a_data_offset + a_data_length);

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
	fc_params.ctx = &sess->roc_se_ctx;

	if (!(sess->auth_first) && unlikely(sess->is_null || sess->cpt_op == ROC_SE_OP_DECODE))
		inplace = 0;

	if (likely(!m_dst && inplace)) {
		/* Case of single buffer without AAD buf or
		 * separate mac buf in place and
		 * not air crypto
		 */
		fc_params.dst_iov = fc_params.src_iov = (void *)src;

		prepare_iov_from_pkt_inplace(m_src, &fc_params, &flags);

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

	fc_params.meta_buf.vaddr = NULL;
	if (unlikely(is_kasumi || !((flags & ROC_SE_SINGLE_BUF_INPLACE) &&
				    (flags & ROC_SE_SINGLE_BUF_HEADROOM)))) {
		mdata = alloc_op_meta(&fc_params.meta_buf, m_info->mlen, m_info->pool, infl_req);
		if (mdata == NULL) {
			plt_dp_err("Error allocating meta buffer for request");
			return -ENOMEM;
		}
	}

	/* Finally prepare the instruction */

	if (is_kasumi) {
		if (cpt_op & ROC_SE_OP_ENCODE)
			ret = cpt_kasumi_enc_prep(flags, d_offs, d_lens, &fc_params, inst,
						  is_sg_ver2);
		else
			ret = cpt_kasumi_dec_prep(d_offs, d_lens, &fc_params, inst, is_sg_ver2);
	} else {
		if (cpt_op & ROC_SE_OP_ENCODE)
			ret = cpt_enc_hmac_prep(flags, d_offs, d_lens, &fc_params, inst,
						is_sg_ver2);
		else
			ret = cpt_dec_hmac_prep(flags, d_offs, d_lens, &fc_params, inst,
						is_sg_ver2);
	}

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

static inline int
fill_passthrough_params(struct rte_crypto_op *cop, struct cpt_inst_s *inst)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct rte_mbuf *m_src, *m_dst;

	const union cpt_inst_w4 w4 = {
		.s.opcode_major = ROC_SE_MAJOR_OP_MISC,
		.s.opcode_minor = ROC_SE_MISC_MINOR_OP_PASSTHROUGH,
		.s.param1 = 1,
		.s.param2 = 1,
		.s.dlen = 0,
	};

	m_src = sym_op->m_src;
	m_dst = sym_op->m_dst;

	if (unlikely(m_dst != NULL && m_dst != m_src)) {
		void *src = rte_pktmbuf_mtod_offset(m_src, void *, cop->sym->cipher.data.offset);
		void *dst = rte_pktmbuf_mtod(m_dst, void *);
		int data_len = cop->sym->cipher.data.length;

		rte_memcpy(dst, src, data_len);
	}

	inst->w0.u64 = 0;
	inst->w5.u64 = 0;
	inst->w4.u64 = w4.u64;

	return 0;
}

static __rte_always_inline int
fill_pdcp_params(struct rte_crypto_op *cop, struct cnxk_se_sess *sess,
		 struct cpt_qp_meta_info *m_info, struct cpt_inflight_req *infl_req,
		 struct cpt_inst_s *inst, const bool is_sg_ver2)
{
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct roc_se_fc_params fc_params;
	uint32_t c_data_len, c_data_off;
	struct rte_mbuf *m_src, *m_dst;
	uint64_t d_offs, d_lens;
	char src[SRC_IOV_SIZE];
	char dst[SRC_IOV_SIZE];
	void *mdata = NULL;
	uint32_t flags = 0;
	int ret;

	/* Cipher only */

	fc_params.cipher_iv_len = sess->iv_length;
	fc_params.auth_iv_len = 0;
	fc_params.iv_buf = NULL;
	fc_params.auth_iv_buf = NULL;

	if (likely(sess->iv_length))
		fc_params.iv_buf = rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset);

	m_src = sym_op->m_src;
	m_dst = sym_op->m_dst;

	c_data_len = sym_op->cipher.data.length;
	c_data_off = sym_op->cipher.data.offset;

	d_offs = (uint64_t)c_data_off << 16;
	d_lens = (uint64_t)c_data_len << 32;

	fc_params.ctx = &sess->roc_se_ctx;

	if (likely(m_dst == NULL || m_src == m_dst)) {
		fc_params.dst_iov = fc_params.src_iov = (void *)src;
		prepare_iov_from_pkt_inplace(m_src, &fc_params, &flags);
	} else {
		/* Out of place processing */

		fc_params.src_iov = (void *)src;
		fc_params.dst_iov = (void *)dst;

		/* Store SG I/O in the api for reuse */
		if (unlikely(prepare_iov_from_pkt(m_src, fc_params.src_iov, 0))) {
			plt_dp_err("Prepare src iov failed");
			ret = -EINVAL;
			goto err_exit;
		}

		if (unlikely(prepare_iov_from_pkt(m_dst, fc_params.dst_iov, 0))) {
			plt_dp_err("Prepare dst iov failed for m_dst %p", m_dst);
			ret = -EINVAL;
			goto err_exit;
		}
	}

	fc_params.meta_buf.vaddr = NULL;
	if (unlikely(!((flags & ROC_SE_SINGLE_BUF_INPLACE) &&
		       (flags & ROC_SE_SINGLE_BUF_HEADROOM)))) {
		mdata = alloc_op_meta(&fc_params.meta_buf, m_info->mlen, m_info->pool, infl_req);
		if (mdata == NULL) {
			plt_dp_err("Could not allocate meta buffer");
			ret = -ENOMEM;
			goto err_exit;
		}
	}

	ret = cpt_pdcp_alg_prep(flags, d_offs, d_lens, &fc_params, inst, is_sg_ver2);
	if (unlikely(ret)) {
		plt_dp_err("Could not prepare instruction");
		goto free_mdata_and_exit;
	}

	return 0;

free_mdata_and_exit:
	if (infl_req->op_flags & CPT_OP_FLAGS_METABUF)
		rte_mempool_put(m_info->pool, infl_req->mdata);
err_exit:
	return ret;
}

static __rte_always_inline int
fill_pdcp_chain_params(struct rte_crypto_op *cop, struct cnxk_se_sess *sess,
		       struct cpt_qp_meta_info *m_info, struct cpt_inflight_req *infl_req,
		       struct cpt_inst_s *inst, const bool is_sg_ver2)
{
	uint32_t ci_data_length, ci_data_offset, a_data_length, a_data_offset;
	struct rte_crypto_sym_op *sym_op = cop->sym;
	struct roc_se_fc_params fc_params;
	struct rte_mbuf *m_src, *m_dst;
	uint8_t cpt_op = sess->cpt_op;
	uint64_t d_offs, d_lens;
	char src[SRC_IOV_SIZE];
	char dst[SRC_IOV_SIZE];
	bool inplace = true;
	uint32_t flags = 0;
	void *mdata;
	int ret;

	fc_params.cipher_iv_len = sess->iv_length;
	fc_params.auth_iv_len = sess->auth_iv_length;
	fc_params.iv_buf = NULL;
	fc_params.auth_iv_buf = NULL;

	m_src = sym_op->m_src;
	m_dst = sym_op->m_dst;

	if (likely(sess->iv_length))
		fc_params.iv_buf = rte_crypto_op_ctod_offset(cop, uint8_t *, sess->iv_offset);

	ci_data_length = sym_op->cipher.data.length;
	ci_data_offset = sym_op->cipher.data.offset;
	a_data_length = sym_op->auth.data.length;
	a_data_offset = sym_op->auth.data.offset;

	/*
	 * For ZUC & SNOW, length & offset is provided in bits. Convert to
	 * bytes.
	 */

	if (sess->zs_cipher) {
		ci_data_length /= 8;
		ci_data_offset /= 8;
	}

	if (sess->zs_auth) {
		a_data_length /= 8;
		a_data_offset /= 8;
		/*
		 * ZUC & SNOW would have valid iv_buf. AES-CMAC doesn't require
		 * IV from application.
		 */
		fc_params.auth_iv_buf =
			rte_crypto_op_ctod_offset(cop, uint8_t *, sess->auth_iv_offset);
#ifdef CNXK_CRYPTODEV_DEBUG
		if (sess->auth_iv_length == 0)
			plt_err("Invalid auth IV length");
#endif
	}

	d_offs = ci_data_offset;
	d_offs = (d_offs << 16) | a_data_offset;
	d_lens = ci_data_length;
	d_lens = (d_lens << 32) | a_data_length;

	if (likely(sess->mac_len)) {
		struct rte_mbuf *m = cpt_m_dst_get(cpt_op, m_src, m_dst);

		cpt_digest_buf_lb_check(sess, m, &fc_params, &flags, sym_op, &inplace,
					a_data_offset, a_data_length, ci_data_offset,
					ci_data_length, true);
	}

	fc_params.ctx = &sess->roc_se_ctx;

	if (likely((m_dst == NULL || m_dst == m_src)) && inplace) {
		fc_params.dst_iov = fc_params.src_iov = (void *)src;
		prepare_iov_from_pkt_inplace(m_src, &fc_params, &flags);
	} else {
		/* Out of place processing */
		fc_params.src_iov = (void *)src;
		fc_params.dst_iov = (void *)dst;

		/* Store SG I/O in the api for reuse */
		if (unlikely(prepare_iov_from_pkt(m_src, fc_params.src_iov, 0))) {
			plt_dp_err("Could not prepare src iov");
			ret = -EINVAL;
			goto err_exit;
		}

		if (unlikely(m_dst != NULL)) {
			if (unlikely(prepare_iov_from_pkt(m_dst, fc_params.dst_iov, 0))) {
				plt_dp_err("Could not prepare m_dst iov %p", m_dst);
				ret = -EINVAL;
				goto err_exit;
			}
		} else {
			fc_params.dst_iov = (void *)src;
		}
	}

	if (unlikely(!((flags & ROC_SE_SINGLE_BUF_INPLACE) &&
		       (flags & ROC_SE_SINGLE_BUF_HEADROOM)))) {
		mdata = alloc_op_meta(&fc_params.meta_buf, m_info->mlen, m_info->pool, infl_req);
		if (unlikely(mdata == NULL)) {
			plt_dp_err("Could not allocate meta buffer for request");
			return -ENOMEM;
		}
	}

	/* Finally prepare the instruction */
	ret = cpt_pdcp_chain_alg_prep(flags, d_offs, d_lens, &fc_params, inst, is_sg_ver2);
	if (unlikely(ret)) {
		plt_dp_err("Could not prepare instruction");
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
		   struct cpt_qp_meta_info *m_info, struct cpt_inflight_req *infl_req,
		   struct cpt_inst_s *inst, const bool is_sg_ver2)
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
		params.auth_iv_len = sess->auth_iv_length;
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
			params.iv_buf = iv_buf;
		}
	}

	d_lens = sym_op->auth.data.length;

	params.ctx = &sess->roc_se_ctx;

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

	ret = cpt_fc_enc_hmac_prep(flags, d_offs, d_lens, &params, inst, is_sg_ver2);
	if (ret)
		goto free_mdata_and_exit;

	return 0;

free_mdata_and_exit:
	if (infl_req->op_flags & CPT_OP_FLAGS_METABUF)
		rte_mempool_put(m_info->pool, infl_req->mdata);
err_exit:
	return ret;
}

static __rte_always_inline int __rte_hot
cpt_sym_inst_fill(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op, struct cnxk_se_sess *sess,
		  struct cpt_inflight_req *infl_req, struct cpt_inst_s *inst, const bool is_sg_ver2)
{
	int ret;

	switch (sess->dp_thr_type) {
	case CPT_DP_THREAD_TYPE_PT:
		ret = fill_passthrough_params(op, inst);
		break;
	case CPT_DP_THREAD_TYPE_PDCP:
		ret = fill_pdcp_params(op, sess, &qp->meta_info, infl_req, inst, is_sg_ver2);
		break;
	case CPT_DP_THREAD_TYPE_FC_CHAIN:
		ret = fill_fc_params(op, sess, &qp->meta_info, infl_req, inst, false, false,
				     is_sg_ver2);
		break;
	case CPT_DP_THREAD_TYPE_FC_AEAD:
		ret = fill_fc_params(op, sess, &qp->meta_info, infl_req, inst, false, true,
				     is_sg_ver2);
		break;
	case CPT_DP_THREAD_TYPE_PDCP_CHAIN:
		ret = fill_pdcp_chain_params(op, sess, &qp->meta_info, infl_req, inst, is_sg_ver2);
		break;
	case CPT_DP_THREAD_TYPE_KASUMI:
		ret = fill_fc_params(op, sess, &qp->meta_info, infl_req, inst, true, false,
				     is_sg_ver2);
		break;
	case CPT_DP_THREAD_TYPE_SM:
		ret = fill_sm_params(op, sess, &qp->meta_info, infl_req, inst, is_sg_ver2);
		break;

	case CPT_DP_THREAD_AUTH_ONLY:
		ret = fill_digest_params(op, sess, &qp->meta_info, infl_req, inst, is_sg_ver2);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

#endif /*_CNXK_SE_H_ */
