/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include "qat_sym_session.h"
#include "qat_sym.h"
#include "qat_asym.h"
#include "qat_crypto.h"
#include "qat_crypto_pmd_gens.h"

static struct rte_cryptodev_capabilities qat_sym_crypto_caps_gen_lce[] = {
	QAT_SYM_AEAD_CAP(AES_GCM,
		CAP_SET(block_size, 16),
		CAP_RNG(key_size, 32, 32, 0), CAP_RNG(digest_size, 16, 16, 0),
		CAP_RNG(aad_size, 0, 240, 1), CAP_RNG(iv_size, 12, 12, 0)),
	RTE_CRYPTODEV_END_OF_CAPABILITIES_LIST()
};

static int
qat_sgl_add_buffer_gen_lce(void *list_in, uint64_t addr, uint32_t len)
{
	struct qat_sgl *list = (struct qat_sgl *)list_in;
	uint32_t nr;

	nr = list->num_bufs;

	if (nr >= QAT_SYM_SGL_MAX_NUMBER) {
		QAT_DP_LOG(ERR, "Adding %d entry failed, no empty SGL buffer", nr);
		return -EINVAL;
	}

	list->buffers[nr].len = len;
	list->buffers[nr].resrvd = 0;
	list->buffers[nr].addr = addr;

	list->num_bufs++;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_LOG(INFO, "SGL with %d buffers:", list->num_bufs);
	QAT_DP_LOG(INFO, "QAT SGL buf %d, len = %d, iova = 0x%012"PRIx64,
		nr, list->buffers[nr].len, list->buffers[nr].addr);
#endif
	return 0;
}

static int
qat_sgl_fill_array_with_mbuf(struct rte_mbuf *buf, int64_t offset,
		void *list_in, uint32_t data_len)
{
	struct qat_sgl *list = (struct qat_sgl *)list_in;
	uint32_t nr, buf_len;
	int res = -EINVAL;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	uint32_t start_idx = list->num_bufs;
#endif

	/* Append to the existing list */
	nr = list->num_bufs;

	for (buf_len = 0; buf && nr < QAT_SYM_SGL_MAX_NUMBER; buf = buf->next) {
		if (offset >= rte_pktmbuf_data_len(buf)) {
			offset -= rte_pktmbuf_data_len(buf);
			/* Jump to next mbuf */
			continue;
		}

		list->buffers[nr].len = rte_pktmbuf_data_len(buf) - offset;
		list->buffers[nr].resrvd = 0;
		list->buffers[nr].addr = rte_pktmbuf_iova_offset(buf, offset);

		offset = 0;
		buf_len += list->buffers[nr].len;

		if (buf_len >= data_len) {
			list->buffers[nr].len -= buf_len - data_len;
			res = 0;
			break;
		}
		++nr;
	}

	if (unlikely(res != 0)) {
		if (nr == QAT_SYM_SGL_MAX_NUMBER)
			QAT_DP_LOG(ERR, "Exceeded max segments in QAT SGL (%u)",
					QAT_SYM_SGL_MAX_NUMBER);
		else
			QAT_DP_LOG(ERR, "Mbuf chain is too short");
	} else {

		list->num_bufs = ++nr;
#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
		QAT_DP_LOG(INFO, "SGL with %d buffers:", list->num_bufs);
		for (nr = start_idx; nr < list->num_bufs; nr++) {
			QAT_DP_LOG(INFO, "QAT SGL buf %d, len = %d, iova = 0x%012"PRIx64,
					nr, list->buffers[nr].len,
					list->buffers[nr].addr);
		}
#endif
	}

	return res;
}

static int
qat_sym_build_op_aead_gen_lce(void *in_op, struct qat_sym_session *ctx,
	uint8_t *out_msg, void *op_cookie)
{
	struct qat_sym_op_cookie *cookie = op_cookie;
	struct rte_crypto_op *op = in_op;
	uint64_t digest_phys_addr, aad_phys_addr;
	uint16_t iv_len, aad_len, digest_len, key_len;
	uint32_t cipher_ofs, iv_offset, cipher_len;
	register struct icp_qat_fw_la_bulk_req *qat_req;
	struct icp_qat_fw_la_cipher_30_req_params *cipher_param;
	enum icp_qat_hw_cipher_dir dir;
	bool is_digest_adjacent = false;

	if (ctx->qat_cmd != ICP_QAT_FW_LA_CMD_CIPHER ||
		ctx->qat_cipher_alg != ICP_QAT_HW_CIPHER_ALGO_AES256 ||
		ctx->qat_mode != ICP_QAT_HW_CIPHER_AEAD_MODE) {

		QAT_DP_LOG(ERR, "Not supported (cmd: %d, alg: %d, mode: %d). "
			"GEN_LCE PMD only supports AES-256 AEAD mode",
			ctx->qat_cmd, ctx->qat_cipher_alg, ctx->qat_mode);
		return -EINVAL;
	}

	qat_req = (struct icp_qat_fw_la_bulk_req *)out_msg;
	rte_mov128((uint8_t *)qat_req, (const uint8_t *)&(ctx->fw_req));
	qat_req->comn_mid.opaque_data = (uint64_t)(uintptr_t)op;
	cipher_param = (void *)&qat_req->serv_specif_rqpars;

	dir = ctx->qat_dir;

	aad_phys_addr = op->sym->aead.aad.phys_addr;
	aad_len = ctx->aad_len;

	iv_offset = ctx->cipher_iv.offset;
	iv_len = ctx->cipher_iv.length;

	cipher_ofs = op->sym->aead.data.offset;
	cipher_len = op->sym->aead.data.length;

	digest_phys_addr = op->sym->aead.digest.phys_addr;
	digest_len = ctx->digest_length;

	/* Up to 16B IV can be directly embedded in descriptor.
	 *  GCM supports only 12B IV for GEN LCE
	 */
	if (iv_len != GCM_IV_LENGTH_GEN_LCE) {
		QAT_DP_LOG(ERR, "iv_len: %d not supported. Must be 12B.", iv_len);
		return -EINVAL;
	}

	rte_memcpy(cipher_param->u.cipher_IV_array,
			rte_crypto_op_ctod_offset(op, uint8_t*, iv_offset), iv_len);

	/* Always SGL */
	RTE_ASSERT((qat_req->comn_hdr.comn_req_flags & ICP_QAT_FW_SYM_COMM_ADDR_SGL) == 1);
	/* Always inplace */
	RTE_ASSERT(op->sym->m_dst == NULL);

	/* Key buffer address is already programmed by reusing the
	 * content-descriptor buffer
	 */
	key_len = ctx->auth_key_length;

	cipher_param->spc_aad_sz = aad_len;
	cipher_param->cipher_length = key_len;
	cipher_param->spc_auth_res_sz = digest_len;

	/* Knowing digest is contiguous to cipher-text helps optimizing SGL */
	if (rte_pktmbuf_iova_offset(op->sym->m_src, cipher_ofs + cipher_len) == digest_phys_addr)
		is_digest_adjacent = true;

	/* SRC-SGL: 3 entries:
	 * a) AAD
	 * b) cipher
	 * c) digest (only for decrypt and buffer is_NOT_adjacent)
	 *
	 */
	cookie->qat_sgl_src.num_bufs = 0;
	if (aad_len)
		qat_sgl_add_buffer_gen_lce(&cookie->qat_sgl_src, aad_phys_addr, aad_len);

	if (is_digest_adjacent && dir == ICP_QAT_HW_CIPHER_DECRYPT) {
		qat_sgl_fill_array_with_mbuf(op->sym->m_src, cipher_ofs, &cookie->qat_sgl_src,
				cipher_len + digest_len);
	} else {
		qat_sgl_fill_array_with_mbuf(op->sym->m_src, cipher_ofs, &cookie->qat_sgl_src,
				cipher_len);

		/* Digest buffer in decrypt job */
		if (dir == ICP_QAT_HW_CIPHER_DECRYPT)
			qat_sgl_add_buffer_gen_lce(&cookie->qat_sgl_src,
					digest_phys_addr, digest_len);
	}

	/* (in-place) DST-SGL: 2 entries:
	 * a) cipher
	 * b) digest (only for encrypt and buffer is_NOT_adjacent)
	 */
	cookie->qat_sgl_dst.num_bufs = 0;

	if (is_digest_adjacent && dir == ICP_QAT_HW_CIPHER_ENCRYPT) {
		qat_sgl_fill_array_with_mbuf(op->sym->m_src, cipher_ofs, &cookie->qat_sgl_dst,
				cipher_len + digest_len);
	} else {
		qat_sgl_fill_array_with_mbuf(op->sym->m_src, cipher_ofs, &cookie->qat_sgl_dst,
				cipher_len);

		/* Digest buffer in Encrypt job */
		if (dir == ICP_QAT_HW_CIPHER_ENCRYPT)
			qat_sgl_add_buffer_gen_lce(&cookie->qat_sgl_dst,
					digest_phys_addr, digest_len);
	}

	/* Length values in 128B descriptor */
	qat_req->comn_mid.src_length = cipher_len;
	qat_req->comn_mid.dst_length = cipher_len;

	if (dir == ICP_QAT_HW_CIPHER_ENCRYPT) /* Digest buffer in Encrypt job */
		qat_req->comn_mid.dst_length += GCM_256_DIGEST_LEN;

	/* src & dst SGL addresses in 128B descriptor */
	qat_req->comn_mid.src_data_addr = cookie->qat_sgl_src_phys_addr;
	qat_req->comn_mid.dest_data_addr = cookie->qat_sgl_dst_phys_addr;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_HEXDUMP_LOG(DEBUG, "qat_req:", qat_req, sizeof(struct icp_qat_fw_la_bulk_req));
	QAT_DP_HEXDUMP_LOG(DEBUG, "src_data:", rte_pktmbuf_mtod(op->sym->m_src, uint8_t*),
			rte_pktmbuf_data_len(op->sym->m_src));
	QAT_DP_HEXDUMP_LOG(DEBUG, "digest:", op->sym->aead.digest.data, digest_len);
	QAT_DP_HEXDUMP_LOG(DEBUG, "aad:", op->sym->aead.aad.data, aad_len);
#endif
	return 0;
}

static int
qat_sym_crypto_set_session_gen_lce(void *cdev __rte_unused, void *session)
{
	struct qat_sym_session *ctx = session;
	qat_sym_build_request_t build_request = NULL;
	enum rte_proc_type_t proc_type = rte_eal_process_type();

	if (proc_type == RTE_PROC_AUTO || proc_type == RTE_PROC_INVALID)
		return -EINVAL;

	/* build request for aead */
	if (ctx->qat_cipher_alg == ICP_QAT_HW_CIPHER_ALGO_AES256 &&
			ctx->qat_hash_alg == ICP_QAT_HW_AUTH_ALGO_GALOIS_128) {
		build_request = qat_sym_build_op_aead_gen_lce;
		ctx->build_request[proc_type] = build_request;
	}
	return 0;
}


static int
qat_sym_crypto_cap_get_gen_lce(struct qat_cryptodev_private *internals,
	const char *capa_memz_name,
	const uint16_t __rte_unused slice_map)
{
	const uint32_t size = sizeof(qat_sym_crypto_caps_gen_lce);
	uint32_t i;

	internals->capa_mz = rte_memzone_lookup(capa_memz_name);
	if (internals->capa_mz == NULL) {
		internals->capa_mz = rte_memzone_reserve(capa_memz_name, size, rte_socket_id(), 0);
		if (internals->capa_mz == NULL) {
			QAT_LOG(DEBUG, "Error allocating memzone for capabilities");
			return -1;
		}
	}

	struct rte_cryptodev_capabilities *addr =
		(struct rte_cryptodev_capabilities *)
		internals->capa_mz->addr;
	const struct rte_cryptodev_capabilities *capabilities =
		qat_sym_crypto_caps_gen_lce;
	const uint32_t capa_num = size / sizeof(struct rte_cryptodev_capabilities);
	uint32_t curr_capa = 0;

	for (i = 0; i < capa_num; i++) {
		memcpy(addr + curr_capa, capabilities + i,
				sizeof(struct rte_cryptodev_capabilities));
		curr_capa++;
	}
	internals->qat_dev_capabilities = internals->capa_mz->addr;

	return 0;
}

RTE_INIT(qat_sym_crypto_gen_lce_init)
{
	qat_sym_gen_dev_ops[QAT_GEN_LCE].cryptodev_ops = &qat_sym_crypto_ops_gen1;
	qat_sym_gen_dev_ops[QAT_GEN_LCE].get_capabilities = qat_sym_crypto_cap_get_gen_lce;
	qat_sym_gen_dev_ops[QAT_GEN_LCE].set_session = qat_sym_crypto_set_session_gen_lce;
	qat_sym_gen_dev_ops[QAT_GEN_LCE].set_raw_dp_ctx = NULL;
	qat_sym_gen_dev_ops[QAT_GEN_LCE].get_feature_flags = qat_sym_crypto_feature_flags_get_gen1;
}

RTE_INIT(qat_asym_crypto_gen_lce_init)
{
	qat_asym_gen_dev_ops[QAT_GEN_LCE].cryptodev_ops = NULL;
	qat_asym_gen_dev_ops[QAT_GEN_LCE].get_capabilities = NULL;
	qat_asym_gen_dev_ops[QAT_GEN_LCE].get_feature_flags = NULL;
	qat_asym_gen_dev_ops[QAT_GEN_LCE].set_session = NULL;
}
