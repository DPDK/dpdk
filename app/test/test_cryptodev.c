/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Intel Corporation
 * Copyright 2020 NXP
 */

#include <time.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_pause.h>
#include <rte_bus_vdev.h>
#include <rte_ether.h>
#include <rte_errno.h>

#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_string_fns.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#ifdef RTE_CRYPTO_SCHEDULER
#include <rte_cryptodev_scheduler.h>
#include <rte_cryptodev_scheduler_operations.h>
#endif

#include <rte_lcore.h>

#include "test.h"
#include "test_cryptodev.h"

#include "test_cryptodev_blockcipher.h"
#include "test_cryptodev_aes_test_vectors.h"
#include "test_cryptodev_des_test_vectors.h"
#include "test_cryptodev_hash_test_vectors.h"
#include "test_cryptodev_kasumi_test_vectors.h"
#include "test_cryptodev_kasumi_hash_test_vectors.h"
#include "test_cryptodev_snow3g_test_vectors.h"
#include "test_cryptodev_snow3g_hash_test_vectors.h"
#include "test_cryptodev_zuc_test_vectors.h"
#include "test_cryptodev_aead_test_vectors.h"
#include "test_cryptodev_hmac_test_vectors.h"
#include "test_cryptodev_mixed_test_vectors.h"
#include "test_cryptodev_sm4_test_vectors.h"
#ifdef RTE_LIB_SECURITY
#include "test_cryptodev_security_ipsec.h"
#include "test_cryptodev_security_ipsec_test_vectors.h"
#include "test_cryptodev_security_pdcp_test_vectors.h"
#include "test_cryptodev_security_pdcp_sdap_test_vectors.h"
#include "test_cryptodev_security_pdcp_test_func.h"
#include "test_cryptodev_security_docsis_test_vectors.h"

#define SDAP_DISABLED	0
#define SDAP_ENABLED	1
#endif

#define VDEV_ARGS_SIZE 100
#define MAX_NB_SESSIONS 4

#define MAX_DRV_SERVICE_CTX_SIZE 256

#define MAX_RAW_DEQUEUE_COUNT	65535

#define IN_PLACE 0
#define OUT_OF_PLACE 1

static int gbl_driver_id;

static enum rte_security_session_action_type gbl_action_type =
	RTE_SECURITY_ACTION_TYPE_NONE;

enum cryptodev_api_test_type global_api_test_type = CRYPTODEV_API_TEST;

struct crypto_unittest_params {
	struct rte_crypto_sym_xform cipher_xform;
	struct rte_crypto_sym_xform auth_xform;
	struct rte_crypto_sym_xform aead_xform;
#ifdef RTE_LIB_SECURITY
	struct rte_security_docsis_xform docsis_xform;
#endif

	union {
		void *sess;
#ifdef RTE_LIB_SECURITY
		void *sec_session;
#endif
	};
#ifdef RTE_LIB_SECURITY
	enum rte_security_session_action_type type;
#endif
	struct rte_crypto_op *op;

	struct rte_mbuf *obuf, *ibuf;

	uint8_t *digest;
};

#define ALIGN_POW2_ROUNDUP(num, align) \
	(((num) + (align) - 1) & ~((align) - 1))

#define ADD_STATIC_TESTSUITE(index, parent_ts, child_ts, num_child_ts)	\
	for (j = 0; j < num_child_ts; index++, j++)			\
		parent_ts.unit_test_suites[index] = child_ts[j]

#define ADD_BLOCKCIPHER_TESTSUITE(index, parent_ts, blk_types, num_blk_types)	\
	for (j = 0; j < num_blk_types; index++, j++)				\
		parent_ts.unit_test_suites[index] =				\
				build_blockcipher_test_suite(blk_types[j])

#define FREE_BLOCKCIPHER_TESTSUITE(index, parent_ts, num_blk_types)		\
	for (j = index; j < index + num_blk_types; j++)				\
		free_blockcipher_test_suite(parent_ts.unit_test_suites[j])

/*
 * Forward declarations.
 */
static int
test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
		struct crypto_unittest_params *ut_params, uint8_t *cipher_key,
		uint8_t *hmac_key);

static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(void *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_param,
		const uint8_t *cipher,
		const uint8_t *digest,
		const uint8_t *iv);

static int
security_proto_supported(enum rte_security_session_action_type action,
	enum rte_security_session_protocol proto);

static int
dev_configure_and_start(uint64_t ff_disable);

static int
check_cipher_capability(const struct crypto_testsuite_params *ts_params,
			const enum rte_crypto_cipher_algorithm cipher_algo,
			const uint16_t key_size, const uint16_t iv_size);

static int
check_auth_capability(const struct crypto_testsuite_params *ts_params,
			const enum rte_crypto_auth_algorithm auth_algo,
			const uint16_t key_size, const uint16_t iv_size,
			const uint16_t tag_size);

static struct rte_mbuf *
setup_test_string(struct rte_mempool *mpool,
		const char *string, size_t len, uint8_t blocksize)
{
	struct rte_mbuf *m = rte_pktmbuf_alloc(mpool);
	size_t t_len = len - (blocksize ? (len % blocksize) : 0);

	if (m) {
		char *dst;

		memset(m->buf_addr, 0, m->buf_len);
		dst = rte_pktmbuf_append(m, t_len);
		if (!dst) {
			rte_pktmbuf_free(m);
			return NULL;
		}
		if (string != NULL)
			rte_memcpy(dst, string, t_len);
		else
			memset(dst, 0, t_len);
	}

	return m;
}

/* Get number of bytes in X bits (rounding up) */
static uint32_t
ceil_byte_length(uint32_t num_bits)
{
	if (num_bits % 8)
		return ((num_bits >> 3) + 1);
	else
		return (num_bits >> 3);
}

static void
post_process_raw_dp_op(void *user_data,	uint32_t index __rte_unused,
		uint8_t is_op_success)
{
	struct rte_crypto_op *op = user_data;
	op->status = is_op_success ? RTE_CRYPTO_OP_STATUS_SUCCESS :
			RTE_CRYPTO_OP_STATUS_ERROR;
}

static struct crypto_testsuite_params testsuite_params = { NULL };
struct crypto_testsuite_params *p_testsuite_params = &testsuite_params;
static struct crypto_unittest_params unittest_params;

int
process_sym_raw_dp_op(uint8_t dev_id, uint16_t qp_id,
		struct rte_crypto_op *op, uint8_t is_cipher, uint8_t is_auth,
		uint8_t len_in_bits, uint8_t cipher_iv_len)
{
	struct rte_crypto_sym_op *sop = op->sym;
	struct rte_crypto_op *ret_op = NULL;
	struct rte_crypto_vec data_vec[UINT8_MAX], dest_data_vec[UINT8_MAX];
	struct rte_crypto_va_iova_ptr cipher_iv, digest, aad_auth_iv;
	union rte_crypto_sym_ofs ofs;
	struct rte_crypto_sym_vec vec;
	struct rte_crypto_sgl sgl, dest_sgl;
	uint32_t max_len;
	union rte_cryptodev_session_ctx sess;
	uint64_t auth_end_iova;
	uint32_t count = 0;
	struct rte_crypto_raw_dp_ctx *ctx;
	uint32_t cipher_offset = 0, cipher_len = 0, auth_offset = 0,
			auth_len = 0;
	int32_t n;
	uint32_t n_success;
	int ctx_service_size;
	int32_t status = 0;
	int enqueue_status, dequeue_status;
	struct crypto_unittest_params *ut_params = &unittest_params;
	int is_sgl = sop->m_src->nb_segs > 1;
	int ret = TEST_SUCCESS, is_oop = 0;

	ctx_service_size = rte_cryptodev_get_raw_dp_ctx_size(dev_id);
	if (ctx_service_size < 0)
		return TEST_SKIPPED;

	ctx = malloc(ctx_service_size);
	if (ctx == NULL)
		return TEST_FAILED;

	/* Both are enums, setting crypto_sess will suit any session type */
	sess.crypto_sess = op->sym->session;

	ret = rte_cryptodev_configure_raw_dp_ctx(dev_id, qp_id, ctx, op->sess_type, sess, 0);
	if (ret == -ENOTSUP) {
		ret = TEST_SKIPPED;
		goto exit;
	} else if (ret) {
		ret = TEST_FAILED;
		goto exit;
	}

	cipher_iv.iova = 0;
	cipher_iv.va = NULL;
	aad_auth_iv.iova = 0;
	aad_auth_iv.va = NULL;
	digest.iova = 0;
	digest.va = NULL;
	sgl.vec = data_vec;
	vec.num = 1;
	vec.src_sgl = &sgl;
	vec.iv = &cipher_iv;
	vec.digest = &digest;
	vec.aad = &aad_auth_iv;
	vec.status = &status;

	ofs.raw = 0;

	if ((sop->m_dst != NULL) && (sop->m_dst != sop->m_src))
		is_oop = 1;

	if (is_cipher && is_auth) {
		cipher_offset = sop->cipher.data.offset;
		cipher_len = sop->cipher.data.length;
		auth_offset = sop->auth.data.offset;
		auth_len = sop->auth.data.length;
		max_len = RTE_MAX(cipher_offset + cipher_len,
				auth_offset + auth_len);
		if (len_in_bits) {
			max_len = max_len >> 3;
			cipher_offset = cipher_offset >> 3;
			auth_offset = auth_offset >> 3;
			cipher_len = cipher_len >> 3;
			auth_len = auth_len >> 3;
		}
		ofs.ofs.cipher.head = cipher_offset;
		ofs.ofs.cipher.tail = max_len - cipher_offset - cipher_len;
		ofs.ofs.auth.head = auth_offset;
		ofs.ofs.auth.tail = max_len - auth_offset - auth_len;
		cipher_iv.va = rte_crypto_op_ctod_offset(op, void *, IV_OFFSET);
		cipher_iv.iova = rte_crypto_op_ctophys_offset(op, IV_OFFSET);
		aad_auth_iv.va = rte_crypto_op_ctod_offset(
				op, void *, IV_OFFSET + cipher_iv_len);
		aad_auth_iv.iova = rte_crypto_op_ctophys_offset(op, IV_OFFSET +
				cipher_iv_len);
		digest.va = (void *)sop->auth.digest.data;
		digest.iova = sop->auth.digest.phys_addr;

		if (is_sgl) {
			uint32_t remaining_off = auth_offset + auth_len;
			struct rte_mbuf *sgl_buf = sop->m_src;
			if (is_oop)
				sgl_buf = sop->m_dst;

			while (remaining_off >= rte_pktmbuf_data_len(sgl_buf)
					&& sgl_buf->next != NULL) {
				remaining_off -= rte_pktmbuf_data_len(sgl_buf);
				sgl_buf = sgl_buf->next;
			}

			auth_end_iova = (uint64_t)rte_pktmbuf_iova_offset(
				sgl_buf, remaining_off);
		} else {
			auth_end_iova = rte_pktmbuf_iova(op->sym->m_src) +
							 auth_offset + auth_len;
		}
		/* Then check if digest-encrypted conditions are met */
		if ((auth_offset + auth_len < cipher_offset + cipher_len) &&
				(digest.iova == auth_end_iova) && is_sgl)
			max_len = RTE_MAX(max_len,
				auth_offset + auth_len +
				ut_params->auth_xform.auth.digest_length);

	} else if (is_cipher) {
		cipher_offset = sop->cipher.data.offset;
		cipher_len = sop->cipher.data.length;
		max_len = cipher_len + cipher_offset;
		if (len_in_bits) {
			max_len = max_len >> 3;
			cipher_offset = cipher_offset >> 3;
			cipher_len = cipher_len >> 3;
		}
		ofs.ofs.cipher.head = cipher_offset;
		ofs.ofs.cipher.tail = max_len - cipher_offset - cipher_len;
		cipher_iv.va = rte_crypto_op_ctod_offset(op, void *, IV_OFFSET);
		cipher_iv.iova = rte_crypto_op_ctophys_offset(op, IV_OFFSET);

	} else if (is_auth) {
		auth_offset = sop->auth.data.offset;
		auth_len = sop->auth.data.length;
		max_len = auth_len + auth_offset;
		if (len_in_bits) {
			max_len = max_len >> 3;
			auth_offset = auth_offset >> 3;
			auth_len = auth_len >> 3;
		}
		ofs.ofs.auth.head = auth_offset;
		ofs.ofs.auth.tail = max_len - auth_offset - auth_len;
		aad_auth_iv.va = rte_crypto_op_ctod_offset(
				op, void *, IV_OFFSET + cipher_iv_len);
		aad_auth_iv.iova = rte_crypto_op_ctophys_offset(op, IV_OFFSET +
				cipher_iv_len);
		digest.va = (void *)sop->auth.digest.data;
		digest.iova = sop->auth.digest.phys_addr;

	} else { /* aead */
		cipher_offset = sop->aead.data.offset;
		cipher_len = sop->aead.data.length;
		max_len = cipher_len + cipher_offset;
		if (len_in_bits) {
			max_len = max_len >> 3;
			cipher_offset = cipher_offset >> 3;
			cipher_len = cipher_len >> 3;
		}
		ofs.ofs.cipher.head = cipher_offset;
		ofs.ofs.cipher.tail = max_len - cipher_offset - cipher_len;
		cipher_iv.va = rte_crypto_op_ctod_offset(op, void *, IV_OFFSET);
		cipher_iv.iova = rte_crypto_op_ctophys_offset(op, IV_OFFSET);
		aad_auth_iv.va = (void *)sop->aead.aad.data;
		aad_auth_iv.iova = sop->aead.aad.phys_addr;
		digest.va = (void *)sop->aead.digest.data;
		digest.iova = sop->aead.digest.phys_addr;
	}

	n = rte_crypto_mbuf_to_vec(sop->m_src, 0, max_len,
			data_vec, RTE_DIM(data_vec));
	if (n < 0 || n > sop->m_src->nb_segs) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		goto exit;
	}

	sgl.num = n;
	/* Out of place */
	if (is_oop) {
		dest_sgl.vec = dest_data_vec;
		vec.dest_sgl = &dest_sgl;
		n = rte_crypto_mbuf_to_vec(sop->m_dst, 0, max_len,
				dest_data_vec, RTE_DIM(dest_data_vec));
		if (n < 0 || n > sop->m_dst->nb_segs) {
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			goto exit;
		}
		dest_sgl.num = n;
	} else
		vec.dest_sgl = NULL;

	if (rte_cryptodev_raw_enqueue_burst(ctx, &vec, ofs, (void **)&op,
			&enqueue_status) < 1) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		goto exit;
	}

	if (enqueue_status == 0) {
		status = rte_cryptodev_raw_enqueue_done(ctx, 1);
		if (status < 0) {
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			goto exit;
		}
	} else if (enqueue_status < 0) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		goto exit;
	}

	n = n_success = 0;
	while (count++ < MAX_RAW_DEQUEUE_COUNT && n == 0) {
		n = rte_cryptodev_raw_dequeue_burst(ctx,
			NULL, 1, post_process_raw_dp_op,
				(void **)&ret_op, 0, &n_success,
				&dequeue_status);
		if (dequeue_status < 0) {
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			goto exit;
		}
		if (n == 0)
			rte_pause();
	}

	if (n == 1 && dequeue_status == 0) {
		if (rte_cryptodev_raw_dequeue_done(ctx, 1) < 0) {
			op->status = RTE_CRYPTO_OP_STATUS_ERROR;
			goto exit;
		}
	}

	op->status = (count == MAX_RAW_DEQUEUE_COUNT + 1 || ret_op != op ||
			ret_op->status == RTE_CRYPTO_OP_STATUS_ERROR ||
			n_success < 1) ? RTE_CRYPTO_OP_STATUS_ERROR :
					RTE_CRYPTO_OP_STATUS_SUCCESS;

exit:
	free(ctx);
	return ret;
}

static void
process_cpu_aead_op(uint8_t dev_id, struct rte_crypto_op *op)
{
	int32_t n, st;
	struct rte_crypto_sym_op *sop;
	union rte_crypto_sym_ofs ofs;
	struct rte_crypto_sgl sgl;
	struct rte_crypto_sym_vec symvec;
	struct rte_crypto_va_iova_ptr iv_ptr, aad_ptr, digest_ptr;
	struct rte_crypto_vec vec[UINT8_MAX];

	sop = op->sym;

	n = rte_crypto_mbuf_to_vec(sop->m_src, sop->aead.data.offset,
		sop->aead.data.length, vec, RTE_DIM(vec));

	if (n < 0 || n != sop->m_src->nb_segs) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return;
	}

	sgl.vec = vec;
	sgl.num = n;
	symvec.src_sgl = &sgl;
	symvec.iv = &iv_ptr;
	symvec.digest = &digest_ptr;
	symvec.aad = &aad_ptr;
	symvec.status = &st;
	symvec.num = 1;

	/* for CPU crypto the IOVA address is not required */
	iv_ptr.va = rte_crypto_op_ctod_offset(op, void *, IV_OFFSET);
	digest_ptr.va = (void *)sop->aead.digest.data;
	aad_ptr.va = (void *)sop->aead.aad.data;

	ofs.raw = 0;

	n = rte_cryptodev_sym_cpu_crypto_process(dev_id, sop->session, ofs,
		&symvec);

	if (n != 1)
		op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
	else
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static void
process_cpu_crypt_auth_op(uint8_t dev_id, struct rte_crypto_op *op)
{
	int32_t n, st;
	struct rte_crypto_sym_op *sop;
	union rte_crypto_sym_ofs ofs;
	struct rte_crypto_sgl sgl;
	struct rte_crypto_sym_vec symvec;
	struct rte_crypto_va_iova_ptr iv_ptr, digest_ptr;
	struct rte_crypto_vec vec[UINT8_MAX];

	sop = op->sym;

	n = rte_crypto_mbuf_to_vec(sop->m_src, sop->auth.data.offset,
		sop->auth.data.length, vec, RTE_DIM(vec));

	if (n < 0 || n != sop->m_src->nb_segs) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return;
	}

	sgl.vec = vec;
	sgl.num = n;
	symvec.src_sgl = &sgl;
	symvec.iv = &iv_ptr;
	symvec.digest = &digest_ptr;
	symvec.status = &st;
	symvec.num = 1;

	iv_ptr.va = rte_crypto_op_ctod_offset(op, void *, IV_OFFSET);
	digest_ptr.va = (void *)sop->auth.digest.data;

	ofs.raw = 0;
	ofs.ofs.cipher.head = sop->cipher.data.offset - sop->auth.data.offset;
	ofs.ofs.cipher.tail = (sop->auth.data.offset + sop->auth.data.length) -
		(sop->cipher.data.offset + sop->cipher.data.length);

	n = rte_cryptodev_sym_cpu_crypto_process(dev_id, sop->session, ofs,
		&symvec);

	if (n != 1)
		op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
	else
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
}

static struct rte_crypto_op *
process_crypto_request(uint8_t dev_id, struct rte_crypto_op *op)
{

	RTE_VERIFY(gbl_action_type != RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO);

	if (rte_cryptodev_enqueue_burst(dev_id, 0, &op, 1) != 1) {
		RTE_LOG(ERR, USER1, "Error sending packet for encryption\n");
		return NULL;
	}

	op = NULL;

	while (rte_cryptodev_dequeue_burst(dev_id, 0, &op, 1) == 0)
		rte_pause();

	if (op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(DEBUG, USER1, "Operation status %d\n", op->status);
		return NULL;
	}

	return op;
}

static int
testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info info;
	uint32_t i = 0, nb_devs, dev_id;
	uint16_t qp_id;

	memset(ts_params, 0, sizeof(*ts_params));

	ts_params->mbuf_pool = rte_mempool_lookup("CRYPTO_MBUFPOOL");
	if (ts_params->mbuf_pool == NULL) {
		/* Not already created so create */
		ts_params->mbuf_pool = rte_pktmbuf_pool_create(
				"CRYPTO_MBUFPOOL",
				NUM_MBUFS, MBUF_CACHE_SIZE, 0, MBUF_SIZE,
				rte_socket_id());
		if (ts_params->mbuf_pool == NULL) {
			RTE_LOG(ERR, USER1, "Can't create CRYPTO_MBUFPOOL\n");
			return TEST_FAILED;
		}
	}

	ts_params->large_mbuf_pool = rte_mempool_lookup(
			"CRYPTO_LARGE_MBUFPOOL");
	if (ts_params->large_mbuf_pool == NULL) {
		/* Not already created so create */
		ts_params->large_mbuf_pool = rte_pktmbuf_pool_create(
				"CRYPTO_LARGE_MBUFPOOL",
				1, 0, 0, UINT16_MAX,
				rte_socket_id());
		if (ts_params->large_mbuf_pool == NULL) {
			RTE_LOG(ERR, USER1,
				"Can't create CRYPTO_LARGE_MBUFPOOL\n");
			return TEST_FAILED;
		}
	}

	ts_params->op_mpool = rte_crypto_op_pool_create(
			"MBUF_CRYPTO_SYM_OP_POOL",
			RTE_CRYPTO_OP_TYPE_SYMMETRIC,
			NUM_MBUFS, MBUF_CACHE_SIZE,
			DEFAULT_NUM_XFORMS *
			sizeof(struct rte_crypto_sym_xform) +
			MAXIMUM_IV_LENGTH,
			rte_socket_id());
	if (ts_params->op_mpool == NULL) {
		RTE_LOG(ERR, USER1, "Can't create CRYPTO_OP_POOL\n");
		return TEST_FAILED;
	}

	nb_devs = rte_cryptodev_count();
	if (nb_devs < 1) {
		RTE_LOG(WARNING, USER1, "No crypto devices found?\n");
		return TEST_SKIPPED;
	}

	if (rte_cryptodev_device_count_by_driver(gbl_driver_id) < 1) {
		RTE_LOG(WARNING, USER1, "No %s devices found?\n",
				rte_cryptodev_driver_name_get(gbl_driver_id));
		return TEST_SKIPPED;
	}

	/* Create list of valid crypto devs */
	for (i = 0; i < nb_devs; i++) {
		rte_cryptodev_info_get(i, &info);
		if (info.driver_id == gbl_driver_id)
			ts_params->valid_devs[ts_params->valid_dev_count++] = i;
	}

	if (ts_params->valid_dev_count < 1)
		return TEST_FAILED;

	/* Set up all the qps on the first of the valid devices found */

	dev_id = ts_params->valid_devs[0];

	rte_cryptodev_info_get(dev_id, &info);

	ts_params->conf.nb_queue_pairs = info.max_nb_queue_pairs;
	ts_params->conf.socket_id = SOCKET_ID_ANY;
	ts_params->conf.ff_disable = RTE_CRYPTODEV_FF_SECURITY;

	unsigned int session_size =
		rte_cryptodev_sym_get_private_session_size(dev_id);

#ifdef RTE_LIB_SECURITY
	unsigned int security_session_size = rte_security_session_get_size(
			rte_cryptodev_get_sec_ctx(dev_id));

	if (session_size < security_session_size)
		session_size = security_session_size;
#endif
	/*
	 * Create mempool with maximum number of sessions.
	 */
	if (info.sym.max_nb_sessions != 0 &&
			info.sym.max_nb_sessions < MAX_NB_SESSIONS) {
		RTE_LOG(ERR, USER1, "Device does not support "
				"at least %u sessions\n",
				MAX_NB_SESSIONS);
		return TEST_FAILED;
	}

	ts_params->session_mpool = rte_cryptodev_sym_session_pool_create(
			"test_sess_mp", MAX_NB_SESSIONS, session_size, 0, 0,
			SOCKET_ID_ANY);
	TEST_ASSERT_NOT_NULL(ts_params->session_mpool,
			"session mempool allocation failed");

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id,
			&ts_params->conf),
			"Failed to configure cryptodev %u with %u qps",
			dev_id, ts_params->conf.nb_queue_pairs);

	ts_params->qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT;
	ts_params->qp_conf.mp_session = ts_params->session_mpool;

	for (qp_id = 0; qp_id < info.max_nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
			dev_id, qp_id, &ts_params->qp_conf,
			rte_cryptodev_socket_id(dev_id)),
			"Failed to setup queue pair %u on cryptodev %u",
			qp_id, dev_id);
	}

	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	int res;

	if (ts_params->mbuf_pool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_MBUFPOOL count %u\n",
		rte_mempool_avail_count(ts_params->mbuf_pool));
	}

	if (ts_params->op_mpool != NULL) {
		RTE_LOG(DEBUG, USER1, "CRYPTO_OP_POOL count %u\n",
		rte_mempool_avail_count(ts_params->op_mpool));
	}

	if (ts_params->session_mpool != NULL) {
		rte_mempool_free(ts_params->session_mpool);
		ts_params->session_mpool = NULL;
	}

	res = rte_cryptodev_close(ts_params->valid_devs[0]);
	if (res)
		RTE_LOG(ERR, USER1, "Crypto device close error %d\n", res);
}

static int
check_capabilities_supported(enum rte_crypto_sym_xform_type type,
		const int *algs, uint16_t num_algs)
{
	uint8_t dev_id = testsuite_params.valid_devs[0];
	bool some_alg_supported = FALSE;
	uint16_t i;

	for (i = 0; i < num_algs && !some_alg_supported; i++) {
		struct rte_cryptodev_sym_capability_idx alg = {
			type, {algs[i]}
		};
		if (rte_cryptodev_sym_capability_get(dev_id,
				&alg) != NULL)
			some_alg_supported = TRUE;
	}
	if (!some_alg_supported)
		return TEST_SKIPPED;

	return 0;
}

int
check_cipher_capabilities_supported(const enum rte_crypto_cipher_algorithm *ciphers,
		uint16_t num_ciphers)
{
	return check_capabilities_supported(RTE_CRYPTO_SYM_XFORM_CIPHER,
			(const int *) ciphers, num_ciphers);
}

int
check_auth_capabilities_supported(const enum rte_crypto_auth_algorithm *auths,
		uint16_t num_auths)
{
	return check_capabilities_supported(RTE_CRYPTO_SYM_XFORM_AUTH,
			(const int *) auths, num_auths);
}

int
check_aead_capabilities_supported(const enum rte_crypto_aead_algorithm *aeads,
		uint16_t num_aeads)
{
	return check_capabilities_supported(RTE_CRYPTO_SYM_XFORM_AEAD,
			(const int *) aeads, num_aeads);
}

static int
null_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_cipher_algorithm ciphers[] = {
		RTE_CRYPTO_CIPHER_NULL
	};
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_NULL
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO)) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for NULL "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_cipher_capabilities_supported(ciphers, RTE_DIM(ciphers)) != 0
			&& check_auth_capabilities_supported(auths,
			RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for NULL "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
crypto_gen_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO)) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for Crypto Gen "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

#ifdef RTE_LIB_SECURITY
static int
ipsec_proto_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_cryptodev_info dev_info;
	int ret = 0;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SECURITY)) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for IPsec Proto "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	/* Reconfigure to enable security */
	ret = dev_configure_and_start(0);
	if (ret != TEST_SUCCESS)
		return ret;

	/* Set action type */
	ut_params->type = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL;

	if (security_proto_supported(
			RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL,
			RTE_SECURITY_PROTOCOL_IPSEC) < 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for IPsec Proto "
				"test not met\n");
		ret = TEST_SKIPPED;
	}

	test_ipsec_alg_list_populate();
	test_ipsec_ah_alg_list_populate();

	/*
	 * Stop the device. Device would be started again by individual test
	 * case setup routine.
	 */
	rte_cryptodev_stop(ts_params->valid_devs[0]);

	return ret;
}

static int
pdcp_proto_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_cipher_algorithm ciphers[] = {
		RTE_CRYPTO_CIPHER_NULL,
		RTE_CRYPTO_CIPHER_AES_CTR,
		RTE_CRYPTO_CIPHER_ZUC_EEA3,
		RTE_CRYPTO_CIPHER_SNOW3G_UEA2
	};
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_NULL,
		RTE_CRYPTO_AUTH_SNOW3G_UIA2,
		RTE_CRYPTO_AUTH_AES_CMAC,
		RTE_CRYPTO_AUTH_ZUC_EIA3
	};

	RTE_BUILD_BUG_ON(RTE_DIM(pdcp_test_params) != RTE_DIM(pdcp_test_auth_key));
	RTE_BUILD_BUG_ON(RTE_DIM(pdcp_test_params) != RTE_DIM(pdcp_test_bearer));
	RTE_BUILD_BUG_ON(RTE_DIM(pdcp_test_params) != RTE_DIM(pdcp_test_crypto_key));
	RTE_BUILD_BUG_ON(RTE_DIM(pdcp_test_params) != RTE_DIM(pdcp_test_data_in));
	RTE_BUILD_BUG_ON(RTE_DIM(pdcp_test_params) != RTE_DIM(pdcp_test_data_in_len));
	RTE_BUILD_BUG_ON(RTE_DIM(pdcp_test_params) != RTE_DIM(pdcp_test_data_out));
	RTE_BUILD_BUG_ON(RTE_DIM(pdcp_test_params) != RTE_DIM(pdcp_test_data_sn_size));
	RTE_BUILD_BUG_ON(RTE_DIM(pdcp_test_params) != RTE_DIM(pdcp_test_hfn));
	RTE_BUILD_BUG_ON(RTE_DIM(pdcp_test_params) != RTE_DIM(pdcp_test_hfn_threshold));
	RTE_BUILD_BUG_ON(RTE_DIM(pdcp_test_params) != RTE_DIM(pdcp_test_packet_direction));

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			!(dev_info.feature_flags &
			RTE_CRYPTODEV_FF_SECURITY)) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for PDCP Proto "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_cipher_capabilities_supported(ciphers, RTE_DIM(ciphers)) != 0
			&& check_auth_capabilities_supported(auths,
			RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for PDCP Proto "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
docsis_proto_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_cipher_algorithm ciphers[] = {
		RTE_CRYPTO_CIPHER_AES_DOCSISBPI
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			!(dev_info.feature_flags &
			RTE_CRYPTODEV_FF_SECURITY)) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for DOCSIS "
				"Proto testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_cipher_capabilities_supported(ciphers, RTE_DIM(ciphers)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for DOCSIS Proto "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}
#endif

static int
aes_ccm_auth_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_aead_algorithm aeads[] = {
		RTE_CRYPTO_AEAD_AES_CCM
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for AES CCM "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_aead_capabilities_supported(aeads, RTE_DIM(aeads)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for AES CCM "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
aes_gcm_auth_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_aead_algorithm aeads[] = {
		RTE_CRYPTO_AEAD_AES_GCM
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO)) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for AES GCM "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_aead_capabilities_supported(aeads, RTE_DIM(aeads)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for AES GCM "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
aes_gmac_auth_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_AES_GMAC
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for AES GMAC "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_auth_capabilities_supported(auths, RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for AES GMAC "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
chacha20_poly1305_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_aead_algorithm aeads[] = {
		RTE_CRYPTO_AEAD_CHACHA20_POLY1305
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for "
				"Chacha20-Poly1305 testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_aead_capabilities_supported(aeads, RTE_DIM(aeads)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for "
				"Chacha20-Poly1305 testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
snow3g_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_cipher_algorithm ciphers[] = {
		RTE_CRYPTO_CIPHER_SNOW3G_UEA2

	};
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_SNOW3G_UIA2
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO)) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for Snow3G "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_cipher_capabilities_supported(ciphers, RTE_DIM(ciphers)) != 0
			&& check_auth_capabilities_supported(auths,
			RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for Snow3G "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
zuc_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_cipher_algorithm ciphers[] = {
		RTE_CRYPTO_CIPHER_ZUC_EEA3
	};
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_ZUC_EIA3
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO)) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for ZUC "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_cipher_capabilities_supported(ciphers, RTE_DIM(ciphers)) != 0
			&& check_auth_capabilities_supported(auths,
			RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for ZUC "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
hmac_md5_auth_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_MD5_HMAC
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for HMAC MD5 "
				"Auth testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_auth_capabilities_supported(auths, RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for HMAC MD5 "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
kasumi_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_cipher_algorithm ciphers[] = {
		RTE_CRYPTO_CIPHER_KASUMI_F8
	};
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_KASUMI_F9
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for Kasumi "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_cipher_capabilities_supported(ciphers, RTE_DIM(ciphers)) != 0
			&& check_auth_capabilities_supported(auths,
			RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for Kasumi "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
negative_aes_gcm_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_aead_algorithm aeads[] = {
		RTE_CRYPTO_AEAD_AES_GCM
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for Negative "
				"AES GCM testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_aead_capabilities_supported(aeads, RTE_DIM(aeads)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for Negative "
				"AES GCM testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
negative_aes_gmac_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_AES_GMAC
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for Negative "
				"AES GMAC testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_auth_capabilities_supported(auths, RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for Negative "
				"AES GMAC testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
mixed_cipher_hash_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	uint64_t feat_flags;
	const enum rte_crypto_cipher_algorithm ciphers[] = {
		RTE_CRYPTO_CIPHER_NULL,
		RTE_CRYPTO_CIPHER_AES_CTR,
		RTE_CRYPTO_CIPHER_ZUC_EEA3,
		RTE_CRYPTO_CIPHER_SNOW3G_UEA2
	};
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_NULL,
		RTE_CRYPTO_AUTH_SNOW3G_UIA2,
		RTE_CRYPTO_AUTH_AES_CMAC,
		RTE_CRYPTO_AUTH_ZUC_EIA3
	};

	rte_cryptodev_info_get(dev_id, &dev_info);
	feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			(global_api_test_type == CRYPTODEV_RAW_API_TEST)) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for Mixed "
				"Cipher Hash testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_cipher_capabilities_supported(ciphers, RTE_DIM(ciphers)) != 0
			&& check_auth_capabilities_supported(auths,
			RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for Mixed "
				"Cipher Hash testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
esn_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_cipher_algorithm ciphers[] = {
		RTE_CRYPTO_CIPHER_AES_CBC
	};
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_SHA1_HMAC
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for ESN "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_cipher_capabilities_supported(ciphers, RTE_DIM(ciphers)) != 0
			&& check_auth_capabilities_supported(auths,
			RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for ESN "
				"testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
multi_session_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_cipher_algorithm ciphers[] = {
		RTE_CRYPTO_CIPHER_AES_CBC
	};
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_SHA512_HMAC
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO)) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for Multi "
				"Session testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_cipher_capabilities_supported(ciphers, RTE_DIM(ciphers)) != 0
			&& check_auth_capabilities_supported(auths,
			RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for Multi "
				"Session testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
negative_hmac_sha1_testsuite_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t dev_id = ts_params->valid_devs[0];
	struct rte_cryptodev_info dev_info;
	const enum rte_crypto_cipher_algorithm ciphers[] = {
		RTE_CRYPTO_CIPHER_AES_CBC
	};
	const enum rte_crypto_auth_algorithm auths[] = {
		RTE_CRYPTO_AUTH_SHA1_HMAC
	};

	rte_cryptodev_info_get(dev_id, &dev_info);

	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO) ||
			((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		RTE_LOG(INFO, USER1, "Feature flag requirements for Negative "
				"HMAC SHA1 testsuite not met\n");
		return TEST_SKIPPED;
	}

	if (check_cipher_capabilities_supported(ciphers, RTE_DIM(ciphers)) != 0
			&& check_auth_capabilities_supported(auths,
			RTE_DIM(auths)) != 0) {
		RTE_LOG(INFO, USER1, "Capability requirements for Negative "
				"HMAC SHA1 testsuite not met\n");
		return TEST_SKIPPED;
	}

	return 0;
}

static int
dev_configure_and_start(uint64_t ff_disable)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	uint16_t qp_id;

	/* Clear unit test parameters before running test */
	memset(ut_params, 0, sizeof(*ut_params));

	/* Reconfigure device to default parameters */
	ts_params->conf.socket_id = SOCKET_ID_ANY;
	ts_params->conf.ff_disable = ff_disable;
	ts_params->qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT;
	ts_params->qp_conf.mp_session = ts_params->session_mpool;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev %u",
			ts_params->valid_devs[0]);

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs ; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0], qp_id,
			&ts_params->qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed to setup queue pair %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);
	}


	rte_cryptodev_stats_reset(ts_params->valid_devs[0]);

	/* Start the device */
	TEST_ASSERT_SUCCESS(rte_cryptodev_start(ts_params->valid_devs[0]),
			"Failed to start cryptodev %u",
			ts_params->valid_devs[0]);

	return TEST_SUCCESS;
}

int
ut_setup(void)
{
	/* Configure and start the device with security feature disabled */
	return dev_configure_and_start(RTE_CRYPTODEV_FF_SECURITY);
}

static int
ut_setup_security(void)
{
	/* Configure and start the device with no features disabled */
	return dev_configure_and_start(0);
}

static int
ut_setup_security_rx_inject(void)
{
	struct rte_mempool *mbuf_pool = rte_mempool_lookup("CRYPTO_MBUFPOOL");
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_eth_conf port_conf = {
		.rxmode = {
			.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM |
				    RTE_ETH_RX_OFFLOAD_SECURITY,
		},
		.txmode = {
			.offloads = RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE,
		},
		.lpbk_mode = 1,  /* Enable loopback */
	};
	struct rte_cryptodev_info dev_info;
	struct rte_eth_rxconf rx_conf = {
		.rx_thresh = {
			.pthresh = 8,
			.hthresh = 8,
			.wthresh = 8,
		},
		.rx_free_thresh = 32,
	};
	uint16_t nb_ports;
	void *sec_ctx;
	int ret;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_SECURITY_RX_INJECT) ||
	    !(dev_info.feature_flags & RTE_CRYPTODEV_FF_SECURITY)) {
		RTE_LOG(INFO, USER1,
			"Feature requirements for IPsec Rx inject test case not met\n");
		return TEST_SKIPPED;
	}

	sec_ctx = rte_cryptodev_get_sec_ctx(ts_params->valid_devs[0]);
	if (sec_ctx == NULL)
		return TEST_SKIPPED;

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		return TEST_SKIPPED;

	ret = rte_eth_dev_configure(0 /* port_id */,
				    1 /* nb_rx_queue */,
				    0 /* nb_tx_queue */,
				    &port_conf);
	if (ret) {
		printf("Could not configure ethdev port 0 [err=%d]\n", ret);
		return TEST_SKIPPED;
	}

	/* Rx queue setup */
	ret = rte_eth_rx_queue_setup(0 /* port_id */,
				     0 /* rx_queue_id */,
				     1024 /* nb_rx_desc */,
				     SOCKET_ID_ANY,
				     &rx_conf,
				     mbuf_pool);
	if (ret) {
		printf("Could not setup eth port 0 queue 0\n");
		return TEST_SKIPPED;
	}

	ret = rte_security_rx_inject_configure(sec_ctx, 0, true);
	if (ret) {
		printf("Could not enable Rx inject offload");
		return TEST_SKIPPED;
	}

	ret = rte_eth_dev_start(0);
	if (ret) {
		printf("Could not start ethdev");
		return TEST_SKIPPED;
	}

	ret = rte_eth_promiscuous_enable(0);
	if (ret) {
		printf("Could not enable promiscuous mode");
		return TEST_SKIPPED;
	}

	/* Configure and start cryptodev with no features disabled */
	return dev_configure_and_start(0);
}

void
ut_teardown(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* free crypto session structure */
#ifdef RTE_LIB_SECURITY
	if (ut_params->type == RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL) {
		if (ut_params->sec_session) {
			rte_security_session_destroy(rte_cryptodev_get_sec_ctx
						(ts_params->valid_devs[0]),
						ut_params->sec_session);
			ut_params->sec_session = NULL;
		}
	} else
#endif
	{
		if (ut_params->sess) {
			rte_cryptodev_sym_session_free(ts_params->valid_devs[0],
					ut_params->sess);
			ut_params->sess = NULL;
		}
	}

	/* free crypto operation structure */
	rte_crypto_op_free(ut_params->op);

	/*
	 * free mbuf - both obuf and ibuf are usually the same,
	 * so check if they point at the same address is necessary,
	 * to avoid freeing the mbuf twice.
	 */
	if (ut_params->obuf) {
		rte_pktmbuf_free(ut_params->obuf);
		if (ut_params->ibuf == ut_params->obuf)
			ut_params->ibuf = 0;
		ut_params->obuf = 0;
	}
	if (ut_params->ibuf) {
		rte_pktmbuf_free(ut_params->ibuf);
		ut_params->ibuf = 0;
	}

	if (ts_params->mbuf_pool != NULL)
		RTE_LOG(DEBUG, USER1, "CRYPTO_MBUFPOOL count %u\n",
			rte_mempool_avail_count(ts_params->mbuf_pool));

	/* Stop the device */
	rte_cryptodev_stop(ts_params->valid_devs[0]);
}

static void
ut_teardown_rx_inject(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	void *sec_ctx;
	int ret;

	if  (rte_eth_dev_count_avail() != 0) {
		ret = rte_eth_dev_reset(0);
		if (ret)
			printf("Could not reset eth port 0");

	}

	ut_teardown();

	sec_ctx = rte_cryptodev_get_sec_ctx(ts_params->valid_devs[0]);
	if (sec_ctx == NULL)
		return;

	ret = rte_security_rx_inject_configure(sec_ctx, 0, false);
	if (ret) {
		printf("Could not disable Rx inject offload");
		return;
	}
}

static int
test_device_configure_invalid_dev_id(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint16_t dev_id, num_devs = 0;

	TEST_ASSERT((num_devs = rte_cryptodev_count()) >= 1,
			"Need at least %d devices for test", 1);

	/* valid dev_id values */
	dev_id = ts_params->valid_devs[0];

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(dev_id);

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id, &ts_params->conf),
			"Failed test for rte_cryptodev_configure: "
			"invalid dev_num %u", dev_id);

	/* invalid dev_id values */
	dev_id = num_devs;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(dev_id, &ts_params->conf),
			"Failed test for rte_cryptodev_configure: "
			"invalid dev_num %u", dev_id);

	dev_id = 0xff;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(dev_id, &ts_params->conf),
			"Failed test for rte_cryptodev_configure:"
			"invalid dev_num %u", dev_id);

	return TEST_SUCCESS;
}

static int
test_device_configure_invalid_queue_pair_ids(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint16_t orig_nb_qps = ts_params->conf.nb_queue_pairs;

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(ts_params->valid_devs[0]);

	/* valid - max value queue pairs */
	ts_params->conf.nb_queue_pairs = orig_nb_qps;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev: dev_id %u, qp_id %u",
			ts_params->valid_devs[0], ts_params->conf.nb_queue_pairs);

	/* valid - one queue pairs */
	ts_params->conf.nb_queue_pairs = 1;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev: dev_id %u, qp_id %u",
			ts_params->valid_devs[0],
			ts_params->conf.nb_queue_pairs);


	/* invalid - zero queue pairs */
	ts_params->conf.nb_queue_pairs = 0;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed test for rte_cryptodev_configure, dev_id %u,"
			" invalid qps: %u",
			ts_params->valid_devs[0],
			ts_params->conf.nb_queue_pairs);


	/* invalid - max value supported by field queue pairs */
	ts_params->conf.nb_queue_pairs = UINT16_MAX;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed test for rte_cryptodev_configure, dev_id %u,"
			" invalid qps: %u",
			ts_params->valid_devs[0],
			ts_params->conf.nb_queue_pairs);


	/* invalid - max value + 1 queue pairs */
	ts_params->conf.nb_queue_pairs = orig_nb_qps + 1;

	TEST_ASSERT_FAIL(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed test for rte_cryptodev_configure, dev_id %u,"
			" invalid qps: %u",
			ts_params->valid_devs[0],
			ts_params->conf.nb_queue_pairs);

	/* revert to original testsuite value */
	ts_params->conf.nb_queue_pairs = orig_nb_qps;

	return TEST_SUCCESS;
}

static int
test_queue_pair_descriptor_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_qp_conf qp_conf = {
		.nb_descriptors = MAX_NUM_OPS_INFLIGHT
	};
	uint16_t qp_id;

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(ts_params->valid_devs[0]);

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev %u",
			ts_params->valid_devs[0]);

	/*
	 * Test various ring sizes on this device. memzones can't be
	 * freed so are re-used if ring is released and re-created.
	 */
	qp_conf.nb_descriptors = MIN_NUM_OPS_INFLIGHT; /* min size*/
	qp_conf.mp_session = ts_params->session_mpool;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for "
				"rte_cryptodev_queue_pair_setup: num_inflights "
				"%u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	qp_conf.nb_descriptors = (uint32_t)(MAX_NUM_OPS_INFLIGHT / 2);

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for"
				" rte_cryptodev_queue_pair_setup: num_inflights"
				" %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT; /* valid */

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for "
				"rte_cryptodev_queue_pair_setup: num_inflights"
				" %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for"
				" rte_cryptodev_queue_pair_setup:"
				"num_inflights %u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	/* test invalid queue pair id */
	qp_conf.nb_descriptors = DEFAULT_NUM_OPS_INFLIGHT;	/*valid */

	qp_id = ts_params->conf.nb_queue_pairs;		/*invalid */

	TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0],
			qp_id, &qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed test for rte_cryptodev_queue_pair_setup:"
			"invalid qp %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);

	qp_id = 0xffff; /*invalid*/

	TEST_ASSERT_FAIL(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0],
			qp_id, &qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed test for rte_cryptodev_queue_pair_setup:"
			"invalid qp %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);

	return TEST_SUCCESS;
}

/* ***** Plaintext data for tests ***** */

const char catch_22_quote_1[] =
		"There was only one catch and that was Catch-22, which "
		"specified that a concern for one's safety in the face of "
		"dangers that were real and immediate was the process of a "
		"rational mind. Orr was crazy and could be grounded. All he "
		"had to do was ask; and as soon as he did, he would no longer "
		"be crazy and would have to fly more missions. Orr would be "
		"crazy to fly more missions and sane if he didn't, but if he "
		"was sane he had to fly them. If he flew them he was crazy "
		"and didn't have to; but if he didn't want to he was sane and "
		"had to. Yossarian was moved very deeply by the absolute "
		"simplicity of this clause of Catch-22 and let out a "
		"respectful whistle. \"That's some catch, that Catch-22\", he "
		"observed. \"It's the best there is,\" Doc Daneeka agreed.";

const char catch_22_quote[] =
		"What a lousy earth! He wondered how many people were "
		"destitute that same night even in his own prosperous country, "
		"how many homes were shanties, how many husbands were drunk "
		"and wives socked, and how many children were bullied, abused, "
		"or abandoned. How many families hungered for food they could "
		"not afford to buy? How many hearts were broken? How many "
		"suicides would take place that same night, how many people "
		"would go insane? How many cockroaches and landlords would "
		"triumph? How many winners were losers, successes failures, "
		"and rich men poor men? How many wise guys were stupid? How "
		"many happy endings were unhappy endings? How many honest men "
		"were liars, brave men cowards, loyal men traitors, how many "
		"sainted men were corrupt, how many people in positions of "
		"trust had sold their souls to bodyguards, how many had never "
		"had souls? How many straight-and-narrow paths were crooked "
		"paths? How many best families were worst families and how "
		"many good people were bad people? When you added them all up "
		"and then subtracted, you might be left with only the children, "
		"and perhaps with Albert Einstein and an old violinist or "
		"sculptor somewhere.";

#define QUOTE_480_BYTES		(480)
#define QUOTE_512_BYTES		(512)
#define QUOTE_768_BYTES		(768)
#define QUOTE_1024_BYTES	(1024)



/* ***** SHA1 Hash Tests ***** */

#define HMAC_KEY_LENGTH_SHA1	(DIGEST_BYTE_LENGTH_SHA1)

static uint8_t hmac_sha1_key[] = {
	0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
	0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
	0xDE, 0xF4, 0xDE, 0xAD };

/* ***** SHA224 Hash Tests ***** */

#define HMAC_KEY_LENGTH_SHA224	(DIGEST_BYTE_LENGTH_SHA224)


/* ***** AES-CBC Cipher Tests ***** */

#define CIPHER_KEY_LENGTH_AES_CBC	(16)
#define CIPHER_IV_LENGTH_AES_CBC	(CIPHER_KEY_LENGTH_AES_CBC)

static uint8_t aes_cbc_key[] = {
	0xE4, 0x23, 0x33, 0x8A, 0x35, 0x64, 0x61, 0xE2,
	0x49, 0x03, 0xDD, 0xC6, 0xB8, 0xCA, 0x55, 0x7A };

static uint8_t aes_cbc_iv[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };


/* ***** AES-CBC / HMAC-SHA1 Hash Tests ***** */

static const uint8_t catch_22_quote_2_512_bytes_AES_CBC_ciphertext[] = {
	0x8B, 0x4D, 0xDA, 0x1B, 0xCF, 0x04, 0xA0, 0x31,
	0xB4, 0xBF, 0xBD, 0x68, 0x43, 0x20, 0x7E, 0x76,
	0xB1, 0x96, 0x8B, 0xA2, 0x7C, 0xA2, 0x83, 0x9E,
	0x39, 0x5A, 0x2F, 0x7E, 0x92, 0xB4, 0x48, 0x1A,
	0x3F, 0x6B, 0x5D, 0xDF, 0x52, 0x85, 0x5F, 0x8E,
	0x42, 0x3C, 0xFB, 0xE9, 0x1A, 0x24, 0xD6, 0x08,
	0xDD, 0xFD, 0x16, 0xFB, 0xE9, 0x55, 0xEF, 0xF0,
	0xA0, 0x8D, 0x13, 0xAB, 0x81, 0xC6, 0x90, 0x01,
	0xB5, 0x18, 0x84, 0xB3, 0xF6, 0xE6, 0x11, 0x57,
	0xD6, 0x71, 0xC6, 0x3C, 0x3F, 0x2F, 0x33, 0xEE,
	0x24, 0x42, 0x6E, 0xAC, 0x0B, 0xCA, 0xEC, 0xF9,
	0x84, 0xF8, 0x22, 0xAA, 0x60, 0xF0, 0x32, 0xA9,
	0x75, 0x75, 0x3B, 0xCB, 0x70, 0x21, 0x0A, 0x8D,
	0x0F, 0xE0, 0xC4, 0x78, 0x2B, 0xF8, 0x97, 0xE3,
	0xE4, 0x26, 0x4B, 0x29, 0xDA, 0x88, 0xCD, 0x46,
	0xEC, 0xAA, 0xF9, 0x7F, 0xF1, 0x15, 0xEA, 0xC3,
	0x87, 0xE6, 0x31, 0xF2, 0xCF, 0xDE, 0x4D, 0x80,
	0x70, 0x91, 0x7E, 0x0C, 0xF7, 0x26, 0x3A, 0x92,
	0x4F, 0x18, 0x83, 0xC0, 0x8F, 0x59, 0x01, 0xA5,
	0x88, 0xD1, 0xDB, 0x26, 0x71, 0x27, 0x16, 0xF5,
	0xEE, 0x10, 0x82, 0xAC, 0x68, 0x26, 0x9B, 0xE2,
	0x6D, 0xD8, 0x9A, 0x80, 0xDF, 0x04, 0x31, 0xD5,
	0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
	0x58, 0x34, 0x85, 0x61, 0x1C, 0x42, 0x10, 0x76,
	0x73, 0x02, 0x42, 0xC9, 0x23, 0x18, 0x8E, 0xB4,
	0x6F, 0xB4, 0xA3, 0x54, 0x6E, 0x88, 0x3B, 0x62,
	0x7C, 0x02, 0x8D, 0x4C, 0x9F, 0xC8, 0x45, 0xF4,
	0xC9, 0xDE, 0x4F, 0xEB, 0x22, 0x83, 0x1B, 0xE4,
	0x49, 0x37, 0xE4, 0xAD, 0xE7, 0xCD, 0x21, 0x54,
	0xBC, 0x1C, 0xC2, 0x04, 0x97, 0xB4, 0x10, 0x61,
	0xF0, 0xE4, 0xEF, 0x27, 0x63, 0x3A, 0xDA, 0x91,
	0x41, 0x25, 0x62, 0x1C, 0x5C, 0xB6, 0x38, 0x4A,
	0x88, 0x71, 0x59, 0x5A, 0x8D, 0xA0, 0x09, 0xAF,
	0x72, 0x94, 0xD7, 0x79, 0x5C, 0x60, 0x7C, 0x8F,
	0x4C, 0xF5, 0xD9, 0xA1, 0x39, 0x6D, 0x81, 0x28,
	0xEF, 0x13, 0x28, 0xDF, 0xF5, 0x3E, 0xF7, 0x8E,
	0x09, 0x9C, 0x78, 0x18, 0x79, 0xB8, 0x68, 0xD7,
	0xA8, 0x29, 0x62, 0xAD, 0xDE, 0xE1, 0x61, 0x76,
	0x1B, 0x05, 0x16, 0xCD, 0xBF, 0x02, 0x8E, 0xA6,
	0x43, 0x6E, 0x92, 0x55, 0x4F, 0x60, 0x9C, 0x03,
	0xB8, 0x4F, 0xA3, 0x02, 0xAC, 0xA8, 0xA7, 0x0C,
	0x1E, 0xB5, 0x6B, 0xF8, 0xC8, 0x4D, 0xDE, 0xD2,
	0xB0, 0x29, 0x6E, 0x40, 0xE6, 0xD6, 0xC9, 0xE6,
	0xB9, 0x0F, 0xB6, 0x63, 0xF5, 0xAA, 0x2B, 0x96,
	0xA7, 0x16, 0xAC, 0x4E, 0x0A, 0x33, 0x1C, 0xA6,
	0xE6, 0xBD, 0x8A, 0xCF, 0x40, 0xA9, 0xB2, 0xFA,
	0x63, 0x27, 0xFD, 0x9B, 0xD9, 0xFC, 0xD5, 0x87,
	0x8D, 0x4C, 0xB6, 0xA4, 0xCB, 0xE7, 0x74, 0x55,
	0xF4, 0xFB, 0x41, 0x25, 0xB5, 0x4B, 0x0A, 0x1B,
	0xB1, 0xD6, 0xB7, 0xD9, 0x47, 0x2A, 0xC3, 0x98,
	0x6A, 0xC4, 0x03, 0x73, 0x1F, 0x93, 0x6E, 0x53,
	0x19, 0x25, 0x64, 0x15, 0x83, 0xF9, 0x73, 0x2A,
	0x74, 0xB4, 0x93, 0x69, 0xC4, 0x72, 0xFC, 0x26,
	0xA2, 0x9F, 0x43, 0x45, 0xDD, 0xB9, 0xEF, 0x36,
	0xC8, 0x3A, 0xCD, 0x99, 0x9B, 0x54, 0x1A, 0x36,
	0xC1, 0x59, 0xF8, 0x98, 0xA8, 0xCC, 0x28, 0x0D,
	0x73, 0x4C, 0xEE, 0x98, 0xCB, 0x7C, 0x58, 0x7E,
	0x20, 0x75, 0x1E, 0xB7, 0xC9, 0xF8, 0xF2, 0x0E,
	0x63, 0x9E, 0x05, 0x78, 0x1A, 0xB6, 0xA8, 0x7A,
	0xF9, 0x98, 0x6A, 0xA6, 0x46, 0x84, 0x2E, 0xF6,
	0x4B, 0xDC, 0x9B, 0x8F, 0x9B, 0x8F, 0xEE, 0xB4,
	0xAA, 0x3F, 0xEE, 0xC0, 0x37, 0x27, 0x76, 0xC7,
	0x95, 0xBB, 0x26, 0x74, 0x69, 0x12, 0x7F, 0xF1,
	0xBB, 0xFF, 0xAE, 0xB5, 0x99, 0x6E, 0xCB, 0x0C
};

static const uint8_t catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA1_digest[] = {
	0x9a, 0x4f, 0x88, 0x1b, 0xb6, 0x8f, 0xd8, 0x60,
	0x42, 0x1a, 0x7d, 0x3d, 0xf5, 0x82, 0x80, 0xf1,
	0x18, 0x8c, 0x1d, 0x32
};


/* Multisession Vector context Test */
/*Begin Session 0 */
static uint8_t ms_aes_cbc_key0[] = {
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static uint8_t ms_aes_cbc_iv0[] = {
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const uint8_t ms_aes_cbc_cipher0[] = {
		0x3C, 0xE4, 0xEE, 0x42, 0xB6, 0x9B, 0xC3, 0x38,
		0x5F, 0xAD, 0x54, 0xDC, 0xA8, 0x32, 0x81, 0xDC,
		0x7A, 0x6F, 0x85, 0x58, 0x07, 0x35, 0xED, 0xEB,
		0xAD, 0x79, 0x79, 0x96, 0xD3, 0x0E, 0xA6, 0xD9,
		0xAA, 0x86, 0xA4, 0x8F, 0xB5, 0xD6, 0x6E, 0x6D,
		0x0C, 0x91, 0x2F, 0xC4, 0x67, 0x98, 0x0E, 0xC4,
		0x8D, 0x83, 0x68, 0x69, 0xC4, 0xD3, 0x94, 0x34,
		0xC4, 0x5D, 0x60, 0x55, 0x22, 0x87, 0x8F, 0x6F,
		0x17, 0x8E, 0x75, 0xE4, 0x02, 0xF5, 0x1B, 0x99,
		0xC8, 0x39, 0xA9, 0xAB, 0x23, 0x91, 0x12, 0xED,
		0x08, 0xE7, 0xD9, 0x25, 0x89, 0x24, 0x4F, 0x8D,
		0x68, 0xF3, 0x10, 0x39, 0x0A, 0xEE, 0x45, 0x24,
		0xDF, 0x7A, 0x9D, 0x00, 0x25, 0xE5, 0x35, 0x71,
		0x4E, 0x40, 0x59, 0x6F, 0x0A, 0x13, 0xB3, 0x72,
		0x1D, 0x98, 0x63, 0x94, 0x89, 0xA5, 0x39, 0x8E,
		0xD3, 0x9C, 0x8A, 0x7F, 0x71, 0x2F, 0xC7, 0xCD,
		0x81, 0x05, 0xDC, 0xC0, 0x8D, 0xCE, 0x6D, 0x18,
		0x30, 0xC4, 0x72, 0x51, 0xF0, 0x27, 0xC8, 0xF6,
		0x60, 0x5B, 0x7C, 0xB2, 0xE3, 0x49, 0x0C, 0x29,
		0xC6, 0x9F, 0x39, 0x57, 0x80, 0x55, 0x24, 0x2C,
		0x9B, 0x0F, 0x5A, 0xB3, 0x89, 0x55, 0x31, 0x96,
		0x0D, 0xCD, 0xF6, 0x51, 0x03, 0x2D, 0x89, 0x26,
		0x74, 0x44, 0xD6, 0xE8, 0xDC, 0xEA, 0x44, 0x55,
		0x64, 0x71, 0x9C, 0x9F, 0x5D, 0xBA, 0x39, 0x46,
		0xA8, 0x17, 0xA1, 0x9C, 0x52, 0x9D, 0xBC, 0x6B,
		0x4A, 0x98, 0xE6, 0xEA, 0x33, 0xEC, 0x58, 0xB4,
		0x43, 0xF0, 0x32, 0x45, 0xA4, 0xC1, 0x55, 0xB7,
		0x5D, 0xB5, 0x59, 0xB2, 0xE3, 0x96, 0xFF, 0xA5,
		0xAF, 0xE1, 0x86, 0x1B, 0x42, 0xE6, 0x3B, 0xA0,
		0x90, 0x4A, 0xE8, 0x8C, 0x21, 0x7F, 0x36, 0x1E,
		0x5B, 0x65, 0x25, 0xD1, 0xC1, 0x5A, 0xCA, 0x3D,
		0x10, 0xED, 0x2D, 0x79, 0xD0, 0x0F, 0x58, 0x44,
		0x69, 0x81, 0xF5, 0xD4, 0xC9, 0x0F, 0x90, 0x76,
		0x1F, 0x54, 0xD2, 0xD5, 0x97, 0xCE, 0x2C, 0xE3,
		0xEF, 0xF4, 0xB7, 0xC6, 0x3A, 0x87, 0x7F, 0x83,
		0x2A, 0xAF, 0xCD, 0x90, 0x12, 0xA7, 0x7D, 0x85,
		0x1D, 0x62, 0xD3, 0x85, 0x25, 0x05, 0xDB, 0x45,
		0x92, 0xA3, 0xF6, 0xA2, 0xA8, 0x41, 0xE4, 0x25,
		0x86, 0x87, 0x67, 0x24, 0xEC, 0x89, 0x23, 0x2A,
		0x9B, 0x20, 0x4D, 0x93, 0xEE, 0xE2, 0x2E, 0xC1,
		0x0B, 0x15, 0x33, 0xCF, 0x00, 0xD1, 0x1A, 0xDA,
		0x93, 0xFD, 0x28, 0x21, 0x5B, 0xCF, 0xD1, 0xF3,
		0x5A, 0x81, 0xBA, 0x82, 0x5E, 0x2F, 0x61, 0xB4,
		0x05, 0x71, 0xB5, 0xF4, 0x39, 0x3C, 0x1F, 0x60,
		0x00, 0x7A, 0xC4, 0xF8, 0x35, 0x20, 0x6C, 0x3A,
		0xCC, 0x03, 0x8F, 0x7B, 0xA2, 0xB6, 0x65, 0x8A,
		0xB6, 0x5F, 0xFD, 0x25, 0xD3, 0x5F, 0x92, 0xF9,
		0xAE, 0x17, 0x9B, 0x5E, 0x6E, 0x9A, 0xE4, 0x55,
		0x10, 0x25, 0x07, 0xA4, 0xAF, 0x21, 0x69, 0x13,
		0xD8, 0xFA, 0x31, 0xED, 0xF7, 0xA7, 0xA7, 0x3B,
		0xB8, 0x96, 0x8E, 0x10, 0x86, 0x74, 0xD8, 0xB1,
		0x34, 0x9E, 0x9B, 0x6A, 0x26, 0xA8, 0xD4, 0xD0,
		0xB5, 0xF6, 0xDE, 0xE7, 0xCA, 0x06, 0xDC, 0xA3,
		0x6F, 0xEE, 0x6B, 0x1E, 0xB5, 0x30, 0x99, 0x23,
		0xF9, 0x76, 0xF0, 0xA0, 0xCF, 0x3B, 0x94, 0x7B,
		0x19, 0x8D, 0xA5, 0x0C, 0x18, 0xA6, 0x1D, 0x07,
		0x89, 0xBE, 0x5B, 0x61, 0xE5, 0xF1, 0x42, 0xDB,
		0xD4, 0x2E, 0x02, 0x1F, 0xCE, 0xEF, 0x92, 0xB1,
		0x1B, 0x56, 0x50, 0xF2, 0x16, 0xE5, 0xE7, 0x4F,
		0xFD, 0xBB, 0x3E, 0xD2, 0xFC, 0x3C, 0xC6, 0x0F,
		0xF9, 0x12, 0x4E, 0xCB, 0x1E, 0x0C, 0x15, 0x84,
		0x2A, 0x14, 0x8A, 0x02, 0xE4, 0x7E, 0x95, 0x5B,
		0x86, 0xDB, 0x9B, 0x62, 0x5B, 0x19, 0xD2, 0x17,
		0xFA, 0x13, 0xBB, 0x6B, 0x3F, 0x45, 0x9F, 0xBF
};


static  uint8_t ms_hmac_key0[] = {
		0xFF, 0x1A, 0x7D, 0x3D, 0xF5, 0x82, 0x80, 0xF1,
		0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
		0x58, 0x34, 0x85, 0x65, 0x1C, 0x42, 0x50, 0x76,
		0x9A, 0xAF, 0x88, 0x1B, 0xB6, 0x8F, 0xF8, 0x60,
		0xA2, 0x5A, 0x7F, 0x3F, 0xF4, 0x72, 0x70, 0xF1,
		0xF5, 0x35, 0x4C, 0x3B, 0xDD, 0x90, 0x65, 0xB0,
		0x47, 0x3A, 0x75, 0x61, 0x5C, 0xA2, 0x10, 0x76,
		0x9A, 0xAF, 0x77, 0x5B, 0xB6, 0x7F, 0xF7, 0x60
};

static const uint8_t ms_hmac_digest0[] = {
		0x43, 0x52, 0xED, 0x34, 0xAB, 0x36, 0xB2, 0x51,
		0xFB, 0xA3, 0xA6, 0x7C, 0x38, 0xFC, 0x42, 0x8F,
		0x57, 0x64, 0xAB, 0x81, 0xA7, 0x89, 0xB7, 0x6C,
		0xA0, 0xDC, 0xB9, 0x4D, 0xC4, 0x30, 0xF9, 0xD4,
		0x10, 0x82, 0x55, 0xD0, 0xAB, 0x32, 0xFB, 0x56,
		0x0D, 0xE4, 0x68, 0x3D, 0x76, 0xD0, 0x7B, 0xE4,
		0xA6, 0x2C, 0x34, 0x9E, 0x8C, 0x41, 0xF8, 0x23,
		0x28, 0x1B, 0x3A, 0x90, 0x26, 0x34, 0x47, 0x90
		};

/* End Session 0 */
/* Begin session 1 */

static  uint8_t ms_aes_cbc_key1[] = {
		0xf1, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static  uint8_t ms_aes_cbc_iv1[] = {
	0xf1, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const uint8_t ms_aes_cbc_cipher1[] = {
		0x5A, 0x7A, 0x67, 0x5D, 0xB8, 0xE1, 0xDC, 0x71,
		0x39, 0xA8, 0x74, 0x93, 0x9C, 0x4C, 0xFE, 0x23,
		0x61, 0xCD, 0xA4, 0xB3, 0xD9, 0xCE, 0x99, 0x09,
		0x2A, 0x23, 0xF3, 0x29, 0xBF, 0x4C, 0xB4, 0x6A,
		0x1B, 0x6B, 0x73, 0x4D, 0x48, 0x0C, 0xCF, 0x6C,
		0x5E, 0x34, 0x9E, 0x7F, 0xBC, 0x8F, 0xCC, 0x8F,
		0x75, 0x1D, 0x3D, 0x77, 0x10, 0x76, 0xC8, 0xB9,
		0x99, 0x6F, 0xD6, 0x56, 0x75, 0xA9, 0xB2, 0x66,
		0xC2, 0x24, 0x2B, 0x9C, 0xFE, 0x40, 0x8E, 0x43,
		0x20, 0x97, 0x1B, 0xFA, 0xD0, 0xCF, 0x04, 0xAB,
		0xBB, 0xF6, 0x5D, 0xF5, 0xA0, 0x19, 0x7C, 0x23,
		0x5D, 0x80, 0x8C, 0x49, 0xF6, 0x76, 0x88, 0x29,
		0x27, 0x4C, 0x59, 0x2B, 0x43, 0xA6, 0xB2, 0x26,
		0x27, 0x78, 0xBE, 0x1B, 0xE1, 0x4F, 0x5A, 0x1F,
		0xFC, 0x68, 0x08, 0xE7, 0xC4, 0xD1, 0x34, 0x68,
		0xB7, 0x13, 0x14, 0x41, 0x62, 0x6B, 0x1F, 0x77,
		0x0C, 0x68, 0x1D, 0x0D, 0xED, 0x89, 0xAA, 0xD8,
		0x97, 0x02, 0xBA, 0x5E, 0xD4, 0x84, 0x25, 0x97,
		0x03, 0xA5, 0xA6, 0x13, 0x66, 0x02, 0xF4, 0xC3,
		0xF3, 0xD3, 0xCC, 0x95, 0xC3, 0x87, 0x46, 0x90,
		0x1F, 0x6E, 0x14, 0xA8, 0x00, 0xF2, 0x6F, 0xD5,
		0xA1, 0xAD, 0xD5, 0x40, 0xA2, 0x0F, 0x32, 0x7E,
		0x99, 0xA3, 0xF5, 0x53, 0xC3, 0x26, 0xA1, 0x45,
		0x01, 0x88, 0x57, 0x84, 0x3E, 0x7B, 0x4E, 0x0B,
		0x3C, 0xB5, 0x3E, 0x9E, 0xE9, 0x78, 0x77, 0xC5,
		0xC0, 0x89, 0xA8, 0xF8, 0xF1, 0xA5, 0x2D, 0x5D,
		0xF9, 0xC6, 0xFB, 0xCB, 0x05, 0x23, 0xBD, 0x6E,
		0x5E, 0x14, 0xC6, 0x57, 0x73, 0xCF, 0x98, 0xBD,
		0x10, 0x8B, 0x18, 0xA6, 0x01, 0x5B, 0x13, 0xAE,
		0x8E, 0xDE, 0x1F, 0xB5, 0xB7, 0x40, 0x6C, 0xC1,
		0x1E, 0xA1, 0x19, 0x20, 0x9E, 0x95, 0xE0, 0x2F,
		0x1C, 0xF5, 0xD9, 0xD0, 0x2B, 0x1E, 0x82, 0x25,
		0x62, 0xB4, 0xEB, 0xA1, 0x1F, 0xCE, 0x44, 0xA1,
		0xCB, 0x92, 0x01, 0x6B, 0xE4, 0x26, 0x23, 0xE3,
		0xC5, 0x67, 0x35, 0x55, 0xDA, 0xE5, 0x27, 0xEE,
		0x8D, 0x12, 0x84, 0xB7, 0xBA, 0xA7, 0x1C, 0xD6,
		0x32, 0x3F, 0x67, 0xED, 0xFB, 0x5B, 0x8B, 0x52,
		0x46, 0x8C, 0xF9, 0x69, 0xCD, 0xAE, 0x79, 0xAA,
		0x37, 0x78, 0x49, 0xEB, 0xC6, 0x8E, 0x76, 0x63,
		0x84, 0xFF, 0x9D, 0x22, 0x99, 0x51, 0xB7, 0x5E,
		0x83, 0x4C, 0x8B, 0xDF, 0x5A, 0x07, 0xCC, 0xBA,
		0x42, 0xA5, 0x98, 0xB6, 0x47, 0x0E, 0x66, 0xEB,
		0x23, 0x0E, 0xBA, 0x44, 0xA8, 0xAA, 0x20, 0x71,
		0x79, 0x9C, 0x77, 0x5F, 0xF5, 0xFE, 0xEC, 0xEF,
		0xC6, 0x64, 0x3D, 0x84, 0xD0, 0x2B, 0xA7, 0x0A,
		0xC3, 0x72, 0x5B, 0x9C, 0xFA, 0xA8, 0x87, 0x95,
		0x94, 0x11, 0x38, 0xA7, 0x1E, 0x58, 0xE3, 0x73,
		0xC6, 0xC9, 0xD1, 0x7B, 0x92, 0xDB, 0x0F, 0x49,
		0x74, 0xC2, 0xA2, 0x0E, 0x35, 0x57, 0xAC, 0xDB,
		0x9A, 0x1C, 0xCF, 0x5A, 0x32, 0x3E, 0x26, 0x9B,
		0xEC, 0xB3, 0xEF, 0x9C, 0xFE, 0xBE, 0x52, 0xAC,
		0xB1, 0x29, 0xDD, 0xFD, 0x07, 0xE2, 0xEE, 0xED,
		0xE4, 0x46, 0x37, 0xFE, 0xD1, 0xDC, 0xCD, 0x02,
		0xF9, 0x31, 0xB0, 0xFB, 0x36, 0xB7, 0x34, 0xA4,
		0x76, 0xE8, 0x57, 0xBF, 0x99, 0x92, 0xC7, 0xAF,
		0x98, 0x10, 0xE2, 0x70, 0xCA, 0xC9, 0x2B, 0x82,
		0x06, 0x96, 0x88, 0x0D, 0xB3, 0xAC, 0x9E, 0x6D,
		0x43, 0xBC, 0x5B, 0x31, 0xCF, 0x65, 0x8D, 0xA6,
		0xC7, 0xFE, 0x73, 0xE1, 0x54, 0xF7, 0x10, 0xF9,
		0x86, 0xF7, 0xDF, 0xA1, 0xA1, 0xD8, 0xAE, 0x35,
		0xB3, 0x90, 0xDC, 0x6F, 0x43, 0x7A, 0x8B, 0xE0,
		0xFE, 0x8F, 0x33, 0x4D, 0x29, 0x6C, 0x45, 0x53,
		0x73, 0xDD, 0x21, 0x0B, 0x85, 0x30, 0xB5, 0xA5,
		0xF3, 0x5D, 0xEC, 0x79, 0x61, 0x9D, 0x9E, 0xB3

};

static uint8_t ms_hmac_key1[] = {
		0xFE, 0x1A, 0x7D, 0x3D, 0xF5, 0x82, 0x80, 0xF1,
		0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
		0x58, 0x34, 0x85, 0x65, 0x1C, 0x42, 0x50, 0x76,
		0x9A, 0xAF, 0x88, 0x1B, 0xB6, 0x8F, 0xF8, 0x60,
		0xA2, 0x5A, 0x7F, 0x3F, 0xF4, 0x72, 0x70, 0xF1,
		0xF5, 0x35, 0x4C, 0x3B, 0xDD, 0x90, 0x65, 0xB0,
		0x47, 0x3A, 0x75, 0x61, 0x5C, 0xA2, 0x10, 0x76,
		0x9A, 0xAF, 0x77, 0x5B, 0xB6, 0x7F, 0xF7, 0x60
};

static const uint8_t ms_hmac_digest1[] = {
		0xCE, 0x6E, 0x5F, 0x77, 0x96, 0x9A, 0xB1, 0x69,
		0x2D, 0x5E, 0xF3, 0x2F, 0x32, 0x10, 0xCB, 0x50,
		0x0E, 0x09, 0x56, 0x25, 0x07, 0x34, 0xC9, 0x20,
		0xEC, 0x13, 0x43, 0x23, 0x5C, 0x08, 0x8B, 0xCD,
		0xDC, 0x86, 0x8C, 0xEE, 0x0A, 0x95, 0x2E, 0xB9,
		0x8C, 0x7B, 0x02, 0x7A, 0xD4, 0xE1, 0x49, 0xB4,
		0x45, 0xB5, 0x52, 0x37, 0xC6, 0xFF, 0xFE, 0xAA,
		0x0A, 0x87, 0xB8, 0x51, 0xF9, 0x2A, 0x01, 0x8F
};
/* End Session 1  */
/* Begin Session 2 */
static  uint8_t ms_aes_cbc_key2[] = {
		0xff, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static  uint8_t ms_aes_cbc_iv2[] = {
		0xff, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const uint8_t ms_aes_cbc_cipher2[] = {
		0xBB, 0x3C, 0x68, 0x25, 0xFD, 0xB6, 0xA2, 0x91,
		0x20, 0x56, 0xF6, 0x30, 0x35, 0xFC, 0x9E, 0x97,
		0xF2, 0x90, 0xFC, 0x7E, 0x3E, 0x0A, 0x75, 0xC8,
		0x4C, 0xF2, 0x2D, 0xAC, 0xD3, 0x93, 0xF0, 0xC5,
		0x14, 0x88, 0x8A, 0x23, 0xC2, 0x59, 0x9A, 0x98,
		0x4B, 0xD5, 0x2C, 0xDA, 0x43, 0xA9, 0x34, 0x69,
		0x7C, 0x6D, 0xDB, 0xDC, 0xCB, 0xC0, 0xA0, 0x09,
		0xA7, 0x86, 0x16, 0x4B, 0xBF, 0xA8, 0xB6, 0xCF,
		0x7F, 0x74, 0x1F, 0x22, 0xF0, 0xF6, 0xBB, 0x44,
		0x8B, 0x4C, 0x9E, 0x23, 0xF8, 0x9F, 0xFC, 0x5B,
		0x9E, 0x9C, 0x2A, 0x79, 0x30, 0x8F, 0xBF, 0xA9,
		0x68, 0xA1, 0x20, 0x71, 0x7C, 0x77, 0x22, 0x34,
		0x07, 0xCD, 0xC6, 0xF6, 0x50, 0x0A, 0x08, 0x99,
		0x17, 0x98, 0xE3, 0x93, 0x8A, 0xB0, 0xEE, 0xDF,
		0xC2, 0xBA, 0x3B, 0x44, 0x73, 0xDF, 0xDD, 0xDC,
		0x14, 0x4D, 0x3B, 0xBB, 0x5E, 0x58, 0xC1, 0x26,
		0xA7, 0xAE, 0x47, 0xF3, 0x24, 0x6D, 0x4F, 0xD3,
		0x6E, 0x3E, 0x33, 0xE6, 0x7F, 0xCA, 0x50, 0xAF,
		0x5D, 0x3D, 0xA0, 0xDD, 0xC9, 0xF3, 0x30, 0xD3,
		0x6E, 0x8B, 0x2E, 0x12, 0x24, 0x34, 0xF0, 0xD3,
		0xC7, 0x8D, 0x23, 0x29, 0xAA, 0x05, 0xE1, 0xFA,
		0x2E, 0xF6, 0x8D, 0x37, 0x86, 0xC0, 0x6D, 0x13,
		0x2D, 0x98, 0xF3, 0x52, 0x39, 0x22, 0xCE, 0x38,
		0xC2, 0x1A, 0x72, 0xED, 0xFB, 0xCC, 0xE4, 0x71,
		0x5A, 0x0C, 0x0D, 0x09, 0xF8, 0xE8, 0x1B, 0xBC,
		0x53, 0xC8, 0xD8, 0x8F, 0xE5, 0x98, 0x5A, 0xB1,
		0x06, 0xA6, 0x5B, 0xE6, 0xA2, 0x88, 0x21, 0x9E,
		0x36, 0xC0, 0x34, 0xF9, 0xFB, 0x3B, 0x0A, 0x22,
		0x00, 0x00, 0x39, 0x48, 0x8D, 0x23, 0x74, 0x62,
		0x72, 0x91, 0xE6, 0x36, 0xAA, 0x77, 0x9C, 0x72,
		0x9D, 0xA8, 0xC3, 0xA9, 0xD5, 0x44, 0x72, 0xA6,
		0xB9, 0x28, 0x8F, 0x64, 0x4C, 0x8A, 0x64, 0xE6,
		0x4E, 0xFA, 0xEF, 0x87, 0xDE, 0x7B, 0x22, 0x44,
		0xB0, 0xDF, 0x2E, 0x5F, 0x0B, 0xA5, 0xF2, 0x24,
		0x07, 0x5C, 0x2D, 0x39, 0xB7, 0x3D, 0x8A, 0xE5,
		0x0E, 0x9D, 0x4E, 0x50, 0xED, 0x03, 0x99, 0x8E,
		0xF0, 0x06, 0x55, 0x4E, 0xA2, 0x24, 0xE7, 0x17,
		0x46, 0xDF, 0x6C, 0xCD, 0xC6, 0x44, 0xE8, 0xF9,
		0xB9, 0x1B, 0x36, 0xF6, 0x7F, 0x10, 0xA4, 0x7D,
		0x90, 0xBD, 0xE4, 0xAA, 0xD6, 0x9E, 0x18, 0x9D,
		0x22, 0x35, 0xD6, 0x55, 0x54, 0xAA, 0xF7, 0x22,
		0xA3, 0x3E, 0xEF, 0xC8, 0xA2, 0x34, 0x8D, 0xA9,
		0x37, 0x63, 0xA6, 0xC3, 0x57, 0xCB, 0x0C, 0x49,
		0x7D, 0x02, 0xBE, 0xAA, 0x13, 0x75, 0xB7, 0x4E,
		0x52, 0x62, 0xA5, 0xC2, 0x33, 0xC7, 0x6C, 0x1B,
		0xF6, 0x34, 0xF6, 0x09, 0xA5, 0x0C, 0xC7, 0xA2,
		0x61, 0x48, 0x62, 0x7D, 0x17, 0x15, 0xE3, 0x95,
		0xC8, 0x63, 0xD2, 0xA4, 0x43, 0xA9, 0x49, 0x07,
		0xB2, 0x3B, 0x2B, 0x62, 0x7D, 0xCB, 0x51, 0xB3,
		0x25, 0x33, 0x47, 0x0E, 0x14, 0x67, 0xDC, 0x6A,
		0x9B, 0x51, 0xAC, 0x9D, 0x8F, 0xA2, 0x2B, 0x57,
		0x8C, 0x5C, 0x5F, 0x76, 0x23, 0x92, 0x0F, 0x84,
		0x46, 0x0E, 0x40, 0x85, 0x38, 0x60, 0xFA, 0x61,
		0x20, 0xC5, 0xE3, 0xF1, 0x70, 0xAC, 0x1B, 0xBF,
		0xC4, 0x2B, 0xC5, 0x67, 0xD1, 0x43, 0xC5, 0x17,
		0x74, 0x71, 0x69, 0x6F, 0x82, 0x89, 0x19, 0x8A,
		0x70, 0x43, 0x92, 0x01, 0xC4, 0x63, 0x7E, 0xB1,
		0x59, 0x4E, 0xCD, 0xEA, 0x93, 0xA4, 0x52, 0x53,
		0x9B, 0x61, 0x5B, 0xD2, 0x3E, 0x19, 0x39, 0xB7,
		0x32, 0xEA, 0x8E, 0xF8, 0x1D, 0x76, 0x5C, 0xB2,
		0x73, 0x2D, 0x91, 0xC0, 0x18, 0xED, 0x25, 0x2A,
		0x53, 0x64, 0xF0, 0x92, 0x31, 0x55, 0x21, 0xA8,
		0x24, 0xA9, 0xD1, 0x02, 0xF6, 0x6C, 0x2B, 0x70,
		0xA9, 0x59, 0xC1, 0xD6, 0xC3, 0x57, 0x5B, 0x92
};

static  uint8_t ms_hmac_key2[] = {
		0xFC, 0x1A, 0x7D, 0x3D, 0xF5, 0x82, 0x80, 0xF1,
		0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
		0x58, 0x34, 0x85, 0x65, 0x1C, 0x42, 0x50, 0x76,
		0x9A, 0xAF, 0x88, 0x1B, 0xB6, 0x8F, 0xF8, 0x60,
		0xA2, 0x5A, 0x7F, 0x3F, 0xF4, 0x72, 0x70, 0xF1,
		0xF5, 0x35, 0x4C, 0x3B, 0xDD, 0x90, 0x65, 0xB0,
		0x47, 0x3A, 0x75, 0x61, 0x5C, 0xA2, 0x10, 0x76,
		0x9A, 0xAF, 0x77, 0x5B, 0xB6, 0x7F, 0xF7, 0x60
};

static const uint8_t ms_hmac_digest2[] = {
		0xA5, 0x0F, 0x9C, 0xFB, 0x08, 0x62, 0x59, 0xFF,
		0x80, 0x2F, 0xEB, 0x4B, 0xE1, 0x46, 0x21, 0xD6,
		0x02, 0x98, 0xF2, 0x8E, 0xF4, 0xEC, 0xD4, 0x77,
		0x86, 0x4C, 0x31, 0x28, 0xC8, 0x25, 0x80, 0x27,
		0x3A, 0x72, 0x5D, 0x6A, 0x56, 0x8A, 0xD3, 0x82,
		0xB0, 0xEC, 0x31, 0x6D, 0x8B, 0x6B, 0xB4, 0x24,
		0xE7, 0x62, 0xC1, 0x52, 0xBC, 0x14, 0x1B, 0x8E,
		0xEC, 0x9A, 0xF1, 0x47, 0x80, 0xD2, 0xB0, 0x59
};

/* End Session 2 */


static int
test_AES_CBC_HMAC_SHA1_encrypt_digest(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_SHA1_HMAC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_AES_CBC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Generate test mbuf data and space for digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			catch_22_quote,	QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA1);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	ut_params->cipher_xform.cipher.key.data = aes_cbc_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;
	ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
	ut_params->cipher_xform.cipher.iv.length = CIPHER_IV_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;

	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	ut_params->auth_xform.auth.key.length = HMAC_KEY_LENGTH_SHA1;
	ut_params->auth_xform.auth.key.data = hmac_sha1_key;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_SHA1;

	rte_errno = 0;
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->cipher_xform,
			ts_params->session_mpool);
	if (rte_errno == ENOTSUP)
		return TEST_SKIPPED;
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	/* Generate crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* Set crypto operation authentication parameters */
	sym_op->auth.digest.data = ut_params->digest;
	sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
			ut_params->ibuf, QUOTE_512_BYTES);

	sym_op->auth.data.offset = 0;
	sym_op->auth.data.length = QUOTE_512_BYTES;

	/* Copy IV at the end of the crypto operation */
	rte_memcpy(rte_crypto_op_ctod_offset(ut_params->op, uint8_t *, IV_OFFSET),
			aes_cbc_iv, CIPHER_IV_LENGTH_AES_CBC);

	/* Set crypto operation cipher parameters */
	sym_op->cipher.data.offset = 0;
	sym_op->cipher.data.length = QUOTE_512_BYTES;

	/* Process crypto operation */
	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
	else
		TEST_ASSERT_NOT_NULL(
			process_crypto_request(ts_params->valid_devs[0],
				ut_params->op),
				"failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	/* Validate obuf */
	uint8_t *ciphertext = rte_pktmbuf_mtod(ut_params->op->sym->m_src,
			uint8_t *);

	TEST_ASSERT_BUFFERS_ARE_EQUAL(ciphertext,
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			QUOTE_512_BYTES,
			"ciphertext data not as expected");

	uint8_t *digest = ciphertext + QUOTE_512_BYTES;

	TEST_ASSERT_BUFFERS_ARE_EQUAL(digest,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA1_digest,
			gbl_driver_id == rte_cryptodev_driver_id_get(
					RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD)) ?
					TRUNCATED_DIGEST_BYTE_LENGTH_SHA1 :
					DIGEST_BYTE_LENGTH_SHA1,
			"Generated digest data not as expected");

	return TEST_SUCCESS;
}

/* ***** AES-CBC / HMAC-SHA512 Hash Tests ***** */

#define HMAC_KEY_LENGTH_SHA512  (DIGEST_BYTE_LENGTH_SHA512)

static uint8_t hmac_sha512_key[] = {
	0x42, 0x1a, 0x7d, 0x3d, 0xf5, 0x82, 0x80, 0xf1,
	0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA,
	0x58, 0x34, 0x85, 0x65, 0x1C, 0x42, 0x50, 0x76,
	0x9a, 0xaf, 0x88, 0x1b, 0xb6, 0x8f, 0xf8, 0x60,
	0xa2, 0x5a, 0x7f, 0x3f, 0xf4, 0x72, 0x70, 0xf1,
	0xF5, 0x35, 0x4C, 0x3B, 0xDD, 0x90, 0x65, 0xB0,
	0x47, 0x3a, 0x75, 0x61, 0x5C, 0xa2, 0x10, 0x76,
	0x9a, 0xaf, 0x77, 0x5b, 0xb6, 0x7f, 0xf7, 0x60 };

static const uint8_t catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA512_digest[] = {
	0x5D, 0x54, 0x66, 0xC1, 0x6E, 0xBC, 0x04, 0xB8,
	0x46, 0xB8, 0x08, 0x6E, 0xE0, 0xF0, 0x43, 0x48,
	0x37, 0x96, 0x9C, 0xC6, 0x9C, 0xC2, 0x1E, 0xE8,
	0xF2, 0x0C, 0x0B, 0xEF, 0x86, 0xA2, 0xE3, 0x70,
	0x95, 0xC8, 0xB3, 0x06, 0x47, 0xA9, 0x90, 0xE8,
	0xA0, 0xC6, 0x72, 0x69, 0x05, 0xC0, 0x0D, 0x0E,
	0x21, 0x96, 0x65, 0x93, 0x74, 0x43, 0x2A, 0x1D,
	0x2E, 0xBF, 0xC2, 0xC2, 0xEE, 0xCC, 0x2F, 0x0A };



static int
test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
		struct crypto_unittest_params *ut_params,
		uint8_t *cipher_key,
		uint8_t *hmac_key);

static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(void *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_params,
		const uint8_t *cipher,
		const uint8_t *digest,
		const uint8_t *iv);


static int
test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
		struct crypto_unittest_params *ut_params,
		uint8_t *cipher_key,
		uint8_t *hmac_key)
{

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	ut_params->cipher_xform.cipher.key.data = cipher_key;
	ut_params->cipher_xform.cipher.key.length = CIPHER_KEY_LENGTH_AES_CBC;
	ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
	ut_params->cipher_xform.cipher.iv.length = CIPHER_IV_LENGTH_AES_CBC;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = &ut_params->cipher_xform;

	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA512_HMAC;
	ut_params->auth_xform.auth.key.data = hmac_key;
	ut_params->auth_xform.auth.key.length = HMAC_KEY_LENGTH_SHA512;
	ut_params->auth_xform.auth.digest_length = DIGEST_BYTE_LENGTH_SHA512;

	return TEST_SUCCESS;
}


static int
test_AES_CBC_HMAC_SHA512_decrypt_perform(void *sess,
		struct crypto_unittest_params *ut_params,
		struct crypto_testsuite_params *ts_params,
		const uint8_t *cipher,
		const uint8_t *digest,
		const uint8_t *iv)
{
	int ret;

	/* Generate test mbuf data and digest */
	ut_params->ibuf = setup_test_string(ts_params->mbuf_pool,
			(const char *)
			cipher,
			QUOTE_512_BYTES, 0);

	ut_params->digest = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			DIGEST_BYTE_LENGTH_SHA512);
	TEST_ASSERT_NOT_NULL(ut_params->digest, "no room to append digest");

	rte_memcpy(ut_params->digest,
			digest,
			DIGEST_BYTE_LENGTH_SHA512);

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	rte_crypto_op_attach_sym_session(ut_params->op, sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	sym_op->auth.digest.data = ut_params->digest;
	sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
			ut_params->ibuf, QUOTE_512_BYTES);

	sym_op->auth.data.offset = 0;
	sym_op->auth.data.length = QUOTE_512_BYTES;

	/* Copy IV at the end of the crypto operation */
	rte_memcpy(rte_crypto_op_ctod_offset(ut_params->op, uint8_t *, IV_OFFSET),
			iv, CIPHER_IV_LENGTH_AES_CBC);

	sym_op->cipher.data.offset = 0;
	sym_op->cipher.data.length = QUOTE_512_BYTES;

	/* Process crypto operation */
	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
	else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		ret = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 0, 0);
		if (ret != TEST_SUCCESS)
			return ret;
	} else
		TEST_ASSERT_NOT_NULL(
				process_crypto_request(ts_params->valid_devs[0],
					ut_params->op),
					"failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	ut_params->obuf = ut_params->op->sym->m_src;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			rte_pktmbuf_mtod(ut_params->obuf, uint8_t *),
			catch_22_quote,
			QUOTE_512_BYTES,
			"Plaintext data not as expected");

	/* Validate obuf */
	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"Digest verification failed");

	return TEST_SUCCESS;
}

/* ***** SNOW 3G Tests ***** */
static int
create_wireless_algo_hash_session(uint8_t dev_id,
	const uint8_t *key, const uint8_t key_len,
	const uint8_t iv_len, const uint8_t auth_len,
	enum rte_crypto_auth_operation op,
	enum rte_crypto_auth_algorithm algo)
{
	uint8_t hash_key[key_len];

	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	memcpy(hash_key, key, key_len);

	debug_hexdump(stdout, "key:", key, key_len);

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.op = op;
	ut_params->auth_xform.auth.algo = algo;
	ut_params->auth_xform.auth.key.length = key_len;
	ut_params->auth_xform.auth.key.data = hash_key;
	ut_params->auth_xform.auth.digest_length = auth_len;
	ut_params->auth_xform.auth.iv.offset = IV_OFFSET;
	ut_params->auth_xform.auth.iv.length = iv_len;
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
			&ut_params->auth_xform, ts_params->session_mpool);
	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");
	return 0;
}

static int
create_wireless_algo_cipher_session(uint8_t dev_id,
			enum rte_crypto_cipher_operation op,
			enum rte_crypto_cipher_algorithm algo,
			const uint8_t *key, const uint8_t key_len,
			uint8_t iv_len)
{
	uint8_t cipher_key[key_len];
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	memcpy(cipher_key, key, key_len);

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = algo;
	ut_params->cipher_xform.cipher.op = op;
	ut_params->cipher_xform.cipher.key.data = cipher_key;
	ut_params->cipher_xform.cipher.key.length = key_len;
	ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
	ut_params->cipher_xform.cipher.iv.length = iv_len;

	debug_hexdump(stdout, "key:", key, key_len);

	/* Create Crypto session */
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
			&ut_params->cipher_xform, ts_params->session_mpool);

	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");
	return 0;
}

static int
create_wireless_algo_cipher_operation(const uint8_t *iv, uint8_t iv_len,
			unsigned int cipher_len,
			unsigned int cipher_offset)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
				"Failed to allocate pktmbuf offload");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* iv */
	rte_memcpy(rte_crypto_op_ctod_offset(ut_params->op, uint8_t *, IV_OFFSET),
			iv, iv_len);
	sym_op->cipher.data.length = cipher_len;
	sym_op->cipher.data.offset = cipher_offset;
	return 0;
}

static int
create_wireless_algo_cipher_operation_oop(const uint8_t *iv, uint8_t iv_len,
			unsigned int cipher_len,
			unsigned int cipher_offset)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
				"Failed to allocate pktmbuf offload");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;
	sym_op->m_dst = ut_params->obuf;

	/* iv */
	rte_memcpy(rte_crypto_op_ctod_offset(ut_params->op, uint8_t *, IV_OFFSET),
			iv, iv_len);
	sym_op->cipher.data.length = cipher_len;
	sym_op->cipher.data.offset = cipher_offset;
	return 0;
}

static int
create_wireless_algo_cipher_auth_session(uint8_t dev_id,
		enum rte_crypto_cipher_operation cipher_op,
		enum rte_crypto_auth_operation auth_op,
		enum rte_crypto_auth_algorithm auth_algo,
		enum rte_crypto_cipher_algorithm cipher_algo,
		const uint8_t *a_key, uint8_t a_key_len,
		const uint8_t *c_key, uint8_t c_key_len,
		uint8_t auth_iv_len, uint8_t auth_len,
		uint8_t cipher_iv_len)

{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.op = auth_op;
	ut_params->auth_xform.auth.algo = auth_algo;
	ut_params->auth_xform.auth.key.length = a_key_len;
	ut_params->auth_xform.auth.key.data = a_key;
	ut_params->auth_xform.auth.digest_length = auth_len;
	/* Auth IV will be after cipher IV */
	ut_params->auth_xform.auth.iv.offset = IV_OFFSET + cipher_iv_len;
	ut_params->auth_xform.auth.iv.length = auth_iv_len;

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = cipher_algo;
	ut_params->cipher_xform.cipher.op = cipher_op;
	ut_params->cipher_xform.cipher.key.data = c_key;
	ut_params->cipher_xform.cipher.key.length = c_key_len;
	ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
	ut_params->cipher_xform.cipher.iv.length = cipher_iv_len;

	debug_hexdump(stdout, "Auth key:", a_key, c_key_len);
	debug_hexdump(stdout, "Cipher key:", c_key, c_key_len);

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
			&ut_params->cipher_xform, ts_params->session_mpool);
	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");
	return 0;
}

static int
create_wireless_cipher_auth_session(uint8_t dev_id,
		enum rte_crypto_cipher_operation cipher_op,
		enum rte_crypto_auth_operation auth_op,
		enum rte_crypto_auth_algorithm auth_algo,
		enum rte_crypto_cipher_algorithm cipher_algo,
		const struct wireless_test_data *tdata)
{
	const uint8_t key_len = tdata->key.len;
	uint8_t cipher_auth_key[key_len];

	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	const uint8_t *key = tdata->key.data;
	const uint8_t auth_len = tdata->digest.len;
	uint8_t cipher_iv_len = tdata->cipher_iv.len;
	uint8_t auth_iv_len = tdata->auth_iv.len;

	memcpy(cipher_auth_key, key, key_len);

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.op = auth_op;
	ut_params->auth_xform.auth.algo = auth_algo;
	ut_params->auth_xform.auth.key.length = key_len;
	/* Hash key = cipher key */
	ut_params->auth_xform.auth.key.data = cipher_auth_key;
	ut_params->auth_xform.auth.digest_length = auth_len;
	/* Auth IV will be after cipher IV */
	ut_params->auth_xform.auth.iv.offset = IV_OFFSET + cipher_iv_len;
	ut_params->auth_xform.auth.iv.length = auth_iv_len;

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = cipher_algo;
	ut_params->cipher_xform.cipher.op = cipher_op;
	ut_params->cipher_xform.cipher.key.data = cipher_auth_key;
	ut_params->cipher_xform.cipher.key.length = key_len;
	ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
	ut_params->cipher_xform.cipher.iv.length = cipher_iv_len;


	debug_hexdump(stdout, "key:", key, key_len);

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
			&ut_params->cipher_xform, ts_params->session_mpool);
	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");
	return 0;
}

static int
create_zuc_cipher_auth_encrypt_generate_session(uint8_t dev_id,
		const struct wireless_test_data *tdata)
{
	return create_wireless_cipher_auth_session(dev_id,
		RTE_CRYPTO_CIPHER_OP_ENCRYPT,
		RTE_CRYPTO_AUTH_OP_GENERATE, RTE_CRYPTO_AUTH_ZUC_EIA3,
		RTE_CRYPTO_CIPHER_ZUC_EEA3, tdata);
}

static int
create_wireless_algo_auth_cipher_session(uint8_t dev_id,
		enum rte_crypto_cipher_operation cipher_op,
		enum rte_crypto_auth_operation auth_op,
		enum rte_crypto_auth_algorithm auth_algo,
		enum rte_crypto_cipher_algorithm cipher_algo,
		const uint8_t *a_key, const uint8_t a_key_len,
		const uint8_t *c_key, const uint8_t c_key_len,
		uint8_t auth_iv_len, uint8_t auth_len,
		uint8_t cipher_iv_len)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.auth.op = auth_op;
	ut_params->auth_xform.next = &ut_params->cipher_xform;
	ut_params->auth_xform.auth.algo = auth_algo;
	ut_params->auth_xform.auth.key.length = a_key_len;
	ut_params->auth_xform.auth.key.data = a_key;
	ut_params->auth_xform.auth.digest_length = auth_len;
	/* Auth IV will be after cipher IV */
	ut_params->auth_xform.auth.iv.offset = IV_OFFSET + cipher_iv_len;
	ut_params->auth_xform.auth.iv.length = auth_iv_len;

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;
	ut_params->cipher_xform.cipher.algo = cipher_algo;
	ut_params->cipher_xform.cipher.op = cipher_op;
	ut_params->cipher_xform.cipher.key.data = c_key;
	ut_params->cipher_xform.cipher.key.length = c_key_len;
	ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
	ut_params->cipher_xform.cipher.iv.length = cipher_iv_len;

	debug_hexdump(stdout, "Auth key:", a_key, a_key_len);
	debug_hexdump(stdout, "Cipher key:", c_key, c_key_len);

	/* Create Crypto session*/
	if (cipher_op == RTE_CRYPTO_CIPHER_OP_DECRYPT) {
		ut_params->auth_xform.next = NULL;
		ut_params->cipher_xform.next = &ut_params->auth_xform;
		ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
			&ut_params->cipher_xform, ts_params->session_mpool);
	} else
		ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
			&ut_params->auth_xform, ts_params->session_mpool);

	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;

	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	return 0;
}

static int
create_wireless_algo_hash_operation(const uint8_t *auth_tag,
		unsigned int auth_tag_len,
		const uint8_t *iv, unsigned int iv_len,
		unsigned int data_pad_len,
		enum rte_crypto_auth_operation op,
		unsigned int auth_len, unsigned int auth_offset)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;

	struct crypto_unittest_params *ut_params = &unittest_params;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
		"Failed to allocate pktmbuf offload");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* iv */
	rte_memcpy(rte_crypto_op_ctod_offset(ut_params->op, uint8_t *, IV_OFFSET),
			iv, iv_len);
	/* digest */
	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
					ut_params->ibuf, auth_tag_len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
				"no room to append auth tag");
	ut_params->digest = sym_op->auth.digest.data;
	sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
			ut_params->ibuf, data_pad_len);
	if (op == RTE_CRYPTO_AUTH_OP_GENERATE)
		memset(sym_op->auth.digest.data, 0, auth_tag_len);
	else
		rte_memcpy(sym_op->auth.digest.data, auth_tag, auth_tag_len);

	debug_hexdump(stdout, "digest:",
		sym_op->auth.digest.data,
		auth_tag_len);

	sym_op->auth.data.length = auth_len;
	sym_op->auth.data.offset = auth_offset;

	return 0;
}

static int
create_wireless_cipher_hash_operation(const struct wireless_test_data *tdata,
	enum rte_crypto_auth_operation op)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	const uint8_t *auth_tag = tdata->digest.data;
	const unsigned int auth_tag_len = tdata->digest.len;
	unsigned int plaintext_len = ceil_byte_length(tdata->plaintext.len);
	unsigned int data_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	const uint8_t *cipher_iv = tdata->cipher_iv.data;
	const uint8_t cipher_iv_len = tdata->cipher_iv.len;
	const uint8_t *auth_iv = tdata->auth_iv.data;
	const uint8_t auth_iv_len = tdata->auth_iv.len;
	const unsigned int cipher_len = tdata->validCipherLenInBits.len;
	const unsigned int auth_len = tdata->validAuthLenInBits.len;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate pktmbuf offload");
	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* digest */
	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, auth_tag_len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append auth tag");
	ut_params->digest = sym_op->auth.digest.data;
	sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
			ut_params->ibuf, data_pad_len);
	if (op == RTE_CRYPTO_AUTH_OP_GENERATE)
		memset(sym_op->auth.digest.data, 0, auth_tag_len);
	else
		rte_memcpy(sym_op->auth.digest.data, auth_tag, auth_tag_len);

	debug_hexdump(stdout, "digest:",
		sym_op->auth.digest.data,
		auth_tag_len);

	/* Copy cipher and auth IVs at the end of the crypto operation */
	uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ut_params->op, uint8_t *,
						IV_OFFSET);
	rte_memcpy(iv_ptr, cipher_iv, cipher_iv_len);
	iv_ptr += cipher_iv_len;
	rte_memcpy(iv_ptr, auth_iv, auth_iv_len);

	sym_op->cipher.data.length = cipher_len;
	sym_op->cipher.data.offset = 0;
	sym_op->auth.data.length = auth_len;
	sym_op->auth.data.offset = 0;

	return 0;
}

static int
create_zuc_cipher_hash_generate_operation(
		const struct wireless_test_data *tdata)
{
	return create_wireless_cipher_hash_operation(tdata,
		RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
create_wireless_algo_cipher_hash_operation(const uint8_t *auth_tag,
		const unsigned auth_tag_len,
		const uint8_t *auth_iv, uint8_t auth_iv_len,
		unsigned data_pad_len,
		enum rte_crypto_auth_operation op,
		const uint8_t *cipher_iv, uint8_t cipher_iv_len,
		const unsigned cipher_len, const unsigned cipher_offset,
		const unsigned auth_len, const unsigned auth_offset)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	enum rte_crypto_cipher_algorithm cipher_algo =
			ut_params->cipher_xform.cipher.algo;
	enum rte_crypto_auth_algorithm auth_algo =
			ut_params->auth_xform.auth.algo;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate pktmbuf offload");
	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* digest */
	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, auth_tag_len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append auth tag");
	ut_params->digest = sym_op->auth.digest.data;

	if (rte_pktmbuf_is_contiguous(ut_params->ibuf)) {
		sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
				ut_params->ibuf, data_pad_len);
	} else {
		struct rte_mbuf *m = ut_params->ibuf;
		unsigned int offset = data_pad_len;

		while (offset > m->data_len && m->next != NULL) {
			offset -= m->data_len;
			m = m->next;
		}
		sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
			m, offset);
	}

	if (op == RTE_CRYPTO_AUTH_OP_GENERATE)
		memset(sym_op->auth.digest.data, 0, auth_tag_len);
	else
		rte_memcpy(sym_op->auth.digest.data, auth_tag, auth_tag_len);

	debug_hexdump(stdout, "digest:",
		sym_op->auth.digest.data,
		auth_tag_len);

	/* Copy cipher and auth IVs at the end of the crypto operation */
	uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ut_params->op, uint8_t *,
						IV_OFFSET);
	rte_memcpy(iv_ptr, cipher_iv, cipher_iv_len);
	iv_ptr += cipher_iv_len;
	rte_memcpy(iv_ptr, auth_iv, auth_iv_len);

	if (cipher_algo == RTE_CRYPTO_CIPHER_SNOW3G_UEA2 ||
		cipher_algo == RTE_CRYPTO_CIPHER_KASUMI_F8 ||
		cipher_algo == RTE_CRYPTO_CIPHER_ZUC_EEA3) {
		sym_op->cipher.data.length = cipher_len;
		sym_op->cipher.data.offset = cipher_offset;
	} else {
		sym_op->cipher.data.length = cipher_len >> 3;
		sym_op->cipher.data.offset = cipher_offset >> 3;
	}

	if (auth_algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2 ||
		auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9 ||
		auth_algo == RTE_CRYPTO_AUTH_ZUC_EIA3) {
		sym_op->auth.data.length = auth_len;
		sym_op->auth.data.offset = auth_offset;
	} else {
		sym_op->auth.data.length = auth_len >> 3;
		sym_op->auth.data.offset = auth_offset >> 3;
	}

	return 0;
}

static int
create_wireless_algo_auth_cipher_operation(
		const uint8_t *auth_tag, unsigned int auth_tag_len,
		const uint8_t *cipher_iv, uint8_t cipher_iv_len,
		const uint8_t *auth_iv, uint8_t auth_iv_len,
		unsigned int data_pad_len,
		unsigned int cipher_len, unsigned int cipher_offset,
		unsigned int auth_len, unsigned int auth_offset,
		uint8_t op_mode, uint8_t do_sgl, uint8_t verify)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	enum rte_crypto_cipher_algorithm cipher_algo =
			ut_params->cipher_xform.cipher.algo;
	enum rte_crypto_auth_algorithm auth_algo =
			ut_params->auth_xform.auth.algo;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate pktmbuf offload");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation mbufs */
	sym_op->m_src = ut_params->ibuf;
	if (op_mode == OUT_OF_PLACE)
		sym_op->m_dst = ut_params->obuf;

	/* digest */
	if (!do_sgl) {
		sym_op->auth.digest.data = rte_pktmbuf_mtod_offset(
			(op_mode == IN_PLACE ?
				ut_params->ibuf : ut_params->obuf),
			uint8_t *, data_pad_len);
		sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
			(op_mode == IN_PLACE ?
				ut_params->ibuf : ut_params->obuf),
			data_pad_len);
		memset(sym_op->auth.digest.data, 0, auth_tag_len);
	} else {
		uint16_t remaining_off = (auth_offset >> 3) + (auth_len >> 3);
		struct rte_mbuf *sgl_buf = (op_mode == IN_PLACE ?
				sym_op->m_src : sym_op->m_dst);
		while (remaining_off >= rte_pktmbuf_data_len(sgl_buf)) {
			remaining_off -= rte_pktmbuf_data_len(sgl_buf);
			sgl_buf = sgl_buf->next;
		}

		/* The last segment should be large enough to hold full digest */
		if (sgl_buf->data_len < auth_tag_len) {
			rte_pktmbuf_free(sgl_buf->next);
			sgl_buf->next = NULL;
			TEST_ASSERT_NOT_NULL(rte_pktmbuf_append(sgl_buf,
					auth_tag_len - sgl_buf->data_len),
					"No room to append auth tag");
		}

		sym_op->auth.digest.data = rte_pktmbuf_mtod_offset(sgl_buf,
				uint8_t *, remaining_off);
		sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(sgl_buf,
				remaining_off);
		memset(sym_op->auth.digest.data, 0, remaining_off);
		while (sgl_buf->next != NULL) {
			memset(rte_pktmbuf_mtod(sgl_buf, uint8_t *),
				0, rte_pktmbuf_data_len(sgl_buf));
			sgl_buf = sgl_buf->next;
		}
	}

	/* Copy digest for the verification */
	if (verify)
		memcpy(sym_op->auth.digest.data, auth_tag, auth_tag_len);

	/* Copy cipher and auth IVs at the end of the crypto operation */
	uint8_t *iv_ptr = rte_crypto_op_ctod_offset(
			ut_params->op, uint8_t *, IV_OFFSET);

	rte_memcpy(iv_ptr, cipher_iv, cipher_iv_len);
	iv_ptr += cipher_iv_len;
	rte_memcpy(iv_ptr, auth_iv, auth_iv_len);

	/* Only copy over the offset data needed from src to dst in OOP,
	 * if the auth and cipher offsets are not aligned
	 */
	if (op_mode == OUT_OF_PLACE) {
		if (cipher_offset > auth_offset)
			rte_memcpy(
				rte_pktmbuf_mtod_offset(
					sym_op->m_dst,
					uint8_t *, auth_offset >> 3),
				rte_pktmbuf_mtod_offset(
					sym_op->m_src,
					uint8_t *, auth_offset >> 3),
				((cipher_offset >> 3) - (auth_offset >> 3)));
	}

	if (cipher_algo == RTE_CRYPTO_CIPHER_SNOW3G_UEA2 ||
		cipher_algo == RTE_CRYPTO_CIPHER_KASUMI_F8 ||
		cipher_algo == RTE_CRYPTO_CIPHER_ZUC_EEA3) {
		sym_op->cipher.data.length = cipher_len;
		sym_op->cipher.data.offset = cipher_offset;
	} else {
		sym_op->cipher.data.length = cipher_len >> 3;
		sym_op->cipher.data.offset = cipher_offset >> 3;
	}

	if (auth_algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2 ||
		auth_algo == RTE_CRYPTO_AUTH_KASUMI_F9 ||
		auth_algo == RTE_CRYPTO_AUTH_ZUC_EIA3) {
		sym_op->auth.data.length = auth_len;
		sym_op->auth.data.offset = auth_offset;
	} else {
		sym_op->auth.data.length = auth_len >> 3;
		sym_op->auth.data.offset = auth_offset >> 3;
	}

	return 0;
}

static int
test_snow3g_authentication(const struct snow3g_hash_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	uint8_t *plaintext;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA) &&
			((tdata->validAuthLenInBits.len % 8) != 0)) {
		printf("Device doesn't support NON-Byte Aligned Data.\n");
		return TEST_SKIPPED;
	}

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_SNOW3G_UIA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create SNOW 3G session */
	retval = create_wireless_algo_hash_session(ts_params->valid_devs[0],
			tdata->key.data, tdata->key.len,
			tdata->auth_iv.len, tdata->digest.len,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_SNOW3G_UIA2);
	if (retval < 0)
		return retval;

	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_hash_operation(NULL, tdata->digest.len,
			tdata->auth_iv.data, tdata->auth_iv.len,
			plaintext_pad_len, RTE_CRYPTO_AUTH_OP_GENERATE,
			tdata->validAuthLenInBits.len,
			0);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 1, 1,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
				ut_params->op);
	ut_params->obuf = ut_params->op->sym->m_src;
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
			+ plaintext_pad_len;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
	ut_params->digest,
	tdata->digest.data,
	DIGEST_BYTE_LENGTH_SNOW3G_UIA2,
	"SNOW 3G Generated auth tag not as expected");

	return 0;
}

static int
test_snow3g_authentication_verify(const struct snow3g_hash_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	uint8_t *plaintext;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA) &&
			((tdata->validAuthLenInBits.len % 8) != 0)) {
		printf("Device doesn't support NON-Byte Aligned Data.\n");
		return TEST_SKIPPED;
	}

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_SNOW3G_UIA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create SNOW 3G session */
	retval = create_wireless_algo_hash_session(ts_params->valid_devs[0],
				tdata->key.data, tdata->key.len,
				tdata->auth_iv.len, tdata->digest.len,
				RTE_CRYPTO_AUTH_OP_VERIFY,
				RTE_CRYPTO_AUTH_SNOW3G_UIA2);
	if (retval < 0)
		return retval;
	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_hash_operation(tdata->digest.data,
			tdata->digest.len,
			tdata->auth_iv.data, tdata->auth_iv.len,
			plaintext_pad_len,
			RTE_CRYPTO_AUTH_OP_VERIFY,
			tdata->validAuthLenInBits.len,
			0);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 1, 1,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
				ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_src;
	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ plaintext_pad_len;

	/* Validate obuf */
	if (ut_params->op->status == RTE_CRYPTO_OP_STATUS_SUCCESS)
		return 0;
	else
		return -1;

	return 0;
}

static int
test_kasumi_authentication(const struct kasumi_hash_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	uint8_t *plaintext;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_KASUMI_F9;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create KASUMI session */
	retval = create_wireless_algo_hash_session(ts_params->valid_devs[0],
			tdata->key.data, tdata->key.len,
			0, tdata->digest.len,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_KASUMI_F9);
	if (retval < 0)
		return retval;

	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	/* Create KASUMI operation */
	retval = create_wireless_algo_hash_operation(NULL, tdata->digest.len,
			NULL, 0,
			plaintext_pad_len, RTE_CRYPTO_AUTH_OP_GENERATE,
			tdata->plaintext.len,
			0);
	if (retval < 0)
		return retval;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
	else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 1, 1,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);

	ut_params->obuf = ut_params->op->sym->m_src;
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
			+ plaintext_pad_len;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
	ut_params->digest,
	tdata->digest.data,
	DIGEST_BYTE_LENGTH_KASUMI_F9,
	"KASUMI Generated auth tag not as expected");

	return 0;
}

static int
test_kasumi_authentication_verify(const struct kasumi_hash_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	uint8_t *plaintext;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_KASUMI_F9;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create KASUMI session */
	retval = create_wireless_algo_hash_session(ts_params->valid_devs[0],
				tdata->key.data, tdata->key.len,
				0, tdata->digest.len,
				RTE_CRYPTO_AUTH_OP_VERIFY,
				RTE_CRYPTO_AUTH_KASUMI_F9);
	if (retval < 0)
		return retval;
	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	/* Create KASUMI operation */
	retval = create_wireless_algo_hash_operation(tdata->digest.data,
			tdata->digest.len,
			NULL, 0,
			plaintext_pad_len,
			RTE_CRYPTO_AUTH_OP_VERIFY,
			tdata->plaintext.len,
			0);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 1, 1,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
				ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_src;
	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ plaintext_pad_len;

	/* Validate obuf */
	if (ut_params->op->status == RTE_CRYPTO_OP_STATUS_SUCCESS)
		return 0;
	else
		return -1;

	return 0;
}

static int
test_snow3g_hash_generate_test_case_1(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_1);
}

static int
test_snow3g_hash_generate_test_case_2(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_2);
}

static int
test_snow3g_hash_generate_test_case_3(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_3);
}

static int
test_snow3g_hash_generate_test_case_4(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_4);
}

static int
test_snow3g_hash_generate_test_case_5(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_5);
}

static int
test_snow3g_hash_generate_test_case_6(void)
{
	return test_snow3g_authentication(&snow3g_hash_test_case_6);
}

static int
test_snow3g_hash_verify_test_case_1(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_1);

}

static int
test_snow3g_hash_verify_test_case_2(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_2);
}

static int
test_snow3g_hash_verify_test_case_3(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_3);
}

static int
test_snow3g_hash_verify_test_case_4(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_4);
}

static int
test_snow3g_hash_verify_test_case_5(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_5);
}

static int
test_snow3g_hash_verify_test_case_6(void)
{
	return test_snow3g_authentication_verify(&snow3g_hash_test_case_6);
}

static int
test_kasumi_hash_generate_test_case_1(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_1);
}

static int
test_kasumi_hash_generate_test_case_2(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_2);
}

static int
test_kasumi_hash_generate_test_case_3(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_3);
}

static int
test_kasumi_hash_generate_test_case_4(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_4);
}

static int
test_kasumi_hash_generate_test_case_5(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_5);
}

static int
test_kasumi_hash_generate_test_case_6(void)
{
	return test_kasumi_authentication(&kasumi_hash_test_case_6);
}

static int
test_kasumi_hash_verify_test_case_1(void)
{
	return test_kasumi_authentication_verify(&kasumi_hash_test_case_1);
}

static int
test_kasumi_hash_verify_test_case_2(void)
{
	return test_kasumi_authentication_verify(&kasumi_hash_test_case_2);
}

static int
test_kasumi_hash_verify_test_case_3(void)
{
	return test_kasumi_authentication_verify(&kasumi_hash_test_case_3);
}

static int
test_kasumi_hash_verify_test_case_4(void)
{
	return test_kasumi_authentication_verify(&kasumi_hash_test_case_4);
}

static int
test_kasumi_hash_verify_test_case_5(void)
{
	return test_kasumi_authentication_verify(&kasumi_hash_test_case_5);
}

static int
test_kasumi_encryption(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_KASUMI_F8;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create KASUMI session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	debug_hexdump(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create KASUMI operation */
	retval = create_wireless_algo_cipher_operation(tdata->cipher_iv.data,
				tdata->cipher_iv.len,
				RTE_ALIGN_CEIL(tdata->validCipherLenInBits.len, 8),
				tdata->validCipherOffsetInBits.len);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 0, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
				ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
	else
		ciphertext = plaintext + (tdata->validCipherOffsetInBits.len >> 3);

	debug_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);

	const uint8_t *reference_ciphertext = tdata->ciphertext.data +
				(tdata->validCipherOffsetInBits.len >> 3);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		reference_ciphertext,
		tdata->validCipherLenInBits.len,
		"KASUMI Ciphertext data not as expected");
	return 0;
}

static int
test_kasumi_encryption_sgl(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;

	uint8_t buffer[10000];
	const uint8_t *ciphertext;

	struct rte_cryptodev_info dev_info;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_KASUMI_F8;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)) {
		printf("Device doesn't support in-place scatter-gather. "
				"Test Skipped.\n");
		return TEST_SKIPPED;
	}

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Create KASUMI session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	plaintext_len = ceil_byte_length(tdata->plaintext.len);


	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);

	ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 10, 0);

	pktmbuf_write(ut_params->ibuf, 0, plaintext_len, tdata->plaintext.data);

	/* Create KASUMI operation */
	retval = create_wireless_algo_cipher_operation(tdata->cipher_iv.data,
				tdata->cipher_iv.len,
				RTE_ALIGN_CEIL(tdata->validCipherLenInBits.len, 8),
				tdata->validCipherOffsetInBits.len);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 0, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;

	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_read(ut_params->obuf, 0,
				plaintext_len, buffer);
	else
		ciphertext = rte_pktmbuf_read(ut_params->ibuf,
				tdata->validCipherOffsetInBits.len >> 3,
				plaintext_len, buffer);

	/* Validate obuf */
	debug_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);

	const uint8_t *reference_ciphertext = tdata->ciphertext.data +
				(tdata->validCipherOffsetInBits.len >> 3);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		reference_ciphertext,
		tdata->validCipherLenInBits.len,
		"KASUMI Ciphertext data not as expected");
	return 0;
}

static int
test_kasumi_encryption_oop(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_KASUMI_F8;
	/* Data-path service does not support OOP */
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Create KASUMI session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	rte_pktmbuf_append(ut_params->obuf, plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	debug_hexdump(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create KASUMI operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->cipher_iv.data,
				tdata->cipher_iv.len,
				RTE_ALIGN_CEIL(tdata->validCipherLenInBits.len, 8),
				tdata->validCipherOffsetInBits.len);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
	else
		ciphertext = plaintext + (tdata->validCipherOffsetInBits.len >> 3);

	debug_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);

	const uint8_t *reference_ciphertext = tdata->ciphertext.data +
				(tdata->validCipherOffsetInBits.len >> 3);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		reference_ciphertext,
		tdata->validCipherLenInBits.len,
		"KASUMI Ciphertext data not as expected");
	return 0;
}

static int
test_kasumi_encryption_oop_sgl(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;

	const uint8_t *ciphertext;
	uint8_t buffer[2048];

	struct rte_cryptodev_info dev_info;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_KASUMI_F8;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;
	if (!(feat_flags & RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT)) {
		printf("Device doesn't support out-of-place scatter-gather "
				"in both input and output mbufs. "
				"Test Skipped.\n");
		return TEST_SKIPPED;
	}

	/* Create KASUMI session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);

	ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 10, 0);
	ut_params->obuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 3, 0);

	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	pktmbuf_write(ut_params->ibuf, 0, plaintext_len, tdata->plaintext.data);

	/* Create KASUMI operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->cipher_iv.data,
				tdata->cipher_iv.len,
				RTE_ALIGN_CEIL(tdata->validCipherLenInBits.len, 8),
				tdata->validCipherOffsetInBits.len);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_read(ut_params->obuf, 0,
				plaintext_pad_len, buffer);
	else
		ciphertext = rte_pktmbuf_read(ut_params->ibuf,
				tdata->validCipherOffsetInBits.len >> 3,
				plaintext_pad_len, buffer);

	const uint8_t *reference_ciphertext = tdata->ciphertext.data +
				(tdata->validCipherOffsetInBits.len >> 3);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		reference_ciphertext,
		tdata->validCipherLenInBits.len,
		"KASUMI Ciphertext data not as expected");
	return 0;
}


static int
test_kasumi_decryption_oop(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *ciphertext, *plaintext;
	unsigned ciphertext_pad_len;
	unsigned ciphertext_len;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_KASUMI_F8;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Create KASUMI session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_DECRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 8);
	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				ciphertext_pad_len);
	rte_pktmbuf_append(ut_params->obuf, ciphertext_pad_len);
	memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);

	debug_hexdump(stdout, "ciphertext:", ciphertext, ciphertext_len);

	/* Create KASUMI operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->cipher_iv.data,
				tdata->cipher_iv.len,
				RTE_ALIGN_CEIL(tdata->validCipherLenInBits.len, 8),
				tdata->validCipherOffsetInBits.len);
	if (retval < 0)
		return retval;

	ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		plaintext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
	else
		plaintext = ciphertext + (tdata->validCipherOffsetInBits.len >> 3);

	debug_hexdump(stdout, "plaintext:", plaintext, ciphertext_len);

	const uint8_t *reference_plaintext = tdata->plaintext.data +
				(tdata->validCipherOffsetInBits.len >> 3);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		plaintext,
		reference_plaintext,
		tdata->validCipherLenInBits.len,
		"KASUMI Plaintext data not as expected");
	return 0;
}

static int
test_kasumi_decryption(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *ciphertext, *plaintext;
	unsigned ciphertext_pad_len;
	unsigned ciphertext_len;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_KASUMI_F8;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create KASUMI session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_DECRYPT,
					RTE_CRYPTO_CIPHER_KASUMI_F8,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	/* Append data which is padded to a multiple */
	/* of the algorithms block size */
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 8);
	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				ciphertext_pad_len);
	memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);

	debug_hexdump(stdout, "ciphertext:", ciphertext, ciphertext_len);

	/* Create KASUMI operation */
	retval = create_wireless_algo_cipher_operation(tdata->cipher_iv.data,
			tdata->cipher_iv.len,
			RTE_ALIGN_CEIL(tdata->validCipherLenInBits.len, 8),
			tdata->validCipherOffsetInBits.len);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 0, 1,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		plaintext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
	else
		plaintext = ciphertext + (tdata->validCipherOffsetInBits.len >> 3);

	debug_hexdump(stdout, "plaintext:", plaintext, ciphertext_len);

	const uint8_t *reference_plaintext = tdata->plaintext.data +
				(tdata->validCipherOffsetInBits.len >> 3);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		plaintext,
		reference_plaintext,
		tdata->validCipherLenInBits.len,
		"KASUMI Plaintext data not as expected");
	return 0;
}

static int
test_snow3g_encryption(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	debug_hexdump(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation(tdata->cipher_iv.data,
					tdata->cipher_iv.len,
					tdata->validCipherLenInBits.len,
					0);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 0, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
	else
		ciphertext = plaintext;

	debug_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		tdata->ciphertext.data,
		tdata->validDataLenInBits.len,
		"SNOW 3G Ciphertext data not as expected");
	return 0;
}


static int
test_snow3g_encryption_oop(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	uint8_t *plaintext, *ciphertext;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device does not support RAW data-path APIs.\n");
		return -ENOTSUP;
	}

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");
	TEST_ASSERT_NOT_NULL(ut_params->obuf,
			"Failed to allocate output buffer in mempool");

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	rte_pktmbuf_append(ut_params->obuf, plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	debug_hexdump(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->cipher_iv.data,
					tdata->cipher_iv.len,
					tdata->validCipherLenInBits.len,
					0);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 0, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
	else
		ciphertext = plaintext;

	debug_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		tdata->ciphertext.data,
		tdata->validDataLenInBits.len,
		"SNOW 3G Ciphertext data not as expected");
	return 0;
}

static int
test_snow3g_encryption_oop_sgl(const struct snow3g_test_data *tdata,
		uint8_t sgl_in, uint8_t sgl_out)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;
	uint8_t buffer[10000];
	const uint8_t *ciphertext;

	struct rte_cryptodev_info dev_info;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;

	if (((sgl_in && sgl_out) && !(feat_flags & RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT))
			|| ((!sgl_in && sgl_out) &&
			!(feat_flags & RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT))
			|| ((sgl_in && !sgl_out) &&
			!(feat_flags & RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT))) {
		printf("Device doesn't support out-of-place scatter gather type. "
				"Test Skipped.\n");
		return TEST_SKIPPED;
	}

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device does not support RAW data-path APIs.\n");
		return -ENOTSUP;
	}

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	if (sgl_in)
		ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
				plaintext_pad_len, 10, 0);
	else {
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
		rte_pktmbuf_append(ut_params->ibuf, plaintext_pad_len);
	}

	if (sgl_out)
		ut_params->obuf = create_segmented_mbuf(ts_params->mbuf_pool,
				plaintext_pad_len, 3, 0);
	else {
		ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
		rte_pktmbuf_append(ut_params->obuf, plaintext_pad_len);
	}

	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");
	TEST_ASSERT_NOT_NULL(ut_params->obuf,
			"Failed to allocate output buffer in mempool");

	pktmbuf_write(ut_params->ibuf, 0, plaintext_len, tdata->plaintext.data);

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->cipher_iv.data,
					tdata->cipher_iv.len,
					tdata->validCipherLenInBits.len,
					0);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 0, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_read(ut_params->obuf, 0,
				plaintext_len, buffer);
	else
		ciphertext = rte_pktmbuf_read(ut_params->ibuf, 0,
				plaintext_len, buffer);

	debug_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		tdata->ciphertext.data,
		tdata->validDataLenInBits.len,
		"SNOW 3G Ciphertext data not as expected");

	return 0;
}

/* Shift right a buffer by "offset" bits, "offset" < 8 */
static void
buffer_shift_right(uint8_t *buffer, uint32_t length, uint8_t offset)
{
	uint8_t curr_byte, prev_byte;
	uint32_t length_in_bytes = ceil_byte_length(length + offset);
	uint8_t lower_byte_mask = (1 << offset) - 1;
	unsigned i;

	prev_byte = buffer[0];
	buffer[0] >>= offset;

	for (i = 1; i < length_in_bytes; i++) {
		curr_byte = buffer[i];
		buffer[i] = ((prev_byte & lower_byte_mask) << (8 - offset)) |
				(curr_byte >> offset);
		prev_byte = curr_byte;
	}
}

static int
test_snow3g_encryption_offset_oop(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	uint8_t *plaintext, *ciphertext;
	int retval;
	uint32_t plaintext_len;
	uint32_t plaintext_pad_len;
	uint8_t extra_offset = 4;
	uint8_t *expected_ciphertext_shifted;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA) &&
			((tdata->validDataLenInBits.len % 8) != 0)) {
		printf("Device doesn't support NON-Byte Aligned Data.\n");
		return TEST_SKIPPED;
	}

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_ENCRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");
	TEST_ASSERT_NOT_NULL(ut_params->obuf,
			"Failed to allocate output buffer in mempool");

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len + extra_offset);
	/*
	 * Append data which is padded to a
	 * multiple of the algorithms block size
	 */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	plaintext = (uint8_t *) rte_pktmbuf_append(ut_params->ibuf,
						plaintext_pad_len);

	rte_pktmbuf_append(ut_params->obuf, plaintext_pad_len);

	memcpy(plaintext, tdata->plaintext.data, (tdata->plaintext.len >> 3));
	buffer_shift_right(plaintext, tdata->plaintext.len, extra_offset);

#ifdef RTE_APP_TEST_DEBUG
	rte_hexdump(stdout, "plaintext:", plaintext, tdata->plaintext.len);
#endif
	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->cipher_iv.data,
					tdata->cipher_iv.len,
					tdata->validCipherLenInBits.len,
					extra_offset);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 0, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
	else
		ciphertext = plaintext;

#ifdef RTE_APP_TEST_DEBUG
	rte_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);
#endif

	expected_ciphertext_shifted = rte_malloc(NULL, plaintext_len, 8);

	TEST_ASSERT_NOT_NULL(expected_ciphertext_shifted,
			"failed to reserve memory for ciphertext shifted\n");

	memcpy(expected_ciphertext_shifted, tdata->ciphertext.data,
			ceil_byte_length(tdata->ciphertext.len));
	buffer_shift_right(expected_ciphertext_shifted, tdata->ciphertext.len,
			extra_offset);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT_OFFSET(
		ciphertext,
		expected_ciphertext_shifted,
		tdata->validDataLenInBits.len,
		extra_offset,
		"SNOW 3G Ciphertext data not as expected");
	return 0;
}

static int test_snow3g_decryption(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned ciphertext_pad_len;
	unsigned ciphertext_len;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_DECRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				ciphertext_pad_len);
	memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);

	debug_hexdump(stdout, "ciphertext:", ciphertext, ciphertext_len);

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation(tdata->cipher_iv.data,
					tdata->cipher_iv.len,
					tdata->validCipherLenInBits.len,
					tdata->cipher.offset_bits);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 0, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		plaintext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
	else
		plaintext = ciphertext;

	debug_hexdump(stdout, "plaintext:", plaintext, ciphertext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(plaintext,
				tdata->plaintext.data,
				tdata->validDataLenInBits.len,
				"SNOW 3G Plaintext data not as expected");
	return 0;
}

static int test_snow3g_decryption_oop(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned ciphertext_pad_len;
	unsigned ciphertext_len;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device does not support RAW data-path APIs.\n");
		return -ENOTSUP;
	}
	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					RTE_CRYPTO_CIPHER_OP_DECRYPT,
					RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer");
	TEST_ASSERT_NOT_NULL(ut_params->obuf,
			"Failed to allocate output buffer");

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	memset(rte_pktmbuf_mtod(ut_params->obuf, uint8_t *), 0,
		       rte_pktmbuf_tailroom(ut_params->obuf));

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				ciphertext_pad_len);
	rte_pktmbuf_append(ut_params->obuf, ciphertext_pad_len);
	memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);

	debug_hexdump(stdout, "ciphertext:", ciphertext, ciphertext_len);

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_operation_oop(tdata->cipher_iv.data,
					tdata->cipher_iv.len,
					tdata->validCipherLenInBits.len,
					0);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 0, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_dst;
	if (ut_params->obuf)
		plaintext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
	else
		plaintext = ciphertext;

	debug_hexdump(stdout, "plaintext:", plaintext, ciphertext_len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(plaintext,
				tdata->plaintext.data,
				tdata->validDataLenInBits.len,
				"SNOW 3G Plaintext data not as expected");
	return 0;
}

static int
test_zuc_cipher_auth(const struct wireless_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;

	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA) &&
			((tdata->validAuthLenInBits.len % 8 != 0) ||
			(tdata->validDataLenInBits.len % 8 != 0))) {
		printf("Device doesn't support NON-Byte Aligned Data.\n");
		return TEST_SKIPPED;
	}

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Check if device supports ZUC EEA3 */
	if (check_cipher_capability(ts_params, RTE_CRYPTO_CIPHER_ZUC_EEA3,
			tdata->key.len, tdata->cipher_iv.len) < 0)
		return TEST_SKIPPED;

	/* Check if device supports ZUC EIA3 */
	if (check_auth_capability(ts_params, RTE_CRYPTO_AUTH_ZUC_EIA3,
			tdata->key.len, tdata->auth_iv.len,
			tdata->digest.len) < 0)
		return TEST_SKIPPED;

	/* Create ZUC session */
	retval = create_zuc_cipher_auth_encrypt_generate_session(
			ts_params->valid_devs[0],
			tdata);
	if (retval != 0)
		return retval;
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	debug_hexdump(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create ZUC operation */
	retval = create_zuc_cipher_hash_generate_operation(tdata);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_src;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
	else
		ciphertext = plaintext;

	debug_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			ciphertext,
			tdata->ciphertext.data,
			tdata->validDataLenInBits.len,
			"ZUC Ciphertext data not as expected");

	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
	    + plaintext_pad_len;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ut_params->digest,
			tdata->digest.data,
			tdata->digest.len,
			"ZUC Generated auth tag not as expected");
	return 0;
}

static int
test_snow3g_cipher_auth(const struct snow3g_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_SNOW3G_UIA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create SNOW 3G session */
	retval = create_wireless_algo_cipher_auth_session(ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_SNOW3G_UIA2,
			RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
			tdata->key.data, tdata->key.len,
			tdata->key.data, tdata->key.len,
			tdata->auth_iv.len, tdata->digest.len,
			tdata->cipher_iv.len);
	if (retval != 0)
		return retval;
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	debug_hexdump(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_cipher_hash_operation(tdata->digest.data,
			tdata->digest.len, tdata->auth_iv.data,
			tdata->auth_iv.len,
			plaintext_pad_len, RTE_CRYPTO_AUTH_OP_GENERATE,
			tdata->cipher_iv.data, tdata->cipher_iv.len,
			tdata->validCipherLenInBits.len,
			0,
			tdata->validAuthLenInBits.len,
			0
			);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_src;
	if (ut_params->obuf)
		ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
	else
		ciphertext = plaintext;

	debug_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			ciphertext,
			tdata->ciphertext.data,
			tdata->validDataLenInBits.len,
			"SNOW 3G Ciphertext data not as expected");

	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
	    + plaintext_pad_len;

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ut_params->digest,
			tdata->digest.data,
			DIGEST_BYTE_LENGTH_SNOW3G_UIA2,
			"SNOW 3G Generated auth tag not as expected");
	return 0;
}

static int
test_snow3g_auth_cipher(const struct snow3g_test_data *tdata,
	uint8_t op_mode, uint8_t verify)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext = NULL, *ciphertext = NULL;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;
	unsigned int ciphertext_pad_len;
	unsigned int ciphertext_len;

	struct rte_cryptodev_info dev_info;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_SNOW3G_UIA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;

	if (op_mode == OUT_OF_PLACE) {
		if (!(feat_flags & RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED)) {
			printf("Device doesn't support digest encrypted.\n");
			return TEST_SKIPPED;
		}
		if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
			return TEST_SKIPPED;
	}

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* Create SNOW 3G session */
	retval = create_wireless_algo_auth_cipher_session(
			ts_params->valid_devs[0],
			(verify ? RTE_CRYPTO_CIPHER_OP_DECRYPT
					: RTE_CRYPTO_CIPHER_OP_ENCRYPT),
			(verify ? RTE_CRYPTO_AUTH_OP_VERIFY
					: RTE_CRYPTO_AUTH_OP_GENERATE),
			RTE_CRYPTO_AUTH_SNOW3G_UIA2,
			RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
			tdata->key.data, tdata->key.len,
			tdata->key.data, tdata->key.len,
			tdata->auth_iv.len, tdata->digest.len,
			tdata->cipher_iv.len);
	if (retval != 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	if (op_mode == OUT_OF_PLACE)
		ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
		rte_pktmbuf_tailroom(ut_params->ibuf));
	if (op_mode == OUT_OF_PLACE)
		memset(rte_pktmbuf_mtod(ut_params->obuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->obuf));

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	if (verify) {
		ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
					ciphertext_pad_len);
		memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);
		if (op_mode == OUT_OF_PLACE)
			rte_pktmbuf_append(ut_params->obuf, ciphertext_pad_len);
		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
	} else {
		plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
					plaintext_pad_len);
		memcpy(plaintext, tdata->plaintext.data, plaintext_len);
		if (op_mode == OUT_OF_PLACE)
			rte_pktmbuf_append(ut_params->obuf, plaintext_pad_len);
		debug_hexdump(stdout, "plaintext:", plaintext, plaintext_len);
	}

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_auth_cipher_operation(
		tdata->digest.data, tdata->digest.len,
		tdata->cipher_iv.data, tdata->cipher_iv.len,
		tdata->auth_iv.data, tdata->auth_iv.len,
		(tdata->digest.offset_bytes == 0 ?
		(verify ? ciphertext_pad_len : plaintext_pad_len)
			: tdata->digest.offset_bytes),
		tdata->validCipherLenInBits.len,
		tdata->cipher.offset_bits,
		tdata->validAuthLenInBits.len,
		tdata->auth.offset_bits,
		op_mode, 0, verify);

	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = (op_mode == IN_PLACE ?
		ut_params->op->sym->m_src : ut_params->op->sym->m_dst);

	if (verify) {
		if (ut_params->obuf)
			plaintext = rte_pktmbuf_mtod(ut_params->obuf,
							uint8_t *);
		else
			plaintext = ciphertext +
				(tdata->cipher.offset_bits >> 3);

		debug_hexdump(stdout, "plaintext:", plaintext,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
		debug_hexdump(stdout, "plaintext expected:",
			tdata->plaintext.data,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
	} else {
		if (ut_params->obuf)
			ciphertext = rte_pktmbuf_mtod(ut_params->obuf,
							uint8_t *);
		else
			ciphertext = plaintext;

		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
		debug_hexdump(stdout, "ciphertext expected:",
			tdata->ciphertext.data, tdata->ciphertext.len >> 3);

		ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
			+ (tdata->digest.offset_bytes == 0 ?
		plaintext_pad_len : tdata->digest.offset_bytes);

		debug_hexdump(stdout, "digest:", ut_params->digest,
			tdata->digest.len);
		debug_hexdump(stdout, "digest expected:", tdata->digest.data,
				tdata->digest.len);
	}

	/* Validate obuf */
	if (verify) {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT_OFFSET(
			plaintext,
			tdata->plaintext.data,
			(tdata->plaintext.len - tdata->cipher.offset_bits -
			 (tdata->digest.len << 3)),
			tdata->cipher.offset_bits,
			"SNOW 3G Plaintext data not as expected");
	} else {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT_OFFSET(
			ciphertext,
			tdata->ciphertext.data,
			(tdata->validDataLenInBits.len -
			 tdata->cipher.offset_bits),
			tdata->cipher.offset_bits,
			"SNOW 3G Ciphertext data not as expected");

		TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ut_params->digest,
			tdata->digest.data,
			DIGEST_BYTE_LENGTH_SNOW3G_UIA2,
			"SNOW 3G Generated auth tag not as expected");
	}
	return 0;
}

static int
test_snow3g_auth_cipher_sgl(const struct snow3g_test_data *tdata,
	uint8_t op_mode, uint8_t verify)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	const uint8_t *plaintext = NULL;
	const uint8_t *ciphertext = NULL;
	const uint8_t *digest = NULL;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;
	unsigned int ciphertext_pad_len;
	unsigned int ciphertext_len;
	uint8_t buffer[10000];
	uint8_t digest_buffer[10000];

	struct rte_cryptodev_info dev_info;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_SNOW3G_UIA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;

	if (op_mode == IN_PLACE) {
		if (!(feat_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)) {
			printf("Device doesn't support in-place scatter-gather "
					"in both input and output mbufs.\n");
			return TEST_SKIPPED;
		}
		if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
			printf("Device doesn't support RAW data-path APIs.\n");
			return TEST_SKIPPED;
		}
	} else {
		if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
			return TEST_SKIPPED;
		if (!(feat_flags & RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT)) {
			printf("Device doesn't support out-of-place scatter-gather "
					"in both input and output mbufs.\n");
			return TEST_SKIPPED;
		}
		if (!(feat_flags & RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED)) {
			printf("Device doesn't support digest encrypted.\n");
			return TEST_SKIPPED;
		}
	}

	/* Create SNOW 3G session */
	retval = create_wireless_algo_auth_cipher_session(
			ts_params->valid_devs[0],
			(verify ? RTE_CRYPTO_CIPHER_OP_DECRYPT
					: RTE_CRYPTO_CIPHER_OP_ENCRYPT),
			(verify ? RTE_CRYPTO_AUTH_OP_VERIFY
					: RTE_CRYPTO_AUTH_OP_GENERATE),
			RTE_CRYPTO_AUTH_SNOW3G_UIA2,
			RTE_CRYPTO_CIPHER_SNOW3G_UEA2,
			tdata->key.data, tdata->key.len,
			tdata->key.data, tdata->key.len,
			tdata->auth_iv.len, tdata->digest.len,
			tdata->cipher_iv.len);

	if (retval != 0)
		return retval;

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 15, 0);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	if (op_mode == OUT_OF_PLACE) {
		ut_params->obuf = create_segmented_mbuf(ts_params->mbuf_pool,
				plaintext_pad_len, 15, 0);
		TEST_ASSERT_NOT_NULL(ut_params->obuf,
				"Failed to allocate output buffer in mempool");
	}

	if (verify) {
		pktmbuf_write(ut_params->ibuf, 0, ciphertext_len,
			tdata->ciphertext.data);
		ciphertext = rte_pktmbuf_read(ut_params->ibuf, 0,
					ciphertext_len, buffer);
		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
	} else {
		pktmbuf_write(ut_params->ibuf, 0, plaintext_len,
			tdata->plaintext.data);
		plaintext = rte_pktmbuf_read(ut_params->ibuf, 0,
					plaintext_len, buffer);
		debug_hexdump(stdout, "plaintext:", plaintext,
			plaintext_len);
	}
	memset(buffer, 0, sizeof(buffer));

	/* Create SNOW 3G operation */
	retval = create_wireless_algo_auth_cipher_operation(
		tdata->digest.data, tdata->digest.len,
		tdata->cipher_iv.data, tdata->cipher_iv.len,
		tdata->auth_iv.data, tdata->auth_iv.len,
		(tdata->digest.offset_bytes == 0 ?
		(verify ? ciphertext_pad_len : plaintext_pad_len)
			: tdata->digest.offset_bytes),
		tdata->validCipherLenInBits.len,
		tdata->cipher.offset_bits,
		tdata->validAuthLenInBits.len,
		tdata->auth.offset_bits,
		op_mode, 1, verify);

	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = (op_mode == IN_PLACE ?
		ut_params->op->sym->m_src : ut_params->op->sym->m_dst);

	if (verify) {
		if (ut_params->obuf)
			plaintext = rte_pktmbuf_read(ut_params->obuf, 0,
					plaintext_len, buffer);
		else
			plaintext = rte_pktmbuf_read(ut_params->ibuf, 0,
					plaintext_len, buffer);

		debug_hexdump(stdout, "plaintext:", plaintext,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
		debug_hexdump(stdout, "plaintext expected:",
			tdata->plaintext.data,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
	} else {
		if (ut_params->obuf)
			ciphertext = rte_pktmbuf_read(ut_params->obuf, 0,
					ciphertext_len, buffer);
		else
			ciphertext = rte_pktmbuf_read(ut_params->ibuf, 0,
					ciphertext_len, buffer);

		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
		debug_hexdump(stdout, "ciphertext expected:",
			tdata->ciphertext.data, tdata->ciphertext.len >> 3);

		if (ut_params->obuf)
			digest = rte_pktmbuf_read(ut_params->obuf,
				(tdata->digest.offset_bytes == 0 ?
				plaintext_pad_len : tdata->digest.offset_bytes),
				tdata->digest.len, digest_buffer);
		else
			digest = rte_pktmbuf_read(ut_params->ibuf,
				(tdata->digest.offset_bytes == 0 ?
				plaintext_pad_len : tdata->digest.offset_bytes),
				tdata->digest.len, digest_buffer);

		debug_hexdump(stdout, "digest:", digest,
			tdata->digest.len);
		debug_hexdump(stdout, "digest expected:",
			tdata->digest.data, tdata->digest.len);
	}

	/* Validate obuf */
	if (verify) {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT_OFFSET(
			plaintext,
			tdata->plaintext.data,
			(tdata->plaintext.len - tdata->cipher.offset_bits -
			 (tdata->digest.len << 3)),
			tdata->cipher.offset_bits,
			"SNOW 3G Plaintext data not as expected");
	} else {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT_OFFSET(
			ciphertext,
			tdata->ciphertext.data,
			(tdata->validDataLenInBits.len -
			 tdata->cipher.offset_bits),
			tdata->cipher.offset_bits,
			"SNOW 3G Ciphertext data not as expected");

		TEST_ASSERT_BUFFERS_ARE_EQUAL(
			digest,
			tdata->digest.data,
			DIGEST_BYTE_LENGTH_SNOW3G_UIA2,
			"SNOW 3G Generated auth tag not as expected");
	}
	return 0;
}

static int
test_kasumi_auth_cipher(const struct kasumi_test_data *tdata,
	uint8_t op_mode, uint8_t verify)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext = NULL, *ciphertext = NULL;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;
	unsigned int ciphertext_pad_len;
	unsigned int ciphertext_len;

	struct rte_cryptodev_info dev_info;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_KASUMI_F9;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_KASUMI_F8;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	if (op_mode == OUT_OF_PLACE) {
		if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
			return TEST_SKIPPED;
		if (!(feat_flags & RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED)) {
			printf("Device doesn't support digest encrypted.\n");
			return TEST_SKIPPED;
		}
	}

	/* Create KASUMI session */
	retval = create_wireless_algo_auth_cipher_session(
			ts_params->valid_devs[0],
			(verify ? RTE_CRYPTO_CIPHER_OP_DECRYPT
					: RTE_CRYPTO_CIPHER_OP_ENCRYPT),
			(verify ? RTE_CRYPTO_AUTH_OP_VERIFY
					: RTE_CRYPTO_AUTH_OP_GENERATE),
			RTE_CRYPTO_AUTH_KASUMI_F9,
			RTE_CRYPTO_CIPHER_KASUMI_F8,
			tdata->key.data, tdata->key.len,
			tdata->key.data, tdata->key.len,
			0, tdata->digest.len,
			tdata->cipher_iv.len);

	if (retval != 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	if (op_mode == OUT_OF_PLACE)
		ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
		rte_pktmbuf_tailroom(ut_params->ibuf));
	if (op_mode == OUT_OF_PLACE)
		memset(rte_pktmbuf_mtod(ut_params->obuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->obuf));

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	if (verify) {
		ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
					ciphertext_pad_len);
		memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);
		if (op_mode == OUT_OF_PLACE)
			rte_pktmbuf_append(ut_params->obuf, ciphertext_pad_len);
		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
	} else {
		plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
					plaintext_pad_len);
		memcpy(plaintext, tdata->plaintext.data, plaintext_len);
		if (op_mode == OUT_OF_PLACE)
			rte_pktmbuf_append(ut_params->obuf, plaintext_pad_len);
		debug_hexdump(stdout, "plaintext:", plaintext,
			plaintext_len);
	}

	/* Create KASUMI operation */
	retval = create_wireless_algo_auth_cipher_operation(
		tdata->digest.data, tdata->digest.len,
		tdata->cipher_iv.data, tdata->cipher_iv.len,
		NULL, 0,
		(tdata->digest.offset_bytes == 0 ?
		(verify ? ciphertext_pad_len : plaintext_pad_len)
			: tdata->digest.offset_bytes),
		tdata->validCipherLenInBits.len,
		tdata->validCipherOffsetInBits.len,
		tdata->validAuthLenInBits.len,
		0,
		op_mode, 0, verify);

	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = (op_mode == IN_PLACE ?
		ut_params->op->sym->m_src : ut_params->op->sym->m_dst);


	if (verify) {
		if (ut_params->obuf)
			plaintext = rte_pktmbuf_mtod(ut_params->obuf,
							uint8_t *);
		else
			plaintext = ciphertext;

		debug_hexdump(stdout, "plaintext:", plaintext,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
		debug_hexdump(stdout, "plaintext expected:",
			tdata->plaintext.data,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
	} else {
		if (ut_params->obuf)
			ciphertext = rte_pktmbuf_mtod(ut_params->obuf,
							uint8_t *);
		else
			ciphertext = plaintext;

		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
		debug_hexdump(stdout, "ciphertext expected:",
			tdata->ciphertext.data, tdata->ciphertext.len >> 3);

		ut_params->digest = rte_pktmbuf_mtod(
			ut_params->obuf, uint8_t *) +
			(tdata->digest.offset_bytes == 0 ?
			plaintext_pad_len : tdata->digest.offset_bytes);

		debug_hexdump(stdout, "digest:", ut_params->digest,
			tdata->digest.len);
		debug_hexdump(stdout, "digest expected:",
			tdata->digest.data, tdata->digest.len);
	}

	/* Validate obuf */
	if (verify) {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			plaintext,
			tdata->plaintext.data,
			tdata->plaintext.len >> 3,
			"KASUMI Plaintext data not as expected");
	} else {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			ciphertext,
			tdata->ciphertext.data,
			tdata->ciphertext.len >> 3,
			"KASUMI Ciphertext data not as expected");

		TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ut_params->digest,
			tdata->digest.data,
			DIGEST_BYTE_LENGTH_KASUMI_F9,
			"KASUMI Generated auth tag not as expected");
	}
	return 0;
}

static int
test_kasumi_auth_cipher_sgl(const struct kasumi_test_data *tdata,
	uint8_t op_mode, uint8_t verify)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	const uint8_t *plaintext = NULL;
	const uint8_t *ciphertext = NULL;
	const uint8_t *digest = NULL;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;
	unsigned int ciphertext_pad_len;
	unsigned int ciphertext_len;
	uint8_t buffer[10000];
	uint8_t digest_buffer[10000];

	struct rte_cryptodev_info dev_info;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_KASUMI_F9;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_KASUMI_F8;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;

	if (op_mode == IN_PLACE) {
		if (!(feat_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)) {
			printf("Device doesn't support in-place scatter-gather "
					"in both input and output mbufs.\n");
			return TEST_SKIPPED;
		}
		if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
			printf("Device doesn't support RAW data-path APIs.\n");
			return TEST_SKIPPED;
		}
	} else {
		if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
			return TEST_SKIPPED;
		if (!(feat_flags & RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT)) {
			printf("Device doesn't support out-of-place scatter-gather "
					"in both input and output mbufs.\n");
			return TEST_SKIPPED;
		}
		if (!(feat_flags & RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED)) {
			printf("Device doesn't support digest encrypted.\n");
			return TEST_SKIPPED;
		}
	}

	/* Create KASUMI session */
	retval = create_wireless_algo_auth_cipher_session(
			ts_params->valid_devs[0],
			(verify ? RTE_CRYPTO_CIPHER_OP_DECRYPT
					: RTE_CRYPTO_CIPHER_OP_ENCRYPT),
			(verify ? RTE_CRYPTO_AUTH_OP_VERIFY
					: RTE_CRYPTO_AUTH_OP_GENERATE),
			RTE_CRYPTO_AUTH_KASUMI_F9,
			RTE_CRYPTO_CIPHER_KASUMI_F8,
			tdata->key.data, tdata->key.len,
			tdata->key.data, tdata->key.len,
			0, tdata->digest.len,
			tdata->cipher_iv.len);

	if (retval != 0)
		return retval;

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 15, 0);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	if (op_mode == OUT_OF_PLACE) {
		ut_params->obuf = create_segmented_mbuf(ts_params->mbuf_pool,
				plaintext_pad_len, 15, 0);
		TEST_ASSERT_NOT_NULL(ut_params->obuf,
				"Failed to allocate output buffer in mempool");
	}

	if (verify) {
		pktmbuf_write(ut_params->ibuf, 0, ciphertext_len,
			tdata->ciphertext.data);
		ciphertext = rte_pktmbuf_read(ut_params->ibuf, 0,
					ciphertext_len, buffer);
		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
	} else {
		pktmbuf_write(ut_params->ibuf, 0, plaintext_len,
			tdata->plaintext.data);
		plaintext = rte_pktmbuf_read(ut_params->ibuf, 0,
					plaintext_len, buffer);
		debug_hexdump(stdout, "plaintext:", plaintext,
			plaintext_len);
	}
	memset(buffer, 0, sizeof(buffer));

	/* Create KASUMI operation */
	retval = create_wireless_algo_auth_cipher_operation(
		tdata->digest.data, tdata->digest.len,
		tdata->cipher_iv.data, tdata->cipher_iv.len,
		NULL, 0,
		(tdata->digest.offset_bytes == 0 ?
		(verify ? ciphertext_pad_len : plaintext_pad_len)
			: tdata->digest.offset_bytes),
		tdata->validCipherLenInBits.len,
		tdata->validCipherOffsetInBits.len,
		tdata->validAuthLenInBits.len,
		0,
		op_mode, 1, verify);

	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = (op_mode == IN_PLACE ?
		ut_params->op->sym->m_src : ut_params->op->sym->m_dst);

	if (verify) {
		if (ut_params->obuf)
			plaintext = rte_pktmbuf_read(ut_params->obuf, 0,
					plaintext_len, buffer);
		else
			plaintext = rte_pktmbuf_read(ut_params->ibuf, 0,
					plaintext_len, buffer);

		debug_hexdump(stdout, "plaintext:", plaintext,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
		debug_hexdump(stdout, "plaintext expected:",
			tdata->plaintext.data,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
	} else {
		if (ut_params->obuf)
			ciphertext = rte_pktmbuf_read(ut_params->obuf, 0,
					ciphertext_len, buffer);
		else
			ciphertext = rte_pktmbuf_read(ut_params->ibuf, 0,
					ciphertext_len, buffer);

		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
		debug_hexdump(stdout, "ciphertext expected:",
			tdata->ciphertext.data, tdata->ciphertext.len >> 3);

		if (ut_params->obuf)
			digest = rte_pktmbuf_read(ut_params->obuf,
				(tdata->digest.offset_bytes == 0 ?
				plaintext_pad_len : tdata->digest.offset_bytes),
				tdata->digest.len, digest_buffer);
		else
			digest = rte_pktmbuf_read(ut_params->ibuf,
				(tdata->digest.offset_bytes == 0 ?
				plaintext_pad_len : tdata->digest.offset_bytes),
				tdata->digest.len, digest_buffer);

		debug_hexdump(stdout, "digest:", digest,
			tdata->digest.len);
		debug_hexdump(stdout, "digest expected:",
			tdata->digest.data, tdata->digest.len);
	}

	/* Validate obuf */
	if (verify) {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			plaintext,
			tdata->plaintext.data,
			tdata->plaintext.len >> 3,
			"KASUMI Plaintext data not as expected");
	} else {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			ciphertext,
			tdata->ciphertext.data,
			tdata->validDataLenInBits.len,
			"KASUMI Ciphertext data not as expected");

		TEST_ASSERT_BUFFERS_ARE_EQUAL(
			digest,
			tdata->digest.data,
			DIGEST_BYTE_LENGTH_KASUMI_F9,
			"KASUMI Generated auth tag not as expected");
	}
	return 0;
}

static int
test_kasumi_cipher_auth(const struct kasumi_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext, *ciphertext;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_KASUMI_F9;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_KASUMI_F8;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create KASUMI session */
	retval = create_wireless_algo_cipher_auth_session(
			ts_params->valid_devs[0],
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			RTE_CRYPTO_AUTH_KASUMI_F9,
			RTE_CRYPTO_CIPHER_KASUMI_F8,
			tdata->key.data, tdata->key.len,
			tdata->key.data, tdata->key.len,
			0, tdata->digest.len,
			tdata->cipher_iv.len);
	if (retval != 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	debug_hexdump(stdout, "plaintext:", plaintext, plaintext_len);

	/* Create KASUMI operation */
	retval = create_wireless_algo_cipher_hash_operation(tdata->digest.data,
				tdata->digest.len, NULL, 0,
				plaintext_pad_len, RTE_CRYPTO_AUTH_OP_GENERATE,
				tdata->cipher_iv.data, tdata->cipher_iv.len,
				RTE_ALIGN_CEIL(tdata->validCipherLenInBits.len, 8),
				tdata->validCipherOffsetInBits.len,
				tdata->validAuthLenInBits.len,
				0
				);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	if (ut_params->op->sym->m_dst)
		ut_params->obuf = ut_params->op->sym->m_dst;
	else
		ut_params->obuf = ut_params->op->sym->m_src;

	ciphertext = rte_pktmbuf_mtod_offset(ut_params->obuf, uint8_t *,
				tdata->validCipherOffsetInBits.len >> 3);

	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
			+ plaintext_pad_len;

	const uint8_t *reference_ciphertext = tdata->ciphertext.data +
				(tdata->validCipherOffsetInBits.len >> 3);
	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
		ciphertext,
		reference_ciphertext,
		tdata->validCipherLenInBits.len,
		"KASUMI Ciphertext data not as expected");

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
		ut_params->digest,
		tdata->digest.data,
		DIGEST_BYTE_LENGTH_SNOW3G_UIA2,
		"KASUMI Generated auth tag not as expected");
	return 0;
}

static int
check_cipher_capability(const struct crypto_testsuite_params *ts_params,
			const enum rte_crypto_cipher_algorithm cipher_algo,
			const uint16_t key_size, const uint16_t iv_size)
{
	struct rte_cryptodev_sym_capability_idx cap_idx;
	const struct rte_cryptodev_symmetric_capability *cap;

	/* Check if device supports the algorithm */
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = cipher_algo;

	cap = rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx);

	if (cap == NULL)
		return -1;

	/* Check if device supports key size and IV size */
	if (rte_cryptodev_sym_capability_check_cipher(cap, key_size,
			iv_size) < 0) {
		return -1;
	}

	return 0;
}

static int
check_auth_capability(const struct crypto_testsuite_params *ts_params,
			const enum rte_crypto_auth_algorithm auth_algo,
			const uint16_t key_size, const uint16_t iv_size,
			const uint16_t tag_size)
{
	struct rte_cryptodev_sym_capability_idx cap_idx;
	const struct rte_cryptodev_symmetric_capability *cap;

	/* Check if device supports the algorithm */
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = auth_algo;

	cap = rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx);

	if (cap == NULL)
		return -1;

	/* Check if device supports key size and IV size */
	if (rte_cryptodev_sym_capability_check_auth(cap, key_size,
			tag_size, iv_size) < 0) {
		return -1;
	}

	return 0;
}

static int
test_zuc_cipher(const struct wireless_test_data *tdata,
		enum rte_crypto_cipher_operation direction)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext = NULL;
	uint8_t *ciphertext = NULL;
	unsigned int plaintext_pad_len, ciphertext_pad_len;
	unsigned int plaintext_len = 0;
	unsigned int ciphertext_len = 0;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Check if device supports ZUC EEA3 */
	if (check_cipher_capability(ts_params, RTE_CRYPTO_CIPHER_ZUC_EEA3,
			tdata->key.len, tdata->cipher_iv.len) < 0)
		return TEST_SKIPPED;

	/* Create ZUC session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
					direction,
					RTE_CRYPTO_CIPHER_ZUC_EEA3,
					tdata->key.data, tdata->key.len,
					tdata->cipher_iv.len);
	if (retval != 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* Clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	       rte_pktmbuf_tailroom(ut_params->ibuf));

	if (direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		plaintext_len = ceil_byte_length(tdata->plaintext.len);
		/* Append data which is padded to a multiple */
		/* of the algorithms block size */
		plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);
		plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
		memcpy(plaintext, tdata->plaintext.data, plaintext_len);

		debug_hexdump(stdout, "plaintext:", plaintext, plaintext_len);
	} else {
		ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
		/* Append data which is padded to a multiple */
		/* of the algorithms block size */
		ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 8);
		ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				ciphertext_pad_len);
		memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);

		debug_hexdump(stdout, "ciphertext:", ciphertext, ciphertext_len);
	}

	/* Create ZUC operation */
	retval = create_wireless_algo_cipher_operation(tdata->cipher_iv.data,
					tdata->cipher_iv.len,
					tdata->plaintext.len,
					tdata->validCipherOffsetInBits.len);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 0, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;

	if (direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		if (ut_params->obuf)
			ciphertext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
		else
			ciphertext = plaintext;

		debug_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);

		/* Validate obuf */
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
				ciphertext,
				tdata->ciphertext.data,
				tdata->validCipherLenInBits.len,
				"ZUC Ciphertext data not as expected");
	} else {
		if (ut_params->obuf)
			plaintext = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *);
		else
			plaintext = ciphertext;

		debug_hexdump(stdout, "plaintext:", plaintext, ciphertext_len);

		const uint8_t *reference_plaintext = tdata->plaintext.data +
				(tdata->validCipherOffsetInBits.len >> 3);

		/* Validate obuf */
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
				plaintext,
				reference_plaintext,
				tdata->validCipherLenInBits.len,
				"ZUC Plaintext data not as expected");
	}

	return 0;
}

static int
test_zuc_cipher_sgl(const struct wireless_test_data *tdata,
		enum rte_crypto_cipher_operation direction)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	unsigned int plaintext_pad_len, ciphertext_pad_len;
	unsigned int plaintext_len = 0;
	unsigned int ciphertext_len = 0;
	const uint8_t *ciphertext, *plaintext;
	uint8_t buffer[2048];
	struct rte_cryptodev_info dev_info;

	/* Check if device supports ZUC EEA3 */
	if (check_cipher_capability(ts_params, RTE_CRYPTO_CIPHER_ZUC_EEA3,
			tdata->key.len, tdata->cipher_iv.len) < 0)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)) {
		printf("Device doesn't support in-place scatter-gather. "
				"Test Skipped.\n");
		return TEST_SKIPPED;
	}

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		plaintext_len = ceil_byte_length(tdata->plaintext.len);

		/* Append data which is padded to a multiple */
		/* of the algorithms block size */
		plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);

		ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
				plaintext_pad_len, 10, 0);

		pktmbuf_write(ut_params->ibuf, 0, plaintext_len,
				tdata->plaintext.data);
	} else {
		ciphertext_len = ceil_byte_length(tdata->ciphertext.len);

		/* Append data which is padded to a multiple */
		/* of the algorithms block size */
		ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 8);

		ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
				ciphertext_pad_len, 10, 0);

		pktmbuf_write(ut_params->ibuf, 0, ciphertext_len,
				tdata->ciphertext.data);

	}

	/* Create ZUC session */
	retval = create_wireless_algo_cipher_session(ts_params->valid_devs[0],
			direction,
			RTE_CRYPTO_CIPHER_ZUC_EEA3,
			tdata->key.data, tdata->key.len,
			tdata->cipher_iv.len);
	if (retval < 0)
		return retval;

	/* Clear mbuf payload */
	if (direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		pktmbuf_write(ut_params->ibuf, 0, plaintext_len, tdata->plaintext.data);
	else
		pktmbuf_write(ut_params->ibuf, 0, ciphertext_len, tdata->ciphertext.data);

	/* Create ZUC operation */
	retval = create_wireless_algo_cipher_operation(tdata->cipher_iv.data,
			tdata->cipher_iv.len, tdata->plaintext.len,
			tdata->validCipherOffsetInBits.len);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 0, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
						ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = ut_params->op->sym->m_dst;

	if (direction == RTE_CRYPTO_CIPHER_OP_ENCRYPT) {
		if (ut_params->obuf)
			ciphertext = rte_pktmbuf_read(ut_params->obuf,
				0, plaintext_len, buffer);
		else
			ciphertext = rte_pktmbuf_read(ut_params->ibuf,
				0, plaintext_len, buffer);

		/* Validate obuf */
		debug_hexdump(stdout, "ciphertext:", ciphertext, plaintext_len);

		/* Validate obuf */
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			ciphertext,
			tdata->ciphertext.data,
			tdata->validCipherLenInBits.len,
			"ZUC Ciphertext data not as expected");
	} else {
		if (ut_params->obuf)
			plaintext = rte_pktmbuf_read(ut_params->obuf,
				0, ciphertext_len, buffer);
		else
			plaintext = rte_pktmbuf_read(ut_params->ibuf,
				0, ciphertext_len, buffer);

		/* Validate obuf */
		debug_hexdump(stdout, "plaintext:", plaintext, ciphertext_len);

		/* Validate obuf */
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			plaintext,
			tdata->plaintext.data,
			tdata->validCipherLenInBits.len,
			"ZUC Plaintext data not as expected");
		}

	return 0;
}

static int
test_zuc_authentication(const struct wireless_test_data *tdata,
		enum rte_crypto_auth_operation auth_op)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	unsigned plaintext_pad_len;
	unsigned plaintext_len;
	uint8_t *plaintext;

	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_NON_BYTE_ALIGNED_DATA) &&
			(tdata->validAuthLenInBits.len % 8 != 0)) {
		printf("Device doesn't support NON-Byte Aligned Data.\n");
		return TEST_SKIPPED;
	}

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Check if device supports ZUC EIA3 */
	if (check_auth_capability(ts_params, RTE_CRYPTO_AUTH_ZUC_EIA3,
			tdata->key.len, tdata->auth_iv.len,
			tdata->digest.len) < 0)
		return TEST_SKIPPED;

	/* Create ZUC session */
	retval = create_wireless_algo_hash_session(ts_params->valid_devs[0],
			tdata->key.data, tdata->key.len,
			tdata->auth_iv.len, tdata->digest.len,
			auth_op, RTE_CRYPTO_AUTH_ZUC_EIA3);
	if (retval != 0)
		return retval;

	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
	rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	/* Append data which is padded to a multiple of */
	/* the algorithms block size */
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 8);
	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	/* Create ZUC operation */
	retval = create_wireless_algo_hash_operation(tdata->digest.data,
			tdata->digest.len,
			tdata->auth_iv.data, tdata->auth_iv.len,
			plaintext_pad_len,
			auth_op, tdata->validAuthLenInBits.len, 0);
	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 1, 1,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
				ut_params->op);
	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");
	ut_params->obuf = ut_params->op->sym->m_src;
	ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
			+ plaintext_pad_len;

	if (auth_op != RTE_CRYPTO_AUTH_OP_VERIFY) {
		/* Validate obuf */
		TEST_ASSERT_BUFFERS_ARE_EQUAL(
				ut_params->digest,
				tdata->digest.data,
				tdata->digest.len,
				"ZUC Generated auth tag not as expected");
		return 0;
	}

	/* Validate obuf */
	if (ut_params->op->status == RTE_CRYPTO_OP_STATUS_SUCCESS)
		return 0;
	else
		return -1;

	return 0;
}

static int
test_zuc_auth_cipher(const struct wireless_test_data *tdata,
	uint8_t op_mode, uint8_t verify)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext = NULL, *ciphertext = NULL;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;
	unsigned int ciphertext_pad_len;
	unsigned int ciphertext_len;

	struct rte_cryptodev_info dev_info;

	/* Check if device supports ZUC EEA3 */
	if (check_cipher_capability(ts_params, RTE_CRYPTO_CIPHER_ZUC_EEA3,
			tdata->key.len, tdata->cipher_iv.len) < 0)
		return TEST_SKIPPED;

	/* Check if device supports ZUC EIA3 */
	if (check_auth_capability(ts_params, RTE_CRYPTO_AUTH_ZUC_EIA3,
			tdata->key.len, tdata->auth_iv.len,
			tdata->digest.len) < 0)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED)) {
		printf("Device doesn't support digest encrypted.\n");
		return TEST_SKIPPED;
	}
	if (op_mode == IN_PLACE) {
		if (!(feat_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)) {
			printf("Device doesn't support in-place scatter-gather "
					"in both input and output mbufs.\n");
			return TEST_SKIPPED;
		}

		if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
			printf("Device doesn't support RAW data-path APIs.\n");
			return TEST_SKIPPED;
		}
	} else {
		if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
			return TEST_SKIPPED;
		if (!(feat_flags & RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT)) {
			printf("Device doesn't support out-of-place scatter-gather "
					"in both input and output mbufs.\n");
			return TEST_SKIPPED;
		}
	}

	/* Create ZUC session */
	retval = create_wireless_algo_auth_cipher_session(
			ts_params->valid_devs[0],
			(verify ? RTE_CRYPTO_CIPHER_OP_DECRYPT
					: RTE_CRYPTO_CIPHER_OP_ENCRYPT),
			(verify ? RTE_CRYPTO_AUTH_OP_VERIFY
					: RTE_CRYPTO_AUTH_OP_GENERATE),
			RTE_CRYPTO_AUTH_ZUC_EIA3,
			RTE_CRYPTO_CIPHER_ZUC_EEA3,
			tdata->key.data, tdata->key.len,
			tdata->key.data, tdata->key.len,
			tdata->auth_iv.len, tdata->digest.len,
			tdata->cipher_iv.len);

	if (retval != 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	if (op_mode == OUT_OF_PLACE)
		ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
		rte_pktmbuf_tailroom(ut_params->ibuf));
	if (op_mode == OUT_OF_PLACE)
		memset(rte_pktmbuf_mtod(ut_params->obuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->obuf));

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	if (verify) {
		ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
					ciphertext_pad_len);
		memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);
		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
	} else {
		/* make sure enough space to cover partial digest verify case */
		plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
					ciphertext_pad_len);
		memcpy(plaintext, tdata->plaintext.data, plaintext_len);
		debug_hexdump(stdout, "plaintext:", plaintext,
			plaintext_len);
	}

	if (op_mode == OUT_OF_PLACE)
		rte_pktmbuf_append(ut_params->obuf, ciphertext_pad_len);

	/* Create ZUC operation */
	retval = create_wireless_algo_auth_cipher_operation(
		tdata->digest.data, tdata->digest.len,
		tdata->cipher_iv.data, tdata->cipher_iv.len,
		tdata->auth_iv.data, tdata->auth_iv.len,
		(tdata->digest.offset_bytes == 0 ?
		(verify ? ciphertext_pad_len : plaintext_pad_len)
			: tdata->digest.offset_bytes),
		tdata->validCipherLenInBits.len,
		tdata->validCipherOffsetInBits.len,
		tdata->validAuthLenInBits.len,
		0,
		op_mode, 0, verify);

	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = (op_mode == IN_PLACE ?
		ut_params->op->sym->m_src : ut_params->op->sym->m_dst);


	if (verify) {
		if (ut_params->obuf)
			plaintext = rte_pktmbuf_mtod(ut_params->obuf,
							uint8_t *);
		else
			plaintext = ciphertext;

		debug_hexdump(stdout, "plaintext:", plaintext,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
		debug_hexdump(stdout, "plaintext expected:",
			tdata->plaintext.data,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
	} else {
		if (ut_params->obuf)
			ciphertext = rte_pktmbuf_mtod(ut_params->obuf,
							uint8_t *);
		else
			ciphertext = plaintext;

		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
		debug_hexdump(stdout, "ciphertext expected:",
			tdata->ciphertext.data, tdata->ciphertext.len >> 3);

		ut_params->digest = rte_pktmbuf_mtod(
			ut_params->obuf, uint8_t *) +
			(tdata->digest.offset_bytes == 0 ?
			plaintext_pad_len : tdata->digest.offset_bytes);

		debug_hexdump(stdout, "digest:", ut_params->digest,
			tdata->digest.len);
		debug_hexdump(stdout, "digest expected:",
			tdata->digest.data, tdata->digest.len);
	}

	/* Validate obuf */
	if (verify) {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			plaintext,
			tdata->plaintext.data,
			tdata->plaintext.len >> 3,
			"ZUC Plaintext data not as expected");
	} else {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			ciphertext,
			tdata->ciphertext.data,
			tdata->ciphertext.len >> 3,
			"ZUC Ciphertext data not as expected");

		TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ut_params->digest,
			tdata->digest.data,
			tdata->digest.len,
			"ZUC Generated auth tag not as expected");
	}
	return 0;
}

static int
test_zuc_auth_cipher_sgl(const struct wireless_test_data *tdata,
	uint8_t op_mode, uint8_t verify)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	const uint8_t *plaintext = NULL;
	const uint8_t *ciphertext = NULL;
	const uint8_t *digest = NULL;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;
	unsigned int ciphertext_pad_len;
	unsigned int ciphertext_len;
	uint8_t buffer[10000];
	uint8_t digest_buffer[10000];

	struct rte_cryptodev_info dev_info;

	/* Check if device supports ZUC EEA3 */
	if (check_cipher_capability(ts_params, RTE_CRYPTO_CIPHER_ZUC_EEA3,
			tdata->key.len, tdata->cipher_iv.len) < 0)
		return TEST_SKIPPED;

	/* Check if device supports ZUC EIA3 */
	if (check_auth_capability(ts_params, RTE_CRYPTO_AUTH_ZUC_EIA3,
			tdata->key.len, tdata->auth_iv.len,
			tdata->digest.len) < 0)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;

	if (op_mode == IN_PLACE) {
		if (!(feat_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)) {
			printf("Device doesn't support in-place scatter-gather "
					"in both input and output mbufs.\n");
			return TEST_SKIPPED;
		}

		if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
			printf("Device doesn't support RAW data-path APIs.\n");
			return TEST_SKIPPED;
		}
	} else {
		if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
			return TEST_SKIPPED;
		if (!(feat_flags & RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT)) {
			printf("Device doesn't support out-of-place scatter-gather "
					"in both input and output mbufs.\n");
			return TEST_SKIPPED;
		}
		if (!(feat_flags & RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED)) {
			printf("Device doesn't support digest encrypted.\n");
			return TEST_SKIPPED;
		}
	}

	/* Create ZUC session */
	retval = create_wireless_algo_auth_cipher_session(
			ts_params->valid_devs[0],
			(verify ? RTE_CRYPTO_CIPHER_OP_DECRYPT
					: RTE_CRYPTO_CIPHER_OP_ENCRYPT),
			(verify ? RTE_CRYPTO_AUTH_OP_VERIFY
					: RTE_CRYPTO_AUTH_OP_GENERATE),
			RTE_CRYPTO_AUTH_ZUC_EIA3,
			RTE_CRYPTO_CIPHER_ZUC_EEA3,
			tdata->key.data, tdata->key.len,
			tdata->key.data, tdata->key.len,
			tdata->auth_iv.len, tdata->digest.len,
			tdata->cipher_iv.len);

	if (retval != 0)
		return retval;

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len);
	plaintext_len = ceil_byte_length(tdata->plaintext.len);
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
			plaintext_pad_len, 15, 0);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	if (op_mode == OUT_OF_PLACE) {
		ut_params->obuf = create_segmented_mbuf(ts_params->mbuf_pool,
				plaintext_pad_len, 15, 0);
		TEST_ASSERT_NOT_NULL(ut_params->obuf,
				"Failed to allocate output buffer in mempool");
	}

	if (verify) {
		pktmbuf_write(ut_params->ibuf, 0, ciphertext_len,
			tdata->ciphertext.data);
		ciphertext = rte_pktmbuf_read(ut_params->ibuf, 0,
					ciphertext_len, buffer);
		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
	} else {
		pktmbuf_write(ut_params->ibuf, 0, plaintext_len,
			tdata->plaintext.data);
		plaintext = rte_pktmbuf_read(ut_params->ibuf, 0,
					plaintext_len, buffer);
		debug_hexdump(stdout, "plaintext:", plaintext,
			plaintext_len);
	}
	memset(buffer, 0, sizeof(buffer));

	/* Create ZUC operation */
	retval = create_wireless_algo_auth_cipher_operation(
		tdata->digest.data, tdata->digest.len,
		tdata->cipher_iv.data, tdata->cipher_iv.len,
		tdata->auth_iv.data, tdata->auth_iv.len,
		(tdata->digest.offset_bytes == 0 ?
		(verify ? ciphertext_pad_len : plaintext_pad_len)
			: tdata->digest.offset_bytes),
		tdata->validCipherLenInBits.len,
		tdata->validCipherOffsetInBits.len,
		tdata->validAuthLenInBits.len,
		0,
		op_mode, 1, verify);

	if (retval < 0)
		return retval;

	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 1,
					       tdata->cipher_iv.len);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = (op_mode == IN_PLACE ?
		ut_params->op->sym->m_src : ut_params->op->sym->m_dst);

	if (verify) {
		if (ut_params->obuf)
			plaintext = rte_pktmbuf_read(ut_params->obuf, 0,
					plaintext_len, buffer);
		else
			plaintext = rte_pktmbuf_read(ut_params->ibuf, 0,
					plaintext_len, buffer);

		debug_hexdump(stdout, "plaintext:", plaintext,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
		debug_hexdump(stdout, "plaintext expected:",
			tdata->plaintext.data,
			(tdata->plaintext.len >> 3) - tdata->digest.len);
	} else {
		if (ut_params->obuf)
			ciphertext = rte_pktmbuf_read(ut_params->obuf, 0,
					ciphertext_len, buffer);
		else
			ciphertext = rte_pktmbuf_read(ut_params->ibuf, 0,
					ciphertext_len, buffer);

		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
		debug_hexdump(stdout, "ciphertext expected:",
			tdata->ciphertext.data, tdata->ciphertext.len >> 3);

		if (ut_params->obuf)
			digest = rte_pktmbuf_read(ut_params->obuf,
				(tdata->digest.offset_bytes == 0 ?
				plaintext_pad_len : tdata->digest.offset_bytes),
				tdata->digest.len, digest_buffer);
		else
			digest = rte_pktmbuf_read(ut_params->ibuf,
				(tdata->digest.offset_bytes == 0 ?
				plaintext_pad_len : tdata->digest.offset_bytes),
				tdata->digest.len, digest_buffer);

		debug_hexdump(stdout, "digest:", digest,
			tdata->digest.len);
		debug_hexdump(stdout, "digest expected:",
			tdata->digest.data, tdata->digest.len);
	}

	/* Validate obuf */
	if (verify) {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			plaintext,
			tdata->plaintext.data,
			tdata->plaintext.len >> 3,
			"ZUC Plaintext data not as expected");
	} else {
		TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
			ciphertext,
			tdata->ciphertext.data,
			tdata->validDataLenInBits.len,
			"ZUC Ciphertext data not as expected");

		TEST_ASSERT_BUFFERS_ARE_EQUAL(
			digest,
			tdata->digest.data,
			tdata->digest.len,
			"ZUC Generated auth tag not as expected");
	}
	return 0;
}

static int
test_kasumi_encryption_test_case_1(void)
{
	return test_kasumi_encryption(&kasumi_test_case_1);
}

static int
test_kasumi_encryption_test_case_1_sgl(void)
{
	return test_kasumi_encryption_sgl(&kasumi_test_case_1);
}

static int
test_kasumi_encryption_test_case_1_oop(void)
{
	return test_kasumi_encryption_oop(&kasumi_test_case_1);
}

static int
test_kasumi_encryption_test_case_1_oop_sgl(void)
{
	return test_kasumi_encryption_oop_sgl(&kasumi_test_case_1);
}

static int
test_kasumi_encryption_test_case_2(void)
{
	return test_kasumi_encryption(&kasumi_test_case_2);
}

static int
test_kasumi_encryption_test_case_3(void)
{
	return test_kasumi_encryption(&kasumi_test_case_3);
}

static int
test_kasumi_encryption_test_case_4(void)
{
	return test_kasumi_encryption(&kasumi_test_case_4);
}

static int
test_kasumi_encryption_test_case_5(void)
{
	return test_kasumi_encryption(&kasumi_test_case_5);
}

static int
test_kasumi_decryption_test_case_1(void)
{
	return test_kasumi_decryption(&kasumi_test_case_1);
}

static int
test_kasumi_decryption_test_case_1_oop(void)
{
	return test_kasumi_decryption_oop(&kasumi_test_case_1);
}

static int
test_kasumi_decryption_test_case_2(void)
{
	return test_kasumi_decryption(&kasumi_test_case_2);
}

static int
test_kasumi_decryption_test_case_3(void)
{
	/* rte_crypto_mbuf_to_vec does not support incomplete mbuf build */
	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;
	return test_kasumi_decryption(&kasumi_test_case_3);
}

static int
test_kasumi_decryption_test_case_4(void)
{
	return test_kasumi_decryption(&kasumi_test_case_4);
}

static int
test_kasumi_decryption_test_case_5(void)
{
	return test_kasumi_decryption(&kasumi_test_case_5);
}
static int
test_snow3g_encryption_test_case_1(void)
{
	return test_snow3g_encryption(&snow3g_test_case_1);
}

static int
test_snow3g_encryption_test_case_1_oop(void)
{
	return test_snow3g_encryption_oop(&snow3g_test_case_1);
}

static int
test_snow3g_encryption_test_case_1_oop_sgl(void)
{
	return test_snow3g_encryption_oop_sgl(&snow3g_test_case_1, 1, 1);
}

static int
test_snow3g_encryption_test_case_1_oop_lb_in_sgl_out(void)
{
	return test_snow3g_encryption_oop_sgl(&snow3g_test_case_1, 0, 1);
}

static int
test_snow3g_encryption_test_case_1_oop_sgl_in_lb_out(void)
{
	return test_snow3g_encryption_oop_sgl(&snow3g_test_case_1, 1, 0);
}

static int
test_snow3g_encryption_test_case_1_offset_oop(void)
{
	return test_snow3g_encryption_offset_oop(&snow3g_test_case_1);
}

static int
test_snow3g_encryption_test_case_2(void)
{
	return test_snow3g_encryption(&snow3g_test_case_2);
}

static int
test_snow3g_encryption_test_case_3(void)
{
	return test_snow3g_encryption(&snow3g_test_case_3);
}

static int
test_snow3g_encryption_test_case_4(void)
{
	return test_snow3g_encryption(&snow3g_test_case_4);
}

static int
test_snow3g_encryption_test_case_5(void)
{
	return test_snow3g_encryption(&snow3g_test_case_5);
}

static int
test_snow3g_decryption_test_case_1(void)
{
	return test_snow3g_decryption(&snow3g_test_case_1);
}

static int
test_snow3g_decryption_test_case_1_oop(void)
{
	return test_snow3g_decryption_oop(&snow3g_test_case_1);
}

static int
test_snow3g_decryption_test_case_2(void)
{
	return test_snow3g_decryption(&snow3g_test_case_2);
}

static int
test_snow3g_decryption_test_case_3(void)
{
	return test_snow3g_decryption(&snow3g_test_case_3);
}

static int
test_snow3g_decryption_test_case_4(void)
{
	return test_snow3g_decryption(&snow3g_test_case_4);
}

static int
test_snow3g_decryption_test_case_5(void)
{
	return test_snow3g_decryption(&snow3g_test_case_5);
}

/*
 * Function prepares snow3g_hash_test_data from snow3g_test_data.
 * Pattern digest from snow3g_test_data must be allocated as
 * 4 last bytes in plaintext.
 */
static void
snow3g_hash_test_vector_setup(const struct snow3g_test_data *pattern,
		struct snow3g_hash_test_data *output)
{
	if ((pattern != NULL) && (output != NULL)) {
		output->key.len = pattern->key.len;

		memcpy(output->key.data,
		pattern->key.data, pattern->key.len);

		output->auth_iv.len = pattern->auth_iv.len;

		memcpy(output->auth_iv.data,
		pattern->auth_iv.data, pattern->auth_iv.len);

		output->plaintext.len = pattern->plaintext.len;

		memcpy(output->plaintext.data,
		pattern->plaintext.data, pattern->plaintext.len >> 3);

		output->digest.len = pattern->digest.len;

		memcpy(output->digest.data,
		&pattern->plaintext.data[pattern->digest.offset_bytes],
		pattern->digest.len);

		output->validAuthLenInBits.len =
		pattern->validAuthLenInBits.len;
	}
}

/*
 * Test case verify computed cipher and digest from snow3g_test_case_7 data.
 */
static int
test_snow3g_decryption_with_digest_test_case_1(void)
{
	int ret;
	struct snow3g_hash_test_data snow3g_hash_data;
	struct rte_cryptodev_info dev_info;
	struct crypto_testsuite_params *ts_params = &testsuite_params;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED)) {
		printf("Device doesn't support encrypted digest operations.\n");
		return TEST_SKIPPED;
	}

	/*
	 * Function prepare data for hash verification test case.
	 * Digest is allocated in 4 last bytes in plaintext, pattern.
	 */
	snow3g_hash_test_vector_setup(&snow3g_test_case_7, &snow3g_hash_data);

	ret = test_snow3g_decryption(&snow3g_test_case_7);
	if (ret != 0)
		return ret;

	return test_snow3g_authentication_verify(&snow3g_hash_data);
}

static int
test_snow3g_cipher_auth_test_case_1(void)
{
	return test_snow3g_cipher_auth(&snow3g_test_case_3);
}

static int
test_snow3g_auth_cipher_test_case_1(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_test_case_1, IN_PLACE, 0);
}

static int
test_snow3g_auth_cipher_test_case_2(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_test_case_2, IN_PLACE, 0);
}

static int
test_snow3g_auth_cipher_test_case_2_oop(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_test_case_2, OUT_OF_PLACE, 0);
}

static int
test_snow3g_auth_cipher_part_digest_enc(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_partial_digest_encryption,
			IN_PLACE, 0);
}

static int
test_snow3g_auth_cipher_part_digest_enc_oop(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_partial_digest_encryption,
			OUT_OF_PLACE, 0);
}

static int
test_snow3g_auth_cipher_test_case_3_sgl(void)
{
	/* rte_crypto_mbuf_to_vec does not support incomplete mbuf build */
	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_test_case_3, IN_PLACE, 0);
}

static int
test_snow3g_auth_cipher_test_case_3_oop_sgl(void)
{
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_test_case_3, OUT_OF_PLACE, 0);
}

static int
test_snow3g_auth_cipher_part_digest_enc_sgl(void)
{
	/* rte_crypto_mbuf_to_vec does not support incomplete mbuf build */
	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_partial_digest_encryption,
			IN_PLACE, 0);
}

static int
test_snow3g_auth_cipher_part_digest_enc_oop_sgl(void)
{
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_partial_digest_encryption,
			OUT_OF_PLACE, 0);
}

static int
test_snow3g_auth_cipher_total_digest_enc_1(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_total_digest_encryption_1, IN_PLACE, 0);
}

static int
test_snow3g_auth_cipher_total_digest_enc_1_oop(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_total_digest_encryption_1, OUT_OF_PLACE, 0);
}

static int
test_snow3g_auth_cipher_total_digest_enc_1_sgl(void)
{
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_total_digest_encryption_1, IN_PLACE, 0);
}

static int
test_snow3g_auth_cipher_total_digest_enc_1_oop_sgl(void)
{
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_total_digest_encryption_1, OUT_OF_PLACE, 0);
}

static int
test_snow3g_auth_cipher_verify_test_case_1(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_test_case_1, IN_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_test_case_2(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_test_case_2, IN_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_test_case_2_oop(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_test_case_2, OUT_OF_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_part_digest_enc(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_partial_digest_encryption,
			IN_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_part_digest_enc_oop(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_partial_digest_encryption,
			OUT_OF_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_test_case_3_sgl(void)
{
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_test_case_3, IN_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_test_case_3_oop_sgl(void)
{
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_test_case_3, OUT_OF_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_part_digest_enc_sgl(void)
{
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_partial_digest_encryption,
			IN_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_part_digest_enc_oop_sgl(void)
{
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_partial_digest_encryption,
			OUT_OF_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_total_digest_enc_1(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_total_digest_encryption_1, IN_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_total_digest_enc_1_oop(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_auth_cipher_total_digest_encryption_1, OUT_OF_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_total_digest_enc_1_sgl(void)
{
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_total_digest_encryption_1, IN_PLACE, 1);
}

static int
test_snow3g_auth_cipher_verify_total_digest_enc_1_oop_sgl(void)
{
	return test_snow3g_auth_cipher_sgl(
		&snow3g_auth_cipher_total_digest_encryption_1, OUT_OF_PLACE, 1);
}

static int
test_snow3g_auth_cipher_with_digest_test_case_1(void)
{
	return test_snow3g_auth_cipher(
		&snow3g_test_case_7, IN_PLACE, 0);
}

static int
test_kasumi_auth_cipher_test_case_1(void)
{
	return test_kasumi_auth_cipher(
		&kasumi_test_case_3, IN_PLACE, 0);
}

static int
test_kasumi_auth_cipher_test_case_2(void)
{
	return test_kasumi_auth_cipher(
		&kasumi_auth_cipher_test_case_2, IN_PLACE, 0);
}

static int
test_kasumi_auth_cipher_test_case_2_oop(void)
{
	return test_kasumi_auth_cipher(
		&kasumi_auth_cipher_test_case_2, OUT_OF_PLACE, 0);
}

static int
test_kasumi_auth_cipher_test_case_2_sgl(void)
{
	return test_kasumi_auth_cipher_sgl(
		&kasumi_auth_cipher_test_case_2, IN_PLACE, 0);
}

static int
test_kasumi_auth_cipher_test_case_2_oop_sgl(void)
{
	return test_kasumi_auth_cipher_sgl(
		&kasumi_auth_cipher_test_case_2, OUT_OF_PLACE, 0);
}

static int
test_kasumi_auth_cipher_verify_test_case_1(void)
{
	return test_kasumi_auth_cipher(
		&kasumi_test_case_3, IN_PLACE, 1);
}

static int
test_kasumi_auth_cipher_verify_test_case_2(void)
{
	return test_kasumi_auth_cipher(
		&kasumi_auth_cipher_test_case_2, IN_PLACE, 1);
}

static int
test_kasumi_auth_cipher_verify_test_case_2_oop(void)
{
	return test_kasumi_auth_cipher(
		&kasumi_auth_cipher_test_case_2, OUT_OF_PLACE, 1);
}

static int
test_kasumi_auth_cipher_verify_test_case_2_sgl(void)
{
	return test_kasumi_auth_cipher_sgl(
		&kasumi_auth_cipher_test_case_2, IN_PLACE, 1);
}

static int
test_kasumi_auth_cipher_verify_test_case_2_oop_sgl(void)
{
	return test_kasumi_auth_cipher_sgl(
		&kasumi_auth_cipher_test_case_2, OUT_OF_PLACE, 1);
}

static int
test_kasumi_cipher_auth_test_case_1(void)
{
	return test_kasumi_cipher_auth(&kasumi_test_case_6);
}

static int
test_zuc_encryption_test_case_1(void)
{
	return test_zuc_cipher(&zuc_test_case_cipher_193b,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT);
}

static int
test_zuc_encryption_test_case_2(void)
{
	return test_zuc_cipher(&zuc_test_case_cipher_800b,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT);
}

static int
test_zuc_encryption_test_case_3(void)
{
	return test_zuc_cipher(&zuc_test_case_cipher_1570b,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT);
}

static int
test_zuc_encryption_test_case_4(void)
{
	return test_zuc_cipher(&zuc_test_case_cipher_2798b,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT);
}

static int
test_zuc_encryption_test_case_5(void)
{
	return test_zuc_cipher(&zuc_test_case_cipher_4019b,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT);
}

static int
test_zuc_encryption_test_case_6_sgl(void)
{
	return test_zuc_cipher_sgl(&zuc_test_case_cipher_193b,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT);
}

static int
test_zuc_decryption_test_case_1(void)
{
	return test_zuc_cipher(&zuc_test_case_cipher_193b,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
}

static int
test_zuc_decryption_test_case_2(void)
{
	return test_zuc_cipher(&zuc_test_case_cipher_800b,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
}

static int
test_zuc_decryption_test_case_3(void)
{
	return test_zuc_cipher(&zuc_test_case_cipher_1570b,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
}

static int
test_zuc_decryption_test_case_4(void)
{
	return test_zuc_cipher(&zuc_test_case_cipher_2798b,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
}

static int
test_zuc_decryption_test_case_5(void)
{
	return test_zuc_cipher(&zuc_test_case_cipher_4019b,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
}

static int
test_zuc_decryption_test_case_6_sgl(void)
{
	return test_zuc_cipher_sgl(&zuc_test_case_cipher_193b,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
}

static int
test_zuc_hash_generate_test_case_1(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_1b,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc_hash_generate_test_case_2(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_90b,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc_hash_generate_test_case_3(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_577b,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc_hash_generate_test_case_4(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_2079b,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc_hash_generate_test_case_5(void)
{
	return test_zuc_authentication(&zuc_test_auth_5670b,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc_hash_generate_test_case_6(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_128b,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc_hash_generate_test_case_7(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_2080b,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc_hash_generate_test_case_8(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_584b,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc_hash_verify_test_case_1(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_1b,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc_hash_verify_test_case_2(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_90b,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc_hash_verify_test_case_3(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_577b,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc_hash_verify_test_case_4(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_2079b,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc_hash_verify_test_case_5(void)
{
	return test_zuc_authentication(&zuc_test_auth_5670b,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc_hash_verify_test_case_6(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_128b,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc_hash_verify_test_case_7(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_2080b,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc_hash_verify_test_case_8(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_584b,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc_cipher_auth_test_case_1(void)
{
	return test_zuc_cipher_auth(&zuc_test_case_cipher_200b_auth_200b);
}

static int
test_zuc_cipher_auth_test_case_2(void)
{
	return test_zuc_cipher_auth(&zuc_test_case_cipher_800b_auth_120b);
}

static int
test_zuc_auth_cipher_test_case_1(void)
{
	return test_zuc_auth_cipher(
		&zuc_auth_cipher_test_case_1, IN_PLACE, 0);
}

static int
test_zuc_auth_cipher_test_case_1_oop(void)
{
	return test_zuc_auth_cipher(
		&zuc_auth_cipher_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_zuc_auth_cipher_test_case_1_sgl(void)
{
	return test_zuc_auth_cipher_sgl(
		&zuc_auth_cipher_test_case_1, IN_PLACE, 0);
}

static int
test_zuc_auth_cipher_test_case_1_oop_sgl(void)
{
	return test_zuc_auth_cipher_sgl(
		&zuc_auth_cipher_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_zuc_auth_cipher_test_case_2(void)
{
	return test_zuc_auth_cipher(
		&zuc_auth_cipher_test_case_2, IN_PLACE, 0);
}

static int
test_zuc_auth_cipher_test_case_2_oop(void)
{
	return test_zuc_auth_cipher(
		&zuc_auth_cipher_test_case_2, OUT_OF_PLACE, 0);
}

static int
test_zuc_auth_cipher_verify_test_case_1(void)
{
	return test_zuc_auth_cipher(
		&zuc_auth_cipher_test_case_1, IN_PLACE, 1);
}

static int
test_zuc_auth_cipher_verify_test_case_1_oop(void)
{
	return test_zuc_auth_cipher(
		&zuc_auth_cipher_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_zuc_auth_cipher_verify_test_case_1_sgl(void)
{
	return test_zuc_auth_cipher_sgl(
		&zuc_auth_cipher_test_case_1, IN_PLACE, 1);
}

static int
test_zuc_auth_cipher_verify_test_case_1_oop_sgl(void)
{
	return test_zuc_auth_cipher_sgl(
		&zuc_auth_cipher_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_zuc_auth_cipher_verify_test_case_2(void)
{
	return test_zuc_auth_cipher(
		&zuc_auth_cipher_test_case_2, IN_PLACE, 1);
}

static int
test_zuc_auth_cipher_verify_test_case_2_oop(void)
{
	return test_zuc_auth_cipher(
		&zuc_auth_cipher_test_case_2, OUT_OF_PLACE, 1);
}

static int
test_zuc256_encryption_test_case_1(void)
{
	return test_zuc_cipher(&zuc256_test_case_cipher_1,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT);
}

static int
test_zuc256_encryption_test_case_2(void)
{
	return test_zuc_cipher(&zuc256_test_case_cipher_2,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT);
}

static int
test_zuc256_decryption_test_case_1(void)
{
	return test_zuc_cipher(&zuc256_test_case_cipher_1,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
}

static int
test_zuc256_decryption_test_case_2(void)
{
	return test_zuc_cipher(&zuc256_test_case_cipher_2,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
}

static int
test_zuc256_hash_generate_4b_tag_test_case_1(void)
{
	return test_zuc_authentication(&zuc256_test_case_auth_1,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc256_hash_generate_4b_tag_test_case_2(void)
{
	return test_zuc_authentication(&zuc256_test_case_auth_2,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc256_hash_generate_4b_tag_test_case_3(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_4000b_mac_32b,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc256_hash_generate_8b_tag_test_case_1(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_4000b_mac_64b,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc256_hash_generate_16b_tag_test_case_1(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_4000b_mac_128b,
			RTE_CRYPTO_AUTH_OP_GENERATE);
}

static int
test_zuc256_hash_verify_4b_tag_test_case_1(void)
{
	return test_zuc_authentication(&zuc256_test_case_auth_1,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc256_hash_verify_4b_tag_test_case_2(void)
{
	return test_zuc_authentication(&zuc256_test_case_auth_2,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc256_hash_verify_4b_tag_test_case_3(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_4000b_mac_32b,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc256_hash_verify_8b_tag_test_case_1(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_4000b_mac_64b,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc256_hash_verify_16b_tag_test_case_1(void)
{
	return test_zuc_authentication(&zuc_test_case_auth_4000b_mac_128b,
			RTE_CRYPTO_AUTH_OP_VERIFY);
}

static int
test_zuc256_cipher_auth_4b_tag_test_case_1(void)
{
	return test_zuc_cipher_auth(&zuc256_test_case_cipher_auth_1);
}

static int
test_zuc256_cipher_auth_4b_tag_test_case_2(void)
{
	return test_zuc_cipher_auth(&zuc256_test_case_cipher_auth_2);
}

static int
test_zuc256_cipher_auth_8b_tag_test_case_1(void)
{
	return test_zuc_cipher_auth(&zuc256_test_case_cipher_auth_3);
}

static int
test_zuc256_cipher_auth_16b_tag_test_case_1(void)
{
	return test_zuc_cipher_auth(&zuc256_test_case_cipher_auth_4);
}

static int
test_zuc256_auth_cipher_4b_tag_test_case_1(void)
{
	return test_zuc_auth_cipher(
		&zuc256_auth_cipher_test_case_1, IN_PLACE, 0);
}

static int
test_zuc256_auth_cipher_4b_tag_test_case_2(void)
{
	return test_zuc_auth_cipher(
		&zuc256_auth_cipher_test_case_2, IN_PLACE, 0);
}

static int
test_zuc256_auth_cipher_8b_tag_test_case_1(void)
{
	return test_zuc_auth_cipher(
		&zuc256_auth_cipher_test_case_3, IN_PLACE, 0);
}

static int
test_zuc256_auth_cipher_16b_tag_test_case_1(void)
{
	return test_zuc_auth_cipher(
		&zuc256_auth_cipher_test_case_4, IN_PLACE, 0);
}

static int
test_zuc256_auth_cipher_verify_4b_tag_test_case_1(void)
{
	return test_zuc_auth_cipher(
		&zuc256_auth_cipher_test_case_1, IN_PLACE, 1);
}

static int
test_zuc256_auth_cipher_verify_4b_tag_test_case_2(void)
{
	return test_zuc_auth_cipher(
		&zuc256_auth_cipher_test_case_2, IN_PLACE, 1);
}

static int
test_zuc256_auth_cipher_verify_8b_tag_test_case_1(void)
{
	return test_zuc_auth_cipher(
		&zuc256_auth_cipher_test_case_3, IN_PLACE, 1);
}

static int
test_zuc256_auth_cipher_verify_16b_tag_test_case_1(void)
{
	return test_zuc_auth_cipher(
		&zuc256_auth_cipher_test_case_4, IN_PLACE, 1);
}

static int
test_mixed_check_if_unsupported(const struct mixed_cipher_auth_test_data *tdata)
{
	uint8_t dev_id = testsuite_params.valid_devs[0];

	struct rte_cryptodev_sym_capability_idx cap_idx;

	/* Check if device supports particular cipher algorithm */
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = tdata->cipher_algo;
	if (rte_cryptodev_sym_capability_get(dev_id, &cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Check if device supports particular hash algorithm */
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = tdata->auth_algo;
	if (rte_cryptodev_sym_capability_get(dev_id, &cap_idx) == NULL)
		return TEST_SKIPPED;

	return 0;
}

static int
test_mixed_auth_cipher(const struct mixed_cipher_auth_test_data *tdata,
	uint8_t op_mode, uint8_t verify)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	uint8_t *plaintext = NULL, *ciphertext = NULL;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;
	unsigned int ciphertext_pad_len;
	unsigned int ciphertext_len;

	struct rte_cryptodev_info dev_info;
	struct rte_crypto_op *op;

	/* Check if device supports particular algorithms separately */
	if (test_mixed_check_if_unsupported(tdata))
		return TEST_SKIPPED;
	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED)) {
		printf("Device doesn't support digest encrypted.\n");
		return TEST_SKIPPED;
	}

	/* Create the session */
	if (verify)
		retval = create_wireless_algo_cipher_auth_session(
				ts_params->valid_devs[0],
				RTE_CRYPTO_CIPHER_OP_DECRYPT,
				RTE_CRYPTO_AUTH_OP_VERIFY,
				tdata->auth_algo,
				tdata->cipher_algo,
				tdata->auth_key.data, tdata->auth_key.len,
				tdata->cipher_key.data, tdata->cipher_key.len,
				tdata->auth_iv.len, tdata->digest_enc.len,
				tdata->cipher_iv.len);
	else
		retval = create_wireless_algo_auth_cipher_session(
				ts_params->valid_devs[0],
				RTE_CRYPTO_CIPHER_OP_ENCRYPT,
				RTE_CRYPTO_AUTH_OP_GENERATE,
				tdata->auth_algo,
				tdata->cipher_algo,
				tdata->auth_key.data, tdata->auth_key.len,
				tdata->cipher_key.data, tdata->cipher_key.len,
				tdata->auth_iv.len, tdata->digest_enc.len,
				tdata->cipher_iv.len);
	if (retval != 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	if (op_mode == OUT_OF_PLACE)
		ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
		rte_pktmbuf_tailroom(ut_params->ibuf));
	if (op_mode == OUT_OF_PLACE) {

		memset(rte_pktmbuf_mtod(ut_params->obuf, uint8_t *), 0,
				rte_pktmbuf_tailroom(ut_params->obuf));
	}

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len_bits);
	plaintext_len = ceil_byte_length(tdata->plaintext.len_bits);
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	if (verify) {
		ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				ciphertext_pad_len);
		memcpy(ciphertext, tdata->ciphertext.data, ciphertext_len);
		debug_hexdump(stdout, "ciphertext:", ciphertext,
				ciphertext_len);
	} else {
		/* make sure enough space to cover partial digest verify case */
		plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				ciphertext_pad_len);
		memcpy(plaintext, tdata->plaintext.data, plaintext_len);
		debug_hexdump(stdout, "plaintext:", plaintext, plaintext_len);
	}

	if (op_mode == OUT_OF_PLACE)
		rte_pktmbuf_append(ut_params->obuf, ciphertext_pad_len);

	/* Create the operation */
	retval = create_wireless_algo_auth_cipher_operation(
			tdata->digest_enc.data, tdata->digest_enc.len,
			tdata->cipher_iv.data, tdata->cipher_iv.len,
			tdata->auth_iv.data, tdata->auth_iv.len,
			(tdata->digest_enc.offset == 0 ?
				plaintext_pad_len
				: tdata->digest_enc.offset),
			tdata->validCipherLen.len_bits,
			tdata->cipher.offset_bits,
			tdata->validAuthLen.len_bits,
			tdata->auth.offset_bits,
			op_mode, 0, verify);

	if (retval < 0)
		return retval;

	op = process_crypto_request(ts_params->valid_devs[0], ut_params->op);

	/* Check if the op failed because the device doesn't */
	/* support this particular combination of algorithms */
	if (op == NULL && ut_params->op->status ==
			RTE_CRYPTO_OP_STATUS_INVALID_SESSION) {
		printf("Device doesn't support this mixed combination. "
				"Test Skipped.\n");
		return TEST_SKIPPED;
	}
	ut_params->op = op;

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = (op_mode == IN_PLACE ?
			ut_params->op->sym->m_src : ut_params->op->sym->m_dst);

	if (verify) {
		if (ut_params->obuf)
			plaintext = rte_pktmbuf_mtod(ut_params->obuf,
							uint8_t *);
		else
			plaintext = ciphertext +
					(tdata->cipher.offset_bits >> 3);

		debug_hexdump(stdout, "plaintext:", plaintext,
				tdata->plaintext.len_bits >> 3);
		debug_hexdump(stdout, "plaintext expected:",
				tdata->plaintext.data,
				tdata->plaintext.len_bits >> 3);
	} else {
		if (ut_params->obuf)
			ciphertext = rte_pktmbuf_mtod(ut_params->obuf,
					uint8_t *);
		else
			ciphertext = plaintext;

		debug_hexdump(stdout, "ciphertext:", ciphertext,
				ciphertext_len);
		debug_hexdump(stdout, "ciphertext expected:",
				tdata->ciphertext.data,
				tdata->ciphertext.len_bits >> 3);

		ut_params->digest = rte_pktmbuf_mtod(ut_params->obuf, uint8_t *)
				+ (tdata->digest_enc.offset == 0 ?
		plaintext_pad_len : tdata->digest_enc.offset);

		debug_hexdump(stdout, "digest:", ut_params->digest,
				tdata->digest_enc.len);
		debug_hexdump(stdout, "digest expected:",
				tdata->digest_enc.data,
				tdata->digest_enc.len);
	}

	if (!verify) {
		TEST_ASSERT_BUFFERS_ARE_EQUAL(
				ut_params->digest,
				tdata->digest_enc.data,
				tdata->digest_enc.len,
				"Generated auth tag not as expected");
	}

	if (tdata->cipher_algo != RTE_CRYPTO_CIPHER_NULL) {
		if (verify) {
			TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
					plaintext,
					tdata->plaintext.data,
					tdata->plaintext.len_bits >> 3,
					"Plaintext data not as expected");
		} else {
			TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
					ciphertext,
					tdata->ciphertext.data,
					tdata->validDataLen.len_bits,
					"Ciphertext data not as expected");
		}
	}

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	return 0;
}

static int
test_mixed_auth_cipher_sgl(const struct mixed_cipher_auth_test_data *tdata,
	uint8_t op_mode, uint8_t verify)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;

	const uint8_t *plaintext = NULL;
	const uint8_t *ciphertext = NULL;
	const uint8_t *digest = NULL;
	unsigned int plaintext_pad_len;
	unsigned int plaintext_len;
	unsigned int ciphertext_pad_len;
	unsigned int ciphertext_len;
	uint8_t buffer[10000];
	uint8_t digest_buffer[10000];

	struct rte_cryptodev_info dev_info;
	struct rte_crypto_op *op;

	/* Check if device supports particular algorithms */
	if (test_mixed_check_if_unsupported(tdata))
		return TEST_SKIPPED;
	if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	uint64_t feat_flags = dev_info.feature_flags;

	if (op_mode == IN_PLACE) {
		if (!(feat_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)) {
			printf("Device doesn't support in-place scatter-gather "
					"in both input and output mbufs.\n");
			return TEST_SKIPPED;
		}
	} else {
		if (!(feat_flags & RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT)) {
			printf("Device doesn't support out-of-place scatter-gather "
					"in both input and output mbufs.\n");
			return TEST_SKIPPED;
		}
		if (!(feat_flags & RTE_CRYPTODEV_FF_DIGEST_ENCRYPTED)) {
			printf("Device doesn't support digest encrypted.\n");
			return TEST_SKIPPED;
		}
	}

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Create the session */
	if (verify)
		retval = create_wireless_algo_cipher_auth_session(
				ts_params->valid_devs[0],
				RTE_CRYPTO_CIPHER_OP_DECRYPT,
				RTE_CRYPTO_AUTH_OP_VERIFY,
				tdata->auth_algo,
				tdata->cipher_algo,
				tdata->auth_key.data, tdata->auth_key.len,
				tdata->cipher_key.data, tdata->cipher_key.len,
				tdata->auth_iv.len, tdata->digest_enc.len,
				tdata->cipher_iv.len);
	else
		retval = create_wireless_algo_auth_cipher_session(
				ts_params->valid_devs[0],
				RTE_CRYPTO_CIPHER_OP_ENCRYPT,
				RTE_CRYPTO_AUTH_OP_GENERATE,
				tdata->auth_algo,
				tdata->cipher_algo,
				tdata->auth_key.data, tdata->auth_key.len,
				tdata->cipher_key.data, tdata->cipher_key.len,
				tdata->auth_iv.len, tdata->digest_enc.len,
				tdata->cipher_iv.len);
	if (retval != 0)
		return retval;

	ciphertext_len = ceil_byte_length(tdata->ciphertext.len_bits);
	plaintext_len = ceil_byte_length(tdata->plaintext.len_bits);
	ciphertext_pad_len = RTE_ALIGN_CEIL(ciphertext_len, 16);
	plaintext_pad_len = RTE_ALIGN_CEIL(plaintext_len, 16);

	ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
			ciphertext_pad_len, 15, 0);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	if (op_mode == OUT_OF_PLACE) {
		ut_params->obuf = create_segmented_mbuf(ts_params->mbuf_pool,
				plaintext_pad_len, 15, 0);
		TEST_ASSERT_NOT_NULL(ut_params->obuf,
				"Failed to allocate output buffer in mempool");
	}

	if (verify) {
		pktmbuf_write(ut_params->ibuf, 0, ciphertext_len,
			tdata->ciphertext.data);
		ciphertext = rte_pktmbuf_read(ut_params->ibuf, 0,
					ciphertext_len, buffer);
		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
	} else {
		pktmbuf_write(ut_params->ibuf, 0, plaintext_len,
			tdata->plaintext.data);
		plaintext = rte_pktmbuf_read(ut_params->ibuf, 0,
					plaintext_len, buffer);
		debug_hexdump(stdout, "plaintext:", plaintext,
			plaintext_len);
	}
	memset(buffer, 0, sizeof(buffer));

	/* Create the operation */
	retval = create_wireless_algo_auth_cipher_operation(
			tdata->digest_enc.data, tdata->digest_enc.len,
			tdata->cipher_iv.data, tdata->cipher_iv.len,
			tdata->auth_iv.data, tdata->auth_iv.len,
			(tdata->digest_enc.offset == 0 ?
				plaintext_pad_len
				: tdata->digest_enc.offset),
			tdata->validCipherLen.len_bits,
			tdata->cipher.offset_bits,
			tdata->validAuthLen.len_bits,
			tdata->auth.offset_bits,
			op_mode, 1, verify);

	if (retval < 0)
		return retval;

	op = process_crypto_request(ts_params->valid_devs[0], ut_params->op);

	/* Check if the op failed because the device doesn't */
	/* support this particular combination of algorithms */
	if (op == NULL && ut_params->op->status ==
			RTE_CRYPTO_OP_STATUS_INVALID_SESSION) {
		printf("Device doesn't support this mixed combination. "
				"Test Skipped.\n");
		return TEST_SKIPPED;
	}
	ut_params->op = op;

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed to retrieve obuf");

	ut_params->obuf = (op_mode == IN_PLACE ?
			ut_params->op->sym->m_src : ut_params->op->sym->m_dst);

	if (verify) {
		if (ut_params->obuf)
			plaintext = rte_pktmbuf_read(ut_params->obuf, 0,
					plaintext_len, buffer);
		else
			plaintext = rte_pktmbuf_read(ut_params->ibuf, 0,
					plaintext_len, buffer);

		debug_hexdump(stdout, "plaintext:", plaintext,
				(tdata->plaintext.len_bits >> 3) -
				tdata->digest_enc.len);
		debug_hexdump(stdout, "plaintext expected:",
				tdata->plaintext.data,
				(tdata->plaintext.len_bits >> 3) -
				tdata->digest_enc.len);
	} else {
		if (ut_params->obuf)
			ciphertext = rte_pktmbuf_read(ut_params->obuf, 0,
					ciphertext_len, buffer);
		else
			ciphertext = rte_pktmbuf_read(ut_params->ibuf, 0,
					ciphertext_len, buffer);

		debug_hexdump(stdout, "ciphertext:", ciphertext,
			ciphertext_len);
		debug_hexdump(stdout, "ciphertext expected:",
			tdata->ciphertext.data,
			tdata->ciphertext.len_bits >> 3);

		if (ut_params->obuf)
			digest = rte_pktmbuf_read(ut_params->obuf,
					(tdata->digest_enc.offset == 0 ?
						plaintext_pad_len :
						tdata->digest_enc.offset),
					tdata->digest_enc.len, digest_buffer);
		else
			digest = rte_pktmbuf_read(ut_params->ibuf,
					(tdata->digest_enc.offset == 0 ?
						plaintext_pad_len :
						tdata->digest_enc.offset),
					tdata->digest_enc.len, digest_buffer);

		debug_hexdump(stdout, "digest:", digest,
				tdata->digest_enc.len);
		debug_hexdump(stdout, "digest expected:",
				tdata->digest_enc.data, tdata->digest_enc.len);
	}

	if (!verify) {
		TEST_ASSERT_BUFFERS_ARE_EQUAL(
				digest,
				tdata->digest_enc.data,
				tdata->digest_enc.len,
				"Generated auth tag not as expected");
	}

	if (tdata->cipher_algo != RTE_CRYPTO_CIPHER_NULL) {
		if (verify) {
			TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
					plaintext,
					tdata->plaintext.data,
					tdata->plaintext.len_bits >> 3,
					"Plaintext data not as expected");
		} else {
			TEST_ASSERT_BUFFERS_ARE_EQUAL_BIT(
					ciphertext,
					tdata->ciphertext.data,
					tdata->validDataLen.len_bits,
					"Ciphertext data not as expected");
		}
	}

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	return 0;
}

/** AUTH AES CMAC + CIPHER AES CTR */

static int
test_aes_cmac_aes_ctr_digest_enc_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_aes_ctr_test_case_1, IN_PLACE, 0);
}

static int
test_aes_cmac_aes_ctr_digest_enc_test_case_1_oop(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_aes_ctr_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_aes_cmac_aes_ctr_digest_enc_test_case_1_sgl(void)
{
	return test_mixed_auth_cipher_sgl(
		&auth_aes_cmac_cipher_aes_ctr_test_case_1, IN_PLACE, 0);
}

static int
test_aes_cmac_aes_ctr_digest_enc_test_case_1_oop_sgl(void)
{
	return test_mixed_auth_cipher_sgl(
		&auth_aes_cmac_cipher_aes_ctr_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_aes_cmac_aes_ctr_digest_enc_test_case_2(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_aes_ctr_test_case_2, IN_PLACE, 0);
}

static int
test_aes_cmac_aes_ctr_digest_enc_test_case_2_oop(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_aes_ctr_test_case_2, OUT_OF_PLACE, 0);
}

static int
test_verify_aes_cmac_aes_ctr_digest_enc_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_aes_ctr_test_case_1, IN_PLACE, 1);
}

static int
test_verify_aes_cmac_aes_ctr_digest_enc_test_case_2(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_aes_ctr_test_case_2, IN_PLACE, 1);
}

static int
test_verify_aes_cmac_aes_ctr_digest_enc_test_case_1_oop(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_aes_ctr_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_verify_aes_cmac_aes_ctr_digest_enc_test_case_1_sgl(void)
{
	return test_mixed_auth_cipher_sgl(
		&auth_aes_cmac_cipher_aes_ctr_test_case_1, IN_PLACE, 1);
}

static int
test_verify_aes_cmac_aes_ctr_digest_enc_test_case_1_oop_sgl(void)
{
	return test_mixed_auth_cipher_sgl(
		&auth_aes_cmac_cipher_aes_ctr_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_verify_aes_cmac_aes_ctr_digest_enc_test_case_2_oop(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_aes_ctr_test_case_2, OUT_OF_PLACE, 1);
}

/** MIXED AUTH + CIPHER */

static int
test_auth_zuc_cipher_snow_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_zuc_cipher_snow_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_zuc_cipher_snow_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_zuc_cipher_snow_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_auth_zuc_cipher_snow_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_zuc_cipher_snow_test_case_1, IN_PLACE, 0);
}

static int
test_verify_auth_zuc_cipher_snow_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_zuc_cipher_snow_test_case_1, IN_PLACE, 1);
}


static int
test_auth_aes_cmac_cipher_snow_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_snow_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_aes_cmac_cipher_snow_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_snow_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_auth_aes_cmac_cipher_snow_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_snow_test_case_1, IN_PLACE, 0);
}

static int
test_verify_auth_aes_cmac_cipher_snow_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_snow_test_case_1, IN_PLACE, 1);
}

static int
test_auth_zuc_cipher_aes_ctr_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_zuc_cipher_aes_ctr_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_zuc_cipher_aes_ctr_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_zuc_cipher_aes_ctr_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_auth_zuc_cipher_aes_ctr_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_zuc_cipher_aes_ctr_test_case_1, IN_PLACE, 0);
}

static int
test_verify_auth_zuc_cipher_aes_ctr_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_zuc_cipher_aes_ctr_test_case_1, IN_PLACE, 1);
}

static int
test_auth_snow_cipher_aes_ctr_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_snow_cipher_aes_ctr_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_snow_cipher_aes_ctr_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_snow_cipher_aes_ctr_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_auth_snow_cipher_aes_ctr_test_case_1_inplace_sgl(void)
{
	return test_mixed_auth_cipher_sgl(
		&auth_snow_cipher_aes_ctr_test_case_1, IN_PLACE, 0);
}

static int
test_auth_snow_cipher_aes_ctr_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_snow_cipher_aes_ctr_test_case_1, IN_PLACE, 0);
}

static int
test_verify_auth_snow_cipher_aes_ctr_test_case_1_inplace_sgl(void)
{
	return test_mixed_auth_cipher_sgl(
		&auth_snow_cipher_aes_ctr_test_case_1, IN_PLACE, 1);
}

static int
test_verify_auth_snow_cipher_aes_ctr_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_snow_cipher_aes_ctr_test_case_1, IN_PLACE, 1);
}

static int
test_auth_snow_cipher_zuc_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_snow_cipher_zuc_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_snow_cipher_zuc_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_snow_cipher_zuc_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_auth_snow_cipher_zuc_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_snow_cipher_zuc_test_case_1, IN_PLACE, 0);
}

static int
test_verify_auth_snow_cipher_zuc_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_snow_cipher_zuc_test_case_1, IN_PLACE, 1);
}

static int
test_auth_aes_cmac_cipher_zuc_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_zuc_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_aes_cmac_cipher_zuc_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_zuc_test_case_1, OUT_OF_PLACE, 1);
}
static int
test_auth_aes_cmac_cipher_zuc_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_zuc_test_case_1, IN_PLACE, 0);
}

static int
test_verify_auth_aes_cmac_cipher_zuc_test_case_1_inplace(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_zuc_test_case_1, IN_PLACE, 1);
}

static int
test_auth_null_cipher_snow_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_null_cipher_snow_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_null_cipher_snow_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_null_cipher_snow_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_auth_null_cipher_zuc_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_null_cipher_zuc_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_null_cipher_zuc_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_null_cipher_zuc_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_auth_snow_cipher_null_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_snow_cipher_null_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_snow_cipher_null_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_snow_cipher_null_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_auth_zuc_cipher_null_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_zuc_cipher_null_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_zuc_cipher_null_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_zuc_cipher_null_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_auth_null_cipher_aes_ctr_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_null_cipher_aes_ctr_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_null_cipher_aes_ctr_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_null_cipher_aes_ctr_test_case_1, OUT_OF_PLACE, 1);
}

static int
test_auth_aes_cmac_cipher_null_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_null_test_case_1, OUT_OF_PLACE, 0);
}

static int
test_verify_auth_aes_cmac_cipher_null_test_case_1(void)
{
	return test_mixed_auth_cipher(
		&auth_aes_cmac_cipher_null_test_case_1, OUT_OF_PLACE, 1);
}

/* ***** AEAD algorithm Tests ***** */

static int
create_aead_session(uint8_t dev_id, enum rte_crypto_aead_algorithm algo,
		enum rte_crypto_aead_operation op,
		const uint8_t *key, const uint8_t key_len,
		const uint16_t aad_len, const uint8_t auth_len,
		uint8_t iv_len)
{
	uint8_t aead_key[key_len];

	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	memcpy(aead_key, key, key_len);

	/* Setup AEAD Parameters */
	ut_params->aead_xform.type = RTE_CRYPTO_SYM_XFORM_AEAD;
	ut_params->aead_xform.next = NULL;
	ut_params->aead_xform.aead.algo = algo;
	ut_params->aead_xform.aead.op = op;
	ut_params->aead_xform.aead.key.data = aead_key;
	ut_params->aead_xform.aead.key.length = key_len;
	ut_params->aead_xform.aead.iv.offset = IV_OFFSET;
	ut_params->aead_xform.aead.iv.length = iv_len;
	ut_params->aead_xform.aead.digest_length = auth_len;
	ut_params->aead_xform.aead.aad_length = aad_len;

	debug_hexdump(stdout, "key:", key, key_len);

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
			&ut_params->aead_xform, ts_params->session_mpool);
	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");
	return 0;
}

static int
create_aead_xform(struct rte_crypto_op *op,
		enum rte_crypto_aead_algorithm algo,
		enum rte_crypto_aead_operation aead_op,
		uint8_t *key, const uint8_t key_len,
		const uint8_t aad_len, const uint8_t auth_len,
		uint8_t iv_len)
{
	TEST_ASSERT_NOT_NULL(rte_crypto_op_sym_xforms_alloc(op, 1),
			"failed to allocate space for crypto transform");

	struct rte_crypto_sym_op *sym_op = op->sym;

	/* Setup AEAD Parameters */
	sym_op->xform->type = RTE_CRYPTO_SYM_XFORM_AEAD;
	sym_op->xform->next = NULL;
	sym_op->xform->aead.algo = algo;
	sym_op->xform->aead.op = aead_op;
	sym_op->xform->aead.key.data = key;
	sym_op->xform->aead.key.length = key_len;
	sym_op->xform->aead.iv.offset = IV_OFFSET;
	sym_op->xform->aead.iv.length = iv_len;
	sym_op->xform->aead.digest_length = auth_len;
	sym_op->xform->aead.aad_length = aad_len;

	debug_hexdump(stdout, "key:", key, key_len);

	return 0;
}

static int
create_aead_operation(enum rte_crypto_aead_operation op,
		const struct aead_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	uint8_t *plaintext, *ciphertext;
	unsigned int aad_pad_len, plaintext_pad_len;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* Append aad data */
	if (tdata->algo == RTE_CRYPTO_AEAD_AES_CCM) {
		aad_pad_len = RTE_ALIGN_CEIL(tdata->aad.len + 18, 16);
		sym_op->aead.aad.data = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				aad_pad_len);
		TEST_ASSERT_NOT_NULL(sym_op->aead.aad.data,
				"no room to append aad");

		sym_op->aead.aad.phys_addr =
				rte_pktmbuf_iova(ut_params->ibuf);
		/* Copy AAD 18 bytes after the AAD pointer, according to the API */
		memcpy(sym_op->aead.aad.data + 18, tdata->aad.data, tdata->aad.len);
		debug_hexdump(stdout, "aad:", sym_op->aead.aad.data + 18,
			tdata->aad.len);

		/* Append IV at the end of the crypto operation*/
		uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ut_params->op,
				uint8_t *, IV_OFFSET);

		/* Copy IV 1 byte after the IV pointer, according to the API */
		rte_memcpy(iv_ptr + 1, tdata->iv.data, tdata->iv.len);
		debug_hexdump(stdout, "iv:", iv_ptr + 1,
			tdata->iv.len);
	} else {
		aad_pad_len = RTE_ALIGN_CEIL(tdata->aad.len, 16);
		sym_op->aead.aad.data = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				aad_pad_len);
		TEST_ASSERT_NOT_NULL(sym_op->aead.aad.data,
				"no room to append aad");

		sym_op->aead.aad.phys_addr =
				rte_pktmbuf_iova(ut_params->ibuf);
		memcpy(sym_op->aead.aad.data, tdata->aad.data, tdata->aad.len);
		debug_hexdump(stdout, "aad:", sym_op->aead.aad.data,
			tdata->aad.len);

		/* Append IV at the end of the crypto operation*/
		uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ut_params->op,
				uint8_t *, IV_OFFSET);

		if (tdata->iv.len == 0) {
			rte_memcpy(iv_ptr, tdata->iv.data, AES_GCM_J0_LENGTH);
			debug_hexdump(stdout, "iv:", iv_ptr,
				AES_GCM_J0_LENGTH);
		} else {
			rte_memcpy(iv_ptr, tdata->iv.data, tdata->iv.len);
			debug_hexdump(stdout, "iv:", iv_ptr,
				tdata->iv.len);
		}
	}

	/* Append plaintext/ciphertext */
	if (op == RTE_CRYPTO_AEAD_OP_ENCRYPT) {
		plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);
		plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
		TEST_ASSERT_NOT_NULL(plaintext, "no room to append plaintext");

		memcpy(plaintext, tdata->plaintext.data, tdata->plaintext.len);
		debug_hexdump(stdout, "plaintext:", plaintext,
				tdata->plaintext.len);

		if (ut_params->obuf) {
			ciphertext = (uint8_t *)rte_pktmbuf_append(
					ut_params->obuf,
					plaintext_pad_len + aad_pad_len);
			TEST_ASSERT_NOT_NULL(ciphertext,
					"no room to append ciphertext");

			memset(ciphertext + aad_pad_len, 0,
					tdata->ciphertext.len);
		}
	} else {
		plaintext_pad_len = RTE_ALIGN_CEIL(tdata->ciphertext.len, 16);
		ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
		TEST_ASSERT_NOT_NULL(ciphertext,
				"no room to append ciphertext");

		memcpy(ciphertext, tdata->ciphertext.data,
				tdata->ciphertext.len);
		debug_hexdump(stdout, "ciphertext:", ciphertext,
				tdata->ciphertext.len);

		if (ut_params->obuf) {
			plaintext = (uint8_t *)rte_pktmbuf_append(
					ut_params->obuf,
					plaintext_pad_len + aad_pad_len);
			TEST_ASSERT_NOT_NULL(plaintext,
					"no room to append plaintext");

			memset(plaintext + aad_pad_len, 0,
					tdata->plaintext.len);
		}
	}

	/* Append digest data */
	if (op == RTE_CRYPTO_AEAD_OP_ENCRYPT) {
		sym_op->aead.digest.data = (uint8_t *)rte_pktmbuf_append(
				ut_params->obuf ? ut_params->obuf :
						ut_params->ibuf,
						tdata->auth_tag.len);
		TEST_ASSERT_NOT_NULL(sym_op->aead.digest.data,
				"no room to append digest");
		memset(sym_op->aead.digest.data, 0, tdata->auth_tag.len);
		sym_op->aead.digest.phys_addr = rte_pktmbuf_iova_offset(
				ut_params->obuf ? ut_params->obuf :
						ut_params->ibuf,
						plaintext_pad_len +
						aad_pad_len);
	} else {
		sym_op->aead.digest.data = (uint8_t *)rte_pktmbuf_append(
				ut_params->ibuf, tdata->auth_tag.len);
		TEST_ASSERT_NOT_NULL(sym_op->aead.digest.data,
				"no room to append digest");
		sym_op->aead.digest.phys_addr = rte_pktmbuf_iova_offset(
				ut_params->ibuf,
				plaintext_pad_len + aad_pad_len);

		rte_memcpy(sym_op->aead.digest.data, tdata->auth_tag.data,
			tdata->auth_tag.len);
		debug_hexdump(stdout, "digest:",
			sym_op->aead.digest.data,
			tdata->auth_tag.len);
	}

	sym_op->aead.data.length = tdata->plaintext.len;
	sym_op->aead.data.offset = aad_pad_len;

	return 0;
}

static int
test_authenticated_encryption(const struct aead_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *ciphertext, *auth_tag;
	uint16_t plaintext_pad_len;
	uint32_t i;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	const struct rte_cryptodev_symmetric_capability *capability;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AEAD;
	cap_idx.algo.aead = tdata->algo;
	capability = rte_cryptodev_sym_capability_get(
			ts_params->valid_devs[0], &cap_idx);
	if (capability == NULL)
		return TEST_SKIPPED;
	if (rte_cryptodev_sym_capability_check_aead(
			capability, tdata->key.len, tdata->auth_tag.len,
			tdata->aad.len, tdata->iv.len))
		return TEST_SKIPPED;

	/* Create AEAD session */
	retval = create_aead_session(ts_params->valid_devs[0],
			tdata->algo,
			RTE_CRYPTO_AEAD_OP_ENCRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			tdata->iv.len);
	if (retval != TEST_SUCCESS)
		return retval;

	if (tdata->aad.len > MBUF_SIZE) {
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->large_mbuf_pool);
		/* Populate full size of add data */
		for (i = 32; i < MAX_AAD_LENGTH; i += 32)
			memcpy(&tdata->aad.data[i], &tdata->aad.data[0], 32);
	} else
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	/* Create AEAD operation */
	retval = create_aead_operation(RTE_CRYPTO_AEAD_OP_ENCRYPT, tdata);
	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;

	/* Process crypto operation */
	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_aead_op(ts_params->valid_devs[0], ut_params->op);
	else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 0, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		TEST_ASSERT_NOT_NULL(
			process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);

	if (ut_params->op->sym->m_dst) {
		ciphertext = rte_pktmbuf_mtod(ut_params->op->sym->m_dst,
				uint8_t *);
		auth_tag = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_dst,
				uint8_t *, plaintext_pad_len);
	} else {
		ciphertext = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_src,
				uint8_t *,
				ut_params->op->sym->cipher.data.offset);
		auth_tag = ciphertext + plaintext_pad_len;
	}

	debug_hexdump(stdout, "ciphertext:", ciphertext, tdata->ciphertext.len);
	debug_hexdump(stdout, "auth tag:", auth_tag, tdata->auth_tag.len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ciphertext,
			tdata->ciphertext.data,
			tdata->ciphertext.len,
			"Ciphertext data not as expected");

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			tdata->auth_tag.data,
			tdata->auth_tag.len,
			"Generated auth tag not as expected");

	return 0;

}

#ifdef RTE_LIB_SECURITY
static int
security_proto_supported(enum rte_security_session_action_type action,
	enum rte_security_session_protocol proto)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;

	const struct rte_security_capability *capabilities;
	const struct rte_security_capability *capability;
	uint16_t i = 0;

	void *ctx = rte_cryptodev_get_sec_ctx(ts_params->valid_devs[0]);


	capabilities = rte_security_capabilities_get(ctx);

	if (capabilities == NULL)
		return -ENOTSUP;

	while ((capability = &capabilities[i++])->action !=
			RTE_SECURITY_ACTION_TYPE_NONE) {
		if (capability->action == action &&
				capability->protocol == proto)
			return 0;
	}

	return -ENOTSUP;
}

/* Basic algorithm run function for async inplace mode.
 * Creates a session from input parameters and runs one operation
 * on input_vec. Checks the output of the crypto operation against
 * output_vec.
 */
static int test_pdcp_proto(int i, int oop, enum rte_crypto_cipher_operation opc,
			   enum rte_crypto_auth_operation opa,
			   const uint8_t *input_vec, unsigned int input_vec_len,
			   const uint8_t *output_vec,
			   unsigned int output_vec_len,
			   enum rte_crypto_cipher_algorithm cipher_alg,
			   const uint8_t *cipher_key, uint32_t cipher_key_len,
			   enum rte_crypto_auth_algorithm auth_alg,
			   const uint8_t *auth_key, uint32_t auth_key_len,
			   uint8_t bearer, enum rte_security_pdcp_domain domain,
			   uint8_t packet_direction, uint8_t sn_size,
			   uint32_t hfn, uint32_t hfn_threshold, uint8_t sdap)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	uint8_t *plaintext;
	int ret = TEST_SUCCESS;
	void *ctx = rte_cryptodev_get_sec_ctx(ts_params->valid_devs[0]);
	struct rte_cryptodev_info dev_info;
	uint64_t feat_flags;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	feat_flags = dev_info.feature_flags;

	/* Verify the capabilities */
	struct rte_security_capability_idx sec_cap_idx;

	sec_cap_idx.action = ut_params->type;
	sec_cap_idx.protocol = RTE_SECURITY_PROTOCOL_PDCP;
	sec_cap_idx.pdcp.domain = domain;
	if (rte_security_capability_get(ctx, &sec_cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Generate test mbuf data */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
						  input_vec_len);
	memcpy(plaintext, input_vec, input_vec_len);

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device does not support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}
	/* Out of place support */
	if (oop) {
		/*
		 * For out-op-place we need to alloc another mbuf
		 */
		ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
		rte_pktmbuf_append(ut_params->obuf, output_vec_len);
	}

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.cipher.algo = cipher_alg;
	ut_params->cipher_xform.cipher.op = opc;
	ut_params->cipher_xform.cipher.key.data = cipher_key;
	ut_params->cipher_xform.cipher.key.length = cipher_key_len;
	ut_params->cipher_xform.cipher.iv.length =
				packet_direction ? 4 : 0;
	ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;

	/* Setup HMAC Parameters if ICV header is required */
	if (auth_alg != 0) {
		ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
		ut_params->auth_xform.next = NULL;
		ut_params->auth_xform.auth.algo = auth_alg;
		ut_params->auth_xform.auth.op = opa;
		ut_params->auth_xform.auth.key.data = auth_key;
		ut_params->auth_xform.auth.key.length = auth_key_len;

		ut_params->cipher_xform.next = &ut_params->auth_xform;
	} else {
		ut_params->cipher_xform.next = NULL;
	}

	struct rte_security_session_conf sess_conf = {
		.action_type = ut_params->type,
		.protocol = RTE_SECURITY_PROTOCOL_PDCP,
		{.pdcp = {
			.bearer = bearer,
			.domain = domain,
			.pkt_dir = packet_direction,
			.sn_size = sn_size,
			.hfn = packet_direction ? 0 : hfn,
			/**
			 * hfn can be set as pdcp_test_hfn[i]
			 * if hfn_ovrd is not set. Here, PDCP
			 * packet direction is just used to
			 * run half of the cases with session
			 * HFN and other half with per packet
			 * HFN.
			 */
			.hfn_threshold = hfn_threshold,
			.hfn_ovrd = packet_direction ? 1 : 0,
			.sdap_enabled = sdap,
		} },
		.crypto_xform = &ut_params->cipher_xform
	};

	/* Create security session */
	ut_params->sec_session = rte_security_session_create(ctx,
				&sess_conf, ts_params->session_mpool);

	if (!ut_params->sec_session) {
		printf("TestCase %s()-%d line %d failed %s: ",
			__func__, i, __LINE__, "Failed to allocate session");
		ret = TEST_FAILED;
		goto on_err;
	}

	/* Generate crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (!ut_params->op) {
		printf("TestCase %s()-%d line %d failed %s: ",
			__func__, i, __LINE__,
			"Failed to allocate symmetric crypto operation struct");
		ret = TEST_FAILED;
		goto on_err;
	}

	uint32_t *per_pkt_hfn = rte_crypto_op_ctod_offset(ut_params->op,
					uint32_t *, IV_OFFSET);
	*per_pkt_hfn = packet_direction ? hfn : 0;

	rte_security_attach_session(ut_params->op, ut_params->sec_session);

	/* set crypto operation source mbuf */
	ut_params->op->sym->m_src = ut_params->ibuf;
	if (oop)
		ut_params->op->sym->m_dst = ut_params->obuf;

	/* Process crypto operation */
	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		/* filling lengths */
		ut_params->op->sym->cipher.data.length = ut_params->op->sym->m_src->pkt_len;
		ut_params->op->sym->auth.data.length = ut_params->op->sym->m_src->pkt_len;

		ret = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 0, 0);
		if (ret != TEST_SUCCESS)
			return ret;
	} else {
		ut_params->op = process_crypto_request(ts_params->valid_devs[0], ut_params->op);
	}
	if (ut_params->op == NULL) {
		printf("TestCase %s()-%d line %d failed %s: ",
			__func__, i, __LINE__,
			"failed to process sym crypto op");
		ret = TEST_FAILED;
		goto on_err;
	}

	if (ut_params->op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		printf("TestCase %s()-%d line %d failed %s: ",
			__func__, i, __LINE__, "crypto op processing failed");
		ret = TEST_FAILED;
		goto on_err;
	}

	/* Validate obuf */
	uint8_t *ciphertext = rte_pktmbuf_mtod(ut_params->op->sym->m_src,
			uint8_t *);
	if (oop) {
		ciphertext = rte_pktmbuf_mtod(ut_params->op->sym->m_dst,
				uint8_t *);
	}

	if (memcmp(ciphertext, output_vec, output_vec_len)) {
		printf("\n=======PDCP TestCase #%d failed: Data Mismatch ", i);
		rte_hexdump(stdout, "encrypted", ciphertext, output_vec_len);
		rte_hexdump(stdout, "reference", output_vec, output_vec_len);
		ret = TEST_FAILED;
		goto on_err;
	}

on_err:
	rte_crypto_op_free(ut_params->op);
	ut_params->op = NULL;

	if (ut_params->sec_session)
		rte_security_session_destroy(ctx, ut_params->sec_session);
	ut_params->sec_session = NULL;

	rte_pktmbuf_free(ut_params->ibuf);
	ut_params->ibuf = NULL;
	if (oop) {
		rte_pktmbuf_free(ut_params->obuf);
		ut_params->obuf = NULL;
	}

	return ret;
}

static int
test_pdcp_proto_SGL(int i, int oop,
	enum rte_crypto_cipher_operation opc,
	enum rte_crypto_auth_operation opa,
	uint8_t *input_vec,
	unsigned int input_vec_len,
	uint8_t *output_vec,
	unsigned int output_vec_len,
	uint32_t fragsz,
	uint32_t fragsz_oop)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	uint8_t *plaintext;
	struct rte_mbuf *buf, *buf_oop = NULL;
	int ret = TEST_SUCCESS;
	int to_trn = 0;
	int to_trn_tbl[16];
	int segs = 1;
	unsigned int trn_data = 0;
	struct rte_cryptodev_info dev_info;
	uint64_t feat_flags;
	void *ctx = rte_cryptodev_get_sec_ctx(ts_params->valid_devs[0]);
	struct rte_mbuf *temp_mbuf;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device does not support RAW data-path APIs.\n");
		return -ENOTSUP;
	}
	/* Verify the capabilities */
	struct rte_security_capability_idx sec_cap_idx;

	sec_cap_idx.action = ut_params->type;
	sec_cap_idx.protocol = RTE_SECURITY_PROTOCOL_PDCP;
	sec_cap_idx.pdcp.domain = pdcp_test_params[i].domain;
	if (rte_security_capability_get(ctx, &sec_cap_idx) == NULL)
		return TEST_SKIPPED;

	if (fragsz > input_vec_len)
		fragsz = input_vec_len;

	uint16_t plaintext_len = fragsz;
	uint16_t frag_size_oop = fragsz_oop ? fragsz_oop : fragsz;

	if (fragsz_oop > output_vec_len)
		frag_size_oop = output_vec_len;

	int ecx = 0;
	if (input_vec_len % fragsz != 0) {
		if (input_vec_len / fragsz + 1 > 16)
			return 1;
	} else if (input_vec_len / fragsz > 16)
		return 1;

	/* Out of place support */
	if (oop) {
		/*
		 * For out-op-place we need to alloc another mbuf
		 */
		ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
		rte_pktmbuf_append(ut_params->obuf, frag_size_oop);
		buf_oop = ut_params->obuf;
	}

	/* Generate test mbuf data */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
						  plaintext_len);
	memcpy(plaintext, input_vec, plaintext_len);
	trn_data += plaintext_len;

	buf = ut_params->ibuf;

	/*
	 * Loop until no more fragments
	 */

	while (trn_data < input_vec_len) {
		++segs;
		to_trn = (input_vec_len - trn_data < fragsz) ?
				(input_vec_len - trn_data) : fragsz;

		to_trn_tbl[ecx++] = to_trn;

		buf->next = rte_pktmbuf_alloc(ts_params->mbuf_pool);
		buf = buf->next;

		memset(rte_pktmbuf_mtod(buf, uint8_t *), 0,
				rte_pktmbuf_tailroom(buf));

		/* OOP */
		if (oop && !fragsz_oop) {
			buf_oop->next =
					rte_pktmbuf_alloc(ts_params->mbuf_pool);
			buf_oop = buf_oop->next;
			memset(rte_pktmbuf_mtod(buf_oop, uint8_t *),
					0, rte_pktmbuf_tailroom(buf_oop));
			rte_pktmbuf_append(buf_oop, to_trn);
		}

		plaintext = (uint8_t *)rte_pktmbuf_append(buf,
				to_trn);

		memcpy(plaintext, input_vec + trn_data, to_trn);
		trn_data += to_trn;
	}

	ut_params->ibuf->nb_segs = segs;

	segs = 1;
	if (fragsz_oop && oop) {
		to_trn = 0;
		ecx = 0;

		trn_data = frag_size_oop;
		while (trn_data < output_vec_len) {
			++segs;
			to_trn =
				(output_vec_len - trn_data <
						frag_size_oop) ?
				(output_vec_len - trn_data) :
						frag_size_oop;

			to_trn_tbl[ecx++] = to_trn;

			buf_oop->next =
				rte_pktmbuf_alloc(ts_params->mbuf_pool);
			buf_oop = buf_oop->next;
			memset(rte_pktmbuf_mtod(buf_oop, uint8_t *),
					0, rte_pktmbuf_tailroom(buf_oop));
			rte_pktmbuf_append(buf_oop, to_trn);

			trn_data += to_trn;
		}
		ut_params->obuf->nb_segs = segs;
	}

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.cipher.algo = pdcp_test_params[i].cipher_alg;
	ut_params->cipher_xform.cipher.op = opc;
	ut_params->cipher_xform.cipher.key.data = pdcp_test_crypto_key[i];
	ut_params->cipher_xform.cipher.key.length =
					pdcp_test_params[i].cipher_key_len;
	ut_params->cipher_xform.cipher.iv.length = 0;

	/* Setup HMAC Parameters if ICV header is required */
	if (pdcp_test_params[i].auth_alg != 0) {
		ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
		ut_params->auth_xform.next = NULL;
		ut_params->auth_xform.auth.algo = pdcp_test_params[i].auth_alg;
		ut_params->auth_xform.auth.op = opa;
		ut_params->auth_xform.auth.key.data = pdcp_test_auth_key[i];
		ut_params->auth_xform.auth.key.length =
					pdcp_test_params[i].auth_key_len;

		ut_params->cipher_xform.next = &ut_params->auth_xform;
	} else {
		ut_params->cipher_xform.next = NULL;
	}

	struct rte_security_session_conf sess_conf = {
		.action_type = ut_params->type,
		.protocol = RTE_SECURITY_PROTOCOL_PDCP,
		{.pdcp = {
			.bearer = pdcp_test_bearer[i],
			.domain = pdcp_test_params[i].domain,
			.pkt_dir = pdcp_test_packet_direction[i],
			.sn_size = pdcp_test_data_sn_size[i],
			.hfn = pdcp_test_hfn[i],
			.hfn_threshold = pdcp_test_hfn_threshold[i],
			.hfn_ovrd = 0,
		} },
		.crypto_xform = &ut_params->cipher_xform
	};

	/* Create security session */
	ut_params->sec_session = rte_security_session_create(ctx,
				&sess_conf, ts_params->session_mpool);

	if (!ut_params->sec_session) {
		printf("TestCase %s()-%d line %d failed %s: ",
			__func__, i, __LINE__, "Failed to allocate session");
		ret = TEST_FAILED;
		goto on_err;
	}

	/* Generate crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (!ut_params->op) {
		printf("TestCase %s()-%d line %d failed %s: ",
			__func__, i, __LINE__,
			"Failed to allocate symmetric crypto operation struct");
		ret = TEST_FAILED;
		goto on_err;
	}

	rte_security_attach_session(ut_params->op, ut_params->sec_session);

	/* set crypto operation source mbuf */
	ut_params->op->sym->m_src = ut_params->ibuf;
	if (oop)
		ut_params->op->sym->m_dst = ut_params->obuf;

	/* Process crypto operation */
	temp_mbuf = ut_params->op->sym->m_src;
	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		/* filling lengths */
		while (temp_mbuf) {
			ut_params->op->sym->cipher.data.length
				+= temp_mbuf->pkt_len;
			ut_params->op->sym->auth.data.length
				+= temp_mbuf->pkt_len;
			temp_mbuf = temp_mbuf->next;
		}

		ret = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 0, 0);
		if (ret != TEST_SUCCESS)
			return ret;
	} else {
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
							ut_params->op);
	}
	if (ut_params->op == NULL) {
		printf("TestCase %s()-%d line %d failed %s: ",
			__func__, i, __LINE__,
			"failed to process sym crypto op");
		ret = TEST_FAILED;
		goto on_err;
	}

	if (ut_params->op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		printf("TestCase %s()-%d line %d failed %s: ",
			__func__, i, __LINE__, "crypto op processing failed");
		ret = TEST_FAILED;
		goto on_err;
	}

	/* Validate obuf */
	uint8_t *ciphertext = rte_pktmbuf_mtod(ut_params->op->sym->m_src,
			uint8_t *);
	if (oop) {
		ciphertext = rte_pktmbuf_mtod(ut_params->op->sym->m_dst,
				uint8_t *);
	}
	if (fragsz_oop)
		fragsz = frag_size_oop;
	if (memcmp(ciphertext, output_vec, fragsz)) {
		printf("\n=======PDCP TestCase #%d failed: Data Mismatch ", i);
		rte_hexdump(stdout, "encrypted", ciphertext, fragsz);
		rte_hexdump(stdout, "reference", output_vec, fragsz);
		ret = TEST_FAILED;
		goto on_err;
	}

	buf = ut_params->op->sym->m_src->next;
	if (oop)
		buf = ut_params->op->sym->m_dst->next;

	unsigned int off = fragsz;

	ecx = 0;
	while (buf) {
		ciphertext = rte_pktmbuf_mtod(buf,
				uint8_t *);
		if (memcmp(ciphertext, output_vec + off, to_trn_tbl[ecx])) {
			printf("\n=======PDCP TestCase #%d failed: Data Mismatch ", i);
			rte_hexdump(stdout, "encrypted", ciphertext, to_trn_tbl[ecx]);
			rte_hexdump(stdout, "reference", output_vec + off,
					to_trn_tbl[ecx]);
			ret = TEST_FAILED;
			goto on_err;
		}
		off += to_trn_tbl[ecx++];
		buf = buf->next;
	}
on_err:
	rte_crypto_op_free(ut_params->op);
	ut_params->op = NULL;

	if (ut_params->sec_session)
		rte_security_session_destroy(ctx, ut_params->sec_session);
	ut_params->sec_session = NULL;

	rte_pktmbuf_free(ut_params->ibuf);
	ut_params->ibuf = NULL;
	if (oop) {
		rte_pktmbuf_free(ut_params->obuf);
		ut_params->obuf = NULL;
	}

	return ret;
}

int
test_pdcp_proto_cplane_encap(int i)
{
	return test_pdcp_proto(
		i, 0, RTE_CRYPTO_CIPHER_OP_ENCRYPT, RTE_CRYPTO_AUTH_OP_GENERATE,
		pdcp_test_data_in[i], pdcp_test_data_in_len[i],
		pdcp_test_data_out[i], pdcp_test_data_in_len[i] + 4,
		pdcp_test_params[i].cipher_alg, pdcp_test_crypto_key[i],
		pdcp_test_params[i].cipher_key_len,
		pdcp_test_params[i].auth_alg, pdcp_test_auth_key[i],
		pdcp_test_params[i].auth_key_len, pdcp_test_bearer[i],
		pdcp_test_params[i].domain, pdcp_test_packet_direction[i],
		pdcp_test_data_sn_size[i], pdcp_test_hfn[i],
		pdcp_test_hfn_threshold[i], SDAP_DISABLED);
}

int
test_pdcp_proto_uplane_encap(int i)
{
	return test_pdcp_proto(
		i, 0, RTE_CRYPTO_CIPHER_OP_ENCRYPT, RTE_CRYPTO_AUTH_OP_GENERATE,
		pdcp_test_data_in[i], pdcp_test_data_in_len[i],
		pdcp_test_data_out[i], pdcp_test_data_in_len[i],
		pdcp_test_params[i].cipher_alg, pdcp_test_crypto_key[i],
		pdcp_test_params[i].cipher_key_len,
		pdcp_test_params[i].auth_alg, pdcp_test_auth_key[i],
		pdcp_test_params[i].auth_key_len, pdcp_test_bearer[i],
		pdcp_test_params[i].domain, pdcp_test_packet_direction[i],
		pdcp_test_data_sn_size[i], pdcp_test_hfn[i],
		pdcp_test_hfn_threshold[i], SDAP_DISABLED);
}

int
test_pdcp_proto_uplane_encap_with_int(int i)
{
	return test_pdcp_proto(
		i, 0, RTE_CRYPTO_CIPHER_OP_ENCRYPT, RTE_CRYPTO_AUTH_OP_GENERATE,
		pdcp_test_data_in[i], pdcp_test_data_in_len[i],
		pdcp_test_data_out[i], pdcp_test_data_in_len[i] + 4,
		pdcp_test_params[i].cipher_alg, pdcp_test_crypto_key[i],
		pdcp_test_params[i].cipher_key_len,
		pdcp_test_params[i].auth_alg, pdcp_test_auth_key[i],
		pdcp_test_params[i].auth_key_len, pdcp_test_bearer[i],
		pdcp_test_params[i].domain, pdcp_test_packet_direction[i],
		pdcp_test_data_sn_size[i], pdcp_test_hfn[i],
		pdcp_test_hfn_threshold[i], SDAP_DISABLED);
}

int
test_pdcp_proto_cplane_decap(int i)
{
	return test_pdcp_proto(
		i, 0, RTE_CRYPTO_CIPHER_OP_DECRYPT, RTE_CRYPTO_AUTH_OP_VERIFY,
		pdcp_test_data_out[i], pdcp_test_data_in_len[i] + 4,
		pdcp_test_data_in[i], pdcp_test_data_in_len[i],
		pdcp_test_params[i].cipher_alg, pdcp_test_crypto_key[i],
		pdcp_test_params[i].cipher_key_len,
		pdcp_test_params[i].auth_alg, pdcp_test_auth_key[i],
		pdcp_test_params[i].auth_key_len, pdcp_test_bearer[i],
		pdcp_test_params[i].domain, pdcp_test_packet_direction[i],
		pdcp_test_data_sn_size[i], pdcp_test_hfn[i],
		pdcp_test_hfn_threshold[i], SDAP_DISABLED);
}

int
test_pdcp_proto_uplane_decap(int i)
{
	return test_pdcp_proto(
		i, 0, RTE_CRYPTO_CIPHER_OP_DECRYPT, RTE_CRYPTO_AUTH_OP_VERIFY,
		pdcp_test_data_out[i], pdcp_test_data_in_len[i],
		pdcp_test_data_in[i], pdcp_test_data_in_len[i],
		pdcp_test_params[i].cipher_alg, pdcp_test_crypto_key[i],
		pdcp_test_params[i].cipher_key_len,
		pdcp_test_params[i].auth_alg, pdcp_test_auth_key[i],
		pdcp_test_params[i].auth_key_len, pdcp_test_bearer[i],
		pdcp_test_params[i].domain, pdcp_test_packet_direction[i],
		pdcp_test_data_sn_size[i], pdcp_test_hfn[i],
		pdcp_test_hfn_threshold[i], SDAP_DISABLED);
}

int
test_pdcp_proto_uplane_decap_with_int(int i)
{
	return test_pdcp_proto(
		i, 0, RTE_CRYPTO_CIPHER_OP_DECRYPT, RTE_CRYPTO_AUTH_OP_VERIFY,
		pdcp_test_data_out[i], pdcp_test_data_in_len[i] + 4,
		pdcp_test_data_in[i], pdcp_test_data_in_len[i],
		pdcp_test_params[i].cipher_alg, pdcp_test_crypto_key[i],
		pdcp_test_params[i].cipher_key_len,
		pdcp_test_params[i].auth_alg, pdcp_test_auth_key[i],
		pdcp_test_params[i].auth_key_len, pdcp_test_bearer[i],
		pdcp_test_params[i].domain, pdcp_test_packet_direction[i],
		pdcp_test_data_sn_size[i], pdcp_test_hfn[i],
		pdcp_test_hfn_threshold[i], SDAP_DISABLED);
}

static int
test_PDCP_PROTO_SGL_in_place_32B(void)
{
	/* i can be used for running any PDCP case
	 * In this case it is uplane 12-bit AES-SNOW DL encap
	 */
	int i = PDCP_UPLANE_12BIT_OFFSET + AES_ENC + SNOW_AUTH + DOWNLINK;
	return test_pdcp_proto_SGL(i, IN_PLACE,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			pdcp_test_data_in[i],
			pdcp_test_data_in_len[i],
			pdcp_test_data_out[i],
			pdcp_test_data_in_len[i]+4,
			32, 0);
}
static int
test_PDCP_PROTO_SGL_oop_32B_128B(void)
{
	/* i can be used for running any PDCP case
	 * In this case it is uplane 18-bit NULL-NULL DL encap
	 */
	int i = PDCP_UPLANE_18BIT_OFFSET + NULL_ENC + NULL_AUTH + DOWNLINK;
	return test_pdcp_proto_SGL(i, OUT_OF_PLACE,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			pdcp_test_data_in[i],
			pdcp_test_data_in_len[i],
			pdcp_test_data_out[i],
			pdcp_test_data_in_len[i]+4,
			32, 128);
}
static int
test_PDCP_PROTO_SGL_oop_32B_40B(void)
{
	/* i can be used for running any PDCP case
	 * In this case it is uplane 18-bit AES DL encap
	 */
	int i = PDCP_UPLANE_OFFSET + AES_ENC + EIGHTEEN_BIT_SEQ_NUM_OFFSET
			+ DOWNLINK;
	return test_pdcp_proto_SGL(i, OUT_OF_PLACE,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			pdcp_test_data_in[i],
			pdcp_test_data_in_len[i],
			pdcp_test_data_out[i],
			pdcp_test_data_in_len[i],
			32, 40);
}
static int
test_PDCP_PROTO_SGL_oop_128B_32B(void)
{
	/* i can be used for running any PDCP case
	 * In this case it is cplane 12-bit AES-ZUC DL encap
	 */
	int i = PDCP_CPLANE_LONG_SN_OFFSET + AES_ENC + ZUC_AUTH + DOWNLINK;
	return test_pdcp_proto_SGL(i, OUT_OF_PLACE,
			RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE,
			pdcp_test_data_in[i],
			pdcp_test_data_in_len[i],
			pdcp_test_data_out[i],
			pdcp_test_data_in_len[i]+4,
			128, 32);
}

static int
test_PDCP_SDAP_PROTO_encap_all(void)
{
	int i = 0, size = 0;
	int err, all_err = TEST_SUCCESS;
	const struct pdcp_sdap_test *cur_test;

	size = RTE_DIM(list_pdcp_sdap_tests);

	for (i = 0; i < size; i++) {
		cur_test = &list_pdcp_sdap_tests[i];
		err = test_pdcp_proto(
			i, 0, RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE, cur_test->data_in,
			cur_test->in_len, cur_test->data_out,
			cur_test->in_len + ((cur_test->auth_key) ? 4 : 0),
			cur_test->param.cipher_alg, cur_test->cipher_key,
			cur_test->param.cipher_key_len,
			cur_test->param.auth_alg,
			cur_test->auth_key, cur_test->param.auth_key_len,
			cur_test->bearer, cur_test->param.domain,
			cur_test->packet_direction, cur_test->sn_size,
			cur_test->hfn,
			cur_test->hfn_threshold, SDAP_ENABLED);
		if (err) {
			printf("\t%d) %s: Encapsulation failed\n",
					cur_test->test_idx,
					cur_test->param.name);
			err = TEST_FAILED;
		} else {
			printf("\t%d) %s: Encap PASS\n", cur_test->test_idx,
					cur_test->param.name);
			err = TEST_SUCCESS;
		}
		all_err += err;
	}

	printf("Success: %d, Failure: %d\n", size + all_err, -all_err);

	return (all_err == TEST_SUCCESS) ? TEST_SUCCESS : TEST_FAILED;
}

static int
test_PDCP_PROTO_short_mac(void)
{
	int i = 0, size = 0;
	int err, all_err = TEST_SUCCESS;
	const struct pdcp_short_mac_test *cur_test;

	size = RTE_DIM(list_pdcp_smac_tests);

	for (i = 0; i < size; i++) {
		cur_test = &list_pdcp_smac_tests[i];
		err = test_pdcp_proto(
			i, 0, RTE_CRYPTO_CIPHER_OP_ENCRYPT,
			RTE_CRYPTO_AUTH_OP_GENERATE, cur_test->data_in,
			cur_test->in_len, cur_test->data_out,
			cur_test->in_len + ((cur_test->auth_key) ? 4 : 0),
			RTE_CRYPTO_CIPHER_NULL, NULL,
			0, cur_test->param.auth_alg,
			cur_test->auth_key, cur_test->param.auth_key_len,
			0, cur_test->param.domain, 0, 0,
			0, 0, 0);
		if (err) {
			printf("\t%d) %s: Short MAC test failed\n",
					cur_test->test_idx,
					cur_test->param.name);
			err = TEST_FAILED;
		} else {
			printf("\t%d) %s: Short MAC test PASS\n",
					cur_test->test_idx,
					cur_test->param.name);
			rte_hexdump(stdout, "MAC I",
				    cur_test->data_out + cur_test->in_len + 2,
				    2);
			err = TEST_SUCCESS;
		}
		all_err += err;
	}

	printf("Success: %d, Failure: %d\n", size + all_err, -all_err);

	return (all_err == TEST_SUCCESS) ? TEST_SUCCESS : TEST_FAILED;

}

static int
test_PDCP_SDAP_PROTO_decap_all(void)
{
	int i = 0, size = 0;
	int err, all_err = TEST_SUCCESS;
	const struct pdcp_sdap_test *cur_test;

	size = RTE_DIM(list_pdcp_sdap_tests);

	for (i = 0; i < size; i++) {
		cur_test = &list_pdcp_sdap_tests[i];
		err = test_pdcp_proto(
			i, 0, RTE_CRYPTO_CIPHER_OP_DECRYPT,
			RTE_CRYPTO_AUTH_OP_VERIFY,
			cur_test->data_out,
			cur_test->in_len + ((cur_test->auth_key) ? 4 : 0),
			cur_test->data_in, cur_test->in_len,
			cur_test->param.cipher_alg,
			cur_test->cipher_key, cur_test->param.cipher_key_len,
			cur_test->param.auth_alg, cur_test->auth_key,
			cur_test->param.auth_key_len, cur_test->bearer,
			cur_test->param.domain, cur_test->packet_direction,
			cur_test->sn_size, cur_test->hfn,
			cur_test->hfn_threshold, SDAP_ENABLED);
		if (err) {
			printf("\t%d) %s: Decapsulation failed\n",
					cur_test->test_idx,
					cur_test->param.name);
			err = TEST_FAILED;
		} else {
			printf("\t%d) %s: Decap PASS\n", cur_test->test_idx,
					cur_test->param.name);
			err = TEST_SUCCESS;
		}
		all_err += err;
	}

	printf("Success: %d, Failure: %d\n", size + all_err, -all_err);

	return (all_err == TEST_SUCCESS) ? TEST_SUCCESS : TEST_FAILED;
}

static inline void
ext_mbuf_callback_fn_free(void *addr __rte_unused, void *opaque __rte_unused)
{
}

static inline void
ext_mbuf_memzone_free(int nb_segs)
{
	int i;

	for (i = 0; i <= nb_segs; i++) {
		char mz_name[RTE_MEMZONE_NAMESIZE];
		const struct rte_memzone *memzone;
		snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "ext_buf_%d", i);
		memzone = rte_memzone_lookup(mz_name);
		if (memzone != NULL) {
			rte_memzone_free(memzone);
			memzone = NULL;
		}
	}
}

static inline struct rte_mbuf *
ext_mbuf_create(struct rte_mempool *mbuf_pool, int pkt_len,
		int nb_segs, const void *input_text)
{
	struct rte_mbuf *m = NULL, *mbuf = NULL;
	size_t data_off = 0;
	uint8_t *dst;
	int i, size;
	int t_len;

	if (pkt_len < 1) {
		printf("Packet size must be 1 or more (is %d)\n", pkt_len);
		return NULL;
	}

	if (nb_segs < 1) {
		printf("Number of segments must be 1 or more (is %d)\n",
				nb_segs);
		return NULL;
	}

	t_len = pkt_len >= nb_segs ? pkt_len / nb_segs : 1;
	size = pkt_len;

	/* Create chained mbuf_src with external buffer */
	for (i = 0; size > 0; i++) {
		struct rte_mbuf_ext_shared_info *ret_shinfo = NULL;
		uint16_t data_len = RTE_MIN(size, t_len);
		char mz_name[RTE_MEMZONE_NAMESIZE];
		const struct rte_memzone *memzone;
		void *ext_buf_addr = NULL;
		rte_iova_t buf_iova;
		bool freed = false;
		uint16_t buf_len;

		buf_len = RTE_ALIGN_CEIL(data_len + 1024 +
			sizeof(struct rte_mbuf_ext_shared_info), 8);

		snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "ext_buf_%d", i);
		memzone = rte_memzone_lookup(mz_name);
		if (memzone != NULL && memzone->len != buf_len) {
			rte_memzone_free(memzone);
			memzone = NULL;
		}
		if (memzone == NULL) {
			memzone = rte_memzone_reserve_aligned(mz_name, buf_len, SOCKET_ID_ANY,
				RTE_MEMZONE_IOVA_CONTIG, RTE_CACHE_LINE_SIZE);
			if (memzone == NULL) {
				printf("Can't allocate memory zone %s\n", mz_name);
				return NULL;
			}
		}

		ext_buf_addr = memzone->addr;
		memcpy(ext_buf_addr, RTE_PTR_ADD(input_text, data_off), data_len);

		/* Create buffer to hold rte_mbuf header */
		m = rte_pktmbuf_alloc(mbuf_pool);
		if (i == 0)
			mbuf = m;

		if (m == NULL) {
			printf("Cannot create segment for source mbuf");
			goto fail;
		}

		/* Save shared data (like callback function) in external buffer's end */
		ret_shinfo = rte_pktmbuf_ext_shinfo_init_helper(ext_buf_addr, &buf_len,
			ext_mbuf_callback_fn_free, &freed);
		if (ret_shinfo == NULL) {
			printf("Shared mem initialization failed!\n");
			goto fail;
		}

		buf_iova = rte_mem_virt2iova(ext_buf_addr);

		/* Attach external buffer to mbuf */
		rte_pktmbuf_attach_extbuf(m, ext_buf_addr, buf_iova, buf_len,
			ret_shinfo);
		if (m->ol_flags != RTE_MBUF_F_EXTERNAL) {
			printf("External buffer is not attached to mbuf\n");
			goto fail;
		}

		dst = (uint8_t *)rte_pktmbuf_append(m, data_len);
		if (dst == NULL) {
			printf("Cannot append %d bytes to the mbuf\n", data_len);
			goto fail;
		}

		if (mbuf != m)
			rte_pktmbuf_chain(mbuf, m);

		size -= data_len;
		data_off += data_len;
	}

	return mbuf;

fail:
	rte_pktmbuf_free(mbuf);
	ext_mbuf_memzone_free(nb_segs);
	return NULL;
}

static int
test_ipsec_proto_crypto_op_enq(struct crypto_testsuite_params *ts_params,
			       struct crypto_unittest_params *ut_params,
			       struct rte_security_ipsec_xform *ipsec_xform,
			       const struct ipsec_test_data *td,
			       const struct ipsec_test_flags *flags,
			       int pkt_num)
{
	uint8_t dev_id = ts_params->valid_devs[0];
	enum rte_security_ipsec_sa_direction dir;
	int ret;

	dir = ipsec_xform->direction;

	/* Generate crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (!ut_params->op) {
		printf("Could not allocate crypto op");
		return TEST_FAILED;
	}

	/* Attach session to operation */
	rte_security_attach_session(ut_params->op, ut_params->sec_session);

	/* Set crypto operation mbufs */
	ut_params->op->sym->m_src = ut_params->ibuf;
	ut_params->op->sym->m_dst = NULL;

	/* Copy IV in crypto operation when IV generation is disabled */
	if (dir == RTE_SECURITY_IPSEC_SA_DIR_EGRESS &&
	    ipsec_xform->options.iv_gen_disable == 1) {
		uint8_t *iv = rte_crypto_op_ctod_offset(ut_params->op,
							uint8_t *,
							IV_OFFSET);
		int len;

		if (td->aead)
			len = td->xform.aead.aead.iv.length;
		else if (td->aes_gmac)
			len = td->xform.chain.auth.auth.iv.length;
		else
			len = td->xform.chain.cipher.cipher.iv.length;

		memcpy(iv, td->iv.data, len);
	}

	/* Process crypto operation */
	process_crypto_request(dev_id, ut_params->op);

	ret = test_ipsec_status_check(td, ut_params->op, flags, dir, pkt_num);

	rte_crypto_op_free(ut_params->op);
	ut_params->op = NULL;

	return ret;
}

static int
test_ipsec_proto_mbuf_enq(struct crypto_testsuite_params *ts_params,
			  struct crypto_unittest_params *ut_params,
			  void *ctx)
{
	uint64_t timeout, userdata;
	struct rte_ether_hdr *hdr;
	struct rte_mbuf *m;
	void **sec_sess;
	int ret;

	RTE_SET_USED(ts_params);

	hdr = (void *)rte_pktmbuf_prepend(ut_params->ibuf, sizeof(struct rte_ether_hdr));
	hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

	ut_params->ibuf->l2_len = sizeof(struct rte_ether_hdr);
	ut_params->ibuf->port = 0;

	sec_sess = &ut_params->sec_session;
	ret = rte_security_inb_pkt_rx_inject(ctx, &ut_params->ibuf, sec_sess, 1);

	if (ret != 1)
		return TEST_FAILED;

	ut_params->ibuf = NULL;

	/* Add a timeout for 1 s */
	timeout = rte_get_tsc_cycles() + rte_get_tsc_hz();

	do {
		/* Get packet from port 0, queue 0 */
		ret = rte_eth_rx_burst(0, 0, &m, 1);
	} while ((ret == 0) && (rte_get_tsc_cycles() < timeout));

	if (ret == 0) {
		printf("Could not receive packets from ethdev\n");
		return TEST_FAILED;
	}

	if (m == NULL) {
		printf("Received mbuf is NULL\n");
		return TEST_FAILED;
	}

	ut_params->ibuf = m;

	if (!(m->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD)) {
		printf("Received packet is not Rx security processed\n");
		return TEST_FAILED;
	}

	if (m->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED) {
		printf("Received packet has failed Rx security processing\n");
		return TEST_FAILED;
	}

	/*
	 * 'ut_params' is set as userdata. Verify that the field is returned
	 * correctly.
	 */
	userdata = *(uint64_t *)rte_security_dynfield(m);
	if (userdata != (uint64_t)ut_params) {
		printf("Userdata retrieved not matching expected\n");
		return TEST_FAILED;
	}

	/* Trim L2 header */
	rte_pktmbuf_adj(m, sizeof(struct rte_ether_hdr));

	return TEST_SUCCESS;
}

static int
test_ipsec_proto_process(const struct ipsec_test_data td[],
			 struct ipsec_test_data res_d[],
			 int nb_td,
			 bool silent,
			 const struct ipsec_test_flags *flags)
{
	uint16_t v6_src[8] = {0x2607, 0xf8b0, 0x400c, 0x0c03, 0x0000, 0x0000,
				0x0000, 0x001a};
	uint16_t v6_dst[8] = {0x2001, 0x0470, 0xe5bf, 0xdead, 0x4957, 0x2174,
				0xe82c, 0x4887};
	const struct rte_ipv4_hdr *ipv4 =
			(const struct rte_ipv4_hdr *)td[0].output_text.data;
	int nb_segs = flags->nb_segs_in_mbuf ? flags->nb_segs_in_mbuf : 1;
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_security_capability_idx sec_cap_idx;
	const struct rte_security_capability *sec_cap;
	struct rte_security_ipsec_xform ipsec_xform;
	uint8_t dev_id = ts_params->valid_devs[0];
	enum rte_security_ipsec_sa_direction dir;
	struct ipsec_test_data *res_d_tmp = NULL;
	uint8_t input_text[IPSEC_TEXT_MAX_LEN];
	int salt_len, i, ret = TEST_SUCCESS;
	void *ctx;
	uint32_t src, dst;
	uint32_t verify;

	ut_params->type = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL;
	gbl_action_type = RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL;

	/* Use first test data to create session */

	/* Copy IPsec xform */
	memcpy(&ipsec_xform, &td[0].ipsec_xform, sizeof(ipsec_xform));

	dir = ipsec_xform.direction;
	verify = flags->tunnel_hdr_verify;

	memcpy(&src, &ipv4->src_addr, sizeof(ipv4->src_addr));
	memcpy(&dst, &ipv4->dst_addr, sizeof(ipv4->dst_addr));

	if ((dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) && verify) {
		if (verify == RTE_SECURITY_IPSEC_TUNNEL_VERIFY_SRC_DST_ADDR)
			src += 1;
		else if (verify == RTE_SECURITY_IPSEC_TUNNEL_VERIFY_DST_ADDR)
			dst += 1;
	}

	if (td->ipsec_xform.mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
		if (td->ipsec_xform.tunnel.type ==
				RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
			memcpy(&ipsec_xform.tunnel.ipv4.src_ip, &src,
			       sizeof(src));
			memcpy(&ipsec_xform.tunnel.ipv4.dst_ip, &dst,
			       sizeof(dst));

			if (flags->df == TEST_IPSEC_SET_DF_0_INNER_1)
				ipsec_xform.tunnel.ipv4.df = 0;

			if (flags->df == TEST_IPSEC_SET_DF_1_INNER_0)
				ipsec_xform.tunnel.ipv4.df = 1;

			if (flags->dscp == TEST_IPSEC_SET_DSCP_0_INNER_1)
				ipsec_xform.tunnel.ipv4.dscp = 0;

			if (flags->dscp == TEST_IPSEC_SET_DSCP_1_INNER_0)
				ipsec_xform.tunnel.ipv4.dscp =
						TEST_IPSEC_DSCP_VAL;

		} else {
			if (flags->dscp == TEST_IPSEC_SET_DSCP_0_INNER_1)
				ipsec_xform.tunnel.ipv6.dscp = 0;

			if (flags->dscp == TEST_IPSEC_SET_DSCP_1_INNER_0)
				ipsec_xform.tunnel.ipv6.dscp =
						TEST_IPSEC_DSCP_VAL;

			memcpy(&ipsec_xform.tunnel.ipv6.src_addr, &v6_src,
			       sizeof(v6_src));
			memcpy(&ipsec_xform.tunnel.ipv6.dst_addr, &v6_dst,
			       sizeof(v6_dst));
		}
	}

	ctx = rte_cryptodev_get_sec_ctx(dev_id);

	sec_cap_idx.action = ut_params->type;
	sec_cap_idx.protocol = RTE_SECURITY_PROTOCOL_IPSEC;
	sec_cap_idx.ipsec.proto = ipsec_xform.proto;
	sec_cap_idx.ipsec.mode = ipsec_xform.mode;
	sec_cap_idx.ipsec.direction = ipsec_xform.direction;

	if (flags->udp_encap)
		ipsec_xform.options.udp_encap = 1;

	sec_cap = rte_security_capability_get(ctx, &sec_cap_idx);
	if (sec_cap == NULL)
		return TEST_SKIPPED;

	/* Copy cipher session parameters */
	if (td[0].aead) {
		memcpy(&ut_params->aead_xform, &td[0].xform.aead,
		       sizeof(ut_params->aead_xform));
		ut_params->aead_xform.aead.key.data = td[0].key.data;
		ut_params->aead_xform.aead.iv.offset = IV_OFFSET;

		/* Verify crypto capabilities */
		if (test_ipsec_crypto_caps_aead_verify(
				sec_cap,
				&ut_params->aead_xform) != 0) {
			if (!silent)
				RTE_LOG(INFO, USER1,
					"Crypto capabilities not supported\n");
			return TEST_SKIPPED;
		}
	} else if (td[0].auth_only) {
		memcpy(&ut_params->auth_xform, &td[0].xform.chain.auth,
		       sizeof(ut_params->auth_xform));
		ut_params->auth_xform.auth.key.data = td[0].auth_key.data;

		if (test_ipsec_crypto_caps_auth_verify(
				sec_cap,
				&ut_params->auth_xform) != 0) {
			if (!silent)
				RTE_LOG(INFO, USER1,
					"Auth crypto capabilities not supported\n");
			return TEST_SKIPPED;
		}
	} else {
		memcpy(&ut_params->cipher_xform, &td[0].xform.chain.cipher,
		       sizeof(ut_params->cipher_xform));
		memcpy(&ut_params->auth_xform, &td[0].xform.chain.auth,
		       sizeof(ut_params->auth_xform));
		ut_params->cipher_xform.cipher.key.data = td[0].key.data;
		ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
		ut_params->auth_xform.auth.key.data = td[0].auth_key.data;

		/* Verify crypto capabilities */

		if (test_ipsec_crypto_caps_cipher_verify(
				sec_cap,
				&ut_params->cipher_xform) != 0) {
			if (!silent)
				RTE_LOG(INFO, USER1,
					"Cipher crypto capabilities not supported\n");
			return TEST_SKIPPED;
		}

		if (test_ipsec_crypto_caps_auth_verify(
				sec_cap,
				&ut_params->auth_xform) != 0) {
			if (!silent)
				RTE_LOG(INFO, USER1,
					"Auth crypto capabilities not supported\n");
			return TEST_SKIPPED;
		}
	}

	if (test_ipsec_sec_caps_verify(&ipsec_xform, sec_cap, silent) != 0)
		return TEST_SKIPPED;

	struct rte_security_session_conf sess_conf = {
		.action_type = ut_params->type,
		.protocol = RTE_SECURITY_PROTOCOL_IPSEC,
	};

	if (td[0].aead || td[0].aes_gmac) {
		salt_len = RTE_MIN(sizeof(ipsec_xform.salt), td[0].salt.len);
		memcpy(&ipsec_xform.salt, td[0].salt.data, salt_len);
	}

	if (td[0].aead) {
		sess_conf.ipsec = ipsec_xform;
		sess_conf.crypto_xform = &ut_params->aead_xform;
	} else if (td[0].auth_only) {
		sess_conf.ipsec = ipsec_xform;
		sess_conf.crypto_xform = &ut_params->auth_xform;
	} else {
		sess_conf.ipsec = ipsec_xform;
		if (dir == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
			sess_conf.crypto_xform = &ut_params->cipher_xform;
			ut_params->cipher_xform.next = &ut_params->auth_xform;
		} else {
			sess_conf.crypto_xform = &ut_params->auth_xform;
			ut_params->auth_xform.next = &ut_params->cipher_xform;
		}
	}

	if (dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS && flags->rx_inject)
		sess_conf.userdata = ut_params;

	/* Create security session */
	ut_params->sec_session = rte_security_session_create(ctx, &sess_conf,
					ts_params->session_mpool);

	if (ut_params->sec_session == NULL)
		return TEST_SKIPPED;

	for (i = 0; i < nb_td; i++) {
		if (flags->antireplay &&
		    (dir == RTE_SECURITY_IPSEC_SA_DIR_EGRESS)) {
			sess_conf.ipsec.esn.value = td[i].ipsec_xform.esn.value;
			ret = rte_security_session_update(ctx,
				ut_params->sec_session, &sess_conf);
			if (ret) {
				printf("Could not update sequence number in "
				       "session\n");
				return TEST_SKIPPED;
			}
		}

		/* Copy test data before modification */
		memcpy(input_text, td[i].input_text.data, td[i].input_text.len);
		if (test_ipsec_pkt_update(input_text, flags)) {
			ret = TEST_FAILED;
			goto mbuf_free;
		}

		/* Setup source mbuf payload */
		if (flags->use_ext_mbuf) {
			ut_params->ibuf = ext_mbuf_create(ts_params->mbuf_pool,
					td[i].input_text.len, nb_segs, input_text);
		} else {
			ut_params->ibuf = create_segmented_mbuf(ts_params->mbuf_pool,
					td[i].input_text.len, nb_segs, 0);
			pktmbuf_write(ut_params->ibuf, 0, td[i].input_text.len, input_text);
		}

		if (dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS && flags->rx_inject)
			ret = test_ipsec_proto_mbuf_enq(ts_params, ut_params,
							ctx);
		else
			ret = test_ipsec_proto_crypto_op_enq(ts_params,
							     ut_params,
							     &ipsec_xform,
							     &td[i], flags,
							     i + 1);

		if (ret != TEST_SUCCESS)
			goto mbuf_free;

		if (res_d != NULL)
			res_d_tmp = &res_d[i];

		ret = test_ipsec_post_process(ut_params->ibuf, &td[i],
					      res_d_tmp, silent, flags);
		if (ret != TEST_SUCCESS)
			goto mbuf_free;

		ret = test_ipsec_stats_verify(ctx, ut_params->sec_session,
					      flags, dir);
		if (ret != TEST_SUCCESS)
			goto mbuf_free;

		rte_pktmbuf_free(ut_params->ibuf);
		ut_params->ibuf = NULL;
	}

mbuf_free:
	if (flags->use_ext_mbuf)
		ext_mbuf_memzone_free(nb_segs);

	rte_pktmbuf_free(ut_params->ibuf);
	ut_params->ibuf = NULL;

	if (ut_params->sec_session)
		rte_security_session_destroy(ctx, ut_params->sec_session);
	ut_params->sec_session = NULL;

	return ret;
}

static int
test_ipsec_proto_known_vec(const void *test_data)
{
	struct ipsec_test_data td_outb;
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	memcpy(&td_outb, test_data, sizeof(td_outb));

	if (td_outb.aes_gmac || td_outb.aead ||
	    ((td_outb.ipsec_xform.proto != RTE_SECURITY_IPSEC_SA_PROTO_AH) &&
	     (td_outb.xform.chain.cipher.cipher.algo != RTE_CRYPTO_CIPHER_NULL))) {
		/* Disable IV gen to be able to test with known vectors */
		td_outb.ipsec_xform.options.iv_gen_disable = 1;
	}

	return test_ipsec_proto_process(&td_outb, NULL, 1, false, &flags);
}

static int
test_ipsec_proto_known_vec_ext_mbuf(const void *test_data)
{
	struct ipsec_test_data td_outb;
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));
	flags.use_ext_mbuf = true;

	memcpy(&td_outb, test_data, sizeof(td_outb));

	if (td_outb.aes_gmac || td_outb.aead ||
	    ((td_outb.ipsec_xform.proto != RTE_SECURITY_IPSEC_SA_PROTO_AH) &&
	     (td_outb.xform.chain.cipher.cipher.algo != RTE_CRYPTO_CIPHER_NULL))) {
		/* Disable IV gen to be able to test with known vectors */
		td_outb.ipsec_xform.options.iv_gen_disable = 1;
	}

	return test_ipsec_proto_process(&td_outb, NULL, 1, false, &flags);
}

static int
test_ipsec_proto_known_vec_inb(const void *test_data)
{
	const struct ipsec_test_data *td = test_data;
	struct ipsec_test_flags flags;
	struct ipsec_test_data td_inb;

	memset(&flags, 0, sizeof(flags));

	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS)
		test_ipsec_td_in_from_out(td, &td_inb);
	else
		memcpy(&td_inb, td, sizeof(td_inb));

	return test_ipsec_proto_process(&td_inb, NULL, 1, false, &flags);
}

static int
test_ipsec_proto_known_vec_fragmented(const void *test_data)
{
	struct ipsec_test_data td_outb;
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));
	flags.fragment = true;

	memcpy(&td_outb, test_data, sizeof(td_outb));

	/* Disable IV gen to be able to test with known vectors */
	td_outb.ipsec_xform.options.iv_gen_disable = 1;

	return test_ipsec_proto_process(&td_outb, NULL, 1, false, &flags);
}

static int
test_ipsec_proto_known_vec_inb_rx_inject(const void *test_data)
{
	const struct ipsec_test_data *td = test_data;
	struct ipsec_test_flags flags;
	struct ipsec_test_data td_inb;

	memset(&flags, 0, sizeof(flags));
	flags.rx_inject = true;

	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS)
		test_ipsec_td_in_from_out(td, &td_inb);
	else
		memcpy(&td_inb, td, sizeof(td_inb));

	return test_ipsec_proto_process(&td_inb, NULL, 1, false, &flags);
}

static int
test_ipsec_proto_all(const struct ipsec_test_flags *flags)
{
	struct ipsec_test_data td_outb[IPSEC_TEST_PACKETS_MAX];
	struct ipsec_test_data td_inb[IPSEC_TEST_PACKETS_MAX];
	unsigned int i, nb_pkts = 1, pass_cnt = 0;
	int ret;

	if (flags->iv_gen ||
	    flags->sa_expiry_pkts_soft ||
	    flags->sa_expiry_pkts_hard)
		nb_pkts = IPSEC_TEST_PACKETS_MAX;

	for (i = 0; i < RTE_DIM(alg_list); i++) {
		test_ipsec_td_prepare(alg_list[i].param1,
				      alg_list[i].param2,
				      flags,
				      td_outb,
				      nb_pkts);

		if (!td_outb->aead) {
			enum rte_crypto_cipher_algorithm cipher_alg;
			enum rte_crypto_auth_algorithm auth_alg;

			cipher_alg = td_outb->xform.chain.cipher.cipher.algo;
			auth_alg = td_outb->xform.chain.auth.auth.algo;

			if (td_outb->aes_gmac && cipher_alg != RTE_CRYPTO_CIPHER_NULL)
				continue;

			/* ICV is not applicable for NULL auth */
			if (flags->icv_corrupt &&
			    auth_alg == RTE_CRYPTO_AUTH_NULL)
				continue;

			/* IV is not applicable for NULL cipher */
			if (flags->iv_gen &&
			    cipher_alg == RTE_CRYPTO_CIPHER_NULL)
				continue;
		}

		ret = test_ipsec_proto_process(td_outb, td_inb, nb_pkts, true,
					       flags);
		if (ret == TEST_SKIPPED)
			continue;

		if (ret == TEST_FAILED)
			return TEST_FAILED;

		test_ipsec_td_update(td_inb, td_outb, nb_pkts, flags);

		ret = test_ipsec_proto_process(td_inb, NULL, nb_pkts, true,
					       flags);
		if (ret == TEST_SKIPPED)
			continue;

		if (ret == TEST_FAILED)
			return TEST_FAILED;

		if (flags->display_alg)
			test_ipsec_display_alg(alg_list[i].param1,
					       alg_list[i].param2);

		pass_cnt++;
	}

	if (pass_cnt > 0)
		return TEST_SUCCESS;
	else
		return TEST_SKIPPED;
}

static int
test_ipsec_ah_proto_all(const struct ipsec_test_flags *flags)
{
	struct ipsec_test_data td_outb[IPSEC_TEST_PACKETS_MAX];
	struct ipsec_test_data td_inb[IPSEC_TEST_PACKETS_MAX];
	unsigned int i, nb_pkts = 1, pass_cnt = 0;
	int ret;

	for (i = 0; i < RTE_DIM(ah_alg_list); i++) {
		test_ipsec_td_prepare(ah_alg_list[i].param1,
				      ah_alg_list[i].param2,
				      flags,
				      td_outb,
				      nb_pkts);

		ret = test_ipsec_proto_process(td_outb, td_inb, nb_pkts, true,
					       flags);
		if (ret == TEST_SKIPPED)
			continue;

		if (ret == TEST_FAILED)
			return TEST_FAILED;

		test_ipsec_td_update(td_inb, td_outb, nb_pkts, flags);

		ret = test_ipsec_proto_process(td_inb, NULL, nb_pkts, true,
					       flags);
		if (ret == TEST_SKIPPED)
			continue;

		if (ret == TEST_FAILED)
			return TEST_FAILED;

		if (flags->display_alg)
			test_ipsec_display_alg(ah_alg_list[i].param1,
					       ah_alg_list[i].param2);

		pass_cnt++;
	}

	if (pass_cnt > 0)
		return TEST_SUCCESS;
	else
		return TEST_SKIPPED;
}

static int
test_ipsec_proto_display_list(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.display_alg = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_ah_tunnel_ipv4(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ah = true;
	flags.display_alg = true;

	return test_ipsec_ah_proto_all(&flags);
}

static int
test_ipsec_proto_ah_transport_ipv4(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ah = true;
	flags.transport = true;

	return test_ipsec_ah_proto_all(&flags);
}

static int
test_ipsec_proto_iv_gen(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.iv_gen = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_sa_exp_pkts_soft(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.sa_expiry_pkts_soft = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_sa_exp_pkts_hard(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.sa_expiry_pkts_hard = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_err_icv_corrupt(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.icv_corrupt = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_udp_encap_custom_ports(void)
{
	struct ipsec_test_flags flags;

	if (gbl_driver_id == rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_CN10K_PMD)))
		return TEST_SKIPPED;

	memset(&flags, 0, sizeof(flags));

	flags.udp_encap = true;
	flags.udp_encap_custom_ports = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_udp_encap(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.udp_encap = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_tunnel_src_dst_addr_verify(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.tunnel_hdr_verify = RTE_SECURITY_IPSEC_TUNNEL_VERIFY_SRC_DST_ADDR;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_tunnel_dst_addr_verify(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.tunnel_hdr_verify = RTE_SECURITY_IPSEC_TUNNEL_VERIFY_DST_ADDR;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_udp_ports_verify(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.udp_encap = true;
	flags.udp_ports_verify = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_inner_ip_csum(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ip_csum = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_inner_l4_csum(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.l4_csum = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_tunnel_v4_in_v4(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = false;
	flags.tunnel_ipv6 = false;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_tunnel_v6_in_v6(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_tunnel_v4_in_v6(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = false;
	flags.tunnel_ipv6 = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_tunnel_v6_in_v4(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = false;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_transport_v4(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = false;
	flags.transport = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_transport_l4_csum(void)
{
	struct ipsec_test_flags flags = {
		.l4_csum = true,
		.transport = true,
	};

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_stats(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.stats_success = true;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_pkt_fragment(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.fragment = true;

	return test_ipsec_proto_all(&flags);

}

static int
test_ipsec_proto_copy_df_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.df = TEST_IPSEC_COPY_DF_INNER_0;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_copy_df_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.df = TEST_IPSEC_COPY_DF_INNER_1;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_set_df_0_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.df = TEST_IPSEC_SET_DF_0_INNER_1;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_set_df_1_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.df = TEST_IPSEC_SET_DF_1_INNER_0;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_ipv4_copy_dscp_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.dscp = TEST_IPSEC_COPY_DSCP_INNER_0;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_ipv4_copy_dscp_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.dscp = TEST_IPSEC_COPY_DSCP_INNER_1;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_ipv4_set_dscp_0_inner_1(void)
{
	struct ipsec_test_flags flags;

	if (gbl_driver_id == rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_CN9K_PMD)))
		return TEST_SKIPPED;

	memset(&flags, 0, sizeof(flags));

	flags.dscp = TEST_IPSEC_SET_DSCP_0_INNER_1;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_ipv4_set_dscp_1_inner_0(void)
{
	struct ipsec_test_flags flags;

	if (gbl_driver_id == rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_CN9K_PMD)))
		return TEST_SKIPPED;

	memset(&flags, 0, sizeof(flags));

	flags.dscp = TEST_IPSEC_SET_DSCP_1_INNER_0;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_ipv6_copy_dscp_inner_0(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.dscp = TEST_IPSEC_COPY_DSCP_INNER_0;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_ipv6_copy_dscp_inner_1(void)
{
	struct ipsec_test_flags flags;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.dscp = TEST_IPSEC_COPY_DSCP_INNER_1;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_ipv6_set_dscp_0_inner_1(void)
{
	struct ipsec_test_flags flags;

	if (gbl_driver_id == rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_CN9K_PMD)))
		return TEST_SKIPPED;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.dscp = TEST_IPSEC_SET_DSCP_0_INNER_1;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_ipv6_set_dscp_1_inner_0(void)
{
	struct ipsec_test_flags flags;

	if (gbl_driver_id == rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_CN9K_PMD)))
		return TEST_SKIPPED;

	memset(&flags, 0, sizeof(flags));

	flags.ipv6 = true;
	flags.tunnel_ipv6 = true;
	flags.dscp = TEST_IPSEC_SET_DSCP_1_INNER_0;

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_sgl(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info dev_info;

	struct ipsec_test_flags flags = {
		.nb_segs_in_mbuf = 5
	};

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)) {
		printf("Device doesn't support in-place scatter-gather. "
				"Test Skipped.\n");
		return TEST_SKIPPED;
	}

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_sgl_ext_mbuf(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info dev_info;

	struct ipsec_test_flags flags = {
		.nb_segs_in_mbuf = 5,
		.use_ext_mbuf = 1
	};

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	if (!(dev_info.feature_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)) {
		printf("Device doesn't support in-place scatter-gather. "
				"Test Skipped.\n");
		return TEST_SKIPPED;
	}

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_pkt_replay(const void *test_data, const uint64_t esn[],
		      bool replayed_pkt[], uint32_t nb_pkts, bool esn_en,
		      uint64_t winsz)
{
	struct ipsec_test_data td_outb[IPSEC_TEST_PACKETS_MAX];
	struct ipsec_test_data td_inb[IPSEC_TEST_PACKETS_MAX];
	struct ipsec_test_flags flags;
	uint32_t i = 0, ret = 0;

	if (nb_pkts == 0)
		return TEST_FAILED;

	memset(&flags, 0, sizeof(flags));
	flags.antireplay = true;

	for (i = 0; i < nb_pkts; i++) {
		memcpy(&td_outb[i], test_data, sizeof(td_outb[i]));
		td_outb[i].ipsec_xform.options.iv_gen_disable = 1;
		td_outb[i].ipsec_xform.replay_win_sz = winsz;
		td_outb[i].ipsec_xform.options.esn = esn_en;
	}

	for (i = 0; i < nb_pkts; i++)
		td_outb[i].ipsec_xform.esn.value = esn[i];

	ret = test_ipsec_proto_process(td_outb, td_inb, nb_pkts, true,
				       &flags);
	if (ret != TEST_SUCCESS)
		return ret;

	test_ipsec_td_update(td_inb, td_outb, nb_pkts, &flags);

	for (i = 0; i < nb_pkts; i++) {
		td_inb[i].ipsec_xform.options.esn = esn_en;
		/* Set antireplay flag for packets to be dropped */
		td_inb[i].ar_packet = replayed_pkt[i];
	}

	ret = test_ipsec_proto_process(td_inb, NULL, nb_pkts, true,
				       &flags);

	return ret;
}

static int
test_ipsec_proto_pkt_antireplay(const void *test_data, uint64_t winsz)
{

	uint32_t nb_pkts = 5;
	bool replayed_pkt[5];
	uint64_t esn[5];

	/* 1. Advance the TOP of the window to WS * 2 */
	esn[0] = winsz * 2;
	/* 2. Test sequence number within the new window(WS + 1) */
	esn[1] = winsz + 1;
	/* 3. Test sequence number less than the window BOTTOM */
	esn[2] = winsz;
	/* 4. Test sequence number in the middle of the window */
	esn[3] = winsz + (winsz / 2);
	/* 5. Test replay of the packet in the middle of the window */
	esn[4] = winsz + (winsz / 2);

	replayed_pkt[0] = false;
	replayed_pkt[1] = false;
	replayed_pkt[2] = true;
	replayed_pkt[3] = false;
	replayed_pkt[4] = true;

	return test_ipsec_pkt_replay(test_data, esn, replayed_pkt, nb_pkts,
				     false, winsz);
}

static int
test_ipsec_proto_pkt_antireplay1024(const void *test_data)
{
	return test_ipsec_proto_pkt_antireplay(test_data, 1024);
}

static int
test_ipsec_proto_pkt_antireplay2048(const void *test_data)
{
	return test_ipsec_proto_pkt_antireplay(test_data, 2048);
}

static int
test_ipsec_proto_pkt_antireplay4096(const void *test_data)
{
	return test_ipsec_proto_pkt_antireplay(test_data, 4096);
}

static int
test_ipsec_proto_pkt_esn_antireplay(const void *test_data, uint64_t winsz)
{

	uint32_t nb_pkts = 7;
	bool replayed_pkt[7];
	uint64_t esn[7];

	/* Set the initial sequence number */
	esn[0] = (uint64_t)(0xFFFFFFFF - winsz);
	/* 1. Advance the TOP of the window to (1<<32 + WS/2) */
	esn[1] = (uint64_t)((1ULL << 32) + (winsz / 2));
	/* 2. Test sequence number within new window (1<<32 + WS/2 + 1) */
	esn[2] = (uint64_t)((1ULL << 32) - (winsz / 2) + 1);
	/* 3. Test with sequence number within window (1<<32 - 1) */
	esn[3] = (uint64_t)((1ULL << 32) - 1);
	/* 4. Test with sequence number within window (1<<32 - 1) */
	esn[4] = (uint64_t)(1ULL << 32);
	/* 5. Test with duplicate sequence number within
	 * new window (1<<32 - 1)
	 */
	esn[5] = (uint64_t)((1ULL << 32) - 1);
	/* 6. Test with duplicate sequence number within new window (1<<32) */
	esn[6] = (uint64_t)(1ULL << 32);

	replayed_pkt[0] = false;
	replayed_pkt[1] = false;
	replayed_pkt[2] = false;
	replayed_pkt[3] = false;
	replayed_pkt[4] = false;
	replayed_pkt[5] = true;
	replayed_pkt[6] = true;

	return test_ipsec_pkt_replay(test_data, esn, replayed_pkt, nb_pkts,
				     true, winsz);
}

static int
test_ipsec_proto_pkt_esn_antireplay1024(const void *test_data)
{
	return test_ipsec_proto_pkt_esn_antireplay(test_data, 1024);
}

static int
test_ipsec_proto_pkt_esn_antireplay2048(const void *test_data)
{
	return test_ipsec_proto_pkt_esn_antireplay(test_data, 2048);
}

static int
test_ipsec_proto_pkt_esn_antireplay4096(const void *test_data)
{
	return test_ipsec_proto_pkt_esn_antireplay(test_data, 4096);
}

static int
test_PDCP_PROTO_all(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_cryptodev_info dev_info;
	int status;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_SECURITY))
		return TEST_SKIPPED;

	/* Set action type */
	ut_params->type = gbl_action_type == RTE_SECURITY_ACTION_TYPE_NONE ?
		RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL :
		gbl_action_type;

	if (security_proto_supported(ut_params->type,
			RTE_SECURITY_PROTOCOL_PDCP) < 0)
		return TEST_SKIPPED;

	status = test_PDCP_PROTO_cplane_encap_all();
	status += test_PDCP_PROTO_cplane_decap_all();
	status += test_PDCP_PROTO_uplane_encap_all();
	status += test_PDCP_PROTO_uplane_decap_all();
	status += test_PDCP_PROTO_SGL_in_place_32B();
	status += test_PDCP_PROTO_SGL_oop_32B_128B();
	status += test_PDCP_PROTO_SGL_oop_32B_40B();
	status += test_PDCP_PROTO_SGL_oop_128B_32B();
	status += test_PDCP_SDAP_PROTO_encap_all();
	status += test_PDCP_SDAP_PROTO_decap_all();
	status += test_PDCP_PROTO_short_mac();

	if (status)
		return TEST_FAILED;
	else
		return TEST_SUCCESS;
}

static int
test_ipsec_proto_ipv4_ttl_decrement(void)
{
	struct ipsec_test_flags flags = {
		.dec_ttl_or_hop_limit = true
	};

	return test_ipsec_proto_all(&flags);
}

static int
test_ipsec_proto_ipv6_hop_limit_decrement(void)
{
	struct ipsec_test_flags flags = {
		.ipv6 = true,
		.dec_ttl_or_hop_limit = true
	};

	return test_ipsec_proto_all(&flags);
}

static int
test_docsis_proto_uplink(const void *data)
{
	const struct docsis_test_data *d_td = data;
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	uint8_t *plaintext = NULL;
	uint8_t *ciphertext = NULL;
	uint8_t *iv_ptr;
	int32_t cipher_len, crc_len;
	uint32_t crc_data_len;
	int ret = TEST_SUCCESS;

	void *ctx = rte_cryptodev_get_sec_ctx(ts_params->valid_devs[0]);

	/* Verify the capabilities */
	struct rte_security_capability_idx sec_cap_idx;
	const struct rte_security_capability *sec_cap;
	const struct rte_cryptodev_capabilities *crypto_cap;
	const struct rte_cryptodev_symmetric_capability *sym_cap;
	int j = 0;

	/* Set action type */
	ut_params->type = gbl_action_type == RTE_SECURITY_ACTION_TYPE_NONE ?
		RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL :
		gbl_action_type;

	if (security_proto_supported(ut_params->type,
			RTE_SECURITY_PROTOCOL_DOCSIS) < 0)
		return TEST_SKIPPED;

	sec_cap_idx.action = ut_params->type;
	sec_cap_idx.protocol = RTE_SECURITY_PROTOCOL_DOCSIS;
	sec_cap_idx.docsis.direction = RTE_SECURITY_DOCSIS_UPLINK;

	sec_cap = rte_security_capability_get(ctx, &sec_cap_idx);
	if (sec_cap == NULL)
		return TEST_SKIPPED;

	while ((crypto_cap = &sec_cap->crypto_capabilities[j++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (crypto_cap->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
				crypto_cap->sym.xform_type ==
					RTE_CRYPTO_SYM_XFORM_CIPHER &&
				crypto_cap->sym.cipher.algo ==
					RTE_CRYPTO_CIPHER_AES_DOCSISBPI) {
			sym_cap = &crypto_cap->sym;
			if (rte_cryptodev_sym_capability_check_cipher(sym_cap,
						d_td->key.len,
						d_td->iv.len) == 0)
				break;
		}
	}

	if (crypto_cap->op == RTE_CRYPTO_OP_TYPE_UNDEFINED)
		return TEST_SKIPPED;

	/* Setup source mbuf payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			d_td->ciphertext.len);

	memcpy(ciphertext, d_td->ciphertext.data, d_td->ciphertext.len);

	/* Setup cipher session parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_DOCSISBPI;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	ut_params->cipher_xform.cipher.key.data = d_td->key.data;
	ut_params->cipher_xform.cipher.key.length = d_td->key.len;
	ut_params->cipher_xform.cipher.iv.length = d_td->iv.len;
	ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
	ut_params->cipher_xform.next = NULL;

	/* Setup DOCSIS session parameters */
	ut_params->docsis_xform.direction = RTE_SECURITY_DOCSIS_UPLINK;

	struct rte_security_session_conf sess_conf = {
		.action_type = ut_params->type,
		.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,
		.docsis = ut_params->docsis_xform,
		.crypto_xform = &ut_params->cipher_xform,
	};

	/* Create security session */
	ut_params->sec_session = rte_security_session_create(ctx, &sess_conf,
					ts_params->session_mpool);

	if (!ut_params->sec_session) {
		printf("Test function %s line %u: failed to allocate session\n",
			__func__, __LINE__);
		ret = TEST_FAILED;
		goto on_err;
	}

	/* Generate crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (!ut_params->op) {
		printf("Test function %s line %u: failed to allocate symmetric "
			"crypto operation\n", __func__, __LINE__);
		ret = TEST_FAILED;
		goto on_err;
	}

	/* Setup CRC operation parameters */
	crc_len = d_td->ciphertext.no_crc == false ?
			(d_td->ciphertext.len -
				d_td->ciphertext.crc_offset -
				RTE_ETHER_CRC_LEN) :
			0;
	crc_len = crc_len > 0 ? crc_len : 0;
	crc_data_len = crc_len == 0 ? 0 : RTE_ETHER_CRC_LEN;
	ut_params->op->sym->auth.data.length = crc_len;
	ut_params->op->sym->auth.data.offset = d_td->ciphertext.crc_offset;

	/* Setup cipher operation parameters */
	cipher_len = d_td->ciphertext.no_cipher == false ?
			(d_td->ciphertext.len -
				d_td->ciphertext.cipher_offset) :
			0;
	cipher_len = cipher_len > 0 ? cipher_len : 0;
	ut_params->op->sym->cipher.data.length = cipher_len;
	ut_params->op->sym->cipher.data.offset = d_td->ciphertext.cipher_offset;

	/* Setup cipher IV */
	iv_ptr = (uint8_t *)ut_params->op + IV_OFFSET;
	rte_memcpy(iv_ptr, d_td->iv.data, d_td->iv.len);

	/* Attach session to operation */
	rte_security_attach_session(ut_params->op, ut_params->sec_session);

	/* Set crypto operation mbufs */
	ut_params->op->sym->m_src = ut_params->ibuf;
	ut_params->op->sym->m_dst = NULL;

	/* Process crypto operation */
	if (process_crypto_request(ts_params->valid_devs[0], ut_params->op) ==
			NULL) {
		printf("Test function %s line %u: failed to process security "
			"crypto op\n", __func__, __LINE__);
		ret = TEST_FAILED;
		goto on_err;
	}

	if (ut_params->op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		printf("Test function %s line %u: failed to process crypto op\n",
			__func__, __LINE__);
		ret = TEST_FAILED;
		goto on_err;
	}

	/* Validate plaintext */
	plaintext = ciphertext;

	if (memcmp(plaintext, d_td->plaintext.data,
			d_td->plaintext.len - crc_data_len)) {
		printf("Test function %s line %u: plaintext not as expected\n",
			__func__, __LINE__);
		rte_hexdump(stdout, "expected", d_td->plaintext.data,
				d_td->plaintext.len);
		rte_hexdump(stdout, "actual", plaintext, d_td->plaintext.len);
		ret = TEST_FAILED;
		goto on_err;
	}

on_err:
	rte_crypto_op_free(ut_params->op);
	ut_params->op = NULL;

	if (ut_params->sec_session)
		rte_security_session_destroy(ctx, ut_params->sec_session);
	ut_params->sec_session = NULL;

	rte_pktmbuf_free(ut_params->ibuf);
	ut_params->ibuf = NULL;

	return ret;
}

static int
test_docsis_proto_downlink(const void *data)
{
	const struct docsis_test_data *d_td = data;
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	uint8_t *plaintext = NULL;
	uint8_t *ciphertext = NULL;
	uint8_t *iv_ptr;
	int32_t cipher_len, crc_len;
	int ret = TEST_SUCCESS;

	void *ctx = rte_cryptodev_get_sec_ctx(ts_params->valid_devs[0]);

	/* Verify the capabilities */
	struct rte_security_capability_idx sec_cap_idx;
	const struct rte_security_capability *sec_cap;
	const struct rte_cryptodev_capabilities *crypto_cap;
	const struct rte_cryptodev_symmetric_capability *sym_cap;
	int j = 0;

	/* Set action type */
	ut_params->type = gbl_action_type == RTE_SECURITY_ACTION_TYPE_NONE ?
		RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL :
		gbl_action_type;

	if (security_proto_supported(ut_params->type,
			RTE_SECURITY_PROTOCOL_DOCSIS) < 0)
		return TEST_SKIPPED;

	sec_cap_idx.action = ut_params->type;
	sec_cap_idx.protocol = RTE_SECURITY_PROTOCOL_DOCSIS;
	sec_cap_idx.docsis.direction = RTE_SECURITY_DOCSIS_DOWNLINK;

	sec_cap = rte_security_capability_get(ctx, &sec_cap_idx);
	if (sec_cap == NULL)
		return TEST_SKIPPED;

	while ((crypto_cap = &sec_cap->crypto_capabilities[j++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (crypto_cap->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
				crypto_cap->sym.xform_type ==
					RTE_CRYPTO_SYM_XFORM_CIPHER &&
				crypto_cap->sym.cipher.algo ==
					RTE_CRYPTO_CIPHER_AES_DOCSISBPI) {
			sym_cap = &crypto_cap->sym;
			if (rte_cryptodev_sym_capability_check_cipher(sym_cap,
						d_td->key.len,
						d_td->iv.len) == 0)
				break;
		}
	}

	if (crypto_cap->op == RTE_CRYPTO_OP_TYPE_UNDEFINED)
		return TEST_SKIPPED;

	/* Setup source mbuf payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			d_td->plaintext.len);

	memcpy(plaintext, d_td->plaintext.data, d_td->plaintext.len);

	/* Setup cipher session parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_DOCSISBPI;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	ut_params->cipher_xform.cipher.key.data = d_td->key.data;
	ut_params->cipher_xform.cipher.key.length = d_td->key.len;
	ut_params->cipher_xform.cipher.iv.length = d_td->iv.len;
	ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
	ut_params->cipher_xform.next = NULL;

	/* Setup DOCSIS session parameters */
	ut_params->docsis_xform.direction = RTE_SECURITY_DOCSIS_DOWNLINK;

	struct rte_security_session_conf sess_conf = {
		.action_type = ut_params->type,
		.protocol = RTE_SECURITY_PROTOCOL_DOCSIS,
		.docsis = ut_params->docsis_xform,
		.crypto_xform = &ut_params->cipher_xform,
	};

	/* Create security session */
	ut_params->sec_session = rte_security_session_create(ctx, &sess_conf,
					ts_params->session_mpool);

	if (!ut_params->sec_session) {
		printf("Test function %s line %u: failed to allocate session\n",
			__func__, __LINE__);
		ret = TEST_FAILED;
		goto on_err;
	}

	/* Generate crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
				RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	if (!ut_params->op) {
		printf("Test function %s line %u: failed to allocate symmetric "
			"crypto operation\n", __func__, __LINE__);
		ret = TEST_FAILED;
		goto on_err;
	}

	/* Setup CRC operation parameters */
	crc_len = d_td->plaintext.no_crc == false ?
			(d_td->plaintext.len -
				d_td->plaintext.crc_offset -
				RTE_ETHER_CRC_LEN) :
			0;
	crc_len = crc_len > 0 ? crc_len : 0;
	ut_params->op->sym->auth.data.length = crc_len;
	ut_params->op->sym->auth.data.offset = d_td->plaintext.crc_offset;

	/* Setup cipher operation parameters */
	cipher_len = d_td->plaintext.no_cipher == false ?
			(d_td->plaintext.len -
				d_td->plaintext.cipher_offset) :
			0;
	cipher_len = cipher_len > 0 ? cipher_len : 0;
	ut_params->op->sym->cipher.data.length = cipher_len;
	ut_params->op->sym->cipher.data.offset = d_td->plaintext.cipher_offset;

	/* Setup cipher IV */
	iv_ptr = (uint8_t *)ut_params->op + IV_OFFSET;
	rte_memcpy(iv_ptr, d_td->iv.data, d_td->iv.len);

	/* Attach session to operation */
	rte_security_attach_session(ut_params->op, ut_params->sec_session);

	/* Set crypto operation mbufs */
	ut_params->op->sym->m_src = ut_params->ibuf;
	ut_params->op->sym->m_dst = NULL;

	/* Process crypto operation */
	if (process_crypto_request(ts_params->valid_devs[0], ut_params->op) ==
			NULL) {
		printf("Test function %s line %u: failed to process crypto op\n",
			__func__, __LINE__);
		ret = TEST_FAILED;
		goto on_err;
	}

	if (ut_params->op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		printf("Test function %s line %u: crypto op processing failed\n",
			__func__, __LINE__);
		ret = TEST_FAILED;
		goto on_err;
	}

	/* Validate ciphertext */
	ciphertext = plaintext;

	if (memcmp(ciphertext, d_td->ciphertext.data, d_td->ciphertext.len)) {
		printf("Test function %s line %u: plaintext not as expected\n",
			__func__, __LINE__);
		rte_hexdump(stdout, "expected", d_td->ciphertext.data,
				d_td->ciphertext.len);
		rte_hexdump(stdout, "actual", ciphertext, d_td->ciphertext.len);
		ret = TEST_FAILED;
		goto on_err;
	}

on_err:
	rte_crypto_op_free(ut_params->op);
	ut_params->op = NULL;

	if (ut_params->sec_session)
		rte_security_session_destroy(ctx, ut_params->sec_session);
	ut_params->sec_session = NULL;

	rte_pktmbuf_free(ut_params->ibuf);
	ut_params->ibuf = NULL;

	return ret;
}
#endif

static int
test_AES_GCM_authenticated_encryption_test_case_1(void)
{
	return test_authenticated_encryption(&gcm_test_case_1);
}

static int
test_AES_GCM_authenticated_encryption_test_case_2(void)
{
	return test_authenticated_encryption(&gcm_test_case_2);
}

static int
test_AES_GCM_authenticated_encryption_test_case_3(void)
{
	return test_authenticated_encryption(&gcm_test_case_3);
}

static int
test_AES_GCM_authenticated_encryption_test_case_4(void)
{
	return test_authenticated_encryption(&gcm_test_case_4);
}

static int
test_AES_GCM_authenticated_encryption_test_case_5(void)
{
	return test_authenticated_encryption(&gcm_test_case_5);
}

static int
test_AES_GCM_authenticated_encryption_test_case_6(void)
{
	return test_authenticated_encryption(&gcm_test_case_6);
}

static int
test_AES_GCM_authenticated_encryption_test_case_7(void)
{
	return test_authenticated_encryption(&gcm_test_case_7);
}

static int
test_AES_GCM_authenticated_encryption_test_case_8(void)
{
	return test_authenticated_encryption(&gcm_test_case_8);
}

static int
test_AES_GCM_J0_authenticated_encryption_test_case_1(void)
{
	return test_authenticated_encryption(&gcm_J0_test_case_1);
}

static int
test_AES_GCM_auth_encryption_test_case_192_1(void)
{
	return test_authenticated_encryption(&gcm_test_case_192_1);
}

static int
test_AES_GCM_auth_encryption_test_case_192_2(void)
{
	return test_authenticated_encryption(&gcm_test_case_192_2);
}

static int
test_AES_GCM_auth_encryption_test_case_192_3(void)
{
	return test_authenticated_encryption(&gcm_test_case_192_3);
}

static int
test_AES_GCM_auth_encryption_test_case_192_4(void)
{
	return test_authenticated_encryption(&gcm_test_case_192_4);
}

static int
test_AES_GCM_auth_encryption_test_case_192_5(void)
{
	return test_authenticated_encryption(&gcm_test_case_192_5);
}

static int
test_AES_GCM_auth_encryption_test_case_192_6(void)
{
	return test_authenticated_encryption(&gcm_test_case_192_6);
}

static int
test_AES_GCM_auth_encryption_test_case_192_7(void)
{
	return test_authenticated_encryption(&gcm_test_case_192_7);
}

static int
test_AES_GCM_auth_encryption_test_case_256_1(void)
{
	return test_authenticated_encryption(&gcm_test_case_256_1);
}

static int
test_AES_GCM_auth_encryption_test_case_256_2(void)
{
	return test_authenticated_encryption(&gcm_test_case_256_2);
}

static int
test_AES_GCM_auth_encryption_test_case_256_3(void)
{
	return test_authenticated_encryption(&gcm_test_case_256_3);
}

static int
test_AES_GCM_auth_encryption_test_case_256_4(void)
{
	return test_authenticated_encryption(&gcm_test_case_256_4);
}

static int
test_AES_GCM_auth_encryption_test_case_256_5(void)
{
	return test_authenticated_encryption(&gcm_test_case_256_5);
}

static int
test_AES_GCM_auth_encryption_test_case_256_6(void)
{
	return test_authenticated_encryption(&gcm_test_case_256_6);
}

static int
test_AES_GCM_auth_encryption_test_case_256_7(void)
{
	return test_authenticated_encryption(&gcm_test_case_256_7);
}

static int
test_AES_GCM_auth_encryption_test_case_aad_1(void)
{
	return test_authenticated_encryption(&gcm_test_case_aad_1);
}

static int
test_AES_GCM_auth_encryption_test_case_aad_2(void)
{
	return test_authenticated_encryption(&gcm_test_case_aad_2);
}

static int
test_AES_GCM_auth_encryption_fail_iv_corrupt(void)
{
	struct aead_test_data tdata;
	int res;

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");
	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	tdata.iv.data[0] += 1;
	res = test_authenticated_encryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "encryption not failed");
	return TEST_SUCCESS;
}

static int
test_AES_GCM_auth_encryption_fail_in_data_corrupt(void)
{
	struct aead_test_data tdata;
	int res;

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");
	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	tdata.plaintext.data[0] += 1;
	res = test_authenticated_encryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "encryption not failed");
	return TEST_SUCCESS;
}

static int
test_AES_GCM_auth_encryption_fail_out_data_corrupt(void)
{
	struct aead_test_data tdata;
	int res;

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");
	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	tdata.ciphertext.data[0] += 1;
	res = test_authenticated_encryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "encryption not failed");
	return TEST_SUCCESS;
}

static int
test_AES_GCM_auth_encryption_fail_aad_len_corrupt(void)
{
	struct aead_test_data tdata;
	int res;

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");
	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	tdata.aad.len += 1;
	res = test_authenticated_encryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "encryption not failed");
	return TEST_SUCCESS;
}

static int
test_AES_GCM_auth_encryption_fail_aad_corrupt(void)
{
	struct aead_test_data tdata;
	uint8_t aad[gcm_test_case_7.aad.len];
	int res;

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");
	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	memcpy(aad, gcm_test_case_7.aad.data, gcm_test_case_7.aad.len);
	aad[0] += 1;
	tdata.aad.data = aad;
	res = test_authenticated_encryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "encryption not failed");
	return TEST_SUCCESS;
}

static int
test_AES_GCM_auth_encryption_fail_tag_corrupt(void)
{
	struct aead_test_data tdata;
	int res;

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");
	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	tdata.auth_tag.data[0] += 1;
	res = test_authenticated_encryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "encryption not failed");
	return TEST_SUCCESS;
}

static int
test_authenticated_decryption(const struct aead_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext;
	uint32_t i;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	const struct rte_cryptodev_symmetric_capability *capability;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AEAD;
	cap_idx.algo.aead = tdata->algo;
	capability = rte_cryptodev_sym_capability_get(
			ts_params->valid_devs[0], &cap_idx);
	if (capability == NULL)
		return TEST_SKIPPED;
	if (rte_cryptodev_sym_capability_check_aead(
			capability, tdata->key.len, tdata->auth_tag.len,
			tdata->aad.len, tdata->iv.len))
		return TEST_SKIPPED;

	/* Create AEAD session */
	retval = create_aead_session(ts_params->valid_devs[0],
			tdata->algo,
			RTE_CRYPTO_AEAD_OP_DECRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			tdata->iv.len);
	if (retval != TEST_SUCCESS)
		return retval;

	/* alloc mbuf and set payload */
	if (tdata->aad.len > MBUF_SIZE) {
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->large_mbuf_pool);
		/* Populate full size of add data */
		for (i = 32; i < MAX_AAD_LENGTH; i += 32)
			memcpy(&tdata->aad.data[i], &tdata->aad.data[0], 32);
	} else
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	/* Create AEAD operation */
	retval = create_aead_operation(RTE_CRYPTO_AEAD_OP_DECRYPT, tdata);
	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;

	/* Process crypto operation */
	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_aead_op(ts_params->valid_devs[0], ut_params->op);
	else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 0, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		TEST_ASSERT_NOT_NULL(
			process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	if (ut_params->op->sym->m_dst)
		plaintext = rte_pktmbuf_mtod(ut_params->op->sym->m_dst,
				uint8_t *);
	else
		plaintext = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_src,
				uint8_t *,
				ut_params->op->sym->cipher.data.offset);

	debug_hexdump(stdout, "plaintext:", plaintext, tdata->ciphertext.len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			plaintext,
			tdata->plaintext.data,
			tdata->plaintext.len,
			"Plaintext data not as expected");

	TEST_ASSERT_EQUAL(ut_params->op->status,
			RTE_CRYPTO_OP_STATUS_SUCCESS,
			"Authentication failed");

	return 0;
}

static int
test_AES_GCM_authenticated_decryption_test_case_1(void)
{
	return test_authenticated_decryption(&gcm_test_case_1);
}

static int
test_AES_GCM_authenticated_decryption_test_case_2(void)
{
	return test_authenticated_decryption(&gcm_test_case_2);
}

static int
test_AES_GCM_authenticated_decryption_test_case_3(void)
{
	return test_authenticated_decryption(&gcm_test_case_3);
}

static int
test_AES_GCM_authenticated_decryption_test_case_4(void)
{
	return test_authenticated_decryption(&gcm_test_case_4);
}

static int
test_AES_GCM_authenticated_decryption_test_case_5(void)
{
	return test_authenticated_decryption(&gcm_test_case_5);
}

static int
test_AES_GCM_authenticated_decryption_test_case_6(void)
{
	return test_authenticated_decryption(&gcm_test_case_6);
}

static int
test_AES_GCM_authenticated_decryption_test_case_7(void)
{
	return test_authenticated_decryption(&gcm_test_case_7);
}

static int
test_AES_GCM_authenticated_decryption_test_case_8(void)
{
	return test_authenticated_decryption(&gcm_test_case_8);
}

static int
test_AES_GCM_J0_authenticated_decryption_test_case_1(void)
{
	return test_authenticated_decryption(&gcm_J0_test_case_1);
}

static int
test_AES_GCM_auth_decryption_test_case_192_1(void)
{
	return test_authenticated_decryption(&gcm_test_case_192_1);
}

static int
test_AES_GCM_auth_decryption_test_case_192_2(void)
{
	return test_authenticated_decryption(&gcm_test_case_192_2);
}

static int
test_AES_GCM_auth_decryption_test_case_192_3(void)
{
	return test_authenticated_decryption(&gcm_test_case_192_3);
}

static int
test_AES_GCM_auth_decryption_test_case_192_4(void)
{
	return test_authenticated_decryption(&gcm_test_case_192_4);
}

static int
test_AES_GCM_auth_decryption_test_case_192_5(void)
{
	return test_authenticated_decryption(&gcm_test_case_192_5);
}

static int
test_AES_GCM_auth_decryption_test_case_192_6(void)
{
	return test_authenticated_decryption(&gcm_test_case_192_6);
}

static int
test_AES_GCM_auth_decryption_test_case_192_7(void)
{
	return test_authenticated_decryption(&gcm_test_case_192_7);
}

static int
test_AES_GCM_auth_decryption_test_case_256_1(void)
{
	return test_authenticated_decryption(&gcm_test_case_256_1);
}

static int
test_AES_GCM_auth_decryption_test_case_256_2(void)
{
	return test_authenticated_decryption(&gcm_test_case_256_2);
}

static int
test_AES_GCM_auth_decryption_test_case_256_3(void)
{
	return test_authenticated_decryption(&gcm_test_case_256_3);
}

static int
test_AES_GCM_auth_decryption_test_case_256_4(void)
{
	return test_authenticated_decryption(&gcm_test_case_256_4);
}

static int
test_AES_GCM_auth_decryption_test_case_256_5(void)
{
	return test_authenticated_decryption(&gcm_test_case_256_5);
}

static int
test_AES_GCM_auth_decryption_test_case_256_6(void)
{
	return test_authenticated_decryption(&gcm_test_case_256_6);
}

static int
test_AES_GCM_auth_decryption_test_case_256_7(void)
{
	return test_authenticated_decryption(&gcm_test_case_256_7);
}

static int
test_AES_GCM_auth_decryption_test_case_aad_1(void)
{
	return test_authenticated_decryption(&gcm_test_case_aad_1);
}

static int
test_AES_GCM_auth_decryption_test_case_aad_2(void)
{
	return test_authenticated_decryption(&gcm_test_case_aad_2);
}

static int
test_AES_GCM_auth_decryption_fail_iv_corrupt(void)
{
	struct aead_test_data tdata;
	int res;

	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	tdata.iv.data[0] += 1;
	res = test_authenticated_decryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "decryption not failed");
	return TEST_SUCCESS;
}

static int
test_AES_GCM_auth_decryption_fail_in_data_corrupt(void)
{
	struct aead_test_data tdata;
	int res;

	RTE_LOG(INFO, USER1, "This is a negative test, errors are expected\n");
	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	tdata.plaintext.data[0] += 1;
	res = test_authenticated_decryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "decryption not failed");
	return TEST_SUCCESS;
}

static int
test_AES_GCM_auth_decryption_fail_out_data_corrupt(void)
{
	struct aead_test_data tdata;
	int res;

	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	tdata.ciphertext.data[0] += 1;
	res = test_authenticated_decryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "decryption not failed");
	return TEST_SUCCESS;
}

static int
test_AES_GCM_auth_decryption_fail_aad_len_corrupt(void)
{
	struct aead_test_data tdata;
	int res;

	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	tdata.aad.len += 1;
	res = test_authenticated_decryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "decryption not failed");
	return TEST_SUCCESS;
}

static int
test_AES_GCM_auth_decryption_fail_aad_corrupt(void)
{
	struct aead_test_data tdata;
	uint8_t aad[gcm_test_case_7.aad.len];
	int res;

	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	memcpy(aad, gcm_test_case_7.aad.data, gcm_test_case_7.aad.len);
	aad[0] += 1;
	tdata.aad.data = aad;
	res = test_authenticated_decryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "decryption not failed");
	return TEST_SUCCESS;
}

static int
test_AES_GCM_auth_decryption_fail_tag_corrupt(void)
{
	struct aead_test_data tdata;
	int res;

	memcpy(&tdata, &gcm_test_case_7, sizeof(struct aead_test_data));
	tdata.auth_tag.data[0] += 1;
	res = test_authenticated_decryption(&tdata);
	if (res == TEST_SKIPPED)
		return res;
	TEST_ASSERT_EQUAL(res, TEST_FAILED, "authentication not failed");
	return TEST_SUCCESS;
}

static int
test_authenticated_encryption_oop(const struct aead_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *ciphertext, *auth_tag;
	uint16_t plaintext_pad_len;
	struct rte_cryptodev_info dev_info;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AEAD;
	cap_idx.algo.aead = tdata->algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP)))
		return TEST_SKIPPED;

	/* not supported with CPU crypto */
	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Create AEAD session */
	retval = create_aead_session(ts_params->valid_devs[0],
			tdata->algo,
			RTE_CRYPTO_AEAD_OP_ENCRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			tdata->iv.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));
	memset(rte_pktmbuf_mtod(ut_params->obuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->obuf));

	/* Create AEAD operation */
	retval = create_aead_operation(RTE_CRYPTO_AEAD_OP_ENCRYPT, tdata);
	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;
	ut_params->op->sym->m_dst = ut_params->obuf;

	/* Process crypto operation */
	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 0, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);

	ciphertext = rte_pktmbuf_mtod_offset(ut_params->obuf, uint8_t *,
			ut_params->op->sym->cipher.data.offset);
	auth_tag = ciphertext + plaintext_pad_len;

	debug_hexdump(stdout, "ciphertext:", ciphertext, tdata->ciphertext.len);
	debug_hexdump(stdout, "auth tag:", auth_tag, tdata->auth_tag.len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ciphertext,
			tdata->ciphertext.data,
			tdata->ciphertext.len,
			"Ciphertext data not as expected");

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			tdata->auth_tag.data,
			tdata->auth_tag.len,
			"Generated auth tag not as expected");

	return 0;

}

static int
test_AES_GCM_authenticated_encryption_oop_test_case_1(void)
{
	return test_authenticated_encryption_oop(&gcm_test_case_5);
}

static int
test_authenticated_decryption_oop(const struct aead_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AEAD;
	cap_idx.algo.aead = tdata->algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* not supported with CPU crypto and raw data-path APIs*/
	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO ||
			global_api_test_type == CRYPTODEV_RAW_API_TEST)
		return TEST_SKIPPED;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device does not support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* Create AEAD session */
	retval = create_aead_session(ts_params->valid_devs[0],
			tdata->algo,
			RTE_CRYPTO_AEAD_OP_DECRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			tdata->iv.len);
	if (retval < 0)
		return retval;

	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));
	memset(rte_pktmbuf_mtod(ut_params->obuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->obuf));

	/* Create AEAD operation */
	retval = create_aead_operation(RTE_CRYPTO_AEAD_OP_DECRYPT, tdata);
	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;
	ut_params->op->sym->m_dst = ut_params->obuf;

	/* Process crypto operation */
	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 0, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	plaintext = rte_pktmbuf_mtod_offset(ut_params->obuf, uint8_t *,
			ut_params->op->sym->cipher.data.offset);

	debug_hexdump(stdout, "plaintext:", plaintext, tdata->ciphertext.len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			plaintext,
			tdata->plaintext.data,
			tdata->plaintext.len,
			"Plaintext data not as expected");

	TEST_ASSERT_EQUAL(ut_params->op->status,
			RTE_CRYPTO_OP_STATUS_SUCCESS,
			"Authentication failed");
	return 0;
}

static int
test_AES_GCM_authenticated_decryption_oop_test_case_1(void)
{
	return test_authenticated_decryption_oop(&gcm_test_case_5);
}

static int
test_authenticated_encryption_sessionless(
		const struct aead_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *ciphertext, *auth_tag;
	uint16_t plaintext_pad_len;
	uint8_t key[tdata->key.len + 1];
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_SYM_SESSIONLESS)) {
		printf("Device doesn't support Sessionless ops.\n");
		return TEST_SKIPPED;
	}

	/* not supported with CPU crypto */
	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AEAD;
	cap_idx.algo.aead = tdata->algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	/* Create AEAD operation */
	retval = create_aead_operation(RTE_CRYPTO_AEAD_OP_ENCRYPT, tdata);
	if (retval < 0)
		return retval;

	/* Create GCM xform */
	memcpy(key, tdata->key.data, tdata->key.len);
	retval = create_aead_xform(ut_params->op,
			tdata->algo,
			RTE_CRYPTO_AEAD_OP_ENCRYPT,
			key, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			tdata->iv.len);
	if (retval < 0)
		return retval;

	ut_params->op->sym->m_src = ut_params->ibuf;

	TEST_ASSERT_EQUAL(ut_params->op->sess_type,
			RTE_CRYPTO_OP_SESSIONLESS,
			"crypto op session type not sessionless");

	/* Process crypto operation */
	TEST_ASSERT_NOT_NULL(process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed crypto process");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op status not success");

	plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);

	ciphertext = rte_pktmbuf_mtod_offset(ut_params->ibuf, uint8_t *,
			ut_params->op->sym->cipher.data.offset);
	auth_tag = ciphertext + plaintext_pad_len;

	debug_hexdump(stdout, "ciphertext:", ciphertext, tdata->ciphertext.len);
	debug_hexdump(stdout, "auth tag:", auth_tag, tdata->auth_tag.len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ciphertext,
			tdata->ciphertext.data,
			tdata->ciphertext.len,
			"Ciphertext data not as expected");

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			tdata->auth_tag.data,
			tdata->auth_tag.len,
			"Generated auth tag not as expected");

	return 0;

}

static int
test_AES_GCM_authenticated_encryption_sessionless_test_case_1(void)
{
	return test_authenticated_encryption_sessionless(
			&gcm_test_case_5);
}

static int
test_authenticated_decryption_sessionless(
		const struct aead_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	int retval;
	uint8_t *plaintext;
	uint8_t key[tdata->key.len + 1];
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if (!(feat_flags & RTE_CRYPTODEV_FF_SYM_SESSIONLESS)) {
		printf("Device doesn't support Sessionless ops.\n");
		return TEST_SKIPPED;
	}

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* not supported with CPU crypto */
	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AEAD;
	cap_idx.algo.aead = tdata->algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* alloc mbuf and set payload */
	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	/* Create AEAD operation */
	retval = create_aead_operation(RTE_CRYPTO_AEAD_OP_DECRYPT, tdata);
	if (retval < 0)
		return retval;

	/* Create AEAD xform */
	memcpy(key, tdata->key.data, tdata->key.len);
	retval = create_aead_xform(ut_params->op,
			tdata->algo,
			RTE_CRYPTO_AEAD_OP_DECRYPT,
			key, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			tdata->iv.len);
	if (retval < 0)
		return retval;

	ut_params->op->sym->m_src = ut_params->ibuf;

	TEST_ASSERT_EQUAL(ut_params->op->sess_type,
			RTE_CRYPTO_OP_SESSIONLESS,
			"crypto op session type not sessionless");

	/* Process crypto operation */
	if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 0, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		TEST_ASSERT_NOT_NULL(process_crypto_request(
			ts_params->valid_devs[0], ut_params->op),
				"failed to process sym crypto op");

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed crypto process");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op status not success");

	plaintext = rte_pktmbuf_mtod_offset(ut_params->ibuf, uint8_t *,
			ut_params->op->sym->cipher.data.offset);

	debug_hexdump(stdout, "plaintext:", plaintext, tdata->ciphertext.len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			plaintext,
			tdata->plaintext.data,
			tdata->plaintext.len,
			"Plaintext data not as expected");

	TEST_ASSERT_EQUAL(ut_params->op->status,
			RTE_CRYPTO_OP_STATUS_SUCCESS,
			"Authentication failed");
	return 0;
}

static int
test_AES_GCM_authenticated_decryption_sessionless_test_case_1(void)
{
	return test_authenticated_decryption_sessionless(
			&gcm_test_case_5);
}

static int
test_AES_CCM_authenticated_encryption_test_case_128_1(void)
{
	return test_authenticated_encryption(&ccm_test_case_128_1);
}

static int
test_AES_CCM_authenticated_encryption_test_case_128_2(void)
{
	return test_authenticated_encryption(&ccm_test_case_128_2);
}

static int
test_AES_CCM_authenticated_encryption_test_case_128_3(void)
{
	return test_authenticated_encryption(&ccm_test_case_128_3);
}

static int
test_AES_CCM_authenticated_decryption_test_case_128_1(void)
{
	return test_authenticated_decryption(&ccm_test_case_128_1);
}

static int
test_AES_CCM_authenticated_decryption_test_case_128_2(void)
{
	return test_authenticated_decryption(&ccm_test_case_128_2);
}

static int
test_AES_CCM_authenticated_decryption_test_case_128_3(void)
{
	return test_authenticated_decryption(&ccm_test_case_128_3);
}

static int
test_AES_CCM_authenticated_encryption_test_case_192_1(void)
{
	return test_authenticated_encryption(&ccm_test_case_192_1);
}

static int
test_AES_CCM_authenticated_encryption_test_case_192_2(void)
{
	return test_authenticated_encryption(&ccm_test_case_192_2);
}

static int
test_AES_CCM_authenticated_encryption_test_case_192_3(void)
{
	return test_authenticated_encryption(&ccm_test_case_192_3);
}

static int
test_AES_CCM_authenticated_decryption_test_case_192_1(void)
{
	return test_authenticated_decryption(&ccm_test_case_192_1);
}

static int
test_AES_CCM_authenticated_decryption_test_case_192_2(void)
{
	return test_authenticated_decryption(&ccm_test_case_192_2);
}

static int
test_AES_CCM_authenticated_decryption_test_case_192_3(void)
{
	return test_authenticated_decryption(&ccm_test_case_192_3);
}

static int
test_AES_CCM_authenticated_encryption_test_case_256_1(void)
{
	return test_authenticated_encryption(&ccm_test_case_256_1);
}

static int
test_AES_CCM_authenticated_encryption_test_case_256_2(void)
{
	return test_authenticated_encryption(&ccm_test_case_256_2);
}

static int
test_AES_CCM_authenticated_encryption_test_case_256_3(void)
{
	return test_authenticated_encryption(&ccm_test_case_256_3);
}

static int
test_AES_CCM_authenticated_decryption_test_case_256_1(void)
{
	return test_authenticated_decryption(&ccm_test_case_256_1);
}

static int
test_AES_CCM_authenticated_decryption_test_case_256_2(void)
{
	return test_authenticated_decryption(&ccm_test_case_256_2);
}

static int
test_AES_CCM_authenticated_decryption_test_case_256_3(void)
{
	return test_authenticated_decryption(&ccm_test_case_256_3);
}

static int
test_stats(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_stats stats;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_SHA1_HMAC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_AES_CBC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (rte_cryptodev_stats_get(ts_params->valid_devs[0], &stats)
			== -ENOTSUP)
		return TEST_SKIPPED;

	rte_cryptodev_stats_reset(ts_params->valid_devs[0]);
	TEST_ASSERT((rte_cryptodev_stats_get(ts_params->valid_devs[0] + 600,
			&stats) == -ENODEV),
		"rte_cryptodev_stats_get invalid dev failed");
	TEST_ASSERT((rte_cryptodev_stats_get(ts_params->valid_devs[0], 0) != 0),
		"rte_cryptodev_stats_get invalid Param failed");

	/* Test expected values */
	test_AES_CBC_HMAC_SHA1_encrypt_digest();
	TEST_ASSERT_SUCCESS(rte_cryptodev_stats_get(ts_params->valid_devs[0],
			&stats),
		"rte_cryptodev_stats_get failed");
	TEST_ASSERT((stats.enqueued_count == 1),
		"rte_cryptodev_stats_get returned unexpected enqueued stat");
	TEST_ASSERT((stats.dequeued_count == 1),
		"rte_cryptodev_stats_get returned unexpected dequeued stat");
	TEST_ASSERT((stats.enqueue_err_count == 0),
		"rte_cryptodev_stats_get returned unexpected enqueued error count stat");
	TEST_ASSERT((stats.dequeue_err_count == 0),
		"rte_cryptodev_stats_get returned unexpected dequeued error count stat");

	/* invalid device but should ignore and not reset device stats*/
	rte_cryptodev_stats_reset(ts_params->valid_devs[0] + 300);
	TEST_ASSERT_SUCCESS(rte_cryptodev_stats_get(ts_params->valid_devs[0],
			&stats),
		"rte_cryptodev_stats_get failed");
	TEST_ASSERT((stats.enqueued_count == 1),
		"rte_cryptodev_stats_get returned unexpected enqueued stat after invalid reset");

	/* check that a valid reset clears stats */
	rte_cryptodev_stats_reset(ts_params->valid_devs[0]);
	TEST_ASSERT_SUCCESS(rte_cryptodev_stats_get(ts_params->valid_devs[0],
			&stats),
					  "rte_cryptodev_stats_get failed");
	TEST_ASSERT((stats.enqueued_count == 0),
		"rte_cryptodev_stats_get returned unexpected enqueued stat after valid reset");
	TEST_ASSERT((stats.dequeued_count == 0),
		"rte_cryptodev_stats_get returned unexpected dequeued stat after valid reset");

	return TEST_SUCCESS;
}

static int
test_device_reconfigure(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint16_t orig_nb_qps = ts_params->conf.nb_queue_pairs;
	struct rte_cryptodev_qp_conf qp_conf = {
		.nb_descriptors = MAX_NUM_OPS_INFLIGHT,
		.mp_session = ts_params->session_mpool
	};
	uint16_t qp_id, dev_id, num_devs = 0;

	TEST_ASSERT((num_devs = rte_cryptodev_count()) >= 1,
			"Need at least %d devices for test", 1);

	dev_id = ts_params->valid_devs[0];

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(dev_id);

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id, &ts_params->conf),
			"Failed test for rte_cryptodev_configure: "
			"dev_num %u", dev_id);

	/* Reconfigure with same configure params */
	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(dev_id, &ts_params->conf),
			"Failed test for rte_cryptodev_configure: "
			"dev_num %u", dev_id);

	/* Reconfigure with just one queue pair */
	ts_params->conf.nb_queue_pairs = 1;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev: dev_id %u, qp_id %u",
			ts_params->valid_devs[0], ts_params->conf.nb_queue_pairs);

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for "
				"rte_cryptodev_queue_pair_setup: num_inflights "
				"%u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	/* Reconfigure with max number of queue pairs */
	ts_params->conf.nb_queue_pairs = orig_nb_qps;

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev: dev_id %u, qp_id %u",
			ts_params->valid_devs[0], ts_params->conf.nb_queue_pairs);

	qp_conf.mp_session = ts_params->session_mpool;

	for (qp_id = 0; qp_id < ts_params->conf.nb_queue_pairs; qp_id++) {
		TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
				ts_params->valid_devs[0], qp_id, &qp_conf,
				rte_cryptodev_socket_id(
						ts_params->valid_devs[0])),
				"Failed test for "
				"rte_cryptodev_queue_pair_setup: num_inflights "
				"%u on qp %u on cryptodev %u",
				qp_conf.nb_descriptors, qp_id,
				ts_params->valid_devs[0]);
	}

	/* Start the device */
	TEST_ASSERT_SUCCESS(rte_cryptodev_start(ts_params->valid_devs[0]),
			"Failed to start cryptodev %u",
			ts_params->valid_devs[0]);

	/* Test expected values */
	return test_AES_CBC_HMAC_SHA1_encrypt_digest();
}

static int MD5_HMAC_create_session(struct crypto_testsuite_params *ts_params,
				   struct crypto_unittest_params *ut_params,
				   enum rte_crypto_auth_operation op,
				   const struct HMAC_MD5_vector *test_case)
{
	uint8_t key[64];

	memcpy(key, test_case->key.data, test_case->key.len);

	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;
	ut_params->auth_xform.auth.op = op;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_MD5_HMAC;

	ut_params->auth_xform.auth.digest_length = MD5_DIGEST_LEN;
	ut_params->auth_xform.auth.key.length = test_case->key.len;
	ut_params->auth_xform.auth.key.data = key;

	ut_params->sess = rte_cryptodev_sym_session_create(
		ts_params->valid_devs[0], &ut_params->auth_xform,
			ts_params->session_mpool);
	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	return 0;
}

static int MD5_HMAC_create_op(struct crypto_unittest_params *ut_params,
			      const struct HMAC_MD5_vector *test_case,
			      uint8_t **plaintext)
{
	uint16_t plaintext_pad_len;

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	plaintext_pad_len = RTE_ALIGN_CEIL(test_case->plaintext.len,
				16);

	*plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			plaintext_pad_len);
	memcpy(*plaintext, test_case->plaintext.data,
			test_case->plaintext.len);

	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, MD5_DIGEST_LEN);
	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append digest");
	sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
			ut_params->ibuf, plaintext_pad_len);

	if (ut_params->auth_xform.auth.op == RTE_CRYPTO_AUTH_OP_VERIFY) {
		rte_memcpy(sym_op->auth.digest.data, test_case->auth_tag.data,
			   test_case->auth_tag.len);
	}

	sym_op->auth.data.offset = 0;
	sym_op->auth.data.length = test_case->plaintext.len;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);
	ut_params->op->sym->m_src = ut_params->ibuf;

	return 0;
}

static int
test_MD5_HMAC_generate(const struct HMAC_MD5_vector *test_case)
{
	uint16_t plaintext_pad_len;
	uint8_t *plaintext, *auth_tag;
	int ret;

	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_MD5_HMAC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (MD5_HMAC_create_session(ts_params, ut_params,
			RTE_CRYPTO_AUTH_OP_GENERATE, test_case))
		return TEST_FAILED;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	plaintext_pad_len = RTE_ALIGN_CEIL(test_case->plaintext.len,
				16);

	if (MD5_HMAC_create_op(ut_params, test_case, &plaintext))
		return TEST_FAILED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
	else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		ret = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 1, 0, 0);
		if (ret != TEST_SUCCESS)
			return ret;
	} else
		TEST_ASSERT_NOT_NULL(
			process_crypto_request(ts_params->valid_devs[0],
				ut_params->op),
				"failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	if (ut_params->op->sym->m_dst) {
		auth_tag = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_dst,
				uint8_t *, plaintext_pad_len);
	} else {
		auth_tag = plaintext + plaintext_pad_len;
	}

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			test_case->auth_tag.data,
			test_case->auth_tag.len,
			"HMAC_MD5 generated tag not as expected");

	return TEST_SUCCESS;
}

static int
test_MD5_HMAC_verify(const struct HMAC_MD5_vector *test_case)
{
	uint8_t *plaintext;
	int ret;

	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_MD5_HMAC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	if (MD5_HMAC_create_session(ts_params, ut_params,
			RTE_CRYPTO_AUTH_OP_VERIFY, test_case)) {
		return TEST_FAILED;
	}

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	if (MD5_HMAC_create_op(ut_params, test_case, &plaintext))
		return TEST_FAILED;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
	else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		ret = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 1, 0, 0);
		if (ret != TEST_SUCCESS)
			return ret;
	} else
		TEST_ASSERT_NOT_NULL(
			process_crypto_request(ts_params->valid_devs[0],
				ut_params->op),
				"failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"HMAC_MD5 crypto op processing failed");

	return TEST_SUCCESS;
}

static int
test_MD5_HMAC_generate_case_1(void)
{
	return test_MD5_HMAC_generate(&HMAC_MD5_test_case_1);
}

static int
test_MD5_HMAC_verify_case_1(void)
{
	return test_MD5_HMAC_verify(&HMAC_MD5_test_case_1);
}

static int
test_MD5_HMAC_generate_case_2(void)
{
	return test_MD5_HMAC_generate(&HMAC_MD5_test_case_2);
}

static int
test_MD5_HMAC_verify_case_2(void)
{
	return test_MD5_HMAC_verify(&HMAC_MD5_test_case_2);
}

static int
test_multi_session(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_cryptodev_info dev_info;
	int i, nb_sess, ret = TEST_SUCCESS;
	void **sessions;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_SHA512_HMAC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_AES_CBC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(ut_params,
			aes_cbc_key, hmac_sha512_key);


	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	sessions = rte_malloc(NULL,
			sizeof(void *) *
			(MAX_NB_SESSIONS + 1), 0);

	/* Create multiple crypto sessions*/
	for (i = 0; i < MAX_NB_SESSIONS; i++) {
		sessions[i] = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->auth_xform,
				ts_params->session_mpool);
		if (sessions[i] == NULL && rte_errno == ENOTSUP) {
			nb_sess = i;
			ret = TEST_SKIPPED;
			break;
		}

		TEST_ASSERT_NOT_NULL(sessions[i],
				"Session creation failed at session number %u",
				i);

		/* Attempt to send a request on each session */
		ret = test_AES_CBC_HMAC_SHA512_decrypt_perform(
			sessions[i],
			ut_params,
			ts_params,
			catch_22_quote_2_512_bytes_AES_CBC_ciphertext,
			catch_22_quote_2_512_bytes_AES_CBC_HMAC_SHA512_digest,
			aes_cbc_iv);

		/* free crypto operation structure */
		rte_crypto_op_free(ut_params->op);

		/*
		 * free mbuf - both obuf and ibuf are usually the same,
		 * so check if they point at the same address is necessary,
		 * to avoid freeing the mbuf twice.
		 */
		if (ut_params->obuf) {
			rte_pktmbuf_free(ut_params->obuf);
			if (ut_params->ibuf == ut_params->obuf)
				ut_params->ibuf = 0;
			ut_params->obuf = 0;
		}
		if (ut_params->ibuf) {
			rte_pktmbuf_free(ut_params->ibuf);
			ut_params->ibuf = 0;
		}

		if (ret != TEST_SUCCESS) {
			i++;
			break;
		}
	}

	nb_sess = i;

	for (i = 0; i < nb_sess; i++) {
		rte_cryptodev_sym_session_free(ts_params->valid_devs[0],
				sessions[i]);
	}

	rte_free(sessions);

	if (ret != TEST_SKIPPED)
		TEST_ASSERT_SUCCESS(ret, "Failed to perform decrypt on request number %u.", i - 1);

	return ret;
}

struct multi_session_params {
	struct crypto_unittest_params ut_params;
	uint8_t *cipher_key;
	uint8_t *hmac_key;
	const uint8_t *cipher;
	const uint8_t *digest;
	uint8_t *iv;
};

#define MB_SESSION_NUMBER 3

static int
test_multi_session_random_usage(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info dev_info;
	int index = 0, ret = TEST_SUCCESS;
	uint32_t nb_sess, i, j;
	void **sessions;
	struct multi_session_params ut_paramz[] = {

		{
			.cipher_key = ms_aes_cbc_key0,
			.hmac_key = ms_hmac_key0,
			.cipher = ms_aes_cbc_cipher0,
			.digest = ms_hmac_digest0,
			.iv = ms_aes_cbc_iv0
		},
		{
			.cipher_key = ms_aes_cbc_key1,
			.hmac_key = ms_hmac_key1,
			.cipher = ms_aes_cbc_cipher1,
			.digest = ms_hmac_digest1,
			.iv = ms_aes_cbc_iv1
		},
		{
			.cipher_key = ms_aes_cbc_key2,
			.hmac_key = ms_hmac_key2,
			.cipher = ms_aes_cbc_cipher2,
			.digest = ms_hmac_digest2,
			.iv = ms_aes_cbc_iv2
		},

	};

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_SHA512_HMAC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = RTE_CRYPTO_CIPHER_AES_CBC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	sessions = rte_malloc(NULL, (sizeof(void *)
					* MAX_NB_SESSIONS) + 1, 0);

	for (i = 0; i < MB_SESSION_NUMBER; i++) {

		rte_memcpy(&ut_paramz[i].ut_params, &unittest_params,
				sizeof(struct crypto_unittest_params));

		test_AES_CBC_HMAC_SHA512_decrypt_create_session_params(
				&ut_paramz[i].ut_params,
				ut_paramz[i].cipher_key, ut_paramz[i].hmac_key);

		/* Create multiple crypto sessions*/
		sessions[i] = rte_cryptodev_sym_session_create(
				ts_params->valid_devs[0],
				&ut_paramz[i].ut_params.auth_xform,
				ts_params->session_mpool);
		if (sessions[i] == NULL && rte_errno == ENOTSUP) {
			nb_sess = i;
			ret = TEST_SKIPPED;
			goto session_clear;
		}

		TEST_ASSERT_NOT_NULL(sessions[i],
				"Session creation failed at session number %u",
				i);
	}

	nb_sess = i;

	srand(time(NULL));
	for (i = 0; i < 40000; i++) {

		j = rand() % MB_SESSION_NUMBER;

		ret = test_AES_CBC_HMAC_SHA512_decrypt_perform(
					sessions[j],
					&ut_paramz[j].ut_params,
					ts_params, ut_paramz[j].cipher,
					ut_paramz[j].digest,
					ut_paramz[j].iv);

		rte_crypto_op_free(ut_paramz[j].ut_params.op);

		/*
		 * free mbuf - both obuf and ibuf are usually the same,
		 * so check if they point at the same address is necessary,
		 * to avoid freeing the mbuf twice.
		 */
		if (ut_paramz[j].ut_params.obuf) {
			rte_pktmbuf_free(ut_paramz[j].ut_params.obuf);
			if (ut_paramz[j].ut_params.ibuf
					== ut_paramz[j].ut_params.obuf)
				ut_paramz[j].ut_params.ibuf = 0;
			ut_paramz[j].ut_params.obuf = 0;
		}
		if (ut_paramz[j].ut_params.ibuf) {
			rte_pktmbuf_free(ut_paramz[j].ut_params.ibuf);
			ut_paramz[j].ut_params.ibuf = 0;
		}

		if (ret != TEST_SKIPPED) {
			index = i;
			break;
		}
	}

session_clear:
	for (i = 0; i < nb_sess; i++) {
		rte_cryptodev_sym_session_free(ts_params->valid_devs[0],
				sessions[i]);
	}

	rte_free(sessions);

	if (ret != TEST_SKIPPED)
		TEST_ASSERT_SUCCESS(ret, "Failed to perform decrypt on request number %u.", index);

	return TEST_SUCCESS;
}

uint8_t orig_data[] = {0xab, 0xab, 0xab, 0xab,
			0xab, 0xab, 0xab, 0xab,
			0xab, 0xab, 0xab, 0xab,
			0xab, 0xab, 0xab, 0xab};

static int
test_null_invalid_operation(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	/* This test is for NULL PMD only */
	if (gbl_driver_id != rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_NULL_PMD)))
		return TEST_SKIPPED;

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_AES_CBC;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->cipher_xform,
			ts_params->session_mpool);
	TEST_ASSERT(ut_params->sess == NULL,
			"Session creation succeeded unexpectedly");

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_SHA1_HMAC;
	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->auth_xform,
			ts_params->session_mpool);
	TEST_ASSERT(ut_params->sess == NULL,
			"Session creation succeeded unexpectedly");

	return TEST_SUCCESS;
}


#define NULL_BURST_LENGTH (32)

static int
test_null_burst_operation(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	unsigned i, burst_len = NULL_BURST_LENGTH;

	struct rte_crypto_op *burst[NULL_BURST_LENGTH] = { NULL };
	struct rte_crypto_op *burst_dequeued[NULL_BURST_LENGTH] = { NULL };

	/* This test is for NULL PMD only */
	if (gbl_driver_id != rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_NULL_PMD)))
		return TEST_SKIPPED;

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = &ut_params->auth_xform;

	ut_params->cipher_xform.cipher.algo = RTE_CRYPTO_CIPHER_NULL;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;

	/* Setup HMAC Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_NULL;
	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
				ts_params->valid_devs[0],
				&ut_params->auth_xform,
				ts_params->session_mpool);
	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	TEST_ASSERT_EQUAL(rte_crypto_op_bulk_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC, burst, burst_len),
			burst_len, "failed to generate burst of crypto ops");

	/* Generate an operation for each mbuf in burst */
	for (i = 0; i < burst_len; i++) {
		struct rte_mbuf *m = rte_pktmbuf_alloc(ts_params->mbuf_pool);

		TEST_ASSERT_NOT_NULL(m, "Failed to allocate mbuf");

		unsigned *data = (unsigned *)rte_pktmbuf_append(m,
				sizeof(unsigned));
		*data = i;

		rte_crypto_op_attach_sym_session(burst[i], ut_params->sess);

		burst[i]->sym->m_src = m;
	}

	/* Process crypto operation */
	TEST_ASSERT_EQUAL(rte_cryptodev_enqueue_burst(ts_params->valid_devs[0],
			0, burst, burst_len),
			burst_len,
			"Error enqueuing burst");

	TEST_ASSERT_EQUAL(rte_cryptodev_dequeue_burst(ts_params->valid_devs[0],
			0, burst_dequeued, burst_len),
			burst_len,
			"Error dequeuing burst");


	for (i = 0; i < burst_len; i++) {
		TEST_ASSERT_EQUAL(
			*rte_pktmbuf_mtod(burst[i]->sym->m_src, uint32_t *),
			*rte_pktmbuf_mtod(burst_dequeued[i]->sym->m_src,
					uint32_t *),
			"data not as expected");

		rte_pktmbuf_free(burst[i]->sym->m_src);
		rte_crypto_op_free(burst[i]);
	}

	return TEST_SUCCESS;
}

static uint16_t
test_enq_callback(uint16_t dev_id, uint16_t qp_id, struct rte_crypto_op **ops,
		  uint16_t nb_ops, void *user_param)
{
	RTE_SET_USED(dev_id);
	RTE_SET_USED(qp_id);
	RTE_SET_USED(ops);
	RTE_SET_USED(user_param);

	printf("crypto enqueue callback called\n");
	return nb_ops;
}

static uint16_t
test_deq_callback(uint16_t dev_id, uint16_t qp_id, struct rte_crypto_op **ops,
		  uint16_t nb_ops, void *user_param)
{
	RTE_SET_USED(dev_id);
	RTE_SET_USED(qp_id);
	RTE_SET_USED(ops);
	RTE_SET_USED(user_param);

	printf("crypto dequeue callback called\n");
	return nb_ops;
}

/*
 * Thread using enqueue/dequeue callback with RCU.
 */
static int
test_enqdeq_callback_thread(void *arg)
{
	RTE_SET_USED(arg);
	/* DP thread calls rte_cryptodev_enqueue_burst()/
	 * rte_cryptodev_dequeue_burst() and invokes callback.
	 */
	test_null_burst_operation();
	return 0;
}

static int
test_enq_callback_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_qp_conf qp_conf = {
		.nb_descriptors = MAX_NUM_OPS_INFLIGHT
	};

	struct rte_cryptodev_cb *cb;
	uint16_t qp_id = 0;

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(ts_params->valid_devs[0]);

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev %u",
			ts_params->valid_devs[0]);

	qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT;
	qp_conf.mp_session = ts_params->session_mpool;

	TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0], qp_id, &qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed test for "
			"rte_cryptodev_queue_pair_setup: num_inflights "
			"%u on qp %u on cryptodev %u",
			qp_conf.nb_descriptors, qp_id,
			ts_params->valid_devs[0]);

	/* Test with invalid crypto device */
	cb = rte_cryptodev_add_enq_callback(RTE_CRYPTO_MAX_DEVS,
			qp_id, test_enq_callback, NULL);
	TEST_ASSERT_NULL(cb, "Add callback on qp %u on "
			"cryptodev %u did not fail",
			qp_id, RTE_CRYPTO_MAX_DEVS);

	/* Test with invalid queue pair */
	cb = rte_cryptodev_add_enq_callback(ts_params->valid_devs[0],
			dev_info.max_nb_queue_pairs + 1,
			test_enq_callback, NULL);
	TEST_ASSERT_NULL(cb, "Add callback on qp %u on "
			"cryptodev %u did not fail",
			dev_info.max_nb_queue_pairs + 1,
			ts_params->valid_devs[0]);

	/* Test with NULL callback */
	cb = rte_cryptodev_add_enq_callback(ts_params->valid_devs[0],
			qp_id, NULL, NULL);
	TEST_ASSERT_NULL(cb, "Add callback on qp %u on "
			"cryptodev %u did not fail",
			qp_id, ts_params->valid_devs[0]);

	/* Test with valid configuration */
	cb = rte_cryptodev_add_enq_callback(ts_params->valid_devs[0],
			qp_id, test_enq_callback, NULL);
	TEST_ASSERT_NOT_NULL(cb, "Failed test to add callback on "
			"qp %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);

	rte_cryptodev_start(ts_params->valid_devs[0]);

	/* Launch a thread */
	rte_eal_remote_launch(test_enqdeq_callback_thread, NULL,
				rte_get_next_lcore(-1, 1, 0));

	/* Wait until reader exited. */
	rte_eal_mp_wait_lcore();

	/* Test with invalid crypto device */
	TEST_ASSERT_FAIL(rte_cryptodev_remove_enq_callback(
			RTE_CRYPTO_MAX_DEVS, qp_id, cb),
			"Expected call to fail as crypto device is invalid");

	/* Test with invalid queue pair */
	TEST_ASSERT_FAIL(rte_cryptodev_remove_enq_callback(
			ts_params->valid_devs[0],
			dev_info.max_nb_queue_pairs + 1, cb),
			"Expected call to fail as queue pair is invalid");

	/* Test with NULL callback */
	TEST_ASSERT_FAIL(rte_cryptodev_remove_enq_callback(
			ts_params->valid_devs[0], qp_id, NULL),
			"Expected call to fail as callback is NULL");

	/* Test with valid configuration */
	TEST_ASSERT_SUCCESS(rte_cryptodev_remove_enq_callback(
			ts_params->valid_devs[0], qp_id, cb),
			"Failed test to remove callback on "
			"qp %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);

	return TEST_SUCCESS;
}

static int
test_deq_callback_setup(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct rte_cryptodev_info dev_info;
	struct rte_cryptodev_qp_conf qp_conf = {
		.nb_descriptors = MAX_NUM_OPS_INFLIGHT
	};

	struct rte_cryptodev_cb *cb;
	uint16_t qp_id = 0;

	/* Stop the device in case it's started so it can be configured */
	rte_cryptodev_stop(ts_params->valid_devs[0]);

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);

	TEST_ASSERT_SUCCESS(rte_cryptodev_configure(ts_params->valid_devs[0],
			&ts_params->conf),
			"Failed to configure cryptodev %u",
			ts_params->valid_devs[0]);

	qp_conf.nb_descriptors = MAX_NUM_OPS_INFLIGHT;
	qp_conf.mp_session = ts_params->session_mpool;

	TEST_ASSERT_SUCCESS(rte_cryptodev_queue_pair_setup(
			ts_params->valid_devs[0], qp_id, &qp_conf,
			rte_cryptodev_socket_id(ts_params->valid_devs[0])),
			"Failed test for "
			"rte_cryptodev_queue_pair_setup: num_inflights "
			"%u on qp %u on cryptodev %u",
			qp_conf.nb_descriptors, qp_id,
			ts_params->valid_devs[0]);

	/* Test with invalid crypto device */
	cb = rte_cryptodev_add_deq_callback(RTE_CRYPTO_MAX_DEVS,
			qp_id, test_deq_callback, NULL);
	TEST_ASSERT_NULL(cb, "Add callback on qp %u on "
			"cryptodev %u did not fail",
			qp_id, RTE_CRYPTO_MAX_DEVS);

	/* Test with invalid queue pair */
	cb = rte_cryptodev_add_deq_callback(ts_params->valid_devs[0],
			dev_info.max_nb_queue_pairs + 1,
			test_deq_callback, NULL);
	TEST_ASSERT_NULL(cb, "Add callback on qp %u on "
			"cryptodev %u did not fail",
			dev_info.max_nb_queue_pairs + 1,
			ts_params->valid_devs[0]);

	/* Test with NULL callback */
	cb = rte_cryptodev_add_deq_callback(ts_params->valid_devs[0],
			qp_id, NULL, NULL);
	TEST_ASSERT_NULL(cb, "Add callback on qp %u on "
			"cryptodev %u did not fail",
			qp_id, ts_params->valid_devs[0]);

	/* Test with valid configuration */
	cb = rte_cryptodev_add_deq_callback(ts_params->valid_devs[0],
			qp_id, test_deq_callback, NULL);
	TEST_ASSERT_NOT_NULL(cb, "Failed test to add callback on "
			"qp %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);

	rte_cryptodev_start(ts_params->valid_devs[0]);

	/* Launch a thread */
	rte_eal_remote_launch(test_enqdeq_callback_thread, NULL,
				rte_get_next_lcore(-1, 1, 0));

	/* Wait until reader exited. */
	rte_eal_mp_wait_lcore();

	/* Test with invalid crypto device */
	TEST_ASSERT_FAIL(rte_cryptodev_remove_deq_callback(
			RTE_CRYPTO_MAX_DEVS, qp_id, cb),
			"Expected call to fail as crypto device is invalid");

	/* Test with invalid queue pair */
	TEST_ASSERT_FAIL(rte_cryptodev_remove_deq_callback(
			ts_params->valid_devs[0],
			dev_info.max_nb_queue_pairs + 1, cb),
			"Expected call to fail as queue pair is invalid");

	/* Test with NULL callback */
	TEST_ASSERT_FAIL(rte_cryptodev_remove_deq_callback(
			ts_params->valid_devs[0], qp_id, NULL),
			"Expected call to fail as callback is NULL");

	/* Test with valid configuration */
	TEST_ASSERT_SUCCESS(rte_cryptodev_remove_deq_callback(
			ts_params->valid_devs[0], qp_id, cb),
			"Failed test to remove callback on "
			"qp %u on cryptodev %u",
			qp_id, ts_params->valid_devs[0]);

	return TEST_SUCCESS;
}

static void
generate_gmac_large_plaintext(uint8_t *data)
{
	uint16_t i;

	for (i = 32; i < GMAC_LARGE_PLAINTEXT_LENGTH; i += 32)
		memcpy(&data[i], &data[0], 32);
}

static int
create_gmac_operation(enum rte_crypto_auth_operation op,
		const struct gmac_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_crypto_sym_op *sym_op;

	uint32_t plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	sym_op = ut_params->op->sym;

	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, tdata->gmac_tag.len);
	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append digest");

	sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
			ut_params->ibuf, plaintext_pad_len);

	if (op == RTE_CRYPTO_AUTH_OP_VERIFY) {
		rte_memcpy(sym_op->auth.digest.data, tdata->gmac_tag.data,
				tdata->gmac_tag.len);
		debug_hexdump(stdout, "digest:",
				sym_op->auth.digest.data,
				tdata->gmac_tag.len);
	}

	uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ut_params->op,
			uint8_t *, IV_OFFSET);

	rte_memcpy(iv_ptr, tdata->iv.data, tdata->iv.len);

	debug_hexdump(stdout, "iv:", iv_ptr, tdata->iv.len);

	sym_op->cipher.data.length = 0;
	sym_op->cipher.data.offset = 0;

	sym_op->auth.data.offset = 0;
	sym_op->auth.data.length = tdata->plaintext.len;

	return 0;
}

static int
create_gmac_operation_sgl(enum rte_crypto_auth_operation op,
		const struct gmac_test_data *tdata,
		void *digest_mem, uint64_t digest_phys)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_crypto_sym_op *sym_op;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate symmetric crypto operation struct");

	sym_op = ut_params->op->sym;

	sym_op->auth.digest.data = digest_mem;
	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append digest");

	sym_op->auth.digest.phys_addr = digest_phys;

	if (op == RTE_CRYPTO_AUTH_OP_VERIFY) {
		rte_memcpy(sym_op->auth.digest.data, tdata->gmac_tag.data,
				tdata->gmac_tag.len);
		debug_hexdump(stdout, "digest:",
				sym_op->auth.digest.data,
				tdata->gmac_tag.len);
	}

	uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ut_params->op,
			uint8_t *, IV_OFFSET);

	rte_memcpy(iv_ptr, tdata->iv.data, tdata->iv.len);

	debug_hexdump(stdout, "iv:", iv_ptr, tdata->iv.len);

	sym_op->cipher.data.length = 0;
	sym_op->cipher.data.offset = 0;

	sym_op->auth.data.offset = 0;
	sym_op->auth.data.length = tdata->plaintext.len;

	return 0;
}

static int create_gmac_session(uint8_t dev_id,
		const struct gmac_test_data *tdata,
		enum rte_crypto_auth_operation auth_op)
{
	uint8_t auth_key[tdata->key.len];

	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	memcpy(auth_key, tdata->key.data, tdata->key.len);

	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.next = NULL;

	ut_params->auth_xform.auth.algo = RTE_CRYPTO_AUTH_AES_GMAC;
	ut_params->auth_xform.auth.op = auth_op;
	ut_params->auth_xform.auth.digest_length = tdata->gmac_tag.len;
	ut_params->auth_xform.auth.key.length = tdata->key.len;
	ut_params->auth_xform.auth.key.data = auth_key;
	ut_params->auth_xform.auth.iv.offset = IV_OFFSET;
	ut_params->auth_xform.auth.iv.length = tdata->iv.len;


	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
			&ut_params->auth_xform, ts_params->session_mpool);
	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	return 0;
}

static int
test_AES_GMAC_authentication(const struct gmac_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	int retval;

	uint8_t *auth_tag, *plaintext;
	uint16_t plaintext_pad_len;

	TEST_ASSERT_NOT_EQUAL(tdata->gmac_tag.len, 0,
			      "No GMAC length in the source data");

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_AES_GMAC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	retval = create_gmac_session(ts_params->valid_devs[0],
			tdata, RTE_CRYPTO_AUTH_OP_GENERATE);

	if (retval == TEST_SKIPPED)
		return TEST_SKIPPED;
	if (retval < 0)
		return retval;

	if (tdata->plaintext.len > MBUF_SIZE)
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->large_mbuf_pool);
	else
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);
	/*
	 * Runtime generate the large plain text instead of use hard code
	 * plain text vector. It is done to avoid create huge source file
	 * with the test vector.
	 */
	if (tdata->plaintext.len == GMAC_LARGE_PLAINTEXT_LENGTH)
		generate_gmac_large_plaintext(tdata->plaintext.data);

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	TEST_ASSERT_NOT_NULL(plaintext, "no room to append plaintext");

	memcpy(plaintext, tdata->plaintext.data, tdata->plaintext.len);
	debug_hexdump(stdout, "plaintext:", plaintext,
			tdata->plaintext.len);

	retval = create_gmac_operation(RTE_CRYPTO_AUTH_OP_GENERATE,
			tdata);

	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
	else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 1, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		TEST_ASSERT_NOT_NULL(
			process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	if (ut_params->op->sym->m_dst) {
		auth_tag = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_dst,
				uint8_t *, plaintext_pad_len);
	} else {
		auth_tag = plaintext + plaintext_pad_len;
	}

	debug_hexdump(stdout, "auth tag:", auth_tag, tdata->gmac_tag.len);

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			tdata->gmac_tag.data,
			tdata->gmac_tag.len,
			"GMAC Generated auth tag not as expected");

	return 0;
}

static int
test_AES_GMAC_authentication_test_case_1(void)
{
	return test_AES_GMAC_authentication(&gmac_test_case_1);
}

static int
test_AES_GMAC_authentication_test_case_2(void)
{
	return test_AES_GMAC_authentication(&gmac_test_case_2);
}

static int
test_AES_GMAC_authentication_test_case_3(void)
{
	return test_AES_GMAC_authentication(&gmac_test_case_3);
}

static int
test_AES_GMAC_authentication_test_case_4(void)
{
	return test_AES_GMAC_authentication(&gmac_test_case_4);
}

static int
test_AES_GMAC_authentication_verify(const struct gmac_test_data *tdata)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	int retval;
	uint32_t plaintext_pad_len;
	uint8_t *plaintext;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	TEST_ASSERT_NOT_EQUAL(tdata->gmac_tag.len, 0,
			      "No GMAC length in the source data");

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_AES_GMAC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	retval = create_gmac_session(ts_params->valid_devs[0],
			tdata, RTE_CRYPTO_AUTH_OP_VERIFY);

	if (retval == TEST_SKIPPED)
		return TEST_SKIPPED;
	if (retval < 0)
		return retval;

	if (tdata->plaintext.len > MBUF_SIZE)
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->large_mbuf_pool);
	else
		ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext_pad_len = RTE_ALIGN_CEIL(tdata->plaintext.len, 16);

	/*
	 * Runtime generate the large plain text instead of use hard code
	 * plain text vector. It is done to avoid create huge source file
	 * with the test vector.
	 */
	if (tdata->plaintext.len == GMAC_LARGE_PLAINTEXT_LENGTH)
		generate_gmac_large_plaintext(tdata->plaintext.data);

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_pad_len);
	TEST_ASSERT_NOT_NULL(plaintext, "no room to append plaintext");

	memcpy(plaintext, tdata->plaintext.data, tdata->plaintext.len);
	debug_hexdump(stdout, "plaintext:", plaintext,
			tdata->plaintext.len);

	retval = create_gmac_operation(RTE_CRYPTO_AUTH_OP_VERIFY,
			tdata);

	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
	else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 1, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		TEST_ASSERT_NOT_NULL(
			process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	return 0;

}

static int
test_AES_GMAC_authentication_verify_test_case_1(void)
{
	return test_AES_GMAC_authentication_verify(&gmac_test_case_1);
}

static int
test_AES_GMAC_authentication_verify_test_case_2(void)
{
	return test_AES_GMAC_authentication_verify(&gmac_test_case_2);
}

static int
test_AES_GMAC_authentication_verify_test_case_3(void)
{
	return test_AES_GMAC_authentication_verify(&gmac_test_case_3);
}

static int
test_AES_GMAC_authentication_verify_test_case_4(void)
{
	return test_AES_GMAC_authentication_verify(&gmac_test_case_4);
}

static int
test_AES_GMAC_authentication_SGL(const struct gmac_test_data *tdata,
				uint32_t fragsz)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_cryptodev_info dev_info;
	uint64_t feature_flags;
	unsigned int trn_data = 0;
	void *digest_mem = NULL;
	uint32_t segs = 1;
	unsigned int to_trn = 0;
	struct rte_mbuf *buf = NULL;
	uint8_t *auth_tag, *plaintext;
	int retval;

	TEST_ASSERT_NOT_EQUAL(tdata->gmac_tag.len, 0,
			      "No GMAC length in the source data");

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = RTE_CRYPTO_AUTH_AES_GMAC;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Check for any input SGL support */
	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	feature_flags = dev_info.feature_flags;

	if ((!(feature_flags & RTE_CRYPTODEV_FF_IN_PLACE_SGL)) ||
			(!(feature_flags & RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT)) ||
			(!(feature_flags & RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT)))
		return TEST_SKIPPED;

	if (fragsz > tdata->plaintext.len)
		fragsz = tdata->plaintext.len;

	uint16_t plaintext_len = fragsz;

	retval = create_gmac_session(ts_params->valid_devs[0],
			tdata, RTE_CRYPTO_AUTH_OP_GENERATE);

	if (retval == TEST_SKIPPED)
		return TEST_SKIPPED;
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				plaintext_len);
	TEST_ASSERT_NOT_NULL(plaintext, "no room to append plaintext");

	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	trn_data += plaintext_len;

	buf = ut_params->ibuf;

	/*
	 * Loop until no more fragments
	 */

	while (trn_data < tdata->plaintext.len) {
		++segs;
		to_trn = (tdata->plaintext.len - trn_data < fragsz) ?
				(tdata->plaintext.len - trn_data) : fragsz;

		buf->next = rte_pktmbuf_alloc(ts_params->mbuf_pool);
		buf = buf->next;

		memset(rte_pktmbuf_mtod(buf, uint8_t *), 0,
				rte_pktmbuf_tailroom(buf));

		plaintext = (uint8_t *)rte_pktmbuf_append(buf,
				to_trn);

		memcpy(plaintext, tdata->plaintext.data + trn_data,
				to_trn);
		trn_data += to_trn;
		if (trn_data  == tdata->plaintext.len)
			digest_mem = (uint8_t *)rte_pktmbuf_append(buf,
					tdata->gmac_tag.len);
	}
	ut_params->ibuf->nb_segs = segs;

	/*
	 * Place digest at the end of the last buffer
	 */
	uint64_t digest_phys = rte_pktmbuf_iova(buf) + to_trn;

	if (!digest_mem) {
		digest_mem = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				+ tdata->gmac_tag.len);
		digest_phys = rte_pktmbuf_iova_offset(ut_params->ibuf,
				tdata->plaintext.len);
	}

	retval = create_gmac_operation_sgl(RTE_CRYPTO_AUTH_OP_GENERATE,
			tdata, digest_mem, digest_phys);

	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		return TEST_SKIPPED;

	TEST_ASSERT_NOT_NULL(
		process_crypto_request(ts_params->valid_devs[0],
		ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	auth_tag = digest_mem;
	debug_hexdump(stdout, "auth tag:", auth_tag, tdata->gmac_tag.len);
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			tdata->gmac_tag.data,
			tdata->gmac_tag.len,
			"GMAC Generated auth tag not as expected");

	return 0;
}

/* Segment size not multiple of block size (16B) */
static int
test_AES_GMAC_authentication_SGL_40B(void)
{
	return test_AES_GMAC_authentication_SGL(&gmac_test_case_1, 40);
}

static int
test_AES_GMAC_authentication_SGL_80B(void)
{
	return test_AES_GMAC_authentication_SGL(&gmac_test_case_1, 80);
}

static int
test_AES_GMAC_authentication_SGL_2048B(void)
{
	return test_AES_GMAC_authentication_SGL(&gmac_test_case_5, 2048);
}

/* Segment size not multiple of block size (16B) */
static int
test_AES_GMAC_authentication_SGL_2047B(void)
{
	return test_AES_GMAC_authentication_SGL(&gmac_test_case_5, 2047);
}

struct test_crypto_vector {
	enum rte_crypto_cipher_algorithm crypto_algo;
	unsigned int cipher_offset;
	unsigned int cipher_len;

	struct {
		uint8_t data[64];
		unsigned int len;
	} cipher_key;

	struct {
		uint8_t data[64];
		unsigned int len;
	} iv;

	struct {
		const uint8_t *data;
		unsigned int len;
	} plaintext;

	struct {
		const uint8_t *data;
		unsigned int len;
	} ciphertext;

	enum rte_crypto_auth_algorithm auth_algo;
	unsigned int auth_offset;

	struct {
		uint8_t data[128];
		unsigned int len;
	} auth_key;

	struct {
		const uint8_t *data;
		unsigned int len;
	} aad;

	struct {
		uint8_t data[128];
		unsigned int len;
	} digest;
};

static const struct test_crypto_vector
hmac_sha1_test_crypto_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.plaintext = {
		.data = plaintext_hash,
		.len = 512
	},
	.auth_key = {
		.data = {
			0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
			0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
			0xDE, 0xF4, 0xDE, 0xAD
		},
		.len = 20
	},
	.digest = {
		.data = {
			0xC4, 0xB7, 0x0E, 0x6B, 0xDE, 0xD1, 0xE7, 0x77,
			0x7E, 0x2E, 0x8F, 0xFC, 0x48, 0x39, 0x46, 0x17,
			0x3F, 0x91, 0x64, 0x59
		},
		.len = 20
	}
};

static const struct test_crypto_vector
aes128_gmac_test_vector = {
	.auth_algo = RTE_CRYPTO_AUTH_AES_GMAC,
	.plaintext = {
		.data = plaintext_hash,
		.len = 512
	},
	.iv = {
		.data = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0A, 0x0B
		},
		.len = 12
	},
	.auth_key = {
		.data = {
			0x42, 0x1A, 0x7D, 0x3D, 0xF5, 0x82, 0x80, 0xF1,
			0xF1, 0x35, 0x5C, 0x3B, 0xDD, 0x9A, 0x65, 0xBA
		},
		.len = 16
	},
	.digest = {
		.data = {
			0xCA, 0x00, 0x99, 0x8B, 0x30, 0x7E, 0x74, 0x56,
			0x32, 0xA7, 0x87, 0xB5, 0xE9, 0xB2, 0x34, 0x5A
		},
		.len = 16
	}
};

static const struct test_crypto_vector
aes128cbc_hmac_sha1_test_vector = {
	.crypto_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.cipher_offset = 0,
	.cipher_len = 512,
	.cipher_key = {
		.data = {
			0xE4, 0x23, 0x33, 0x8A, 0x35, 0x64, 0x61, 0xE2,
			0x49, 0x03, 0xDD, 0xC6, 0xB8, 0xCA, 0x55, 0x7A
		},
		.len = 16
	},
	.iv = {
		.data = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
		},
		.len = 16
	},
	.plaintext = {
		.data = plaintext_hash,
		.len = 512
	},
	.ciphertext = {
		.data = ciphertext512_aes128cbc,
		.len = 512
	},
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.auth_offset = 0,
	.auth_key = {
		.data = {
			0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
			0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
			0xDE, 0xF4, 0xDE, 0xAD
		},
		.len = 20
	},
	.digest = {
		.data = {
			0x9A, 0x4F, 0x88, 0x1B, 0xB6, 0x8F, 0xD8, 0x60,
			0x42, 0x1A, 0x7D, 0x3D, 0xF5, 0x82, 0x80, 0xF1,
			0x18, 0x8C, 0x1D, 0x32
		},
		.len = 20
	}
};

static const struct test_crypto_vector
aes128cbc_hmac_sha1_aad_test_vector = {
	.crypto_algo = RTE_CRYPTO_CIPHER_AES_CBC,
	.cipher_offset = 8,
	.cipher_len = 496,
	.cipher_key = {
		.data = {
			0xE4, 0x23, 0x33, 0x8A, 0x35, 0x64, 0x61, 0xE2,
			0x49, 0x03, 0xDD, 0xC6, 0xB8, 0xCA, 0x55, 0x7A
		},
		.len = 16
	},
	.iv = {
		.data = {
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
		},
		.len = 16
	},
	.plaintext = {
		.data = plaintext_hash,
		.len = 512
	},
	.ciphertext = {
		.data = ciphertext512_aes128cbc_aad,
		.len = 512
	},
	.auth_algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
	.auth_offset = 0,
	.auth_key = {
		.data = {
			0xF8, 0x2A, 0xC7, 0x54, 0xDB, 0x96, 0x18, 0xAA,
			0xC3, 0xA1, 0x53, 0xF6, 0x1F, 0x17, 0x60, 0xBD,
			0xDE, 0xF4, 0xDE, 0xAD
		},
		.len = 20
	},
	.digest = {
		.data = {
			0x6D, 0xF3, 0x50, 0x79, 0x7A, 0x2A, 0xAC, 0x7F,
			0xA6, 0xF0, 0xC6, 0x38, 0x1F, 0xA4, 0xDD, 0x9B,
			0x62, 0x0F, 0xFB, 0x10
		},
		.len = 20
	}
};

static void
data_corruption(uint8_t *data)
{
	data[0] += 1;
}

static void
tag_corruption(uint8_t *data, unsigned int tag_offset)
{
	data[tag_offset] += 1;
}

static int
create_auth_session(struct crypto_unittest_params *ut_params,
		uint8_t dev_id,
		const struct test_crypto_vector *reference,
		enum rte_crypto_auth_operation auth_op)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t auth_key[reference->auth_key.len + 1];

	memcpy(auth_key, reference->auth_key.data, reference->auth_key.len);

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.auth.op = auth_op;
	ut_params->auth_xform.next = NULL;
	ut_params->auth_xform.auth.algo = reference->auth_algo;
	ut_params->auth_xform.auth.key.length = reference->auth_key.len;
	ut_params->auth_xform.auth.key.data = auth_key;
	ut_params->auth_xform.auth.digest_length = reference->digest.len;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
				&ut_params->auth_xform,
				ts_params->session_mpool);
	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;

	return 0;
}

static int
create_auth_cipher_session(struct crypto_unittest_params *ut_params,
		uint8_t dev_id,
		const struct test_crypto_vector *reference,
		enum rte_crypto_auth_operation auth_op,
		enum rte_crypto_cipher_operation cipher_op)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t cipher_key[reference->cipher_key.len + 1];
	uint8_t auth_key[reference->auth_key.len + 1];

	memcpy(cipher_key, reference->cipher_key.data,
			reference->cipher_key.len);
	memcpy(auth_key, reference->auth_key.data, reference->auth_key.len);

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.auth.op = auth_op;
	ut_params->auth_xform.auth.algo = reference->auth_algo;
	ut_params->auth_xform.auth.key.length = reference->auth_key.len;
	ut_params->auth_xform.auth.key.data = auth_key;
	ut_params->auth_xform.auth.digest_length = reference->digest.len;

	if (reference->auth_algo == RTE_CRYPTO_AUTH_AES_GMAC) {
		ut_params->auth_xform.auth.iv.offset = IV_OFFSET;
		ut_params->auth_xform.auth.iv.length = reference->iv.len;
	} else {
		ut_params->auth_xform.next = &ut_params->cipher_xform;

		/* Setup Cipher Parameters */
		ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
		ut_params->cipher_xform.next = NULL;
		ut_params->cipher_xform.cipher.algo = reference->crypto_algo;
		ut_params->cipher_xform.cipher.op = cipher_op;
		ut_params->cipher_xform.cipher.key.data = cipher_key;
		ut_params->cipher_xform.cipher.key.length = reference->cipher_key.len;
		ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
		ut_params->cipher_xform.cipher.iv.length = reference->iv.len;
	}

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(dev_id,
				&ut_params->auth_xform,
				ts_params->session_mpool);
	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;

	return 0;
}

static int
create_auth_operation(struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int auth_generate)
{
	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate pktmbuf offload");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* digest */
	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, reference->digest.len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append auth tag");

	sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
			ut_params->ibuf, reference->plaintext.len);

	if (auth_generate)
		memset(sym_op->auth.digest.data, 0, reference->digest.len);
	else
		memcpy(sym_op->auth.digest.data,
				reference->digest.data,
				reference->digest.len);

	debug_hexdump(stdout, "digest:",
			sym_op->auth.digest.data,
			reference->digest.len);

	sym_op->auth.data.length = reference->plaintext.len;
	sym_op->auth.data.offset = 0;

	return 0;
}

static int
create_auth_GMAC_operation(struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int auth_generate)
{
	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate pktmbuf offload");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* digest */
	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, reference->digest.len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append auth tag");

	sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
			ut_params->ibuf, reference->ciphertext.len);

	if (auth_generate)
		memset(sym_op->auth.digest.data, 0, reference->digest.len);
	else
		memcpy(sym_op->auth.digest.data,
				reference->digest.data,
				reference->digest.len);

	debug_hexdump(stdout, "digest:",
			sym_op->auth.digest.data,
			reference->digest.len);

	rte_memcpy(rte_crypto_op_ctod_offset(ut_params->op, uint8_t *, IV_OFFSET),
			reference->iv.data, reference->iv.len);

	sym_op->cipher.data.length = 0;
	sym_op->cipher.data.offset = 0;

	sym_op->auth.data.length = reference->plaintext.len;
	sym_op->auth.data.offset = 0;

	return 0;
}

static int
create_cipher_auth_operation(struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int auth_generate)
{
	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
			"Failed to allocate pktmbuf offload");

	/* Set crypto operation data parameters */
	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	/* set crypto operation source mbuf */
	sym_op->m_src = ut_params->ibuf;

	/* digest */
	sym_op->auth.digest.data = (uint8_t *)rte_pktmbuf_append(
			ut_params->ibuf, reference->digest.len);

	TEST_ASSERT_NOT_NULL(sym_op->auth.digest.data,
			"no room to append auth tag");

	sym_op->auth.digest.phys_addr = rte_pktmbuf_iova_offset(
			ut_params->ibuf, reference->ciphertext.len);

	if (auth_generate)
		memset(sym_op->auth.digest.data, 0, reference->digest.len);
	else
		memcpy(sym_op->auth.digest.data,
				reference->digest.data,
				reference->digest.len);

	debug_hexdump(stdout, "digest:",
			sym_op->auth.digest.data,
			reference->digest.len);

	rte_memcpy(rte_crypto_op_ctod_offset(ut_params->op, uint8_t *, IV_OFFSET),
			reference->iv.data, reference->iv.len);

	sym_op->cipher.data.length = reference->cipher_len;
	sym_op->cipher.data.offset = reference->cipher_offset;

	sym_op->auth.data.length = reference->plaintext.len;
	sym_op->auth.data.offset = reference->auth_offset;

	return 0;
}

static int
create_auth_verify_operation(struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return create_auth_operation(ts_params, ut_params, reference, 0);
}

static int
create_auth_verify_GMAC_operation(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return create_auth_GMAC_operation(ts_params, ut_params, reference, 0);
}

static int
create_cipher_auth_verify_operation(struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return create_cipher_auth_operation(ts_params, ut_params, reference, 0);
}

static int
test_authentication_verify_fail_when_data_corruption(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int data_corrupted)
{
	int retval;

	uint8_t *plaintext;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = reference->auth_algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;


	/* Create session */
	retval = create_auth_session(ut_params,
			ts_params->valid_devs[0],
			reference,
			RTE_CRYPTO_AUTH_OP_VERIFY);

	if (retval == TEST_SKIPPED)
		return TEST_SKIPPED;
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			reference->plaintext.len);
	TEST_ASSERT_NOT_NULL(plaintext, "no room to append plaintext");
	memcpy(plaintext, reference->plaintext.data, reference->plaintext.len);

	debug_hexdump(stdout, "plaintext:", plaintext,
		reference->plaintext.len);

	/* Create operation */
	retval = create_auth_verify_operation(ts_params, ut_params, reference);

	if (retval < 0)
		return retval;

	if (data_corrupted)
		data_corruption(plaintext);
	else
		tag_corruption(plaintext, reference->plaintext.len);

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO) {
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
		TEST_ASSERT_NOT_EQUAL(ut_params->op->status,
			RTE_CRYPTO_OP_STATUS_SUCCESS,
			"authentication not failed");
	} else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 1, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else {
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
	}
	if (ut_params->op == NULL)
		return 0;
	else if (ut_params->op->status != RTE_CRYPTO_OP_STATUS_SUCCESS)
		return 0;

	return -1;
}

static int
test_authentication_verify_GMAC_fail_when_corruption(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int data_corrupted)
{
	int retval;
	uint8_t *plaintext;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = reference->auth_algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create session */
	retval = create_auth_cipher_session(ut_params,
			ts_params->valid_devs[0],
			reference,
			RTE_CRYPTO_AUTH_OP_VERIFY,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
	if (retval == TEST_SKIPPED)
		return TEST_SKIPPED;
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			reference->plaintext.len);
	TEST_ASSERT_NOT_NULL(plaintext, "no room to append plaintext");
	memcpy(plaintext, reference->plaintext.data, reference->plaintext.len);

	debug_hexdump(stdout, "plaintext:", plaintext,
		reference->plaintext.len);

	/* Create operation */
	retval = create_auth_verify_GMAC_operation(ts_params,
			ut_params,
			reference);

	if (retval < 0)
		return retval;

	if (data_corrupted)
		data_corruption(plaintext);
	else
		tag_corruption(plaintext, reference->aad.len);

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO) {
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
		TEST_ASSERT_NOT_EQUAL(ut_params->op->status,
			RTE_CRYPTO_OP_STATUS_SUCCESS,
			"authentication not failed");
	} else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 1, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else {
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
		TEST_ASSERT_NULL(ut_params->op, "authentication not failed");
	}

	return 0;
}

static int
test_authenticated_decryption_fail_when_corruption(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference,
		unsigned int data_corrupted)
{
	int retval;

	uint8_t *ciphertext;
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = reference->auth_algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = reference->crypto_algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create session */
	retval = create_auth_cipher_session(ut_params,
			ts_params->valid_devs[0],
			reference,
			RTE_CRYPTO_AUTH_OP_VERIFY,
			RTE_CRYPTO_CIPHER_OP_DECRYPT);
	if (retval == TEST_SKIPPED)
		return TEST_SKIPPED;
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			reference->ciphertext.len);
	TEST_ASSERT_NOT_NULL(ciphertext, "no room to append ciphertext");
	memcpy(ciphertext, reference->ciphertext.data,
			reference->ciphertext.len);

	/* Create operation */
	retval = create_cipher_auth_verify_operation(ts_params,
			ut_params,
			reference);

	if (retval < 0)
		return retval;

	if (data_corrupted)
		data_corruption(ciphertext);
	else
		tag_corruption(ciphertext, reference->ciphertext.len);

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO) {
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
		TEST_ASSERT_NOT_EQUAL(ut_params->op->status,
			RTE_CRYPTO_OP_STATUS_SUCCESS,
			"authentication not failed");
	} else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else {
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);
		TEST_ASSERT_NULL(ut_params->op, "authentication not failed");
	}

	return 0;
}

static int
test_authenticated_encrypt_with_esn(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	int retval;

	uint8_t *authciphertext, *plaintext, *auth_tag;
	uint16_t plaintext_pad_len;
	uint8_t cipher_key[reference->cipher_key.len + 1];
	uint8_t auth_key[reference->auth_key.len + 1];
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = reference->auth_algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = reference->crypto_algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create session */
	memcpy(cipher_key, reference->cipher_key.data,
			reference->cipher_key.len);
	memcpy(auth_key, reference->auth_key.data, reference->auth_key.len);

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.cipher.algo = reference->crypto_algo;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	ut_params->cipher_xform.cipher.key.data = cipher_key;
	ut_params->cipher_xform.cipher.key.length = reference->cipher_key.len;
	ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
	ut_params->cipher_xform.cipher.iv.length = reference->iv.len;

	ut_params->cipher_xform.next = &ut_params->auth_xform;

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
	ut_params->auth_xform.auth.algo = reference->auth_algo;
	ut_params->auth_xform.auth.key.length = reference->auth_key.len;
	ut_params->auth_xform.auth.key.data = auth_key;
	ut_params->auth_xform.auth.digest_length = reference->digest.len;
	ut_params->auth_xform.next = NULL;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->cipher_xform,
			ts_params->session_mpool);
	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			reference->plaintext.len);
	TEST_ASSERT_NOT_NULL(plaintext, "no room to append plaintext");
	memcpy(plaintext, reference->plaintext.data, reference->plaintext.len);

	/* Create operation */
	retval = create_cipher_auth_operation(ts_params,
			ut_params,
			reference, 0);

	if (retval < 0)
		return retval;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
	else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(
			ts_params->valid_devs[0], ut_params->op);

	TEST_ASSERT_NOT_NULL(ut_params->op, "no crypto operation returned");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");

	plaintext_pad_len = RTE_ALIGN_CEIL(reference->plaintext.len, 16);

	authciphertext = rte_pktmbuf_mtod_offset(ut_params->ibuf, uint8_t *,
			ut_params->op->sym->auth.data.offset);
	auth_tag = authciphertext + plaintext_pad_len;
	debug_hexdump(stdout, "ciphertext:", authciphertext,
			reference->ciphertext.len);
	debug_hexdump(stdout, "auth tag:", auth_tag, reference->digest.len);

	/* Validate obuf */
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			authciphertext,
			reference->ciphertext.data,
			reference->ciphertext.len,
			"Ciphertext data not as expected");

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			reference->digest.data,
			reference->digest.len,
			"Generated digest not as expected");

	return TEST_SUCCESS;

}

static int
test_authenticated_decrypt_with_esn(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	int retval;

	uint8_t *ciphertext;
	uint8_t cipher_key[reference->cipher_key.len + 1];
	uint8_t auth_key[reference->auth_key.len + 1];
	struct rte_cryptodev_info dev_info;

	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	uint64_t feat_flags = dev_info.feature_flags;

	if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
		printf("Device doesn't support RAW data-path APIs.\n");
		return TEST_SKIPPED;
	}

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = reference->auth_algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = reference->crypto_algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/* Create session */
	memcpy(cipher_key, reference->cipher_key.data,
			reference->cipher_key.len);
	memcpy(auth_key, reference->auth_key.data, reference->auth_key.len);

	/* Setup Authentication Parameters */
	ut_params->auth_xform.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ut_params->auth_xform.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
	ut_params->auth_xform.auth.algo = reference->auth_algo;
	ut_params->auth_xform.auth.key.length = reference->auth_key.len;
	ut_params->auth_xform.auth.key.data = auth_key;
	ut_params->auth_xform.auth.digest_length = reference->digest.len;
	ut_params->auth_xform.next = &ut_params->cipher_xform;

	/* Setup Cipher Parameters */
	ut_params->cipher_xform.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	ut_params->cipher_xform.next = NULL;
	ut_params->cipher_xform.cipher.algo = reference->crypto_algo;
	ut_params->cipher_xform.cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	ut_params->cipher_xform.cipher.key.data = cipher_key;
	ut_params->cipher_xform.cipher.key.length = reference->cipher_key.len;
	ut_params->cipher_xform.cipher.iv.offset = IV_OFFSET;
	ut_params->cipher_xform.cipher.iv.length = reference->iv.len;

	/* Create Crypto session*/
	ut_params->sess = rte_cryptodev_sym_session_create(
			ts_params->valid_devs[0], &ut_params->auth_xform,
			ts_params->session_mpool);
	if (ut_params->sess == NULL && rte_errno == ENOTSUP)
		return TEST_SKIPPED;
	TEST_ASSERT_NOT_NULL(ut_params->sess, "Session creation failed");

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
	TEST_ASSERT_NOT_NULL(ut_params->ibuf,
			"Failed to allocate input buffer in mempool");

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	ciphertext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			reference->ciphertext.len);
	TEST_ASSERT_NOT_NULL(ciphertext, "no room to append ciphertext");
	memcpy(ciphertext, reference->ciphertext.data,
			reference->ciphertext.len);

	/* Create operation */
	retval = create_cipher_auth_verify_operation(ts_params,
			ut_params,
			reference);

	if (retval < 0)
		return retval;

	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_crypt_auth_op(ts_params->valid_devs[0],
			ut_params->op);
	else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 1, 1, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		ut_params->op = process_crypto_request(ts_params->valid_devs[0],
			ut_params->op);

	TEST_ASSERT_NOT_NULL(ut_params->op, "failed crypto process");
	TEST_ASSERT_EQUAL(ut_params->op->status,
			RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing passed");

	ut_params->obuf = ut_params->op->sym->m_src;
	TEST_ASSERT_NOT_NULL(ut_params->obuf, "failed to retrieve obuf");

	return 0;
}

static int
create_aead_operation_SGL(enum rte_crypto_aead_operation op,
		const struct aead_test_data *tdata,
		void *digest_mem, uint64_t digest_phys)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;

	const unsigned int auth_tag_len = tdata->auth_tag.len;
	const unsigned int iv_len = tdata->iv.len;
	unsigned int aad_len = tdata->aad.len;
	unsigned int aad_len_pad = 0;

	/* Generate Crypto op data structure */
	ut_params->op = rte_crypto_op_alloc(ts_params->op_mpool,
			RTE_CRYPTO_OP_TYPE_SYMMETRIC);
	TEST_ASSERT_NOT_NULL(ut_params->op,
		"Failed to allocate symmetric crypto operation struct");

	struct rte_crypto_sym_op *sym_op = ut_params->op->sym;

	sym_op->aead.digest.data = digest_mem;

	TEST_ASSERT_NOT_NULL(sym_op->aead.digest.data,
			"no room to append digest");

	sym_op->aead.digest.phys_addr = digest_phys;

	if (op == RTE_CRYPTO_AEAD_OP_DECRYPT) {
		rte_memcpy(sym_op->aead.digest.data, tdata->auth_tag.data,
				auth_tag_len);
		debug_hexdump(stdout, "digest:",
				sym_op->aead.digest.data,
				auth_tag_len);
	}

	/* Append aad data */
	if (tdata->algo == RTE_CRYPTO_AEAD_AES_CCM) {
		uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ut_params->op,
				uint8_t *, IV_OFFSET);

		/* Copy IV 1 byte after the IV pointer, according to the API */
		rte_memcpy(iv_ptr + 1, tdata->iv.data, iv_len);

		aad_len = RTE_ALIGN_CEIL(aad_len + 18, 16);

		sym_op->aead.aad.data = (uint8_t *)rte_pktmbuf_prepend(
				ut_params->ibuf, aad_len);
		TEST_ASSERT_NOT_NULL(sym_op->aead.aad.data,
				"no room to prepend aad");
		sym_op->aead.aad.phys_addr = rte_pktmbuf_iova(
				ut_params->ibuf);

		memset(sym_op->aead.aad.data, 0, aad_len);
		/* Copy AAD 18 bytes after the AAD pointer, according to the API */
		rte_memcpy(sym_op->aead.aad.data, tdata->aad.data, aad_len);

		debug_hexdump(stdout, "iv:", iv_ptr, iv_len);
		debug_hexdump(stdout, "aad:",
				sym_op->aead.aad.data, aad_len);
	} else {
		uint8_t *iv_ptr = rte_crypto_op_ctod_offset(ut_params->op,
				uint8_t *, IV_OFFSET);

		rte_memcpy(iv_ptr, tdata->iv.data, iv_len);

		aad_len_pad = RTE_ALIGN_CEIL(aad_len, 16);

		sym_op->aead.aad.data = (uint8_t *)rte_pktmbuf_prepend(
				ut_params->ibuf, aad_len_pad);
		TEST_ASSERT_NOT_NULL(sym_op->aead.aad.data,
				"no room to prepend aad");
		sym_op->aead.aad.phys_addr = rte_pktmbuf_iova(
				ut_params->ibuf);

		memset(sym_op->aead.aad.data, 0, aad_len);
		rte_memcpy(sym_op->aead.aad.data, tdata->aad.data, aad_len);

		debug_hexdump(stdout, "iv:", iv_ptr, iv_len);
		debug_hexdump(stdout, "aad:",
				sym_op->aead.aad.data, aad_len);
	}

	sym_op->aead.data.length = tdata->plaintext.len;
	sym_op->aead.data.offset = aad_len_pad;

	return 0;
}

#define SGL_MAX_NO	16

static int
test_authenticated_encryption_SGL(const struct aead_test_data *tdata,
		const int oop, uint32_t fragsz, uint32_t fragsz_oop)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	struct crypto_unittest_params *ut_params = &unittest_params;
	struct rte_mbuf *buf, *buf_oop = NULL, *buf_last_oop = NULL;
	int retval;
	int to_trn = 0;
	int to_trn_tbl[SGL_MAX_NO];
	int segs = 1;
	unsigned int trn_data = 0;
	uint8_t *plaintext, *ciphertext, *auth_tag;
	struct rte_cryptodev_info dev_info;

	/* Verify the capabilities */
	struct rte_cryptodev_sym_capability_idx cap_idx;
	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AEAD;
	cap_idx.algo.aead = tdata->algo;
	if (rte_cryptodev_sym_capability_get(ts_params->valid_devs[0],
			&cap_idx) == NULL)
		return TEST_SKIPPED;

	/*
	 * SGL not supported on AESNI_MB PMD CPU crypto,
	 * OOP not supported on AESNI_GCM CPU crypto
	 */
	if (gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO &&
			(gbl_driver_id == rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD)) || oop))
		return TEST_SKIPPED;

	/* Detailed check for the particular SGL support flag */
	rte_cryptodev_info_get(ts_params->valid_devs[0], &dev_info);
	if (!oop) {
		unsigned int sgl_in = fragsz < tdata->plaintext.len;
		if (sgl_in && (!(dev_info.feature_flags &
				RTE_CRYPTODEV_FF_IN_PLACE_SGL)))
			return TEST_SKIPPED;

		uint64_t feat_flags = dev_info.feature_flags;

		if ((global_api_test_type == CRYPTODEV_RAW_API_TEST) &&
			(!(feat_flags & RTE_CRYPTODEV_FF_SYM_RAW_DP))) {
			printf("Device doesn't support RAW data-path APIs.\n");
			return TEST_SKIPPED;
		}
	} else {
		unsigned int sgl_in = fragsz < tdata->plaintext.len;
		unsigned int sgl_out = (fragsz_oop ? fragsz_oop : fragsz) <
				tdata->plaintext.len;
		/* Raw data path API does not support OOP */
		if (global_api_test_type == CRYPTODEV_RAW_API_TEST)
			return TEST_SKIPPED;
		if (sgl_in && !sgl_out) {
			if (!(dev_info.feature_flags &
					RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT))
				return TEST_SKIPPED;
		} else if (!sgl_in && sgl_out) {
			if (!(dev_info.feature_flags &
					RTE_CRYPTODEV_FF_OOP_LB_IN_SGL_OUT))
				return TEST_SKIPPED;
		} else if (sgl_in && sgl_out) {
			if (!(dev_info.feature_flags &
					RTE_CRYPTODEV_FF_OOP_SGL_IN_SGL_OUT))
				return TEST_SKIPPED;
		}
	}

	if (fragsz > tdata->plaintext.len)
		fragsz = tdata->plaintext.len;

	uint16_t plaintext_len = fragsz;
	uint16_t frag_size_oop = fragsz_oop ? fragsz_oop : fragsz;

	if (fragsz_oop > tdata->plaintext.len)
		frag_size_oop = tdata->plaintext.len;

	int ecx = 0;
	void *digest_mem = NULL;

	uint32_t prepend_len = RTE_ALIGN_CEIL(tdata->aad.len, 16);

	if (tdata->plaintext.len % fragsz != 0) {
		if (tdata->plaintext.len / fragsz + 1 > SGL_MAX_NO)
			return 1;
	}	else {
		if (tdata->plaintext.len / fragsz > SGL_MAX_NO)
			return 1;
	}

	/*
	 * For out-op-place we need to alloc another mbuf
	 */
	if (oop) {
		ut_params->obuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);
		rte_pktmbuf_append(ut_params->obuf,
				frag_size_oop + prepend_len);
		buf_oop = ut_params->obuf;
	}

	/* Create AEAD session */
	retval = create_aead_session(ts_params->valid_devs[0],
			tdata->algo,
			RTE_CRYPTO_AEAD_OP_ENCRYPT,
			tdata->key.data, tdata->key.len,
			tdata->aad.len, tdata->auth_tag.len,
			tdata->iv.len);
	if (retval < 0)
		return retval;

	ut_params->ibuf = rte_pktmbuf_alloc(ts_params->mbuf_pool);

	/* clear mbuf payload */
	memset(rte_pktmbuf_mtod(ut_params->ibuf, uint8_t *), 0,
			rte_pktmbuf_tailroom(ut_params->ibuf));

	plaintext = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
			plaintext_len);

	memcpy(plaintext, tdata->plaintext.data, plaintext_len);

	trn_data += plaintext_len;

	buf = ut_params->ibuf;

	/*
	 * Loop until no more fragments
	 */

	while (trn_data < tdata->plaintext.len) {
		++segs;
		to_trn = (tdata->plaintext.len - trn_data < fragsz) ?
				(tdata->plaintext.len - trn_data) : fragsz;

		to_trn_tbl[ecx++] = to_trn;

		buf->next = rte_pktmbuf_alloc(ts_params->mbuf_pool);
		buf = buf->next;

		memset(rte_pktmbuf_mtod(buf, uint8_t *), 0,
				rte_pktmbuf_tailroom(buf));

		/* OOP */
		if (oop && !fragsz_oop) {
			buf_last_oop = buf_oop->next =
					rte_pktmbuf_alloc(ts_params->mbuf_pool);
			buf_oop = buf_oop->next;
			memset(rte_pktmbuf_mtod(buf_oop, uint8_t *),
					0, rte_pktmbuf_tailroom(buf_oop));
			rte_pktmbuf_append(buf_oop, to_trn);
		}

		plaintext = (uint8_t *)rte_pktmbuf_append(buf,
				to_trn);

		memcpy(plaintext, tdata->plaintext.data + trn_data,
				to_trn);
		trn_data += to_trn;
		if (trn_data  == tdata->plaintext.len) {
			if (oop) {
				if (!fragsz_oop)
					digest_mem = rte_pktmbuf_append(buf_oop,
						tdata->auth_tag.len);
			} else
				digest_mem = (uint8_t *)rte_pktmbuf_append(buf,
					tdata->auth_tag.len);
		}
	}

	uint64_t digest_phys = 0;

	ut_params->ibuf->nb_segs = segs;

	segs = 1;
	if (fragsz_oop && oop) {
		to_trn = 0;
		ecx = 0;

		if (frag_size_oop == tdata->plaintext.len) {
			digest_mem = rte_pktmbuf_append(ut_params->obuf,
				tdata->auth_tag.len);

			digest_phys = rte_pktmbuf_iova_offset(
					ut_params->obuf,
					tdata->plaintext.len + prepend_len);
		}

		trn_data = frag_size_oop;
		while (trn_data < tdata->plaintext.len) {
			++segs;
			to_trn =
				(tdata->plaintext.len - trn_data <
						frag_size_oop) ?
				(tdata->plaintext.len - trn_data) :
						frag_size_oop;

			to_trn_tbl[ecx++] = to_trn;

			buf_last_oop = buf_oop->next =
					rte_pktmbuf_alloc(ts_params->mbuf_pool);
			buf_oop = buf_oop->next;
			memset(rte_pktmbuf_mtod(buf_oop, uint8_t *),
					0, rte_pktmbuf_tailroom(buf_oop));
			rte_pktmbuf_append(buf_oop, to_trn);

			trn_data += to_trn;

			if (trn_data  == tdata->plaintext.len) {
				digest_mem = rte_pktmbuf_append(buf_oop,
					tdata->auth_tag.len);
			}
		}

		ut_params->obuf->nb_segs = segs;
	}

	/*
	 * Place digest at the end of the last buffer
	 */
	if (!digest_phys)
		digest_phys = rte_pktmbuf_iova(buf) + to_trn;
	if (oop && buf_last_oop)
		digest_phys = rte_pktmbuf_iova(buf_last_oop) + to_trn;

	if (!digest_mem && !oop) {
		digest_mem = (uint8_t *)rte_pktmbuf_append(ut_params->ibuf,
				+ tdata->auth_tag.len);
		digest_phys = rte_pktmbuf_iova_offset(ut_params->ibuf,
				tdata->plaintext.len);
	}

	/* Create AEAD operation */
	retval = create_aead_operation_SGL(RTE_CRYPTO_AEAD_OP_ENCRYPT,
			tdata, digest_mem, digest_phys);

	if (retval < 0)
		return retval;

	rte_crypto_op_attach_sym_session(ut_params->op, ut_params->sess);

	ut_params->op->sym->m_src = ut_params->ibuf;
	if (oop)
		ut_params->op->sym->m_dst = ut_params->obuf;

	/* Process crypto operation */
	if (oop == IN_PLACE &&
			gbl_action_type == RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO)
		process_cpu_aead_op(ts_params->valid_devs[0], ut_params->op);
	else if (global_api_test_type == CRYPTODEV_RAW_API_TEST) {
		retval = process_sym_raw_dp_op(ts_params->valid_devs[0], 0, ut_params->op, 0, 0, 0,
					       0);
		if (retval != TEST_SUCCESS)
			return retval;
	} else
		TEST_ASSERT_NOT_NULL(
			process_crypto_request(ts_params->valid_devs[0],
			ut_params->op), "failed to process sym crypto op");

	TEST_ASSERT_EQUAL(ut_params->op->status, RTE_CRYPTO_OP_STATUS_SUCCESS,
			"crypto op processing failed");


	ciphertext = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_src,
			uint8_t *, prepend_len);
	if (oop) {
		ciphertext = rte_pktmbuf_mtod_offset(ut_params->op->sym->m_dst,
				uint8_t *, prepend_len);
	}

	if (fragsz_oop)
		fragsz = fragsz_oop;

	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			ciphertext,
			tdata->ciphertext.data,
			fragsz,
			"Ciphertext data not as expected");

	buf = ut_params->op->sym->m_src->next;
	if (oop)
		buf = ut_params->op->sym->m_dst->next;

	unsigned int off = fragsz;

	ecx = 0;
	while (buf) {
		ciphertext = rte_pktmbuf_mtod(buf,
				uint8_t *);

		TEST_ASSERT_BUFFERS_ARE_EQUAL(
				ciphertext,
				tdata->ciphertext.data + off,
				to_trn_tbl[ecx],
				"Ciphertext data not as expected");

		off += to_trn_tbl[ecx++];
		buf = buf->next;
	}

	auth_tag = digest_mem;
	TEST_ASSERT_BUFFERS_ARE_EQUAL(
			auth_tag,
			tdata->auth_tag.data,
			tdata->auth_tag.len,
			"Generated auth tag not as expected");

	return 0;
}

static int
test_AES_GCM_auth_encrypt_SGL_out_of_place_400B_400B(void)
{
	return test_authenticated_encryption_SGL(
			&gcm_test_case_SGL_1, OUT_OF_PLACE, 400, 400);
}

static int
test_AES_GCM_auth_encrypt_SGL_out_of_place_1500B_2000B(void)
{
	return test_authenticated_encryption_SGL(
			&gcm_test_case_SGL_1, OUT_OF_PLACE, 1500, 2000);
}

static int
test_AES_GCM_auth_encrypt_SGL_out_of_place_400B_1seg(void)
{
	return test_authenticated_encryption_SGL(
			&gcm_test_case_8, OUT_OF_PLACE, 400,
			gcm_test_case_8.plaintext.len);
}

static int
test_AES_GCM_auth_encrypt_SGL_in_place_1500B(void)
{
	/* This test is not for OPENSSL PMD */
	if (gbl_driver_id == rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD)))
		return TEST_SKIPPED;

	return test_authenticated_encryption_SGL(
			&gcm_test_case_SGL_1, IN_PLACE, 1500, 0);
}

static int
test_authentication_verify_fail_when_data_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authentication_verify_fail_when_data_corruption(
			ts_params, ut_params, reference, 1);
}

static int
test_authentication_verify_fail_when_tag_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authentication_verify_fail_when_data_corruption(
			ts_params, ut_params, reference, 0);
}

static int
test_authentication_verify_GMAC_fail_when_data_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authentication_verify_GMAC_fail_when_corruption(
			ts_params, ut_params, reference, 1);
}

static int
test_authentication_verify_GMAC_fail_when_tag_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authentication_verify_GMAC_fail_when_corruption(
			ts_params, ut_params, reference, 0);
}

static int
test_authenticated_decryption_fail_when_data_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authenticated_decryption_fail_when_corruption(
			ts_params, ut_params, reference, 1);
}

static int
test_authenticated_decryption_fail_when_tag_corrupted(
		struct crypto_testsuite_params *ts_params,
		struct crypto_unittest_params *ut_params,
		const struct test_crypto_vector *reference)
{
	return test_authenticated_decryption_fail_when_corruption(
			ts_params, ut_params, reference, 0);
}

static int
authentication_verify_HMAC_SHA1_fail_data_corrupt(void)
{
	return test_authentication_verify_fail_when_data_corrupted(
			&testsuite_params, &unittest_params,
			&hmac_sha1_test_crypto_vector);
}

static int
authentication_verify_HMAC_SHA1_fail_tag_corrupt(void)
{
	return test_authentication_verify_fail_when_tag_corrupted(
			&testsuite_params, &unittest_params,
			&hmac_sha1_test_crypto_vector);
}

static int
authentication_verify_AES128_GMAC_fail_data_corrupt(void)
{
	return test_authentication_verify_GMAC_fail_when_data_corrupted(
			&testsuite_params, &unittest_params,
			&aes128_gmac_test_vector);
}

static int
authentication_verify_AES128_GMAC_fail_tag_corrupt(void)
{
	return test_authentication_verify_GMAC_fail_when_tag_corrupted(
			&testsuite_params, &unittest_params,
			&aes128_gmac_test_vector);
}

static int
auth_decryption_AES128CBC_HMAC_SHA1_fail_data_corrupt(void)
{
	return test_authenticated_decryption_fail_when_data_corrupted(
			&testsuite_params,
			&unittest_params,
			&aes128cbc_hmac_sha1_test_vector);
}

static int
auth_decryption_AES128CBC_HMAC_SHA1_fail_tag_corrupt(void)
{
	return test_authenticated_decryption_fail_when_tag_corrupted(
			&testsuite_params,
			&unittest_params,
			&aes128cbc_hmac_sha1_test_vector);
}

static int
auth_encrypt_AES128CBC_HMAC_SHA1_esn_check(void)
{
	return test_authenticated_encrypt_with_esn(
			&testsuite_params,
			&unittest_params,
			&aes128cbc_hmac_sha1_aad_test_vector);
}

static int
auth_decrypt_AES128CBC_HMAC_SHA1_esn_check(void)
{
	return test_authenticated_decrypt_with_esn(
			&testsuite_params,
			&unittest_params,
			&aes128cbc_hmac_sha1_aad_test_vector);
}

static int
test_chacha20_poly1305_encrypt_test_case_rfc8439(void)
{
	return test_authenticated_encryption(&chacha20_poly1305_case_rfc8439);
}

static int
test_chacha20_poly1305_decrypt_test_case_rfc8439(void)
{
	return test_authenticated_decryption(&chacha20_poly1305_case_rfc8439);
}

static int
test_chacha20_poly1305_encrypt_SGL_out_of_place(void)
{
	return test_authenticated_encryption_SGL(
		&chacha20_poly1305_case_2, OUT_OF_PLACE, 32,
		chacha20_poly1305_case_2.plaintext.len);
}

#ifdef RTE_CRYPTO_SCHEDULER

/* global AESNI worker IDs for the scheduler test */
uint8_t aesni_ids[2];

static int
scheduler_testsuite_setup(void)
{
	uint32_t i = 0;
	int32_t nb_devs, ret;
	char vdev_args[VDEV_ARGS_SIZE] = {""};
	char temp_str[VDEV_ARGS_SIZE] = {"mode=multi-core,"
		"ordering=enable,name=cryptodev_test_scheduler,corelist="};
	uint16_t worker_core_count = 0;
	uint16_t socket_id = 0;

	if (gbl_driver_id == rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD))) {

		/* Identify the Worker Cores
		 * Use 2 worker cores for the device args
		 */
		RTE_LCORE_FOREACH_WORKER(i) {
			if (worker_core_count > 1)
				break;
			snprintf(vdev_args, sizeof(vdev_args),
					"%s%d", temp_str, i);
			strcpy(temp_str, vdev_args);
			strlcat(temp_str, ";", sizeof(temp_str));
			worker_core_count++;
			socket_id = rte_lcore_to_socket_id(i);
		}
		if (worker_core_count != 2) {
			RTE_LOG(ERR, USER1,
				"Cryptodev scheduler test require at least "
				"two worker cores to run. "
				"Please use the correct coremask.\n");
			return TEST_FAILED;
		}
		strcpy(temp_str, vdev_args);
		snprintf(vdev_args, sizeof(vdev_args), "%s,socket_id=%d",
				temp_str, socket_id);
		RTE_LOG(DEBUG, USER1, "vdev_args: %s\n", vdev_args);
		nb_devs = rte_cryptodev_device_count_by_driver(
				rte_cryptodev_driver_id_get(
				RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD)));
		if (nb_devs < 1) {
			ret = rte_vdev_init(
				RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD),
					vdev_args);
			TEST_ASSERT(ret == 0,
				"Failed to create instance %u of pmd : %s",
				i, RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD));
		}
	}
	return testsuite_setup();
}

static int
test_scheduler_attach_worker_op(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t sched_id = ts_params->valid_devs[0];
	uint32_t i, nb_devs_attached = 0;
	int ret;
	char vdev_name[32];
	unsigned int count = rte_cryptodev_count();

	/* create 2 AESNI_MB vdevs on top of existing devices */
	for (i = count; i < count + 2; i++) {
		snprintf(vdev_name, sizeof(vdev_name), "%s_%u",
				RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD),
				i);
		ret = rte_vdev_init(vdev_name, NULL);

		TEST_ASSERT(ret == 0,
			"Failed to create instance %u of"
			" pmd : %s",
			i, RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD));

		if (ret < 0) {
			RTE_LOG(ERR, USER1,
				"Failed to create 2 AESNI MB PMDs.\n");
			return TEST_SKIPPED;
		}
	}

	/* attach 2 AESNI_MB cdevs */
	for (i = count; i < count + 2; i++) {
		struct rte_cryptodev_info info;
		unsigned int session_size;

		rte_cryptodev_info_get(i, &info);
		if (info.driver_id != rte_cryptodev_driver_id_get(
				RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD)))
			continue;

		session_size = rte_cryptodev_sym_get_private_session_size(i);
		/*
		 * Create the session mempool again, since now there are new devices
		 * to use the mempool.
		 */
		if (ts_params->session_mpool) {
			rte_mempool_free(ts_params->session_mpool);
			ts_params->session_mpool = NULL;
		}

		if (info.sym.max_nb_sessions != 0 &&
				info.sym.max_nb_sessions < MAX_NB_SESSIONS) {
			RTE_LOG(ERR, USER1,
					"Device does not support "
					"at least %u sessions\n",
					MAX_NB_SESSIONS);
			return TEST_FAILED;
		}
		/*
		 * Create mempool with maximum number of sessions,
		 * to include the session headers
		 */
		if (ts_params->session_mpool == NULL) {
			ts_params->session_mpool =
				rte_cryptodev_sym_session_pool_create(
						"test_sess_mp",
						MAX_NB_SESSIONS, session_size,
						0, 0, SOCKET_ID_ANY);
			TEST_ASSERT_NOT_NULL(ts_params->session_mpool,
					"session mempool allocation failed");
		}

		ts_params->qp_conf.mp_session = ts_params->session_mpool;

		ret = rte_cryptodev_scheduler_worker_attach(sched_id,
				(uint8_t)i);

		TEST_ASSERT(ret == 0,
			"Failed to attach device %u of pmd : %s", i,
			RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD));

		aesni_ids[nb_devs_attached] = (uint8_t)i;

		nb_devs_attached++;
	}

	return 0;
}

static int
test_scheduler_detach_worker_op(void)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t sched_id = ts_params->valid_devs[0];
	uint32_t i;
	int ret;

	for (i = 0; i < 2; i++) {
		ret = rte_cryptodev_scheduler_worker_detach(sched_id,
				aesni_ids[i]);
		TEST_ASSERT(ret == 0,
			"Failed to detach device %u", aesni_ids[i]);
	}

	return 0;
}

static int
test_scheduler_mode_op(enum rte_cryptodev_scheduler_mode scheduler_mode)
{
	struct crypto_testsuite_params *ts_params = &testsuite_params;
	uint8_t sched_id = ts_params->valid_devs[0];
	/* set mode */
	return rte_cryptodev_scheduler_mode_set(sched_id,
		scheduler_mode);
}

static int
test_scheduler_mode_roundrobin_op(void)
{
	TEST_ASSERT(test_scheduler_mode_op(CDEV_SCHED_MODE_ROUNDROBIN) ==
			0, "Failed to set roundrobin mode");
	return 0;

}

static int
test_scheduler_mode_multicore_op(void)
{
	TEST_ASSERT(test_scheduler_mode_op(CDEV_SCHED_MODE_MULTICORE) ==
			0, "Failed to set multicore mode");

	return 0;
}

static int
test_scheduler_mode_failover_op(void)
{
	TEST_ASSERT(test_scheduler_mode_op(CDEV_SCHED_MODE_FAILOVER) ==
			0, "Failed to set failover mode");

	return 0;
}

static int
test_scheduler_mode_pkt_size_distr_op(void)
{
	TEST_ASSERT(test_scheduler_mode_op(CDEV_SCHED_MODE_PKT_SIZE_DISTR) ==
			0, "Failed to set pktsize mode");

	return 0;
}

static int
scheduler_multicore_testsuite_setup(void)
{
	if (test_scheduler_attach_worker_op() < 0)
		return TEST_SKIPPED;
	if (test_scheduler_mode_op(CDEV_SCHED_MODE_MULTICORE) < 0)
		return TEST_SKIPPED;
	return 0;
}

static int
scheduler_roundrobin_testsuite_setup(void)
{
	if (test_scheduler_attach_worker_op() < 0)
		return TEST_SKIPPED;
	if (test_scheduler_mode_op(CDEV_SCHED_MODE_ROUNDROBIN) < 0)
		return TEST_SKIPPED;
	return 0;
}

static int
scheduler_failover_testsuite_setup(void)
{
	if (test_scheduler_attach_worker_op() < 0)
		return TEST_SKIPPED;
	if (test_scheduler_mode_op(CDEV_SCHED_MODE_FAILOVER) < 0)
		return TEST_SKIPPED;
	return 0;
}

static int
scheduler_pkt_size_distr_testsuite_setup(void)
{
	if (test_scheduler_attach_worker_op() < 0)
		return TEST_SKIPPED;
	if (test_scheduler_mode_op(CDEV_SCHED_MODE_PKT_SIZE_DISTR) < 0)
		return TEST_SKIPPED;
	return 0;
}

static void
scheduler_mode_testsuite_teardown(void)
{
	test_scheduler_detach_worker_op();
}

#endif /* RTE_CRYPTO_SCHEDULER */

static struct unit_test_suite end_testsuite = {
	.suite_name = NULL,
	.setup = NULL,
	.teardown = NULL,
	.unit_test_suites = NULL
};

#ifdef RTE_LIB_SECURITY
static struct unit_test_suite ipsec_proto_testsuite  = {
	.suite_name = "IPsec Proto Unit Test Suite",
	.setup = ipsec_proto_testsuite_setup,
	.unit_test_cases = {
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-GCM 128)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec, &pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector ext_mbuf mode (ESP tunnel mode IPv4 AES-GCM 128)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_ext_mbuf, &pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-GCM 192)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec, &pkt_aes_192_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-GCM 256)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec, &pkt_aes_256_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-CCM 256)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec, &pkt_aes_256_ccm),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-CBC MD5 [12B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_aes_128_cbc_md5),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_aes_128_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA384 [24B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_aes_128_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA512 [32B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_aes_128_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv6 AES-GCM 128)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec, &pkt_aes_256_gcm_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv6 AES-CBC 128 HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_aes_128_cbc_hmac_sha256_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 NULL AES-XCBC-MAC [12B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_null_aes_xcbc),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_des_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA384 [24B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_des_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA512 [32B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_des_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec, &pkt_3des_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA384 [24B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec, &pkt_3des_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA512 [32B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec, &pkt_3des_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv6 DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_des_cbc_hmac_sha256_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (ESP tunnel mode IPv6 3DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec, &pkt_3des_cbc_hmac_sha256_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (AH tunnel mode IPv4 HMAC-SHA256)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_ah_tunnel_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (AH transport mode IPv4 HMAC-SHA256)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_ah_transport_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound known vector (AH transport mode IPv4 AES-GMAC 128)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec,
			&pkt_ah_ipv4_aes_gmac_128),
		TEST_CASE_NAMED_WITH_DATA(
			"Outbound fragmented packet",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_fragmented,
			&pkt_aes_128_gcm_frag),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-GCM 128)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb, &pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-GCM 192)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb, &pkt_aes_192_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-GCM 256)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb, &pkt_aes_256_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-CCM 256)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb, &pkt_aes_256_ccm),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-CBC 128)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb, &pkt_aes_128_cbc_null),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-CBC MD5 [12B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_aes_128_cbc_md5),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_aes_128_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA384 [24B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_aes_128_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-CBC 128 HMAC-SHA512 [32B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_aes_128_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv6 AES-GCM 128)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb, &pkt_aes_256_gcm_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv6 AES-CBC 128 HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_aes_128_cbc_hmac_sha256_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 NULL AES-XCBC-MAC [12B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_null_aes_xcbc),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_des_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA384 [24B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_des_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 DES-CBC HMAC-SHA512 [32B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_des_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb, &pkt_3des_cbc_hmac_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA384 [24B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb, &pkt_3des_cbc_hmac_sha384),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 3DES-CBC HMAC-SHA512 [32B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb, &pkt_3des_cbc_hmac_sha512),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv6 DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_des_cbc_hmac_sha256_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv6 3DES-CBC HMAC-SHA256 [16B ICV])",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb, &pkt_3des_cbc_hmac_sha256_v6),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (AH tunnel mode IPv4 HMAC-SHA256)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_ah_tunnel_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (AH transport mode IPv4 HMAC-SHA256)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_ah_transport_sha256),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (AH transport mode IPv4 AES-GMAC 128)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_known_vec_inb,
			&pkt_ah_ipv4_aes_gmac_128),
		TEST_CASE_NAMED_ST(
			"Combined test alg list",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_display_list),
		TEST_CASE_NAMED_ST(
			"Combined test alg list (AH)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ah_tunnel_ipv4),
		TEST_CASE_NAMED_ST(
			"IV generation",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_iv_gen),
		TEST_CASE_NAMED_ST(
			"UDP encapsulation",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_udp_encap),
		TEST_CASE_NAMED_ST(
			"UDP encapsulation with custom ports",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_udp_encap_custom_ports),
		TEST_CASE_NAMED_ST(
			"UDP encapsulation ports verification test",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_udp_ports_verify),
		TEST_CASE_NAMED_ST(
			"SA expiry packets soft",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_sa_exp_pkts_soft),
		TEST_CASE_NAMED_ST(
			"SA expiry packets hard",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_sa_exp_pkts_hard),
		TEST_CASE_NAMED_ST(
			"Negative test: ICV corruption",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_err_icv_corrupt),
		TEST_CASE_NAMED_ST(
			"Tunnel dst addr verification",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_tunnel_dst_addr_verify),
		TEST_CASE_NAMED_ST(
			"Tunnel src and dst addr verification",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_tunnel_src_dst_addr_verify),
		TEST_CASE_NAMED_ST(
			"Inner IP checksum",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_inner_ip_csum),
		TEST_CASE_NAMED_ST(
			"Inner L4 checksum",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_inner_l4_csum),
		TEST_CASE_NAMED_ST(
			"Tunnel IPv4 in IPv4",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_tunnel_v4_in_v4),
		TEST_CASE_NAMED_ST(
			"Tunnel IPv6 in IPv6",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_tunnel_v6_in_v6),
		TEST_CASE_NAMED_ST(
			"Tunnel IPv4 in IPv6",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_tunnel_v4_in_v6),
		TEST_CASE_NAMED_ST(
			"Tunnel IPv6 in IPv4",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_tunnel_v6_in_v4),
		TEST_CASE_NAMED_ST(
			"Transport IPv4",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_transport_v4),
		TEST_CASE_NAMED_ST(
			"AH transport IPv4",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ah_transport_ipv4),
		TEST_CASE_NAMED_ST(
			"Transport l4 checksum",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_transport_l4_csum),
		TEST_CASE_NAMED_ST(
			"Statistics: success",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_stats),
		TEST_CASE_NAMED_ST(
			"Fragmented packet",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_pkt_fragment),
		TEST_CASE_NAMED_ST(
			"Tunnel header copy DF (inner 0)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_copy_df_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header copy DF (inner 1)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_copy_df_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header set DF 0 (inner 1)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_set_df_0_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header set DF 1 (inner 0)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_set_df_1_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv4 copy DSCP (inner 0)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ipv4_copy_dscp_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv4 copy DSCP (inner 1)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ipv4_copy_dscp_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv4 set DSCP 0 (inner 1)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ipv4_set_dscp_0_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv4 set DSCP 1 (inner 0)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ipv4_set_dscp_1_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 copy DSCP (inner 0)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ipv6_copy_dscp_inner_0),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 copy DSCP (inner 1)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ipv6_copy_dscp_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 set DSCP 0 (inner 1)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ipv6_set_dscp_0_inner_1),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 set DSCP 1 (inner 0)",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ipv6_set_dscp_1_inner_0),
		TEST_CASE_NAMED_WITH_DATA(
			"Antireplay with window size 1024",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_pkt_antireplay1024, &pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Antireplay with window size 2048",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_pkt_antireplay2048, &pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"Antireplay with window size 4096",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_pkt_antireplay4096, &pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"ESN and Antireplay with window size 1024",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_pkt_esn_antireplay1024,
			&pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"ESN and Antireplay with window size 2048",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_pkt_esn_antireplay2048,
			&pkt_aes_128_gcm),
		TEST_CASE_NAMED_WITH_DATA(
			"ESN and Antireplay with window size 4096",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_pkt_esn_antireplay4096,
			&pkt_aes_128_gcm),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv4 decrement inner TTL",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ipv4_ttl_decrement),
		TEST_CASE_NAMED_ST(
			"Tunnel header IPv6 decrement inner hop limit",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_ipv6_hop_limit_decrement),
		TEST_CASE_NAMED_ST(
			"Multi-segmented mode",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_sgl),
		TEST_CASE_NAMED_ST(
			"Multi-segmented external mbuf mode",
			ut_setup_security, ut_teardown,
			test_ipsec_proto_sgl_ext_mbuf),
		TEST_CASE_NAMED_WITH_DATA(
			"Inbound known vector (ESP tunnel mode IPv4 AES-GCM 128) Rx inject",
			ut_setup_security_rx_inject, ut_teardown_rx_inject,
			test_ipsec_proto_known_vec_inb_rx_inject, &pkt_aes_128_gcm),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite pdcp_proto_testsuite  = {
	.suite_name = "PDCP Proto Unit Test Suite",
	.setup = pdcp_proto_testsuite_setup,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup_security, ut_teardown,
			test_PDCP_PROTO_all),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

#define ADD_UPLINK_TESTCASE(data)						\
	TEST_CASE_NAMED_WITH_DATA(data.test_descr_uplink, ut_setup_security,	\
	ut_teardown, test_docsis_proto_uplink, (const void *) &data),		\

#define ADD_DOWNLINK_TESTCASE(data)						\
	TEST_CASE_NAMED_WITH_DATA(data.test_descr_downlink, ut_setup_security,	\
	ut_teardown, test_docsis_proto_downlink, (const void *) &data),		\

static struct unit_test_suite docsis_proto_testsuite  = {
	.suite_name = "DOCSIS Proto Unit Test Suite",
	.setup = docsis_proto_testsuite_setup,
	.unit_test_cases = {
		/* Uplink */
		ADD_UPLINK_TESTCASE(docsis_test_case_1)
		ADD_UPLINK_TESTCASE(docsis_test_case_2)
		ADD_UPLINK_TESTCASE(docsis_test_case_3)
		ADD_UPLINK_TESTCASE(docsis_test_case_4)
		ADD_UPLINK_TESTCASE(docsis_test_case_5)
		ADD_UPLINK_TESTCASE(docsis_test_case_6)
		ADD_UPLINK_TESTCASE(docsis_test_case_7)
		ADD_UPLINK_TESTCASE(docsis_test_case_8)
		ADD_UPLINK_TESTCASE(docsis_test_case_9)
		ADD_UPLINK_TESTCASE(docsis_test_case_10)
		ADD_UPLINK_TESTCASE(docsis_test_case_11)
		ADD_UPLINK_TESTCASE(docsis_test_case_12)
		ADD_UPLINK_TESTCASE(docsis_test_case_13)
		ADD_UPLINK_TESTCASE(docsis_test_case_14)
		ADD_UPLINK_TESTCASE(docsis_test_case_15)
		ADD_UPLINK_TESTCASE(docsis_test_case_16)
		ADD_UPLINK_TESTCASE(docsis_test_case_17)
		ADD_UPLINK_TESTCASE(docsis_test_case_18)
		ADD_UPLINK_TESTCASE(docsis_test_case_19)
		ADD_UPLINK_TESTCASE(docsis_test_case_20)
		ADD_UPLINK_TESTCASE(docsis_test_case_21)
		ADD_UPLINK_TESTCASE(docsis_test_case_22)
		ADD_UPLINK_TESTCASE(docsis_test_case_23)
		ADD_UPLINK_TESTCASE(docsis_test_case_24)
		ADD_UPLINK_TESTCASE(docsis_test_case_25)
		ADD_UPLINK_TESTCASE(docsis_test_case_26)
		/* Downlink */
		ADD_DOWNLINK_TESTCASE(docsis_test_case_1)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_2)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_3)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_4)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_5)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_6)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_7)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_8)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_9)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_10)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_11)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_12)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_13)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_14)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_15)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_16)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_17)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_18)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_19)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_20)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_21)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_22)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_23)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_24)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_25)
		ADD_DOWNLINK_TESTCASE(docsis_test_case_26)
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};
#endif

static struct unit_test_suite cryptodev_gen_testsuite  = {
	.suite_name = "Crypto General Unit Test Suite",
	.setup = crypto_gen_testsuite_setup,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_device_reconfigure),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_device_configure_invalid_dev_id),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_queue_pair_descriptor_setup),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_device_configure_invalid_queue_pair_ids),
		TEST_CASE_ST(ut_setup, ut_teardown, test_stats),
		TEST_CASE_ST(ut_setup, ut_teardown, test_enq_callback_setup),
		TEST_CASE_ST(ut_setup, ut_teardown, test_deq_callback_setup),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_negative_hmac_sha1_testsuite = {
	.suite_name = "Negative HMAC SHA1 Unit Test Suite",
	.setup = negative_hmac_sha1_testsuite_setup,
	.unit_test_cases = {
		/** Negative tests */
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_HMAC_SHA1_fail_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_HMAC_SHA1_fail_tag_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			auth_decryption_AES128CBC_HMAC_SHA1_fail_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			auth_decryption_AES128CBC_HMAC_SHA1_fail_tag_corrupt),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_multi_session_testsuite = {
	.suite_name = "Multi Session Unit Test Suite",
	.setup = multi_session_testsuite_setup,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown, test_multi_session),
		TEST_CASE_ST(ut_setup, ut_teardown,
				test_multi_session_random_usage),

		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static struct unit_test_suite cryptodev_null_testsuite  = {
	.suite_name = "NULL Test Suite",
	.setup = null_testsuite_setup,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_null_invalid_operation),
		TEST_CASE_ST(ut_setup, ut_teardown, test_null_burst_operation),
		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_aes_ccm_auth_testsuite  = {
	.suite_name = "AES CCM Authenticated Test Suite",
	.setup = aes_ccm_auth_testsuite_setup,
	.unit_test_cases = {
		/** AES CCM Authenticated Encryption 128 bits key*/
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_encryption_test_case_128_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_encryption_test_case_128_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_encryption_test_case_128_3),

		/** AES CCM Authenticated Decryption 128 bits key*/
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_decryption_test_case_128_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_decryption_test_case_128_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_decryption_test_case_128_3),

		/** AES CCM Authenticated Encryption 192 bits key */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_encryption_test_case_192_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_encryption_test_case_192_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_encryption_test_case_192_3),

		/** AES CCM Authenticated Decryption 192 bits key*/
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_decryption_test_case_192_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_decryption_test_case_192_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_decryption_test_case_192_3),

		/** AES CCM Authenticated Encryption 256 bits key */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_encryption_test_case_256_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_encryption_test_case_256_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_encryption_test_case_256_3),

		/** AES CCM Authenticated Decryption 256 bits key*/
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_decryption_test_case_256_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_decryption_test_case_256_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_CCM_authenticated_decryption_test_case_256_3),
		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_aes_gcm_auth_testsuite  = {
	.suite_name = "AES GCM Authenticated Test Suite",
	.setup = aes_gcm_auth_testsuite_setup,
	.unit_test_cases = {
		/** AES GCM Authenticated Encryption */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encrypt_SGL_in_place_1500B),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encrypt_SGL_out_of_place_400B_400B),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encrypt_SGL_out_of_place_1500B_2000B),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encrypt_SGL_out_of_place_400B_1seg),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_encryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_encryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_encryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_encryption_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_encryption_test_case_7),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_encryption_test_case_8),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_J0_authenticated_encryption_test_case_1),

		/** AES GCM Authenticated Decryption */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_decryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_decryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_decryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_decryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_decryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_decryption_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_decryption_test_case_7),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_decryption_test_case_8),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_J0_authenticated_decryption_test_case_1),

		/** AES GCM Authenticated Encryption 192 bits key */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_192_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_192_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_192_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_192_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_192_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_192_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_192_7),

		/** AES GCM Authenticated Decryption 192 bits key */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_192_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_192_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_192_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_192_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_192_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_192_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_192_7),

		/** AES GCM Authenticated Encryption 256 bits key */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_256_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_256_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_256_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_256_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_256_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_256_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_256_7),

		/** AES GCM Authenticated Decryption 256 bits key */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_256_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_256_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_256_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_256_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_256_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_256_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_256_7),

		/** AES GCM Authenticated Encryption big aad size */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_aad_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_test_case_aad_2),

		/** AES GCM Authenticated Decryption big aad size */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_aad_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_test_case_aad_2),

		/** Out of place tests */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_encryption_oop_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_decryption_oop_test_case_1),

		/** Session-less tests */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_encryption_sessionless_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_authenticated_decryption_sessionless_test_case_1),

		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_aes_gmac_auth_testsuite  = {
	.suite_name = "AES GMAC Authentication Test Suite",
	.setup = aes_gmac_auth_testsuite_setup,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_verify_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_SGL_40B),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_SGL_80B),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_SGL_2048B),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GMAC_authentication_SGL_2047B),

		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_chacha20_poly1305_testsuite  = {
	.suite_name = "Chacha20-Poly1305 Test Suite",
	.setup = chacha20_poly1305_testsuite_setup,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_chacha20_poly1305_encrypt_test_case_rfc8439),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_chacha20_poly1305_decrypt_test_case_rfc8439),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_chacha20_poly1305_encrypt_SGL_out_of_place),
		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_snow3g_testsuite  = {
	.suite_name = "SNOW 3G Test Suite",
	.setup = snow3g_testsuite_setup,
	.unit_test_cases = {
		/** SNOW 3G encrypt only (UEA2) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_5),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1_oop_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1_oop_lb_in_sgl_out),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1_oop_sgl_in_lb_out),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_encryption_test_case_1_offset_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_1_oop),

		/** SNOW 3G generate auth, then encrypt (UEA2) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_test_case_2_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_part_digest_enc),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_part_digest_enc_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_test_case_3_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_test_case_3_oop_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_part_digest_enc_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_part_digest_enc_oop_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_total_digest_enc_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_total_digest_enc_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_total_digest_enc_1_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_total_digest_enc_1_oop_sgl),

		/** SNOW 3G decrypt (UEA2), then verify auth */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_test_case_2_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_part_digest_enc),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_part_digest_enc_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_test_case_3_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_test_case_3_oop_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_part_digest_enc_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_part_digest_enc_oop_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_total_digest_enc_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_total_digest_enc_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_total_digest_enc_1_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_verify_total_digest_enc_1_oop_sgl),

		/** SNOW 3G decrypt only (UEA2) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_decryption_with_digest_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_3),

		/* Tests with buffers which length is not byte-aligned */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_generate_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_3),

		/* Tests with buffers which length is not byte-aligned */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_hash_verify_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_cipher_auth_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_snow3g_auth_cipher_with_digest_test_case_1),
		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_zuc_testsuite  = {
	.suite_name = "ZUC Test Suite",
	.setup = zuc_testsuite_setup,
	.unit_test_cases = {
		/** ZUC encrypt only (EEA3) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_encryption_test_case_6_sgl),

		/** ZUC decrypt only (EEA3) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_decryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_decryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_decryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_decryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_decryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_decryption_test_case_6_sgl),

		/** ZUC authenticate (EIA3) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_7),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_generate_test_case_8),

		/** ZUC verify (EIA3) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_verify_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_verify_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_verify_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_verify_test_case_6),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_verify_test_case_7),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_hash_verify_test_case_8),

		/** ZUC alg-chain (EEA3/EIA3) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_cipher_auth_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_cipher_auth_test_case_2),

		/** ZUC generate auth, then encrypt (EEA3) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_test_case_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_test_case_1_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_test_case_1_oop_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_test_case_2_oop),

		/** ZUC decrypt (EEA3), then verify auth */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_verify_test_case_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_verify_test_case_1_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_verify_test_case_1_oop_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc_auth_cipher_verify_test_case_2_oop),

		/** ZUC-256 encrypt only **/
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_encryption_test_case_2),

		/** ZUC-256 decrypt only **/
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_decryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_decryption_test_case_2),

		/** ZUC-256 authentication only **/
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_hash_generate_4b_tag_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_hash_generate_4b_tag_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_hash_generate_4b_tag_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_hash_generate_8b_tag_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_hash_generate_16b_tag_test_case_1),

		/** ZUC-256 authentication verify only **/
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_hash_verify_4b_tag_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_hash_verify_4b_tag_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_hash_verify_4b_tag_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_hash_verify_8b_tag_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_hash_verify_16b_tag_test_case_1),

		/** ZUC-256 encrypt and authenticate **/
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_cipher_auth_4b_tag_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_cipher_auth_4b_tag_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_cipher_auth_8b_tag_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_cipher_auth_16b_tag_test_case_1),

		/** ZUC-256 generate auth, then encrypt */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_auth_cipher_4b_tag_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_auth_cipher_4b_tag_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_auth_cipher_8b_tag_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_auth_cipher_16b_tag_test_case_1),

		/** ZUC-256 decrypt, then verify auth */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_auth_cipher_verify_4b_tag_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_auth_cipher_verify_4b_tag_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_auth_cipher_verify_8b_tag_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_zuc256_auth_cipher_verify_16b_tag_test_case_1),

		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_hmac_md5_auth_testsuite  = {
	.suite_name = "HMAC_MD5 Authentication Test Suite",
	.setup = hmac_md5_auth_testsuite_setup,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_MD5_HMAC_generate_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_MD5_HMAC_verify_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_MD5_HMAC_generate_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_MD5_HMAC_verify_case_2),
		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_kasumi_testsuite  = {
	.suite_name = "Kasumi Test Suite",
	.setup = kasumi_testsuite_setup,
	.unit_test_cases = {
		/** KASUMI hash only (UIA1) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_generate_test_case_6),

		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_verify_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_verify_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_hash_verify_test_case_5),

		/** KASUMI encrypt only (UEA1) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_1_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_1_oop_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_encryption_test_case_5),

		/** KASUMI decrypt only (UEA1) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_3),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_4),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_5),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_decryption_test_case_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_cipher_auth_test_case_1),

		/** KASUMI generate auth, then encrypt (F8) */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_test_case_2_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_test_case_2_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_test_case_2_oop_sgl),

		/** KASUMI decrypt (F8), then verify auth */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_verify_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_verify_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_verify_test_case_2_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_verify_test_case_2_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_kasumi_auth_cipher_verify_test_case_2_oop_sgl),

		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_esn_testsuite  = {
	.suite_name = "ESN Test Suite",
	.setup = esn_testsuite_setup,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			auth_encrypt_AES128CBC_HMAC_SHA1_esn_check),
		TEST_CASE_ST(ut_setup, ut_teardown,
			auth_decrypt_AES128CBC_HMAC_SHA1_esn_check),
		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_negative_aes_gcm_testsuite  = {
	.suite_name = "Negative AES GCM Test Suite",
	.setup = negative_aes_gcm_testsuite_setup,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_fail_iv_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_fail_in_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_fail_out_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_fail_aad_len_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_fail_aad_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_encryption_fail_tag_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_fail_iv_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_fail_in_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_fail_out_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_fail_aad_len_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_fail_aad_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_AES_GCM_auth_decryption_fail_tag_corrupt),

		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_negative_aes_gmac_testsuite  = {
	.suite_name = "Negative AES GMAC Test Suite",
	.setup = negative_aes_gmac_testsuite_setup,
	.unit_test_cases = {
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_AES128_GMAC_fail_data_corrupt),
		TEST_CASE_ST(ut_setup, ut_teardown,
			authentication_verify_AES128_GMAC_fail_tag_corrupt),

		TEST_CASES_END()
	}
};

static struct unit_test_suite cryptodev_mixed_cipher_hash_testsuite  = {
	.suite_name = "Mixed CIPHER + HASH algorithms Test Suite",
	.setup = mixed_cipher_hash_testsuite_setup,
	.unit_test_cases = {
		/** AUTH AES CMAC + CIPHER AES CTR */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_aes_cmac_aes_ctr_digest_enc_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_aes_cmac_aes_ctr_digest_enc_test_case_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_aes_cmac_aes_ctr_digest_enc_test_case_1_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_aes_cmac_aes_ctr_digest_enc_test_case_1_oop_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_aes_cmac_aes_ctr_digest_enc_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_aes_cmac_aes_ctr_digest_enc_test_case_1_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_aes_cmac_aes_ctr_digest_enc_test_case_1_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_aes_cmac_aes_ctr_digest_enc_test_case_1_oop_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_aes_cmac_aes_ctr_digest_enc_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_aes_cmac_aes_ctr_digest_enc_test_case_2_oop),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_aes_cmac_aes_ctr_digest_enc_test_case_2),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_aes_cmac_aes_ctr_digest_enc_test_case_2_oop),

		/** AUTH ZUC + CIPHER SNOW3G */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_zuc_cipher_snow_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_zuc_cipher_snow_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_zuc_cipher_snow_test_case_1_inplace),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_zuc_cipher_snow_test_case_1_inplace),
		/** AUTH AES CMAC + CIPHER SNOW3G */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_aes_cmac_cipher_snow_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_aes_cmac_cipher_snow_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_aes_cmac_cipher_snow_test_case_1_inplace),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_aes_cmac_cipher_snow_test_case_1_inplace),
		/** AUTH ZUC + CIPHER AES CTR */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_zuc_cipher_aes_ctr_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_zuc_cipher_aes_ctr_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_zuc_cipher_aes_ctr_test_case_1_inplace),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_zuc_cipher_aes_ctr_test_case_1_inplace),
		/** AUTH SNOW3G + CIPHER AES CTR */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_snow_cipher_aes_ctr_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_snow_cipher_aes_ctr_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_snow_cipher_aes_ctr_test_case_1_inplace),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_snow_cipher_aes_ctr_test_case_1_inplace_sgl),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_snow_cipher_aes_ctr_test_case_1_inplace),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_snow_cipher_aes_ctr_test_case_1_inplace_sgl),
		/** AUTH SNOW3G + CIPHER ZUC */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_snow_cipher_zuc_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_snow_cipher_zuc_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_snow_cipher_zuc_test_case_1_inplace),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_snow_cipher_zuc_test_case_1_inplace),
		/** AUTH AES CMAC + CIPHER ZUC */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_aes_cmac_cipher_zuc_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_aes_cmac_cipher_zuc_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_aes_cmac_cipher_zuc_test_case_1_inplace),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_aes_cmac_cipher_zuc_test_case_1_inplace),

		/** AUTH NULL + CIPHER SNOW3G */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_null_cipher_snow_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_null_cipher_snow_test_case_1),
		/** AUTH NULL + CIPHER ZUC */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_null_cipher_zuc_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_null_cipher_zuc_test_case_1),
		/** AUTH SNOW3G + CIPHER NULL */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_snow_cipher_null_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_snow_cipher_null_test_case_1),
		/** AUTH ZUC + CIPHER NULL */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_zuc_cipher_null_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_zuc_cipher_null_test_case_1),
		/** AUTH NULL + CIPHER AES CTR */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_null_cipher_aes_ctr_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_null_cipher_aes_ctr_test_case_1),
		/** AUTH AES CMAC + CIPHER NULL */
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_auth_aes_cmac_cipher_null_test_case_1),
		TEST_CASE_ST(ut_setup, ut_teardown,
			test_verify_auth_aes_cmac_cipher_null_test_case_1),
		TEST_CASES_END()
	}
};

static int
run_cryptodev_testsuite(const char *pmd_name)
{
	uint8_t ret, j, i = 0, blk_start_idx = 0;
	const enum blockcipher_test_type blk_suites[] = {
		BLKCIPHER_AES_CHAIN_TYPE,
		BLKCIPHER_AES_CIPHERONLY_TYPE,
		BLKCIPHER_AES_DOCSIS_TYPE,
		BLKCIPHER_3DES_CHAIN_TYPE,
		BLKCIPHER_3DES_CIPHERONLY_TYPE,
		BLKCIPHER_DES_CIPHERONLY_TYPE,
		BLKCIPHER_DES_DOCSIS_TYPE,
		BLKCIPHER_SM4_CHAIN_TYPE,
		BLKCIPHER_SM4_CIPHERONLY_TYPE,
		BLKCIPHER_AUTHONLY_TYPE};
	struct unit_test_suite *static_suites[] = {
		&cryptodev_multi_session_testsuite,
		&cryptodev_null_testsuite,
		&cryptodev_aes_ccm_auth_testsuite,
		&cryptodev_aes_gcm_auth_testsuite,
		&cryptodev_aes_gmac_auth_testsuite,
		&cryptodev_snow3g_testsuite,
		&cryptodev_chacha20_poly1305_testsuite,
		&cryptodev_zuc_testsuite,
		&cryptodev_hmac_md5_auth_testsuite,
		&cryptodev_kasumi_testsuite,
		&cryptodev_esn_testsuite,
		&cryptodev_negative_aes_gcm_testsuite,
		&cryptodev_negative_aes_gmac_testsuite,
		&cryptodev_mixed_cipher_hash_testsuite,
		&cryptodev_negative_hmac_sha1_testsuite,
		&cryptodev_gen_testsuite,
#ifdef RTE_LIB_SECURITY
		&ipsec_proto_testsuite,
		&pdcp_proto_testsuite,
		&docsis_proto_testsuite,
#endif
		&end_testsuite
	};
	static struct unit_test_suite ts = {
		.suite_name = "Cryptodev Unit Test Suite",
		.setup = testsuite_setup,
		.teardown = testsuite_teardown,
		.unit_test_cases = {TEST_CASES_END()}
	};

	gbl_driver_id = rte_cryptodev_driver_id_get(pmd_name);

	if (gbl_driver_id == -1) {
		RTE_LOG(ERR, USER1, "%s PMD must be loaded.\n", pmd_name);
		return TEST_SKIPPED;
	}

	ts.unit_test_suites = malloc(sizeof(struct unit_test_suite *) *
			(RTE_DIM(blk_suites) + RTE_DIM(static_suites)));

	ADD_BLOCKCIPHER_TESTSUITE(i, ts, blk_suites, RTE_DIM(blk_suites));
	ADD_STATIC_TESTSUITE(i, ts, static_suites, RTE_DIM(static_suites));
	ret = unit_test_suite_runner(&ts);

	FREE_BLOCKCIPHER_TESTSUITE(blk_start_idx, ts, RTE_DIM(blk_suites));
	free(ts.unit_test_suites);
	return ret;
}

static int
require_feature_flag(const char *pmd_name, uint64_t flag, const char *flag_name)
{
	struct rte_cryptodev_info dev_info;
	uint8_t i, nb_devs;
	int driver_id;

	driver_id = rte_cryptodev_driver_id_get(pmd_name);
	if (driver_id == -1) {
		RTE_LOG(WARNING, USER1, "%s PMD must be loaded.\n", pmd_name);
		return TEST_SKIPPED;
	}

	nb_devs = rte_cryptodev_count();
	if (nb_devs < 1) {
		RTE_LOG(WARNING, USER1, "No crypto devices found?\n");
		return TEST_SKIPPED;
	}

	for (i = 0; i < nb_devs; i++) {
		rte_cryptodev_info_get(i, &dev_info);
		if (dev_info.driver_id == driver_id) {
			if (!(dev_info.feature_flags & flag)) {
				RTE_LOG(INFO, USER1, "%s not supported\n",
						flag_name);
				return TEST_SKIPPED;
			}
			return 0; /* found */
		}
	}

	RTE_LOG(INFO, USER1, "%s not supported\n", flag_name);
	return TEST_SKIPPED;
}

static int
test_cryptodev_qat(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_QAT_SYM_PMD));
}

static int
test_cryptodev_uadk(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_UADK_PMD));
}

static int
test_cryptodev_virtio(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_VIRTIO_PMD));
}

static int
test_cryptodev_aesni_mb(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD));
}

static int
test_cryptodev_cpu_aesni_mb(void)
{
	int32_t rc;
	enum rte_security_session_action_type at = gbl_action_type;
	gbl_action_type = RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO;
	rc = run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD));
	gbl_action_type = at;
	return rc;
}

static int
test_cryptodev_chacha_poly_mb(void)
{
	int32_t rc;
	enum rte_security_session_action_type at = gbl_action_type;
	rc = run_cryptodev_testsuite(
			RTE_STR(CRYPTODEV_NAME_CHACHA20_POLY1305_PMD));
	gbl_action_type = at;
	return rc;
}

static int
test_cryptodev_openssl(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_OPENSSL_PMD));
}

static int
test_cryptodev_aesni_gcm(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_AESNI_GCM_PMD));
}

static int
test_cryptodev_cpu_aesni_gcm(void)
{
	int32_t rc;
	enum rte_security_session_action_type at = gbl_action_type;
	gbl_action_type = RTE_SECURITY_ACTION_TYPE_CPU_CRYPTO;
	rc  = run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_AESNI_GCM_PMD));
	gbl_action_type = at;
	return rc;
}

static int
test_cryptodev_mlx5(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_MLX5_PMD));
}

static int
test_cryptodev_null(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_NULL_PMD));
}

static int
test_cryptodev_sw_snow3g(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_SNOW3G_PMD));
}

static int
test_cryptodev_sw_kasumi(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_KASUMI_PMD));
}

static int
test_cryptodev_sw_zuc(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_ZUC_PMD));
}

static int
test_cryptodev_armv8(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_ARMV8_PMD));
}

static int
test_cryptodev_mrvl(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_MVSAM_PMD));
}

#ifdef RTE_CRYPTO_SCHEDULER

static int
test_cryptodev_scheduler(void)
{
	uint8_t ret, sched_i, j, i = 0, blk_start_idx = 0;
	const enum blockcipher_test_type blk_suites[] = {
		BLKCIPHER_AES_CHAIN_TYPE,
		BLKCIPHER_AES_CIPHERONLY_TYPE,
		BLKCIPHER_AUTHONLY_TYPE
	};
	static struct unit_test_suite scheduler_multicore = {
		.suite_name = "Scheduler Multicore Unit Test Suite",
		.setup = scheduler_multicore_testsuite_setup,
		.teardown = scheduler_mode_testsuite_teardown,
		.unit_test_cases = {TEST_CASES_END()}
	};
	static struct unit_test_suite scheduler_round_robin = {
		.suite_name = "Scheduler Round Robin Unit Test Suite",
		.setup = scheduler_roundrobin_testsuite_setup,
		.teardown = scheduler_mode_testsuite_teardown,
		.unit_test_cases = {TEST_CASES_END()}
	};
	static struct unit_test_suite scheduler_failover = {
		.suite_name = "Scheduler Failover Unit Test Suite",
		.setup = scheduler_failover_testsuite_setup,
		.teardown = scheduler_mode_testsuite_teardown,
		.unit_test_cases = {TEST_CASES_END()}
	};
	static struct unit_test_suite scheduler_pkt_size_distr = {
		.suite_name = "Scheduler Pkt Size Distr Unit Test Suite",
		.setup = scheduler_pkt_size_distr_testsuite_setup,
		.teardown = scheduler_mode_testsuite_teardown,
		.unit_test_cases = {TEST_CASES_END()}
	};
	struct unit_test_suite *sched_mode_suites[] = {
		&scheduler_multicore,
		&scheduler_round_robin,
		&scheduler_failover,
		&scheduler_pkt_size_distr
	};
	static struct unit_test_suite scheduler_config = {
		.suite_name = "Crypto Device Scheduler Config Unit Test Suite",
		.unit_test_cases = {
			TEST_CASE(test_scheduler_attach_worker_op),
			TEST_CASE(test_scheduler_mode_multicore_op),
			TEST_CASE(test_scheduler_mode_roundrobin_op),
			TEST_CASE(test_scheduler_mode_failover_op),
			TEST_CASE(test_scheduler_mode_pkt_size_distr_op),
			TEST_CASE(test_scheduler_detach_worker_op),

			TEST_CASES_END() /**< NULL terminate array */
		}
	};
	struct unit_test_suite *static_suites[] = {
		&scheduler_config,
		&end_testsuite
	};
	struct unit_test_suite *sched_mode_static_suites[] = {
#ifdef RTE_LIB_SECURITY
		&docsis_proto_testsuite,
#endif
		&end_testsuite
	};
	static struct unit_test_suite ts = {
		.suite_name = "Scheduler Unit Test Suite",
		.setup = scheduler_testsuite_setup,
		.teardown = testsuite_teardown,
		.unit_test_cases = {TEST_CASES_END()}
	};

	gbl_driver_id =	rte_cryptodev_driver_id_get(
			RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD));

	if (gbl_driver_id == -1) {
		RTE_LOG(ERR, USER1, "SCHEDULER PMD must be loaded.\n");
		return TEST_SKIPPED;
	}

	if (rte_cryptodev_driver_id_get(
				RTE_STR(CRYPTODEV_NAME_AESNI_MB_PMD)) == -1) {
		RTE_LOG(ERR, USER1, "AESNI MB PMD must be loaded.\n");
		return TEST_SKIPPED;
	}

	for (sched_i = 0; sched_i < RTE_DIM(sched_mode_suites); sched_i++) {
		uint8_t blk_i = 0;
		sched_mode_suites[sched_i]->unit_test_suites = malloc(sizeof
				(struct unit_test_suite *) *
				(RTE_DIM(blk_suites) +
				RTE_DIM(sched_mode_static_suites) + 1));
		ADD_BLOCKCIPHER_TESTSUITE(blk_i, (*sched_mode_suites[sched_i]),
				blk_suites, RTE_DIM(blk_suites));
		ADD_STATIC_TESTSUITE(blk_i, (*sched_mode_suites[sched_i]),
				sched_mode_static_suites,
				RTE_DIM(sched_mode_static_suites));
		sched_mode_suites[sched_i]->unit_test_suites[blk_i] = &end_testsuite;
	}

	ts.unit_test_suites = malloc(sizeof(struct unit_test_suite *) *
			(RTE_DIM(static_suites) + RTE_DIM(sched_mode_suites)));
	ADD_STATIC_TESTSUITE(i, ts, sched_mode_suites,
			RTE_DIM(sched_mode_suites));
	ADD_STATIC_TESTSUITE(i, ts, static_suites, RTE_DIM(static_suites));
	ret = unit_test_suite_runner(&ts);

	for (sched_i = 0; sched_i < RTE_DIM(sched_mode_suites); sched_i++) {
		FREE_BLOCKCIPHER_TESTSUITE(blk_start_idx,
				(*sched_mode_suites[sched_i]),
				RTE_DIM(blk_suites));
		free(sched_mode_suites[sched_i]->unit_test_suites);
	}
	free(ts.unit_test_suites);
	return ret;
}

REGISTER_DRIVER_TEST(cryptodev_scheduler_autotest, test_cryptodev_scheduler);

#endif

static int
test_cryptodev_dpaa2_sec(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_DPAA2_SEC_PMD));
}

static int
test_cryptodev_dpaa_sec(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_DPAA_SEC_PMD));
}

static int
test_cryptodev_ccp(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_CCP_PMD));
}

static int
test_cryptodev_octeontx(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_OCTEONTX_SYM_PMD));
}

static int
test_cryptodev_caam_jr(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_CAAM_JR_PMD));
}

static int
test_cryptodev_nitrox(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_NITROX_PMD));
}

static int
test_cryptodev_bcmfs(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_BCMFS_PMD));
}

static int
run_cryptodev_raw_testsuite(const char *pmd_name)
{
	int ret;

	ret = require_feature_flag(pmd_name, RTE_CRYPTODEV_FF_SYM_RAW_DP, "RAW API");
	if (ret)
		return ret;

	global_api_test_type = CRYPTODEV_RAW_API_TEST;
	ret = run_cryptodev_testsuite(pmd_name);
	global_api_test_type = CRYPTODEV_API_TEST;

	return ret;
}

static int
test_cryptodev_qat_raw_api(void)
{
	return run_cryptodev_raw_testsuite(RTE_STR(CRYPTODEV_NAME_QAT_SYM_PMD));
}

static int
test_cryptodev_cn9k(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_CN9K_PMD));
}

static int
test_cryptodev_cn10k(void)
{
	return run_cryptodev_testsuite(RTE_STR(CRYPTODEV_NAME_CN10K_PMD));
}

static int
test_cryptodev_cn10k_raw_api(void)
{
	return run_cryptodev_raw_testsuite(RTE_STR(CRYPTODEV_NAME_CN10K_PMD));
}

static int
test_cryptodev_dpaa2_sec_raw_api(void)
{
	return run_cryptodev_raw_testsuite(RTE_STR(CRYPTODEV_NAME_DPAA2_SEC_PMD));
}

static int
test_cryptodev_dpaa_sec_raw_api(void)
{
	return run_cryptodev_raw_testsuite(RTE_STR(CRYPTODEV_NAME_DPAA_SEC_PMD));
}

REGISTER_DRIVER_TEST(cryptodev_cn10k_raw_api_autotest,
		test_cryptodev_cn10k_raw_api);
REGISTER_DRIVER_TEST(cryptodev_dpaa2_sec_raw_api_autotest,
		test_cryptodev_dpaa2_sec_raw_api);
REGISTER_DRIVER_TEST(cryptodev_dpaa_sec_raw_api_autotest,
		test_cryptodev_dpaa_sec_raw_api);
REGISTER_DRIVER_TEST(cryptodev_qat_raw_api_autotest,
		test_cryptodev_qat_raw_api);
REGISTER_DRIVER_TEST(cryptodev_qat_autotest, test_cryptodev_qat);
REGISTER_DRIVER_TEST(cryptodev_aesni_mb_autotest, test_cryptodev_aesni_mb);
REGISTER_DRIVER_TEST(cryptodev_cpu_aesni_mb_autotest,
	test_cryptodev_cpu_aesni_mb);
REGISTER_DRIVER_TEST(cryptodev_chacha_poly_mb_autotest,
	test_cryptodev_chacha_poly_mb);
REGISTER_DRIVER_TEST(cryptodev_openssl_autotest, test_cryptodev_openssl);
REGISTER_DRIVER_TEST(cryptodev_aesni_gcm_autotest, test_cryptodev_aesni_gcm);
REGISTER_DRIVER_TEST(cryptodev_cpu_aesni_gcm_autotest,
	test_cryptodev_cpu_aesni_gcm);
REGISTER_DRIVER_TEST(cryptodev_mlx5_autotest, test_cryptodev_mlx5);
REGISTER_DRIVER_TEST(cryptodev_null_autotest, test_cryptodev_null);
REGISTER_DRIVER_TEST(cryptodev_sw_snow3g_autotest, test_cryptodev_sw_snow3g);
REGISTER_DRIVER_TEST(cryptodev_sw_kasumi_autotest, test_cryptodev_sw_kasumi);
REGISTER_DRIVER_TEST(cryptodev_sw_zuc_autotest, test_cryptodev_sw_zuc);
REGISTER_DRIVER_TEST(cryptodev_sw_armv8_autotest, test_cryptodev_armv8);
REGISTER_DRIVER_TEST(cryptodev_sw_mvsam_autotest, test_cryptodev_mrvl);
REGISTER_DRIVER_TEST(cryptodev_dpaa2_sec_autotest, test_cryptodev_dpaa2_sec);
REGISTER_DRIVER_TEST(cryptodev_dpaa_sec_autotest, test_cryptodev_dpaa_sec);
REGISTER_DRIVER_TEST(cryptodev_ccp_autotest, test_cryptodev_ccp);
REGISTER_DRIVER_TEST(cryptodev_uadk_autotest, test_cryptodev_uadk);
REGISTER_DRIVER_TEST(cryptodev_virtio_autotest, test_cryptodev_virtio);
REGISTER_DRIVER_TEST(cryptodev_octeontx_autotest, test_cryptodev_octeontx);
REGISTER_DRIVER_TEST(cryptodev_caam_jr_autotest, test_cryptodev_caam_jr);
REGISTER_DRIVER_TEST(cryptodev_nitrox_autotest, test_cryptodev_nitrox);
REGISTER_DRIVER_TEST(cryptodev_bcmfs_autotest, test_cryptodev_bcmfs);
REGISTER_DRIVER_TEST(cryptodev_cn9k_autotest, test_cryptodev_cn9k);
REGISTER_DRIVER_TEST(cryptodev_cn10k_autotest, test_cryptodev_cn10k);
