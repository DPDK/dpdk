/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#include <stdbool.h>

#include <rte_byteorder.h>
#include <rte_crypto_sym.h>
#include <rte_cryptodev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include "bcmfs_sym_defs.h"
#include "bcmfs_sym_engine.h"
#include "bcmfs_sym_req.h"
#include "bcmfs_sym_session.h"

/** Process cipher operation */
static int
process_crypto_cipher_op(struct rte_crypto_op *op,
			 struct rte_mbuf *mbuf_src,
			 struct rte_mbuf *mbuf_dst,
			 struct bcmfs_sym_session *sess,
			 struct bcmfs_sym_request *req)
{
	int rc = 0;
	struct fsattr src, dst, iv, key;
	struct rte_crypto_sym_op *sym_op = op->sym;

	fsattr_sz(&src) = sym_op->cipher.data.length;
	fsattr_sz(&dst) = sym_op->cipher.data.length;

	fsattr_va(&src) = rte_pktmbuf_mtod_offset
					(mbuf_src,
					 uint8_t *,
					 op->sym->cipher.data.offset);

	fsattr_va(&dst) = rte_pktmbuf_mtod_offset
					(mbuf_dst,
					 uint8_t *,
					 op->sym->cipher.data.offset);

	fsattr_pa(&src) = rte_pktmbuf_iova(mbuf_src);
	fsattr_pa(&dst) = rte_pktmbuf_iova(mbuf_dst);

	fsattr_va(&iv) = rte_crypto_op_ctod_offset(op,
						   uint8_t *,
						   sess->cipher.iv.offset);

	fsattr_sz(&iv) = sess->cipher.iv.length;

	fsattr_va(&key) = sess->cipher.key.data;
	fsattr_pa(&key) = 0;
	fsattr_sz(&key) = sess->cipher.key.length;

	rc = bcmfs_crypto_build_cipher_req(req, sess->cipher.algo,
					   sess->cipher.op, &src,
					   &dst, &key, &iv);
	if (rc)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;

	return rc;
}

/** Process auth operation */
static int
process_crypto_auth_op(struct rte_crypto_op *op,
		       struct rte_mbuf *mbuf_src,
		       struct bcmfs_sym_session *sess,
		       struct bcmfs_sym_request *req)
{
	int rc = 0;
	struct fsattr src, dst, mac, key, iv;

	fsattr_sz(&src) = op->sym->auth.data.length;
	fsattr_va(&src) = rte_pktmbuf_mtod_offset(mbuf_src,
						  uint8_t *,
						  op->sym->auth.data.offset);
	fsattr_pa(&src) = rte_pktmbuf_iova(mbuf_src);

	if (!sess->auth.op) {
		fsattr_va(&mac) = op->sym->auth.digest.data;
		fsattr_pa(&mac) = op->sym->auth.digest.phys_addr;
		fsattr_sz(&mac) = sess->auth.digest_length;
	} else {
		fsattr_va(&dst) = op->sym->auth.digest.data;
		fsattr_pa(&dst) = op->sym->auth.digest.phys_addr;
		fsattr_sz(&dst) = sess->auth.digest_length;
	}

	fsattr_va(&key) = sess->auth.key.data;
	fsattr_pa(&key) = 0;
	fsattr_sz(&key) = sess->auth.key.length;

	/* AES-GMAC uses AES-GCM-128 authenticator */
	if (sess->auth.algo == RTE_CRYPTO_AUTH_AES_GMAC) {
		fsattr_va(&iv) = rte_crypto_op_ctod_offset(op,
							   uint8_t *,
							   sess->auth.iv.offset);
		fsattr_pa(&iv) = 0;
		fsattr_sz(&iv) = sess->auth.iv.length;
	} else {
		fsattr_va(&iv) = NULL;
		fsattr_sz(&iv) = 0;
	}

		rc = bcmfs_crypto_build_auth_req(req, sess->auth.algo,
						 sess->auth.op,
						 &src,
						 (sess->auth.op) ? (&dst) : NULL,
						 (sess->auth.op) ? NULL  : (&mac),
						 &key, &iv);

	if (rc)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;

	return rc;
}

/** Process combined/chained mode operation */
static int
process_crypto_combined_op(struct rte_crypto_op *op,
			   struct rte_mbuf *mbuf_src,
			   struct rte_mbuf *mbuf_dst,
			   struct bcmfs_sym_session *sess,
			   struct bcmfs_sym_request *req)
{
	int rc = 0, aad_size = 0;
	struct fsattr src, dst, iv;
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct fsattr cipher_key, aad, mac, auth_key;

	fsattr_sz(&src) = sym_op->cipher.data.length;
	fsattr_sz(&dst) = sym_op->cipher.data.length;

	fsattr_va(&src) = rte_pktmbuf_mtod_offset
					(mbuf_src,
					 uint8_t *,
					 sym_op->cipher.data.offset);

	fsattr_va(&dst) = rte_pktmbuf_mtod_offset
					(mbuf_dst,
					 uint8_t *,
					 sym_op->cipher.data.offset);

	fsattr_pa(&src) = rte_pktmbuf_iova_offset(mbuf_src,
						  sym_op->cipher.data.offset);
	fsattr_pa(&dst) = rte_pktmbuf_iova_offset(mbuf_dst,
						  sym_op->cipher.data.offset);

	fsattr_va(&iv) = rte_crypto_op_ctod_offset(op,
						   uint8_t *,
						   sess->cipher.iv.offset);

	fsattr_pa(&iv) = 0;
	fsattr_sz(&iv) = sess->cipher.iv.length;

	fsattr_va(&cipher_key) = sess->cipher.key.data;
	fsattr_pa(&cipher_key) = 0;
	fsattr_sz(&cipher_key) = sess->cipher.key.length;

	fsattr_va(&auth_key) = sess->auth.key.data;
	fsattr_pa(&auth_key) = 0;
	fsattr_sz(&auth_key) = sess->auth.key.length;

	fsattr_va(&mac) = op->sym->auth.digest.data;
	fsattr_pa(&mac) = op->sym->auth.digest.phys_addr;
	fsattr_sz(&mac) = sess->auth.digest_length;

	aad_size = sym_op->auth.data.length - sym_op->cipher.data.length;

	if (aad_size > 0) {
		fsattr_sz(&aad) =  aad_size;
		fsattr_va(&aad) = rte_pktmbuf_mtod_offset
						(mbuf_src,
						 uint8_t *,
						sym_op->auth.data.offset);
		fsattr_pa(&aad) = rte_pktmbuf_iova_offset(mbuf_src,
						sym_op->auth.data.offset);
	}

	rc = bcmfs_crypto_build_chain_request(req, sess->cipher.algo,
					      sess->cipher.op,
					      sess->auth.algo,
					      sess->auth.op,
					      &src, &dst, &cipher_key,
					      &auth_key, &iv,
					      (aad_size > 0) ? (&aad) : NULL,
					      &mac, sess->cipher_first);

	if (rc)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;

	return rc;
}

/** Process AEAD operation */
static int
process_crypto_aead_op(struct rte_crypto_op *op,
		       struct rte_mbuf *mbuf_src,
		       struct rte_mbuf *mbuf_dst,
		       struct bcmfs_sym_session *sess,
		       struct bcmfs_sym_request *req)
{
	int rc = 0;
	struct fsattr src, dst, iv;
	struct rte_crypto_sym_op *sym_op = op->sym;
	struct fsattr key, aad, mac;

	fsattr_sz(&src) = sym_op->aead.data.length;
	fsattr_sz(&dst) = sym_op->aead.data.length;

	fsattr_va(&src) = rte_pktmbuf_mtod_offset(mbuf_src,
						  uint8_t *,
						  sym_op->aead.data.offset);

	fsattr_va(&dst) = rte_pktmbuf_mtod_offset(mbuf_dst,
						  uint8_t *,
						  sym_op->aead.data.offset);

	fsattr_pa(&src) = rte_pktmbuf_iova_offset(mbuf_src,
						  sym_op->aead.data.offset);
	fsattr_pa(&dst) = rte_pktmbuf_iova_offset(mbuf_dst,
						  sym_op->aead.data.offset);

	fsattr_va(&iv) = rte_crypto_op_ctod_offset(op,
						   uint8_t *,
						   sess->aead.iv.offset);

	fsattr_pa(&iv) = 0;
	fsattr_sz(&iv) = sess->aead.iv.length;

	fsattr_va(&key) = sess->aead.key.data;
	fsattr_pa(&key) = 0;
	fsattr_sz(&key) = sess->aead.key.length;

	fsattr_va(&mac) = op->sym->aead.digest.data;
	fsattr_pa(&mac) = op->sym->aead.digest.phys_addr;
	fsattr_sz(&mac) = sess->aead.digest_length;

	fsattr_va(&aad) = op->sym->aead.aad.data;
	fsattr_pa(&aad) = op->sym->aead.aad.phys_addr;
	fsattr_sz(&aad) = sess->aead.aad_length;

	rc = bcmfs_crypto_build_aead_request(req, sess->aead.algo,
					     sess->aead.op, &src, &dst,
					     &key, &iv, &aad, &mac);

	if (rc)
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;

	return rc;
}

/** Process crypto operation for mbuf */
int
bcmfs_process_sym_crypto_op(struct rte_crypto_op *op,
			    struct bcmfs_sym_session *sess,
			    struct bcmfs_sym_request *req)
{
	struct rte_mbuf *msrc, *mdst;
	int rc = 0;

	msrc = op->sym->m_src;
	mdst = op->sym->m_dst ? op->sym->m_dst : op->sym->m_src;
	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

	switch (sess->chain_order) {
	case BCMFS_SYM_CHAIN_ONLY_CIPHER:
		rc = process_crypto_cipher_op(op, msrc, mdst, sess, req);
		break;
	case BCMFS_SYM_CHAIN_ONLY_AUTH:
		rc = process_crypto_auth_op(op, msrc, sess, req);
		break;
	case BCMFS_SYM_CHAIN_CIPHER_AUTH:
	case BCMFS_SYM_CHAIN_AUTH_CIPHER:
		rc = process_crypto_combined_op(op, msrc, mdst, sess, req);
		break;
	case BCMFS_SYM_CHAIN_AEAD:
		rc = process_crypto_aead_op(op, msrc, mdst, sess, req);
		break;
	default:
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		break;
	}

	return rc;
}
