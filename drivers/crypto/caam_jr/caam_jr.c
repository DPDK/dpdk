/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2019 NXP
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
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_security_driver.h>
#include <rte_hexdump.h>

#include <caam_jr_capabilities.h>
#include <caam_jr_config.h>
#include <caam_jr_hw_specific.h>
#include <caam_jr_pvt.h>
#include <caam_jr_desc.h>
#include <caam_jr_log.h>

/* RTA header files */
#include <desc/common.h>
#include <desc/algo.h>
#include <dpaa_of.h>
#ifdef RTE_LIBRTE_PMD_CAAM_JR_DEBUG
#define CAAM_JR_DBG    1
#else
#define CAAM_JR_DBG	0
#endif
#define CRYPTODEV_NAME_CAAM_JR_PMD	crypto_caam_jr
static uint8_t cryptodev_driver_id;

/* Lists the states possible for the SEC user space driver. */
enum sec_driver_state_e {
	SEC_DRIVER_STATE_IDLE,		/* Driver not initialized */
	SEC_DRIVER_STATE_STARTED,	/* Driver initialized and can be used*/
	SEC_DRIVER_STATE_RELEASE,	/* Driver release is in progress */
};

/* Job rings used for communication with SEC HW */
static struct sec_job_ring_t g_job_rings[MAX_SEC_JOB_RINGS];

/* The current state of SEC user space driver */
static enum sec_driver_state_e g_driver_state = SEC_DRIVER_STATE_IDLE;

/* The number of job rings used by SEC user space driver */
static int g_job_rings_no;
static int g_job_rings_max;

struct sec_outring_entry {
	phys_addr_t desc;	/* Pointer to completed descriptor */
	uint32_t status;	/* Status for completed descriptor */
} __rte_packed;

/* virtual address conversin when mempool support is available for ctx */
static inline phys_addr_t
caam_jr_vtop_ctx(struct caam_jr_op_ctx *ctx, void *vaddr)
{
	return (size_t)vaddr - ctx->vtop_offset;
}

static inline void
caam_jr_op_ending(struct caam_jr_op_ctx *ctx)
{
	/* report op status to sym->op and then free the ctx memory  */
	rte_mempool_put(ctx->ctx_pool, (void *)ctx);
}

static inline struct caam_jr_op_ctx *
caam_jr_alloc_ctx(struct caam_jr_session *ses)
{
	struct caam_jr_op_ctx *ctx;
	int ret;

	ret = rte_mempool_get(ses->ctx_pool, (void **)(&ctx));
	if (!ctx || ret) {
		CAAM_JR_DP_WARN("Alloc sec descriptor failed!");
		return NULL;
	}
	/*
	 * Clear SG memory. There are 16 SG entries of 16 Bytes each.
	 * one call to dcbz_64() clear 64 bytes, hence calling it 4 times
	 * to clear all the SG entries. caam_jr_alloc_ctx() is called for
	 * each packet, memset is costlier than dcbz_64().
	 */
	dcbz_64(&ctx->sg[SG_CACHELINE_0]);
	dcbz_64(&ctx->sg[SG_CACHELINE_1]);
	dcbz_64(&ctx->sg[SG_CACHELINE_2]);
	dcbz_64(&ctx->sg[SG_CACHELINE_3]);

	ctx->ctx_pool = ses->ctx_pool;
	ctx->vtop_offset = (size_t) ctx - rte_mempool_virt2iova(ctx);

	return ctx;
}

static
void caam_jr_stats_get(struct rte_cryptodev *dev,
			struct rte_cryptodev_stats *stats)
{
	struct caam_jr_qp **qp = (struct caam_jr_qp **)
					dev->data->queue_pairs;
	int i;

	PMD_INIT_FUNC_TRACE();
	if (stats == NULL) {
		CAAM_JR_ERR("Invalid stats ptr NULL");
		return;
	}
	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		if (qp[i] == NULL) {
			CAAM_JR_WARN("Uninitialised queue pair");
			continue;
		}

		stats->enqueued_count += qp[i]->tx_pkts;
		stats->dequeued_count += qp[i]->rx_pkts;
		stats->enqueue_err_count += qp[i]->tx_errs;
		stats->dequeue_err_count += qp[i]->rx_errs;
		CAAM_JR_INFO("extra stats:\n\tRX Poll ERR = %" PRIu64
			     "\n\tTX Ring Full = %" PRIu64,
			     qp[i]->rx_poll_err,
			     qp[i]->tx_ring_full);
	}
}

static
void caam_jr_stats_reset(struct rte_cryptodev *dev)
{
	int i;
	struct caam_jr_qp **qp = (struct caam_jr_qp **)
				   (dev->data->queue_pairs);

	PMD_INIT_FUNC_TRACE();
	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		if (qp[i] == NULL) {
			CAAM_JR_WARN("Uninitialised queue pair");
			continue;
		}
		qp[i]->rx_pkts = 0;
		qp[i]->rx_errs = 0;
		qp[i]->rx_poll_err = 0;
		qp[i]->tx_pkts = 0;
		qp[i]->tx_errs = 0;
		qp[i]->tx_ring_full = 0;
	}
}

static inline int
is_cipher_only(struct caam_jr_session *ses)
{
	return ((ses->cipher_alg != RTE_CRYPTO_CIPHER_NULL) &&
		(ses->auth_alg == RTE_CRYPTO_AUTH_NULL));
}

static inline int
is_auth_only(struct caam_jr_session *ses)
{
	return ((ses->cipher_alg == RTE_CRYPTO_CIPHER_NULL) &&
		(ses->auth_alg != RTE_CRYPTO_AUTH_NULL));
}

static inline int
is_aead(struct caam_jr_session *ses)
{
	return ((ses->cipher_alg == 0) &&
		(ses->auth_alg == 0) &&
		(ses->aead_alg != 0));
}

static inline int
is_auth_cipher(struct caam_jr_session *ses)
{
	return ((ses->cipher_alg != RTE_CRYPTO_CIPHER_NULL) &&
		(ses->auth_alg != RTE_CRYPTO_AUTH_NULL) &&
		(ses->proto_alg != RTE_SECURITY_PROTOCOL_IPSEC));
}

static inline int
is_proto_ipsec(struct caam_jr_session *ses)
{
	return (ses->proto_alg == RTE_SECURITY_PROTOCOL_IPSEC);
}

static inline int
is_encode(struct caam_jr_session *ses)
{
	return ses->dir == DIR_ENC;
}

static inline int
is_decode(struct caam_jr_session *ses)
{
	return ses->dir == DIR_DEC;
}

static inline void
caam_auth_alg(struct caam_jr_session *ses, struct alginfo *alginfo_a)
{
	switch (ses->auth_alg) {
	case RTE_CRYPTO_AUTH_NULL:
		ses->digest_length = 0;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		alginfo_a->algtype =
			(ses->proto_alg == RTE_SECURITY_PROTOCOL_IPSEC) ?
			OP_PCL_IPSEC_HMAC_MD5_96 : OP_ALG_ALGSEL_MD5;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		alginfo_a->algtype =
			(ses->proto_alg == RTE_SECURITY_PROTOCOL_IPSEC) ?
			OP_PCL_IPSEC_HMAC_SHA1_96 : OP_ALG_ALGSEL_SHA1;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		alginfo_a->algtype =
			(ses->proto_alg == RTE_SECURITY_PROTOCOL_IPSEC) ?
			OP_PCL_IPSEC_HMAC_SHA1_160 : OP_ALG_ALGSEL_SHA224;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		alginfo_a->algtype =
			(ses->proto_alg == RTE_SECURITY_PROTOCOL_IPSEC) ?
			OP_PCL_IPSEC_HMAC_SHA2_256_128 : OP_ALG_ALGSEL_SHA256;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		alginfo_a->algtype =
			(ses->proto_alg == RTE_SECURITY_PROTOCOL_IPSEC) ?
			OP_PCL_IPSEC_HMAC_SHA2_384_192 : OP_ALG_ALGSEL_SHA384;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		alginfo_a->algtype =
			(ses->proto_alg == RTE_SECURITY_PROTOCOL_IPSEC) ?
			OP_PCL_IPSEC_HMAC_SHA2_512_256 : OP_ALG_ALGSEL_SHA512;
		alginfo_a->algmode = OP_ALG_AAI_HMAC;
		break;
	default:
		CAAM_JR_DEBUG("unsupported auth alg %u", ses->auth_alg);
	}
}

static inline void
caam_cipher_alg(struct caam_jr_session *ses, struct alginfo *alginfo_c)
{
	switch (ses->cipher_alg) {
	case RTE_CRYPTO_CIPHER_NULL:
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		alginfo_c->algtype =
			(ses->proto_alg == RTE_SECURITY_PROTOCOL_IPSEC) ?
			OP_PCL_IPSEC_AES_CBC : OP_ALG_ALGSEL_AES;
		alginfo_c->algmode = OP_ALG_AAI_CBC;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		alginfo_c->algtype =
			(ses->proto_alg == RTE_SECURITY_PROTOCOL_IPSEC) ?
			OP_PCL_IPSEC_3DES : OP_ALG_ALGSEL_3DES;
		alginfo_c->algmode = OP_ALG_AAI_CBC;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		alginfo_c->algtype =
			(ses->proto_alg == RTE_SECURITY_PROTOCOL_IPSEC) ?
			OP_PCL_IPSEC_AES_CTR : OP_ALG_ALGSEL_AES;
		alginfo_c->algmode = OP_ALG_AAI_CTR;
		break;
	default:
		CAAM_JR_DEBUG("unsupported cipher alg %d", ses->cipher_alg);
	}
}

static inline void
caam_aead_alg(struct caam_jr_session *ses, struct alginfo *alginfo)
{
	switch (ses->aead_alg) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		alginfo->algtype = OP_ALG_ALGSEL_AES;
		alginfo->algmode = OP_ALG_AAI_GCM;
		break;
	default:
		CAAM_JR_DEBUG("unsupported AEAD alg %d", ses->aead_alg);
	}
}

/* prepare command block of the session */
static int
caam_jr_prep_cdb(struct caam_jr_session *ses)
{
	struct alginfo alginfo_c = {0}, alginfo_a = {0}, alginfo = {0};
	int32_t shared_desc_len = 0;
	struct sec_cdb *cdb;
	int err;
#if CAAM_BYTE_ORDER == CORE_BYTE_ORDER
	int swap = false;
#else
	int swap = true;
#endif

	if (ses->cdb)
		caam_jr_dma_free(ses->cdb);

	cdb = caam_jr_dma_mem_alloc(L1_CACHE_BYTES, sizeof(struct sec_cdb));
	if (!cdb) {
		CAAM_JR_ERR("failed to allocate memory for cdb\n");
		return -1;
	}

	ses->cdb = cdb;

	memset(cdb, 0, sizeof(struct sec_cdb));

	if (is_cipher_only(ses)) {
		caam_cipher_alg(ses, &alginfo_c);
		if (alginfo_c.algtype == (unsigned int)CAAM_JR_ALG_UNSUPPORT) {
			CAAM_JR_ERR("not supported cipher alg");
			rte_free(cdb);
			return -ENOTSUP;
		}

		alginfo_c.key = (size_t)ses->cipher_key.data;
		alginfo_c.keylen = ses->cipher_key.length;
		alginfo_c.key_enc_flags = 0;
		alginfo_c.key_type = RTA_DATA_IMM;

		shared_desc_len = cnstr_shdsc_blkcipher(
						cdb->sh_desc, true,
						swap, SHR_NEVER, &alginfo_c,
						ses->iv.length,
						ses->dir);
	} else if (is_auth_only(ses)) {
		caam_auth_alg(ses, &alginfo_a);
		if (alginfo_a.algtype == (unsigned int)CAAM_JR_ALG_UNSUPPORT) {
			CAAM_JR_ERR("not supported auth alg");
			rte_free(cdb);
			return -ENOTSUP;
		}

		alginfo_a.key = (size_t)ses->auth_key.data;
		alginfo_a.keylen = ses->auth_key.length;
		alginfo_a.key_enc_flags = 0;
		alginfo_a.key_type = RTA_DATA_IMM;

		shared_desc_len = cnstr_shdsc_hmac(cdb->sh_desc, true,
						   swap, SHR_NEVER, &alginfo_a,
						   !ses->dir,
						   ses->digest_length);
	} else if (is_aead(ses)) {
		caam_aead_alg(ses, &alginfo);
		if (alginfo.algtype == (unsigned int)CAAM_JR_ALG_UNSUPPORT) {
			CAAM_JR_ERR("not supported aead alg");
			rte_free(cdb);
			return -ENOTSUP;
		}
		alginfo.key = (size_t)ses->aead_key.data;
		alginfo.keylen = ses->aead_key.length;
		alginfo.key_enc_flags = 0;
		alginfo.key_type = RTA_DATA_IMM;

		if (ses->dir == DIR_ENC)
			shared_desc_len = cnstr_shdsc_gcm_encap(
					cdb->sh_desc, true, swap,
					SHR_NEVER, &alginfo,
					ses->iv.length,
					ses->digest_length);
		else
			shared_desc_len = cnstr_shdsc_gcm_decap(
					cdb->sh_desc, true, swap,
					SHR_NEVER, &alginfo,
					ses->iv.length,
					ses->digest_length);
	} else {
		caam_cipher_alg(ses, &alginfo_c);
		if (alginfo_c.algtype == (unsigned int)CAAM_JR_ALG_UNSUPPORT) {
			CAAM_JR_ERR("not supported cipher alg");
			rte_free(cdb);
			return -ENOTSUP;
		}

		alginfo_c.key = (size_t)ses->cipher_key.data;
		alginfo_c.keylen = ses->cipher_key.length;
		alginfo_c.key_enc_flags = 0;
		alginfo_c.key_type = RTA_DATA_IMM;

		caam_auth_alg(ses, &alginfo_a);
		if (alginfo_a.algtype == (unsigned int)CAAM_JR_ALG_UNSUPPORT) {
			CAAM_JR_ERR("not supported auth alg");
			rte_free(cdb);
			return -ENOTSUP;
		}

		alginfo_a.key = (size_t)ses->auth_key.data;
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
			CAAM_JR_ERR("Crypto: Incorrect key lengths");
			rte_free(cdb);
			return err;
		}
		if (cdb->sh_desc[2] & 1)
			alginfo_c.key_type = RTA_DATA_IMM;
		else {
			alginfo_c.key = (size_t)caam_jr_mem_vtop(
						(void *)(size_t)alginfo_c.key);
			alginfo_c.key_type = RTA_DATA_PTR;
		}
		if (cdb->sh_desc[2] & (1<<1))
			alginfo_a.key_type = RTA_DATA_IMM;
		else {
			alginfo_a.key = (size_t)caam_jr_mem_vtop(
						(void *)(size_t)alginfo_a.key);
			alginfo_a.key_type = RTA_DATA_PTR;
		}
		cdb->sh_desc[0] = 0;
		cdb->sh_desc[1] = 0;
		cdb->sh_desc[2] = 0;
		if (is_proto_ipsec(ses)) {
			if (ses->dir == DIR_ENC) {
				shared_desc_len = cnstr_shdsc_ipsec_new_encap(
						cdb->sh_desc,
						true, swap, SHR_SERIAL,
						&ses->encap_pdb,
						(uint8_t *)&ses->ip4_hdr,
						&alginfo_c, &alginfo_a);
			} else if (ses->dir == DIR_DEC) {
				shared_desc_len = cnstr_shdsc_ipsec_new_decap(
						cdb->sh_desc,
						true, swap, SHR_SERIAL,
						&ses->decap_pdb,
						&alginfo_c, &alginfo_a);
			}
		} else {
			/* Auth_only_len is overwritten in fd for each job */
			shared_desc_len = cnstr_shdsc_authenc(cdb->sh_desc,
					true, swap, SHR_SERIAL,
					&alginfo_c, &alginfo_a,
					ses->iv.length,
					ses->digest_length, ses->dir);
		}
	}

	if (shared_desc_len < 0) {
		CAAM_JR_ERR("error in preparing command block");
		return shared_desc_len;
	}

#if CAAM_JR_DBG
	SEC_DUMP_DESC(cdb->sh_desc);
#endif

	cdb->sh_hdr.hi.field.idlen = shared_desc_len;

	return 0;
}

/* @brief Poll the HW for already processed jobs in the JR
 * and silently discard the available jobs or notify them to UA
 * with indicated error code.
 *
 * @param [in,out]  job_ring        The job ring to poll.
 * @param [in]  do_notify           Can be #TRUE or #FALSE. Indicates if
 *				    descriptors are to be discarded
 *                                  or notified to UA with given error_code.
 * @param [out] notified_descs    Number of notified descriptors. Can be NULL
 *					if do_notify is #FALSE
 */
static void
hw_flush_job_ring(struct sec_job_ring_t *job_ring,
		  uint32_t do_notify,
		  uint32_t *notified_descs)
{
	int32_t jobs_no_to_discard = 0;
	int32_t discarded_descs_no = 0;

	CAAM_JR_DEBUG("Jr[%p] pi[%d] ci[%d].Flushing jr notify desc=[%d]",
		job_ring, job_ring->pidx, job_ring->cidx, do_notify);

	jobs_no_to_discard = hw_get_no_finished_jobs(job_ring);

	/* Discard all jobs */
	CAAM_JR_DEBUG("Jr[%p] pi[%d] ci[%d].Discarding %d descs",
		  job_ring, job_ring->pidx, job_ring->cidx,
		  jobs_no_to_discard);

	while (jobs_no_to_discard > discarded_descs_no) {
		discarded_descs_no++;
		/* Now increment the consumer index for the current job ring,
		 * AFTER saving job in temporary location!
		 * Increment the consumer index for the current job ring
		 */
		job_ring->cidx = SEC_CIRCULAR_COUNTER(job_ring->cidx,
					 SEC_JOB_RING_SIZE);

		hw_remove_entries(job_ring, 1);
	}

	if (do_notify == true) {
		ASSERT(notified_descs != NULL);
		*notified_descs = discarded_descs_no;
	}
}

/* @brief Poll the HW for already processed jobs in the JR
 * and notify the available jobs to UA.
 *
 * @param [in]  job_ring	The job ring to poll.
 * @param [in]  limit           The maximum number of jobs to notify.
 *                              If set to negative value, all available jobs are
 *				notified.
 *
 * @retval >=0 for No of jobs notified to UA.
 * @retval -1 for error
 */
static int
hw_poll_job_ring(struct sec_job_ring_t *job_ring,
		 struct rte_crypto_op **ops, int32_t limit,
		 struct caam_jr_qp *jr_qp)
{
	int32_t jobs_no_to_notify = 0; /* the number of done jobs to notify*/
	int32_t number_of_jobs_available = 0;
	int32_t notified_descs_no = 0;
	uint32_t sec_error_code = 0;
	struct job_descriptor *current_desc;
	phys_addr_t current_desc_addr;
	phys_addr_t *temp_addr;
	struct caam_jr_op_ctx *ctx;

	/* TODO check for ops have memory*/
	/* check here if any JR error that cannot be written
	 * in the output status word has occurred
	 */
	if (JR_REG_JRINT_JRE_EXTRACT(GET_JR_REG(JRINT, job_ring))) {
		CAAM_JR_INFO("err received");
		sec_error_code = JR_REG_JRINT_ERR_TYPE_EXTRACT(
					GET_JR_REG(JRINT, job_ring));
		if (unlikely(sec_error_code)) {
			hw_job_ring_error_print(job_ring, sec_error_code);
			return -1;
		}
	}
	/* compute the number of jobs available in the job ring based on the
	 * producer and consumer index values.
	 */
	number_of_jobs_available = hw_get_no_finished_jobs(job_ring);
	/* Compute the number of notifications that need to be raised to UA
	 * If limit > total number of done jobs -> notify all done jobs
	 * If limit = 0 -> error
	 * If limit < total number of done jobs -> notify a number
	 * of done jobs equal with limit
	 */
	jobs_no_to_notify = (limit > number_of_jobs_available) ?
				number_of_jobs_available : limit;
	CAAM_JR_DP_DEBUG(
		"Jr[%p] pi[%d] ci[%d].limit =%d Available=%d.Jobs to notify=%d",
		job_ring, job_ring->pidx, job_ring->cidx,
		limit, number_of_jobs_available, jobs_no_to_notify);

	rte_smp_rmb();

	while (jobs_no_to_notify > notified_descs_no) {
		static uint64_t false_alarm;
		static uint64_t real_poll;

		/* Get job status here */
		sec_error_code = job_ring->output_ring[job_ring->cidx].status;
		/* Get completed descriptor */
		temp_addr = &(job_ring->output_ring[job_ring->cidx].desc);
		current_desc_addr = (phys_addr_t)sec_read_addr(temp_addr);

		real_poll++;
		/* todo check if it is false alarm no desc present */
		if (!current_desc_addr) {
			false_alarm++;
			printf("false alarm %" PRIu64 "real %" PRIu64
				" sec_err =0x%x cidx Index =0%d\n",
				false_alarm, real_poll,
				sec_error_code, job_ring->cidx);
			rte_panic("CAAM JR descriptor NULL");
			return notified_descs_no;
		}
		current_desc = (struct job_descriptor *)
				caam_jr_dma_ptov(current_desc_addr);
		/* now increment the consumer index for the current job ring,
		 * AFTER saving job in temporary location!
		 */
		job_ring->cidx = SEC_CIRCULAR_COUNTER(job_ring->cidx,
				 SEC_JOB_RING_SIZE);
		/* Signal that the job has been processed and the slot is free*/
		hw_remove_entries(job_ring, 1);
		/*TODO for multiple ops, packets*/
		ctx = container_of(current_desc, struct caam_jr_op_ctx, jobdes);
		if (unlikely(sec_error_code)) {
			CAAM_JR_ERR("desc at cidx %d generated error 0x%x\n",
				job_ring->cidx, sec_error_code);
			hw_handle_job_ring_error(job_ring, sec_error_code);
			//todo improve with exact errors
			ctx->op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			jr_qp->rx_errs++;
		} else {
			ctx->op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
#if CAAM_JR_DBG
			if (ctx->op->sym->m_dst) {
				rte_hexdump(stdout, "PROCESSED",
				rte_pktmbuf_mtod(ctx->op->sym->m_dst, void *),
				rte_pktmbuf_data_len(ctx->op->sym->m_dst));
			} else {
				rte_hexdump(stdout, "PROCESSED",
				rte_pktmbuf_mtod(ctx->op->sym->m_src, void *),
				rte_pktmbuf_data_len(ctx->op->sym->m_src));
			}
#endif
		}
		if (ctx->op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
			struct ip *ip4_hdr;

			if (ctx->op->sym->m_dst) {
				/*TODO check for ip header or other*/
				ip4_hdr = (struct ip *)
				rte_pktmbuf_mtod(ctx->op->sym->m_dst, char*);
				ctx->op->sym->m_dst->pkt_len =
					rte_be_to_cpu_16(ip4_hdr->ip_len);
				ctx->op->sym->m_dst->data_len =
					rte_be_to_cpu_16(ip4_hdr->ip_len);
			} else {
				ip4_hdr = (struct ip *)
				rte_pktmbuf_mtod(ctx->op->sym->m_src, char*);
				ctx->op->sym->m_src->pkt_len =
					rte_be_to_cpu_16(ip4_hdr->ip_len);
				ctx->op->sym->m_src->data_len =
					rte_be_to_cpu_16(ip4_hdr->ip_len);
			}
		}
		*ops = ctx->op;
		caam_jr_op_ending(ctx);
		ops++;
		notified_descs_no++;
	}
	return notified_descs_no;
}

static uint16_t
caam_jr_dequeue_burst(void *qp, struct rte_crypto_op **ops,
		       uint16_t nb_ops)
{
	struct caam_jr_qp *jr_qp = (struct caam_jr_qp *)qp;
	struct sec_job_ring_t *ring = jr_qp->ring;
	int num_rx;
	int ret;

	CAAM_JR_DP_DEBUG("Jr[%p]Polling. limit[%d]", ring, nb_ops);

	/* Poll job ring
	 * If nb_ops < 0 -> poll JR until no more notifications are available.
	 * If nb_ops > 0 -> poll JR until limit is reached.
	 */

	/* Run hw poll job ring */
	num_rx = hw_poll_job_ring(ring, ops, nb_ops, jr_qp);
	if (num_rx < 0) {
		CAAM_JR_ERR("Error polling SEC engine (%d)", num_rx);
		return 0;
	}

	CAAM_JR_DP_DEBUG("Jr[%p].Jobs notified[%d]. ", ring, num_rx);

	if (ring->jr_mode == SEC_NOTIFICATION_TYPE_NAPI) {
		if (num_rx < nb_ops) {
			ret = caam_jr_enable_irqs(ring->irq_fd);
			SEC_ASSERT(ret == 0, ret,
			"Failed to enable irqs for job ring %p", ring);
		}
	} else if (ring->jr_mode == SEC_NOTIFICATION_TYPE_IRQ) {

		/* Always enable IRQ generation when in pure IRQ mode */
		ret = caam_jr_enable_irqs(ring->irq_fd);
		SEC_ASSERT(ret == 0, ret,
			"Failed to enable irqs for job ring %p", ring);
	}

	jr_qp->rx_pkts += num_rx;

	return num_rx;
}

/**
 * packet looks like:
 *		|<----data_len------->|
 *    |ip_header|ah_header|icv|payload|
 *              ^
 *		|
 *	   mbuf->pkt.data
 */
static inline struct caam_jr_op_ctx *
build_auth_only_sg(struct rte_crypto_op *op, struct caam_jr_session *ses)
{
	struct rte_crypto_sym_op *sym = op->sym;
	struct rte_mbuf *mbuf = sym->m_src;
	struct caam_jr_op_ctx *ctx;
	struct sec4_sg_entry *sg;
	int	length;
	struct sec_cdb *cdb;
	uint64_t sdesc_offset;
	struct sec_job_descriptor_t *jobdescr;
	uint8_t extra_segs;

	if (is_decode(ses))
		extra_segs = 2;
	else
		extra_segs = 1;

	if ((mbuf->nb_segs + extra_segs) > MAX_SG_ENTRIES) {
		CAAM_JR_DP_ERR("Auth: Max sec segs supported is %d",
				MAX_SG_ENTRIES);
		return NULL;
	}

	ctx = caam_jr_alloc_ctx(ses);
	if (!ctx)
		return NULL;

	ctx->op = op;

	cdb = ses->cdb;
	sdesc_offset = (size_t) ((char *)&cdb->sh_desc - (char *)cdb);

	jobdescr = (struct sec_job_descriptor_t *) ctx->jobdes.desc;

	SEC_JD_INIT(jobdescr);
	SEC_JD_SET_SD(jobdescr,
		(phys_addr_t)(caam_jr_dma_vtop(cdb)) + sdesc_offset,
		cdb->sh_hdr.hi.field.idlen);

	/* output */
	SEC_JD_SET_OUT_PTR(jobdescr, (uint64_t)sym->auth.digest.phys_addr,
			0, ses->digest_length);

	/*input */
	sg = &ctx->sg[0];
	length = sym->auth.data.length;
	sg->ptr = cpu_to_caam64(rte_pktmbuf_iova(mbuf) + sym->auth.data.offset);
	sg->len = cpu_to_caam32(mbuf->data_len - sym->auth.data.offset);

	/* Successive segs */
	mbuf = mbuf->next;
	while (mbuf) {
		sg++;
		sg->ptr = cpu_to_caam64(rte_pktmbuf_iova(mbuf));
		sg->len = cpu_to_caam32(mbuf->data_len);
		mbuf = mbuf->next;
	}

	if (is_decode(ses)) {
		/* digest verification case */
		sg++;
		/* hash result or digest, save digest first */
		rte_memcpy(ctx->digest, sym->auth.digest.data,
			   ses->digest_length);
#if CAAM_JR_DBG
		rte_hexdump(stdout, "ICV", ctx->digest, ses->digest_length);
#endif
		sg->ptr = cpu_to_caam64(caam_jr_vtop_ctx(ctx, ctx->digest));
		sg->len = cpu_to_caam32(ses->digest_length);
		length += ses->digest_length;
	} else {
		sg->len -= ses->digest_length;
	}

	/* last element*/
	sg->len |= cpu_to_caam32(SEC4_SG_LEN_FIN);

	SEC_JD_SET_IN_PTR(jobdescr,
		(uint64_t)caam_jr_vtop_ctx(ctx, &ctx->sg[0]), 0, length);
	/* enabling sg list */
	(jobdescr)->seq_in.command.word  |= 0x01000000;

	return ctx;
}

static inline struct caam_jr_op_ctx *
build_auth_only(struct rte_crypto_op *op, struct caam_jr_session *ses)
{
	struct rte_crypto_sym_op *sym = op->sym;
	struct caam_jr_op_ctx *ctx;
	struct sec4_sg_entry *sg;
	rte_iova_t start_addr;
	struct sec_cdb *cdb;
	uint64_t sdesc_offset;
	struct sec_job_descriptor_t *jobdescr;

	ctx = caam_jr_alloc_ctx(ses);
	if (!ctx)
		return NULL;

	ctx->op = op;

	cdb = ses->cdb;
	sdesc_offset = (size_t) ((char *)&cdb->sh_desc - (char *)cdb);

	start_addr = rte_pktmbuf_iova(sym->m_src);

	jobdescr = (struct sec_job_descriptor_t *) ctx->jobdes.desc;

	SEC_JD_INIT(jobdescr);
	SEC_JD_SET_SD(jobdescr,
		(phys_addr_t)(caam_jr_dma_vtop(cdb)) + sdesc_offset,
		cdb->sh_hdr.hi.field.idlen);

	/* output */
	SEC_JD_SET_OUT_PTR(jobdescr, (uint64_t)sym->auth.digest.phys_addr,
			0, ses->digest_length);

	/*input */
	if (is_decode(ses)) {
		sg = &ctx->sg[0];
		SEC_JD_SET_IN_PTR(jobdescr,
			(uint64_t)caam_jr_vtop_ctx(ctx, sg), 0,
			(sym->auth.data.length + ses->digest_length));
		/* enabling sg list */
		(jobdescr)->seq_in.command.word  |= 0x01000000;

		/* hash result or digest, save digest first */
		rte_memcpy(ctx->digest, sym->auth.digest.data,
			   ses->digest_length);
		sg->ptr = cpu_to_caam64(start_addr + sym->auth.data.offset);
		sg->len = cpu_to_caam32(sym->auth.data.length);

#if CAAM_JR_DBG
		rte_hexdump(stdout, "ICV", ctx->digest, ses->digest_length);
#endif
		/* let's check digest by hw */
		sg++;
		sg->ptr = cpu_to_caam64(caam_jr_vtop_ctx(ctx, ctx->digest));
		sg->len = cpu_to_caam32(ses->digest_length);
		/* last element*/
		sg->len |= cpu_to_caam32(SEC4_SG_LEN_FIN);
	} else {
		SEC_JD_SET_IN_PTR(jobdescr, (uint64_t)start_addr,
			sym->auth.data.offset, sym->auth.data.length);
	}
	return ctx;
}

static inline struct caam_jr_op_ctx *
build_cipher_only_sg(struct rte_crypto_op *op, struct caam_jr_session *ses)
{
	struct rte_crypto_sym_op *sym = op->sym;
	struct rte_mbuf *mbuf = sym->m_src;
	struct caam_jr_op_ctx *ctx;
	struct sec4_sg_entry *sg, *in_sg;
	int length;
	struct sec_cdb *cdb;
	uint64_t sdesc_offset;
	uint8_t *IV_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			ses->iv.offset);
	struct sec_job_descriptor_t *jobdescr;
	uint8_t reg_segs;

	if (sym->m_dst) {
		mbuf = sym->m_dst;
		reg_segs = mbuf->nb_segs + sym->m_src->nb_segs + 2;
	} else {
		mbuf = sym->m_src;
		reg_segs = mbuf->nb_segs * 2 + 2;
	}

	if (reg_segs > MAX_SG_ENTRIES) {
		CAAM_JR_DP_ERR("Cipher: Max sec segs supported is %d",
				MAX_SG_ENTRIES);
		return NULL;
	}

	ctx = caam_jr_alloc_ctx(ses);
	if (!ctx)
		return NULL;

	ctx->op = op;
	cdb = ses->cdb;
	sdesc_offset = (size_t) ((char *)&cdb->sh_desc - (char *)cdb);

	jobdescr = (struct sec_job_descriptor_t *) ctx->jobdes.desc;

	SEC_JD_INIT(jobdescr);
	SEC_JD_SET_SD(jobdescr,
		(phys_addr_t)(caam_jr_dma_vtop(cdb)) + sdesc_offset,
		cdb->sh_hdr.hi.field.idlen);

#if CAAM_JR_DBG
	CAAM_JR_INFO("mbuf offset =%d, cipher offset = %d, length =%d+%d",
			sym->m_src->data_off, sym->cipher.data.offset,
			sym->cipher.data.length, ses->iv.length);
#endif
	/* output */
	if (sym->m_dst)
		mbuf = sym->m_dst;
	else
		mbuf = sym->m_src;

	sg = &ctx->sg[0];
	length = sym->cipher.data.length;

	sg->ptr = cpu_to_caam64(rte_pktmbuf_iova(mbuf)
		+ sym->cipher.data.offset);
	sg->len = cpu_to_caam32(mbuf->data_len - sym->cipher.data.offset);

	/* Successive segs */
	mbuf = mbuf->next;
	while (mbuf) {
		sg++;
		sg->ptr = cpu_to_caam64(rte_pktmbuf_iova(mbuf));
		sg->len = cpu_to_caam32(mbuf->data_len);
		mbuf = mbuf->next;
	}
	/* last element*/
	sg->len |= cpu_to_caam32(SEC4_SG_LEN_FIN);

	SEC_JD_SET_OUT_PTR(jobdescr,
			(uint64_t)caam_jr_vtop_ctx(ctx, &ctx->sg[0]), 0,
			length);
	/*enabling sg bit */
	(jobdescr)->seq_out.command.word  |= 0x01000000;

	/*input */
	sg++;
	mbuf = sym->m_src;
	in_sg = sg;

	length = sym->cipher.data.length + ses->iv.length;

	/* IV */
	sg->ptr = cpu_to_caam64(caam_jr_dma_vtop(IV_ptr));
	sg->len = cpu_to_caam32(ses->iv.length);

	/* 1st seg */
	sg++;
	sg->ptr = cpu_to_caam64(rte_pktmbuf_iova(mbuf)
				+ sym->cipher.data.offset);
	sg->len = cpu_to_caam32(mbuf->data_len - sym->cipher.data.offset);

	/* Successive segs */
	mbuf = mbuf->next;
	while (mbuf) {
		sg++;
		sg->ptr = cpu_to_caam64(rte_pktmbuf_iova(mbuf));
		sg->len = cpu_to_caam32(mbuf->data_len);
		mbuf = mbuf->next;
	}
	/* last element*/
	sg->len |= cpu_to_caam32(SEC4_SG_LEN_FIN);


	SEC_JD_SET_IN_PTR(jobdescr, (uint64_t)caam_jr_vtop_ctx(ctx, in_sg), 0,
				length);
	/*enabling sg bit */
	(jobdescr)->seq_in.command.word  |= 0x01000000;

	return ctx;
}

static inline struct caam_jr_op_ctx *
build_cipher_only(struct rte_crypto_op *op, struct caam_jr_session *ses)
{
	struct rte_crypto_sym_op *sym = op->sym;
	struct caam_jr_op_ctx *ctx;
	struct sec4_sg_entry *sg;
	rte_iova_t src_start_addr, dst_start_addr;
	struct sec_cdb *cdb;
	uint64_t sdesc_offset;
	uint8_t *IV_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			ses->iv.offset);
	struct sec_job_descriptor_t *jobdescr;

	ctx = caam_jr_alloc_ctx(ses);
	if (!ctx)
		return NULL;

	ctx->op = op;
	cdb = ses->cdb;
	sdesc_offset = (size_t) ((char *)&cdb->sh_desc - (char *)cdb);

	src_start_addr = rte_pktmbuf_iova(sym->m_src);
	if (sym->m_dst)
		dst_start_addr = rte_pktmbuf_iova(sym->m_dst);
	else
		dst_start_addr = src_start_addr;

	jobdescr = (struct sec_job_descriptor_t *) ctx->jobdes.desc;

	SEC_JD_INIT(jobdescr);
	SEC_JD_SET_SD(jobdescr,
		(phys_addr_t)(caam_jr_dma_vtop(cdb)) + sdesc_offset,
		cdb->sh_hdr.hi.field.idlen);

#if CAAM_JR_DBG
	CAAM_JR_INFO("mbuf offset =%d, cipher offset = %d, length =%d+%d",
			sym->m_src->data_off, sym->cipher.data.offset,
			sym->cipher.data.length, ses->iv.length);
#endif
	/* output */
	SEC_JD_SET_OUT_PTR(jobdescr, (uint64_t)dst_start_addr,
			sym->cipher.data.offset,
			sym->cipher.data.length + ses->iv.length);

	/*input */
	sg = &ctx->sg[0];
	SEC_JD_SET_IN_PTR(jobdescr, (uint64_t)caam_jr_vtop_ctx(ctx, sg), 0,
				sym->cipher.data.length + ses->iv.length);
	/*enabling sg bit */
	(jobdescr)->seq_in.command.word  |= 0x01000000;

	sg->ptr = cpu_to_caam64(caam_jr_dma_vtop(IV_ptr));
	sg->len = cpu_to_caam32(ses->iv.length);

	sg = &ctx->sg[1];
	sg->ptr = cpu_to_caam64(src_start_addr + sym->cipher.data.offset);
	sg->len = cpu_to_caam32(sym->cipher.data.length);
	/* last element*/
	sg->len |= cpu_to_caam32(SEC4_SG_LEN_FIN);

	return ctx;
}

/* For decapsulation:
 *     Input:
 * +----+----------------+--------------------------------+-----+
 * | IV | Auth-only data | Authenticated & Encrypted data | ICV |
 * +----+----------------+--------------------------------+-----+
 *     Output:
 * +----+--------------------------+
 * | Decrypted & authenticated data |
 * +----+--------------------------+
 */

static inline struct caam_jr_op_ctx *
build_cipher_auth_sg(struct rte_crypto_op *op, struct caam_jr_session *ses)
{
	struct rte_crypto_sym_op *sym = op->sym;
	struct caam_jr_op_ctx *ctx;
	struct sec4_sg_entry *sg, *out_sg, *in_sg;
	struct rte_mbuf *mbuf;
	uint32_t length = 0;
	struct sec_cdb *cdb;
	uint64_t sdesc_offset;
	uint8_t req_segs;
	uint8_t *IV_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			ses->iv.offset);
	struct sec_job_descriptor_t *jobdescr;
	uint16_t auth_hdr_len = sym->cipher.data.offset -
			sym->auth.data.offset;
	uint16_t auth_tail_len = sym->auth.data.length -
			sym->cipher.data.length - auth_hdr_len;
	uint32_t auth_only_len = (auth_tail_len << 16) | auth_hdr_len;

	if (sym->m_dst) {
		mbuf = sym->m_dst;
		req_segs = mbuf->nb_segs + sym->m_src->nb_segs + 3;
	} else {
		mbuf = sym->m_src;
		req_segs = mbuf->nb_segs * 2 + 3;
	}

	if (req_segs > MAX_SG_ENTRIES) {
		CAAM_JR_DP_ERR("Cipher-Auth: Max sec segs supported is %d",
				MAX_SG_ENTRIES);
		return NULL;
	}

	ctx = caam_jr_alloc_ctx(ses);
	if (!ctx)
		return NULL;

	ctx->op = op;
	cdb = ses->cdb;
	sdesc_offset = (size_t) ((char *)&cdb->sh_desc - (char *)cdb);

	jobdescr = (struct sec_job_descriptor_t *) ctx->jobdes.desc;

	SEC_JD_INIT(jobdescr);
	SEC_JD_SET_SD(jobdescr,
		(phys_addr_t)(caam_jr_dma_vtop(cdb)) + sdesc_offset,
		cdb->sh_hdr.hi.field.idlen);

	/* output */
	if (sym->m_dst)
		mbuf = sym->m_dst;
	else
		mbuf = sym->m_src;

	out_sg = &ctx->sg[0];
	if (is_encode(ses))
		length = sym->auth.data.length + ses->digest_length;
	else
		length = sym->auth.data.length;

	sg = &ctx->sg[0];

	/* 1st seg */
	sg->ptr = cpu_to_caam64(rte_pktmbuf_iova(mbuf)
		+ sym->auth.data.offset);
	sg->len = cpu_to_caam32(mbuf->data_len - sym->auth.data.offset);

	/* Successive segs */
	mbuf = mbuf->next;
	while (mbuf) {
		sg++;
		sg->ptr = cpu_to_caam64(rte_pktmbuf_iova(mbuf));
		sg->len = cpu_to_caam32(mbuf->data_len);
		mbuf = mbuf->next;
	}

	if (is_encode(ses)) {
		/* set auth output */
		sg++;
		sg->ptr = cpu_to_caam64(sym->auth.digest.phys_addr);
		sg->len = cpu_to_caam32(ses->digest_length);
	}
	/* last element*/
	sg->len |= cpu_to_caam32(SEC4_SG_LEN_FIN);

	SEC_JD_SET_OUT_PTR(jobdescr,
			   (uint64_t)caam_jr_dma_vtop(out_sg), 0, length);
	/* set sg bit */
	(jobdescr)->seq_out.command.word  |= 0x01000000;

	/* input */
	sg++;
	mbuf = sym->m_src;
	in_sg = sg;
	if (is_encode(ses))
		length = ses->iv.length + sym->auth.data.length;
	else
		length = ses->iv.length + sym->auth.data.length
						+ ses->digest_length;

	sg->ptr = cpu_to_caam64(caam_jr_dma_vtop(IV_ptr));
	sg->len = cpu_to_caam32(ses->iv.length);

	sg++;
	/* 1st seg */
	sg->ptr = cpu_to_caam64(rte_pktmbuf_iova(mbuf)
		+ sym->auth.data.offset);
	sg->len = cpu_to_caam32(mbuf->data_len - sym->auth.data.offset);

	/* Successive segs */
	mbuf = mbuf->next;
	while (mbuf) {
		sg++;
		sg->ptr = cpu_to_caam64(rte_pktmbuf_iova(mbuf));
		sg->len = cpu_to_caam32(mbuf->data_len);
		mbuf = mbuf->next;
	}

	if (is_decode(ses)) {
		sg++;
		rte_memcpy(ctx->digest, sym->auth.digest.data,
		       ses->digest_length);
		sg->ptr = cpu_to_caam64(caam_jr_dma_vtop(ctx->digest));
		sg->len = cpu_to_caam32(ses->digest_length);
	}
	/* last element*/
	sg->len |= cpu_to_caam32(SEC4_SG_LEN_FIN);

	SEC_JD_SET_IN_PTR(jobdescr, (uint64_t)caam_jr_dma_vtop(in_sg), 0,
				length);
	/* set sg bit */
	(jobdescr)->seq_in.command.word  |= 0x01000000;
	/* Auth_only_len is set as 0 in descriptor and it is
	 * overwritten here in the jd which will update
	 * the DPOVRD reg.
	 */
	if (auth_only_len)
		/* set sg bit */
		(jobdescr)->dpovrd = 0x80000000 | auth_only_len;

	return ctx;
}

static inline struct caam_jr_op_ctx *
build_cipher_auth(struct rte_crypto_op *op, struct caam_jr_session *ses)
{
	struct rte_crypto_sym_op *sym = op->sym;
	struct caam_jr_op_ctx *ctx;
	struct sec4_sg_entry *sg;
	rte_iova_t src_start_addr, dst_start_addr;
	uint32_t length = 0;
	struct sec_cdb *cdb;
	uint64_t sdesc_offset;
	uint8_t *IV_ptr = rte_crypto_op_ctod_offset(op, uint8_t *,
			ses->iv.offset);
	struct sec_job_descriptor_t *jobdescr;
	uint16_t auth_hdr_len = sym->cipher.data.offset -
			sym->auth.data.offset;
	uint16_t auth_tail_len = sym->auth.data.length -
			sym->cipher.data.length - auth_hdr_len;
	uint32_t auth_only_len = (auth_tail_len << 16) | auth_hdr_len;

	src_start_addr = rte_pktmbuf_iova(sym->m_src);
	if (sym->m_dst)
		dst_start_addr = rte_pktmbuf_iova(sym->m_dst);
	else
		dst_start_addr = src_start_addr;

	ctx = caam_jr_alloc_ctx(ses);
	if (!ctx)
		return NULL;

	ctx->op = op;
	cdb = ses->cdb;
	sdesc_offset = (size_t) ((char *)&cdb->sh_desc - (char *)cdb);

	jobdescr = (struct sec_job_descriptor_t *) ctx->jobdes.desc;

	SEC_JD_INIT(jobdescr);
	SEC_JD_SET_SD(jobdescr,
		(phys_addr_t)(caam_jr_dma_vtop(cdb)) + sdesc_offset,
		cdb->sh_hdr.hi.field.idlen);

	/* input */
	sg = &ctx->sg[0];
	if (is_encode(ses)) {
		sg->ptr = cpu_to_caam64(caam_jr_dma_vtop(IV_ptr));
		sg->len = cpu_to_caam32(ses->iv.length);
		length += ses->iv.length;

		sg++;
		sg->ptr = cpu_to_caam64(src_start_addr + sym->auth.data.offset);
		sg->len = cpu_to_caam32(sym->auth.data.length);
		length += sym->auth.data.length;
		/* last element*/
		sg->len |= cpu_to_caam32(SEC4_SG_LEN_FIN);
	} else {
		sg->ptr = cpu_to_caam64(caam_jr_dma_vtop(IV_ptr));
		sg->len = cpu_to_caam32(ses->iv.length);
		length += ses->iv.length;

		sg++;
		sg->ptr = cpu_to_caam64(src_start_addr + sym->auth.data.offset);
		sg->len = cpu_to_caam32(sym->auth.data.length);
		length += sym->auth.data.length;

		rte_memcpy(ctx->digest, sym->auth.digest.data,
		       ses->digest_length);
		sg++;
		sg->ptr = cpu_to_caam64(caam_jr_dma_vtop(ctx->digest));
		sg->len = cpu_to_caam32(ses->digest_length);
		length += ses->digest_length;
		/* last element*/
		sg->len |= cpu_to_caam32(SEC4_SG_LEN_FIN);
	}

	SEC_JD_SET_IN_PTR(jobdescr, (uint64_t)caam_jr_dma_vtop(&ctx->sg[0]), 0,
				length);
	/* set sg bit */
	(jobdescr)->seq_in.command.word  |= 0x01000000;

	/* output */
	sg = &ctx->sg[6];

	sg->ptr = cpu_to_caam64(dst_start_addr + sym->cipher.data.offset);
	sg->len = cpu_to_caam32(sym->cipher.data.length);
	length = sym->cipher.data.length;

	if (is_encode(ses)) {
		/* set auth output */
		sg++;
		sg->ptr = cpu_to_caam64(sym->auth.digest.phys_addr);
		sg->len = cpu_to_caam32(ses->digest_length);
		length += ses->digest_length;
	}
	/* last element*/
	sg->len |= cpu_to_caam32(SEC4_SG_LEN_FIN);

	SEC_JD_SET_OUT_PTR(jobdescr,
			   (uint64_t)caam_jr_dma_vtop(&ctx->sg[6]), 0, length);
	/* set sg bit */
	(jobdescr)->seq_out.command.word  |= 0x01000000;

	/* Auth_only_len is set as 0 in descriptor and it is
	 * overwritten here in the jd which will update
	 * the DPOVRD reg.
	 */
	if (auth_only_len)
		/* set sg bit */
		(jobdescr)->dpovrd = 0x80000000 | auth_only_len;

	return ctx;
}

static inline struct caam_jr_op_ctx *
build_proto(struct rte_crypto_op *op, struct caam_jr_session *ses)
{
	struct rte_crypto_sym_op *sym = op->sym;
	struct caam_jr_op_ctx *ctx = NULL;
	phys_addr_t src_start_addr, dst_start_addr;
	struct sec_cdb *cdb;
	uint64_t sdesc_offset;
	struct sec_job_descriptor_t *jobdescr;

	ctx = caam_jr_alloc_ctx(ses);
	if (!ctx)
		return NULL;
	ctx->op = op;

	src_start_addr = rte_pktmbuf_iova(sym->m_src);
	if (sym->m_dst)
		dst_start_addr = rte_pktmbuf_iova(sym->m_dst);
	else
		dst_start_addr = src_start_addr;

	cdb = ses->cdb;
	sdesc_offset = (size_t) ((char *)&cdb->sh_desc - (char *)cdb);

	jobdescr = (struct sec_job_descriptor_t *) ctx->jobdes.desc;

	SEC_JD_INIT(jobdescr);
	SEC_JD_SET_SD(jobdescr,
		(phys_addr_t)(caam_jr_dma_vtop(cdb)) + sdesc_offset,
			cdb->sh_hdr.hi.field.idlen);

	/* output */
	SEC_JD_SET_OUT_PTR(jobdescr, (uint64_t)dst_start_addr, 0,
			sym->m_src->buf_len - sym->m_src->data_off);
	/* input */
	SEC_JD_SET_IN_PTR(jobdescr, (uint64_t)src_start_addr, 0,
			sym->m_src->pkt_len);
	sym->m_src->packet_type &= ~RTE_PTYPE_L4_MASK;

	return ctx;
}

static int
caam_jr_enqueue_op(struct rte_crypto_op *op, struct caam_jr_qp *qp)
{
	struct sec_job_ring_t *ring = qp->ring;
	struct caam_jr_session *ses;
	struct caam_jr_op_ctx *ctx = NULL;
	struct sec_job_descriptor_t *jobdescr __rte_unused;
#if CAAM_JR_DBG
	int i;
#endif

	switch (op->sess_type) {
	case RTE_CRYPTO_OP_WITH_SESSION:
		ses = (struct caam_jr_session *)
		get_sym_session_private_data(op->sym->session,
					cryptodev_driver_id);
		break;
	case RTE_CRYPTO_OP_SECURITY_SESSION:
		ses = (struct caam_jr_session *)
			get_sec_session_private_data(
					op->sym->sec_session);
		break;
	default:
		CAAM_JR_DP_ERR("sessionless crypto op not supported");
		qp->tx_errs++;
		return -1;
	}

	if (unlikely(!ses->qp || ses->qp != qp)) {
		CAAM_JR_DP_DEBUG("Old:sess->qp=%p New qp = %p\n", ses->qp, qp);
		ses->qp = qp;
		caam_jr_prep_cdb(ses);
	}

	if (rte_pktmbuf_is_contiguous(op->sym->m_src)) {
		if (is_auth_cipher(ses))
			ctx = build_cipher_auth(op, ses);
		else if (is_aead(ses))
			goto err1;
		else if (is_auth_only(ses))
			ctx = build_auth_only(op, ses);
		else if (is_cipher_only(ses))
			ctx = build_cipher_only(op, ses);
		else if (is_proto_ipsec(ses))
			ctx = build_proto(op, ses);
	} else {
		if (is_auth_cipher(ses))
			ctx = build_cipher_auth_sg(op, ses);
		else if (is_aead(ses))
			goto err1;
		else if (is_auth_only(ses))
			ctx = build_auth_only_sg(op, ses);
		else if (is_cipher_only(ses))
			ctx = build_cipher_only_sg(op, ses);
	}
err1:
	if (unlikely(!ctx)) {
		qp->tx_errs++;
		CAAM_JR_ERR("not supported sec op");
		return -1;
	}
#if CAAM_JR_DBG
	if (is_decode(ses))
		rte_hexdump(stdout, "DECODE",
			rte_pktmbuf_mtod(op->sym->m_src, void *),
			rte_pktmbuf_data_len(op->sym->m_src));
	else
		rte_hexdump(stdout, "ENCODE",
			rte_pktmbuf_mtod(op->sym->m_src, void *),
			rte_pktmbuf_data_len(op->sym->m_src));

	printf("\n JD before conversion\n");
	for (i = 0; i < 12; i++)
		printf("\n 0x%08x", ctx->jobdes.desc[i]);
#endif

	CAAM_JR_DP_DEBUG("Jr[%p] pi[%d] ci[%d].Before sending desc",
		      ring, ring->pidx, ring->cidx);

	/* todo - do we want to retry */
	if (SEC_JOB_RING_IS_FULL(ring->pidx, ring->cidx,
			 SEC_JOB_RING_SIZE, SEC_JOB_RING_SIZE)) {
		CAAM_JR_DP_DEBUG("Ring FULL Jr[%p] pi[%d] ci[%d].Size = %d",
			      ring, ring->pidx, ring->cidx, SEC_JOB_RING_SIZE);
		caam_jr_op_ending(ctx);
		qp->tx_ring_full++;
		return -EBUSY;
	}

#if CORE_BYTE_ORDER != CAAM_BYTE_ORDER
	jobdescr = (struct sec_job_descriptor_t *) ctx->jobdes.desc;

	jobdescr->deschdr.command.word =
		cpu_to_caam32(jobdescr->deschdr.command.word);
	jobdescr->sd_ptr = cpu_to_caam64(jobdescr->sd_ptr);
	jobdescr->seq_out.command.word =
		cpu_to_caam32(jobdescr->seq_out.command.word);
	jobdescr->seq_out_ptr = cpu_to_caam64(jobdescr->seq_out_ptr);
	jobdescr->out_ext_length = cpu_to_caam32(jobdescr->out_ext_length);
	jobdescr->seq_in.command.word =
		cpu_to_caam32(jobdescr->seq_in.command.word);
	jobdescr->seq_in_ptr = cpu_to_caam64(jobdescr->seq_in_ptr);
	jobdescr->in_ext_length = cpu_to_caam32(jobdescr->in_ext_length);
	jobdescr->load_dpovrd.command.word =
		cpu_to_caam32(jobdescr->load_dpovrd.command.word);
	jobdescr->dpovrd = cpu_to_caam32(jobdescr->dpovrd);
#endif

	/* Set ptr in input ring to current descriptor	*/
	sec_write_addr(&ring->input_ring[ring->pidx],
			(phys_addr_t)caam_jr_vtop_ctx(ctx, ctx->jobdes.desc));
	rte_smp_wmb();

	/* Notify HW that a new job is enqueued */
	hw_enqueue_desc_on_job_ring(ring);

	/* increment the producer index for the current job ring */
	ring->pidx = SEC_CIRCULAR_COUNTER(ring->pidx, SEC_JOB_RING_SIZE);

	return 0;
}

static uint16_t
caam_jr_enqueue_burst(void *qp, struct rte_crypto_op **ops,
		       uint16_t nb_ops)
{
	/* Function to transmit the frames to given device and queuepair */
	uint32_t loop;
	int32_t ret;
	struct caam_jr_qp *jr_qp = (struct caam_jr_qp *)qp;
	uint16_t num_tx = 0;
	/*Prepare each packet which is to be sent*/
	for (loop = 0; loop < nb_ops; loop++) {
		ret = caam_jr_enqueue_op(ops[loop], jr_qp);
		if (!ret)
			num_tx++;
	}

	jr_qp->tx_pkts += num_tx;

	return num_tx;
}

/* Release queue pair */
static int
caam_jr_queue_pair_release(struct rte_cryptodev *dev,
			   uint16_t qp_id)
{
	struct sec_job_ring_t *internals;
	struct caam_jr_qp *qp = NULL;

	PMD_INIT_FUNC_TRACE();
	CAAM_JR_DEBUG("dev =%p, queue =%d", dev, qp_id);

	internals = dev->data->dev_private;
	if (qp_id >= internals->max_nb_queue_pairs) {
		CAAM_JR_ERR("Max supported qpid %d",
			     internals->max_nb_queue_pairs);
		return -EINVAL;
	}

	qp = &internals->qps[qp_id];
	qp->ring = NULL;
	dev->data->queue_pairs[qp_id] = NULL;

	return 0;
}

/* Setup a queue pair */
static int
caam_jr_queue_pair_setup(
		struct rte_cryptodev *dev, uint16_t qp_id,
		__rte_unused const struct rte_cryptodev_qp_conf *qp_conf,
		__rte_unused int socket_id)
{
	struct sec_job_ring_t *internals;
	struct caam_jr_qp *qp = NULL;

	PMD_INIT_FUNC_TRACE();
	CAAM_JR_DEBUG("dev =%p, queue =%d, conf =%p", dev, qp_id, qp_conf);

	internals = dev->data->dev_private;
	if (qp_id >= internals->max_nb_queue_pairs) {
		CAAM_JR_ERR("Max supported qpid %d",
			     internals->max_nb_queue_pairs);
		return -EINVAL;
	}

	qp = &internals->qps[qp_id];
	qp->ring = internals;
	dev->data->queue_pairs[qp_id] = qp;

	return 0;
}

/* Returns the size of the aesni gcm session structure */
static unsigned int
caam_jr_sym_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	return sizeof(struct caam_jr_session);
}

static int
caam_jr_cipher_init(struct rte_cryptodev *dev __rte_unused,
		    struct rte_crypto_sym_xform *xform,
		    struct caam_jr_session *session)
{
	session->cipher_alg = xform->cipher.algo;
	session->iv.length = xform->cipher.iv.length;
	session->iv.offset = xform->cipher.iv.offset;
	session->cipher_key.data = rte_zmalloc(NULL, xform->cipher.key.length,
					       RTE_CACHE_LINE_SIZE);
	if (session->cipher_key.data == NULL && xform->cipher.key.length > 0) {
		CAAM_JR_ERR("No Memory for cipher key\n");
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
caam_jr_auth_init(struct rte_cryptodev *dev __rte_unused,
		  struct rte_crypto_sym_xform *xform,
		  struct caam_jr_session *session)
{
	session->auth_alg = xform->auth.algo;
	session->auth_key.data = rte_zmalloc(NULL, xform->auth.key.length,
					     RTE_CACHE_LINE_SIZE);
	if (session->auth_key.data == NULL && xform->auth.key.length > 0) {
		CAAM_JR_ERR("No Memory for auth key\n");
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
caam_jr_aead_init(struct rte_cryptodev *dev __rte_unused,
		  struct rte_crypto_sym_xform *xform,
		  struct caam_jr_session *session)
{
	session->aead_alg = xform->aead.algo;
	session->iv.length = xform->aead.iv.length;
	session->iv.offset = xform->aead.iv.offset;
	session->auth_only_len = xform->aead.aad_length;
	session->aead_key.data = rte_zmalloc(NULL, xform->aead.key.length,
					     RTE_CACHE_LINE_SIZE);
	if (session->aead_key.data == NULL && xform->aead.key.length > 0) {
		CAAM_JR_ERR("No Memory for aead key\n");
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
caam_jr_set_session_parameters(struct rte_cryptodev *dev,
			       struct rte_crypto_sym_xform *xform, void *sess)
{
	struct sec_job_ring_t *internals = dev->data->dev_private;
	struct caam_jr_session *session = sess;

	PMD_INIT_FUNC_TRACE();

	if (unlikely(sess == NULL)) {
		CAAM_JR_ERR("invalid session struct");
		return -EINVAL;
	}

	/* Default IV length = 0 */
	session->iv.length = 0;

	/* Cipher Only */
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER && xform->next == NULL) {
		session->auth_alg = RTE_CRYPTO_AUTH_NULL;
		caam_jr_cipher_init(dev, xform, session);

	/* Authentication Only */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		   xform->next == NULL) {
		session->cipher_alg = RTE_CRYPTO_CIPHER_NULL;
		caam_jr_auth_init(dev, xform, session);

	/* Cipher then Authenticate */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER &&
		   xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
			caam_jr_cipher_init(dev, xform, session);
			caam_jr_auth_init(dev, xform->next, session);
		} else {
			CAAM_JR_ERR("Not supported: Auth then Cipher");
			goto err1;
		}

	/* Authenticate then Cipher */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH &&
		   xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (xform->next->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
			caam_jr_auth_init(dev, xform, session);
			caam_jr_cipher_init(dev, xform->next, session);
		} else {
			CAAM_JR_ERR("Not supported: Auth then Cipher");
			goto err1;
		}

	/* AEAD operation for AES-GCM kind of Algorithms */
	} else if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD &&
		   xform->next == NULL) {
		caam_jr_aead_init(dev, xform, session);

	} else {
		CAAM_JR_ERR("Invalid crypto type");
		return -EINVAL;
	}
	session->ctx_pool = internals->ctx_pool;

	return 0;

err1:
	rte_free(session->cipher_key.data);
	rte_free(session->auth_key.data);
	memset(session, 0, sizeof(struct caam_jr_session));

	return -EINVAL;
}

static int
caam_jr_sym_session_configure(struct rte_cryptodev *dev,
			      struct rte_crypto_sym_xform *xform,
			      struct rte_cryptodev_sym_session *sess,
			      struct rte_mempool *mempool)
{
	void *sess_private_data;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (rte_mempool_get(mempool, &sess_private_data)) {
		CAAM_JR_ERR("Couldn't get object from session mempool");
		return -ENOMEM;
	}

	memset(sess_private_data, 0, sizeof(struct caam_jr_session));
	ret = caam_jr_set_session_parameters(dev, xform, sess_private_data);
	if (ret != 0) {
		CAAM_JR_ERR("failed to configure session parameters");
		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}

	set_sym_session_private_data(sess, dev->driver_id, sess_private_data);

	return 0;
}

/* Clear the memory of session so it doesn't leave key material behind */
static void
caam_jr_sym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_sym_session_private_data(sess, index);
	struct caam_jr_session *s = (struct caam_jr_session *)sess_priv;

	PMD_INIT_FUNC_TRACE();

	if (sess_priv) {
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);

		rte_free(s->cipher_key.data);
		rte_free(s->auth_key.data);
		memset(s, 0, sizeof(struct caam_jr_session));
		set_sym_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}

static int
caam_jr_set_ipsec_session(__rte_unused struct rte_cryptodev *dev,
			  struct rte_security_session_conf *conf,
			  void *sess)
{
	struct sec_job_ring_t *internals = dev->data->dev_private;
	struct rte_security_ipsec_xform *ipsec_xform = &conf->ipsec;
	struct rte_crypto_auth_xform *auth_xform;
	struct rte_crypto_cipher_xform *cipher_xform;
	struct caam_jr_session *session = (struct caam_jr_session *)sess;

	PMD_INIT_FUNC_TRACE();

	if (ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		cipher_xform = &conf->crypto_xform->cipher;
		auth_xform = &conf->crypto_xform->next->auth;
	} else {
		auth_xform = &conf->crypto_xform->auth;
		cipher_xform = &conf->crypto_xform->next->cipher;
	}
	session->proto_alg = conf->protocol;
	session->cipher_key.data = rte_zmalloc(NULL,
					       cipher_xform->key.length,
					       RTE_CACHE_LINE_SIZE);
	if (session->cipher_key.data == NULL &&
			cipher_xform->key.length > 0) {
		CAAM_JR_ERR("No Memory for cipher key\n");
		return -ENOMEM;
	}

	session->cipher_key.length = cipher_xform->key.length;
	session->auth_key.data = rte_zmalloc(NULL,
					auth_xform->key.length,
					RTE_CACHE_LINE_SIZE);
	if (session->auth_key.data == NULL &&
			auth_xform->key.length > 0) {
		CAAM_JR_ERR("No Memory for auth key\n");
		rte_free(session->cipher_key.data);
		return -ENOMEM;
	}
	session->auth_key.length = auth_xform->key.length;
	memcpy(session->cipher_key.data, cipher_xform->key.data,
			cipher_xform->key.length);
	memcpy(session->auth_key.data, auth_xform->key.data,
			auth_xform->key.length);

	switch (auth_xform->algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		session->auth_alg = RTE_CRYPTO_AUTH_SHA1_HMAC;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		session->auth_alg = RTE_CRYPTO_AUTH_MD5_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		session->auth_alg = RTE_CRYPTO_AUTH_SHA256_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		session->auth_alg = RTE_CRYPTO_AUTH_SHA384_HMAC;
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		session->auth_alg = RTE_CRYPTO_AUTH_SHA512_HMAC;
		break;
	case RTE_CRYPTO_AUTH_AES_CMAC:
		session->auth_alg = RTE_CRYPTO_AUTH_AES_CMAC;
		break;
	case RTE_CRYPTO_AUTH_NULL:
		session->auth_alg = RTE_CRYPTO_AUTH_NULL;
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
	case RTE_CRYPTO_AUTH_AES_XCBC_MAC:
	case RTE_CRYPTO_AUTH_SNOW3G_UIA2:
	case RTE_CRYPTO_AUTH_SHA1:
	case RTE_CRYPTO_AUTH_SHA256:
	case RTE_CRYPTO_AUTH_SHA512:
	case RTE_CRYPTO_AUTH_SHA224:
	case RTE_CRYPTO_AUTH_SHA384:
	case RTE_CRYPTO_AUTH_MD5:
	case RTE_CRYPTO_AUTH_AES_GMAC:
	case RTE_CRYPTO_AUTH_KASUMI_F9:
	case RTE_CRYPTO_AUTH_AES_CBC_MAC:
	case RTE_CRYPTO_AUTH_ZUC_EIA3:
		CAAM_JR_ERR("Crypto: Unsupported auth alg %u\n",
			auth_xform->algo);
		goto out;
	default:
		CAAM_JR_ERR("Crypto: Undefined Auth specified %u\n",
			auth_xform->algo);
		goto out;
	}

	switch (cipher_xform->algo) {
	case RTE_CRYPTO_CIPHER_AES_CBC:
		session->cipher_alg = RTE_CRYPTO_CIPHER_AES_CBC;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		session->cipher_alg = RTE_CRYPTO_CIPHER_3DES_CBC;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
		session->cipher_alg = RTE_CRYPTO_CIPHER_AES_CTR;
		break;
	case RTE_CRYPTO_CIPHER_NULL:
	case RTE_CRYPTO_CIPHER_SNOW3G_UEA2:
	case RTE_CRYPTO_CIPHER_3DES_ECB:
	case RTE_CRYPTO_CIPHER_AES_ECB:
	case RTE_CRYPTO_CIPHER_KASUMI_F8:
		CAAM_JR_ERR("Crypto: Unsupported Cipher alg %u\n",
			cipher_xform->algo);
		goto out;
	default:
		CAAM_JR_ERR("Crypto: Undefined Cipher specified %u\n",
			cipher_xform->algo);
		goto out;
	}

	if (ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		memset(&session->encap_pdb, 0, sizeof(struct ipsec_encap_pdb) +
				sizeof(session->ip4_hdr));
		session->ip4_hdr.ip_v = IPVERSION;
		session->ip4_hdr.ip_hl = 5;
		session->ip4_hdr.ip_len = rte_cpu_to_be_16(
						sizeof(session->ip4_hdr));
		session->ip4_hdr.ip_tos = ipsec_xform->tunnel.ipv4.dscp;
		session->ip4_hdr.ip_id = 0;
		session->ip4_hdr.ip_off = 0;
		session->ip4_hdr.ip_ttl = ipsec_xform->tunnel.ipv4.ttl;
		session->ip4_hdr.ip_p = (ipsec_xform->proto ==
				RTE_SECURITY_IPSEC_SA_PROTO_ESP) ? IPPROTO_ESP
				: IPPROTO_AH;
		session->ip4_hdr.ip_sum = 0;
		session->ip4_hdr.ip_src = ipsec_xform->tunnel.ipv4.src_ip;
		session->ip4_hdr.ip_dst = ipsec_xform->tunnel.ipv4.dst_ip;
		session->ip4_hdr.ip_sum = calc_chksum((uint16_t *)
						(void *)&session->ip4_hdr,
						sizeof(struct ip));

		session->encap_pdb.options =
			(IPVERSION << PDBNH_ESP_ENCAP_SHIFT) |
			PDBOPTS_ESP_OIHI_PDB_INL |
			PDBOPTS_ESP_IVSRC;
		if (ipsec_xform->options.dec_ttl)
			session->encap_pdb.options |= PDBHMO_ESP_ENCAP_DTTL;
		if (ipsec_xform->options.esn)
			session->encap_pdb.options |= PDBOPTS_ESP_ESN;
		session->encap_pdb.spi = ipsec_xform->spi;
		session->encap_pdb.ip_hdr_len = sizeof(struct ip);

		session->dir = DIR_ENC;
	} else if (ipsec_xform->direction ==
			RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		memset(&session->decap_pdb, 0, sizeof(struct ipsec_decap_pdb));
		session->decap_pdb.options = sizeof(struct ip) << 16;
		if (ipsec_xform->options.esn)
			session->decap_pdb.options |= PDBOPTS_ESP_ESN;
		session->dir = DIR_DEC;
	} else
		goto out;
	session->ctx_pool = internals->ctx_pool;

	return 0;
out:
	rte_free(session->auth_key.data);
	rte_free(session->cipher_key.data);
	memset(session, 0, sizeof(struct caam_jr_session));
	return -1;
}

static int
caam_jr_security_session_create(void *dev,
				struct rte_security_session_conf *conf,
				struct rte_security_session *sess,
				struct rte_mempool *mempool)
{
	void *sess_private_data;
	struct rte_cryptodev *cdev = (struct rte_cryptodev *)dev;
	int ret;

	if (rte_mempool_get(mempool, &sess_private_data)) {
		CAAM_JR_ERR("Couldn't get object from session mempool");
		return -ENOMEM;
	}

	switch (conf->protocol) {
	case RTE_SECURITY_PROTOCOL_IPSEC:
		ret = caam_jr_set_ipsec_session(cdev, conf,
				sess_private_data);
		break;
	case RTE_SECURITY_PROTOCOL_MACSEC:
		return -ENOTSUP;
	default:
		return -EINVAL;
	}
	if (ret != 0) {
		CAAM_JR_ERR("failed to configure session parameters");
		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}

	set_sec_session_private_data(sess, sess_private_data);

	return ret;
}

/* Clear the memory of session so it doesn't leave key material behind */
static int
caam_jr_security_session_destroy(void *dev __rte_unused,
				 struct rte_security_session *sess)
{
	PMD_INIT_FUNC_TRACE();
	void *sess_priv = get_sec_session_private_data(sess);

	struct caam_jr_session *s = (struct caam_jr_session *)sess_priv;

	if (sess_priv) {
		struct rte_mempool *sess_mp = rte_mempool_from_obj(sess_priv);

		rte_free(s->cipher_key.data);
		rte_free(s->auth_key.data);
		memset(sess, 0, sizeof(struct caam_jr_session));
		set_sec_session_private_data(sess, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
	return 0;
}


static int
caam_jr_dev_configure(struct rte_cryptodev *dev,
		       struct rte_cryptodev_config *config __rte_unused)
{
	char str[20];
	struct sec_job_ring_t *internals;

	PMD_INIT_FUNC_TRACE();

	internals = dev->data->dev_private;
	snprintf(str, sizeof(str), "ctx_pool_%d", dev->data->dev_id);
	if (!internals->ctx_pool) {
		internals->ctx_pool = rte_mempool_create((const char *)str,
						CTX_POOL_NUM_BUFS,
						sizeof(struct caam_jr_op_ctx),
						CTX_POOL_CACHE_SIZE, 0,
						NULL, NULL, NULL, NULL,
						SOCKET_ID_ANY, 0);
		if (!internals->ctx_pool) {
			CAAM_JR_ERR("%s create failed\n", str);
			return -ENOMEM;
		}
	} else
		CAAM_JR_INFO("mempool already created for dev_id : %d",
				dev->data->dev_id);

	return 0;
}

static int
caam_jr_dev_start(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	return 0;
}

static void
caam_jr_dev_stop(struct rte_cryptodev *dev __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
}

static int
caam_jr_dev_close(struct rte_cryptodev *dev)
{
	struct sec_job_ring_t *internals;

	PMD_INIT_FUNC_TRACE();

	if (dev == NULL)
		return -ENOMEM;

	internals = dev->data->dev_private;
	rte_mempool_free(internals->ctx_pool);
	internals->ctx_pool = NULL;

	return 0;
}

static void
caam_jr_dev_infos_get(struct rte_cryptodev *dev,
		       struct rte_cryptodev_info *info)
{
	struct sec_job_ring_t *internals = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();
	if (info != NULL) {
		info->max_nb_queue_pairs = internals->max_nb_queue_pairs;
		info->feature_flags = dev->feature_flags;
		info->capabilities = caam_jr_get_cryptodev_capabilities();
		info->sym.max_nb_sessions = internals->max_nb_sessions;
		info->driver_id = cryptodev_driver_id;
	}
}

static struct rte_cryptodev_ops caam_jr_ops = {
	.dev_configure	      = caam_jr_dev_configure,
	.dev_start	      = caam_jr_dev_start,
	.dev_stop	      = caam_jr_dev_stop,
	.dev_close	      = caam_jr_dev_close,
	.dev_infos_get        = caam_jr_dev_infos_get,
	.stats_get	      = caam_jr_stats_get,
	.stats_reset	      = caam_jr_stats_reset,
	.queue_pair_setup     = caam_jr_queue_pair_setup,
	.queue_pair_release   = caam_jr_queue_pair_release,
	.sym_session_get_size = caam_jr_sym_session_get_size,
	.sym_session_configure = caam_jr_sym_session_configure,
	.sym_session_clear    = caam_jr_sym_session_clear
};

static struct rte_security_ops caam_jr_security_ops = {
	.session_create = caam_jr_security_session_create,
	.session_update = NULL,
	.session_stats_get = NULL,
	.session_destroy = caam_jr_security_session_destroy,
	.set_pkt_metadata = NULL,
	.capabilities_get = caam_jr_get_security_capabilities
};

/* @brief Flush job rings of any processed descs.
 * The processed descs are silently dropped,
 * WITHOUT being notified to UA.
 */
static void
close_job_ring(struct sec_job_ring_t *job_ring)
{
	if (job_ring->irq_fd != -1) {
		/* Producer index is frozen. If consumer index is not equal
		 * with producer index, then we have descs to flush.
		 */
		while (job_ring->pidx != job_ring->cidx)
			hw_flush_job_ring(job_ring, false, NULL);

		/* free the uio job ring */
		free_job_ring(job_ring->irq_fd);
		job_ring->irq_fd = -1;
		caam_jr_dma_free(job_ring->input_ring);
		caam_jr_dma_free(job_ring->output_ring);
		g_job_rings_no--;
	}
}

/** @brief Release the software and hardware resources tied to a job ring.
 * @param [in] job_ring The job ring
 *
 * @retval  0 for success
 * @retval  -1 for error
 */
static int
shutdown_job_ring(struct sec_job_ring_t *job_ring)
{
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	ASSERT(job_ring != NULL);
	ret = hw_shutdown_job_ring(job_ring);
	SEC_ASSERT(ret == 0, ret,
		"Failed to shutdown hardware job ring %p",
		job_ring);

	if (job_ring->coalescing_en)
		hw_job_ring_disable_coalescing(job_ring);

	if (job_ring->jr_mode != SEC_NOTIFICATION_TYPE_POLL) {
		ret = caam_jr_disable_irqs(job_ring->irq_fd);
		SEC_ASSERT(ret == 0, ret,
		"Failed to disable irqs for job ring %p",
		job_ring);
	}

	return ret;
}

/*
 * @brief Release the resources used by the SEC user space driver.
 *
 * Reset and release SEC's job rings indicated by the User Application at
 * init_job_ring() and free any memory allocated internally.
 * Call once during application tear down.
 *
 * @note In case there are any descriptors in-flight (descriptors received by
 * SEC driver for processing and for which no response was yet provided to UA),
 * the descriptors are discarded without any notifications to User Application.
 *
 * @retval ::0			is returned for a successful execution
 * @retval ::-1		is returned if SEC driver release is in progress
 */
static int
caam_jr_dev_uninit(struct rte_cryptodev *dev)
{
	struct sec_job_ring_t *internals;

	PMD_INIT_FUNC_TRACE();
	if (dev == NULL)
		return -ENODEV;

	internals = dev->data->dev_private;
	rte_free(dev->security_ctx);

	/* If any descriptors in flight , poll and wait
	 * until all descriptors are received and silently discarded.
	 */
	if (internals) {
		shutdown_job_ring(internals);
		close_job_ring(internals);
		rte_mempool_free(internals->ctx_pool);
	}

	CAAM_JR_INFO("Closing crypto device %s", dev->data->name);

	/* last caam jr instance) */
	if (g_job_rings_no == 0)
		g_driver_state = SEC_DRIVER_STATE_IDLE;

	return SEC_SUCCESS;
}

/* @brief Initialize the software and hardware resources tied to a job ring.
 * @param [in] jr_mode;		Model to be used by SEC Driver to receive
 *				notifications from SEC.  Can be either
 *				of the three: #SEC_NOTIFICATION_TYPE_NAPI
 *				#SEC_NOTIFICATION_TYPE_IRQ or
 *				#SEC_NOTIFICATION_TYPE_POLL
 * @param [in] NAPI_mode	The NAPI work mode to configure a job ring at
 *				startup. Used only when #SEC_NOTIFICATION_TYPE
 *				is set to #SEC_NOTIFICATION_TYPE_NAPI.
 * @param [in] irq_coalescing_timer This value determines the maximum
 *					amount of time after processing a
 *					descriptor before raising an interrupt.
 * @param [in] irq_coalescing_count This value determines how many
 *					descriptors are completed before
 *					raising an interrupt.
 * @param [in] reg_base_addr,	The job ring base address register
 * @param [in] irq_id		The job ring interrupt identification number.
 * @retval  job_ring_handle for successful job ring configuration
 * @retval  NULL on error
 *
 */
static void *
init_job_ring(void *reg_base_addr, int irq_id)
{
	struct sec_job_ring_t *job_ring = NULL;
	int i, ret = 0;
	int jr_mode = SEC_NOTIFICATION_TYPE_POLL;
	int napi_mode = 0;
	int irq_coalescing_timer = 0;
	int irq_coalescing_count = 0;

	for (i = 0; i < MAX_SEC_JOB_RINGS; i++) {
		if (g_job_rings[i].irq_fd == -1) {
			job_ring = &g_job_rings[i];
			g_job_rings_no++;
			break;
		}
	}
	if (job_ring == NULL) {
		CAAM_JR_ERR("No free job ring\n");
		return NULL;
	}

	job_ring->register_base_addr = reg_base_addr;
	job_ring->jr_mode = jr_mode;
	job_ring->napi_mode = 0;
	job_ring->irq_fd = irq_id;

	/* Allocate mem for input and output ring */

	/* Allocate memory for input ring */
	job_ring->input_ring = caam_jr_dma_mem_alloc(L1_CACHE_BYTES,
				SEC_DMA_MEM_INPUT_RING_SIZE);
	memset(job_ring->input_ring, 0, SEC_DMA_MEM_INPUT_RING_SIZE);

	/* Allocate memory for output ring */
	job_ring->output_ring = caam_jr_dma_mem_alloc(L1_CACHE_BYTES,
				SEC_DMA_MEM_OUTPUT_RING_SIZE);
	memset(job_ring->output_ring, 0, SEC_DMA_MEM_OUTPUT_RING_SIZE);

	/* Reset job ring in SEC hw and configure job ring registers */
	ret = hw_reset_job_ring(job_ring);
	if (ret != 0) {
		CAAM_JR_ERR("Failed to reset hardware job ring");
		goto cleanup;
	}

	if (jr_mode == SEC_NOTIFICATION_TYPE_NAPI) {
	/* When SEC US driver works in NAPI mode, the UA can select
	 * if the driver starts with IRQs on or off.
	 */
		if (napi_mode == SEC_STARTUP_INTERRUPT_MODE) {
			CAAM_JR_INFO("Enabling DONE IRQ generationon job ring - %p",
				job_ring);
			ret = caam_jr_enable_irqs(job_ring->irq_fd);
			if (ret != 0) {
				CAAM_JR_ERR("Failed to enable irqs for job ring");
				goto cleanup;
			}
		}
	} else if (jr_mode == SEC_NOTIFICATION_TYPE_IRQ) {
	/* When SEC US driver works in pure interrupt mode,
	 * IRQ's are always enabled.
	 */
		CAAM_JR_INFO("Enabling DONE IRQ generation on job ring - %p",
			 job_ring);
		ret = caam_jr_enable_irqs(job_ring->irq_fd);
		if (ret != 0) {
			CAAM_JR_ERR("Failed to enable irqs for job ring");
			goto cleanup;
		}
	}
	if (irq_coalescing_timer || irq_coalescing_count) {
		hw_job_ring_set_coalescing_param(job_ring,
			 irq_coalescing_timer,
			 irq_coalescing_count);

		hw_job_ring_enable_coalescing(job_ring);
		job_ring->coalescing_en = 1;
	}

	job_ring->jr_state = SEC_JOB_RING_STATE_STARTED;
	job_ring->max_nb_queue_pairs = RTE_CAAM_MAX_NB_SEC_QPS;
	job_ring->max_nb_sessions = RTE_CAAM_JR_PMD_MAX_NB_SESSIONS;

	return job_ring;
cleanup:
	caam_jr_dma_free(job_ring->output_ring);
	caam_jr_dma_free(job_ring->input_ring);
	return NULL;
}


static int
caam_jr_dev_init(const char *name,
		 struct rte_vdev_device *vdev,
		 struct rte_cryptodev_pmd_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct rte_security_ctx *security_instance;
	struct uio_job_ring *job_ring;
	char str[RTE_CRYPTODEV_NAME_MAX_LEN];

	PMD_INIT_FUNC_TRACE();

	/* Validate driver state */
	if (g_driver_state == SEC_DRIVER_STATE_IDLE) {
		g_job_rings_max = sec_configure();
		if (!g_job_rings_max) {
			CAAM_JR_ERR("No job ring detected on UIO !!!!");
			return -1;
		}
		/* Update driver state */
		g_driver_state = SEC_DRIVER_STATE_STARTED;
	}

	if (g_job_rings_no >= g_job_rings_max) {
		CAAM_JR_ERR("No more job rings available max=%d!!!!",
				g_job_rings_max);
		return -1;
	}

	job_ring = config_job_ring();
	if (job_ring == NULL) {
		CAAM_JR_ERR("failed to create job ring");
		goto init_error;
	}

	snprintf(str, sizeof(str), "caam_jr%d", job_ring->jr_id);

	dev = rte_cryptodev_pmd_create(name, &vdev->device, init_params);
	if (dev == NULL) {
		CAAM_JR_ERR("failed to create cryptodev vdev");
		goto cleanup;
	}
	/*TODO free it during teardown*/
	dev->data->dev_private = init_job_ring(job_ring->register_base_addr,
						job_ring->uio_fd);

	if (!dev->data->dev_private) {
		CAAM_JR_ERR("Ring memory allocation failed\n");
		goto cleanup2;
	}

	dev->driver_id = cryptodev_driver_id;
	dev->dev_ops = &caam_jr_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = caam_jr_dequeue_burst;
	dev->enqueue_burst = caam_jr_enqueue_burst;
	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_SECURITY |
			RTE_CRYPTODEV_FF_IN_PLACE_SGL |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT;

	/* For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		CAAM_JR_WARN("Device already init by primary process");
		return 0;
	}

	/*TODO free it during teardown*/
	security_instance = rte_malloc("caam_jr",
				sizeof(struct rte_security_ctx), 0);
	if (security_instance == NULL) {
		CAAM_JR_ERR("memory allocation failed\n");
		//todo error handling.
		goto cleanup2;
	}

	security_instance->device = (void *)dev;
	security_instance->ops = &caam_jr_security_ops;
	security_instance->sess_cnt = 0;
	dev->security_ctx = security_instance;

	RTE_LOG(INFO, PMD, "%s cryptodev init\n", dev->data->name);

	return 0;

cleanup2:
	caam_jr_dev_uninit(dev);
	rte_cryptodev_pmd_release_device(dev);
cleanup:
	free_job_ring(job_ring->uio_fd);
init_error:
	CAAM_JR_ERR("driver %s: cryptodev_caam_jr_create failed",
			init_params->name);

	return -ENXIO;
}

/** Initialise CAAM JR crypto device */
static int
cryptodev_caam_jr_probe(struct rte_vdev_device *vdev)
{
	int ret;

	struct rte_cryptodev_pmd_init_params init_params = {
		"",
		sizeof(struct sec_job_ring_t),
		rte_socket_id(),
		RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS
	};
	const char *name;
	const char *input_args;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	input_args = rte_vdev_device_args(vdev);
	rte_cryptodev_pmd_parse_input_args(&init_params, input_args);

	ret = of_init();
	if (ret) {
		RTE_LOG(ERR, PMD,
		"of_init failed\n");
		return -EINVAL;
	}
	/* if sec device version is not configured */
	if (!rta_get_sec_era()) {
		const struct device_node *caam_node;

		for_each_compatible_node(caam_node, NULL, "fsl,sec-v4.0") {
			const uint32_t *prop = of_get_property(caam_node,
					"fsl,sec-era",
					NULL);
			if (prop) {
				rta_set_sec_era(
					INTL_SEC_ERA(rte_be_to_cpu_32(*prop)));
				break;
			}
		}
	}
#ifdef RTE_LIBRTE_PMD_CAAM_JR_BE
	if (rta_get_sec_era() > RTA_SEC_ERA_8) {
		RTE_LOG(ERR, PMD,
		"CAAM is compiled in BE mode for device with sec era > 8???\n");
		return -EINVAL;
	}
#endif

	return caam_jr_dev_init(name, vdev, &init_params);
}

/** Uninitialise CAAM JR crypto device */
static int
cryptodev_caam_jr_remove(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	caam_jr_dev_uninit(cryptodev);

	return rte_cryptodev_pmd_destroy(cryptodev);
}

static void
sec_job_rings_init(void)
{
	int i;

	for (i = 0; i < MAX_SEC_JOB_RINGS; i++)
		g_job_rings[i].irq_fd = -1;
}

static struct rte_vdev_driver cryptodev_caam_jr_drv = {
	.probe = cryptodev_caam_jr_probe,
	.remove = cryptodev_caam_jr_remove
};

static struct cryptodev_driver caam_jr_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_CAAM_JR_PMD, cryptodev_caam_jr_drv);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_CAAM_JR_PMD,
	"max_nb_queue_pairs=<int>"
	"socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(caam_jr_crypto_drv, cryptodev_caam_jr_drv.driver,
		cryptodev_driver_id);

RTE_INIT(caam_jr_init)
{
	sec_uio_job_rings_init();
	sec_job_rings_init();
}

RTE_LOG_REGISTER(caam_jr_logtype, pmd.crypto.caam, NOTICE);
