/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_AE_H_
#define _CNXK_AE_H_

#include <rte_common.h>
#include <rte_crypto_asym.h>
#include <rte_malloc.h>

#include "roc_ae.h"

#include "cnxk_cryptodev_ops.h"

#define ASYM_SESS_SIZE sizeof(struct rte_cryptodev_asym_session)

struct cnxk_ae_sess {
	uint8_t rte_sess[ASYM_SESS_SIZE];
	enum rte_crypto_asym_xform_type xfrm_type;
	union {
		struct rte_crypto_rsa_xform rsa_ctx;
		struct rte_crypto_modex_xform mod_ctx;
		struct roc_ae_ec_ctx ec_ctx;
	};
	uint64_t *cnxk_fpm_iova;
	struct roc_ae_ec_group **ec_grp;
	uint64_t cpt_inst_w7;
	uint64_t cpt_inst_w2;
	struct cnxk_cpt_qp *qp;
	struct roc_cpt_lf *lf;
	struct hw_ctx_s {
		union {
			struct {
				uint64_t rsvd : 48;

				uint64_t ctx_push_size : 7;
				uint64_t rsvd1 : 1;

				uint64_t ctx_hdr_size : 2;
				uint64_t aop_valid : 1;
				uint64_t rsvd2 : 1;
				uint64_t ctx_size : 4;
			} s;
			uint64_t u64;
		} w0;
		uint8_t rsvd[256];
	} hw_ctx __plt_aligned(ROC_ALIGN);
};

static __rte_always_inline void
cnxk_ae_modex_param_normalize(uint8_t **data, size_t *len)
{
	size_t i;

	/* Strip leading NUL bytes */
	for (i = 0; i < *len; i++) {
		if ((*data)[i] != 0)
			break;
	}
	*data += i;
	*len -= i;
}

static __rte_always_inline int
cnxk_ae_fill_modex_params(struct cnxk_ae_sess *sess,
			  struct rte_crypto_asym_xform *xform)
{
	struct rte_crypto_modex_xform *ctx = &sess->mod_ctx;
	size_t exp_len = xform->modex.exponent.length;
	size_t mod_len = xform->modex.modulus.length;
	uint8_t *exp = xform->modex.exponent.data;
	uint8_t *mod = xform->modex.modulus.data;

	cnxk_ae_modex_param_normalize(&mod, &mod_len);
	cnxk_ae_modex_param_normalize(&exp, &exp_len);

	if (unlikely(exp_len == 0 || mod_len == 0))
		return -EINVAL;

	if (unlikely(exp_len > mod_len))
		return -ENOTSUP;

	/* Allocate buffer to hold modexp params */
	ctx->modulus.data = rte_malloc(NULL, mod_len + exp_len, 0);
	if (ctx->modulus.data == NULL)
		return -ENOMEM;

	/* Set up modexp prime modulus and private exponent */
	memcpy(ctx->modulus.data, mod, mod_len);
	ctx->exponent.data = ctx->modulus.data + mod_len;
	memcpy(ctx->exponent.data, exp, exp_len);

	ctx->modulus.length = mod_len;
	ctx->exponent.length = exp_len;

	return 0;
}

static __rte_always_inline int
cnxk_ae_fill_rsa_params(struct cnxk_ae_sess *sess,
			struct rte_crypto_asym_xform *xform)
{
	struct rte_crypto_rsa_priv_key_qt qt = xform->rsa.qt;
	struct rte_crypto_rsa_xform *xfrm_rsa = &xform->rsa;
	struct rte_crypto_rsa_xform *rsa = &sess->rsa_ctx;
	struct rte_crypto_param_t d = xform->rsa.d;
	size_t mod_len = xfrm_rsa->n.length;
	size_t exp_len = xfrm_rsa->e.length;
	uint64_t total_size;
	size_t len = 0;

	/* Set private key type */
	rsa->key_type = xfrm_rsa->key_type;

	if (rsa->key_type == RTE_RSA_KEY_TYPE_QT) {
		if (qt.p.length != 0 && qt.p.data == NULL)
			return -EINVAL;

		/* Make sure key length used is not more than mod_len/2 */
		if (qt.p.data != NULL)
			len = (((mod_len / 2) < qt.p.length) ? 0 : qt.p.length * 5);
	} else if (rsa->key_type == RTE_RSA_KEY_TYPE_EXP) {
		if (d.length != 0 && d.data == NULL)
			return -EINVAL;

		len = d.length;
	}

	/* Total size required for RSA key params(n,e,(q,dQ,p,dP,qInv)) */
	total_size = mod_len + exp_len + len;

	/* Allocate buffer to hold all RSA keys */
	rsa->n.data = rte_malloc(NULL, total_size, 0);
	if (rsa->n.data == NULL)
		return -ENOMEM;

	/* Set up RSA prime modulus and public key exponent */
	memcpy(rsa->n.data, xfrm_rsa->n.data, mod_len);
	rsa->e.data = rsa->n.data + mod_len;
	memcpy(rsa->e.data, xfrm_rsa->e.data, exp_len);

	if (rsa->key_type == RTE_RSA_KEY_TYPE_QT) {
		/* Private key in quintuple format */
		rsa->qt.q.data = rsa->e.data + exp_len;
		memcpy(rsa->qt.q.data, qt.q.data, qt.q.length);
		rsa->qt.dQ.data = rsa->qt.q.data + qt.q.length;
		memcpy(rsa->qt.dQ.data, qt.dQ.data, qt.dQ.length);
		rsa->qt.p.data = rsa->qt.dQ.data + qt.dQ.length;
		if (qt.p.data != NULL)
			memcpy(rsa->qt.p.data, qt.p.data, qt.p.length);
		rsa->qt.dP.data = rsa->qt.p.data + qt.p.length;
		memcpy(rsa->qt.dP.data, qt.dP.data, qt.dP.length);
		rsa->qt.qInv.data = rsa->qt.dP.data + qt.dP.length;
		memcpy(rsa->qt.qInv.data, qt.qInv.data, qt.qInv.length);

		rsa->qt.q.length = qt.q.length;
		rsa->qt.dQ.length = qt.dQ.length;
		rsa->qt.p.length = qt.p.length;
		rsa->qt.dP.length = qt.dP.length;
		rsa->qt.qInv.length = qt.qInv.length;
	} else if (rsa->key_type == RTE_RSA_KEY_TYPE_EXP) {
		/* Private key in exponent format */
		rsa->d.data = rsa->e.data + exp_len;
		memcpy(rsa->d.data, d.data, d.length);
		rsa->d.length = d.length;
	}
	rsa->n.length = mod_len;
	rsa->e.length = exp_len;

	return 0;
}

static __rte_always_inline int
cnxk_ae_fill_ec_params(struct cnxk_ae_sess *sess,
		       struct rte_crypto_asym_xform *xform)
{
	struct roc_ae_ec_ctx *ec = &sess->ec_ctx;

	switch (xform->ec.curve_id) {
	case RTE_CRYPTO_EC_GROUP_SECP192R1:
		ec->curveid = ROC_AE_EC_ID_P192;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP224R1:
		ec->curveid = ROC_AE_EC_ID_P224;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP256R1:
		ec->curveid = ROC_AE_EC_ID_P256;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP384R1:
		ec->curveid = ROC_AE_EC_ID_P384;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP521R1:
		ec->curveid = ROC_AE_EC_ID_P521;
		break;
	default:
		/* Only NIST curves (FIPS 186-4) are supported */
		return -EINVAL;
	}

	return 0;
}

static __rte_always_inline int
cnxk_ae_fill_session_parameters(struct cnxk_ae_sess *sess,
				struct rte_crypto_asym_xform *xform)
{
	int ret;

	sess->xfrm_type = xform->xform_type;

	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		ret = cnxk_ae_fill_rsa_params(sess, xform);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		ret = cnxk_ae_fill_modex_params(sess, xform);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		/* Fall through */
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
	case RTE_CRYPTO_ASYM_XFORM_ECFPM:
		ret = cnxk_ae_fill_ec_params(sess, xform);
		break;
	default:
		return -ENOTSUP;
	}
	return ret;
}

static inline void
cnxk_ae_free_session_parameters(struct cnxk_ae_sess *sess)
{
	struct rte_crypto_modex_xform *mod;
	struct rte_crypto_rsa_xform *rsa;

	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		rsa = &sess->rsa_ctx;
		rte_free(rsa->n.data);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		mod = &sess->mod_ctx;
		rte_free(mod->modulus.data);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		/* Fall through */
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
	case RTE_CRYPTO_ASYM_XFORM_ECFPM:
		break;
	default:
		break;
	}
}

static __rte_always_inline int
cnxk_ae_modex_prep(struct rte_crypto_op *op, struct roc_ae_buf_ptr *meta_buf,
		   struct rte_crypto_modex_xform *mod, struct cpt_inst_s *inst)
{
	uint32_t exp_len = mod->exponent.length;
	uint32_t mod_len = mod->modulus.length;
	struct rte_crypto_mod_op_param mod_op;
	uint64_t total_key_len;
	union cpt_inst_w4 w4;
	uint32_t base_len;
	uint32_t dlen;
	uint8_t *dptr;

	mod_op = op->asym->modex;

	base_len = mod_op.base.length;
	if (unlikely(base_len > mod_len)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -ENOTSUP;
	}

	total_key_len = mod_len + exp_len;

	/* Input buffer */
	dptr = meta_buf->vaddr;
	inst->dptr = (uintptr_t)dptr;
	memcpy(dptr, mod->modulus.data, total_key_len);
	dptr += total_key_len;
	memcpy(dptr, mod_op.base.data, base_len);
	dptr += base_len;
	dlen = total_key_len + base_len;

	/* Setup opcodes */
	w4.s.opcode_major = ROC_AE_MAJOR_OP_MODEX;
	w4.s.opcode_minor = ROC_AE_MINOR_OP_MODEX;

	w4.s.param1 = mod_len;
	w4.s.param2 = exp_len;
	w4.s.dlen = dlen;

	inst->w4.u64 = w4.u64;
	inst->rptr = (uintptr_t)dptr;

	return 0;
}

static __rte_always_inline void
cnxk_ae_rsa_prep(struct rte_crypto_op *op, struct roc_ae_buf_ptr *meta_buf,
		 struct rte_crypto_rsa_xform *rsa,
		 rte_crypto_param *crypto_param, struct cpt_inst_s *inst)
{
	struct rte_crypto_rsa_op_param rsa_op;
	uint32_t mod_len = rsa->n.length;
	uint32_t exp_len = rsa->e.length;
	uint64_t total_key_len;
	union cpt_inst_w4 w4;
	uint32_t in_size;
	uint32_t dlen;
	uint8_t *dptr;

	rsa_op = op->asym->rsa;
	total_key_len = mod_len + exp_len;

	/* Input buffer */
	dptr = meta_buf->vaddr;
	inst->dptr = (uintptr_t)dptr;
	memcpy(dptr, rsa->n.data, total_key_len);
	dptr += total_key_len;

	in_size = crypto_param->length;
	memcpy(dptr, crypto_param->data, in_size);

	dptr += in_size;
	dlen = total_key_len + in_size;

	if (rsa_op.padding.type == RTE_CRYPTO_RSA_PADDING_NONE) {
		/* Use mod_exp operation for no_padding type */
		w4.s.opcode_minor = ROC_AE_MINOR_OP_MODEX;
		w4.s.param2 = exp_len;
	} else {
		if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT) {
			w4.s.opcode_minor = ROC_AE_MINOR_OP_PKCS_ENC;
			/* Public key encrypt, use BT2*/
			w4.s.param2 = ROC_AE_CPT_BLOCK_TYPE2 |
				      ((uint16_t)(exp_len) << 1);
		} else if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_VERIFY) {
			w4.s.opcode_minor = ROC_AE_MINOR_OP_PKCS_DEC;
			/* Public key decrypt, use BT1 */
			w4.s.param2 = ROC_AE_CPT_BLOCK_TYPE1;
		}
	}

	w4.s.opcode_major = ROC_AE_MAJOR_OP_MODEX;

	w4.s.param1 = mod_len;
	w4.s.dlen = dlen;

	inst->w4.u64 = w4.u64;
	inst->rptr = (uintptr_t)dptr;
}

static __rte_always_inline void
cnxk_ae_rsa_exp_prep(struct rte_crypto_op *op, struct roc_ae_buf_ptr *meta_buf,
		     struct rte_crypto_rsa_xform *rsa, rte_crypto_param *crypto_param,
		     struct cpt_inst_s *inst)
{
	struct rte_crypto_rsa_op_param rsa_op;
	uint32_t privkey_len = rsa->d.length;
	uint32_t mod_len = rsa->n.length;
	union cpt_inst_w4 w4;
	uint32_t in_size;
	uint32_t dlen;
	uint8_t *dptr;

	rsa_op = op->asym->rsa;

	/* Input buffer */
	dptr = meta_buf->vaddr;
	inst->dptr = (uintptr_t)dptr;
	memcpy(dptr, rsa->n.data, mod_len);
	dptr += mod_len;
	memcpy(dptr, rsa->d.data, privkey_len);
	dptr += privkey_len;

	in_size = crypto_param->length;
	memcpy(dptr, crypto_param->data, in_size);

	dptr += in_size;
	dlen = mod_len + privkey_len + in_size;

	if (rsa_op.padding.type == RTE_CRYPTO_RSA_PADDING_NONE) {
		/* Use mod_exp operation for no_padding type */
		w4.s.opcode_minor = ROC_AE_MINOR_OP_MODEX;
		w4.s.param2 = privkey_len;
	} else {
		if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_SIGN) {
			w4.s.opcode_minor = ROC_AE_MINOR_OP_PKCS_ENC;
			/* Private key encrypt (exponent), use BT1*/
			w4.s.param2 = ROC_AE_CPT_BLOCK_TYPE1 | ((uint16_t)(privkey_len) << 1);
		} else if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_DECRYPT) {
			w4.s.opcode_minor = ROC_AE_MINOR_OP_PKCS_DEC;
			/* Private key decrypt (exponent), use BT2 */
			w4.s.param2 = ROC_AE_CPT_BLOCK_TYPE2;
		}
	}

	w4.s.opcode_major = ROC_AE_MAJOR_OP_MODEX;

	w4.s.param1 = mod_len;
	w4.s.dlen = dlen;

	inst->w4.u64 = w4.u64;
	inst->rptr = (uintptr_t)dptr;
}

static __rte_always_inline void
cnxk_ae_rsa_crt_prep(struct rte_crypto_op *op, struct roc_ae_buf_ptr *meta_buf,
		     struct rte_crypto_rsa_xform *rsa, rte_crypto_param *crypto_param,
		     struct cpt_inst_s *inst)
{
	uint32_t qInv_len = rsa->qt.qInv.length;
	struct rte_crypto_rsa_op_param rsa_op;
	uint32_t dP_len = rsa->qt.dP.length;
	uint32_t dQ_len = rsa->qt.dQ.length;
	uint32_t p_len = rsa->qt.p.length;
	uint32_t q_len = rsa->qt.q.length;
	uint32_t mod_len = rsa->n.length;
	uint64_t total_key_len;
	union cpt_inst_w4 w4;
	uint32_t in_size;
	uint32_t dlen;
	uint8_t *dptr;

	rsa_op = op->asym->rsa;
	total_key_len = p_len + q_len + dP_len + dQ_len + qInv_len;

	/* Input buffer */
	dptr = meta_buf->vaddr;
	inst->dptr = (uintptr_t)dptr;
	memcpy(dptr, rsa->qt.q.data, total_key_len);
	dptr += total_key_len;

	in_size = crypto_param->length;
	memcpy(dptr, crypto_param->data, in_size);

	dptr += in_size;
	dlen = total_key_len + in_size;

	if (rsa_op.padding.type == RTE_CRYPTO_RSA_PADDING_NONE) {
		/*Use mod_exp operation for no_padding type */
		w4.s.opcode_minor = ROC_AE_MINOR_OP_MODEX_CRT;
	} else {
		if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_SIGN) {
			w4.s.opcode_minor = ROC_AE_MINOR_OP_PKCS_ENC_CRT;
			/* Private encrypt, use BT1 */
			w4.s.param2 = ROC_AE_CPT_BLOCK_TYPE1;
		} else if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_DECRYPT) {
			w4.s.opcode_minor = ROC_AE_MINOR_OP_PKCS_DEC_CRT;
			/* Private decrypt, use BT2 */
			w4.s.param2 = ROC_AE_CPT_BLOCK_TYPE2;
		}
	}

	w4.s.opcode_major = ROC_AE_MAJOR_OP_MODEX;

	w4.s.param1 = mod_len;
	w4.s.dlen = dlen;

	inst->w4.u64 = w4.u64;
	inst->rptr = (uintptr_t)dptr;
}

static __rte_always_inline int __rte_hot
cnxk_ae_enqueue_rsa_op(struct rte_crypto_op *op, struct roc_ae_buf_ptr *meta_buf,
		       struct cnxk_ae_sess *sess, struct cpt_inst_s *inst)
{
	struct rte_crypto_rsa_op_param *rsa = &op->asym->rsa;
	struct rte_crypto_rsa_xform *ctx = &sess->rsa_ctx;

	switch (rsa->op_type) {
	case RTE_CRYPTO_ASYM_OP_VERIFY:
		cnxk_ae_rsa_prep(op, meta_buf, ctx, &rsa->sign, inst);
		break;
	case RTE_CRYPTO_ASYM_OP_ENCRYPT:
		cnxk_ae_rsa_prep(op, meta_buf, ctx, &rsa->message, inst);
		break;
	case RTE_CRYPTO_ASYM_OP_SIGN:
		if (ctx->key_type == RTE_RSA_KEY_TYPE_QT)
			cnxk_ae_rsa_crt_prep(op, meta_buf, ctx, &rsa->message, inst);
		else
			cnxk_ae_rsa_exp_prep(op, meta_buf, ctx, &rsa->message, inst);
		break;
	case RTE_CRYPTO_ASYM_OP_DECRYPT:
		if (ctx->key_type == RTE_RSA_KEY_TYPE_QT)
			cnxk_ae_rsa_crt_prep(op, meta_buf, ctx, &rsa->cipher, inst);
		else
			cnxk_ae_rsa_exp_prep(op, meta_buf, ctx, &rsa->cipher, inst);
		break;
	default:
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}
	return 0;
}

static __rte_always_inline void
cnxk_ae_ecdsa_sign_prep(struct rte_crypto_ecdsa_op_param *ecdsa,
			struct roc_ae_buf_ptr *meta_buf,
			uint64_t fpm_table_iova, struct roc_ae_ec_group *ec_grp,
			uint8_t curveid, struct cpt_inst_s *inst)
{
	uint16_t message_len = ecdsa->message.length;
	uint16_t pkey_len = ecdsa->pkey.length;
	uint16_t p_align, k_align, m_align;
	uint16_t k_len = ecdsa->k.length;
	uint16_t order_len, prime_len;
	uint16_t o_offset, pk_offset;
	union cpt_inst_w4 w4;
	uint16_t dlen;
	uint8_t *dptr;

	prime_len = ec_grp->prime.length;
	order_len = ec_grp->order.length;

	/* Truncate input length to curve prime length */
	if (message_len > prime_len)
		message_len = prime_len;
	m_align = RTE_ALIGN_CEIL(message_len, 8);

	p_align = RTE_ALIGN_CEIL(prime_len, 8);
	k_align = RTE_ALIGN_CEIL(k_len, 8);

	/* Set write offset for order and private key */
	o_offset = prime_len - order_len;
	pk_offset = prime_len - pkey_len;

	/* Input buffer */
	dptr = meta_buf->vaddr;
	inst->dptr = (uintptr_t)dptr;

	/*
	 * Set dlen = sum(sizeof(fpm address), ROUNDUP8(scalar len, input len),
	 * ROUNDUP8(priv key len, prime len, order len)).
	 * Please note, private key, order cannot exceed prime
	 * length i.e 3 * p_align.
	 */
	dlen = sizeof(fpm_table_iova) + k_align + m_align + p_align * 5;

	memset(dptr, 0, dlen);

	*(uint64_t *)dptr = fpm_table_iova;
	dptr += sizeof(fpm_table_iova);

	memcpy(dptr, ecdsa->k.data, k_len);
	dptr += k_align;

	memcpy(dptr, ec_grp->prime.data, prime_len);
	dptr += p_align;

	memcpy(dptr + o_offset, ec_grp->order.data, order_len);
	dptr += p_align;

	memcpy(dptr + pk_offset, ecdsa->pkey.data, pkey_len);
	dptr += p_align;

	memcpy(dptr, ecdsa->message.data, message_len);
	dptr += m_align;

	memcpy(dptr, ec_grp->consta.data, prime_len);
	dptr += p_align;

	memcpy(dptr, ec_grp->constb.data, prime_len);
	dptr += p_align;

	/* Setup opcodes */
	w4.s.opcode_major = ROC_AE_MAJOR_OP_ECDSA;
	w4.s.opcode_minor = ROC_AE_MINOR_OP_ECDSA_SIGN;

	w4.s.param1 = curveid | (message_len << 8);
	w4.s.param2 = (pkey_len << 8) | k_len;
	w4.s.dlen = dlen;

	inst->w4.u64 = w4.u64;
	inst->rptr = (uintptr_t)dptr;
}

static __rte_always_inline void
cnxk_ae_ecdsa_verify_prep(struct rte_crypto_ecdsa_op_param *ecdsa,
			  struct roc_ae_buf_ptr *meta_buf,
			  uint64_t fpm_table_iova,
			  struct roc_ae_ec_group *ec_grp, uint8_t curveid,
			  struct cpt_inst_s *inst)
{
	uint32_t message_len = ecdsa->message.length;
	uint16_t o_offset, r_offset, s_offset;
	uint16_t qx_len = ecdsa->q.x.length;
	uint16_t qy_len = ecdsa->q.y.length;
	uint16_t r_len = ecdsa->r.length;
	uint16_t s_len = ecdsa->s.length;
	uint16_t order_len, prime_len;
	uint16_t qx_offset, qy_offset;
	uint16_t p_align, m_align;
	union cpt_inst_w4 w4;
	uint16_t dlen;
	uint8_t *dptr;

	prime_len = ec_grp->prime.length;
	order_len = ec_grp->order.length;

	/* Truncate input length to curve prime length */
	if (message_len > prime_len)
		message_len = prime_len;

	m_align = RTE_ALIGN_CEIL(message_len, 8);
	p_align = RTE_ALIGN_CEIL(prime_len, 8);

	/* Set write offset for sign, order and public key coordinates */
	o_offset = prime_len - order_len;
	qx_offset = prime_len - qx_len;
	qy_offset = prime_len - qy_len;
	r_offset = prime_len - r_len;
	s_offset = prime_len - s_len;

	/* Input buffer */
	dptr = meta_buf->vaddr;
	inst->dptr = (uintptr_t)dptr;

	/*
	 * Set dlen = sum(sizeof(fpm address), ROUNDUP8(message len),
	 * ROUNDUP8(sign len(r and s), public key len(x and y coordinates),
	 * prime len, order len)).
	 * Please note sign, public key and order can not exceed prime length
	 * i.e. 6 * p_align
	 */
	dlen = sizeof(fpm_table_iova) + m_align + (8 * p_align);

	memset(dptr, 0, dlen);

	*(uint64_t *)dptr = fpm_table_iova;
	dptr += sizeof(fpm_table_iova);

	memcpy(dptr + r_offset, ecdsa->r.data, r_len);
	dptr += p_align;

	memcpy(dptr + s_offset, ecdsa->s.data, s_len);
	dptr += p_align;

	memcpy(dptr, ecdsa->message.data, message_len);
	dptr += m_align;

	memcpy(dptr + o_offset, ec_grp->order.data, order_len);
	dptr += p_align;

	memcpy(dptr, ec_grp->prime.data, prime_len);
	dptr += p_align;

	memcpy(dptr + qx_offset, ecdsa->q.x.data, qx_len);
	dptr += p_align;

	memcpy(dptr + qy_offset, ecdsa->q.y.data, qy_len);
	dptr += p_align;

	memcpy(dptr, ec_grp->consta.data, prime_len);
	dptr += p_align;

	memcpy(dptr, ec_grp->constb.data, prime_len);
	dptr += p_align;

	/* Setup opcodes */
	w4.s.opcode_major = ROC_AE_MAJOR_OP_ECDSA;
	w4.s.opcode_minor = ROC_AE_MINOR_OP_ECDSA_VERIFY;

	w4.s.param1 = curveid | (message_len << 8);
	w4.s.param2 = 0;
	w4.s.dlen = dlen;

	inst->w4.u64 = w4.u64;
	inst->rptr = (uintptr_t)dptr;
}

static __rte_always_inline int __rte_hot
cnxk_ae_enqueue_ecdsa_op(struct rte_crypto_op *op,
			 struct roc_ae_buf_ptr *meta_buf,
			 struct cnxk_ae_sess *sess, uint64_t *fpm_iova,
			 struct roc_ae_ec_group **ec_grp,
			 struct cpt_inst_s *inst)
{
	struct rte_crypto_ecdsa_op_param *ecdsa = &op->asym->ecdsa;
	uint8_t curveid = sess->ec_ctx.curveid;

	if (ecdsa->op_type == RTE_CRYPTO_ASYM_OP_SIGN)
		cnxk_ae_ecdsa_sign_prep(ecdsa, meta_buf, fpm_iova[curveid],
					ec_grp[curveid], curveid, inst);
	else if (ecdsa->op_type == RTE_CRYPTO_ASYM_OP_VERIFY)
		cnxk_ae_ecdsa_verify_prep(ecdsa, meta_buf, fpm_iova[curveid],
					  ec_grp[curveid], curveid, inst);
	else {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}
	return 0;
}

static __rte_always_inline int
cnxk_ae_ecfpm_prep(struct rte_crypto_ecpm_op_param *ecpm,
		   struct roc_ae_buf_ptr *meta_buf, uint64_t *fpm_iova,
		   struct roc_ae_ec_group *ec_grp, uint8_t curveid,
		   struct cpt_inst_s *inst, int cpt_ver)
{
	uint16_t scalar_align, p_align;
	uint16_t dlen, prime_len;
	uint64_t fpm_table_iova;
	union cpt_inst_w4 w4;
	uint8_t *dptr;

	prime_len = ec_grp->prime.length;
	fpm_table_iova = (uint64_t)fpm_iova[curveid];

	/* Input buffer */
	dptr = meta_buf->vaddr;
	inst->dptr = (uintptr_t)dptr;

	p_align = RTE_ALIGN_CEIL(prime_len, 8);
	scalar_align = RTE_ALIGN_CEIL(ecpm->scalar.length, 8);

	/*
	 * Set dlen = sum(prime, scalar length, table address and
	 * optionally ROUNDUP8(input point(x and y coordinates)).
	 * Please note point length is equivalent to prime of the curve
	 */
	if (cpt_ver == ROC_CPT_REVISION_ID_96XX_C0) {
		dlen = sizeof(fpm_table_iova) + 3 * p_align + scalar_align;
		memset(dptr, 0, dlen);
		*(uint64_t *)dptr = fpm_table_iova;
		dptr += sizeof(fpm_table_iova);
		memcpy(dptr, ecpm->scalar.data, ecpm->scalar.length);
		dptr += scalar_align;
		memcpy(dptr, ec_grp->prime.data, ec_grp->prime.length);
		dptr += p_align;
		memcpy(dptr, ec_grp->consta.data, ec_grp->consta.length);
		dptr += p_align;
		memcpy(dptr, ec_grp->constb.data, ec_grp->constb.length);
		dptr += p_align;
	} else {
		dlen = sizeof(fpm_table_iova) + p_align + scalar_align;
		memset(dptr, 0, dlen);
		memcpy(dptr, ecpm->scalar.data, ecpm->scalar.length);
		dptr += scalar_align;
		memcpy(dptr, ec_grp->prime.data, ec_grp->prime.length);
		dptr += p_align;
		*(uint64_t *)dptr = fpm_table_iova;
		dptr += sizeof(fpm_table_iova);
	}

	/* Setup opcodes */
	w4.s.opcode_major = ROC_AE_MAJOR_OP_ECC;
	w4.s.opcode_minor = ROC_AE_MINOR_OP_ECC_FPM;

	w4.s.param1 = curveid | (1 << 8);
	w4.s.param2 = ecpm->scalar.length;
	w4.s.dlen = dlen;

	inst->w4.u64 = w4.u64;
	inst->rptr = (uintptr_t)dptr;

	return 0;
}

static __rte_always_inline int
cnxk_ae_ecpm_prep(struct rte_crypto_ecpm_op_param *ecpm,
		  struct roc_ae_buf_ptr *meta_buf,
		  struct roc_ae_ec_group *ec_grp, uint8_t curveid,
		  struct cpt_inst_s *inst)
{
	uint16_t x1_len = ecpm->p.x.length;
	uint16_t y1_len = ecpm->p.y.length;
	uint16_t scalar_align, p_align;
	uint16_t x1_offset, y1_offset;
	uint16_t dlen, prime_len;
	union cpt_inst_w4 w4;
	uint8_t *dptr;

	prime_len = ec_grp->prime.length;

	/* Input buffer */
	dptr = meta_buf->vaddr;
	inst->dptr = (uintptr_t)dptr;

	p_align = RTE_ALIGN_CEIL(prime_len, 8);
	scalar_align = RTE_ALIGN_CEIL(ecpm->scalar.length, 8);

	/*
	 * Set dlen = sum(ROUNDUP8(input point(x and y coordinates), prime,
	 * scalar length),
	 * Please note point length is equivalent to prime of the curve
	 */
	dlen = 5 * p_align + scalar_align;

	x1_offset = prime_len - x1_len;
	y1_offset = prime_len - y1_len;

	memset(dptr, 0, dlen);

	/* Copy input point, scalar, prime */
	memcpy(dptr + x1_offset, ecpm->p.x.data, x1_len);
	dptr += p_align;
	memcpy(dptr + y1_offset, ecpm->p.y.data, y1_len);
	dptr += p_align;
	memcpy(dptr, ecpm->scalar.data, ecpm->scalar.length);
	dptr += scalar_align;
	memcpy(dptr, ec_grp->prime.data, ec_grp->prime.length);
	dptr += p_align;
	memcpy(dptr, ec_grp->consta.data, ec_grp->consta.length);
	dptr += p_align;
	memcpy(dptr, ec_grp->constb.data, ec_grp->constb.length);
	dptr += p_align;

	/* Setup opcodes */
	w4.s.opcode_major = ROC_AE_MAJOR_OP_ECC;
	w4.s.opcode_minor = ROC_AE_MINOR_OP_ECC_UMP;

	w4.s.param1 = curveid;
	w4.s.param2 = ecpm->scalar.length;
	w4.s.dlen = dlen;

	inst->w4.u64 = w4.u64;
	inst->rptr = (uintptr_t)dptr;

	return 0;
}

static __rte_always_inline void
cnxk_ae_dequeue_rsa_op(struct rte_crypto_op *cop, uint8_t *rptr,
		       struct rte_crypto_rsa_xform *rsa_ctx)
{
	struct rte_crypto_rsa_op_param *rsa = &cop->asym->rsa;

	switch (rsa->op_type) {
	case RTE_CRYPTO_ASYM_OP_ENCRYPT:
		rsa->cipher.length = rsa_ctx->n.length;
		memcpy(rsa->cipher.data, rptr, rsa->cipher.length);
		break;
	case RTE_CRYPTO_ASYM_OP_DECRYPT:
		if (rsa->padding.type == RTE_CRYPTO_RSA_PADDING_NONE) {
			rsa->message.length = rsa_ctx->n.length;
			memcpy(rsa->message.data, rptr, rsa->message.length);
		} else {
			/* Get length of decrypted output */
			rsa->message.length =
				rte_cpu_to_be_16(*((uint16_t *)rptr));
			/*
			 * Offset output data pointer by length field
			 * (2 bytes) and copy decrypted data.
			 */
			memcpy(rsa->message.data, rptr + 2,
			       rsa->message.length);
		}
		break;
	case RTE_CRYPTO_ASYM_OP_SIGN:
		rsa->sign.length = rsa_ctx->n.length;
		memcpy(rsa->sign.data, rptr, rsa->sign.length);
		break;
	case RTE_CRYPTO_ASYM_OP_VERIFY:
		if (rsa->padding.type == RTE_CRYPTO_RSA_PADDING_NONE) {
			rsa->sign.length = rsa_ctx->n.length;
			memcpy(rsa->sign.data, rptr, rsa->sign.length);
		} else {
			/* Get length of signed output */
			rsa->sign.length =
				rte_cpu_to_be_16(*((uint16_t *)rptr));
			/*
			 * Offset output data pointer by length field
			 * (2 bytes) and copy signed data.
			 */
			memcpy(rsa->sign.data, rptr + 2, rsa->sign.length);
		}
		if (memcmp(rsa->sign.data, rsa->message.data,
			   rsa->message.length)) {
			cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
		}
		break;
	default:
		cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		break;
	}
}

static __rte_always_inline void
cnxk_ae_dequeue_ecdsa_op(struct rte_crypto_ecdsa_op_param *ecdsa, uint8_t *rptr,
			 struct roc_ae_ec_ctx *ec,
			 struct roc_ae_ec_group **ec_grp)
{
	int prime_len = ec_grp[ec->curveid]->prime.length;

	if (ecdsa->op_type == RTE_CRYPTO_ASYM_OP_VERIFY)
		return;

	/* Separate out sign r and s components */
	memcpy(ecdsa->r.data, rptr, prime_len);
	memcpy(ecdsa->s.data, rptr + RTE_ALIGN_CEIL(prime_len, 8), prime_len);
	ecdsa->r.length = prime_len;
	ecdsa->s.length = prime_len;
}

static __rte_always_inline void
cnxk_ae_dequeue_ecpm_op(struct rte_crypto_ecpm_op_param *ecpm, uint8_t *rptr,
			struct roc_ae_ec_ctx *ec,
			struct roc_ae_ec_group **ec_grp)
{
	int prime_len = ec_grp[ec->curveid]->prime.length;

	memcpy(ecpm->r.x.data, rptr, prime_len);
	memcpy(ecpm->r.y.data, rptr + RTE_ALIGN_CEIL(prime_len, 8), prime_len);
	ecpm->r.x.length = prime_len;
	ecpm->r.y.length = prime_len;
}

static __rte_always_inline void *
cnxk_ae_alloc_meta(struct roc_ae_buf_ptr *buf,
		   struct rte_mempool *cpt_meta_pool,
		   struct cpt_inflight_req *infl_req)
{
	uint8_t *mdata;

	if (unlikely(rte_mempool_get(cpt_meta_pool, (void **)&mdata) < 0))
		return NULL;

	buf->vaddr = mdata;

	infl_req->mdata = mdata;
	infl_req->op_flags |= CPT_OP_FLAGS_METABUF;

	return mdata;
}

static __rte_always_inline int32_t __rte_hot
cnxk_ae_enqueue(struct cnxk_cpt_qp *qp, struct rte_crypto_op *op,
		struct cpt_inflight_req *infl_req, struct cpt_inst_s *inst,
		struct cnxk_ae_sess *sess)
{
	struct cpt_qp_meta_info *minfo = &qp->meta_info;
	struct rte_crypto_asym_op *asym_op = op->asym;
	struct roc_ae_buf_ptr meta_buf;
	uint64_t *mop;
	void *mdata;
	int ret;

	mdata = cnxk_ae_alloc_meta(&meta_buf, minfo->pool, infl_req);
	if (mdata == NULL)
		return -ENOMEM;

	/* Reserve 8B for RPTR */
	meta_buf.vaddr = PLT_PTR_ADD(mdata, sizeof(uint64_t));

	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		ret = cnxk_ae_modex_prep(op, &meta_buf, &sess->mod_ctx, inst);
		if (unlikely(ret))
			goto req_fail;
		break;
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		ret = cnxk_ae_enqueue_rsa_op(op, &meta_buf, sess, inst);
		if (unlikely(ret))
			goto req_fail;
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		ret = cnxk_ae_enqueue_ecdsa_op(op, &meta_buf, sess,
					       sess->cnxk_fpm_iova,
					       sess->ec_grp, inst);
		if (unlikely(ret))
			goto req_fail;
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
		ret = cnxk_ae_ecpm_prep(&asym_op->ecpm, &meta_buf,
					sess->ec_grp[sess->ec_ctx.curveid],
					sess->ec_ctx.curveid, inst);
		if (unlikely(ret))
			goto req_fail;
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECFPM:
		ret = cnxk_ae_ecfpm_prep(&asym_op->ecpm, &meta_buf,
					 sess->cnxk_fpm_iova,
					 sess->ec_grp[sess->ec_ctx.curveid],
					 sess->ec_ctx.curveid, inst,
					 sess->lf->roc_cpt->cpt_revision);
		if (unlikely(ret))
			goto req_fail;
		break;
	default:
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		ret = -EINVAL;
		goto req_fail;
	}

	mop = mdata;
	mop[0] = inst->rptr;
	return 0;

req_fail:
	rte_mempool_put(minfo->pool, infl_req->mdata);
	return ret;
}

static __rte_always_inline void
cnxk_ae_post_process(struct rte_crypto_op *cop, struct cnxk_ae_sess *sess,
		     uint8_t *rptr)
{
	struct rte_crypto_asym_op *op = cop->asym;

	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		cnxk_ae_dequeue_rsa_op(cop, rptr, &sess->rsa_ctx);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		op->modex.result.length = sess->mod_ctx.modulus.length;
		memcpy(op->modex.result.data, rptr, op->modex.result.length);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		cnxk_ae_dequeue_ecdsa_op(&op->ecdsa, rptr, &sess->ec_ctx,
					 sess->ec_grp);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
	case RTE_CRYPTO_ASYM_XFORM_ECFPM:
		cnxk_ae_dequeue_ecpm_op(&op->ecpm, rptr, &sess->ec_ctx,
					sess->ec_grp);
		break;
	default:
		cop->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		break;
	}
}
#endif /* _CNXK_AE_H_ */
