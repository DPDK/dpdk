/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_crypto.h>
#include <rte_crypto_sym.h>
#include <rte_cryptodev.h>
#include <rte_memcpy.h>
#include <rte_pdcp.h>
#include <rte_pdcp_hdr.h>

#include "pdcp_crypto.h"
#include "pdcp_entity.h"
#include "pdcp_process.h"

static int
pdcp_crypto_xfrm_get(const struct rte_pdcp_entity_conf *conf, struct rte_crypto_sym_xform **c_xfrm,
		     struct rte_crypto_sym_xform **a_xfrm)
{
	*c_xfrm = NULL;
	*a_xfrm = NULL;

	if (conf->crypto_xfrm == NULL)
		return -EINVAL;

	if (conf->crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		*c_xfrm = conf->crypto_xfrm;
		*a_xfrm = conf->crypto_xfrm->next;
	} else if (conf->crypto_xfrm->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		*a_xfrm = conf->crypto_xfrm;
		*c_xfrm = conf->crypto_xfrm->next;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int
pdcp_entity_priv_populate(struct entity_priv *en_priv, const struct rte_pdcp_entity_conf *conf)
{
	struct rte_crypto_sym_xform *c_xfrm, *a_xfrm;
	int ret;

	ret = pdcp_crypto_xfrm_get(conf, &c_xfrm, &a_xfrm);
	if (ret)
		return ret;

	/**
	 * flags.is_authenticated
	 *
	 * MAC-I would be added in case of control plane packets and when authentication
	 * transform is not NULL.
	 */

	if ((conf->pdcp_xfrm.domain == RTE_SECURITY_PDCP_MODE_CONTROL) && (a_xfrm == NULL))
		return -EINVAL;

	if (a_xfrm != NULL)
		en_priv->flags.is_authenticated = 1;

	/**
	 * flags.is_cipher_in_bits
	 *
	 * For ZUC & SNOW3G cipher algos, offset & length need to be provided in bits.
	 */

	if ((c_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_SNOW3G_UEA2) ||
	    (c_xfrm->cipher.algo == RTE_CRYPTO_CIPHER_ZUC_EEA3))
		en_priv->flags.is_cipher_in_bits = 1;

	/**
	 * flags.is_auth_in_bits
	 *
	 * For ZUC & SNOW3G authentication algos, offset & length need to be provided in bits.
	 */

	if (a_xfrm != NULL) {
		if ((a_xfrm->auth.algo == RTE_CRYPTO_AUTH_SNOW3G_UIA2) ||
		    (a_xfrm->auth.algo == RTE_CRYPTO_AUTH_ZUC_EIA3))
			en_priv->flags.is_auth_in_bits = 1;
	}

	/**
	 * flags.is_ul_entity
	 *
	 * Indicate whether the entity is UL/transmitting PDCP entity.
	 */
	if (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK)
		en_priv->flags.is_ul_entity = 1;

	/**
	 * flags.is_null_auth
	 *
	 * For NULL auth, 4B zeros need to be added by lib PDCP. Indicate that
	 * algo is NULL auth to perform the same.
	 */
	if (a_xfrm != NULL && a_xfrm->auth.algo == RTE_CRYPTO_AUTH_NULL)
		en_priv->flags.is_null_auth = 1;

	/**
	 * hdr_sz
	 *
	 * PDCP header size of the entity
	 */
	en_priv->hdr_sz = pdcp_hdr_size_get(conf->pdcp_xfrm.sn_size);

	/**
	 * aad_sz
	 *
	 * For AES-CMAC, additional message is prepended for processing. Need to be trimmed after
	 * crypto processing is done.
	 */
	if (a_xfrm != NULL && a_xfrm->auth.algo == RTE_CRYPTO_AUTH_AES_CMAC)
		en_priv->aad_sz = 8;
	else
		en_priv->aad_sz = 0;

	return 0;
}

int
pdcp_process_func_set(struct rte_pdcp_entity *entity, const struct rte_pdcp_entity_conf *conf)
{
	struct entity_priv *en_priv;
	int ret;

	if (entity == NULL || conf == NULL)
		return -EINVAL;

	en_priv = entity_priv_get(entity);

	ret = pdcp_entity_priv_populate(en_priv, conf);
	if (ret)
		return ret;

	return 0;
}
