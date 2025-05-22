/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 ZTE Corporation
 */

#include "zsda_crypto.h"

#define choose_dst_mbuf(mbuf_src, mbuf_dst) ((mbuf_dst) == NULL ? (mbuf_src) : (mbuf_dst))
#define LBADS_MAX_REMAINDER (16 - 1)

int
zsda_encry_match(const void *op_in)
{
	const struct rte_crypto_op *op = op_in;
	struct rte_cryptodev_sym_session *session = op->sym->session;
	struct zsda_sym_session *sess =
		(struct zsda_sym_session *)session->driver_priv_data;

	if (sess->chain_order == ZSDA_SYM_CHAIN_ONLY_CIPHER &&
	    sess->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		return 1;
	else
		return 0;
}

int
zsda_decry_match(const void *op_in)
{
	const struct rte_crypto_op *op = op_in;
	struct rte_cryptodev_sym_session *session = op->sym->session;
	struct zsda_sym_session *sess =
		(struct zsda_sym_session *)session->driver_priv_data;

	if (sess->chain_order == ZSDA_SYM_CHAIN_ONLY_CIPHER &&
	    sess->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT)
		return 1;
	else
		return 0;
}

int
zsda_hash_match(const void *op_in)
{
	const struct rte_crypto_op *op = op_in;
	struct rte_cryptodev_sym_session *session = op->sym->session;
	struct zsda_sym_session *sess =
		(struct zsda_sym_session *)session->driver_priv_data;

	if (sess->chain_order == ZSDA_SYM_CHAIN_ONLY_AUTH)
		return 1;
	else
		return 0;
}

static uint8_t
zsda_opcode_hash_get(struct zsda_sym_session *sess)
{
	switch (sess->auth.algo) {
	case RTE_CRYPTO_AUTH_SHA1:
		return ZSDA_OPC_HASH_SHA1;

	case RTE_CRYPTO_AUTH_SHA224:
		return ZSDA_OPC_HASH_SHA2_224;

	case RTE_CRYPTO_AUTH_SHA256:
		return ZSDA_OPC_HASH_SHA2_256;

	case RTE_CRYPTO_AUTH_SHA384:
		return ZSDA_OPC_HASH_SHA2_384;

	case RTE_CRYPTO_AUTH_SHA512:
		return ZSDA_OPC_HASH_SHA2_512;

	case RTE_CRYPTO_AUTH_SM3:
		return ZSDA_OPC_HASH_SM3;
	default:
		break;
	}

	return ZSDA_OPC_INVALID;
}

static uint8_t
zsda_opcode_crypto_get(struct zsda_sym_session *sess)
{
	if (sess->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		if (sess->cipher.algo == RTE_CRYPTO_CIPHER_AES_XTS &&
		    sess->cipher.key_encry.length == 32)
			return ZSDA_OPC_EC_AES_XTS_256;
		else if (sess->cipher.algo == RTE_CRYPTO_CIPHER_AES_XTS &&
			 sess->cipher.key_encry.length == 64)
			return ZSDA_OPC_EC_AES_XTS_512;
		else if (sess->cipher.algo == RTE_CRYPTO_CIPHER_SM4_XTS)
			return ZSDA_OPC_EC_SM4_XTS_256;
	} else if (sess->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
		if (sess->cipher.algo == RTE_CRYPTO_CIPHER_AES_XTS &&
		    sess->cipher.key_decry.length == 32)
			return ZSDA_OPC_DC_AES_XTS_256;
		else if (sess->cipher.algo == RTE_CRYPTO_CIPHER_AES_XTS &&
			 sess->cipher.key_decry.length == 64)
			return ZSDA_OPC_DC_AES_XTS_512;
		else if (sess->cipher.algo == RTE_CRYPTO_CIPHER_SM4_XTS)
			return ZSDA_OPC_DC_SM4_XTS_256;
	}
	return ZSDA_OPC_INVALID;
}

static int
zsda_len_lbads_chk(uint32_t data_len, uint32_t lbads_size)
{
	if (data_len < 16) {
		ZSDA_LOG(ERR, "data_len wrong data_len 0x%x!", data_len);
		return ZSDA_FAILED;
	}
	if (lbads_size != 0) {
		if (!(((data_len % lbads_size) == 0) ||
		      ((data_len % lbads_size) > LBADS_MAX_REMAINDER))) {
			ZSDA_LOG(ERR, "data_len wrong! data_len 0x%x - %d",
				data_len, data_len % lbads_size);
			return ZSDA_FAILED;
		}
	}

	return ZSDA_SUCCESS;
}

int
zsda_crypto_wqe_build(void *op_in, const struct zsda_queue *queue,
			 void **op_cookies, const uint16_t new_tail)
{
	struct rte_crypto_op *op = op_in;

	struct rte_cryptodev_sym_session *session = op->sym->session;
	struct zsda_sym_session *sess =
		(struct zsda_sym_session *)session->driver_priv_data;

	struct zsda_wqe_crpt *wqe =
		(struct zsda_wqe_crpt *)(queue->base_addr +
					 (new_tail * queue->msg_size));
	struct zsda_op_cookie *cookie = op_cookies[new_tail];
	struct zsda_sgl *sgl_src = (struct zsda_sgl *)&cookie->sgl_src;
	struct zsda_sgl *sgl_dst = (struct zsda_sgl *)&cookie->sgl_dst;
	struct rte_mbuf *mbuf;

	int ret;
	uint32_t op_offset;
	uint32_t op_src_len;
	uint32_t op_dst_len;
	const uint8_t *iv_addr = NULL;
	uint8_t iv_len = 0;

	ret = zsda_len_lbads_chk(op->sym->cipher.data.length,
				   sess->cipher.dataunit_len);
	if (ret) {
		ZSDA_LOG(ERR, "data_len 0x%x", op->sym->cipher.data.length);
		return ZSDA_FAILED;
	}

	op_offset = op->sym->cipher.data.offset;
	op_src_len = op->sym->cipher.data.length;
	mbuf = op->sym->m_src;
	ret = zsda_sgl_fill(mbuf, op_offset, sgl_src, cookie->sgl_src_phys_addr,
			    op_src_len, NULL);

	mbuf = choose_dst_mbuf(op->sym->m_src, op->sym->m_dst);
	op_dst_len = mbuf->pkt_len - op_offset;
	ret |= zsda_sgl_fill(mbuf, op_offset, sgl_dst,
			     cookie->sgl_dst_phys_addr, op_dst_len, NULL);

	if (ret) {
		ZSDA_LOG(ERR, "Failed! zsda_sgl_fill");
		return ret;
	}

	cookie->used = true;
	cookie->sid = new_tail;
	cookie->op = op;

	memset(wqe, 0, sizeof(struct zsda_wqe_crpt));
	wqe->rx_length = op_src_len;
	wqe->tx_length = op_dst_len;
	wqe->valid = queue->valid;
	wqe->op_code = zsda_opcode_crypto_get(sess);
	wqe->sid = cookie->sid;
	wqe->rx_sgl_type = WQE_ELM_TYPE_LIST;
	wqe->tx_sgl_type = WQE_ELM_TYPE_LIST;
	wqe->rx_addr = cookie->sgl_src_phys_addr;
	wqe->tx_addr = cookie->sgl_dst_phys_addr;
	wqe->cfg.lbads = sess->cipher.lbads;

	if (sess->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		memcpy((uint8_t *)wqe->cfg.key, sess->cipher.key_encry.data,
		       ZSDA_CIPHER_KEY_MAX_LEN);
	else
		memcpy((uint8_t *)wqe->cfg.key, sess->cipher.key_decry.data,
		       ZSDA_CIPHER_KEY_MAX_LEN);

	iv_addr = (const uint8_t *)rte_crypto_op_ctod_offset(
			       op, char *, sess->cipher.iv.offset);
	iv_len = sess->cipher.iv.length;
	zsda_reverse_memcpy((uint8_t *)wqe->cfg.slba_H, iv_addr, iv_len / 2);
	zsda_reverse_memcpy((uint8_t *)wqe->cfg.slba_L, iv_addr + 8, iv_len / 2);

	return ret;
}

int
zsda_hash_wqe_build(void *op_in, const struct zsda_queue *queue,
	       void **op_cookies, const uint16_t new_tail)
{
	struct rte_crypto_op *op = op_in;

	struct rte_cryptodev_sym_session *session = op->sym->session;
	struct zsda_sym_session *sess =
		(struct zsda_sym_session *)session->driver_priv_data;

	struct zsda_wqe_crpt *wqe =
		(struct zsda_wqe_crpt *)(queue->base_addr +
					 (new_tail * queue->msg_size));
	struct zsda_op_cookie *cookie = op_cookies[new_tail];
	struct zsda_sgl *sgl_src = &cookie->sgl_src;
	uint8_t opcode;
	uint32_t op_offset;
	uint32_t op_src_len;
	int ret;

	opcode = zsda_opcode_hash_get(sess);
	if (opcode == ZSDA_OPC_INVALID) {
		ZSDA_LOG(ERR, "Failed! zsda_opcode_hash_get");
		return ZSDA_FAILED;
	}

	op_offset = op->sym->auth.data.offset;
	op_src_len = op->sym->auth.data.length;
	ret = zsda_sgl_fill(op->sym->m_src, op_offset, sgl_src,
				   cookie->sgl_src_phys_addr, op_src_len, NULL);
	if (ret) {
		ZSDA_LOG(ERR, "Failed! zsda_sgl_fill");
		return ret;
	}

	memset(wqe, 0, sizeof(struct zsda_wqe_crpt));
	cookie->used = true;
	cookie->sid = new_tail;
	cookie->op = op;
	wqe->valid = queue->valid;
	wqe->op_code = opcode;
	wqe->sid = cookie->sid;
	wqe->rx_sgl_type = WQE_ELM_TYPE_LIST;
	wqe->tx_sgl_type = WQE_ELM_TYPE_PHYS_ADDR;
	wqe->rx_addr = cookie->sgl_src_phys_addr;
	wqe->tx_addr = op->sym->auth.digest.phys_addr;
	wqe->rx_length = op->sym->auth.data.length;
	wqe->tx_length = sess->auth.digest_length;

	return ret;
}

int
zsda_crypto_callback(void *cookie_in, struct zsda_cqe *cqe)
{
	struct zsda_op_cookie *tmp_cookie = cookie_in;
	struct rte_crypto_op *op = tmp_cookie->op;

	if (!(CQE_ERR0(cqe->err0) || CQE_ERR1(cqe->err1)))
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	else {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return ZSDA_FAILED;
	}

	return ZSDA_SUCCESS;
}
