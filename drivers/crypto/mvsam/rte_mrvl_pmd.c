/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Marvell International Ltd.
 * Copyright(c) 2017 Semihalf.
 * All rights reserved.
 */

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_security_driver.h>
#include <bus_vdev_driver.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>
#include <rte_kvargs.h>
#include <rte_mvep_common.h>

#include "mrvl_pmd_private.h"

#define MRVL_PMD_MAX_NB_SESS_ARG		("max_nb_sessions")
#define MRVL_PMD_DEFAULT_MAX_NB_SESSIONS	2048

static uint8_t cryptodev_driver_id;

struct mrvl_pmd_init_params {
	struct rte_cryptodev_pmd_init_params common;
	uint32_t max_nb_sessions;
};

const char *mrvl_pmd_valid_params[] = {
	RTE_CRYPTODEV_PMD_NAME_ARG,
	RTE_CRYPTODEV_PMD_MAX_NB_QP_ARG,
	RTE_CRYPTODEV_PMD_SOCKET_ID_ARG,
	MRVL_PMD_MAX_NB_SESS_ARG
};

/**
 * Flag if particular crypto algorithm is supported by PMD/MUSDK.
 *
 * The idea is to have Not Supported value as default (0).
 * This way we need only to define proper map sizes,
 * non-initialized entries will be by default not supported.
 */
enum algo_supported {
	ALGO_NOT_SUPPORTED = 0,
	ALGO_SUPPORTED = 1,
};

/** Map elements for cipher mapping.*/
struct cipher_params_mapping {
	enum algo_supported  supported;   /**< On/Off switch */
	enum sam_cipher_alg  cipher_alg;  /**< Cipher algorithm */
	enum sam_cipher_mode cipher_mode; /**< Cipher mode */
	unsigned int max_key_len;         /**< Maximum key length (in bytes)*/
}
/* We want to squeeze in multiple maps into the cache line. */
__rte_aligned(32);

/** Map elements for auth mapping.*/
struct auth_params_mapping {
	enum algo_supported supported;  /**< On/off switch */
	enum sam_auth_alg   auth_alg;   /**< Auth algorithm */
}
/* We want to squeeze in multiple maps into the cache line. */
__rte_aligned(32);

/**
 * Map of supported cipher algorithms.
 */
static const
struct cipher_params_mapping cipher_map[] = {
	[RTE_CRYPTO_CIPHER_NULL] = {
		.supported = ALGO_SUPPORTED,
		.cipher_alg = SAM_CIPHER_NONE },
	[RTE_CRYPTO_CIPHER_3DES_CBC] = {
		.supported = ALGO_SUPPORTED,
		.cipher_alg = SAM_CIPHER_3DES,
		.cipher_mode = SAM_CIPHER_CBC,
		.max_key_len = BITS2BYTES(192) },
	[RTE_CRYPTO_CIPHER_3DES_CTR] = {
		.supported = ALGO_SUPPORTED,
		.cipher_alg = SAM_CIPHER_3DES,
		.cipher_mode = SAM_CIPHER_CTR,
		.max_key_len = BITS2BYTES(192) },
	[RTE_CRYPTO_CIPHER_3DES_ECB] = {
		.supported = ALGO_SUPPORTED,
		.cipher_alg = SAM_CIPHER_3DES,
		.cipher_mode = SAM_CIPHER_ECB,
		.max_key_len = BITS2BYTES(192) },
	[RTE_CRYPTO_CIPHER_AES_CBC] = {
		.supported = ALGO_SUPPORTED,
		.cipher_alg = SAM_CIPHER_AES,
		.cipher_mode = SAM_CIPHER_CBC,
		.max_key_len = BITS2BYTES(256) },
	[RTE_CRYPTO_CIPHER_AES_CTR] = {
		.supported = ALGO_SUPPORTED,
		.cipher_alg = SAM_CIPHER_AES,
		.cipher_mode = SAM_CIPHER_CTR,
		.max_key_len = BITS2BYTES(256) },
	[RTE_CRYPTO_CIPHER_AES_ECB] = {
		.supported = ALGO_SUPPORTED,
		.cipher_alg = SAM_CIPHER_AES,
		.cipher_mode = SAM_CIPHER_ECB,
		.max_key_len = BITS2BYTES(256) },
};

/**
 * Map of supported auth algorithms.
 */
static const
struct auth_params_mapping auth_map[] = {
	[RTE_CRYPTO_AUTH_NULL] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_NONE },
	[RTE_CRYPTO_AUTH_MD5_HMAC] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HMAC_MD5 },
	[RTE_CRYPTO_AUTH_MD5] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HASH_MD5 },
	[RTE_CRYPTO_AUTH_SHA1_HMAC] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HMAC_SHA1 },
	[RTE_CRYPTO_AUTH_SHA1] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HASH_SHA1 },
	[RTE_CRYPTO_AUTH_SHA224_HMAC] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HMAC_SHA2_224 },
	[RTE_CRYPTO_AUTH_SHA224] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HASH_SHA2_224 },
	[RTE_CRYPTO_AUTH_SHA256_HMAC] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HMAC_SHA2_256 },
	[RTE_CRYPTO_AUTH_SHA256] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HASH_SHA2_256 },
	[RTE_CRYPTO_AUTH_SHA384_HMAC] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HMAC_SHA2_384 },
	[RTE_CRYPTO_AUTH_SHA384] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HASH_SHA2_384 },
	[RTE_CRYPTO_AUTH_SHA512_HMAC] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HMAC_SHA2_512 },
	[RTE_CRYPTO_AUTH_SHA512] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_HASH_SHA2_512 },
	[RTE_CRYPTO_AUTH_AES_GMAC] = {
		.supported = ALGO_SUPPORTED,
		.auth_alg = SAM_AUTH_AES_GMAC },
};

/**
 * Map of supported aead algorithms.
 */
static const
struct cipher_params_mapping aead_map[] = {
	[RTE_CRYPTO_AEAD_AES_GCM] = {
		.supported = ALGO_SUPPORTED,
		.cipher_alg = SAM_CIPHER_AES,
		.cipher_mode = SAM_CIPHER_GCM,
		.max_key_len = BITS2BYTES(256) },
};

/*
 *-----------------------------------------------------------------------------
 * Forward declarations.
 *-----------------------------------------------------------------------------
 */
static int cryptodev_mrvl_crypto_uninit(struct rte_vdev_device *vdev);

/*
 *-----------------------------------------------------------------------------
 * Session Preparation.
 *-----------------------------------------------------------------------------
 */

/**
 * Get xform chain order.
 *
 * @param xform Pointer to configuration structure chain for crypto operations.
 * @returns Order of crypto operations.
 */
static enum mrvl_crypto_chain_order
mrvl_crypto_get_chain_order(const struct rte_crypto_sym_xform *xform)
{
	/* Currently, Marvell supports max 2 operations in chain */
	if (xform->next != NULL && xform->next->next != NULL)
		return MRVL_CRYPTO_CHAIN_NOT_SUPPORTED;

	if (xform->next != NULL) {
		if ((xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) &&
			(xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER))
			return MRVL_CRYPTO_CHAIN_AUTH_CIPHER;

		if ((xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) &&
			(xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH))
			return MRVL_CRYPTO_CHAIN_CIPHER_AUTH;
	} else {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return MRVL_CRYPTO_CHAIN_AUTH_ONLY;

		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return MRVL_CRYPTO_CHAIN_CIPHER_ONLY;

		if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD)
			return MRVL_CRYPTO_CHAIN_COMBINED;
	}
	return MRVL_CRYPTO_CHAIN_NOT_SUPPORTED;
}

/**
 * Set session parameters for cipher part.
 *
 * @param sess Crypto session pointer.
 * @param cipher_xform Pointer to configuration structure for cipher operations.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_crypto_set_cipher_session_parameters(struct mrvl_crypto_session *sess,
		const struct rte_crypto_sym_xform *cipher_xform)
{
	uint8_t *cipher_key;

	/* Make sure we've got proper struct */
	if (cipher_xform->type != RTE_CRYPTO_SYM_XFORM_CIPHER) {
		MRVL_LOG(ERR, "Wrong xform struct provided!");
		return -EINVAL;
	}

	/* See if map data is present and valid */
	if ((cipher_xform->cipher.algo > RTE_DIM(cipher_map)) ||
		(cipher_map[cipher_xform->cipher.algo].supported
			!= ALGO_SUPPORTED)) {
		MRVL_LOG(ERR, "Cipher algorithm not supported!");
		return -EINVAL;
	}

	sess->cipher_iv_offset = cipher_xform->cipher.iv.offset;

	sess->sam_sess_params.dir =
		(cipher_xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT) ?
		SAM_DIR_ENCRYPT : SAM_DIR_DECRYPT;
	sess->sam_sess_params.cipher_alg =
		cipher_map[cipher_xform->cipher.algo].cipher_alg;
	sess->sam_sess_params.cipher_mode =
		cipher_map[cipher_xform->cipher.algo].cipher_mode;

	/* Assume IV will be passed together with data. */
	sess->sam_sess_params.cipher_iv = NULL;

	/* Get max key length. */
	if (cipher_xform->cipher.key.length >
		cipher_map[cipher_xform->cipher.algo].max_key_len) {
		MRVL_LOG(ERR, "Wrong key length!");
		return -EINVAL;
	}

	cipher_key = malloc(cipher_xform->cipher.key.length);
	if (cipher_key == NULL) {
		MRVL_LOG(ERR, "Insufficient memory!");
		return -ENOMEM;
	}

	memcpy(cipher_key, cipher_xform->cipher.key.data,
			cipher_xform->cipher.key.length);

	sess->sam_sess_params.cipher_key_len = cipher_xform->cipher.key.length;
	sess->sam_sess_params.cipher_key = cipher_key;

	return 0;
}

/**
 * Set session parameters for authentication part.
 *
 * @param sess Crypto session pointer.
 * @param auth_xform Pointer to configuration structure for auth operations.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_crypto_set_auth_session_parameters(struct mrvl_crypto_session *sess,
		const struct rte_crypto_sym_xform *auth_xform)
{
	uint8_t *auth_key = NULL;

	/* Make sure we've got proper struct */
	if (auth_xform->type != RTE_CRYPTO_SYM_XFORM_AUTH) {
		MRVL_LOG(ERR, "Wrong xform struct provided!");
		return -EINVAL;
	}

	/* See if map data is present and valid */
	if ((auth_xform->auth.algo > RTE_DIM(auth_map)) ||
		(auth_map[auth_xform->auth.algo].supported != ALGO_SUPPORTED)) {
		MRVL_LOG(ERR, "Auth algorithm not supported!");
		return -EINVAL;
	}

	sess->sam_sess_params.dir =
		(auth_xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE) ?
		SAM_DIR_ENCRYPT : SAM_DIR_DECRYPT;
	sess->sam_sess_params.auth_alg =
		auth_map[auth_xform->auth.algo].auth_alg;
	sess->sam_sess_params.u.basic.auth_icv_len =
		auth_xform->auth.digest_length;

	if (auth_xform->auth.key.length > 0) {
		auth_key = malloc(auth_xform->auth.key.length);
		if (auth_key == NULL) {
			MRVL_LOG(ERR, "Not enough memory!");
			return -EINVAL;
		}

		memcpy(auth_key, auth_xform->auth.key.data,
				auth_xform->auth.key.length);
	}

	/* auth_key must be NULL if auth algorithm does not use HMAC */
	sess->sam_sess_params.auth_key = auth_key;
	sess->sam_sess_params.auth_key_len = auth_xform->auth.key.length;

	return 0;
}

/**
 * Set session parameters for aead part.
 *
 * @param sess Crypto session pointer.
 * @param aead_xform Pointer to configuration structure for aead operations.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
mrvl_crypto_set_aead_session_parameters(struct mrvl_crypto_session *sess,
		const struct rte_crypto_sym_xform *aead_xform)
{
	uint8_t *aead_key;

	/* Make sure we've got proper struct */
	if (aead_xform->type != RTE_CRYPTO_SYM_XFORM_AEAD) {
		MRVL_LOG(ERR, "Wrong xform struct provided!");
		return -EINVAL;
	}

	/* See if map data is present and valid */
	if ((aead_xform->aead.algo > RTE_DIM(aead_map)) ||
		(aead_map[aead_xform->aead.algo].supported
			!= ALGO_SUPPORTED)) {
		MRVL_LOG(ERR, "AEAD algorithm not supported!");
		return -EINVAL;
	}

	sess->sam_sess_params.dir =
		(aead_xform->aead.op == RTE_CRYPTO_AEAD_OP_ENCRYPT) ?
		SAM_DIR_ENCRYPT : SAM_DIR_DECRYPT;
	sess->sam_sess_params.cipher_alg =
		aead_map[aead_xform->aead.algo].cipher_alg;
	sess->sam_sess_params.cipher_mode =
		aead_map[aead_xform->aead.algo].cipher_mode;

	if (sess->sam_sess_params.cipher_mode == SAM_CIPHER_GCM) {
		/* IV must include nonce for all counter modes */
		sess->cipher_iv_offset = aead_xform->cipher.iv.offset;

		/* Set order of authentication then encryption to 0 in GCM */
		sess->sam_sess_params.u.basic.auth_then_encrypt = 0;
	}

	/* Assume IV will be passed together with data. */
	sess->sam_sess_params.cipher_iv = NULL;

	/* Get max key length. */
	if (aead_xform->aead.key.length >
		aead_map[aead_xform->aead.algo].max_key_len) {
		MRVL_LOG(ERR, "Wrong key length!");
		return -EINVAL;
	}

	aead_key = malloc(aead_xform->aead.key.length);
	if (aead_key == NULL) {
		MRVL_LOG(ERR, "Insufficient memory!");
		return -ENOMEM;
	}

	memcpy(aead_key, aead_xform->aead.key.data,
			aead_xform->aead.key.length);

	sess->sam_sess_params.cipher_key = aead_key;
	sess->sam_sess_params.cipher_key_len = aead_xform->aead.key.length;

	if (sess->sam_sess_params.cipher_mode == SAM_CIPHER_GCM)
		sess->sam_sess_params.auth_alg = SAM_AUTH_AES_GCM;

	sess->sam_sess_params.u.basic.auth_icv_len =
		aead_xform->aead.digest_length;

	sess->sam_sess_params.u.basic.auth_aad_len =
		aead_xform->aead.aad_length;

	return 0;
}

/**
 * Parse crypto transform chain and setup session parameters.
 *
 * @param dev Pointer to crypto device
 * @param sess Pointer to crypto session
 * @param xform Pointer to configuration structure chain for crypto operations.
 * @returns 0 in case of success, negative value otherwise.
 */
int
mrvl_crypto_set_session_parameters(struct mrvl_crypto_session *sess,
		const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *aead_xform = NULL;

	/* Filter out spurious/broken requests */
	if (xform == NULL)
		return -EINVAL;

	sess->chain_order = mrvl_crypto_get_chain_order(xform);
	switch (sess->chain_order) {
	case MRVL_CRYPTO_CHAIN_CIPHER_AUTH:
		cipher_xform = xform;
		auth_xform = xform->next;
		break;
	case MRVL_CRYPTO_CHAIN_AUTH_CIPHER:
		auth_xform = xform;
		cipher_xform = xform->next;
		break;
	case MRVL_CRYPTO_CHAIN_CIPHER_ONLY:
		cipher_xform = xform;
		break;
	case MRVL_CRYPTO_CHAIN_AUTH_ONLY:
		auth_xform = xform;
		break;
	case MRVL_CRYPTO_CHAIN_COMBINED:
		aead_xform = xform;
		break;
	default:
		return -EINVAL;
	}

	if ((cipher_xform != NULL) &&
		(mrvl_crypto_set_cipher_session_parameters(
			sess, cipher_xform) < 0)) {
		MRVL_LOG(ERR, "Invalid/unsupported cipher parameters!");
		return -EINVAL;
	}

	if ((auth_xform != NULL) &&
		(mrvl_crypto_set_auth_session_parameters(
			sess, auth_xform) < 0)) {
		MRVL_LOG(ERR, "Invalid/unsupported auth parameters!");
		return -EINVAL;
	}

	if ((aead_xform != NULL) &&
		(mrvl_crypto_set_aead_session_parameters(
			sess, aead_xform) < 0)) {
		MRVL_LOG(ERR, "Invalid/unsupported aead parameters!");
		return -EINVAL;
	}

	return 0;
}

static int
replay_wsz_to_mask(uint32_t replay_win_sz)
{
	int mask = 0;

	switch (replay_win_sz) {
	case 0:
		mask = SAM_ANTI_REPLY_MASK_NONE;
		break;
	case 32:
		mask = SAM_ANTI_REPLY_MASK_32B;
		break;
	case 64:
		mask = SAM_ANTI_REPLY_MASK_64B;
		break;
	case 128:
		mask = SAM_ANTI_REPLY_MASK_128B;
		break;
	default:
		MRVL_LOG(ERR, "Invalid antireplay window size");
		return -EINVAL;
	}

	return mask;
}

/**
 * Parse IPSEC session parameters.
 *
 * @param sess Pointer to security session
 * @param ipsec_xform Pointer to configuration structure IPSEC operations.
 * @param crypto_xform Pointer to chain for crypto operations.
 * @returns 0 in case of success, negative value otherwise.
 */
int
mrvl_ipsec_set_session_parameters(struct mrvl_crypto_session *sess,
		struct rte_security_ipsec_xform *ipsec_xform,
		struct rte_crypto_sym_xform *crypto_xform)
{
	int seq_mask_size;

	/* Filter out spurious/broken requests */
	if (ipsec_xform == NULL || crypto_xform == NULL)
		return -EINVAL;

	/* Crypto parameters handling */
	if (mrvl_crypto_set_session_parameters(sess, crypto_xform))
		return -EINVAL;

	seq_mask_size = replay_wsz_to_mask(ipsec_xform->replay_win_sz);
	if (seq_mask_size < 0)
		return -EINVAL;

	/* IPSEC protocol parameters handling */
	sess->sam_sess_params.proto = SAM_PROTO_IPSEC;
	sess->sam_sess_params.u.ipsec.is_esp =
		(ipsec_xform->proto == RTE_SECURITY_IPSEC_SA_PROTO_ESP) ?
		1 : 0;
	sess->sam_sess_params.u.ipsec.is_ip6 = 0;
	sess->sam_sess_params.u.ipsec.is_tunnel =
		(ipsec_xform->mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) ?
		1 : 0;
	sess->sam_sess_params.u.ipsec.is_esn = ipsec_xform->options.esn;
	sess->sam_sess_params.u.ipsec.seq_mask_size = seq_mask_size;

	sess->sam_sess_params.u.ipsec.tunnel.u.ipv4.sip =
		(uint8_t *)(&ipsec_xform->tunnel.ipv4.src_ip.s_addr);
	sess->sam_sess_params.u.ipsec.tunnel.u.ipv4.dip =
		(uint8_t *)&(ipsec_xform->tunnel.ipv4.dst_ip.s_addr);

	sess->sam_sess_params.u.ipsec.tunnel.u.ipv4.dscp =
		ipsec_xform->tunnel.ipv4.dscp;
	sess->sam_sess_params.u.ipsec.tunnel.u.ipv4.ttl =
		ipsec_xform->tunnel.ipv4.ttl;
	sess->sam_sess_params.u.ipsec.tunnel.u.ipv4.df =
		ipsec_xform->tunnel.ipv4.df;
	sess->sam_sess_params.u.ipsec.tunnel.copy_dscp =
		ipsec_xform->options.copy_dscp;
	sess->sam_sess_params.u.ipsec.tunnel.copy_flabel =
		ipsec_xform->options.copy_flabel;
	sess->sam_sess_params.u.ipsec.tunnel.copy_df =
		ipsec_xform->options.copy_df;

	sess->sam_sess_params.u.ipsec.is_natt = 0;
	sess->sam_sess_params.u.ipsec.spi = ipsec_xform->spi;
	sess->sam_sess_params.u.ipsec.seq = 0;

	return 0;
}

/*
 *-----------------------------------------------------------------------------
 * Process Operations
 *-----------------------------------------------------------------------------
 */

/**
 * Prepare a single request.
 *
 * This function basically translates DPDK crypto request into one
 * understandable by MUDSK's SAM. If this is a first request in a session,
 * it starts the session.
 *
 * @param request Pointer to pre-allocated && reset request buffer [Out].
 * @param src_bd Pointer to pre-allocated source descriptor [Out].
 * @param dst_bd Pointer to pre-allocated destination descriptor [Out].
 * @param op Pointer to DPDK crypto operation struct [In].
 */
static inline int
mrvl_request_prepare_crp(struct sam_cio_op_params *request,
		struct sam_buf_info *src_bd,
		struct sam_buf_info *dst_bd,
		struct rte_crypto_op *op)
{
	struct mrvl_crypto_session *sess;
	struct rte_mbuf *src_mbuf, *dst_mbuf;
	uint16_t segments_nb;
	uint8_t *digest;
	int i;

	if (unlikely(op->sess_type == RTE_CRYPTO_OP_SESSIONLESS)) {
		MRVL_LOG(ERR, "MRVL CRYPTO PMD only supports session "
				"oriented requests, op (%p) is sessionless!",
				op);
		return -EINVAL;
	}

	sess = CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session);

	request->sa = sess->sam_sess;
	request->cookie = op;

	src_mbuf = op->sym->m_src;
	segments_nb = src_mbuf->nb_segs;
	/* The following conditions must be met:
	 * - Destination buffer is required when segmented source buffer
	 * - Segmented destination buffer is not supported
	 */
	if ((segments_nb > 1) && (!op->sym->m_dst)) {
		MRVL_LOG(ERR, "op->sym->m_dst = NULL!");
		return -1;
	}
	/* For non SG case:
	 * If application delivered us null dst buffer, it means it expects
	 * us to deliver the result in src buffer.
	 */
	dst_mbuf = op->sym->m_dst ? op->sym->m_dst : op->sym->m_src;

	if (!rte_pktmbuf_is_contiguous(dst_mbuf)) {
		MRVL_LOG(ERR, "Segmented destination buffer not supported!");
		return -1;
	}

	request->num_bufs = segments_nb;
	for (i = 0; i < segments_nb; i++) {
		/* Empty source. */
		if (rte_pktmbuf_data_len(src_mbuf) == 0) {
			/* EIP does not support 0 length buffers. */
			MRVL_LOG(ERR, "Buffer length == 0 not supported!");
			return -1;
		}
		src_bd[i].vaddr = rte_pktmbuf_mtod(src_mbuf, void *);
		src_bd[i].paddr = rte_pktmbuf_iova(src_mbuf);
		src_bd[i].len = rte_pktmbuf_data_len(src_mbuf);

		src_mbuf = src_mbuf->next;
	}
	request->src = src_bd;

	/* Empty destination. */
	if (rte_pktmbuf_data_len(dst_mbuf) == 0) {
		/* Make dst buffer fit at least source data. */
		if (rte_pktmbuf_append(dst_mbuf,
			rte_pktmbuf_data_len(op->sym->m_src)) == NULL) {
			MRVL_LOG(ERR, "Unable to set big enough dst buffer!");
			return -1;
		}
	}

	request->dst = dst_bd;
	dst_bd->vaddr = rte_pktmbuf_mtod(dst_mbuf, void *);
	dst_bd->paddr = rte_pktmbuf_iova(dst_mbuf);

	/*
	 * We can use all available space in dst_mbuf,
	 * not only what's used currently.
	 */
	dst_bd->len = dst_mbuf->buf_len - rte_pktmbuf_headroom(dst_mbuf);

	if (sess->chain_order == MRVL_CRYPTO_CHAIN_COMBINED) {
		request->cipher_len = op->sym->aead.data.length;
		request->cipher_offset = op->sym->aead.data.offset;
		request->cipher_iv = rte_crypto_op_ctod_offset(op, uint8_t *,
						  sess->cipher_iv_offset);

		request->auth_aad = op->sym->aead.aad.data;
		request->auth_offset = request->cipher_offset;
		request->auth_len = request->cipher_len;
	} else {
		request->cipher_len = op->sym->cipher.data.length;
		request->cipher_offset = op->sym->cipher.data.offset;
		request->cipher_iv = rte_crypto_op_ctod_offset(op, uint8_t *,
				sess->cipher_iv_offset);

		request->auth_offset = op->sym->auth.data.offset;
		request->auth_len = op->sym->auth.data.length;
	}

	digest = sess->chain_order == MRVL_CRYPTO_CHAIN_COMBINED ?
		op->sym->aead.digest.data : op->sym->auth.digest.data;
	if (digest == NULL) {
		/* No auth - no worry. */
		return 0;
	}

	request->auth_icv_offset = request->auth_offset + request->auth_len;

	/*
	 * EIP supports only scenarios where ICV(digest buffer) is placed at
	 * auth_icv_offset.
	 */
	if (sess->sam_sess_params.dir == SAM_DIR_ENCRYPT) {
		/*
		 * This should be the most common case anyway,
		 * EIP will overwrite DST buffer at auth_icv_offset.
		 */
		if (rte_pktmbuf_mtod_offset(
				dst_mbuf, uint8_t *,
				request->auth_icv_offset) == digest)
			return 0;
	} else {/* sess->sam_sess_params.dir == SAM_DIR_DECRYPT */
		/*
		 * EIP will look for digest at auth_icv_offset
		 * offset in SRC buffer. It must be placed in the last
		 * segment and the offset must be set to reach digest
		 * in the last segment
		 */
		struct rte_mbuf *last_seg =  op->sym->m_src;
		uint32_t d_offset = request->auth_icv_offset;
		u32 d_size = sess->sam_sess_params.u.basic.auth_icv_len;
		unsigned char *d_ptr;

		/* Find the last segment and the offset for the last segment */
		while ((last_seg->next != NULL) &&
				(d_offset >= last_seg->data_len)) {
			d_offset -= last_seg->data_len;
			last_seg = last_seg->next;
		}

		if (rte_pktmbuf_mtod_offset(last_seg, uint8_t *,
					    d_offset) == digest)
			return 0;

		/* copy digest to last segment */
		if (last_seg->buf_len >= (d_size + d_offset)) {
			d_ptr = (unsigned char *)last_seg->buf_addr +
				 d_offset;
			rte_memcpy(d_ptr, digest, d_size);
			return 0;
		}
	}

	/*
	 * If we landed here it means that digest pointer is
	 * at different than expected place.
	 */
	return -1;
}

/**
 * Prepare a single security protocol request.
 *
 * This function basically translates DPDK security request into one
 * understandable by MUDSK's SAM. If this is a first request in a session,
 * it starts the session.
 *
 * @param request Pointer to pre-allocated && reset request buffer [Out].
 * @param src_bd Pointer to pre-allocated source descriptor [Out].
 * @param dst_bd Pointer to pre-allocated destination descriptor [Out].
 * @param op Pointer to DPDK crypto operation struct [In].
 */
static inline int
mrvl_request_prepare_sec(struct sam_cio_ipsec_params *request,
		struct sam_buf_info *src_bd,
		struct sam_buf_info *dst_bd,
		struct rte_crypto_op *op)
{
	struct mrvl_crypto_session *sess;
	struct rte_mbuf *src_mbuf, *dst_mbuf;
	uint16_t segments_nb;
	int i;

	if (unlikely(op->sess_type != RTE_CRYPTO_OP_SECURITY_SESSION)) {
		MRVL_LOG(ERR, "MRVL SECURITY: sess_type is not SECURITY_SESSION");
		return -EINVAL;
	}

	sess = SECURITY_GET_SESS_PRIV(op->sym->session);
	if (unlikely(sess == NULL)) {
		MRVL_LOG(ERR, "Session was not created for this device! %d",
			 cryptodev_driver_id);
		return -EINVAL;
	}

	request->sa = sess->sam_sess;
	request->cookie = op;
	src_mbuf = op->sym->m_src;
	segments_nb = src_mbuf->nb_segs;
	/* The following conditions must be met:
	 * - Destination buffer is required when segmented source buffer
	 * - Segmented destination buffer is not supported
	 */
	if ((segments_nb > 1) && (!op->sym->m_dst)) {
		MRVL_LOG(ERR, "op->sym->m_dst = NULL!");
		return -1;
	}
	/* For non SG case:
	 * If application delivered us null dst buffer, it means it expects
	 * us to deliver the result in src buffer.
	 */
	dst_mbuf = op->sym->m_dst ? op->sym->m_dst : op->sym->m_src;

	if (!rte_pktmbuf_is_contiguous(dst_mbuf)) {
		MRVL_LOG(ERR, "Segmented destination buffer not supported!");
		return -1;
	}

	request->num_bufs = segments_nb;
	for (i = 0; i < segments_nb; i++) {
		/* Empty source. */
		if (rte_pktmbuf_data_len(src_mbuf) == 0) {
			/* EIP does not support 0 length buffers. */
			MRVL_LOG(ERR, "Buffer length == 0 not supported!");
			return -1;
		}
		src_bd[i].vaddr = rte_pktmbuf_mtod(src_mbuf, void *);
		src_bd[i].paddr = rte_pktmbuf_iova(src_mbuf);
		src_bd[i].len = rte_pktmbuf_data_len(src_mbuf);

		src_mbuf = src_mbuf->next;
	}
	request->src = src_bd;

	/* Empty destination. */
	if (rte_pktmbuf_data_len(dst_mbuf) == 0) {
		/* Make dst buffer fit at least source data. */
		if (rte_pktmbuf_append(dst_mbuf,
			rte_pktmbuf_data_len(op->sym->m_src)) == NULL) {
			MRVL_LOG(ERR, "Unable to set big enough dst buffer!");
			return -1;
		}
	}

	request->dst = dst_bd;
	dst_bd->vaddr = rte_pktmbuf_mtod(dst_mbuf, void *);
	dst_bd->paddr = rte_pktmbuf_iova(dst_mbuf);

	/*
	 * We can use all available space in dst_mbuf,
	 * not only what's used currently.
	 */
	dst_bd->len = dst_mbuf->buf_len - rte_pktmbuf_headroom(dst_mbuf);


	request->l3_offset = 0;
	request->pkt_size = rte_pktmbuf_pkt_len(op->sym->m_src);

	return 0;
}

/*
 *-----------------------------------------------------------------------------
 * PMD Framework handlers
 *-----------------------------------------------------------------------------
 */

/**
 * Enqueue burst.
 *
 * @param queue_pair Pointer to queue pair.
 * @param ops Pointer to ops requests array.
 * @param nb_ops Number of elements in ops requests array.
 * @returns Number of elements consumed from ops.
 */
static uint16_t
mrvl_crypto_pmd_enqueue_burst(void *queue_pair, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	uint16_t iter_ops = 0;
	uint16_t to_enq_crp = 0;
	uint16_t to_enq_sec = 0;
	uint16_t consumed = 0;
	int ret;
	int iter;
	struct sam_cio_op_params requests_crp[nb_ops];
	struct sam_cio_ipsec_params requests_sec[nb_ops];
	uint16_t indx_map_crp[nb_ops];
	uint16_t indx_map_sec[nb_ops];

	/*
	 * SAM does not store bd pointers, so on-stack scope will be enough.
	 */
	struct mrvl_crypto_src_table src_bd[nb_ops];
	struct sam_buf_info          dst_bd[nb_ops];
	struct mrvl_crypto_qp *qp = (struct mrvl_crypto_qp *)queue_pair;

	if (nb_ops == 0)
		return 0;

	/* Prepare the burst. */
	memset(&requests_crp, 0, sizeof(requests_crp));
	memset(&requests_sec, 0, sizeof(requests_sec));
	memset(&src_bd, 0, sizeof(src_bd));

	/* Iterate through */
	for (; iter_ops < nb_ops; ++iter_ops) {
		/* store the op id for debug */
		if (ops[iter_ops]->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
			src_bd[iter_ops].iter_ops = to_enq_crp;
			indx_map_crp[to_enq_crp] = iter_ops;

			if (mrvl_request_prepare_crp(&requests_crp[to_enq_crp],
						src_bd[iter_ops].src_bd,
						&dst_bd[iter_ops],
						ops[iter_ops]) < 0) {
				MRVL_LOG(ERR,
					"Error while preparing parameters!");
				qp->stats.enqueue_err_count++;
				ops[iter_ops]->status =
					RTE_CRYPTO_OP_STATUS_ERROR;
				/*
				 * Number of handled ops is increased
				 * (even if the result of handling is error).
				 */
				++consumed;

				break;
			}
			/* Increase the number of ops to enqueue. */
			++to_enq_crp;
		} else {
			src_bd[iter_ops].iter_ops = to_enq_sec;
			indx_map_sec[to_enq_sec] = iter_ops;
			if (mrvl_request_prepare_sec(&requests_sec[to_enq_sec],
						src_bd[iter_ops].src_bd,
						&dst_bd[iter_ops],
						ops[iter_ops]) < 0) {
				MRVL_LOG(ERR,
					"Error while preparing parameters!");
				qp->stats.enqueue_err_count++;
				ops[iter_ops]->status =
					RTE_CRYPTO_OP_STATUS_ERROR;
				/*
				 * Number of handled ops is increased
				 * (even if the result of handling is error).
				 */
				++consumed;

				break;
			}
			/* Increase the number of ops to enqueue. */
			++to_enq_sec;
		}

		ops[iter_ops]->status =
			RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;

	} /* for (; iter_ops < nb_ops;... */

	if (to_enq_crp > 0) {
		/* Send the burst */
		ret = sam_cio_enq(qp->cio, requests_crp, &to_enq_crp);
		consumed += to_enq_crp;
		if (ret < 0) {
			/*
			 * Trust SAM that in this case returned value will be at
			 * some point correct (now it is returned unmodified).
			 */
			qp->stats.enqueue_err_count += to_enq_crp;
			for (iter = 0; iter < to_enq_crp; ++iter)
				ops[indx_map_crp[iter]]->status =
					RTE_CRYPTO_OP_STATUS_ERROR;
		}
	}

	if (to_enq_sec > 0) {
		/* Send the burst */
		ret = sam_cio_enq_ipsec(qp->cio, requests_sec, &to_enq_sec);
		consumed += to_enq_sec;
		if (ret < 0) {
			/*
			 * Trust SAM that in this case returned value will be at
			 * some point correct (now it is returned unmodified).
			 */
			qp->stats.enqueue_err_count += to_enq_sec;
			for (iter = 0; iter < to_enq_crp; ++iter)
				ops[indx_map_sec[iter]]->status =
					RTE_CRYPTO_OP_STATUS_ERROR;
		}
	}

	qp->stats.enqueued_count += to_enq_sec + to_enq_crp;
	return consumed;
}

/**
 * Dequeue burst.
 *
 * @param queue_pair Pointer to queue pair.
 * @param ops Pointer to ops requests array.
 * @param nb_ops Number of elements in ops requests array.
 * @returns Number of elements dequeued.
 */
static uint16_t
mrvl_crypto_pmd_dequeue_burst(void *queue_pair,
		struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	int ret;
	struct mrvl_crypto_qp *qp = queue_pair;
	struct sam_cio *cio = qp->cio;
	struct sam_cio_op_result results[nb_ops];
	uint16_t i;
	struct rte_mbuf *dst;

	ret = sam_cio_deq(cio, results, &nb_ops);
	if (ret < 0) {
		/* Count all dequeued as error. */
		qp->stats.dequeue_err_count += nb_ops;

		/* But act as they were dequeued anyway*/
		qp->stats.dequeued_count += nb_ops;

		return 0;
	}

	/* Unpack and check results. */
	for (i = 0; i < nb_ops; ++i) {
		ops[i] = results[i].cookie;

		switch (results[i].status) {
		case SAM_CIO_OK:
			ops[i]->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
			if (ops[i]->sess_type ==
				RTE_CRYPTO_OP_SECURITY_SESSION) {

				if (ops[i]->sym->m_dst)
					dst = ops[i]->sym->m_dst;
				else
					dst = ops[i]->sym->m_src;
				dst->pkt_len = results[i].out_len;
				dst->data_len = results[i].out_len;
			}
			break;
		case SAM_CIO_ERR_ICV:
			MRVL_LOG(DEBUG, "CIO returned SAM_CIO_ERR_ICV.");
			ops[i]->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
			break;
		default:
			MRVL_LOG(DEBUG,
				"CIO returned Error: %d.", results[i].status);
			ops[i]->status = RTE_CRYPTO_OP_STATUS_ERROR;
			break;
		}
	}

	qp->stats.dequeued_count += nb_ops;
	return nb_ops;
}

/**
 * Create a new crypto device.
 *
 * @param name Driver name.
 * @param vdev Pointer to device structure.
 * @param init_params Pointer to initialization parameters.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
cryptodev_mrvl_crypto_create(const char *name,
		struct rte_vdev_device *vdev,
		struct mrvl_pmd_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct mrvl_crypto_private *internals;
	struct sam_init_params	sam_params;
	struct rte_security_ctx *security_instance;
	int ret = -EINVAL;

	dev = rte_cryptodev_pmd_create(name, &vdev->device,
			&init_params->common);
	if (dev == NULL) {
		MRVL_LOG(ERR, "Failed to create cryptodev vdev!");
		goto init_error;
	}

	dev->driver_id = cryptodev_driver_id;
	dev->dev_ops = rte_mrvl_crypto_pmd_ops;

	/* Register rx/tx burst functions for data path. */
	dev->enqueue_burst = mrvl_crypto_pmd_enqueue_burst;
	dev->dequeue_burst = mrvl_crypto_pmd_dequeue_burst;

	dev->feature_flags = RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO |
			RTE_CRYPTODEV_FF_SYM_OPERATION_CHAINING |
			RTE_CRYPTODEV_FF_HW_ACCELERATED |
			RTE_CRYPTODEV_FF_OOP_SGL_IN_LB_OUT |
			RTE_CRYPTODEV_FF_OOP_LB_IN_LB_OUT |
			RTE_CRYPTODEV_FF_SECURITY;

	internals = dev->data->dev_private;

	internals->max_nb_qpairs = init_params->common.max_nb_queue_pairs;
	internals->max_nb_sessions = init_params->max_nb_sessions;

	ret = rte_mvep_init(MVEP_MOD_T_SAM, NULL);
	if (ret)
		goto init_error;

	sam_params.max_num_sessions = internals->max_nb_sessions;

	/* Initialize security_ctx only for primary process*/
	security_instance = rte_malloc("rte_security_instances_ops",
		sizeof(struct rte_security_ctx), 0);
	if (security_instance == NULL)
		return -ENOMEM;
	security_instance->device = (void *)dev;
	security_instance->ops = rte_mrvl_security_pmd_ops;
	security_instance->sess_cnt = 0;
	dev->security_ctx = security_instance;

	/*sam_set_debug_flags(3);*/

	ret = sam_init(&sam_params);
	if (ret)
		goto init_error;

	rte_cryptodev_pmd_probing_finish(dev);

	return 0;

init_error:
	MRVL_LOG(ERR,
		"Driver %s: %s failed!", init_params->common.name, __func__);

	cryptodev_mrvl_crypto_uninit(vdev);
	return ret;
}

/** Parse integer from integer argument */
static int
parse_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int *i = (int *) extra_args;

	*i = atoi(value);
	if (*i < 0) {
		MRVL_LOG(ERR, "Argument has to be positive!");
		return -EINVAL;
	}

	return 0;
}

/** Parse name */
static int
parse_name_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct rte_cryptodev_pmd_init_params *params = extra_args;

	if (strlen(value) >= RTE_CRYPTODEV_NAME_MAX_LEN - 1) {
		MRVL_LOG(ERR, "Invalid name %s, should be less than %u bytes!",
			 value, RTE_CRYPTODEV_NAME_MAX_LEN - 1);
		return -EINVAL;
	}

	strncpy(params->name, value, RTE_CRYPTODEV_NAME_MAX_LEN);

	return 0;
}

static int
mrvl_pmd_parse_input_args(struct mrvl_pmd_init_params *params,
			 const char *input_args)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	if (params == NULL)
		return -EINVAL;

	if (input_args) {
		kvlist = rte_kvargs_parse(input_args,
					  mrvl_pmd_valid_params);
		if (kvlist == NULL)
			return -1;

		/* Common VDEV parameters */
		ret = rte_kvargs_process(kvlist,
					 RTE_CRYPTODEV_PMD_MAX_NB_QP_ARG,
					 &parse_integer_arg,
					 &params->common.max_nb_queue_pairs);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist,
					 RTE_CRYPTODEV_PMD_SOCKET_ID_ARG,
					 &parse_integer_arg,
					 &params->common.socket_id);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist,
					 RTE_CRYPTODEV_PMD_NAME_ARG,
					 &parse_name_arg,
					 &params->common.name);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist,
					 MRVL_PMD_MAX_NB_SESS_ARG,
					 &parse_integer_arg,
					 &params->max_nb_sessions);
		if (ret < 0)
			goto free_kvlist;

	}

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

/**
 * Initialize the crypto device.
 *
 * @param vdev Pointer to device structure.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
cryptodev_mrvl_crypto_init(struct rte_vdev_device *vdev)
{
	struct mrvl_pmd_init_params init_params = {
		.common = {
			.name = "",
			.private_data_size =
				sizeof(struct mrvl_crypto_private),
			.max_nb_queue_pairs =
				sam_get_num_inst() * sam_get_num_cios(0),
			.socket_id = rte_socket_id()
		},
		.max_nb_sessions = MRVL_PMD_DEFAULT_MAX_NB_SESSIONS
	};

	const char *name, *args;
	int ret;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;
	args = rte_vdev_device_args(vdev);

	ret = mrvl_pmd_parse_input_args(&init_params, args);
	if (ret) {
		MRVL_LOG(ERR, "Failed to parse initialisation arguments[%s]!",
			 args);
		return -EINVAL;
	}

	return cryptodev_mrvl_crypto_create(name, vdev, &init_params);
}

/**
 * Uninitialize the crypto device
 *
 * @param vdev Pointer to device structure.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
cryptodev_mrvl_crypto_uninit(struct rte_vdev_device *vdev)
{
	struct rte_cryptodev *cryptodev;
	const char *name = rte_vdev_device_name(vdev);

	if (name == NULL)
		return -EINVAL;

	MRVL_LOG(INFO, "Closing Marvell crypto device %s on numa socket %u.",
		 name, rte_socket_id());

	sam_deinit();
	rte_mvep_deinit(MVEP_MOD_T_SAM);

	cryptodev = rte_cryptodev_pmd_get_named_dev(name);
	if (cryptodev == NULL)
		return -ENODEV;

	return rte_cryptodev_pmd_destroy(cryptodev);
}

/**
 * Basic driver handlers for use in the constructor.
 */
static struct rte_vdev_driver cryptodev_mrvl_pmd_drv = {
	.probe = cryptodev_mrvl_crypto_init,
	.remove = cryptodev_mrvl_crypto_uninit
};

static struct cryptodev_driver mrvl_crypto_drv;

/* Register the driver in constructor. */
RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_MRVL_PMD, cryptodev_mrvl_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_MRVL_PMD,
	"max_nb_queue_pairs=<int> "
	"max_nb_sessions=<int> "
	"socket_id=<int>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(mrvl_crypto_drv, cryptodev_mrvl_pmd_drv.driver,
		cryptodev_driver_id);
RTE_LOG_REGISTER_DEFAULT(mrvl_logtype_driver, NOTICE);
