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

static inline void
cop_prepare(const struct entity_priv *en_priv, struct rte_mbuf *mb, struct rte_crypto_op *cop,
	    uint8_t data_offset, uint32_t count, const bool is_auth)
{
	const struct rte_crypto_op cop_init = {
		.type = RTE_CRYPTO_OP_TYPE_SYMMETRIC,
		.status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED,
		.sess_type = RTE_CRYPTO_OP_WITH_SESSION,
	};
	struct rte_crypto_sym_op *op;
	uint32_t pkt_len;

	const uint8_t cipher_shift = 3 * en_priv->flags.is_cipher_in_bits;
	const uint8_t auth_shift = 3 * en_priv->flags.is_auth_in_bits;

	op = cop->sym;
	cop->raw = cop_init.raw;
	op->m_src = mb;
	op->m_dst = mb;

	/* Set IV */
	en_priv->iv_gen(cop, en_priv, count);

	/* Prepare op */
	pkt_len = rte_pktmbuf_pkt_len(mb);
	op->cipher.data.offset = data_offset << cipher_shift;
	op->cipher.data.length = (pkt_len - data_offset) << cipher_shift;

	if (is_auth) {
		op->auth.data.offset = 0;
		op->auth.data.length = (pkt_len - RTE_PDCP_MAC_I_LEN) << auth_shift;
		op->auth.digest.data = rte_pktmbuf_mtod_offset(mb, uint8_t *,
							       (pkt_len - RTE_PDCP_MAC_I_LEN));
	}

	__rte_crypto_sym_op_attach_sym_session(op, en_priv->crypto_sess);
}

static inline bool
pdcp_pre_process_uplane_sn_12_ul_set_sn(struct entity_priv *en_priv, struct rte_mbuf *mb,
					uint32_t *count)
{
	struct rte_pdcp_up_data_pdu_sn_12_hdr *pdu_hdr;
	const uint8_t hdr_sz = en_priv->hdr_sz;
	uint32_t sn;

	/* Prepend PDU header */
	pdu_hdr = (struct rte_pdcp_up_data_pdu_sn_12_hdr *)rte_pktmbuf_prepend(mb, hdr_sz);
	if (unlikely(pdu_hdr == NULL))
		return false;

	/* Update sequence num in the PDU header */
	*count = en_priv->state.tx_next++;
	sn = pdcp_sn_from_count_get(*count, RTE_SECURITY_PDCP_SN_SIZE_12);

	pdu_hdr->d_c = RTE_PDCP_PDU_TYPE_DATA;
	pdu_hdr->sn_11_8 = ((sn & 0xf00) >> 8);
	pdu_hdr->sn_7_0 = (sn & 0xff);
	pdu_hdr->r = 0;
	return true;
}

static uint16_t
pdcp_pre_process_uplane_sn_12_ul(const struct rte_pdcp_entity *entity, struct rte_mbuf *in_mb[],
				 struct rte_crypto_op *cop[], uint16_t num, uint16_t *nb_err_ret)
{
	struct entity_priv *en_priv = entity_priv_get(entity);
	uint16_t nb_cop, nb_prep = 0, nb_err = 0;
	struct rte_mbuf *mb;
	uint32_t count;
	uint8_t *mac_i;
	int i;

	const uint8_t data_offset = en_priv->hdr_sz + en_priv->aad_sz;
	const int is_null_auth = en_priv->flags.is_null_auth;

	nb_cop = rte_crypto_op_bulk_alloc(en_priv->cop_pool, RTE_CRYPTO_OP_TYPE_SYMMETRIC, cop,
					  num);

	if (en_priv->flags.is_authenticated) {
		for (i = 0; i < nb_cop; i++) {
			mb = in_mb[i];
			mac_i = (uint8_t *)rte_pktmbuf_append(mb, RTE_PDCP_MAC_I_LEN);
			if (unlikely(mac_i == NULL)) {
				in_mb[nb_err++] = mb;
				continue;
			}

			/* Clear MAC-I field for NULL auth */
			if (is_null_auth)
				memset(mac_i, 0, RTE_PDCP_MAC_I_LEN);

			if (unlikely(!pdcp_pre_process_uplane_sn_12_ul_set_sn(en_priv, mb,
									      &count))) {
				in_mb[nb_err++] = mb;
				continue;
			}

			cop_prepare(en_priv, mb, cop[nb_prep++], data_offset, count, true);
		}
	} else {
		for (i = 0; i < nb_cop; i++) {
			mb = in_mb[i];
			if (unlikely(!pdcp_pre_process_uplane_sn_12_ul_set_sn(en_priv, mb,
									      &count))) {
				in_mb[nb_err++] = mb;
				continue;
			}

			cop_prepare(en_priv, mb, cop[nb_prep++], data_offset, count, false);
		}
	}

	if (unlikely(nb_err))
		/* Using mempool API since crypto API is not providing bulk free */
		rte_mempool_put_bulk(en_priv->cop_pool, (void *)&cop[nb_prep], nb_cop - nb_prep);

	*nb_err_ret = num - nb_prep;

	return nb_prep;
}

static inline bool
pdcp_pre_process_uplane_sn_18_ul_set_sn(struct entity_priv *en_priv, struct rte_mbuf *mb,
					uint32_t *count)
{
	struct rte_pdcp_up_data_pdu_sn_18_hdr *pdu_hdr;
	const uint8_t hdr_sz = en_priv->hdr_sz;
	uint32_t sn;

	/* Prepend PDU header */
	pdu_hdr = (struct rte_pdcp_up_data_pdu_sn_18_hdr *)rte_pktmbuf_prepend(mb, hdr_sz);
	if (unlikely(pdu_hdr == NULL))
		return false;

	/* Update sequence num in the PDU header */
	*count = en_priv->state.tx_next++;
	sn = pdcp_sn_from_count_get(*count, RTE_SECURITY_PDCP_SN_SIZE_18);

	pdu_hdr->d_c = RTE_PDCP_PDU_TYPE_DATA;
	pdu_hdr->sn_17_16 = ((sn & 0x30000) >> 16);
	pdu_hdr->sn_15_8 = ((sn & 0xff00) >> 8);
	pdu_hdr->sn_7_0 = (sn & 0xff);
	pdu_hdr->r = 0;

	return true;
}

static inline uint16_t
pdcp_pre_process_uplane_sn_18_ul(const struct rte_pdcp_entity *entity, struct rte_mbuf *in_mb[],
				 struct rte_crypto_op *cop[], uint16_t num, uint16_t *nb_err_ret)
{
	struct entity_priv *en_priv = entity_priv_get(entity);
	uint16_t nb_cop, nb_prep = 0, nb_err = 0;
	struct rte_mbuf *mb;
	uint32_t count;
	uint8_t *mac_i;
	int i;

	const uint8_t data_offset = en_priv->hdr_sz + en_priv->aad_sz;
	const int is_null_auth = en_priv->flags.is_null_auth;

	nb_cop = rte_crypto_op_bulk_alloc(en_priv->cop_pool, RTE_CRYPTO_OP_TYPE_SYMMETRIC, cop,
					  num);

	if (en_priv->flags.is_authenticated) {
		for (i = 0; i < nb_cop; i++) {
			mb = in_mb[i];
			mac_i = (uint8_t *)rte_pktmbuf_append(mb, RTE_PDCP_MAC_I_LEN);
			if (unlikely(mac_i == NULL)) {
				in_mb[nb_err++] = mb;
				continue;
			}

			/* Clear MAC-I field for NULL auth */
			if (is_null_auth)
				memset(mac_i, 0, RTE_PDCP_MAC_I_LEN);

			if (unlikely(!pdcp_pre_process_uplane_sn_18_ul_set_sn(en_priv, mb,
									      &count))) {
				in_mb[nb_err++] = mb;
				continue;
			}

			cop_prepare(en_priv, mb, cop[nb_prep++], data_offset, count, true);
		}
	} else {
		for (i = 0; i < nb_cop; i++) {
			mb = in_mb[i];
			if (unlikely(!pdcp_pre_process_uplane_sn_18_ul_set_sn(en_priv, mb,
									      &count))) {

				in_mb[nb_err++] = mb;
				continue;
			}

			cop_prepare(en_priv, mb, cop[nb_prep++], data_offset, count, false);
		}
	}

	if (unlikely(nb_err))
		/* Using mempool API since crypto API is not providing bulk free */
		rte_mempool_put_bulk(en_priv->cop_pool, (void *)&cop[nb_prep], nb_cop - nb_prep);

	*nb_err_ret = num - nb_prep;

	return nb_prep;
}

static uint16_t
pdcp_pre_process_cplane_sn_12_ul(const struct rte_pdcp_entity *entity, struct rte_mbuf *in_mb[],
				 struct rte_crypto_op *cop[], uint16_t num, uint16_t *nb_err_ret)
{
	struct entity_priv *en_priv = entity_priv_get(entity);
	struct rte_pdcp_cp_data_pdu_sn_12_hdr *pdu_hdr;
	uint16_t nb_cop, nb_prep = 0, nb_err = 0;
	struct rte_mbuf *mb;
	uint32_t count, sn;
	uint8_t *mac_i;
	int i;

	const uint8_t hdr_sz = en_priv->hdr_sz;
	const uint8_t data_offset = hdr_sz + en_priv->aad_sz;
	const int is_null_auth = en_priv->flags.is_null_auth;

	nb_cop = rte_crypto_op_bulk_alloc(en_priv->cop_pool, RTE_CRYPTO_OP_TYPE_SYMMETRIC, cop,
					  num);

	for (i = 0; i < nb_cop; i++) {
		mb = in_mb[i];
		/* Prepend PDU header */
		pdu_hdr = (struct rte_pdcp_cp_data_pdu_sn_12_hdr *)rte_pktmbuf_prepend(mb, hdr_sz);
		if (unlikely(pdu_hdr == NULL)) {
			in_mb[nb_err++] = mb;
			continue;
		}

		mac_i = (uint8_t *)rte_pktmbuf_append(mb, RTE_PDCP_MAC_I_LEN);
		if (unlikely(mac_i == NULL)) {
			in_mb[nb_err++] = mb;
			continue;
		}

		/* Clear MAC-I field for NULL auth */
		if (is_null_auth)
			memset(mac_i, 0, RTE_PDCP_MAC_I_LEN);

		/* Update sequence number in the PDU header */
		count = en_priv->state.tx_next++;
		sn = pdcp_sn_from_count_get(count, RTE_SECURITY_PDCP_SN_SIZE_12);

		pdu_hdr->sn_11_8 = ((sn & 0xf00) >> 8);
		pdu_hdr->sn_7_0 = (sn & 0xff);
		pdu_hdr->r = 0;

		cop_prepare(en_priv, mb, cop[nb_prep++], data_offset, count, true);
	}

	if (unlikely(nb_err))
		/* Using mempool API since crypto API is not providing bulk free */
		rte_mempool_put_bulk(en_priv->cop_pool, (void *)&cop[nb_prep], nb_cop - nb_prep);

	*nb_err_ret = num - nb_prep;

	return nb_prep;
}

static uint16_t
pdcp_post_process_ul(const struct rte_pdcp_entity *entity,
		     struct rte_mbuf *in_mb[], struct rte_mbuf *out_mb[],
		     uint16_t num, uint16_t *nb_err_ret)
{
	struct entity_priv *en_priv = entity_priv_get(entity);
	const uint32_t hdr_trim_sz = en_priv->aad_sz;
	int i, nb_success = 0, nb_err = 0;
	struct rte_mbuf *mb, *err_mb[num];

#ifdef RTE_ARCH_PPC_64
	err_mb[0] = NULL; /* workaround PPC-GCC bug */
#endif
	for (i = 0; i < num; i++) {
		mb = in_mb[i];
		if (unlikely(mb->ol_flags & RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED)) {
			err_mb[nb_err++] = mb;
			continue;
		}

		if (hdr_trim_sz)
			rte_pktmbuf_adj(mb, hdr_trim_sz);

		out_mb[nb_success++] = mb;
	}

	if (unlikely(nb_err != 0))
		rte_memcpy(&out_mb[nb_success], err_mb, nb_err * sizeof(struct rte_mbuf *));

	*nb_err_ret = nb_err;
	return nb_success;
}

static int
pdcp_pre_post_func_set(struct rte_pdcp_entity *entity, const struct rte_pdcp_entity_conf *conf)
{
	entity->pre_process = NULL;
	entity->post_process = NULL;

	if ((conf->pdcp_xfrm.domain == RTE_SECURITY_PDCP_MODE_CONTROL) &&
	    (conf->pdcp_xfrm.sn_size == RTE_SECURITY_PDCP_SN_SIZE_12) &&
	    (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK)) {
		entity->pre_process = pdcp_pre_process_cplane_sn_12_ul;
		entity->post_process = pdcp_post_process_ul;
	}

	if ((conf->pdcp_xfrm.domain == RTE_SECURITY_PDCP_MODE_DATA) &&
	    (conf->pdcp_xfrm.sn_size == RTE_SECURITY_PDCP_SN_SIZE_12) &&
	    (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK)) {
		entity->pre_process = pdcp_pre_process_uplane_sn_12_ul;
		entity->post_process = pdcp_post_process_ul;
	}

	if ((conf->pdcp_xfrm.domain == RTE_SECURITY_PDCP_MODE_DATA) &&
	    (conf->pdcp_xfrm.sn_size == RTE_SECURITY_PDCP_SN_SIZE_18) &&
	    (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK)) {
		entity->pre_process = pdcp_pre_process_uplane_sn_18_ul;
		entity->post_process = pdcp_post_process_ul;
	}

	if (entity->pre_process == NULL || entity->post_process == NULL)
		return -ENOTSUP;

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

	ret = pdcp_pre_post_func_set(entity, conf);
	if (ret)
		return ret;

	return 0;
}
