/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#include <rte_crypto.h>
#include <rte_crypto_sym.h>
#include <rte_log.h>

#include "bcmfs_logs.h"
#include "bcmfs_sym_defs.h"
#include "bcmfs_sym_pmd.h"
#include "bcmfs_sym_session.h"

/** Configure the session from a crypto xform chain */
static enum bcmfs_sym_chain_order
crypto_get_chain_order(const struct rte_crypto_sym_xform *xform)
{
	enum bcmfs_sym_chain_order res = BCMFS_SYM_CHAIN_NOT_SUPPORTED;

	if (xform != NULL) {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD)
			res = BCMFS_SYM_CHAIN_AEAD;

		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
			if (xform->next == NULL)
				res =  BCMFS_SYM_CHAIN_ONLY_AUTH;
			else if (xform->next->type ==
					RTE_CRYPTO_SYM_XFORM_CIPHER)
				res = BCMFS_SYM_CHAIN_AUTH_CIPHER;
		}
		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
			if (xform->next == NULL)
				res =  BCMFS_SYM_CHAIN_ONLY_CIPHER;
			else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
				res =  BCMFS_SYM_CHAIN_CIPHER_AUTH;
		}
	}

	return res;
}

/* Get session cipher key from input cipher key */
static void
get_key(const uint8_t *input_key, int keylen, uint8_t *session_key)
{
	memcpy(session_key, input_key, keylen);
}

/* Set session cipher parameters */
static int
crypto_set_session_cipher_parameters(struct bcmfs_sym_session *sess,
			 const struct rte_crypto_cipher_xform *cipher_xform)
{
	if (cipher_xform->key.length > BCMFS_MAX_KEY_SIZE) {
		BCMFS_DP_LOG(ERR, "key length not supported");
		return -EINVAL;
	}

	sess->cipher.key.length = cipher_xform->key.length;
	sess->cipher.iv.offset = cipher_xform->iv.offset;
	sess->cipher.iv.length = cipher_xform->iv.length;
	sess->cipher.op = cipher_xform->op;
	sess->cipher.algo = cipher_xform->algo;

	get_key(cipher_xform->key.data,
		sess->cipher.key.length,
		sess->cipher.key.data);

	return 0;
}

/* Set session auth parameters */
static int
crypto_set_session_auth_parameters(struct bcmfs_sym_session *sess,
			const struct rte_crypto_auth_xform *auth_xform)
{
	if (auth_xform->key.length > BCMFS_MAX_KEY_SIZE) {
		BCMFS_DP_LOG(ERR, "key length not supported");
		return -EINVAL;
	}

	sess->auth.op = auth_xform->op;
	sess->auth.key.length = auth_xform->key.length;
	sess->auth.digest_length = auth_xform->digest_length;
	sess->auth.iv.length = auth_xform->iv.length;
	sess->auth.iv.offset = auth_xform->iv.offset;
	sess->auth.algo = auth_xform->algo;

	get_key(auth_xform->key.data,
		auth_xform->key.length,
		sess->auth.key.data);

	return 0;
}

/* Set session aead parameters */
static int
crypto_set_session_aead_parameters(struct bcmfs_sym_session *sess,
			const struct rte_crypto_sym_xform *aead_xform)
{
	if (aead_xform->aead.key.length > BCMFS_MAX_KEY_SIZE) {
		BCMFS_DP_LOG(ERR, "key length not supported");
		return -EINVAL;
	}

	sess->aead.iv.offset = aead_xform->aead.iv.offset;
	sess->aead.iv.length = aead_xform->aead.iv.length;
	sess->aead.aad_length = aead_xform->aead.aad_length;
	sess->aead.key.length = aead_xform->aead.key.length;
	sess->aead.digest_length = aead_xform->aead.digest_length;
	sess->aead.op = aead_xform->aead.op;
	sess->aead.algo = aead_xform->aead.algo;

	get_key(aead_xform->aead.key.data,
		aead_xform->aead.key.length,
		sess->aead.key.data);

	return 0;
}

static struct rte_crypto_auth_xform *
crypto_get_auth_xform(struct rte_crypto_sym_xform *xform)
{
	do {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return &xform->auth;

		xform = xform->next;
	} while (xform);

	return NULL;
}

static struct rte_crypto_cipher_xform *
crypto_get_cipher_xform(struct rte_crypto_sym_xform *xform)
{
	do {
		if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return &xform->cipher;

		xform = xform->next;
	} while (xform);

	return NULL;
}

/** Parse crypto xform chain and set private session parameters */
static int
crypto_set_session_parameters(struct bcmfs_sym_session *sess,
			      struct rte_crypto_sym_xform *xform)
{
	int rc = 0;
	struct rte_crypto_cipher_xform *cipher_xform =
			crypto_get_cipher_xform(xform);
	struct rte_crypto_auth_xform *auth_xform =
			crypto_get_auth_xform(xform);

	sess->chain_order = crypto_get_chain_order(xform);

	switch (sess->chain_order) {
	case BCMFS_SYM_CHAIN_ONLY_CIPHER:
		if (crypto_set_session_cipher_parameters(sess, cipher_xform))
			rc = -EINVAL;
		break;
	case BCMFS_SYM_CHAIN_ONLY_AUTH:
		if (crypto_set_session_auth_parameters(sess, auth_xform))
			rc = -EINVAL;
		break;
	case BCMFS_SYM_CHAIN_AUTH_CIPHER:
		sess->cipher_first = false;
		if (crypto_set_session_auth_parameters(sess, auth_xform)) {
			rc = -EINVAL;
			goto error;
		}

		if (crypto_set_session_cipher_parameters(sess, cipher_xform))
			rc = -EINVAL;
		break;
	case BCMFS_SYM_CHAIN_CIPHER_AUTH:
		sess->cipher_first = true;
		if (crypto_set_session_auth_parameters(sess, auth_xform)) {
			rc = -EINVAL;
			goto error;
		}

		if (crypto_set_session_cipher_parameters(sess, cipher_xform))
			rc = -EINVAL;
		break;
	case BCMFS_SYM_CHAIN_AEAD:
		if (crypto_set_session_aead_parameters(sess, xform))
			rc = -EINVAL;
		break;
	default:
		BCMFS_DP_LOG(ERR, "Invalid chain order\n");
		rc = -EINVAL;
		break;
	}

error:
	return rc;
}

struct bcmfs_sym_session *
bcmfs_sym_get_session(struct rte_crypto_op *op)
{
	struct bcmfs_sym_session *sess = NULL;

	if (unlikely(op->sess_type == RTE_CRYPTO_OP_SESSIONLESS)) {
		BCMFS_DP_LOG(ERR, "operations op(%p) is sessionless", op);
	} else if (likely(op->sym->session != NULL)) {
		/* get existing session */
		sess = (struct bcmfs_sym_session *)
			  get_sym_session_private_data(op->sym->session,
						       cryptodev_bcmfs_driver_id);
	}

	if (sess == NULL)
		op->status = RTE_CRYPTO_OP_STATUS_INVALID_SESSION;

	return sess;
}

int
bcmfs_sym_session_configure(struct rte_cryptodev *dev,
			    struct rte_crypto_sym_xform *xform,
			    struct rte_cryptodev_sym_session *sess,
			    struct rte_mempool *mempool)
{
	void *sess_private_data;
	int ret;

	if (unlikely(sess == NULL)) {
		BCMFS_DP_LOG(ERR, "Invalid session struct");
		return -EINVAL;
	}

	if (rte_mempool_get(mempool, &sess_private_data)) {
		BCMFS_DP_LOG(ERR,
			"Couldn't get object from session mempool");
		return -ENOMEM;
	}

	ret = crypto_set_session_parameters(sess_private_data, xform);

	if (ret != 0) {
		BCMFS_DP_LOG(ERR, "Failed configure session parameters");
		/* Return session to mempool */
		rte_mempool_put(mempool, sess_private_data);
		return ret;
	}

	set_sym_session_private_data(sess, dev->driver_id,
				     sess_private_data);

	return 0;
}

/* Clear the memory of session so it doesn't leave key material behind */
void
bcmfs_sym_session_clear(struct rte_cryptodev *dev,
			struct rte_cryptodev_sym_session  *sess)
{
	uint8_t index = dev->driver_id;
	void *sess_priv = get_sym_session_private_data(sess, index);

	if (sess_priv) {
		struct rte_mempool *sess_mp;

		memset(sess_priv, 0, sizeof(struct bcmfs_sym_session));
		sess_mp = rte_mempool_from_obj(sess_priv);

		set_sym_session_private_data(sess, index, NULL);
		rte_mempool_put(sess_mp, sess_priv);
	}
}

unsigned int
bcmfs_sym_session_get_private_size(struct rte_cryptodev *dev __rte_unused)
{
	return sizeof(struct bcmfs_sym_session);
}
