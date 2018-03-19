/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <unistd.h>

#include <rte_hexdump.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <rte_cryptodev_pmd.h>

#include "ccp_dev.h"
#include "ccp_crypto.h"
#include "ccp_pci.h"
#include "ccp_pmd_private.h"

static enum ccp_cmd_order
ccp_get_cmd_id(const struct rte_crypto_sym_xform *xform)
{
	enum ccp_cmd_order res = CCP_CMD_NOT_SUPPORTED;

	if (xform == NULL)
		return res;
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (xform->next == NULL)
			return CCP_CMD_AUTH;
		else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return CCP_CMD_HASH_CIPHER;
	}
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (xform->next == NULL)
			return CCP_CMD_CIPHER;
		else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return CCP_CMD_CIPHER_HASH;
	}
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD)
		return CCP_CMD_COMBINED;
	return res;
}

/* configure session */
static int
ccp_configure_session_cipher(struct ccp_session *sess,
			     const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_cipher_xform *cipher_xform = NULL;

	cipher_xform = &xform->cipher;

	/* set cipher direction */
	if (cipher_xform->op ==  RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		sess->cipher.dir = CCP_CIPHER_DIR_ENCRYPT;
	else
		sess->cipher.dir = CCP_CIPHER_DIR_DECRYPT;

	/* set cipher key */
	sess->cipher.key_length = cipher_xform->key.length;
	rte_memcpy(sess->cipher.key, cipher_xform->key.data,
		   cipher_xform->key.length);

	/* set iv parameters */
	sess->iv.offset = cipher_xform->iv.offset;
	sess->iv.length = cipher_xform->iv.length;

	switch (cipher_xform->algo) {
	default:
		CCP_LOG_ERR("Unsupported cipher algo");
		return -1;
	}


	switch (sess->cipher.engine) {
	default:
		CCP_LOG_ERR("Invalid CCP Engine");
		return -ENOTSUP;
	}
	return 0;
}

static int
ccp_configure_session_auth(struct ccp_session *sess,
			   const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_auth_xform *auth_xform = NULL;

	auth_xform = &xform->auth;

	sess->auth.digest_length = auth_xform->digest_length;
	if (auth_xform->op ==  RTE_CRYPTO_AUTH_OP_GENERATE)
		sess->auth.op = CCP_AUTH_OP_GENERATE;
	else
		sess->auth.op = CCP_AUTH_OP_VERIFY;
	switch (auth_xform->algo) {
	default:
		CCP_LOG_ERR("Unsupported hash algo");
		return -ENOTSUP;
	}
	return 0;
}

static int
ccp_configure_session_aead(struct ccp_session *sess,
			   const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_aead_xform *aead_xform = NULL;

	aead_xform = &xform->aead;

	sess->cipher.key_length = aead_xform->key.length;
	rte_memcpy(sess->cipher.key, aead_xform->key.data,
		   aead_xform->key.length);

	if (aead_xform->op == RTE_CRYPTO_AEAD_OP_ENCRYPT) {
		sess->cipher.dir = CCP_CIPHER_DIR_ENCRYPT;
		sess->auth.op = CCP_AUTH_OP_GENERATE;
	} else {
		sess->cipher.dir = CCP_CIPHER_DIR_DECRYPT;
		sess->auth.op = CCP_AUTH_OP_VERIFY;
	}
	sess->auth.aad_length = aead_xform->aad_length;
	sess->auth.digest_length = aead_xform->digest_length;

	/* set iv parameters */
	sess->iv.offset = aead_xform->iv.offset;
	sess->iv.length = aead_xform->iv.length;

	switch (aead_xform->algo) {
	default:
		CCP_LOG_ERR("Unsupported aead algo");
		return -ENOTSUP;
	}
	return 0;
}

int
ccp_set_session_parameters(struct ccp_session *sess,
			   const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *aead_xform = NULL;
	int ret = 0;

	sess->cmd_id = ccp_get_cmd_id(xform);

	switch (sess->cmd_id) {
	case CCP_CMD_CIPHER:
		cipher_xform = xform;
		break;
	case CCP_CMD_AUTH:
		auth_xform = xform;
		break;
	case CCP_CMD_CIPHER_HASH:
		cipher_xform = xform;
		auth_xform = xform->next;
		break;
	case CCP_CMD_HASH_CIPHER:
		auth_xform = xform;
		cipher_xform = xform->next;
		break;
	case CCP_CMD_COMBINED:
		aead_xform = xform;
		break;
	default:
		CCP_LOG_ERR("Unsupported cmd_id");
		return -1;
	}

	/* Default IV length = 0 */
	sess->iv.length = 0;
	if (cipher_xform) {
		ret = ccp_configure_session_cipher(sess, cipher_xform);
		if (ret != 0) {
			CCP_LOG_ERR("Invalid/unsupported cipher parameters");
			return ret;
		}
	}
	if (auth_xform) {
		ret = ccp_configure_session_auth(sess, auth_xform);
		if (ret != 0) {
			CCP_LOG_ERR("Invalid/unsupported auth parameters");
			return ret;
		}
	}
	if (aead_xform) {
		ret = ccp_configure_session_aead(sess, aead_xform);
		if (ret != 0) {
			CCP_LOG_ERR("Invalid/unsupported aead parameters");
			return ret;
		}
	}
	return ret;
}
