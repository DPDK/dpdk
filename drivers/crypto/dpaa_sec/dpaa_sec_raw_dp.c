/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 NXP
 */

#include <rte_byteorder.h>
#include <rte_common.h>
#include <cryptodev_pmd.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>
#ifdef RTE_LIB_SECURITY
#include <rte_security_driver.h>
#endif

/* RTA header files */
#include <desc/algo.h>
#include <desc/ipsec.h>

#include <rte_dpaa_bus.h>
#include <dpaa_sec.h>
#include <dpaa_sec_log.h>

struct dpaa_sec_raw_dp_ctx {
	dpaa_sec_session *session;
	uint32_t tail;
	uint32_t head;
	uint16_t cached_enqueue;
	uint16_t cached_dequeue;
};

static inline int
is_encode(dpaa_sec_session *ses)
{
	return ses->dir == DIR_ENC;
}

static inline int is_decode(dpaa_sec_session *ses)
{
	return ses->dir == DIR_DEC;
}

static __rte_always_inline int
dpaa_sec_raw_enqueue_done(void *qp_data, uint8_t *drv_ctx, uint32_t n)
{
	RTE_SET_USED(qp_data);
	RTE_SET_USED(drv_ctx);
	RTE_SET_USED(n);

	return 0;
}

static __rte_always_inline int
dpaa_sec_raw_dequeue_done(void *qp_data, uint8_t *drv_ctx, uint32_t n)
{
	RTE_SET_USED(qp_data);
	RTE_SET_USED(drv_ctx);
	RTE_SET_USED(n);

	return 0;
}

static inline struct dpaa_sec_op_ctx *
dpaa_sec_alloc_raw_ctx(dpaa_sec_session *ses, int sg_count)
{
	struct dpaa_sec_op_ctx *ctx;
	int i, retval;

	retval = rte_mempool_get(
			ses->qp[rte_lcore_id() % MAX_DPAA_CORES]->ctx_pool,
			(void **)(&ctx));
	if (!ctx || retval) {
		DPAA_SEC_DP_WARN("Alloc sec descriptor failed!");
		return NULL;
	}
	/*
	 * Clear SG memory. There are 16 SG entries of 16 Bytes each.
	 * one call to dcbz_64() clear 64 bytes, hence calling it 4 times
	 * to clear all the SG entries. dpaa_sec_alloc_ctx() is called for
	 * each packet, memset is costlier than dcbz_64().
	 */
	for (i = 0; i < sg_count && i < MAX_JOB_SG_ENTRIES; i += 4)
		dcbz_64(&ctx->job.sg[i]);

	ctx->ctx_pool = ses->qp[rte_lcore_id() % MAX_DPAA_CORES]->ctx_pool;
	ctx->vtop_offset = (size_t) ctx - rte_mempool_virt2iova(ctx);

	return ctx;
}

static struct dpaa_sec_job *
build_dpaa_raw_dp_auth_fd(uint8_t *drv_ctx,
			struct rte_crypto_sgl *sgl,
			struct rte_crypto_sgl *dest_sgl,
			struct rte_crypto_va_iova_ptr *iv,
			struct rte_crypto_va_iova_ptr *digest,
			struct rte_crypto_va_iova_ptr *auth_iv,
			union rte_crypto_sym_ofs ofs,
			void *userdata,
			struct qm_fd *fd)
{
	RTE_SET_USED(dest_sgl);
	RTE_SET_USED(iv);
	RTE_SET_USED(auth_iv);
	RTE_SET_USED(fd);

	dpaa_sec_session *ses =
		((struct dpaa_sec_raw_dp_ctx *)drv_ctx)->session;
	struct dpaa_sec_job *cf;
	struct dpaa_sec_op_ctx *ctx;
	struct qm_sg_entry *sg, *out_sg, *in_sg;
	phys_addr_t start_addr;
	uint8_t *old_digest, extra_segs;
	int data_len, data_offset, total_len = 0;
	unsigned int i;

	for (i = 0; i < sgl->num; i++)
		total_len += sgl->vec[i].len;

	data_len = total_len - ofs.ofs.auth.head - ofs.ofs.auth.tail;
	data_offset =  ofs.ofs.auth.head;

	/* Support only length in bits for SNOW3G and ZUC */

	if (is_decode(ses))
		extra_segs = 3;
	else
		extra_segs = 2;

	if (sgl->num > MAX_SG_ENTRIES) {
		DPAA_SEC_DP_ERR("Auth: Max sec segs supported is %d",
				MAX_SG_ENTRIES);
		return NULL;
	}
	ctx = dpaa_sec_alloc_raw_ctx(ses, sgl->num * 2 + extra_segs);
	if (!ctx)
		return NULL;

	cf = &ctx->job;
	ctx->userdata = (void *)userdata;
	old_digest = ctx->digest;

	/* output */
	out_sg = &cf->sg[0];
	qm_sg_entry_set64(out_sg, digest->iova);
	out_sg->length = ses->digest_length;
	cpu_to_hw_sg(out_sg);

	/* input */
	in_sg = &cf->sg[1];
	/* need to extend the input to a compound frame */
	in_sg->extension = 1;
	in_sg->final = 1;
	in_sg->length = data_len;
	qm_sg_entry_set64(in_sg, rte_dpaa_mem_vtop(&cf->sg[2]));

	/* 1st seg */
	sg = in_sg + 1;

	if (ses->iv.length) {
		uint8_t *iv_ptr;

		iv_ptr = rte_crypto_op_ctod_offset(userdata, uint8_t *,
						   ses->iv.offset);

		if (ses->auth_alg == RTE_CRYPTO_AUTH_SNOW3G_UIA2) {
			iv_ptr = conv_to_snow_f9_iv(iv_ptr);
			sg->length = 12;
		} else if (ses->auth_alg == RTE_CRYPTO_AUTH_ZUC_EIA3) {
			iv_ptr = conv_to_zuc_eia_iv(iv_ptr);
			sg->length = 8;
		} else {
			sg->length = ses->iv.length;
		}
		qm_sg_entry_set64(sg, rte_dpaa_mem_vtop(iv_ptr));
		in_sg->length += sg->length;
		cpu_to_hw_sg(sg);
		sg++;
	}

	qm_sg_entry_set64(sg, sgl->vec[0].iova);
	sg->offset = data_offset;

	if (data_len <= (int)(sgl->vec[0].len - data_offset)) {
		sg->length = data_len;
	} else {
		sg->length = sgl->vec[0].len - data_offset;

		/* remaining i/p segs */
		for (i = 1; i < sgl->num; i++) {
			cpu_to_hw_sg(sg);
			sg++;
			qm_sg_entry_set64(sg, sgl->vec[i].iova);
			if (data_len > (int)sgl->vec[i].len)
				sg->length = sgl->vec[0].len;
			else
				sg->length = data_len;

			data_len = data_len - sg->length;
			if (data_len < 1)
				break;
		}
	}

	if (is_decode(ses)) {
		/* Digest verification case */
		cpu_to_hw_sg(sg);
		sg++;
		rte_memcpy(old_digest, digest->va,
				ses->digest_length);
		start_addr = rte_dpaa_mem_vtop(old_digest);
		qm_sg_entry_set64(sg, start_addr);
		sg->length = ses->digest_length;
		in_sg->length += ses->digest_length;
	}
	sg->final = 1;
	cpu_to_hw_sg(sg);
	cpu_to_hw_sg(in_sg);

	return cf;
}

static inline struct dpaa_sec_job *
build_raw_cipher_auth_gcm_sg(uint8_t *drv_ctx,
			struct rte_crypto_sgl *sgl,
			struct rte_crypto_sgl *dest_sgl,
			struct rte_crypto_va_iova_ptr *iv,
			struct rte_crypto_va_iova_ptr *digest,
			struct rte_crypto_va_iova_ptr *auth_iv,
			union rte_crypto_sym_ofs ofs,
			void *userdata,
			struct qm_fd *fd)
{
	dpaa_sec_session *ses =
		((struct dpaa_sec_raw_dp_ctx *)drv_ctx)->session;
	struct dpaa_sec_job *cf;
	struct dpaa_sec_op_ctx *ctx;
	struct qm_sg_entry *sg, *out_sg, *in_sg;
	uint8_t extra_req_segs;
	uint8_t *IV_ptr = iv->va;
	int data_len = 0, aead_len = 0;
	unsigned int i;

	for (i = 0; i < sgl->num; i++)
		data_len += sgl->vec[i].len;

	extra_req_segs = 4;
	aead_len = data_len - ofs.ofs.cipher.head - ofs.ofs.cipher.tail;

	if (ses->auth_only_len)
		extra_req_segs++;

	if (sgl->num > MAX_SG_ENTRIES) {
		DPAA_SEC_DP_ERR("AEAD: Max sec segs supported is %d",
				MAX_SG_ENTRIES);
		return NULL;
	}

	ctx = dpaa_sec_alloc_raw_ctx(ses,  sgl->num * 2 + extra_req_segs);
	if (!ctx)
		return NULL;

	cf = &ctx->job;
	ctx->userdata = (void *)userdata;

	rte_prefetch0(cf->sg);

	/* output */
	out_sg = &cf->sg[0];
	out_sg->extension = 1;
	if (is_encode(ses))
		out_sg->length = aead_len + ses->digest_length;
	else
		out_sg->length = aead_len;

	/* output sg entries */
	sg = &cf->sg[2];
	qm_sg_entry_set64(out_sg, rte_dpaa_mem_vtop(sg));
	cpu_to_hw_sg(out_sg);

	if (dest_sgl) {
		/* 1st seg */
		qm_sg_entry_set64(sg, dest_sgl->vec[0].iova);
		sg->length = dest_sgl->vec[0].len - ofs.ofs.cipher.head;
		sg->offset = ofs.ofs.cipher.head;

		/* Successive segs */
		for (i = 1; i < dest_sgl->num; i++) {
			cpu_to_hw_sg(sg);
			sg++;
			qm_sg_entry_set64(sg, dest_sgl->vec[i].iova);
			sg->length = dest_sgl->vec[i].len;
		}
	} else {
		/* 1st seg */
		qm_sg_entry_set64(sg, sgl->vec[0].iova);
		sg->length = sgl->vec[0].len - ofs.ofs.cipher.head;
		sg->offset = ofs.ofs.cipher.head;

		/* Successive segs */
		for (i = 1; i < sgl->num; i++) {
			cpu_to_hw_sg(sg);
			sg++;
			qm_sg_entry_set64(sg, sgl->vec[i].iova);
			sg->length = sgl->vec[i].len;
		}

	}

	if (is_encode(ses)) {
		cpu_to_hw_sg(sg);
		/* set auth output */
		sg++;
		qm_sg_entry_set64(sg, digest->iova);
		sg->length = ses->digest_length;
	}
	sg->final = 1;
	cpu_to_hw_sg(sg);

	/* input */
	in_sg = &cf->sg[1];
	in_sg->extension = 1;
	in_sg->final = 1;
	if (is_encode(ses))
		in_sg->length = ses->iv.length + aead_len
						+ ses->auth_only_len;
	else
		in_sg->length = ses->iv.length + aead_len
				+ ses->auth_only_len + ses->digest_length;

	/* input sg entries */
	sg++;
	qm_sg_entry_set64(in_sg, rte_dpaa_mem_vtop(sg));
	cpu_to_hw_sg(in_sg);

	/* 1st seg IV */
	qm_sg_entry_set64(sg, rte_dpaa_mem_vtop(IV_ptr));
	sg->length = ses->iv.length;
	cpu_to_hw_sg(sg);

	/* 2 seg auth only */
	if (ses->auth_only_len) {
		sg++;
		qm_sg_entry_set64(sg, auth_iv->iova);
		sg->length = ses->auth_only_len;
		cpu_to_hw_sg(sg);
	}

	/* 3rd seg */
	sg++;
	qm_sg_entry_set64(sg, sgl->vec[0].iova);
	sg->length = sgl->vec[0].len - ofs.ofs.cipher.head;
	sg->offset = ofs.ofs.cipher.head;

	/* Successive segs */
	for (i = 1; i < sgl->num; i++) {
		cpu_to_hw_sg(sg);
		sg++;
		qm_sg_entry_set64(sg, sgl->vec[i].iova);
		sg->length =  sgl->vec[i].len;
	}

	if (is_decode(ses)) {
		cpu_to_hw_sg(sg);
		sg++;
		memcpy(ctx->digest, digest->va,
			ses->digest_length);
		qm_sg_entry_set64(sg, rte_dpaa_mem_vtop(ctx->digest));
		sg->length = ses->digest_length;
	}
	sg->final = 1;
	cpu_to_hw_sg(sg);

	if (ses->auth_only_len)
		fd->cmd = 0x80000000 | ses->auth_only_len;

	return cf;
}

static inline struct dpaa_sec_job *
build_dpaa_raw_dp_chain_fd(uint8_t *drv_ctx,
			struct rte_crypto_sgl *sgl,
			struct rte_crypto_sgl *dest_sgl,
			struct rte_crypto_va_iova_ptr *iv,
			struct rte_crypto_va_iova_ptr *digest,
			struct rte_crypto_va_iova_ptr *auth_iv,
			union rte_crypto_sym_ofs ofs,
			void *userdata,
			struct qm_fd *fd)
{
	RTE_SET_USED(auth_iv);

	dpaa_sec_session *ses =
		((struct dpaa_sec_raw_dp_ctx *)drv_ctx)->session;
	struct dpaa_sec_job *cf;
	struct dpaa_sec_op_ctx *ctx;
	struct qm_sg_entry *sg, *out_sg, *in_sg;
	uint8_t *IV_ptr = iv->va;
	unsigned int i;
	uint16_t auth_hdr_len = ofs.ofs.cipher.head -
				ofs.ofs.auth.head;
	uint16_t auth_tail_len = ofs.ofs.auth.tail;
	uint32_t auth_only_len = (auth_tail_len << 16) | auth_hdr_len;
	int data_len = 0, auth_len = 0, cipher_len = 0;

	for (i = 0; i < sgl->num; i++)
		data_len += sgl->vec[i].len;

	cipher_len = data_len - ofs.ofs.cipher.head - ofs.ofs.cipher.tail;
	auth_len = data_len - ofs.ofs.auth.head - ofs.ofs.auth.tail;

	if (sgl->num > MAX_SG_ENTRIES) {
		DPAA_SEC_DP_ERR("Cipher-Auth: Max sec segs supported is %d",
				MAX_SG_ENTRIES);
		return NULL;
	}

	ctx = dpaa_sec_alloc_raw_ctx(ses, sgl->num * 2 + 4);
	if (!ctx)
		return NULL;

	cf = &ctx->job;
	ctx->userdata = (void *)userdata;

	rte_prefetch0(cf->sg);

	/* output */
	out_sg = &cf->sg[0];
	out_sg->extension = 1;
	if (is_encode(ses))
		out_sg->length = cipher_len + ses->digest_length;
	else
		out_sg->length = cipher_len;

	/* output sg entries */
	sg = &cf->sg[2];
	qm_sg_entry_set64(out_sg, rte_dpaa_mem_vtop(sg));
	cpu_to_hw_sg(out_sg);

	/* 1st seg */
	if (dest_sgl) {
		qm_sg_entry_set64(sg, dest_sgl->vec[0].iova);
		sg->length = dest_sgl->vec[0].len - ofs.ofs.cipher.head;
		sg->offset = ofs.ofs.cipher.head;

		/* Successive segs */
		for (i = 1; i < dest_sgl->num; i++) {
			cpu_to_hw_sg(sg);
			sg++;
			qm_sg_entry_set64(sg, dest_sgl->vec[i].iova);
			sg->length = dest_sgl->vec[i].len;
		}
	} else {
		qm_sg_entry_set64(sg, sgl->vec[0].iova);
		sg->length = sgl->vec[0].len - ofs.ofs.cipher.head;
		sg->offset = ofs.ofs.cipher.head;

		/* Successive segs */
		for (i = 1; i < sgl->num; i++) {
			cpu_to_hw_sg(sg);
			sg++;
			qm_sg_entry_set64(sg, sgl->vec[i].iova);
			sg->length = sgl->vec[i].len;
		}
	}

	if (is_encode(ses)) {
		cpu_to_hw_sg(sg);
		/* set auth output */
		sg++;
		qm_sg_entry_set64(sg, digest->iova);
		sg->length = ses->digest_length;
	}
	sg->final = 1;
	cpu_to_hw_sg(sg);

	/* input */
	in_sg = &cf->sg[1];
	in_sg->extension = 1;
	in_sg->final = 1;
	if (is_encode(ses))
		in_sg->length = ses->iv.length + auth_len;
	else
		in_sg->length = ses->iv.length + auth_len
						+ ses->digest_length;

	/* input sg entries */
	sg++;
	qm_sg_entry_set64(in_sg, rte_dpaa_mem_vtop(sg));
	cpu_to_hw_sg(in_sg);

	/* 1st seg IV */
	qm_sg_entry_set64(sg, rte_dpaa_mem_vtop(IV_ptr));
	sg->length = ses->iv.length;
	cpu_to_hw_sg(sg);

	/* 2 seg */
	sg++;
	qm_sg_entry_set64(sg, sgl->vec[0].iova);
	sg->length = sgl->vec[0].len - ofs.ofs.auth.head;
	sg->offset = ofs.ofs.auth.head;

	/* Successive segs */
	for (i = 1; i < sgl->num; i++) {
		cpu_to_hw_sg(sg);
		sg++;
		qm_sg_entry_set64(sg, sgl->vec[i].iova);
		sg->length = sgl->vec[i].len;
	}

	if (is_decode(ses)) {
		cpu_to_hw_sg(sg);
		sg++;
		memcpy(ctx->digest, digest->va,
			ses->digest_length);
		qm_sg_entry_set64(sg, rte_dpaa_mem_vtop(ctx->digest));
		sg->length = ses->digest_length;
	}
	sg->final = 1;
	cpu_to_hw_sg(sg);

	if (auth_only_len)
		fd->cmd = 0x80000000 | auth_only_len;

	return cf;
}

static struct dpaa_sec_job *
build_dpaa_raw_dp_cipher_fd(uint8_t *drv_ctx,
			struct rte_crypto_sgl *sgl,
			struct rte_crypto_sgl *dest_sgl,
			struct rte_crypto_va_iova_ptr *iv,
			struct rte_crypto_va_iova_ptr *digest,
			struct rte_crypto_va_iova_ptr *auth_iv,
			union rte_crypto_sym_ofs ofs,
			void *userdata,
			struct qm_fd *fd)
{
	RTE_SET_USED(digest);
	RTE_SET_USED(auth_iv);
	RTE_SET_USED(fd);

	dpaa_sec_session *ses =
		((struct dpaa_sec_raw_dp_ctx *)drv_ctx)->session;
	struct dpaa_sec_job *cf;
	struct dpaa_sec_op_ctx *ctx;
	struct qm_sg_entry *sg, *out_sg, *in_sg;
	unsigned int i;
	uint8_t *IV_ptr = iv->va;
	int data_len, total_len = 0, data_offset;

	for (i = 0; i < sgl->num; i++)
		total_len += sgl->vec[i].len;

	data_len = total_len - ofs.ofs.cipher.head - ofs.ofs.cipher.tail;
	data_offset = ofs.ofs.cipher.head;

	/* Support lengths in bits only for SNOW3G and ZUC */
	if (sgl->num > MAX_SG_ENTRIES) {
		DPAA_SEC_DP_ERR("Cipher: Max sec segs supported is %d",
				MAX_SG_ENTRIES);
		return NULL;
	}

	ctx = dpaa_sec_alloc_raw_ctx(ses, sgl->num * 2 + 3);
	if (!ctx)
		return NULL;

	cf = &ctx->job;
	ctx->userdata = (void *)userdata;

	/* output */
	out_sg = &cf->sg[0];
	out_sg->extension = 1;
	out_sg->length = data_len;
	qm_sg_entry_set64(out_sg, rte_dpaa_mem_vtop(&cf->sg[2]));
	cpu_to_hw_sg(out_sg);

	if (dest_sgl) {
		/* 1st seg */
		sg = &cf->sg[2];
		qm_sg_entry_set64(sg, dest_sgl->vec[0].iova);
		sg->length = dest_sgl->vec[0].len - data_offset;
		sg->offset = data_offset;

		/* Successive segs */
		for (i = 1; i < dest_sgl->num; i++) {
			cpu_to_hw_sg(sg);
			sg++;
			qm_sg_entry_set64(sg, dest_sgl->vec[i].iova);
			sg->length = dest_sgl->vec[i].len;
		}
	} else {
		/* 1st seg */
		sg = &cf->sg[2];
		qm_sg_entry_set64(sg, sgl->vec[0].iova);
		sg->length = sgl->vec[0].len - data_offset;
		sg->offset = data_offset;

		/* Successive segs */
		for (i = 1; i < sgl->num; i++) {
			cpu_to_hw_sg(sg);
			sg++;
			qm_sg_entry_set64(sg, sgl->vec[i].iova);
			sg->length = sgl->vec[i].len;
		}

	}
	sg->final = 1;
	cpu_to_hw_sg(sg);

	/* input */
	in_sg = &cf->sg[1];
	in_sg->extension = 1;
	in_sg->final = 1;
	in_sg->length = data_len + ses->iv.length;

	sg++;
	qm_sg_entry_set64(in_sg, rte_dpaa_mem_vtop(sg));
	cpu_to_hw_sg(in_sg);

	/* IV */
	qm_sg_entry_set64(sg, rte_dpaa_mem_vtop(IV_ptr));
	sg->length = ses->iv.length;
	cpu_to_hw_sg(sg);

	/* 1st seg */
	sg++;
	qm_sg_entry_set64(sg, sgl->vec[0].iova);
	sg->length = sgl->vec[0].len - data_offset;
	sg->offset = data_offset;

	/* Successive segs */
	for (i = 1; i < sgl->num; i++) {
		cpu_to_hw_sg(sg);
		sg++;
		qm_sg_entry_set64(sg, sgl->vec[i].iova);
		sg->length = sgl->vec[i].len;
	}
	sg->final = 1;
	cpu_to_hw_sg(sg);

	return cf;
}

#ifdef RTE_LIBRTE_SECURITY
static inline struct dpaa_sec_job *
build_dpaa_raw_proto_sg(uint8_t *drv_ctx,
			struct rte_crypto_sgl *sgl,
			struct rte_crypto_sgl *dest_sgl,
			struct rte_crypto_va_iova_ptr *iv,
			struct rte_crypto_va_iova_ptr *digest,
			struct rte_crypto_va_iova_ptr *auth_iv,
			union rte_crypto_sym_ofs ofs,
			void *userdata,
			struct qm_fd *fd)
{
	RTE_SET_USED(iv);
	RTE_SET_USED(digest);
	RTE_SET_USED(auth_iv);
	RTE_SET_USED(ofs);

	dpaa_sec_session *ses =
		((struct dpaa_sec_raw_dp_ctx *)drv_ctx)->session;
	struct dpaa_sec_job *cf;
	struct dpaa_sec_op_ctx *ctx;
	struct qm_sg_entry *sg, *out_sg, *in_sg;
	uint32_t in_len = 0, out_len = 0;
	unsigned int i;

	if (sgl->num > MAX_SG_ENTRIES) {
		DPAA_SEC_DP_ERR("Proto: Max sec segs supported is %d",
				MAX_SG_ENTRIES);
		return NULL;
	}

	ctx = dpaa_sec_alloc_raw_ctx(ses, sgl->num * 2 + 4);
	if (!ctx)
		return NULL;
	cf = &ctx->job;
	ctx->userdata = (void *)userdata;
	/* output */
	out_sg = &cf->sg[0];
	out_sg->extension = 1;
	qm_sg_entry_set64(out_sg, rte_dpaa_mem_vtop(&cf->sg[2]));

	if (dest_sgl) {
		/* 1st seg */
		sg = &cf->sg[2];
		qm_sg_entry_set64(sg, dest_sgl->vec[0].iova);
		sg->offset = 0;
		sg->length = dest_sgl->vec[0].len;
		out_len += sg->length;

		for (i = 1; i < dest_sgl->num; i++) {
		/* Successive segs */
			cpu_to_hw_sg(sg);
			sg++;
			qm_sg_entry_set64(sg, dest_sgl->vec[i].iova);
			sg->offset = 0;
			sg->length = dest_sgl->vec[i].len;
			out_len += sg->length;
		}
		sg->length = dest_sgl->vec[i - 1].tot_len;
	} else {
		/* 1st seg */
		sg = &cf->sg[2];
		qm_sg_entry_set64(sg, sgl->vec[0].iova);
		sg->offset = 0;
		sg->length = sgl->vec[0].len;
		out_len += sg->length;

		for (i = 1; i < sgl->num; i++) {
		/* Successive segs */
			cpu_to_hw_sg(sg);
			sg++;
			qm_sg_entry_set64(sg, sgl->vec[i].iova);
			sg->offset = 0;
			sg->length = sgl->vec[i].len;
			out_len += sg->length;
		}
		sg->length = sgl->vec[i - 1].tot_len;

	}
	out_len += sg->length;
	sg->final = 1;
	cpu_to_hw_sg(sg);

	out_sg->length = out_len;
	cpu_to_hw_sg(out_sg);

	/* input */
	in_sg = &cf->sg[1];
	in_sg->extension = 1;
	in_sg->final = 1;
	in_len = sgl->vec[0].len;

	sg++;
	qm_sg_entry_set64(in_sg, rte_dpaa_mem_vtop(sg));

	/* 1st seg */
	qm_sg_entry_set64(sg, sgl->vec[0].iova);
	sg->length = sgl->vec[0].len;
	sg->offset = 0;

	/* Successive segs */
	for (i = 1; i < sgl->num; i++) {
		cpu_to_hw_sg(sg);
		sg++;
		qm_sg_entry_set64(sg, sgl->vec[i].iova);
		sg->length = sgl->vec[i].len;
		sg->offset = 0;
		in_len += sg->length;
	}
	sg->final = 1;
	cpu_to_hw_sg(sg);

	in_sg->length = in_len;
	cpu_to_hw_sg(in_sg);

	if ((ses->ctxt == DPAA_SEC_PDCP) && ses->pdcp.hfn_ovd) {
		fd->cmd = 0x80000000 |
			*((uint32_t *)((uint8_t *)userdata +
			ses->pdcp.hfn_ovd_offset));
		DPAA_SEC_DP_DEBUG("Per packet HFN: %x, ovd:%u\n",
			*((uint32_t *)((uint8_t *)userdata +
			ses->pdcp.hfn_ovd_offset)),
			ses->pdcp.hfn_ovd);
	}

	return cf;
}
#endif

static uint32_t
dpaa_sec_raw_enqueue_burst(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_sym_vec *vec, union rte_crypto_sym_ofs ofs,
	void *user_data[], int *status)
{
	/* Function to transmit the frames to given device and queuepair */
	uint32_t loop;
	struct dpaa_sec_qp *dpaa_qp = (struct dpaa_sec_qp *)qp_data;
	uint16_t num_tx = 0;
	struct qm_fd fds[DPAA_SEC_BURST], *fd;
	uint32_t frames_to_send;
	struct dpaa_sec_job *cf;
	dpaa_sec_session *ses =
			((struct dpaa_sec_raw_dp_ctx *)drv_ctx)->session;
	uint32_t flags[DPAA_SEC_BURST] = {0};
	struct qman_fq *inq[DPAA_SEC_BURST];

	if (unlikely(!DPAA_PER_LCORE_PORTAL)) {
		if (rte_dpaa_portal_init((void *)0)) {
			DPAA_SEC_ERR("Failure in affining portal");
			return 0;
		}
	}

	while (vec->num) {
		frames_to_send = (vec->num > DPAA_SEC_BURST) ?
				DPAA_SEC_BURST : vec->num;
		for (loop = 0; loop < frames_to_send; loop++) {
			if (unlikely(!ses->qp[rte_lcore_id() % MAX_DPAA_CORES])) {
				if (dpaa_sec_attach_sess_q(dpaa_qp, ses)) {
					frames_to_send = loop;
					goto send_pkts;
				}
			} else if (unlikely(ses->qp[rte_lcore_id() %
						MAX_DPAA_CORES] != dpaa_qp)) {
				DPAA_SEC_DP_ERR("Old:sess->qp = %p"
					" New qp = %p\n",
					ses->qp[rte_lcore_id() %
					MAX_DPAA_CORES], dpaa_qp);
				frames_to_send = loop;
				goto send_pkts;
			}

			/*Clear the unused FD fields before sending*/
			fd = &fds[loop];
			memset(fd, 0, sizeof(struct qm_fd));
			cf = ses->build_raw_dp_fd(drv_ctx,
						&vec->src_sgl[loop],
						&vec->dest_sgl[loop],
						&vec->iv[loop],
						&vec->digest[loop],
						&vec->auth_iv[loop],
						ofs,
						user_data[loop],
						fd);
			if (!cf) {
				DPAA_SEC_ERR("error: Improper packet contents"
					" for crypto operation");
				goto skip_tx;
			}
			inq[loop] = ses->inq[rte_lcore_id() % MAX_DPAA_CORES];
			qm_fd_addr_set64(fd, rte_dpaa_mem_vtop(cf->sg));
			fd->_format1 = qm_fd_compound;
			fd->length29 = 2 * sizeof(struct qm_sg_entry);

			status[loop] = 1;
		}
send_pkts:
		loop = 0;
		while (loop < frames_to_send) {
			loop += qman_enqueue_multi_fq(&inq[loop], &fds[loop],
					&flags[loop], frames_to_send - loop);
		}
		vec->num -= frames_to_send;
		num_tx += frames_to_send;
	}

skip_tx:
	dpaa_qp->tx_pkts += num_tx;
	dpaa_qp->tx_errs += vec->num - num_tx;

	return num_tx;
}

static int
dpaa_sec_deq_raw(struct dpaa_sec_qp *qp, void **out_user_data,
		uint8_t is_user_data_array,
		rte_cryptodev_raw_post_dequeue_t post_dequeue,
		int nb_ops)
{
	struct qman_fq *fq;
	unsigned int pkts = 0;
	int num_rx_bufs, ret;
	struct qm_dqrr_entry *dq;
	uint32_t vdqcr_flags = 0;
	uint8_t is_success = 0;

	fq = &qp->outq;
	/*
	 * Until request for four buffers, we provide exact number of buffers.
	 * Otherwise we do not set the QM_VDQCR_EXACT flag.
	 * Not setting QM_VDQCR_EXACT flag can provide two more buffers than
	 * requested, so we request two less in this case.
	 */
	if (nb_ops < 4) {
		vdqcr_flags = QM_VDQCR_EXACT;
		num_rx_bufs = nb_ops;
	} else {
		num_rx_bufs = nb_ops > DPAA_MAX_DEQUEUE_NUM_FRAMES ?
			(DPAA_MAX_DEQUEUE_NUM_FRAMES - 2) : (nb_ops - 2);
	}
	ret = qman_set_vdq(fq, num_rx_bufs, vdqcr_flags);
	if (ret)
		return 0;

	do {
		const struct qm_fd *fd;
		struct dpaa_sec_job *job;
		struct dpaa_sec_op_ctx *ctx;

		dq = qman_dequeue(fq);
		if (!dq)
			continue;

		fd = &dq->fd;
		/* sg is embedded in an op ctx,
		 * sg[0] is for output
		 * sg[1] for input
		 */
		job = rte_dpaa_mem_ptov(qm_fd_addr_get64(fd));

		ctx = container_of(job, struct dpaa_sec_op_ctx, job);
		ctx->fd_status = fd->status;
		if (is_user_data_array)
			out_user_data[pkts] = ctx->userdata;
		else
			out_user_data[0] = ctx->userdata;

		if (!ctx->fd_status) {
			is_success = true;
		} else {
			is_success = false;
			DPAA_SEC_DP_WARN("SEC return err:0x%x", ctx->fd_status);
		}
		post_dequeue(ctx->op, pkts, is_success);
		pkts++;

		/* report op status to sym->op and then free the ctx memory */
		rte_mempool_put(ctx->ctx_pool, (void *)ctx);

		qman_dqrr_consume(fq, dq);
	} while (fq->flags & QMAN_FQ_STATE_VDQCR);

	return pkts;
}


static __rte_always_inline uint32_t
dpaa_sec_raw_dequeue_burst(void *qp_data, uint8_t *drv_ctx,
	rte_cryptodev_raw_get_dequeue_count_t get_dequeue_count,
	uint32_t max_nb_to_dequeue,
	rte_cryptodev_raw_post_dequeue_t post_dequeue,
	void **out_user_data, uint8_t is_user_data_array,
	uint32_t *n_success, int *dequeue_status)
{
	RTE_SET_USED(drv_ctx);
	RTE_SET_USED(get_dequeue_count);
	uint16_t num_rx;
	struct dpaa_sec_qp *dpaa_qp = (struct dpaa_sec_qp *)qp_data;
	uint32_t nb_ops = max_nb_to_dequeue;

	if (unlikely(!DPAA_PER_LCORE_PORTAL)) {
		if (rte_dpaa_portal_init((void *)0)) {
			DPAA_SEC_ERR("Failure in affining portal");
			return 0;
		}
	}

	num_rx = dpaa_sec_deq_raw(dpaa_qp, out_user_data,
			is_user_data_array, post_dequeue, nb_ops);

	dpaa_qp->rx_pkts += num_rx;
	*dequeue_status = 1;
	*n_success = num_rx;

	DPAA_SEC_DP_DEBUG("SEC Received %d Packets\n", num_rx);

	return num_rx;
}

static __rte_always_inline int
dpaa_sec_raw_enqueue(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_vec *data_vec,
	uint16_t n_data_vecs, union rte_crypto_sym_ofs ofs,
	struct rte_crypto_va_iova_ptr *iv,
	struct rte_crypto_va_iova_ptr *digest,
	struct rte_crypto_va_iova_ptr *aad_or_auth_iv,
	void *user_data)
{
	RTE_SET_USED(qp_data);
	RTE_SET_USED(drv_ctx);
	RTE_SET_USED(data_vec);
	RTE_SET_USED(n_data_vecs);
	RTE_SET_USED(ofs);
	RTE_SET_USED(iv);
	RTE_SET_USED(digest);
	RTE_SET_USED(aad_or_auth_iv);
	RTE_SET_USED(user_data);

	return 0;
}

static __rte_always_inline void *
dpaa_sec_raw_dequeue(void *qp_data, uint8_t *drv_ctx, int *dequeue_status,
	enum rte_crypto_op_status *op_status)
{
	RTE_SET_USED(qp_data);
	RTE_SET_USED(drv_ctx);
	RTE_SET_USED(dequeue_status);
	RTE_SET_USED(op_status);

	return NULL;
}

int
dpaa_sec_configure_raw_dp_ctx(struct rte_cryptodev *dev, uint16_t qp_id,
	struct rte_crypto_raw_dp_ctx *raw_dp_ctx,
	enum rte_crypto_op_sess_type sess_type,
	union rte_cryptodev_session_ctx session_ctx, uint8_t is_update)
{
	dpaa_sec_session *sess;
	struct dpaa_sec_raw_dp_ctx *dp_ctx;
	RTE_SET_USED(qp_id);

	if (!is_update) {
		memset(raw_dp_ctx, 0, sizeof(*raw_dp_ctx));
		raw_dp_ctx->qp_data = dev->data->queue_pairs[qp_id];
	}

	if (sess_type == RTE_CRYPTO_OP_SECURITY_SESSION)
		sess = (dpaa_sec_session *)get_sec_session_private_data(
				session_ctx.sec_sess);
	else if (sess_type == RTE_CRYPTO_OP_WITH_SESSION)
		sess = (dpaa_sec_session *)get_sym_session_private_data(
			session_ctx.crypto_sess, dpaa_cryptodev_driver_id);
	else
		return -ENOTSUP;
	raw_dp_ctx->dequeue_burst = dpaa_sec_raw_dequeue_burst;
	raw_dp_ctx->dequeue = dpaa_sec_raw_dequeue;
	raw_dp_ctx->dequeue_done = dpaa_sec_raw_dequeue_done;
	raw_dp_ctx->enqueue_burst = dpaa_sec_raw_enqueue_burst;
	raw_dp_ctx->enqueue = dpaa_sec_raw_enqueue;
	raw_dp_ctx->enqueue_done = dpaa_sec_raw_enqueue_done;

	if (sess->ctxt == DPAA_SEC_CIPHER)
		sess->build_raw_dp_fd = build_dpaa_raw_dp_cipher_fd;
	else if (sess->ctxt == DPAA_SEC_AUTH)
		sess->build_raw_dp_fd = build_dpaa_raw_dp_auth_fd;
	else if (sess->ctxt == DPAA_SEC_CIPHER_HASH)
		sess->build_raw_dp_fd = build_dpaa_raw_dp_chain_fd;
	else if (sess->ctxt == DPAA_SEC_AEAD)
		sess->build_raw_dp_fd = build_raw_cipher_auth_gcm_sg;
#ifdef RTE_LIBRTE_SECURITY
	else if (sess->ctxt == DPAA_SEC_IPSEC ||
			sess->ctxt == DPAA_SEC_PDCP)
		sess->build_raw_dp_fd = build_dpaa_raw_proto_sg;
#endif
	else
		return -ENOTSUP;
	dp_ctx = (struct dpaa_sec_raw_dp_ctx *)raw_dp_ctx->drv_ctx_data;
	dp_ctx->session = sess;

	return 0;
}

int
dpaa_sec_get_dp_ctx_size(__rte_unused struct rte_cryptodev *dev)
{
	return sizeof(struct dpaa_sec_raw_dp_ctx);
}
