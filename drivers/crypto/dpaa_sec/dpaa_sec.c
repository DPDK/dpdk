/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2017 NXP.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of NXP nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <net/if.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cryptodev_pmd.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_cycles.h>
#include <rte_dev.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>

#include <fsl_usd.h>
#include <fsl_qman.h>
#include <of.h>

/* RTA header files */
#include <hw/desc/common.h>
#include <hw/desc/algo.h>
#include <hw/desc/ipsec.h>

#include <rte_dpaa_bus.h>
#include <dpaa_sec.h>
#include <dpaa_sec_log.h>

enum rta_sec_era rta_sec_era;

static uint8_t cryptodev_driver_id;

static __thread struct rte_crypto_op **dpaa_sec_ops;
static __thread int dpaa_sec_op_nb;

static inline void
dpaa_sec_op_ending(struct dpaa_sec_op_ctx *ctx)
{
	if (!ctx->fd_status) {
		ctx->op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	} else {
		PMD_RX_LOG(ERR, "SEC return err: 0x%x", ctx->fd_status);
		ctx->op->status = RTE_CRYPTO_OP_STATUS_ERROR;
	}

	/* report op status to sym->op and then free the ctx memeory  */
	rte_mempool_put(ctx->ctx_pool, (void *)ctx);
}

static inline struct dpaa_sec_op_ctx *
dpaa_sec_alloc_ctx(dpaa_sec_session *ses)
{
	struct dpaa_sec_op_ctx *ctx;
	int retval;

	retval = rte_mempool_get(ses->ctx_pool, (void **)(&ctx));
	if (!ctx || retval) {
		PMD_TX_LOG(ERR, "Alloc sec descriptor failed!");
		return NULL;
	}
	/*
	 * Clear SG memory. There are 16 SG entries of 16 Bytes each.
	 * one call to dcbz_64() clear 64 bytes, hence calling it 4 times
	 * to clear all the SG entries. dpaa_sec_alloc_ctx() is called for
	 * each packet, memset is costlier than dcbz_64().
	 */
	dcbz_64(&ctx->job.sg[SG_CACHELINE_0]);
	dcbz_64(&ctx->job.sg[SG_CACHELINE_1]);
	dcbz_64(&ctx->job.sg[SG_CACHELINE_2]);
	dcbz_64(&ctx->job.sg[SG_CACHELINE_3]);

	ctx->ctx_pool = ses->ctx_pool;

	return ctx;
}

static inline rte_iova_t
dpaa_mem_vtop(void *vaddr)
{
	const struct rte_memseg *memseg = rte_eal_get_physmem_layout();
	uint64_t vaddr_64, paddr;
	int i;

	vaddr_64 = (uint64_t)vaddr;
	for (i = 0; i < RTE_MAX_MEMSEG && memseg[i].addr_64 != 0; i++) {
		if (vaddr_64 >= memseg[i].addr_64 &&
		    vaddr_64 < memseg[i].addr_64 + memseg[i].len) {
			paddr = memseg[i].phys_addr +
				(vaddr_64 - memseg[i].addr_64);

			return (rte_iova_t)paddr;
		}
	}
	return (rte_iova_t)(NULL);
}

static inline void *
dpaa_mem_ptov(rte_iova_t paddr)
{
	const struct rte_memseg *memseg = rte_eal_get_physmem_layout();
	int i;

	for (i = 0; i < RTE_MAX_MEMSEG && memseg[i].addr_64 != 0; i++) {
		if (paddr >= memseg[i].phys_addr &&
		    (char *)paddr < (char *)memseg[i].phys_addr + memseg[i].len)
			return (void *)(memseg[i].addr_64 +
					(paddr - memseg[i].phys_addr));
	}
	return NULL;
}

static void
ern_sec_fq_handler(struct qman_portal *qm __rte_unused,
		   struct qman_fq *fq,
		   const struct qm_mr_entry *msg)
{
	RTE_LOG_DP(ERR, PMD, "sec fq %d error, RC = %x, seqnum = %x\n",
		   fq->fqid, msg->ern.rc, msg->ern.seqnum);
}

/* initialize the queue with dest chan as caam chan so that
 * all the packets in this queue could be dispatched into caam
 */
static int
dpaa_sec_init_rx(struct qman_fq *fq_in, rte_iova_t hwdesc,
		 uint32_t fqid_out)
{
	struct qm_mcc_initfq fq_opts;
	uint32_t flags;
	int ret = -1;

	/* Clear FQ options */
	memset(&fq_opts, 0x00, sizeof(struct qm_mcc_initfq));

	flags = QMAN_FQ_FLAG_LOCKED | QMAN_FQ_FLAG_DYNAMIC_FQID |
		QMAN_FQ_FLAG_TO_DCPORTAL;

	ret = qman_create_fq(0, flags, fq_in);
	if (unlikely(ret != 0)) {
		PMD_INIT_LOG(ERR, "qman_create_fq failed");
		return ret;
	}

	flags = QMAN_INITFQ_FLAG_SCHED;
	fq_opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_CONTEXTA |
			  QM_INITFQ_WE_CONTEXTB;

	qm_fqd_context_a_set64(&fq_opts.fqd, hwdesc);
	fq_opts.fqd.context_b = fqid_out;
	fq_opts.fqd.dest.channel = qm_channel_caam;
	fq_opts.fqd.dest.wq = 0;

	fq_in->cb.ern  = ern_sec_fq_handler;

	ret = qman_init_fq(fq_in, flags, &fq_opts);
	if (unlikely(ret != 0))
		PMD_INIT_LOG(ERR, "qman_init_fq failed");

	return ret;
}

/* something is put into in_fq and caam put the crypto result into out_fq */
static enum qman_cb_dqrr_result
dqrr_out_fq_cb_rx(struct qman_portal *qm __always_unused,
		  struct qman_fq *fq __always_unused,
		  const struct qm_dqrr_entry *dqrr)
{
	const struct qm_fd *fd;
	struct dpaa_sec_job *job;
	struct dpaa_sec_op_ctx *ctx;

	if (dpaa_sec_op_nb >= DPAA_SEC_BURST)
		return qman_cb_dqrr_defer;

	if (!(dqrr->stat & QM_DQRR_STAT_FD_VALID))
		return qman_cb_dqrr_consume;

	fd = &dqrr->fd;
	/* sg is embedded in an op ctx,
	 * sg[0] is for output
	 * sg[1] for input
	 */
	job = dpaa_mem_ptov(qm_fd_addr_get64(fd));
	ctx = container_of(job, struct dpaa_sec_op_ctx, job);
	ctx->fd_status = fd->status;
	dpaa_sec_ops[dpaa_sec_op_nb++] = ctx->op;
	dpaa_sec_op_ending(ctx);

	return qman_cb_dqrr_consume;
}

/* caam result is put into this queue */
static int
dpaa_sec_init_tx(struct qman_fq *fq)
{
	int ret;
	struct qm_mcc_initfq opts;
	uint32_t flags;

	flags = QMAN_FQ_FLAG_NO_ENQUEUE | QMAN_FQ_FLAG_LOCKED |
		QMAN_FQ_FLAG_DYNAMIC_FQID;

	ret = qman_create_fq(0, flags, fq);
	if (unlikely(ret)) {
		PMD_INIT_LOG(ERR, "qman_create_fq failed");
		return ret;
	}

	memset(&opts, 0, sizeof(opts));
	opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_FQCTRL |
		       QM_INITFQ_WE_CONTEXTA | QM_INITFQ_WE_CONTEXTB;

	/* opts.fqd.dest.channel = dpaa_sec_pool_chan; */

	fq->cb.dqrr = dqrr_out_fq_cb_rx;
	fq->cb.ern  = ern_sec_fq_handler;

	ret = qman_init_fq(fq, 0, &opts);
	if (unlikely(ret)) {
		PMD_INIT_LOG(ERR, "unable to init caam source fq!");
		return ret;
	}

	return ret;
}

static inline int is_cipher_only(dpaa_sec_session *ses)
{
	return ((ses->cipher_alg != RTE_CRYPTO_CIPHER_NULL) &&
		(ses->auth_alg == RTE_CRYPTO_AUTH_NULL));
}

static inline int is_auth_only(dpaa_sec_session *ses)
{
	return ((ses->cipher_alg == RTE_CRYPTO_CIPHER_NULL) &&
		(ses->auth_alg != RTE_CRYPTO_AUTH_NULL));
}

static inline int is_aead(dpaa_sec_session *ses)
{
	return ((ses->cipher_alg == 0) &&
		(ses->auth_alg == 0) &&
		(ses->aead_alg != 0));
}

static inline int is_auth_cipher(dpaa_sec_session *ses)
{
	return ((ses->cipher_alg != RTE_CRYPTO_CIPHER_NULL) &&
		(ses->auth_alg != RTE_CRYPTO_AUTH_NULL));
}

static inline int is_encode(dpaa_sec_session *ses)
{
	return ses->dir == DIR_ENC;
}

static inline int is_decode(dpaa_sec_session *ses)
{
	return ses->dir == DIR_DEC;
}

static inline void
caam_auth_alg(dpaa_sec_session *ses, struct alginfo *alginfo_a)
{
	switch (ses->auth_alg) {
	case RTE_CRYPTO_AUTH_NULL:
		ses->digest_length = 0;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		alginfo_a->algtype = OP_ALG_ALGSEL_MD5;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		alginfo_a->algtype = OP_ALG_ALGSEL_SHA1;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		alginfo_a->algtype = OP_ALG_ALGSEL_SHA224;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		alginfo_a->algtype = OP_ALG_ALGSEL_SHA256;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		alginfo_a->algtype = OP_ALG_ALGSEL_SHA384;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		alginfo_a->algtype = OP_ALG_ALGSEL_SHA512;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	default:
		PMD_INIT_LOG(ERR, "unsupported auth alg %u", ses->auth_alg);
	}
}

static inline void
caam_cipher_alg(dpaa_sec_session *ses, struct alginfo *alginfo_c)
{
	switch (ses->cipher_alg) {
	case RTE_CRYPTO_CIPHER_NULL:
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		alginfo_c->algtype = OP_ALG_ALGSEL_AES;
		alginfo_c->algmode = OP_ALG_AAI_CBC;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		alginfo_c->algtype = OP_ALG_ALGSEL_3DES;
		alginfo_c->algmode = OP_ALG_AAI_CBC;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		alginfo_c->algtype = OP_ALG_ALGSEL_AES;
		alginfo_c->algmode = OP_ALG_AAI_CTR;
		break;
	default:
		PMD_INIT_LOG(ERR, "unsupported cipher alg %d", ses->cipher_alg);
	}
}

static inline void
caam_aead_alg(dpaa_sec_session *ses, struct alginfo *alginfo)
{
	switch (ses->aead_alg) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		alginfo->algtype = OP_ALG_ALGSEL_AES;
		alginfo->algmode = OP_ALG_AAI_GCM;
		break;
	default:
		PMD_INIT_LOG(ERR, "unsupported AEAD alg %d", ses->aead_alg);
	}
}


/* prepare command block of the session */
static int
dpaa_sec_prep_cdb(dpaa_sec_session *ses)
{
	struct alginfo alginfo_c = {0}, alginfo_a = {0}, alginfo = {0};
	uint32_t shared_desc_len = 0;
	struct sec_cdb *cdb = &ses->qp->cdb;
	int err;
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	int swap = false;
#else
	int swap = true;
#endif

	memset(cdb, 0, sizeof(struct sec_cdb));

	if (is_cipher_only(ses)) {
		caam_cipher_alg(ses, &alginfo_c);
		if (alginfo_c.algtype == (unsigned int)DPAA_SEC_ALG_UNSUPPORT) {
			PMD_TX_LOG(ERR, "not supported cipher alg\n");
			return -ENOTSUP;
		}

		alginfo_c.key = (uint64_t)ses->cipher_key.data;
		alginfo_c.keylen = ses->cipher_key.length;
		alginfo_c.key_enc_flags = 0;
		alginfo_c.key_type = RTA_DATA_IMM;

		shared_desc_len = cnstr_shdsc_blkcipher(
						cdb->sh_desc, true,
						swap, &alginfo_c,
						NULL,
						ses->iv.length,
						ses->dir);
	} else if (is_auth_only(ses)) {
		caam_auth_alg(ses, &alginfo_a);
		if (alginfo_a.algtype == (unsigned int)DPAA_SEC_ALG_UNSUPPORT) {
			PMD_TX_LOG(ERR, "not supported auth alg\n");
			return -ENOTSUP;
		}

		alginfo_a.key = (uint64_t)ses->auth_key.data;
		alginfo_a.keylen = ses->auth_key.length;
		alginfo_a.key_enc_flags = 0;
		alginfo_a.key_type = RTA_DATA_IMM;

		shared_desc_len = cnstr_shdsc_hmac(cdb->sh_desc, true,
						   swap, &alginfo_a,
						   !ses->dir,
						   ses->digest_length);
	} else if (is_aead(ses)) {
		caam_aead_alg(ses, &alginfo);
		if (alginfo.algtype == (unsigned int)DPAA_SEC_ALG_UNSUPPORT) {
			PMD_TX_LOG(ERR, "not supported aead alg\n");
			return -ENOTSUP;
		}
		alginfo.key = (uint64_t)ses->aead_key.data;
		alginfo.keylen = ses->aead_key.length;
		alginfo.key_enc_flags = 0;
		alginfo.key_type = RTA_DATA_IMM;

		if (ses->dir == DIR_ENC)
			shared_desc_len = cnstr_shdsc_gcm_encap(
					cdb->sh_desc, true, swap,
					&alginfo,
					ses->iv.length,
					ses->digest_length);
		else
			shared_desc_len = cnstr_shdsc_gcm_decap(
					cdb->sh_desc, true, swap,
					&alginfo,
					ses->iv.length,
					ses->digest_length);
	} else {
		caam_cipher_alg(ses, &alginfo_c);
		if (alginfo_c.algtype == (unsigned int)DPAA_SEC_ALG_UNSUPPORT) {
			PMD_TX_LOG(ERR, "not supported cipher alg\n");
			return -ENOTSUP;
		}

		alginfo_c.key = (uint64_t)ses->cipher_key.data;
		alginfo_c.keylen = ses->cipher_key.length;
		alginfo_c.key_enc_flags = 0;
		alginfo_c.key_type = RTA_DATA_IMM;

		caam_auth_alg(ses, &alginfo_a);
		if (alginfo_a.algtype == (unsigned int)DPAA_SEC_ALG_UNSUPPORT) {
			PMD_TX_LOG(ERR, "not supported auth alg\n");
			return -ENOTSUP;
		}

		alginfo_a.key = (uint64_t)ses->auth_key.data;
		alginfo_a.keylen = ses->auth_key.length;
		alginfo_a.key_enc_flags = 0;
		alginfo_a.key_type = RTA_DATA_IMM;

		cdb->sh_desc[0] = alginfo_c.keylen;
		cdb->sh_desc[1] = alginfo_a.keylen;
		err = rta_inline_query(IPSEC_AUTH_VAR_AES_DEC_BASE_DESC_LEN,
				       MIN_JOB_DESC_SIZE,
				       (unsigned int *)cdb->sh_desc,
				       &cdb->sh_desc[2], 2);

		if (err < 0) {
			PMD_TX_LOG(ERR, "Crypto: Incorrect key lengths");
			return err;
		}
		if (cdb->sh_desc[2] & 1)
			alginfo_c.key_type = RTA_DATA_IMM;
		else {
			alginfo_c.key = (uint64_t)dpaa_mem_vtop(
							(void *)alginfo_c.key);
			alginfo_c.key_type = RTA_DATA_PTR;
		}
		if (cdb->sh_desc[2] & (1<<1))
			alginfo_a.key_type = RTA_DATA_IMM;
		else {
			alginfo_a.key = (uint64_t)dpaa_mem_vtop(
							(void *)alginfo_a.key);
			alginfo_a.key_type = RTA_DATA_PTR;
		}
		cdb->sh_desc[0] = 0;
		cdb->sh_desc[1] = 0;
		cdb->sh_desc[2] = 0;

		/* Auth_only_len is set as 0 here and it will be overwritten
		 *  in fd for each packet.
		 */
		shared_desc_len = cnstr_shdsc_authenc(cdb->sh_desc,
				true, swap, &alginfo_c, &alginfo_a,
				ses->iv.length, 0,
				ses->digest_length, ses->dir);
	}
	cdb->sh_hdr.hi.field.idlen = shared_desc_len;
	cdb->sh_hdr.hi.word = rte_cpu_to_be_32(cdb->sh_hdr.hi.word);
	cdb->sh_hdr.lo.word = rte_cpu_to_be_32(cdb->sh_hdr.lo.word);

	return 0;
}

static inline unsigned int
dpaa_volatile_deq(struct qman_fq *fq, unsigned int len, bool exact)
{
	unsigned int pkts = 0;
	int ret;
	struct qm_mcr_queryfq_np np;
	enum qman_fq_state state;
	uint32_t flags;
	uint32_t vdqcr;

	qman_query_fq_np(fq, &np);
	if (np.frm_cnt) {
		vdqcr = QM_VDQCR_NUMFRAMES_SET(len);
		if (exact)
			vdqcr |= QM_VDQCR_EXACT;
		ret = qman_volatile_dequeue(fq, 0, vdqcr);
		if (ret)
			return 0;
		do {
			pkts += qman_poll_dqrr(len);
			qman_fq_state(fq, &state, &flags);
		} while (flags & QMAN_FQ_STATE_VDQCR);
	}
	return pkts;
}

/* qp is lockless, should be accessed by only one thread */
static int
dpaa_sec_deq(struct dpaa_sec_qp *qp, struct rte_crypto_op **ops, int nb_ops)
{
	struct qman_fq *fq;

	fq = &qp->outq;
	dpaa_sec_op_nb = 0;
	dpaa_sec_ops = ops;

	if (unlikely(nb_ops > DPAA_SEC_BURST))
		nb_ops = DPAA_SEC_BURST;

	return dpaa_volatile_deq(fq, nb_ops, 1);
}

/**
 * packet looks like:
 *		|<----data_len------->|
 *    |ip_header|ah_header|icv|payload|
 *              ^
 *		|
 *	   mbuf->pkt.data
 */
static inline struct dpaa_sec_job *
build_auth_only(struct rte_crypto_op *op, dpaa_sec_session *ses)
{
	struct rte_crypto_sym_op *sym = op->sym;
	struct rte_mbuf *mbuf = sym->m_src;
	struct dpaa_sec_job *cf;
	struct dpaa_sec_op_ctx *ctx;
	struct qm_sg_entry *sg;
	rte_iova_t start_addr;
	uint8_t *old_digest;

	ctx = dpaa_sec_alloc_ctx(ses);
	if (!ctx)
		return NULL;

	cf = &ctx->job;
	ctx->op = op;
	old_digest = ctx->digest;

	start_addr = rte_pktmbuf_iova(mbuf);
	/* output */
	sg = &cf->sg[0];
	qm_sg_entry_set64(sg, sym->auth.digest.phys_addr);
	sg->length = ses->digest_length;
	cpu_to_hw_sg(sg);

	/* input */
	sg = &cf->sg[1];
	if (is_decode(ses)) {
		/* need to extend the input to a compound frame */
		sg->extension = 1;
		qm_sg_entry_set64(sg, dpaa_mem_vtop(&cf->sg[2]));
		sg->length = sym->auth.data.length + ses->digest_length;
		sg->final = 1;
		cpu_to_hw_sg(sg);

		sg = &cf->sg[2];
		/* hash result or digest, save digest first */
		rte_memcpy(old_digest, sym->auth.digest.data,
			   ses->digest_length);
		qm_sg_entry_set64(sg, start_addr + sym->auth.data.offset);
		sg->length = sym->auth.data.length;
		cpu_to_hw_sg(sg);

		/* let's check digest by hw */
		start_addr = dpaa_mem_vtop(old_digest);
		sg++;
		qm_sg_entry_set64(sg, start_addr);
		sg->length = ses->digest_length;
		sg->final = 1;
		cpu_to_hw_sg(sg);
	} else {
		qm_sg_entry_set64(sg, start_addr + sym->auth.data.offset);
		sg->length = sym->auth.data.length;
		sg->final = 1;
		cpu_to_hw_sg(sg);
	}

	return cf;
}

static inline struct dpaa_sec_job *
build_cipher_only(struct rte_crypto_op *op, dpaa_sec_session *ses)
{
	struct rte_crypto_sym_op *sym = op->sym;
	struct dpaa_sec_job *cf;
	struct dpaa_sec_op_ctx *ctx;
	struct qm_sg_entry *sg;
	rte_iova_t src_start_addr, dst_start_addr;
	uint8_t *IV_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			ses->iv.offset);

	ctx = dpaa_sec_alloc_ctx(ses);
	if (!ctx)
		return NULL;

	cf = &ctx->job;
	ctx->op = op;

	src_start_addr = rte_pktmbuf_iova(sym->m_src);

	if (sym->m_dst)
		dst_start_addr = rte_pktmbuf_iova(sym->m_dst);
	else
		dst_start_addr = src_start_addr;

	/* output */
	sg = &cf->sg[0];
	qm_sg_entry_set64(sg, dst_start_addr + sym->cipher.data.offset);
	sg->length = sym->cipher.data.length + ses->iv.length;
	cpu_to_hw_sg(sg);

	/* input */
	sg = &cf->sg[1];

	/* need to extend the input to a compound frame */
	sg->extension = 1;
	sg->final = 1;
	sg->length = sym->cipher.data.length + ses->iv.length;
	qm_sg_entry_set64(sg, dpaa_mem_vtop(&cf->sg[2]));
	cpu_to_hw_sg(sg);

	sg = &cf->sg[2];
	qm_sg_entry_set64(sg, dpaa_mem_vtop(IV_ptr));
	sg->length = ses->iv.length;
	cpu_to_hw_sg(sg);

	sg++;
	qm_sg_entry_set64(sg, src_start_addr + sym->cipher.data.offset);
	sg->length = sym->cipher.data.length;
	sg->final = 1;
	cpu_to_hw_sg(sg);

	return cf;
}

static inline struct dpaa_sec_job *
build_cipher_auth_gcm(struct rte_crypto_op *op, dpaa_sec_session *ses)
{
	struct rte_crypto_sym_op *sym = op->sym;
	struct dpaa_sec_job *cf;
	struct dpaa_sec_op_ctx *ctx;
	struct qm_sg_entry *sg;
	uint32_t length = 0;
	rte_iova_t src_start_addr, dst_start_addr;
	uint8_t *IV_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			ses->iv.offset);

	src_start_addr = sym->m_src->buf_iova + sym->m_src->data_off;

	if (sym->m_dst)
		dst_start_addr = sym->m_dst->buf_iova + sym->m_dst->data_off;
	else
		dst_start_addr = src_start_addr;

	ctx = dpaa_sec_alloc_ctx(ses);
	if (!ctx)
		return NULL;

	cf = &ctx->job;
	ctx->op = op;

	/* input */
	rte_prefetch0(cf->sg);
	sg = &cf->sg[2];
	qm_sg_entry_set64(&cf->sg[1], dpaa_mem_vtop(sg));
	if (is_encode(ses)) {
		qm_sg_entry_set64(sg, dpaa_mem_vtop(IV_ptr));
		sg->length = ses->iv.length;
		length += sg->length;
		cpu_to_hw_sg(sg);

		sg++;
		if (ses->auth_only_len) {
			qm_sg_entry_set64(sg,
					  dpaa_mem_vtop(sym->aead.aad.data));
			sg->length = ses->auth_only_len;
			length += sg->length;
			cpu_to_hw_sg(sg);
			sg++;
		}
		qm_sg_entry_set64(sg, src_start_addr + sym->aead.data.offset);
		sg->length = sym->aead.data.length;
		length += sg->length;
		sg->final = 1;
		cpu_to_hw_sg(sg);
	} else {
		qm_sg_entry_set64(sg, dpaa_mem_vtop(IV_ptr));
		sg->length = ses->iv.length;
		length += sg->length;
		cpu_to_hw_sg(sg);

		sg++;
		if (ses->auth_only_len) {
			qm_sg_entry_set64(sg,
					  dpaa_mem_vtop(sym->aead.aad.data));
			sg->length = ses->auth_only_len;
			length += sg->length;
			cpu_to_hw_sg(sg);
			sg++;
		}
		qm_sg_entry_set64(sg, src_start_addr + sym->aead.data.offset);
		sg->length = sym->aead.data.length;
		length += sg->length;
		cpu_to_hw_sg(sg);

		memcpy(ctx->digest, sym->aead.digest.data,
		       ses->digest_length);
		sg++;

		qm_sg_entry_set64(sg, dpaa_mem_vtop(ctx->digest));
		sg->length = ses->digest_length;
		length += sg->length;
		sg->final = 1;
		cpu_to_hw_sg(sg);
	}
	/* input compound frame */
	cf->sg[1].length = length;
	cf->sg[1].extension = 1;
	cf->sg[1].final = 1;
	cpu_to_hw_sg(&cf->sg[1]);

	/* output */
	sg++;
	qm_sg_entry_set64(&cf->sg[0], dpaa_mem_vtop(sg));
	qm_sg_entry_set64(sg,
		dst_start_addr + sym->aead.data.offset - ses->auth_only_len);
	sg->length = sym->aead.data.length + ses->auth_only_len;
	length = sg->length;
	if (is_encode(ses)) {
		cpu_to_hw_sg(sg);
		/* set auth output */
		sg++;
		qm_sg_entry_set64(sg, sym->aead.digest.phys_addr);
		sg->length = ses->digest_length;
		length += sg->length;
	}
	sg->final = 1;
	cpu_to_hw_sg(sg);

	/* output compound frame */
	cf->sg[0].length = length;
	cf->sg[0].extension = 1;
	cpu_to_hw_sg(&cf->sg[0]);

	return cf;
}

static inline struct dpaa_sec_job *
build_cipher_auth(struct rte_crypto_op *op, dpaa_sec_session *ses)
{
	struct rte_crypto_sym_op *sym = op->sym;
	struct dpaa_sec_job *cf;
	struct dpaa_sec_op_ctx *ctx;
	struct qm_sg_entry *sg;
	rte_iova_t src_start_addr, dst_start_addr;
	uint32_t length = 0;
	uint8_t *IV_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			ses->iv.offset);

	src_start_addr = sym->m_src->buf_iova + sym->m_src->data_off;
	if (sym->m_dst)
		dst_start_addr = sym->m_dst->buf_iova + sym->m_dst->data_off;
	else
		dst_start_addr = src_start_addr;

	ctx = dpaa_sec_alloc_ctx(ses);
	if (!ctx)
		return NULL;

	cf = &ctx->job;
	ctx->op = op;

	/* input */
	rte_prefetch0(cf->sg);
	sg = &cf->sg[2];
	qm_sg_entry_set64(&cf->sg[1], dpaa_mem_vtop(sg));
	if (is_encode(ses)) {
		qm_sg_entry_set64(sg, dpaa_mem_vtop(IV_ptr));
		sg->length = ses->iv.length;
		length += sg->length;
		cpu_to_hw_sg(sg);

		sg++;
		qm_sg_entry_set64(sg, src_start_addr + sym->auth.data.offset);
		sg->length = sym->auth.data.length;
		length += sg->length;
		sg->final = 1;
		cpu_to_hw_sg(sg);
	} else {
		qm_sg_entry_set64(sg, dpaa_mem_vtop(IV_ptr));
		sg->length = ses->iv.length;
		length += sg->length;
		cpu_to_hw_sg(sg);

		sg++;

		qm_sg_entry_set64(sg, src_start_addr + sym->auth.data.offset);
		sg->length = sym->auth.data.length;
		length += sg->length;
		cpu_to_hw_sg(sg);

		memcpy(ctx->digest, sym->auth.digest.data,
		       ses->digest_length);
		sg++;

		qm_sg_entry_set64(sg, dpaa_mem_vtop(ctx->digest));
		sg->length = ses->digest_length;
		length += sg->length;
		sg->final = 1;
		cpu_to_hw_sg(sg);
	}
	/* input compound frame */
	cf->sg[1].length = length;
	cf->sg[1].extension = 1;
	cf->sg[1].final = 1;
	cpu_to_hw_sg(&cf->sg[1]);

	/* output */
	sg++;
	qm_sg_entry_set64(&cf->sg[0], dpaa_mem_vtop(sg));
	qm_sg_entry_set64(sg, dst_start_addr + sym->cipher.data.offset);
	sg->length = sym->cipher.data.length;
	length = sg->length;
	if (is_encode(ses)) {
		cpu_to_hw_sg(sg);
		/* set auth output */
		sg++;
		qm_sg_entry_set64(sg, sym->auth.digest.phys_addr);
		sg->length = ses->digest_length;
		length += sg->length;
	}
	sg->final = 1;
	cpu_to_hw_sg(sg);

	/* output compound frame */
	cf->sg[0].length = length;
	cf->sg[0].extension = 1;
	cpu_to_hw_sg(&cf->sg[0]);

	return cf;
}

static int
dpaa_sec_enqueue_op(struct rte_crypto_op *op,  struct dpaa_sec_qp *qp)
{
	struct dpaa_sec_job *cf;
	dpaa_sec_session *ses;
	struct qm_fd fd;
	int ret;
	uint32_t auth_only_len = op->sym->auth.data.length -
				op->sym->cipher.data.length;

	ses = (dpaa_sec_session *)get_session_private_data(op->sym->session,
					cryptodev_driver_id);

	if (unlikely(!qp->ses || qp->ses != ses)) {
		qp->ses = ses;
		ses->qp = qp;
		ret = dpaa_sec_prep_cdb(ses);
		if (ret)
			return ret;
	}

	/*
	 * Segmented buffer is not supported.
	 */
	if (!rte_pktmbuf_is_contiguous(op->sym->m_src)) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return -ENOTSUP;
	}
	if (is_auth_only(ses)) {
		cf = build_auth_only(op, ses);
	} else if (is_cipher_only(ses)) {
		cf = build_cipher_only(op, ses);
	} else if (is_aead(ses)) {
		cf = build_cipher_auth_gcm(op, ses);
		auth_only_len = ses->auth_only_len;
	} else if (is_auth_cipher(ses)) {
		cf = build_cipher_auth(op, ses);
	} else {
		PMD_TX_LOG(ERR, "not supported sec op");
		return -ENOTSUP;
	}
	if (unlikely(!cf))
		return -ENOMEM;

	memset(&fd, 0, sizeof(struct qm_fd));
	qm_fd_addr_set64(&fd, dpaa_mem_vtop(cf->sg));
	fd._format1 = qm_fd_compound;
	fd.length29 = 2 * sizeof(struct qm_sg_entry);
	/* Auth_only_len is set as 0 in descriptor and it is overwritten
	 * here in the fd.cmd which will update the DPOVRD reg.
	 */
	if (auth_only_len)
		fd.cmd = 0x80000000 | auth_only_len;
	do {
		ret = qman_enqueue(&qp->inq, &fd, 0);
	} while (ret != 0);

	return 0;
}

static uint16_t
dpaa_sec_enqueue_burst(void *qp, struct rte_crypto_op **ops,
		       uint16_t nb_ops)
{
	/* Function to transmit the frames to given device and queuepair */
	uint32_t loop;
	int32_t ret;
	struct dpaa_sec_qp *dpaa_qp = (struct dpaa_sec_qp *)qp;
	uint16_t num_tx = 0;

	if (unlikely(nb_ops == 0))
		return 0;

	/*Prepare each packet which is to be sent*/
	for (loop = 0; loop < nb_ops; loop++) {
		if (ops[loop]->sess_type != RTE_CRYPTO_OP_WITH_SESSION) {
			PMD_TX_LOG(ERR, "sessionless crypto op not supported");
			return 0;
		}
		ret = dpaa_sec_enqueue_op(ops[loop], dpaa_qp);
		if (!ret)
			num_tx++;
	}
	dpaa_qp->tx_pkts += num_tx;
	dpaa_qp->tx_errs += nb_ops - num_tx;

	return num_tx;
}

static uint16_t
dpaa_sec_dequeue_burst(void *qp, struct rte_crypto_op **ops,
		       uint16_t nb_ops)
{
	uint16_t num_rx;
	struct dpaa_sec_qp *dpaa_qp = (struct dpaa_sec_qp *)qp;

	num_rx = dpaa_sec_deq(dpaa_qp, ops, nb_ops);

	dpaa_qp->rx_pkts += num_rx;
	dpaa_qp->rx_errs += nb_ops - num_rx;

	PMD_RX_LOG(DEBUG, "SEC Received %d Packets\n", num_rx);

	return num_rx;
}

/** Release queue pair */
static int
dpaa_sec_queue_pair_release(struct rte_cryptodev *dev,
			    uint16_t qp_id)
{
	struct dpaa_sec_dev_private *internals;
	struct dpaa_sec_qp *qp = NULL;

	PMD_INIT_FUNC_TRACE();

	PMD_INIT_LOG(DEBUG, "dev =%p, queue =%d", dev, qp_id);

	internals = dev->data->dev_private;
	if (qp_id >= internals->max_nb_queue_pairs) {
		PMD_INIT_LOG(ERR, "Max supported qpid %d",
			     internals->max_nb_queue_pairs);
		return -EINVAL;
	}

	qp = &internals->qps[qp_id];
	qp->internals = NULL;
	dev->data->queue_pairs[qp_id] = NULL;

	return 0;
}

/** Setup a queue pair */
static int
dpaa_sec_queue_pair_setup(struct rte_cryptodev *dev, uint16_t qp_id,
		__rte_unused const struct rte_cryptodev_qp_conf *qp_conf,
		__rte_unused int socket_id,
		__rte_unused struct rte_mempool *session_pool)
{
	struct dpaa_sec_dev_private *internals;
	struct dpaa_sec_qp *qp = NULL;

	PMD_INIT_LOG(DEBUG, "dev =%p, queue =%d, conf =%p",
		     dev, qp_id, qp_conf);

	internals = dev->data->dev_private;
	if (qp_id >= internals->max_nb_queue_pairs) {
		PMD_INIT_LOG(ERR, "Max supported qpid %d",
			     internals->max_nb_queue_pairs);
		return -EINVAL;
	}

	qp = &internals->qps[qp_id];
	qp->internals = internals;
	dev->data->queue_pairs[qp_id] = qp;

	return 0;
}

/** Start queue pair */
static int
dpaa_sec_queue_pair_start(__rte_unused struct rte_cryptodev *dev,
			  __rte_unused uint16_t queue_pair_id)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

/** Stop queue pair */
static int
dpaa_sec_queue_pair_stop(__rte_unused struct rte_cryptodev *dev,
			 __rte_unused uint16_t queue_pair_id)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

/** Return the number of allocated queue pairs */
static uint32_t
dpaa_sec_queue_pair_count(struct rte_cryptodev *dev)
{
	PMD_INIT_FUNC_TRACE();

	return dev->data->nb_queue_pairs;
}

/** Returns the size of session structure */
static unsigned int
dpaa_sec_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return sizeof(dpaa_sec_session);
}

static int
dpaa_sec_cipher_init(struct rte_cryptodev *dev __rte_unused,
		     struct rte_crypto_sym_xform *xform,
		     dpaa_sec_session *session)
{
	session->cipher_alg = xform->cipher.algo;
	session->iv.length = xform->cipher.iv.length;
	session->iv.offset = xform->cipher.iv.offset;
	session->cipher_key.data = rte_zmalloc(NULL, xform->cipher.key.length,
					       RTE_CACHE_LINE_SIZE);
	if (session->cipher_key.data == NULL && xform->cipher.key.length > 0) {
		PMD_INIT_LOG(ERR, "No Memory for cipher key\n");
		return -ENOMEM;
	}
	session->cipher_key.length = xform->cipher.key.length;

	memcpy(session->cipher_key.data, xform->cipher.key.data,
	       xform->cipher.key.length);
	session->dir = (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) ?
			DIR_ENC : DIR_DEC;

	return 0;
}

static int
dpaa_sec_auth_init(struct rte_cryptodev *dev __rte_unused,
		   struct rte_crypto_sym_xform *xform,
		   dpaa_sec_session *session)
{
	session->auth_alg = xform->auth.algo;
	session->auth_key.data = rte_zmalloc(NULL, xform->auth.key.length,
					     RTE_CACHE_LINE_SIZE);
	if (session->auth_key.data == NULL && xform->auth.key.length > 0) {
		PMD_INIT_LOG(ERR, "No Memory for auth key\n");
		return -ENOMEM;
	}
	session->auth_key.length = xform->auth.key.length;
	session->digest_length = xform->auth.digest_length;

	memcpy(session->auth_key.data, xform->auth.key.data,
	       xform->auth.key.length);
	session->dir = (xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE) ?
			DIR_ENC : DIR_DEC;

	return 0;
}

static int
dpaa_sec_aead_init(struct rte_cryptodev *dev __rte_unused,
		   struct rte_crypto_sym_xform *xform,
		   dpaa_sec_session *session)
{
	session->aead_alg = xform->aead.algo;
	session->iv.length = xform->aead.iv.length;
	session->iv.offset = xform->aead.iv.offset;
	session->auth_only_len = xform->aead.aad_length;
	session->aead_key.data = rte_zmalloc(NULL, xform->aead.key.length,
					     RTE_CACHE_LINE_SIZE);
	if (session->aead_key.data == NULL && xform->aead.key.length > 0) {
		PMD_INIT_LOG(ERR, "No Memory for aead key\n");
		return -ENOMEM;
	}
	session->aead_key.length = xform->aead.key.length;
	session->digest_length = xform->aead.digest_length;

	memcpy(session->aead_key.data, xform->aead.key.data,
	       xform->aead.key.length);
	session->dir = (xform->aead.op == RTE_CRYPTO_AEAD_OP_ENCRYPT) ?
			DIR_ENC : DIR_DEC;

	return 0;
}

static int
dpaa_sec_qp_attach_sess(struct rte_cryptodev *dev, uint16_t qp_id, void *ses)
{
	dpaa_sec_session *sess = ses;
	struct dpaa_sec_qp *qp;

	PMD_INIT_FUNC_TRACE();

	qp = dev->data->queue_pairs[qp_id];
	if (qp->ses != NULL) {
		PMD_INIT_LOG(ERR, "qp in-use by another session\n");
		return -EBUSY;
	}

	qp->ses = sess;
	sess->qp = qp;

	return dpaa_sec_prep_cdb(sess);
}

static int
dpaa_sec_qp_detach_sess(struct rte_cryptodev *dev, uint16_t qp_id, void *ses)
{
	dpaa_sec_session *sess = ses;
	struct dpaa_sec_qp *qp;

	PMD_INIT_FUNC_TRACE();

	qp = dev->data->queue_pairs[qp_id];
	if (qp->ses != NULL) {
		qp->ses = NULL;
		sess->qp = NULL;
		return 0;
	}

	PMD_DRV_LOG(ERR, "No session attached to qp");
	return -EINVAL;
}

static int
dpaa_sec_set_session_parameters(struct rte_cryptodev *dev,
			    struct rte_crypto_sym_xform *xform,	void *sess)
{
	struct dpaa_sec_dev_private *internals = dev->data->dev_private;
	dpaa_sec_session *session = sess;

	PMD_INIT_FUNC_TRACE();

	if (unlikely(sess == NULL)) {
		RTE_LOG(ERR, PMD, "invalid session struct\n");
		return -EINVAL;
	}

	/* Default IV length = 0 */
	session->iv.length = 0;

	/* Cipher Only */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER && xform->next == NULL) {
		session->auth_alg = RTE_CRYPTO_AUTH_NULL;
		dpaa_sec_cipher_init(dev, xform, session);

	/* Authentication Only */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		   xform->next == NULL) {
		session->cipher_alg = RTE_CRYPTO_CIPHER_NULL;
		dpaa_sec_auth_init(dev, xform, session);

	/* Cipher then Authenticate */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		   xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
			dpaa_sec_cipher_init(dev, xform, session);
			dpaa_sec_auth_init(dev, xform->next, session);
		} else {
			PMD_DRV_LOG(ERR, "Not supported: Auth then Cipher");
			return -EINVAL;
		}

	/* Authenticate then Cipher */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		   xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (xform->next->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
			dpaa_sec_auth_init(dev, xform, session);
			dpaa_sec_cipher_init(dev, xform->next, session);
		} else {
			PMD_DRV_LOG(ERR, "Not supported: Auth then Cipher");
			return -EINVAL;
		}

	/* AEAD operation for AES-GCM kind of Algorithms */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD &&
		   xform->next == NULL) {
		dpaa_sec_aead_init(dev, xform, session);

	} else {
		PMD_DRV_LOG(ERR, "Invalid crypto type");
		return -EINVAL;
	}
	session->ctx_pool = internals->ctx_pool;

	return 0;
}

static int
dpaa_sec_session_configure(struct rte_cryptodev *dev,
		struct rte_crypto_sym_xform *xform,
		struct rte_cryptodev_sym_session *sess,
		struct rte_mempool *mempool)
{
	void *sess_private_data;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (rte_mempool_get(mempool, &sess_private_data)) {
		CDEV_LOG_ERR(
			"Couldn't get object from session mempool");
		return -ENOMEM;
	}

	ret = dpaa_sec_set_session_parameters(dev, xform, sess_private_data);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "DPAA PMD: failed to configure "
				"session parameters");

		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}

	set_session_private_data(sess, dev->driver_id,
			sess_private_data);

	return 0;
}

/** Clear the memory of session so it doesn't leave key material behind */
static void
dpaa_sec_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	PMD_INIT_FUNC_TRACE();
	uint8_t index = dev->driver_id;
	void *sess_priv = get_session_private_data(sess, index);
	dpaa_sec_session *s = (dpaa_sec_session *)sess_priv;

	if (sess_priv) {
		rte_free(s->cipher_key.data);
		rte_free(s->auth_key.data);
		memset(s, 0, sizeof(dpaa_sec_session));
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);
		set_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}

static int
dpaa_sec_dev_configure(struct rte_cryptodev *dev __rte_unused,
		       struct rte_cryptodev_config *config __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static int
dpaa_sec_dev_start(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	return 0;
}

static void
dpaa_sec_dev_stop(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
}

static int
dpaa_sec_dev_close(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	return 0;
}

static void
dpaa_sec_dev_infos_get(struct rte_cryptodev *dev,
		       struct rte_cryptodev_info *info)
{
	struct dpaa_sec_dev_private *internals = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	if (info != NULL) {
		info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		info->feature_flags = dev->feature_flags;
		info->capabilities = dpaa_sec_capabilities;
		info->sym.max_nb_sessions = internals->max_nb_sessions;
		info->sym.max_nb_sessions_per_qp =
			RTE_DPAA_SEC_PMD_MAX_NB_SESSIONS / RTE_MAX_NB_SEC_QPS;
		info->driver_id = cryptodev_driver_id;
	}
}

static struct rte_cryptodev_ops crypto_ops = {
	.dev_configure	      = dpaa_sec_dev_configure,
	.dev_start	      = dpaa_sec_dev_start,
	.dev_stop	      = dpaa_sec_dev_stop,
	.dev_close	      = dpaa_sec_dev_close,
	.dev_infos_get        = dpaa_sec_dev_infos_get,
	.queue_pair_setup     = dpaa_sec_queue_pair_setup,
	.queue_pair_release   = dpaa_sec_queue_pair_release,
	.queue_pair_start     = dpaa_sec_queue_pair_start,
	.queue_pair_stop      = dpaa_sec_queue_pair_stop,
	.queue_pair_count     = dpaa_sec_queue_pair_count,
	.session_get_size     = dpaa_sec_session_get_size,
	.session_configure    = dpaa_sec_session_configure,
	.session_clear        = dpaa_sec_session_clear,
	.qp_attach_session    = dpaa_sec_qp_attach_sess,
	.qp_detach_session    = dpaa_sec_qp_detach_sess,
};

static int
dpaa_sec_uninit(struct rte_cryptodev *dev)
{
	struct dpaa_sec_dev_private *internals = dev->data->dev_private;

	if (dev == NULL)
		return -ENODEV;

	rte_mempool_free(internals->ctx_pool);
	rte_free(internals);

	PMD_INIT_LOG(INFO, "Closing DPAA_SEC device %s on numa socket %u\n",
		     dev->data->name, rte_socket_id());

	return 0;
}

static int
dpaa_sec_dev_init(struct rte_cryptodev *cryptodev)
{
	struct dpaa_sec_dev_private *internals;
	struct dpaa_sec_qp *qp;
	uint32_t i;
	int ret;
	char str[20];

	PMD_INIT_FUNC_TRACE();

	cryptodev->driver_id = cryptodev_driver_id;
	cryptodev->dev_ops = &crypto_ops;

	cryptodev->enqueue_burst = dpaa_sec_enqueue_burst;
	cryptodev->dequeue_burst = dpaa_sec_dequeue_burst;
	cryptodev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING;

	internals = cryptodev->data->dev_private;
	internals->max_nb_queue_pairs = RTE_MAX_NB_SEC_QPS;
	internals->max_nb_sessions = RTE_DPAA_SEC_PMD_MAX_NB_SESSIONS;

	for (i = 0; i < internals->max_nb_queue_pairs; i++) {
		/* init qman fq for queue pair */
		qp = &internals->qps[i];
		ret = dpaa_sec_init_tx(&qp->outq);
		if (ret) {
			PMD_INIT_LOG(ERR, "config tx of queue pair  %d", i);
			goto init_error;
		}
		ret = dpaa_sec_init_rx(&qp->inq, dpaa_mem_vtop(&qp->cdb),
				       qman_fq_fqid(&qp->outq));
		if (ret) {
			PMD_INIT_LOG(ERR, "config rx of queue pair %d", i);
			goto init_error;
		}
	}

	sprintf(str, "ctx_pool_%d", cryptodev->data->dev_id);
	internals->ctx_pool = rte_mempool_create((const char *)str,
			CTX_POOL_NUM_BUFS,
			CTX_POOL_BUF_SIZE,
			CTX_POOL_CACHE_SIZE, 0,
			NULL, NULL, NULL, NULL,
			SOCKET_ID_ANY, 0);
	if (!internals->ctx_pool) {
		RTE_LOG(ERR, PMD, "%s create failed\n", str);
		goto init_error;
	}

	PMD_INIT_LOG(DEBUG, "driver %s: created\n", cryptodev->data->name);
	return 0;

init_error:
	PMD_INIT_LOG(ERR, "driver %s: create failed\n", cryptodev->data->name);

	dpaa_sec_uninit(cryptodev);
	return -EFAULT;
}

static int
cryptodev_dpaa_sec_probe(struct rte_dpaa_driver *dpaa_drv,
				struct rte_dpaa_device *dpaa_dev)
{
	struct rte_cryptodev *cryptodev;
	char cryptodev_name[RTE_CRYPTODEV_NAME_MAX_LEN];

	int retval;

	sprintf(cryptodev_name, "dpaa_sec-%d", dpaa_dev->id.dev_id);

	cryptodev = rte_cryptodev_pmd_allocate(cryptodev_name, rte_socket_id());
	if (cryptodev == NULL)
		return -ENOMEM;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		cryptodev->data->dev_private = rte_zmalloc_socket(
					"cryptodev private structure",
					sizeof(struct dpaa_sec_dev_private),
					RTE_CACHE_LINE_SIZE,
					rte_socket_id());

		if (cryptodev->data->dev_private == NULL)
			rte_panic("Cannot allocate memzone for private "
					"device data");
	}

	dpaa_dev->crypto_dev = cryptodev;
	cryptodev->device = &dpaa_dev->device;
	cryptodev->device->driver = &dpaa_drv->driver;

	/* init user callbacks */
	TAILQ_INIT(&(cryptodev->link_intr_cbs));

	/* if sec device version is not configured */
	if (!rta_get_sec_era()) {
		const struct device_node *caam_node;

		for_each_compatible_node(caam_node, NULL, "fsl,sec-v4.0") {
			const uint32_t *prop = of_get_property(caam_node,
					"fsl,sec-era",
					NULL);
			if (prop) {
				rta_set_sec_era(
					INTL_SEC_ERA(rte_cpu_to_be_32(*prop)));
				break;
			}
		}
	}

	/* Invoke PMD device initialization function */
	retval = dpaa_sec_dev_init(cryptodev);
	if (retval == 0)
		return 0;

	/* In case of error, cleanup is done */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(cryptodev->data->dev_private);

	rte_cryptodev_pmd_release_device(cryptodev);

	return -ENXIO;
}

static int
cryptodev_dpaa_sec_remove(struct rte_dpaa_device *dpaa_dev)
{
	struct rte_cryptodev *cryptodev;
	int ret;

	cryptodev = dpaa_dev->crypto_dev;
	if (cryptodev == NULL)
		return -ENODEV;

	ret = dpaa_sec_uninit(cryptodev);
	if (ret)
		return ret;

	return rte_cryptodev_pmd_destroy(cryptodev);
}

static struct rte_dpaa_driver rte_dpaa_sec_driver = {
	.drv_type = FSL_DPAA_CRYPTO,
	.driver = {
		.name = "DPAA SEC PMD"
	},
	.probe = cryptodev_dpaa_sec_probe,
	.remove = cryptodev_dpaa_sec_remove,
};

static struct cryptodev_driver dpaa_sec_crypto_drv;

RTE_PMD_REGISTER_DPAA(CRYPTODEV_NAME_DPAA_SEC_PMD, rte_dpaa_sec_driver);
RTE_PMD_REGISTER_CRYPTO_DRIVER(dpaa_sec_crypto_drv, rte_dpaa_sec_driver,
		cryptodev_driver_id);
