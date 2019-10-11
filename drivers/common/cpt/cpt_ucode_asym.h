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
	opcode_info_t opcode;
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
	opcode.s.major = CPT_MAJOR_OP_MODEX;
	opcode.s.minor = CPT_MINOR_OP_MODEX;
	vq_cmd_w0.s.opcode = opcode.flags;

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
	opcode_info_t opcode;
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
		opcode.s.minor = CPT_MINOR_OP_MODEX;
		vq_cmd_w0.s.param2 = exp_len;
	} else {
		if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_ENCRYPT) {
			opcode.s.minor = CPT_MINOR_OP_PKCS_ENC;
			/* Public key encrypt, use BT2*/
			vq_cmd_w0.s.param2 = CPT_BLOCK_TYPE2 |
					((uint16_t)(exp_len) << 1);
		} else if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_VERIFY) {
			opcode.s.minor = CPT_MINOR_OP_PKCS_DEC;
			/* Public key decrypt, use BT1 */
			vq_cmd_w0.s.param2 = CPT_BLOCK_TYPE1;
			/* + 2 for decrypted len */
			rlen += 2;
		}
	}

	/* Setup opcodes */
	opcode.s.major = CPT_MAJOR_OP_MODEX;
	vq_cmd_w0.s.opcode = opcode.flags;

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
	opcode_info_t opcode;
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
		opcode.s.minor = CPT_MINOR_OP_MODEX_CRT;
	} else {
		if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_SIGN) {
			opcode.s.minor = CPT_MINOR_OP_PKCS_ENC_CRT;
			/* Private encrypt, use BT1 */
			vq_cmd_w0.s.param2 = CPT_BLOCK_TYPE1;
		} else if (rsa_op.op_type == RTE_CRYPTO_ASYM_OP_DECRYPT) {
			opcode.s.minor = CPT_MINOR_OP_PKCS_DEC_CRT;
			/* Private decrypt, use BT2 */
			vq_cmd_w0.s.param2 = CPT_BLOCK_TYPE2;
			/* + 2 for decrypted len */
			rlen += 2;
		}
	}

	/* Setup opcodes */
	opcode.s.major = CPT_MAJOR_OP_MODEX;
	vq_cmd_w0.s.opcode = opcode.flags;

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

static __rte_always_inline int __hot
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
#endif /* _CPT_UCODE_ASYM_H_ */
