/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2017.
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
 *     * Neither the name of Cavium networks nor the names of its
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

#include <stdbool.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>

#include "armv8_crypto_defs.h"

#include "rte_armv8_pmd_private.h"

static int cryptodev_armv8_crypto_uninit(struct rte_vdev_device *vdev);

/**
 * Pointers to the supported combined mode crypto functions are stored
 * in the static tables. Each combined (chained) cryptographic operation
 * can be described by a set of numbers:
 * - order:	order of operations (cipher, auth) or (auth, cipher)
 * - direction:	encryption or decryption
 * - calg:	cipher algorithm such as AES_CBC, AES_CTR, etc.
 * - aalg:	authentication algorithm such as SHA1, SHA256, etc.
 * - keyl:	cipher key length, for example 128, 192, 256 bits
 *
 * In order to quickly acquire each function pointer based on those numbers,
 * a hierarchy of arrays is maintained. The final level, 3D array is indexed
 * by the combined mode function parameters only (cipher algorithm,
 * authentication algorithm and key length).
 *
 * This gives 3 memory accesses to obtain a function pointer instead of
 * traversing the array manually and comparing function parameters on each loop.
 *
 *                   +--+CRYPTO_FUNC
 *            +--+ENC|
 *      +--+CA|
 *      |     +--+DEC
 * ORDER|
 *      |     +--+ENC
 *      +--+AC|
 *            +--+DEC
 *
 */

/**
 * 3D array type for ARM Combined Mode crypto functions pointers.
 * CRYPTO_CIPHER_MAX:			max cipher ID number
 * CRYPTO_AUTH_MAX:			max auth ID number
 * CRYPTO_CIPHER_KEYLEN_MAX:		max key length ID number
 */
typedef const crypto_func_t
crypto_func_tbl_t[CRYPTO_CIPHER_MAX][CRYPTO_AUTH_MAX][CRYPTO_CIPHER_KEYLEN_MAX];

/* Evaluate to key length definition */
#define KEYL(keyl)		(ARMV8_CRYPTO_CIPHER_KEYLEN_ ## keyl)

/* Local aliases for supported ciphers */
#define CIPH_AES_CBC		RTE_CRYPTO_CIPHER_AES_CBC
/* Local aliases for supported hashes */
#define AUTH_SHA1_HMAC		RTE_CRYPTO_AUTH_SHA1_HMAC
#define AUTH_SHA256_HMAC	RTE_CRYPTO_AUTH_SHA256_HMAC

/**
 * Arrays containing pointers to particular cryptographic,
 * combined mode functions.
 * crypto_op_ca_encrypt:	cipher (encrypt), authenticate
 * crypto_op_ca_decrypt:	cipher (decrypt), authenticate
 * crypto_op_ac_encrypt:	authenticate, cipher (encrypt)
 * crypto_op_ac_decrypt:	authenticate, cipher (decrypt)
 */
static const crypto_func_tbl_t
crypto_op_ca_encrypt = {
	/* [cipher alg][auth alg][key length] = crypto_function, */
	[CIPH_AES_CBC][AUTH_SHA1_HMAC][KEYL(128)] = aes128cbc_sha1_hmac,
	[CIPH_AES_CBC][AUTH_SHA256_HMAC][KEYL(128)] = aes128cbc_sha256_hmac,
};

static const crypto_func_tbl_t
crypto_op_ca_decrypt = {
	NULL
};

static const crypto_func_tbl_t
crypto_op_ac_encrypt = {
	NULL
};

static const crypto_func_tbl_t
crypto_op_ac_decrypt = {
	/* [cipher alg][auth alg][key length] = crypto_function, */
	[CIPH_AES_CBC][AUTH_SHA1_HMAC][KEYL(128)] = sha1_hmac_aes128cbc_dec,
	[CIPH_AES_CBC][AUTH_SHA256_HMAC][KEYL(128)] = sha256_hmac_aes128cbc_dec,
};

/**
 * Arrays containing pointers to particular cryptographic function sets,
 * covering given cipher operation directions (encrypt, decrypt)
 * for each order of cipher and authentication pairs.
 */
static const crypto_func_tbl_t *
crypto_cipher_auth[] = {
	&crypto_op_ca_encrypt,
	&crypto_op_ca_decrypt,
	NULL
};

static const crypto_func_tbl_t *
crypto_auth_cipher[] = {
	&crypto_op_ac_encrypt,
	&crypto_op_ac_decrypt,
	NULL
};

/**
 * Top level array containing pointers to particular cryptographic
 * function sets, covering given order of chained operations.
 * crypto_cipher_auth:	cipher first, authenticate after
 * crypto_auth_cipher:	authenticate first, cipher after
 */
static const crypto_func_tbl_t **
crypto_chain_order[] = {
	crypto_cipher_auth,
	crypto_auth_cipher,
	NULL
};

/**
 * Extract particular combined mode crypto function from the 3D array.
 */
#define CRYPTO_GET_ALGO(order, cop, calg, aalg, keyl)			\
({									\
	crypto_func_tbl_t *func_tbl =					\
				(crypto_chain_order[(order)])[(cop)];	\
									\
	((*func_tbl)[(calg)][(aalg)][KEYL(keyl)]);		\
})

/*----------------------------------------------------------------------------*/

/**
 * 2D array type for ARM key schedule functions pointers.
 * CRYPTO_CIPHER_MAX:			max cipher ID number
 * CRYPTO_CIPHER_KEYLEN_MAX:		max key length ID number
 */
typedef const crypto_key_sched_t
crypto_key_sched_tbl_t[CRYPTO_CIPHER_MAX][CRYPTO_CIPHER_KEYLEN_MAX];

static const crypto_key_sched_tbl_t
crypto_key_sched_encrypt = {
	/* [cipher alg][key length] = key_expand_func, */
	[CIPH_AES_CBC][KEYL(128)] = aes128_key_sched_enc,
};

static const crypto_key_sched_tbl_t
crypto_key_sched_decrypt = {
	/* [cipher alg][key length] = key_expand_func, */
	[CIPH_AES_CBC][KEYL(128)] = aes128_key_sched_dec,
};

/**
 * Top level array containing pointers to particular key generation
 * function sets, covering given operation direction.
 * crypto_key_sched_encrypt:	keys for encryption
 * crypto_key_sched_decrypt:	keys for decryption
 */
static const crypto_key_sched_tbl_t *
crypto_key_sched_dir[] = {
	&crypto_key_sched_encrypt,
	&crypto_key_sched_decrypt,
	NULL
};

/**
 * Extract particular combined mode crypto function from the 3D array.
 */
#define CRYPTO_GET_KEY_SCHED(cop, calg, keyl)				\
({									\
	crypto_key_sched_tbl_t *ks_tbl = crypto_key_sched_dir[(cop)];	\
									\
	((*ks_tbl)[(calg)][KEYL(keyl)]);				\
})

/*----------------------------------------------------------------------------*/

/*
 *------------------------------------------------------------------------------
 * Session Prepare
 *------------------------------------------------------------------------------
 */

/** Get xform chain order */
static enum armv8_crypto_chain_order
armv8_crypto_get_chain_order(const struct rte_crypto_sym_xform *xform)
{

	/*
	 * This driver currently covers only chained operations.
	 * Ignore only cipher or only authentication operations
	 * or chains longer than 2 xform structures.
	 */
	if (xform->next == NULL || xform->next->next != NULL)
		return ARMV8_CRYPTO_CHAIN_NOT_SUPPORTED;

	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return ARMV8_CRYPTO_CHAIN_AUTH_CIPHER;
	}

	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return ARMV8_CRYPTO_CHAIN_CIPHER_AUTH;
	}

	return ARMV8_CRYPTO_CHAIN_NOT_SUPPORTED;
}

static inline void
auth_hmac_pad_prepare(struct armv8_crypto_session *sess,
				const struct rte_crypto_sym_xform *xform)
{
	size_t i;

	/* Generate i_key_pad and o_key_pad */
	memset(sess->auth.hmac.i_key_pad, 0, sizeof(sess->auth.hmac.i_key_pad));
	rte_memcpy(sess->auth.hmac.i_key_pad, sess->auth.hmac.key,
							xform->auth.key.length);
	memset(sess->auth.hmac.o_key_pad, 0, sizeof(sess->auth.hmac.o_key_pad));
	rte_memcpy(sess->auth.hmac.o_key_pad, sess->auth.hmac.key,
							xform->auth.key.length);
	/*
	 * XOR key with IPAD/OPAD values to obtain i_key_pad
	 * and o_key_pad.
	 * Byte-by-byte operation may seem to be the less efficient
	 * here but in fact it's the opposite.
	 * The result ASM code is likely operate on NEON registers
	 * (load auth key to Qx, load IPAD/OPAD to multiple
	 * elements of Qy, eor 128 bits at once).
	 */
	for (i = 0; i < SHA_BLOCK_MAX; i++) {
		sess->auth.hmac.i_key_pad[i] ^= HMAC_IPAD_VALUE;
		sess->auth.hmac.o_key_pad[i] ^= HMAC_OPAD_VALUE;
	}
}

static inline int
auth_set_prerequisites(struct armv8_crypto_session *sess,
			const struct rte_crypto_sym_xform *xform)
{
	uint8_t partial[64] = { 0 };
	int error;

	switch (xform->auth.algo) {
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		/*
		 * Generate authentication key, i_key_pad and o_key_pad.
		 */
		/* Zero memory under key */
		memset(sess->auth.hmac.key, 0, SHA1_AUTH_KEY_LENGTH);

		if (xform->auth.key.length > SHA1_AUTH_KEY_LENGTH) {
			/*
			 * In case the key is longer than 160 bits
			 * the algorithm will use SHA1(key) instead.
			 */
			error = sha1_block(NULL, xform->auth.key.data,
				sess->auth.hmac.key, xform->auth.key.length);
			if (error != 0)
				return -1;
		} else {
			/*
			 * Now copy the given authentication key to the session
			 * key assuming that the session key is zeroed there is
			 * no need for additional zero padding if the key is
			 * shorter than SHA1_AUTH_KEY_LENGTH.
			 */
			rte_memcpy(sess->auth.hmac.key, xform->auth.key.data,
							xform->auth.key.length);
		}

		/* Prepare HMAC padding: key|pattern */
		auth_hmac_pad_prepare(sess, xform);
		/*
		 * Calculate partial hash values for i_key_pad and o_key_pad.
		 * Will be used as initialization state for final HMAC.
		 */
		error = sha1_block_partial(NULL, sess->auth.hmac.i_key_pad,
		    partial, SHA1_BLOCK_SIZE);
		if (error != 0)
			return -1;
		memcpy(sess->auth.hmac.i_key_pad, partial, SHA1_BLOCK_SIZE);

		error = sha1_block_partial(NULL, sess->auth.hmac.o_key_pad,
		    partial, SHA1_BLOCK_SIZE);
		if (error != 0)
			return -1;
		memcpy(sess->auth.hmac.o_key_pad, partial, SHA1_BLOCK_SIZE);

		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		/*
		 * Generate authentication key, i_key_pad and o_key_pad.
		 */
		/* Zero memory under key */
		memset(sess->auth.hmac.key, 0, SHA256_AUTH_KEY_LENGTH);

		if (xform->auth.key.length > SHA256_AUTH_KEY_LENGTH) {
			/*
			 * In case the key is longer than 256 bits
			 * the algorithm will use SHA256(key) instead.
			 */
			error = sha256_block(NULL, xform->auth.key.data,
				sess->auth.hmac.key, xform->auth.key.length);
			if (error != 0)
				return -1;
		} else {
			/*
			 * Now copy the given authentication key to the session
			 * key assuming that the session key is zeroed there is
			 * no need for additional zero padding if the key is
			 * shorter than SHA256_AUTH_KEY_LENGTH.
			 */
			rte_memcpy(sess->auth.hmac.key, xform->auth.key.data,
							xform->auth.key.length);
		}

		/* Prepare HMAC padding: key|pattern */
		auth_hmac_pad_prepare(sess, xform);
		/*
		 * Calculate partial hash values for i_key_pad and o_key_pad.
		 * Will be used as initialization state for final HMAC.
		 */
		error = sha256_block_partial(NULL, sess->auth.hmac.i_key_pad,
		    partial, SHA256_BLOCK_SIZE);
		if (error != 0)
			return -1;
		memcpy(sess->auth.hmac.i_key_pad, partial, SHA256_BLOCK_SIZE);

		error = sha256_block_partial(NULL, sess->auth.hmac.o_key_pad,
		    partial, SHA256_BLOCK_SIZE);
		if (error != 0)
			return -1;
		memcpy(sess->auth.hmac.o_key_pad, partial, SHA256_BLOCK_SIZE);

		break;
	default:
		break;
	}

	return 0;
}

static inline int
cipher_set_prerequisites(struct armv8_crypto_session *sess,
			const struct rte_crypto_sym_xform *xform)
{
	crypto_key_sched_t cipher_key_sched;

	cipher_key_sched = sess->cipher.key_sched;
	if (likely(cipher_key_sched != NULL)) {
		/* Set up cipher session key */
		cipher_key_sched(sess->cipher.key.data, xform->cipher.key.data);
	}

	return 0;
}

static int
armv8_crypto_set_session_chained_parameters(struct armv8_crypto_session *sess,
		const struct rte_crypto_sym_xform *cipher_xform,
		const struct rte_crypto_sym_xform *auth_xform)
{
	enum armv8_crypto_chain_order order;
	enum armv8_crypto_cipher_operation cop;
	enum rte_crypto_cipher_algorithm calg;
	enum rte_crypto_auth_algorithm aalg;

	/* Validate and prepare scratch order of combined operations */
	switch (sess->chain_order) {
	case ARMV8_CRYPTO_CHAIN_CIPHER_AUTH:
	case ARMV8_CRYPTO_CHAIN_AUTH_CIPHER:
		order = sess->chain_order;
		break;
	default:
		return -EINVAL;
	}
	/* Select cipher direction */
	sess->cipher.direction = cipher_xform->cipher.op;
	/* Select cipher key */
	sess->cipher.key.length = cipher_xform->cipher.key.length;
	/* Set cipher direction */
	cop = sess->cipher.direction;
	/* Set cipher algorithm */
	calg = cipher_xform->cipher.algo;

	/* Select cipher algo */
	switch (calg) {
	/* Cover supported cipher algorithms */
	case RTE_CRYPTO_CIPHER_AES_CBC:
		sess->cipher.algo = calg;
		/* IV len is always 16 bytes (block size) for AES CBC */
		sess->cipher.iv_len = 16;
		break;
	default:
		return -EINVAL;
	}
	/* Select auth generate/verify */
	sess->auth.operation = auth_xform->auth.op;

	/* Select auth algo */
	switch (auth_xform->auth.algo) {
	/* Cover supported hash algorithms */
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
	case RTE_CRYPTO_AUTH_SHA256_HMAC: /* Fall through */
		aalg = auth_xform->auth.algo;
		sess->auth.mode = ARMV8_CRYPTO_AUTH_AS_HMAC;
		break;
	default:
		return -EINVAL;
	}

	/* Verify supported key lengths and extract proper algorithm */
	switch (cipher_xform->cipher.key.length << 3) {
	case 128:
		sess->crypto_func =
				CRYPTO_GET_ALGO(order, cop, calg, aalg, 128);
		sess->cipher.key_sched =
				CRYPTO_GET_KEY_SCHED(cop, calg, 128);
		break;
	case 192:
	case 256:
		/* These key lengths are not supported yet */
	default: /* Fall through */
		sess->crypto_func = NULL;
		sess->cipher.key_sched = NULL;
		return -EINVAL;
	}

	if (unlikely(sess->crypto_func == NULL)) {
		/*
		 * If we got here that means that there must be a bug
		 * in the algorithms selection above. Nevertheless keep
		 * it here to catch bug immediately and avoid NULL pointer
		 * dereference in OPs processing.
		 */
		ARMV8_CRYPTO_LOG_ERR(
			"No appropriate crypto function for given parameters");
		return -EINVAL;
	}

	/* Set up cipher session prerequisites */
	if (cipher_set_prerequisites(sess, cipher_xform) != 0)
		return -EINVAL;

	/* Set up authentication session prerequisites */
	if (auth_set_prerequisites(sess, auth_xform) != 0)
		return -EINVAL;

	return 0;
}

/** Parse crypto xform chain and set private session parameters */
int
armv8_crypto_set_session_parameters(struct armv8_crypto_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	bool is_chained_op;
	int ret;

	/* Filter out spurious/broken requests */
	if (xform == NULL)
		return -EINVAL;

	sess->chain_order = armv8_crypto_get_chain_order(xform);
	switch (sess->chain_order) {
	case ARMV8_CRYPTO_CHAIN_CIPHER_AUTH:
		cipher_xform = xform;
		auth_xform = xform->next;
		is_chained_op = true;
		break;
	case ARMV8_CRYPTO_CHAIN_AUTH_CIPHER:
		auth_xform = xform;
		cipher_xform = xform->next;
		is_chained_op = true;
		break;
	default:
		is_chained_op = false;
		return -EINVAL;
	}

	if (is_chained_op) {
		ret = armv8_crypto_set_session_chained_parameters(sess,
						cipher_xform, auth_xform);
		if (unlikely(ret != 0)) {
			ARMV8_CRYPTO_LOG_ERR(
			"Invalid/unsupported chained (cipher/auth) parameters");
			return -EINVAL;
		}
	} else {
		ARMV8_CRYPTO_LOG_ERR("Invalid/unsupported operation");
		return -EINVAL;
	}

	return 0;
}

/** Provide session for operation */
static inline struct armv8_crypto_session *
get_session(struct armv8_crypto_qp *qp, struct rte_crypto_op *op)
{
	struct armv8_crypto_session *sess = NULL;

	if (op->sym->sess_type == RTE_CRYPTO_SYM_OP_WITH_SESSION) {
		/* get existing session */
		if (likely(op->sym->session != NULL &&
				op->sym->session->dev_type ==
				RTE_CRYPTODEV_ARMV8_PMD)) {
			sess = (struct armv8_crypto_session *)
				op->sym->session->_private;
		}
	} else {
		/* provide internal session */
		void *_sess = NULL;

		if (!rte_mempool_get(qp->sess_mp, (void **)&_sess)) {
			sess = (struct armv8_crypto_session *)
				((struct rte_cryptodev_sym_session *)_sess)
				->_private;

			if (unlikely(armv8_crypto_set_session_parameters(
					sess, op->sym->xform) != 0)) {
				rte_mempool_put(qp->sess_mp, _sess);
				sess = NULL;
			} else
				op->sym->session = _sess;
		}
	}

	if (unlikely(sess == NULL))
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;

	return sess;
}

/*
 *------------------------------------------------------------------------------
 * Process Operations
 *------------------------------------------------------------------------------
 */

/*----------------------------------------------------------------------------*/

/** Process cipher operation */
static inline void
process_armv8_chained_op
		(struct rte_crypto_op *op, struct armv8_crypto_session *sess,
		struct rte_mbuf *mbuf_src, struct rte_mbuf *mbuf_dst)
{
	crypto_func_t crypto_func;
	crypto_arg_t arg;
	struct rte_mbuf *m_asrc, *m_adst;
	uint8_t *csrc, *cdst;
	uint8_t *adst, *asrc;
	uint64_t clen, alen;
	int error;

	clen = op->sym->cipher.data.length;
	alen = op->sym->auth.data.length;

	csrc = rte_pktmbuf_mtod_offset(mbuf_src, uint8_t *,
			op->sym->cipher.data.offset);
	cdst = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
			op->sym->cipher.data.offset);

	switch (sess->chain_order) {
	case ARMV8_CRYPTO_CHAIN_CIPHER_AUTH:
		m_asrc = m_adst = mbuf_dst;
		break;
	case ARMV8_CRYPTO_CHAIN_AUTH_CIPHER:
		m_asrc = mbuf_src;
		m_adst = mbuf_dst;
		break;
	default:
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return;
	}
	asrc = rte_pktmbuf_mtod_offset(m_asrc, uint8_t *,
				op->sym->auth.data.offset);

	switch (sess->auth.mode) {
	case ARMV8_CRYPTO_AUTH_AS_AUTH:
		/* Nothing to do here, just verify correct option */
		break;
	case ARMV8_CRYPTO_AUTH_AS_HMAC:
		arg.digest.hmac.key = sess->auth.hmac.key;
		arg.digest.hmac.i_key_pad = sess->auth.hmac.i_key_pad;
		arg.digest.hmac.o_key_pad = sess->auth.hmac.o_key_pad;
		break;
	default:
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return;
	}

	if (sess->auth.operation == RTE_CRYPTO_AUTH_OP_GENERATE) {
		adst = op->sym->auth.digest.data;
		if (adst == NULL) {
			adst = rte_pktmbuf_mtod_offset(m_adst,
					uint8_t *,
					op->sym->auth.data.offset +
					op->sym->auth.data.length);
		}
	} else {
		adst = (uint8_t *)rte_pktmbuf_append(m_asrc,
				op->sym->auth.digest.length);
	}

	if (unlikely(op->sym->cipher.iv.length != sess->cipher.iv_len)) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return;
	}

	arg.cipher.iv = op->sym->cipher.iv.data;
	arg.cipher.key = sess->cipher.key.data;
	/* Acquire combined mode function */
	crypto_func = sess->crypto_func;
	ARMV8_CRYPTO_ASSERT(crypto_func != NULL);
	error = crypto_func(csrc, cdst, clen, asrc, adst, alen, &arg);
	if (error != 0) {
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;
		return;
	}

	op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	if (sess->auth.operation == RTE_CRYPTO_AUTH_OP_VERIFY) {
		if (memcmp(adst, op->sym->auth.digest.data,
				op->sym->auth.digest.length) != 0) {
			op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
		}
		/* Trim area used for digest from mbuf. */
		rte_pktmbuf_trim(m_asrc,
				op->sym->auth.digest.length);
	}
}

/** Process crypto operation for mbuf */
static inline int
process_op(const struct armv8_crypto_qp *qp, struct rte_crypto_op *op,
		struct armv8_crypto_session *sess)
{
	struct rte_mbuf *msrc, *mdst;

	msrc = op->sym->m_src;
	mdst = op->sym->m_dst ? op->sym->m_dst : op->sym->m_src;

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

	switch (sess->chain_order) {
	case ARMV8_CRYPTO_CHAIN_CIPHER_AUTH:
	case ARMV8_CRYPTO_CHAIN_AUTH_CIPHER: /* Fall through */
		process_armv8_chained_op(op, sess, msrc, mdst);
		break;
	default:
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		break;
	}

	/* Free session if a session-less crypto op */
	if (op->sym->sess_type == RTE_CRYPTO_SYM_OP_SESSIONLESS) {
		memset(sess, 0, sizeof(struct armv8_crypto_session));
		rte_mempool_put(qp->sess_mp, op->sym->session);
		op->sym->session = NULL;
	}

	if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;

	if (unlikely(op->status == RTE_CRYPTO_OP_STATUS_ERROR))
		return -1;

	return 0;
}

/*
 *------------------------------------------------------------------------------
 * PMD Framework
 *------------------------------------------------------------------------------
 */

/** Enqueue burst */
static uint16_t
armv8_crypto_pmd_enqueue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct armv8_crypto_session *sess;
	struct armv8_crypto_qp *qp = queue_pair;
	int i, retval;

	for (i = 0; i < nb_ops; i++) {
		sess = get_session(qp, ops[i]);
		if (unlikely(sess == NULL))
			goto enqueue_err;

		retval = process_op(qp, ops[i], sess);
		if (unlikely(retval < 0))
			goto enqueue_err;
	}

	retval = rte_ring_enqueue_burst(qp->processed_ops, (void *)ops, i,
			NULL);
	qp->stats.enqueued_count += retval;

	return retval;

enqueue_err:
	retval = rte_ring_enqueue_burst(qp->processed_ops, (void *)ops, i,
			NULL);
	if (ops[i] != NULL)
		ops[i]->status = RTE_CRYPTO_OP_STATUS_INVALID_ARGS;

	qp->stats.enqueue_err_count++;
	return retval;
}

/** Dequeue burst */
static uint16_t
armv8_crypto_pmd_dequeue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct armv8_crypto_qp *qp = queue_pair;

	unsigned int nb_dequeued = 0;

	nb_dequeued = rte_ring_dequeue_burst(qp->processed_ops,
			(void **)ops, nb_ops, NULL);
	qp->stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

/** Create ARMv8 crypto device */
static int
cryptodev_armv8_crypto_create(const char *name,
			struct rte_vdev_device *vdev,
			struct rte_crypto_vdev_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct armv8_crypto_private *internals;

	/* Check CPU for support for AES instruction set */
	if (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_AES)) {
		ARMV8_CRYPTO_LOG_ERR(
			"AES instructions not supported by CPU");
		return -EFAULT;
	}

	/* Check CPU for support for SHA instruction set */
	if (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_SHA1) ||
	    !rte_cpu_get_flag_enabled(RTE_CPUFLAG_SHA2)) {
		ARMV8_CRYPTO_LOG_ERR(
			"SHA1/SHA2 instructions not supported by CPU");
		return -EFAULT;
	}

	/* Check CPU for support for Advance SIMD instruction set */
	if (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_NEON)) {
		ARMV8_CRYPTO_LOG_ERR(
			"Advanced SIMD instructions not supported by CPU");
		return -EFAULT;
	}

	if (init_params->name[0] == '\0')
		snprintf(init_params->name, sizeof(init_params->name),
				"%s", name);

	dev = rte_cryptodev_pmd_virtual_dev_init(init_params->name,
				sizeof(struct armv8_crypto_private),
				init_params->socket_id);
	if (dev == NULL) {
		ARMV8_CRYPTO_LOG_ERR("failed to create cryptodev vdev");
		goto init_error;
	}

	dev->dev_type = RTE_CRYPTODEV_ARMV8_PMD;
	dev->dev_ops = rte_armv8_crypto_pmd_ops;

	/* register rx/tx burst functions for data path */
	dev->dequeue_burst = armv8_crypto_pmd_dequeue_burst;
	dev->enqueue_burst = armv8_crypto_pmd_enqueue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_CPU_NEON |
			RTE_CRYPTODEV_FF_CPU_ARM_CE;

	/* Set vector instructions mode supported */
	internals = dev->data->dev_private;

	internals->max_nb_qpairs = init_params->max_nb_queue_pairs;
	internals->max_nb_sessions = init_params->max_nb_sessions;

	return 0;

init_error:
	ARMV8_CRYPTO_LOG_ERR(
		"driver %s: cryptodev_armv8_crypto_create failed",
		init_params->name);

	cryptodev_armv8_crypto_uninit(vdev);
	return -EFAULT;
}

/** Initialise ARMv8 crypto device */
static int
cryptodev_armv8_crypto_init(struct rte_vdev_device *vdev)
{
	struct rte_crypto_vdev_init_params init_params = {
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_QUEUE_PAIRS,
		RTE_CRYPTODEV_VDEV_DEFAULT_MAX_NB_SESSIONS,
		rte_socket_id(),
		{0}
	};
	const char *name;
	const char *input_args;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;
	input_args = rte_vdev_device_args(vdev);
	rte_cryptodev_parse_vdev_init_params(&init_params, input_args);

	RTE_LOG(INFO, PMD, "Initialising %s on NUMA node %d\n", name,
			init_params.socket_id);
	if (init_params.name[0] != '\0') {
		RTE_LOG(INFO, PMD, "  User defined name = %s\n",
			init_params.name);
	}
	RTE_LOG(INFO, PMD, "  Max number of queue pairs = %d\n",
			init_params.max_nb_queue_pairs);
	RTE_LOG(INFO, PMD, "  Max number of sessions = %d\n",
			init_params.max_nb_sessions);

	return cryptodev_armv8_crypto_create(name, vdev, &init_params);
}

/** Uninitialise ARMv8 crypto device */
static int
cryptodev_armv8_crypto_uninit(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	RTE_LOG(INFO, PMD,
		"Closing ARMv8 crypto device %s on numa socket %u\n",
		name, rte_socket_id());

	return 0;
}

static struct rte_vdev_driver armv8_crypto_drv = {
	.probe = cryptodev_armv8_crypto_init,
	.remove = cryptodev_armv8_crypto_uninit
};

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_ARMV8_PMD, armv8_crypto_drv);
RTE_PMD_REGISTER_ALIAS(CRYPTODEV_NAME_ARMV8_PMD, cryptodev_armv8_pmd);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_ARMV8_PMD,
	"max_nb_queue_pairs=<int> "
	"max_nb_sessions=<int> "
	"socket_id=<int>");
