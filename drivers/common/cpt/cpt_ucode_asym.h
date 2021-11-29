/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2019 Marvell International Ltd.
 */

#ifndef _CPT_UCODE_ASYM_H_
#define _CPT_UCODE_ASYM_H_

#include <rte_common.h>
#include <rte_crypto_asym.h>
#include <rte_malloc.h>

#include "cpt_common.h"
#include "cpt_hw_types.h"
#include "cpt_mcode_defines.h"

static __rte_always_inline void
cpt_modex_param_normalize(uint8_t **data, size_t *len)
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
cpt_fill_modex_params(struct cpt_asym_sess_misc *sess,
		      struct rte_crypto_asym_xform *xform)
{
	struct rte_crypto_modex_xform *ctx = &sess->mod_ctx;
	size_t exp_len = xform->modex.exponent.length;
	size_t mod_len = xform->modex.modulus.length;
	uint8_t *exp = xform->modex.exponent.data;
	uint8_t *mod = xform->modex.modulus.data;

	cpt_modex_param_normalize(&mod, &mod_len);
	cpt_modex_param_normalize(&exp, &exp_len);

	if (unlikely(exp_len == 0 || mod_len == 0))
		return -EINVAL;

	if (unlikely(exp_len > mod_len)) {
		CPT_LOG_DP_ERR("Exponent length greater than modulus length is not supported");
		return -ENOTSUP;
	}

	/* Allocate buffer to hold modexp params */
	ctx->modulus.data = rte_malloc(NULL, mod_len + exp_len, 0);
	if (ctx->modulus.data == NULL) {
		CPT_LOG_DP_ERR("Could not allocate buffer for modex params");
		return -ENOMEM;
	}

	/* Set up modexp prime modulus and private exponent */

	memcpy(ctx->modulus.data, mod, mod_len);
	ctx->exponent.data = ctx->modulus.data + mod_len;
	memcpy(ctx->exponent.data, exp, exp_len);

	ctx->modulus.length = mod_len;
	ctx->exponent.length = exp_len;

	return 0;
}

static __rte_always_inline int
cpt_fill_rsa_params(struct cpt_asym_sess_misc *sess,
		    struct rte_crypto_asym_xform *xform)
{
	struct rte_crypto_rsa_priv_key_qt qt = xform->rsa.qt;
	struct rte_crypto_rsa_xform *xfrm_rsa = &xform->rsa;
	struct rte_crypto_rsa_xform *rsa = &sess->rsa_ctx;
	size_t mod_len = xfrm_rsa->n.length;
	size_t exp_len = xfrm_rsa->e.length;
	uint64_t total_size;
	size_t len = 0;

	/* Make sure key length used is not more than mod_len/2 */
	if (qt.p.data != NULL)
		len = (((mod_len / 2) < qt.p.length) ? len : qt.p.length);

	/* Total size required for RSA key params(n,e,(q,dQ,p,dP,qInv)) */
	total_size = mod_len + exp_len + 5 * len;

	/* Allocate buffer to hold all RSA keys */
	rsa->n.data = rte_malloc(NULL, total_size, 0);
	if (rsa->n.data == NULL) {
		CPT_LOG_DP_ERR("Could not allocate buffer for RSA keys");
		return -ENOMEM;
	}

	/* Set up RSA prime modulus and public key exponent */
	memcpy(rsa->n.data, xfrm_rsa->n.data, mod_len);
	rsa->e.data = rsa->n.data + mod_len;
	memcpy(rsa->e.data, xfrm_rsa->e.data, exp_len);

	/* Private key in quintuple format */
	if (len != 0) {
		rsa->qt.q.data = rsa->e.data + exp_len;
		memcpy(rsa->qt.q.data, qt.q.data, qt.q.length);
		rsa->qt.dQ.data = rsa->qt.q.data + qt.q.length;
		memcpy(rsa->qt.dQ.data, qt.dQ.data, qt.dQ.length);
		rsa->qt.p.data = rsa->qt.dQ.data + qt.dQ.length;
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
	}
	rsa->n.length = mod_len;
	rsa->e.length = exp_len;

	return 0;
}

static __rte_always_inline int
cpt_fill_ec_params(struct cpt_asym_sess_misc *sess,
		      struct rte_crypto_asym_xform *xform)
{
	struct cpt_asym_ec_ctx *ec = &sess->ec_ctx;

	switch (xform->ec.curve_id) {
	case RTE_CRYPTO_EC_GROUP_SECP192R1:
		ec->curveid = CPT_EC_ID_P192;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP224R1:
		ec->curveid = CPT_EC_ID_P224;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP256R1:
		ec->curveid = CPT_EC_ID_P256;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP384R1:
		ec->curveid = CPT_EC_ID_P384;
		break;
	case RTE_CRYPTO_EC_GROUP_SECP521R1:
		ec->curveid = CPT_EC_ID_P521;
		break;
	default:
		/* Only NIST curves (FIPS 186-4) are supported */
		CPT_LOG_DP_ERR("Unsupported curve");
		return -EINVAL;
	}

	return 0;
}

static __rte_always_inline int
cpt_fill_asym_session_parameters(struct cpt_asym_sess_misc *sess,
				 struct rte_crypto_asym_xform *xform)
{
	int ret;

	sess->xfrm_type = xform->xform_type;

	switch (xform->xform_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		ret = cpt_fill_rsa_params(sess, xform);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		ret = cpt_fill_modex_params(sess, xform);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		/* Fall through */
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
		ret = cpt_fill_ec_params(sess, xform);
		break;
	default:
		CPT_LOG_DP_ERR("Unsupported transform type");
		return -ENOTSUP;
	}
	return ret;
}

static __rte_always_inline void
cpt_free_asym_session_parameters(struct cpt_asym_sess_misc *sess)
{
	struct rte_crypto_modex_xform *mod;
	struct rte_crypto_rsa_xform *rsa;

	switch (sess->xfrm_type) {
	case RTE_CRYPTO_ASYM_XFORM_RSA:
		rsa = &sess->rsa_ctx;
		if (rsa->n.data)
			rte_free(rsa->n.data);
		break;
	case RTE_CRYPTO_ASYM_XFORM_MODEX:
		mod = &sess->mod_ctx;
		if (mod->modulus.data)
			rte_free(mod->modulus.data);
		break;
	case RTE_CRYPTO_ASYM_XFORM_ECDSA:
		/* Fall through */
	case RTE_CRYPTO_ASYM_XFORM_ECPM:
		break;
	default:
		CPT_LOG_DP_ERR("Invalid transform type");
		break;
	}
}

static __rte_always_inline void
cpt_fill_req_comp_addr(struct cpt_request_info *req, buf_ptr_t addr)
{
	void *completion_addr = RTE_PTR_ALIGN(addr.vaddr, 16);

	/* Pointer to cpt_res_s, updated by CPT */
	req->completion_addr = (volatile uint64_t *)completion_addr;
	req->comp_baddr = addr.dma_addr +
			  RTE_PTR_DIFF(completion_addr, addr.vaddr);
	*(req->completion_addr) = COMPLETION_CODE_INIT;
}

static __rte_always_inline int
cpt_modex_prep(struct asym_op_params *modex_params,
	       struct rte_crypto_modex_xform *mod)
{
	struct cpt_request_info *req = modex_params->req;
	phys_addr_t mphys = modex_params->meta_buf;
	uint32_t exp_len = mod->exponent.length;
	uint32_t mod_len = mod->modulus.length;
	struct rte_crypto_mod_op_param mod_op;
	struct rte_crypto_op **op;
	vq_cmd_word0_t vq_cmd_w0;
	uint64_t total_key_len;
	uint32_t dlen, rlen;
	uint32_t base_len;
	buf_ptr_t caddr;
	uint8_t *dptr;

	/* Extracting modex op form params->req->op[1]->asym->modex */
	op = RTE_PTR_ADD(req->op, sizeof(uintptr_t));
	mod_op = ((struct rte_crypto_op *)*op)->asym->modex;

	base_len = mod_op.base.length;
	if (unlikely(base_len > mod_len)) {
		CPT_LOG_DP_ERR("Base length greater than modulus length is not supported");
		(*op)->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -ENOTSUP;
	}

	total_key_len = mod_len + exp_len;

	/* Input buffer */
	dptr = RTE_PTR_ADD(req, sizeof(struct cpt_request_info));
	memcpy(dptr, mod->modulus.data, total_key_len);
	dptr += total_key_len;
	memcpy(dptr, mod_op.base.data, base_len);
	dptr += base_len;
	dlen = total_key_len + base_len;

	/* Result buffer */
	rlen = mod_len;

	/* Setup opcodes */
	vq_cmd_w0.s.opcode.major = CPT_MAJOR_OP_MODEX;
	vq_cmd_w0.s.opcode.minor = CPT_MINOR_OP_MODEX;

	/* GP op header */
	vq_cmd_w0.s.param1 = mod_len;
	vq_cmd_w0.s.param2 = exp_len;
	vq_cmd_w0.s.dlen = dlen;

	/* Filling cpt_request_info structure */
	req->ist.ei0 = vq_cmd_w0.u64;
	req->ist.ei1 = mphys;
	req->ist.ei2 = mphys + dlen;

	/* Result pointer to store result data */
	req->rptr = dptr;

	/* alternate_caddr to write completion status of the microcode */
	req->alternate_caddr = (uint64_t *)(dptr + rlen);
	*req->alternate_caddr = ~((uint64_t)COMPLETION_CODE_INIT);

	/* Preparing completion addr, +1 for completion code */
	caddr.vaddr = dptr + rlen + 1;
	caddr.dma_addr = mphys + dlen + rlen + 1;

	cpt_fill_req_comp_addr(req, caddr);
	return 0;
}

static __rte_always_inline void
cpt_rsa_prep(struct asym_op_params *rsa_params,
	     struct rte_crypto_rsa_xform *rsa,
	     rte_crypto_param *crypto_param)
{
	struct cpt_request_info *req = rsa_params->req;
	phys_addr_t mphys = rsa_params->meta_buf;
	struct rte_crypto_rsa_op_param rsa_op;
	uint32_t mod_len = rsa->n.length;
	uint32_t exp_len = rsa->e.length;
	struct rte_crypto_op **op;
	vq_cmd_word0_t vq_cmd_w0;
	uint64_t total_key_len;
	uint32_t dlen, rlen;
	uint32_t in_size;
	buf_ptr_t caddr;
	uint8_t *dptr;

	/* Extracting rsa op form params->req->op[1]->asym->rsa */
	op = RTE_PTR_ADD(req->op, sizeof(uintptr_t));
	rsa_op = ((struct rte_crypto_op *)*op)->asym->rsa;
	total_key_len  = mod_len + exp_len;

	/* Input buffer */
	dptr = RTE_PTR_ADD(req, sizeof(struct cpt_request_info));
	memcpy(dptr, rsa->n.data, total_key_len);
	dptr += total_key_len;

	in_size = crypto_param->length;
	memcpy(dptr, crypto_param->data, in_size);

	dptr += in_size;
	dlen = total_key_len + in_size;

	/* Result buffer */
	rlen = mod_len;

	if (rsa_op.pad == RTE_CRYPTO_RSA_PADDING_NONE) {
		/* Use mod_exp operation for no_padding type */
		vq_cmd_w0.s.opcode.minor = CPT_MINOR_OP_MODEX;
		vq_cmd_w0.s.param2 = exp_len;
	} else {
		if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT) {
			vq_cmd_w0.s.opcode.minor = CPT_MINOR_OP_PKCS_ENC;
			/* Public key encrypt, use BT2*/
			vq_cmd_w0.s.param2 = CPT_BLOCK_TYPE2 |
					((uint16_t)(exp_len) << 1);
		} else if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_VERIFY) {
			vq_cmd_w0.s.opcode.minor = CPT_MINOR_OP_PKCS_DEC;
			/* Public key decrypt, use BT1 */
			vq_cmd_w0.s.param2 = CPT_BLOCK_TYPE1;
			/* + 2 for decrypted len */
			rlen += 2;
		}
	}

	vq_cmd_w0.s.opcode.major = CPT_MAJOR_OP_MODEX;

	/* GP op header */
	vq_cmd_w0.s.param1 = mod_len;
	vq_cmd_w0.s.dlen = dlen;

	/* Filling cpt_request_info structure */
	req->ist.ei0 = vq_cmd_w0.u64;
	req->ist.ei1 = mphys;
	req->ist.ei2 = mphys + dlen;

	/* Result pointer to store result data */
	req->rptr = dptr;

	/* alternate_caddr to write completion status of the microcode */
	req->alternate_caddr = (uint64_t *)(dptr + rlen);
	*req->alternate_caddr = ~((uint64_t)COMPLETION_CODE_INIT);

	/* Preparing completion addr, +1 for completion code */
	caddr.vaddr = dptr + rlen + 1;
	caddr.dma_addr = mphys + dlen + rlen + 1;

	cpt_fill_req_comp_addr(req, caddr);
}

static __rte_always_inline void
cpt_rsa_crt_prep(struct asym_op_params *rsa_params,
		 struct rte_crypto_rsa_xform *rsa,
		 rte_crypto_param *crypto_param)
{
	struct cpt_request_info *req = rsa_params->req;
	phys_addr_t mphys = rsa_params->meta_buf;
	uint32_t qInv_len = rsa->qt.qInv.length;
	struct rte_crypto_rsa_op_param rsa_op;
	uint32_t dP_len = rsa->qt.dP.length;
	uint32_t dQ_len = rsa->qt.dQ.length;
	uint32_t p_len = rsa->qt.p.length;
	uint32_t q_len = rsa->qt.q.length;
	uint32_t mod_len = rsa->n.length;
	struct rte_crypto_op **op;
	vq_cmd_word0_t vq_cmd_w0;
	uint64_t total_key_len;
	uint32_t dlen, rlen;
	uint32_t in_size;
	buf_ptr_t caddr;
	uint8_t *dptr;

	/* Extracting rsa op form params->req->op[1]->asym->rsa */
	op = RTE_PTR_ADD(req->op, sizeof(uintptr_t));
	rsa_op = ((struct rte_crypto_op *)*op)->asym->rsa;
	total_key_len = p_len + q_len + dP_len + dQ_len + qInv_len;

	/* Input buffer */
	dptr = RTE_PTR_ADD(req, sizeof(struct cpt_request_info));
	memcpy(dptr, rsa->qt.q.data, total_key_len);
	dptr += total_key_len;

	in_size = crypto_param->length;
	memcpy(dptr, crypto_param->data, in_size);

	dptr += in_size;
	dlen = total_key_len + in_size;

	/* Result buffer */
	rlen = mod_len;

	if (rsa_op.pad == RTE_CRYPTO_RSA_PADDING_NONE) {
		/*Use mod_exp operation for no_padding type */
		vq_cmd_w0.s.opcode.minor = CPT_MINOR_OP_MODEX_CRT;
	} else {
		if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_SIGN) {
			vq_cmd_w0.s.opcode.minor = CPT_MINOR_OP_PKCS_ENC_CRT;
			/* Private encrypt, use BT1 */
			vq_cmd_w0.s.param2 = CPT_BLOCK_TYPE1;
		} else if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_DECRYPT) {
			vq_cmd_w0.s.opcode.minor = CPT_MINOR_OP_PKCS_DEC_CRT;
			/* Private decrypt, use BT2 */
			vq_cmd_w0.s.param2 = CPT_BLOCK_TYPE2;
			/* + 2 for decrypted len */
			rlen += 2;
		}
	}

	vq_cmd_w0.s.opcode.major = CPT_MAJOR_OP_MODEX;

	/* GP op header */
	vq_cmd_w0.s.param1 = mod_len;
	vq_cmd_w0.s.dlen = dlen;

	/* Filling cpt_request_info structure */
	req->ist.ei0 = vq_cmd_w0.u64;
	req->ist.ei1 = mphys;
	req->ist.ei2 = mphys + dlen;

	/* Result pointer to store result data */
	req->rptr = dptr;

	/* alternate_caddr to write completion status of the microcode */
	req->alternate_caddr = (uint64_t *)(dptr + rlen);
	*req->alternate_caddr = ~((uint64_t)COMPLETION_CODE_INIT);

	/* Preparing completion addr, +1 for completion code */
	caddr.vaddr = dptr + rlen + 1;
	caddr.dma_addr = mphys + dlen + rlen + 1;

	cpt_fill_req_comp_addr(req, caddr);
}

static __rte_always_inline int __rte_hot
cpt_enqueue_rsa_op(struct rte_crypto_op *op,
	       struct asym_op_params *params,
	       struct cpt_asym_sess_misc *sess)
{
	struct rte_crypto_rsa_op_param *rsa = &op->asym->rsa;

	switch (rsa->op_type) {
	case RTE_CRYPTO_ASYM_OP_VERIFY:
		cpt_rsa_prep(params, &sess->rsa_ctx, &rsa->sign);
		break;
	case RTE_CRYPTO_ASYM_OP_ENCRYPT:
		cpt_rsa_prep(params, &sess->rsa_ctx, &rsa->message);
		break;
	case RTE_CRYPTO_ASYM_OP_SIGN:
		cpt_rsa_crt_prep(params, &sess->rsa_ctx, &rsa->message);
		break;
	case RTE_CRYPTO_ASYM_OP_DECRYPT:
		cpt_rsa_crt_prep(params, &sess->rsa_ctx, &rsa->cipher);
		break;
	default:
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}
	return 0;
}

static const struct cpt_ec_group ec_grp[CPT_EC_ID_PMAX] = {
	{
		.prime = {
				.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
				.length = 24,
			},
		.order = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0x99, 0xDE, 0xF8, 0x36, 0x14, 0x6B,
				   0xC9, 0xB1, 0xB4, 0xD2, 0x28, 0x31},
			  .length = 24},
		.consta = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				    0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF,
				    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC},
			   .length = 24},
		.constb = {.data = {0x64, 0x21, 0x05, 0x19, 0xE5, 0x9C,
				    0x80, 0xE7, 0x0F, 0xA7, 0xE9, 0xAB,
				    0x72, 0x24, 0x30, 0x49, 0xFE, 0xB8,
				    0xDE, 0xEC, 0xC1, 0x46, 0xB9, 0xB1},
			   .length = 24},
	},
	{
		.prime = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
			  .length = 28},
		.order = {.data = {0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
				   0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF, 0XFF,
				   0X16, 0XA2, 0XE0, 0XB8, 0XF0, 0X3E, 0X13,
				   0XDD, 0X29, 0X45, 0X5C, 0X5C, 0X2A, 0X3D},
			  .length = 28},
		.consta = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				    0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE},
			   .length = 28},
		.constb = {.data = {0xB4, 0x05, 0x0A, 0x85, 0x0C, 0x04, 0xB3,
				    0xAB, 0xF5, 0x41, 0x32, 0x56, 0x50, 0x44,
				    0xB0, 0xB7, 0xD7, 0xBF, 0xD8, 0xBA, 0x27,
				    0x0B, 0x39, 0x43, 0x23, 0x55, 0xFF, 0xB4},
			   .length = 28},
	},
	{
		.prime = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
				   0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xFF},
			  .length = 32},
		.order = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
				   0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0xBC, 0xE6, 0xFA, 0xAD, 0xA7,
				   0x17, 0x9E, 0x84, 0xF3, 0xB9, 0xCA, 0xC2,
				   0xFC, 0x63, 0x25, 0x51},
			  .length = 32},
		.consta = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00,
				    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
				    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				    0xFF, 0xFF, 0xFF, 0xFC},
			   .length = 32},
		.constb = {.data = {0x5A, 0xC6, 0x35, 0xD8, 0xAA, 0x3A, 0x93,
				    0xE7, 0xB3, 0xEB, 0xBD, 0x55, 0x76, 0x98,
				    0x86, 0xBC, 0x65, 0x1D, 0x06, 0xB0, 0xCC,
				    0x53, 0xB0, 0xF6, 0x3B, 0xCE, 0x3C, 0x3E,
				    0x27, 0xD2, 0x60, 0x4B},
			   .length = 32},
	},
	{
		.prime = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF,
				   0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF},
			  .length = 48},
		.order = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xC7, 0x63, 0x4D, 0x81,
				   0xF4, 0x37, 0x2D, 0xDF, 0x58, 0x1A, 0x0D,
				   0xB2, 0x48, 0xB0, 0xA7, 0x7A, 0xEC, 0xEC,
				   0x19, 0x6A, 0xCC, 0xC5, 0x29, 0x73},
			  .length = 48},
		.consta = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				    0xFF, 0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF,
				    0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				    0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFC},
			   .length = 48},
		.constb = {.data = {0xB3, 0x31, 0x2F, 0xA7, 0xE2, 0x3E, 0xE7,
				    0xE4, 0x98, 0x8E, 0x05, 0x6B, 0xE3, 0xF8,
				    0x2D, 0x19, 0x18, 0x1D, 0x9C, 0x6E, 0xFE,
				    0x81, 0x41, 0x12, 0x03, 0x14, 0x08, 0x8F,
				    0x50, 0x13, 0x87, 0x5A, 0xC6, 0x56, 0x39,
				    0x8D, 0x8A, 0x2E, 0xD1, 0x9D, 0x2A, 0x85,
				    0xC8, 0xED, 0xD3, 0xEC, 0x2A, 0xEF},
			   .length = 48},
	},
	{.prime = {.data = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFF},
		   .length = 66},
	 .order = {.data = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			    0xFF, 0xFA, 0x51, 0x86, 0x87, 0x83, 0xBF, 0x2F,
			    0x96, 0x6B, 0x7F, 0xCC, 0x01, 0x48, 0xF7, 0x09,
			    0xA5, 0xD0, 0x3B, 0xB5, 0xC9, 0xB8, 0x89, 0x9C,
			    0x47, 0xAE, 0xBB, 0x6F, 0xB7, 0x1E, 0x91, 0x38,
			    0x64, 0x09},
		   .length = 66},
	 .consta = {.data = {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			     0xFF, 0xFC},
		    .length = 66},
	 .constb = {.data = {0x00, 0x51, 0x95, 0x3E, 0xB9, 0x61, 0x8E, 0x1C,
			     0x9A, 0x1F, 0x92, 0x9A, 0x21, 0xA0, 0xB6, 0x85,
			     0x40, 0xEE, 0xA2, 0xDA, 0x72, 0x5B, 0x99, 0xB3,
			     0x15, 0xF3, 0xB8, 0xB4, 0x89, 0x91, 0x8E, 0xF1,
			     0x09, 0xE1, 0x56, 0x19, 0x39, 0x51, 0xEC, 0x7E,
			     0x93, 0x7B, 0x16, 0x52, 0xC0, 0xBD, 0x3B, 0xB1,
			     0xBF, 0x07, 0x35, 0x73, 0xDF, 0x88, 0x3D, 0x2C,
			     0x34, 0xF1, 0xEF, 0x45, 0x1F, 0xD4, 0x6B, 0x50,
			     0x3F, 0x00},
		    .length = 66}}};

static __rte_always_inline void
cpt_ecdsa_sign_prep(struct rte_crypto_ecdsa_op_param *ecdsa,
		    struct asym_op_params *ecdsa_params,
		    uint64_t fpm_table_iova,
		    uint8_t curveid)
{
	struct cpt_request_info *req = ecdsa_params->req;
	uint16_t message_len = ecdsa->message.length;
	phys_addr_t mphys = ecdsa_params->meta_buf;
	uint16_t pkey_len = ecdsa->pkey.length;
	uint16_t p_align, k_align, m_align;
	uint16_t k_len = ecdsa->k.length;
	uint16_t order_len, prime_len;
	uint16_t o_offset, pk_offset;
	vq_cmd_word0_t vq_cmd_w0;
	uint16_t rlen, dlen;
	buf_ptr_t caddr;
	uint8_t *dptr;

	prime_len = ec_grp[curveid].prime.length;
	order_len = ec_grp[curveid].order.length;

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
	dptr = RTE_PTR_ADD(req, sizeof(struct cpt_request_info));

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

	memcpy(dptr, ec_grp[curveid].prime.data, prime_len);
	dptr += p_align;

	memcpy(dptr + o_offset, ec_grp[curveid].order.data, order_len);
	dptr += p_align;

	memcpy(dptr + pk_offset, ecdsa->pkey.data, pkey_len);
	dptr += p_align;

	memcpy(dptr, ecdsa->message.data, message_len);
	dptr += m_align;

	memcpy(dptr, ec_grp[curveid].consta.data, prime_len);
	dptr += p_align;

	memcpy(dptr, ec_grp[curveid].constb.data, prime_len);
	dptr += p_align;

	/* 2 * prime length (for sign r and s ) */
	rlen = 2 * p_align;

	/* Setup opcodes */
	vq_cmd_w0.s.opcode.major = CPT_MAJOR_OP_ECDSA;
	vq_cmd_w0.s.opcode.minor = CPT_MINOR_OP_ECDSA_SIGN;

	/* GP op header */
	vq_cmd_w0.s.param1 = curveid | (message_len << 8);
	vq_cmd_w0.s.param2 = (pkey_len << 8) | k_len;
	vq_cmd_w0.s.dlen = dlen;

	/* Filling cpt_request_info structure */
	req->ist.ei0 = vq_cmd_w0.u64;
	req->ist.ei1 = mphys;
	req->ist.ei2 = mphys + dlen;

	/* Result pointer to store result data */
	req->rptr = dptr;

	/* alternate_caddr to write completion status of the microcode */
	req->alternate_caddr = (uint64_t *)(dptr + rlen);
	*req->alternate_caddr = ~((uint64_t)COMPLETION_CODE_INIT);

	/* Preparing completion addr, +1 for completion code */
	caddr.vaddr = dptr + rlen + 1;
	caddr.dma_addr = mphys + dlen + rlen + 1;

	cpt_fill_req_comp_addr(req, caddr);
}

static __rte_always_inline void
cpt_ecdsa_verify_prep(struct rte_crypto_ecdsa_op_param *ecdsa,
		      struct asym_op_params *ecdsa_params,
		      uint64_t fpm_table_iova,
		      uint8_t curveid)
{
	struct cpt_request_info *req = ecdsa_params->req;
	uint32_t message_len = ecdsa->message.length;
	phys_addr_t mphys = ecdsa_params->meta_buf;
	uint16_t o_offset, r_offset, s_offset;
	uint16_t qx_len = ecdsa->q.x.length;
	uint16_t qy_len = ecdsa->q.y.length;
	uint16_t r_len = ecdsa->r.length;
	uint16_t s_len = ecdsa->s.length;
	uint16_t order_len, prime_len;
	uint16_t qx_offset, qy_offset;
	uint16_t p_align, m_align;
	vq_cmd_word0_t vq_cmd_w0;
	buf_ptr_t caddr;
	uint16_t dlen;
	uint8_t *dptr;

	prime_len = ec_grp[curveid].prime.length;
	order_len = ec_grp[curveid].order.length;

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
	dptr = RTE_PTR_ADD(req, sizeof(struct cpt_request_info));

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

	memcpy(dptr + o_offset, ec_grp[curveid].order.data, order_len);
	dptr += p_align;

	memcpy(dptr, ec_grp[curveid].prime.data, prime_len);
	dptr += p_align;

	memcpy(dptr + qx_offset, ecdsa->q.x.data, qx_len);
	dptr += p_align;

	memcpy(dptr + qy_offset, ecdsa->q.y.data, qy_len);
	dptr += p_align;

	memcpy(dptr, ec_grp[curveid].consta.data, prime_len);
	dptr += p_align;

	memcpy(dptr, ec_grp[curveid].constb.data, prime_len);
	dptr += p_align;

	/* Setup opcodes */
	vq_cmd_w0.s.opcode.major = CPT_MAJOR_OP_ECDSA;
	vq_cmd_w0.s.opcode.minor = CPT_MINOR_OP_ECDSA_VERIFY;

	/* GP op header */
	vq_cmd_w0.s.param1 = curveid | (message_len << 8);
	vq_cmd_w0.s.param2 = 0;
	vq_cmd_w0.s.dlen = dlen;

	/* Filling cpt_request_info structure */
	req->ist.ei0 = vq_cmd_w0.u64;
	req->ist.ei1 = mphys;
	req->ist.ei2 = mphys + dlen;

	/* Result pointer to store result data */
	req->rptr = dptr;

	/* alternate_caddr to write completion status of the microcode */
	req->alternate_caddr = (uint64_t *)dptr;
	*req->alternate_caddr = ~((uint64_t)COMPLETION_CODE_INIT);

	/* Preparing completion addr, +1 for completion code */
	caddr.vaddr = dptr + 1;
	caddr.dma_addr = mphys + dlen + 1;

	cpt_fill_req_comp_addr(req, caddr);
}

static __rte_always_inline int __rte_hot
cpt_enqueue_ecdsa_op(struct rte_crypto_op *op,
		     struct asym_op_params *params,
		     struct cpt_asym_sess_misc *sess,
		     uint64_t *fpm_iova)
{
	struct rte_crypto_ecdsa_op_param *ecdsa = &op->asym->ecdsa;
	uint8_t curveid = sess->ec_ctx.curveid;

	if (ecdsa->op_type == RTE_CRYPTO_ASYM_OP_SIGN)
		cpt_ecdsa_sign_prep(ecdsa, params, fpm_iova[curveid], curveid);
	else if (ecdsa->op_type == RTE_CRYPTO_ASYM_OP_VERIFY)
		cpt_ecdsa_verify_prep(ecdsa, params, fpm_iova[curveid],
				      curveid);
	else {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return -EINVAL;
	}
	return 0;
}

static __rte_always_inline int
cpt_ecpm_prep(struct rte_crypto_ecpm_op_param *ecpm,
	      struct asym_op_params *asym_params,
	      uint8_t curveid)
{
	struct cpt_request_info *req = asym_params->req;
	phys_addr_t mphys = asym_params->meta_buf;
	uint16_t x1_len = ecpm->p.x.length;
	uint16_t y1_len = ecpm->p.y.length;
	uint16_t scalar_align, p_align;
	uint16_t dlen, rlen, prime_len;
	uint16_t x1_offset, y1_offset;
	vq_cmd_word0_t vq_cmd_w0;
	buf_ptr_t caddr;
	uint8_t *dptr;

	prime_len = ec_grp[curveid].prime.length;

	/* Input buffer */
	dptr = RTE_PTR_ADD(req, sizeof(struct cpt_request_info));

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
	memcpy(dptr, ec_grp[curveid].prime.data, ec_grp[curveid].prime.length);
	dptr += p_align;

	memcpy(dptr, ec_grp[curveid].consta.data,
	       ec_grp[curveid].consta.length);
	dptr += p_align;

	memcpy(dptr, ec_grp[curveid].constb.data,
	       ec_grp[curveid].constb.length);
	dptr += p_align;

	/* Setup opcodes */
	vq_cmd_w0.s.opcode.major = CPT_MAJOR_OP_ECC;
	vq_cmd_w0.s.opcode.minor = CPT_MINOR_OP_ECC_UMP;

	/* GP op header */
	vq_cmd_w0.s.param1 = curveid;
	vq_cmd_w0.s.param2 = ecpm->scalar.length;
	vq_cmd_w0.s.dlen = dlen;

	/* Filling cpt_request_info structure */
	req->ist.ei0 = vq_cmd_w0.u64;
	req->ist.ei1 = mphys;
	req->ist.ei2 = mphys + dlen;

	/* Result buffer will store output point where length of
	 * each coordinate will be of prime length, thus set
	 * rlen to twice of prime length.
	 */
	rlen = p_align << 1;
	req->rptr = dptr;

	/* alternate_caddr to write completion status by the microcode */
	req->alternate_caddr = (uint64_t *)(dptr + rlen);
	*req->alternate_caddr = ~((uint64_t)COMPLETION_CODE_INIT);

	/* Preparing completion addr, +1 for completion code */
	caddr.vaddr = dptr + rlen + 1;
	caddr.dma_addr = mphys + dlen + rlen + 1;

	cpt_fill_req_comp_addr(req, caddr);
	return 0;
}
#endif /* _CPT_UCODE_ASYM_H_ */
