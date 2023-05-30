/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_crypto.h>
#include <rte_crypto_sym.h>
#include <rte_cryptodev.h>
#include <rte_errno.h>
#include <rte_pdcp.h>
#include <rte_pdcp_hdr.h>

#include "pdcp_crypto.h"
#include "pdcp_entity.h"

static int
pdcp_crypto_caps_cipher_verify(uint8_t dev_id, const struct rte_crypto_sym_xform *c_xfrm)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	int ret;

	cap_idx.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
	cap_idx.algo.cipher = c_xfrm->cipher.algo;

	cap = rte_cryptodev_sym_capability_get(dev_id, &cap_idx);
	if (cap == NULL)
		return -1;

	ret = rte_cryptodev_sym_capability_check_cipher(cap, c_xfrm->cipher.key.length,
							c_xfrm->cipher.iv.length);

	return ret;
}

static int
pdcp_crypto_caps_auth_verify(uint8_t dev_id, const struct rte_crypto_sym_xform *a_xfrm)
{
	const struct rte_cryptodev_symmetric_capability *cap;
	struct rte_cryptodev_sym_capability_idx cap_idx;
	int ret;

	cap_idx.type = RTE_CRYPTO_SYM_XFORM_AUTH;
	cap_idx.algo.auth = a_xfrm->auth.algo;

	cap = rte_cryptodev_sym_capability_get(dev_id, &cap_idx);
	if (cap == NULL)
		return -1;

	ret = rte_cryptodev_sym_capability_check_auth(cap, a_xfrm->auth.key.length,
						      a_xfrm->auth.digest_length,
						      a_xfrm->auth.iv.length);

	return ret;
}

static int
pdcp_crypto_xfrm_validate(const struct rte_pdcp_entity_conf *conf,
				 const struct rte_crypto_sym_xform *c_xfrm,
				 const struct rte_crypto_sym_xform *a_xfrm,
				 bool is_auth_then_cipher)
{
	uint16_t cipher_iv_len, auth_digest_len, auth_iv_len;
	int ret;

	/*
	 * Uplink means PDCP entity is configured for transmit. Downlink means PDCP entity is
	 * configured for receive. When integrity protection is enabled, PDCP always performs
	 * digest-encrypted or auth-gen-encrypt for uplink (and decrypt-auth-verify for downlink).
	 * So for uplink, crypto chain would be auth-cipher while for downlink it would be
	 * cipher-auth.
	 *
	 * When integrity protection is not required, xform would be cipher only.
	 */

	if (c_xfrm == NULL)
		return -EINVAL;

	if (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK) {

		/* With UPLINK, if auth is enabled, it should be before cipher */
		if (a_xfrm != NULL && !is_auth_then_cipher)
			return -EINVAL;

		/* With UPLINK, cipher operation must be encrypt */
		if (c_xfrm->cipher.op != RTE_CRYPTO_CIPHER_OP_ENCRYPT)
			return -EINVAL;

		/* With UPLINK, auth operation (if present) must be generate */
		if (a_xfrm != NULL && a_xfrm->auth.op != RTE_CRYPTO_AUTH_OP_GENERATE)
			return -EINVAL;

	} else if (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK) {

		/* With DOWNLINK, if auth is enabled, it should be after cipher */
		if (a_xfrm != NULL && is_auth_then_cipher)
			return -EINVAL;

		/* With DOWNLINK, cipher operation must be decrypt */
		if (c_xfrm->cipher.op != RTE_CRYPTO_CIPHER_OP_DECRYPT)
			return -EINVAL;

		/* With DOWNLINK, auth operation (if present) must be verify */
		if (a_xfrm != NULL && a_xfrm->auth.op != RTE_CRYPTO_AUTH_OP_VERIFY)
			return -EINVAL;

	} else {
		return -EINVAL;
	}

	if ((c_xfrm->cipher.algo != RTE_CRYPTO_CIPHER_NULL) &&
	    (c_xfrm->cipher.algo != RTE_CRYPTO_CIPHER_AES_CTR) &&
	    (c_xfrm->cipher.algo != RTE_CRYPTO_CIPHER_ZUC_EEA3) &&
	    (c_xfrm->cipher.algo != RTE_CRYPTO_CIPHER_SNOW3G_UEA2))
		return -EINVAL;

	if (c_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_NULL)
		cipher_iv_len = 0;
	else
		cipher_iv_len = PDCP_IV_LEN;

	if (cipher_iv_len != c_xfrm->cipher.iv.length)
		return -EINVAL;

	if (a_xfrm != NULL) {
		if ((a_xfrm->auth.algo != RTE_CRYPTO_AUTH_NULL) &&
		    (a_xfrm->auth.algo != RTE_CRYPTO_AUTH_AES_CMAC) &&
		    (a_xfrm->auth.algo != RTE_CRYPTO_AUTH_ZUC_EIA3) &&
		    (a_xfrm->auth.algo != RTE_CRYPTO_AUTH_SNOW3G_UIA2))
			return -EINVAL;

		/* For AUTH NULL, lib PDCP would add 4 byte 0s */
		if (a_xfrm->auth.algo == RTE_CRYPTO_AUTH_NULL)
			auth_digest_len = 0;
		else
			auth_digest_len = RTE_PDCP_MAC_I_LEN;

		if (auth_digest_len != a_xfrm->auth.digest_length)
			return -EINVAL;

		if ((a_xfrm->auth.algo == RTE_CRYPTO_AUTH_ZUC_EIA3) ||
		    (a_xfrm->auth.algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2))
			auth_iv_len = PDCP_IV_LEN;
		else
			auth_iv_len = 0;

		if (a_xfrm->auth.iv.length != auth_iv_len)
			return -EINVAL;
	}

	if (!rte_cryptodev_is_valid_dev(conf->dev_id))
		return -EINVAL;

	ret = pdcp_crypto_caps_cipher_verify(conf->dev_id, c_xfrm);
	if (ret)
		return -ENOTSUP;

	if (a_xfrm != NULL) {
		ret = pdcp_crypto_caps_auth_verify(conf->dev_id, a_xfrm);
		if (ret)
			return -ENOTSUP;
	}

	return 0;
}

int
pdcp_crypto_sess_create(struct rte_pdcp_entity *entity, const struct rte_pdcp_entity_conf *conf)
{
	struct rte_crypto_sym_xform *c_xfrm, *a_xfrm;
	struct entity_priv *en_priv;
	bool is_auth_then_cipher;
	int ret;

	if (entity == NULL || conf == NULL || conf->crypto_xfrm == NULL)
		return -EINVAL;

	en_priv = entity_priv_get(entity);

	en_priv->dev_id = conf->dev_id;

	if (conf->crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		c_xfrm = conf->crypto_xfrm;
		a_xfrm = conf->crypto_xfrm->next;
		is_auth_then_cipher = false;
	} else if (conf->crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		a_xfrm = conf->crypto_xfrm;
		c_xfrm = conf->crypto_xfrm->next;
		is_auth_then_cipher = true;
	} else {
		return -EINVAL;
	}

	ret = pdcp_crypto_xfrm_validate(conf, c_xfrm, a_xfrm, is_auth_then_cipher);
	if (ret)
		return ret;

	if (c_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_NULL)
		c_xfrm->cipher.iv.offset = 0;
	else
		c_xfrm->cipher.iv.offset = PDCP_IV_OFFSET;

	if (a_xfrm != NULL) {
		if (a_xfrm->auth.algo == RTE_CRYPTO_AUTH_NULL)
			a_xfrm->auth.iv.offset = 0;
		else
			if (c_xfrm->cipher.iv.offset)
				a_xfrm->auth.iv.offset = PDCP_IV_OFFSET + PDCP_IV_LEN;
			else
				a_xfrm->auth.iv.offset = PDCP_IV_OFFSET;
	}

	if (conf->sess_mpool == NULL)
		return -EINVAL;

	en_priv->crypto_sess = rte_cryptodev_sym_session_create(conf->dev_id, conf->crypto_xfrm,
								conf->sess_mpool);
	if (en_priv->crypto_sess == NULL) {
		/* rte_errno is set as positive values of error codes */
		return -rte_errno;
	}

	rte_cryptodev_sym_session_opaque_data_set(en_priv->crypto_sess, (uint64_t)entity);

	return 0;
}

void
pdcp_crypto_sess_destroy(struct rte_pdcp_entity *entity)
{
	struct entity_priv *en_priv;

	en_priv = entity_priv_get(entity);

	if (en_priv->crypto_sess != NULL) {
		rte_cryptodev_sym_session_free(en_priv->dev_id, en_priv->crypto_sess);
		en_priv->crypto_sess = NULL;
	}
}
