/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 NXP
 */

#include <cryptodev_pmd.h>
#include <rte_fslmc.h>
#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_dpio.h>

#include "dpaa2_sec_priv.h"
#include "dpaa2_sec_logs.h"

#include <desc/algo.h>

struct dpaa2_sec_raw_dp_ctx {
	dpaa2_sec_session *session;
	uint32_t tail;
	uint32_t head;
	uint16_t cached_enqueue;
	uint16_t cached_dequeue;
};

static int
build_raw_dp_chain_fd(uint8_t *drv_ctx,
		       struct rte_crypto_sgl *sgl,
		       struct rte_crypto_sgl *dest_sgl,
		       struct rte_crypto_va_iova_ptr *iv,
		       struct rte_crypto_va_iova_ptr *digest,
		       struct rte_crypto_va_iova_ptr *auth_iv,
		       union rte_crypto_sym_ofs ofs,
		       void *userdata,
		       struct qbman_fd *fd)
{
	RTE_SET_USED(auth_iv);

	dpaa2_sec_session *sess =
		((struct dpaa2_sec_raw_dp_ctx *)drv_ctx)->session;
	struct ctxt_priv *priv = sess->ctxt;
	struct qbman_fle *fle, *sge, *ip_fle, *op_fle;
	struct sec_flow_context *flc;
	int data_len = 0, auth_len = 0, cipher_len = 0;
	unsigned int i = 0;
	uint16_t auth_hdr_len = ofs.ofs.cipher.head -
				ofs.ofs.auth.head;

	uint16_t auth_tail_len = ofs.ofs.auth.tail;
	uint32_t auth_only_len = (auth_tail_len << 16) | auth_hdr_len;
	int icv_len = sess->digest_length;
	uint8_t *old_icv;
	uint8_t *iv_ptr = iv->va;

	for (i = 0; i < sgl->num; i++)
		data_len += sgl->vec[i].len;

	cipher_len = data_len - ofs.ofs.cipher.head - ofs.ofs.cipher.tail;
	auth_len = data_len - ofs.ofs.auth.head - ofs.ofs.auth.tail;
	/* first FLE entry used to store session ctxt */
	fle = (struct qbman_fle *)rte_malloc(NULL,
			FLE_SG_MEM_SIZE(2 * sgl->num),
			RTE_CACHE_LINE_SIZE);
	if (unlikely(!fle)) {
		DPAA2_SEC_ERR("AUTHENC SG: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_SG_MEM_SIZE(2 * sgl->num));
	DPAA2_SET_FLE_ADDR(fle, (size_t)userdata);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);

	op_fle = fle + 1;
	ip_fle = fle + 2;
	sge = fle + 3;

	/* Save the shared descriptor */
	flc = &priv->flc_desc[0].flc;

	/* Configure FD as a FRAME LIST */
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(op_fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	/* Configure Output FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_SG_EXT(op_fle);
	DPAA2_SET_FLE_ADDR(op_fle, DPAA2_VADDR_TO_IOVA(sge));

	if (auth_only_len)
		DPAA2_SET_FLE_INTERNAL_JD(op_fle, auth_only_len);

	op_fle->length = (sess->dir == DIR_ENC) ?
			(cipher_len + icv_len) :
			cipher_len;

	/* OOP */
	if (dest_sgl) {
		/* Configure Output SGE for Encap/Decap */
		DPAA2_SET_FLE_ADDR(sge, dest_sgl->vec[0].iova);
		DPAA2_SET_FLE_OFFSET(sge, ofs.ofs.cipher.head);
		sge->length = dest_sgl->vec[0].len - ofs.ofs.cipher.head;

		/* o/p segs */
		for (i = 1; i < dest_sgl->num; i++) {
			sge++;
			DPAA2_SET_FLE_ADDR(sge, dest_sgl->vec[i].iova);
			DPAA2_SET_FLE_OFFSET(sge, 0);
			sge->length = dest_sgl->vec[i].len;
		}
	} else {
		/* Configure Output SGE for Encap/Decap */
		DPAA2_SET_FLE_ADDR(sge, sgl->vec[0].iova);
		DPAA2_SET_FLE_OFFSET(sge, ofs.ofs.cipher.head);
		sge->length = sgl->vec[0].len - ofs.ofs.cipher.head;

		/* o/p segs */
		for (i = 1; i < sgl->num; i++) {
			sge++;
			DPAA2_SET_FLE_ADDR(sge, sgl->vec[i].iova);
			DPAA2_SET_FLE_OFFSET(sge, 0);
			sge->length = sgl->vec[i].len;
		}
	}

	if (sess->dir == DIR_ENC) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge,
			digest->iova);
		sge->length = icv_len;
	}
	DPAA2_SET_FLE_FIN(sge);

	sge++;

	/* Configure Input FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_ADDR(ip_fle, DPAA2_VADDR_TO_IOVA(sge));
	DPAA2_SET_FLE_SG_EXT(ip_fle);
	DPAA2_SET_FLE_FIN(ip_fle);

	ip_fle->length = (sess->dir == DIR_ENC) ?
			(auth_len + sess->iv.length) :
			(auth_len + sess->iv.length +
			icv_len);

	/* Configure Input SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(iv_ptr));
	sge->length = sess->iv.length;

	sge++;
	DPAA2_SET_FLE_ADDR(sge, sgl->vec[0].iova);
	DPAA2_SET_FLE_OFFSET(sge, ofs.ofs.auth.head);
	sge->length = sgl->vec[0].len - ofs.ofs.auth.head;

	for (i = 1; i < sgl->num; i++) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, sgl->vec[i].iova);
		DPAA2_SET_FLE_OFFSET(sge, 0);
		sge->length = sgl->vec[i].len;
	}

	if (sess->dir == DIR_DEC) {
		sge++;
		old_icv = (uint8_t *)(sge + 1);
		memcpy(old_icv, digest->va,
			icv_len);
		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(old_icv));
		sge->length = icv_len;
	}

	DPAA2_SET_FLE_FIN(sge);
	if (auth_only_len) {
		DPAA2_SET_FLE_INTERNAL_JD(ip_fle, auth_only_len);
		DPAA2_SET_FD_INTERNAL_JD(fd, auth_only_len);
	}
	DPAA2_SET_FD_LEN(fd, ip_fle->length);

	return 0;
}

static int
build_raw_dp_aead_fd(uint8_t *drv_ctx,
		       struct rte_crypto_sgl *sgl,
		       struct rte_crypto_sgl *dest_sgl,
		       struct rte_crypto_va_iova_ptr *iv,
		       struct rte_crypto_va_iova_ptr *digest,
		       struct rte_crypto_va_iova_ptr *auth_iv,
		       union rte_crypto_sym_ofs ofs,
		       void *userdata,
		       struct qbman_fd *fd)
{
	dpaa2_sec_session *sess =
		((struct dpaa2_sec_raw_dp_ctx *)drv_ctx)->session;
	struct ctxt_priv *priv = sess->ctxt;
	struct qbman_fle *fle, *sge, *ip_fle, *op_fle;
	struct sec_flow_context *flc;
	uint32_t auth_only_len = sess->ext_params.aead_ctxt.auth_only_len;
	int icv_len = sess->digest_length;
	uint8_t *old_icv;
	uint8_t *IV_ptr = iv->va;
	unsigned int i = 0;
	int data_len = 0, aead_len = 0;

	for (i = 0; i < sgl->num; i++)
		data_len += sgl->vec[i].len;

	aead_len = data_len - ofs.ofs.cipher.head - ofs.ofs.cipher.tail;

	/* first FLE entry used to store mbuf and session ctxt */
	fle = (struct qbman_fle *)rte_malloc(NULL,
			FLE_SG_MEM_SIZE(2 * sgl->num),
			RTE_CACHE_LINE_SIZE);
	if (unlikely(!fle)) {
		DPAA2_SEC_ERR("GCM SG: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_SG_MEM_SIZE(2 * sgl->num));
	DPAA2_SET_FLE_ADDR(fle, (size_t)userdata);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);

	op_fle = fle + 1;
	ip_fle = fle + 2;
	sge = fle + 3;

	/* Save the shared descriptor */
	flc = &priv->flc_desc[0].flc;

	/* Configure FD as a FRAME LIST */
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(op_fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	/* Configure Output FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_SG_EXT(op_fle);
	DPAA2_SET_FLE_ADDR(op_fle, DPAA2_VADDR_TO_IOVA(sge));

	if (auth_only_len)
		DPAA2_SET_FLE_INTERNAL_JD(op_fle, auth_only_len);

	op_fle->length = (sess->dir == DIR_ENC) ?
			(aead_len + icv_len) :
			aead_len;

	/* OOP */
	if (dest_sgl) {
		/* Configure Output SGE for Encap/Decap */
		DPAA2_SET_FLE_ADDR(sge, dest_sgl->vec[0].iova);
		DPAA2_SET_FLE_OFFSET(sge, ofs.ofs.cipher.head);
		sge->length = dest_sgl->vec[0].len - ofs.ofs.cipher.head;

		/* o/p segs */
		for (i = 1; i < dest_sgl->num; i++) {
			sge++;
			DPAA2_SET_FLE_ADDR(sge, dest_sgl->vec[i].iova);
			DPAA2_SET_FLE_OFFSET(sge, 0);
			sge->length = dest_sgl->vec[i].len;
		}
	} else {
		/* Configure Output SGE for Encap/Decap */
		DPAA2_SET_FLE_ADDR(sge, sgl->vec[0].iova);
		DPAA2_SET_FLE_OFFSET(sge, ofs.ofs.cipher.head);
		sge->length = sgl->vec[0].len - ofs.ofs.cipher.head;

		/* o/p segs */
		for (i = 1; i < sgl->num; i++) {
			sge++;
			DPAA2_SET_FLE_ADDR(sge, sgl->vec[i].iova);
			DPAA2_SET_FLE_OFFSET(sge, 0);
			sge->length = sgl->vec[i].len;
		}
	}

	if (sess->dir == DIR_ENC) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, digest->iova);
		sge->length = icv_len;
	}
	DPAA2_SET_FLE_FIN(sge);

	sge++;

	/* Configure Input FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_ADDR(ip_fle, DPAA2_VADDR_TO_IOVA(sge));
	DPAA2_SET_FLE_SG_EXT(ip_fle);
	DPAA2_SET_FLE_FIN(ip_fle);
	ip_fle->length = (sess->dir == DIR_ENC) ?
		(aead_len + sess->iv.length + auth_only_len) :
		(aead_len + sess->iv.length + auth_only_len +
		icv_len);

	/* Configure Input SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(IV_ptr));
	sge->length = sess->iv.length;

	sge++;
	if (auth_only_len) {
		DPAA2_SET_FLE_ADDR(sge, auth_iv->iova);
		sge->length = auth_only_len;
		sge++;
	}

	DPAA2_SET_FLE_ADDR(sge, sgl->vec[0].iova);
	DPAA2_SET_FLE_OFFSET(sge, ofs.ofs.cipher.head);
	sge->length = sgl->vec[0].len - ofs.ofs.cipher.head;

	/* i/p segs */
	for (i = 1; i < sgl->num; i++) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, sgl->vec[i].iova);
		DPAA2_SET_FLE_OFFSET(sge, 0);
		sge->length = sgl->vec[i].len;
	}

	if (sess->dir == DIR_DEC) {
		sge++;
		old_icv = (uint8_t *)(sge + 1);
		memcpy(old_icv,  digest->va, icv_len);
		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(old_icv));
		sge->length = icv_len;
	}

	DPAA2_SET_FLE_FIN(sge);
	if (auth_only_len) {
		DPAA2_SET_FLE_INTERNAL_JD(ip_fle, auth_only_len);
		DPAA2_SET_FD_INTERNAL_JD(fd, auth_only_len);
	}
	DPAA2_SET_FD_LEN(fd, ip_fle->length);

	return 0;
}

static int
build_raw_dp_auth_fd(uint8_t *drv_ctx,
		       struct rte_crypto_sgl *sgl,
		       struct rte_crypto_sgl *dest_sgl,
		       struct rte_crypto_va_iova_ptr *iv,
		       struct rte_crypto_va_iova_ptr *digest,
		       struct rte_crypto_va_iova_ptr *auth_iv,
		       union rte_crypto_sym_ofs ofs,
		       void *userdata,
		       struct qbman_fd *fd)
{
	RTE_SET_USED(iv);
	RTE_SET_USED(auth_iv);
	RTE_SET_USED(dest_sgl);

	dpaa2_sec_session *sess =
		((struct dpaa2_sec_raw_dp_ctx *)drv_ctx)->session;
	struct qbman_fle *fle, *sge, *ip_fle, *op_fle;
	struct sec_flow_context *flc;
	int total_len = 0, data_len = 0, data_offset;
	uint8_t *old_digest;
	struct ctxt_priv *priv = sess->ctxt;
	unsigned int i;

	for (i = 0; i < sgl->num; i++)
		total_len += sgl->vec[i].len;

	data_len = total_len - ofs.ofs.auth.head - ofs.ofs.auth.tail;
	data_offset = ofs.ofs.auth.head;

	/* For SNOW3G and ZUC, lengths in bits only supported */
	fle = (struct qbman_fle *)rte_malloc(NULL,
		FLE_SG_MEM_SIZE(2 * sgl->num),
			RTE_CACHE_LINE_SIZE);
	if (unlikely(!fle)) {
		DPAA2_SEC_ERR("AUTH SG: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_SG_MEM_SIZE(2*sgl->num));
	/* first FLE entry used to store mbuf and session ctxt */
	DPAA2_SET_FLE_ADDR(fle, (size_t)userdata);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);
	op_fle = fle + 1;
	ip_fle = fle + 2;
	sge = fle + 3;

	flc = &priv->flc_desc[DESC_INITFINAL].flc;

	/* sg FD */
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(op_fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);

	/* o/p fle */
	DPAA2_SET_FLE_ADDR(op_fle,
			DPAA2_VADDR_TO_IOVA(digest->va));
	op_fle->length = sess->digest_length;

	/* i/p fle */
	DPAA2_SET_FLE_SG_EXT(ip_fle);
	DPAA2_SET_FLE_ADDR(ip_fle, DPAA2_VADDR_TO_IOVA(sge));
	ip_fle->length = data_len;

	if (sess->iv.length) {
		uint8_t *iv_ptr;

		iv_ptr = rte_crypto_op_ctod_offset(userdata, uint8_t *,
						sess->iv.offset);

		if (sess->auth_alg == RTE_CRYPTO_AUTH_SNOW3G_UIA2) {
			iv_ptr = conv_to_snow_f9_iv(iv_ptr);
			sge->length = 12;
		} else if (sess->auth_alg == RTE_CRYPTO_AUTH_ZUC_EIA3) {
			iv_ptr = conv_to_zuc_eia_iv(iv_ptr);
			sge->length = 8;
		} else {
			sge->length = sess->iv.length;
		}
		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(iv_ptr));
		ip_fle->length += sge->length;
		sge++;
	}
	/* i/p 1st seg */
	DPAA2_SET_FLE_ADDR(sge, sgl->vec[0].iova);
	DPAA2_SET_FLE_OFFSET(sge, data_offset);

	if (data_len <= (int)(sgl->vec[0].len - data_offset)) {
		sge->length = data_len;
		data_len = 0;
	} else {
		sge->length = sgl->vec[0].len - data_offset;
		for (i = 1; i < sgl->num; i++) {
			sge++;
			DPAA2_SET_FLE_ADDR(sge, sgl->vec[i].iova);
			DPAA2_SET_FLE_OFFSET(sge, 0);
			sge->length = sgl->vec[i].len;
		}
	}
	if (sess->dir == DIR_DEC) {
		/* Digest verification case */
		sge++;
		old_digest = (uint8_t *)(sge + 1);
		rte_memcpy(old_digest, digest->va,
			sess->digest_length);
		DPAA2_SET_FLE_ADDR(sge, DPAA2_VADDR_TO_IOVA(old_digest));
		sge->length = sess->digest_length;
		ip_fle->length += sess->digest_length;
	}
	DPAA2_SET_FLE_FIN(sge);
	DPAA2_SET_FLE_FIN(ip_fle);
	DPAA2_SET_FD_LEN(fd, ip_fle->length);

	return 0;
}

static int
build_raw_dp_proto_fd(uint8_t *drv_ctx,
		       struct rte_crypto_sgl *sgl,
		       struct rte_crypto_sgl *dest_sgl,
		       struct rte_crypto_va_iova_ptr *iv,
		       struct rte_crypto_va_iova_ptr *digest,
		       struct rte_crypto_va_iova_ptr *auth_iv,
		       union rte_crypto_sym_ofs ofs,
		       void *userdata,
		       struct qbman_fd *fd)
{
	RTE_SET_USED(iv);
	RTE_SET_USED(digest);
	RTE_SET_USED(auth_iv);
	RTE_SET_USED(ofs);

	dpaa2_sec_session *sess =
		((struct dpaa2_sec_raw_dp_ctx *)drv_ctx)->session;
	struct ctxt_priv *priv = sess->ctxt;
	struct qbman_fle *fle, *sge, *ip_fle, *op_fle;
	struct sec_flow_context *flc;
	uint32_t in_len = 0, out_len = 0, i;

	/* first FLE entry used to store mbuf and session ctxt */
	fle = (struct qbman_fle *)rte_malloc(NULL,
			FLE_SG_MEM_SIZE(2 * sgl->num),
			RTE_CACHE_LINE_SIZE);
	if (unlikely(!fle)) {
		DPAA2_SEC_DP_ERR("Proto:SG: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_SG_MEM_SIZE(2 * sgl->num));
	DPAA2_SET_FLE_ADDR(fle, (size_t)userdata);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);

	/* Save the shared descriptor */
	flc = &priv->flc_desc[0].flc;
	op_fle = fle + 1;
	ip_fle = fle + 2;
	sge = fle + 3;

	DPAA2_SET_FD_IVP(fd);
	DPAA2_SET_FLE_IVP(op_fle);
	DPAA2_SET_FLE_IVP(ip_fle);

	/* Configure FD as a FRAME LIST */
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(op_fle));
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	/* Configure Output FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_SG_EXT(op_fle);
	DPAA2_SET_FLE_ADDR(op_fle, DPAA2_VADDR_TO_IOVA(sge));

	/* OOP */
	if (dest_sgl) {
		/* Configure Output SGE for Encap/Decap */
		DPAA2_SET_FLE_ADDR(sge, dest_sgl->vec[0].iova);
		DPAA2_SET_FLE_OFFSET(sge, 0);
		sge->length = dest_sgl->vec[0].len;
		out_len += sge->length;
		/* o/p segs */
		for (i = 1; i < dest_sgl->num; i++) {
			sge++;
			DPAA2_SET_FLE_ADDR(sge, dest_sgl->vec[i].iova);
			DPAA2_SET_FLE_OFFSET(sge, 0);
			sge->length = dest_sgl->vec[i].len;
			out_len += sge->length;
		}
		sge->length = dest_sgl->vec[i - 1].tot_len;

	} else {
		/* Configure Output SGE for Encap/Decap */
		DPAA2_SET_FLE_ADDR(sge, sgl->vec[0].iova);
		DPAA2_SET_FLE_OFFSET(sge, 0);
		sge->length = sgl->vec[0].len;
		out_len += sge->length;
		/* o/p segs */
		for (i = 1; i < sgl->num; i++) {
			sge++;
			DPAA2_SET_FLE_ADDR(sge, sgl->vec[i].iova);
			DPAA2_SET_FLE_OFFSET(sge, 0);
			sge->length = sgl->vec[i].len;
			out_len += sge->length;
		}
		sge->length = sgl->vec[i - 1].tot_len;
	}
	out_len += sge->length;

	DPAA2_SET_FLE_FIN(sge);
	op_fle->length = out_len;

	sge++;

	/* Configure Input FLE with Scatter/Gather Entry */
	DPAA2_SET_FLE_ADDR(ip_fle, DPAA2_VADDR_TO_IOVA(sge));
	DPAA2_SET_FLE_SG_EXT(ip_fle);
	DPAA2_SET_FLE_FIN(ip_fle);

	/* Configure input SGE for Encap/Decap */
	DPAA2_SET_FLE_ADDR(sge, sgl->vec[0].iova);
	DPAA2_SET_FLE_OFFSET(sge, 0);
	sge->length = sgl->vec[0].len;
	in_len += sge->length;
	/* i/p segs */
	for (i = 1; i < sgl->num; i++) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, sgl->vec[i].iova);
		DPAA2_SET_FLE_OFFSET(sge, 0);
		sge->length = sgl->vec[i].len;
		in_len += sge->length;
	}

	ip_fle->length = in_len;
	DPAA2_SET_FLE_FIN(sge);

	/* In case of PDCP, per packet HFN is stored in
	 * mbuf priv after sym_op.
	 */
	if (sess->ctxt_type == DPAA2_SEC_PDCP && sess->pdcp.hfn_ovd) {
		uint32_t hfn_ovd = *(uint32_t *)((uint8_t *)userdata +
				sess->pdcp.hfn_ovd_offset);
		/* enable HFN override */
		DPAA2_SET_FLE_INTERNAL_JD(ip_fle, hfn_ovd);
		DPAA2_SET_FLE_INTERNAL_JD(op_fle, hfn_ovd);
		DPAA2_SET_FD_INTERNAL_JD(fd, hfn_ovd);
	}
	DPAA2_SET_FD_LEN(fd, ip_fle->length);

	return 0;
}

static int
build_raw_dp_cipher_fd(uint8_t *drv_ctx,
		       struct rte_crypto_sgl *sgl,
		       struct rte_crypto_sgl *dest_sgl,
		       struct rte_crypto_va_iova_ptr *iv,
		       struct rte_crypto_va_iova_ptr *digest,
		       struct rte_crypto_va_iova_ptr *auth_iv,
		       union rte_crypto_sym_ofs ofs,
		       void *userdata,
		       struct qbman_fd *fd)
{
	RTE_SET_USED(digest);
	RTE_SET_USED(auth_iv);

	dpaa2_sec_session *sess =
		((struct dpaa2_sec_raw_dp_ctx *)drv_ctx)->session;
	struct qbman_fle *ip_fle, *op_fle, *sge, *fle;
	int total_len = 0, data_len = 0, data_offset;
	struct sec_flow_context *flc;
	struct ctxt_priv *priv = sess->ctxt;
	unsigned int i;

	for (i = 0; i < sgl->num; i++)
		total_len += sgl->vec[i].len;

	data_len = total_len - ofs.ofs.cipher.head - ofs.ofs.cipher.tail;
	data_offset = ofs.ofs.cipher.head;

	/* For SNOW3G and ZUC, lengths in bits only supported */
	/* first FLE entry used to store mbuf and session ctxt */
	fle = (struct qbman_fle *)rte_malloc(NULL,
			FLE_SG_MEM_SIZE(2*sgl->num),
			RTE_CACHE_LINE_SIZE);
	if (!fle) {
		DPAA2_SEC_ERR("RAW CIPHER SG: Memory alloc failed for SGE");
		return -ENOMEM;
	}
	memset(fle, 0, FLE_SG_MEM_SIZE(2*sgl->num));
	/* first FLE entry used to store userdata and session ctxt */
	DPAA2_SET_FLE_ADDR(fle, (size_t)userdata);
	DPAA2_FLE_SAVE_CTXT(fle, (ptrdiff_t)priv);

	op_fle = fle + 1;
	ip_fle = fle + 2;
	sge = fle + 3;

	flc = &priv->flc_desc[0].flc;

	DPAA2_SEC_DP_DEBUG(
		"RAW CIPHER SG: cipher_off: 0x%x/length %d, ivlen=%d\n",
		data_offset,
		data_len,
		sess->iv.length);

	/* o/p fle */
	DPAA2_SET_FLE_ADDR(op_fle, DPAA2_VADDR_TO_IOVA(sge));
	op_fle->length = data_len;
	DPAA2_SET_FLE_SG_EXT(op_fle);

	/* OOP */
	if (dest_sgl) {
		/* o/p 1st seg */
		DPAA2_SET_FLE_ADDR(sge, dest_sgl->vec[0].iova);
		DPAA2_SET_FLE_OFFSET(sge, data_offset);
		sge->length = dest_sgl->vec[0].len - data_offset;

		/* o/p segs */
		for (i = 1; i < dest_sgl->num; i++) {
			sge++;
			DPAA2_SET_FLE_ADDR(sge, dest_sgl->vec[i].iova);
			DPAA2_SET_FLE_OFFSET(sge, 0);
			sge->length = dest_sgl->vec[i].len;
		}
	} else {
		/* o/p 1st seg */
		DPAA2_SET_FLE_ADDR(sge, sgl->vec[0].iova);
		DPAA2_SET_FLE_OFFSET(sge, data_offset);
		sge->length = sgl->vec[0].len - data_offset;

		/* o/p segs */
		for (i = 1; i < sgl->num; i++) {
			sge++;
			DPAA2_SET_FLE_ADDR(sge, sgl->vec[i].iova);
			DPAA2_SET_FLE_OFFSET(sge, 0);
			sge->length = sgl->vec[i].len;
		}
	}
	DPAA2_SET_FLE_FIN(sge);

	DPAA2_SEC_DP_DEBUG(
		"RAW CIPHER SG: 1 - flc = %p, fle = %p FLEaddr = %x-%x, len %d\n",
		flc, fle, fle->addr_hi, fle->addr_lo,
		fle->length);

	/* i/p fle */
	sge++;
	DPAA2_SET_FLE_ADDR(ip_fle, DPAA2_VADDR_TO_IOVA(sge));
	ip_fle->length = sess->iv.length + data_len;
	DPAA2_SET_FLE_SG_EXT(ip_fle);

	/* i/p IV */
	DPAA2_SET_FLE_ADDR(sge, iv->iova);
	DPAA2_SET_FLE_OFFSET(sge, 0);
	sge->length = sess->iv.length;

	sge++;

	/* i/p 1st seg */
	DPAA2_SET_FLE_ADDR(sge, sgl->vec[0].iova);
	DPAA2_SET_FLE_OFFSET(sge, data_offset);
	sge->length = sgl->vec[0].len - data_offset;

	/* i/p segs */
	for (i = 1; i < sgl->num; i++) {
		sge++;
		DPAA2_SET_FLE_ADDR(sge, sgl->vec[i].iova);
		DPAA2_SET_FLE_OFFSET(sge, 0);
		sge->length = sgl->vec[i].len;
	}
	DPAA2_SET_FLE_FIN(sge);
	DPAA2_SET_FLE_FIN(ip_fle);

	/* sg fd */
	DPAA2_SET_FD_ADDR(fd, DPAA2_VADDR_TO_IOVA(op_fle));
	DPAA2_SET_FD_LEN(fd, ip_fle->length);
	DPAA2_SET_FD_COMPOUND_FMT(fd);
	DPAA2_SET_FD_FLC(fd, DPAA2_VADDR_TO_IOVA(flc));

	DPAA2_SEC_DP_DEBUG(
		"RAW CIPHER SG: fdaddr =%" PRIx64 " off =%d, len =%d\n",
		DPAA2_GET_FD_ADDR(fd),
		DPAA2_GET_FD_OFFSET(fd),
		DPAA2_GET_FD_LEN(fd));

	return 0;
}

static __rte_always_inline uint32_t
dpaa2_sec_raw_enqueue_burst(void *qp_data, uint8_t *drv_ctx,
	struct rte_crypto_sym_vec *vec, union rte_crypto_sym_ofs ofs,
	void *user_data[], int *status)
{
	RTE_SET_USED(user_data);
	uint32_t loop;
	int32_t ret;
	struct qbman_fd fd_arr[MAX_TX_RING_SLOTS];
	uint32_t frames_to_send, retry_count;
	struct qbman_eq_desc eqdesc;
	struct dpaa2_sec_qp *dpaa2_qp = (struct dpaa2_sec_qp *)qp_data;
	dpaa2_sec_session *sess =
		((struct dpaa2_sec_raw_dp_ctx *)drv_ctx)->session;
	struct qbman_swp *swp;
	uint16_t num_tx = 0;
	uint32_t flags[MAX_TX_RING_SLOTS] = {0};

	if (unlikely(vec->num == 0))
		return 0;

	if (sess == NULL) {
		DPAA2_SEC_ERR("sessionless raw crypto not supported");
		return 0;
	}
	/*Prepare enqueue descriptor*/
	qbman_eq_desc_clear(&eqdesc);
	qbman_eq_desc_set_no_orp(&eqdesc, DPAA2_EQ_RESP_ERR_FQ);
	qbman_eq_desc_set_response(&eqdesc, 0, 0);
	qbman_eq_desc_set_fq(&eqdesc, dpaa2_qp->tx_vq.fqid);

	if (!DPAA2_PER_LCORE_DPIO) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_SEC_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;

	while (vec->num) {
		frames_to_send = (vec->num > dpaa2_eqcr_size) ?
			dpaa2_eqcr_size : vec->num;

		for (loop = 0; loop < frames_to_send; loop++) {
			/*Clear the unused FD fields before sending*/
			memset(&fd_arr[loop], 0, sizeof(struct qbman_fd));
			ret = sess->build_raw_dp_fd(drv_ctx,
						    &vec->src_sgl[loop],
						    &vec->dest_sgl[loop],
						    &vec->iv[loop],
						    &vec->digest[loop],
						    &vec->auth_iv[loop],
						    ofs,
						    user_data[loop],
						    &fd_arr[loop]);
			if (ret) {
				DPAA2_SEC_ERR("error: Improper packet contents"
					      " for crypto operation");
				goto skip_tx;
			}
			status[loop] = 1;
		}

		loop = 0;
		retry_count = 0;
		while (loop < frames_to_send) {
			ret = qbman_swp_enqueue_multiple(swp, &eqdesc,
							 &fd_arr[loop],
							 &flags[loop],
							 frames_to_send - loop);
			if (unlikely(ret < 0)) {
				retry_count++;
				if (retry_count > DPAA2_MAX_TX_RETRY_COUNT) {
					num_tx += loop;
					vec->num -= loop;
					goto skip_tx;
				}
			} else {
				loop += ret;
				retry_count = 0;
			}
		}

		num_tx += loop;
		vec->num -= loop;
	}
skip_tx:
	dpaa2_qp->tx_vq.tx_pkts += num_tx;
	dpaa2_qp->tx_vq.err_pkts += vec->num;

	return num_tx;
}

static __rte_always_inline int
dpaa2_sec_raw_enqueue(void *qp_data, uint8_t *drv_ctx,
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

static inline void *
sec_fd_to_userdata(const struct qbman_fd *fd)
{
	struct qbman_fle *fle;
	void *userdata;
	fle = (struct qbman_fle *)DPAA2_IOVA_TO_VADDR(DPAA2_GET_FD_ADDR(fd));

	DPAA2_SEC_DP_DEBUG("FLE addr = %x - %x, offset = %x\n",
			   fle->addr_hi, fle->addr_lo, fle->fin_bpid_offset);
	userdata = (struct rte_crypto_op *)DPAA2_GET_FLE_ADDR((fle - 1));
	/* free the fle memory */
	rte_free((void *)(fle-1));

	return userdata;
}

static __rte_always_inline uint32_t
dpaa2_sec_raw_dequeue_burst(void *qp_data, uint8_t *drv_ctx,
	rte_cryptodev_raw_get_dequeue_count_t get_dequeue_count,
	uint32_t max_nb_to_dequeue,
	rte_cryptodev_raw_post_dequeue_t post_dequeue,
	void **out_user_data, uint8_t is_user_data_array,
	uint32_t *n_success, int *dequeue_status)
{
	RTE_SET_USED(drv_ctx);
	RTE_SET_USED(get_dequeue_count);

	/* Function is responsible to receive frames for a given device and VQ*/
	struct dpaa2_sec_qp *dpaa2_qp = (struct dpaa2_sec_qp *)qp_data;
	struct qbman_result *dq_storage;
	uint32_t fqid = dpaa2_qp->rx_vq.fqid;
	int ret, num_rx = 0;
	uint8_t is_last = 0, status, is_success = 0;
	struct qbman_swp *swp;
	const struct qbman_fd *fd;
	struct qbman_pull_desc pulldesc;
	void *user_data;
	uint32_t nb_ops = max_nb_to_dequeue;

	if (!DPAA2_PER_LCORE_DPIO) {
		ret = dpaa2_affine_qbman_swp();
		if (ret) {
			DPAA2_SEC_ERR(
				"Failed to allocate IO portal, tid: %d\n",
				rte_gettid());
			return 0;
		}
	}
	swp = DPAA2_PER_LCORE_PORTAL;
	dq_storage = dpaa2_qp->rx_vq.q_storage->dq_storage[0];

	qbman_pull_desc_clear(&pulldesc);
	qbman_pull_desc_set_numframes(&pulldesc,
				      (nb_ops > dpaa2_dqrr_size) ?
				      dpaa2_dqrr_size : nb_ops);
	qbman_pull_desc_set_fq(&pulldesc, fqid);
	qbman_pull_desc_set_storage(&pulldesc, dq_storage,
				    (uint64_t)DPAA2_VADDR_TO_IOVA(dq_storage),
				    1);

	/*Issue a volatile dequeue command. */
	while (1) {
		if (qbman_swp_pull(swp, &pulldesc)) {
			DPAA2_SEC_WARN(
				"SEC VDQ command is not issued : QBMAN busy");
			/* Portal was busy, try again */
			continue;
		}
		break;
	};

	/* Receive the packets till Last Dequeue entry is found with
	 * respect to the above issues PULL command.
	 */
	while (!is_last) {
		/* Check if the previous issued command is completed.
		 * Also seems like the SWP is shared between the Ethernet Driver
		 * and the SEC driver.
		 */
		while (!qbman_check_command_complete(dq_storage))
			;

		/* Loop until the dq_storage is updated with
		 * new token by QBMAN
		 */
		while (!qbman_check_new_result(dq_storage))
			;
		/* Check whether Last Pull command is Expired and
		 * setting Condition for Loop termination
		 */
		if (qbman_result_DQ_is_pull_complete(dq_storage)) {
			is_last = 1;
			/* Check for valid frame. */
			status = (uint8_t)qbman_result_DQ_flags(dq_storage);
			if (unlikely(
				(status & QBMAN_DQ_STAT_VALIDFRAME) == 0)) {
				DPAA2_SEC_DP_DEBUG("No frame is delivered\n");
				continue;
			}
		}

		fd = qbman_result_DQ_fd(dq_storage);
		user_data = sec_fd_to_userdata(fd);
		if (is_user_data_array)
			out_user_data[num_rx] = user_data;
		else
			out_user_data[0] = user_data;
		if (unlikely(fd->simple.frc)) {
			/* TODO Parse SEC errors */
			DPAA2_SEC_ERR("SEC returned Error - %x",
				      fd->simple.frc);
			is_success = false;
		} else {
			is_success = true;
		}
		post_dequeue(user_data, num_rx, is_success);

		num_rx++;
		dq_storage++;
	} /* End of Packet Rx loop */

	dpaa2_qp->rx_vq.rx_pkts += num_rx;
	*dequeue_status = 1;
	*n_success = num_rx;

	DPAA2_SEC_DP_DEBUG("SEC Received %d Packets\n", num_rx);
	/*Return the total number of packets received to DPAA2 app*/
	return num_rx;
}

static __rte_always_inline void *
dpaa2_sec_raw_dequeue(void *qp_data, uint8_t *drv_ctx, int *dequeue_status,
		enum rte_crypto_op_status *op_status)
{
	RTE_SET_USED(qp_data);
	RTE_SET_USED(drv_ctx);
	RTE_SET_USED(dequeue_status);
	RTE_SET_USED(op_status);

	return NULL;
}

static __rte_always_inline int
dpaa2_sec_raw_enqueue_done(void *qp_data, uint8_t *drv_ctx, uint32_t n)
{
	RTE_SET_USED(qp_data);
	RTE_SET_USED(drv_ctx);
	RTE_SET_USED(n);

	return 0;
}

static __rte_always_inline int
dpaa2_sec_raw_dequeue_done(void *qp_data, uint8_t *drv_ctx, uint32_t n)
{
	RTE_SET_USED(qp_data);
	RTE_SET_USED(drv_ctx);
	RTE_SET_USED(n);

	return 0;
}

int
dpaa2_sec_configure_raw_dp_ctx(struct rte_cryptodev *dev, uint16_t qp_id,
	struct rte_crypto_raw_dp_ctx *raw_dp_ctx,
	enum rte_crypto_op_sess_type sess_type,
	union rte_cryptodev_session_ctx session_ctx, uint8_t is_update)
{
	dpaa2_sec_session *sess;
	struct dpaa2_sec_raw_dp_ctx *dp_ctx;
	RTE_SET_USED(qp_id);

	if (!is_update) {
		memset(raw_dp_ctx, 0, sizeof(*raw_dp_ctx));
		raw_dp_ctx->qp_data = dev->data->queue_pairs[qp_id];
	}

	if (sess_type == RTE_CRYPTO_OP_SECURITY_SESSION)
		sess = (dpaa2_sec_session *)get_sec_session_private_data(
				session_ctx.sec_sess);
	else if (sess_type == RTE_CRYPTO_OP_WITH_SESSION)
		sess = (dpaa2_sec_session *)get_sym_session_private_data(
			session_ctx.crypto_sess, cryptodev_driver_id);
	else
		return -ENOTSUP;
	raw_dp_ctx->dequeue_burst = dpaa2_sec_raw_dequeue_burst;
	raw_dp_ctx->dequeue = dpaa2_sec_raw_dequeue;
	raw_dp_ctx->dequeue_done = dpaa2_sec_raw_dequeue_done;
	raw_dp_ctx->enqueue_burst = dpaa2_sec_raw_enqueue_burst;
	raw_dp_ctx->enqueue = dpaa2_sec_raw_enqueue;
	raw_dp_ctx->enqueue_done = dpaa2_sec_raw_enqueue_done;

	if (sess->ctxt_type == DPAA2_SEC_CIPHER_HASH)
		sess->build_raw_dp_fd = build_raw_dp_chain_fd;
	else if (sess->ctxt_type == DPAA2_SEC_AEAD)
		sess->build_raw_dp_fd = build_raw_dp_aead_fd;
	else if (sess->ctxt_type == DPAA2_SEC_AUTH)
		sess->build_raw_dp_fd = build_raw_dp_auth_fd;
	else if (sess->ctxt_type == DPAA2_SEC_CIPHER)
		sess->build_raw_dp_fd = build_raw_dp_cipher_fd;
	else if (sess->ctxt_type == DPAA2_SEC_IPSEC ||
		sess->ctxt_type == DPAA2_SEC_PDCP)
		sess->build_raw_dp_fd = build_raw_dp_proto_fd;
	else
		return -ENOTSUP;
	dp_ctx = (struct dpaa2_sec_raw_dp_ctx *)raw_dp_ctx->drv_ctx_data;
	dp_ctx->session = sess;

	return 0;
}

int
dpaa2_sec_get_dp_ctx_size(__rte_unused struct rte_cryptodev *dev)
{
	return sizeof(struct dpaa2_sec_raw_dp_ctx);
}
