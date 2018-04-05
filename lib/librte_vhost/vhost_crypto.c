/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_mbuf.h>
#include <rte_cryptodev.h>

#include "vhost.h"
#include "vhost_user.h"
#include "virtio_crypto.h"

#define IV_OFFSET		(sizeof(struct rte_crypto_op) + \
				sizeof(struct rte_crypto_sym_op))

#ifdef RTE_LIBRTE_VHOST_DEBUG
#define VC_LOG_ERR(fmt, args...)				\
	RTE_LOG(ERR, USER1, "[%s] %s() line %u: " fmt "\n",	\
		"Vhost-Crypto",	__func__, __LINE__, ## args)
#define VC_LOG_INFO(fmt, args...)				\
	RTE_LOG(INFO, USER1, "[%s] %s() line %u: " fmt "\n",	\
		"Vhost-Crypto",	__func__, __LINE__, ## args)

#define VC_LOG_DBG(fmt, args...)				\
	RTE_LOG(DEBUG, USER1, "[%s] %s() line %u: " fmt "\n",	\
		"Vhost-Crypto",	__func__, __LINE__, ## args)
#else
#define VC_LOG_ERR(fmt, args...)				\
	RTE_LOG(ERR, USER1, "[VHOST-Crypto]: " fmt "\n", ## args)
#define VC_LOG_INFO(fmt, args...)				\
	RTE_LOG(INFO, USER1, "[VHOST-Crypto]: " fmt "\n", ## args)
#define VC_LOG_DBG(fmt, args...)
#endif

static int
cipher_algo_transform(uint32_t virtio_cipher_algo)
{
	int ret;

	switch (virtio_cipher_algo) {
	case VIRTIO_CRYPTO_CIPHER_AES_CBC:
		ret = RTE_CRYPTO_CIPHER_AES_CBC;
		break;
	case VIRTIO_CRYPTO_CIPHER_AES_CTR:
		ret = RTE_CRYPTO_CIPHER_AES_CTR;
		break;
	case VIRTIO_CRYPTO_CIPHER_DES_ECB:
		ret = -VIRTIO_CRYPTO_NOTSUPP;
		break;
	case VIRTIO_CRYPTO_CIPHER_DES_CBC:
		ret = RTE_CRYPTO_CIPHER_DES_CBC;
		break;
	case VIRTIO_CRYPTO_CIPHER_3DES_ECB:
		ret = RTE_CRYPTO_CIPHER_3DES_ECB;
		break;
	case VIRTIO_CRYPTO_CIPHER_3DES_CBC:
		ret = RTE_CRYPTO_CIPHER_3DES_CBC;
		break;
	case VIRTIO_CRYPTO_CIPHER_3DES_CTR:
		ret = RTE_CRYPTO_CIPHER_3DES_CTR;
		break;
	case VIRTIO_CRYPTO_CIPHER_KASUMI_F8:
		ret = RTE_CRYPTO_CIPHER_KASUMI_F8;
		break;
	case VIRTIO_CRYPTO_CIPHER_SNOW3G_UEA2:
		ret = RTE_CRYPTO_CIPHER_SNOW3G_UEA2;
		break;
	case VIRTIO_CRYPTO_CIPHER_AES_F8:
		ret = RTE_CRYPTO_CIPHER_AES_F8;
		break;
	case VIRTIO_CRYPTO_CIPHER_AES_XTS:
		ret = RTE_CRYPTO_CIPHER_AES_XTS;
		break;
	case VIRTIO_CRYPTO_CIPHER_ZUC_EEA3:
		ret = RTE_CRYPTO_CIPHER_ZUC_EEA3;
		break;
	default:
		ret = -VIRTIO_CRYPTO_BADMSG;
		break;
	}

	return ret;
}

static int
auth_algo_transform(uint32_t virtio_auth_algo)
{
	int ret;

	switch (virtio_auth_algo) {

	case VIRTIO_CRYPTO_NO_MAC:
		ret = RTE_CRYPTO_AUTH_NULL;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_MD5:
		ret = RTE_CRYPTO_AUTH_MD5_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA1:
		ret = RTE_CRYPTO_AUTH_SHA1_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_224:
		ret = RTE_CRYPTO_AUTH_SHA224_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_256:
		ret = RTE_CRYPTO_AUTH_SHA256_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_384:
		ret = RTE_CRYPTO_AUTH_SHA384_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_HMAC_SHA_512:
		ret = RTE_CRYPTO_AUTH_SHA512_HMAC;
		break;
	case VIRTIO_CRYPTO_MAC_CMAC_3DES:
		ret = -VIRTIO_CRYPTO_NOTSUPP;
		break;
	case VIRTIO_CRYPTO_MAC_CMAC_AES:
		ret = RTE_CRYPTO_AUTH_AES_CMAC;
		break;
	case VIRTIO_CRYPTO_MAC_KASUMI_F9:
		ret = RTE_CRYPTO_AUTH_KASUMI_F9;
		break;
	case VIRTIO_CRYPTO_MAC_SNOW3G_UIA2:
		ret = RTE_CRYPTO_AUTH_SNOW3G_UIA2;
		break;
	case VIRTIO_CRYPTO_MAC_GMAC_AES:
		ret = RTE_CRYPTO_AUTH_AES_GMAC;
		break;
	case VIRTIO_CRYPTO_MAC_GMAC_TWOFISH:
		ret = -VIRTIO_CRYPTO_NOTSUPP;
		break;
	case VIRTIO_CRYPTO_MAC_CBCMAC_AES:
		ret = RTE_CRYPTO_AUTH_AES_CBC_MAC;
		break;
	case VIRTIO_CRYPTO_MAC_CBCMAC_KASUMI_F9:
		ret = -VIRTIO_CRYPTO_NOTSUPP;
		break;
	case VIRTIO_CRYPTO_MAC_XCBC_AES:
		ret = RTE_CRYPTO_AUTH_AES_XCBC_MAC;
		break;
	default:
		ret = -VIRTIO_CRYPTO_BADMSG;
		break;
	}

	return ret;
}

static int get_iv_len(enum rte_crypto_cipher_algorithm algo)
{
	int len;

	switch (algo) {
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		len = 8;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CTR:
		len = 8;
		break;
	case RTE_CRYPTO_CIPHER_3DES_ECB:
		len = 8;
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		len = 16;
		break;

	/* TODO: add common algos */

	default:
		len = -1;
		break;
	}

	return len;
}

/**
 * vhost_crypto struct is used to maintain a number of virtio_cryptos and
 * one DPDK crypto device that deals with all crypto workloads. It is declared
 * here and defined in vhost_crypto.c
 */
struct vhost_crypto {
	/** Used to lookup DPDK Cryptodev Session based on VIRTIO crypto
	 *  session ID.
	 */
	struct rte_hash *session_map;
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *sess_pool;

	/** DPDK cryptodev ID */
	uint8_t cid;
	uint16_t nb_qps;

	uint64_t last_session_id;

	uint64_t cache_session_id;
	struct rte_cryptodev_sym_session *cache_session;
	/** socket id for the device */
	int socket_id;

	struct virtio_net *dev;

	uint8_t option;
} __rte_cache_aligned;

static int
transform_cipher_param(struct rte_crypto_sym_xform *xform,
		VhostUserCryptoSessionParam *param)
{
	int ret;

	ret = cipher_algo_transform(param->cipher_algo);
	if (unlikely(ret < 0))
		return ret;

	xform->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	xform->cipher.algo = (uint32_t)ret;
	xform->cipher.key.length = param->cipher_key_len;
	if (xform->cipher.key.length > 0)
		xform->cipher.key.data = param->cipher_key_buf;
	if (param->dir == VIRTIO_CRYPTO_OP_ENCRYPT)
		xform->cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
	else if (param->dir == VIRTIO_CRYPTO_OP_DECRYPT)
		xform->cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
	else {
		VC_LOG_DBG("Bad operation type");
		return -VIRTIO_CRYPTO_BADMSG;
	}

	ret = get_iv_len(xform->cipher.algo);
	if (unlikely(ret < 0))
		return ret;
	xform->cipher.iv.length = (uint16_t)ret;
	xform->cipher.iv.offset = IV_OFFSET;
	return 0;
}

static int
transform_chain_param(struct rte_crypto_sym_xform *xforms,
		VhostUserCryptoSessionParam *param)
{
	struct rte_crypto_sym_xform *xform_cipher, *xform_auth;
	int ret;

	switch (param->chaining_dir) {
	case VIRTIO_CRYPTO_SYM_ALG_CHAIN_ORDER_HASH_THEN_CIPHER:
		xform_auth = xforms;
		xform_cipher = xforms->next;
		xform_cipher->cipher.op = RTE_CRYPTO_CIPHER_OP_DECRYPT;
		xform_auth->auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
		break;
	case VIRTIO_CRYPTO_SYM_ALG_CHAIN_ORDER_CIPHER_THEN_HASH:
		xform_cipher = xforms;
		xform_auth = xforms->next;
		xform_cipher->cipher.op = RTE_CRYPTO_CIPHER_OP_ENCRYPT;
		xform_auth->auth.op = RTE_CRYPTO_AUTH_OP_GENERATE;
		break;
	default:
		return -VIRTIO_CRYPTO_BADMSG;
	}

	/* cipher */
	ret = cipher_algo_transform(param->cipher_algo);
	if (unlikely(ret < 0))
		return ret;
	xform_cipher->type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	xform_cipher->cipher.algo = (uint32_t)ret;
	xform_cipher->cipher.key.length = param->cipher_key_len;
	xform_cipher->cipher.key.data = param->cipher_key_buf;
	ret = get_iv_len(xform_cipher->cipher.algo);
	if (unlikely(ret < 0))
		return ret;
	xform_cipher->cipher.iv.length = (uint16_t)ret;
	xform_cipher->cipher.iv.offset = IV_OFFSET;

	/* auth */
	xform_auth->type = RTE_CRYPTO_SYM_XFORM_AUTH;
	ret = auth_algo_transform(param->hash_algo);
	if (unlikely(ret < 0))
		return ret;
	xform_auth->auth.algo = (uint32_t)ret;
	xform_auth->auth.digest_length = param->digest_len;
	xform_auth->auth.key.length = param->auth_key_len;
	xform_auth->auth.key.data = param->auth_key_buf;

	return 0;
}

static void
vhost_crypto_create_sess(struct vhost_crypto *vcrypto,
		VhostUserCryptoSessionParam *sess_param)
{
	struct rte_crypto_sym_xform xform1 = {0}, xform2 = {0};
	struct rte_cryptodev_sym_session *session;
	int ret;

	switch (sess_param->op_type) {
	case VIRTIO_CRYPTO_SYM_OP_NONE:
	case VIRTIO_CRYPTO_SYM_OP_CIPHER:
		ret = transform_cipher_param(&xform1, sess_param);
		if (unlikely(ret)) {
			VC_LOG_ERR("Error transform session msg (%i)", ret);
			sess_param->session_id = ret;
			return;
		}
		break;
	case VIRTIO_CRYPTO_SYM_OP_ALGORITHM_CHAINING:
		if (unlikely(sess_param->hash_mode !=
				VIRTIO_CRYPTO_SYM_HASH_MODE_AUTH)) {
			sess_param->session_id = -VIRTIO_CRYPTO_NOTSUPP;
			VC_LOG_ERR("Error transform session message (%i)",
					-VIRTIO_CRYPTO_NOTSUPP);
			return;
		}

		xform1.next = &xform2;

		ret = transform_chain_param(&xform1, sess_param);
		if (unlikely(ret)) {
			VC_LOG_ERR("Error transform session message (%i)", ret);
			sess_param->session_id = ret;
			return;
		}

		break;
	default:
		VC_LOG_ERR("Algorithm not yet supported");
		sess_param->session_id = -VIRTIO_CRYPTO_NOTSUPP;
		return;
	}

	session = rte_cryptodev_sym_session_create(vcrypto->sess_pool);
	if (!session) {
		VC_LOG_ERR("Failed to create session");
		sess_param->session_id = -VIRTIO_CRYPTO_ERR;
		return;
	}

	if (rte_cryptodev_sym_session_init(vcrypto->cid, session, &xform1,
			vcrypto->sess_pool) < 0) {
		VC_LOG_ERR("Failed to initialize session");
		sess_param->session_id = -VIRTIO_CRYPTO_ERR;
		return;
	}

	/* insert hash to map */
	if (rte_hash_add_key_data(vcrypto->session_map,
			&vcrypto->last_session_id, session) < 0) {
		VC_LOG_ERR("Failed to insert session to hash table");

		if (rte_cryptodev_sym_session_clear(vcrypto->cid, session) < 0)
			VC_LOG_ERR("Failed to clear session");
		else {
			if (rte_cryptodev_sym_session_free(session) < 0)
				VC_LOG_ERR("Failed to free session");
		}
		sess_param->session_id = -VIRTIO_CRYPTO_ERR;
		return;
	}

	VC_LOG_INFO("Session %"PRIu64" created for vdev %i.",
			vcrypto->last_session_id, vcrypto->dev->vid);

	sess_param->session_id = vcrypto->last_session_id;
	vcrypto->last_session_id++;
}

static int
vhost_crypto_close_sess(struct vhost_crypto *vcrypto, uint64_t session_id)
{
	struct rte_cryptodev_sym_session *session;
	uint64_t sess_id = session_id;
	int ret;

	ret = rte_hash_lookup_data(vcrypto->session_map, &sess_id,
			(void **)&session);

	if (unlikely(ret < 0)) {
		VC_LOG_ERR("Failed to delete session %"PRIu64".", session_id);
		return -VIRTIO_CRYPTO_INVSESS;
	}

	if (rte_cryptodev_sym_session_clear(vcrypto->cid, session) < 0) {
		VC_LOG_DBG("Failed to clear session");
		return -VIRTIO_CRYPTO_ERR;
	}

	if (rte_cryptodev_sym_session_free(session) < 0) {
		VC_LOG_DBG("Failed to free session");
		return -VIRTIO_CRYPTO_ERR;
	}

	if (rte_hash_del_key(vcrypto->session_map, &sess_id) < 0) {
		VC_LOG_DBG("Failed to delete session from hash table.");
		return -VIRTIO_CRYPTO_ERR;
	}

	VC_LOG_INFO("Session %"PRIu64" deleted for vdev %i.", sess_id,
			vcrypto->dev->vid);

	return 0;
}

static int
vhost_crypto_msg_post_handler(int vid, void *msg, uint32_t *require_reply)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_crypto *vcrypto;
	VhostUserMsg *vmsg = msg;
	int ret = 0;

	if (dev == NULL || require_reply == NULL) {
		VC_LOG_ERR("Invalid vid %i", vid);
		return -EINVAL;
	}

	vcrypto = dev->extern_data;
	if (vcrypto == NULL) {
		VC_LOG_ERR("Cannot find required data, is it initialized?");
		return -ENOENT;
	}

	*require_reply = 0;

	if (vmsg->request.master == VHOST_USER_CRYPTO_CREATE_SESS) {
		vhost_crypto_create_sess(vcrypto,
				&vmsg->payload.crypto_session);
		*require_reply = 1;
	} else if (vmsg->request.master == VHOST_USER_CRYPTO_CLOSE_SESS)
		ret = vhost_crypto_close_sess(vcrypto, vmsg->payload.u64);
	else
		ret = -EINVAL;

	return ret;
}
