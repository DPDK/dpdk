/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <rte_crypto_sym.h>
#include <rte_cryptodev.h>
#include <rte_security.h>

#include <cryptodev_pmd.h>

#include "roc_cpt.h"
#include "roc_se.h"

#include "cn10k_cryptodev_sec.h"
#include "cn10k_tls.h"
#include "cnxk_cryptodev.h"
#include "cnxk_cryptodev_ops.h"
#include "cnxk_security.h"

static int
tls_xform_cipher_auth_verify(struct rte_crypto_sym_xform *cipher_xform,
			     struct rte_crypto_sym_xform *auth_xform)
{
	enum rte_crypto_cipher_algorithm c_algo = cipher_xform->cipher.algo;
	enum rte_crypto_auth_algorithm a_algo = auth_xform->auth.algo;
	int ret = -ENOTSUP;

	switch (c_algo) {
	case RTE_CRYPTO_CIPHER_NULL:
		if ((a_algo == RTE_CRYPTO_AUTH_MD5_HMAC) || (a_algo == RTE_CRYPTO_AUTH_SHA1_HMAC) ||
		    (a_algo == RTE_CRYPTO_AUTH_SHA256_HMAC) ||
		    (a_algo == RTE_CRYPTO_AUTH_SHA384_HMAC))
			ret = 0;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		if (a_algo == RTE_CRYPTO_AUTH_SHA1_HMAC)
			ret = 0;
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		if ((a_algo == RTE_CRYPTO_AUTH_SHA1_HMAC) ||
		    (a_algo == RTE_CRYPTO_AUTH_SHA256_HMAC) ||
		    (a_algo == RTE_CRYPTO_AUTH_SHA384_HMAC))
			ret = 0;
		break;
	default:
		break;
	}

	return ret;
}

static int
tls_xform_cipher_verify(struct rte_crypto_sym_xform *crypto_xform)
{
	enum rte_crypto_cipher_algorithm c_algo = crypto_xform->cipher.algo;
	uint16_t keylen = crypto_xform->cipher.key.length;

	if (((c_algo == RTE_CRYPTO_CIPHER_NULL) && (keylen == 0)) ||
	    ((c_algo == RTE_CRYPTO_CIPHER_3DES_CBC) && (keylen == 24)) ||
	    ((c_algo == RTE_CRYPTO_CIPHER_AES_CBC) && ((keylen == 16) || (keylen == 32))))
		return 0;

	return -EINVAL;
}

static int
tls_xform_auth_verify(struct rte_crypto_sym_xform *crypto_xform)
{
	enum rte_crypto_auth_algorithm a_algo = crypto_xform->auth.algo;
	uint16_t keylen = crypto_xform->auth.key.length;

	if (((a_algo == RTE_CRYPTO_AUTH_MD5_HMAC) && (keylen == 16)) ||
	    ((a_algo == RTE_CRYPTO_AUTH_SHA1_HMAC) && (keylen == 20)) ||
	    ((a_algo == RTE_CRYPTO_AUTH_SHA256_HMAC) && (keylen == 32)) ||
	    ((a_algo == RTE_CRYPTO_AUTH_SHA384_HMAC) && (keylen == 48)))
		return 0;

	return -EINVAL;
}

static int
tls_xform_aead_verify(struct rte_security_tls_record_xform *tls_xform,
		      struct rte_crypto_sym_xform *crypto_xform)
{
	uint16_t keylen = crypto_xform->aead.key.length;

	if (tls_xform->type == RTE_SECURITY_TLS_SESS_TYPE_WRITE &&
	    crypto_xform->aead.op != RTE_CRYPTO_AEAD_OP_ENCRYPT)
		return -EINVAL;

	if (tls_xform->type == RTE_SECURITY_TLS_SESS_TYPE_READ &&
	    crypto_xform->aead.op != RTE_CRYPTO_AEAD_OP_DECRYPT)
		return -EINVAL;

	if (crypto_xform->aead.algo == RTE_CRYPTO_AEAD_AES_GCM) {
		if ((keylen == 16) || (keylen == 32))
			return 0;
	}

	if ((crypto_xform->aead.algo == RTE_CRYPTO_AEAD_CHACHA20_POLY1305) && (keylen == 32))
		return 0;

	return -EINVAL;
}

static int
cnxk_tls_xform_verify(struct rte_security_tls_record_xform *tls_xform,
		      struct rte_crypto_sym_xform *crypto_xform)
{
	struct rte_crypto_sym_xform *auth_xform, *cipher_xform = NULL;
	int ret = 0;

	if ((tls_xform->ver != RTE_SECURITY_VERSION_TLS_1_2) &&
	    (tls_xform->ver != RTE_SECURITY_VERSION_DTLS_1_2) &&
	    (tls_xform->ver != RTE_SECURITY_VERSION_TLS_1_3))
		return -EINVAL;

	if ((tls_xform->type != RTE_SECURITY_TLS_SESS_TYPE_READ) &&
	    (tls_xform->type != RTE_SECURITY_TLS_SESS_TYPE_WRITE))
		return -EINVAL;

	if (crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		/* optional padding is not allowed in TLS-1.2 for AEAD */
		if ((tls_xform->options.extra_padding_enable == 1) &&
		    (tls_xform->ver != RTE_SECURITY_VERSION_TLS_1_3))
			return -EINVAL;

		return tls_xform_aead_verify(tls_xform, crypto_xform);
	}

	/* TLS-1.3 only support AEAD.
	 * Control should not reach here for TLS-1.3
	 */
	if (tls_xform->ver == RTE_SECURITY_VERSION_TLS_1_3)
		return -EINVAL;

	if (tls_xform->type == RTE_SECURITY_TLS_SESS_TYPE_WRITE) {
		/* Egress */

		/* First should be for auth in Egress */
		if (crypto_xform->type != RTE_CRYPTO_SYM_XFORM_AUTH)
			return -EINVAL;

		/* Next if present, should be for cipher in Egress */
		if ((crypto_xform->next != NULL) &&
		    (crypto_xform->next->type != RTE_CRYPTO_SYM_XFORM_CIPHER))
			return -EINVAL;

		auth_xform = crypto_xform;
		cipher_xform = crypto_xform->next;
	} else {
		/* Ingress */

		/* First can be for auth only when next is NULL in Ingress. */
		if ((crypto_xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) &&
		    (crypto_xform->next != NULL))
			return -EINVAL;
		else if ((crypto_xform->type != RTE_CRYPTO_SYM_XFORM_CIPHER) ||
			 (crypto_xform->next->type != RTE_CRYPTO_SYM_XFORM_AUTH))
			return -EINVAL;

		cipher_xform = crypto_xform;
		auth_xform = crypto_xform->next;
	}

	if (cipher_xform) {
		if ((tls_xform->type == RTE_SECURITY_TLS_SESS_TYPE_WRITE) &&
		    !(cipher_xform->cipher.op == RTE_CRYPTO_CIPHER_OP_ENCRYPT &&
		      auth_xform->auth.op == RTE_CRYPTO_AUTH_OP_GENERATE))
			return -EINVAL;

		if ((tls_xform->type == RTE_SECURITY_TLS_SESS_TYPE_READ) &&
		    !(cipher_xform->cipher.op == RTE_CRYPTO_CIPHER_OP_DECRYPT &&
		      auth_xform->auth.op == RTE_CRYPTO_AUTH_OP_VERIFY))
			return -EINVAL;
	} else {
		if ((tls_xform->type == RTE_SECURITY_TLS_SESS_TYPE_WRITE) &&
		    (auth_xform->auth.op != RTE_CRYPTO_AUTH_OP_GENERATE))
			return -EINVAL;

		if ((tls_xform->type == RTE_SECURITY_TLS_SESS_TYPE_READ) &&
		    (auth_xform->auth.op == RTE_CRYPTO_AUTH_OP_VERIFY))
			return -EINVAL;
	}

	if (cipher_xform)
		ret = tls_xform_cipher_verify(cipher_xform);

	if (!ret)
		ret = tls_xform_auth_verify(auth_xform);

	if (cipher_xform && !ret)
		return tls_xform_cipher_auth_verify(cipher_xform, auth_xform);

	return ret;
}

static int
tls_write_rlens_get(struct rte_security_tls_record_xform *tls_xfrm,
		    struct rte_crypto_sym_xform *crypto_xfrm)
{
	enum rte_crypto_cipher_algorithm c_algo = RTE_CRYPTO_CIPHER_NULL;
	enum rte_crypto_auth_algorithm a_algo = RTE_CRYPTO_AUTH_NULL;
	uint8_t roundup_byte, tls_hdr_len;
	uint8_t mac_len, iv_len;

	switch (tls_xfrm->ver) {
	case RTE_SECURITY_VERSION_TLS_1_2:
	case RTE_SECURITY_VERSION_TLS_1_3:
		tls_hdr_len = 5;
		break;
	case RTE_SECURITY_VERSION_DTLS_1_2:
		tls_hdr_len = 13;
		break;
	default:
		tls_hdr_len = 0;
		break;
	}

	/* Get Cipher and Auth algo */
	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AEAD)
		return tls_hdr_len + ROC_CPT_AES_GCM_IV_LEN + ROC_CPT_AES_GCM_MAC_LEN;

	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		c_algo = crypto_xfrm->cipher.algo;
		if (crypto_xfrm->next)
			a_algo = crypto_xfrm->next->auth.algo;
	} else {
		a_algo = crypto_xfrm->auth.algo;
		if (crypto_xfrm->next)
			c_algo = crypto_xfrm->next->cipher.algo;
	}

	switch (c_algo) {
	case RTE_CRYPTO_CIPHER_NULL:
		roundup_byte = 4;
		iv_len = 0;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		roundup_byte = ROC_CPT_DES_BLOCK_LENGTH;
		iv_len = ROC_CPT_DES_IV_LEN;
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		roundup_byte = ROC_CPT_AES_BLOCK_LENGTH;
		iv_len = ROC_CPT_AES_CBC_IV_LEN;
		break;
	default:
		roundup_byte = 0;
		iv_len = 0;
		break;
	}

	switch (a_algo) {
	case RTE_CRYPTO_AUTH_NULL:
		mac_len = 0;
		break;
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		mac_len = 16;
		break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		mac_len = 20;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		mac_len = 32;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		mac_len = 32;
		break;
	default:
		mac_len = 0;
		break;
	}

	return tls_hdr_len + iv_len + mac_len + roundup_byte;
}

static void
tls_write_sa_init(struct roc_ie_ot_tls_write_sa *sa)
{
	size_t offset;

	memset(sa, 0, sizeof(struct roc_ie_ot_tls_write_sa));

	offset = offsetof(struct roc_ie_ot_tls_write_sa, tls_12.w26_rsvd7);
	sa->w0.s.hw_ctx_off = offset / ROC_CTX_UNIT_8B;
	sa->w0.s.ctx_push_size = sa->w0.s.hw_ctx_off;
	sa->w0.s.ctx_size = ROC_IE_OT_TLS_CTX_ILEN;
	sa->w0.s.ctx_hdr_size = ROC_IE_OT_TLS_CTX_HDR_SIZE;
	sa->w0.s.aop_valid = 1;
}

static void
tls_read_sa_init(struct roc_ie_ot_tls_read_sa *sa)
{
	size_t offset;

	memset(sa, 0, sizeof(struct roc_ie_ot_tls_read_sa));

	offset = offsetof(struct roc_ie_ot_tls_read_sa, tls_12.ctx);
	sa->w0.s.hw_ctx_off = offset / ROC_CTX_UNIT_8B;
	sa->w0.s.ctx_push_size = sa->w0.s.hw_ctx_off;
	sa->w0.s.ctx_size = ROC_IE_OT_TLS_CTX_ILEN;
	sa->w0.s.ctx_hdr_size = ROC_IE_OT_TLS_CTX_HDR_SIZE;
	sa->w0.s.aop_valid = 1;
}

static size_t
tls_read_ctx_size(struct roc_ie_ot_tls_read_sa *sa, enum rte_security_tls_version tls_ver)
{
	size_t size;

	/* Variable based on Anti-replay Window */
	if (tls_ver == RTE_SECURITY_VERSION_TLS_1_3) {
		size = offsetof(struct roc_ie_ot_tls_read_sa, tls_13.ctx) +
		       sizeof(struct roc_ie_ot_tls_1_3_read_ctx_update_reg);
	} else {
		size = offsetof(struct roc_ie_ot_tls_read_sa, tls_12.ctx) +
		       offsetof(struct roc_ie_ot_tls_read_ctx_update_reg, ar_winbits);
	}

	if (sa->w0.s.ar_win)
		size += (1 << (sa->w0.s.ar_win - 1)) * sizeof(uint64_t);

	return size;
}

static int
tls_read_sa_fill(struct roc_ie_ot_tls_read_sa *read_sa,
		 struct rte_security_tls_record_xform *tls_xfrm,
		 struct rte_crypto_sym_xform *crypto_xfrm, struct cn10k_tls_opt *tls_opt,
		 uint8_t ctx_ilen)
{
	enum rte_security_tls_version tls_ver = tls_xfrm->ver;
	struct rte_crypto_sym_xform *auth_xfrm, *cipher_xfrm;
	const uint8_t *key = NULL;
	uint64_t *tmp, *tmp_key;
	uint32_t replay_win_sz;
	uint8_t *cipher_key;
	int i, length = 0;
	size_t offset;

	/* Initialize the SA */
	memset(read_sa, 0, sizeof(struct roc_ie_ot_tls_read_sa));

	if (tls_ver == RTE_SECURITY_VERSION_TLS_1_2) {
		read_sa->w2.s.version_select = ROC_IE_OT_TLS_VERSION_TLS_12;
		read_sa->tls_12.ctx.ar_valid_mask = tls_xfrm->tls_1_2.seq_no - 1;
	} else if (tls_ver == RTE_SECURITY_VERSION_DTLS_1_2) {
		read_sa->w2.s.version_select = ROC_IE_OT_TLS_VERSION_DTLS_12;
	} else if (tls_ver == RTE_SECURITY_VERSION_TLS_1_3) {
		read_sa->w2.s.version_select = ROC_IE_OT_TLS_VERSION_TLS_13;
		read_sa->tls_13.ctx.ar_valid_mask = tls_xfrm->tls_1_3.seq_no - 1;
	}

	cipher_key = read_sa->cipher_key;

	/* Set encryption algorithm */
	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		length = crypto_xfrm->aead.key.length;
		if (crypto_xfrm->aead.algo == RTE_CRYPTO_AEAD_AES_GCM) {
			read_sa->w2.s.cipher_select = ROC_IE_OT_TLS_CIPHER_AES_GCM;
			if (length == 16)
				read_sa->w2.s.aes_key_len = ROC_IE_OT_TLS_AES_KEY_LEN_128;
			else
				read_sa->w2.s.aes_key_len = ROC_IE_OT_TLS_AES_KEY_LEN_256;
		}

		if (crypto_xfrm->aead.algo == RTE_CRYPTO_AEAD_CHACHA20_POLY1305) {
			read_sa->w2.s.cipher_select = ROC_IE_OT_TLS_CIPHER_CHACHA_POLY;
			read_sa->w2.s.aes_key_len = ROC_IE_OT_TLS_AES_KEY_LEN_256;
		}

		key = crypto_xfrm->aead.key.data;
		memcpy(cipher_key, key, length);

		if (tls_ver == RTE_SECURITY_VERSION_TLS_1_2)
			memcpy(((uint8_t *)cipher_key + 32), &tls_xfrm->tls_1_2.imp_nonce, 4);
		else if (tls_ver == RTE_SECURITY_VERSION_DTLS_1_2)
			memcpy(((uint8_t *)cipher_key + 32), &tls_xfrm->dtls_1_2.imp_nonce, 4);
		else if (tls_ver == RTE_SECURITY_VERSION_TLS_1_3)
			memcpy(((uint8_t *)cipher_key + 32), &tls_xfrm->tls_1_3.imp_nonce, 12);

		goto key_swap;
	}

	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		auth_xfrm = crypto_xfrm;
		cipher_xfrm = crypto_xfrm->next;
	} else {
		cipher_xfrm = crypto_xfrm;
		auth_xfrm = crypto_xfrm->next;
	}

	if (cipher_xfrm != NULL) {
		if (cipher_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_3DES_CBC) {
			read_sa->w2.s.cipher_select = ROC_IE_OT_TLS_CIPHER_3DES;
			length = cipher_xfrm->cipher.key.length;
		} else if (cipher_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_AES_CBC) {
			read_sa->w2.s.cipher_select = ROC_IE_OT_TLS_CIPHER_AES_CBC;
			length = cipher_xfrm->cipher.key.length;
			if (length == 16)
				read_sa->w2.s.aes_key_len = ROC_IE_OT_TLS_AES_KEY_LEN_128;
			else if (length == 32)
				read_sa->w2.s.aes_key_len = ROC_IE_OT_TLS_AES_KEY_LEN_256;
			else
				return -EINVAL;
		} else {
			return -EINVAL;
		}

		key = cipher_xfrm->cipher.key.data;
		memcpy(cipher_key, key, length);
	}

	switch (auth_xfrm->auth.algo) {
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		read_sa->w2.s.mac_select = ROC_IE_OT_TLS_MAC_MD5;
		tls_opt->mac_len = 0;
		break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		read_sa->w2.s.mac_select = ROC_IE_OT_TLS_MAC_SHA1;
		tls_opt->mac_len = 20;
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		read_sa->w2.s.mac_select = ROC_IE_OT_TLS_MAC_SHA2_256;
		tls_opt->mac_len = 32;
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		read_sa->w2.s.mac_select = ROC_IE_OT_TLS_MAC_SHA2_384;
		tls_opt->mac_len = 48;
		break;
	default:
		return -EINVAL;
	}

	roc_se_hmac_opad_ipad_gen(read_sa->w2.s.mac_select, auth_xfrm->auth.key.data,
				  auth_xfrm->auth.key.length, read_sa->tls_12.opad_ipad,
				  ROC_SE_TLS);

	tmp = (uint64_t *)read_sa->tls_12.opad_ipad;
	for (i = 0; i < (int)(ROC_CTX_MAX_OPAD_IPAD_LEN / sizeof(uint64_t)); i++)
		tmp[i] = rte_be_to_cpu_64(tmp[i]);

key_swap:
	tmp_key = (uint64_t *)cipher_key;
	for (i = 0; i < (int)(ROC_IE_OT_TLS_CTX_MAX_KEY_IV_LEN / sizeof(uint64_t)); i++)
		tmp_key[i] = rte_be_to_cpu_64(tmp_key[i]);

	if (tls_xfrm->ver == RTE_SECURITY_VERSION_DTLS_1_2) {
		/* Only support power-of-two window sizes supported */
		replay_win_sz = tls_xfrm->dtls_1_2.ar_win_sz;
		if (replay_win_sz) {
			if (!rte_is_power_of_2(replay_win_sz) ||
			    replay_win_sz > ROC_IE_OT_TLS_AR_WIN_SIZE_MAX)
				return -ENOTSUP;

			read_sa->w0.s.ar_win = rte_log2_u32(replay_win_sz) - 5;
		}
	}

	read_sa->w0.s.ctx_hdr_size = ROC_IE_OT_TLS_CTX_HDR_SIZE;
	read_sa->w0.s.aop_valid = 1;

	offset = offsetof(struct roc_ie_ot_tls_read_sa, tls_12.ctx);
	if (tls_ver == RTE_SECURITY_VERSION_TLS_1_3)
		offset = offsetof(struct roc_ie_ot_tls_read_sa, tls_13.ctx);

	/* Entire context size in 128B units */
	read_sa->w0.s.ctx_size =
		(PLT_ALIGN_CEIL(tls_read_ctx_size(read_sa, tls_ver), ROC_CTX_UNIT_128B) /
		 ROC_CTX_UNIT_128B) -
		1;

	if (read_sa->w0.s.ctx_size < ctx_ilen)
		read_sa->w0.s.ctx_size = ctx_ilen;

	/* Word offset for HW managed CTX field */
	read_sa->w0.s.hw_ctx_off = offset / 8;
	read_sa->w0.s.ctx_push_size = read_sa->w0.s.hw_ctx_off;

	rte_wmb();

	return 0;
}

static int
tls_write_sa_fill(struct roc_ie_ot_tls_write_sa *write_sa,
		  struct rte_security_tls_record_xform *tls_xfrm,
		  struct rte_crypto_sym_xform *crypto_xfrm, uint8_t ctx_ilen)
{
	enum rte_security_tls_version tls_ver = tls_xfrm->ver;
	struct rte_crypto_sym_xform *auth_xfrm, *cipher_xfrm;
	const uint8_t *key = NULL;
	uint8_t *cipher_key;
	uint64_t *tmp_key;
	int i, length = 0;
	size_t offset;

	if (tls_ver == RTE_SECURITY_VERSION_TLS_1_2) {
		write_sa->w2.s.version_select = ROC_IE_OT_TLS_VERSION_TLS_12;
		write_sa->tls_12.seq_num = tls_xfrm->tls_1_2.seq_no - 1;
	} else if (tls_ver == RTE_SECURITY_VERSION_DTLS_1_2) {
		write_sa->w2.s.version_select = ROC_IE_OT_TLS_VERSION_DTLS_12;
		write_sa->tls_12.seq_num = ((uint64_t)tls_xfrm->dtls_1_2.epoch << 48) |
					   (tls_xfrm->dtls_1_2.seq_no & 0x0000ffffffffffff);
		write_sa->tls_12.seq_num -= 1;
	} else if (tls_ver == RTE_SECURITY_VERSION_TLS_1_3) {
		write_sa->w2.s.version_select = ROC_IE_OT_TLS_VERSION_TLS_13;
		write_sa->tls_13.seq_num = tls_xfrm->tls_1_3.seq_no - 1;
	}

	cipher_key = write_sa->cipher_key;

	/* Set encryption algorithm */
	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		length = crypto_xfrm->aead.key.length;
		if (crypto_xfrm->aead.algo == RTE_CRYPTO_AEAD_AES_GCM) {
			write_sa->w2.s.cipher_select = ROC_IE_OT_TLS_CIPHER_AES_GCM;
			if (length == 16)
				write_sa->w2.s.aes_key_len = ROC_IE_OT_TLS_AES_KEY_LEN_128;
			else
				write_sa->w2.s.aes_key_len = ROC_IE_OT_TLS_AES_KEY_LEN_256;
		}
		if (crypto_xfrm->aead.algo == RTE_CRYPTO_AEAD_CHACHA20_POLY1305) {
			write_sa->w2.s.cipher_select = ROC_IE_OT_TLS_CIPHER_CHACHA_POLY;
			write_sa->w2.s.aes_key_len = ROC_IE_OT_TLS_AES_KEY_LEN_256;
		}

		key = crypto_xfrm->aead.key.data;
		memcpy(cipher_key, key, length);

		if (tls_ver == RTE_SECURITY_VERSION_TLS_1_2)
			memcpy(((uint8_t *)cipher_key + 32), &tls_xfrm->tls_1_2.imp_nonce, 4);
		else if (tls_ver == RTE_SECURITY_VERSION_DTLS_1_2)
			memcpy(((uint8_t *)cipher_key + 32), &tls_xfrm->dtls_1_2.imp_nonce, 4);
		else if (tls_ver == RTE_SECURITY_VERSION_TLS_1_3)
			memcpy(((uint8_t *)cipher_key + 32), &tls_xfrm->tls_1_3.imp_nonce, 12);

		goto key_swap;
	}

	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		auth_xfrm = crypto_xfrm;
		cipher_xfrm = crypto_xfrm->next;
	} else {
		cipher_xfrm = crypto_xfrm;
		auth_xfrm = crypto_xfrm->next;
	}

	if (cipher_xfrm != NULL) {
		if (cipher_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_3DES_CBC) {
			write_sa->w2.s.cipher_select = ROC_IE_OT_TLS_CIPHER_3DES;
			length = cipher_xfrm->cipher.key.length;
		} else if (cipher_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_AES_CBC) {
			write_sa->w2.s.cipher_select = ROC_IE_OT_TLS_CIPHER_AES_CBC;
			length = cipher_xfrm->cipher.key.length;
			if (length == 16)
				write_sa->w2.s.aes_key_len = ROC_IE_OT_TLS_AES_KEY_LEN_128;
			else if (length == 32)
				write_sa->w2.s.aes_key_len = ROC_IE_OT_TLS_AES_KEY_LEN_256;
			else
				return -EINVAL;
		} else {
			return -EINVAL;
		}

		key = cipher_xfrm->cipher.key.data;
		if (key != NULL && length != 0) {
			/* Copy encryption key */
			memcpy(cipher_key, key, length);
		}
	}

	if (auth_xfrm != NULL) {
		if (auth_xfrm->auth.algo == RTE_CRYPTO_AUTH_MD5_HMAC)
			write_sa->w2.s.mac_select = ROC_IE_OT_TLS_MAC_MD5;
		else if (auth_xfrm->auth.algo == RTE_CRYPTO_AUTH_SHA1_HMAC)
			write_sa->w2.s.mac_select = ROC_IE_OT_TLS_MAC_SHA1;
		else if (auth_xfrm->auth.algo == RTE_CRYPTO_AUTH_SHA256_HMAC)
			write_sa->w2.s.mac_select = ROC_IE_OT_TLS_MAC_SHA2_256;
		else if (auth_xfrm->auth.algo == RTE_CRYPTO_AUTH_SHA384_HMAC)
			write_sa->w2.s.mac_select = ROC_IE_OT_TLS_MAC_SHA2_384;
		else
			return -EINVAL;

		roc_se_hmac_opad_ipad_gen(write_sa->w2.s.mac_select, auth_xfrm->auth.key.data,
					  auth_xfrm->auth.key.length, write_sa->tls_12.opad_ipad,
					  ROC_SE_TLS);
	}

	tmp_key = (uint64_t *)write_sa->tls_12.opad_ipad;
	for (i = 0; i < (int)(ROC_CTX_MAX_OPAD_IPAD_LEN / sizeof(uint64_t)); i++)
		tmp_key[i] = rte_be_to_cpu_64(tmp_key[i]);

key_swap:
	tmp_key = (uint64_t *)cipher_key;
	for (i = 0; i < (int)(ROC_IE_OT_TLS_CTX_MAX_KEY_IV_LEN / sizeof(uint64_t)); i++)
		tmp_key[i] = rte_be_to_cpu_64(tmp_key[i]);

	write_sa->w0.s.ctx_hdr_size = ROC_IE_OT_TLS_CTX_HDR_SIZE;
	/* Entire context size in 128B units */
	write_sa->w0.s.ctx_size =
		(PLT_ALIGN_CEIL(sizeof(struct roc_ie_ot_tls_write_sa), ROC_CTX_UNIT_128B) /
		 ROC_CTX_UNIT_128B) -
		1;
	offset = offsetof(struct roc_ie_ot_tls_write_sa, tls_12.w26_rsvd7);

	if (tls_ver == RTE_SECURITY_VERSION_TLS_1_3) {
		offset = offsetof(struct roc_ie_ot_tls_write_sa, tls_13.w10_rsvd7);
		write_sa->w0.s.ctx_size -= 1;
	}

	if (write_sa->w0.s.ctx_size < ctx_ilen)
		write_sa->w0.s.ctx_size = ctx_ilen;

	/* Word offset for HW managed CTX field */
	write_sa->w0.s.hw_ctx_off = offset / 8;
	write_sa->w0.s.ctx_push_size = write_sa->w0.s.hw_ctx_off;

	write_sa->w0.s.aop_valid = 1;

	write_sa->w2.s.iv_at_cptr = ROC_IE_OT_TLS_IV_SRC_DEFAULT;

	if (write_sa->w2.s.version_select != ROC_IE_OT_TLS_VERSION_TLS_13) {
#ifdef LA_IPSEC_DEBUG
		if (tls_xfrm->options.iv_gen_disable == 1)
			write_sa->w2.s.iv_at_cptr = ROC_IE_OT_TLS_IV_SRC_FROM_SA;
#else
		if (tls_xfrm->options.iv_gen_disable) {
			plt_err("Application provided IV is not supported");
			return -ENOTSUP;
		}
#endif
	}

	rte_wmb();

	return 0;
}

static int
cn10k_tls_read_sa_create(struct roc_cpt *roc_cpt, struct roc_cpt_lf *lf,
			 struct rte_security_tls_record_xform *tls_xfrm,
			 struct rte_crypto_sym_xform *crypto_xfrm,
			 struct cn10k_sec_session *sec_sess)
{
	struct roc_ie_ot_tls_read_sa *sa_dptr;
	uint8_t tls_ver = tls_xfrm->ver;
	struct cn10k_tls_record *tls;
	union cpt_inst_w4 inst_w4;
	void *read_sa;
	int ret = 0;

	tls = &sec_sess->tls_rec;
	read_sa = &tls->read_sa;

	/* Allocate memory to be used as dptr for CPT ucode WRITE_SA op */
	sa_dptr = plt_zmalloc(sizeof(struct roc_ie_ot_tls_read_sa), 8);
	if (sa_dptr == NULL) {
		plt_err("Could not allocate memory for SA DPTR");
		return -ENOMEM;
	}

	/* Translate security parameters to SA */
	ret = tls_read_sa_fill(sa_dptr, tls_xfrm, crypto_xfrm, &sec_sess->tls_opt,
			       roc_cpt->ctx_ilen);
	if (ret) {
		plt_err("Could not fill read session parameters");
		goto sa_dptr_free;
	}
	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		sec_sess->iv_offset = crypto_xfrm->aead.iv.offset;
		sec_sess->iv_length = crypto_xfrm->aead.iv.length;
	} else if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		sec_sess->iv_offset = crypto_xfrm->cipher.iv.offset;
		sec_sess->iv_length = crypto_xfrm->cipher.iv.length;
	} else {
		sec_sess->iv_offset = crypto_xfrm->auth.iv.offset;
		sec_sess->iv_length = crypto_xfrm->auth.iv.length;
	}

	sec_sess->proto = RTE_SECURITY_PROTOCOL_TLS_RECORD;

	/* pre-populate CPT INST word 4 */
	inst_w4.u64 = 0;
	if ((tls_ver == RTE_SECURITY_VERSION_TLS_1_2) ||
	    (tls_ver == RTE_SECURITY_VERSION_DTLS_1_2)) {
		inst_w4.s.opcode_major = ROC_IE_OT_TLS_MAJOR_OP_RECORD_DEC | ROC_IE_OT_INPLACE_BIT;
		sec_sess->tls_opt.tail_fetch_len = 0;
		if (sa_dptr->w2.s.cipher_select == ROC_IE_OT_TLS_CIPHER_3DES)
			sec_sess->tls_opt.tail_fetch_len = 1;
		else if (sa_dptr->w2.s.cipher_select == ROC_IE_OT_TLS_CIPHER_AES_CBC)
			sec_sess->tls_opt.tail_fetch_len = 2;
	} else if (tls_xfrm->ver == RTE_SECURITY_VERSION_TLS_1_3) {
		inst_w4.s.opcode_major =
			ROC_IE_OT_TLS13_MAJOR_OP_RECORD_DEC | ROC_IE_OT_INPLACE_BIT;
	}

	sec_sess->tls_opt.tls_ver = tls_ver;
	sec_sess->inst.w4 = inst_w4.u64;
	sec_sess->inst.w7 = cnxk_cpt_sec_inst_w7_get(roc_cpt, read_sa);

	memset(read_sa, 0, sizeof(struct roc_ie_ot_tls_read_sa));

	/* Copy word0 from sa_dptr to populate ctx_push_sz ctx_size fields */
	memcpy(read_sa, sa_dptr, 8);

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	/* Write session using microcode opcode */
	ret = roc_cpt_ctx_write(lf, sa_dptr, read_sa, sizeof(struct roc_ie_ot_tls_read_sa));
	if (ret) {
		plt_err("Could not write read session to hardware");
		goto sa_dptr_free;
	}

	/* Trigger CTX flush so that data is written back to DRAM */
	ret = roc_cpt_lf_ctx_flush(lf, read_sa, true);
	if (ret == -EFAULT) {
		plt_err("Could not flush TLS read session to hardware");
		goto sa_dptr_free;
	}

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

sa_dptr_free:
	plt_free(sa_dptr);

	return ret;
}

static int
cn10k_tls_write_sa_create(struct roc_cpt *roc_cpt, struct roc_cpt_lf *lf,
			  struct rte_security_tls_record_xform *tls_xfrm,
			  struct rte_crypto_sym_xform *crypto_xfrm,
			  struct cn10k_sec_session *sec_sess)
{
	struct roc_ie_ot_tls_write_sa *sa_dptr;
	uint8_t tls_ver = tls_xfrm->ver;
	struct cn10k_tls_record *tls;
	union cpt_inst_w4 inst_w4;
	void *write_sa;
	int ret = 0;

	tls = &sec_sess->tls_rec;
	write_sa = &tls->write_sa;

	/* Allocate memory to be used as dptr for CPT ucode WRITE_SA op */
	sa_dptr = plt_zmalloc(sizeof(struct roc_ie_ot_tls_write_sa), 8);
	if (sa_dptr == NULL) {
		plt_err("Could not allocate memory for SA DPTR");
		return -ENOMEM;
	}

	/* Translate security parameters to SA */
	ret = tls_write_sa_fill(sa_dptr, tls_xfrm, crypto_xfrm, roc_cpt->ctx_ilen);
	if (ret) {
		plt_err("Could not fill write session parameters");
		goto sa_dptr_free;
	}

	if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		sec_sess->iv_offset = crypto_xfrm->aead.iv.offset;
		sec_sess->iv_length = crypto_xfrm->aead.iv.length;
	} else if (crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		sec_sess->iv_offset = crypto_xfrm->cipher.iv.offset;
		sec_sess->iv_length = crypto_xfrm->cipher.iv.length;
	} else {
		sec_sess->iv_offset = crypto_xfrm->next->cipher.iv.offset;
		sec_sess->iv_length = crypto_xfrm->next->cipher.iv.length;
	}

	sec_sess->tls_opt.is_write = 1;
	sec_sess->tls_opt.pad_shift = 0;
	sec_sess->tls_opt.tls_ver = tls_ver;
	sec_sess->tls_opt.enable_padding = tls_xfrm->options.extra_padding_enable;
	sec_sess->max_extended_len = tls_write_rlens_get(tls_xfrm, crypto_xfrm);
	sec_sess->proto = RTE_SECURITY_PROTOCOL_TLS_RECORD;

	/* pre-populate CPT INST word 4 */
	inst_w4.u64 = 0;
	if ((tls_ver == RTE_SECURITY_VERSION_TLS_1_2) ||
	    (tls_ver == RTE_SECURITY_VERSION_DTLS_1_2)) {
		inst_w4.s.opcode_major = ROC_IE_OT_TLS_MAJOR_OP_RECORD_ENC | ROC_IE_OT_INPLACE_BIT;
		if (sa_dptr->w2.s.cipher_select == ROC_IE_OT_TLS_CIPHER_3DES)
			sec_sess->tls_opt.pad_shift = 3;
		else
			sec_sess->tls_opt.pad_shift = 4;
	} else if (tls_ver == RTE_SECURITY_VERSION_TLS_1_3) {
		inst_w4.s.opcode_major =
			ROC_IE_OT_TLS13_MAJOR_OP_RECORD_ENC | ROC_IE_OT_INPLACE_BIT;
	}
	sec_sess->inst.w4 = inst_w4.u64;
	sec_sess->inst.w7 = cnxk_cpt_sec_inst_w7_get(roc_cpt, write_sa);

	memset(write_sa, 0, sizeof(struct roc_ie_ot_tls_write_sa));

	/* Copy word0 from sa_dptr to populate ctx_push_sz ctx_size fields */
	memcpy(write_sa, sa_dptr, 8);

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	/* Write session using microcode opcode */
	ret = roc_cpt_ctx_write(lf, sa_dptr, write_sa, sizeof(struct roc_ie_ot_tls_write_sa));
	if (ret) {
		plt_err("Could not write tls write session to hardware");
		goto sa_dptr_free;
	}

	/* Trigger CTX flush so that data is written back to DRAM */
	ret = roc_cpt_lf_ctx_flush(lf, write_sa, false);
	if (ret == -EFAULT) {
		plt_err("Could not flush TLS write session to hardware");
		goto sa_dptr_free;
	}

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

sa_dptr_free:
	plt_free(sa_dptr);

	return ret;
}

int
cn10k_tls_record_session_update(struct cnxk_cpt_vf *vf, struct cnxk_cpt_qp *qp,
				struct cn10k_sec_session *sess,
				struct rte_security_session_conf *conf)
{
	struct roc_cpt *roc_cpt;
	int ret;

	if (conf->tls_record.type == RTE_SECURITY_TLS_SESS_TYPE_READ)
		return -ENOTSUP;

	roc_cpt = &vf->cpt;
	ret = cn10k_tls_write_sa_create(roc_cpt, &qp->lf, &conf->tls_record, conf->crypto_xform,
					(struct cn10k_sec_session *)sess);
	return ret;
}

int
cn10k_tls_record_session_create(struct cnxk_cpt_vf *vf, struct cnxk_cpt_qp *qp,
				struct rte_security_tls_record_xform *tls_xfrm,
				struct rte_crypto_sym_xform *crypto_xfrm,
				struct rte_security_session *sess)
{
	struct roc_cpt *roc_cpt;
	int ret;

	ret = cnxk_tls_xform_verify(tls_xfrm, crypto_xfrm);
	if (ret)
		return ret;

	roc_cpt = &vf->cpt;

	if (tls_xfrm->type == RTE_SECURITY_TLS_SESS_TYPE_READ)
		return cn10k_tls_read_sa_create(roc_cpt, &qp->lf, tls_xfrm, crypto_xfrm,
						(struct cn10k_sec_session *)sess);
	else
		return cn10k_tls_write_sa_create(roc_cpt, &qp->lf, tls_xfrm, crypto_xfrm,
						 (struct cn10k_sec_session *)sess);
}

int
cn10k_sec_tls_session_destroy(struct cnxk_cpt_qp *qp, struct cn10k_sec_session *sess)
{
	struct cn10k_tls_record *tls;
	struct roc_cpt_lf *lf;
	void *sa_dptr = NULL;
	int ret;

	lf = &qp->lf;

	tls = &sess->tls_rec;

	/* Trigger CTX flush to write dirty data back to DRAM */
	roc_cpt_lf_ctx_flush(lf, &tls->read_sa, false);

	ret = -1;

	if (sess->tls_opt.is_write) {
		sa_dptr = plt_zmalloc(sizeof(struct roc_ie_ot_tls_write_sa), 8);
		if (sa_dptr != NULL) {
			tls_write_sa_init(sa_dptr);

			ret = roc_cpt_ctx_write(lf, sa_dptr, &tls->write_sa,
						sizeof(struct roc_ie_ot_tls_write_sa));
		}
	} else {
		sa_dptr = plt_zmalloc(sizeof(struct roc_ie_ot_tls_read_sa), 8);
		if (sa_dptr != NULL) {
			tls_read_sa_init(sa_dptr);

			ret = roc_cpt_ctx_write(lf, sa_dptr, &tls->read_sa,
						sizeof(struct roc_ie_ot_tls_read_sa));
		}
	}

	plt_free(sa_dptr);

	if (ret) {
		/* MC write_ctx failed. Attempt reload of CTX */

		/* Wait for 1 ms so that flush is complete */
		rte_delay_ms(1);

		rte_atomic_thread_fence(rte_memory_order_seq_cst);

		/* Trigger CTX reload to fetch new data from DRAM */
		roc_cpt_lf_ctx_reload(lf, &tls->read_sa);
	}

	return 0;
}
